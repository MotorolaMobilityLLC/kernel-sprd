# SPRD CPU DAI Support

SPRD_SOUND_TREE := drivers/unisoc_platform/audio/sprd
SPRD_AUDIO_ASSISTANT_TREE		:= drivers/unisoc_platform/audio/sprd_audio

ccflags-y += -I$(srctree)/$(SPRD_SOUND_TREE) \
			-I$(srctree)/$(SPRD_SOUND_TREE)/platform/ \
			-I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/audiosipc/

snd-soc-sprd-dai-objs := sprd-dai.o
obj-$(CONFIG_SND_SOC_UNISOC_DAI) += snd-soc-sprd-dai.o
