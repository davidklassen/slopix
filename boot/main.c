void uart_init(void);
void uart_puts(const char *s);
void halt(void);

void boot_main(void) {
	uart_init();
	uart_puts("slopix bootloader\n");
	halt();
}
