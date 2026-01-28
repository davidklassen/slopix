#include <test.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define T_DIR	  2
#define T_DEVICE  3
#define T_BDEVICE 4
#define BSIZE	  1024
#define FSMAGIC	  0x10203040

TEST(null_read_eof) {
	int fd = open("/dev/null", O_RDONLY);
	ASSERT(fd >= 0, "open /dev/null");
	char buf[16];
	int r = read(fd, buf, sizeof(buf));
	ASSERT_EQ(r, 0, "null read returns 0 (EOF)");
	close(fd);
	return 0;
}

TEST(null_write_discard) {
	int fd = open("/dev/null", O_WRONLY);
	ASSERT(fd >= 0, "open /dev/null for write");
	const char *msg = "hello world";
	int r = write(fd, msg, 11);
	ASSERT_EQ(r, 11, "null write returns n");
	close(fd);
	return 0;
}

TEST(console_exists) {
	struct stat st;
	int r = stat("/dev/console", &st);
	ASSERT_EQ(r, 0, "stat /dev/console");
	ASSERT_EQ(st.st_mode, T_DEVICE, "console is T_DEVICE");
	return 0;
}

TEST(disk_read_superblock) {
	int fd = open("/dev/disk", O_RDONLY);
	ASSERT(fd >= 0, "open /dev/disk");
	int r = lseek(fd, BSIZE, 0);
	ASSERT_EQ(r, BSIZE, "seek to superblock");
	unsigned int magic;
	r = read(fd, (char *)&magic, sizeof(magic));
	ASSERT_EQ(r, (int)sizeof(magic), "read magic");
	ASSERT_EQ(magic, FSMAGIC, "magic matches");
	close(fd);
	return 0;
}

TEST(disk_seek) {
	int fd = open("/dev/disk", O_RDONLY);
	ASSERT(fd >= 0, "open /dev/disk");
	int r = lseek(fd, 2048, 0);
	ASSERT_EQ(r, 2048, "seek absolute");
	r = lseek(fd, 100, 1);
	ASSERT_EQ(r, 2148, "seek relative");
	close(fd);
	return 0;
}

TEST(dev_dir_exists) {
	struct stat st;
	int r = stat("/dev", &st);
	ASSERT_EQ(r, 0, "stat /dev");
	ASSERT_EQ(st.st_mode, T_DIR, "/dev is T_DIR");
	return 0;
}

TEST_SUITE(devices) {
	RUN_TEST(null_read_eof);
	RUN_TEST(null_write_discard);
	RUN_TEST(console_exists);
	RUN_TEST(disk_read_superblock);
	RUN_TEST(disk_seek);
	RUN_TEST(dev_dir_exists);
}
