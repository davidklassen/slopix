.global write, exit, read, sleep, getpid, fork, wait, exec, poll, poweroff, sbrk
.global open, close, fstat, dup, mkdir, mknod, link, unlink, chdir, pipe
.global stat, getcwd, lseek, rename

write:
	mov x8, #0
	svc #0
	ret

exit:
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

wait:
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

mkdir:
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
