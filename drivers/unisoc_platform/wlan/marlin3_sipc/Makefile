# SPDX-License-Identifier: GPL-2.0-only
ccflags-y += -DSC2355_FTR -DSC2355_TX_FILTER -DSC2355_QOS_MAP
ccflags-y += -DIBSS_SUPPORT -DIBSS_RSN_SUPPORT
ccflags-y += -DNAN_SUPPORT
ccflags-y += -DRTT_SUPPORT
#ccflags-y += -DSC2355_RX_NO_LOOP
ccflags-y += -DACS_SUPPORT -DSC2355_HW_CSUM
ccflags-y += -DWMMAC_WFA_CERTIFICATION
ccflags-y += -DCOMPAT_SAMPILE_CODE
ccflags-y += -DRND_MAC_SUPPORT
#ccflags-y += -DSOFTAP_HOOK
ccflags-y += -DATCMD_ASSERT
ccflags-y += -DTCPACK_DELAY_SUPPORT
ccflags-y += -DSIPC_SUPPORT
#ifeq ($(strip $(BSP_BUILD_VARIANT)),userdebug)
ccflags-y += -DCONFIG_SPRD_WCN_DEBUG
#endif
ccflags-y += -DSPLIT_STACK

obj-$(CONFIG_SC2355_SIPC_WLAN)	+= sc2355_sipc_wlan.o
sc2355_sipc_wlan-objs += \
		main.o cfg80211.o txrx.o cmdevt.o npi.o msg.o \
		work.o vendor.o tcp_ack.o mm.o reorder.o \
		core_sc2355.o tx_msg_sc2355.o rx_msg_sc2355.o \
		if_sc2355.o qos.o edma_test.o defrag.o ibss.o \
		nan.o tracer.o rf_marlin3.o rtt.o api_version.o \
		rnd_mac_addr.o debug.o txrx_buf_mm.o \
		sipc_txrx_mm.o sipc_debug.o
