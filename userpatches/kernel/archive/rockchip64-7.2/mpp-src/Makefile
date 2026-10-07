# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

ccflags-y += -I$(src) -I$(src)/compat_include
ccflags-y += -include $(src)/bsp_compat_shim.h
ccflags-y += -DMPP_VERSION=\"edge-mainline-port-rk3568\"
ccflags-y += -Wno-error

rk_vcodec-objs := mpp_service.o mpp_common.o mpp_iommu.o
rk_vcodec-$(CONFIG_ROCKCHIP_MPP_RKVDEC2) += mpp_rkvdec2.o mpp_rkvdec2_link.o
rk_vcodec-$(CONFIG_ROCKCHIP_MPP_VDPU2)  += mpp_vdpu2.o
rk_vcodec-$(CONFIG_ROCKCHIP_MPP_VEPU2)  += mpp_vepu2.o
rk_vcodec-$(CONFIG_ROCKCHIP_MPP_JPGDEC) += mpp_jpgdec.o
rk_vcodec-$(CONFIG_ROCKCHIP_MPP_RKVENC) += mpp_rkvenc.o

obj-$(CONFIG_ROCKCHIP_MPP_SERVICE) += rk_vcodec.o
