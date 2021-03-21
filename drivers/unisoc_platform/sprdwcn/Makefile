subdir-ccflags-y := \
	-I $(srctree)/$(src)/include \
	-I $(srctree)/$(src)/boot \
	-I $(srctree)/$(src)/platform \
	-I $(srctree)/$(src)/pcie \
	-I $(srctree)/$(src)/platform/rf \
	-I $(srctree)/$(src)/sleep \
	-I $(srctree)/$(src)/sipc

#WCN MODULE
obj-$(CONFIG_UNISOC_WCN_BSP) += wcn_bsp.o
wcn_bsp-y += wcn_bus.o

#PLATFORM FILE
wcn_bsp-y += platform/wcn_parn_parser.o
wcn_bsp-y += platform/wcn_procfs.o
wcn_bsp-y += platform/wcn_txrx.o
#wcn_bsp-y += platform/rdc_debug.o
wcn_bsp-y += platform/wcn_log.o
wcn_bsp-y += platform/wcn_misc.o
wcn_bsp-y += platform/bufring.o
wcn_bsp-$(CONFIG_WCN_BOOT) += platform/wcn_op.o
wcn_bsp-y += platform/sysfs.o
wcn_bsp-y += platform/reset.o
wcn_bsp-y += platform/reset_test.o
wcn_bsp-y += platform/loopcheck.o
wcn_bsp-$(CONFIG_WCN_INTEG) += platform/rf/rf.o
wcn_bsp-$(CONFIG_WCN_INTEG) += platform/wcn_dump_integrate.o
wcn_bsp-$(CONFIG_WCN_BOOT) += platform/wcn_boot.o
wcn_bsp-$(CONFIG_WCN_BOOT) += platform/gnss/gnss.o
wcn_bsp-$(CONFIG_WCN_BOOT) += platform/wcn_dump.o
#wcn_bsp-$(CONFIG_WCN_PCIE) += platform/pcie_boot.o
wcn_bsp-$(CONFIG_WCN_SWD_DAP) += platform/wcn_swd_dap.o
wcn_bsp-$(CONFIG_SDIOHAL) += platform/mem_pd_mgr.o
wcn_bsp-$(CONFIG_WCN_UTILS) += platform/wcn_debugfs.o
wcn_bsp-$(CONFIG_WCN_SLP) += sleep/sdio_int.o
wcn_bsp-$(CONFIG_WCN_SLP) += sleep/slp_mgr.o
wcn_bsp-$(CONFIG_WCN_SLP) += sleep/slp_sdio.o
wcn_bsp-$(CONFIG_WCN_SLP) += sleep/slp_test.o

#BOOT FILE
wcn_bsp-$(CONFIG_WCN_INTEG) += boot/wcn_integrate.o
wcn_bsp-$(CONFIG_WCN_INTEG) += boot/wcn_integrate_dev.o
wcn_bsp-$(CONFIG_WCN_INTEG) += boot/wcn_integrate_boot.o

#SIPC FILE
wcn_bsp-$(CONFIG_WCN_SIPC) += sipc/wcn_sipc.o

#PCIE FILE
#wcn_bsp-$(CONFIG_WCN_PCIE) += pcie/pcie.o
#wcn_bsp-$(CONFIG_WCN_PCIE) += pcie/edma_engine.o
#wcn_bsp-$(CONFIG_WCN_PCIE) += pcie/pcie_dbg.o
#wcn_bsp-$(CONFIG_WCN_PCIE) += pcie/ioctl.o
#wcn_bsp-$(CONFIG_WCN_PCIE) += pcie/mchn.o
#wcn_bsp-$(CONFIG_WCN_PCIE) += pcie/edma_test.o
#wcn_bsp-$(CONFIG_WCN_PCIE) += pcie/pcie_attach_bus.o
#wcn_bsp-$(CONFIG_WCN_PCIE) += pcie/pcie_pm.o

#SDIO FILE
wcn_bsp-$(CONFIG_SDIOHAL) += sdio/sdiohal_main.o
wcn_bsp-$(CONFIG_SDIOHAL) += sdio/sdiohal_common.o
wcn_bsp-$(CONFIG_SDIOHAL) += sdio/sdiohal_tx.o
wcn_bsp-$(CONFIG_SDIOHAL) += sdio/sdiohal_rx.o
wcn_bsp-$(CONFIG_SDIOHAL) += sdio/sdiohal_if.o
#wcn_bsp-$(CONFIG_DEBUG_FS) += sdio/sdiohal_ctl.o
