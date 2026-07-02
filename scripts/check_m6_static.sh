#!/usr/bin/env bash
set -euo pipefail
make check-m6
mkdir -p evidence/M6
cp build/pmm.undefined.txt evidence/M6/ 2>/dev/null || true
cp build/pmm.objdump.txt evidence/M6/
echo "[PASS] M6 static check + evidence tersimpan di evidence/M6/"
