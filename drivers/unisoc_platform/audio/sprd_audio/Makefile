# SPRD audio assistant modules

SPRD_SOUND_USB_TREE				:= sound/usb
subdir-ccflags-y			+= -I$(srctree)/$(SPRD_SOUND_USB_TREE)/

obj-$(CONFIG_UNISOC_AUDIO_DSP_DUMP)		+= audiodspdump/
obj-$(CONFIG_UNISOC_AUDIO_MEMORY_MANAGER)		+= audiomem/
obj-$(CONFIG_UNISOC_AUDIO_PIPE)		+= audio_pipe/
obj-$(CONFIG_UNISOC_AUDIO_SIPC)			+= audiosipc/
obj-$(CONFIG_UNISOC_AUDIO_MCDT)			+= mcdt/mcdt_r1p0/
obj-$(CONFIG_UNISOC_AUDIO_MCDT_R2P0)	+= mcdt/mcdt_r2p0/
obj-$(CONFIG_UNISOC_HW_DEVICE_DVFS_AGCP) 		+= audiodvfs/
obj-$(CONFIG_UNISOC_AUDIO_CPBOOT)			+= audiocpboot/
obj-$(CONFIG_UNISOC_AUDIO_SAUDIO)			+= saudio/
obj-$(CONFIG_UNISOC_AUDIO_VOICE_TRIGGER)	+= voice_trigger/
obj-$(CONFIG_UNISOC_AUDIO_VOICE_TRIGGER_IRQ)	+= voice_trigger/
obj-$(CONFIG_UNISOC_AUDIO_USB_OFFLOAD)	+= usb_offload/
obj-$(CONFIG_UNISOC_AUDIO_AGDSP_PD)		+= agdsp_pd/
