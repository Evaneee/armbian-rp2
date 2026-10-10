# Z96A（RK3568）U-Boot 重启模式改造报告

## 结论

三个重启功能都能正常工作，CPU 开机 1416 MHz 的加速也保留着：

- `reset` 和 Linux `reboot`：正常重启。
- `maskrom`：进 BootROM Maskrom 模式（`2207:350a`，没有字符串描述符）。
- `loader`：进原厂 Rockchip Loader 模式，Windows RKDevTool 和 Linux upgrade_tool 都能认出来。

另外，编译时已经去掉了 rockusb、USB gadget 和 fastboot。

刷写 U-Boot：`sudo upgrade_tool wl 64 u-boot-rockchip.bin`

提交：`6cffa74e6`（rp3576-work）。

---

## 一、重启卡死（`reset` 和 Linux `reboot`）

### 原来的问题
加上 ATF 补丁 `1012`（开机就把 CPU 设到 1416 MHz）以后，U-Boot 里执行 `reset` 就会卡死：蓝灯灭、黑屏，也进不了 BootROM。Linux 下执行 `reboot` 也有同样的风险。

### 原因
- RK3568 的 CPU 跑到高频时，时钟来源是 **PVTPLL**。这是芯片内部的一个环形振荡器（ring oscillator），不是普通的 APLL。
- 如果 CPU 还在 PVTPLL 上就直接做全局软复位（global soft reset），芯片会在进入 BootROM 之前挂住。
- U-Boot 自带的 `rockchip_sysreset` 驱动是直接写 CRU 寄存器来复位的，完全绕过了 TF-A（ATF）。所以就算 ATF 里做了保护，U-Boot 的 `reset` 也用不上。

### 修改
1. **新增 ATF 补丁 `1014-rk3568-leave-pvtpll-before-soft-reset.patch`**：在 `soc_global_soft_reset()` 一开始先调用 `sys_reset_pvtplls_prepare()`，把 CPU 和 PVTPLL 相关时钟切回安全的 PLL。然后停掉三个 PVTPLL 振荡环，也就是往 `CPUGRF+0x10`、`GRF+0x700`、`GRF+0x740` 写 `0xffff0000`，最后再做全局软复位。
2. **U-Boot 改走 PSCI 重启**：
   - 打开 `CONFIG_SYSRESET_PSCI=y`。
   - 在 `clk_rk3568.c` 里，U-Boot proper 不再绑定 `rockchip_sysreset`。SPL 里照旧保留，因为 SPL 阶段还没有 ATF。
   - 这样 `reset` 会通过 PSCI `SYSTEM_RESET` 交给 ATF，再走上面第 1 条的安全路径。
3. Linux 的 `reboot` 本来就是走 PSCI 调 ATF，所以 `1014` 加上以后它也一起好了，已实测确认。

### 排除过的方向
- `1013` 补丁试图在复位时保留 BootROM download 标志，结果反而把复位弄坏了，所以一直保持禁用（`.disabled`），只作参考。
- RK809 sleep-pin 断电的猜测：查寄存器后确认 `SYS_CFG3=0x20`、sleep 功能为空，不是这个原因。板子上实际的 PMIC 是 RK817。

---

## 二、`maskrom` 命令（进 BootROM Maskrom）

### 原来的问题
按理说写入 `BOOT_BROM_DOWNLOAD` 标志再重启，就会进 Maskrom，但实际上又正常开机了。原因有两层：
1. PMUGRF `SOC_CON1` 寄存器的 bit2 默认会让 `OS_REG` 在复位时被清掉，标志根本留不到下次启动。
2. 原厂 DDR TPL v1.13 不检查这个标志，DDR 初始化完就照常往下启动。

### 原理和修改
- **`maskrom` 命令**（`stubs/z96a_loader.c`）依次做这几步：
  1. `usb_stop()`
  2. 复位 USB2PHY0 的 GRF 设置
  3. 清掉 PMUGRF `SOC_CON1` 的 bit2（往 `0xFDC20104` 写 `0x40000`），让标志能在复位后保留
  4. 往 `0xFDC20200`（PMUGRF `OS_REG0`）写入 `0xEF08A53C`
  5. 发 PSCI `SYSTEM_RESET` 重启
- **改过的 DDR TPL** 是 `rk3568_ddr_1560MHz_v1.13_maskrom.bin`：在 TPL 入口附近（`0x810c`/`0x8110` 改成 nop，`0x8114` 改成 `b.ne`）加了 EF08 标志判断。检测到这个标志时，TPL 返回 -1 给 BootROM，BootROM 就进入 Maskrom。
- 结果：主机上看到 `2207:350a`，没有字符串描述符，就是真正的 BootROM Maskrom。

### 踩过的坑
- 调试用的 LED 探针里用了 `bl`，把 BootROM 的返回地址（lr）覆盖了。后来改成先把 lr 保存到 x8 再调用。
- Armbian 的产物缓存哈希不包含 TPL 二进制的内容，改了 TPL 内容也会直接用旧缓存。所以每次修改都要换文件名，才能强制重新打包。

---

## 三、`loader` 命令（进原厂 Rockchip Loader）

### 原来的问题
1. 最开始的 `loader` 用的是主线 U-Boot 的 rockusb（`rockusb 0 mmc 0`）。设备名是 "USB download gadget"，不是原厂 Loader。
2. 改成启动原厂 usbplug 以后，设备已经能正常枚举（RockChip / USB-MSC），但 Windows 刷机工具把它认成了 Maskrom，不是 Loader。

### 原理
- 原厂 Loader 的实体是 rkbin 里 `rk356x_spl_loader_v1.21.113.bin` 中的 **usbplug**（`rk356x_usbplug_v1`，大小 `0x18800`）。平时 `upgrade_tool db` 就是让 BootROM 把它加载到 DRAM 的 0 地址，然后在 EL3、MMU 关闭的状态下跳过去运行。
- Rockchip 刷机工具判断模式**只看 USB 设备描述符里 `bcdUSB` 的最低位**（rkdeveloptool 源码 `RKScan.cpp` 第 395-400 行）：
  - 最低位是 0 → Maskrom
  - 最低位是 1 → Loader
- 原厂 usbplug 报的是 `bcdUSB=0x0200`，所以执行 `db` 之后工具显示的仍然是 "Maskrom"。

### 修改
1. **`loader` 命令**：流程和 `maskrom` 一样，只是写入的标志换成 `BOOT_LOADER`（`0x5242C301`）。
2. **在 SPL 里接管**（`stubs/z96a_spl_usbplug.c`，只编进 SPL）：
   - 借用 `board_early_init_f()` 这个钩子。它在 SPL 很早的阶段运行，那时 DDR 已经可用，但还没开 cache，也还没加载 ATF。在这里检测到 `BOOT_LOADER` 时：
     1. 先把标志改回 `BOOT_NORMAL`，这样在 Loader 里执行 `rd` 之后能正常开机。
     2. 把 SPL 里通过 `.incbin` 内嵌的 usbplug 先拷到 `0x04000000` 暂存。
     3. 把一小段与地址无关的跳板代码拷到 `0x04200000`。
     4. 关中断，刷 I-cache，跳到跳板。
     5. 跳板把 usbplug 拷到 DRAM 0 地址，再跳到 0 地址执行。这和 BootROM 执行 `db` 时的做法一样。
   - SPL 大小从 139 KB 增加到 234776 字节，上限是 262144 字节（`0x40000`），用掉 89%。
3. **改一个字节让工具认成 Loader**：`rk356x_usbplug_v1.21_loader.bin` 只把文件偏移 `0x14f34` 处的 `bcdUSB` 从 `0x0200` 改成 `0x0201`，其余和原厂完全一样。
4. **从编译中去掉 rockusb**：
   - 关掉 `USB_GADGET`、`USB_GADGET_DOWNLOAD`、`USB_FUNCTION_ROCKUSB`、`CMD_ROCKUSB`、`USB_DWC3_GADGET` 以及 fastboot 相关配置项。
   - 删除 gadget 的 VID/PID/厂商名配置，删除 `boot_mode.c` 里 `BOOT_LOADER` 时跑 rockusb 的 preboot 逻辑，删除 `g_dnl_board_usb_cable_connected()`，删除 rockusb 补丁 `0002`–`0004`。
   - 在 U-Boot 设备树里把 Type-C OTG 控制器（`usb_host0_xhci`）设为 `disabled`。键盘走 `usb_host1`，不受影响。

---

## 四、改动文件清单（`userpatches/`）

| 文件 | 作用 |
|---|---|
| `atf/atf-rockchip64/v2.14/1012-rk3568-early-cpu-1416mhz.patch` | 开机就把 CPU 设到 1416 MHz（保留） |
| `atf/atf-rockchip64/v2.14/1014-rk3568-leave-pvtpll-before-soft-reset.patch` | 复位前切回安全 PLL 并停掉 PVTPLL |
| `u-boot/v2026.07/board_z96a-rk3568-laptop/stubs/z96a_loader.c` | `maskrom`、`loader` 两个命令 |
| `u-boot/v2026.07/board_z96a-rk3568-laptop/stubs/z96a_spl_usbplug.c` | SPL 启动原厂 usbplug |
| `u-boot/v2026.07/board_z96a-rk3568-laptop/rk3568-z96a-uboot-extras.dtsi` | 关闭 OTG 控制器 |
| `overlay/z96a/rkbin/rk3568_ddr_1560MHz_v1.13_maskrom.bin` | 加了 EF08 判断的 DDR TPL |
| `overlay/z96a/rkbin/rk356x_usbplug_v1.21_loader.bin` | 改过 `bcdUSB` 的原厂 usbplug |
| `config/boards/z96a-rk3568-laptop/common/uboot.inc` | 构建集成：PSCI 重启、去 rockusb、SPL usbplug 编译规则 |

## 五、维护注意
- 以后改 TPL 或 usbplug 这类二进制文件时，**一定要换文件名，并同步改 `uboot.inc` 里的引用**，否则 Armbian 缓存会继续用旧产物。
- SPL 已经用掉 89% 的空间，还剩约 27 KB。再往 SPL 里加功能时要注意大小。
- 升级 rkbin 的 usbplug 后，要重新确认设备描述符的位置（找 `12 01 00 02 … 07 22`），再改 `bcdUSB`。
