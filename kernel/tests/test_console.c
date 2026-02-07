#include "test.h"
#include "console.h"
#include "file.h"

TEST(console_devsw_registered) {
	ASSERT_NOT_NULL(devsw[CONSOLE].read, "console read should be registered");
	ASSERT_NOT_NULL(devsw[CONSOLE].write, "console write should be registered");
	return 0;
}

TEST(console_write_basic) {
	const char *msg = "test";
	int n = devsw[CONSOLE].write(msg, 4);
	ASSERT_EQ(n, 4, "consolewrite should return bytes written");
	return 0;
}

TEST(console_file_device_type) {
	struct file *f = filealloc();
	ASSERT_NOT_NULL(f, "filealloc should succeed");
	f->type = FD_DEVICE;
	f->major = CONSOLE;
	f->writable = 1;

	ASSERT_EQ(f->type, FD_DEVICE, "file type should be FD_DEVICE");
	ASSERT_EQ(f->major, CONSOLE, "file major should be CONSOLE");

	fileclose(f);
	return 0;
}

TEST(console_filewrite) {
	struct file *f = filealloc();
	ASSERT_NOT_NULL(f, "filealloc should succeed");
	f->type = FD_DEVICE;
	f->major = CONSOLE;
	f->writable = 1;
	f->readable = 0;

	const char *msg = "hello";
	int n = filewrite(f, msg, 5);
	ASSERT_EQ(n, 5, "filewrite should return bytes written");

	fileclose(f);
	return 0;
}

TEST(console_filewrite_not_writable) {
	struct file *f = filealloc();
	ASSERT_NOT_NULL(f, "filealloc should succeed");
	f->type = FD_DEVICE;
	f->major = CONSOLE;
	f->writable = 0;
	f->readable = 1;

	const char *msg = "hello";
	int n = filewrite(f, msg, 5);
	ASSERT_EQ(n, -1, "filewrite should fail if not writable");

	fileclose(f);
	return 0;
}

TEST(console_raw_mode_default) {
	ASSERT_EQ(console_get_raw(), 0, "raw mode off by default");
	return 0;
}

TEST(console_raw_mode_set) {
	console_set_raw(1);
	ASSERT_EQ(console_get_raw(), 1, "raw mode on");
	console_set_raw(0);
	ASSERT_EQ(console_get_raw(), 0, "raw mode off");
	return 0;
}

TEST_SUITE(console) {
	RUN_TEST(console_devsw_registered);
	RUN_TEST(console_write_basic);
	RUN_TEST(console_file_device_type);
	RUN_TEST(console_filewrite);
	RUN_TEST(console_filewrite_not_writable);
	RUN_TEST(console_raw_mode_default);
	RUN_TEST(console_raw_mode_set);
}
