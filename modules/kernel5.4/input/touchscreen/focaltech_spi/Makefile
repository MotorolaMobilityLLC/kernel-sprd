#
# Makefile for the focaltech touchscreen drivers.
#

ccflags-y += -DDEBUG

obj-m += focaltech_spi_ts.o

focaltech_spi_ts-objs := focaltech_core.o \
	focaltech_ex_fun.o \
	focaltech_ex_mode.o \
	focaltech_gesture.o \
	focaltech_esdcheck.o \
	focaltech_point_report_check.o \
	focaltech_spi.o \
	focaltech_flash.o \
	ts_func_test.o

.PHONY:modules

modules:
	make $(BSP_MAKE_EXTRA_ARGS) ARCH=$(BSP_BOARD_ARCH) CROSS_COMPILE=$(BSP_KERNEL_CROSS_COMPILE) -C $(BSP_KERNEL_PATH) O=$(BSP_KERNEL_OUT) M=`pwd` $@

modules_install:
	make $(BSP_MAKE_EXTRA_ARGS) ARCH=$(BSP_BOARD_ARCH) CROSS_COMPILE=$(BSP_KERNEL_CROSS_COMPILE) -C $(BSP_KERNEL_PATH) O=$(BSP_KERNEL_OUT) M=`pwd` INSTALL_MOD_PATH=$(BSP_MODULES_OUT) $@

clean:
	make -C $(BSP_KERNEL_PATH) O=$(BSP_KERNEL_OUT) M=`pwd` $@
