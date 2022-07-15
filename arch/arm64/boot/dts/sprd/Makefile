# SPDX-License-Identifier: GPL-2.0

ifeq ($(BSP_BUILD_DT_OVERLAY),y)

ifeq ($(BSP_BUILD_ANDROID_OS),y)

ifeq ($(BSP_BUILD_FAMILY),sharkl3)
dtb-$(CONFIG_ARCH_SPRD) += \
    sp9863a-1h10-overlay.dtb \
    sp9863a-1h10_go-overlay.dtb

#sp9863a-1h10-overlay.dtbo-base := sp9863a.dtb
#sp9863a-1h10_go-overlay.dtbo-base := sp9863a.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),sharkl5Pro)

dtb-$(CONFIG_ARCH_SPRD) += ums512-1h10-overlay.dtbo
dtb-$(CONFIG_ARCH_SPRD) += ums512-1h10-go-overlay.dtbo

ums512-1h10-overlay.dtbo-base := ums512-base.dtb
ums512-1h10-go-overlay.dtbo-base := ums512-base.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),sharkl5)
dtb-$(CONFIG_ARCH_SPRD) += \
	ums312-2h10-overlay.dtb \
	ums312-2h10-go-overlay.dtb

#ums312-2h10-overlay.dtbo-base := ums312.dtb
#ums312-2h10-go-overlay.dtbo-base := ums312.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),qogirl6)
dtb-$(CONFIG_ARCH_SPRD) += \
	ums9230-1h10-overlay.dtb \
	ums9230-1h10_go-overlay.dtb \
	ums9230-4h10-overlay.dtb \
	ums9230-4h10_go-overlay.dtb

#ums9230-1h10-overlay.dtbo-base := ums9230.dtb
#ums9230-1h10_go-overlay.dtbo-base := ums9230.dtb
#ums9230-4h10-overlay.dtbo-base := ums9230.dtb
#ums9230-4h10_go-overlay.dtbo-base := ums9230.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),qogirn6pro)
dtb-$(CONFIG_ARCH_SPRD) += \
	ums9620-1h10-overlay.dtbo \
	ums9620-2h10-overlay.dtbo

ums9620-1h10-overlay.dtbo-base := ums9620-base.dtb
ums9620-2h10-overlay.dtbo-base := ums9620-base.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),qogirn6l)
dtb-$(CONFIG_ARCH_SPRD) += \
    ums9610-haps-overlay.dtbo \
    ums9610-zebu-overlay.dtbo

ums9610-haps-overlay.dtbo-base := ums9610-base.dtb
ums9610-zebu-overlay.dtbo-base := ums9610-base.dtb
endif

endif #//BSP_BUILD_ANDROID_OS

else #//For UPSTREAM or AiAiAi

dtb-$(CONFIG_ARCH_SPRD) += sc9836-openphone.dtb \
			sp9860g-1h10.dtb	\
			sp9863a-1h10.dtb	\
			ums512-1h10.dtb

endif #//BSP_BUILD_DT_OVERLAY
