# userpatches — 统一板级私有配置

本仓库以官方 [armbian/build](https://github.com/armbian/build) 为骨架，**板子独有内容只放 `userpatches/`**。

## 已支持板型

| 板子 | Board conf | Family | 推荐内核 |
|------|------------|--------|----------|
| 荣品 RP-RK3576J | `config/boards/rp-rk3576j.conf` | 官方 `rk35xx` | `BRANCH=vendor`（6.1-rkr7.2） |
| Z96A 笔记本 (RK3568) | `config/boards/z96a-rk3568-laptop.conf` | 官方 `rk35xx`（同 Rock 3A） | **eDP：`BRANCH=current`（6.18）**；vendor 6.1 仍可用 |

## Z96A：Noble + current eDP（推荐）

笔记本面板 eDP 已在 mainline `current`（6.18）打通：`userpatches/kernel/archive/rockchip64-6.18/zz-z96a-0001*`…`0008` + `dt/rk3568-z96a.dts`。

关键点：强制 `STRM_VALID` 时必须保留 force-HPD（`SYS_CTL_3` 含 `F_HPD|HPD_CTRL`）。刷机后 dmesg 应见 `z96a-edp-v19` / `force STRM_VALID+HPD`。

### U-Boot：USB HID + eDP vidconsole（mainline v2026.07）

栈不变：`tag:v2026.07` + DDR **v1.13** + Rock 3A defconfig + RK817 DTS。  
Radxa next-dev DRM（VOP2 + Analogix eDP）以 userpatch 导入，详见  
`u-boot/v2026.07/board_z96a-rk3568-laptop/README.md`。

| 能力 | 配置 |
|------|------|
| 键盘供电 | DTS `z96a_keyboad_vcc` → `GPIO0_PC5` |
| USB HID | `CONFIG_USB_KEYBOARD=y` + `CONFIG_PREBOOT="usb start"` |
| eDP 控制台 | `CONFIG_DRM_ROCKCHIP*`（勿开旧 `DISPLAY_ROCKCHIP_EDP`） |

```bash
./compile.sh uboot BOARD=z96a-rk3568-laptop BRANCH=current \
  KERNEL_CONFIGURE=no CPUTHREADS=20 PREFER_DOCKER=yes
# 在编译目录直接 Maskrom 刷写（先确认板子进 Maskrom / 宿主机见 2207）:
UB=cache/sources/u-boot-worktree/u-boot-z96a-rk3568-laptop/v2026.07
LOADER=cache/sources/rkbin-tools/rk35/rk356x_spl_loader_v1.21.113.bin
sudo upgrade_tool LD
sudo upgrade_tool db "$LOADER"
sudo upgrade_tool wl 64 "$UB/u-boot-rockchip.bin"
sudo upgrade_tool rd
```

验证：串口/`usb tree` 见 HID；键盘可打断 autoboot；面板出 U-Boot 文字。  
回滚：Maskrom + `overlay/z96a/uboot-prebuilt/`。**禁止**换 DDR v1.21。

### 音频 / 蓝牙 / 开机（current）

均在 `userpatches/`（勿再依赖板子 `/tmp` 手工拷贝）：

| 项 | 位置 |
|----|------|
| RK817 + AW8737 amp、耳机检测 | `kernel/archive/rockchip64-6.18/dt/rk3568-z96a.dts`（PineTab2 式 `simple-audio-amplifier`，GPIO3_PA6 / GPIO3_PB2） |
| RTL8821CS BT serdev | 同上 DTS：`&uart1` / `realtek,rtl8821cs-bt`（vendor 唤醒脚 GPIO0_PB6 / PC4；PC1 与 WiFi enable 共享） |
| 喇叭路径脚本 | `overlay/z96a/bin/z96a-ensound` + `amixer_enspk1.service`（设一次即退出，勿空等） |
| BT 固件 | `overlay/z96a/bluetooth/rtl8821cs_{fw,config}` + `.bin` symlink（mainline btrtl） |
| BT 就绪 | `z96a-bt-post` / `z96a-bt-ready.service`；`bluetooth-rtl8821cs` 仅作 fallback |
| 开机加速 | 镜像阶段 `mask plymouth-quit-wait`；GDM `MUTTER_DEBUG_DISABLE_HW_CURSORS=1` |
| 登录→桌面 | 密码后约 30s 主要是 GNOME Shell JS：DING / Evolution / GeoClue / Tracker；见下 |

### 登录后桌面慢（GNOME）

journal 时间线（密码 → “GNOME Shell started” ≈30s）：mutter/KMS ~5–10s，其余多为 Shell 扩展与 autostart。

| 措施 | 文件 / 动作 |
|------|-------------|
| 关掉桌面图标扩展 DING、关动画 | `overlay/z96a/etc/dconf/db/local.d/03-z96a-gnome-login` |
| 禁用 Evolution / GeoClue / 无用 GSD / snap / pulseaudio | `overlay/z96a/etc/xdg/autostart/*.desktop`（`Hidden=true`） |
| Dock 缺 typelib | 镜像装 `gir1.2-dbusmenu-glib-0.4` |
| Tracker + Evolution EDS | `systemctl --global mask tracker-miner-fs-3` / `evolution-*-factory` |
| GSK/mutter 环境 | `etc/environment.d/90-z96a-gnome.conf` + GDM drop-in |

黑屏原因：输完密码后 GDM 收掉 greeter，新 `gnome-shell` Wayland 抢 DRM，首帧出来前整屏黑（本机约 25–30s）。上述只缩短超时，无法变成瞬时切屏。

已装系统：登出再登或重启验证；需要桌面图标：`gnome-extensions enable ding@rastersoft.com`。

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

## Z96A：Noble + vendor + GPU / Mesa

上游已删除 `ENABLE_EXTENSIONS=mesa-vpu`。桌面镜像在 rootfs 阶段由：

```text
armbian-config --api module_desktops install … tier=mid mode=build
```

自动安装 Mesa（`libgl1-mesa-dri`、`mesa-vulkan-drivers` 等）。板 conf 里还额外 `add_packages_to_image` 了一遍作兜底。

内核侧：DTS 中 `&gpu { status = "okay"; }`，走 **Panfrost**（Mali-G52）。

```bash
./compile.sh build \
  BOARD=z96a-rk3568-laptop \
  BRANCH=vendor \
  RELEASE=noble \
  BUILD_DESKTOP=yes \
  DESKTOP_ENVIRONMENT=gnome \
  DESKTOP_TIER=mid \
  KERNEL_CONFIGURE=no
```

GNOME 镜像会额外装 Ubuntu Dock / 桌面图标 / Yaru / `ubuntu-session`。登录时选 **Ubuntu**（不要选纯 GNOME）。

**不要**再加 `ENABLE_EXTENSIONS=mesa-vpu`。

### 电池 / DC 闪电图标 / rkmpp / 蓝牙

| 项 | edge/current | vendor (rk35xx 6.1) |
|----|----------------|---------------------|
| DC 检测 | `z96a_bat_adc` GPIO3_PA5 IRQ + 50ms | `rk817,charger` `dc_det_gpio` GPIO3_PA5 IRQ + 10ms（勿用 PC0） |
| 电量 | SARADC bat-adc + hysteresis | `rk817,battery` + SARADC patch |
| UPower | overlay `UPower.conf` CriticalPowerAction=Ignore | 同左（`edge.inc` mpp hook） |
| 闪电 KEY_POWER 误触 | `z96a-pwrkey-filter` 2s | 同左 |
| VPU / Chromium | in-tree `rk_vcodec` + PPA `+rkmpp` | 内核已有 MPP；用户态同 PPA `+rkmpp` |
| 蓝牙 | uart1 H5 serdev | `bluetooth-platdata` + `bluetooth-rtl8821cs.service` (`rtk_hciattach`) |

勿再把 mainline DC 状态绑回 5s×N 次轮询去抖。vendor 的 `dc_det_gpio` 必须是 **GPIO3_PA5**（pinctrl 已是 PA5）。

## RP-3576J 构建示例

```bash
./compile.sh build BOARD=rp-rk3576j BRANCH=vendor RELEASE=noble BUILD_DESKTOP=yes DESKTOP_TIER=mid
```

## 目录约定

```
userpatches/
├── config/boards/                  # 板级入口
├── config/sources/families/        # 仅旧私有 family 归档（Z96A 已改用官方 rk35xx）
├── linux-*.config                  # 板级内核 defconfig（按需）
├── kernel/rk35xx-vendor-6.1/       # RP + Z96A 的 vendor 补丁 / dt/
│   └── dt/rk3568-z96a.dts
└── overlay/                        # 固件与工具（Z96A 蓝牙等）
```

## 升级官方主干

```bash
git fetch upstream
git merge upstream/main
```

冲突优先保留 `userpatches/`。vendor 内核升档后若 Makefile/DTS 上下文变了，检查 `userpatches/kernel/rk35xx-vendor-6.1/`。

## 备注

- Z96A 旧 5.10 legacy family 仍留在 `config/sources/families/rockchip-rk3568-z96a.conf` 与 `kernel/rockchip-rk3568-z96a/`，仅作参考，板 conf 已不再引用。
- Jailhouse/虚拟化相关已禁用。
- `current`（6.18）：eDP 补丁与 DTS 在 `userpatches/kernel/archive/rockchip64-6.18/`（见上）。
