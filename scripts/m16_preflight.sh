#!/usr/bin/env bash
set -euo pipefail
mkdir -p logs/m16 evidence/m16 build/m16
{
  echo "== M16 preflight =="
  date -Iseconds
  echo "== host =="
  uname -a
  lsb_release -a 2>/dev/null || cat /etc/os-release
  echo "== tools =="
  clang --version | head -n 1
  make --version | head -n 1
  nm --version | head -n 1
  readelf --version | head -n 1
  objdump --version | head -n 1
  sha256sum --version | head -n 1
  qemu-system-x86_64 --version | head -n 1 || true
  echo "== git =="
  git status --short
  git rev-parse --short HEAD || true
  echo "== subsystem probes =="
  find kernel fs -maxdepth 4 -type f | sort | sed -n '1,120p'
} | tee logs/m16/preflight.log
