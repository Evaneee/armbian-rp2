#!/bin/bash
# Identify whether thin stripes are Analogix BIST or something else.
set -uo pipefail
dm(){ busybox devmem "$@"; }
bl(){ echo "$1" > /sys/class/backlight/backlight/brightness; }
systemctl stop gdm gdm3 2>/dev/null || true
bl 13
dm 0xfe0c0020 32 0x80
dm 0xfe0c0044 32 0x03   # FORMAT_SEL=0 for BIST

echo "A) BIST OFF + solid RED fb — 5s (expect solid red OR white/stripes if not fb)"
dm 0xfe0c002c 32 0x0
python3 -c 'open("/dev/fb0","r+b",buffering=0).write(bytes([0,0,255,0])*1920*1080)'
sleep 5

echo "B) BIST colorbar FINE 0x8 — 5s"
dm 0xfe0c002c 32 0x8
sleep 5

echo "C) BIST colorbar WIDE 0xC — 5s"
dm 0xfe0c002c 32 0xc
sleep 5

echo "D) BIST white/gray/black WIDE 0xD — 5s (should be gray bands, NOT rainbow)"
dm 0xfe0c002c 32 0xd
sleep 5

echo "E) BIST moving-white WIDE 0xE — 5s (should MOVE)"
dm 0xfe0c002c 32 0xe
sleep 5

echo "F) BIST OFF again solid GREEN — 5s"
dm 0xfe0c002c 32 0x0
python3 -c 'open("/dev/fb0","r+b",buffering=0).write(bytes([0,255,0,0])*1920*1080)'
sleep 5

echo "G) leave BIST OFF solid BLUE"
python3 -c 'open("/dev/fb0","r+b",buffering=0).write(bytes([255,0,0,0])*1920*1080)'
echo "CTL4=$(dm 0xfe0c002c 32) CTL10=$(dm 0xfe0c0044 32)"
echo "DONE — tell which steps looked DIFFERENT"
