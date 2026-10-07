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
