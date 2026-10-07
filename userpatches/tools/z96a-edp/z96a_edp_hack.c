// SPDX-License-Identifier: GPL-2.0
/*
 * Tiny loadable helper for Z96A eDP bring-up (does NOT replace analogix_dp).
 * On load: force slave mode, REGISTER timing bit, VIDEO_EN, STRM_VALID+HPD,
 * optional BIST. Unload clears BIST only.
 *
 * Build on board (with linux-headers matching uname -r):
 *   make
 *   sudo insmod z96a_edp_hack.ko bist=1
 */
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>

#define DP_BASE		0xfe0c0000
#define DP_SIZE		0x10000

#define REG_FUNC_EN_1	0x18
#define REG_VIDEO_CTL_1	0x20
#define REG_VIDEO_CTL_4	0x2c
#define REG_VIDEO_CTL_10 0x44
#define REG_SYS_CTL_3	0x608
#define REG_SOC_GENERAL	0x800

#define VIDEO_EN	BIT(7)
#define BIST_EN		BIT(3)
#define FORMAT_SEL	BIT(4)
#define F_HPD		BIT(5)
#define HPD_CTRL	BIT(4)
#define F_VALID		BIT(1)
#define VALID_CTRL	BIT(0)
#define AUDIO_SPDIF	BIT(8)
#define VIDEO_SLAVE	BIT(0)

static void __iomem *dp;
static bool bist = false;
module_param(bist, bool, 0644);
MODULE_PARM_DESC(bist, "enable Analogix BIST colorbar on load");

static u32 r(u32 off) { return readl(dp + off); }
static void w(u32 off, u32 v) { writel(v, dp + off); }

static int __init z96a_edp_hack_init(void)
{
	u32 c10, sys3;

	dp = ioremap(DP_BASE, DP_SIZE);
	if (!dp)
		return -ENOMEM;

	/* slave + VIDEO_EN + REGISTER timing + force valid/HPD */
	w(REG_SOC_GENERAL, AUDIO_SPDIF | VIDEO_SLAVE);
	c10 = r(REG_VIDEO_CTL_10) | FORMAT_SEL;
	w(REG_VIDEO_CTL_10, c10);
	w(REG_VIDEO_CTL_1, r(REG_VIDEO_CTL_1) | VIDEO_EN);
	sys3 = r(REG_SYS_CTL_3) | F_HPD | HPD_CTRL | F_VALID | VALID_CTRL;
	w(REG_SYS_CTL_3, sys3);

	if (bist)
		w(REG_VIDEO_CTL_4, BIST_EN);
	else
		w(REG_VIDEO_CTL_4, 0);

	pr_info("z96a_edp_hack: SOC=0x%x CTL1=0x%x CTL4=0x%x CTL10=0x%x SYS3=0x%x bist=%d\n",
		r(REG_SOC_GENERAL), r(REG_VIDEO_CTL_1), r(REG_VIDEO_CTL_4),
		r(REG_VIDEO_CTL_10), r(REG_SYS_CTL_3), bist);
	return 0;
}

static void __exit z96a_edp_hack_exit(void)
{
	if (dp) {
		w(REG_VIDEO_CTL_4, 0);
		iounmap(dp);
	}
	pr_info("z96a_edp_hack: unloaded (BIST cleared)\n");
}

module_init(z96a_edp_hack_init);
module_exit(z96a_edp_hack_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Z96A bring-up");
MODULE_DESCRIPTION("Z96A eDP live MMIO helper");
