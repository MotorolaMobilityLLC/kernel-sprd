
dcam_r5p0-objs := src/dcam_drv.o\
		src/dcam_int.o\
		src/dcam_cap.o\
		src/dcam_full_path.o\
		src/dcam_bin_path.o\
		src/dcam_raw_sizer.o \
		src/dcam_fetch.o

isp_r6p12-objs := src/isp_drv.o \
		src/isp_slice.o \
		src/isp_path.o \
		src/isp_cfg_param.o \
		src/isp_int.o \
		src/isp_buf.o \
		src/isp_cfg.o \
		src/isp_3dnr_cap.o \
		src/isp_3dnr_drv.o

obj-$(CONFIG_COMPAT) = src/cam_drv_compat.o

CAM_SRC = $(wildcard $(srctree)/$(src)/block/*.c)
CAM_DIR = $(notdir $(CAM_SRC))
CAM_OBJ = $(patsubst %c, block/%o, $(CAM_DIR))

obj-y += isp_r6p12.o $(CAM_OBJ)
obj-y += dcam_r5p0.o
obj-y += src/cam_core.o \
		src/cam_queue.o \
		src/cam_statistic.o \
		src/cam_buf.o \
		src/cam_common.o \
		src/cam_flash.o \
		src/cam_gen_scale_coef.o \
		src/cam_sin_cos.o \
		src/cam_debug.o

obj-$(CONFIG_SPRD_CAMERA_CSI_RECEIVER_R2P0) += src/cam_pw_domain.o

ccflags-y += 	-I$(srctree)/drivers/misc/sprd_camera/dcam_if_r5p0_isp_r6p12/inc \
		-I$(srctree)/drivers/misc/sprd_camera/sensor \
		-I$(srctree)/drivers/misc/sprd_camera/flash \
		-I$(srctree)/drivers/staging/android/ion

ifeq ($(CONFIG_SPRD_CAMERA_CSI_RECEIVER_R2P0),y)
ccflags-y += -I$(srctree)/drivers/misc/sprd_camera/sensor/csi2/csi_sprd/receiver_r2p0
else ifeq ($(CONFIG_SPRD_CAMERA_CSI_R2P0V2),y)
ccflags-y += -I$(srctree)/drivers/misc/sprd_camera/sensor/csi2/csi_sprd/r2p0v2
else ifeq ($(CONFIG_SPRD_CAM_PW_DOMAIN_R5P0),y)
ccflags-y += -I$(srctree)/drivers/misc/sprd_camera/common/r5p0
endif
