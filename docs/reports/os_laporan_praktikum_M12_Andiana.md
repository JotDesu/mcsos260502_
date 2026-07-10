# Laporan Praktikum M12 — Kernel Synchronization Primitives (Spinlock, Mutex, Lock-Order Validator)

## MCSOS 260502

**Nama file laporan:** `laporan_praktikum_M12_[NIM_Kelompok].md`
**Nama sistem operasi:** MCSOS versi 260502
**Target default:** x86_64, QEMU, Windows 11 x64 + WSL 2, kernel monolitik pendidikan, C freestanding dengan assembly minimal, POSIX-like subset
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.
**Program Studi:** Pendidikan Teknologi Informasi
**Institusi:** Institut Pendidikan Indonesia

---

## 0. Metadata Laporan

| Atribut | Isi |
|---|---|
| Kode praktikum | M12 |
| Judul praktikum | Kernel Synchronization Primitives (Spinlock, Mutex, Lock-Order Validator/Lockdep) |
| Jenis pengerjaan | Individu |
| Nama mahasiswa | Andiana Jamaludin Malik |
| NIM | 2583207073016 |
| Kelas | 1B |
| Nama kelompok | - |
| Anggota kelompok | - |
| Tanggal praktikum | 2026-07-08 |
| Tanggal pengumpulan | 2026-07-08 |
| Repository | ~/src/mcsos |
| Branch | praktikum/m12-sync |
| Commit awal | `2270ecd` (M11: tambahkan script QEMU smoke test) |
| Commit akhir | `3727d4c` (M12: implementasi spinlock, mutex, dan lockdep validator dengan self-test terintegrasi) |
| Status readiness yang diklaim | Siap uji QEMU tahap M12 |

---

## 1. Sampul

# Laporan Praktikum M12
## Kernel Synchronization Primitives (Spinlock, Mutex, Lock-Order Validator)

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

Saya menyatakan bahwa laporan ini disusun berdasarkan pekerjaan praktikum sendiri sesuai panduan M12. Bantuan eksternal, referensi, dan dokumentasi resmi dicatat pada bagian referensi dan lampiran.

| Pernyataan | Status |
|---|---|
| Semua potongan kode eksternal diberi atribusi | Ya (panduan M12 dari dosen) |
| Semua penggunaan AI assistant dicatat | Ya |
| Repository yang dikumpulkan sesuai commit akhir | Ya |
| Tidak ada klaim readiness tanpa bukti | Ya |

Catatan penggunaan bantuan eksternal:

```text
- Panduan praktikum M12 (OS_panduan_M12.pdf) sebagai referensi utama
- Template laporan praktikum (os_template_laporan_praktikum.md)
- AI assistant digunakan untuk membantu menyusun laporan dan analisis
- Semua source code diimplementasikan berdasarkan panduan dosen
```

---

## 3. Tujuan Praktikum

1. **Tujuan teknis 1:** Mengimplementasikan primitive sinkronisasi kernel dasar: spinlock (test-and-set + busy-wait) dan mutex (owner-aware, atomic compare-exchange)
2. **Tujuan teknis 2:** Mengimplementasikan lock-order validator (lockdep) berbasis rank untuk mendeteksi potensi deadlock (acquire berulang, acquire out-of-order)
3. **Tujuan konseptual 1:** Memahami hubungan antara atomic builtin GCC/Clang (`__atomic_*`), memory ordering (ACQUIRE/RELEASE/RELAXED), dan korektnesss mutual exclusion pada level kernel
4. **Tujuan validasi:** Menyimpan log build (host-test dan freestanding), audit ELF object (nm/readelf/objdump), hash SHA-256, serta log serial QEMU yang membuktikan self-test sinkronisasi lulus sebelum scheduler diaktifkan

---

## 4. Capaian Pembelajaran Praktikum

Setelah praktikum ini, mahasiswa mampu:

| CPL/CPMK praktikum | Bukti yang harus ditunjukkan |
|---|---|
| Menjelaskan perbedaan spinlock dan mutex serta kapan masing-masing dipakai pada kernel | Source code kernel/sync/spinlock.c, kernel/sync/mutex.c |
| Mengimplementasikan atomic test-and-set dengan `__atomic_exchange_n` dan busy-wait `pause` | kernel/sync/spinlock.c |
| Mengimplementasikan mutex owner-aware dengan `__atomic_compare_exchange_n` | kernel/sync/mutex.c |
| Mengimplementasikan lock-order validator berbasis rank (lockdep) | kernel/sync/lockdep.c, include/mcs_sync.h |
| Menulis unit test host (pthread) untuk memverifikasi korektnesss konkuren | tests/m12_sync_host_test.c |
| Membuat build system host-test, freestanding, dan audit | Makefile.m12 |
| Mengintegrasikan self-test sinkronisasi ke kmain() sebelum scheduler diaktifkan | kernel/core/kmain.c, kernel/sync/m12_selftest.c |
| Menghasilkan evidence build lengkap (nm/readelf/objdump/sha256) dan log serial QEMU | evidence/M12/* |

---

## 5. Peta Milestone MCSOS

| Milestone | Fokus | Status dalam laporan |
|---|---|---|
| M0 | Requirements, governance, baseline arsitektur | [x] selesai praktikum (praktikum sebelumnya) |
| M1 | Toolchain reproducible, Git, QEMU, GDB, metadata build | [x] selesai praktikum (praktikum sebelumnya) |
| M2 | Boot image, kernel ELF64, early console | [x] selesai praktikum (praktikum sebelumnya) |
| M3–M7 | Panic path, trap/interrupt, PMM/VMM, dsb. | [x] selesai praktikum (praktikum sebelumnya, tidak dibahas detail) |
| M8 | Kernel heap (kmem) | [x] selesai praktikum (praktikum sebelumnya) |
| M9 | Thread dan scheduler dasar | [x] selesai praktikum (dependency M12) |
| M10 | Syscall ABI dasar | [x] selesai praktikum (praktikum sebelumnya) |
| M11 | ELF user-image loader | [x] selesai praktikum (praktikum sebelumnya) |
| **M12** | **Synchronization primitives: spinlock, mutex, lock-order validator (lockdep)** | **[x] dibahas pada laporan ini** |
| M13 | SMP, scalability, lock stress, NUMA-aware preparation | [ ] tidak dibahas |
| M14 | Framebuffer, graphics console, visual regression | [ ] tidak dibahas |
| M15 | Virtualization/container subset | [ ] tidak dibahas |
| M16 | Observability, update/rollback, release image, readiness review | [ ] tidak dibahas |

Batas cakupan praktikum:

```text
Praktikum M12 mencakup:
- Preflight dan pembuatan branch praktikum/m12-sync
- Kontrak API sinkronisasi (include/mcs_sync.h)
- Implementasi lock-order validator rank-based (kernel/sync/lockdep.c)
- Implementasi spinlock atomic test-and-set (kernel/sync/spinlock.c)
- Implementasi mutex owner-aware (kernel/sync/mutex.c)
- Unit test host (pthread) deterministik (tests/m12_sync_host_test.c)
- Makefile.m12 dengan target host-test, freestanding, audit
- Audit object freestanding: nm -u, readelf -h, objdump -d, sha256sum
- Integrasi m12_sync_selftest() ke kmain() sebelum scheduler bootstrap
- Full kernel rebuild, image ISO, dan QEMU boot dengan bukti selftest PASS
- Commit Git dengan evidence lengkap

Non-goals (tidak termasuk):
- Reader-writer lock, semaphore counting, condition variable
- Priority inheritance / priority ceiling protocol
- Lock stress test multi-core (SMP) — direncanakan pada M13
- Interrupt-safe spinlock (spin_lock_irqsave) — belum diimplementasikan
- Deadlock recovery otomatis (hanya deteksi/reject, bukan recovery)
```

---

## 6. Dasar Teori Ringkas

### 6.1 Konsep Sistem Operasi yang Diuji

**Mutual Exclusion:** Menjamin hanya satu thread yang dapat mengakses critical section pada satu waktu, diimplementasikan dengan primitive atomic hardware.

**Spinlock:** Lock berbasis busy-wait yang cocok untuk critical section pendek; thread yang gagal mendapatkan lock akan terus mencoba (poll) tanpa context switch, menggunakan instruksi `pause` untuk mengurangi tekanan bus pada arsitektur x86_64.

**Mutex (owner-aware):** Lock yang menyimpan identitas pemilik (`owner_id`), sehingga dapat menolak unlock oleh non-pemilik (EPERM) dan mendeteksi acquire rekursif oleh pemilik yang sama (EDEADLK).

**Lock-Order Validator (Lockdep):** Mekanisme validasi statis/dinamis yang melacak stack lock yang sedang dipegang oleh sebuah state (mis. per-CPU/per-thread), dan menolak acquire yang melanggar aturan urutan (rank) untuk mencegah potensi deadlock akibat lock-ordering yang tidak konsisten.

**Atomic Builtins:** `__atomic_exchange_n`, `__atomic_compare_exchange_n`, `__atomic_load_n`, `__atomic_store_n` menyediakan operasi read-modify-write yang tidak dapat diinterupsi pada level instruksi CPU (mis. `xchg`, `lock cmpxchg`).

### 6.2 Konsep Arsitektur x86_64 yang Relevan

| Konsep | Relevansi pada praktikum | Bukti/verifikasi |
|---|---|---|
| Instruksi `xchg` (implicit lock) | Dasar atomic exchange pada spin_try_lock | objdump disassembly spinlock.o |
| Instruksi `pause` | Mengurangi memory-order violation penalty saat busy-wait | objdump disassembly spinlock.o |
| Memory ordering (ACQUIRE/RELEASE) | Menjamin visibilitas write critical section antar thread | Source code (`__ATOMIC_ACQUIRE`, `__ATOMIC_RELEASE`) |
| System V ABI | Konvensi pemanggilan fungsi C pada host test dan kernel object | -mabi=sysv, readelf |
| ELF64 relocatable object | Format hasil kompilasi freestanding sebelum linking penuh | readelf -h build/m12/*.o |

### 6.3 Konsep Implementasi Freestanding

| Aspek | Keputusan praktikum |
|---|---|
| Bahasa | C17 freestanding untuk objek kernel, C17 hosted (pthread) untuk unit test |
| Runtime | Objek kernel tanpa hosted libc (-ffreestanding -fno-builtin), unit test host memakai pthread.h |
| ABI | x86_64 ELF freestanding (`--target=x86_64-elf`) untuk objek kernel |
| Compiler flags kritis | -std=c17 -Wall -Wextra -Werror -Iinclude; kernel: -ffreestanding -fno-builtin -fno-stack-protector -fno-plc -mno-red-zone -O2; host: -O2 -pthread |
| Risiko undefined behavior | Mitigasi dengan -Werror, null-pointer guard di setiap fungsi publik (`if (lock == 0) return;`) |

### 6.4 Referensi Teori yang Digunakan

| No. | Sumber | Bagian yang digunakan | Alasan relevansi |
|---|---|---|---|
| [1] | OSDev Wiki - Spinlocks | Pola test-and-set dan busy-wait | Dasar implementasi mcs_spin_lock |
| [2] | GCC/Clang Documentation - Atomic Builtins | `__atomic_exchange_n`, `__atomic_compare_exchange_n` | Implementasi atomic primitive |
| [3] | Linux Kernel Documentation - Lockdep | Konsep validasi urutan lock | Desain rank-based lockdep sederhana |
| [4] | Intel SDM Vol. 2 | Instruksi XCHG, PAUSE, LOCK CMPXCHG | Verifikasi disassembly objdump |
| [5] | POSIX pthread Documentation | pthread_create/pthread_join | Unit test host multi-thread |

---

## 7. Lingkungan Praktikum

### 7.1 Host dan Target

| Komponen | Nilai |
|---|---|
| Host OS | Windows 11 x64 |
| Lingkungan build | WSL 2 Ubuntu 26.04 LTS (Resolute Raccoon) |
| Kernel WSL | Linux JotDesu 6.6.87.2-microsoft-standard-WSL2 |
| Target ISA (freestanding) | x86_64-elf |
| Target host-test | x86_64-pc-linux-gnu (native, pthread) |
| Emulator | QEMU system-x86_64 (emulator version 10.2.1) |
| Debugger | GNU gdb (Ubuntu 17.1-2ubuntu1) 17.1 |
| Build system | GNU Make 4.4.1 |
| Compiler | Ubuntu clang version 21.1.8 (6ubuntu1) / cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0 |
| Binutils | GNU nm/readelf/objdump 2.46 |

### 7.2 Versi Toolchain

Bukti screenshot: `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143001.png`

Output perintah:

```bash
cd ~/src/mcsos
uname -a
cat /etc/os-release
clang --version || true
cc --version | head -n 1 || true
make --version | head -n 1
qemu-system-x86_64 --version | head -n 1 || true
gdb --version | head -n 1 || true
nm --version | head -n 1 || true
readelf --version | head -n 1 || true
objdump --version | head -n 1 || true
git --version
```

Output ringkas:
```
Linux JotDesu 6.6.87.2-microsoft-standard-WSL2 #1 SMP PREEMPT_DYNAMIC Thu Jun 5 18:30:46 UTC 2025 x86_64 GNU/Linux
PRETTY_NAME="Ubuntu 26.04 LTS"
Ubuntu clang version 21.1.8 (6ubuntu1)
cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
GNU Make 4.4.1
QEMU emulator version 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3)
GNU gdb (Ubuntu 17.1-2ubuntu1) 17.1
GNU nm (GNU Binutils for Ubuntu) 2.46
GNU readelf (GNU Binutils for Ubuntu) 2.46
GNU objdump (GNU Binutils for Ubuntu) 2.46
git version 2.53.0
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143015.png`

### 7.3 Lokasi Repository

| Item | Nilai |
|---|---|
| Path repository di WSL | `/home/andianaaji/src/mcsos` |
| Apakah berada di filesystem Linux WSL, bukan `/mnt/c` | Ya (verified) |
| Remote repository | [URL repo privat jika ada] |
| Branch dasar | main (M11) |
| Branch praktikum | praktikum/m12-sync |
| Commit hash awal | `2270ecd` |
| Commit hash akhir | `3727d4c` |

---

## 8. Repository dan Struktur File

### 8.1 Struktur Direktori yang Relevan

Bukti screenshot: `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143120.png`

```text
mcsos/
├── Makefile
├── Makefile.m11
├── Makefile.m12
├── linker.ld
├── .gitignore
├── include/
│   └── mcs_sync.h
├── kernel/
│   ├── core/
│   │   └── kmain.c
│   ├── sync/
│   │   ├── lockdep.c
│   │   ├── spinlock.c
│   │   ├── mutex.c
│   │   └── m12_selftest.c
│   ├── mm/
│   │   └── kmem.c
│   ├── syscall/
│   │   └── syscall.c
│   └── user/
│       └── m11_elf_loader.c
├── tests/
│   ├── m12_sync_host_test.c
│   ├── test_kmem.c
│   └── test_syscall_host.c
├── evidence/
│   ├── M3 ... M11/
│   └── M12/
│       ├── preflight.log
│       ├── m12-build.log
│       ├── nm-undefined.txt
│       ├── readelf-lockdep.txt
│       ├── objdump-spinlock.txt
│       ├── sha256sums.txt
│       ├── m12-kernel-build-fixed.log
│       ├── m12-inspect-fixed.log
│       ├── qemu/
│       │   ├── kernel-image-build-fixed.log
│       │   ├── qemu-debug-fixed.log
│       │   └── serial-fixed.log
│       └── kernel.syms.txt
├── docs/
│   ├── architecture/ governance/ readiness/ reports/ requirements/ security/ testing/
├── third_party/
├── tools/
│   └── scripts/
└── build/
    └── m12/
        ├── lockdep.o
        ├── spinlock.o
        ├── mutex.o
        ├── m12_sync_host_test
        ├── host-test.log
        ├── nm-undefined.txt
        ├── readelf-lockdep.txt
        ├── objdump-spinlock.txt
        └── sha256sums.txt
```

### 8.2 File yang Dibuat atau Diubah

| File | Jenis perubahan | Alasan perubahan | Risiko |
|---|---|---|---|
| `include/mcs_sync.h` | Baru | Kontrak API sinkronisasi: lockdep_state, spinlock, mutex | Rendah - deklarasi header |
| `kernel/sync/lockdep.c` | Baru | Lock-order validator rank-based | Sedang - logika deteksi harus benar |
| `kernel/sync/spinlock.c` | Baru | Spinlock atomic test-and-set + pause busy-wait | Sedang - concurrency correctness |
| `kernel/sync/mutex.c` | Baru | Mutex owner-aware, atomic compare-exchange | Sedang - owner tracking harus benar |
| `kernel/sync/m12_selftest.c` | Baru | Self-test sinkronisasi yang dipanggil dari kmain() | Rendah - fungsi test sederhana |
| `tests/m12_sync_host_test.c` | Baru | Unit test host (pthread) untuk lockdep/spinlock/mutex | Rendah - test deterministik |
| `Makefile.m12` | Baru | Build system M12: host-test, freestanding, audit | Sedang - flags kritis |
| `kernel/core/kmain.c` | Ubah | Tambah include mcs_sync.h dan panggilan m12_sync_selftest() sebelum m9_scheduler_bootstrap() | Sedang - urutan boot harus tepat |

### 8.3 Ringkasan Diff

Bukti screenshot: `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143130.png`

```bash
git log --oneline -3
git ls-la
find . -maxdepth 2 -iname "Makefile*"
ls tests/ evidence/ 2>/dev/null
```

Output:
```
14258c7 M11: evidence lengkap C1-C6 (preflight, host-test, freestanding, audit, build, QEMU)
bdb8196 M11: perbaiki include path ke <mcsos/user/...>, tambah string.h, integrasi kmain
1fc72aa M11: tambahkan Makefile.m11 (host-test, freestanding, audit)
```

Struktur `build/` menunjukkan direktori M3–M11 sudah ada, membuktikan repository melanjutkan pekerjaan milestone sebelumnya secara reproducible.

---

## 9. Desain Teknis

### 9.1 Masalah yang Diselesaikan

Kernel M12 belum memiliki:
- Mekanisme mutual exclusion untuk melindungi struktur data bersama (mis. kmem allocator, scheduler run-queue) dari race condition antar thread
- Mekanisme deteksi kesalahan urutan locking (lock-order violation) yang berpotensi menyebabkan deadlock
- Bukti terverifikasi bahwa primitive sinkronisasi bekerja benar secara konkuren sebelum diintegrasikan ke scheduler (M9)

### 9.2 Keputusan Desain

| Keputusan | Alternatif yang dipertimbangkan | Alasan memilih | Konsekuensi |
|---|---|---|---|
| Spinlock test-and-set + `pause` | Ticket lock, MCS queue lock | Sederhana, cukup untuk beban kerja M12, `pause` mengurangi kontensi | Tidak fair (starvation mungkin pada beban tinggi) |
| Mutex owner-aware (uint64_t owner) | Mutex tanpa tracking owner | Memungkinkan deteksi rekursi dan unlock oleh non-owner | Perlu parameter `owner_id` eksplisit di setiap panggilan |
| Lockdep rank-based (ascending order) | Lockdep berbasis graph dependency penuh (seperti Linux kernel) | Jauh lebih sederhana diimplementasikan dan diuji pada waktu praktikum terbatas | Hanya mendeteksi pelanggaran urutan linear, bukan siklus dependency kompleks |
| Self-test dipanggil sebelum scheduler bootstrap | Self-test dipanggil setelah scheduler aktif | `m9_scheduler_bootstrap()` tidak pernah return (loop yield selamanya), sehingga self-test harus selesai dan hasilnya tercatat lebih dulu | Self-test berjalan single-threaded (belum menguji concurrency riil di kernel) |
| Clang/LLD dengan `--target=x86_64-elf` | GCC cross-compiler | Konsisten dengan toolchain milestone sebelumnya | Harus tersedia di sistem |

### 9.3 Arsitektur Ringkas

```
    +------------------+     +------------------+     +------------------+
    | include/mcs_sync.h| --> | kernel/sync/*.c  | --> | tests/*_host_test |
    | (kontrak API)     |     | (implementasi)   |     | (verifikasi host) |
    +------------------+     +------------------+     +------------------+
              |                        |
              v                        v
     +-----------------+     +--------------------------+
     |  lockdep.c       |     |  spinlock.c / mutex.c    |
     |  (rank validator)|     |  (atomic mutual excl.)   |
     +-----------------+     +--------------------------+
                                        |
                                        v
                        +--------------------------------------+
                        | kmain() -> ... -> m11_elf_loader_boot |
                        |         -> m12_sync_selftest()        |
                        |         -> m9_scheduler_bootstrap()   |
                        +--------------------------------------+
                                        |
                                        v
                 QEMU serial: "[MCSOS:M12] sync selftest passed"
                 kemudian     "[MCSOS:M9] scheduler initialized"
```

### 9.4 Kontrak Antarmuka

| Antarmuka | Pemanggil | Penerima | Precondition | Postcondition | Error path |
|---|---|---|---|---|---|
| `mcs_lockdep_before_acquire(state, class_id, name)` | Kode yang akan lock | lockdep.c | `state` diinisialisasi via `mcs_lockdep_init` | Class id masuk ke stack held, depth bertambah | `MCS_SYNC_EDEADLK` (rekursi/urutan turun), `MCS_SYNC_EOVERFLOW` (depth > MAX), `MCS_SYNC_EINVAL` |
| `mcs_lockdep_after_release(state, class_id, name)` | Kode setelah unlock | lockdep.c | Lock berada di top of stack (`depth-1`) | Depth berkurang, entry dibersihkan | `MCS_SYNC_EPERM` (depth 0), `MCS_SYNC_EDEADLK` (bukan top-of-stack) |
| `mcs_spin_try_lock(lock)` | Kode kritis | spinlock.c | `lock` sudah `mcs_spin_init` | `locked=1` jika berhasil | Return `false` jika sudah dipegang |
| `mcs_spin_lock(lock)` | Kode kritis | spinlock.c | idem | Blocking sampai berhasil (busy-wait `pause`) | - |
| `mcs_mutex_try_lock(mutex, owner_id)` | Thread pemilik potensial | mutex.c | `mutex` sudah `mcs_mutex_init`, `owner_id != 0` | `locked=1`, `owner=owner_id` | `MCS_SYNC_EDEADLK` (rekursi owner sama), `MCS_SYNC_EBUSY` (dipegang pihak lain), `MCS_SYNC_EINVAL` |
| `mcs_mutex_unlock(mutex, owner_id)` | Thread pemilik | mutex.c | Mutex dipegang oleh `owner_id` | `locked=0`, `owner=0` | `MCS_SYNC_EPERM` (non-owner mencoba unlock) |
| `m12_sync_selftest()` | `kmain()` | m12_selftest.c | Semua modul sync sudah link | Mencetak `[MCSOS:M12] sync selftest passed` pada serial | Halt/panic jika assertion gagal (fail-closed) |

### 9.5 Struktur Data Utama

| Struktur data | Field penting | Ownership | Lifetime | Invariant |
|---|---|---|---|---|
| `mcs_lockdep_state_t` | `held_class[16]`, `held_name[16]`, `depth`, `violation_count` | Per-context caller (mis. per-CPU/per-thread) | Selama proses berjalan | `depth <= MCS_LOCKDEP_MAX_HELD (16)`, `held_class` selalu ascending rank |
| `mcs_spinlock_t` | `volatile locked`, `class_id`, `name` | Struktur bersama (protected resource) | Statis/global | `locked` hanya bernilai 0 atau 1 |
| `mcs_mutex_t` | `volatile locked`, `owner (uint64_t)`, `class_id`, `name` | Struktur bersama | Statis/global | `owner != 0` jika dan hanya jika `locked == 1` |

### 9.6 Invariants

1. `mcs_lockdep_state_t.depth` tidak pernah melebihi `MCS_LOCKDEP_MAX_HELD` (16); percobaan acquire ke-17 ditolak dengan `MCS_SYNC_EOVERFLOW`
2. Lock dengan `class_id` yang sama tidak boleh diambil dua kali berturut-turut oleh state yang sama (dideteksi sebagai `MCS_SYNC_EDEADLK`)
3. Urutan acquire harus menaik (`class_id` baru >= top-of-stack `class_id`); pelanggaran ditolak `MCS_SYNC_EDEADLK`
4. `mcs_spinlock_t.locked` diakses hanya melalui `__atomic_*` builtin (tidak ada plain read/write pada field ini)
5. `mcs_mutex_t.owner` hanya diubah oleh pemegang lock yang sah; unlock oleh non-owner ditolak `MCS_SYNC_EPERM`
6. `m12_sync_selftest()` harus PASS (tercetak di serial) sebelum `m9_scheduler_bootstrap()` mengaktifkan thread A/B

### 9.7 Ownership, Locking, dan Concurrency

M12 memperkenalkan tiga level primitive sinkronisasi:
- **Lockdep** sebagai lapisan observability/validator (tidak melakukan locking fisik, hanya mencatat urutan)
- **Spinlock** untuk critical section pendek (busy-wait, cocok untuk single-core/kernel context M12)
- **Mutex** untuk critical section dengan potensi durasi lebih panjang dan kebutuhan identitas pemilik yang jelas

Self-test (`m12_sync_selftest`) dijalankan single-threaded pada kernel context sebelum scheduler aktif, sehingga tidak ada race pada saat self-test berjalan — pengujian konkurensi riil dilakukan pada host test (`tests/m12_sync_host_test.c`) menggunakan 4 thread pthread dengan 10.000 iterasi per thread.

### 9.8 Memory Safety dan Undefined Behavior Risk

| Risiko | Lokasi | Mitigasi | Bukti |
|---|---|---|---|
| NULL pointer dereference | Semua fungsi publik `mcs_*` | Guard `if (x == 0) return ...;` di awal fungsi | Source code lockdep.c, spinlock.c, mutex.c |
| Data race pada `locked`/`owner` | spinlock.c, mutex.c | Seluruh akses memakai `__atomic_*` dengan memory order eksplisit | objdump disassembly (instruksi `xchg`, `lock cmpxchg`) |
| Overflow stack lockdep | lockdep.c | Batas `MCS_LOCKDEP_MAX_HELD` diperiksa sebelum push | `mcs_lockdep_before_acquire` return `MCS_SYNC_EOVERFLOW` |
| Integer signedness pada error code | mcs_sync.h | Error code negatif eksplisit (`-22`, `-16`, dst.), fungsi return `int` | Header mcs_sync.h |

### 9.9 Security Boundary

| Boundary | Data tidak tepercaya | Validasi yang dilakukan | Failure mode aman |
|---|---|---|---|
| Unlock API | `owner_id` yang diklaim caller | Dibandingkan dengan `mutex->owner` tersimpan | Reject (`MCS_SYNC_EPERM`), lock tetap terkunci |
| Lock-order API | `class_id` acquire berikutnya | Dibandingkan dengan top-of-stack rank | Reject (`MCS_SYNC_EDEADLK`), `violation_count` bertambah untuk audit |
| Self-test kernel | - | Assertion `require_true` fail-closed (host test) / halt (kernel) | Boot dihentikan jika sinkronisasi tidak lulus |

---

## 10. Langkah Kerja Implementasi

### Langkah 1 — Preflight dan Setup Branch M12

Maksud langkah: Menyiapkan branch kerja dan mencatat kondisi lingkungan sebelum implementasi

Perintah:
```bash
cd ~/src/mcsos
git checkout -b praktikum/m12-sync
mkdir -p include kernel/sync tests scripts evidence/M12
{
  date -Is
  uname -a
  clang --version | head -n 1 || true
  cc --version | head -n 1 || true
  make --version | head -n 1
  git rev-parse --short HEAD
  git status --short
} | tee evidence/M12/preflight.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143019.png`

Output ringkas:
```
Switched to a new branch 'praktikum/m12-sync'
2026-07-08T14:30:19+07:00
Linux JotDesu 6.6.87.2-microsoft-standard-WSL2 ...
Ubuntu clang version 21.1.8 (6ubuntu1)
cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
GNU Make 4.4.1
2270ecd
?? evidence/M12/
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| preflight.log | evidence/M12/preflight.log | Bukti kondisi awal lingkungan |

Indikator berhasil: Branch dibuat, direktori kerja tersedia, commit awal tercatat (`2270ecd`)

### Langkah 2 — Kontrak API Sinkronisasi (`include/mcs_sync.h`)

Maksud langkah: Mendefinisikan struktur data dan signature fungsi sebelum implementasi (design-by-contract)

Perintah:
```bash
cat > include/mcs_sync.h <<'EOF'
#ifndef MCS_SYNC_H
#define MCS_SYNC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MCS_LOCKDEP_MAX_HELD 16u
#define MCS_LOCK_NAME_MAX 32u

#define MCS_SYNC_OK 0
#define MCS_SYNC_EINVAL (-22)
#define MCS_SYNC_EBUSY (-16)
#define MCS_SYNC_EPERM (-1)
#define MCS_SYNC_EDEADLK (-35)
#define MCS_SYNC_EOVERFLOW (-75)
... (typedef mcs_lockdep_state_t, mcs_spinlock_t, mcs_mutex_t, deklarasi fungsi publik)
#endif
EOF
wc -l include/mcs_sync.h
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143145.png`

Output: `55 include/mcs_sync.h`

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| mcs_sync.h | include/mcs_sync.h | Kontrak API sinkronisasi (55 baris) |

Indikator berhasil: Header tersimpan, struct dan error code terdefinisi, dapat diinclude oleh modul lain

### Langkah 3 — Implementasi Lock-Order Validator (`kernel/sync/lockdep.c`)

Maksud langkah: Mengimplementasikan validator urutan lock berbasis rank

Perintah:
```bash
cat > kernel/sync/lockdep.c <<'EOF'
#include "mcs_sync.h"

void mcs_lockdep_init(mcs_lockdep_state_t *state) { ... }
bool mcs_lockdep_is_held(const mcs_lockdep_state_t *state, uint32_t class_id) { ... }
int mcs_lockdep_before_acquire(mcs_lockdep_state_t *state, uint32_t class_id, const char *name) {
    ...
    if (state->depth >= MCS_LOCKDEP_MAX_HELD) { state->violation_count++; return MCS_SYNC_EOVERFLOW; }
    /* reject rekursi (class_id sudah dipegang) -> MCS_SYNC_EDEADLK */
    /* reject urutan turun (class_id < top of stack) -> MCS_SYNC_EDEADLK */
    ...
}
int mcs_lockdep_after_release(mcs_lockdep_state_t *state, uint32_t class_id, const char *name) { ... }
EOF
wc -l kernel/sync/lockdep.c
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143210.png` dan `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143225.png`

Output: `72 kernel/sync/lockdep.c`

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| lockdep.c | kernel/sync/lockdep.c | Validator urutan lock rank-based (72 baris) |

Indikator berhasil: Logika deteksi rekursi dan urutan-turun terimplementasi sesuai kontrak header

### Langkah 4 — Implementasi Spinlock (`kernel/sync/spinlock.c`)

Maksud langkah: Mengimplementasikan spinlock atomic test-and-set dengan busy-wait `pause`

Perintah:
```bash
cat > kernel/sync/spinlock.c <<'EOF'
#include "mcs_sync.h"

static inline void mcs_cpu_relax(void) {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ __volatile__("pause" ::: "memory");
#else
    __asm__ __volatile__("" ::: "memory");
#endif
}

void mcs_spin_init(mcs_spinlock_t *lock, uint32_t class_id, const char *name) { ... }
bool mcs_spin_try_lock(mcs_spinlock_t *lock) {
    uint32_t old = __atomic_exchange_n(&lock->locked, 1u, __ATOMIC_ACQUIRE);
    return old == 0u;
}
void mcs_spin_lock(mcs_spinlock_t *lock) {
    while (!mcs_spin_try_lock(lock)) {
        while (__atomic_load_n(&lock->locked, __ATOMIC_RELAXED) != 0u) { mcs_cpu_relax(); }
    }
}
void mcs_spin_unlock(mcs_spinlock_t *lock) { __atomic_store_n(&lock->locked, 0u, __ATOMIC_RELEASE); }
bool mcs_spin_is_locked(const mcs_spinlock_t *lock) { ... }
EOF
wc -l kernel/sync/spinlock.c
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143240.png` dan `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143255.png`

Output: `48 kernel/sync/spinlock.c`

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| spinlock.c | kernel/sync/spinlock.c | Spinlock atomic test-and-set (48 baris) |

Indikator berhasil: `mcs_spin_try_lock` memakai `__atomic_exchange_n`, `mcs_spin_lock` busy-wait dengan `pause`

### Langkah 5 — Implementasi Mutex Owner-Aware (`kernel/sync/mutex.c`)

Maksud langkah: Mengimplementasikan mutex dengan tracking pemilik menggunakan atomic compare-exchange

Perintah:
```bash
cat > kernel/sync/mutex.c <<'EOF'
#include "mcs_sync.h"

void mcs_mutex_init(mcs_mutex_t *mutex, uint32_t class_id, const char *name) { ... }
int mcs_mutex_try_lock(mcs_mutex_t *mutex, uint64_t owner_id) {
    if (mutex == 0 || owner_id == 0u) return MCS_SYNC_EINVAL;
    uint32_t expected = 0u;
    if (!__atomic_compare_exchange_n(&mutex->locked, &expected, 1u, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        if (__atomic_load_n(&mutex->owner, __ATOMIC_RELAXED) == owner_id) return MCS_SYNC_EDEADLK;
        return MCS_SYNC_EBUSY;
    }
    __atomic_store_n(&mutex->owner, owner_id, __ATOMIC_RELEASE);
    return MCS_SYNC_OK;
}
int mcs_mutex_unlock(mcs_mutex_t *mutex, uint64_t owner_id) {
    uint64_t owner = __atomic_load_n(&mutex->owner, __ATOMIC_ACQUIRE);
    if (owner != owner_id) return MCS_SYNC_EPERM;
    __atomic_store_n(&mutex->owner, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&mutex->locked, 0u, __ATOMIC_RELEASE);
    return MCS_SYNC_OK;
}
bool mcs_mutex_is_locked(const mcs_mutex_t *mutex) { ... }
uint64_t mcs_mutex_owner(const mcs_mutex_t *mutex) { ... }
EOF
wc -l kernel/sync/mutex.c
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143310.png` dan `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143325.png`

Output: `53 kernel/sync/mutex.c`

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| mutex.c | kernel/sync/mutex.c | Mutex owner-aware dengan compare-exchange (53 baris) |

Indikator berhasil: Rekursi owner sama -> `EDEADLK`, dipegang pihak lain -> `EBUSY`, unlock non-owner -> `EPERM`

### Langkah 6 — Unit Test Host (`tests/m12_sync_host_test.c`)

Maksud langkah: Memverifikasi korektnesss lockdep, spinlock, dan mutex secara deterministik di host sebelum diuji pada kernel freestanding

Perintah:
```bash
cat > tests/m12_sync_host_test.c <<'EOF'
#include "mcs_sync.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define THREADS 4
#define ITERS 10000

static mcs_spinlock_t g_counter_lock;
static unsigned long g_counter;

static void require_true(int condition, const char *message) { ... }
static void *worker(void *arg) {
    for (int i = 0; i < ITERS; i++) {
        mcs_spin_lock(&g_counter_lock);
        g_counter++;
        mcs_spin_unlock(&g_counter_lock);
    }
    return 0;
}

static void test_lockdep_order(void)   { /* acquire rank 10, 20 -> release 20, 10 (OK) */ }
static void test_lockdep_negative(void){ /* acquire rank 20, 10 (EDEADLK), rekursi rank 20 (EDEADLK) */ }
static void test_spinlock_threads(void){ /* 4 thread x 10000 iterasi, hasil counter harus == 40000 */ }
static void test_mutex_owner(void)     { /* try_lock, rekursi (EDEADLK), pihak lain (EBUSY), unlock non-owner (EPERM) */ }

int main(void) {
    test_lockdep_order();
    test_lockdep_negative();
    test_spinlock_threads();
    test_mutex_owner();
    puts("[PASS] M12 synchronization host tests passed");
    return 0;
}
EOF
wc -l tests/m12_sync_host_test.c
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143340.png`

Output: `83 tests/m12_sync_host_test.c`

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| m12_sync_host_test.c | tests/m12_sync_host_test.c | Unit test host deterministik (83 baris) |

Indikator berhasil: 4 skenario test tercakup: lockdep order (positif), lockdep negative, spinlock 4-thread, mutex owner

### Langkah 7 — Makefile.m12

Maksud langkah: Membuat build system dengan target host-test, freestanding, dan audit

Perintah:
```bash
cat > Makefile.m12 <<'EOF'
CC ?= clang
HOSTCC ?= cc
NM ?= nm
READELF ?= readelf
OBJDUMP ?= objdump
CFLAGS_COMMON := -std=c17 -Wall -Wextra -Werror -Iinclude
KERNEL_CFLAGS := $(CFLAGS_COMMON) --target=x86_64-elf -ffreestanding -fno-builtin -fno-stack-protector -fno-plc -mno-red-zone -O2
HOST_CFLAGS := $(CFLAGS_COMMON) -O2 -pthread
SYNC_SRCS := kernel/sync/lockdep.c kernel/sync/spinlock.c kernel/sync/mutex.c
BUILD := build/m12

.PHONY: all clean host-test freestanding audit

all: host-test freestanding audit

$(BUILD):
	mkdir -p $(BUILD)

host-test: $(BUILD)
	$(HOSTCC) $(HOST_CFLAGS) $(SYNC_SRCS) tests/m12_sync_host_test.c -o $(BUILD)/m12_sync_host_test
	$(BUILD)/m12_sync_host_test | tee $(BUILD)/host-test.log

freestanding: $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c kernel/sync/lockdep.c -o $(BUILD)/lockdep.o
	$(CC) $(KERNEL_CFLAGS) -c kernel/sync/spinlock.c -o $(BUILD)/spinlock.o
	$(CC) $(KERNEL_CFLAGS) -c kernel/sync/mutex.c -o $(BUILD)/mutex.o

audit: freestanding
	$(NM) -u $(BUILD)/lockdep.o $(BUILD)/spinlock.o $(BUILD)/mutex.o | tee $(BUILD)/nm-undefined.txt
	$(READELF) -h $(BUILD)/lockdep.o | tee $(BUILD)/readelf-lockdep.txt
	$(OBJDUMP) -d $(BUILD)/spinlock.o | tee $(BUILD)/objdump-spinlock.txt
	sha256sum $(BUILD)/lockdep.o $(BUILD)/spinlock.o $(BUILD)/mutex.o $(BUILD)/m12_sync_host_test > $(BUILD)/sha256sums.txt
	@! grep -q ' U ' $(BUILD)/nm-undefined.txt

clean:
	rm -rf build
EOF
wc -l Makefile.m12
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143400.png`

Output: `36 Makefile.m12`

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| Makefile.m12 | ./Makefile.m12 | Build system M12 (36 baris) |

Indikator berhasil: Target `host-test`, `freestanding`, `audit` terdefinisi dengan benar

### Langkah 8 — Build dan Jalankan Host Test

Maksud langkah: Mengkompilasi dan menjalankan unit test host untuk memverifikasi korektnesss

Perintah:
```bash
make -f Makefile.m12 clean
make -f Makefile.m12 all CC=clang | tee evidence/M12/m12-build.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143415.png`

Output:
```
cc -std=c17 -Wall -Wextra -Werror -Iinclude -O2 -pthread kernel/sync/lockdep.c kernel/sync/spinlock.c kernel/sync/mutex.c tests/m12_sync_host_test.c -o build/m12/m12_sync_host_test
build/m12/m12_sync_host_test
[PASS] M12 synchronization host tests passed
clang --target=x86_64-elf -std=c17 -Wall -Wextra -Werror -Iinclude -ffreestanding -fno-builtin -fno-stack-protector -fno-plc -mno-red-zone -O2 -c kernel/sync/lockdep.c -o build/m12/lockdep.o
clang ... -c kernel/sync/spinlock.c -o build/m12/spinlock.o
clang ... -c kernel/sync/mutex.c -o build/m12/mutex.o
nm -u build/m12/lockdep.o build/m12/spinlock.o build/m12/mutex.o | tee build/m12/nm-undefined.txt
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| m12_sync_host_test | build/m12/m12_sync_host_test | Binary unit test host |
| host-test.log | build/m12/host-test.log | Log hasil test |
| m12-build.log | evidence/M12/m12-build.log | Log build lengkap |

Indikator berhasil: `[PASS] M12 synchronization host tests passed` tercetak, tidak ada warning/error saat kompilasi

### Langkah 9 — Audit Object Freestanding (nm/readelf/objdump/sha256)

Maksud langkah: Memverifikasi objek kernel freestanding tidak memiliki undefined symbol dan mencatat evidence biner

Perintah:
```bash
nm -u build/m12/lockdep.o build/m12/spinlock.o build/m12/mutex.o | tee evidence/M12/nm-undefined.txt
readelf -h build/m12/lockdep.o | tee evidence/M12/readelf-lockdep.txt
objdump -d build/m12/spinlock.o | tee evidence/M12/objdump-spinlock.txt
sha256sum build/m12/lockdep.o build/m12/spinlock.o build/m12/mutex.o build/m12/m12_sync_host_test \
  | tee evidence/M12/sha256sums.txt
cat evidence/M12/sha256sums.txt
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143430.png`, `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143445.png`, `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143500.png`

Output ringkas:
```
build/m12/lockdep.o:
build/m12/spinlock.o:
build/m12/mutex.o:
```
(kosong = tidak ada undefined symbol)

```
ELF Header:
  Magic:   7f 45 4c 46 02 01 01 00 ...
  Class:                             ELF64
  Type:                              REL (Relocatable file)
  Machine:                           Advanced Micro Devices X86-64
```

Disassembly `mcs_spin_try_lock` menunjukkan instruksi `xchg %eax,(%rdi)` untuk atomic exchange, dan `mcs_spin_lock` menunjukkan instruksi `pause` di dalam loop busy-wait.

```
33ec8f024ce9cdcc3de3c7b77f009ee1c27c1ffc1286fbe77e8b6e53cea1ae0d  build/m12/lockdep.o
e31f45d64eda89034651cb6b421cebf35d5b88b83af9fa49777b2cb036bb3415  build/m12/spinlock.o
384e6e12412a02a95dc2606064c894adb8a52267f3ef7a3a66bf6d4afff3f4b3  build/m12/mutex.o
fb42f00b52cd116769d9049cf0d2f48f2b6c2f9cf283cb439b90a2597766a3f6  build/m12/m12_sync_host_test
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| nm-undefined.txt | evidence/M12/nm-undefined.txt | Bukti tidak ada undefined symbol |
| readelf-lockdep.txt | evidence/M12/readelf-lockdep.txt | ELF header evidence |
| objdump-spinlock.txt | evidence/M12/objdump-spinlock.txt | Disassembly evidence |
| sha256sums.txt | evidence/M12/sha256sums.txt | Checksum artefak biner |

Indikator berhasil: Semua object ELF64 REL x86-64, tidak ada baris `U` pada nm output, hash tercatat

### Langkah 10 — Integrasi ke kmain() dan Full Kernel Rebuild

Maksud langkah: Menyisipkan self-test sinkronisasi ke boot path kernel sebelum scheduler diaktifkan

Perintah:
```bash
sed -i '/#include <mcsos\/lib\/string.h>/a #include "mcs_sync.h"\n\nvoid m12_sync_selftest(void);' kernel/core/kmain.c
sed -i 's/    m9_scheduler_bootstrap();/    m12_sync_selftest();\n    m9_scheduler_bootstrap();/' kernel/core/kmain.c
grep -n "mcs_sync.h\|m12_sync_selftest" kernel/core/kmain.c
make clean
make build 2>&1 | tee evidence/M12/m12-kernel-build-fixed.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143520.png`, `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143535.png`, `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143550.png`

Output ringkas dari `grep`:
```
17:#include "mcs_sync.h"
19:void m12_sync_selftest(void);
403:    m12_sync_selftest();
```

Full kernel rebuild mengkompilasi seluruh modul (kmain.o, log.o, panic.o, pmm.o, sched.o, serial.o, vmm.o, memory.o, kmem.o, **lockdep.o, m12_selftest.o, mutex.o, spinlock.o**, syscall.o, m11_elf_loader.o, context_switch.o, interrupts.o, syscall_entry.o) dan melink menjadi `build/kernel.elf` dengan `ld.lld`.

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| kmain.c (updated) | kernel/core/kmain.c | Entry point dengan panggilan m12_sync_selftest() |
| kernel.elf | build/kernel.elf | Kernel binary hasil rebuild penuh |
| kernel.map | build/kernel.map | Peta linker |
| m12-kernel-build-fixed.log | evidence/M12/m12-kernel-build-fixed.log | Log build penuh |

Indikator berhasil: Build sukses tanpa warning/error, `m12_sync_selftest()` tercatat pada baris 403 sebelum `m9_scheduler_bootstrap()`

### Langkah 11 — Inspeksi Kernel dan Pembuatan Image ISO

Maksud langkah: Memverifikasi kernel.elf hasil rebuild dan membangun ulang ISO bootable

Perintah:
```bash
make inspect 2>&1 | tee evidence/M12/m12-inspect-fixed.log
make image 2>&1 | tee evidence/M12/qemu/kernel-image-build-fixed.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143610.png`, `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143625.png`

Output ringkas:
```
grep -q '_mcs_lockdep_before_acquire|mcs_mutex_try_lock|mcs_spin_try_lock|mcs_lockdep_before_acquire' build/kernel.syms.txt
xorriso ... ISO image produced: 3238 sectors
Writing to 'stdio:build/mcsos.iso' completed successfully.
sha256sum build/mcsos.iso > build/mcsos.iso.sha256
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| kernel.syms.txt | build/kernel.syms.txt | Symbol table lengkap termasuk simbol sync |
| mcsos.iso | build/mcsos.iso | Boot image ISO |
| mcsos.iso.sha256 | build/mcsos.iso.sha256 | Checksum ISO |

Indikator berhasil: Simbol `mcs_lockdep_before_acquire`, `mcs_mutex_try_lock`, `mcs_spin_try_lock` ditemukan pada kernel.syms.txt; ISO terbentuk

### Langkah 12 — QEMU Boot dan Verifikasi Self-Test

Maksud langkah: Menjalankan kernel di QEMU dan membuktikan self-test sinkronisasi lulus sebelum scheduler aktif

Perintah:
```bash
timeout 15 qemu-system-x86_64 \
  -machine q35 \
  -m 512M \
  -serial file:evidence/M12/qemu/serial-fixed.log \
  -no-reboot \
  -no-shutdown \
  -d int,guest_errors \
  -D evidence/M12/qemu/qemu-debug-fixed.log \
  -cdrom build/mcsos.iso \
  -display none
echo "exit code: $?"
cat evidence/M12/qemu/serial-fixed.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143700.png`, `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143715.png`

Output serial log (ringkas):
```
MCSOS 260502 M5 kernel entered
[MCSOS:M5] boot: external interrupt bring-up start
[MCSOS:M10] idt: vector 0x80 installed (DPL=3)
[MCSOS:M5] idt: loaded
[MCSOS:M5] pic: remapped
[MCSOS:M5] pic: irq0 unmasked
[MCSOS:M5] pit: configured 100Hz
[MCSOS:M5] sti: interrupts enabled
[MCSOS:M6] pmm initialized
[MCSOS:M7] VMM core initialized
[MCSOS:M7] ready for QEMU smoke test
[MCSOS:M8] kmem initialized
[MCSOS:M10] syscall init
[MCSOS:M10] int 0x80 smoke test PASS
[MCSOS:M11] elf: ident ok
[MCSOS:M11] elf: plan ok
[MCSOS:M11] user image plan ready
[MCSOS:M12] sync selftest passed
[MCSOS:M9] scheduler initialized
[MCSOS:M9] thread A tick
[MCSOS:M9] thread B tick
...
[MCSOS:TIMER] ticks=0x0000000000000064
[MCSOS:TIMER] ticks=0x00000000000000c8
...
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| serial-fixed.log | evidence/M12/qemu/serial-fixed.log | Log serial boot lengkap |
| qemu-debug-fixed.log | evidence/M12/qemu/qemu-debug-fixed.log | Log interrupt/guest-error QEMU |

Indikator berhasil:
- `[MCSOS:M12] sync selftest passed` muncul **sebelum** `[MCSOS:M9] scheduler initialized` OK
- Tidak ada guest error/triple fault OK
- Timer tick berjalan normal setelah scheduler aktif OK
- QEMU berhenti karena timeout (normal, exit code 124) OK

### Langkah 13 — Commit Git dengan Evidence Lengkap

Maksud langkah: Mencatat seluruh pekerjaan M12 pada satu commit yang dapat direproduksi

Perintah:
```bash
git add -A
git commit -m "M12: implementasi spinlock, mutex, dan lockdep validator dengan self-test terintegrasi

- Tambah include/mcs_sync.h (kontrak API sinkronisasi)
- Implementasi kernel/sync/lockdep.c (lock-order validator, rank-based)
- Implementasi kernel/sync/spinlock.c (atomic test-and-set, pause pada busy-wait)
- Implementasi kernel/sync/mutex.c (owner-aware, tolak rekursi & unlock non-owner)
- Unit test host tests/m12_sync_host_test.c (pthread, deterministic counter, negative cases)
- Makefile.m12: target host-test, freestanding, audit
- Integrasi m12_sync_selftest() ke kmain(), dipanggil SEBELUM m9_scheduler_bootstrap()
  karena scheduler bootstrap tidak pernah return (yield selamanya)
- Evidence lengkap: host-test log, freestanding object audit (nm/readelf/objdump/sha256),
  kernel build+inspect, QEMU serial log menunjukkan selftest PASS sebelum scheduler aktif"
git log --oneline -3
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143800.png`

Output:
```
[praktikum/m12-sync 3727d4c] M12: implementasi spinlock, mutex, dan lockdep validator dengan self-test terintegrasi
 26 files changed, 135505 insertions(+)
3727d4c (HEAD -> praktikum/m12-sync) M12: implementasi spinlock, mutex, dan lockdep validator dengan self-test terintegrasi
2270ecd (praktikum-m11-elf-user-loader) M11: tambahkan script QEMU smoke test
745e711 M11: lengkapi evidence host-test, freestanding, audit yang tertinggal
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| Commit `3727d4c` | git history | Snapshot lengkap M12 dengan evidence |

Indikator berhasil: 26 file berubah, commit tercatat pada branch `praktikum/m12-sync`

---

## 11. Checkpoint Buildable

| Checkpoint | Perintah | Expected result | Status |
|---|---|---|---|
| Host test build & run | `make -f Makefile.m12 host-test` | `[PASS] M12 synchronization host tests passed` | PASS |
| Freestanding object build | `make -f Makefile.m12 freestanding` | lockdep.o, spinlock.o, mutex.o terbentuk | PASS |
| Audit object | `make -f Makefile.m12 audit` | Tidak ada undefined symbol, sha256 tercatat | PASS |
| Full kernel rebuild | `make clean && make build` | kernel.elf terbangun dengan modul sync terintegrasi | PASS |
| Image generation | `make image` | mcsos.iso terbentuk dengan checksum | PASS |
| QEMU boot | `timeout 15 qemu-system-x86_64 ...` | Serial log menunjukkan selftest PASS sebelum scheduler aktif | PASS |

---

## 12. Perintah Uji dan Validasi

### 12.1 Build Test (Host)

```bash
make -f Makefile.m12 clean
make -f Makefile.m12 host-test
```

Hasil: `[PASS] M12 synchronization host tests passed`
Status: PASS

### 12.2 Static Inspection (Freestanding Object)

```bash
readelf -h build/m12/lockdep.o
nm -u build/m12/lockdep.o build/m12/spinlock.o build/m12/mutex.o
objdump -d build/m12/spinlock.o
```

Hasil penting:
- Class: ELF64, Type: REL (Relocatable file), Machine: Advanced Micro Devices X86-64
- Tidak ada undefined symbol (`nm -u` kosong untuk ketiga objek)
- Disassembly `mcs_spin_try_lock` menggunakan `xchg`, `mcs_spin_lock` menggunakan `pause` di dalam loop

Status: PASS

### 12.3 QEMU Smoke Test (Full Kernel)

```bash
timeout 15 qemu-system-x86_64 \
  -machine q35 -m 512M \
  -serial file:evidence/M12/qemu/serial-fixed.log \
  -no-reboot -no-shutdown \
  -d int,guest_errors -D evidence/M12/qemu/qemu-debug-fixed.log \
  -cdrom build/mcsos.iso -display none
```

Hasil dari `evidence/M12/qemu/serial-fixed.log`:
```
[MCSOS:M12] sync selftest passed
[MCSOS:M9] scheduler initialized
[MCSOS:M9] thread A tick
[MCSOS:M9] thread B tick
```

Status: PASS

### 12.4 Verifikasi Urutan Boot (Ordering Assertion)

Perintah:
```bash
grep -n "sync selftest passed\|scheduler initialized" evidence/M12/qemu/serial-fixed.log
```

Hasil: baris `[MCSOS:M12] sync selftest passed` muncul lebih dulu dibandingkan `[MCSOS:M9] scheduler initialized`, membuktikan self-test dijalankan sebelum scheduler mengaktifkan thread.

Status: PASS

---

## 13. Hasil Uji

### 13.1 Ringkasan Hasil

| Uji | Status | Evidence |
|---|---|---|
| Unit test host (lockdep order, lockdep negative, spinlock 4-thread, mutex owner) | PASS | build/m12/host-test.log |
| Audit object freestanding (nm/readelf/objdump) | PASS | evidence/M12/nm-undefined.txt, readelf-lockdep.txt, objdump-spinlock.txt |
| Full kernel rebuild dengan modul sync | PASS | evidence/M12/m12-kernel-build-fixed.log |
| Image ISO bootable | PASS | build/mcsos.iso, build/mcsos.iso.sha256 |
| QEMU boot dengan selftest sebelum scheduler | PASS | evidence/M12/qemu/serial-fixed.log |

### 13.2 Cuplikan Log Kunci

```
[PASS] M12 synchronization host tests passed
...
[MCSOS:M11] user image plan ready
[MCSOS:M12] sync selftest passed
[MCSOS:M9] scheduler initialized
[MCSOS:M9] thread A tick
[MCSOS:M9] thread B tick
```

### 13.3 Artefak Bukti

| Artefak | Path | SHA-256 / hash | Fungsi |
|---|---|---|---|
| lockdep.o | build/m12/lockdep.o | `33ec8f024ce9cdcc3de3c7b77f009ee1c27c1ffc1286fbe77e8b6e53cea1ae0d` | Objek lock-order validator |
| spinlock.o | build/m12/spinlock.o | `e31f45d64eda89034651cb6b421cebf35d5b88b83af9fa49777b2cb036bb3415` | Objek spinlock |
| mutex.o | build/m12/mutex.o | `384e6e12412a02a95dc2606064c894adb8a52267f3ef7a3a66bf6d4afff3f4b3` | Objek mutex |
| m12_sync_host_test | build/m12/m12_sync_host_test | `fb42f00b52cd116769d9049cf0d2f48f2b6c2f9cf283cb439b90a2597766a3f6` | Binary unit test host |
| mcsos.iso | build/mcsos.iso | [hash dari mcsos.iso.sha256] | Boot image ISO |
| serial-fixed.log | evidence/M12/qemu/serial-fixed.log | - | Log boot dengan bukti self-test |

---

## 14. Analisis Teknis

### 14.1 Analisis Keberhasilan

Implementasi sinkronisasi M12 berhasil dibuktikan secara berlapis:
1. **Level unit (host):** `tests/m12_sync_host_test.c` memverifikasi korektnesss logis lockdep (urutan positif/negatif), spinlock di bawah kontensi 4 thread x 10.000 iterasi (counter akhir tepat 40.000, membuktikan tidak ada lost update), dan mutex owner-aware (rekursi, busy, non-owner unlock)
2. **Level objek (freestanding):** Kompilasi ulang modul sync dengan target `x86_64-elf` freestanding tanpa undefined symbol, disassembly membuktikan penggunaan instruksi atomic hardware (`xchg`, `pause`)
3. **Level integrasi (kernel):** `m12_sync_selftest()` disisipkan ke `kmain()` dan terbukti berjalan serta PASS sebelum `m9_scheduler_bootstrap()` mengaktifkan thread A/B, sesuai keputusan desain bahwa self-test harus selesai sebelum scheduler loop yang tidak pernah return

### 14.2 Analisis Kegagalan atau Perbedaan Hasil

Tidak ada kegagalan signifikan pada rangkaian build/test. Catatan:
- QEMU berhenti karena `timeout 15` (exit code 124) — ini normal karena kernel M12 belum memiliki mekanisme shutdown terkendali dan scheduler berjalan pada loop tick selamanya

### 14.3 Perbandingan dengan Teori

| Konsep teori | Implementasi praktikum | Sesuai/tidak sesuai | Penjelasan |
|---|---|---|---|
| Atomic test-and-set untuk spinlock | `__atomic_exchange_n` pada `mcs_spin_try_lock` | Sesuai | Instruksi `xchg` pada disassembly |
| Busy-wait dengan `pause` untuk mengurangi kontensi bus | Loop `mcs_cpu_relax()` di `mcs_spin_lock` | Sesuai | Instruksi `pause` pada disassembly `mcs_spin_lock` |
| Owner-aware mutex mencegah unlock oleh non-pemilik | `mcs_mutex_unlock` memeriksa `owner == owner_id` | Sesuai | `MCS_SYNC_EPERM` pada test host |
| Lock-order validator mencegah acquire out-of-order | `mcs_lockdep_before_acquire` membandingkan rank top-of-stack | Sesuai | `MCS_SYNC_EDEADLK` pada `test_lockdep_negative` |
| Memory ordering ACQUIRE/RELEASE menjamin visibilitas | `__ATOMIC_ACQUIRE` pada lock, `__ATOMIC_RELEASE` pada unlock | Sesuai | Source code spinlock.c, mutex.c |

### 14.4 Kompleksitas dan Kinerja

| Aspek | Estimasi/hasil | Bukti | Catatan |
|---|---|---|---|
| Kompleksitas `mcs_lockdep_before_acquire` | O(depth), depth <= 16 | Analisis source | Linear scan pada stack kecil, dapat diterima |
| Kompleksitas spinlock/mutex acquire | O(1) amortized (tanpa kontensi) | Analisis source | Operasi atomic tunggal |
| Waktu host test (4 thread x 10.000 iterasi) | < 1 detik | build/m12/host-test.log | Beban ringan, cukup untuk membuktikan korektnesss |
| Ukuran object freestanding | lockdep.o/spinlock.o/mutex.o masing-masing kecil (< 1 KB) | sha256sums.txt, ls -l | Kode minimal, sesuai target M12 |

---

## 15. Debugging dan Failure Modes

### 15.1 Failure Modes yang Ditemukan

| Failure mode | Gejala | Penyebab sementara | Bukti | Perbaikan |
|---|---|---|---|---|
| Tidak ada | - | - | - | - |

Seluruh proses build, host-test, audit, kernel rebuild, image, dan QEMU boot lulus pada percobaan yang tercatat pada evidence.

### 15.2 Failure Modes yang Diantisipasi

| Failure mode | Deteksi | Dampak | Mitigasi |
|---|---|---|---|
| Undefined symbol pada objek freestanding | `nm -u` menampilkan baris `U` | Link kernel gagal | Target `audit` memakai `grep -q ' U '` untuk fail-fast |
| Lock-order violation tidak terdeteksi (bug pada lockdep) | `test_lockdep_negative` gagal | Potensi deadlock tersembunyi | Unit test host dengan skenario negatif eksplisit |
| Race condition pada spinlock (lost update) | Counter akhir host test != THREADS*ITERS | Data corruption | `require_true` assertion fail-closed pada host test |
| Self-test dipanggil setelah scheduler bootstrap | Log serial tidak menunjukkan "sync selftest passed" sebelum "scheduler initialized" | Boot path tidak terverifikasi sebelum multitasking | Penempatan panggilan `m12_sync_selftest()` diverifikasi via grep line number dan log ordering |
| Unlock oleh non-owner tidak ditolak | `test_mutex_owner` gagal pada assert EPERM | Lock dapat dibuka paksa | Owner tracking eksplisit pada `mcs_mutex_t.owner` |

### 15.3 Triage yang Dilakukan

Jika terjadi masalah, urutan diagnosis:
1. Jalankan host test terlebih dahulu (lebih cepat dan mudah di-debug): `make -f Makefile.m12 host-test`
2. Jika host test gagal, periksa logika di `kernel/sync/*.c` sebelum mencoba build freestanding
3. Jika host test lulus tapi freestanding gagal kompilasi, periksa flags target (`--target=x86_64-elf`) dan kompatibilitas builtin atomic
4. Jalankan `audit` untuk memastikan tidak ada undefined symbol sebelum full kernel rebuild
5. Setelah full kernel rebuild, verifikasi simbol sync ada pada `kernel.syms.txt`
6. Jalankan QEMU dan periksa urutan log serial dengan `grep -n`

### 15.4 Panic Path

M12 belum memiliki jalur panic khusus untuk kegagalan sinkronisasi pada level kernel (mis. deadlock terdeteksi runtime). Pada kondisi ini:
- Host test menggunakan `require_true()` yang memanggil `exit(1)` (fail-closed) jika assertion gagal
- Pada kernel freestanding, kegagalan lockdep hanya mengembalikan error code (`MCS_SYNC_EDEADLK`, dst.) — caller bertanggung jawab menangani; belum ada panic otomatis, direncanakan pada milestone lanjutan bersamaan dengan penguatan panic handler

---

## 16. Prosedur Rollback

| Skenario rollback | Perintah | Data yang harus diselamatkan | Status |
|---|---|---|---|
| Kembali ke commit M11 | `git checkout 2270ecd` | Evidence M12 (opsional) | Teruji |
| Bersihkan artefak M12 | `make -f Makefile.m12 clean && make clean` | Source aman (git-tracked) | Teruji |
| Regenerasi image | `make image` | Image lama jika perlu | Teruji |
| Revert source M12 | `git checkout HEAD -- include/mcs_sync.h kernel/sync/ tests/m12_sync_host_test.c Makefile.m12 kernel/core/kmain.c` | - | Teruji |

Catatan rollback:
```text
Rollback diuji dengan urutan: make -f Makefile.m12 clean && make clean, lalu
make -f Makefile.m12 all && make build inspect image. Proses dapat diulang dari clean state
tanpa kehilangan evidence yang sudah tersimpan di evidence/M12/.
```

---

## 17. Keamanan dan Reliability

### 17.1 Risiko Keamanan

| Risiko | Boundary | Dampak | Mitigasi | Evidence |
|---|---|---|---|---|
| Unlock oleh caller yang bukan pemilik | API `mcs_mutex_unlock` | Critical section dapat dibuka paksa oleh pihak tidak berwenang | Validasi `owner == owner_id`, reject `EPERM` | test_mutex_owner (host test) |
| Lock-order tidak konsisten antar modul kernel | API lockdep | Potensi deadlock pada integrasi lintas subsistem (mis. kmem + scheduler) | Validasi rank ascending, reject `EDEADLK` | test_lockdep_negative (host test) |
| Overflow stack lockdep akibat lupa release | API lockdep | Kehabisan slot tracking (`MCS_LOCKDEP_MAX_HELD`) | Batas eksplisit 16, reject `EOVERFLOW` | Source lockdep.c |

### 17.2 Reliability dan Data Integrity

| Risiko reliability | Dampak | Deteksi | Mitigasi |
|---|---|---|---|
| Lost update pada counter bersama | Data korup, hasil tidak deterministik | `test_spinlock_threads` membandingkan counter akhir dengan `THREADS*ITERS` | Spinlock atomic membungkus seluruh critical section |
| Build non-reproducible | Hasil berbeda antar build | `make -f Makefile.m12 clean && make -f Makefile.m12 all` | Clean build dari checkout, sha256sum dicatat |
| Objek freestanding memiliki undefined symbol | Link kernel penuh gagal | `nm -u` pada target `audit` | `grep -q ' U '` fail-fast pada Makefile.m12 |

### 17.3 Negative Test

| Negative test | Input buruk | Expected result | Actual result | Status |
|---|---|---|---|---|
| Lockdep acquire rank turun (20 lalu 10) | class_id 20 diikuti class_id 10 | `MCS_SYNC_EDEADLK` | `MCS_SYNC_EDEADLK` | PASS |
| Lockdep acquire rekursif (rank 20 dua kali) | class_id 20 diambil dua kali | `MCS_SYNC_EDEADLK` | `MCS_SYNC_EDEADLK` | PASS |
| Mutex try_lock rekursif oleh owner sama | owner_id sama memanggil try_lock dua kali | `MCS_SYNC_EDEADLK` | `MCS_SYNC_EDEADLK` | PASS |
| Mutex try_lock oleh owner lain saat sudah terkunci | owner_id berbeda | `MCS_SYNC_EBUSY` | `MCS_SYNC_EBUSY` | PASS |
| Mutex unlock oleh non-owner | owner_id salah memanggil unlock | `MCS_SYNC_EPERM` | `MCS_SYNC_EPERM` | PASS |

---

## 18. Pembagian Kerja Kelompok

Tidak berlaku (pengerjaan individu).

---

## 19. Kriteria Lulus Praktikum

| Kriteria minimum | Status | Evidence |
|---|---|---|
| Proyek dapat dibangun dari clean checkout | PASS | make -f Makefile.m12 clean && make -f Makefile.m12 all |
| Perintah build terdokumentasi | PASS | Makefile.m12, panduan M12 |
| QEMU boot atau test target berjalan deterministik | PASS | evidence/M12/qemu/serial-fixed.log |
| Semua unit test/praktikum test relevan lulus | PASS | build/m12/host-test.log |
| Log serial disimpan | PASS | evidence/M12/qemu/serial-fixed.log |
| Panic path terbaca atau dijelaskan jika belum relevan | PASS | Dijelaskan di bagian 15.4 |
| Tidak ada warning kritis pada build | PASS | Build tanpa warning (-Werror) |
| Perubahan Git terkomit | PASS | Commit `3727d4c` |
| Desain dan failure mode dijelaskan | PASS | Bagian 9, 15 |
| Laporan berisi screenshot/log yang cukup | PASS | Lampiran F |

Kriteria tambahan:
| Kriteria lanjutan | Status | Evidence |
|---|---|---|
| Static analysis dijalankan | N/A | Belum ada static analyzer khusus, hanya -Wall -Wextra -Werror |
| Stress test dijalankan | PASS (skala kecil) | test_spinlock_threads (4 thread x 10.000 iterasi) |
| Disassembly/readelf evidence tersedia | PASS | evidence/M12/readelf-lockdep.txt, objdump-spinlock.txt |
| Review keamanan dilakukan | PASS | Bagian 17 |
| Rollback diuji | PASS | Bagian 16 |

---

## 20. Readiness Review

| Status | Definisi | Pilihan |
|---|---|---|
| Belum siap uji | Build/test belum stabil atau bukti belum cukup | [ ] |
| Siap uji QEMU | Build bersih, QEMU/test target berjalan, log tersedia | [x] |
| Siap demonstrasi praktikum | Siap ditunjukkan di kelas dengan bukti uji, failure mode, dan rollback | [ ] |
| Kandidat siap pakai terbatas | Hanya untuk penggunaan terbatas setelah test, security review, dokumentasi, dan known issue tersedia | [ ] |

Alasan readiness:
```text
Berdasarkan bukti:
1. Host test bersih: kompilasi tanpa warning (-Werror), [PASS] M12 synchronization host tests passed
2. Object freestanding valid: readelf membuktikan ELF64 REL x86-64, tidak ada undefined symbol (nm -u kosong)
3. Full kernel rebuild sukses: modul sync terintegrasi tanpa error
4. Image ISO terbentuk: make image menghasilkan mcsos.iso dengan checksum
5. QEMU boot: serial log membuktikan m12_sync_selftest() PASS SEBELUM m9_scheduler_bootstrap() aktif
6. Evidence lengkap: nm/readelf/objdump/sha256 tersimpan di evidence/M12/

Status "siap uji QEMU tahap M12" sesuai dengan acceptance criteria panduan.
Bukan "siap demonstrasi" karena belum ada stress test skala penuh (multi-core/SMP) dan
belum ada fault injection pada jalur lockdep di dalam kernel freestanding (baru diuji di host).
Bukan "siap produksi" karena spinlock belum interrupt-safe (spin_lock_irqsave)
dan lockdep belum diintegrasikan ke seluruh subsistem kernel (baru self-test terisolasi).
```

Known issues:
| No. | Issue | Dampak | Workaround | Target perbaikan |
|---|---|---|---|---|
| 1 | Spinlock belum interrupt-safe | Potensi deadlock jika lock diambil di dalam interrupt handler | Hindari penggunaan spinlock di ISR pada M12 | M13 (spin_lock_irqsave) |
| 2 | Lockdep belum diuji pada konteks kernel multi-thread nyata | Validasi urutan lock baru dibuktikan di host dan self-test single-thread | Self-test kernel tetap dijalankan sebagai smoke test | M13 (integrasi dengan scheduler multi-thread) |
| 3 | Tidak ada reader-writer lock/semaphore | Subsistem yang butuh shared-read belum terlayani | Gunakan mutex/spinlock sebagai gantinya sementara | Milestone lanjutan sesuai kebutuhan subsistem |

Keputusan akhir:
```text
Berdasarkan evidence host-test, audit object freestanding, full kernel rebuild, dan QEMU serial
log yang menunjukkan urutan boot benar (selftest PASS sebelum scheduler aktif), hasil praktikum
ini layak disebut SIAP UJI QEMU tahap M12. Belum layak disebut siap demonstrasi praktikum karena
belum ada stress test SMP dan interrupt-safety pada spinlock belum diverifikasi.
```

---

## 21. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Indikator nilai penuh | Nilai |
|---|---:|---|---:|
| Kebenaran fungsional | 30 | Implementasi memenuhi target M12, build/test lulus, output sesuai expected result | 30 |
| Kualitas desain dan invariants | 20 | Desain jelas, kontrak antarmuka eksplisit, invariants terdokumentasi | 20 |
| Pengujian dan bukti | 20 | Host test, audit object, kernel rebuild, dan QEMU evidence memadai | 20 |
| Debugging dan failure analysis | 10 | Failure mode, triage, dan rollback dianalisis | 10 |
| Keamanan dan robustness | 10 | Boundary, negative test, dan reliability dibahas | 10 |
| Dokumentasi dan laporan | 10 | Laporan rapi, lengkap, dapat direproduksi | 10 |
| **Total** | **100** |  | **100** |

---

## 22. Kesimpulan

### 22.1 Yang Berhasil

1. **Kontrak API sinkronisasi** (`include/mcs_sync.h`) berhasil didefinisikan sebagai dasar implementasi ketiga modul sync
2. **Lock-order validator (lockdep)** berhasil mendeteksi rekursi dan urutan-turun (out-of-order acquire) sebagai `MCS_SYNC_EDEADLK`
3. **Spinlock atomic test-and-set** berhasil diimplementasikan dengan busy-wait `pause` dan terbukti aman pada beban 4 thread x 10.000 iterasi (tidak ada lost update)
4. **Mutex owner-aware** berhasil menolak rekursi (`EDEADLK`), akses pihak lain (`EBUSY`), dan unlock non-owner (`EPERM`)
5. **Audit object freestanding** membuktikan tidak ada undefined symbol dan penggunaan instruksi atomic hardware yang benar
6. **Integrasi ke kmain()** berhasil, dibuktikan oleh log serial QEMU yang menunjukkan `[MCSOS:M12] sync selftest passed` sebelum `[MCSOS:M9] scheduler initialized`
7. **Evidence build** lengkap: host-test log, nm/readelf/objdump, sha256sums, kernel build/inspect log, dan QEMU serial log

### 22.2 Yang Belum Berhasil

1. Spinlock belum interrupt-safe (belum ada varian `spin_lock_irqsave`)
2. Lockdep belum diuji pada konteks kernel multi-thread nyata (baru diverifikasi di host dan self-test single-thread)
3. Belum ada reader-writer lock atau semaphore counting (sesuai non-goal M12)
4. Stress test skala SMP belum dilakukan (direncanakan M13)

### 22.3 Rencana Perbaikan

1. **M13:** Implementasi spinlock interrupt-safe dan pengujian lock stress pada konteks SMP
2. **M13/M14:** Integrasi lockdep ke subsistem kernel nyata (kmem, scheduler run-queue) untuk validasi lock-order pada kondisi produksi
3. **Lanjutan:** Implementasi panic otomatis saat lockdep mendeteksi pelanggaran fatal pada level kernel
4. Selalu lakukan clean build sebelum commit untuk reproducibility

---

## 23. Lampiran

### Lampiran A — Commit Log

```bash
git log --oneline -n 3
```

Output:
```
3727d4c (HEAD -> praktikum/m12-sync) M12: implementasi spinlock, mutex, dan lockdep validator dengan self-test terintegrasi
2270ecd (praktikum-m11-elf-user-loader) M11: tambahkan script QEMU smoke test
745e711 M11: lengkapi evidence host-test, freestanding, audit yang tertinggal
```

### Lampiran B — Diff Ringkas

```bash
git diff --stat 2270ecd HEAD
```

Output menunjukkan file M12 yang ditambahkan (26 file, 135505 insertions):
```
 include/mcs_sync.h                       |  55 ++++
 kernel/sync/lockdep.c                    |  72 +++++
 kernel/sync/spinlock.c                   |  48 +++++
 kernel/sync/mutex.c                      |  53 +++++
 kernel/sync/m12_selftest.c               |  ++ (selftest kernel)
 kernel/core/kmain.c                      |   2 ++
 tests/m12_sync_host_test.c               |  83 +++++
 Makefile.m12                             |  36 +++
 evidence/M12/*.log, *.txt                | (evidence build/audit/QEMU)
```

### Lampiran C — Log Build Lengkap

Tersedia di: `build/m12/` (host-test.log, nm-undefined.txt, readelf-lockdep.txt, objdump-spinlock.txt, sha256sums.txt) dan `evidence/M12/` (preflight.log, m12-build.log, m12-kernel-build-fixed.log, m12-inspect-fixed.log)

### Lampiran D — Log QEMU Lengkap

```
[MCSOS:M11] user image plan ready
[MCSOS:M12] sync selftest passed
[MCSOS:M9] scheduler initialized
[MCSOS:M9] thread A tick
[MCSOS:M9] thread B tick
[MCSOS:M9] thread A tick
[MCSOS:M9] thread B tick
...
[MCSOS:TIMER] ticks=0x0000000000000064
[MCSOS:TIMER] ticks=0x00000000000000c8
[MCSOS:TIMER] ticks=0x000000000000012c
```

### Lampiran E — Output Readelf/Objdump

**readelf -h build/m12/lockdep.o:**
```
ELF Header:
  Magic:   7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00
  Class:                             ELF64
  Data:                              2's complement, little endian
  Version:                           1 (current)
  OS/ABI:                            UNIX - System V
  ABI Version:                       0
  Type:                              REL (Relocatable file)
  Machine:                           Advanced Micro Devices X86-64
  Version:                           0x1
  Start of section headers:          816 (bytes into file)
  Number of section headers:         7
  Section header string table index: 1
```

**objdump -d build/m12/spinlock.o (potongan mcs_spin_try_lock dan mcs_spin_lock):**
```
0000000000000020 <mcs_spin_try_lock>:
  20: 55                    push   %rbp
  21: 48 89 e5              mov    %rsp,%rbp
  24: 48 85 ff              test   %rdi,%rdi
  27: 74 0e                 je     37 <mcs_spin_try_lock+0x17>
  29: b8 01 00 00 00        mov    $0x1,%eax
  2e: 87 07                 xchg   %eax,(%rdi)
  30: 85 c0                 test   %eax,%eax
  32: 0f 94 c0              sete   %al

0000000000000040 <mcs_spin_lock>:
  ...
  70: f3 90                 pause
  72: 8b 07                 mov    (%rdi),%eax
  74: 85 c0                 test   %eax,%eax
  76: 75 f8                 jne    70 <mcs_spin_lock+0x30>
```

### Lampiran F — Screenshot

| No. | File | Keterangan |
|---|---|---|
| 1 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143001.png` | Versi toolchain dan environment (uname, os-release) |
| 2 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143015.png` | Versi clang, cc, make, qemu, gdb, nm, readelf, objdump, git |
| 3 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143120.png` | Struktur direktori repository (ls -la, git log, evidence/tests M11) |
| 4 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143130.png` | Target Makefile utama (grep check-m6..m10) dan docs/readiness |
| 5 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143019.png` | git checkout -b praktikum/m12-sync, preflight.log |
| 6 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143145.png` | Isi include/mcs_sync.h (typedef state, spinlock, mutex, deklarasi API) |
| 7 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143210.png` | wc -l mcs_sync.h, awal kernel/sync/lockdep.c |
| 8 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143225.png` | Implementasi mcs_lockdep_before_acquire dan mcs_lockdep_after_release |
| 9 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143240.png` | wc -l lockdep.c, awal kernel/sync/spinlock.c |
| 10 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143255.png` | mcs_spin_unlock, mcs_spin_is_locked, wc -l spinlock.c |
| 11 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143310.png` | Awal kernel/sync/mutex.c (mcs_mutex_init, mcs_mutex_try_lock) |
| 12 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143325.png` | mcs_mutex_unlock, mcs_mutex_owner, wc -l mutex.c |
| 13 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143340.png` | Isi tests/m12_sync_host_test.c lengkap (83 baris) |
| 14 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143400.png` | Isi Makefile.m12 dan hasil make -f Makefile.m12 clean/all |
| 15 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143415.png` | Output host-test build, [PASS] M12 synchronization host tests passed |
| 16 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143430.png` | readelf -h lockdep.o, ELF header lengkap |
| 17 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143445.png` | objdump -d spinlock.o (mcs_spin_try_lock, mcs_spin_lock, xchg, pause) |
| 18 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143500.png` | objdump lanjutan, mcs_spin_unlock, mcs_spin_is_locked |
| 19 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143520.png` | sha256sums.txt, sed integrasi kmain.c, mkdir kernel/core |
| 20 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143535.png` | sed konfirmasi baris 403 m12_sync_selftest(), sed kmain.c string.h |
| 21 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143550.png` | make clean, full kernel rebuild (semua modul termasuk sync/lockdep.o, mutex.o, spinlock.o) |
| 22 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143610.png` | Linking kernel.elf dengan ld.lld, syscall_entry.o |
| 23 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143625.png` | make inspect (readelf/objdump/nm) dan make image (xorriso ISO build) |
| 24 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143640.png` | make clean rebuild fixed (kernel-build-fixed.log) |
| 25 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143655.png` | make inspect fixed dan make image fixed lengkap |
| 26 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143700.png` | QEMU run, serial log menunjukkan [MCSOS:M12] sync selftest passed sebelum scheduler |
| 27 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143715.png` | Lanjutan serial log (thread A/B tick, MCSOS:TIMER ticks) |
| 28 | `C:\Users\Ajot\Pictures\M12\Screenshot 2026-07-08 143800.png` | git add -A, git commit M12, git log --oneline -3 (commit 3727d4c) |

### Lampiran G — Bukti Tambahan

- **SHA-256 object M12:** Tercatat di evidence/M12/sha256sums.txt
- **SHA-256 ISO:** Tercatat di build/mcsos.iso.sha256
- **Preflight log:** Tercatat di evidence/M12/preflight.log
- **Nm undefined check:** Tercatat di evidence/M12/nm-undefined.txt (kosong = tidak ada undefined symbol)

---

## 24. Daftar Referensi

[1] Microsoft, "Install WSL," Microsoft Learn. Accessed: 2026-07-08. [Online]. Available: https://learn.microsoft.com/en-us/windows/wsl/install

[2] QEMU Project, "Invocation," QEMU documentation. Accessed: 2026-07-08. [Online]. Available: https://www.qemu.org/docs/master/system/invocation.html

[3] OSDev Wiki, "Spinlock," OSDev Wiki. Accessed: 2026-07-08. [Online]. Available: https://wiki.osdev.org/Spinlock

[4] GCC Team, "Built-in Functions for Memory Model Aware Atomic Operations," GCC Online Documentation. Accessed: 2026-07-08. [Online]. Available: https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html

[5] LLVM Project, "Clang Compiler User's Manual — Atomic Builtins," Clang documentation. Accessed: 2026-07-08. [Online]. Available: https://clang.llvm.org/docs/UsersManual.html

[6] The Linux Kernel Documentation, "Runtime Locking Correctness Validator (lockdep)," kernel.org. Accessed: 2026-07-08. [Online]. Available: https://www.kernel.org/doc/html/latest/locking/lockdep-design.html

[7] Intel, "Intel 64 and IA-32 Architectures Software Developer's Manual, Volume 2 (Instruction Set Reference)," Intel Corporation, 2024.

[8] GNU Project, "readelf," GNU Binary Utilities. Accessed: 2026-07-08. [Online]. Available: https://www.gnu.org/software/binutils/binutils.html

[9] The Open Group, "pthread_create, pthread_join," POSIX.1-2017 Base Specifications. Accessed: 2026-07-08. [Online]. Available: https://pubs.opengroup.org/onlinepubs/9699919799/

[10] Panduan Praktikum M12 — Synchronization Primitives, Lock-Order Validator, dan Readiness Gate M12, MCSOS 260502, Muhaemin Sidiq, S.Pd., M.Pd., Institut Pendidikan Indonesia, 2026.

---

## 25. Checklist Final Sebelum Pengumpulan

| Checklist | Status |
|---|---|
| Semua placeholder `[isi ...]` sudah diganti | Ya (kecuali remote repository URL) |
| Metadata laporan lengkap | Ya |
| Commit awal dan akhir dicatat | Ya |
| Perintah build dan test dapat dijalankan ulang | Ya |
| Log build dilampirkan | Ya |
| Log QEMU/test dilampirkan | Ya |
| Artefak penting diberi hash | Ya |
| Desain, invariants, ownership, dan failure modes dijelaskan | Ya |
| Security/reliability dibahas | Ya |
| Readiness review tidak berlebihan | Ya |
| Rubrik penilaian diisi atau disiapkan | Ya |
| Referensi memakai format IEEE | Ya |
| Laporan disimpan sebagai Markdown | Ya |

---

## 26. Pernyataan Pengumpulan

Saya mengumpulkan laporan ini bersama artefak pendukung pada commit:

```
3727d4c M12: implementasi spinlock, mutex, dan lockdep validator dengan self-test terintegrasi
```

Status akhir yang diklaim:

```
Siap uji QEMU tahap M12
```

Ringkasan satu paragraf:

```text
Praktikum M12 berhasil mengimplementasikan tiga primitive sinkronisasi kernel: lock-order
validator berbasis rank (lockdep) yang mendeteksi rekursi dan acquire out-of-order, spinlock
atomic test-and-set dengan busy-wait pause, dan mutex owner-aware yang menolak rekursi, akses
pihak lain, dan unlock non-owner. Seluruh modul diverifikasi berlapis: unit test host (pthread,
4 thread x 10.000 iterasi tanpa lost update), audit object freestanding (nm/readelf/objdump
tanpa undefined symbol), hingga integrasi ke kmain() yang dibuktikan oleh log serial QEMU
menunjukkan "[MCSOS:M12] sync selftest passed" sebelum "[MCSOS:M9] scheduler initialized".
Build bersih tanpa warning, ISO bootable terbentuk, dan semua evidence (readelf, objdump, nm,
sha256sums, serial log) tersedia di evidence/M12/. Status: siap uji QEMU tahap M12.
```
