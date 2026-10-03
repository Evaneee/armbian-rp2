# Z96A proven U-Boot binaries

Extracted from a known-booting image:
`Armbian-unofficial_24.2.0-trunk_Z96a-rk3568-laptop_jammy_legacy_5.10.160_xfce_desktop.img`

Rock 3A / rk35xx `next-dev-v2024.10` U-Boot does not initialize DDR/PMIC on this laptop.
These blobs (Radxa rock3 4.19 era) are packaged into the vendor image via
`build_custom_uboot__z96a_install_proven_bins`.

Required files in this directory:
- `idbloader.img`
- `u-boot.itb`
