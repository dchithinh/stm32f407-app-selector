#ifndef DEBUG_UART_H
#define DEBUG_UART_H

#include <stdbool.h>
#include <stdint.h>

void debug_uart_init(void);

/* Non-blocking: true if a byte was read (e.g. 'b' to stay in bootloader). */
bool debug_uart_getchar_nonblock(uint8_t *out);

#endif
