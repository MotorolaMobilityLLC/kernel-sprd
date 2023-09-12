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

#include <linux/sched.h>
#include "sprd_vdsp_iommu_record.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: [iommu_record]: %d %d %s: "\
               fmt, current->pid, __LINE__, __func__

int record_init(struct sprd_vdsp_iommu_map_record *record_dev)
{
	return 0;
}

void record_release(struct sprd_vdsp_iommu_map_record *record_dev)
{
	return;
}

static bool record_insert_slot(struct sprd_vdsp_iommu_map_record *record_dev,
	unsigned long sg_table_addr, unsigned long buf_addr,
	unsigned long iova_addr, unsigned long iova_size)
{
	int index = 0;
	struct sprd_iommu_map_slot *slot;

	pr_debug("sg_table_addr = 0x%lx\n", sg_table_addr);
	pr_debug("buf_addr      = 0x%lx\n", buf_addr);
	pr_debug("iova_addr     = 0x%lx\n", iova_addr);
	pr_debug("iova_size     = 0x%zx\n", iova_size);

	if (record_dev->record_count >= SPRD_MAX_SG_CACHED_CNT) {
		pr_err("Error: record slot is full,now record %d\n", record_dev->record_count);
		BUG_ON(record_dev->record_count >= SPRD_MAX_SG_CACHED_CNT);
	}

	for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
		slot = &(record_dev->slot[index]);
		if (slot->status == SG_SLOT_FREE) {
			slot->sg_table_addr = sg_table_addr;
			slot->buf_addr = buf_addr;
			slot->iova_addr = iova_addr;
			slot->iova_size = iova_size;
			slot->status = SG_SLOT_USED;
			slot->map_usrs++;
			record_dev->record_count++;
			break;
		}
	}

	if (index < SPRD_MAX_SG_CACHED_CNT)
		return true;
	else
		return false;
}

static bool record_remove_slot(struct sprd_vdsp_iommu_map_record *record_dev,
	unsigned long iova_addr)
{
	int index = 0;

	for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
		if (record_dev->slot[index].status == SG_SLOT_USED) {
			if (record_dev->slot[index].iova_addr == iova_addr) {

				record_dev->slot[index].map_usrs--;

				if (record_dev->slot[index].map_usrs == 0) {
					record_dev->slot[index].buf_addr = 0;
					record_dev->slot[index].sg_table_addr = 0;
					record_dev->slot[index].iova_addr = 0;
					record_dev->slot[index].iova_size = 0;
					record_dev->slot[index].status = SG_SLOT_FREE;
					record_dev->record_count--;
				} else {
					pr_warn("Warning: record_remove_slot only map_usrs--\n");
					pr_warn("Warning: map_usrs = %d\n", record_dev->slot[index].map_usrs);
				}
				break;
			}
		}
	}

	if (index < SPRD_MAX_SG_CACHED_CNT)
		return true;
	else
		return false;
}

static bool record_map_check(struct sprd_vdsp_iommu_map_record *record_dev,
	unsigned long buf_addr, unsigned long *iova_addr)
{
	int index = 0;

	for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
		if (record_dev->slot[index].status == SG_SLOT_USED &&
			record_dev->slot[index].buf_addr == buf_addr) {
			*iova_addr = record_dev->slot[index].iova_addr;
			record_dev->slot[index].map_usrs++;
			break;
		}
	}
	if (index < SPRD_MAX_SG_CACHED_CNT)
		return true;
	else
		return false;
}

static bool record_unmap_check(struct sprd_vdsp_iommu_map_record *record_dev,
	unsigned long buf_addr)
{
	int index = 0;

	for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
		if (record_dev->slot[index].status == SG_SLOT_USED &&
			record_dev->slot[index].buf_addr == buf_addr) {
			break;
		}
	}
	if (index < SPRD_MAX_SG_CACHED_CNT) {
		return true;
	} else {
		return false;
	}
}

static void record_show_all_slot(struct sprd_vdsp_iommu_map_record *record_dev)
{
	int index;
	struct sprd_iommu_map_slot *slot;

	pr_debug("record_dev->record_count %d\n", record_dev->record_count);

	if (record_dev->record_count > 0) {
		for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
			slot = &(record_dev->slot[index]);
			if (slot->status == SG_SLOT_USED) {
				pr_debug("Warning: buffer iova 0x%lx size 0x%lx sg 0x%08lx buf 0x%08lx"
					" map_usrs %d should be unmapped!\n",
					slot->iova_addr, slot->iova_size, slot->sg_table_addr,
					slot->buf_addr, slot->map_usrs);
			}
		}
	}
}

static bool record_target_iova_find_buf(struct sprd_vdsp_iommu_map_record *record_dev,
	unsigned long iova_addr, size_t iova_size, unsigned long *buf)
{
	int index = 0;

	for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
		if (record_dev->slot[index].status == SG_SLOT_USED &&
			record_dev->slot[index].iova_addr == iova_addr &&
			record_dev->slot[index].iova_size == iova_size) {
			*buf = record_dev->slot[index].buf_addr;
			break;
		}
	}
	if (index < SPRD_MAX_SG_CACHED_CNT)
		return true;
	else
		return false;
}

static bool reocrd_target_buf_find_iova(struct sprd_vdsp_iommu_map_record *record_dev,
	unsigned long buf_addr, size_t iova_size, unsigned long *iova_addr)
{
	int index;

	for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
		if (record_dev->slot[index].status == SG_SLOT_USED &&
			record_dev->slot[index].buf_addr == buf_addr &&
			record_dev->slot[index].iova_size == iova_size) {
			*iova_addr = record_dev->slot[index].iova_addr;
			break;
		}
	}
	if (index < SPRD_MAX_SG_CACHED_CNT)
		return true;
	else
		return false;
}

struct sprd_vdsp_iommu_map_record_ops iommu_map_record_ops = {
	.init = record_init,
	.release = record_release,
	.insert_slot = record_insert_slot,
	.remove_slot = record_remove_slot,
	.map_check = record_map_check,
	.unmap_check = record_unmap_check,
	.show_all = record_show_all_slot,
	.iova_find_buf = record_target_iova_find_buf,
	.buf_find_iova = reocrd_target_buf_find_iova,
};
