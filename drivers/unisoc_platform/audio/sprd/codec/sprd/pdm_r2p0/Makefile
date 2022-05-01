#SPRD PDM

SPRD_SOUND_TREE := drivers/unisoc_platform/audio/sprd
SPRD_AUDIO_ASSISTANT_TREE := drivers/unisoc_platform/audio/sprd_audio
ccflags-y += -I$(srctree)/$(SPRD_SOUND_TREE)/codec/sprd \
			 -I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/audiosipc/

snd-soc-sprd-pdm-r2p0-objs   := sprd-pdm.o
obj-$(CONFIG_SND_SOC_UNISOC_PDM_R2P0)		+= snd-soc-sprd-pdm-r2p0.o
