# SPDX-License-Identifier: GPL-2.0

ifeq ($(BSP_BUILD_DT_OVERLAY),y)

ifeq ($(BSP_BUILD_ANDROID_OS),y)

ifeq ($(BSP_BUILD_FAMILY),sharkl3)
dtbo-$(CONFIG_ARCH_SPRD) += \
    sp9863a-1h10-overlay.dtbo \
    sp9863a-1h10_go-overlay.dtbo

sp9863a-1h10-overlay.dtbo-base := sp9863a.dtb
sp9863a-1h10_go-overlay.dtbo-base := sp9863a.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),sharkl5Pro)
dtbo-$(CONFIG_ARCH_SPRD) += ums512-1h10-overlay.dtbo
dtbo-$(CONFIG_ARCH_SPRD) += ums512-1h10-go-overlay.dtbo

ums512-1h10-overlay.dtbo-base := ums512.dtb
ums512-1h10-go-overlay.dtbo-base := ums512.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),sharkl5)
dtbo-$(CONFIG_ARCH_SPRD) += \
	ums312-2h10-overlay.dtbo \
	ums312-2h10-go-overlay.dtbo

ums312-2h10-overlay.dtbo-base := ums312.dtb
ums312-2h10-go-overlay.dtbo-base := ums312.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),qogirl6)
dtbo-$(CONFIG_ARCH_SPRD) += \
	ums9230-1h10-overlay.dtbo \
	ums9230-1h10_go-overlay.dtbo \
	ums9230-4h10-overlay.dtbo \
	ums9230-4h10_go-overlay.dtbo

ums9230-1h10-overlay.dtbo-base := ums9230.dtb
ums9230-1h10_go-overlay.dtbo-base := ums9230.dtb
ums9230-4h10-overlay.dtbo-base := ums9230.dtb
ums9230-4h10_go-overlay.dtbo-base := ums9230.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),qogirn6pro)
dtbo-$(CONFIG_ARCH_SPRD) += \
	ums9620-1h10-overlay.dtbo \
	ums9620-2h10-overlay.dtbo

ums9620-1h10-overlay.dtbo-base := ums9620.dtb
ums9620-2h10-overlay.dtbo-base := ums9620.dtb
endif

endif #//BSP_BUILD_ANDROID_OS

else #//For AiAiAi or NO DTBO
dtb-$(CONFIG_ARCH_SPRD) += \
		sc9863a-w12v1.dtb \
		ums512-w27v1.dtb

dtb-$(CONFIG_ARCH_SPRD) += \
	sp9863a.dtb \
	ums512.dtb \
	ums312.dtb \
	ums9230.dtb \
	ums9620.dtb

dtbo-$(CONFIG_ARCH_SPRD) += \
	sp9863a-1h10-overlay.dtbo \
	sp9863a-1h10_go-overlay.dtbo \
	ums512-1h10-overlay.dtbo \
	ums512-1h10-go-overlay.dtbo \
	ums312-2h10-overlay.dtbo \
	ums312-2h10-go-overlay.dtbo \
	ums9230-1h10-overlay.dtbo \
	ums9230-1h10_go-overlay.dtbo \
	ums9230-4h10_go-overlay.dtbo \
	ums9230-4h10-overlay.dtbo \
	ums9620-1h10-overlay.dtbo \
	ums9620-2h10-overlay.dtbo

endif #//BSP_BUILD_DT_OVERLAY

always		:= $(dtb-y) $(dtbo-y)
clean-files	:= *.dtb *.dtbo
