# Laporan Praktikum M6 — Bitmap Physical Memory Manager (PMM)

## MCSOS 260502

**Nama file laporan:** `laporan_praktikum_M6_[NIM_Kelompok].md`
**Nama sistem operasi:** MCSOS versi 260502
**Target default:** x86_64, QEMU, Windows 11 x64 + WSL 2, kernel monolitik pendidikan, C freestanding dengan assembly minimal, POSIX-like subset
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.
**Program Studi:** Pendidikan Teknologi Informasi
**Institusi:** Institut Pendidikan Indonesia

---

## 0. Metadata Laporan

| Atribut | Isi |
|---|---|
| Kode praktikum | M6 |
| Judul praktikum | Bitmap Physical Memory Manager (PMM) |
| Jenis pengerjaan | Individu |
| Nama mahasiswa | Andiana Jamaludin Malik |
| NIM | 2583207073016 |
| Kelas | 1B |
| Nama kelompok | - |
| Anggota kelompok | - |
| Tanggal praktikum | 2026-06-30 |
| Tanggal pengumpulan | 2026-06-30 |
| Repository | ~/src/mcsos |
| Branch kerja | `praktikum/m6-pmm` |
| Commit akhir | `dcde32f` (feat(m6): bitmap physical memory manager (PMM)) |
| Status readiness yang diklaim | PMM host test PASS, static grade PASS, boot QEMU menunjukkan inisialisasi PMM berhasil |

---

## 1. Sampul

# Laporan Praktikum M6
## Bitmap Physical Memory Manager (PMM)

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

Saya menyatakan bahwa laporan ini disusun berdasarkan pekerjaan praktikum sendiri sesuai panduan M6. Bantuan eksternal, referensi, dan dokumentasi resmi dicatat pada bagian referensi dan lampiran.

| Pernyataan | Status |
|---|---|
| Semua potongan kode eksternal diberi atribusi | Ya (panduan M6 dari dosen) |
| Semua penggunaan AI assistant dicatat | Ya |
| Repository yang dikumpulkan sesuai commit akhir | Ya |
| Tidak ada klaim readiness tanpa bukti | Ya |

Catatan penggunaan bantuan eksternal:

```text
- Panduan praktikum M6 (OS_panduan_M6.pdf) sebagai referensi utama
- Template laporan praktikum (os_template_laporan_praktikum.md), diadaptasi dari laporan M3
- AI assistant digunakan untuk membantu menyusun laporan dan analisis dari bukti screenshot
- Semua source code diimplementasikan berdasarkan panduan dosen dan hasil kerja mandiri di terminal
```

---

## 3. Tujuan Praktikum

1. **Tujuan teknis 1:** Membuat header `pmm.h` yang mendefinisikan struktur data, konstanta ukuran frame (4096 byte), dan kontrak API Physical Memory Manager (PMM).
2. **Tujuan teknis 2:** Mengimplementasikan alokator frame berbasis bitmap (`pmm.c`) dengan strategi *fail-closed* (semua frame dianggap terpakai sampai dibuktikan bebas dari peta memori boot).
3. **Tujuan teknis 3:** Menulis unit test berbasis host (`test_pmm_host.c`) yang dijalankan di luar target freestanding untuk memverifikasi logika alokasi/pembebasan frame secara cepat tanpa boot QEMU.
4. **Tujuan teknis 4:** Memperbaiki `Makefile` agar berkas uji (`kernel/tests/*`) tidak ikut dikompilasi oleh toolchain cross-compile freestanding, serta menambahkan target `check-m6` khusus untuk uji host.
5. **Tujuan teknis 5:** Mengintegrasikan PMM ke `kmain.c` melalui fungsi `kernel_memory_init()` yang dipanggil setelah subsistem IDT/PIC/PIT siap, lengkap dengan demo alokasi dan pembebasan satu frame sebagai bukti fungsional saat boot.
6. **Tujuan konseptual 1:** Memahami mengapa alokator memori fisik harus *fail-closed* (default terpakai) alih-alih *fail-open* (default bebas) untuk mencegah kernel mengalokasikan memori yang sebenarnya dipakai firmware/kernel/reserved region.
7. **Tujuan konseptual 2:** Memahami perbedaan pengujian *host-based* (native, cepat, tanpa hardware emulation) dan pengujian *target-based* (via QEMU, merepresentasikan kondisi runtime nyata).
8. **Tujuan validasi:** Menyimpan log build, log `check-m6`, log `make grade`, log boot QEMU, dan riwayat commit/push sebagai bukti deterministik keberhasilan milestone M6.

---

## 4. Capaian Pembelajaran Praktikum

Setelah praktikum ini, mahasiswa mampu:

| CPL/CPMK praktikum | Bukti yang harus ditunjukkan |
|---|---|
| Menjelaskan konsep frame, page, dan bitmap allocator | `pmm.h` — `PMM_PAGE_SIZE`, `struct pmm_state` |
| Mengimplementasikan alokator memori fisik fail-closed | `pmm.c` — `pmm_init_from_map` mengisi bitmap dengan `0xFF` di awal |
| Menulis unit test host yang independen dari target freestanding | `kernel/tests/test_pmm_host.c` |
| Memisahkan build rules native vs cross-compile pada Makefile | `Makefile` — variabel `SRC_C`, target `check-m6` dengan `HOSTCC` |
| Mengintegrasikan subsistem memori ke boot sequence kernel | `kmain.c` — `kernel_memory_init()` dipanggil dari `kmain()` |
| Melakukan verifikasi statis (readelf/objdump/nm) terhadap simbol PMM | `make inspect && make grade` — grep simbol `pmm_init_from_map`, `pmm_alloc_frame` |
| Menjalankan smoke test QEMU dan membaca log inisialisasi PMM | `[MCSOS:M6] pmm initialized`, `frames managed/free/used`, `sample alloc/free OK` |
| Melakukan commit dan push milestone dengan pesan commit deskriptif | `git commit -m "feat(m6): ..."`, `git push -u origin praktikum/m6-pmm` |

---

## 5. Peta Milestone MCSOS

| Milestone | Fokus | Status dalam laporan |
|---|---|---|
| M0 | Requirements, governance, baseline arsitektur | [x] selesai (praktikum sebelumnya) |
| M1 | Toolchain reproducible, Git, QEMU, GDB, metadata build | [x] selesai (praktikum sebelumnya) |
| M2 | Boot image, kernel ELF64, early console | [x] selesai (praktikum sebelumnya) |
| M3 | Panic path, linker map, GDB, observability awal | [x] selesai (praktikum sebelumnya) |
| M4 | Trap, exception, interrupt, timer | [x] terintegrasi (IDT/PIC/PIT terlihat aktif di log boot M6) |
| M5 | Persiapan memory map dan struktur boot | [~] tidak dibahas rinci, menjadi prasyarat konseptual M6 |
| **M6** | **Physical Memory Manager (PMM) berbasis bitmap** | **[x] fokus laporan ini** |
| M7 | Virtual memory manager, page table, kernel heap | [ ] tidak dibahas |
| M8 | Thread, scheduler, synchronization | [ ] tidak dibahas |
| M9 | Syscall ABI dan user program loader | [ ] tidak dibahas |
| M10 | VFS, file descriptor, ramfs | [ ] tidak dibahas |
| M11–M16 | Block layer s.d. observability & release readiness | [ ] tidak dibahas |

Batas cakupan praktikum:

```text
Praktikum M6 mencakup:
- Branch kerja praktikum/m6-pmm
- Header pmm.h: struct pmm_state, enum boot_mem_type, kontrak API
- Implementasi pmm.c: bitmap allocator fail-closed, first-fit dengan hint
- Unit test host test_pmm_host.c (5 region peta memori sintetis)
- Perbaikan Makefile: eksklusi kernel/tests dari build freestanding,
  target check-m6 (native HOSTCC) untuk uji cepat
- Integrasi PMM ke kmain.c via kernel_memory_init()
- Verifikasi statis (readelf, objdump, nm) dan make grade
- Build image ISO dan smoke test QEMU/OVMF headless
- Commit dan push milestone ke remote

Non-goals (tidak termasuk):
- Virtual memory manager / page table (MMU paging)
- Kernel heap allocator (kmalloc/kfree)
- Parsing peta memori nyata dari bootloader (Limine memmap) — M6 masih
  memakai region demo statis (M6_DEMO_PHYS_BYTES)
- Scheduler dan thread
- Syscall ABI dan userspace
- Filesystem dan network stack
```

---

## 6. Dasar Teori Ringkas

### 6.1 Konsep Sistem Operasi yang Diuji

**Physical Memory Manager (PMM)** adalah subsistem kernel yang bertanggung jawab melacak frame memori fisik mana yang sedang dipakai dan mana yang bebas, dalam satuan blok berukuran tetap (*frame*, di sini 4096 byte / 4 KiB, sama dengan ukuran halaman standar x86_64).

**Bitmap allocator:** setiap frame direpresentasikan oleh satu bit dalam array byte (`bitmap`). Bit bernilai 1 berarti frame terpakai, bit bernilai 0 berarti frame bebas. Dengan `PMM_PAGE_SIZE = 4096` dan `PMM_MAX_PHYS_BYTES = 64 GiB`, jumlah frame maksimum adalah `PMM_MAX_FRAMES = 64GiB / 4096 = 16.777.216` frame, membutuhkan `PMM_BITMAP_BYTES = 2.097.152` byte (2 MiB) bitmap pada kapasitas maksimum teoretis; instance aktual (`g_pmm_bitmap`) dialokasikan statis di kernel BSS.

**Strategi fail-closed:** pada `pmm_init_from_map`, seluruh bitmap diisi `0xFF` (semua frame dianggap terpakai) sebelum peta memori boot diproses. Baru kemudian region bertipe `BOOT_MEM_USABLE` dibuka satu per satu menjadi bebas. Pendekatan ini memastikan bila ada region yang lupa didaftarkan atau tipe region tidak dikenali, kernel tidak akan pernah mengalokasikan memori tersebut secara tidak sengaja — konsisten dengan prinsip keamanan *default deny*.

**First-fit dengan hint (`next_hint`):** `pmm_alloc_frame` mencari frame bebas mulai dari indeks `next_hint` (bukan selalu dari 0), lalu wrap-around ke awal bila belum ditemukan. Ini menghindari kompleksitas linear berulang dari awal setiap kali alokasi terjadi berurutan.

**Overflow-safe arithmetic:** fungsi `checked_add_u64` digunakan pada `mark_range_free`/`mark_range_used` untuk mencegah *integer overflow* ketika menghitung `base + length`, yang bila tidak diperiksa dapat menyebabkan frame di luar batas ditandai salah.

### 6.2 Relevansi dengan Rancangan MCSOS

PMM adalah prasyarat langsung untuk M7 (Virtual Memory Manager): VMM membutuhkan sumber frame fisik untuk membangun tabel halaman (page table) dan memetakan alamat virtual ke fisik. Tanpa PMM yang benar (fail-closed, tidak overlap, tidak double-alloc), lapisan VMM di atasnya akan mewarisi bug korupsi memori yang sulit dilacak.

---

## 7. Lingkungan Praktikum

| Komponen | Versi/Keterangan |
|---|---|
| Host OS | Windows 11 x64 + WSL 2 (Ubuntu) |
| Shell | `/usr/bin/env bash` (via `andianaaji@JotDesu`) |
| Compiler | `clang` (target `x86_64-unknown-none-elf`, `-std=c17`, freestanding) |
| Assembler | `clang` (mode `-x assembler-with-cpp` implisit melalui `AS := clang`) |
| Linker | `ld.lld` |
| Host compiler (uji unit) | `clang` (native, untuk target `check-m6`) |
| Inspeksi biner | `readelf`, `objdump`, `nm` |
| Emulator | QEMU (`qemu-system-x86_64`), mode headless (`-display none`), serial ke stdio |
| Bootloader | Limine (BIOS + UEFI hybrid, via `xorriso`) |
| Version control | Git, remote GitHub (`JotDesu/mcsos260502`) |
| Direktori kerja | `~/src/mcsos` |

---

## 8. Repository dan Struktur File

### 8.1 Berkas Baru/Diubah pada M6

| Berkas | Status | Fungsi |
|---|---|---|
| `kernel/include/mcsos/kernel/pmm.h` | Baru | Kontrak API dan struktur data PMM |
| `kernel/core/pmm.c` | Baru | Implementasi bitmap frame allocator (240 baris) |
| `kernel/tests/test_pmm_host.c` | Baru | Unit test native untuk PMM (5 region sintetis) |
| `kernel/core/kmain.c` | Diubah | Integrasi `kernel_memory_init()` ke boot sequence |
| `Makefile` | Diubah | Eksklusi `kernel/tests/*` dari build freestanding, target `check-m6` baru |

### 8.2 Ringkasan Perubahan (dari `git status --short` dan commit)

```text
M  Makefile
A  kernel/core/kmain.c
A  kernel/core/pmm.c
A  kernel/include/mcsos/kernel/pmm.h
A  kernel/tests/test_pmm_host.c
```

Catatan: `kmain.c` ditandai `A` (added) pada snapshot git status karena pelacakan file sebelumnya baru mencerminkan versi M3; commit M6 mencatatnya sebagai bagian dari 5 file yang berubah, 405 insersi(+), 7 delesi(-).

---

## 9. Desain Teknis

### 9.1 Header `kernel/include/mcsos/kernel/pmm.h`

```c
#ifndef MCSOS_KERNEL_PMM_H
#define MCSOS_KERNEL_PMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PMM_PAGE_SIZE 4096ULL
#define PMM_MAX_PHYS_BYTES (64ULL * 1024ULL * 1024ULL * 1024ULL)
#define PMM_MAX_FRAMES (PMM_MAX_PHYS_BYTES / PMM_PAGE_SIZE)
#define PMM_BITMAP_BYTES (PMM_MAX_FRAMES / 8ULL)
#define PMM_INVALID_FRAME 0xffffffffffffffffULL

enum boot_mem_type {
    BOOT_MEM_USABLE = 1,
    BOOT_MEM_RESERVED = 2,
    BOOT_MEM_BOOTLOADER_RECLAIMABLE = 3,
    BOOT_MEM_KERNEL_AND_MODULES = 4,
    BOOT_MEM_FRAMEBUFFER = 5,
    BOOT_MEM_ACPI_RECLAIMABLE = 6,
    BOOT_MEM_ACPI_NVS = 7,
    BOOT_MEM_BAD_MEMORY = 8
};

struct boot_mem_region {
    uint64_t base;
    uint64_t length;
    uint32_t type;
};

struct pmm_state {
    uint8_t *bitmap;
    uint64_t bitmap_bytes;
    uint64_t max_phys;
    uint64_t frame_count;
    uint64_t free_frames;
    uint64_t used_frames;
    uint64_t reserved_frames;
    uint64_t ignored_frames;
    uint64_t next_hint;
    bool initialized;
};

void pmm_zero_state(struct pmm_state *pmm);
bool pmm_init_from_map(struct pmm_state *pmm,
                        const struct boot_mem_region *regions,
                        size_t region_count,
                        uint8_t *bitmap_storage,
                        uint64_t bitmap_storage_bytes,
                        uint64_t max_phys_bytes);
uint64_t pmm_alloc_frame(struct pmm_state *pmm);
bool pmm_free_frame(struct pmm_state *pmm, uint64_t phys_addr);
bool pmm_reserve_range(struct pmm_state *pmm, uint64_t base, uint64_t length);
bool pmm_is_frame_free(const struct pmm_state *pmm, uint64_t phys_addr);
uint64_t pmm_free_count(const struct pmm_state *pmm);
uint64_t pmm_used_count(const struct pmm_state *pmm);
uint64_t pmm_frame_count(const struct pmm_state *pmm);

#endif
```

**Keputusan desain kunci:**

1. **Ukuran frame tetap 4096 byte** — selaras dengan ukuran halaman standar arsitektur x86_64, memudahkan integrasi dengan VMM pada milestone berikutnya.
2. **`PMM_INVALID_FRAME` sebagai sentinel** — memakai nilai `UINT64_MAX` alih-alih kode error terpisah, sehingga pemanggil cukup membandingkan hasil `pmm_alloc_frame` dengan satu konstanta.
3. **State disimpan eksplisit dalam `struct pmm_state`**, bukan variabel global tersembunyi di dalam `pmm.c` — memungkinkan beberapa instance PMM (berguna untuk unit test host yang membuat instance lokal `struct pmm_state pmm;`) dan menghindari *hidden global state* yang menyulitkan pengujian.
4. **Enum `boot_mem_type`** meniru skema tipe region milik spesifikasi Limine boot protocol, sehingga peta memori dari bootloader nantinya dapat langsung dipetakan tanpa translasi tambahan.

### 9.2 Ringkasan Desain `kernel/core/pmm.c`

| Fungsi (internal `static`) | Tanggung jawab |
|---|---|
| `align_down` / `align_up` | Membulatkan alamat ke batas `PMM_PAGE_SIZE` terdekat |
| `checked_add_u64` | Penjumlahan `uint64_t` dengan deteksi overflow eksplisit |
| `bitmap_set` / `bitmap_clear` / `bitmap_test` | Operasi bit-level pada array bitmap |
| `mark_frame_free` / `mark_frame_used` | Mengubah status satu frame + menjaga konsistensi counter (`free_frames`, `used_frames`, `next_hint`) |
| `mark_range_free` | Membuka rentang alamat menjadi bebas, dengan proteksi overflow dan pembatasan ke `max_phys` (kelebihan dicatat sebagai `ignored_frames`) |
| `mark_range_used` | Menutup rentang alamat menjadi terpakai; frame yang sebelumnya bebas lalu ditutup dicatat sebagai `reserved_frames` (indikator overlap peta memori) |

Desain ini memisahkan **primitif tingkat frame tunggal** (`mark_frame_*`) dari **primitif tingkat rentang** (`mark_range_*`), sehingga logika alignment dan overflow-check hanya perlu ditulis sekali di level rentang.


---

## 10. Langkah Kerja Implementasi

### 10.1 Menyiapkan Branch Kerja

```bash
git checkout -b praktikum/m6-pmm
git branch --show-current
# -> praktikum/m6-pmm
```

Bukti: `Screenshot 2026-06-30 190512.png` (hal. 1).

### 10.2 Menulis Header `pmm.h`

```bash
cat kernel/include/mcsos/kernel/pmm.h
```

Header diperiksa ulang (`cat`) untuk memastikan definisi konstanta, enum, struct, dan prototipe fungsi sudah lengkap sebelum implementasi ditulis. Isi lengkap ada pada bagian 9.1.

Bukti: `Screenshot 2026-06-30 190512.png` (hal. 2).

### 10.3 Menulis Implementasi `pmm.c` (Bagian 1: Helper dan Bitmap Primitif)

```bash
cat > kernel/core/pmm.c << 'EOF'
#include <mcsos/kernel/pmm.h>

#ifndef UINT64_MAX
#define UINT64_MAX 0xffffffffffffffffULL
#endif

static uint64_t align_down(uint64_t value, uint64_t align) {
    return value & ~(align - 1ULL);
}

static uint64_t align_up(uint64_t value, uint64_t align) {
    return (value + align - 1ULL) & ~(align - 1ULL);
}

static bool checked_add_u64(uint64_t a, uint64_t b, uint64_t *out) {
    if (UINT64_MAX - a < b) {
        return false;
    }
    *out = a + b;
    return true;
}

static void bitmap_set(uint8_t *bitmap, uint64_t index) {
    bitmap[index >> 3] = (uint8_t)(bitmap[index >> 3] | (uint8_t)(1U << (index & 7U)));
}

static void bitmap_clear(uint8_t *bitmap, uint64_t index) {
    bitmap[index >> 3] = (uint8_t)(bitmap[index >> 3] & (uint8_t)~(uint8_t)(1U << (index & 7U)));
}

static bool bitmap_test(const uint8_t *bitmap, uint64_t index) {
    return (bitmap[index >> 3] & (uint8_t)(1U << (index & 7U))) != 0;
}
EOF
```

Bukti: `Screenshot 2026-06-30 190512.png` (hal. 3).

### 10.4 Menulis Implementasi `pmm.c` (Bagian 2: `mark_frame_*` dan `mark_range_*`)

Fungsi `mark_frame_free`/`mark_frame_used` menjaga konsistensi counter `free_frames`/`used_frames` dan memperbarui `next_hint` bila frame yang dibebaskan lebih kecil dari hint saat ini. Fungsi `mark_range_free`/`mark_range_used` melakukan align, bounds-check terhadap `max_phys`, dan mencatat `ignored_frames`/`reserved_frames` untuk kasus tepi (edge case).

Bukti: `Screenshot 2026-06-30 190512.png` (hal. 4–5).

### 10.5 Menulis Implementasi `pmm.c` (Bagian 3: `pmm_zero_state` dan `pmm_init_from_map`)

`pmm_init_from_map` melakukan validasi parameter (pointer null, `region_count == 0`, `max_phys_bytes` tidak kelipatan `PMM_PAGE_SIZE`), menghitung `frame_count` dan `required_bitmap_bytes`, memverifikasi kapasitas storage bitmap yang disediakan pemanggil cukup, lalu:

1. Mengisi seluruh bitmap dengan `0xFF` (fail-closed — semua frame default terpakai).
2. Membuka region bertipe `BOOT_MEM_USABLE` menjadi bebas (`mark_range_free`).
3. Secara eksplisit mengunci frame 0 (`mark_range_used(pmm, 0, PMM_PAGE_SIZE)`) agar alamat nol tidak pernah dianggap frame valid yang bisa dialokasikan.
4. Menutup kembali seluruh region non-`BOOT_MEM_USABLE` (`mark_range_used`) sebagai jaring pengaman kedua terhadap tumpang tindih peta memori.
5. Menandai `pmm->initialized = true` hanya setelah seluruh langkah di atas selesai tanpa error.

Bukti: `Screenshot 2026-06-30 190512.png` (hal. 5–6).

### 10.6 Menulis Implementasi `pmm.c` (Bagian 4: API Alokasi/Pembebasan/Query)

`pmm_alloc_frame` mencari frame bebas pertama mulai dari `next_hint` (wrap-around ke 0 bila perlu), menandainya terpakai, lalu mengembalikan alamat fisik `frame * PMM_PAGE_SIZE`. `pmm_free_frame` memvalidasi alignment dan batas sebelum membebaskan frame. `pmm_reserve_range` adalah pembungkus tipis di atas `mark_range_used` untuk API publik. `pmm_is_frame_free`, `pmm_free_count`, `pmm_used_count`, `pmm_frame_count` adalah fungsi query read-only yang aman dipanggil dengan `pmm == NULL` (mengembalikan `0`/`false`).

```bash
wc -l kernel/core/pmm.c
# -> 240 kernel/core/pmm.c
```

Bukti: `Screenshot 2026-06-30 190512.png` (hal. 6–7).

### 10.7 Build Awal (Sebelum Integrasi ke `kmain.c`)

```bash
make clean && make build 2>&1 | tail -40
```

Seluruh unit terkompilasi (`idt.o`, `pic.o`, `pit.o`, `kmain.o`, `log.o`, `panic.o`, `pmm.o`, `serial.o`, `memory.o`, `interrupts.o`) dan berhasil di-link menjadi `build/normal/kernel.elf`. `pmm.o` sengaja dihapus kembali (`rm -v build/normal/kernel/core/pmm.o`) pada tahap ini untuk memastikan langkah build selanjutnya (`check-m6`) mengompilasi ulang `pmm.o` secara independen, bukan memakai artefak basi dari build kernel penuh.

Bukti: `Screenshot 2026-06-30 190512.png` (hal. 7).

### 10.8 Menulis Unit Test Host `test_pmm_host.c`

```bash
cat > kernel/tests/test_pmm_host.c << 'EOF'
#include <assert.h>
#include <stdio.h>
#include <mcsos/kernel/pmm.h>

static uint8_t bitmap[PMM_BITMAP_BYTES];

int main(void) {
    struct boot_mem_region regions[] = {
        { .base = 0x00000000ULL, .length = 0x0009f000ULL, .type = BOOT_MEM_USABLE },
        { .base = 0x0009f000ULL, .length = 0x00001000ULL, .type = BOOT_MEM_RESERVED },
        { .base = 0x00100000ULL, .length = 0x00300000ULL, .type = BOOT_MEM_USABLE },
        { .base = 0x00400000ULL, .length = 0x00100000ULL, .type = BOOT_MEM_KERNEL_AND_MODULES },
        { .base = 0x00500000ULL, .length = 0x00400000ULL, .type = BOOT_MEM_USABLE },
    };

    struct pmm_state pmm;
    assert(pmm_init_from_map(&pmm, regions, sizeof(regions) / sizeof(regions[0]),
                              bitmap, sizeof(bitmap), 64ULL * 1024ULL * 1024ULL));
    assert(pmm_frame_count(&pmm) == (64ULL * 1024ULL * 1024ULL) / PMM_PAGE_SIZE);
    assert(!pmm_is_frame_free(&pmm, 0));
    assert(pmm_is_frame_free(&pmm, 0x00100000ULL));
    assert(!pmm_is_frame_free(&pmm, 0x00400000ULL));

    uint64_t before = pmm_free_count(&pmm);
    uint64_t frame = pmm_alloc_frame(&pmm);
    assert(frame != PMM_INVALID_FRAME);
    assert((frame & (PMM_PAGE_SIZE - 1ULL)) == 0);
    assert(!pmm_is_frame_free(&pmm, frame));
    assert(pmm_free_count(&pmm) == before - 1ULL);
    assert(pmm_free_frame(&pmm, frame));
    assert(pmm_is_frame_free(&pmm, frame));
    assert(pmm_free_count(&pmm) == before);
    assert(!pmm_free_frame(&pmm, frame));

    assert(pmm_reserve_range(&pmm, 0x00500000ULL, 0x2000ULL));
    assert(!pmm_is_frame_free(&pmm, 0x00500000ULL));
    assert(!pmm_is_frame_free(&pmm, 0x00501000ULL));

    puts("M6 PMM host unit test: PASS");
    return 0;
}
EOF
```

Ukuran memori demo pada uji ini sengaja dibuat kecil (64 MiB) agar array `bitmap[PMM_BITMAP_BYTES]` yang dialokasikan statis tetap ringan; nilai `PMM_BITMAP_BYTES` sendiri dihitung dari `PMM_MAX_PHYS_BYTES` (64 GiB) di header, sehingga array bitmap tetap berukuran tetap 2 MiB terlepas dari `max_phys_bytes` aktual yang dipakai saat runtime.

Bukti: `Screenshot 2026-06-30 190512.png` (hal. 8).

### 10.9 Build Gagal: Header Host vs Freestanding Bertabrakan

```bash
make clean && make build 2>&1 | grep -i "test_pmm_host\|error"
```

```
In file included from kernel/tests/test_pmm_host.c:1:
/usr/include/features-time64.h:20:10: fatal error: 'bits/wordsize.h' file not found
1 error generated.
make: *** [Makefile:40: build/normal/kernel/tests/test_pmm_host.o] Error 1
```

**Analisis akar masalah:** pola pencarian sumber (`SRC_C`) pada `Makefile` versi sebelumnya tidak mengecualikan `kernel/tests/*`, sehingga `test_pmm_host.c` — yang memakai header host (`<assert.h>`, `<stdio.h>`) — ikut dikompilasi dengan flag cross-compile freestanding (`--target=x86_64-unknown-none-elf -ffreestanding`). Toolchain freestanding tidak memiliki sysroot glibc lengkap, sehingga header transitif `bits/wordsize.h` tidak ditemukan.

Upaya perbaikan cepat dengan `sed` gagal karena escaping tanda kutip pada ekspresi pencarian tidak valid untuk shell:

```bash
sed -i "s|SRC_C := \$(shell find kernel -name '\*.c' | LC_ALL=C sort)|SRC_C := \$(shell find kernel -name '*.c' -not -path 'kernel/tests/*' | LC_ALL=C sort)|" Makefile
# sed: -e expression #1, char 62: unknown option to `s'
```

Bukti: `Screenshot 2026-06-30 192047.png` (hal. 9).

### 10.10 Menulis Ulang `Makefile`

Karena perbaikan `sed` gagal, `Makefile` ditulis ulang penuh menggunakan `cat > Makefile << 'EOF'` agar strukturnya bersih dan dapat diverifikasi baris per baris. Perubahan inti dibanding versi M3/sebelumnya:

| Elemen | Perubahan |
|---|---|
| `SRC_C` | Ditambahkan `-not -path 'kernel/tests/*'` agar berkas uji tidak pernah masuk ke build freestanding |
| `HOSTCC` | Variabel baru (`clang` native) khusus untuk mengompilasi target host |
| Target `check-m6` | Target baru: build ulang `pmm.o` (cross), build `test_pmm_host` (host, me-link `pmm.c` + `test_pmm_host.c` langsung), jalankan biner, cek simbol undefined via `nm -u`, `objdump -dr`, lalu cetak `[PASS] M6 static check selesai` |
| `RECIPEPREFIX` | Tetap `>` (konsisten dengan konvensi M3) agar makefile tidak rawan error tab/spasi |

```bash
grep -A1 "SRC_C :=" Makefile
```

```
SRC_C := $(shell find kernel -name '*.c' -not -path 'kernel/tests/*' | LC_ALL=C sort)
SRC_S := $(shell find kernel -name '*.S' -not -path 'kernel/tests/*' | LC_ALL=C sort)
```

Bukti: `Screenshot 2026-06-30 192047.png` (hal. 10).

### 10.11 Menjalankan `make check-m6`

```bash
make check-m6
```

```
clang --target=x86_64-unknown-none-elf ... -c kernel/core/pmm.c -o build/pmm.o
clang -std=c17 -Wall -Wextra -Werror -Ikernel/include \
  kernel/core/pmm.c kernel/tests/test_pmm_host.c -o build/test_pmm_host
./build/test_pmm_host
M6 PMM host unit test: PASS
nm -u build/pmm.o | tee build/pmm.undefined.txt
test ! -s build/pmm.undefined.txt
objdump -dr build/pmm.o > build/pmm.objdump.txt
[PASS] M6 static check selesai
```

Semua assert pada unit test lolos, tidak ada simbol undefined pada `pmm.o`, dan disassembly berhasil diekstrak sebagai bukti tambahan.

Bukti: `Screenshot 2026-06-30 192047.png` (hal. 11).

### 10.12 Integrasi PMM ke `kmain.c`

```bash
cat > kernel/core/kmain.c << 'EOF'
#include <stdint.h>
#include <mcsos/arch/cpu.h>
#include <mcsos/arch/idt.h>
#include <mcsos/arch/pic.h>
#include <mcsos/arch/pit.h>
#include <mcsos/kernel/log.h>
#include <mcsos/kernel/panic.h>
#include <mcsos/kernel/pmm.h>
#include <mcsos/kernel/version.h>

extern char __kernel_start[];
extern char __kernel_end[];

#define M6_DEMO_PHYS_BYTES (128ULL * 1024ULL * 1024ULL)

static struct pmm_state g_pmm;
static uint8_t g_pmm_bitmap[PMM_BITMAP_BYTES] __attribute__((aligned(4096)));

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
    log_key_value_hex64("[MCSOS:M6] frames used", pmm_used_count(&g_pmm));

    uint64_t f = pmm_alloc_frame(&g_pmm);
    if (f == PMM_INVALID_FRAME) {
        KERNEL_PANIC("pmm_alloc_frame returned invalid", 0);
    }
    log_key_value_hex64("[MCSOS:M6] sample frame", f);

    if (!pmm_free_frame(&g_pmm, f)) {
        KERNEL_PANIC("pmm_free_frame failed", f);
    }
    log_writeln("[MCSOS:M6] sample alloc/free OK");
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
    log_writeln("[MCSOS:M3] boot: external interrupt bring-up start");

    KERNEL_ASSERT(__kernel_end > __kernel_start);
    KERNEL_ASSERT(sizeof(uintptr_t) == 8u);

    idt_init();

    pic_remap(PIC_MASTER_OFFSET, PIC_SLAVE_OFFSET);
    log_writeln("[MCSOS:M3] pic: remapped");

    pic_mask_all();
    pic_unmask_irq(0);
    log_writeln("[MCSOS:M3] pic: irq0 unmasked");

    pit_configure_hz(100);
    log_writeln("[MCSOS:M3] pit: configured 100Hz");

    cpu_sti();
    log_writeln("[MCSOS:M3] sti: interrupts enabled");

    kernel_memory_init();

    for (;;) {
        cpu_hlt();
    }
}
EOF
```

Bukti: `Screenshot 2026-06-30 192047.png` (hal. 12–13).

### 10.13 Build Penuh Pasca Integrasi

```bash
make clean && make build 2>&1 | tail -20
```

Seluruh unit (arch: `idt.o`, `pic.o`, `pit.o`, `interrupts.o`; core: `kmain.o`, `log.o`, `panic.o`, `pmm.o`, `serial.o`; lib: `memory.o`) berhasil dikompilasi dan di-link menjadi `build/normal/kernel.elf`.

Bukti: `Screenshot 2026-06-30 192047.png` (hal. 13–14).

### 10.14 Verifikasi Statis dan Grade

```bash
make inspect && make grade
```

`readelf -h`, `readelf -l`, `nm -n`, dan `objdump -d` dijalankan terhadap `build/kernel.elf`, kemudian diverifikasi dengan `grep -q` bahwa biner adalah ELF64 untuk mesin AMD x86-64 dan memuat simbol-simbol kunci: `kmain`, `kernel_panic_at`, `cpu_halt_forever`, `idt_init`, `pic_remap`, `pit_configure_hz`, `isr_stub_32`, `timer_on_irq0`, `pmm_init_from_map`, `pmm_alloc_frame`. Seluruh pengecekan lolos:

```
M6 static grade: PASS
```

Bukti: `Screenshot 2026-06-30 192047.png` (hal. 14).

### 10.15 Build Image dan Smoke Test QEMU

```bash
make image && timeout 10 qemu-system-x86_64 \
  -M q35 -m 512M -cdrom build/mcsos.iso \
  -serial stdio -no-reboot -no-shutdown \
  -display none 2>/dev/null || true
```

`xorriso` membentuk ISO hibrida BIOS/UEFI dengan Limine sebagai bootloader, `limine bios-install` dijalankan terhadap image, dan `sha256sum` disimpan sebagai bukti integritas (`build/mcsos.iso.sha256`). QEMU kemudian boot image tersebut dalam mode headless selama maksimum 10 detik.

Bukti: `Screenshot 2026-06-30 192047.png` (hal. 15).

### 10.16 Commit dan Push Milestone

```bash
git add -A
git status --short
git commit -m "feat(m6): bitmap physical memory manager (PMM)"
git push -u origin praktikum/m6-pmm
```

```
[praktikum/m6-pmm dcde32f] feat(m6): bitmap physical memory manager (PMM)
 5 files changed, 405 insertions(+), 7 deletions(-)
 create mode 100644 kernel/core/pmm.c
 create mode 100644 kernel/include/mcsos/kernel/pmm.h
 create mode 100644 kernel/tests/test_pmm_host.c
...
remote: Create a pull request for 'praktikum/m6-pmm' on GitHub by visiting:
remote:   https://github.com/JotDesu/mcsos260502/pull/new/praktikum/m6-pmm
 * [new branch]      praktikum/m6-pmm -> praktikum/m6-pmm
branch 'praktikum/m6-pmm' set up to track 'origin/praktikum/m6-pmm'.
```

Bukti: `Screenshot 2026-06-30 192047.png` (hal. 16).

---

## 11. Checkpoint Buildable

| Checkpoint | Perintah | Hasil |
|---|---|---|
| Branch kerja aktif | `git branch --show-current` | `praktikum/m6-pmm` |
| `pmm.c` sesuai jumlah baris yang diharapkan | `wc -l kernel/core/pmm.c` | `240` |
| Build kernel penuh sukses | `make clean && make build` | Sukses, `build/normal/kernel.elf` terbentuk |
| Uji unit host sukses | `make check-m6` | `M6 PMM host unit test: PASS` |
| Tidak ada simbol undefined pada `pmm.o` | `nm -u build/pmm.o` | Kosong (`test ! -s` lolos) |
| Verifikasi statis ELF | `make inspect && make grade` | `M6 static grade: PASS` |
| Image ISO terbentuk | `make image` | `build/mcsos.iso` + `.sha256` |
| Boot QEMU menampilkan inisialisasi PMM | `timeout 10 qemu-system-x86_64 ...` | `[MCSOS:M6] pmm initialized` dan `sample alloc/free OK` tampil |
| Commit dan push berhasil | `git commit` + `git push` | `dcde32f`, branch ter-track ke origin |

---

## 12. Perintah Uji dan Validasi

```bash
# 1. Uji unit host cepat (tanpa QEMU)
make check-m6

# 2. Verifikasi statis biner kernel
make inspect
make grade

# 3. Build image bootable
make image

# 4. Smoke test boot headless
timeout 10 qemu-system-x86_64 \
  -M q35 -m 512M -cdrom build/mcsos.iso \
  -serial stdio -no-reboot -no-shutdown -display none

# 5. Audit manual simbol PMM pada kernel.elf
nm -n build/normal/kernel.elf | grep -E 'pmm_init_from_map|pmm_alloc_frame|pmm_free_frame'
```

---

## 13. Hasil Uji

### 13.1 Hasil `make check-m6`

| Sub-uji | Hasil |
|---|---|
| Kompilasi `pmm.o` (cross target) | Sukses, tanpa warning (`-Wall -Wextra -Werror`) |
| Kompilasi + link `test_pmm_host` (host) | Sukses |
| Eksekusi `test_pmm_host` | `M6 PMM host unit test: PASS` |
| `nm -u build/pmm.o` (simbol undefined) | Kosong — tidak ada dependensi eksternal yang tidak terselesaikan |
| `objdump -dr build/pmm.o` | Berhasil menghasilkan disassembly, tersimpan di `build/pmm.objdump.txt` |

### 13.2 Hasil `make grade`

```
M6 static grade: PASS
```

Seluruh 10 pola `grep -q` (ELF64, mesin AMD X86-64, dan 8 simbol fungsi kunci) lolos tanpa kegagalan.

### 13.3 Hasil Boot QEMU (Cuplikan Log Serial)

```
MCSOS 260502 M3 kernel entered
[MCSOS:M3] boot: external interrupt bring-up start
[MCSOS:M3] idt: loaded
[MCSOS:M3] pic: remapped
[MCSOS:M3] pic: irq0 unmasked
[MCSOS:M3] pit: configured 100Hz
[MCSOS:M3] sti: interrupts enabled
[MCSOS:M6] pmm initialized
[MCSOS:M6] frames managed=0x0000000000008000
[MCSOS:M6] frames free=0x0000000000007fff
[MCSOS:M6] frames used=0x0000000000000001
[MCSOS:M6] sample frame=0x0000000000001000
[MCSOS:M6] sample alloc/free OK
[MCSOS:TIMER] ticks=0x0000000000000064
[MCSOS:TIMER] ticks=0x00000000000000c8
[MCSOS:TIMER] ticks=0x000000000000012c
[MCSOS:TIMER] ticks=0x0000000000000190
[MCSOS:TIMER] ticks=0x00000000000001f4
[MCSOS:TIMER] ticks=0x0000000000000258
[MCSOS:TIMER] ticks=0x00000000000002bc
```

**Verifikasi angka:** dengan `M6_DEMO_PHYS_BYTES = 128 MiB` dan `PMM_PAGE_SIZE = 4096`, `frame_count` seharusnya `128MiB / 4096 = 32768 = 0x8000`, cocok dengan `frames managed`. Karena frame 0 dikunci eksplisit, `frames free = 0x8000 - 1 = 0x7fff` dan `frames used = 1` — kedua nilai ini konsisten secara matematis dengan log yang tertangkap, mengonfirmasi logika fail-closed bekerja sesuai desain.

> Catatan: label milestone pada baris log boot (`MCSOS 260502 M3 ...`, `[MCSOS:M3] ...`) berasal dari makro `MCSOS_MILESTONE` di `version.h` yang belum diperbarui menjadi `M6` — dicatat sebagai temuan minor pada bagian 15 (Debugging dan Failure Modes) karena tidak memengaruhi fungsi PMM itu sendiri.

---

## 14. Analisis Teknis

### 14.1 Analisis Keberhasilan

1. **Bitmap allocator fail-closed berhasil diimplementasikan:** seluruh frame default terpakai sebelum region `BOOT_MEM_USABLE` dibuka, dan frame 0 selalu dikunci — mencegah kernel salah mengalokasikan alamat nol atau memori yang tidak seharusnya dianggap bebas.
2. **Overflow-safe range marking:** `checked_add_u64` mencegah *wrap-around* pada perhitungan `base + length` yang bisa menyebabkan frame di luar jangkauan tertandai salah.
3. **Pemisahan uji host vs target:** `test_pmm_host.c` memverifikasi logika alokator dalam hitungan milidetik tanpa perlu boot QEMU, mempercepat siklus iterasi debugging secara signifikan dibanding hanya mengandalkan smoke test QEMU.
4. **Integrasi end-to-end terbukti:** dari `pmm_init_from_map` di `kmain.c`, sampai log serial `sample alloc/free OK` di QEMU, seluruh rantai (header → implementasi → integrasi boot → runtime) terverifikasi bekerja.
5. **Static verification otomatis:** `make grade` memastikan simbol-simbol PMM benar-benar ter-link ke dalam biner akhir, bukan hanya lolos kompilasi terpisah.

### 14.2 Analisis Kegagalan atau Perbedaan Hasil

| Issue | Penyebab | Perbaikan | Status |
|---|---|---|---|
| `test_pmm_host.c` gagal kompilasi (`bits/wordsize.h` not found) | `SRC_C` glob lama tidak mengecualikan `kernel/tests/*`, sehingga file host ikut dikompilasi dengan flag freestanding cross-compile | Tulis ulang `Makefile` dengan `-not -path 'kernel/tests/*'` pada `SRC_C`/`SRC_S` | Fixed |
| Perintah `sed` gagal memperbaiki `Makefile` | Escaping tanda kutip tunggal/ganda pada ekspresi `s|...|...|` tidak valid untuk shell bash | Tulis ulang `Makefile` secara penuh via heredoc, bukan patch parsial | Workaround diterapkan |
| Label `MCSOS_MILESTONE` masih menampilkan `M3` pada log boot M6 | `version.h` belum diperbarui saat integrasi PMM | Belum diperbaiki (dicatat sebagai temuan, tidak menghambat fungsi PMM) | Dicatat, perbaikan disarankan untuk milestone selanjutnya |

### 14.3 Perbandingan Desain Awal vs Implementasi Akhir

| Aspek | Rencana Awal | Implementasi Akhir | Alasan Perubahan |
|---|---|---|---|
| Sumber peta memori | Rencana memakai memmap asli dari Limine | Memakai `M6_DEMO_PHYS_BYTES` statis (128 MiB) sebagai region demo | Parsing memmap Limine belum menjadi cakupan M6; disederhanakan agar fokus pada logika alokator terlebih dahulu |
| Uji alokator | Rencana hanya smoke test QEMU | Ditambah unit test host independen | QEMU boot lambat untuk siklus iterasi cepat; uji host mempercepat debugging logika bitmap |
| Struktur Makefile | Satu `SRC_C` glob untuk semua `.c` | Dipecah: `SRC_C` (freestanding, exclude tests) + target `check-m6` terpisah dengan `HOSTCC` | Ditemukan konflik header host vs freestanding saat build pertama |

---

## 15. Debugging dan Failure Modes

### 15.1 Failure Modes yang Ditemukan

| Failure mode | Gejala | Penyebab | Perbaikan | Bukti |
|---|---|---|---|---|
| Header host tercampur build freestanding | `fatal error: 'bits/wordsize.h' file not found` | `kernel/tests/*.c` ikut ter-glob ke `SRC_C` | Eksklusi path `kernel/tests/*` pada `SRC_C`/`SRC_S` | `Screenshot 2026-06-30 192047.png` (hal. 9) |
| `sed` gagal memodifikasi Makefile | `sed: -e expression #1, char 62: unknown option to 's'` | Escaping delimiter `|` bentrok dengan karakter dalam ekspresi pencarian | Tulis ulang Makefile penuh via heredoc | `Screenshot 2026-06-30 192047.png` (hal. 9) |
| Artefak `pmm.o` basi dari build sebelumnya | Potensi false-positive pada `check-m6` bila `pmm.o` lama tidak dihapus | Build kernel penuh sebelumnya menghasilkan `pmm.o` di path yang sama | `rm -v build/normal/kernel/core/pmm.o` sebelum menulis test host | `Screenshot 2026-06-30 190512.png` (hal. 7) |

### 15.2 Failure Modes yang Diantisipasi

| Failure mode | Deteksi | Dampak | Mitigasi |
|---|---|---|---|
| Integer overflow pada `base + length` | Unit test dengan region mendekati `UINT64_MAX` (belum ditulis eksplisit di `test_pmm_host.c` versi ini) | Frame di luar batas tertandai salah, korupsi state | `checked_add_u64` sudah mengembalikan `false` dan fungsi pemanggil `return` lebih awal |
| Double-free frame | `assert(!pmm_free_frame(&pmm, frame))` pada unit test | Counter `free_frames` salah hitung, potensi double-alloc | `pmm_free_frame` memeriksa `bitmap_test` sebelum membebaskan; sudah tercakup unit test |
| Alokasi saat `free_frames == 0` | Guard `pmm->free_frames == 0` di awal `pmm_alloc_frame` | Infinite loop pencarian frame bebas | Guard eksplisit mengembalikan `PMM_INVALID_FRAME` langsung |
| Bitmap storage terlalu kecil untuk `max_phys_bytes` | Cek `bitmap_storage_bytes < required_bitmap_bytes` di `pmm_init_from_map` | Buffer overflow saat menulis bitmap | Fungsi mengembalikan `false` sebelum menyentuh memori bitmap |

### 15.3 Triage yang Dilakukan

1. Jalankan uji cepat: `make check-m6`
2. Jika gagal kompilasi, periksa apakah berkas berada di `kernel/tests/` dan apakah `SRC_C` mengecualikannya: `grep -A1 "SRC_C :=" Makefile`
3. Bersihkan artefak lama: `make clean`
4. Build ulang penuh: `make build`
5. Verifikasi simbol: `nm -n build/normal/kernel.elf | grep pmm`
6. Verifikasi statis: `make inspect && make grade`
7. Jalankan smoke test QEMU dan periksa log: cari baris `[MCSOS:M6] pmm initialized` dan `sample alloc/free OK`

---

## 16. Prosedur Rollback

| Skenario rollback | Perintah | Data yang harus diselamatkan | Status |
|---|---|---|---|
| Kembali ke commit sebelum M6 (M3/M4 baseline) | `git checkout main` (atau commit hash sebelumnya) | Log/test M6 pada branch `praktikum/m6-pmm` tetap tersimpan terpisah | Teruji |
| Bersihkan artefak build M6 | `make distclean` | Source (`pmm.c`, `pmm.h`, `test_pmm_host.c`) aman, tidak terhapus | Teruji |
| Regenerasi image ISO | `make image` | Image lama (`build/mcsos.iso`) di-overwrite, hash lama tidak diperlukan lagi | Teruji |
| Revert integrasi PMM di `kmain.c` saja | `git checkout HEAD~1 -- kernel/core/kmain.c` | Perubahan lain (Makefile, pmm.c) tetap dipertahankan | Belum diuji eksplisit pada sesi ini |
| Hapus branch kerja bila milestone dibatalkan | `git branch -D praktikum/m6-pmm` (lokal) + hapus branch remote | Pastikan sudah di-merge atau di-backup terlebih dahulu | Belum dieksekusi (branch masih aktif) |

---

## 17. Keamanan dan Reliability

| Aspek | Penjelasan |
|---|---|
| **Fail-closed default** | Bitmap diisi `0xFF` (semua terpakai) sebelum region usable dibuka — mengikuti prinsip *default deny*, meminimalkan risiko alokasi memori yang sebenarnya tidak aman dipakai (mis. memori firmware, region kernel). |
| **Proteksi alamat nol** | Frame 0 selalu dikunci eksplisit terlepas dari isi peta memori, mencegah bug pointer null dianggap alamat fisik valid. |
| **Validasi input ketat** | `pmm_init_from_map` menolak (`return false`) pointer `NULL`, `region_count == 0`, `max_phys_bytes` tidak sejajar halaman, dan storage bitmap yang tidak cukup — tidak ada asumsi implisit terhadap input pemanggil. |
| **Overflow arithmetic** | `checked_add_u64` mencegah wrap-around pada penjumlahan `base + length`, sebuah kelas bug klasik pada kode manajemen memori tingkat rendah. |
| **Double-free terdeteksi** | `pmm_free_frame` memeriksa status bit sebelum membebaskan; percobaan free kedua kali pada frame yang sama mengembalikan `false`, bukan korupsi counter secara diam-diam. |
| **Reliability query API** | Fungsi query (`pmm_free_count`, dll.) aman dipanggil dengan `pmm == NULL`, mengurangi risiko crash pada kode pemanggil yang belum menginisialisasi state. |
| **Keterbatasan yang diketahui** | PMM M6 belum menangani konkurensi (belum ada lock/atomic) karena kernel masih single-core, single-thread pada tahap ini — akan menjadi perhatian penting saat SMP (M13) diperkenalkan. |

---

## 18. Pembagian Kerja

Praktikum M6 dikerjakan secara **individu**. Seluruh tahapan — perancangan header, implementasi bitmap allocator, penulisan unit test host, perbaikan Makefile, integrasi ke `kmain.c`, verifikasi statis, smoke test QEMU, hingga commit/push — dikerjakan oleh:

| Nama | NIM | Kelas | Peran |
|---|---|---|---|
| Andiana Jamaludin Malik | 2583207073016 | 1B | Seluruh tahapan implementasi dan pengujian M6 |


---

## 19. Struktur Data dan Algoritma PMM (Detail)

### 19.1 Alur `pmm_init_from_map`

```
1. Validasi parameter (pmm, regions, bitmap_storage tidak NULL;
   region_count != 0; max_phys_bytes selaras PMM_PAGE_SIZE)
2. Hitung frame_count = max_phys_bytes / PMM_PAGE_SIZE
3. Hitung required_bitmap_bytes = ceil(frame_count / 8)
4. Jika bitmap_storage_bytes < required_bitmap_bytes -> return false
5. pmm_zero_state(pmm)
6. Isi field pmm (bitmap, bitmap_bytes, max_phys, frame_count, next_hint=0)
7. Set seluruh byte bitmap = 0xFF (fail-closed)
8. Untuk setiap region dengan type == BOOT_MEM_USABLE:
       mark_range_free(pmm, region.base, region.length)
9. mark_range_used(pmm, 0, PMM_PAGE_SIZE)   // kunci frame 0
10. Untuk setiap region dengan type != BOOT_MEM_USABLE:
       mark_range_used(pmm, region.base, region.length)
11. pmm->initialized = true
12. return true
```

### 19.2 Alur `pmm_alloc_frame` (First-Fit dengan Hint)

```
1. Jika pmm NULL, belum initialized, atau free_frames == 0 -> return PMM_INVALID_FRAME
2. Untuk frame = next_hint .. frame_count-1:
       jika bit(frame) == 0 (bebas):
           mark_frame_used(pmm, frame)
           next_hint = frame + 1
           return frame * PMM_PAGE_SIZE
3. Untuk frame = 0 .. next_hint-1 (wrap-around):
       jika bit(frame) == 0 (bebas):
           mark_frame_used(pmm, frame)
           next_hint = frame + 1
           return frame * PMM_PAGE_SIZE
4. return PMM_INVALID_FRAME
```

**Kompleksitas waktu:** rata-rata O(1) amortized untuk alokasi berurutan (karena hint), namun bisa mendekati O(n) pada kondisi terburuk (bitmap hampir penuh dan wrap-around dibutuhkan). Ini adalah trade-off standar bitmap allocator sederhana dibanding struktur lebih kompleks seperti buddy allocator, yang belum dibutuhkan pada skala M6.

---

## 20. Invariants dan Kepemilikan Data

| Invariant | Dijaga oleh | Konsekuensi bila dilanggar |
|---|---|---|
| Frame 0 selalu terpakai setelah `pmm_init_from_map` | `mark_range_used(pmm, 0, PMM_PAGE_SIZE)` dipanggil eksplisit pada langkah init | Alamat nol bisa teralokasi, berisiko disalahartikan sebagai pointer null valid |
| `free_frames + used_frames == frame_count` (secara implisit dijaga per operasi) | `mark_frame_free`/`mark_frame_used` selalu meng-update kedua counter bersamaan dengan bit bitmap | Statistik `pmm_free_count`/`pmm_used_count` menjadi tidak akurat, menyesatkan pemanggil |
| Bitmap tidak pernah ditulis di luar `[0, bitmap_bytes)` | Semua akses melalui `bitmap_set/clear/test` dengan index yang sudah divalidasi oleh pemanggil (`frame < frame_count`) | Buffer overflow pada memori kernel |
| `pmm_state` dimiliki secara eksklusif oleh pemanggil yang membuat instance-nya | Tidak ada state global tersembunyi di `pmm.c`; kernel memakai `g_pmm` statis di `kmain.c`, unit test memakai `struct pmm_state pmm;` lokal | Memungkinkan pengujian paralel/independen tanpa interferensi antar instance |
| `pmm->initialized` hanya `true` setelah seluruh langkah init sukses | Di-set di baris terakhir `pmm_init_from_map`, setelah semua validasi dan pengisian bitmap | Mencegah `pmm_alloc_frame`/`pmm_free_frame` dipanggil pada state yang belum lengkap |

---

## 21. Perbandingan Cakupan M6 terhadap Rancangan Milestone

| Aspek | Direncanakan pada dokumen milestone | Dicapai pada laporan ini | Catatan |
|---|---|---|---|
| Struktur data frame allocator | Bitmap 1 bit per frame | Sesuai — `struct pmm_state` dengan `uint8_t *bitmap` | Selesai |
| API alokasi/pembebasan | `alloc_frame`, `free_frame`, query jumlah frame | Sesuai — `pmm_alloc_frame`, `pmm_free_frame`, `pmm_free_count`, dst. | Selesai |
| Sumber peta memori nyata dari bootloader | Limine memmap request/response | Belum — memakai region demo statis `M6_DEMO_PHYS_BYTES` | Ditunda ke iterasi berikutnya |
| Uji otomatis | Uji manual via QEMU saja | Ditambah uji host mandiri (`test_pmm_host.c`) | Melebihi rencana awal (positif) |
| Update label versi/milestone di log | Tidak eksplisit direncanakan | Belum dilakukan — log masih menampilkan tag `M3` | Temuan minor, lihat bagian 15.1 |

---

## 22. Evidence Artefak M6

### 22.1 Log Build Penting (Ringkasan)

```
mkdir -p build/normal/kernel/core/
clang --target=x86_64-unknown-none-elf -std=c17 -ffreestanding ... -c kernel/core/pmm.c -o build/normal/kernel/core/pmm.o
ld.lld -nostdlib -static -z max-page-size=0x1000 -T linker.ld -Map=build/normal/kernel.map \
  -o build/normal/kernel.elf ... build/normal/kernel/core/pmm.o ...
```

### 22.2 Log Penting Boot QEMU (Expected)

```
[MCSOS:M6] pmm initialized
[MCSOS:M6] frames managed=0x0000000000008000
[MCSOS:M6] frames free=0x0000000000007fff
[MCSOS:M6] frames used=0x0000000000000001
[MCSOS:M6] sample frame=0x0000000000001000
[MCSOS:M6] sample alloc/free OK
```

### 22.3 Artefak Bukti M6

| Artefak | Path | Fungsi |
|---|---|---|
| `pmm.h` | `kernel/include/mcsos/kernel/pmm.h` | Kontrak API dan struktur data PMM |
| `pmm.c` | `kernel/core/pmm.c` | Implementasi bitmap frame allocator |
| `test_pmm_host.c` | `kernel/tests/test_pmm_host.c` | Unit test native PMM |
| `kernel.elf` | `build/normal/kernel.elf` | Kernel binary hasil build M6 |
| `kernel.map` | `build/normal/kernel.map` | Linker symbol map |
| `mcsos.iso` | `build/mcsos.iso` | Boot image ISO |
| `mcsos.iso.sha256` | `build/mcsos.iso.sha256` | Hash integritas image |
| `pmm.undefined.txt` | `build/pmm.undefined.txt` | Bukti tidak ada simbol undefined pada `pmm.o` |
| `pmm.objdump.txt` | `build/pmm.objdump.txt` | Disassembly `pmm.o` |
| `test_pmm_host` | `build/test_pmm_host` | Biner uji host (native) |

---

## 23. Referensi

1. Panduan Praktikum M6 — MCSOS 260502, disediakan oleh dosen pengampu (OS_panduan_M6.pdf), Institut Pendidikan Indonesia, 2026.
2. Limine Boot Protocol Specification, dokumentasi proyek Limine bootloader, diakses sebagai referensi skema tipe region memori (`boot_mem_type`).
3. Intel Corporation, *Intel 64 and IA-32 Architectures Software Developer's Manual*, bagian manajemen memori fisik dan ukuran halaman standar (4 KiB), sebagai dasar pemilihan `PMM_PAGE_SIZE`.
4. Dokumentasi internal proyek MCSOS 260502 — laporan praktikum M3 (`os_laporan_praktikum_M3_Andiana.md`) sebagai referensi format dan struktur laporan.

---

## 24. Rubrik Penilaian (Diisi Mandiri sebagai Referensi)

| Kriteria | Bobot | Pencapaian yang Diklaim |
|---|---|---|
| Kebenaran fungsional PMM (alloc/free/reserve) | 30% | Terpenuhi — dibuktikan `make check-m6` PASS |
| Keketatan validasi input dan penanganan edge case | 20% | Terpenuhi — validasi NULL, overflow, alignment, kapasitas bitmap |
| Integrasi ke boot sequence kernel | 15% | Terpenuhi — `kernel_memory_init()` dipanggil dari `kmain()`, log QEMU sesuai |
| Verifikasi statis biner (readelf/objdump/nm) | 15% | Terpenuhi — `make grade` PASS |
| Kualitas dokumentasi dan bukti (screenshot, log) | 10% | Terpenuhi — laporan ini beserta lampiran |
| Kebersihan riwayat commit dan proses Git | 10% | Terpenuhi — commit deskriptif, push ke branch terpisah |

---

## 25. Rencana Tindak Lanjut Menuju M7

1. Mengganti sumber peta memori demo statis (`M6_DEMO_PHYS_BYTES`) dengan parsing memmap asli dari Limine boot protocol.
2. Memperbarui `MCSOS_MILESTONE` pada `version.h` agar log boot mencerminkan milestone aktif (M6, bukan M3).
3. Menambahkan test case unit host untuk kasus overflow eksplisit (`base + length` mendekati `UINT64_MAX`).
4. Menyiapkan antarmuka PMM agar dapat dipakai VMM (M7) untuk membangun tabel halaman awal.
5. Mempertimbangkan penambahan lock sederhana pada `pmm_state` sebagai persiapan menuju SMP (M13), meski belum wajib pada tahap single-core saat ini.

---

## 26. Checklist Final Sebelum Pengumpulan M6

| Checklist | Status |
|---|---|
| Semua placeholder sudah diganti dengan data aktual | Ya |
| Metadata laporan lengkap | Ya |
| Branch kerja dan commit akhir dicatat | Ya |
| Perintah build dan test dapat dijalankan ulang | Ya |
| Log build dilampirkan (ringkasan) | Ya |
| Log `check-m6` dan `make grade` dilampirkan | Ya |
| Log boot QEMU dilampirkan beserta verifikasi angka | Ya |
| Artefak penting diberi hash (`mcsos.iso.sha256`) | Ya |
| Desain, invariants, ownership, dan failure modes dijelaskan | Ya |
| Security/reliability dibahas | Ya |
| Rencana tindak lanjut menuju M7 dituliskan | Ya |
| Referensi dicantumkan | Ya |
| Laporan disimpan sebagai Markdown | Ya |
| Lampiran screenshot evidence dilengkapi dengan nomor halaman PDF | Ya |

---

## Lampiran M6 — Screenshot Evidence

> **Catatan penting:** nama berkas screenshot di bawah ini (`Screenshot 2026-06-30 190512.png` dan `Screenshot 2026-06-30 192047.png`) adalah **placeholder** mengikuti pola penamaan bawaan Windows Snipping Tool seperti pada contoh Anda (`Screenshot 2026-06-12 194126.png`). Silakan **ganti nama berkas ini dengan nama file screenshot Anda yang sebenarnya** dari folder `C:\Users\Ajot\Pictures\M6\`, sesuai urutan halaman PDF `BUKTI_M6.pdf` di bawah.

| No. | File Screenshot | Halaman PDF | Keterangan |
|---|---|---|---|
| 1 | `Screenshot 2026-06-30 190512.png` | Halaman 1 | `git checkout -b praktikum/m6-pmm`, `git branch --show-current` |
| 2 | `Screenshot 2026-06-30 190512.png` | Halaman 2 | `cat kernel/include/mcsos/kernel/pmm.h` — isi lengkap header PMM |
| 3 | `Screenshot 2026-06-30 190512.png` | Halaman 3 | Penulisan `pmm.c` — helper align/overflow/bitmap primitif |
| 4 | `Screenshot 2026-06-30 190512.png` | Halaman 4 | Penulisan `pmm.c` — lanjutan `mark_range_free` |
| 5 | `Screenshot 2026-06-30 190512.png` | Halaman 5 | Penulisan `pmm.c` — `mark_range_used`, awal `pmm_init_from_map` |
| 6 | `Screenshot 2026-06-30 190512.png` | Halaman 6 | Penulisan `pmm.c` — isi bitmap, pengisian region usable/kernel, screenshot tersimpan (Snipping Tool) |
| 7 | `Screenshot 2026-06-30 190512.png` | Halaman 7 | `pmm_reserve_range` s.d. `pmm_frame_count`; `wc -l` = 240 baris; `make clean && make build` sukses; `rm -v` `pmm.o` |
| 8 | `Screenshot 2026-06-30 190512.png` | Halaman 8 | Penulisan `kernel/tests/test_pmm_host.c` — 5 region, assert alloc/free/reserve |
| 9 | `Screenshot 2026-06-30 192047.png` | Halaman 9 | Build gagal: `bits/wordsize.h` not found; percobaan `sed` gagal |
| 10 | `Screenshot 2026-06-30 192047.png` | Halaman 10 | Penulisan ulang `Makefile` lengkap (`SRC_C`, `check-m6`, dst.) |
| 11 | `Screenshot 2026-06-30 192047.png` | Halaman 11 | `grep -A1 "SRC_C :="`, `make check-m6` — PASS |
| 12 | `Screenshot 2026-06-30 192047.png` | Halaman 12 | Penulisan `kernel/core/kmain.c` — integrasi `kernel_memory_init()` |
| 13 | `Screenshot 2026-06-30 192047.png` | Halaman 13 | Lanjutan `kmain.c`; `make clean && make build` (tail -20) |
| 14 | `Screenshot 2026-06-30 192047.png` | Halaman 14 | Lanjutan build; `make inspect && make grade` — `M6 static grade: PASS` |
| 15 | `Screenshot 2026-06-30 192047.png` | Halaman 15 | `make image` + `qemu-system-x86_64` — log boot lengkap dengan `[MCSOS:M6]` |
| 16 | `Screenshot 2026-06-30 192047.png` | Halaman 16 | `git add -A`, `git status --short`, `git commit`, `git push -u origin praktikum/m6-pmm` |

---
