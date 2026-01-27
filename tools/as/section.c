#include "as.h"

SectionBuf text_section;
SectionBuf data_section;
size_t bss_size;

void section_init(SectionBuf *sec) {
	sec->capacity = 256;
	sec->data = malloc(sec->capacity);
	sec->size = 0;
}

static void ensure_capacity(SectionBuf *sec, size_t needed) {
	while (sec->size + needed > sec->capacity) {
		sec->capacity *= 2;
		sec->data = realloc(sec->data, sec->capacity);
	}
}

void section_emit8(SectionBuf *sec, uint8_t val) {
	ensure_capacity(sec, 1);
	sec->data[sec->size++] = val;
}

void section_emit32(SectionBuf *sec, uint32_t val) {
	ensure_capacity(sec, 4);
	sec->data[sec->size++] = (uint8_t)(val);
	sec->data[sec->size++] = (uint8_t)(val >> 8);
	sec->data[sec->size++] = (uint8_t)(val >> 16);
	sec->data[sec->size++] = (uint8_t)(val >> 24);
}

void section_emit64(SectionBuf *sec, uint64_t val) {
	ensure_capacity(sec, 8);
	sec->data[sec->size++] = (uint8_t)(val);
	sec->data[sec->size++] = (uint8_t)(val >> 8);
	sec->data[sec->size++] = (uint8_t)(val >> 16);
	sec->data[sec->size++] = (uint8_t)(val >> 24);
	sec->data[sec->size++] = (uint8_t)(val >> 32);
	sec->data[sec->size++] = (uint8_t)(val >> 40);
	sec->data[sec->size++] = (uint8_t)(val >> 48);
	sec->data[sec->size++] = (uint8_t)(val >> 56);
}

void section_align(SectionBuf *sec, int power) {
	size_t alignment = 1UL << power;
	size_t aligned = (sec->size + alignment - 1) & ~(alignment - 1);
	size_t padding = aligned - sec->size;
	if (padding > 0) {
		section_emit_zeros(sec, padding);
	}
}

void section_emit_zeros(SectionBuf *sec, size_t count) {
	ensure_capacity(sec, count);
	memset(sec->data + sec->size, 0, count);
	sec->size += count;
}
