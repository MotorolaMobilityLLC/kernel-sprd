ifeq ($(BUILD_DPWK),YES)
    ccflags-y += -DAUTH_VERSION
    $(info ++++++++++++++++++++++++++firefly++++++++++++++++ AUTH version)
endif
obj-$(CONFIG_SPRD_MEMDISK)	+= sprd_memdisk.o
obj-$(CONFIG_SPRD_7SRESET)	+= sprd_7sreset.o
obj-$(CONFIG_SPRD_SYSTIMER)	+= sprd_systimer.o
obj-$(CONFIG_SPRD_TIME_SYNC)	+= sprd_time_sync.o
obj-$(CONFIG_SPRD_TIME_SYNC_CP) += sprd_time_sync_cp.o
obj-$(CONFIG_SPRD_IOMMU)        += iommu/
obj-$(CONFIG_SPRD_PMIC_SYSCON)	+= sprd_pmic_syscon.o
obj-y				+= modem/
obj-y				+= domain/
obj-$(CONFIG_SPRD_DEBUG)	+= debug/
obj-$(CONFIG_SPRD_POWER)	+= power/
obj-$(CONFIG_SPRD_SYSDUMP)      += debug/sysdump/
obj-$(CONFIG_SPRD_NATIVE_HANG_MONITOR)  += debug/native_hang_monitor/
obj-$(CONFIG_SPRD_HANG_DEBUG)      += debug/hang_debug/
obj-y				+= performance/
obj-y				+= pub/
obj-y				+= autotest/
obj-$(CONFIG_SPRD_HWFEATURE)	+= sprd_hwfeature.o
obj-$(CONFIG_SPRD_PIKE2_SLEEP)  += sprd_pike2_sleep.o
obj-$(CONFIG_SPRD_HRTIMER_DEBUG)	+= sprd_hrtimer_debug.o
obj-$(CONFIG_SPRD_TIME_SYNC_CH)	+= sprd_time_sync_ch.o
obj-$(CONFIG_SPRD_USBPINMUX)	+= usbpinmux/
