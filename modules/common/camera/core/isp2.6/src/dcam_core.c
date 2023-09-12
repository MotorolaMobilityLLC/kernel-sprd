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

#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/sprd_ion.h>
#include <linux/sprd_iommu.h>
#include <linux/sprd_ion.h>
#include <isp_hw.h>
#include "sprd_img.h"
#include <sprd_mm.h>

#include "cam_debugger.h"
#include "dcam_reg.h"
#include "dcam_int.h"
#include "dcam_path.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "DCAM_CORE: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define DCAM_STOP_TIMEOUT             msecs_to_jiffies(3500)

/* VCH2 maybe used for raw picture output
 * If yes, PDAF should not output data though VCH2
 * todo: avoid conflict between raw/pdaf type3
 */
struct statis_path_buf_info s_statis_path_info_all[] = {
	{DCAM_PATH_PDAF,    0,  0, STATIS_PDAF},
	{DCAM_PATH_VCH2,    0,  0, STATIS_EBD},
	{DCAM_PATH_AEM,     0,  0, STATIS_AEM},
	{DCAM_PATH_AFM,     0,  0, STATIS_AFM},
	{DCAM_PATH_AFL,     0,  0, STATIS_AFL},
	{DCAM_PATH_HIST,    0,  0, STATIS_HIST},
	{DCAM_PATH_FRGB_HIST,0, 0, STATIS_HIST2},
	{DCAM_PATH_3DNR,    0,  0, STATIS_3DNR},
	{DCAM_PATH_LSCM,    0,  0, STATIS_LSCM},
	{DCAM_PATH_GTM_HIST,0,  0, STATIS_GTMHIST},
};

atomic_t s_dcam_opened[DCAM_SW_CONTEXT_MAX];
static DEFINE_MUTEX(s_dcam_dev_mutex);
static struct dcam_pipe_dev *s_dcam_dev;

/*
 * set MIPI capture related register
 * range: 0x0100 ~ 0x010c
 *
 * TODO: support DCAM2, some registers only exist in DCAM0/1
 */

static int dcamcore_dcamsec_cfg(struct dcam_sw_context *pctx, void *param)
{

	pctx->dcamsec_eb = 0;

	pr_info("camca : dcamsec_mode=%d\n", pctx->dcamsec_eb);
	return 0;
}

static int dcamcore_rps_cfg(struct dcam_sw_context *pctx, void *param)
{
	uint32_t *data = (uint32_t *)param;

	pctx->rps = *data;

	pr_info("hwsim : rps[%d]\n", pctx->rps);
	return 0;
}

int dcam_core_hw_slices_set(struct dcam_sw_context *pctx,
	struct camera_frame *pframe, uint32_t slice_wmax)
{
	int ret = 0, i = 0;
	uint32_t w, offset = 0;
	uint32_t slc_w = 0, f_align;

	w = pframe->width;
	if (w <= slice_wmax) {
		slc_w = w;
		goto slices;
	}

	if (pctx->fetch.pack_bits == 2)
		f_align = 8;/* 8p * 16bits/p = 128 bits fetch aligned */
	else
		f_align = 64;/* 64p * 10bits/p = 128bits * 5 */

	slc_w = slice_wmax / f_align * f_align;

	/* can not get valid slice w aligned  */
	if ((slc_w > slice_wmax) ||
		(slc_w * DCAM_OFFLINE_SLC_MAX) < pframe->width) {
		pr_err("dcam%d failed, pic_w %d, slc_limit %d, algin %d\n",
			pctx->hw_ctx_id, pframe->width, slice_wmax, f_align);
		return -EFAULT;
	}

slices:
	while (w > 0) {
		pctx->slice_info.slice_trim[i].start_x = offset;
		pctx->slice_info.slice_trim[i].start_y = 0;
		pctx->slice_info.slice_trim[i].size_x = (w > slc_w) ? (slc_w - DCAM_OVERLAP) : w;
		pctx->slice_info.slice_trim[i].size_y = pframe->height;
		pr_info("slc%d, (%d %d %d %d), limit %d\n", i,
			pctx->slice_info.slice_trim[i].start_x, pctx->slice_info.slice_trim[i].start_y,
			pctx->slice_info.slice_trim[i].size_x, pctx->slice_info.slice_trim[i].size_y,
			slice_wmax);

		w -= pctx->slice_info.slice_trim[i].size_x;
		offset += pctx->slice_info.slice_trim[i].size_x;
		i++;
	}
	pctx->slice_num = i;
	pctx->slice_count = i;

	return ret;
}


int dcam_core_slice_trim_get(uint32_t width, uint32_t heigth, uint32_t slice_num,
	uint32_t slice_no, struct img_trim *slice_trim)
{
	int ret = 0;
	uint32_t slice_w = 0, slice_h = 0;
	uint32_t slice_x_num = 0, slice_y_num = 0;
	uint32_t start_x = 0, size_x = 0;
	uint32_t start_y = 0, size_y = 0;

	if (!width || !slice_num || !slice_trim) {
		pr_err("fail to get valid param %d, %d, %p.\n", width, slice_num, slice_trim);
		return -EFAULT;
	}

	if (heigth > DCAM_SW_SLICE_HEIGHT_MAX) {
		slice_x_num = slice_num / 2;
		slice_y_num = 2;
	} else {
		slice_x_num = slice_num;
		slice_y_num = 1;
	}
	slice_w = width / slice_x_num;
	slice_w = ALIGN(slice_w, 2);
	slice_h = heigth / slice_y_num;
	slice_h = ALIGN(slice_h, 2);

	start_x = slice_w * (slice_no % slice_x_num);
	size_x = slice_w;
	if (size_x & 0x03) {
		if (slice_no != 0) {
			start_x -=  ALIGN(size_x, 4) - size_x;
			size_x = ALIGN(size_x, 4);
		} else {
			start_x -=  ALIGN(size_x, 4);
			size_x = ALIGN(size_x, 4);
		}
	}

	start_y = (slice_no / slice_x_num) * slice_h;
	size_y = slice_h;

	slice_trim->start_x = start_x;
	slice_trim->size_x = size_x;
	slice_trim->start_y = start_y;
	slice_trim->size_y = size_y;
	pr_debug("slice %d [%d, %d, %d, %d].\n", slice_no, start_x, size_x, start_y, size_y);
	return ret;
}

static void dcamcore_src_frame_ret(void *param)
{
	struct camera_frame *frame = NULL;
	struct dcam_sw_context *pctx = NULL;

	if (!param) {
		pr_err("fail to get valid param.\n");
		return;
	}

	frame = (struct camera_frame *)param;
	pctx = (struct dcam_sw_context *)frame->priv_data;
	if (!pctx) {
		pr_err("fail to get valid src_frame pctx.\n");
		return;
	}

	pr_debug("frame %p, ch_id %d, buf_fd %d\n",
		frame, frame->channel_id, frame->buf.mfd[0]);

	cam_buf_iommu_unmap(&frame->buf);
	pctx->dcam_cb_func(DCAM_CB_RET_SRC_BUF, frame, pctx->cb_priv_data);
}

static void dcamcore_out_frame_ret(void *param)
{
	struct camera_frame *frame = NULL;
	struct dcam_sw_context *pctx = NULL;
	struct dcam_path_desc *path = NULL;

	if (!param) {
		pr_err("fail to get valid param.\n");
		return;
	}

	frame = (struct camera_frame *)param;

	if (frame->is_reserved) {
		path = (struct dcam_path_desc *)frame->priv_data;
		if (!path) {
			pr_err("fail to get valid out_frame path.\n");
			return;
		}
		cam_queue_enqueue(&path->reserved_buf_queue, &frame->list);
	} else {
		cam_buf_iommu_unmap(&frame->buf);
		pctx = (struct dcam_sw_context *)frame->priv_data;
		if (!pctx) {
			pr_err("fail to get valid out_frame pctx.\n");
			return;
		}
		pctx->dcam_cb_func(DCAM_CB_DATA_DONE, frame, pctx->cb_priv_data);
	}
}

static void dcamcore_reserved_buf_destroy(void *param)
{
	struct camera_frame *frame;

	if (!param) {
		pr_err("fail to get valid param.\n");
		return;
	}

	frame = (struct camera_frame *)param;

	if (unlikely(frame->is_reserved == 0)) {
		pr_err("fail to check frame type if reserved.\n");
		return;
	}
	/* is_reserved:
	 *  1:  basic mapping reserved buffer;
	 *  2:  copy of reserved buffer.
	 */
	if (frame->is_reserved == 1) {
		cam_buf_iommu_unmap(&frame->buf);
		cam_buf_ionbuf_put(&frame->buf);
	}
	cam_queue_empty_frame_put(frame);
}

void dcamcore_empty_interrupt_put(void *param)
{
	struct camera_interrupt *interruption = NULL;
	if (!param) {
		pr_err("fail to get valid param.\n");
		return;
	}

	interruption = (struct camera_interrupt *)param;
	cam_queue_empty_interrupt_put(interruption);
}

static struct camera_buf *dcamcore_reserved_buffer_get(struct dcam_sw_context *pctx, size_t size)
{
	int ret = 0;
	int iommu_enable = 0; /* todo: get from dev dts config value */
	struct camera_buf *ion_buf = NULL;

	ion_buf = kzalloc(sizeof(*ion_buf), GFP_KERNEL);
	if (!ion_buf) {
		pr_err("fail to alloc buffer.\n");
		goto nomem;
	}

	if (size == 0) {
		pr_err("fail to get valid res buffer size\n");
		goto ion_fail;
	}

	if (cam_buf_iommu_status_get(CAM_IOMMUDEV_DCAM) == 0)
		iommu_enable = 1;

	ret = cam_buf_alloc(ion_buf, size, iommu_enable);
	if (ret) {
		pr_err("fail to get dcam reserverd buffer\n");
		goto ion_fail;
	}

	pr_info("sw_ctx_id %d done. ion %px, size %d\n", pctx->sw_ctx_id, ion_buf, (uint32_t)size);
	return ion_buf;

ion_fail:
	kfree(ion_buf);
nomem:
	return NULL;
}

static int dcamcore_reserved_buffer_put(struct dcam_sw_context *pctx)
{
	struct camera_buf *ion_buf = NULL;

	ion_buf = (struct camera_buf *)pctx->internal_reserved_buf;
	if (!ion_buf) {
		pr_info("no reserved buffer.\n");
		return 0;
	}
	pr_info("dcam%d, ion %p\n", pctx->hw_ctx_id, ion_buf);

	if (ion_buf->type != CAM_BUF_USER)
		cam_buf_free(ion_buf);

	kfree(ion_buf);
	pctx->internal_reserved_buf = NULL;

	return 0;
}

static void dcamcore_reserved_statis_bufferq_init(struct dcam_sw_context *pctx, size_t size)
{
	int ret = 0;
	int i, j;
	enum isp_statis_buf_type stats_type;
	struct camera_frame *newfrm;
	enum dcam_path_id path_id;
	struct camera_buf *ion_buf = NULL;
	struct dcam_path_desc *path;

	pr_info("enter\n");

	if (pctx->internal_reserved_buf == NULL) {
		ion_buf = dcamcore_reserved_buffer_get(pctx, size);
		if (IS_ERR_OR_NULL(ion_buf))
			return;

		pctx->internal_reserved_buf = ion_buf;
	}
	ion_buf = (struct camera_buf *)pctx->internal_reserved_buf;

	if (ion_buf->type == CAM_BUF_USER) {
		ret = cam_buf_ionbuf_get(ion_buf);
		if (ret) {
			pr_err("fail to get buf for %d\n", ion_buf->mfd[0]);
			return;
		}
		pr_debug("reserverd statis buffer get %p\n", ion_buf);
	}

	ret = cam_buf_iommu_map(ion_buf, CAM_IOMMUDEV_DCAM);
	if (ret) {
		pr_err("fail to map dcam reserved buffer to iommu\n");
		return;
	}

	for (i = 0; i < (int)ARRAY_SIZE(s_statis_path_info_all); i++) {
		path_id = s_statis_path_info_all[i].path_id;
		stats_type = s_statis_path_info_all[i].buf_type;
		if (!stats_type)
			continue;

		path = &pctx->path[path_id];
		if (path_id == DCAM_PATH_VCH2 && path->src_sel)
			continue;

		if ((path_id == DCAM_PATH_GTM_HIST) && pctx->dev->hw->ip_isp->rgb_gtm_support)
			continue;
		j  = 0;

		while (j < DCAM_RESERVE_BUF_Q_LEN) {
			newfrm = cam_queue_empty_frame_get();
			if (newfrm) {
				newfrm->is_reserved = 1;
				memcpy(&newfrm->buf, ion_buf, sizeof(struct camera_buf));
				cam_queue_enqueue(&path->reserved_buf_queue, &newfrm->list);
				j++;
			}
			pr_debug("path%d reserved buffer %d\n", path->path_id, j);
		}
	}
	pr_info("done\n");
}

static enum dcam_path_id dcamcore_statis_type_to_path_id(enum isp_statis_buf_type type)
{
	switch (type) {
	case STATIS_AEM:
		return DCAM_PATH_AEM;
	case STATIS_AFM:
		return DCAM_PATH_AFM;
	case STATIS_AFL:
		return DCAM_PATH_AFL;
	case STATIS_HIST:
		return DCAM_PATH_HIST;
	case STATIS_HIST2:
		return DCAM_PATH_FRGB_HIST;
	case STATIS_PDAF:
		return DCAM_PATH_PDAF;
	case STATIS_EBD:
		return DCAM_PATH_VCH2;
	case STATIS_3DNR:
		return DCAM_PATH_3DNR;
	case STATIS_LSCM:
		return DCAM_PATH_LSCM;
	case STATIS_GTMHIST:
		return DCAM_PATH_GTM_HIST;
	default:
		return DCAM_PATH_MAX;
	}
}

static void dcamcore_statis_buf_destroy(void *param)
{
	struct camera_frame *frame;

	if (!param) {
		pr_err("fail to get valid param.\n");
		return;
	}

	frame = (struct camera_frame *)param;
	cam_queue_empty_frame_put(frame);
	frame = NULL;
}

static int dcamcore_statis_bufferq_init(struct dcam_sw_context *pctx)
{
	int ret = 0;
	int i, j;
	enum dcam_path_id path_id;
	enum isp_statis_buf_type stats_type;
	struct camera_buf *ion_buf;
	struct camera_frame *pframe;
	struct dcam_path_desc *path;
	size_t res_buf_size = 0;

	pr_debug("enter\n");

	for (i = 0; i < ARRAY_SIZE(s_statis_path_info_all); i++) {
		path_id = s_statis_path_info_all[i].path_id;
		stats_type = s_statis_path_info_all[i].buf_type;
		if (!stats_type)
			continue;
		if ((stats_type == STATIS_GTMHIST) && pctx->dev->hw->ip_isp->rgb_gtm_support)
			continue;

		path = &pctx->path[path_id];
		if (path_id == DCAM_PATH_VCH2 && path->src_sel)
			continue;

		cam_queue_init(&path->out_buf_queue,
			DCAM_OUT_BUF_Q_LEN, dcamcore_statis_buf_destroy);
		cam_queue_init(&path->result_queue,
			DCAM_RESULT_Q_LEN, dcamcore_statis_buf_destroy);
		cam_queue_init(&path->reserved_buf_queue,
			DCAM_RESERVE_BUF_Q_LEN,
			dcamcore_statis_buf_destroy);
	}

	for (i = 0; i < ARRAY_SIZE(s_statis_path_info_all); i++) {

		path_id = s_statis_path_info_all[i].path_id;
		stats_type = s_statis_path_info_all[i].buf_type;
		if (!stats_type)
			continue;

		path = &pctx->path[path_id];
		if (path_id == DCAM_PATH_VCH2 && path->src_sel)
			continue;
		if ((stats_type == STATIS_GTMHIST) && pctx->dev->hw->ip_isp->rgb_gtm_support)
			continue;

		for (j = 0; j < STATIS_BUF_NUM_MAX; j++) {
			ion_buf = &pctx->statis_buf_array[stats_type][j];
			if (ion_buf->mfd[0] <= 0 || stats_type == STATIS_3DNR)
				continue;

			ret = cam_buf_ionbuf_get(ion_buf);
			if (ret) {
				continue;
			}

			ret = cam_buf_iommu_map(ion_buf, CAM_IOMMUDEV_DCAM);
			if (ret) {
				cam_buf_ionbuf_put(ion_buf);
				continue;
			}

			if (stats_type != STATIS_PDAF) {
				ret = cam_buf_kmap(ion_buf);
				if (ret) {
					pr_err("fail to kmap statis buf %d\n", ion_buf->mfd[0]);
					memset(ion_buf->addr_k, 0, sizeof(ion_buf->addr_k));
				}
			}

			pframe = cam_queue_empty_frame_get();
			pframe->channel_id = 0;
			pframe->irq_property = stats_type;
			pframe->buf = *ion_buf;

			ret = cam_queue_enqueue(&path->out_buf_queue, &pframe->list);
			if (ret) {
				pr_info("dcam%d statis %d overflow\n", pctx->hw_ctx_id, stats_type);
				cam_queue_empty_frame_put(pframe);
			}

			pr_debug("dcam%d statis %d buf %d kaddr 0x%lx iova 0x%08x\n",
				pctx->hw_ctx_id, stats_type, ion_buf->mfd[0],
				ion_buf->addr_k[0], (uint32_t)ion_buf->iova[0]);

			if (ion_buf->size[0] > res_buf_size)
				res_buf_size = ion_buf->size[0];
		}
	}

	dcamcore_reserved_statis_bufferq_init(pctx, res_buf_size);

	pr_info("done.\n");
	return ret;
}

static int dcamcore_statis_bufferq_deinit(struct dcam_sw_context *pctx)
{
	int ret = 0;
	int i;
	enum dcam_path_id path_id;
	struct dcam_path_desc *path;

	pr_info("enter\n");

	for (i = 0; i < ARRAY_SIZE(s_statis_path_info_all); i++) {
		path_id = s_statis_path_info_all[i].path_id;
		path = &pctx->path[path_id];

		if (path_id == DCAM_PATH_VCH2 && path->src_sel)
			continue;
		if ((path_id == DCAM_PATH_GTM_HIST) && pctx->dev->hw->ip_isp->rgb_gtm_support)
			continue;

		pr_debug("path_id[%d] i[%d]\n", path_id, i);
		if (path_id == DCAM_PATH_MAX)
			continue;

		atomic_set(&path->user_cnt, 0);
		cam_queue_clear(&path->out_buf_queue, struct camera_frame, list);
		cam_queue_clear(&path->result_queue, struct camera_frame, list);
		cam_queue_clear(&path->reserved_buf_queue,
			struct camera_frame, list);
	}

	pr_info("done.\n");
	return ret;
}

static int dcamcore_statis_buffer_unmap(struct dcam_sw_context *pctx)
{
	int i, j;
	int32_t mfd;
	enum dcam_path_id path_id;
	enum isp_statis_buf_type stats_type;
	struct camera_buf *ion_buf = NULL;
	struct dcam_path_desc *path;

	for (i = 0; i < ARRAY_SIZE(s_statis_path_info_all); i++) {
		path_id = s_statis_path_info_all[i].path_id;
		stats_type = s_statis_path_info_all[i].buf_type;
		path = &pctx->path[path_id];
		if (!stats_type)
			continue;
		if (path_id == DCAM_PATH_VCH2 && path->src_sel)
			continue;
		if ((path_id == DCAM_PATH_GTM_HIST) && pctx->dev->hw->ip_isp->rgb_gtm_support)
			continue;

		for (j = 0; j < STATIS_BUF_NUM_MAX; j++) {
			ion_buf = &pctx->statis_buf_array[stats_type][j];
			mfd = ion_buf->mfd[0];
			if (mfd <= 0 || stats_type == STATIS_3DNR)
				continue;

			pr_debug("stats %d,  j %d,  mfd %d, offset %d\n",
				stats_type, j, mfd, ion_buf->offset[0]);
			if (ion_buf->mapping_state & CAM_BUF_MAPPING_KERNEL)
				cam_buf_kunmap(ion_buf);
			if (ion_buf->mapping_state & CAM_BUF_MAPPING_DEV)
				cam_buf_iommu_unmap(ion_buf);
			cam_buf_ionbuf_put(ion_buf);
			memset(ion_buf->iova, 0, sizeof(ion_buf->iova));
			memset(ion_buf->addr_k, 0, sizeof(ion_buf->addr_k));
		}
	}

	if (pctx->internal_reserved_buf) {
		ion_buf = pctx->internal_reserved_buf;
		pr_debug("reserved statis buffer unmap %p\n", ion_buf);

		cam_buf_iommu_unmap(ion_buf);
		if (ion_buf->type == CAM_BUF_USER)
			cam_buf_ionbuf_put(ion_buf);
	}

	pr_info("done\n");
	return 0;
}

static int dcamcore_statis_buffer_cfg(
		struct dcam_sw_context *pctx,
		struct isp_statis_buf_input *input)
{
	int ret = 0;
	int i, j;
	int32_t mfd;
	uint32_t offset;
	enum dcam_path_id path_id;
	enum isp_statis_buf_type stats_type;
	struct camera_buf *ion_buf = NULL;
	struct camera_frame *pframe = NULL;
	struct dcam_path_desc *path = NULL;

	if (input->type == STATIS_INIT) {
		memset(&pctx->statis_buf_array[0][0], 0, sizeof(pctx->statis_buf_array));
		for (i = 0; i < ARRAY_SIZE(s_statis_path_info_all); i++) {
			path_id = s_statis_path_info_all[i].path_id;
			stats_type = s_statis_path_info_all[i].buf_type;
			if (!stats_type || stats_type == STATIS_3DNR)
				continue;
			path = &pctx->path[path_id];
			if (((path_id == DCAM_PATH_VCH2) && path->src_sel) ||
				((path_id == DCAM_PATH_GTM_HIST) && pctx->dev->hw->ip_isp->rgb_gtm_support))
				continue;

			for (j = 0; j < STATIS_BUF_NUM_MAX; j++) {
				mfd = input->mfd_array[stats_type][j];

				pr_debug("i %d, type %d, mfd %d, offset %d\n", i, stats_type, mfd, input->offset_array[stats_type][j]);

				if (mfd <= 0)
					continue;

				ion_buf = &pctx->statis_buf_array[stats_type][j];
				ion_buf->mfd[0] = mfd;
				ion_buf->offset[0] = input->offset_array[stats_type][j];
				ion_buf->type = CAM_BUF_USER;
				pr_debug("stats %d, mfd %d, off %d\n", stats_type, mfd, ion_buf->offset[0]);
			}
		}
		pr_info("statis init done\n");

	} else {
		path_id = dcamcore_statis_type_to_path_id(input->type);
		if (path_id == DCAM_PATH_MAX) {
			pr_err("fail to get a valid statis type: %d\n", input->type);
			ret = -EINVAL;
			goto exit;
		}

		path = &pctx->path[path_id];
		if (path_id == DCAM_PATH_VCH2 && path->src_sel)
			goto exit;

		for (j = 0; j < STATIS_BUF_NUM_MAX; j++) {
			mfd = pctx->statis_buf_array[input->type][j].mfd[0];
			offset = pctx->statis_buf_array[input->type][j].offset[0];
			if ((mfd > 0) && (mfd == input->mfd) && (offset == input->offset)) {
				ion_buf = &pctx->statis_buf_array[input->type][j];
				break;
			}
		}

		if (ion_buf == NULL) {
			pr_err("fail to get statis buf %d, type %d\n", input->type, input->mfd);
			ret = -EINVAL;
			goto exit;
		}

		pframe = cam_queue_empty_frame_get();
		pframe->irq_property = input->type;
		pframe->buf = *ion_buf;
		path = &pctx->path[path_id];
		ret = cam_queue_enqueue(&path->out_buf_queue, &pframe->list);
		pr_debug("dcam sw %d, statis %d, mfd %d, off %d, iova 0x%08x,  kaddr 0x%lx\n",
			pctx->sw_ctx_id, input->type, mfd, offset, (uint32_t)pframe->buf.iova[0], pframe->buf.addr_k[0]);

		if (ret) {
			pr_err("fail to statis_type %d, enqueue out_buf_queue, cnt %d\n", input->type, cam_queue_cnt_get(&path->out_buf_queue));
			cam_queue_empty_frame_put(pframe);
		}
	}
exit:
	return ret;
}

static int dcamcore_statis_buffer_skip_cfg(struct dcam_sw_context *pctx, struct camera_frame *pframe)
{
	int ret = 0;
	enum dcam_path_id path_id;

	path_id = dcamcore_statis_type_to_path_id(pframe->irq_property);
	if (path_id == DCAM_PATH_MAX) {
		pr_err("invalid statis type: %d\n", pframe->irq_property);
		ret = -EINVAL;
		goto exit;
	}

	ret = cam_queue_enqueue(&pctx->path[path_id].out_buf_queue, &pframe->list);
exit:
	return ret;
}

static int dcamcore_pmctx_init(
		void *dev,
		struct dcam_pipe_context *pctx)
{
	int ret = 0;
	int iommu_enable = 0;
	struct dcam_dev_param *blk_pm_ctx = &pctx->blk_pm;
	struct dcam_sw_context *sw_pctx = (struct dcam_sw_context *)dev;

	memset(blk_pm_ctx, 0, sizeof(struct dcam_dev_param));
	if (cam_buf_iommu_status_get(CAM_IOMMUDEV_DCAM) == 0)
		iommu_enable = 1;

	ret = cam_buf_alloc(&blk_pm_ctx->lsc.buf, DCAM_LSC_BUF_SIZE, iommu_enable);
	if (ret)
		goto alloc_fail;

	ret = cam_buf_kmap(&blk_pm_ctx->lsc.buf);
	if (ret)
		goto map_fail;

	blk_pm_ctx->offline = (pctx->ctx_id == 0) ? 0 : 1;
	blk_pm_ctx->idx = sw_pctx->hw_ctx_id;
	blk_pm_ctx->dev = (void *)sw_pctx;

	atomic_set(&pctx->user_cnt, 1);
	mutex_init(&blk_pm_ctx->lsc.lsc_lock);
	mutex_init(&blk_pm_ctx->param_lock);

	init_dcam_pm(blk_pm_ctx);
	return 0;

map_fail:
	cam_buf_free(&blk_pm_ctx->lsc.buf);
alloc_fail:
	pr_err("failed %d\n", ret);
	return ret;
}

static int dcamcore_pmctx_deinit(struct dcam_pipe_context *pctx)
{
	struct dcam_dev_param *blk_pm_ctx = &pctx->blk_pm;
	struct dcam_sw_context *sw_pctx = NULL;

	sw_pctx = (struct dcam_sw_context *)blk_pm_ctx->dev;

	mutex_destroy(&blk_pm_ctx->param_lock);
	mutex_destroy(&blk_pm_ctx->lsc.lsc_lock);

	if (blk_pm_ctx->lsc.buf.mapping_state & CAM_BUF_MAPPING_DEV)
		cam_buf_iommu_unmap(&blk_pm_ctx->lsc.buf);
	cam_buf_kunmap(&blk_pm_ctx->lsc.buf);
	cam_buf_free(&blk_pm_ctx->lsc.buf);
	if(blk_pm_ctx->lsc.weight_tab) {
		kfree(blk_pm_ctx->lsc.weight_tab);
		blk_pm_ctx->lsc.weight_tab = NULL;
	}
	if(blk_pm_ctx->lsc.weight_tab_x) {
		kfree(blk_pm_ctx->lsc.weight_tab_x);
		blk_pm_ctx->lsc.weight_tab_x = NULL;
	}
	if(blk_pm_ctx->lsc.weight_tab_y) {
		kfree(blk_pm_ctx->lsc.weight_tab_y);
		blk_pm_ctx->lsc.weight_tab_y = NULL;
	}
	atomic_set(&pctx->user_cnt, 0);

	pr_info("sw_ctx_id:%d ctx[%d] done\n", sw_pctx->sw_ctx_id, pctx->ctx_id);
	return 0;
}

void dcam_core_offline_debug_dump(
	struct dcam_sw_context *pctx,
	struct dcam_dev_param *pm,
	struct camera_frame *proc_frame)
{
	int size;
	struct camera_frame *frame = NULL;
	struct debug_base_info *base_info;
	void *pm_data;

	pctx->dcam_cb_func(DCAM_CB_GET_PMBUF,
		(void *)&frame, pctx->cb_priv_data);
	if (frame == NULL) {
		pr_debug("no pmbuf for dcam%d\n", pctx->hw_ctx_id);
		return;
	}

	base_info = (struct debug_base_info *)frame->buf.addr_k[0];
	if (base_info == NULL) {
		cam_queue_empty_frame_put(frame);
		return;
	}
	base_info->cam_id = -1;
	base_info->dcam_cid = pctx->hw_ctx_id | (proc_frame->irq_property << 8);
	base_info->isp_cid = -1;
	base_info->scene_id = PM_SCENE_CAP;
	if (proc_frame->irq_property == CAM_FRAME_FDRL)
		base_info->scene_id = PM_SCENE_FDRL;
	if (proc_frame->irq_property == CAM_FRAME_FDRH)
		base_info->scene_id = PM_SCENE_FDRH;

	base_info->frame_id = proc_frame->fid;
	base_info->sec = proc_frame->sensor_time.tv_sec;
	base_info->usec = proc_frame->sensor_time.tv_usec;
	frame->fid = proc_frame->fid;
	frame->sensor_time = proc_frame->sensor_time;

	pm_data = (void *)(base_info + 1);
	size = dcam_k_dump_pm(pm_data, (void *)pm);
	if (size >= 0)
		base_info->size = (int32_t)size;
	else
		base_info->size = 0;

	pr_debug("dcam_cxt %d, scene %d  fid %d  dsize %d\n",
		base_info->dcam_cid, base_info->scene_id,
		base_info->frame_id, base_info->size);

	pctx->dcam_cb_func(DCAM_CB_STATIS_DONE,
		frame, pctx->cb_priv_data);
}
int dcam_core_offline_slices_sw_start(void *param)
{
	int ret = 0;
	int i, loop;
	uint32_t force_ids = DCAM_CTRL_ALL;
	uint32_t dev_fid;
	struct dcam_sw_context *sw_pctx = NULL;
	struct camera_frame *pframe = NULL;
	struct dcam_path_desc *path;
	struct dcam_fetch_info *fetch;
	struct dcam_hw_force_copy copyarg;
	struct cam_hw_info *hw = NULL;
	struct dcam_hw_path_start patharg;
	struct dcam_hw_slice_fetch slicearg;
	struct dcam_hw_fbc_ctrl fbc_arg;
	uint32_t slice_no;

	sw_pctx = (struct dcam_sw_context *)param;
	sw_pctx->offline = 1;
	fetch = &sw_pctx->fetch;
	hw = sw_pctx->dev->hw;

	if (sw_pctx->slice_count) {
		pframe = cam_queue_dequeue(&sw_pctx->proc_queue,
				struct camera_frame, list);
		if (!pframe) {
			pr_err("fail to map buf to dcam%d iommu.\n", sw_pctx->hw_ctx_id);
			goto map_err;
		}
	} else {
		pframe = cam_queue_dequeue(&sw_pctx->in_queue, struct camera_frame, list);
		if (pframe == NULL) {
			pr_warn("warning: no frame from in_q. dcam%d\n", sw_pctx->hw_ctx_id);
			return 0;
		}

		pr_debug("frame %p, dcam%d  ch_id %d.  buf_fd %d\n", pframe,
			sw_pctx->hw_ctx_id, pframe->channel_id, pframe->buf.mfd[0]);
		pr_debug("size %d %d,  endian %d, pattern %d\n",
			pframe->width, pframe->height, pframe->endian, pframe->pattern);

		ret = cam_buf_iommu_map(&pframe->buf, CAM_IOMMUDEV_DCAM);
		if (ret) {
			pr_err("fail to map buf to dcam%d iommu.\n", sw_pctx->hw_ctx_id);
			goto map_err;
		}

		sw_pctx->slice_count = sw_pctx->slice_num;
	}
	slice_no = sw_pctx->slice_num - sw_pctx->slice_count;

	ret = wait_for_completion_interruptible_timeout(
		&sw_pctx->slice_done, DCAM_OFFLINE_TIMEOUT);
	if (ret <= 0) {
		pr_err("error: dcam%d offline timeout. ret: %d\n", sw_pctx->hw_ctx_id, ret);
		ret = -EFAULT;
		goto wait_err;
	}

	loop = 0;
	do {
		ret = cam_queue_enqueue(&sw_pctx->proc_queue, &pframe->list);
		if (ret == 0)
			break;
		pr_info("wait for proc queue. loop %d\n", loop);

		/* wait for previous frame proccessed done. */
		mdelay(1);
	} while (loop++ < 500);
	if (ret) {
		pr_err("error: input frame queue tmeout.\n");
		ret = 0;
		goto inq_overflow;
	}

	/* prepare frame info for tx done
	 * ASSERT: this dev has no cap_sof
	 */
	sw_pctx->index_to_set = pframe->fid - sw_pctx->base_fid;
	dev_fid = sw_pctx->index_to_set;
	if (pframe->sensor_time.tv_sec || pframe->sensor_time.tv_usec) {
		sw_pctx->frame_ts[tsid(dev_fid)].tv_sec =
			pframe->sensor_time.tv_sec;
		sw_pctx->frame_ts[tsid(dev_fid)].tv_nsec =
			pframe->sensor_time.tv_usec * NSEC_PER_USEC;
		sw_pctx->frame_ts_boot[tsid(dev_fid)] = pframe->boot_sensor_time;
		pr_info("frame[%d]\n", pframe->fid);
	}
	/* cfg path output and path */
	for (i  = 0; i < DCAM_PATH_MAX; i++) {
		path = &sw_pctx->path[i];
		if (atomic_read(&path->user_cnt) < 1 || atomic_read(&path->is_shutoff) > 0)
			continue;
		ret = dcam_path_store_frm_set(sw_pctx, path); /* TODO: */
		if (ret == 0) {
			/* interrupt need > 1 */
			atomic_set(&path->set_frm_cnt, 1);
			atomic_inc(&path->set_frm_cnt);
			patharg.path_id = i;
			patharg.idx = sw_pctx->hw_ctx_id;
			patharg.slowmotion_count = sw_pctx->slowmotion_count;
			patharg.pdaf_path_eb = 0;
			patharg.cap_info = sw_pctx->cap_info;
			patharg.pack_bits = sw_pctx->path[i].pack_bits;
			patharg.src_sel = sw_pctx->path[i].src_sel;
			patharg.bayer_pattern = sw_pctx->path[i].bayer_pattern;
			patharg.in_trim = sw_pctx->path[i].in_trim;
			patharg.endian = sw_pctx->path[i].endian;
			hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_START, &patharg);

			if (sw_pctx->path[i].fbc_mode) {
				fbc_arg.idx = sw_pctx->hw_ctx_id;
				fbc_arg.path_id = i;
				fbc_arg.fmt = sw_pctx->path[i].out_fmt;
				fbc_arg.data_bits = sw_pctx->path[i].data_bits;
				fbc_arg.fbc_mode = sw_pctx->path[i].fbc_mode;
				hw->dcam_ioctl(hw, DCAM_HW_CFG_FBC_CTRL, &fbc_arg);
			}
		} else {
			pr_err("fail to set dcam%d path%d store frm\n",
				sw_pctx->hw_ctx_id, path->path_id);
			ret = 0;
			goto dequeue;
		}
	}

	dcam_core_slice_trim_get(pframe->width, pframe->height, sw_pctx->slice_num,
		slice_no, &sw_pctx->slice_trim);
	sw_pctx->cur_slice = &sw_pctx->slice_trim;

	/* cfg fetch */
	fetch->pack_bits = 0;
	fetch->endian = pframe->endian;
	fetch->pattern = pframe->pattern;
	fetch->size.w = pframe->width;
	fetch->size.h = pframe->height;
	fetch->trim.start_x = 0;
	fetch->trim.start_y = 0;
	fetch->trim.size_x = pframe->width;
	fetch->trim.size_y = pframe->height;
	fetch->addr.addr_ch0 = (uint32_t)pframe->buf.iova[0];
	slicearg.idx = sw_pctx->hw_ctx_id;
	slicearg.fetch = &sw_pctx->fetch;
	slicearg.cur_slice = sw_pctx->cur_slice;
	slicearg.slice_trim = sw_pctx->slice_trim;
	slicearg.dcam_slice_mode = sw_pctx->dcam_slice_mode;
	slicearg.slice_count = sw_pctx->slice_count;
	slicearg.slice_num = sw_pctx->slice_num;

	hw->dcam_ioctl(hw, DCAM_HW_CFG_SLICE_FETCH_SET, &slicearg);

	dcam_init_lsc_slice(&sw_pctx->ctx[sw_pctx->cur_ctx_id].blk_pm, 0);
	copyarg.id = force_ids;
	copyarg.idx = sw_pctx->hw_ctx_id;
	copyarg.glb_reg_lock = sw_pctx->glb_reg_lock;
	hw->dcam_ioctl(hw, DCAM_HW_CFG_FORCE_COPY, &copyarg);
	udelay(500);
	sw_pctx->iommu_status = (uint32_t)(-1);
	atomic_set(&sw_pctx->state, STATE_RUNNING);
	sw_pctx->err_count = 1;
	hw->dcam_ioctl(hw, DCAM_HW_CFG_FETCH_START, hw);
	pr_debug("done slice %d.\n", slice_no);

	return 0;

dequeue:
	pframe = cam_queue_dequeue_tail(&sw_pctx->proc_queue);
inq_overflow:
wait_err:
	cam_buf_iommu_unmap(&pframe->buf);
	complete(&sw_pctx->slice_done);
	complete(&sw_pctx->frm_done);
map_err:
	sw_pctx->slice_num = 0;
	sw_pctx->slice_count = 0;
	sw_pctx->dcam_cb_func(DCAM_CB_RET_SRC_BUF, pframe, sw_pctx->cb_priv_data);
	return ret;
}

static int dcamcore_gtm_ltm_bypass_cfg(struct dcam_sw_context *sw_pctx, struct isp_io_param *io_param)
{
	int ret = 0;
	struct dcam_path_desc *path = NULL;
	struct isp_dev_ltm_bypass ltm_bypass_info;
	struct dcam_dev_raw_gtm_bypass gtm_bypass_info;
	struct camera_frame *frame = NULL;
	uint32_t for_capture = 0, for_fdr = 0;
	if ((io_param->scene_id == PM_SCENE_FDRL) ||
		(io_param->scene_id == PM_SCENE_FDRH) ||
		(io_param->scene_id == PM_SCENE_FDR_PRE) ||
		(io_param->scene_id == PM_SCENE_FDR_DRC))
		for_fdr = 1;
	for_capture = (io_param->scene_id == PM_SCENE_CAP ? 1 : 0) | for_fdr;
	if (io_param->sub_block == ISP_BLOCK_RGB_LTM)
		ret = copy_from_user((void *)(&ltm_bypass_info), io_param->property_param, sizeof(struct isp_dev_ltm_bypass));
	else
		ret = copy_from_user((void *)(&gtm_bypass_info), io_param->property_param, sizeof(struct dcam_dev_raw_gtm_bypass));
	if (ret) {
		pr_err("fail to copy, ret=0x%x\n", (unsigned int)ret);
		return -EPERM;
	}
	if (for_capture)
		path = &sw_pctx->path[DCAM_PATH_FULL];
	else
		path = &sw_pctx->path[DCAM_PATH_BIN];
	frame = cam_queue_tail_peek(&path->result_queue, struct camera_frame, list);
	if (frame) {
		if (io_param->sub_block == ISP_BLOCK_RGB_LTM) {
			frame->need_ltm_hist = !ltm_bypass_info.ltm_hist_stat_bypass;
			frame->need_ltm_map = !ltm_bypass_info.ltm_map_bypass;
			pr_debug("dcam ctx%d,path%d fid %d , ltm hist bypass %d, map bypass %d\n",
				sw_pctx->sw_ctx_id, path->path_id, frame->fid, ltm_bypass_info.ltm_hist_stat_bypass, ltm_bypass_info.ltm_map_bypass);
		} else {
			frame->need_gtm_hist = !gtm_bypass_info.gtm_hist_stat_bypass;
			frame->need_gtm_map = !gtm_bypass_info.gtm_map_bypass;
			frame->gtm_mod_en = gtm_bypass_info.gtm_mod_en;
			pr_debug("dcam ctx%d,path%d fid %d , gtm enalbe %d hist bypass %d, map bypass %d\n",
				sw_pctx->sw_ctx_id, path->path_id, frame->fid, gtm_bypass_info.gtm_mod_en, gtm_bypass_info.gtm_hist_stat_bypass, gtm_bypass_info.gtm_map_bypass);
		}
	} else
		pr_warn("dcam ctx%d,path%d dont have buffer\n", sw_pctx->cur_ctx_id, path->path_id);
	return ret;
}

static int dcamcore_param_cfg(void *dcam_handle, void *param)
{
	int ret = 0;
	uint32_t i = 0;
	func_dcam_cfg_param cfg_fun_ptr = NULL;
	struct dcam_sw_context *sw_pctx = NULL;
	struct isp_io_param *io_param;
	struct dcam_dev_param *pm = NULL;
	struct cam_hw_info *ops = NULL;
	struct dcam_hw_block_func_get get;
	struct dcam_pipe_context *pctx;

	if (!dcam_handle || !param) {
		pr_err("fail to get valid param, dev %p, param %p\n",
			dcam_handle, param);
		return -EFAULT;
	}

	sw_pctx= (struct dcam_sw_context *)dcam_handle;
	ops = sw_pctx->dev->hw;
	io_param = (struct isp_io_param *)param;

	pctx = &sw_pctx->ctx[DCAM_CXT_0];
	if (io_param->scene_id == PM_SCENE_FDRL || io_param->scene_id == PM_SCENE_FDR_PRE ||
		io_param->scene_id == PM_SCENE_OFFLINE_BPC)
		pctx = &sw_pctx->ctx[DCAM_CXT_1];
	if (io_param->scene_id == PM_SCENE_FDRH || io_param->scene_id == PM_SCENE_FDR_DRC ||
		io_param->scene_id == PM_SCENE_OFFLINE_CAP || (io_param->scene_id == PM_SCENE_SFNR))
		pctx = &sw_pctx->ctx[DCAM_CXT_2];
	pm = &pctx->blk_pm;

	if (io_param->scene_id == PM_SCENE_OFFLINE_CAP) {
		if (sw_pctx->pm) {
			pm = sw_pctx->pm;
			pm->offline = sw_pctx->offline;
			pr_info("pm ptr:%p.\n", pm);
		} else
			pr_warn("warning:Not get pm buffer ptr.\n");
	}

	if (((io_param->sub_block == ISP_BLOCK_RGB_LTM) && (io_param->property == ISP_PRO_RGB_LTM_BYPASS)) ||
		((io_param->sub_block == ISP_BLOCK_RGB_GTM) && ((io_param->property == DCAM_PRO_GTM_BYPASS)))) {
		ret = dcamcore_gtm_ltm_bypass_cfg(sw_pctx, io_param);
		return ret;
	}

	if (atomic_read(&pctx->user_cnt) == 0) {
		pr_info("init hw_ctx_id:%d sw_ctx_id:%d ctx[%d]\n", sw_pctx->hw_ctx_id, sw_pctx->sw_ctx_id, pctx->ctx_id);
		ret = dcamcore_pmctx_init(sw_pctx, pctx);
		if (ret)
			goto exit;
	}

	i = io_param->sub_block - DCAM_BLOCK_BASE;
	get.index = i;
	ops->dcam_ioctl(ops, DCAM_HW_CFG_BLOCK_FUNC_GET, &get);
	if (get.dcam_entry != NULL &&
		get.dcam_entry->sub_block == io_param->sub_block) {
		cfg_fun_ptr = get.dcam_entry->cfg_func;
	} else { /* if not, some error */
		pr_err("fail to check param, io_param->sub_block = %d, error\n", io_param->sub_block);
	}
	if (cfg_fun_ptr == NULL) {
		pr_debug("block %d not supported.\n", io_param->sub_block);
		goto exit;
	}

	if (sw_pctx->dcam_slice_mode && sw_pctx->slice_count > 0
		&& (io_param->sub_block != DCAM_BLOCK_LSC))
		return 0;

	if (io_param->sub_block == DCAM_BLOCK_LSC)
		mutex_lock(&pm->lsc.lsc_lock);

	pm->dcam_slice_mode = sw_pctx->dcam_slice_mode;
	ret = cfg_fun_ptr(io_param, pm);

	if ((io_param->sub_block == DCAM_BLOCK_LSC) &&
		(sw_pctx->offline == 0) &&
		(atomic_read(&sw_pctx->state) == STATE_RUNNING)) {
		dcam_update_lsc(pm);
	}

	if (io_param->sub_block == DCAM_BLOCK_LSC)
		mutex_unlock(&pm->lsc.lsc_lock);
exit:
	return ret;
}

static int dcamcore_param_reconfig(
	struct dcam_sw_context *sw_pctx, uint32_t cxt_id, void *param)
{
	int ret = 0;
	struct cam_hw_info *hw = NULL;
	struct dcam_pipe_context *pctx = NULL;
	struct dcam_dev_param *pm;

	pr_info("dcam%d,  cxt_id %d\n", sw_pctx->hw_ctx_id, cxt_id);
	if (cxt_id > DCAM_CXT_3) {
		pr_err("invalid context id\n");
		return -1;
	}

	sw_pctx->cur_ctx_id = cxt_id;
	pctx = &sw_pctx->ctx[sw_pctx->cur_ctx_id];
	pm = &pctx->blk_pm;
	hw = sw_pctx->dev->hw;
	pm->in_size = sw_pctx->cap_info.cap_size;
	if (param)
		pm->non_zsl_cap = *((uint32_t *)param);

	if (sw_pctx->slowmotion_count)
		pm->is_high_fps = 1;
	else
		pm->is_high_fps = 0;
	if (atomic_read(&pctx->user_cnt) <= 0) {
		pr_err("context%d is not in use\n", cxt_id);
		return -1;
	}

	hw->dcam_ioctl(hw, DCAM_HW_CFG_MODULE_BYPASS, &sw_pctx->hw_ctx_id);
	hw->dcam_ioctl(hw, DCAM_HW_CFG_BLOCKS_SETSTATIS, pm);
	hw->dcam_ioctl(hw, DCAM_HW_CFG_BLOCKS_SETALL, pm);

	pr_info("dcam%d done\n", sw_pctx->hw_ctx_id);
	return ret;
}

static inline void dcamcore_frame_info_show(struct dcam_sw_context *pctx,
			struct dcam_path_desc *path,
			struct camera_frame *frame)
{
	uint32_t size = 0, pack_bits = 0;
	struct dcam_compress_cal_para cal_fbc = {0};

	pack_bits = path->pack_bits;
	if (frame->is_compressed) {
		cal_fbc.compress_4bit_bypass = frame->compress_4bit_bypass;
		cal_fbc.data_bits = path->data_bits;
		cal_fbc.fbc_info = &frame->fbc_info;
		cal_fbc.fmt = path->out_fmt;
		cal_fbc.height = frame->height;
		cal_fbc.width = frame->width;
		size = dcam_if_cal_compressed_size (&cal_fbc);
	} else
		size = cal_sprd_raw_pitch(frame->width, pack_bits) * frame->height;

	pr_debug("DCAM%u %s frame %u %u size %u %u buf %08lx %08x\n",
		pctx->hw_ctx_id, dcam_path_name_get(path->path_id),
		frame->is_reserved, frame->is_compressed,
		frame->width, frame->height,
		frame->buf.iova[0], size);
}

static int dcamcore_path_get(void *dcam_handle, int path_id)
{
	struct dcam_sw_context *pctx;
	struct dcam_path_desc *path = NULL;

	if (!dcam_handle) {
		pr_err("fail to get valid param, dcam_handle=%p\n", dcam_handle);
		return -EFAULT;
	}
	if (path_id < DCAM_PATH_FULL || path_id >= DCAM_PATH_MAX) {
		pr_err("fail to get a valid path_id, path id %d\n", path_id);
		return -EFAULT;
	}

	pctx = (struct dcam_sw_context *)dcam_handle;
	path = &pctx->path[path_id];
	if (atomic_inc_return(&path->user_cnt) > 1) {
		atomic_dec(&path->user_cnt);
		pr_err("fail to get a valid param, dcam%d path %d in use.\n", pctx->sw_ctx_id, path_id);
		return -EFAULT;
	}

	cam_queue_init(&path->result_queue, DCAM_RESULT_Q_LEN,
		dcamcore_out_frame_ret);
	cam_queue_init(&path->out_buf_queue, DCAM_OUT_BUF_Q_LEN,
		dcamcore_out_frame_ret);
	cam_queue_init(&path->alter_out_queue, DCAM_OUT_BUF_Q_LEN,
		dcamcore_out_frame_ret);
	cam_queue_init(&path->reserved_buf_queue, DCAM_RESERVE_BUF_Q_LEN,
		dcamcore_reserved_buf_destroy);
	cam_queue_init(&path->middle_queue, DCAM_MIDDLE_Q_LEN,
		dcamcore_out_frame_ret);

	return 0;
}

static int dcamcore_path_put(void *dcam_handle, int path_id)
{
	int ret = 0;
	struct dcam_sw_context *pctx;
	struct dcam_path_desc *path = NULL;
	unsigned long flag = 0;

	if (!dcam_handle) {
		pr_err("fail to get a valid param,  dcam_handle=%p\n",
			dcam_handle);
		return -EFAULT;
	}
	if (path_id < DCAM_PATH_FULL || path_id >= DCAM_PATH_MAX) {
		pr_err("fail to get a valid param, path id %d\n", path_id);
		return -EFAULT;
	}

	pctx = (struct dcam_sw_context *)dcam_handle;
	path = &pctx->path[path_id];

	if (atomic_read(&path->user_cnt) == 0) {
		pr_debug("fail to get a valid user_cnt, dcam%d path %d is not in use.\n",
			pctx->hw_ctx_id, path_id);
		return -EFAULT;
	}

	if (atomic_dec_return(&path->user_cnt) != 0) {
		pr_warn("warning: dcam%d path %d has multi users.\n",
			pctx->hw_ctx_id, path_id);
		atomic_set(&path->user_cnt, 0);
	}
	spin_lock_irqsave(&path->size_lock, flag);
	path->size_update = 0;
	spin_unlock_irqrestore(&path->size_lock, flag);
	cam_queue_clear(&path->result_queue, struct camera_frame, list);
	cam_queue_clear(&path->out_buf_queue, struct camera_frame, list);
	cam_queue_clear(&path->alter_out_queue, struct camera_frame, list);
	cam_queue_clear(&path->reserved_buf_queue, struct camera_frame, list);
	cam_queue_clear(&path->middle_queue, struct camera_frame, list);

	pr_info("put dcam%d path %d done\n", pctx->hw_ctx_id, path_id);
	return ret;
}

static int dcamcore_path_cfg(void *dcam_handle, enum dcam_path_cfg_cmd cfg_cmd,
	int path_id, void *param)
{
	int ret = 0;
	int i = 0;
	struct dcam_sw_context *pctx = NULL;
	struct dcam_path_desc *path = NULL;
	struct dcam_path_desc *path_raw = NULL;
	struct cam_hw_info *hw = NULL;
	struct dcam_hw_path_src_sel patharg;
	struct camera_frame *pframe = NULL, *newfrm = NULL, *rawfrm = NULL;
	uint32_t lowlux_4in1 = 0;
	uint32_t shutoff = 0;
	unsigned long flag = 0;
	enum dcam_path_state state = DCAM_PATH_IDLE;

	static const char *tb_src[] = {"(4c)raw", "bin-sum"};/* for log */

	if (!dcam_handle || !param) {
		pr_err("fail to get a valid param, input param: %p, %p\n",
			dcam_handle, param);
		return -EFAULT;
	}
	if (path_id < DCAM_PATH_FULL || path_id >= DCAM_PATH_MAX) {
		pr_err("fail to get a valid param, dcam path id %d\n", path_id);
		return -EFAULT;
	}

	pctx = (struct dcam_sw_context *)dcam_handle;
	hw = pctx->dev->hw;
	path = &pctx->path[path_id];
	path_raw = &pctx->path[DCAM_PATH_RAW];

	if (atomic_read(&path->user_cnt) == 0) {
		pr_err("fail to get a valid user_cnt, dcam%d, path %d is not in use.%d\n",
			pctx->hw_ctx_id, path_id, cfg_cmd);
		return -EFAULT;
	}

	switch (cfg_cmd) {
	case DCAM_PATH_CFG_OUTPUT_BUF:
		pframe = (struct camera_frame *)param;
		if (path_id != DCAM_PATH_FULL && !(path_id == DCAM_PATH_VCH2 && pframe->channel_id == CAM_CH_DCAM_VCH)) {
			ret = cam_buf_iommu_map(&pframe->buf, CAM_IOMMUDEV_DCAM);
			if (ret)
				goto exit;
		}

		if (atomic_read(&pctx->state) != STATE_RUNNING)
			dcamcore_frame_info_show(pctx, path, pframe);

		pframe->is_reserved = 0;
		pframe->not_use_isp_reserved_buf = 0;
		pframe->priv_data = pctx;
		ret = cam_queue_enqueue(&path->out_buf_queue, &pframe->list);
		if (ret) {
			pr_warn("warning:enqueue frame of dcam path %d fail\n",
				path_id);
			cam_buf_iommu_unmap(&pframe->buf);
			goto exit;
		}
		pr_debug("config dcam%d path %d output buffer:%d.\n", pctx->hw_ctx_id,  path_id, pframe->buf.mfd[0]);
		break;
	case DCAM_PATH_CLR_OUTPUT_SHARE_BUF:
		state = *(uint32_t *)param;
		do {
			pframe = cam_queue_dequeue(&path->out_buf_queue,
				struct camera_frame, list);
			if (pframe == NULL)
				break;
			pctx->buf_get_cb(SHARE_BUF_SET_CB, pframe, pctx->buf_cb_data);

		} while (1);
		do {
			if (state == DCAM_PATH_PAUSE)
				break;
			else {
				pframe = cam_queue_dequeue(&path->result_queue,
					struct camera_frame, list);
				if (pframe == NULL)
					break;
				pctx->buf_get_cb(SHARE_BUF_SET_CB, pframe, pctx->buf_cb_data);
			}

		} while (1);
		break;
	case DCAM_PATH_CFG_OUTPUT_ALTER_BUF:
		pframe = (struct camera_frame *)param;
		pframe->is_reserved = 0;
		pframe->priv_data = pctx;
		ret = cam_queue_enqueue(&path->alter_out_queue, &pframe->list);
		if (ret) {
			pr_err("fail to enqueue frame of dcam path %d\n",
				path_id);
			goto exit;
		}
		pr_debug("config dcam alter output buffer %d\n", pframe->buf.mfd[0]);
		break;

	case DCAM_PATH_CLR_OUTPUT_ALTER_BUF:
		do {
			pframe = cam_queue_dequeue(&path->alter_out_queue,
				struct camera_frame, list);
			if (pframe == NULL)
				break;
			pr_debug("clr fdr raw buf fd %d, type %d, mapping %x\n",
				pframe->buf.mfd[0], pframe->buf.type, pframe->buf.mapping_state);
			cam_buf_ionbuf_put(&pframe->buf);
			cam_queue_empty_frame_put(pframe);

		} while (1);
		break;
	case DCAM_PATH_CFG_OUTPUT_RESERVED_BUF:
		pframe = (struct camera_frame *)param;
		ret = cam_buf_iommu_single_page_map(&pframe->buf,
			CAM_IOMMUDEV_DCAM);
		if (ret) {
			pr_err("fail to map dcam path %d reserve buffer.\n", path_id);
			goto exit;
		}

		pframe->is_reserved = 1;
		pframe->priv_data = path;
		if (path->fbc_mode)
			pframe->is_compressed = 1;
		ret = cam_queue_enqueue(&path->reserved_buf_queue, &pframe->list);
		if (ret) {
			pr_err("fail to enqueue frame of dcam path %d reserve buffer.\n",
				path_id);
			cam_buf_iommu_unmap(&pframe->buf);
			goto exit;
		}
		pr_info("config dcam path%d output reserverd buffer.\n", path_id);

		i = 1;
		while (i < DCAM_RESERVE_BUF_Q_LEN) {
			newfrm = cam_queue_empty_frame_get();
			if (newfrm) {
				newfrm->is_reserved = 2;
				newfrm->priv_data = path;
				newfrm->pyr_status = pframe->pyr_status;
				memcpy(&newfrm->buf, &pframe->buf, sizeof(pframe->buf));
				if (path->fbc_mode)
					newfrm->is_compressed = 1;
				ret = cam_queue_enqueue(&path->reserved_buf_queue, &newfrm->list);
				if (path_id == DCAM_PATH_FULL && pctx->raw_alg_type == RAW_ALG_AI_SFNR) {
					rawfrm = cam_queue_empty_frame_get();
					if (rawfrm) {
						rawfrm->is_reserved = 2;
						rawfrm->priv_data = path_raw;
						rawfrm->pyr_status = pframe->pyr_status;
						memcpy(&rawfrm->buf, &pframe->buf, sizeof(pframe->buf));
						ret = cam_queue_enqueue(&path_raw->reserved_buf_queue, &rawfrm->list);
					}
				}
				i++;
			}
		}
		break;
	case DCAM_PATH_CFG_SIZE:
		ret = dcam_path_size_cfg(pctx, path, param);
		break;
	case DCAM_PATH_CFG_BASE:
		ret = dcam_path_base_cfg(pctx, path, param);
		break;
	case DCAM_PATH_CFG_FULL_SOURCE:
		lowlux_4in1 = *(uint32_t *)param;

		if (lowlux_4in1) {
			pctx->lowlux_4in1 = 1;
			patharg.src_sel = PROCESS_RAW_SRC_SEL;
			patharg.idx = pctx->hw_ctx_id;
			ret = hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_SRC_SEL, &patharg);
			pctx->skip_4in1 = 1; /* auto copy, so need skip 1 frame */
		} else {
			pctx->lowlux_4in1 = 0;
			patharg.src_sel = PROCESS_RAW_SRC_SEL;
			patharg.idx = pctx->hw_ctx_id;
			ret = hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_SRC_SEL, &patharg);
			pctx->skip_4in1 = 1;
		}
		pr_info("dev%d lowlux %d, skip_4in1 %d, full src: %s\n", pctx->hw_ctx_id,
			pctx->lowlux_4in1, pctx->skip_4in1, tb_src[lowlux_4in1]);
		break;
	case DCAM_PATH_CFG_SHUTOFF:
		shutoff = *(uint32_t *)param;
		atomic_set(&path->is_shutoff, shutoff);
		pr_debug("set path %d shutoff %d\n", path_id, path->is_shutoff);
		break;
	case DCAM_PATH_CFG_STATE:
		state = *(uint32_t *)param;
		spin_lock_irqsave(&path->state_lock, flag);
		path->state = state;
		path->state_update = 1;
		spin_unlock_irqrestore(&path->state_lock, flag);
		break;
	default:
		pr_warn("warning: unsupported command: %d\n", cfg_cmd);
		break;
	}
exit:
	return ret;
}

/* get path rect from register
 */
static int dcamcore_path_rect_get(struct dcam_sw_context *pctx, void *param)
{
	struct sprd_img_path_rect *p = (struct sprd_img_path_rect *)param;
	struct dcam_path_desc *path;
	struct dcam_dev_aem_win *aem_win;
	struct isp_img_rect *afm_crop;
	struct dcam_dev_param *pm;

	if ((!pctx) || (!param)) {
		pr_err("fail to get valid param, dev=%p, param=%p\n", pctx, param);
		return -EINVAL;
	}
	path = &pctx->path[DCAM_PATH_BIN];
	p->trim_valid_rect.x = path->in_trim.start_x;
	p->trim_valid_rect.y = path->in_trim.start_y;
	p->trim_valid_rect.w = path->in_trim.size_x;
	p->trim_valid_rect.h = path->in_trim.size_y;

	pm = &pctx->ctx[pctx->cur_ctx_id].blk_pm;
	aem_win = &(pm->aem.win_info);
	afm_crop = &(pm->afm.crop_size);
	p->ae_valid_rect.x = aem_win->offset_x;
	p->ae_valid_rect.y = aem_win->offset_y;
	p->ae_valid_rect.w = aem_win->blk_width * aem_win->blk_num_x;
	p->ae_valid_rect.h = aem_win->blk_height * aem_win->blk_num_y;

	p->af_valid_rect.x = afm_crop->x;
	p->af_valid_rect.y = afm_crop->y;
	p->af_valid_rect.w = afm_crop->w;
	p->af_valid_rect.h = afm_crop->h;

	return 0;
}

static int dcamcore_thread_loop(void *arg)
{
	struct cam_thread_info *thrd = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}

	thrd = (struct cam_thread_info *)arg;
	pr_debug("%s loop starts %px\n", thrd->thread_name, thrd);
	while (1) {
		if (!IS_ERR_OR_NULL(thrd) && wait_for_completion_interruptible(
			&thrd->thread_com) == 0) {
			if (atomic_cmpxchg(&thrd->thread_stop, 1, 0) == 1) {
				pr_info("thread %s should stop.\n", thrd->thread_name);
				break;
			}
			pr_debug("thread %s trigger\n", thrd->thread_name);
			thrd->proc_func(thrd->ctx_handle);
		} else {
			pr_debug("thread %s exit!", thrd->thread_name);
			break;
		}
	}
	pr_debug("%s thread stop.\n", thrd->thread_name);
	complete(&thrd->thread_stop_com);

	return 0;
}

int dcamcore_thread_create(void *ctx_handle, struct cam_thread_info *thrd, proc_func func)
{
	thrd->ctx_handle = ctx_handle;
	thrd->proc_func = func;
	atomic_set(&thrd->thread_stop, 0);
	init_completion(&thrd->thread_com);
	init_completion(&thrd->thread_stop_com);
	thrd->thread_task = kthread_run(dcamcore_thread_loop,
		thrd, "%s", thrd->thread_name);
	if (IS_ERR_OR_NULL(thrd->thread_task)) {
		pr_err("fail to start thread %s\n", thrd->thread_name);
		thrd->thread_task = NULL;
		return -EFAULT;
	}
	return 0;
}

static void dcamcore_thread_stop(struct cam_thread_info *thrd)
{
	int ret = 0;

	if (thrd->thread_task) {
		atomic_set(&thrd->thread_stop, 1);
		complete(&thrd->thread_com);
		ret = wait_for_completion_timeout(&thrd->thread_stop_com, DCAM_STOP_TIMEOUT);
		if (ret == 0)
			pr_err("fail to wait interruption_proc, timeout.\n");
		else
			pr_info("dcam interruption_proc thread wait time %d\n", ret);
		thrd->thread_task = NULL;
	}
}

static int dcamcore_ioctrl(void *dcam_handle, enum dcam_ioctrl_cmd cmd, void *param)
{
	int ret = 0;
	struct dcam_sw_context *pctx = NULL;
	struct dcam_mipi_info *cap = NULL;
	struct dcam_path_desc *path = NULL;
	struct camera_frame *frame = NULL;
	struct dcam_hw_fbc_ctrl arg;
	struct dcam_hw_ebd_set set;
	struct cam_hw_gtm_update gtmarg;
	struct dcam_dev_param *pm = NULL;
	int *fbc_mode = NULL;
	uint32_t gtm_param_idx = 0;
	struct cam_hw_info *hw = NULL;

	if (!dcam_handle) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	pctx = (struct dcam_sw_context *)dcam_handle;

	if (unlikely(atomic_read(&pctx->state) == STATE_INIT)) {
		pr_err("fail to get valid dev state of DCAM%d\n", pctx->hw_ctx_id);
		return -EINVAL;
	}
	hw = pctx->dev->hw;
	switch (cmd) {
	case DCAM_IOCTL_CFG_CAP:
		cap = &pctx->cap_info;
		memcpy(cap, param, sizeof(struct dcam_mipi_info));
		pctx->is_4in1 = cap->is_4in1;
		pctx->dcam_slice_mode = cap->dcam_slice_mode;
		pctx->slice_count = 0;
		break;
	case DCAM_IOCTL_CFG_STATIS_BUF:
		ret = dcamcore_statis_buffer_cfg(pctx, param);
		break;
	case DCAM_IOCTL_PUT_RESERV_STATSBUF:
		ret = dcamcore_reserved_buffer_put(pctx);
		break;
	case DCAM_IOCTL_INIT_STATIS_Q:
		ret = dcamcore_statis_bufferq_init(pctx);
		break;
	case DCAM_IOCTL_DEINIT_STATIS_Q:
		ret = dcamcore_statis_bufferq_deinit(pctx);
		dcamcore_statis_buffer_unmap(pctx);
		break;
	case DCAM_IOCTL_RECFG_PARAM:
		/* online context id is always 0 */
		ret = dcamcore_param_reconfig(pctx, DCAM_CXT_0, param);
		break;
	case DCAM_IOCTL_CFG_EBD:
		pctx->is_ebd = 1;
		set.idx = pctx->hw_ctx_id;
		set.p = param;
		ret = hw->dcam_ioctl(hw, DCAM_HW_CFG_EBD_SET, &set);
		break;
	case DCAM_IOCTL_CFG_SEC:
		ret = dcamcore_dcamsec_cfg(pctx, param);
		break;
	case DCAM_IOCTL_CFG_FBC:
		fbc_mode = (int *)param;
		/* update compressed flag for reserved buffer */
		if (*fbc_mode == DCAM_FBC_FULL_14_BIT ||
			*fbc_mode == DCAM_FBC_FULL_10_BIT)
			path = &pctx->path[DCAM_PATH_FULL];
		else if (*fbc_mode == DCAM_FBC_BIN_14_BIT ||
			*fbc_mode == DCAM_FBC_BIN_10_BIT)
			path = &pctx->path[DCAM_PATH_BIN];
		else if (*fbc_mode == DCAM_FBC_RAW_14_BIT ||
			*fbc_mode == DCAM_FBC_RAW_10_BIT)
			path = &pctx->path[DCAM_PATH_RAW];
		else if (*fbc_mode == DCAM_FBC_DISABLE)
			return 0;

		if (!path) {
			pr_info("Unsupport fbc mode %d\n", *fbc_mode);
			return 0;
		}
		arg.idx = pctx->hw_ctx_id;
		arg.fbc_mode = *fbc_mode;
		arg.data_bits = path->data_bits;
		arg.fmt = path->out_fmt;
		arg.path_id = path->path_id;

		list_for_each_entry(frame, &path->reserved_buf_queue.head, list) {
			if (!frame)
				break;
			else {
				frame->is_compressed = 1;
				if (*fbc_mode == DCAM_FBC_FULL_14_BIT ||
					*fbc_mode == DCAM_FBC_BIN_14_BIT ||
					*fbc_mode == DCAM_FBC_RAW_14_BIT)
					frame->compress_4bit_bypass = 0;
			}
		}
		break;
	case DCAM_IOCTL_CFG_RPS:
		ret = dcamcore_rps_cfg(pctx, param);
		break;
	case DCAM_IOCTL_GET_PATH_RECT:
		ret = dcamcore_path_rect_get(pctx, param);
		break;
	case DCAM_IOCTL_CFG_STATIS_BUF_SKIP:
		ret = dcamcore_statis_buffer_skip_cfg(pctx, param);
		break;
	case DCAM_IOCTL_CFG_GTM_UPDATE:
		pm = &pctx->ctx[DCAM_CXT_0].blk_pm;
		gtm_param_idx = *(uint32_t *)param;

		if (gtm_param_idx == DCAM_GTM_PARAM_CAP) {
			pm->gtm[DCAM_GTM_PARAM_PRE].update_en = 0;
			pm->rgb_gtm[DCAM_GTM_PARAM_PRE].update_en = 0;
		} else {
			pm->gtm[DCAM_GTM_PARAM_PRE].update_en = 1;
			pm->rgb_gtm[DCAM_GTM_PARAM_PRE].update_en = 1;
		}
		pr_debug("DCAM_IOCTL_CFG_GTM_UPDATE gtm_param_idx %d\n", gtm_param_idx);
		gtmarg.gtm_idx = gtm_param_idx;
		gtmarg.idx = pctx->hw_ctx_id;
		gtmarg.hw = hw;
		gtmarg.blk_dcam_pm = pm;
		gtmarg.glb_reg_lock = pctx->glb_reg_lock;
		hw->dcam_ioctl(hw, DCAM_HW_CFG_GTM_UPDATE, &gtmarg);
		break;
	case DCAM_IOCTL_CFG_PYR_DEC_EN:
		pctx->is_pyr_rec = *(uint32_t *)param;
		break;
	case DCAM_IOCTL_CREAT_INT_THREAD:
		sprintf(pctx->dcam_interruption_proc_thrd.thread_name, "dcam%d_interruption_proc", pctx->sw_ctx_id);
		ret = dcamcore_thread_create(pctx, &pctx->dcam_interruption_proc_thrd, dcamint_interruption_proc);
		cam_queue_init(&pctx->interruption_sts_queue, DCAM_INT_PROC_FRM_NUM, dcamcore_empty_interrupt_put);
		if (ret) {
			pr_err("fail to creat offline dcam int proc thread.\n");
			dcamcore_thread_stop(&pctx->dcam_interruption_proc_thrd);
		}
		break;
	default:
		pr_err("fail to get a known cmd: %d\n", cmd);
		ret = -EFAULT;
		break;
	}

	return ret;
}

static int dcamcore_cb_set(void *dcam_handle, int ctx_id,
		dcam_dev_callback cb, void *priv_data)
{
	int ret = 0;
	struct dcam_pipe_dev *dev = NULL;
	struct dcam_sw_context *pctx = NULL;

	if (!dcam_handle || !cb || !priv_data) {
		pr_err("fail to get valid input ptr, dcam_handle %p, callback %p, priv_data %p\n",
			dcam_handle, cb, priv_data);
		return -EFAULT;
	}
	if (ctx_id < 0 || ctx_id >= DCAM_SW_CONTEXT_MAX) {
		pr_err("fail to get legal ctx_id %d\n", ctx_id);
		return -EFAULT;
	}

	dev = (struct dcam_pipe_dev *)dcam_handle;
	pctx = &dev->sw_ctx[ctx_id];
	if (pctx->dcam_cb_func == NULL) {
		pctx->dcam_cb_func = cb;
		pctx->cb_priv_data = priv_data;
		pr_debug("ctx: %d, cb %p, %p\n", ctx_id, cb, priv_data);
	}

	return ret;
}

static int dcamcore_share_buf_cb_set(void *dcam_handle, int ctx_id,
		share_buf_get_cb cb, void *priv_data)
{
	int ret = 0;
	struct dcam_pipe_dev *dev = NULL;
	struct dcam_sw_context *pctx = NULL;

	if (!dcam_handle || !cb || !priv_data) {
		pr_err("fail to get valid param, dcam_handle=%p, cb=%p, priv_data=%p\n",
			dcam_handle, cb, priv_data);
		return -EFAULT;
	}

	dev = (struct dcam_pipe_dev *)dcam_handle;
	pctx = &dev->sw_ctx[ctx_id];
	if (pctx->buf_get_cb == NULL) {
		pctx->buf_get_cb = cb;
		pctx->buf_cb_data = priv_data;
	}

	return ret;
}



static int dcamcore_dev_start(void *dcam_handle, int online)
{
	int ret = 0;
	int i = 0;
	struct dcam_sw_context *pctx = NULL;
	struct dcam_dev_param *pm = NULL;
	struct dcam_path_desc *path = NULL;
	struct cam_hw_info *hw = NULL;
	struct cam_hw_reg_trace trace;
	struct dcam_hw_path_ctrl pause;
	struct dcam_hw_start parm;
	struct dcam_hw_mipi_cap caparg;
	struct dcam_hw_path_start patharg;
	struct dcam_hw_sram_ctrl sramarg;
	struct dcam_hw_fbc_ctrl fbc_arg;
	unsigned long flag;
	struct dcam_fmcu_enable fmcu_enable = {0};
	struct dcam_hw_context *pctx_hw = NULL;

	if (!dcam_handle) {
		pr_err("fail to get valid dcam_sw_context\n");
		return -EFAULT;
	}

	pctx = (struct dcam_sw_context *)dcam_handle;
	pctx_hw = pctx->hw_ctx;
	pctx->cur_ctx_id = DCAM_CXT_0;
	hw = pctx->dev->hw;
	pm = &pctx->ctx[pctx->cur_ctx_id].blk_pm;

	if ((pm->lsc.buf.mapping_state & CAM_BUF_MAPPING_DEV) == 0) {
		ret = cam_buf_iommu_map(&pm->lsc.buf, CAM_IOMMUDEV_DCAM);
		if (ret)
			pm->lsc.buf.iova[0] = 0L;
	}

	if (pctx->offline) {
		init_completion(&pctx->frm_done);
		complete(&pctx->frm_done);
		init_completion(&pctx->slice_done);
		complete(&pctx->slice_done);
		atomic_set(&pctx->state, STATE_RUNNING);
		pctx->frame_index = DCAM_FRAME_INDEX_MAX;
		return ret;
	}

	ret = atomic_read(&pctx->state);
	if (unlikely(ret != STATE_IDLE)) {
		pr_err("fail to get a valid state, starting DCAM%u in state %d\n", pctx->hw_ctx_id, ret);
		return -EINVAL;
	}

	pr_info("DCAM%u start: %px, state = %d, sw%d\n", pctx->hw_ctx_id, pctx, atomic_read(&pctx->state), pctx->sw_ctx_id);

	sprintf(pctx->dcam_interruption_proc_thrd.thread_name, "dcam%d_interruption_proc", pctx->sw_ctx_id);
	if (pctx->dcam_interruption_proc_thrd.thread_task == NULL) {
		ret = dcamcore_thread_create(pctx, &pctx->dcam_interruption_proc_thrd, dcamint_interruption_proc);
		cam_queue_init(&pctx->interruption_sts_queue, DCAM_INT_PROC_FRM_NUM, dcamcore_empty_interrupt_put);
		if (ret) {
			pr_err("fail to creat online dcam int pro thread.\n");
			dcamcore_thread_stop(&pctx->dcam_interruption_proc_thrd);
			return ret;
		}
	}

	if (pctx->raw_callback) {
		atomic_set(&pctx->path[DCAM_PATH_AEM].user_cnt, 0);
		atomic_set(&pctx->path[DCAM_PATH_PDAF].user_cnt, 0);
		atomic_set(&pctx->path[DCAM_PATH_AFM].user_cnt, 0);
		atomic_set(&pctx->path[DCAM_PATH_AFL].user_cnt, 0);
		atomic_set(&pctx->path[DCAM_PATH_HIST].user_cnt, 0);
		atomic_set(&pctx->path[DCAM_PATH_LSCM].user_cnt, 0);
		atomic_set(&pctx->path[DCAM_PATH_3DNR].user_cnt, 0);
		atomic_set(&pctx->path[DCAM_PATH_GTM_HIST].user_cnt, 0);
	} else {
		/* enable statistic paths  */
		if (pm->aem.bypass == 0)
			atomic_set(&pctx->path[DCAM_PATH_AEM].user_cnt, 1);
		if (pm->lscm.bypass == 0)
			atomic_set(&pctx->path[DCAM_PATH_LSCM].user_cnt, 1);
		if (pm->afm.bypass == 0)
			atomic_set(&pctx->path[DCAM_PATH_AFM].user_cnt, 1);
		if (pm->afl.afl_info.bypass == 0)
			atomic_set(&pctx->path[DCAM_PATH_AFL].user_cnt, 1);
		if (pm->hist.bayerHist_info.hist_bypass == 0)
			atomic_set(&pctx->path[DCAM_PATH_HIST].user_cnt, 1);

		if (hw->ip_isp->frbg_hist_support == 0 && pm->hist_roi.hist_roi_info.bypass == 0)
			atomic_set(&pctx->path[DCAM_PATH_FRGB_HIST].user_cnt, 1);

		if (pm->pdaf.bypass == 0)
			atomic_set(&pctx->path[DCAM_PATH_PDAF].user_cnt, 1);
		if (pctx->is_3dnr)
			atomic_set(&pctx->path[DCAM_PATH_3DNR].user_cnt, 1);

		if (pctx->is_ebd)
			atomic_set(&pctx->path[DCAM_PATH_VCH2].user_cnt, 1);

		if ((pm->gtm[DCAM_GTM_PARAM_PRE].gtm_info.bypass_info.gtm_mod_en || pm->gtm[DCAM_GTM_PARAM_CAP].gtm_info.bypass_info.gtm_mod_en)
			&& (pm->gtm[DCAM_GTM_PARAM_PRE].gtm_calc_mode == GTM_SW_CALC))
			atomic_set(&pctx->path[DCAM_PATH_GTM_HIST].user_cnt, 1);
		if ((pm->rgb_gtm[DCAM_GTM_PARAM_PRE].rgb_gtm_info.bypass_info.gtm_mod_en || pm->rgb_gtm[DCAM_GTM_PARAM_CAP].rgb_gtm_info.bypass_info.gtm_mod_en)
			&& (pm->rgb_gtm[DCAM_GTM_PARAM_PRE].gtm_calc_mode == GTM_SW_CALC))
			atomic_set(&pctx->path[DCAM_PATH_GTM_HIST].user_cnt, 1);
	}

	pctx->base_fid = pm->frm_idx;
	pctx->frame_index = 0;
	pctx->index_to_set = 0;
	pr_info("dcam%d start  frame idx %d\n", pctx->hw_ctx_id, pm->frm_idx);
	pctx->iommu_status = 0;
	memset(pctx->frame_ts, 0,
		sizeof(pctx->frame_ts[0]) * DCAM_FRAME_TIMESTAMP_COUNT);
	memset(pctx->frame_ts_boot, 0,
		sizeof(pctx->frame_ts_boot[0]) * DCAM_FRAME_TIMESTAMP_COUNT);

	caparg.idx = pctx->hw_ctx_id;
	caparg.cap_info = pctx->cap_info;
	caparg.slowmotion_count = pctx->slowmotion_count;
	ret = hw->dcam_ioctl(hw, DCAM_HW_CFG_MIPI_CAP_SET, &caparg);
	if (ret < 0) {
		pr_err("fail to set DCAM%u mipi cap\n", pctx->hw_ctx_id);
		return ret;
	}

	for (i = 0; i < DCAM_PATH_MAX; i++) {
		path = &pctx->path[i];
		patharg.path_id = i;
		patharg.idx = pctx->hw_ctx_id;
		patharg.slowmotion_count = pctx->slowmotion_count;
		patharg.pdaf_path_eb = (pm->pdaf.bypass == 0) ? 1 : 0;
		patharg.pdaf_type = pm->pdaf.pdaf_type;
		patharg.cap_info = pctx->cap_info;
		patharg.pack_bits = pctx->path[i].pack_bits;
		patharg.src_sel = pctx->path[i].src_sel;
		patharg.bayer_pattern = pctx->path[i].bayer_pattern;
		patharg.in_trim = pctx->path[i].in_trim;
		patharg.endian = pctx->path[i].endian;
		patharg.out_fmt = pctx->path[i].out_fmt;
		patharg.data_bits = pctx->path[i].data_bits;
		patharg.is_pack = pctx->path[i].is_pack;
		pr_debug("path %d, fmt %d, bits %d, is pack %d src_sel %d pack_bits %d\n", i, patharg.out_fmt, patharg.data_bits, patharg.is_pack, patharg.src_sel, patharg.pack_bits);
		atomic_set(&path->set_frm_cnt, 0);

		if (atomic_read(&path->user_cnt) < 1 || atomic_read(&path->is_shutoff) > 0)
			continue;

		if (path->path_id == DCAM_PATH_FULL) {
			spin_lock_irqsave(&path->state_lock, flag);
			if (path->state == DCAM_PATH_PAUSE) {
				hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_START, &patharg);
				pause.idx = pctx->hw_ctx_id;
				pause.path_id = path->path_id;
				pause.type = HW_DCAM_PATH_PAUSE;
				hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_CTRL, &pause);
				spin_unlock_irqrestore(&path->state_lock, flag);
				continue;
			}
			spin_unlock_irqrestore(&path->state_lock, flag);
		}
		path->size_update = 1;

		/* online ctx use fmcu means slowmotion */
		if (pctx->slw_type != DCAM_SLW_FMCU)
			ret = dcam_path_store_frm_set(pctx, path);
		if (ret < 0) {
			pr_err("fail to set frame for DCAM%u %s , ret %d\n",
				pctx->hw_ctx_id, dcam_path_name_get(path->path_id), ret);
			return ret;
		}

		if ((atomic_read(&path->set_frm_cnt) > 0) || (pctx->slw_type == DCAM_SLW_FMCU)) {
			hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_START, &patharg);
			if (pctx->path[i].fbc_mode) {
				fbc_arg.idx = pctx->hw_ctx_id;
				fbc_arg.path_id = i;
				fbc_arg.fmt = pctx->path[i].out_fmt;
				fbc_arg.data_bits = pctx->path[i].data_bits;
				fbc_arg.fbc_mode = pctx->path[i].fbc_mode;
				hw->dcam_ioctl(hw, DCAM_HW_CFG_FBC_CTRL, &fbc_arg);
			}
		}
	}

	if (!pctx->param_frame_sync && pctx->raw_alg_type  == RAW_ALG_AI_SFNR) {
		uint32_t shutoff = 1;
		struct dcam_hw_path_stop patharg;

		patharg.path_id = DCAM_PATH_RAW;
		patharg.idx = pctx->hw_ctx_id;
		hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_STOP, &patharg);
		pctx->dev->dcam_pipe_ops->cfg_path(pctx,
			DCAM_PATH_CFG_SHUTOFF, patharg.path_id, &shutoff);
	}

	if (pctx->is_4in1 == 0)
		ret = dcam_init_lsc(pm, 1);
	if (ret < 0) {
		pr_err("fail to init lsc\n");
		return ret;
	}

	if (pctx->slw_type == DCAM_SLW_FMCU)
		ret = dcam_path_fmcu_slw_queue_set(pctx);

	/* TODO: change AFL trigger */
	atomic_set(&pctx->path[DCAM_PATH_AFL].user_cnt, 0);
	atomic_set(&pctx->state, STATE_RUNNING);

	dcam_int_tracker_reset(pctx->hw_ctx_id);

	if (pctx->slw_type == DCAM_SLW_FMCU) {
		fmcu_enable.enable = 1;
		fmcu_enable.idx = pctx->hw_ctx_id;
		hw->dcam_ioctl(hw, DCAM_HW_FMCU_EBABLE, &fmcu_enable);
		pctx_hw->fmcu->ops->hw_start(pctx_hw->fmcu);
	} else {
		parm.idx = pctx->hw_ctx_id;
		parm.format = pctx->cap_info.format;
		parm.raw_callback = pctx->raw_callback;
		parm.dcam_sw_context = pctx;
		pr_debug("idx %d  format %d raw_callback %d x %d\n", parm.idx, parm.format, parm.raw_callback, pctx->cap_info.cap_size.size_x);
		hw->dcam_ioctl(hw, DCAM_HW_CFG_START, &parm);
	}

	if (pctx->is_4in1 == 0) {
		sramarg.sram_ctrl_en = 1;
		sramarg.idx = pctx->hw_ctx_id;
		hw->dcam_ioctl(hw, DCAM_HW_CFG_SRAM_CTRL_SET, &sramarg);
	}

	trace.type = NORMAL_REG_TRACE;
	trace.idx = pctx->hw_ctx_id;
	hw->isp_ioctl(hw, ISP_HW_CFG_REG_TRACE, &trace);
	pctx->auto_cpy_id = 0;
	pctx->err_count = 1;
	pr_info("dcam%d done state = %d\n", pctx->hw_ctx_id, atomic_read(&pctx->state));
	return ret;
}

static int dcamcore_dev_stop(void *dcam_handle, enum dcam_stop_cmd pause)
{
	int ret = 0, state = 0;
	int i = 0;
	struct dcam_sw_context *pctx = NULL;
	struct dcam_dev_param *pm;
	struct dcam_path_desc *path = NULL;
	struct cam_hw_info *hw = NULL;
	uint32_t hw_ctx_id = DCAM_HW_CONTEXT_MAX;

	if (!dcam_handle) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	pctx = (struct dcam_sw_context *)dcam_handle;

	hw_ctx_id = pctx->hw_ctx_id;
	hw = pctx->dev->hw;
	state = atomic_read(&pctx->state);
	if ((unlikely(state == STATE_INIT) || unlikely(state == STATE_IDLE)) &&
			((pctx->csi_connect_stat == DCAM_CSI_RESUME) || (hw->csi_connect_type == DCAM_BIND_FIXED))) {
		pr_warn("warning: DCAM%d not started yet\n", pctx->hw_ctx_id);
		return -EINVAL;
	}

	if (hw_ctx_id != DCAM_HW_CONTEXT_MAX && pause != DCAM_RECOVERY) {
		hw->dcam_ioctl(hw, DCAM_HW_CFG_STOP, pctx);
		hw->dcam_ioctl(hw, DCAM_HW_CFG_RESET, &hw_ctx_id);
		dcam_int_tracker_dump(hw_ctx_id);
		dcam_int_tracker_reset(hw_ctx_id);
	}

	if (!pctx->offline && pause != DCAM_DEV_ERR && !pctx->virtualsensor) {
		dcamcore_thread_stop(&pctx->dcam_interruption_proc_thrd);
		cam_queue_clear(&pctx->interruption_sts_queue, struct camera_interrupt, list);
	}

	atomic_set(&pctx->state, STATE_IDLE);
	for (i = DCAM_CXT_1; i < DCAM_CXT_NUM; i++) {
		pctx->ctx[i].ctx_id = i;
		if (atomic_read(&pctx->ctx[i].user_cnt) > 0)
			dcamcore_pmctx_deinit(&pctx->ctx[i]);
	}

	pm = &pctx->ctx[DCAM_CXT_0].blk_pm;
	if (pause == DCAM_STOP || pause == DCAM_DEV_ERR) {
		pm->aem.bypass = 1;
		pm->afm.bypass = 1;
		pm->afl.afl_info.bypass = 1;
		pm->hist.bayerHist_info.hist_bypass = 1;
		pm->hist_roi.hist_roi_info.bypass = 1;
		pm->lscm.bypass = 1;
		pm->pdaf.bypass = 1;
		pm->gtm[DCAM_GTM_PARAM_PRE].update_en = 1;
		pm->gtm[DCAM_GTM_PARAM_CAP].update_en = 1;
		pm->frm_idx = 0;
		pr_info("stop all\n");
		pctx->is_3dnr = pctx->is_4in1 = pctx->is_raw_alg = 0;
	} else if (pause == DCAM_PAUSE_ONLINE || pause == DCAM_RECOVERY) {
		pm->frm_idx = pctx->base_fid + pctx->frame_index;
		pr_info("dcam%d online pause fram id %d %d, base_fid %d, new %d\n", hw_ctx_id,
			pctx->frame_index, pctx->index_to_set, pctx->base_fid, pm->frm_idx);
	} else {
		pr_info("offline stopped %d\n", pause);
	}

	if (pm->lsc.buf.mapping_state & CAM_BUF_MAPPING_DEV)
		cam_buf_iommu_unmap(&pm->lsc.buf);

	pctx->err_count = 0;

	for (i = 0; i < DCAM_PATH_MAX; i++) {
		path = &pctx->path[i];
		atomic_set(&path->is_shutoff, 0);
	}

	atomic_set(&pctx->shadow_done_cnt, 0);
	atomic_set(&pctx->shadow_config_cnt, 0);

	pr_info("stop dcam pipe dev[%d] state = %d!\n", hw_ctx_id, atomic_read(&pctx->state));
	return ret;
}

void dcam_core_get_fmcu(struct dcam_hw_context *pctx_hw)
{
	int ret = 0, hw_ctx_id = 0;
	struct cam_hw_info *hw = NULL;
	struct dcam_fmcu_ctx_desc *fmcu_info = NULL;

	if (!pctx_hw) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	hw = pctx_hw->hw;
	hw_ctx_id = pctx_hw->hw_ctx_id;

	if(hw->ip_dcam[hw_ctx_id]->fmcu_support) {
		fmcu_info = dcam_fmcu_ctx_desc_get(hw, hw_ctx_id);
		if (fmcu_info && fmcu_info->ops) {
			fmcu_info->hw = hw;
			ret = fmcu_info->ops->ctx_init(fmcu_info);
			if (ret) {
				pr_err("fail to init fmcu ctx\n");
				dcam_fmcu_ctx_desc_put(fmcu_info);
				pctx_hw->fmcu = NULL;
			} else
				pctx_hw->fmcu = fmcu_info;
		} else
			pr_debug("no more fmcu or ops\n");
	}
}

void dcam_core_put_fmcu(struct dcam_hw_context *pctx_hw)
{
	struct dcam_fmcu_ctx_desc *fmcu_info = NULL;

	if (!pctx_hw) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	fmcu_info = (struct dcam_fmcu_ctx_desc *)pctx_hw->fmcu;
	if (fmcu_info) {
		fmcu_info->ops->ctx_deinit(fmcu_info);
		dcam_fmcu_ctx_desc_put(fmcu_info);
		pctx_hw->fmcu = NULL;
	}
}

static void dcamcore_pmbuf_destroy(void *param)
{
	struct camera_frame *param_frm = NULL;

	if (!param) {
		pr_err("fail to get input ptr.\n");
		return;
	}

	param_frm = (struct camera_frame *)param;

	mutex_destroy(&param_frm->pm->param_lock);
	mutex_destroy(&param_frm->pm->lsc.lsc_lock);

	if (param_frm->pm->lsc.buf.mapping_state & CAM_BUF_MAPPING_DEV)
		cam_buf_iommu_unmap(&param_frm->pm->lsc.buf);
	cam_buf_kunmap(&param_frm->pm->lsc.buf);
	cam_buf_free(&param_frm->pm->lsc.buf);
	if(param_frm->pm->lsc.weight_tab) {
		kfree(param_frm->pm->lsc.weight_tab);
		param_frm->pm->lsc.weight_tab = NULL;
	}
	if(param_frm->pm->lsc.weight_tab_x) {
		kfree(param_frm->pm->lsc.weight_tab_x);
		param_frm->pm->lsc.weight_tab_x = NULL;
	}
	if(param_frm->pm->lsc.weight_tab_y) {
		kfree(param_frm->pm->lsc.weight_tab_y);
		param_frm->pm->lsc.weight_tab_y = NULL;
	}
	vfree(param_frm->pm);
	param_frm->pm = NULL;
	cam_queue_empty_frame_put(param_frm);
}

static int dcamcore_context_init(struct dcam_pipe_dev *dev)
{
	int i = 0, j = 0, ret = 0;
	struct dcam_sw_context *pctx_sw = NULL;
	struct dcam_hw_context *pctx_hw = NULL;

	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	pr_debug("dcam sw contexts init start!\n");
	memset(&dev->sw_ctx[0], 0, sizeof(dev->sw_ctx));
	for (i = 0; i < DCAM_SW_CONTEXT_MAX; i++) {
		pctx_sw = &dev->sw_ctx[i];
		pctx_sw->sw_ctx_id = i;
		pctx_sw->hw_ctx_id = DCAM_HW_CONTEXT_MAX;
		pctx_sw->dev = dev;
		pctx_sw->hw_ctx = NULL;
		atomic_set(&pctx_sw->user_cnt, 0);
		atomic_set(&pctx_sw->shadow_done_cnt, 0);
		atomic_set(&pctx_sw->shadow_config_cnt, 0);
		cam_queue_init(&pctx_sw->blk_param_queue, DCAM_OFFLINE_PARAM_Q_LEN, dcamcore_pmbuf_destroy);
		pr_info("dcam context %d init done!\n", i);

		for (j = 0; j < DCAM_CXT_NUM; j++) {
			pctx_sw->ctx[j].ctx_id = j;
			atomic_set(&pctx_sw->ctx[j].user_cnt, 0);
		}
		/* use context[0] by default */
		pctx_sw->cur_ctx_id = DCAM_CXT_0;
		ret = dcamcore_pmctx_init(pctx_sw, &pctx_sw->ctx[pctx_sw->cur_ctx_id]);
		if (ret) {
			pr_err("fail to init dcam pm[0]\n");
			return ret;
		}
	}

	pr_debug("dcam hw contexts init start!\n");
	memset(&dev->hw_ctx[0], 0, sizeof(dev->hw_ctx));
	for (i = 0; i < DCAM_HW_CONTEXT_MAX; i++) {
		pctx_hw = &dev->hw_ctx[i];
		pctx_hw->hw_ctx_id = i;
		pctx_hw->sw_ctx_id = DCAM_SW_CONTEXT_MAX;
		pctx_hw->sw_ctx = NULL;
		pctx_hw->hw = dev->hw;
		atomic_set(&pctx_hw->user_cnt, 0);

		pr_debug("register irq for dcam %d. irq_no %d\n", i, dev->hw->ip_dcam[i]->irq_no);
		ret = dcam_int_irq_request(&dev->hw->pdev->dev,
			dev->hw->ip_dcam[i]->irq_no, pctx_hw);
		if (ret)
			pr_err("fail to register irq for hw_ctx %d\n", i);

		dcam_core_get_fmcu(pctx_hw);
	}
	pr_debug("done!\n");
	return ret;
}

static int dcamcore_context_deinit(struct dcam_pipe_dev *dev)
{
	int ret = 0;
	int i = 0, j = 0, k = 0;
	struct dcam_sw_context *pctx_sw = NULL;
	struct dcam_hw_context *pctx_hw = NULL;
	struct dcam_path_desc *path = NULL;

	pr_debug("enter.\n");
	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	for (k = DCAM_ONLINE_CTX; k <= DCAM_OFFLINE_CTX; k++) {
		for (i = 0; i < DCAM_SW_CONTEXT_MAX; i++) {
			pctx_sw = &dev->sw_ctx[i];
			/*free offline ctx when k = 0, free online ctx when k = 1*/
			if (!pctx_sw || (k == pctx_sw->offline))
				continue;
			cam_queue_clear(&pctx_sw->blk_param_queue, struct camera_frame, list);
			for (j = DCAM_CXT_0; j < DCAM_CXT_NUM; j++) {
				pctx_sw->ctx[j].ctx_id = j;
				if (atomic_read(&pctx_sw->ctx[j].user_cnt) > 0)
					dcamcore_pmctx_deinit(&pctx_sw->ctx[j]);
			}

			/* free all used path here if user did not call put_path  */
			for (j = 0; j < DCAM_PATH_MAX; j++) {
				path = &pctx_sw->path[i];
				if (atomic_read(&path->user_cnt) > 0)
					dev->dcam_pipe_ops->put_path(pctx_sw, path->path_id);
			}
			dev->dcam_pipe_ops->put_context(dev, pctx_sw->sw_ctx_id);
			memset(pctx_sw, 0, sizeof(struct dcam_sw_context));
			atomic_set(&pctx_sw->user_cnt, 0);
		}
	}
	for (i = 0; i < DCAM_HW_CONTEXT_MAX; i++) {
		pctx_hw = &dev->hw_ctx[i];
		dcam_core_put_fmcu(pctx_hw);
		dcam_int_irq_free(&dev->hw->pdev->dev, pctx_hw);
		atomic_set(&pctx_hw->user_cnt, 0);
	}
	pr_debug("dcam contexts deinit done!\n");
	return ret;
}

static int dcamcore_dev_open(void *dcam_handle)
{
	int ret = 0;
	int hw_ctx_id = 0;
	struct dcam_pipe_dev *dev = (struct dcam_pipe_dev *)dcam_handle;

	pr_info("enter.\n");
	if (!dcam_handle) {
		pr_err("fail to get valid input ptr, isp_handle %p\n", dcam_handle);
		return -EFAULT;
	}

	mutex_lock(&dev->ctx_mutex);
	if (atomic_inc_return(&dev->enable) == 1) {
		ret = dcam_drv_hw_init(dev);
		if (ret) {
			pr_err("fail to init hw. ret: %d\n", ret);
			goto context_init_fail;
		}
		dev->hw->dcam_ioctl(dev->hw, DCAM_HW_CFG_INIT_AXI, &hw_ctx_id);

		ret = dcamcore_context_init(dev);
		if (ret) {
			pr_err("fail to init dcam context.\n");
			ret = -EFAULT;
			goto exit;
		}

		mutex_init(&dev->path_mutex);
		spin_lock_init(&dev->ctx_lock);
	}
	mutex_unlock(&dev->ctx_mutex);
	pr_info("open dcam pipe dev done! enable %d\n",atomic_read(&dev->enable));
	return 0;

context_init_fail:
	dcamcore_context_deinit(dev);
exit:
	atomic_dec(&dev->enable);
	mutex_unlock(&dev->ctx_mutex);
	pr_err("fail to open dcam dev!\n");
	return ret;
}

static int dcamcore_dev_close(void *dcam_handle)
{
	int ret = 0;
	struct dcam_pipe_dev *dev = (struct dcam_pipe_dev *)dcam_handle;

	if (!dcam_handle) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	mutex_lock(&dev->ctx_mutex);
	if (atomic_dec_return(&dev->enable) == 0) {
		mutex_destroy(&dev->path_mutex);
		ret = dcamcore_context_deinit(dev);
		ret = dcam_drv_hw_deinit(dev);
	}
	mutex_unlock(&dev->ctx_mutex);
	pr_info("dcam dev disable done enable %d\n",atomic_read(&dev->enable));
	return ret;

}

static int dcamcore_context_get(void *dcam_handle)
{
	int ret = 0;
	int i = 0;
	int sel_ctx_id = -1;
	struct dcam_sw_context *pctx = NULL;
	struct dcam_pipe_dev *dev = NULL;
	struct dcam_path_desc *path = NULL;

	if (!dcam_handle) {
		pr_err("fail to get valid input ptr, dcam_handle %px\n",
			dcam_handle);
		return -EFAULT;
	}
	pr_debug("start.\n");

	dev = (struct dcam_pipe_dev *)dcam_handle;

	mutex_lock(&dev->path_mutex);

	for (i = 0; i < DCAM_SW_CONTEXT_MAX; i++) {
		pctx = &dev->sw_ctx[i];
		if (atomic_inc_return(&pctx->user_cnt) == 1) {
			sel_ctx_id = i;
			pr_info("sw %d, get context\n", pctx->sw_ctx_id);
			break;
		}
		atomic_dec(&pctx->user_cnt);
	}

	if (sel_ctx_id == -1) {
		pr_err("fail to get dam sw context\n");
		goto exit;
	}
	ret = atomic_read(&pctx->state);
	if (unlikely(ret != STATE_INIT)) {
		pr_err("fail to get a valid ctx state, ctx%u, state=%d\n",
			pctx->hw_ctx_id, ret);
		mutex_unlock(&dev->path_mutex);
		return -EINVAL;
	}

	memset(&pctx->path[0], 0, sizeof(pctx->path));
	for (i = 0; i < DCAM_PATH_MAX; i++) {
		path = &pctx->path[i];
		path->path_id = i;
		atomic_set(&path->user_cnt, 0);
		atomic_set(&path->set_frm_cnt, 0);
		atomic_set(&path->is_shutoff, 0);
		spin_lock_init(&path->size_lock);
		spin_lock_init(&path->state_lock);

		if (path->path_id == DCAM_PATH_BIN) {
			path->rds_coeff_size = RDS_COEF_TABLE_SIZE;
			path->rds_coeff_buf = kzalloc(path->rds_coeff_size, GFP_KERNEL);
			if (path->rds_coeff_buf == NULL) {
				path->rds_coeff_size = 0;
				pr_err("fail to alloc rds coeff buffer.\n");
				ret = -ENOMEM;
				goto exit;
			}
		}
	}

	cam_queue_init(&pctx->in_queue, DCAM_IN_Q_LEN, dcamcore_src_frame_ret);
	cam_queue_init(&pctx->proc_queue, DCAM_PROC_Q_LEN, dcamcore_src_frame_ret);
	cam_queue_init(&pctx->fullpath_mv_queue, DCAM_FULL_MV_Q_LEN, cam_queue_empty_mv_state_put);
	cam_queue_init(&pctx->binpath_mv_queue, DCAM_BIN_MV_Q_LEN, cam_queue_empty_mv_state_put);
	memset(&pctx->nr3_me, 0, sizeof(struct nr3_me_data));

	atomic_set(&pctx->state, STATE_IDLE);
	spin_lock_init(&pctx->glb_reg_lock);
	spin_lock_init(&pctx->fbc_lock);
	/* for debugfs */
	atomic_inc(&s_dcam_opened[pctx->sw_ctx_id]);
exit:
	mutex_unlock(&dev->path_mutex);
	pr_info("Get context id %d\n", sel_ctx_id);
	return sel_ctx_id;
}

static int dcamcore_context_put(void *dcam_handle, int ctx_id)
{
	int ret = 0;
	struct dcam_pipe_dev *dev = NULL;
	struct dcam_sw_context *pctx = NULL;

	if (!dcam_handle) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	if (ctx_id < 0 || ctx_id >= DCAM_SW_CONTEXT_MAX) {
		pr_err("fail to get a vaild sw_ctx_id %d\n", ctx_id);
		return -EFAULT;
	}

	dev = (struct dcam_pipe_dev *)dcam_handle;
	pctx = &dev->sw_ctx[ctx_id];

	mutex_lock(&dev->path_mutex);

	if (atomic_dec_return(&pctx->user_cnt) == 0) {
		if (unlikely(atomic_read(&pctx->state) == STATE_INIT)) {
			mutex_unlock(&dev->path_mutex);
			pr_err("fail to get dev state, DCAM%u already closed\n", pctx->hw_ctx_id);
			return -EINVAL;
		}

		cam_queue_clear(&pctx->in_queue, struct camera_frame, list);
		cam_queue_clear(&pctx->proc_queue, struct camera_frame, list);
		cam_queue_clear(&pctx->fullpath_mv_queue, struct camera_frame, list);
		cam_queue_clear(&pctx->binpath_mv_queue, struct camera_frame, list);
		if (pctx->offline || pctx->virtualsensor) {
			dcamcore_thread_stop(&pctx->dcam_interruption_proc_thrd);
			cam_queue_clear(&pctx->interruption_sts_queue, struct camera_interrupt, list);
		}
		memset(&pctx->nr3_me, 0, sizeof(struct nr3_me_data));
		pctx->dcam_cb_func = NULL;
		pctx->buf_get_cb = NULL;

		if (pctx->path[DCAM_PATH_BIN].rds_coeff_buf) {
			kfree(pctx->path[DCAM_PATH_BIN].rds_coeff_buf);
			pctx->path[DCAM_PATH_BIN].rds_coeff_buf = NULL;
			pctx->path[DCAM_PATH_BIN].rds_coeff_size = 0;
		}
		/* for debugfs */
		atomic_dec(&s_dcam_opened[pctx->sw_ctx_id]);
	} else {
		pr_debug("ctx%d.already release.\n", ctx_id);
		atomic_set(&pctx->user_cnt, 0);
	}
	atomic_set(&pctx->state, STATE_INIT);
	mutex_unlock(&dev->path_mutex);

	pr_info("put dcam ctx[%d]!\n", ctx_id);
	return ret;
}

static int dcamcore_scene_fdrl_get(uint32_t prj_id,
		struct dcam_data_ctrl_info * out)
{
	int ret = 0;

	if (!out) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	switch (prj_id) {
	case SHARKL5pro:
		if (out->raw_alg_type){
			out->start_ctrl = DCAM_START_CTRL_EN;
			out->callback_ctrl = DCAM_CALLBACK_CTRL_USER;
		} else {
			out->start_ctrl = DCAM_START_CTRL_EN;
			out->callback_ctrl = DCAM_CALLBACK_CTRL_ISP;
			out->in_format = DCAM_STORE_RAW_BASE;
		}
		break;
	case QOGIRL6:
		if (out->raw_alg_type) {
			out->start_ctrl = DCAM_START_CTRL_EN;
			out->callback_ctrl = DCAM_CALLBACK_CTRL_USER;
		} else {
			out->start_ctrl = DCAM_START_CTRL_EN;
			out->callback_ctrl = DCAM_CALLBACK_CTRL_ISP;
			out->in_format = DCAM_STORE_RAW_BASE;
		}
		break;
	case QOGIRN6pro:
	case QOGIRN6L:
		if (out->raw_alg_type == RAW_ALG_MFNR) {
			out->start_ctrl = DCAM_START_CTRL_EN;
			out->callback_ctrl = DCAM_CALLBACK_CTRL_USER;
			out->in_format = DCAM_STORE_RAW_BASE;
			out->out_format = DCAM_STORE_RAW_BASE;
			out->need_raw_path = 1;
		} else {
			out->start_ctrl = DCAM_START_CTRL_EN;
			out->callback_ctrl = DCAM_CALLBACK_CTRL_USER;
			out->in_format = DCAM_STORE_RAW_BASE;
			out->out_format = DCAM_STORE_FRGB;
		}
		break;
	default:
		pr_err("fail to support current project %d\n", prj_id);
		ret = -EFAULT;
		break;
	}

	return ret;
}

static int dcamcore_scene_fdrh_get(uint32_t prj_id,
		struct dcam_data_ctrl_info * out)
{
	int ret = 0;

	if (!out) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	switch (prj_id) {
	case SHARKL5pro:
		out->start_ctrl = DCAM_START_CTRL_EN;
		out->callback_ctrl = DCAM_CALLBACK_CTRL_ISP;
		out->in_format = DCAM_STORE_RAW_BASE;
		out->out_format = DCAM_STORE_RAW_BASE;
		break;
	case QOGIRL6:
		if (out->raw_alg_type == RAW_ALG_FDR_V1) {
			out->start_ctrl = DCAM_START_CTRL_DIS;
		} else {
			out->start_ctrl = DCAM_START_CTRL_EN;
			out->callback_ctrl = DCAM_CALLBACK_CTRL_ISP;
			out->in_format = DCAM_STORE_RAW_BASE;
			out->out_format = DCAM_STORE_RAW_BASE;
		}
		break;
	case QOGIRN6pro:
	case QOGIRN6L:
		if (out->raw_alg_type == RAW_ALG_FDR_V1) {
			out->start_ctrl = DCAM_START_CTRL_EN;
			out->callback_ctrl = DCAM_CALLBACK_CTRL_ISP;
			out->in_format = DCAM_STORE_FRGB;
			out->out_format = DCAM_STORE_YUV420;
		} else {
			out->start_ctrl = DCAM_START_CTRL_EN;
			out->callback_ctrl = DCAM_CALLBACK_CTRL_ISP;
			out->in_format = DCAM_STORE_RAW_BASE;
			out->out_format = DCAM_STORE_YUV420;
		}
		break;
	default:
		pr_err("fail to support current project %d\n", prj_id);
		ret = -EFAULT;
		break;
	}

	return ret;
}

static int dcamcore_datactrl_get(void *handle, void *in, void *out)
{
	int ret = 0;
	uint32_t prj_id = 0;
	struct dcam_sw_context *pctx = NULL;
	struct dcam_data_ctrl_info *data_ctrl = NULL;
	struct cam_data_ctrl_in *cfg_in = NULL;

	if (!handle || !in || !out) {
		pr_err("fail to get valid input ptr %p %p %p\n", handle, in, out);
		return -EFAULT;
	}

	pctx = (struct dcam_sw_context *)handle;
	cfg_in = (struct cam_data_ctrl_in *)in;
	data_ctrl = (struct dcam_data_ctrl_info *)out;
	prj_id = pctx->dev->hw->prj_id;
	data_ctrl->raw_alg_type = pctx->raw_alg_type;

	switch (cfg_in->scene_type) {
	case CAM_SCENE_CTRL_FDR_L:
		ret = dcamcore_scene_fdrl_get(prj_id, data_ctrl);
		if (ret) {
			pr_err("fail to get scene fdrl %d\n", prj_id);
			goto exit;
		}
		break;
	case CAM_SCENE_CTRL_FDR_H:
		ret = dcamcore_scene_fdrh_get(prj_id, data_ctrl);
		if (ret) {
			pr_err("fail to get scene fdrh %d\n", prj_id);
			goto exit;
		}
		break;
	default:
		pr_err("fail to support current scene %d\n", cfg_in->scene_type);
		ret = -EFAULT;
		break;
	}

exit:
	return ret;
}

/*
 * Operations for this dcam_pipe_ops.
 */
static struct dcam_pipe_ops s_dcam_pipe_ops = {
	.open = dcamcore_dev_open,
	.close = dcamcore_dev_close,
	.start = dcamcore_dev_start,
	.stop = dcamcore_dev_stop,
	.get_path = dcamcore_path_get,
	.put_path = dcamcore_path_put,
	.cfg_path = dcamcore_path_cfg,
	.ioctl = dcamcore_ioctrl,
	.cfg_blk_param = dcamcore_param_cfg,
	.set_callback = dcamcore_cb_set,
	.get_context = dcamcore_context_get,
	.put_context = dcamcore_context_put,
	.get_datactrl = dcamcore_datactrl_get,
	.share_buf_set_cb = dcamcore_share_buf_cb_set,
};

int dcam_core_hw_context_id_get(struct dcam_sw_context *pctx)
{
	int i = 0;
	int hw_ctx_id = DCAM_HW_CONTEXT_MAX;
	struct dcam_pipe_dev *dev = pctx->dev;
	struct dcam_hw_context *pctx_hw = NULL;

	for (i = 0; i < DCAM_HW_CONTEXT_MAX; i++) {
		pctx_hw = &dev->hw_ctx[i];
		if ((pctx->sw_ctx_id == pctx_hw->sw_ctx_id)
			&& (pctx == pctx_hw->sw_ctx)) {
			hw_ctx_id = pctx_hw->hw_ctx_id;
			break;
		}
	}
	pr_debug("get dcam hw_ctx_id %d\n", hw_ctx_id);
	return hw_ctx_id;
}

int dcam_core_context_bind(struct dcam_sw_context *pctx, enum dcam_bind_mode mode, uint32_t dcam_idx)
{
	int i = 0;
	uint32_t hw_ctx_id = DCAM_HW_CONTEXT_MAX;
	unsigned long flag = 0;
	struct dcam_pipe_dev *dev = pctx->dev;
	struct dcam_hw_context *pctx_hw = NULL;

	spin_lock_irqsave(&dev->ctx_lock, flag);
	if (mode == DCAM_BIND_FIXED) {
		pctx_hw = &dev->hw_ctx[dcam_idx];
		if (pctx->sw_ctx_id == pctx_hw->sw_ctx_id
			&& pctx_hw->sw_ctx == pctx) {
			atomic_inc(&pctx_hw->user_cnt);
			pr_info("sw %d & hw %d are already binded, cnt=%d\n",
				pctx->sw_ctx_id, dcam_idx, atomic_read(&pctx_hw->user_cnt));
			spin_unlock_irqrestore(&dev->ctx_lock, flag);
			return 0;
		}
		if (atomic_inc_return(&pctx_hw->user_cnt) == 1) {
			hw_ctx_id = pctx_hw->hw_ctx_id;
			goto exit;
		}
		atomic_dec(&pctx_hw->user_cnt);
	} else if (mode == DCAM_BIND_DYNAMIC) {
		for (i = 0; i < DCAM_HW_CONTEXT_MAX; i++) {
			pctx_hw = &dev->hw_ctx[i];
			if (pctx->sw_ctx_id == pctx_hw->sw_ctx_id
				&& pctx_hw->sw_ctx == pctx) {
				atomic_inc(&pctx_hw->user_cnt);
				pr_info("sw %d & hw %d are already binded, cnt=%d\n",
					pctx->sw_ctx_id, i, atomic_read(&pctx_hw->user_cnt));
				spin_unlock_irqrestore(&dev->ctx_lock, flag);
				return 0;
			}
		}

		for (i = 0; i < DCAM_HW_CONTEXT_MAX; i++) {
			pctx_hw = &dev->hw_ctx[i];
			if (atomic_inc_return(&pctx_hw->user_cnt) == 1) {
				hw_ctx_id = pctx_hw->hw_ctx_id;
				if (pctx->slowmotion_count && dev->hw->ip_dcam[i]->fmcu_support) {
					pctx->slw_type = DCAM_SLW_FMCU;
					if (!pctx_hw->fmcu)
						continue;
				}
				goto exit;
			}
			atomic_dec(&pctx_hw->user_cnt);
		}
	}
exit:
	spin_unlock_irqrestore(&dev->ctx_lock, flag);
	if (hw_ctx_id == DCAM_HW_CONTEXT_MAX) {
		pr_debug("fail to get hw_ctx_id. mode=%d\n", mode);
		return -1;
	}
	pctx_hw->sw_ctx = pctx;
	pctx_hw->sw_ctx_id = pctx->sw_ctx_id;
	pctx->hw_ctx_id = pctx_hw->hw_ctx_id;
	pctx->hw_ctx = pctx_hw;
	pctx->ctx[DCAM_CXT_0].blk_pm.idx = pctx_hw->hw_ctx_id;
	pctx->ctx[DCAM_CXT_1].blk_pm.idx = pctx_hw->hw_ctx_id;
	pctx->ctx[DCAM_CXT_2].blk_pm.idx = pctx_hw->hw_ctx_id;
	pctx->ctx[DCAM_CXT_3].blk_pm.idx = pctx_hw->hw_ctx_id;
	pr_info("sw_ctx_id=%d, hw_ctx_id=%d, mode=%d, cnt=%d\n", pctx_hw->sw_ctx_id, hw_ctx_id, mode, atomic_read(&pctx_hw->user_cnt));
	return 0;
}

int dcam_core_context_unbind(struct dcam_sw_context *pctx)
{
	int i, j, cnt;
	struct dcam_pipe_dev *dev = pctx->dev;
	struct dcam_hw_context *pctx_hw = NULL;
	unsigned long flag = 0;

	if (dcam_core_hw_context_id_get(pctx) < 0) {
		pr_err("fail to binding sw ctx %d to any hw ctx\n", pctx->sw_ctx_id);
		return -EINVAL;
	}

	spin_lock_irqsave(&dev->ctx_lock, flag);
	for (i = 0; i < DCAM_HW_CONTEXT_MAX; i++) {
		pctx_hw = &dev->hw_ctx[i];
		if ((pctx->sw_ctx_id != pctx_hw->sw_ctx_id) ||
			(pctx != pctx_hw->sw_ctx))
				continue;

		if (atomic_dec_return(&pctx_hw->user_cnt) == 0) {
			pr_info("sw_id=%d, hw_id=%d unbind success\n",
				pctx->sw_ctx_id, pctx_hw->hw_ctx_id);
			pctx_hw->sw_ctx = NULL;
			pctx_hw->sw_ctx_id = DCAM_SW_CONTEXT_MAX;
			pctx->hw_ctx_id = DCAM_HW_CONTEXT_MAX;
			pctx->hw_ctx = NULL;
			for (j = DCAM_CXT_0; j < DCAM_CXT_NUM; j++) {
				pctx->ctx[j].blk_pm.idx = DCAM_HW_CONTEXT_MAX;
				pctx->ctx[j].blk_pm.non_zsl_cap = 0;
			}
			goto exit;
		}

		cnt = atomic_read(&pctx_hw->user_cnt);
		if (cnt >= 1) {
			pr_info("sw id=%d, hw_id=%d, cnt=%d\n",
				pctx->sw_ctx_id, pctx_hw->hw_ctx_id, cnt);
		} else {
			pr_info("should not be here: sw id=%d, hw id=%d, cnt=%d\n",
				pctx->sw_ctx_id, pctx_hw->hw_ctx_id, cnt);
			spin_unlock_irqrestore(&dev->ctx_lock, flag);
			return -EINVAL;
		}
	}

exit:
	spin_unlock_irqrestore(&dev->ctx_lock, flag);
	return 0;
}

void *dcam_core_pipe_dev_get(struct cam_hw_info *hw)
{
	struct dcam_pipe_dev *dev = NULL;

	mutex_lock(&s_dcam_dev_mutex);

	if (s_dcam_dev) {
		atomic_inc(&s_dcam_dev->user_cnt);
		dev = s_dcam_dev;
		pr_info("s_dcam_dev_new is already exist=%px, user_cnt=%d",
			s_dcam_dev, atomic_read(&s_dcam_dev->user_cnt));
		goto exit;
	}

	dev = vzalloc(sizeof(struct dcam_pipe_dev));
	if (!dev)
		goto exit;

	atomic_set(&dev->user_cnt, 1);
	atomic_set(&dev->enable, 0);
	mutex_init(&dev->ctx_mutex);

	dev->dcam_pipe_ops = &s_dcam_pipe_ops;
	dev->hw = hw;
	s_dcam_dev = dev;
	if (dev)
		pr_info("get dcam pipe dev: %px\n", dev);
exit:
	mutex_unlock(&s_dcam_dev_mutex);

	return dev;
}

int dcam_core_pipe_dev_put(void *dcam_handle)
{
	int ret = 0;
	struct dcam_pipe_dev *dev = (struct dcam_pipe_dev *)dcam_handle;

	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	pr_info("put dcam pipe dev:%px, s_dcam_dev:%px,  users: %d, enable: %d\n",
		dev, s_dcam_dev, atomic_read(&dev->user_cnt), atomic_read(&dev->enable));

	mutex_lock(&s_dcam_dev_mutex);

	if (dev != s_dcam_dev) {
		mutex_unlock(&s_dcam_dev_mutex);
		pr_err("fail to match dev: %px, %px\n", dev, s_dcam_dev);
		return -EINVAL;
	}

	if (atomic_dec_return(&dev->user_cnt) == 0) {
		pr_info("free dcam pipe dev %px\n", dev);
		mutex_destroy(&dev->ctx_mutex);
		vfree(dev);
		dev = NULL;
		s_dcam_dev = NULL;
	}
	mutex_unlock(&s_dcam_dev_mutex);

	if (dev)
		pr_info("put dcam pipe dev: %px\n", dev);

	return ret;
}
