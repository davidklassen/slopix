void uart_init(void);
void uart_puts(const char *s);
void uart_putc(char c);
void uart_puthex(unsigned int val);
void halt(void);

void virtio_init(void);

int fs_init(void);
int fs_read_file(const char *path, void *buf, unsigned int max_size);
int fs_file_exists(const char *path);

void jump_to_kernel(unsigned long dtb_addr, unsigned long runtests, unsigned long entry_point);

#define KERNEL_PATH	"/boot/kernel.bin"
#define KERNEL_LOAD_PA	0x40080000UL
#define DTB_PA		0x40000000UL
#define MAX_KERNEL_SIZE (512 * 1024)

void boot_main(void) {
	uart_init();
	uart_puts("slopix bootloader\n");

	virtio_init();

	if (fs_init() < 0) {
		uart_puts("fs: init failed\n");
		halt();
	}
	uart_puts("fs: ok\n");

	uart_puts("loading ");
	uart_puts(KERNEL_PATH);
	uart_puts("\n");

	int size = fs_read_file(KERNEL_PATH, (void *)KERNEL_LOAD_PA, MAX_KERNEL_SIZE);
	if (size < 0) {
		uart_puts("kernel not found\n");
		halt();
	}

	uart_puts("kernel size: ");
	uart_puthex(size);
	uart_puts(" bytes\n");

	unsigned long runtests = fs_file_exists("/boot/runtests");

	uart_puts("booting kernel\n");
	jump_to_kernel(DTB_PA, runtests, KERNEL_LOAD_PA);
}
