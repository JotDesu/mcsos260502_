#include <mcsos/arch/idt.h>
#include <mcsos/arch/cpu.h>
#include <mcsos/arch/pic.h>
#include <mcsos/arch/pit.h>
#include <mcsos/kernel/log.h>
#include <mcsos/kernel/panic.h>

#define IDT_TYPE_INTERRUPT 0x8Eu

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} idt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} idtr_t;

static idt_entry_t g_idt[IDT_ENTRIES];

extern void *isr_stub_table[];

static void idt_set_entry(uint8_t vector, void *handler, uint16_t cs) {
    uint64_t addr = (uint64_t)handler;
    idt_entry_t *e = &g_idt[vector];
    e->offset_low  = (uint16_t)(addr & 0xFFFFu);
    e->selector    = cs;
    e->ist         = 0;
    e->type_attr   = IDT_TYPE_INTERRUPT;
    e->offset_mid  = (uint16_t)((addr >> 16u) & 0xFFFFu);
    e->offset_high = (uint32_t)((addr >> 32u) & 0xFFFFFFFFu);
    e->zero        = 0;
}

void idt_init(void) {
    uint16_t cs = cpu_read_cs();
    for (uint8_t i = 0; i < IDT_ENTRIES; i++) {
        idt_set_entry(i, isr_stub_table[i], cs);
    }
    idtr_t idtr = {
        .limit = (uint16_t)(sizeof(g_idt) - 1u),
        .base  = (uint64_t)g_idt,
    };
    __asm__ volatile ("lidt %0" :: "m"(idtr) : "memory");
    log_writeln("[MCSOS:M5] idt: loaded");
}

void x86_64_trap_dispatch(trap_frame_t *f) {
    if (f->vector >= PIC_MASTER_OFFSET) {
        uint8_t irq = (uint8_t)(f->vector - PIC_MASTER_OFFSET);
        if (irq == 0u) {
            timer_on_irq0();
        } else {
            pic_send_eoi(irq);
        }
        return;
    }
    log_key_value_hex64("[MCSOS] exception vector", f->vector);
    log_key_value_hex64("[MCSOS] error_code", f->error_code);
    log_key_value_hex64("[MCSOS] rip", f->rip);
    KERNEL_PANIC("unhandled CPU exception", f->vector);
}
