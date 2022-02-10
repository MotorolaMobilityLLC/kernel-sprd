# SPDX-License-Identifier: GPL-2.0-only

ifeq ($(TARGET_BUILD_VARIANT),userdebug)
ccflags-y += -DCONFIG_SPRD_WLAN_DEBUGFS
endif

obj-$(CONFIG_SPRD_WLAN)	+= sprd_wlan.o
sprd_wlan-objs += \
		cfg80211.o \
		debug.o \
		delay_work.o \
		iface.o \
		msg.o \
		npi.o \
		report.o \
		tdls.o
