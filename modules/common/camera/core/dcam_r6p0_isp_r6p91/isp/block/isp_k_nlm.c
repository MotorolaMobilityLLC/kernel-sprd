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
#include <linux/vmalloc.h>
#include "isp_block.h"

#define ISP_RAW_NLM_MOUDLE_BUF0                0
#define ISP_RAW_NLM_MOUDLE_BUF1                1

static int32_t isp_k_nlm_block(struct isp_io_param *param,
	struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t i = 0;
	struct isp_dev_nlm_info nlm_info;
	uint32_t *buff0 = NULL;
	uint32_t *buff1 = NULL;
	uint32_t *buff2 = NULL;
	void *vst_addr = NULL;
	void *ivst_addr = NULL;
	void *nlm_addr = NULL;

	memset(&nlm_info, 0, sizeof(nlm_info));
	ret = copy_from_user((void *)&nlm_info,
		param->property_param, sizeof(nlm_info));
	if (ret != 0) {
		pr_info("copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	buff0 = vzalloc(nlm_info.vst_len);
	if (buff0 == NULL)
		goto no_mem_err;

	buff1 = vzalloc(nlm_info.ivst_len);
	if (buff1 == NULL) {
		vfree(buff0);
		goto no_mem_err;
	}

	buff2 = vzalloc(nlm_info.nlm_len);
	if (buff2 == NULL) {
		vfree(buff0);
		vfree(buff1);
		goto no_mem_err;
	}

	vst_addr = (void *)(nlm_info.vst_addr[0]);
	ret = copy_from_user((void *)buff0, vst_addr, nlm_info.vst_len);
	if (ret != 0) {
		pr_info("vst copy error, ret=0x%x\n", (uint32_t)ret);
		vfree(buff0);
		vfree(buff1);
		vfree(buff2);
		return -1;
	}

	ivst_addr = (void *)(nlm_info.ivst_addr[0]);
	ret = copy_from_user((void *)buff1, ivst_addr, nlm_info.ivst_len);
	if (ret != 0) {
		pr_info("ivst copy error,ret=0x%x\n", (uint32_t)ret);
		vfree(buff0);
		vfree(buff1);
		vfree(buff2);
		return -1;
	}

	nlm_addr = (void *)(nlm_info.nlm_addr[0]);
	ret = copy_from_user((void *)buff2, nlm_addr, nlm_info.nlm_len);
	if (ret != 0) {
		pr_info("nlm copy error, ret=0x%x\n", (uint32_t)ret);
		vfree(buff0);
		vfree(buff1);
		vfree(buff2);
		return -1;
	}

	ISP_REG_MWR(ISP_NLM_PARA, BIT_1, nlm_info.imp_opt_bypass << 1);
	ISP_REG_MWR(ISP_NLM_PARA, BIT_2, nlm_info.flat_opt_bypass << 2);
	ISP_REG_MWR(ISP_NLM_PARA, BIT_3, nlm_info.flat_thr_bypass << 3);
	ISP_REG_MWR(ISP_NLM_PARA, BIT_4, nlm_info.direction_mode_bypass << 4);

	for (i = 0; i < 5; i++) {
		val = (nlm_info.thresh[i] & 0x3FFF)
			| ((nlm_info.cnt[i] & 0x1F) << 16)
			| ((nlm_info.strength[i] & 0xFF) << 24);
		ISP_REG_WR(ISP_NLM_FLAT_PARA_0 + i * 4, val);
	}

	ISP_REG_MWR(ISP_NLM_STERNGTH, 0x7F, nlm_info.den_strength);

	ISP_REG_MWR(ISP_NLM_STERNGTH, 0xFF00, nlm_info.texture_dec << 8);

	ISP_REG_MWR(ISP_NLM_ADD_BACK, 0x7F, nlm_info.addback);
	ISP_REG_MWR(ISP_NLM_ADD_BACK, BIT_7, nlm_info.opt_mode << 7);

	ISP_REG_MWR(ISP_NLM_ADD_BACK_NEW0, 0x7F,
		nlm_info.addback_new[0]);
	ISP_REG_MWR(ISP_NLM_ADD_BACK_NEW0, (0x7F << 8),
		(nlm_info.addback_new[1] << 8));
	ISP_REG_MWR(ISP_NLM_ADD_BACK_NEW0, (0x7F << 16),
		(nlm_info.addback_new[2] << 16));
	ISP_REG_MWR(ISP_NLM_ADD_BACK_NEW0, (0x7F << 24),
		(nlm_info.addback_new[3] << 24));
	ISP_REG_MWR(ISP_NLM_ADD_BACK_NEW1, 0x7F,
		nlm_info.addback_new[4]);

	ISP_REG_MWR(ISP_NLM_DIRECTION_0, BIT_17
		| BIT_16, nlm_info.dist_mode << 16);

	val = ((nlm_info.w_shift[0] & 0x3) << 4)
		| ((nlm_info.w_shift[1] & 0x3) << 6)
		| ((nlm_info.w_shift[2] & 0x3) << 8);
	ISP_REG_MWR(ISP_NLM_DIRECTION_0, 0x3F0, val);

	ISP_REG_MWR(ISP_NLM_DIRECTION_0, 0x7, nlm_info.cnt_th);

	ISP_REG_MWR(ISP_NLM_DIRECTION_1, 0xFFFF0000,
		nlm_info.tdist_min_th << 16);

	ISP_REG_MWR(ISP_NLM_DIRECTION_1, 0xFFFF, nlm_info.diff_th);

	for (i = 0; i < 24; i++) {
		val = (nlm_info.lut_w[i*3+0] & 0x3FF)
			| ((nlm_info.lut_w[i*3+1] & 0x3FF) << 10)
			| ((nlm_info.lut_w[i*3+2] & 0x3FF) << 20);
		ISP_REG_WR(ISP_NLM_LUT_W_0 + i * 4, val);
	}

	if (isp_k_param->raw_nlm_buf_id) {
		isp_k_param->raw_nlm_buf_id = ISP_RAW_NLM_MOUDLE_BUF0;
		memcpy((void *)(ISP_NLM_BUF0_CH0 + ISP_BASE_ADDR), buff2,
			ISP_VST_IVST_NUM * sizeof(uint32_t));
		memcpy((void *)(ISP_VST_BUF0_CH0 + ISP_BASE_ADDR), buff0,
			ISP_VST_IVST_NUM * sizeof(uint32_t));
		memcpy((void *)(ISP_IVST_BUF0_CH0 + ISP_BASE_ADDR), buff1,
			ISP_VST_IVST_NUM * sizeof(uint32_t));
	} else {
		isp_k_param->raw_nlm_buf_id = ISP_RAW_NLM_MOUDLE_BUF1;
		memcpy((void *)(ISP_NLM_BUF1_CH0 + ISP_BASE_ADDR), buff2,
			ISP_VST_IVST_NUM * sizeof(uint32_t));
		memcpy((void *)(ISP_VST_BUF1_CH0 + ISP_BASE_ADDR), buff0,
			ISP_VST_IVST_NUM * sizeof(uint32_t));
		memcpy((void *)(ISP_IVST_BUF1_CH0 + ISP_BASE_ADDR), buff1,
			ISP_VST_IVST_NUM * sizeof(uint32_t));
	}

	ISP_REG_MWR(ISP_NLM_PARA, BIT_31, isp_k_param->raw_nlm_buf_id << 31);
	ISP_REG_MWR(ISP_VST_PARA, BIT_1, isp_k_param->raw_nlm_buf_id << 1);
	ISP_REG_MWR(ISP_IVST_PARA, BIT_1, isp_k_param->raw_nlm_buf_id << 1);

	ISP_REG_MWR(ISP_NLM_PARA, BIT_0, nlm_info.bypass);
	ISP_REG_MWR(ISP_IVST_PARA, BIT_0, nlm_info.bypass);
	ISP_REG_MWR(ISP_VST_PARA, BIT_0, nlm_info.bypass);

	vfree(buff0);
	vfree(buff1);
	vfree(buff2);
	return ret;
no_mem_err:
	pr_info("vmalloc err\n");
	return -1;
}

int32_t isp_k_cfg_nlm(struct isp_io_param *param,
	struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;

	if (!param) {
		pr_info("isp_k_cfg_nlm: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("isp_k_cfg_nlm: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_NLM_BLOCK:
		ret = isp_k_nlm_block(param, isp_k_param);
		break;
	default:
		pr_info("fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
