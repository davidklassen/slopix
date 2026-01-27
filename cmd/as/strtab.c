#include "as.h"

void strtab_init(StringTable *st) {
	st->capacity = 256;
	st->data = malloc(st->capacity);
	st->size = 1;
	st->data[0] = '\0';
}

static void ensure_capacity(StringTable *st, size_t needed) {
	while (st->size + needed > st->capacity) {
		st->capacity *= 2;
		st->data = realloc(st->data, st->capacity);
	}
}

uint32_t strtab_add(StringTable *st, const char *str) {
	size_t len = strlen(str) + 1;
	uint32_t offset = (uint32_t)st->size;
	ensure_capacity(st, len);
	memcpy(st->data + st->size, str, len);
	st->size += len;
	return offset;
}
