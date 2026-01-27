#include "as.h"

static LiteralEntry *pool_head;
static int pool_count;

void literal_pool_init(void) {
	pool_head = NULL;
	pool_count = 0;
}

LiteralEntry *literal_pool_add_value(uint64_t value) {
	for (LiteralEntry *e = pool_head; e; e = e->next) {
		if (!e->symbol && e->value == value) {
			return e;
		}
	}
	LiteralEntry *e = calloc(1, sizeof(LiteralEntry));
	e->value = value;
	e->symbol = NULL;
	e->pool_offset = (uint64_t)pool_count * 8;
	e->next = pool_head;
	pool_head = e;
	pool_count++;
	return e;
}

LiteralEntry *literal_pool_add_symbol(const char *name) {
	for (LiteralEntry *e = pool_head; e; e = e->next) {
		if (e->symbol && strcmp(e->symbol, name) == 0) {
			return e;
		}
	}
	LiteralEntry *e = calloc(1, sizeof(LiteralEntry));
	e->value = 0;
	e->symbol = strdup(name);
	e->pool_offset = (uint64_t)pool_count * 8;
	e->next = pool_head;
	pool_head = e;
	pool_count++;
	return e;
}

int literal_pool_count(void) {
	return pool_count;
}

uint64_t literal_pool_size(void) {
	return (uint64_t)pool_count * 8;
}

LiteralEntry *literal_pool_get_list(void) {
	return pool_head;
}
