# Z96A — mainline U-Boot + latest Armbian

## Full image (recommended)

```bash
cd /home/evanee/mygit/armbian-rp2
./compile.sh build \
  BOARD=z96a-rk3568-laptop BRANCH=vendor RELEASE=noble \
  BUILD_DESKTOP=yes DESKTOP_ENVIRONMENT=gnome DESKTOP_ENVIRONMENT_CONFIG_NAME=config_base \
  KERNEL_CONFIGURE=no PREFER_DOCKER=no
```

Image includes mainline U-Boot v2026.04 (DDR **v1.13**, no OP-TEE) and FAT
`boot.scr` from `boot-z96a.cmd` (`load_addr=0x9000000`, always loads `uInitrd`).

Boot path matches Rock 3A: **bootflow → BOOTMETH_SCRIPT → boot.scr**.

## U-Boot only (Maskrom)

Also refresh FAT `boot.scr` on the existing rootfs — do not leave a stale script.

```bash
cd /home/evanee/mygit/armbian-rp2
rm -rf cache/sources/u-boot-worktree/u-boot-z96a-rk3568-laptop/v2026.04
./compile.sh uboot ARTIFACT_IGNORE_CACHE=yes BOARD=z96a-rk3568-laptop BRANCH=vendor KERNEL_CONFIGURE=no PREFER_DOCKER=no

sudo upgrade_tool LD
sudo upgrade_tool db cache/sources/rkbin-tools/rk35/rk356x_spl_loader_v1.21.113.bin
sudo upgrade_tool wl 64 cache/sources/u-boot-worktree/u-boot-z96a-rk3568-laptop/v2026.04/u-boot-rockchip.bin
sudo upgrade_tool rd
```

On the running system (or mounted boot partition):

```bash
mkimage -C none -A arm -T script -d /boot/boot.cmd /boot/boot.scr
# or copy from config/bootscripts/boot-z96a.cmd → boot.cmd then mkimage
```

Expect serial: `z96a ADC: ... (dnl disabled)` then `Booting bootflow ... with script` / `Boot script loaded`.

## Do not regress

| Item | Value |
|------|-------|
| DDR | `rk3568_ddr_1560MHz_v1.13.bin` |
| OP-TEE / BL32 | off |
| `ARMV8_SWITCH_TO_EL1` | off (EL2) |
| dnl download key | always 0 |
| `load_addr` | `0x9000000` (Rock 3A) |
| Boot path | bootflow + `boot.scr` (do not bypass) |
| Initrd | required |
