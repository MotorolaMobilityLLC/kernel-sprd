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
#include "cam_types.h"
#include "cam_block.h"
#include "isp_gtm.h"
#include "isp_reg.h"
#include "cam_queue.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "GTM: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static void isp_k_raw_gtm_set_default(struct dcam_dev_raw_gtm_block_info *p,
	struct cam_gtm_mapping *map)
{
	p->gtm_tm_out_bit_depth = 0xE;
	p->gtm_tm_in_bit_depth = 0xE;
	p->gtm_cur_is_first_frame = 0x0;
	p->gtm_log_diff = 769156;
	p->gtm_log_diff_int = 698;
	p->gtm_log_max_int = 0x0;
	p->gtm_log_min_int = 32580;
	p->gtm_lr_int = 61517;
	p->gtm_tm_param_calc_by_hw = 0x1;
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
	p->gtm_ymin = 2;
	p->gtm_target_norm = 4015;

	if (map) {
		map->ymin = 2;
		map->ymax = 0;
		map->yavg = 0;
		map->target = 4015;
		map->lr_int = 61517;
		map->log_min_int = 32580;
		map->log_diff_int = 698;
		map->diff = 769156;
	}
}

int isp_k_gtm_mapping_get(void *param)
{
	uint32_t val = 0;
	struct isp_gtm_mapping *mapping = NULL;

	if (!param) {
		pr_err("fail to input ptr NULL\n");
		return -1;
	}

	mapping = (struct isp_gtm_mapping *)param;

	val = ISP_HREG_RD(ISP_GTM_STATUS0);
	mapping->gtm_hw_ymin = val & 0xFF;
	mapping->gtm_hw_ymax = (val >> 16) & 0x3FFF;

	val = ISP_HREG_RD(ISP_GTM_STATUS1);
	mapping->gtm_hw_yavg= val & 0x3FFF;
	mapping->gtm_hw_target_norm = (val >> 16) & 0x3FFF;

	val = ISP_HREG_RD(ISP_GTM_STATUS2);
	mapping->gtm_img_key_value = val & 0x7FF;
	mapping->gtm_hw_lr_int = (val >> 16) & 0xFFFF;

	val = ISP_HREG_RD(ISP_GTM_STATUS3);
	mapping->gtm_hw_log_min_int = val & 0xFFFF;
	mapping->gtm_hw_log_diff_int = (val >> 16) & 0xFFFF;

	val = ISP_HREG_RD(ISP_GTM_STATUS4);
	mapping->gtm_hw_log_diff = val & 0x1FFFFFFF;

	pr_debug("ctx %d,gtm mapping hw_ymin %d, hw_ymax %d, hw_yavg %d, target_norm %d\n",
		mapping->ctx_id, mapping->gtm_hw_ymin, mapping->gtm_hw_ymax,
		mapping->gtm_hw_yavg, mapping->gtm_hw_target_norm);
	pr_debug("ctx %d, gtm img_key_value %d, lr_int %d, log_min_int %d, log_diff_int %d, hw_log_diff %d\n",
		mapping->ctx_id, mapping->gtm_img_key_value, mapping->gtm_hw_lr_int, mapping->gtm_hw_log_min_int,
		mapping->gtm_hw_log_diff_int, mapping->gtm_hw_log_diff);

	return 0;
}

int isp_k_gtm_mapping_set(void *param)
{
	uint32_t val = 0;
	uint32_t idx = 0;
	struct isp_gtm_mapping *mapping = NULL;

	if (!param) {
		pr_err("fail to get input ptr \n");
		return -1;
	}

	mapping = (struct isp_gtm_mapping *)param;
	idx = mapping->ctx_id;
	if (mapping->sw_mode) {
		ISP_REG_MWR(idx, ISP_GTM_HIST_CTRL0, BIT_0, 0);
		ISP_REG_MWR(idx, ISP_GTM_HIST_CTRL1, BIT_0, 0);
		ISP_REG_MWR(idx, ISP_GTM_GLB_CTRL, BIT_3, 0);
		ISP_REG_MWR(idx, ISP_GTM_GLB_CTRL, BIT_4, 1);
	} else {
		ISP_REG_MWR(idx, ISP_GTM_HIST_CTRL0, BIT_0, 1);
		ISP_REG_MWR(idx, ISP_GTM_HIST_CTRL1, BIT_0, 1);
		ISP_REG_MWR(idx, ISP_GTM_GLB_CTRL, BIT_3, BIT_3);
	}

	if (g_isp_bypass[idx] & (1 << _EISP_GTM))
		return 0;

	if ((!mapping->gtm_hw_ymin) && (!mapping->gtm_hw_target_norm) && (!mapping->gtm_hw_lr_int)
		&& (!mapping->gtm_hw_log_min_int) && (!mapping->gtm_hw_log_diff_int) && (!mapping->gtm_hw_log_diff)) {
		pr_err("fail to get normal gtm mapping param\n");
		ISP_REG_MWR(idx, ISP_GTM_GLB_CTRL, BIT_1, BIT_1);
		return -1;
	}
	val = ((mapping->gtm_hw_ymin & 0xFF) << 0);
	ISP_REG_MWR(idx, ISP_GTM_HIST_YMIN, 0xFF, val);

	val = ((mapping->gtm_hw_target_norm & 0x3FFF) << 2);
	ISP_REG_MWR(idx, ISP_GTM_HIST_CTRL1, 0x3FFF << 2, val);

	val = ((mapping->gtm_hw_lr_int & 0xFFFF) << 0) |
		((mapping->gtm_hw_log_min_int & 0xFFFF) << 16);
	ISP_REG_MWR(idx, ISP_GTM_HIST_CTRL3, 0xFFFFFFFF, val);

	val = ((mapping->gtm_hw_log_diff_int & 0xFFFF) << 16);
	ISP_REG_MWR(idx,  ISP_GTM_HIST_CTRL4, 0xFFFF0000, val);

	val = ((mapping->gtm_hw_log_diff & 0x1FFFFFFF) << 0);
	ISP_REG_WR(idx, ISP_GTM_LOG_DIFF, val);

	pr_debug("ctx %d, sw mode %d, gtm hw_ymin %d, target_norm %d, lr_int %d\n",
		idx, mapping->sw_mode, mapping->gtm_hw_ymin, mapping->gtm_hw_target_norm, mapping->gtm_hw_lr_int);
	pr_debug("ctx %d, gtm log_min_int %d, log_diff_int %d, log_diff %d\n",
		idx, mapping->gtm_hw_log_min_int, mapping->gtm_hw_log_diff_int, mapping->gtm_hw_log_diff);

	return 0;
}

int isp_k_gtm_block(void *pctx, void *param, void *param2)
{
	int ret = 0;
	uint32_t idx = 0;
	unsigned int i = 0;
	unsigned int val = 0;
	struct isp_gtm_ctx_desc *ctx = NULL;
	struct dcam_dev_raw_gtm_block_info *p = NULL;
	struct dcam_dev_gtm_slice_info *gtm_slice = NULL;
	struct cam_gtm_mapping *map = NULL;

	if (!pctx || !param) {
		pr_err("fail to get input ptr ctx %p, param %p\n", pctx,  param);
		return -1;
	}

	p = (struct dcam_dev_raw_gtm_block_info *)param;
	gtm_slice = &p->slice;

	ctx = (struct isp_gtm_ctx_desc *)pctx;
	idx = ctx->ctx_id;

	if (g_isp_bypass[idx] & (1 << _EISP_GTM)) {
		pr_debug("ctx_id %d, g_isp_bypass GTM\n", idx);
		p->bypass_info.gtm_mod_en = 0;
	}

	if (ctx->fid == 0)
		map = (struct cam_gtm_mapping *)param2;
	isp_k_raw_gtm_set_default(p, map);

	ISP_REG_MWR(idx, ISP_GTM_GLB_CTRL, BIT_0, (p->bypass_info.gtm_mod_en & 0x1));
	if (p->bypass_info.gtm_mod_en == 0) {
		pr_debug("ctx_id %d, gtm mod_en disable\n", idx);
		ISP_REG_MWR(idx, ISP_GTM_GLB_CTRL, BIT_2 | BIT_1, 3 << 1);
		return 0;
	}
	p->gtm_cur_is_first_frame = 1;

	if (ctx->mode == MODE_GTM_PRE) {
		if (atomic_read(&ctx->cnt) == 0)
			p->gtm_cur_is_first_frame = 1;
		else
			p->gtm_cur_is_first_frame = 0;
		atomic_inc(&ctx->cnt);
	} else {
		p->gtm_cur_is_first_frame = 0;
	}

	pr_debug("ctx_id %d, mod_en %d, map %d, hist_stat %d, calc mode %d\n",
		idx, p->bypass_info.gtm_mod_en, p->bypass_info.gtm_map_bypass, p->bypass_info.gtm_hist_stat_bypass, p->gtm_tm_param_calc_by_hw);

	val = ((p->bypass_info.gtm_map_bypass & 0x1) << 1)
		| ((p->bypass_info.gtm_hist_stat_bypass & 0x1) << 2)
		| ((p->gtm_tm_param_calc_by_hw & 0x1) << 3)
		| ((p->gtm_cur_is_first_frame &0x1) << 4)
		| ((p->gtm_tm_luma_est_mode & 0x3) << 5)
		| ((0 & 0x1) << 21)/*last slice*/
		| ((0 & 0x1) << 22)/*first slice*/
		| ((0 & 0x1) << 23)/*stat slice en*/
		| ((p->gtm_tm_in_bit_depth & 0xF) << 24)
		| ((p->gtm_tm_out_bit_depth & 0xF) << 28);
	ISP_REG_MWR(idx, ISP_GTM_GLB_CTRL, 0xFFE0007E, val);

	val = (p->gtm_imgkey_setting_mode & 0x1)
		| ((p->gtm_imgkey_setting_value & 0x7FFF) << 4);
	ISP_REG_MWR(idx, ISP_GTM_HIST_CTRL0, 0x7FFF1, val);

	val = (p->gtm_target_norm_setting_mode & 0x1)
		| ((p->gtm_target_norm & 0x3FFF) << 2)
		| ((p->gtm_target_norm_coeff & 0x3FFF) << 16);
	ISP_REG_MWR(idx, ISP_GTM_HIST_CTRL1, 0x3FFFFFFD, val);

	val = p->gtm_yavg_diff_thr & 0x3FFF;
	ISP_REG_WR(idx, ISP_GTM_HIST_CTRL2, val);

	val = ((p->gtm_lr_int & 0xFFFF) << 0) |
		((p->gtm_log_min_int & 0xFFFF) << 16);
	ISP_REG_MWR(idx, ISP_GTM_HIST_CTRL3, 0xFFFFFFFF, val);

	val = ((p->gtm_log_diff_int & 0xFFFF) << 16);
	ISP_REG_MWR(idx,  ISP_GTM_HIST_CTRL4, 0xFFFFFFFF, val);

	pr_debug("ctx_id %d, gtm_hist_total w %d, h %d\n", idx, ctx->src.w, ctx->src.h);
	p->gtm_hist_total = ctx->src.w * ctx->src.h;
	val = ((p->gtm_hist_total & 0x3FFFFFF) << 0);
	ISP_REG_WR(idx,  ISP_GTM_HIST_CTRL5, val);

	val = ((p->gtm_min_per * p->gtm_hist_total) >> 16) & 0xFFFFF;
	ISP_REG_WR(idx, ISP_GTM_HIST_CTRL6, val);

	val = ((p->gtm_max_per * p->gtm_hist_total) >> 16) & 0xFFFFF;
	ISP_REG_WR(idx, ISP_GTM_HIST_CTRL7, val);

	val = ((p->gtm_log_diff & 0x1FFFFFFF) << 0);
	ISP_REG_WR(idx, ISP_GTM_LOG_DIFF, val);

	val = (p->gtm_ymax_diff_thr & 0x3FFF)
		| ((p->gtm_cur_ymin_weight & 0x1FF) << 14)
		| ((p->gtm_pre_ymin_weight & 0x1FF) << 23);
	ISP_REG_WR(idx, ISP_GTM_TM_YMIN_SMOOTH, val);

	val = (p->tm_lumafilter_c[0][0] & 0xFF)
		| ((p->tm_lumafilter_c[0][1] & 0xFF) << 8)
		| ((p->tm_lumafilter_c[0][2] & 0xFF) << 16)
		| ((p->tm_lumafilter_c[1][0] & 0xFF) << 24);
	ISP_REG_WR(idx, ISP_GTM_TM_LUMAFILTER0, val);

	val = (p->tm_lumafilter_c[1][1] & 0xFF)
		| ((p->tm_lumafilter_c[1][2] & 0xFF) << 8)
		| ((p->tm_lumafilter_c[2][0] & 0xFF) << 16)
		| ((p->tm_lumafilter_c[2][1] & 0xFF) << 24);
	ISP_REG_WR(idx, ISP_GTM_TM_LUMAFILTER1, val);

	val = (p->tm_lumafilter_c[2][2] & 0xFF)
		| ((p->tm_lumafilter_shift & 0xF) << 28);
	ISP_REG_MWR(idx, ISP_GTM_TM_LUMAFILTER2, 0xF00000FF, val);

	val = ((p->tm_rgb2y_g_coeff & 0x7FF) << 16)|
		((p->tm_rgb2y_r_coeff & 0x7FF) << 0);
	ISP_REG_MWR(idx, ISP_GTM_TM_RGB2TCOEFF0, 0x7FF07FF, val);

	val = ((p->tm_rgb2y_b_coeff & 0x7FF) << 0);
	ISP_REG_MWR(idx, ISP_GTM_TM_RGB2TCOEFF1, 0x7FF, val);

	for (i = 0; i < 128; i+=2) {
		val = ((p->tm_hist_xpts[i] & 0x3FFF) << 16) | (p->tm_hist_xpts[i+1] & 0x3FFF);
		ISP_REG_WR(idx, ISP_GTM_HIST_XPTS_0 + i*2, val);
	}

	return ret;
}

int isp_k_rgb_gtm_bypass(void *param)
{
	uint32_t val = 0;
	struct isp_gtm_bypass_param *p = (struct isp_gtm_bypass_param *)param;

	if (!param) {
		pr_err("fail to input ptr NULL\n");
		return -1;
	}

	if (g_isp_bypass[p->ctx_id] & (1 << _E_GTM)) {
		pr_debug("gtm mapping disable, idx %d\n", p->ctx_id);
		return 0;
	}

	val = ((p->hist_bypass & 1) << 2) | ((p->map_bypass & 1) << 1) | (p->mod_en & 1);
	ISP_REG_MWR(p->ctx_id, ISP_GTM_GLB_CTRL, 0x7, val);
	pr_debug("isp%d mod en %d, hitst bypass %d, map bypass %d\n",
		p->ctx_id, p->mod_en, p->hist_bypass, p->map_bypass);
	return 0;
}

int isp_k_cfg_rgb_gtm(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	uint32_t *calc_mode = NULL;
	struct dcam_dev_raw_gtm_block_info *gtm = NULL;
	struct cam_gtm_mapping *gtm_alg_calc = NULL;

	gtm = &isp_k_param->gtm_rgb_info;

	switch (param->property) {
	case DCAM_PRO_GTM_BLOCK:
		gtm = &isp_k_param->gtm_rgb_info;
		ret = copy_from_user((void *)gtm, param->property_param,
				sizeof(struct dcam_dev_raw_gtm_block_info));
		if (ret) {
			pr_err("fail to copy, ret=0x%x\n", (unsigned int)ret);
			return -EPERM;
		}
		isp_k_param->gtm_rgb_info.isupdate = 1;
		if (param->scene_id == PM_SCENE_CAP)
			gtm->bypass_info.gtm_hist_stat_bypass = 1;
		pr_debug("ctx_id %d , mod_en %d, map %d, hist_stat %d\n",
			idx, gtm->bypass_info.gtm_mod_en, gtm->bypass_info.gtm_map_bypass, gtm->bypass_info.gtm_hist_stat_bypass);
		break;
	case DCAM_PRO_GTM_MAPPING:
		gtm_alg_calc = &isp_k_param->gtm_sw_map_info;
		ret = copy_from_user((void *)gtm_alg_calc, param->property_param, sizeof(struct cam_gtm_mapping));
		if (ret) {
			pr_err("fail to copy alg_calc ret %d\n", ret);
			return -EPERM;
		}
		isp_k_param->gtm_sw_map_info.isupdate = 1;
		pr_debug("ctx_id %d, get gtm alg_calc info\n", idx);
		break;
	case DCAM_PRO_GTM_CALC_MODE:
		calc_mode = &isp_k_param->gtm_calc_mode;

		ret = copy_from_user((void *)calc_mode, param->property_param, sizeof(uint32_t));
		if (ret) {
			pr_err("fail to copy, ret=0x%x\n", (unsigned int)ret);
			return -EPERM;
		}
		pr_debug("gtm calc mode %d\n", isp_k_param->gtm_calc_mode);
		break;
	default:
		pr_err("fail cmd id:%d, not supported.\n",
			param->property);
		break;
	}
	return ret;
}

int isp_k_cpy_rgb_gtm(struct isp_k_block *param_block, struct isp_k_block *isp_k_param)
{
	int ret = 0;
	if (isp_k_param->gtm_rgb_info.isupdate == 1) {
		memcpy(&param_block->gtm_rgb_info, &isp_k_param->gtm_rgb_info, sizeof(struct dcam_dev_raw_gtm_block_info));
		param_block->gtm_calc_mode = isp_k_param->gtm_calc_mode;
		isp_k_param->gtm_rgb_info.isupdate = 0;
		param_block->gtm_rgb_info.isupdate = 1;
	}

	if (isp_k_param->gtm_sw_map_info.isupdate == 1) {
		memcpy(&param_block->gtm_sw_map_info, &isp_k_param->gtm_sw_map_info, sizeof(struct cam_gtm_mapping));
		isp_k_param->gtm_sw_map_info.isupdate = 0;
		param_block->gtm_sw_map_info.isupdate = 1;
	}
	return ret;
}
