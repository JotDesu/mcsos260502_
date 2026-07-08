#include "mcs_sync.h"
#include <mcsos/kernel/log.h>
#include <mcsos/kernel/panic.h>

static mcs_spinlock_t g_boot_stats_lock;
static mcs_lockdep_state_t g_boot_lockdep;
static uint64_t g_boot_counter;

void m12_sync_selftest(void) {
    mcs_lockdep_init(&g_boot_lockdep);
    mcs_spin_init(&g_boot_stats_lock, 10u, "boot_stats");

    if (mcs_lockdep_before_acquire(&g_boot_lockdep, 10u, "boot_stats") != MCS_SYNC_OK) {
        KERNEL_PANIC("M12: lockdep acquire failed", 0);
    }

    mcs_spin_lock(&g_boot_stats_lock);
    g_boot_counter++;
    mcs_spin_unlock(&g_boot_stats_lock);

    if (mcs_lockdep_after_release(&g_boot_lockdep, 10u, "boot_stats") != MCS_SYNC_OK) {
        KERNEL_PANIC("M12: lockdep release failed", 0);
    }

    if (mcs_spin_is_locked(&g_boot_stats_lock)) {
        KERNEL_PANIC("M12: spinlock stuck locked after selftest", 0);
    }

    log_writeln("[MCSOS:M12] sync selftest passed");
}
