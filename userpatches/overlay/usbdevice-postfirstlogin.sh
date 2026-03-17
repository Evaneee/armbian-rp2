#!/bin/sh
set -eu

if [ -f /root/.not_logged_in_yet ]; then
	exit 0
fi

systemctl enable usbdevice.service
systemctl start usbdevice.service

systemctl disable usbdevice-postfirstlogin.service
