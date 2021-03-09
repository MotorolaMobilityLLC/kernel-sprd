SPRD_AUDIO_ASSISTANT_TREE := drivers/unisoc_platform/audio/sprd_audio
ccflags-y += -I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/audiomem/ \
         -I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/audiosipc/ \
         -I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/agdsp_access/

obj-$(CONFIG_UNISOC_HW_DEVICE_DVFS_AGCP)     += sprd_audcp_dvfs.o
