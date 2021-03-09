# SPRD Audio Dsp Log Support

SPRD_AUDIO_ASSISTANT_TREE := drivers/unisoc_platform/audio/sprd_audio
ccflags-y += -I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/audiomem/ \
	     -I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/audiosipc/ \
	     -I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/agdsp_access/ \
	     -I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/include

audio-dsp-dump-objs		:= audio_sblock.o audio_dsp_dump.o
obj-$(CONFIG_UNISOC_AUDIO_DSP_DUMP)	+= audio-dsp-dump.o
