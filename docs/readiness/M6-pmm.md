# Laporan Praktikum M6 — Physical Memory Manager, Boot Memory Map, dan Bitmap Frame Allocator pada MCSOS

## 21.1 Sampul

- Judul: Praktikum M6 — Physical Memory Manager, Boot Memory Map, dan Bitmap Frame Allocator pada MCSOS
- Nama mahasiswa: Andiana Jamaludin Malik
- NIM: 2583207073016
- Kelas: 1 B
- Mode pengerjaan: Individu
- Dosen: Muhaemin Sidiq, S.Pd., M.Pd.
- Program Studi Pendidikan Teknologi Informasi
- Institut Pendidikan Indonesia

## 21.2 Tujuan

Mengimplementasikan Physical Memory Manager (PMM) berbasis bitmap frame allocator
4096 byte di atas fondasi M5 (interrupt/timer), mengubah boot memory map dari
bootloader menjadi status frame used/free/reserved, dan membuktikan operasi
alloc/free frame tunggal berjalan tanpa panic maupun triple fault di QEMU.

## 21.3 Dasar Teori Ringkas

Boot memory map dari bootloader (kompatibel Limine) menjadi sumber kebenaran awal
region fisik. PMM mengubah region tersebut menjadi bitmap, satu bit per frame
4096 byte, dengan model fail-closed: seluruh frame dianggap used di awal, hanya
region USABLE yang dibuka menjadi free, frame 0 selalu direserve, dan region
non-usable ditandai used kembali setelah region usable diproses agar overlap
akibat firmware tetap fail-closed. PMM v0 ini belum menyediakan VMM baru maupun
heap umum; keduanya menjadi scope M7/M8.

## 21.4 Lingkungan

| Komponen | Versi / Nilai |
|---|---|
| Windows | Windows 11 x64 |
| WSL distro | Ubuntu/Debian-like (WSL2) |
| Compiler | clang (target x86_64-unknown-none-elf, C17 freestanding) |
| Linker | ld.lld |
| QEMU | qemu-system-x86_64 (machine q35, cpu qemu64, OVMF) |
| Target | x86_64 |
| Commit hash | 2e5b737 |

## 21.5 Desain

Empat lapisan sesuai Bagian 2B panduan:
- Boot memory input: `struct boot_mem_region` (base, length, type)
- PMM core: `kernel/include/mcsos/kernel/pmm.h`, `kernel/core/pmm.c`
- Host test: `kernel/tests/test_pmm_host.c`
- Kernel integration: `kernel/core/kmain.c` (`kernel_memory_init()`, dipanggil
  setelah `sti` M5 siap)

API yang diimplementasikan: `pmm_init_from_map`, `pmm_alloc_frame`,
`pmm_free_frame`, `pmm_reserve_range`, `pmm_is_frame_free`, `pmm_free_count`,
`pmm_used_count`, `pmm_frame_count`. Invariant utama: frame 0 selalu reserved,
region non-usable overwrite region usable, alokasi gagal mengembalikan
`PMM_INVALID_FRAME`.

Catatan proses: selama pengerjaan M6, ditemukan draft kode M7 (VMM, page fault
handler) yang sempat tercampur di working tree yang sama (`kernel/core/kmain.c`,
`kernel/arch/x86_64/src/idt.c`, `Makefile` target `check-m7`). Kode tersebut
dipisahkan ke branch `m7-vmm-wip` agar tidak mengganggu verifikasi M6, sesuai
Non-Goals M6 (Bagian 2A: "Tidak membuat virtual memory manager penuh").

## 21.6 Langkah Kerja

1. Implementasi `include/pmm.h` dan `src/pmm.c` sesuai kontrak API M6.
2. Penulisan host unit test `tests/test_pmm_host.c`.
3. Verifikasi target `check-m6` pada Makefile (kompilasi freestanding,
   host unit test, freestanding audit `nm -u`, objdump).
4. Pembuatan `scripts/check_m6_static.sh` yang membungkus `make check-m6`
   dan menyalin bukti ke `evidence/M6/`.
5. Perbaikan `tools/scripts/run_qemu.sh`: marker verifikasi lama (M2) sudah
   usang, diganti dengan marker M3 dan M6 (`[MCSOS:M6] pmm initialized`,
   `[MCSOS:M6] sample alloc/free OK`), serta `grep -q` diganti `grep -qF`
   karena pola mengandung karakter `[` `]` yang dibaca sebagai character
   class oleh regex biasa.
6. Perbaikan `.gitignore`: aturan `*.log` sebelumnya memblokir seluruh
   evidence log (termasuk M3/M4 yang sudah lama ada tapi tidak pernah
   ter-commit); ditambahkan pengecualian `!evidence/**/*.log`.
7. Pemisahan kode M7 yang tercampur (`vmm.c`, `vmm.h`, `test_vmm_host.c`,
   perubahan `idt.c`/`kmain.c`/`Makefile` terkait VMM) ke branch terpisah
   `m7-vmm-wip` agar branch M6 murni.
8. Pembersihan file kosong nyasar di root repo (`!`, `cp`, `grep`, `mkdir`)
   dan `kernel/core/kmain.c.bak`.

## 21.7 Hasil Uji

| Uji | Perintah | Hasil | Bukti |
|---|---|---|---|
| Host PMM test | `./scripts/check_m6_static.sh` | PASS | `evidence/M6/pmm.objdump.txt` |
| Freestanding audit | `nm -u build/pmm.o` | PASS (kosong) | `evidence/M6/pmm.undefined.txt` |
| Kernel build | `make check-m6` | PASS | log terminal |
| QEMU smoke | `./tools/scripts/run_qemu.sh` | PASS | `evidence/M6/m6_qemu_serial.log` |

Cuplikan serial log QEMU (lengkap di `evidence/M6/m6_qemu_serial.log`):
[MCSOS:M6] pmm initialized
[MCSOS:M6] frames managed=0x0000000000008000
[MCSOS:M6] frames free=0x0000000000007fff
[MCSOS:M6] frames used=0x0000000000000001
[MCSOS:M6] sample frame=0x0000000000001000
[MCSOS:M6] sample alloc/free OK
## 21.8 Analisis

PMM berhasil mengelola 0x8000 (32768) frame atau 128 MiB memori dengan hanya
frame 0 yang used pada kondisi awal (`frames used=0x1`), sesuai kontrak fail-closed
dan reservasi frame 0. Alokasi dan pelepasan satu frame sample berjalan tanpa
panic maupun page fault, dibuktikan baik lewat host unit test maupun boot QEMU
sungguhan. Bug proses (bukan bug logika PMM) yang ditemukan dan diperbaiki:
tooling `run_qemu.sh` yang usang (marker M2), `.gitignore` yang tidak sengaja
memblokir bukti evidence, dan pencampuran kode M7 ke working tree M6 sebelum
readiness M6 ditutup.

## 21.9 Keamanan dan Reliability

- Reserved memory corruption: dimitigasi dengan model fail-closed (semua frame
  used di awal, non-usable overwrite usable setelah region usable dibuka).
- Invalid free / double free: ditangani oleh `pmm_free_frame` (menolak frame
  yang sudah free/reserved).
- Overflow `base + length`: ditangani lewat `checked_add_u64` sesuai kontrak
  Bagian 4.
- Page fault saat init: tidak terjadi pada boot QEMU smoke test M6 (dibuktikan
  log bersih tanpa panic). Perlu dicatat: kode M7 draft (di branch `m7-vmm-wip`)
  memiliki page fault yang **disengaja** untuk demo, dan tidak diintegrasikan
  ke jalur boot M6.

## 21.10 Kesimpulan

M6 berhasil: PMM bitmap allocator berfungsi, teruji lewat host unit test dan
QEMU smoke test, freestanding audit bersih. Belum ada VMM, belum ada heap umum
(sesuai scope M6). Rencana M7: integrasi VMM (draft sudah ada di branch
`m7-vmm-wip`) — perlu review ulang sebelum merge karena masih mengandung kode
demo page fault yang harus dihapus/dikondisikan.

## 21.11 Lampiran

- Source: `kernel/include/mcsos/kernel/pmm.h`, `kernel/core/pmm.c`
- Test: `kernel/tests/test_pmm_host.c`
- Script: `scripts/check_m6_static.sh`
- Evidence: `evidence/M6/pmm.objdump.txt`, `evidence/M6/pmm.undefined.txt`,
  `evidence/M6/m6_qemu_serial.log`
- Referensi: lihat Bagian 23 panduan M6 (Limine memory map docs, Intel SDM,
  QEMU GDB usage documentation).
