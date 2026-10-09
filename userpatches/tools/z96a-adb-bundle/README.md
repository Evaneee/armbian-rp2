# Z96A ADB bundle

Self-contained USB ADB gadget for the Z96A laptop (OTG).

## Copy to board

```bash
# from build host
scp -r userpatches/tools/z96a-adb-bundle evanee@192.168.71.141:/tmp/
```

## Enable

```bash
# on board
cd /tmp/z96a-adb-bundle
sudo ./enable-adb.sh
```

## On PC

```bash
adb devices
adb shell
```

## Stop

```bash
sudo z96a-adb stop
```

Contents: `adbd` (aarch64), `z96a-adb`, `enable-adb.sh`.
