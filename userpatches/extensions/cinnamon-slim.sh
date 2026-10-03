# Slim Cinnamon: do not keep cinnamon-desktop-environment kitchen-sink
# (LibreOffice, Thunderbird, Shotwell, Rhythmbox, Synaptic, fonts-noto, GIMP, …).
# Prefer cinnamon-core. Enable with: enable_extension "cinnamon-slim"
#
# No lib/ hook required: after desktop install (or fat rootfs cache hit),
# pre_customize_image purges the bloat. (A pre_install_desktop yaml rewrite
# would need a call_extension_method in rootfs-create.sh — avoided so lib/
# can track mainline.)

function pre_customize_image__cinnamon_slim_purge() {
	[[ "${BUILD_DESKTOP}" == "yes" && "${DESKTOP_ENVIRONMENT}" == "cinnamon" ]] || return 0

	display_alert "cinnamon-slim" "purge cinnamon-desktop-environment extras" "info"
	# Explicit list from cinnamon-desktop-environment Depends/Recommends (noble).
	# Keep mid-tier apps (chromium, vlc, loupe, …) and cinnamon-core stack.
	local -a purge_pkgs=(
		cinnamon-desktop-environment
		libreoffice-writer libreoffice-calc libreoffice-impress libreoffice-draw
		libreoffice-math libreoffice-base-core libreoffice-gnome libreoffice-gtk3
		libreoffice-common libreoffice-core
		libreoffice-style-colibre libreoffice-style-elementary libreoffice-style-yaru
		libreoffice-uiconfig-writer libreoffice-uiconfig-calc libreoffice-uiconfig-impress
		libreoffice-uiconfig-draw libreoffice-uiconfig-math libreoffice-uiconfig-common
		thunderbird
		shotwell shotwell-common
		rhythmbox rhythmbox-data rhythmbox-plugins
		rhythmbox-plugin-cdrecorder rhythmbox-plugin-alternative-toolbar
		synaptic
		fonts-noto fonts-noto-core fonts-noto-ui-core fonts-noto-extra fonts-noto-ui-extra
		fonts-noto-cjk fonts-noto-cjk-extra fonts-noto-color-emoji fonts-noto-mono fonts-noto-unhinted
		gimp inkscape
		cheese cheese-common
		brasero brasero-common brasero-cdrkit
		deja-dup
		hexchat hexchat-common hexchat-plugins hexchat-python3 hexchat-perl hexchat-lua
		remmina remmina-common remmina-plugin-rdp remmina-plugin-vnc remmina-plugin-secret
		seahorse simple-scan sound-juicer
		yelp yelp-xsl
		gnome-games gnome-characters gnome-font-viewer gnome-logs gnome-sound-recorder
		gnote pidgin pidgin-data
		eog gnome-screenshot
		gdebi gdebi-core
		orca vino mate-themes
	)

	local -a installed=()
	local p
	for p in "${purge_pkgs[@]}"; do
		if chroot_sdcard dpkg-query -W -f='${Status}' "${p}" 2>/dev/null | grep -q 'install ok installed'; then
			installed+=("${p}")
		fi
	done
	if [[ ${#installed[@]} -eq 0 ]]; then
		display_alert "cinnamon-slim" "already slim (nothing to purge)" "info"
		return 0
	fi

	display_alert "cinnamon-slim" "purging ${#installed[@]} packages" "info"
	DONT_MAINTAIN_APT_CACHE="yes" chroot_sdcard_apt_get purge "${installed[@]}" || true
	DONT_MAINTAIN_APT_CACHE="yes" chroot_sdcard_apt_get autoremove --purge || true
}
