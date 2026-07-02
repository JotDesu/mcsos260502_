#include <stdint.h>
#include <mcsos/arch/cpu.h>
#include <mcsos/arch/idt.h>
#include <mcsos/arch/pic.h>
#include <mcsos/arch/pit.h>
#include <mcsos/kernel/log.h>
#include <mcsos/kernel/panic.h>
#include <mcsos/kernel/pmm.h>
#include <mcsos/kernel/vmm.h>
#include <mcsos/kernel/version.h>

extern char __kernel_start[];
extern char __kernel_end[];

#define M6_DEMO_PHYS_BYTES (128ULL * 1024ULL * 1024ULL)

static struct pmm_state g_pmm;
static uint8_t g_pmm_bitmap[PMM_BITMAP_BYTES] __attribute__((aligned(4096)));
static struct vmm_space g_vmm;

static uint64_t kernel_vmm_alloc(void *ctx) {
    (void)ctx;
    return pmm_alloc_frame(&g_pmm);
}

static void kernel_vmm_free(void *ctx, uint64_t frame_paddr) {
    (void)ctx;
    pmm_free_frame(&g_pmm, frame_paddr);
}

static void *kernel_phys_to_virt(void *ctx, uint64_t paddr) {
    (void)ctx;
    /* M7 awal: identity map sementara untuk frame dalam demo range PMM.
       Belum memakai HHDM asli dari Limine. */
    return (void *)(uintptr_t)paddr;
}

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

    uint64_t f = pmm_alloc_frame(&g_pmm);
    if (f == PMM_INVALID_FRAME) {
        KERNEL_PANIC("pmm_alloc_frame returned invalid", 0);
    }
    if (!pmm_free_frame(&g_pmm, f)) {
        KERNEL_PANIC("pmm_free_frame failed", f);
    }
    log_writeln("[MCSOS:M6] sample alloc/free OK");
}

static void kernel_vmm_init(void) {
    uint64_t root = pmm_alloc_frame(&g_pmm);
    if (root == PMM_INVALID_FRAME) {
        KERNEL_PANIC("M7: cannot allocate root page table", 0);
    }

    uint64_t *root_virt = (uint64_t *)kernel_phys_to_virt(0, root);
    for (int i = 0; i < 512; i++) {
        root_virt[i] = 0;
    }

    int rc = vmm_space_init(&g_vmm, root, 0,
                             kernel_vmm_alloc, kernel_vmm_free, kernel_phys_to_virt);
    if (rc != VMM_MAP_OK) {
        KERNEL_PANIC("M7: vmm_space_init failed", (uint64_t)rc);
    }

    log_writeln("[MCSOS:M7] VMM core initialized");
    log_key_value_hex64("[MCSOS:M7] root_paddr", root);

    /* Demo map/query/unmap satu halaman untuk membuktikan table walker bekerja. */
    uint64_t test_vaddr = 0x0000000000600000ULL;
    uint64_t test_paddr = pmm_alloc_frame(&g_pmm);
    if (test_paddr == PMM_INVALID_FRAME) {
        KERNEL_PANIC("M7: cannot allocate demo frame", 0);
    }

    rc = vmm_map_page(&g_vmm, test_vaddr, test_paddr, VMM_PTE_WRITABLE);
    if (rc != VMM_MAP_OK) {
        KERNEL_PANIC("M7: vmm_map_page demo failed", (uint64_t)rc);
    }

    struct vmm_mapping m;
    rc = vmm_query_page(&g_vmm, test_vaddr, &m);
    if (rc != VMM_MAP_OK || m.paddr != test_paddr) {
        KERNEL_PANIC("M7: vmm_query_page demo mismatch", (uint64_t)rc);
    }
    log_key_value_hex64("[MCSOS:M7] demo map vaddr", m.vaddr);
    log_key_value_hex64("[MCSOS:M7] demo map paddr", m.paddr);

    rc = vmm_unmap_page(&g_vmm, test_vaddr);
    if (rc != VMM_MAP_OK) {
        KERNEL_PANIC("M7: vmm_unmap_page demo failed", (uint64_t)rc);
    }
    log_writeln("[MCSOS:M7] demo map/query/unmap OK");
    log_writeln("[MCSOS:M7] ready for QEMU smoke test");

#ifdef MCSOS_M7_DEMO_PAGEFAULT
    log_writeln("[MCSOS:M7] triggering controlled page fault test");
    volatile uint64_t *bad_ptr = (volatile uint64_t *)0xFFFFFF0000000000ULL;
    *bad_ptr = 0xDEADBEEFULL;
#endif

    pmm_free_frame(&g_pmm, test_paddr);

    /* Tugas wajib berhenti di sini. Jangan write_cr3 sebelum mapping kernel,
       stack, IDT/GDT, framebuffer/serial MMIO, dan PMM metadata lengkap. */
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
    kernel_vmm_init();

    for (;;) {
        cpu_hlt();
    }
}
