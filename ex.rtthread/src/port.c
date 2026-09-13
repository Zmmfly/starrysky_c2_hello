#include <xhive_config.h>
#include "port.h"

_Static_assert(sizeof(rt_ubase_t) == 4, "PicoRV32 requires 32-bit context words");
_Static_assert(RT_ALIGN_SIZE >= 16, "RISC-V thread stacks require 16-byte alignment");

static volatile rt_ubase_t switch_from;
static volatile rt_ubase_t switch_to;
static volatile uint32_t switch_pending;
static uint32_t next_tick;

/* A complete 128-byte frame: word 0 is PC; word n is xn. x2 is the
 * implicit pre-interrupt SP; gp/tp (x3/x4) are reserved for IRQ hardware. */
rt_uint8_t *rt_hw_stack_init(void *entry, void *parameter,
                           rt_uint8_t *stack_addr, void *exit)
{
    uintptr_t top = RT_ALIGN_DOWN((uintptr_t)stack_addr + sizeof(uint32_t), 16);
    uint32_t *frame = (uint32_t *)(top - 128);
    rt_memset(frame, 0, 128);
    frame[0] = (uintptr_t)entry;
    frame[1] = (uintptr_t)exit;
    frame[10] = (uintptr_t)parameter;
    return (rt_uint8_t *)frame;
}

void c2_tick_init(void)
{
    next_tick = c2_cycle() + C2_TICK_CYCLES;
    c2_timer_set(C2_TICK_CYCLES);
}

/* All switches enter the hardware IRQ path, including voluntary switches.
 * This avoids enabling interrupts between restoring gp (return PC) and
 * retirq outside IRQ context, where another IRQ could overwrite that PC.
 * rt_schedule() calls with IRQs masked and has already selected the new
 * thread, so consume this request before accounting any elapsed ticks. */
void rt_hw_context_switch(rt_ubase_t from, rt_ubase_t to)
{
    switch_from = from;
    switch_to = to;
    switch_pending = 1;
    c2_timer_set(1);
    rt_hw_interrupt_enable(C2_IRQ_MASK);
    while (switch_pending)
        __asm__ volatile ("nop");
    rt_hw_interrupt_disable();
}

void rt_hw_context_switch_to(rt_ubase_t to)
{
    rt_hw_context_switch(0, to);
    for (;;);
}

void rt_hw_context_switch_interrupt(rt_ubase_t from, rt_ubase_t to)
{
    if (!switch_pending)
        switch_from = from;
    switch_to = to;
    switch_pending = 1;
}

/* Called with IRQs masked, on the interrupted thread's aligned stack.
 * PicoRV32 does not nest IRQs. rt_interrupt_leave() only updates nesting;
 * the scheduler records a pending switch through the function above. */
uint32_t *c2_irq_dispatch(uint32_t *frame)
{
    if (!switch_pending) {
        rt_interrupt_enter();
        uint32_t now = c2_cycle();
        while ((int32_t)(now - next_tick) >= 0) {
            next_tick += C2_TICK_CYCLES;
            rt_tick_increase();
        }
        rt_interrupt_leave();
    }
    if (switch_pending) {
        if (switch_from)
            *(uint32_t **)switch_from = frame;
        frame = *(uint32_t **)switch_to;
        switch_pending = 0;
    }
    /* Preserve the periodic deadline across forced context-switch IRQs. */
    int32_t remaining = (int32_t)(next_tick - c2_cycle());
    c2_timer_set(remaining > 0 ? (uint32_t)remaining : 1u);
    return frame;
}
