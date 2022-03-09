# SPDX-License-Identifier: GPL-2.0-only

SC2332_WLAN := \
		acs.o \
		init.o \
		cmdevt.o \
		qos.o \
		scan.o \
		tcp_ack.o \
		txrx.o \
		fcc.o

obj-$(CONFIG_SC2332_SDIO_WLAN)	+= sc2332_sdio_wlan.o
sc2332_sdio_wlan-objs += \
		$(SC2332_WLAN) \
		sdio.o

sc2332_sdio_wlan-$(CONFIG_SPRD_WLAN_VENDOR_SPECIFIC)	+= vendor.o

obj-$(CONFIG_SC2332_SIPC_WLAN)	+= sc2332_sipc_wlan.o
sc2332_sipc_wlan-objs += \
		$(SC2332_WLAN) \
		sipc.o

sc2332_sipc_wlan-$(CONFIG_SPRD_WLAN_VENDOR_SPECIFIC)	+= vendor.o
