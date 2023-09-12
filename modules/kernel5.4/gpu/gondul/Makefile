# mali_gondul.o

PRODUCT_OUT := $(BSP_ROOT_DI)
#such as arm
TARGET_KERNEL_ARCH := $(BSP_BOARD_ARCH)
KERNEL_CROSS_COMPILE := $(BSP_KERNEL_CROSS_COMPILE)

#need to be defined in device dir
$(warning BSP_BOARD_NAME: $(BSP_BOARD_NAME))

TARGET_BOARD := $(BSP_BOARD_NAME)
PRODUCT_OUT := $(PRODUCT_OUT)

$(warning PRODUCT_OUT_ABSOLUTE: $(PRODUCT_OUT_ABSOLUTE))
$(warning KERNEL_CROSS_COMPILE: $(KERNEL_CROSS_COMPILE))

ifeq ($(TARGET_BUILD_VARIANT),user)
  DEBUGMODE := BUILD=no
else
  DEBUGMODE := $(DEBUGMODE)
endif
  #MALI_PLATFORM_NAME variable was added at file :device/sprd/*platform*/common/BoardCommon.mk

#convert to absolute directory
#PRODUCT_OUT_ABSOLUTE:=$(shell cd $(PRODUCT_OUT); pwd)

KO_MODULE_NAME := mali_gondul
KO_MODULE_OUT := $(BSP_MODULES_OUT)/$(KO_MODULE_NAME)
KO_MODULE_KBUILD := $(CURDIR)/mali/Kbuild

.PHONY: modules modules_install clean

modules:
	@mkdir -p $(KO_MODULE_OUT) && ln -snf $(KO_MODULE_KBUILD) $(KO_MODULE_OUT)/Kbuild
	@ln -snf $(CURDIR) $(KO_MODULE_OUT)/source
	$(MAKE) -C $(CURDIR)/mali CONFIG_MALI_PLATFORM_NAME=$(MALI_PLATFORM_NAME) CONFIG_MALI_BOARD_PLATFORM=$(TARGET_BOARD_PLATFORM) CONFIG_TARGET_BOARD=$(TARGET_BOARD) $(DEBUGMODE) CONFIG_MALI_VOLTAGE_LEVEL=$(VOLTAGE_CONTROLLER_LEVEL) KDIR=$(BSP_KERNEL_PATH) M=$(KO_MODULE_OUT) src=$(CURDIR)/mali

modules_install:
	$(MAKE) -C $(BSP_KERNEL_PATH) M=$(KO_MODULE_OUT) $@

# Remove the out wholly
clean:
	rm -rf $(KO_MODULE_OUT)
