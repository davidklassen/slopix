.section .text
.global _start

_start:
	ldr	x0, =0x40080000
	mov	sp, x0
	bl	boot_main
	b	halt

.global halt
halt:
	wfe
	b	halt
