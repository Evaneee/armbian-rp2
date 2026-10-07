# Z96A — BRANCH=edge / BRANCH=current (rockchip64 mainline)
# Shared U-Boot / GNOME / Bluetooth / sound → common/
# Always sourced; hooks gate on BRANCH=edge|current.
#

function z96a_mainline_archive_dir() {
	case "${BRANCH}" in
		edge) echo "${SRC}/userpatches/kernel/archive/rockchip64-7.2" ;;
		current) echo "${SRC}/userpatches/kernel/archive/rockchip64-6.18" ;;
		*) echo "" ;;
	esac
}

function post_family_config_branch_edge__z96a_mainline_kernel() {
	# Proprietary package names: linux-{image,dtb,headers}-edge-z96a (not rockchip64 —
	# so apt.armbian.com cannot overwrite our kernel on apt upgrade).
	# Keep patches/config on rockchip64-* so we do not move the archive tree.
	display_alert "${BOARD}" "BRANCH=edge: LINUXFAMILY→z96a (pkgs), patches archive/rockchip64-7.2" "info"
	declare -g LINUXFAMILY=z96a
	declare -g LINUXCONFIG=linux-rockchip64-edge
	declare -g KERNEL_PATCH_ARCHIVE_BASE=rockchip64
	declare -g KERNELPATCHDIR=archive/rockchip64-7.2
	# Userpatches: kernel/archive/rockchip64-7.2/{dt/rk3568-z96a.dts,zz-z96a-*.patch,mpp-src/,bat-adc-src/}
	# Config:     userpatches/config/kernel/linux-rockchip64-edge.config
	# U-Boot:     common/uboot.inc (same mainline v2026.07 as current/vendor)
	declare -g BOOT_FDT_FILE="${BOOT_FDT_FILE:-rockchip/rk3568-z96a.dtb}"
}

# Patching does `git reset` + drops untracked files, so board sources must be copied
# AFTER patching and BEFORE olddefconfig. custom_kernel_config runs in that window.
function custom_kernel_config__z96a_bat_adc() {
	if [[ "${BRANCH}" != "edge" && "${BRANCH}" != "current" ]]; then
		return 0
	fi

	declare arch_dir
	arch_dir="$(z96a_mainline_archive_dir)"
	declare bat_src="${arch_dir}/bat-adc-src/z96a_bat_adc.c"
	declare bat_ver="missing"
	if [[ -f "${bat_src}" ]]; then
		bat_ver="$(cksum "${bat_src}" 2>/dev/null | awk '{print $1}')"
	fi
	# shellcheck disable=SC2034
	kernel_config_modifying_hashes+=("z96a-bat-adc=${bat_ver}")

	if [[ ! -f .config ]]; then
		return 0
	fi
	if [[ ! -f "${bat_src}" ]]; then
		display_alert "${BOARD}" "Z96A bat-adc source missing: ${bat_src}" "wrn"
		return 0
	fi

	display_alert "${BOARD}" "Installing Z96A SARADC battery gauge into ${BRANCH} tree" "info"
	run_host_command_logged cp -a "${bat_src}" "drivers/power/supply/z96a_bat_adc.c"
	if ! grep -q 'CONFIG_BATTERY_Z96A_ADC' drivers/power/supply/Makefile; then
		echo 'obj-$(CONFIG_BATTERY_Z96A_ADC) += z96a_bat_adc.o' >> drivers/power/supply/Makefile
	fi
	if ! grep -q 'config BATTERY_Z96A_ADC' drivers/power/supply/Kconfig; then
		cat >> drivers/power/supply/Kconfig <<- EOF

			config BATTERY_Z96A_ADC
				tristate "Z96A SARADC battery gauge (vendor ADC path)"
				depends on IIO && OF
				help
				  2S pack voltage via Rockchip SARADC channel 1, matching
				  vendor rk817,battery ADC scale (ref 1800 / res 1024 / ratio 4900).
		EOF
	fi
	kernel_config_set_m CONFIG_BATTERY_Z96A_ADC
}

# Naneng eDP PHY: .c arrives via zz-z96a-regen-phy-*.patch; Kbuild via
# zz-z96a-phy-naneng-edp-kconfig.patch. Force =y so olddefconfig cannot drop it.
function custom_kernel_config__z96a_naneng_edp() {
	if [[ "${BRANCH}" != "edge" && "${BRANCH}" != "current" ]]; then
		return 0
	fi
	# shellcheck disable=SC2034
	kernel_config_modifying_hashes+=("z96a-naneng-edp=y")
	if [[ ! -f .config ]]; then
		return 0
	fi
	if [[ ! -f drivers/phy/rockchip/phy-rockchip-naneng-edp.c ]]; then
		display_alert "${BOARD}" "naneng-edp .c missing after patches — eDP will stay blank" "wrn"
		return 0
	fi
	if [[ -f drivers/phy/rockchip/Makefile ]] &&
		! grep -q 'PHY_ROCKCHIP_NANENG_EDP' drivers/phy/rockchip/Makefile; then
		display_alert "${BOARD}" "wiring Naneng eDP PHY into Makefile" "info"
		echo 'obj-$(CONFIG_PHY_ROCKCHIP_NANENG_EDP)	+= phy-rockchip-naneng-edp.o' \
			>> drivers/phy/rockchip/Makefile
	fi
	if [[ -f drivers/phy/rockchip/Kconfig ]] &&
		! grep -q 'config PHY_ROCKCHIP_NANENG_EDP' drivers/phy/rockchip/Kconfig; then
		display_alert "${BOARD}" "wiring Naneng eDP PHY into Kconfig" "info"
		cat >> drivers/phy/rockchip/Kconfig <<- EOF

			config PHY_ROCKCHIP_NANENG_EDP
				tristate "Rockchip Naneng eDP Transmitter PHY driver"
				depends on ARCH_ROCKCHIP && OF
				select GENERIC_PHY
				help
				  Support for Rockchip eDP Transmitter PHY with Naneng IP block.
		EOF
	fi
	kernel_config_set_y CONFIG_PHY_ROCKCHIP_NANENG_EDP
}

function custom_kernel_config__z96a_mpp_mainline() {
	if [[ "${BRANCH}" != "edge" && "${BRANCH}" != "current" ]]; then
		return 0
	fi

	declare arch_dir
	arch_dir="$(z96a_mainline_archive_dir)"
	declare mpp_src="${arch_dir}/mpp-src"
	declare mpp_ver="missing"
	if [[ -f "${mpp_src}/mpp_service.c" ]]; then
		mpp_ver="$(cksum "${mpp_src}/mpp_service.c" "${mpp_src}/Makefile" "${mpp_src}/Kconfig" 2>/dev/null | cksum | awk '{print $1}')"
	fi
	# shellcheck disable=SC2034
	kernel_config_modifying_hashes+=("z96a-mpp-mainline=${mpp_ver}")

	# Hook is invoked twice; only the in-tree pass has .config and a writable worktree.
	if [[ ! -f .config ]]; then
		return 0
	fi
	if [[ ! -d "${mpp_src}" ]]; then
		display_alert "${BOARD}" "MPP sources missing: ${mpp_src}" "wrn"
		return 0
	fi

	declare mpp_glue="${arch_dir}/mpp-glue"
	display_alert "${BOARD}" "Installing Rockchip MPP (rk_vcodec) into ${BRANCH} tree for rkmpp" "info"
	mkdir -p drivers/video/rockchip/mpp
	run_host_command_logged cp -a "${mpp_glue}/Kconfig" "drivers/video/rockchip/Kconfig"
	run_host_command_logged cp -a "${mpp_glue}/Makefile" "drivers/video/rockchip/Makefile"
	run_host_command_logged cp -a "${mpp_src}/." "drivers/video/rockchip/mpp/"
	run_host_command_logged cp -a "${mpp_src}/compat_include/uapi/linux/rk-mpp.h" \
		"include/uapi/linux/rk-mpp.h"
}

# MPP userspace — Chromium +rkmpp, libv4l dummy V4L nodes, mpv hwdec, UPower.
# Mainline also autoloads rk_vcodec + z96a_bat_adc; vendor MPP is in-tree.
function pre_customize_image__z96a_mpp_udev() {
	if [[ "${BRANCH}" != "edge" && "${BRANCH}" != "current" && "${BRANCH}" != "vendor" ]]; then
		return 0
	fi

	local ov="${SRC}/userpatches/overlay/z96a"
	display_alert "${BOARD}" "${BRANCH} MPP/rkmpp: udev, Chromium pin, V4L nodes" "info"

	install -Dm644 "${ov}/etc/udev/rules.d/60-mpp-service.rules" \
		"${SDCARD}/etc/udev/rules.d/60-mpp-service.rules"
	if [[ "${BRANCH}" == "edge" || "${BRANCH}" == "current" ]]; then
		install -Dm644 "${ov}/etc/modules-load.d/rk_vcodec.conf" \
			"${SDCARD}/etc/modules-load.d/rk_vcodec.conf"
		install -Dm644 "${ov}/etc/modules-load.d/z96a-bat-adc.conf" \
			"${SDCARD}/etc/modules-load.d/z96a-bat-adc.conf"
	fi
	install -Dm644 "${ov}/etc/UPower/UPower.conf" \
		"${SDCARD}/etc/UPower/UPower.conf"
	install -Dm644 "${ov}/etc/apt/preferences.d/rkmpp-chromium" \
		"${SDCARD}/etc/apt/preferences.d/rkmpp-chromium"
	install -Dm644 "${ov}/etc/chromium.d/armbian-flags" \
		"${SDCARD}/etc/chromium.d/armbian-flags"
	install -Dm644 "${ov}/lib/systemd/system/z96a-rkmpp-v4l-nodes.service" \
		"${SDCARD}/lib/systemd/system/z96a-rkmpp-v4l-nodes.service"
	# Overlay fallback; rockchip-multimedia-config also ships /usr/bin/...
	install -Dm755 "${ov}/bin/create-chromium-vda-vea-devices.sh" \
		"${SDCARD}/usr/local/bin/create-chromium-vda-vea-devices.sh"

	# Rockchip-multimedia PPA: Chromium +rkmpp, libv4l-rkmpp, ffmpeg/mpv.
	# Pin must land before apt so Armbian desktop Chromium (1001) loses to *rkmpp* (1002).
	# +rkmpp .pak files live in the chromium package (not chromium-common). Downgrading
	# over Armbian/xtradeb chromium-common 154 drops those files — purge first, then
	# install the +rkmpp set in one transaction (include chromium-l10n).
	# Write /etc/mpv/mpv.conf AFTER apt so dpkg does not prompt on conffile conflict.
	if [[ "${BUILD_DESKTOP}" == "yes" ]] && [[ "${RELEASE}" == "noble" || "${RELEASE}" == "jammy" ]]; then
		display_alert "${BOARD}" "install rockchip-multimedia PPA (+rkmpp Chromium / libv4l)" "info"
		do_with_retries 3 chroot_sdcard add-apt-repository ppa:liujianfeng1994/rockchip-multimedia --yes
		do_with_retries 3 chroot_sdcard_apt_get_update
		# Drop stock desktop Chromium so +rkmpp unpack owns /usr/lib/chromium/*.pak.
		chroot_sdcard env DEBIAN_FRONTEND=noninteractive apt-get -y --allow-remove-essential \
			remove --purge chromium chromium-common chromium-sandbox chromium-l10n \
			chromium-browser chromium-codecs-ffmpeg-extra 2>/dev/null || true
		do_with_retries 3 chroot_sdcard env DEBIAN_FRONTEND=noninteractive apt-get -y \
			-o Dpkg::Options::=--force-confdef -o Dpkg::Options::=--force-confold \
			--allow-downgrades --no-install-recommends install \
			rockchip-multimedia-config libv4l-rkmpp v4l-utils \
			chromium chromium-common chromium-sandbox chromium-l10n \
			ffmpeg mpv
		# Fail the image build if resources.pak missing (broken Chromium).
		if [[ ! -f "${SDCARD}/usr/lib/chromium/resources.pak" ]]; then
			exit_with_error "Chromium +rkmpp missing resources.pak under /usr/lib/chromium"
		fi
	fi

	mkdir -p "${SDCARD}/etc/mpv"
	cat > "${SDCARD}/etc/mpv/mpv.conf" <<- EOF
		hwdec=rkmpp
		hwdec-codecs=all
	EOF

	# Unit ExecStart=/usr/bin/... — use package script, else overlay fallback.
	if [[ ! -f "${SDCARD}/usr/bin/create-chromium-vda-vea-devices.sh" ]]; then
		ln -sfn /usr/local/bin/create-chromium-vda-vea-devices.sh \
			"${SDCARD}/usr/bin/create-chromium-vda-vea-devices.sh"
	fi

	chroot_sdcard systemctl enable z96a-rkmpp-v4l-nodes.service
}
