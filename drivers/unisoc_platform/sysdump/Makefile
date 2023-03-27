sysdump-$(CONFIG_SPRD_SYSDUMP) += \
		unisoc_sysdump.o \
		unisoc_vmcoreinfo.o \
		last_kmsg.o

obj-$(CONFIG_SPRD_SYSDUMP) += sysdump.o

obj-$(CONFIG_SPRD_NATIVE_HANG_MONITOR) += native_hang_monitor.o
obj-$(CONFIG_UNISOC_LASTKMSG)	+= unisoc_last_kmsg.o
unisoc_last_kmsg-y := unisoc_dump_info.o
unisoc_last_kmsg-$(CONFIG_UNISOC_DUMP_IO)   += unisoc_dump_io.o
