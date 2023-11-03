moto_swap-objs += zram-5.15/zram_drv.o
moto_swap-objs += hybridswap/hybridswap_main.o
moto_swap-objs += hybridswap/hybridswap_eswap.o

#ifneq ($(filter m y,$(CONFIG_HYBRIDSWAP_ASYNC_COMPRESS)),)
#moto_swap-objs += hybridswap/hybridswap_akcompress.o
#endif

moto_swap-objs += hybridswap/hybridswap_swapd.o
moto_swap-objs += zram-5.15/zcomp.o

obj-m += moto_swap.o
