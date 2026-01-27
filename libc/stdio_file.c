#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>

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

FILE *open_memstream(char **ptr, size_t *sizeloc) {
	if (!ptr || !sizeloc) {
		return NULL;
	}
	FILE *f = file_alloc();
	if (!f) {
		return NULL;
	}
	size_t initial_size = 64;
	f->wbuf = malloc(initial_size);
	if (!f->wbuf) {
		free(f);
		return NULL;
	}
	f->flags = _FILE_WRITE;
	f->wbuf_size = initial_size;
	f->wbuf_pos = 0;
	f->wbuf[0] = '\0';
	f->is_memstream = 1;
	f->memstream_ptr = ptr;
	f->memstream_size = sizeloc;
	*ptr = f->wbuf;
	*sizeloc = 0;
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

static void reverse(char *buf, int len) {
	int i = 0, j = len - 1;
	while (i < j) {
		char tmp = buf[i];
		buf[i] = buf[j];
		buf[j] = tmp;
		i++;
		j--;
	}
}

static int format_int(char *buf, long val, int is_signed) {
	int neg = 0;
	unsigned long uval;
	int len = 0;

	if (is_signed && val < 0) {
		neg = 1;
		uval = -(unsigned long)val;
	} else {
		uval = val;
	}

	if (uval == 0) {
		buf[len++] = '0';
		return len;
	}

	while (uval > 0) {
		buf[len++] = '0' + (uval % 10);
		uval /= 10;
	}

	if (neg) {
		buf[len++] = '-';
	}

	reverse(buf, len);
	return len;
}

static int format_hex(char *buf, unsigned long val, int width, int upper) {
	const char *hex = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	int len = 0;
	int digits = 0;

	if (val == 0 && width == 0) {
		buf[len++] = '0';
		return len;
	}

	unsigned long tmp = val;
	while (tmp > 0 || digits < width) {
		buf[len++] = hex[tmp & 0xf];
		tmp >>= 4;
		digits++;
		if (digits >= width && tmp == 0) {
			break;
		}
	}

	reverse(buf, len);
	return len;
}

int vfprintf(FILE *stream, const char *fmt, va_list ap) {
	if (!stream || !fmt) {
		return -1;
	}

	int count = 0;
	char numbuf[32];

	while (*fmt) {
		if (*fmt != '%') {
			if (fputc(*fmt, stream) == EOF) {
				return -1;
			}
			count++;
			fmt++;
			continue;
		}
		fmt++;

		int zero_pad = 0;
		if (*fmt == '0') {
			zero_pad = 1;
			fmt++;
		}

		int width = 0;
		while (*fmt >= '0' && *fmt <= '9') {
			width = width * 10 + (*fmt - '0');
			fmt++;
		}

		int is_long = 0;
		if (*fmt == 'l') {
			is_long = 1;
			fmt++;
		}
		if (*fmt == 'l') {
			is_long = 2;
			fmt++;
		}

		switch (*fmt) {
		case 'd':
		case 'i': {
			long val;
			if (is_long) {
				val = va_arg(ap, long);
			} else {
				val = va_arg(ap, int);
			}
			int len = format_int(numbuf, val, 1);
			int pad = width - len;
			if (pad > 0 && !zero_pad) {
				while (pad-- > 0) {
					if (fputc(' ', stream) == EOF) {
						return -1;
					}
					count++;
				}
			}
			if (pad > 0 && zero_pad) {
				int neg = (val < 0);
				if (neg) {
					if (fputc('-', stream) == EOF) {
						return -1;
					}
					count++;
					len--;
					pad = width - len - 1;
				}
				while (pad-- > 0) {
					if (fputc('0', stream) == EOF) {
						return -1;
					}
					count++;
				}
				for (int i = neg ? 1 : 0; i < len + (neg ? 1 : 0); i++) {
					if (fputc(numbuf[i], stream) == EOF) {
						return -1;
					}
					count++;
				}
			} else {
				for (int i = 0; i < len; i++) {
					if (fputc(numbuf[i], stream) == EOF) {
						return -1;
					}
					count++;
				}
			}
			break;
		}
		case 'u': {
			unsigned long val;
			if (is_long) {
				val = va_arg(ap, unsigned long);
			} else {
				val = va_arg(ap, unsigned int);
			}
			int len = format_int(numbuf, (long)val, 0);
			int pad = width - len;
			char pad_char = zero_pad ? '0' : ' ';
			while (pad-- > 0) {
				if (fputc(pad_char, stream) == EOF) {
					return -1;
				}
				count++;
			}
			for (int i = 0; i < len; i++) {
				if (fputc(numbuf[i], stream) == EOF) {
					return -1;
				}
				count++;
			}
			break;
		}
		case 'x':
		case 'X': {
			unsigned long val;
			if (is_long) {
				val = va_arg(ap, unsigned long);
			} else {
				val = va_arg(ap, unsigned int);
			}
			int upper = (*fmt == 'X');
			int len = format_hex(numbuf, val, 0, upper);
			int pad = width - len;
			char pad_char = zero_pad ? '0' : ' ';
			while (pad-- > 0) {
				if (fputc(pad_char, stream) == EOF) {
					return -1;
				}
				count++;
			}
			for (int i = 0; i < len; i++) {
				if (fputc(numbuf[i], stream) == EOF) {
					return -1;
				}
				count++;
			}
			break;
		}
		case 'p': {
			unsigned long ptr = (unsigned long)va_arg(ap, void *);
			if (fputc('0', stream) == EOF) {
				return -1;
			}
			count++;
			if (fputc('x', stream) == EOF) {
				return -1;
			}
			count++;
			int len = format_hex(numbuf, ptr, 16, 0);
			for (int i = 0; i < len; i++) {
				if (fputc(numbuf[i], stream) == EOF) {
					return -1;
				}
				count++;
			}
			break;
		}
		case 's': {
			const char *s = va_arg(ap, const char *);
			if (!s) {
				s = "(null)";
			}
			int slen = 0;
			const char *p = s;
			while (*p++) {
				slen++;
			}
			int pad = width - slen;
			while (pad-- > 0) {
				if (fputc(' ', stream) == EOF) {
					return -1;
				}
				count++;
			}
			while (*s) {
				if (fputc(*s++, stream) == EOF) {
					return -1;
				}
				count++;
			}
			break;
		}
		case 'c': {
			int c = va_arg(ap, int);
			if (fputc(c, stream) == EOF) {
				return -1;
			}
			count++;
			break;
		}
		case '%':
			if (fputc('%', stream) == EOF) {
				return -1;
			}
			count++;
			break;
		default:
			if (fputc('%', stream) == EOF) {
				return -1;
			}
			count++;
			if (*fmt) {
				if (fputc(*fmt, stream) == EOF) {
					return -1;
				}
				count++;
			}
			break;
		}
		if (*fmt) {
			fmt++;
		}
	}

	return count;
}

int fprintf(FILE *stream, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int result = vfprintf(stream, fmt, ap);
	va_end(ap);
	return result;
}

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
	if (!fmt) {
		return -1;
	}
	if (!str || size == 0) {
		str = 0;
		size = 0;
	}

	int count = 0;
	char numbuf[32];

	while (*fmt) {
		if (*fmt != '%') {
			if (str && (size_t)count < size - 1) {
				str[count] = *fmt;
			}
			count++;
			fmt++;
			continue;
		}
		fmt++;

		int zero_pad = 0;
		if (*fmt == '0') {
			zero_pad = 1;
			fmt++;
		}

		int width = 0;
		while (*fmt >= '0' && *fmt <= '9') {
			width = width * 10 + (*fmt - '0');
			fmt++;
		}

		int is_long = 0;
		if (*fmt == 'l') {
			is_long = 1;
			fmt++;
		}
		if (*fmt == 'l') {
			is_long = 2;
			fmt++;
		}

		switch (*fmt) {
		case 'd':
		case 'i': {
			long val;
			if (is_long) {
				val = va_arg(ap, long);
			} else {
				val = va_arg(ap, int);
			}
			int len = format_int(numbuf, val, 1);
			int pad = width - len;
			if (pad > 0 && !zero_pad) {
				while (pad-- > 0) {
					if (str && (size_t)count < size - 1) {
						str[count] = ' ';
					}
					count++;
				}
			}
			if (pad > 0 && zero_pad) {
				int neg = (val < 0);
				if (neg) {
					if (str && (size_t)count < size - 1) {
						str[count] = '-';
					}
					count++;
					len--;
					pad = width - len - 1;
				}
				while (pad-- > 0) {
					if (str && (size_t)count < size - 1) {
						str[count] = '0';
					}
					count++;
				}
				for (int i = neg ? 1 : 0; i < len + (neg ? 1 : 0); i++) {
					if (str && (size_t)count < size - 1) {
						str[count] = numbuf[i];
					}
					count++;
				}
			} else {
				for (int i = 0; i < len; i++) {
					if (str && (size_t)count < size - 1) {
						str[count] = numbuf[i];
					}
					count++;
				}
			}
			break;
		}
		case 'u': {
			unsigned long val;
			if (is_long) {
				val = va_arg(ap, unsigned long);
			} else {
				val = va_arg(ap, unsigned int);
			}
			int len = format_int(numbuf, (long)val, 0);
			int pad = width - len;
			char pad_char = zero_pad ? '0' : ' ';
			while (pad-- > 0) {
				if (str && (size_t)count < size - 1) {
					str[count] = pad_char;
				}
				count++;
			}
			for (int i = 0; i < len; i++) {
				if (str && (size_t)count < size - 1) {
					str[count] = numbuf[i];
				}
				count++;
			}
			break;
		}
		case 'x':
		case 'X': {
			unsigned long val;
			if (is_long) {
				val = va_arg(ap, unsigned long);
			} else {
				val = va_arg(ap, unsigned int);
			}
			int upper = (*fmt == 'X');
			int len = format_hex(numbuf, val, 0, upper);
			int pad = width - len;
			char pad_char = zero_pad ? '0' : ' ';
			while (pad-- > 0) {
				if (str && (size_t)count < size - 1) {
					str[count] = pad_char;
				}
				count++;
			}
			for (int i = 0; i < len; i++) {
				if (str && (size_t)count < size - 1) {
					str[count] = numbuf[i];
				}
				count++;
			}
			break;
		}
		case 'p': {
			unsigned long ptr = (unsigned long)va_arg(ap, void *);
			if (str && (size_t)count < size - 1) {
				str[count] = '0';
			}
			count++;
			if (str && (size_t)count < size - 1) {
				str[count] = 'x';
			}
			count++;
			int len = format_hex(numbuf, ptr, 16, 0);
			for (int i = 0; i < len; i++) {
				if (str && (size_t)count < size - 1) {
					str[count] = numbuf[i];
				}
				count++;
			}
			break;
		}
		case 's': {
			const char *s = va_arg(ap, const char *);
			if (!s) {
				s = "(null)";
			}
			int slen = 0;
			const char *p = s;
			while (*p++) {
				slen++;
			}
			int pad = width - slen;
			while (pad-- > 0) {
				if (str && (size_t)count < size - 1) {
					str[count] = ' ';
				}
				count++;
			}
			while (*s) {
				if (str && (size_t)count < size - 1) {
					str[count] = *s;
				}
				s++;
				count++;
			}
			break;
		}
		case 'c': {
			int c = va_arg(ap, int);
			if (str && (size_t)count < size - 1) {
				str[count] = c;
			}
			count++;
			break;
		}
		case '%':
			if (str && (size_t)count < size - 1) {
				str[count] = '%';
			}
			count++;
			break;
		default:
			if (str && (size_t)count < size - 1) {
				str[count] = '%';
			}
			count++;
			if (*fmt) {
				if (str && (size_t)count < size - 1) {
					str[count] = *fmt;
				}
				count++;
			}
			break;
		}
		if (*fmt) {
			fmt++;
		}
	}

	if (str && size > 0) {
		size_t term_pos = (size_t)count < size - 1 ? (size_t)count : size - 1;
		str[term_pos] = '\0';
	}

	return count;
}

int snprintf(char *str, size_t size, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int result = vsnprintf(str, size, fmt, ap);
	va_end(ap);
	return result;
}

int vsprintf(char *str, const char *fmt, va_list ap) {
	return vsnprintf(str, (size_t)-1, fmt, ap);
}

int sprintf(char *str, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int result = vsprintf(str, fmt, ap);
	va_end(ap);
	return result;
}
