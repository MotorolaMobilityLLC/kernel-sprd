#SPRD CODEC

SPRD_SOUND_TREE := drivers/unisoc_platform/audio/sprd
ccflags-y += -I$(srctree)/$(SPRD_SOUND_TREE)/codec/sprd

snd-soc-sprd-codec-ump9620-objs   := sprd-codec.o sprd-headset-ump9620.o
snd-soc-sprd-codec-ump9620-power-objs		:= sprd-audio-power.o
snd-soc-sprd-codec-ump9620-power-dev-objs	:= sprd-audio-power-dev.o
obj-$(CONFIG_SND_SOC_UNISOC_CODEC_UMP9620)		+= snd-soc-sprd-codec-ump9620.o
obj-$(CONFIG_SND_SOC_UNISOC_CODEC_UMP9620)		+= snd-soc-sprd-codec-ump9620-power.o
obj-$(CONFIG_SND_SOC_UNISOC_CODEC_UMP9620)		+= snd-soc-sprd-codec-ump9620-power-dev.o
