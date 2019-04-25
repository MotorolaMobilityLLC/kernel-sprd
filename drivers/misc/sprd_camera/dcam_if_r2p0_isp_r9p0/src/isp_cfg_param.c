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

#include <linux/uaccess.h>

#include "isp_block.h"

typedef int (*isp_cfg_fun_ptr)(struct isp_io_param *isp_param,
			struct isp_k_block *isp_k_param, uint32_t idx);

struct isp_cfg_fun {
	unsigned int sub_block;
	isp_cfg_fun_ptr cfg_fun;
};

static struct isp_cfg_fun isp_cfg_fun_tab[] = {
	{ISP_BLOCK_CFA,         isp_k_cfg_cfa},
	{ISP_BLOCK_HIST,        isp_k_cfg_hist},
	{ISP_BLOCK_BRIGHTNESS,  isp_k_cfg_brightness},
	{ISP_BLOCK_CMC,         isp_k_cfg_cmc10},
	{ISP_BLOCK_CONTRAST,    isp_k_cfg_contrast},
	{ISP_BLOCK_CSA,         isp_k_cfg_csa},
	{ISP_BLOCK_EDGE,        isp_k_cfg_edge},
	{ISP_BLOCK_CCE,         isp_k_cfg_cce},
	{ISP_BLOCK_POST_CDN,    isp_k_cfg_post_cdn},
	{ISP_BLOCK_PRE_CDN,     isp_k_cfg_pre_cdn},
	{ISP_BLOCK_YUV_CDN,     isp_k_cfg_yuv_cdn},
	{ISP_BLOCK_UVD,         isp_k_cfg_uvd},
	{ISP_BLOCK_HSV,         isp_k_cfg_hsv},
	{ISP_BLOCK_YNR,         isp_k_cfg_ynr},
	{ISP_BLOCK_IIRCNR,      isp_k_cfg_iircnr},
	{ISP_BLOCK_NLM,         isp_k_cfg_nlm},
	{ISP_BLOCK_GAMMA,       isp_k_cfg_gamma},
	{ISP_BLOCK_COMMON,      isp_k_cfg_common},
	{ISP_BLOCK_DISPATCH,    isp_k_cfg_dispatch},
	{ISP_BLOCK_3DNR,        isp_k_cfg_3dnr}
};

struct dcam_cfg_fun {
	unsigned int sub_block;
	isp_cfg_fun_ptr cfg_fun;
};

static struct dcam_cfg_fun dcam_cfg_fun_tab[] = {
#ifdef FPGA_BRINGUP
	{ISP_BLOCK_BLC,              isp_k_cfg_blc},
	{ISP_BLOCK_BPC,              isp_k_cfg_bpc},
	{ISP_BLOCK_GRGB,             isp_k_cfg_grgb},
	{ISP_BLOCK_PDAF,             isp_k_cfg_pdaf},
	{ISP_BLOCK_EBD,              isp_k_cfg_ebd},
	{ISP_BLOCK_RGBG,             isp_k_cfg_rgb_gain},
	{ISP_BLOCK_RAW_AEM,          isp_k_cfg_raw_aem},
	{ISP_BLOCK_RAW_AFM,          isp_k_cfg_rgb_afm},
	{ISP_BLOCK_ANTI_FLICKER_NEW, isp_k_cfg_anti_flicker_new},
	{ISP_BLOCK_2D_LSC,           isp_k_cfg_2d_lsc},
	{ISP_BLOCK_AWBC,             isp_k_cfg_awbc},
	{ISP_BLOCK_RGBG_DITHER,      isp_k_cfg_rgb_dither},
	{ISP_BLOCK_GTM,              isp_k_cfg_gtm}
#endif
};

int sprd_isp_cfg_param(void *param,
		struct isp_k_block *isp_k_param, void *isp_handle)
{
	int ret = 0;
	unsigned int i = 0, cnt = 0;
	uint32_t idx = 0, idx1 = 0;
	isp_cfg_fun_ptr cfg_fun_ptr = NULL, dcam_func_ptr = NULL;
	struct isp_pipe_dev *dev = NULL;
	struct isp_io_param isp_param = {0, 0, 0, 0, NULL};

	dev = (struct isp_pipe_dev *)isp_handle;
	idx = dev->com_idx;

	if (!param) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}
	ret = copy_from_user((void *)&isp_param,
			(void *)param, sizeof(isp_param));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = 0x%x\n",
			(uint32_t)ret);
		return -1;
	}
	ISP_SET_SCENE_ID(idx, isp_param.scene_id);
	pr_debug("isp_cfg subblock %d, com_idx 0x%x, scene id %d\n",
		isp_param.sub_block, idx, isp_param.scene_id);

	cnt = ARRAY_SIZE(isp_cfg_fun_tab);
	for (i = 0; i < cnt; i++) {
		if (isp_param.sub_block ==
			isp_cfg_fun_tab[i].sub_block) {
			cfg_fun_ptr = isp_cfg_fun_tab[i].cfg_fun;
			break;
		}
	}

	if (cfg_fun_ptr != NULL)
		ret = cfg_fun_ptr(&isp_param, isp_k_param, idx);

	cnt = ARRAY_SIZE(dcam_cfg_fun_tab);
	for (i = 0; i < cnt; i++) {
		if (isp_param.sub_block ==
			dcam_cfg_fun_tab[i].sub_block) {
			dcam_func_ptr = dcam_cfg_fun_tab[i].cfg_fun;
			break;
		}
	}

	isp_k_param->need_4in1 = dev->need_4in1;
	if (dev->need_4in1) {
		idx1 = idx;
		if (isp_param.scene_id == 1)
			ISP_SET_ISP_ID(idx1, DCAM_ID_1);
		else
			ISP_SET_ISP_ID(idx1, DCAM_ID_0);
		if (dcam_func_ptr != NULL)
			ret = dcam_func_ptr(&isp_param, isp_k_param, idx1);
	} else {
		if (dcam_func_ptr != NULL)
			ret = dcam_func_ptr(&isp_param, isp_k_param, idx);
	}

	return ret;
}
