# SPRD Audio SIPC Support

SPRD_SND_TREE   := sound/soc/sprd/
SPRD_AUDIO_ASSISTANT_TREE := drivers/misc/sprd_audio/
ccflags-y	+= -Idrivers/staging/sprd/modem/include/ \
		   -I$(SPRD_AUDIO_ASSISTANT_TREE)/audiomem/ \
		   -I$(SPRD_SND_TREE)/include/

obj-$(CONFIG_SPRD_AUDIO_SIPC) += audio-sipc.o audio-smsg.o audio-pipe.o

# Remove some audio log for user version by Tinno
ifneq ($(TARGET_BUILD_VARIANT),userdebug)
CFLAGS_audio-sipc.o += -DCONFIG_SPRD_AUDIO_NODEBUG
CFLAGS_audio-smsg.o += -DCONFIG_SPRD_AUDIO_NODEBUG
CFLAGS_audio-pipe.o += -DCONFIG_SPRD_AUDIO_NODEBUG
endif
