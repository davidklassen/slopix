#include "uart.h"
#include "kprintf.h"
#include "gic.h"
#include "timer.h"
#include "prompt.h"
#include "arch.h"
#include "mmu.h"
#include "pmem.h"
#include "proc.h"
#include "initramfs.h"
#include "elf.h"
#include "tests/test.h"

DECLARE_SUITE(uart);
DECLARE_SUITE(kprintf);
DECLARE_SUITE(exception);
DECLARE_SUITE(timer);
DECLARE_SUITE(mmu);
DECLARE_SUITE(pmem);
DECLARE_SUITE(proc);
DECLARE_SUITE(uvm);
DECLARE_SUITE(elf);
DECLARE_SUITE(initramfs);

static void start_user_init(void) {
	struct initramfs_entry entry;
	if (initramfs_find("init", &entry) < 0) {
		kpanic("init not found in initramfs");
	}

	pte_t *pt = uvm_create();
	if (!pt) {
		kpanic("failed to create user page table");
	}

	unsigned long entry_addr;
	if (elf_load(entry.data, entry.size, pt, &entry_addr) < 0) {
		kpanic("failed to load init ELF");
	}

	paddr_t stack_pa = pmem_alloc();
	if (stack_pa == 0) {
		kpanic("failed to allocate stack page");
	}

	unsigned long stack_va = USER_STACK - PAGE_SIZE;
	if (uvm_map_page(pt, stack_va, stack_pa, 1, 0) < 0) {
		kpanic("failed to map stack page");
	}

	int pid = proc_create_user(pt, entry_addr, USER_STACK);
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
	RUN_SUITE(initramfs);

	gic_init();
	timer_init();
	enable_irq();
	RUN_SUITE(timer);

	TEST_REPORT();
	TEST_EXIT();

	uart_puts("Welcome to Slopix!\n");
	uart_puts("To exit QEMU press Ctrl-a x\n\n");

	start_user_init();

	proc_create(prompt);
	scheduler();
}
