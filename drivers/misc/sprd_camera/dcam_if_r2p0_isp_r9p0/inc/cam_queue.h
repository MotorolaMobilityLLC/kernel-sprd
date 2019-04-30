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

#ifndef _CAM_QUEUE_H_
#define _CAM_QUEUE_H_

#include <linux/spinlock.h>

/*genenal queue for camera system, when queue full, nodes will not be assert*/
struct cam_buf_queue {
	char *node;
	char *write;
	char *read;
	uint32_t wcnt;
	uint32_t rcnt;
	uint32_t node_num;
	uint32_t node_size;
	spinlock_t lock;
	char *q_str;/*debug string, can be NULL*/
};

int sprd_cam_queue_buf_init(struct cam_buf_queue *queue,
	int node_size, int node_num, char *q_str);
int sprd_cam_queue_buf_write(struct cam_buf_queue *queue, void *node);
int sprd_cam_queue_buf_read(struct cam_buf_queue *queue, void *node);
int sprd_cam_queue_buf_clear(struct cam_buf_queue *queue);
int sprd_cam_queue_buf_deinit(struct cam_buf_queue *queue);
uint32_t sprd_cam_queue_buf_cur_nodes(struct cam_buf_queue *queue);

/*genenal queue for camera system, when queue full, nodes will not be assert*/
struct cam_frm_queue {
	uint32_t magic;
	uint32_t valid_cnt;
	char *frm_array;
	uint32_t node_size;
	uint32_t node_num;
	spinlock_t lock;
	char *q_str;/*debug string, can be NULL*/
};

int sprd_cam_queue_frm_init(struct cam_frm_queue *queue,
		int node_size, int node_num, char *q_str);
int sprd_cam_queue_frm_enqueue(struct cam_frm_queue *queue,
			void *node);
int sprd_cam_queue_frm_dequeue(struct cam_frm_queue *queue,
			void *node);
int sprd_cam_queue_frm_dequeue_n(struct cam_frm_queue *queue,
			uint32_t n, void *node);
int sprd_cam_queue_frm_cur_nodes(struct cam_frm_queue *queue);
int sprd_cam_queue_frm_firstnode_get(struct cam_frm_queue *queue,
			void **node);
void sprd_cam_queue_frm_clear(struct cam_frm_queue *queue);
int sprd_cam_queue_frm_deinit(struct cam_frm_queue *queue);

#endif /* _CAM_QUEUE_H_ */
