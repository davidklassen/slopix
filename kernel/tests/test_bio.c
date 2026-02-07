#include "test.h"
#include "bio.h"

#define SCRATCH_BLOCK_1 50
#define SCRATCH_BLOCK_2 51
#define SCRATCH_BLOCK_3 52

TEST(bread_returns_buffer) {
	struct buf *b = bio_read(0, SCRATCH_BLOCK_1);
	ASSERT_NOT_NULL(b, "bread should return non-null buffer");
	ASSERT_EQ(b->valid, 1, "buffer should be valid after bread");
	bio_release(b);
	return 0;
}

TEST(brelse_frees_buffer) {
	struct buf *b = bio_read(0, SCRATCH_BLOCK_1);
	unsigned int refcnt_before = b->refcnt;
	bio_release(b);
	ASSERT_EQ(b->refcnt, refcnt_before - 1, "refcnt should decrement after brelse");
	return 0;
}

TEST(bread_cache_hit) {
	struct buf *b1 = bio_read(0, SCRATCH_BLOCK_1);
	b1->data[0] = 0xAB;
	bio_release(b1);

	struct buf *b2 = bio_read(0, SCRATCH_BLOCK_1);
	ASSERT_EQ(b2, b1, "second read should return same buffer");
	ASSERT_EQ(b2->data[0], 0xAB, "cached data should persist");
	bio_release(b2);
	return 0;
}

TEST(bread_cache_miss) {
	struct buf *b1 = bio_read(0, SCRATCH_BLOCK_1);
	struct buf *b2 = bio_read(0, SCRATCH_BLOCK_2);
	ASSERT_NE(b1, b2, "different blocks should use different buffers");
	bio_release(b1);
	bio_release(b2);
	return 0;
}

TEST(bwrite_persists_data) {
	struct buf *b = bio_read(0, SCRATCH_BLOCK_3);
	for (int i = 0; i < BSIZE; i++) {
		b->data[i] = (unsigned char)(i & 0xFF);
	}
	bio_write(b);

	// Invalidate before release to force re-read from disk on next bio_read().
	// This tests that bio_write() actually persists data to disk.
	b->valid = 0;
	bio_release(b);

	b = bio_read(0, SCRATCH_BLOCK_3);
	for (int i = 0; i < BSIZE; i++) {
		if (b->data[i] != (unsigned char)(i & 0xFF)) {
			bio_release(b);
			ASSERT(0, "data mismatch after write-read");
		}
	}
	bio_release(b);
	return 0;
}

TEST(cache_lru_eviction) {
	struct buf *first = bio_read(0, SCRATCH_BLOCK_1);
	first->data[0] = 0x42;
	bio_release(first);

	for (unsigned int i = 0; i < NBUF; i++) {
		struct buf *b = bio_read(0, 1000 + i);
		bio_release(b);
	}

	struct buf *evicted = bio_read(0, SCRATCH_BLOCK_1);
	ASSERT_NE(evicted, first, "buffer should have been evicted");
	bio_release(evicted);
	return 0;
}

TEST_SUITE(bio) {
	RUN_TEST(bread_returns_buffer);
	RUN_TEST(brelse_frees_buffer);
	RUN_TEST(bread_cache_hit);
	RUN_TEST(bread_cache_miss);
	RUN_TEST(bwrite_persists_data);
	RUN_TEST(cache_lru_eviction);
}
