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
#include <mcsos/user/m11_elf_loader.h>
#include <mcsos/lib/string.h>
#include "mcs_sync.h"
#include "mcs_vfs.h"
#include "mcsos/block.h"

void m12_sync_selftest(void);

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


/* ===== M11: ELF64 user process image loader (parse-only, konservatif) ===== */

#define M11_DEMO_IMAGE_SIZE 12288u
static unsigned char g_m11_demo_image[M11_DEMO_IMAGE_SIZE] __attribute__((aligned(16)));

static void m11_build_demo_image(unsigned char *image) {
    memset(image, 0, M11_DEMO_IMAGE_SIZE);
    struct m11_elf64_ehdr *eh = (struct m11_elf64_ehdr *)(void *)image;
    eh->e_ident[0] = M11_ELFMAG0;
    eh->e_ident[1] = M11_ELFMAG1;
    eh->e_ident[2] = M11_ELFMAG2;
    eh->e_ident[3] = M11_ELFMAG3;
    eh->e_ident[4] = M11_ELFCLASS64;
    eh->e_ident[5] = M11_ELFDATA2LSB;
    eh->e_ident[6] = M11_EV_CURRENT;
    eh->e_type = M11_ET_EXEC;
    eh->e_machine = M11_EM_X86_64;
    eh->e_version = M11_EV_CURRENT;
    eh->e_entry = 0x0000000000401000ull;
    eh->e_phoff = sizeof(struct m11_elf64_ehdr);
    eh->e_ehsize = sizeof(struct m11_elf64_ehdr);
    eh->e_phentsize = sizeof(struct m11_elf64_phdr);
    eh->e_phnum = 2u;
    struct m11_elf64_phdr *ph = (struct m11_elf64_phdr *)(void *)(image + eh->e_phoff);
    ph[0].p_type = M11_PT_LOAD;
    ph[0].p_flags = M11_PF_R | M11_PF_X;
    ph[0].p_offset = 0x1000u;
    ph[0].p_vaddr = 0x0000000000400000ull;
    ph[0].p_filesz = 16u;
    ph[0].p_memsz = 4096u;
    ph[0].p_align = M11_PAGE_SIZE;
    ph[1].p_type = M11_PT_LOAD;
    ph[1].p_flags = M11_PF_R | M11_PF_W;
    ph[1].p_offset = 0x2000u;
    ph[1].p_vaddr = 0x0000000000401000ull;
    ph[1].p_filesz = 8u;
    ph[1].p_memsz = 4096u;
    ph[1].p_align = M11_PAGE_SIZE;
}

static void m11_elf_loader_bootstrap(void) {
    struct m11_user_region region;
    region.base = 0x0000000000400000ull;
    region.limit = 0x0000008000000000ull;

    m11_build_demo_image(g_m11_demo_image);
    log_writeln("[MCSOS:M11] elf: ident ok");

    struct m11_process_image_plan plan;
    int rc = m11_elf64_plan_load(g_m11_demo_image, M11_DEMO_IMAGE_SIZE, region, &plan);
    if (rc != M11_OK) {
        KERNEL_PANIC("M11: m11_elf64_plan_load failed", (uint64_t)rc);
    }

    log_key_value_hex64("[MCSOS:M11] elf phnum", 2u);
    for (uint32_t i = 0; i < plan.segment_count; i++) {
        log_key_value_hex64("[MCSOS:M11] segment vaddr", plan.segments[i].vaddr);
        log_key_value_hex64("[MCSOS:M11] segment filesz", plan.segments[i].filesz);
        log_key_value_hex64("[MCSOS:M11] segment memsz", plan.segments[i].memsz);
        log_key_value_hex64("[MCSOS:M11] segment flags", (uint64_t)plan.segments[i].flags);
    }
    log_key_value_hex64("[MCSOS:M11] elf plan entry", plan.entry);
    log_writeln("[MCSOS:M11] elf: plan ok");
    log_writeln("[MCSOS:M11] user image plan ready");
}


/* ===== M13: VFS minimal, FD table, RAMFS self-test ===== */

static mcs_ramfs_t g_m13_ramfs;
static mcs_process_t g_m13_test_process;

static void m13_vfs_selftest(void) {
    int fd;
    mcs_ssize_t n;
    char buf[32];

    mcs_ramfs_init(&g_m13_ramfs);
    if (mcs_ramfs_seed_file(&g_m13_ramfs, "/m13-demo.txt",
                             (const uint8_t *)"mcsos-m13-ramfs-ok", 18) != MCS_OK) {
        KERNEL_PANIC("M13: ramfs seed failed", 0);
    }

    g_m13_test_process.pid = 1;
    mcs_fd_table_init(&g_m13_test_process.fd_table);

    fd = mcs_sys_open(&g_m13_test_process, &g_m13_ramfs, "/m13-demo.txt", MCS_O_RDONLY);
    if (fd < 0) {
        KERNEL_PANIC("M13: sys_open demo failed", (uint64_t)(int64_t)fd);
    }

    memset(buf, 0, sizeof(buf));
    n = mcs_sys_read(&g_m13_test_process, fd, buf, 18);
    if (n != 18) {
        KERNEL_PANIC("M13: sys_read demo failed", (uint64_t)(int64_t)n);
    }

    if (mcs_sys_lseek(&g_m13_test_process, fd, 0, MCS_SEEK_SET) != 0) {
        KERNEL_PANIC("M13: sys_lseek demo failed", 0);
    }

    if (mcs_sys_close(&g_m13_test_process, fd) != MCS_OK) {
        KERNEL_PANIC("M13: sys_close demo failed", 0);
    }

    if (mcs_sys_read(&g_m13_test_process, fd, buf, 1) != MCS_EBADF) {
        KERNEL_PANIC("M13: EBADF check failed after close", 0);
    }

    fd = mcs_sys_open(&g_m13_test_process, &g_m13_ramfs, "/m13-log.txt",
                       MCS_O_CREAT | MCS_O_RDWR | MCS_O_TRUNC);
    if (fd < 0) {
        KERNEL_PANIC("M13: sys_open create failed", (uint64_t)(int64_t)fd);
    }
    n = mcs_sys_write(&g_m13_test_process, fd, "kernel-write-ok", 15);
    if (n != 15) {
        KERNEL_PANIC("M13: sys_write demo failed", (uint64_t)(int64_t)n);
    }
    if (mcs_sys_close(&g_m13_test_process, fd) != MCS_OK) {
        KERNEL_PANIC("M13: sys_close after write failed", 0);
    }

    log_writeln("[MCSOS:M13] vfs/fd/ramfs self-test PASS");
}


/* ===== M14: block device layer + RAM block driver + buffer cache demo ===== */

#define M14_RAMDISK_STORAGE_BYTES (512u * 64u)
static unsigned char g_m14_ramdisk_storage[M14_RAMDISK_STORAGE_BYTES] __attribute__((aligned(4096)));
static mcsos_blk_device_t g_m14_ramdisk_dev;
static mcsos_ramblk_t g_m14_ramdisk;

static void m14_block_demo_init(void) {
    mcsos_blk_registry_reset();

    mcsos_blk_status_t st = mcsos_ramblk_init(&g_m14_ramdisk_dev,
                                              &g_m14_ramdisk,
                                              "ram0",
                                              g_m14_ramdisk_storage,
                                              sizeof(g_m14_ramdisk_storage),
                                              512u);
    if (st != MCSOS_BLK_OK) {
        KERNEL_PANIC("M14: mcsos_ramblk_init failed", (uint64_t)(int64_t)st);
    }

    st = mcsos_blk_register(&g_m14_ramdisk_dev);
    if (st != MCSOS_BLK_OK) {
        KERNEL_PANIC("M14: mcsos_blk_register failed", (uint64_t)(int64_t)st);
    }

    log_writeln("[MCSOS:M14] block layer initialized");
    log_key_value_hex64("[MCSOS:M14] ram0 block_size", g_m14_ramdisk_dev.block_size);
    log_key_value_hex64("[MCSOS:M14] ram0 block_count", g_m14_ramdisk_dev.block_count);

    unsigned char pattern[512];
    unsigned char readback[512];
    for (unsigned i = 0; i < sizeof(pattern); i++) {
        pattern[i] = (unsigned char)(i + 0x14u);
    }
    memset(readback, 0, sizeof(readback));

    st = mcsos_blk_write(&g_m14_ramdisk_dev, 0u, 1u, pattern);
    if (st != MCSOS_BLK_OK) {
        KERNEL_PANIC("M14: demo blk_write failed", (uint64_t)(int64_t)st);
    }
    st = mcsos_blk_read(&g_m14_ramdisk_dev, 0u, 1u, readback);
    if (st != MCSOS_BLK_OK) {
        KERNEL_PANIC("M14: demo blk_read failed", (uint64_t)(int64_t)st);
    }
    {
        int m14_mismatch = 0;
        for (unsigned i = 0; i < sizeof(pattern); i++) {
            if (pattern[i] != readback[i]) {
                m14_mismatch = 1;
                break;
            }
        }
        if (m14_mismatch) {
            KERNEL_PANIC("M14: demo read/write mismatch", 0);
        }
    }

    log_writeln("[MCSOS:M14] ram0 read/write self-test PASS");
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
    m11_elf_loader_bootstrap();
    m12_sync_selftest();
    m13_vfs_selftest();
    m14_block_demo_init();
    m9_scheduler_bootstrap();

    for (;;) {
        cpu_hlt();
    }
}
