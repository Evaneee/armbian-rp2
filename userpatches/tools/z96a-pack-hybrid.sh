#!/usr/bin/env bash
# Pack a small U-Boot + kernel FIT hybrid image for Z96A Maskrom flash.
# Boots to an initramfs shell (no rootfs / no USB) for eMMC bring-up.
#
# Layout (eMMC) — must match Armbian fat boot + rootfs (BOOTSIZE=256):
#   sector 64        u-boot-rockchip.bin          (ends before 16 MiB)
#   sector 32768     FAT boot partition           (boot.scr + FIT only)
#   sector 557056    rootfs                       (16 MiB + 256 MiB) — NEVER written by --flash
#
# Usage (from armbian-rp2 root):
#   ./userpatches/tools/z96a-pack-hybrid.sh edge
#   ./userpatches/tools/z96a-pack-hybrid.sh edge --flash       # safe: no GPT, no rootfs
#   ./userpatches/tools/z96a-pack-hybrid.sh edge --flash-uboot # U-Boot only
#   ./userpatches/tools/z96a-pack-hybrid.sh edge --flash-wipe  # DESTRUCTIVE GPT rewrite
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT_DIR="${REPO_ROOT}/output/z96a-hybrid"
Z96A_BUILD_ID="${Z96A_BUILD_ID:-z96a-edp-unknown}"
Z96A_BUILD_TS="${Z96A_BUILD_TS:-}"
WORK_DIR="${OUT_DIR}/work"
FLASH_MODE="" # flash | flash-uboot | flash-wipe
BRANCH="${BRANCH:-current}"

die() { echo "ERROR: $*" >&2; exit 1; }
need() { command -v "$1" >/dev/null || die "missing tool: $1"; }

for arg in "$@"; do
	case "${arg}" in
		--flash) FLASH_MODE="flash" ;;
		--flash-uboot) FLASH_MODE="flash-uboot" ;;
		--flash-wipe) FLASH_MODE="flash-wipe" ;;
		--branch=*) BRANCH="${arg#*=}" ;;
		current | edge | vendor) BRANCH="${arg}" ;;
		*) die "unknown arg: ${arg} (use current|edge|vendor|--flash|--flash-uboot|--flash-wipe)" ;;
	esac
done

UBOOT_BIN="${UBOOT_BIN:-${REPO_ROOT}/cache/sources/u-boot-worktree/u-boot-z96a-rk3568-laptop/v2026.07/u-boot-rockchip.bin}"
SPL_LOADER="${SPL_LOADER:-${REPO_ROOT}/cache/sources/rkbin-tools/rk35/rk356x_spl_loader_v1.21.113.bin}"

case "${BRANCH}" in
	current) KERNEL_WT="6.18__rockchip64__arm64" ;;
	edge) KERNEL_WT="7.2__rockchip64__arm64" ;;
	vendor) KERNEL_WT="" ;; # deb-only; no mainline worktree override
	*) die "BRANCH must be current|edge|vendor (got ${BRANCH})" ;;
esac

# Prefer newest matching debs for BRANCH
IMAGE_DEB="${IMAGE_DEB:-$(ls -t "${REPO_ROOT}"/output/debs/linux-image-${BRANCH}-rockchip64_*_arm64__*.deb 2>/dev/null | head -1)}"
DTB_DEB="${DTB_DEB:-$(ls -t "${REPO_ROOT}"/output/debs/linux-dtb-${BRANCH}-rockchip64_*_arm64__*.deb 2>/dev/null | head -1)}"

# Boot FAT must stay inside Armbian BOOTSIZE (256 MiB). FIT≈40MiB → 64 MiB is enough.
# Do not ship a duplicate Image on FAT (kernel is already inside the FIT).
BOOT_PART_MIB=256
FAT_MIB=64
GAP_SECTORS=32768 # 16 MiB — U-Boot only; rootfs/boot must not start before this
SECTOR=512
FAT_SECTORS=$((FAT_MIB * 1024 * 1024 / SECTOR))
BOOT_PART_SECTORS=$((BOOT_PART_MIB * 1024 * 1024 / SECTOR))
ROOTFS_START_SECTOR=$((GAP_SECTORS + BOOT_PART_SECTORS)) # 557056 @ 256MiB boot
UBOOT_SEEK=64
FIT_LOAD_ADDR=0x02080000
[[ "${FAT_SECTORS}" -le "${BOOT_PART_SECTORS}" ]] || die "FAT_MIB=${FAT_MIB} exceeds BOOT_PART_MIB=${BOOT_PART_MIB}"

for t in mkimage gzip cpio sgdisk mkfs.vfat mcopy mmd dpkg-deb curl file; do need "$t"; done
[[ -n "${IMAGE_DEB}" && -f "${IMAGE_DEB}" ]] || die "no linux-image-${BRANCH} deb in output/debs"
[[ -n "${DTB_DEB}" && -f "${DTB_DEB}" ]] || die "no linux-dtb-${BRANCH} deb in output/debs"
[[ -f "${UBOOT_BIN}" ]] || die "U-Boot missing: ${UBOOT_BIN}"

echo "==> OUT ${OUT_DIR}  BRANCH=${BRANCH}"
echo "    kernel ${IMAGE_DEB}"
echo "    dtb    ${DTB_DEB}"
echo "    uboot  ${UBOOT_BIN}"

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"/{extract,initramfs/{bin,dev,proc,sys,tmp,newroot},fat,busybox}

# --- extract Image + DTB ---
dpkg-deb -x "${IMAGE_DEB}" "${WORK_DIR}/extract"
dpkg-deb -x "${DTB_DEB}" "${WORK_DIR}/extract"
KERNEL_IMG="$(ls "${WORK_DIR}"/extract/boot/vmlinuz-* | head -1)"
DTB_PATH="${WORK_DIR}/extract/boot/dtb-"*/rockchip/rk3568-z96a.dtb
DTB_PATH="$(ls ${DTB_PATH} | head -1)"
# Prefer freshly built artifacts from kernel worktree / override
if [[ -n "${KERNEL_WT}" ]]; then
	IMAGE_OVERRIDE="${IMAGE_OVERRIDE:-${REPO_ROOT}/cache/sources/linux-kernel-worktree/${KERNEL_WT}/arch/arm64/boot/Image}"
	DTB_OVERRIDE="${DTB_OVERRIDE:-${REPO_ROOT}/cache/sources/linux-kernel-worktree/${KERNEL_WT}/arch/arm64/boot/dts/rockchip/rk3568-z96a.dtb}"
fi
[[ -n "${IMAGE_OVERRIDE:-}" && -f "${IMAGE_OVERRIDE}" ]] && KERNEL_IMG="${IMAGE_OVERRIDE}"
[[ -n "${DTB_OVERRIDE:-}" && -f "${DTB_OVERRIDE}" ]] && DTB_PATH="${DTB_OVERRIDE}"
[[ -f "${KERNEL_IMG}" ]] || die "vmlinuz/Image not found"
[[ -f "${DTB_PATH}" ]] || die "rk3568-z96a.dtb not found"
echo "    using Image ${KERNEL_IMG}"
echo "    using DTB ${DTB_PATH}"
cp -f "${KERNEL_IMG}" "${WORK_DIR}/Image"
# Refuse packing a kernel that does not contain the expected build stamp
if [[ -n "${Z96A_BUILD_ID}" && "${Z96A_BUILD_ID}" != "z96a-edp-unknown" ]]; then
	if ! grep -aFq "${Z96A_BUILD_ID}" "${WORK_DIR}/Image"; then
		die "Image missing build stamp ${Z96A_BUILD_ID} — rebuild kernel before pack"
	fi
	echo "    Image stamp OK: ${Z96A_BUILD_ID}"
fi
cp -f "${DTB_PATH}" "${WORK_DIR}/rk3568-z96a.dtb"

# --- arm64 busybox for initramfs ---
BB_DEB="${WORK_DIR}/busybox-static.deb"
if [[ ! -f "${BB_DEB}" ]]; then
	curl -fsSL -o "${BB_DEB}" \
		'http://ports.ubuntu.com/ubuntu-ports/pool/universe/b/busybox/busybox-static_1.38.0-3ubuntu4_arm64.deb'
fi
dpkg-deb -x "${BB_DEB}" "${WORK_DIR}/busybox"
BB="${WORK_DIR}/busybox/usr/bin/busybox-static"
file "${BB}" | grep -q 'aarch64\|ARM aarch64' || die "busybox is not aarch64"
cp -f "${BB}" "${WORK_DIR}/initramfs/bin/busybox"
chmod 755 "${WORK_DIR}/initramfs/bin/busybox"
# Pre-rendered 1920x1080 XRGB8888 patterns for white-screen bring-up
mkdir -p "${WORK_DIR}/initramfs/opt"
for pat in z96a-fb-bars.rgb z96a-fb-black.rgb; do
	if [[ -f "${REPO_ROOT}/userpatches/tools/${pat}" ]]; then
		cp -f "${REPO_ROOT}/userpatches/tools/${pat}" "${WORK_DIR}/initramfs/opt/${pat}"
		echo "    packed /opt/${pat}"
	fi
done
# applets via host qemu if present, else common set
APPLETS=(sh ash mount umount mkdir ls cat echo dmesg sleep uname reboot poweroff
	mdev lsmod blkid find grep head less more ps top free df du cp mv rm ln
	chmod chown kill hostname ifconfig ip route wget nc hexdump xxd devmem dd)
if command -v qemu-aarch64-static >/dev/null; then
	mapfile -t APPLETS < <(qemu-aarch64-static "${BB}" --list)
fi
(
	cd "${WORK_DIR}/initramfs/bin"
	for a in "${APPLETS[@]}"; do
		[[ "$a" == "busybox" ]] && continue
		ln -sf busybox "$a"
	done
	ln -sf busybox sh
)

# Pack DRM modules — rockchipdrm is =m so hybrid must modprobe or eDP stays blank.
KVER="$(ls "${WORK_DIR}/extract/lib/modules" | head -1)"
MODSRC="${WORK_DIR}/extract/lib/modules/${KVER}"
MODDST="${WORK_DIR}/initramfs/lib/modules/${KVER}"
mkdir -p "${MODDST}"
python3 - "${MODSRC}" "${MODDST}" <<'PY'
import shutil, sys
from pathlib import Path
src, dst = Path(sys.argv[1]), Path(sys.argv[2])
deps = {}
for line in (src / "modules.dep").read_text().splitlines():
	if not line.strip():
		continue
	left, _, right = line.partition(":")
	deps[left.strip()] = [x.strip() for x in right.split() if x.strip()]
want = [
	"kernel/drivers/gpu/drm/rockchip/rockchipdrm.ko",
	"kernel/drivers/gpu/drm/bridge/analogix/analogix_dp.ko",
	"kernel/drivers/gpu/drm/panel/panel-simple.ko",
	"kernel/drivers/video/backlight/pwm_bl.ko",
]
need, stack = set(), list(want)
while stack:
	m = stack.pop()
	key = next((k for k in deps if k == m or k.endswith("/" + Path(m).name)), None)
	if not key or key in need:
		continue
	need.add(key)
	stack.extend(deps[key])
for rel in sorted(need):
	s, d = src / rel, dst / rel
	d.parent.mkdir(parents=True, exist_ok=True)
	shutil.copy2(s, d)
	print("  +", rel)
for name in ("modules.dep", "modules.alias", "modules.softdep", "modules.builtin", "modules.builtin.modinfo"):
	p = src / name
	if p.exists():
		shutil.copy2(p, dst / name)
print(f"packed {len(need)} modules for {src.name}")
PY

cat >"${WORK_DIR}/initramfs/init" <<'INIT'
#!/bin/sh
export PATH=/bin
mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev 2>/dev/null || mount -t tmpfs none /dev
# Do NOT use failing exec-redirect (kills init). Attach serial explicitly.
for c in /dev/ttyS2 /dev/console /dev/ttyS0; do
	if [ -e "$c" ]; then
		exec 0<>"$c" 1>"$c" 2>"$c"
		break
	fi
done
echo
echo "=== Z96A hybrid bring-up initramfs ==="
echo "=== BUILD_ID: __Z96A_BUILD_ID__  TS: __Z96A_BUILD_TS__ ==="
uname -a 2>/dev/null || true
echo "dmesg build tag:"; dmesg 2>/dev/null | grep -F 'z96a-edp-' | head -n 8 || true

echo "--- load display modules ---"
KVER=$(uname -r)
echo "  modules dir: /lib/modules/$KVER"
ls /lib/modules/"$KVER"/kernel/drivers/gpu/drm/rockchip 2>/dev/null || echo "  (rockchipdrm.ko missing from initramfs — reflash new hybrid)"
# Prefer insmod of known paths if modprobe has no dep map in busybox
insmod_one() {
	f="$1"
	[ -f "$f" ] || return 1
	insmod "$f" 2>/dev/null && echo "  insmod $(basename "$f") OK" && return 0
	echo "  insmod $(basename "$f") FAIL"; return 1
}
BASE=/lib/modules/$KVER/kernel
insmod_one "$BASE/drivers/media/cec/core/cec.ko"
insmod_one "$BASE/drivers/gpu/drm/display/drm_display_helper.ko"
insmod_one "$BASE/drivers/gpu/drm/display/drm_dp_aux_bus.ko"
insmod_one "$BASE/drivers/gpu/drm/bridge/analogix/analogix_dp.ko"
insmod_one "$BASE/drivers/gpu/drm/bridge/synopsys/dw-hdmi.ko"
insmod_one "$BASE/drivers/gpu/drm/bridge/synopsys/dw-hdmi-qp.ko"
insmod_one "$BASE/drivers/gpu/drm/bridge/synopsys/dw-dp.ko"
insmod_one "$BASE/drivers/gpu/drm/bridge/synopsys/dw-mipi-dsi.ko"
insmod_one "$BASE/drivers/gpu/drm/bridge/synopsys/dw-mipi-dsi2.ko"
insmod_one "$BASE/drivers/gpu/drm/panel/panel-simple.ko"
insmod_one "$BASE/drivers/video/backlight/pwm_bl.ko"
insmod_one "$BASE/drivers/gpu/drm/rockchip/rockchipdrm.ko"
# Also try modprobe names (if busybox modprobe works)
for m in panel_simple pwm_bl rockchipdrm; do
	modprobe "$m" 2>/dev/null && echo "  modprobe $m OK"
done
sleep 2
echo "--- phy / edp / panel ---"
ls -l /sys/bus/platform/devices/fdcb0000.edp-phy /sys/bus/platform/devices/fdcb0000.syscon 2>/dev/null || echo "  (no edp-phy/syscon @fdcb0000)"
ls -l /sys/devices/platform/fdcb0000.syscon/fdcb0000.edp-phy/driver \
	/sys/bus/platform/devices/fdcb0000.edp-phy/driver 2>/dev/null || echo "  (edp-phy unbound)"
ls -l /sys/bus/platform/devices/fe0c0000.edp 2>/dev/null || echo "  (no fe0c0000.edp device)"
ls -l /sys/bus/platform/devices/fe0c0000.edp/driver 2>/dev/null || echo "  (edp unbound)"
ls -ld /sys/bus/platform/devices/edp-panel /sys/firmware/devicetree/base/edp-panel 2>/dev/null || echo "  (no edp-panel)"
ls /sys/bus/platform/devices/edp-panel/driver 2>/dev/null || echo "  (edp-panel unbound)"
ls /sys/class/phy 2>/dev/null || echo "  (no /sys/class/phy)"
if command -v devmem >/dev/null 2>&1; then
	echo "  edp-phy GRF CON0=$(devmem 0xfdcb0000 32) CON6=$(devmem 0xfdcb0018 32) STATUS0=$(devmem 0xfdcb0030 32) (PLL_RDY=bit0)"
fi
mount -t debugfs none /sys/kernel/debug 2>/dev/null || true
if [ -f /sys/kernel/debug/devices_deferred ]; then
	echo "  devices_deferred:"
	cat /sys/kernel/debug/devices_deferred 2>/dev/null || true
fi
echo "--- gpio / panel_en / backlight force ---"
if [ -f /sys/kernel/debug/gpio ]; then
	echo "  LCD/panel enable lines (want out hi):"
	grep -iE 'vcc3v3-lcd0|panel-en|lcd-vcc' /sys/kernel/debug/gpio 2>/dev/null || true
	grep -A24 'gpiochip0\|fdd60000' /sys/kernel/debug/gpio 2>/dev/null | head -n 28 || true
	grep -A24 'gpiochip3\|fe760000' /sys/kernel/debug/gpio 2>/dev/null | head -n 28 || true
fi
# Raw GPIO0 DR bit23 (PC7) via mmap if busybox has devmem
if command -v devmem >/dev/null 2>&1; then
	drl=$(devmem 0xfdd60000 32 2>/dev/null || true)
	drh=$(devmem 0xfdd60004 32 2>/dev/null || true)
	echo "  GPIO0 DR_L@fdd60000=$drl DR_H@fdd60004=$drh (PC7=DR_H bit7)"
fi
echo "--- drm / backlight / regulators ---"
ls -l /sys/class/drm 2>/dev/null || echo "(no /sys/class/drm)"
ls -l /sys/class/backlight 2>/dev/null || echo "(no backlight)"
# Keep LCD / panel_en regulators on if present
for r in /sys/class/regulator/*; do
	n=$(cat "$r/name" 2>/dev/null) || continue
	case "$n" in *lcd*|*panel*) echo "  regulator $n state=$(cat "$r/state" 2>/dev/null)";; esac
done
# Force PWM backlight on for blank-screen diagnosis (BL is PWM10, not a GPIO)
for bl in /sys/class/backlight/*; do
	[ -d "$bl" ] || continue
	echo 0 >"$bl/bl_power" 2>/dev/null || true
	echo 255 >"$bl/brightness" 2>/dev/null || echo 13 >"$bl/brightness" 2>/dev/null || true
	echo "  forced $bl brightness=$(cat "$bl/brightness" 2>/dev/null) bl_power=$(cat "$bl/bl_power" 2>/dev/null)"
done
echo "--- drm connectors / fb ---"
for c in /sys/class/drm/card0-*; do
	[ -e "$c/status" ] || continue
	echo "  $(basename "$c") status=$(cat "$c/status" 2>/dev/null) enabled=$(cat "$c/enabled" 2>/dev/null)"
	head -n 3 "$c/modes" 2>/dev/null | sed 's/^/    mode: /'
done
ls -l /dev/fb0 /sys/class/graphics/fb0 2>/dev/null || echo "  (no fb0)"
if [ -e /sys/class/graphics/fb0/blank ]; then
	echo 0 >/sys/class/graphics/fb0/blank 2>/dev/null || true
fi
# Bind framebuffer console to fb0 (DRM loaded late as module).
for v in /sys/class/vtconsole/vtcon*; do
	[ -e "$v/name" ] || continue
	if grep -qi frame "$v/name" 2>/dev/null; then
		echo 1 >"$v/bind" 2>/dev/null || true
		echo "  bound $(cat "$v/name" 2>/dev/null)"
	fi
done
# Do not paint bars over fb0 — use fbcon/tty1 for CLI.
# Manual eDP check if needed:  cat /opt/z96a-fb-bars.rgb >/dev/fb0
if [ -e /dev/fb0 ]; then
	echo "  fb0 ready (fbcon/tty1). optional bars: cat /opt/z96a-fb-bars.rgb >/dev/fb0"
fi
if [ -e /dev/tty1 ]; then
	# Clear + banner on eDP console (serial stays primary below).
	printf '\033[H\033[J' >/dev/tty1 2>/dev/null || true
	{
		echo
		echo "=== Z96A eDP console (tty1) ==="
		echo "=== BUILD_ID: __Z96A_BUILD_ID__ ==="
		uname -a 2>/dev/null
		echo "Type here (keyboard). Serial shell remains on ttyS2."
		echo
	} >/dev/tty1 2>/dev/null || true
fi
dmesg 2>/dev/null | grep -iE 'edp|edpphy|naneng|analogix|rockchip-drm|panel|pwm|vop|drm|lcd0|panel_en|deferred|failed to get|no DP phy|link train|Clock recovery|Channel EQ' | tail -n 40

echo "--- block devices ---"
ls -l /sys/block 2>/dev/null || true
for n in /sys/class/block/*; do
	[ -e "$n" ] || continue
	b=${n##*/}
	case "$b" in loop*|ram*) continue ;; esac
	size=$(cat "$n/size" 2>/dev/null || echo '?')
	echo "  $b size_sectors=$size"
done
echo "--- mmc ---"
ls -l /sys/bus/mmc/devices 2>/dev/null || echo "(no mmc devices)"
dmesg 2>/dev/null | grep -iE 'mmc|sdhci|dwmmc|fe310000' | tail -n 20
echo
echo "Shell ready: serial ttyS2 + eDP tty1 (no color bars)."
echo "Diag: ls /sys/class/drm; dmesg | grep -iE 'edp|drm|panel|deferred'"
# Quiet late deferred-probe spam on any leftover console; serial already got the diag dump.
echo 3 >/proc/sys/kernel/printk 2>/dev/null || true
# Interactive shell on eDP; keep serial as controlling shell too.
if [ -e /dev/tty1 ]; then
	if command -v setsid >/dev/null 2>&1; then
		setsid /bin/sh -c 'exec /bin/sh <>/dev/tty1 >&0 2>&0' </dev/null >/dev/null 2>&1 &
	else
		(/bin/sh <>/dev/tty1 >&0 2>&0) </dev/null >/dev/null 2>&1 &
	fi
fi
exec /bin/sh
INIT
chmod 755 "${WORK_DIR}/initramfs/init"
# Stamp build id into init banner (visible before uname)
sed -i "s/__Z96A_BUILD_ID__/${Z96A_BUILD_ID}/g; s/__Z96A_BUILD_TS__/${Z96A_BUILD_TS}/g" \
	"${WORK_DIR}/initramfs/init"
echo "    BUILD_ID ${Z96A_BUILD_ID} TS ${Z96A_BUILD_TS}"

(
	cd "${WORK_DIR}/initramfs"
	find . | cpio -H newc -o --owner=0:0 2>/dev/null | gzip -9 >"${WORK_DIR}/initramfs.cpio.gz"
)

# --- FIT ---
cat >"${WORK_DIR}/fit.its" <<ITS
/dts-v1/;
/ {
	description = "Z96A U-Boot+kernel hybrid bring-up";
	#address-cells = <1>;
	images {
		kernel {
			description = "Linux";
			data = /incbin/("Image");
			type = "kernel";
			arch = "arm64";
			os = "linux";
			compression = "none";
			load = <0x02080000>;
			entry = <0x02080000>;
			hash-1 { algo = "crc32"; };
		};
		fdt {
			description = "rk3568-z96a";
			data = /incbin/("rk3568-z96a.dtb");
			type = "flat_dt";
			arch = "arm64";
			compression = "none";
			hash-1 { algo = "crc32"; };
		};
		ramdisk {
			description = "bring-up initramfs";
			data = /incbin/("initramfs.cpio.gz");
			type = "ramdisk";
			arch = "arm64";
			os = "linux";
			compression = "none";
			hash-1 { algo = "crc32"; };
		};
	};
	configurations {
		default = "conf";
		conf {
			description = "Z96A bring-up";
			kernel = "kernel";
			fdt = "fdt";
			ramdisk = "ramdisk";
		};
	};
};
ITS
(
	cd "${WORK_DIR}"
	mkimage -f fit.its "${OUT_DIR}/z96a-bringup.itb"
)

# --- boot.scr: load FIT from FAT and bootm ---
cat >"${WORK_DIR}/boot.cmd" <<'CMD'
# Z96A hybrid: boot FIT from this FAT partition (no rootfs / no USB)
setenv load_addr "0x09000000"
setenv verbosity "7"
# printk only on serial — eDP tty1 is for the interactive shell (no dmesg spam on panel)
setenv consoleargs "earlycon=uart8250,mmio32,0xfe660000 console=ttyS2,1500000"
setenv bootargs "${consoleargs} consoleblank=0 loglevel=${verbosity} ignore_loglevel video=eDP-1:1920x1080@60 fbcon=map:0"
echo "Z96A hybrid: loading z96a-bringup.itb ..."
load ${devtype} ${devnum}:${distro_bootpart} ${load_addr} z96a-bringup.itb
echo "Z96A hybrid: bootm"
bootm ${load_addr}
CMD
mkimage -C none -A arm64 -T script -d "${WORK_DIR}/boot.cmd" "${WORK_DIR}/boot.scr"

# --- FAT image (≤ BOOTSIZE; no duplicate Image — kernel is in the FIT) ---
FAT_BYTES=$((FAT_SECTORS * SECTOR))
FAT_IMG="${WORK_DIR}/bootfs.img"
rm -f "${FAT_IMG}"
truncate -s "${FAT_BYTES}" "${FAT_IMG}"
mkfs.vfat -n Z96AHYBRID -F 32 "${FAT_IMG}" >/dev/null
mcopy -i "${FAT_IMG}" "${WORK_DIR}/boot.scr" ::boot.scr
mcopy -i "${FAT_IMG}" "${OUT_DIR}/z96a-bringup.itb" ::z96a-bringup.itb
mmd -i "${FAT_IMG}" ::dtb ::dtb/rockchip
mcopy -i "${FAT_IMG}" "${WORK_DIR}/rk3568-z96a.dtb" ::dtb/rockchip/rk3568-z96a.dtb
printf 'fdtfile=rockchip/rk3568-z96a.dtb\nverbosity=7\n' | mcopy -i "${FAT_IMG}" - ::armbianEnv.txt

# --- reference hybrid disk image (for --flash-wipe / empty eMMC only) ---
# Full Armbian-sized boot partition (256 MiB) + small empty rootfs stub in the file.
# --flash never uses this image; safe path writes uboot+64MiB bootfs only.
BOOT_LAST=$((GAP_SECTORS + BOOT_PART_SECTORS - 1))
ROOTFS_STUB_SECTORS=8192 # 4 MiB placeholder so GPT part2 is valid in-file
TOTAL_SECTORS=$((ROOTFS_START_SECTOR + ROOTFS_STUB_SECTORS + 64))
ROOTFS_LAST=$((ROOTFS_START_SECTOR + ROOTFS_STUB_SECTORS - 1))
HYBRID_IMG="${OUT_DIR}/z96a-uboot-kernel-hybrid.img"
rm -f "${HYBRID_IMG}"
truncate -s $((TOTAL_SECTORS * SECTOR)) "${HYBRID_IMG}"

sgdisk --clear \
	--new=1:${GAP_SECTORS}:${BOOT_LAST} \
	--typecode=1:0700 \
	--change-name=1:boot \
	--attributes=1:set:2 \
	--new=2:${ROOTFS_START_SECTOR}:${ROOTFS_LAST} \
	--typecode=2:8300 \
	--change-name=2:rootfs \
	"${HYBRID_IMG}"

dd if="${UBOOT_BIN}" of="${HYBRID_IMG}" bs=512 seek=${UBOOT_SEEK} conv=notrunc status=none
dd if="${FAT_IMG}" of="${HYBRID_IMG}" bs=512 seek=${GAP_SECTORS} conv=notrunc status=none

cp -f "${FAT_IMG}" "${OUT_DIR}/z96a-bootfs.img"
cp -f "${UBOOT_BIN}" "${OUT_DIR}/u-boot-rockchip.bin"
cp -f "${WORK_DIR}/boot.cmd" "${OUT_DIR}/boot.cmd"
cp -f "${WORK_DIR}/boot.scr" "${OUT_DIR}/boot.scr"

UBOOT_END_SECTOR=$((UBOOT_SEEK + ($(stat -c%s "${UBOOT_BIN}") + SECTOR - 1) / SECTOR))
BOOTFS_END_SECTOR=$((GAP_SECTORS + FAT_SECTORS))

cat >"${OUT_DIR}/FLASH.txt" <<EOF
Z96A hybrid flash (Maskrom) — BRANCH=${BRANCH}

Safe ranges (do NOT touch rootfs @ sector ${ROOTFS_START_SECTOR} / $((ROOTFS_START_SECTOR * SECTOR / 1024 / 1024)) MiB):
  U-Boot : sector ${UBOOT_SEEK} .. ~${UBOOT_END_SECTOR}  (file u-boot-rockchip.bin)
  bootfs : sector ${GAP_SECTORS} .. ${BOOTFS_END_SECTOR}  (${FAT_MIB} MiB FAT, inside ${BOOT_PART_MIB} MiB boot)

DEFAULT — keep GPT + rootfs, rewrite U-Boot + boot FAT only:
  cd ${REPO_ROOT}
  ./userpatches/tools/z96a-pack-hybrid.sh ${BRANCH} --flash
  # manual:
  sudo upgrade_tool LD
  sudo upgrade_tool db ${SPL_LOADER}
  sudo upgrade_tool wl ${UBOOT_SEEK} ${OUT_DIR}/u-boot-rockchip.bin
  sudo upgrade_tool wl ${GAP_SECTORS} ${OUT_DIR}/z96a-bootfs.img
  sudo upgrade_tool rd

U-Boot only:
  ./userpatches/tools/z96a-pack-hybrid.sh ${BRANCH} --flash-uboot

DESTRUCTIVE (empty eMMC / accept GPT rewrite) — avoid on installed systems:
  ./userpatches/tools/z96a-pack-hybrid.sh ${BRANCH} --flash-wipe
  # sudo upgrade_tool wl 0 ${HYBRID_IMG}

Expect serial: boot.scr -> bootm FIT -> "Z96A hybrid bring-up initramfs" shell.
uname -r should show *-${BRANCH}-rockchip64

NOTE: Images built before BOOTSIZE=256 had rootfs at 16 MiB. Those need a full
Armbian re-image (fat boot + rootfs) before --flash is rootfs-safe.

--- USB bring-up (recommended: zero eMMC rootfs risk) ---
U-Boot already has boot target "usb" (after mmc0). Same FAT payload:
  sudo dd if=${OUT_DIR}/z96a-bootfs.img of=/dev/sdX bs=4M status=progress conv=fsync
  # or: mkfs.vfat /dev/sdX1 && cp boot.scr z96a-bringup.itb /mnt/

If eMMC still has a boot.scr, it wins first. Prefer USB once:
  # at U-Boot prompt:
  usb start
  setenv boot_targets usb
  boot
  # or: bootflow scan -lb
EOF

{
	echo "artifacts:"
	ls -lh "${OUT_DIR}/z96a-uboot-kernel-hybrid.img" "${OUT_DIR}/z96a-bootfs.img" \
		"${OUT_DIR}/z96a-bringup.itb" "${OUT_DIR}/u-boot-rockchip.bin"
	echo
	cat "${OUT_DIR}/FLASH.txt"
} | tee "${OUT_DIR}/README.txt"

flash_db() {
	command -v upgrade_tool >/dev/null || die "upgrade_tool not in PATH"
	[[ -f "${SPL_LOADER}" ]] || die "SPL loader missing: ${SPL_LOADER}"
	sudo upgrade_tool LD
	sudo upgrade_tool db "${SPL_LOADER}"
}

if [[ -n "${FLASH_MODE}" ]]; then
	case "${FLASH_MODE}" in
		flash)
			echo "==> SAFE flash: U-Boot @${UBOOT_SEEK} + bootfs @${GAP_SECTORS} (no GPT, stop before rootfs @${ROOTFS_START_SECTOR})"
			flash_db
			sudo upgrade_tool wl "${UBOOT_SEEK}" "${OUT_DIR}/u-boot-rockchip.bin"
			sudo upgrade_tool wl "${GAP_SECTORS}" "${OUT_DIR}/z96a-bootfs.img"
			sudo upgrade_tool rd
			;;
		flash-uboot)
			echo "==> U-Boot only @${UBOOT_SEEK} (ends ~sector ${UBOOT_END_SECTOR}, before 16 MiB)"
			flash_db
			sudo upgrade_tool wl "${UBOOT_SEEK}" "${OUT_DIR}/u-boot-rockchip.bin"
			sudo upgrade_tool rd
			;;
		flash-wipe)
			echo "==> WIPE flash: wl 0 hybrid (rewrites GPT). Ctrl-C within 3s to abort..."
			sleep 3
			flash_db
			sudo upgrade_tool wl 0 "${HYBRID_IMG}"
			sudo upgrade_tool rd
			;;
	esac
	echo "==> done; watch serial"
fi
