# SPDX-License-Identifier: GPL-2.0

sprd-gsp-$(CONFIG_DRM_SPRD_GSP) += gsp_r6p0/gsp_r6p0_core.o \
	gsp_r6p0/gsp_r6p0_coef_cal.o \
	gsp_lite_r2p0/gsp_lite_r2p0_core.o \
	gsp_lite_r2p0/gsp_lite_r2p0_coef_cal.o \
	gsp_lite_r3p0/gsp_lite_r3p0_core.o \
	gsp_lite_r3p0/gsp_lite_r3p0_coef_cal.o \
	gsp_lite_r4p0/gsp_lite_r4p0_core.o \
	gsp_lite_r4p0/gsp_lite_r4p0_coef_cal.o \
	gsp_r8p0/gsp_r8p0_core.o \
	gsp_r8p0/gsp_r8p0_coef_cal.o \
	gsp_r9p0/gsp_r9p0_core.o \
	gsp_r9p0/gsp_r9p0_coef_cal.o \
	gsp_r9p0/gsp_r9p0_dvfs.o

sprd-gsp-$(CONFIG_DRM_SPRD_GSP) += gsp_interface/gsp_interface_sharkl3.o \
	gsp_interface/gsp_interface_sharkle.o \
	gsp_interface/gsp_interface_pike2.o \
    gsp_interface/gsp_interface_sharkl5.o \
	gsp_interface/gsp_interface_sharkl5pro.o \
	gsp_interface/gsp_interface_qogirl6.o \
	gsp_interface/gsp_interface_qogirn6pro.o

sprd-gsp-$(CONFIG_DRM_SPRD_GSP) += gsp_core.o gsp_dev.o gsp_interface.o gsp_kcfg.o \
	 gsp_layer.o gsp_sync.o gsp_sysfs.o gsp_workqueue.o

obj-$(CONFIG_DRM_SPRD_GSP) += sprd-gsp.o
#sprd-gsp-$(CONFIG_DRM_SPRD_GSP_IPC_TRUSTY) += gsp_ipc_trusty.o
