# rk3568-z96a.dts (mainline / BRANCH=current)

Bring-up DTS for Armbian `current` (linux-rockchip64 6.18), derived from `rk3568-rock-3a.dts`.

## Display (eDP)

Laptop panel is wired as `panel-dpi` on Rockchip Analogix eDP (`&edp` → `vp1`).

Working kernel path (`zz-z96a-0001` / `0001b` / `0005` / `0007` / `0008`; SoC eDP nodes live in this board DTS):

- Naneng eDP PHY + ASSR/SSC/enhanced framing
- REGISTER MSA timing + `force_stream_valid`
- **Preserve force-HPD** when forcing `STRM_VALID` (`SYS_CTL_3` must keep `F_HPD|HPD_CTRL`)

Expect dmesg: `z96a-edp-v19` and `force STRM_VALID+HPD` with `SYS_CTL_3` ≈ `0x7x` (not `0x3`).

Panel console needs `fbcon=map:0` + `video=eDP-1:1920x1080@60` (hybrid-validated).
Defaults: DTS `chosen/bootargs`, `bootscripts/boot-z96a.cmd`.

## Other intentional differences from vendor 6.1

- PMIC: `rockchip,rk817` (Rock 3A base used RK809)
- Lid hall: GPIO0_C6 → `SW_LID`
- Battery / EC keyboard / touchpad: may still be incomplete vs vendor image

## Build

```bash
./compile.sh build \
  BOARD=z96a-rk3568-laptop \
  BRANCH=current \
  RELEASE=noble \
  BUILD_DESKTOP=yes \
  DESKTOP_ENVIRONMENT=gnome \
  DESKTOP_TIER=mid \
  KERNEL_CONFIGURE=no \
  PREFER_DOCKER=no \
  ARTIFACT_IGNORE_CACHE=yes
```

DTB: `rockchip/rk3568-z96a.dtb` in `linux-dtb-current-rockchip64`.

## Post-flash checklist

1. Serial: U-Boot → `rk3568-z96a.dtb` → kernel
2. `uname -r` → `*-current-rockchip64`
3. `dmesg | grep z96a-edp-v19` → LT ok + `STRM_VALID+HPD`
4. Panel shows framebuffer / desktop (not pure white)
