#include <test.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>

TEST(opendir_root) {
	DIR *d = opendir("/");
	ASSERT(d != 0, "opendir root succeeds");
	closedir(d);
	return 0;
}

TEST(readdir_finds_entries) {
	DIR *d = opendir("/");
	ASSERT(d != 0, "opendir succeeds");
	int found_dev = 0;
	int found_hello = 0;
	struct dirent *ent;
	while ((ent = readdir(d)) != 0) {
		if (strcmp(ent->d_name, "dev") == 0) {
			found_dev = 1;
		}
		if (strcmp(ent->d_name, "hello") == 0) {
			found_hello = 1;
		}
	}
	closedir(d);
	ASSERT(found_dev, "found /dev");
	ASSERT(found_hello, "found /hello");
	return 0;
}

TEST(readdir_dots) {
	DIR *d = opendir("/");
	ASSERT(d != 0, "opendir succeeds");
	int found_dot = 0;
	int found_dotdot = 0;
	struct dirent *ent;
	while ((ent = readdir(d)) != 0) {
		if (strcmp(ent->d_name, ".") == 0) {
			found_dot = 1;
		}
		if (strcmp(ent->d_name, "..") == 0) {
			found_dotdot = 1;
		}
	}
	closedir(d);
	ASSERT(found_dot, "found .");
	ASSERT(found_dotdot, "found ..");
	return 0;
}

TEST(opendir_nonexistent) {
	DIR *d = opendir("/nonexistent_dir");
	ASSERT(d == 0, "opendir nonexistent returns NULL");
	return 0;
}

TEST(closedir_works) {
	DIR *d = opendir("/");
	ASSERT(d != 0, "opendir succeeds");
	int r = closedir(d);
	ASSERT_EQ(r, 0, "closedir returns 0");
	return 0;
}

TEST_SUITE(dirent) {
	RUN_TEST(opendir_root);
	RUN_TEST(readdir_finds_entries);
	RUN_TEST(readdir_dots);
	RUN_TEST(opendir_nonexistent);
	RUN_TEST(closedir_works);
}
