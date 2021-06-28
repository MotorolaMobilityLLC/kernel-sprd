# SPRD Sound Card Support

SPRD_SOUND_TREE				:= drivers/unisoc_platform/audio/sprd
SPRD_AUDIO_ASSISTANT_TREE		:= drivers/unisoc_platform/audio/sprd_audio
ccflags-y				+= -I$(srctree)/$(SPRD_SOUND_TREE)/codec/sprd/
subdir-ccflags-y			+= -I$(srctree)/$(SPRD_SOUND_TREE)/include/ \
					   -I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/agdsp_access/

obj-$(CONFIG_SND_SOC_UNISOC_AUDIO_DMAENGINE)	+= platform/
obj-$(CONFIG_SND_SOC_UNISOC_AUDIO_TWO_STAGE_DMAENGINE)	+= platform/
obj-$(CONFIG_SND_SOC_UNISOC_VBC_V4)		+= dai/vbc/v4/
obj-$(CONFIG_SND_SOC_UNISOC_VBC_R1P0V3)		+= dai/vbc/r1p0v3/
obj-$(CONFIG_SND_SOC_UNISOC_VAUDIO)		+= dai/vaudio/
obj-$(CONFIG_SND_SOC_UNISOC_I2S)			+= dai/i2s/
obj-$(CONFIG_SND_SOC_UNISOC_CODEC_DUMMY)		+= codec/dummy-codec/
obj-$(CONFIG_SND_SOC_UNISOC_CODEC_SC2730)		+= codec/sprd/sc2730/
obj-$(CONFIG_SND_SOC_UNISOC_PA_UCP1301)		+= codec/sprd/ucp1301/
obj-$(CONFIG_SND_SOC_UNISOC_CODEC_SC2721)		+= codec/sprd/sc2721/
obj-$(CONFIG_SND_SOC_UNISOC_PDM_R2P0)		+= codec/sprd/pdm_r2p0/
obj-$(CONFIG_SND_SOC_UNISOC_CODEC_UMP9620)		+= codec/sprd/ump9620/

# Machines
snd-soc-sprd-card-objs		:= sprd-asoc-common.o sprd-asoc-card-utils.o \
				   sprd-asoc-card-utils-legacy.o \
				   sprd-asoc-card-utils-hook.o \
				   vbc-rxpx-codec-sc27xx.o
obj-$(CONFIG_SND_SOC_UNISOC_VBC_V4_SC2730)	+= snd-soc-sprd-card.o
obj-$(CONFIG_SND_SOC_UNISOC_R1P0V3_SC2721)	+= snd-soc-sprd-card.o
obj-$(CONFIG_SND_SOC_UNISOC_VBC_V4_SC2730_MCDT_R2P0)	+= snd-soc-sprd-card.o
obj-$(CONFIG_SND_SOC_UNISOC_VBC_V4_UMP9620_MCDT_R2P0)	+= snd-soc-sprd-card.o

snd-soc-sprd-i2s-card-objs		:= i2s-r0p0-dummy-codec.o
obj-$(CONFIG_SND_SOC_UNISOC_I2S_DUMMY)		+= snd-soc-sprd-i2s-card.o
