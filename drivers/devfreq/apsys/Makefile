subdir-ccflags-y += -I$(src)
subdir-ccflags-y += -I$(src)/sys
obj-y += sys/
obj-y += sprd_dvfs_apsys.o

obj-y += dpu/
obj-y += sprd_dvfs_dpu.o

obj-$(CONFIG_DRM_SPRD_GSP_DVFS) += gsp/
obj-$(CONFIG_DRM_SPRD_GSP_DVFS) += sprd_dvfs_gsp.o

obj-y += vsp/
obj-y += sprd_dvfs_vsp.o

obj-y += vdsp/
obj-y += sprd_dvfs_vdsp.o
