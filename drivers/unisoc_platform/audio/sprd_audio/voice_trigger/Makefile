# SPRD Voice Trigger Support
SPRD_AUDIO_ASSISTANT_TREE := drivers/unisoc_platform/audio/sprd_audio
ccflags-y	+= -I$(srctree)/$(SPRD_AUDIO_ASSISTANT_TREE)/agdsp_access/

obj-$(CONFIG_UNISOC_AUDIO_VOICE_TRIGGER)  += voice_trigger_node.o
obj-$(CONFIG_UNISOC_AUDIO_VOICE_TRIGGER_IRQ)  += voice_trigger_irq.o
