void uart_init(void);
void uart_puts(const char *s);
void uart_putc(char c);
void uart_puthex(unsigned int val);
void uart_puthex8(unsigned char val);
void halt(void);

void virtio_init(void);
int virtio_read(unsigned long sector, void *buf);

int fs_init(void);
int fs_read_file(const char *path, void *buf, unsigned int max_size);

#define FILE_BUF_PA 0x40074000UL

void boot_main(void) {
	uart_init();
	uart_puts("slopix bootloader\n");

	virtio_init();

	if (fs_init() < 0) {
		uart_puts("fs: init failed\n");
		halt();
	}
	uart_puts("fs: ok\n");

	unsigned char *buf = (unsigned char *)FILE_BUF_PA;
	int size = fs_read_file("/hello", buf, 256);
	if (size < 0) {
		uart_puts("read failed\n");
		halt();
	}

	uart_puts("contents: ");
	for (int i = 0; i < size; i++)
		uart_putc(buf[i]);
	uart_puts("\n");

	halt();
}
