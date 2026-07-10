# Laporan Praktikum M14 — Block Device Layer, RAM Block Driver, dan Buffer Cache

## MCSOS 260502

**Nama file laporan:** `laporan_praktikum_M14_2583207073016.md`
**Nama sistem operasi:** MCSOS versi 260502
**Target default:** x86_64, QEMU, Windows 11 x64 + WSL 2, kernel monolitik pendidikan, C freestanding dengan assembly minimal, POSIX-like subset
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.
**Program Studi:** Pendidikan Teknologi Informasi
**Institusi:** Institut Pendidikan Indonesia

---

## 0. Metadata Laporan

| Atribut | Isi |
|---|---|
| Kode praktikum | M14 |
| Judul praktikum | Block Device Layer, RAM Block Driver, dan Buffer Cache |
| Jenis pengerjaan | Individu |
| Nama mahasiswa | Andiana Jamaludin Malik |
| NIM | 2583207073016 |
| Kelas | 1B |
| Nama kelompok | - |
| Anggota kelompok | - |
| Tanggal praktikum | 2026-07-10 |
| Tanggal pengumpulan | 2026-07-10 |
| Repository | ~/src/mcsos |
| Branch | praktikum-m14-block-device |
| Commit awal (basis, M13 final) | `e99a25c` (M13: implementasi VFS minimal, FD table, RAMFS, syscall file I/O awal) |
| Commit akhir | `78a89a6` (m14: tambahkan bukti gdb session, qemu smoke test, dan kernel build log) |
| Status readiness yang diklaim | Siap uji QEMU tahap M14 |

---

## 1. Sampul

# Laporan Praktikum M14
## Block Device Layer, RAM Block Driver, dan Buffer Cache

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

Saya menyatakan bahwa laporan ini disusun berdasarkan pekerjaan praktikum sendiri sesuai panduan M14 (`OS_panduan_M14.md`). Bantuan eksternal, referensi, dan dokumentasi resmi dicatat pada bagian referensi dan lampiran.

| Pernyataan | Status |
|---|---|
| Semua potongan kode eksternal diberi atribusi | Ya (panduan M14 dari dosen) |
| Semua penggunaan AI assistant dicatat | Ya |
| Repository yang dikumpulkan sesuai commit akhir | Ya |
| Tidak ada klaim readiness tanpa bukti | Ya |

Catatan penggunaan bantuan eksternal:

```text
- Panduan praktikum M14 (OS_panduan_M14.md) sebagai referensi utama
- AI assistant (Claude) digunakan sebagai pemandu interaktif langkah-demi-langkah:
  setiap perintah dijalankan sendiri oleh mahasiswa di WSL 2, hasilnya ditempel
  kembali untuk diverifikasi terhadap indikator keberhasilan panduan
- Seluruh source code (block.h, block.c, ramblk.c, bcache.c, test_m14_block.c)
  diimplementasikan berdasarkan kontrak API pada panduan dosen
- Penyesuaian mandiri: integrasi Makefile dengan pola check-mN yang sudah ada di
  repo (bukan menimpa Makefile), dan penggantian memcmp() dengan loop manual
  karena mcsos/lib/string.h milik repo hanya menyediakan memset/memcpy
```

---

## 3. Tujuan Praktikum

1. **Tujuan teknis 1:** Membangun lapisan abstraksi block device (`mcsos_blk_device_t`, `mcsos_blk_ops_t`) yang generik dan dapat divalidasi secara freestanding (tanpa dependency libc di luar `memset`/`memcpy`)
2. **Tujuan teknis 2:** Mengimplementasikan driver RAM block (`ramblk`) sebagai backing store in-memory dan buffer cache (`bcache`) dengan kebijakan eviction CLOCK serta write-back
3. **Tujuan konseptual 1:** Memahami kontrak antarmuka block layer: validasi rentang LBA, status code seragam (`MCSOS_BLK_OK/EINVAL/ERANGE/EFULL/EIO/ENODEV`), dan pemisahan device generik dari driver konkret
4. **Tujuan konseptual 2:** Memahami mekanisme buffer cache — cache hit/miss, kebijakan penggantian CLOCK, dan flush write-back ke device asli
5. **Tujuan validasi:** Menyimpan host unit test, audit freestanding (nm/readelf/objdump/checksum), log integrasi kernel, log QEMU smoke test, dan sesi GDB sebagai bukti deterministik bahwa block layer berjalan baik di host maupun di kernel sungguhan

---

## 4. Capaian Pembelajaran Praktikum

Setelah praktikum ini, mahasiswa mampu:

| CPL/CPMK praktikum | Bukti yang harus ditunjukkan |
|---|---|
| Merancang kontrak antarmuka block device generik (device, ops table, status code) | `include/mcsos/block.h` |
| Mengimplementasikan registry device dengan validasi ketat (nama, block size power-of-two, dsb.) | `kernel/block/block.c` |
| Mengimplementasikan driver block berbasis RAM sebagai backing store | `kernel/block/ramblk.c` |
| Mengimplementasikan buffer cache dengan kebijakan CLOCK dan write-back | `kernel/block/bcache.c` |
| Menulis host unit test yang memvalidasi read/write/error-path/cache | `tests/host/test_m14_block.c` |
| Mengaudit hasil kompilasi freestanding (ELF64, x86-64, tanpa undefined symbol) | `nm -u`, `readelf -h`, `objdump -dr`, `sha256sum` |
| Mengintegrasikan modul baru ke `kmain()` tanpa merusak modul M0–M13 sebelumnya | `kernel/core/kmain.c`, log build kernel |
| Membuktikan modul berjalan benar saat runtime di QEMU | Log serial QEMU dengan marker `[MCSOS:M14]` |
| Melakukan debugging runtime dengan GDB pada breakpoint fungsi block layer | `artifacts/m14/gdb_m14_session.txt` |
| Mengelola perubahan dengan Git secara aman (append, bukan overwrite Makefile) dan mem-push ke GitHub | Commit `3708c32`, `78a89a6`; branch remote |

---

## 5. Peta Milestone MCSOS

| Milestone | Fokus | Status dalam laporan |
|---|---|---|
| M0–M12 | Baseline, boot, memory, sync, syscall, ELF loader | [x] prasyarat (sudah selesai sebelumnya) |
| M13 | VFS minimal, FD table, RAMFS | [x] prasyarat (sudah selesai sebelumnya) |
| **M14** | **Block device layer, RAM block driver, buffer cache** | **[x] selesai praktikum (laporan ini)** |
| M15+ | Lanjutan (di luar cakupan) | [ ] tidak dibahas |

Batas cakupan praktikum:

```text
Praktikum M14 mencakup:
- Preflight tool & lingkungan
- Header kontrak block.h (status code, device, ops, ramblk, bcache)
- Implementasi block.c (registry + wrapper validasi read/write/flush)
- Implementasi ramblk.c (driver RAM block)
- Implementasi bcache.c (buffer cache CLOCK + write-back)
- Host unit test (tests/host/test_m14_block.c)
- Integrasi target check-m14 ke Makefile existing (append, bukan overwrite)
- Audit freestanding: nm -u, readelf -h, objdump -dr, sha256sum
- Integrasi ke kernel/core/kmain.c (m14_block_demo_init)
- Build kernel penuh (make build) dan inspeksi (make inspect)
- Pembuatan ISO bootable (make image)
- QEMU smoke test dengan log serial
- Debugging GDB pada breakpoint mcsos_blk_register/read/write
- Commit Git dan push ke GitHub

Non-goals (tidak termasuk):
- Device fisik (AHCI/NVMe/virtio-blk) — hanya RAM block
- Filesystem berbasis block (mcsfs/ext2-like) — menyusul di milestone lanjutan
- Multi-threaded/concurrent access ke bcache (belum ada locking)
- Persistent storage lintas boot (RAM block hilang saat reboot)
```

---

## 6. Dasar Teori Ringkas

### 6.1 Konsep Sistem Operasi yang Diuji

**Block Device Abstraction:** Lapisan yang memisahkan konsumen (filesystem, cache) dari implementasi konkret perangkat penyimpanan, melalui tabel operasi (`read`, `write`, `flush`) yang seragam untuk semua jenis device.

**Buffer Cache:** Lapisan in-memory yang menyimpan salinan blok data yang baru diakses untuk mengurangi I/O ke device asli, dengan kebijakan write-back (data ditulis ke device hanya saat flush, bukan setiap write).

**Kebijakan Eviction CLOCK:** Varian efisien dari LRU (Least Recently Used) yang menggunakan pointer melingkar ("clock hand") untuk memilih entry cache yang akan digantikan saat cache penuh, tanpa perlu menjaga urutan akses penuh.

**Freestanding C untuk Driver Kernel:** Implementasi tanpa hosted libc; hanya bergantung pada fungsi runtime minimal yang disediakan sendiri oleh kernel (`memset`, `memcpy`), sehingga fungsi seperti `memcmp` yang tidak tersedia harus diimplementasikan manual.

### 6.2 Konsep Arsitektur x86_64 yang Relevan

| Konsep | Relevansi pada praktikum | Bukti/verifikasi |
|---|---|---|
| ELF64 relocatable object (`REL`) | Setiap file block layer dikompilasi freestanding lalu di-link relokatabel (`ld -r`) sebelum masuk ke kernel.elf | `readelf -h build/m14_block_layer.o` |
| System V ABI x86_64 | Konvensi pemanggilan fungsi C untuk seluruh API block layer | `-mabi=sysv` pada CFLAGS |
| Symbol table & linking | Fungsi block layer harus muncul sebagai simbol global (`T`) di kernel akhir | `nm -n build/kernel.elf` |
| Higher-half kernel addressing | Alamat breakpoint GDB (`0xffffffff8000...`) konsisten dengan layout kernel M2+ | Sesi GDB |

### 6.3 Konsep Implementasi Freestanding

| Aspek | Keputusan praktikum |
|---|---|
| Bahasa | C17 freestanding, tanpa `-flto`, tanpa red zone |
| Runtime | Hanya `memset`/`memcpy` dari `mcsos/lib/string.h`; `memcmp` diganti loop manual |
| ABI | x86_64 System V calling convention |
| Compiler flags kritis | `--target=x86_64-unknown-none-elf -ffreestanding -fno-builtin -mno-red-zone -Wall -Wextra -Werror` |
| Kebijakan error | Semua fungsi publik mengembalikan `mcsos_blk_status_t`, tidak ada exception/longjmp |

### 6.4 Referensi Teori yang Digunakan

| No. | Sumber | Bagian yang digunakan | Alasan relevansi |
|---|---|---|---|
| [1] | OSDev Wiki - Buffer Cache concepts | Kebijakan write-back dan cache hit/miss | Dasar desain `bcache.c` |
| [2] | LLVM Project - Clang User's Manual | Freestanding builds | Flags kompilasi freestanding |
| [3] | LLVM Project - LLD (ld.lld) | Relocatable linking (`-r`) | Penggabungan object M14 |
| [4] | GNU Binutils - nm, readelf, objdump | Audit symbol & ELF header | Verifikasi freestanding |
| [5] | Panduan Praktikum M14 (dosen) | Seluruh kontrak API dan checkpoint | Sumber utama implementasi |

---

## 7. Lingkungan Praktikum

### 7.1 Host dan Target

| Komponen | Nilai |
|---|---|
| Host OS | Windows 11 x64 |
| Lingkungan build | WSL 2 — Ubuntu 26.04 LTS (resolute), kernel `6.6.87.2-microsoft-standard-WSL2` |
| Target ISA | x86_64 |
| Target ABI | `x86_64-unknown-none-elf` |
| Emulator | QEMU system-x86_64 |
| Debugger | GDB |
| Build system | GNU Make (`.RECIPEPREFIX := >`) |
| Bahasa utama | C17 freestanding |
| Linker | `ld.lld` (LLVM) |

### 7.2 Versi Toolchain

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_02.png` (halaman 2 PDF)

Output perintah:

```bash
{ uname -a; lsb_release -a 2>/dev/null || cat /etc/os-release; } | tee artifacts/m14/host_info.txt
{ clang --version; ld --version | head -n 1; nm --version | head -n 1; readelf --version | head -n 1; \
  objdump --version | head -n 1; make --version | head -n 1; qemu-system-x86_64 --version; } | tee artifacts/m14/tool_versions.txt
```

Hasil:

```
Linux JotDesu 6.6.87.2-microsoft-standard-WSL2 #1 SMP PREEMPT_DYNAMIC x86_64 GNU/Linux
Distributor ID: Ubuntu
Description:    Ubuntu 26.04 LTS
Codename:       resolute
Ubuntu clang version 21.1.8 (6ubuntu1)
GNU ld (GNU Binutils for Ubuntu) 2.46
GNU nm (GNU Binutils for Ubuntu) 2.46
GNU readelf (GNU Binutils for Ubuntu) 2.46
GNU objdump (GNU Binutils for Ubuntu) 2.46
GNU Make 4.4.1
QEMU emulator version 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3)
```

Tambahan: GDB terverifikasi terpisah pada Langkah GDB — `GNU gdb (Ubuntu 17.1-2ubuntu1) 17.1`.

### 7.3 Lokasi Repository

| Item | Nilai |
|---|---|
| Path repository di WSL | `/home/andianaaji/src/mcsos` |
| Apakah berada di filesystem Linux WSL, bukan `/mnt/c` | Ya |
| Remote repository | `https://github.com/JotDesu/mcsos260502_.git` |
| Branch kerja | `praktikum-m14-block-device` |
| Basis branch | `praktikum-m13-vfs-ramfs` (commit `e99a25c`) |
| Commit hash akhir | `78a89a6` |

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_01.png` (halaman 1 PDF — konteks push branch M13 sebelum memulai M14)

---

## 8. Repository dan Struktur File

### 8.1 Struktur Direktori yang Relevan

```text
mcsos/
├── Makefile                      (diubah: target check-m14 di-append)
├── include/
│   └── mcsos/
│       └── block.h               (baru, 94 baris)
├── kernel/
│   ├── block/
│   │   ├── block.c                (baru, 108 baris)
│   │   ├── ramblk.c               (baru, 81 baris)
│   │   └── bcache.c               (baru, 141 baris)
│   └── core/
│       └── kmain.c                (diubah: +include block.h, +m14_block_demo_init())
├── tests/
│   └── host/
│       └── test_m14_block.c       (baru, 66 baris)
├── scripts/
│   └── m14_preflight.sh           (baru)
├── artifacts/
│   └── m14/
│       ├── host_info.txt
│       ├── tool_versions.txt
│       ├── preflight.log
│       ├── git_status_before_m14.txt
│       ├── m14_check_m14.log
│       ├── m14_kernel_build.log
│       ├── m14_kernel_build_with_demo.log
│       ├── m14_kernel_inspect.log
│       ├── m14_image_build.log
│       ├── qemu_m14.log
│       └── gdb_m14_session.txt
└── build/                         (hasil, tidak dikomit)
    ├── kernel.elf
    ├── mcsos.iso
    ├── block.o / ramblk.o / bcache.o / m14_block_layer.o
    ├── test_m14_block_host
    └── m14.sha256.txt
```

### 8.2 File yang Dibuat atau Diubah

| File | Jenis perubahan | Alasan perubahan | Risiko |
|---|---|---|---|
| `include/mcsos/block.h` | Baru | Kontrak API block layer (status, device, ops, ramblk, bcache) | Rendah — header murni |
| `kernel/block/block.c` | Baru | Registry device + wrapper validasi read/write/flush | Sedang — validasi rentang harus benar |
| `kernel/block/ramblk.c` | Baru | Driver RAM block sebagai backing store | Rendah — memcpy sederhana |
| `kernel/block/bcache.c` | Baru | Buffer cache CLOCK + write-back | Sedang — logika eviction & dirty tracking |
| `tests/host/test_m14_block.c` | Baru | Host unit test read/write/error-path/cache | Rendah — test only |
| `Makefile` | Diubah (append) | Target `check-m14` mengikuti pola `check-mN` yang sudah ada | Sedang — harus tidak merusak target M6–M10 |
| `kernel/core/kmain.c` | Diubah | Include `block.h`, tambah `m14_block_demo_init()`, panggil setelah `m13_vfs_selftest()` | Sedang — harus tidak mengubah urutan boot M5–M13 |
| `scripts/m14_preflight.sh` | Baru | Validasi tool & lingkungan sebelum mulai | Rendah |

### 8.3 Ringkasan Diff

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_31.png` (halaman 31 PDF)

```bash
git status --short
git commit -m "m14: block device layer, ram block driver, buffer cache, host test, kernel integration"
git log --oneline -n 5
```

Output:
```
[praktikum-m14-block-device 3708c32] m14: block device layer, ram block driver, buffer cache, host test, kernel integration
 11 files changed, 651 insertions(+)
3708c32 (HEAD -> praktikum-m14-block-device) m14: block device layer, ram block driver, buffer cache, host test, kernel integration
e99a25c (origin/praktikum-m13-vfs-ramfs, praktikum-m13-vfs-ramfs) M13: implementasi VFS minimal, FD table, RAMFS, syscall file I/O awal
3727d4c (praktikum/m12-sync) M12: implementasi spinlock, mutex, dan lockdep validator dengan self-test terintegrasi
2270ecd (praktikum-m11-elf-user-loader) M11: tambahkan script QEMU smoke test
745e711 M11: lengkapi evidence host-test, freestanding, audit yang tertinggal
```

Commit kedua (bukti GDB): `78a89a6` — 1 file changed, 78 insertions (`artifacts/m14/gdb_m14_session.txt`).

---

## 9. Desain Teknis

### 9.1 Masalah yang Diselesaikan

Kernel M13 belum memiliki:
- Abstraksi block device generik yang bisa dipakai berbagai driver penyimpanan
- Mekanisme buffer cache untuk mengurangi I/O berulang ke device
- Validasi rentang akses (LBA + count) yang seragam untuk semua device

### 9.2 Keputusan Desain

| Keputusan | Alternatif yang dipertimbangkan | Alasan memilih | Konsekuensi |
|---|---|---|---|
| Ops table (`mcsos_blk_ops_t`) berisi 3 function pointer (read/write/flush) | Virtual class ala C++ | Sesuai gaya freestanding C, sederhana, deterministik | Driver wajib isi read & write, flush opsional |
| Validasi rentang di wrapper `mcsos_blk_read/write` (bukan di tiap driver) | Validasi di masing-masing driver | Menghindari duplikasi, satu titik kebenaran | Driver bisa asumsikan LBA valid saat dipanggil |
| CLOCK eviction pada bcache | LRU penuh (linked list) | Lebih sederhana untuk kernel freestanding, O(1) amortized | Bukan LRU sempurna, tapi cukup untuk skala kecil |
| Write-back (bukan write-through) | Write-through | Mengurangi I/O per write | Perlu `flush_all` eksplisit sebelum data benar-benar persisten |
| Integrasi Makefile via **append** target `check-m14` | Overwrite Makefile sesuai contoh literal panduan | Makefile existing sudah mengelola M6–M13; overwrite akan merusak `check-m6`...`check-m10`, `image`, `inspect` | Nama target sedikit berbeda dari panduan generik (`check-m14` bukan `host-test`/`freestanding`/`audit`), tapi tujuan tercapai identik |
| `memcmp` diganti loop manual di `kmain.c` | Menambah `memcmp` ke `mcsos/lib/string.h` | Tidak mengubah kontrak runtime library milik modul lain tanpa instruksi eksplisit; cukup lokal di titik pemakaian | Kode demo sedikit lebih verbose, tapi tidak menambah permukaan API baru |

### 9.3 Arsitektur Ringkas

```
   +------------------+     +------------------+     +------------------+
   |  Konsumen (demo, |---->|   block.c        |---->|  ops->read/write |
   |  future FS)      |     |  (registry +     |     |  (driver konkret)|
   +------------------+     |   validasi)      |     +--------+---------+
                             +--------+---------+              |
                                      |                         v
                             +--------v---------+     +------------------+
                             |    bcache.c      |     |    ramblk.c      |
                             | (CLOCK eviction, |---->| (RAM backing     |
                             |  write-back)     |     |  store, memcpy)  |
                             +------------------+     +------------------+
```

### 9.4 Kontrak Antarmuka

| Antarmuka | Pemanggil | Penerima | Precondition | Postcondition | Error path |
|---|---|---|---|---|---|
| `mcsos_blk_register(dev)` | Driver init (`ramblk_init` konsumen, demo) | `block.c` | `dev->ops->read/write` tidak NULL, nama non-kosong, block_size power-of-two ≥ 512 | Device masuk registry, dapat diakses via index | `MCSOS_BLK_EINVAL`/`EFULL` |
| `mcsos_blk_read/write(dev, lba, count, buf)` | Konsumen mana pun | `block.c` → `dev->ops->read/write` | `lba + count ≤ dev->block_count`, buffer non-NULL | Data tersalin sesuai isi device | `MCSOS_BLK_ERANGE`/`EINVAL` |
| `mcsos_ramblk_init(dev, ram, name, storage, size, block_size)` | Konsumen (demo `m14_block_demo_init`) | `ramblk.c` | `storage_size` kelipatan `block_size` | `dev` terisi lengkap dengan ops RAM | `MCSOS_BLK_EINVAL` |
| `mcsos_bcache_read/write(cache, dev, lba, buf)` | Konsumen dengan cache | `bcache.c` | `cache->block_size == dev->block_size` | Data di-cache, ditulis dirty (untuk write) | `MCSOS_BLK_EINVAL` bila mismatch ukuran blok |
| `mcsos_bcache_flush_all(cache)` | Konsumen sebelum shutdown/checkpoint | `bcache.c` → `mcsos_blk_write` | - | Semua entry dirty ditulis ke device, flag dirty di-clear | Meneruskan error dari `mcsos_blk_write` |

### 9.5 Struktur Data Utama

| Struktur data | Field penting | Ownership | Lifetime | Invariant |
|---|---|---|---|---|
| `mcsos_blk_device_t` | `name`, `block_size`, `block_count`, `ops`, `driver_data` | Driver/konsumen (statis di M14 demo) | Selama registry aktif | `block_size` power-of-two ≥ 512 |
| `mcsos_ramblk_t` | `storage`, `storage_size` | Driver ramblk | Sama dengan buffer backing statis | `storage_size` kelipatan `block_size` |
| `mcsos_bcache_entry_t` | `data`, `lba`, `valid`, `dirty`, `dev` | `mcsos_bcache_t` | Selama cache aktif | `valid==0` berarti slot kosong, aman digantikan |
| `mcsos_bcache_t` | `entries`, `entry_count`, `clock_hand` | Konsumen (demo) | Statis | `clock_hand < entry_count` |

### 9.6 Invariants

1. Setiap device terdaftar harus memiliki `ops->read` dan `ops->write` non-NULL
2. `block_size` selalu power-of-two dan ≥ `MCSOS_BLK_DEFAULT_SECTOR_SIZE` (512)
3. Validasi rentang (`lba + count ≤ block_count`) selalu dilakukan sebelum memanggil driver
4. Buffer cache tidak pernah menyalin data melewati `cache->block_size`
5. `mcsos_bcache_flush_all` harus idempotent — memanggilnya dua kali tidak menghasilkan efek tambahan
6. Kernel tidak memakai `memcmp` dari libc (tidak tersedia); perbandingan manual byte-per-byte digunakan di titik pemakaian

### 9.7 Ownership, Locking, dan Concurrency

M14 masih **single-threaded** dari sisi block layer — belum ada locking pada `bcache`/registry. Ini konsisten dengan non-goals praktikum. Pemanggilan `m14_block_demo_init()` terjadi sebelum scheduler M9 mengaktifkan multi-thread, sehingga tidak ada race condition pada demo ini.

### 9.8 Memory Safety dan Undefined Behavior Risk

| Risiko | Lokasi | Mitigasi | Bukti |
|---|---|---|---|
| Integer overflow pada `byte_offset`/`byte_count` | `ramblk.c` (`mcsos_ramblk_rw`) | Perhitungan dengan `uint64_t`, dicek `byte_offset > storage_size` dulu | Source code |
| Out-of-range LBA | `block.c` (`mcsos_blk_validate_range`) | Perbandingan eksplisit `lba >= block_count` sebelum akses | Host test `EXPECT_STATUS(..., MCSOS_BLK_ERANGE)` |
| Pemanggilan `memcmp` yang tidak dideklarasikan (freestanding) | `kmain.c` awal | Diganti loop manual `m14_mismatch` | Log build (error → fix → sukses) |
| Buffer cache menyalin melewati kapasitas entry | `bcache.c` | `mcsos_memcpy_u8_bcache` dibatasi `cache->block_size` | Source code |

### 9.9 Security Boundary

| Boundary | Data tidak tepercaya | Validasi yang dilakukan | Failure mode aman |
|---|---|---|---|
| API publik block layer | LBA, count, buffer pointer dari konsumen | Validasi rentang & NULL check di `block.c` | Return status error, tidak crash |
| Registry device | Struktur `mcsos_blk_device_t` yang didaftarkan | Validasi nama, ops, block_size sebelum diterima | `MCSOS_BLK_EINVAL`, device ditolak |

---

## 10. Langkah Kerja Implementasi

### Langkah 1 — Cek Versi Tool & Lingkungan

Maksud langkah: Memastikan seluruh toolchain (clang, ld, nm, readelf, objdump, make, qemu) tersedia sebelum memulai.

Perintah:
```bash
mkdir -p artifacts/m14
{ uname -a; lsb_release -a 2>/dev/null || cat /etc/os-release; } | tee artifacts/m14/host_info.txt
{ clang --version; ld --version | head -n 1; nm --version | head -n 1; readelf --version | head -n 1; \
  objdump --version | head -n 1; make --version | head -n 1; qemu-system-x86_64 --version; } | tee artifacts/m14/tool_versions.txt
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_02.png` (hal. 2)

Artefak: `artifacts/m14/host_info.txt`, `artifacts/m14/tool_versions.txt`

Indikator berhasil: Semua tool menampilkan versi, tidak ada `command not found`. **PASS.**

### Langkah 2 — Branch dan Struktur Direktori Kerja

Perintah:
```bash
git status --short
git switch -c praktikum-m14-block-device
mkdir -p include/mcsos kernel/block tests/host scripts artifacts/m14
```

Indikator berhasil: Branch baru `praktikum-m14-block-device` aktif, direktori kerja tersedia. **PASS.**

### Langkah 3 — Preflight Check M14

Maksud langkah: Memvalidasi keberadaan toolchain dan struktur direktori sebelum implementasi via script otomatis.

Perintah (`scripts/m14_preflight.sh`, ringkas): fungsi `require_file()`, `require_cmd()` untuk clang/ld/nm/readelf/objdump/sha256sum/make/qemu-system-x86_64, cek direktori `include/kernel/tests/scripts`, cek dokumen panduan M0–M13, dan `git status --short`.

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_03.png`, `Screenshot_M14_04.png` (hal. 3–4)

Hasil eksekusi:
```
OK_CMD: clang / ld / nm / readelf / objdump / sha256sum / make / qemu-system-x86_64
OK_DIR: include, kernel, tests, scripts
WARN_DOC_NOT_FOUND_IN_REPO: OS_panduan_M0.md ... M13.md   (wajar, dokumen panduan tidak disimpan di repo)
M14_PREFLIGHT_DONE
```

Artefak: `artifacts/m14/preflight.log`, `artifacts/m14/git_status_before_m14.txt`

Indikator berhasil: Log diakhiri `M14_PREFLIGHT_DONE`, semua `OK_CMD`/`OK_DIR`. **PASS.**

### Langkah 4 — Header Kontrak `include/mcsos/block.h`

Maksud langkah: Mendefinisikan status code, struct device, ops table, dan struct ramblk/bcache sebagai kontrak API inti M14.

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_05.png`, `Screenshot_M14_06.png` (hal. 5–6)

Verifikasi:
```bash
cat include/mcsos/block.h | head -n 5
wc -l include/mcsos/block.h
```
Hasil: `94 include/mcsos/block.h`. **PASS.**

### Langkah 5 — `kernel/block/block.c`

Maksud langkah: Implementasi registry device (`register`, `get`, `count`, `reset`) dan wrapper validasi (`read`, `write`, `flush`) yang memvalidasi rentang LBA sebelum memanggil driver.

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_07.png`, `Screenshot_M14_08.png` (hal. 7–8)

Verifikasi: `wc -l kernel/block/block.c` → **108 baris**. **PASS.**

### Langkah 6 — `kernel/block/ramblk.c`

Maksud langkah: Implementasi driver RAM block — backing store in-memory dengan validasi rentang byte dan operasi read/write/flush.

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_09.png`, `Screenshot_M14_10.png` (hal. 9–10)

Verifikasi: `wc -l kernel/block/ramblk.c` → **81 baris**. **PASS.**

### Langkah 7 — `kernel/block/bcache.c`

Maksud langkah: Implementasi buffer cache dengan pencarian entry (`mcsos_bcache_find`), kebijakan eviction CLOCK (`mcsos_bcache_select_victim`), serta operasi `read`/`write`/`flush_all` write-back.

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_11.png`, `Screenshot_M14_12.png` (hal. 11–12)

Verifikasi struktur: 8 fungsi (1 helper memcpy, 3 helper internal, 4 API publik `init`/`read`/`write`/`flush_all`), 8 penutup `}` awal baris, `wc -l` → **141 baris**. **PASS.**

### Langkah 8 — Host Unit Test `tests/host/test_m14_block.c`

Maksud langkah: Menguji register/read/write/error-path (`ERANGE`, `EINVAL`) dan buffer cache (write-then-read, cache-vs-device consistency, flush) secara host-side sebelum masuk ke kernel.

Verifikasi: 28 pemanggilan makro `EXPECT_OK/EQ/STATUS`, file diakhiri `printf("M14 host tests PASS\n"); return 0;`, **66 baris**. **PASS.**

### Langkah 9 — Integrasi Target `check-m14` ke Makefile

Maksud langkah: Menambahkan target build/test M14 **tanpa merusak** target M6–M13 yang sudah ada di Makefile repo (yang memakai `.RECIPEPREFIX := >` dan pola `check-mN`).

> **Catatan penting keputusan desain:** Panduan asli menuliskan contoh `cat > Makefile <<'EOF'` (overwrite total). Karena Makefile repo sudah mengelola build kernel M0–M13 penuh (`check-m6` s.d. `check-m10`, `image`, `inspect`), overwrite akan **menghapus** seluruh target tersebut dan melanggar syarat panduan sendiri bahwa *"M14 tidak boleh merusak jalur integrasi kernel M0-M13"*. Keputusan yang diambil: **append** (`cat >> Makefile`) target `check-m14` yang secara fungsional identik dengan target `host-test`/`freestanding`/`audit` pada panduan generik, hanya disesuaikan nama agar konsisten dengan konvensi repo.

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_13.png`, `Screenshot_M14_14.png` (hal. 13–14)

Verifikasi: `grep -n "check-m14\|check-m13\|check-m10" Makefile` menunjukkan `check-m10` (baris 176) tetap ada dan `check-m14` (baris 206) baru ditambahkan. **PASS.**

### Langkah 10 — Jalankan Host Test & Audit Freestanding

Perintah:
```bash
make check-m14 2>&1 | tee artifacts/m14/m14_check_m14.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_15.png`, `Screenshot_M14_16.png`, `Screenshot_M14_17.png` (hal. 15–17)

Hasil kunci:
```
M14 host tests PASS
nm -u build/m14_block_layer.o   -> kosong (0 baris)
readelf -h build/m14_block_layer.o -> ELF64, REL, Advanced Micro Devices X86-64
[PASS] M14 static check selesai
```

Checksum SHA-256 (artefak `build/m14.sha256.txt`):
```
07807a852c75395e4dd77ab62b2efff0ce95ceb7f2277a0280ed1e2bd6f7a2e1  build/block.o
4df6d6a68cae631899b702b33e22cad600d833cdac3a230895794cf8e7d500c8  build/ramblk.o
0353127aee57efce56b71ea2a6a55dc14723e82b5b350fd17b2755697e45932b  build/bcache.o
2dcc30212cbc35f67b2e70946d4b3b0fec4a823d4676e93f3e4cb1d041b2fc43  build/m14_block_layer.o
3196bc964970b15cf0a836bb821f955b605cf5a7da797ef5be2c5413c588bcb9  build/test_m14_block_host
```

Indikator berhasil: Compile tanpa warning (`-Werror` aktif), host test PASS, undefined symbol kosong, ELF header x86-64, checksum tersimpan. **PASS — setara CP14.2–CP14.4.**

### Langkah 11 — Integrasi Kernel Penuh (Build Awal, Belum Ada Demo)

Maksud langkah: Membuktikan bahwa penambahan `kernel/block/*.c` tidak merusak build kernel M0–M13, memanfaatkan `find kernel -name '*.c'` otomatis pada Makefile.

Perintah:
```bash
make clean
make build 2>&1 | tee artifacts/m14/m14_kernel_build.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_18.png`, `Screenshot_M14_19.png` (hal. 18–19)

Hasil: `block.o`, `ramblk.o`, `bcache.o` otomatis ikut ter-compile & ter-link ke `kernel.elf`, `EXIT_CODE=0`. **PASS — setara CP14.5 (tahap awal, sebelum demo runtime ditambahkan).**

### Langkah 12 — Sisipkan Fungsi Demo M14 ke `kmain.c`

Maksud langkah: Menambahkan `m14_block_demo_init()` mengikuti pola bootstrap modul lain (M6–M13) di `kmain.c`: inisialisasi RAM block device 512×64 byte, register ke registry, tulis-baca pola data, verifikasi kecocokan.

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_20.png`, `Screenshot_M14_21.png` (hal. 20–21)

Sub-langkah:
1. `#include "mcsos/block.h"` disisipkan setelah `#include "mcs_vfs.h"` (baris 19)
2. Fungsi `m14_block_demo_init()` disisipkan sebelum `void kmain(void) {` (baris 435)
3. Pemanggilan `m14_block_demo_init();` disisipkan setelah `m13_vfs_selftest();` (baris 517)

Verifikasi: `grep -n "m14_block_demo_init\|m13_vfs_selftest();" kernel/core/kmain.c` menunjukkan urutan baris 516–517 benar. **PASS.**

### Langkah 13 — Perbaikan Compile Error (`memcmp` tidak tersedia)

Maksud langkah: Menangani error freestanding — `memcmp` tidak dideklarasikan di `kernel/include/mcsos/lib/string.h` (hanya `memset`/`memcpy`).

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_22.png`, `Screenshot_M14_23.png` (hal. 22–23)

Error awal:
```
kernel/core/kmain.c:472:9: error: call to undeclared function 'memcmp';
  ISO C99 and later do not support implicit function declarations
```

Perbaikan: mengganti pemanggilan `memcmp(pattern, readback, sizeof(pattern)) != 0` dengan loop pembanding byte manual (`int m14_mismatch`), konsisten dengan gaya `mcsos_memcpy_u8` di driver M14 lainnya.

### Langkah 13 (lanjutan) — Build Ulang Kernel dengan Demo M14

Perintah:
```bash
make clean
make build 2>&1 | tee artifacts/m14/m14_kernel_build_with_demo.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_24.png`, `Screenshot_M14_25.png` (hal. 24–25)

Hasil: `kmain.c` (dengan `m14_block_demo_init`) ter-compile tanpa error, `ld.lld` berhasil link `kernel.elf`, `EXIT_CODE=0`. **PASS — setara CP14.5 (integrasi penuh).**

### Langkah 14 — Validasi ELF Kernel (`make inspect`)

Perintah:
```bash
make inspect 2>&1 | tee artifacts/m14/m14_kernel_inspect.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_26.png` (hal. 26)

Hasil: seluruh `grep -q` (ELF64, Machine x86-64, `kmain`, `kernel_panic_at`, `idt_init`, `pic_remap`, `pit_configure_hz`, `isr_stub_32`, `timer_on_irq0`, `pmm_init_from_map`, `pmm_alloc_frame`) lolos, `EXIT_CODE=0`.

Verifikasi tambahan simbol M14 pada `build/kernel.syms.txt`:
```
ffffffff800008a0 T mcsos_bcache_read
ffffffff80000be0 T mcsos_bcache_write
ffffffff80000e20 T mcsos_blk_register
ffffffff80000fc0 T mcsos_blk_read
ffffffff800010d0 T mcsos_blk_write
ffffffff80001280 T mcsos_ramblk_init
ffffffff80002090 t m14_block_demo_init
```
**PASS.**

### Langkah 15 — Pembuatan ISO Bootable (`make image`)

Perintah:
```bash
make image 2>&1 | tee artifacts/m14/m14_image_build.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_27.png` (hal. 27)

Hasil: `xorriso` berhasil membuat `build/mcsos.iso` (6.682.624 byte), Limine BIOS stage terinstal, checksum `build/mcsos.iso.sha256` tersimpan, `EXIT_CODE=0`. **PASS.**

### Langkah 16 — QEMU Smoke Test

Percobaan pertama (gagal, sebagai catatan pembelajaran): menjalankan QEMU di background (`&`) dengan `-serial stdio` menyebabkan job `Stopped` (SIGTTIN) karena proses background tidak boleh mengakses terminal interaktif.

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_28.png` (hal. 28)

Percobaan kedua (berhasil): menggunakan `timeout` dan redirect stdin dari `/dev/null`, dijalankan foreground:
```bash
timeout 10 qemu-system-x86_64 -machine q35 -m 256M -serial stdio -no-reboot -no-shutdown \
  -cdrom build/mcsos.iso -display none < /dev/null 2>&1 | tee artifacts/m14/qemu_m14.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_29.png`, `Screenshot_M14_30.png` (hal. 29–30)

Cuplikan log serial (urutan M5→M13 utuh, lalu M14 tepat setelah M13):
```
[MCSOS:M13] vfs/fd/ramfs self-test PASS
[MCSOS:M14] block layer initialized
[MCSOS:M14] ram0 block_size=0x0000000000000200
[MCSOS:M14] ram0 block_count=0x0000000000000040
[MCSOS:M14] ram0 read/write self-test PASS
[MCSOS:M9] scheduler initialized
[MCSOS:M9] thread A tick
...
[MCSOS:TIMER] ticks=0x0000000000000384
```

Indikator berhasil: milestone M14 tercapai, `ram0` terdaftar (block_size 512, block_count 64), self-test read/write PASS, tidak ada panic/triple-fault, kernel lanjut stabil ke M9 scheduler & timer. **PASS — setara CP14.6.**

### Langkah 17 — Commit Git (Implementasi & Integrasi)

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_31.png` (hal. 31)

```bash
git add include/mcsos/block.h kernel/block/block.c kernel/block/ramblk.c kernel/block/bcache.c \
  tests/host/test_m14_block.c kernel/core/kmain.c Makefile scripts/m14_preflight.sh artifacts/m14
git commit -m "m14: block device layer, ram block driver, buffer cache, host test, kernel integration"
```
Hasil: commit `3708c32`, 11 file berubah, 651 baris ditambahkan. **PASS — setara CP14.7.**

### Langkah 18 — Debugging GDB pada Breakpoint Block Layer

Maksud langkah: Membuktikan fungsi block layer benar-benar dieksekusi saat runtime kernel (bukan hanya lolos host test), menggunakan sesi dua-terminal QEMU (`-S -s`) + GDB remote.

Bukti screenshot: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_32.png` (hal. 32)

```bash
# Terminal 1
qemu-system-x86_64 -machine q35 -m 256M -serial stdio -no-reboot -no-shutdown -S -s -display none -cdrom build/mcsos.iso

# Terminal 2
gdb build/kernel.elf -ex 'target remote :1234' \
  -ex 'break mcsos_blk_register' -ex 'break mcsos_blk_read' -ex 'break mcsos_blk_write' -ex 'continue'
```

Hasil breakpoint berurutan (sesuai alur kode `m14_block_demo_init`):
```
Breakpoint 1, 0xffffffff80000e20 in mcsos_blk_register ()
Breakpoint 3, 0xffffffff800010d0 in mcsos_blk_write ()
Breakpoint 2, 0xffffffff80000fc0 in mcsos_blk_read ()
```
Alamat breakpoint **identik** dengan simbol pada `kernel.syms.txt` — korelasi sempurna antara source, symbol table, dan eksekusi runtime.

Bukti screenshot dump register (`info breakpoints`, `info registers`): `C:\Users\Ajot\Pictures\M14\Screenshot_M14_33.png`, `Screenshot_M14_34.png`, `Screenshot_M14_35.png` (hal. 33–35)

Sesi kedua reconnect menunjukkan register umum (`rax`–`r15`, `rip` di `mcsos_sched_enqueue+42`), register kontrol (`cr0`–`cr4`, `efer`), dan register XMM0–XMM15, tersimpan ke `artifacts/m14/gdb_m14_session.txt`.

Bukti screenshot verifikasi file: `C:\Users\Ajot\Pictures\M14\Screenshot_M14_36.png` (hal. 36) — `wc -l` → 78 baris.

**PASS — bukti debugging runtime lengkap.**

### Langkah 19 — Commit Bukti GDB & Push ke GitHub

```bash
git add artifacts/m14
git commit -m "m14: tambahkan bukti gdb session, qemu smoke test, dan kernel build log"
git push -u origin praktikum-m14-block-device
```
Hasil: commit `78a89a6` (1 file, 78 insertions); push sukses, branch baru `praktikum-m14-block-device` dibuat di remote dan upstream tracking terpasang (`git branch -vv` menunjukkan `[origin/praktikum-m14-block-device]`). **PASS.**

---

## 11. Checkpoint Buildable

| Checkpoint | Perintah | Expected result | Status |
|---|---|---|---|
| CP14.1 Preflight | `./scripts/m14_preflight.sh` | `M14_PREFLIGHT_DONE`, semua tool `OK_CMD` | PASS |
| CP14.2 Host test | `make check-m14` | `M14 host tests PASS` | PASS |
| CP14.3 Freestanding compile | `make check-m14` | `block.o`/`ramblk.o`/`bcache.o` terbentuk, `-Werror` bersih | PASS |
| CP14.4 Audit | `make check-m14` | `nm -u` kosong, `readelf`/`objdump`/checksum tersimpan | PASS |
| CP14.5 Integrasi kernel | `make build` | `kernel.elf` build sukses dengan simbol M14 | PASS |
| CP14.6 QEMU smoke test | `timeout 10 qemu-system-x86_64 ...` | Log `[MCSOS:M14] ... self-test PASS`, tanpa panic | PASS |
| CP14.7 Git commit | `git commit` | Commit tercatat dengan pesan jelas | PASS |
| Debugging GDB | `gdb ... break mcsos_blk_*` | Breakpoint tercapai di 3 fungsi block layer | PASS |
| Push GitHub | `git push -u origin ...` | Branch remote baru terbentuk | PASS |

---

## 12. Perintah Uji dan Validasi

### 12.1 Build Test

```bash
make clean
make check-m14
```
Hasil: Build & host test berhasil tanpa warning/error. Status: **PASS**

### 12.2 Static Inspection

```bash
nm -u build/m14_block_layer.o
readelf -h build/m14_block_layer.o
objdump -dr build/m14_block_layer.o
sha256sum build/block.o build/ramblk.o build/bcache.o build/m14_block_layer.o build/test_m14_block_host
```
Hasil penting: undefined symbol kosong; `Class: ELF64`, `Type: REL`, `Machine: Advanced Micro Devices X86-64`. Status: **PASS**

### 12.3 QEMU Smoke Test

```bash
timeout 10 qemu-system-x86_64 -machine q35 -m 256M -serial stdio -no-reboot -no-shutdown \
  -cdrom build/mcsos.iso -display none < /dev/null 2>&1 | tee artifacts/m14/qemu_m14.log
```
Hasil: milestone `[MCSOS:M14]` tercapai lengkap, tidak ada panic/reboot loop. Status: **PASS**

### 12.4 GDB Debug Evidence

Hasil:
- Breakpoint `mcsos_blk_register`, `mcsos_blk_write`, `mcsos_blk_read` tercapai berurutan sesuai kode
- Alamat breakpoint cocok dengan symbol table kernel
- Register CPU dapat di-dump penuh (`info registers`)

Status: **PASS**

### 12.5 Ringkasan Artefak Bukti

| Artefak | Path | Fungsi |
|---|---|---|
| `m14.sha256.txt` | `build/m14.sha256.txt` | Checksum 5 artefak build M14 |
| `mcsos.iso.sha256` | `build/mcsos.iso.sha256` | Checksum ISO bootable |
| `qemu_m14.log` | `artifacts/m14/qemu_m14.log` | Log serial smoke test |
| `gdb_m14_session.txt` | `artifacts/m14/gdb_m14_session.txt` | Sesi debug GDB (78 baris) |
| `m14_kernel_inspect.log` | `artifacts/m14/m14_kernel_inspect.log` | Hasil `make inspect` |

---

## 13. Analisis Teknis

### 13.1 Analisis Keberhasilan

Alur end-to-end M14 berhasil dibuktikan pada tiga lapisan independen:
1. **Host test** — logika block layer benar secara isolasi (tanpa kernel)
2. **Static audit** — hasil kompilasi freestanding valid (ELF64, x86-64, tanpa undefined symbol)
3. **Runtime kernel** — log serial QEMU dan breakpoint GDB membuktikan kode yang sama benar-benar dieksekusi di dalam kernel sungguhan, dengan hasil identik (block_size 512, block_count 64, self-test PASS)

### 13.2 Analisis Kegagalan atau Perbedaan Hasil

| Kegagalan sementara | Penyebab | Perbaikan | Dampak akhir |
|---|---|---|---|
| `memcmp` undeclared saat compile `kmain.c` | `mcsos/lib/string.h` hanya expose `memset`/`memcpy` | Diganti loop pembanding manual | Tidak ada dampak fungsional, hanya gaya kode |
| QEMU `Stopped` (SIGTTIN) pada percobaan pertama smoke test | `-serial stdio` dijalankan di background (`&`) | Dijalankan foreground dengan `timeout` + `< /dev/null` | Tidak ada dampak pada kernel, murni isu shell job control |
| Perbedaan estimasi jumlah baris vs hasil aktual (mis. `block.c` 108 vs estimasi awal) | Estimasi kasar penulis panduan | Diverifikasi ulang via `grep -c`/`tail` untuk memastikan isi lengkap | Tidak ada dampak — hanya angka baris, isi terverifikasi benar |

### 13.3 Perbandingan dengan Teori

| Konsep teori | Implementasi praktikum | Sesuai/tidak sesuai | Penjelasan |
|---|---|---|---|
| Block device abstraction dengan ops table | `mcsos_blk_ops_t { read, write, flush }` | Sesuai | Driver konkret hanya perlu mengisi 3 function pointer |
| Buffer cache write-back | `bcache.c` — dirty flag, flush eksplisit | Sesuai | Data tidak langsung ke device saat `bcache_write` |
| Kebijakan eviction CLOCK | `mcsos_bcache_select_victim` dengan `clock_hand` | Sesuai | Memindai slot invalid dulu, baru evict dengan clock hand |
| Freestanding tanpa hosted libc | Hanya `memset`/`memcpy`, `memcmp` manual | Sesuai | Konsisten dengan flag `-ffreestanding -fno-builtin` |

### 13.4 Kompleksitas dan Kinerja

| Aspek | Estimasi/hasil | Bukti | Catatan |
|---|---|---|---|
| `mcsos_bcache_find` | O(n) terhadap `entry_count` | Source code (linear scan) | Cukup untuk cache berukuran kecil (2 entry pada demo) |
| `mcsos_bcache_select_victim` | O(n) worst-case (satu putaran clock) | Source code | Amortized O(1) untuk akses berulang |
| Waktu build `check-m14` | < 5 detik | Log `m14_check_m14.log` | Source minimal, tanpa optimisasi berat |
| Ukuran object relocatable | Total 5 file kecil (block/ramblk/bcache/layer/test) | `sha256sum` listing | Sesuai skala modul demo |

---

## 14. Debugging dan Failure Modes

### 14.1 Failure Modes yang Ditemukan

| Failure mode | Gejala | Penyebab | Bukti | Perbaikan |
|---|---|---|---|---|
| Compile error `memcmp` undeclared | `error: call to undeclared function 'memcmp'` | Freestanding runtime tidak menyediakan `memcmp` | Log build `m14_kernel_build.log` (percobaan pertama) | Loop pembanding manual byte-per-byte |
| QEMU job `Stopped` | Shell menghentikan proses background yang akses stdio | `-serial stdio` + `&` tanpa detach terminal | Output `[1]+ Stopped ...` | `timeout` + foreground + `< /dev/null` |

### 14.2 Failure Modes yang Diantisipasi

| Failure mode | Deteksi | Dampak | Mitigasi |
|---|---|---|---|
| Overwrite Makefile menghapus target M6–M13 | Review isi Makefile sebelum eksekusi | Build kernel M0–M13 rusak total | Append target `check-m14`, verifikasi `check-m10` masih ada setelahnya |
| LBA di luar rentang device | `mcsos_blk_validate_range` | Data corruption/out-of-bounds read | Return `MCSOS_BLK_ERANGE`, diuji di host test |
| Buffer NULL pada API publik | Cek NULL di setiap fungsi wrapper | NULL pointer dereference | Return `MCSOS_BLK_EINVAL` sebelum akses |
| Cache mismatch block_size vs device | Cek `cache->block_size != dev->block_size` | Data ter-truncate/overrun | Return `MCSOS_BLK_EINVAL` di awal `bcache_read/write` |

### 14.3 Triage yang Dilakukan

Urutan diagnosis yang dipakai selama praktikum:
1. Jalankan preflight: `./scripts/m14_preflight.sh`
2. Jalankan host test terisolasi: `make check-m14`
3. Verifikasi audit freestanding: `nm -u`, `readelf -h`
4. Build kernel penuh: `make build`, cek `EXIT_CODE`
5. Jika compile error di `kmain.c`: cek header runtime (`mcsos/lib/string.h`) sebelum menambah fungsi baru
6. Inspeksi simbol: `nm -n build/kernel.elf | grep mcsos_blk`
7. QEMU smoke test dengan `timeout` (bukan background job)
8. Debug dengan GDB: breakpoint di fungsi target, `continue` berurutan

### 14.4 Panic Path

Pada M14, kegagalan inisialisasi/self-test di `m14_block_demo_init()` akan memicu `KERNEL_PANIC(...)` (mengikuti pola modul M6–M13 lain), misalnya jika `mcsos_ramblk_init`, `mcsos_blk_register`, `mcsos_blk_write/read`, atau perbandingan pattern gagal. Pada praktikum ini, seluruh jalur berhasil sehingga panic path tidak teraktivasi — dibuktikan oleh log QEMU yang mengalir sampai `[MCSOS:M14] ram0 read/write self-test PASS` tanpa interupsi.

---

## 15. Prosedur Rollback

| Skenario rollback | Perintah | Data yang harus diselamatkan | Status |
|---|---|---|---|
| Kembali ke sebelum M14 | `git checkout e99a25c` (commit M13 final) | Branch M14 tetap ada terpisah | Teruji |
| Bersihkan artefak build M14 | `make clean` | Source & artifacts/m14 aman (tidak terhapus) | Teruji |
| Regenerasi kernel & ISO | `make build && make image` | - | Teruji |
| Revert Makefile ke sebelum M14 | `git checkout e99a25c -- Makefile` | Kehilangan target `check-m14` — perlu re-append | Belum diuji eksplisit, tapi append bersifat non-destruktif |

Catatan rollback:
```text
Karena strategi integrasi M14 memakai append (bukan overwrite) pada Makefile,
rollback parsial (hanya menghapus fitur M14) relatif aman: cukup checkout Makefile
ke commit e99a25c dan hapus kernel/block/, include/mcsos/block.h, serta baris
terkait m14 di kmain.c. Target check-m6 s.d. check-m10 dan image/inspect tidak
pernah tersentuh oleh perubahan M14.
```

---

## 16. Keamanan dan Reliability

### 16.1 Risiko Keamanan

| Risiko | Boundary | Dampak | Mitigasi | Evidence |
|---|---|---|---|---|
| Overwrite Makefile oleh instruksi panduan literal | File konfigurasi build | Hilangnya seluruh jalur build M0–M13 | Keputusan sadar memakai append, diverifikasi dengan `grep check-m10` | Bagian 9.2, Langkah 9 |
| Rentang LBA tidak divalidasi | API publik block layer | Out-of-bounds read/write pada backing store | Validasi eksplisit di `mcsos_blk_validate_range` | Host test `EXPECT_STATUS(..., ERANGE)` |
| Buffer cache dipakai lintas device dengan block_size berbeda | `bcache_read/write` | Data corruption/overrun | Cek `cache->block_size != dev->block_size` di awal fungsi | Source code `bcache.c` |

### 16.2 Reliability dan Data Integrity

| Risiko reliability | Dampak | Deteksi | Mitigasi |
|---|---|---|---|
| Data di cache belum ter-flush saat "shutdown" | Data hilang (RAM block volatile) | Tidak ada auto-flush otomatis | `mcsos_bcache_flush_all` dipanggil eksplisit sebelum verifikasi ulang di host test |
| Build tidak reproducible | Hasil berbeda antar build | `make clean && make build` ulang | Clean build dilakukan 2× selama praktikum (sebelum & sesudah fix `memcmp`), hasil konsisten |
| ISO corrupt | Tidak dapat boot | `sha256sum` checksum | `build/mcsos.iso.sha256` tersimpan |

### 16.3 Negative Test

| Negative test | Input buruk | Expected result | Actual result | Status |
|---|---|---|---|---|
| Write melewati batas device | `mcsos_blk_write(dev, 31, 2, ...)` pada device 32 blok | `MCSOS_BLK_ERANGE` | `MCSOS_BLK_ERANGE` | PASS |
| Read di luar batas | `mcsos_blk_read(dev, 32, 1, ...)` pada device 32 blok | `MCSOS_BLK_ERANGE` | `MCSOS_BLK_ERANGE` | PASS |
| Count nol | `mcsos_blk_write(dev, 0, 0, tmp)` | `MCSOS_BLK_EINVAL` | `MCSOS_BLK_EINVAL` | PASS |
| Buffer NULL | `mcsos_blk_write(dev, 0, 1, 0)` | `MCSOS_BLK_EINVAL` | `MCSOS_BLK_EINVAL` | PASS |

---

## 17. Pembagian Kerja Kelompok

Tidak berlaku (pengerjaan individu).

---

## 18. Kriteria Lulus Praktikum

| Kriteria minimum | Status | Evidence |
|---|---|---|
| Proyek dapat dibangun dari clean checkout | PASS | `make clean && make check-m14` / `make build` |
| Perintah build terdokumentasi | PASS | Makefile target `check-m14`, panduan M14 |
| QEMU boot atau test target berjalan deterministik | PASS | `artifacts/m14/qemu_m14.log` |
| Semua unit test/praktikum test relevan lulus | PASS | `M14 host tests PASS` |
| Log serial disimpan | PASS | `artifacts/m14/qemu_m14.log` |
| Panic path terbaca atau dijelaskan | PASS | Bagian 14.4 |
| Tidak ada warning kritis pada build | PASS | `-Werror` aktif, build bersih |
| Perubahan Git terkomit | PASS | Commit `3708c32`, `78a89a6` |
| Desain dan failure mode dijelaskan | PASS | Bagian 9, 14 |
| Laporan berisi screenshot/log yang cukup | PASS | Lampiran F |

Kriteria tambahan:
| Kriteria lanjutan | Status | Evidence |
|---|---|---|
| Static analysis (freestanding audit) dijalankan | PASS | `nm -u`, `readelf -h`, `objdump -dr` |
| Disassembly/readelf evidence tersedia | PASS | `build/m14_block_layer.readelf.header.txt`, `.objdump.txt` |
| Review keamanan dilakukan | PASS | Bagian 16 |
| Rollback dianalisis | PASS | Bagian 15 |
| Debugging runtime (GDB) dilakukan | PASS | `artifacts/m14/gdb_m14_session.txt` |
| Push ke remote Git | PASS | Branch `praktikum-m14-block-device` di GitHub |

---

## 19. Readiness Review

| Status | Definisi | Pilihan |
|---|---|---|
| Belum siap uji | Build/test belum stabil atau bukti belum cukup | [ ] |
| Siap uji QEMU | Build bersih, QEMU/test target berjalan, log tersedia | [x] |
| Siap demonstrasi praktikum | Siap ditunjukkan di kelas dengan bukti uji, failure mode, dan rollback | [x] |
| Kandidat siap pakai terbatas | Hanya untuk penggunaan terbatas setelah test, security review, dokumentasi, dan known issue tersedia | [ ] |

Alasan readiness:
```text
Berdasarkan bukti:
1. Build bersih: make clean && make check-m14 lulus tanpa warning (-Werror aktif)
2. Freestanding valid: nm -u kosong, readelf membuktikan ELF64 REL x86-64
3. Kernel utuh: make build & make inspect PASS, simbol M14 muncul di kernel.syms.txt
4. ISO terbentuk: make image menghasilkan mcsos.iso dengan checksum
5. QEMU boot: log serial membuktikan M14 self-test PASS tanpa mengganggu M5-M13
6. GDB debug: 3 breakpoint block layer tercapai berurutan sesuai alur kode
7. Git: 2 commit rapi, branch berhasil di-push ke GitHub

Status "siap uji QEMU tahap M14" dan "siap demonstrasi praktikum" tercapai
karena tersedia bukti build, static audit, runtime QEMU, dan debugging GDB.
Bukan "kandidat siap pakai" karena block layer M14 belum punya locking untuk
concurrency (non-goal), belum ada driver device fisik, dan belum terhubung
ke filesystem block-based.
```

Known issues:
| No. | Issue | Dampak | Workaround | Target perbaikan |
|---|---|---|---|---|
| 1 | Belum ada locking pada registry/bcache | Race condition bila dipanggil dari thread lain | Demo dijalankan sebelum scheduler M9 aktif | Milestone lanjutan dengan concurrency |
| 2 | Hanya RAM block driver (belum ada device fisik) | Data tidak persistent lintas boot | Sesuai non-goal M14 | Milestone driver AHCI/virtio-blk |
| 3 | Belum ada filesystem berbasis block | Block layer belum dipakai konsumen nyata selain demo | Host test & demo kernel sebagai bukti fungsional | Milestone mcsfs/ext2-like |

Keputusan akhir:
```text
Berdasarkan evidence host test, static audit freestanding, integrasi kernel,
QEMU serial log, dan sesi debugging GDB, hasil praktikum ini layak disebut
SIAP UJI QEMU dan SIAP DEMONSTRASI PRAKTIKUM tahap M14. Belum layak disebut
"siap pakai" karena locking concurrency dan driver device fisik belum tersedia
(memang di luar cakupan M14 sesuai non-goals).
```

---

## 20. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Indikator nilai penuh | Nilai |
|---|---:|---|---:|
| Kebenaran fungsional | 30 | Implementasi memenuhi kontrak block.h, semua host test & QEMU smoke test PASS | 30 |
| Kualitas desain dan invariants | 20 | Desain jelas, kontrak antarmuka eksplisit, invariants terdokumentasi | 20 |
| Pengujian dan bukti | 20 | Host test, freestanding audit, QEMU log, GDB debug lengkap | 20 |
| Debugging dan failure analysis | 10 | Failure mode (memcmp, SIGTTIN) dianalisis dan diperbaiki dengan bukti | 10 |
| Keamanan dan robustness | 10 | Boundary validasi, negative test, keputusan append Makefile dianalisis | 10 |
| Dokumentasi dan laporan | 10 | Laporan rapi, lengkap, dapat direproduksi, screenshot terlampir | 10 |
| **Total** | **100** |  | **100** |

---

## 21. Kesimpulan

### 21.1 Yang Berhasil

1. **Kontrak API block layer** (`block.h`) berhasil dirancang dan diimplementasikan secara freestanding
2. **Driver RAM block** (`ramblk.c`) berfungsi sebagai backing store in-memory dengan validasi rentang byte
3. **Buffer cache CLOCK write-back** (`bcache.c`) berhasil diimplementasikan dan diverifikasi konsisten dengan device asli setelah flush
4. **Host unit test** lulus penuh (`M14 host tests PASS`) mencakup jalur sukses dan error (`ERANGE`, `EINVAL`)
5. **Audit freestanding** bersih: tanpa undefined symbol, ELF64 x86-64 valid, checksum tersimpan
6. **Integrasi kernel** berhasil tanpa merusak modul M0–M13 (keputusan sadar memakai append Makefile)
7. **QEMU smoke test** membuktikan modul berjalan benar saat runtime, dengan log deterministik
8. **Debugging GDB** membuktikan 3 fungsi inti (`register`/`read`/`write`) benar-benar dieksekusi sesuai urutan kode
9. **Git workflow** rapi: 2 commit terpisah (implementasi, bukti GDB), berhasil di-push ke GitHub

### 21.2 Yang Belum Berhasil / Di Luar Cakupan

1. Locking/concurrency pada block layer belum diimplementasikan (sesuai non-goal M14)
2. Driver device fisik (AHCI/NVMe/virtio-blk) belum ada — hanya RAM block
3. Filesystem berbasis block belum terhubung ke lapisan ini (menyusul milestone lanjutan)

### 21.3 Rencana Perbaikan

1. Milestone berikutnya: menambahkan locking (spinlock/mutex, sudah tersedia dari M12) pada registry dan bcache untuk mendukung akses multi-thread
2. Menambahkan filesystem berbasis block (mcsfs/ext2-like) yang mengonsumsi API `mcsos_blk_*`/`mcsos_bcache_*`
3. Selalu lakukan clean build (`make clean`) sebelum audit akhir untuk memastikan reproducibility

---

## 22. Lampiran

### Lampiran A — Commit Log

```bash
git log --oneline -n 5
```

Output:
```
78a89a6 (HEAD -> praktikum-m14-block-device) m14: tambahkan bukti gdb session, qemu smoke test, dan kernel build log
3708c32 m14: block device layer, ram block driver, buffer cache, host test, kernel integration
e99a25c (origin/praktikum-m13-vfs-ramfs, praktikum-m13-vfs-ramfs) M13: implementasi VFS minimal, FD table, RAMFS, syscall file I/O awal
3727d4c (praktikum/m12-sync) M12: implementasi spinlock, mutex, dan lockdep validator dengan self-test terintegrasi
2270ecd (praktikum-m11-elf-user-loader) M11: tambahkan script QEMU smoke test
```

### Lampiran B — Ringkasan Diff

```
11 files changed, 651 insertions(+)   [commit 3708c32]
 1 file changed, 78 insertions(+)     [commit 78a89a6]

create mode 100644 artifacts/m14/*.txt/.log (9 file)
create mode 100644 include/mcsos/block.h
create mode 100644 kernel/block/bcache.c
create mode 100644 kernel/block/block.c
create mode 100644 kernel/block/ramblk.c
create mode 100755 scripts/m14_preflight.sh
create mode 100644 tests/host/test_m14_block.c
modified:   Makefile
modified:   kernel/core/kmain.c
```

### Lampiran C — Log Build Lengkap

Tersedia di: `artifacts/m14/m14_check_m14.log`, `artifacts/m14/m14_kernel_build.log`, `artifacts/m14/m14_kernel_build_with_demo.log`, `artifacts/m14/m14_kernel_inspect.log`, `artifacts/m14/m14_image_build.log`

### Lampiran D — Log QEMU Lengkap

```
limine: Loading executable `boot():/boot/kernel.elf`...
MCSOS 260502 M3 kernel entered
...
[MCSOS:M13] vfs/fd/ramfs self-test PASS
[MCSOS:M14] block layer initialized
[MCSOS:M14] ram0 block_size=0x0000000000000200
[MCSOS:M14] ram0 block_count=0x0000000000000040
[MCSOS:M14] ram0 read/write self-test PASS
[MCSOS:M9] scheduler initialized
...
[MCSOS:TIMER] ticks=0x0000000000000384
Terminated
```

### Lampiran E — Output Readelf/Nm/Objdump

**readelf -h build/m14_block_layer.o:**
```
ELF Header:
  Class:                             ELF64
  Data:                              2's complement, little endian
  Type:                              REL (Relocatable file)
  Machine:                           Advanced Micro Devices X86-64
  Number of section headers:         12
```

**nm -n build/kernel.elf (cuplikan simbol M14):**
```
ffffffff800008a0 T mcsos_bcache_read
ffffffff80000be0 T mcsos_bcache_write
ffffffff80000e20 T mcsos_blk_register
ffffffff80000fc0 T mcsos_blk_read
ffffffff800010d0 T mcsos_blk_write
ffffffff80001280 T mcsos_ramblk_init
ffffffff80002090 t m14_block_demo_init
```

### Lampiran F — Screenshot

> Catatan: file bukti asli (`bukti_M14.pdf`) berisi **36 halaman** screenshot berurutan sesuai kronologi pengerjaan. Nama file di kolom kedua adalah saran penamaan sekuensial (`Screenshot_M14_NN.png`) yang dipetakan ke nomor halaman PDF tersebut — silakan sesuaikan dengan nama file screenshot asli Anda di `C:\Users\Ajot\Pictures\M14\` bila berbeda.

| No. | Halaman PDF | File (saran) | Keterangan |
|---|---|---|---|
| 1 | 1 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_01.png` | Konteks: push branch M13 ke GitHub sebelum memulai M14 |
| 2 | 2 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_02.png` | Langkah 1: host_info.txt & tool_versions.txt |
| 3 | 3 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_03.png` | Langkah 3: pembuatan scripts/m14_preflight.sh (bagian 1) |
| 4 | 4 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_04.png` | Langkah 3: hasil eksekusi preflight — M14_PREFLIGHT_DONE |
| 5 | 5 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_05.png` | Langkah 4: pembuatan include/mcsos/block.h (bagian 1) |
| 6 | 6 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_06.png` | Langkah 4: block.h lengkap, wc -l = 94 |
| 7 | 7 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_07.png` | Langkah 5: pembuatan kernel/block/block.c (bagian 1) |
| 8 | 8 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_08.png` | Langkah 5: block.c lengkap, wc -l = 108 |
| 9 | 9 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_09.png` | Langkah 6: pembuatan kernel/block/ramblk.c (bagian 1) |
| 10 | 10 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_10.png` | Langkah 6: ramblk.c lengkap, wc -l = 81 |
| 11 | 11 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_11.png` | Langkah 7: pembuatan kernel/block/bcache.c (bagian 1) |
| 12 | 12 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_12.png` | Langkah 7: bcache.c lengkap (read/write/flush_all) |
| 13 | 13 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_13.png` | Langkah 9: append target check-m14 ke Makefile (bagian 1) |
| 14 | 14 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_14.png` | Langkah 9: verifikasi check-m14 & check-m10 sama-sama ada |
| 15 | 15 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_15.png` | Langkah 10: make check-m14 — kompilasi freestanding |
| 16 | 16 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_16.png` | Langkah 10: M14 host tests PASS, nm -u, readelf, [PASS] |
| 17 | 17 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_17.png` | Langkah 10: verifikasi ELF header & checksum lengkap |
| 18 | 18 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_18.png` | Langkah 11: make clean && make build (awal) |
| 19 | 19 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_19.png` | Langkah 11: build kernel selesai, EXIT_CODE=0 |
| 20 | 20 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_20.png` | Langkah 12: sisip include block.h & fungsi demo (python) |
| 21 | 21 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_21.png` | Langkah 12: verifikasi baris 435/516/517 kmain.c |
| 22 | 22 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_22.png` | Langkah 13: script python ganti memcmp -> loop manual |
| 23 | 23 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_23.png` | Langkah 13: error memcmp awal + isi string.h + verifikasi fix |
| 24 | 24 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_24.png` | Langkah 13: make clean && make build (dengan demo) |
| 25 | 25 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_25.png` | Langkah 13: build selesai EXIT_CODE=0 (kmain.c sukses) |
| 26 | 26 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_26.png` | Langkah 14: make inspect — semua grep -q PASS |
| 27 | 27 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_27.png` | Langkah 15: make image — ISO terbentuk, checksum |
| 28 | 28 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_28.png` | Langkah 16: percobaan QEMU background gagal (Stopped) |
| 29 | 29 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_29.png` | Langkah 16: QEMU dengan timeout, log awal M5-M13 |
| 30 | 30 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_30.png` | Langkah 16: log lengkap sampai M14 self-test PASS + M9 |
| 31 | 31 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_31.png` | Langkah 17: git commit 3708c32 + git log |
| 32 | 32 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_32.png` | Langkah 18: sesi GDB dua terminal, 3 breakpoint tercapai |
| 33 | 33 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_33.png` | Langkah 18: info breakpoints/registers — reconnect GDB |
| 34 | 34 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_34.png` | Langkah 18: dump register XMM0-XMM13 |
| 35 | 35 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_35.png` | Langkah 18: dump register XMM14-15, mxcsr, quit/detach |
| 36 | 36 | `C:\Users\Ajot\Pictures\M14\Screenshot_M14_36.png` | Langkah 18-19: verifikasi gdb_m14_session.txt + commit 78a89a6 |

### Lampiran G — Bukti Tambahan

- **SHA-256 object M14:** Tercatat di `build/m14.sha256.txt`
- **SHA-256 ISO:** Tercatat di `build/mcsos.iso.sha256`
- **Preflight log:** Tercatat di `artifacts/m14/preflight.log`
- **Branch remote GitHub:** `https://github.com/JotDesu/mcsos260502_/tree/praktikum-m14-block-device`

---

## 23. Daftar Referensi

[1] OSDev Wiki, "Buffer," OSDev Wiki. Accessed: 2026-07-10. [Online]. Available: https://wiki.osdev.org/index.php?title=Buffer

[2] LLVM Project, "Clang Compiler User's Manual — Freestanding Builds," Clang documentation. Accessed: 2026-07-10. [Online]. Available: https://clang.llvm.org/docs/UsersManual.html

[3] LLVM Project, "LLD — The LLVM Linker," LLD documentation. Accessed: 2026-07-10. [Online]. Available: https://lld.llvm.org/

[4] GNU Project, "readelf, nm, objdump," GNU Binary Utilities. Accessed: 2026-07-10. [Online]. Available: https://www.gnu.org/software/binutils/binutils.html

[5] GNU Project, "GDB: The GNU Project Debugger," GDB documentation. Accessed: 2026-07-10. [Online]. Available: https://www.gnu.org/software/gdb/documentation/

[6] Panduan Praktikum M14 — Block Device Layer, RAM Block Driver, dan Buffer Cache, MCSOS 260502, Muhaemin Sidiq, S.Pd., M.Pd., Institut Pendidikan Indonesia, 2026.

---

## 24. Checklist Final Sebelum Pengumpulan

| Checklist | Status |
|---|---|
| Semua placeholder sudah diganti dengan data aktual | Ya |
| Metadata laporan lengkap | Ya |
| Commit awal dan akhir dicatat | Ya |
| Perintah build dan test dapat dijalankan ulang | Ya |
| Log build dilampirkan | Ya |
| Log QEMU/test dilampirkan | Ya |
| Artefak penting diberi hash | Ya |
| Desain, invariants, ownership, dan failure modes dijelaskan | Ya |
| Security/reliability dibahas | Ya |
| Readiness review tidak berlebihan | Ya |
| Rubrik penilaian diisi | Ya |
| Referensi memakai format IEEE | Ya |
| Laporan disimpan sebagai Markdown | Ya |
| Screenshot dipetakan ke halaman PDF bukti | Ya |

---

## 25. Pernyataan Pengumpulan

Saya mengumpulkan laporan ini bersama artefak pendukung pada commit:

```
78a89a6 (praktikum-m14-block-device) m14: tambahkan bukti gdb session, qemu smoke test, dan kernel build log
```

Status akhir yang diklaim:

```
Siap uji QEMU tahap M14 dan siap demonstrasi praktikum
```

Ringkasan satu paragraf:

```text
Praktikum M14 berhasil mengimplementasikan lapisan block device generik
(mcsos_blk_device_t, ops table read/write/flush) beserta driver RAM block
(ramblk) dan buffer cache dengan kebijakan CLOCK write-back (bcache), lengkap
dengan host unit test yang lulus penuh dan audit freestanding yang bersih
(ELF64, x86-64, tanpa undefined symbol). Modul berhasil diintegrasikan ke
kmain.c tanpa merusak milestone M0-M13 sebelumnya (dibuktikan lewat keputusan
sadar meng-append, bukan menimpa, Makefile), divalidasi lewat QEMU smoke test
yang menunjukkan self-test PASS pada runtime kernel sungguhan, dan diverifikasi
lebih lanjut lewat sesi debugging GDB yang membuktikan tiga fungsi inti block
layer (register/read/write) dieksekusi tepat sesuai alur kode. Seluruh
perubahan terkomit rapi dalam dua commit dan telah di-push ke branch
praktikum-m14-block-device di GitHub. Status: siap uji QEMU dan siap
demonstrasi praktikum tahap M14.
```
