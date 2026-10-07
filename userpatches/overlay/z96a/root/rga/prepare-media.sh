#!/bin/bash
# Build sample 4K H264 + raw NV12 for RGA benches (not shipped in the image).
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

FRAMES=${FRAMES:-60}
if [[ ! -f test4k.mp4 ]]; then
	echo "Generating test4k.mp4 (8s 3840x2160 H264)..."
	ffmpeg -y -hide_banner -loglevel error \
		-f lavfi -i "testsrc2=size=3840x2160:rate=30" \
		-f lavfi -i "sine=frequency=880:sample_rate=48000" \
		-t 8 -c:v libx264 -preset ultrafast -pix_fmt yuv420p -g 30 \
		-c:a aac -shortest test4k.mp4
fi

if [[ ! -f in4k.nv12 ]]; then
	echo "Decoding ${FRAMES} frames via rkmpp → in4k.nv12..."
	ffmpeg -y -hide_banner -loglevel error -hwaccel rkmpp -i test4k.mp4 \
		-frames:v "$FRAMES" -f rawvideo -pix_fmt nv12 in4k.nv12 \
	|| ffmpeg -y -hide_banner -loglevel error -i test4k.mp4 \
		-frames:v "$FRAMES" -f rawvideo -pix_fmt nv12 in4k.nv12
fi

if [[ ! -f wm.png ]]; then
	ffmpeg -y -hide_banner -loglevel error -f lavfi -i color=c=red@0.7:s=320x80 \
		-frames:v 1 wm.png
fi

ls -lh test4k.mp4 in4k.nv12 wm.png
echo "Media ready under $DIR"
