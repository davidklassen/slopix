#ifndef _ERRNO_H
#define _ERRNO_H

extern int errno;

#define EPERM	1  /* Operation not permitted */
#define ENOENT	2  /* No such file or directory */
#define ESRCH	3  /* No such process */
#define EINTR	4  /* Interrupted system call */
#define EIO	5  /* I/O error */
#define EBADF	9  /* Bad file descriptor */
#define ENOMEM	12 /* Out of memory */
#define EACCES	13 /* Permission denied */
#define EFAULT	14 /* Bad address */
#define EEXIST	17 /* File exists */
#define ENOTDIR 20 /* Not a directory */
#define EISDIR	21 /* Is a directory */
#define EINVAL	22 /* Invalid argument */
#define ENOSPC	28 /* No space left on device */

char *strerror(int errnum);

#endif
