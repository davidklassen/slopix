#include "uart.h"
#include "kprintf.h"
#include "gic.h"
#include "timer.h"
#include "cpu.h"
#include "pmm.h"
#include "proc.h"
#include "init.h"
#include "virtio.h"
#include "bio.h"
#include "fs.h"
#include "file.h"
#include "console.h"
#include "disk.h"
#include "dtb.h"
#include "cmdline.h"
#include "initramfs.h"
#include "tests/test.h"

DECLARE_SUITE(string);
DECLARE_SUITE(sync);
DECLARE_SUITE(uart);
DECLARE_SUITE(kprintf);
DECLARE_SUITE(exception);
DECLARE_SUITE(timer);
DECLARE_SUITE(vmm);
DECLARE_SUITE(tlb);
DECLARE_SUITE(pmm);
DECLARE_SUITE(virtio);
DECLARE_SUITE(virtio_features);
DECLARE_SUITE(virtio_queue);
DECLARE_SUITE(virtio_read);
DECLARE_SUITE(virtio_write);
DECLARE_SUITE(virtio_intr);
DECLARE_SUITE(virtio_errors);
DECLARE_SUITE(bio);
DECLARE_SUITE(fs);
DECLARE_SUITE(fs_dir);
DECLARE_SUITE(fs_read);
DECLARE_SUITE(fs_file);
DECLARE_SUITE(console);
DECLARE_SUITE(pipe);
DECLARE_SUITE(dtb);
DECLARE_SUITE(cmdline);
DECLARE_SUITE(gic);

void kernel_main(void) {
	uart_init();
	console_init();
	disk_init();

	extern unsigned long _dtb_address;
	dtb_init((void *)_dtb_address);
	initramfs_init();

	unsigned long initrd_start = dtb_get_initrd_start();
	unsigned long initrd_end = dtb_get_initrd_end();
	if (initrd_start != 0 && initrd_end != 0) {
		pmm_reserve_region(initrd_start, initrd_end);
	}

	RUN_SUITE(dtb);
	cmdline_init(dtb_get_bootargs());
	RUN_SUITE(cmdline);
	RUN_SUITE(string);
	RUN_SUITE(sync);
	RUN_SUITE(uart);
	RUN_SUITE(kprintf);
	RUN_SUITE(exception);
	RUN_SUITE(vmm);

	pmm_init();
	RUN_SUITE(pmm);
	RUN_SUITE(tlb);

	virtio_init();
	RUN_SUITE(virtio);
	RUN_SUITE(virtio_features);
	RUN_SUITE(virtio_queue);
	RUN_SUITE(virtio_read);
	RUN_SUITE(virtio_write);

	bio_init();

	gic_init();
	RUN_SUITE(gic);
	timer_init();
	uart_init_irq();
	virtio_init_irq();
	enable_irq();
	RUN_SUITE(timer);
	RUN_SUITE(virtio_intr);
	RUN_SUITE(virtio_errors);
	RUN_SUITE(bio);

	fs_init(0);
	RUN_SUITE(fs);
	RUN_SUITE(fs_dir);
	RUN_SUITE(fs_read);
	RUN_SUITE(fs_file);
	RUN_SUITE(console);
	RUN_SUITE(pipe);

	TEST_REPORT();

	const char *init_prog = cmdline_get("init");
	if (!init_prog) {
		kpanic("init= not specified in kernel command line");
	}

	init(init_prog);

	proc_scheduler();
}
