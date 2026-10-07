# Z96A — RK817 sound / PipeWire rootfs
# Sourced from common/common.inc
#


function pre_customize_image__z96a_ensound() {
	display_alert "${BOARD}" "install z96a-ensound (RK817 HP mux + Internal Speakers amp)" "info"
	local ov="${SRC}/userpatches/overlay/z96a"
	local udir d

	install -Dm755 "${ov}/bin/z96a-ensound" "${SDCARD}/usr/local/bin/z96a-ensound"
	install -Dm755 "${ov}/bin/z96a-set-pipewire-sink" "${SDCARD}/usr/local/bin/z96a-set-pipewire-sink"
	install -Dm644 "${ov}/lib/systemd/user/z96a-set-pipewire-sink.service" \
		"${SDCARD}/usr/lib/systemd/user/z96a-set-pipewire-sink.service"
	install -Dm644 "${ov}/etc/wireplumber/main.lua.d/51-z96a-rk817-stereo.lua" \
		"${SDCARD}/etc/wireplumber/main.lua.d/51-z96a-rk817-stereo.lua"
	# Prefer stereo + Speakers; also install oneshot mixer route if present.
	if [[ -f "${ov}/etc/systemd/system/z96a-speaker-route.service" ]]; then
		install -Dm644 "${ov}/etc/systemd/system/z96a-speaker-route.service" \
			"${SDCARD}/etc/systemd/system/z96a-speaker-route.service"
		chroot_sdcard systemctl enable z96a-speaker-route.service 2>/dev/null || true
	fi
	# XDG autostart as fallback when systemd --user wants are not started from skel.
	install -Dm644 "${ov}/etc/xdg/autostart/z96a-set-pipewire-sink.desktop" \
		"${SDCARD}/etc/xdg/autostart/z96a-set-pipewire-sink.desktop"
	# Prefer PipeWire over classic pulseaudio daemon (matches GNOME stack).
	chroot_sdcard systemctl --global enable z96a-set-pipewire-sink.service 2>/dev/null || true
	chroot_sdcard systemctl --global disable pulseaudio.socket pulseaudio.service 2>/dev/null || true
	chroot_sdcard systemctl --global mask pulseaudio.socket pulseaudio.service 2>/dev/null || true

	# ALSA default: overlay has stereo card0; else placeholder (z96a-ensound may rewrite).
	if [[ -f "${ov}/etc/asound.conf" ]]; then
		install -Dm644 "${ov}/etc/asound.conf" "${SDCARD}/etc/asound.conf"
	else
		cat > "${SDCARD}/etc/asound.conf" <<- EOD
			pcm.!default {
				type plug
				slave.pcm "qhw"
			}
			pcm.qhw {
				type hw
				card 0
				channels 2
			}
			ctl.!default {
				type hw
				card 0
			}
		EOD
	fi

	install -Dm644 "${ov}/lib/systemd/system/amixer_enspk1.service" \
		"${SDCARD}/etc/systemd/system/amixer_enspk1.service"
	chroot_sdcard systemctl enable amixer_enspk1.service
	# plymouth-quit-wait often stalls ~30s after GDM is already up — drop it.
	chroot_sdcard systemctl mask plymouth-quit-wait.service 2>/dev/null || true

	# eDP cursor trails on this panel — force Mutter software cursors.
	install -Dm644 "${ov}/etc/systemd/system/gdm.service.d/z96a-mutter.conf" \
		"${SDCARD}/etc/systemd/system/gdm.service.d/z96a-mutter.conf"

	# Faster GNOME login: drop DING + heavy autostarts; shorten post-password black screen.
	install -Dm644 "${ov}/etc/dconf/db/local.d/03-z96a-gnome-login" \
		"${SDCARD}/etc/dconf/db/local.d/03-z96a-gnome-login"
	install -Dm644 "${ov}/etc/environment.d/90-z96a-gnome.conf" \
		"${SDCARD}/etc/environment.d/90-z96a-gnome.conf"
	for f in geoclue-demo-agent org.gnome.Evolution-alarm-notify pulseaudio \
		org.gnome.SettingsDaemon.Wacom org.gnome.SettingsDaemon.Smartcard \
		org.gnome.SettingsDaemon.PrintNotifications org.gnome.SettingsDaemon.Wwan \
		org.gnome.SettingsDaemon.UsbProtection org.gnome.SettingsDaemon.Sharing \
		org.gnome.SettingsDaemon.DiskUtilityNotify; do
		install -Dm644 "${ov}/etc/xdg/autostart/${f}.desktop" \
			"${SDCARD}/etc/xdg/autostart/${f}.desktop"
	done
	chroot_sdcard dconf update 2>/dev/null || true
	# Optional typelib for ubuntu-dock AppIndicator menus (avoids DBusMenu spam).
	chroot_sdcard apt-get install -y gir1.2-dbusmenu-glib-0.4 2>/dev/null || true
	# Tracker / Evolution EDS factories activate during Shell start — mask by default.
	for u in tracker-miner-fs-3.service evolution-calendar-factory.service \
		evolution-source-registry.service evolution-addressbook-factory.service; do
		chroot_sdcard systemctl --global disable "${u}" 2>/dev/null || true
		chroot_sdcard systemctl --global mask "${u}" 2>/dev/null || true
	done

	# Desktop apps (browser/video) use PipeWire, not raw ALSA like speaker-test.
	for d in "${SDCARD}/etc/skel" "${SDCARD}"/home/*; do
		[[ -d "${d}" ]] || continue
		udir="${d}/.config/systemd/user/default.target.wants"
		mkdir -p "${udir}"
		ln -sf /usr/lib/systemd/user/z96a-set-pipewire-sink.service \
			"${udir}/z96a-set-pipewire-sink.service"
	done
}
