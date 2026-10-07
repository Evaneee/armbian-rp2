#!/bin/bash
# Visual 4K: MPP decode + GPU scale/rotate + OSD "watermark"
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/1000}
export WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-wayland-0}
export DISPLAY=${DISPLAY:-:0}
VIDEO=${1:-"$DIR/test4k.mp4"}
if [[ ! -f "$VIDEO" ]]; then
	"$DIR/prepare-media.sh"
	VIDEO="$DIR/test4k.mp4"
fi
exec mpv --hwdec=rkmpp --vo=gpu \
  --gpu-hwdec-interop=auto \
  --video-rotate=90 \
  --autofit=80% \
  --osd-level=2 \
  --osd-font-size=48 \
  --osd-color='#FFFF00' \
  --osd-msg1='Z96A RGA/MPP TEST' \
  --osd-playing-msg='4K hwdec=rkmpp rotate=90' \
  --loop-file=inf \
  "$VIDEO"
