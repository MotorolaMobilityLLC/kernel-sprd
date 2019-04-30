/*
 * Copyright (C) 2019 Unisoc Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CAM_BUF_H_
#define _CAM_BUF_H_

#include <linux/types.h>
#include <linux/sprd_iommu.h>

enum {
	CAM_BUF_USER_TYPE,/*app alloc/free*/
	CAM_BUF_KERNEL_TYPE,/*kernel alloc/free*/
	CAM_BUF_SWAP_TYPE,/*dcam/isp swap*/
};

enum {
	CAM_BUF_STATE_MAPPING_DCAM = 0x01,
	CAM_BUF_STATE_MAPPING_ISP = 0x02,
	CAM_BUF_STATE_MAPPING = 0x03,
};

struct cam_buf_info {
	uint32_t idx;      /*camera index*/
	struct device *dev;
	uint32_t type;
	uint32_t num; /*valid buf num,max 3*/
	uint32_t state;
	size_t size[3];
	uint32_t offset[3];
	unsigned long iova[3];
	uint32_t mfd[3];
	void *buf[3];
	struct dma_buf *dmabuf_p[3];
	struct ion_client *client[3];/*for ion alloc buffer*/
	struct ion_handle *handle[3];
	uint32_t *kaddr[3];
};

static inline uint32_t sprd_cam_buf_is_valid(struct cam_buf_info *buf_info)
{
	if ((buf_info->type == CAM_BUF_USER_TYPE && buf_info->mfd[0] != 0)
	|| (buf_info->client[0] != NULL && buf_info->handle[0] != NULL))
		return 1;

	pr_debug("fail to get type mfd or client handle %d %x %p %p\n",
		buf_info->type, buf_info->mfd[0],
		buf_info->client[0], buf_info->handle[0]);

	return 0;
}

static inline uint32_t sprd_cam_buf_is_equal(struct cam_buf_info *buf_info1,
	struct cam_buf_info *buf_info2)
{
	if (buf_info1->type == CAM_BUF_USER_TYPE
		&& buf_info1->mfd[0] == buf_info2->mfd[0]
		&& buf_info1->offset[0] == buf_info2->offset[0])
		return 1;
	else if (buf_info1->type != CAM_BUF_USER_TYPE
		&& buf_info1->client[0] == buf_info2->client[0]
		&& buf_info1->handle[0] == buf_info2->handle[0])
		return 1;

	return 0;
}

int sprd_cam_buf_sg_table_get(struct cam_buf_info *info);
int sprd_cam_buf_sg_table_put(uint32_t idx);
int sprd_cam_buf_addr_map(struct cam_buf_info *info);
int sprd_cam_buf_addr_unmap(struct cam_buf_info *info);
int sprd_cam_buf_alloc(struct cam_buf_info *buf_info, uint32_t idx,
	struct device *dev, size_t size, uint32_t num, uint32_t type);

#endif /* _CAM_BUF_H_ */
