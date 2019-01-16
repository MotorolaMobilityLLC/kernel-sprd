/*
 * Copyright (C) 2015-2016 Spreadtrum Communications Inc.
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
#ifndef _CAM_UTIL_H_
#define _CAM_UTIL_H_

#include <linux/spinlock.h>

struct cam_queue {
	char *node;
	char *write;
	char *read;
	unsigned int wcnt;
	unsigned int rcnt;
	unsigned int node_num;
	unsigned int node_size;
	spinlock_t lock;
};

int sprd_cam_queue_init(struct cam_queue *queue, int node_size, int node_num);
int sprd_cam_queue_write(struct cam_queue *queue, void *node);
int sprd_cam_queue_read(struct cam_queue *queue, void *node);
int sprd_cam_queue_deinit(struct cam_queue *queue);


#endif /* _CAM_IOMMU_H_ */
