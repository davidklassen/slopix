#include <stdio.h>
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

TEST_SUITE(stdio) {
	RUN_TEST(stdio_stdin_exists);
	RUN_TEST(stdio_stdout_exists);
	RUN_TEST(stdio_stderr_exists);
	RUN_TEST(fileno_stdin);
	RUN_TEST(fileno_stdout);
	RUN_TEST(fileno_stderr);
	RUN_TEST(fileno_null);
	RUN_TEST(stdio_constants);
}
