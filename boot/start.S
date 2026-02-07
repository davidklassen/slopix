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

.global dsb
dsb:
	dsb	sy
	ret

.global nop
nop:
	nop
	ret

.global jump_to_kernel
jump_to_kernel:
	// x0 = DTB address (stays in x0 for kernel)
	// x1 = runtests flag (stays in x1 for kernel)
	// x2 = kernel entry point
	br	x2
