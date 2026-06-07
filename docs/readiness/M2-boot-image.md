# Readiness Review M2 — Boot Image dan Early Serial Console

## Identitas

- Proyek: MCSOS 260502
- Praktikum: M2
- Target: x86_64, QEMU, OVMF, Limine
- Nama/Kelompok:
- Commit hash:
- Tanggal:

## Ringkasan Status

Status yang diajukan: siap uji QEMU tahap M2 / belum siap uji QEMU tahap M2.

Alasan ringkas:

## Evidence Matrix

| Evidence | Lokasi | Status | Catatan |
|----------|--------|--------|---------|
| Preflight M2 | `build/meta/m2-preflight.txt` | PASS/FAIL | |
| Kernel ELF | `build/kernel.elf` | PASS/FAIL | |
| Kernel map | `build/kernel.map` | PASS/FAIL | |
| readelf header | `build/inspect/readelf-header.txt` | PASS/FAIL | |
| readelf PHDR | `build/inspect/readelf-program-headers.txt` | PASS/FAIL | |
| objdump | `build/inspect/objdump-disassembly.txt` | PASS/FAIL | |
| ISO | `build/mcsos.iso` | PASS/FAIL | |
| ISO checksum | `build/mcsos.iso.sha256` | PASS/FAIL | |
| Serial log | `build/qemu-serial.log` | PASS/FAIL | |
| Git commit | `build/meta/m2-commit.txt` | PASS/FAIL | |

## Invariants yang Diperiksa

1. Kernel adalah ELF64 x86_64.
2. Entry point sesuai linker script.
3. Kernel tidak memakai hosted libc.
4. Source dikompilasi dengan `-ffreestanding` dan `-mno-red-zone`.
5. Serial console tersedia sebelum subsistem kompleks.
6. Kernel tidak kembali setelah `kmain`.
7. Output QEMU disimpan sebagai log file.

## Failure Modes yang Diuji atau Dianalisis

| Failure mode | Pernah terjadi? | Diagnosis | Perbaikan |
|--------------|-----------------|-----------|-----------|
| Toolchain salah | Ya/Tidak | | |
| OVMF tidak ditemukan | Ya/Tidak | | |
| Limine gagal fetch | Ya/Tidak | | |
| ISO gagal dibuat | Ya/Tidak | | |
| QEMU log kosong | Ya/Tidak | | |
| Entry point salah | Ya/Tidak | | |
| Reboot loop | Ya/Tidak | | |
| CRLF script | Ya/Tidak | | |

## Keputusan Readiness

- [ ] Lulus M2: siap uji QEMU tahap M2.
- [ ] Belum lulus M2: perlu perbaikan.

## Catatan Reviewer

