#ifdef RUN_TESTS

#include "test.h"
#include "fs.h"
#include "proc.h"

static struct superblock test_sb;

TEST(fs_superblock_magic) {
	readsb(0, &test_sb);
	ASSERT_EQ(test_sb.magic, FSMAGIC, "superblock magic should be 0x10203040");
	return 0;
}

TEST(fs_superblock_inodestart) {
	readsb(0, &test_sb);
	ASSERT_EQ(test_sb.inodestart, 2, "inodestart should be 2");
	return 0;
}

TEST(fs_superblock_sizes) {
	readsb(0, &test_sb);
	ASSERT(test_sb.size > 0, "size should be > 0");
	ASSERT(test_sb.nblocks > 0, "nblocks should be > 0");
	ASSERT(test_sb.ninodes > 0, "ninodes should be > 0");
	return 0;
}

TEST(fs_iget_root) {
	struct inode *ip = iget(0, ROOTINO);
	ASSERT_NOT_NULL(ip, "iget should return non-null inode");
	ASSERT_EQ(ip->dev, 0, "inode dev should be 0");
	ASSERT_EQ(ip->inum, ROOTINO, "inode inum should be ROOTINO");
	ASSERT(ip->ref >= 1, "inode ref should be >= 1");
	iput(ip);
	return 0;
}

TEST(fs_iget_same_twice) {
	struct inode *ip1 = iget(0, ROOTINO);
	int ref1 = ip1->ref;
	struct inode *ip2 = iget(0, ROOTINO);
	ASSERT_EQ(ip1, ip2, "same inode should return same pointer");
	ASSERT_EQ(ip2->ref, ref1 + 1, "ref should increment");
	iput(ip1);
	iput(ip2);
	return 0;
}

TEST(fs_ilock_root_type) {
	struct inode *ip = iget(0, ROOTINO);
	ilock(ip);
	ASSERT_EQ(ip->type, T_DIR, "root inode type should be T_DIR");
	iunlock(ip);
	iput(ip);
	return 0;
}

TEST(fs_ilock_root_entries) {
	struct inode *ip = iget(0, ROOTINO);
	ilock(ip);
	ASSERT(ip->size >= 32, "root dir size should be >= 32");
	ASSERT(ip->nlink >= 1, "root dir nlink should be >= 1");
	iunlock(ip);
	iput(ip);
	return 0;
}

TEST(fs_bmap_root_direct) {
	struct inode *ip = iget(0, ROOTINO);
	ilock(ip);
	unsigned int bn = bmap(ip, 0);
	ASSERT(bn > 0, "bmap(0) should return valid block number");
	iunlock(ip);
	iput(ip);
	return 0;
}

TEST_SUITE(fs) {
	RUN_TEST(fs_superblock_magic);
	RUN_TEST(fs_superblock_inodestart);
	RUN_TEST(fs_superblock_sizes);
	RUN_TEST(fs_iget_root);
	RUN_TEST(fs_iget_same_twice);
	RUN_TEST(fs_ilock_root_type);
	RUN_TEST(fs_ilock_root_entries);
	RUN_TEST(fs_bmap_root_direct);
}

TEST(fs_dir_dot) {
	struct inode *root = iget(0, ROOTINO);
	ilock(root);
	struct inode *dot = dirlookup(root, ".", 0);
	ASSERT_NOT_NULL(dot, "dirlookup should find '.'");
	ASSERT_EQ(dot->inum, ROOTINO, "'.' should be root inode");
	iput(dot);
	iunlock(root);
	iput(root);
	return 0;
}

TEST(fs_dir_dotdot) {
	struct inode *root = iget(0, ROOTINO);
	ilock(root);
	struct inode *dotdot = dirlookup(root, "..", 0);
	ASSERT_NOT_NULL(dotdot, "dirlookup should find '..'");
	ASSERT_EQ(dotdot->inum, ROOTINO, "'..' should be root inode");
	iput(dotdot);
	iunlock(root);
	iput(root);
	return 0;
}

TEST(fs_namei_root) {
	struct inode *ip = namei("/");
	ASSERT_NOT_NULL(ip, "namei('/') should return non-null");
	ASSERT_EQ(ip->inum, ROOTINO, "namei('/') should return root inode");
	ilock(ip);
	ASSERT_EQ(ip->type, T_DIR, "root should be T_DIR");
	iunlock(ip);
	iput(ip);
	return 0;
}

TEST(fs_namei_file) {
	struct inode *ip = namei("/hello");
	ASSERT_NOT_NULL(ip, "namei('/hello') should return non-null");
	ilock(ip);
	ASSERT_EQ(ip->type, T_FILE, "hello should be T_FILE");
	iunlock(ip);
	iput(ip);
	return 0;
}

TEST(fs_namei_relative) {
	struct proc fake_proc;
	fake_proc.cwd = iget(0, ROOTINO);
	ilock(fake_proc.cwd);
	iunlock(fake_proc.cwd);

	struct proc *saved_current = current;
	current = &fake_proc;

	struct inode *ip = namei("hello");
	ASSERT_NOT_NULL(ip, "namei('hello') should return non-null");
	ilock(ip);
	ASSERT_EQ(ip->type, T_FILE, "hello should be T_FILE");
	iunlock(ip);
	iput(ip);

	current = saved_current;
	iput(fake_proc.cwd);
	return 0;
}

TEST(fs_namei_relative_dot) {
	struct proc fake_proc;
	fake_proc.cwd = iget(0, ROOTINO);
	ilock(fake_proc.cwd);
	iunlock(fake_proc.cwd);

	struct proc *saved_current = current;
	current = &fake_proc;

	struct inode *ip = namei(".");
	ASSERT_NOT_NULL(ip, "namei('.') should return non-null");
	ASSERT_EQ(ip->inum, ROOTINO, "namei('.') should return root");

	iput(ip);
	current = saved_current;
	iput(fake_proc.cwd);
	return 0;
}

TEST_SUITE(fs_dir) {
	RUN_TEST(fs_dir_dot);
	RUN_TEST(fs_dir_dotdot);
	RUN_TEST(fs_namei_root);
	RUN_TEST(fs_namei_file);
	RUN_TEST(fs_namei_relative);
	RUN_TEST(fs_namei_relative_dot);
}

#endif
