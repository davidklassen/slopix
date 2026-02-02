.global write, _exit, read, sleep, getpid, fork, _wait_syscall, exec, poll, poweroff, sbrk
.global open, close, fstat, dup, _mkdir_syscall, mknod, link, unlink, chdir, pipe
.global stat, getcwd, lseek, rename, mmap, munmap
.global kill, getprocs, getppid, _waitpid_syscall
.global setpgid, getpgid, tcsetpgrp, tcgetpgrp, tcsetraw, tcgetraw, ftruncate, getdents, reboot

write:
	mov x8, #0
	svc #0
	b __syscall_ret

_exit:
	mov x8, #1
	svc #0
	ret

read:
	mov x8, #2
	svc #0
	b __syscall_ret

sleep:
	mov x8, #3
	svc #0
	b __syscall_ret

getpid:
	mov x8, #4
	svc #0
	ret

fork:
	mov x8, #5
	svc #0
	b __syscall_ret

_wait_syscall:
	mov x8, #6
	svc #0
	b __syscall_ret

exec:
	mov x8, #7
	svc #0
	b __syscall_ret

poll:
	mov x8, #8
	svc #0
	b __syscall_ret

poweroff:
	mov x8, #9
	svc #0
	ret

sbrk:
	mov x8, #10
	svc #0
	b __syscall_ret

open:
	mov x8, #11
	svc #0
	b __syscall_ret

close:
	mov x8, #12
	svc #0
	b __syscall_ret

fstat:
	mov x8, #13
	svc #0
	b __syscall_ret

dup:
	mov x8, #14
	svc #0
	b __syscall_ret

_mkdir_syscall:
	mov x8, #15
	svc #0
	b __syscall_ret

mknod:
	mov x8, #16
	svc #0
	b __syscall_ret

link:
	mov x8, #17
	svc #0
	b __syscall_ret

unlink:
	mov x8, #18
	svc #0
	b __syscall_ret

chdir:
	mov x8, #19
	svc #0
	b __syscall_ret

pipe:
	mov x8, #20
	svc #0
	b __syscall_ret

stat:
	mov x8, #21
	svc #0
	b __syscall_ret

getcwd:
	mov x8, #22
	svc #0
	b __syscall_ret

lseek:
	mov x8, #23
	svc #0
	b __syscall_ret

rename:
	mov x8, #24
	svc #0
	b __syscall_ret

mmap:
	mov x8, #25
	svc #0
	b __syscall_ret

munmap:
	mov x8, #26
	svc #0
	b __syscall_ret

kill:
	mov x8, #27
	svc #0
	b __syscall_ret

getprocs:
	mov x8, #28
	svc #0
	b __syscall_ret

getppid:
	mov x8, #29
	svc #0
	ret

_waitpid_syscall:
	mov x8, #30
	svc #0
	b __syscall_ret

setpgid:
	mov x8, #31
	svc #0
	b __syscall_ret

getpgid:
	mov x8, #32
	svc #0
	b __syscall_ret

tcsetpgrp:
	mov x8, #33
	svc #0
	b __syscall_ret

tcgetpgrp:
	mov x8, #34
	svc #0
	ret

tcsetraw:
	mov x8, #35
	svc #0
	b __syscall_ret

tcgetraw:
	mov x8, #36
	svc #0
	ret

ftruncate:
	mov x8, #37
	svc #0
	b __syscall_ret

getdents:
	mov x8, #38
	svc #0
	b __syscall_ret

reboot:
	mov x8, #39
	svc #0
	ret

// Common syscall return handler
// Check for error (negative return), set errno if needed
__syscall_ret:
	cmp x0, #0
	b.ge __syscall_ok
	neg x0, x0
	adrp x1, errno
	str w0, [x1, :lo12:errno]
	mov x0, #-1
__syscall_ok:
	ret
