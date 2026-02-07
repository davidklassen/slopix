#ifdef RUN_TESTS

#include "test.h"
#include "sync.h"
#include "cpu.h"

TEST(spinlock_init) {
	struct spinlock lk;
	spin_init(&lk, "test");
	ASSERT_EQ(lk.locked, 0, "spinlock starts unlocked");
	ASSERT_EQ(spin_holding(&lk), 0, "not holding");
	return 0;
}

TEST(spinlock_lock_unlock) {
	struct spinlock lk;
	spin_init(&lk, "test");

	spin_lock(&lk);
	ASSERT_EQ(spin_holding(&lk), 1, "holding after lock");

	spin_unlock(&lk);
	ASSERT_EQ(spin_holding(&lk), 0, "not holding after unlock");
	return 0;
}

TEST(spinlock_disables_irq) {
	struct spinlock lk;
	spin_init(&lk, "test");

	enable_irq();
	unsigned long before = read_daif();

	spin_lock(&lk);
	unsigned long during = read_daif();

	spin_unlock(&lk);
	unsigned long after = read_daif();

	ASSERT_EQ(before & DAIF_IRQ_BIT, 0, "IRQ enabled before");
	ASSERT_EQ(during & DAIF_IRQ_BIT, DAIF_IRQ_BIT, "IRQ disabled during");
	ASSERT_EQ(after & DAIF_IRQ_BIT, 0, "IRQ enabled after");
	return 0;
}

TEST(sleeplock_init) {
	struct sleeplock lk;
	sleep_init(&lk, "test");
	ASSERT_EQ(lk.locked, 0, "sleeplock starts unlocked");
	ASSERT_EQ(sleep_holding(&lk), 0, "not holding");
	return 0;
}

TEST(sleeplock_lock_unlock) {
	struct sleeplock lk;
	sleep_init(&lk, "test");

	sleep_lock(&lk);
	ASSERT_EQ(sleep_holding(&lk), 1, "holding after lock");

	sleep_unlock(&lk);
	ASSERT_EQ(sleep_holding(&lk), 0, "not holding after unlock");
	return 0;
}

TEST(sleeplock_allows_irq) {
	struct sleeplock lk;
	sleep_init(&lk, "test");

	enable_irq();
	sleep_lock(&lk);
	unsigned long during = read_daif();
	sleep_unlock(&lk);

	ASSERT_EQ(during & DAIF_IRQ_BIT, 0, "IRQ enabled while holding sleeplock");
	return 0;
}

TEST(spinlock_static_init) {
	static struct spinlock lk = SPINLOCK_INIT("static");
	ASSERT_EQ(lk.locked, 0, "static init unlocked");
	spin_lock(&lk);
	ASSERT_EQ(lk.locked, 1, "can lock static");
	spin_unlock(&lk);
	return 0;
}

TEST(sleeplock_static_init) {
	static struct sleeplock lk = SLEEPLOCK_INIT("static");
	ASSERT_EQ(lk.locked, 0, "static init unlocked");
	sleep_lock(&lk);
	ASSERT_EQ(lk.locked, 1, "can lock static");
	sleep_unlock(&lk);
	return 0;
}

TEST_SUITE(sync) {
	RUN_TEST(spinlock_init);
	RUN_TEST(spinlock_lock_unlock);
	RUN_TEST(spinlock_disables_irq);
	RUN_TEST(sleeplock_init);
	RUN_TEST(sleeplock_lock_unlock);
	RUN_TEST(sleeplock_allows_irq);
	RUN_TEST(spinlock_static_init);
	RUN_TEST(sleeplock_static_init);
}

#endif
