# SPDX-License-Identifier: GPL-2.0
dtb-$(CONFIG_ARCH_SPRD) += sc9836-openphone.dtb \
			sp9860g-1h10.dtb	\
			sp9863a-1h10.dtb	\
			ums512-1h10.dtb		\
			ums9230-haps.dtb

dtbo-$(CONFIG_ARCH_SPRD) += \
			sp9863a-1h10-overlay.dtbo	\
			ums512-1h10-overlay.dtbo	\
			ums9230-haps-overlay.dtbo

always		:= $(dtb-y) $(dtbo-y)
clean-files	:= *.dtb *.dtbo
