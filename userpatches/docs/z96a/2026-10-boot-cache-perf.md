# Z96A 笔记本启动加速与缓存、流畅度优化报告

## 一、结论

开机后登录界面提前约 2.5 秒出现，GDM 不再被网络拖住。缓存相关的配置都已核实在内存里，并修掉了两个隐患：`/run/user` 容量不足，以及着色器缓存每次重启都丢失。

| 指标 | 优化前 | 优化后 | 变化 |
|---|---|---|---|
| GDM 服务启动（开机计时） | 约 17.6 s | 12.86 s | 提前约 4.7 s |
| 登录界面会话建立 | 17.3–17.9 s | 13.35 s | 提前约 4 s |
| 登录界面画面出现 | 23.2–23.7 s | 20.7 s | 提前约 2.5 s |
| 输完密码到桌面 | 约 5 s | 约 5 s | 已接近极限，未改 |
| `/run/user/1000` 容量 | 392 MB | 1 GB | 不会再被写满 |
| Mesa 着色器缓存 | tmpfs，重启清空 | eMMC，重启保留 | 首次绘制不再重编着色器 |
| GPU 最低频率 | 200 MHz | 400 MHz | 点击后响应更快 |

`systemd-analyze` 统计的开机总时间没有缩短（22.2 s → 24.0 s），因为 GDM 现在和网络、蓝牙等服务并行启动，排在后面的服务结束得反而晚一些。对使用者来说，有意义的指标是登录界面什么时候出现，这一项确实提前了。

提交（rp3576-work）：`fe42878c9` 是刷机前已有的桌面优化，`4590d058c` 是本次的启动和缓存优化。

**测试环境**：Z96A（RK3568，4 GB 内存，eMMC 运行在 HS200），内核 7.3.0-rc6-edge，Ubuntu 24.04 + GNOME（Wayland），Chromium 带 rkmpp 补丁。

---

## 二、优化前的状态核查

| 项目 | 实测结果 | 是否在内存里 |
|---|---|---|
| CMA | 512 MB（`cma=512M` 已写进 `armbianEnv.txt` 并生效），空闲 518640 kB | — |
| `/tmp` | tmpfs，2 GB | 是 |
| `/dev/shm` | tmpfs，2 GB；已去掉 Chromium 的 `--disable-dev-shm-usage` | 是 |
| Chromium 磁盘缓存 | `/run/user/1000/chromium-disk-cache`，上限 256 MB | 是 |
| Chromium 配置文件 | profile-sync-daemon（psd）放在 `/run/user/1000/psd`（39 MB），定期同步回 eMMC | 是 |
| `XDG_CACHE_HOME` | `/run/user/1000/xdg-cache` | 是 |
| Chromium GPU | GPU 进程打开了 panfrost 的 `renderD128` 和 `dma_heap`；GPU 光栅化、zero-copy 和 Wayland 都已启用 | — |
| CPU | `performance` 调速器，2208 MHz | — |
| swap | zram 1.9 GB（lzo-rle），`swappiness=100` | 是 |

CMA 空闲量大是正常的：没有播放视频时 VPU 用不到它，而 CMA 空闲时普通程序也能借用，不算浪费。

### 发现的问题
1. **GDM 被网络拖住**：登录界面要等网络就绪才开始启动，详见第三节。
2. **`/run/user` 容量不足**：它是 tmpfs，默认只有内存的 10%（392 MB），但上面同时放着 Chromium 缓存（上限 256 MB）、psd 配置文件和 XDG 缓存，用久了有写满的风险。
3. **着色器缓存放错了地方**：Mesa 着色器缓存跟着 `XDG_CACHE_HOME` 放在 tmpfs 上，每次重启都清空，GNOME 和 Chromium 第一次绘制时要重新编译着色器。
4. **GPU 空闲频率低**：GPU 用 `simple_ondemand` 调速，空闲时停在 200 MHz（一次开机累计约 180 秒），点击后要先升频，第一帧容易掉帧。
5. **启动了用不上的服务**：apport（3.3 s）、gnome-remote-desktop（1.8 s）、ModemManager（确认没有 modem）、cups 和 cups-browsed。

---

## 三、启动加速：让 GDM 不再等网络

### 原因
`systemd-analyze critical-chain` 显示，GDM 是通过三条依赖链被网络拖住的：

1. **`z96a-pwrkey-filter.service` 排在 multi-user.target 之后**：电源键去抖服务写了 `After=multi-user.target` 和 `Before=gdm.service`，等于强制 GDM 等 multi-user.target 完成。而 multi-user.target 要等 `rc-local.service`，`rc-local` 又排在 `network-online.target` 之后。
2. **空的 `rc.local`**：`/etc/rc.local` 里只有一行 `exit 0`，但因为它有执行权限，systemd 照样把它拉进启动流程，并排在网络就绪之后。
3. **`systemd-user-sessions.service` 等网络**：它带有 `After=network.target`，这是为网络登录准备的，笔记本用不上。GDM 排在它后面，于是要等 NetworkManager 启动完成。

NetworkManager 启动要 4.9–5.3 秒，主要耗在通过 SDIO 初始化 rtw88 无线网卡上。

### 修改
| 修改 | 位置 | 作用 |
|---|---|---|
| pwrkey-filter 改为 `After=systemd-udevd.service systemd-logind.service` | `overlay/z96a/lib/systemd/system/z96a-pwrkey-filter.service` | 只等输入设备就绪，不再等 multi-user |
| 去掉 `/etc/rc.local` 的执行权限 | 镜像钩子 `pre_customize_image__z96a_boot_speedups` | 不再把网络就绪拉进 GDM 的依赖链 |
| 在 `/etc/systemd/system/` 放一份去掉 `network.target` 的 `systemd-user-sessions.service` | 同上 | GDM 不再等 NetworkManager |
| NetworkManager 只保留 keyfile 插件 | 同上，`zz-30-z96a-keyfile-only.conf` | 板子没有 `/etc/network/interfaces`，ifupdown 插件用不上 |

### 验证
- 只改 pwrkey-filter 时，GDM 只提前约 1 秒，因为 `systemd-user-sessions` 还在等 `network.target`。去掉这个依赖后，GDM 就和网络并行启动了。
- 改完后的 critical-chain：`gdm.service` → `z96a-pwrkey-filter`（@7.6 s）→ `systemd-logind` → `basic.target`，依赖链里已经没有网络。
- NetworkManager 单独重启测试：只保留 keyfile 插件比原来快约 0.4 秒（3303 ms vs 3719 ms）。
- Wi-Fi 照常自动连接，VPN 插件（OpenVPN、PPTP、VPNC、SSH）全部保留。`plugins=keyfile` 只影响连接配置从哪里读取，与 VPN 插件无关。

### 踩过的坑
- 用 drop-in 覆盖 pwrkey-filter 的 `After=` 不生效：systemd 不允许在 drop-in 里清空 `After=` 这类依赖。所以最终直接修改 unit 文件本身。
- 通过 adb 写入的文件默认是所有人可写的，systemd 会报警告，需要改为 644 权限。

---

## 四、缓存与流畅度优化

### 1. `/run/user` 扩到 1 GB
- **修改**：`/etc/systemd/logind.conf.d/20-z96a-runtime-dir.conf` 设置 `RuntimeDirectorySize=1G`。
- **验证**：`/run/user/1000` 变为 1.0 GB，当前用了 44 MB。
- tmpfs 只按实际使用量占用内存，设成 1 GB 不会预先占掉 1 GB。

### 2. Mesa 着色器缓存放回 eMMC
- **修改**：在 XDG 缓存的环境变量生成器和 `environment.d` 配置里，加上 `MESA_SHADER_CACHE_DIR=$HOME/.cache/mesa_shader_cache`。其他 XDG 缓存仍然留在内存里。
- **原因**：着色器缓存很小（实测 1.4–1.7 MB），写 eMMC 几乎没有开销；但它被清空后，每次开机第一次绘制都要重编着色器，代价大得多。
- **验证**：GNOME Shell 的环境变量里已有 `MESA_SHADER_CACHE_DIR`，`~/.cache/mesa_shader_cache` 已生成。gdm 用户也有自己的缓存目录 `/var/lib/gdm3/.cache/mesa_shader_cache`，登录界面同样受益。
- Chromium 自己的 GPU 着色器缓存在配置文件目录里，由 psd 同步回 eMMC，不受影响。

### 3. GPU 最低频率 400 MHz
- **修改**：udev 规则 `/etc/udev/rules.d/90-z96a-gpu-minfreq.rules`，开机时把 `fde60000.gpu` 的 `min_freq` 设为 400 MHz。
- **验证**：`min_freq=400000000`。GPU 工作时最低就是 400 MHz，一次都没回到 300 MHz。
- 空闲时偶尔读到 200 MHz 是正常的：GPU 空闲 50 ms 后会自动挂起，挂起时读到的就是最低频率，一有任务就恢复到 400 MHz 以上。
- 和 thermal-guard 不冲突：它过热时最多把上限压到 400 MHz，不会低于这个下限。

### 4. 关掉用不上的服务
| 服务 | 原启动耗时 | 处理 |
|---|---|---|
| apport（含 path、timer、socket） | 3.3 s | 禁用（崩溃上报，用不上） |
| gnome-remote-desktop | 1.8 s | 禁用 |
| ModemManager | 0.7 s | mask（确认没有 modem） |
| cups、cups-browsed（含 socket、path） | — | 禁用（需要打印时可重新启用） |

这些服务不在 GDM 的依赖链上，但会在登录前后抢 CPU 和 I/O。

---

## 五、保留的已有优化（`fe42878c9`）

| 类别 | 配置 |
|---|---|
| GNOME | 关闭桌面图标扩展 DING 和动画；保留硬件光标（软件光标会阻止 Mutter 全屏直通显示）；GSK 渲染器设为 `ngl`；Nautilus 用轻量设置（关闭全文搜索和缩略图）；屏蔽 Tracker 和 Evolution 数据服务 |
| Chromium | `chromium.d/zz-z96a-perf`：去掉 `--disable-dev-shm-usage`、磁盘缓存放 tmpfs、光栅线程数设为 4 |
| 内存 | `cma=512M`，供 Chromium 和 VPU 的 zero-copy 视频解码使用 |
| CPU | `performance` 调速器，最高 2208 MHz；thermal-guard 过热降频，GPU 硬限制调到 400 MHz |
| 缓存 | `XDG_CACHE_HOME` 放在会话 tmpfs 上，注销即清空 |

---

## 六、顺带修复的问题

`z96a-speaker-route.service` 开机失败：摄像头上电修好以后，它的 USB 麦克风占了声卡 0，RK817 变成了声卡 2，而这个服务和 `asound.conf` 都把声卡编号写死成 0。现在两者都改为按声卡名 `RK817` 访问，服务已恢复为 active。这个修复也在 `4590d058c` 里。

---

## 七、改动文件清单（`4590d058c`）

| 文件 | 内容 |
|---|---|
| `config/.../common/common.inc` | 新增钩子 `pre_customize_image__z96a_boot_speedups`：`systemd-user-sessions` 覆盖、禁用 `rc.local`、NetworkManager 只用 keyfile、`RuntimeDirectorySize=1G`、GPU 下限 udev 规则、禁用服务 |
| `overlay/z96a/lib/systemd/system/z96a-pwrkey-filter.service` | 改为 `After=systemd-udevd.service systemd-logind.service` |
| `overlay/z96a/usr/lib/systemd/user-environment-generators/90-z96a-xdg-cache` | 增加 `MESA_SHADER_CACHE_DIR` |
| `overlay/z96a/etc/skel/.config/environment.d/90-z96a-xdg-cache.conf` | 同上（新用户默认配置） |
| `overlay/z96a/etc/systemd/system/z96a-speaker-route.service` | 按声卡名 `RK817` 访问 |
| `overlay/z96a/etc/asound.conf` | 同上 |
| `userpatches/README.md` | 优化表里补充以上各项说明 |

---

## 八、使用说明

- **需要打印时**重新启用 cups：

```bash
sudo systemctl enable --now cups cups-browsed
```

- **调整 Chromium 缓存上限**：改 `/etc/chromium.d/zz-z96a-perf` 里的 `--disk-cache-size`。当前是 256 MB，`/run/user` 已扩到 1 GB，可以容纳。
- **刷新镜像后的第一次开机**：着色器缓存还是空的，所以第一次登录和打开 Chromium 会稍慢，第二次起就是正常速度。

## 九、遗留事项

1. **Chromium 的实际体验还没有反馈**：着色器缓存放回 eMMC、GPU 下限提到 400 MHz 之后，Chromium 第二次打开是否更快、点击是否更跟手，还需要实际使用确认。
2. **NetworkManager 启动仍然要约 5 秒**：瓶颈在 rtw88 SDIO 网卡初始化。它已经不影响登录界面出现，但开机后 Wi-Fi 要在桌面出现后几秒才连上。
3. **另外两个启动较慢的服务**：`z96a-bt-ready`（4.6 s）和 `z96a-adb`（3.1 s）都不在 GDM 的依赖链上，所以这次没有动。如果以后要继续压缩开机后的总负载，可以从这两个入手。
4. **eMMC 运行在 HS200**：之前排查 HS400 的工作已经中止，目前 HS200（200 MHz、8 位总线）的读写速度对日常使用够用。
