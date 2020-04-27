# SPDX-License-Identifier: GPL-2.0

sprd-drm-y := \
	sprd_drm.o \
	sprd_gem.o \
	sprd_crtc.o \
	sprd_plane.o \
	disp_lib.o

sprd-drm-$(CONFIG_DRM_SPRD_DUMMY) += \
	dummy/dummy_crtc.o \
	dummy/dummy_connector.o

sprd-drm-$(CONFIG_DRM_SPRD_DPU0) += \
	sprd_dpu.o \
	dpu/dpu_r2p0.o \
	dpu/dpu_r4p0.o

sprd-drm-$(CONFIG_DRM_SPRD_DSI) += \
	sprd_dsi.o \
	sprd_dphy.o \
	sprd_dsi_panel.o \
	dsi/sprd_dsi_api.o \
	dsi/core/dsi_ctrl_r1p0.o \
	dsi/core/dsi_ctrl_r1p0_ppi.o \
	dphy/sprd_dphy_api.o \
	dphy/pll/megacores_sharkle.o \
	dphy/pll/megacores_sharkl5.o

sprd-drm-y += \
	sharkl3/global_dispc.o \
	sharkl3/global_dsi.o \
	sharkl3/global_dphy.o \
	sharkl5pro/global_dpu.o \
	sharkl5pro/global_dsi.o \
	sharkl5pro/global_dphy.o

sprd-drm-y += sysfs/sysfs_class.o

obj-$(CONFIG_DRM_SPRD) += sprd-drm.o

obj-$(CONFIG_DRM_SPRD_GSP) += gsp/
