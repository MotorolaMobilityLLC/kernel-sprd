# SPRD Audio SIPC Support

SPRD_AUDIO_ASSISTANT_TREE := drivers/unisoc_platform/audio/sprd_audio
ccflags-y	+= -I$(srctree)/drivers/staging/sprd/modem/include/ \
		   -I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/audiomem/ \
		   -I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/agdsp_access/ \
		   -I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/include/

audio_sipc-objs		:= audio-sipc.o audio-smsg.o
obj-$(CONFIG_UNISOC_AUDIO_SIPC)	+= audio_sipc.o
obj-$(CONFIG_UNISOC_AUDIO_SIPC)	+= audio-pipe.o
