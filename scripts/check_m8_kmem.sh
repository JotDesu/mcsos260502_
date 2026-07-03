#!/usr/bin/env bash
set -euo pipefail

printf '[M8] checking repository baseline...\n'
required=(
  include/mcsos/kmem.h
  kernel/mm/kmem.c
  tests/test_kmem.c
  Makefile
)
for f in "${required[@]}"; do
  if [[ ! -f "$f" ]]; then
    printf '[FAIL] missing %s\n' "$f" >&2
    exit 1
  fi
done

printf '[M8] checking toolchain...\n'
command -v clang >/dev/null
command -v nm >/dev/null
command -v objdump >/dev/null
command -v readelf >/dev/null
command -v make >/dev/null

printf '[M8] tool versions...\n'
clang --version | head -n 1
ld.lld --version 2>/dev/null | head -n 1 || true
make --version | head -n 1

printf '[M8] running make check-m8...\n'
make check-m8

grep -q 'PASS' build/test_kmem.log
printf '[PASS] M8 preflight completed.\n'
