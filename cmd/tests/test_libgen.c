#include <libgen.h>
#include <string.h>
#include <test.h>

TEST(dirname_absolute_path) {
	char path[] = "/usr/lib";
	char *result = dirname(path);
	ASSERT_EQ(strcmp(result, "/usr"), 0, "dirname /usr/lib");
	return 0;
}

TEST(dirname_trailing_slash) {
	char path[] = "/usr/";
	char *result = dirname(path);
	ASSERT_EQ(strcmp(result, "/"), 0, "dirname /usr/");
	return 0;
}

TEST(dirname_relative) {
	char path[] = "usr";
	char *result = dirname(path);
	ASSERT_EQ(strcmp(result, "."), 0, "dirname usr");
	return 0;
}

TEST(dirname_root) {
	char path[] = "/";
	char *result = dirname(path);
	ASSERT_EQ(strcmp(result, "/"), 0, "dirname /");
	return 0;
}

TEST(dirname_empty) {
	char path[] = "";
	char *result = dirname(path);
	ASSERT_EQ(strcmp(result, "."), 0, "dirname empty");
	return 0;
}

TEST(dirname_null) {
	char *result = dirname(0);
	ASSERT_EQ(strcmp(result, "."), 0, "dirname null");
	return 0;
}

TEST(dirname_just_filename) {
	char path[] = "file.txt";
	char *result = dirname(path);
	ASSERT_EQ(strcmp(result, "."), 0, "dirname file.txt");
	return 0;
}

TEST(dirname_deep_path) {
	char path[] = "/a/b/c/d";
	char *result = dirname(path);
	ASSERT_EQ(strcmp(result, "/a/b/c"), 0, "dirname /a/b/c/d");
	return 0;
}

TEST(basename_absolute_path) {
	char path[] = "/usr/lib";
	char *result = basename(path);
	ASSERT_EQ(strcmp(result, "lib"), 0, "basename /usr/lib");
	return 0;
}

TEST(basename_trailing_slash) {
	char path[] = "/usr/";
	char *result = basename(path);
	ASSERT_EQ(strcmp(result, "usr"), 0, "basename /usr/");
	return 0;
}

TEST(basename_relative) {
	char path[] = "usr";
	char *result = basename(path);
	ASSERT_EQ(strcmp(result, "usr"), 0, "basename usr");
	return 0;
}

TEST(basename_root) {
	char path[] = "/";
	char *result = basename(path);
	ASSERT_EQ(strcmp(result, "/"), 0, "basename /");
	return 0;
}

TEST(basename_empty) {
	char path[] = "";
	char *result = basename(path);
	ASSERT_EQ(strcmp(result, "."), 0, "basename empty");
	return 0;
}

TEST(basename_null) {
	char *result = basename(0);
	ASSERT_EQ(strcmp(result, "."), 0, "basename null");
	return 0;
}

TEST(basename_just_filename) {
	char path[] = "file.txt";
	char *result = basename(path);
	ASSERT_EQ(strcmp(result, "file.txt"), 0, "basename file.txt");
	return 0;
}

TEST(basename_deep_path) {
	char path[] = "/a/b/c/d";
	char *result = basename(path);
	ASSERT_EQ(strcmp(result, "d"), 0, "basename /a/b/c/d");
	return 0;
}

TEST(dirname_no_modify) {
	char path[] = "/usr/lib";
	dirname(path);
	ASSERT_EQ(strcmp(path, "/usr/lib"), 0, "dirname should not modify input");
	return 0;
}

TEST(basename_no_modify) {
	char path[] = "/usr/lib/";
	basename(path);
	ASSERT_EQ(strcmp(path, "/usr/lib/"), 0, "basename should not modify input");
	return 0;
}

TEST_SUITE(libgen) {
	RUN_TEST(dirname_absolute_path);
	RUN_TEST(dirname_trailing_slash);
	RUN_TEST(dirname_relative);
	RUN_TEST(dirname_root);
	RUN_TEST(dirname_empty);
	RUN_TEST(dirname_null);
	RUN_TEST(dirname_just_filename);
	RUN_TEST(dirname_deep_path);
	RUN_TEST(basename_absolute_path);
	RUN_TEST(basename_trailing_slash);
	RUN_TEST(basename_relative);
	RUN_TEST(basename_root);
	RUN_TEST(basename_empty);
	RUN_TEST(basename_null);
	RUN_TEST(basename_just_filename);
	RUN_TEST(basename_deep_path);
	RUN_TEST(dirname_no_modify);
	RUN_TEST(basename_no_modify);
}
