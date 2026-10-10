// SPDX-License-Identifier: GPL-2.0+
/*
 * Z96A reboot-to-download commands:
 *
 *   maskrom: BOOT_BROM_DOWNLOAD → patched DDR TPL v1.13 returns -1 to
 *            BootROM → Maskrom USB (2207:350a, no strings).
 *   loader:  BOOT_LOADER → SPL starts the vendor rk356x usbplug
 *            (Rockchip Loader mode, same as "upgrade_tool db").
 *
 * Stock TF-A PSCI SYSTEM_RESET keeps OS_REG0 as long as PMUGRF SOC_CON1
 * bit2 is cleared first.
 */
#include <common.h>
#include <command.h>
#include <usb.h>
#include <linux/arm-smccc.h>
#include <linux/psci.h>
#include <asm/arch-rockchip/boot_mode.h>
#include <asm/barriers.h>
#include <asm/io.h>

#define RK3568_PMUGRF_SOC_CON1		0xFDC20104
#define RK3568_PMUGRF_SOC_CON1_CLR_BIT2	0x00040000
/* usb2phy_grf: clear OTG+HOST phy_sus */
#define RK3568_USB2PHY0_GRF		0xFDCA8000

static int z96a_reboot_mode(const char *name, u32 flag)
{
	void *reg = (void *)(uintptr_t)CONFIG_ROCKCHIP_BOOT_MODE_REG;
	struct arm_smccc_res res;

	printf("%s: reboot with boot mode 0x%08x (via PSCI)\n", name, flag);

	/* Release OTG/PHY from U-Boot USB before the next stage takes it over. */
	usb_stop();
	writel(0x00030002, RK3568_USB2PHY0_GRF);
	writel(0x00030002, RK3568_USB2PHY0_GRF + 4);
	dsb();

	writel(RK3568_PMUGRF_SOC_CON1_CLR_BIT2, RK3568_PMUGRF_SOC_CON1);
	writel(flag, reg);
	dsb();
	printf("%s: *0x%08x = 0x%08x, resetting\n", name,
	       CONFIG_ROCKCHIP_BOOT_MODE_REG, readl(reg));

	arm_smccc_smc(PSCI_0_2_FN_SYSTEM_RESET, 0, 0, 0, 0, 0, 0, 0, &res);

	printf("%s: PSCI SYSTEM_RESET returned 0x%lx (unexpected)\n",
	       name, res.a0);
	return CMD_RET_FAILURE;
}

static int do_maskrom(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	return z96a_reboot_mode("maskrom", BOOT_BROM_DOWNLOAD);
}

U_BOOT_CMD(maskrom, 1, 0, do_maskrom,
	   "reboot to BootROM Maskrom (factory download key)",
	   "\n"
	   "\tWrite BOOT_BROM_DOWNLOAD and PSCI reset; DDR TPL returns\n"
	   "\tto BootROM → Maskrom USB.\n");

static int do_loader(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	return z96a_reboot_mode("loader", BOOT_LOADER);
}

U_BOOT_CMD(loader, 1, 0, do_loader,
	   "reboot to vendor Rockchip Loader mode (usbplug)",
	   "\n"
	   "\tWrite BOOT_LOADER and PSCI reset; SPL starts the vendor\n"
	   "\trk356x usbplug on the Type-C OTG port.\n");
