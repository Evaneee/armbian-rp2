#!/bin/bash
# Live-align with vendor eDP clock/pixel path (no kernel rebuild).
set -euo pipefail
python3 - <<'PY'
import os, struct, mmap

def poke_enable_edp200():
    cru = 0xfdd20000
    fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
    mm = mmap.mmap(fd, 0x1000, offset=cru)
    before = struct.unpack_from("<I", mm, 0x354)[0]
    struct.pack_into("<I", mm, 0x354, (1 << 9) << 16)
    after = struct.unpack_from("<I", mm, 0x354)[0]
    mm.close(); os.close(fd)
    print(f"clk_edp_200m gate21 {before:#010x} -> {after:#010x}")

def vp1_dsp():
    fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
    mm = mmap.mmap(fd, 0x2000, offset=0xfe040000)
    dsp = struct.unpack_from("<I", mm, 0xd00)[0]
    print(f"VP1_DSP_CTRL={dsp:#010x} OUT_MODE={dsp & 0xf} PRE_DITH={(dsp >> 16) & 1}")
    if (dsp >> 16) & 1:
        new = dsp & ~(1 << 16)
        struct.pack_into("<I", mm, 0xd00, new)
        struct.pack_into("<I", mm, 0, (1 << 15) | (1 << 1) | (1 << 17))
        print(f"VP1 cleared PRE_DITH -> {struct.unpack_from('<I', mm, 0xd00)[0]:#010x}")
    print(f"VOP IF={struct.unpack_from('<I', mm, 0x28)[0]:#010x}")
    mm.close(); os.close(fd)

def force_valid():
    fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
    mm = mmap.mmap(fd, 0x1000, offset=0xfe0c0000)
    sys3 = struct.unpack_from("<I", mm, 0x608)[0]
    struct.pack_into("<I", mm, 0x608, sys3 | 0x37)
    print(f"SYS_CTL_3 {sys3:#x} -> {struct.unpack_from('<I', mm, 0x608)[0]:#x}")
    for n, o in [("SYS1",0x600),("SYS2",0x604),("SYS3",0x608),("SYS4",0x60c),("VCTL1",0x20),("VCTL4",0x2c),("VCTL10",0x44)]:
        print(f"  {n}={struct.unpack_from('<I', mm, o)[0]:#04x}")
    mm.close(); os.close(fd)

def fatbars():
    W, H, stride = 1920, 1080, 7680
    try:
        with open("/sys/class/graphics/fb0/stride") as f:
            stride = int(f.read())
    except Exception:
        pass
    fd = os.open("/dev/fb0", os.O_RDWR)
    mm = mmap.mmap(fd, stride * H)
    cols = [0x000000FF, 0x0000FF00, 0x00FF0000, 0x00FFFFFF, 0x0000FFFF, 0x00FFFF00]
    bw = W // 6
    for y in range(H):
        row = y * stride
        for x in range(W):
            struct.pack_into("<I", mm, row + x * 4, cols[min(x // bw, 5)])
    mm.close(); os.close(fd)
    print("painted 6 fat bars")

poke_enable_edp200()
vp1_dsp()
force_valid()
try:
    open("/sys/class/graphics/fb0/blank", "w").write("0\n")
except Exception as e:
    print("blank:", e)
fatbars()
PY
grep clk_edp_200m /sys/kernel/debug/clk/clk_summary 2>/dev/null || true
echo "Done — expect 6 wide bars R/G/B/W/Y/C"
