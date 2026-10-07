# Rockchip MPP on edge (mainline) — RK3568 / Z96A

Vendor `rk_vcodec` (`/dev/mpp_service`) ported from `rk35xx` 6.1 so
`librockchip-mpp` / ffmpeg `rkmpp` / `mpv --hwdec=rkmpp` work on
`BRANCH=edge` (rockchip64 7.2).

## Layout

| Path | Role |
|------|------|
| `*.c` / `hack/` | Vendor MPP driver (API-drift patched for 7.2) |
| `compat_include/` | Stub BSP headers (dmc/opp/sip/…) + `rk-mpp.h` |
| `bsp_compat_shim.h` | Forced-include: drops `CONFIG_PM_DEVFREQ` cluster |
| `Makefile` / `Kconfig` | In-tree build as `rk_vcodec.ko` |

## How it is applied

1. `custom_kernel_config__z96a_mpp_mainline` (board `edge.inc`) copies this tree
   to `drivers/video/rockchip/mpp/` **after** patching (patching git-resets
   untracked files) and **before** `olddefconfig`.
2. `zz-z96a-mpp-0101-*.patch` wires `drivers/video/{Kconfig,Makefile}`.
3. `zz-z96a-mpp-0100-*.patch` adds MPP DT nodes to `rk356x-base.dtsi` (`vdec_sram` from Armbian `rk356x-add-rkvdec2-support.patch`).
4. `dt/rk3568-z96a.dts` enables MPP nodes and disables V4L2 `vpu`/`vepu`/`vdec`.

## Fast iterate: out-of-tree `.ko` only

After one full edge kernel build (needs `Module.symvers` + Armbian Docker gcc 14):

```bash
./build-oot.sh
# → /tmp/z96a-mpp-oot/rk_vcodec.ko  (~few seconds)

scp /tmp/z96a-mpp-oot/rk_vcodec.ko evanee@192.168.71.38:/tmp/
ssh evanee@192.168.71.38 'sudo mkdir -p /lib/modules/$(uname -r)/updates &&
  sudo cp /tmp/rk_vcodec.ko /lib/modules/$(uname -r)/updates/ &&
  sudo depmod -a && sudo modprobe -r rk_vcodec; sudo modprobe rk_vcodec'
```

Module params (mainline port):

- `mpp_disable_link=1` (default): force `task-capacity=1` (no rkvdec link mode)
- IOMMU: skip `iommu_set_fault_handler` on DMA domains; `mpp_iommu_refresh` uses empty-domain bounce

DT / Kconfig / Image changes still need a full `compile.sh kernel`.

## Verify on board

```bash
ls -l /dev/mpp_service
lsmod | grep rk_vcodec
ffmpeg -hwaccel rkmpp -i test.mp4 -f null -
mpv --hwdec=rkmpp video.mp4
```

## Refresh from vendor

Re-copy from
`cache/sources/linux-kernel-worktree/6.1__rk35xx__arm64/drivers/video/rockchip/mpp`
and re-apply the API-drift edits documented in SympleNZ
`rkvdec-vdpu383-mpp-mainline` (`class_create` 1-arg, `MODULE_IMPORT_NS("…")`,
`void remove`, `fd_file`, `iommu_map(..., GFP_KERNEL)`, `MAX_PAGE_ORDER`).
