# Z96A — 共用入口 (BRANCH=current 与 BRANCH=vendor)
# timezone / packages / powersave + source 同目录功能片段
# Path: z96a-rk3568-laptop/common/
#
# shellcheck source=/dev/null
Z96A_COMMON_INC_DIR="${BASH_SOURCE[0]%/*}"
source "${Z96A_COMMON_INC_DIR}/uboot.inc"
source "${Z96A_COMMON_INC_DIR}/gnome.inc"
source "${Z96A_COMMON_INC_DIR}/bluetooth.inc"
source "${Z96A_COMMON_INC_DIR}/sound.inc"


function post_family_config__z96a_timezone_shanghai() {
	# GeoClue MLS returns "SOUP error: Not Found" on this board/network — GNOME
	# Automatic Time Zone never resolves. Pin Asia/Shanghai for the image.
	declare -g TZDATA="Asia/Shanghai"
	display_alert "${BOARD}" "default timezone ${TZDATA}" "info"
}



function post_family_config__z96a_no_snap() {
	# Match live board: no snapd / no gnome-software-plugin-snap.
	# Keep libsnapd-glib (gnome-shell needs it) and libsnappy (compression).
	display_alert "${BOARD}" "excluding snapd from rootfs/image" "info"
	remove_packages snapd gnome-software-plugin-snap
}

function post_family_config__z96a_add_packages() {
	display_alert "${BOARD}" "adding base packages to image" "info"
	add_packages_to_image vim net-tools arping network-manager-openvpn-gnome \
		openssl

	# /root/{cpu,gpu,rga,jpegd} stress scripts (glmark2 / gst / mpv / ffmpeg)
	# Note: librockchip-mpp* / rockchip-mpp-demos come from Radxa/Armbian rockchip
	# multimedia PPA — not in Ubuntu ports; install on-board or via overlay postinst.
	add_packages_to_image \
		glmark2-es2 glmark2-es2-wayland \
		gstreamer1.0-tools gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
		gstreamer1.0-plugins-bad v4l-utils \
		mpv ffmpeg

	if [[ "${BUILD_DESKTOP}" == "yes" ]]; then
		display_alert "${BOARD}" "ensuring Mesa / Panfrost userspace packages" "info"
		add_packages_to_image \
			libgl1-mesa-dri libglx-mesa0 libegl-mesa0 libgbm1 \
			mesa-utils mesa-vulkan-drivers vulkan-tools

		# Prefer PipeWire stack so z96a-set-pipewire-sink can run.
		add_packages_to_image pipewire pipewire-pulse wireplumber pipewire-audio \
			libspa-0.2-bluetooth pavucontrol

		if [[ "${DESKTOP_ENVIRONMENT}" == "gnome" ]]; then
			# Ubuntu-like GNOME: dock + desktop icons + Yaru (Armbian mid ships vanilla GNOME
			# and explicitly removes ubuntu-session). Keep this lean — not ubuntu-desktop-minimal.
			add_packages_to_image \
				gnome-shell-extension-prefs chrome-gnome-shell \
				ubuntu-session ubuntu-settings ubuntu-wallpapers \
				gnome-shell-extension-ubuntu-dock \
				gnome-shell-extension-desktop-icons-ng \
				yaru-theme-gtk yaru-theme-gnome-shell yaru-theme-icon yaru-theme-sound \
				fonts-ubuntu gnome-terminal software-properties-gtk \
				xdg-desktop-portal-gnome \
				gnome-calendar gnome-clocks gnome-characters evince
		fi

		if [[ "${RELEASE}" == "noble" || "${RELEASE}" == "jammy" ]]; then
			add_packages_to_image guvcview fcitx5 fcitx5-chinese-addons fcitx5-config-qt \
				fcitx5-frontend-gtk4 fcitx5-frontend-gtk3 fcitx5-frontend-gtk2 im-config zenity
		fi
	fi
}



function pre_customize_image__z96a_not_powersaving() {
	echo 'alias ls="ls --color=auto"' >> "${SDCARD}/etc/bash.bashrc"

	if [[ "${BUILD_DESKTOP}" == "yes" && "${DESKTOP_ENVIRONMENT}" == "gnome" ]]; then
		display_alert "${BOARD}" "setup wifi keep-alive arping" "info"
		echo '15 * * * * root /usr/local/bin/arping-router' >> "${SDCARD}/etc/crontab"
	fi
	return 0
}

function pre_customize_image__z96a_purge_snap() {
	# Goal: same as live board after snap removal —
	#   snapd gone, /snap gone, apt pinned; libsnapd-glib kept for gnome-shell.
	display_alert "${BOARD}" "image: purge snapd (keep libsnapd-glib)" "info"
	local ov="${SRC}/userpatches/overlay/z96a"

	# 1) Prefer never installing; still purge if a desktop seed pulled it in.
	chroot_sdcard apt-get -y purge snapd gnome-software-plugin-snap 2>/dev/null || true
	# Never: purge libsnapd-glib* / autoremove — that removes gnome-shell.

	# 2) Leftover dirs / units / autostart
	rm -rf "${SDCARD}/snap" "${SDCARD}/var/lib/snapd" "${SDCARD}/var/cache/snapd" \
		"${SDCARD}/usr/lib/snapd" 2>/dev/null || true
	rm -f "${SDCARD}/etc/xdg/autostart/snap-userd-autostart.desktop" 2>/dev/null || true
	rm -f "${SDCARD}/etc/systemd/system/snapd.service" \
		"${SDCARD}/etc/systemd/system/snapd.socket" \
		"${SDCARD}/etc/systemd/system/snapd.seeded.service" \
		"${SDCARD}/etc/systemd/system/snapd.apparmor.service" 2>/dev/null || true
	find "${SDCARD}/etc/systemd/system" -name '*snapd*' -type l -delete 2>/dev/null || true

	# 3) Apt pin (same as live /etc/apt/preferences.d/nosnap.pref)
	mkdir -p "${SDCARD}/etc/apt/preferences.d"
	if [[ -f "${ov}/etc/apt/preferences.d/nosnap.pref" ]]; then
		install -Dm644 "${ov}/etc/apt/preferences.d/nosnap.pref" \
			"${SDCARD}/etc/apt/preferences.d/nosnap.pref"
	else
		cat > "${SDCARD}/etc/apt/preferences.d/nosnap.pref" <<'EOF'
Package: snapd
Pin: release a=*
Pin-Priority: -10

Package: gnome-software-plugin-snap
Pin: release a=*
Pin-Priority: -10
EOF
	fi

	# 4) Hold so accidental apt install snapd fails closed
	chroot_sdcard apt-mark hold snapd gnome-software-plugin-snap 2>/dev/null || true

	# 5) Mask units if package files somehow remain
	chroot_sdcard systemctl mask snapd.service snapd.socket snapd.seeded.service 2>/dev/null || true

	return 0
}

function pre_customize_image__z96a_cpu_oc_cpufrequtils() {
	# Unlock CPU OC OPPs (2088/2208) — needs ATF 1010 + DTS cpu0_opp.
	local ov="${SRC}/userpatches/overlay/z96a"
	if [[ -f "${ov}/etc/default/cpufrequtils" ]]; then
		display_alert "${BOARD}" "image: cpufrequtils MAX=2208000 (CPU OC)" "info"
		install -Dm644 "${ov}/etc/default/cpufrequtils" \
			"${SDCARD}/etc/default/cpufrequtils"
	fi
	return 0
}

function pre_customize_image__z96a_thermal_guard() {
	# Soft/hard freq caps from thermal zones (HOT 86.5C / HARD 88C).
	local ov="${SRC}/userpatches/overlay/z96a"
	[[ -f "${ov}/bin/z96a-thermal-guard" ]] || {
		display_alert "${BOARD}" "skip thermal-guard: missing ${ov}/bin/z96a-thermal-guard" "wrn"
		return 0
	}
	display_alert "${BOARD}" "image: install z96a-thermal-guard" "info"
	install -Dm755 "${ov}/bin/z96a-thermal-guard" \
		"${SDCARD}/usr/local/bin/z96a-thermal-guard"
	install -Dm644 "${ov}/etc/default/z96a-thermal-guard" \
		"${SDCARD}/etc/default/z96a-thermal-guard"
	install -Dm644 "${ov}/etc/systemd/system/z96a-thermal-guard.service" \
		"${SDCARD}/etc/systemd/system/z96a-thermal-guard.service"
	chroot_sdcard systemctl enable z96a-thermal-guard.service
	return 0
}

function pre_customize_image__z96a_install_root_tests() {
	# Package CPU / GPU OC benches + RGA/4K + jpegd scripts under /root/{cpu,gpu,rga,jpegd}.
	local ov="${SRC}/userpatches/overlay/z96a"
	local src="${ov}/root"
	[[ -d "${src}" ]] || {
		display_alert "${BOARD}" "skip root tests: missing ${src}" "wrn"
		return 0
	}
	display_alert "${BOARD}" "install /root/{cpu,gpu,rga,jpegd} test scripts" "info"
	mkdir -p "${SDCARD}/root"
	cp -a "${src}/." "${SDCARD}/root/"
	find "${SDCARD}/root" -type f \( -name '*.sh' -o -name '*.py' -o -name 'jpegd_test' \) -exec chmod 755 {} +
	# Only under /root — do not install benches into /usr/local/bin.
	return 0
}

function pre_customize_image__z96a_rockchip_multimedia_ppa() {
	# jpegd_test needs rockchip-mpp-demos; packages live on liujianfeng1994 PPA (not Ubuntu).
	local ov="${SRC}/userpatches/overlay/z96a"
	local src="${ov}/etc/apt/sources.list.d/liujianfeng1994-ubuntu-rockchip-multimedia-noble.sources"
	[[ "${RELEASE}" == "noble" ]] || {
		display_alert "${BOARD}" "skip rockchip-multimedia PPA (RELEASE=${RELEASE})" "wrn"
		return 0
	}
	[[ -f "${src}" ]] || {
		display_alert "${BOARD}" "missing ${src}" "wrn"
		return 0
	}
	display_alert "${BOARD}" "add rockchip-multimedia PPA + mpp demos (jpegd)" "info"
	install -Dm644 "${src}" \
		"${SDCARD}/etc/apt/sources.list.d/liujianfeng1994-ubuntu-rockchip-multimedia-noble.sources"
	chroot_sdcard apt-get -y update
	chroot_sdcard apt-get -y --no-install-recommends install \
		librockchip-mpp1 rockchip-mpp-demos || \
		display_alert "${BOARD}" "rockchip-mpp install failed (jpegd_test needs it on-board)" "wrn"
	return 0
}
