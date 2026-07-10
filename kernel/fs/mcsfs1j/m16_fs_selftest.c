/*
 * MCSOS M16 - kernel-side self-test for MCSFS1J journal/recovery.
 * Uses a static RAM-backed m16_blockdev, mirroring the M13/M14 self-test pattern.
 * This is NOT wired to the real M14 block device yet; see M16 guide Langkah 5 note
 * on driver ownership/locking review before that step.
 */
#include <mcsos/kernel/log.h>
#include <mcsos/kernel/panic.h>
#include "m16_mcsfs_journal.h"

static struct m16_blockdev g_m16_dev;

void m16_fs_selftest(void) {
    uint8_t out[64];
    uint32_t out_size = 0;
    const uint8_t hello[] = { 'h', 'e', 'l', 'l', 'o', '-', 'm', '1', '6' };
    const uint8_t crashy[] = { 'c', 'r', 'a', 's', 'h', '-', 'r', 'e', 'p', 'l', 'a', 'y' };

    m16_dev_init(&g_m16_dev);

    if (m16_format(&g_m16_dev) != M16_E_OK) {
        KERNEL_PANIC("M16: format failed", 0);
    }
    if (m16_fsck(&g_m16_dev) != M16_E_OK) {
        KERNEL_PANIC("M16: fsck after format failed", 0);
    }
    if (m16_write_file(&g_m16_dev, "hello.txt", hello, (uint32_t)sizeof(hello)) != M16_E_OK) {
        KERNEL_PANIC("M16: write hello failed", 0);
    }
    if (m16_read_file(&g_m16_dev, "hello.txt", out, sizeof(out), &out_size) != M16_E_OK) {
        KERNEL_PANIC("M16: read hello failed", 0);
    }
    if (out_size != sizeof(hello) || out[0] != 'h' || out[8] != '6') {
        KERNEL_PANIC("M16: hello content mismatch", 0);
    }

    /* Simulate crash: commit record written, home-location write skipped. */
    if (m16_write_file_ex(&g_m16_dev, "crash.txt", crashy, (uint32_t)sizeof(crashy), 1) != M16_E_OK) {
        KERNEL_PANIC("M16: crash transaction commit failed", 0);
    }
    if (m16_journal_recover(&g_m16_dev) != M16_E_OK) {
        KERNEL_PANIC("M16: journal replay after committed crash failed", 0);
    }
    if (m16_read_file(&g_m16_dev, "crash.txt", out, sizeof(out), &out_size) != M16_E_OK) {
        KERNEL_PANIC("M16: read crash.txt after replay failed", 0);
    }
    if (out_size != sizeof(crashy) || out[0] != 'c' || out[11] != 'y') {
        KERNEL_PANIC("M16: crash.txt content mismatch after replay", 0);
    }
    if (m16_fsck(&g_m16_dev) != M16_E_OK) {
        KERNEL_PANIC("M16: fsck after replay failed", 0);
    }

    log_writeln("[MCSOS:M16] mcsfs1j journal format/write/crash/replay/fsck self-test PASS");
}
