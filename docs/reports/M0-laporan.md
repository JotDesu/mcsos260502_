# Laporan Praktikum M0 — Baseline Requirements, Governance, dan Lingkungan
Pengembangan
## 1. Sampul
- Judul praktikum: Praktikum M0 — Baseline Requirements, Governance, dan
Lingkungan Pengembangan Reproducible MCSOS 260502
- Nama mahasiswa / kelompok:
- NIM:
- Kelas:
- Dosen: Muhaemin Sidiq, S.Pd., M.Pd.
- Program Studi: Pendidikan Teknologi Informasi, Institut Pendidikan
Indonesia
- Tanggal:
## 2. Tujuan
Tuliskan capaian teknis dan konseptual M0.
## 3. Dasar teori ringkas
Jelaskan host vs target, WSL 2, cross-compilation, ELF object, QEMU, OVMF,
Git, reproducibility, dan evidence-first engineering.
## 4. Lingkungan
| Komponen | Versi / output |
|---|---|
| Windows | |
| WSL distro | |
| Kernel Linux WSL | |
| Git | |
| Clang | |
| LLD | |
| binutils/readelf | |
| NASM | |
| QEMU | |
| GDB | |
| Python | |
Lampirkan isi `build/meta/toolchain-versions.txt`.
## 5. Desain baseline
Jelaskan struktur repository, dokumen baseline, assumptions, non-goals,
dan threat model awal.
## 6. Langkah kerja
Tuliskan perintah yang dijalankan, alasan teknis, dan hasilnya.
## 7. Hasil uji
| Pengujian | Command | Hasil | Pass/Fail |
|---|---|---|---|
| WSL version | `wsl --list --verbose` | | |
| Tool check | `bash tools/check_env.sh` | | |
| Metadata | `cat build/meta/toolchain-versions.txt` | | |
| Smoke object | `make smoke` | | |
| ELF header | `readelf -h build/smoke/freestanding.o` | | |
| Git status | `git status` | | |
## 8. Analisis
Jelaskan kendala, error, penyebab, perbaikan, dan bukti bahwa perbaikan
berhasil.
## 9. Keamanan dan reliability
Jelaskan risiko supply-chain, toolchain mismatch, repository path,
permission, log integrity, dan mitigasi yang diterapkan.
## 10. Failure modes dan rollback
| Failure mode | Gejala | Diagnosis | Rollback/perbaikan |
|---|---|---|---|
| WSL bukan versi 2 | | | |
| Tool tidak ditemukan | | | |
| Repository di `/mnt/c` | | | |
| Smoke object salah target | | | |
| OVMF tidak ditemukan | | | |
## 11. Kesimpulan
Nyatakan apakah M0 hanya siap uji lingkungan, belum siap boot, dan apa
syarat masuk M1.
## 12. Lampiran
- Output `tools/check_env.sh`
- Isi `build/meta/toolchain-versions.txt`
- Output `readelf -h`
- Output `objdump` ringkas
- Screenshot relevan
- Commit hash
## 13. Referensi
Gunakan format IEEE sesuai panduan praktikum.
