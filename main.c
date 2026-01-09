#include "uart.h"
#include "printf.h"

void main(void) {
    uart_init();
    printf("SLOPIX\n");

    // Infinite loop
    while (1) {
        __asm__ volatile("wfe");
    }
}
