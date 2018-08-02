ccflags-y += -Iinclude/drm

subdir-ccflags-y += -I$(src)

obj-y := sprd_drm.o \
	sprd_dpu.o \
	sprd_dsi.o \
	sprd_dphy.o \
	sprd_panel.o

obj-y += disp_lib.o

obj-y += dpu/
obj-y += dsi/
obj-y += dphy/

#obj-y += sharkl3/

obj-$(CONFIG_DRM_SPRD_GSP) += gsp/
