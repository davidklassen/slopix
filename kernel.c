#include "uart.h"
#include "gic.h"
#include "timer.h"
#include "prompt.h"
#include "arch.h"
#include "mmu.h"
#include "pmem.h"
#include "proc.h"
#include "tests/test.h"

DECLARE_SUITE(uart);
DECLARE_SUITE(kprintf);
DECLARE_SUITE(exception);
DECLARE_SUITE(timer);
DECLARE_SUITE(mmu);
DECLARE_SUITE(pmem);
DECLARE_SUITE(proc);

static void cursor_blink(void) {
	for (;;) {
		uart_puts("\x1b[?25h");
		ksleep(50);
		uart_puts("\x1b[?25l");
		ksleep(50);
	}
}

void kernel_main(void) {
	uart_init();
	RUN_SUITE(uart);
	RUN_SUITE(kprintf);
	RUN_SUITE(exception);

	mmu_init();
	uart_use_virtual_address();
	RUN_SUITE(mmu);

	pmem_init();
	RUN_SUITE(pmem);
	RUN_SUITE(proc);

	gic_init();
	timer_init();
	enable_irq();
	RUN_SUITE(timer);

	TEST_REPORT();
	TEST_EXIT();

	proc_create(cursor_blink);
	proc_create(prompt);
	scheduler();
}
