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
#define pr_fmt(fmt) "NLM: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

#define ISP_RAW_NLM_MOUDLE_BUF0                0
#define ISP_RAW_NLM_MOUDLE_BUF1                1

static int isp_k_nlm_block(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	unsigned int i = 0;
	unsigned int j = 0;
	struct isp_dev_nlm_info nlm_info;
	unsigned int *buff0 = NULL;
	unsigned int *buff1 = NULL;
	void *vst_addr = NULL;
	void *ivst_addr = NULL;

	memset(&nlm_info, 0x00, sizeof(nlm_info));
	ret = copy_from_user((void *)&nlm_info,
		param->property_param, sizeof(nlm_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	/* debugging vst_len and ivst_len parameters */
	if (nlm_info.vst_len == 0 ||
		nlm_info.ivst_len == 0 ||
		nlm_info.vst_len > 256*PAGE_SIZE ||
		nlm_info.ivst_len > 256*PAGE_SIZE) {
		pr_err("fail to nlm allocation -- (%u, %u)\n",
				nlm_info.vst_len, nlm_info.ivst_len);
		return -1;
	}

	if ((nlm_info.vst_len >= ISP_NLM_BUF_SIZE) ||
		(nlm_info.ivst_len >= ISP_NLM_BUF_SIZE)) {
		pr_err("fail to alloc vst or ivst buf too small!\n");
		return -EPERM;
	}

	if (!isp_k_param->nlm_vst_addr ||
		!isp_k_param->nlm_ivst_addr) {
		pr_err("fail to get nlm vst or ist addr!\n");
		return -EFAULT;
	}
	buff0 = (unsigned int *)isp_k_param->nlm_vst_addr;
	buff1 = (unsigned int *)isp_k_param->nlm_ivst_addr;

#ifdef CONFIG_64BIT
	vst_addr = (void *)(((unsigned long)nlm_info.vst_addr[1] << 32)
					| nlm_info.vst_addr[0]);
#else
	vst_addr = (void *)(nlm_info.vst_addr[0]);
#endif
	ret = copy_from_user((void *)buff0, vst_addr, nlm_info.vst_len);
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

#ifdef CONFIG_64BIT
	ivst_addr = (void *)(((unsigned long)nlm_info.ivst_addr[1] << 32)
					| nlm_info.ivst_addr[0]);
#else
	ivst_addr = (void *)(nlm_info.ivst_addr[0]);
#endif
	ret = copy_from_user((void *)buff1, ivst_addr, nlm_info.ivst_len);
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_REG_MWR(idx, ISP_NLM_PARA, BIT_0, nlm_info.bypass);
	ISP_REG_MWR(idx, ISP_IVST_PARA, BIT_0, nlm_info.bypass);
	ISP_REG_MWR(idx, ISP_VST_PARA, BIT_0, nlm_info.bypass);

	ISP_REG_MWR(idx, ISP_NLM_PARA, BIT_1, nlm_info.imp_opt_bypass << 1);
	ISP_REG_MWR(idx, ISP_NLM_PARA, BIT_2, nlm_info.flat_opt_bypass << 2);
	ISP_REG_MWR(idx, ISP_NLM_PARA,
			BIT_4, nlm_info.direction_mode_bypass << 4);
	ISP_REG_MWR(idx, ISP_NLM_PARA, BIT_5, nlm_info.first_lum_byapss << 5);
	ISP_REG_MWR(idx, ISP_NLM_PARA, BIT_6, nlm_info.simple_bpc_bypass << 6);

	if (nlm_info.bypass) {
		isp_dbg_s_ori_byp(SBLK_BYPASS, raw_nlm, idx);
		isp_dbg_s_ori_byp(SBLK_BYPASS, raw_ivst, idx);
		isp_dbg_s_ori_byp(SBLK_BYPASS, raw_vst, idx);
		return 0;
	}
	val = ((nlm_info.direction_cnt_th & 0x3) << 24) |
		  ((nlm_info.w_shift[2] & 0x3) << 20) |
		  ((nlm_info.w_shift[1] & 0x3) << 18) |
		  ((nlm_info.w_shift[0] & 0x3) << 16) |
		  ((nlm_info.den_strength & 0x3F) << 8) |
		  ((nlm_info.flat_opt_mode & 0x1) << 4) |
		  (nlm_info.dist_mode & 0x3);
	ISP_REG_WR(idx, ISP_NLM_MODE_CNT, val);

	val = ((nlm_info.simple_bpc_th & 0xFF)  << 16) |
		  (nlm_info.simple_bpc_lum_th & 0x3FF);
	ISP_REG_WR(idx, ISP_NLM_SIMPLE_BPC, val);

	val = ((nlm_info.lum_th1 & 0x3FF)  << 16) |
		   (nlm_info.lum_th0 & 0x3FF);
	ISP_REG_WR(idx, ISP_NLM_LUM_TH, val);

	val = ((nlm_info.tdist_min_th & 0xFFFF)  << 16) |
		   (nlm_info.diff_th	  & 0xFFFF);
	ISP_REG_WR(idx, ISP_NLM_DIRECTION_TH, val);

	for (i = 0; i < 24; i++) {
		val =  (nlm_info.lut_w[i*3+0] & 0x3FF) |
			  ((nlm_info.lut_w[i*3+1] & 0x3FF) << 10) |
			  ((nlm_info.lut_w[i*3+2] & 0x3FF) << 20);
		ISP_REG_WR(idx, ISP_NLM_LUT_W_0 + i * 4, val);
	}

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			val = ((nlm_info.lum_flat[i][j].thresh & 0x3FFF) << 16)
			| ((nlm_info.lum_flat[i][j].match_count & 0x1F) << 8)
			|(nlm_info.lum_flat[i][j].inc_strength & 0xFF);
			ISP_REG_WR(idx, ISP_NLM_LUM0_FLAT0_PARAM +
							(i*4+j)*8, val);
		}
	}

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 4; j++) {
			val = ((nlm_info.lum_flat_addback_min[i][j]
			& 0x7FF) << 20)
			| ((nlm_info.lum_flat_addback_max[i][j] & 0x3FF) << 8)
			| (nlm_info.lum_flat_addback0[i][j] & 0x7F);
			ISP_REG_WR(idx, ISP_NLM_LUM0_FLAT0_ADDBACK +
							(i*4+j)*8, val);
		}
	}

	for (i = 0; i < 3; i++) {
		val = ((nlm_info.lum_flat_addback1[i][0] & 0x7F) << 22) |
			  ((nlm_info.lum_flat_addback1[i][1] & 0x7F) << 15) |
			  ((nlm_info.lum_flat_addback1[i][2] & 0x7F) << 8) |
			   (nlm_info.lum_flat_dec_strenth[i] & 0xFF);
		ISP_REG_WR(idx, ISP_NLM_LUM0_FLAT3_PARAM + i*32, val);
	}

	 val = ((nlm_info.lum_flat_addback1[2][3] & 0x7F) << 14) |
		   ((nlm_info.lum_flat_addback1[1][3] & 0x7F) << 7) |
			(nlm_info.lum_flat_addback1[0][3] & 0x7F);
	ISP_REG_WR(idx, ISP_NLM_ADDBACK3, val);

	if (ISP_GET_MID(idx) == ISP_AP_MODE) {

		if (isp_k_param->raw_nlm_buf_id) {
			isp_k_param->raw_nlm_buf_id = ISP_RAW_NLM_MOUDLE_BUF0;
			memcpy((void *)(ISP_BASE_ADDR(idx) +
				ISP_VST_BUF0_CH0), buff0,
				ISP_VST_IVST_NUM * sizeof(unsigned int));
			memcpy((void *)(ISP_BASE_ADDR(idx) +
				ISP_IVST_BUF0_CH0), buff1,
				ISP_VST_IVST_NUM * sizeof(unsigned int));
		} else {
			isp_k_param->raw_nlm_buf_id = ISP_RAW_NLM_MOUDLE_BUF1;
			memcpy((void *)(ISP_BASE_ADDR(idx) +
				ISP_VST_BUF1_CH0), buff0,
				ISP_VST_IVST_NUM * sizeof(unsigned int));
			memcpy((void *)(ISP_BASE_ADDR(idx) +
				ISP_IVST_BUF1_CH0), buff1,
				ISP_VST_IVST_NUM * sizeof(unsigned int));
		}
	} else {
	/*
	 * In CFG MODE, we don't need to select parameter buffer.
	 * Just let HW to pick the correct buffer by itself.
	 * DO NOT use buffer 1 or it will cause capture frame
	 * all black problem.
	 *
	 */
		isp_k_param->raw_nlm_buf_id = ISP_RAW_NLM_MOUDLE_BUF0;
		memcpy((void *)(ISP_BASE_ADDR(idx) +
			ISP_VST_BUF0_CH0), buff0,
			ISP_VST_IVST_NUM * sizeof(unsigned int));
		memcpy((void *)(ISP_BASE_ADDR(idx) +
			ISP_IVST_BUF0_CH0), buff1,
			ISP_VST_IVST_NUM * sizeof(unsigned int));
	}

	ISP_REG_MWR(idx, ISP_VST_PARA, BIT_1,
			isp_k_param->raw_nlm_buf_id << 1);
	ISP_REG_MWR(idx, ISP_IVST_PARA, BIT_1,
			isp_k_param->raw_nlm_buf_id << 1);

	return ret;
}

int isp_k_cfg_nlm(struct isp_io_param *param,
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
	case ISP_PRO_NLM_BLOCK:
		ret = isp_k_nlm_block(param, isp_k_param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}

