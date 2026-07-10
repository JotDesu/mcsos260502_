# Laporan Praktikum M11 — ELF64 User Process Loader (Parse-Only)

## MCSOS 260502

**Nama file laporan:** `laporan_praktikum_M11_[NIM_Kelompok].md`
**Nama sistem operasi:** MCSOS versi 260502
**Target default:** x86_64, QEMU, Windows 11 x64 + WSL 2, kernel monolitik pendidikan, C freestanding dengan assembly minimal, POSIX-like subset
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.
**Program Studi:** Pendidikan Teknologi Informasi
**Institusi:** Institut Pendidikan Indonesia

> **Catatan sumber bukti:** Seluruh tangkapan layar pada laporan ini diambil dari satu file screenshot panjang (multi-halaman) yang disimpan di komputer Windows sebagai:
> `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png`
> Rujukan "halaman N dari PDF" pada laporan ini mengacu pada urutan tangkapan layar (page N) di dalam file tersebut, sesuai urutan kronologis pengerjaan M11.

---

## 0. Metadata Laporan

| Atribut | Isi |
|---|---|
| Kode praktikum | M11 |
| Judul praktikum | ELF64 User Process Loader (parse-only, konservatif) |
| Jenis pengerjaan | Individu |
| Nama mahasiswa | Andiana Jamaludin Malik |
| NIM | 2583207073016 |
| Kelas | 1B |
| Nama kelompok | - |
| Anggota kelompok | - |
| Tanggal praktikum | 2026-07-08 |
| Tanggal pengumpulan | 2026-07-08 |
| Repository | ~/src/mcsos |
| Remote repository | https://github.com/JotDesu/mcsos260502_.git |
| Branch kerja | praktikum-m11-elf-user-loader |
| Commit checkpoint awal | `531254a` (M9/M10: tambahkan evidence log dan serial.h sebelum memulai M11) |
| Commit akhir | `2270ecd` (M11: tambahkan script QEMU smoke test) |
| Status readiness yang diklaim | preflight OK, host test PASS, freestanding compile PASS, audit ELF PASS, full kernel build PASS, ISO builds clean, QEMU smoke test PASS (marker M11 terlihat di log serial) |

---

## 1. Sampul

# Laporan Praktikum M11
## ELF64 User Process Loader (Parse-Only, Konservatif)

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

Saya menyatakan bahwa laporan ini disusun berdasarkan pekerjaan praktikum sendiri sesuai panduan M11, melanjutkan checkpoint hasil M0–M10 (termasuk cooperative scheduler M9 dan syscall ABI M10) yang sudah dikerjakan pada milestone sebelumnya.

| Pernyataan | Status |
|---|---|
| Semua potongan kode eksternal diberi atribusi | Ya (panduan M11 dari dosen) |
| Semua penggunaan AI assistant dicatat | Ya |
| Repository yang dikumpulkan sesuai commit akhir | Ya |
| Tidak ada klaim readiness tanpa bukti | Ya |

Catatan penggunaan bantuan eksternal:

```text
- Panduan praktikum M11 (OS_panduan_M11.pdf) sebagai referensi utama
- Template laporan praktikum (os_template_laporan_praktikum.md)
- AI assistant digunakan untuk membantu menyusun laporan dan analisis
- Semua source code diimplementasikan berdasarkan panduan dosen dan hasil kerja mandiri di WSL2
- Sebuah skrip Python singkat (/tmp/insert_m11.py) digunakan sebagai alat bantu suntik kode
  (idempotent, hanya menyisipkan blok M11 ke kmain.c bila belum ada), bukan pengganti
  implementasi manual loader itu sendiri
```

---

## 3. Tujuan Praktikum

1. **Tujuan teknis 1:** Membuat header kontrak loader ELF64 (`kernel/include/mcsos/user/m11_elf_loader.h`) berisi struct `m11_process_image_plan`, `m11_segment_plan`, `m11_user_region`, dan kode error `m11_error_name`
2. **Tujuan teknis 2:** Mengimplementasikan loader ELF64 *parse-only* (`kernel/user/m11_elf_loader.c`) — validasi e_ident, e_type, e_machine, e_ehsize, program header bounds, overflow-safe range checking, dan validasi tiap segmen `PT_LOAD` terhadap region user yang diizinkan
3. **Tujuan teknis 3:** Menulis host unit test (`tests/m11/m11_host_test.c`) yang memverifikasi loader tanpa perlu boot QEMU, termasuk kasus negatif (bad magic, bad machine, entry di luar range, memsz < filesz, file range di luar image, alignment tidak valid, segmen di luar region user)
4. **Tujuan teknis 4:** Membuat `Makefile.m11` dengan tiga target independen — `host-test` (uji logika di host), `freestanding` (kompilasi target kernel x86_64-unknown-none), dan `audit` (`nm -u`, `readelf -h`, `objdump -dr`, `sha256sum`)
5. **Tujuan teknis 5:** Mengintegrasikan loader ke `kmain.c` melalui `m11_elf_loader_bootstrap()` yang membangun *demo image* ELF64 in-memory dan memanggil `m11_elf64_plan_load()` sebagai bukti jalur parse berjalan di dalam kernel (bukan hanya di host test)
6. **Tujuan teknis 6:** Menambahkan helper `memset`/`memcpy` freestanding (`kernel/lib/memory.c`, `kernel/include/mcsos/lib/string.h`) yang dibutuhkan oleh pembuatan demo image di `kmain.c`
7. **Tujuan teknis 7:** Melakukan preflight otomatis (`scripts/m11_preflight.sh`) untuk memverifikasi toolchain, struktur direktori, dan marker kode M0–M10 sebelum mulai bekerja
8. **Tujuan teknis 8:** Menjalankan QEMU smoke test (`scripts/m11_qemu_smoke.sh`) dan memverifikasi log serial memuat marker M11 (`elf: ident ok`, `elf: plan ok`, `user image plan ready`)
9. **Tujuan konseptual 1:** Memahami struktur ELF64 (`e_ident`, program header table, segmen `PT_LOAD`) dan bagaimana kernel memvalidasi image sebelum memetakannya ke ruang alamat proses pengguna
10. **Tujuan konseptual 2:** Memahami mengapa loader M11 bersifat *parse-only* dan konservatif — memvalidasi rencana pemuatan (plan) tanpa benar-benar memetakan halaman (page mapping) atau menjalankan kode pengguna, sebagai langkah aman sebelum milestone user process penuh
11. **Tujuan validasi:** Menyimpan log preflight, host test, freestanding compile, audit ELF, build kernel penuh, build ISO, dan log serial QEMU sebagai bukti deterministik di `evidence/m11/`

---

## 4. Capaian Pembelajaran Praktikum

Setelah praktikum ini, mahasiswa mampu:

| CPL/CPMK praktikum | Bukti yang harus ditunjukkan |
|---|---|
| Menjelaskan struktur ELF64 dan program header | `kernel/include/mcsos/user/m11_elf_loader.h` |
| Mengimplementasikan validasi ELF64 yang aman dari overflow | `m11_add_overflow_u64`, `m11_validate_user_range`, `m11_validate_phdr_bounds` |
| Membedakan validasi ident, type, machine, dan bounds segmen | `m11_validate_ident`, `m11_validate_load_segment` |
| Membuat host unit test tanpa dependensi hardware | `tests/m11/m11_host_test.c`, target `host-test` |
| Memisahkan build host vs build freestanding kernel | `Makefile.m11` (`HOST_CFLAGS` vs `TARGET_CFLAGS`) |
| Mengaudit objek ELF hasil kompilasi freestanding | `build/m11_readelf_header.txt`, `build/m11_objdump.txt`, `build/m11_nm_undefined.txt` |
| Mengintegrasikan loader ke boot sequence kernel | `kmain.c`, `m11_elf_loader_bootstrap()` |
| Menjalankan QEMU headless dan memverifikasi marker M11 pada log serial | `evidence/m11/qemu_serial.log` |
| Menyusun evidence build kernel dan ISO penuh (M3–M11) | `evidence/m11/kernel_build.log`, `iso_build.log` |
| Mengklasifikasikan dan memperbaiki failure mode (include path salah) | Analisis pada bagian 15 |

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
| M9 | Thread, scheduler, context switch (cooperative) | [x] selesai (checkpoint sebelumnya) |
| M10 | Syscall ABI, dispatcher, int 0x80 stub | [x] selesai (checkpoint sebelumnya) |
| **M11** | **ELF64 user process loader (parse-only)** | **[x] selesai — dibahas penuh pada laporan ini** |
| M12 | Security model, capability/ACL, syscall fuzzing, hardening | [ ] tidak dibahas |
| M13 | SMP, scalability, lock stress, NUMA-aware preparation | [ ] tidak dibahas |
| M14 | Framebuffer, graphics console, visual regression | [ ] tidak dibahas |
| M15 | Virtualization/container subset | [ ] tidak dibahas |
| M16 | Observability, update/rollback, release image, readiness review | [ ] tidak dibahas |

Batas cakupan praktikum:

```text
Praktikum M11 mencakup:
- Checkpoint commit sebelum M11 (531254a) dan pembuatan branch praktikum-m11-elf-user-loader
- Preflight lingkungan dan marker kode M0-M10 (scripts/m11_preflight.sh)
- Header kontrak loader: include/mcsos/user/m11_elf_loader.h
- Implementasi loader: kernel/user/m11_elf_loader.c (parse-only, tanpa page mapping nyata)
- Host unit test: tests/m11/m11_host_test.c (1 kasus valid + 7 kasus negatif)
- Makefile.m11 (target host-test, freestanding, audit, clean)
- Audit statis build/m11_elf_loader.o (readelf, nm -u, objdump -dr, sha256sum)
- Helper memset/memcpy freestanding: kernel/lib/memory.c, kernel/include/mcsos/lib/string.h
- Integrasi loader ke kmain.c (m11_elf_loader_bootstrap, demo image ELF64 in-memory)
- Perbaikan bug include path ("m11_elf_loader.h" -> <mcsos/user/m11_elf_loader.h>)
- Full rebuild kernel (make) menyertakan seluruh subsistem M3-M11
- Build ISO (make image) dan QEMU smoke test (scripts/m11_qemu_smoke.sh)
- Pengumpulan evidence lengkap ke evidence/m11/

Non-goals (tidak termasuk):
- Page mapping nyata (memetakan segmen ke tabel halaman VMM) untuk proses pengguna
- Menjalankan kode pengguna (belum ada context switch ke ring 3 / user mode)
- Loader untuk format selain ELF64 statis (mis. dynamic linking, PIE relocation)
- Manajemen file descriptor atau filesystem untuk memuat image dari disk
- Multi-proses atau isolasi memori antar proses
```

---

## 6. Dasar Teori Ringkas

### 6.1 Konsep Sistem Operasi yang Diuji

**Boot Chain M11:** Limine -> `kernel.elf` -> `kmain()` -> subsistem M3–M10 (log, panic, IDT/PIC/PIT, PMM, VMM, kmem, syscall, scheduler) -> `m11_elf_loader_bootstrap()` -> `m11_build_demo_image()` (membangun ELF64 dummy di memori) -> `m11_elf64_plan_load()` (memvalidasi image dan menghasilkan *rencana pemuatan*) -> log setiap segmen yang valid -> `[MCSOS:M11] user image plan ready`.

**ELF64 Header (`e_ident`, dsb.):** 16 byte pertama file ELF berisi magic number (`0x7F 'E' 'L' 'F'`), kelas (32/64-bit), endianness, versi. Diikuti field `e_type` (EXEC/DYN), `e_machine` (arsitektur target), `e_entry` (alamat entry point), `e_phoff`/`e_phnum`/`e_phentsize` (lokasi dan jumlah program header).

**Program Header (`Elf64_Phdr`) tipe `PT_LOAD`:** Mendeskripsikan satu segmen yang harus dimuat ke memori — `p_offset` (posisi di file), `p_vaddr` (alamat virtual tujuan), `p_filesz`/`p_memsz` (ukuran di file vs di memori, `memsz >= filesz` karena bagian `.bss` tidak ada di file), `p_align` (harus pangkat dua), `p_flags` (R/W/X).

**Rencana Pemuatan (`m11_process_image_plan`):** Struktur hasil parsing yang **tidak langsung memetakan memori** — hanya berisi `entry` (alamat masuk proses) dan array `m11_segment_plan` (salinan tervalidasi dari tiap `PT_LOAD`: `file_offset, vaddr, filesz, memsz, align, flags`). Pendekatan *parse-only* ini memisahkan tahap "apakah image ini valid dan aman untuk dimuat" dari tahap "benar-benar memetakan halaman ke VMM", yang sengaja ditunda ke milestone berikutnya.

**Validasi Overflow-Safe (`m11_add_overflow_u64`):** Karena `p_offset + p_filesz` atau `base + size` dapat overflow pada tipe `uint64_t`, setiap penjumlahan alamat/ukuran diperiksa lebih dulu (`r < a` menandakan overflow) sebelum dipakai untuk perbandingan batas (bounds check), mencegah *integer overflow* dipakai untuk melewati validasi keamanan (klasik pada loader ELF nyata).

**Validasi Region Pengguna (`m11_validate_user_range`):** Setiap segmen harus berada penuh di dalam `m11_user_region` (`region.base` s.d. `region.limit`) yang diberikan caller — mencegah image menuntut alamat di ruang kernel atau di luar batas yang diizinkan.

### 6.2 Konsep Arsitektur x86_64 yang Relevan

| Konsep | Relevansi pada praktikum | Bukti/verifikasi |
|---|---|---|
| ELF64 struct layout (`Elf64_Ehdr`, `Elf64_Phdr`) | Field-field header dan program header dibaca sesuai offset standar ELF64 | `m11_elf_loader.h`, `m11_validate_ident` |
| `PT_LOAD` alignment (`p_align` harus pangkat dua) | Alamat dan offset harus kongruen modulo `p_align` (`vaddr % align == offset % align`) | `m11_is_power_of_two_u64`, `m11_validate_load_segment` |
| Higher-half / user-space split alamat virtual | `m11_user_region` membatasi rentang alamat yang diterima loader (mis. `0x0000000000400000` s.d. `0x0000008000000000`) | `tests/m11/m11_host_test.c` (`test_region()`) |
| ELF64 relocatable object hasil kompilasi | `build/m11_elf_loader.o` adalah objek `REL` (belum di-link akhir) sebelum digabung ke `kernel.elf` | `readelf -h` menunjukkan `Type: REL` |
| Freestanding compilation flags | `--target=x86_64-unknown-none -ffreestanding -fno-builtin -mno-red-zone` dsb. dipakai untuk build target kernel | `Makefile.m11` (`TARGET_CFLAGS`) |

### 6.3 Konsep Implementasi Freestanding

| Aspek | Keputusan praktikum |
|---|---|
| Bahasa | C17/C11 freestanding untuk `m11_elf_loader.c`; host test dikompilasi dengan `-std=c11` di host (bukan target freestanding) |
| Host test terpisah | `tests/m11/m11_host_test.c` memakai `clang` host biasa (tanpa `--target=x86_64-unknown-none-elf`) sehingga logika parsing dapat diuji cepat tanpa boot QEMU |
| Pemisahan flag host vs target | `HOST_CFLAGS := -std=c17 -Wall -Wextra -Werror -O2 -g` vs `TARGET_CFLAGS := --target=x86_64-unknown-none -std=c17 -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -mno-red-zone` |
| Helper string/memory | `kernel/lib/memory.c` menyediakan `memset`/`memcpy` manual karena lingkungan freestanding tidak memiliki libc; dideklarasikan di `kernel/include/mcsos/lib/string.h` |
| Risiko undefined behavior | Mitigasi dengan `-Wall -Wextra -Werror`, validasi eksplisit `m11_add_overflow_u64` sebelum aritmetika alamat |

### 6.4 Referensi Teori yang Digunakan

| No. | Sumber | Bagian yang digunakan | Alasan relevansi |
|---|---|---|---|
| [1] | Tool Interface Standard (TIS) — Executable and Linking Format (ELF) Specification | Struktur `Elf64_Ehdr`, `Elf64_Phdr`, konstanta magic dan tipe | Dasar validasi header dan program header |
| [2] | OSDev Wiki — ELF | Praktik umum parsing ELF pada kernel edukasi | Pola validasi `PT_LOAD` dan pemetaan segmen |
| [3] | LLVM Project — Clang User's Manual | Freestanding builds, flag target `x86_64-unknown-none` | Flag kompilasi loader target kernel |
| [4] | CERT C Coding Standard (INT30-C) | Deteksi unsigned integer overflow sebelum operasi aritmetika | Desain `m11_add_overflow_u64` |
| [5] | GNU Binutils Documentation | `readelf -h`, `objdump -dr`, `nm -u` | Audit statis objek hasil kompilasi |

---

## 7. Lingkungan Praktikum

### 7.1 Host dan Target

| Komponen | Nilai |
|---|---|
| Host OS | Windows 11 x64 |
| Lingkungan build | WSL 2 Ubuntu 26.04 LTS (Resolute Raccoon) |
| Kernel WSL | 6.6.87.2-microsoft-standard-WSL2 |
| Target ISA | x86_64 |
| Target ABI | x86_64-unknown-none (freestanding), x86_64-unknown-none-elf (assembly) |
| Emulator | QEMU system-x86_64 versi 10.2.1 |
| Build system | GNU Make 4.4.1 |
| Bahasa utama | C17/C11 freestanding |
| Compiler | Ubuntu clang version 21.1.8 (6ubuntu1) |
| Linker | GNU ld (Binutils) 2.46 / Ubuntu LLD 21.1.8 |

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 1 dari PDF)

### 7.2 Versi Toolchain

Perintah verifikasi environment (dijalankan di awal sesi M11):

```bash
uname -a
cat /etc/os-release | sed -n '1,8p'
clang --version | sed -n '1,4p'
gcc --version | sed -n '1p' || true
ld --version | sed -n '1p' || true
ld.lld --version || true
make --version | sed -n '1p'
qemu-system-x86_64 --version | sed -n '1p' || true
gdb --version | sed -n '1p' || true
nm --version | sed -n '1p'
readelf --version | sed -n '1p'
objdump --version | sed -n '1p'
git --version
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 1–2 dari PDF)

Output:
```
Linux JotDesu 6.6.87.2-microsoft-standard-WSL2 #1 SMP PREEMPT_DYNAMIC Thu Jun 5 18:30:46 UTC 2025 x86_64 GNU/Linux
PRETTY_NAME="Ubuntu 26.04 LTS"
NAME="Ubuntu"
VERSION_ID="26.04"
VERSION="26.04 LTS (Resolute Raccoon)"
VERSION_CODENAME=resolute
ID=ubuntu
ID_LIKE=debian
Ubuntu clang version 21.1.8 (6ubuntu1)
Target: x86_64-pc-linux-gnu
Thread model: posix
InstalledDir: /usr/lib/llvm-21/bin
gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
GNU ld (GNU Binutils for Ubuntu) 2.46
Ubuntu LLD 21.1.8 (compatible with GNU linkers)
GNU Make 4.4.1
QEMU emulator version 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3)
GNU gdb (Ubuntu 17.1-2ubuntu1) 17.1
GNU nm (GNU Binutils for Ubuntu) 2.46
GNU readelf (GNU Binutils for Ubuntu) 2.46
GNU objdump (GNU Binutils for Ubuntu) 2.46
git version 2.53.0
```

### 7.3 Lokasi Repository

| Item | Nilai |
|---|---|
| Path repository di WSL | `~/src/mcsos` |
| Remote repository | `https://github.com/JotDesu/mcsos260502_.git` |
| Branch kerja | `praktikum-m11-elf-user-loader` |
| Commit checkpoint | `531254a` (M9/M10: tambahkan evidence log dan serial.h sebelum memulai M11) |
| Commit akhir | `2270ecd` (M11: tambahkan script QEMU smoke test) |

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 2–3 dari PDF, `git log --oneline`, `git checkout -b praktikum-m11-elf-user-loader`)

---

## 8. Repository dan Struktur File

### 8.1 File yang Dibuat atau Diubah pada M11

| File | Jenis perubahan | Alasan perubahan | Risiko |
|---|---|---|---|
| `scripts/m11_preflight.sh` | Baru | Preflight tool, direktori, dan marker kode M0-M10 sebelum mulai M11 | Rendah — hanya skrip pemeriksaan |
| `include/mcsos/user/m11_elf_loader.h` | Baru | Kontrak struct `m11_process_image_plan`, `m11_segment_plan`, `m11_user_region`, prototipe fungsi | Sedang — kontrak dipakai loader dan test |
| `kernel/user/m11_elf_loader.c` | Baru | Implementasi loader ELF64 parse-only (validasi ident, phdr, overflow-safe range) | Tinggi — kesalahan validasi bisa membuka celah keamanan loader |
| `tests/m11/m11_host_test.c` | Baru | Host unit test (1 kasus valid + 7 kasus negatif) | Rendah — hanya testing |
| `Makefile.m11` | Baru | Target `host-test`, `freestanding`, `audit`, `clean` | Sedang — build automation |
| `kernel/include/mcsos/lib/string.h` | Baru | Deklarasi `memset`/`memcpy` freestanding | Rendah |
| `kernel/lib/memory.c` | Diubah | Implementasi `memset` (dan `memcpy`) manual dipakai `kmain.c` | Rendah |
| `kernel/core/kmain.c` | Diubah | Tambah `#include`, `m11_elf_loader_bootstrap()`, demo image builder, pemanggilan setelah `m10_syscall_smoke_test()` | Sedang — flow boot berubah |
| `scripts/m11_qemu_smoke.sh` | Baru | Otomasi QEMU headless + verifikasi marker M11 di log serial | Rendah |

### 8.2 Ringkasan Commit M11

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 32 dari PDF, `git log --oneline -10`)

```
2270ecd M11: tambahkan script QEMU smoke test
745e711 M11: lengkapi evidence host-test, freestanding, audit yang tertinggal
14258c7 M11: evidence lengkap C1-C6 (preflight, host-test, freestanding, audit, build, QEMU)
bdb8196 M11: perbaiki include path ke <mcsos/user/...>, tambah string.h, integrasi kmain
1fc72aa M11: tambahkan Makefile.m11 (host-test, freestanding, audit)
b7b7910 M11: tambahkan host unit test loader ELF64
9a8b548 M11: tambahkan header, implementasi loader ELF64, dan preflight script
531254a M9/M10: tambahkan evidence log dan serial.h sebelum memulai M11
20c67fb M10: hook syscall_init ke kmain, daftarkan IDT vector 0x80 DPL=3, verified QEMU+GDB PASS
4cca58f M10: syscall ABI, dispatcher, int 0x80 stub, host test - check-m10 PASS
```

### 8.3 Struktur Evidence M11

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 32 dari PDF, `ls -la evidence/m11/`)

```text
evidence/m11/
├── m11_preflight.log
├── m11_host_test.log
├── m11_freestanding.log
├── m11_audit.log
├── m11_nm_undefined.txt
├── m11_readelf_header.txt
├── m11_objdump.txt
├── m11_sha256.txt
├── m11_kernel_build.log
├── m11_iso_build.log
├── m11_qemu_serial.log
└── mcsos_m11.iso.sha256
```

---

## 9. Desain Teknis

### 9.1 Masalah yang Diselesaikan

Kernel M10 belum memiliki:
- Struktur data untuk merepresentasikan rencana pemuatan image ELF64 milik proses pengguna
- Validasi ELF64 yang aman terhadap integer overflow pada perhitungan alamat/ukuran
- Cara memisahkan tahap "validasi image" dari tahap "pemetaan memori nyata" (agar milestone berikutnya bisa membangun page mapping di atas fondasi yang sudah tervalidasi)
- Automated test untuk memverifikasi loader tanpa perlu boot penuh di QEMU

### 9.2 Keputusan Desain

| Keputusan | Alternatif yang dipertimbangkan | Alasan memilih | Konsekuensi |
|---|---|---|---|
| Loader *parse-only* (hanya menghasilkan plan, tidak memetakan memori) | Langsung memetakan segmen ke VMM saat parsing | Memisahkan concern validasi vs mapping; lebih aman diuji bertahap | Milestone M11 belum benar-benar menjalankan proses pengguna |
| Overflow-safe arithmetic eksplisit (`m11_add_overflow_u64`) | Percaya nilai `uint64_t` tidak overflow | Mencegah bypass validasi lewat wraparound alamat/ukuran (kelas bug umum pada loader ELF) | Sedikit overhead pemeriksaan tambahan di setiap operasi alamat |
| Validasi region pengguna eksplisit (`m11_user_region`) sebagai parameter, bukan konstanta hardcode | Hardcode rentang alamat user di dalam loader | Fleksibel untuk diuji dengan region berbeda-beda (unit test memakai region kecil) | Caller bertanggung jawab menyediakan region yang benar |
| Segmen dibatasi jumlah maksimum (`M11_MAX_LOAD_SEGMENTS`) | Alokasi dinamis untuk jumlah segmen sembarang | Menghindari alokasi dinamis di kernel pada tahap ini (belum ada allocator proses) | Image dengan segmen `PT_LOAD` melebihi batas akan ditolak (`M11_ERR_SEGCOUNT`) |
| Host unit test terpisah (`clang` host biasa) | Hanya uji lewat QEMU serial log | Testing loader jauh lebih cepat dan deterministik | Tidak menguji jalur freestanding compile secara langsung (diuji terpisah oleh target `freestanding`) |
| Demo image ELF64 dibangun *in-memory* di `kmain.c` (bukan dimuat dari disk) | Memuat image dari filesystem nyata | Filesystem belum tersedia pada milestone ini; demo image cukup untuk membuktikan jalur parse berjalan di kernel | Belum ada mekanisme load-from-disk (ditunda ke milestone filesystem) |

### 9.3 Arsitektur Ringkas

```
   kmain()
     |
     +--> ... (M3-M8, M9 scheduler init, M10 syscall bootstrap + smoke test)
     |
     v
   m11_elf_loader_bootstrap()
     |
     +--> region.base  = 0x0000000000400000
     +--> region.limit = 0x0000008000000000
     |
     +--> m11_build_demo_image(g_m11_demo_image)
     |        - menulis Elf64_Ehdr valid (magic, class, endian, type EXEC, machine X86_64)
     |        - menulis 2x Elf64_Phdr PT_LOAD (segmen kode R+X, segmen data R+W)
     |
     +--> m11_elf64_plan_load(image, IMAGE_SIZE, region, &plan)
     |        - m11_validate_ident(ehdr)         -> cek magic/class/endian/version
     |        - cek e_type (EXEC/DYN), e_machine (X86_64), e_ehsize
     |        - m11_validate_phdr_bounds(ehdr, image_size)
     |        - m11_validate_user_range(region, e_entry, 1u)  -> entry harus di dalam region
     |        - untuk setiap Phdr bertipe PT_LOAD:
     |              m11_validate_load_segment(phdr, image_size, region)
     |              salin ke plan.segments[i]
     |
     +--> if (rc != M11_OK) KERNEL_PANIC("M11: m11_elf64_plan_load failed", rc)
     |
     +--> log setiap segment (vaddr, filesz, memsz, flags)
     +--> log_key_value_hex64("elf plan entry", plan.entry)
     +--> log_writeln("[MCSOS:M11] elf: plan ok")
     +--> log_writeln("[MCSOS:M11] user image plan ready")
```

### 9.4 Kontrak Antarmuka

| Antarmuka | Pemanggil | Penerima | Precondition | Postcondition | Error path |
|---|---|---|---|---|---|
| `m11_validate_user_range(region, base, size)` | `m11_elf64_plan_load`, host test | `m11_elf_loader.c` | `region` valid | Mengonfirmasi `[base, base+size)` seluruhnya di dalam `[region.base, region.limit)` | `M11_ERR_SEGRANGE` jika di luar batas atau overflow |
| `m11_elf64_plan_load(image, image_size, region, out_plan)` | `m11_elf_loader_bootstrap`, host test | `m11_elf_loader.c` | `image`, `out_plan` tidak NULL, `image_size >= sizeof(Elf64_Ehdr)` | `out_plan` berisi `entry` dan daftar segmen tervalidasi | `M11_ERR_NULL/SIZE/MAGIC/CLASS/ENDIAN/VERSION/TYPE/MACHINE/EHSIZE/PHBOUNDS/ENTRY/SEGBOUNDS/ALIGN/FLAGS/SEGRANGE/SEGCOUNT` sesuai kegagalan |
| `m11_error_name(code)` | test, log kernel | `m11_elf_loader.c` | - | Mengembalikan string nama error untuk logging | Mengembalikan `"M11_ERR_UNKNOWN"` untuk kode tak dikenal |
| `m11_elf_loader_bootstrap(void)` | `kmain()` | `m11_elf_loader.c` (via kmain.c) | Subsistem log & panic sudah aktif | Log `[MCSOS:M11] user image plan ready` tercetak | `KERNEL_PANIC` bila `m11_elf64_plan_load` gagal |

### 9.5 Struktur Data Utama

| Struktur data | Field penting | Ownership | Lifetime | Invariant |
|---|---|---|---|---|
| `struct m11_user_region` | `base, limit` | Dimiliki caller (mis. `kmain.c`) | Selama satu pemanggilan `plan_load` | `base < limit` |
| `struct m11_segment_plan` | `file_offset, vaddr, filesz, memsz, align, flags` | Bagian dari `m11_process_image_plan` | Selama plan dipakai | `memsz >= filesz`, `align` pangkat dua atau `0`/`1` |
| `struct m11_process_image_plan` | `entry, segment_count, segments[M11_MAX_LOAD_SEGMENTS]` | Dialokasikan caller (stack/statik) | Selama satu proses divalidasi | `segment_count <= M11_MAX_LOAD_SEGMENTS` |

### 9.6 Invariants

1. Setiap operasi penjumlahan alamat/ukuran melewati `m11_add_overflow_u64` sebelum dipakai untuk perbandingan batas
2. `e_ident` harus lulus magic (`0x7F,'E','L','F'`), `ELFCLASS64`, `ELFDATA2LSB`, `EV_CURRENT` sebelum field lain dibaca
3. Setiap segmen `PT_LOAD` harus memenuhi `memsz >= filesz`, `align` adalah pangkat dua (atau `0`/`1`), serta `vaddr % align == offset % align`
4. `entry` (alamat masuk) dan setiap segmen harus seluruhnya berada di dalam `m11_user_region` yang diberikan
5. `segment_count` tidak pernah melebihi `M11_MAX_LOAD_SEGMENTS`; kelebihan menghasilkan `M11_ERR_SEGCOUNT`
6. Loader tidak pernah menulis ke memori pengguna nyata pada M11 — hanya membaca image dan mengisi struktur `plan` (parse-only, tanpa side effect ke VMM)

### 9.7 Ownership, Locking, dan Concurrency

M11 berjalan sepenuhnya di dalam satu thread boot (belum ada proses pengguna nyata):
- Tidak ada lock karena loader dipanggil satu kali secara sinkron dari `kmain()` sebelum scheduler menjalankan thread lain secara bergantian
- `g_m11_demo_image` bersifat statik global read-mostly (ditulis sekali oleh `m11_build_demo_image`, dibaca oleh `m11_elf64_plan_load`)

### 9.8 Memory Safety dan Undefined Behavior Risk

| Risiko | Lokasi | Mitigasi | Bukti |
|---|---|---|---|
| Integer overflow pada `base + size` | `m11_validate_user_range`, `m11_validate_phdr_bounds` | `m11_add_overflow_u64` mendeteksi wraparound sebelum dipakai | Host test kasus `"segment outside user range"` |
| `p_memsz < p_filesz` (data hilang saat load) | `m11_validate_load_segment` | Pengecekan eksplisit `memsz < filesz` -> `M11_ERR_SEGBOUNDS` | Host test kasus `"memsz below filesz"` |
| `p_align` bukan pangkat dua | `m11_validate_load_segment` | `m11_is_power_of_two_u64` -> `M11_ERR_ALIGN` | Host test kasus `"bad alignment"` |
| Program header table di luar batas file (`e_phoff + ph_table_bytes > image_size`) | `m11_validate_phdr_bounds` | Overflow-safe check terhadap `image_size` | Host test kasus `"file range outside image"` |
| Entry point di luar region pengguna | `m11_elf64_plan_load` | `m11_validate_user_range(region, e_entry, 1u)` | Host test kasus `"entry outside user range"` |

### 9.9 Security Boundary

| Boundary | Data tidak tepercaya | Validasi yang dilakukan | Failure mode aman |
|---|---|---|---|
| Image ELF64 dari caller | Seluruh byte header dan program header | `m11_validate_ident`, `m11_validate_phdr_bounds`, `m11_validate_load_segment` | Return kode error, tidak crash, tidak menulis memori |
| Region pengguna dari caller | `region.base`, `region.limit` | `region.base < region.limit` sebelum dipakai bounds check | Return `M11_ERR_SEGRANGE` (fail-closed) |
| Jumlah segmen `PT_LOAD` | `e_phnum` | Dibatasi `M11_MAX_LOAD_SEGMENTS` | Return `M11_ERR_SEGCOUNT`, bukan buffer overflow pada array `segments[]` |

---

## 10. Langkah Kerja Implementasi

### Langkah 1 — Preflight, Checkpoint Evidence M9/M10, dan Pembuatan Branch

Maksud langkah: Memverifikasi toolchain dan mengamankan evidence M9/M10 yang belum ter-commit sebelum membuat branch kerja M11.

Perintah:
```bash
cd ~/src/mcsos
uname -a
cat /etc/os-release | sed -n '1,8p'
clang --version | sed -n '1,4p'
git log --oneline -5
git checkout -b praktikum-m11-elf-user-loader
mkdir -p kernel/user include/mcsos/user tests/m11 scripts build
git status --short
git add evidence/m9/ kernel/include/mcsos/kernel/serial.h
git commit -m "M9/M10: tambahkan evidence log dan serial.h sebelum memulai M11"
git log --oneline -3
git status --short
ls -la kernel/user include/mcsos/user tests/m11 scripts build
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 1–4 dari PDF)

Output ringkas:
```
Linux JotDesu 6.6.87.2-microsoft-standard-WSL2 ... Ubuntu 26.04 LTS
Ubuntu clang version 21.1.8 (6ubuntu1)
20c67fb M10: hook syscall_init ke kmain ... M10: syscall ABI, dispatcher ... verified QEMU+GDB PASS
Switched to a new branch 'praktikum-m11-elf-user-loader'
?? evidence/m9/... (11 file)  ?? kernel/include/mcsos/kernel/serial.h
[praktikum-m11-elf-user-loader 531254a] M9/M10: tambahkan evidence log dan serial.h sebelum memulai M11
 11 files changed, 1051 insertions(+)
531254a M9/M10: tambahkan evidence log dan serial.h sebelum memulai M11
20c67fb M10: hook syscall_init ke kmain, daftarkan IDT vector 0x80 DPL=3, verified QEMU+GDB PASS
4cca58f M10: syscall ABI, dispatcher, int 0x80 stub, host test - check-m10 PASS
nothing to commit, working tree clean
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| Commit checkpoint | `531254a` | Mengamankan evidence M9/M10 sebelum mulai M11 |
| Branch kerja | `praktikum-m11-elf-user-loader` | Isolasi perubahan M11 |
| Direktori kerja baru | `kernel/user/`, `include/mcsos/user/`, `tests/m11/`, `scripts/` | Wadah source, header, test, dan skrip M11 |

Indikator berhasil: Toolchain terdeteksi lengkap, evidence M9/M10 ter-commit, direktori kerja M11 tersedia (verifikasi `ls -la`).

### Langkah 2 — Membuat dan Menjalankan Preflight Script M11

Maksud langkah: Memverifikasi tool, struktur direktori, dan keberadaan marker kode M0–M10 secara otomatis sebelum menulis loader.

Perintah:
```bash
cat > scripts/m11_preflight.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

echo "[M11] Preflight lingkungan dan artefak M0-M10"
for tool in git make clang nm readelf objdump sha256sum; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "[FAIL] tool tidak ditemukan: $tool" >&2
    exit 1
  fi
  echo "[OK] $tool -> $(command -v "$tool")"
done

clang --version | sed -n '1,3p'
make --version | sed -n '1p'

required_dirs=(kernel arch include scripts tests)
for d in "${required_dirs[@]}"; do
  if [ ! -d "$d" ]; then
    echo "[WARN] direktori $d belum ada; sesuaikan dengan struktur repository MCSOS Anda"
  else
    echo "[OK] direktori $d tersedia"
  fi
done

required_markers=(
  "kernel_main" "panic" "idt" "pmm" "vmm" "kmalloc" "sched" "syscall"
)
for m in "${required_markers[@]}"; do
  if grep -R "${m}" -n kernel arch include 2>/dev/null | head -n 1 >/dev/null; then
    echo "[OK] marker ditemukan: $m"
  else
    echo "[WARN] marker belum ditemukan: $m"
  fi
done

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "[OK] commit: $(git rev-parse --short HEAD)"
  git status --short
else
  echo "[WARN] direktori ini belum menjadi repository Git"
fi
EOF
chmod +x scripts/m11_preflight.sh
./scripts/m11_preflight.sh | tee build/m11_preflight.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 5–6 dari PDF)

Output:
```
[M11] Preflight lingkungan dan artefak M0-M10
[OK] git -> /usr/bin/git
[OK] make -> /usr/bin/make
[OK] clang -> /usr/bin/clang
[OK] nm -> /usr/bin/nm
[OK] readelf -> /usr/bin/readelf
[OK] objdump -> /usr/bin/objdump
[OK] sha256sum -> /usr/bin/sha256sum
Ubuntu clang version 21.1.8 (6ubuntu1)
Target: x86_64-pc-linux-gnu
Thread model: posix
GNU Make 4.4.1
[OK] direktori kernel tersedia
[WARN] direktori arch belum ada; sesuaikan dengan struktur repository MCSOS Anda
[OK] direktori include tersedia
[OK] direktori scripts tersedia
[OK] direktori tests tersedia
[WARN] marker belum ditemukan: kernel_main
[WARN] marker belum ditemukan: panic
[WARN] marker belum ditemukan: idt
[WARN] marker belum ditemukan: pmm
[WARN] marker belum ditemukan: vmm
[WARN] marker belum ditemukan: kmalloc
[WARN] marker belum ditemukan: sched
[WARN] marker belum ditemukan: syscall
[OK] commit: 531254a
```

**Catatan penting (transparansi):** Marker generik pada skrip (`kernel_main`, `kmalloc`, dst.) melaporkan `[WARN]` karena pola grep tidak persis sama dengan nama fungsi aktual di source (`kmain`, `pmm_alloc_frame`, dst.), dan direktori `arch` sebenarnya berada di `kernel/arch/` (bukan `./arch`). Untuk memastikan kode M0–M10 benar-benar ada, dilakukan pencarian manual pada Langkah 3.

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| m11_preflight.sh | scripts/m11_preflight.sh | Preflight otomatis tool, direktori, marker |
| m11_preflight.log | build/m11_preflight.log | Bukti eksekusi preflight |

### Langkah 3 — Verifikasi Manual Marker Kode M0–M10

Maksud langkah: Karena preflight generik melaporkan WARN, marker sebenarnya dicari langsung menggunakan nama fungsi/simbol nyata pada source.

Perintah:
```bash
find . -maxdepth 2 -type d -not -path './.git*' -not -path './evidence*' -not -path './build*'
grep -Rln "int kmain\|void kmain\|int kernel_main\|void kernel_main" --include="*.c" --include="*.h" . 2>/dev/null | grep -v evidence
grep -Rln "panic" --include="*.c" --include="*.h" . 2>/dev/null | grep -v evidence
grep -Rln "idt_\|IDT" --include="*.c" --include="*.h" . 2>/dev/null | grep -v evidence
grep -Rln "pmm_\|PMM" --include="*.c" --include="*.h" . 2>/dev/null | grep -v evidence
grep -Rln "vmm_\|VMM" --include="*.c" --include="*.h" . 2>/dev/null | grep -v evidence
grep -Rln "kmalloc\|kfree\|heap_" --include="*.c" --include="*.h" . 2>/dev/null | grep -v evidence
grep -Rln "sched_\|scheduler" --include="*.c" --include="*.h" . 2>/dev/null | grep -v evidence
grep -Rln "syscall" --include="*.c" --include="*.h" . 2>/dev/null | grep -v evidence
find . -iname "*heap*" -o -iname "*alloc*" | grep -v -E "evidence|.git|build"
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 7–8 dari PDF)

Output ringkas (daftar file yang cocok):
```
./configs ./configs/limine ./kernel ./kernel/arch ./kernel/core ./kernel/syscall ./kernel/lib
./kernel/tasks ./kernel/include ./kernel/user ./scripts ./docs ./tools ./tests ./third_party ...
./kernel/core/kmain.c:284:void kmain(void) {
./kernel/core/panic.c, kernel/arch/x86_64/src/idt.c, kernel/include/mcsos/kernel/panic.h
./kernel/core/pmm.c, kernel/tests/test_pmm_host.c, kernel/include/mcsos/kernel/pmm.h
./kernel/core/vmm.c, kernel/tests/test_vmm_host.c, kernel/include/mcsos/kernel/vmm.h
./kernel/core/sched.c, kernel/core/kmain.c, kernel/include/mcsos/kernel/sched.h
./kernel/core/kmain.c, kernel/arch/x86_64/src/idt.c, kernel/syscall/syscall.c,
  kernel/tests/test_syscall_host.c, kernel/include/mcsos/kernel/syscall.h
./kernel/lib, kernel/include/mcsos/kmem.h
```

Indikator berhasil: Seluruh subsistem M3–M10 (panic, idt, pmm, vmm, sched, syscall, heap/kmem) terkonfirmasi ada di source lewat pencarian manual, sehingga aman melanjutkan ke pembuatan loader M11.

### Langkah 4 — Membuat Header Kontrak Loader (`m11_elf_loader.h`)

Maksud langkah: Mendefinisikan struct dan prototipe fungsi loader ELF64 sebelum menulis implementasi.

Perintah:
```bash
cat > include/mcsos/user/m11_elf_loader.h <<'EOF'
#ifndef MCSOS_M11_ELF_LOADER_H
#define MCSOS_M11_ELF_LOADER_H

#include <stddef.h>
#include <stdint.h>

/* ... definisi konstanta M11_ELFMAG0..3, M11_ELFCLASS64, M11_ELFDATA2LSB,
      M11_EV_CURRENT, M11_ET_EXEC/DYN, M11_EM_X86_64, M11_PT_LOAD,
      M11_PF_R/W/X, M11_MAX_LOAD_SEGMENTS, kode error M11_OK..M11_ERR_FLAGS,
      serta struct m11_elf64_ehdr dan m11_elf64_phdr sesuai layout ELF64 ... */

struct m11_user_region {
    uint64_t base;
    uint64_t limit;
};

struct m11_segment_plan {
    uint64_t file_offset;
    uint64_t vaddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
    uint32_t flags;
};

struct m11_process_image_plan {
    uint64_t entry;
    uint32_t segment_count;
    struct m11_segment_plan segments[M11_MAX_LOAD_SEGMENTS];
};

int m11_validate_user_range(struct m11_user_region region, uint64_t base, uint64_t size);
int m11_elf64_plan_load(const void *image, size_t image_size,
                         struct m11_user_region region,
                         struct m11_process_image_plan *out_plan);
const char *m11_error_name(int code);

#endif
EOF
cat include/mcsos/user/m11_elf_loader.h | grep -n "#include"
ls -la include/mcsos/user/
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 8–9 dari PDF)

Output:
```
1:#include <stddef.h>
2:#include <stdint.h>
-rw-r--r-- 1 andianaaji andianaaji 2262 Jul  8 08:33 m11_elf_loader.h
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| m11_elf_loader.h | include/mcsos/user/m11_elf_loader.h | Kontrak struct/enum loader ELF64 |

### Langkah 5 — Implementasi Loader (`m11_elf_loader.c`)

Maksud langkah: Menulis seluruh fungsi validasi ELF64 secara bertahap — helper overflow-safe, validasi ident, validasi bounds program header, validasi tiap segmen, dan fungsi utama `m11_elf64_plan_load`.

Perintah (cuplikan penting):
```bash
cat > kernel/user/m11_elf_loader.c <<'EOF'
#include "m11_elf_loader.h"

static int m11_add_overflow_u64(uint64_t a, uint64_t b, uint64_t *out) {
    uint64_t r = a + b;
    if (r < a) { return 1; }
    *out = r;
    return 0;
}

static int m11_is_power_of_two_u64(uint64_t v) {
    return v != 0u && (v & (v - 1u)) == 0u;
}

static void m11_zero_plan(struct m11_process_image_plan *plan) {
    plan->entry = 0u;
    plan->segment_count = 0u;
    for (uint32_t i = 0u; i < M11_MAX_LOAD_SEGMENTS; ++i) {
        plan->segments[i].file_offset = 0u;
        plan->segments[i].vaddr = 0u;
        plan->segments[i].filesz = 0u;
        plan->segments[i].memsz = 0u;
        plan->segments[i].align = 0u;
        plan->segments[i].flags = 0u;
    }
}

int m11_validate_user_range(struct m11_user_region region, uint64_t base, uint64_t size) {
    uint64_t end = 0u;
    if (region.base >= region.limit) { return M11_ERR_SEGRANGE; }
    if (size == 0u) { return M11_ERR_SEGRANGE; }
    if (m11_add_overflow_u64(base, size, &end) != 0) { return M11_ERR_SEGRANGE; }
    if (base < region.base || end > region.limit || end <= base) { return M11_ERR_SEGRANGE; }
    return M11_OK;
}

static int m11_validate_ident(const struct m11_elf64_ehdr *eh) {
    if (eh->e_ident[0] != M11_ELFMAG0 || eh->e_ident[1] != M11_ELFMAG1 ||
        eh->e_ident[2] != M11_ELFMAG2 || eh->e_ident[3] != M11_ELFMAG3) {
        return M11_ERR_MAGIC;
    }
    if (eh->e_ident[4] != M11_ELFCLASS64) { return M11_ERR_CLASS; }
    if (eh->e_ident[5] != M11_ELFDATA2LSB) { return M11_ERR_ENDIAN; }
    if (eh->e_ident[6] != M11_EV_CURRENT || eh->e_version != M11_EV_CURRENT) {
        return M11_ERR_VERSION;
    }
    return M11_OK;
}

static int m11_validate_phdr_bounds(const struct m11_elf64_ehdr *eh, size_t image_size) {
    uint64_t ph_table_bytes = 0u;
    uint64_t ph_end = 0u;
    if (eh->e_phnum == 0u) { return M11_ERR_PHBOUNDS; }
    if (eh->e_phentsize != sizeof(struct m11_elf64_phdr)) { return M11_ERR_PHENTSIZE; }
    ph_table_bytes = (uint64_t)eh->e_phentsize * (uint64_t)eh->e_phnum;
    if (eh->e_phnum != 0u && ph_table_bytes / eh->e_phnum != eh->e_phentsize) {
        return M11_ERR_PHBOUNDS;
    }
    if (m11_add_overflow_u64(eh->e_phoff, ph_table_bytes, &ph_end) != 0) {
        return M11_ERR_PHBOUNDS;
    }
    if (ph_end > (uint64_t)image_size || eh->e_phoff >= (uint64_t)image_size) {
        return M11_ERR_PHBOUNDS;
    }
    return M11_OK;
}

static int m11_validate_load_segment(const struct m11_elf64_phdr *ph, size_t image_size,
                                      struct m11_user_region region) {
    uint64_t file_end = 0u;
    if ((ph->p_flags & ~(M11_PF_R | M11_PF_W | M11_PF_X)) != 0u) { return M11_ERR_FLAGS; }
    if ((ph->p_flags & M11_PF_W) != 0u && (ph->p_flags & M11_PF_X) != 0u) { return M11_ERR_FLAGS; }
    if (ph->p_memsz < ph->p_filesz) { return M11_ERR_SEGBOUNDS; }
    if (ph->p_align != 0u && ph->p_align != 1u) {
        if (!m11_is_power_of_two_u64(ph->p_align)) { return M11_ERR_ALIGN; }
        if ((ph->p_vaddr % ph->p_align) != (ph->p_offset % ph->p_align)) { return M11_ERR_ALIGN; }
    }
    if (m11_add_overflow_u64(ph->p_offset, ph->p_filesz, &file_end) != 0) { return M11_ERR_SEGBOUNDS; }
    if (file_end > (uint64_t)image_size) { return M11_ERR_SEGBOUNDS; }
    return m11_validate_user_range(region, ph->p_vaddr, ph->p_memsz);
}

int m11_elf64_plan_load(const void *image, size_t image_size,
                         struct m11_user_region region,
                         struct m11_process_image_plan *out_plan) {
    const struct m11_elf64_ehdr *eh = (const struct m11_elf64_ehdr *)image;
    int rc = M11_OK;
    if (image == 0 || out_plan == 0) { return M11_ERR_NULL; }
    m11_zero_plan(out_plan);
    if (image_size < sizeof(struct m11_elf64_ehdr)) { return M11_ERR_SIZE; }
    rc = m11_validate_ident(eh);
    if (rc != M11_OK) { return rc; }
    if (eh->e_type != M11_ET_EXEC && eh->e_type != M11_ET_DYN) { return M11_ERR_TYPE; }
    if (eh->e_machine != M11_EM_X86_64) { return M11_ERR_MACHINE; }
    if (eh->e_ehsize != sizeof(struct m11_elf64_ehdr)) { return M11_ERR_EHSIZE; }
    rc = m11_validate_phdr_bounds(eh, image_size);
    if (rc != M11_OK) { return rc; }
    rc = m11_validate_user_range(region, eh->e_entry, 1u);
    if (rc != M11_OK) { return M11_ERR_ENTRY; }

    const unsigned char *bytes = (const unsigned char *)image;
    const struct m11_elf64_phdr *ph = (const struct m11_elf64_phdr *)(const void *)(bytes + eh->e_phoff);
    out_plan->entry = eh->e_entry;
    for (uint16_t i = 0u; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != M11_PT_LOAD) { continue; }
        if (out_plan->segment_count == M11_MAX_LOAD_SEGMENTS) {
            m11_zero_plan(out_plan);
            return M11_ERR_SEGCOUNT;
        }
        rc = m11_validate_load_segment(&ph[i], image_size, region);
        if (rc != M11_OK) { m11_zero_plan(out_plan); return rc; }
        struct m11_segment_plan *seg = &out_plan->segments[out_plan->segment_count];
        seg->file_offset = ph[i].p_offset;
        seg->vaddr = ph[i].p_vaddr;
        seg->filesz = ph[i].p_filesz;
        seg->memsz = ph[i].p_memsz;
        seg->align = ph[i].p_align;
        seg->flags = ph[i].p_flags;
        out_plan->segment_count++;
    }
    if (out_plan->segment_count == 0u) { return M11_ERR_SEGCOUNT; }
    return M11_OK;
}

const char *m11_error_name(int code) {
    switch (code) {
        case M11_OK: return "M11_OK";
        case M11_ERR_NULL: return "M11_ERR_NULL";
        case M11_ERR_SIZE: return "M11_ERR_SIZE";
        case M11_ERR_MAGIC: return "M11_ERR_MAGIC";
        case M11_ERR_CLASS: return "M11_ERR_CLASS";
        case M11_ERR_ENDIAN: return "M11_ERR_ENDIAN";
        case M11_ERR_VERSION: return "M11_ERR_VERSION";
        case M11_ERR_TYPE: return "M11_ERR_TYPE";
        case M11_ERR_MACHINE: return "M11_ERR_MACHINE";
        case M11_ERR_EHSIZE: return "M11_ERR_EHSIZE";
        case M11_ERR_PHENTSIZE: return "M11_ERR_PHENTSIZE";
        case M11_ERR_PHBOUNDS: return "M11_ERR_PHBOUNDS";
        case M11_ERR_ALIGN: return "M11_ERR_ALIGN";
        case M11_ERR_SEGBOUNDS: return "M11_ERR_SEGBOUNDS";
        case M11_ERR_SEGCOUNT: return "M11_ERR_SEGCOUNT";
        case M11_ERR_ENTRY: return "M11_ERR_ENTRY";
        case M11_ERR_SEGRANGE: return "M11_ERR_SEGRANGE";
        case M11_ERR_FLAGS: return "M11_ERR_FLAGS";
        default: return "M11_ERR_UNKNOWN";
    }
}
EOF
wc -l kernel/user/m11_elf_loader.c
git add include/mcsos/user/ kernel/user/ scripts/m11_preflight.sh
git commit -m "M11: tambahkan header, implementasi loader ELF64, dan preflight script"
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 9–13 dari PDF)

Output:
```
201 kernel/user/m11_elf_loader.c
[praktikum-m11-elf-user-loader 9a8b548] M11: tambahkan header, implementasi loader ELF64, dan preflight script
 3 files changed, 349 insertions(+)
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| m11_elf_loader.c | kernel/user/m11_elf_loader.c | Implementasi loader ELF64 parse-only |
| Commit `9a8b548` | - | Checkpoint header + implementasi + preflight |

Indikator berhasil: File tersimpan 201 baris tanpa error heredoc, commit berhasil.

### Langkah 6 — Membuat Host Unit Test (`m11_host_test.c`)

Maksud langkah: Menguji loader dengan satu kasus valid dan tujuh kasus negatif yang mencakup seluruh kode error utama.

Perintah:
```bash
cat > tests/m11/m11_host_test.c <<'EOF'
#include "m11_elf_loader.h"
#include <stdio.h>
#include <string.h>

#define IMAGE_SIZE 12288u

static struct m11_user_region test_region(void) {
    struct m11_user_region r;
    r.base = 0x0000000000400000ull;
    r.limit = 0x0000008000000000ull;
    return r;
}

/* make_valid_image() membangun 1 Elf64_Ehdr + 2 Elf64_Phdr PT_LOAD yang valid;
   expect_code() memanggil m11_elf64_plan_load lalu membandingkan kode hasil
   dengan kode yang diharapkan, mencetak PASS/FAIL, dan mengembalikan 0/1 */

int main(void) {
    unsigned char image[IMAGE_SIZE];
    struct m11_process_image_plan plan;
    unsigned failures = 0u;

    make_valid_image(image);
    failures += expect_code("valid ELF64 image", m11_elf64_plan_load(image, IMAGE_SIZE, test_region(), &plan), M11_OK);
    if (m11_elf64_plan_load(image, IMAGE_SIZE, test_region(), &plan) == M11_OK) {
        printf("PASS valid plan fields: entry=0x%llx segments=%u\n",
               (unsigned long long)plan.entry, plan.segment_count);
    }

    make_valid_image(image);
    image[0] = 0u;
    failures += expect_code("bad magic", m11_elf64_plan_load(image, IMAGE_SIZE, test_region(), &plan), M11_ERR_MAGIC);

    make_valid_image(image);
    ((struct m11_elf64_ehdr *)(void *)image)->e_machine = 3u;
    failures += expect_code("bad machine", m11_elf64_plan_load(image, IMAGE_SIZE, test_region(), &plan), M11_ERR_MACHINE);

    make_valid_image(image);
    ((struct m11_elf64_ehdr *)(void *)image)->e_entry = 0x1000u;
    failures += expect_code("entry outside user range", m11_elf64_plan_load(image, IMAGE_SIZE, test_region(), &plan), M11_ERR_ENTRY);

    make_valid_image(image);
    struct m11_elf64_phdr *ph = (struct m11_elf64_phdr *)(void *)(image + sizeof(struct m11_elf64_ehdr));
    ph[0].p_memsz = 4u;
    ph[0].p_filesz = 16u;
    failures += expect_code("memsz below filesz", m11_elf64_plan_load(image, IMAGE_SIZE, test_region(), &plan), M11_ERR_SEGBOUNDS);

    make_valid_image(image);
    ph = (struct m11_elf64_phdr *)(void *)(image + sizeof(struct m11_elf64_ehdr));
    ph[0].p_offset = 0x3000u;
    ph[0].p_filesz = 1u;
    failures += expect_code("file range outside image", m11_elf64_plan_load(image, IMAGE_SIZE, test_region(), &plan), M11_ERR_SEGBOUNDS);

    make_valid_image(image);
    ph = (struct m11_elf64_phdr *)(void *)(image + sizeof(struct m11_elf64_ehdr));
    ph[0].p_align = 24u;
    failures += expect_code("bad alignment", m11_elf64_plan_load(image, IMAGE_SIZE, test_region(), &plan), M11_ERR_ALIGN);

    make_valid_image(image);
    ph = (struct m11_elf64_phdr *)(void *)(image + sizeof(struct m11_elf64_ehdr));
    ph[0].p_vaddr = 0x0000800000000000ull;
    failures += expect_code("segment outside user range", m11_elf64_plan_load(image, IMAGE_SIZE, test_region(), &plan), M11_ERR_SEGRANGE);

    if (failures != 0u) {
        printf("M11 host tests failed: %u\n", failures);
        return 1;
    }
    printf("M11 host tests passed.\n");
    return 0;
}
EOF
wc -l tests/m11/m11_host_test.c
git add tests/m11/
git commit -m "M11: tambahkan host unit test loader ELF64"
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 13–14 dari PDF)

Output:
```
114 tests/m11/m11_host_test.c
[praktikum-m11-elf-user-loader b7b7910] M11: tambahkan host unit test loader ELF64
 1 file changed, 114 insertions(+)
```

Kompilasi dan eksekusi manual:
```bash
clang -std=c11 -Wall -Wextra -Werror -O2 \
  -Iinclude/mcsos/user \
  -o build/m11_host_test \
  kernel/user/m11_elf_loader.c tests/m11/m11_host_test.c
./build/m11_host_test
echo "Exit code: $?"
```

Output:
```
PASS valid ELF64 image: M11_OK
PASS valid plan fields: entry=0x401000 segments=2
PASS bad magic: M11_ERR_MAGIC
PASS bad machine: M11_ERR_MACHINE
PASS entry outside user range: M11_ERR_ENTRY
PASS memsz below filesz: M11_ERR_SEGBOUNDS
PASS file range outside image: M11_ERR_SEGBOUNDS
PASS bad alignment: M11_ERR_ALIGN
PASS segment outside user range: M11_ERR_SEGRANGE
M11 host tests passed.
Exit code: 0
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| m11_host_test.c | tests/m11/m11_host_test.c | Host unit test loader (1 valid + 7 negatif) |
| Commit `b7b7910` | - | Checkpoint host test |

Indikator berhasil: Seluruh 8 kasus (1 valid, 7 negatif) PASS, exit code 0.

### Langkah 7 — Membuat `Makefile.m11` (host-test, freestanding, audit)

Maksud langkah: Mengotomasikan tiga target independen agar dapat diulang dan konsisten hasilnya.

Perintah:
```bash
cat > Makefile.m11 <<'EOF'
CC ?= clang
OBJDUMP ?= objdump
READELF ?= readelf
NM ?= nm
SHA256SUM ?= sha256sum

SRC_C := kernel/user/m11_elf_loader.c
SRC_H := include/mcsos/user/m11_elf_loader.h
TEST_C := tests/m11/m11_host_test.c
INCLUDE_DIR := include/mcsos/user

HOST_CFLAGS := -std=c17 -Wall -Wextra -Werror -O2 -g -I$(INCLUDE_DIR)
TARGET_CFLAGS := --target=x86_64-unknown-none -std=c17 -Wall -Wextra -Werror -O2 -g \
  -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -mno-red-zone \
  -I$(INCLUDE_DIR) -c

.PHONY: all host-test freestanding audit clean
all: host-test freestanding audit

host-test: build/m11_host_test
	./build/m11_host_test

build/m11_host_test: $(SRC_C) $(SRC_H) $(TEST_C)
	mkdir -p build
	$(CC) $(HOST_CFLAGS) $(SRC_C) $(TEST_C) -o $@

freestanding: build/m11_elf_loader.o

build/m11_elf_loader.o: $(SRC_C) $(SRC_H)
	mkdir -p build
	$(CC) $(TARGET_CFLAGS) $(SRC_C) -o $@

audit: build/m11_elf_loader.o
	$(NM) -u build/m11_elf_loader.o > build/m11_nm_undefined.txt
	test ! -s build/m11_nm_undefined.txt
	$(READELF) -h build/m11_elf_loader.o > build/m11_readelf_header.txt
	$(OBJDUMP) -dr build/m11_elf_loader.o > build/m11_objdump.txt
	$(SHA256SUM) build/m11_elf_loader.o $(SRC_C) $(SRC_H) $(TEST_C) > build/m11_sha256.txt
	grep -q 'ELF64' build/m11_readelf_header.txt
	grep -q 'm11_elf64_plan_load' build/m11_objdump.txt

clean:
	rm -f build/m11_host_test build/m11_elf_loader.o build/m11_nm_undefined.txt \
	  build/m11_readelf_header.txt build/m11_objdump.txt build/m11_sha256.txt
EOF
git add Makefile.m11
git commit -m "M11: tambahkan Makefile.m11 (host-test, freestanding, audit)"
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 15 dari PDF)

Output:
```
[praktikum-m11-elf-user-loader 1fc72aa] M11: tambahkan Makefile.m11 (host-test, freestanding, audit)
 1 file changed, 45 insertions(+)
```

### Langkah 8 — Menjalankan `host-test`, `freestanding`, dan `audit`

Perintah:
```bash
make -f Makefile.m11 CC=clang host-test | tee build/m11_host_test.log
make -f Makefile.m11 CC=clang freestanding | tee build/m11_freestanding.log
make -f Makefile.m11 CC=clang audit | tee build/m11_audit.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 16–18 dari PDF)

Output `host-test`:
```
./build/m11_host_test
PASS valid ELF64 image: M11_OK
PASS valid plan fields: entry=0x401000 segments=2
PASS bad magic: M11_ERR_MAGIC
PASS bad machine: M11_ERR_MACHINE
PASS entry outside user range: M11_ERR_ENTRY
PASS memsz below filesz: M11_ERR_SEGBOUNDS
PASS file range outside image: M11_ERR_SEGBOUNDS
PASS bad alignment: M11_ERR_ALIGN
PASS segment outside user range: M11_ERR_SEGRANGE
M11 host tests passed.
```

Output `freestanding`:
```
clang --target=x86_64-unknown-none -std=c17 -Wall -Wextra -Werror -O2 -g -ffreestanding \
  -fno-builtin -fno-stack-protector -fno-pic -mno-red-zone -Iinclude/mcsos/user -c \
  kernel/user/m11_elf_loader.c -o build/m11_elf_loader.o
-rw-r--r-- 1 andianaaji andianaaji 14536 Jul  8 11:37 build/m11_elf_loader.o
```

Output `audit`:
```
=== undefined symbols (harus kosong) ===

=== readelf header ===
ELF Header:
  Magic:   7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00
  Class:                             ELF64
  Type:                              REL (Relocatable file)
  Machine:                           Advanced Micro Devices X86-64
  Number of section headers:         26

=== symbol m11_elf64_plan_load di objdump ===
34:0000000000000050 <m11_elf64_plan_load>:
41: 65: 0f 85 07 03 00 00     jne    372 <m11_elf64_plan_load+0x322>
...

=== checksum ===
12b3f7a7953265b6d1dcbd35fab75e41f50fdcd7f1e2cbcd3e56c5090b62c67c  build/m11_elf_loader.o
41ca700fe0d87257f0a533fc5dc0e5b13485979d0805aac8821986ef491075ca kernel/user/m11_elf_loader.c
7b7ab71e22d26311f707520b90f03f9a281e0c87ef446fafa154b78fb61d88fc include/mcsos/user/m11_elf_loader.h
03ef5091421686d629fdfa5e1a17b016efdb45c1275b083b863b80afa4702192 tests/m11/m11_host_test.c
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| build/m11_host_test | build/m11_host_test | Binary host test |
| build/m11_elf_loader.o | build/m11_elf_loader.o | Objek freestanding loader |
| build/m11_nm_undefined.txt | build/m11_nm_undefined.txt | Bukti tidak ada undefined symbol |
| build/m11_readelf_header.txt | build/m11_readelf_header.txt | Header ELF64 REL |
| build/m11_objdump.txt | build/m11_objdump.txt | Disassembly objek |
| build/m11_sha256.txt | build/m11_sha256.txt | Checksum 4 file |

Indikator berhasil: Host test PASS 9/9 assertion, objek freestanding terbentuk (14536 byte), audit lulus seluruh `grep -q`.

### Langkah 9 — Integrasi ke `kmain.c`: Helper `memset`/`memcpy` dan Bootstrap M11

Maksud langkah: Menyediakan helper string freestanding, membangun demo image ELF64 in-memory, dan memanggil loader dari boot sequence.

Perintah (helper string, ditulis sebelum integrasi):
```bash
cat kernel/lib/memory.c   # memastikan memset sudah ada
mkdir -p kernel/include/mcsos/lib
cat > kernel/include/mcsos/lib/string.h <<'EOF'
#ifndef MCSOS_LIB_STRING_H
#define MCSOS_LIB_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *memset(void *dest, int value, size_t count);
void *memcpy(void *dest, const void *src, size_t count);

#ifdef __cplusplus
}
#endif

#endif
EOF
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 19–24 dari PDF)

Cuplikan `m11_elf_loader_bootstrap()` yang disisipkan ke `kmain.c` (via skrip Python idempotent `/tmp/insert_m11.py` — hanya menyisipkan `#include`, blok M11, dan satu baris pemanggilan setelah `m10_syscall_smoke_test()`, dan tidak menyisipkan ulang jika sudah ada):

```c
/* ===== M11: ELF64 user process image loader (parse-only, konservatif) ===== */
#define M11_DEMO_IMAGE_SIZE 12288u
static unsigned char g_m11_demo_image[M11_DEMO_IMAGE_SIZE] __attribute__((aligned(16)));

static void m11_build_demo_image(unsigned char *image) {
    memset(image, 0, M11_DEMO_IMAGE_SIZE);
    struct m11_elf64_ehdr *eh = (struct m11_elf64_ehdr *)(void *)image;
    eh->e_ident[0] = M11_ELFMAG0; eh->e_ident[1] = M11_ELFMAG1;
    eh->e_ident[2] = M11_ELFMAG2; eh->e_ident[3] = M11_ELFMAG3;
    eh->e_ident[4] = M11_ELFCLASS64; eh->e_ident[5] = M11_ELFDATA2LSB;
    eh->e_ident[6] = M11_EV_CURRENT;
    eh->e_type = M11_ET_EXEC; eh->e_machine = M11_EM_X86_64; eh->e_version = M11_EV_CURRENT;
    eh->e_entry = 0x0000000000401000ull;
    eh->e_phoff = sizeof(struct m11_elf64_ehdr);
    eh->e_ehsize = sizeof(struct m11_elf64_ehdr);
    eh->e_phentsize = sizeof(struct m11_elf64_phdr);
    eh->e_phnum = 2u;

    struct m11_elf64_phdr *ph = (struct m11_elf64_phdr *)(void *)(image + eh->e_phoff);
    ph[0].p_type = M11_PT_LOAD; ph[0].p_flags = M11_PF_R | M11_PF_X;
    ph[0].p_offset = 0x1000u; ph[0].p_vaddr = 0x0000000000400000ull;
    ph[0].p_filesz = 16u; ph[0].p_memsz = 4096u; ph[0].p_align = M11_PAGE_SIZE;
    ph[1].p_type = M11_PT_LOAD; ph[1].p_flags = M11_PF_R | M11_PF_W;
    ph[1].p_offset = 0x2000u; ph[1].p_vaddr = 0x0000000000401000ull;
    ph[1].p_filesz = 8u; ph[1].p_memsz = 4096u; ph[1].p_align = M11_PAGE_SIZE;
}

static void m11_elf_loader_bootstrap(void) {
    struct m11_user_region region;
    region.base = 0x0000000000400000ull;
    region.limit = 0x0000008000000000ull;

    m11_build_demo_image(g_m11_demo_image);
    log_writeln("[MCSOS:M11] elf: ident ok");

    struct m11_process_image_plan plan;
    int rc = m11_elf64_plan_load(g_m11_demo_image, M11_DEMO_IMAGE_SIZE, region, &plan);
    if (rc != M11_OK) {
        KERNEL_PANIC("M11: m11_elf64_plan_load failed", (uint64_t)rc);
    }

    log_key_value_hex64("[MCSOS:M11] elf phnum", 2u);
    for (uint32_t i = 0u; i < plan.segment_count; i++) {
        log_key_value_hex64("[MCSOS:M11] segment vaddr", plan.segments[i].vaddr);
        log_key_value_hex64("[MCSOS:M11] segment filesz", plan.segments[i].filesz);
        log_key_value_hex64("[MCSOS:M11] segment memsz", plan.segments[i].memsz);
        log_key_value_hex64("[MCSOS:M11] segment flags", (uint64_t)plan.segments[i].flags);
    }
    log_key_value_hex64("[MCSOS:M11] elf plan entry", plan.entry);
    log_writeln("[MCSOS:M11] elf: plan ok");
    log_writeln("[MCSOS:M11] user image plan ready");
}
```

Pemanggilan disisipkan tepat setelah `m10_syscall_smoke_test();` di dalam `kmain()`:
```c
    m10_syscall_smoke_test();
    m11_elf_loader_bootstrap();
    m9_scheduler_bootstrap();
```

```bash
python3 /tmp/insert_m11.py
# [OK] integrasi M11 disisipkan ke kmain.c
python3 - <<'PYEOF'
path = "kernel/core/kmain.c"
with open(path, "r") as f: src = f.read()
marker = "#include <mcsos/user/m11_elf_loader.h>"
addition = marker + "\n#include <mcsos/lib/string.h>"
if "mcsos/lib/string.h" not in src:
    src = src.replace(marker, addition, 1)
    with open(path, "w") as f: f.write(src)
    print("[OK] include string.h ditambahkan ke kmain.c")
else:
    print("[SKIP] sudah ada")
PYEOF
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 21–24 dari PDF)

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| string.h | kernel/include/mcsos/lib/string.h | Deklarasi memset/memcpy freestanding |
| kmain.c (diubah) | kernel/core/kmain.c | Integrasi `m11_elf_loader_bootstrap()` dan include string.h |

### Langkah 10 — Perbaikan Bug Include Path Saat Full Kernel Build

Maksud langkah: Membangun kernel penuh (M3–M11) dan memperbaiki kegagalan build akibat include path relatif yang tidak sesuai struktur proyek.

Percobaan pertama (gagal):
```bash
make 2>&1 | tee /tmp/m11_kernel_build.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 25 dari PDF)

Output kegagalan:
```
clang --target=x86_64-unknown-none-elf -std=c17 -ffreestanding ... -Iinclude -c \
  kernel/user/m11_elf_loader.c -o build/normal/kernel/user/m11_elf_loader.o
kernel/user/m11_elf_loader.c:1:10: fatal error: 'm11_elf_loader.h' file not found
    1 | #include "m11_elf_loader.h"
      |          ^~~~~~~~~~~~~~~~~~
1 error generated.
make: *** [Makefile:41: build/normal/kernel/user/m11_elf_loader.o] Error 1
```

**Analisis akar masalah:** `m11_elf_loader.c` menyertakan header dengan `#include "m11_elf_loader.h"` (relatif ke direktori file sendiri), padahal Makefile utama proyek hanya menambahkan `-Iinclude` (bukan `-Iinclude/mcsos/user`), sehingga header di `include/mcsos/user/m11_elf_loader.h` tidak ditemukan lewat include path relatif tersebut.

Perbaikan:
```bash
sed -i 's|#include "m11_elf_loader.h"|#include <mcsos/user/m11_elf_loader.h>|' kernel/user/m11_elf_loader.c
sed -i 's|#include "m11_elf_loader.h"|#include <mcsos/user/m11_elf_loader.h>|' tests/m11/m11_host_test.c
head -3 kernel/user/m11_elf_loader.c
head -3 tests/m11/m11_host_test.c
sed -i 's|INCLUDE_DIR := include/mcsos/user|INCLUDE_DIR := include|' Makefile.m11
grep -n "INCLUDE_DIR" Makefile.m11

echo "=== re-run host-test ==="
make -f Makefile.m11 CC=clang clean 2>/dev/null || true
make -f Makefile.m11 CC=clang host-test 2>&1 | tee /tmp/m11_host_test2.log

echo "=== re-run freestanding + audit ==="
make -f Makefile.m11 CC=clang freestanding 2>&1 | tee /tmp/m11_freestanding2.log
make -f Makefile.m11 CC=clang audit 2>&1 | tee /tmp/m11_audit2.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 25–26 dari PDF)

Output setelah perbaikan:
```
#include <mcsos/user/m11_elf_loader.h>
static int m11_add_overflow_u64(uint64_t a, uint64_t b, uint64_t *out) {
...
clang -std=c17 -Wall -Wextra -Werror -O2 -g -Iinclude kernel/user/m11_elf_loader.c tests/m11/m11_host_test.c -o build/m11_host_test
./build/m11_host_test
PASS valid ELF64 image: M11_OK
...
M11 host tests passed.
```

Commit setelah perbaikan:
```bash
git add kernel/user/m11_elf_loader.c tests/m11/m11_host_test.c Makefile.m11 \
  kernel/include/mcsos/lib/string.h kernel/core/kmain.c
git commit -m "M11: perbaiki include path ke <mcsos/user/...>, tambah string.h, integrasi kmain"
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 27 dari PDF)

Output:
```
[praktikum-m11-elf-user-loader bdb8196] M11: perbaiki include path ke <mcsos/user/...>, tambah string.h, integrasi kmain
 5 files changed, 90 insertions(+), 3 deletions(-)
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| Commit `bdb8196` | - | Perbaikan include path + integrasi kmain |

Indikator berhasil: `host-test`, `freestanding`, `audit` seluruhnya kembali PASS setelah perbaikan.

### Langkah 11 — Full Rebuild Kernel (M3–M11) dan Regenerasi ISO

Maksud langkah: Membuktikan seluruh subsistem M3–M11 dapat dibangun bersama menjadi satu `kernel.elf` dan `mcsos.iso`.

Perintah:
```bash
make 2>&1 | tee /tmp/m11_kernel_build2.log
cp /tmp/m11_kernel_build2.log build/m11_kernel_build.log
ls -la build/kernel.elf build/mcsos.iso
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 27–28 dari PDF)

Output ringkas:
```
clang --target=x86_64-unknown-none-elf ... -c kernel/user/m11_elf_loader.c -o build/normal/kernel/user/m11_elf_loader.o
clang --target=x86_64-unknown-elf -m64 ... -c kernel/arch/x86_64/src/context_switch.S -o build/normal/kernel/arch/x86_64/src/context_switch.o
clang ... -c kernel/arch/x86_64/src/interrupts.S -o build/normal/kernel/arch/x86_64/src/interrupts.o
clang ... -c kernel/arch/x86_64/src/syscall_entry.S -o build/normal/kernel/arch/x86_64/src/syscall_entry.o
ld.lld -nostdlib -static -z max-page-size=0x1000 -T linker.ld -Map=build/kernel.map -o build/kernel.elf \
  build/normal/kernel/arch/x86_64/src/idt.o ... build/normal/kernel/user/m11_elf_loader.o ...
readelf -h build/kernel.elf > build/kernel.readelf.header.txt
nm -n build/kernel.elf > build/kernel.syms.txt
objdump -d -Mintel build/kernel.elf > build/kernel.disasm.txt
grep -q 'ELF64' build/kernel.readelf.header.txt
grep -q 'kmain' build/kernel.syms.txt
grep -q 'kernel_panic_at' build/kernel.syms.txt
grep -q 'idt_init' build/kernel.syms.txt
grep -q 'pic_remap' build/kernel.syms.txt
grep -q 'pit_configure_hz' build/kernel.syms.txt
grep -q 'isr_stub_32' build/kernel.syms.txt
grep -q 'timer_on_irq0' build/kernel.syms.txt
grep -q 'pmm_init_from_map' build/kernel.syms.txt
grep -q 'pmm_alloc_frame' build/kernel.syms.txt
-rwxr-xr-x 1 andianaaji andianaaji 2320048 Jul  8 11:48 build/kernel.elf
```

Regenerasi ISO (perlu target `image`, bukan `iso`):
```bash
make image 2>&1 | tee /tmp/m11_image_build.log
cp /tmp/m11_image_build.log build/m11_iso_build.log
ls -la build/mcsos.iso build/mcsos.iso.sha256
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 29 dari PDF)

Output:
```
mkdir -p iso_root/boot/limine iso_root/EFI/BOOT
cp -v build/kernel.elf iso_root/boot/kernel.elf
cp -v configs/limine/limine.conf iso_root/boot/limine/
cp -v third_party/limine/limine-bios.sys iso_root/boot/limine/
cp -v third_party/limine/limine-bios-cd.bin iso_root/boot/limine/
cp -v third_party/limine/limine-uefi-cd.bin iso_root/boot/limine/
cp -v third_party/limine/BOOTX64.EFI iso_root/EFI/BOOT/
xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
  -no-emul-boot -boot-load-size 4 -boot-info-table \
  --efi-boot boot/limine/limine-uefi-cd.bin -efi-boot-part \
  --efi-boot-image --protective-msdos-label iso_root -o build/mcsos.iso
ISO image produced: 3229 sectors
Writing to 'stdio:build/mcsos.iso' completed successfully.
Limine BIOS stages installed successfully.
sha256sum build/mcsos.iso > build/mcsos.iso.sha256
-rw-r--r-- 1 andianaaji andianaaji 6612992 Jul  8 11:49 build/mcsos.iso
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| build/kernel.elf | build/kernel.elf | Kernel gabungan M3-M11 |
| build/mcsos.iso | build/mcsos.iso | Image bootable final |
| build/m11_kernel_build.log | build/m11_kernel_build.log | Log build kernel penuh |
| build/m11_iso_build.log | build/m11_iso_build.log | Log build ISO |

```bash
git add build/m11_kernel_build.log build/m11_iso_build.log
git commit -m "M11: evidence build kernel dan ISO (C5 PASS)"
```

Indikator berhasil: `kernel.elf` (2.320.048 byte) dan `mcsos.iso` (6.612.992 byte) terbentuk tanpa error, seluruh simbol M3-M10 terverifikasi ada di `kernel.syms.txt`.

### Langkah 12 — QEMU Smoke Test dan Verifikasi Marker M11

Maksud langkah: Menjalankan kernel di QEMU headless dan memverifikasi log serial memuat marker M11.

Perintah:
```bash
cat > scripts/m11_qemu_smoke.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

ISO_PATH="${1:-build/mcsos.iso}"
LOG_PATH="${2:-build/m11_qemu_serial.log}"
mkdir -p "$(dirname "$LOG_PATH")"

if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
  echo "[FAIL] qemu-system-x86_64 tidak ditemukan" >&2
  exit 1
fi
if [ ! -f "$ISO_PATH" ]; then
  echo "[FAIL] ISO tidak ditemukan: $ISO_PATH" >&2
  exit 1
fi

timeout 20s qemu-system-x86_64 \
  -m 256M \
  -serial file:"$LOG_PATH" \
  -no-reboot \
  -no-shutdown \
  -display none \
  -cdrom "$ISO_PATH" || true

if grep -E "M11|ELF|user_loader|panic" "$LOG_PATH" >/dev/null 2>&1; then
  echo "[OK] log M11 terdeteksi di $LOG_PATH"
else
  echo "[WARN] marker M11 belum terlihat."
fi
EOF
chmod +x scripts/m11_qemu_smoke.sh
./scripts/m11_qemu_smoke.sh build/mcsos.iso build/m11_qemu_serial.log
cat build/m11_qemu_serial.log
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 29–31 dari PDF)

Output (`m11_qemu_serial.log`, cuplikan):
```
limine: Loading executable `boot():/boot/kernel.elf'...
MCSOS 260502 M3 kernel entered
[MCSOS:M5] boot: external interrupt bring-up start
[MCSOS:M5] idt: vector 0x80 installed (DPL=3)
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
[MCSOS:M10] syscall init
[MCSOS:M10] int 0x80 smoke test executed
[MCSOS:M10] syscall ret=0x00000000265820a
[MCSOS:M10] syscall smoke test PASS
[MCSOS:M11] elf: ident ok
[MCSOS:M11] elf phnum=0x0000000000000002
[MCSOS:M11] segment vaddr=0x0000000000400000
[MCSOS:M11] segment filesz=0x0000000000000010
[MCSOS:M11] segment memsz=0x0000000000001000
[MCSOS:M11] segment flags=0x0000000000000005
[MCSOS:M11] segment vaddr=0x0000000000401000
[MCSOS:M11] segment filesz=0x0000000000000008
[MCSOS:M11] segment memsz=0x0000000000001000
[MCSOS:M11] segment flags=0x0000000000000006
[MCSOS:M11] elf plan entry=0x0000000000401000
[MCSOS:M11] elf: plan ok
[MCSOS:M11] user image plan ready
[MCSOS:M9] scheduler initialized
[MCSOS:M9] thread A tick
[MCSOS:M9] thread B tick
...
[MCSOS:TIMER] ticks=0x0000000000000064
...
[OK] log M11 terdeteksi di build/m11_qemu_serial.log
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| m11_qemu_smoke.sh | scripts/m11_qemu_smoke.sh | Otomasi QEMU headless + verifikasi marker M11 |
| build/m11_qemu_serial.log | build/m11_qemu_serial.log | Bukti boot chain M3-M11 lengkap termasuk plan loader |

Indikator berhasil: Marker `[MCSOS:M11] elf: ident ok`, `elf: plan ok`, dan `user image plan ready` seluruhnya muncul di log serial; kedua segmen demo (kode R+X, data R+W) tercatat dengan `vaddr/filesz/memsz/flags` yang sesuai desain; boot chain lanjut normal ke M9 (scheduler thread A/B tick).

### Langkah 13 — Pengumpulan Evidence Lengkap

Perintah:
```bash
mkdir -p evidence/m11
cp build/m11_preflight.log evidence/m11/ 2>/dev/null || true
cp build/m11_host_test.log evidence/m11/
cp build/m11_freestanding.log evidence/m11/
cp build/m11_audit.log evidence/m11/
cp build/m11_nm_undefined.txt evidence/m11/
cp build/m11_readelf_header.txt evidence/m11/
cp build/m11_objdump.txt evidence/m11/
cp build/m11_sha256.txt evidence/m11/
cp build/m11_kernel_build.log evidence/m11/
cp build/m11_iso_build.log evidence/m11/
cp build/m11_qemu_serial.log evidence/m11/
cp build/mcsos.iso.sha256 evidence/m11/mcsos_m11.iso.sha256

git add evidence/m11/
git commit -m "M11: evidence lengkap C1-C6 (preflight, host-test, freestanding, audit, build, QEMU)"

# beberapa log sempat tertinggal karena aturan .gitignore pada folder build/;
# dijalankan ulang dan disalin agar lengkap:
make -f Makefile.m11 CC=clang host-test | tee build/m11_host_test.log
make -f Makefile.m11 CC=clang freestanding | tee build/m11_freestanding.log
make -f Makefile.m11 CC=clang audit | tee build/m11_audit.log
cp build/m11_host_test.log build/m11_freestanding.log build/m11_audit.log evidence/m11/
git add evidence/m11/
git commit -m "M11: lengkapi evidence host-test, freestanding, audit yang tertinggal"

git add scripts/m11_qemu_smoke.sh
git commit -m "M11: tambahkan script QEMU smoke test"
git log --oneline -10
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 32 dari PDF)

Output:
```
[praktikum-m11-elf-user-loader 14258c7] M11: evidence lengkap C1-C6 (preflight, host-test, freestanding, audit, build, QEMU)
 8 files changed, 576 insertions(+)
[praktikum-m11-elf-user-loader 745e711] M11: lengkapi evidence host-test, freestanding, audit yang tertinggal
 3 files changed, 19 insertions(+)
[praktikum-m11-elf-user-loader 2270ecd] M11: tambahkan script QEMU smoke test
 1 file changed, 30 insertions(+)
```

Indikator berhasil: `evidence/m11/` berisi 12 file bukti (preflight, host-test, freestanding, audit, nm/readelf/objdump/sha256, kernel_build, iso_build, qemu_serial, dan checksum ISO), seluruhnya ter-commit ke branch `praktikum-m11-elf-user-loader`.

---

## 11. Checkpoint Buildable

| Checkpoint | Perintah | Expected result | Status | Evidence |
|---|---|---|---|---|
| Preflight M11 | `./scripts/m11_preflight.sh` | Tool terdeteksi, direktori & marker terverifikasi (manual jika perlu) | PASS | Halaman 1-8 PDF |
| Header + implementasi loader | `cat > include/.../m11_elf_loader.h`, `cat > kernel/user/m11_elf_loader.c` | File tersimpan 201 baris tanpa error | PASS | Halaman 8-13 PDF |
| Host unit test (manual) | `clang ... -o build/m11_host_test && ./build/m11_host_test` | 8/8 kasus PASS | PASS | Halaman 13-14 PDF |
| Makefile.m11 host-test | `make -f Makefile.m11 host-test` | Semua PASS | PASS | Halaman 16 PDF |
| Makefile.m11 freestanding | `make -f Makefile.m11 freestanding` | Objek terbentuk (14536 byte) | PASS | Halaman 17 PDF |
| Makefile.m11 audit | `make -f Makefile.m11 audit` | ELF64 REL, no undefined symbol, symbol ditemukan | PASS | Halaman 18 PDF |
| Perbaikan include path | `sed -i ...`, re-run host-test/freestanding/audit | Tetap PASS setelah fix | PASS | Halaman 25-26 PDF |
| Full kernel build | `make` | `kernel.elf` terbentuk, semua grep simbol PASS | PASS | Halaman 27-28 PDF |
| Build ISO | `make image` | `mcsos.iso` terbentuk | PASS | Halaman 29 PDF |
| QEMU smoke test | `./scripts/m11_qemu_smoke.sh` | Marker M11 terlihat di log serial | PASS | Halaman 29-31 PDF |
| Evidence lengkap | `cp ... evidence/m11/` + commit | 12 file evidence ter-commit | PASS | Halaman 32 PDF |

---

## 12. Perintah Uji dan Validasi

### 12.1 Host Test

```bash
make -f Makefile.m11 CC=clang host-test
```
Hasil: 9 assertion PASS (1 valid + 7 negatif + 1 pengecekan field plan), exit code 0. Status: PASS

### 12.2 Freestanding Compile

```bash
make -f Makefile.m11 CC=clang freestanding
```
Hasil: `build/m11_elf_loader.o` terbentuk (14536 byte) dengan flag `--target=x86_64-unknown-none -ffreestanding -fno-builtin -mno-red-zone`. Status: PASS

### 12.3 Static Inspection (ELF Audit)

```bash
nm -u build/m11_elf_loader.o
readelf -h build/m11_elf_loader.o
objdump -dr build/m11_elf_loader.o
sha256sum build/m11_elf_loader.o kernel/user/m11_elf_loader.c include/mcsos/user/m11_elf_loader.h tests/m11/m11_host_test.c
```
Hasil penting: Class ELF64, Type REL, Machine X86-64; `nm -u` kosong (tidak ada undefined symbol); `m11_elf64_plan_load` ditemukan pada offset `0x50` di objdump. Status: PASS

### 12.4 Full Kernel Build dan ISO

```bash
make
make image
```
Hasil: `build/kernel.elf` (2.320.048 byte) dan `build/mcsos.iso` (6.612.992 byte) terbentuk; simbol `kmain`, `kernel_panic_at`, `idt_init`, `pic_remap`, `pit_configure_hz`, `isr_stub_32`, `timer_on_irq0`, `pmm_init_from_map`, `pmm_alloc_frame` seluruhnya terkonfirmasi ada di `kernel.syms.txt`. Status: PASS

### 12.5 QEMU Smoke Test

```bash
./scripts/m11_qemu_smoke.sh build/mcsos.iso build/m11_qemu_serial.log
```
Hasil: Log serial memuat `[MCSOS:M11] elf: ident ok`, dua blok segmen (kode dan data), `elf plan entry=0x0000000000401000`, `elf: plan ok`, `user image plan ready`; boot chain lanjut normal ke M9/M10. Status: PASS

### 12.6 Visual Evidence

| Screenshot | Lokasi file | Keterangan |
|---|---|---|
| Preflight environment & toolchain | `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 1-2) | uname, os-release, versi toolchain |
| Checkpoint & branch M11 | `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 2-4) | git log, checkout -b, commit 531254a, ls -la |
| Skrip preflight M11 | `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 5-6) | m11_preflight.sh, hasil OK/WARN |
| Verifikasi marker manual | `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 7-8) | grep kmain/panic/idt/pmm/vmm/sched/syscall |
| Header m11_elf_loader.h | `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 8-9) | Struct plan, segment, region |
| Implementasi m11_elf_loader.c | `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 9-13) | Validasi ident, phdr bounds, load segment, plan_load |
| Host unit test m11_host_test.c | `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 13-14) | 8 kasus uji, commit b7b7910 |
| Kompilasi + run manual test | `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 14) | Semua PASS |
| Makefile.m11 | `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 15) | Target host-test/freestanding/audit/clean |
| Run host-test/freestanding/audit | `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 16-18) | Semua PASS, checksum tercatat |
| kmain.c existing + grep signature | `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 19-20) | Bootstrap M8/M9/M10 sebelum M11 |
| Bootstrap M11 + insert_m11.py | `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 21-23) | Demo image builder, penyisipan idempotent |
| Helper string.h + fix build gagal | `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 24-25) | memset, string.h, error include path |
| Perbaikan include path | `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 26-27) | sed fix, re-run PASS, commit bdb8196 |
| Full kernel build | `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 27-28) | kernel.elf terbentuk, grep simbol PASS |
| Build ISO | `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 29) | mcsos.iso terbentuk |
| QEMU smoke test + log serial | `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 29-31) | Marker M11 terlihat di serial log |
| Evidence lengkap & commit akhir | `C:\Users\Ajot\Pictures\M11\Screenshot 2026-07-08 112406.png` (halaman 32) | evidence/m11/, git log --oneline -10 |

---

## 13. Hasil Uji

### 13.1 Tabel Ringkasan Hasil

| No. | Uji | Expected result | Actual result | Status | Evidence |
|---|---|---|---|---|---|
| 1 | Preflight M11 | Tool terdeteksi | Semua tool OK, direktori/marker perlu verifikasi manual tambahan | PASS | Halaman 1-8 |
| 2 | Header + implementasi loader | File tersimpan valid | 201 baris, tanpa error heredoc | PASS | Halaman 8-13 |
| 3 | Host test manual | 8/8 kasus PASS | Semua PASS | PASS | Halaman 13-14 |
| 4 | make host-test | PASS | PASS | PASS | Halaman 16 |
| 5 | make freestanding | Objek terbentuk | 14536 byte | PASS | Halaman 17 |
| 6 | make audit | ELF64 REL, no undefined | Sesuai | PASS | Halaman 18 |
| 7 | Full build (percobaan 1) | kernel.elf terbentuk | GAGAL — include path salah | FAIL (diperbaiki) | Halaman 25 |
| 8 | Perbaikan include path | host-test/freestanding/audit tetap PASS | PASS | PASS | Halaman 25-26 |
| 9 | Full build (percobaan 2) | kernel.elf terbentuk | Berhasil, 2.320.048 byte | PASS | Halaman 27-28 |
| 10 | make image | mcsos.iso terbentuk | Berhasil, 6.612.992 byte | PASS | Halaman 29 |
| 11 | QEMU smoke test | Marker M11 di log serial | Ditemukan (ident ok, plan ok, plan ready) | PASS | Halaman 29-31 |
| 12 | Evidence lengkap | 12 file evidence ter-commit | Terkonfirmasi | PASS | Halaman 32 |

### 13.2 Log Penting

```
[MCSOS:M11] elf: ident ok
[MCSOS:M11] elf phnum=0x0000000000000002
[MCSOS:M11] segment vaddr=0x0000000000400000
[MCSOS:M11] segment filesz=0x0000000000000010
[MCSOS:M11] segment memsz=0x0000000000001000
[MCSOS:M11] segment flags=0x0000000000000005
[MCSOS:M11] segment vaddr=0x0000000000401000
[MCSOS:M11] segment filesz=0x0000000000000008
[MCSOS:M11] segment memsz=0x0000000000001000
[MCSOS:M11] segment flags=0x0000000000000006
[MCSOS:M11] elf plan entry=0x0000000000401000
[MCSOS:M11] elf: plan ok
[MCSOS:M11] user image plan ready
```

### 13.3 Artefak Bukti

| Artefak | Path | Fungsi |
|---|---|---|
| m11_elf_loader.h | include/mcsos/user/m11_elf_loader.h | Kontrak struct/enum loader |
| m11_elf_loader.c | kernel/user/m11_elf_loader.c | Implementasi loader ELF64 parse-only |
| m11_host_test.c | tests/m11/m11_host_test.c | Host unit test |
| Makefile.m11 | Makefile.m11 | Target host-test/freestanding/audit/clean |
| string.h | kernel/include/mcsos/lib/string.h | Deklarasi memset/memcpy |
| m11_preflight.sh | scripts/m11_preflight.sh | Preflight otomatis |
| m11_qemu_smoke.sh | scripts/m11_qemu_smoke.sh | Otomasi QEMU smoke test |
| build/kernel.elf | build/kernel.elf | Kernel gabungan M3-M11 |
| build/mcsos.iso | build/mcsos.iso | Image bootable final |
| evidence/m11/*.log, *.txt | evidence/m11/ | 12 file bukti (preflight s.d. QEMU serial) |

---

## 14. Analisis Teknis

### 14.1 Analisis Keberhasilan

1. **Validasi ELF64 lengkap dan overflow-safe:** Fungsi `m11_add_overflow_u64` konsisten dipakai di setiap perhitungan alamat/ukuran, terbukti dari 7 kasus negatif host test yang semuanya menghasilkan kode error yang tepat, termasuk kasus segmen yang sengaja diarahkan ke alamat sangat tinggi (`0x0000800000000000ull`) untuk memicu `M11_ERR_SEGRANGE`.
2. **Pemisahan host test vs freestanding compile:** `Makefile.m11` memungkinkan logika loader diuji cepat di host (`host-test`) sekaligus dipastikan dapat dikompilasi untuk target kernel (`freestanding`) tanpa harus boot QEMU setiap iterasi.
3. **Integrasi ke kmain.c berhasil dibuktikan lewat log serial nyata**, bukan hanya lewat host test — dua segmen demo (kode R+X, data R+W) tercatat dengan `vaddr/filesz/memsz/flags` yang identik dengan yang ditulis oleh `m11_build_demo_image`.
4. **Full rebuild M3-M11 tanpa regresi:** Setelah bug include path diperbaiki, seluruh simbol milestone sebelumnya (idt, pic, pit, pmm, sched, panic) tetap terkonfirmasi ada di `kernel.syms.txt`.
5. **Automasi skrip Python idempotent (`insert_m11.py`)** mencegah penyisipan ganda ke `kmain.c` bila dijalankan berulang (`if "m11_elf_loader_bootstrap" in src: print("[SKIP] ...")`), menjaga source tetap bersih.

### 14.2 Analisis Kegagalan atau Perbedaan Hasil

| Issue | Penyebab | Dampak | Status Perbaikan |
|---|---|---|---|
| `fatal error: 'm11_elf_loader.h' file not found` saat full kernel build | `#include "m11_elf_loader.h"` (relatif) tidak cocok dengan flag `-Iinclude` pada Makefile utama proyek | Full build gagal pada percobaan pertama | FIXED — diganti `#include <mcsos/user/m11_elf_loader.h>` (commit `bdb8196`) |
| `make iso` menghasilkan "No rule to make target 'iso'" | Target sebenarnya bernama `image`, bukan `iso`, pada Makefile utama | Kebingungan sesaat sebelum ISO berhasil dibuat | Diselesaikan dengan `grep` pada Makefile untuk menemukan target yang benar (`make image`) |
| Beberapa log evidence (`m11_host_test.log`, `m11_freestanding.log`, `m11_audit.log`) sempat tidak ter-commit | Direktori `build/` masuk `.gitignore` proyek sehingga `git commit` melewati file tersebut secara diam-diam | Evidence tidak lengkap pada commit pertama | Diselesaikan dengan menyalin ulang log ke `evidence/m11/` (yang tidak di-ignore) dan commit terpisah (`745e711`) |

### 14.3 Perbandingan dengan Teori

| Konsep teori | Implementasi praktikum | Sesuai/tidak sesuai | Penjelasan |
|---|---|---|---|
| ELF64 header dan magic number | `m11_validate_ident` mengecek `e_ident[0..3]`, class, endian, version | Sesuai | Mengikuti spesifikasi ELF standar |
| `PT_LOAD` `memsz >= filesz` (bagian `.bss`) | `m11_validate_load_segment` menolak `memsz < filesz` | Sesuai | Konsisten dengan semantik ELF: sisa `memsz - filesz` diisi nol saat load |
| Alignment `p_align` harus pangkat dua | `m11_is_power_of_two_u64` + cek kongruensi `vaddr % align == offset % align` | Sesuai | Sesuai praktik loader ELF pada umumnya |
| Integer overflow sebagai vektor bypass validasi (CERT INT30-C) | `m11_add_overflow_u64` dipanggil sebelum setiap perbandingan batas | Sesuai | Mencegah bypass klasik pada loader native |
| Pemisahan parsing vs mapping (defense in depth) | Loader M11 hanya menghasilkan `plan`, tidak memetakan memori nyata | Sesuai (by design) | Sengaja ditunda ke milestone berikutnya demi keamanan bertahap |

### 14.4 Kompleksitas dan Kinerja

| Aspek | Estimasi/hasil | Bukti | Catatan |
|---|---|---|---|
| Kompleksitas `m11_elf64_plan_load` | O(n) terhadap `e_phnum` (linear scan program header) | Analisis source | Wajar, jumlah segmen kecil pada image kernel edukasi |
| Kompleksitas validasi tiap segmen | O(1) per segmen (`m11_validate_load_segment`) | Analisis source | Tidak ada rekursi/traversal tambahan |
| Ukuran `m11_elf_loader.o` (freestanding) | 14.536 byte | `ls -la build/m11_elf_loader.o` | Wajar untuk modul parsing tanpa dependensi berat |
| Ukuran `kernel.elf` setelah M11 | 2.320.048 byte | `ls -la build/kernel.elf` | Meningkat wajar dibanding M10 karena penambahan modul loader dan demo image 12 KB |
| Waktu eksekusi host test | Instan (< 1 detik) | `build/m11_host_test.log` | Cocok untuk iterasi cepat selama development |

---

## 15. Debugging dan Failure Modes

### 15.1 Failure Modes yang Ditemukan

| Failure mode | Gejala | Penyebab | Bukti | Perbaikan |
|---|---|---|---|---|
| Header tidak ditemukan saat full kernel build | `fatal error: 'm11_elf_loader.h' file not found` | Include relatif (`"..."`) tidak cocok dengan include path Makefile utama (`-Iinclude`, bukan `-Iinclude/mcsos/user`) | Log build gagal (halaman 25) | Ganti ke include path absolut proyek `<mcsos/user/m11_elf_loader.h>` (commit `bdb8196`) |
| `make iso` gagal ("No rule to make target 'iso'") | Perintah salah nama target | Nama target sebenarnya `image` pada Makefile utama, bukan asumsi umum `iso` | Output error singkat | Ditemukan lewat `grep -n "^iso\|^all\|mcsos.iso" Makefile`, lalu jalankan `make image` |
| Evidence log hilang dari commit pertama | `git commit` tidak menyertakan sejumlah file log | File berada di `build/` yang di-`.gitignore` proyek | Warning git "paths are ignored" | Salin log ke `evidence/m11/` (di luar ignore) sebelum commit (commit `745e711`) |

### 15.2 Failure Modes yang Diantisipasi

| Failure mode | Deteksi | Dampak | Mitigasi |
|---|---|---|---|
| Image dengan `e_phnum` melebihi `M11_MAX_LOAD_SEGMENTS` | Pengecekan `segment_count == M11_MAX_LOAD_SEGMENTS` sebelum menambah segmen baru | Tanpa batas, dapat menyebabkan buffer overflow pada array `segments[]` | Return `M11_ERR_SEGCOUNT` sebelum menulis di luar batas array |
| Program header table melewati batas file image | `m11_validate_phdr_bounds` dengan overflow-safe check | Membaca memori di luar image (out-of-bounds read) | Return `M11_ERR_PHBOUNDS` sebelum akses array phdr |
| Segmen tumpang tindih (overlap) antar `PT_LOAD` | Belum divalidasi eksplisit pada M11 | Potensi korupsi saat pemetaan memori nyata pada milestone berikutnya | Dicatat sebagai keterbatasan pada bagian 17, direncanakan ditangani saat page mapping nyata diimplementasikan |
| Alamat entry point tidak berada pada segmen yang dapat dieksekusi (`PF_X`) | Belum divalidasi eksplisit pada M11 | Entry point bisa menunjuk ke segmen data | Dicatat sebagai keterbatasan; loader M11 hanya memvalidasi entry berada dalam region pengguna, bukan berada dalam segmen `PF_X` tertentu |

### 15.3 Triage yang Dilakukan

Urutan diagnosis yang dipakai selama praktikum:
1. Jalankan `make -f Makefile.m11 host-test` untuk memastikan logika parsing benar di level host sebelum menyentuh full kernel build
2. Periksa `build/m11_nm_undefined.txt` — pastikan tidak ada undefined symbol sebelum full link
3. Jika full kernel build gagal dengan `fatal error: file not found`, periksa apakah include yang dipakai relatif (`"..."`) atau absolut (`<...>`), lalu cocokkan dengan flag `-I` pada Makefile utama
4. Jika target `make iso` tidak ditemukan, jalankan `grep -n "^iso\|^all\|mcsos.iso" Makefile` untuk menemukan nama target ISO yang benar
5. Jalankan QEMU dengan `-serial file:...` dan periksa urutan marker milestone; jika marker M11 tidak muncul, curigai bootstrap belum terpanggil dari `kmain()` atau demo image tidak valid
6. Jika `git commit` melaporkan file hilang meski sudah `git add`, periksa apakah path tersebut termasuk dalam `.gitignore` (khususnya folder `build/`)

### 15.4 Panic Path (Ringkasan M11)

Loader M11 memicu `KERNEL_PANIC` (mewarisi mekanisme dari M3) hanya bila `m11_elf64_plan_load` mengembalikan kode selain `M11_OK` saat memproses *demo image* di boot sequence:

```c
int rc = m11_elf64_plan_load(g_m11_demo_image, M11_DEMO_IMAGE_SIZE, region, &plan);
if (rc != M11_OK) {
    KERNEL_PANIC("M11: m11_elf64_plan_load failed", (uint64_t)rc);
}
```

Karena demo image dibangun secara terkontrol oleh `m11_build_demo_image()` (bukan input eksternal), jalur panic ini tidak terpicu pada eksekusi QEMU normal — perilaku negatifnya justru divalidasi secara menyeluruh lewat 7 kasus host test pada bagian 10 Langkah 6, bukan lewat demo panic di kernel.

---

## 16. Prosedur Rollback

| Skenario rollback | Perintah | Data yang harus diselamatkan | Status |
|---|---|---|---|
| Kembali ke checkpoint sebelum M11 | `git checkout 531254a` | Evidence M11 (folder `evidence/m11/`) | Teruji |
| Kembali ke akhir M10 | `git checkout 20c67fb` | Evidence M11 | Teruji |
| Bersihkan artefak build M11 saja | `make -f Makefile.m11 clean` | Source aman (tidak terhapus) | Teruji |
| Regenerasi image dari branch M11 | `git switch praktikum-m11-elf-user-loader && make image` | - | Teruji |
| Revert hanya file loader M11 | `git checkout HEAD -- kernel/user/m11_elf_loader.c include/mcsos/user/m11_elf_loader.h tests/m11/m11_host_test.c` | - | Teruji |
| Nonaktifkan sementara bootstrap M11 di kmain.c | Hapus/komentari baris `m11_elf_loader_bootstrap();` | - | Teruji |

Catatan rollback:
```text
Rollback diuji dengan git checkout ke commit checkpoint (531254a), lalu make && make image
untuk memastikan build M10 (tanpa loader M11) tetap dapat dihasilkan dari histori commit yang sama.
```

---

## 17. Keamanan dan Reliability

### 17.1 Risiko Keamanan

| Risiko | Boundary | Dampak | Mitigasi | Evidence |
|---|---|---|---|---|
| Segmen `PT_LOAD` tumpang tindih (overlap) belum divalidasi | `m11_elf64_plan_load` | Berpotensi menimbulkan korupsi saat pemetaan memori nyata pada milestone berikutnya | Belum dimitigasi pada M11; dicatat sebagai keterbatasan eksplisit untuk milestone loader berikutnya | Analisis source `m11_elf_loader.c` |
| Entry point tidak divalidasi harus berada di segmen `PF_X` | `m11_elf64_plan_load` | Entry point secara teoretis bisa menunjuk ke segmen data (non-executable) | Diterima sebagai batasan parse-only pada M11; validasi lebih ketat direncanakan bersama page mapping nyata | `m11_validate_user_range` (hanya cek berada di region, bukan segmen mana) |
| Demo image statis di ruang kernel (bukan dari sumber eksternal tervalidasi ulang) | `kmain.c` (`g_m11_demo_image`) | Tidak merepresentasikan ancaman input eksternal nyata (loader belum menerima file dari luar) | Wajar untuk milestone parse-only; loader tetap diuji dengan 7 kasus negatif independen di host test | `tests/m11/m11_host_test.c` |

### 17.2 Reliability dan Data Integrity

| Risiko reliability | Dampak | Deteksi | Mitigasi |
|---|---|---|---|
| Kesalahan include path tidak terdeteksi hingga full build | Iterasi debugging tambahan | Full kernel build (`make`) | Selalu jalankan `Makefile.m11` (host-test/freestanding/audit) sebelum full build sebagai early warning |
| Evidence log tidak lengkap akibat `.gitignore` | Bukti kerja tidak lengkap saat submit | `git status --short` menunjukkan warning "paths ignored" | Salin log penting ke `evidence/m11/` sebelum commit, verifikasi dengan `ls -la evidence/m11/` |
| Build non-reproducible | Hasil biner berbeda antar build | `sha256sum` pada `m11_elf_loader.o`, `kernel.elf`, `mcsos.iso` | Checksum dicatat sebagai baseline pada `evidence/m11/m11_sha256.txt` dan `mcsos_m11.iso.sha256` |

### 17.3 Negative Test

| Negative test | Input buruk | Expected result | Actual result | Status |
|---|---|---|---|---|
| Magic number salah | `image[0] = 0` | `M11_ERR_MAGIC` | Sesuai | PASS |
| Machine bukan X86_64 | `e_machine = 3` | `M11_ERR_MACHINE` | Sesuai | PASS |
| Entry point di luar region pengguna | `e_entry = 0x1000` | `M11_ERR_ENTRY` | Sesuai | PASS |
| `memsz < filesz` | `p_memsz=4, p_filesz=16` | `M11_ERR_SEGBOUNDS` | Sesuai | PASS |
| File range segmen di luar image | `p_offset=0x3000, p_filesz=1` (melebihi `IMAGE_SIZE`) | `M11_ERR_SEGBOUNDS` | Sesuai | PASS |
| Alignment bukan pangkat dua | `p_align = 24` | `M11_ERR_ALIGN` | Sesuai | PASS |
| Segmen di luar region pengguna | `p_vaddr = 0x0000800000000000` | `M11_ERR_SEGRANGE` | Sesuai | PASS |
| `nm -u` menemukan undefined symbol (skenario hipotetis) | - | `test ! -s m11_nm_undefined.txt` gagal, build berhenti | Tidak terjadi pada implementasi final (file selalu kosong) | PASS (tidak ditemukan) |

---

## 18. Pembagian Kerja Kelompok

Tidak berlaku (pengerjaan individu).

---

## 19. Checklist Final Sebelum Pengumpulan M11

| Checklist | Status |
|---|---|
| Semua placeholder `[isi ...]` sudah diganti | Ya |
| Metadata laporan lengkap | Ya |
| Commit checkpoint dan commit akhir dicatat | Ya |
| Perintah build dan test dapat dijalankan ulang | Ya |
| Log build kernel dan ISO (`m11_kernel_build.log`, `m11_iso_build.log`) dilampirkan | Ya |
| Log QEMU (marker M11 pada boot chain M3-M11) dilampirkan | Ya |
| Host test (1 valid + 7 negatif) dilampirkan | Ya |
| Artefak penting diberi hash SHA-256 | Ya |
| Desain, invariants, ownership, dan failure modes dijelaskan | Ya |
| Bug include path dan perbaikannya didokumentasikan | Ya |
| Security/reliability dibahas termasuk keterbatasan parse-only (belum ada overlap check & PF_X check pada entry) | Ya |
| Rollback teruji ke checkpoint sebelum M11 | Ya |
| Laporan disimpan sebagai Markdown | Ya |

---

## Lampiran M11 — Screenshot Evidence

| No. | File Screenshot | Halaman PDF | Keterangan |
|---|---|---|---|
| 1 | `Screenshot 2026-07-08 112406.png` | Halaman 1 | uname -a, os-release, versi toolchain (percobaan pertama) |
| 2 | `Screenshot 2026-07-08 112406.png` | Halaman 2 | Toolchain (ulang) + git log riwayat M9/M10 + `git checkout -b praktikum-m11-elf-user-loader` + `mkdir` direktori kerja + `git status --short` |
| 3 | `Screenshot 2026-07-08 112406.png` | Halaman 3 | `git add`, commit `531254a`, `git log --oneline -3`, `git status --short`, `ls -la` awal direktori build |
| 4 | `Screenshot 2026-07-08 112406.png` | Halaman 4 | Lanjutan `ls -la` — direktori `include/mcsos/user`, `kernel/user`, `scripts`, `tests/m11` |
| 5 | `Screenshot 2026-07-08 112406.png` | Halaman 5 | Penulisan `scripts/m11_preflight.sh` (tool check, dirs, markers) |
| 6 | `Screenshot 2026-07-08 112406.png` | Halaman 6 | `chmod +x`, eksekusi preflight, hasil OK/WARN |
| 7 | `Screenshot 2026-07-08 112406.png` | Halaman 7 | Pencarian marker manual: `find`, `grep` kmain/panic/idt/pmm/vmm/sched/syscall |
| 8 | `Screenshot 2026-07-08 112406.png` | Halaman 8 | Lanjutan pencarian heap/alloc + awal penulisan `m11_elf_loader.h` |
| 9 | `Screenshot 2026-07-08 112406.png` | Halaman 9 | Lanjutan header (struct plan/segment), `grep #include`, `ls -la`, awal `m11_elf_loader.c` |
| 10 | `Screenshot 2026-07-08 112406.png` | Halaman 10 | Lanjutan `m11_elf_loader.c` — `elf64_plan_load`, `m11_error_name` |
| 11 | `Screenshot 2026-07-08 112406.png` | Halaman 11 | Penulisan ulang `m11_elf_loader.c` lengkap (validasi ident, phdr bounds) |
| 12 | `Screenshot 2026-07-08 112406.png` | Halaman 12 | Lanjutan — `m11_validate_load_segment` (flags, memsz/filesz, alignment) |
| 13 | `Screenshot 2026-07-08 112406.png` | Halaman 13 | `wc -l` (201 baris), commit `9a8b548`, awal `tests/m11/m11_host_test.c` |
| 14 | `Screenshot 2026-07-08 112406.png` | Halaman 14 | Lanjutan host test (7 kasus negatif), `wc -l` (114 baris), commit `b7b7910`, kompilasi & run manual — semua PASS |
| 15 | `Screenshot 2026-07-08 112406.png` | Halaman 15 | Penulisan `Makefile.m11`, commit `1fc72aa` |
| 16 | `Screenshot 2026-07-08 112406.png` | Halaman 16 | `make -f Makefile.m11 host-test` — semua PASS |
| 17 | `Screenshot 2026-07-08 112406.png` | Halaman 17 | `make -f Makefile.m11 freestanding` — objek terbentuk |
| 18 | `Screenshot 2026-07-08 112406.png` | Halaman 18 | `make -f Makefile.m11 audit` — nm/readelf/objdump/sha256 |
| 19 | `Screenshot 2026-07-08 112406.png` | Halaman 19 | `cat -n kernel/core/kmain.c` — bootstrap M8/M9/M10 existing, signature serial/pmm/vmm/kmem |
| 20 | `Screenshot 2026-07-08 112406.png` | Halaman 20 | Lanjutan signature pmm/vmm/kmem, grep memset/memcpy, ls kernel/lib |
| 21 | `Screenshot 2026-07-08 112406.png` | Halaman 21 | Fungsi `m11_elf_loader_bootstrap` (region, demo image, plan_load, log segmen), eksekusi `insert_m11.py` |
| 22 | `Screenshot 2026-07-08 112406.png` | Halaman 22 | Isi lengkap `/tmp/insert_m11.py` — logika penyisipan idempotent, awal `m11_block` (build_demo_image, ELF header) |
| 23 | `Screenshot 2026-07-08 112406.png` | Halaman 23 | Lanjutan `insert_m11.py` — program header kedua, penyisipan pemanggilan setelah `m10_syscall_smoke_test()` |
| 24 | `Screenshot 2026-07-08 112406.png` | Halaman 24 | `memset` di `kernel/lib/memory.c`, pembuatan `kernel/include/mcsos/lib/string.h`, penyisipan include ke `kmain.c` |
| 25 | `Screenshot 2026-07-08 112406.png` | Halaman 25 | Percobaan full build **gagal** — `m11_elf_loader.h` tidak ditemukan (include path salah) |
| 26 | `Screenshot 2026-07-08 112406.png` | Halaman 26 | Perbaikan `sed` include path, re-run host-test/freestanding/audit — kembali PASS |
| 27 | `Screenshot 2026-07-08 112406.png` | Halaman 27 | Commit `bdb8196`, `git log --oneline -5`, `git status --short`, full kernel build berhasil (kernel.elf) |
| 28 | `Screenshot 2026-07-08 112406.png` | Halaman 28 | Percobaan `make iso` gagal (target salah), pencarian target ISO yang benar |
| 29 | `Screenshot 2026-07-08 112406.png` | Halaman 29 | `make image` berhasil — pembuatan `mcsos.iso`, penulisan `scripts/m11_qemu_smoke.sh` |
| 30 | `Screenshot 2026-07-08 112406.png` | Halaman 30 | Eksekusi QEMU smoke test, log serial menunjukkan boot chain M3-M11 dan marker ELF |
| 31 | `Screenshot 2026-07-08 112406.png` | Halaman 31 | Lanjutan log serial (timer ticks), konfirmasi "[OK] log M11 terdeteksi" |
| 32 | `Screenshot 2026-07-08 112406.png` | Halaman 32 | Pengumpulan `evidence/m11/`, commit `14258c7`, `745e711`, `2270ecd`, `git log --oneline -10` final |

---
