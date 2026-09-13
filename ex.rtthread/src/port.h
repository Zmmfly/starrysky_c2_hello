#ifndef C2_NANO_PORT_H
#define C2_NANO_PORT_H
#include <rthw.h>
#include <stdint.h>

/* Only the PicoRV32 internal timer (IRQ 0) is enabled. */
#define C2_IRQ_MASK UINT32_C(0xfffffffe)
#define C2_TICK_CYCLES (CONFIG_STARRYSKY_C2_CLOCK_HZ / RT_TICK_PER_SECOND)

void c2_tick_init(void);
void c2_timer_set(uint32_t cycles);
uint32_t c2_cycle(void);
uint32_t *c2_irq_dispatch(uint32_t *frame);
#endif
