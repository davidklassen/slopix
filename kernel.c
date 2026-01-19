#include "uart.h"
#include "kprintf.h"
#include "gic.h"
#include "timer.h"
#include "prompt.h"
#include "arch.h"
#include "mmu.h"
#include "pmem.h"
#include "proc.h"
#include "tests/test.h"

#define USER_BASE  0x0000000000000000UL
#define USER_STACK 0x0000000080000000UL

DECLARE_SUITE(uart);
DECLARE_SUITE(kprintf);
DECLARE_SUITE(exception);
DECLARE_SUITE(timer);
DECLARE_SUITE(mmu);
DECLARE_SUITE(pmem);
DECLARE_SUITE(proc);
DECLARE_SUITE(uvm);
DECLARE_SUITE(elf);

static void start_user_init(void) {
	pte_t *pt = uvm_create();
	if (!pt) {
		kpanic("failed to create user page table");
	}

	paddr_t code_pa = pmem_alloc();
	if (code_pa == 0) {
		kpanic("failed to allocate code page");
	}

	unsigned int *code = (unsigned int *)PA_TO_VA(code_pa);
	// write(1, "Hi\n", 3); exit(0);
	code[0] = 0xd2800020; // mov x0, #1        (fd)
	code[1] = 0x100000e1; // adr x1, pc+28     (buf -> code[8])
	code[2] = 0xd2800062; // mov x2, #3        (len)
	code[3] = 0xd2800008; // mov x8, #0        (SYS_write)
	code[4] = 0xd4000001; // svc #0
	code[5] = 0xd2800000; // mov x0, #0        (status)
	code[6] = 0xd2800028; // mov x8, #1        (SYS_exit)
	code[7] = 0xd4000001; // svc #0
	code[8] = 0x000a6948; // "Hi\n\0"

	if (uvm_map_page(pt, USER_BASE, code_pa, 0, 1) < 0) {
		kpanic("failed to map code page");
	}

	paddr_t stack_pa = pmem_alloc();
	if (stack_pa == 0) {
		kpanic("failed to allocate stack page");
	}

	unsigned long stack_va = USER_STACK - PAGE_SIZE;
	if (uvm_map_page(pt, stack_va, stack_pa, 1, 0) < 0) {
		kpanic("failed to map stack page");
	}

	int pid = proc_create_user(pt, USER_BASE, USER_STACK);
	if (pid < 0) {
		kpanic("failed to create user process");
	}

	kprintf("Created user process %d\n", pid);
}

void kernel_main(void) {
	uart_init();
	RUN_SUITE(uart);
	RUN_SUITE(kprintf);
	RUN_SUITE(exception);
	RUN_SUITE(mmu);

	pmem_init();
	RUN_SUITE(pmem);
	RUN_SUITE(proc);
	RUN_SUITE(uvm);
	RUN_SUITE(elf);

	gic_init();
	timer_init();
	enable_irq();
	RUN_SUITE(timer);

	TEST_REPORT();
	TEST_EXIT();

	uart_puts("Welcome to Slopix!\n");
	uart_puts("To exit QEMU press Ctrl-a x\n\n");

	start_user_init();

	proc_create(cursor_blink);
	proc_create(prompt);
	scheduler();
}
