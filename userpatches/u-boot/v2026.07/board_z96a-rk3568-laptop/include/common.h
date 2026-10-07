/* Compatibility shim: Radxa DRM sources still #include <common.h>; gone in U-Boot 2026. */
#ifndef __Z96A_COMMON_H_COMPAT_
#define __Z96A_COMMON_H_COMPAT_ 1
#ifndef __ASSEMBLY__
#include <config.h>
#include <errno.h>
#include <time.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/stringify.h>
#include <linux/kernel.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <log.h>
#include <part.h>
#include <image.h>
#include <asm/u-boot.h>
#include <asm/global_data.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <malloc.h>
#include <command.h>
#include <z96a_drm_compat.h>
#endif
#endif
