#!/usr/bin/env bash
set -euo pipefail

echo "[M7-PREFLIGHT] pemeriksaan lingkungan dan hasil M0-M6"

need_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "[FAIL] command tidak ditemukan: $1" >&2
    exit 1
  fi
  echo "[OK] $1 -> $(command -v "$1")"
}

need_file() {
  if [ ! -f "$1" ]; then
    echo "[FAIL] file wajib tidak ada: $1" >&2
    exit 1
  fi
  echo "[OK] file ada: $1"
}

need_dir() {
  if [ ! -d "$1" ]; then
    echo "[FAIL] direktori wajib tidak ada: $1" >&2
    exit 1
  fi
  echo "[OK] direktori ada: $1"
}

need_cmd git
need_cmd make
need_cmd clang
need_cmd ld.lld
need_cmd readelf
need_cmd objdump
need_cmd nm
need_cmd qemu-system-x86_64

need_dir kernel/include
need_dir kernel/core
need_dir kernel/tests
need_file kernel/include/mcsos/kernel/pmm.h
need_file kernel/core/pmm.c
need_file kernel/include/mcsos/kernel/vmm.h
need_file kernel/core/vmm.c
need_file kernel/tests/test_vmm_host.c
need_file Makefile

if ! grep -R "pmm_alloc_frame" kernel/include kernel/core >/dev/null 2>&1; then
  echo "[FAIL] API pmm_alloc_frame dari M6 tidak ditemukan" >&2
  exit 1
fi
if ! grep -R "pmm_free_frame" kernel/include kernel/core >/dev/null 2>&1; then
  echo "[FAIL] API pmm_free_frame dari M6 tidak ditemukan" >&2
  exit 1
fi
if ! grep -R "x86_64_trap_dispatch" kernel/include kernel/arch kernel/core >/dev/null 2>&1; then
  echo "[WARN] dispatcher trap M4 belum ditemukan; page fault logging M7 harus diintegrasikan manual"
else
  echo "[OK] dispatcher trap M4 terdeteksi"
fi
if ! grep -R "timer" kernel/include kernel/arch kernel/core >/dev/null 2>&1; then
  echo "[WARN] artefak timer M5 belum terdeteksi; M7 tetap dapat diuji host-side, tetapi readiness M5 harus diperbaiki"
else
  echo "[OK] artefak timer M5 terdeteksi"
fi

make clean >/dev/null 2>&1 || true
make check-m7

if nm -u build/vmm.o | grep -v '^$'; then
  echo "[FAIL] build/vmm.o memiliki unresolved symbol" >&2
  exit 1
fi

objdump -dr build/vmm.o > build/vmm.objdump.txt
grep -q "invlpg" build/vmm.objdump.txt || { echo "[FAIL] invlpg tidak terlihat pada disassembly" >&2; exit 1; }
grep -q "cr3" build/vmm.objdump.txt || { echo "[FAIL] akses CR3 tidak terlihat pada disassembly" >&2; exit 1; }

echo "[PASS] M7 preflight selesai. Lanjutkan integrasi QEMU hanya setelah laporan M0-M6 lengkap."
