/*
 * SPDX-FileCopyrightText: 2020-2022 Unisoc (Shanghai) Technologies Co., Ltd
 * SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
 *
 * Copyright 2020-2022 Unisoc (Shanghai) Technologies Co., Ltd.
 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */

#ifndef _SPRD_VDSP_IOMMU_DEV_H_
#define _SPRD_VDSP_IOMMU_DEV_H_

#include <linux/of.h>

struct sprd_vdsp_iommu_map_conf {

	unsigned long buf_addr;
	struct sg_table *table;
	// enum sprd_iommu_chtype ch_type;
	unsigned int sg_offset;
	unsigned long iova_addr;	/*output */
	size_t iova_size;
	int isfixed;                // flag for fixed map, 0: no 1:fixed_offset 2:fixed_addr
	unsigned long fixed_data;   // fixed map addr or offset
};

struct sprd_vdsp_iommu_unmap_conf {

	unsigned long buf_addr;
	unsigned long iova_addr;
	size_t iova_size;
};

struct sprd_vdsp_iommu_dev {
	int id;			// iommu id
	const char *name;	// iommu name
	unsigned int iommu_version;	// iommu revison
	unsigned int status;	// iommu status 0:init status

	unsigned long iova_base;	// iova base addr
	size_t iova_size;	// iova range size
	unsigned long pgt_base;	// page table base address :flow origine duplicate ctrl_reg ioremap dtb set
	size_t pgt_size;	// page table array size   :flow origine duplicate ctrl_reg size det set
	spinlock_t pgt_lock;	// page table spinlock
	unsigned long ctrl_reg;	// control register base

	unsigned long pagt_base_ddr;	// page table phy base address dma_alloc_coherent return parameter
	unsigned long pagt_base_virt;	// page table vir base address dma_alloc_coherent return val
	unsigned long pagt_ddr_size;	// page table array size

	struct device *dev;
	struct sprd_vdsp_iommu_dev_ops *ops;	//iommu ops
	struct sprd_vdsp_iommu_iova *iova_dev;	// iommu iova manager
	struct sprd_vdsp_iommus *iommus;      //parent iommus

	// struct sprd_vdsp_iommu_hw_ops *hw_ops; // iommu hw ops
	struct sprd_vdsp_iommu_widget *iommu_hw_dev;	// iommu hw dev
	struct sprd_vdsp_iommu_map_record *record_dev;	// iommu map record manager
	struct xvp *xvp;	// parent xvp
	struct mutex mutex;	// lock for iommu_dev
	void *private;
	unsigned long fault_page;	// compatible with the original version, not use
};

struct sprd_vdsp_iommu_dev_ops {

	int (*init) (struct sprd_vdsp_iommu_dev * iommu_dev,
		     struct device_node * of_node, struct device * dev);
	void (*release) (struct sprd_vdsp_iommu_dev * iommu_dev);

	int (*map) (struct sprd_vdsp_iommu_dev * iommu_dev,
		    struct sprd_vdsp_iommu_map_conf * map_conf);
	int (*unmap) (struct sprd_vdsp_iommu_dev * iommu_dev,
		      struct sprd_vdsp_iommu_unmap_conf * unmap_conf);
	//power and clock
	int (*suspend) (struct sprd_vdsp_iommu_dev * iommu_dev);
	int (*resume) (struct sprd_vdsp_iommu_dev * iommu_dev);
	int (*restore) (struct sprd_vdsp_iommu_dev * iommu_dev);
};

extern struct sprd_vdsp_iommu_dev_ops iommu_dev_ops;

#endif //_SPRD_VDSP_IOMMU_DEV_H_
