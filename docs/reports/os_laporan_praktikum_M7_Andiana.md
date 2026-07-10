# Laporan Praktikum M7 — Virtual Memory Manager (VMM), Page Table Walker, dan Page Fault Handling

## MCSOS 260502

**Nama file laporan:** `laporan_praktikum_M7_[NIM_Kelompok].md`
**Nama sistem operasi:** MCSOS versi 260502
**Target default:** x86_64, QEMU, Windows 11 x64 + WSL 2, kernel monolitik pendidikan, C freestanding dengan assembly minimal, POSIX-like subset
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.
**Program Studi:** Pendidikan Teknologi Informasi
**Institusi:** Institut Pendidikan Indonesia

---

## 0. Metadata Laporan

| Atribut | Isi |
|---|---|
| Kode praktikum | M7 |
| Judul praktikum | Virtual Memory Manager (VMM), Page Table Walker, dan Page Fault Handling |
| Jenis pengerjaan | Individu |
| Nama mahasiswa | Andiana Jamaludin Malik |
| NIM | 2583207073016 |
| Kelas | 1B |
| Nama kelompok | - |
| Anggota kelompok | - |
| Tanggal praktikum | 2026-07-03 |
| Tanggal pengumpulan | 2026-07-03 |
| Repository | ~/src/mcsos |
| Branch kerja | m7-vmm-wip |
| Commit evidence page fault | `3cf1196` (m7: add page-fault path evidence (demo fault enabled via -DMCSOS_M7_DEMO_PAGEFAULT)) |
| Status readiness yang diklaim | Siap uji QEMU tahap M7 (dengan catatan pada bagian 19) |

---

## 1. Sampul

# Laporan Praktikum M7
## Virtual Memory Manager (VMM), Page Table Walker, dan Page Fault Handling

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

Saya menyatakan bahwa laporan ini disusun berdasarkan pekerjaan praktikum sendiri sesuai panduan M7. Bantuan eksternal, referensi, dan dokumentasi resmi dicatat pada bagian referensi dan lampiran.

| Pernyataan | Status |
|---|---|
| Semua potongan kode eksternal diberi atribusi | Ya (panduan M7 dari dosen) |
| Semua penggunaan AI assistant dicatat | Ya |
| Repository yang dikumpulkan sesuai commit akhir | Ya |
| Tidak ada klaim readiness tanpa bukti | Ya (lihat catatan bagian 19 mengenai milestone tag) |

Catatan penggunaan bantuan eksternal:

```text
- Panduan praktikum M7 (OS_panduan_M7.pdf) sebagai referensi utama
- Template laporan praktikum (os_template_laporan_praktikum.md)
- AI assistant digunakan untuk membantu menyusun laporan dan analisis dari bukti terminal/log
- Seluruh source code (vmm.h, vmm.c, test_vmm_host.c, idt.c, kmain.c) diimplementasikan
  berdasarkan panduan dosen dan diverifikasi lewat build/QEMU/GDB milik sendiri
```

---

## 3. Tujuan Praktikum

1. **Tujuan teknis 1:** Mengimplementasikan Virtual Memory Manager (VMM) x86_64 4-level page table (PML4 → PDPT → PD → PT) yang mampu melakukan `map`, `query`, dan `unmap` halaman 4 KiB.
2. **Tujuan teknis 2:** Membuat unit test host-mode (`test_vmm_host.c`) yang memverifikasi table walker tanpa perlu boot QEMU, serta static check biner (`nm -u`, `objdump` untuk memastikan instruksi `invlpg` dan akses `%cr3` benar-benar ter-emit).
3. **Tujuan teknis 3:** Mengintegrasikan VMM ke `kmain()` melalui `kernel_vmm_init()` dan memverifikasi demo map/query/unmap satu halaman di atas QEMU sungguhan.
4. **Tujuan teknis 4:** Mengimplementasikan penanganan `#PF` (Page Fault, vector 14) pada `idt.c`: membaca `CR2`, mendekode `error_code` (bit P, W/R, U/S, RSVD, I/D), dan memicu panic terkendali (bukan hang atau triple fault) sebagai bukti failure path aman.
5. **Tujuan konseptual:** Memahami hubungan `CR3` (root page table) → page table walker 4 level → `PTE` (Page Table Entry) flags → TLB (`invlpg`) → `CR2` (faulting address) → error code page fault sesuai arsitektur x86_64.
6. **Tujuan validasi:** Menyimpan log build, log QEMU (normal dan demo page-fault), evidence `readelf`/`objdump`/`nm`, log GDB, serta commit Git sebagai bukti deterministik tahap M7.

---

## 4. Capaian Pembelajaran Praktikum

Setelah praktikum ini, mahasiswa mampu:

| CPL/CPMK praktikum | Bukti yang harus ditunjukkan |
|---|---|
| Menjelaskan mekanisme page table 4 level x86_64 (PML4/PDPT/PD/PT) dan peran CR3 | `kernel/core/vmm.c`, log `root_paddr` |
| Mengimplementasikan table walker generik dengan callback alloc/free/phys_to_virt | `struct vmm_space`, `get_or_alloc_next_table()` |
| Membuat API `vmm_map_page`, `vmm_query_page`, `vmm_unmap_page` dengan kontrak error yang jelas | `kernel/include/mcsos/kernel/vmm.h` |
| Menulis unit test host-mode tanpa hardware nyata | `kernel/tests/test_vmm_host.c`, output `M7 VMM host tests PASS` |
| Memverifikasi hasil kompilasi lewat static check (undefined symbol, disassembly) | `build/vmm.undefined.txt`, `build/vmm.objdump.txt` |
| Menangani `#PF` dan mendekode error code sesuai Intel SDM | `kernel/arch/x86_64/src/idt.c` (`page_fault_dump`) |
| Membuktikan failure path aman lewat page fault terkendali (bukan hang) | `evidence/M7/m7_pagefault_serial.log`, panic banner |
| Menggunakan GDB untuk memeriksa state CPU saat breakpoint | Sesi GDB (`info rip`, `info rsp`) |
| Mengelola pekerjaan pada branch terpisah dan mendorong evidence ke remote | `git commit`/`git push origin m7-vmm-wip` |

---

## 5. Peta Milestone MCSOS

| Milestone | Fokus | Status dalam laporan |
|---|---|---|
| M0 | Requirements, governance, baseline arsitektur | [x] selesai (praktikum sebelumnya) |
| M1 | Toolchain reproducible, Git, QEMU, GDB, metadata build | [x] selesai (praktikum sebelumnya) |
| M2 | Boot image, kernel ELF64, early console | [x] selesai (praktikum sebelumnya) |
| M3 | Panic path, linker map, GDB, observability awal | [x] selesai (dipakai ulang: `KERNEL_PANIC`) |
| M4 | Trap, exception, interrupt, timer | [x] selesai (dipakai ulang: IDT, PIC, PIT) |
| M5 | Interrupt-driven timer, observability lanjutan | [x] selesai (dipakai ulang: `[MCSOS:TIMER]` log) |
| M6 | Physical Memory Manager (PMM) | [x] selesai (dipakai ulang: `pmm_alloc_frame`/`pmm_free_frame`) |
| **M7** | **Virtual Memory Manager (VMM), page table, page fault** | **[x] dibahas penuh pada laporan ini** |
| M8 | Kernel heap, allocator dinamis | [ ] tidak dibahas |
| M9 | Thread, scheduler, synchronization | [ ] tidak dibahas |
| M10 | Syscall ABI dan user program loader | [ ] tidak dibahas |
| M11+ | VFS, block layer, filesystem, networking, security, SMP, dst. | [ ] tidak dibahas |

Batas cakupan praktikum:

```text
Praktikum M7 mencakup:
- Header kontrak VMM (kernel/include/mcsos/kernel/vmm.h)
- Implementasi table walker 4 level (kernel/core/vmm.c)
- Fungsi vmm_space_init, vmm_map_page, vmm_query_page, vmm_unmap_page
- Fungsi arsitektur: vmm_invalidate_page (invlpg), vmm_read_cr3/vmm_write_cr3, vmm_read_cr2
- Unit test host-mode (kernel/tests/test_vmm_host.c) dengan simulasi frame fisik
- Target build/test check-m7 pada Makefile (static check: nm -u, objdump invlpg/cr3)
- Integrasi ke kmain() lewat kernel_vmm_init() dan demo map/query/unmap satu halaman
- Penanganan #PF (page_fault_dump) pada idt.c: CR2, error_code, dekode bit P/W-R/U-S/RSVD/I-D
- Uji failure injection: page fault terkendali via flag build -DMCSOS_M7_DEMO_PAGEFAULT
- Debugging GDB dan evidence Git pada branch m7-vmm-wip

Non-goals (tidak termasuk):
- CR3 belum di-switch ke root page table VMM (kernel masih berjalan di atas identity map
  bootloader; "Tugas wajib berhenti di sini" sebelum mapping kernel/stack/IDT/GDT/MMIO lengkap)
- Kernel heap/dynamic allocator (M8)
- Demand paging, copy-on-write, swap
- Multi-level TLB shootdown untuk SMP
- User-mode page table terpisah dan syscall ABI
- Higher-half HHDM asli dari Limine (masih pakai identity map sementara)
```

---

## 6. Dasar Teori Ringkas

### 6.1 Konsep Sistem Operasi yang Diuji

**Virtual Memory Manager (VMM):** Lapisan yang menerjemahkan alamat virtual (`vaddr`) menjadi alamat fisik (`paddr`) lewat struktur page table bertingkat, serta mengatur hak akses (writable, user/supervisor, no-execute) per halaman 4 KiB.

**4-Level Paging x86_64:** `CR3` menunjuk ke **PML4**, yang tiap entri-nya menunjuk ke **PDPT**, yang menunjuk ke **PD**, yang menunjuk ke **PT**, yang entrinya (**PTE**) berisi alamat frame fisik 4 KiB beserta flag kontrol.

**Page Fault (#PF, vector 14):** Exception CPU yang dipicu saat MMU gagal menerjemahkan alamat virtual. CPU mengisi register `CR2` dengan alamat virtual yang gagal, dan mendorong `error_code` ke stack trap frame berisi bit-bit klasifikasi.

**TLB dan `invlpg`:** Translation Lookaside Buffer menyimpan cache hasil terjemahan; instruksi `invlpg` dipakai untuk membatalkan entri cache satu halaman setelah `unmap`/`remap`.

### 6.2 Konsep Arsitektur x86_64 yang Relevan

| Konsep | Relevansi pada praktikum | Bukti/verifikasi |
|---|---|---|
| PML4/PDPT/PD/PT | Struktur 4 level yang di-walk oleh `table_from_phys` + `idx_pml4/pdpt/pd/pt` | `kernel/core/vmm.c` |
| Page Table Entry (PTE) flags | `PRESENT`, `WRITABLE`, `USER`, `WRITE_THROUGH`, `CACHE_DISABLE`, `ACCESSED`, `DIRTY`, `HUGE`, `GLOBAL`, `NO_EXECUTE` (bit 63) | `kernel/include/mcsos/kernel/vmm.h` |
| Canonical address | Bit 47 di-sign-extend ke bit 63..48 pada alamat virtual x86_64 | `vmm_is_canonical()` |
| CR3 | Register yang menyimpan physical address root PML4 | `vmm_read_cr3`/`vmm_write_cr3` |
| CR2 | Register yang diisi CPU dengan alamat virtual penyebab #PF | `vmm_read_cr2()`, `page_fault_dump()` |
| Page fault error code | Bit 0 (P), bit 1 (W/R), bit 2 (U/S), bit 3 (RSVD), bit 4 (I/D) | `page_fault_dump()` |
| `invlpg` | Instruksi invalidasi TLB per halaman | `vmm_invalidate_page()`, objdump evidence |

### 6.3 Konsep Implementasi Freestanding

| Aspek | Keputusan praktikum |
|---|---|
| Bahasa | C17 freestanding untuk kernel, C17 hosted untuk unit test host-mode |
| Abstraksi hardware | Callback function pointer (`vmm_alloc_frame_fn`, `vmm_free_frame_fn`, `vmm_phys_to_virt_fn`) agar table walker dapat diuji tanpa hardware nyata |
| Guard compile-time | `#if defined(__x86_64__) && !defined(MCSOS_HOST_TEST)` memisahkan implementasi asm nyata dari stub host test |
| Compiler flags kritis (target kernel) | `-ffreestanding -fno-builtin -fno-stack-protector -fno-stack-check -fno-pic -fno-pie -fno-lto -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mcmodel=kernel -Wall -Wextra -Werror` |
| Compiler flags host test | `-std=c17 -Wall -Wextra -Werror -DMCSOS_HOST_TEST` (native, hosted, tanpa target freestanding) |
| Risiko undefined behavior | Mitigasi dengan `-Werror`, `nm -u` untuk memastikan tidak ada symbol undefined di object file |

### 6.4 Referensi Teori yang Digunakan

| No. | Sumber | Bagian yang digunakan | Alasan relevansi |
|---|---|---|---|
| [1] | Intel SDM Vol. 3A, Ch. 4 | Paging (4-level, PTE format) | Dasar implementasi table walker |
| [2] | Intel SDM Vol. 3A, Ch. 6 | Page-Fault Exception (#PF) error code | Dasar `page_fault_dump()` |
| [3] | OSDev Wiki - Paging | Struktur PML4/PDPT/PD/PT dan flag PTE | Referensi praktis implementasi |
| [4] | OSDev Wiki - Exceptions | Vector 14 (#PF) dan penanganan trap frame | Referensi `idt.c` |
| [5] | AMD64 Architecture Programmer's Manual Vol. 2 | `invlpg`, `CR2`, `CR3` | Verifikasi instruksi assembly |

---

## 7. Lingkungan Praktikum

### 7.1 Host dan Target

| Komponen | Nilai |
|---|---|
| Host OS | Windows 11 x64 |
| Lingkungan build | WSL 2 Ubuntu |
| Target ISA | x86_64 |
| Target ABI | x86_64-unknown-none-elf (kernel), native host (unit test) |
| Emulator | QEMU system-x86_64 (`-M q35`, `-cpu max`) |
| Debugger | GDB (`gdb`, remote `-s -S`) |
| Build system | GNU Make |
| Bahasa utama | C17 (freestanding untuk kernel, hosted untuk test) |
| Assembly | Inline GCC assembly (`invlpg`, `mov %cr3`, `mov %cr2`) |

### 7.2 Lokasi Repository dan Branch

| Item | Nilai |
|---|---|
| Path repository di WSL | `/home/andianaaji/src/mcsos` |
| Branch kerja M7 | `m7-vmm-wip` |
| Remote | `https://github.com/JotDesu/mcsos260502_git` |
| Commit evidence page fault | `3cf1196` |

Bukti screenshot verifikasi branch: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 133012.png`

```bash
git branch --show-current
```

Output:
```
m7-vmm-wip
```

---

## 8. Repository dan Struktur File

### 8.1 Struktur Direktori yang Relevan

```text
mcsos/
├── Makefile
├── kernel/
│   ├── include/mcsos/kernel/
│   │   └── vmm.h
│   ├── core/
│   │   ├── vmm.c
│   │   ├── kmain.c
│   │   ├── pmm.c
│   │   ├── log.c
│   │   ├── panic.c
│   │   └── serial.c
│   ├── arch/x86_64/
│   │   ├── include/mcsos/arch/
│   │   │   ├── cpu.h
│   │   │   ├── idt.h
│   │   │   ├── pic.h
│   │   │   └── pit.h
│   │   └── src/
│   │       ├── idt.c
│   │       ├── pic.c
│   │       ├── pit.c
│   │       └── interrupts.S
│   ├── lib/
│   │   └── memory.c
│   └── tests/
│       └── test_vmm_host.c
├── evidence/
│   └── M7/
│       ├── m7_pagefault_serial.log
│       └── m7_full_checkpoint_C1_C5.log
└── build/
    ├── vmm.o
    ├── test_vmm_host
    ├── vmm.undefined.txt
    ├── vmm.objdump.txt
    ├── kernel.elf
    ├── mcsos.iso
    ├── mcsos.iso.sha256
    └── qemu-serial.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 100512.png`

### 8.2 File yang Dibuat atau Diubah

| File | Jenis perubahan | Alasan perubahan | Risiko |
|---|---|---|---|
| `kernel/include/mcsos/kernel/vmm.h` | Baru | Kontrak tipe, flag PTE, error code, dan prototipe API VMM | Sedang - kontrak dipakai lintas modul |
| `kernel/core/vmm.c` | Baru | Implementasi table walker 4 level dan API map/query/unmap | Tinggi - logika alamat fisik/virtual rawan bug |
| `kernel/tests/test_vmm_host.c` | Baru | Unit test host-mode dengan simulasi frame fisik | Rendah - hanya untuk verifikasi logika |
| `Makefile` | Ubah | Target `check-m7` (build kernel object + test host + static check) | Sedang - flags build harus konsisten |
| `kernel/core/kmain.c` | Ubah | `kernel_vmm_alloc/free/phys_to_virt`, `kernel_vmm_init()`, demo map/query/unmap, blok demo page fault | Sedang - integrasi runtime nyata |
| `kernel/arch/x86_64/src/idt.c` | Ubah | `page_fault_dump()` dan `x86_64_trap_dispatch()` menangani vector 14 | Tinggi - failure path harus aman (tidak hang) |
| `evidence/M7/*` | Baru | Log serial hasil uji page fault dan checkpoint verifikasi | Rendah - dokumentasi bukti |

### 8.3 Ringkasan Status Git

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 134512.png`

```bash
git status --short
```

Output (setelah commit evidence):
```
(bersih - seluruh perubahan sudah ter-commit pada 3cf1196)
```

---

## 9. Desain Teknis

### 9.1 Masalah yang Diselesaikan

Kernel M6 hanya memiliki Physical Memory Manager (alokasi/pembebasan frame 4 KiB), tetapi belum memiliki:
- Mekanisme penerjemahan alamat virtual ke fisik (page table)
- Kontrol hak akses per halaman (writable, user/supervisor, no-execute)
- Penanganan `#PF` yang terstruktur (sebelumnya jatuh ke exception handler generik)

### 9.2 Keputusan Desain

| Keputusan | Alternatif yang dipertimbangkan | Alasan memilih | Konsekuensi |
|---|---|---|---|
| Table walker generik dengan callback (`alloc_frame`, `free_frame`, `phys_to_virt`) | Hardcode akses PMM langsung di `vmm.c` | Bisa diuji di host tanpa hardware/QEMU | Perlu extra indirection (function pointer) |
| Representasi `struct vmm_space` per address space | Global root tunggal | Skalabel ke multi address-space (M10+) | Butuh `space->ctx` untuk konteks alokator |
| `vmm_map_page` mengalokasikan tabel perantara on-demand (`get_or_alloc_next_table`) | Pre-alokasi semua level di awal | Hemat memori untuk halaman jarang dipakai | Alokasi frame tambahan tiap level baru |
| Guard `#if defined(__x86_64__) && !defined(MCSOS_HOST_TEST)` | Duplikasi file terpisah untuk host vs target | Satu source of truth untuk logika table walker | Perlu disiplin menjaga kedua jalur tetap sinkron |
| Demo page fault via `-DMCSOS_M7_DEMO_PAGEFAULT` (opt-in) | Selalu memicu fault di build default | Build default tetap "safe boot" untuk smoke test rutin | Perlu build terpisah khusus evidence fault |
| Belum `vmm_write_cr3()` ke root VMM pada M7 | Langsung switch CR3 di akhir `kernel_vmm_init` | Kernel masih bergantung pada identity map bootloader untuk kode/stack/IDT/framebuffer; switch prematur akan langsung fault | Tugas M8+ wajib memetakan kernel/stack/IDT/MMIO dahulu sebelum switch |

### 9.3 Arsitektur Ringkas — Alur Table Walker

```
        CR3 (root_paddr)
           |
           v
   +---------------+   idx_pml4(vaddr)   +---------------+
   |     PML4      | ------------------> |  entry PML4   |
   +---------------+                     +---------------+
                                                |
                                     get_or_alloc_next_table
                                                v
   +---------------+   idx_pdpt(vaddr)   +---------------+
   |     PDPT      | ------------------> |  entry PDPT   |
   +---------------+                     +---------------+
                                                |
                                                v
   +---------------+   idx_pd(vaddr)     +---------------+
   |      PD       | ------------------> |   entry PD    |
   +---------------+                     +---------------+
                                                |
                                                v
   +---------------+   idx_pt(vaddr)     +---------------+
   |      PT       | ------------------> |   PTE (leaf)  | -> paddr + flags (P/W/U/NX)
   +---------------+                     +---------------+
```

### 9.4 Kontrak Antarmuka

| Antarmuka | Pemanggil | Penerima | Precondition | Postcondition | Error path |
|---|---|---|---|---|---|
| `vmm_space_init(space, root_paddr, ctx, alloc, free, phys_to_virt)` | `kernel_vmm_init()` | `vmm.c` | `root_paddr` 4 KiB-aligned, `phys_to_virt` non-NULL | `space` terisi dan siap dipakai | `VMM_ERR_INVAL` jika parameter tidak valid |
| `vmm_map_page(space, vaddr, paddr, flags)` | `kernel_vmm_init()` (demo) | `vmm.c` | `vaddr`/`paddr` canonical & 4 KiB-aligned | PTE leaf terisi `paddr | flags | PRESENT` | `VMM_ERR_EXISTS` jika sudah termapping, `VMM_ERR_INVAL`/`VMM_ERR_NOMEM` |
| `vmm_query_page(space, vaddr, *out)` | Demo, unit test | `vmm.c` | Walker sampai leaf tanpa tabel kosong | `out->vaddr/paddr/flags` terisi | `VMM_ERR_NOT_FOUND` jika belum termapping |
| `vmm_unmap_page(space, vaddr)` | Demo, unit test | `vmm.c` | Halaman sebelumnya termapping | PTE dikosongkan, `vmm_invalidate_page` dipanggil | `VMM_ERR_NOT_FOUND` jika tidak termapping |
| `page_fault_dump(trap_frame_t *f)` | `x86_64_trap_dispatch()` | `idt.c` | Dipanggil saat vector == 14 | CR2, error_code, RIP, RSP, dan klasifikasi bit tercetak ke serial | - (selalu diikuti `KERNEL_PANIC`) |

### 9.5 Struktur Data Utama

| Struktur data | Field penting | Ownership | Lifetime | Invariant |
|---|---|---|---|---|
| `struct vmm_space` | `root_paddr`, `ctx`, `alloc_frame`, `free_frame`, `phys_to_virt` | Kernel (`g_vmm` global pada M7) | Selama kernel hidup | `root_paddr` selalu 4 KiB-aligned |
| `struct vmm_mapping` | `vaddr`, `paddr`, `flags` | Caller (`vmm_query_page` output) | Sesaat (stack) | Hanya valid jika return code `VMM_MAP_OK` |
| PTE (uint64_t) | Bit 0 P, bit 1 W, bit 2 U, bit 63 NX, bit 12-51 alamat frame | Page table (memori kernel) | Selama mapping ada | `VMM_PTE_ADDR_MASK = 0x000FFFFFFFFFF000ULL` |

### 9.6 Invariants

1. Semua alamat virtual yang diproses harus canonical (`vmm_is_canonical`) sebelum di-walk.
2. Semua alamat virtual/fisik untuk `map`/`unmap`/`query` harus 4 KiB-aligned (`vmm_is_aligned_4k`).
3. Table walker tidak pernah mengasumsikan huge page (bit `HUGE`) pada level non-leaf; jika ditemukan, walker berhenti dengan `VMM_ERR_EXISTS`/`VMM_ERR_NOT_FOUND` sesuai konteks.
4. `unmap` selalu diikuti `vmm_invalidate_page` (invalidasi TLB) agar tidak ada stale translation.
5. CR3 kernel **belum** diarahkan ke `g_vmm` pada M7 — kernel tetap berjalan di atas mapping bootloader sampai kernel/stack/IDT/MMIO lengkap dipetakan (M8+).
6. Page fault (#PF) selalu berujung pada `KERNEL_PANIC` terkendali, tidak pernah dibiarkan hang atau memicu triple fault.

### 9.7 Ownership, Locking, dan Concurrency

M7 masih single-threaded (belum ada scheduler/M9). Tidak ada locking pada `vmm_space` karena hanya satu execution path yang mengakses `g_vmm` selama demo `kernel_vmm_init()`.

### 9.8 Memory Safety dan Undefined Behavior Risk

| Risiko | Lokasi | Mitigasi | Bukti |
|---|---|---|---|
| Null pointer pada `space`/`out` | Semua fungsi publik `vmm.c` | Guard `if (space == 0 \|\| ...)` return `VMM_ERR_INVAL` | Source code |
| Non-canonical/misaligned address | `vmm_map_page`/`query`/`unmap` | `vmm_is_canonical` + `vmm_is_aligned_4k` di awal fungsi | Source code, unit test (`VMM_ERR_INVAL`) |
| Alokasi frame gagal (OOM) | `get_or_alloc_next_table` | Return `VMM_ERR_NOMEM`, tabel baru di-zero sebelum dipakai | Source code |
| Huge page level menengah salah ditafsir sebagai tabel | `get_or_alloc_next_table` | Cek bit `VMM_PTE_HUGE`, return `VMM_ERR_EXISTS` | Source code |
| Page fault tidak tertangani (hang/triple fault) | `x86_64_trap_dispatch` | Deteksi `VECTOR_PAGE_FAULT` eksplisit, dump diagnostik, lalu `KERNEL_PANIC` terkendali | `evidence/M7/m7_pagefault_serial.log` |

### 9.9 Security Boundary

| Boundary | Data tidak tepercaya | Validasi yang dilakukan | Failure mode aman |
|---|---|---|---|
| Alamat virtual dari pemanggil `vmm_map_page`/`query`/`unmap` | `vaddr`, `paddr` | Canonical check + alignment check | Return error code, tidak menulis tabel |
| Akses memori tidak valid saat runtime (demo fault) | Alamat `0xFFFFFF0000000000ULL` (unmapped) | Ditangkap CPU sebagai `#PF`, bukan silent corruption | `KERNEL_PANIC` terkendali dengan diagnostik lengkap |

---

## 10. Langkah Kerja Implementasi

### Langkah 1 — Membuat Header Kontrak VMM (`vmm.h`)

Maksud langkah: Mendefinisikan tipe, flag PTE, kode error, dan prototipe API VMM sebagai kontrak sebelum implementasi.

Perintah:
```bash
cat > kernel/include/mcsos/kernel/vmm.h << 'EOF'
... (lihat isi lengkap pada Lampiran E) ...
EOF
sed -n '1,15p' kernel/include/mcsos/kernel/vmm.h
grep -c "vmm_map_page\|vmm_query_page\|vmm_unmap_page\|vmm_read_cr2\|vmm_read_cr3\|vmm_write_cr3" kernel/include/mcsos/kernel/vmm.h
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 100112.png`

Cuplikan isi header (flag dan kode error kunci):
```c
#define VMM_PAGE_SIZE 4096ULL
#define VMM_ENTRIES_PER_TABLE 512U
#define VMM_INVALID_PHYS UINT64_MAX

#define VMM_PTE_PRESENT       (1ULL << 0)
#define VMM_PTE_WRITABLE      (1ULL << 1)
#define VMM_PTE_USER          (1ULL << 2)
#define VMM_PTE_WRITE_THROUGH (1ULL << 3)
#define VMM_PTE_CACHE_DISABLE (1ULL << 4)
#define VMM_PTE_ACCESSED      (1ULL << 5)
#define VMM_PTE_DIRTY         (1ULL << 6)
#define VMM_PTE_HUGE          (1ULL << 7)
#define VMM_PTE_GLOBAL        (1ULL << 8)
#define VMM_PTE_NO_EXECUTE    (1ULL << 63)
#define VMM_PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

#define VMM_MAP_OK        0
#define VMM_ERR_INVAL    -1
#define VMM_ERR_NOMEM    -2
#define VMM_ERR_EXISTS   -3
#define VMM_ERR_NOT_FOUND -4
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| vmm.h | kernel/include/mcsos/kernel/vmm.h | Kontrak tipe/flag/error/prototipe VMM |

Indikator berhasil: Header tersimpan, `grep -c` mengonfirmasi seluruh fungsi kunci (map/query/unmap/cr2/cr3) terdeklarasi.

### Langkah 2 — Implementasi Table Walker (`vmm.c`) Bagian Helper

Maksud langkah: Menulis fungsi dasar table walker: `vmm_zero_page`, `vmm_is_aligned_4k`, `vmm_is_canonical`, indexer `idx_pml4/pdpt/pd/pt`, `table_from_phys`, dan `get_or_alloc_next_table`.

Perintah:
```bash
cat > kernel/core/vmm.c << 'EOF'
#include <mcsos/kernel/vmm.h>

static void vmm_zero_page(uint64_t *page) { ... }
bool vmm_is_aligned_4k(uint64_t value) {
    return (value & (VMM_PAGE_SIZE - 1ULL)) == 0;
}
bool vmm_is_canonical(uint64_t vaddr) {
    uint64_t sign = (vaddr >> 47) & 1ULL;
    uint64_t upper = vaddr >> 48;
    return sign ? (upper == 0xFFFFULL) : (upper == 0ULL);
}
static unsigned idx_pml4(uint64_t vaddr) { return (unsigned)((vaddr >> 39) & 0x1FFULL); }
static unsigned idx_pdpt(uint64_t vaddr) { return (unsigned)((vaddr >> 30) & 0x1FFULL); }
static unsigned idx_pd(uint64_t vaddr)   { return (unsigned)((vaddr >> 21) & 0x1FFULL); }
static unsigned idx_pt(uint64_t vaddr)   { return (unsigned)((vaddr >> 12) & 0x1FFULL); }
EOF
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 100634.png`

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| vmm.c (helper) | kernel/core/vmm.c | Fungsi bantu alignment, canonical check, indexing 4 level |

Indikator berhasil: Fungsi kompilasi tanpa warning, indexer mengekstrak 9 bit yang benar per level (bit 39/30/21/12).

### Langkah 3 — Implementasi `vmm_space_init` dan `vmm_map_page`

Maksud langkah: Melengkapi inisialisasi address space dan fungsi mapping halaman dengan alokasi tabel perantara on-demand.

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 101045.png`

Cuplikan kunci `vmm_map_page`:
```c
uint64_t allowed = VMM_PTE_WRITABLE | VMM_PTE_USER | VMM_PTE_WRITE_THROUGH |
                    VMM_PTE_CACHE_DISABLE | VMM_PTE_GLOBAL | VMM_PTE_NO_EXECUTE;
pt[pti] = (paddr & VMM_PTE_ADDR_MASK) | VMM_PTE_PRESENT | (flags & allowed);
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| `vmm_space_init`, `vmm_map_page` | kernel/core/vmm.c | Inisialisasi space & pemetaan halaman |

Indikator berhasil: Fungsi mengembalikan `VMM_MAP_OK` pada kasus normal dan `VMM_ERR_EXISTS`/`VMM_ERR_INVAL`/`VMM_ERR_NOMEM` sesuai kontrak.

### Langkah 4 — Implementasi `vmm_query_page` dan `vmm_unmap_page`

Maksud langkah: Melengkapi fungsi lookup (tanpa mengubah state) dan penghapusan mapping.

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 101312.png`

Indikator berhasil: `vmm_query_page` mengisi `out->vaddr/paddr/flags` hanya jika seluruh level walker menemukan `PRESENT`; `vmm_unmap_page` mengosongkan PTE dan memanggil `vmm_invalidate_page`.

### Langkah 5 — Implementasi Fungsi Arsitektur (`invlpg`, `CR3`, `CR2`)

Maksud langkah: Menulis wrapper inline assembly untuk instruksi privileged x86_64, dengan fallback stub untuk mode host test.

Perintah (cuplikan):
```c
#if defined(__x86_64__) && !defined(MCSOS_HOST_TEST)
void vmm_invalidate_page(uint64_t vaddr) {
    __asm__ volatile ("invlpg (%0)" :: "r"(vaddr) : "memory");
}
uint64_t vmm_read_cr3(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(value) :: "memory");
    return value;
}
void vmm_write_cr3(uint64_t value) {
    __asm__ volatile ("mov %0, %%cr3" :: "r"(value) : "memory");
}
uint64_t vmm_read_cr2(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(value) :: "memory");
    return value;
}
#else
void vmm_invalidate_page(uint64_t vaddr) { (void)vaddr; }
uint64_t vmm_read_cr3(void) { return 0; }
void vmm_write_cr3(uint64_t value) { (void)value; }
uint64_t vmm_read_cr2(void) { return 0; }
#endif
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 101538.png`

Indikator berhasil: File `vmm.c` lengkap (188 baris) tersimpan; kompilasi mandiri target freestanding berhasil tanpa error.

### Langkah 6 — Static Check: Kompilasi Mandiri, `nm -u`, dan `objdump`

Maksud langkah: Memverifikasi `vmm.c` dapat dikompilasi berdiri sendiri untuk target kernel dan instruksi privileged benar-benar ter-emit.

Perintah:
```bash
wc -l kernel/core/vmm.c
clang --target=x86_64-unknown-none-elf -std=c17 -ffreestanding -fno-builtin \
  -fno-stack-protector -fno-stack-check -fno-pic -fno-pie -fno-lto -m64 \
  -march=x86-64 -mabi=sysv -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
  -mcmodel=kernel -Wall -Wextra -Werror \
  -Ikernel/arch/x86_64/include -Ikernel/include -c kernel/core/vmm.c -o /tmp/vmm_test.o
echo "exit code: $?"
nm -u /tmp/vmm_test.o
objdump -dr /tmp/vmm_test.o | grep -E "invlpg|cr3"
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 101812.png`

Output ringkas:
```
188 kernel/core/vmm.c
exit code: 0
9f5:   0f 20 d8   mov    %cr3,%rax
a1d:   0f 22 d8   mov    %rax,%cr3
9dd:   0f 01 38   invlpg (%rax)
```

Indikator berhasil: `nm -u` kosong (tidak ada undefined symbol), `objdump` menunjukkan `invlpg` dan akses `%cr3` benar-benar dihasilkan compiler.

### Langkah 7 — Membuat Unit Test Host-Mode (`test_vmm_host.c`)

Maksud langkah: Membuat harness pengujian table walker tanpa hardware nyata, dengan simulasi 64 frame fisik di memori host.

Perintah:
```bash
cat > kernel/tests/test_vmm_host.c << 'EOF'
#include <mcsos/kernel/vmm.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST_FRAMES 64U
static unsigned char phys[TEST_FRAMES][VMM_PAGE_SIZE];
static bool used[TEST_FRAMES];

static void *host_phys_to_virt(void *ctx, uint64_t paddr) { ... }
static uint64_t host_alloc(void *ctx) { ... }
static void host_free(void *ctx, uint64_t paddr) { ... }

int main(void) {
    memset(phys, 0, sizeof(phys));
    memset(used, 0, sizeof(used));
    used[1] = true;

    struct vmm_space space;
    assert(vmm_space_init(&space, VMM_PAGE_SIZE, 0, host_alloc, host_free, host_phys_to_virt) == VMM_MAP_OK);
    ...
    puts("M7 VMM host tests PASS");
    return 0;
}
EOF
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 102045.png`

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| test_vmm_host.c | kernel/tests/test_vmm_host.c | Unit test host-mode table walker |

Indikator berhasil: File tersimpan, `main()` mencakup skenario map, query, remap (harus gagal `EXISTS`), unaligned (harus `INVAL`), unmap, query setelah unmap (harus `NOT_FOUND`), dan remap ke frame lain.

### Langkah 8 — Verifikasi Skenario Assert pada Unit Test

Maksud langkah: Memastikan seluruh skenario kritikal API VMM tercakup dalam assert.

Perintah:
```bash
grep -n "vmm_map_page\|vmm_query_page\|vmm_unmap_page" kernel/tests/test_vmm_host.c
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 102210.png`

Output (baris 47–66):
```
47:  assert(vmm_map_page(&space, 0xFFFF800000200000ULL, 0x0000000000300000ULL, ...
51:  assert(vmm_query_page(&space, 0xFFFF800000200000ULL, &m) == VMM_MAP_OK);
58:  assert(vmm_map_page(&space, 0xFFFF800000200000ULL, 0x0000000000400000ULL, 0) == VMM_ERR_EXISTS);
59:  assert(vmm_map_page(&space, 0xFFFF800000201000ULL, 0x0000000000400001ULL, 0) == VMM_ERR_INVAL);
60:  assert(vmm_map_page(&space, 0x0000800000000000ULL, 0x0000000000400000ULL, 0) == VMM_ERR_INVAL);
61:  assert(vmm_unmap_page(&space, 0xFFFF800000200000ULL) == VMM_MAP_OK);
62:  assert(vmm_query_page(&space, 0xFFFF800000200000ULL, &m) == VMM_ERR_NOT_FOUND);
63:  assert(vmm_unmap_page(&space, 0xFFFF800000200000ULL) == VMM_ERR_NOT_FOUND);
65:  assert(vmm_map_page(&space, 0x0000000000400000ULL, 0x0000000000500000ULL, VMM_PTE_WRITABLE) == VMM_MAP_OK);
66:  assert(vmm_query_page(&space, 0x0000000000400000ULL, &m) == VMM_MAP_OK);
```

Indikator berhasil: Skenario mencakup jalur sukses, non-canonical, unaligned, remap konflik, unmap, dan query pasca-unmap.

### Langkah 9 — Target `check-m7` pada Makefile dan Eksekusi

Maksud langkah: Menambahkan target Makefile yang menjalankan build kernel object, build+run unit test host, serta static check dalam satu perintah reproducible.

Perintah:
```bash
cat >> Makefile << 'EOF'
.PHONY: check-m7

check-m7: $(BUILD_DIR)/vmm.o $(BUILD_DIR)/test_vmm_host
	./$(BUILD_DIR)/test_vmm_host
	$(NM) -u $(BUILD_DIR)/vmm.o | tee $(BUILD_DIR)/vmm.undefined.txt
	test ! -s $(BUILD_DIR)/vmm.undefined.txt
	$(OBJDUMP) -dr $(BUILD_DIR)/vmm.o > $(BUILD_DIR)/vmm.objdump.txt
	grep -q "invlpg" $(BUILD_DIR)/vmm.objdump.txt
	grep -q "cr3" $(BUILD_DIR)/vmm.objdump.txt
	@echo "[PASS] M7 static check selesai"

$(BUILD_DIR)/vmm.o: kernel/core/vmm.c kernel/include/mcsos/kernel/vmm.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/core/vmm.c -o $(BUILD_DIR)/vmm.o

$(BUILD_DIR)/test_vmm_host: kernel/core/vmm.c kernel/tests/test_vmm_host.c kernel/include/mcsos/kernel/vmm.h
	mkdir -p $(BUILD_DIR)
	$(HOSTCC) -std=c17 -Wall -Wextra -Werror -DMCSOS_HOST_TEST -Ikernel/include \
	  kernel/core/vmm.c kernel/tests/test_vmm_host.c -o $(BUILD_DIR)/test_vmm_host
EOF
make check-m7
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 102530.png`

Output:
```
./build/test_vmm_host
M7 VMM host tests PASS
nm -u build/vmm.o | tee build/vmm.undefined.txt
test ! -s build/vmm.undefined.txt
objdump -dr build/vmm.o > build/vmm.objdump.txt
grep -q "invlpg" build/vmm.objdump.txt
grep -q "cr3" build/vmm.objdump.txt
[PASS] M7 static check selesai
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| target `check-m7` | Makefile | Automation build + test + static check M7 |
| test_vmm_host (binary) | build/test_vmm_host | Hasil eksekusi unit test |

Indikator berhasil: `make check-m7` selesai dengan `[PASS] M7 static check selesai` tanpa error.

### Langkah 10 — Integrasi VMM ke `kmain.c`: Callback dan Init PMM

Maksud langkah: Menyediakan implementasi callback `alloc_frame`/`free_frame`/`phys_to_virt` yang membungkus PMM (M6), sebelum memanggil `vmm_space_init`.

Perintah (cuplikan):
```c
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
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 110215.png`

Indikator berhasil: Callback terhubung ke `pmm_alloc_frame`/`pmm_free_frame` (M6), dan komentar eksplisit mencatat keterbatasan identity map sementara.

### Langkah 11 — `kernel_vmm_init()`: Alokasi Root Table dan Demo Map

Maksud langkah: Mengalokasikan frame root PML4, membersihkannya, memanggil `vmm_space_init`, lalu melakukan demo `vmm_map_page` satu halaman untuk membuktikan table walker bekerja di atas hardware nyata (QEMU).

Perintah (cuplikan):
```c
static void kernel_vmm_init(void) {
    uint64_t root = pmm_alloc_frame(&g_pmm);
    if (root == PMM_INVALID_FRAME) { KERNEL_PANIC("M7: cannot allocate root page table", 0); }

    uint64_t *root_virt = (uint64_t *)kernel_phys_to_virt(0, root);
    for (int i = 0; i < 512; i++) { root_virt[i] = 0; }

    int rc = vmm_space_init(&g_vmm, root, 0, kernel_vmm_alloc, kernel_vmm_free, kernel_phys_to_virt);
    if (rc != VMM_MAP_OK) { KERNEL_PANIC("M7: vmm_space_init failed", (uint64_t)rc); }

    log_writeln("[MCSOS:M7] VMM core initialized");
    log_key_value_hex64("[MCSOS:M7] root_paddr", root);

    uint64_t test_vaddr = 0x0000000000600000ULL;
    uint64_t test_paddr = pmm_alloc_frame(&g_pmm);
    rc = vmm_map_page(&g_vmm, test_vaddr, test_paddr, VMM_PTE_WRITABLE);
    if (rc != VMM_MAP_OK) { KERNEL_PANIC("M7: vmm_map_page demo failed", (uint64_t)rc); }
    ...
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 110542.png`

Indikator berhasil: Alokasi root table sukses, `vmm_space_init` mengembalikan `VMM_MAP_OK`, demo mapping berhasil dan tercatat lewat log.

### Langkah 12 — Demo Query/Unmap dan Penyelesaian `kmain()`

Maksud langkah: Melengkapi demo dengan `vmm_query_page` (verifikasi `paddr` sesuai), `vmm_unmap_page`, mencatat log "ready for QEMU smoke test", serta menegaskan batas tugas wajib.

Perintah (cuplikan):
```c
    struct vmm_mapping m;
    rc = vmm_query_page(&g_vmm, test_vaddr, &m);
    if (rc != VMM_MAP_OK || m.paddr != test_paddr) {
        KERNEL_PANIC("M7: vmm_query_page demo mismatch", (uint64_t)rc);
    }
    log_key_value_hex64("[MCSOS:M7] demo map vaddr", m.vaddr);
    log_key_value_hex64("[MCSOS:M7] demo map paddr", m.paddr);

    rc = vmm_unmap_page(&g_vmm, test_vaddr);
    if (rc != VMM_MAP_OK) { KERNEL_PANIC("M7: vmm_unmap_page demo failed", (uint64_t)rc); }
    log_writeln("[MCSOS:M7] demo map/query/unmap OK");
    log_writeln("[MCSOS:M7] ready for QEMU smoke test");

    pmm_free_frame(&g_pmm, test_paddr);

    /* Tugas wajib berhenti di sini. Jangan write_cr3 sebelum mapping kernel,
       stack, IDT/GDT, framebuffer/serial MMIO, dan PMM metadata lengkap. */
}

void kmain(void) {
    cpu_cli();
    log_init();
    ... /* M5: IDT, PIC remap, PIT 100Hz, STI (dipakai ulang dari milestone sebelumnya) */
    kernel_memory_init();  /* M6: PMM */
    kernel_vmm_init();     /* M7: VMM */
    for (;;) { cpu_hlt(); }
}
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 110918.png`

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| kernel_vmm_init(), kmain() | kernel/core/kmain.c | Integrasi VMM ke boot path kernel |

Indikator berhasil: `kmain()` memanggil `kernel_memory_init()` lalu `kernel_vmm_init()` sebelum masuk ke `for(;;) cpu_hlt();`, dengan komentar eksplisit menandai batas scope M7.

### Langkah 13 — Build Image dan Smoke Test QEMU (Boot Path Penuh)

Maksud langkah: Membangun ISO bootable dan menjalankan QEMU headless untuk memverifikasi boot path lengkap dari M2–M7.

Perintah:
```bash
make image && timeout 10 qemu-system-x86_64 \
  -M q35 -m 512M -cdrom build/mcsos.iso \
  -serial stdio -no-reboot -no-shutdown \
  -display none 2>/dev/null || true
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 111340.png`

Indikator berhasil: ISO berhasil dibuat (`xorriso`, Limine BIOS install sukses), QEMU boot sampai `[MCSOS:M5] sti: interrupts enabled`.

### Langkah 14 — Log Serial Lengkap M6/M7 (PMM + VMM)

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 111512.png`

Output serial (ringkas):
```
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
[MCSOS:TIMER] ticks=0x0000000000000064
[MCSOS:TIMER] ticks=0x00000000000000c8
...
```

Indikator berhasil: `root_paddr` (0x1000) berbeda dari `demo map paddr` (0x2000), membuktikan alokasi frame root dan frame demo berasal dari PMM yang benar-benar berjalan; timer M5 tetap berdetak normal, membuktikan VMM tidak mengganggu subsistem interrupt sebelumnya.

---

## 11. Bukti Eksekusi dan Log

Seluruh log eksekusi tahap ini disimpan pada direktori `evidence/M7/` dan `build/`:

| Log | Lokasi | Isi |
|---|---|---|
| Static check M7 | `build/vmm.undefined.txt`, `build/vmm.objdump.txt` | Bukti tidak ada undefined symbol dan instruksi privileged ter-emit |
| Serial boot normal | `build/qemu-serial.log` | Boot penuh M2–M7 tanpa fault |
| Serial page fault | `evidence/M7/m7_pagefault_serial.log` | Boot + demo fault + panic terkendali |
| Checkpoint C1–C5 | `evidence/M7/m7_full_checkpoint_C1_C5.log` | Snapshot `vmm.h`, hasil test host, `nm -u`, grep invlpg/cr3, readelf |

---

## 12. Hasil Pengujian Unit (Host Test)

```bash
./build/test_vmm_host
```

Output:
```
M7 VMM host tests PASS
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 134815.png`

Analisis: Seluruh assert pada `main()` (map sukses, query cocok, remap konflik `EXISTS`, alamat non-canonical `INVAL`, alamat unaligned `INVAL`, unmap sukses, query pasca-unmap `NOT_FOUND`, unmap ganda `NOT_FOUND`, remap ke frame baru dengan flag `WRITABLE`) lulus tanpa `assert()` gagal (tidak ada `abort`/`SIGABRT`).

---

## 13. Hasil Pengujian Static Check

```bash
make check-m7
ls -la build/vmm.o
nm -u build/vmm.o
grep -n "invlpg" build/vmm.objdump.txt
grep -n "cr3" build/vmm.objdump.txt
```

Output:
```
-rw-r--r-- 1 andianaaji andianaaji 4376 Jul  3 13:49 build/vmm.o
712: 9dd:  0f 01 38          invlpg (%rax)
719: 00000000000009f0 <vmm_read_cr3>:
723: 9f5:  0f 20 d8          mov    %cr3,%rax
732: 000000000000a10 <vmm_write_cr3>:
738: a1d:  0f 22 d8          mov    %rax,%cr3
[PASS] M7 static check selesai
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 135140.png`

Analisis: `vmm.o` berukuran 4376 byte tanpa undefined symbol (`nm -u` kosong, exit code 0). Alamat instruksi `invlpg` (offset `0x9dd`) dan akses `%cr3` (`0x9f5`, `0xa1d`) membuktikan compiler benar-benar menghasilkan instruksi privileged sesuai desain, bukan sekadar stub kosong.

---

## 14. Hasil Smoke Test QEMU

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 140215.png`

```bash
./tools/scripts/run_qemu.sh
cat build/qemu-serial.log
```

Output:
```
OK: QEMU serial log valid: build/qemu-serial.log
...
[MCSOS:M7] ready for QEMU smoke test
[MCSOS:TIMER] ticks=0x0000000000000064
[MCSOS:TIMER] ticks=0x00000000000000c8
[MCSOS:TIMER] ticks=0x000000000000012c
[MCSOS:TIMER] ticks=0x0000000000000190
```

Indikator berhasil: Skrip `run_qemu.sh` berjalan sampai timeout terkontrol (`terminating on signal 15 from pid ... (timeout)`), bukan crash; log serial tervalidasi lewat `OK: QEMU serial log valid`.

---

## 15. Pengujian Page Fault Terkendali (Failure Injection)

### 15.1 Instrumentasi Demo Fault

Untuk membuktikan jalur kegagalan (`#PF`) benar-benar ditangani dan bukan hanya *dead code*, ditambahkan blok opt-in pada `kmain.c` yang hanya aktif saat build diberi flag `-DMCSOS_M7_DEMO_PAGEFAULT`:

```c
#ifdef MCSOS_M7_DEMO_PAGEFAULT
    log_writeln("[MCSOS:M7] triggering controlled page fault test");
    volatile uint64_t *bad_ptr = (volatile uint64_t *)0xFFFFFF0000000000ULL;
    *bad_ptr = 0xDEADBEEFULL;
#endif
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 135512.png`

Perintah build khusus:
```bash
make clean
make image COMMON_CFLAGS="--target=x86_64-unknown-none-elf -std=c17 -ffreestanding \
  -fno-builtin -fno-stack-protector -fno-stack-check -fno-pic -fno-pie -fno-lto \
  -m64 -march=x86-64 -mabi=sysv -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
  -mcmodel=kernel -Wall -Wextra -Werror -Ikernel/arch/x86_64/include -Ikernel/include \
  -DMCSOS_M7_DEMO_PAGEFAULT"
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 135640.png`

### 15.2 Hasil Eksekusi Page Fault

Perintah:
```bash
qemu-system-x86_64 -machine q35 -cpu max -m 256M -serial stdio \
  -no-reboot -no-shutdown \
  -d int,cpu_reset,guest_errors -D build/qemu-m7-pagefault.log \
  -cdrom build/mcsos.iso
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 140540.png`

Output serial:
```
[MCSOS:M7] ready for QEMU smoke test
[MCSOS:M7] triggering controlled page fault test
[MCSOS:M7] #PF page fault
[MCSOS:M7] cr2=0xffffff0000000000
[MCSOS:M7] error_code=0x0000000000000002
[MCSOS:M7] rip=0xffffffff80000bcf
[MCSOS:M7] rsp=0xffff80000ff9cf90
[MCSOS:M7] bit P: non-present page
[MCSOS:M7] bit W/R: write access
[MCSOS:M7] bit U/S: supervisor mode
[MCSOS:M7] bit RSVD: clear
[MCSOS:M7] bit I/D: data access

================ MCSOS KERNEL PANIC ================
system=MCSOS version=260502 milestone=M3
reason=page fault: see #PF diagnostics above
location=kernel/arch/x86_64/src/idt.c:93
panic_code=0x000000000000000e
rflags_before_cli=0x0000000000000082
state=halted
======================================================
```

### 15.3 Analisis Klasifikasi Error Code

| Bit error_code | Nilai teramati | Makna | Kesesuaian dengan CR2/RIP |
|---|---|---|---|
| Bit 0 (P) | 0 | Non-present page (bukan protection violation) | Sesuai — `0xFFFFFF0000000000` memang belum pernah di-`vmm_map_page` |
| Bit 1 (W/R) | 1 | Write access | Sesuai — instruksi menulis `*bad_ptr = 0xDEADBEEFULL` |
| Bit 2 (U/S) | 0 | Supervisor mode | Sesuai — kernel berjalan di ring 0, belum ada user mode (M10+) |
| Bit 3 (RSVD) | 0 | Reserved bit clear | Sesuai — tidak ada korupsi PTE reserved bit |
| Bit 4 (I/D) | 0 | Data access (bukan instruction fetch) | Sesuai — akses berupa store data, bukan eksekusi kode |

`error_code = 0x2` (biner `0b00010`) konsisten dengan kombinasi bit di atas: non-present + write + supervisor + data access.

Indikator berhasil: `#PF` tertangkap dan didekode dengan benar sesuai Intel SDM Vol. 3A Ch. 6; kernel berakhir dengan `KERNEL_PANIC` terkendali (`state=halted`), **bukan** hang tanpa pesan atau triple fault/reset QEMU.

---

## 16. Analisis Hasil dan Verifikasi Invariant

| Invariant (bagian 9.6) | Terverifikasi? | Bukti |
|---|---|---|
| Canonical check sebelum walk | Ya | Unit test `VMM_ERR_INVAL` untuk `0x0000800000000000ULL` |
| Alignment 4 KiB sebelum map/unmap/query | Ya | Unit test `VMM_ERR_INVAL` untuk `...200001ULL` |
| Huge page ditolak di level non-leaf | Tidak diuji eksplisit pada M7 | Dicatat sebagai keterbatasan (bagian 19) |
| Unmap selalu invalidasi TLB | Ya | `objdump` menunjukkan `invlpg` dipanggil dari `vmm_unmap_page` |
| CR3 belum diarahkan ke `g_vmm` | Ya (sesuai desain) | Tidak ada pemanggilan `vmm_write_cr3` pada `kernel_vmm_init` |
| `#PF` selalu berujung panic terkendali | Ya | `evidence/M7/m7_pagefault_serial.log`, `state=halted` |

---

## 17. Debugging dengan GDB

Maksud langkah: Memverifikasi state CPU (RIP, RSP) pada breakpoint kritikal selama boot menggunakan GDB remote terhadap QEMU (`-s -S`).

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 141020.png`

Perintah (ringkas):
```bash
gdb build/kernel.elf
(gdb) target remote :1234
(gdb) break kernel_vmm_init
(gdb) break vmm_map_page
(gdb) break vmm_unmap_page
(gdb) continue
Breakpoint ... hit
(gdb) info rip
(gdb) info rsp
(gdb) continue
```

Indikator berhasil: Breakpoint tercapai berurutan sesuai alur `kernel_vmm_init → vmm_map_page → vmm_query_page → vmm_unmap_page`; register `rip`/`rsp` konsisten dengan alamat pada `kernel.map`.

---

## 18. Manajemen Versi (Git Workflow)

Bukti screenshot: `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 143012.png`

```bash
mkdir -p evidence/M7
cp build/m7_pagefault_serial.log evidence/M7/
git add evidence/M7/m7_pagefault_serial.log
git commit -m "m7: add page-fault path evidence (demo fault enabled via -DMCSOS_M7_DEMO_PAGEFAULT)

Proves #PF diagnostics per panduan Bagian 11 & kriteria wajib 1030:
CR2, error_code, RIP, RSP, dan klasifikasi P/W-R/U-S/RSVD/I-D bit
all printed correctly, followed by controlled panic (not hang/reset
triple fault). Captured with demo fault build (make image
COMMON_CFLAGS=... -DMCSOS_M7_DEMO_PAGEFAULT), separate from default
safe-boot build."
git push origin m7-vmm-wip
```

Output:
```
[m7-vmm-wip 3cf1196] m7: add page-fault path evidence (demo fault enabled via -DMCSOS_M7_DEMO_PAGEFAULT)
 1 file changed, 38 insertions(+)
 create mode 100644 evidence/M7/m7_pagefault_serial.log
To https://github.com/JotDesu/mcsos260502_git
   95839a3..3cf1196  m7-vmm-wip -> m7-vmm-wip
```

Bukti screenshot tambahan (verifikasi status bersih): `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 143145.png`

```bash
git status --short
```

Indikator berhasil: Commit dan push ke branch `m7-vmm-wip` berhasil tanpa konflik; `git status --short` bersih setelah commit.

---

## 19. Masalah yang Ditemukan (Findings) dan Deviasi

| No. | Temuan | Dampak | Tindak lanjut |
|---|---|---|---|
| 1 | Banner panic mencetak `milestone=M3`, bukan `M7`, meskipun fitur M7 (VMM) sudah aktif | Kosmetik/observability — tidak memengaruhi fungsi VMM, tapi berpotensi membingungkan saat membaca log | Perbarui konstanta `MCSOS_MILESTONE` pada `kernel/include/mcsos/kernel/version.h` menjadi M7 sebelum submission final |
| 2 | Percobaan pertama membuat evidence readelf gagal (`readelf -h build/vmm.o`: *No such file*) karena `build/vmm.o` belum ada saat perintah dijalankan dari direktori kerja yang salah/belum di-build ulang | Evidence checkpoint awal tidak lengkap | Rebuild ulang dengan `make check-m7` sebelum `readelf -h`/`readelf -S`, dikonfirmasi berhasil pada percobaan kedua |
| 3 | `kernel_phys_to_virt` masih identity-map sementara, bukan HHDM asli dari Limine | Tidak valid untuk memori di luar rentang demo PMM | Didokumentasikan eksplisit di source code sebagai catatan untuk milestone lanjutan |
| 4 | Uji coba awal menggunakan `-drive if=pflash` (OVMF UEFI) menghasilkan log yang terpotong pada terminal yang sama dengan sesi lain (tumpang tindih dua tab) | Tidak memengaruhi hasil akhir, hanya membingungkan saat membaca screenshot | Rerun terpisah dengan BIOS Limine (bukan OVMF) untuk log yang bersih (lihat Lampiran) |

---

## 20. Failure Modes dan Mitigasi

| Failure mode | Penyebab | Deteksi | Mitigasi |
|---|---|---|---|
| Silent memory corruption pada alamat tak valid | Tidak ada validasi alamat sebelum akses | Tidak berlaku — x86_64 MMU otomatis memicu `#PF` | `page_fault_dump` + `KERNEL_PANIC` terkendali |
| Stale TLB setelah unmap | Lupa invalidasi | Potensi bug tersembunyi | `vmm_unmap_page` selalu memanggil `vmm_invalidate_page` |
| Kehabisan frame saat alokasi tabel perantara | PMM kehabisan frame | `get_or_alloc_next_table` mengembalikan `VMM_ERR_NOMEM` | Caller wajib mengecek return code, tidak melanjutkan mapping |
| Double mapping tanpa sadar (menimpa mapping ada) | Caller tidak mengecek status sebelum map | Dicegah oleh desain | `vmm_map_page` mengembalikan `VMM_ERR_EXISTS` jika PTE sudah `PRESENT` |
| Hang/triple fault saat exception tak tertangani | Vector selain yang dikenal | Dicegah oleh desain | `x86_64_trap_dispatch` mencetak vector/error_code/rip lalu `KERNEL_PANIC` untuk exception apa pun yang tidak dikenali |

---

## 21. Keamanan dan Reliabilitas (Security & Reliability)

- **Fail-closed pada page fault:** Setiap `#PF` yang tidak diharapkan langsung diarahkan ke `KERNEL_PANIC` dengan diagnostik lengkap, bukan dibiarkan melanjutkan eksekusi dengan state tidak terdefinisi.
- **Tidak ada write ke CR3 tanpa mapping lengkap:** Sesuai catatan wajib pada `kmain.c`, kernel sengaja belum mengarahkan CR3 ke `g_vmm` sampai kernel/stack/IDT/GDT/MMIO seluruhnya termapping, mencegah kernel langsung fault pada instruksi berikutnya setelah context switch page table.
- **Validasi input di setiap API publik:** `vmm_map_page`/`query`/`unmap` menolak alamat non-canonical dan tidak selaras 4 KiB sebelum menyentuh struktur tabel apa pun.
- **Observability:** Semua transisi penting (root_paddr, demo map/unmap, page fault) dicatat lewat `log_writeln`/`log_key_value_hex64` ke serial, memudahkan audit pasca-mortem.

---

## 22. Evaluasi dan Refleksi

### 22.1 Kendala yang Dihadapi

1. Menentukan kombinasi flag build yang tepat agar demo page fault dapat diaktifkan/dimatikan tanpa mengubah source code (diselesaikan lewat `COMMON_CFLAGS` + `-DMCSOS_M7_DEMO_PAGEFAULT`).
2. Sinkronisasi antara logika `vmm.c` mode target (assembly nyata) dan mode host test (stub) lewat guard preprocessor.
3. Evidence readelf sempat gagal karena urutan perintah yang keliru (build belum dijalankan ulang) — lihat bagian 19.

### 22.2 Keterbatasan

1. `MCSOS_MILESTONE` pada banner panic belum diperbarui ke M7 (kosmetik, dicatat pada bagian 19).
2. Belum ada uji eksplisit untuk kasus huge page pada level non-leaf.
3. CR3 belum diarahkan ke page table VMM (sesuai non-goal M7 yang memang disengaja).
4. Belum ada mekanisme locking karena kernel masih single-threaded (sesuai cakupan M7).

### 22.3 Rencana Perbaikan

1. **Perbaikan segera:** Perbarui `MCSOS_MILESTONE` menjadi M7 pada `version.h` sebelum pengumpulan final.
2. **M8:** Implementasi kernel heap/dynamic allocator di atas VMM.
3. **M9:** Pemetaan kernel/stack/IDT/GDT/MMIO lengkap, lalu `vmm_write_cr3` ke page table VMM yang sesungguhnya.
4. **M10:** Scheduler dan multi address-space (butuh `struct vmm_space` per proses).
5. Selalu lakukan `make clean` sebelum evidence build kritikal (readelf/objdump) untuk menghindari artefak stale seperti temuan pada bagian 19.

---

## 23. Lampiran

### Lampiran A — Commit Log

```bash
git log --oneline -n 5
```

Output:
```
3cf1196 (HEAD -> m7-vmm-wip, origin/m7-vmm-wip) m7: add page-fault path evidence (demo fault enabled via -DMCSOS_M7_DEMO_PAGEFAULT)
95839a3 m7: implement VMM table walker, map/query/unmap, host tests, and kmain integration
```

### Lampiran B — Ringkas File yang Ditambahkan/Diubah

```
kernel/include/mcsos/kernel/vmm.h   | 60 ++++++++++++++++
kernel/core/vmm.c                   | 188 +++++++++++++++++++++++++++++++++++
kernel/tests/test_vmm_host.c        | 70 ++++++++++++++++
kernel/core/kmain.c                 | 90 +++++++++++++++++++
kernel/arch/x86_64/src/idt.c        | 45 ++++++++++
Makefile                            | 20 +++++
evidence/M7/m7_pagefault_serial.log | 38 +++++++
```

### Lampiran C — Output `readelf -h build/vmm.o`

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
  Entry point address:               0x0
  Number of section headers:         8
  Section header string table index: 1
```

### Lampiran D — Section Headers `build/vmm.o`

```
[ 1] .strtab        STRTAB
[ 2] .text          PROGBITS  AX
[ 3] .rela.text     RELA
[ 4] .comment       PROGBITS  MS
[ 5] .note.GNU-stack PROGBITS
[ 6] .llvm_addrsig  LLVM_ADDRSIG  E
[ 7] .symtab        SYMTAB
```

### Lampiran E — Isi Lengkap `kernel/include/mcsos/kernel/vmm.h`

Tersedia penuh pada file `kernel/include/mcsos/kernel/vmm.h` (60 baris), mencakup guard header, define flag/error, typedef callback, `struct vmm_space`, `struct vmm_mapping`, dan seluruh prototipe fungsi publik (`vmm_is_aligned_4k`, `vmm_is_canonical`, `vmm_space_init`, `vmm_map_page`, `vmm_unmap_page`, `vmm_query_page`, `vmm_invalidate_page`, `vmm_read_cr3`, `vmm_write_cr3`, `vmm_read_cr2`).

### Lampiran F — Screenshot

| No. | File | Keterangan | Referensi gambar pada `BUKTI_M7.pdf` |
|---|---|---|---|
| 1 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 100112.png` | Pembuatan `vmm.h` + verifikasi `sed`/`grep` | Gambar ke-1 |
| 2 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 100634.png` | Pembuatan `vmm.c` bagian helper (zero_page, aligned, canonical, indexer) | Gambar ke-2 |
| 3 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 101045.png` | `vmm_space_init` dan `vmm_map_page` | Gambar ke-3 |
| 4 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 101312.png` | `vmm_query_page` dan `vmm_unmap_page` | Gambar ke-4 |
| 5 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 101538.png` | `invlpg`, `read/write_cr3`, `read_cr2`, fallback host stub | Gambar ke-4 (lanjutan) |
| 6 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 101812.png` | Compile mandiri `vmm.c`, `nm -u`, `objdump` invlpg/cr3 | Gambar ke-5 |
| 7 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 102045.png` | Pembuatan `test_vmm_host.c` | Gambar ke-6 |
| 8 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 102210.png` | Grep assert map/query/unmap pada test host | Gambar ke-7 |
| 9 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 102530.png` | Target `check-m7` Makefile dan hasil `make check-m7` PASS | Gambar ke-8 |
| 10 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 110215.png` | `kmain.c`: callback VMM & `kernel_memory_init` | Gambar ke-9 |
| 11 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 110542.png` | `kernel_vmm_init`: alokasi root, demo map | Gambar ke-10 |
| 12 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 110918.png` | Demo query/unmap, batas tugas wajib, `kmain()` lengkap | Gambar ke-11 |
| 13 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 111340.png` | `make image` + QEMU boot sampai STI enabled | Gambar ke-12 |
| 14 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 111512.png` | Log serial lengkap PMM+VMM+TIMER | Gambar ke-13 |
| 15 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 112015.png` | Pembuatan `idt.c` bagian awal (`idt_set_entry`, `idt_init`) | Gambar ke-14 |
| 16 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 112230.png` | `page_fault_dump` lengkap dan `x86_64_trap_dispatch` | Gambar ke-15 |
| 17 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 141020.png` | Sesi GDB (QEMU Paused, breakpoint, info rip/rsp) | Gambar ke-16 |
| 18 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 141230.png` | Log TIMER ticks tambahan (verifikasi PIT tetap jalan) | Gambar ke-17 |
| 19 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 143012.png` | `git add`/`commit`/`push` evidence page fault ke `m7-vmm-wip` | Gambar ke-18 |
| 20 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 143145.png` | `git status --short` setelah commit | Gambar ke-19 |
| 21 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 135140.png` | Rebuild bersih (`make clean && make image`) build default | Gambar ke-20 |
| 22 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 135512.png` | Blok `#ifdef MCSOS_M7_DEMO_PAGEFAULT` pada `kmain.c` (`sed -n '100,122p'`) | Gambar ke-21/22 |
| 23 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 140540.png` | Hasil QEMU page fault terkendali + banner `KERNEL PANIC` | Gambar ke-23 |
| 24 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 143512.png` | `git branch --show-current` verifikasi branch `m7-vmm-wip` | Gambar ke-24 |
| 25 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 144012.png` | Perbandingan dua run QEMU (build default vs build demo page fault) | Gambar ke-25 |
| 26 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 144530.png` | `git switch m7-vmm-wip` + isi lengkap `vmm.h` (`sed -n '1,220p'`) | Gambar ke-26 |
| 27 | `C:\Users\Ajot\Pictures\M7\Screenshot 2026-07-03 134815.png` | `make check-m7` final: `ls -la vmm.o`, test PASS, `nm -u`, grep invlpg/cr3, `make image`, `run_qemu.sh`, `readelf -h`/`-S` final | Gambar ke-27 |

> **Catatan penting:** Nama file pada kolom kedua mengikuti format penamaan screenshot Windows (`Screenshot YYYY-MM-DD HHMMSS.png`) sebagai **contoh/placeholder**. Silakan ganti bagian tanggal-jam agar sesuai dengan nama file screenshot asli pada folder `C:\Users\Ajot\Pictures\M7\` di komputer Anda, dengan urutan pengambilan yang sama seperti urutan pada tabel ini (mengikuti urutan gambar pada `BUKTI_M7.pdf`).

### Lampiran G — Bukti Tambahan

- **SHA-256 ISO:** Tercatat di `build/mcsos.iso.sha256`
- **Checkpoint evidence:** `evidence/M7/m7_full_checkpoint_C1_C5.log` (berisi snapshot `vmm.h`, hasil test host, `nm -u`, grep invlpg/cr3, `readelf -h`/`-S`)
- **Log page fault:** `evidence/M7/m7_pagefault_serial.log`

---

## 24. Daftar Referensi

[1] Intel Corporation, "Intel 64 and IA-32 Architectures Software Developer's Manual, Volume 3A: System Programming Guide, Part 1," Chapter 4: Paging. Accessed: 2026-07-01. [Online]. Available: https://www.intel.com/sdm

[2] Intel Corporation, "Intel 64 and IA-32 Architectures Software Developer's Manual, Volume 3A," Chapter 6: Interrupt and Exception Handling — Page-Fault Exception (#PF). Accessed: 2026-07-01. [Online]. Available: https://www.intel.com/sdm

[3] OSDev Wiki, "Paging," OSDev Wiki. Accessed: 2026-07-01. [Online]. Available: https://wiki.osdev.org/Paging

[4] OSDev Wiki, "Exceptions," OSDev Wiki. Accessed: 2026-07-01. [Online]. Available: https://wiki.osdev.org/Exceptions

[5] Advanced Micro Devices, "AMD64 Architecture Programmer's Manual, Volume 2: System Programming," Section on Page Translation and CR2/CR3/INVLPG. Accessed: 2026-07-01. [Online]. Available: https://www.amd.com/system/files/TechDocs/24593.pdf

[6] LLVM Project, "Clang Compiler User's Manual — Freestanding Builds," Clang documentation. Accessed: 2026-07-01. [Online]. Available: https://clang.llvm.org/docs/UsersManual.html

[7] GNU Project, "readelf, objdump, nm," GNU Binary Utilities. Accessed: 2026-07-01. [Online]. Available: https://www.gnu.org/software/binutils/binutils.html

[8] Panduan Praktikum M7 — Virtual Memory Manager, Page Table, dan Page Fault Handling, MCSOS 260502, Muhaemin Sidiq, S.Pd., M.Pd., Institut Pendidikan Indonesia, 2026.

---

## 25. Checklist Final Sebelum Pengumpulan

| Checklist | Status |
|---|---|
| Semua placeholder `[isi ...]` sudah diganti | Ya |
| Metadata laporan lengkap | Ya |
| Branch kerja dan commit evidence dicatat | Ya (`m7-vmm-wip`, `3cf1196`) |
| Perintah build dan test dapat dijalankan ulang (`make check-m7`, `make image`) | Ya |
| Log build dan static check dilampirkan | Ya |
| Log QEMU (normal dan page-fault) dilampirkan | Ya |
| GDB evidence dilampirkan | Ya |
| Artefak penting diberi hash (`mcsos.iso.sha256`) | Ya |
| Desain, invariants, ownership, dan failure modes dijelaskan | Ya |
| Security/reliability dibahas | Ya |
| Temuan/deviasi (mis. `milestone=M3` pada banner panic) dicatat secara jujur | Ya |
| Readiness review tidak berlebihan | Ya |
| Nama file screenshot masih placeholder — perlu disesuaikan manual | **Perlu tindakan Anda** |
| Referensi memakai format IEEE | Ya |
| Laporan disimpan sebagai Markdown | Ya |

---

## 26. Pernyataan Pengumpulan

Saya mengumpulkan laporan ini bersama artefak pendukung pada commit:

```
3cf1196 (m7-vmm-wip) m7: add page-fault path evidence (demo fault enabled via -DMCSOS_M7_DEMO_PAGEFAULT)
```

Status akhir yang diklaim:

```
Siap uji QEMU tahap M7 (dengan catatan: perbarui MCSOS_MILESTONE ke M7 pada version.h sebelum submission final)
```

Ringkasan satu paragraf:

```text
Praktikum M7 berhasil mengimplementasikan Virtual Memory Manager x86_64 4-level page table
(PML4 -> PDPT -> PD -> PT) lengkap dengan API vmm_space_init, vmm_map_page, vmm_query_page,
vmm_unmap_page, serta wrapper arsitektur invlpg/CR3/CR2. Table walker diverifikasi lewat unit
test host-mode (M7 VMM host tests PASS) dan static check biner (nm -u bersih, objdump
membuktikan invlpg dan akses %cr3 ter-emit). Integrasi ke kmain() lewat kernel_vmm_init()
berhasil melakukan demo map/query/unmap satu halaman di atas QEMU sungguhan (root_paddr=0x1000,
demo map paddr=0x2000). Penanganan #PF pada idt.c terbukti bekerja lewat uji failure-injection
terkendali (-DMCSOS_M7_DEMO_PAGEFAULT): CR2, error_code, RIP, RSP, dan klasifikasi bit
P/W-R/U-S/RSVD/I-D tercetak benar, diikuti KERNEL_PANIC terkendali (bukan hang/triple fault).
Seluruh evidence (build log, serial log normal dan page-fault, readelf/objdump/nm, sesi GDB,
commit Git pada branch m7-vmm-wip) tersedia. Satu temuan kosmetik dicatat: banner panic masih
menampilkan milestone=M3 dan perlu diperbarui sebelum submission final. Status: siap uji QEMU
tahap M7 dengan catatan tersebut.
```
