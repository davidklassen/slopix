.global write, exit, read, sleep, getpid, fork, wait, exec, poll, poweroff, sbrk

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
