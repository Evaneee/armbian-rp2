# Z96A ADB bundle

Self-contained USB ADB gadget for the Z96A laptop (OTG Type-C).

## Important (VMware / xHCI)

Gadget is forced to **USB2 high-speed**. SuperSpeed rewrites `bMaxPacketSize0`
to `0x09` and breaks host enumeration (`Invalid ep0 maxpacket` / `can't set config #1`).

On the PC VM: USB controller compatibility **USB 3.1** is fine; the gadget itself
stays HS. Network fallback: `adb connect <board-ip>:5555`.

## Copy to a running board

```bash
scp -r userpatches/tools/z96a-adb-bundle evanee@BOARD:/tmp/
ssh BOARD 'cd /tmp/z96a-adb-bundle && sudo ./enable-adb.sh'
```

## Image integration

Sources of truth for rootfs:

| Path | Role |
|------|------|
| `tools/z96a-adb-bundle/` | Standalone bundle + `enable-adb.sh` |
| `overlay/z96a/bin/{adbd,z96a-adb,adbservice}` | Copied into image |
| `overlay/z96a/lib/systemd/system/z96a-adb.service` | Boot unit |
| `config/boards/.../common/bluetooth.inc` | `pre_customize_image` install + enable |

## On PC

```bash
adb devices
adb shell
# or
adb connect BOARD_IP:5555
```

## Stop / disable autostart

```bash
sudo systemctl stop z96a-adb
sudo systemctl disable z96a-adb
```

Contents: `adbd` (aarch64), `z96a-adb`, `enable-adb.sh`, `z96a-adb.service`.
