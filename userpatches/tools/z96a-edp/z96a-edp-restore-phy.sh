#!/bin/bash
set -uo pipefail
dm(){ busybox devmem "$@"; }
# Restore naneng CON6 (was ~0x997d with SSC). GRF: (mask<<16)|val
dm 0xfdcb0018 32 0xffff997d
echo "CON0=$(dm 0xfdcb0000 32) CON6=$(dm 0xfdcb0018 32) ST0=$(dm 0xfdcb0030 32)"
# Restore VP1 DSP P888+pre_dither
dm 0xfe040d00 32 0x10000
dm 0xfe040000 32 0x8002
echo "DSP=$(dm 0xfe040d00 32)"
# fat bars
python3 - <<'PY'
w,h=1920,1080
cs=[bytes([0,0,255,0]),bytes([0,255,0,0]),bytes([255,0,0,0]),bytes([0,255,255,0]),
    bytes([255,0,255,0]),bytes([255,255,0,0]),bytes([255,255,255,0]),bytes([0,0,0,0])]
row=b"".join(c*(w//8) for c in cs)[:w*4]
open("/dev/fb0","r+b",buffering=0).write(row*h)
print("fat bars")
PY
echo 13 > /sys/class/backlight/backlight/brightness
dm 0xfe0c002c 32 0x0
dm 0xfe0c0020 32 0x80
dm 0xfe0c0608 32 0x77
echo "sink:"; python3 /home/evanee/z96a-edp/z96a-edp-aux.py read 0x202 -n 6
