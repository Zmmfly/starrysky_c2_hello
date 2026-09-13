/* C2 peripheral timer / standard CSR diagnostic, independent of Nano.
 * Register addresses: embedded-sdk board/StarrySkyC2/board.h.
 * CONFIG[3] and VALUE reload semantics: reference counter_timer RTL only;
 * the readback and interrupt tests below check those assumptions on hardware.
 * No PicoRV32 maskirq/timer/retirq instructions are used.
 */
#include <stdint.h>

#define REG(addr) (*(volatile uint32_t *)(uintptr_t)(addr))
#define UART_DIV REG(0x10001000)
#define UART_DATA REG(0x10001004)
#define TIM_BASE(n) (0x10002000u + (n) * 0x1000u)
#define TIM_CFG(n) REG(TIM_BASE(n))
#define TIM_RELOAD(n) REG(TIM_BASE(n) + 4)
#define TIM_COUNT(n) REG(TIM_BASE(n) + 8)
#define PERIOD 720000u
#define CSR_READ(name) ({ uint32_t v; __asm__ volatile ("csrr %0, " #name : "=r"(v) :: "memory"); v; })
#define CSR_WRITE(name, val) __asm__ volatile ("csrw " #name ", %0" :: "r"((uint32_t)(val)) : "memory")

static volatile uint32_t irq_hits;
static volatile uint32_t irq_cause;
static volatile uint32_t irq_stamp[3];
static volatile uint32_t clock_timer;

static void puts_uart(const char *s)
{
    while (*s) UART_DATA = (uint8_t)*s++;
}

static void hex(uint32_t v)
{
    static const char digits[] = "0123456789abcdef";
    for (int shift = 28; shift >= 0; shift -= 4)
        UART_DATA = digits[(v >> shift) & 15u];
}

static void field(const char *name, uint32_t value)
{
    puts_uart(name);
    hex(value);
    puts_uart("\r\n");
}

static void pause_cycles(void)
{
    for (volatile unsigned i = 0; i < 2000; ++i)
        __asm__ volatile ("nop");
}

/* This handler only runs if standard machine-mode CSRs actually exist.
 * A synchronous trap prints evidence and stops, rather than skipping an
 * unsupported instruction and incorrectly reporting the test as successful. */
__attribute__((interrupt("machine"), aligned(4)))
static void trap_entry(void)
{
    uint32_t cause = CSR_READ(mcause);
    if (!(cause & 0x80000000u)) {
        CSR_WRITE(mie, 0);
        TIM_CFG(0) = TIM_CFG(1) = 0x100;
        field("SYNC TRAP cause=", cause);
        field("epc=", CSR_READ(mepc));
        for (;;);
    }
    irq_cause = cause;
    uint32_t n = irq_hits;
    if (n < 3) {
        irq_stamp[n] = TIM_COUNT(clock_timer);
        irq_hits = n + 1;
    }
    if (irq_hits == 3) CSR_WRITE(mie, 0);
}

static void inspect_timer(unsigned n)
{
    puts_uart(n ? "Timer1 registers\r\n" : "Timer0 registers\r\n");
    TIM_CFG(n) = 0x100; // SDK's stopped/configuration value.
    TIM_RELOAD(n) = PERIOD - 1;
    TIM_COUNT(n) = PERIOD - 1;
    field("stopped cfg=", TIM_CFG(n));
    field("reload=", TIM_RELOAD(n));
    field("loaded count=", TIM_COUNT(n));
    TIM_CFG(n) = 0x101; // SDK's polling mode, IRQ disabled.
    uint32_t a = TIM_COUNT(n);
    pause_cycles();
    uint32_t b = TIM_COUNT(n);
    TIM_CFG(n) = 0x100;
    field("count before=", a);
    field("count after=", b);
    TIM_CFG(n) = 0x108; // Test IRQ-enable readback with counting stopped.
    field("IRQ bit readback=", TIM_CFG(n));
    TIM_CFG(n) = 0x100;
}

static void test_interrupt(unsigned n)
{
    puts_uart(n ? "Timer1 standard IRQ test\r\n" : "Timer0 standard IRQ test\r\n");
    __asm__ volatile ("csrci mstatus, 8" ::: "memory");
    clock_timer = n ^ 1u;
    irq_hits = irq_cause = 0;
    TIM_CFG(clock_timer) = 0x100;
    TIM_RELOAD(clock_timer) = 0xffffffffu;
    TIM_COUNT(clock_timer) = 0xffffffffu;
    TIM_CFG(clock_timer) = 0x101;
    TIM_CFG(n) = 0x100;
    TIM_RELOAD(n) = PERIOD - 1;
    TIM_COUNT(n) = PERIOD - 1;
    TIM_CFG(n) = 0x109; // Continuous down-count + peripheral IRQ enable.
    /* MTIE/MEIE cover standard timer/external delivery. Bit 8 additionally
     * probes the reference Timer1 direct line; reserved WARL bits read zero.
     * No undocumented PLIC MMIO address is accessed. */
    CSR_WRITE(mie, (1u << 7) | (1u << 8) | (1u << 11));
    uint32_t enables = CSR_READ(mie);
    __asm__ volatile ("csrsi mstatus, 8" ::: "memory");
    for (volatile unsigned i = 0; i < 1000000 && irq_hits < 3; ++i)
        __asm__ volatile ("nop");
    __asm__ volatile ("csrci mstatus, 8" ::: "memory");
    uint32_t pending = CSR_READ(mip);
    CSR_WRITE(mie, 0);
    TIM_CFG(0) = TIM_CFG(1) = 0x100;
    field("mie readback=", enables);
    field("mip=", pending);
    field("IRQ hits=", irq_hits);
    field("IRQ cause=", irq_cause);
    if (irq_hits == 3) {
        uint32_t d0 = irq_stamp[0] - irq_stamp[1];
        uint32_t d1 = irq_stamp[1] - irq_stamp[2];
        field("interval0=", d0);
        field("interval1=", d1);
        puts_uart(d0 > PERIOD / 2 && d0 < PERIOD * 2 &&
                  d1 > PERIOD / 2 && d1 < PERIOD * 2
                  ? "PERIODIC IRQ OBSERVED\r\n" : "IRQ TIMING UNCONFIRMED\r\n");
    } else {
        puts_uart("NO PERIODIC IRQ OBSERVED (routing/controller may differ)\r\n");
    }
}

int main(void)
{
    UART_DIV = 625;
    puts_uart("C2 peripheral timer / CSR probe v1\r\n");
    inspect_timer(0);
    inspect_timer(1);
    puts_uart("Before standard mstatus read\r\n");
    uint32_t status = CSR_READ(mstatus);
    __asm__ volatile ("csrci mstatus, 8" ::: "memory");
    field("mstatus=", status);
    CSR_WRITE(mie, 0);
    puts_uart("Before mtvec setup\r\n");
    CSR_WRITE(mtvec, (uintptr_t)trap_entry);
    uint32_t vector = CSR_READ(mtvec);
    field("mtvec=", vector);
    if (vector != (uint32_t)(uintptr_t)trap_entry) {
        puts_uart("mtvec readback mismatch; IRQ test skipped\r\n");
        for (;;);
    }
    test_interrupt(0);
    test_interrupt(1);
    puts_uart("C2 timer/CSR probe complete\r\n");
    for (;;);
}
