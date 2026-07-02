#!/usr/bin/env bash
set -euo pipefail
make check-m7
mkdir -p evidence/M7
cp build/vmm.undefined.txt evidence/M7/ 2>/dev/null || true
cp build/vmm.objdump.txt evidence/M7/
echo "[PASS] M7 static check + evidence tersimpan di evidence/M7/"
