# SPDX-License-Identifier: GPL-2.0
ccflags-y := -I $(srctree)/$(src)

sprd-iommu-y := \
	sprd_iommu.o \
	sprd_iommu_sysfs.o

sprd-iommu-y += \
	drv/sprd_iommuex_ops.o \
	drv/sprd_iommuvau_ops.o

sprd-iommu-y += drv/api/sprd_iommu_api.o

sprd-iommu-y += \
	drv/cll/sprd_iommuex_cll.o \
	drv/cll/sprd_iommuvau_cll.o

sprd-iommu-y += drv/com/sprd_com.o

sprd-iommu-y += \
	drv/hal/sprd_iommuex_hal_register.o \
	drv/hal/sprd_iommuvau_hal_register.o

sprd-iommu-y += drv/osal/sprd_osal.o

obj-$(CONFIG_SPRD_IOMMU) += sprd-iommu.o
