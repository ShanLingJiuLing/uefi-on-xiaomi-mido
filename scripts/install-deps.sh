#!/bin/bash
set -o pipefail
export DEBIAN_FRONTEND=noninteractive
echo "===== installing build deps ====="
echo 666666 | sudo -S -p '' apt-get install -y --no-install-recommends \
  git build-essential uuid-dev acpica-tools nasm python3-dev python3-venv \
  pkg-config unzip bc device-tree-compiler curl ca-certificates \
  2>&1 | tail -15
echo "===== versions ====="
for t in git gcc g++ make iasl nasm dtc python3 curl; do
  printf '%-8s ' "$t"
  command -v "$t" >/dev/null 2>&1 && { "$t" --version 2>&1 | head -1; } || echo "(STILL MISSING)"
done
echo "===== disk after ====="
df -h / | tail -1
