#include "ld.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static uint32_t read_be32(uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static size_t parse_ar_size(char *field) {
	char buf[11];
	memcpy(buf, field, 10);
	buf[10] = '\0';
	return strtoul(buf, NULL, 10);
}

static char *parse_ar_name(char *field, uint8_t *strtab, size_t strtab_size) {
	if (field[0] == '/' && field[1] >= '0' && field[1] <= '9') {
		size_t offset = strtoul(field + 1, NULL, 10);
		if (strtab && offset < strtab_size) {
			char *start = (char *)strtab + offset;
			char *end = strchr(start, '/');
			if (!end) {
				end = strchr(start, '\n');
			}
			size_t len = end ? (size_t)(end - start) : strlen(start);
			char *name = malloc(len + 1);
			memcpy(name, start, len);
			name[len] = '\0';
			return name;
		}
	}

	char *end = memchr(field, '/', 16);
	if (!end) {
		end = field + 16;
	}
	while (end > field && end[-1] == ' ') {
		end--;
	}
	size_t len = end - field;
	char *name = malloc(len + 1);
	memcpy(name, field, len);
	name[len] = '\0';
	return name;
}

Archive *archive_open(const char *path) {
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		error("cannot open %s: %s", path, strerror(errno));
	}

	struct stat st;
	if (fstat(fd, &st) < 0) {
		error("cannot stat %s: %s", path, strerror(errno));
	}

	if (st.st_size < AR_MAGIC_LEN) {
		error("%s: file too small for archive", path);
	}

	void *data = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (data == MAP_FAILED) {
		error("cannot mmap %s: %s", path, strerror(errno));
	}
	close(fd);

	if (memcmp(data, AR_MAGIC, AR_MAGIC_LEN) != 0) {
		error("%s: not an archive file", path);
	}

	Archive *ar = calloc(1, sizeof(Archive));
	ar->path = strdup(path);
	ar->data = data;
	ar->size = st.st_size;

	uint8_t *symtab_data = NULL;
	size_t symtab_size = 0;
	uint8_t *strtab = NULL;
	size_t strtab_size = 0;

	int capacity = 16;
	ar->members = malloc(capacity * sizeof(ArchiveMember));

	size_t offset = AR_MAGIC_LEN;
	while (offset + 60 <= ar->size) {
		char *hdr = (char *)(ar->data + offset);

		if (hdr[58] != '`' || hdr[59] != '\n') {
			break;
		}

		size_t member_size = parse_ar_size(hdr + 48);

		if (hdr[0] == '/' && hdr[1] == ' ') {
			symtab_data = ar->data + offset + 60;
			symtab_size = member_size;
		} else if (hdr[0] == '/' && hdr[1] == '/') {
			strtab = ar->data + offset + 60;
			strtab_size = member_size;
		} else {
			if (ar->member_count >= capacity) {
				capacity *= 2;
				ar->members = realloc(ar->members, capacity * sizeof(ArchiveMember));
			}
			ArchiveMember *m = &ar->members[ar->member_count++];
			m->name = parse_ar_name(hdr, strtab, strtab_size);
			m->data = ar->data + offset + 60;
			m->size = member_size;
			m->offset = offset;
			m->extracted = false;
		}

		offset += 60 + member_size;
		if (offset & 1) {
			offset++;
		}
	}

	if (symtab_data && symtab_size >= 4) {
		uint32_t sym_count = read_be32(symtab_data);
		if (symtab_size >= 4 + sym_count * 4) {
			ar->symbols = malloc(sym_count * sizeof(ArchiveSymbol));
			ar->symbol_count = sym_count;

			uint8_t *offsets = symtab_data + 4;
			char *names = (char *)(symtab_data + 4 + sym_count * 4);

			for (uint32_t i = 0; i < sym_count; i++) {
				uint32_t file_offset = read_be32(offsets + i * 4);

				int member_idx = -1;
				for (int j = 0; j < ar->member_count; j++) {
					if (ar->members[j].offset == file_offset) {
						member_idx = j;
						break;
					}
				}

				ar->symbols[i].name = names;
				ar->symbols[i].member_idx = member_idx;

				names += strlen(names) + 1;
			}
		}
	}

	return ar;
}

void archive_close(Archive *ar) {
	if (ar) {
		for (int i = 0; i < ar->member_count; i++) {
			free(ar->members[i].name);
		}
		free(ar->members);
		free(ar->symbols);
		munmap(ar->data, ar->size);
		free(ar->path);
		free(ar);
	}
}

ObjectFile *archive_extract_member(Archive *ar, int member_idx) {
	if (member_idx < 0 || member_idx >= ar->member_count) {
		return NULL;
	}

	ArchiveMember *m = &ar->members[member_idx];
	if (m->extracted) {
		return NULL;
	}

	m->extracted = true;

	size_t name_len = strlen(ar->path) + strlen(m->name) + 3;
	char *name = malloc(name_len);
	snprintf(name, name_len, "%s(%s)", ar->path, m->name);

	ObjectFile *obj = elf_read_memory(m->data, m->size, name);
	free(name);

	return obj;
}

int archive_find_symbol(Archive *ar, const char *name) {
	for (int i = 0; i < ar->symbol_count; i++) {
		if (strcmp(ar->symbols[i].name, name) == 0) {
			return ar->symbols[i].member_idx;
		}
	}
	return -1;
}
