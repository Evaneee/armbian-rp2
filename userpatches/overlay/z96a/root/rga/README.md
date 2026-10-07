# Z96A 4K / RGA tests

| Path | Device | Result |
|------|--------|--------|
| RGA scale/rotate | mainline `/dev/video0` (`rockchip-rga`) via GStreamer `v4l2convert` | OK — IRQ on `fdeb0000.rga` increments |
| ffmpeg `scale_rkrga` / `overlay_rkrga` | vendor `/dev/rga` + librga | **not available** on mainline edge |
| mpv 4K decode | `--hwdec=rkmpp` | OK |
| mpv rotate/OSD | GPU/OSD | visual only — **not** RGA |

```bash
# Generate test4k.mp4 + in4k.nv12 (once; ~60–700MB)
./prepare-media.sh

# Prove RGA HW (watch IRQ):
grep rga /proc/interrupts
./run-rga-bench.sh
grep rga /proc/interrupts

# Visual mpv (MPP + GPU rotate + OSD):
./run-mpv-4k.sh

# Visual path that uses V4L2 RGA then displays:
./run-gst-rga-play.sh
```
