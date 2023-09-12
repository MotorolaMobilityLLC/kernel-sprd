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

#include <linux/types.h>
#include "sprd_vdsp_mem_xvp_init.h"
#include "sprd_vdsp_mem_xvpfile.h"
#include "sprd_vdsp_mem_test_debug.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: [mem_xvpfile]: %d %d %s: "\
        fmt, current->pid, __LINE__, __func__

int xvpfile_buf_init(struct xvp_file *xvp_file)
{
	INIT_LIST_HEAD(&xvp_file->buf_list);
	mutex_init(&xvp_file->xvpfile_buf_list_lock);

	return 0;
}

int xvpfile_buf_deinit(struct xvp_file *xvp_file)
{
	struct xvp_buf *buf;

	mutex_lock(&xvp_file->xvpfile_buf_list_lock);
	while (!list_empty(&xvp_file->buf_list)) {
		buf = list_first_entry(&xvp_file->buf_list, struct xvp_buf, xvp_file_list_node);
		if (buf->iova) {
			if (xvpfile_buf_iommu_unmap(xvp_file, buf)) {
				mutex_unlock(&xvp_file->xvpfile_buf_list_lock);
				return -1;
			}
		}
		list_del(&buf->xvp_file_list_node);
		if (xvp_buf_free(xvp_file->xvp, buf)) {
			mutex_unlock(&xvp_file->xvpfile_buf_list_lock);
			return -1;
		}
	}
	mutex_unlock(&xvp_file->xvpfile_buf_list_lock);
	return 0;
}

struct xvp_buf *xvpfile_buf_alloc(struct xvp_file *xvp_file, char *name,
	uint64_t size, uint32_t type, uint32_t attr)
{

	struct xvp_buf *buf = NULL;

	buf = xvp_buf_alloc(xvp_file->xvp, name, size, type, attr);
	if (!buf) {
		goto err;
	}
	buf->owner = ( unsigned long) xvp_file;
	//what is meaning here.
	mutex_lock(&xvp_file->xvpfile_buf_list_lock);
	list_add_tail(&buf->xvp_file_list_node, &xvp_file->buf_list);
	mutex_unlock(&xvp_file->xvpfile_buf_list_lock);
	return buf;
err:
	pr_err("xvpfile_buf_alloc \"%s\" failed\n", buf->name);
	return NULL;
}

int xvpfile_buf_free(struct xvp_file *xvp_file, struct xvp_buf *buf)
{

	struct xvp *xvp = NULL;

	xvp = xvp_file->xvp;
	if (!xvp) {
		pr_err("Error xvp is NULL\n");
		return -EINVAL;
	}
	mutex_lock(&xvp_file->xvpfile_buf_list_lock);
	list_del(&buf->xvp_file_list_node);
	mutex_unlock(&xvp_file->xvpfile_buf_list_lock);
	if (xvp_buf_free(xvp, buf)) {
		return -1;
	}
	return 0;
}

struct xvp_buf *xvpfile_buf_alloc_with_iommu(struct xvp_file *xvp_file,
	char *name, uint64_t size, uint32_t type, uint32_t attr)
{
	struct xvp_buf *buf = NULL;

	buf = xvp_buf_alloc_with_iommu(xvp_file->xvp, name, size, type, attr);
	if (!buf) {
		goto err;
	}
	buf->owner = ( unsigned long) xvp_file;
	mutex_lock(&xvp_file->xvpfile_buf_list_lock);
	list_add_tail(&buf->xvp_file_list_node, &xvp_file->buf_list);
	mutex_unlock(&xvp_file->xvpfile_buf_list_lock);
	return buf;
err:
	pr_err("Error: xvfilep_buf_alloc_with_iommu failed,buf name=%s\n", name);
	return NULL;
}

int xvpfile_buf_free_with_iommu(struct xvp_file *xvp_file, struct xvp_buf *buf)
{
	mutex_lock(&xvp_file->xvpfile_buf_list_lock);
	list_del(&buf->xvp_file_list_node);
	mutex_unlock(&xvp_file->xvpfile_buf_list_lock);
	if (xvp_buf_free_with_iommu(xvp_file->xvp, buf)) {
		return -1;
	}
	return 0;
}

int xvpfile_buf_kmap(struct xvp_file *xvp_file, struct xvp_buf *buf)
{
	return xvp_buf_kmap(xvp_file->xvp, buf);
}

int xvpfile_buf_kunmap(struct xvp_file *xvp_file, struct xvp_buf *buf)
{
	return xvp_buf_kunmap(xvp_file->xvp, buf);
}

int xvpfile_buf_iommu_map(struct xvp_file *xvp_file, struct xvp_buf *buf)
{
	return xvp_buf_iommu_map(xvp_file->xvp, buf);
}

int xvpfile_buf_iommu_unmap(struct xvp_file *xvp_file, struct xvp_buf *buf)
{
	return xvp_buf_iommu_unmap(xvp_file->xvp, buf);
}

struct xvp_buf *xvpfile_buf_get(struct xvp_file *xvp_file, uint32_t buf_id)
{
	struct xvp_buf *buf = NULL;

	mutex_lock(&xvp_file->xvpfile_buf_list_lock);
	list_for_each_entry(buf, &xvp_file->buf_list, xvp_file_list_node)
	{
		if (buf->buf_id == buf_id) {
			mutex_unlock(&xvp_file->xvpfile_buf_list_lock);
			return buf;
		}
	}
	mutex_unlock(&xvp_file->xvpfile_buf_list_lock);
	return NULL;
}

void *xvpfile_buf_get_vaddr(struct xvp_buf *buf)
{
	return xvp_buf_get_vaddr(buf);
}

phys_addr_t xvpfile_buf_get_iova(struct xvp_buf *buf)
{
	return xvp_buf_get_iova(buf);
}

struct xvp_buf *xvpfile_buf_import(struct xvp_file *xvp_file, char *name,
	uint64_t size, uint32_t heap_id, uint32_t attr, uint64_t buf_fd, uint64_t cpu_ptr)
{
	struct xvp *xvp = xvp_file->xvp;
	struct xvp_buf *buf = NULL;
	int ret = 0;

	struct xvp_mem_dev *mem_dev = xvp->mem_dev;

	if (unlikely(!xvp)) {
		pr_err("Error: xvp is NULL");
		return NULL;
	}
	if (!mem_dev) {
		pr_err("Error: mem_dev is NULL");
		return NULL;
	}

	mutex_lock(&mem_dev->buf_list_mutex);
	buf = __xvp_buf_creat(xvp, name, size, heap_id, attr);
	if (!buf) {
		mutex_unlock(&mem_dev->buf_list_mutex);
		return NULL;
	}
	ret = sprd_vdsp_mem_import(xvp->dev, xvp->drv_mem_ctx, heap_id, ( size_t) size, attr, buf_fd,
		cpu_ptr, &buf->buf_id);
	if (ret) {
		pr_err("Error: sprd_vdsp_mem_import failed\n");
		mutex_unlock(&mem_dev->buf_list_mutex);
		goto err_vdsp_mem_import;
	}

	if ((sprd_vdsp_mem_get_heap_id(SPRD_VDSP_MEM_HEAP_TYPE_CARVEOUT) == heap_id)
		|| (sprd_vdsp_mem_get_heap_id(SPRD_VDSP_MEM_HEAP_TYPE_DMABUF) == heap_id)) {
		buf->paddr = sprd_vdsp_mem_get_phy_addr(xvp->drv_mem_ctx, buf->buf_id);
	}

	buf->buf_hnd = buf_fd;	// need modify later
	buf->owner = ( unsigned long) xvp_file;

	list_add_tail(&buf->xvp_file_list_node, &xvp_file->buf_list);
	mutex_unlock(&mem_dev->buf_list_mutex);

	return buf;

err_vdsp_mem_import:
	__xvp_buf_destroy(xvp, buf);
	return NULL;
}

int xvpfile_buf_export(struct xvp_file *xvp_file, struct xvp_buf *buf)
{

	struct xvp *xvp = NULL;

	xvp = xvp_file->xvp;
	if (!xvp) {
		pr_err("Error xvp is NULL\n");
		return -EINVAL;
	}
	if (sprd_vdsp_mem_export(xvp->dev, xvp->drv_mem_ctx, buf->buf_id,
		( size_t) buf->size, buf->attributes, &buf->buf_hnd)) {
		return -1;
	}
	return 0;
}
