# QEMU Baseline MCSOS 260502

Target awal MCSOS menggunakan QEMU system emulator untuk x86_64.

## Baseline M0

- M0 hanya memverifikasi keberadaan QEMU dan OVMF.
- M0 belum menjalankan kernel image.
- Jalur UEFI/OVMF akan digunakan pada milestone boot berikutnya.

## Command template untuk M1/M2
Belum wajib berhasil pada M0.

```bash
qemu-system-x86_64 \
  -machine q35 \
  -cpu qemu64 \
  -m 512M \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE.fd \
  -serial file:build/qemu-serial.log \
  -display none \
  -no-reboot \
  -no-shutdown
qemu-system-x86_64 -s -S ...
gdb -ex "target remote localhost:1234"
