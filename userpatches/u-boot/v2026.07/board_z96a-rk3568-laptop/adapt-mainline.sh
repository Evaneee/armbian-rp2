#!/usr/bin/env bash
# Adapt Radxa next-dev DRM sources / headers for mainline U-Boot v2026.07.
# Safe to re-run. Pass either a U-Boot build tree or the board userpatches dir.
set -euo pipefail

TARGET="${1:-.}"
DRM="${TARGET}/drivers/video/drm"
INC="${TARGET}/include"

if [[ ! -d "${DRM}" && -d "${TARGET}/drm" ]]; then
	DRM="${TARGET}/drm"
	INC="${TARGET}/include"
fi

[[ -d "${DRM}" ]] || { echo "adapt-mainline: no drm dir under ${TARGET}" >&2; exit 1; }

echo "adapt-mainline: DRM=${DRM}"

find "${DRM}" -type f \( -name '*.c' -o -name '*.h' \) -print0 | while IFS= read -r -d '' f; do
	# Longer udevice->node patterns first (avoid eating conn->dev->node as dev->node)
	sed -i \
		-e 's/\bconn->dev->node\b/dev_ofnode(conn->dev)/g' \
		-e 's/\bcrtc_state->dev->node\b/dev_ofnode(crtc_state->dev)/g' \
		-e 's/\bpanel->dev->node\b/dev_ofnode(panel->dev)/g' \
		-e 's/\binno->dev->node\b/dev_ofnode(inno->dev)/g' \
		-e 's/\bdp->dev->node\b/dev_ofnode(dp->dev)/g' \
		-e 's/\bdev->node\b/dev_ofnode(dev)/g' \
		-e 's/\bdisk_partition_t\b/struct disk_partition/g' \
		-e 's/\bvideo_uc_platdata\b/video_uc_plat/g' \
		-e 's/\bdev_get_uclass_platdata\b/dev_get_uclass_plat/g' \
		-e 's/\bcmd_tbl_t\b/struct cmd_tbl/g' \
		"$f"

	# of_find_node_by_phandle(phandle) → (NULL, phandle); skip already-adapted
	perl -i -pe 's/\bof_find_node_by_phandle\(([^\),]+)\)/of_find_node_by_phandle(NULL, $1)/g unless /of_find_node_by_phandle\([^,]+,/;' "$f"

	# clk_set_defaults(dev) → clk_set_defaults(dev, CLK_DEFAULTS_PRE)
	perl -i -pe 's/\bclk_set_defaults\(([^\),]+)\)/clk_set_defaults($1, CLK_DEFAULTS_PRE)/g unless /clk_set_defaults\([^,]+,/;' "$f"


	# DM field renames (Radxa → mainline 2026)
	sed -i \
		-e 's/\bpriv_auto_alloc_size\b/priv_auto/g' \
		-e 's/\bplatdata_auto_alloc_size\b/plat_auto/g' \
		-e 's/\bofdata_to_platdata\b/of_to_plat/g' \
		-e 's/\bdm_spi_slave_platdata\b/dm_spi_slave_plat/g' \
		-e 's/\bdev_get_parent_platdata\b/dev_get_parent_plat/g' \
		"$f"
	# udevice.seq → dev_seq(); keep function names that contain ofdata_to_platdata
	perl -i -pe 's/\b([a-zA-Z_][a-zA-Z0-9_]*)->seq\b/dev_seq($1)/g' "$f"
	# generic_phy_set_mode(phy, mode) → add submode
	perl -i -pe 's/\bgeneric_phy_set_mode\(([^,]+),\s*([^,)]+)\)/generic_phy_set_mode($1, $2, 0)/g unless /generic_phy_set_mode\([^,]+,[^,]+,/;' "$f"
	# Radxa: dev_read_addr_size(dev, "reg", &sz) → mainline 2-arg form
	perl -i -pe 's/\bdev_read_addr_size\(([^,]+),\s*"reg",\s*/dev_read_addr_size($1, /g' "$f"
	# Other prop names → dev_read_addr_size_name
	perl -i -pe 's/\bdev_read_addr_size\(([^,]+),\s*"([^"]+)",\s*/dev_read_addr_size_name($1, "$2", /g unless /dev_read_addr_size_name/;' "$f"
done

# CUBIC LUT default (append after SPDX block, before first #include)
if [[ -f "${DRM}/rockchip_display.c" ]] && ! grep -q 'Z96A_CUBIC_LUT_COMPAT' "${DRM}/rockchip_display.c"; then
	perl -i -0pe 's|(SPDX-License-Identifier:[^\n]+\n\s*\*/\n)|$1\n/* Z96A_CUBIC_LUT_COMPAT */\n#ifndef CONFIG_ROCKCHIP_CUBIC_LUT_SIZE\n#define CONFIG_ROCKCHIP_CUBIC_LUT_SIZE 0\n#endif\n\n|s' \
		"${DRM}/rockchip_display.c"
fi

if [[ -f "${INC}/drm/drm_dp_helper.h" ]] && ! grep -q '#include <edid.h>' "${INC}/drm/drm_dp_helper.h"; then
	sed -i '/#define _DRM_DP_HELPER_H_/a\
\
#include <edid.h>
' "${INC}/drm/drm_dp_helper.h"
fi

UC="${INC}/dm/uclass-id.h"
if [[ -f "${UC}" ]] && ! grep -q 'UCLASS_VIDEO_CRTC' "${UC}"; then
	sed -i '/UCLASS_VIDEO_OSD,/a\
	UCLASS_VIDEO_CRTC,	/* Rockchip DRM CRTC (Radxa port) */
' "${UC}"
	echo "adapt-mainline: added UCLASS_VIDEO_CRTC to ${UC}"
fi

# Also patch build-tree uclass-id if adapting a full U-Boot tree
UC2="${TARGET}/include/dm/uclass-id.h"
if [[ "${UC2}" != "${UC}" && -f "${UC2}" ]] && ! grep -q 'UCLASS_VIDEO_CRTC' "${UC2}"; then
	sed -i '/UCLASS_VIDEO_OSD,/a\
	UCLASS_VIDEO_CRTC,	/* Rockchip DRM CRTC (Radxa port) */
' "${UC2}"
	echo "adapt-mainline: added UCLASS_VIDEO_CRTC to ${UC2}"
fi

COMPAT="${INC}/z96a_drm_compat.h"
cat > "${COMPAT}" <<'EOF'
/* SPDX-License-Identifier: GPL-2.0+ */
/* Shims for Radxa next-dev DRM on mainline U-Boot v2026.07 */
#ifndef Z96A_DRM_COMPAT_H
#define Z96A_DRM_COMPAT_H

#ifndef CONFIG_ROCKCHIP_CUBIC_LUT_SIZE
#define CONFIG_ROCKCHIP_CUBIC_LUT_SIZE 0
#endif

#define video_uc_platdata video_uc_plat
#ifndef dev_get_uclass_platdata
#define dev_get_uclass_platdata dev_get_uclass_plat
#endif

/* Radxa DRM uses pre-DM-rename helper; mainline has dev_has_ofnode(). */
#ifndef dev_of_valid
#define dev_of_valid(dev) dev_has_ofnode(dev)
#endif

#endif /* Z96A_DRM_COMPAT_H */
EOF

# common.h is maintained in userpatches; do not append edid.h here (clashes with panel.h).
if [[ -f "${INC}/common.h" ]] && ! grep -q 'z96a_drm_compat.h' "${INC}/common.h"; then
	cat >> "${INC}/common.h" <<'EOF'

#ifndef __ASSEMBLY__
#include <z96a_drm_compat.h>
#endif
EOF
fi

echo "adapt-mainline: done"
