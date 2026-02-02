void uart_init(void);
void uart_puts(const char *s);
void uart_putc(char c);
void uart_puthex(unsigned int val);
void uart_puthex8(unsigned char val);
void halt(void);

void virtio_init(void);
int virtio_read(unsigned long sector, void *buf);

#define READ_BUF_PA 0x40073000UL

void boot_main(void) {
	uart_init();
	uart_puts("slopix bootloader\n");

	virtio_init();

	unsigned char *buf = (unsigned char *)READ_BUF_PA;

	uart_puts("reading sector 2 (superblock)\n");
	if (virtio_read(2, buf) < 0) {
		uart_puts("read failed\n");
		halt();
	}

	uart_puts("data: ");
	for (int i = 0; i < 16; i++) {
		uart_puthex8(buf[i]);
		uart_putc(' ');
	}
	uart_puts("\n");

	halt();
}
