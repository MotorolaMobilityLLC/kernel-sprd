#
# Makefile for the Novatek NT36xxx touchscreen driver.
#
# Each configuration option enables a list of files.

ifneq ($(CONFIG_TOUCHSCREEN_NOVATEK_MODEL), )
ccflags-y += -DTOUCHSCREEN_NOVATEK_MODEL="\"$(CONFIG_TOUCHSCREEN_NOVATEK_MODEL)\""
endif

obj-$(CONFIG_TOUCHSCREEN_NOVATEK_S00) += \
	nt36xxx.o 		\
	nt36xxx_fw_update.o 	\
	nt36xxx_ext_proc.o	\
	nt36xxx_mp_ctrlram.o 	\
