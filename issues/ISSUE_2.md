# ISSUE_2: uart_read() Exits After Reading Single Character

## Severity
**Critical**

## File and Location
- **File:** `kernel/uart.c`
- **Line:** 110
- **Function:** `uart_read()`

## Description
The `uart_read()` function has an unconditional `break` statement at line 110 that immediately exits the outer `while (i < len)` loop after reading exactly one character from the UART receive buffer, regardless of the requested `len` parameter.

The function signature promises to read up to `len` bytes, but always returns after reading the first available character. This severely limits UART input functionality, particularly for:
- Reading multi-character input sequences
- Terminal input that requires buffering multiple bytes
- Any caller expecting to read more than 1 character

## Current Behavior
```c
int uart_read(char *buf, unsigned long len) {
	unsigned long i = 0;
	while (i < len) {
		while (uart_rx.head == uart_rx.tail) {
			if (proc_wait(&uart_rx) < 0) {
				return i > 0 ? (int)i : -EINTR;
			}
		}
		buf[i++] = uart_rx.buf[uart_rx.tail];
		uart_rx.tail = (uart_rx.tail + 1) % UART_RX_BUF_SIZE;
		break;  // <-- LINE 110: Unconditional break exits outer loop
	}
	return i;
}
```

After reading one character at line 108-109, the unconditional `break` at line 110 exits the outer loop, preventing the function from reading additional characters that may be available in the circular buffer.

## How to Reproduce
1. Call `uart_read(buf, 5)` expecting to read up to 5 bytes
2. Provide input of 5 or more characters
3. Function returns 1 (only first character read)
4. Expected: Function should read up to 5 characters and return the actual count

## Test Recommendations
Create an automated test that:
1. Simulates UART input by populating `uart_rx.buf` with multiple characters
2. Calls `uart_read(buf, N)` with `N > 1`
3. Verifies that the function reads all `N` bytes (or returns the actual count if fewer available)
4. Confirms that `uart_rx.tail` advances through all read characters

Example test scenario:
```
uart_rx.buf = "hello"
uart_rx.head = 5, uart_rx.tail = 0
result = uart_read(buf, 5)
ASSERT_EQ(result, 5, "should read 5 bytes")
ASSERT_STR_EQ(buf, "hello", "should read all characters")
```

## Fixing Recommendations
Remove the unconditional `break` statement at line 110. The function should only exit the outer loop when:
1. All requested `len` bytes have been read (`i < len` condition in outer while loop)
2. A signal interrupt occurs (handled by `proc_wait()` returning -EINTR)

**Corrected code:**
```c
int uart_read(char *buf, unsigned long len) {
	unsigned long i = 0;
	while (i < len) {
		while (uart_rx.head == uart_rx.tail) {
			if (proc_wait(&uart_rx) < 0) {
				return i > 0 ? (int)i : -EINTR;
			}
		}
		buf[i++] = uart_rx.buf[uart_rx.tail];
		uart_rx.tail = (uart_rx.tail + 1) % UART_RX_BUF_SIZE;
		// Remove the break statement
	}
	return i;
}
```

With the `break` removed, the outer loop will naturally continue to read more characters until either:
- The requested `len` characters are read
- The buffer becomes empty and `proc_wait()` returns an error
