#include "as.h"

static Reloc *text_relocs;
static Reloc *data_relocs;
static int text_reloc_count;
static int data_reloc_count;

void reloc_init(void) {
	text_relocs = NULL;
	data_relocs = NULL;
	text_reloc_count = 0;
	data_reloc_count = 0;
}

void reloc_add(int section, uint64_t offset, int type, int sym_idx, int64_t addend) {
	Reloc *r = malloc(sizeof(Reloc));
	r->section = section;
	r->offset = offset;
	r->type = type;
	r->symbol_idx = sym_idx;
	r->addend = addend;

	if (section == SECTION_TEXT) {
		r->next = text_relocs;
		text_relocs = r;
		text_reloc_count++;
	} else if (section == SECTION_DATA) {
		r->next = data_relocs;
		data_relocs = r;
		data_reloc_count++;
	} else {
		free(r);
	}
}

int reloc_count(int section) {
	if (section == SECTION_TEXT) {
		return text_reloc_count;
	}
	if (section == SECTION_DATA) {
		return data_reloc_count;
	}
	return 0;
}

Reloc *reloc_get_list(int section) {
	if (section == SECTION_TEXT) {
		return text_relocs;
	}
	if (section == SECTION_DATA) {
		return data_relocs;
	}
	return NULL;
}
