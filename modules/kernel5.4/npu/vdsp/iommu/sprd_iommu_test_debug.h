/*
 * SPDX-FileCopyrightText: 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
 * SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
 *
 * Copyright 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd.
 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */

#ifndef _IOMMU_DEV_TEST_DEBUG_
#define _IOMMU_DEV_TEST_DEBUG_

//file: sprd_iommu_dev_test_debug.c
//print struct iommu_dev
extern void debug_print_iommu_dev(struct sprd_vdsp_iommu_dev *iommu_dev);

//dump iommu_dev pagetable
extern int iommu_dump_pagetable(struct sprd_vdsp_iommu_dev *iommu_dev);

//alloc memory of size and init sg_table
extern void *alloc_sg_list(struct sg_table *sgt, size_t size);

//print all iommu_dev' map_record
extern void show_iommus_all_record(struct sprd_vdsp_iommus *iommus);

#endif //_IOMMU_DEV_TEST_DEBUG_
