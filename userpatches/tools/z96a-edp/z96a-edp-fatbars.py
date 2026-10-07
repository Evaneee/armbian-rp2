#!/usr/bin/env python3
"""Show fat colorbars: HW wide BIST, then FB 8-bar pattern."""
import os, subprocess, sys, time

def sh(*a):
    return subprocess.check_output(a, text=True).strip()

def bl(v=13):
    open("/sys/class/backlight/backlight/brightness", "w").write(str(v))

def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "fb"
    bl(13)
    os.system("systemctl stop gdm gdm3 2>/dev/null")

    if mode == "hw":
        # BIST_EN | BIST_WIDTH(1) | TYPE colorbar = 0xC
        sh("busybox", "devmem", "0xfe0c002c", "32", "0xc")
        sh("busybox", "devmem", "0xfe0c0020", "32", "0x80")
        print("HW wide BIST CTL4=", sh("busybox", "devmem", "0xfe0c002c", "32"))
        return

    if mode == "hwfine":
        sh("busybox", "devmem", "0xfe0c002c", "32", "0x8")
        print("HW fine BIST CTL4=", sh("busybox", "devmem", "0xfe0c002c", "32"))
        return

    # FB fat bars (BIST off)
    sh("busybox", "devmem", "0xfe0c002c", "32", "0x0")
    w, h = 1920, 1080
    colors = [
        bytes([0, 0, 255, 0]),      # R
        bytes([0, 255, 0, 0]),      # G
        bytes([255, 0, 0, 0]),      # B
        bytes([0, 255, 255, 0]),    # Y
        bytes([255, 0, 255, 0]),    # M
        bytes([255, 255, 0, 0]),    # C
        bytes([255, 255, 255, 0]),  # W
        bytes([0, 0, 0, 0]),        # K
    ]
    bw = w // len(colors)
    row = b"".join(c * bw for c in colors)[: w * 4]
    with open("/dev/fb0", "r+b", buffering=0) as fb:
        fb.seek(0)
        fb.write(row * h)
    print("FB 8 fat bars (240px), BIST off, CTL4=",
          sh("busybox", "devmem", "0xfe0c002c", "32"))

if __name__ == "__main__":
    if os.geteuid() != 0:
        sys.exit("need root")
    main()
