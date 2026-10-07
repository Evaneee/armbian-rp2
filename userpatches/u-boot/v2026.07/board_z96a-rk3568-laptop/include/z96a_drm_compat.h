/* SPDX-License-Identifier: GPL-2.0+ */
/* Shims for Radxa next-dev DRM on mainline U-Boot v2026.07 */
#ifndef Z96A_DRM_COMPAT_H
#define Z96A_DRM_COMPAT_H

#ifndef CONFIG_ROCKCHIP_CUBIC_LUT_SIZE
#define CONFIG_ROCKCHIP_CUBIC_LUT_SIZE 0
#endif

#define video_uc_platdata video_uc_plat
#ifndef dev_get_uclass_platdata
#define dev_get_uclass_platdata dev_get_uclass_plat
#endif

/* Radxa DRM uses pre-DM-rename helper; mainline has dev_has_ofnode(). */
#ifndef dev_of_valid
#define dev_of_valid(dev) dev_has_ofnode(dev)
#endif

#endif /* Z96A_DRM_COMPAT_H */
