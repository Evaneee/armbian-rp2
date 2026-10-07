#!/bin/bash
# Generate sample JPEGs for jpegd_test (not shipped in the image).
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"
command -v ffmpeg >/dev/null || { echo "need ffmpeg" >&2; exit 1; }

ffmpeg -y -hide_banner -nostdin -loglevel error \
  -f lavfi -i "testsrc2=size=1920x1080:rate=1" -frames:v 1 -q:v 2 sample-1080.jpg
ffmpeg -y -hide_banner -nostdin -loglevel error \
  -f lavfi -i "testsrc2=size=3840x2160:rate=1" -frames:v 1 -q:v 3 sample-4k.jpg

ls -lh sample-1080.jpg sample-4k.jpg
