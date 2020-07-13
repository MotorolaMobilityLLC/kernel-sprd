# SPDX-License-Identifier: GPL-2.0

sprd-gsp-y += gsp_r6p0/gsp_r6p0_core.o \
	gsp_r6p0/gsp_r6p0_coef_cal.o \
	gsp_r8p0/gsp_r8p0_core.o \
	gsp_r8p0/gsp_r8p0_coef_cal.o


sprd-gsp-y += gsp_interface/gsp_interface_sharkl3.o \
	gsp_interface/gsp_interface_sharkl5pro.o

sprd-gsp-y += gsp_core.o gsp_dev.o gsp_interface.o gsp_kcfg.o \
	 gsp_layer.o gsp_sync.o gsp_sysfs.o gsp_workqueue.o

obj-$(CONFIG_DRM_SPRD_GSP) += sprd-gsp.o
