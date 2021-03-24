# SPRD Audio AGDSP Access Support

ccflags-y += -Idrivers/staging/sprd/modem/include/
obj-$(CONFIG_SPRD_AUDIO_AGDSP_ACCESS) += agdsp_access.o

# Remove some audio log for user version by Tinno
ifneq ($(TARGET_BUILD_VARIANT),userdebug)
CFLAGS_agdsp_access.o += -DCONFIG_SPRD_AUDIO_NODEBUG
endif
