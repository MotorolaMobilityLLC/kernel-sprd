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
#include <linux/err.h>
#include <linux/vmalloc.h>

#include "cam_util.h"

int sprd_cam_queue_init(struct cam_queue *queue, int node_size, int node_num)
{
	if (queue == NULL)
		return -EINVAL;

	memset(queue, 0, sizeof(*queue));
	queue->node = vzalloc(node_size * node_num);
	queue->write = &queue->node[0];
	queue->read = &queue->node[0];
	queue->node_size = node_size;
	queue->node_num = node_num;
	spin_lock_init(&queue->lock);
	return 0;
}

int sprd_cam_queue_write(struct cam_queue *queue,
				void *node)
{
	char *ori_node;
	char *last_node;
	unsigned long flags;

	if (queue == NULL || node == NULL)
		return -EINVAL;

	spin_lock_irqsave(&queue->lock, flags);
	ori_node = queue->write;
	last_node = queue->node + queue->node_size * (queue->node_num - 1);
	queue->wcnt++;
	memcpy(queue->write, node, queue->node_size);
	queue->write += queue->node_size;
	if (queue->write > last_node)
		queue->write = &queue->node[0];

	if (queue->write == queue->read) {
		queue->write = ori_node;
		pr_info("warning, queue is full\n");
	}
	spin_unlock_irqrestore(&queue->lock, flags);
	return 0;
}

int sprd_cam_queue_read(struct cam_queue *queue, void *node)
{
	int ret = 0;
	int flag = 0;
	unsigned long flags;
	char *last_node;

	if (queue == NULL || node == NULL)
		return -EINVAL;

	spin_lock_irqsave(&queue->lock, flags);
	if (queue->read != queue->write) {
		flag = 1;
		last_node = queue->node +
			queue->node_size * (queue->node_num - 1);

		memcpy(node, queue->read, queue->node_size);
		queue->read += queue->node_size;
		queue->rcnt++;
		if (queue->read > last_node)
			queue->read = &queue->node[0];
	}
	if (!flag)
		ret = -EAGAIN;
	spin_unlock_irqrestore(&queue->lock, flags);

	return ret;
}

int sprd_cam_queue_deinit(struct cam_queue *queue)
{
	unsigned long flags;

	if (queue == NULL)
		return -EINVAL;

	spin_lock_irqsave(&queue->lock, flags);
	if (queue->node) {
		vfree(queue->node);
		queue->node = NULL;
	}
	spin_unlock_irqrestore(&queue->lock, flags);
	return 0;
}

