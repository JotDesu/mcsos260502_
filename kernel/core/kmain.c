#include <stdint.h>
#include <mcsos/arch/cpu.h>
#include <mcsos/arch/idt.h>
#include <mcsos/arch/pic.h>
#include <mcsos/arch/pit.h>
#include <mcsos/kernel/log.h>
#include <mcsos/kernel/panic.h>
#include <mcsos/kernel/version.h>

extern char __kernel_start[];
extern char __kernel_end[];

void kmain(void) {
    cpu_cli();

    log_init();
    log_write(MCSOS_NAME);
    log_write(" ");
    log_write(MCSOS_VERSION);
    log_write(" ");
    log_write(MCSOS_MILESTONE);
    log_writeln(" kernel entered");
    log_writeln("[MCSOS:M5] boot: external interrupt bring-up start");

    KERNEL_ASSERT(__kernel_end > __kernel_start);
    KERNEL_ASSERT(sizeof(uintptr_t) == 8u);

    log_key_value_hex64("[MCSOS:M5] cs", (uint64_t)cpu_read_cs());
    log_key_value_hex64("[MCSOS:M5] rflags", cpu_read_rflags());

    idt_init();

    pic_remap(PIC_MASTER_OFFSET, PIC_SLAVE_OFFSET);
    log_writeln("[MCSOS:M5] pic: remapped");

    pic_mask_all();
    pic_unmask_irq(0);
    log_writeln("[MCSOS:M5] pic: irq0 unmasked");

    pit_configure_hz(100);
    log_writeln("[MCSOS:M5] pit: configured 100Hz");

    cpu_sti();
    log_writeln("[MCSOS:M5] sti: interrupts enabled");

    for (;;) {
        cpu_hlt();
    }
}
