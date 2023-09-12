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

#ifndef _SPRD_VDSP_IOVA_H_
#define _SPRD_VDSP_IOVA_H_

#include <linux/types.h>
#include <linux/genalloc.h>

struct iova_reserve {
	char *name;
	struct genpool_data_fixed fixed;
	unsigned long iova_addr;
	size_t size;
	int status;
};

struct sprd_vdsp_iommu_iova {
	unsigned long iova_base;	// iova base addr
	size_t iova_size;	        // iova range size
	struct gen_pool *pool;
	struct iommu_iova_ops *ops;
	struct iova_reserve *reserve_data; // reserve data
	unsigned reserve_num;              // reserve num
};

struct iommu_iova_ops {
	int (*iova_init) (struct sprd_vdsp_iommu_iova *iova, unsigned long iova_base, size_t iova_size,
		int min_alloc_order);
	void (*iova_release) (struct sprd_vdsp_iommu_iova *iova);
	unsigned long (*iova_alloc) (struct sprd_vdsp_iommu_iova *iova, size_t iova_length);
	void (*iova_free) (struct sprd_vdsp_iommu_iova *iova, unsigned long iova_addr, size_t iova_length);
	int (*iova_reserve_init)(struct sprd_vdsp_iommu_iova *iova, struct iova_reserve *reserve_data,
		unsigned int reserve_num);
	void (*iova_reserve_relsase)(struct sprd_vdsp_iommu_iova *iova);
	int (*iova_alloc_fixed)(struct sprd_vdsp_iommu_iova *iova, unsigned long *iova_addr, size_t iova_length);
};

extern struct iommu_iova_ops version12_iova_ops;

#endif // end _SPRD_VDSP_IOVA_H_
