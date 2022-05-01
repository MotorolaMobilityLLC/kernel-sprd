#SPRD CODEC

SPRD_SOUND_TREE := drivers/unisoc_platform/audio/sprd
ccflags-y += -I$(SPRD_SOUND_TREE)/codec/sprd

snd-soc-sprd-codec-sc2721-objs			:= sprd-codec.o sprd-headset-sc2721.o
snd-soc-sprd-codec-sc2721-power-objs		:= sprd-audio-power.o
snd-soc-sprd-codec-sc2721-power-dev-objs	:= sprd-audio-power-dev.o
obj-$(CONFIG_SND_SOC_UNISOC_CODEC_SC2721)		+= snd-soc-sprd-codec-sc2721.o
obj-$(CONFIG_SND_SOC_UNISOC_CODEC_SC2721)		+= snd-soc-sprd-codec-sc2721-power.o
obj-$(CONFIG_SND_SOC_UNISOC_CODEC_SC2721)		+= snd-soc-sprd-codec-sc2721-power-dev.o
