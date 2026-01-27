#include "as.h"

static Symbol *symbols;
static int nsymbols;
static int capacity;

void symtab_init(void) {
	capacity = 64;
	symbols = calloc(capacity, sizeof(Symbol));
	nsymbols = 0;
}

Symbol *symtab_lookup(const char *name) {
	for (int i = 0; i < nsymbols; i++) {
		if (strcmp(symbols[i].name, name) == 0) {
			return &symbols[i];
		}
	}
	return NULL;
}

Symbol *symtab_add(const char *name) {
	Symbol *sym = symtab_lookup(name);
	if (sym) {
		return sym;
	}

	if (nsymbols >= capacity) {
		capacity *= 2;
		symbols = realloc(symbols, capacity * sizeof(Symbol));
	}

	sym = &symbols[nsymbols++];
	sym->name = strdup(name);
	sym->section = SECTION_NONE;
	sym->value = 0;
	sym->binding = STB_LOCAL;
	sym->type = STT_NOTYPE;
	sym->size = 0;
	sym->defined = 0;
	return sym;
}

void symtab_set_binding(Symbol *sym, int binding) {
	sym->binding = binding;
}

void symtab_set_type(Symbol *sym, int type) {
	sym->type = type;
}

int symtab_count(void) {
	return nsymbols;
}

Symbol *symtab_get(int index) {
	if (index < 0 || index >= nsymbols) {
		return NULL;
	}
	return &symbols[index];
}
