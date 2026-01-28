#include "ld.h"

static uint32_t hash_string(const char *s) {
	uint32_t h = 5381;
	for (; *s; s++) {
		h = ((h << 5) + h) + (unsigned char)*s;
	}
	return h;
}

void symtab_init(SymbolTable *tab) {
	memset(tab->buckets, 0, sizeof(tab->buckets));
	tab->count = 0;
}

Symbol *symbol_lookup(SymbolTable *tab, const char *name) {
	uint32_t h = hash_string(name) % SYMTAB_BUCKETS;
	for (Symbol *s = tab->buckets[h]; s; s = s->next) {
		if (strcmp(s->name, name) == 0) {
			return s;
		}
	}
	return NULL;
}

static Symbol *symbol_add(SymbolTable *tab, const char *name, uint64_t value, uint64_t size, uint8_t type, uint8_t binding, ObjectFile *file, uint16_t shndx) {
	Symbol *sym = malloc(sizeof(Symbol));
	sym->name = name;
	sym->value = value;
	sym->size = size;
	sym->type = type;
	sym->binding = binding;
	sym->file = file;
	sym->shndx = shndx;
	sym->output_shndx = 0;

	uint32_t h = hash_string(name) % SYMTAB_BUCKETS;
	sym->next = tab->buckets[h];
	tab->buckets[h] = sym;
	tab->count++;
	return sym;
}

static void symbol_replace(Symbol *existing, uint64_t value, uint64_t size, uint8_t type, uint8_t binding, ObjectFile *file, uint16_t shndx) {
	existing->value = value;
	existing->size = size;
	existing->type = type;
	existing->binding = binding;
	existing->file = file;
	existing->shndx = shndx;
}

bool resolve_symbols(ObjectFile **objects, int count, SymbolTable *global) {
	bool ok = true;

	// Pass 1: collect definitions
	for (int i = 0; i < count; i++) {
		ObjectFile *obj = objects[i];
		if (!obj->symtab) {
			continue;
		}

		for (int j = 1; j < obj->symcount; j++) {
			Elf64_Sym *sym = &obj->symtab[j];
			uint8_t binding = ELF64_ST_BIND(sym->st_info);
			uint8_t type = ELF64_ST_TYPE(sym->st_info);

			if (binding == STB_LOCAL) {
				continue;
			}

			if (sym->st_shndx == SHN_UNDEF) {
				continue;
			}

			const char *name = symbol_name(obj, j);
			if (!name || name[0] == '\0') {
				continue;
			}

			Symbol *existing = symbol_lookup(global, name);
			if (existing) {
				if (existing->binding == STB_WEAK &&
				    binding == STB_GLOBAL) {
					symbol_replace(existing, sym->st_value, sym->st_size, type, binding, obj, sym->st_shndx);
				} else if (existing->binding == STB_GLOBAL &&
					   binding == STB_GLOBAL) {
					fprintf(stderr,
						"ld: multiple definition of '%s'\n",
						name);
					fprintf(stderr, "  first defined in %s\n", existing->file->filename);
					fprintf(stderr, "  also defined in %s\n", obj->filename);
					ok = false;
				}
			} else {
				symbol_add(global, name, sym->st_value, sym->st_size, type, binding, obj, sym->st_shndx);
			}
		}
	}

	// Pass 2: check undefined references
	for (int i = 0; i < count; i++) {
		ObjectFile *obj = objects[i];
		if (!obj->symtab) {
			continue;
		}

		for (int j = 1; j < obj->symcount; j++) {
			Elf64_Sym *sym = &obj->symtab[j];
			uint8_t binding = ELF64_ST_BIND(sym->st_info);

			if (binding == STB_LOCAL) {
				continue;
			}

			if (sym->st_shndx != SHN_UNDEF) {
				continue;
			}

			const char *name = symbol_name(obj, j);
			if (!name || name[0] == '\0') {
				continue;
			}

			Symbol *found = symbol_lookup(global, name);
			if (!found && binding == STB_GLOBAL) {
				fprintf(stderr,
					"ld: undefined reference to '%s'\n",
					name);
				fprintf(stderr, "  referenced from %s\n", obj->filename);
				ok = false;
			}
		}
	}

	// Check for _start entry point
	if (!symbol_lookup(global, "_start")) {
		fprintf(stderr, "ld: undefined reference to '_start'\n");
		fprintf(stderr, "  (entry point not found)\n");
		ok = false;
	}

	return ok;
}

void dump_globals(SymbolTable *global) {
	printf("Global symbol table (%d symbols):\n", global->count);
	printf("%-40s %-10s %-8s %-8s %s\n", "Name", "Value", "Size", "Bind", "File");

	for (int i = 0; i < SYMTAB_BUCKETS; i++) {
		for (Symbol *s = global->buckets[i]; s; s = s->next) {
			const char *bind =
			    s->binding == STB_GLOBAL ? "GLOBAL" : s->binding == STB_WEAK ? "WEAK"
											 : "?";
			printf("%-40s 0x%08llx %-8llu %-8s %s\n", s->name, (unsigned long long)s->value, (unsigned long long)s->size, bind, s->file->filename);
		}
	}
}
