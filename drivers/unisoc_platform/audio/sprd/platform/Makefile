# SPRD Platform Support
SPRD_AUDIO_ASSISTANT_TREE	:= drivers/unisoc_platform/audio/sprd_audio
ccflags-y	+= -I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/audiomem/ \
				-I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/audiosipc/ \
				-I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/mcdt/

obj-$(CONFIG_SND_SOC_UNISOC_AUDIO_DMAENGINE)       += sprd-dmaengine-pcm.o
obj-$(CONFIG_SND_SOC_UNISOC_AUDIO_DMAENGINE)       += sprd-compr-2stage-dma.o
obj-$(CONFIG_SND_SOC_UNISOC_AUDIO_DMAENGINE)       += sprd-platform-pcm-routing.o

obj-$(CONFIG_SND_SOC_UNISOC_AUDIO_TWO_STAGE_DMAENGINE)       += sprd-2stage-dmaengine-pcm.o
