#include <stdint.h>
#include <mcsos/arch/cpu.h>
#include <mcsos/arch/idt.h>
#include <mcsos/arch/pic.h>
#include <mcsos/arch/pit.h>
#include <mcsos/kernel/log.h>
#include <mcsos/kernel/panic.h>
#include <mcsos/kernel/pmm.h>
#include <mcsos/kernel/vmm.h>
#include <mcsos/kernel/sched.h>
#include <mcsos/kernel/version.h>
#include <mcsos/kernel/serial.h>
#include "mcsos/kmem.h"
#include <mcsos/syscall.h>

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
}

#define M8_BOOT_HEAP_SIZE (64u * 1024u)
static unsigned char m8_boot_heap[M8_BOOT_HEAP_SIZE] __attribute__((aligned(4096)));

static void m8_heap_bootstrap(void) {
    int rc = kmem_init(m8_boot_heap, sizeof(m8_boot_heap));
    if (rc != 0) {
        KERNEL_PANIC("M8: kmem_init failed", (uint64_t)rc);
    }

    void *probe = kmem_alloc(128);
    if (probe == (void *)0) {
        KERNEL_PANIC("M8: kmem_alloc probe failed", 0);
    }

    if (kmem_free_checked(probe) != 0) {
        KERNEL_PANIC("M8: kmem_free_checked probe failed", 0);
    }

    kmem_stats_t st;
    kmem_get_stats(&st);
    log_writeln("[MCSOS:M8] kmem initialized");
    log_key_value_hex64("[MCSOS:M8] heap total_bytes", (uint64_t)st.total_bytes);
    log_key_value_hex64("[MCSOS:M8] heap free_bytes", (uint64_t)st.free_bytes);
    log_key_value_hex64("[MCSOS:M8] heap largest_free", (uint64_t)st.largest_free);
    log_key_value_hex64("[MCSOS:M8] heap block_count", (uint64_t)st.block_count);
}

/* ===== M9: kernel thread scheduler (cooperative, stack statik) ===== */

#define M9_DEMO_MAX_TICKS 6u

static mcsos_scheduler_t g_sched;
static mcsos_thread_t g_boot_thread;
static mcsos_thread_t g_thread_a;
static mcsos_thread_t g_thread_b;
static unsigned char g_stack_a[8192] __attribute__((aligned(16)));
static unsigned char g_stack_b[8192] __attribute__((aligned(16)));

static void m9_demo_thread_a(void *arg) {
    (void)arg;
    for (unsigned i = 0; i < M9_DEMO_MAX_TICKS; i++) {
        log_writeln("[MCSOS:M9] thread A tick");
        mcsos_sched_yield(&g_sched);
    }
    for (;;) {
        mcsos_sched_yield(&g_sched);
    }
}

static void m9_demo_thread_b(void *arg) {
    (void)arg;
    for (unsigned i = 0; i < M9_DEMO_MAX_TICKS; i++) {
        log_writeln("[MCSOS:M9] thread B tick");
        mcsos_sched_yield(&g_sched);
    }
    for (;;) {
        mcsos_sched_yield(&g_sched);
    }
}

static void m9_scheduler_bootstrap(void) {
    int rc = mcsos_scheduler_init(&g_sched, &g_boot_thread);
    if (rc != MCSOS_SCHED_OK) {
        KERNEL_PANIC("M9: mcsos_scheduler_init failed", (uint64_t)rc);
    }

#ifdef MCSOS_M9_DEMO_BAD_STACK
    {
        log_writeln("[MCSOS:M9] triggering controlled bad-stack panic test");
        static unsigned char tiny_stack[64] __attribute__((aligned(16)));
        mcsos_thread_t bad_thread;
        int bad_rc = mcsos_thread_prepare(&bad_thread, "bad-stack", m9_demo_thread_a, (void *)0,
                                           tiny_stack, sizeof(tiny_stack), 999);
        if (bad_rc != MCSOS_SCHED_OK) {
            KERNEL_PANIC("M9: demo undersized stack correctly rejected", (uint64_t)bad_rc);
        }
    }
#endif

    rc = mcsos_thread_prepare(&g_thread_a, "demo-a", m9_demo_thread_a, (void *)0,
                              g_stack_a, sizeof(g_stack_a), g_sched.next_id++);
    if (rc != MCSOS_SCHED_OK) {
        KERNEL_PANIC("M9: mcsos_thread_prepare a failed", (uint64_t)rc);
    }

    rc = mcsos_thread_prepare(&g_thread_b, "demo-b", m9_demo_thread_b, (void *)0,
                              g_stack_b, sizeof(g_stack_b), g_sched.next_id++);
    if (rc != MCSOS_SCHED_OK) {
        KERNEL_PANIC("M9: mcsos_thread_prepare b failed", (uint64_t)rc);
    }

    rc = mcsos_sched_enqueue(&g_sched, &g_thread_a);
    if (rc != MCSOS_SCHED_OK) {
        KERNEL_PANIC("M9: enqueue thread a failed", (uint64_t)rc);
    }

#ifdef MCSOS_M9_DEMO_BAD_MAGIC
    log_writeln("[MCSOS:M9] triggering controlled bad-magic panic test");
    g_thread_b.magic = 0xdeaddeaddeadbeefULL;
#endif

    rc = mcsos_sched_enqueue(&g_sched, &g_thread_b);
    if (rc != MCSOS_SCHED_OK) {
        KERNEL_PANIC("M9: enqueue thread b failed (corrupt TCB rejected)", (uint64_t)rc);
    }

    if (mcsos_sched_validate(&g_sched) != MCSOS_SCHED_OK) {
        KERNEL_PANIC("M9: sched_validate failed after setup", 0);
    }

    log_writeln("[MCSOS:M9] scheduler initialized");
    mcsos_sched_yield(&g_sched);
}


/* ===== M10: syscall ABI bootstrap ===== */

static uint64_t m10_get_ticks(void) {
    return timer_ticks();
}

static void m10_yield_current(void) {
    mcsos_sched_yield(&g_sched);
}

static void m10_exit_current(int code) {
    log_writeln("[MCSOS:M10] exit_thread called (no teardown yet)");
    log_key_value_hex64("[MCSOS:M10] exit code", (uint64_t)code);
    KERNEL_PANIC("M10: exit_thread invoked before M9 teardown ready", (uint64_t)code);
}

static int64_t m10_write_serial(const char *buf, size_t len) {
    if (buf == 0) return -1;
    for (size_t i = 0; i < len; i++) {
        serial_putc(buf[i]);
    }
    return (int64_t)len;
}

static void m10_syscall_bootstrap(void) {
    mcsos_syscall_ops_t ops = {
        .get_ticks = m10_get_ticks,
        .yield_current = m10_yield_current,
        .exit_current = m10_exit_current,
        .write_serial = m10_write_serial,
    };
    mcsos_syscall_init(&ops);
    log_writeln("[MCSOS:M10] syscall init");
}


static void m10_syscall_smoke_test(void) {
    int64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"((uint64_t)MCSOS_SYS_PING)
        : "memory"
    );
    log_writeln("[MCSOS:M10] int 0x80 smoke test executed");
    log_key_value_hex64("[MCSOS:M10] ping ret", (uint64_t)ret);
    if (ret != 0x2605020AL) {
        KERNEL_PANIC("M10: int 0x80 smoke test returned unexpected value", (uint64_t)ret);
    }
    log_writeln("[MCSOS:M10] int 0x80 smoke test PASS");
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
    m8_heap_bootstrap();
    m10_syscall_bootstrap();
    m10_syscall_smoke_test();
    m9_scheduler_bootstrap();

    for (;;) {
        cpu_hlt();
    }
}
