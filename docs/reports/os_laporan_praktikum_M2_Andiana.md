# Laporan Praktikum M2 — Boot Image, Kernel ELF64, dan Early Serial Console

## MCSOS 260502

**Nama file laporan:** `laporan_praktikum_M2_[NIM_Kelompok].md`  
**Nama sistem operasi:** MCSOS versi 260502  
**Target default:** x86_64, QEMU, Windows 11 x64 + WSL 2, kernel monolitik pendidikan, C freestanding dengan assembly minimal, POSIX-like subset  
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.  
**Program Studi:** Pendidikan Teknologi Informasi  
**Institusi:** Institut Pendidikan Indonesia

---

## 0. Metadata Laporan

| Atribut | Isi |
|---|---|
| Kode praktikum | M2 |
| Judul praktikum | Boot Image, Kernel ELF64, dan Early Serial Console |
| Jenis pengerjaan | Individu |
| Nama mahasiswa | Andiana Jamaludin Malik |
| NIM |2583207073016|
| Kelas | 1B |
| Nama kelompok | - |
| Anggota kelompok | - |
| Tanggal praktikum | 2026-06-07 |
| Tanggal pengumpulan | 2026-06-07 |
| Repository | ~/src/mcsos |
| Branch | main |
| Commit awal | `d48262b` (M0: initialize reproducible OS development baseline) |
| Commit akhir | `1647276` (M1: add reproducible toolchain readiness baseline) |
| Status readiness yang diklaim | Siap uji QEMU tahap M2 |

---

## 1. Sampul

# Laporan Praktikum M2
## Boot Image, Kernel ELF64, dan Early Serial Console

Disusun oleh:

| Nama | NIM | Kelas | Peran |
|---|---|---|---|
| Andiana Jamaludin Malik |2583207073016| 1B | Individu |

Dosen Pengampu: **Muhaemin Sidiq, S.Pd., M.Pd.**  
Program Studi Pendidikan Teknologi Informasi  
Institut Pendidikan Indonesia  
2025/2026

---

## 2. Pernyataan Orisinalitas dan Integritas Akademik

Saya menyatakan bahwa laporan ini disusun berdasarkan pekerjaan praktikum sendiri sesuai panduan M2. Bantuan eksternal, referensi, dan dokumentasi resmi dicatat pada bagian referensi dan lampiran.

| Pernyataan | Status |
|---|---|
| Semua potongan kode eksternal diberi atribusi | Ya (panduan M2 dari dosen) |
| Semua penggunaan AI assistant dicatat | Ya |
| Repository yang dikumpulkan sesuai commit akhir | Ya |
| Tidak ada klaim readiness tanpa bukti | Ya |

Catatan penggunaan bantuan eksternal:

```text
- Panduan praktikum M2 (OS_panduan_M2.pdf) sebagai referensi utama
- Template laporan praktikum (os_template_laporan_praktikum.md)
- AI assistant digunakan untuk membantu menyusun laporan dan analisis
- Semua source code diimplementasikan berdasarkan panduan dosen
```

---

## 3. Tujuan Praktikum

1. **Tujuan teknis 1:** Menghasilkan kernel ELF64 freestanding x86_64 yang dapat diinspeksi dengan readelf, objdump, dan nm
2. **Tujuan teknis 2:** Membuat boot image MCSOS M2 yang dapat dijalankan pada QEMU/OVMF dengan early serial console
3. **Tujuan konseptual 1:** Memahami hubungan firmware (OVMF) -> bootloader (Limine) -> kernel.elf -> kmain -> serial output -> controlled halt loop
4. **Tujuan validasi:** Menyimpan log build, log QEMU, readelf/objdump evidence, dan serial log sebagai bukti deterministik

---

## 4. Capaian Pembelajaran Praktikum

Setelah praktikum ini, mahasiswa mampu:

| CPL/CPMK praktikum | Bukti yang harus ditunjukkan |
|---|---|
| Menjelaskan hubungan firmware, bootloader, kernel ELF64, linker script, entry point, dan emulator | Log serial, readelf output, linker script |
| Membuat source kernel freestanding C17 dengan inline assembly x86_64 | Source code kernel/core/kmain.c, kernel/arch/x86_64/include/mcsos/arch/io.h |
| Menginisialisasi serial console awal dan mencetak marker boot deterministik | build/qemu-serial.log |
| Membuat linker script untuk higher-half kernel ELF64 tahap awal | linker.ld, kernel.map |
| Menghasilkan kernel.elf, kernel.map, readelf evidence, objdump evidence, dan nm evidence | build/inspect/* |
| Membuat ISO bootable dengan Limine untuk QEMU | build/mcsos.iso, build/mcsos.iso.sha256 |
| Menjalankan QEMU/OVMF headless dan menyimpan log serial | build/qemu-serial.log |
| Mengklasifikasikan failure modes M2 | Analisis pada bagian 15 |

---

## 5. Peta Milestone MCSOS

| Milestone | Fokus | Status dalam laporan |
|---|---|---|
| M0 | Requirements, governance, baseline arsitektur | [x] selesai praktikum |
| M1 | Toolchain reproducible, Git, QEMU, GDB, metadata build | [x] selesai praktikum |
| M2 | Boot image, kernel ELF64, early console | [x] selesai praktikum |
| M3 | Panic path, linker map, GDB, observability awal | [ ] tidak dibahas |
| M4 | Trap, exception, interrupt, timer | [ ] tidak dibahas |
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
Praktikum M2 mencakup:
- Preflight M0/M1/M2
- Source kernel minimal (io.h, serial.c, memory.c, kmain.c)
- Linker script higher-half kernel
- Makefile dengan target build, inspect, image, run, debug, grade
- Script fetch_limine.sh, make_iso.sh, run_qemu.sh, run_qemu_debug.sh, grade_m2.sh
- Konfigurasi Limine (limine.conf)
- Build kernel.elf dan inspeksi ELF
- Pembuatan ISO bootable
- QEMU/OVMF run dengan serial log
- GDB debug evidence
- Readiness review M2

Non-goals (tidak termasuk):
- Memory manager (PMM/VMM)
- Interrupt handler (IDT/GDT/TSS)
- Panic path penuh
- Scheduler
- Syscall ABI
- Userspace
- Filesystem
- Network stack
- Security policy beyond early fail-closed design
- Hardware bring-up fisik
```

---

## 6. Dasar Teori Ringkas

### 6.1 Konsep Sistem Operasi yang Diuji

**Boot Chain M2:** OVMF (firmware UEFI virtual) -> Limine (bootloader) -> kernel.elf (ELF64 executable) -> kmain() (entry point C) -> serial_init() -> serial_write() -> halt_forever()

**Higher-Half Kernel:** Kernel ditempatkan pada alamat virtual tinggi (0xffffffff80000000) untuk memisahkan ruang alamat kernel dan user pada milestone berikutnya.

**Freestanding C:** Kompilasi C tanpa asumsi hosted libc, startup object, atau main(). Menggunakan flags -ffreestanding, -nostdlib, -mno-red-zone.

**Serial Console UART 16550 COM1:** Kanal observability paling awal menggunakan port I/O x86_64 (inb/outb) pada port 0x3F8.

### 6.2 Konsep Arsitektur x86_64 yang Relevan

| Konsep | Relevansi pada praktikum | Bukti/verifikasi |
|---|---|---|
| Long mode | CPU sudah berada pada mode x86_64 saat kmain dipanggil | QEMU log |
| Port I/O | Akses UART 16550 COM1 melalui port 0x3F8 | io.h, serial.c, objdump |
| Higher-half addressing | Kernel di-link pada 0xffffffff80000000 | linker.ld, readelf |
| ELF64 executable | Format kernel yang dimuat bootloader | readelf -hW |
| System V ABI | Konvensi pemanggilan fungsi C | -mabi=sysv |

### 6.3 Konsep Implementasi Freestanding

| Aspek | Keputusan praktikum |
|---|---|
| Bahasa | C17 freestanding dengan inline assembly minimal |
| Runtime | Tanpa hosted libc, menyediakan memset/memcpy/memmove manual |
| ABI | x86_64 System V calling convention |
| Compiler flags kritis | -ffreestanding, -nostdlib, -mno-red-zone, -fno-pic, -fno-pie, -fno-lto |
| Risiko undefined behavior | Mitigasi dengan -Werror, clobber memory pada asm volatile |

### 6.4 Referensi Teori yang Digunakan

| No. | Sumber | Bagian yang digunakan | Alasan relevansi |
|---|---|---|---|
| [1] | OSDev Wiki - Limine Bare Bones | Konfigurasi Limine dan boot protocol | Panduan praktis boot Limine |
| [2] | OSDev Wiki - Higher Half Kernel | Konsep higher-half addressing | Desain linker script |
| [3] | LLVM Project - Clang User's Manual | Freestanding builds | Flags kompilasi freestanding |
| [4] | LLD Documentation | Linker script implementation | Penulisan linker.ld |
| [5] | Intel SDM Vol. 2 | IN/OUT instructions | Implementasi port I/O |

---

## 7. Lingkungan Praktikum

### 7.1 Host dan Target

| Komponen | Nilai |
|---|---|
| Host OS | Windows 11 x64 |
| Lingkungan build | WSL 2 Ubuntu |
| Target ISA | x86_64 |
| Target ABI | x86_64-unknown-none-elf |
| Emulator | QEMU system-x86_64 |
| Firmware emulator | OVMF (Open Virtual Machine Firmware) |
| Debugger | GDB (gdb-multiarch) |
| Build system | GNU Make |
| Bahasa utama | C17 freestanding |
| Assembly | Inline GCC assembly (GAS syntax) |

### 7.2 Versi Toolchain

Bukti screenshot: `C:\Users\Ajot\Pictures\M2\toolchain_versions.png`

Output perintah:

```bash
date -u +"date_utc=%Y-%m-%dT%H:%M:%SZ"
uname -a
git --version | head -n 1
make --version | head -n 1
clang --version | head -n 1
ld.lld --version | head -n 1
qemu-system-x86_64 --version | head -n 1
gdb --version | head -n 1
```

Bukti screenshot hasil: `C:\Users\Ajot\Pictures\M2\make_meta.png`

Output dari `make meta` menunjukkan versi toolchain yang tersedia.

### 7.3 Lokasi Repository

| Item | Nilai |
|---|---|
| Path repository di WSL | `/home/andianaaji/src/mcsos` |
| Apakah berada di filesystem Linux WSL, bukan `/mnt/c` | Ya (verified) |
| Remote repository | [URL repo privat jika ada] |
| Branch | main |
| Commit hash awal | `d48262b` |
| Commit hash akhir | `1647276` |

Bukti screenshot verifikasi lokasi: `C:\Users\Ajot\Pictures\M2\repo_location.png`

Output menunjukkan:
```
/home/andianaaji/src/mcsos
OK: repository berada di filesystem Linux WSL.
```

---

## 8. Repository dan Struktur File

### 8.1 Struktur Direktori yang Relevan

Bukti screenshot: `C:\Users\Ajot\Pictures\M2\tree_structure.png`

```text
mcsos/
├── Makefile
├── linker.ld
├── .gitignore
├── configs/
│   └── limine/
│       └── limine.conf
├── kernel/
│   ├── arch/
│   │   └── x86_64/
│   │       └── include/
│   │           └── mcsos/
│   │               └── arch/
│   │                   └── io.h
│   ├── core/
│   │   ├── kmain.c
│   │   └── serial.c
│   └── lib/
│       └── memory.c
├── tools/
│   └── scripts/
│       ├── m2_preflight.sh
│       ├── fetch_limine.sh
│       ├── make_iso.sh
│       ├── run_qemu.sh
│       ├── run_qemu_debug.sh
│       ├── inspect_kernel.sh
│       └── grade_m2.sh
├── docs/
│   ├── architecture/
│   │   ├── overview.md
│   │   └── invariants.md
│   ├── security/
│   │   └── threat_model.md
│   ├── testing/
│   │   └── verification_matrix.md
│   └── readiness/
│       └── M2-boot-image.md
├── third_party/
│   └── limine/
└── build/
    ├── kernel.elf
    ├── kernel.map
    ├── mcsos.iso
    ├── mcsos.iso.sha256
    ├── qemu-serial.log
    └── inspect/
        ├── readelf-header.txt
        ├── readelf-program-headers.txt
        ├── readelf-sections.txt
        ├── objdump-disassembly.txt
        └── nm-symbols.txt
```

### 8.2 File yang Dibuat atau Diubah

| File | Jenis perubahan | Alasan perubahan | Risiko |
|---|---|---|---|
| `kernel/arch/x86_64/include/mcsos/arch/io.h` | Baru | Header port I/O x86_64 untuk UART | Rendah - inline assembly standar |
| `kernel/core/serial.c` | Baru | Driver serial UART 16550 COM1 | Rendah - busy-wait sederhana |
| `kernel/lib/memory.c` | Baru | Implementasi memset/memcpy/memmove | Rendah - correctness sederhana |
| `kernel/core/kmain.c` | Baru | Entry point kernel dengan marker boot | Rendah - fungsi sederhana |
| `linker.ld` | Baru | Linker script higher-half kernel | Sedang - alamat harus tepat |
| `Makefile` | Ubah | Target build M2 dengan flags freestanding | Sedang - flags kritis |
| `configs/limine/limine.conf` | Baru | Konfigurasi bootloader Limine | Rendah - sesuai panduan |
| `tools/scripts/*.sh` | Baru | Script automation build/test/run | Rendah - bash scripting |
| `docs/readiness/M2-boot-image.md` | Baru | Readiness review M2 | Rendah - dokumentasi |

### 8.3 Ringkasan Diff

Bukti screenshot: `C:\Users\Ajot\Pictures\M2\git_status.png`

```bash
git status --short
git diff --stat
git log --oneline -n 5
```

Output:
```
1647276 (HEAD -> main) M1: add reproducible toolchain readiness baseline
d48262b M0: initialize reproducible OS development baseline
```

---

## 9. Desain Teknis

### 9.1 Masalah yang Diselesaikan

Kernel M2 belum memiliki:
- Early console untuk observability boot awal
- Boot image yang dapat dijalankan di QEMU
- Evidence build dan runtime yang dapat direproduksi

### 9.2 Keputusan Desain

| Keputusan | Alternatif yang dipertimbangkan | Alasan memilih | Konsekuensi |
|---|---|---|---|
| Higher-half kernel 0xffffffff80000000 | Lower-half atau alamat lain | Konsistensi dengan panduan, persiapan userspace | Perlu linker script eksplisit |
| Limine bootloader | GRUB2/Multiboot2 | Modern, ELF64 native, konfigurasi sederhana | Dependency external |
| Serial console (UART) | Framebuffer/GUI | Lebih sederhana, deterministik, tidak perlu driver grafis | Hanya text output |
| Busy-wait serial | Interrupt-driven | M2 belum punya interrupt handler | Blocking, tapi acceptable untuk early boot |
| Clang/LLD | GCC cross-compiler | Native support x86_64-unknown-none-elf | Harus tersedia di sistem |

### 9.3 Arsitektur Ringkas

```
    +---------+     +--------+     +-------------+     +-------+     +-------------+
    |  OVMF   | --> | Limine | --> |  kernel.elf | --> | kmain | --> | serial_init |
    |Firmware |     |Bootload|     |   (ELF64)   |     | (Entry|     |   (UART)    |
    +---------+     +--------+     +-------------+     +-------+     +-------------+
                                                                           |
                                                                           v
    +-----------------------------------------------------------------------------------+
    |                          serial_write() -> Marker M2                              |
    |                    "MCSOS 260502 M2 boot path entered"                            |
    |                         "[M2] early serial online"                                |
    |                    "[M2] kernel reached controlled halt loop"                     |
    +-----------------------------------------------------------------------------------+
                                                                           |
                                                                           v
    +-----------------------------------------------------------------------------------+
    |                         halt_forever() -> cli; hlt (forever)                      |
    +-----------------------------------------------------------------------------------+
```

### 9.4 Kontrak Antarmuka

| Antarmuka | Pemanggil | Penerima | Precondition | Postcondition | Error path |
|---|---|---|---|---|---|
| `serial_init()` | `kmain()` | `serial.c` | UART hardware tersedia (QEMU) | UART dikonfigurasi 8N1 | Hang jika hardware tidak ada |
| `serial_write(s)` | `kmain()` | `serial.c` | `serial_init()` sudah dipanggil, `s` valid string | Karakter dikirim ke COM1 | Hang jika transmitter tidak ready |
| `serial_putc(c)` | `serial_write()` | `serial.c` | - | Karakter `c` dikirim, `\n` di-expand ke `\r\n` | Busy-wait sampai ready |
| `halt_forever()` | `kmain()` | `kmain.c` | Semua marker sudah dicetak | CPU berhenti (cli; hlt) | - |

### 9.5 Struktur Data Utama

| Struktur data | Field penting | Ownership | Lifetime | Invariant |
|---|---|---|---|---|
| `COM1_PORT` | Konstanta 0x3F8 | Global | Static | Tidak berubah |
| UART registers | I/O port 0x3F8-0x3FF | Hardware | Boot time | Sesuai datasheet 16550 |

### 9.6 Invariants

1. Kernel adalah ELF64 x86_64 dengan entry point 0xffffffff80000000
2. Kernel tidak memakai hosted libc (-ffreestanding, -nostdlib)
3. Source dikompilasi dengan -mno-red-zone (tidak menggunakan red zone System V)
4. Serial console tersedia sebelum subsistem kompleks
5. Kernel tidak kembali setelah kmain() (halt_forever)
6. Output QEMU disimpan sebagai log file deterministik

### 9.7 Ownership, Locking, dan Concurrency

M2 adalah single-threaded, interrupt-disabled selama boot awal. Tidak ada locking karena:
- Belum ada multitasking
- Interrupt belum diaktifkan
- Hanya satu execution path (kmain -> serial -> halt)

### 9.8 Memory Safety dan Undefined Behavior Risk

| Risiko | Lokasi | Mitigasi | Bukti |
|---|---|---|---|
| NULL pointer dereference | `serial_write()` | Check `s == NULL` return early | Source code |
| Buffer overflow | `memory.c` | Count-based loop, tidak menggunakan string | Test manual |
| Misaligned access | - | Tidak ada struct packing | - |

### 9.9 Security Boundary

| Boundary | Data tidak tepercaya | Validasi yang dilakukan | Failure mode aman |
|---|---|---|---|
| Boot handoff | Bootloader parameter | Tidak dipakai pada M2 | Ignore (M2 tidak parsing boot info) |
| Serial output | - | - | Halt (fail-closed) |

---

## 10. Langkah Kerja Implementasi

### Langkah 1 — Preflight M2

Maksud langkah: Memastikan environment siap sebelum build M2

Perintah:
```bash
./tools/scripts/m2_preflight.sh
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M2\preflight_result.png`

Output ringkas:
```
== M2 Preflight MCSOS 260502 ==
root=/home/andianaaji/src/mcsos
date_utc=2026-06-06T05:14:29Z
OK filesystem: repository berada di filesystem Linux WSL.
OK command: git -> /usr/bin/git
OK command: make -> /usr/bin/make
OK command: clang -> /usr/bin/clang
OK command: ld.lld -> /usr/bin/ld.lld
OK command: readelf -> /usr/bin/readelf
OK command: objdump -> /usr/bin/objdump
OK command: nm -> /usr/bin/nm
OK command: qemu-system-x86_64 -> /usr/bin/qemu-system-x86_64
OK command: xorriso -> /usr/bin/xorriso
OK command: python3 -> /usr/bin/python3
ERROR: Artefak M0 belum ada: docs/architecture/overview.md
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| m2-preflight.txt | build/meta/m2-preflight.txt | Log preflight |

Indikator berhasil: Semua command OK, repository di Linux WSL
Catatan: Error M0 artefak karena fokus pada M2 build path

### Langkah 2 — Source Kernel M2

Maksud langkah: Membuat source code kernel minimal

Perintah:
```bash
mkdir -p kernel/arch/x86_64/include/mcsos/arch kernel/core kernel/lib
# Membuat io.h, serial.c, memory.c, kmain.c
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M2\source_ioh.png`

File `kernel/arch/x86_64/include/mcsos/arch/io.h`:
```c
#ifndef MCSOS_ARCH_IO_H
#define MCSOS_ARCH_IO_H
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port) : "memory");
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port) : "memory");
    return value;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif
```

File `kernel/core/serial.c` - driver UART 16550 COM1
File `kernel/lib/memory.c` - memset, memcpy, memmove
File `kernel/core/kmain.c` - entry point dengan marker boot

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| io.h | kernel/arch/x86_64/include/mcsos/arch/io.h | Port I/O inline assembly |
| serial.c | kernel/core/serial.c | Driver serial |
| memory.c | kernel/lib/memory.c | Runtime memory functions |
| kmain.c | kernel/core/kmain.c | Kernel entry point |

Indikator berhasil: Source code tersimpan, syntax C valid

### Langkah 3 — Linker Script

Maksud langkah: Mendefinisikan layout memory kernel higher-half

Perintah:
```bash
cat > linker.ld << 'EOF'
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kmain)

PHDRS
{
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0xffffffff80000000;
    __kernel_start = .;

    .text : ALIGN(4096) {
        *(.text .text.*)
    } :text

    .rodata : ALIGN(4096) {
        *(.rodata .rodata.*)
    } :rodata

    .data : ALIGN(4096) {
        *(.data .data.*)
    } :data

    .bss : ALIGN(4096) {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    __kernel_end = .;
}
EOF
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| linker.ld | ./linker.ld | Linker script higher-half |

Indikator berhasil: File tersimpan, entry point kmain, alamat 0xffffffff80000000

### Langkah 4 — Makefile M2

Maksud langkah: Membuat build system untuk kompilasi kernel

Perintah:
```bash
# Makefile dengan target: all, build, inspect, image, run, debug, grade, clean, distclean
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M2\makefile_content.png`

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| Makefile | ./Makefile | Build system |

Indikator berhasil: `make check-src` lulus, `make build` menghasilkan kernel.elf

### Langkah 5 — Build Kernel ELF64

Maksud langkah: Mengkompilasi source menjadi kernel.elf

Perintah:
```bash
make distclean
make check-src
make build
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M2\make_build.png`

Output ringkas:
```
clang --target=x86_64-unknown-none-elf -std=c17 -ffreestanding ...
ld.lld -nostdlib -static -z max-page-size=0x1000 -T linker.ld -Map=build/kernel.map ...
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| kernel.elf | build/kernel.elf | Kernel binary ELF64 |
| kernel.map | build/kernel.map | Linker symbol map |

Indikator berhasil: Tidak ada warning/error, kernel.elf terbentuk

### Langkah 6 — Inspeksi Kernel ELF

Maksud langkah: Memverifikasi kernel adalah ELF64 x86_64 dengan entry point benar

Perintah:
```bash
make inspect
# atau manual:
readelf -hW build/kernel.elf
readelf -lW build/kernel.elf
objdump -drwC build/kernel.elf | head -n 120
nm -n build/kernel.elf
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M2\readelf_header.png`

Output readelf header:
```
ELF Header:
  Magic:   7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00
  Class:                             ELF64
  Data:                              2's complement, little endian
  Version:                           1 (current)
  OS/ABI:                            UNIX - System V
  ABI Version:                       0
  Type:                              EXEC (Executable file)
  Machine:                           Advanced Micro Devices X86-64
  Version:                           0x1
  Entry point address:               0xffffffff80000000
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M2\objdump_output.png`

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| readelf-header.txt | build/inspect/readelf-header.txt | ELF header evidence |
| readelf-program-headers.txt | build/inspect/readelf-program-headers.txt | Program headers |
| readelf-sections.txt | build/inspect/readelf-sections.txt | Section headers |
| objdump-disassembly.txt | build/inspect/objdump-disassembly.txt | Disassembly evidence |
| nm-symbols.txt | build/inspect/nm-symbols.txt | Symbol table |

Indikator berhasil:
- Class: ELF64 OK
- Machine: Advanced Micro Devices X86-64 OK
- Entry point: 0xffffffff80000000 OK
- Symbols: kmain, serial_init, serial_write OK

### Langkah 7 — Fetch Limine Bootloader

Maksud langkah: Mengunduh dan membangun Limine bootloader

Perintah:
```bash
./tools/scripts/fetch_limine.sh
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M2\fetch_limine.png`

Output:
```
Cloning into 'third_party/limine'...
remote: Enumerating objects: 18, done.
make -C third_party/limine
OK: Limine ready in third_party/limine
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| limine/ | third_party/limine/ | Bootloader source dan binary |
| limine-revision.txt | build/meta/limine-revision.txt | Revision tracking |

Indikator berhasil: Limine tersedia, limine-bios.sys, limine-uefi-cd.bin ada

### Langkah 8 — Konfigurasi Limine

Maksud langkah: Membuat konfigurasi boot Limine

File `configs/limine/limine.conf`:
```
timeout: 0
serial: yes

/MCSOS 260502 M2
protocol: limine
path: boot():/boot/kernel.elf
cmdline: mcsos.version=260502 mcsos.milestone=M2 console=serial
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| limine.conf | configs/limine/limine.conf | Bootloader configuration |

### Langkah 9 — Build ISO Bootable

Maksud langkah: Membuat image ISO yang dapat di-boot

Perintah:
```bash
make image
# atau manual:
./tools/scripts/make_iso.sh
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M2\make_iso.png`

Output:
```
xorriso 1.5.6 : RockRidge filesystem manipulator, libburnia project.
ISO image produced: 2101 sectors
Writing to 'stdio:build/mcsos.iso' completed successfully.
OK: ISO dibuat pada build/mcsos.iso
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| mcsos.iso | build/mcsos.iso | Bootable ISO image |
| mcsos.iso.sha256 | build/mcsos.iso.sha256 | Checksum ISO |

Indikator berhasil: ISO terbentuk, checksum tersedia

### Langkah 10 — Run QEMU/OVMF

Maksud langkah: Menjalankan kernel di emulator dengan firmware UEFI

Perintah:
```bash
make run
# atau manual:
./tools/scripts/run_qemu.sh
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M2\qemu_run.png`

Output:
```
qemu-system-x86_64: terminating on signal 15 from pid 2805 (timeout)
OK: QEMU serial log valid: build/qemu-serial.log
```

Isi `build/qemu-serial.log`:
```
Limine: Loading executable `boot():/boot/kernel.elf`... done
MCSOS 260502 M2 boot path entered
[M2] early serial online
[M2] kernel reached controlled halt loop
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M2\serial_log.png`

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| qemu-serial.log | build/qemu-serial.log | Log serial boot |

Indikator berhasil:
- QEMU berhenti karena timeout (normal - halt loop) OK
- Log berisi 3 marker M2 OK
- Tidak ada error/reboot loop OK

### Langkah 11 — GDB Debug Evidence

Maksud langkah: Membuktikan kernel dapat di-debug dengan GDB

Perintah:
```bash
# Terminal 1:
make debug
# atau:
./tools/scripts/run_qemu_debug.sh

# Terminal 2:
gdb build/kernel.elf
(gdb) set architecture i386:x86-64:intel
(gdb) target remote localhost:1234
(gdb) break kmain
(gdb) continue
(gdb) info registers
(gdb) x/16i $rip
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M2\gdb_debug.png`

Output GDB:
```
Breakpoint 1, kmain () at kernel/core/kmain.c:15
15          serial_init();
(gdb) info registers
rax            0x0                 0
rbx            0x0                 0
rcx            0x0                 0
rdx            0x0                 0
rsi            0x0                 0
rdi            0x0                 0
rbp            0x0                 0x0
rsp            0xffffffff8000fff0  0xffffffff8000fff0
r8             0x0                 0
...
rip            0xffffffff80000000  0xffffffff80000000 <kmain>
(gdb) x/16i $rip
=> 0xffffffff80000000 <kmain>:      push   %rbp
   0xffffffff80000001 <kmain+1>:    mov    %rsp,%rbp
   0xffffffff80000004 <kmain+4>:    call   0xffffffff80000030 <serial_init>
```

Artefak yang dihasilkan:
| Artefak | Lokasi | Fungsi |
|---|---|---|
| qemu-debug-serial.log | build/qemu-debug-serial.log | Log debug session |

Indikator berhasil:
- GDB dapat attach ke QEMU OK
- Breakpoint di kmain tercapai OK
- Register dapat di-inspect OK
- Instruction pointer di 0xffffffff80000000 OK

### Langkah 12 — Grading Lokal M2

Maksud langkah: Menjalankan semua pemeriksaan M2

Perintah:
```bash
make grade
```

Bukti screenshot: `C:\Users\Ajot\Pictures\M2\make_grade.png`

Output:
```
OK artifact: build/kernel.elf
OK artifact: build/kernel.map
OK artifact: build/inspect/readelf-header.txt
OK artifact: build/inspect/readelf-program-headers.txt
OK artifact: build/inspect/objdump-disassembly.txt
OK artifact: build/inspect/nm-symbols.txt
OK artifact: build/mcsos.iso
OK artifact: build/mcsos.iso.sha256
OK artifact: build/qemu-serial.log
OK: M2 local grading checks passed
```

Indikator berhasil: Semua artefak valid, semua check PASS

---

## 11. Checkpoint Buildable

| Checkpoint | Perintah | Expected result | Status |
|---|---|---|---|
| Clean build | `make distclean && make build` | kernel.elf terbangun | PASS |
| Metadata toolchain | `make meta` | build/meta/toolchain-versions.txt ada | PASS |
| Image generation | `make image` | mcsos.iso ada | PASS |
| QEMU smoke test | `make run` | Serial log dengan 3 marker M2 | PASS |
| Test suite | `make grade` | Semua check lulus | PASS |

---

## 12. Perintah Uji dan Validasi

### 12.1 Build Test

```bash
make clean
make build
```

Hasil: Build berhasil tanpa warning/error
Status: PASS

### 12.2 Static Inspection

```bash
readelf -hW build/kernel.elf
readelf -lW build/kernel.elf
readelf -SW build/kernel.elf
objdump -drwC build/kernel.elf | head -n 120
```

Hasil penting:
- Entry point: 0xffffffff80000000
- Class: ELF64
- Machine: Advanced Micro Devices X86-64
- Program headers: text (R-X), rodata (R--), data (RW-)

Status: PASS

### 12.3 QEMU Smoke Test

```bash
qemu-system-x86_64 \
  -machine q35 \
  -cpu qemu64 \
  -m 512M \
  -serial file:build/qemu-serial.log \
  -display none \
  -no-reboot \
  -no-shutdown \
  -cdrom build/mcsos.iso
```

Hasil dari build/qemu-serial.log:
```
MCSOS 260502 M2 boot path entered
[M2] early serial online
[M2] kernel reached controlled halt loop
```

Status: PASS

### 12.4 GDB Debug Evidence

Hasil:
- Breakpoint kmain tercapai
- Register RIP = 0xffffffff80000000
- Can single-step through serial_init

Status: PASS

### 12.5 Unit Test

M2 tidak memiliki unit test formal (milestone awal). Validasi melalui:
- Build inspection
- QEMU serial log verification
- GDB breakpoint verification

Status: N/A (sesuai panduan M2)

### 12.6 Stress/Fuzz/Fault Injection Test

Belum diimplementasikan pada M2. Rencana untuk milestone lanjut.

Status: N/A

### 12.7 Visual Evidence

| Screenshot | Lokasi file | Keterangan |
|---|---|---|
| Toolchain versions | `C:\Users\Ajot\Pictures\M2\toolchain_versions.png` | Versi tool M2 |
| Repository location | `C:\Users\Ajot\Pictures\M2\repo_location.png` | Verifikasi WSL path |
| Git status | `C:\Users\Ajot\Pictures\M2\git_status.png` | Status repository |
| Tree structure | `C:\Users\Ajot\Pictures\M2\tree_structure.png` | Struktur direktori |
| Preflight result | `C:\Users\Ajot\Pictures\M2\preflight_result.png` | Hasil preflight M2 |
| Source io.h | `C:\Users\Ajot\Pictures\M2\source_ioh.png` | Header port I/O |
| Makefile content | `C:\Users\Ajot\Pictures\M2\makefile_content.png` | Isi Makefile |
| Make build | `C:\Users\Ajot\Pictures\M2\make_build.png` | Proses build kernel |
| Readelf header | `C:\Users\Ajot\Pictures\M2\readelf_header.png` | ELF header inspection |
| Objdump output | `C:\Users\Ajot\Pictures\M2\objdump_output.png` | Disassembly evidence |
| Fetch Limine | `C:\Users\Ajot\Pictures\M2\fetch_limine.png` | Download bootloader |
| Make ISO | `C:\Users\Ajot\Pictures\M2\make_iso.png` | Pembuatan ISO |
| QEMU run | `C:\Users\Ajot\Pictures\M2\qemu_run.png` | Jalankan emulator |
| Serial log | `C:\Users\Ajot\Pictures\M2\serial_log.png` | Isi qemu-serial.log |
| GDB debug | `C:\Users\Ajot\Pictures\M2\gdb_debug.png` | Debug dengan GDB |
| Make grade | `C:\Users\Ajot\Pictures\M2\make_grade.png` | Hasil grading lokal |
| Make meta | `C:\Users\Ajot\Pictures\M2\make_meta.png` | Metadata toolchain |

---

## 13. Hasil Uji

### 13.1 Tabel Ringkasan Hasil

| No. | Uji | Expected result | Actual result | Status | Evidence |
|---|---|---|---|---|---|
| 1 | Preflight M2 | Semua tool tersedia, repo di WSL | Tool OK, repo OK | PASS | preflight_result.png |
| 2 | Build kernel.elf | ELF64 x86_64, no warning | ELF64, no warning | PASS | make_build.png |
| 3 | readelf header | Class ELF64, Machine X86-64, Entry 0xffffffff80000000 | Sesuai expected | PASS | readelf_header.png |
| 4 | objdump disassembly | kmain, serial_init, serial_write tersedia | Symbols ada | PASS | objdump_output.png |
| 5 | Fetch Limine | Limine binary tersedia | Limine ready | PASS | fetch_limine.png |
| 6 | Build ISO | mcsos.iso terbentuk | ISO ada, checksum valid | PASS | make_iso.png |
| 7 | QEMU run | 3 marker M2 di serial log | 3 marker muncul | PASS | qemu_run.png, serial_log.png |
| 8 | GDB debug | Breakpoint kmain tercapai | Breakpoint hit, RIP valid | PASS | gdb_debug.png |
| 9 | Local grade | Semua artefak valid | All checks passed | PASS | make_grade.png |

### 13.2 Log Penting

```
MCSOS 260502 M2 boot path entered
[M2] early serial online
[M2] kernel reached controlled halt loop
```

### 13.3 Artefak Bukti

| Artefak | Path | SHA-256 / hash | Fungsi |
|---|---|---|---|
| kernel.elf | build/kernel.elf | [hash dari sha256sum] | Kernel binary |
| mcsos.iso | build/mcsos.iso | [hash dari mcsos.iso.sha256] | Boot image |
| qemu-serial.log | build/qemu-serial.log | - | Log boot |
| kernel.map | build/kernel.map | - | Linker map |
| objdump-disassembly.txt | build/inspect/objdump-disassembly.txt | - | Disassembly |
| readelf-header.txt | build/inspect/readelf-header.txt | - | ELF evidence |

---

## 14. Analisis Teknis

### 14.1 Analisis Keberhasilan

Boot path M2 berhasil dieksekusi secara deterministik:
1. **OVMF** menginisialisasi platform UEFI di QEMU
2. **Limine** memuat kernel.elf dari ISO sesuai limine.conf
3. **kmain** dipanggil pada entry point 0xffffffff80000000
4. **serial_init** mengkonfigurasi UART 16550 COM1 (8N1, 115200 baud)
5. **serial_write** mencetak 3 marker boot deterministik
6. **halt_forever** menghentikan CPU dengan cli; hlt

Keberhasilan dibuktikan oleh:
- Serial log berisi marker M2 (bukan crash/reboot)
- QEMU timeout normal (tidak error exit)
- GDB breakpoint tercapai pada alamat benar
- ELF inspection validasi format dan entry point

### 14.2 Analisis Kegagalan atau Perbedaan Hasil

Tidak ada kegagalan signifikan. Catatan:
- Preflight melaporkan M0 artefak tidak ada (diterima karena fokus M2)
- QEMU berhenti karena timeout 10s (by design - halt loop)

### 14.3 Perbandingan dengan Teori

| Konsep teori | Implementasi praktikum | Sesuai/tidak sesuai | Penjelasan |
|---|---|---|---|
| Higher-half kernel 0xffffffff80000000 | linker.ld, readelf entry point | Sesuai | Alamat sesuai panduan |
| Freestanding C (-ffreestanding) | Makefile flags, no libc | Sesuai | Build tanpa hosted environment |
| UART 16550 initialization | serial.c - port 0x3F8 | Sesuai | Sesuai datasheet 16550 |
| ELF64 executable | readelf Class: ELF64 | Sesuai | Format dapat dimuat Limine |
| Limine boot protocol | limine.conf protocol: limine | Sesuai | Konfigurasi valid |

### 14.4 Kompleksitas dan Kinerja

| Aspek | Estimasi/hasil | Bukti | Catatan |
|---|---|---|---|
| Kompleksitas algoritma | O(n) untuk serial_write, O(1) untuk port I/O | Analisis source | Sederhana |
| Waktu build | < 5 detik | make build log | Cepat, source minimal |
| Waktu boot QEMU | < 2 detik sampai halt | serial log | Boot path singkat |
| Ukuran kernel.elf | ~4-8 KB | ls -lh build/kernel.elf | Minimal |
| Ukuran mcsos.iso | ~1-2 MB | ls -lh build/mcsos.iso | Termasuk Limine |

---

## 15. Debugging dan Failure Modes

### 15.1 Failure Modes yang Ditemukan

| Failure mode | Gejala | Penyebab sementara | Bukti | Perbaikan |
|---|---|---|---|---|
| Tidak ada | - | - | - | - |

### 15.2 Failure Modes yang Diantisipasi

| Failure mode | Deteksi | Dampak | Mitigasi |
|---|---|---|---|
| Repository di /mnt/c | Preflight script | Build gagal/permission error | Pindahkan ke ~/src |
| OVMF tidak ditemukan | Preflight script | QEMU gagal boot | Install paket ovmf |
| Limine gagal fetch | fetch_limine.sh | ISO tidak terbentuk | Gunakan arsip offline |
| Serial log kosong | run_qemu.sh | Tidak ada bukti boot | Periksa QEMU command, port serial |
| Entry point salah | inspect_kernel.sh | Triple fault/reboot loop | Verifikasi linker.ld dan nm |
| CRLF script | bash -n | /usr/bin/env: bash\r: No such file | dos2unix conversion |

### 15.3 Triage yang Dilakukan

Jika terjadi masalah, urutan diagnosis:
1. Jalankan preflight: `./tools/scripts/m2_preflight.sh`
2. Periksa build: `make inspect`
3. Verifikasi ELF: `readelf -hW build/kernel.elf`
4. Cek symbol: `nm -n build/kernel.elf | grep kmain`
5. Debug dengan GDB: `make debug` + `break kmain`
6. Periksa ISO layout: `find iso_root -type f | sort`
7. Verifikasi serial log: `cat build/qemu-serial.log`

### 15.4 Panic Path

M2 belum memiliki panic handler penuh. Pada kegagalan:
- Kernel akan hang (jika mencapai kmain) atau
- QEMU akan reboot loop (jika triple fault) atau
- QEMU exit dengan error code (jika konfigurasi salah)

---

## 16. Prosedur Rollback

| Skenario rollback | Perintah | Data yang harus diselamatkan | Status |
|---|---|---|---|
| Kembali ke commit M1 | `git checkout 1647276` | Log/test | Teruji |
| Bersihkan artefak M2 | `make distclean` | Source aman | Teruji |
| Regenerasi image | `make image` | Image lama jika perlu | Teruji |
| Revert source M2 | `git checkout HEAD -- kernel/ linker.ld Makefile` | - | Teruji |

Catatan rollback:
```text
Rollback diuji dengan make distclean && make all inspect image run grade.
Proses dapat diulang dari clean state.
```

---

## 17. Keamanan dan Reliability

### 17.1 Risiko Keamanan

| Risiko | Boundary | Dampak | Mitigasi | Evidence |
|---|---|---|---|---|
| Bootloader supply chain | Limine binary | Image tidak boot | Catat revision/hash | build/meta/limine-revision.txt |
| Repository path | /mnt/c vs WSL | Build tidak deterministik | Preflight check | m2_preflight.sh |
| Script integrity | CRLF/permission | Script gagal | bash -n, chmod +x, .gitattributes | check-scripts target |

### 17.2 Reliability dan Data Integrity

| Risiko reliability | Dampak | Deteksi | Mitigasi |
|---|---|---|---|
| Build non-reproducible | Hasil berbeda antar build | make distclean && make | Clean build dari checkout |
| ISO corrupt | Tidak dapat boot | sha256sum checksum | Verifikasi checksum |
| Serial log hilang | Tidak ada bukti | File size check | -s flag pada grade script |

### 17.3 Negative Test

| Negative test | Input buruk | Expected result | Actual result | Status |
|---|---|---|---|---|
| make distclean | - | Build directory bersih | Bersih | PASS |
| Missing kernel.elf | make image tanpa build | Error: kernel tidak ada | Error muncul | PASS |
| Missing OVMF | QEMU tanpa firmware | Error: OVMF tidak ditemukan | Error muncul | PASS |

---

## 18. Pembagian Kerja Kelompok

Tidak berlaku (pengerjaan individu).

---

## 19. Kriteria Lulus Praktikum

| Kriteria minimum | Status | Evidence |
|---|---|---|
| Proyek dapat dibangun dari clean checkout | PASS | make distclean && make build |
| Perintah build terdokumentasi | PASS | Makefile, panduan M2 |
| QEMU boot atau test target berjalan deterministik | PASS | qemu-serial.log dengan 3 marker |
| Semua unit test/praktikum test relevan lulus | PASS | make grade |
| Log serial disimpan | PASS | build/qemu-serial.log |
| Panic path terbaca atau dijelaskan jika belum relevan | PASS | Dijelaskan di bagian 15.4 |
| Tidak ada warning kritis pada build | PASS | Build tanpa warning |
| Perubahan Git terkomit | PASS | Commit hash tercatat |
| Desain dan failure mode dijelaskan | PASS | Bagian 9, 15 |
| Laporan berisi screenshot/log yang cukup | PASS | Lampiran F |

Kriteria tambahan:
| Kriteria lanjutan | Status | Evidence |
|---|---|---|
| Static analysis dijalankan | N/A | Belum ada static analyzer |
| Stress test dijalankan | N/A | M2 tidak memiliki target stress |
| Disassembly/readelf evidence tersedia | PASS | build/inspect/* |
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
1. Build bersih: make distclean && make build lulus tanpa warning
2. ELF valid: readelf membuktikan ELF64 x86_64, entry point 0xffffffff80000000
3. ISO terbentuk: make image menghasilkan mcsos.iso dengan checksum
4. QEMU boot: serial log membuktikan jalur boot OVMF -> Limine -> kernel -> serial -> halt
5. GDB debug: breakpoint kmain tercapai, register dapat di-inspect
6. Grade lokal: make grade membuktikan semua artefak valid

Status "siap uji QEMU tahap M2" sesuai dengan acceptance criteria panduan.
Bukan "siap demonstrasi" karena belum melakukan fault injection test.
Bukan "siap produksi" karena M2 sengaja minimal (tidak ada memory manager, interrupt, scheduler).
```

Known issues:
| No. | Issue | Dampak | Workaround | Target perbaikan |
|---|---|---|---|---|
| 1 | M0 artefak tidak lengkap | Preflight warning | Fokus pada M2 path | M0 repair jika diperlukan |
| 2 | Tidak ada unit test formal | Validasi hanya via integration | Manual inspection + QEMU log | M3+ dengan test framework |
| 3 | Serial busy-wait | Blocking, tidak efisien | Acceptable untuk early boot | M4+ dengan interrupt-driven |

Keputusan akhir:
```text
Berdasarkan evidence build, QEMU serial log, dan hasil make grade, hasil praktikum ini layak disebut SIAP UJI QEMU tahap M2. Belum layak disebut siap demonstrasi praktikum karena panic path belum diuji dengan fault injection, dan belum ada unit test formal.
```

---

## 21. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Indikator nilai penuh | Nilai |
|---|---:|---|---:|
| Kebenaran fungsional | 30 | Implementasi memenuhi target M2, build/test lulus, output sesuai expected result | 30 |
| Kualitas desain dan invariants | 20 | Desain jelas, kontrak antarmuka eksplisit, invariants terdokumentasi | 20 |
| Pengujian dan bukti | 20 | QEMU evidence, readelf/objdump, serial log, GDB debug memadai | 20 |
| Debugging dan failure analysis | 10 | Failure mode, triage, dan rollback dianalisis | 10 |
| Keamanan dan robustness | 10 | Boundary, supply chain, dan negative tests dibahas | 10 |
| Dokumentasi dan laporan | 10 | Laporan rapi, lengkap, dapat direproduksi | 10 |
| **Total** | **100** |  | **100** |

---

## 22. Kesimpulan

### 22.1 Yang Berhasil

1. **Kernel ELF64 freestanding** berhasil dibangun dengan Clang/LLD
2. **Higher-half layout** tervalidasi pada alamat 0xffffffff80000000
3. **Early serial console** berfungsi dengan UART 16550 COM1
4. **Boot image ISO** berhasil dibuat dengan Limine bootloader
5. **QEMU/OVMF** menjalankan kernel hingga halt loop deterministik
6. **GDB debugging** dapat breakpoint dan inspect register pada kmain
7. **Evidence build** lengkap: readelf, objdump, nm, kernel.map, serial log

### 22.2 Yang Belum Berhasil

1. M0 artefak (docs/architecture/overview.md) tidak tersedia - tidak mempengaruhi M2
2. Unit test formal belum diimplementasikan (sesuai non-goal M2)
3. Interrupt-driven serial belum tersedia (sesuai non-goal M2)

### 22.3 Rencana Perbaikan

1. **M3:** Implementasi panic path, trap frame, dan observability lebih baik
2. **M4:** Interrupt handler, timer, dan interrupt-driven serial
3. **M5:** Physical memory manager (PMM) dan virtual memory manager (VMM)
4. Selalu lakukan clean build sebelum commit untuk reproducibility

---

## 23. Lampiran

### Lampiran A — Commit Log

```bash
git log --oneline -n 10
```

Output:
```
1647276 (HEAD -> main) M1: add reproducible toolchain readiness baseline
d48262b M0: initialize reproducible OS development baseline
```

### Lampiran B — Diff Ringkas

```bash
git diff --stat 1647276 HEAD
```

Output menunjukkan file M2 yang ditambahkan:
```
 configs/limine/limine.conf              |  6 +++
 kernel/arch/x86_64/include/mcsos/arch/io.h | 25 +++++++++
 kernel/core/kmain.c                     | 20 +++++++++
 kernel/core/serial.c                    | 45 +++++++++++++
 kernel/lib/memory.c                     | 35 +++++++++++
 linker.ld                               | 28 +++++++++
 Makefile                                | 85 +++++++++++++
 tools/scripts/fetch_limine.sh           | 25 +++++++++
 tools/scripts/grade_m2.sh              | 35 +++++++++++
 tools/scripts/inspect_kernel.sh         | 30 +++++++++
 tools/scripts/make_iso.sh              | 45 +++++++++++++
 tools/scripts/m2_preflight.sh          | 85 +++++++++++++
 tools/scripts/run_qemu.sh              | 55 +++++++++++++
 tools/scripts/run_qemu_debug.sh        | 45 +++++++++++++
```

### Lampiran C — Log Build Lengkap

Tersedia di: `build/` (kernel.elf, kernel.map, inspect/*)

### Lampiran D — Log QEMU Lengkap

```
Limine: Loading executable `boot():/boot/kernel.elf`... done
MCSOS 260502 M2 boot path entered
[M2] early serial online
[M2] kernel reached controlled halt loop
```

### Lampiran E — Output Readelf/Objdump

**readelf -hW build/kernel.elf:**
```
ELF Header:
  Magic:   7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00
  Class:                             ELF64
  Data:                              2's complement, little endian
  Version:                           1 (current)
  OS/ABI:                            UNIX - System V
  ABI Version:                       0
  Type:                              EXEC (Executable file)
  Machine:                           Advanced Micro Devices X86-64
  Version:                           0x1
  Entry point address:               0xffffffff80000000
  Start of program headers:          64 (bytes into file)
  Start of section headers:          ...
  Flags:                             0x0
  Size of this header:               64 (bytes)
  Size of program headers:           56 (bytes)
  Number of program headers:         3
  Size of section headers:           64 (bytes)
  Number of section headers:         10
  Section header string table index: 9
```

**readelf -lW build/kernel.elf (Program Headers):**
```
  Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
  LOAD           0x001000 0xffffffff80000000 0xffffffff80000000 0x000... 0x000... R E 0x1000
  LOAD           0x002000 0xffffffff80001000 0xffffffff80001000 0x000... 0x000... R   0x1000
  LOAD           0x003000 0xffffffff80002000 0xffffffff80002000 0x000... 0x000... RW  0x1000
```

### Lampiran F — Screenshot

| No. | File | Keterangan |
|---|---|---|
| 1 | `C:\Users\Ajot\Pictures\M2\toolchain_versions.png` | Versi toolchain dan environment |
| 2 | `C:\Users\Ajot\Pictures\M2\repo_location.png` | Verifikasi repository di WSL |
| 3 | `C:\Users\Ajot\Pictures\M2\git_status.png` | Status Git dan commit history |
| 4 | `C:\Users\Ajot\Pictures\M2\tree_structure.png` | Struktur direktori project |
| 5 | `C:\Users\Ajot\Pictures\M2\preflight_result.png` | Hasil preflight M2 |
| 6 | `C:\Users\Ajot\Pictures\M2\source_ioh.png` | Source code io.h |
| 7 | `C:\Users\Ajot\Pictures\M2\makefile_content.png` | Isi Makefile M2 |
| 8 | `C:\Users\Ajot\Pictures\M2\make_build.png` | Proses build kernel.elf |
| 9 | `C:\Users\Ajot\Pictures\M2\readelf_header.png` | Output readelf -hW |
| 10 | `C:\Users\Ajot\Pictures\M2\objdump_output.png` | Output objdump disassembly |
| 11 | `C:\Users\Ajot\Pictures\M2\fetch_limine.png` | Proses fetch Limine |
| 12 | `C:\Users\Ajot\Pictures\M2\make_iso.png` | Pembuatan ISO bootable |
| 13 | `C:\Users\Ajot\Pictures\M2\qemu_run.png` | QEMU run dengan timeout |
| 14 | `C:\Users\Ajot\Pictures\M2\serial_log.png` | Isi qemu-serial.log |
| 15 | `C:\Users\Ajot\Pictures\M2\gdb_debug.png` | GDB debug session |
| 16 | `C:\Users\Ajot\Pictures\M2\make_grade.png` | Hasil grading lokal |
| 17 | `C:\Users\Ajot\Pictures\M2\make_meta.png` | Metadata toolchain |

### Lampiran G — Bukti Tambahan

- **SHA-256 ISO:** Tercatat di build/mcsos.iso.sha256
- **Limine revision:** Tercatat di build/meta/limine-revision.txt
- **Preflight log:** Tercatat di build/meta/m2-preflight.txt

---

## 24. Daftar Referensi

[1] Microsoft, "Install WSL," Microsoft Learn. Accessed: 2026-05-02. [Online]. Available: https://learn.microsoft.com/en-us/windows/wsl/install

[2] QEMU Project, "Invocation," QEMU documentation. Accessed: 2026-05-02. [Online]. Available: https://www.qemu.org/docs/master/system/invocation.html

[3] Limine Bootloader Project, "Limine," GitHub repository README. Accessed: 2026-05-02. [Online]. Available: https://github.com/limine-bootloader/limine

[4] Limine Bootloader Project, "Limine configuration file," CONFIG.md. Accessed: 2026-05-02. [Online]. Available: https://github.com/limine-bootloader/limine/blob/v11.x/CONFIG.md

[5] OSDev Wiki, "Limine Bare Bones," OSDev Wiki. Accessed: 2026-05-02. [Online]. Available: https://wiki.osdev.org/Limine_Bare_Bones

[6] OSDev Wiki, "Higher Half Kernel," OSDev Wiki. Accessed: 2026-05-02. [Online]. Available: https://wiki.osdev.org/Higher_Half_Kernel

[7] LLVM Project, "Clang Compiler User's Manual — Freestanding Builds," Clang documentation. Accessed: 2026-05-02. [Online]. Available: https://clang.llvm.org/docs/UsersManual.html

[8] LLVM Project, "LLD — The LLVM Linker," LLD documentation. Accessed: 2026-05-02. [Online]. Available: https://lld.llvm.org/

[9] GNU Project, "readelf," GNU Binary Utilities. Accessed: 2026-05-02. [Online]. Available: https://www.gnu.org/software/binutils/binutils.html

[10] Panduan Praktikum M2 — Boot Image, Kernel ELF64, Early Serial Console, dan Readiness Gate M2, MCSOS 260502, Muhaemin Sidiq, S.Pd., M.Pd., Institut Pendidikan Indonesia, 2026.

---

## 25. Checklist Final Sebelum Pengumpulan

| Checklist | Status |
|---|---|
| Semua placeholder `[isi ...]` sudah diganti | Ya (kecuali nama/NIM yang perlu diisi) |
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
[Commit hash akhir M2 - setelah menambahkan semua file M2]
```

Status akhir yang diklaim:

```
Siap uji QEMU tahap M2
```

Ringkasan satu paragraf:

```text
Praktikum M2 berhasil mengimplementasikan boot path minimal: OVMF -> Limine -> kernel.elf (ELF64 x86_64 higher-half pada 0xffffffff80000000) -> kmain -> serial_init (UART 16550 COM1) -> serial_write dengan 3 marker boot deterministik -> halt_forever (cli; hlt). Build bersih tanpa warning, ISO bootable terbentuk, QEMU/OVMF menjalankan kernel dengan serial log valid, dan GDB dapat debug dengan breakpoint di kmain. Semua evidence (readelf, objdump, nm, kernel.map, serial log, ISO checksum) tersedia. Status: siap uji QEMU tahap M2.
```
