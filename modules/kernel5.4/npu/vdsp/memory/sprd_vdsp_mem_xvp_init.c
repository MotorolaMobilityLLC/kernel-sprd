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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of.h>
#include "sprd_vdsp_mem_core.h"
#include "sprd_vdsp_mem_xvp_init.h"
#include "sprd_vdsp_mem_test_debug.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: [mem_xvp]: %d %d %s: "\
        fmt, current->pid, __LINE__, __func__

static struct heap_config heap_configs[] = {
	{
		.type = SPRD_VDSP_MEM_HEAP_TYPE_UNIFIED,
		.options.unified = {
			.gfp_type = GFP_KERNEL | __GFP_ZERO,
		},
		.to_dev_addr = NULL,
	},
	{
		.type = SPRD_VDSP_MEM_HEAP_TYPE_DMABUF,
		.to_dev_addr = NULL,
	},
	{
		.type = SPRD_VDSP_MEM_HEAP_TYPE_ANONYMOUS,
		.to_dev_addr = NULL,
	},
#ifdef FACEID_VDSP_FULL_TEE
	{
		.type = SPRD_VDSP_MEM_HEAP_TYPE_CARVEOUT,	//NOTE: for test can set static parameter
		.to_dev_addr = NULL,
	},
#endif
};

static int sprd_vdsp_parse_reserved_mem(struct heap_config *hc)
{
	struct device_node *np;
	u32 out_values[4];
	int ret;

	np = of_find_node_by_name(NULL, "vdsp-mem");
	if (!np) {
		pr_err("Error: find vdsp-mem node failed\n");
		return -ENOENT;
	}
	ret = of_property_read_u32_array(np, "reg", out_values, 4);

	if (!ret) {
		hc->options.carveout.phys = out_values[1];
		hc->options.carveout.size = out_values[3];
	} else {
		pr_err("Error: read vdsp-mem reg failed\n");
		return -ENOENT;
	}
	pr_debug("vdsp-mem node: mem addr %#llx size %#lx\n",
		hc->options.carveout.phys, hc->options.carveout.size);
	return 0;
}

int sprd_vdsp_mem_xvp_init(struct xvp *xvp)
{
	int ret = 0;
	unsigned int i;
	unsigned int heaps = 0;
	struct xvp_heap *xvp_heap = NULL;
	struct xvp_mem_dev *xvp_mem_dev = NULL;

	if (!xvp) {
		pr_err("Error: xvp is NULL\n");
		return -EINVAL;
	}

	xvp->mem_dev = kzalloc(sizeof(struct xvp_mem_dev), GFP_KERNEL);
	xvp_mem_dev = xvp->mem_dev;
	if (!xvp_mem_dev) {
		ret = -ENOMEM;
		pr_err("failed to alloc xvp_mem_dev!\n");
		return -ENOMEM;
	}
	pr_debug("xvp->mem_dev=%p\n", xvp->mem_dev);
	// mem man init
	sprd_vdsp_mem_core_init();

	//xvp mem: heap init
	INIT_LIST_HEAD(&xvp_mem_dev->heaps);
	INIT_LIST_HEAD(&xvp_mem_dev->buf_list);
	mutex_init(&xvp_mem_dev->buf_list_mutex);
	heaps = sizeof(heap_configs) / sizeof(struct heap_config);
	pr_debug("heaps = %d\n", heaps);

	for (i = 0; i < heaps; i++) {
		pr_debug("adding heap of type %d\n", heap_configs[i].type);

		if (heap_configs[i].type == SPRD_VDSP_MEM_HEAP_TYPE_CARVEOUT) {
			sprd_vdsp_parse_reserved_mem(&heap_configs[i]);
		}
		xvp_heap = kzalloc(sizeof(struct xvp_heap), GFP_KERNEL);
		if (!xvp_heap) {
			ret = -ENOMEM;
			pr_err("failed to alloc xvp_heap!\n");
			goto heap_add_failed;
		}

		ret = sprd_vdsp_mem_add_heap(&heap_configs[i], &xvp_heap->id);
		if (ret < 0) {
			pr_err("failed to init heap (type %d)!\n", heap_configs[i].type);
			kfree(xvp_heap);
			goto heap_add_failed;
		}
		list_add(&xvp_heap->list_node, &xvp_mem_dev->heaps);
	}
	//xvp mem: drv ctx init
	ret = sprd_vdsp_mem_create_proc_ctx(&xvp_mem_dev->xvp_mem_ctx);
	if (ret) {
		pr_err("Error: failed to create mem context (err:%d)!\n", ret);
		goto create_proc_ctx_failed;
	}

	xvp_mem_dev->xvp_mem_ctx->xvp = xvp;
	xvp->drv_mem_ctx = xvp_mem_dev->xvp_mem_ctx;

	pr_debug("sprd vdsp mem vxp init done\n");

	return 0;

create_proc_ctx_failed:
heap_add_failed:
	while (!list_empty(&xvp_mem_dev->heaps)) {
		xvp_heap = list_first_entry(&xvp_mem_dev->heaps, struct xvp_heap, list_node);
		list_del(&xvp_heap->list_node);
		sprd_vdsp_mem_del_heap(xvp_heap->id);
		kfree(xvp_heap);
	}
	sprd_vdsp_mem_core_exit();

	return ret;
}

int sprd_vdsp_mem_xvp_release(struct xvp_mem_dev *xvp_mem_dev)
{
	struct xvp_heap *heap = NULL;
	struct xvp_buf *buf = NULL;

	/* Deinitialize memory management component */
	while (!list_empty(&xvp_mem_dev->heaps)) {
		heap = list_first_entry(&xvp_mem_dev->heaps, struct xvp_heap, list_node);
		list_del(&heap->list_node);
		sprd_vdsp_mem_del_heap(heap->id);
		kfree(heap);
	}
	mutex_lock(&xvp_mem_dev->buf_list_mutex);
	while (!list_empty(&xvp_mem_dev->buf_list)) {
		buf = list_first_entry(&xvp_mem_dev->buf_list, struct xvp_buf, list_node);
		list_del(&buf->list_node);
		kfree(buf);
	}
	mutex_unlock(&xvp_mem_dev->buf_list_mutex);
	sprd_vdsp_mem_core_exit();	//NOTE: now only one vdsp ip,so free with mem_man
	kfree(xvp_mem_dev);
	return 0;
}

static inline int vxp_buf_node_in_list(const struct list_head *node)
{
	// ASSERT(node);
	return (node->next != NULL && node->prev != NULL && node->next != node
		&& node->next != LIST_POISON1 && node->prev != LIST_POISON2);
}

int xvp_mem_check_args(struct xvp *xvp, struct xvp_buf *xvp_buf,
	struct mem_ctx **mem_ctx)
{
	struct xvp_mem_dev *mem_dev = NULL;

	if (unlikely(!xvp)) {
		pr_err("Error: xvp is NULL");
		return -EINVAL;
	}
	if (unlikely(!xvp_buf)) {
		pr_err("Error: xvp_buf is NULL");
		return -EINVAL;
	}
	mem_dev = xvp->mem_dev;
	if (!mem_dev) {
		pr_err("Error: mem_dev is NULL");
		return -EINVAL;
	}
	if (mem_ctx) {
		*mem_ctx = xvp->mem_dev->xvp_mem_ctx;
		if (!*mem_ctx) {
			pr_err("Error: mem_ctx is NULL");
			return -EINVAL;
		}
	}
	if (!vxp_buf_node_in_list(&xvp_buf->list_node)) {
		pr_err("Error: xvp_buf->list_node is not in list\n");
		pr_err("Error: xvp_buf must be alloc by __xvp_buf_creat\n");
		return -EINVAL;
	}

	return 0;
}

struct xvp_buf *__xvp_buf_creat(struct xvp *xvp, char *name, uint64_t size,
	uint32_t type, uint32_t attr)
{
	struct xvp_buf *buf = NULL;
	struct xvp_mem_dev *mem_dev = NULL;

	if (unlikely(!xvp)) {
		pr_err("Error: xvp is NULL");
		return NULL;
	}
	mem_dev = xvp->mem_dev;
	if (!mem_dev) {
		pr_err("Error: mem_dev is NULL");
		return NULL;
	}
	buf = kzalloc(sizeof(struct xvp_buf), GFP_KERNEL);
	if (!buf) {
		pr_err("Error kzalloc xvp_buf failed\n");
		return NULL;
	}

	strncpy(( void *) buf->name, ( void *) name, sizeof(buf->name));
	buf->name[sizeof(buf->name) - 1] = '\0';
	buf->size = size;
	buf->heap_type = type;
	buf->attributes = attr;
	list_add_tail(&buf->list_node, &mem_dev->buf_list);
	return buf;

}

int __xvp_buf_destroy(struct xvp *xvp, struct xvp_buf *xvp_buf)
{

	struct xvp_mem_dev *mem_dev = NULL;

	if (unlikely(!xvp)) {
		pr_err("Error: xvp is NULL");
		return -EINVAL;
	}
	if (unlikely(!xvp_buf)) {
		pr_err("Error: xvp_buf is NULL");
		return -EINVAL;
	}
	mem_dev = xvp->mem_dev;
	if (!mem_dev) {
		pr_err("Error: mem_dev is NULL");
		return -EINVAL;
	}
	list_del(&xvp_buf->list_node);
	kfree(xvp_buf);
	xvp_buf = NULL;
	return 0;
}

int __xvp_buf_alloc(struct xvp *xvp, struct xvp_buf *xvp_buf)
{
	struct mem_ctx *mem_ctx = NULL;
	unsigned int heap_id = 0;
	int ret = 0;

	ret = xvp_mem_check_args(xvp, xvp_buf, &mem_ctx);
	if (ret) {
		pr_err("Error: input args EINVAL\n");
		return ret;
	}
	pr_debug("xvp_alloc_buffer :\"%s\"\n", xvp_buf->name);

	heap_id = sprd_vdsp_mem_get_heap_id(xvp_buf->heap_type);
	if (heap_id == -1) {
		pr_err("Error: sprd_vdsp_mem_get_heap_id failed\n");
		return -1;
	}

	ret = sprd_vdsp_mem_alloc(xvp->dev, mem_ctx, heap_id, xvp_buf->size,
		xvp_buf->attributes, &xvp_buf->buf_id);

	if (unlikely(ret)) {
		pr_err("Error:  \"%s\" alloc failed\n", xvp_buf->name);
		return -ENOMEM;
	}

	if ((sprd_vdsp_mem_get_heap_id(SPRD_VDSP_MEM_HEAP_TYPE_CARVEOUT) == heap_id)
		|| (sprd_vdsp_mem_get_heap_id(SPRD_VDSP_MEM_HEAP_TYPE_UNIFIED) == heap_id)) {
		xvp_buf->paddr = sprd_vdsp_mem_get_phy_addr(mem_ctx, xvp_buf->buf_id);
	}
	pr_debug("\"%s\" alloc sucessed, buffer id %d\n", xvp_buf->name, xvp_buf->buf_id);

	return 0;
}

int __xvp_buf_free(struct xvp *xvp, struct xvp_buf *xvp_buf)
{
	struct mem_ctx *mem_ctx = NULL;
	int ret = 0;

	ret = xvp_mem_check_args(xvp, xvp_buf, &mem_ctx);
	if (ret) {
		pr_err("Error: __xvp_buf_free failed, input args EINVAL\n");
		return ret;
	}
	sprd_vdsp_mem_free(mem_ctx, xvp_buf->buf_id);
	xvp_buf->buf_id = 0;
	pr_debug("xvp_free_buffer \"%s\" sucessed\n", xvp_buf->name);
	return 0;
}

int xvp_buf_kmap(struct xvp *xvp, struct xvp_buf *xvp_buf)
{
	struct mem_ctx *mem_ctx = NULL;
	int ret = 0;

	pr_debug("xvp buf kmap start\n");

	ret = xvp_mem_check_args(xvp, xvp_buf, &mem_ctx);
	if (ret) {
		pr_err("Error: input args EINVAL\n");
		return ret;
	}
	//why get  twice here.
	xvp_buf->vaddr = sprd_vdsp_mem_get_kptr(mem_ctx, xvp_buf->buf_id);

	pr_debug("xvp buf kmap vaddr:%lx\n", ( unsigned long) (xvp_buf->vaddr));

	BUG_ON(xvp_buf->vaddr);
	ret = sprd_vdsp_mem_map_km(mem_ctx, xvp_buf->buf_id);
	if (unlikely(0 != ret)) {
		pr_err("Error: \"%s\" kmap fialed\n", xvp_buf->name);
		return -EFAULT;
	}
	xvp_buf->vaddr = sprd_vdsp_mem_get_kptr(mem_ctx, xvp_buf->buf_id);

	pr_debug("\"%s\" kmap sucessed ,vaddr:%lx,size:%lld\n",
		xvp_buf->name, ( unsigned long) (xvp_buf->vaddr), xvp_buf->size);

	return 0;
}

int xvp_buf_kunmap(struct xvp *xvp, struct xvp_buf *xvp_buf)
{

	struct mem_ctx *mem_ctx = NULL;
	int ret = 0;

	ret = xvp_mem_check_args(xvp, xvp_buf, &mem_ctx);
	if (ret) {
		pr_err("Error: input args EINVAL\n");
		return ret;
	}
	BUG_ON(!xvp_buf->vaddr);
	xvp_buf->vaddr = sprd_vdsp_mem_get_kptr(mem_ctx, xvp_buf->buf_id);
	BUG_ON(!xvp_buf->vaddr);
	ret = sprd_vdsp_mem_unmap_km(mem_ctx, xvp_buf->buf_id);
	if (unlikely(0 != ret)) {
		pr_err("Error: \"%s\" kunmap fialed\n", xvp_buf->name);
		return -EFAULT;
	}
	xvp_buf->vaddr = NULL;
	pr_debug("\"%s\" kunmap sucessed\n", xvp_buf->name);

	return 0;
}

int xvp_buf_iommu_map(struct xvp *xvp, struct xvp_buf *xvp_buf)
{
	struct mem_ctx *mem_ctx = NULL;
	int ret = 0;

	ret = xvp_mem_check_args(xvp, xvp_buf, &mem_ctx);
	if (ret) {
		pr_err("Error: input args EINVAL\n");
		return ret;
	}
	xvp_buf->iova = sprd_vdsp_mem_get_dev_addr(mem_ctx, xvp_buf->buf_id);

	if (xvp_buf->iova) {
		pr_warn("Warning: \"%s\" is been aiommu_maped  ,iova:%#llx\n",
			xvp_buf->name, xvp_buf->iova);
		//BUG_ON(xvp_buf->iova);
		return 0;
	}

	ret = sprd_vdsp_mem_map_iova(mem_ctx, xvp_buf->buf_id, xvp_buf->isfixed, xvp_buf->fixed_data);
	if (unlikely(ret)) {
		pr_err("Error: \"%s\" iommu_map fialed\n", xvp_buf->name);
		return ret;
	}
	xvp_buf->iova = sprd_vdsp_mem_get_dev_addr(mem_ctx, xvp_buf->buf_id);
	pr_debug("\"%s\" iommu_map sucessed ,iova:%#llx\n", xvp_buf->name, xvp_buf->iova);
	return 0;
}

int xvp_buf_iommu_unmap(struct xvp *xvp, struct xvp_buf *xvp_buf)
{
	struct mem_ctx *mem_ctx = NULL;
	int ret = 0;

	ret = xvp_mem_check_args(xvp, xvp_buf, &mem_ctx);
	if (ret) {
		pr_err("Error: input args EINVAL\n");
		return ret;
	}

	if (likely(xvp_buf->iova)) {
		// xvp_buf->iova = sprd_vdsp_mem_get_dev_addr(mem_ctx, xvp_buf->buf_id);
		// BUG_ON(xvp_buf->iova == 0);
		ret = sprd_vdsp_mem_unmap_iova(mem_ctx, xvp_buf->buf_id);
		if (unlikely(ret)) {
			pr_err("Error: \"%s\" iommu_unmap fialed\n", xvp_buf->name);
			return -EFAULT;
		}
	} else {
		pr_err("Error: xvp_buf->iova is NULL\n");
		return -EINVAL;
	}
	xvp_buf->iova = 0;
	pr_debug("\"%s\" iommu_unmap sucessed\n", xvp_buf->name);

	return 0;
}

struct xvp_buf *xvp_buf_alloc(struct xvp *xvp, char *name, uint64_t size,
	uint32_t type, uint32_t attr)
{

	struct xvp_buf *buf = NULL;
	int ret = 0;
	struct xvp_mem_dev *mem_dev = xvp->mem_dev;

	pr_debug("xvp buf alloc, name[%s] size[%lld] type[%d] attr[%d]\n", name, size, type, attr);

	if (!mem_dev) {
		pr_err("Error: mem_dev is NULL");
		return NULL;
	}
	mutex_lock(&mem_dev->buf_list_mutex);
	buf = __xvp_buf_creat(xvp, name, size, type, attr);
	if (!buf) {
		mutex_unlock(&mem_dev->buf_list_mutex);
		goto err;
	}
	ret = __xvp_buf_alloc(xvp, buf);
	if (ret) {
		mutex_unlock(&mem_dev->buf_list_mutex);
		goto err_alloc;
	}
	mutex_unlock(&mem_dev->buf_list_mutex);
	pr_debug("xvp_buf_alloc \"%s\" sucessed\n", buf->name);
	return buf;

err_alloc:
	__xvp_buf_destroy(xvp, buf);
err:
	return NULL;
}

int xvp_buf_free(struct xvp *xvp, struct xvp_buf *buf)
{
	unsigned int buf_id = buf->buf_id;
	struct xvp_mem_dev *mem_dev = xvp->mem_dev;

	if (!mem_dev) {
		pr_err("Error: mem_dev is NULL");
		return -EINVAL;
	}

	mutex_lock(&mem_dev->buf_list_mutex);
	if (__xvp_buf_free(xvp, buf)) {
		goto err;
	}
	if (__xvp_buf_destroy(xvp, buf)) {
		goto err;
	}
	mutex_unlock(&mem_dev->buf_list_mutex);
	pr_debug("xvp_buf_free buf_id=%d sucessed\n", buf_id);
	return 0;
err:
	mutex_unlock(&mem_dev->buf_list_mutex);
	return -1;

}

struct xvp_buf *xvp_buf_alloc_with_iommu(struct xvp *xvp, char *name,
	uint64_t size, uint32_t type, uint32_t attr)
{
	struct xvp_buf *buf;

	buf = xvp_buf_alloc(xvp, name, size, type, attr);
	if (!buf) {
		goto err;
	}
	if (xvp_buf_iommu_map(xvp, buf)) {
		xvp_buf_free(xvp, buf);
		goto err;
	}
	return buf;
err:
	pr_err("Error:xvp_buf_alloc_with_iommu failed,buf name=%s\n", name);
	return NULL;
}

int xvp_buf_free_with_iommu(struct xvp *xvp, struct xvp_buf *buf)
{

	BUG_ON(buf->iova == 0);
	if (xvp_buf_iommu_unmap(xvp, buf)) {
		return -1;
	}
	if (xvp_buf_free(xvp, buf)) {
		return -1;
	}
	return 0;
}

struct xvp_buf *xvp_buf_get_by_id(struct xvp *xvp, uint32_t buf_id)
{
	struct xvp_buf *buf = NULL;
	struct xvp_mem_dev *mem_dev = NULL;
	bool find = false;

	if (unlikely(!xvp)) {
		pr_err("Error: xvp is NULL");
		return NULL;
	}
	mem_dev = xvp->mem_dev;
	if (!mem_dev) {
		pr_err("Error: mem_dev is NULL");
		return NULL;
	}
	pr_debug("get buf_id=%d\n", buf_id);

	mutex_lock(&mem_dev->buf_list_mutex);
	list_for_each_entry(buf, &mem_dev->buf_list, list_node)
	{
		if (buf->buf_id != buf_id) {
			continue;
		}
		find = true;
		break;
	}
	mutex_unlock(&mem_dev->buf_list_mutex);
	if (find) {
		debug_xvp_buf_print(buf);
		return buf;
	} else {
		pr_err("Error: xvp_buf_get_by_id failed id=%d\n", buf_id);
		return NULL;
	}
}

int xvp_buf_free_by_id(struct xvp *xvp, uint32_t buf_id)
{
	struct xvp_buf *buf = NULL;

	buf = xvp_buf_get_by_id(xvp, buf_id);
	if (!buf) {
		goto err;
	}
	if (!xvp_buf_free(xvp, buf)) {
		goto err;
	}
	return 0;
err:
	pr_err("Error: xvp_buf_free_by_id failed\n");
	return -1;
}

int xvp_buf_iommu_map_by_id(struct xvp *xvp, uint32_t buf_id)
{
	struct xvp_buf *buf = NULL;

	buf = xvp_buf_get_by_id(xvp, buf_id);
	if (!buf) {
		goto err;
	}
	if (xvp_buf_iommu_map(xvp, buf)) {
		goto err;
	}
	return 0;
err:
	pr_err("Error: xvp_buf_iommu_map_by_id failed\n");
	return -1;
}

int xvp_buf_iommu_unmap_by_id(struct xvp *xvp, uint32_t buf_id)
{
	struct xvp_buf *buf = NULL;

	buf = xvp_buf_get_by_id(xvp, buf_id);
	if (!buf) {
		goto err;
	}
	if (xvp_buf_iommu_unmap(xvp, buf)) {
		goto err;
	}
	return 0;
err:
	pr_err("Error: xvp_buf_iommu_unmap_by_id failed\n");
	return -1;
}

void *xvp_buf_get_vaddr(struct xvp_buf *buf)
{
	if (unlikely(!buf)) {
		pr_err("Error: buf is NULL");
		return NULL;
	}
	if (unlikely(!buf->vaddr)) {
		pr_err("Error: \"%s\" buf->vaddr is NULL", buf->name);
		return NULL;
	}
	return buf->vaddr;
}

phys_addr_t xvp_buf_get_iova(struct xvp_buf *buf)
{
	if (unlikely(!buf)) {
		pr_err("Error: buf is NULL");
		return 0;
	}
	if (unlikely(!buf->iova)) {
		pr_err("Error: \"%s\" buf->iova is NULL", buf->name);
		return 0;
	}
	return buf->iova;
}

void *xvp_buf_get_vaddr_by_id(struct xvp *xvp, uint32_t buf_id)
{
	struct xvp_buf *buf = NULL;

	buf = xvp_buf_get_by_id(xvp, buf_id);
	if (!buf) {
		return NULL;
	}
	return xvp_buf_get_vaddr(buf);
}

phys_addr_t xvp_buf_get_iova_by_id(struct xvp *xvp, uint32_t buf_id)
{
	struct xvp_buf *buf = NULL;

	buf = xvp_buf_get_by_id(xvp, buf_id);
	if (!buf) {
		return 0;
	}
	return xvp_buf_get_iova(buf);
}

