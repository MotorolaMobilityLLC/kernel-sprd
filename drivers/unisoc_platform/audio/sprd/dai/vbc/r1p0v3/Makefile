# SPRD VBC Support
SPRD_SOUND_TREE := drivers/unisoc_platform/audio/sprd
ccflags-y += -I$(srctree)/$(SPRD_SOUND_TREE)/ \
			 -I$(srctree)/$(SPRD_SOUND_TREE)/dai/ \
			 -I$(srctree)/$(SPRD_SOUND_TREE)/dai/vbc/ \
			 -I$(srctree)/$(SPRD_SOUND_TREE)/platform/

snd-soc-sprd-vbc-r1p0v3-objs       := vbc.o
obj-$(CONFIG_SND_SOC_UNISOC_VBC_R1P0V3)    += snd-soc-sprd-vbc-r1p0v3.o
