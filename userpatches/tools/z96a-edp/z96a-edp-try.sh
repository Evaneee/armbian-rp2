#!/bin/bash
# Run as root on Z96A. Cycles patterns; relies on human/phone for screen.
set -uo pipefail
DP=0xfe0c0000
AUX=/home/evanee/z96a-edp/z96a-edp-aux.py
dm() { busybox devmem "$@"; }
bl() { echo "$1" > /sys/class/backlight/backlight/brightness; }

systemctl stop gdm gdm3 2>/dev/null || true
bl 13

paint() {
	python3 - "$1" <<'PY'
import sys
color=sys.argv[1]
fb=open("/dev/fb0","r+b",buffering=0)
n=1920*1080
if color=="red":
    pix=bytes([0,0,255,0])
elif color=="green":
    pix=bytes([0,255,0,0])
elif color=="blue":
    pix=bytes([255,0,0,0])
elif color=="black":
    pix=bytes([0,0,0,0])
elif color=="white":
    pix=bytes([255,255,255,0])
else:
    raise SystemExit("bad color")
fb.seek(0); fb.write(pix*n)
print("painted", color, "head", open("/dev/fb0","rb").read(8).hex())
PY
}

echo "=== baseline sink ==="
python3 "$AUX" read 0x202 -n 6

echo "=== A: ASSR off + BIST + REGISTER ==="
dm 0xfe0c09d8 32 0x50
python3 "$AUX" write 0x10a 0x00 || true
dm 0xfe0c0044 32 0x13
dm 0xfe0c002c 32 0x8
dm 0xfe0c0020 32 0x80
dm 0xfe0c0608 32 0x77
# fixed M/N
dm 0xfe0c060c 32 0x0c
dm 0xfe0c0700 32 0x07; dm 0xfe0c0704 32 0x43; dm 0xfe0c0708 32 0x00
dm 0xfe0c070c 32 0x00; dm 0xfe0c0710 32 0x80; dm 0xfe0c0714 32 0x00
echo "SCREEN? expect colorbars (ASSR off+BIST) — wait 4s"
sleep 4
python3 "$AUX" read 0x202 -n 6

echo "=== B: swing level1 on lanes ==="
# DPCD training lane0/1 set VS=1 PE=0 → 0x01 each; also bump TX
python3 "$AUX" write 0x103 0x01 || true
python3 "$AUX" write 0x104 0x01 || true
# Analogix LN0/LN1 training ctl @ 0x68c / 0x690 — set VS level1
dm 0xfe0c068c 32 0x01
dm 0xfe0c0690 32 0x01
echo "SCREEN? BIST + VS1 — wait 4s"
sleep 4

echo "=== C: BIST off, paint BLACK ==="
dm 0xfe0c002c 32 0x0
paint black
echo "SCREEN? should be BLACK if pixels work — wait 4s"
sleep 4

echo "=== D: paint RED ==="
paint red
echo "SCREEN? should be RED — wait 4s"
sleep 4

echo "=== E: paint WHITE ==="
paint white
echo "SCREEN? white — wait 3s"
sleep 3

echo "=== F: re-enable ASSR + BIST ==="
dm 0xfe0c09d8 32 0xd0
python3 "$AUX" write 0x10a 0x01 || true
dm 0xfe0c002c 32 0x8
echo "SCREEN? colorbars with ASSR — wait 4s"
sleep 4

echo "=== final regs/dpcd ==="
python3 "$AUX" dump
echo "CTL4=$(dm 0xfe0c002c 32) LINK=$(dm 0xfe0c09d8 32) SYS3=$(dm 0xfe0c0608 32)"
echo DONE
