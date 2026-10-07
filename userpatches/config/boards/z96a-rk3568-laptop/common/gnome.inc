# Z96A — GNOME / Ubuntu-like desktop defaults
# Sourced from common/common.inc
#

# Make GNOME defaults closer to Ubuntu (dock left, Yaru, desktop icons, useful favorites).
function pre_customize_image__z96a_ubuntu_like_gnome() {
	[[ "${BUILD_DESKTOP}" == "yes" && "${DESKTOP_ENVIRONMENT}" == "gnome" ]] || return 0

	display_alert "${BOARD}" "apply Ubuntu-like GNOME dconf defaults" "info"

	local keys="/etc/dconf/db/local.d/01-z96a-ubuntu-gnome"
	local profile="/etc/dconf/profile/user"
	# Latest upstream Armbian (configng branding) ships exactly two desktop wallpapers:
	# light + dark Dre0x. Default matches gnome postinst: light for day, dark for dark mode.
	local wp_light="armbian18-Dre0x-Minum-light-3840x2160.jpg"
	local wp_dark="armbian03-Dre0x-Minum-dark-3840x2160.jpg"
	local wp_uri="file:///usr/share/backgrounds/armbian/${wp_light}"
	local wp_uri_dark="file:///usr/share/backgrounds/armbian/${wp_dark}"
	local wp_src="${SRC}/cache/sources/armbian-configng/tools/modules/desktops/branding/wallpapers"

	mkdir -p "${SDCARD}/usr/share/backgrounds/armbian" \
		"${SDCARD}/usr/share/gnome-background-properties"
	# Branding module normally installs these; stage from configng cache if missing.
	for f in "${wp_light}" "${wp_dark}"; do
		if [[ ! -f "${SDCARD}/usr/share/backgrounds/armbian/${f}" ]]; then
			if [[ ! -f "${wp_src}/${f}" ]]; then
				exit_with_error "${BOARD}" "missing upstream wallpaper ${f} (armbian-configng branding)"
			fi
			cp -f "${wp_src}/${f}" "${SDCARD}/usr/share/backgrounds/armbian/"
		fi
	done
	cat > "${SDCARD}/usr/share/gnome-background-properties/armbian.xml" <<- EOF
		<?xml version="1.0"?>
		<!DOCTYPE wallpapers SYSTEM "gnome-wp-list.dtd">
		<wallpapers>
		  <wallpaper deleted="false">
		    <name>Armbian light</name>
		    <filename>/usr/share/backgrounds/armbian/${wp_light}</filename>
		    <options>zoom</options>
		    <pcolor>#ffffff</pcolor>
		    <scolor>#000000</scolor>
		  </wallpaper>
		  <wallpaper deleted="false">
		    <name>Armbian dark</name>
		    <filename>/usr/share/backgrounds/armbian/${wp_dark}</filename>
		    <options>zoom</options>
		    <pcolor>#ffffff</pcolor>
		    <scolor>#000000</scolor>
		  </wallpaper>
		</wallpapers>
	EOF

	mkdir -p "${SDCARD}/etc/dconf/db/local.d" "${SDCARD}/etc/dconf/profile"
	cat > "${SDCARD}${keys}" <<- EOF
		[org/gnome/shell]
		enabled-extensions=['ubuntu-dock@ubuntu.com', 'ding@rastersoft.com', 'ubuntu-appindicators@ubuntu.com']
		favorite-apps=['org.gnome.Nautilus.desktop', 'chromium.desktop', 'org.gnome.Terminal.desktop', 'org.gnome.TextEditor.desktop', 'org.gnome.Calculator.desktop']

		[org/gnome/shell/extensions/dash-to-dock]
		dock-position='LEFT'
		extend-height=true
		dash-max-icon-size=48
		show-trash=false
		show-mounts=true
		click-action='focus-or-previews'
		multi-monitor=true

		[org/gnome/desktop/interface]
		gtk-theme='Yaru'
		icon-theme='Yaru'
		cursor-theme='Yaru'
		color-scheme='default'
		font-name='Ubuntu 11'
		document-font-name='Ubuntu 11'
		monospace-font-name='Ubuntu Mono 13'

		[org/gnome/desktop/wm/preferences]
		button-layout='appmenu:minimize,maximize,close'

		# Auto TZ needs GeoClue MLS; on-device it fails with SOUP Not Found — leave off.
		[org/gnome/desktop/datetime]
		automatic-timezone=false

		[org/gnome/desktop/peripherals/touchpad]
		tap-to-click=true
		natural-scroll=true

		# Lid closed: blank display only (logind HandleLidSwitch=ignore).
		# Power button stays interactive; z96a-pwrkey-filter drops DC-jack KEY_POWER glitches.
		[org/gnome/settings-daemon/plugins/power]
		lid-close-ac-action='blank'
		lid-close-battery-action='blank'
		power-button-action='interactive'

		# Default terminal: gnome-terminal (Ctrl+Alt+T, Nautilus, desktop right-click).
		[org/gnome/desktop/default-applications/terminal]
		exec='gnome-terminal'
		exec-arg='--'

		[org/gnome/settings-daemon/plugins/media-keys]
		terminal=['<Primary><Alt>t']

		[org/gnome/shell/extensions/ding]
		terminal='gnome-terminal'

		[org/gnome/desktop/background]
		picture-uri='${wp_uri}'
		picture-uri-dark='${wp_uri_dark}'
		picture-options='zoom'
		primary-color='#456789'
		secondary-color='#FFFFFF'

		[org/gnome/desktop/screensaver]
		picture-uri='${wp_uri_dark}'
		picture-options='zoom'
		primary-color='#456789'
		secondary-color='#FFFFFF'
	EOF

	# Ensure user profile loads system-db (gnome postinst usually creates this).
	if [[ ! -f "${SDCARD}${profile}" ]] || ! grep -q 'system-db:local' "${SDCARD}${profile}" 2>/dev/null; then
		cat > "${SDCARD}${profile}" <<- EOF
			user-db:user
			system-db:local
		EOF
	fi

	# Drop stock 00-bg favorites (terminator/…) so first paint uses our 01-z96a keys only.
	# Wallpaper URIs above match upstream configng gnome postinst.
	rm -f "${SDCARD}/etc/dconf/db/local.d/00-bg"

	# gschema overrides apply before user dconf — dock/DING/theme ready at shell start.
	mkdir -p "${SDCARD}/usr/share/glib-2.0/schemas"
	cat > "${SDCARD}/usr/share/glib-2.0/schemas/90_z96a-ubuntu-gnome.gschema.override" <<- EOF
		[org.gnome.shell]
		enabled-extensions=['ubuntu-dock@ubuntu.com', 'ding@rastersoft.com', 'ubuntu-appindicators@ubuntu.com']
		favorite-apps=['org.gnome.Nautilus.desktop', 'chromium.desktop', 'org.gnome.Terminal.desktop', 'org.gnome.TextEditor.desktop', 'org.gnome.Calculator.desktop']
		disable-user-extensions=false
		welcome-dialog-last-shown-version='99.0'

		[org.gnome.shell.extensions.dash-to-dock]
		dock-position='LEFT'
		extend-height=true
		dash-max-icon-size=48
		show-trash=false
		show-mounts=true
		click-action='focus-or-previews'
		multi-monitor=true

		[org.gnome.desktop.interface]
		gtk-theme='Yaru'
		icon-theme='Yaru'
		cursor-theme='Yaru'
		font-name='Ubuntu 11'
		document-font-name='Ubuntu 11'
		monospace-font-name='Ubuntu Mono 13'

		[org.gnome.desktop.wm.preferences]
		button-layout='appmenu:minimize,maximize,close'

		[org.gnome.desktop.datetime]
		automatic-timezone=false

		[org.gnome.desktop.background]
		picture-uri='${wp_uri}'
		picture-uri-dark='${wp_uri_dark}'
		picture-options='zoom'
		primary-color='#456789'
		secondary-color='#FFFFFF'

		[org.gnome.desktop.screensaver]
		picture-uri='${wp_uri_dark}'
		picture-options='zoom'
		primary-color='#456789'
		secondary-color='#FFFFFF'

		[org.gnome.desktop.default-applications.terminal]
		exec='gnome-terminal'
		exec-arg='--'
	EOF
	if [[ ! -f "${SDCARD}/usr/share/backgrounds/armbian/${wp_light}" ]]; then
		exit_with_error "${BOARD}" "default wallpaper not staged: ${wp_light}"
	fi
	chroot_sdcard glib-compile-schemas /usr/share/glib-2.0/schemas || true
	chroot_sdcard dconf update || true

	# Default session must be "ubuntu" (not plain "gnome") so dock/DING load with the
	# shell mode instead of enabling a few seconds after an empty desktop appears.
	mkdir -p "${SDCARD}/etc/skel/.config"
	cat > "${SDCARD}/etc/skel/.dmrc" <<- EOF
		[Desktop]
		Session=ubuntu
	EOF
	if [[ -d "${SDCARD}/etc/gdm3" ]]; then
		mkdir -p "${SDCARD}/etc/gdm3"
		if [[ -f "${SDCARD}/etc/gdm3/custom.conf" ]]; then
			sed -i 's/^#\?WaylandEnable=.*/WaylandEnable=false/' "${SDCARD}/etc/gdm3/custom.conf" || true
		fi
		# PreferredSession used by some gdm builds; harmless if ignored.
		grep -q '^\[daemon\]' "${SDCARD}/etc/gdm3/custom.conf" 2>/dev/null || \
			printf '%s\n' '[daemon]' >> "${SDCARD}/etc/gdm3/custom.conf"
		if grep -q '^#\?PreferredSession=' "${SDCARD}/etc/gdm3/custom.conf" 2>/dev/null; then
			sed -i 's/^#\?PreferredSession=.*/PreferredSession=ubuntu/' "${SDCARD}/etc/gdm3/custom.conf"
		else
			sed -i '/^\[daemon\]/a PreferredSession=ubuntu' "${SDCARD}/etc/gdm3/custom.conf"
		fi
	fi
	if [[ -f "${SDCARD}/etc/lightdm/lightdm.conf" ]]; then
		sed -i 's/^#\?user-session=.*/user-session=ubuntu/' "${SDCARD}/etc/lightdm/lightdm.conf" || true
	fi
	mkdir -p "${SDCARD}/etc/lightdm/lightdm.conf.d"
	cat > "${SDCARD}/etc/lightdm/lightdm.conf.d/90-z96a-ubuntu-session.conf" <<- EOF
		[Seat:*]
		user-session=ubuntu
	EOF

	# Skip first-login wizards that rewrite desktop/extensions after you land.
	mkdir -p "${SDCARD}/etc/xdg/autostart"
	for f in gnome-initial-setup-first-login.desktop gnome-welcome-tour.desktop; do
		if [[ -f "${SDCARD}/etc/xdg/autostart/${f}" ]]; then
			echo 'Hidden=true' >> "${SDCARD}/etc/xdg/autostart/${f}"
		fi
	done
	# Marker so initial-setup considers itself done for new users.
	touch "${SDCARD}/etc/skel/.config/gnome-initial-setup-done"

	local ov="${SRC}/userpatches/overlay/z96a"

	# Quiet idle CPU: PackageKit + gnome-software auto-refresh hammer Cortex-A55.
	# Keep the apps installed; only stop background refresh / session autostart.
	for f in gnome-software-service.desktop org.gnome.Software.desktop; do
		if [[ -f "${SDCARD}/etc/xdg/autostart/${f}" ]]; then
			echo 'X-GNOME-Autostart-enabled=false' >> "${SDCARD}/etc/xdg/autostart/${f}"
			echo 'Hidden=true' >> "${SDCARD}/etc/xdg/autostart/${f}"
		fi
	done
	mkdir -p "${SDCARD}/etc/systemd/system"
	# packagekit.service is D-Bus activated; masking stops background refresh storms.
	chroot_sdcard systemctl mask packagekit.service packagekit-offline-update.service 2>/dev/null || true
	if [[ -f "${SDCARD}/etc/PackageKit/PackageKit.conf" ]]; then
		sed -i 's/^#\?GetUpdatesInterval=.*/GetUpdatesInterval=8640000/' "${SDCARD}/etc/PackageKit/PackageKit.conf" || true
	fi

	# GNOME Settings polls Whoopsie every ~60s; without unix-group:admin that becomes
	# endless "Authentication Required". Create admin, allow sudo/admin via polkit, mask whoopsie.
	chroot_sdcard groupadd -f admin || true
	local home_user
	for home_user in "${SDCARD}"/home/*; do
		[[ -d "${home_user}" ]] || continue
		chroot_sdcard usermod -aG admin "$(basename "${home_user}")" 2>/dev/null || true
	done
	if [[ -f "${ov}/usr/share/polkit-1/rules.d/10-z96a-whoopsie.rules" ]]; then
		install -Dm644 "${ov}/usr/share/polkit-1/rules.d/10-z96a-whoopsie.rules" \
			"${SDCARD}/usr/share/polkit-1/rules.d/10-z96a-whoopsie.rules"
	fi
	printf '%s\n' 'report_crashes=false' > "${SDCARD}/etc/default/whoopsie"
	chroot_sdcard systemctl disable whoopsie.service 2>/dev/null || true
	chroot_sdcard systemctl mask whoopsie.service 2>/dev/null || true

	# xdg/x-terminal-emulator + new-user login shell -> bash via gnome-terminal.
	if [[ -x "${SDCARD}/usr/bin/gnome-terminal.wrapper" ]]; then
		chroot_sdcard update-alternatives --install /usr/bin/x-terminal-emulator x-terminal-emulator /usr/bin/gnome-terminal.wrapper 50 || true
		chroot_sdcard update-alternatives --set x-terminal-emulator /usr/bin/gnome-terminal.wrapper || true
	fi
	if [[ -f "${SDCARD}/etc/adduser.conf" ]]; then
		sed -i 's|^#\?DSHELL=.*|DSHELL=/bin/bash|' "${SDCARD}/etc/adduser.conf"
	fi
	if [[ -f "${SDCARD}/etc/default/useradd" ]]; then
		sed -i 's|^#\?SHELL=.*|SHELL=/bin/bash|' "${SDCARD}/etc/default/useradd"
	fi

	# GNOME Privacy → Location Services ("Automatic Device Location") → GeoIP timezone.
	# Keeps org.gnome.desktop.datetime automatic-timezone=false so gnome-shell does not
	# fight us with broken MLS/geoclue; our helper owns timedatectl when location is on.
	if [[ -f "${ov}/bin/z96a-geoip-timezone" ]]; then
		display_alert "${BOARD}" "install GeoIP timezone helper (GNOME Location Services)" "info"
		install -Dm755 "${ov}/bin/z96a-geoip-timezone" \
			"${SDCARD}/usr/local/bin/z96a-geoip-timezone"
		install -Dm644 "${ov}/lib/systemd/user/z96a-geoip-timezone.service" \
			"${SDCARD}/lib/systemd/user/z96a-geoip-timezone.service"
		install -Dm644 "${ov}/usr/share/polkit-1/rules.d/10-z96a-geoip-timezone.rules" \
			"${SDCARD}/usr/share/polkit-1/rules.d/10-z96a-geoip-timezone.rules"
		# No NetworkManager dispatcher — timezone is not refreshed on every link up (VPN-safe).
		rm -f "${SDCARD}/etc/NetworkManager/dispatcher.d/90-z96a-geoip-timezone"
		mkdir -p "${SDCARD}/etc/skel/.config/systemd/user/default.target.wants"
		ln -sfn /lib/systemd/user/z96a-geoip-timezone.service \
			"${SDCARD}/etc/skel/.config/systemd/user/default.target.wants/z96a-geoip-timezone.service"
		# Enable for already-created desktop users under /home.
		local home_dir
		for home_dir in "${SDCARD}"/home/*; do
			[[ -d "${home_dir}" ]] || continue
			mkdir -p "${home_dir}/.config/systemd/user/default.target.wants"
			ln -sfn /lib/systemd/user/z96a-geoip-timezone.service \
				"${home_dir}/.config/systemd/user/default.target.wants/z96a-geoip-timezone.service"
			chown -R --reference="${home_dir}" "${home_dir}/.config/systemd" 2>/dev/null || true
		done
	fi
}
