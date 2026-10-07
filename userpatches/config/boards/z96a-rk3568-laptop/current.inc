# Z96A — BRANCH=current only (rockchip64 mainline 6.18)
# Shared hooks → common/
# MPP / bat-adc / rkmpp Chromium hooks live in edge.inc (gated for current|edge).
# Always sourced; only post_family_config_branch_current__* runs on current.
#

function post_family_config_branch_current__z96a_mainline_kernel() {
	display_alert "${BOARD}" "BRANCH=current: LINUXFAMILY→rockchip64, patches archive/rockchip64-6.18" "info"
	# Userpatches: kernel/archive/rockchip64-6.18/{dt/,zz-z96a-*.patch};
	# bat-adc-src + mpp-src are symlinks → rockchip64-7.2 (shared IRQ DC-det driver).
	# Config:     userpatches/config/kernel/linux-rockchip64-current.config
	declare -g BOOT_FDT_FILE="${BOOT_FDT_FILE:-rockchip/rk3568-z96a.dtb}"
}
