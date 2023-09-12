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

#include <linux/uaccess.h>
#include <sprd_mm.h>
#include "isp_hw.h"

#include "dcam_reg.h"
#include "dcam_interface.h"
#include "cam_types.h"
#include "cam_block.h"
#include "dcam_core.h"


#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "GTM: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

void dcam_k_raw_gtm_set_default(struct dcam_dev_raw_gtm_block_info *p)
{
	p->gtm_tm_out_bit_depth = 0xE;
	p->gtm_tm_in_bit_depth = 0xE;
	p->gtm_cur_is_first_frame = 0x0;
	p->gtm_log_diff = 0x0;
	p->gtm_log_diff_int = 0x25C9;
	p->gtm_log_max_int = 0x0;
	p->gtm_log_min_int = 0x496B;
	p->gtm_lr_int = 0x23F;
	p->gtm_tm_param_calc_by_hw = 0x1;
	p->gtm_yavg = 0x0;
	p->gtm_ymax = 0x0;
	p->gtm_ymin = 0x4;
	p->tm_lumafilter_c[0][0] = 0x4;
	p->tm_lumafilter_c[0][1] = 0x2;
	p->tm_lumafilter_c[0][2] = 0x4;
	p->tm_lumafilter_c[1][0] = 0x2;
	p->tm_lumafilter_c[1][1] = 0x28;
	p->tm_lumafilter_c[1][2] = 0x2;
	p->tm_lumafilter_c[2][0] = 0x4;
	p->tm_lumafilter_c[2][1] = 0x2;
	p->tm_lumafilter_c[2][2] = 0x4;
	p->tm_lumafilter_shift = 0x6;
	p->slice.gtm_slice_line_startpos = 0x0;
	p->slice.gtm_slice_line_endpos = 0x0;
	p->slice.gtm_slice_main = 0x0;
}

int dcam_k_raw_gtm_slice(uint32_t idx, struct dcam_dev_gtm_slice_info *gtm_slice)
{
	int ret = 0;
	unsigned int val = 0;

	/* for slice */
	val =((gtm_slice->gtm_slice_main & 0x1) << 7);
	DCAM_REG_MWR(idx, DCAM_GTM_GLB_CTRL, 0x80, val);

	val = (gtm_slice->gtm_slice_line_startpos & 0x1FFF);
	DCAM_REG_WR(idx, GTM_SLICE_LINE_STARTPOS, val);

	val = (gtm_slice->gtm_slice_line_endpos & 0x1FFF);
	DCAM_REG_WR(idx, GTM_SLICE_LINE_ENDPOS, val);

	return ret;
}

int dcam_k_raw_gtm_block(uint32_t gtm_param_idx,
	struct dcam_dev_param *param)
{
	int ret = 0;
	unsigned int i = 0;
	uint32_t idx = param->idx;
	unsigned int val = 0;
	struct dcam_dev_raw_gtm_block_info *p;
	struct dcam_dev_gtm_slice_info *gtm_slice;
	struct dcam_sw_context *sw_ctx = NULL;
	struct dcam_dev_gtm_param *gtm = NULL;

	gtm = &param->gtm[gtm_param_idx];
	if (!gtm->update_en)
		return 0;

	sw_ctx = (struct dcam_sw_context *)param->dev;
	p = &(gtm->gtm_info);
	dcam_k_raw_gtm_set_default(p);

	if (g_dcam_bypass[idx] & (1 << _E_GTM)) {
		pr_debug("dcam gtm bypass  idx%d, \n", idx);
		p->bypass_info.gtm_mod_en = 0;
	}
	gtm_slice = &(p->slice);
	DCAM_REG_MWR(idx, DCAM_GTM_GLB_CTRL, BIT_0, (p->bypass_info.gtm_mod_en & 0x1));
	if (!p->bypass_info.gtm_mod_en) {
		pr_debug("dcam gtm disable, idx%d\n", idx);
		return 0;
	}

	if (gtm->gtm_calc_mode == GTM_SW_CALC) {
		p->gtm_cur_is_first_frame = 1;
		pr_info("gtm_sw_calc first frame need gtm map\n");
	} else if (atomic_read(&sw_ctx->state) != STATE_RUNNING || sw_ctx->frame_index == DCAM_FRAME_INDEX_MAX) {
		pr_debug("sw_ctx->state %d offline_gtm_bypass_status %d\n", atomic_read(&sw_ctx->state), sw_ctx->frame_index);
		p->gtm_cur_is_first_frame = 1;
		p->bypass_info.gtm_map_bypass = 1;
	}

	val = ((p->bypass_info.gtm_map_bypass & 0x1) << 1) |
		((p->bypass_info.gtm_hist_stat_bypass & 0x1) << 2) |
		((p->gtm_tm_param_calc_by_hw & 0x1) << 3) |
		((p->gtm_cur_is_first_frame &0x1) << 4) |
		((p->gtm_tm_luma_est_mode & 0x3) << 5) |
		((p->gtm_tm_in_bit_depth & 0xF) << 24) |
		((p->gtm_tm_out_bit_depth & 0xF) << 28);
	DCAM_REG_MWR(idx, DCAM_GTM_GLB_CTRL, 0xFF00007E, val);

	val = (p->gtm_imgkey_setting_mode & 0x1) | ((p->gtm_imgkey_setting_value & 0x7FFF) << 4);
	DCAM_REG_MWR(idx, GTM_HIST_CTRL0, 0x7FFF1, val);

	val = (p->gtm_target_norm_setting_mode & 0x1)
		| ((p->gtm_target_norm & 0x3FFF) << 2)
		| ((p->gtm_target_norm_coeff & 0x3FFF) << 16);
	DCAM_REG_MWR(idx, GTM_HIST_CTRL1, 0x3FFFFFFD, val);

	val = p->gtm_yavg_diff_thr & 0x3FFF;
	DCAM_REG_WR(idx, GTM_HIST_CTRL2, val);

	p->gtm_hist_total = sw_ctx->cap_info.cap_size.size_x * sw_ctx->cap_info.cap_size.size_y;
	val = p->gtm_hist_total & 0x3FFFFFF;
	DCAM_REG_WR(idx, GTM_HIST_CTRL5, val);

	val = ((p->gtm_min_per * p->gtm_hist_total) >> 16) & 0xFFFFF;
	DCAM_REG_WR(idx, GTM_HIST_CTRL6, val);

	val = ((p->gtm_max_per * p->gtm_hist_total) >> 16) & 0xFFFFF;
	DCAM_REG_WR(idx, GTM_HIST_CTRL7, val);

	val = p->gtm_log_diff & 0x1FFFFFFF;
	DCAM_REG_WR(idx, GTM_LOG_DIFF, val);

	val = (p->gtm_ymax_diff_thr & 0x3FFF)
		| ((p->gtm_cur_ymin_weight & 0x1FF) << 14)
		| ((p->gtm_pre_ymin_weight & 0x1FF) << 23);
	DCAM_REG_WR(idx, GTM_TM_YMIN_SMOOTH, val);

	val = (p->tm_lumafilter_c[0][0] & 0xFF)
		| ((p->tm_lumafilter_c[0][1] & 0xFF) << 8)
		| ((p->tm_lumafilter_c[0][2] & 0xFF) << 16)
		| ((p->tm_lumafilter_c[1][0] & 0xFF) << 24);
	DCAM_REG_WR(idx, GTM_TM_LUMAFILTER0, val);

	val = (p->tm_lumafilter_c[1][1] & 0xFF)
		| ((p->tm_lumafilter_c[1][2] & 0xFF) << 8)
		| ((p->tm_lumafilter_c[2][0] & 0xFF) << 16)
		| ((p->tm_lumafilter_c[2][1] & 0xFF) << 24);
	DCAM_REG_WR(idx, GTM_TM_LUMAFILTER1, val);

	val = (p->tm_lumafilter_c[2][2] & 0xFF) | ((p->tm_lumafilter_shift & 0xF) << 28);
	DCAM_REG_MWR(idx, GTM_TM_LUMAFILTER2, 0xF00000FF, val);

	val = (p->tm_rgb2y_r_coeff & 0x7FF) | ((p->tm_rgb2y_g_coeff & 0x7FF) << 16);
	DCAM_REG_MWR(idx, GTM_TM_RGB2YCOEFF0, 0x7FF07FF, val);

	val = (p->tm_rgb2y_b_coeff & 0x7FF);
	DCAM_REG_MWR(idx, GTM_TM_RGB2YCOEFF1, 0x7FF, val);

	for (i = 0; i < GTM_HIST_XPTS_CNT / 2; i += 2) {
		val = ((p->tm_hist_xpts[i] & 0x3FFF) << 16) | (p->tm_hist_xpts[i + 1] & 0x3FFF);
		DCAM_REG_WR(idx, GTM_HIST_XPTS + i * 2, val);
	}

	/* for slice */
	dcam_k_raw_gtm_slice(idx, gtm_slice);

	return ret;
}

int dcam_k_raw_gtm_mapping(struct cam_gtm_mapping *param)
{
	uint32_t idx = 0;
	uint32_t val = 0;

	if (!param) {
		pr_err("fail to input ptr NULL\n");
		return -1;
	}

	idx = param->idx;

	if (g_dcam_bypass[idx] & (1 << _E_GTM)) {
		pr_debug("gtm mapping disable, idx %d\n", idx);
		return 0;
	}

	if ((!param->ymin) && (!param->target) && (!param->lr_int) && (!param->log_min_int) && (!param->log_diff_int) && (!param->diff)) {
		pr_err("fail to get normal mapping param\n");
		DCAM_REG_MWR(idx, DCAM_GTM_GLB_CTRL, BIT_1, BIT_1);
		return -1;
	}
	val = ((param->ymin & 0xFF) << 0);
	DCAM_REG_MWR(idx, GTM_HIST_YMIN, 0xFF, val);

	val = ((param->target & 0x3FFF) << 2);
	DCAM_REG_MWR(idx, GTM_HIST_CTRL1, 0x3FFF << 2, val);

	val = ((param->lr_int & 0xFFFF) << 0) |
		((param->log_min_int & 0xFFFF) << 16);
	DCAM_REG_MWR(idx, GTM_HIST_CTRL3, 0xFFFFFFFF, val);

	val = ((param->log_diff_int & 0xFFFF) << 16);
	DCAM_REG_MWR(idx,  GTM_HIST_CTRL4, 0xFFFFFFFF, val);

	val = ((param->diff & 0x1FFFFFFF) << 0);
	DCAM_REG_WR(idx, GTM_LOG_DIFF, val);

	DCAM_REG_MWR(idx, DCAM_GTM_GLB_CTRL, BIT_1, 0);
	DCAM_REG_MWR(idx, DCAM_GTM_GLB_CTRL, BIT_3, 0);

	pr_debug("hw ctx %d, hw_ymin %d, target_norm %d, lr_int %d, log_min %d, log_diff %d, log_diff %d\n",
		idx, param->ymin, param->target, param->lr_int, param->log_min_int, param->log_diff_int, param->diff);

	return 0;
}

int dcam_k_gtm_bypass(struct dcam_dev_param *param, struct dcam_dev_raw_gtm_bypass *bypass_info)
{
	uint32_t val = 0;

	if ((!param) || (!bypass_info)) {
		pr_err("fail to input ptr %px %px\n", param, bypass_info);
		return -1;
	}

	if (g_dcam_bypass[param->idx] & (1 << _E_GTM)) {
		pr_debug("gtm mapping disable, idx %d\n", param->idx);
		return 0;
	}

	val = (bypass_info->gtm_mod_en & 1) |
		((bypass_info->gtm_hist_stat_bypass & 1) << 2) |
		((bypass_info->gtm_map_bypass & 1) << 1);
	DCAM_REG_MWR(param->idx, DCAM_GTM_GLB_CTRL, 0x3, val);
	pr_debug("dcam%d mod en %d, hitst bypass %d, map bypass %d\n",
		param->idx, bypass_info->gtm_mod_en, bypass_info->gtm_hist_stat_bypass, bypass_info->gtm_map_bypass);
	return 0;
}

int dcam_k_cfg_raw_gtm(struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;
	uint32_t gtm_param_idx = DCAM_GTM_PARAM_MAX;
	uint32_t *calc_mode = NULL;
	struct dcam_dev_raw_gtm_block_info *gtm_block = NULL;
	struct cam_gtm_mapping *mapping = NULL;
	struct dcam_dev_raw_gtm_bypass *gtm_bypass = NULL;

	switch (param->property) {
	case DCAM_PRO_GTM_BLOCK:
		if (param->scene_id == PM_SCENE_CAP) {
			gtm_param_idx = DCAM_GTM_PARAM_CAP;
			gtm_block = &p->gtm[DCAM_GTM_PARAM_CAP].gtm_info;
		} else if (param->scene_id == PM_SCENE_PRE || param->scene_id == PM_SCENE_VID) {
			gtm_param_idx = DCAM_GTM_PARAM_PRE;
			gtm_block = &p->gtm[DCAM_GTM_PARAM_PRE].gtm_info;
		} else {
			pr_debug("gtm block do not support, scene id %d\n", param->scene_id);
			break;
		}
		/* online mode not need mutex, response faster
		 * Offline need mutex to protect param
		 */
		if (p->offline == 0) {
			ret = copy_from_user((void *)gtm_block, param->property_param, sizeof(struct dcam_dev_raw_gtm_block_info));
			if (ret) {
				pr_err("fail to copy, ret=0x%x\n", (unsigned int)ret);
				return -EPERM;
			}

			if (p->idx == DCAM_HW_CONTEXT_MAX)
				return 0;

			pr_debug("gtm  mod_en %d,  hist_stat_bypass %d, map_bypass %d\n",
				gtm_block->bypass_info.gtm_mod_en, gtm_block->bypass_info.gtm_hist_stat_bypass, gtm_block->bypass_info.gtm_map_bypass);
			dcam_k_raw_gtm_block(gtm_param_idx, p);
		} else {
			mutex_lock(&p->param_lock);
			ret = copy_from_user((void *)gtm_block, param->property_param, sizeof(struct dcam_dev_raw_gtm_block_info));
			if (ret) {
				mutex_unlock(&p->param_lock);
				pr_err("fail to copy, ret=0x%x\n", (unsigned int)ret);
				return -EPERM;
			}
			mutex_unlock(&p->param_lock);
		}
		break;
	case DCAM_PRO_GTM_MAPPING:
		if (param->scene_id == PM_SCENE_CAP) {
			mapping = &p->gtm[DCAM_GTM_PARAM_CAP].mapping_info;
		} else if (param->scene_id == PM_SCENE_PRE || param->scene_id == PM_SCENE_VID) {
			mapping = &p->gtm[DCAM_GTM_PARAM_PRE].mapping_info;
		} else {
			pr_debug("gtm mapping do not support, scene id %d\n", param->scene_id);
			break;
		}

		pr_debug("get mapping info, scene_id %d, offline %d\n", param->scene_id, p->offline);

		ret = copy_from_user((void *)mapping, param->property_param, sizeof(struct cam_gtm_mapping));
		if (ret) {
			pr_err("fail to copy, ret=0x%x\n", (unsigned int)ret);
			return -EPERM;
		}
		if (p->idx == DCAM_HW_CONTEXT_MAX)
			return 0;

		mapping->idx = p->idx;
		dcam_k_raw_gtm_mapping(mapping);
		break;
	case DCAM_PRO_GTM_BYPASS:
		if (param->scene_id == PM_SCENE_CAP) {
			gtm_param_idx = DCAM_GTM_PARAM_CAP;
			gtm_bypass = &p->gtm[DCAM_GTM_PARAM_CAP].gtm_info.bypass_info;
		} else if (param->scene_id == PM_SCENE_PRE || param->scene_id == PM_SCENE_VID) {
			gtm_param_idx = DCAM_GTM_PARAM_PRE;
			gtm_bypass = &p->gtm[DCAM_GTM_PARAM_PRE].gtm_info.bypass_info;
		} else {
			pr_debug("gtm block do not support, scene id %d\n", param->scene_id);
			break;
		}

		ret = copy_from_user((void *)gtm_bypass, param->property_param, sizeof(struct dcam_dev_raw_gtm_bypass));
		if (ret) {
			pr_err("fail to copy, ret=0x%x\n", (unsigned int)ret);
			return -EPERM;
		}
		pr_debug("scene%d mod en %d, hitst bypass %d, map bypass %d\n",
			param->scene_id, gtm_bypass->gtm_mod_en, gtm_bypass->gtm_hist_stat_bypass, gtm_bypass->gtm_map_bypass);

		if (p->idx == DCAM_HW_CONTEXT_MAX)
			return 0;
		dcam_k_gtm_bypass(p, gtm_bypass);
		break;
	case DCAM_PRO_GTM_CALC_MODE:
		if (param->scene_id == PM_SCENE_CAP) {
			calc_mode = &p->gtm[DCAM_GTM_PARAM_CAP].gtm_calc_mode;
		} else if (param->scene_id == PM_SCENE_PRE || param->scene_id == PM_SCENE_VID) {
			calc_mode = &p->gtm[DCAM_GTM_PARAM_PRE].gtm_calc_mode;
		} else {
			pr_debug("wrong scene id %d\n", param->scene_id);
			break;
		}

		ret = copy_from_user((void *)calc_mode, param->property_param, sizeof(uint32_t));
		if (ret) {
			pr_err("fail to copy, ret=0x%x\n", (unsigned int)ret);
			return -EPERM;
		}
		pr_debug("gtm calc mode %d\n", *calc_mode);
		break;
	default:
		pr_err("fail cmd id:%d, not supported.\n", param->property);
		break;
	}
	return ret;
}

