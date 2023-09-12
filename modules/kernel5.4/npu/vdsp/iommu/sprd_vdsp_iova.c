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

#include <linux/genalloc.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "sprd_vdsp_iova.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: [iommu_iova]: %d %d %s: "\
        fmt, current->pid, __LINE__, __func__

static int genalloc_iova_init(struct sprd_vdsp_iommu_iova *iova,
	unsigned long iova_base, size_t iova_size,
	int min_alloc_order)
{
	int ret = 0;

	if (unlikely(!iova)) {
		pr_err("Error: iova is NULL\n");
		return -EINVAL;
	}
	iova->iova_base = iova_base;
	iova->iova_size = iova_size;
	iova->pool = gen_pool_create(min_alloc_order, -1);
	if (!iova->pool) {
		pr_err("gen_pool_create error\n");
		return -1;
	}
	ret = gen_pool_add(iova->pool, iova_base, iova_size, -1);
	if (ret) {
		gen_pool_destroy(iova->pool);
		return ret;
	}
	pr_debug("iova init success,iova->iova_base=0x%lx,iova->iova_size=0x%zx",
		iova->iova_base, iova->iova_size);
	return 0;
}

static void genalloc_iova_release(struct sprd_vdsp_iommu_iova *iova)
{
	gen_pool_destroy(iova->pool);	//return type of void
	return;
}

static unsigned long genalloc_iova_alloc(struct sprd_vdsp_iommu_iova *iova,
	size_t iova_length)
{
	unsigned long iova_addr = 0;

	iova_addr = gen_pool_alloc(iova->pool, iova_length);	//gen_pool_alloc failed return 0
	if (!iova_addr) {
		pr_err("Error: gen_pool_alloc failed!\n");
		return 0;
	}
	return iova_addr;
}

static void genalloc_iova_free(struct sprd_vdsp_iommu_iova *iova,
	unsigned long iova_addr, size_t iova_length)
{
	struct iova_reserve *reserve = iova->reserve_data;
	int i;

	pr_debug("iova->pool:0x%lx\n", ( unsigned long) iova->pool);
	pr_debug("iova_addr:0x%lx\n", iova_addr);
	//free reserve iova
	if (reserve) {
		for (i = 0; i < iova->reserve_num; i++) {
			if ((iova_addr == reserve[i].iova_addr)
				&& (iova_length == reserve[i].size)) {
				reserve->status = 0;
				return;
			}
		}
	}

	gen_pool_free(iova->pool, iova_addr, iova_length);	//return type of void
	return;
}

static int genalloc_reserve_init(struct sprd_vdsp_iommu_iova *iova,
	struct iova_reserve reserve_data[],
	unsigned int reserve_num)
{
	unsigned long iova_addr = 0;
	struct iova_reserve *reserve = NULL;
	int i;
	if (reserve_data != NULL && reserve_num > 0) {
		reserve = (struct iova_reserve *)kzalloc(sizeof(struct iova_reserve) * reserve_num, GFP_KERNEL);
		if (!reserve) {
			pr_err("Error: kzalloc  failed\n");
			return -ENOMEM;
		}
		memcpy(reserve, reserve_data, sizeof(struct iova_reserve) * reserve_num);

		for (i = 0; i < reserve_num; i++) {
			iova_addr = gen_pool_alloc_algo(iova->pool, reserve[i].size, gen_pool_fixed_alloc,
				&reserve[i].fixed);
			if (!iova_addr) {
				pr_err("Error: gen_pool_alloc_algo failed!\n");
				goto error_alloc_faied;
			}
			reserve[i].iova_addr = iova_addr;
			reserve[i].status = 0;
		}
	}
	iova->reserve_data = reserve;
	iova->reserve_num = reserve_num;
	pr_debug("iova_reserve_init sucessed\n");
	return 0;

error_alloc_faied:
	for (i = 0; i < reserve_num; i++) {
		if (reserve_data[i].iova_addr) {
			gen_pool_free(iova->pool, reserve_data[i].iova_addr, reserve_data[i].size);
		}
	}
	kfree(reserve);
	return -1;
}

static void genalloc_reserve_release(struct sprd_vdsp_iommu_iova *iova)
{
	struct iova_reserve *reserve = iova->reserve_data;
	int i;
	if (reserve) {
		for (i = 0; i < iova->reserve_num; i++) {
			if (reserve->status == 1) {
				pr_warn("Warning: iova reserver [%s] is uesd\n", reserve->name);
			}
			gen_pool_free(iova->pool, reserve->iova_addr, reserve->size);	//return type of void
			reserve->iova_addr = 0;
			reserve->status = 0;
		}
	}
	kfree(iova->reserve_data);
	iova->reserve_data = NULL;
	iova->reserve_num = 0;
	return;
}

static int genalloc_iova_alloc_fixed(struct sprd_vdsp_iommu_iova *iova,
	unsigned long *iova_addr, size_t iova_length)
{
	unsigned long iova_start = *iova_addr;
	struct iova_reserve *res;
	int i;
	int matchflag = 0;
	struct genpool_data_fixed fixed;

	if ((iova_start < iova->iova_base)
		|| (iova_start + iova_length > iova->iova_base + iova->iova_size)) {
		pr_err("Error: input parameter error\n");
		return -EINVAL;
	}

	if (iova->reserve_data != NULL) {
		for (i = 0; i < iova->reserve_num; i++) {
			res = &iova->reserve_data[i];
			// if(strcmp(res->name,name)==0){
			if ((res->iova_addr == iova_start) && (res->size == iova_length)) {
				pr_debug("iova match %s\n", res->name);
				matchflag = 1;
			}
		}
	}
	if (matchflag) {	// use reserve iova
		if (res->status == 1) {
			pr_err("Error: reserve_data is used\n");
			*iova_addr = 0;
			return -1;
		} else {
			res->status = 1;
			return 0;
		}
	} else {		// try fixed alloc
		pr_warn("Warning: reserve iova match failed,try fixed alloc\n");
		fixed.offset = iova_start - iova->iova_base;
		if (gen_pool_alloc_algo(iova->pool, iova_length, gen_pool_fixed_alloc, &fixed)) {
			pr_debug("fixed alloc sucessed\n");
			return 0;
		} else {
			pr_err("Error:genalloc_iova_alloc_fixed failed\n");
			*iova_addr = 0;
			return -1;
		}
	}
	return 0;
}

struct iommu_iova_ops version12_iova_ops = {
	.iova_init = genalloc_iova_init,
	.iova_release = genalloc_iova_release,
	.iova_alloc = genalloc_iova_alloc,
	.iova_free = genalloc_iova_free,
	.iova_reserve_init = genalloc_reserve_init,
	.iova_reserve_relsase = genalloc_reserve_release,
	.iova_alloc_fixed = genalloc_iova_alloc_fixed,
};
