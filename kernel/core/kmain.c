#include <stdint.h>
#include <mcsos/arch/cpu.h>
#include <mcsos/arch/idt.h>
#include <mcsos/arch/pic.h>
#include <mcsos/arch/pit.h>
#include <mcsos/kernel/log.h>
#include <mcsos/kernel/panic.h>
#include <mcsos/kernel/pmm.h>
#include <mcsos/kernel/version.h>

extern char __kernel_start[];
extern char __kernel_end[];

#define M6_DEMO_PHYS_BYTES (128ULL * 1024ULL * 1024ULL)

static struct pmm_state g_pmm;
static uint8_t g_pmm_bitmap[PMM_BITMAP_BYTES] __attribute__((aligned(4096)));

static void kernel_memory_init(void) {
    uint64_t kstart = (uint64_t)(uintptr_t)__kernel_start;
    uint64_t kend   = (uint64_t)(uintptr_t)__kernel_end;
    uint64_t klen   = kend - kstart;

    struct boot_mem_region regions[] = {
        { .base = 0x0000000000000000ULL, .length = M6_DEMO_PHYS_BYTES, .type = BOOT_MEM_USABLE },
        { .base = kstart, .length = klen, .type = BOOT_MEM_KERNEL_AND_MODULES },
    };

    bool ok = pmm_init_from_map(&g_pmm, regions,
                                 sizeof(regions) / sizeof(regions[0]),
                                 g_pmm_bitmap, sizeof(g_pmm_bitmap),
                                 M6_DEMO_PHYS_BYTES);
    if (!ok) {
        KERNEL_PANIC("pmm_init_from_map failed", 0);
    }

    log_writeln("[MCSOS:M6] pmm initialized");
    log_key_value_hex64("[MCSOS:M6] frames managed", pmm_frame_count(&g_pmm));
    log_key_value_hex64("[MCSOS:M6] frames free", pmm_free_count(&g_pmm));
    log_key_value_hex64("[MCSOS:M6] frames used", pmm_used_count(&g_pmm));

    uint64_t f = pmm_alloc_frame(&g_pmm);
    if (f == PMM_INVALID_FRAME) {
        KERNEL_PANIC("pmm_alloc_frame returned invalid", 0);
    }
    log_key_value_hex64("[MCSOS:M6] sample frame", f);

    if (!pmm_free_frame(&g_pmm, f)) {
        KERNEL_PANIC("pmm_free_frame failed", f);
    }
    log_writeln("[MCSOS:M6] sample alloc/free OK");
}

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

    kernel_memory_init();

    for (;;) {
        cpu_hlt();
    }
}
