#!/bin/bash
# Stress RK3568 jpegd; watch /proc/interrupts.
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"
chmod +x jpegd_test prepare-media.sh 2>/dev/null || true

command -v mpi_dec_test >/dev/null || {
	echo "install: apt-get install -y rockchip-mpp-demos" >&2
	exit 1
}

[[ -f sample-1080.jpg && -f sample-4k.jpg ]] || ./prepare-media.sh

N=${N:-50}
echo "===== jpegd IRQ before ====="
grep jpegd /proc/interrupts || true

echo "===== 1080p x${N} ====="
./jpegd_test -n "$N" sample-1080.jpg

echo "===== 4K x${N} ====="
./jpegd_test -n "$N" sample-4k.jpg

echo "===== write one YUV ====="
./jpegd_test -o /tmp/jpegd-out.yuv sample-1080.jpg
ls -lh /tmp/jpegd-out.yuv

echo "===== jpegd IRQ after ====="
grep jpegd /proc/interrupts || true
echo DONE
