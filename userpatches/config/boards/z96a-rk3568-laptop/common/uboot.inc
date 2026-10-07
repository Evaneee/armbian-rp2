# Z96A — U-Boot (mainline v2026.07), shared by current + vendor
# Sourced from common/common.inc
#
#

function post_family_config__z96a_use_mainline_uboot() {
	display_alert "${BOARD}" "mainline U-Boot v2026.07 + TF-A v2.14 (SCMI) + DDR v1.13 + RK817 DTS" "info"

	# Keep a dedicated FAT boot partition (board.conf BOOTFS_TYPE=fat).
	# Hybrid Maskrom updates only live here so rootfs starting after boot is never rewritten.
	# Do NOT unset BOOTFS_TYPE — single-partition images put rootfs at 16MiB and hybrid stomps it.
	declare -g BOOTSIZE=256 # MiB; hybrid bootfs must stay ≤ this
	declare -g BOOTCONFIG="rock-3a-rk3568_defconfig"
	declare -g BOOTDELAY=1
	declare -g BOOTSOURCE="https://github.com/u-boot/u-boot.git"
	declare -g BOOTBRANCH="tag:v2026.07"
	# Shared Armbian v2026.07 patches + empty board dir under userpatches
	declare -g BOOTPATCHDIR="v2026.07 v2026.07/board_${BOARD}"
	declare -g BOOTDIR="u-boot-${BOARD}"
	declare -g BOOT_SOC="rk3568"
	# Critical: jammy-proven DDR firmware (not family default v1.21)
	declare -g DDR_BLOB="rk35/rk3568_ddr_1560MHz_v1.13.bin"
	# Do NOT pack OP-TEE/BL32. Working Jammy FIT has ATF+U-Boot only (no tee).
	# Packing BL32 made kernel reach initramfs then hard-reset (secure DRAM).
	declare -g BOOTSCRIPT="boot-z96a.cmd:boot.cmd"

	# TF-A ≥ v2.14 adds rk3568 SCMI (CPU/GPU clocks). Family default v2.13 has none
	# → Linux arm-scmi times out, Panfrost and cpufreq stay deferred.
	# prepare_boot_configuration() hard-sets v2.13 before this hook; override here.
	declare -g ATFBRANCH="tag:v2.14.0"
	declare -g ATFPATCHDIR="atf-rockchip64/v2.14"

	# Open BL31 (Trusted Firmware-A). BOOT_SCENARIO=binman-atf-mainline builds bl31.elf.
	# Keep Rockchip DDR TPL only — still required for LPDDR4X on this board.
	declare -g UBOOT_TARGET_MAP="BL31=bl31.elf ROCKCHIP_TPL=${RKBIN_DIR}/${DDR_BLOB};;u-boot-rockchip.bin"
	unset uboot_custom_postprocess write_uboot_platform write_uboot_platform_mtd

	function write_uboot_platform() {
		dd "if=$1/u-boot-rockchip.bin" "of=$2" bs=32k seek=1 conv=notrunc status=none
	}
}

# Belt-and-suspenders: ATFSOURCEDIR is computed after late_family_config.
function late_family_config__z96a_atf_v214_scmi() {
	declare -g ATFBRANCH="tag:v2.14.0"
	declare -g ATFPATCHDIR="atf-rockchip64/v2.14"
	display_alert "${BOARD}" "ATF forced ${ATFBRANCH} (rk3568 SCMI) patchdir=${ATFPATCHDIR}" "info"
}



# Prefer eMMC only in U-Boot (empty SD / WiFi-SDIO probe prints -110).
function pre_config_uboot_target__z96a_boot_order() {
	# No onboard Ethernet — skip pxe/dhcp boot targets.
	declare -a rockchip_uboot_targets=("mmc0" "usb")
	display_alert "${BOARD}" "U-Boot boot order: ${rockchip_uboot_targets[*]}" "info"
	if [[ -f include/configs/rockchip-common.h ]]; then
		sed -i -e "s/#define BOOT_TARGETS.*/#define BOOT_TARGETS \"${rockchip_uboot_targets[*]}\"/" include/configs/rockchip-common.h
	fi
}



# Import Radxa next-dev DRM (VOP2 + Analogix eDP) into mainline v2026.07 tree.
function pre_config_uboot_target__z96a_import_radxa_drm() {
	local src="${USERPATCHES_PATH}/u-boot/v2026.07/board_${BOARD}"
	[[ -d "${src}/drm" ]] || exit_with_error "Missing ${src}/drm (Radxa DRM userpatch)"

	display_alert "${BOARD}" "importing Radxa DRM eDP stack into drivers/video/drm" "info"
	rm -rf drivers/video/drm
	mkdir -p drivers/video/drm
	rsync -a --delete \
		--exclude='*.o' --exclude='*.su' --exclude='*.cmd' --exclude='*.a' \
		--exclude='built-in.a' --exclude='modules.order' --exclude='Module.symvers' \
		"${src}/drm/" drivers/video/drm/

	# Headers needed by DRM (common.h shim + helpers).
	# Keep mainline video_bridge.h / i2c.h / edid.h / drm_modes.h / linux/hdmi.h —
	# Radxa copies of those live under drivers/video/drm/include (ccflags-y).
	# Restore mainline edid/i2c if a previous build overwrote them with Radxa copies.
	if [[ -f include/edid.h ]] && grep -q 'drm_modes.h' include/edid.h 2>/dev/null; then
		git show HEAD:include/edid.h > include/edid.h 2>/dev/null || true
	fi
	if [[ -f include/i2c.h ]] && grep -q 'CONFIG_SYS_NUM_I2C_BUSES' include/i2c.h 2>/dev/null; then
		git show HEAD:include/i2c.h > include/i2c.h 2>/dev/null || true
	fi
	rm -rf include/drm include/drm_modes.h include/linux/hdmi.h
	# Global shims only (common.h, asm/arch, boot_rkimg, …). Radxa edid/drm_* live in drm/include.
	rsync -a \
		--exclude='video_bridge.h' --exclude='i2c.h' \
		--exclude='edid.h' --exclude='drm_modes.h' --exclude='linux/hdmi.h' \
		--exclude='drm' --exclude='video_rockchip.h' \
		"${src}/include/" include/
	if [[ ! -f include/video_bridge.h && -f "${src}/include/video_bridge.h" ]]; then
		cp -a "${src}/include/video_bridge.h" include/
	fi

	# Wire into mainline video build
	if [[ -f drivers/video/Makefile ]] && ! grep -q 'CONFIG_DRM_ROCKCHIP' drivers/video/Makefile; then
		echo 'obj-$(CONFIG_DRM_ROCKCHIP) += drm/' >> drivers/video/Makefile
	fi
	if [[ -f drivers/video/Kconfig ]] && ! grep -q 'drivers/video/drm/Kconfig' drivers/video/Kconfig; then
		cat >> drivers/video/Kconfig <<'EOF'

source "drivers/video/drm/Kconfig"
EOF
	fi

	# Weak stubs for Android/resource helpers not present on mainline
	if [[ -f "${src}/stubs/z96a_drm_stubs.c" ]]; then
		cp -a "${src}/stubs/z96a_drm_stubs.c" drivers/video/drm/
		if ! grep -q 'z96a_drm_stubs' drivers/video/drm/Makefile; then
			echo 'obj-y += z96a_drm_stubs.o' >> drivers/video/drm/Makefile
		fi
	fi

	# Adapt Radxa DRM APIs to mainline v2026.07 (node_, clk_set_defaults, …)
	if [[ -x "${src}/adapt-mainline.sh" ]]; then
		display_alert "${BOARD}" "adapting Radxa DRM for mainline v2026.07" "info"
		"${src}/adapt-mainline.sh" "$(pwd)"
	fi

	# Mainline v2026.07 lacks Naneng eDP PHY; Analogix uses generic_phy("dp").
	if [[ -f "${src}/phy/phy-rockchip-naneng-edp.c" ]]; then
		display_alert "${BOARD}" "importing Naneng eDP PHY (rk3568-edp-phy)" "info"
		cp -a "${src}/phy/phy-rockchip-naneng-edp.c" drivers/phy/rockchip/
		if [[ -f drivers/phy/rockchip/Makefile ]] &&
			! grep -q 'PHY_ROCKCHIP_NANENG_EDP' drivers/phy/rockchip/Makefile; then
			echo 'obj-$(CONFIG_PHY_ROCKCHIP_NANENG_EDP) += phy-rockchip-naneng-edp.o' \
				>> drivers/phy/rockchip/Makefile
		fi
		if [[ -f drivers/phy/rockchip/Kconfig ]] &&
			! grep -q 'PHY_ROCKCHIP_NANENG_EDP' drivers/phy/rockchip/Kconfig; then
			cat >> drivers/phy/rockchip/Kconfig <<'EOF'

config PHY_ROCKCHIP_NANENG_EDP
	bool "Support Rockchip Naneng eDP Transmitter PHY"
	depends on ARCH_ROCKCHIP
	select PHY
	help
	  Support for Rockchip eDP Transmitter PHY with Naneng IP block.
EOF
		fi
	fi
}



# Create rk3568-z96a.dts from rock-3a with RK817 PMIC (both legacy and OF_UPSTREAM layouts).
function pre_config_uboot_target__z96a_make_uboot_dts() {
	local src="" dst="" mk=""
	if [[ -f arch/arm/dts/rk3568-rock-3a.dts ]]; then
		src="arch/arm/dts/rk3568-rock-3a.dts"
		dst="arch/arm/dts/rk3568-z96a.dts"
		mk="arch/arm/dts/Makefile"
	elif [[ -f dts/upstream/src/arm64/rockchip/rk3568-rock-3a.dts ]]; then
		src="dts/upstream/src/arm64/rockchip/rk3568-rock-3a.dts"
		dst="dts/upstream/src/arm64/rockchip/rk3568-z96a.dts"
		mk="dts/upstream/src/arm64/rockchip/Makefile"
	else
		exit_with_error "Missing Rock 3A U-Boot DTS (arch/arm/dts or dts/upstream)"
	fi

	display_alert "${BOARD}" "generating ${dst} (RK817) from rock-3a" "info"
	# Upstream rock-3a uses "radxa,rock3a" (no hyphen) and label rk809: — rename carefully.
	sed \
		-e 's/Radxa ROCK 3A/Z96A RK3568 Laptop/g' \
		-e 's/Radxa ROCK3 Model A/Z96A RK3568 Laptop/g' \
		-e 's/"radxa,rock3a"/"z96a,rk3568-laptop"/g' \
		-e 's/"radxa,rock-3a"/"z96a,rk3568-laptop"/g' \
		-e 's/compatible = "rockchip,rk809"/compatible = "rockchip,rk817"/g' \
		-e 's/\brk809:/rk817:/g' \
		-e 's/\&rk809\b/\&rk817/g' \
		"${src}" > "${dst}"

	# Z96A / RK817 tree uses vcc_1v8; rock-3a saradc points at vcca_1v8
	if grep -q 'vcca_1v8' "${dst}" && ! grep -q 'vcca_1v8:' "${dst}"; then
		sed -i 's/vref-supply = <\&vcca_1v8>/vref-supply = <\&vcc_1v8>/g' "${dst}"
	fi

	# Rock 3A uses TCS4525@0x1c; Z96A is SYR827@0x40. Also set init 1.10V so
	# APLL_HZ=1416 MHz (vendor clk-init) has headroom before Linux cpufreq.
	if grep -q 'regulator@1c' "${dst}"; then
		display_alert "${BOARD}" "U-Boot DTS: vdd_cpu SYR827@0x40 init=1.10V" "info"
		python3 - <<PY
from pathlib import Path
p = Path("${dst}")
t = p.read_text()
old = """	vdd_cpu: regulator@1c {
		compatible = "tcs,tcs4525";
		reg = <0x1c>;
		fcs,suspend-voltage-selector = <1>;
		regulator-name = "vdd_cpu";
		regulator-always-on;
		regulator-boot-on;
		regulator-min-microvolt = <800000>;
		regulator-max-microvolt = <1150000>;
		regulator-ramp-delay = <2300>;
		vin-supply = <&vcc5v0_sys>;

		regulator-state-mem {
			regulator-off-in-suspend;
		};
	};"""
new = """	vdd_cpu: regulator@40 {
		compatible = "silergy,syr827";
		reg = <0x40>;
		fcs,suspend-voltage-selector = <1>;
		regulator-name = "vdd_cpu";
		regulator-always-on;
		regulator-boot-on;
		regulator-min-microvolt = <712500>;
		regulator-max-microvolt = <1390000>;
		regulator-init-microvolt = <1100000>;
		regulator-ramp-delay = <2300>;
		vin-supply = <&vcc5v0_sys>;

		regulator-state-mem {
			regulator-off-in-suspend;
		};
	};"""
if old not in t:
	raise SystemExit("vdd_cpu@1c block not found for SYR827 rewrite")
p.write_text(t.replace(old, new, 1))
PY
	fi

	# Ensure SoC clock/power/irq bindings resolve for eDP SoC nodes in extras.
	if ! grep -q 'dt-bindings/interrupt-controller/arm-gic.h' "${dst}"; then
		sed -i '/#include <dt-bindings\/pinctrl\/rockchip.h>/a\
#include <dt-bindings/interrupt-controller/arm-gic.h>\
#include <dt-bindings/clock/rk3568-cru.h>\
#include <dt-bindings/power/rk3568-power.h>
' "${dst}"
	fi

	# OF_UPSTREAM auto-includes arch/arm/dts/<basename>-u-boot.dtsi
	# (rock-3a dtsi may list hs400 props; leave CONFIG_MMC_HS400_SUPPORT off — probe failed, see HS400.md)
	if [[ -f arch/arm/dts/rk3568-rock-3a-u-boot.dtsi ]]; then
		cp -f arch/arm/dts/rk3568-rock-3a-u-boot.dtsi arch/arm/dts/rk3568-z96a-u-boot.dtsi
		local extras_src="${USERPATCHES_PATH}/u-boot/v2026.07/board_${BOARD}/rk3568-z96a-uboot-extras.dtsi"
		if [[ -f "${extras_src}" ]]; then
			display_alert "${BOARD}" "U-Boot DTS: keyboard HID + eDP extras" "info"
			{
				echo ''
				echo '/* Keep saradc enabled in U-Boot so we can print ADC. */'
				echo "#include \"${extras_src}\""
			} >> arch/arm/dts/rk3568-z96a-u-boot.dtsi
			# Absolute include may fail in dtc; copy beside dtsi and use relative name.
			cp -f "${extras_src}" arch/arm/dts/rk3568-z96a-uboot-extras.dtsi
			sed -i 's|#include ".*/rk3568-z96a-uboot-extras.dtsi"|#include "rk3568-z96a-uboot-extras.dtsi"|' \
				arch/arm/dts/rk3568-z96a-u-boot.dtsi
		fi
	fi

	# OF_UPSTREAM builds DTB from the .dts path alone (no Makefile list required).
	# Legacy arch/arm/dts still needs a Makefile entry when used.
	if [[ -n "${mk}" && -f "${mk}" ]] && ! grep -q 'rk3568-z96a' "${mk}"; then
		if grep -q 'rk3568-rock-3a' "${mk}"; then
			sed -i 's/rk3568-rock-3a\.dtb/rk3568-rock-3a.dtb rk3568-z96a.dtb/' "${mk}"
		else
			echo 'dtb-$(CONFIG_ARCH_ROCKCHIP) += rk3568-z96a.dtb' >> "${mk}"
		fi
	fi
}



# OF_UPSTREAM + binman FIT need matching DEFAULT_DEVICE_TREE / OF_LIST / SPL_OF_LIST.
function post_config_uboot_target__z96a_select_dtb() {
	# Early boot CPU: vendor clk-init is 1416 MHz; mainline APLL_HZ defaults to 816.
	# Board patch 0001 also does this; sed here is belt-and-suspenders.
	if [[ -f arch/arm/include/asm/arch-rockchip/cru_rk3568.h ]]; then
		display_alert "${BOARD}" "U-Boot: APLL_HZ=1416 MHz (early boot boost)" "info"
		sed -i 's|#define APLL_HZ\t\t(816 \* MHz)|#define APLL_HZ\t\t(1416 * MHz)|' \
			arch/arm/include/asm/arch-rockchip/cru_rk3568.h
		grep -n 'APLL_HZ' arch/arm/include/asm/arch-rockchip/cru_rk3568.h | head -3
	fi

	local dt="rockchip/rk3568-z96a"
	display_alert "${BOARD}" "U-Boot DT/OF_LIST=${dt}" "info"
	for key in CONFIG_DEFAULT_DEVICE_TREE CONFIG_OF_LIST CONFIG_SPL_OF_LIST; do
		if grep -q "^${key}=" .config; then
			sed -i "s|^${key}=.*|${key}=\"${dt}\"|" .config
		else
			echo "${key}=\"${dt}\"" >> .config
		fi
	done
	if grep -q '^CONFIG_DEFAULT_FDT_FILE=' .config; then
		sed -i "s|^CONFIG_DEFAULT_FDT_FILE=.*|CONFIG_DEFAULT_FDT_FILE=\"${dt}.dtb\"|" .config
	fi

	# Skip EFI bootmgr (fails without ESP and probes empty MMC)
	for key in CONFIG_BOOTMETH_EFI_MGR CONFIG_EFI_BOOTMGR; do
		if grep -q "^${key}=y" .config; then
			sed -i "s|^${key}=y|# ${key} is not set|" .config
		fi
	done

	# Quiet Armbian 1001-fdt_fixup_ethernet-add-logs.patch spam
	if [[ -f boot/fdt_support.c ]]; then
		sed -i 's/log_info("\[fdt_fixup_ethernet\]/log_debug("[fdt_fixup_ethernet]/g' boot/fdt_support.c
	fi
	if [[ -f boot/image-fdt.c ]]; then
		sed -i 's/log_info("\[image_setup_libfdt\]/log_debug("[image_setup_libfdt]/g' boot/image-fdt.c
	fi

	# Enable interactive ADC read: adc list / adc single <dev> <channel>
	if grep -q '^# CONFIG_CMD_ADC is not set' .config; then
		sed -i 's|^# CONFIG_CMD_ADC is not set|CONFIG_CMD_ADC=y|' .config
	elif ! grep -q '^CONFIG_CMD_ADC=y' .config; then
		echo 'CONFIG_CMD_ADC=y' >> .config
	fi

	# SYR827@0x40 (vdd_cpu) — needed so regulator-init-microvolt=1.10V applies
	# before / with APLL_HZ=1416 early boost.
	display_alert "${BOARD}" "U-Boot: DM_REGULATOR_FAN53555 (SYR827)" "info"
	# DM_PMIC_FAN53555 selects DM_REGULATOR_FAN53555
	for key in CONFIG_DM_REGULATOR CONFIG_DM_PMIC_FAN53555; do
		if grep -q "^# ${key} is not set" .config; then
			sed -i "s|^# ${key} is not set|${key}=y|" .config
		elif ! grep -q "^${key}=y" .config; then
			echo "${key}=y" >> .config
		fi
	done

	# Shut down again if PMIC ON_SOURCE was charger plug-in (not power key).
	# Without this, "poweroff then insert AC" always boots — laptop-unfriendly.
	if grep -q '^# CONFIG_ROCKCHIP_RK8XX_DISABLE_BOOT_ON_POWERON is not set' .config; then
		sed -i 's|^# CONFIG_ROCKCHIP_RK8XX_DISABLE_BOOT_ON_POWERON is not set|CONFIG_ROCKCHIP_RK8XX_DISABLE_BOOT_ON_POWERON=y|' .config
	elif ! grep -q '^CONFIG_ROCKCHIP_RK8XX_DISABLE_BOOT_ON_POWERON=y' .config; then
		echo 'CONFIG_ROCKCHIP_RK8XX_DISABLE_BOOT_ON_POWERON=y' >> .config
	fi
	display_alert "${BOARD}" "U-Boot: power off on charger plug-in (no auto-boot)" "info"

	# No RJ45 on Z96A — networking is a Kconfig choice (default NET).
	# Must select NO_NET or olddefconfig restores CONFIG_NET=y.
	display_alert "${BOARD}" "U-Boot: CONFIG_NO_NET (no ethernet)" "info"
	sed -i \
		-e 's|^CONFIG_NET=y|# CONFIG_NET is not set|' \
		-e 's|^CONFIG_NET_LWIP=y|# CONFIG_NET_LWIP is not set|' \
		-e 's|^# CONFIG_NO_NET is not set|CONFIG_NO_NET=y|' \
		.config
	if ! grep -q '^CONFIG_NO_NET=y' .config; then
		echo 'CONFIG_NO_NET=y' >> .config
	fi
	for key in CONFIG_CMD_NET CONFIG_CMD_DHCP CONFIG_CMD_PING \
		CONFIG_CMD_TFTPBOOT CONFIG_CMD_DNS CONFIG_CMD_PXE \
		CONFIG_BOOTDEV_ETH CONFIG_BOOTMETH_EXTLINUX_PXE \
		CONFIG_DWC_ETH_QOS CONFIG_DWC_ETH_QOS_ROCKCHIP CONFIG_PHY_REALTEK \
		CONFIG_NETDEVICES CONFIG_DM_ETH CONFIG_DM_ETH_PHY CONFIG_ETH; do
		if grep -q "^${key}=y" .config; then
			sed -i "s|^${key}=y|# ${key} is not set|" .config
		fi
	done

	# USB must start before BOOTDELAY so the laptop keyboard can abort autoboot.
	# No more "USB only after key" — that only works on serial.
	# Keep settle short: old "sleep 3" was the big boot tax; 1s is usually enough for VBUS.
	display_alert "${BOARD}" "U-Boot: usb start then BOOTDELAY (laptop kbd abort)" "info"
	for key in CONFIG_USB_KEYBOARD CONFIG_USE_PREBOOT CONFIG_DM_KEYBOARD CONFIG_CMD_SLEEP CONFIG_CMD_USB; do
		if grep -q "^# ${key} is not set" .config; then
			sed -i "s|^# ${key} is not set|${key}=y|" .config
		elif ! grep -q "^${key}=y" .config; then
			echo "${key}=y" >> .config
		fi
	done
	if grep -q '^CONFIG_PREBOOT=' .config; then
		sed -i 's|^CONFIG_PREBOOT=.*|CONFIG_PREBOOT="sleep 1; usb start"|' .config
	else
		echo 'CONFIG_PREBOOT="sleep 1; usb start"' >> .config
	fi
	# Drop previous abort-only USB hook if present (clean rebuild from git usually).
	if [[ -f common/autoboot.c ]] && grep -q 'z96a_usb_on_key' common/autoboot.c; then
		display_alert "${BOARD}" "U-Boot: remove abort-only USB hook" "info"
		# Not needed on clean tree; keep for safety if incremental.
		true
	fi
	# Restore standard countdown prompt if we rewrote it.
	if [[ -f common/autoboot.c ]] && grep -q 'z96a_usb_hint' common/autoboot.c; then
		sed -i 's|"Hit any key for USB/shell (else skip USB): %d" /\* z96a_usb_hint \*/|"Hit any key to stop autoboot: %d"|' \
			common/autoboot.c
	fi
	# Rockchip DWC3 → xHCI (needed when host*_xhci status=okay).
	for key in CONFIG_USB_XHCI_HCD CONFIG_USB_XHCI_DWC3 CONFIG_USB_DWC3 CONFIG_USB_DWC3_GENERIC; do
		if grep -q "^# ${key} is not set" .config; then
			sed -i "s|^# ${key} is not set|${key}=y|" .config
		elif ! grep -q "^${key}=y" .config; then
			echo "${key}=y" >> .config
		fi
	done

	# Z96A laptop HID keyboard 6080:8060 never returns idle reports — same as
	# Apple/Keychron. Without this quirk usb_kbd aborts:
	#   "Failed to get keyboard state from device 6080:8060"
	#
	# Also: EHCI EVENT_POLL creates/destroys an int queue every tstc() and
	# ignores nonblock → "Timeout poll on interrupt endpoint" spam and no keys.
	# Use persistent INT_QUEUE polling, and still create the queue when the
	# no-idle-report quirk skips the initial blocking get_report.
	if [[ -f common/usb_kbd.c ]] && ! grep -q 'USB_VENDOR_ID_Z96A_KB' common/usb_kbd.c; then
		display_alert "${BOARD}" "U-Boot: usb_kbd quirk for 6080:8060 (no idle report)" "info"
		sed -i '/#define USB_VENDOR_ID_KEYCHRON/a\
\
#define USB_VENDOR_ID_Z96A_KB	0x6080	/* Z96A laptop keyboard/touchpad combo */' \
			common/usb_kbd.c
		sed -i '/case USB_VENDOR_ID_KEYCHRON:/a\
	case USB_VENDOR_ID_Z96A_KB:' common/usb_kbd.c
	fi
	if [[ -f common/usb_kbd.c ]] && ! grep -q 'Z96A_INTQ_AFTER_NO_IDLE_QUIRK' common/usb_kbd.c; then
		display_alert "${BOARD}" "U-Boot: usb_kbd create intq after no-idle quirk" "info"
		python3 - <<'PY'
from pathlib import Path
p = Path("common/usb_kbd.c")
t = p.read_text()
old = (
	"\tif (quirks & USB_HID_QUIRK_POLL_NO_REPORT_IDLE) {\n"
	"\t\tdebug(\"USB KBD: quirk: skip testing device state\\n\");\n"
	"\t\treturn 1;\n"
	"\t}\n"
)
new = (
	"\tif (quirks & USB_HID_QUIRK_POLL_NO_REPORT_IDLE) {\n"
	"\t\tdebug(\"USB KBD: quirk: skip testing device state\\n\");\n"
	"\t\t/* Z96A_INTQ_AFTER_NO_IDLE_QUIRK */\n"
	"#if defined(CONFIG_SYS_USB_EVENT_POLL_VIA_INT_QUEUE)\n"
	"\t\tdata->intq = create_int_queue(dev, data->intpipe, 1,\n"
	"\t\t\t\t      USB_KBD_BOOT_REPORT_SIZE, data->new,\n"
	"\t\t\t\t      data->intinterval);\n"
	"\t\tif (!data->intq)\n"
	"\t\t\treturn 0;\n"
	"#endif\n"
	"\t\treturn 1;\n"
	"\t}\n"
)
if old not in t:
	raise SystemExit("usb_kbd no-idle quirk block not found for INTQ patch")
p.write_text(t.replace(old, new, 1))
PY
	fi
	# Prefer INT_QUEUE over EVENT_POLL (choice). EVENT_POLL creates/destroys
	# an int queue every tstc() and ignores nonblock → Timeout spam.
	display_alert "${BOARD}" "U-Boot: SYS_USB_EVENT_POLL_VIA_INT_QUEUE" "info"
	sed -i \
		-e 's|^CONFIG_SYS_USB_EVENT_POLL=y|# CONFIG_SYS_USB_EVENT_POLL is not set|' \
		-e 's|^# CONFIG_SYS_USB_EVENT_POLL_VIA_INT_QUEUE is not set|CONFIG_SYS_USB_EVENT_POLL_VIA_INT_QUEUE=y|' \
		-e 's|^CONFIG_SYS_USB_EVENT_POLL_VIA_CONTROL_EP=y|# CONFIG_SYS_USB_EVENT_POLL_VIA_CONTROL_EP is not set|' \
		.config
	if ! grep -q '^CONFIG_SYS_USB_EVENT_POLL_VIA_INT_QUEUE=y' .config; then
		echo 'CONFIG_SYS_USB_EVENT_POLL_VIA_INT_QUEUE=y' >> .config
	fi
	# Drop stale EVENT_POLL=y if still present after append.
	sed -i 's|^CONFIG_SYS_USB_EVENT_POLL=y|# CONFIG_SYS_USB_EVENT_POLL is not set|' .config

	# Radxa DRM eDP → vidconsole (do NOT enable legacy CONFIG_DISPLAY_ROCKCHIP_EDP).
	display_alert "${BOARD}" "U-Boot: DRM_ROCKCHIP eDP vidconsole" "info"
	# Mainline v2026.07: VIDEO (not DM_VIDEO). Force DRM symbols after Kconfig is sourced.
	# NOTE: do not enable CONFIG_SIMPLE_PANEL — mainline panel.h redefines
	# struct drm_display_mode and clashes with Radxa edid/drm_modes headers.
	for key in CONFIG_VIDEO CONFIG_VIDEO_BRIDGE CONFIG_PHY \
		CONFIG_PHY_ROCKCHIP_NANENG_EDP \
		CONFIG_DRM_ROCKCHIP CONFIG_DRM_ROCKCHIP_ANALOGIX_DP \
		CONFIG_DRM_ROCKCHIP_INNO_VIDEO_COMBO_PHY CONFIG_DRM_ROCKCHIP_PANEL \
		CONFIG_DRM_ROCKCHIP_VIDEO_FRAMEBUFFER \
		CONFIG_DRM_DP_HELPER CONFIG_DM_PWM CONFIG_PWM_ROCKCHIP \
		CONFIG_BACKLIGHT_PWM CONFIG_PANEL CONFIG_DISPLAY; do
		if grep -q "^# ${key} is not set" .config; then
			sed -i "s|^# ${key} is not set|${key}=y|" .config
		elif ! grep -q "^${key}=" .config; then
			echo "${key}=y" >> .config
		fi
	done
	if grep -q '^CONFIG_DRM_MEM_RESERVED_SIZE_MBYTES=' .config; then
		sed -i 's|^CONFIG_DRM_MEM_RESERVED_SIZE_MBYTES=.*|CONFIG_DRM_MEM_RESERVED_SIZE_MBYTES=16|' .config
	else
		echo 'CONFIG_DRM_MEM_RESERVED_SIZE_MBYTES=16' >> .config
	fi
	# Fallback pre-reloc FB reserve (~24MiB = 1920x1080x4 + 16MiB pool) if
	# display-subsystem is not bound early enough for video_reserve().
	if grep -q '^CONFIG_VIDEO_PCI_DEFAULT_FB_SIZE=' .config; then
		sed -i 's|^CONFIG_VIDEO_PCI_DEFAULT_FB_SIZE=.*|CONFIG_VIDEO_PCI_DEFAULT_FB_SIZE=0x1800000|' .config
	else
		echo 'CONFIG_VIDEO_PCI_DEFAULT_FB_SIZE=0x1800000' >> .config
	fi
	# Route console to eDP vidconsole + keep UART; USB HID on stdin after PREBOOT.
	# without CONSOLE_IS_IN_ENV + CONSOLE_MUX, U-Boot stays on serial only even
	# when ROCKCHIP_DEVICE_SETTINGS lists vidconsole.
	display_alert "${BOARD}" "U-Boot: CONSOLE_MUX serial+vidconsole+usbkbd" "info"
	for key in CONFIG_SYS_CONSOLE_IS_IN_ENV CONFIG_CONSOLE_MUX; do
		if grep -q "^# ${key} is not set" .config; then
			sed -i "s|^# ${key} is not set|${key}=y|" .config
		elif ! grep -q "^${key}=y" .config; then
			echo "${key}=y" >> .config
		fi
	done
	if [[ -f include/configs/evb_rk3568.h ]]; then
		cat > include/configs/evb_rk3568.h <<'EOF'
/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2021 Rockchip Electronics Co., Ltd
 * Z96A: serial + eDP vidconsole + USB keyboard.
 */

#ifndef __EVB_RK3568_H
#define __EVB_RK3568_H

#define ROCKCHIP_DEVICE_SETTINGS \
			"stdin=serial,usbkbd\0" \
			"stdout=serial,vidconsole\0" \
			"stderr=serial,vidconsole\0"

#include <configs/rk3568_common.h>

#endif
EOF
	fi
	for key in CONFIG_VIDEO_ROCKCHIP CONFIG_DISPLAY_ROCKCHIP_EDP CONFIG_DISPLAY_ROCKCHIP_HDMI \
		CONFIG_SIMPLE_PANEL; do
		if grep -q "^${key}=y" .config; then
			sed -i "s|^${key}=y|# ${key} is not set|" .config
		fi
	done

	# Default 8x16 bitmap — matches typical Linux FB/VT console glyph size.
	display_alert "${BOARD}" "U-Boot: VIDEO_FONT_8X16 (default console)" "info"
	sed -i -E \
		'/^#? ?CONFIG_VIDEO_FONT_8X16/d; /^CONFIG_VIDEO_FONT_16X32=/d; /^# CONFIG_VIDEO_FONT_16X32 /d; /^CONFIG_VIDEO_FONT_SUN12X22=/d; /^# CONFIG_VIDEO_FONT_SUN12X22 /d' \
		.config
	printf '%s\n' \
		'CONFIG_VIDEO_FONT_8X16=y' \
		'# CONFIG_VIDEO_FONT_SUN12X22 is not set' \
		'# CONFIG_VIDEO_FONT_16X32 is not set' >> .config

	# Do NOT enable CONFIG_ARMV8_SWITCH_TO_EL1: vendor kernel then reports
	# "CPUs started in inconsistent modes" (CPU0 EL1 vs secondaries EL2).
	# Stay in EL2 for all CPUs (matches working Jammy/ATF handoff).
	if grep -q '^CONFIG_ARMV8_SWITCH_TO_EL1=y' .config; then
		sed -i 's|^CONFIG_ARMV8_SWITCH_TO_EL1=y|# CONFIG_ARMV8_SWITCH_TO_EL1 is not set|' .config
		display_alert "${BOARD}" "disable ARMV8_SWITCH_TO_EL1 (consistent EL2 for SMP)" "info"
	fi

	# Boot path = Rock 3A / standard rk35xx bootflow:
	#   bootstd scans mmc0 → bootmeth script → FAT boot.scr (from BOOTSCRIPT=boot-z96a.cmd).
	# Do NOT replace CONFIG_BOOTCOMMAND / disable BOOTMETH_SCRIPT — that was only a
	# temporary rescue when eMMC had a broken boot.scr + OP-TEE. Production image
	# ships a correct boot.scr (load_addr=0x9000000, uInitrd required).
	# Keep EFI bootmgr off (above); leave BOOTSTD_* and BOOTMETH_SCRIPT as defconfig.
	display_alert "${BOARD}" "bootflow + boot.scr (boot-z96a.cmd), same pattern as Rock 3A" "info"

	# Never enter download mode via SARADC (Z96A: ch1=battery was misread as Loader key →
	# download loop; Maskrom return path also unsafe). Silent stub — ADC printf was debug-only.
	if [[ -f arch/arm/mach-rockchip/boot_mode.c ]]; then
		display_alert "${BOARD}" "disable download-key (silent stub)" "info"
		# Ensure setup_boot_mode still calls the check (so our hook runs).
		sed -i 's|^\t// z96a: rockchip_dnl_mode_check();$|\trockchip_dnl_mode_check();|' \
			arch/arm/mach-rockchip/boot_mode.c
		python3 - <<'PY'
from pathlib import Path
import re
p = Path("arch/arm/mach-rockchip/boot_mode.c")
t = p.read_text()
# Drop older z96a stubs (dnl_disabled / adc_debug with printf).
t = re.sub(
    r"\n/\* z96a_dnl_disabled: Z96A has no safe Maskrom return path.*?\n}\n?",
    "\n",
    t,
    count=1,
    flags=re.S,
)
# Replace any previous weak override (incl. ADC debug printf) with silent stub.
stub = """__weak int rockchip_dnl_key_pressed(void)
{
	return 0; /* z96a_dnl_off: never enter download mode */
}"""
t2, n = re.subn(
    r"__weak int rockchip_dnl_key_pressed\(void\)\s*\{.*?\n\}",
    lambda _m: stub,
    t,
    count=1,
    flags=re.S,
)
if n != 1:
    raise SystemExit(f"failed to patch rockchip_dnl_key_pressed (n={n})")
p.write_text(t2)
PY
	fi
}
