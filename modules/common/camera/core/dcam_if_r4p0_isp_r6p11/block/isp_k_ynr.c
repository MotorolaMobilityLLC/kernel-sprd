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
#define pr_fmt(fmt) "YNR: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

static int isp_k_ynr_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_dev_ynr_info ynr_info;
	unsigned int i;

	memset(&ynr_info, 0x00, sizeof(ynr_info));
	ret = copy_from_user((void *)&ynr_info,
		param->property_param, sizeof(ynr_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_REG_MWR(idx, ISP_YNR_CTRL0, BIT_0, ynr_info.bypass);
	if (ynr_info.bypass) {
		pr_debug("bypass.\n");
		isp_dbg_s_ori_byp(SBLK_BYPASS, yuv_ynr, idx);
		return 0;
	}

	val = (((ynr_info.lowlux_bypass & 0x1) << 1) |
		  ((ynr_info.nr_enable & 0x1) << 2) |
		  ((ynr_info.l_blf_en[2] & 0x1) << 3) |
		  ((ynr_info.l_blf_en[1] & 0x1) << 4) |
		  ((ynr_info.l_blf_en[0] & 0x1) << 5));
	ISP_REG_MWR(idx, ISP_YNR_CTRL0, 0x3E, val);

	val = ((ynr_info.edge_th    & 0xFF) << 24) |
		  ((ynr_info.txt_th     & 0xFF) << 16) |
		  ((ynr_info.flat_th[0] & 0xFF) << 8) |
		   (ynr_info.flat_th[1] & 0xFF);
	ISP_REG_WR(idx, ISP_YNR_CFG0, val);

	val = ((ynr_info.flat_th[2] & 0xFF) << 24) |
		  ((ynr_info.flat_th[3] & 0xFF) << 16) |
		  ((ynr_info.flat_th[4] & 0xFF) << 8) |
		   (ynr_info.flat_th[5] & 0xFF);
	ISP_REG_WR(idx, ISP_YNR_CFG1, val);

	val = ((ynr_info.flat_th[6] & 0xFF) << 24) |
		  ((ynr_info.lut_th[0]  & 0xFF) << 16) |
		  ((ynr_info.lut_th[1]  & 0xFF) << 8) |
		   (ynr_info.lut_th[2]  & 0xFF);
	ISP_REG_WR(idx, ISP_YNR_CFG2, val);

	val = ((ynr_info.lut_th[3] & 0xFF) << 24) |
		  ((ynr_info.lut_th[4] & 0xFF) << 16) |
		  ((ynr_info.lut_th[5] & 0xFF) << 8) |
		   (ynr_info.lut_th[6] & 0xFF);
	ISP_REG_WR(idx, ISP_YNR_CFG3, val);

	for (i = 0; i < 2; i++) {
		val = ((ynr_info.addback[i*4] & 0xFF) << 24) |
			  ((ynr_info.addback[i*4+1] & 0xFF) << 16) |
			  ((ynr_info.addback[i*4+2] & 0xFF) << 8) |
			   (ynr_info.addback[i*4+3] & 0xFF);
		ISP_REG_WR(idx, ISP_YNR_CFG4 + 4*i, val);
	}

	val = ((ynr_info.addback[8] & 0xFF) << 24) |
		  ((ynr_info.sub_th[0] & 0x7F) << 16) |
		  ((ynr_info.sub_th[1] & 0x7F) << 8) |
		   (ynr_info.sub_th[2] & 0x7F);
	ISP_REG_WR(idx, ISP_YNR_CFG6, val);

	val = ((ynr_info.sub_th[3] & 0x7F) << 24) |
		  ((ynr_info.sub_th[4] & 0x7F) << 16) |
		  ((ynr_info.sub_th[5] & 0x7F) << 8) |
		   (ynr_info.sub_th[6] & 0x7F);
	ISP_REG_WR(idx, ISP_YNR_CFG7, val);

	val = ((ynr_info.sub_th[7] & 0x7F) << 24) |
		  ((ynr_info.sub_th[8] & 0x7F) << 16) |
		  ((ynr_info.l_euroweight[0][0] & 0xF) << 8) |
		  ((ynr_info.l_euroweight[0][1] & 0xF) << 4) |
		   (ynr_info.l_euroweight[0][2] & 0xF);
	ISP_REG_WR(idx, ISP_YNR_CFG8, val);

	val = ((ynr_info.l_euroweight[1][0] & 0xF) << 20) |
		  ((ynr_info.l_euroweight[1][1] & 0xF) << 16) |
		  ((ynr_info.l_euroweight[1][2] & 0xF) << 12) |
		  ((ynr_info.l_euroweight[2][0] & 0xF) << 8) |
		  ((ynr_info.l_euroweight[2][1] & 0xF) << 4) |
		   (ynr_info.l_euroweight[2][2] & 0xF);
	ISP_REG_WR(idx, ISP_YNR_CFG9, val);

	val = ((ynr_info.l_wf_index[0] & 0xF) << 24) |
		  ((ynr_info.l_wf_index[1] & 0xF) << 20) |
		  ((ynr_info.l_wf_index[2] & 0xF) << 16) |
		  ((ynr_info.l1_txt_th0 & 0x7) << 12) |
		  ((ynr_info.l1_txt_th1 & 0x7) << 8) |
		  ((ynr_info.l0_lut_th0 & 0x7) << 4) |
		   (ynr_info.l0_lut_th1  & 0x7);
	ISP_REG_WR(idx, ISP_YNR_CFG10, val);


	for (i = 0; i < 6; i++) {
		val = ((ynr_info.wlt_th[i*4] & 0x7F) << 24) |
			  ((ynr_info.wlt_th[i*4+1] & 0x7F) << 16) |
			  ((ynr_info.wlt_th[i*4+2] & 0x7F) << 8) |
			   (ynr_info.wlt_th[i*4+3] & 0x7F);
		ISP_REG_WR(idx, ISP_YNR_WLT0 + 4 * i, val);
	}

	for (i = 0; i < 8; i++) {
		val = ((ynr_info.freq_ratio[i*3+0] & 0x3FF) << 20) |
			  ((ynr_info.freq_ratio[i*3+1] & 0x3FF) << 10) |
			   (ynr_info.freq_ratio[i*3+2] & 0x3FF);
		ISP_REG_WR(idx, ISP_YNR_FRATIO0 +	4 * i, val);
	}


	/*for slice config*/
	ISP_REG_WR(idx, ISP_YNR_CFG11, 0);

	val = ((ynr_info.center.x & 0xFFFF) << 16) |
		   (ynr_info.center.y & 0xFFFF);
	ISP_REG_WR(idx, ISP_YNR_CFG12, val);

	val = ((ynr_info.radius & 0xFFFF) << 16) |
		   (ynr_info.dist_interval & 0xFFFF);
	ISP_REG_WR(idx, ISP_YNR_CFG13, val);

	for (i = 0; i < 2; i++) {
		val = ((ynr_info.sal_nr_str[i*4+0] & 0x7F) << 24) |
			  ((ynr_info.sal_nr_str[i*4+1] & 0x7F) << 16) |
			  ((ynr_info.sal_nr_str[i*4+2] & 0x7F) << 8) |
			   (ynr_info.sal_nr_str[i*4+3] & 0x7F);
		ISP_REG_WR(idx, ISP_YNR_CFG14 + 4*i, val);
	}

	for (i = 0; i < 2; i++) {
		val = ((ynr_info.sal_offset[i*4+0] & 0xFF) << 24) |
			  ((ynr_info.sal_offset[i*4+1] & 0xFF) << 16) |
			  ((ynr_info.sal_offset[i*4+2] & 0xFF) << 8) |
			   (ynr_info.sal_offset[i*4+3] & 0xFF);
		ISP_REG_WR(idx, ISP_YNR_CFG16 + 4*i, val);
	}

	return ret;
}

int isp_k_cfg_ynr(struct isp_io_param *param,
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
	case ISP_PRO_YNR_BLOCK:
	    ret = isp_k_ynr_block(param, idx);
	    break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}
