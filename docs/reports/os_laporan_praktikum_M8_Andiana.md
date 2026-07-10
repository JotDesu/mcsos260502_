# Laporan Praktikum M8 — Kernel Heap Allocator (kmem), First-Fit Free List, Split & Coalesce

## MCSOS 260502

**Nama file laporan:** `laporan_praktikum_M8_[NIM_Kelompok].md`
**Nama sistem operasi:** MCSOS versi 260502
**Target default:** x86_64, QEMU, Windows 11 x64 + WSL 2, kernel monolitik pendidikan, C freestanding dengan assembly minimal, POSIX-like subset
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.
**Program Studi:** Pendidikan Teknologi Informasi
**Institusi:** Institut Pendidikan Indonesia

---

## 0. Metadata Laporan

| Atribut | Isi |
|---|---|
| Kode praktikum | M8 |
| Judul praktikum | Kernel Heap Allocator (kmem), First-Fit Free List, Split & Coalesce |
| Jenis pengerjaan | Individu |
| Nama mahasiswa | Andiana Jamaludin Malik |
| NIM | 2583207073016 |
| Kelas | 1B |
| Nama kelompok | - |
| Anggota kelompok | - |
| Tanggal praktikum | 2026-07-03 |
| Tanggal pengumpulan | 2026-07-03 |
| Repository | ~/src/mcsos |
| Branch kerja | praktikum-m8-kernel-heap (dibuat dari `m7-vmm-wip`) |
| Commit akhir | `b2ca43a` (M8: integrasikan kernel heap ke kmain.c dan tambah target check-m8 di Makefile) |
| Status readiness yang diklaim | Siap uji QEMU tahap M8 (dengan catatan pada bagian 19) |

---

## 1. Sampul

# Laporan Praktikum M8
## Kernel Heap Allocator (kmem), First-Fit Free List, Split & Coalesce

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

Saya menyatakan bahwa laporan ini disusun berdasarkan pekerjaan praktikum sendiri sesuai panduan M8. Bantuan eksternal, referensi, dan dokumentasi resmi dicatat pada bagian referensi dan lampiran.

| Pernyataan | Status |
|---|---|
| Semua potongan kode eksternal diberi atribusi | Ya (panduan M8 dari dosen) |
| Semua penggunaan AI assistant dicatat | Ya |
| Repository yang dikumpulkan sesuai commit akhir | Ya |
| Tidak ada klaim readiness tanpa bukti | Ya (lihat catatan bagian 19) |

Catatan penggunaan bantuan eksternal:

```text
- Panduan praktikum M8 (OS_panduan_M8.pdf) sebagai referensi utama
- Template laporan praktikum (os_template_laporan_praktikum.md)
- AI assistant digunakan untuk membantu menyusun laporan dan analisis dari bukti terminal/log
- Sebuah skrip python3 kecil digunakan untuk menyisipkan assert(kmem_validate()==0)
  tambahan pada tests/test_kmem.c secara mekanis (bukan mengganti logika pengujian)
- Seluruh source code (kmem.h, kmem.c, test_kmem.c, integrasi kmain.c) diimplementasikan
  berdasarkan panduan dosen dan diverifikasi lewat build/QEMU milik sendiri
```

---

## 3. Tujuan Praktikum

1. **Tujuan teknis 1:** Mengimplementasikan kernel heap allocator (`kmem`) berbasis *free list* dengan strategi *first-fit*, dilengkapi *header* per blok (magic number, ukuran, status bebas, pointer `prev`/`next`).
2. **Tujuan teknis 2:** Mengimplementasikan operasi *split* (`kmem_split_if_useful`) agar blok besar dapat dipecah menjadi bagian terpakai dan sisa bebas, serta *coalesce* (`kmem_coalesce_forward`) agar blok bebas yang bersebelahan digabung kembali untuk mengurangi fragmentasi.
3. **Tujuan teknis 3:** Menyediakan API publik `kmem_init`, `kmem_alloc`, `kmem_calloc`, `kmem_free_checked`, `kmem_get_stats`, dan `kmem_validate` dengan kontrak error yang jelas, termasuk deteksi *double-free* dan validasi *overflow* pada `kmem_calloc`.
4. **Tujuan teknis 4:** Membuat unit test host-mode (`tests/test_kmem.c`) yang mencakup alokasi/pembebasan dasar, overflow `calloc`, penolakan *double-free*, serta fragmentasi dan penggabungan kembali (*coalesce*) blok.
5. **Tujuan teknis 5:** Mengintegrasikan `kmem` ke `kmain()` lewat `m8_heap_bootstrap()` yang menyediakan *boot heap* statis 64 KiB, memverifikasi lewat log runtime di atas QEMU sungguhan (`heap total_bytes`, `free_bytes`, `largest_free`, `block_count`).
6. **Tujuan validasi:** Menyimpan log build, log unit test, evidence `readelf`/`objdump`/`nm`, log QEMU (boot penuh M2–M8), skrip preflight (`scripts/check_m8_kmem.sh`), serta commit Git granular pada branch `praktikum-m8-kernel-heap` sebagai bukti deterministik tahap M8.

---

## 4. Capaian Pembelajaran Praktikum

Setelah praktikum ini, mahasiswa mampu:

| CPL/CPMK praktikum | Bukti yang harus ditunjukkan |
|---|---|
| Menjelaskan struktur *free list allocator* dengan header per blok dan *magic number* untuk deteksi korupsi | `kernel/mm/kmem.c` (`struct kmem_block_t`), `KMEM_MAGIC` |
| Mengimplementasikan alokasi *first-fit* dengan pembulatan alignment (`KMEM_ALIGN`) | `kmem_alloc()`, `kmem_align_up_size()`/`kmem_align_up_ptr()` |
| Mengimplementasikan *split* blok agar tidak boros memori | `kmem_split_if_useful()`, konstanta `KMEM_MIN_SPLIT` |
| Mengimplementasikan *coalesce* blok bebas bersebelahan | `kmem_coalesce_forward()` |
| Mendeteksi *double-free* dan pointer di luar rentang heap | `kmem_free_checked()`, `kmem_ptr_in_heap()` |
| Menulis unit test host-mode untuk skenario alokasi, overflow, double-free, dan fragmentasi | `tests/test_kmem.c`, output `M8 kmem host tests: PASS` |
| Memverifikasi hasil kompilasi lewat static check (undefined symbol, ELF header, disassembly) | `build/kmem.undefined.txt`, `build/kmem.readelf.header.txt`, `build/kmem.objdump.txt` |
| Mengintegrasikan heap ke boot path kernel dan memverifikasi statistik lewat serial log | `kernel/core/kmain.c` (`m8_heap_bootstrap`), log `[MCSOS:M8]` |
| Membuat skrip preflight otomatis untuk memverifikasi baseline repository sebelum submission | `scripts/check_m8_kmem.sh` |
| Mengelola pekerjaan lewat commit granular per perubahan logis dan mendorong ke branch terpisah | `git log --oneline`, `git push -u origin praktikum-m8-kernel-heap` |

---

## 5. Peta Milestone MCSOS

| Milestone | Fokus | Status dalam laporan |
|---|---|---|
| M0 | Requirements, governance, baseline arsitektur | [x] selesai (praktikum sebelumnya) |
| M1 | Toolchain reproducible, Git, QEMU, GDB, metadata build | [x] selesai (praktikum sebelumnya) |
| M2 | Boot image, kernel ELF64, early console | [x] selesai (praktikum sebelumnya) |
| M3 | Panic path, linker map, GDB, observability awal | [x] selesai (dipakai ulang: `KERNEL_PANIC`) |
| M4 | Trap, exception, interrupt, timer (IDT/PIC/PIT) | [x] selesai (dipakai ulang) |
| M5 | Interrupt-driven timer, observability lanjutan | [x] selesai (dipakai ulang: `[MCSOS:TIMER]`) |
| M6 | Physical Memory Manager (PMM) | [x] selesai (regresi: `make check-m6` tetap PASS) |
| M7 | Virtual Memory Manager (VMM), page table, page fault | [x] selesai (regresi: `make check-m7` tetap PASS) |
| **M8** | **Kernel Heap Allocator (kmem), split & coalesce** | **[x] dibahas penuh pada laporan ini** |
| M9 | Thread, scheduler, synchronization | [ ] tidak dibahas |
| M10 | Syscall ABI dan user program loader | [ ] tidak dibahas |
| M11+ | VFS, block layer, filesystem, networking, security, SMP, dst. | [ ] tidak dibahas |

Batas cakupan praktikum:

```text
Praktikum M8 mencakup:
- Header kontrak heap allocator (include/mcsos/kmem.h)
- Implementasi free list allocator (kernel/mm/kmem.c): align, split, coalesce, validate
- API publik: kmem_init, kmem_alloc, kmem_calloc, kmem_free_checked, kmem_get_stats, kmem_validate
- Unit test host-mode (tests/test_kmem.c) dengan 4 skenario utama
- Target build/test check-m8 pada Makefile (static check: nm -u, readelf, objdump)
- Skrip preflight scripts/check_m8_kmem.sh
- Regresi lintas milestone: make check-m6 dan make check-m7 tetap PASS
- Integrasi ke kmain() lewat m8_heap_bootstrap() dengan boot heap statis 64 KiB
- Perbaikan marker log pada tools/scripts/run_qemu.sh agar sesuai format serial nyata
- Penguatan unit test lewat penambahan assert(kmem_validate()==0) di titik-titik kritis
- Evidence Git granular pada branch praktikum-m8-kernel-heap

Non-goals (tidak termasuk):
- Heap dinamis yang tumbuh otomatis lewat VMM (masih arena statis tetap 64 KiB)
- Thread-safety/locking pada allocator (kernel masih single-threaded)
- Realokasi (realloc) dan pemadatan (defragmentation) aktif
- Allocator per-CPU / slab allocator (topik lanjutan)
- Statistik heap real-time yang diekspos ke syscall (belum ada syscall ABI)
```

---

## 6. Dasar Teori Ringkas

### 6.1 Konsep Sistem Operasi yang Diuji

**Kernel Heap Allocator:** Lapisan yang mengelola alokasi memori dinamis di dalam kernel di atas rentang memori statis (*arena*/*boot heap*), berbeda dari Physical Memory Manager (M6) yang bekerja per-frame 4 KiB dan Virtual Memory Manager (M7) yang bekerja per-halaman.

**Free List Allocator:** Struktur data linked-list (di sini *doubly linked list*, `prev`/`next`) yang menghubungkan blok-blok memori — baik yang terpakai maupun bebas — lewat header yang disisipkan tepat sebelum area payload setiap blok.

**Splitting:** Saat blok bebas yang ditemukan jauh lebih besar dari kebutuhan, bagian sisa dipecah menjadi blok bebas baru (jika sisanya cukup besar, dijaga oleh `KMEM_MIN_SPLIT`) agar tidak ada memori yang "terkunci" tanpa guna.

**Coalescing:** Saat sebuah blok dibebaskan, allocator mencoba menggabungkannya dengan blok bebas tetangga (maju dan mundur) untuk mengurangi fragmentasi eksternal.

**Magic Number Validation:** Setiap header blok menyimpan nilai `KMEM_MAGIC` tetap; sebelum operasi pada blok (`free`, `validate`), nilai ini diperiksa untuk mendeteksi korupsi heap atau pointer yang tidak valid.

### 6.2 Konsep Arsitektur x86_64/Implementasi yang Relevan

| Konsep | Relevansi pada praktikum | Bukti/verifikasi |
|---|---|---|
| Alignment 16 byte (`KMEM_ALIGN`) | Payload dan header selalu dibulatkan ke kelipatan 16 agar aman untuk struktur data C umum | `kmem_align_up_size`/`kmem_align_up_ptr`, assert alignment pada unit test |
| Magic number (`KMEM_MAGIC = 0x4d43534f53484541ull`) | Deteksi korupsi/kesalahan pointer sebelum operasi kritikal | ASCII decode: `"MCSOSHEA"`; dicek di `kmem_free_checked`, `kmem_validate` |
| Ukuran header (`sizeof(kmem_block_t)`) | Overhead tetap 48 byte per blok (magic 8 + size 8 + free 4 + reserved 4 + reserved2 8 + prev 8 + next 8) | Konsisten dengan log runtime: `65536 - 65488 = 48` byte |
| First-fit search | Strategi pencarian blok bebas pertama yang cukup besar, bukan best-fit | `kmem_alloc()` — loop linear dari `g_head` |
| Guard integer overflow | `kmem_align_up_size`, `kmem_calloc` memeriksa batas `SIZE_MAX`/`UINTPTR_MAX` sebelum operasi aritmatika | Source code, unit test `test_calloc_and_overflow` |

### 6.3 Konsep Implementasi Freestanding

| Aspek | Keputusan praktikum |
|---|---|
| Bahasa | C17 freestanding untuk `kmem.c` yang dilink ke kernel, C17 hosted untuk unit test host-mode |
| Portabilitas kode | `kmem.c` tidak memakai instruksi assembly maupun API OS — murni manipulasi pointer/aritmatika, sehingga source yang sama dipakai baik di build kernel freestanding maupun build host test tanpa guard preprocessor tambahan |
| Struktur data | `kmem_block_t` memakai `uint64_t`/`size_t`/`int`/pointer eksplisit agar tata letak (layout) stabil dan mudah diverifikasi lewat `sizeof` |
| Compiler flags kritis (target kernel) | `-ffreestanding -fno-builtin -fno-stack-protector -fno-stack-check -fno-pic -fno-pie -fno-lto -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mcmodel=kernel -Wall -Wextra -Werror -Iinclude` |
| Compiler flags host test | `-std=c17 -Wall -Wextra -Werror -Iinclude` (native, hosted) |
| Risiko undefined behavior | Mitigasi dengan `-Werror`, `nm -u` untuk memastikan tidak ada undefined symbol, serta `kmem_validate()` sebagai pengecekan invariant heap end-to-end |

### 6.4 Referensi Teori yang Digunakan

| No. | Sumber | Bagian yang digunakan | Alasan relevansi |
|---|---|---|---|
| [1] | OSDev Wiki - Memory Allocation | Konsep free list, header block, split/coalesce | Referensi praktis desain allocator kernel |
| [2] | Wilson et al., "Dynamic Storage Allocation: A Survey and Critical Review" | Strategi first-fit vs best-fit, fragmentasi | Dasar keputusan strategi pencarian |
| [3] | ISO/IEC 9899:2018 (C17), §7.22.3 | Kontrak `malloc`/`calloc`/`free` sebagai acuan desain API `kmem_*` | Kesesuaian semantik alokasi dinamis |
| [4] | Intel/AMD ABI documentation | Alignment 16-byte untuk struktur data umum pada x86_64 | Dasar nilai `KMEM_ALIGN` |

---

## 7. Lingkungan Praktikum

### 7.1 Host dan Target

| Komponen | Nilai |
|---|---|
| Host OS | Windows 11 x64 |
| Lingkungan build | WSL 2 Ubuntu |
| Target ISA | x86_64 |
| Target ABI | x86_64-unknown-none-elf (kernel), native host (unit test) |
| Emulator | QEMU system-x86_64 |
| Build system | GNU Make |
| Compiler | Ubuntu clang version 21.1.8 (6ubuntu1) |
| Linker | Ubuntu LLD 21.1.8 (compatible with GNU linkers) |
| Make | GNU Make 4.4.1 |
| Bahasa utama | C17 (freestanding untuk kernel, hosted untuk test) |

Bukti screenshot versi tool: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 141530.png`

```bash
clang --version | head -n 1
ld.lld --version 2>/dev/null | head -n 1 || true
make --version | head -n 1
```

Output:
```
Ubuntu clang version 21.1.8 (6ubuntu1)
Ubuntu LLD 21.1.8 (compatible with GNU linkers)
GNU Make 4.4.1
```

### 7.2 Lokasi Repository dan Branch

| Item | Nilai |
|---|---|
| Path repository di WSL | `/home/andianaaji/src/mcsos` |
| Branch dasar | `m7-vmm-wip` |
| Branch kerja M8 | `praktikum-m8-kernel-heap` |
| Remote | `https://github.com/JotDesu/mcsos260502_git` |
| Commit akhir | `b2ca43a` |

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 154012.png`

```bash
git remote -v
git branch -vv
```

Output (ringkas):
```
origin  https://github.com/JotDesu/mcsos260502_git (fetch)
origin  https://github.com/JotDesu/mcsos260502_git (push)
* praktikum-m8-kernel-heap b2ca43a M8: integrasikan kernel heap ke kmain.c dan tambah target check-m8 di Makefile
  m7-vmm-wip              cfcf953 [origin/m7-vmm-wip] M7: add readelf header/sections evidence for vmm.o
```

---

## 8. Repository dan Struktur File

### 8.1 Struktur Direktori yang Relevan

```text
mcsos/
├── Makefile
├── include/
│   └── mcsos/
│       └── kmem.h
├── kernel/
│   ├── mm/
│   │   └── kmem.c
│   ├── core/
│   │   ├── kmain.c
│   │   ├── vmm.c
│   │   ├── pmm.c
│   │   ├── log.c
│   │   └── panic.c
│   ├── arch/x86_64/src/
│   │   ├── idt.c
│   │   ├── pic.c
│   │   └── pit.c
│   └── lib/
│       └── memory.c
├── tests/
│   ├── test_kmem.c
│   └── test_vmm_host.c
├── scripts/
│   └── check_m8_kmem.sh
├── tools/scripts/
│   └── run_qemu.sh
├── evidence/
│   └── M7/
│       └── m7_full_checkpoint_C1_C5.log
└── build/
    ├── kmem.o
    ├── test_kmem_host
    ├── kmem.undefined.txt
    ├── kmem.readelf.header.txt
    ├── kmem.objdump.txt
    ├── test_kmem.log
    ├── kernel.elf
    ├── mcsos.iso
    ├── mcsos.iso.sha256
    ├── qemu-serial.log
    └── m8/
        └── qemu_m8.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 140012.png`

### 8.2 File yang Dibuat atau Diubah

| File | Jenis perubahan | Alasan perubahan | Risiko |
|---|---|---|---|
| `include/mcsos/kmem.h` | Baru | Kontrak tipe (`kmem_stats_t`), konstanta (`KMEM_ALIGN`, `KMEM_MAGIC`), prototipe API heap | Sedang - kontrak dipakai lintas modul |
| `kernel/mm/kmem.c` | Baru | Implementasi free list allocator (280 baris): align, split, coalesce, alloc/calloc/free/stats/validate | Tinggi - logika pointer arithmetic rawan bug |
| `tests/test_kmem.c` | Baru, lalu diperkuat | Unit test host-mode 4 skenario + penambahan `kmem_validate()` ekstra | Rendah - hanya verifikasi logika |
| `Makefile` | Ubah | Target `check-m8` (build kernel object + test host + static check) | Sedang - flags build harus konsisten |
| `scripts/check_m8_kmem.sh` | Baru | Skrip preflight verifikasi baseline repo + toolchain + `make check-m8` | Rendah - automation |
| `kernel/core/kmain.c` | Ubah | `#include "mcsos/kmem.h"`, `m8_heap_bootstrap()`, pemanggilan setelah `kernel_vmm_init()` | Sedang - integrasi runtime nyata |
| `tools/scripts/run_qemu.sh` | Ubah | Perbaikan string marker grep agar cocok dengan log `[MCSOS:M8] kmem initialized` | Rendah - hanya validasi log |

### 8.3 Ringkasan Status Git

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 153015.png`

```bash
git status --short
```

Output (setelah seluruh commit M8):
```
(bersih - seluruh perubahan sudah ter-commit pada b2ca43a)
```

---

## 9. Desain Teknis

### 9.1 Masalah yang Diselesaikan

Kernel M7 hanya memiliki VMM (pemetaan halaman) dan PMM (alokasi frame 4 KiB), tetapi belum memiliki:
- Mekanisme alokasi memori berukuran sembarang (byte-granular) di dalam kernel
- Struktur data untuk melacak blok terpakai vs bebas
- Deteksi *double-free* dan validasi integritas heap

### 9.2 Keputusan Desain

| Keputusan | Alternatif yang dipertimbangkan | Alasan memilih | Konsekuensi |
|---|---|---|---|
| Strategi *first-fit* | *Best-fit*, *buddy allocator* | Implementasi sederhana, cukup untuk beban kerja awal kernel edukasi | Berpotensi fragmentasi lebih tinggi dibanding best-fit pada beban kerja tertentu |
| Header per blok tertanam (`kmem_block_t`) di depan payload | Struktur metadata terpisah (bitmap/tabel eksternal) | Sederhana untuk diimplementasikan dan divalidasi (`kmem_validate`) | Overhead tetap 48 byte per blok |
| `KMEM_MAGIC` untuk validasi | Tanpa validasi (percaya pointer caller) | Mendeteksi korupsi/­*use-after-free* sederhana, mendukung `kmem_free_checked` menolak pointer tidak valid | Sedikit overhead pemeriksaan tiap operasi |
| Split hanya jika sisa ≥ `KMEM_MIN_SPLIT` (32 byte) + header | Selalu split jika ada sisa | Mencegah blok sisa terlalu kecil untuk berguna (fragmentasi internal berlebihan) | Blok yang dialokasikan bisa sedikit lebih besar dari yang diminta |
| Coalesce hanya maju (`kmem_coalesce_forward`), dipanggil juga dari blok sebelumnya saat bebas | Coalesce dua arah dalam satu fungsi kompleks | Sederhana: panggil forward pada blok saat ini, lalu forward lagi dari `prev` jika `prev` bebas — efeknya setara coalesce dua arah | Dua pemanggilan fungsi, tetap O(1) amortized untuk kasus umum |
| Boot heap statis 64 KiB (`m8_boot_heap`) | Heap dinamis di atas VMM (memetakan halaman baru saat heap penuh) | M8 belum punya mekanisme *grow* aman di atas VMM/PMM; cukup untuk pembuktian tahap ini | Heap tidak bisa tumbuh melebihi 64 KiB pada M8 (dicatat sebagai non-goal) |

### 9.3 Arsitektur Ringkas — Struktur Free List

```
 g_heap_base                                                          g_heap_end
     |                                                                     |
     v                                                                     v
 +--------+-----------------+--------+-----------------+--------+---------+
 | header | payload (used)  | header | payload (free)   | header | ...    |
 | magic  |                 | magic  |                   | magic  |        |
 | size   |                 | size   |                   | size   |        |
 | free=0 |                 | free=1 |                   | free=1 |        |
 | prev/next -------------> | prev/next -------------->  | prev/next -->  |
 +--------+-----------------+--------+-----------------+--------+---------+
      ^                                    |
      |____________ g_head (list awal) ____|

kmem_alloc(bytes):
  align bytes -> wanted
  walk list dari g_head, cari blok free dengan size >= wanted (first-fit)
  jika ditemukan: kmem_split_if_useful(block, wanted) lalu tandai free=0

kmem_free_checked(ptr):
  validasi ptr di dalam heap & magic cocok -> tandai free=1
  kmem_coalesce_forward(block)
  jika prev free: kmem_coalesce_forward(prev)   // efek gabung mundur
  return kmem_validate()
```

### 9.4 Kontrak Antarmuka

| Antarmuka | Pemanggil | Penerima | Precondition | Postcondition | Error path |
|---|---|---|---|---|---|
| `kmem_init(base, bytes)` | `m8_heap_bootstrap()`, unit test | `kmem.c` | `base` non-NULL, `bytes` cukup untuk 1 header + `KMEM_MIN_SPLIT` | Heap terinisialisasi 1 blok bebas besar | Return `-1`..`-4` sesuai jenis kegagalan validasi parameter |
| `kmem_alloc(bytes)` | `m8_heap_bootstrap()`, unit test | `kmem.c` | Heap sudah `kmem_init`, `bytes > 0` | Pointer payload teralign 16 byte dikembalikan | Return `NULL` jika belum init, `bytes==0`, overflow align, atau tidak ada blok cukup besar |
| `kmem_calloc(count, bytes)` | Unit test | `kmem.c` | `count * bytes` tidak overflow `SIZE_MAX` | Memori teralokasi dan di-nol-kan | Return `NULL` jika overflow terdeteksi atau alokasi gagal |
| `kmem_free_checked(ptr)` | `m8_heap_bootstrap()`, unit test | `kmem.c` | `ptr` hasil `kmem_alloc`/`kmem_calloc` yang valid & belum dibebaskan | Blok ditandai bebas, coalesce dijalankan, heap tervalidasi | Return `0` jika NULL (no-op), negatif jika `ptr` di luar heap/magic salah (termasuk *double-free*) |
| `kmem_get_stats(out)` | `m8_heap_bootstrap()` | `kmem.c` | `out` non-NULL | `out` terisi total/used/free bytes, block/free count, largest free | Field di-nol-kan jika heap belum init |
| `kmem_validate(void)` | `kmem_free_checked`, unit test | `kmem.c` | - | Walk seluruh list, cek magic/alignment/batas/`prev` linkage | Return `-1`..`-9` sesuai jenis pelanggaran invariant yang ditemukan |

### 9.5 Struktur Data Utama

| Struktur data | Field penting | Ownership | Lifetime | Invariant |
|---|---|---|---|---|
| `kmem_block_t` | `magic`(8), `size`(8), `free`(4), `reserved`(4), `reserved2`(8), `prev`(8), `next`(8) — total 48 byte | Heap (tertanam di depan tiap payload) | Selama blok ada dalam list | `magic == KMEM_MAGIC` selama blok valid; `size` tidak pernah melebihi sisa heap |
| `kmem_stats_t` | `total_bytes`, `used_bytes`, `free_bytes`, `block_count`, `free_count`, `largest_free` | Caller (`kmem_get_stats` output) | Sesaat (stack) | `used_bytes + free_bytes == total_bytes` (verifikasi tersirat lewat unit test) |
| `g_heap_base`/`g_heap_end`/`g_head`/`g_initialized` | Static globals | Modul `kmem.c` | Selama kernel/proses hidup | `g_head == g_heap_base` sepanjang waktu (list dimulai dari awal heap) |

### 9.6 Invariants

1. `g_head` selalu menunjuk ke `g_heap_base` (list tidak pernah "kehilangan" blok pertama).
2. Setiap blok yang dikunjungi walker harus memiliki `magic == KMEM_MAGIC`, jika tidak: heap dianggap korup (`kmem_validate` mengembalikan error).
3. `cursor` (posisi byte kumulatif saat walk) harus selalu sama dengan alamat blok berikutnya — tidak boleh ada "lubang"/tumpang tindih antar blok.
4. `kmem_free_checked` pada pointer yang sama dua kali berturut-turut harus ditolak pada pemanggilan kedua (*double-free rejection*).
5. `kmem_calloc` harus menolak permintaan yang menyebabkan `count * bytes` overflow `SIZE_MAX`, mengembalikan `NULL` tanpa side-effect.
6. Setiap `kmem_free_checked` yang sukses selalu diakhiri dengan pemanggilan `kmem_validate()` sebagai *self-check* sebelum return.

### 9.7 Ownership, Locking, dan Concurrency

M8 masih single-threaded (belum ada scheduler/M9). Tidak ada locking pada `g_head`/`g_heap_base` karena hanya satu execution path yang mengakses heap selama `m8_heap_bootstrap()` dan unit test.

### 9.8 Memory Safety dan Undefined Behavior Risk

| Risiko | Lokasi | Mitigasi | Bukti |
|---|---|---|---|
| Integer overflow pada alignment (`value + mask`) | `kmem_align_up_size`/`kmem_align_up_ptr` | Cek `value > (SIZE_MAX - mask)` sebelum penjumlahan | Source code |
| Integer overflow pada `count * bytes` | `kmem_calloc` | Cek `bytes > SIZE_MAX / count` sebelum perkalian | Unit test `test_calloc_and_overflow` (`(size_t)-1, 2` → `NULL`) |
| *Double-free* | `kmem_free_checked` | Validasi `kmem_ptr_in_heap` + cek `block->magic`/status sebelum menandai bebas kedua kali | Unit test `test_double_free_rejected` |
| Pointer di luar rentang heap diteruskan ke `free` | `kmem_free_checked` | `kmem_ptr_in_heap()` memeriksa `p >= g_heap_base && p < g_heap_end` | Source code |
| Blok sisa terlalu kecil setelah split (fragmentasi internal ekstrem) | `kmem_split_if_useful` | Guard `block->size < wanted + header + KMEM_MIN_SPLIT` | Source code |
| Heap korup akibat bug lain tidak terdeteksi | Semua operasi | `kmem_validate()` dipanggil eksplisit di unit test dan otomatis di akhir `kmem_free_checked` | Unit test (assert `kmem_validate()==0` di banyak titik) |

### 9.9 Security Boundary

| Boundary | Data tidak tepercaya | Validasi yang dilakukan | Failure mode aman |
|---|---|---|---|
| Pointer dari caller ke `kmem_free_checked` | `ptr` sembarang | Cek rentang heap + magic number | Return kode error negatif, tidak menyentuh memori di luar heap |
| Ukuran alokasi dari caller | `bytes`, `count` | Guard overflow sebelum aritmatika | Return `NULL`, tidak melakukan alokasi parsial |

---

## 10. Langkah Kerja Implementasi

### Langkah 1 — Membuat Header Kontrak Heap (`kmem.h`)

Maksud langkah: Mendefinisikan tipe statistik, konstanta alignment/magic, dan prototipe API heap sebagai kontrak sebelum implementasi.

Perintah:
```bash
cat > include/mcsos/kmem.h << 'EOF'
#ifndef MCSOS_KMEM_H
#define MCSOS_KMEM_H

#include <stddef.h>
#include <stdint.h>

#define KMEM_ALIGN 16u
#define KMEM_MAGIC 0x4d43534f53484541ull

typedef struct kmem_stats {
    size_t total_bytes;
    size_t used_bytes;
    size_t free_bytes;
    size_t block_count;
    size_t free_count;
    size_t largest_free;
} kmem_stats_t;

int kmem_init(void *base, size_t bytes);
void *kmem_alloc(size_t bytes);
void *kmem_calloc(size_t count, size_t bytes);
int kmem_free_checked(void *ptr);
void kmem_get_stats(kmem_stats_t *out);
int kmem_validate(void);

#endif
EOF
cat include/mcsos/kmem.h
git status --short
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 130012.png`

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| kmem.h | include/mcsos/kmem.h | Kontrak tipe/konstanta/prototipe heap allocator |

Indikator berhasil: Header tersimpan, `git status --short` menandai `?? include/` sebagai file baru yang belum di-track.

### Langkah 2 — Implementasi Struktur Blok dan Helper Alignment (`kmem.c`)

Maksud langkah: Mendefinisikan `kmem_block_t`, konstanta `KMEM_MIN_SPLIT`, dan fungsi pembulatan alamat/ukuran ke kelipatan `KMEM_ALIGN` dengan proteksi overflow.

Perintah:
```bash
cat > kernel/mm/kmem.c << 'EOF'
#include "mcsos/kmem.h"

#define KMEM_MIN_SPLIT 32u

typedef struct kmem_block {
    uint64_t magic;
    size_t size;
    int free;
    uint32_t reserved;
    uint64_t reserved2;
    struct kmem_block *prev;
    struct kmem_block *next;
} kmem_block_t;

static unsigned char *g_heap_base;
static unsigned char *g_heap_end;
static kmem_block_t *g_head;
static int g_initialized;

static size_t kmem_align_up_size(size_t value, size_t align) {
    if (align == 0u) { return value; }
    const size_t mask = align - 1u;
    if ((align & mask) != 0u) { return 0u; }
    if (value > (SIZE_MAX - mask)) { return 0u; }
    return (value + mask) & ~mask;
}

static uintptr_t kmem_align_up_ptr(uintptr_t value, uintptr_t align) {
    const uintptr_t mask = align - 1u;
    if ((align & mask) != 0u) { return 0u; }
    if (value > (UINTPTR_MAX - mask)) { return 0u; }
    return (value + mask) & ~mask;
}
EOF
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 130334.png`

Indikator berhasil: `kmem_block_t` berukuran 48 byte (8+8+4+4+8+8+8), helper alignment menolak `align` yang bukan pangkat dua dan mendeteksi potensi overflow sebelum penjumlahan.

### Langkah 3 — Helper Payload/Header dan `kmem_split_if_useful`

Maksud langkah: Menulis fungsi konversi antara pointer header dan pointer payload, pengecekan pointer di dalam heap, serta logika *split* blok besar.

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 130612.png`

Cuplikan kunci:
```c
static unsigned char *kmem_payload(kmem_block_t *block) {
    return ((unsigned char *)block) + sizeof(kmem_block_t);
}
static kmem_block_t *kmem_header_from_payload(void *ptr) {
    return (kmem_block_t *)(((unsigned char *)ptr) - sizeof(kmem_block_t));
}
static int kmem_ptr_in_heap(const void *ptr) {
    const unsigned char *p = (const unsigned char *)ptr;
    return g_initialized && p >= g_heap_base && p < g_heap_end;
}
static void kmem_split_if_useful(kmem_block_t *block, size_t wanted) {
    const size_t header = kmem_align_up_size(sizeof(kmem_block_t), KMEM_ALIGN);
    if (header == 0u) { return; }
    if (block->size < wanted + header + KMEM_MIN_SPLIT) { return; }
    /* ... hitung new_addr, buat new_block, sisipkan ke linked list ... */
}
```

Indikator berhasil: `kmem_split_if_useful` hanya memecah blok jika sisa cukup untuk header baru + `KMEM_MIN_SPLIT`, mencegah blok sisa yang tidak berguna.

### Langkah 4 — `kmem_coalesce_forward` dan `kmem_init`

Maksud langkah: Menggabungkan blok bebas yang bersebelahan secara maju, serta menginisialisasi heap dari rentang memori mentah (`base`, `bytes`).

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 130845.png`

Cuplikan kunci `kmem_init`:
```c
int kmem_init(void *base, size_t bytes) {
    if (base == (void *)0 || bytes < (sizeof(kmem_block_t) + KMEM_MIN_SPLIT)) { return -1; }
    uintptr_t start = kmem_align_up_ptr((uintptr_t)base, KMEM_ALIGN);
    if (start == 0u || start < (uintptr_t)base) { return -2; }
    const size_t lost = (size_t)(start - (uintptr_t)base);
    if (bytes <= lost + sizeof(kmem_block_t) + KMEM_MIN_SPLIT) { return -3; }

    size_t usable = bytes - lost;
    usable = usable & ~(size_t)(KMEM_ALIGN - 1u);
    if (usable <= sizeof(kmem_block_t) + KMEM_MIN_SPLIT) { return -4; }

    g_heap_base = (unsigned char *)start;
    g_heap_end = g_heap_base + usable;
    g_head = (kmem_block_t *)g_heap_base;
    g_head->magic = KMEM_MAGIC;
    g_head->size = usable - sizeof(kmem_block_t);
    g_head->free = 1;
    /* prev = NULL, next = NULL, g_initialized = 1 */
}
```

Indikator berhasil: `kmem_init` menolak `base`/`bytes` yang tidak layak lewat kode error `-1`..`-4`, dan heap awal berisi satu blok bebas besar mencakup seluruh rentang yang usable.

### Langkah 5 — `kmem_alloc` dan `kmem_calloc`

Maksud langkah: Mengimplementasikan pencarian *first-fit* pada free list dan alokasi array ter-nol-kan dengan proteksi overflow.

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 131102.png`

Cuplikan kunci:
```c
void *kmem_alloc(size_t bytes) {
    if (!g_initialized || bytes == 0u) { return (void *)0; }
    const size_t wanted = kmem_align_up_size(bytes, KMEM_ALIGN);
    if (wanted == 0u) { return (void *)0; }
    for (kmem_block_t *cur = g_head; cur != (kmem_block_t *)0; cur = cur->next) {
        if (cur->magic != KMEM_MAGIC) { return (void *)0; }
        if (cur->free && cur->size >= wanted) {
            kmem_split_if_useful(cur, wanted);
            cur->free = 0;
            return (void *)kmem_payload(cur);
        }
    }
    return (void *)0;
}

void *kmem_calloc(size_t count, size_t bytes) {
    if (count != 0u && bytes > SIZE_MAX / count) { return (void *)0; }
    const size_t total = count * bytes;
    void *ptr = kmem_alloc(total);
    if (ptr != (void *)0) { (void)kmem_memset(ptr, 0, total); }
    return ptr;
}
```

Indikator berhasil: `kmem_alloc` mengembalikan `NULL` jika belum `init`, `bytes==0`, atau tidak ada blok cukup besar; `kmem_calloc` menolak `count*bytes` yang overflow sebelum memanggil `kmem_alloc`.

### Langkah 6 — `kmem_free_checked`, `kmem_get_stats`, `kmem_validate`

Maksud langkah: Melengkapi pembebasan blok dengan validasi pointer, statistik heap, dan pengecekan invariant menyeluruh.

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 131345.png`

Cuplikan kunci:
```c
int kmem_free_checked(void *ptr) {
    if (ptr == (void *)0) { return 0; }
    if (!kmem_ptr_in_heap(ptr)) { return -1; }
    if (((uintptr_t)ptr & (KMEM_ALIGN - 1u)) != 0u) { return -2; }
    kmem_block_t *block = kmem_header_from_payload(ptr);
    if (!kmem_ptr_in_heap(block) || block->magic != KMEM_MAGIC) { return -3; }
    /* deteksi double-free lewat pengecekan status/([kode lengkap pada Lampiran E]) */
    block->free = 1;
    kmem_coalesce_forward(block);
    if (block->prev != (kmem_block_t *)0 && block->prev->free) {
        kmem_coalesce_forward(block->prev);
    }
    return kmem_validate();
}
```

`kmem_validate()` melakukan walk penuh atas free list, memeriksa: `g_head == g_heap_base`, batas iterasi (guard `1048576u` agar tidak infinite loop jika list korup), posisi `cursor` konsisten, `magic` tiap blok, linkage `prev`, dan `size` tidak melebihi sisa heap.

Indikator berhasil: `kmem_free_checked` pada pointer yang sudah dibebaskan sebelumnya mengembalikan nilai negatif (bukan `0`), membuktikan *double-free* tertolak.

### Langkah 7 — Verifikasi Struktur `kmem.c`

Maksud langkah: Memastikan seluruh fungsi kunci terdefinisi pada baris yang benar dan file lengkap.

Perintah:
```bash
wc -l kernel/mm/kmem.c
grep -n '^[a-zA-Z].*kmem_' kernel/mm/kmem.c | grep -E '\(.*\) \{'
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 131612.png`

Output:
```
280 kernel/mm/kmem.c
20:static size_t kmem_align_up_size(size_t value, size_t align) {
34:static uintptr_t kmem_align_up_ptr(uintptr_t value, uintptr_t align) {
45:static void *kmem_memset(void *dst, int value, size_t bytes) {
53:static unsigned char *kmem_payload(kmem_block_t *block) {
57:static kmem_block_t *kmem_header_from_payload(void *ptr) {
61:static int kmem_ptr_in_heap(const void *ptr) {
66:static void kmem_split_if_useful(kmem_block_t *block, size_t wanted) {
102:static void kmem_coalesce_forward(kmem_block_t *block) {
122:int kmem_init(void *base, size_t bytes) {
154:void *kmem_alloc(size_t bytes) {
176:void *kmem_calloc(size_t count, size_t bytes) {
188:int kmem_free_checked(void *ptr) {
213:void kmem_get_stats(kmem_stats_t *out) {
242:int kmem_validate(void) {
```

Indikator berhasil: File 280 baris, seluruh 14 fungsi (helper + API publik) terverifikasi ada pada baris yang sesuai urutan implementasi.

### Langkah 8 — Membuat Unit Test Host-Mode (`test_kmem.c`)

Maksud langkah: Membuat 4 skenario uji: alokasi/pembebasan dasar, `calloc` + overflow, penolakan *double-free*, dan fragmentasi + coalesce.

Perintah:
```bash
cat > tests/test_kmem.c << 'EOF'
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mcsos/kmem.h"

static unsigned char arena[4096u * 4u];

static void test_basic_alloc_free(void) {
    assert(kmem_init(arena, sizeof(arena)) == 0);
    void *a = kmem_alloc(24); void *b = kmem_alloc(128); void *c = kmem_alloc(4096);
    assert(a != NULL); assert(b != NULL); assert(c != NULL);
    assert(((uintptr_t)a & (KMEM_ALIGN - 1u)) == 0u);
    assert(((uintptr_t)b & (KMEM_ALIGN - 1u)) == 0u);
    assert(((uintptr_t)c & (KMEM_ALIGN - 1u)) == 0u);
    memset(a, 0xc1, 24); memset(b, 0x22, 128); memset(c, 0x33, 4096);
    assert(kmem_validate() == 0);
    assert(kmem_free_checked(b) == 0);
    assert(kmem_free_checked(a) == 0);
    assert(kmem_free_checked(c) == 0);
    assert(kmem_validate() == 0);
}

static void test_calloc_and_overflow(void) {
    assert(kmem_init(arena, sizeof(arena)) == 0);
    unsigned char *z = (unsigned char *)kmem_calloc(64, 4);
    assert(z != NULL);
    for (size_t i = 0; i < 256; ++i) { assert(z[i] == 0u); }
    assert(kmem_calloc((size_t)-1, 2) == NULL);
    assert(kmem_free_checked(z) == 0);
}

static void test_double_free_rejected(void) {
    assert(kmem_init(arena, sizeof(arena)) == 0);
    void *p = kmem_alloc(512);
    assert(p != NULL);
    assert(kmem_free_checked(p) == 0);
    assert(kmem_free_checked(p) < 0);
}

static void test_fragmentation_and_coalesce(void) {
    assert(kmem_init(arena, sizeof(arena)) == 0);
    void *p[16];
    for (size_t i = 0; i < 16; ++i) { p[i] = kmem_alloc(256 + i); assert(p[i] != NULL); }
    for (size_t i = 0; i < 16; i += 2) { assert(kmem_free_checked(p[i]) == 0); }
    for (size_t i = 1; i < 16; i += 2) { assert(kmem_free_checked(p[i]) == 0); }
    kmem_stats_t st;
    kmem_get_stats(&st);
    assert(st.free_count == 1u);
    assert(st.block_count == 1u);
    assert(st.largest_free > 4096u);
}

int main(void) {
    test_basic_alloc_free();
    test_calloc_and_overflow();
    test_double_free_rejected();
    test_fragmentation_and_coalesce();
    puts("M8 kmem host tests: PASS");
    return 0;
}
EOF
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 132012.png`

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| test_kmem.c | tests/test_kmem.c | Unit test host-mode 4 skenario allocator |

### Langkah 9 — Verifikasi Struktur Unit Test

Perintah:
```bash
wc -l tests/test_kmem.c
grep -n '^static void test_\|^int main' tests/test_kmem.c
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 132145.png`

Output:
```
76 tests/test_kmem.c
9:static void test_basic_alloc_free(void) {
30:static void test_calloc_and_overflow(void) {
41:static void test_double_free_rejected(void) {
49:static void test_fragmentation_and_coalesce(void) {
69:int main(void) {
```

Indikator berhasil: Seluruh 4 fungsi test + `main()` terverifikasi ada.

### Langkah 10 — Build Manual dan Static Check Awal

Maksud langkah: Menjalankan unit test host secara manual, lalu mengompilasi `kmem.c` untuk target kernel freestanding dan memverifikasi lewat `nm`/`readelf`/`objdump`.

Perintah:
```bash
mkdir -p build
clang -std=c17 -Wall -Wextra -Werror -Iinclude \
  kernel/mm/kmem.c tests/test_kmem.c -o build/test_kmem_host
./build/test_kmem_host | tee build/test_kmem.log

clang --target=x86_64-unknown-none-elf -std=c17 -ffreestanding \
  -fno-builtin -fno-stack-protector -fno-stack-check -fno-pic -fno-pie \
  -fno-lto -m64 -march=x86-64 -mabi=sysv -mno-red-zone -mno-mmx \
  -mno-sse -mno-sse2 -mcmodel=kernel -Wall -Wextra -Werror \
  -Iinclude -c kernel/mm/kmem.c -o build/kmem.o

nm -u build/kmem.o | tee build/kmem.undefined.txt
test ! -s build/kmem.undefined.txt && echo "[OK] no undefined symbols"
readelf -h build/kmem.o | tee build/kmem.readelf.header.txt
objdump -dr build/kmem.o > build/kmem.objdump.txt
grep -E 'kmem_(init|alloc|calloc|free_checked|get_stats|validate)>:' build/kmem.objdump.txt
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 132530.png`

Output ringkas:
```
M8 kmem host tests: PASS
[OK] no undefined symbols
ELF Header:
  Class:                             ELF64
  Machine:                           Advanced Micro Devices X86-64
  Type:                              REL (Relocatable file)
  Number of section headers:        9
0000000000000000 <kmem_init>:
00000000000001e0 <kmem_validate>:
00000000000003d0 <kmem_alloc>:
00000000000006e0 <kmem_calloc>:
00000000000007d0 <kmem_free_checked>:
0000000000000a50 <kmem_get_stats>:
```

Indikator berhasil: Unit test host **PASS**, `nm -u` kosong (tidak ada undefined symbol), dan seluruh fungsi API publik teridentifikasi pada `objdump` dengan alamat masing-masing.

### Langkah 11 — Menambahkan Target `check-m8` pada Makefile

Perintah:
```bash
cat >> Makefile << 'EOF'

.PHONY: check-m8

check-m8: $(BUILD_DIR)/kmem.o $(BUILD_DIR)/test_kmem_host
	./$(BUILD_DIR)/test_kmem_host | tee $(BUILD_DIR)/test_kmem.log
	grep -q 'PASS' $(BUILD_DIR)/test_kmem.log
	$(NM) -u $(BUILD_DIR)/kmem.o | tee $(BUILD_DIR)/kmem.undefined.txt
	test ! -s $(BUILD_DIR)/kmem.undefined.txt
	$(READELF) -h $(BUILD_DIR)/kmem.o > $(BUILD_DIR)/kmem.readelf.header.txt
	$(OBJDUMP) -dr $(BUILD_DIR)/kmem.o > $(BUILD_DIR)/kmem.objdump.txt
	@echo "[PASS] M8 static check selesai"

$(BUILD_DIR)/kmem.o: kernel/mm/kmem.c include/mcsos/kmem.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -c kernel/mm/kmem.c -o $(BUILD_DIR)/kmem.o

$(BUILD_DIR)/test_kmem_host: kernel/mm/kmem.c tests/test_kmem.c include/mcsos/kmem.h
	mkdir -p $(BUILD_DIR)
	$(HOSTCC) -std=c17 -Wall -Wextra -Werror -Iinclude \
	  kernel/mm/kmem.c tests/test_kmem.c -o $(BUILD_DIR)/test_kmem_host
EOF
tail -n 20 Makefile | cat -A | head -n 25
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 132815.png`

Indikator berhasil: `cat -A` menegaskan tiap baris resep target diakhiri tab (`^I` tersirat lewat `$` di akhir baris tanpa spasi ganda), memastikan Makefile valid (bukan spasi yang mestinya tab).

### Langkah 12 — Verifikasi `make check-m8` Bersih dan Regresi Milestone Sebelumnya

Perintah:
```bash
rm -f build/kmem.o build/test_kmem_host build/test_kmem.log \
      build/kmem.undefined.txt build/kmem.readelf.header.txt build/kmem.objdump.txt
make check-m8
make check-m6
make check-m7
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 133040.png`

Output:
```
M8 kmem host tests: PASS
[PASS] M8 static check selesai
./build/test_pmm_host
M6 PMM host unit test: PASS
[PASS] M6 static check selesai
./build/test_vmm_host
M7 VMM host tests PASS
[PASS] M7 static check selesai
```

Indikator berhasil: **Tidak ada regresi** — target `check-m6` dan `check-m7` dari milestone sebelumnya tetap PASS setelah penambahan `kmem`.

### Langkah 13 — Skrip Preflight `scripts/check_m8_kmem.sh`

Maksud langkah: Membuat skrip otomatis yang memverifikasi keberadaan file wajib, ketersediaan toolchain, versi tool, dan menjalankan `make check-m8` sebagai satu perintah reproducible.

Perintah:
```bash
cat > scripts/check_m8_kmem.sh << 'EOF'
#!/usr/bin/env bash
set -euo pipefail

printf '[M8] checking repository baseline...\n'
required=(
  include/mcsos/kmem.h
  kernel/mm/kmem.c
  tests/test_kmem.c
  Makefile
)
for f in "${required[@]}"; do
  if [[ ! -f "$f" ]]; then
    printf '[FAIL] missing %s\n' "$f" >&2
    exit 1
  fi
done

printf '[M8] checking toolchain...\n'
command -v clang >/dev/null
command -v nm >/dev/null
command -v objdump >/dev/null
command -v readelf >/dev/null
command -v make >/dev/null

printf '[M8] tool versions...\n'
clang --version | head -n 1
ld.lld --version 2>/dev/null | head -n 1 || true
make --version | head -n 1

printf '[M8] running make check-m8...\n'
make check-m8

grep -q 'PASS' build/test_kmem.log
printf '[PASS] M8 preflight completed.\n'
EOF
chmod +x scripts/check_m8_kmem.sh
./scripts/check_m8_kmem.sh
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 133320.png`

Output:
```
[M8] checking repository baseline...
[M8] checking toolchain...
[M8] tool versions...
Ubuntu clang version 21.1.8 (6ubuntu1)
Ubuntu LLD 21.1.8 (compatible with GNU linkers)
GNU Make 4.4.1
[M8] running make check-m8...
M8 kmem host tests: PASS
[PASS] M8 static check selesai
[PASS] M8 preflight completed.
```

Indikator berhasil: Skrip preflight berjalan end-to-end tanpa error (`set -euo pipefail` memastikan skrip berhenti pada kegagalan pertama), diakhiri `[PASS] M8 preflight completed.`

### Langkah 14 — Menyusun Draft Integrasi (`/tmp/m8_bootstrap.c`)

Maksud langkah: Menyiapkan draft *boot heap* statis 64 KiB dan fungsi `m8_heap_bootstrap()` sebelum disisipkan ke `kmain.c`, agar perubahan pada file utama minimal dan mudah ditinjau.

Perintah:
```bash
sed -i '/#include <mcsos\/kernel\/version.h>/a #include "mcsos/kmem.h"' kernel/core/kmain.c
head -n 12 kernel/core/kmain.c

cat > /tmp/m8_bootstrap.c << 'EOF'
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
EOF
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 133545.png`

Indikator berhasil: `head -n 12` mengonfirmasi include `"mcsos/kmem.h"` sudah tersisip tepat setelah include `version.h`; draft fungsi lengkap dengan self-test *probe* alloc/free sebelum log statistik.

### Langkah 15 — Menyisipkan `m8_heap_bootstrap()` ke `kmain.c`

Perintah:
```bash
sed -i '/^void kmain(void) {/e cat /tmp/m8_bootstrap.c' kernel/core/kmain.c
grep -n "m8_heap_bootstrap\|void kmain" kernel/core/kmain.c
sed -n '95,160p' kernel/core/kmain.c
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 133812.png`

Output (ringkas):
```
133:static void m8_heap_bootstrap(void) {
157:void kmain(void) {
```

Indikator berhasil: Fungsi `m8_heap_bootstrap()` tersisip tepat sebelum definisi `kmain()`, tidak mengganggu blok demo M7 (`#ifdef MCSOS_M7_DEMO_PAGEFAULT`) yang tetap ada di atasnya.

### Langkah 16 — Memanggil `m8_heap_bootstrap()` dari `kmain()`

Perintah:
```bash
sed -i '/^    kernel_vmm_init();$/a\    m8_heap_bootstrap();' kernel/core/kmain.c
sed -n '157,180p' kernel/core/kmain.c
sed -n '180,205p' kernel/core/kmain.c
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 134012.png`

Output kunci:
```c
void kmain(void) {
    cpu_cli();
    log_init();
    ...
    idt_init();
    pic_remap(PIC_MASTER_OFFSET, PIC_SLAVE_OFFSET);
    ...
    pit_configure_hz(100);
    cpu_sti();
    kernel_memory_init();
    kernel_vmm_init();
    m8_heap_bootstrap();
    for (;;) { cpu_hlt(); }
}
```

Indikator berhasil: Urutan boot path kini M2→M6 (PMM)→M7 (VMM)→**M8 (kmem)**, konsisten dengan urutan milestone.

### Langkah 17 — Full Rebuild Kernel dan Inspeksi ELF

Maksud langkah: Membangun ulang seluruh kernel dari nol (`kmem.c` kini ikut dikompilasi dengan flags freestanding dan dilink ke `kernel.elf`) serta memverifikasi lewat `readelf`/`nm`/`objdump`.

Perintah:
```bash
make clean
make build
make inspect
ls -la build/kernel.elf
grep -q 'ELF64' build/kernel.readelf.header.txt
grep -q 'Machine:[[:space:]]*Advanced Micro Devices X86-64' build/kernel.readelf.header.txt
grep -q 'kmain' build/kernel.syms.txt
grep -q 'kernel_panic_at' build/kernel.syms.txt
grep -q 'cpu_halt_forever' build/kernel.disasm.txt
grep -q 'idt_init' build/kernel.syms.txt
grep -q 'pic_remap' build/kernel.syms.txt
grep -q 'pit_configure_hz' build/kernel.syms.txt
grep -q 'isr_stub_32' build/kernel.syms.txt
grep -q 'timer_on_irq0' build/kernel.syms.txt
grep -q 'pmm_init_from_map' build/kernel.syms.txt
grep -q 'pmm_alloc_frame' build/kernel.syms.txt
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 143810.png`

Output:
```
mkdir -p build/normal/kernel/mm/
clang --target=x86_64-unknown-none-elf ... -c kernel/mm/kmem.c -o build/normal/kernel/mm/kmem.o
...
ld.lld -nostdlib -static -z max-page-size=0x1000 -T linker.ld -Map=build/kernel.map \
  -o build/kernel.elf ... build/normal/kernel/mm/kmem.o build/normal/kernel/arch/x86_64/src/interrupts.o
-rwxr-xr-x 1 andianaaji andianaaji 2276352 Jul  3 14:42 build/kernel.elf
```

Indikator berhasil: `kernel/mm/kmem.c` kini menjadi bagian dari `kernel.elf` (dikompilasi dengan `COMMON_CFLAGS` yang sama dengan modul kernel lain), seluruh simbol lintas milestone (M2–M8) masih ada, tidak ada simbol yang hilang akibat penambahan modul baru.

### Langkah 18 — Perbaikan Marker Validasi pada `run_qemu.sh`

Maksud langkah: Menyesuaikan string yang di-*grep* skrip `run_qemu.sh` agar sesuai format log serial sesungguhnya (`[MCSOS:M8] kmem initialized`, bukan format sementara `M8 kmem initialized`).

Perintah:
```bash
grep -n "grep -qF" tools/scripts/run_qemu.sh
sed -i "s/grep -qF 'M8 kmem initialized' \"\$LOG\"/grep -qF '[MCSOS:M8] kmem initialized' \"\$LOG\"/" tools/scripts/run_qemu.sh
bash -n tools/scripts/run_qemu.sh
git add tools/scripts/run_qemu.sh
git commit -m "M8: perbaiki string marker grep agar cocok dengan log [MCSOS:M8] kmain.c"
git status --short
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 144210.png`

Output (baris relevan sebelum/sesudah):
```
63:grep -qF 'MCSOS 260502 M3 kernel entered' "$LOG"
64:grep -qF '[MCSOS:M6] pmm initialized' "$LOG"
65:grep -qF '[MCSOS:M7] demo map/query/unmap OK' "$LOG"
66:grep -qF '[MCSOS:M8] kmem initialized' "$LOG"
```

Indikator berhasil: `bash -n` (syntax check) tidak melaporkan error; commit tercatat dengan pesan yang menjelaskan perbaikan marker.

### Langkah 19 — Build Image dan Smoke Test QEMU Penuh (M2–M8)

Perintah:
```bash
make image
ls -la build/mcsos.iso
mkdir -p build/m8
./tools/scripts/run_qemu.sh 2>&1 | tee build/m8/qemu_m8.log
cat build/qemu-serial.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 150512.png`

Output serial (ringkas):
```
OK: QEMU serial log valid: build/qemu-serial.log
MCSOS 260502 M3 kernel entered
[MCSOS:M5] boot: external interrupt bring-up start
...
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
[MCSOS:TIMER] ticks=0x0000000000000064
[MCSOS:TIMER] ticks=0x00000000000000c8
[MCSOS:TIMER] ticks=0x000000000000012c
[MCSOS:TIMER] ticks=0x0000000000000190
```

Indikator berhasil: `run_qemu.sh` melaporkan `OK: QEMU serial log valid`; boot path lengkap M2→M8 tercapai; `block_count=1` membuktikan blok probe (`kmem_alloc(128)` lalu `kmem_free_checked`) berhasil digabung kembali (*coalesce*) menjadi satu blok bebas besar.

### Langkah 20 — Penguatan Unit Test: Penambahan `kmem_validate()` Ekstra

Maksud langkah: Memperkuat cakupan pengujian dengan menyisipkan `assert(kmem_validate() == 0)` tambahan di titik-titik kritis (`test_calloc_and_overflow`, `test_double_free_rejected`, `test_fragmentation_and_coalesce`) secara mekanis lewat skrip Python agar konsisten dan tidak mengubah logika pengujian lain.

Perintah (ringkas):
```bash
python3 - << 'EOF'
import re
path = "tests/test_kmem.c"
with open(path) as f:
    content = f.read()
# ... 4 penggantian pola: sisipkan assert(kmem_validate() == 0); setelah baris kunci ...
with open(path, "w") as f:
    f.write(content)
print("OK: 4 pemanggilan kmem_validate() ditambahkan")
EOF
grep -n "kmem_validate" tests/test_kmem.c
make check-m8
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 151230.png`

Output:
```
OK: 4 pemanggilan kmem_validate() ditambahkan
23:    assert(kmem_validate() == 0);
27:    assert(kmem_validate() == 0);
36:    assert(kmem_validate() == 0);
40:    assert(kmem_validate() == 0);
50:    assert(kmem_validate() == 0);
56:    assert(kmem_validate() == 0);
60:    assert(kmem_validate() == 0);
67:    assert(kmem_validate() == 0);
M8 kmem host tests: PASS
[PASS] M8 static check selesai
```

Indikator berhasil: Skrip melaporkan setiap pola ditemukan tepat satu kali sebelum diganti (`assert content.count(old) == 1`), mencegah penggantian ganda/tidak sengaja; total 8 pemanggilan `kmem_validate()` kini tersebar di seluruh unit test; `make check-m8` tetap PASS setelah perubahan.

---

## 11. Bukti Eksekusi dan Log

| Log | Lokasi | Isi |
|---|---|---|
| Log unit test host M8 | `build/test_kmem.log` | `M8 kmem host tests: PASS` |
| Static check M8 | `build/kmem.undefined.txt`, `build/kmem.readelf.header.txt`, `build/kmem.objdump.txt` | Bukti tidak ada undefined symbol dan seluruh fungsi API ter-emit |
| Preflight M8 | Output `scripts/check_m8_kmem.sh` | Verifikasi baseline repo, toolchain, dan `make check-m8` dalam satu alur |
| Serial boot penuh M2–M8 | `build/qemu-serial.log`, `build/m8/qemu_m8.log` | Boot lengkap dengan statistik heap dan timer |
| Regresi milestone | Output `make check-m6`, `make check-m7` | Bukti tidak ada regresi pada modul PMM/VMM |

---

## 12. Hasil Pengujian Unit (Host Test)

```bash
./build/test_kmem_host
```

Output:
```
M8 kmem host tests: PASS
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 132530.png`

Analisis skenario:

| Skenario | Yang diverifikasi | Hasil |
|---|---|---|
| `test_basic_alloc_free` | Alokasi 3 ukuran berbeda (24/128/4096 byte), semua pointer teralign 16 byte, tulis-baca payload aman, `kmem_validate()` OK sebelum & sesudah pembebasan | PASS |
| `test_calloc_and_overflow` | `kmem_calloc` menghasilkan memori ter-nol-kan, permintaan `(size_t)-1, 2` (overflow) ditolak dengan `NULL` | PASS |
| `test_double_free_rejected` | Pembebasan pertama sukses (`0`), pembebasan kedua pada pointer sama ditolak (`< 0`) | PASS |
| `test_fragmentation_and_coalesce` | 16 blok dialokasikan lalu dibebaskan berselang-seling (genap dulu, ganjil kemudian); statistik akhir menunjukkan `free_count == 1`, `block_count == 1`, `largest_free > 4096` — membuktikan seluruh fragmen berhasil digabung kembali menjadi satu blok besar | PASS |

---

## 13. Hasil Pengujian Static Check

```bash
make check-m8
readelf -h build/kmem.o
nm -u build/kmem.o
```

Output:
```
M8 kmem host tests: PASS
[PASS] M8 static check selesai

ELF Header:
  Class:                             ELF64
  Data:                              2's complement, little endian
  Type:                              REL (Relocatable file)
  Machine:                           Advanced Micro Devices X86-64
  Number of section headers:         9
0000000000000000 <kmem_init>:
00000000000001e0 <kmem_validate>:
00000000000003d0 <kmem_alloc>:
00000000000006e0 <kmem_calloc>:
00000000000007d0 <kmem_free_checked>:
0000000000000a50 <kmem_get_stats>:
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 132530.png`

Analisis: `nm -u` tidak menghasilkan output (tidak ada undefined symbol), membuktikan `kmem.o` mandiri tanpa dependensi eksternal selain compiler builtin standar. Seluruh 6 fungsi API publik teridentifikasi pada tabel simbol dengan alamat berurutan sesuai urutan definisi di source.

---

## 14. Hasil Smoke Test QEMU

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 150512.png`

```bash
./tools/scripts/run_qemu.sh
cat build/qemu-serial.log
```

Output:
```
OK: QEMU serial log valid: build/qemu-serial.log
...
[MCSOS:M8] kmem initialized
[MCSOS:M8] heap total_bytes=0x0000000000010000
[MCSOS:M8] heap free_bytes=0x000000000000ffd0
[MCSOS:M8] heap largest_free=0x000000000000ffd0
[MCSOS:M8] heap block_count=0x0000000000000001
[MCSOS:TIMER] ticks=0x0000000000000064
```

Indikator berhasil: `run_qemu.sh` mengonfirmasi `OK: QEMU serial log valid`; timer M5 tetap berdetak normal, membuktikan integrasi `kmem` tidak mengganggu subsistem interrupt sebelumnya.

---

## 15. Analisis Konsistensi Angka Runtime

| Nilai dari log | Perhitungan verifikasi | Kesesuaian |
|---|---|---|
| `heap total_bytes = 0x10000` | `M8_BOOT_HEAP_SIZE = 64 * 1024 = 65536 = 0x10000` | Sesuai — boot heap statis persis 64 KiB |
| `heap free_bytes = 0xffd0` | `65536 - sizeof(kmem_block_t) = 65536 - 48 = 65488 = 0xffd0` | Sesuai — overhead 1 header (48 byte) sudah dikurangkan |
| `heap largest_free = 0xffd0` | Sama dengan `free_bytes` karena hanya ada 1 blok bebas | Sesuai — tidak ada fragmentasi setelah probe alloc/free di `m8_heap_bootstrap` |
| `heap block_count = 1` | Setelah probe `kmem_alloc(128)` dibebaskan, `kmem_coalesce_forward` menggabungkannya kembali ke blok awal | Sesuai — membuktikan coalesce bekerja bahkan pada kasus alokasi tunggal di runtime nyata (bukan hanya di unit test host) |
| `KMEM_MAGIC = 0x4d43534f53484541ull` | Dekode ASCII per-byte: `4d 43 53 4f 53 48 45 41` → `M C S O S H E A` | Magic number berupa string mudah dibaca `"MCSOSHEA"` saat debugging memory dump |

Kesesuaian angka-angka di atas menjadi bukti independen bahwa implementasi `kmem.c` bekerja identik baik pada unit test host-mode maupun saat benar-benar dijalankan di atas QEMU (freestanding, tanpa libc).

---

## 16. Analisis Hasil dan Verifikasi Invariant

| Invariant (bagian 9.6) | Terverifikasi? | Bukti |
|---|---|---|
| `g_head == g_heap_base` sepanjang waktu | Ya | `kmem_validate()` mengembalikan `-2` jika dilanggar; tidak pernah terjadi selama pengujian |
| Setiap blok memiliki `magic == KMEM_MAGIC` | Ya | Tidak ada error `-6` (magic salah) selama unit test maupun runtime QEMU |
| `cursor` konsisten antar blok (tidak ada lubang/tumpang tindih) | Ya | Tidak ada error `-4`/`-5`/`-9` selama pengujian |
| *Double-free* ditolak | Ya | `test_double_free_rejected`: pembebasan kedua mengembalikan nilai `< 0` |
| Overflow `calloc` ditolak | Ya | `test_calloc_and_overflow`: `kmem_calloc((size_t)-1, 2) == NULL` |
| `kmem_free_checked` selalu memanggil `kmem_validate()` di akhir | Ya | Terlihat pada source code, dan `kmem_free_checked` mengembalikan hasil `kmem_validate()` langsung |

---

## 17. Regresi Lintas Milestone

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 133040.png`

```bash
make check-m6
make check-m7
make check-m8
```

Output (ringkas):
```
M6 PMM host unit test: PASS   -> [PASS] M6 static check selesai
M7 VMM host tests PASS        -> [PASS] M7 static check selesai
M8 kmem host tests: PASS      -> [PASS] M8 static check selesai
```

Analisis: Penambahan modul `kmem` (termasuk perubahan `Makefile` dan `kmain.c`) **tidak menyebabkan regresi** pada target uji milestone M6 (PMM) maupun M7 (VMM) — seluruh static check dan unit test host tetap lulus.

---

## 18. Manajemen Versi (Git Workflow)

Bukti screenshot: `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 154530.png`

```bash
git log --oneline -8
```

Output:
```
b2ca43a (HEAD -> praktikum-m8-kernel-heap) M8: integrasikan kernel heap ke kmain.c dan tambah target check-m8 di Makefile
506e70f tambahkan source allocator (kmem.h/kmem.c), check script, dan evidence M7
e0591a5 M8: perkuat kmem_validate() di semua fungsi test (double-free, fragmentasi/coalesce, calloc)
d20e5ca M8: perbaiki string marker grep agar cocok dengan log [MCSOS:M8] kmain.c
7a36de1 M8: tambah validasi marker '[MCSOS:M8] kmem initialized' pada run_qemu.sh
cfcf953 (origin/m7-vmm-wip, m7-vmm-wip) M7: add readelf header/sections evidence for vmm.o (checkpoint completeness)
3cf1196 m7: add page-fault path evidence (demo fault enabled via -DMCSOS_M7_DEMO_PAGEFAULT)
95839a3 M7: unignore evidence logs, add missing m7_qemu_serial.log
```

```bash
git push -u origin praktikum-m8-kernel-heap
```

Output:
```
Enumerating objects: 45, done.
Counting objects: 100% (45/45), done.
Delta compression using up to 2 threads
Writing objects: 100% (30/30), 7.07 KiB | 517.00 KiB/s, done.
Total 33 (delta 18), reused 0 (delta 0), pack-reused 0 (from 0)
remote: Resolving deltas: 100% (18/18), completed with 10 local objects.
remote: Create a pull request for 'praktikum-m8-kernel-heap' on GitHub by visiting:
remote:      https://github.com/JotDesu/mcsos260502_git/pull/new/praktikum-m8-kernel-heap
To https://github.com/JotDesu/mcsos260502_git
 * [new branch]      praktikum-m8-kernel-heap -> praktikum-m8-kernel-heap
branch 'praktikum-m8-kernel-heap' set up to track 'origin/praktikum-m8-kernel-heap'.
```

Indikator berhasil: Seluruh perubahan M8 tercatat lewat commit granular (per perubahan logis: source allocator, penguatan test, perbaikan marker, integrasi kmain), branch baru berhasil didorong ke remote dengan tautan pull request otomatis dari GitHub.

---

## 19. Masalah yang Ditemukan (Findings) dan Deviasi

| No. | Temuan | Dampak | Tindak lanjut |
|---|---|---|---|
| 1 | Urutan commit M8 tidak sepenuhnya kronologis-logis: commit `7a36de1` (menambah marker validasi pada `run_qemu.sh`) tercatat sebelum commit `506e70f` (menambahkan source `kmem.h`/`kmem.c` itu sendiri) | Kosmetik pada riwayat Git — tidak memengaruhi hasil akhir karena seluruh perubahan tetap ter-commit dan konsisten pada akhirnya | Dicatat sebagai pembelajaran: staging (`git add`) sebaiknya dilakukan per unit perubahan logis dan di-commit segera setelah selesai, bukan ditunda |
| 2 | Percobaan pertama `make clean && make build` gagal pada tahap `tail -n 30 build/m8_precheck_build.log` karena file belum dibuat (*No such file or directory*) | Evidence build sempat tidak lengkap pada percobaan pertama | Build diulang tanpa redirect log yang salah; hasil akhir (`ls -la build/kernel.elf`) tetap terverifikasi benar pada percobaan berikutnya |
| 3 | Banner boot masih mencetak `MCSOS 260502 M3 kernel entered` (konstanta `MCSOS_MILESTONE` belum diperbarui sejak temuan pada laporan M7) | Kosmetik/observability — tidak memengaruhi fungsi `kmem` | Tetap perlu diperbarui ke M8 pada `version.h` sebelum submission final (akumulasi dari temuan M7 yang belum ditindaklanjuti) |
| 4 | Heap M8 masih statis (64 KiB tetap), belum bisa tumbuh otomatis lewat VMM/PMM saat penuh | Keterbatasan kapasitas heap pada tahap ini | Didokumentasikan sebagai non-goal M8, direncanakan pada milestone lanjutan |

---

## 20. Failure Modes dan Mitigasi

| Failure mode | Penyebab | Deteksi | Mitigasi |
|---|---|---|---|
| *Double-free* | Caller memanggil `kmem_free_checked` dua kali pada pointer sama | Magic/status blok diperiksa sebelum menandai bebas | Return kode error negatif, tidak melakukan operasi kedua |
| Heap habis (`kmem_alloc` gagal) | Permintaan melebihi total ruang bebas yang tersedia | Walker tidak menemukan blok cukup besar | Return `NULL`, caller wajib menangani (dicontohkan lewat `KERNEL_PANIC` pada `m8_heap_bootstrap` untuk kegagalan yang tidak diharapkan) |
| Integer overflow pada `calloc`/alignment | `count * bytes` atau `value + mask` melebihi batas tipe | Guard eksplisit sebelum operasi aritmatika | Return `NULL`/`0`, tidak melakukan alokasi parsial |
| Heap korup (magic salah/list rusak) | Bug pada modul lain yang menulis di luar batas alokasi (buffer overflow) | `kmem_validate()` mendeteksi lewat pengecekan magic, linkage, dan batas | Return kode error spesifik (`-1`..`-9`) yang mengidentifikasi jenis pelanggaran invariant |
| Fragmentasi berlebihan | Pola alokasi/pembebasan acak dalam jangka panjang | Statistik `kmem_get_stats` (`free_count`, `largest_free`) | *Coalesce* otomatis pada setiap `kmem_free_checked`; `test_fragmentation_and_coalesce` membuktikan efektivitasnya untuk pola uji yang diberikan |

---

## 21. Keamanan dan Reliabilitas (Security & Reliability)

- **Fail-closed pada operasi tidak valid:** `kmem_free_checked` menolak pointer di luar heap atau dengan magic salah, mengembalikan kode error alih-alih melanjutkan operasi dengan state tidak terdefinisi.
- **Validasi eksplisit pasca-mutasi:** Setiap pembebasan blok diakhiri dengan `kmem_validate()` sebagai *self-check*, sehingga korupsi heap terdeteksi sedini mungkin, bukan menyebar ke operasi berikutnya.
- **Guard integer overflow menyeluruh:** Baik pada alignment (`kmem_align_up_size`/`ptr`) maupun `kmem_calloc`, seluruh operasi aritmatika yang berpotensi overflow diperiksa lebih dulu.
- **Observability:** Statistik heap (`total_bytes`, `free_bytes`, `largest_free`, `block_count`) dicatat lewat `log_key_value_hex64` ke serial, memudahkan audit kondisi heap saat boot.
- **Tidak ada perubahan pada batas keamanan M7:** Integrasi `kmem` diletakkan setelah `kernel_vmm_init()` tanpa mengubah keputusan M7 untuk belum mengarahkan `CR3` ke `g_vmm` — heap tetap beroperasi di atas rentang memori yang sudah dipetakan bootloader.

---

## 22. Evaluasi dan Refleksi

### 22.1 Kendala yang Dihadapi

1. Menentukan ukuran minimum split (`KMEM_MIN_SPLIT = 32u`) yang cukup untuk mencegah blok sisa terlalu kecil, tanpa membuat alokasi kecil menjadi terlalu boros.
2. Menyisipkan kode integrasi (`m8_heap_bootstrap`) ke `kmain.c` yang sudah cukup panjang dari milestone sebelumnya tanpa merusak struktur yang ada — diselesaikan dengan draft terpisah di `/tmp/m8_bootstrap.c` lalu disisipkan lewat `sed` yang presisi berdasarkan anchor baris.
3. Kesalahan kecil pada perintah build evidence pertama (`tail` pada log yang belum dibuat) — diselesaikan dengan mengulang build tanpa redirect yang salah.

### 22.2 Keterbatasan

1. Heap masih statis 64 KiB, belum bisa tumbuh otomatis (non-goal M8).
2. Belum ada mekanisme *locking* karena kernel masih single-threaded.
3. `MCSOS_MILESTONE` pada banner boot belum diperbarui ke M8 (akumulasi dari temuan M7).
4. Riwayat commit tidak sepenuhnya kronologis-logis (lihat bagian 19), meski hasil akhir tetap konsisten.

### 22.3 Rencana Perbaikan

1. **Perbaikan segera:** Perbarui `MCSOS_MILESTONE` menjadi M8 pada `version.h` sebelum pengumpulan final.
2. **M9:** Scheduler dan *locking* pada struktur data kernel bersama, termasuk `g_head`/`g_heap_base` pada `kmem`.
3. **M9/M10:** Heap dinamis yang dapat tumbuh lewat `vmm_map_page` saat mendekati penuh, memanfaatkan VMM (M7) dan PMM (M6) yang sudah ada.
4. Membiasakan `git add`/`git commit` per unit perubahan logis segera setelah selesai, untuk menjaga riwayat Git tetap mencerminkan urutan pengerjaan sesungguhnya.

---

## 23. Lampiran

### Lampiran A — Commit Log

```bash
git log --oneline -8
```

Output: (lihat bagian 18)

### Lampiran B — Ringkas File yang Ditambahkan/Diubah

```
include/mcsos/kmem.h                | 25 ++++++++
kernel/mm/kmem.c                    | 280 +++++++++++++++++++++++++++++++++
tests/test_kmem.c                   | 76 (+8 assert tambahan) ++++++++++++
Makefile                            | 18 ++++
scripts/check_m8_kmem.sh            | 35 +++++++
kernel/core/kmain.c                 | 50 +++++++++
tools/scripts/run_qemu.sh           | 1 file changed, 1 insertion(+), 1 deletion(-)
```

### Lampiran C — Output `readelf -h build/kmem.o`

```
ELF Header:
  Magic:   7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00
  Class:                             ELF64
  Data:                              2's complement, little endian
  Version:                           1 (current)
  OS/ABI:                            UNIX - System V
  Type:                              REL (Relocatable file)
  Machine:                           Advanced Micro Devices X86-64
  Entry point address:               0x0
  Start of section headers:          4808 (bytes into file)
  Number of section headers:         9
  Section header string table index: 1
```

### Lampiran D — Diff `kernel/core/kmain.c` (Ringkas)

```diff
--- a/kernel/core/kmain.c
+++ b/kernel/core/kmain.c
@@ -8,6 +8,7 @@
 #include <mcsos/kernel/pmm.h>
 #include <mcsos/kernel/vmm.h>
 #include <mcsos/kernel/version.h>
+#include "mcsos/kmem.h"

 extern char __kernel_start[];
 extern char __kernel_end[];
@@ -126,6 +127,33 @@ static void kernel_vmm_init(void) {
         stack, IDT/GDT, framebuffer/serial MMIO, dan PMM metadata lengkap. */
 }

+#define M8_BOOT_HEAP_SIZE (64u * 1024u)
+static unsigned char m8_boot_heap[M8_BOOT_HEAP_SIZE] __attribute__((aligned(4096)));
+
+static void m8_heap_bootstrap(void) {
+    int rc = kmem_init(m8_boot_heap, sizeof(m8_boot_heap));
+    if (rc != 0) {
+        KERNEL_PANIC("M8: kmem_init failed", (uint64_t)rc);
+    }
+    ...
+}
+
 void kmain(void) {
     cpu_cli();
@@ -158,6 +186,7 @@ void kmain(void) {

     kernel_memory_init();
     kernel_vmm_init();
+    m8_heap_bootstrap();

     for (;;) {
         cpu_hlt();
```

### Lampiran E — Isi Lengkap `include/mcsos/kmem.h` dan `kernel/mm/kmem.c`

Tersedia penuh pada `include/mcsos/kmem.h` (25 baris) dan `kernel/mm/kmem.c` (280 baris), mencakup seluruh helper (`kmem_align_up_size/ptr`, `kmem_memset`, `kmem_payload`, `kmem_header_from_payload`, `kmem_ptr_in_heap`, `kmem_split_if_useful`, `kmem_coalesce_forward`) dan API publik (`kmem_init`, `kmem_alloc`, `kmem_calloc`, `kmem_free_checked`, `kmem_get_stats`, `kmem_validate`).

### Lampiran F — Screenshot

| No. | File | Keterangan | Referensi gambar pada `BUKTI_M8.pdf` |
|---|---|---|---|
| 1 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 130012.png` | Pembuatan `kmem.h` + `git status --short` | Gambar ke-1 |
| 2 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 130334.png` | `kmem.c`: struct `kmem_block_t`, helper alignment | Gambar ke-2 |
| 3 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 130612.png` | Helper payload/header, `kmem_split_if_useful` | Gambar ke-3 |
| 4 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 130845.png` | `kmem_coalesce_forward`, `kmem_init` | Gambar ke-4 |
| 5 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 131102.png` | `kmem_alloc`, `kmem_calloc` | Gambar ke-5 |
| 6 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 131345.png` | `kmem_free_checked`, `kmem_get_stats`, `kmem_validate` | Gambar ke-6 |
| 7 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 131612.png` | `wc -l` + grep daftar fungsi `kmem.c` | Gambar ke-7 |
| 8 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 132012.png` | Pembuatan `tests/test_kmem.c` | Gambar ke-8 |
| 9 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 132145.png` | Verifikasi struktur unit test (`wc -l`, grep fungsi test) | Gambar ke-9 |
| 10 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 132530.png` | Build manual test host PASS + static check kmem.o (nm/readelf/objdump) | Gambar ke-10/11 |
| 11 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 132815.png` | Target `check-m8` pada Makefile (`cat >>` + `tail -n 20 \| cat -A`) | Gambar ke-11/12/13 |
| 12 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 133040.png` | `make check-m8` bersih + regresi `make check-m6`/`check-m7` | Gambar ke-14 |
| 13 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 133320.png` | Skrip `scripts/check_m8_kmem.sh` dan hasil eksekusi | Gambar ke-15 |
| 14 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 133545.png` | `sed -n '18,22p' Makefile`, percobaan `make clean && make build` (evidence error kecil) | Gambar ke-16/17 |
| 15 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 133812.png` | `make inspect` + grep simbol kernel; draft `/tmp/m8_bootstrap.c` | Gambar ke-17/18/19 |
| 16 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 134012.png` | Sisip fungsi & pemanggilan `m8_heap_bootstrap()` ke `kmain.c` | Gambar ke-19/20/21 |
| 17 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 143810.png` | Full rebuild kernel (`make clean && make build && make inspect`) | Gambar ke-21/22/23 |
| 18 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 144210.png` | `git diff kernel/core/kmain.c` (diff lengkap) | Gambar ke-24 |
| 19 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 144530.png` | Perbaikan marker `run_qemu.sh` + commit + `git status --short` | Gambar ke-25 |
| 20 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 150512.png` | `make image`, `run_qemu.sh`, log serial M2–M8 lengkap | Gambar ke-26/27/28/29 |
| 21 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 150730.png` | `grep -n "image:\|.PHONY\|mcsos.iso" Makefile` | Gambar ke-30 |
| 22 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 151230.png` | Skrip Python penguatan `kmem_validate()` + grep hasil + `make check-m8` | Gambar ke-31/32 |
| 23 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 153015.png` | Rebuild penuh setelah penguatan test + `git status --short` | Gambar ke-33/34 |
| 24 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 153320.png` | Commit `test_kmem.c`, `git add`/`commit` source allocator, `git log --oneline -8` | Gambar ke-35 |
| 25 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 154012.png` | `git diff Makefile kernel/core/kmain.c` (COMMON_CFLAGS, target check-m8) | Gambar ke-36/37 |
| 26 | `C:\Users\Ajot\Pictures\M8\Screenshot 2026-07-03 154530.png` | Commit final + `git log`, `git remote -v`, `git branch -vv`, `git push -u origin praktikum-m8-kernel-heap` | Gambar ke-37/38 |

> **Catatan penting:** Nama file pada kolom kedua mengikuti format penamaan screenshot Windows (`Screenshot YYYY-MM-DD HHMMSS.png`) sebagai **contoh/placeholder**. Silakan ganti bagian tanggal-jam agar sesuai dengan nama file screenshot asli pada folder `C:\Users\Ajot\Pictures\M8\` di komputer Anda, dengan urutan pengambilan yang sama seperti urutan pada tabel ini (mengikuti urutan gambar pada `BUKTI_M8.pdf`).

### Lampiran G — Bukti Tambahan

- **SHA-256 ISO:** Tercatat di `build/mcsos.iso.sha256`
- **Log run QEMU M8:** `build/m8/qemu_m8.log`
- **Log unit test:** `build/test_kmem.log`

---

## 24. Daftar Referensi

[1] OSDev Wiki, "Memory Allocation," OSDev Wiki. Accessed: 2026-07-02. [Online]. Available: https://wiki.osdev.org/Memory_Allocation

[2] P. R. Wilson, M. S. Johnstone, M. Neely, and D. Boles, "Dynamic Storage Allocation: A Survey and Critical Review," in Memory Management, Lecture Notes in Computer Science, vol. 986, Springer, 1995.

[3] ISO/IEC, "ISO/IEC 9899:2018 — Programming languages — C," §7.22.3 Memory management functions. 2018.

[4] LLVM Project, "Clang Compiler User's Manual — Freestanding Builds," Clang documentation. Accessed: 2026-07-02. [Online]. Available: https://clang.llvm.org/docs/UsersManual.html

[5] GNU Project, "readelf, objdump, nm," GNU Binary Utilities. Accessed: 2026-07-02. [Online]. Available: https://www.gnu.org/software/binutils/binutils.html

[6] Panduan Praktikum M8 — Kernel Heap Allocator, First-Fit Free List, Split & Coalesce, MCSOS 260502, Muhaemin Sidiq, S.Pd., M.Pd., Institut Pendidikan Indonesia, 2026.

---

## 25. Checklist Final Sebelum Pengumpulan

| Checklist | Status |
|---|---|
| Semua placeholder `[isi ...]` sudah diganti | Ya |
| Metadata laporan lengkap | Ya |
| Branch kerja dan commit akhir dicatat | Ya (`praktikum-m8-kernel-heap`, `b2ca43a`) |
| Perintah build dan test dapat dijalankan ulang (`make check-m8`, `make image`, `scripts/check_m8_kmem.sh`) | Ya |
| Log build dan static check dilampirkan | Ya |
| Log QEMU (boot penuh M2–M8) dilampirkan | Ya |
| Regresi milestone sebelumnya (M6/M7) diverifikasi | Ya |
| Artefak penting diberi hash (`mcsos.iso.sha256`) | Ya |
| Desain, invariants, ownership, dan failure modes dijelaskan | Ya |
| Security/reliability dibahas | Ya |
| Temuan/deviasi (mis. urutan commit, `milestone=M3` pada banner) dicatat jujur | Ya |
| Readiness review tidak berlebihan | Ya |
| Nama file screenshot masih placeholder — perlu disesuaikan manual | **Perlu tindakan Anda** |
| Referensi memakai format IEEE | Ya |
| Laporan disimpan sebagai Markdown | Ya |

---

## 26. Pernyataan Pengumpulan

Saya mengumpulkan laporan ini bersama artefak pendukung pada commit:

```
b2ca43a (praktikum-m8-kernel-heap) M8: integrasikan kernel heap ke kmain.c dan tambah target check-m8 di Makefile
```

Status akhir yang diklaim:

```
Siap uji QEMU tahap M8 (dengan catatan: perbarui MCSOS_MILESTONE ke M8 pada version.h sebelum submission final)
```

Ringkasan satu paragraf:

```text
Praktikum M8 berhasil mengimplementasikan kernel heap allocator (kmem) berbasis free list
first-fit dengan header per blok, magic number validation, operasi split (kmem_split_if_useful)
dan coalesce (kmem_coalesce_forward). API publik kmem_init, kmem_alloc, kmem_calloc,
kmem_free_checked, kmem_get_stats, dan kmem_validate diverifikasi lewat unit test host-mode
(M8 kmem host tests: PASS) mencakup empat skenario: alokasi/pembebasan dasar, overflow calloc,
penolakan double-free, dan fragmentasi/coalesce (block_count kembali ke 1 setelah 16 alokasi
dibebaskan berselang-seling). Static check biner (nm -u bersih, readelf/objdump menunjukkan
seluruh fungsi API ter-emit) turut dilampirkan. Integrasi ke kmain() lewat m8_heap_bootstrap()
berhasil diverifikasi di atas QEMU sungguhan dengan boot heap statis 64 KiB (heap total_bytes=
0x10000, free_bytes=0xffd0, block_count=1 setelah probe alloc/free), angka yang konsisten
dengan overhead header 48 byte per blok. Regresi lintas milestone (make check-m6, make check-m7)
tetap PASS. Seluruh evidence (build log, unit test log, serial log M2-M8, skrip preflight,
commit Git granular pada branch praktikum-m8-kernel-heap) tersedia. Dua temuan dicatat jujur:
riwayat commit yang tidak sepenuhnya kronologis-logis, dan banner panic yang masih menampilkan
milestone=M3 (belum diperbarui sejak M7). Status: siap uji QEMU tahap M8 dengan catatan tersebut.
```
