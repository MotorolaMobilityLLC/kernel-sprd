#SPRD CODEC

SPRD_SOUND_TREE := drivers/unisoc_platform/audio/sprd
ccflags-y += -I$(srctree)/$(SPRD_SOUND_TREE)/codec/sprd

snd-soc-sprd-codec-sc2730-objs   := sprd-codec.o sprd-headset-sc2730.o
snd-soc-sprd-codec-sc2730-power-objs		:= sprd-audio-power.o
snd-soc-sprd-codec-sc2730-power-dev-objs	:= sprd-audio-power-dev.o
obj-$(CONFIG_SND_SOC_UNISOC_CODEC_SC2730)		+= snd-soc-sprd-codec-sc2730.o
obj-$(CONFIG_SND_SOC_UNISOC_CODEC_SC2730)		+= snd-soc-sprd-codec-sc2730-power.o
obj-$(CONFIG_SND_SOC_UNISOC_CODEC_SC2730)		+= snd-soc-sprd-codec-sc2730-power-dev.o