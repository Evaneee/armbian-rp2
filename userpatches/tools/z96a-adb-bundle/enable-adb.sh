#!/usr/bin/env bash
# One-shot: install adbd into /usr/local/bin, enable systemd unit, start gadget.
# Copy this whole directory to the board, then:
#   cd z96a-adb-bundle && sudo ./enable-adb.sh

set -euo pipefail

HERE="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"

if [[ "$(id -u)" -ne 0 ]]; then
	echo "Need root. Run: sudo $0"
	exit 1
fi

if [[ ! -x "${HERE}/adbd" ]]; then
	echo "Missing ${HERE}/adbd"
	exit 1
fi

install -m 755 "${HERE}/adbd" /usr/local/bin/adbd
install -m 755 "${HERE}/z96a-adb" /usr/local/bin/z96a-adb
# keep old name
ln -sfn /usr/local/bin/z96a-adb /usr/local/bin/adbservice

if [[ -f "${HERE}/z96a-adb.service" ]]; then
	install -m 644 "${HERE}/z96a-adb.service" /etc/systemd/system/z96a-adb.service
	systemctl daemon-reload
	systemctl enable z96a-adb.service
fi

export ADBD_BIN=/usr/local/bin/adbd
/usr/local/bin/z96a-adb restart
/usr/local/bin/z96a-adb status

echo
echo "Done. On the PC (USB cable plugged into OTG port):"
echo "  adb devices"
echo "  adb shell"
echo "Autostart: systemctl enable --now z96a-adb"
echo "Stop later: sudo systemctl stop z96a-adb"
