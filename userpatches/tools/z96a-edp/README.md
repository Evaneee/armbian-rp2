# Z96A eDP live tools（不编整内核）

`analogix_dp` / `naneng-edp` 当前是 `=y` 编进内核，**不能** `rmmod` 热替换。  
快速迭代用脚本 + 可选小 helper `.ko`（只改 MMIO，不替换驱动）。

## 板上

```bash
# 编译机把目录拷过去
scp -r userpatches/tools/z96a-edp evanee@BOARD:~/

# 板上
cd ~/z96a-edp
chmod +x z96a-edp-live.sh
sudo ./z96a-edp-live.sh dump          # 先看 MSA/DPCD/SINK_STATUS
sudo ./z96a-edp-live.sh fixup         # PSR关 + MSA重写 + slave + BIST + 刷红
```

看屏：`fixup` 里先应出现 **BIST 彩条**，再变红。把完整终端输出贴回来。

可选 helper 模块（需同版本 headers）：

```bash
sudo dpkg -i linux-headers-current-rockchip64_*Pacfb*.deb
make
sudo insmod z96a_edp_hack.ko bist=1
dmesg | tail -5
sudo rmmod z96a_edp_hack
```

## 主机交叉编 helper（可选）

```bash
cd userpatches/tools/z96a-edp
make KDIR=../../../cache/sources/linux-kernel-worktree/6.18__rockchip64__arm64 \
     ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
scp z96a_edp_hack.ko evanee@BOARD:~/
```
