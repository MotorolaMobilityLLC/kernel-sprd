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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include "sprd_vdsp_iommu_dev.h"
#include "sprd_vdsp_iova.h"
#include "sprd_vdsp_iommuvau_register.h"
#include "sprd_vdsp_iommuvau_cll.h"
#include "sprd_vdsp_iommu_record.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: [iommus]: %d %d %s: " fmt, current->pid, __LINE__, __func__

static int iommus_iova_init(struct sprd_vdsp_iommus *iommus)
{
	int ret = 0;
	struct sprd_vdsp_iommu_iova *iova = NULL;

	if (unlikely(iommus == NULL)) {
		pr_err("Error: iommus is NULL!\n");
		return -EINVAL;
	}

	iova = kzalloc(sizeof(struct sprd_vdsp_iommu_iova), GFP_KERNEL);
	if (!iova) {
		pr_err("Error: kzalloc failed\n");
		return -ENOMEM;
	}
	iommus->iova_dev = iova;
	switch (VDSP_IOMMU_VERSION) {
	case 12:
		iova->ops = &version12_iova_ops;
		break;
	default:
		pr_err("Error: iommus->iommu_devs[0]->iommu_version:%d\n",
			iommus->iommu_devs[0]->iommu_version);
		return -EINVAL;
	}
	iommus->iova_base = iommus->iommu_devs[0]->iova_base;
	iommus->iova_size = iommus->iommu_devs[0]->iova_size;
	ret = iova->ops->iova_init(iova, iommus->iova_base, iommus->iova_size, 12);
	pr_debug("iommus_iova_init sucess\n");
	return ret;
}

static void iommus_iova_release(struct sprd_vdsp_iommus *iommus)
{
	struct sprd_vdsp_iommu_iova *iova = NULL;

	iova = iommus->iova_dev;
	if (!iova) {
		pr_err(" iova is NULL\n");
	} else {
		iova->ops->iova_release(iova);
	}
	return;
}


static int iommus_map_record_init(struct sprd_vdsp_iommus *iommus)
{

	struct sprd_vdsp_iommu_map_record *record = NULL;

	if (unlikely(iommus == NULL)) {
		pr_err("Error: iommus is NULL!\n");
		return -EINVAL;
	}
	record = (struct sprd_vdsp_iommu_map_record *)devm_kzalloc(iommus->dev,
		sizeof(struct sprd_vdsp_iommu_map_record),GFP_KERNEL);
	if (!record) {
		pr_err("Error: devm_kzalloc failed\n");
		return -ENOMEM;
	}
	record->ops = &iommu_map_record_ops;
	record->ops->init(record);
	iommus->record_dev = record;
	pr_debug("iommus_map_record_init sucess\n");
	return 0;
}

static void iommus_map_record_relsase(struct sprd_vdsp_iommus *iommus)
{
	struct sprd_vdsp_iommu_map_record *record = NULL;

	record = iommus->record_dev;
	if (record) {
		record->ops->release(record);
		devm_kfree(iommus->dev, iommus->record_dev);
	}
	return;
}


static void iommus_release(struct sprd_vdsp_iommus *iommus);
static int iommus_init(struct sprd_vdsp_iommus *iommus,
	struct device_node *vxp_dev_of_node, struct device *dev)
{

	int iommu_cnt = 0;
	unsigned int index = 0;
	struct device_node *iommu_dev_of_node = NULL;
	struct sprd_vdsp_iommu_dev *iommu_dev = NULL;
	int ret = 0;

#ifdef KERNEL_VERSION_54
	const char *iommu_name;
#endif

	if (unlikely(!iommus)) {
		pr_err("Error: iommus is NULL!\n");
		return -EINVAL;
	}
	if (unlikely(!vxp_dev_of_node)) {
		pr_err("Error: vxp_dev_of_node is NULL!\n");
		return -EINVAL;
	}
#ifdef KERNEL_VERSION_54
	iommu_cnt = of_property_count_strings(vxp_dev_of_node, "iommu_names");
	if (iommu_cnt < 1) {
		pr_err("Error: iommu_names attribute not found\n");
		return -ENODEV;
	}
#else
	if (!of_find_property(vxp_dev_of_node, "iommus", NULL)) {
		pr_err("Error: iommus attribute not found");
		return -ENODEV;
	}
	iommu_cnt = of_count_phandle_with_args(vxp_dev_of_node, "iommus", NULL);
#endif
	pr_debug("iommu_cnt = %d\n", iommu_cnt);
	if (iommu_cnt <= 0) {
		pr_err("Error reading wsa device from DT. iommu_cnt = %d\n", iommu_cnt);
		return -ENODEV;
	}
	if (iommu_cnt > SPRD_VDSP_IOMMU_MAX) {
		iommu_cnt = SPRD_VDSP_IOMMU_MAX;
		pr_warn("Warning: iommu_cnt != SPRD_VDSP_IOMMU_MAX. iommu_cnt = %d\n", iommu_cnt);
	}

	for (index = 0; index < iommu_cnt; index++) {
#ifdef KERNEL_VERSION_54
		ret = of_property_read_string_index(vxp_dev_of_node, "iommu_names", index, &iommu_name);
		if (ret) {	// return 0 on success
			pr_err("Error: of_property_read_string_index failed\n");
			return ret;
		}

		iommu_dev_of_node = of_find_compatible_node(NULL, NULL, iommu_name);
		if (unlikely(!iommu_dev_of_node)) {
			pr_err("Error: iommu node[%d]=%s not found\n", index, iommu_name);
			return -EINVAL;
		} else {
			pr_debug("iommu node[%d]=%s  found\n", index, iommu_name);
		}

#else
		iommu_dev_of_node = of_parse_phandle(vxp_dev_of_node, "iommus", index);
		if (unlikely(!iommu_dev_of_node)) {
			pr_err("Error: iommus phandle index:%d not found\n", index);
			return -EINVAL;
		}
#endif

		iommu_dev = devm_kzalloc(dev, sizeof(struct sprd_vdsp_iommu_dev), GFP_KERNEL);
		if (iommu_dev == NULL) {
			pr_err("Error: devm_kzalloc failed\n");
			return -ENOMEM;
		}
		iommu_dev->ops = &iommu_dev_ops;
		ret = iommu_dev->ops->init(iommu_dev, iommu_dev_of_node, dev);
		if (ret) {
			iommus_release(iommus);
			pr_err("Error: iommu_dev[%d] init failed, iommus_release\n", index);
			return ret;
		}

		iommus->iommu_devs[index] = iommu_dev;
		iommu_dev->iommus = iommus;
		of_node_put(iommu_dev_of_node);
	}
	iommus->dev = dev;
#ifdef VDSP_IOMMU_USE_SIGNAL_IOVA
	ret = iommus_iova_init(iommus);
	if (ret)
		goto error_iova_init;
	ret = iommus_map_record_init(iommus);
	if (ret)
		goto error_map_record_init;
#endif
	mutex_init(&iommus->mutex);
	return 0;

#ifdef VDSP_IOMMU_USE_SIGNAL_IOVA
error_map_record_init:
	iommus_iova_release(iommus);
error_iova_init:
	for (index = 0; index < iommu_cnt; index++) {
		iommu_dev = iommus->iommu_devs[index];
		iommu_dev->ops->release(iommu_dev);
		devm_kfree(dev, iommu_dev);
		iommus->iommu_devs[index] = NULL;
	}
	mutex_init(&iommus->mutex);
	return ret;
#endif
}

static void iommus_release(struct sprd_vdsp_iommus *iommus)
{
	unsigned int index = 0;
	struct sprd_vdsp_iommu_dev *iommu_dev = NULL;

	if (unlikely(!iommus)) {
		pr_err("Error: iommus is NULL!\n");
		return;
	}
	mutex_lock(&iommus->mutex);

	for (index = 0; index < SPRD_VDSP_IOMMU_MAX; index++) {
		iommu_dev = iommus->iommu_devs[index];
		if ((iommu_dev) && (iommu_dev->status & (0x1 << 0))) {
			iommu_dev->ops->release(iommu_dev);
			kfree(iommu_dev);
			iommus->iommu_devs[index] = NULL;
			pr_debug("release iommus[%d] sucessed\n", index);
		}
	}
#ifdef VDSP_IOMMU_USE_SIGNAL_IOVA
	iommus_iova_release(iommus);
	iommus_map_record_relsase(iommus);
#endif
	mutex_unlock(&iommus->mutex);
	return;
}

static int iommus_map_idx(struct sprd_vdsp_iommus *iommus,
	struct sprd_vdsp_iommu_map_conf *map_conf,
	int iommu_dev_id)
{

	struct sprd_vdsp_iommu_dev *iommu_dev = NULL;
	int ret = 0;

	if (unlikely(iommus == NULL || map_conf == NULL)) {
		pr_err("Error: iommu_dev_list or map_conf is NULL!\n");
		return -EINVAL;
	}

	if ((iommu_dev_id < 0) || (iommu_dev_id > SPRD_VDSP_IOMMU_MAX)) {
		pr_err("Error: iommu_dev_id inval [%d]\n", iommu_dev_id);
		return -EINVAL;
	}

	iommu_dev = iommus->iommu_devs[iommu_dev_id];
	if (unlikely(!iommu_dev)) {
		pr_err("Error iommu_dev is NULL!\n");
		return -EINVAL;
	}
	ret = iommu_dev->ops->map(iommu_dev, map_conf);
	return ret;
}

static int iommus_map_all(struct sprd_vdsp_iommus *iommus,
	struct sprd_vdsp_iommu_map_conf *map_conf)
{

	unsigned int index = 0;
	int ret = 0;
	int max = 0;
#ifdef VDSP_IOMMU_USE_SIGNAL_IOVA
	struct sprd_vdsp_iommu_iova *iova_dev = NULL;
	struct sprd_vdsp_iommu_map_record *record_dev = NULL;
	unsigned long iova = 0;
#endif

	if (unlikely(iommus == NULL || map_conf == NULL)) {
		pr_err("Error: iommu_dev_list or map_conf is NULL!\n");
		return -EINVAL;
	}
	mutex_lock(&iommus->mutex);

	max = SPRD_VDSP_IOMMU_MAX;
#ifdef VDSP_IOMMU_USE_SIGNAL_IOVA
	iova_dev = iommus->iova_dev;
	if (!iova_dev) {
		pr_err("Error: use signal iova but iommus->iova_dev=NULL\n");
		return -1;
	}
	if (map_conf->isfixed == 1 || map_conf->isfixed == 2) {
		iova = map_conf->fixed_data;
		if (map_conf->isfixed == 1) { // 1:fixed offset 2:fixed addr
			iova = iova + iova_dev->iova_base;
		}
		ret = iova_dev->ops->iova_alloc_fixed(iova_dev, &iova, map_conf->iova_size);
		if (ret) {
			pr_err("Error: iommus iova_alloc_fixed failed\n");
			mutex_unlock(&iommus->mutex);
			return -1;
		}
	} else {
		iova = iova_dev->ops->iova_alloc(iova_dev, map_conf->iova_size);
		if (iova == 0) {
			pr_err("Error: iommus iova_alloc failed\n");
			mutex_unlock(&iommus->mutex);
			return -1;
		}
	}
	map_conf->iova_addr = iova;
#endif

	for (index = 0; index < max; index++) {	//SPRD_VDSP_IOMMU_MAX
		ret = iommus_map_idx(iommus, map_conf, index);
		if (ret) {
			pr_err("Error:iommus_map_all failed,index=%d\n", index);
#ifdef VDSP_IOMMU_USE_SIGNAL_IOVA
			iova_dev->ops->iova_free(iova_dev, iova, map_conf->iova_size);
			map_conf->iova_addr = 0;
#endif
			mutex_unlock(&iommus->mutex);
			return ret;
		}
	}
#ifdef VDSP_IOMMU_USE_SIGNAL_IOVA
	record_dev = iommus->record_dev;
	record_dev->ops->insert_slot(record_dev, ( unsigned long) map_conf->table,
		map_conf->buf_addr, map_conf->iova_addr, map_conf->iova_size);
#endif
	mutex_unlock(&iommus->mutex);
	return ret;
}

static int iommus_unmap_idx(struct sprd_vdsp_iommus *iommus,
	struct sprd_vdsp_iommu_unmap_conf *unmap_conf,
	int iommu_id)
{

	struct sprd_vdsp_iommu_dev *iommu_dev = NULL;
	int ret = 0;

	if (unlikely(iommus == NULL || unmap_conf == NULL)) {
		pr_err("Error: iommu_dev_list or unmap_conf is NULL!\n");
		return -EINVAL;
	}
	if ((iommu_id < 0) || (iommu_id > SPRD_VDSP_IOMMU_MAX - 1)) {
		pr_err("Error: iommu_id inval[%d]\n", iommu_id);
		return -EINVAL;
	}

	iommu_dev = iommus->iommu_devs[iommu_id];
	if (unlikely(!iommu_dev)) {
		pr_err("Error iommu_dev is NULL!\n");
		return -EINVAL;
	}
	ret = iommu_dev->ops->unmap(iommu_dev, unmap_conf);
	return ret;
}

int iommus_unmap_all(struct sprd_vdsp_iommus *iommus,
	struct sprd_vdsp_iommu_unmap_conf *unmap_conf)
{

	unsigned int index = 0;
	int ret = 0;
	int max = 0;
#ifdef VDSP_IOMMU_USE_SIGNAL_IOVA
	struct sprd_vdsp_iommu_iova *iova_dev = NULL;
	struct sprd_vdsp_iommu_map_record *record_dev = NULL;
#endif

	if (unlikely(iommus == NULL || unmap_conf == NULL)) {
		pr_err("Error: iommu_dev_list or unmap_conf is NULL!\n");
		return -EINVAL;
	}
	mutex_lock(&iommus->mutex);

	max = SPRD_VDSP_IOMMU_MAX;
	for (index = 0; index < max; index++) {
		ret = iommus_unmap_idx(iommus, unmap_conf, index);
		if (ret) {
			pr_err("Error:sprd_vdsp_iommu_unmap_idx failed,ret=%d\n", ret);
			mutex_unlock(&iommus->mutex);
			return ret;
		}
	}
#ifdef VDSP_IOMMU_USE_SIGNAL_IOVA
	iova_dev = iommus->iova_dev;
	iova_dev->ops->iova_free(iova_dev, unmap_conf->iova_addr, unmap_conf->iova_size);
	record_dev = iommus->record_dev;
	record_dev->ops->remove_slot(record_dev, unmap_conf->iova_addr);
#endif
	mutex_unlock(&iommus->mutex);
	return ret;
}


static int iommus_reserve_init(struct sprd_vdsp_iommus *iommus,
	struct iova_reserve *reserve_data, unsigned int reserve_num)
{
	unsigned int index = 0;
	int ret = 0;
	int max = 0;
	struct sprd_vdsp_iommu_iova *iova_dev;

	if (unlikely(!iommus)) {
		pr_err("Error: iommus is NULL!\n");
		return -EINVAL;
	}

	mutex_lock(&iommus->mutex);

	max = SPRD_VDSP_IOMMU_MAX;
#ifndef VDSP_IOMMU_USE_SIGNAL_IOVA
	for (index = 0; index < max; index++) {	//SPRD_VDSP_IOMMU_MAX
		iova_dev = iommus->iommu_devs[index]->iova_dev;
		ret = iova_dev->ops->iova_reserve_init(iova_dev, reserve_data, reserve_num);
		if (ret) {
			pr_err("Error:iova_reserve_init failed,index=%d\n", index);
			mutex_unlock(&iommus->mutex);
			return ret;
		}
	}
#else
	iova_dev = iommus->iova_dev;
	ret = iova_dev->ops->iova_reserve_init(iova_dev, reserve_data, reserve_num);
	if (ret) {
		pr_err("Error:iova_reserve_init failed,index=%d\n", index);
		mutex_unlock(&iommus->mutex);
		return ret;
	}
#endif
	mutex_unlock(&iommus->mutex);
	return ret;
}

static void iommus_reserve_release(struct sprd_vdsp_iommus *iommus)
{
	int max = 0;
	struct sprd_vdsp_iommu_iova *iova_dev;

#ifndef VDSP_IOMMU_USE_SIGNAL_IOVA
	unsigned int index = 0;
#endif

	if (unlikely(!iommus)) {
		pr_err("Error: iommus is NULL!\n");
		return;
	}

	mutex_lock(&iommus->mutex);

	max = SPRD_VDSP_IOMMU_MAX;
#ifndef VDSP_IOMMU_USE_SIGNAL_IOVA
	for (index = 0; index < max; index++) {	//SPRD_VDSP_IOMMU_MAX
		iova_dev = iommus->iommu_devs[index]->iova_dev;
		iova_dev->ops->iova_reserve_relsase(iova_dev);
	}
#else
	iova_dev = iommus->iova_dev;
	iova_dev->ops->iova_reserve_relsase(iova_dev);
#endif
	mutex_unlock(&iommus->mutex);
	return;
}

struct sprd_vdsp_iommus_ops iommus_ops = {
	.init = iommus_init,
	.release = iommus_release,
	.map_all = iommus_map_all,
	.unmap_all = iommus_unmap_all,
	.map_idx = iommus_map_idx,
	.unmap_idx = iommus_unmap_idx,
	.reserve_init = iommus_reserve_init,
	.reserve_release = iommus_reserve_release,
};
