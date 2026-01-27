#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <test.h>

TEST(stdio_stdin_exists) {
	ASSERT_NOT_NULL(stdin, "stdin exists");
	return 0;
}

TEST(stdio_stdout_exists) {
	ASSERT_NOT_NULL(stdout, "stdout exists");
	return 0;
}

TEST(stdio_stderr_exists) {
	ASSERT_NOT_NULL(stderr, "stderr exists");
	return 0;
}

TEST(fileno_stdin) {
	ASSERT_EQ(fileno(stdin), 0, "stdin fd");
	return 0;
}

TEST(fileno_stdout) {
	ASSERT_EQ(fileno(stdout), 1, "stdout fd");
	return 0;
}

TEST(fileno_stderr) {
	ASSERT_EQ(fileno(stderr), 2, "stderr fd");
	return 0;
}

TEST(fileno_null) {
	ASSERT_EQ(fileno(0), -1, "null returns -1");
	return 0;
}

TEST(stdio_constants) {
	ASSERT_EQ(EOF, -1, "EOF");
	ASSERT_EQ(BUFSIZ, 1024, "BUFSIZ");
	return 0;
}

TEST(fopen_write_mode) {
	FILE *f = fopen("/test_write.txt", "w");
	ASSERT_NOT_NULL(f, "fopen w mode");
	fclose(f);
	unlink("/test_write.txt");
	return 0;
}

TEST(fopen_read_nonexistent) {
	FILE *f = fopen("/nonexistent_file_12345.txt", "r");
	ASSERT_NULL(f, "fopen nonexistent returns NULL");
	return 0;
}

TEST(fopen_invalid_mode) {
	FILE *f = fopen("/test.txt", "x");
	ASSERT_NULL(f, "fopen invalid mode returns NULL");
	return 0;
}

TEST(fwrite_fread_basic) {
	FILE *f = fopen("/test_rw.txt", "w");
	ASSERT_NOT_NULL(f, "fopen w");
	const char *msg = "hello world";
	size_t n = fwrite(msg, 1, strlen(msg), f);
	ASSERT_EQ((int)n, (int)strlen(msg), "fwrite count");
	fclose(f);

	f = fopen("/test_rw.txt", "r");
	ASSERT_NOT_NULL(f, "fopen r");
	char buf[32];
	memset(buf, 0, sizeof(buf));
	n = fread(buf, 1, sizeof(buf) - 1, f);
	ASSERT_EQ((int)n, (int)strlen(msg), "fread count");
	ASSERT_EQ(strcmp(buf, msg), 0, "fread content");
	fclose(f);
	unlink("/test_rw.txt");
	return 0;
}

TEST(fputc_fgetc_basic) {
	FILE *f = fopen("/test_char.txt", "w");
	ASSERT_NOT_NULL(f, "fopen w");
	ASSERT_EQ(fputc('A', f), 'A', "fputc A");
	ASSERT_EQ(fputc('B', f), 'B', "fputc B");
	ASSERT_EQ(fputc('C', f), 'C', "fputc C");
	fclose(f);

	f = fopen("/test_char.txt", "r");
	ASSERT_NOT_NULL(f, "fopen r");
	ASSERT_EQ(fgetc(f), 'A', "fgetc A");
	ASSERT_EQ(fgetc(f), 'B', "fgetc B");
	ASSERT_EQ(fgetc(f), 'C', "fgetc C");
	ASSERT_EQ(fgetc(f), EOF, "fgetc EOF");
	fclose(f);
	unlink("/test_char.txt");
	return 0;
}

TEST(fflush_write) {
	FILE *f = fopen("/test_flush.txt", "w");
	ASSERT_NOT_NULL(f, "fopen w");
	fputc('X', f);
	ASSERT_EQ(fflush(f), 0, "fflush returns 0");
	fclose(f);

	f = fopen("/test_flush.txt", "r");
	ASSERT_NOT_NULL(f, "fopen r");
	ASSERT_EQ(fgetc(f), 'X', "fgetc X");
	fclose(f);
	unlink("/test_flush.txt");
	return 0;
}

TEST(fread_partial) {
	FILE *f = fopen("/test_partial.txt", "w");
	ASSERT_NOT_NULL(f, "fopen w");
	fwrite("abc", 1, 3, f);
	fclose(f);

	f = fopen("/test_partial.txt", "r");
	ASSERT_NOT_NULL(f, "fopen r");
	char buf[10];
	size_t n = fread(buf, 1, 10, f);
	ASSERT_EQ((int)n, 3, "fread partial count");
	fclose(f);
	unlink("/test_partial.txt");
	return 0;
}

TEST(fwrite_buffer_boundary) {
	FILE *f = fopen("/test_large.txt", "w");
	ASSERT_NOT_NULL(f, "fopen w");
	char buf[128];
	memset(buf, 'Z', sizeof(buf));
	int total = 0;
	for (int i = 0; i < 16; i++) {
		size_t n = fwrite(buf, 1, sizeof(buf), f);
		ASSERT_EQ((int)n, (int)sizeof(buf), "fwrite chunk");
		total += (int)n;
	}
	ASSERT_EQ(total, 2048, "total written");
	fclose(f);

	f = fopen("/test_large.txt", "r");
	ASSERT_NOT_NULL(f, "fopen r");
	int read_total = 0;
	int all_z = 1;
	while (1) {
		memset(buf, 0, sizeof(buf));
		size_t n = fread(buf, 1, sizeof(buf), f);
		if (n == 0) {
			break;
		}
		read_total += (int)n;
		for (size_t j = 0; j < n; j++) {
			if (buf[j] != 'Z') {
				all_z = 0;
				break;
			}
		}
	}
	ASSERT_EQ(read_total, 2048, "total read");
	ASSERT_EQ(all_z, 1, "all bytes are Z");
	fclose(f);
	unlink("/test_large.txt");
	return 0;
}

TEST_SUITE(stdio) {
	RUN_TEST(stdio_stdin_exists);
	RUN_TEST(stdio_stdout_exists);
	RUN_TEST(stdio_stderr_exists);
	RUN_TEST(fileno_stdin);
	RUN_TEST(fileno_stdout);
	RUN_TEST(fileno_stderr);
	RUN_TEST(fileno_null);
	RUN_TEST(stdio_constants);
	RUN_TEST(fopen_write_mode);
	RUN_TEST(fopen_read_nonexistent);
	RUN_TEST(fopen_invalid_mode);
	RUN_TEST(fwrite_fread_basic);
	RUN_TEST(fputc_fgetc_basic);
	RUN_TEST(fflush_write);
	RUN_TEST(fread_partial);
	RUN_TEST(fwrite_buffer_boundary);
}
