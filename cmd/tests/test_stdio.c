#include <fcntl.h>
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

TEST(fseek_set) {
	FILE *f = fopen("/test_seek.txt", "w");
	ASSERT_NOT_NULL(f, "fopen w");
	fwrite("hello world", 1, 11, f);
	fclose(f);

	f = fopen("/test_seek.txt", "r");
	ASSERT_NOT_NULL(f, "fopen r");
	ASSERT_EQ(fseek(f, 6, SEEK_SET), 0, "fseek set");
	ASSERT_EQ(fgetc(f), 'w', "read after seek");
	fclose(f);
	unlink("/test_seek.txt");
	return 0;
}

TEST(fseek_cur) {
	FILE *f = fopen("/test_seek2.txt", "w");
	ASSERT_NOT_NULL(f, "fopen w");
	fwrite("abcdefghij", 1, 10, f);
	fclose(f);

	f = fopen("/test_seek2.txt", "r");
	ASSERT_NOT_NULL(f, "fopen r");
	fgetc(f);
	fgetc(f);
	ASSERT_EQ(fseek(f, 3, SEEK_CUR), 0, "fseek cur");
	ASSERT_EQ(fgetc(f), 'f', "read after seek cur");
	fclose(f);
	unlink("/test_seek2.txt");
	return 0;
}

TEST(fseek_end) {
	FILE *f = fopen("/test_seek3.txt", "w");
	ASSERT_NOT_NULL(f, "fopen w");
	fwrite("0123456789", 1, 10, f);
	fclose(f);

	f = fopen("/test_seek3.txt", "r");
	ASSERT_NOT_NULL(f, "fopen r");
	ASSERT_EQ(fseek(f, -3, SEEK_END), 0, "fseek end");
	ASSERT_EQ(fgetc(f), '7', "read after seek end");
	fclose(f);
	unlink("/test_seek3.txt");
	return 0;
}

TEST(ftell_basic) {
	FILE *f = fopen("/test_tell.txt", "w");
	ASSERT_NOT_NULL(f, "fopen w");
	fwrite("hello", 1, 5, f);
	fclose(f);

	f = fopen("/test_tell.txt", "r");
	ASSERT_NOT_NULL(f, "fopen r");
	ASSERT_EQ(ftell(f), 0, "ftell at start");
	fgetc(f);
	fgetc(f);
	ASSERT_EQ(ftell(f), 2, "ftell after 2 reads");
	fseek(f, 0, SEEK_END);
	ASSERT_EQ(ftell(f), 5, "ftell at end");
	fclose(f);
	unlink("/test_tell.txt");
	return 0;
}

TEST(feof_not_eof) {
	FILE *f = fopen("/test_eof.txt", "w");
	ASSERT_NOT_NULL(f, "fopen w");
	fwrite("x", 1, 1, f);
	fclose(f);

	f = fopen("/test_eof.txt", "r");
	ASSERT_NOT_NULL(f, "fopen r");
	ASSERT_EQ(feof(f), 0, "not eof initially");
	fgetc(f);
	ASSERT_EQ(feof(f), 0, "not eof after read");
	fclose(f);
	unlink("/test_eof.txt");
	return 0;
}

TEST(feof_at_eof) {
	FILE *f = fopen("/test_eof2.txt", "w");
	ASSERT_NOT_NULL(f, "fopen w");
	fwrite("x", 1, 1, f);
	fclose(f);

	f = fopen("/test_eof2.txt", "r");
	ASSERT_NOT_NULL(f, "fopen r");
	fgetc(f);
	int c = fgetc(f);
	ASSERT_EQ(c, EOF, "returns EOF");
	ASSERT(feof(f) != 0, "feof set after EOF");
	fclose(f);
	unlink("/test_eof2.txt");
	return 0;
}

TEST(fseek_clears_eof) {
	FILE *f = fopen("/test_eof3.txt", "w");
	ASSERT_NOT_NULL(f, "fopen w");
	fwrite("ab", 1, 2, f);
	fclose(f);

	f = fopen("/test_eof3.txt", "r");
	ASSERT_NOT_NULL(f, "fopen r");
	fgetc(f);
	fgetc(f);
	fgetc(f);
	ASSERT(feof(f) != 0, "eof set");
	fseek(f, 0, SEEK_SET);
	ASSERT_EQ(feof(f), 0, "fseek clears eof");
	ASSERT_EQ(fgetc(f), 'a', "can read after fseek");
	fclose(f);
	unlink("/test_eof3.txt");
	return 0;
}

TEST(memstream_large) {
	// Test memstream with >4096 bytes directly (no file I/O)
	char *buf;
	size_t buflen;
	FILE *f = open_memstream(&buf, &buflen);
	ASSERT_NOT_NULL(f, "open_memstream");

	// Write 5000 bytes with known pattern
	for (int i = 0; i < 5000; i++) {
		fputc('A' + (i % 26), f);
	}
	fclose(f);

	ASSERT_EQ((int)buflen, 5000, "buflen");
	ASSERT_EQ(buf[0], 'A', "byte 0");
	ASSERT_EQ(buf[2047], 'A' + (2047 % 26), "byte 2047");
	ASSERT_EQ(buf[2048], 'A' + (2048 % 26), "byte 2048");
	ASSERT_EQ(buf[3604], 'A' + (3604 % 26), "byte 3604");
	ASSERT_EQ(buf[3605], 'A' + (3605 % 26), "byte 3605");
	ASSERT_EQ(buf[4095], 'A' + (4095 % 26), "byte 4095");
	ASSERT_EQ(buf[4096], 'A' + (4096 % 26), "byte 4096");
	ASSERT_EQ(buf[4999], 'A' + (4999 % 26), "byte 4999");

	free(buf);
	return 0;
}

TEST(large_file_read) {
	// Write a 5000 byte file with known pattern
	FILE *f = fopen("/test_large.txt", "w");
	ASSERT_NOT_NULL(f, "fopen w");
	for (int i = 0; i < 5000; i++) {
		fputc('A' + (i % 26), f);
	}
	fclose(f);

	// Read using same pattern as chibicc's read_file
	f = fopen("/test_large.txt", "r");
	ASSERT_NOT_NULL(f, "fopen r");

	char *buf;
	size_t buflen;
	FILE *out = open_memstream(&buf, &buflen);
	ASSERT_NOT_NULL(out, "open_memstream");

	char *buf2 = malloc(4096);
	for (;;) {
		int n = fread(buf2, 1, 4096, f);
		if (n == 0) {
			break;
		}
		fwrite(buf2, 1, n, out);
	}
	free(buf2);
	fclose(f);
	fclose(out);

	ASSERT_EQ((int)buflen, 5000, "buflen is 5000");

	// Check bytes around the problem area (3605)
	ASSERT_EQ(buf[3600], 'A' + (3600 % 26), "byte 3600");
	ASSERT_EQ(buf[3605], 'A' + (3605 % 26), "byte 3605");
	ASSERT_EQ(buf[3610], 'A' + (3610 % 26), "byte 3610");
	ASSERT_EQ(buf[4000], 'A' + (4000 % 26), "byte 4000");
	ASSERT_EQ(buf[4999], 'A' + (4999 % 26), "byte 4999");

	free(buf);
	unlink("/test_large.txt");
	return 0;
}

TEST(large_file_read_content) {
	// Write file with specific content at byte 3605
	FILE *f = fopen("/test_large2.txt", "w");
	ASSERT_NOT_NULL(f, "fopen w");
	for (int i = 0; i < 3605; i++) {
		fputc('X', f);
	}
	fputc('!', f); // byte 3605 should be '!'
	for (int i = 3606; i < 5000; i++) {
		fputc('Y', f);
	}
	fclose(f);

	// Read it back
	f = fopen("/test_large2.txt", "r");
	ASSERT_NOT_NULL(f, "fopen r");

	char *buf;
	size_t buflen;
	FILE *out = open_memstream(&buf, &buflen);
	ASSERT_NOT_NULL(out, "open_memstream");

	char *buf2 = malloc(4096);
	for (;;) {
		int n = fread(buf2, 1, 4096, f);
		if (n == 0) {
			break;
		}
		fwrite(buf2, 1, n, out);
	}
	free(buf2);
	fclose(f);
	fclose(out);

	ASSERT_EQ((int)buflen, 5000, "buflen");
	ASSERT_EQ(buf[3604], 'X', "byte before marker");
	ASSERT_EQ(buf[3605], '!', "marker byte 3605");
	ASSERT_EQ(buf[3606], 'Y', "byte after marker");

	free(buf);
	unlink("/test_large2.txt");
	return 0;
}

// Test writing blocks one at a time
TEST(blockwise_file_rw) {
	int fd = open("/test_block.txt", O_WRONLY | O_CREAT | O_TRUNC);
	ASSERT_TRUE(fd >= 0, "open for write");

	// Write 5 blocks (1024 bytes each)
	char wbuf[1024];
	for (int block = 0; block < 5; block++) {
		for (int i = 0; i < 1024; i++) {
			int global_i = block * 1024 + i;
			wbuf[i] = 'A' + (global_i % 26);
		}
		int n = write(fd, wbuf, 1024);
		ASSERT_EQ(n, 1024, "write block");
	}
	close(fd);

	// Read back block by block
	fd = open("/test_block.txt", O_RDONLY);
	ASSERT_TRUE(fd >= 0, "open for read");

	char rbuf[1024];
	for (int block = 0; block < 5; block++) {
		int n = read(fd, rbuf, 1024);
		ASSERT_EQ(n, 1024, "read block");

		for (int i = 0; i < 1024; i++) {
			int global_i = block * 1024 + i;
			char expected = 'A' + (global_i % 26);
			if (rbuf[i] != expected) {
				// Found first mismatch
				close(fd);
				unlink("/test_block.txt");
				ASSERT_EQ(global_i, -1, "byte mismatch");
			}
		}
	}
	close(fd);
	unlink("/test_block.txt");
	return 0;
}

// Test using raw syscalls to bypass stdio - single large write
TEST(raw_large_file_rw) {
	// Write using raw write()
	int fd = open("/test_raw.txt", O_WRONLY | O_CREAT | O_TRUNC);
	ASSERT_TRUE(fd >= 0, "open for write");

	char *wbuf = malloc(5000);
	for (int i = 0; i < 5000; i++) {
		wbuf[i] = 'A' + (i % 26);
	}
	int written = write(fd, wbuf, 5000);
	ASSERT_EQ(written, 5000, "write 5000");
	close(fd);

	// Read using raw read()
	fd = open("/test_raw.txt", O_RDONLY);
	ASSERT_TRUE(fd >= 0, "open for read");

	char *rbuf = malloc(5000);
	int total = 0;
	while (total < 5000) {
		int n = read(fd, rbuf + total, 5000 - total);
		if (n <= 0) {
			break;
		}
		total += n;
	}
	close(fd);

	ASSERT_EQ(total, 5000, "read 5000 bytes");

	// Find first mismatch
	int bad_byte = -1;
	for (int i = 0; i < 5000; i++) {
		char expected = 'A' + (i % 26);
		if (rbuf[i] != expected) {
			bad_byte = i;
			break;
		}
	}
	ASSERT_EQ(bad_byte, -1, "all bytes match");

	free(wbuf);
	free(rbuf);
	unlink("/test_raw.txt");
	return 0;
}

// Test fwrite to memstream directly
TEST(fwrite_to_memstream) {
	char *inbuf = malloc(5000);
	for (int i = 0; i < 5000; i++) {
		inbuf[i] = 'A' + (i % 26);
	}

	char *buf;
	size_t buflen;
	FILE *out = open_memstream(&buf, &buflen);
	ASSERT_NOT_NULL(out, "open_memstream");

	// Write in 4096-byte chunks like the failing test
	size_t written = fwrite(inbuf, 1, 4096, out);
	ASSERT_EQ((int)written, 4096, "first fwrite");

	written = fwrite(inbuf + 4096, 1, 904, out);
	ASSERT_EQ((int)written, 904, "second fwrite");

	fclose(out);

	ASSERT_EQ((int)buflen, 5000, "buflen");

	int bad_byte = -1;
	for (int i = 0; i < 5000; i++) {
		char expected = 'A' + (i % 26);
		if (buf[i] != expected) {
			bad_byte = i;
			break;
		}
	}
	ASSERT_EQ(bad_byte, -1, "all bytes match");

	free(inbuf);
	free(buf);
	return 0;
}

// Simple test: raw write, then fgetc read one byte at a time
TEST(write_raw_fgetc_simple) {
	// Write using raw write()
	int fd = open("/test_fgetc.txt", O_WRONLY | O_CREAT | O_TRUNC);
	ASSERT_TRUE(fd >= 0, "open for write");

	char *wbuf = malloc(5000);
	for (int i = 0; i < 5000; i++) {
		wbuf[i] = 'A' + (i % 26);
	}
	write(fd, wbuf, 5000);
	close(fd);

	// Read using fgetc directly
	FILE *f = fopen("/test_fgetc.txt", "r");
	ASSERT_NOT_NULL(f, "fopen r");

	int bad_byte = -1;
	for (int i = 0; i < 5000; i++) {
		int c = fgetc(f);
		if (c == EOF) {
			bad_byte = i;
			break;
		}
		char expected = 'A' + (i % 26);
		if ((char)c != expected) {
			bad_byte = i;
			break;
		}
	}
	fclose(f);

	ASSERT_EQ(bad_byte, -1, "all bytes match");
	free(wbuf);
	unlink("/test_fgetc.txt");
	return 0;
}

// Write with raw syscalls, read with stdio
TEST(write_raw_read_stdio) {
	// Write using raw write()
	int fd = open("/test_mixed1.txt", O_WRONLY | O_CREAT | O_TRUNC);
	ASSERT_TRUE(fd >= 0, "open for write");

	char *wbuf = malloc(5000);
	for (int i = 0; i < 5000; i++) {
		wbuf[i] = 'A' + (i % 26);
	}
	write(fd, wbuf, 5000);
	close(fd);

	// Read using stdio fread
	FILE *f = fopen("/test_mixed1.txt", "r");
	ASSERT_NOT_NULL(f, "fopen r");

	char *buf;
	size_t buflen;
	FILE *out = open_memstream(&buf, &buflen);

	char *buf2 = malloc(4096);
	for (;;) {
		int n = fread(buf2, 1, 4096, f);
		if (n == 0) {
			break;
		}
		fwrite(buf2, 1, n, out);
	}
	free(buf2);
	fclose(f);
	fclose(out);

	ASSERT_EQ((int)buflen, 5000, "buflen");

	int bad_byte = -1;
	for (int i = 0; i < 5000; i++) {
		char expected = 'A' + (i % 26);
		if (buf[i] != expected) {
			bad_byte = i;
			break;
		}
	}
	ASSERT_EQ(bad_byte, -1, "byte 3605");

	free(wbuf);
	free(buf);
	unlink("/test_mixed1.txt");
	return 0;
}

// Write with stdio, read with raw syscalls
TEST(write_stdio_read_raw) {
	// Write using stdio fputc
	FILE *f = fopen("/test_mixed2.txt", "w");
	ASSERT_NOT_NULL(f, "fopen w");
	for (int i = 0; i < 5000; i++) {
		fputc('A' + (i % 26), f);
	}
	fclose(f);

	// Read using raw read()
	int fd = open("/test_mixed2.txt", O_RDONLY);
	ASSERT_TRUE(fd >= 0, "open for read");

	char *rbuf = malloc(5000);
	int total = 0;
	while (total < 5000) {
		int n = read(fd, rbuf + total, 5000 - total);
		if (n <= 0) {
			break;
		}
		total += n;
	}
	close(fd);

	ASSERT_EQ(total, 5000, "read 5000 bytes");
	ASSERT_EQ(rbuf[3605], 'A' + (3605 % 26), "byte 3605");

	free(rbuf);
	unlink("/test_mixed2.txt");
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
	RUN_TEST(fseek_set);
	RUN_TEST(fseek_cur);
	RUN_TEST(fseek_end);
	RUN_TEST(ftell_basic);
	RUN_TEST(feof_not_eof);
	RUN_TEST(feof_at_eof);
	RUN_TEST(fseek_clears_eof);
	RUN_TEST(memstream_large);
	RUN_TEST(blockwise_file_rw);
	RUN_TEST(raw_large_file_rw);
	RUN_TEST(fwrite_to_memstream);
	RUN_TEST(write_raw_fgetc_simple);
	RUN_TEST(write_raw_read_stdio);
	RUN_TEST(write_stdio_read_raw);
	RUN_TEST(large_file_read);
	RUN_TEST(large_file_read_content);
}
