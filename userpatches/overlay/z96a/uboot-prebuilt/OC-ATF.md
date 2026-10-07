# Z96A CPU / GPU 超频方法（已实测确认）

Canonical tree: `/home/evanee/mygit/armbian-rp2`  
Board: `z96a-rk3568-laptop` · TF-A v2.14 SCMI · `BOOT_SCENARIO=binman-atf-mainline`

## 方法总览（缺一不可）

| 目标 | ATF (BL31/SCMI) | Kernel DTS OPP | Userspace |
|------|-----------------|----------------|-----------|
| CPU 2088 / 2208 | patch **1010**（PVTPLL + `rk3568_cpu_rates`） | `&cpu0_opp_table` 两档 | `cpufrequtils` `MAX_SPEED=2208000` |
| GPU 900 / 1000 | patch **1011**（PVTPLL；`rk3568_gpu_rates` 原表已含 900/1000） | `&gpu_opp_table` 两档 | 默认 `simple_ondemand` 即可 |
| 启动早期 CPU 1416 | patch **1012** | — | U-Boot `APLL_HZ=1416` + SYR827 init 1.10V |

假频成因：只改 DTS、不改 ATF → 软件列表上去，`clk_scmi_*` 不动。  
判真标准：有负载时读 `/sys/kernel/debug/clk/clk_scmi_{cpu,gpu}/clk_rate`。

## 路径清单

### TF-A — `userpatches/atf/atf-rockchip64/v2.14/`

- `1010-rk3568-scmi-add-2088-2208-mhz.patch` — CPU 2088/2208
- `1011-rk3568-scmi-gpu-oc-900-1000.patch` — GPU PVTPLL 900/1000
- `1012-rk3568-early-cpu-1416mhz.patch` — SCMI init 早期 1416

启用：`uboot.inc` 中 `ATFBRANCH=tag:v2.14.0`、`ATFPATCHDIR=atf-rockchip64/v2.14`；  
板级 `BOOT_SCENARIO="binman-atf-mainline"`。

### Kernel DTS

| Tree | File |
|------|------|
| edge 7.2 | `userpatches/kernel/archive/rockchip64-7.2/dt/rk3568-z96a.dts` |
| current 6.18 | `userpatches/kernel/archive/rockchip64-6.18/dt/rk3568-z96a.dts` |
| vendor 6.1 | `userpatches/kernel/rk35xx-vendor-6.1/dt/rk3568-z96a.dts` |

OPP 电压（已验证）：

- CPU：2088 @ **1.225 V**，2208 @ **1.30 V**（SYR827 max 1.39 V）
- GPU：900 @ **1.05 V**，1000 @ **1.10 V**

驱动：CPU 为 `cpufreq-dt`（必须有 DTS OPP）；时钟经 SCMI 下发。

### Overlay / U-Boot

- `overlay/z96a/etc/default/cpufrequtils` → `MAX_SPEED=2208000`
- `common/uboot.inc`：APLL 1416、SYR827@40、USB kbd PREBOOT、ATF v2.14
- 规范固件：`overlay/z96a/uboot-prebuilt/u-boot-rockchip.bin`  
  （= usbkbd 构建：1010+1011+1012 + early1416 + USB 键盘）

## 实测结论（2026-10-05）

**CPU**（userspace 锁频，中途读 `clk_scmi_cpu`；短测 openssl sha256×4 / 1s）：

| 设定 | SCMI | 相对 1992 |
|------|------|-----------|
| 1992 | 1992 真 | 基准 |
| 2088 | 2088 真 | — |
| 2208 | 2208 真 | sha256 **≈+7.4%**（理论频率比 +10.8%） |

**GPU**（先挂 Wayland glmark 负载，再 `min=max` 锁频，读 `clk_scmi_gpu` + `vdd_gpu`）：

| 设定 | SCMI | vdd_gpu | 结论 |
|------|------|---------|------|
| 800 | 800 | 1.00 V | 真 |
| 900 | 900 | 1.05 V | 真超频 |
| 1000 | 1000 | 1.10 V | 真超频（空闲会掉到 ~200@0.85V，属正常） |

注意：空闲时 GPU soft `cur_freq` 可能仍显示高档，但 SCMI 已掉到 ~200 — **必须以负载下 SCMI 为准**。

## 重建 / 刷写

```bash
cd /home/evanee/mygit/armbian-rp2
./compile.sh uboot BOARD=z96a-rk3568-laptop BRANCH=edge \
  KERNEL_CONFIGURE=no PREFER_DOCKER=yes
# Maskrom: upgrade_tool wl 64 …/u-boot-rockchip.bin
# 或用: userpatches/overlay/z96a/uboot-prebuilt/u-boot-rockchip.bin

./compile.sh kernel BOARD=z96a-rk3568-laptop BRANCH=edge \
  KERNEL_CONFIGURE=no CPUTHREADS=20 PREFER_DOCKER=yes
```

## 快速自检

```bash
cat /sys/devices/system/cpu/cpufreq/policy0/scaling_available_frequencies
# 有负载: cat /sys/kernel/debug/clk/clk_scmi_cpu/clk_rate
cat /sys/class/devfreq/fde60000.gpu/available_frequencies
# 渲染中: cat /sys/kernel/debug/clk/clk_scmi_gpu/clk_rate
```
