# Z96A — no Snap (image + live)

## Target state (same as current board)

- **Removed:** `snapd`, `gnome-software-plugin-snap`, `/snap`, `/var/lib/snapd`, snapd units, `snap` CLI
- **Kept:** `libsnapd-glib` (required by gnome-shell), `libsnappy` (compression, unrelated), `xdg-desktop-portal`

## How the next image gets this

| Stage | Hook / file | Effect |
|-------|-------------|--------|
| package lists | `common.inc` → `post_family_config__z96a_no_snap` | `remove_packages snapd gnome-software-plugin-snap` |
| customize | `common.inc` → `pre_customize_image__z96a_purge_snap` | purge if seeded, wipe dirs, pin + hold, mask units |
| overlay | `overlay/z96a/etc/apt/preferences.d/nosnap.pref` | apt Pin-Priority -10 |

Do **not** add `libsnapd-glib` to remove/purge/pin — that deletes the desktop.

## Build

```bash
cd /home/evanee/mygit/armbian-rp2
./compile.sh build BOARD=z96a-rk3568-laptop BRANCH=edge RELEASE=noble \
  BUILD_DESKTOP=yes DESKTOP_ENVIRONMENT=gnome \
  KERNEL_CONFIGURE=no PREFER_DOCKER=yes
```
