#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

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

static FILE *file_alloc(void) {
	FILE *f = calloc(1, sizeof(FILE));
	if (!f) {
		return NULL;
	}
	f->fd = -1;
	return f;
}

int fflush(FILE *stream) {
	if (!stream || !(stream->flags & _FILE_WRITE)) {
		return 0;
	}
	if (stream->is_memstream) {
		if (stream->memstream_ptr) {
			*stream->memstream_ptr = stream->wbuf;
		}
		if (stream->memstream_size) {
			*stream->memstream_size = stream->wbuf_pos;
		}
		return 0;
	}
	if (stream->wbuf_pos > 0) {
		long written = write(stream->fd, stream->wbuf, stream->wbuf_pos);
		if (written < 0) {
			stream->error = 1;
			return EOF;
		}
		stream->wbuf_pos = 0;
	}
	return 0;
}

int fputc(int c, FILE *stream) {
	if (!stream || !(stream->flags & _FILE_WRITE)) {
		return EOF;
	}
	if (stream->flags & _FILE_UNBUF) {
		unsigned char ch = (unsigned char)c;
		long written = write(stream->fd, &ch, 1);
		if (written != 1) {
			stream->error = 1;
			return EOF;
		}
		return (unsigned char)c;
	}
	if (!stream->wbuf) {
		stream->wbuf = malloc(BUFSIZ);
		if (!stream->wbuf) {
			stream->error = 1;
			return EOF;
		}
		stream->wbuf_size = BUFSIZ;
		stream->wbuf_pos = 0;
	}
	if (stream->is_memstream) {
		if (stream->wbuf_pos >= stream->wbuf_size - 1) {
			int newsize = stream->wbuf_size * 2;
			char *newbuf = realloc(stream->wbuf, newsize);
			if (!newbuf) {
				stream->error = 1;
				return EOF;
			}
			stream->wbuf = newbuf;
			stream->wbuf_size = newsize;
		}
		stream->wbuf[stream->wbuf_pos++] = (char)c;
		stream->wbuf[stream->wbuf_pos] = '\0';
		return (unsigned char)c;
	}
	stream->wbuf[stream->wbuf_pos++] = (char)c;
	int should_flush = stream->wbuf_pos >= stream->wbuf_size;
	if (stream->fd == 1 && c == '\n') {
		should_flush = 1;
	}
	if (should_flush) {
		if (fflush(stream) == EOF) {
			return EOF;
		}
	}
	return (unsigned char)c;
}

int fgetc(FILE *stream) {
	if (!stream || !(stream->flags & _FILE_READ)) {
		return EOF;
	}
	if (!stream->rbuf) {
		stream->rbuf = malloc(BUFSIZ);
		if (!stream->rbuf) {
			stream->error = 1;
			return EOF;
		}
		stream->rbuf_size = BUFSIZ;
		stream->rbuf_pos = 0;
		stream->rbuf_len = 0;
	}
	if (stream->rbuf_pos >= stream->rbuf_len) {
		long nread = read(stream->fd, stream->rbuf, stream->rbuf_size);
		if (nread < 0) {
			stream->error = 1;
			return EOF;
		}
		if (nread == 0) {
			stream->eof = 1;
			return EOF;
		}
		stream->rbuf_pos = 0;
		stream->rbuf_len = nread;
	}
	return (unsigned char)stream->rbuf[stream->rbuf_pos++];
}

size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream) {
	if (!stream || !ptr || size == 0 || count == 0) {
		return 0;
	}
	const unsigned char *p = ptr;
	size_t total = size * count;
	size_t written = 0;
	for (size_t i = 0; i < total; i++) {
		if (fputc(p[i], stream) == EOF) {
			break;
		}
		written++;
	}
	return written / size;
}

size_t fread(void *ptr, size_t size, size_t count, FILE *stream) {
	if (!stream || !ptr || size == 0 || count == 0) {
		return 0;
	}
	unsigned char *p = ptr;
	size_t total = size * count;
	size_t nread = 0;
	while (nread < total) {
		int c = fgetc(stream);
		if (c == EOF) {
			break;
		}
		p[nread++] = (unsigned char)c;
	}
	return nread / size;
}

FILE *fopen(const char *path, const char *mode) {
	if (!path || !mode) {
		return NULL;
	}
	int file_flags = 0;
	int fd_flags = 0;
	if (mode[0] == 'r') {
		if (mode[1] == '+') {
			file_flags = _FILE_READ | _FILE_WRITE;
			fd_flags = O_RDWR;
		} else {
			file_flags = _FILE_READ;
			fd_flags = O_RDONLY;
		}
	} else if (mode[0] == 'w') {
		if (mode[1] == '+') {
			file_flags = _FILE_READ | _FILE_WRITE;
			fd_flags = O_RDWR | O_CREAT | O_TRUNC;
		} else {
			file_flags = _FILE_WRITE;
			fd_flags = O_WRONLY | O_CREAT | O_TRUNC;
		}
	} else if (mode[0] == 'a') {
		if (mode[1] == '+') {
			file_flags = _FILE_READ | _FILE_WRITE | _FILE_APPEND;
			fd_flags = O_RDWR | O_CREAT | O_APPEND;
		} else {
			file_flags = _FILE_WRITE | _FILE_APPEND;
			fd_flags = O_WRONLY | O_CREAT | O_APPEND;
		}
	} else {
		return NULL;
	}
	int fd = open(path, fd_flags);
	if (fd < 0) {
		return NULL;
	}
	FILE *f = file_alloc();
	if (!f) {
		close(fd);
		return NULL;
	}
	f->fd = fd;
	f->flags = file_flags;
	if (file_flags & _FILE_READ) {
		f->rbuf = malloc(BUFSIZ);
		if (!f->rbuf) {
			close(fd);
			free(f);
			return NULL;
		}
		f->rbuf_size = BUFSIZ;
	}
	if (file_flags & _FILE_WRITE) {
		f->wbuf = malloc(BUFSIZ);
		if (!f->wbuf) {
			close(fd);
			free(f->rbuf);
			free(f);
			return NULL;
		}
		f->wbuf_size = BUFSIZ;
	}
	return f;
}

int fclose(FILE *stream) {
	if (!stream) {
		return EOF;
	}
	int result = fflush(stream);
	if (stream->fd >= 0) {
		if (close(stream->fd) < 0) {
			result = EOF;
		}
	}
	if (!stream->is_memstream) {
		free(stream->rbuf);
		free(stream->wbuf);
	}
	if (stream != stdin && stream != stdout && stream != stderr) {
		free(stream);
	}
	return result;
}
