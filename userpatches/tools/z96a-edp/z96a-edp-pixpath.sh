#!/bin/bash
# Diagnose VOP→Analogix pixel path. Root on board.
set -uo pipefail
dm(){ busybox devmem "$@"; }
bl(){ echo "$1" > /sys/class/backlight/backlight/brightness; }
cfg_done(){ # pulse VP1 cfg done
  local v; v=$(dm 0xfe040000 32); v=${v,,}; v=${v#0x}
  # set bit15 + bit1 (vp1)
  dm 0xfe040000 32 $(( (16#$v) | 0x8002 ))
}

systemctl stop gdm gdm3 2>/dev/null || true
bl 13

paint_fat() {
python3 - <<'PY'
w,h=1920,1080
colors=[bytes([0,0,255,0]),bytes([0,255,0,0]),bytes([255,0,0,0]),
        bytes([0,255,255,0]),bytes([255,0,255,0]),bytes([255,255,0,0]),
        bytes([255,255,255,0]),bytes([0,0,0,0])]
bw=w//len(colors)
row=b"".join(c*bw for c in colors)[:w*4]
open("/dev/fb0","r+b",buffering=0).write(row*h)
print("fb fat bars written")
PY
}

echo "===== 1) HW WIDE BIST (64px) FORMAT_SEL=0 (required) ====="
dm 0xfe0c0044 32 0x03          # clear FORMAT_SEL, keep pol
dm 0xfe0c002c 32 0x0c          # BIST_EN|WIDTH64|COLORBAR
dm 0xfe0c0020 32 0x80
echo "CTL4=$(dm 0xfe0c002c 32) CTL10=$(dm 0xfe0c0044 32)"
echo "LOOK: wide colorbars? (5s)"
sleep 5

echo "===== 2) CAPTURE timing + FB fat (BIST off) ====="
dm 0xfe0c002c 32 0x0
dm 0xfe0c0044 32 0x03          # CAPTURE
paint_fat
echo "CTL10=$(dm 0xfe0c0044 32) LOOK: 8 fat bars? (5s)"
sleep 5

echo "===== 3) REGISTER timing + FB fat ====="
dm 0xfe0c0044 32 0x13
paint_fat
echo "CTL10=$(dm 0xfe0c0044 32) LOOK: 8 fat bars? (5s)"
sleep 5

echo "===== 4) VP1 BG=red-ish + black fb (plane should cover; if red leaks, path ok) ====="
# DSP_BG often {R[9:0],G[9:0],B[9:0]} packed — try full red 0x3ff<<20
dm 0xfe040d2c 32 0x3ff00000
cfg_done
python3 -c 'open("/dev/fb0","r+b",buffering=0).write(bytes(1920*1080*4)); print("fb black")'
echo "BG=$(dm 0xfe040d2c 32) LOOK: black or red? (4s)"
sleep 4

echo "===== 5) RB swap on VP1 + fat bars ====="
dm 0xfe040d00 32 0x10200       # PRE_DITHER|RB_SWAP, OUT P888
cfg_done
dm 0xfe0c0044 32 0x03
paint_fat
echo "DSP=$(dm 0xfe040d00 32) LOOK: fat bars? (5s)"
sleep 5

echo "===== leave: WIDE BIST for clear visual ====="
dm 0xfe040d00 32 0x10000
cfg_done
dm 0xfe0c0044 32 0x03
dm 0xfe0c002c 32 0x0c
echo "DONE CTL4=$(dm 0xfe0c002c 32) CTL10=$(dm 0xfe0c0044 32)"
echo "Now should be WIDE hardware colorbars (64px)."
