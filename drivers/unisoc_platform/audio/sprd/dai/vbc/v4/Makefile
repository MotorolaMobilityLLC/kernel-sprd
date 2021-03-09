# SPRD VBC Support
SPRD_SOUND_TREE := drivers/unisoc_platform/audio/sprd
SPRD_AUDIO_ASSISTANT_TREE := drivers/unisoc_platform/audio/sprd_audio
ccflags-y += -I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/audiosipc/ \
		 -I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/audiomem/ \
	     -I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/mcdt/ \
	     -I$(srctree)/$(SPRD_SOUND_TREE)/dai/ \
	     -I$(srctree)/$(SPRD_SOUND_TREE)/platform/ \
	     -I$(srctree)/$(SPRD_SOUND_TREE)/dai/vbc/

snd-soc-sprd-vbc-v4-objs       := vbc-dai.o vbc-phy-v4.o
snd-soc-sprd-vbc-fe-objs       := sprd-fe-dai.o
obj-$(CONFIG_SND_SOC_UNISOC_VBC_V4)    += snd-soc-sprd-vbc-v4.o
obj-$(CONFIG_SND_SOC_UNISOC_VBC_V4)    += snd-soc-sprd-vbc-fe.o
