#!/bin/bash
# Sweep eDP pin polarities + Analogix sync pol. Watch for real image.
set -uo pipefail
dm(){ busybox devmem "$@"; }
bl(){ echo "$1" > /sys/class/backlight/backlight/brightness; }
cfg(){ dm 0xfe040000 32 0x8002; }

systemctl stop gdm gdm3 2>/dev/null || true
bl 13
dm 0xfe0c002c 32 0x0   # BIST off
# fat fb bars so a good polarity is obvious
python3 - <<'PY'
w,h=1920,1080
cs=[bytes([0,0,255,0]),bytes([0,255,0,0]),bytes([255,0,0,0]),bytes([0,255,255,0]),
    bytes([255,0,255,0]),bytes([255,255,0,0]),bytes([255,255,255,0]),bytes([0,0,0,0])]
bw=w//8
row=b"".join(c*bw for c in cs)[:w*4]
open("/dev/fb0","r+b",buffering=0).write(row*h)
print("fat bars ready")
PY

# Keep CFG_DONE_IMD bit28 in IF_POL writes
base_hi=0x10000000

echo "=== Sweep VOP EDP_PIN_POL (bits15:12), 16 values × 1.5s ==="
for p in $(seq 0 15); do
  val=$(( base_hi | (p << 12) ))
  dm 0xfe040030 32 "$val"
  cfg
  printf 'POL=%x IF_POL=%s\n' "$p" "$(dm 0xfe040030 32)"
  sleep 1.5
done

echo "=== Analogix CTL10 sync pol variants (FORMAT_SEL=REGISTER) ==="
for v in 0x10 0x11 0x12 0x13 0x14 0x15 0x16 0x17; do
  dm 0xfe0c0044 32 "$v"
  echo "CTL10=$v"
  sleep 1.5
done

echo "=== Toggle F_VALID (SYS3) ==="
dm 0xfe0c0608 32 0x70   # HPD only
echo "F_VALID off"
sleep 2
dm 0xfe0c0608 32 0x77
echo "F_VALID on"
sleep 2

echo "=== TX ASSR off ==="
dm 0xfe0c09d8 32 0x50
python3 /home/evanee/z96a-edp/z96a-edp-aux.py write 0x10a 0x00 2>/dev/null || true
sleep 2

echo "=== restore ASSR + best-effort defaults ==="
dm 0xfe0c09d8 32 0xd0
python3 /home/evanee/z96a-edp/z96a-edp-aux.py write 0x10a 0x01 2>/dev/null || true
dm 0xfe040030 32 0x10008000
dm 0xfe0c0044 32 0x13
dm 0xfe0c0608 32 0x77
cfg
echo "DONE IF_POL=$(dm 0xfe040030 32) CTL10=$(dm 0xfe0c0044 32)"
echo "Tell me: any POL value showed FAT color bars / desktop-like image?"
