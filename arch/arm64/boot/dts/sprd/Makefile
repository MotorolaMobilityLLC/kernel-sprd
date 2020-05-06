# SPDX-License-Identifier: GPL-2.0

ifeq ($(BSP_BUILD_DT_OVERLAY),y)

ifeq ($(BSP_BUILD_ANDROID_OS),y)

ifeq ($(BSP_BUILD_FAMILY),sharkl3)
dtbo-$(CONFIG_ARCH_SPRD) += sp9863a-1h10-overlay.dtbo

sp9863a-1h10-overlay.dtbo-base := sp9863a.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),sharkl5Pro)
dtbo-$(CONFIG_ARCH_SPRD) += ums512-1h10-overlay.dtbo

ums512-1h10-overlay.dtbo-base := ums512.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),qogirl6)
dtbo-$(CONFIG_ARCH_SPRD) += ums9230-haps-overlay.dtbo \
			ums9230-haps-flash-overlay.dtbo

ums9230-haps-overlay.dtbo-base := ums9230.dtb
ums9230-haps-flash-overlay.dtbo-base := ums9230.dtb
endif

endif #//BSP_BUILD_ANDROID_OS

else
dtb-$(CONFIG_ARCH_SPRD) += sc9836-openphone.dtb \
				sp9860g-1h10.dtb        \
				sp9863a-1h10.dtb        \
				ums512-1h10.dtb         \
				ums9230-haps.dtb

endif #//BSP_BUILD_DT_OVERLAY

always		:= $(dtb-y) $(dtbo-y)
clean-files	:= *.dtb *.dtbo
