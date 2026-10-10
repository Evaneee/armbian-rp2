// SPDX-License-Identifier: GPL-2.0+
/*
 * Z96A SPL: BOOT_LOADER → vendor Rockchip Loader (rk356x usbplug).
 *
 * Same state BootROM leaves after "upgrade_tool db": DDR up (DDR TPL ran),
 * EL3, MMU/D-cache off, usbplug at DRAM 0x0 entered at 0x0. SPL itself is
 * linked at 0x0, so stage the blob high and copy it down from a trampoline.
 */
#include <asm/io.h>
#include <asm/system.h>
#include <asm/arch-rockchip/boot_mode.h>
#include <linux/string.h>

#define USBPLUG_STAGE	0x04000000UL
#define TRAMP_ADDR	0x04200000UL

extern const char z96a_usbplug_start[], z96a_usbplug_end[];
extern const char z96a_tramp_start[], z96a_tramp_end[];

asm(".pushsection .rodata.z96a_usbplug, \"a\"\n"
    ".balign 8\n"
    ".global z96a_usbplug_start\n"
    "z96a_usbplug_start:\n"
    ".incbin \"" Z96A_USBPLUG_BIN "\"\n"
    ".balign 8\n"
    ".global z96a_usbplug_end\n"
    "z96a_usbplug_end:\n"
    ".popsection\n");

/* x0 = dst (0x0), x1 = src, x2 = len (multiple of 8). Position independent. */
asm(".pushsection .text.z96a_tramp, \"ax\"\n"
    ".balign 8\n"
    ".global z96a_tramp_start\n"
    "z96a_tramp_start:\n"
    "1:	ldr	x3, [x1], #8\n"
    "	str	x3, [x0], #8\n"
    "	subs	x2, x2, #8\n"
    "	b.gt	1b\n"
    "	dsb	sy\n"
    "	ic	iallu\n"
    "	dsb	sy\n"
    "	isb\n"
    "	mov	x4, #0\n"
    "	br	x4\n"
    ".global z96a_tramp_end\n"
    "z96a_tramp_end:\n"
    ".popsection\n");

int board_early_init_f(void)
{
	void *reg = (void *)CONFIG_ROCKCHIP_BOOT_MODE_REG;
	size_t len = z96a_usbplug_end - z96a_usbplug_start;
	void (*tramp)(ulong dst, ulong src, ulong len);

	if (readl(reg) != BOOT_LOADER)
		return 0;

	writel(BOOT_NORMAL, reg);

	memcpy((void *)USBPLUG_STAGE, z96a_usbplug_start, len);
	memcpy((void *)TRAMP_ADDR, z96a_tramp_start,
	       z96a_tramp_end - z96a_tramp_start);

	asm volatile("msr daifset, #0xf\n"
		     "dsb sy\n"
		     "ic iallu\n"
		     "dsb sy\n"
		     "isb" : : : "memory");

	tramp = (void *)TRAMP_ADDR;
	tramp(0, USBPLUG_STAGE, len);

	return 0;
}
