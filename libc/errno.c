#include <errno.h>

int errno = 0;

static const char *error_messages[] = {
    [0] = "Success",
    [1] = "Operation not permitted",
    [2] = "No such file or directory",
    [3] = "No such process",
    [4] = "Interrupted system call",
    [5] = "I/O error",
    [9] = "Bad file descriptor",
    [12] = "Out of memory",
    [13] = "Permission denied",
    [14] = "Bad address",
    [17] = "File exists",
    [20] = "Not a directory",
    [21] = "Is a directory",
    [22] = "Invalid argument",
    [25] = "Inappropriate ioctl for device",
    [28] = "No space left on device",
};

#define ERRMSG_COUNT (sizeof(error_messages) / sizeof(error_messages[0]))

char *strerror(int errnum) {
	if (errnum >= 0 && (unsigned)errnum < ERRMSG_COUNT &&
	    error_messages[errnum]) {
		return (char *)error_messages[errnum];
	}
	return "Unknown error";
}
