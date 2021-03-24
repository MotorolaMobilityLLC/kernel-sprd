# SPRD VBC Support
SPRD_SOUND_TREE := sound/soc/sprd/
SPRD_AUDIO_ASSISTANT_TREE := drivers/misc/sprd_audio/
SPRD_MCDT_TREE := drivers/misc/sprd_audio/mcdt/
ccflags-y += -I$(SPRD_AUDIO_ASSISTANT_TREE)/audiosipc \
		-I$(SPRD_AUDIO_ASSISTANT_TREE)/audiomem \
	     -I$(SPRD_SOUND_TREE)/dai \
	     -I$(SPRD_SOUND_TREE)/platform \
	     -I$(SPRD_SOUND_TREE)/dai/vbc/ \
	     -I$(SPRD_MCDT_TREE)

snd-soc-sprd-vbc-v4-objs       := vbc-dai.o vbc-phy-v4.o sprd-fe-dai.o
obj-$(CONFIG_SND_SOC_SPRD_VBC_V4)    += snd-soc-sprd-vbc-v4.o

# Remove some audio log for user version by Tinno
ifneq ($(TARGET_BUILD_VARIANT),userdebug)
CFLAGS_vbc-dai.o += -DCONFIG_SPRD_AUDIO_NODEBUG
CFLAGS_vbc-phy-v4.o += -DCONFIG_SPRD_AUDIO_NODEBUG
CFLAGS_sprd-fe-dai.o += -DCONFIG_SPRD_AUDIO_NODEBUG
endif
