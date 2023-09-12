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

#include "sprd_mm.h"
#include "sprd_isp_hw.h"
#include "isp_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "Post-CDN: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

static int isp_k_post_cdn_block(struct isp_io_param *param,
				enum isp_id idx)
{
	int ret = 0;
	struct isp_dev_post_cdn_info post_cdn_info;
	unsigned int val = 0;
	unsigned int i = 0;

	memset(&post_cdn_info, 0x00, sizeof(post_cdn_info));

	ret = copy_from_user((void *)&post_cdn_info,
		param->property_param, sizeof(post_cdn_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	val = ((post_cdn_info.bypass & 0x1) |
		 ((post_cdn_info.downsample_bypass & 0x1) << 1) |
		 ((post_cdn_info.mode & 0x1) << 2) |
		 ((post_cdn_info.writeback_en & 0x1) << 3) |
		 ((post_cdn_info.uvjoint & 0x1) << 4) |
		 ((post_cdn_info.median_mode & 0x7) << 5));

	ISP_REG_MWR(idx, ISP_POSTCDN_COMMON_CTRL, 0xFF, val);


	if (post_cdn_info.bypass) {
		pr_debug("bypass.\n");
		isp_dbg_s_ori_byp(SBLK_BYPASS, yuv_postcdn, idx);
		return 0;
	}

	ISP_REG_WR(idx, ISP_POSTCDN_ADPT_THR,
			post_cdn_info.adapt_med_thr & 0x3FFF);

	val = (post_cdn_info.uvthr0 & 0x1FF)
		| ((post_cdn_info.uvthr1 & 0x1FF) << 16);
	ISP_REG_WR(idx, ISP_POSTCDN_UVTHR, val);

	val = (post_cdn_info.thr_uv.thru0 & 0xFF)
		| ((post_cdn_info.thr_uv.thru1 & 0xFF) << 16);
	ISP_REG_WR(idx, ISP_POSTCDN_THRU, val);

	val = (post_cdn_info.thr_uv.thrv0 & 0xFF)
		| ((post_cdn_info.thr_uv.thrv1 & 0xFF) << 16);
	ISP_REG_WR(idx, ISP_POSTCDN_THRV, val);

	for (i = 0; i < 3; i++) {
		val = (post_cdn_info.r_segu[0][i*2] & 0xFF) |
			  ((post_cdn_info.r_segu[1][i*2] & 0x7)  << 8) |
			  ((post_cdn_info.r_segu[0][i*2+1] & 0xFF) << 16) |
			  ((post_cdn_info.r_segu[1][i*2+1] & 0x7) << 24);
		ISP_REG_WR(idx, ISP_POSTCDN_RSEGU01 + i * 4, val);
	}

	val = (post_cdn_info.r_segu[0][6] & 0xFF)
		| ((post_cdn_info.r_segu[1][6] & 0x7) << 8);
	ISP_REG_WR(idx, ISP_POSTCDN_RSEGU6, val);

	for (i = 0; i < 3; i++) {
		val = (post_cdn_info.r_segv[0][i*2] & 0xFF) |
			  ((post_cdn_info.r_segv[1][i*2] & 0x7) << 8) |
			  ((post_cdn_info.r_segv[0][i*2+1] & 0xFF) << 16) |
			  ((post_cdn_info.r_segv[1][i*2+1] & 0x7) << 24);
		ISP_REG_WR(idx, ISP_POSTCDN_RSEGV01 + i * 4, val);
	}

	val = (post_cdn_info.r_segv[0][6] & 0xFF)
		| ((post_cdn_info.r_segv[1][6] & 0x7) << 8);
	ISP_REG_WR(idx, ISP_POSTCDN_RSEGV6, val);

	for (i = 0; i < 15; i++) {
		val = (post_cdn_info.r_distw[i][0] & 0x7) |
			  ((post_cdn_info.r_distw[i][1] & 0x7) << 3) |
			  ((post_cdn_info.r_distw[i][2] & 0x7) << 6) |
			  ((post_cdn_info.r_distw[i][3] & 0x7) << 9) |
			  ((post_cdn_info.r_distw[i][4] & 0x7) << 12);
		ISP_REG_WR(idx, ISP_POSTCDN_R_DISTW0	+ i	* 4, val);
	}
	/*start_row_mode4 = start_row & 0x3 for sclice*/
	ISP_REG_WR(idx, ISP_POSTCDN_START_ROW_MOD4, 0);

	return ret;
}

int isp_k_cfg_post_cdn(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param.\n");
		return -EPERM;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property param.\n");
		return -EPERM;
	}

	switch (param->property) {
	case ISP_PRO_POST_CDN_BLOCK:
		ret = isp_k_post_cdn_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}
