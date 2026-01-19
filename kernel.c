#include "uart.h"
#include "kprintf.h"
#include "gic.h"
#include "timer.h"
#include "arch.h"
#include "pmem.h"
#include "proc.h"
#include "init.h"
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

	init("shell");

	scheduler();
}
