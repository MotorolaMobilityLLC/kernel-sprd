include $(BOARD_PATH)

KERNEL_CONFIG := $(KERNEL_OUT)/.config
KERNEL_MODULES_OUT := $(PRODUCT_OUT)/$(TARGET_COPY_OUT_VENDOR)/lib/modules
KERNEL_USR_HEADERS := $(KERNEL_OUT)/usr
ifdef KERNEL_PATH
KERNEL_REAL_PATH := $(KERNEL_PATH)
else
KERNEL_REAL_PATH := kernel
endif
JOBS := $(shell if [ $(cat /proc/cpuinfo | grep processor | wc -l) -gt 8 ]; then echo 8; else echo 4; fi)
TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/arch/$(TARGET_KERNEL_ARCH)/boot/Image

TARGET_BOARD_SPEC_CONFIG := $(KERNEL_DIFF_CONFIG_ARCH)/$(TARGET_BOARD)_diff_config

ifdef BUILD_FROM_KERNEL
$(KERNEL_CONFIG): arch/$(TARGET_KERNEL_ARCH)/configs/$(KERNEL_DEFCONFIG)
	$(MAKE) ARCH=$(TARGET_KERNEL_ARCH) $(KERNEL_DEFCONFIG)
define sprd_create_user_config
	$(shell ./scripts/sprd_create_user_config.sh $(1) $(2))
endef
else
$(KERNEL_CONFIG): $(KERNEL_REAL_PATH)/arch/$(TARGET_KERNEL_ARCH)/configs/$(KERNEL_DEFCONFIG)
	echo "KERNEL_OUT = $(KERNEL_OUT),  KERNEL_DEFCONFIG = $(KERNEL_DEFCONFIG)"
	mkdir -p $(KERNEL_OUT)
	$(MAKE) ARCH=$(TARGET_KERNEL_ARCH) -C $(KERNEL_REAL_PATH)  O=../$(KERNEL_OUT) $(KERNEL_DEFCONFIG)
define sprd_create_user_config
	$(shell ./$(KERNEL_REAL_PATH)/scripts/sprd_create_user_config.sh $(1) $(2))
endef
endif

$(KERNEL_OUT):
	@echo "==== Start Kernel Compiling ... ===="
	echo "KERNEL_REAL_PATH = $(KERNEL_REAL_PATH)"


#sharkle
ifeq ($(strip $(BOARD_TEE_CONFIG)),trusty)
ifneq ($(strip $(BOARD_TEE_64BIT)),)
ifeq ($(strip $(BOARD_TEE_64BIT)),false)
TARGET_DEVICE_TRUSTY_CONFIG := $(KERNEL_DIFF_CONFIG_ARCH)/trusty_aarch32_diff_config
else
TARGET_DEVICE_TRUSTY_CONFIG := $(KERNEL_DIFF_CONFIG_ARCH)/trusty_aarch64_diff_config
endif
else
TARGET_DEVICE_TRUSTY_CONFIG := $(KERNEL_DIFF_CONFIG_ARCH)/trusty_diff_config
endif
endif


ifeq ($(strip $(BOARD_EXT_PMIC_CONFIG)),SC2703)
TARGET_DEVICE_EXT_PMIC_CONFIG := $(KERNEL_DIFF_CONFIG_COMMON)/ext_pmic_sc2703_diff_config
endif

ifeq ($(strip $(BOARD_WCN_CONFIG)),ext)
TARGET_DEVICE_WCN_CONFIG := $(KERNEL_DIFF_CONFIG_COMMON)/wcn_ext_diff_config
endif

ifeq ($(strip $(BOARD_WCN_CONFIG)),built-in)
TARGET_DEVICE_WCN_CONFIG := $(KERNEL_DIFF_CONFIG_COMMON)/wcn_built_in_diff_config
endif

ifeq ($(strip $(PRODUCT_GO_DEVICE)),true)
TARGET_GO_DEVICE_CONFIG := $(KERNEL_DIFF_CONFIG_ARCH)/go_google_diff_config
endif

ifeq ($(TARGET_BUILD_VARIANT),user)
DEBUGMODE := BUILD=no
USER_CONFIG := $(TARGET_OUT)/dummy
#TARGET_DEVICE_USER_CONFIG := $(KERNEL_DIFF_CONFIG_ARCH)/user_diff_config
ifeq ($(TARGET_KERNEL_ARCH), arm)
TARGET_DEVICE_USER_CONFIG := $(KERNEL_DIFF_CONFIG_ARCH)/aarch32_user_diff_config
endif
ifeq ($(TARGET_KERNEL_ARCH), arm64)
TARGET_DEVICE_USER_CONFIG := $(KERNEL_DIFF_CONFIG_ARCH)/aarch64_user_diff_config
endif
$(USER_CONFIG) : $(KERNEL_CONFIG)
	$(call sprd_create_user_config, $(KERNEL_CONFIG), $(TARGET_DEVICE_USER_CONFIG))
	$(call sprd_create_user_config, $(KERNEL_CONFIG), $(TARGET_BOARD_SPEC_CONFIG))
ifeq ($(strip $(BOARD_TEE_CONFIG)),trusty)
	$(call sprd_create_user_config, $(KERNEL_CONFIG), $(TARGET_DEVICE_TRUSTY_CONFIG))
endif
else
DEBUGMODE := $(DEBUGMODE)
USER_CONFIG := $(TARGET_OUT)/dummy
$(USER_CONFIG) : $(KERNEL_CONFIG)
	$(call sprd_create_user_config, $(KERNEL_CONFIG), $(TARGET_BOARD_SPEC_CONFIG))
ifeq ($(strip $(BOARD_TEE_CONFIG)),trusty)
	$(call sprd_create_user_config, $(KERNEL_CONFIG), $(TARGET_DEVICE_TRUSTY_CONFIG))
endif
endif

ifeq ($(strip $(BOARD_EXT_PMIC_CONFIG)),SC2703)
	$(call sprd_create_user_config, $(KERNEL_CONFIG), $(TARGET_DEVICE_EXT_PMIC_CONFIG))
endif

ifeq ($(strip $(BOARD_WCN_CONFIG)),ext)
	$(call sprd_create_user_config, $(KERNEL_CONFIG), $(TARGET_DEVICE_WCN_CONFIG))
endif
ifeq ($(strip $(BOARD_WCN_CONFIG)),built-in)
	$(call sprd_create_user_config, $(KERNEL_CONFIG), $(TARGET_DEVICE_WCN_CONFIG))
endif

ifeq ($(strip $(PRODUCT_GO_DEVICE)),true)
	$(call sprd_create_user_config, $(KERNEL_CONFIG), $(TARGET_GO_DEVICE_CONFIG))
endif


$(KERNEL_OUT)/config : $(KERNEL_OUT) $(USER_CONFIG)


$(TARGET_PREBUILT_KERNEL) : $(KERNEL_OUT) $(USER_CONFIG)
	$(MAKE) -C $(KERNEL_REAL_PATH) O=../$(KERNEL_OUT) ARCH=$(TARGET_KERNEL_ARCH) CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) headers_install
	$(MAKE) -C $(KERNEL_REAL_PATH) O=../$(KERNEL_OUT) ARCH=$(TARGET_KERNEL_ARCH) CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) -j${JOBS}
	$(MAKE) -C $(KERNEL_REAL_PATH) O=../$(KERNEL_OUT) ARCH=$(TARGET_KERNEL_ARCH) CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) modules
	@-mkdir -p $(KERNEL_MODULES_OUT)
	@-find $(TARGET_OUT_INTERMEDIATES) -name *.ko | xargs -I{} cp {} -v $(KERNEL_MODULES_OUT)

$(KERNEL_USR_HEADERS) : $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_REAL_PATH) O=../$(KERNEL_OUT) ARCH=$(TARGET_KERNEL_ARCH) CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) headers_install

$(KERNEL_OUT)/kernelheader:
	mkdir -p $(KERNEL_OUT)
	$(MAKE) ARCH=$(TARGET_KERNEL_ARCH) -C $(KERNEL_REAL_PATH) O=../$(KERNEL_OUT) headers_install
