#
# Makefile for the Novatek NT36xxx touchscreen driver.
#
# ccflags-y +=    -I$(srctree)/drivers/input/touchscreen/nt36xxx/${TARGET_PRODUCT}/
# Each configuration option enables a list of files.
# obj-$(CONFIG_TOUCHSCREEN_NT36XXX_TP) += nt36xxx.o nt36xxx_fw_update.o nt36xxx_ext_proc.o nt36xxx_mp_ctrlram.o
obj-$(CONFIG_TOUCHSCREEN_NT36XXX_SPI)  += adaptive-ts.o
adaptive-ts-objs := nt36xxx_spi.o
adaptive-ts-objs += nt36xxx_fw_update.o
adaptive-ts-objs += nt36xxx_ext_proc.o
adaptive-ts-objs += nt36xxx_mp_ctrlram.o

