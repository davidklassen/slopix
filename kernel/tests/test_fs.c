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

TEST(fs_readi_small) {
	struct inode *ip = namei("/hello");
	ASSERT_NOT_NULL(ip, "namei('/hello') should return non-null");
	ilock(ip);
	char buf[64];
	int n = readi(ip, buf, 0, sizeof(buf));
	ASSERT(n > 0, "readi should return bytes read");
	ASSERT(n == (int)ip->size, "readi should read entire file");
	ASSERT(buf[0] == 'H', "first byte should be 'H'");
	iunlock(ip);
	iput(ip);
	return 0;
}

TEST(fs_readi_offset) {
	struct inode *ip = namei("/hello");
	ASSERT_NOT_NULL(ip, "namei('/hello') should return non-null");
	ilock(ip);
	char buf[16];
	int n = readi(ip, buf, 6, 4);
	ASSERT_EQ(n, 4, "readi should read 4 bytes");
	ASSERT(buf[0] == 'f', "offset 6 should be 'f' (from 'from')");
	iunlock(ip);
	iput(ip);
	return 0;
}

TEST(fs_readi_eof) {
	struct inode *ip = namei("/hello");
	ASSERT_NOT_NULL(ip, "namei('/hello') should return non-null");
	ilock(ip);
	unsigned int sz = ip->size;
	char buf[16];
	int n = readi(ip, buf, sz - 2, 16);
	ASSERT_EQ(n, 2, "readi should clamp to EOF");
	iunlock(ip);
	iput(ip);
	return 0;
}

TEST(fs_stati) {
	struct inode *ip = namei("/hello");
	ASSERT_NOT_NULL(ip, "namei('/hello') should return non-null");
	ilock(ip);
	struct stat st;
	stati(ip, &st);
	ASSERT_EQ(st.type, T_FILE, "stat type should be T_FILE");
	ASSERT_EQ(st.ino, ip->inum, "stat ino should match inode");
	ASSERT_EQ(st.size, ip->size, "stat size should match inode");
	ASSERT(st.size > 0, "file should have content");
	iunlock(ip);
	iput(ip);
	return 0;
}

TEST(fs_readi_large) {
	struct inode *ip = namei("/large");
	ASSERT_NOT_NULL(ip, "namei('/large') should return non-null");
	ilock(ip);
	ASSERT(ip->size > BSIZE, "large file should span multiple blocks");
	char buf[64];
	int n = readi(ip, buf, 0, 4);
	ASSERT_EQ(n, 4, "readi should read 4 bytes");
	ASSERT(buf[0] == 'L', "first byte should be 'L'");
	n = readi(ip, buf, 1024, 4);
	ASSERT_EQ(n, 4, "readi at block boundary should read 4 bytes");
	ASSERT(buf[0] == 's', "byte at offset 1024 should be 's'");
	n = readi(ip, buf, 1022, 8);
	ASSERT_EQ(n, 8, "readi across block boundary should read 8 bytes");
	iunlock(ip);
	iput(ip);
	return 0;
}

TEST_SUITE(fs_read) {
	RUN_TEST(fs_readi_small);
	RUN_TEST(fs_readi_offset);
	RUN_TEST(fs_readi_eof);
	RUN_TEST(fs_stati);
	RUN_TEST(fs_readi_large);
}

#endif
