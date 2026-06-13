#!/usr/bin/env bash
set -Eeuo pipefail

ISO="${1:-build/mcsos.iso}"
LOG="build/qemu-debug-serial.log"

OVMF_CODE="${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE_4M.fd}"
OVMF_VARS_TEMPLATE="${OVMF_VARS:-/usr/share/OVMF/OVMF_VARS_4M.fd}"
OVMF_VARS="build/OVMF_VARS.fd"

test -f "$ISO" || { echo "FAIL: ISO tidak ditemukan: $ISO" >&2; exit 1; }
test -f "$OVMF_CODE" || { echo "FAIL: OVMF_CODE tidak ditemukan: $OVMF_CODE" >&2; exit 1; }
test -f "$OVMF_VARS_TEMPLATE" || { echo "FAIL: OVMF_VARS tidak ditemukan: $OVMF_VARS_TEMPLATE" >&2; exit 1; }

rm -f "$LOG"
cp "$OVMF_VARS_TEMPLATE" "$OVMF_VARS"

exec qemu-system-x86_64 \
    -machine q35 \
    -m 256M \
    -smp 1 \
    -cpu qemu64 \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file="$OVMF_VARS" \
    -cdrom "$ISO" \
    -boot d \
    -serial file:"$LOG" \
    -display none \
    -no-reboot \
    -no-shutdown \
    -s -S
