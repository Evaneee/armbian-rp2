# Z96A — RTL8821CS Bluetooth rootfs
# Sourced from common/common.inc
#


# GNOME / Bluetooth / sound rootfs hooks — same userspace for current + vendor.
# Kernel BT differs by DT:
#   current/edge: uart1 { bluetooth { compatible = "realtek,rtl8821cs-bt"; } } → hci_uart_h5 serdev
#   vendor:  wireless-bluetooth { compatible = "bluetooth-platdata"; } → rtk_hciattach (enabled below)
#
function pre_customize_image__z96a_enbluetooth() {
	display_alert "${BOARD}" "install RTL8821CS bluetooth from userpatches/overlay" "info"

	local ov="${SRC}/userpatches/overlay/z96a"
	[[ -d "${ov}/bluetooth" ]] || exit_with_error "Missing Z96A bluetooth overlay" "${ov}/bluetooth"
	[[ -f "${ov}/bin/rtk_hciattach" ]] || exit_with_error "Missing rtk_hciattach" "${ov}/bin"

	add_packages_to_image rfkill bluetooth bluez bluez-tools

	mkdir -p "${SDCARD}/lib/firmware/rtl_bt"
	# Vendor blobs + mainline .bin names (symlinks in overlay/bluetooth).
	cp -a "${ov}/bluetooth/rtl8821cs_fw" "${ov}/bluetooth/rtl8821cs_config" \
		"${ov}/bluetooth/rtl8821cs_fw.bin" "${ov}/bluetooth/rtl8821cs_config.bin" \
		"${SDCARD}/lib/firmware/rtl_bt/"
	# Ensure .bin names exist even if cp dropped symlinks.
	ln -sfn rtl8821cs_fw "${SDCARD}/lib/firmware/rtl_bt/rtl8821cs_fw.bin"
	ln -sfn rtl8821cs_config "${SDCARD}/lib/firmware/rtl_bt/rtl8821cs_config.bin"

	install -Dm755 "${ov}/bin/bt_load_rtk_firmware" "${SDCARD}/usr/local/bin/bt_load_rtk_firmware"
	install -Dm755 "${ov}/bin/rtk_hciattach" "${SDCARD}/usr/local/bin/rtk_hciattach"
	# Canonical bring-up: /etc/z96a/z96a-bt (pipe-stdin btmgmt; ssp/le off then power on)
	install -Dm755 "${ov}/etc/z96a/z96a-bt" "${SDCARD}/etc/z96a/z96a-bt"
	ln -sfn /etc/z96a/z96a-bt "${SDCARD}/usr/local/bin/z96a-bt-post"
	install -Dm755 "${ov}/bin/z96a-bt-fw-link" "${SDCARD}/usr/local/bin/z96a-bt-fw-link"
	install -Dm644 "${ov}/lib/systemd/system/z96a-bt-fw-link.service" \
		"${SDCARD}/lib/systemd/system/z96a-bt-fw-link.service"
	install -Dm644 "${ov}/lib/systemd/system/z96a-bt-ready.service" \
		"${SDCARD}/lib/systemd/system/z96a-bt-ready.service"
	install -Dm644 "${ov}/lib/systemd/system/bluetooth.service.d/z96a-after-hci.conf" \
		"${SDCARD}/etc/systemd/system/bluetooth.service.d/z96a-after-hci.conf"

	for f in adbd adbservice io jtagactive jtaginquire arping-router; do
		[[ -f "${ov}/bin/${f}" ]] && install -Dm755 "${ov}/bin/${f}" "${SDCARD}/usr/local/bin/${f}"
	done

	for m in hci_uart rtw88_8821cs gpio_keys snd-soc-simple-amplifier; do
		grep -qxF "$m" "${SDCARD}/etc/modules" 2>/dev/null || echo "$m" >> "${SDCARD}/etc/modules"
	done
	install -Dm644 "${ov}/etc/modules-load.d/z96a-gpio-keys.conf" \
		"${SDCARD}/etc/modules-load.d/z96a-gpio-keys.conf"
	install -Dm644 "${ov}/etc/modules-load.d/z96a-audio.conf" \
		"${SDCARD}/etc/modules-load.d/z96a-audio.conf"

	# Lid hall → SW_LID: do NOT suspend. logind ignores lid; z96a-lid-blank
	# toggles backlight (GNOME "blank" is unreliable with HandleLidSwitch=ignore).
	mkdir -p "${SDCARD}/etc/systemd/logind.conf.d"
	cat > "${SDCARD}/etc/systemd/logind.conf.d/10-z96a-lid.conf" <<- EOD
		[Login]
		HandleLidSwitch=ignore
		HandleLidSwitchExternalPower=ignore
		HandleLidSwitchDocked=ignore
	EOD
	# Debounce rk805 KEY_POWER (DC jack falsely pulses PWRON → GNOME 60s dialog).
	install -Dm755 "${ov}/bin/z96a-pwrkey-filter" "${SDCARD}/usr/local/bin/z96a-pwrkey-filter"
	install -Dm644 "${ov}/lib/systemd/system/z96a-pwrkey-filter.service" \
		"${SDCARD}/lib/systemd/system/z96a-pwrkey-filter.service"
	chroot_sdcard systemctl enable z96a-pwrkey-filter.service
	install -Dm755 "${ov}/bin/z96a-lid-blank" "${SDCARD}/usr/local/bin/z96a-lid-blank"
	install -Dm755 "${ov}/bin/z96a-lid-diag" "${SDCARD}/usr/local/bin/z96a-lid-diag"
	install -Dm644 "${ov}/lib/systemd/system/z96a-lid-blank.service" \
		"${SDCARD}/lib/systemd/system/z96a-lid-blank.service"
	install -Dm644 "${ov}/etc/default/z96a-lid-blank" \
		"${SDCARD}/etc/default/z96a-lid-blank"
	chroot_sdcard systemctl enable z96a-lid-blank.service

	# AutoEnable without duplicating [General] (append was breaking main.conf).
	if [[ -f "${SDCARD}/etc/bluetooth/main.conf" ]]; then
		if grep -qE '^#?AutoEnable=' "${SDCARD}/etc/bluetooth/main.conf"; then
			sed -i 's/^#\?AutoEnable=.*/AutoEnable=false/' "${SDCARD}/etc/bluetooth/main.conf"
		else
			sed -i '/^\[General\]/a AutoEnable=false' "${SDCARD}/etc/bluetooth/main.conf"
		fi
	fi

	# current/edge: uart1 H5 serdev (DT realtek,rtl8821cs-bt) — do not grab ttyS1.
	# vendor: bluetooth-platdata — rtk_hciattach on /dev/ttyS1, then z96a-bt.
	install -Dm644 "${ov}/lib/systemd/system/bluetooth-rtl8821cs.service" \
		"${SDCARD}/etc/systemd/system/bluetooth-rtl8821cs.service"
	if [[ "${BRANCH}" == "vendor" ]]; then
		chroot_sdcard systemctl enable bluetooth-rtl8821cs.service
	else
		chroot_sdcard systemctl disable bluetooth-rtl8821cs.service || true
	fi
	chroot_sdcard systemctl enable z96a-bt-fw-link.service
	chroot_sdcard systemctl enable z96a-bt-ready.service
	# BlueZ starts from /etc/z96a/z96a-bt after hci0 (not at multi-user).
	chroot_sdcard systemctl disable bluetooth.service || true
}
