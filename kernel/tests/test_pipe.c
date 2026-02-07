#include "test.h"
#include "file.h"
#include "pipe.h"

TEST(pipe_alloc_basic) {
	struct file *rf, *wf;
	int r = pipealloc(&rf, &wf);
	ASSERT_EQ(r, 0, "pipealloc should succeed");
	ASSERT_NOT_NULL(rf, "read file should be allocated");
	ASSERT_NOT_NULL(wf, "write file should be allocated");
	ASSERT_EQ(rf->type, FD_PIPE, "read file type should be FD_PIPE");
	ASSERT_EQ(wf->type, FD_PIPE, "write file type should be FD_PIPE");
	ASSERT_EQ(rf->readable, 1, "read end should be readable");
	ASSERT_EQ(rf->writable, 0, "read end should not be writable");
	ASSERT_EQ(wf->readable, 0, "write end should not be readable");
	ASSERT_EQ(wf->writable, 1, "write end should be writable");
	fileclose(rf);
	fileclose(wf);
	return 0;
}

TEST(pipe_initial_state) {
	struct file *rf, *wf;
	pipealloc(&rf, &wf);
	struct pipe *pi = rf->pipe;
	ASSERT_EQ(pi->nread, 0, "nread should start at 0");
	ASSERT_EQ(pi->nwrite, 0, "nwrite should start at 0");
	ASSERT_EQ(pi->readopen, 1, "readopen should be 1");
	ASSERT_EQ(pi->writeopen, 1, "writeopen should be 1");
	fileclose(rf);
	fileclose(wf);
	return 0;
}

TEST(pipe_shared_struct) {
	struct file *rf, *wf;
	pipealloc(&rf, &wf);
	ASSERT_EQ(rf->pipe, wf->pipe, "both files should share pipe");
	ASSERT(rf != wf, "read and write files should be different");
	fileclose(rf);
	fileclose(wf);
	return 0;
}

TEST_SUITE(pipe) {
	RUN_TEST(pipe_alloc_basic);
	RUN_TEST(pipe_initial_state);
	RUN_TEST(pipe_shared_struct);
}
