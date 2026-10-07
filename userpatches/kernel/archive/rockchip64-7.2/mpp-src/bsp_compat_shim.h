/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Forced-include for Rockchip MPP on mainline/edge.
 * System headers (e.g. soc/rockchip/pm_domains.h) shadow -Icompat copies for
 * paths that already exist upstream, so BSP-only symbols live here.
 */
#ifndef __BSP_COMPAT_SHIM_H
#define __BSP_COMPAT_SHIM_H

#ifdef CONFIG_PM_DEVFREQ
#undef CONFIG_PM_DEVFREQ
#endif

#include <linux/types.h>

struct device;

static inline int rockchip_pmu_idle_request(struct device *dev, bool idle)
{
	return 0;
}

static inline int rockchip_save_qos(struct device *dev)
{
	return 0;
}

static inline int rockchip_restore_qos(struct device *dev)
{
	return 0;
}

static inline int rockchip_pmu_pd_on(struct device *dev)
{
	return 0;
}

static inline int rockchip_pmu_pd_off(struct device *dev)
{
	return 0;
}

static inline bool rockchip_pmu_pd_is_on(struct device *dev)
{
	return true;
}

/* Vendor SiP helper — absent from mainline soc/rockchip/rockchip_sip.h */
static inline int sip_smc_vpu_reset(u32 cfg, u32 arg1, u32 arg2)
{
	return 0;
}

#endif /* __BSP_COMPAT_SHIM_H */
