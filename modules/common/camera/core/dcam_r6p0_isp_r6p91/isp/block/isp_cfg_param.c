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
#include "sprd_isp_r6p91.h"
#include "isp_block.h"

typedef int32_t (*isp_cfg_fun_ptr)(struct isp_io_param *isp_param);

struct isp_cfg_fun {
	uint32_t sub_block;
	isp_cfg_fun_ptr cfg_fun;
};

static struct isp_cfg_fun isp_cfg_fun_tab[] = {
	{ISP_BLOCK_PGG,           isp_k_cfg_pgg},
	{ISP_BLOCK_BLC,                    isp_k_cfg_blc},
	{ISP_BLOCK_RGBG,               isp_k_cfg_rgb_gain},
	{ISP_BLOCK_BINNING4AWB,                isp_k_cfg_binning},
	{ISP_BLOCK_AWBC,                    isp_k_cfg_awbc},
	{ISP_BLOCK_RAW_AEM,                isp_k_cfg_raw_aem},
	{ISP_BLOCK_ANTI_FLICKER_NEW,        isp_k_cfg_anti_flicker_new},
	{ISP_BLOCK_BPC,                 isp_k_cfg_bpc},
	{ISP_BLOCK_BDN,               isp_k_cfg_bdn},
	{ISP_BLOCK_GRGB,                   isp_k_cfg_grgb},
	{ISP_BLOCK_CFA,                 isp_k_cfg_cfa},
	{ISP_BLOCK_CMC,                    isp_k_cfg_cmc10},
	{ISP_BLOCK_CCE,                    isp_k_cfg_cce},
	{ISP_BLOCK_HSV,                    isp_k_cfg_hsv},
	{ISP_BLOCK_PRE_CDN_RGB,            isp_k_cfg_pre_cdn_rgb},
	{ISP_BLOCK_PSTRZ,              isp_k_cfg_posterize},
	{ISP_BLOCK_RAW_AFM,                 isp_k_cfg_rgb_afm},
	{ISP_BLOCK_YUV_PRECDN,             isp_k_cfg_yuv_precdn},
	{ISP_BLOCK_PRE_FILTER,             isp_k_cfg_prefilter},
	{ISP_BLOCK_BRIGHTNESS,             isp_k_cfg_brightness},
	{ISP_BLOCK_HIST,                   isp_k_cfg_hist},
	{ISP_BLOCK_HIST2,                  isp_k_cfg_hist2},
	{ISP_BLOCK_YUV_CDN,                isp_k_cfg_yuv_cdn},
	{ISP_BLOCK_EDGE,                   isp_k_cfg_edge},
	{ISP_BLOCK_CSS,                    isp_k_cfg_css},
	{ISP_BLOCK_CSA,                    isp_k_cfg_csa},
	{ISP_BLOCK_HUE,                    isp_k_cfg_hue},
	{ISP_BLOCK_POST_CDN,               isp_k_cfg_post_cdn},
	{ISP_BLOCK_YDELAY,                 isp_k_cfg_ydelay},
	{ISP_BLOCK_IIRCNR,                 isp_k_cfg_iircnr},
	{ISP_BLOCK_DISPATCH,               isp_k_cfg_dispatch},
	{ISP_BLOCK_ARBITER,             isp_k_cfg_arbiter},
	{ISP_BLOCK_COMMON,              isp_k_cfg_common},
};

int32_t isp_cfg_param(void  *param, struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;
	uint32_t i = 0, cnt = 0;
	isp_cfg_fun_ptr cfg_fun_ptr = NULL;
	struct isp_io_param isp_param = {0, 0, 0, NULL};

	if (!param) {
		pr_info("isp_cfg_param: param is null error.\n");
		return -1;
	}

	ret = copy_from_user((void *)&isp_param, (void *)param,
				sizeof(isp_param));
	if (ret != 0) {
		pr_info("isp_cfg_param: copy error, ret = 0x%x\n",
					(uint32_t)ret);
		return -1;
	}

	if (isp_param.sub_block == ISP_BLOCK_2D_LSC) {
		ret = isp_k_cfg_2d_lsc(&isp_param, isp_k_param);
	} else if (isp_param.sub_block == ISP_BLOCK_GAMMA) {
		ret = isp_k_cfg_gamma(&isp_param, isp_k_param);
	} else if (isp_param.sub_block == ISP_BLOCK_YGAMMA) {
		ret = isp_k_cfg_ygamma(&isp_param, isp_k_param);
	} else if (isp_param.sub_block == ISP_BLOCK_NLM) {
		ret = isp_k_cfg_nlm(&isp_param, isp_k_param);
	} else if (isp_param.sub_block == ISP_BLOCK_FETCH) {
		ret = isp_k_cfg_fetch(&isp_param, isp_k_param);
	} else if (isp_param.sub_block == ISP_BLOCK_STORE) {
		ret = isp_k_cfg_store(&isp_param, isp_k_param);
	} else {
		cnt = ARRAY_SIZE(isp_cfg_fun_tab);
		for (i = 0; i < cnt; i++) {
			if (isp_param.sub_block ==
					isp_cfg_fun_tab[i].sub_block) {
				cfg_fun_ptr = isp_cfg_fun_tab[i].cfg_fun;
				break;
			}
		}

		if (cfg_fun_ptr != NULL)
			ret = cfg_fun_ptr(&isp_param);
	}

	return ret;
}
