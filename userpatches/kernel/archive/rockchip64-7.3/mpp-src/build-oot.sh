#!/usr/bin/env bash
# Out-of-tree build for rk_vcodec.ko — edit mpp-src, rebuild only the module (~1–2 min).
# Uses Armbian Docker (gcc 14) against the already-built edge kernel worktree.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
ARMBIAN="${ARMBIAN:-/home/evanee/mygit/armbian-rp2}"
KDIR_HOST="${KDIR:-$ARMBIAN/cache/sources/linux-kernel-worktree/7.2__rockchip64__arm64}"
KDIR_DOCKER="/armbian/cache/sources/linux-kernel-worktree/7.2__rockchip64__arm64"
OUT_HOST="${OUT:-/tmp/z96a-mpp-oot}"
OUT_DOCKER="/oot"
JOBS="${JOBS:-$(nproc)}"
DOCKER_IMAGE="${DOCKER_IMAGE:-}"

if [[ ! -f "$KDIR_HOST/Module.symvers" || ! -f "$KDIR_HOST/.config" ]]; then
	echo "ERROR: kernel worktree not ready: $KDIR_HOST" >&2
	echo "Run a full edge kernel build once first." >&2
	exit 1
fi

if [[ -z "$DOCKER_IMAGE" ]]; then
	DOCKER_IMAGE="$(docker images --format '{{.Repository}}:{{.Tag}}' 'armbian.local.only/armbian-build' | head -1 || true)"
fi
if [[ -z "$DOCKER_IMAGE" ]]; then
	echo "ERROR: no armbian.local.only/armbian-build image; run one full compile.sh first." >&2
	exit 1
fi

mkdir -p "$OUT_HOST"
rsync -a --delete \
	--exclude 'oot-out/' --exclude 'build-oot.sh' --exclude 'README.md' \
	--exclude '*.ko' --exclude '*.o' --exclude '*.mod' --exclude '*.mod.c' \
	--exclude 'modules.order' --exclude 'Module.symvers' --exclude '.*.cmd' \
	--exclude '*.a' --exclude '.tmp_versions/' \
	"$ROOT/" "$OUT_HOST/"

echo "==> Building rk_vcodec.ko in $DOCKER_IMAGE"
echo "    KDIR=$KDIR_HOST  OUT=$OUT_HOST"

docker run --rm \
	-v "$ARMBIAN:/armbian" \
	-v "$OUT_HOST:$OUT_DOCKER" \
	"$DOCKER_IMAGE" \
	bash -lc "make -C '$KDIR_DOCKER' M='$OUT_DOCKER' ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION=-edge-rockchip64 -j$JOBS modules"

KO="$OUT_HOST/rk_vcodec.ko"
ls -lh "$KO"
file "$KO"
modinfo "$KO" 2>/dev/null | head -20 || true
echo "OK: $KO"
echo "Push: scp $KO evanee@192.168.71.38:/tmp/ && ssh … 'sudo mkdir -p /lib/modules/\$(uname -r)/updates && sudo cp /tmp/rk_vcodec.ko … && sudo depmod -a && sudo modprobe -r rk_vcodec; sudo modprobe rk_vcodec'"
