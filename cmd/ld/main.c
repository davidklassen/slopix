#include "ld.h"

#include <fcntl.h>
#include <unistd.h>

#define LD_VERSION "ld (slopix) 1.0"

void error(char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "ld: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(1);
}

static void usage(int code) {
	fprintf(stderr, "Usage: ld [options] <input.o> ...\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -o <file>         Output file (default: a.out)\n");
	fprintf(stderr, "  -L <dir>          Add library search path\n");
	fprintf(stderr, "  -l <name>         Link with libNAME.a\n");
	fprintf(stderr, "  -e <symbol>       Set entry point (default: _start)\n");
	fprintf(stderr, "  --verbose         Verbose output\n");
	fprintf(stderr, "  --dump-sections   Print sections and exit\n");
	fprintf(stderr, "  --dump-symbols    Print symbols and exit\n");
	fprintf(stderr, "  --dump-globals    Print resolved globals and exit\n");
	fprintf(stderr, "  --dump-merged     Print merged output sections and exit\n");
	fprintf(stderr, "  --dump-archives   Print archive contents and exit\n");
	fprintf(stderr, "  --help            Print this help and exit\n");
	fprintf(stderr, "  --version         Print version and exit\n");
	exit(code);
}

static void strarray_push(StringArray *arr, char *s) {
	if (arr->len >= arr->capacity) {
		arr->capacity = arr->capacity ? arr->capacity * 2 : 8;
		arr->data = realloc(arr->data, arr->capacity * sizeof(char *));
	}
	arr->data[arr->len++] = s;
}

static char *find_library(const char *name, StringArray *paths) {
	static char buf[4096];
	for (int i = 0; i < paths->len; i++) {
		snprintf(buf, sizeof(buf), "%s/lib%s.a", paths->data[i], name);
		if (access(buf, R_OK) == 0) {
			return strdup(buf);
		}
	}
	return NULL;
}

static bool is_archive(const char *path) {
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		return false;
	}
	char buf[AR_MAGIC_LEN];
	bool is_ar = (read(fd, buf, AR_MAGIC_LEN) == AR_MAGIC_LEN &&
		      memcmp(buf, AR_MAGIC, AR_MAGIC_LEN) == 0);
	close(fd);
	return is_ar;
}

static void dump_archive(Archive *ar) {
	printf("Archive: %s\n", ar->path);
	printf("Members (%d):\n", ar->member_count);
	for (int i = 0; i < ar->member_count; i++) {
		ArchiveMember *m = &ar->members[i];
		printf("  [%d] %-20s %zu bytes\n", i, m->name, m->size);
	}
	printf("Symbols (%d):\n", ar->symbol_count);
	for (int i = 0; i < ar->symbol_count; i++) {
		ArchiveSymbol *s = &ar->symbols[i];
		printf("  %-40s -> member %d", s->name, s->member_idx);
		if (s->member_idx >= 0 && s->member_idx < ar->member_count) {
			printf(" (%s)", ar->members[s->member_idx].name);
		}
		printf("\n");
	}
}

static void version(void) {
	printf("%s\n", LD_VERSION);
	exit(0);
}

static const char *section_type_name(uint32_t type) {
	switch (type) {
	case SHT_NULL:
		return "NULL";
	case SHT_PROGBITS:
		return "PROGBITS";
	case SHT_SYMTAB:
		return "SYMTAB";
	case SHT_STRTAB:
		return "STRTAB";
	case SHT_RELA:
		return "RELA";
	case SHT_NOBITS:
		return "NOBITS";
	default:
		return "UNKNOWN";
	}
}

static void format_section_flags(uint64_t flags, char *buf) {
	buf[0] = '\0';
	if (flags & SHF_WRITE) {
		strcat(buf, "W");
	}
	if (flags & SHF_ALLOC) {
		strcat(buf, "A");
	}
	if (flags & SHF_EXECINSTR) {
		strcat(buf, "X");
	}
	if (flags & SHF_INFO_LINK) {
		strcat(buf, "I");
	}
	if (buf[0] == '\0') {
		strcpy(buf, "-");
	}
}

static void dump_sections(ObjectFile *obj) {
	printf("[Nr] Name              Type       Flags    Size     Offset\n");
	for (int i = 0; i < obj->shnum; i++) {
		Elf64_Shdr *sh = &obj->shdrs[i];
		const char *name = section_name(obj, i);
		char flags[16];
		format_section_flags(sh->sh_flags, flags);
		printf("[%2d] %-17s %-10s %-8s %-8llu 0x%llx\n",
		       i,
		       name ? name : "(null)",
		       section_type_name(sh->sh_type),
		       flags,
		       (unsigned long long)sh->sh_size,
		       (unsigned long long)sh->sh_offset);
	}
}

static const char *symbol_type_name(uint8_t info) {
	switch (ELF64_ST_TYPE(info)) {
	case STT_NOTYPE:
		return "NOTYPE";
	case STT_OBJECT:
		return "OBJECT";
	case STT_FUNC:
		return "FUNC";
	case STT_SECTION:
		return "SECTION";
	case STT_FILE:
		return "FILE";
	default:
		return "UNKNOWN";
	}
}

static const char *symbol_bind_name(uint8_t info) {
	switch (ELF64_ST_BIND(info)) {
	case STB_LOCAL:
		return "LOCAL";
	case STB_GLOBAL:
		return "GLOBAL";
	case STB_WEAK:
		return "WEAK";
	default:
		return "UNKNOWN";
	}
}

static void dump_symbols(ObjectFile *obj) {
	if (!obj->symtab) {
		printf("No symbol table\n");
		return;
	}

	printf("[Nr] Value            Size Type    Bind   Section  Name\n");
	for (int i = 0; i < obj->symcount; i++) {
		Elf64_Sym *sym = &obj->symtab[i];
		const char *name = symbol_name(obj, i);

		char section[16];
		if (sym->st_shndx == SHN_UNDEF) {
			strcpy(section, "UNDEF");
		} else if (sym->st_shndx == SHN_ABS) {
			strcpy(section, "ABS");
		} else {
			const char *secname = section_name(obj, sym->st_shndx);
			if (secname) {
				snprintf(section, sizeof(section), "%s", secname);
			} else {
				snprintf(section, sizeof(section), "%d", sym->st_shndx);
			}
		}

		printf("[%2d] 0x%-14llx %-4llu %-7s %-6s %-8s %s\n",
		       i,
		       (unsigned long long)sym->st_value,
		       (unsigned long long)sym->st_size,
		       symbol_type_name(sym->st_info),
		       symbol_bind_name(sym->st_info),
		       section,
		       name ? name : "(null)");
	}
}

int main(int argc, char **argv) {
	char *output_file = NULL;
	char *entry_point = "_start";
	bool verbose_flag = false;
	bool dump_sections_flag = false;
	bool dump_symbols_flag = false;
	bool dump_globals_flag = false;
	bool dump_merged_flag = false;
	bool dump_archives_flag = false;
	StringArray lib_paths = {0};
	char **input_files = NULL;
	int input_count = 0;

	input_files = malloc(argc * sizeof(char *));

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-o") == 0) {
			if (i + 1 >= argc) {
				usage(1);
			}
			output_file = argv[++i];
		} else if (strcmp(argv[i], "-L") == 0) {
			if (i + 1 >= argc) {
				error("-L requires an argument");
			}
			strarray_push(&lib_paths, argv[++i]);
		} else if (strncmp(argv[i], "-L", 2) == 0) {
			strarray_push(&lib_paths, argv[i] + 2);
		} else if (strcmp(argv[i], "-l") == 0) {
			if (i + 1 >= argc) {
				error("-l requires an argument");
			}
			char *resolved = find_library(argv[++i], &lib_paths);
			if (!resolved) {
				error("cannot find -l%s", argv[i]);
			}
			input_files[input_count++] = resolved;
		} else if (strncmp(argv[i], "-l", 2) == 0) {
			char *name = argv[i] + 2;
			char *resolved = find_library(name, &lib_paths);
			if (!resolved) {
				error("cannot find -l%s", name);
			}
			input_files[input_count++] = resolved;
		} else if (strcmp(argv[i], "-e") == 0) {
			if (i + 1 >= argc) {
				error("-e requires an argument");
			}
			entry_point = argv[++i];
		} else if (strcmp(argv[i], "--verbose") == 0) {
			verbose_flag = true;
		} else if (strcmp(argv[i], "--dump-sections") == 0) {
			dump_sections_flag = true;
		} else if (strcmp(argv[i], "--dump-symbols") == 0) {
			dump_symbols_flag = true;
		} else if (strcmp(argv[i], "--dump-globals") == 0) {
			dump_globals_flag = true;
		} else if (strcmp(argv[i], "--dump-merged") == 0) {
			dump_merged_flag = true;
		} else if (strcmp(argv[i], "--dump-archives") == 0) {
			dump_archives_flag = true;
		} else if (strcmp(argv[i], "--version") == 0) {
			version();
		} else if (strcmp(argv[i], "--help") == 0) {
			usage(0);
		} else if (argv[i][0] == '-') {
			error("unknown option: %s", argv[i]);
		} else {
			input_files[input_count++] = argv[i];
		}
	}

	if (input_count == 0) {
		error("no input files");
	}

	ObjectFile **objects = NULL;
	int object_count = 0;
	int object_capacity = 0;

	Archive **archives = NULL;
	int archive_count = 0;

	for (int i = 0; i < input_count; i++) {
		if (is_archive(input_files[i])) {
			if (verbose_flag) {
				fprintf(stderr, "loading archive %s\n", input_files[i]);
			}
			archives = realloc(archives, (archive_count + 1) * sizeof(Archive *));
			archives[archive_count++] = archive_open(input_files[i]);
		} else {
			if (verbose_flag) {
				fprintf(stderr, "loading %s\n", input_files[i]);
			}
			if (object_count >= object_capacity) {
				object_capacity = object_capacity ? object_capacity * 2 : 16;
				objects = realloc(objects, object_capacity * sizeof(ObjectFile *));
			}
			objects[object_count++] = elf_read(input_files[i]);
		}
	}

	if (dump_archives_flag) {
		for (int i = 0; i < archive_count; i++) {
			if (i > 0) {
				printf("\n");
			}
			dump_archive(archives[i]);
		}
	}

	if (dump_sections_flag || dump_symbols_flag) {
		for (int i = 0; i < object_count; i++) {
			if (object_count > 1) {
				printf("\n%s:\n", objects[i]->filename);
			}
			if (dump_sections_flag) {
				dump_sections(objects[i]);
			}
			if (dump_symbols_flag) {
				dump_symbols(objects[i]);
			}
		}
	}

	static SymbolTable global;
	symtab_init(&global);

	collect_definitions(objects, object_count, &global);
	resolve_archives(&objects, &object_count, &object_capacity, archives, archive_count, &global, entry_point, verbose_flag);
	if (!check_undefined(objects, object_count, &global, entry_point)) {
		exit(1);
	}

	OutputSection sections[OUT_COUNT];
	output_section_init(&sections[OUT_NULL], "", SHT_NULL, 0);
	output_section_init(&sections[OUT_TEXT], ".text", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR);
	output_section_init(&sections[OUT_RODATA], ".rodata", SHT_PROGBITS, SHF_ALLOC);
	output_section_init(&sections[OUT_DATA], ".data", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE);
	output_section_init(&sections[OUT_BSS], ".bss", SHT_NOBITS, SHF_ALLOC | SHF_WRITE);

	merge_sections(objects, object_count, sections);
	assign_addresses(sections);
	update_symbol_values(&global, sections);

	if (!apply_relocations(objects, object_count, &global, sections)) {
		exit(1);
	}

	if (dump_globals_flag) {
		dump_globals(&global);
	}

	if (dump_merged_flag) {
		dump_output_sections(sections);
	}

	bool dump_only = dump_sections_flag || dump_symbols_flag || dump_globals_flag ||
			 dump_merged_flag || dump_archives_flag;
	if (!dump_only) {
		const char *out_path = output_file ? output_file : "a.out";
		if (verbose_flag) {
			fprintf(stderr, "writing %s\n", out_path);
		}
		if (!write_executable(out_path, sections, &global, entry_point)) {
			exit(1);
		}
	}

	for (int i = 0; i < object_count; i++) {
		elf_free(objects[i]);
	}
	free(objects);
	for (int i = 0; i < archive_count; i++) {
		archive_close(archives[i]);
	}
	free(archives);
	free(input_files);
	free(lib_paths.data);
	return 0;
}
