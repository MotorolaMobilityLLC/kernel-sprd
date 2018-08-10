obj-$(CONFIG_WCN_BOOT) := marlin.o

#marlin-c-objs += gnss/gnss_common_ctl.o
#marlin-c-objs += gnss/gnss_pmnotify_ctl.o
#marlin-c-objs += gnss/gnss_dbg.o
#marlin-c-objs += gnss/gnss_dump.o
marlin-c-objs += wcn_boot.o
marlin-c-objs += wcn_dump.o
marlin-c-objs += bufring.o
marlin-c-objs += wcn_txrx.o
marlin-c-objs += wcn_op.o
#marlin-c-objs += rf/rf.o
marlin-c-objs += wcn_parn_parser.o
marlin-c-objs += rdc_debug.o
marlin-c-objs += wcn_log.o
marlin-c-objs += wcn_procfs.o
marlin-c-objs += mem_pd_mgr.o
marlin-$(CONFIG_WCN_UTILS) += \
		wcn_debugfs.o

marlin-objs += $(marlin-c-objs)

#WCN_PLATFORM
obj-$(CONFIG_WCN_PLATFORM) := wcn_platform.o
wcn_platform-c-objs += wcn_parn_parser.o

wcn_platform-c-objs +=	gnss/gnss_common_ctl.o \
			gnss/gnss_pmnotify_ctl.o \
			gnss/gnss_dbg.o \
			gnss/gnss_dump.o
wcn_platform-c-objs +=  wcn_procfs.o \
			wcn_txrx.o \
			rdc_debug.o \
			wcn_log.o \
			rf/rf.o \
			bufring.o

wcn_platform-objs += $(wcn_platform-c-objs)
