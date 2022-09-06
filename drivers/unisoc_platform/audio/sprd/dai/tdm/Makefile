# SPRD TDM Support
SPRD_SOUND_TREE := drivers/unisoc_platform/audio/sprd
ccflags-y += -I$(srctree)/$(SPRD_SOUND_TREE)/ \
			 -I$(srctree)/$(SPRD_SOUND_TREE)/dai/tdm/ \
			 -I$(srctree)/$(SPRD_SOUND_TREE)/platform/
snd-soc-sprd-tdm-objs       := tdm.o
obj-$(CONFIG_SND_SOC_UNISOC_TDM)       += snd-soc-sprd-tdm.o
