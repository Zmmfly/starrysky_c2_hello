#include <stddef.h>
#include <stdint.h>
#include <xhive_config.h>

#include "c2_timer.h"
#include "c2_uart.h"

/**
 * @brief Print a periodic greeting through the board's CP2102 serial port.
 *
 * SYS_UART uses fixed pins and 8N1 framing. The board oscillator drives both
 * UART and Timer_0; no PLL, GPIO, interrupt, or external RAM setup is needed.
 * Timer_0 is exclusively owned by this application for the one-second delay.
 *
 * @return int Nonzero if peripheral setup, a write, or a delay fails.
 * @note Normally never returns. Startup parks the CPU if main returns.
 */
int main(void)
{
    static const uint8_t greeting[] = "Hello World from xhive / StarrySky C2!\r\n";
    c2_sys_uart_t uart;
    size_t written;

    if (c2_sys_uart_init(&uart, C2_UART0, CONFIG_STARRYSKY_C2_CLOCK_HZ,
                         115200u) != C2_OK) {
        return 1;
    }

    for (;;) {
        /* SYS_UART has no TX-ready register. Consecutive DATA writes follow
         * the official SDK transmit path and rely on hardware pacing. */
        if (c2_sys_uart_write(&uart, greeting, sizeof(greeting) - 1u,
                              &written) != C2_OK ||
            written != sizeof(greeting) - 1u) {
            return 2;
        }
        if (c2_timer_delay_ms(C2_TIM0, CONFIG_STARRYSKY_C2_CLOCK_HZ,
                               1000u, UINT32_C(10000000)) != C2_OK) {
            return 3;
        }
    }
}
