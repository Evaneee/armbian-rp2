# userpatches — 统一板级私有配置

本仓库以官方 [armbian/build](https://github.com/armbian/build) 为骨架，**板子独有内容只放 `userpatches/`**。

## 已支持板型

| 板子 | Board conf | Family | 推荐内核 |
|------|------------|--------|----------|
| 荣品 RP-RK3576J | `config/boards/rp-rk3576j.conf` | 官方 `rk35xx` | `BRANCH=vendor`（6.1-rkr7.2） |
| Z96A 笔记本 (RK3568) | `config/boards/z96a-rk3568-laptop.conf` | 官方 `rk35xx`（同 Rock 3A） | `BRANCH=vendor`（6.1-rkr7.2） |

## Z96A：Noble + GPU / Mesa

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

### Cinnamon 瘦身

板级默认启用 extension `cinnamon-slim`：安装前把 `cinnamon-desktop-environment` 换成 `cinnamon-core`，避免带上 LibreOffice / Thunderbird / Shotwell / Rhythmbox / Synaptic / fonts-noto 等。  
若命中旧的胖 rootfs 缓存，`pre_customize_image` 还会再 purge 一遍。建议删掉旧缓存后重编：

```bash
rm -f cache/rootfs/rootfs-arm64-noble-cinnamon-desktop-mid_*.tar.zst
```

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
- `current`/`edge`（6.18/7.2）需另迁 mainline DTS，尚未启用。
