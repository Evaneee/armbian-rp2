# Z96A board U-Boot patches (mainline v2026.07)

Stay on **mainline `tag:v2026.07`** + DDR **v1.13** + Rock 3A defconfig + RK817 DTS.
DRM is imported from Radxa next-dev as a userpatch (do not switch the whole tree).

## Status (verified on hardware)

| Piece | Result |
|-------|--------|
| eDP link (Analogix + naneng PHY) | `Link Training success!` (HBR / 2-lane) |
| Panel power | `GPIO0_PC7` LCD 3V3 + `GPIO3_PA3` panel enable |
| Logo on panel | Rockchip DRM logo / BMP |
| USB HID power | `GPIO0_PC5` (`z96a_keyboad_vcc`) + `PREBOOT="usb start"` |
| Console mux | `stdin=serial,usbkbd` / `stdout,stderr=serial,vidconsole` |

## Layout

| Path | Role |
|------|------|
| `rk3568-z96a-uboot-extras.dtsi` | Keyboard `GPIO0_PC5`, LCD VCC `GPIO0_PC7`, eDP panel/backlight/vp1/route-edp |
| `drm/` | Radxa DRM subset (VOP2 + Analogix eDP + panel) |
| `phy/phy-rockchip-naneng-edp.c` | RK3568 eDP PHY (+ refclk / pclk_grf enable) |
| `drm/include/` | Private Radxa headers (`rk_edid.h`, `rk_drm_modes.h`, …) |
| `include/` | Global shims (`common.h`, `asm/arch/{cpu,clock}.h`, …) |
| `adapt-mainline.sh` | Radxa → mainline 2026 API adapt (import hook) |
| `stubs/z96a_drm_stubs.c` | Weak stubs (resource / MIPI / bootdev) |

## Build

```bash
cd /home/evanee/mygit/armbian-rp2
LOG=/tmp/z96a-uboot-$(date +%Y%m%d-%H%M%S).log
./userpatches/scripts/wait-armbian-build.sh --run "$LOG" -- \
  ./compile.sh uboot BOARD=z96a-rk3568-laptop BRANCH=current \
  KERNEL_CONFIGURE=no CPUTHREADS=20 PREFER_DOCKER=yes
```

Deb: `output/debs/linux-u-boot-z96a-rk3568-laptop-current_*_arm64__2026.07-*.deb`  
Blob: `cache/sources/u-boot-worktree/u-boot-z96a-rk3568-laptop/v2026.07/u-boot-rockchip.bin`

## Flash (Maskrom + upgrade_tool)

Flash from the compile tree (no copy to `/tmp`). Agent must **ask before flashing**.

```bash
cd /home/evanee/mygit/armbian-rp2
UB=cache/sources/u-boot-worktree/u-boot-z96a-rk3568-laptop/v2026.07
LOADER=cache/sources/rkbin-tools/rk35/rk356x_spl_loader_v1.21.113.bin
sudo upgrade_tool LD
sudo upgrade_tool db "$LOADER"
sudo upgrade_tool wl 64 "$UB/u-boot-rockchip.bin"
sudo upgrade_tool rd
```

## Verify

1. Banner shows new `Pxxxx`; serial has `Link Training success!` (not `AUX CH` / `dpcd caps: -110`).
2. Panel lights with U-Boot logo; after console-mux build, `Out:` includes `vidconsole` and prompt text appears on the panel.
3. Interrupt autoboot with USB keyboard (`PREBOOT="usb start"`).
4. `usb tree` — Terminus hub + HID keyboard/mouse; type at U-Boot prompt.

Expected config: `CONFIG_USB_KEYBOARD=y`, `CONFIG_PREBOOT="usb start"`, `CONFIG_DRM_ROCKCHIP*=y`,
`CONFIG_PHY_ROCKCHIP_NANENG_EDP=y`, `CONFIG_SYS_CONSOLE_IS_IN_ENV=y`, `CONFIG_CONSOLE_MUX=y`.

## Bring-up notes (AUX / DPCD)

If AUX times out (`AUX CH error` / `failed to read dpcd caps: -110`):

1. LCD 3V3 enable **must** drive `GPIO0_PC7` high (vendor regulator gpio / mainline gpio-hog).
2. Naneng PHY needs `refclk` + `pclk_grf` clocks enabled (Linux does; bare Radxa port did not).
3. Analogix controller needs `clk_bulk` enable for `dp`/`pclk`/`spdif`/`hclk`.
4. Panel `enable-gpios` (`GPIO3_PA3`) + prepare delay before link train.

## Rollback

- Maskrom + rescue blobs: `userpatches/overlay/z96a/uboot-prebuilt/`
- Never switch DDR to **v1.21** (bricks this LPDDR4X).
- Do not enable mainline `CONFIG_DISPLAY_ROCKCHIP_EDP` (old `rk_edp`).