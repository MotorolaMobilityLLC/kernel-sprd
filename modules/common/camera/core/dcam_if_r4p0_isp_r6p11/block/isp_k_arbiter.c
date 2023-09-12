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
#define pr_fmt(fmt) "ARBITER: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

struct arbiter_endian {
	/*
	 *ISP_ARBITER_ENDIAN_COMM
	 */
	uint32_t  bpc_wr_endian;/* bpc reference endian */
	uint32_t  binning_endian;
	uint32_t  afl_endian;
	uint32_t  aem_endian;
	uint32_t  cfg_wr_endian;
	uint32_t  fmcu_endian;
	uint32_t  bpc_rd_endian;
	uint32_t  lens_endian;
	uint32_t  cfg_rd_endian;

	/*
	 *ISP_ARBITER_ENDIAN_CH0
	 */
	uint32_t  fetch_raw_word_change;
	uint32_t  fetch_raw_bit_reorder;
	uint32_t  fetch_raw_endian;
};

struct isp_arbiter_info_inner {
	struct arbiter_endian endian;
	uint32_t  qos_wr[16];
	uint32_t  rf_pri_wr[16];
	uint32_t  rf_timeout_thr_wr[16];
	uint32_t  qos_rd[16];
	uint32_t  rf_pri_rd[16];
	uint32_t  rf_timeout_thr_rd[16];
};

static struct isp_arbiter_info_inner s_arbiter_info_inner = {
	{1/* big */, 1, 1, 1, 1, 1, 1, 1/* lsc, big */, 0, 0, 0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0,	0x0, 0x0, 0x0, 0x0},
	{0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF,
		0xF, 0xF, 0xF, 0xF,	0xF, 0xF, 0xF, 0xF},
	{0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
		0x10, 0x10, 0x10, 0x10,	0x10, 0x10, 0x10, 0x10},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0,	0x0, 0x0, 0x0, 0x0},
	{0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF,
		0xF, 0xF, 0xF, 0xF,	0xF, 0xF, 0xF, 0xF},
	{0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
		0x10, 0x10, 0x10, 0x10,	0x10, 0x10, 0x10, 0x10},
};

static inline void isp_arbiter_overwrite_info(
	struct isp_arbiter_info_inner *arbiter_info_inner,
	struct isp_dev_arbiter_info *arbiter_info)
{
	arbiter_info_inner->endian.fetch_raw_endian =
		arbiter_info->fetch_raw_endian;

	arbiter_info_inner->endian.fetch_raw_bit_reorder =
		arbiter_info->fetch_bit_reorder;

	arbiter_info_inner->endian.fetch_raw_word_change =
		arbiter_info->fetch_raw_word_change;
}

static int isp_k_arbiter_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	struct isp_dev_arbiter_info arbiter_info;
	struct isp_arbiter_info_inner arbiter_info_inner = s_arbiter_info_inner;
	int i;
	unsigned int val = 0;

	memset(&arbiter_info, 0x00, sizeof(arbiter_info));

	ret = copy_from_user((void *)&arbiter_info, param->property_param,
			sizeof(arbiter_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	isp_arbiter_overwrite_info(&arbiter_info_inner, &arbiter_info);
	val = ((arbiter_info_inner.endian.bpc_wr_endian		& 0x1) << 21) |
		  ((arbiter_info_inner.endian.binning_endian	& 0x1) << 20) |
		  ((arbiter_info_inner.endian.afl_endian	& 0x1) << 18) |
		  ((arbiter_info_inner.endian.aem_endian	& 0x1) << 17) |
		  ((arbiter_info_inner.endian.cfg_wr_endian	& 0x1) << 16) |
		  ((arbiter_info_inner.endian.fmcu_endian	& 0x1) << 3) |
		  ((arbiter_info_inner.endian.bpc_rd_endian	& 0x1) << 2) |
		  ((arbiter_info_inner.endian.lens_endian	& 0x1) << 1) |
		  (arbiter_info_inner.endian.cfg_rd_endian	& 0x1);

	ISP_HREG_WR(idx, ISP_ARBITER_ENDIAN_COMM, val);

	val = ((arbiter_info_inner.endian.fetch_raw_word_change & 0x1) << 3) |
	  ((arbiter_info_inner.endian.fetch_raw_bit_reorder & 0x1) << 2) |
	  (arbiter_info_inner.endian.fetch_raw_endian & 0x3);
	ISP_HREG_WR(idx, ISP_ARBITER_ENDIAN_CH0, val);

	for (i = 0; i < 16; i += 2) {
		val = ((arbiter_info_inner.qos_wr[i] & 0xF) << 28) |
		  ((arbiter_info_inner.rf_pri_wr[i] & 0xF) << 24) |
		  ((arbiter_info_inner.rf_timeout_thr_wr[i] & 0xFF) << 16) |
		  ((arbiter_info_inner.qos_wr[i+1] & 0xF)	<< 12) |
		  ((arbiter_info_inner.rf_pri_wr[i+1] & 0xF) << 8)  |
		  (arbiter_info_inner.rf_timeout_thr_wr[i+1] & 0xFF);
		ISP_HREG_WR(idx, ISP_ARBITER_WR_PARAM0 + 4*(i>>1), val);

		val = ((arbiter_info_inner.qos_rd[i] & 0xF) << 28) |
		((arbiter_info_inner.rf_pri_rd[i] & 0xF) << 24) |
		((arbiter_info_inner.rf_timeout_thr_rd[i] & 0xFF) << 16) |
		((arbiter_info_inner.qos_rd[i+1] & 0xF) << 12) |
		((arbiter_info_inner.rf_pri_rd[i+1] & 0xF) << 8) |
		(arbiter_info_inner.rf_timeout_thr_rd[i+1] & 0xFF);
		ISP_HREG_WR(idx, ISP_ARBITER_RD_PARAM0 + 4*(i>>1), val);
	}

	return ret;
}

int isp_k_cfg_arbiter(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param\n");
		return -EPERM;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param\n");
		return -EPERM;
	}

	switch (param->property) {
	case ISP_PRO_ARBITER_BLOCK:
		ret = isp_k_arbiter_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}

