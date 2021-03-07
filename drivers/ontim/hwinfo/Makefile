ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ifeq ($(BUILD_SMT),YES)
    ccflags-y += -DSMT_VERSION
    $(info ++++++++++++++++++++++++++firefly++++++++++++++++ SMT version)
endif
obj-y += gpio_boardid.o
obj-y += hwinfo.o
#obj-y += board_id_adc.o
