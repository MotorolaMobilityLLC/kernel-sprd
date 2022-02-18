# SPDX-License-Identifier: GPL-2.0

ccflags-y := -I $(srctree)/$(src)

sprd-drm-$(CONFIG_UNISOC_DISP) := \
	sprd_drm.o \
	sprd_gem.o \
	sprd_crtc.o \
	sprd_plane.o \
	disp_lib.o

sprd-drm-$(CONFIG_UNISOC_DISP) += \
	dummy/dummy_crtc.o \
	dummy/dummy_connector.o

sprd-drm-$(CONFIG_UNISOC_DISP) += \
	sprd_dpu.o \
	sprd_bl.o \
	sprd_corner.o \
	dpu/dpu_r4p0.o

sprd-drm-$(CONFIG_UNISOC_DISP) += \
	sprd_dsi.o \
	sprd_dphy.o \
	sprd_dsi_panel.o \
	dsi/sprd_dsi_api.o \
	dsi/core/dsi_ctrl_r1p0.o \
	dsi/core/dsi_ctrl_r1p0_ppi.o \
	dphy/sprd_dphy_api.o \
	dphy/pll/megacores_sharkle.o \
	dphy/pll/megacores_sharkl5.o

sprd-drm-$(CONFIG_UNISOC_DISP) += \
	sharkl5pro/global_dpu.o \
	sharkl5pro/global_dsi.o \
	sharkl5pro/global_dphy.o \


sprd-drm-$(CONFIG_UNISOC_DISP) += \
	sysfs/sysfs_class.o \
	sysfs/sysfs_dpu.o \
	sysfs/sysfs_dphy.o \
	sysfs/sysfs_dsi.o \
	sysfs/sysfs_panel.o

obj-$(CONFIG_UNISOC_DISP) += sprd-drm.o

#sprd-drm-y += sprd_ioc32.o
