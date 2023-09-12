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

#ifndef _SPRD_VDSP_IOMMU_MAP_RECORD_H_
#define _SPRD_VDSP_IOMMU_MAP_RECORD_H_

#include <linux/types.h>
#define SPRD_MAX_SG_CACHED_CNT  2048

enum sg_pool_status {
	SG_SLOT_FREE = 0,
	SG_SLOT_USED,
};

struct sprd_iommu_map_slot {
	unsigned long sg_table_addr;	// sg_table addr
	unsigned long buf_addr;	// buf of mem core addr
	unsigned long iova_addr;	// iova addr
	unsigned long iova_size;
	enum sg_pool_status status;	// slot status
	int map_usrs;		// map counter
};

struct sprd_vdsp_iommu_map_record {
	struct sprd_iommu_map_slot slot[SPRD_MAX_SG_CACHED_CNT];
	unsigned int record_count;
	struct sprd_vdsp_iommu_map_record_ops *ops;
};

struct sprd_vdsp_iommu_map_record_ops {
	int (*init) (struct sprd_vdsp_iommu_map_record *record_dev);
	void (*release) (struct sprd_vdsp_iommu_map_record *record_dev);

	bool (*insert_slot) (struct sprd_vdsp_iommu_map_record *record_dev, unsigned long sg_table_addr,
		unsigned long buf_addr, unsigned long iova_addr, unsigned long iova_size);
	bool (*remove_slot) (struct sprd_vdsp_iommu_map_record *record_dev, unsigned long iova_addr);
	bool (*map_check) (struct sprd_vdsp_iommu_map_record *record_dev, unsigned long buf_addr,
		unsigned long *iova_addr);
	bool (*unmap_check) (struct sprd_vdsp_iommu_map_record *record_dev, unsigned long buf_addr);
	void (*show_all) (struct sprd_vdsp_iommu_map_record *record_dev);
	bool (*iova_find_buf) (struct sprd_vdsp_iommu_map_record *record_dev, unsigned long iova_addr,
		size_t iova_size, unsigned long *buf);
	bool (*buf_find_iova) (struct sprd_vdsp_iommu_map_record *record_dev, unsigned long buf_addr,
		size_t iova_size, unsigned long *iova_addr);
};

extern struct sprd_vdsp_iommu_map_record_ops iommu_map_record_ops;

#endif //_SPRD_VDSP_IOMMU_MAP_RECORD_H_
