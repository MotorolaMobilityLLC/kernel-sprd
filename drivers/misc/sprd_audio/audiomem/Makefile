# SPRD Audio Memory Manager Support
SPRD_SOUND_TREE                         := sound/soc/sprd/
subdir-ccflags-y                        += -I$(SPRD_SOUND_TREE)/include/

obj-$(CONFIG_SPRD_AUDIO_MEMORY_MANAGER)  += audio_mem.o

# Remove some audio log for user version by Tinno
ifneq ($(TARGET_BUILD_VARIANT),userdebug)
CFLAGS_audio_mem.o += -DCONFIG_SPRD_AUDIO_NODEBUG
endif
