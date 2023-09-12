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
#define pr_fmt(fmt) "Pre-CDN: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

static int isp_k_yuv_precdn_block(struct isp_io_param *param,
							enum isp_id idx)
{
	int ret = 0;
	struct isp_dev_yuv_precdn_info pre_cdn_info;
	unsigned int val = 0;
	unsigned int i = 0;

	memset(&pre_cdn_info, 0x00, sizeof(pre_cdn_info));

	ret = copy_from_user((void *)&pre_cdn_info,
		param->property_param, sizeof(pre_cdn_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}
	ISP_REG_MWR(idx, ISP_PRECDN_PARAM, BIT_0, pre_cdn_info.bypass);
	if (pre_cdn_info.bypass) {
		pr_debug("bypass.\n");
		isp_dbg_s_ori_byp(SBLK_BYPASS, yuv_precdn, idx);
		return 0;
	}

	if (pre_cdn_info.mode)
		ISP_REG_OWR(idx, ISP_PRECDN_PARAM, BIT_4);
	else
		ISP_REG_MWR(idx, ISP_PRECDN_PARAM, BIT_4, 0);


	if (pre_cdn_info.median_writeback_en)
		ISP_REG_OWR(idx, ISP_PRECDN_PARAM, BIT_8);
	else
		ISP_REG_MWR(idx, ISP_PRECDN_PARAM, BIT_8, 0);


	ISP_REG_MWR(idx, ISP_PRECDN_PARAM, BIT_13 | BIT_12,
		pre_cdn_info.median_mode << 12);

	ISP_REG_MWR(idx, ISP_PRECDN_PARAM, BIT_17 | BIT_16,
		pre_cdn_info.den_stren << 16);

	if (pre_cdn_info.uv_joint)
		ISP_REG_OWR(idx, ISP_PRECDN_PARAM, BIT_20);
	else
		ISP_REG_MWR(idx, ISP_PRECDN_PARAM, BIT_20, 0);


	val = (pre_cdn_info.median_thr_uv.thru0 & 0x7F) |
		  ((pre_cdn_info.median_thr_uv.thru1 & 0xFF) << 8) |
		  ((pre_cdn_info.median_thr_uv.thrv0 & 0x7F) << 16) |
		  ((pre_cdn_info.median_thr_uv.thrv1 & 0xFF) << 24);
	ISP_REG_WR(idx, ISP_PRECDN_MEDIAN_THRUV01, val);

	val = (pre_cdn_info.median_thr & 0x1FF) |
		  ((pre_cdn_info.uv_thr & 0xFF) << 16) |
		  ((pre_cdn_info.y_thr & 0xFF) << 24);
	ISP_REG_WR(idx, ISP_PRECDN_THRYUV, val);

	for (i = 0; i < 3; i++) {
		val = (pre_cdn_info.r_segu[0][i*2] & 0xFF) |
			  ((pre_cdn_info.r_segu[1][i*2] & 0x7)  << 8) |
			  ((pre_cdn_info.r_segu[0][i*2+1] & 0xFF) << 16) |
			  ((pre_cdn_info.r_segu[1][i*2+1] & 0x7)  << 24);
		ISP_REG_WR(idx, ISP_PRECDN_SEGU_0 + i * 4, val);
	}
	val = (pre_cdn_info.r_segu[0][6] & 0xFF)
		| ((pre_cdn_info.r_segu[1][6] & 0x7) << 8);
	ISP_REG_WR(idx, ISP_PRECDN_SEGU_3, val);

	for (i = 0; i < 3; i++) {
		val = (pre_cdn_info.r_segv[0][i*2] & 0xFF) |
			  ((pre_cdn_info.r_segv[1][i*2]   & 0x7) << 8) |
			  ((pre_cdn_info.r_segv[0][i*2+1] & 0xFF) << 16) |
			  ((pre_cdn_info.r_segv[1][i*2+1] & 0x7) << 24);
		ISP_REG_WR(idx, ISP_PRECDN_SEGV_0 + i * 4, val);
	}
	val = (pre_cdn_info.r_segv[0][6] & 0xFF)
		| ((pre_cdn_info.r_segv[1][6] & 0x7) << 8);
	ISP_REG_WR(idx, ISP_PRECDN_SEGV_3, val);

	for (i = 0; i < 3; i++) {
		val = (pre_cdn_info.r_segy[0][i*2] & 0xFF) |
			  ((pre_cdn_info.r_segy[1][i*2] & 0x7) << 8) |
			  ((pre_cdn_info.r_segy[0][i*2+1] & 0xFF) << 16) |
			  ((pre_cdn_info.r_segy[1][i*2+1] & 0x7) << 24);
		ISP_REG_WR(idx, ISP_PRECDN_SEGY_0 + i * 4, val);
	}
	val = (pre_cdn_info.r_segy[0][6] & 0xFF)
		| ((pre_cdn_info.r_segy[1][6] & 0x7) << 8);
	ISP_REG_WR(idx, ISP_PRECDN_SEGY_3, val);

	for (i = 0; i < 3; i++) {
		val =  (pre_cdn_info.r_distw[i*8]   & 0x7) |
			  ((pre_cdn_info.r_distw[i*8+1] & 0x7) << 4) |
			  ((pre_cdn_info.r_distw[i*8+2] & 0x7) << 8) |
			  ((pre_cdn_info.r_distw[i*8+3] & 0x7) << 12) |
			  ((pre_cdn_info.r_distw[i*8+4] & 0x7) << 16) |
			  ((pre_cdn_info.r_distw[i*8+5] & 0x7) << 20) |
			  ((pre_cdn_info.r_distw[i*8+6] & 0x7) << 24) |
			  ((pre_cdn_info.r_distw[i*8+7] & 0x7) << 28);
		ISP_REG_WR(idx, ISP_PRECDN_DISTW0 + i * 4, val);
	}
	val = pre_cdn_info.r_distw[24] & 0x7;
	ISP_REG_WR(idx, ISP_PRECDN_DISTW3, val);

	return ret;
}

int isp_k_cfg_pre_cdn(struct isp_io_param *param,
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
	case ISP_PRO_PRE_CDN_BLOCK:
		ret = isp_k_yuv_precdn_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}
