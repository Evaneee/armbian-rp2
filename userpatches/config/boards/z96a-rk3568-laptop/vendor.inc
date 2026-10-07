# Z96A — BRANCH=vendor only (rk35xx 6.1 kernel)
# Shared hooks → common/（U-Boot / GNOME / BT / sound）
# DC jack: DTS charger dc_det_gpio GPIO3_PA5 (rk817 charger IRQ, 10ms).
# BT: bluetooth-platdata + overlay bluetooth-rtl8821cs.service (rtk_hciattach).
# rkmpp/Chromium/UPower: pre_customize_image__z96a_mpp_udev in edge.inc (gated vendor too).
# Always sourced; only post_family_config_branch_vendor__* runs on vendor.
#

function post_family_config_branch_vendor__z96a_vendor_kernel() {
	display_alert "${BOARD}" "BRANCH=vendor: rk35xx 6.1 + userpatches/kernel/rk35xx-vendor-6.1" "info"
	# Userpatches: kernel/rk35xx-vendor-6.1/{dt/rk3568-z96a.dts,z96a-*.patch}
	declare -g BOOT_FDT_FILE="${BOOT_FDT_FILE:-rockchip/rk3568-z96a.dtb}"
}
