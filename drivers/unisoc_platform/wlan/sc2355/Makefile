# SPDX-License-Identifier: GPL-2.0-only

ifeq ($(TARGET_BUILD_VARIANT),userdebug)
ccflags-y += -DCONFIG_SPRD_WCN_DEBUG
endif

SC2355_WLAN := \
		acs.o \
		api_version.o \
		cmdevt.o \
		defrag.o \
		hw_param.o \
		init.o \
		mm.o \
		qos.o \
		reorder.o \
		rx.o \
		scan.o \
		tcp_ack.o \
		tx.o \
		fcc.o

obj-$(CONFIG_SC2355_SDIO_WLAN)	+= sc2355_sdio_wlan.o
sc2355_sdio_wlan-objs += \
		$(SC2355_WLAN) \
		sdio.o

sc2355_sdio_wlan-$(CONFIG_SPRD_WLAN_VENDOR_SPECIFIC)	+= vendor.o
sc2355_sdio_wlan-$(CONFIG_SC2355_WLAN_NAN)		+= nan.o
sc2355_sdio_wlan-$(CONFIG_SC2355_WLAN_RTT)		+= rtt.o

obj-$(CONFIG_SC2355_PCIE_WLAN)	+= sc2355_pcie_wlan.o
sc2355_pcie_wlan-objs += \
		$(SC2355_WLAN) \
		pcie.o  \
		pcie_buf.o

sc2355_pcie_wlan-$(CONFIG_SPRD_WLAN_VENDOR_SPECIFIC)	+= vendor.o
sc2355_pcie_wlan-$(CONFIG_SC2355_WLAN_NAN)		+= nan.o
sc2355_pcie_wlan-$(CONFIG_SC2355_WLAN_RTT)		+= rtt.o
