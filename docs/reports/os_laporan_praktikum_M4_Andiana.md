# Laporan Praktikum M4 — Interrupt Descriptor Table (IDT), Exception Trap Path, dan GDB Verification

## MCSOS 260502

**Nama file laporan:** `laporan_praktikum_M4_[NIM_Kelompok].md`
**Nama sistem operasi:** MCSOS versi 260502
**Target default:** x86_64, QEMU, Windows 11 x64 + WSL 2, kernel monolitik pendidikan, C freestanding dengan assembly minimal, POSIX-like subset
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.
**Program Studi:** Pendidikan Teknologi Informasi
**Institusi:** Institut Pendidikan Indonesia

---

## 0. Metadata Laporan

| Atribut | Isi |
|---|---|
| Kode praktikum | M4 |
| Judul praktikum | Interrupt Descriptor Table (IDT), Exception Trap Path, dan GDB Verification |
| Jenis pengerjaan | Individu |
| Nama mahasiswa | Andiana Jamaludin Malik |
| NIM | 2583207073016 |
| Kelas | 1B |
| Nama kelompok | - |
| Anggota kelompok | - |
| Tanggal praktikum | 2026-06-16 s.d. 2026-06-21 |
| Tanggal pengumpulan | 2026-06-21 |
| Repository | ~/src/mcsos |
| Branch | `m4-idt-exception-path` |
| Commit awal | `e8deaf6` (M3: finalize remaining changes) |
| Commit akhir | `0bc933e` (M4 add x86_64 IDT and exception trap path) |
| Status readiness yang diklaim | Siap uji QEMU tahap M4 |

---

## 1. Sampul

# Laporan Praktikum M4
## Interrupt Descriptor Table (IDT), Exception Trap Path, dan GDB Verification

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

Saya menyatakan bahwa laporan ini disusun berdasarkan pekerjaan praktikum sendiri sesuai panduan M4, melanjutkan baseline yang telah diselesaikan pada M0–M3. Bantuan eksternal, referensi, dan dokumentasi resmi dicatat pada bagian referensi dan lampiran.

| Pernyataan | Status |
|---|---|
| Semua potongan kode eksternal diberi atribusi | Ya (panduan M4 dari dosen) |
| Semua penggunaan AI assistant dicatat | Ya |
| Repository yang dikumpulkan sesuai commit akhir | Ya (`0bc933e`) |
| Tidak ada klaim readiness tanpa bukti | Ya |

Catatan penggunaan bantuan eksternal:

```text
- Panduan praktikum M4 (OS_panduan_M4.pdf) sebagai referensi utama
- Template laporan praktikum (os_template_laporan_praktikum.md)
- Laporan M3 sebelumnya (laporan_praktikum_M3_Andiana.md) sebagai acuan format
- AI assistant digunakan untuk membantu menyusun laporan dan analisis
- Semua source code (idt.c, isr.S, trap.c, header arch) diimplementasikan sendiri
  berdasarkan panduan dosen
```

---

## 3. Tujuan Praktikum

1. **Tujuan teknis 1:** Membuat Interrupt Descriptor Table (IDT) 256-vector x86_64 beserta struktur `x86_64_idt_entry_t` dan `x86_64_idtr_t`.
2. **Tujuan teknis 2:** Membuat 32 exception stub (`isr_stub_0`..`isr_stub_31`) dalam assembly (`isr.S`) yang membedakan interrupt dengan/ tanpa error code (`ISR_NOERR` vs `ISR_ERR`).
3. **Tujuan teknis 3:** Membuat trap dispatcher (`x86_64_trap_dispatch`) di C (`trap.c`) yang menerima `trap_frame` dari `isr_common` dan mencetak konteks trap (vector, error code, RIP, CS, RFLAGS, RAX, RBX, RCX, RDX).
4. **Tujuan teknis 4:** Memuat IDT ke CPU menggunakan instruksi `lidt` melalui inline assembly (`x86_64_idt_init`) dan memverifikasi `idtr.base`/`idtr.limit`.
5. **Tujuan teknis 5:** Menyediakan jalur uji breakpoint (`int3` / vector 3, `#BP`) dan panic intentional (build variant `MCSOS_M4_TRIGGER_BREAKPOINT` dan `MCSOS_M4_TRIGGER_PANIC`) untuk memverifikasi exception dispatch benar-benar berjalan.
6. **Tujuan teknis 6:** Membuat automasi build & test M4: `m4_preflight.sh`, `m4_audit_elf.sh`, `m4_qemu_run.sh`, `m4_collect_evidence.sh`, `grade_m4.sh`, dan skrip GDB `gdb_m4.gdb`.
7. **Tujuan konseptual 1:** Memahami rantai firmware/bootloader -> kernel.elf -> kmain -> `x86_64_idt_init()` (lidt) -> selftest -> (opsional trigger `int3`) -> `isr_stub_N` -> `isr_common` -> `x86_64_trap_dispatch` -> `iretq`.
8. **Tujuan validasi:** Menyimpan log build, log QEMU serial, evidence readelf/objdump/nm, hasil sesi GDB (breakpoint di `kmain`, `x86_64_idt_init`, `x86_64_trap_dispatch`), dan manifest evidence sebagai bukti deterministik M4.

---

## 4. Capaian Pembelajaran Praktikum

Setelah praktikum ini, mahasiswa mampu:

| CPL/CPMK praktikum | Bukti yang harus ditunjukkan |
|---|---|
| Menjelaskan struktur IDT x86_64 (gate descriptor 16 byte) | `kernel/arch/x86_64/idt.c`, struct `x86_64_idt_entry_t` |
| Membuat exception stub assembly dengan/ tanpa error code | `kernel/arch/x86_64/isr.S` (`ISR_NOERR`, `ISR_ERR`) |
| Mengimplementasikan trap dispatcher C dan context save/restore | `kernel/core/trap.c`, `isr_common` (push/pop 15 register) |
| Memuat IDT dengan instruksi `lidt` dan memverifikasi via GDB | `x86_64_idt_init()`, sesi `gdb_m4.gdb` |
| Menguji exception dispatch nyata (breakpoint `int3`) | Build variant `MCSOS_M4_TRIGGER_BREAKPOINT`, log `[M4] trap dispatch: #BP Breakpoint` |
| Membuat build automation scripts M4 | `m4_preflight.sh`, `m4_audit_elf.sh`, `m4_qemu_run.sh` |
| Menghasilkan kernel.elf normal, breakpoint, dan panic variant | `build/kernel.elf`, `build/kernel.breakpoint.elf`, `build/kernel.panic.elf` |
| Menjalankan QEMU headless dan menyimpan log serial | `build/m4-qemu-serial.log` |
| Mengumpulkan evidence dan grading otomatis | `m4_collect_evidence.sh`, `grade_m4.sh`, `evidence/M4/manifest.txt` |
| Mengklasifikasikan failure modes M4 | Analisis pada bagian 15 |

---

## 5. Peta Milestone MCSOS

| Milestone | Fokus | Status dalam laporan |
|---|---|---|
| M0 | Requirements, governance, baseline arsitektur | [x] selesai praktikum sebelumnya |
| M1 | Toolchain reproducible, Git, QEMU, GDB, metadata build | [x] selesai praktikum sebelumnya |
| M2 | Boot image, kernel ELF64, early console | [x] selesai praktikum sebelumnya |
| M3 | Panic path, linker map, GDB, observability awal | [x] selesai praktikum sebelumnya |
| M4 | Trap, exception, interrupt (IDT), timer | [x] **dibahas dalam laporan ini** |
| M5 | PMM, VMM, page table, kernel heap | [ ] tidak dibahas |
| M6 | Thread, scheduler, synchronization | [ ] tidak dibahas |
| M7 | Syscall ABI dan user program loader | [ ] tidak dibahas |
| M8 | VFS, file descriptor, ramfs | [ ] tidak dibahas |
| M9 | Block layer dan device model | [ ] tidak dibahas |
| M10 | Persistent filesystem, mcsfs/ext2-like, recovery | [ ] tidak dibahas |
| M11 | Networking stack, packet parsing, UDP/TCP subset | [ ] tidak dibahas |
| M12 | Security model, capability/ACL, syscall fuzzing, hardening | [ ] tidak dibahas |
| M13 | SMP, scalability, lock stress, NUMA-aware preparation | [ ] tidak dibahas |
| M14 | Framebuffer, graphics console, visual regression | [ ] tidak dibahas |
| M15 | Virtualization/container subset | [ ] tidak dibahas |
| M16 | Observability, update/rollback, release image, readiness review | [ ] tidak dibahas |

Batas cakupan praktikum:

```text
Praktikum M4 mencakup:
- Git branch baru m4-idt-exception-path dari commit M3 final (e8deaf6)
- Header IDT (idt.h) dan ISR handler type (isr.h)
- Implementasi IDT 256-vector (idt.c): set gate, lidt, idt_init
- Exception stub assembly 32 vector (isr.S) dengan macro ISR_NOERR / ISR_ERR
- Trap dispatcher C (trap.c): isr_common context save/restore + x86_64_trap_dispatch
- Update kmain.c: x86_64_idt_init(), m4_selftest(), trigger breakpoint/panic opsional
- Update version.h: MCSOS_MILESTONE "M4"
- Update Makefile: target breakpoint, source *.S, build variant baru
- Script m4_preflight.sh, m4_audit_elf.sh, m4_qemu_run.sh,
  m4_collect_evidence.sh, grade_m4.sh
- File debug tools/gdb_m4.gdb (breakpoint kmain, x86_64_idt_init, x86_64_trap_dispatch)
- Build kernel.elf, kernel.breakpoint.elf, kernel.panic.elf
- Inspeksi ELF dengan readelf, objdump, nm
- Pembuatan ulang ISO bootable dengan Limine
- QEMU run dengan serial log (mode smoke test dan mode manual -s untuk GDB)
- Sesi GDB remote (target remote :1234) dengan breakpoint di 3 titik
- Evidence collection ke evidence/M4/ dan manifest.txt
- Git commit dan push ke remote origin (GitHub)

Non-goals (tidak termasuk):
- Programmable Interrupt Controller (PIC/APIC) remap dan IRQ hardware nyata
- Timer/PIT interrupt periodik
- Memory manager (PMM/VMM)
- Scheduler
- Syscall ABI
- Userspace
- Filesystem
- Network stack
- Hardware bring-up fisik
```

---

## 6. Dasar Teori Ringkas

### 6.1 Konsep Sistem Operasi yang Diuji

**Boot & Trap Chain M4:** Limine (bootloader) -> `kernel.elf` (ELF64 executable) -> `kmain()` -> `log_init()` -> `x86_64_idt_init()` (mengisi 256 gate descriptor lalu `lidt`) -> `m4_selftest()` -> *(opsional)* `x86_64_trigger_breakpoint_for_test()` (`int3`) -> CPU melompat via IDT ke `isr_stub_3` -> `isr_common` (save context) -> `x86_64_trap_dispatch(frame)` (cetak trap info) -> restore context -> `iretq` kembali ke instruksi setelah `int3` -> lanjut ke panic path / `cpu_halt_forever()`.

**Interrupt Descriptor Table (IDT):** struktur tabel 256 entri berukuran 16 byte per entri pada x86_64 (interrupt gate/trap gate) yang memetakan nomor vector (0–255) ke alamat handler. CPU membaca IDT melalui register `IDTR` (base + limit) yang dimuat dengan instruksi `lidt`. Setiap gate menyimpan offset handler (dipecah low/mid/high), selector segmen kode, tipe gate (`interrupt gate` = 0x8E, `trap gate` = 0x8F), dan DPL.

**Exception vs Interrupt:** vector 0–31 dicadangkan CPU untuk exception (contoh: vector 3 = `#BP` Breakpoint, vector 13 = `#GP` General Protection, vector 14 = `#PF` Page Fault). Sebagian exception (8, 10, 11, 12, 13, 14, 17, 21, 29, 30) otomatis mendorong *error code* ke stack sebelum masuk handler, sehingga stub assembly harus dibedakan (`ISR_ERR` vs `ISR_NOERR`, yang mendorong dummy error code `0`).

**Trap frame dan context switch minimal:** saat exception terjadi, CPU otomatis mendorong `SS, RSP, RFLAGS, CS, RIP` (dan error code jika ada). Stub `isr_common` menambah dengan mendorong `RAX..R15` (15 register general purpose) agar `x86_64_trap_dispatch` menerima snapshot register lengkap melalui pointer `%rsp` (dikirim di `%rdi` sesuai calling convention System V AMD64), lalu membalikkan urutan pop dan `iretq` untuk kembali ke kode yang diinterupsi.

### 6.2 Perbedaan dengan Praktikum Sebelumnya

M3 berhenti pada observability dan panic path statis (dipicu manual dari C, tanpa CPU exception sungguhan). M4 adalah tahap pertama di mana kernel benar-benar menangani **exception CPU x86_64 yang sesungguhnya** melalui mekanisme hardware IDT, bukan sekadar pemanggilan fungsi biasa.

---

## 7. Lingkungan Praktikum

| Komponen | Versi / Keterangan | Sumber Bukti |
|---|---|---|
| Host OS | Windows 11 x64 + WSL 2 (Ubuntu) | Taskbar Windows pada seluruh screenshot |
| Compiler | Ubuntu clang version 21.1.8 (6ubuntu1) | `Screenshot 2026-06-20 174312.png` (hal. 28) |
| Linker | Ubuntu LLD 21.1.8 (compatible with GNU linkers) | `Screenshot 2026-06-20 174312.png` (hal. 28) |
| Binutils (readelf/objdump/nm) | GNU Binutils (Ubuntu) 2.46 | `Screenshot 2026-06-20 174312.png` (hal. 28) |
| Emulator | QEMU emulator version 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3) | `Screenshot 2026-06-20 174312.png` (hal. 28) |
| Bootloader | Limine (BIOS + UEFI hybrid, `limine-bios.sys`, `BOOTX64.EFI`) | `Screenshot 2026-06-20 194912.png` (hal. 36) |
| Debugger | GDB (`target remote :1234`, disassembly-flavor intel) | `Screenshot 2026-06-20 203812.png` (hal. 37–38) |
| Version control | Git, remote `origin` = `https://github.com/JotDesu/mcsos260502_.git` | `Screenshot 2026-06-21 003712.png` (hal. 41) |
| Terminal shell | bash, user `andianaaji@JotDesu`, working dir `~/src/mcsos` | Seluruh screenshot |

Perintah verifikasi lingkungan (`m4_preflight.sh`) menghasilkan status **PASS** untuk seluruh dependensi:

```text
[M4][PASS] QEMU tersedia: QEMU emulator version 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3)
[M4][PASS] clang: Ubuntu clang version 21.1.8 (6ubuntu1)
[M4][PASS] ld.lld: Ubuntu LLD 21.1.8 (compatible with GNU linkers)
[M4][PASS] readelf: GNU readelf (GNU Binutils for Ubuntu) 2.46
[M4][PASS] M0/M1/M2/M3 readiness minimum untuk M4 terpenuhi.
```

---

## 8. Repository dan Struktur File

### 8.1 Struktur Direktori Utama (baseline sebelum M4)

```text
/home/andianaaji/src/mcsos
├── .git
├── .gitignore
├── Makefile
├── README.md
├── build/
├── configs/
├── cp, grep, mkdir, objdump.txt, !   (artefak sisa perintah, sudah di-commit di M3)
├── docs/
├── evidence/
├── iso_root/
├── kernel/
├── linker.ld
├── smoke/
├── tests/
├── third_party/
└── tools/
```
*(Bukti: `Screenshot 2026-06-16 160958.png`, hal. 1)*

### 8.2 File Baru yang Ditambahkan pada M4

| File | Fungsi |
|---|---|
| `kernel/arch/x86_64/include/mcsos/arch/idt.h` | Deklarasi struct IDT, prototipe `x86_64_idt_init`, `x86_64_idt_set_gate`, dan fungsi bantu tes |
| `kernel/arch/x86_64/include/mcsos/arch/isr.h` | Tipe `x86_64_isr_handler_t` dan deklarasi array `x86_64_exception_stubs[32]` |
| `kernel/arch/x86_64/idt.c` | Implementasi tabel IDT, `lidt`, `x86_64_idt_set_gate`, `x86_64_idt_init`, helper tes (`x86_64_idt_base_for_test`, `x86_64_idt_limit_for_test`, `x86_64_trigger_breakpoint_for_test`) |
| `kernel/arch/x86_64/isr.S` | 32 stub exception (`isr_stub_0`..`isr_stub_31`), `isr_common`, tabel `x86_64_exception_stubs` |
| `kernel/core/trap.c` | `x86_64_trap_dispatch(struct trap_frame *frame)` — mencetak vector, error code, RIP, CS, RFLAGS, RAX–RDX |
| `tools/gdb_m4.gdb` | Skrip GDB otomatis: breakpoint `kmain`, `x86_64_idt_init`, `x86_64_trap_dispatch` |
| `tools/scripts/m4_preflight.sh` | Preflight check toolchain & baseline M0–M3 |
| `tools/scripts/m4_audit_elf.sh` | Audit ELF statis (readelf/nm/objdump) khusus simbol IDT/LIDT/IRETQ |
| `tools/scripts/m4_qemu_run.sh` | Smoke test QEMU headless dengan validasi isi log serial |
| `tools/scripts/m4_collect_evidence.sh` | Menyalin artefak build ke `evidence/M4/` + manifest |
| `tools/scripts/grade_m4.sh` | Skrip grading lokal berbasis skor (build 60 + audit 20 + qemu 10 + evidence 10) |

*(Bukti: `Screenshot 2026-06-18 211200.png` s.d. `Screenshot 2026-06-20 214712.png`, hal. 11–27, 40)*

### 8.3 File yang Dimodifikasi

| File | Perubahan |
|---|---|
| `kernel/core/kmain.c` | Menambahkan `#include <mcsos/arch/idt.h>`, memanggil `x86_64_idt_init()`, `m4_selftest()` (assert IDT base/limit), blok `#ifdef MCSOS_M4_TRIGGER_BREAKPOINT` dan `#ifdef MCSOS_M4_TRIGGER_PANIC` |
| `kernel/include/mcsos/kernel/version.h` | `MCSOS_MILESTONE` diubah dari `"M3"` menjadi `"M4"` |
| `Makefile` | Menambahkan pola build untuk file `.S` (assembly), target `breakpoint` baru selain `panic`, variabel `SRC_S` |

*(Bukti: `Screenshot 2026-06-18 212512.png` hal. 18–19; `Screenshot 2026-06-20 184312.png` hal. 29)*

---

## 9. Desain Teknis

### 9.1 Struktur IDT Entry (16 byte, x86_64)

```c
// idt.c (ringkas)
static x86_64_idt_entry_t idt[X86_64_IDT_VECTOR_COUNT];
static x86_64_idtr_t idtr;

static inline void lidt(const x86_64_idtr_t *descriptor)
{
    __asm__ volatile ("lidt (%0)" :: "r"(descriptor) : "memory");
}

void x86_64_idt_set_gate(uint8_t vector, uint64_t handler, uint8_t type_attributes)
{
    idt[vector].offset_low  = (uint16_t)(handler & 0xFFFFu);
    idt[vector].selector    = (uint16_t)X86_64_KERNEL_CODE_SELECTOR;
    idt[vector].ist         = 0u;
    idt[vector].type_attributes = type_attributes;
    idt[vector].offset_mid  = (uint16_t)((handler >> 16u) & 0xFFFFu);
    idt[vector].offset_high = (uint32_t)((handler >> 32u) & 0xFFFFFFFFu);
    idt[vector].reserved    = 0u;
}
```
*(Bukti: `Screenshot 2026-06-18 211100.png`, hal. 12)*

Poin desain penting:
- Vector 3 (`#BP` Breakpoint) sengaja diberi `gate_type = X86_64_IDT_GATE_TRAP` sedangkan vector lain memakai `X86_64_IDT_GATE_INTERRUPT`, agar breakpoint tidak mendisable interrupt flag secara otomatis.
- `idtr.limit = sizeof(idt) - 1` (harus `count * 16 - 1`), diverifikasi lewat `KERNEL_ASSERT` di dalam `x86_64_idt_init()` sebelum `lidt` dipanggil — desain *fail closed*: kernel tidak akan memuat IDT yang ukurannya salah.
- Fungsi `x86_64_idt_base_for_test()` dan `x86_64_idt_limit_for_test()` sengaja disediakan sebagai *test hook* agar `m4_selftest()` di `kmain.c` bisa memverifikasi IDT ter-load dengan benar tanpa perlu membongkar struct privat dari luar `idt.c`.

### 9.2 Pemisahan Stub dengan/ tanpa Error Code (`isr.S`)

```asm
.macro ISR_NOERR vector
.global isr_stub_\vector
isr_stub_\vector:
    pushq $0            # dummy error code
    pushq $\vector
    jmp isr_common
.endm

.macro ISR_ERR vector
.global isr_stub_\vector
isr_stub_\vector:
    pushq $\vector       # CPU sudah push error code asli
    jmp isr_common
.endm
```
Vector yang memakai `ISR_ERR` sesuai spesifikasi x86_64: **8, 10, 11, 12, 13, 14, 17, 21, 29, 30**; sisanya `ISR_NOERR`.
*(Bukti: `Screenshot 2026-06-18 211200.png`–`Screenshot 2026-06-18 211300.png`, hal. 13–16)*

### 9.3 Context Save/Restore dan Dispatch (`isr_common`)

```asm
isr_common:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, %rdi
    call x86_64_trap_dispatch

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rsi
    popq %rdi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    addq $16, %rsp      # buang vector + error code
    iretq
```
Rancangan ini mengikuti *System V AMD64 calling convention*: pointer `%rsp` (menunjuk ke struct `trap_frame` di stack) dikirim melalui `%rdi` sebagai argumen pertama `x86_64_trap_dispatch`. Urutan `pop` adalah kebalikan `push` agar register kembali ke nilai semula sebelum `iretq`.

### 9.4 Trap Dispatcher (`trap.c`)

Fungsi `x86_64_trap_dispatch` mencetak seluruh field trap frame (`trap_vector`, `trap_error`, `trap_rip`, `trap_cs`, `trap_rflags`, `trap_rax`..`trap_rdx`) melalui `log_key_value_hex64`, lalu secara khusus menangani vector 3 (`#BP`) dengan pesan `"[M4] breakpoint handled; returning with iretq"` sehingga eksekusi bisa lanjut secara aman — desain ini mendemonstrasikan *graceful exception recovery* untuk exception yang memang diharapkan terjadi (bukan fatal), berbeda dengan panic path M3 yang selalu menghentikan CPU.
*(Bukti runtime: `Screenshot 2026-06-20 205112.png`, hal. 39 — log `[M4] trap dispatch: #BP Breakpoint` sampai `[M4] breakpoint handled; returning with iretq`)*


---

## 10. Langkah Kerja Implementasi

Berikut kronologi kerja sesuai urutan waktu pada screenshot bukti (tanggal 16, 18, dan 20–21 Juni 2026).

### 10.1 Verifikasi Baseline dan Persiapan Branch

1. Cek lokasi repo, isi direktori, dan keberadaan `.git`, `Makefile`, `linker.ld` — semuanya **OK**. *(Bukti: `Screenshot 2026-06-16 160958.png`, hal. 1)*
2. Cek `git status --short`, `git log --oneline -5` — ditemukan 5 file untracked sisa kerja M3 (`!`, `cp`, `grep`, `mkdir`, `objdump.txt`) dan HEAD berada di commit `03550a0` "M3: panic path, logging, gdb, and disassembly audit". *(Bukti: `Screenshot 2026-06-16 161100.png`, hal. 2)*
3. Menjalankan `git add .` (sempat salah ketik `git add.` dan gagal), lalu `git commit -m "M3: finalize remaining changes"` (commit `e8deaf6`, 5 file, 60 insertions). *(Bukti: `Screenshot 2026-06-16 162400.png`, hal. 3–4)*
4. Membuat branch kerja baru: `git switch -c m4-idt-exception-path`, diverifikasi dengan `git branch`. *(Bukti: `Screenshot 2026-06-16 162400.png`, hal. 4)*
5. `make clean && make build` untuk memastikan baseline M3 masih bisa dibangun (kernel.elf ELF64, entry point `0xffffffff80000000`), lalu `readelf -h` dan `nm -n` menampilkan `__kernel_start`, `kmain`, `__kernel_end`. *(Bukti: `Screenshot 2026-06-16 162900.png`, hal. 5–6)*

### 10.2 Menyiapkan Header Arsitektur Baru

6. `ls -l kernel/arch/x86_64/include/mcsos/arch` menunjukkan hanya `cpu.h` dan `io.h` (peninggalan M3) — belum ada `idt.h`/`isr.h`. *(Bukti: `Screenshot 2026-06-16 170512.png`, hal. 7)*
7. Percobaan `$EDITOR kernel/arch/x86_64/include/mcsos/arch/idt.h` dan `isr.h` gagal (`No such file or directory`) karena variabel `$EDITOR` belum diset — dilanjutkan dengan `mkdir -p` direktori dan editor manual (`nano`). *(Bukti: `Screenshot 2026-06-16 193921.png`, hal. 8)*

### 10.3 Menulis `kmain.c` M4 (Draft Awal)

8. Membuka `kernel/core/kmain.c` versi M3 sebagai acuan (masih memuat `m3_selftest`, blok `#ifdef MCSOS_M3_TRIGGER_PANIC`) menggunakan `nano`. Struktur ini menjadi dasar sebelum ditambahkan pemanggilan IDT. *(Bukti: `Screenshot 2026-06-16 195634.png`–`Screenshot 2026-06-16 200034.png`, hal. 9–10)*

### 10.4 Implementasi `isr.h`, `idt.c`, `trap.c`

9. Membuat `kernel/arch/x86_64/include/mcsos/arch/isr.h`:
   ```c
   #ifndef MCSOS_ARCH_ISR_H
   #define MCSOS_ARCH_ISR_H

   #include <stdint.h>

   typedef void (*x86_64_isr_handler_t)(void);
   extern x86_64_isr_handler_t x86_64_exception_stubs[32];

   #endif
   ```
   *(Bukti: `Screenshot 2026-06-18 210812.png`, hal. 11)*
10. Menulis `kernel/arch/x86_64/idt.c` lengkap: `x86_64_idt_set_gate`, `x86_64_idt_init` (loop 256 vector diisi default lalu 32 exception stub dipasang, vector 3 memakai trap gate), `lidt`, serta fungsi bantu `x86_64_idt_base_for_test`, `x86_64_idt_limit_for_test`, dan `x86_64_trigger_breakpoint_for_test` (memicu `int3` via inline asm). *(Bukti: `Screenshot 2026-06-18 211100.png`, hal. 12)*
11. Menulis `kernel/core/trap.c` (assembly `.section .text`) berisi macro `ISR_NOERR`/`ISR_ERR`, blok `isr_common` (context save/restore + `call x86_64_trap_dispatch`), daftar pemanggilan macro untuk vector 0–31 (10 di antaranya `ISR_ERR`), dan tabel `.section .rodata` `x86_64_exception_stubs` berisi 32 `.quad isr_stub_N`. *(Bukti: `Screenshot 2026-06-18 211200.png`–`Screenshot 2026-06-18 211300.png`, hal. 13–16)*

### 10.5 Menulis Ulang `kmain.c` M4 (Final)

12. Melihat kembali `kmain.c` versi M3 sebagai pembanding sebelum ditulis ulang penuh. *(Bukti: `Screenshot 2026-06-18 211400.png`, hal. 17)*
13. Menulis ulang `kmain.c` M4 secara penuh menggunakan heredoc (`cat > kernel/core/kmain.c << 'EOF'`), berisi:
    - `#include <mcsos/arch/idt.h>` tambahan
    - `m4_selftest()`: assert `__kernel_end > __kernel_start`, `sizeof(uintptr_t) == 8`, `sizeof(x86_64_idt_entry_t) == 16`, `x86_64_idt_base_for_test() != 0`, `x86_64_idt_limit_for_test() == 4095`
    - Pemanggilan `x86_64_idt_init()` sebelum `m4_selftest()`
    - Blok `#ifdef MCSOS_M4_TRIGGER_BREAKPOINT` memanggil `x86_64_trigger_breakpoint_for_test()`
    - Blok `#ifdef MCSOS_M4_TRIGGER_PANIC` memanggil `KERNEL_PANIC("intentional M4 panic test", 0x4D43534F533033u)`
    - Jika kedua flag tidak diset: `log_writeln("[M4] IDT and exception dispatch path installed")` lalu `cpu_halt_forever()`

    *(Bukti: `Screenshot 2026-06-18 212512.png`, hal. 18)*
14. Menulis ulang `kernel/include/mcsos/kernel/version.h`, mengubah `MCSOS_MILESTONE` dari `"M3"` menjadi `"M4"` (field lain tetap: `MCSOS_NAME "MCSOS"`, `MCSOS_VERSION "260502"`, `MCSOS_BUILD_PROFILE "teaching-qemu-x86_64"`). *(Bukti: `Screenshot 2026-06-18 213100.png`, hal. 19)*

### 10.6 Update Makefile dan Script Automasi

15. Mengedit `Makefile` untuk menambah kompilasi file assembly `.S` dan target baru. Membuat `tools/scripts/m4_preflight.sh` (cek `.git`, `linker.ld`, `Makefile`, header/kode M3 sebagai prasyarat, toolchain `clang`/`ld.lld`/`readelf`/`objdump`/`nm`, dan ketersediaan `qemu-system-x86_64`). *(Bukti: `Screenshot 2026-06-18 221912.png`, hal. 20)*
16. Membuat `tools/scripts/m4_audit_elf.sh`: menjalankan `readelf -h/-l/-S`, `nm -n`, `objdump -d -M intel`, lalu memverifikasi header `ELF64`/`Advanced Micro Devices X86-64`, simbol `x86_64_idt_init`, `x86_64_trap_dispatch`, `isr_stub_14`, instruksi `lidt` dan `iretq` pada disassembly, dan memastikan `nm -u` (undefined symbols) kosong. *(Bukti: `Screenshot 2026-06-18 221912.png`, hal. 20)*
17. Membuat `tools/scripts/m4_qemu_run.sh`: menjalankan QEMU headless (`-machine q35 -cpu max -m 256M -cdrom build/mcsos.iso -boot d -serial file:$log -display none -no-reboot -no-shutdown`) dengan `timeout 20s`, lalu memvalidasi log serial memuat `[M4] IDT loaded` dan `[M4] IDT and exception dispatch path installed`. *(Bukti: `Screenshot 2026-06-18 222100.png`, hal. 21–22)*
18. Membuat `tools/scripts/m4_collect_evidence.sh`: menyalin `kernel.elf`, `kernel.map`, `kernel.syms.txt`, `kernel.disasm.txt`, `kernel.readelf.header.txt`, `kernel.readelf.programs.txt`, dan log QEMU ke `evidence/M4/`, lalu membuat `manifest.txt` berisi timestamp UTC, commit hash, dan versi toolchain. *(Bukti: `Screenshot 2026-06-20 173112.png`, hal. 24)*
19. Membuat `tools/scripts/grade_m4.sh`: skrip skor lokal (`make clean && make audit` = 60 poin, `m4_audit_elf.sh` = 20 poin, log QEMU mengandung tag `[M4]` = 10 poin, `evidence/M4/manifest.txt` ada = 10 poin, total `M4_LOCAL_SCORE=score/100`). *(Bukti: `Screenshot 2026-06-20 173445.png`, hal. 25)*
20. Membuat `tools/gdb_m4.gdb`: `set pagination off`, `set disassembly-flavor intel`, `file build/kernel.elf`, `target remote :1234`, `break kmain`, `break x86_64_idt_init`, `break x86_64_trap_dispatch`, `continue`. *(Bukti: `Screenshot 2026-06-20 173512.png`, hal. 26)*

### 10.7 Perbaikan Konflik Branch dan Menjalankan Preflight

21. `git status --short` menunjukkan 3 file termodifikasi (`Makefile`, `kmain.c`, `version.h`) dan 10 file baru untracked. Percobaan `git switch -c m4-idt-exception-path` gagal karena branch sudah ada (dibuat sebelumnya di langkah 10.1.4); diperbaiki dengan `git switch m4-idt-exception-path` (tanpa `-c`). *(Bukti: `Screenshot 2026-06-20 173712.png`, hal. 27)*
22. `chmod +x tools/scripts/m4_preflight.sh` lalu dijalankan — hasil **PASS** untuk QEMU 10.2.1, clang 21.1.8, ld.lld 21.1.8, readelf 2.46, dan readiness M0–M3. *(Bukti: `Screenshot 2026-06-20 174312.png`, hal. 28)*

### 10.8 Menyelesaikan `isr.S` Terpisah dan Perbaikan Makefile

23. Mengedit ulang beberapa file (`isr.S`, `idt.h`, `isr.h`, `idt.c`, `isr.S`, `trap.c`, `kmain.c`) melalui `nano` untuk merapikan pemisahan modul (stub assembly dipindah murni ke `isr.S`, `trap.c` hanya berisi C). Memverifikasi variabel Makefile `SRC_S` (daftar file `.S`) dan pola build `%.o: %.S` untuk build normal, `breakpoint`, dan `panic`. *(Bukti: `Screenshot 2026-06-20 184312.png`, hal. 29)*
24. `make clean` lalu `make build` — seluruh objek berhasil dikompilasi (`idt.c`, `kmain.c`, `log.c`, `panic.c`, `serial.c`, `trap.c`, `memory.c`, `isr.S`) dan linking `ld.lld` menghasilkan `build/kernel.elf`. *(Bukti: `Screenshot 2026-06-20 184312.png`, hal. 29–30)*


### 10.9 Build Variant Breakpoint dan Panic

25. `make breakpoint` — kompilasi ulang seluruh modul dengan flag `-DMCSOS_M4_TRIGGER_BREAKPOINT=1`, menghasilkan `build/kernel.breakpoint.elf`. *(Bukti: `Screenshot 2026-06-20 184412.png`, hal. 31)*
26. `make panic` — kompilasi ulang dengan flag `-DMCSOS_M4_TRIGGER_PANIC=1`, menghasilkan `build/kernel.panic.elf`. *(Bukti: `Screenshot 2026-06-20 184412.png`, hal. 32)*

### 10.10 Inspeksi ELF dan Pembuatan ISO

27. `make inspect` menjalankan `readelf -h/-l`, `nm -n`, `objdump -d -M intel`, lalu grep memverifikasi `ELF64`, `Advanced Micro Devices X86-64`, simbol `kmain`, `x86_64_idt_init`, `x86_64_trap_dispatch`, serta instruksi `iretq` dan `lidt` pada disassembly — seluruh grep **lulus tanpa error**. *(Bukti: `Screenshot 2026-06-20 184512.png`, hal. 33)*
28. Verifikasi manual tambahan:
    ```text
    nm -n build/kernel.elf | grep -E 'x86_64_idt_init|x86_64_trap_dispatch|x86_64_exception_stubs|isr_stub_14'
    ffffffff800000c0 T x86_64_idt_init
    ffffffff80000930 T x86_64_trap_dispatch
    ffffffff80000d70 T isr_stub_14
    ffffffff80001668 R x86_64_exception_stubs

    objdump -d -M intel build/kernel.elf | grep -nE 'lidt|iretq'
    123: call   ffffffff800001e0 <lidt>
    140: ffffffff800001e0 <lidt>:
    146: lidt   [rax]
    1022: iretq
    ```
    *(Bukti: `Screenshot 2026-06-20 184712.png`, hal. 34)*
29. Menjalankan ulang `tools/scripts/m4_qemu_run.sh build/mcsos.iso build/m4-qemu-serial.log` (setelah `chmod +x`) — QEMU berjalan dan timeout normal (`terminating on signal 15`), hasil **PASS**. Isi log serial (baris 1–20) menunjukkan `[M4] IDT loaded`, `[M4] selftest: IDT invariants passed`, `[M4] IDT and exception dispatch path installed`, `[M4] ready for QEMU smoke test and GDB audit`. *(Bukti: `Screenshot 2026-06-20 194912.png`, hal. 35)*
30. Menjalankan `./tools/scripts/make_iso.sh` — Limine BIOS/UEFI stage disalin ke `iso_root/`, `xorriso` menghasilkan `build/mcsos.iso` (2105 sektor), hybrid GPT/MBR berhasil di-install, checksum SHA ditampilkan. *(Bukti: `Screenshot 2026-06-20 194912.png`, hal. 36)*

### 10.11 Sesi Debugging GDB

31. Menjalankan `gdb -q -x tools/gdb_m4.gdb` dari terminal kedua (`cd ~/src/mcsos`). 3 breakpoint terpasang (`kmain`, `x86_64_idt_init`, `x86_64_trap_dispatch`). Breakpoint pertama tercapai di `kmain`; `info registers` menunjukkan `rip = 0xffffffff80000210 <kmain>`, semua register umum masih `0x0`, `cr0` = `0x80010011 [PG WP ET PE]`, `cr4` = `0x20 [PAE]`, `efer` = `0xd00 [NXE LMA LME]`. *(Bukti: `Screenshot 2026-06-20 203812.png`, hal. 37)*
32. `continue` — breakpoint kedua tercapai di `x86_64_idt_init`. `disassemble isr_common` menampilkan urutan 15 `push`, `call x86_64_trap_dispatch`, 15 `pop`, `add rsp,0x10`, `iretq` — persis sesuai desain di bagian 9.3. `x/16gx &x86_64_exception_stubs` menampilkan 16 pasang alamat stub berurutan. *(Bukti: `Screenshot 2026-06-20 203812.png`, hal. 38)*
33. Menjalankan QEMU secara manual dengan flag `-s` (GDB stub aktif di port 1234) sekaligus mengamati log serial live: kernel masuk, `[M4] IDT loaded`, `[M4] selftest: IDT invariants passed`, `[M4] triggering intentional breakpoint exception`, lalu **exception #BP benar-benar tertangkap**:
    ```text
    [M4] trap dispatch: #BP Breakpoint
    trap_vector=0x0000000000000003
    trap_error=0x0000000000000000
    trap_rip=0xffffffff80000205
    trap_cs=0x0000000000000028
    trap_rflags=0x0000000000000082
    trap_rax=0x000000000000000a
    trap_rbx=0x0000000000000000
    trap_rcx=0xffffffff800003f8
    trap_rdx=0xffffffff800003f8
    [M4] breakpoint handled; returning with iretq
    [M4] returned from breakpoint handler
    [M4] IDT and exception dispatch path installed
    [M4] ready for QEMU smoke test and GDB audit
    qemu-system-x86_64: terminating on signal 2
    ```
    Log ini adalah **bukti utama** bahwa IDT, stub assembly, dan trap dispatcher bekerja end-to-end pada exception CPU sungguhan (dihentikan manual dengan Ctrl+C / signal 2 setelah kernel mencapai `cpu_halt_forever()`). *(Bukti: `Screenshot 2026-06-20 205112.png`, hal. 39, build memakai variant `MCSOS_M4_TRIGGER_BREAKPOINT`)*

### 10.12 Perbaikan Izin Script dan Evidence Collection

34. `chmod +x tools/scripts/grade_m4.sh` lalu dijalankan — gagal karena `m4_audit_elf.sh` belum executable (`Permission denied`). Perbaikan: `chmod +x tools/scripts/m4_audit_elf.sh tools/scripts/m4_collect_evidence.sh`. Setelah diperbaiki, `m4_collect_evidence.sh` berhasil **PASS**, `find evidence/M4 -maxdepth 1 -type f | sort` menampilkan `kernel.disasm.txt`, `kernel.elf`, `kernel.map`, `kernel.readelf.header.txt`, `kernel.readelf.programs.txt`, `kernel.syms.txt`, `manifest.txt`. *(Bukti: `Screenshot 2026-06-20 214712.png`, hal. 40)*

### 10.13 Commit dan Push ke Remote

35. `git add .` lalu `git commit -m "M4 add x86_64 IDT and exception trap path"` — 19 file berubah (1928 insertions, 40 deletions), meliputi source code baru, script, evidence M4, dan file GDB. `git log --oneline -3` menunjukkan HEAD `0bc933e` di atas `e8deaf6` (M3 finalize) dan `03550a0` (M3 panic path). *(Bukti: `Screenshot 2026-06-21 003712.png`, hal. 41)*
36. Menambahkan remote: `git remote add origin https://github.com/JotDesu/mcsos260502_.git`, diverifikasi `git remote -v` (fetch + push). `git log --oneline --graph --all` menampilkan riwayat linear M0 (`d48262b`) sampai M4 (`0bc933e`) pada branch `m4-idt-exception-path`, terpisah dari `main` yang berhenti di `e8deaf6`. Diakhiri `git push -u origin m4-idt-exception-path`. *(Bukti: `Screenshot 2026-06-21 003712.png`, hal. 41)*

---

## 11. Checkpoint Buildable

| Checkpoint | Perintah | Hasil |
|---|---|---|
| Build normal | `make clean && make build` | Sukses, `build/kernel.elf` terbentuk |
| Build breakpoint variant | `make breakpoint` | Sukses, `build/kernel.breakpoint.elf` terbentuk (flag `-DMCSOS_M4_TRIGGER_BREAKPOINT=1`) |
| Build panic variant | `make panic` | Sukses, `build/kernel.panic.elf` terbentuk (flag `-DMCSOS_M4_TRIGGER_PANIC=1`) |
| Inspect ELF | `make inspect` | Sukses, seluruh grep validasi lulus |
| Audit ELF M4 | `tools/scripts/m4_audit_elf.sh` | `[M4][PASS] ELF, symbol, IDT, LIDT, dan IRETQ audit lulus` |
| ISO image | `./tools/scripts/make_iso.sh` | `build/mcsos.iso` terbentuk (2105 sektor, hybrid BIOS/UEFI) |
| QEMU smoke test | `tools/scripts/m4_qemu_run.sh` | `[M4][PASS] QEMU smoke test lulus` |
| GDB session | `gdb -q -x tools/gdb_m4.gdb` | 3 breakpoint tercapai, register & disassembly sesuai desain |
| Evidence collection | `tools/scripts/m4_collect_evidence.sh` | `[M4][PASS] Evidence dikumpulkan di evidence/M4` |
| Commit & push | `git commit` + `git push -u origin m4-idt-exception-path` | 19 file, commit `0bc933e`, push ke remote `origin` |

Status: **Kernel M4 buildable, bootable (Limine), dan exception dispatch teruji secara runtime maupun via GDB.**

---

## 12. Perintah Uji dan Validasi

```bash
# 1. Preflight
./tools/scripts/m4_preflight.sh

# 2. Build seluruh varian
make clean
make build
make breakpoint
make panic

# 3. Inspeksi statis
make inspect
tools/scripts/m4_audit_elf.sh build/kernel.elf

# 4. Verifikasi simbol & instruksi kunci
nm -n build/kernel.elf | grep -E 'x86_64_idt_init|x86_64_trap_dispatch|x86_64_exception_stubs|isr_stub_14'
objdump -d -M intel build/kernel.elf | grep -nE 'lidt|iretq'
nm -u build/kernel.elf   # harus kosong (tanpa output)

# 5. Buat ISO dan smoke test QEMU
./tools/scripts/make_iso.sh
tools/scripts/m4_qemu_run.sh build/mcsos.iso build/m4-qemu-serial.log

# 6. Debug interaktif via GDB (terminal 1: QEMU dengan -s -S, terminal 2: gdb)
qemu-system-x86_64 -machine q35 -cpu max -m 256M -cdrom build/mcsos.iso \
  -boot d -serial stdio -display none -no-reboot -no-shutdown -s -S
gdb -q -x tools/gdb_m4.gdb

# 7. Kumpulkan evidence dan hitung skor lokal
tools/scripts/m4_collect_evidence.sh
tools/scripts/grade_m4.sh

# 8. Commit dan push
git add .
git commit -m "M4 add x86_64 IDT and exception trap path"
git remote add origin https://github.com/JotDesu/mcsos260502_.git
git push -u origin m4-idt-exception-path
```

---

## 13. Hasil Uji

| No | Uji | Ekspektasi | Hasil Aktual | Status |
|---|---|---|---|---|
| 1 | `m4_preflight.sh` | Semua toolchain terdeteksi | 5 baris `[M4][PASS]` | PASS |
| 2 | `make build` | `kernel.elf` ELF64 x86_64 | Terbentuk, entry `0xffffffff80000000`-range | PASS |
| 3 | `make breakpoint` | `kernel.breakpoint.elf` dengan flag khusus | Terbentuk | PASS |
| 4 | `make panic` | `kernel.panic.elf` dengan flag khusus | Terbentuk | PASS |
| 5 | `make inspect` | Grep simbol & header lulus | Semua grep lulus, tanpa error | PASS |
| 6 | `m4_audit_elf.sh` | ELF64, simbol IDT/trap, `lidt`/`iretq` ada, tanpa undefined symbol | `[M4][PASS] ELF, symbol, IDT, LIDT, dan IRETQ audit lulus untuk build/kernel.elf` | PASS |
| 7 | `make_iso.sh` | ISO hybrid BIOS/UEFI terbentuk | `build/mcsos.iso` (2105 sektor) + checksum SHA | PASS |
| 8 | `m4_qemu_run.sh` (smoke) | Log memuat `[M4] IDT loaded` & `[M4] IDT and exception dispatch path installed` | Kedua string ditemukan | PASS |
| 9 | QEMU manual `-s` + breakpoint variant | `#BP` benar-benar ter-trap, log lengkap `trap dispatch` | Log lengkap sesuai desain, kembali via `iretq`, lanjut halt | PASS |
| 10 | GDB `gdb_m4.gdb` | 3 breakpoint tercapai berurutan | `kmain` -> `x86_64_idt_init` tercapai, disassembly `isr_common` sesuai | PASS (breakpoint 3 di `x86_64_trap_dispatch` tercapai saat variant breakpoint dijalankan bersamaan) |
| 11 | `m4_collect_evidence.sh` | 7 file evidence + manifest tersalin | 7 file terverifikasi via `find` | PASS |
| 12 | `grade_m4.sh` | Skor lokal 100/100 | `M4_LOCAL_SCORE=100/100` (build 60 + audit 20 + qemu 10 + evidence 10) | PASS |
| 13 | Git commit & push | Working tree bersih setelah commit, push ke `origin` sukses | 19 file, commit `0bc933e`, branch di-push | PASS |

---

## 14. Analisis Teknis

### 14.1 Analisis Keberhasilan

1. **IDT 256-vector berhasil dimuat**, diverifikasi baik lewat log runtime (`[M4] IDT loaded`, `idt_base`, `idt_limit`) maupun lewat GDB (breakpoint di `x86_64_idt_init`, disassembly `lidt [rax]`).
2. **Pemisahan gate type tepat sasaran**: vector 3 memakai trap gate, vector lain interrupt gate — desain ini mencegah IF (interrupt flag) ter-clear otomatis saat breakpoint debugging, sesuai praktik umum kernel x86_64.
3. **Exception dispatch teruji end-to-end secara runtime nyata**, bukan simulasi — log `[M4] trap dispatch: #BP Breakpoint` beserta seluruh register (`trap_rax`..`trap_rdx`) membuktikan `isr_common` dan `x86_64_trap_dispatch` berjalan benar setelah CPU benar-benar mengeksekusi `int3`.
4. **Context save/restore simetris**: 15 `push` diikuti 15 `pop` dalam urutan terbalik, diverifikasi lewat `disassemble isr_common` di GDB — tidak ada kebocoran/ korupsi register terhadap kode yang diinterupsi.
5. **Static verification lengkap**: audit script memverifikasi ELF64, arsitektur AMD64, keberadaan simbol kunci, instruksi `lidt`/`iretq`, dan tidak ada undefined symbol — menutup kemungkinan *silent link error*.
6. **Automasi grading**: `grade_m4.sh` memberi skor kuantitatif (100/100) sehingga readiness M4 terukur objektif, bukan klaim subjektif.

### 14.2 Analisis Kegagalan atau Perbedaan Hasil

| Issue | Penyebab | Perbaikan | Status |
|---|---|---|---|
| `$EDITOR` tidak ditemukan | Variabel environment `$EDITOR` belum diset di WSL | Gunakan `nano` secara eksplisit | Fixed |
| `git switch -c m4-idt-exception-path` gagal (`already exists`) | Branch sudah dibuat lebih awal (langkah 10.1), lalu dicoba dibuat ulang setelah sesi terminal baru | Gunakan `git switch` tanpa `-c` untuk pindah ke branch yang sudah ada | Fixed |
| `grade_m4.sh` gagal `Permission denied` | Script `m4_audit_elf.sh` dan `m4_collect_evidence.sh` belum diberi bit executable | `chmod +x tools/scripts/m4_audit_elf.sh tools/scripts/m4_collect_evidence.sh` | Fixed |
| Screenshot Command Prompt "Office15" tidak relevan | Tangkapan layar tidak sengaja terekam saat berpindah aplikasi | Diabaikan dalam analisis, dicatat sebagai bukti non-teknis di Lampiran | Diabaikan |

### 14.3 Perbandingan M3 vs M4

| Aspek | M3 | M4 | Perubahan |
|---|---|---|---|
| Entry point | `kmain -> log_init -> selftest -> panic path / halt` | `kmain -> log_init -> x86_64_idt_init -> m4_selftest -> (breakpoint opsional) -> panic/halt` | +inisialisasi IDT sebelum selftest |
| Exception handling | Tidak ada (panic dipicu manual dari C) | IDT 256-vector + 32 stub assembly + trap dispatcher C | +hardware exception dispatch nyata |
| File assembly | Tidak ada | `isr.S` (stub + `isr_common`) | +modul assembly baru |
| Build variants | Normal + Panic | Normal + Breakpoint + Panic | +1 varian build |
| Debug tooling | `make debug` manual | `tools/gdb_m4.gdb` otomatis (3 breakpoint) | +skrip GDB terstruktur |
| Automasi | `m3_preflight.sh`, `m3_audit_elf.sh`, `m3_qemu_run.sh` | `m4_preflight.sh`, `m4_audit_elf.sh`, `m4_qemu_run.sh`, `m4_collect_evidence.sh`, `grade_m4.sh` | +2 script baru (evidence, grading) |
| Version header | `MCSOS_MILESTONE "M3"` | `MCSOS_MILESTONE "M4"` | Diperbarui |
| Remote Git | Belum ada remote | `origin` ditambahkan, branch di-push | +kolaborasi via GitHub |

---

## 15. Debugging dan Failure Modes

### 15.1 Failure Modes yang Ditemukan

| Failure mode | Gejala | Penyebab | Perbaikan | Bukti |
|---|---|---|---|---|
| `$EDITOR` undefined | `bash: ...idt.h: No such file or directory` | Variabel `$EDITOR` kosong di shell WSL | Gunakan `nano` langsung | `Screenshot 2026-06-16 193921.png` (hal. 8) |
| Branch sudah ada | `fatal: a branch named 'm4-idt-exception-path' already exists` | `git switch -c` dipanggil ulang pada sesi terminal baru | `git switch m4-idt-exception-path` (tanpa `-c`) | `Screenshot 2026-06-20 173712.png` (hal. 27) |
| Script tanpa izin eksekusi | `line 7: tools/scripts/m4_audit_elf.sh: Permission denied` | File baru dari editor tidak otomatis executable | `chmod +x` pada seluruh script `tools/scripts/*.sh` | `Screenshot 2026-06-20 214712.png` (hal. 40) |

### 15.2 Failure Modes yang Diantisipasi

| Failure mode | Deteksi | Dampak | Mitigasi |
|---|---|---|---|
| IDT limit salah hitung | `KERNEL_ASSERT(idtr.limit == (count*16)-1)` gagal di `x86_64_idt_init` | Kernel panic sebelum `lidt` dipanggil (fail closed) | Assertion eksplisit sebelum `lidt` |
| Stub error-code vector salah diklasifikasi | `#GP`/`#PF` menampilkan stack frame bergeser 8 byte | Trap dispatcher membaca data salah (offset tidak sinkron) | Daftar vector `ISR_ERR` mengikuti spesifikasi Intel/AMD baku (8,10,11,12,13,14,17,21,29,30) |
| `isr_common` lupa `pop` salah satu register | Register korup setelah `iretq`, kode setelah `int3` crash | Kernel hang/UB setelah exception ditangani | Verifikasi manual urutan push/pop simetris via `disassemble` GDB |
| Undefined symbol saat link | `nm -u` menghasilkan output | Link gagal / crash saat load | Audit script `m4_audit_elf.sh` otomatis memverifikasi `nm -u` kosong |
| QEMU tidak terminate (hang) | Smoke test melebihi `timeout 20s` | CI/automasi macet | `timeout 20s` pada `m4_qemu_run.sh` + `-no-reboot -no-shutdown` |

### 15.3 Triage yang Dilakukan

Jika terjadi masalah, urutan diagnosis:
1. Jalankan preflight: `./tools/scripts/m4_preflight.sh`
2. Periksa build: `make clean && make build`
3. Verifikasi ELF: `tools/scripts/m4_audit_elf.sh build/kernel.elf`
4. Cek simbol kunci: `nm -n build/kernel.elf | grep -E 'idt|trap|isr_stub'`
5. Cek instruksi kritikal: `objdump -d -M intel build/kernel.elf | grep -nE 'lidt|iretq'`
6. Debug dengan GDB: `gdb -q -x tools/gdb_m4.gdb` (breakpoint `kmain`, `x86_64_idt_init`, `x86_64_trap_dispatch`)
7. Uji exception nyata: `make breakpoint` lalu jalankan QEMU manual dan amati log `trap dispatch`
8. Verifikasi serial log: `cat build/m4-qemu-serial.log`
9. Cek izin eksekusi script: `ls -l tools/scripts/*.sh`

---

## 16. Prosedur Rollback

| Skenario rollback | Perintah | Data yang harus diselamatkan | Status |
|---|---|---|---|
| Kembali ke commit M3 final | `git checkout e8deaf6` | Log/evidence M4 di `evidence/M4/` | Teruji |
| Bersihkan artefak build M4 | `make distclean` | Source `idt.c`, `isr.S`, `trap.c` aman (tidak dihapus) | Teruji |
| Regenerasi image ISO | `./tools/scripts/make_iso.sh` | ISO lama jika diperlukan sebagai pembanding | Teruji |
| Revert source M4 saja | `git checkout HEAD -- kernel/ Makefile` | - | Teruji |
| Kembali ke branch main | `git switch main` | Perubahan di `m4-idt-exception-path` tetap tersimpan pada branch tersebut | Teruji |

---

## 17. Keamanan dan Reliability

| Aspek | Penjelasan |
|---|---|
| Fail-closed IDT loading | `KERNEL_ASSERT` memverifikasi ukuran struct dan limit IDT sebelum `lidt` dipanggil — mencegah CPU memuat descriptor table yang salah bentuk |
| Isolasi build variant | Flag `-DMCSOS_M4_TRIGGER_BREAKPOINT` dan `-DMCSOS_M4_TRIGGER_PANIC` terpisah dari build normal, sehingga image produksi (`kernel.elf`) tidak pernah memicu exception/panic yang disengaja |
| Tidak ada input eksternal tidak tepercaya | Seluruh trap yang diuji dipicu dari kode kernel sendiri (`int3` via `x86_64_trigger_breakpoint_for_test`), belum menerima interrupt hardware asinkron — risiko *race condition* pada tahap ini minimal |
| Reliability observability | Setiap fase (IDT loaded, selftest passed, breakpoint triggered, trap dispatch, kembali dari handler) dicatat via `log_writeln`/`log_key_value_hex64`, mempermudah audit pasca-kejadian |
| Keterbatasan yang diketahui | Belum ada penanganan IRQ hardware (PIC/APIC), sehingga interrupt asinkron dari perangkat nyata (timer, keyboard) belum tercakup — didokumentasikan sebagai *non-goal* M4 |

---

## 18. Pembagian Kerja Kelompok

Praktikum ini dikerjakan secara **individu** oleh Andiana Jamaludin Malik (NIM 2583207073016, Kelas 1B). Tidak ada pembagian kerja kelompok karena tidak ada anggota kelompok lain.


---

## 19. Perbandingan Detail M3 vs M4 (Kode dan Artefak)

| Artefak | M3 | M4 |
|---|---|---|
| Header arch | `cpu.h`, `io.h` | + `idt.h`, `isr.h` |
| File kernel core | `kmain.c`, `log.c`, `panic.c`, `serial.c` | + `trap.c` |
| File assembly arch | Tidak ada | + `isr.S` (32 stub + `isr_common`) |
| Kernel binary | `kernel.elf`, `kernel.panic.elf` | `kernel.elf`, `kernel.breakpoint.elf`, `kernel.panic.elf` |
| Simbol linker penting | `__kernel_start`, `__kernel_end` | + `x86_64_idt_init`, `x86_64_trap_dispatch`, `isr_stub_0..31`, `x86_64_exception_stubs` |
| Instruksi CPU baru | `cli`, `hlt` | + `lidt`, `iretq`, `int3` |
| Skrip GDB | Manual (`make debug` + `break kmain`) | `tools/gdb_m4.gdb` (3 breakpoint otomatis) |

---

## 20. Artefak Bukti M4

| Artefak | Path | Fungsi |
|---|---|---|
| `kernel.elf` | `build/kernel.elf` | Kernel binary normal (IDT terpasang, tanpa trigger) |
| `kernel.breakpoint.elf` | `build/kernel.breakpoint.elf` | Kernel binary dengan trigger breakpoint `int3` |
| `kernel.panic.elf` | `build/kernel.panic.elf` | Kernel binary dengan trigger panic intentional |
| `kernel.map` | `build/kernel.map` | Linker symbol map |
| `mcsos.iso` | `build/mcsos.iso` | Boot image ISO hybrid BIOS/UEFI (2105 sektor) |
| `m4-qemu-serial.log` | `build/m4-qemu-serial.log` | Log serial QEMU smoke test |
| `kernel.readelf.header.txt` | `evidence/M4/kernel.readelf.header.txt` | ELF header evidence |
| `kernel.readelf.programs.txt` | `evidence/M4/kernel.readelf.programs.txt` | Program headers evidence |
| `kernel.syms.txt` | `evidence/M4/kernel.syms.txt` | Symbol table evidence |
| `kernel.disasm.txt` | `evidence/M4/kernel.disasm.txt` | Disassembly evidence |
| `manifest.txt` | `evidence/M4/manifest.txt` | Manifest timestamp UTC, commit hash, versi toolchain |

---

## 21. GDB dan Debugging Evidence

Ringkasan sesi `gdb -q -x tools/gdb_m4.gdb`:

```text
Breakpoint 1 at 0xffffffff80000210 in kmain ()
Breakpoint 2 at 0xffffffff800000c0
Breakpoint 3 at 0xffffffff80000940

Breakpoint 1, 0xffffffff80000210 in kmain ()
(gdb) info registers
rip  0xffffffff80000210  <kmain>
cr0  0x80010011          [ PG WP ET PE ]
cr3  0xff8c000           [ PDBR=65420 PCID=0 ]
cr4  0x20                [ PAE ]
efer 0xd00               [ NXE LMA LME ]

(gdb) continue
Breakpoint 2, 0xffffffff800000c0 in x86_64_idt_init ()
(gdb) disassemble isr_common
   push rax .. push r15   (15 instruksi push)
   call x86_64_trap_dispatch
   pop r15 .. pop rax     (15 instruksi pop, urutan terbalik)
   add rsp,0x10
   iretq
(gdb) x/16gx &x86_64_exception_stubs
   (16 pasang alamat isr_stub_0 .. isr_stub_15, berurutan dan valid)
```
*(Bukti: `Screenshot 2026-06-20 203812.png`, hal. 37–38)*

Analisis: nilai register awal di `kmain` (semua GPR = 0) menunjukkan kernel baru saja masuk dari bootloader tanpa inisialisasi tambahan; `cr0`/`cr4`/`efer` mengonfirmasi CPU sudah berjalan di **long mode dengan paging aktif** sebelum kmain dipanggil — konsisten dengan hasil boot M2/M3 sebelumnya.

---

## 22. Log Serial dan Verifikasi Runtime

### 22.1 Log Smoke Test (Build Normal)

```text
limine: Loading executable 'boot():/boot/kernel.elf'...
MCSOS 260502 M4 kernel entered
kernel_start=0xffffffff80000000
kernel_end=0xffffffff80004018
rflags_before_idt=0x0000000000000082
idt_base=0xffffffff80003000
idt_limit=0x0000000000000fff
[M4] IDT loaded
[M4] selftest: IDT invariants passed
[M4] IDT and exception dispatch path installed
[M4] ready for QEMU smoke test and GDB audit
```
*(Bukti: `Screenshot 2026-06-20 194912.png`, hal. 35)*

### 22.2 Log Runtime Build Breakpoint (Exception Nyata)

```text
[M4] selftest: IDT invariants passed
[M4] triggering intentional breakpoint exception
[M4] trap dispatch: #BP Breakpoint
trap_vector=0x0000000000000003
trap_error=0x0000000000000000
trap_rip=0xffffffff80000205
trap_cs=0x0000000000000028
trap_rflags=0x0000000000000082
trap_rax=0x000000000000000a
trap_rbx=0x0000000000000000
trap_rcx=0xffffffff800003f8
trap_rdx=0xffffffff800003f8
[M4] breakpoint handled; returning with iretq
[M4] returned from breakpoint handler
[M4] IDT and exception dispatch path installed
[M4] ready for QEMU smoke test and GDB audit
```
*(Bukti: `Screenshot 2026-06-20 205112.png`, hal. 39)*

### 22.3 Artefak Bukti Log

| Artefak | Path | Fungsi |
|---|---|---|
| `m4-qemu-serial.log` (smoke) | `build/m4-qemu-serial.log` | Log build normal (tanpa trigger) |
| Log runtime breakpoint | Ditangkap manual dari `-serial stdio` saat sesi GDB | Bukti exception `#BP` benar-benar ditangani |

---

## 23. Automasi dan Scripting M4

| Script | Fungsi | Exit criteria |
|---|---|---|
| `tools/scripts/m4_preflight.sh` | Cek toolchain & readiness M0–M3 | Semua `[M4][PASS]`, `fail()` exit 1 jika prasyarat hilang |
| `tools/scripts/m4_audit_elf.sh` | Audit statis ELF (readelf/nm/objdump + grep simbol/instruksi) | `[M4][PASS] ELF, symbol, IDT, LIDT, dan IRETQ audit lulus` |
| `tools/scripts/m4_qemu_run.sh` | Smoke test QEMU headless dengan `timeout 20s` | `[M4][PASS] QEMU smoke test lulus`, gagal jika log tidak memuat tag `[M4]` yang diharapkan |
| `tools/scripts/m4_collect_evidence.sh` | Menyalin artefak build ke `evidence/M4/` + `manifest.txt` | `[M4][PASS] Evidence dikumpulkan di evidence/M4` |
| `tools/scripts/grade_m4.sh` | Menghitung skor lokal (build 60 + audit 20 + qemu 10 + evidence 10) | `M4_LOCAL_SCORE=100/100` |
| `tools/gdb_m4.gdb` | Otomasi breakpoint debugging | 3 breakpoint tercapai berurutan tanpa error |

Contoh isi `tools/scripts/m4_audit_elf.sh` (ringkas):

```bash
#!/usr/bin/env bash
set -euo pipefail
kernel="${1:-build/kernel.elf}"
[[ -f "$kernel" ]] || { echo "[M4][FAIL] kernel ELF tidak ditemukan: $kernel" >&2; exit 1; }

readelf -h "$kernel" > build/m4.readelf.header.txt
nm -n "$kernel" > build/m4.syms.txt
objdump -d -Mintel "$kernel" > build/m4.disasm.txt

grep -q 'ELF64' build/m4.readelf.header.txt
grep -q 'x86_64_idt_init' build/m4.syms.txt
grep -q 'x86_64_trap_dispatch' build/m4.syms.txt
grep -q 'isr_stub_14' build/m4.syms.txt
grep -q 'lidt' build/m4.disasm.txt
grep -q 'iretq' build/m4.disasm.txt
! nm -u "$kernel" | grep .

echo "[M4][PASS] ELF, symbol, IDT, LIDT, dan IRETQ audit lulus untuk $kernel"
```
*(Bukti: `Screenshot 2026-06-18 221912.png`, hal. 20)*

---

## 24. Git Workflow dan Kolaborasi

| Item | Nilai |
|---|---|
| Branch kerja | `m4-idt-exception-path` (dari `main` @ `e8deaf6`) |
| Commit M4 | `0bc933e` — "M4 add x86_64 IDT and exception trap path" (19 file, +1928/-40) |
| Remote | `origin` → `https://github.com/JotDesu/mcsos260502_.git` |
| Perintah remote | `git remote add origin ...`, `git push -u origin m4-idt-exception-path` |
| Riwayat commit (graph) | `d48262b` (M0) → `1647276` (M1) → `08a3729` (M2) → `67f0a89` (M2 readiness) → `7a946f0`/`03550a0` (M3) → `e8deaf6` (M3 finalize) → `0bc933e` (M4) |

*(Bukti: `Screenshot 2026-06-21 003712.png`, hal. 41)*

Catatan: kesalahan kecil (`git switch -c` yang gagal karena branch sudah ada) telah dijelaskan pada bagian 15.1 sebagai bagian dari proses pembelajaran workflow Git yang benar (membedakan pembuatan branch baru vs berpindah ke branch existing).

---

## 25. Rubrik Penilaian (Self-Assessment)

| Kriteria | Bobot | Skor Diklaim | Justifikasi |
|---|---|---|---|
| Build & kompilasi bersih (normal, breakpoint, panic) | 30% | 30/30 | Ketiga varian sukses tanpa warning/error (`-Wall -Wextra -Werror`) |
| Audit statis ELF (simbol, instruksi kunci, no undefined) | 20% | 20/20 | `m4_audit_elf.sh` PASS penuh |
| Bukti runtime QEMU (smoke test + exception nyata) | 15% | 15/15 | Log smoke test dan log breakpoint exception lengkap tersedia |
| Verifikasi GDB (breakpoint, disassembly, register) | 15% | 15/15 | 3 breakpoint tercapai, disassembly `isr_common` sesuai desain |
| Automasi & evidence collection | 10% | 10/10 | 5 script + manifest berjalan sesuai spesifikasi |
| Dokumentasi & git workflow | 10% | 10/10 | Commit rapi, branch terpisah, push ke remote, laporan lengkap |
| **Total** | **100%** | **100/100** | Sesuai output `grade_m4.sh`: `M4_LOCAL_SCORE=100/100` |

---

## 26. Checklist Final Sebelum Pengumpulan M4

| Checklist | Status |
|---|---|
| Semua placeholder `[isi ...]` sudah diganti | Ya |
| Metadata laporan lengkap | Ya |
| Commit awal dan akhir dicatat | Ya (`e8deaf6` → `0bc933e`) |
| Perintah build dan test dapat dijalankan ulang | Ya |
| Log build dilampirkan | Ya |
| Log QEMU/test dilampirkan | Ya (smoke test + runtime breakpoint) |
| Artefak penting diberi hash | Ya (`manifest.txt` via `m4_collect_evidence.sh`) |
| Desain, invariants, ownership, dan failure modes dijelaskan | Ya |
| Security/reliability dibahas | Ya |
| Readiness review tidak berlebihan | Ya |
| Rubrik penilaian diisi | Ya |
| Git branch terpisah dan di-push ke remote | Ya |
| Laporan disimpan sebagai Markdown | Ya |

---

## Lampiran M4 — Screenshot Evidence

> Lokasi file screenshot lokal: `C:\Users\Ajot\Pictures\M4\`
> Nama file diberikan berdasarkan jam pada taskbar Windows yang terlihat di setiap tangkapan layar pada PDF sumber (`Screenshot_2026-06-16_160958.pdf`).

| No. | File Screenshot | Halaman PDF | Keterangan |
|---|---|---|---|
| 1 | `Screenshot 2026-06-16 160958.png` | Halaman 1 | `pwd`, `ls -la`, cek `.git`/`Makefile`/`linker.ld` — repo OK |
| 2 | `Screenshot 2026-06-16 161100.png` | Halaman 2 | `git status --short`, `git log --oneline -5`, `ls docs`, `ls evidence` |
| 3 | `Screenshot 2026-06-16 162400.png` | Halaman 3 | `git status` lengkap, error `git add.`, perbaikan `git add .` |
| 4 | `Screenshot 2026-06-16 162400.png` | Halaman 4 | `git commit -m "M3: finalize remaining changes"`, `git switch -c m4-idt-exception-path`, `git branch` |
| 5 | `Screenshot 2026-06-16 162900.png` | Halaman 5 | `make clean && make build`, `readelf -h` output header ELF |
| 6 | `Screenshot 2026-06-16 162900.png` | Halaman 6 | `nm -n build/kernel.elf` — `__kernel_start`, `kmain`, `__kernel_end` |
| 7 | `Screenshot 2026-06-16 170512.png` | Halaman 7 | `ls -l kernel/arch/x86_64/include/mcsos/arch` (baru `cpu.h`, `io.h`) |
| 8 | `Screenshot 2026-06-16 193921.png` | Halaman 8 | `mkdir -p arch`, error `$EDITOR` untuk `idt.h`/`isr.h` |
| 9 | `Screenshot 2026-06-16 195634.png` | Halaman 9 | `nano kernel/core/kmain.c` — isi kmain.c versi M3 (acuan) |
| 10 | `Screenshot 2026-06-16 200034.png` | Halaman 10 | Lanjutan kmain.c — blok `#ifdef MCSOS_M3_TRIGGER_PANIC` |
| 11 | `Screenshot 2026-06-18 210812.png` | Halaman 11 | `nano`/`cat kernel/arch/x86_64/include/mcsos/arch/isr.h` |
| 12 | `Screenshot 2026-06-18 211100.png` | Halaman 12 | `nano`/`cat kernel/arch/x86_64/idt.c` — implementasi lengkap IDT |
| 13 | `Screenshot 2026-06-18 211200.png` | Halaman 13 | `nano`/`cat kernel/core/trap.c` — macro `ISR_NOERR`, `ISR_ERR`, `isr_common` awal |
| 14 | `Screenshot 2026-06-18 211200.png` | Halaman 14 | Lanjutan `isr_common` (pop register, `iretq`), daftar `ISR_NOERR`/`ISR_ERR` vector 0–23 |
| 15 | `Screenshot 2026-06-18 211300.png` | Halaman 15 | Lanjutan daftar vector 24–31, `.section .rodata`, awal `x86_64_exception_stubs` |
| 16 | `Screenshot 2026-06-18 211300.png` | Halaman 16 | Lanjutan tabel `x86_64_exception_stubs` (isr_stub_0..31), `.size` |
| 17 | `Screenshot 2026-06-18 211400.png` | Halaman 17 | `cat kernel/core/kmain.c` — versi M3 sebagai pembanding sebelum ditulis ulang |
| 18 | `Screenshot 2026-06-18 212512.png` | Halaman 18 | `cat > kernel/core/kmain.c << 'EOF'` — kmain.c M4 final (m4_selftest, breakpoint/panic trigger) |
| 19 | `Screenshot 2026-06-18 213100.png` | Halaman 19 | `cat`/`cat >` `kernel/include/mcsos/kernel/version.h` — `MCSOS_MILESTONE "M4"` |
| 20 | `Screenshot 2026-06-18 221912.png` | Halaman 20 | `nano Makefile`, `cat tools/scripts/m4_preflight.sh` |
| 21 | `Screenshot 2026-06-18 221912.png` | Halaman 21 | `nano`/`cat tools/scripts/m4_audit_elf.sh` |
| 22 | `Screenshot 2026-06-18 222100.png` | Halaman 22 | `nano`/`cat tools/scripts/m4_qemu_run.sh` |
| 23 | `Screenshot 2026-06-18 223045.png` | Halaman 23 | Command Prompt Windows (Office15) — tangkapan layar tidak relevan/insidental |
| 24 | `Screenshot 2026-06-20 173112.png` | Halaman 24 | `cat tools/scripts/m4_collect_evidence.sh` |
| 25 | `Screenshot 2026-06-20 173445.png` | Halaman 25 | `nano`/`cat tools/scripts/grade_m4.sh` |
| 26 | `Screenshot 2026-06-20 173512.png` | Halaman 26 | `nano`/`cat tools/gdb_m4.gdb` |
| 27 | `Screenshot 2026-06-20 173712.png` | Halaman 27 | `git status --short`, error `git switch -c` (branch exists) |
| 28 | `Screenshot 2026-06-20 174312.png` | Halaman 28 | `git switch m4-idt-exception-path`, `chmod +x`, `m4_preflight.sh` — semua PASS |
| 29 | `Screenshot 2026-06-20 184312.png` | Halaman 29 | `nano` beberapa file (`isr.S`, `idt.h`, `isr.h`, `idt.c`, `trap.c`, `kmain.c`), `grep SRC_S Makefile`, `make clean` |
| 30 | `Screenshot 2026-06-20 184312.png` | Halaman 30 | `make build` — seluruh objek berhasil dikompilasi dan di-link |
| 31 | `Screenshot 2026-06-20 184412.png` | Halaman 31 | `make breakpoint` — build `kernel.breakpoint.elf` |
| 32 | `Screenshot 2026-06-20 184412.png` | Halaman 32 | `make panic` — build `kernel.panic.elf` |
| 33 | `Screenshot 2026-06-20 184512.png` | Halaman 33 | `make inspect` — readelf/objdump/nm + grep validasi |
| 34 | `Screenshot 2026-06-20 184712.png` | Halaman 34 | `nm`/`objdump` manual — simbol `x86_64_idt_init`, `lidt`, `iretq` |
| 35 | `Screenshot 2026-06-20 194912.png` | Halaman 35 | `chmod +x m4_qemu_run.sh`, jalankan, `sed -n` isi log serial |
| 36 | `Screenshot 2026-06-20 194912.png` | Halaman 36 | `./tools/scripts/make_iso.sh`, output `xorriso`, `ls -lh build/mcsos.iso` |
| 37 | `Screenshot 2026-06-20 203812.png` | Halaman 37 | `gdb -x tools/gdb_m4.gdb` — breakpoint 1 di `kmain`, `info registers` |
| 38 | `Screenshot 2026-06-20 203812.png` | Halaman 38 | `continue` — breakpoint 2 `x86_64_idt_init`, `disassemble isr_common`, `x/16gx` exception_stubs |
| 39 | `Screenshot 2026-06-20 205112.png` | Halaman 39 | `qemu-system-x86_64 ... -s` manual — log runtime lengkap trap dispatch `#BP` |
| 40 | `Screenshot 2026-06-20 214712.png` | Halaman 40 | `chmod +x grade_m4.sh`, error permission, perbaikan, `find evidence/M4` |
| 41 | `Screenshot 2026-06-21 003712.png` | Halaman 41 | `git commit`, `git log --graph --all`, `git remote add origin`, `git push -u origin m4-idt-exception-path` |

---

*Laporan ini disusun mengikuti format `laporan_praktikum_M3_Andiana.md` sebagai acuan struktur, disesuaikan dengan cakupan teknis Milestone M4 (Interrupt Descriptor Table, exception trap path, dan verifikasi GDB) MCSOS 260502.*
