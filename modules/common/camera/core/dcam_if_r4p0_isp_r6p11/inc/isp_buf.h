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

#ifndef _ISP_BUF_HEADER_
#define _ISP_BUF_HEADER_

#include "isp_drv.h"
#include "isp_cfg.h"

#define MM_ION_OFFSET 0x80000000
#define LSC_BUF_NAME                            "2D_LSC"

extern struct platform_device *s_isp_pdev;
extern spinlock_t isp_mod_lock;
extern uint32_t is_dual_cam;

int isp_frame_enqueue(struct isp_frm_queue *queue,
	struct camera_frame *frame);
int isp_frame_dequeue(struct isp_frm_queue *queue,
	struct camera_frame *frame);
int isp_pframe_peekqueue(struct isp_frm_queue *queue,
	struct camera_frame **pframe);

int isp_queue_init(struct isp_queue *queue);
int32_t isp_queue_read(struct isp_queue *queue, struct isp_node *node);
void isp_buf_queue_init(struct isp_buf_queue *queue);
int isp_buf_queue_peek(struct isp_buf_queue *queue,
                       struct camera_frame *frame);
int isp_buf_queue_read_if(struct isp_buf_queue *queue,
                          bool (*filter)(struct camera_frame *, void *),
                          void *data,
                          struct camera_frame *frame);
int isp_buf_queue_read(struct isp_buf_queue *queue,
	struct camera_frame *frame);
int isp_buf_queue_write(struct isp_buf_queue *queue,
	struct camera_frame *frame);
void isp_frm_clear(struct isp_pipe_dev *dev, enum isp_path_index path_index);
int isp_buf_recycle(struct offline_buf_desc *buf_desc,
		    struct isp_buf_queue *dst_q,
		    struct isp_frm_queue *src_q,
		    uint32_t recycle_buf_cnt);
struct offline_buf_desc *isp_offline_sel_buf(
	struct isp_offline_desc *off_desc, uint8_t off_type);
int get_off_frm_q_len(
	struct isp_offline_desc *off_desc, uint32_t *len);
int isp_offline_init_buf(
	struct isp_offline_desc *off_desc, uint8_t off_type, bool queue_only);
int isp_offline_get_buf(
	struct isp_offline_desc *off_desc, uint8_t off_type);
int isp_offline_put_buf(
	struct isp_offline_desc *off_desc, uint8_t off_type);
int isp_offline_buf_iommu_map(
	struct isp_offline_desc *off_desc, uint8_t off_type);
int isp_offline_buf_iommu_unmap(
	struct isp_offline_desc *off_desc, uint8_t off_type);
int isp_offline_buf_iommu_unmap_external(
	struct isp_offline_desc *off_desc, uint8_t off_type);
int isp_offline_set_next_frm(struct isp_module *module,
	uint8_t off_type, struct camera_frame *out_frame);
void isp_frm_queue_clear(struct isp_frm_queue *queue);
int isp_coeff_queue_init(struct isp_sc_array *scl_array);
int isp_coeff_get_new_node(struct isp_sc_coeff_queue *queue,
			   struct isp_sc_coeff **coeff, int type);
int isp_coeff_get_valid_node(struct isp_sc_coeff_queue *queue,
			     struct isp_sc_coeff **coeff, int type);
int isp_gen_buf_alloc(struct isp_buf_info *buf_info);
int isp_gen_buf_free(struct isp_buf_info *buf_info);
int isp_gen_buf_hw_map(struct isp_buf_info *buf_info);
int isp_gen_buf_hw_unmap(struct isp_buf_info *buf_info);
void *isp_buf_get_kaddr(int fd);
int isp_block_buf_alloc(struct isp_pipe_dev *dev);
int isp_block_buf_free(struct isp_pipe_dev *dev);
#endif

