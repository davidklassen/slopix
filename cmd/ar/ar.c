#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define AR_MAGIC     "!<arch>\n"
#define AR_MAGIC_LEN 8

#define ELF_MAGIC "\x7f" \
		  "ELF"

#define SHT_SYMTAB 2
#define SHT_STRTAB 3

#define STB_GLOBAL 1
#define STB_WEAK   2
#define SHN_UNDEF  0

typedef struct {
	char *name;
	void *data;
	size_t size;
	size_t offset;
} Member;

typedef struct {
	char *name;
	int member_idx;
} Symbol;

static Member *members;
static int member_count;
static int member_cap;

static Symbol *symbols;
static int symbol_count;
static int symbol_cap;

static void add_member(const char *path) {
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "ar: cannot open %s\n", path);
		exit(1);
	}

	struct stat st;
	if (fstat(fd, &st) < 0) {
		fprintf(stderr, "ar: cannot stat %s\n", path);
		exit(1);
	}

	void *data = malloc(st.st_size);
	if (read(fd, data, st.st_size) != st.st_size) {
		fprintf(stderr, "ar: cannot read %s\n", path);
		exit(1);
	}
	close(fd);

	const char *name = strrchr(path, '/');
	name = name ? name + 1 : path;

	if (member_count >= member_cap) {
		member_cap = member_cap ? member_cap * 2 : 16;
		members = realloc(members, member_cap * sizeof(Member));
	}

	Member *m = &members[member_count++];
	m->name = strdup(name);
	m->data = data;
	m->size = st.st_size;
	m->offset = 0;
}

static void add_symbol(const char *name, int member_idx) {
	if (symbol_count >= symbol_cap) {
		symbol_cap = symbol_cap ? symbol_cap * 2 : 64;
		symbols = realloc(symbols, symbol_cap * sizeof(Symbol));
	}
	Symbol *s = &symbols[symbol_count++];
	s->name = strdup(name);
	s->member_idx = member_idx;
}

static void extract_symbols(int member_idx) {
	Member *m = &members[member_idx];
	uint8_t *data = m->data;

	if (m->size < 64 || memcmp(data, ELF_MAGIC, 4) != 0) {
		return;
	}

	int is64 = (data[4] == 2);
	if (!is64) {
		return;
	}

	uint64_t shoff = *(uint64_t *)(data + 40);
	uint16_t shentsize = *(uint16_t *)(data + 58);
	uint16_t shnum = *(uint16_t *)(data + 60);
	uint16_t shstrndx = *(uint16_t *)(data + 62);

	if (shoff == 0 || shnum == 0) {
		return;
	}

	uint8_t *symtab = NULL;
	uint64_t symtab_size = 0;
	uint32_t symtab_link = 0;

	for (int i = 0; i < shnum; i++) {
		uint8_t *sh = data + shoff + i * shentsize;
		uint32_t sh_type = *(uint32_t *)(sh + 4);
		if (sh_type == SHT_SYMTAB) {
			uint64_t sh_offset = *(uint64_t *)(sh + 24);
			symtab_size = *(uint64_t *)(sh + 32);
			symtab_link = *(uint32_t *)(sh + 40);
			symtab = data + sh_offset;
			break;
		}
	}

	if (!symtab) {
		return;
	}

	uint8_t *strtab_sh = data + shoff + symtab_link * shentsize;
	uint64_t strtab_offset = *(uint64_t *)(strtab_sh + 24);
	char *strtab = (char *)(data + strtab_offset);

	int sym_count = symtab_size / 24;
	for (int i = 1; i < sym_count; i++) {
		uint8_t *sym = symtab + i * 24;
		uint32_t st_name = *(uint32_t *)(sym + 0);
		uint8_t st_info = sym[4];
		uint16_t st_shndx = *(uint16_t *)(sym + 6);

		int bind = st_info >> 4;
		if ((bind == STB_GLOBAL || bind == STB_WEAK) && st_shndx != SHN_UNDEF) {
			add_symbol(strtab + st_name, member_idx);
		}
	}
}

static void write_be32(uint8_t *p, uint32_t v) {
	p[0] = (v >> 24) & 0xff;
	p[1] = (v >> 16) & 0xff;
	p[2] = (v >> 8) & 0xff;
	p[3] = v & 0xff;
}

static void write_header(FILE *f, const char *name, size_t size) {
	char hdr[60];
	memset(hdr, ' ', 60);

	size_t namelen = strlen(name);
	if (namelen > 15) {
		namelen = 15;
	}
	memcpy(hdr, name, namelen);
	if (strcmp(name, "/") != 0) {
		hdr[namelen] = '/';
	}

	char sizebuf[11];
	snprintf(sizebuf, sizeof(sizebuf), "%lu", (unsigned long)size);
	memcpy(hdr + 48, sizebuf, strlen(sizebuf));

	hdr[58] = '`';
	hdr[59] = '\n';

	fwrite(hdr, 1, 60, f);
}

static void usage(void) {
	fprintf(stderr, "usage: ar rcs <archive> <file>...\n");
	exit(1);
}

int main(int argc, char **argv) {
	if (argc < 4) {
		usage();
	}

	if (strcmp(argv[1], "rcs") != 0) {
		fprintf(stderr, "ar: only 'rcs' mode supported\n");
		exit(1);
	}

	const char *archive = argv[2];

	for (int i = 3; i < argc; i++) {
		add_member(argv[i]);
	}

	for (int i = 0; i < member_count; i++) {
		extract_symbols(i);
	}

	size_t symtab_names_size = 0;
	for (int i = 0; i < symbol_count; i++) {
		symtab_names_size += strlen(symbols[i].name) + 1;
	}
	size_t symtab_size = 4 + symbol_count * 4 + symtab_names_size;

	size_t offset = AR_MAGIC_LEN;
	if (symbol_count > 0) {
		offset += 60 + symtab_size;
		if (symtab_size & 1) {
			offset++;
		}
	}

	for (int i = 0; i < member_count; i++) {
		members[i].offset = offset;
		offset += 60 + members[i].size;
		if (members[i].size & 1) {
			offset++;
		}
	}

	FILE *out = fopen(archive, "w");
	if (!out) {
		fprintf(stderr, "ar: cannot create %s\n", archive);
		return 1;
	}

	fwrite(AR_MAGIC, 1, AR_MAGIC_LEN, out);

	if (symbol_count > 0) {
		write_header(out, "/", symtab_size);

		uint8_t *symtab_data = calloc(1, symtab_size);
		write_be32(symtab_data, symbol_count);

		size_t name_offset = 0;
		for (int i = 0; i < symbol_count; i++) {
			uint32_t member_offset = members[symbols[i].member_idx].offset;
			write_be32(symtab_data + 4 + i * 4, member_offset);
		}

		char *names = (char *)(symtab_data + 4 + symbol_count * 4);
		for (int i = 0; i < symbol_count; i++) {
			size_t len = strlen(symbols[i].name) + 1;
			memcpy(names + name_offset, symbols[i].name, len);
			name_offset += len;
		}

		fwrite(symtab_data, 1, symtab_size, out);
		free(symtab_data);

		if (symtab_size & 1) {
			fputc('\n', out);
		}
	}

	for (int i = 0; i < member_count; i++) {
		Member *m = &members[i];
		write_header(out, m->name, m->size);
		fwrite(m->data, 1, m->size, out);
		if (m->size & 1) {
			fputc('\n', out);
		}
	}

	fclose(out);
	return 0;
}
