#include <mcsos/arch/pit.h>
#include <mcsos/arch/pic.h>
#include <mcsos/arch/io.h>
#include <mcsos/kernel/log.h>

#define PIT_CHANNEL0 0x40u
#define PIT_CMD      0x43u
#define PIT_MODE3    0x36u

static volatile uint64_t g_ticks = 0;

void pit_configure_hz(uint32_t hz) {
    uint32_t divisor = PIT_BASE_FREQUENCY_HZ / hz;
    outb(PIT_CMD, PIT_MODE3);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFFu));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8u) & 0xFFu));
}

uint64_t timer_ticks(void) {
    return g_ticks;
}

void timer_on_irq0(void) {
    g_ticks++;
    if ((g_ticks % 100u) == 0u) {
        log_key_value_hex64("[MCSOS:TIMER] ticks", g_ticks);
    }
    pic_send_eoi(0);
}
