// SPDX-License-Identifier: GPL-2.0+
/*
 * Z96A: one-shot USB RockUSB loader entry for laptop keyboard use.
 *
 * Type "loader" once (while USB kbd still works). Internally runs
 * env loader_cmd (default: usb stop; rockusb 0 mmc 0) so OTG can
 * switch from host to gadget without a second keystroke.
 */
#include <common.h>
#include <command.h>
#include <env.h>
#include <g_dnl.h>

#define Z96A_LOADER_CMD_DEFAULT	"usb stop; rockusb 0 mmc 0"

int g_dnl_board_usb_cable_connected(void)
{
	/* Type-C OTG has no VBUS sense GPIO in this board DT. */
	return 1;
}

static int do_loader(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	const char *script = env_get("loader_cmd");

	if (!script || !script[0])
		script = Z96A_LOADER_CMD_DEFAULT;

	printf("USB loader: %s\n", script);
	printf("(USB host stops — use serial or PC rkdeveloptool; Ctrl+C on serial to exit)\n");

	return run_command(script, 0);
}

U_BOOT_CMD(loader, 1, 0, do_loader,
	   "enter USB RockUSB loader mode (one shot)",
	   "\n"
	   "\tStops USB host then runs rockusb so Type-C enumerates as Rockchip loader.\n"
	   "\tOverride script with: setenv loader_cmd '...'; saveenv\n"
	   "\tDefault: " Z96A_LOADER_CMD_DEFAULT "\n");
