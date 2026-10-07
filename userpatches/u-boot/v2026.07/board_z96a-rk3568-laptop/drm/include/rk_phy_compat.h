/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef RK_PHY_COMPAT_H
#define RK_PHY_COMPAT_H
#include <generic-phy.h>
#include <generic-phy-dp.h>
#ifndef __RK_PHY_CONFIGURE_OPTS_DEFINED
#define __RK_PHY_CONFIGURE_OPTS_DEFINED
/* Mainline generic-phy.h may lack DP configure opts used by Analogix eDP. */
union phy_configure_opts {
	struct phy_configure_opts_dp dp;
};
#endif
#endif
