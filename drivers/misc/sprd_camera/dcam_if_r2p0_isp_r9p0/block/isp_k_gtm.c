/*
 * Copyright(C) 2018-2019 Spreadtrum Communications	Inc.
 *
 * This	software is licensed under the terms of	the GNU	General	Public
 * License version 2, as published by the Free Software	Foundation, and
 * may be copied, distributed, and modified under those	terms.
 *
 * This	program	is distributed in the hope that	it will	be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 */

#include <linux/uaccess.h>
#include <video/sprd_mm.h>

#include "isp_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "GTM: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_raw_gtm_bypass(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int bypass = 0;

	ret = copy_from_user((void *)&bypass,
			param->property_param, sizeof(bypass));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	DCAM_REG_MWR(idx, DCAM_GTM_GLB_CTRL, BIT_0, bypass);

	return ret;
}

int isp_k_raw_gtm_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0, i = 0;
	unsigned int val[GTM_HIST_BIN_NUM] = {0};
	struct isp_dev_gtm_info gtm_info;

	memset(&gtm_info, 0x00, sizeof(gtm_info));
	ret = copy_from_user((void *)&gtm_info, param->property_param,
			sizeof(struct isp_dev_gtm_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		ret = -1;
	}

	DCAM_REG_MWR(idx, DCAM_GTM_GLB_CTRL,
		BIT_0,
		gtm_info.gtm_glb_ctrl.gtm_mod_en);
	if (!gtm_info.gtm_glb_ctrl.gtm_mod_en) {
		pr_info("gtm is disable");
		return ret;
	}
	DCAM_REG_MWR(idx, DCAM_GTM_GLB_CTRL,
		0xF << 28,
		gtm_info.gtm_glb_ctrl.gtm_tm_out_bit_depth << 28);
	DCAM_REG_MWR(idx, DCAM_GTM_GLB_CTRL,
		0xF << 24,
		gtm_info.gtm_glb_ctrl.gtm_tm_in_bit_depth << 24);
	DCAM_REG_MWR(idx, DCAM_GTM_GLB_CTRL,
		BIT_7,
		gtm_info.gtm_glb_ctrl.gtm_slice_main << 7);
	DCAM_REG_MWR(idx, DCAM_GTM_GLB_CTRL,
		0x3 << 5,
		gtm_info.gtm_glb_ctrl.gtm_tm_luma_est_mode << 5);
	DCAM_REG_MWR(idx, DCAM_GTM_GLB_CTRL,
		BIT_4,
		gtm_info.gtm_glb_ctrl.gtm_cur_is_first_frame << 4);
	DCAM_REG_MWR(idx, DCAM_GTM_GLB_CTRL,
		BIT_3,
		gtm_info.gtm_glb_ctrl.gtm_tm_param_calc_by_hw << 3);
	DCAM_REG_MWR(idx, DCAM_GTM_GLB_CTRL,
		BIT_2,
		gtm_info.gtm_glb_ctrl.gtm_hist_stat_bypass << 2);
	DCAM_REG_MWR(idx, DCAM_GTM_GLB_CTRL,
		BIT_1,
		gtm_info.gtm_glb_ctrl.gtm_map_bypass << 1);

	DCAM_REG_MWR(idx, DCAM_GTM_HIST_CTRL0,
		0x7FFF << 4,
		gtm_info.gtm_hist_ctrl0.gtm_imgkey_setting_value << 4);
	DCAM_REG_MWR(idx, DCAM_GTM_HIST_CTRL0,
		BIT_0,
		gtm_info.gtm_hist_ctrl0.gtm_imgkey_setting_mode);
	DCAM_REG_MWR(idx, DCAM_GTM_HIST_CTRL1,
		0x3FFF << 16,
		gtm_info.gtm_hist_ctrl1.gtm_target_norm_coeff << 16);
	DCAM_REG_MWR(idx, DCAM_GTM_HIST_CTRL1,
		0x3FFF << 2,
		gtm_info.gtm_hist_ctrl1.gtm_target_norm << 2);
	DCAM_REG_MWR(idx, DCAM_GTM_HIST_CTRL1,
		BIT_0,
		gtm_info.gtm_hist_ctrl1.gtm_target_norm_setting_mode);
	DCAM_REG_MWR(idx, DCAM_GTM_HIST_YMIN,
		0xFF,
		gtm_info.gtm_hist_ymin.gtm_ymin);
	DCAM_REG_MWR(idx, DCAM_GTM_HIST_CTRL2,
		0x3FFF << 16,
		gtm_info.gtm_hist_ctrl2.gtm_yavg << 16);
	DCAM_REG_MWR(idx, DCAM_GTM_HIST_CTRL2,
		0x3FFF,
		gtm_info.gtm_hist_ctrl2.gtm_ymax);
	DCAM_REG_MWR(idx, DCAM_GTM_HIST_CTRL3,
		0xFFFF << 16,
		gtm_info.gtm_hist_ctrl3.gtm_log_min_int << 16);
	DCAM_REG_MWR(idx, DCAM_GTM_HIST_CTRL3,
		0xFFFF,
		gtm_info.gtm_hist_ctrl3.gtm_lr_int);
	DCAM_REG_MWR(idx, DCAM_GTM_HIST_CTRL4,
		0xFFFF << 16,
		gtm_info.gtm_hist_ctrl4.gtm_log_diff_int << 16);
	DCAM_REG_MWR(idx, DCAM_GTM_HIST_CTRL4,
		0xFFFF,
		gtm_info.gtm_hist_ctrl4.gtm_log_max_int);
	DCAM_REG_MWR(idx, DCAM_GTM_HIST_CTRL5,
		0x3FFFFFF,
		gtm_info.gtm_hist_ctrl5.gtm_hist_total);
	DCAM_REG_MWR(idx, DCAM_GTM_HIST_CTRL6,
		0xFFFFF,
		gtm_info.gtm_hist_ctrl6.gtm_min_per);
	DCAM_REG_MWR(idx, DCAM_GTM_HIST_CTRL7,
		0xFFFFF,
		gtm_info.gtm_hist_ctrl7.gtm_max_per);
	DCAM_REG_MWR(idx, DCAM_GTM_LOG_DIFF,
		0x1FFFFFFF,
		gtm_info.gtm_log_diff.gtm_log_diff);
	DCAM_REG_MWR(idx, DCAM_GTM_TM_YMIN_SMOOTH,
		0x1FF << 23,
		gtm_info.gtm_tm_ymin_smooth.gtm_pre_ymin_weight << 23);
	DCAM_REG_MWR(idx, DCAM_GTM_TM_YMIN_SMOOTH,
		0x1FF << 14,
		gtm_info.gtm_tm_ymin_smooth.gtm_cur_ymin_weight << 14);
	DCAM_REG_MWR(idx, DCAM_GTM_TM_YMIN_SMOOTH,
		0x1FF,
		gtm_info.gtm_tm_ymin_smooth.gtm_ymax_diff_thr);
	DCAM_REG_MWR(idx, DCAM_GTM_TM_LUMAFILTER0,
		0xFF << 24,
		gtm_info.gtm_tm_lumafilter0.tm_lumafilter_c10 << 24);
	DCAM_REG_MWR(idx, DCAM_GTM_TM_LUMAFILTER0,
		0xFF << 16,
		gtm_info.gtm_tm_lumafilter0.tm_lumafilter_c02 << 16);
	DCAM_REG_MWR(idx, DCAM_GTM_TM_LUMAFILTER0,
		0xFF << 8,
		gtm_info.gtm_tm_lumafilter0.tm_lumafilter_c01 << 8);
	DCAM_REG_MWR(idx, DCAM_GTM_TM_LUMAFILTER0,
		0xFF,
		gtm_info.gtm_tm_lumafilter0.tm_lumafilter_c00);
	DCAM_REG_MWR(idx, DCAM_GTM_TM_LUMAFILTER1,
		0xFF << 24,
		gtm_info.gtm_tm_lumafilter1.tm_lumafilter_c21 << 24);
	DCAM_REG_MWR(idx, DCAM_GTM_TM_LUMAFILTER1,
		0xFF << 16,
		gtm_info.gtm_tm_lumafilter1.tm_lumafilter_c20 << 16);
	DCAM_REG_MWR(idx, DCAM_GTM_TM_LUMAFILTER1,
		0xFF << 8,
		gtm_info.gtm_tm_lumafilter1.tm_lumafilter_c12 << 8);
	DCAM_REG_MWR(idx, DCAM_GTM_TM_LUMAFILTER1,
		0xFF,
		gtm_info.gtm_tm_lumafilter1.tm_lumafilter_c11);
	DCAM_REG_MWR(idx, DCAM_GTM_TM_LUMAFILTER2,
		0xF << 28,
		gtm_info.gtm_tm_lumafilter2.tm_lumafilter_shift << 28);
	DCAM_REG_MWR(idx, DCAM_GTM_TM_LUMAFILTER2,
		0xFF,
		gtm_info.gtm_tm_lumafilter2.tm_lumafilter_c22);
	DCAM_REG_MWR(idx, DCAM_GTM_TM_RGB2YCOEFF0,
		0x7FF << 16,
		gtm_info.gtm_tm_rgb2ycoeff0.tm_rgb2y_g_coeff << 16);
	DCAM_REG_MWR(idx, DCAM_GTM_TM_RGB2YCOEFF0,
		0x7FF,
		gtm_info.gtm_tm_rgb2ycoeff0.tm_rgb2y_r_coeff);
	DCAM_REG_MWR(idx, DCAM_GTM_TM_RGB2YCOEFF1,
		0x7FF,
		gtm_info.gtm_tm_rgb2ycoeff1.tm_rgb2y_b_coeff);

	DCAM_REG_MWR(idx, DCAM_GTM_SLICE_LINE_STARTPOS,
		0x1FFF,
		gtm_info.gtm_slice_line_startpos.gtm_slice_line_startpos);
	DCAM_REG_MWR(idx, DCAM_GTM_SLICE_LINE_ENDPOS,
		0x1FFF,
		gtm_info.gtm_slice_line_endpos.gtm_slice_line_endpos);

	for (i = 0; i < GTM_HIST_BIN_NUM; i += 2)
		val[i] = gtm_info.histBin[i] << 16 | gtm_info.histBin[i+1];

	for (i = 0; i < GTM_HIST_BIN_NUM / 2; i++) {
		DCAM_REG_MWR(idx, DCAM_GTM_HIST_XPTS_0 + i*4,
				0x3FFF << 16 | 0x3FFF,
				val[i]);
	}

	return ret;
}

int isp_k_cfg_gtm(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t com_idx)
{
	int ret = 0;
	enum isp_id idx = ISP_ID_0;

	if (!param) {
		pr_err("fail to get param\n");
		return -1;
	}

	if (!param->property_param) {
		pr_err("fail to get property_param\n");
		return -1;
	}

	idx = ISP_GET_ISP_ID(com_idx);
	switch (param->property) {
	case ISP_PRO_RGB_GTM_BYPASS:
		ret = isp_k_raw_gtm_bypass(param, idx);
		break;
	case ISP_PRO_RGB_GTM_BLOCK:
		ret = isp_k_raw_gtm_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
