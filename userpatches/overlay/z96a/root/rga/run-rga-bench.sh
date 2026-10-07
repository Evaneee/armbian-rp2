#!/bin/bash
# Mainline rockchip-rga via V4L2 (/dev/video0). ffmpeg scale_rkrga needs /dev/rga (vendor).
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
RAW=${1:-"$DIR/in4k.nv12"}
if [[ ! -f "$RAW" ]]; then
	"$DIR/prepare-media.sh"
	RAW="$DIR/in4k.nv12"
fi
echo "RGA IRQ before: $(grep rga /proc/interrupts | awk '{print $2}')"
gst-launch-1.0 -e \
  filesrc location="$RAW" ! \
  rawvideoparse format=nv12 width=3840 height=2160 framerate=30/1 ! \
  v4l2convert extra-controls=cid,rotate=90 ! \
  video/x-raw,format=NV12,width=1920,height=1080 ! \
  fpsdisplaysink video-sink=fakesink text-overlay=false sync=false
echo "RGA IRQ after:  $(grep rga /proc/interrupts | awk '{print $2}')"
