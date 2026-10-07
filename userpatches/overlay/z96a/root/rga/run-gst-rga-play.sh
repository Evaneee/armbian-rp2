#!/bin/bash
# Raw NV12 + V4L2 RGA + waylandsink/autovideosink
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/1000}
export WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-wayland-0}
RAW=${1:-"$DIR/in4k.nv12"}
if [[ ! -f "$RAW" ]]; then
	"$DIR/prepare-media.sh"
	RAW="$DIR/in4k.nv12"
fi
SINK=waylandsink
gst-inspect-1.0 waylandsink >/dev/null 2>&1 || SINK=autovideosink
echo "Using sink=$SINK ; RGA IRQ before=$(grep rga /proc/interrupts | awk '{print $2}')"
gst-launch-1.0 -e \
  filesrc location="$RAW" ! \
  rawvideoparse format=nv12 width=3840 height=2160 framerate=30/1 ! \
  v4l2convert extra-controls=cid,rotate=90 ! \
  video/x-raw,format=NV12,width=1280,height=720 ! \
  videoconvert ! "$SINK"
echo "RGA IRQ after=$(grep rga /proc/interrupts | awk '{print $2}')"
