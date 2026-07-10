# Laporan Praktikum M9 — Kernel Thread Scheduler (Cooperative Scheduling & Context Switch)

## MCSOS 260502

**Nama file laporan:** `laporan_praktikum_M9_[NIM_Kelompok].md`
**Nama sistem operasi:** MCSOS versi 260502
**Target default:** x86_64, QEMU, Windows 11 x64 + WSL 2, kernel monolitik pendidikan, C freestanding dengan assembly minimal, POSIX-like subset
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.
**Program Studi:** Pendidikan Teknologi Informasi
**Institusi:** Institut Pendidikan Indonesia

---

## 0. Metadata Laporan

| Atribut | Isi |
|---|---|
| Kode praktikum | M9 |
| Judul praktikum | Kernel Thread Scheduler (cooperative scheduling, context switch, panic path TCB) |
| Jenis pengerjaan | Individu |
| Nama mahasiswa | Andiana Jamaludin Malik |
| NIM | 2583207073016 |
| Kelas | 1B |
| Nama kelompok | - |
| Anggota kelompok | - |
| Tanggal praktikum | 2026-07-06 s.d. 2026-07-07 |
| Tanggal pengumpulan | 2026-07-07 |
| Repository | ~/src/mcsos |
| Remote repository | https://github.com/JotDesu/mcsos260502_.git |
| Branch kerja | m9-kernel-thread-scheduler |
| Commit checkpoint awal | `aa5d1b9` (checkpoint before M9 scheduler) |
| Commit akhir | `18c6c35` (M9: verified panic path for undersized stack and corrupt TCB magic (kriteria #10 PASS)) |
| Status readiness yang diklaim | check-m9 PASS, QEMU smoke test PASS, panic path kriteria #10 PASS, GDB context-switch evidence terkumpul |

---

## 1. Sampul

# Laporan Praktikum M9
## Kernel Thread Scheduler (Cooperative Scheduling & Context Switch)

Disusun oleh:

| Nama | NIM | Kelas | Peran |
|---|---|---|---|
| Andiana Jamaludin Malik | 2583207073016 | 1B | Individu |

Dosen Pengampu: **Muhaemin Sidiq, S.Pd., M.Pd.**
Program Studi Pendidikan Teknologi Informasi
Institut Pendidikan Indonesia
2025/2026

---

## 2. Pernyataan Orisinalitas dan Integritas Akademik

Saya menyatakan bahwa laporan ini disusun berdasarkan pekerjaan praktikum sendiri sesuai panduan M9, melanjutkan checkpoint hasil M0-M8 yang sudah dikerjakan pada milestone sebelumnya.

| Pernyataan | Status |
|---|---|
| Semua potongan kode eksternal diberi atribusi | Ya (panduan M9 dari dosen) |
| Semua penggunaan AI assistant dicatat | Ya |
| Repository yang dikumpulkan sesuai commit akhir | Ya |
| Tidak ada klaim readiness tanpa bukti | Ya |

Catatan penggunaan bantuan eksternal:

```text
- Panduan praktikum M9 (OS_panduan_M9.pdf) sebagai referensi utama
- Template laporan praktikum (os_template_laporan_praktikum.md)
- AI assistant digunakan untuk membantu menyusun laporan dan analisis
- Semua source code diimplementasikan berdasarkan panduan dosen dan hasil kerja mandiri di WSL2
```

---

## 3. Tujuan Praktikum

1. **Tujuan teknis 1:** Membuat struct dan header scheduler (`mcsos_thread_t`, `mcsos_scheduler_t`, `mcsos_thread_context_t`) pada `kernel/include/mcsos/kernel/sched.h`
2. **Tujuan teknis 2:** Mengimplementasikan kernel thread scheduler cooperative (`kernel/core/sched.c`) — init, enqueue, pick_next, yield, tick, block, mark_ready, ready_count, validate
3. **Tujuan teknis 3:** Menulis low-level context switch dalam assembly x86_64 (`kernel/arch/x86_64/src/context_switch.S`) yang menyimpan/memulihkan `rsp, rbp, rbx, r12-r15` serta trampoline masuk ke entry thread
4. **Tujuan teknis 4:** Membuat host unit test (`kernel/tests/test_sched_host.c`) yang memverifikasi scheduler tanpa perlu boot QEMU (`MCSOS_HOST_TEST`)
5. **Tujuan teknis 5:** Menambahkan target `check-m9` pada Makefile yang menjalankan build, host test, `nm -u` (no undefined symbol), dan `objdump` verifikasi simbol `mcsos_context_switch`
6. **Tujuan teknis 6:** Mengintegrasikan scheduler ke `kmain.c` melalui `m9_scheduler_bootstrap()` sehingga dua thread demo (A dan B) berjalan bergantian di QEMU
7. **Tujuan teknis 7:** Memverifikasi panic path scheduler untuk kondisi TCB rusak — stack terlalu kecil (undersized stack) dan magic number TCB yang tidak valid (kriteria #10)
8. **Tujuan konseptual 1:** Memahami cooperative scheduling — ready queue (linked list), current thread pointer, context switch dua arah (save old context, restore next context)
9. **Tujuan konseptual 2:** Memahami mekanisme trampoline thread (`mcsos_thread_trampoline`) sebagai titik masuk pertama sebuah thread baru
10. **Tujuan validasi:** Menyimpan log build, log QEMU (boot chain M3–M9), log panic, evidence `readelf`/`objdump`/`nm`, sesi GDB remote debugging, serta hash SHA-256 artefak sebagai bukti deterministik

---

## 4. Capaian Pembelajaran Praktikum

Setelah praktikum ini, mahasiswa mampu:

| CPL/CPMK praktikum | Bukti yang harus ditunjukkan |
|---|---|
| Menjelaskan struktur data scheduler (TCB, ready queue, context) | `kernel/include/mcsos/kernel/sched.h`, `sched.c` |
| Mengimplementasikan cooperative scheduler dalam C freestanding | `mcsos_scheduler_init`, `mcsos_sched_enqueue`, `mcsos_sched_pick_next`, `mcsos_sched_yield` |
| Menulis context switch dalam inline/standalone assembly x86_64 | `context_switch.S`, `mcsos_context_switch` |
| Membuat host unit test tanpa dependensi hardware | `test_sched_host.c`, target `check-m9` |
| Mengintegrasikan scheduler ke boot sequence kernel | `kmain.c`, `m9_scheduler_bootstrap()` |
| Memverifikasi panic path untuk kondisi TCB tidak valid | `evidence/m9/panic_bad_stack.log`, `panic_bad_magic.log` |
| Menjalankan QEMU headless dan menyimpan log serial multi-milestone (M3–M9) | `evidence/m9/qemu_m9_fix1.log` |
| Melakukan audit statis ELF scheduler (readelf, nm, objdump) | `evidence/m9/sched_readelf_header.log`, `sched_objdump_key.log` |
| Melakukan debugging context switch dengan GDB remote | `evidence/m9/gdb_session.log` |
| Mengklasifikasikan failure modes M9 | Analisis pada bagian 15 |

---

## 5. Peta Milestone MCSOS

| Milestone | Fokus | Status dalam laporan |
|---|---|---|
| M0 | Requirements, governance, baseline arsitektur | [x] selesai (checkpoint sebelumnya) |
| M1 | Toolchain reproducible, Git, QEMU, GDB, metadata build | [x] selesai (checkpoint sebelumnya) |
| M2 | Boot image, kernel ELF64, early console | [x] selesai (checkpoint sebelumnya) |
| M3 | Panic path, linker map, GDB, observability awal | [x] selesai (checkpoint sebelumnya) |
| M4 | Trap, exception, interrupt, timer | [x] selesai (checkpoint sebelumnya) |
| M5 | PMM, VMM, page table, kernel heap (bagian awal) | [x] selesai (checkpoint sebelumnya) |
| M6 | PMM lanjutan (frame allocator) | [x] selesai (checkpoint sebelumnya) |
| M7 | VMM core, mapping, unmapping, query | [x] selesai (checkpoint sebelumnya) |
| M8 | Kernel heap (kmem) | [x] selesai (checkpoint sebelumnya) |
| **M9** | **Thread, scheduler, context switch (cooperative)** | **[x] selesai — dibahas penuh pada laporan ini** |
| M10 | Persistent filesystem, mcsfs/ext2-like, recovery | [ ] tidak dibahas |
| M11 | Networking stack, packet parsing, UDP/TCP subset | [ ] tidak dibahas |
| M12 | Security model, capability/ACL, syscall fuzzing, hardening | [ ] tidak dibahas |
| M13 | SMP, scalability, lock stress, NUMA-aware preparation | [ ] tidak dibahas |
| M14 | Framebuffer, graphics console, visual regression | [ ] tidak dibahas |
| M15 | Virtualization/container subset | [ ] tidak dibahas |
| M16 | Observability, update/rollback, release image, readiness review | [ ] tidak dibahas |

Batas cakupan praktikum:

```text
Praktikum M9 mencakup:
- Checkpoint commit sebelum M9 (aa5d1b9) dan pembuatan branch m9-kernel-thread-scheduler
- Header scheduler: mcsos/kernel/sched.h (struct thread, struct scheduler, context, state, error code)
- Implementasi scheduler: kernel/core/sched.c
- Context switch assembly: kernel/arch/x86_64/src/context_switch.S
- Host unit test: kernel/tests/test_sched_host.c
- Target Makefile: check-m9 (build sched_combined.o + test_sched_host, jalankan test, nm -u, objdump)
- Integrasi scheduler ke kmain.c (m9_scheduler_bootstrap, thread demo A dan B)
- Perbaikan bug trampoline (thread entry tidak terpanggil pada percobaan pertama)
- QEMU smoke test A/B alternate (thread A tick / thread B tick bergantian)
- Verifikasi panic path kriteria #10: undersized stack dan corrupt TCB magic
- Full rebuild kernel (make clean && make image) menyertakan seluruh subsistem M3-M9
- Audit statis build/sched_combined.o (readelf, nm -u, objdump -dr)
- Perhitungan SHA-256 artefak (sched_combined.o, test_sched_host, kernel.elf, mcsos.iso)
- Sesi debugging GDB remote pada mcsos_context_switch dan mcsos_sched_yield
- git push branch m9-kernel-thread-scheduler ke remote GitHub

Non-goals (tidak termasuk):
- Preemptive scheduling / timer-driven context switch otomatis
- Priority scheduling atau multilevel feedback queue
- Synchronization primitives (mutex, semaphore, spinlock)
- Syscall ABI dan userspace thread
- SMP / multi-core scheduling
- Filesystem dan network stack
- Hardware bring-up fisik
```

---

## 6. Dasar Teori Ringkas

### 6.1 Konsep Sistem Operasi yang Diuji

**Boot Chain M9:** Limine (bootloader) -> `kernel.elf` (ELF64) -> `kmain()` -> inisialisasi subsistem M3–M8 (log, panic, IDT/PIC/PIT, PMM, VMM, kmem) -> `m9_scheduler_bootstrap()` -> `mcsos_scheduler_init()` -> `mcsos_thread_prepare()` (thread A, thread B) -> `mcsos_sched_enqueue()` -> `mcsos_sched_yield()` -> thread A/B berjalan bergantian secara cooperative.

**Thread Control Block (TCB) — `mcsos_thread_t`:** Struktur yang menyimpan `magic` (validasi objek), `id`, `name`, `state` (NEW/READY/RUNNING/BLOCKED), `entry` (fungsi thread), `arg`, `stack_base`, `stack_size`, `context` (register tersimpan), `next` (linked list ready queue), `switches`, `ticks`, `exit_code`.

**Scheduler — `mcsos_scheduler_t`:** Menyimpan `current` (thread yang berjalan), `idle` (thread idle boot), `ready_head`/`ready_tail` (FIFO ready queue berbasis linked list), `next_id`, `runnable_count`, `context_switches`, `ticks`, `initialized`.

**Context Switch:** Rutin assembly `mcsos_context_switch(old_context*, new_context*)` menyimpan register callee-saved (`rsp, rbp, rbx, r12, r13, r14, r15`) milik thread lama ke `old_context`, lalu memulihkan register yang sama dari `new_context`, kemudian melompat (`jmp *56(%rsi)`) ke alamat instruksi (`rip`) thread baru — atau melanjutkan eksekusi normal (`ret`) jika thread yang di-switch sebelumnya sudah pernah berjalan.

**Trampoline (`mcsos_thread_trampoline`):** Titik masuk pertama sebuah thread baru yang belum pernah dijalankan. Karena `context_switch` hanya memulihkan register dan melompat ke `rip` yang tersimpan, thread baru diarahkan pertama kali ke trampoline ini, yang selanjutnya harus memanggil fungsi entry thread yang sebenarnya (bug ini ditemukan dan diperbaiki pada commit `6ddfdd0`, lihat bagian 15).

**Cooperative Scheduling:** Thread hanya berpindah saat memanggil `mcsos_sched_yield()` secara eksplisit (bukan preemptive/timer-interrupt driven pada M9). Validasi integritas ready queue dilakukan lewat `mcsos_sched_validate()` yang menelusuri linked list dan memastikan jumlah node konsisten dengan `runnable_count`.

**Panic Path TCB (kriteria #10):** `mcsos_thread_prepare()` menolak stack yang lebih kecil dari `MCSOS_MIN_KERNEL_STACK` (mengembalikan `MCSOS_SCHED_ESTACK`), dan fungsi validasi (`valid_thread_object`) menolak TCB dengan `magic` yang tidak sama dengan `MCSOS_THREAD_MAGIC`. Kedua kondisi ini diverifikasi lewat panic demo terkendali di `kmain.c` yang memanggil `KERNEL_PANIC` saat kondisi tersebut sengaja dipicu.

### 6.2 Konsep Arsitektur x86_64 yang Relevan

| Konsep | Relevansi pada praktikum | Bukti/verifikasi |
|---|---|---|
| Callee-saved registers (System V ABI) | `rbx, rbp, r12–r15, rsp` harus disimpan/dipulihkan saat context switch | `context_switch.S`, objdump |
| Stack layout & alignment | Stack thread baru dipotong (align down) dan disiapkan agar `rsp` valid sebelum trampoline dipanggil | `mcsos_thread_prepare()` (align_down_uintptr, MCSOS_STACK_ALIGN) |
| Indirect jump (`jmp *reg`) | Melompat ke alamat `rip` yang tersimpan pada context TCB | objdump: `jmp *0x38(%rsi)` pada `mcsos_context_switch` |
| Higher-half kernel addressing | Alamat breakpoint GDB `0xffffffff8000420c` berada pada higher-half kernel | `gdb_session.log` |
| ELF64 relocatable object | `sched_combined.o` adalah hasil `ld -r` (relocatable, belum final link) | `readelf -h` menunjukkan `Type: REL` |
| PLT32 relocation | Simbol `mcsos_context_switch` direferensikan lewat `R_X86_64_PLT32` sebelum link akhir | `objdump -dr` |

### 6.3 Konsep Implementasi Freestanding

| Aspek | Keputusan praktikum |
|---|---|
| Bahasa | C17 freestanding untuk `sched.c`, GAS assembly untuk `context_switch.S` |
| Host test terpisah | `test_sched_host.c` dikompilasi dengan `-DMCSOS_HOST_TEST` memakai clang host (bukan target freestanding) sehingga scheduler dapat diuji tanpa context switch nyata (hanya validasi struktur data & flow) |
| ABI | x86_64 System V calling convention untuk parameter `old_context*` (rdi) dan `new_context*` (rsi) |
| Compiler flags kritis (build kernel) | `-ffreestanding -nostdlib -fno-builtin -fno-stack-protector -fno-stack-check -fno-pic -fno-pie -fno-lto -mno-red-zone -mno-sse -mno-sse2 -mcmodel=kernel` |
| Compiler flags assembly | `-fno-pic -fno-pie -mcmodel=kernel` (tanpa `-ffreestanding` karena file `.S` murni assembly) |
| Risiko undefined behavior | Mitigasi dengan `-Wall -Wextra -Werror`, validasi objek TCB (`valid_thread_object`) sebelum dipakai |

### 6.4 Referensi Teori yang Digunakan

| No. | Sumber | Bagian yang digunakan | Alasan relevansi |
|---|---|---|---|
| [1] | OSDev Wiki - Kernel Multitasking | Konsep context switch dan TCB | Dasar desain scheduler cooperative |
| [2] | System V AMD64 ABI | Callee-saved register convention | Menentukan register yang disimpan pada context switch |
| [3] | LLVM Project - Clang User's Manual | Freestanding builds, `-mno-red-zone` | Flag kompilasi kernel |
| [4] | LLD Documentation | `ld -r` relocatable linking | Pembuatan `sched_combined.o` |
| [5] | Intel SDM Vol. 1 | Stack alignment, calling convention | Perhitungan `align_down_uintptr` dan `MCSOS_STACK_ALIGN` |
| [6] | GNU GDB Documentation | Remote debugging (`target remote`), breakpoint pada fungsi assembly | Sesi `gdb_session.log` |

---

## 7. Lingkungan Praktikum

### 7.1 Host dan Target

| Komponen | Nilai |
|---|---|
| Host OS | Windows 11 x64 |
| Lingkungan build | WSL 2 Ubuntu 26.04 LTS |
| Kernel WSL | 6.6.87.2-microsoft-standard-WSL2 |
| Target ISA | x86_64 |
| Target ABI | x86_64-unknown-none-elf |
| Emulator | QEMU system-x86_64 |
| Debugger | GDB (remote via `target remote localhost:1234`) |
| Build system | GNU Make 4.4.1 |
| Bahasa utama | C17 freestanding |
| Assembly | GAS syntax (`.S` file terpisah, bukan inline) |

Bukti screenshot: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 1 dari PDF)

### 7.2 Versi Toolchain

Output perintah verifikasi environment (dijalankan ulang di akhir praktikum dan disimpan sebagai `evidence/m9/environment_final.log`):

```bash
{
  echo "== OS =="
  cat /etc/os-release | grep PRETTY_NAME
  uname -r
  echo
  echo "== Toolchain =="
  clang --version
  ld.lld --version
  gdb --version | head -1
  qemu-system-x86_64 --version
  echo
  echo "== Git =="
  git log --oneline -6
  git rev-parse --short HEAD
} | tee evidence/m9/environment_final.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 24 dari PDF)

Output:
```
== OS ==
PRETTY_NAME="Ubuntu 26.04 LTS"
6.6.87.2-microsoft-standard-WSL2

== Toolchain ==
Ubuntu clang version 21.1.8 (6ubuntu1)
Target: x86_64-pc-linux-gnu
Thread model: posix
InstalledDir: /usr/lib/llvm-21/bin
Ubuntu LLD 21.1.8 (compatible with GNU linkers)
GNU gdb (Ubuntu 17.1-2ubuntu1) 17.1
QEMU emulator version 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3)
Copyright (c) 2003-2025 Fabrice Bellard and the QEMU Project developers

== Git ==
18c6c35 M9: verified panic path for undersized stack and corrupt TCB magic (kriteria #10 PASS)
6ddfdd0 M9: fix trampoline to invoke thread entry; QEMU smoke test PASS (A/B alternate)
dd2d183 M9: integrate scheduler into kmain, ISO builds clean
d95b198 M9: header, sched.c, context_switch.S, host test - check-m9 PASS
aa5d1b9 checkpoint before M9 scheduler
b2ca43a M8: integrasikan kernel heap ke kmain.c dan tambah target check-m8 di Makefile
18c6c35
```

### 7.3 Lokasi Repository

| Item | Nilai |
|---|---|
| Path repository di WSL | `/home/andianaaji/src/mcsos` |
| Apakah berada di filesystem Linux WSL, bukan `/mnt/c` | Ya (verified) |
| Remote repository | `https://github.com/JotDesu/mcsos260502_.git` |
| Branch kerja | `m9-kernel-thread-scheduler` |
| Commit checkpoint | `aa5d1b9` (checkpoint before M9 scheduler) |
| Commit akhir | `18c6c35` |

Bukti screenshot: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 16 dari PDF, `git remote -v` dan `git push`)

Output push:
```
Enumerating objects: 57, done.
Counting objects: 100% (57/57), done.
...
To https://github.com/JotDesu/mcsos260502_.git
 * [new branch]      m9-kernel-thread-scheduler -> m9-kernel-thread-scheduler
branch 'm9-kernel-thread-scheduler' set up to track 'origin/m9-kernel-thread-scheduler'.
```

---

## 8. Repository dan Struktur File

### 8.1 File yang Dibuat atau Diubah pada M9

| File | Jenis perubahan | Alasan perubahan | Risiko |
|---|---|---|---|
| `kernel/include/mcsos/kernel/sched.h` | Baru | Header struct thread, scheduler, context, state, error code | Sedang — kontrak dipakai banyak fungsi |
| `kernel/core/sched.c` | Baru | Implementasi cooperative scheduler (init, enqueue, pick_next, yield, tick, block, mark_ready, ready_count, validate) | Sedang — logika linked-list ready queue |
| `kernel/arch/x86_64/src/context_switch.S` | Baru | Assembly context switch dua arah (save/restore callee-saved register + trampoline jump) | Tinggi — kesalahan register clobber dapat merusak state CPU |
| `kernel/tests/test_sched_host.c` | Baru | Host unit test (`MCSOS_HOST_TEST`) untuk scheduler tanpa QEMU | Rendah — hanya testing |
| `Makefile` | Ubah | Target `check-m9` (build sched_combined.o, test_sched_host, jalankan test, `nm -u`, `objdump`) | Sedang — build automation |
| `kernel/core/kmain.c` | Ubah | Tambah `m9_scheduler_bootstrap()`, thread demo A/B, panic demo undersized stack | Sedang — flow boot berubah |

### 8.2 Ringkasan Commit M9

Bukti screenshot: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 24 dari PDF)

```
18c6c35 M9: verified panic path for undersized stack and corrupt TCB magic (kriteria #10 PASS)
6ddfdd0 M9: fix trampoline to invoke thread entry; QEMU smoke test PASS (A/B alternate)
dd2d183 M9: integrate scheduler into kmain, ISO builds clean
d95b198 M9: header, sched.c, context_switch.S, host test - check-m9 PASS
aa5d1b9 checkpoint before M9 scheduler
```

### 8.3 Struktur Evidence M9

Bukti screenshot: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 25 dari PDF, `ls -la evidence/m9/`)

```text
evidence/m9/
├── check_m9_full.log
├── environment_final.log
├── gdb_session.log
├── panic_bad_magic.log
├── panic_bad_stack.log
├── preflight_m9.log
├── qemu_final.log
├── qemu_gdb_session.log
├── qemu_m9.log
├── qemu_m9_fix1.log
├── sched_objdump_full.txt
├── sched_objdump_key.log
├── sched_readelf_header.log
├── sched_undefined.log
└── sha256_artifacts.log
```

---

## 9. Desain Teknis

### 9.1 Masalah yang Diselesaikan

Kernel M9 belum memiliki:
- Struktur data thread (TCB) dan scheduler
- Mekanisme context switch antar thread
- Cara menjalankan dua atau lebih alur eksekusi bergantian di dalam satu kernel
- Panic path untuk kondisi TCB tidak valid (stack terlalu kecil, magic number rusak)
- Automated test untuk memverifikasi scheduler tanpa perlu boot penuh di QEMU

### 9.2 Keputusan Desain

| Keputusan | Alternatif yang dipertimbangkan | Alasan memilih | Konsekuensi |
|---|---|---|---|
| Cooperative scheduling (yield eksplisit) | Preemptive (timer interrupt-driven) | Lebih sederhana untuk milestone dasar, tidak perlu sinkronisasi dengan IRQ timer dulu | Thread harus memanggil `sched_yield()` sendiri; tidak ada fairness otomatis |
| Ready queue FIFO linked list (`ready_head`/`ready_tail`) | Array/bitmap ready queue | Ukuran dinamis, mudah divalidasi dengan `sched_validate()` | O(n) validasi, namun cukup untuk skala kecil |
| Context switch di assembly terpisah (`.S`), bukan inline asm | Inline `__asm__ volatile` di `sched.c` | Lebih mudah dikontrol untuk operasi stack-sensitive (mengubah `rsp` di tengah fungsi C berisiko) | Perlu linking terpisah (`ld -r`) sebelum digabung ke kernel akhir |
| Trampoline terpisah (`mcsos_thread_trampoline`) | Langsung menaruh alamat fungsi entry di `context.rip` | Memberi titik kontrol tunggal sebelum lompat ke entry asli (tempat memasang guard/idle loop jika entry return) | Butuh perbaikan tambahan agar trampoline benar-benar memanggil entry (lihat bagian 15) |
| Validasi TCB lewat `magic` number (`MCSOS_THREAD_MAGIC`) | Tidak ada validasi, percaya pointer valid | Mendeteksi corrupt TCB / pointer sembarangan sebelum dipakai scheduler | Overhead pemeriksaan kecil di setiap operasi scheduler |
| Minimum kernel stack (`MCSOS_MIN_KERNEL_STACK`) dengan panic jika kurang | Terima ukuran stack berapapun | Mencegah stack overflow senyap saat thread berjalan | `mcsos_thread_prepare()` mengembalikan `MCSOS_SCHED_ESTACK` bila gagal |
| Host unit test terpisah (`MCSOS_HOST_TEST`) | Hanya uji lewat QEMU serial log | Testing scheduler jauh lebih cepat dan deterministik tanpa boot penuh | Context switch nyata (assembly) tidak dieksekusi saat host test (di-`#ifdef` off) |

### 9.3 Arsitektur Ringkas

```
   kmain()
     |
     v
   m9_scheduler_bootstrap()
     |
     +--> mcsos_scheduler_init(&g_sched, &g_boot_thread)
     |
     +--> mcsos_thread_prepare(&g_thread_a, "demo-a", m9_demo_thread_a, stack_a, ...)
     +--> mcsos_thread_prepare(&g_thread_b, "demo-b", m9_demo_thread_b, stack_b, ...)
     |
     +--> mcsos_sched_enqueue(&g_sched, &g_thread_a)
     +--> mcsos_sched_enqueue(&g_sched, &g_thread_b)
     |
     +--> mcsos_sched_validate(&g_sched)     -> pastikan ready queue konsisten
     |
     +--> log_writeln("[MCSOS:M9] scheduler initialized")
     |
     v
   mcsos_sched_yield(&g_sched)
     |
     +--> old_thread = sched->current
     +--> next_thread = mcsos_sched_pick_next(sched)
     +--> if next_thread != old_thread:
     |        old_thread->state = READY; enqueue(old_thread)
     |        next_thread->state = RUNNING; sched->current = next_thread
     |        mcsos_context_switch(&old_thread->context, &next_thread->context)
     |
     v
   [thread A tick] <---yield---> [thread B tick]   (berulang, dicatat di serial log)
```

### 9.4 Kontrak Antarmuka

| Antarmuka | Pemanggil | Penerima | Precondition | Postcondition | Error path |
|---|---|---|---|---|---|
| `mcsos_scheduler_init(sched, boot_thread)` | `m9_scheduler_bootstrap()` | `sched.c` | `sched`, `boot_thread` tidak NULL | `sched` terinisialisasi, `boot_thread` jadi `current` | `MCSOS_SCHED_EINVAL` jika NULL |
| `mcsos_thread_prepare(thread, name, entry, arg, stack_base, stack_size, id)` | `m9_scheduler_bootstrap()` | `sched.c` | `stack_size >= MCSOS_MIN_KERNEL_STACK` | Context TCB siap (`rsp` di-align, `rip` = trampoline) | `MCSOS_SCHED_ESTACK` jika stack kurang, `MCSOS_SCHED_EINVAL` jika parameter NULL |
| `mcsos_sched_enqueue(sched, thread)` | bootstrap, `mark_ready` | `sched.c` | Thread valid (`valid_thread_object`), state NEW/READY/BLOCKED | Thread masuk ready queue, state = READY | `MCSOS_SCHED_ESTATE` jika state tidak sesuai |
| `mcsos_sched_pick_next(sched)` | `mcsos_sched_yield()` | `sched.c` | Scheduler terinisialisasi | Mengembalikan head ready queue atau `idle` jika kosong | Mengembalikan NULL jika sched invalid |
| `mcsos_sched_yield(sched)` | thread demo, `kmain` | `sched.c`, `context_switch.S` | `sched->current` valid | Context switch terjadi bila ada thread lain runnable | `MCSOS_SCHED_ECORRUPT` jika next thread invalid |
| `mcsos_sched_tick(sched)` | (dipanggil manual pada M9, belum via IRQ) | `sched.c` | Scheduler valid | `ticks` dan `current->ticks` bertambah | `MCSOS_SCHED_EINVAL` |
| `mcsos_thread_block_current(sched)` | (disediakan, belum dipakai demo M9) | `sched.c` | current bukan idle thread | State = BLOCKED, yield dipanggil | `MCSOS_SCHED_ESTATE` jika current == idle |
| `mcsos_thread_mark_ready(sched, thread)` | (disediakan untuk wake-up) | `sched.c` | Thread state = BLOCKED | Thread di-enqueue kembali | `MCSOS_SCHED_ESTATE` jika state bukan BLOCKED |
| `mcsos_sched_validate(sched)` | bootstrap, test host | `sched.c` | - | Menelusuri ready queue, cek jumlah node = `runnable_count` | `MCSOS_SCHED_ECORRUPT` jika linked list rusak |
| `mcsos_context_switch(old_ctx, new_ctx)` | `mcsos_sched_yield()` | `context_switch.S` | `old_ctx`, `new_ctx` valid pointer | Register callee-saved lama tersimpan, register baru dipulihkan, eksekusi lanjut di thread baru | Tidak ada validasi runtime (murni assembly, tanggung jawab caller) |

### 9.5 Struktur Data Utama

| Struktur data | Field penting | Ownership | Lifetime | Invariant |
|---|---|---|---|---|
| `mcsos_thread_context_t` | `rsp, rbp, rbx, r12, r13, r14, r15, rip` | Dimiliki tiap TCB | Selama thread hidup | Field diakses via offset tetap oleh `context_switch.S` (0,8,16,24,32,40,48,56) |
| `mcsos_thread_t` | `magic, id, name, state, entry, arg, stack_base, stack_size, context, next, switches, ticks, exit_code` | Statik (demo A/B, boot thread, idle) | Selama kernel hidup pada M9 | `magic == MCSOS_THREAD_MAGIC` bila valid |
| `mcsos_scheduler_t` | `current, idle, ready_head, ready_tail, next_id, runnable_count, context_switches, ticks, initialized` | Statik global (`g_sched`) | Selama kernel hidup | `runnable_count` selalu sama dengan jumlah node pada linked list `ready_head..ready_tail` |

### 9.6 Invariants

1. Setiap TCB yang dipakai scheduler harus memiliki `magic == MCSOS_THREAD_MAGIC` (diverifikasi `valid_thread_object`)
2. `stack_size` thread baru harus `>= MCSOS_MIN_KERNEL_STACK`, jika tidak `mcsos_thread_prepare` menolak dengan `MCSOS_SCHED_ESTACK`
3. `sched->ready_head`/`ready_tail` selalu konsisten dengan `runnable_count` (diverifikasi `mcsos_sched_validate`)
4. `context.rsp` untuk thread baru selalu di-align sesuai `MCSOS_STACK_ALIGN` sebelum dipakai
5. Context switch tidak pernah dipanggil dengan `old_thread == next_thread` yang sama tanpa short-circuit (`mcsos_sched_yield` mengecek kesamaan pointer terlebih dulu)
6. `sched->context_switches` bertambah setiap kali context switch benar-benar terjadi (diverifikasi host test: `context_switches == 3u`)
7. Kernel tidak kembali dari `kmain()`; setelah scheduler jalan, eksekusi berpindah selamanya antar thread A/B/idle

### 9.7 Ownership, Locking, dan Concurrency

M9 masih single-core, cooperative, tanpa interrupt-driven preemption pada scheduler:
- Tidak ada lock karena hanya satu thread berjalan pada satu waktu (tidak ada true parallelism, hanya interleaving lewat yield eksplisit)
- Ready queue dimodifikasi hanya oleh thread yang sedang `RUNNING` (tidak ada race condition antar-CPU karena single core)
- Context switch sendiri adalah titik kritis: register CPU tidak boleh diinterupsi di tengah operasi save/restore (pada M9 hal ini belum eksplisit di-`cli`, dicatat sebagai risiko pada bagian 17)

### 9.8 Memory Safety dan Undefined Behavior Risk

| Risiko | Lokasi | Mitigasi | Bukti |
|---|---|---|---|
| Corrupt TCB magic dipakai scheduler | `mcsos_sched_enqueue`, `mcsos_sched_yield` | `valid_thread_object()` mengecek `thread != NULL && magic == MCSOS_THREAD_MAGIC` | `panic_bad_magic.log` |
| Stack terlalu kecil menyebabkan overflow | `mcsos_thread_prepare` | Cek `stack_size < MCSOS_MIN_KERNEL_STACK` -> return `MCSOS_SCHED_ESTACK` | `panic_bad_stack.log` |
| Ready queue rusak (linked list loop/putus) | `mcsos_sched_validate` | Traversal dengan batas `count > runnable_count + 1u` -> `MCSOS_SCHED_ECORRUPT` | Source `sched.c` |
| Context switch register clobber salah | `context_switch.S` | Assembly eksplisit menyimpan seluruh callee-saved register sebelum memodifikasi `rsp` | `objdump` bukti urutan `mov` |
| Trampoline tidak memanggil entry thread | `mcsos_thread_trampoline` | Diperbaiki pada commit `6ddfdd0` (lihat bagian 15) | `qemu_m9_fix1.log` |

### 9.9 Security Boundary

| Boundary | Data tidak tepercaya | Validasi yang dilakukan | Failure mode aman |
|---|---|---|---|
| TCB dari caller (bootstrap/demo) | Pointer thread, magic number | `valid_thread_object` | Return error code, tidak crash |
| Ukuran stack dari caller | `stack_size` | Perbandingan dengan `MCSOS_MIN_KERNEL_STACK` | Return `MCSOS_SCHED_ESTACK`, panic terkendali jika dipanggil lewat demo panic |
| Ready queue internal | - | `mcsos_sched_validate` | Panic/`ECORRUPT` (fail-closed) |

---

## 10. Langkah Kerja Implementasi

### Langkah 1 — Preflight M9, Checkpoint, dan Pembuatan Branch

Maksud langkah: Memverifikasi environment (git, toolchain) dan artefak milestone sebelumnya (M3–M8) sebelum mulai bekerja pada scheduler, lalu membuat commit checkpoint dan branch kerja baru.

Perintah:
```bash
cd ~/src/mcsos
mkdir -p evidence/m9
{
  echo "== git =="
  git rev-parse --show-toplevel
  git rev-parse --short HEAD
  git status --short
  echo
  echo "== tools =="
  clang --version || true
  gcc --version | head -n 1 || true
  ld.lld --version || true
  ld --version | head -n 1 || true
  make --version | head -n 1 || true
  qemu-system-x86_64 --version || true
  gdb --version | head -n 1 || true
  echo
  echo "== previous artifacts =="
  find build evidence -maxdepth 3 -type f 2>/dev/null | sort | grep -E 'M[0-8]|m[0-8]|kernel|iso|map|o$' || true
} | tee evidence/m9/preflight_m9.log
git add .
git commit -m "checkpoint before M9 scheduler" || true
git switch -c m9-kernel-thread-scheduler
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 1 dari PDF)

Output ringkas:
```
== git ==
/home/andianaaji/src/mcsos
b2ca43a
?? evidence/m9/

== tools ==
Ubuntu clang version 21.1.8 (6ubuntu1)
Ubuntu LLD 21.1.8 (compatible with GNU linkers)
GNU Make 4.4.1
QEMU emulator version 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3)
GNU gdb (Ubuntu 17.1-2ubuntu1) 17.1

== previous artifacts ==
build/kernel.disasm.txt ... build/mcsos.iso ... evidence/M3/... evidence/M4/... evidence/M7/...

[m9-kernel-thread-scheduler d95b198] ... (branch dibuat setelah commit checkpoint)
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| preflight_m9.log | evidence/m9/preflight_m9.log | Bukti environment dan artefak sebelumnya sebelum M9 |
| Commit checkpoint | `aa5d1b9` | Titik kembali aman sebelum mulai M9 |
| Branch kerja | `m9-kernel-thread-scheduler` | Isolasi perubahan M9 dari `main` |

Indikator berhasil: Semua tool terdeteksi, artefak M3–M8 masih ada, checkpoint commit dan branch baru berhasil dibuat.

Catatan: Pada bagian eksplorasi lokal (di luar repository) sempat terlihat direktori `Documents/APLIKASI` pada Windows Explorer berisi arsip aplikasi pihak ketiga. Direktori ini **tidak berkaitan** dengan pekerjaan praktikum M9 dan tidak digunakan sebagai bagian dari toolchain MCSOS.

### Langkah 2 — Membuat Header dan Implementasi Scheduler (`sched.c`)

Maksud langkah: Menulis seluruh struktur data dan fungsi scheduler cooperative dalam satu file `kernel/core/sched.c`, memakai header `kernel/include/mcsos/kernel/sched.h` yang berisi definisi `mcsos_thread_t`, `mcsos_scheduler_t`, `mcsos_thread_context_t`, enum state, dan kode error.

Perintah (heredoc pembuatan file, dipotong menjadi cuplikan penting):

```bash
cat > kernel/core/sched.c << 'EOF'
#include "mcsos/kernel/sched.h"
#include <stdint.h>

static uintptr_t align_down_uintptr(uintptr_t value, uintptr_t alignment) {
    return value & -(alignment - 1u);
}

static int valid_thread_object(const mcsos_thread_t *thread) {
    return thread != (const mcsos_thread_t *)0 && thread->magic == MCSOS_THREAD_MAGIC;
}

static void zero_context(mcsos_thread_context_t *context) {
    context->rsp = 0; context->rbp = 0; context->rbx = 0;
    context->r12 = 0; context->r13 = 0; context->r14 = 0; context->r15 = 0;
    context->rip = 0;
}

void mcsos_thread_trampoline(void) {
    for (;;) {
#if defined(__x86_64__)
        __asm__ volatile("hlt");
#else
        __builtin_trap();
#endif
    }
}

int mcsos_scheduler_init(mcsos_scheduler_t *sched, mcsos_thread_t *boot_thread) { ... }

int mcsos_thread_prepare(mcsos_thread_t *thread, const char *name,
                          mcsos_thread_entry_t entry, void *arg,
                          void *stack_base, size_t stack_size, uint64_t id) {
    if (stack_size < MCSOS_MIN_KERNEL_STACK) {
        return MCSOS_SCHED_ESTACK;
    }
    uintptr_t low  = (uintptr_t)stack_base;
    uintptr_t high = low + (uintptr_t)stack_size;
    if (high <= low) { return MCSOS_SCHED_ESTACK; }
    uintptr_t top = align_down_uintptr(high, MCSOS_STACK_ALIGN);
    if (top <= low + 128u) { return MCSOS_SCHED_ESTACK; }
    top -= sizeof(uint64_t);
    *((uint64_t *)top) = UINT64_C(0);

    thread->magic = MCSOS_THREAD_MAGIC;
    thread->id = id;
    thread->name = name;
    thread->state = MCSOS_THREAD_NEW;
    zero_context(&thread->context);
    thread->context.rsp = (uint64_t)top;
    thread->context.rip = (uint64_t)(uintptr_t)mcsos_thread_trampoline;
    thread->entry = entry;
    thread->arg = arg;
    thread->stack_base = (uint8_t *)stack_base;
    thread->stack_size = stack_size;
    thread->next = (mcsos_thread_t *)0;
    thread->switches = 0; thread->ticks = 0; thread->exit_code = 0;
    return MCSOS_SCHED_OK;
}

int mcsos_sched_enqueue(mcsos_scheduler_t *sched, mcsos_thread_t *thread) { ... }
mcsos_thread_t *mcsos_sched_pick_next(mcsos_scheduler_t *sched) { ... }
int mcsos_sched_yield(mcsos_scheduler_t *sched) {
    ...
    old_thread->state = MCSOS_THREAD_READY;
    int rc = mcsos_sched_enqueue(sched, old_thread);
    ...
    next_thread->state = MCSOS_THREAD_RUNNING;
    sched->current = next_thread;
    old_thread->switches++; next_thread->switches++; sched->context_switches++;
#if !defined(MCSOS_HOST_TEST)
    mcsos_context_switch(&old_thread->context, &next_thread->context);
#endif
    return MCSOS_SCHED_OK;
}

int mcsos_sched_tick(mcsos_scheduler_t *sched) { ... }
int mcsos_thread_block_current(mcsos_scheduler_t *sched) { ... }
int mcsos_thread_mark_ready(mcsos_scheduler_t *sched, mcsos_thread_t *thread) { ... }
size_t mcsos_sched_ready_count(const mcsos_scheduler_t *sched) { ... }
int mcsos_sched_validate(const mcsos_scheduler_t *sched) {
    ...
    while (cursor != (const mcsos_thread_t *)0) {
        if (!valid_thread_object(cursor) || cursor->state != MCSOS_THREAD_READY) {
            return MCSOS_SCHED_ECORRUPT;
        }
        if (cursor == sched->current) { return MCSOS_SCHED_ECORRUPT; }
        last = cursor; cursor = cursor->next; count++;
        if (count > sched->runnable_count + 1u) { return MCSOS_SCHED_ECORRUPT; }
    }
    if (last != sched->ready_tail) { return MCSOS_SCHED_ECORRUPT; }
    if (count != (size_t)sched->runnable_count) { return MCSOS_SCHED_ECORRUPT; }
    return MCSOS_SCHED_OK;
}
EOF
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 3–6 dari PDF)

Verifikasi syntax:
```bash
clang -std=c17 -Wall -Wextra -Werror -DMCSOS_HOST_TEST -Ikernel/include -fsyntax-only kernel/core/sched.c
echo "exit code: $?"
```

Output: `exit code: 0`

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| sched.c | kernel/core/sched.c | Implementasi cooperative scheduler |
| sched.h (diasumsikan sudah dibuat mendahului sched.c) | kernel/include/mcsos/kernel/sched.h | Kontrak struct dan enum scheduler |

Indikator berhasil: `-fsyntax-only` lulus dengan `-Wall -Wextra -Werror` tanpa error.

### Langkah 3 — Membuat Context Switch Assembly (`context_switch.S`)

Maksud langkah: Menulis rutin context switch tingkat rendah yang menyimpan register callee-saved milik thread lama dan memulihkan register thread baru.

Perintah:
```bash
cat > kernel/arch/x86_64/src/context_switch.S << 'EOF'
    .section .text
    .globl mcsos_context_switch
    .type mcsos_context_switch, @function
mcsos_context_switch:
    leaq 1f(%rip), %rax
    movq %rsp, 0(%rdi)
    movq %rbp, 8(%rdi)
    movq %rbx, 16(%rdi)
    movq %r12, 24(%rdi)
    movq %r13, 32(%rdi)
    movq %r14, 40(%rdi)
    movq %r15, 48(%rdi)
    movq %rax, 56(%rdi)

    movq 0(%rsi), %rsp
    movq 8(%rsi), %rbp
    movq 16(%rsi), %rbx
    movq 24(%rsi), %r12
    movq 32(%rsi), %r13
    movq 40(%rsi), %r14
    movq 48(%rsi), %r15
    jmp *56(%rsi)
1:
    ret
    .size mcsos_context_switch, . - mcsos_context_switch
EOF
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 7 dari PDF)

Verifikasi build objek dan disassembly:
```bash
mkdir -p build/m9
clang --target=x86_64-unknown-none-elf -m64 -march=x86-64 -fno-pic -fno-pie \
  -mcmodel=kernel -c kernel/arch/x86_64/src/context_switch.S -o build/m9/context_switch.o
objdump -d build/m9/context_switch.o | grep -A2 "mcsos_context_switch"
```

Output:
```
0000000000000000 <mcsos_context_switch>:
   0: 48 8d 05 3d 00 00 00   lea    0x3d(%rip),%rax    # 44 <mcsos_context_switch+0x44>
   7: 48 89 27               mov    %rsp,(%rdi)
   a: 48 89 6f 08            mov    %rbp,0x8(%rdi)
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| context_switch.S | kernel/arch/x86_64/src/context_switch.S | Rutin context switch dua arah |
| context_switch.o | build/m9/context_switch.o | Bukti kompilasi assembly berhasil |

Indikator berhasil: Objek terbentuk, disassembly menunjukkan urutan `mov` menyimpan seluruh register sesuai desain offset (`0,8,16,24,32,40,48,56`).

### Langkah 4 — Membuat Host Unit Test (`test_sched_host.c`)

Maksud langkah: Menulis pengujian scheduler yang berjalan di host (bukan di QEMU) untuk memverifikasi logika enqueue, pick_next, yield, tick, dan validate tanpa memerlukan context switch nyata.

Perintah:
```bash
mkdir -p kernel/tests
cat > kernel/tests/test_sched_host.c << 'EOF'
#include <stdio.h>
#include <stdint.h>
#include "mcsos/kernel/sched.h"

static void noop(void *arg) { (void)arg; }

#define REQUIRE(expr) do { if (!(expr)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } \
} while (0)

int main(void) {
    mcsos_scheduler_t sched;
    mcsos_thread_t boot;
    mcsos_thread_t a;
    mcsos_thread_t b;
    unsigned char stack_a[8192];
    unsigned char stack_b[8192];

    REQUIRE(mcsos_scheduler_init(&sched, &boot) == MCSOS_SCHED_OK);
    REQUIRE(mcsos_sched_validate(&sched) == MCSOS_SCHED_OK);
    REQUIRE(mcsos_thread_prepare(&a, "a", noop, NULL, stack_a, sizeof(stack_a), sched.next_id++) == MCSOS_SCHED_OK);
    REQUIRE(mcsos_thread_prepare(&b, "b", noop, NULL, stack_b, sizeof(stack_b), sched.next_id++) == MCSOS_SCHED_OK);
    REQUIRE((a.context.rsp & 0xfu) == 8u);
    REQUIRE(mcsos_sched_enqueue(&sched, &a) == MCSOS_SCHED_OK);
    REQUIRE(mcsos_sched_enqueue(&sched, &b) == MCSOS_SCHED_OK);
    REQUIRE(mcsos_sched_ready_count(&sched) == 2u);
    REQUIRE(mcsos_sched_validate(&sched) == MCSOS_SCHED_OK);
    REQUIRE(mcsos_sched_yield(&sched) == MCSOS_SCHED_OK);
    REQUIRE(sched.current == &a);
    REQUIRE(a.state == MCSOS_THREAD_RUNNING);
    REQUIRE(mcsos_sched_ready_count(&sched) == 1u);
    REQUIRE(mcsos_sched_tick(&sched) == MCSOS_SCHED_OK);
    REQUIRE(a.ticks == 1u);
    REQUIRE(mcsos_sched_yield(&sched) == MCSOS_SCHED_OK);
    REQUIRE(sched.current == &b);
    REQUIRE(mcsos_sched_yield(&sched) == MCSOS_SCHED_OK);
    REQUIRE(sched.current == &a);
    REQUIRE(sched.context_switches == 3u);
    REQUIRE(mcsos_sched_validate(&sched) == MCSOS_SCHED_OK);
    puts("M9 scheduler host unit test PASS");
    return 0;
}
EOF
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 9 dari PDF)

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| test_sched_host.c | kernel/tests/test_sched_host.c | Host unit test scheduler cooperative |

Indikator berhasil: File tersimpan tanpa error syntax; hasil eksekusi dibahas pada langkah berikutnya.

### Langkah 5 — Menambahkan Target `check-m9` pada Makefile dan Menjalankannya

Maksud langkah: Mengotomasikan build `sched_combined.o`, build & jalankan `test_sched_host`, cek tidak ada undefined symbol, dan cek disassembly memuat simbol `mcsos_context_switch`.

Perintah:
```bash
cat >> Makefile << 'EOF'

.PHONY: check-m9

check-m9: $(BUILD_DIR)/sched_combined.o $(BUILD_DIR)/test_sched_host
	./$(BUILD_DIR)/test_sched_host | tee $(BUILD_DIR)/test_sched.log
	grep -q 'PASS' $(BUILD_DIR)/test_sched.log
	$(NM) -u $(BUILD_DIR)/sched_combined.o | tee $(BUILD_DIR)/sched.undefined.txt
	test ! -s $(BUILD_DIR)/sched.undefined.txt
	$(OBJDUMP) -dr $(BUILD_DIR)/sched_combined.o > $(BUILD_DIR)/sched.objdump.txt
	grep -q 'mcsos_context_switch' $(BUILD_DIR)/sched.objdump.txt
	@echo "[PASS] M9 static check selesai"

$(BUILD_DIR)/sched.o: kernel/core/sched.c kernel/include/mcsos/kernel/sched.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/core/sched.c -o $(BUILD_DIR)/sched.o

$(BUILD_DIR)/context_switch.o: kernel/arch/x86_64/src/context_switch.S
	mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c kernel/arch/x86_64/src/context_switch.S -o $(BUILD_DIR)/context_switch.o

$(BUILD_DIR)/sched_combined.o: $(BUILD_DIR)/sched.o $(BUILD_DIR)/context_switch.o
	$(LD) -r -o $(BUILD_DIR)/sched_combined.o $(BUILD_DIR)/sched.o $(BUILD_DIR)/context_switch.o

$(BUILD_DIR)/test_sched_host: kernel/core/sched.c kernel/tests/test_sched_host.c kernel/include/mcsos/kernel/sched.h
	mkdir -p $(BUILD_DIR)
	$(HOSTCC) -std=c17 -Wall -Wextra -Werror -Ikernel/include -DMCSOS_HOST_TEST \
	  kernel/core/sched.c kernel/tests/test_sched_host.c -o $(BUILD_DIR)/test_sched_host
EOF
make check-m9
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 10 dari PDF)

Output:
```
clang --target=x86_64-unknown-none-elf -std=c17 -ffreestanding ... -c kernel/core/sched.c -o build/sched.o
clang --target=x86_64-unknown-none-elf -m64 -march=x86-64 -fno-pic -fno-pie -mcmodel=kernel -c kernel/arch/x86_64/src/context_switch.S -o build/context_switch.o
ld.lld -r -o build/sched_combined.o build/sched.o build/context_switch.o
clang -std=c17 -Wall -Wextra -Werror -Ikernel/include -DMCSOS_HOST_TEST \
  kernel/core/sched.c kernel/tests/test_sched_host.c -o build/test_sched_host
./build/test_sched_host | tee build/test_sched.log
M9 scheduler host unit test PASS
grep -q 'PASS' build/test_sched.log
nm -u build/sched_combined.o | tee build/sched.undefined.txt
test ! -s build/sched.undefined.txt
objdump -dr build/sched_combined.o > build/sched.objdump.txt
grep -q 'mcsos_context_switch' build/sched.objdump.txt
[PASS] M9 static check selesai
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| build/sched_combined.o | build/sched_combined.o | Objek gabungan sched.c + context_switch.S |
| build/test_sched_host | build/test_sched_host | Binary host test |
| build/test_sched.log | build/test_sched.log | Log hasil host test |

Indikator berhasil: `M9 scheduler host unit test PASS`, tidak ada undefined symbol, `[PASS] M9 static check selesai`.

Commit setelah langkah ini:
```bash
git add -A
git commit -m "M9: header, sched.c, context_switch.S, host test - check-m9 PASS"
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 11 dari PDF)

### Langkah 6 — Integrasi Scheduler ke `kmain.c`

Maksud langkah: Memanggil scheduler dari boot sequence kernel, menyiapkan dua thread demo (A dan B), dan memvalidasi ready queue sebelum menjalankan yield pertama.

Perintah (cuplikan `kmain.c`, fungsi baru):
```bash
cat -n kernel/core/kmain.c
```

Cuplikan kode:
```c
static void m9_scheduler_bootstrap(void) {
    int rc = mcsos_scheduler_init(&g_sched, &g_boot_thread);
    if (rc != MCSOS_SCHED_OK) {
        KERNEL_PANIC("M9: mcsos_scheduler_init failed", (uint64_t)rc);
    }

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
    if (rc != MCSOS_SCHED_OK) { KERNEL_PANIC("M9: enqueue thread a failed", (uint64_t)rc); }

    rc = mcsos_sched_enqueue(&g_sched, &g_thread_b);
    if (rc != MCSOS_SCHED_OK) { KERNEL_PANIC("M9: enqueue thread b failed", (uint64_t)rc); }

    if (mcsos_sched_validate(&g_sched) != MCSOS_SCHED_OK) {
        KERNEL_PANIC("M9: sched_validate failed after setup", 0);
    }

    log_writeln("[MCSOS:M9] scheduler initialized");
    mcsos_sched_yield(&g_sched);
}

void kmain(void) {
    cpu_cli();
    log_init();
    log_write(MCSOS_NAME);
    log_write(" ");
    log_write(MCSOS_VERSION);
    log_write(" ");
    ...
}
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 11–12 dari PDF)

Setelah integrasi, build ISO dan jalankan QEMU:
```bash
make clean && make image
qemu-system-x86_64 -m 256M -machine q35 -serial file:evidence/m9/qemu_m9_fix1.log \
  -display none -no-reboot -no-shutdown -cdrom build/mcsos.iso
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| kmain.c (diubah) | kernel/core/kmain.c | Integrasi scheduler + thread demo A/B |
| mcsos.iso | build/mcsos.iso | Image bootable dengan M9 aktif |

Commit setelah langkah ini:
```
dd2d183 M9: integrate scheduler into kmain, ISO builds clean
```

### Langkah 7 — Perbaikan Bug Trampoline dan QEMU Smoke Test A/B Alternate

Maksud langkah: Memperbaiki bug pada `mcsos_thread_trampoline` yang pada percobaan pertama hanya melakukan `hlt` tanpa memanggil entry point thread, sehingga thread A/B tidak benar-benar mengeksekusi kode demo-nya. Setelah diperbaiki, log serial menunjukkan thread A dan B benar-benar bertukar giliran ("tick").

Bukti screenshot log QEMU: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 13 dari PDF, `cat evidence/m9/qemu_m9_fix1.log`)

Cuplikan log (`evidence/m9/qemu_m9_fix1.log`):
```
limine: Loading executable `boot():/boot/kernel.elf`...
MCSOS 260502 M3 kernel entered
[MCSOS:M5] boot: external interrupt bring-up start
[MCSOS:M5] idt: loaded
[MCSOS:M5] pic: remapped
[MCSOS:M5] pic: irq0 unmasked
[MCSOS:M5] pit: configured 100Hz
[MCSOS:M5] sti: interrupts enabled
[MCSOS:M6] pmm initialized
[MCSOS:M6] frames managed=0x0000000000008000
[MCSOS:M6] frames free=0x0000000000007fff
[MCSOS:M6] sample alloc/free OK
[MCSOS:M7] VMM core initialized
[MCSOS:M7] root_paddr=0x0000000000001000
[MCSOS:M7] demo map vaddr=0x0000000000600000
[MCSOS:M7] demo map paddr=0x0000000000002000
[MCSOS:M7] demo map/query/unmap OK
[MCSOS:M7] ready for QEMU smoke test
[MCSOS:M8] kmem initialized
[MCSOS:M8] heap total_bytes=0x0000000000010000
[MCSOS:M8] heap free_bytes=0x000000000000ffd0
[MCSOS:M8] heap largest_free=0x000000000000ffd0
[MCSOS:M8] heap block_count=0x0000000000000001
[MCSOS:M9] scheduler initialized
[MCSOS:M9] thread A tick
[MCSOS:M9] thread B tick
[MCSOS:M9] thread A tick
[MCSOS:M9] thread B tick
[MCSOS:M9] thread A tick
[MCSOS:M9] thread B tick
[MCSOS:M9] thread A tick
[MCSOS:M9] thread B tick
[MCSOS:M9] thread A tick
[MCSOS:M9] thread B tick
[MCSOS:TIMER] ticks=0x0000000000000064
[MCSOS:TIMER] ticks=0x00000000000000c8
[MCSOS:TIMER] ticks=0x000000000000012c
...
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| qemu_m9_fix1.log | evidence/m9/qemu_m9_fix1.log | Bukti boot chain penuh M3–M9 dan alternating thread A/B |

Indikator berhasil: Boot chain M3 s.d. M9 lengkap tanpa error, thread A dan B berganti giliran secara konsisten (5 pasang tick sebelum kernel lanjut ke observasi timer).

Commit setelah langkah ini:
```
6ddfdd0 M9: fix trampoline to invoke thread entry; QEMU smoke test PASS (A/B alternate)
```

### Langkah 8 — Verifikasi Panic Path Kriteria #10 (Undersized Stack & Corrupt TCB Magic)

Maksud langkah: Membuktikan bahwa scheduler menolak stack yang terlalu kecil dan TCB dengan magic number tidak valid, dengan memicu panic terkendali dari `kmain.c` sebagai demo.

Perintah:
```bash
qemu-system-x86_64 \
  -m 256M -machine q35 \
  -serial file:evidence/m9/panic_bad_stack.log \
  -display none -no-reboot -no-shutdown \
  -cdrom build/mcsos.iso
cat evidence/m9/panic_bad_stack.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 14 dari PDF)

Output (`panic_bad_stack.log`):
```
limine: Loading executable `boot():/boot/kernel.elf`...
MCSOS 260502 M3 kernel entered
[MCSOS:M5] ... [MCSOS:M6] ... [MCSOS:M7] ... [MCSOS:M8] heap block_count=0x0000000000000001
[MCSOS:M9] triggering controlled bad-stack panic test

================= MCSOS KERNEL PANIC =================
system=MCSOS version=260502 milestone=M3
reason=M9: demo undersized stack correctly rejected
location=kernel/core/kmain.c:199
panic_code=0xfffffffffffffffd
rflags_before_cli=0x0000000000000282
state=halted
=======================================================
```

Pengujian serupa juga dilakukan untuk kondisi corrupt TCB magic, disimpan sebagai `evidence/m9/panic_bad_magic.log` (skenario: sebuah TCB dengan field `magic` diubah menjadi nilai bukan `MCSOS_THREAD_MAGIC` sebelum dipakai `mcsos_sched_enqueue`, yang kemudian ditolak scheduler dan memicu `KERNEL_PANIC`).

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| panic_bad_stack.log | evidence/m9/panic_bad_stack.log | Bukti panic path untuk stack terlalu kecil |
| panic_bad_magic.log | evidence/m9/panic_bad_magic.log | Bukti panic path untuk TCB magic tidak valid |

Indikator berhasil: `panic_code=0xfffffffffffffffd` (representasi `MCSOS_SCHED_ESTACK` sebagai nilai negatif 64-bit), lokasi panic tercatat tepat di `kmain.c:199`, kernel berhenti dengan aman (`state=halted`).

Commit setelah langkah ini:
```bash
git add -A
git commit -m "M9: verified panic path for undersized stack and corrupt TCB magic (kriteria #10 PASS)"
git push -u origin m9-kernel-thread-scheduler
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 16 dari PDF)

### Langkah 9 — Full Rebuild dari Clean State dan Regenerasi ISO

Maksud langkah: Memastikan seluruh milestone (M3–M9) dapat dibangun ulang dari kondisi bersih (`make clean`) tanpa artefak lama yang tersisa.

Perintah:
```bash
make clean
make image
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 18–19 dari PDF)

Output ringkas (seluruh objek dikompilasi ulang):
```
rm -rf build
mkdir -p build/normal/kernel/arch/x86_64/src/
clang ... -c kernel/arch/x86_64/src/idt.c -o build/normal/kernel/arch/x86_64/src/idt.o
clang ... -c kernel/arch/x86_64/src/pic.c -o build/normal/kernel/arch/x86_64/src/pic.o
clang ... -c kernel/arch/x86_64/src/pit.c -o build/normal/kernel/arch/x86_64/src/pit.o
clang ... -c kernel/core/kmain.c   -o build/normal/kernel/core/kmain.o
clang ... -c kernel/core/log.c    -o build/normal/kernel/core/log.o
clang ... -c kernel/core/panic.c  -o build/normal/kernel/core/panic.o
clang ... -c kernel/core/pmm.c    -o build/normal/kernel/core/pmm.o
clang ... -c kernel/core/sched.c  -o build/normal/kernel/core/sched.o
clang ... -c kernel/core/serial.c -o build/normal/kernel/core/serial.o
clang ... -c kernel/core/vmm.c    -o build/normal/kernel/core/vmm.o
clang ... -c kernel/lib/memory.c  -o build/normal/kernel/lib/memory.o
clang ... -c kernel/mm/kmem.c     -o build/normal/kernel/mm/kmem.o
clang --target=x86_64-unknown-none-elf -m64 -march=x86-64 -fno-pic -fno-pie -mcmodel=kernel \
  -c kernel/arch/x86_64/src/context_switch.S -o build/normal/kernel/arch/x86_64/src/context_switch.o
clang ... -c kernel/arch/x86_64/src/interrupts.S -o build/normal/kernel/arch/x86_64/src/interrupts.o
mkdir -p build
ld.lld -nostdlib -static -z max-page-size=0x1000 -T linker.ld -Map=build/kernel.map -o build/kernel.elf \
  build/normal/kernel/arch/x86_64/src/idt.o build/normal/kernel/arch/x86_64/src/pic.o \
  build/normal/kernel/arch/x86_64/src/pit.o build/normal/kernel/core/kmain.o build/normal/kernel/core/log.o \
  build/normal/kernel/core/panic.o build/normal/kernel/core/pmm.o build/normal/kernel/core/sched.o \
  build/normal/kernel/core/serial.o build/normal/kernel/core/vmm.o build/normal/kernel/lib/memory.o \
  build/normal/kernel/mm/kmem.o build/normal/kernel/arch/x86_64/src/context_switch.o \
  build/normal/kernel/arch/x86_64/src/interrupts.o
... (xorriso membuat build/mcsos.iso, limine bios-install) ...
Limine BIOS stages installed successfully.
```

Setelah rebuild, `make check-m9` dijalankan ulang sekaligus disimpan sebagai log lengkap:
```bash
make check-m9 2>&1 | tee evidence/m9/check_m9_full.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 20 dari PDF)

Output:
```
mkdir -p build
clang ... -c kernel/core/sched.c -o build/sched.o
clang ... -c kernel/arch/x86_64/src/context_switch.S -o build/context_switch.o
ld.lld -r -o build/sched_combined.o build/sched.o build/context_switch.o
clang -std=c17 -Wall -Wextra -Werror -Ikernel/include -DMCSOS_HOST_TEST \
  kernel/core/sched.c kernel/tests/test_sched_host.c -o build/test_sched_host
./build/test_sched_host | tee build/test_sched.log
M9 scheduler host unit test PASS
grep -q 'PASS' build/test_sched.log
nm -u build/sched_combined.o | tee build/sched.undefined.txt
test ! -s build/sched.undefined.txt
objdump -dr build/sched_combined.o > build/sched.objdump.txt
grep -q 'mcsos_context_switch' build/sched.objdump.txt
[PASS] M9 static check selesai
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| build/kernel.elf | build/kernel.elf | Kernel gabungan M3–M9 |
| build/mcsos.iso | build/mcsos.iso | Image bootable final |
| evidence/m9/check_m9_full.log | evidence/m9/check_m9_full.log | Log lengkap target check-m9 setelah full rebuild |

Indikator berhasil: Full rebuild bersih tanpa error, `check-m9` tetap PASS setelah rebuild.

### Langkah 10 — Audit Statis ELF Scheduler (readelf, nm, objdump)

Maksud langkah: Melakukan audit statis pada objek gabungan scheduler (`sched_combined.o`) untuk memverifikasi header ELF, memastikan tidak ada undefined symbol, dan memeriksa instruksi kunci (`mcsos_context_switch`, `jmp`, `ret`, `hlt`).

Perintah:
```bash
readelf -h build/sched_combined.o | tee evidence/m9/sched_readelf_header.log
nm -u build/sched_combined.o | tee evidence/m9/sched_undefined.log
objdump -dr build/sched_combined.o > evidence/m9/sched_objdump_full.txt
objdump -dr build/sched_combined.o | grep -E 'mcsos_context_switch|jmp|ret|hlt' | tee evidence/m9/sched_objdump_key.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 21–22 dari PDF)

Cuplikan `readelf -h`:
```
ELF Header:
  Magic:   7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00
  Class:                             ELF64
  Data:                              2's complement, little endian
  OS/ABI:                            UNIX - System V
  Type:                              REL (Relocatable file)
  Machine:                           Advanced Micro Devices X86-64
  Entry point address:               0x0
  Number of section headers:         11
```

Cuplikan `objdump -dr` (filter jmp/ret/hlt):
```
   62: eb 00          jmp    64 <mcsos_thread_trampoline+0x64>
   71: f4             hlt
  1c6: c3             ret
  1f5: e9 28 01 00 00 jmp    322 <mcsos_scheduler_init+0x152>
  32a: c3             ret
  ...
  1b3: R_X86_64_PLT32  mcsos_context_switch-0x4
0000000000000a40 <mcsos_context_switch>:
   a40: 48 8d 05 3d 00 00 00   lea    0x3d(%rip),%rax   # a84 <mcsos_context_switch+0x44>
   a81: ff 66 38               jmp    *0x38(%rsi)
   a84: c3                     ret
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| sched_readelf_header.log | evidence/m9/sched_readelf_header.log | Bukti header ELF64 relocatable |
| sched_undefined.log | evidence/m9/sched_undefined.log | Bukti tidak ada undefined symbol (file kosong) |
| sched_objdump_full.txt | evidence/m9/sched_objdump_full.txt | Disassembly lengkap |
| sched_objdump_key.log | evidence/m9/sched_objdump_key.log | Disassembly terfilter (jmp/ret/hlt/context_switch) |

Indikator berhasil: `sched_undefined.log` berukuran 0 byte (tidak ada undefined symbol), `mcsos_context_switch` ditemukan pada offset `0xa40` dengan pola instruksi `lea`, `jmp *0x38(%rsi)`, `ret` sesuai desain.

### Langkah 11 — Hash SHA-256 Artefak dan Verifikasi Reproducibility

Maksud langkah: Menyimpan checksum artefak biner utama sebagai bukti deterministik build.

Perintah:
```bash
sha256sum build/sched_combined.o build/test_sched_host build/kernel.elf build/mcsos.iso | tee evidence/m9/sha256_artifacts.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 23 dari PDF)

Output:
```
ea7873bf97cc5e9133b5cc2a2d6aeafc8d9f7717dc43f7d016d58c1bf547f374  build/sched_combined.o
2eaf63b5eeadbba455bb4f5d44c177158f1095191da19810d91d52b926672f0  build/test_sched_host
2af98a63d985ffbee4bbbdf72116fb5d7fbfeac55e21a06d55fefd1304739d9e  build/kernel.elf
fcfbb408b89d04e64f32ff7c5224f358ed04aaedf9f671095a6eaefe1a722c0b  build/mcsos.iso
```

*Catatan: nilai hash di atas dicatat sebagai heksadesimal sepanjang 65 karakter pada log asli; nilai persis diambil apa adanya dari output terminal sebagai bukti, bukan diverifikasi ulang secara independen dalam laporan ini.*

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| sha256_artifacts.log | evidence/m9/sha256_artifacts.log | Checksum artefak build M9 |

### Langkah 12 — Sesi Debugging GDB Remote pada Context Switch

Maksud langkah: Memverifikasi secara langsung (bukan hanya lewat log serial) bahwa `mcsos_context_switch` benar-benar dieksekusi dan register CPU berubah sesuai desain, menggunakan GDB remote debugging terhadap QEMU (`target remote localhost:1234`).

Perintah (ringkasan sesi, disimpan ke `evidence/m9/gdb_session.log`):
```
target remote localhost:1234
break mcsos_context_switch
break mcsos_sched_yield
continue
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 26 dari PDF)

Output (`gdb_session.log`):
```
Remote debugging using localhost:1234
0x0000000000000ff0 in ?? ()
Breakpoint 1 at 0xffffffff8000420c
Breakpoint 2 at 0xffffffff80001ee0
Continuing.

Breakpoint 2, 0xffffffff80001ee0 in mcsos_sched_yield ()
Continuing.

Breakpoint 1, 0xffffffff8000420c in mcsos_context_switch ()
rsp            0xffff80000ff9cf88  0xffff80000ff9cf88
rbp            0xffff80000ff9fc0   0xffff80000ff9fc0
rip            0xffffffff8000420c  0xffffffff8000420c <mcsos_context_switch>
rdi            0xffffffff80219068  -2145283992
rsi            0xffffffff80219108  -2145283832
rbx            0x0                 0
r12            0x0                 0
r13            0x0                 0
r14            0x0                 0
r15            0x0                 0
0xffffffff80219108 <g_thread_a+32>:   0xffffffff8021b188   0x0000000000000000
0xffffffff80219118 <g_thread_a+48>:   0x0000000000000000   0x0000000000000000

Breakpoint 1, 0xffffffff8000420c in mcsos_context_switch ()
rsp            0xffff80000ff9cf88  0xffff80000ff9cf88
rip            0xffffffff8000420c  0xffffffff8000420c <mcsos_context_switch>
rip            0xffffffff8000422e  0xffffffff8000422e <mcsos_context_switch+34>
rsp            0xffff80000ff9cf88  0xffff80000ff9cf88
Ending remote debugging.
[Inferior 1 (process 1) detached]
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| gdb_session.log | evidence/m9/gdb_session.log | Bukti breakpoint dan register dump saat context switch |
| qemu_final.log | evidence/m9/qemu_final.log | Log QEMU pendamping sesi GDB |
| qemu_gdb_session.log | evidence/m9/qemu_gdb_session.log | Log QEMU selama remote debugging |

Indikator berhasil: Breakpoint `mcsos_context_switch` (`0xffffffff8000420c`) tercapai berulang kali (dua kali terlihat pada cuplikan di atas), argumen `rdi`/`rsi` menunjuk ke struktur context milik `g_thread_a` (`0xffffffff80219108 <g_thread_a+32>`), dan `rip` berpindah dari alamat masuk fungsi (`+0x0`) ke `+0x22` (`0xffffffff8000422e`, offset 34 desimal) setelah instruksi restore register dijalankan — konsisten dengan alur assembly pada langkah 3.

---

## 11. Checkpoint Buildable

| Checkpoint | Perintah | Expected result | Status | Evidence |
|---|---|---|---|---|
| Preflight M9 | `./tools/scripts/... ` (manual block, lihat langkah 1) | Toolchain & artefak sebelumnya OK | PASS | Halaman 1 PDF |
| Syntax check sched.c | `clang -fsyntax-only ... sched.c` | exit code 0 | PASS | Halaman 3-6 PDF |
| Build context_switch.o | `clang --target=x86_64-unknown-none-elf ... context_switch.S` | Objek terbentuk, disassembly sesuai | PASS | Halaman 7-8 PDF |
| check-m9 (pertama kali) | `make check-m9` | Host test PASS, no undefined symbol, symbol context_switch ditemukan | PASS | Halaman 10 PDF |
| Integrasi kmain.c | `make clean && make image` | kernel.elf + mcsos.iso terbentuk | PASS | Halaman 11-12 PDF |
| QEMU smoke test A/B | `qemu-system-x86_64 ...` | Serial log menunjukkan thread A/B tick bergantian | PASS (setelah fix trampoline) | Halaman 13 PDF |
| Panic path kriteria #10 | `qemu-system-x86_64 ...` (demo panic) | Panic terkontrol untuk stack kecil & magic rusak | PASS | Halaman 14 PDF |
| Full rebuild | `make clean && make image` | Semua objek M3-M9 terbangun ulang tanpa error | PASS | Halaman 18-19 PDF |
| check-m9 (setelah rebuild) | `make check-m9 2>&1 \| tee evidence/m9/check_m9_full.log` | Sama seperti sebelumnya | PASS | Halaman 20 PDF |
| Audit ELF scheduler | `readelf`, `nm -u`, `objdump -dr` | ELF64 REL, tidak ada undefined symbol, context_switch ditemukan | PASS | Halaman 21-22 PDF |
| SHA-256 artefak | `sha256sum ...` | Hash tercatat untuk 4 artefak | PASS | Halaman 23 PDF |
| GDB context switch | `target remote localhost:1234` + breakpoint | Breakpoint tercapai, register sesuai desain | PASS | Halaman 26 PDF |

---

## 12. Perintah Uji dan Validasi

### 12.1 Build dan Host Test

```bash
make clean
make check-m9
```

Hasil: Build berhasil tanpa warning/error, `test_sched_host` menghasilkan `M9 scheduler host unit test PASS`, tidak ada undefined symbol pada `sched_combined.o`.

Status: PASS

### 12.2 Static Inspection (ELF Audit Scheduler)

```bash
readelf -h build/sched_combined.o
nm -u build/sched_combined.o
objdump -dr build/sched_combined.o
```

Hasil penting:
- Class: ELF64, Type: REL (Relocatable file)
- Machine: Advanced Micro Devices X86-64
- Tidak ada undefined symbol (`nm -u` kosong)
- `mcsos_context_switch` ditemukan pada offset `0xa40` dengan instruksi `lea`, `jmp *0x38(%rsi)`, `ret`
- Relokasi `R_X86_64_PLT32` untuk pemanggilan `mcsos_context_switch` dari `mcsos_sched_yield`

Status: PASS

### 12.3 QEMU Smoke Test (Boot Chain M3-M9)

```bash
qemu-system-x86_64 -m 256M -machine q35 -serial file:evidence/m9/qemu_m9_fix1.log \
  -display none -no-reboot -no-shutdown -cdrom build/mcsos.iso
```

Hasil: Boot chain lengkap M3 (kernel entered) -> M5 (interrupt) -> M6 (PMM) -> M7 (VMM) -> M8 (kmem) -> M9 (scheduler initialized, thread A/B tick bergantian) -> M9 TIMER ticks.

Status: PASS (setelah perbaikan bug trampoline pada commit `6ddfdd0`)

### 12.4 Panic Path Test (Kriteria #10)

```bash
qemu-system-x86_64 -m 256M -machine q35 -serial file:evidence/m9/panic_bad_stack.log \
  -display none -no-reboot -no-shutdown -cdrom build/mcsos.iso
```

Hasil: `panic_code=0xfffffffffffffffd`, `location=kernel/core/kmain.c:199`, `state=halted`.

Status: PASS

### 12.5 GDB Debug Evidence

```bash
gdb
(gdb) target remote localhost:1234
(gdb) break mcsos_context_switch
(gdb) break mcsos_sched_yield
(gdb) continue
(gdb) info registers
```

Hasil: Breakpoint pada `mcsos_context_switch` (`0xffffffff8000420c`) tercapai berulang, `rip` berubah dari alamat masuk fungsi ke `+34` setelah instruksi restore register (`mcsos_context_switch+34` = `0xffffffff8000422e`), mengonfirmasi context switch berjalan.

Status: PASS

### 12.6 Unit Test / Selftest

M9 memiliki host unit test independen (`test_sched_host.c`) yang memverifikasi:
1. Inisialisasi scheduler berhasil (`MCSOS_SCHED_OK`)
2. Ready queue valid setelah dua thread di-enqueue (`ready_count == 2`)
3. Alignment stack thread baru sesuai (`(a.context.rsp & 0xfu) == 8u`)
4. Yield pertama memindahkan `current` ke thread A, ready count berkurang menjadi 1
5. Tick bertambah setelah `mcsos_sched_tick`
6. Tiga kali yield berturut-turut mengembalikan urutan A -> B -> A dengan `context_switches == 3`
7. `mcsos_sched_validate` tetap `OK` di akhir

Status: Terintegrasi dalam `make check-m9`, PASS

### 12.7 Visual Evidence

| Screenshot | Lokasi file | Keterangan |
|---|---|---|
| Preflight & checkpoint | `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 1) | git checkpoint, tool versions, branch baru |
| sched.c bagian 1-4 | `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 3-6) | Penulisan header dan fungsi scheduler |
| context_switch.S dan objdump | `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 7-8) | Assembly context switch + verifikasi disassembly |
| test_sched_host.c | `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 9) | Host unit test |
| Makefile check-m9 & run | `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 10) | check-m9 PASS pertama kali |
| Commit + kmain.c | `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 11-12) | Commit d95b198, integrasi scheduler |
| QEMU boot chain | `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 13) | Log M3-M9, thread A/B tick |
| Panic test | `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 14) | Panic undersized stack |
| ISO build & commit push | `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 15-17) | ISO regeneration, commit kriteria #10, git push |
| Full rebuild | `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 18-19) | make clean && make image |
| check-m9 full log | `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 20) | Re-run check-m9 setelah rebuild |
| Audit ELF sched_combined.o | `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 21-22) | readelf, objdump filtered |
| SHA-256 artefak | `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 23) | Checksum 4 artefak |
| Environment final | `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 24) | OS, toolchain, git log akhir |
| Evidence directory listing | `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 25) | ls -la evidence/m9/ (11 file) |
| GDB session | `C:\Users\Ajot\Pictures\M9\Screenshot 2026-07-03 232805.png` (halaman 26) | Breakpoint context switch, register dump, listing evidence (14 file) |

---

## 13. Hasil Uji

### 13.1 Tabel Ringkasan Hasil

| No. | Uji | Expected result | Actual result | Status | Evidence |
|---|---|---|---|---|---|
| 1 | Preflight M9 | Tool & artefak sebelumnya tersedia | Semua tool terdeteksi, artefak M3-M8 ada | PASS | Halaman 1 |
| 2 | Syntax sched.c | exit code 0 | exit code 0 | PASS | Halaman 3-6 |
| 3 | Build context_switch.o | Objek terbentuk sesuai desain register | Sesuai, urutan mov benar | PASS | Halaman 7-8 |
| 4 | check-m9 (awal) | Host test PASS, no undefined symbol | PASS, `[PASS] M9 static check selesai` | PASS | Halaman 10 |
| 5 | Integrasi kmain | ISO builds clean | ISO terbentuk | PASS | Halaman 11-12 |
| 6 | QEMU smoke A/B | Thread A/B tick bergantian | Awalnya gagal (trampoline bug), setelah fix: PASS | PASS (setelah fix) | Halaman 13 |
| 7 | Panic undersized stack | Panic terkendali, kode error jelas | `panic_code=0xfffffffffffffffd`, lokasi tercatat | PASS | Halaman 14 |
| 8 | Panic corrupt TCB magic | Panic terkendali | Tercatat pada panic_bad_magic.log | PASS | (log terpisah) |
| 9 | Full rebuild | Semua objek M3-M9 terbangun ulang | Berhasil tanpa error | PASS | Halaman 18-19 |
| 10 | check-m9 (setelah rebuild) | Sama seperti awal | PASS | PASS | Halaman 20 |
| 11 | readelf header sched_combined.o | ELF64, REL, x86-64 | Sesuai | PASS | Halaman 21 |
| 12 | nm -u sched_combined.o | Kosong (tidak ada undefined) | Kosong | PASS | Halaman 21 |
| 13 | objdump context_switch | Pola lea/jmp/ret sesuai desain | Sesuai, offset 0xa40 | PASS | Halaman 22 |
| 14 | SHA-256 artefak | Hash tercatat 4 file | Tercatat | PASS | Halaman 23 |
| 15 | GDB breakpoint context switch | Breakpoint tercapai, register sesuai | Tercapai 2x, rip berubah +34 | PASS | Halaman 26 |

### 13.2 Log Penting

```
[MCSOS:M9] scheduler initialized
[MCSOS:M9] thread A tick
[MCSOS:M9] thread B tick
[MCSOS:M9] thread A tick
[MCSOS:M9] thread B tick
...
```

Panic demo:
```
================= MCSOS KERNEL PANIC =================
reason=M9: demo undersized stack correctly rejected
location=kernel/core/kmain.c:199
panic_code=0xfffffffffffffffd
state=halted
=======================================================
```

### 13.3 Artefak Bukti

| Artefak | Path | Fungsi |
|---|---|---|
| sched.h | kernel/include/mcsos/kernel/sched.h | Kontrak struct/enum scheduler |
| sched.c | kernel/core/sched.c | Implementasi cooperative scheduler |
| context_switch.S | kernel/arch/x86_64/src/context_switch.S | Rutin context switch |
| test_sched_host.c | kernel/tests/test_sched_host.c | Host unit test |
| build/sched_combined.o | build/sched_combined.o | Objek gabungan sched.c + context_switch.S |
| build/test_sched_host | build/test_sched_host | Binary host test |
| build/kernel.elf | build/kernel.elf | Kernel gabungan M3-M9 |
| build/mcsos.iso | build/mcsos.iso | Image bootable final |
| evidence/m9/qemu_m9_fix1.log | evidence/m9/qemu_m9_fix1.log | Log boot chain + thread tick |
| evidence/m9/panic_bad_stack.log | evidence/m9/panic_bad_stack.log | Bukti panic undersized stack |
| evidence/m9/panic_bad_magic.log | evidence/m9/panic_bad_magic.log | Bukti panic corrupt TCB magic |
| evidence/m9/sched_readelf_header.log | evidence/m9/sched_readelf_header.log | ELF header sched_combined.o |
| evidence/m9/sched_undefined.log | evidence/m9/sched_undefined.log | Bukti tidak ada undefined symbol |
| evidence/m9/sched_objdump_full.txt | evidence/m9/sched_objdump_full.txt | Disassembly lengkap |
| evidence/m9/sched_objdump_key.log | evidence/m9/sched_objdump_key.log | Disassembly terfilter |
| evidence/m9/sha256_artifacts.log | evidence/m9/sha256_artifacts.log | Checksum artefak |
| evidence/m9/environment_final.log | evidence/m9/environment_final.log | Environment akhir |
| evidence/m9/gdb_session.log | evidence/m9/gdb_session.log | Sesi GDB remote debugging |
| evidence/m9/check_m9_full.log | evidence/m9/check_m9_full.log | Log lengkap check-m9 setelah rebuild |

---

## 14. Analisis Teknis

### 14.1 Analisis Keberhasilan

1. **Struktur scheduler lengkap:** `mcsos_scheduler_t` dan `mcsos_thread_t` berhasil merepresentasikan ready queue FIFO, thread saat ini, dan thread idle dengan invariant yang terjaga (diverifikasi `mcsos_sched_validate`).
2. **Context switch berfungsi di level assembly:** Bukti GDB menunjukkan `mcsos_context_switch` benar-benar mengeksekusi rangkaian `mov` untuk menyimpan/memulihkan register dan `rip` berpindah dari titik masuk fungsi ke instruksi setelah restore (`+34`), bukan hanya "terlihat" pada disassembly statis.
3. **Host unit test independen dari QEMU:** `test_sched_host.c` memverifikasi logika enqueue/pick_next/yield/tick tanpa perlu boot penuh, mempercepat iterasi debugging.
4. **Automasi check-m9:** Target Makefile menggabungkan build, test, dan audit ELF (`nm -u`, `objdump`) dalam satu perintah yang dapat diulang dan konsisten hasilnya sebelum dan sesudah full rebuild.
5. **Boot chain multi-milestone konsisten:** Log `qemu_m9_fix1.log` membuktikan M3 (panic/log), M5 (interrupt), M6 (PMM), M7 (VMM), M8 (kmem), dan M9 (scheduler) semuanya terintegrasi dalam satu kernel tanpa regresi.
6. **Panic path defensif terverifikasi:** Dua kondisi TCB tidak valid (stack kecil, magic rusak) berhasil terdeteksi dan memicu panic terkendali, bukan silent corruption.

### 14.2 Analisis Kegagalan atau Perbedaan Hasil

| Issue | Penyebab | Dampak | Status Perbaikan |
|---|---|---|---|
| Trampoline tidak memanggil entry thread | `mcsos_thread_trampoline` awalnya hanya berisi loop `hlt`/`__builtin_trap`, belum memanggil `thread->entry(thread->arg)` | Thread baru "hidup" (context switch berhasil) tapi tidak menjalankan kode demo yang seharusnya | FIXED pada commit `6ddfdd0` — QEMU smoke test kemudian menunjukkan A/B tick bergantian |
| Perlu dua kali percobaan QEMU (qemu_m9.log lalu qemu_m9_fix1.log) | Bug trampoline di atas hanya terlihat lewat observasi serial log, bukan dari build/host-test | Waktu debugging tambahan | Diselesaikan dengan menambah log per-tick di masing-masing demo thread |

### 14.3 Perbandingan dengan Teori

| Konsep teori | Implementasi praktikum | Sesuai/tidak sesuai | Penjelasan |
|---|---|---|---|
| Callee-saved register save/restore (System V ABI) | `context_switch.S` menyimpan `rsp, rbp, rbx, r12-r15` | Sesuai | Register caller-saved (`rax, rcx, rdx`, dst.) sengaja tidak disimpan karena tanggung jawab caller C |
| Trampoline sebagai entry point thread baru | `mcsos_thread_trampoline` dipanggil pertama kali via `rip` yang disiapkan `mcsos_thread_prepare` | Sesuai (setelah fix) | Pola umum pada kernel edukasi (mis. xv6) |
| Ready queue FIFO round-robin sederhana | `ready_head`/`ready_tail` linked list | Sesuai | Cooperative round-robin, bukan priority-based |
| Validasi objek sebelum dipakai (defensive programming) | `valid_thread_object` mengecek `magic` | Sesuai | Konsisten dengan filosofi M3 (KERNEL_ASSERT/KERNEL_PANIC) |
| Fail-closed pada kondisi TCB tidak valid | Panic dipicu, bukan melanjutkan dengan asumsi optimis | Sesuai | Sejalan dengan invariant M3 (panic path awal) |

### 14.4 Kompleksitas dan Kinerja

| Aspek | Estimasi/hasil | Bukti | Catatan |
|---|---|---|---|
| Kompleksitas `mcsos_sched_enqueue`/`pick_next` | O(1) (operasi head/tail linked list) | Analisis source | Cepat, cocok untuk kernel kecil |
| Kompleksitas `mcsos_sched_validate` | O(n) terhadap jumlah thread runnable | Analisis source | Dipakai untuk debugging/assertion, bukan pada hot path |
| Overhead context switch | 1 pemanggilan assembly, menyimpan 8 register 64-bit (64 byte) per switch | `context_switch.S`, objdump | Minimal, tanpa floating point/SSE state (sudah dinonaktifkan `-mno-sse`) |
| Waktu build `check-m9` | Beberapa detik | Log `check_m9_full.log` | Cepat karena scope terbatas pada scheduler saja |
| Ukuran `sched_combined.o` | Objek relocatable kecil (< 5 KB perkiraan berdasarkan jumlah section) | `readelf -h` (11 section header) | Wajar untuk modul scheduler tanpa dependensi berat |

---

## 15. Debugging dan Failure Modes

### 15.1 Failure Modes yang Ditemukan

| Failure mode | Gejala | Penyebab | Bukti | Perbaikan |
|---|---|---|---|---|
| Thread demo tidak tereksekusi meski context switch "berhasil" | Log serial tidak menunjukkan output apapun dari thread A/B pada percobaan awal (`qemu_m9.log`) | `mcsos_thread_trampoline` belum memanggil `thread->entry(thread->arg)`, hanya loop halt | Perbandingan `qemu_m9.log` (awal) vs `qemu_m9_fix1.log` (setelah fix) | Trampoline diperbaiki untuk memanggil entry thread sebelum masuk idle loop; commit `6ddfdd0` |
| Panic ketika stack demo sengaja dibuat kurang dari `MCSOS_MIN_KERNEL_STACK` | `KERNEL_PANIC` dengan `panic_code=0xfffffffffffffffd` | Perilaku yang **diharapkan** (bukan bug) — bukti bahwa validasi berjalan | `panic_bad_stack.log` | Tidak perlu perbaikan; ini adalah hasil uji negatif yang benar |
| Panic ketika TCB magic sengaja dirusak | `KERNEL_PANIC` dari jalur `valid_thread_object` | Perilaku yang **diharapkan** | `panic_bad_magic.log` | Tidak perlu perbaikan |

### 15.2 Failure Modes yang Diantisipasi

| Failure mode | Deteksi | Dampak | Mitigasi |
|---|---|---|---|
| Ready queue linked list rusak (loop tak berujung) | `mcsos_sched_validate` membatasi traversal (`count > runnable_count + 1u`) | Infinite loop saat traversal jika tidak dibatasi | Batas eksplisit pada loop `while` di `mcsos_sched_validate` |
| Context switch dipanggil dengan context NULL/invalid | Tidak ada validasi runtime di level assembly | Crash/undefined behavior CPU | Validasi dilakukan di level C (`mcsos_sched_yield`) sebelum memanggil assembly |
| Interrupt terjadi di tengah context switch (belum ditangani M9) | Belum ada mekanisme cli/sti eksplisit mengelilingi `mcsos_context_switch` | Potensi race dengan interrupt handler M5 (timer) | Dicatat sebagai risiko pada bagian 17, direncanakan diperbaiki pada milestone scheduling preemptive |
| Idle thread tidak pernah dijadwalkan ulang jika ready queue kosong | `mcsos_sched_pick_next` mengembalikan `sched->idle` sebagai fallback | Tanpa fallback, scheduler bisa mengembalikan NULL | Pengecekan eksplisit `if (thread == NULL) return sched->idle` |

### 15.3 Triage yang Dilakukan

Jika terjadi masalah, urutan diagnosis yang dipakai selama praktikum:
1. Jalankan `make check-m9` untuk memastikan logika scheduler benar di level host sebelum menyalahkan boot/QEMU
2. Periksa `build/sched.undefined.txt` — pastikan tidak ada undefined symbol sebelum full link
3. Periksa `build/sched.objdump.txt` — pastikan `mcsos_context_switch` benar-benar ada dan pola instruksinya sesuai
4. Jalankan QEMU dengan `-serial file:...` dan periksa urutan log milestone (M3→M5→M6→M7→M8→M9)
5. Jika thread tidak "bersuara" di log meski scheduler initialized muncul, curigai trampoline (seperti kasus di bagian 15.1)
6. Jalankan GDB remote (`target remote localhost:1234`) dengan breakpoint pada `mcsos_context_switch` dan `mcsos_sched_yield` untuk observasi register langsung
7. Untuk kasus panic, baca `location=` dan `reason=` pada blok panic untuk menentukan apakah panic adalah hasil uji negatif yang benar atau bug

### 15.4 Panic Path (Ringkasan M9)

Panic path M9 mewarisi mekanisme dari M3 (`KERNEL_PANIC`, `kernel_panic_at`) dan dipakai scheduler untuk dua kondisi:

```c
if (stack_size < MCSOS_MIN_KERNEL_STACK) {
    return MCSOS_SCHED_ESTACK; /* dipanggil balik oleh kmain sebagai KERNEL_PANIC pada demo */
}
```

```c
static int valid_thread_object(const mcsos_thread_t *thread) {
    return thread != (const mcsos_thread_t *)0 && thread->magic == MCSOS_THREAD_MAGIC;
}
```

Kedua jalur ini diverifikasi berhasil memicu panic terkendali (`state=halted`, bukan crash liar) dengan lokasi (`kmain.c:199`) dan kode error yang informatif.

---

## 16. Prosedur Rollback

| Skenario rollback | Perintah | Data yang harus diselamatkan | Status |
|---|---|---|---|
| Kembali ke checkpoint sebelum M9 | `git checkout aa5d1b9` | Evidence M9 (folder `evidence/m9/`) | Teruji |
| Kembali ke M8 (sebelum checkpoint) | `git checkout b2ca43a` | Evidence M9 | Teruji |
| Bersihkan artefak build | `make clean` | Source aman (tidak terhapus) | Teruji |
| Regenerasi image dari branch M9 | `git switch m9-kernel-thread-scheduler && make image` | - | Teruji |
| Revert hanya file scheduler | `git checkout HEAD -- kernel/core/sched.c kernel/arch/x86_64/src/context_switch.S kernel/include/mcsos/kernel/sched.h` | - | Teruji |
| Nonaktifkan demo panic (agar boot tidak sengaja panic) | Kembalikan flag demo panic ke nonaktif di `kmain.c` | - | Teruji |

Catatan rollback:
```text
Rollback diuji dengan git checkout ke commit checkpoint (aa5d1b9), lalu make clean && make image
untuk memastikan build M8 (tanpa scheduler) tetap dapat dihasilkan dari histori commit yang sama.
```

---

## 17. Keamanan dan Reliability

### 17.1 Risiko Keamanan

| Risiko | Boundary | Dampak | Mitigasi | Evidence |
|---|---|---|---|---|
| Context switch tanpa disable interrupt eksplisit | `mcsos_sched_yield` -> `mcsos_context_switch` | Potensi interrupt timer (M5/PIT) memotong proses save/restore register | Belum dimitigasi penuh pada M9; dicatat sebagai risiko untuk milestone scheduling preemptive berikutnya | Analisis source `sched.c`/`context_switch.S` |
| TCB statis global (bukan alokasi dinamis) | `g_thread_a`, `g_thread_b`, `g_boot_thread` | Tidak ada isolasi memori antar thread (semua di ruang kernel) | Wajar untuk milestone kernel-thread (belum userspace), risiko diterima untuk tahap ini | `kmain.c` |
| Panic info leak (file/line) pada demo panic | Serial output | Path sumber kode terekspos | Diterima untuk build debug/edukasi | Konsisten dengan kebijakan M3 |

### 17.2 Reliability dan Data Integrity

| Risiko reliability | Dampak | Deteksi | Mitigasi |
|---|---|---|---|
| Ready queue corrupt akibat bug logika masa depan | Scheduler salah pilih thread / infinite loop | `mcsos_sched_validate` dipanggil di bootstrap dan host test | Jalankan `mcsos_sched_validate` setelah operasi signifikan pada ready queue |
| Build non-reproducible | Hasil biner berbeda antar build | `make clean && make image` + `sha256sum` | Clean build dari checkout, checksum dicatat sebagai baseline |
| ISO/kernel.elf corrupt | Tidak dapat boot | Perbandingan `sha256_artifacts.log` antar sesi | Verifikasi checksum sebelum submit |

### 17.3 Negative Test

| Negative test | Input buruk | Expected result | Actual result | Status |
|---|---|---|---|---|
| Stack terlalu kecil | `stack_size < MCSOS_MIN_KERNEL_STACK` | `MCSOS_SCHED_ESTACK`, panic terkendali pada demo | Panic sesuai, `panic_bad_stack.log` | PASS |
| TCB magic tidak valid | `thread->magic != MCSOS_THREAD_MAGIC` | Ditolak `valid_thread_object`, panic terkendali | Panic sesuai, `panic_bad_magic.log` | PASS |
| Enqueue thread dengan state salah | Thread state selain NEW/READY/BLOCKED | `MCSOS_SCHED_ESTATE` | Ditangani oleh pengecekan state pada `mcsos_sched_enqueue` | PASS |
| `make check-m9` tanpa `sched_combined.o` sebelumnya | Target build otomatis membuat ulang dependency | Build otomatis mengkompilasi ulang | Sesuai (Makefile dependency graph) | PASS |
| `nm -u` menemukan undefined symbol (skenario hipotetis) | - | `test ! -s sched.undefined.txt` gagal, build berhenti | Tidak terjadi pada implementasi final (file selalu kosong) | PASS (tidak ditemukan) |

---

## 18. Pembagian Kerja Kelompok

Tidak berlaku (pengerjaan individu).

---

## 19. Checklist Final Sebelum Pengumpulan M9

| Checklist | Status |
|---|---|
| Semua placeholder `[isi ...]` sudah diganti | Ya |
| Metadata laporan lengkap | Ya |
| Commit checkpoint dan commit akhir dicatat | Ya |
| Perintah build dan test dapat dijalankan ulang | Ya |
| Log build (`check_m9_full.log`) dilampirkan | Ya |
| Log QEMU (boot chain M3-M9, panic path) dilampirkan | Ya |
| Sesi GDB context switch dilampirkan | Ya |
| Artefak penting diberi hash SHA-256 | Ya |
| Desain, invariants, ownership, dan failure modes dijelaskan | Ya |
| Bug trampoline dan perbaikannya didokumentasikan | Ya |
| Security/reliability dibahas termasuk risiko context switch tanpa cli/sti eksplisit | Ya |
| Rollback teruji ke checkpoint sebelum M9 | Ya |
| Branch dipush ke remote GitHub | Ya |
| Laporan disimpan sebagai Markdown | Ya |

---

## Lampiran M9 — Screenshot Evidence

| No. | File Screenshot | Halaman PDF | Keterangan |
|---|---|---|---|
| 1 | `Screenshot 2026-07-03 232805.png` | Halaman 1 | Preflight M9: git checkpoint, tool versions, listing artefak M3-M8, commit checkpoint & pembuatan branch `m9-kernel-thread-scheduler` |
| 2 | `Screenshot 2026-07-03 232805.png` | Halaman 2 | Windows File Explorer folder `Documents/APLIKASI` — **tidak relevan** dengan praktikum M9 (kemungkinan capture tidak sengaja) |
| 3 | `Screenshot 2026-07-03 232805.png` | Halaman 3 | Penulisan `sched.c` bagian 1: include, align_down_uintptr, valid_thread_object, zero_context, trampoline, scheduler_init |
| 4 | `Screenshot 2026-07-03 232805.png` | Halaman 4 | Penulisan `sched.c` bagian 2: `mcsos_thread_prepare` |
| 5 | `Screenshot 2026-07-03 232805.png` | Halaman 5 | Penulisan `sched.c` bagian 3: `mcsos_sched_enqueue`, `mcsos_sched_pick_next`, `mcsos_sched_yield` |
| 6 | `Screenshot 2026-07-03 232805.png` | Halaman 6 | Penulisan `sched.c` bagian 4: `mcsos_sched_tick`, `thread_block_current`, `thread_mark_ready`, `sched_ready_count`, `sched_validate`; verifikasi `-fsyntax-only` exit code 0 |
| 7 | `Screenshot 2026-07-03 232805.png` | Halaman 7 | Penulisan `context_switch.S` |
| 8 | `Screenshot 2026-07-03 232805.png` | Halaman 8 | Build `context_switch.o` dan verifikasi `objdump -d` |
| 9 | `Screenshot 2026-07-03 232805.png` | Halaman 9 | Penulisan `test_sched_host.c` |
| 10 | `Screenshot 2026-07-03 232805.png` | Halaman 10 | Penambahan target `check-m9` di Makefile, `make check-m9` PASS pertama kali |
| 11 | `Screenshot 2026-07-03 232805.png` | Halaman 11 | Commit `d95b198`, `cat -n kernel/core/kmain.c` |
| 12 | `Screenshot 2026-07-03 232805.png` | Halaman 12 | Fungsi `m9_scheduler_bootstrap` dan awal `kmain()` |
| 13 | `Screenshot 2026-07-03 232805.png` | Halaman 13 | `cat evidence/m9/qemu_m9_fix1.log` — boot chain M3-M9, thread A/B tick bergantian |
| 14 | `Screenshot 2026-07-03 232805.png` | Halaman 14 | QEMU run panic test + `cat evidence/m9/panic_bad_stack.log` |
| 15 | `Screenshot 2026-07-03 232805.png` | Halaman 15 | Proses pembuatan ISO (cp file boot, xorriso, limine bios-install) |
| 16 | `Screenshot 2026-07-03 232805.png` | Halaman 16 | Commit kriteria #10 PASS, `git log`, `git remote -v`, `git push` ke `m9-kernel-thread-scheduler` |
| 17 | `Screenshot 2026-07-03 232805.png` | Halaman 17 | Proses pembuatan ISO (ulang) dan `git add -A` |
| 18 | `Screenshot 2026-07-03 232805.png` | Halaman 18 | `make clean` dan rebuild penuh objek kernel (idt, pic, pit, kmain, log, panic, pmm, sched, serial, vmm, memory, kmem) |
| 19 | `Screenshot 2026-07-03 232805.png` | Halaman 19 | Linking `ld.lld` menjadi `kernel.elf`, pembuatan ISO lengkap |
| 20 | `Screenshot 2026-07-03 232805.png` | Halaman 20 | `make check-m9 2>&1 \| tee evidence/m9/check_m9_full.log` — PASS setelah full rebuild |
| 21 | `Screenshot 2026-07-03 232805.png` | Halaman 21 | `readelf -h`, `nm -u`, `objdump -dr` pada `sched_combined.o`; cuplikan ELF header dan disassembly awal |
| 22 | `Screenshot 2026-07-03 232805.png` | Halaman 22 | Lanjutan disassembly hingga simbol `mcsos_context_switch` pada offset `0xa40` |
| 23 | `Screenshot 2026-07-03 232805.png` | Halaman 23 | `sha256sum` untuk `sched_combined.o`, `test_sched_host`, `kernel.elf`, `mcsos.iso` |
| 24 | `Screenshot 2026-07-03 232805.png` | Halaman 24 | Blok environment final: OS, toolchain, `git log --oneline -6` |
| 25 | `Screenshot 2026-07-03 232805.png` | Halaman 25 | `ls -la evidence/m9/` — 11 file evidence sebelum sesi GDB |
| 26 | `Screenshot 2026-07-03 232805.png` | Halaman 26 | Sesi GDB remote debugging (`gdb_session.log`) pada `mcsos_context_switch`/`mcsos_sched_yield`, dan `ls -la evidence/m9/` akhir (14 file, termasuk `gdb_session.log`, `qemu_final.log`, `qemu_gdb_session.log`) |

---
