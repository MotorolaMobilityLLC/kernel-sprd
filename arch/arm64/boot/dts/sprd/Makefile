# SPDX-License-Identifier: GPL-2.0

ifeq ($(BSP_BUILD_DT_OVERLAY),y)

ifeq ($(BSP_BUILD_ANDROID_OS),y)

ifeq ($(BSP_BUILD_FAMILY),sharkl3)
dtbo-$(CONFIG_ARCH_SPRD) += \
	sp9863a-3h10-overlay.dtbo \
	sp9863a-1c10-overlay.dtbo \
	sp9863a-1h10-overlay.dtbo

sp9863a-3h10-overlay.dtbo-base := sp9863a.dtb
sp9863a-1c10-overlay.dtbo-base := sp9863a.dtb
sp9863a-1h10-overlay.dtbo-base := sp9863a.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),sharkl5Pro)
dtbo-$(CONFIG_ARCH_SPRD) += \
	ums512-1h10-overlay.dtbo

ums512-1h10-overlay.dtbo-base := ums512.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),sharkl5)
dtbo-$(CONFIG_ARCH_SPRD) += \
	ums312-1h10-overlay.dtbo

ums312-1h10-overlay.dtbo-base := ums312.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),qogirl6)
dtbo-$(CONFIG_ARCH_SPRD) += ums9230-haps-backdoor-overlay.dtbo \
	ums9230-haps-flash-overlay.dtbo

ums9230-haps-backdoor-overlay.dtbo-base := ums9230-haps.dtb
ums9230-haps-flash-overlay.dtbo-base := ums9230-haps.dtb
endif

endif #//BSP_BUILD_ANDROID_OS

else #//AiAiAi or No DTBO
dtb-$(CONFIG_ARCH_SPRD) += \
	sc9863a-w12v1.dtb   \
	ums512-w27v1.dtb \
	sp9863a.dtb \
	ums512.dtb \
	ums312.dtb \
	ums9230-haps.dtb
dtbo-$(CONFIG_ARCH_SPRD) += \
	sp9863a-3h10-overlay.dtbo \
	sp9863a-1c10-overlay.dtbo \
	sp9863a-1h10-overlay.dtbo \
	ums512-1h10-overlay.dtbo \
	ums312-1h10-overlay.dtbo \
	ums9230-haps-backdoor-overlay.dtbo \
	ums9230-haps-flash-overlay.dtbo

endif #//BSP_BUILD_DT_OVERLAY

always		:= $(dtb-y) $(dtbo-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
