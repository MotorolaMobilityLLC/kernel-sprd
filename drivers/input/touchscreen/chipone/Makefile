ifneq ($(CONFIG_TOUCHSCREEN_CHIPONE_MODEL), )
ccflags-y += -DTOUCHSCREEN_NOVATEK_MODEL="\"$(CONFIG_TOUCHSCREEN_CHIPONE_MODEL)\""
endif

obj-$(CONFIG_TOUCHSCREEN_CHIPONE_SOO) += chipone-tddi.o

chipone-tddi-objs := cts_i2c_driver.o cts_core.o cts_platform.o cts_sfctrlv2.o cts_spi_flash.o cts_firmware.o cts_tool.o cts_sysfs.o cts_test.o cts_charger_detect.o cts_earjack_detect.o cts_strerror.o

