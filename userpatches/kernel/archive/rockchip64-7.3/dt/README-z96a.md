# rk3568-z96a.dts (mainline / BRANCH=edge)

Port of the Armbian `current` (rockchip64-6.18) Z96A bring-up DTS to `edge`
(linux-rockchip64 7.2). Same board wiring; U-Boot remains the shared mainline
v2026.07 stack from `userpatches/config/boards/z96a-rk3568-laptop/common/uboot.inc`.

## Display (eDP)

Same path as current: `panel-dpi` on Analogix eDP (`&edp` → `vp1`) with Naneng PHY
patches `zz-z96a-0001` / `0001b` / `0005` / `0007` / `0008`.

Console on the panel needs `fbcon=map:0` (not `map:1`) plus
`video=eDP-1:1920x1080@60`. Defaults live in:
- DTS `chosen/bootargs` (this tree)
- `userpatches/bootscripts/boot-z96a.cmd` (`z96a_displayargs`)
- hybrid FIT: `userpatches/tools/z96a-pack-hybrid.sh`

## Build

```bash
./compile.sh kernel \
  BOARD=z96a-rk3568-laptop \
  BRANCH=edge \
  KERNEL_CONFIGURE=no \
  CPUTHREADS=20 \
  PREFER_DOCKER=yes
```

DTB: `rockchip/rk3568-z96a.dtb` in `linux-dtb-edge-rockchip64`.

## Notes

- Kernel patches were copied from `archive/rockchip64-6.18/`; refresh if 7.2 rejects.
- Kconfig delta vs stock edge: DRM_ROCKCHIP/PANEL_SIMPLE/BACKLIGHT_PWM=y + NANENG_EDP=y
  (see `userpatches/config/kernel/linux-rockchip64-edge.config`).
