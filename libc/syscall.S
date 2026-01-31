.global write, _exit, read, sleep, getpid, fork, _wait_syscall, exec, poll, poweroff, sbrk
.global open, close, fstat, dup, _mkdir_syscall, mknod, link, unlink, chdir, pipe
.global stat, getcwd, lseek, rename, mmap, munmap
.global kill, getprocs, getppid, _waitpid_syscall
.global setpgid, getpgid, tcsetpgrp, tcgetpgrp, tcsetraw, tcgetraw, ftruncate, getdents

write:
	mov x8, #0
	svc #0
	ret

_exit:
	mov x8, #1
	svc #0
	ret

read:
	mov x8, #2
	svc #0
	ret

sleep:
	mov x8, #3
	svc #0
	ret

getpid:
	mov x8, #4
	svc #0
	ret

fork:
	mov x8, #5
	svc #0
	ret

_wait_syscall:
	mov x8, #6
	svc #0
	ret

exec:
	mov x8, #7
	svc #0
	ret

poll:
	mov x8, #8
	svc #0
	ret

poweroff:
	mov x8, #9
	svc #0
	ret

sbrk:
	mov x8, #10
	svc #0
	ret

open:
	mov x8, #11
	svc #0
	ret

close:
	mov x8, #12
	svc #0
	ret

fstat:
	mov x8, #13
	svc #0
	ret

dup:
	mov x8, #14
	svc #0
	ret

_mkdir_syscall:
	mov x8, #15
	svc #0
	ret

mknod:
	mov x8, #16
	svc #0
	ret

link:
	mov x8, #17
	svc #0
	ret

unlink:
	mov x8, #18
	svc #0
	ret

chdir:
	mov x8, #19
	svc #0
	ret

pipe:
	mov x8, #20
	svc #0
	ret

stat:
	mov x8, #21
	svc #0
	ret

getcwd:
	mov x8, #22
	svc #0
	ret

lseek:
	mov x8, #23
	svc #0
	ret

rename:
	mov x8, #24
	svc #0
	ret

mmap:
	mov x8, #25
	svc #0
	ret

munmap:
	mov x8, #26
	svc #0
	ret

kill:
	mov x8, #27
	svc #0
	ret

getprocs:
	mov x8, #28
	svc #0
	ret

getppid:
	mov x8, #29
	svc #0
	ret

_waitpid_syscall:
	mov x8, #30
	svc #0
	ret

setpgid:
	mov x8, #31
	svc #0
	ret

getpgid:
	mov x8, #32
	svc #0
	ret

tcsetpgrp:
	mov x8, #33
	svc #0
	ret

tcgetpgrp:
	mov x8, #34
	svc #0
	ret

tcsetraw:
	mov x8, #35
	svc #0
	ret

tcgetraw:
	mov x8, #36
	svc #0
	ret

ftruncate:
	mov x8, #37
	svc #0
	ret

getdents:
	mov x8, #38
	svc #0
	ret
