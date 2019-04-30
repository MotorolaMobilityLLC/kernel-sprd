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

#include <linux/err.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>

#include "cam_common.h"
#include "cam_queue.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "cam_queue: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define MAGIC_BUF_QUEUE 0x5a5a5a5a
#define MAGIC_FRM_QUEUE 0x9b9b9b9b

int sprd_cam_queue_buf_init(struct cam_buf_queue *queue,
	int node_size, int node_num, char *q_str)
{
	int ret = 0;

	if (queue == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	memset(queue, 0x00, sizeof(*queue));
	queue->node = vzalloc(node_size * node_num + strlen(q_str) + 1);
	if (queue->node) {
		queue->write = &queue->node[0];
		queue->read = &queue->node[0];
		queue->node_size = node_size;
		queue->node_num = node_num;
		queue->q_str = queue->node + (node_size * node_num);
		strcpy(queue->q_str, q_str);
		spin_lock_init(&queue->lock);
		pr_debug("buf queue (%s) init ok\n", queue->q_str);
		ret = 0;
	} else {
		pr_err("fail to init buf queue (%s)\n", queue->q_str);
		ret = -ENOMEM;
	}

	return ret;
}

int sprd_cam_queue_buf_write(struct cam_buf_queue *queue, void *node)
{
	char *ori_node;
	char *last_node;
	unsigned long flags = 0;

	if (queue == NULL || queue->node == NULL
		|| node == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&queue->lock, flags);
	ori_node = queue->write;
	last_node = queue->node + queue->node_size * (queue->node_num - 1);
	memcpy(queue->write, node, queue->node_size);
	queue->write += queue->node_size;
	queue->wcnt++;
	if (queue->write > last_node)
		queue->write = &queue->node[0];

	if (queue->write == queue->read) {
		queue->write = ori_node;
		queue->wcnt--;
		pr_info("warning, buf queue (%s) is full\n", queue->q_str);
	}
	spin_unlock_irqrestore(&queue->lock, flags);
	pr_debug("buf queue (%s) w ok\n", queue->q_str);

	return 0;
}

int sprd_cam_queue_buf_read(struct cam_buf_queue *queue, void *node)
{
	int ret = 0;
	int flag = 0;
	unsigned long flags = 0;
	char *last_node;

	if (queue == NULL || queue->node == NULL
		|| node == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

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
		pr_debug("buf queue (%s) r ok\n", queue->q_str);
	}
	if (!flag) {
		ret = -EAGAIN;
		pr_debug("buf queue (%s) is empty!\n", queue->q_str);
	}
	spin_unlock_irqrestore(&queue->lock, flags);

	return ret;
}

int sprd_cam_queue_buf_clear(struct cam_buf_queue *queue)
{
	int ret = 0;
	unsigned long flags = 0;

	if (queue == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	if (queue->node) {
		spin_lock_irqsave(&queue->lock, flags);
		queue->write = &queue->node[0];
		queue->read = &queue->node[0];
		memset(queue->node, 0, queue->node_size * queue->node_num);
		spin_unlock_irqrestore(&queue->lock, flags);
		ret = 0;
	} else {
		pr_err("fail to get valid buf queue!\n");
		ret = -ENOMEM;
	}

	return ret;
}

int sprd_cam_queue_buf_deinit(struct cam_buf_queue *queue)
{
	unsigned long flags = 0;

	if (queue == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&queue->lock, flags);
	if (queue->node) {
		vfree(queue->node);
		queue->node = NULL;
		queue->write = NULL;
		queue->read = NULL;
	}
	spin_unlock_irqrestore(&queue->lock, flags);
	return 0;
}

uint32_t sprd_cam_queue_buf_cur_nodes(struct cam_buf_queue *queue)
{
	unsigned long flags = 0;
	uint32_t node_num = 0;

	if (queue == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&queue->lock, flags);
	if (queue->write > queue->read)
		node_num = (queue->write - queue->read) / queue->node_size;
	else if (queue->write < queue->read)
		node_num = queue->node_num -
		(queue->read - queue->write) / queue->node_size;
	else
		node_num = 0;
	spin_unlock_irqrestore(&queue->lock, flags);

	return node_num;
}

static int sprd_camqueue_frm_check(struct cam_frm_queue *queue)
{
	if (queue->magic != MAGIC_FRM_QUEUE
		|| queue->node_size == 0 || queue->node_num == 0) {
		pr_err("fail to get valid frm queue (%s)n",
			queue->q_str);
		return -1;
	}
	return 0;
}

int sprd_cam_queue_frm_init(struct cam_frm_queue *queue,
		int node_size, int node_num, char *q_str)
{
	int ret = 0;

	if (queue == NULL || node_size <= 0 || node_num <= 0) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	memset(queue, 0x00, sizeof(*queue));
	queue->frm_array = vzalloc(node_size * node_num + strlen(q_str) + 1);
	if (queue->frm_array) {
		queue->magic = MAGIC_FRM_QUEUE;
		queue->node_size = node_size;
		queue->node_num = node_num;
		queue->q_str = queue->frm_array + (node_size * node_num);
		strcpy(queue->q_str, q_str);
		spin_lock_init(&queue->lock);
		pr_debug("frm queue (%s) init ok\n", queue->q_str);
		ret = 0;
	} else {
		pr_err("fail to init frm queue (%s)\n", queue->q_str);
		ret = -ENOMEM;
	}

	return ret;
}

int sprd_cam_queue_frm_deinit(struct cam_frm_queue *queue)
{
	unsigned long flags = 0;

	if (queue == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&queue->lock, flags);
	if (queue->frm_array) {
		vfree(queue->frm_array);
		queue->frm_array = NULL;
	}
	spin_unlock_irqrestore(&queue->lock, flags);

	return 0;
}

int sprd_cam_queue_frm_enqueue(struct cam_frm_queue *queue,
			void *node)
{
	unsigned long flags = 0;

	if (queue == NULL || queue->frm_array == NULL
		|| node == NULL) {
		pr_err("fail to get valid parm %p, %p\n", queue, node);
		return -1;
	}

	if (sprd_camqueue_frm_check(queue))
		return -1;

	spin_lock_irqsave(&queue->lock, flags);

	if (queue->valid_cnt >= queue->node_num) {
		pr_info("frm queue (%s) over flow\n", queue->q_str);
		spin_unlock_irqrestore(&queue->lock, flags);
		return -1;
	}

	memcpy(queue->frm_array + queue->node_size * queue->valid_cnt, node,
			queue->node_size);
	queue->valid_cnt++;
	spin_unlock_irqrestore(&queue->lock, flags);

	return 0;
}

int sprd_cam_queue_frm_dequeue(struct cam_frm_queue *queue,
			void *node)
{
	uint32_t i = 0;
	unsigned long flags = 0;

	if (queue == NULL || queue->frm_array == NULL
		|| node == NULL) {
		pr_err("fail to get valid parm %p, %p\n",
			queue, node);
		return -1;
	}

	if (sprd_camqueue_frm_check(queue))
		return -1;

	spin_lock_irqsave(&queue->lock, flags);

	if (queue->valid_cnt == 0) {
		pr_debug("fail to get frm queue (%s): under flow\n",
			queue->q_str);
		spin_unlock_irqrestore(&queue->lock, flags);
		return -1;
	}

	memcpy(node, queue->frm_array, queue->node_size);
	queue->valid_cnt--;
	for (i = 0; i < queue->valid_cnt; i++) {
		memcpy(queue->frm_array + queue->node_size * i,
				queue->frm_array + queue->node_size * (i + 1),
				queue->node_size);
	}
	spin_unlock_irqrestore(&queue->lock, flags);

	return 0;
}

int sprd_cam_queue_frm_dequeue_n(struct cam_frm_queue *queue,
			uint32_t n, void *node)
{
	uint32_t i = 0;
	unsigned long flags = 0;

	if (queue == NULL || queue->frm_array == NULL
		|| node == NULL) {
		pr_err("fail to get valid parm %p, %p\n",
			queue, node);
		return -1;
	}

	if (sprd_camqueue_frm_check(queue))
		return -1;

	spin_lock_irqsave(&queue->lock, flags);

	if (queue->valid_cnt == 0 || n >= queue->valid_cnt) {
		pr_err("fail to get frm queue (%s): %d %d\n",
			queue->q_str, n, queue->valid_cnt);
		spin_unlock_irqrestore(&queue->lock, flags);
		return -1;
	}

	memcpy(node, queue->frm_array + n * queue->node_size, queue->node_size);
	queue->valid_cnt--;
	for (i = n; i < queue->valid_cnt - n; i++) {
		memcpy(queue->frm_array + queue->node_size * i,
				queue->frm_array + queue->node_size * (i + 1),
				queue->node_size);
	}
	spin_unlock_irqrestore(&queue->lock, flags);

	return 0;
}

int sprd_cam_queue_frm_cur_nodes(struct cam_frm_queue *queue)
{
	unsigned long flags = 0;
	uint32_t valid_cnt;

	if (queue == NULL || queue->frm_array == NULL) {
		pr_err("fail to get valid parm %p\n", queue);
		return -1;
	}

	if (sprd_camqueue_frm_check(queue))
		return -1;

	spin_lock_irqsave(&queue->lock, flags);
	valid_cnt = queue->valid_cnt;
	spin_unlock_irqrestore(&queue->lock, flags);

	return valid_cnt;
}

int sprd_cam_queue_frm_firstnode_get(struct cam_frm_queue *queue,
			void **node)
{
	unsigned long flags = 0;

	if (queue == NULL || queue->frm_array == NULL
		|| node == NULL) {
		pr_err("fail to get valid parm %p, %p\n",
			queue, node);
		return -1;
	}

	if (sprd_camqueue_frm_check(queue))
		return -1;

	spin_lock_irqsave(&queue->lock, flags);

	if (queue->valid_cnt == 0) {
		pr_err("fail to get frm queue (%s):under flow\n",
			queue->q_str);
		spin_unlock_irqrestore(&queue->lock, flags);
		return -1;
	}

	*node = queue->frm_array;
	spin_unlock_irqrestore(&queue->lock, flags);

	return 0;
}

void sprd_cam_queue_frm_clear(struct cam_frm_queue *queue)
{
	unsigned long flags = 0;

	if (queue == NULL) {
		pr_err("fail to get valid heap %p\n", queue);
		return;
	}

	if (sprd_camqueue_frm_check(queue))
		return;

	spin_lock_irqsave(&queue->lock, flags);
	if (queue->frm_array)
		memset((void *)queue->frm_array, 0,
			queue->node_size * queue->node_num);
	queue->valid_cnt = 0;
	spin_unlock_irqrestore(&queue->lock, flags);
}
