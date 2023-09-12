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
#include "sprd_isp_hw.h"
#include "isp_block.h"
#include "isp_buf.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_CFG_PARAM: %d: %d %s: "\
	fmt, current->pid, __LINE__, __func__

typedef int (*isp_cfg_fun_ptr)(struct isp_io_param *isp_param,
			       struct isp_k_block *isp_k_param,
			       enum isp_id idx);

struct isp_cfg_fun {
	uint32_t sub_block;
	isp_cfg_fun_ptr cfg_fun;
};

static struct isp_cfg_fun isp_cfg_fun_tab[] = {
	 {ISP_BLOCK_FETCH,		isp_k_cfg_fetch},
	 {ISP_BLOCK_DISPATCH,		isp_k_cfg_dispatch},
	 {ISP_BLOCK_ARBITER,		isp_k_cfg_arbiter},

	 /* 3A */
	 {ISP_BLOCK_AWB,		isp_k_cfg_awb},
	 {ISP_BLOCK_RAW_AEM,		isp_k_cfg_raw_aem},
	 {ISP_BLOCK_BINNING,		isp_k_cfg_binning},
	 {ISP_BLOCK_RAW_AFM,		isp_k_cfg_rgb_afm},

	 /* RAW RGB */
	 {ISP_BLOCK_PGG,		isp_k_cfg_pgg},
	 {ISP_BLOCK_BLC,		isp_k_cfg_blc},
	 {ISP_BLOCK_POST_BLC,		isp_k_cfg_post_blc},
	 {ISP_BLOCK_NLM,		isp_k_cfg_nlm},
	 {ISP_BLOCK_RGBG,		isp_k_cfg_rgb_gain},
	 {ISP_BLOCK_NLC,		isp_k_cfg_nlc},
	 {ISP_BLOCK_2D_LSC,		isp_k_cfg_2d_lsc},
	 {ISP_BLOCK_RLSC,		isp_k_cfg_rlsc},
	 {ISP_BLOCK_BPC,		isp_k_cfg_bpc},
	 {ISP_BLOCK_GRGB,		isp_k_cfg_grgb},
	 {ISP_BLOCK_RGBG_DITHER,	isp_k_cfg_rgb_dither},

	 /* FRGB 8 */
	 {ISP_BLOCK_PSTRZ,		isp_k_cfg_pstrz},
	 {ISP_BLOCK_CCE,		isp_k_cfg_cce},
	 {ISP_BLOCK_UVD,		isp_k_cfg_uvd},

	 /* FRGB 10 */
	 {ISP_BLOCK_CFA,		isp_k_cfg_cfa},
	 {ISP_BLOCK_CMC,		isp_k_cfg_cmc10},
	 {ISP_BLOCK_GAMMA,		isp_k_cfg_gamma},
	 {ISP_BLOCK_HSV,		isp_k_cfg_hsv},

	 /* YIQ */
	 {ISP_BLOCK_ANTI_FLICKER_NEW,	isp_k_cfg_anti_flicker_new},

	 /* YUV*/
	 {ISP_BLOCK_PRE_CDN,		isp_k_cfg_pre_cdn},
	 {ISP_BLOCK_YUV_CDN,		isp_k_cfg_yuv_cdn},
	 {ISP_BLOCK_POST_CDN,		isp_k_cfg_post_cdn},
	 {ISP_BLOCK_YNR,		isp_k_cfg_ynr},
	 {ISP_BLOCK_BRIGHTNESS,		isp_k_cfg_brightness},
	 {ISP_BLOCK_CONTRAST,		isp_k_cfg_contrast},
	 {ISP_BLOCK_HIST,		isp_k_cfg_hist},
	 {ISP_BLOCK_HIST2,		isp_k_cfg_hist2},
	 {ISP_BLOCK_EDGE,		isp_k_cfg_edge},
	 {ISP_BLOCK_YGAMMA,		isp_k_cfg_ygamma},
	 {ISP_BLOCK_CSA,		isp_k_cfg_csa},
	 {ISP_BLOCK_HUE,		isp_k_cfg_hue},
	 {ISP_BLOCK_YDELAY,		isp_k_cfg_ydelay},
	 {ISP_BLOCK_IIRCNR,		isp_k_cfg_iircnr},
	 {ISP_BLOCK_NOISE_FILTER,	isp_k_cfg_noise_filter},
};

int isp_cfg_param(void *param, struct isp_k_block *isp_k_param,
		  struct isp_pipe_dev *dev)
{
	int ret = 0;
	uint32_t i = 0, cnt = 0;
	uint32_t idx = 0;
	isp_cfg_fun_ptr cfg_fun_ptr = NULL;

	struct isp_io_param isp_param = {0, 0, 0, 0, NULL};

	if (!param) {
		pr_err("fail to get param.\n");
		return -EPERM;
	}
	ret = copy_from_user((void *)&isp_param,
			(void *)param, sizeof(isp_param));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = 0x%x\n",
				(uint32_t)ret);
		return -EPERM;
	}

	idx = dev->com_idx;

	/*
	 * TODO: need to check if the scene_id from userspace is valid,
	 * Maybe the scene_id in userspace is correct, but to kernel is
	 * wrong, in case of bit-flip
	 */
	if (CHECK_ID_VALID(isp_param.scene_id))
		ISP_SET_SID(idx, isp_param.scene_id);
	else if (isp_param.scene_id != BLOCK_SCENE_DEF) {
		pr_err("fail to invalid scene id:%x, sub block:%u, property:%u\n",
		       isp_param.scene_id, isp_param.sub_block,
		       isp_param.property);
		return -EPERM;
	}

	if (unlikely(dev->is_raw_capture))
		ISP_SET_SID(idx, ISP_SCENE_CAP);

	pr_debug("block%u, property %u, com_idx 0x%x, reg_base[%d]=0x%lx\n",
		 isp_param.sub_block, isp_param.property, idx,
		 ISP_GET_IID(idx), ISP_BASE_ADDR(idx));

	switch (isp_param.sub_block) {
	case ISP_BLOCK_COMMON:
		ret = isp_k_cfg_common(&isp_param, idx, dev->is_raw_capture);
		break;
	case ISP_BLOCK_STORE:
		ret = isp_k_cfg_store(&isp_param, idx, dev->is_raw_capture);
		break;
	default:
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
		break;
	}

	return ret;
}
