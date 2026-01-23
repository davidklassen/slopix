#include "uart.h"
#include "kprintf.h"
#include "gic.h"
#include "timer.h"
#include "cpu.h"
#include "pmm.h"
#include "proc.h"
#include "init.h"
#include "virtio.h"
#include "tests/test.h"

DECLARE_SUITE(uart);
DECLARE_SUITE(kprintf);
DECLARE_SUITE(exception);
DECLARE_SUITE(timer);
DECLARE_SUITE(vmm);
DECLARE_SUITE(pmm);
DECLARE_SUITE(proc);
DECLARE_SUITE(vmm_user);
DECLARE_SUITE(elf);
DECLARE_SUITE(initramfs);
DECLARE_SUITE(virtio);

void kernel_main(void) {
	uart_init();
	RUN_SUITE(uart);
	RUN_SUITE(kprintf);
	RUN_SUITE(exception);
	RUN_SUITE(vmm);

	pmm_init();
	RUN_SUITE(pmm);
	RUN_SUITE(proc);
	RUN_SUITE(vmm_user);
	RUN_SUITE(elf);
	RUN_SUITE(initramfs);

	virtio_init();
	RUN_SUITE(virtio);

	gic_init();
	timer_init();
	uart_init_irq();
	enable_irq();
	RUN_SUITE(timer);

	TEST_REPORT();

#ifdef RUN_TESTS
	init("tests");
#else
	uart_puts("\nWelcome to Slopix!\n");
	uart_puts("To exit QEMU press Ctrl-a x\n\n");
	init("shell");
#endif

	scheduler();
}
