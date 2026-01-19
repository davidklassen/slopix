.global _start
_start:
	mov x29, #0
	mov x30, #0
	bl main
	mov x8, #1
	svc #0
