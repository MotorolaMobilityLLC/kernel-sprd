# SPDX-License-Identifier: GPL-2.0
dtb-$(CONFIG_ARCH_SPRD) += sc9836-openphone.dtb \
			   sp9860g-1h10.dtb	\
			   sp9832e-1h10.dtb	\
			   sp9863a-1h10.dtb	\
			   sp9863a-2h10.dtb	\
			   ud710-haps.dtb	\
			   ums510-haps.dtb

dtbo-$(CONFIG_ARCH_SPRD) += \
	sp9863a-1h10-overlay.dtbo \
	sp9863a-2h10-overlay.dtbo \
	ums510-haps-overlay.dtbo

always		:= $(dtb-y) $(dtbo-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
