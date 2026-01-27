#include <stdio.h>
#include <stdlib.h>
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

TEST(snprintf_basic) {
	char buf[20];
	int n = snprintf(buf, sizeof(buf), "%d + %d = %d", 2, 3, 5);
	ASSERT_EQ(strcmp(buf, "2 + 3 = 5"), 0, "snprintf content");
	ASSERT_EQ(n, 9, "snprintf returns length");
	return 0;
}

TEST(snprintf_truncate) {
	char buf[10];
	int n = snprintf(buf, sizeof(buf), "hello world");
	ASSERT_EQ(n, 11, "snprintf returns full length");
	ASSERT_EQ(strcmp(buf, "hello wor"), 0, "snprintf truncates");
	return 0;
}

TEST(sprintf_hex) {
	char buf[20];
	sprintf(buf, "%x %X", 255, 255);
	ASSERT_EQ(strcmp(buf, "ff FF"), 0, "sprintf hex");
	return 0;
}

TEST(sprintf_pointer) {
	char buf[32];
	void *p = (void *)0x12345678;
	sprintf(buf, "%p", p);
	ASSERT(strstr(buf, "12345678") != 0, "sprintf pointer");
	return 0;
}

TEST(sprintf_width) {
	char buf[32];
	sprintf(buf, "%5d", 42);
	ASSERT_EQ(strcmp(buf, "   42"), 0, "sprintf width");
	return 0;
}

TEST(sprintf_zero_pad) {
	char buf[32];
	sprintf(buf, "%08x", 0x1234);
	ASSERT_EQ(strcmp(buf, "00001234"), 0, "sprintf zero pad");
	return 0;
}

TEST(sprintf_string) {
	char buf[32];
	sprintf(buf, "hello %s", "world");
	ASSERT_EQ(strcmp(buf, "hello world"), 0, "sprintf string");
	return 0;
}

TEST(sprintf_char) {
	char buf[32];
	sprintf(buf, "%c%c%c", 'a', 'b', 'c');
	ASSERT_EQ(strcmp(buf, "abc"), 0, "sprintf char");
	return 0;
}

TEST(sprintf_percent) {
	char buf[32];
	sprintf(buf, "100%%");
	ASSERT_EQ(strcmp(buf, "100%"), 0, "sprintf percent");
	return 0;
}

TEST(sprintf_negative) {
	char buf[32];
	sprintf(buf, "%d", -42);
	ASSERT_EQ(strcmp(buf, "-42"), 0, "sprintf negative");
	return 0;
}

TEST(sprintf_unsigned) {
	char buf[32];
	sprintf(buf, "%u", 4294967295U);
	ASSERT_EQ(strcmp(buf, "4294967295"), 0, "sprintf unsigned");
	return 0;
}

TEST(sprintf_long) {
	char buf[32];
	sprintf(buf, "%ld", -1234567890L);
	ASSERT_EQ(strcmp(buf, "-1234567890"), 0, "sprintf long");
	return 0;
}

TEST(sprintf_null_string) {
	char buf[32];
	sprintf(buf, "%s", (char *)0);
	ASSERT_EQ(strcmp(buf, "(null)"), 0, "sprintf null string");
	return 0;
}

TEST(fprintf_basic) {
	FILE *f = fopen("/test_fprintf.txt", "w");
	ASSERT_NOT_NULL(f, "fopen w");
	int n = fprintf(f, "num=%d str=%s\n", 42, "hello");
	ASSERT(n > 0, "fprintf returns positive");
	fclose(f);

	f = fopen("/test_fprintf.txt", "r");
	ASSERT_NOT_NULL(f, "fopen r");
	char buf[50];
	memset(buf, 0, sizeof(buf));
	fread(buf, 1, sizeof(buf) - 1, f);
	ASSERT_EQ(strcmp(buf, "num=42 str=hello\n"), 0, "fprintf content");
	fclose(f);
	unlink("/test_fprintf.txt");
	return 0;
}

TEST(open_memstream_basic) {
	char *buf;
	size_t buflen;
	FILE *f = open_memstream(&buf, &buflen);
	ASSERT_NOT_NULL(f, "open_memstream succeeded");
	fputc('H', f);
	fputc('i', f);
	fflush(f);
	ASSERT_EQ((int)buflen, 2, "buflen is 2");
	ASSERT_EQ(buf[0], 'H', "first char is H");
	ASSERT_EQ(buf[1], 'i', "second char is i");
	ASSERT_EQ(buf[2], '\0', "null terminated");
	fclose(f);
	free(buf);
	return 0;
}

TEST(open_memstream_fprintf) {
	char *buf;
	size_t buflen;
	FILE *f = open_memstream(&buf, &buflen);
	ASSERT_NOT_NULL(f, "open_memstream");
	fprintf(f, "hello %s!", "world");
	fclose(f);
	ASSERT_EQ(strcmp(buf, "hello world!"), 0, "fprintf content");
	ASSERT_EQ((int)buflen, 12, "buflen");
	free(buf);
	return 0;
}

TEST(open_memstream_grow) {
	char *buf;
	size_t buflen;
	FILE *f = open_memstream(&buf, &buflen);
	ASSERT_NOT_NULL(f, "open_memstream");
	for (int i = 0; i < 100; i++) {
		fputc('x', f);
	}
	fclose(f);
	ASSERT_EQ((int)buflen, 100, "buflen is 100");
	ASSERT_EQ(buf[99], 'x', "last char is x");
	ASSERT_EQ(buf[100], '\0', "null terminated");
	free(buf);
	return 0;
}

TEST(open_memstream_chibicc_pattern) {
	char *buf;
	size_t buflen;
	FILE *out = open_memstream(&buf, &buflen);
	ASSERT_NOT_NULL(out, "open_memstream");
	fprintf(out, "  mov x0, #%d\n", 42);
	fprintf(out, "  ret\n");
	fclose(out);
	ASSERT(strstr(buf, "mov x0, #42") != 0, "contains mov");
	ASSERT(strstr(buf, "ret") != 0, "contains ret");
	free(buf);
	return 0;
}

TEST(open_memstream_null_args) {
	FILE *f1 = open_memstream(NULL, NULL);
	ASSERT_NULL(f1, "NULL ptr rejected");
	char *buf;
	FILE *f2 = open_memstream(&buf, NULL);
	ASSERT_NULL(f2, "NULL sizeloc rejected");
	size_t size;
	FILE *f3 = open_memstream(NULL, &size);
	ASSERT_NULL(f3, "NULL ptr rejected 2");
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
	RUN_TEST(snprintf_basic);
	RUN_TEST(snprintf_truncate);
	RUN_TEST(sprintf_hex);
	RUN_TEST(sprintf_pointer);
	RUN_TEST(sprintf_width);
	RUN_TEST(sprintf_zero_pad);
	RUN_TEST(sprintf_string);
	RUN_TEST(sprintf_char);
	RUN_TEST(sprintf_percent);
	RUN_TEST(sprintf_negative);
	RUN_TEST(sprintf_unsigned);
	RUN_TEST(sprintf_long);
	RUN_TEST(sprintf_null_string);
	RUN_TEST(fprintf_basic);
	RUN_TEST(open_memstream_basic);
	RUN_TEST(open_memstream_fprintf);
	RUN_TEST(open_memstream_grow);
	RUN_TEST(open_memstream_chibicc_pattern);
	RUN_TEST(open_memstream_null_args);
}
