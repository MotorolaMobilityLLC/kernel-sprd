/*
 * Copyright (C) 2021-2022 UNISOC Communications Inc.
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

#include <linux/types.h>
#include <linux/list.h>
#include <linux/spinlock.h>

#include "cam_types.h"
#include "cam_buf.h"
#include "cam_block.h"
#include "isp_interface.h"
#include "isp_cfg.h"
#include "dcam_interface.h"

#define CAM_EMP_Q_LEN_INC               16
#define CAM_EMP_Q_LEN_MAX               3072
#define CAM_EMP_STATE_LEN_MAX           128
#define CAM_INT_EMP_Q_LEN_INC           48
#define CAM_INT_EMP_Q_LEN_MAX           256

enum {
	CAM_Q_INIT,
	CAM_Q_EMPTY,
	CAM_Q_FULL,
	CAM_Q_CLEAR
};

enum pyr_status {
	PYR_OFF,
	ONLINE_DEC_ON,
	OFFLINE_DEC_ON
};

struct blk_param_info {
	uint32_t update;
	struct isp_k_block *param_block;
	void *blk_param_node;
};

struct camera_interrupt {
	struct list_head list;
	uint32_t dcamint_status;
	uint32_t dcamint_status1;
	uint32_t frame_index;
};

struct camera_frame {
	struct list_head list;
	uint32_t fid;
	uint32_t width;
	uint32_t height;
	uint32_t img_fmt;
	uint32_t data_bits;
	uint16_t pattern;
	uint16_t endian;
	uint32_t evt;
	uint32_t channel_id;
	uint32_t irq_type;
	uint32_t irq_property;
	uint32_t is_reserved;
	uint32_t is_compressed;
	uint32_t compress_4bit_bypass;
	enum pyr_status pyr_status;
	/* use for dcam dec & isp rec */
	uint32_t need_pyr_rec;
	/* use for isp dec */
	uint32_t need_pyr_dec;
	/*use for isp ltm ctrl*/
	uint32_t need_ltm_hist;
	uint32_t need_ltm_map;
	uint32_t need_gtm_hist;
	uint32_t need_gtm_map;
	uint32_t gtm_mod_en;
	uint32_t data_src_dec;
	uint32_t dec_ctx_id;
	uint32_t need_dewarp;
	uint32_t not_use_isp_reserved_buf;
	uint32_t user_fid;
	uint32_t dcam_idx;
	uint32_t zoom_ratio;
	uint32_t total_zoom;
	uint32_t sw_slice_num;
	uint32_t sw_slice_no;
	uint32_t bpc_raw_flag;
	uint32_t is_raw_alg;
	uint32_t cap_cnt;/*odd:stop capture, even:start capture,frame cap_cnt match isp_ctx cap_cnt*/
	struct img_trim slice_trim;
	struct dcam_compress_info fbc_info;
	struct sprd_img_rect zoom_crop;
	struct sprd_img_rect total_zoom_crop;
	void *priv_data;
	/* for more param extend especially in offline process */
	void *param_data;
	timeval time;/* time without suspend @ISP DONE */
	ktime_t boot_time;/* ns from boot @ISP DONE */
	timeval sensor_time;/* time without suspend @SOF */
	ktime_t boot_sensor_time;/* ns from boot @SOF */
	int64_t frame_interval_time;/* dual frame diff @SOF */
	struct blk_param_info blkparam_info;
	struct camera_buf buf;
	struct nr3_me_data nr3_me;
	struct dcam_dev_param *pm;
	uint32_t is_flash_status;
};

/**
 * @list: for support en/dequeue operation
 * @state: current stream state
 * @buf_type: isp output buffer source
 * @data_src: isp input frame source
 * @in @in_crop @out @out_crop: size info
 *.@cur_cnt: current frame cout in one complete stream
 * @max_cnt: one complete stream total frame count
 * @in_fmt: input frame format
 */
struct isp_stream_ctrl {
	struct list_head list;
	enum isp_stream_state state;
	enum isp_stream_buf_type buf_type[ISP_SPATH_NUM];
	enum isp_stream_data_src data_src;
	enum isp_stream_frame_type frame_type;
	struct img_size in;
	struct img_trim in_crop;
	struct img_size out[ISP_SPATH_NUM];
	struct img_trim out_crop[ISP_SPATH_NUM];
	uint32_t cur_cnt;
	uint32_t max_cnt;
	uint32_t in_fmt;
};

struct tasklet_irq_info {
	struct list_head list;
	uint32_t is_used;
	uint32_t irq_status0;
	uint32_t irq_status1;
	/* add interrupt proc function array */
};

struct dcam_3dnrmv_ctrl {
	struct list_head list;
	struct nr3_me_data nr3_me;
};

struct camera_queue {
	uint32_t state;
	uint32_t max;
	uint32_t cnt;
	spinlock_t lock;
	struct list_head head;
	void (*destroy)(void *param);
};

#define cam_queue_dequeue(queue, type, member) ({                            \
	unsigned long __flags;                                               \
	struct camera_queue *__q = (queue);                                  \
	type *__node = NULL;                                                 \
	if (__q != NULL) {                                                   \
		spin_lock_irqsave(&__q->lock, __flags);                      \
		if ((!list_empty(&__q->head)) && (__q->cnt)                  \
			&& (__q->state != CAM_Q_CLEAR)) {                    \
			__node = list_first_entry(&__q->head, type, member); \
			if (__node)                                          \
				list_del(&__node->member);                   \
			__q->cnt--;                                          \
		}                                                            \
		spin_unlock_irqrestore(&__q->lock, __flags);                 \
	}                                                                    \
	__node;                                                              \
})

#define cam_queue_dequeue_peek(queue, type, member) ({                       \
	unsigned long __flags;                                               \
	struct camera_queue *__q = (queue);                                  \
	type *__node = NULL;                                                 \
	if (__q != NULL) {                                                   \
		spin_lock_irqsave(&__q->lock, __flags);                      \
		if ((!list_empty(&__q->head)) && (__q->cnt)                  \
			&& (__q->state != CAM_Q_CLEAR)) {                    \
			__node = list_first_entry(&__q->head, type, member); \
		}                                                            \
		spin_unlock_irqrestore(&__q->lock, __flags);                 \
	}                                                                    \
	__node;                                                              \
})

#define cam_queue_clear(queue, type, member) ({                              \
	unsigned long __flags;                                               \
	struct camera_queue *__q = (queue);                                  \
	type *__node = NULL;                                                 \
	if (__q != NULL) {                                                   \
		spin_lock_irqsave(&__q->lock, __flags);                      \
		do {                                                         \
			if ((list_empty(&__q->head)) || (__q->cnt == 0))     \
				break;                                       \
			__node = list_first_entry(&__q->head, type, member); \
			if (__node == NULL)                                  \
				break;                                       \
			list_del(&__node->member);                           \
			__q->cnt--;                                          \
			if (__q->destroy) {                                  \
				spin_unlock_irqrestore(&__q->lock, __flags); \
				__q->destroy(__node);                        \
				spin_lock_irqsave(&__q->lock, __flags);      \
			}                                                    \
		} while (1);                                                 \
		__q->cnt = 0;                                                \
		__q->max = 0;                                                \
		__q->state = CAM_Q_CLEAR;                                    \
		__q->destroy = NULL;                                         \
		INIT_LIST_HEAD(&__q->head);                                  \
		spin_unlock_irqrestore(&__q->lock, __flags);                 \
	}                                                                    \
})

#define cam_queue_del_tail(queue, type, member) ({                           \
	unsigned long __flags;                                               \
	struct camera_queue *__q = (queue);                                  \
	type *__node = NULL;                                                 \
	if (__q != NULL) {                                                   \
		spin_lock_irqsave(&__q->lock, __flags);                      \
		if ((!list_empty(&__q->head)) && (__q->cnt)                  \
			&& (__q->state != CAM_Q_CLEAR)) {                    \
			__node = list_last_entry(&__q->head, type, member);  \
			if (__node)                                          \
				list_del(&__node->member);                   \
			__q->cnt--;                                          \
		}                                                            \
		spin_unlock_irqrestore(&__q->lock, __flags);                 \
	}                                                                    \
	__node;                                                              \
})

#define cam_queue_tail_peek(queue, type, member) ({                          \
	unsigned long __flags;                                               \
	struct camera_queue *__q = (queue);                                  \
	type *__node = NULL;                                                 \
	if (__q != NULL) {                                                   \
		spin_lock_irqsave(&__q->lock, __flags);                      \
		if ((!list_empty(&__q->head)) && (__q->cnt)                  \
			&& (__q->state != CAM_Q_CLEAR)) {                    \
			__node = list_last_entry(&__q->head, type, member);  \
		}                                                            \
		spin_unlock_irqrestore(&__q->lock, __flags);                 \
	}                                                                    \
	__node;                                                              \
})

int cam_queue_enqueue_front(struct camera_queue *q, struct list_head *list);
int cam_queue_enqueue(struct camera_queue *q, struct list_head *list);
struct camera_frame *cam_queue_dequeue_tail(struct camera_queue *q);
struct camera_frame *cam_queue_dequeue_if(struct camera_queue *q,
	bool (*filter)(struct camera_frame *, void *), void *data);
int cam_queue_init(struct camera_queue *q,
			uint32_t max, void (*cb_func)(void *));
uint32_t cam_queue_cnt_get(struct camera_queue *q);
int cam_queue_same_frame_get(struct camera_queue *q0,
	struct camera_frame **pf0, int64_t t_sec, int64_t t_usec);
struct camera_frame *cam_queue_empty_frame_get(void);
int cam_queue_empty_frame_put(struct camera_frame *pframe);
void cam_queue_empty_frame_free(void *param);

struct camera_interrupt *cam_queue_empty_interrupt_get(void);
int cam_queue_empty_interrupt_put(struct camera_interrupt *interruption);
void cam_queue_empty_interrupt_free(void *param);

struct isp_stream_ctrl *cam_queue_empty_state_get(void);
void cam_queue_empty_state_put(void *param);
void cam_queue_empty_state_free(void *param);
int cam_queue_recycle_blk_param(struct camera_queue *q, struct camera_frame *param_pframe);
struct camera_frame * cam_queue_empty_blk_param_get(struct camera_queue *q);
int cam_queue_frame_param_unbind(struct camera_queue *q, struct camera_frame *p);
struct dcam_3dnrmv_ctrl *cam_queue_empty_mv_state_get(void);
void cam_queue_empty_mv_state_put(void *param);
void cam_queue_empty_mv_state_free(void *param);
#endif/* _CAM_QUEUE_H_ */
