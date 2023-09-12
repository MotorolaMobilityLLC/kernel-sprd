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

#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/scatterlist.h>

#include "sprd_vdsp_iommu_record.h"
#include "sprd_vdsp_iommus.h"

#include "sprd_iommu_test_debug.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: [mem_debug]: %d %d %s: "\
                fmt, current->pid, __LINE__, __func__

//print struct iommu_dev
void debug_print_iommu_dev(struct sprd_vdsp_iommu_dev *iommu_dev)
{
	pr_debug("----------------------------------------\n");
	pr_debug(" id             :%d \n", iommu_dev->id);
	pr_debug(" name           :%s \n", iommu_dev->name);
	pr_debug(" iommu_version  :%d \n", iommu_dev->iommu_version);
	pr_debug(" iova_base      :0x%lx \n", iommu_dev->iova_base);
	pr_debug(" iova_size      :0x%zx \n", iommu_dev->iova_size);
	pr_debug(" pgt_base       :0x%lx \n", iommu_dev->pgt_base);
	pr_debug(" pgt_size       :0x%zx \n", iommu_dev->pgt_size);
	pr_debug(" ctrl_reg       :0x%lx \n", iommu_dev->ctrl_reg);
	pr_debug(" pagt_base_ddr  :0x%lx \n", iommu_dev->pagt_base_ddr);
	pr_debug(" pagt_base_virt :0x%lx \n", iommu_dev->pagt_base_virt);
	pr_debug(" pagt_ddr_size  :0x%lx \n", iommu_dev->pagt_ddr_size);
	pr_debug(" iova_dev       :0x%lx \n", (unsigned long)iommu_dev->iova_dev);
	pr_debug("----------------------------------------\n");
}

int iommu_dump_pagetable(struct sprd_vdsp_iommu_dev *iommu_dev)
{
	unsigned long *addr = 0;
	unsigned long index = 0;
	const int field = sizeof(unsigned long) * 2;
	unsigned int unused_num = 0;

	if (unlikely(iommu_dev == NULL)) {
		pr_err("Error: iommu_dev is NULL!\n");
		return -EINVAL;
	}
	pr_debug("####start####\n");
	pr_debug("pagetable addr =0x%lx\n", iommu_dev->pagt_base_virt);
	pr_debug("pagetable size =0x%lx\n", iommu_dev->pagt_ddr_size);
	addr = (unsigned long *)iommu_dev->pagt_base_virt;
	for (index = 0; index < iommu_dev->pagt_ddr_size;) {
		if (~(*addr) != 0) {
			pr_debug("[index:%ld][addr:0x%lx]:0x%0*lx\n",
				index, (unsigned long)addr, field, *(addr));
			unused_num = 0;
		} else {
			unused_num++;
			if (unused_num == 1) {
				pr_debug("unused_num =1\n");
			}
		}
		addr = addr + 1;
		index = index + sizeof(*addr);
	}
	pr_debug("####end####\n");
	return 0;
}

void *alloc_sg_list(struct sg_table *sgt, size_t size)
{
	struct page **pages;
	void *vaddr, *ptr;
	unsigned int n_pages;
	int i;
	dma_addr_t dma_addr = 0;
	dma_addr_t phy_addr = 0;
	u32 sg_len = 0;
	u32 si = 0;
	struct scatterlist *sg_entry;

	vaddr = vmalloc(size);
	if (!vaddr)
		return NULL;
	pr_debug("vmalloc size = %ld\n", size);
	n_pages = PAGE_ALIGN(size) >> PAGE_SHIFT;
	pr_debug("n_pages  = %d\n", n_pages);
	pages = kvmalloc_array(n_pages, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		vfree(vaddr);
		pr_err("failed to allocate memory for pages\n");
		return NULL;
	}
	for (i = 0, ptr = vaddr; i < n_pages; ++i, ptr += PAGE_SIZE)
		pages[i] = vmalloc_to_page(ptr);

	if (sg_alloc_table_from_pages(sgt, pages, n_pages, 0, size, GFP_KERNEL)) {
		kvfree(pages);
		vfree(vaddr);
		pr_err("failed to allocate sgt with num_pages\n");
		return NULL;
	}
	kvfree(pages);

	//DEBUG
	pr_debug("===============================================\n");
	pr_debug("vaddr = %lx\n", (unsigned long)vaddr);
	for_each_sg(sgt->sgl, sg_entry, sgt->nents, si) {
		dma_addr = sg_dma_address(sg_entry);
		sg_len = sg_dma_len(sg_entry);
		phy_addr = sg_phys(sg_entry);	//page_to_phys(sg_page(sg)) + sg->offset;
		pr_debug("index=%d\n", si);
		pr_debug("dma_addr=%llx\n", dma_addr);
		pr_debug("phy_addr=%llx\n", phy_addr);
		pr_debug("sg_len=%d\n", sg_len);
		pr_debug("-----------------------------------------------\n");
	}
	pr_debug("===============================================\n");
	return vaddr;
}

void show_iommus_all_record(struct sprd_vdsp_iommus *iommus)
{
	unsigned int index = 0;
	struct sprd_vdsp_iommu_dev *iommu_dev = NULL;

	if (unlikely(!iommus)) {
		pr_err("Error: iommus is NULL!\n");
		return;
	}
	for (index = 0; index < SPRD_VDSP_IOMMU_MAX - 1; index++) {
		iommu_dev = iommus->iommu_devs[index];
		if (iommu_dev == NULL) {
			pr_err("Error: iommus->iommu_devs[%d] is NULL\n", index);
			continue;
		}
		pr_debug("iommus[%d]\n", index);
		if (iommu_dev->status & (0x1 << 0)) {
			iommu_dev->record_dev->ops->show_all(iommu_dev->record_dev);
		} else {
			pr_err("Error:iommu_dev->status=%d\n", iommu_dev->status);
		}
	}
}
