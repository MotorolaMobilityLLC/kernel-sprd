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

#include <linux/io.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include "sprd_vdsp_iommu_dev.h"
#include "sprd_vdsp_iova.h"
#include "sprd_vdsp_iommuvau_register.h"
#include "sprd_vdsp_iommuvau_cll.h"
#include "sprd_vdsp_iommu_record.h"
#include "sprd_iommu_test_debug.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: [iommu_dev]: %d %d %s: "\
        fmt, current->pid, __LINE__, __func__

static int iommu_dev_parse_dt(struct sprd_vdsp_iommu_dev *iommu_dev,
	struct device_node *iommu_dev_of_node)
{
	uint32_t val32 = 0;
	uint64_t val64 = 0;
	uint32_t size32 = 0;
	uint64_t size64 = 0;
	int ret = 0;
	unsigned long page = 0;	//For compatibility fault_page use

	//compatible
	ret = of_property_read_string(iommu_dev_of_node, "compatible", &iommu_dev->name);
	if (ret) {
		pr_err("Error: Unable to read compatible\n");
	}
	pr_debug("compatible:%s\n", iommu_dev->name);

	//iommu-version
	ret = of_property_read_u32(iommu_dev_of_node, "sprd,iommu-version", &val32);
	if (ret < 0)
		val32 = 12;	// read failed set default version

	pr_debug("sprd,iommu-version:%x\n", val32);
	iommu_dev->iommu_version = val32;

	//#address-cells
	pr_debug("#address-cells=%d\n", of_n_addr_cells(iommu_dev_of_node));
	pr_debug("#size-cells   =%d\n", of_n_size_cells(iommu_dev_of_node));

	//ctrl reg and pgt_base
	if (1 == of_n_addr_cells(iommu_dev_of_node)) {
		ret = of_property_read_u32_index(iommu_dev_of_node, "reg", 0, &val32);
		if (ret < 0) {
			pr_err("read reg base fail\n");
			return ret;
		}
		ret = of_property_read_u32_index(iommu_dev_of_node, "reg", 1, &size32);
		if (ret < 0) {
			pr_err("read reg size fail\n");
			return ret;
		}

		pr_debug("reg base:0x%x\n", val32);
		pr_debug("reg size:0x%x\n", size32);

		iommu_dev->pgt_base = (unsigned long)ioremap(val32, size32);	//0xf12f9000
		iommu_dev->pgt_size = size32;
		iommu_dev->ctrl_reg = (unsigned long)ioremap(val32, size32);	//0xf12fb000 Duplicate mapping
	} else {
		ret = of_property_read_u64_index(iommu_dev_of_node, "reg", 0, &val64);
		if (ret < 0) {
			pr_err("read reg base fail\n");
			return ret;
		}
		ret = of_property_read_u64_index(iommu_dev_of_node, "reg", 1, &size64);
		if (ret < 0) {
			pr_err("read reg size fail\n");
			return ret;
		}

		pr_debug("reg base:0x%llx\n", val64);
		pr_debug("reg size:0x%llx\n", size64);

		iommu_dev->pgt_base = (unsigned long)ioremap(val64, size64);
		iommu_dev->pgt_size = size64;
		iommu_dev->ctrl_reg = (unsigned long)ioremap(val64, size64);
	}

	BUG_ON(!iommu_dev->ctrl_reg);

	ret = of_property_read_u32(iommu_dev_of_node, "sprd,iova-base", &val32);
	if (ret < 0) {
		ret = of_property_read_u32(iommu_dev_of_node, "iova-base", &val32);
		if (ret < 0) {
			pr_err("read iova-base fail\n");
			return ret;
		}
	}
	iommu_dev->iova_base = val32;
	ret = of_property_read_u32(iommu_dev_of_node, "sprd,iova-size", &val32);
	if (ret < 0) {
		ret = of_property_read_u32(iommu_dev_of_node, "iova-size", &val32);
		if (ret < 0) {
			pr_err("read iova-size fail\n");
			return ret;
		}
	}
	iommu_dev->iova_size = val32;
	page = __get_free_page(GFP_KERNEL);
	if (page)
		iommu_dev->fault_page = virt_to_phys((void *)page);
	else
		iommu_dev->fault_page = 0;

	pr_debug("iova_base:0x%lx\n", iommu_dev->iova_base);
	pr_debug("iova_size:0x%zx\n", iommu_dev->iova_size);
	pr_debug("fault_page: 0x%lx\n", iommu_dev->fault_page);

	return 0;
}

static void iommu_dev_dt_release(struct sprd_vdsp_iommu_dev *iommu_dev)
{
	__free_page(phys_to_virt(iommu_dev->fault_page));

	pr_debug("iommu_dev_dt_release sucessed\n");

	return;
}

static int iommu_dev_iova_init(struct sprd_vdsp_iommu_dev *iommu_dev)
{
	int ret = 0;
#ifndef VDSP_IOMMU_USE_SIGNAL_IOVA
	struct sprd_vdsp_iommu_iova *iova = NULL;

	if (unlikely(iommu_dev == NULL)) {
		pr_err("Error: iommu_dev is NULL!\n");
		return -EINVAL;
	}

	iova = kzalloc(sizeof(struct sprd_vdsp_iommu_iova), GFP_KERNEL);
	if (!iova) {
		pr_err("Error: kzalloc failed\n");
		return -ENOMEM;
	}
	iommu_dev->iova_dev = iova;
	switch (VDSP_IOMMU_VERSION) {
	case 12:
		iova->ops = &version12_iova_ops;
		break;
	default:
		pr_err("Error: iommu_dev->iommu_version:%d\n", iommu_dev->iommu_version);
		return -EINVAL;
	}
	ret = iova->ops->iova_init(iova, iommu_dev->iova_base, iommu_dev->iova_size, 12);
	pr_debug("iommu_dev_iova_init sucess\n");
#endif
	return ret;
}

static void iommu_dev_iova_release(struct sprd_vdsp_iommu_dev *iommu_dev)
{
#ifndef VDSP_IOMMU_USE_SIGNAL_IOVA
	struct sprd_vdsp_iommu_iova *iova = NULL;

	iova = iommu_dev->iova_dev;
	if (!iova){
		pr_err(" iova is NULL\n");
		return;
	}
	iova->ops->iova_release(iova);
#endif
	return;
}

static int iommu_dev_pagetable_init(struct sprd_vdsp_iommu_dev *iommu_dev)
{
	unsigned long size = 0;

	if (unlikely(iommu_dev == NULL)) {
		pr_err("Error: iommu_dev is NULL!\n");
		return -EINVAL;
	}
	pr_debug("iommu_dev_pagetable_init\n");

	//pagt_base_ddr\pagt_base_virt
	size = iommu_dev->iova_size / MMU_MAPING_PAGESIZE * 4;
	iommu_dev->pagt_base_virt = (unsigned long)dma_alloc_coherent(iommu_dev->dev, size,
		(dma_addr_t*) (&(iommu_dev->pagt_base_ddr)), GFP_DMA | GFP_KERNEL);
	if (!(iommu_dev->pagt_base_virt)) {
		pr_err("Error: dma_alloc_coherent failed\n");
		return -ENOMEM;
	}
	iommu_dev->pagt_ddr_size = size;

	pr_debug("iommu %s : pgt virt 0x%lx\n", iommu_dev->name, iommu_dev->pagt_base_virt);
	pr_debug("iommu %s : pgt phy  0x%lx\n", iommu_dev->name, iommu_dev->pagt_base_ddr);
	pr_debug("iommu %s : pgt size 0x%zx\n", iommu_dev->name,
		(iommu_dev->iova_size / MMU_MAPING_PAGESIZE * 4));

	spin_lock_init(&iommu_dev->pgt_lock);	// pagetable spinlock init
	return 0;
}

static void iommu_dev_pagetable_release(struct sprd_vdsp_iommu_dev *iommu_dev)
{
	unsigned long size = 0;

	if (iommu_dev->pagt_base_virt) {
		size = iommu_dev->iova_size / MMU_MAPING_PAGESIZE * 4;
		dma_free_coherent(iommu_dev->dev, size, (void *)iommu_dev->pagt_base_virt,
			iommu_dev->pagt_base_ddr);
	}
	return;
}

static int iommu_dev_hw_init(struct sprd_vdsp_iommu_dev *iommu_dev)
{
	struct sprd_vdsp_iommu_widget *iommu_widget = NULL;	// iommu hw dev
	struct sprd_vdsp_iommu_init_param iommu_init_param;	// iommu hw init param
	int ret = 0;

	if (unlikely(iommu_dev == NULL)) {
		pr_err("Error: iommu_dev is NULL!\n");
		return -EINVAL;
	}
	memset(&iommu_init_param, 0, sizeof(struct sprd_vdsp_iommu_init_param));

	iommu_init_param.iommu_type = 0;	//get_iommuvau_type(data->iommuex_rev, &chip);
	iommu_init_param.chip = 0;	//chip;

	/*master reg base addr */
	iommu_init_param.master_reg_addr = iommu_dev->pgt_base;
	/*iommu base reg */
	iommu_init_param.ctrl_reg_addr = iommu_dev->ctrl_reg;
	/*va base addr */
	iommu_init_param.fm_base_addr = iommu_dev->iova_base;
	iommu_init_param.fm_ram_size = iommu_dev->iova_size;
	iommu_init_param.iommu_id = iommu_dev->id;
	// iommu_init_param.faultpage_addr = iommu_dev->fault_page;

	iommu_init_param.pagt_base_virt = iommu_dev->pagt_base_virt;
	iommu_init_param.pagt_base_ddr = iommu_dev->pagt_base_ddr;
	iommu_init_param.pagt_ddr_size = iommu_dev->pagt_ddr_size;

	iommu_widget = (struct sprd_vdsp_iommu_widget *)devm_kzalloc(iommu_dev->dev,
		sizeof(struct sprd_vdsp_iommu_widget), GFP_KERNEL);
	if (!iommu_widget) {
		pr_err("Error: devm_kzalloc failed\n");
		return -ENOMEM;
	}
	iommu_dev->iommu_hw_dev = iommu_widget;

	switch (iommu_dev->iommu_version) {
	case 12:
		iommu_widget->p_iommu_tbl = (&iommuvau_func_tbl);
		break;
	default:
		pr_err("Error: iommu_dev->iommu_version:%d\n", iommu_dev->iommu_version);
		return -EINVAL;
	}

	ret = iommu_widget->p_iommu_tbl->init(&iommu_init_param, iommu_widget);
	if (ret) {
		pr_err("Error:iommu_widget->p_iommu_tbl->init failed\n");
		devm_kfree(iommu_dev->dev, iommu_dev->iommu_hw_dev);
		return ret;
	}

	return 0;
}

static void iommu_dev_hw_release(struct sprd_vdsp_iommu_dev *iommu_dev)
{
	struct sprd_vdsp_iommu_widget *iommu_widget = NULL;

	iommu_widget = iommu_dev->iommu_hw_dev;
	if (iommu_widget) {
		iommu_widget->p_iommu_tbl->uninit(iommu_widget);
		devm_kfree(iommu_dev->dev, iommu_dev->iommu_hw_dev);
	}
	return;
}

static int iommu_dev_map_record_init(struct sprd_vdsp_iommu_dev *iommu_dev)
{
#ifndef VDSP_IOMMU_USE_SIGNAL_IOVA
	struct sprd_vdsp_iommu_map_record *record = NULL;

	if (unlikely(iommu_dev == NULL)) {
		pr_err("Error: iommu_dev is NULL!\n");
		return -EINVAL;
	}
	record = (struct sprd_vdsp_iommu_map_record *)devm_kzalloc(iommu_dev->dev,
		sizeof(struct sprd_vdsp_iommu_map_record), GFP_KERNEL);
	if (!record) {
		pr_err("Error: devm_kzalloc failed\n");
		return -ENOMEM;
	}
	record->ops = &iommu_map_record_ops;
	record->ops->init(record);
	iommu_dev->record_dev = record;
#endif
	return 0;
}

static void iommu_dev_map_record_relsase(struct sprd_vdsp_iommu_dev *iommu_dev)
{
#ifndef VDSP_IOMMU_USE_SIGNAL_IOVA
	struct sprd_vdsp_iommu_map_record *record = NULL;

	record = iommu_dev->record_dev;
	if (record) {
		record->ops->release(record);
		devm_kfree(iommu_dev->dev, iommu_dev->record_dev);
	}
#endif
	return;
}

static int iommu_dev_init(struct sprd_vdsp_iommu_dev *iommu_dev,
	struct device_node *of_node, struct device *dev)
{
	int ret = 0;

	if (unlikely(!iommu_dev)) {
		pr_err("Error: iommu_dev is NULL!\n");
		return -EINVAL;
	}
	if (unlikely(!of_node)) {
		pr_err("Error: of_node is NULL!\n");
		return -EINVAL;
	}
	if (unlikely(!dev)) {
		pr_err("Error: dev is NULL!\n");
		return -EINVAL;
	}

	iommu_dev->dev = dev;
	ret = iommu_dev_parse_dt(iommu_dev, of_node);
	if (ret)
		goto error_iommu_dev_parse_dt;

	ret = iommu_dev_iova_init(iommu_dev);
	if (ret)
		goto error_iova_init;

	ret = iommu_dev_pagetable_init(iommu_dev);
	if (ret)
		goto error_pagetable_init;

	ret = iommu_dev_hw_init(iommu_dev);
	if (ret)
		goto error_hw_init;

	ret = iommu_dev_map_record_init(iommu_dev);
	if (ret)
		goto error_map_record_init;

	mutex_init(&iommu_dev->mutex);
	iommu_dev->status = iommu_dev->status | (0x1 << 0);	// init done status bit

	return 0;

error_map_record_init:
	iommu_dev_hw_release(iommu_dev);
error_hw_init:
	iommu_dev_pagetable_release(iommu_dev);
error_pagetable_init:
	iommu_dev_iova_release(iommu_dev);
error_iova_init:
	iommu_dev_dt_release(iommu_dev);
error_iommu_dev_parse_dt:
	return ret;
}

static void iommu_dev_release(struct sprd_vdsp_iommu_dev *iommu_dev)
{
	if (unlikely(!iommu_dev)) {
		pr_err("Error: iommu_dev is NULL!\n");
		return;
	}
	/*
	   It does not involve the situation of reusing the page table after restroe,
	   and directly releases the resources without performing the operation of traversing the record and unmapping.
	 */
	if (!iommu_dev->iova_dev)
		iommu_dev_iova_release(iommu_dev);
	if (!iommu_dev->iommu_hw_dev)
		iommu_dev_hw_release(iommu_dev);
	if (!iommu_dev->pagt_base_ddr)
		iommu_dev_pagetable_release(iommu_dev);
	if (!iommu_dev->record_dev)
		iommu_dev_map_record_relsase(iommu_dev);

	iommu_dev_dt_release(iommu_dev);
	return;
}
#ifndef VDSP_IOMMU_USE_SIGNAL_IOVA
static int iommu_dev_map(struct sprd_vdsp_iommu_dev *iommu_dev,
	struct sprd_vdsp_iommu_map_conf *map_conf)
{
	struct sprd_vdsp_iommu_iova *iova_dev = NULL;
	struct sprd_vdsp_iommu_widget *iommu_hw_dev = NULL;
	struct sprd_vdsp_iommu_map_record *record_dev = NULL;
	struct sg_table *sg_table = NULL;
	unsigned long iova = 0;
	struct sprd_iommu_map_param map_param;
	unsigned long irq_flag = 0;
	int ret;

	if (unlikely(iommu_dev == NULL || map_conf == NULL)) {
		pr_err("Error: iommu_dev or map_conf is NULL!\n");
		return -EINVAL;
	}

	if (unlikely(!map_conf->buf_addr)) {
		pr_err("Error: map_conf->buf_addr is NULL!\n");
		return -EINVAL;
	}

	iova_dev = iommu_dev->iova_dev;
	if (unlikely(!iova_dev)) {
		pr_err("Error iova_dev is NULL!\n");
		return -EINVAL;
	}
	iommu_hw_dev = iommu_dev->iommu_hw_dev;
	if (unlikely(!iommu_hw_dev)) {
		pr_err("Error: iommu_hw_dev is NULL!\n");
		return -EINVAL;
	}
	if (unlikely(iommu_hw_dev->p_iommu_tbl->map == NULL)) {
		pr_err("Error: iommu_hw_dev->p_iommu_tbl->map is NULL!\n");
		return -EINVAL;
	}
	record_dev = iommu_dev->record_dev;
	if (unlikely(!record_dev)) {
		pr_err("Error: record_dev is NULL!\n");
		return -EINVAL;
	}
	mutex_lock(&iommu_dev->mutex);
	if (record_dev->ops->map_check(record_dev, map_conf->buf_addr, (unsigned long *)&iova)) {
		map_conf->iova_addr = iova;
		pr_warn("Warning: buf 0x%lx has been mapped,iova = 0x%lx\n", map_conf->buf_addr, iova);
		mutex_unlock(&iommu_dev->mutex);
		return 0;
	}
	sg_table = map_conf->table;

	if (map_conf->isfixed == 1 || map_conf->isfixed == 2) {
		iova = map_conf->fixed_data;
		if (map_conf->isfixed == 1) { // 1:fixed offset 2:fixed addr
			iova = iova + iova_dev->iova_base;
		}
		ret = iova_dev->ops->iova_alloc_fixed(iova_dev, &iova, map_conf->iova_size);
		if (ret) {
			pr_err("Error: iova_alloc_fixed failed\n");
			mutex_unlock(&iommu_dev->mutex);
			return ret;
		}
	} else {
		iova = iova_dev->ops->iova_alloc(iova_dev, map_conf->iova_size);
		if (iova < 0) {
			pr_err("Error: iova_alloc failed\n");
			ret = iova;
			mutex_unlock(&iommu_dev->mutex);
			return ret;
		}
	}
	memset(&map_param, 0, sizeof(map_param));
	map_param.start_virt_addr = iova;
	map_param.total_map_size = map_conf->iova_size;
	map_param.p_sg_table = sg_table;

	spin_lock_irqsave(&iommu_dev->pgt_lock, irq_flag);
	ret = iommu_hw_dev->p_iommu_tbl->map(iommu_hw_dev, &map_param);
	spin_unlock_irqrestore(&iommu_dev->pgt_lock, irq_flag);

	if (ret) {
		pr_err("Error:iommu_hw_dev->p_iommu_tbl->map fialed:ret = %x\n", ret);
		iova_dev->ops->iova_free(iova_dev, iova, map_conf->iova_size);	//iova_free return void
		map_conf->iova_addr = 0;
		mutex_unlock(&iommu_dev->mutex);
		return ret;
	}

	map_conf->iova_addr = iova;
	record_dev->ops->insert_slot(record_dev, ( unsigned long) sg_table,
		map_conf->buf_addr, map_conf->iova_addr, map_conf->iova_size);
	mutex_unlock(&iommu_dev->mutex);

	return ret;
}
#else
static int iommu_dev_map(struct sprd_vdsp_iommu_dev *iommu_dev,
	struct sprd_vdsp_iommu_map_conf *map_conf)
{
	struct sprd_vdsp_iommu_iova *iova_dev = NULL;
	struct sprd_vdsp_iommu_widget *iommu_hw_dev = NULL;
	struct sprd_vdsp_iommu_map_record *record_dev = NULL;
	struct sg_table *sg_table = NULL;
	unsigned long iova = 0;
	struct sprd_iommu_map_param map_param;
	unsigned long irq_flag = 0;
	int ret;

	if (unlikely(iommu_dev == NULL || map_conf == NULL)) {
		pr_err("Error: iommu_dev or map_conf is NULL!\n");
		return -EINVAL;
	}

	if (unlikely(!map_conf->buf_addr)) {
		pr_err("Error: map_conf->buf_addr is NULL!\n");
		return -EINVAL;
	}

	iova_dev = iommu_dev->iommus->iova_dev;
	if (unlikely(!iova_dev)) {
		pr_err("Error iommus iova_dev is NULL!\n");
		return -EINVAL;
	}
	iommu_hw_dev = iommu_dev->iommu_hw_dev;
	if (unlikely(!iommu_hw_dev)) {
		pr_err("Error: iommu_hw_dev is NULL!\n");
		return -EINVAL;
	}
	if (unlikely(iommu_hw_dev->p_iommu_tbl->map == NULL)) {
		pr_err("Error: iommu_hw_dev->p_iommu_tbl->map is NULL!\n");
		return -EINVAL;
	}
	record_dev = iommu_dev->iommus->record_dev;
	if (unlikely(!record_dev)) {
		pr_err("Error: iommus record_dev is NULL!\n");
		return -EINVAL;
	}
	mutex_lock(&iommu_dev->mutex);
	if (record_dev->ops->map_check(record_dev, map_conf->buf_addr, (unsigned long *)&iova)) {
		map_conf->iova_addr = iova;
		pr_warn("Warning: buf 0x%lx has been mapped,iova = 0x%lx\n", map_conf->buf_addr, iova);
		mutex_unlock(&iommu_dev->mutex);
		return 0;
	}
	sg_table = map_conf->table;

	memset(&map_param, 0, sizeof(map_param));
	map_param.start_virt_addr = map_conf->iova_addr;
	map_param.total_map_size = map_conf->iova_size;
	map_param.p_sg_table = sg_table;

	spin_lock_irqsave(&iommu_dev->pgt_lock, irq_flag);
	ret = iommu_hw_dev->p_iommu_tbl->map(iommu_hw_dev, &map_param);
	spin_unlock_irqrestore(&iommu_dev->pgt_lock, irq_flag);

	if (ret) {
		pr_err("Error:iommu_hw_dev->p_iommu_tbl->map fialed:ret = %x\n", ret);
		mutex_unlock(&iommu_dev->mutex);
		return ret;
	}
	mutex_unlock(&iommu_dev->mutex);
	return ret;
}
#endif

#ifndef VDSP_IOMMU_USE_SIGNAL_IOVA
static int iommu_dev_unmap(struct sprd_vdsp_iommu_dev *iommu_dev,
	struct sprd_vdsp_iommu_unmap_conf *unmap_conf)
{
	struct sprd_vdsp_iommu_iova *iova_dev = NULL;
	struct sprd_vdsp_iommu_widget *iommu_hw_dev = NULL;
	struct sprd_vdsp_iommu_map_record *record_dev = NULL;
	struct sprd_iommu_unmap_param unmap_param; //temp
	unsigned long irq_flag = 0;
	int ret;

	if (unlikely(iommu_dev == NULL || unmap_conf == NULL)) {
		pr_err("Error: iommu_dev or unmap_conf is NULL!\n");
		return -EINVAL;
	}

	iova_dev = iommu_dev->iova_dev;
	if (unlikely(!iova_dev)) {
		pr_err("Error: iova_dev is NULL!\n");
		return -EINVAL;
	}
	iommu_hw_dev = iommu_dev->iommu_hw_dev;
	if (unlikely(!iommu_hw_dev)) {
		pr_err("Error: iommu_hw_dev is NULL!\n");
		return -EINVAL;
	}
	if (unlikely(iommu_hw_dev->p_iommu_tbl->unmap == NULL)) {
		pr_err("Error: iommu_hw_dev->p_iommu_tbl->map is NULL!\n");
		return -EINVAL;
	}
	record_dev = iommu_dev->record_dev;
	if (unlikely(!record_dev)) {
		pr_err("Error: record_dev is NULL!\n");
		return -EINVAL;
	}
	mutex_lock(&iommu_dev->mutex);
	if (!(record_dev->ops->unmap_check(record_dev, unmap_conf->buf_addr))) {
		pr_err("Error: buf_addr 0x%lx is not iova mapped \n", unmap_conf->buf_addr);
		mutex_unlock(&iommu_dev->mutex);
		return -EINVAL;
	}

	memset(&unmap_param, 0, sizeof(unmap_param));
	unmap_param.start_virt_addr = unmap_conf->iova_addr;
	unmap_param.total_map_size = unmap_conf->iova_size;

	spin_lock_irqsave(&iommu_dev->pgt_lock, irq_flag);
	ret = iommu_hw_dev->p_iommu_tbl->unmap(iommu_hw_dev, &unmap_param);
	spin_unlock_irqrestore(&iommu_dev->pgt_lock, irq_flag);

	if (ret) {
		pr_err("Error: iommu_hw_dev->p_iommu_tbl->unmap failed\n");
		mutex_unlock(&iommu_dev->mutex);
		return -1;
	}
	iova_dev->ops->iova_free(iova_dev, unmap_conf->iova_addr, unmap_conf->iova_size);
	record_dev->ops->remove_slot(record_dev, unmap_conf->iova_addr);
	mutex_unlock(&iommu_dev->mutex);
	return 0;
}
#else
static int iommu_dev_unmap(struct sprd_vdsp_iommu_dev *iommu_dev,
	struct sprd_vdsp_iommu_unmap_conf *unmap_conf)
{
	struct sprd_vdsp_iommu_widget *iommu_hw_dev = NULL;
	struct sprd_vdsp_iommu_map_record *record_dev = NULL;
	struct sprd_iommu_unmap_param unmap_param;	//临时，兼容变量
	unsigned long irq_flag = 0;
	int ret;

	if (unlikely(iommu_dev == NULL || unmap_conf == NULL)) {
		pr_err("Error: iommu_dev or unmap_conf is NULL!\n");
		return -EINVAL;
	}
	iommu_hw_dev = iommu_dev->iommu_hw_dev;
	if (unlikely(!iommu_hw_dev)) {
		pr_err("Error: iommu_hw_dev is NULL!\n");
		return -EINVAL;
	}
	if (unlikely(iommu_hw_dev->p_iommu_tbl->unmap == NULL)) {
		pr_err("Error: iommu_hw_dev->p_iommu_tbl->map is NULL!\n");
		return -EINVAL;
	}
	record_dev = iommu_dev->iommus->record_dev;
	if (unlikely(!record_dev)) {
		pr_err("Error: record_dev is NULL!\n");
		return -EINVAL;
	}
	mutex_lock(&iommu_dev->mutex);
	if (!(record_dev->ops->unmap_check(record_dev, unmap_conf->buf_addr))) {
		pr_err("Error: buf_addr 0x%lx is not iova mapped \n", unmap_conf->buf_addr);
		mutex_unlock(&iommu_dev->mutex);
		return -EINVAL;
	}

	memset(&unmap_param, 0, sizeof(unmap_param));
	unmap_param.start_virt_addr = unmap_conf->iova_addr;
	unmap_param.total_map_size = unmap_conf->iova_size;

	spin_lock_irqsave(&iommu_dev->pgt_lock, irq_flag);
	ret = iommu_hw_dev->p_iommu_tbl->unmap(iommu_hw_dev, &unmap_param);
	spin_unlock_irqrestore(&iommu_dev->pgt_lock, irq_flag);

	if (ret) {
		pr_err("Error: iommu_hw_dev->p_iommu_tbl->unmap failed\n");
		mutex_unlock(&iommu_dev->mutex);
		return -1;
	}
	mutex_unlock(&iommu_dev->mutex);
	return 0;
}
#endif
struct sprd_vdsp_iommu_dev_ops iommu_dev_ops = {
	.init = iommu_dev_init,
	.release = iommu_dev_release,
	.map = iommu_dev_map,
	.unmap = iommu_dev_unmap,
};

