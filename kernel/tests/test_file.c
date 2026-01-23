#ifdef RUN_TESTS

#include "test.h"
#include "file.h"
#include "fs.h"
#include "proc.h"

TEST(file_alloc_basic) {
	struct file *f = filealloc();
	ASSERT_NOT_NULL(f, "filealloc should return non-null");
	ASSERT_EQ(f->ref, 1, "new file should have ref=1");
	ASSERT_EQ(f->type, FD_NONE, "new file should have type=FD_NONE");
	fileclose(f);
	return 0;
}

TEST(file_dup_increments_ref) {
	struct file *f = filealloc();
	ASSERT_NOT_NULL(f, "filealloc should return non-null");
	int ref_before = f->ref;
	struct file *f2 = filedup(f);
	ASSERT_EQ(f, f2, "filedup should return same pointer");
	ASSERT_EQ(f->ref, ref_before + 1, "filedup should increment ref");
	fileclose(f);
	fileclose(f2);
	return 0;
}

TEST(file_close_decrements_ref) {
	struct file *f = filealloc();
	filedup(f);
	ASSERT_EQ(f->ref, 2, "ref should be 2 after dup");
	fileclose(f);
	ASSERT_EQ(f->ref, 1, "ref should be 1 after close");
	fileclose(f);
	ASSERT_EQ(f->ref, 0, "ref should be 0 after second close");
	return 0;
}

TEST(file_close_releases_inode) {
	struct inode *ip = fs_namei("/hello");
	ASSERT_NOT_NULL(ip, "namei should find /hello");
	int inode_ref_before = ip->ref;

	struct file *f = filealloc();
	f->type = FD_INODE;
	f->ip = fs_idup(ip);
	ASSERT_EQ(ip->ref, inode_ref_before + 1, "inode ref should increment");

	fileclose(f);
	ASSERT_EQ(ip->ref, inode_ref_before, "inode ref should return to original");

	fs_iput(ip);
	return 0;
}

TEST(file_stat_from_inode) {
	struct inode *ip = fs_namei("/hello");
	ASSERT_NOT_NULL(ip, "namei should find /hello");

	struct file *f = filealloc();
	f->type = FD_INODE;
	f->ip = fs_idup(ip);

	struct stat st;
	int r = filestat(f, &st);
	ASSERT_EQ(r, 0, "filestat should succeed");
	ASSERT_EQ(st.type, T_FILE, "stat type should be T_FILE");
	ASSERT_EQ(st.ino, ip->inum, "stat ino should match");

	fileclose(f);
	fs_iput(ip);
	return 0;
}

TEST(file_read_advances_offset) {
	struct inode *ip = fs_namei("/hello");
	ASSERT_NOT_NULL(ip, "namei should find /hello");

	struct file *f = filealloc();
	f->type = FD_INODE;
	f->ip = fs_idup(ip);
	f->readable = 1;
	f->off = 0;

	char buf[16];
	int n = fileread(f, buf, 4);
	ASSERT_EQ(n, 4, "fileread should read 4 bytes");
	ASSERT_EQ(f->off, 4, "offset should advance to 4");

	n = fileread(f, buf, 4);
	ASSERT_EQ(n, 4, "second fileread should read 4 bytes");
	ASSERT_EQ(f->off, 8, "offset should advance to 8");

	fileclose(f);
	fs_iput(ip);
	return 0;
}

TEST(file_read_not_readable) {
	struct inode *ip = fs_namei("/hello");
	ASSERT_NOT_NULL(ip, "namei should find /hello");

	struct file *f = filealloc();
	f->type = FD_INODE;
	f->ip = fs_idup(ip);
	f->readable = 0;
	f->off = 0;

	char buf[16];
	int n = fileread(f, buf, 4);
	ASSERT_EQ(n, -1, "fileread should fail if not readable");

	fileclose(f);
	fs_iput(ip);
	return 0;
}

TEST(file_fdalloc_lowest) {
	struct proc fake_proc;
	for (int i = 0; i < 16; i++) {
		fake_proc.ofile[i] = 0;
	}
	struct proc *saved = current;
	current = &fake_proc;

	struct file *f1 = filealloc();
	struct file *f2 = filealloc();

	int fd1 = fdalloc(f1);
	ASSERT_EQ(fd1, 0, "first fdalloc should return 0");

	int fd2 = fdalloc(f2);
	ASSERT_EQ(fd2, 1, "second fdalloc should return 1");

	fake_proc.ofile[0] = 0;
	fileclose(f1);

	struct file *f3 = filealloc();
	int fd3 = fdalloc(f3);
	ASSERT_EQ(fd3, 0, "fdalloc should reuse lowest fd");

	fileclose(f2);
	fileclose(f3);
	current = saved;
	return 0;
}

TEST_SUITE(fs_file) {
	RUN_TEST(file_alloc_basic);
	RUN_TEST(file_dup_increments_ref);
	RUN_TEST(file_close_decrements_ref);
	RUN_TEST(file_close_releases_inode);
	RUN_TEST(file_stat_from_inode);
	RUN_TEST(file_read_advances_offset);
	RUN_TEST(file_read_not_readable);
	RUN_TEST(file_fdalloc_lowest);
}

#endif
