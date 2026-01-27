#include <stdio.h>

struct _FILE {
	int fd;
	int flags;
	int error;
	int eof;
	char *rbuf;
	int rbuf_size;
	int rbuf_pos;
	int rbuf_len;
	char *wbuf;
	int wbuf_size;
	int wbuf_pos;
	char **memstream_ptr;
	unsigned long *memstream_size;
	int is_memstream;
};

static FILE _stdin = {.fd = 0, .flags = _FILE_READ};
static FILE _stdout = {.fd = 1, .flags = _FILE_WRITE};
static FILE _stderr = {.fd = 2, .flags = _FILE_WRITE | _FILE_UNBUF};

FILE *stdin = &_stdin;
FILE *stdout = &_stdout;
FILE *stderr = &_stderr;

int fileno(FILE *stream) {
	if (!stream) {
		return -1;
	}
	return stream->fd;
}
