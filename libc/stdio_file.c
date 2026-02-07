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
	unsigned char uc = (unsigned char)c;
	if (stream->flags & _FILE_UNBUF) {
		long written = write(stream->fd, &uc, 1);
		if (written != 1) {
			stream->error = 1;
			return EOF;
		}
		return uc;
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
		stream->wbuf[stream->wbuf_pos++] = uc;
		stream->wbuf[stream->wbuf_pos] = '\0';
		return uc;
	}
	stream->wbuf[stream->wbuf_pos++] = uc;
	int should_flush = stream->wbuf_pos >= stream->wbuf_size;
	if (stream->fd == 1 && c == '\n') {
		should_flush = 1;
	}
	if (should_flush) {
		if (fflush(stream) == EOF) {
			return EOF;
		}
	}
	return uc;
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

char *fgets(char *s, int size, FILE *stream) {
	if (!s || size <= 0 || !stream) {
		return NULL;
	}
	int i = 0;
	int c;
	while (i < size - 1 && (c = fgetc(stream)) != EOF) {
		s[i++] = (char)c;
		if (c == '\n') {
			break;
		}
	}
	if (i == 0) {
		return NULL;
	}
	s[i] = '\0';
	return s;
}

int fputs(const char *s, FILE *stream) {
	if (!s || !stream) {
		return EOF;
	}
	while (*s) {
		if (fputc(*s++, stream) == EOF) {
			return EOF;
		}
	}
	return 0;
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

int fseek(FILE *stream, long offset, int whence) {
	if (!stream || stream->fd < 0) {
		return -1;
	}
	fflush(stream);
	if (whence == 1) {
		long fd_pos = lseek(stream->fd, 0, 1);
		if (fd_pos < 0) {
			return -1;
		}
		long logical_pos = fd_pos - (stream->rbuf_len - stream->rbuf_pos);
		offset = logical_pos + offset;
		whence = 0;
	}
	stream->rbuf_pos = 0;
	stream->rbuf_len = 0;
	stream->eof = 0;
	long pos = lseek(stream->fd, offset, whence);
	if (pos < 0) {
		return -1;
	}
	return 0;
}

long ftell(FILE *stream) {
	if (!stream || stream->fd < 0) {
		return -1;
	}
	long pos = lseek(stream->fd, 0, 1);
	if (pos < 0) {
		return -1;
	}
	pos -= stream->rbuf_len - stream->rbuf_pos;
	pos += stream->wbuf_pos;
	return pos;
}

int feof(FILE *stream) {
	if (!stream) {
		return 0;
	}
	return stream->eof;
}

typedef struct {
	int (*putc)(int c, void *ctx);
	void *ctx;
} PrintWriter;

typedef struct {
	int left_align;
	int zero_pad;
	int show_sign;
	int space_sign;
	int alt_form;
	int width;
	int precision;
	int length;
} FmtSpec;

static int emit_char(PrintWriter *w, int c, int *count) {
	int r = w->putc(c, w->ctx);
	if (r == EOF) {
		return -1;
	}
	(*count)++;
	return 0;
}

static int emit_chars(PrintWriter *w, int c, int n, int *count) {
	for (int i = 0; i < n; i++) {
		if (emit_char(w, c, count) < 0) {
			return -1;
		}
	}
	return 0;
}

static int emit_str(PrintWriter *w, const char *s, int len, int *count) {
	for (int i = 0; i < len; i++) {
		if (emit_char(w, s[i], count) < 0) {
			return -1;
		}
	}
	return 0;
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

static int format_uint(char *buf, unsigned long val) {
	int len = 0;
	if (val == 0) {
		buf[len++] = '0';
		return len;
	}
	while (val > 0) {
		buf[len++] = '0' + (val % 10);
		val /= 10;
	}
	reverse(buf, len);
	return len;
}

static int format_hex(char *buf, unsigned long val, int upper) {
	const char *hex = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	int len = 0;
	if (val == 0) {
		buf[len++] = '0';
		return len;
	}
	while (val > 0) {
		buf[len++] = hex[val & 0xf];
		val >>= 4;
	}
	reverse(buf, len);
	return len;
}

static int format_octal(char *buf, unsigned long val) {
	int len = 0;
	if (val == 0) {
		buf[len++] = '0';
		return len;
	}
	while (val > 0) {
		buf[len++] = '0' + (val & 7);
		val >>= 3;
	}
	reverse(buf, len);
	return len;
}

static int vprintf_core(PrintWriter *w, const char *fmt, va_list ap) {
	int count = 0;
	char numbuf[32];

	while (*fmt) {
		if (*fmt != '%') {
			if (emit_char(w, *fmt, &count) < 0) {
				return -1;
			}
			fmt++;
			continue;
		}
		fmt++;

		FmtSpec spec = {0, 0, 0, 0, 0, 0, -1, 0};

		while (*fmt == '-' || *fmt == '0' || *fmt == '+' || *fmt == ' ' ||
		       *fmt == '#') {
			switch (*fmt) {
			case '-':
				spec.left_align = 1;
				break;
			case '0':
				spec.zero_pad = 1;
				break;
			case '+':
				spec.show_sign = 1;
				break;
			case ' ':
				spec.space_sign = 1;
				break;
			case '#':
				spec.alt_form = 1;
				break;
			}
			fmt++;
		}

		if (spec.left_align) {
			spec.zero_pad = 0;
		}

		if (*fmt == '*') {
			spec.width = va_arg(ap, int);
			if (spec.width < 0) {
				spec.left_align = 1;
				spec.zero_pad = 0;
				spec.width = -spec.width;
			}
			fmt++;
		} else {
			while (*fmt >= '0' && *fmt <= '9') {
				spec.width = spec.width * 10 + (*fmt - '0');
				fmt++;
			}
		}

		if (*fmt == '.') {
			fmt++;
			if (*fmt == '*') {
				spec.precision = va_arg(ap, int);
				if (spec.precision < 0) {
					spec.precision = -1;
				}
				fmt++;
			} else {
				spec.precision = 0;
				while (*fmt >= '0' && *fmt <= '9') {
					spec.precision = spec.precision * 10 + (*fmt - '0');
					fmt++;
				}
			}
		}

		if (*fmt == 'h') {
			spec.length = -1;
			fmt++;
			if (*fmt == 'h') {
				spec.length = -2;
				fmt++;
			}
		} else if (*fmt == 'l') {
			spec.length = 1;
			fmt++;
			if (*fmt == 'l') {
				spec.length = 2;
				fmt++;
			}
		} else if (*fmt == 'z') {
			spec.length = 3;
			fmt++;
		}

		switch (*fmt) {
		case 'd':
		case 'i': {
			long val;
			if (spec.length == 2 || spec.length == 1) {
				val = va_arg(ap, long);
			} else if (spec.length == 3) {
				val = (long)va_arg(ap, size_t);
			} else {
				val = va_arg(ap, int);
				if (spec.length == -1) {
					val = (short)val;
				} else if (spec.length == -2) {
					val = (signed char)val;
				}
			}
			int neg = (val < 0);
			unsigned long uval = neg ? -(unsigned long)val : (unsigned long)val;
			int len = format_uint(numbuf, uval);
			char sign_char = 0;
			if (neg) {
				sign_char = '-';
			} else if (spec.show_sign) {
				sign_char = '+';
			} else if (spec.space_sign) {
				sign_char = ' ';
			}
			int sign_len = sign_char ? 1 : 0;
			int total_len = len + sign_len;
			int pad = spec.width - total_len;
			if (!spec.left_align && !spec.zero_pad && pad > 0) {
				if (emit_chars(w, ' ', pad, &count) < 0) {
					return -1;
				}
			}
			if (sign_char) {
				if (emit_char(w, sign_char, &count) < 0) {
					return -1;
				}
			}
			if (!spec.left_align && spec.zero_pad && pad > 0) {
				if (emit_chars(w, '0', pad, &count) < 0) {
					return -1;
				}
			}
			if (emit_str(w, numbuf, len, &count) < 0) {
				return -1;
			}
			if (spec.left_align && pad > 0) {
				if (emit_chars(w, ' ', pad, &count) < 0) {
					return -1;
				}
			}
			break;
		}
		case 'u': {
			unsigned long val;
			if (spec.length == 2 || spec.length == 1) {
				val = va_arg(ap, unsigned long);
			} else if (spec.length == 3) {
				val = va_arg(ap, size_t);
			} else {
				val = va_arg(ap, unsigned int);
				if (spec.length == -1) {
					val = (unsigned short)val;
				} else if (spec.length == -2) {
					val = (unsigned char)val;
				}
			}
			int len = format_uint(numbuf, val);
			int pad = spec.width - len;
			if (!spec.left_align && !spec.zero_pad && pad > 0) {
				if (emit_chars(w, ' ', pad, &count) < 0) {
					return -1;
				}
			}
			if (!spec.left_align && spec.zero_pad && pad > 0) {
				if (emit_chars(w, '0', pad, &count) < 0) {
					return -1;
				}
			}
			if (emit_str(w, numbuf, len, &count) < 0) {
				return -1;
			}
			if (spec.left_align && pad > 0) {
				if (emit_chars(w, ' ', pad, &count) < 0) {
					return -1;
				}
			}
			break;
		}
		case 'o': {
			unsigned long val;
			if (spec.length == 2 || spec.length == 1) {
				val = va_arg(ap, unsigned long);
			} else if (spec.length == 3) {
				val = va_arg(ap, size_t);
			} else {
				val = va_arg(ap, unsigned int);
				if (spec.length == -1) {
					val = (unsigned short)val;
				} else if (spec.length == -2) {
					val = (unsigned char)val;
				}
			}
			int len = format_octal(numbuf, val);
			int prefix_len = 0;
			if (spec.alt_form && val != 0) {
				prefix_len = 1;
			}
			int total_len = len + prefix_len;
			int pad = spec.width - total_len;
			if (!spec.left_align && !spec.zero_pad && pad > 0) {
				if (emit_chars(w, ' ', pad, &count) < 0) {
					return -1;
				}
			}
			if (prefix_len) {
				if (emit_char(w, '0', &count) < 0) {
					return -1;
				}
			}
			if (!spec.left_align && spec.zero_pad && pad > 0) {
				if (emit_chars(w, '0', pad, &count) < 0) {
					return -1;
				}
			}
			if (emit_str(w, numbuf, len, &count) < 0) {
				return -1;
			}
			if (spec.left_align && pad > 0) {
				if (emit_chars(w, ' ', pad, &count) < 0) {
					return -1;
				}
			}
			break;
		}
		case 'x':
		case 'X': {
			unsigned long val;
			if (spec.length == 2 || spec.length == 1) {
				val = va_arg(ap, unsigned long);
			} else if (spec.length == 3) {
				val = va_arg(ap, size_t);
			} else {
				val = va_arg(ap, unsigned int);
				if (spec.length == -1) {
					val = (unsigned short)val;
				} else if (spec.length == -2) {
					val = (unsigned char)val;
				}
			}
			int upper = (*fmt == 'X');
			int len = format_hex(numbuf, val, upper);
			int prefix_len = 0;
			if (spec.alt_form && val != 0) {
				prefix_len = 2;
			}
			int total_len = len + prefix_len;
			int pad = spec.width - total_len;
			if (!spec.left_align && !spec.zero_pad && pad > 0) {
				if (emit_chars(w, ' ', pad, &count) < 0) {
					return -1;
				}
			}
			if (prefix_len) {
				if (emit_char(w, '0', &count) < 0) {
					return -1;
				}
				if (emit_char(w, upper ? 'X' : 'x', &count) < 0) {
					return -1;
				}
			}
			if (!spec.left_align && spec.zero_pad && pad > 0) {
				if (emit_chars(w, '0', pad, &count) < 0) {
					return -1;
				}
			}
			if (emit_str(w, numbuf, len, &count) < 0) {
				return -1;
			}
			if (spec.left_align && pad > 0) {
				if (emit_chars(w, ' ', pad, &count) < 0) {
					return -1;
				}
			}
			break;
		}
		case 'p': {
			unsigned long ptr = (unsigned long)va_arg(ap, void *);
			char hexbuf[20];
			int hlen = 0;
			hexbuf[hlen++] = '0';
			hexbuf[hlen++] = 'x';
			int numlen = format_hex(numbuf, ptr, 0);
			int zeros = 16 - numlen;
			for (int i = 0; i < zeros; i++) {
				hexbuf[hlen++] = '0';
			}
			for (int i = 0; i < numlen; i++) {
				hexbuf[hlen++] = numbuf[i];
			}
			int pad = spec.width - hlen;
			if (!spec.left_align && pad > 0) {
				if (emit_chars(w, ' ', pad, &count) < 0) {
					return -1;
				}
			}
			if (emit_str(w, hexbuf, hlen, &count) < 0) {
				return -1;
			}
			if (spec.left_align && pad > 0) {
				if (emit_chars(w, ' ', pad, &count) < 0) {
					return -1;
				}
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
			if (spec.precision >= 0 && spec.precision < slen) {
				slen = spec.precision;
			}
			int pad = spec.width - slen;
			if (!spec.left_align && pad > 0) {
				if (emit_chars(w, ' ', pad, &count) < 0) {
					return -1;
				}
			}
			if (emit_str(w, s, slen, &count) < 0) {
				return -1;
			}
			if (spec.left_align && pad > 0) {
				if (emit_chars(w, ' ', pad, &count) < 0) {
					return -1;
				}
			}
			break;
		}
		case 'c': {
			int c = va_arg(ap, int);
			int pad = spec.width - 1;
			if (!spec.left_align && pad > 0) {
				if (emit_chars(w, ' ', pad, &count) < 0) {
					return -1;
				}
			}
			if (emit_char(w, c, &count) < 0) {
				return -1;
			}
			if (spec.left_align && pad > 0) {
				if (emit_chars(w, ' ', pad, &count) < 0) {
					return -1;
				}
			}
			break;
		}
		case '%':
			if (emit_char(w, '%', &count) < 0) {
				return -1;
			}
			break;
		default:
			if (emit_char(w, '%', &count) < 0) {
				return -1;
			}
			if (*fmt) {
				if (emit_char(w, *fmt, &count) < 0) {
					return -1;
				}
			}
			break;
		}
		if (*fmt) {
			fmt++;
		}
	}

	return count;
}

static int file_putc(int c, void *ctx) {
	return fputc(c, (FILE *)ctx);
}

int vfprintf(FILE *stream, const char *fmt, va_list ap) {
	if (!stream || !fmt) {
		return -1;
	}
	PrintWriter w = {file_putc, stream};
	return vprintf_core(&w, fmt, ap);
}

int fprintf(FILE *stream, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int result = vfprintf(stream, fmt, ap);
	va_end(ap);
	return result;
}

int printf(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int result = vfprintf(stdout, fmt, ap);
	va_end(ap);
	return result;
}

int vprintf(const char *fmt, va_list ap) {
	return vfprintf(stdout, fmt, ap);
}

typedef struct {
	char *buf;
	size_t size;
	size_t pos;
} StrWriterCtx;

static int str_putc(int c, void *ctx) {
	StrWriterCtx *s = ctx;
	unsigned char uc = (unsigned char)c;
	if (s->buf && s->pos < s->size - 1) {
		s->buf[s->pos] = uc;
	}
	s->pos++;
	return uc;
}

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
	if (!fmt) {
		return -1;
	}
	StrWriterCtx ctx = {str, size, 0};
	if (!str || size == 0) {
		ctx.buf = 0;
		ctx.size = 0;
	}
	PrintWriter w = {str_putc, &ctx};
	int count = vprintf_core(&w, fmt, ap);
	if (ctx.buf && ctx.size > 0) {
		size_t term_pos = ctx.pos < ctx.size ? ctx.pos : ctx.size - 1;
		ctx.buf[term_pos] = '\0';
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

ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
	if (!lineptr || !n || !stream) {
		return -1;
	}

	size_t pos = 0;
	size_t cap = *n;
	char *buf = *lineptr;

	if (!buf || cap == 0) {
		cap = 128;
		buf = malloc(cap);
		if (!buf) {
			return -1;
		}
	}

	int c;
	while ((c = fgetc(stream)) != EOF) {
		if (pos + 1 >= cap) {
			size_t newcap = cap * 2;
			char *newbuf = realloc(buf, newcap);
			if (!newbuf) {
				*lineptr = buf;
				*n = cap;
				return -1;
			}
			buf = newbuf;
			cap = newcap;
		}
		buf[pos++] = (char)c;
		if (c == '\n') {
			break;
		}
	}

	if (pos == 0 && c == EOF) {
		*lineptr = buf;
		*n = cap;
		return -1;
	}

	buf[pos] = '\0';
	*lineptr = buf;
	*n = cap;
	return (ssize_t)pos;
}
