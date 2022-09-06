# SPRD I2S Support
SPRD_SOUND_TREE := drivers/unisoc_platform/audio/sprd
ccflags-y += -I$(srctree)/$(SPRD_SOUND_TREE)/ \
			 -I$(srctree)/$(SPRD_SOUND_TREE)/dai/i2s/ \
			 -I$(srctree)/$(SPRD_SOUND_TREE)/platform/

snd-soc-sprd-i2s-objs       := i2s.o
obj-$(CONFIG_SND_SOC_UNISOC_I2S)       += snd-soc-sprd-i2s.o
