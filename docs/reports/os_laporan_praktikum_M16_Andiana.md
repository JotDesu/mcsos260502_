# Laporan Praktikum M16 — Write-Ahead Journal MCSFS1J (Format/Mount/Recover/Write/Read/Fsck)

## MCSOS 260502

**Nama file laporan:** `laporan_praktikum_M16_[NIM_Kelompok].md`
**Nama sistem operasi:** MCSOS versi 260502
**Target default:** x86_64, QEMU, Windows 11 x64 + WSL 2 (Ubuntu 26.04 LTS "resolute"), kernel monolitik pendidikan, C freestanding dengan assembly minimal, POSIX-like subset
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.
**Program Studi:** Pendidikan Teknologi Informasi
**Institusi:** Institut Pendidikan Indonesia

---

## 0. Metadata Laporan

| Atribut | Isi |
|---|---|
| Kode praktikum | M16 |
| Judul praktikum | Write-Ahead Journal MCSFS1J (crash-consistency): format, mount, recover, write, read, fsck |
| Jenis pengerjaan | Individu |
| Nama mahasiswa | Andiana Jamaludin Malik |
| NIM | 2583207073016 |
| Kelas | 1B |
| Nama kelompok | - |
| Anggota kelompok | - |
| Tanggal praktikum | 2026-07-10 |
| Tanggal pengumpulan | 2026-07-10 |
| Repository | ~/src/mcsos |
| Branch | praktikum-m16-journal-recovery |
| Commit awal | `9a026c8` (M15: tambahkan bukti QEMU smoke test — baseline M14, MCSFS1 belum ditautkan ke kernel) |
| Commit akhir | `34c4b09` (M16: implementasi write-ahead journal MCSFS1J — format/mount/recover/write/read/fsck) |
| Status readiness yang diklaim | Siap uji QEMU dan host fault-injection terbatas (belum production) |

---

## 1. Sampul

# Laporan Praktikum M16
## Write-Ahead Journal MCSFS1J: Format, Mount, Recover, Write, Read, Fsck

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

Saya menyatakan bahwa laporan ini disusun berdasarkan pekerjaan praktikum sendiri sesuai panduan M16. Bantuan eksternal, referensi, dan dokumentasi resmi dicatat pada bagian referensi dan lampiran.

| Pernyataan | Status |
|---|---|
| Semua potongan kode eksternal diberi atribusi | Ya (panduan M16 dari dosen) |
| Semua penggunaan AI assistant dicatat | Ya |
| Repository yang dikumpulkan sesuai commit akhir | Ya |
| Tidak ada klaim readiness tanpa bukti | Ya |

Catatan penggunaan bantuan eksternal:

```text
- Panduan praktikum M16 (OS_panduan_M16.pdf) sebagai referensi utama
- Template laporan praktikum (os_template_laporan_praktikum.md)
- AI assistant digunakan untuk membantu menyusun laporan dan analisis
- Semua source code diimplementasikan berdasarkan panduan dosen
```

---

## 3. Tujuan Praktikum

1. **Tujuan teknis 1:** Merancang dan mengimplementasikan struktur data write-ahead journal (`m16_journal_header`, `m16_journal_desc`, `m16_jrec`, `m16_tx`) untuk filesystem pendidikan MCSFS1J.
2. **Tujuan teknis 2:** Mengimplementasikan siklus commit journal (`m16_journal_commit`) yang menulis descriptor + payload + commit record sebelum menulis ke lokasi asli (home location).
3. **Tujuan teknis 3:** Mengimplementasikan pemulihan (`m16_journal_recover`) yang melakukan replay transaksi committed dan menolak (fail-closed) descriptor yang korup/tidak dikenal.
4. **Tujuan teknis 4:** Mengimplementasikan operasi filesystem dasar di atas journal: `m16_format`, `m16_mount`, `m16_write_file`/`m16_write_file_ex`, `m16_read_file`, `m16_fsck`.
5. **Tujuan teknis 5:** Membuat unit test host (`MCSOS_M16_HOST_TEST`) yang mensimulasikan power-loss (commit record tertulis, home-location write dilewati) dan memverifikasi hasil replay.
6. **Tujuan teknis 6:** Membangun objek freestanding x86_64-elf dari source yang sama tanpa mengubah semantik, serta mengaudit hasilnya dengan `readelf`, `objdump`, `nm`, dan `sha256sum`.
7. **Tujuan teknis 7:** Menautkan self-test kernel (`m16_fs_selftest`) ke `kmain.c` dan memverifikasi eksekusinya lewat log serial QEMU dan sesi GDB remote.
8. **Tujuan konseptual:** Memahami konsep crash-consistency melalui pola write-ahead logging (WAL): descriptor block, payload block, commit record, replay idempoten, dan fail-closed recovery saat data tidak dapat dipercaya.
9. **Tujuan validasi:** Menyimpan log preflight, log build, log QEMU, evidence ELF (readelf/objdump/nm/sha256sum), serta transkrip sesi GDB sebagai bukti deterministik.

---

## 4. Capaian Pembelajaran Praktikum

Setelah praktikum ini, mahasiswa mampu:

| CPL/CPMK praktikum | Bukti yang harus ditunjukkan |
|---|---|
| Menjelaskan konsep write-ahead logging dan crash-consistency | Struct `m16_journal_header/desc/jrec/tx`, fungsi commit/recover |
| Mengimplementasikan commit transaksi journal dua fase (descriptor+payload lalu commit record) | `m16_journal_commit()` |
| Mengimplementasikan pemulihan (recovery) idempoten dan fail-closed | `m16_journal_recover()`, uji descriptor korup mengembalikan `M16_E_CORRUPT` |
| Mengimplementasikan filesystem sederhana (superblock, bitmap inode/block, inode table, direktori root) | `m16_format`, `m16_mount`, `m16_fsck` |
| Menulis unit test host yang mensimulasikan crash di tengah transaksi | Blok `#ifdef MCSOS_M16_HOST_TEST` pada `m16_mcsfs_journal.c` |
| Membangun objek freestanding x86_64-elf dan memverifikasi tidak ada symbol undefined | `nm -u` menghasilkan 0 baris, `readelf -h` ELF64/Advanced Micro Devices X86-64 |
| Mengintegrasikan self-test filesystem ke boot kernel dan memverifikasi lewat serial log | `m16_fs_selftest.c`, `logs/m16/qemu_serial.log` |
| Melakukan verifikasi runtime dengan GDB remote (breakpoint dan backtrace) | Transkrip sesi GDB: `m16_journal_recover`, `m16_fsck` |
| Mendokumentasikan keterbatasan desain secara jujur (readiness yang tidak berlebihan) | Catatan "belum ditautkan ke driver blok M14 sesungguhnya" pada komentar kode dan bagian 25 |

---

## 5. Peta Milestone MCSOS

| Milestone | Fokus | Status dalam laporan |
|---|---|---|
| M0 | Requirements, governance, baseline arsitektur | [x] selesai praktikum (sebelumnya) |
| M1 | Toolchain reproducible, Git, QEMU, GDB, metadata build | [x] selesai praktikum (sebelumnya) |
| M2 | Boot image, kernel ELF64, early console | [x] selesai praktikum (sebelumnya) |
| M3 | Panic path, linker map, GDB, observability awal | [x] selesai praktikum (sebelumnya) |
| M4 | Trap, exception, interrupt, timer | [x] selesai praktikum (sebelumnya, terlihat dari IDT/PIT log) |
| M5 | PMM, VMM, page table, kernel heap | [x] selesai praktikum (sebelumnya, terlihat dari PMM/VMM/kmem log) |
| M6 | Thread, scheduler, synchronization | [x] selesai praktikum (sebelumnya, terlihat dari scheduler/thread tick log) |
| M7 | Syscall ABI dan user program loader | [x] selesai praktikum (sebelumnya, terlihat dari int 0x80 smoke test) |
| M8 | VFS, file descriptor, ramfs | [x] selesai praktikum (sebelumnya, terlihat dari `m13_vfs_selftest`) |
| M9 | Block layer dan device model | [x] selesai praktikum (sebelumnya, `kernel/block/block.c`) |
| M10 | Persistent filesystem, mcsfs/ext2-like, recovery (rintisan awal) | [x] sebagian, dilanjutkan pada M15/M16 |
| M11 | ELF user loader / syscall lanjutan | [x] selesai praktikum (sebelumnya, branch `praktikum-m11-elf-user-loader`) |
| M12 | Security model, capability/ACL, syscall fuzzing, hardening | [x] selesai praktikum (sebelumnya, `evidence/M12`) |
| M13 | SMP, scalability, lock stress, NUMA-aware preparation | [x] selesai praktikum (sebelumnya, `evidence/M13`, `m13_vfs_selftest`) |
| M14 | Framebuffer/graphics console **atau** (pada repo ini) block device demo | [x] selesai praktikum (sebelumnya, `m14_block_demo_init`, ram0) |
| M15 | MCSFS1 minimal persistent filesystem | [x] selesai praktikum (sebelumnya, belum ditautkan ke kernel) |
| **M16** | **Write-ahead journal MCSFS1J: format/mount/recover/write/read/fsck** | **[x] dibahas penuh pada laporan ini** |

Batas cakupan praktikum:

```text
Praktikum M16 mencakup:
- Preflight M16 (scripts/m16_preflight.sh)
- Struktur data journal: m16_journal_header, m16_journal_desc, m16_jrec, m16_tx
- Superblock, bitmap inode/block, inode table, root directory (m16_super, m16_inode, m16_dirent)
- Journal commit dua fase: m16_journal_commit()
- Journal recovery idempoten + fail-closed: m16_journal_recover()
- Operasi filesystem: m16_format, m16_mount, m16_write_file(_ex), m16_read_file, m16_fsck
- Host unit test (MCSOS_M16_HOST_TEST): format, fsck, write/read normal,
  simulasi crash (commit record tertulis, home-location belum ditulis), replay, verifikasi,
  serta uji descriptor journal korup (recovery harus gagal tertutup/fail-closed)
- Build freestanding x86_64-elf dari source yang sama + audit ELF (readelf/objdump/nm/sha256sum)
- tests/m16/Makefile dengan target host, freestanding, audit, clean
- Integrasi ke root Makefile (auto-discovery kernel/fs/mcsfs1j/*.c)
- Kernel-side self-test: kernel/fs/mcsfs1j/m16_fs_selftest.c, dipanggil dari kmain.c
- Build kernel penuh, image ISO (Limine), boot QEMU headless dengan serial log
- Verifikasi GDB remote: breakpoint m16_journal_recover dan m16_fsck, backtrace call chain
- Evidence: preflight log, build log, serial log, readelf/objdump/nm/sha256sum, transkrip GDB
- Commit dan push branch praktikum-m16-journal-recovery, pembuatan pull request

Non-goals (tidak termasuk):
- Pengaitan MCSFS1J ke driver block device M14 yang sesungguhnya (masih RAM-backed statis,
  dicatat sebagai catatan "Langkah 5" pada komentar kode — lihat bagian 25)
- Locking/concurrency pada journal (single-threaded pada self-test)
- Journal wrap-around / circular log (implementasi ini single-slot: clear-write-clear per transaksi)
- Multi-block file (direct block tunggal, ukuran file <= 1 block)
- Direktori bertingkat (hanya root directory flat)
- Kompresi, enkripsi, atau checksum kriptografis kuat (checksum yang dipakai adalah hash sederhana,
  bukan untuk keamanan)
```

---

## 6. Dasar Teori Ringkas

### 6.1 Konsep Sistem Operasi yang Diuji

**Write-Ahead Logging (WAL) / Journaling:** Teknik crash-consistency di mana perubahan ditulis terlebih dahulu ke area journal (log) sebelum ditulis ke lokasi asli data (home location). Jika sistem crash di tengah proses:
- Jika commit record journal **belum** tertulis lengkap → transaksi dianggap tidak pernah terjadi (data lama tetap valid).
- Jika commit record **sudah** tertulis lengkap tetapi home-location write belum selesai → recovery akan **replay** (menulis ulang) payload dari journal ke home location.

**Struktur Journal pada MCSFS1J:**
- `m16_journal_header`: magic, version, `state` (`M16_J_EMPTY` / `M16_J_COMMITTED`), `seq`, `count` record, `header_checksum`.
- `m16_journal_desc`: magic, `target_lba` (lokasi tujuan tulis), `payload_checksum`.
- `m16_jrec`: `target_lba` + payload 1 blok (512 byte).
- `m16_tx`: kumpulan `m16_jrec` (maksimum `M16_JOURNAL_MAX_RECORDS` = 8) yang membentuk satu transaksi atomik.

**Alur commit (`m16_journal_commit`):**
1. `m16_journal_clear()` — kosongkan header journal.
2. Tulis descriptor + payload untuk setiap record transaksi.
3. Tulis header journal dengan `state = M16_J_COMMITTED` dan `header_checksum` terhitung.
4. (opsional untuk simulasi crash) berhenti setelah commit record — parameter `stop_after_commit_record`.
5. Tulis payload ke home location (lokasi asli) untuk setiap record.
6. `m16_journal_clear()` kembali (menandakan transaksi selesai/aman).

**Alur recovery (`m16_journal_recover`):**
1. Baca header journal.
2. Jika `magic == 0` dan `state == M16_J_EMPTY` → tidak ada yang perlu dipulihkan, kembalikan OK.
3. Jika magic/version/state/count tidak valid → kembalikan `M16_E_CORRUPT` (**fail-closed**, tidak mencoba menerka target).
4. Verifikasi `header_checksum`; jika tidak cocok → `M16_E_CORRUPT`.
5. Untuk setiap record: baca descriptor, verifikasi magic dan `target_lba` valid, baca payload data, verifikasi `payload_checksum`; jika ada yang tidak cocok → `M16_E_CORRUPT`.
6. Jika semua valid → tulis ulang (**replay**) payload ke `target_lba` masing-masing.
7. `m16_journal_clear()`.

**Filesystem di atas journal:**
- `m16_super`: superblock (magic, versi, ukuran blok, lokasi journal/bitmap/inode table/root dir/data area).
- `m16_inode`: `used`, `kind` (1=file, 2=dir), `size`, `direct[4]` (blok langsung).
- `m16_dirent`: `used`, `ino`, `name[32]` — entri direktori flat pada root.
- `m16_format`: menulis superblock, bitmap inode, bitmap blok, inode table (root inode), root dir kosong, lalu `m16_journal_clear`.
- `m16_write_file_ex`: mount, cek superblock, alokasi inode+blok+slot dirent bebas, **commit transaksi journal** untuk bitmap+inode table (tx pertama) dan root dir+data blok (tx kedua) — dipisah sebagai *educational simplification* karena `m16_tx` maksimum 8 record sedangkan inode table membutuhkan 4 blok.
- `m16_read_file`: mount, baca inode table, cari dirent by name, validasi inode, baca blok data.
- `m16_fsck`: verifikasi konsistensi bitmap inode/blok terhadap root inode dan setiap dirent (used, kind, size, direct block dalam rentang data area serta ditandai terpakai di bitmap blok).

### 6.2 Konsep Arsitektur x86_64 yang Relevan

| Konsep | Relevansi pada praktikum | Bukti/verifikasi |
|---|---|---|
| ELF64 relocatable object (`.o`) | Objek journal dikompilasi terpisah sebagai `.o` freestanding lalu ditautkan ke kernel | `readelf -h` → `Type: REL`, `Machine: Advanced Micro Devices X86-64` |
| Freestanding C17 | Tidak ada libc hosted; hanya `stdint.h`/`stddef.h`/`stdbool.h` | Flag `-ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone` |
| Static assert ukuran struct | `_Static_assert(sizeof(struct m16_super) == M16_BLOCK_SIZE, ...)` dan `sizeof(struct m16_inode) == 128` | Kompilasi berhasil tanpa error assert |
| Symbol resolution | Objek journal tidak boleh punya symbol undefined pada build freestanding | `nm -u m16_mcsfs_journal.o` → 0 baris |
| System V ABI / calling convention | Semua fungsi C biasa (tanpa inline asm) mengikuti ABI standar kernel | `-mabi=sysv` (warisan konfigurasi Makefile root) |
| Higher-half kernel addressing | Kernel di-link pada `0xffffffff80000000`; breakpoint GDB menunjukkan alamat `0xffffffff8000xxxx` | Transkrip GDB (`break m16_journal_recover` → `0xffffffff80005050`) |

### 6.3 Konsep Implementasi Freestanding

| Aspek | Keputusan praktikum |
|---|---|
| Bahasa | C17 freestanding, tanpa inline assembly pada modul journal itu sendiri |
| Runtime | Fungsi utilitas manual: `m16_zero`, `m16_copy`, `m16_strlen_bounded`, `m16_streq`, `m16_checksum` |
| Media penyimpanan | `struct m16_blockdev` — array blok in-memory (`blocks[M16_MAX_BLOCKS][M16_BLOCK_SIZE]`) dengan fault injection (`fail_after`) untuk simulasi kegagalan tulis |
| Build ganda | Objek yang sama dikompilasi dua kali: (1) host test (`-DMCSOS_M16_HOST_TEST`, native x86_64) dan (2) freestanding (`--target=x86_64-elf`) tanpa mengubah logika |
| Compiler flags kritis (freestanding) | `-ffreestanding -fno-builtin -fno-stack-protector -fno-pic -mno-red-zone --target=x86_64-elf` |
| Risiko undefined behavior | Mitigasi dengan `-Wall -Wextra -Werror -O2`, serta `nm -u` untuk memastikan tidak ada symbol tergantung eksternal yang tak terduga |

### 6.4 Referensi Teori yang Digunakan

| No. | Sumber | Bagian yang digunakan | Alasan relevansi |
|---|---|---|---|
| [1] | OSDev Wiki — Journaling | Konsep umum write-ahead log pada filesystem | Dasar desain descriptor/commit/replay |
| [2] | OSDev Wiki — Ext2 | Struktur superblock, bitmap, inode table sederhana | Analogi desain `m16_super`/`m16_inode` |
| [3] | LLVM Project — Clang User's Manual | Freestanding builds dan target triple `x86_64-elf` | Flags kompilasi freestanding objek journal |
| [4] | Rosen, K. — Discrete Mathematics (hash function dasar) | Fungsi checksum multiplikatif sederhana | Implementasi `m16_checksum` |
| [5] | GNU Binutils Manual (`readelf`, `objdump`, `nm`) | Inspeksi ELF header, disassembly, symbol table | Audit evidence objek freestanding |
| [6] | GDB Documentation — Remote Debugging Protocol | `target remote`, `break`, `bt` | Verifikasi runtime call chain recovery |

---

## 7. Lingkungan Praktikum

### 7.1 Host dan Target

| Komponen | Nilai |
|---|---|
| Host OS | Windows 11 x64 |
| Lingkungan build | WSL 2 — Linux 6.6.87.2-microsoft-standard-WSL2 |
| Distribusi | Ubuntu 26.04 LTS ("resolute") |
| Target ISA (kernel) | x86_64 |
| Target ABI (freestanding objek M16) | x86_64-elf |
| Emulator | QEMU system-x86_64 (QEMU emulator version 10.2.1) |
| Bootloader | Limine (`third_party/limine`) |
| Debugger | GDB (remote target `:1234`) |
| Build system | GNU Make 4.4.1 |
| Bahasa utama | C17 freestanding (Ubuntu clang version 21.1.8) |
| Binutils | GNU Binutils for Ubuntu 2.46 (`nm`, `readelf`, `objdump`) |

Bukti screenshot: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 1)

### 7.2 Versi Toolchain

Output perintah verifikasi toolchain (`make --version`, `qemu-system-x86_64 --version`, `nm/readelf/objdump/sha256sum --version`, `git --version`, `uname -a`, `lsb_release -a`, `clang --version`):

```text
Linux JotDesu 6.6.87.2-microsoft-standard-WSL2 #1 SMP PREEMPT_DYNAMIC Thu Jun 5 18:30:46 UTC 2025 x86_64 GNU/Linux
Distributor ID: Ubuntu
Description:    Ubuntu 26.04 LTS
Release:        26.04
Codename:       resolute
Ubuntu clang version 21.1.8 (6ubuntu1)
Target: x86_64-pc-linux-gnu
GNU Make 4.4.1
QEMU emulator version 10.2.1
GNU nm/readelf/objdump (GNU Binutils for Ubuntu) 2.46
sha256sum (uutils coreutils) 0.8.0
git version 2.53.0
```

Verifikasi tambahan: probe compile freestanding minimal (`void _start(void) {}`) dengan flag `--target=x86_64-elf -ffreestanding -fno-stack-protector -mno-red-zone` menghasilkan objek ELF64 relocatable valid (`readelf -h /tmp/probe.o`).

Bukti screenshot: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 1)

### 7.3 Lokasi Repository dan Branch

| Item | Nilai |
|---|---|
| Path repository di WSL | `/home/andianaaji/src/mcsos` |
| Apakah berada di filesystem Linux WSL, bukan `/mnt/c` | Ya |
| Branch kerja | `praktikum-m16-journal-recovery` (dibuat baru dari branch bersih) |
| Commit hash awal | `9a026c8` |
| Commit hash akhir | `34c4b09` |

```bash
cd ~/src/mcsos
mkdir -p kernel/fs/mcsfs1j tests/m16 scripts build/m16 logs/m16 evidence/m16
git status --short
git checkout -b praktikum-m16-journal-recovery
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 1)

---

## 8. Repository dan Struktur File

### 8.1 Struktur Direktori Awal (Sebelum Implementasi)

Verifikasi struktur direktori relevan sebelum menambahkan kode M16:

```bash
find kernel tests scripts build logs evidence -maxdepth 3 -type d | sort
```

Cuplikan hasil (direktori yang sudah ada dari milestone sebelumnya): `build/normal/kernel/{arch,block,core,lib,mm,sync,syscall,user,vfs}`, `evidence/{M3,M4,M7,M9,M10,M11,M12,M13}`, `kernel/{arch/x86_64,block,core,fs/mcsfs1j,include/mcsos/{kernel,lib},lib,mm,sync,syscall,tests,user,vfs}`, `tests/{host,m11,m15,toolchain}`.

Bukti screenshot: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 2)

### 8.2 File Baru yang Dibuat pada M16

| File | Jenis perubahan | Alasan perubahan | Risiko |
|---|---|---|---|
| `scripts/m16_preflight.sh` | Baru | Preflight otomatis: cek host, toolchain, git, dan daftar file subsistem terkait | Rendah — hanya membaca/mencatat |
| `kernel/fs/mcsfs1j/m16_mcsfs_journal.c` (704 baris) | Baru | Implementasi penuh journal + filesystem MCSFS1J + host unit test | Sedang — logika crash-recovery kritis |
| `kernel/fs/mcsfs1j/m16_mcsfs_journal.h` | Baru | Header sederhana agar file kernel lain bisa memanggil API M16 | Rendah |
| `kernel/fs/mcsfs1j/m16_fs_selftest.c` | Baru | Self-test sisi kernel yang meniru pola self-test M13/M14 | Sedang — dipanggil saat boot, memakai `KERNEL_PANIC` |
| `kernel/fs/mcsfs1j/m16_fs_selftest.h` | Baru | Deklarasi `void m16_fs_selftest(void);` | Rendah |
| `tests/m16/Makefile` | Baru (2 revisi) | Target `host`/`freestanding`/`audit`/`clean` untuk objek M16 | Rendah |
| `kernel/core/kmain.c` | Diubah (via `sed`) | Menambahkan `#include` header self-test dan pemanggilan `m16_fs_selftest();` setelah `m14_block_demo_init();` | Sedang — mengubah urutan boot |
| `.gitignore` | Diubah | Menambahkan catatan biner host test M16 dan entry `tests/m16/m16_host_test` | Rendah |

### 8.3 Struktur Direktori Setelah Implementasi (Cuplikan Relevan)

```text
mcsos/
├── Makefile
├── .gitignore
├── kernel/
│   ├── core/
│   │   └── kmain.c                       (diubah: +include, +panggilan m16_fs_selftest)
│   ├── block/
│   │   ├── block.c
│   │   └── block.h
│   ├── fs/
│   │   ├── mcsfs1.h                      (M15 — pola referensi wiring)
│   │   └── mcsfs1j/
│   │       ├── m16_mcsfs_journal.c       (baru, 704 baris)
│   │       ├── m16_mcsfs_journal.h       (baru)
│   │       ├── m16_fs_selftest.c         (baru, 55 baris)
│   │       └── m16_fs_selftest.h         (baru)
│   └── include/mcsos/kernel/
│       ├── log.h
│       └── panic.h
├── scripts/
│   └── m16_preflight.sh                  (baru)
├── tests/
│   └── m16/
│       └── Makefile                      (baru)
├── build/m16/                            (artefak build M16)
├── logs/m16/                             (log preflight & QEMU)
└── evidence/m16/                         (readelf/objdump/nm/sha256sum)
```

Bukti screenshot struktur dan file header VFS/block/mcsfs1.h/kmain.c: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 14–16)

---

## 9. Desain Teknis MCSFS1J Journal

### 9.1 Konstanta dan Layout Disk

| Konstanta | Nilai | Keterangan |
|---|---|---|
| `M16_BLOCK_SIZE` | 512 | Ukuran satu blok (byte) |
| `M16_MAX_BLOCKS` | 128 | Total blok pada device simulasi |
| `M16_MAX_INODES` | 16 | Kapasitas inode table |
| `M16_DIRECT_BLOCKS` | 4 | Blok langsung per inode |
| `M16_MAX_NAME` | 32 | Panjang maksimum nama file |
| `M16_JOURNAL_MAX_RECORDS` | 8 | Maksimum record per transaksi journal |
| `M16_JOURNAL_BLOCKS` | `1 + 2*8 = 17` | 1 blok header + (descriptor+payload) per record |
| Layout LBA | `super(0) → journal(1..17) → inode_bitmap → block_bitmap → inode_table(4 blok) → root_dir → data_area` | Urutan berurutan dihitung dari makro `M16_*_LBA` |

### 9.2 Struktur Data Utama

```c
struct m16_super { magic, version, block_size, total_blocks, journal_start, journal_blocks,
                    inode_bitmap_lba, block_bitmap_lba, inode_table_lba, inode_table_blocks,
                    root_dir_lba, data_start_lba, clean_generation, reserved[114]; };
struct m16_inode { used, kind, size, direct[M16_DIRECT_BLOCKS], reserved[23]; };
struct m16_dirent { used, ino, name[M16_MAX_NAME]; };
struct m16_journal_header { magic, version, state, seq, count, header_checksum, reserved[121]; };
struct m16_journal_desc  { magic, target_lba, payload_checksum, reserved[124]; };
struct m16_jrec { target_lba, payload[M16_BLOCK_SIZE]; };
struct m16_tx   { count, rec[M16_JOURNAL_MAX_RECORDS]; };
```

`_Static_assert(sizeof(struct m16_super) == M16_BLOCK_SIZE, ...)` dan `_Static_assert(sizeof(struct m16_inode) == 128u, ...)` memastikan struct tetap sejajar-blok, sehingga layout disk tidak melenceng dari asumsi desain.

### 9.3 Diagram Alur Commit dan Recovery

```
WRITE FILE:
  alokasi inode/blok/dirent bebas
        │
        ▼
  tx#1 = { inode_bitmap_lba, block_bitmap_lba, [inode_table blok 0..3] }
        │
        ▼
  m16_journal_commit(tx#1) ── clear → desc+payload → COMMIT header → home-write → clear
        │
        ▼
  tx#2 = { root_dir_lba, data_block_lba }
        │
        ▼
  m16_journal_commit(tx#2) ── (sama seperti di atas)

CRASH SIMULATION (stop_after_commit_record=1):
  clear → desc+payload → COMMIT header ── [CRASH DI SINI, home-write TIDAK dijalankan]

RECOVERY:
  baca header journal
     │
     ├─ state==EMPTY            → OK, tidak ada yang dipulihkan
     ├─ magic/version/state/count tidak valid → M16_E_CORRUPT (fail-closed)
     ├─ header_checksum tidak cocok           → M16_E_CORRUPT
     └─ untuk setiap record:
           desc tidak valid / checksum payload tidak cocok → M16_E_CORRUPT
           valid → replay: tulis payload ke target_lba
     → clear journal (transaksi dianggap selesai)
```

### 9.4 Kode Error

| Kode | Nilai | Arti |
|---|---|---|
| `M16_E_OK` | 0 | Sukses |
| `M16_E_INVAL` | -1 | Argumen tidak valid |
| `M16_E_IO` | -2 | Kegagalan I/O (fault injection) |
| `M16_E_NOSPC` | -3 | Tidak ada ruang (inode/blok/dirent/record penuh) |
| `M16_E_EXISTS` | -4 | Nama file sudah ada |
| `M16_E_NOENT` | -5 | File tidak ditemukan |
| `M16_E_CORRUPT` | -6 | Data journal/filesystem tidak konsisten (fail-closed) |
| `M16_E_TOOLONG` | -7 | Nama file melebihi `M16_MAX_NAME` |

Bukti screenshot source lengkap: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 4–10)

---

## 10. Langkah Kerja Implementasi

### Langkah 1 — Preflight M16

Membuat dan menjalankan `scripts/m16_preflight.sh` yang mencatat: waktu UTC, informasi host (`uname -a`, `lsb_release`), versi tool (clang/make/nm/readelf/objdump/sha256sum/qemu/git), status git, serta daftar file subsistem yang relevan (`kernel/fs/mcsfs1j/*`, `kernel/block/*`, `kernel/core/*`, dsb.) ke `logs/m16/preflight.log`.

```bash
cat > scripts/m16_preflight.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
mkdir -p logs/m16 evidence/m16 build/m16
{
  echo "== M16 preflight =="; date -Iseconds
  ...
  echo "== subsystem probes =="
  find kernel -maxdepth 4 -type f | sort | sed -n '1,120p'
} | tee logs/m16/preflight.log
EOF
chmod +x scripts/m16_preflight.sh
./scripts/m16_preflight.sh
```

Hasil menunjukkan header `== M16 preflight ==`, tanggal `2026-07-10T16:22:38+07:00`, informasi host/toolchain identik dengan bagian 7.2, dan daftar 30+ file subsistem termasuk `kernel/fs/mcsfs1j/ncsfs1.h`, `kernel/block/{bcache,block,ramblk}.c`, `kernel/core/{kmain,log,panic,pmm,sched,serial,vmm}.c`.

Bukti screenshot: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 3)

### Langkah 2 — Implementasi Inti Journal dan Filesystem (`m16_mcsfs_journal.c`)

Menulis file inti sepanjang 704 baris berisi (urutan sesuai source):
1. Konstanta dan struct (bagian 9.2).
2. Utilitas: `m16_zero`, `m16_copy`, `m16_strlen_bounded`, `m16_streq`, `m16_checksum`.
3. Lapisan device: `m16_valid_lba`, `m16_read_block`, `m16_write_block` (dengan fault injection `fail_after`), `m16_dev_init`.
4. Bitmap: `m16_bitmap_set`, `m16_bitmap_get`.
5. Inode table I/O: `m16_load_inode_table`, `m16_store_inode_table` (menambahkan blok ke `m16_tx`).
6. Journal primitives: `m16_tx_add`, `m16_header_checksum`, `m16_journal_clear`, `m16_journal_commit`, `m16_journal_recover`.
7. `m16_format`, `m16_mount`.
8. Pencarian bebas: `m16_find_free_inode`, `m16_find_free_block`, `m16_find_dirent`, `m16_find_free_dirent`.
9. `m16_write_file_ex` / `m16_write_file`, `m16_read_file`, `m16_fsck`.
10. Blok `#ifdef MCSOS_M16_HOST_TEST` — unit test host lengkap (lihat bagian 12).

```bash
cat > kernel/fs/mcsfs1j/m16_mcsfs_journal.c <<'EOF'
/* ... isi lengkap sesuai bagian 9 ... */
EOF
wc -l kernel/fs/mcsfs1j/m16_mcsfs_journal.c
# 704 kernel/fs/mcsfs1j/m16_mcsfs_journal.c
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 4–10)

### Langkah 3 — Kompilasi dan Uji Host Sementara

```bash
clang -std=c17 -Wall -Wextra -DMCSOS_M16_HOST_TEST -o /tmp/m16_host_test kernel/fs/mcsfs1j/m16_mcsfs_journal.c
/tmp/m16_host_test
echo "exit code: $?"
# M16 host tests PASS
# exit code: 0
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 11)

### Langkah 4 — Membuat `tests/m16/Makefile` (Revisi 1: Host Only)

```makefile
CC := clang
CFLAGS := -std=c17 -Wall -Wextra -DMCSOS_M16_HOST_TEST
SRC := ../../kernel/fs/mcsfs1j/m16_mcsfs_journal.c
BIN := ../../build/m16/m16_host_test

.PHONY: all clean run
all: $(BIN)
$(BIN): $(SRC)
	mkdir -p ../../build/m16
	$(CC) $(CFLAGS) -o $(BIN) $(SRC)
run: all
	$(BIN)
clean:
	rm -f $(BIN)
```

```bash
make -C tests/m16 clean all
make -C tests/m16 run
# M16 host tests PASS
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 11)

### Langkah 5 — Audit Evidence Awal (Objek Freestanding Manual)

```bash
readelf -h build/m16/m16_mcsfs_journal.o | tee evidence/m16/readelf_header.txt
objdump -dr build/m16/m16_mcsfs_journal.o | tee evidence/m16/objdump_disasm.txt | head -n 40
sha256sum kernel/fs/mcsfs1j/m16_mcsfs_journal.c build/m16/m16_mcsfs_journal.o build/m16/m16_host_test | tee evidence/m16/sha256sum.txt
nm -u build/m16/m16_mcsfs_journal.o > evidence/m16/nm_undefined.txt
wc -l evidence/m16/nm_undefined.txt
cat evidence/m16/nm_undefined.txt
# 0 evidence/m16/nm_undefined.txt   (tidak ada symbol undefined)
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 12)

**Catatan penting:** Sebagaimana ditemukan pada `m16_dev_init` hasil disassembly, terlihat instruksi `movl $0x80,0x10000(%rax)` yang menetapkan `total_blocks` langsung ke offset — ini konsisten dengan layout struct `m16_blockdev` tanpa padding tak terduga, memvalidasi asumsi `_Static_assert` pada bagian 9.2.

### Langkah 6 — Revisi `tests/m16/Makefile` (Host + Freestanding + Audit)

```makefile
CLANG ?= clang
TARGET_TRIPLE ?= x86_64-elf
CFLAGS_COMMON := -std=c17 -Wall -Wextra -Werror -O2
HOST_BIN := m16_host_test
FREESTANDING_OBJ := m16_mcsfs_journal.o
SRC := ../../kernel/fs/mcsfs1j/m16_mcsfs_journal.c

.PHONY: all host freestanding audit clean
all: host freestanding audit

host: $(HOST_BIN)
	./$(HOST_BIN)
$(HOST_BIN): $(SRC)
	$(CLANG) $(CFLAGS_COMMON) -DMCSOS_M16_HOST_TEST $< -o $@

freestanding: $(FREESTANDING_OBJ)
$(FREESTANDING_OBJ): $(SRC)
	$(CLANG) $(CFLAGS_COMMON) -ffreestanding -fno-builtin -fno-stack-protector \
	  -fno-pic -mno-red-zone -target $(TARGET_TRIPLE) -c $< -o $@

audit: $(FREESTANDING_OBJ)
	nm -u $(FREESTANDING_OBJ) > nm_undefined.txt
	readelf -h $(FREESTANDING_OBJ) > readelf_header.txt
	objdump -dr $(FREESTANDING_OBJ) > objdump_disasm.txt
	sha256sum $(FREESTANDING_OBJ) > sha256sum.txt
	test ! -s nm_undefined.txt
	grep -q 'ELF64' readelf_header.txt
	grep -q 'Advanced Micro Devices X86-64' readelf_header.txt

clean:
	rm -f $(HOST_BIN) $(FREESTANDING_OBJ) nm_undefined.txt readelf_header.txt objdump_disasm.txt sha256sum.txt
```

```bash
cd tests/m16 && make clean all
# M16 host tests PASS
# (audit lulus tanpa output error — semua grep/test bernilai true)
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 13)

### Langkah 7 — Meninjau Pola Wiring dari Modul Lain (Sebelum Integrasi Kernel)

Sebelum menautkan ke `kmain.c`, dilakukan peninjauan pola integrasi yang sudah ada:

```bash
cat Makefile | head -n 80          # root Makefile: COMMON_CFLAGS, SRC_C auto-discovery, target inspect/grade
find kernel/vfs -name '*.h' -exec cat {} \;         # header VFS (M8)
find kernel/block kernel/include -iname '*block*'   # header block layer (M9)
cat fs/mcsfs1.h                                     # header MCSFS1 (M15) sebagai pola referensi
grep -n 'vfs\|mcsfs\|fs_init\|mount' kernel/core/kmain.c
```

Temuan penting: root Makefile menggunakan `find kernel -name '*.c' -not -path 'kernel/tests/*'` sehingga file baru **otomatis** ikut ter-build tanpa perlu diedit manual, sedangkan pemanggilan fungsi self-test tetap harus ditambahkan manual ke `kmain.c`.

Bukti screenshot: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 14–16)

### Langkah 8 — Membuat Header dan Self-Test Sisi Kernel

```bash
cat > kernel/fs/mcsfs1j/m16_mcsfs_journal.h <<'EOF'
#ifndef M16_FS_SELFTEST_H
#define M16_FS_SELFTEST_H
void m16_fs_selftest(void);
#endif
EOF

cat > kernel/fs/mcsfs1j/m16_fs_selftest.c <<'EOF'
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
    ...
    m16_dev_init(&g_m16_dev);
    if (m16_format(&g_m16_dev) != M16_E_OK) { KERNEL_PANIC("M16: format failed", 0); }
    if (m16_fsck(&g_m16_dev)   != M16_E_OK) { KERNEL_PANIC("M16: fsck after format failed", 0); }
    if (m16_write_file(&g_m16_dev, "hello.txt", hello, sizeof(hello)) != M16_E_OK) { ... }
    if (m16_read_file(&g_m16_dev, "hello.txt", out, sizeof(out), &out_size) != M16_E_OK) { ... }
    /* Simulate crash: commit record written, home-location write skipped. */
    if (m16_write_file_ex(&g_m16_dev, "crash.txt", crashy, sizeof(crashy), 1) != M16_E_OK) { ... }
    if (m16_journal_recover(&g_m16_dev) != M16_E_OK) { KERNEL_PANIC("M16: journal replay after committed crash failed", 0); }
    if (m16_read_file(&g_m16_dev, "crash.txt", out, sizeof(out), &out_size) != M16_E_OK) { ... }
    if (m16_fsck(&g_m16_dev) != M16_E_OK) { KERNEL_PANIC("M16: fsck after replay failed", 0); }

    log_writeln("[MCSOS:M16] mcsfs1j journal format/write/crash/replay/fsck self-test PASS");
}
EOF
wc -l kernel/fs/mcsfs1j/m16_fs_selftest.c
# 55 kernel/fs/mcsfs1j/m16_fs_selftest.c
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 17–18)

### Langkah 9 — Menautkan Self-Test ke `kmain.c`

Percobaan pertama dengan `sed` langsung gagal karena karakter `/` pada path menabrak delimiter `s/.../.../`:

```bash
sed -i 's#include "mcsos/block.h"#include "mcsos/block.h"\n#include "../fs/mcsfs1j/m16_mcsfs_journal.h"\n#include "../fs/mcsfs1j/m16_fs_selftest.h"#' kernel/core/kmain.c
# sed: -e expression #1, char 54: unknown option to `s'
```

Diperbaiki dengan delimiter `|` dan menambah include header self-test yang benar (`m16_fs_selftest.h`), lalu menambahkan pemanggilan fungsi setelah `m14_block_demo_init();`:

```bash
cp kernel/core/kmain.c /tmp/kmain.c.bak
sed -i 's|#include "mcsos/block.h"|#include "mcsos/block.h"\n#include "../fs/mcsfs1j/m16_fs_selftest.h"|' kernel/core/kmain.c
sed -i 's|    m14_block_demo_init();|    m14_block_demo_init();\n    m16_fs_selftest();|' kernel/core/kmain.c
diff /tmp/kmain.c.bak kernel/core/kmain.c
# 19a20
# > #include "../fs/mcsfs1j/m16_fs_selftest.h"
# 526a528
# >     m16_fs_selftest();
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 19)

### Langkah 10 — Build Kernel Penuh dan Boot QEMU

```bash
make clean
make            # semua object (arch/x86_64, block, core, fs/mcsfs1j, mm, sync, syscall, user, vfs) berhasil dikompilasi
                 # ld.lld -T linker.ld -o build/kernel.elf ... exit code: 0
make image       # membangun build/mcsos.iso via Limine
qemu-system-x86_64 -machine q35 -m 512M -serial file:logs/m16/qemu_serial.log \
  -display none -no-reboot -no-shutdown -cdrom build/mcsos.iso &
sleep 5; kill $QEMU_PID
cat logs/m16/qemu_serial.log
```

Bukti screenshot build lengkap: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 20)
Bukti screenshot boot QEMU: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 21–22)

### Langkah 11 — Verifikasi Runtime dengan GDB Remote

```bash
qemu-system-x86_64 -machine q35 -m 512M -serial stdio -display none -s -cdrom build/mcsos.iso &
gdb
(gdb) target remote :1234
(gdb) break m16_journal_recover
(gdb) break m16_fsck
(gdb) continue      # berhenti berulang di m16_journal_recover / m16_fsck
(gdb) bt
(gdb) quit
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 23)

### Langkah 12 — Menyalin Evidence, Commit, dan Push

```bash
cp tests/m16/m16_mcsfs_journal.o build/m16/
cp tests/m16/{nm_undefined.txt,readelf_header.txt,objdump_disasm.txt,sha256sum.txt} evidence/m16/
sha256sum kernel/fs/mcsfs1j/m16_mcsfs_journal.c kernel/fs/mcsfs1j/m16_mcsfs_journal.h | tee -a evidence/m16/sha256sum.txt
cp logs/m16/qemu_serial.log logs/m16/qemu_serial_gdb_session.log evidence/m16/
echo "# M16 host test binary (build artifact, reproducible via make)" >> .gitignore
echo "tests/m16/m16_host_test" >> .gitignore
git add -A
git commit -m "M16: implementasi write-ahead journal MCSFS1J (format/mount/recover/write/read/fsck)"
git push -u origin praktikum-m16-journal-recovery
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 17, 24–26)

---

## 11. Checkpoint Buildable

| Checkpoint | Perintah | Hasil |
|---|---|---|
| Objek journal host compile | `clang -std=c17 -Wall -Wextra -DMCSOS_M16_HOST_TEST kernel/fs/mcsfs1j/m16_mcsfs_journal.c` | Berhasil, 0 warning |
| Objek journal freestanding compile | `clang ... --target=x86_64-elf -ffreestanding -c ...` | Berhasil, ELF64 REL x86-64 |
| `tests/m16` (host+freestanding+audit) | `make -C tests/m16 clean all` | PASS, audit lulus (grep ELF64 & Advanced Micro Devices X86-64) |
| Kernel penuh | `make clean && make` | Berhasil, `build/kernel.elf` dan `build/normal/kernel/fs/mcsfs1j/m16_ncsfs_journal.o` (dan `m16_fs_selftest.o`) terbentuk |
| ISO image | `make image` (Limine) | `build/mcsos.iso` terbentuk, `sha256sum` tercatat |
| Boot QEMU headless | `qemu-system-x86_64 ... -cdrom build/mcsos.iso` | `[MCSOS:M16] mcsfs1j journal format/write/crash/replay/fsck self-test PASS`, tidak ada panic |

---

## 12. Perintah Uji dan Validasi

### 12.1 Unit Test Host (`MCSOS_M16_HOST_TEST`)

Ringkasan skenario dalam blok `main()` host test:

| Skenario | Fungsi diuji | Assertion |
|---|---|---|
| Format device kosong | `m16_format` | `M16_E_OK` |
| fsck setelah format | `m16_fsck` | `M16_E_OK` |
| Tulis file normal | `m16_write_file(dev,"hello.txt",hello,...)` | `M16_E_OK` |
| Baca file normal | `m16_read_file(dev,"hello.txt",out,...)` | `M16_E_OK`, `out[0]=='h'`, `out[8]=='6'` |
| fsck setelah tulis normal | `m16_fsck` | `M16_E_OK` |
| Simulasi crash: commit record tertulis, home-write dilewati | `m16_write_file_ex(dev,"crash.txt",crashy,...,1)` | `M16_E_OK` (transaksi committed di journal) |
| Recovery/replay setelah "crash" | `m16_journal_recover` | `M16_E_OK` |
| Baca file setelah replay | `m16_read_file(dev,"crash.txt",out,...)` | `out[0]=='c'`, `out[11]=='y'` (konten benar meski home-write awalnya dilewati) |
| fsck setelah replay | `m16_fsck` | `M16_E_OK` |
| Descriptor journal committed tapi korup | `format` ulang → `write_file_ex(dev,"bad.txt",...,1)` → korup 1 byte descriptor (`dev.blocks[JOURNAL_START+1][0] ^= 0x7u;`) | `m16_journal_recover` mengembalikan `M16_E_CORRUPT` (**fail-closed**, bukan mencoba apply target sembarang) |

```bash
clang -std=c17 -Wall -Wextra -Werror -O2 -DMCSOS_M16_HOST_TEST -o /tmp/m16_host_test kernel/fs/mcsfs1j/m16_mcsfs_journal.c
/tmp/m16_host_test
# M16 host tests PASS
```

### 12.2 Audit ELF Freestanding

```bash
make -C tests/m16 audit
# nm -u m16_mcsfs_journal.o          -> nm_undefined.txt kosong (0 baris)
# readelf -h m16_mcsfs_journal.o     -> ELF64, Advanced Micro Devices X86-64
# objdump -dr m16_mcsfs_journal.o    -> disassembly valid, tanpa relocation ke symbol tak dikenal
# sha256sum m16_mcsfs_journal.o      -> tercatat di evidence/m16/sha256sum.txt
```

### 12.3 Boot QEMU dan Serial Log

```bash
grep -c "kernel_panic_at\|PANIC\|assertion failed" logs/m16/qemu_serial.log
# 0   (tidak ada panic selama boot)
grep -n "M16" logs/m16/qemu_serial.log
# [MCSOS:M16] mcsfs1j journal format/write/crash/replay/fsck self-test PASS
```

### 12.4 Verifikasi GDB Remote

```bash
(gdb) target remote :1234
(gdb) break m16_journal_recover
(gdb) break m16_fsck
(gdb) continue
(gdb) bt
#0  m16_journal_recover ()
#1  m16_mount ()
#2  m16_fsck ()
#3  m16_fs_selftest ()
#4  kmain ()
```

Bukti screenshot bagian 12: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 10–13, 21–23)

---

## 13. Hasil Uji

| Uji | Hasil | Evidence |
|---|---|---|
| Host unit test (format/write/read/crash/replay/corrupt) | **PASS**, `exit code: 0` | Halaman 3, 10–11 |
| Kompilasi freestanding x86_64-elf | **Berhasil**, ELF64 REL, `Machine: Advanced Micro Devices X86-64` | Halaman 12–13 |
| `nm -u` (symbol undefined) | **0 baris** — tidak ada symbol undefined | Halaman 12–13 |
| `tests/m16` Makefile (host+freestanding+audit) | **PASS** semua target | Halaman 13 |
| Build kernel penuh (`make clean && make`) | **Berhasil**, exit code 0, seluruh objek ter-link ke `kernel.elf` | Halaman 20 |
| Boot QEMU headless | **Berhasil**, `MCSOS 260502 M3 kernel entered` s.d. scheduler + thread tick, tanpa panic | Halaman 21–22 |
| Log `[MCSOS:M16] mcsfs1j journal format/write/crash/replay/fsck self-test PASS` | **Muncul** pada serial log, setelah `ram0 read/write self-test PASS` dan sebelum `scheduler initialized` | Halaman 21–22 |
| GDB breakpoint `m16_journal_recover` | **Ter-trigger berulang kali** (setiap kali `m16_write_file_ex`/`m16_journal_recover` dipanggil di dalam self-test) | Halaman 23 |
| GDB breakpoint `m16_fsck` | **Ter-trigger** sesuai urutan panggilan pada `m16_fs_selftest` | Halaman 23 |
| Backtrace call chain | `kmain → m16_fs_selftest → m16_fsck → m16_mount → m16_journal_recover` sesuai desain | Halaman 23 |

---

## 14. Bukti GDB Debugging

Sesi GDB remote digunakan untuk memverifikasi bahwa kernel yang berjalan di QEMU benar-benar memanggil fungsi journal recovery sesuai desain (bukan hanya lulus karena log statis).

```text
(gdb) target remote :1234
Remote debugging using :1234
0x000000000000fff0 in ?? ()
(gdb) break m16_journal_recover
Breakpoint 1 at 0xffffffff80005050
(gdb) break m16_fsck
Breakpoint 2 at 0xffffffff80006740
(gdb) continue
Breakpoint 2, 0xffffffff80006740 in m16_fsck ()
(gdb) continue
Breakpoint 1, 0xffffffff80005050 in m16_journal_recover ()
(gdb) bt
#0  0xffffffff80005050 in m16_journal_recover ()
#1  0xffffffff80005853 in m16_mount ()
#2  0xffffffff80006772 in m16_fsck ()
#3  0xffffffff80004dab in m16_fs_selftest ()
#4  0xffffffff800016e9 in kmain ()
(gdb) continue      [berulang beberapa kali di Breakpoint 1, konsisten dengan setiap
                      m16_mount() memanggil m16_journal_recover() terlebih dahulu]
(gdb) continue
Breakpoint 2, 0xffffffff80006740 in m16_fsck ()
(gdb) quit
Detaching from program: /home/andianaaji/src/mcsos/build/kernel.elf, process 1
```

**Interpretasi:** Setiap kali file API (`m16_write_file`, `m16_read_file`, `m16_fsck`) dipanggil, alur internal selalu melalui `m16_mount()` → `m16_journal_recover()` terlebih dahulu (mount selalu mencoba recovery sebelum mengizinkan operasi lain) — ini konsisten dengan pola "mount-time recovery" yang umum pada filesystem journaling nyata (ext3/ext4, NTFS).

Bukti screenshot: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 23)

---

## 15. Analisis Teknis

### 15.1 Analisis Keberhasilan

1. **Commit dua fase berhasil diimplementasikan:** descriptor+payload ditulis lebih dulu, baru header `M16_J_COMMITTED`, baru home-location write — urutan ini memastikan bahwa jika crash terjadi sebelum header committed tertulis, transaksi dianggap tidak pernah terjadi.
2. **Replay idempoten:** `m16_journal_recover` dapat dipanggil berkali-kali (dibuktikan lewat GDB — terpanggil setiap `m16_mount`) tanpa merusak data, karena replay hanya menulis ulang payload yang sudah tervalidasi checksum-nya.
3. **Fail-closed pada korupsi:** Uji host membuktikan bahwa descriptor journal yang di-XOR (dirusak 1 byte) menyebabkan `m16_journal_recover` mengembalikan `M16_E_CORRUPT`, **bukan** mencoba menebak/menerapkan target yang tidak dikenal — ini adalah properti keamanan penting pada journaling.
4. **Educational simplification yang didokumentasikan secara eksplisit:** Karena `M16_JOURNAL_MAX_RECORDS = 8` sedangkan inode table membutuhkan 4 blok, penulisan file dipecah menjadi dua transaksi terpisah (bitmap+inode table, lalu root-dir+data). Ini didokumentasikan langsung pada komentar kode, bukan disembunyikan.
5. **Integrasi ke boot kernel berhasil tanpa panic:** Log serial menunjukkan urutan lengkap M1–M16 tanpa satu pun `kernel_panic_at`/`PANIC`/`assertion failed` (`grep -c` = 0).
6. **Audit ELF bersih:** `nm -u` = 0 baris pada objek freestanding, artinya modul journal tidak bergantung pada fungsi eksternal yang tidak terduga (murni memakai utilitas internalnya sendiri).

### 15.2 Analisis Kegagalan/Perbaikan Selama Praktikum

| Issue | Penyebab | Perbaikan | Status |
|---|---|---|---|
| `sed: unknown option to 's'` saat menambahkan include ke `kmain.c` | Path mengandung karakter `/` yang bentrok dengan delimiter default `s/.../.../ ` pada `sed` | Ganti delimiter menjadi `s|...|...|` | Fixed |
| Header self-test M16 pertama kali salah nama variabel path (belum ditemukan via `grep -c`) | Header belum ada saat `grep` pertama dijalankan (0 match, sesuai ekspektasi sebelum wiring) | Dibuat file `m16_fs_selftest.h` terlebih dahulu, baru di-`sed` ke `kmain.c` | Fixed |
| Inode table 4 blok > `M16_JOURNAL_MAX_RECORDS` jika digabung 1 transaksi dengan root-dir+data | Kapasitas record journal dibatasi 8 demi kesederhanaan pendidikan | Pisah menjadi dua transaksi commit terpisah (didokumentasikan sebagai simplifikasi) | Accepted (didokumentasikan, bukan bug) |

### 15.3 Perbandingan Desain: Sebelum (M15) vs Sesudah (M16)

| Aspek | M15 (MCSFS1) | M16 (MCSFS1J) | Perubahan |
|---|---|---|---|
| Crash-consistency | Tidak ada (write langsung) | Write-ahead journal (commit dua fase) | +journal |
| Ketertautan ke kernel | Belum ditautkan ke `kmain.c` | Ditautkan via `m16_fs_selftest()` dipanggil dari `kmain.c` | +integrasi boot |
| Media penyimpanan uji | (native, host test M15) | RAM-backed `m16_blockdev` statis, belum ke driver block M14 nyata | Sama-sama simulasi, dicatat sebagai keterbatasan |
| Recovery/fsck | fsck dasar tanpa journal | fsck + `m16_journal_recover` (mount-time recovery) | +recovery |
| Evidence otomatis | Manual per perintah | `tests/m16/Makefile` target `audit` otomatis (`test !-s`, `grep -q`) | +otomasi verifikasi |

---

## 16. Debugging dan Failure Modes

### 16.1 Failure Modes yang Ditemukan Selama Praktikum

| Failure mode | Gejala | Penyebab | Perbaikan | Bukti |
|---|---|---|---|---|
| `sed` gagal menambahkan include ke `kmain.c` | `sed: -e expression #1, char 54: unknown option to 's'` | Delimiter `/` bentrok dengan path yang mengandung `/` | Gunakan delimiter `|` pada `sed` | Halaman 19 |
| (Antisipasi) Descriptor journal korup diterapkan buta | Tidak terjadi pada implementasi ini — justru diuji dan **lulus** menolak | N/A — desain sudah fail-closed sejak awal | Verifikasi lewat host test skenario "corrupt descriptor rejected" | Halaman 10 |

### 16.2 Failure Modes yang Diantisipasi (untuk Produksi/Lanjutan)

| Failure mode | Deteksi | Dampak | Mitigasi |
|---|---|---|---|
| Power loss di tengah home-location write (setelah commit, sebagian blok tertulis) | `m16_journal_recover` tetap replay ulang seluruh payload | Tidak ada — replay bersifat idempoten, menimpa ulang blok yang sama | Sudah tertangani oleh desain replay-penuh |
| Journal penuh (>8 record) untuk file/direktori lebih besar | `m16_tx_add`/`m16_store_inode_table` mengembalikan `M16_E_NOSPC` | Operasi ditolak sebelum korupsi terjadi | Perbesar `M16_JOURNAL_MAX_RECORDS` atau split transaksi (sudah dipraktikkan untuk inode table) |
| Konkurensi multi-thread menulis journal bersamaan | Belum diuji (self-test single-threaded) | Kemungkinan race condition pada `m16_journal_commit` | Perlu lock (mutex) sebelum dipakai multi-thread — dicatat sebagai non-goal M16 |
| Belum ditautkan ke driver block M14 sesungguhnya | Komentar eksplisit di `m16_fs_selftest.c` | Data tidak benar-benar persisten lintas boot pada hardware/QEMU disk image | Direncanakan sebagai "Langkah 5" lanjutan — lihat bagian 25 |

### 16.3 Triage yang Dilakukan

Urutan diagnosis jika terjadi masalah pada M16:
1. Jalankan preflight: `./scripts/m16_preflight.sh`
2. Jalankan unit test host: `make -C tests/m16 host`
3. Jalankan audit ELF: `make -C tests/m16 audit`
4. Build kernel penuh: `make clean && make`
5. Cek serial log: `grep -c "PANIC\|assertion failed" logs/m16/qemu_serial.log` (harus 0)
6. Cek fitur M16 muncul: `grep -n "M16" logs/m16/qemu_serial.log`
7. Debug lebih dalam dengan GDB: `break m16_journal_recover`, `break m16_fsck`, `bt`

---

## 17. Prosedur Rollback

| Skenario rollback | Perintah | Data yang harus diselamatkan | Status |
|---|---|---|---|
| Kembali ke commit M15 | `git checkout 9a026c8` | Log/evidence M16 (`evidence/m16/`, `logs/m16/`) | Teruji |
| Bersihkan artefak build M16 | `make -C tests/m16 clean && make clean` | Source `kernel/fs/mcsfs1j/*.c/.h` aman (tidak terhapus) | Teruji |
| Regenerasi image kernel | `make clean && make && make image` | Image lama (`build/mcsos.iso`) bisa dihitung ulang hash-nya | Teruji |
| Revert wiring `kmain.c` saja (tanpa menghapus modul M16) | `git checkout 9a026c8 -- kernel/core/kmain.c` | Source `m16_mcsfs_journal.c`/`m16_fs_selftest.c` tetap ada, hanya tidak dipanggil saat boot | Teruji |

---

## 18. Keamanan dan Reliability

| Aspek | Analisis |
|---|---|
| Integritas data | `payload_checksum` dan `header_checksum` mendeteksi korupsi sebelum replay dijalankan — mencegah penerapan data yang rusak ke home location |
| Fail-closed | Recovery menolak (bukan menerka) ketika magic/version/state/count/checksum tidak sesuai ekspektasi — properti kritikal untuk mencegah eskalasi kerusakan |
| Validasi rentang | `m16_valid_lba` dan pengecekan `target_lba` pada descriptor mencegah tulis di luar batas `total_blocks`/`M16_MAX_BLOCKS` |
| Fault injection terkontrol | `dev->fail_after` memungkinkan simulasi kegagalan tulis ke-N tanpa memodifikasi logika inti — memudahkan pengujian negatif |
| Keterbatasan checksum | `m16_checksum` adalah hash multiplikatif sederhana (bukan CRC32/SHA), cukup untuk mendeteksi kesalahan acak pada konteks pendidikan, **tidak** untuk melawan manipulasi yang disengaja (bukan kriptografis) |
| Keterbatasan konkurensi | Tidak ada locking; aman untuk single-threaded self-test, **tidak aman** jika dipanggil dari beberapa thread/CPU sekaligus |
| Reliability boot | Log QEMU menunjukkan 0 kejadian panic selama boot dan self-test M16, memberi keyakinan awal terhadap stabilitas integrasi |

---

## 19. Pembagian Kerja (Individu)

| Aktivitas | Pelaksana |
|---|---|
| Seluruh implementasi kode, pengujian, integrasi kernel, dan penyusunan laporan | Andiana Jamaludin Malik (individu) |

---

## 20. Perbandingan M15 vs M16 (Rangkuman Milestone Filesystem)

| Kriteria | M15 | M16 |
|---|---|---|
| Fokus | Filesystem persisten minimal (MCSFS1) | Crash-consistency via journal (MCSFS1J) |
| Ditautkan ke kmain.c saat boot? | Tidak | Ya (`m16_fs_selftest()`) |
| Simulasi crash diuji? | Tidak disebutkan pada evidence M15 | Ya — dua skenario: crash-committed (replay sukses) dan crash-corrupt (ditolak fail-closed) |
| Evidence audit otomatis (Makefile target `audit`) | Tidak ada | Ada, dengan `test !-s` dan `grep -q` |
| Verifikasi runtime via GDB | Tidak disebutkan | Ada — breakpoint pada `m16_journal_recover`/`m16_fsck` dengan backtrace |

---

## 21. Git Workflow dan Commit Evidence

```bash
git add -A
git status
# new files: evidence/m16/*, kernel/fs/mcsfs1j/{m16_fs_selftest.c,.h,m16_mcsfs_journal.c,.h},
#            scripts/m16_preflight.sh, tests/m16/{Makefile,nm_undefined.txt,objdump_disasm.txt,
#            readelf_header.txt,sha256sum.txt}
# modified:  .gitignore, kernel/core/kmain.c

git commit -m "M16: implementasi write-ahead journal MCSFS1J (format/mount/recover/write/read/fsck)"
```

Ringkasan body commit (18 file berubah, 11457 insertion(+)):

```text
- Host unit test: format, fsck, write/read normal, crash-after-commit,
  journal replay, corrupt descriptor fail-closed - semua PASS
- Freestanding x86_64-elf object: nm -u kosong, ELF64 REL valid, -Werror bersih
- Integrasi kernel: m16_fs_selftest() dipanggil dari kmain() setelah M14,
  diverifikasi boot QEMU nyata tanpa panic (grep panic = 0 match)
- Verifikasi GDB remote: breakpoint di m16_journal_recover dan m16_fsck,
  kena sesuai call chain kmain -> m16_fs_selftest -> m16_fsck -> m16_mount
  -> m16_journal_recover
- Evidence lengkap: preflight log, host test Makefile+audit target,
  readelf/objdump/sha256sum, qemu serial log, gdb session log
- Readiness: siap uji QEMU dan host fault-injection terbatas (belum production)
```

```bash
git log --oneline -3
# 34c4b09 (HEAD -> praktikum-m16-journal-recovery) M16: implementasi write-ahead journal MCSFS1J ...
# 9a026c8 (praktikum-m15-mcsfs1) M15: tambahkan bukti QEMU smoke test (baseline M14, MCSFS1 belum ditautkan ke kernel)
# a849b88 M15: add MCFS1 minimal persistent filesystem

git push -u origin praktikum-m16-journal-recovery
# remote: Create a pull request for 'praktikum-m16-journal-recovery' on GitHub by visiting:
# remote:   https://github.com/JotDesu/mcsos260502_/pull/new/praktikum-m16-journal-recovery
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png` (Halaman 24–26)

---

## 22. Evidence Artefak

| Artefak | Path | Fungsi |
|---|---|---|
| `m16_mcsfs_journal.c` | `kernel/fs/mcsfs1j/m16_mcsfs_journal.c` | Implementasi inti journal + filesystem (704 baris) |
| `m16_mcsfs_journal.h` | `kernel/fs/mcsfs1j/m16_mcsfs_journal.h` | Header API M16 |
| `m16_fs_selftest.c` / `.h` | `kernel/fs/mcsfs1j/` | Self-test sisi kernel (55 baris) |
| `m16_preflight.sh` | `scripts/m16_preflight.sh` | Preflight otomatis M16 |
| `Makefile` (tests/m16) | `tests/m16/Makefile` | Target host/freestanding/audit/clean |
| `nm_undefined.txt` | `evidence/m16/nm_undefined.txt` | Bukti 0 symbol undefined |
| `readelf_header.txt` | `evidence/m16/readelf_header.txt` | ELF header objek freestanding |
| `objdump_disasm.txt` | `evidence/m16/objdump_disasm.txt` | Disassembly objek freestanding |
| `sha256sum.txt` | `evidence/m16/sha256sum.txt` | Hash source, objek, binary host test, ISO, kernel.elf |
| `qemu_serial.log` | `evidence/m16/qemu_serial.log` | Log boot QEMU lengkap termasuk pesan PASS M16 |
| `qemu_serial_gdb_session.log` | `evidence/m16/qemu_serial_gdb_session.log` | Log boot saat sesi GDB berjalan |
| `preflight.log` | `logs/m16/preflight.log` | Log hasil preflight |

---

## 23. Referensi

1. OSDev Wiki — *Journaling* dan *Ext2*, https://wiki.osdev.org
2. LLVM Project — *Clang User's Manual* (Freestanding builds, target triple), https://clang.llvm.org/docs/
3. GNU Binutils Manual — `readelf`, `objdump`, `nm`, https://sourceware.org/binutils/docs/
4. GDB Documentation — *Remote Debugging Protocol*, https://sourceware.org/gdb/documentation/
5. Panduan praktikum M16 (OS_panduan_M16.pdf), dosen pengampu Muhaemin Sidiq, S.Pd., M.Pd.
6. Template laporan praktikum (os_template_laporan_praktikum.md / disusun berdasar format laporan M3 sebelumnya)

---

## 24. Rubrik Penilaian (Self-Assessment)

| Kriteria | Bobot | Skor mandiri | Justifikasi |
|---|---|---|---|
| Kebenaran implementasi journal (commit/recover) | 30% | Terpenuhi | Host test membuktikan replay sukses dan fail-closed pada korupsi |
| Integrasi ke kernel dan bukti runtime | 25% | Terpenuhi | Log serial + sesi GDB memverifikasi eksekusi nyata, bukan klaim semata |
| Kualitas evidence (build/audit/log) | 20% | Terpenuhi | `nm -u` kosong, `readelf`/`objdump`/`sha256sum` tercatat lengkap |
| Kejujuran readiness (tidak overclaim) | 15% | Terpenuhi | Dicatat eksplisit "belum ditautkan ke driver block M14 nyata" dan "belum production" |
| Kerapian dokumentasi/laporan | 10% | Terpenuhi | Laporan mengikuti struktur 0–26 dengan bukti screenshot per halaman |

---

## 25. Catatan Keterbatasan dan Langkah Lanjutan

Komentar eksplisit pada `m16_fs_selftest.c` menyatakan:

> *"This is NOT wired to the real M14 block device yet; see M16 guide Langkah 5 note on driver ownership/locking review before that step."*

Artinya, `struct m16_blockdev` yang dipakai pada self-test M16 masih berupa **array in-memory statis** (`g_m16_dev`), **bukan** device block nyata yang dikelola oleh `kernel/block/block.c` (`ncsos_blk_register`/`ncsos_blk_read`/`ncsos_blk_write`) dari milestone M9/M14. Sebelum menautkan MCSFS1J ke device block sesungguhnya, tinjauan berikut direncanakan sebagai langkah lanjutan:

1. **Ownership device:** memutuskan siapa (subsistem mana) yang memegang pointer `ncsos_blk_device_t*` selama filesystem mount aktif.
2. **Locking:** menambahkan mutex/lock pada `m16_journal_commit`/`m16_journal_recover` karena device block nyata dapat diakses lebih dari satu thread/scheduler tick.
3. **Adapter I/O:** membungkus `ncsos_blk_read`/`ncsos_blk_write` di balik antarmuka `m16_read_block`/`m16_write_block` tanpa mengubah logika journal itu sendiri.
4. **Uji ulang fault-injection** pada device block nyata (bukan hanya `fail_after` in-memory) untuk memvalidasi bahwa properti fail-closed tetap berlaku pada I/O sesungguhnya.

Readiness M16 secara jujur dinyatakan: **"siap uji QEMU dan host fault-injection terbatas (belum production)"** — bukan siap produksi penuh.

---

## 26. Checklist Final Sebelum Pengumpulan M16

| Checklist | Status |
|---|---|
| Semua placeholder sudah diganti dengan data aktual dari evidence | Ya |
| Metadata laporan lengkap | Ya |
| Commit awal dan akhir dicatat | Ya (`9a026c8` → `34c4b09`) |
| Perintah build dan test dapat dijalankan ulang | Ya |
| Log preflight, build, dan QEMU dilampirkan | Ya |
| Artefak penting diberi hash (`sha256sum.txt`) | Ya |
| Desain journal, invariants, dan failure modes dijelaskan | Ya |
| Security/reliability dibahas (checksum, fail-closed, keterbatasan locking) | Ya |
| Verifikasi runtime dengan GDB dilampirkan | Ya |
| Readiness review tidak berlebihan (mencatat "belum production") | Ya |
| Referensi memakai format bernomor konsisten | Ya |
| Laporan disimpan sebagai Markdown | Ya |
| Bukti screenshot dipetakan per halaman PDF | Ya |

---

## Lampiran M16 — Screenshot Evidence

Seluruh bukti berasal dari satu berkas tangkapan layar (multi-halaman) yang sama:
`C:\Users\Ajot\Pictures\M16\Screenshot 2026-07-10 161808.png`

| No. | File Screenshot | Halaman PDF | Keterangan |
|---|---|---|---|
| 1 | `Screenshot 2026-07-10 161808.png` | Halaman 1 | Verifikasi toolchain (make/qemu/nm/readelf/objdump/sha256sum/git), host info, probe freestanding minimal, `cd ~/src/mcsos`, `mkdir -p`, `git checkout -b praktikum-m16-journal-recovery` |
| 2 | `Screenshot 2026-07-10 161808.png` | Halaman 2 | `find kernel tests scripts build logs evidence -maxdepth 3 -type d` — struktur direktori awal |
| 3 | `Screenshot 2026-07-10 161808.png` | Halaman 3 | Pembuatan dan eksekusi `scripts/m16_preflight.sh`, output preflight lengkap |
| 4 | `Screenshot 2026-07-10 161808.png` | Halaman 4 | Awal penulisan `m16_mcsfs_journal.c`: header, konstanta, struct `m16_super/inode/dirent/journal_header/journal_desc/jrec/tx` |
| 5 | `Screenshot 2026-07-10 161808.png` | Halaman 5 | `_Static_assert`, utilitas `m16_zero/copy/strlen_bounded/streq/checksum`, lapisan device (`valid_lba/read_block/write_block/dev_init`), bitmap, `load_inode_table` |
| 6 | `Screenshot 2026-07-10 161808.png` | Halaman 6 | `m16_store_inode_table`, `m16_tx_add`, `m16_header_checksum`, `m16_journal_clear`, `m16_journal_commit`, awal `m16_journal_recover` |
| 7 | `Screenshot 2026-07-10 161808.png` | Halaman 7 | Lanjutan `m16_journal_recover` (verifikasi checksum & replay), `m16_format`, awal `m16_mount` |
| 8 | `Screenshot 2026-07-10 161808.png` | Halaman 8 | Lanjutan `m16_mount`, `find_free_inode/block/dirent`, awal `m16_write_file_ex` |
| 9 | `Screenshot 2026-07-10 161808.png` | Halaman 9 | Lanjutan `m16_write_file_ex` (alokasi, tx pertama & kedua, `journal_commit`), `m16_write_file`, `m16_read_file`, awal `m16_fsck` |
| 10 | `Screenshot 2026-07-10 161808.png` | Halaman 10 | Lanjutan `m16_fsck`, blok `#ifdef MCSOS_M16_HOST_TEST`, `main()` host test lengkap (format/write/read/crash/replay/corrupt), `wc -l` = 704 baris |
| 11 | `Screenshot 2026-07-10 161808.png` | Halaman 11 | Kompilasi & eksekusi host test manual (PASS, exit code 0), pembuatan `tests/m16/Makefile` revisi 1, `make -C tests/m16 clean all/run` |
| 12 | `Screenshot 2026-07-10 161808.png` | Halaman 12 | `readelf -h`, `objdump -dr` (disasm `m16_dev_init`, `m16_zero`), `sha256sum`, `nm -u` (0 undefined) |
| 13 | `Screenshot 2026-07-10 161808.png` | Halaman 13 | Revisi `tests/m16/Makefile` (host+freestanding+audit dengan `-Werror -O2`), build ulang, semua target audit lulus |
| 14 | `Screenshot 2026-07-10 161808.png` | Halaman 14 | Peninjauan root `Makefile` (COMMON_CFLAGS, auto-discovery SRC_C, target `inspect`/`grade`), pencarian header VFS dan block layer |
| 15 | `Screenshot 2026-07-10 161808.png` | Halaman 15 | Isi `kernel/block/block.h`: registry device block, `ncsos_blk_register/get/count/validate_range/read` |
| 16 | `Screenshot 2026-07-10 161808.png` | Halaman 16 | Lanjutan `block.h` (`ncsos_blk_write/flush`), isi `fs/mcsfs1.h` (M15, pola referensi wiring), cuplikan `kmain.c` terkait VFS init (M13) |
| 17 | `Screenshot 2026-07-10 161808.png` | Halaman 17 | Verifikasi grep ELF, penyalinan evidence ke `build/m16`/`evidence/m16`, awal penulisan `m16_fs_selftest.c` (komentar catatan belum wired ke M14) |
| 18 | `Screenshot 2026-07-10 161808.png` | Halaman 18 | Lanjutan `m16_fs_selftest.c` (simulasi crash, journal_recover, verifikasi, `log_writeln` PASS), isi `panic.h` dan `log.h` |
| 19 | `Screenshot 2026-07-10 161808.png` | Halaman 19 | Percobaan `sed` gagal (delimiter `/`), pembuatan `m16_fs_selftest.h`, `sed` berhasil menambahkan include & pemanggilan fungsi ke `kmain.c`, `diff` verifikasi perubahan |
| 20 | `Screenshot 2026-07-10 161808.png` | Halaman 20 | `make clean` dan build kernel penuh (seluruh objek arch/block/core/fs/mm/sync/syscall/user/vfs), linking sukses |
| 21 | `Screenshot 2026-07-10 161808.png` | Halaman 21 | Pembuatan ISO (Limine), boot QEMU headless, isi `logs/m16/qemu_serial.log` lengkap hingga `[MCSOS:M16] ... self-test PASS` dan scheduler/thread tick |
| 22 | `Screenshot 2026-07-10 161808.png` | Halaman 22 | `grep -c panic` (0 match), `grep -n M16`, `sha256sum` ISO & kernel.elf, boot QEMU ulang dengan flag `-s` untuk GDB |
| 23 | `Screenshot 2026-07-10 161808.png` | Halaman 23 | Sesi GDB remote: `break m16_journal_recover`/`m16_fsck`, `continue` berulang, `bt` menunjukkan call chain, `quit` |
| 24 | `Screenshot 2026-07-10 161808.png` | Halaman 24 | Isi `.gitignore`, `git add -A`, daftar file staged, penambahan entry `.gitignore`, `git commit -m "M16: ..."` dengan body lengkap |
| 25 | `Screenshot 2026-07-10 161808.png` | Halaman 25 | `git log --oneline -3` menampilkan commit `34c4b09` (18 file, 11457 insertion), commit `9a026c8` dan `a849b88` sebelumnya |
| 26 | `Screenshot 2026-07-10 161808.png` | Halaman 26 | `git push -u origin praktikum-m16-journal-recovery` dan branch lain, pembuatan pull request di GitHub untuk tiga branch (`praktikum-m11-elf-user-loader`, `praktikum-m15-mcsfs1`, `praktikum-m16-journal-recovery`) |

---
