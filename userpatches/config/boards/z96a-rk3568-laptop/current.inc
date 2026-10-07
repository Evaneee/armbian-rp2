# Z96A — BRANCH=current only (rockchip64 mainline 6.18)
# Shared hooks → common/
# MPP / bat-adc / rkmpp Chromium hooks live in edge.inc (gated for current|edge).
# Always sourced; only post_family_config_branch_current__* runs on current.
#

function post_family_config_branch_current__z96a_mainline_kernel() {
	# Proprietary package names: linux-{image,dtb,headers}-current-z96a (not rockchip64).
	# Keep patches/config on rockchip64-* so we do not move the archive tree.
	display_alert "${BOARD}" "BRANCH=current: LINUXFAMILY→z96a (pkgs), patches archive/rockchip64-6.18" "info"
	declare -g LINUXFAMILY=z96a
	declare -g LINUXCONFIG=linux-rockchip64-current
	declare -g KERNEL_PATCH_ARCHIVE_BASE=rockchip64
	declare -g KERNELPATCHDIR=archive/rockchip64-6.18
	# Userpatches: kernel/archive/rockchip64-6.18/{dt/,zz-z96a-*.patch};
	# bat-adc-src + mpp-src are symlinks → rockchip64-7.2 (shared IRQ DC-det driver).
	# Config:     userpatches/config/kernel/linux-rockchip64-current.config
	declare -g BOOT_FDT_FILE="${BOOT_FDT_FILE:-rockchip/rk3568-z96a.dtb}"
}
