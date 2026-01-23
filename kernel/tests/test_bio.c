#ifdef RUN_TESTS

#include "test.h"
#include "bio.h"

#define SCRATCH_BLOCK_1 50
#define SCRATCH_BLOCK_2 51
#define SCRATCH_BLOCK_3 52

TEST(bread_returns_buffer) {
	struct buf *b = bread(0, SCRATCH_BLOCK_1);
	ASSERT_NOT_NULL(b, "bread should return non-null buffer");
	ASSERT_EQ(b->valid, 1, "buffer should be valid after bread");
	brelse(b);
	return 0;
}

TEST(brelse_frees_buffer) {
	struct buf *b = bread(0, SCRATCH_BLOCK_1);
	unsigned int refcnt_before = b->refcnt;
	brelse(b);
	ASSERT_EQ(b->refcnt, refcnt_before - 1, "refcnt should decrement after brelse");
	return 0;
}

TEST(bread_cache_hit) {
	struct buf *b1 = bread(0, SCRATCH_BLOCK_1);
	b1->data[0] = 0xAB;
	brelse(b1);

	struct buf *b2 = bread(0, SCRATCH_BLOCK_1);
	ASSERT_EQ(b2, b1, "second read should return same buffer");
	ASSERT_EQ(b2->data[0], 0xAB, "cached data should persist");
	brelse(b2);
	return 0;
}

TEST(bread_cache_miss) {
	struct buf *b1 = bread(0, SCRATCH_BLOCK_1);
	struct buf *b2 = bread(0, SCRATCH_BLOCK_2);
	ASSERT_NE(b1, b2, "different blocks should use different buffers");
	brelse(b1);
	brelse(b2);
	return 0;
}

TEST(bwrite_persists_data) {
	struct buf *b = bread(0, SCRATCH_BLOCK_3);
	for (int i = 0; i < BSIZE; i++) {
		b->data[i] = (unsigned char)(i & 0xFF);
	}
	bwrite(b);

	// Invalidate before release to force re-read from disk on next bread().
	// This tests that bwrite() actually persists data to disk.
	b->valid = 0;
	brelse(b);

	b = bread(0, SCRATCH_BLOCK_3);
	for (int i = 0; i < BSIZE; i++) {
		if (b->data[i] != (unsigned char)(i & 0xFF)) {
			brelse(b);
			ASSERT(0, "data mismatch after write-read");
		}
	}
	brelse(b);
	return 0;
}

TEST(cache_lru_eviction) {
	struct buf *first = bread(0, SCRATCH_BLOCK_1);
	first->data[0] = 0x42;
	brelse(first);

	for (unsigned int i = 0; i < NBUF; i++) {
		struct buf *b = bread(0, 1000 + i);
		brelse(b);
	}

	struct buf *evicted = bread(0, SCRATCH_BLOCK_1);
	ASSERT_NE(evicted, first, "buffer should have been evicted");
	brelse(evicted);
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

#endif
