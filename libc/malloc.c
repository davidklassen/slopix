#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct block_header {
	size_t size;
	struct block_header *next;
	size_t flags;
	size_t _pad;
} block_header_t;

#define FLAG_FREE   1
#define HEADER_SIZE sizeof(block_header_t)
#define MIN_ALLOC   16
#define ALIGN_UP(x) (((x) + 15) & ~15UL)

static block_header_t *free_list;

void *malloc(size_t size) {
	if (size == 0) {
		return NULL;
	}

	size = ALIGN_UP(size);
	if (size < MIN_ALLOC) {
		size = MIN_ALLOC;
	}

	block_header_t *prev = NULL;
	block_header_t *curr = free_list;
	while (curr) {
		if ((curr->flags & FLAG_FREE) && curr->size >= size) {
			if (curr->size >= size + HEADER_SIZE + MIN_ALLOC) {
				block_header_t *new_block =
				    (block_header_t *)((char *)curr +
						       HEADER_SIZE + size);
				new_block->size =
				    curr->size - size - HEADER_SIZE;
				new_block->next = curr->next;
				new_block->flags = FLAG_FREE;
				curr->size = size;
				curr->next = new_block;
			}

			if (prev) {
				prev->next = curr->next;
			} else {
				free_list = curr->next;
			}

			curr->flags &= ~FLAG_FREE;
			curr->next = NULL;
			return (char *)curr + HEADER_SIZE;
		}
		prev = curr;
		curr = curr->next;
	}

	size_t alloc_size = HEADER_SIZE + size;
	block_header_t *block = sbrk((long)alloc_size);
	if (block == (void *)-1) {
		return NULL;
	}

	block->size = size;
	block->next = NULL;
	block->flags = 0;
	return (char *)block + HEADER_SIZE;
}

void free(void *ptr) {
	if (!ptr) {
		return;
	}

	block_header_t *block = (block_header_t *)((char *)ptr - HEADER_SIZE);
	block->flags |= FLAG_FREE;
	block->next = free_list;
	free_list = block;
}

void *calloc(size_t count, size_t size) {
	if (count == 0 || size == 0) {
		return NULL;
	}

	if (count > (size_t)-1 / size) {
		return NULL;
	}

	size_t total = count * size;
	void *ptr = malloc(total);
	if (ptr) {
		memset(ptr, 0, total);
	}
	return ptr;
}

void *realloc(void *ptr, size_t size) {
	if (!ptr) {
		return malloc(size);
	}

	if (size == 0) {
		free(ptr);
		return NULL;
	}

	block_header_t *block = (block_header_t *)((char *)ptr - HEADER_SIZE);
	size_t old_size = block->size;

	size = ALIGN_UP(size);
	if (size < MIN_ALLOC) {
		size = MIN_ALLOC;
	}

	if (old_size >= size) {
		return ptr;
	}

	void *new_ptr = malloc(size);
	if (!new_ptr) {
		return NULL;
	}

	memcpy(new_ptr, ptr, old_size);
	free(ptr);
	return new_ptr;
}
