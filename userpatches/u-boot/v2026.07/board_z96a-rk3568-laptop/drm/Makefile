#
ccflags-y := -I$(src)/include $(ccflags-y)
# Z96A minimal Rockchip DRM (from Radxa next-dev-v2024.10)
# SPDX-License-Identifier: GPL-2.0+
#

ifndef CONFIG_SPL_BUILD
obj-y += drm_modes.o
obj-y += rockchip_display.o rockchip_display_helper.o rockchip_crtc.o
obj-y += rockchip_phy.o rockchip_bridge.o
obj-y += rockchip_vop.o rockchip_vop_reg.o rockchip_vop2.o
obj-y += bmp_helper.o libnsbmp.o
obj-y += rockchip_connector.o rockchip_post_csc.o
obj-y += drm_of.o

obj-$(CONFIG_DRM_DP_HELPER) += drm_dp_helper.o
obj-$(CONFIG_DRM_ROCKCHIP_ANALOGIX_DP) += analogix_dp.o analogix_dp_reg.o
obj-$(CONFIG_DRM_ROCKCHIP_INNO_VIDEO_COMBO_PHY) += inno_video_combo_phy.o
obj-$(CONFIG_DRM_ROCKCHIP_PANEL) += rockchip_panel.o
obj-y += z96a_drm_stubs.o
endif
