#include <xhive_config.h>
#include "port.h"
#include "c2_uart.h"
#include <finsh.h>
#include <stdarg.h>

static c2_sys_uart_t console;
static rt_uint8_t heap[RT_HEAP_SIZE_KB * 1024] __attribute__((aligned(16)));
static volatile uint32_t spin_count[2];
static volatile uint32_t spin_stop;
static volatile int spin_result[2] = {-1, -1};
extern int c2_spin_probe(volatile uint32_t *counter, volatile uint32_t *stop);

void rt_hw_console_output(const char *str)
{
    /* Polling UART: no RX flow control or lossless burst guarantee. */
    while (*str) {
        if (*str == '\n')
            c2_sys_uart_write_byte(&console, '\r');
        c2_sys_uart_write_byte(&console, (uint8_t)*str++);
    }
}

/* Nano's weak implementation shares an unprotected static buffer. Serialize
 * formatting and output across threads/IRQs; tick accounting catches up after
 * the bounded print. Do not send serial bursts while long output is pending. */
int rt_kprintf(const char *fmt, ...)
{
    rt_base_t level = rt_hw_interrupt_disable();
    char buffer[RT_CONSOLEBUF_SIZE];
    va_list args;
    va_start(args, fmt);
    int length = rt_vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    rt_hw_console_output(buffer);
    rt_hw_interrupt_enable(level);
    return length < (int)sizeof(buffer) ? length : (int)sizeof(buffer) - 1;
}

char rt_hw_console_getchar(void)
{
    uint8_t byte;
    while (c2_sys_uart_read_byte_nonblocking(&console, &byte) != C2_OK)
        rt_thread_mdelay(1);
    return (char)byte;
}

static void spinner(void *parameter)
{
    unsigned index = (uintptr_t)parameter;
    /* Deliberately never yield: both counters advancing proves preemption
     * and equal-priority time slicing, rather than cooperative scheduling. */
    spin_result[index] = c2_spin_probe(&spin_count[index], &spin_stop);
}

static void application(void *parameter)
{
    (void)parameter;
    rt_thread_t a = rt_thread_create("spin0", spinner, (void *)0, 1024, 22, 5);
    rt_thread_t b = rt_thread_create("spin1", spinner, (void *)1, 1024, 22, 5);
    RT_ASSERT(a && b);
    rt_thread_startup(a);
    rt_thread_startup(b);
    rt_tick_t start = rt_tick_get();
    rt_thread_mdelay(100);
    rt_tick_t elapsed = rt_tick_get() - start;
    spin_stop = 1;
    rt_thread_mdelay(20);
    rt_kprintf("preempt: %s tick=%u spin0=%u spin1=%u\n",
               spin_count[0] && spin_count[1] && elapsed >= 100 &&
               spin_result[0] == 0 && spin_result[1] == 0 ? "PASS" : "FAIL",
               (unsigned)elapsed,
               (unsigned)spin_count[0], (unsigned)spin_count[1]);
    rt_base_t outer = rt_hw_interrupt_disable();
    rt_base_t inner = rt_hw_interrupt_disable();
    start = rt_tick_get();
    uint32_t cycle = c2_cycle();
    while (c2_cycle() - cycle < 3 * C2_TICK_CYCLES);
    rt_hw_interrupt_enable(inner);
    int held = rt_tick_get() == start;
    rt_hw_interrupt_enable(outer);
    rt_thread_mdelay(2);
    rt_kprintf("irq-mask: %s catchup=%u\n",
               held && rt_tick_get() - start >= 3 ? "PASS" : "FAIL",
               (unsigned)(rt_tick_get() - start));
}

static int uptime(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    rt_kprintf("tick=%u rate=%uHz\n", (unsigned)rt_tick_get(), RT_TICK_PER_SECOND);
    return 0;
}
MSH_CMD_EXPORT(uptime, Show the interrupt driven system tick);

int main(void)
{
    if (c2_sys_uart_init(&console, C2_UART0, CONFIG_STARRYSKY_C2_CLOCK_HZ,
                         115200u) != C2_OK)
        for (;;);
    rt_kprintf("StarrySky C2 / SYS_UART / PicoRV32 IRQ Nano\n");
    rt_show_version();
    rt_system_heap_init(heap, heap + sizeof(heap));
    rt_system_timer_init();
    rt_system_scheduler_init();
    rt_thread_t app = rt_thread_create("main", application, RT_NULL, 2048, 10, 10);
    RT_ASSERT(app);
    rt_thread_startup(app);
    extern int finsh_system_init(void);
    finsh_system_init();
    rt_thread_idle_init();
    c2_tick_init();
    rt_system_scheduler_start();
    for (;;);
}
