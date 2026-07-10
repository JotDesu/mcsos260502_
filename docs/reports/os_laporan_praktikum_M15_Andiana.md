# Laporan Praktikum M15 — Filesystem Persistent Minimal MCSFS1, On-Disk Superblock/Inode/Directory, dan Fsck-Lite

## MCSOS 260502

**Nama file laporan:** `laporan_praktikum_M15_[NIM_Kelompok].md`
**Nama sistem operasi:** MCSOS versi 260502
**Target default:** x86_64, QEMU, Windows 11 x64 + WSL 2, kernel monolitik pendidikan, C17 freestanding dengan assembly minimal, POSIX-like subset
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.
**Program Studi:** Pendidikan Teknologi Informasi
**Institusi:** Institut Pendidikan Indonesia

---

## 0. Metadata Laporan

| Atribut | Isi |
|---|---|
| Kode praktikum | M15 |
| Judul praktikum | Filesystem Persistent Minimal MCSFS1, On-Disk Superblock/Inode/Directory, dan Fsck-Lite |
| Jenis pengerjaan | Individu |
| Nama mahasiswa | Andiana Jamaludin Malik |
| NIM | 2583207073016 |
| Kelas | 1B |
| Nama kelompok | - |
| Anggota kelompok | - |
| Tanggal praktikum | 2026-07-10 |
| Tanggal pengumpulan | 2026-07-10 |
| Repository | ~/src/mcsos |
| Branch | praktikum-m15-mcsfs1 |
| Commit awal | `78a89a6` (m14: tambahkan bukti gdb session, qemu smoke test, dan kernel build log) |
| Commit akhir | `9a026c8` (M15: tambahkan bukti QEMU smoke test — baseline M14, MCSFS1 belum ditautkan ke kernel) |
| Status readiness yang diklaim | Siap uji QEMU untuk filesystem persistent minimal MCSFS1 (bukan crash-consistent penuh, bukan siap produksi) |

---

## 1. Sampul

# Laporan Praktikum M15
## Filesystem Persistent Minimal MCSFS1, On-Disk Superblock/Inode/Directory, dan Fsck-Lite

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

Saya menyatakan bahwa laporan ini disusun berdasarkan pekerjaan praktikum sendiri sesuai panduan M15 (`OS_panduan_M15.md`). Bantuan eksternal, referensi, dan dokumentasi resmi dicatat pada bagian ini.

| Pernyataan | Status |
|---|---|
| Semua potongan kode eksternal diberi atribusi | Ya (panduan M15 dari dosen) |
| Semua penggunaan AI assistant dicatat | Ya |
| Repository yang dikumpulkan sesuai commit akhir | Ya |
| Tidak ada klaim readiness tanpa bukti | Ya |

Catatan penggunaan bantuan eksternal:

```text
- Panduan praktikum M15 (OS_panduan_M15.md) sebagai referensi utama
- AI assistant (Claude) digunakan untuk memandu langkah kerja, menyusun perintah
  yang dijalankan mahasiswa di WSL 2, memverifikasi setiap output terhadap
  indikator panduan, dan menyusun laporan ini
- Seluruh perintah aktual dijalankan sendiri oleh mahasiswa di terminal WSL 2
  (bukti: screenshot terminal, lihat Lampiran)
- Semua source code diimplementasikan berdasarkan panduan dosen; penyesuaian
  Makefile (target check-m15) dilakukan agar konsisten dengan konvensi
  Makefile repository milik mahasiswa sendiri (pola check-mX, .RECIPEPREFIX)
```

---

## 3. Tujuan Praktikum

1. **Tujuan teknis 1:** Mengimplementasikan filesystem persistent minimal **MCSFS1** (superblock, inode bitmap, block bitmap, inode table, root directory, direct data block) di atas block device layer M14.
2. **Tujuan teknis 2:** Mengimplementasikan operasi `format`, `mount`, `fsck`, `create`, `write`, `read`, dan `unlink` tanpa hosted libc (freestanding C17).
3. **Tujuan teknis 3:** Membuktikan operasi tersebut melalui host unit test dengan RAM-backed block device.
4. **Tujuan teknis 4:** Mengompilasi source filesystem menjadi object freestanding x86_64 dan mengauditnya dengan `nm`, `readelf`, `objdump`.
5. **Tujuan teknis 5:** Menambahkan fault injection tambahan untuk membuktikan `fsck-lite` dan validasi input bekerja.
6. **Tujuan teknis 6:** Menjalankan QEMU smoke test untuk memastikan tidak ada boot regression pada kernel MCSOS.
7. **Tujuan konseptual 1:** Memahami hubungan VFS (M13) — MCSFS1 (M15) — block layer (M14): VFS menangani file descriptor/syscall, MCSFS1 menangani namespace/inode/alokasi block, block layer menangani I/O berbasis LBA.
8. **Tujuan validasi:** Menyimpan log build, log host test, audit ELF, checksum, dan log QEMU sebagai bukti deterministik.

---

## 4. Capaian Pembelajaran Praktikum

Setelah praktikum ini, mahasiswa mampu:

| CPL/CPMK praktikum | Bukti yang harus ditunjukkan |
|---|---|
| Menjelaskan hubungan VFS, block device, buffer cache, dan filesystem persistent | Bagian 9 dan 15 laporan ini |
| Mendesain superblock, inode bitmap, block bitmap, inode table, root directory, direct data block | `fs/mcsfs1/mcsfs1.h`, `fs/mcsfs1/mcsfs1.c` |
| Menjelaskan invariant filesystem (magic/version, root inode, bitmap konsisten, LBA in range, file size, dirent aktif) | Bagian 14.2 laporan ini |
| Mengimplementasikan format/mount/fsck/create/write/read/unlink pada filesystem root-only | `fs/mcsfs1/mcsfs1.c` |
| Menguji operasi filesystem dengan RAM-backed block device pada host test | `tests/m15/test_mcsfs1.c` |
| Mengompilasi source filesystem menjadi object freestanding x86_64 tanpa dependensi libc tersembunyi | `build/mcsfs1.o`, audit `nm -u` kosong |
| Menghasilkan bukti audit `nm`, `readelf`, `objdump`, `sha256sum`, host test, dan QEMU smoke log | `build/mcsfs1.undefined.txt`, `build/mcsfs1.readelf.header.txt`, `build/mcsfs1.objdump.txt`, `build/m15.sha256.txt`, `artifacts/m15/qemu_serial.txt` |
| Menganalisis failure modes seperti corrupt superblock, nama terlalu panjang, dan sebagainya | Bagian 15 laporan ini |

---

## 5. Peta Milestone MCSOS

| Milestone | Fokus | Status dalam laporan |
|---|---|---|
| M0 | Requirements, governance, baseline arsitektur | [ ] tidak dibahas (artefak lokal missing saat preflight M15) |
| M1 | Toolchain reproducible, Git, QEMU, GDB, metadata build | [ ] tidak dibahas |
| M2 | Boot image, kernel ELF64, early console | [ ] tidak dibahas |
| M3 | Panic path, linker map, GDB, observability awal | [ ] tidak dibahas |
| M4 | Trap, exception, interrupt, timer | [ ] tidak dibahas |
| M5 | PIC/PIT/IRQ0, timer tick | [ ] tidak dibahas langsung (terverifikasi lewat log QEMU M15) |
| M6 | PMM bitmap | [ ] tidak dibahas langsung (terverifikasi lewat log QEMU M15) |
| M7 | VMM awal | [ ] tidak dibahas langsung (terverifikasi lewat log QEMU M15) |
| M8 | Kernel heap | [ ] tidak dibahas langsung (terverifikasi lewat log QEMU M15) |
| M9 | Kernel thread/scheduler | [ ] tidak dibahas langsung (terverifikasi lewat log QEMU M15) |
| M10 | Syscall ABI awal | [ ] tidak dibahas langsung (terverifikasi lewat log QEMU M15) |
| M11 | ELF user loader | [ ] tidak dibahas langsung (terverifikasi lewat log QEMU M15) |
| M12 | Synchronization | [ ] tidak dibahas langsung (terverifikasi lewat log QEMU M15) |
| M13 | VFS/RAMFS/FD table | [ ] tidak dibahas langsung, tapi menjadi konsumen konseptual MCSFS1 |
| M14 | Block layer/RAM block/buffer cache | [x] prasyarat langsung M15, status **present** saat preflight |
| **M15** | **Filesystem persistent minimal MCSFS1, superblock/inode/directory, fsck-lite** | **[x] selesai praktikum (laporan ini)** |
| M16 | Observability, update/rollback, release image, readiness review | [ ] tidak dibahas |

Batas cakupan praktikum:

```text
Praktikum M15 mencakup:
- Preflight M15 (scripts/m15_preflight.sh)
- Header fs/mcsfs1/mcsfs1.h (konstanta format, error code, API)
- Implementasi fs/mcsfs1/mcsfs1.c (format, mount, fsck, create, write,
  read, unlink, helper freestanding: memset/memcpy/memcmp/strlen_bound)
- Host unit test tests/m15/test_mcsfs1.c dengan RAM-backed block device
- Fault injection tambahan (create-name-too-long)
- Target Makefile check-m15 (disesuaikan dengan konvensi repository)
- Audit nm -u, readelf -h, objdump -dr, sha256sum
- Build ISO (make image) dan QEMU smoke test baseline
- Commit git M15 sebanyak 2 kali

Non-goals (tidak termasuk, sesuai panduan M15):
- Kompatibilitas ext2/ext4, POSIX penuh
- Directory bertingkat (subdirectory)
- Permission DAC, ACL, hard link, symbolic link
- Journaling, crash recovery penuh, fsync POSIX penuh
- Quota, xattr, mmap, page cache, writeback daemon
- Driver disk nyata, virtio-blk, AHCI, NVMe, DMA
- Integrasi penuh MCSFS1 ke kernel image (tugas pengayaan, belum dikerjakan)
- Production readiness
```

---

## 6. Dasar Teori Ringkas

### 6.1 Konsep Sistem Operasi yang Diuji

**Layer chain M15:** VFS (M13, syscall file I/O) → **MCSFS1 (M15, namespace/inode/alokasi block)** → Block layer (M14, I/O berbasis LBA) → RAM block driver.

MCSFS1 mengambil gagasan inti dari desain filesystem klasik (superblock, inode, directory entry, bitmap allocator) sebagaimana dijelaskan pada dokumentasi ext2 dan VFS Linux [1][2], namun dalam versi jauh lebih kecil: root-only, direct block saja (tanpa indirect block), maksimal 32 inode, maksimal 8 direct block per file (batas ukuran file 4096 byte).

### 6.2 Layout On-Disk MCSFS1

| LBA | Isi | Keterangan |
|---:|---|---|
| 0 | Superblock | Magic, version, block size, block count, lokasi metadata |
| 1 | Inode bitmap | Bit inode aktif. Inode 1 adalah root |
| 2 | Block bitmap | Bit block aktif. Block 0-7 reserved |
| 3-6 | Inode table | 32 inode; tiap inode menyimpan mode, links, size, direct blocks |
| 7 | Root directory block | Maksimal 16 directory entry |
| 8..N | Data blocks | Data file regular |

Block size = 512 byte, batas ukuran file per inode = `8 × 512 = 4096` byte.

### 6.3 Invariant Utama (I15-01 s.d. I15-10)

| ID | Invariant | Alasan |
|---|---|---|
| I15-01 | `super.magic == MCSFS1_MAGIC` dan `super.version == 1` | Mount tidak boleh menerima format asing |
| I15-02 | `block_size == 512` dan `block_count == dev->block_count` | Driver dan filesystem sepakat ukuran/range |
| I15-03 | Root inode = inode 1, bertipe directory, `direct[0]` menunjuk block 7 | Root directory adalah anchor namespace |
| I15-04 | Block metadata 0-7 ditandai used pada block bitmap | Metadata tidak boleh dialokasikan untuk data file |
| I15-05 | Directory entry aktif harus menunjuk inode aktif | Nama file tidak boleh menunjuk inode bebas |
| I15-06 | File inode bertipe file, size ≤ 4096 byte | Direct-only filesystem, tidak mendukung ukuran lebih besar |
| I15-07 | Direct block file berada pada range data block dan bitnya used | Mencegah pembacaan metadata sebagai data |
| I15-08 | Nama file tidak kosong, tidak memuat `/`, maksimal 27 byte | Root-only flat namespace |
| I15-09 | Operasi metadata sukses harus flush eksplisit | Mengurangi risiko stale metadata |
| I15-10 | Source freestanding tidak memakai hosted libc | Kernel belum memiliki libc |

### 6.4 Keterbatasan yang Disengaja

M15 **belum** menyediakan journaling, ordered mode, copy-on-write, fsync POSIX penuh, multi-directory, permission model, hard link, symbolic link, page cache, atau recovery setelah power-loss arbitrer. Ini adalah keterbatasan yang **disengaja** oleh desain panduan, bukan bug.

---

## 7. Lingkungan Praktikum

### 7.1 Host dan Target

| Komponen | Nilai |
|---|---|
| Host OS | Windows 11 x64 |
| Lingkungan build | WSL 2, distro `JotDesu`, Ubuntu 26.04 LTS (codename resolute) |
| Kernel WSL | Linux 6.6.87.2-microsoft-standard-WSL2 |
| Target ISA | x86_64 |
| Target ABI (kernel) | `x86_64-unknown-none-elf` |
| Target ABI (host test) | hosted C17 (native Linux/WSL) |
| Emulator | QEMU system-x86_64 versi 10.2.1 |
| Firmware emulator | OVMF (paket `ovmf` 2025.11-3ubuntu7), path `/usr/share/ovmf/OVMF.fd` |
| Build system | GNU Make 4.4.1 dengan `.RECIPEPREFIX := >` kustom |
| Bahasa utama | C17 freestanding (kernel), C17 hosted (host test) |
| Compiler | Ubuntu clang version 21.1.8 (6ubuntu1) |
| Linker | GNU ld (Binutils 2.46) untuk MCSFS1 relocatable object; `ld.lld` untuk kernel image |

### 7.2 Versi Toolchain

Bukti screenshot: `C:\Users\Ajot\Pictures\M15\Screenshot 2026-07-10 153320.png` (Halaman 1)

Perintah yang dijalankan:

```bash
mkdir -p artifacts/m15
{ uname -a; lsb_release -a 2>/dev/null || cat /etc/os-release; } | tee artifacts/m15/host_info.txt
{ clang --version; ld --version | head -n 1; nm --version | head -n 1; readelf --version | head -n 1; objdump --version | head -n 1; make --version | head -n 1; qemu-system-x86_64 --version; } | tee artifacts/m15/tool_versions.txt
```

Output aktual (disimpan di `artifacts/m15/host_info.txt` dan `artifacts/m15/tool_versions.txt`):

```text
Linux JotDesu 6.6.87.2-microsoft-standard-WSL2 #1 SMP PREEMPT_DYNAMIC Thu Jun 5 18:30:46 UTC 2025 x86_64 GNU/Linux
Distributor ID: Ubuntu
Description:    Ubuntu 26.04 LTS
Release:        26.04
Codename:       resolute
Ubuntu clang version 21.1.8 (6ubuntu1)
Target: x86_64-pc-linux-gnu
GNU ld (GNU Binutils for Ubuntu) 2.46
GNU nm (GNU Binutils for Ubuntu) 2.46
GNU readelf (GNU Binutils for Ubuntu) 2.46
GNU objdump (GNU Binutils for Ubuntu) 2.46
GNU Make 4.4.1
QEMU emulator version 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3)
```

Semua tool ditemukan; `qemu-system-x86_64` tersedia sehingga QEMU smoke test dapat dijalankan tanpa penundaan.

### 7.3 Lokasi Repository

| Item | Nilai |
|---|---|
| Path repository di WSL | `/home/andianaaji/src/mcsos` |
| Apakah berada di filesystem Linux WSL, bukan `/mnt/c` | Ya |
| Branch kerja | `praktikum-m15-mcsfs1` |
| Branch dasar | `praktikum-m14-block-device` |
| Commit hash awal | `78a89a6` |
| Commit hash akhir | `9a026c8` |

---

## 8. Repository dan Struktur File

### 8.1 Struktur Tambahan Setelah M15

Bukti screenshot: `C:\Users\Ajot\Pictures\M15\Screenshot 2026-07-10 153320.png` (Halaman 3)

```text
mcsos/
├── fs/
│   └── mcsfs1/
│       ├── mcsfs1.h
│       └── mcsfs1.c
├── tests/
│   └── m15/
│       └── test_mcsfs1.c
├── artifacts/
│   └── m15/
│       ├── host_info.txt
│       ├── preflight.txt
│       ├── tool_versions.txt
│       └── qemu_serial.txt
├── scripts/
│   └── m15_preflight.sh
├── build/                     (gitignored, artefak reproducible)
│   ├── mcsfs1.o
│   ├── test_mcsfs1_host
│   ├── test_m15_mcsfs1.log
│   ├── mcsfs1.undefined.txt
│   ├── mcsfs1.readelf.header.txt
│   ├── mcsfs1.objdump.txt
│   ├── m15.sha256.txt
│   └── mcsos.iso
└── Makefile (dimodifikasi: target check-m15 ditambahkan)
```

### 8.2 Penyesuaian terhadap Konvensi Repository

Panduan M15 generik mengasumsikan struktur `artifacts/m15/*` dan target Makefile `m15-all`. Repository mahasiswa **sudah memiliki pola sendiri** sejak M0-M14: target `check-mX`, output ke `$(BUILD_DIR)` (=`build/`), variabel `$(CC)`, `$(HOSTCC)`, `$(NM)`, `$(READELF)`, `$(OBJDUMP)`, serta `.RECIPEPREFIX := >` (bukan tab). Untuk menjaga konsistensi dan tidak merusak target M0-M14, target M15 disusun sebagai **`check-m15`** mengikuti pola `check-m14` persis (lihat Bagian 10.6).

---

## 9. Desain Teknis

### 9.1 Struktur Data On-Disk

```c
struct mcsfs1_super_disk {
    uint32_t magic, version, block_size, block_count, inode_count;
    uint32_t inode_bmap_lba, block_bmap_lba, inode_table_lba, inode_table_blocks;
    uint32_t root_ino, root_dir_lba, data_start_lba, clean;
    uint32_t reserved[115];
};

struct mcsfs1_inode_disk {
    uint16_t mode, links;
    uint32_t size;
    uint32_t direct[MCSFS1_DIRECT_BLOCKS]; /* 8 direct block */
    uint32_t reserved[5];
};

struct mcsfs1_dirent_disk {
    uint32_t ino;
    uint8_t type;
    char name[MCSFS1_MAX_NAME]; /* 27 byte */
};
```

### 9.2 API Publik (`fs/mcsfs1/mcsfs1.h`)

```c
int mcsfs1_format(struct mcsfs1_blkdev *dev);
int mcsfs1_mount(struct mcsfs1_mount *mnt, struct mcsfs1_blkdev *dev);
int mcsfs1_fsck(struct mcsfs1_blkdev *dev);
int mcsfs1_create(struct mcsfs1_mount *mnt, const char *name);
int mcsfs1_write(struct mcsfs1_mount *mnt, const char *name, const uint8_t *buf, uint32_t len);
int mcsfs1_read(struct mcsfs1_mount *mnt, const char *name, uint8_t *buf, uint32_t cap, uint32_t *out_len);
int mcsfs1_unlink(struct mcsfs1_mount *mnt, const char *name);
```

Interface block device diabstraksi lewat `struct mcsfs1_blkdev` berisi pointer fungsi `read`, `write`, `flush` — sehingga MCSFS1 tidak bergantung pada implementasi block device tertentu (dapat dipasangkan dengan RAM-backed device pada host test, atau block layer M14 pada kernel).

### 9.3 Helper Freestanding

Karena kernel belum memiliki hosted libc, source memakai helper lokal: `mcsfs_memset`, `mcsfs_memcpy`, `mcsfs_memcmp`, `mcsfs_strlen_bound` — semuanya diimplementasikan dari nol tanpa memanggil fungsi libc apa pun (memenuhi invariant I15-10).

### 9.4 Ownership dan Lifetime

`struct mcsfs1_mount` tidak memiliki (own) block device; ia hanya menyimpan pointer pinjaman ke `struct mcsfs1_blkdev`. Buffer 512 byte lokal hidup hanya selama stack frame fungsi. Directory entry dan inode on-disk tidak dipakai sebagai pointer jangka panjang — setiap operasi membaca ulang metadata dari device.

### 9.5 Concurrency Model

MCSFS1 M15 diasumsikan single-core atau dilindungi lock eksternal dari VFS/filesystem layer; tidak ada mutex internal pada source M15.

---

## 10. Langkah Kerja Implementasi

### 10.1 Preflight dan Pemeriksaan Readiness M0-M14

Bukti screenshot: `C:\Users\Ajot\Pictures\M15\Screenshot 2026-07-10 153320.png` (Halaman 2)

```bash
mkdir -p scripts artifacts/m15
cat > scripts/m15_preflight.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
mkdir -p artifacts/m15
{
  echo "== git =="
  git status --short || true
  git rev-parse --short HEAD || true
  echo "== toolchain =="
  clang --version | head -n 1
  ld --version | head -n 1
  nm --version | head -n 1
  readelf --version | head -n 1
  objdump --version | head -n 1
  make --version | head -n 1
  echo "== prior artifacts =="
  for d in m0 m1 m2 m3 m4 m5 m6 m7 m8 m9 m10 m11 m12 m13 m14; do
    if [ -d "artifacts/$d" ]; then
      echo "artifacts/$d: present"
    else
      echo "artifacts/$d: missing"
    fi
  done
} | tee artifacts/m15/preflight.txt
EOF
chmod +x scripts/m15_preflight.sh
./scripts/m15_preflight.sh
```

Hasil: `artifacts/m0` s.d. `artifacts/m13` **missing**, hanya `artifacts/m14` **present**. Karena M15 secara langsung membutuhkan M14 (block device layer) sebagai prasyarat, dan itu tersedia, praktikum dilanjutkan dengan catatan limitasi ini (lihat Bagian 14.3, Catatan #1).

### 10.2 Branch Kerja dan Struktur Direktori

Bukti screenshot: `C:\Users\Ajot\Pictures\M15\Screenshot 2026-07-10 153320.png` (Halaman 3)

```bash
git switch -c praktikum-m15-mcsfs1
mkdir -p fs/mcsfs1 tests/m15 artifacts/m15
```

### 10.3 Header `fs/mcsfs1/mcsfs1.h`

Bukti screenshot: `C:\Users\Ajot\Pictures\M15\Screenshot 2026-07-10 153320.png` (Halaman 3)

Header berisi konstanta format (`MCSFS1_BLOCK_SIZE=512`, `MCSFS1_MAGIC`, `MCSFS1_MAX_INODES=32`, `MCSFS1_DIRECT_BLOCKS=8`, `MCSFS1_MAX_NAME=27`), 9 kode error (`MCSFS1_ERR_OK` s.d. `MCSFS1_ERR_RANGE`), `struct mcsfs1_blkdev`, `struct mcsfs1_mount`, dan deklarasi 7 fungsi API.

### 10.4 Implementasi `fs/mcsfs1/mcsfs1.c` (654 baris)

Bukti screenshot: `C:\Users\Ajot\Pictures\M15\Screenshot 2026-07-10 153320.png` (Halaman 4–14)

Implementasi dibangun bertahap sesuai urutan berikut, seluruhnya dalam satu file `fs/mcsfs1/mcsfs1.c`:

| Bagian fungsi | Halaman bukti | Ringkasan |
|---|---|---|
| Struct disk layout, `mcsfs_memset`, `mcsfs_memcpy` | Halaman 4 | Definisi `mcsfs1_super_disk`, `mcsfs1_inode_disk`, `mcsfs1_dirent_disk`, helper memset/memcpy freestanding |
| `mcsfs_memcmp`, `mcsfs_strlen_bound`, `valid_name`, `dev_read`, `dev_write`, `dev_flush` (awal) | Halaman 5 | Validasi nama (I15-08), wrapper device I/O dengan pengecekan bound |
| `dev_flush` (akhir), `bit_set/clear/test`, `load_super`, `read_inode` | Halaman 6 | Bitmap primitives, validasi superblock (I15-01, I15-02) |
| `write_inode`, `load_bmaps`, `store_bmaps`, `find_dirent` | Halaman 7 | Penulisan inode ke inode table, pencarian nama di root directory |
| `alloc_inode_block`, `alloc_data_block` | Halaman 8 | Alokasi inode bebas (mulai inode 2, karena inode 1 = root) dan block data bebas |
| `free_inode_and_blocks`, `mcsfs1_format` (awal) | Halaman 9 | Pembebasan resource saat unlink; inisialisasi superblock saat format |
| `mcsfs1_format` (akhir), `mcsfs1_mount`, `mcsfs1_create` (awal) | Halaman 10 | Reservasi block metadata 0-7 (I15-04), root inode (I15-03), validasi mount |
| `mcsfs1_create` (akhir), `mcsfs1_write` | Halaman 11 | Alokasi direct block bertahap, chunking penulisan per 512 byte |
| `mcsfs1_read` | Halaman 12 | Pembacaan multi-block dengan validasi range (`cap < inode.size`) |
| `mcsfs1_unlink` | Halaman 13 | Penghapusan dirent, pembebasan inode + block, zero-fill inode |
| `mcsfs1_fsck` | Halaman 14 | Pemeriksaan invariant I15-01 s.d. I15-07 secara berurutan |

Verifikasi ukuran file setelah heredoc selesai:

```bash
wc -l fs/mcsfs1/mcsfs1.c
```

```text
654 fs/mcsfs1/mcsfs1.c
```

### 10.5 Host Unit Test `tests/m15/test_mcsfs1.c`

Bukti screenshot: `C:\Users\Ajot\Pictures\M15\Screenshot 2026-07-10 153320.png` (Halaman 15)

Test memakai RAM-backed block device 128 block × 512 byte (`ram_read`, `ram_write`, `ram_flush` dengan array statis `disk[128][512]`), dan memverifikasi urutan skenario: `format` → `mount` → `fsck-empty` → `create-alpha` → `create-duplicate` (harus `EXIST`) → `write-alpha` (34 byte) → `read-alpha` (harus identik) → `write-big`/`read-big` (1400 byte, multi-block) → `read-small-cap` (harus `RANGE`) → `missing` (harus `NOENT`) → `fsck-populated` → `unlink` → `read-after-unlink` (harus `NOENT`) → `fsck-after-unlink` → `corrupt-super` (flip 1 bit pada magic, harus `CORRUPT`) → cek `flush_count > 0`.

### 10.6 Target Makefile `check-m15`

Bukti screenshot: `C:\Users\Ajot\Pictures\M15\Screenshot 2026-07-10 153320.png` (Halaman 16–17)

Sebelum menambah target, dilakukan inspeksi Makefile eksisting untuk memastikan tidak merusak target M0-M14:

```bash
grep -n "^CC\|^HOST_CFLAGS\|^FREESTANDING_CFLAGS\|^\.PHONY\|^clean:\|m14-all\|^all:" Makefile
sed -n '1,35p' Makefile
sed -n '190,230p' Makefile
```

Ditemukan Makefile memakai `.RECIPEPREFIX := >`, variabel `$(CC):=clang`, `$(HOSTCC):=clang`, `$(LD):=ld.lld`, `$(BUILD_DIR):=build`, dan pola target `check-m14` yang lengkap dengan audit `nm -u`, `readelf -h`, `objdump -dr`, `sha256sum`. Target `check-m15` disusun mengikuti pola tersebut persis:

```makefile
.PHONY: check-m15
check-m15: $(BUILD_DIR)/mcsfs1.o $(BUILD_DIR)/test_mcsfs1_host
>./$(BUILD_DIR)/test_mcsfs1_host | tee $(BUILD_DIR)/test_m15_mcsfs1.log
>grep -q 'M15 host test passed' $(BUILD_DIR)/test_m15_mcsfs1.log
>$(NM) -u $(BUILD_DIR)/mcsfs1.o | tee $(BUILD_DIR)/mcsfs1.undefined.txt
>test ! -s $(BUILD_DIR)/mcsfs1.undefined.txt
>$(READELF) -h $(BUILD_DIR)/mcsfs1.o > $(BUILD_DIR)/mcsfs1.readelf.header.txt
>grep -q 'Machine:[[:space:]]*Advanced Micro Devices X86-64' $(BUILD_DIR)/mcsfs1.readelf.header.txt
>$(OBJDUMP) -dr $(BUILD_DIR)/mcsfs1.o > $(BUILD_DIR)/mcsfs1.objdump.txt
>sha256sum $(BUILD_DIR)/mcsfs1.o $(BUILD_DIR)/test_mcsfs1_host > $(BUILD_DIR)/m15.sha256.txt
>@echo "[PASS] M15 static check selesai"
$(BUILD_DIR)/mcsfs1.o: fs/mcsfs1/mcsfs1.c fs/mcsfs1/mcsfs1.h
>mkdir -p $(BUILD_DIR)
>$(CC) $(CFLAGS) -c fs/mcsfs1/mcsfs1.c -o $(BUILD_DIR)/mcsfs1.o
$(BUILD_DIR)/test_mcsfs1_host: tests/m15/test_mcsfs1.c fs/mcsfs1/mcsfs1.c fs/mcsfs1/mcsfs1.h
>mkdir -p $(BUILD_DIR)
>$(HOSTCC) -std=c17 -Wall -Wextra -Werror -Ifs/mcsfs1 \
>  tests/m15/test_mcsfs1.c fs/mcsfs1/mcsfs1.c -o $(BUILD_DIR)/test_mcsfs1_host
```

Catatan penting: karakter `>` di awal baris resep bukan redirect shell, melainkan `.RECIPEPREFIX` kustom milik Makefile repository ini.

### 10.7 Fault Injection Tambahan

Bukti screenshot: `C:\Users\Ajot\Pictures\M15\Screenshot 2026-07-10 153320.png` (Halaman 18)

Selain `corrupt-super` bawaan panduan, ditambahkan fault injection **"nama terlalu panjang"** (invariant I15-08) dengan skrip Python untuk menyisipkan kode secara presisi sebelum blok penutup test:

```c
char long_name[64];
for (unsigned i = 0; i < sizeof(long_name) - 1; i++) long_name[i] = 'a';
long_name[sizeof(long_name) - 1] = '\0';
fails += expect_int("create-name-too-long", mcsfs1_create(&mnt, long_name), MCSFS1_ERR_NAMETOOLONG);
```

### 10.8 Commit Git

Bukti screenshot: `C:\Users\Ajot\Pictures\M15\Screenshot 2026-07-10 153320.png` (Halaman 19)

```bash
git add fs/mcsfs1 tests/m15 Makefile artifacts/m15 scripts/m15_preflight.sh
git commit -m "M15: add MCSFS1 minimal persistent filesystem"
```

Hasil: commit `a849b88`, 8 file berubah, 891 baris ditambahkan.

### 10.9 Build ISO (`make image`)

Bukti screenshot: `C:\Users\Ajot\Pictures\M15\Screenshot 2026-07-10 153320.png` (Halaman 20, 22)

```bash
make image
```

Proses: `iso_root` disusun (kernel.elf, `limine.conf`, `limine-bios.sys`, `limine-bios-cd.bin`, `limine-uefi-cd.bin`, `BOOTX64.EFI`) → `xorriso -as mkisofs` membuat ISO hybrid (BIOS + UEFI, GPT) → `limine bios-install` menautkan bootloader stage 2 → checksum disimpan. Hasil: `build/mcsos.iso` 6.682.624 byte.

### 10.10 Verifikasi OVMF dan Konvensi Boot

Bukti screenshot: `C:\Users\Ajot\Pictures\M15\Screenshot 2026-07-10 153320.png` (Halaman 21)

```bash
ls -la /usr/share/ovmf/OVMF.fd
dpkg -l | grep -i ovmf
grep -n -i "ovmf\|uefi\|bios\|limine" Makefile | head -20
ls iso_root
```

Ditemukan path OVMF sistem berbeda dari asumsi panduan generik (`/usr/share/OVMF/OVMF_CODE.fd`); path aktual adalah `/usr/share/ovmf/OVMF.fd` (paket `ovmf` 2025.11-3ubuntu7). Repository memakai bootloader **Limine** dengan dukungan hybrid BIOS + UEFI.

### 10.11 QEMU Smoke Test

Bukti screenshot: `C:\Users\Ajot\Pictures\M15\Screenshot 2026-07-10 153320.png` (Halaman 23)

```bash
mkdir -p artifacts/m15
timeout 20 qemu-system-x86_64 \
  -machine q35 \
  -m 256M \
  -serial file:artifacts/m15/qemu_serial.log \
  -display none \
  -no-reboot \
  -no-shutdown \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/ovmf/OVMF.fd \
  -cdrom build/mcsos.iso
```

`timeout 20` diperlukan karena kernel masuk *tick loop* scheduler (M9) tanpa henti; exit code `124` (`terminating on signal 15 from pid ... (timeout)`) adalah perilaku **normal**, bukan crash.

### 10.12 Penyimpanan Bukti QEMU sebagai Evidence Git

Bukti screenshot: `C:\Users\Ajot\Pictures\M15\Screenshot 2026-07-10 153320.png` (Halaman 24)

Karena `.gitignore` repository memiliki rule `*.log` (dengan pengecualian `evidence/**/*.log`, konvensi lama M7-M13 yang sudah ditinggalkan sejak M14), log QEMU M15 disalin ke ekstensi `.txt` (mengikuti pola `gdb_m14_session.txt` pada M14) agar dapat ter-*commit*:

```bash
cp artifacts/m15/qemu_serial.log artifacts/m15/qemu_serial.txt
git add artifacts/m15/qemu_serial.txt
git commit -m "M15: tambahkan bukti QEMU smoke test (baseline M14, MCSFS1 belum ditautkan ke kernel)"
```

Hasil: commit `9a026c8`, 1 file berubah, 76 baris ditambahkan.

---

## 11. Checkpoint Buildable

| Checkpoint | Perintah | Evidence | Status |
|---|---|---|---|
| CP15-1 Preflight | `./scripts/m15_preflight.sh` | `artifacts/m15/preflight.txt` | Lulus (M14 present, M0-M13 missing & dicatat) |
| CP15-2 Host compile | `make check-m15` | `build/test_mcsfs1_host` terbentuk | Lulus |
| CP15-3 Host test | `./build/test_mcsfs1_host` | `M15 host test passed: flush_count=5` | Lulus, identik 2× build |
| CP15-4 Freestanding object | `make check-m15` | `build/mcsfs1.o` (x86_64, ffreestanding) terbentuk | Lulus |
| CP15-5 Relocatable/object link | N/A (kernel Makefile tidak memakai `ld -r` terpisah untuk M15) | `build/mcsfs1.o` | Lulus (sebagai object; belum ditautkan ke `kernel.elf`) |
| CP15-6 Undefined symbol audit | `nm -u build/mcsfs1.o` | `build/mcsfs1.undefined.txt` kosong | Lulus |
| CP15-7 ELF audit | `readelf -h build/mcsfs1.o` | `Machine: Advanced Micro Devices X86-64` | Lulus |
| CP15-8 Disassembly audit | `objdump -dr build/mcsfs1.o` | `build/mcsfs1.objdump.txt` | Lulus |
| CP15-9 Checksum | `sha256sum build/mcsfs1.o build/test_mcsfs1_host` | `build/m15.sha256.txt` | Lulus |
| CP15-10 QEMU smoke | perintah pada Bagian 10.11 | `artifacts/m15/qemu_serial.txt` | Lulus (boot bersih M3-M14, tanpa regression) |

---

## 12. Perintah Uji dan Validasi

### 12.1 Build Pertama (Clean Checkout)

```bash
make check-m15
```

```text
mkdir -p build
clang --target=x86_64-unknown-none-elf -std=c17 -ffreestanding -fno-builtin -fno-stack-protector \
  -fno-stack-check -fno-pic -fno-pie -fno-lto -m64 -march=x86-64 -mabi=sysv -mno-red-zone -mno-mmx \
  -mno-sse -mno-sse2 -mcmodel=kernel -Wall -Wextra -Werror -Ikernel/arch/x86_64/include \
  -Ikernel/include -Iinclude -c fs/mcsfs1/mcsfs1.c -o build/mcsfs1.o
clang -std=c17 -Wall -Wextra -Werror -Ifs/mcsfs1 tests/m15/test_mcsfs1.c fs/mcsfs1/mcsfs1.c -o build/test_mcsfs1_host
./build/test_mcsfs1_host | tee build/test_m15_mcsfs1.log
M15 host test passed: flush_count=5
grep -q 'M15 host test passed' build/test_m15_mcsfs1.log
nm -u build/mcsfs1.o | tee build/mcsfs1.undefined.txt
test ! -s build/mcsfs1.undefined.txt
readelf -h build/mcsfs1.o > build/mcsfs1.readelf.header.txt
grep -q 'Machine:[[:space:]]*Advanced Micro Devices X86-64' build/mcsfs1.readelf.header.txt
objdump -dr build/mcsfs1.o > build/mcsfs1.objdump.txt
sha256sum build/mcsfs1.o build/test_mcsfs1_host > build/m15.sha256.txt
[PASS] M15 static check selesai
```

### 12.2 Rebuild dari Kondisi Bersih (Verifikasi Tidak Ada Dependensi Tersembunyi)

```bash
rm -rf build/mcsfs1.o build/test_mcsfs1_host build/test_m15_mcsfs1.log build/mcsfs1.undefined.txt \
  build/mcsfs1.readelf.header.txt build/mcsfs1.objdump.txt build/m15.sha256.txt
make check-m15
```

Hasil **identik** dengan build pertama: `M15 host test passed: flush_count=5`, seluruh audit lulus kembali. Ini membuktikan tidak ada dependensi tersembunyi pada build M15 (setara dengan tujuan Langkah 13.7 panduan).

### 12.3 Build Setelah Fault Injection Tambahan

```bash
rm -f build/mcsfs1.o build/test_mcsfs1_host build/test_m15_mcsfs1.log build/mcsfs1.undefined.txt \
  build/mcsfs1.readelf.header.txt build/mcsfs1.objdump.txt build/m15.sha256.txt
make check-m15
```

Hasil: `M15 host test passed: flush_count=5` (tetap sama — fault injection `create-name-too-long` gagal validasi *sebelum* mencapai `dev_flush`, sehingga `flush_count` tidak bertambah). Tidak ada baris `FAIL` sama sekali.

---

## 13. Hasil Uji

### 13.1 Isi Log Host Test (`build/test_m15_mcsfs1.log`)

```text
M15 host test passed: flush_count=5
```

Log hanya berisi satu baris karena fungsi `expect_int()` hanya mencetak `FAIL ...` ketika hasil aktual berbeda dari yang diharapkan. Seluruh 15 assertion (`format`, `mount`, `fsck-empty`, `create-alpha`, `create-duplicate`, `write-alpha`, `read-alpha`, `write-big`, `read-big`, `read-small-cap`, `missing`, `fsck-populated`, `unlink`, `read-after-unlink`, `fsck-after-unlink`, `create-name-too-long`, `corrupt-super`) lulus tanpa satupun mencetak `FAIL`.

### 13.2 ELF Header `build/mcsfs1.o`

Diverifikasi dengan `grep -q 'Machine:[[:space:]]*Advanced Micro Devices X86-64'` — lulus, artinya object berhasil dikompilasi untuk arsitektur x86-64 dengan flag freestanding lengkap (`-ffreestanding -fno-builtin -mno-red-zone -mcmodel=kernel`, dst).

### 13.3 Undefined Symbol Audit

```bash
nm -u build/mcsfs1.o | tee build/mcsfs1.undefined.txt
test ! -s build/mcsfs1.undefined.txt
```

Hasil: file kosong, perintah `test ! -s` lulus tanpa error → tidak ada pemanggilan fungsi eksternal (libc atau lainnya) dari `mcsfs1.o`, memenuhi invariant I15-10.

### 13.4 QEMU Smoke Test — Log Serial

Bukti screenshot: `C:\Users\Ajot\Pictures\M15\Screenshot 2026-07-10 153320.png` (Halaman 23)

Cuplikan log (`artifacts/m15/qemu_serial.txt`):

```text
limine: Loading executable `boot():/boot/kernel.elf`...
MCSOS 260502 M3 kernel entered
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
[MCSOS:M11] elf: plan ok
[MCSOS:M11] user image plan ready
[MCSOS:M12] sync selftest passed
[MCSOS:M13] vfs/fd/ramfs self-test PASS
[MCSOS:M14] block layer initialized
[MCSOS:M14] ram0 read/write self-test PASS
[MCSOS:M9] scheduler initialized
[MCSOS:M9] thread A tick
[MCSOS:M9] thread B tick
... (tick loop berlanjut sampai timeout 20 detik)
[MCSOS:TIMER] ticks=0x00000000000005dc
```

Seluruh milestone M3 s.d. M14 berjalan bersih secara berurutan, termasuk **VFS/FD/RAMFS self-test PASS (M13)** dan **block layer + ram0 self-test PASS (M14)**. Tidak ditemukan baris `[MCSOS:M15]` — sesuai ekspektasi karena `fs/mcsfs1/mcsfs1.c` belum ditautkan ke `kernel.elf` (lihat Bagian 14.3, Catatan #3).

---

## 14. Analisis Teknis

### 14.1 Analisis Keberhasilan

1. **MCSFS1 berhasil diimplementasikan penuh:** ketujuh operasi (`format`, `mount`, `fsck`, `create`, `write`, `read`, `unlink`) berjalan sesuai kontrak API.
2. **Freestanding murni:** tidak ada satu pun panggilan hosted libc (`nm -u` kosong), semua operasi memori memakai helper lokal.
3. **Host test komprehensif:** 15 skenario tercakup, termasuk multi-block write/read (1400 byte), duplicate detection, range error, missing file, dan dua fault injection (`corrupt-super`, `create-name-too-long`).
4. **Reproducibility terbukti:** dua kali build dari kondisi bersih menghasilkan `flush_count=5` yang identik.
5. **Tidak ada boot regression:** QEMU smoke test menunjukkan seluruh milestone M3-M14 tetap berjalan normal setelah penambahan source M15.

### 14.2 Invariant yang Dibuktikan Host Unit Test

| Invariant | Bukti dari test |
|---|---|
| I15-01/I15-02 (magic/version/block_size valid) | `format` + `mount` sukses; `corrupt-super` gagal `MCSFS1_ERR_CORRUPT` setelah byte magic diubah |
| I15-03/I15-04 (root inode & metadata block 0-7 reserved) | `fsck-empty` lulus tepat setelah format |
| I15-05 (dirent aktif → inode aktif) | `fsck-populated` dan `fsck-after-unlink` lulus |
| I15-06/I15-07 (size ≤4096, direct block valid & used) | `write-big` (1400 byte, multi-block), `read-big` mengembalikan data identik |
| I15-08 (nama valid) | Fault injection `create-name-too-long` |
| I15-09 (flush eksplisit) | `flush_count=5 > 0` di akhir test |

### 14.3 Catatan Limitasi dan Penyesuaian (Wajib Dicatat)

1. **Artefak M0-M13 tidak tersedia** di repository saat preflight M15 dijalankan (hanya `artifacts/m14` yang *present*). M15 dilanjutkan atas dasar M14 sebagai prasyarat langsung yang tersedia dan valid.
2. **Makefile memakai pola `check-mX` milik repository sendiri**, bukan `m15-all` generik dari dokumen panduan, termasuk `.RECIPEPREFIX := >` khusus dan penamaan variabel `$(CC)`, `$(HOSTCC)`, `$(NM)`, dst.
3. **QEMU smoke test M15 adalah baseline M14** — `fs/mcsfs1/mcsfs1.c` belum ditautkan ke kernel (`SRC_C` di Makefile mencari `.c` hanya di bawah `kernel/`, tidak mencakup `fs/`), sehingga tidak ada log `[MCSOS:M15]` pada serial output. Integrasi kernel penuh adalah tugas pengayaan (bagian 22.2 panduan), belum dikerjakan pada laporan ini.
4. **Path OVMF berbeda dari asumsi panduan generik**: sistem memakai `/usr/share/ovmf/OVMF.fd`, bukan `/usr/share/OVMF/OVMF_CODE.fd`.
5. **Evidence log QEMU disimpan dengan ekstensi `.txt`**, bukan `.log`, karena `.gitignore` repository memiliki rule global `*.log` (kecuali `evidence/**/*.log`, konvensi lama yang sudah ditinggalkan sejak M14).

---

## 15. Debugging dan Failure Modes

### 15.1 Failure Modes yang Ditemukan Selama Praktikum

| Failure mode | Gejala | Penyebab | Perbaikan | Bukti |
|---|---|---|---|---|
| `find / -iname "OVMF_CODE*.fd"` menggantung | Perintah tidak kunjung selesai, terlihat seperti macet | `find /` menyisir seluruh filesystem termasuk mount point Windows (`/mnt/c`) via 9P/DrvFs yang lambat | Hentikan dengan `Ctrl+C`, ganti dengan pencarian terarah (`ls` path spesifik + `dpkg -l \| grep ovmf`) | Screenshot 2026-07-10 153320.png (Halaman 21) |
| `git add artifacts/m15/qemu_serial.log` diabaikan git | `The following paths are ignored ... hint: Use -f` | Rule `.gitignore` `*.log` | Salin file ke ekstensi `.txt` (`qemu_serial.txt`) mengikuti pola `gdb_m14_session.txt` milik M14 | Screenshot 2026-07-10 153320.png (Halaman 24) |
| Path OVMF tidak sesuai asumsi panduan | Perintah QEMU generik panduan akan gagal `file not found` bila dijalankan apa adanya | Paket `ovmf` di Ubuntu 26.04 menaruh firmware gabungan di `/usr/share/ovmf/OVMF.fd`, bukan `/usr/share/OVMF/OVMF_CODE.fd` | Sesuaikan argumen `-drive if=pflash,...,file=/usr/share/ovmf/OVMF.fd` | Screenshot 2026-07-10 153320.png (Halaman 21) |

### 15.2 Fault Injection yang Diantisipasi Panduan (Tabel Referensi)

| Fault | Cara simulasi | Ekspektasi | Status pengujian |
|---|---|---|---|
| Superblock magic rusak | Ubah byte pada block 0 | `mcsfs1_fsck` → `MCSFS1_ERR_CORRUPT` | Diuji (`corrupt-super`, lulus) |
| Nama terlalu panjang | `mcsfs1_create` dengan nama >27 byte | `MCSFS1_ERR_NAMETOOLONG` | Diuji (`create-name-too-long`, lulus) |
| Root inode salah mode | Ubah mode root menjadi file | `mount`/`fsck` gagal | Belum diuji eksplisit (di luar cakupan wajib) |
| Directory entry menunjuk inode bebas | Hapus bit inode, biarkan dirent | `fsck` gagal | Belum diuji eksplisit |
| Direct block keluar range | Ubah direct block ke `block_count + 1` | `fsck` gagal | Belum diuji eksplisit |
| File terlalu besar | `mcsfs1_write` dengan len >4096 | `MCSFS1_ERR_RANGE` | Belum diuji eksplisit (tercakup implisit lewat batas `MCSFS1_DIRECT_BLOCKS * MCSFS1_BLOCK_SIZE`) |
| Directory penuh | Buat lebih dari 16 file | `MCSFS1_ERR_NOSPC` | Belum diuji eksplisit |

### 15.3 Triage yang Dilakukan

Urutan diagnosis yang dipakai selama praktikum:
1. Jalankan `./scripts/m15_preflight.sh` untuk memastikan toolchain dan artefak prasyarat.
2. Build dari kondisi bersih: hapus artefak M15 di `build/`, jalankan `make check-m15`.
3. Periksa log host test: `cat build/test_m15_mcsfs1.log`.
4. Audit object: `nm -u`, `readelf -h`, `objdump -dr` pada `build/mcsfs1.o`.
5. Bila menambah kode: verifikasi penyisipan dengan `grep -n -A2 -B2` sebelum build ulang.
6. Untuk QEMU: jalankan dengan `timeout N` agar tidak menggantung pada tick loop scheduler M9.

---

## 16. Prosedur Rollback

| Skenario rollback | Perintah | Data yang harus diselamatkan | Status |
|---|---|---|---|
| Kembali ke commit M14 | `git switch praktikum-m14-block-device` atau `git checkout 78a89a6` | Log/test M15 (belum ada risiko karena M15 di branch terpisah) | Tersedia, belum perlu dijalankan |
| Simpan percobaan gagal sebelum restore | `git diff > artifacts/m15/m15_failed_attempt.diff` | Diff perubahan | Tidak diperlukan (tidak ada percobaan gagal fatal) |
| Bersihkan hanya artefak M15 | `rm -rf build/mcsfs1.o build/test_mcsfs1_host build/test_m15_mcsfs1.log build/mcsfs1.undefined.txt build/mcsfs1.readelf.header.txt build/mcsfs1.objdump.txt build/m15.sha256.txt` | Source `fs/mcsfs1`, `tests/m15` aman | Teruji (dipakai pada Bagian 12.2 dan 12.3) |
| Regenerasi image | `make image` | Image lama jika perlu | Teruji |

Catatan: `make clean` penuh **dihindari** secara sengaja karena berisiko menghapus seluruh `build/` termasuk artefak M0-M13 yang statusnya sudah *missing* di repository (lihat Bagian 14.3, Catatan #1) dan mungkin tidak bisa di-*rebuild* ulang dengan mudah.

---

## 17. Keamanan dan Reliability

### 17.1 Validasi Input

MCSFS1 memvalidasi nama file (`valid_name`): tidak boleh kosong, tidak boleh memuat karakter `/`, dan maksimal 27 byte (I15-08) — dibuktikan lewat fault injection `create-name-too-long`. Semua akses device (`dev_read`, `dev_write`) memeriksa `lba >= dev->block_count` sebelum memanggil fungsi read/write aktual, mencegah out-of-bound access ke block device.

### 17.2 Reliability dan Batasan Crash Consistency

MCSFS1 **belum** crash-consistent terhadap power-loss arbitrer. Tidak ada journaling, ordered-write, maupun copy-on-write — urutan tulis metadata (bitmap → inode → dirent) bisa terpotong di tengah jika sistem mati mendadak, berpotensi meninggalkan metadata dalam kondisi tidak konsisten (misalnya dirent menunjuk inode yang belum tertulis penuh). Yang tersedia hanya:
- **Flush eksplisit** setelah setiap operasi metadata sukses (I15-09, dibuktikan `flush_count=5`).
- **Fsck-lite** untuk **mendeteksi** (bukan memperbaiki secara otomatis) pelanggaran invariant setelah restart.

### 17.3 Hubungan MCSFS1 ke VFS M13

VFS M13 seharusnya memanggil MCSFS1 sebagai *backend operations* di balik antarmuka file descriptor — VFS tidak perlu mengetahui detail superblock/bitmap/inode on-disk. Panggilan `open()`/`read()`/`write()`/`unlink()` pada level syscall diterjemahkan VFS menjadi pemanggilan `mcsfs1_create`/`mcsfs1_read`/`mcsfs1_write`/`mcsfs1_unlink`, sementara MCSFS1 sendiri meneruskan I/O fisik ke block layer M14 lewat `struct mcsfs1_blkdev`. Integrasi ini **belum dilakukan** pada laporan ini (tugas pengayaan).

---

## 18. Pembagian Kerja

Praktikum dikerjakan secara **individu**; tidak ada pembagian kerja kelompok.

| Nama | NIM | Peran | Kontribusi |
|---|---|---|---|
| Andiana Jamaludin Malik | 2583207073016 | Individu | Seluruh implementasi, pengujian, audit, QEMU smoke test, dan penyusunan laporan |

---

## 19. Tugas Wajib (Sesuai Bagian 22.1 Panduan) — Status Pemenuhan

| No. | Tugas Wajib | Status | Bukti |
|---|---|---|---|
| 1 | Implementasikan source MCSFS1 sesuai dokumen | Selesai | `fs/mcsfs1/mcsfs1.c` (654 baris), `fs/mcsfs1/mcsfs1.h` |
| 2 | Jalankan build lengkap dari clean checkout | Selesai (`make check-m15`, disesuaikan dari `make CC=clang m15-all`) | Bagian 12.1, 12.2 |
| 3 | Simpan seluruh artifact M15 | Selesai | `artifacts/m15/*`, `build/*` (gitignored, reproducible) |
| 4 | Tambahkan minimal 1 fault injection tambahan selain corrupt-super | Selesai | `create-name-too-long` (Bagian 10.7) |
| 5 | Jelaskan invariant yang dibuktikan host unit test | Selesai | Bagian 14.2 |
| 6 | Jelaskan batasan crash consistency MCSFS1 | Selesai | Bagian 17.2 |
| 7 | Jelaskan hubungan MCSFS1 ke VFS M13 | Selesai | Bagian 17.3 |
| 8 | Commit git dengan pesan `M15: add MCSFS1 minimal persistent filesystem` | Selesai | Commit `a849b88` |

Tambahan di luar tugas wajib: QEMU smoke test (CP15-10) dijalankan dan dicatat sebagai commit kedua `9a026c8`.

---

## 20. Pertanyaan Analisis (Sesuai Bagian 23 Panduan)

1. **Mengapa superblock harus memiliki magic number dan version?**
   Agar `mount` dapat menolak block device yang bukan berformat MCSFS1 atau berformat versi berbeda, mencegah salah interpretasi layout data sebagai filesystem yang valid (I15-01).

2. **Mengapa metadata block harus ditandai used pada block bitmap?**
   Supaya alokasi data file (`alloc_data_block`) tidak pernah memilih LBA 0-7 yang sudah dipakai superblock/bitmap/inode table/root directory, mencegah data file menimpa metadata (I15-04).

3. **Apa risiko jika directory entry ditulis sebelum inode selesai ditulis?**
   Dirent bisa menunjuk ke inode yang isinya belum valid (mode/size/direct block belum konsisten) jika terjadi crash di antara kedua penulisan — pelanggaran I15-05 yang baru terdeteksi saat `fsck` berikutnya.

4. **Apa risiko jika bitmap ditulis sebelum data block selesai ditulis?**
   Block dianggap "used" oleh bitmap padahal isinya belum valid; jika crash terjadi, block tersebut secara logis teralokasi namun datanya tidak terjamin benar.

5. **Mengapa M15 belum boleh disebut crash-consistent?**
   Karena tidak ada journaling/ordered-write/copy-on-write; urutan tulis metadata bisa terpotong power-loss arbitrer tanpa mekanisme pemulihan otomatis (lihat Bagian 17.2).

6. **Apa perbedaan fsck detection dan fsck repair?**
   `mcsfs1_fsck` pada M15 hanya **mendeteksi** pelanggaran invariant (mengembalikan `MCSFS1_ERR_CORRUPT`), tidak melakukan perbaikan otomatis (repair) seperti merekonstruksi bitmap atau menghapus orphan inode.

7. **Mengapa source freestanding tidak boleh memakai `printf` atau `malloc`?**
   Kernel belum memiliki hosted libc/heap runtime; `printf`/`malloc` bergantung pada layanan sistem operasi (stdio buffer, heap allocator OS) yang tidak tersedia pada tahap boot awal kernel pendidikan ini.

8. **Mengapa `nm -u` harus kosong untuk linked relocatable object M15?**
   Untuk membuktikan tidak ada simbol eksternal tak terdefinisi (invariant I15-10) — bila ada, artinya source memanggil fungsi libc/compiler-runtime yang tidak tersedia saat linking ke kernel, berpotensi menyebabkan link error atau crash runtime.

9. **Bagaimana desain MCSFS1 berubah jika mendukung subdirectory?**
   Root directory block tidak lagi cukup tunggal; setiap direktori memerlukan blok directory-nya sendiri, inode bertipe direktori perlu direct block ke blok dirent-nya, dan lookup path perlu traversal rekursif (bukan pencarian flat 16 slot).

10. **Bagaimana desain MCSFS1 berubah jika mendukung file lebih besar dari 4096 byte?**
    Perlu indirect block (single/double indirect) seperti pada ext2, karena `MCSFS1_DIRECT_BLOCKS=8` saat ini membatasi file maksimal `8 × 512 = 4096` byte secara direct-only.

11. **Apa akibat security jika nama file tidak divalidasi?**
    Nama kosong, nama memuat `/`, atau nama melebihi `MCSFS1_MAX_NAME` bisa menimbulkan corruption pada struct dirent (buffer overflow saat `mcsfs_memcpy`), atau ambiguitas namespace flat root-only.

12. **Bagaimana MCSFS1 harus berinteraksi dengan lock M12 jika ada multi-threaded file I/O?**
    Caller wajib memegang filesystem-wide lock selama `create`/`write`/`unlink`, dan minimal shared/read lock selama `read`/`fsck`, mengikuti urutan lock VFS → filesystem → buffer cache → block device untuk menghindari deadlock (tidak boleh dibalik).

---

## 21. Perbandingan M14 vs M15

| Aspek | M14 | M15 | Perubahan |
|---|---|---|---|
| Layer utama | Block device layer, RAM block driver, buffer cache | Filesystem persistent minimal MCSFS1 | +superblock, +inode, +directory, +bitmap allocator |
| Unit data | Block mentah berbasis LBA | File bernama dengan metadata (mode, size, direct block) | +namespace |
| Operasi | `block_read`, `block_write`, `block_flush` | `format`, `mount`, `fsck`, `create`, `write`, `read`, `unlink` | +7 operasi filesystem-level |
| Konsistensi | Dirty flag + flush per block | Flush eksplisit + fsck-lite per operasi metadata | +deteksi invariant |
| Audit ELF | `m14_block_layer.o` | `mcsfs1.o` | Pola audit sama (nm/readelf/objdump/sha256) |
| Integrasi kernel | Sudah ditautkan (`SRC_C` mencakup `kernel/block/*.c`) | **Belum ditautkan** (`fs/` di luar cakupan `SRC_C`) | Integrasi penuh menjadi tugas pengayaan |

---

## 22. Artefak Bukti M15

| Artefak | Path | Fungsi |
|---|---|---|
| `mcsfs1.h` | `fs/mcsfs1/mcsfs1.h` | Header API dan konstanta format |
| `mcsfs1.c` | `fs/mcsfs1/mcsfs1.c` | Implementasi filesystem (654 baris) |
| `test_mcsfs1.c` | `tests/m15/test_mcsfs1.c` | Host unit test dengan RAM-backed block device |
| `m15_preflight.sh` | `scripts/m15_preflight.sh` | Script pengumpulan bukti readiness |
| `mcsfs1.o` | `build/mcsfs1.o` | Object freestanding x86_64 |
| `test_mcsfs1_host` | `build/test_mcsfs1_host` | Binary host test |
| `test_m15_mcsfs1.log` | `build/test_m15_mcsfs1.log` | Log hasil host test |
| `mcsfs1.undefined.txt` | `build/mcsfs1.undefined.txt` | Audit `nm -u` (kosong) |
| `mcsfs1.readelf.header.txt` | `build/mcsfs1.readelf.header.txt` | Header ELF64 x86-64 |
| `mcsfs1.objdump.txt` | `build/mcsfs1.objdump.txt` | Disassembly |
| `m15.sha256.txt` | `build/m15.sha256.txt` | Checksum artefak |
| `mcsos.iso` | `build/mcsos.iso` | Boot image ISO (6.682.624 byte) |
| `qemu_serial.txt` | `artifacts/m15/qemu_serial.txt` | Log serial QEMU smoke test |
| `host_info.txt`, `tool_versions.txt`, `preflight.txt` | `artifacts/m15/` | Bukti toolchain dan readiness |

---

## 23. Status Readiness M15

Sesuai kriteria Bagian 28 panduan, hasil M15 ini dinyatakan:

> **Siap uji QEMU untuk filesystem persistent minimal MCSFS1.**

Status ini **bukan** bukti filesystem crash-consistent penuh, **bukan** bukti kompatibilitas POSIX penuh, **bukan** bukti aman terhadap power-loss pada perangkat nyata, dan **bukan** bukti siap produksi.

| Area | Evidence minimum | Status M15 |
|---|---|---|
| Build | Clean build, host test, freestanding object | Siap uji (`make check-m15` lulus 2× dari kondisi bersih) |
| Functional | Format, mount, create, write, read, unlink, fsck-lite | Siap uji host (15 skenario lulus) |
| Debuggability | `readelf`, `objdump`, `nm` | Siap audit object |
| Runtime | QEMU smoke log | Diuji, baseline M14 tanpa regression; MCSFS1 belum ditautkan kernel |
| Security | Validasi nama/range/size | Baseline, belum access-control penuh |
| Reliability | Flush eksplisit dan fsck-lite | Belum crash-consistency penuh |
| Release | Dokumentasi dan rollback | Siap demonstrasi praktikum terbatas |

---

## 24. Tugas Pengayaan yang Belum Dikerjakan (Opsional, Sesuai Bagian 22.2 Panduan)

Sesuai arahan (*"sesuai panduan saja"*), tugas pengayaan berikut **tidak dikerjakan** pada laporan ini dan didokumentasikan sebagai potensi pengembangan lanjutan:

1. Operasi `stat` untuk membaca ukuran file.
2. Test directory penuh (>16 file).
3. Fsck check dua dirent menunjuk inode sama.
4. Block leak detection sederhana.
5. Mount flag read-only bila fsck gagal.
6. Integrasi MCSFS1 sebagai mount backend ops pada VFS M13 (termasuk penautan `fs/mcsfs1/mcsfs1.c` ke `SRC_C` kernel Makefile agar muncul log `[MCSOS:M15]` pada QEMU).

---

## 25. Referensi

[1] Linux Kernel Documentation, "Overview of the Linux Virtual File System," The Linux Kernel documentation. [Online]. Available: https://docs.kernel.org/filesystems/vfs.html. Accessed: 2026-05-03.

[2] Linux Kernel Documentation, "The Second Extended Filesystem," The Linux Kernel documentation. [Online]. Available: https://www.kernel.org/doc/html/v6.6/filesystems/ext2.html. Accessed: 2026-05-03.

[3] Linux Kernel Documentation, "Buffer Heads," The Linux Kernel documentation. [Online]. Available: https://docs.kernel.org/filesystems/buffer.html. Accessed: 2026-05-03.

[4] QEMU Project, "GDB usage," QEMU documentation. [Online]. Available: https://qemu-project.gitlab.io/qemu/system/gdb.html. Accessed: 2026-05-03.

[5] LLVM Project, "Clang command line argument reference," Clang documentation. [Online]. Available: https://clang.llvm.org/docs/ClangCommandLineReference.html. Accessed: 2026-05-03.

[6] GNU Project, "GNU Binary Utilities," GNU Binutils documentation. [Online]. Available: https://www.sourceware.org/binutils/docs/binutils.html. Accessed: 2026-05-03.

---

## 26. Checklist Final Sebelum Pengumpulan M15

| Checklist | Status |
|---|---|
| Semua placeholder sudah diganti dengan data aktual | Ya |
| Metadata laporan lengkap | Ya |
| Commit awal dan akhir dicatat | Ya (`78a89a6` → `9a026c8`) |
| Perintah build dan test dapat dijalankan ulang | Ya (`make check-m15`, teruji 2× clean build) |
| Log build dilampirkan | Ya |
| Log host test dilampirkan | Ya |
| Log QEMU dilampirkan | Ya (`qemu_serial.txt`) |
| Artefak penting diberi hash (`sha256sum`) | Ya (`build/m15.sha256.txt`) |
| Desain, invariants, ownership, dan failure modes dijelaskan | Ya |
| Security/reliability dibahas | Ya |
| Fault injection tambahan di luar corrupt-super | Ya (`create-name-too-long`) |
| Hubungan ke VFS M13 dan block layer M14 dijelaskan | Ya |
| Readiness review tidak berlebihan (batasan dinyatakan jujur) | Ya |
| Tugas pengayaan yang belum dikerjakan dicatat eksplisit | Ya |
| Referensi memakai format IEEE | Ya |
| Laporan disimpan sebagai Markdown | Ya |

---

## Lampiran M15 — Screenshot Evidence

| No. | File Screenshot | Halaman PDF | Keterangan |
|---|---|---|---|
| 1 | `Screenshot 2026-07-10 153320.png` | Halaman 1 | `mkdir artifacts/m15`, output `host_info.txt` dan `tool_versions.txt` (uname, distro, versi clang/ld/nm/readelf/objdump/make/QEMU) |
| 2 | `Screenshot 2026-07-10 153320.png` | Halaman 2 | Pembuatan `scripts/m15_preflight.sh`, hasil run: `artifacts/m0`–`m13` missing, `artifacts/m14` present |
| 3 | `Screenshot 2026-07-10 153320.png` | Halaman 3 | `git switch -c praktikum-m15-mcsfs1`, `mkdir` struktur direktori, pembuatan `fs/mcsfs1/mcsfs1.h` lengkap |
| 4 | `Screenshot 2026-07-10 153320.png` | Halaman 4 | `mcsfs1.c` bagian awal: struct disk layout, `mcsfs_memset`, `mcsfs_memcpy` |
| 5 | `Screenshot 2026-07-10 153320.png` | Halaman 5 | `mcsfs1.c` lanjutan: `mcsfs_memcmp`, `mcsfs_strlen_bound`, `valid_name`, `dev_read`, `dev_write`, `dev_flush` (awal) |
| 6 | `Screenshot 2026-07-10 153320.png` | Halaman 6 | `mcsfs1.c` lanjutan: `dev_flush` (akhir), `bit_set/clear/test`, `load_super`, `read_inode` |
| 7 | `Screenshot 2026-07-10 153320.png` | Halaman 7 | `mcsfs1.c` lanjutan: `write_inode`, `load_bmaps`, `store_bmaps`, `find_dirent` |
| 8 | `Screenshot 2026-07-10 153320.png` | Halaman 8 | `mcsfs1.c` lanjutan: `alloc_inode_block`, `alloc_data_block` |
| 9 | `Screenshot 2026-07-10 153320.png` | Halaman 9 | `mcsfs1.c` lanjutan: `free_inode_and_blocks`, `mcsfs1_format` (awal) |
| 10 | `Screenshot 2026-07-10 153320.png` | Halaman 10 | `mcsfs1.c` lanjutan: `mcsfs1_format` (akhir), `mcsfs1_mount`, `mcsfs1_create` (awal) |
| 11 | `Screenshot 2026-07-10 153320.png` | Halaman 11 | `mcsfs1.c` lanjutan: `mcsfs1_create` (akhir), `mcsfs1_write` |
| 12 | `Screenshot 2026-07-10 153320.png` | Halaman 12 | `mcsfs1.c` lanjutan: `mcsfs1_read` |
| 13 | `Screenshot 2026-07-10 153320.png` | Halaman 13 | `mcsfs1.c` lanjutan: `mcsfs1_unlink` |
| 14 | `Screenshot 2026-07-10 153320.png` | Halaman 14 | `mcsfs1.c` akhir: `mcsfs1_fsck` (invariant I15-01 s.d. I15-07) |
| 15 | `Screenshot 2026-07-10 153320.png` | Halaman 15 | Pembuatan `tests/m15/test_mcsfs1.c` lengkap; cek `ls -la Makefile` → sudah ada |
| 16 | `Screenshot 2026-07-10 153320.png` | Halaman 16 | Inspeksi Makefile eksisting: `grep` variabel kunci, `sed` menampilkan isi Makefile (CC, HOSTCC, LD, target `check-m14` sebagai pola referensi) |
| 17 | `Screenshot 2026-07-10 153320.png` | Halaman 17 | Penambahan target `check-m15` via `cat >> Makefile`, verifikasi `tail -n 20 Makefile` |
| 18 | `Screenshot 2026-07-10 153320.png` | Halaman 18 | `make check-m15` pertama (PASS, `flush_count=5`), penyisipan fault injection via Python, verifikasi `grep -n "create-name-too-long"` |
| 19 | `Screenshot 2026-07-10 153320.png` | Halaman 19 | `cat build/test_m15_mcsfs1.log`, `git add`, `git status --short`, `git commit -m "M15: add MCSFS1 minimal persistent filesystem"`, `git log --oneline` |
| 20 | `Screenshot 2026-07-10 153320.png` | Halaman 20 | `make image` — build `iso_root`, `xorriso`, `limine bios-install`, checksum ISO |
| 21 | `Screenshot 2026-07-10 153320.png` | Halaman 21 | Pencarian OVMF (`ls`, `dpkg -l`), `grep` Makefile untuk pola ovmf/uefi/bios/limine, `ls iso_root` |
| 22 | `Screenshot 2026-07-10 153320.png` | Halaman 22 | `make image` (verifikasi ulang), `ls -la build/mcsos.iso` (6.682.624 byte) |
| 23 | `Screenshot 2026-07-10 153320.png` | Halaman 23 | Log tick QEMU sebelumnya, perintah `qemu-system-x86_64` smoke test lengkap dengan `timeout 20`, isi penuh `qemu_serial.log` (boot M3–M14) |
| 24 | `Screenshot 2026-07-10 153320.png` | Halaman 24 | `ls evidence/`, pencarian evidence M14, `ls artifacts/m14`, verifikasi `git ls-files`, `cp qemu_serial.log qemu_serial.txt`, `git add`/`git commit` evidence QEMU M15 |

---

**Repository:** `~/src/mcsos` — branch `praktikum-m15-mcsfs1`
**Commit rentang laporan:** `78a89a6` → `9a026c8`
