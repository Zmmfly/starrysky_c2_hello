#include <stddef.h>
#include <stdint.h>
#include <xhive_config.h>

#include "c2_uart.h"

/**
 * @brief Print a startup greeting and echo bytes through the CP2102 port.
 *
 * SYS_UART uses fixed pins and 115200 8N1 framing. Print Hello once so later
 * output consists only of received bytes, with no added prefix or newline.
 * Poll RX continuously; no timer, interrupt, or software buffer is required.
 *
 * @return int Nonzero if UART setup, a read, or a write fails.
 * @note Normally never returns. Startup parks the CPU if main returns.
 */
int main(void)
{
    static const uint8_t greeting[] = "Hello World from xhive Vendor StarrySky C2!\r\n";
    c2_sys_uart_t uart;
    size_t written;

    if (c2_sys_uart_init(&uart, C2_UART0, CONFIG_STARRYSKY_C2_CLOCK_HZ,
                         115200u) != C2_OK) {
        return 1;
    }

    /* SYS_UART has no TX-ready register. Writes follow the official SDK
     * transmit path and rely on hardware pacing, not a software delay. */
    if (c2_sys_uart_write(&uart, greeting, sizeof(greeting) - 1u,
                          &written) != C2_OK ||
        written != sizeof(greeting) - 1u) {
        return 2;
    }

    for (;;) {
        uint8_t byte;
        c2_status_t status = c2_sys_uart_read_byte_nonblocking(&uart, &byte);

        if (status == C2_ERROR_BUSY) {
            continue;
        }
        if (status != C2_OK) {
            return 3;
        }
        if (c2_sys_uart_write_byte(&uart, byte) != C2_OK) {
            return 4;
        }
    }
}
