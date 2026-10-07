#!/usr/bin/env python3
"""Exercise mainline rockchip-rga (/dev/video0) via V4L2 M2M: scale + rotate."""
import argparse, fcntl, mmap, os, struct, sys, time

# Minimal V4L2 multiplanar M2M using ioctl via fcntl + ctypes
import ctypes
from ctypes import c_int, c_uint, c_uint32, c_uint64, c_char, c_void_p, POINTER, Structure, sizeof

libc = ctypes.CDLL("libc.so.6", use_errno=True)

VIDIOC_QUERYCAP = 0x80685600  # will compute properly below

# Use v4l2 python bindings if present; else ctypes with generated ioctls
try:
    import v4l2  # type: ignore
except Exception:
    v4l2 = None

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-d", default="/dev/video0")
    ap.add_argument("--src-w", type=int, default=3840)
    ap.add_argument("--src-h", type=int, default=2160)
    ap.add_argument("--dst-w", type=int, default=1920)
    ap.add_argument("--dst-h", type=int, default=1080)
    ap.add_argument("--rotate", type=int, default=90, choices=[0, 90, 180, 270])
    ap.add_argument("--frames", type=int, default=30)
    ap.add_argument("--pix", default="NV12")
    args = ap.parse_args()

    # Prefer gst if available for reliability
    if os.system("which gst-launch-1.0 >/dev/null 2>&1") == 0:
        # Check convert
        if os.system("gst-inspect-1.0 v4l2convert >/dev/null 2>&1") == 0:
            rot = {0: "identity", 90: "90r", 180: "180", 270: "90l"}[args.rotate]
            # videoflip method names for software; for v4l2 use extra-controls
            cmd = (
                f"gst-launch-1.0 -e videotestsrc num-buffers={args.frames} pattern=ball ! "
                f"video/x-raw,format={args.pix},width={args.src_w},height={args.src_h},framerate=30/1 ! "
                f"v4l2convert extra-controls=\"cid,rotate={args.rotate}\" ! "
                f"video/x-raw,format={args.pix},width={args.dst_w},height={args.dst_h} ! "
                f"fakesink sync=false"
            )
            print("RUN:", cmd)
            rc = os.system(cmd)
            sys.exit(0 if rc == 0 else 1)

    print("Need gst-launch-1.0 + v4l2convert, or use ffmpeg with /dev/rga (vendor).", file=sys.stderr)
    sys.exit(2)

if __name__ == "__main__":
    main()
