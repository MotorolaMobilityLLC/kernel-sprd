#
# npu_img_mem.ko
#
# Kbuild: for kernel building external module
#
# Note:
# - Please refer to modules/sample/Kbuild to find out what should be
#   done is this Kbuild file
#

#
# Source List
#
KO_MODULE_NAME := npu_img_vha
KO_MODULE_PATH := $(src)
KO_MODULE_SRC  :=
KO_MODULE_SRC += \
	$(shell find $(KO_MODULE_PATH) -name "*.c")

#
# Build Options
#
subdir-ccflags-y += -I$(KO_MODULE_PATH)/img_mem/imgmmu/mmulib
subdir-ccflags-y += -I$(KO_MODULE_PATH)/include
subdir-ccflags-y += -I$(KO_MODULE_PATH)/vha/chipdep/n6pro
subdir-ccflags-y += -I$(KO_MODULE_PATH)/vha/single
subdir-ccflags-y += -I$(KO_MODULE_PATH)/vha
subdir-ccflags-y += -I$(KO_MODULE_PATH)/vha/platform

ccflags-y                += -DCFG_SYS_AURA
ccflags-y                += -DHW_AX3
ccflags-y                += -DVHA_MMU_MIRRORED_CTX_SUPPORT
ccflags-y                += -DOSID=0
ccflags-y                += -DVHA_DEVFREQ
ccflags-y                += -DVHA_USE_LO_PRI_SUB_SEGMENTS
ccflags-y                += -DDEFAULT_SYMBOL_NAMESPACE=VHA_CORE
ifdef CONFIG_SPRD_NPU_COOLING_DEVICE
  ccflags-y                += -DSPRD_NPU_COOLING
else ifdef CONFIG_UNISOC_NPU_COOLING_DEVICE
  ccflags-y                += -DUNISOC_NPU_COOLING
endif

#
# Final Objects
#
# Comment it if the only object file has the same name with module
obj-m := $(KO_MODULE_NAME).o
$(KO_MODULE_NAME)-y := $(patsubst $(src)/%.c,%.o,$(KO_MODULE_SRC))

