#
# sprd_vdsp.ko
#
# Makefile: for external make invocation
#
# Note:
# - Please refer to modules/sample/Makefile to find out what should be
#   done in this Makefile
#

KO_MODULE_NAME := sprd_vdsp
KO_MODULE_OUT := $(BSP_MODULES_OUT)/sprd_vdsp
KO_MODULE_KBUILD := $(CURDIR)/Kbuild

KBUILD_EXTRA_SYMBOLS += $(BSP_MODULES_OUT)/mmdvfs/Module.symvers
export KBUILD_EXTRA_SYMBOLS

.PHONY: modules modules_install clean

ifndef BSP_BOARD_PRODUCT_USING_VDSP
modules modules_install clean:
	@echo "Skipped, Kbuild only for Cadence vdsp!"
else
modules:
	@mkdir -p $(KO_MODULE_OUT) && ln -snf $(KO_MODULE_KBUILD) $(KO_MODULE_OUT)/Kbuild
	@ln -snf $(CURDIR) $(KO_MODULE_OUT)/source
	$(MAKE) -C $(BSP_KERNEL_PATH) M=$(KO_MODULE_OUT) src=$(CURDIR) $@

modules_install:
	$(MAKE) -C $(BSP_KERNEL_PATH) M=$(KO_MODULE_OUT) $@

# Remove the out directory wholly
clean:
	@#$(MAKE) -C $(BSP_KERNEL_PATH) M=$(KO_MODULE_OUT) src=$(CURDIR) $@
	rm -rf $(KO_MODULE_OUT)
endif
