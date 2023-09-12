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

#include <linux/err.h>
#include <sprd_mm.h>

#include "cam_scaler.h"
#include "cam_scaler_ex.h"
#include "cam_queue.h"
#include "cam_debugger.h"
#include "dcam_reg.h"
#include "dcam_int.h"
#include "cam_scaler_ex.h"
#include "dcam_hw_adpt.h"
#include "dcam_path.h"

/* Macro Definitions */
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "DCAM_PATH: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

/*
 * path name for debug output
 */
static const char *_DCAM_PATH_NAMES[DCAM_PATH_MAX] = {
	[DCAM_PATH_FULL] = "FULL",
	[DCAM_PATH_BIN] = "BIN",
	[DCAM_PATH_RAW] = "RAW",
	[DCAM_PATH_PDAF] = "PDAF",
	[DCAM_PATH_VCH2] = "VCH2",
	[DCAM_PATH_VCH3] = "VCH3",
	[DCAM_PATH_AEM] = "AEM",
	[DCAM_PATH_AFM] = "AFM",
	[DCAM_PATH_AFL] = "AFL",
	[DCAM_PATH_HIST] = "HIST",
	[DCAM_PATH_FRGB_HIST] = "HIST ROI",
	[DCAM_PATH_3DNR] = "3DNR",
	[DCAM_PATH_BPC] = "BPC",
	[DCAM_PATH_LSCM] = "LSCM",
	[DCAM_PATH_GTM_HIST] = "GTM_HIST",
};

/*
 * convert @path_id to path name
 */
const char *dcam_path_name_get(enum dcam_path_id path_id)
{
	return is_path_id(path_id) ? _DCAM_PATH_NAMES[path_id] : "(null)";
}

int dcam_path_base_cfg(void *dcam_ctx_handle,
		struct dcam_path_desc *path, void *param)
{
	int ret = 0;
	unsigned long flags = 0;
	struct dcam_sw_context *dcam_sw_ctx = NULL;
	struct dcam_path_cfg_param *ch_desc = NULL;

	if (!dcam_ctx_handle || !path || !param) {
		pr_err("fail to get valid param, dcam_handle=%p, path=%p, param=%p.\n",
			dcam_ctx_handle, path, param);
		return -EFAULT;
	}
	dcam_sw_ctx = (struct dcam_sw_context *)dcam_ctx_handle;
	ch_desc = (struct dcam_path_cfg_param *)param;
	dcam_sw_ctx->cap_info.cap_size = ch_desc->input_trim;

	switch (path->path_id) {
	case DCAM_PATH_FULL:
		spin_lock_irqsave(&path->size_lock, flags);
		/* for l3 & l5 & l5p & l6*/
		path->src_sel = ch_desc->is_raw ? ORI_RAW_SRC_SEL : PROCESS_RAW_SRC_SEL;
		path->frm_deci = ch_desc->frm_deci;
		path->frm_skip = ch_desc->frm_skip;
		if (ch_desc->raw_fmt == DCAM_RAW_8)
			path->pack_bits = RAW_8;
		else if (ch_desc->raw_fmt == DCAM_RAW_HALFWORD_10)
			path->pack_bits = RAW_HALF10;
		else if (ch_desc->raw_fmt == DCAM_RAW_PACK_10)
			path->pack_bits = RAW_PACK10;
		else if (ch_desc->raw_fmt == DCAM_RAW_14)
			path->pack_bits = RAW_HALF14;

		path->endian = ch_desc->endian;
		path->is_4in1 = ch_desc->is_4in1;
		path->bayer_pattern = ch_desc->bayer_pattern;
		path->out_fmt = ch_desc->dcam_out_fmt;
		path->data_bits = ch_desc->dcam_out_bits;

		path->is_pack = 0;
		if ((path->out_fmt & DCAM_STORE_YUV_BASE) && (path->data_bits == DCAM_STORE_10_BIT))
			path->is_pack = 1;
		if ((path->out_fmt & DCAM_STORE_RAW_BASE) && (path->pack_bits == RAW_PACK10))
			path->is_pack = 1;

		path->base_update = 1;
		spin_unlock_irqrestore(&path->size_lock, flags);
		break;
	case DCAM_PATH_BIN:
		path->frm_deci = ch_desc->frm_deci;
		path->frm_skip = ch_desc->frm_skip;
		if (ch_desc->raw_fmt == DCAM_RAW_8)
			path->pack_bits = RAW_8;
		else if (ch_desc->raw_fmt == DCAM_RAW_HALFWORD_10)
			path->pack_bits = RAW_HALF10;
		else if (ch_desc->raw_fmt == DCAM_RAW_PACK_10)
			path->pack_bits = RAW_PACK10;
		else if (ch_desc->raw_fmt == DCAM_RAW_14)
			path->pack_bits = RAW_HALF14;

		path->endian = ch_desc->endian;
		path->is_4in1 = ch_desc->is_4in1;
		path->bayer_pattern = ch_desc->bayer_pattern;
		path->out_fmt = ch_desc->dcam_out_fmt;
		path->data_bits = ch_desc->dcam_out_bits;
		path->pyr_data_bits = ch_desc->pyr_data_bits;
		path->pyr_is_pack = ch_desc->pyr_is_pack;

		path->is_pack = 0;
		if ((path->out_fmt & DCAM_STORE_YUV_BASE) && (path->data_bits == DCAM_STORE_10_BIT))
			path->is_pack = 1;
		if ((path->out_fmt & DCAM_STORE_RAW_BASE) && (path->pack_bits == RAW_PACK10))
			path->is_pack = 1;

		/*
		 * TODO:
		 * Better not binding dcam_if feature to BIN path, which is a
		 * architecture defect and not going to be fixed now.
		 */
		dcam_sw_ctx->slowmotion_count = ch_desc->slowmotion_count;
		if (!dcam_sw_ctx->slowmotion_count)
			dcam_sw_ctx->slw_type = DCAM_SLW_OFF;
		else
			dcam_sw_ctx->slw_type = DCAM_SLW_AP;
		dcam_sw_ctx->is_3dnr |= ch_desc->enable_3dnr;
		dcam_sw_ctx->raw_cap = ch_desc->raw_cap;
		break;
	case DCAM_PATH_RAW:
		/* for n6pro*/
		path->src_sel = ch_desc->is_raw ? ORI_RAW_SRC_SEL : ch_desc->raw_src;
		path->frm_deci = ch_desc->frm_deci;
		path->frm_skip = ch_desc->frm_skip;
		if (ch_desc->raw_fmt == DCAM_RAW_8) {
			path->pack_bits = RAW_8;
			path->data_bits = DCAM_STORE_8_BIT;
			path->is_pack = 0;
		} else if (ch_desc->raw_fmt == DCAM_RAW_HALFWORD_10) {
			path->pack_bits = RAW_HALF10;
			path->data_bits = DCAM_STORE_10_BIT;
			path->is_pack = 0;
		} else if (ch_desc->raw_fmt == DCAM_RAW_PACK_10) {
			path->pack_bits = RAW_PACK10;
			path->data_bits = DCAM_STORE_10_BIT;
			path->is_pack = 1;
		} else if (ch_desc->raw_fmt == DCAM_RAW_14) {
			path->pack_bits = RAW_HALF14;
			path->data_bits = DCAM_STORE_14_BIT;
			path->is_pack = 0;
		}
		path->endian = ch_desc->endian;
		path->is_4in1 = ch_desc->is_4in1;
		path->bayer_pattern = ch_desc->bayer_pattern;
		path->out_fmt = ch_desc->dcam_out_fmt;
		dcam_sw_ctx->raw_cap = ch_desc->raw_cap;
		pr_info("raw path src %d, pack bits %d\n", path->src_sel, path->pack_bits);
		break;
	case DCAM_PATH_VCH2:
		path->endian = ch_desc->endian;
		path->src_sel = ch_desc->is_raw ? 1 : 0;
		break;
	default:
		pr_err("fail to get known path %d\n", path->path_id);
		ret = -EFAULT;
		break;
	}
	return ret;
}

int dcam_path_size_cfg(void *dcam_ctx_handle,
		struct dcam_path_desc *path,
		void *param)
{
	int ret = 0;
	uint32_t invalid = 0;
	struct img_size crop_size, dst_size;
	struct dcam_sw_context *dcam_sw_ctx = NULL;
	struct dcam_path_cfg_param *ch_desc;
	struct cam_hw_info *hw = NULL;
	struct dcam_hw_calc_rds_phase arg;
	unsigned long flag;

	if (!dcam_ctx_handle || !path || !param) {
		pr_err("fail to get valid param, dcam_handle=%p, path=%p, param=%p.\n",
			dcam_ctx_handle, path, param);
		return -EFAULT;
	}
	dcam_sw_ctx = (struct dcam_sw_context *)dcam_ctx_handle;
	hw = dcam_sw_ctx->dev->hw;
	if (!hw) {
		pr_err("fail to get a valid hw, hw ptr is NULL.\n");
		return -EFAULT;
	}

	ch_desc = (struct dcam_path_cfg_param *)param;

	switch (path->path_id) {
	case DCAM_PATH_RAW:
	case DCAM_PATH_FULL:
		spin_lock_irqsave(&path->size_lock, flag);
		if (path->size_update) {
			spin_unlock_irqrestore(&path->size_lock, flag);
			return -EFAULT;
		}
		path->in_size = ch_desc->input_size;
		path->in_trim = ch_desc->input_trim;

		invalid = 0;
		invalid |= ((path->in_size.w == 0) || (path->in_size.h == 0));
		invalid |= ((path->in_trim.start_x + path->in_trim.size_x) > path->in_size.w);
		invalid |= ((path->in_trim.start_y + path->in_trim.size_y) > path->in_size.h);
		if (invalid) {
			spin_unlock_irqrestore(&path->size_lock, flag);
			pr_err("fail to get valid size, size:%d %d, trim %d %d %d %d\n",
				path->in_size.w, path->in_size.h,
				path->in_trim.start_x, path->in_trim.start_y,
				path->in_trim.size_x,
				path->in_trim.size_y);
			return -EINVAL;
		}

		path->out_size = ch_desc->output_size;

		if (path->out_fmt & DCAM_STORE_RAW_BASE)
			path->out_pitch = cal_sprd_raw_pitch(path->out_size.w, path->pack_bits);
		else if (path->out_fmt & DCAM_STORE_YUV_BASE)
			path->out_pitch = cal_sprd_yuv_pitch(path->out_size.w, path->data_bits, path->is_pack);
		else
			path->out_pitch = path->out_size.w * 8;

		path->priv_size_data = ch_desc->priv_size_data;
		path->size_update = 1;
		spin_unlock_irqrestore(&path->size_lock, flag);

		pr_info("cfg %s path done. size %d %d %d %d\n",
			path->path_id == DCAM_PATH_RAW ? "raw" : "full", path->in_size.w, path->in_size.h,
			path->out_size.w, path->out_size.h);

		pr_info("sel %d. trim %d %d %d %d\n", path->src_sel,
			path->in_trim.start_x, path->in_trim.start_y,
			path->in_trim.size_x, path->in_trim.size_y);
		break;

	case DCAM_PATH_BIN:
		/* lock here to keep all size parameters updating is atomic.
		 * because the rds coeff caculation may be time-consuming,
		 * we should not disable irq here, or else may cause irq missed.
		 * Just trylock set_next_frame in irq handling to avoid deadlock
		 * If last updating has not been applied yet, will return error.
		 * error may happen if too frequent zoom ratio updateing,
		 * but should not happen for first time cfg before stream on,
		 * if error return, caller can discard updating
		 * or try cfg_size again after while.
		 */
		spin_lock_irqsave(&path->size_lock, flag);
		if (path->size_update) {
			if (atomic_read(&dcam_sw_ctx->state) != STATE_RUNNING)
				pr_info("Overwrite dcam path size before dcam start if any\n");
			else {
				spin_unlock_irqrestore(&path->size_lock, flag);
				pr_info("Previous path updating pending\n");
				return -EFAULT;
			}
		}

		path->in_size = ch_desc->input_size;
		path->in_trim = ch_desc->input_trim;
		path->zoom_ratio_base = ch_desc->zoom_ratio_base;
		path->total_in_trim = ch_desc->total_input_trim;
		path->out_size = ch_desc->output_size;

		invalid = 0;
		invalid |= ((path->in_size.w == 0) || (path->in_size.h == 0));
		/* trim should not be out range of source */
		invalid |= ((path->in_trim.start_x +
				path->in_trim.size_x) > path->in_size.w);
		invalid |= ((path->in_trim.start_y +
				path->in_trim.size_y) > path->in_size.h);

		/* output size should not be larger than trim ROI */
		invalid |= path->in_trim.size_x < path->out_size.w;
		invalid |= path->in_trim.size_y < path->out_size.h;

		/* Down scaling should not be smaller then 1/4*/
		invalid |= path->in_trim.size_x >
				(path->out_size.w * DCAM_SCALE_DOWN_MAX);
		invalid |= path->in_trim.size_y >
				(path->out_size.h * DCAM_SCALE_DOWN_MAX);

		if (invalid) {
			spin_unlock_irqrestore(&path->size_lock, flag);
			pr_err("fail to get valid size, size:%d %d, trim %d %d %d %d, dst %d %d\n",
				path->in_size.w, path->in_size.h,
				path->in_trim.start_x, path->in_trim.start_y,
				path->in_trim.size_x, path->in_trim.size_y,
				path->out_size.w, path->out_size.h);
			return -EINVAL;
		}
		crop_size.w = path->in_trim.size_x;
		crop_size.h = path->in_trim.size_y;
		dst_size = path->out_size;

		switch (path->out_fmt) {
			case DCAM_STORE_YUV_BASE:
			case DCAM_STORE_YUV422:
			case DCAM_STORE_YVU422:
			case DCAM_STORE_YUV420:
			case DCAM_STORE_YVU420:
				path->out_pitch = cal_sprd_yuv_pitch(path->out_size.w, path->data_bits, path->is_pack);

				if ((crop_size.w == dst_size.w) && (crop_size.h == dst_size.h)) {
					path->scaler_sel = DCAM_SCALER_BYPASS;
					break;
				}
				if (dst_size.w > DCAM_SCALER_MAX_WIDTH || path->in_trim.size_x > (dst_size.w * DCAM_SCALE_DOWN_MAX)) {
					pr_err("fail to support scaler, in width %d, out width %d\n",
						path->in_trim.size_x, dst_size.w);
					ret = -1;
				}
				path->scaler_sel = DCAM_SCALER_BY_YUVSCALER;
				path->scaler_info.scaler_factor_in = path->in_trim.size_x;
				path->scaler_info.scaler_factor_out = dst_size.w;
				path->scaler_info.scaler_ver_factor_in = path->in_trim.size_y;
				path->scaler_info.scaler_ver_factor_out = dst_size.h;
				path->scaler_info.scaler_out_width = dst_size.w;
				path->scaler_info.scaler_out_height = dst_size.h;
				path->scaler_info.work_mode = 2;
				path->scaler_info.scaler_bypass = 0;
				ret = cam_scaler_coeff_calc_ex(&path->scaler_info);
				if (ret)
					pr_err("fail to calc scaler coeff\n");
				break;
			case DCAM_STORE_RAW_BASE:
			case DCAM_STORE_FRGB:
				if ((crop_size.w == dst_size.w) &&
					(crop_size.h == dst_size.h))
					path->scaler_sel = DCAM_SCALER_BYPASS;
				else if ((dst_size.w * 2 == crop_size.w) &&
					(dst_size.h * 2 == crop_size.h)) {
					pr_debug("1/2 binning used. src %d %d, dst %d %d\n",
						crop_size.w, crop_size.h, dst_size.w,
						dst_size.h);
					path->scaler_sel = DCAM_SCALER_BINNING;
					path->bin_ratio = 0;
				} else if ((dst_size.w * 4 == crop_size.w) &&
					(dst_size.h * 4 == crop_size.h)) {
					pr_debug("1/4 binning used. src %d %d, dst %d %d\n",
						crop_size.w, crop_size.h, dst_size.w,
						dst_size.h);
					path->scaler_sel = DCAM_SCALER_BINNING;
					path->bin_ratio = 1;
				} else {
					pr_debug("RDS used. in %d %d, out %d %d\n",
						crop_size.w, crop_size.h, dst_size.w,
						dst_size.h);
					if (path->out_fmt & DCAM_STORE_RAW_BASE) {
						path->scaler_sel = DCAM_SCALER_RAW_DOWNSISER;
						path->gphase.rds_input_h_global = path->in_trim.size_y;
						path->gphase.rds_input_w_global = path->in_trim.size_x;
						path->gphase.rds_output_w_global = path->out_size.w;
						path->gphase.rds_output_h_global = path->out_size.h;
						arg.gphase = &path->gphase;
						arg.slice_id = 0;
						arg.slice_end0 = 0;
						arg.slice_end1 = 0;
						ret = hw->dcam_ioctl(hw, DCAM_HW_CFG_CALC_RDS_PHASE_INFO, &arg);
						if (ret)
							pr_err("fail to calc rds phase info\n");
						cam_scaler_dcam_rds_coeff_gen((uint16_t)crop_size.w,
							(uint16_t)crop_size.h,
							(uint16_t)dst_size.w,
							(uint16_t)dst_size.h,
							(uint32_t *)path->rds_coeff_buf);
					}
				}

				if (path->out_fmt & DCAM_STORE_RAW_BASE)
					path->out_pitch = cal_sprd_raw_pitch(path->out_size.w, path->pack_bits);
				else if (path->out_fmt == DCAM_STORE_FRGB)
					path->out_pitch = path->out_size.w * 8;
				break;
			default:
				pr_err("fail to get path->out_fmt :%d\n", path->out_fmt);
				break;
		}

		path->priv_size_data = ch_desc->priv_size_data;
		path->size_update = 1;
		/* if 3dnr path enable, need update when zoom */
		{
			struct dcam_path_desc *path_3dnr;

			path_3dnr = &dcam_sw_ctx->path[DCAM_PATH_3DNR];
			if (atomic_read(&path_3dnr->user_cnt) > 0)
				path_3dnr->size_update = 1;
			path_3dnr->in_trim = path->in_trim;
		}
		spin_unlock_irqrestore(&path->size_lock, flag);

		pr_info("cfg bin path done. size %d %d  dst %d %d\n",
			path->in_size.w, path->in_size.h,
			path->out_size.w, path->out_size.h);
		pr_info("scaler %d. trim %d %d %d %d\n", path->scaler_sel,
			path->in_trim.start_x, path->in_trim.start_y,
			path->in_trim.size_x, path->in_trim.size_y);
		break;

	default:
		if (path->path_id == DCAM_PATH_VCH2 && path->src_sel)
			return ret;

		pr_err("fail to get known path %d\n", path->path_id);
		ret = -EFAULT;
		break;
	}
	return ret;
}

/*
 * Set skip num for path @path_id so that we can update address accordingly in
 * CAP SOF interrupt.
 *
 * For slow motion mode.
 * On previous project, there's no slow motion support for AEM on DCAM. Instead,
 * we use @skip_num to generate AEM TX DONE every @skip_num frame. If @skip_num
 * is set by algorithm, we should configure AEM output address accordingly in
 * CAP SOF every @skip_num frame.
 */
int dcam_path_skip_num_set(void *dcam_ctx_handle,
		int path_id, uint32_t skip_num)
{
	struct dcam_path_desc *path = NULL;
	struct dcam_sw_context *dcam_sw_ctx = (struct dcam_sw_context *)dcam_ctx_handle;
	if (unlikely(!dcam_sw_ctx || !is_path_id(path_id)))
		return -EINVAL;

	if (atomic_read(&dcam_sw_ctx->state) == STATE_RUNNING) {
		pr_warn("warning: DCAM%u %s set skip_num while running is forbidden\n",
			dcam_sw_ctx->hw_ctx_id, dcam_path_name_get(path_id));
		return -EINVAL;
	}

	path = &dcam_sw_ctx->path[path_id];
	path->frm_deci = skip_num;
	path->frm_deci_cnt = 0;

	pr_info("DCAM%u %s set skip_num %u\n",
		dcam_sw_ctx->hw_ctx_id, dcam_path_name_get(path_id), skip_num);

	return 0;
}

static inline struct camera_frame *
dcam_path_frame_cycle(struct dcam_sw_context *dcam_sw_ctx, struct dcam_path_desc *path)
{
	uint32_t src = 0;
	struct camera_frame *frame = NULL;

	if ((path->path_id == DCAM_PATH_FULL || path->path_id == DCAM_PATH_RAW) &&
		(dcam_sw_ctx->is_raw_alg)) {
		frame = cam_queue_dequeue(&path->alter_out_queue, struct camera_frame, list);
		src = 0;
	}

	if (frame == NULL) {
		frame = cam_queue_dequeue(&path->out_buf_queue, struct camera_frame, list);
		src = 1;
		if (frame != NULL && (path->path_id == DCAM_PATH_FULL || (path->path_id == DCAM_PATH_VCH2 && frame->channel_id == CAM_CH_DCAM_VCH))) {
			if (!(frame->buf.mapping_state & CAM_BUF_MAPPING_DEV)) {
				if (cam_buf_iommu_map(&frame->buf, CAM_IOMMUDEV_DCAM)) {
					pr_err("fail to map dcam buf\n");
					cam_queue_enqueue(&path->out_buf_queue, &frame->list);
					frame = NULL;
				}
			}
		}
	}

	if (frame && (src == 0)) {
		/* usr buffer for raw, mapping delay to here */
		if (!(frame->buf.mapping_state & CAM_BUF_MAPPING_DEV)) {
			if (cam_buf_iommu_map(&frame->buf, CAM_IOMMUDEV_DCAM)) {
				pr_err("fail to map dcam buf\n");
				cam_queue_enqueue(&path->alter_out_queue, &frame->list);
				pr_debug("mapping raw buffer for ch %d, mfd %d\n",
					frame->channel_id, frame->buf.mfd[0]);
				frame = NULL;
			}
		}
	}

	if (path->path_id == DCAM_PATH_FULL && frame == NULL) {
		if (dcam_sw_ctx->buf_get_cb)
			dcam_sw_ctx->buf_get_cb(SHARE_BUF_GET_CB, (void *)&frame, dcam_sw_ctx->buf_cb_data);
		if (frame != NULL) {
			frame->is_reserved = 0;
			frame->priv_data = dcam_sw_ctx;
			if (!(frame->buf.mapping_state & CAM_BUF_MAPPING_DEV)) {
				if (cam_buf_iommu_map(&frame->buf, CAM_IOMMUDEV_DCAM)) {
					pr_err("fail to iommu map\n");
					cam_queue_enqueue(&path->out_buf_queue, &frame->list);
					frame = NULL;
				}
			}
		}
	}

	if (frame == NULL) {
		frame = cam_queue_dequeue(&path->reserved_buf_queue, struct camera_frame, list);
		pr_debug("use reserved buffer for path %d, DCAM %d\n", path->path_id, dcam_sw_ctx->hw_ctx_id);
	}

	if (frame == NULL) {
		pr_debug("DCAM%u %s buffer unavailable\n",
			dcam_sw_ctx->hw_ctx_id, dcam_path_name_get(path->path_id));
		return ERR_PTR(-ENOMEM);
	}

	if (cam_queue_enqueue(&path->result_queue, &frame->list) < 0) {
		if (frame->is_reserved)
			cam_queue_enqueue(&path->reserved_buf_queue, &frame->list);
		else if (src == 1) {
			cam_queue_enqueue(&path->out_buf_queue, &frame->list);
			if (path->path_id == DCAM_PATH_FULL || (path->path_id == DCAM_PATH_VCH2 && frame->channel_id == CAM_CH_DCAM_VCH))
				cam_buf_iommu_unmap(&frame->buf);
		} else {
			cam_buf_iommu_unmap(&frame->buf);
			cam_queue_enqueue(&path->alter_out_queue, &frame->list);
		}
		pr_err_ratelimited("fail to enqueue frame to result_queue, hw_ctx_id %u %s overflow\n",
			dcam_sw_ctx->hw_ctx_id, dcam_path_name_get(path->path_id));
		return ERR_PTR(-EPERM);
	}

	frame->fid = dcam_sw_ctx->base_fid + dcam_sw_ctx->index_to_set;
	return frame;
}

static inline void dcampath_frame_pointer_swap(struct camera_frame **frame1,
			struct camera_frame **frame2)
{
	struct camera_frame *frame;

	frame = *frame1;
	*frame1 = *frame2;
	*frame2 = frame;
}

static uint32_t dcampath_dec_align_width(uint32_t w, uint32_t layer_num)
{
	uint32_t width = 0, i = 0;
	uint32_t w_align = PYR_DEC_WIDTH_ALIGN;

	for (i = 0; i < layer_num; i++)
		w_align *= 2;

	width = ALIGN(w, w_align);
	return width;
}

static uint32_t dcampath_dec_align_heigh(uint32_t h, uint32_t layer_num)
{
	uint32_t height = 0, i = 0;
	uint32_t h_align = PYR_DEC_HEIGHT_ALIGN;

	for (i = 0; i < layer_num; i++)
		h_align *= 2;

	height = ALIGN(h, h_align);
	return height;
}

static int dcampath_pyr_dec_cfg(struct dcam_path_desc *path,
		struct camera_frame *frame, struct cam_hw_info *hw, uint32_t idx)
{
	int ret = 0, i = 0;
	uint32_t align_w = 0, align_h = 0;
	uint32_t layer_num = 0;
	struct dcam_hw_dec_store_cfg dec_store;
	struct dcam_hw_dec_online_cfg dec_online;

	if (!path || !frame || !hw) {
		pr_err("fail to check param, path%px, frame%px\n", path, frame);
		return -EINVAL;
	}

	memset(&dec_store, 0, sizeof(struct dcam_hw_dec_store_cfg));
	memset(&dec_online, 0, sizeof(struct dcam_hw_dec_online_cfg));

	layer_num = path->dec_store_info.layer_num;
	dec_online.idx = idx;
	dec_online.layer_num = layer_num;
	dec_online.chksum_clr_mode = 0;
	dec_online.chksum_work_mode = 0;
	dec_online.path_sel = DACM_DEC_PATH_DEC;
	dec_online.hor_padding_num = path->dec_store_info.align_w - path->out_size.w;
	dec_online.ver_padding_num = path->dec_store_info.align_h - path->out_size.h;
	if (dec_online.hor_padding_num)
		dec_online.hor_padding_en = 1;
	if (dec_online.ver_padding_num)
		dec_online.ver_padding_en = 1;
	dec_online.flust_width = path->out_size.w;
	dec_online.flush_hblank_num = dec_online.hor_padding_num + 20;
	dec_online.flush_line_num = dec_online.ver_padding_num + 20;
	hw->dcam_ioctl(hw, DCAM_HW_CFG_DEC_ONLINE, &dec_online);

	dec_store.idx = idx;
	dec_store.bypass = 1;
	dec_store.endian = path->endian.y_endian;
	if (path->out_fmt == DCAM_STORE_FRGB)
		dec_store.color_format = 0;
	else if (path->out_fmt == DCAM_STORE_YUV422)
		dec_store.color_format = 1;
	else if (path->out_fmt == DCAM_STORE_YVU422)
		dec_store.color_format = 2;
	else if (path->out_fmt == DCAM_STORE_YUV420)
		dec_store.color_format = 4;
	else if (path->out_fmt == DCAM_STORE_YVU420)
		dec_store.color_format = 5;
	dec_store.border_up = 0;
	dec_store.border_down = 0;
	dec_store.border_left = 0;
	dec_store.border_right = 0;
	/*because hw limit, pyr output 10bit*/
	dec_store.data_10b = 1;
	dec_store.flip_en = 0;
	dec_store.last_frm_en = 1;
	dec_store.mirror_en = 0;
	dec_store.mono_en = 0;
	dec_store.speed2x = 1;
	dec_store.rd_ctrl = 0;
	dec_store.store_res = 0;
	dec_store.burst_len = 1;

	pr_debug("dcam %d padding w %d h %d alignw %d h%d\n", idx,
		dec_online.hor_padding_num, dec_online.ver_padding_num, align_w, align_h);
	for (i = 0; i < layer_num; i++) {
		dec_store.bypass = 0;
		dec_store.cur_layer = i;
		dec_store.width = path->dec_store_info.size_t[i].w;
		dec_store.height = path->dec_store_info.size_t[i].h;
		dec_store.pitch[0] = path->dec_store_info.pitch_t[i].pitch_ch0;
		dec_store.pitch[1] = path->dec_store_info.pitch_t[i].pitch_ch1;
		/* when zoom, if necessary size update may set with path size udapte
		 thus, the dec_store need remember on path or ctx, and calc & reg set
		 need separate too, now just */
		hw->dcam_ioctl(hw, DCAM_HW_CFG_DEC_SIZE_UPDATE, &dec_store);

		pr_debug("dcam %d dec_layer %d w %d h %d\n", idx, i, dec_store.width, dec_store.height);
	}

	return ret;
}

static int dcampath_update_pyr_dec_addr(struct dcam_sw_context *ctx, struct dcam_path_desc *path,
			struct camera_frame *frame, uint32_t idx)
{
	int ret = 0, i = 0;
	uint32_t layer_num = 0;
	uint32_t offset = 0, align = 1, size = 0;
	uint32_t align_w = 0, align_h = 0;
	struct cam_hw_info *hw = NULL;
	struct dcam_hw_dec_store_cfg *dec_store = NULL;

	if (!path || !frame) {
		pr_err("fail to check param, path%px, frame%px\n", path, frame);
		return -EINVAL;
	}

	dec_store = &path->dec_store_info;
	hw = ctx->dev->hw;
	dec_store->idx = idx;

	layer_num = DCAM_PYR_DEC_LAYER_NUM;
	/* update layer num based on img size */
	while (isp_rec_small_layer_w(path->out_size.w, layer_num) < MIN_PYR_WIDTH ||
		isp_rec_small_layer_h(path->out_size.h, layer_num) < MIN_PYR_HEIGHT) {
		pr_debug("layer num need decrease based on small input %d %d\n",
			path->out_size.w, path->out_size.h);
		layer_num--;
		dec_store->layer_num = layer_num;
		hw->dcam_ioctl(hw, DCAM_HW_BYPASS_DEC, dec_store);
		if (layer_num == 0)
			break;
	}
	if (!layer_num)
		frame->need_pyr_rec = 0;
	else
		frame->need_pyr_rec = 1;

	align_w = dcampath_dec_align_width(path->out_size.w, layer_num);
	align_h = dcampath_dec_align_heigh(path->out_size.h, layer_num);
	size = path->out_pitch * path->out_size.h;
	dec_store->layer_num = layer_num;
	dec_store->align_w = align_w;
	dec_store->align_h = align_h;

	pr_debug("dcam %d out pitch %d addr %x\n", dec_store->idx, path->out_pitch, frame->buf.iova[0]);
	for (i = 0; i < layer_num; i++) {
		align = align * 2;
		if (i == 0 && frame->is_compressed)
			offset += frame->fbc_info.buffer_size;
		else
			offset += (size * 3 / 2);
		dec_store->cur_layer = i;
		dec_store->size_t[i].w = align_w / align;
		dec_store->size_t[i].h = align_h / align;
		dec_store->pitch_t[i].pitch_ch0 = cal_sprd_yuv_pitch(dec_store->size_t[i].w, path->pyr_data_bits, path->pyr_is_pack);
		dec_store->pitch_t[i].pitch_ch1 = dec_store->pitch_t[i].pitch_ch0;
		size = dec_store->pitch_t[i].pitch_ch0 * dec_store->size_t[i].h;
		dec_store->addr[0] = frame->buf.iova[0] + offset;
		dec_store->addr[1] = dec_store->addr[0] + size;
		hw->dcam_ioctl(hw, DCAM_HW_CFG_DEC_STORE_ADDR, dec_store);

		pr_debug("dcam %d dec_layer %d w %d h %d\n", dec_store->idx, i, dec_store->size_t[i].w, dec_store->size_t[i].h);
		pr_debug("dcam %d dec_layer %d w %x h %x\n", dec_store->idx, i, dec_store->addr[0], dec_store->addr[1]);
	}

	return ret;
}

void dcampath_update_statis_head(struct dcam_sw_context *sw_ctx, struct dcam_path_desc *path, struct camera_frame *frame)
{
	struct dcam_dev_param *blk_dcam_pm;
	unsigned long flags = 0;
	struct cam_hw_info *hw = NULL;

	hw = sw_ctx->dev->hw;
	blk_dcam_pm = &sw_ctx->ctx[sw_ctx->cur_ctx_id].blk_pm;

	/* Re-config aem win if it is updated */
	if (path->path_id == DCAM_PATH_AEM && (!sw_ctx->offline || sw_ctx->rps)) {
		struct dcam_dev_aem_win *win;
		struct sprd_img_rect *zoom_rect;

		spin_lock_irqsave(&path->size_lock, flags);
		dcam_k_aem_win(blk_dcam_pm);

		if (frame->buf.addr_k[0]) {
			win = (struct dcam_dev_aem_win *)(frame->buf.addr_k[0]);
			pr_debug("kaddr %lx\n", frame->buf.addr_k[0]);
			memcpy(win,
				&blk_dcam_pm->aem.win_info,
				sizeof(struct dcam_dev_aem_win));
			win++;
			zoom_rect = (struct sprd_img_rect *)win;
			zoom_rect->x = sw_ctx->next_roi.start_x;
			zoom_rect->y = sw_ctx->next_roi.start_y;
			zoom_rect->w = sw_ctx->next_roi.size_x;
			zoom_rect->h = sw_ctx->next_roi.size_y;
		}
		spin_unlock_irqrestore(&path->size_lock, flags);
	}

	/* Re-config hist win if it is updated */
	if (path->path_id == DCAM_PATH_HIST) {
		spin_lock_irqsave(&path->size_lock, flags);
		hw->dcam_ioctl(hw, DCAM_HW_CFG_HIST_ROI_UPDATE, blk_dcam_pm);
		if (frame->buf.addr_k[0]) {
			struct dcam_dev_hist_info *info = NULL;
			info = (struct dcam_dev_hist_info *)frame->buf.addr_k[0];
			memcpy(info, &blk_dcam_pm->hist.bayerHist_info,
				sizeof(struct dcam_dev_hist_info));
		}
		spin_unlock_irqrestore(&path->size_lock, flags);
	}

}

void dcampath_update_addr_and_size(struct dcam_sw_context *ctx, struct dcam_path_desc *path,
			struct camera_frame *frame, struct dcam_hw_slw_fmcu_cmds *slw, uint32_t idx)
{
	struct cam_hw_info *hw = NULL;
	uint32_t path_id = 0;
	struct dcam_hw_path_size path_size = {0};
	unsigned long flags = 0;
	int layer_num = ISP_PYR_DEC_LAYER_NUM;
	struct dcam_compress_info fbc_info = {0};
	struct dcam_hw_fbc_addr fbcadr = {0};
	struct dcam_hw_cfg_store_addr store_arg = {0};
	struct dcam_dev_param *blk_dcam_pm = NULL;

	hw = ctx->dev->hw;
	path_id = path->path_id;
	blk_dcam_pm = &ctx->ctx[ctx->cur_ctx_id].blk_pm;

	spin_lock_irqsave(&path->size_lock, flags);
	if (slw == NULL) {
		if (frame->is_compressed) {
			struct compressed_addr fbc_addr = {0};
			struct img_size *size = &path->out_size;
			struct dcam_compress_cal_para cal_fbc = {0};

			cal_fbc.compress_4bit_bypass = frame->compress_4bit_bypass;
			cal_fbc.data_bits = path->data_bits;
			cal_fbc.fbc_info = &frame->fbc_info;
			cal_fbc.in = frame->buf.iova[0];
			cal_fbc.fmt = path->out_fmt;
			cal_fbc.height = size->h;
			cal_fbc.width = size->w;
			cal_fbc.out = &fbc_addr;
			dcam_if_cal_compressed_addr(&cal_fbc);
			ctx->fbc_info = frame->fbc_info;
			fbc_info = frame->fbc_info;
			fbcadr.idx = idx;
			fbcadr.addr = path->reg_addr;
			fbcadr.fbc_addr = &fbc_addr;
			fbcadr.path_id = path_id;
			fbcadr.data_bits = path->data_bits;
			hw->dcam_ioctl(hw, DCAM_HW_CFG_FBC_ADDR_SET, &fbcadr);
		} else {
			store_arg.idx = idx;
			store_arg.frame_addr[0] = frame->buf.iova[0];
			store_arg.frame_addr[1] = frame->buf.iova[1];
			store_arg.frame_addr[2] = frame->buf.iova[2];
			store_arg.path_id= path_id;
			store_arg.reg_addr = path->reg_addr;
			store_arg.out_fmt = path->out_fmt;
			store_arg.out_size.h = path->out_size.h;
			store_arg.out_size.w = path->out_size.w;
			store_arg.out_pitch = path->out_pitch;
			store_arg.in_fmt = ctx->cap_info.format;
			store_arg.blk_param = blk_dcam_pm;
			hw->dcam_ioctl(hw, DCAM_HW_CFG_STORE_ADDR, &store_arg);
		}

		if ((path_id == DCAM_PATH_BIN) && (frame->pyr_status == ONLINE_DEC_ON))
			dcampath_update_pyr_dec_addr(ctx, path, frame, idx);
	}

	if (!frame->is_reserved || path_id == DCAM_PATH_FULL || path_id == DCAM_PATH_RAW || slw != NULL || (path_id == DCAM_PATH_3DNR)) {
		if ((path_id == DCAM_PATH_FULL) || (path_id == DCAM_PATH_BIN) ||
			(path_id == DCAM_PATH_3DNR) || (path_id == DCAM_PATH_RAW)) {
			if ((path_id == DCAM_PATH_BIN) && (frame->pyr_status == ONLINE_DEC_ON))
				dcampath_pyr_dec_cfg(path, frame, hw, idx);
			if (path->size_update) {
				path_size.idx = idx;
				path_size.auto_cpy_id = ctx->auto_cpy_id;
				path_size.size_x = ctx->cap_info.cap_size.size_x;
				path_size.size_y = ctx->cap_info.cap_size.size_y;
				path_size.path_id = path->path_id;
				path_size.src_sel = path->src_sel;
				path_size.bin_ratio = path->bin_ratio;
				path_size.scaler_sel = path->scaler_sel;
				path_size.rds_coeff_size = path->rds_coeff_size;
				path_size.rds_coeff_buf = path->rds_coeff_buf;
				path_size.in_size = path->in_size;
				path_size.in_trim = path->in_trim;
				path_size.out_size = path->out_size;
				path_size.out_pitch= path->out_pitch;
				path_size.rds_init_phase_int0 = path->gphase.rds_init_phase_int0;
				path_size.rds_init_phase_int1 = path->gphase.rds_init_phase_int1;
				path_size.rds_init_phase_rdm0 = path->gphase.rds_init_phase_rdm0;
				path_size.rds_init_phase_rdm1 = path->gphase.rds_init_phase_rdm1;
				path_size.compress_info = frame->fbc_info;
				path_size.scaler_info = &path->scaler_info;
				frame->param_data = path->priv_size_data;
				hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_SIZE_UPDATE, &path_size);

				path->size_update = 0;
				path->priv_size_data = NULL;

				if (path_id == DCAM_PATH_BIN) {
					ctx->next_roi = path->in_trim;
					ctx->zoom_ratio = ZOOM_RATIO_DEFAULT * path->zoom_ratio_base.w / path->in_trim.size_x;
					ctx->total_zoom = ZOOM_RATIO_DEFAULT *path->in_size.w / path->total_in_trim.size_x;
					pr_debug("total_zoom: %d (%d %d), zoom_ratio: %d (%d %d)\n",
						ctx->total_zoom, path->in_size.w, path->total_in_trim.size_x,
						ctx->zoom_ratio, path->zoom_ratio_base.w, path->in_trim.size_x);
				}
			}
		}
	}

	if (path_id == DCAM_PATH_FULL && path->base_update) {
		struct dcam_hw_path_src_sel patharg;
		patharg.idx = idx;
		patharg.src_sel = path->src_sel;
		patharg.pack_bits = path->pack_bits;
		hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_SRC_SEL, &patharg);
	}
	path->base_update = 0;
	if (path_id == DCAM_PATH_FULL || path_id == DCAM_PATH_RAW || path_id == DCAM_PATH_BIN) {
		frame->width = path->out_size.w;
		frame->height = path->out_size.h;
	}
	if (path_id == DCAM_PATH_FULL && frame->pyr_status == OFFLINE_DEC_ON) {
		while (isp_rec_small_layer_w(path->out_size.w, layer_num) < MIN_PYR_WIDTH ||
			isp_rec_small_layer_h(path->out_size.h, layer_num) < MIN_PYR_HEIGHT) {
			pr_debug("layer num need decrease based on small input %d %d\n",
				path->out_size.w, path->out_size.h);
			if (--layer_num == 0)
				break;
		}
		if (!layer_num) {
			frame->need_pyr_dec = 0;
			frame->need_pyr_rec = 0;
		} else
			frame->need_pyr_dec = 1;
	}
	frame->zoom_ratio = ctx->zoom_ratio;
	frame->total_zoom = ctx->total_zoom;
	spin_unlock_irqrestore(&path->size_lock, flags);
}

int dcam_path_fmcu_slw_store_buf_set(
		struct dcam_path_desc *path,
		struct dcam_sw_context *sw_ctx,
		struct dcam_hw_slw_fmcu_cmds *slw, int slw_idx)
{
	int ret = 0, path_id = 0;
	struct camera_frame *out_frame = NULL;
	struct cam_hw_info *hw = NULL;
	uint32_t idx = 0;

	if (!path || !sw_ctx) {
		pr_err("fail to check param, path%px, sw_ctx %px\n", path, sw_ctx);
		return -EINVAL;
	}

	hw = sw_ctx->dev->hw;
	idx = sw_ctx->hw_ctx_id;
	path_id = path->path_id;

	if (idx >= DCAM_HW_CONTEXT_MAX)
		return 0;

	if (atomic_read(&path->user_cnt) < 1)
		return ret;

	if ((path->path_id <  DCAM_PATH_AEM) || (slw_idx == 0)) {
		out_frame = dcam_path_frame_cycle(sw_ctx, path);
		if (path->path_id == DCAM_PATH_BIN)
			pr_debug("enqueue bin path buffer, no.%d, cnt %d\n", slw_idx, path->result_queue.cnt);
	} else {
		out_frame = cam_queue_dequeue(&path->reserved_buf_queue, struct camera_frame, list);

		if (out_frame == NULL) {
			pr_err("fail to get reserve buffer, path %d\n", path_id);
			return -1;
		}
		ret = cam_queue_enqueue(&path->result_queue, &out_frame->list);
		if (ret) {
			if (out_frame->is_reserved)
				cam_queue_enqueue(&path->reserved_buf_queue, &out_frame->list);
			else {
				cam_queue_enqueue(&path->out_buf_queue, &out_frame->list);
			}
			return -EINVAL;
		}
		if (path->path_id == DCAM_PATH_BIN)
			pr_debug("enqueue bin path buffer, no.%d\n", slw_idx);

	}
	if (IS_ERR(out_frame))
		return PTR_ERR(out_frame);
	atomic_inc(&path->set_frm_cnt);

	out_frame->fid = sw_ctx->base_fid + sw_ctx->index_to_set + slw_idx;
	out_frame->need_pyr_dec = 0;
	out_frame->need_pyr_rec = 0;
	out_frame->pyr_status = PYR_OFF;

	slw->store_info[path_id].reg_addr = *(hw->ip_dcam[0]->store_addr_tab + path_id);
	if (out_frame->is_compressed) {
		struct compressed_addr fbc_addr;
		struct img_size *size = &path->out_size;
		struct dcam_compress_cal_para cal_fbc;

		cal_fbc.compress_4bit_bypass = out_frame->compress_4bit_bypass;
		cal_fbc.data_bits = path->data_bits;
		cal_fbc.fbc_info = &out_frame->fbc_info;
		cal_fbc.in = out_frame->buf.iova[0];
		cal_fbc.fmt = path->out_fmt;
		cal_fbc.height = size->h;
		cal_fbc.width = size->w;
		cal_fbc.out = &fbc_addr;
		dcam_if_cal_compressed_addr(&cal_fbc);
		slw->store_info[path_id].is_compressed = out_frame->is_compressed;
		slw->store_info[path_id].store_addr.addr_ch0 = fbc_addr.addr0;
		slw->store_info[path_id].store_addr.addr_ch1 = fbc_addr.addr1;
		slw->store_info[path_id].store_addr.addr_ch2 = fbc_addr.addr2;
	} else {
		slw->store_info[path_id].store_addr.addr_ch0 = out_frame->buf.iova[0];

		if (path->out_fmt & DCAM_STORE_YUV_BASE)
			out_frame->buf.iova[1] = out_frame->buf.iova[0] + path->out_pitch * path->out_size.h;

		if (path_id == DCAM_PATH_AFL)
			out_frame->buf.iova[1] = out_frame->buf.iova[0] + hw->ip_dcam[idx]->afl_gbuf_size;

		slw->store_info[path_id].store_addr.addr_ch1 = out_frame->buf.iova[1];
		slw->store_info[path_id].store_addr.addr_ch2 = out_frame->buf.iova[2];
	}

	dcampath_update_addr_and_size(sw_ctx, path, out_frame, slw, idx);
	dcampath_update_statis_head(sw_ctx, path, out_frame);

	pr_debug("path%d set no.%d buffer done!pitch:%d.\n", path_id, slw_idx, path->out_pitch);

	return ret;
}

int dcam_path_fmcu_slw_queue_set(struct dcam_sw_context *sw_ctx)
{
	struct cam_hw_info *hw = NULL;
	struct dcam_path_desc *path = NULL;
	int i = 0, j = 0;
	struct dcam_hw_slw_fmcu_cmds slw;
	struct dcam_fmcu_ctx_desc *fmcu = NULL;
	int ret = 0;
	struct dcam_hw_context *hw_ctx = NULL;

	hw = sw_ctx->dev->hw;
	hw_ctx = sw_ctx->hw_ctx;
	if (!sw_ctx || !hw || !hw_ctx)
		pr_err("fail to check param sw_ctx %px, hw %px, hw_ctx %px\n", sw_ctx, hw, hw_ctx);

	memset(&slw, 0, sizeof(struct dcam_hw_slw_fmcu_cmds));
	for (j = 0; j < sw_ctx->slowmotion_count; j++) {
		for (i = 0; i < DCAM_PATH_MAX; i++) {
			path = &sw_ctx->path[i];
			if (atomic_read(&path->user_cnt) < 1 || atomic_read(&path->is_shutoff) > 0)
				continue;
			/* TODO: frm_deci and frm_skip in slow motion */
			path->frm_cnt++;
			if (path->frm_cnt <= path->frm_skip)
				continue;

			/* @frm_deci is the frame index of output frame */
			if ((path->frm_deci_cnt++ >= path->frm_deci)) {
				path->frm_deci_cnt = 0;
				pr_debug("set slow buffer, no.%d frame, path %d\n", j, i);
				dcam_path_fmcu_slw_store_buf_set(path, sw_ctx, &slw, j);
			}
		}
		fmcu = hw_ctx->fmcu;
		if (!fmcu)
			pr_err("fail to check param fmcu%px\n", fmcu);
		slw.fmcu_handle = fmcu;
		slw.ctx_id = sw_ctx->hw_ctx_id;
		slw.slw_id = j;
		slw.slw_cnt = sw_ctx->slowmotion_count;
		if ((sw_ctx->index_to_set == 0) && (j == 0))
			slw.is_first_cycle = 1;
		else
			slw.is_first_cycle = 0;
		ret = hw->dcam_ioctl(hw, DCAM_HW_CFG_SLW_FMCU_CMDS, &slw);
		if (ret)
			pr_err("fail to prepare %d slw cmd\n", dcam_path_name_get(i));
	}

	return ret;
}

void dcampath_check_path_status(struct dcam_sw_context *dcam_sw_ctx, struct dcam_path_desc *path)
{
	struct dcam_dev_param *blk_dcam_pm = NULL;
	struct camera_frame *frame = NULL;
	int ret = 0, recycle = 0;

	if (unlikely(!dcam_sw_ctx || !path))
		return;

	blk_dcam_pm = &dcam_sw_ctx->ctx[dcam_sw_ctx->cur_ctx_id].blk_pm;

	if (blk_dcam_pm->gtm[DCAM_GTM_PARAM_PRE].gtm_info.bypass_info.gtm_hist_stat_bypass
		&& (blk_dcam_pm->gtm[DCAM_GTM_PARAM_PRE].gtm_info.bypass_info.gtm_mod_en == 0))
		recycle = 1;
	if (blk_dcam_pm->rgb_gtm[DCAM_GTM_PARAM_PRE].rgb_gtm_info.bypass_info.gtm_hist_stat_bypass
		&& (blk_dcam_pm->rgb_gtm[DCAM_GTM_PARAM_PRE].rgb_gtm_info.bypass_info.gtm_mod_en == 0))
		recycle = 1;
	if (recycle) {
		pr_debug("dcam sw%d gtm bypass, no need buf\n", dcam_sw_ctx->sw_ctx_id);
		frame = cam_queue_del_tail(&path->result_queue, struct camera_frame, list);
		if (frame) {
			ret = cam_queue_enqueue(&path->out_buf_queue, &frame->list);
			if (ret)
				pr_err("fail to enqueue gtm\n");
		}
	}
}

int dcam_path_store_frm_set(void *dcam_ctx_handle, struct dcam_path_desc *path)
{
	struct dcam_sw_context *dcam_sw_ctx = (struct dcam_sw_context *)dcam_ctx_handle;
	struct dcam_dev_param *blk_dcam_pm;
	struct cam_hw_info *hw = NULL;
	struct camera_frame *frame = NULL, *saved = NULL;
	uint32_t idx = 0, path_id = 0;
	unsigned long addr = 0, ppe_addr = 0;
	const int _bin = 0, _aem = 1, _hist = 2;
	int i = 0, ret = 0;
	uint32_t slm_path = 0;

	if (unlikely(!dcam_ctx_handle || !path))
		return -EINVAL;

	dcam_sw_ctx = (struct dcam_sw_context *)dcam_ctx_handle;
	blk_dcam_pm = &dcam_sw_ctx->ctx[dcam_sw_ctx->cur_ctx_id].blk_pm;
	hw = dcam_sw_ctx->dev->hw;
	idx = dcam_sw_ctx->hw_ctx_id;
	path_id = path->path_id;

	if (idx >= DCAM_HW_CONTEXT_MAX)
		return 0;

	dcam_sw_ctx->auto_cpy_id |= *(hw->ip_dcam[idx]->path_ctrl_id_tab + path_id);
	if (dcam_sw_ctx->cap_info.format == DCAM_CAP_MODE_YUV)
		dcam_sw_ctx->auto_cpy_id |= *(hw->ip_dcam[idx]->path_ctrl_id_tab + DCAM_PATH_BIN);

	if (path_id == DCAM_PATH_GTM_HIST)
		dcampath_check_path_status(dcam_sw_ctx, path);

	frame = dcam_path_frame_cycle(dcam_sw_ctx, path);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	if (path_id == DCAM_PATH_GTM_HIST) {
		atomic_inc(&path->set_frm_cnt);
		return 0;
	}

	/* assign last buffer for AEM and HIST in slow motion */
	i = dcam_sw_ctx->slowmotion_count - 1;

	if (dcam_sw_ctx->slowmotion_count && path_id == DCAM_PATH_AEM) {
		/* slow motion AEM */
		addr = slowmotion_store_addr[_aem][i];
		frame->fid += i;
	} else if (dcam_sw_ctx->slowmotion_count && path_id == DCAM_PATH_HIST) {
		/* slow motion HIST */
		addr = slowmotion_store_addr[_hist][i];
		frame->fid += i;
	} else {
		/* normal scene */
		addr = *(hw->ip_dcam[idx]->store_addr_tab + path_id);
	}

	if (addr == 0L) {
		pr_info("DCAM%d invalid path id %d, path name %s\n",
			idx, path_id, dcam_path_name_get(path_id));
		return 0;
	}

	if (saved)
		dcampath_frame_pointer_swap(&frame, &saved);

	if (saved)
		dcampath_frame_pointer_swap(&frame, &saved);

	atomic_inc(&path->set_frm_cnt);
	if (path_id == DCAM_PATH_AFL)
		DCAM_REG_WR(idx, ISP_AFL_REGION_WADDR,
			frame->buf.iova[0] + hw->ip_dcam[idx]->afl_gbuf_size);

	if (!blk_dcam_pm->pdaf.bypass &&
		blk_dcam_pm->pdaf.pdaf_type == DCAM_PDAF_TYPE3 && path_id == DCAM_PATH_PDAF) {
		/* PDAF type3, half buffer for right PD, TBD */
		ppe_addr = hw->ip_dcam[idx]->pdaf_type3_reg_addr;
		DCAM_REG_WR(idx, ppe_addr,
			frame->buf.iova[0] + frame->buf.size[0] / 2);
		pr_debug("reg %08x,  PDAF iova %08x,  offset %x\n", (uint32_t)ppe_addr, (uint32_t)frame->buf.iova[0],
			(uint32_t)frame->buf.size[0] / 2);
	}

	pr_debug("DCAM%u %s, fid %u, count %d, path->out_size.w %d, is_reserver %d, channel_id %d, reg %08x, addr %08x\n",
		idx, dcam_path_name_get(path_id), frame->fid, atomic_read(&path->set_frm_cnt), path->out_size.w,
		frame->is_reserved, frame->channel_id, (uint32_t)addr, (uint32_t)frame->buf.iova[0]);

	path->reg_addr = addr;

	dcampath_update_addr_and_size(dcam_sw_ctx, path, frame, NULL, idx);

	dcampath_update_statis_head(dcam_sw_ctx, path, frame);

	slm_path = hw->ip_dcam[idx]->slm_path;
	if (dcam_sw_ctx->slowmotion_count && !dcam_sw_ctx->index_to_set &&
		(path_id == DCAM_PATH_AEM || path_id == DCAM_PATH_HIST)
		&& (slm_path & BIT(path_id))) {
		/* configure reserved buffer for AEM and hist */
		frame = cam_queue_dequeue(&path->reserved_buf_queue,
			struct camera_frame, list);
		if (!frame) {
			pr_debug("DCAM%u %s buffer unavailable\n", idx, dcam_path_name_get(path_id));
			return -ENOMEM;
		}

		i = 0;
		while (i < dcam_sw_ctx->slowmotion_count - 1) {
			if (path_id == DCAM_PATH_AEM)
				addr = slowmotion_store_addr[_aem][i];
			else
				addr = slowmotion_store_addr[_hist][i];
			DCAM_REG_WR(idx, addr, frame->buf.iova[0]);

			pr_debug("DCAM%u %s set reserved frame\n", idx,
				 dcam_path_name_get(path_id));
			i++;
		}

		/* put it back */
		cam_queue_enqueue(&path->reserved_buf_queue, &frame->list);
	} else if (dcam_sw_ctx->slowmotion_count && path_id == DCAM_PATH_BIN) {
		i = 1;
		while (i < dcam_sw_ctx->slowmotion_count) {
			frame = dcam_path_frame_cycle(dcam_sw_ctx, path);
			/* in init phase, return failure if error happens */
			if (IS_ERR(frame) && !dcam_sw_ctx->index_to_set) {
				ret = PTR_ERR(frame);
				goto enqueue_reserved;
			}

			/* in normal running, just stop configure */
			if (IS_ERR(frame))
				break;

			addr = slowmotion_store_addr[_bin][i];
			if (saved)
				dcampath_frame_pointer_swap(&frame, &saved);
			DCAM_REG_WR(idx, addr, frame->buf.iova[0]);
			if (saved)
				dcampath_frame_pointer_swap(&frame, &saved);
			atomic_inc(&path->set_frm_cnt);

			frame->fid = dcam_sw_ctx->base_fid + dcam_sw_ctx->index_to_set + i;

			pr_debug("DCAM%u BIN set frame: fid %u, count %d\n",
				idx, frame->fid, atomic_read(&path->set_frm_cnt));
			i++;
		}

		if (unlikely(i != dcam_sw_ctx->slowmotion_count))
			pr_warn("warning: DCAM%u BIN %d frame missed\n",
				idx, dcam_sw_ctx->slowmotion_count - i);
	}

enqueue_reserved:
	if (saved)
		cam_queue_enqueue(&path->reserved_buf_queue, &saved->list);

	return ret;
}
