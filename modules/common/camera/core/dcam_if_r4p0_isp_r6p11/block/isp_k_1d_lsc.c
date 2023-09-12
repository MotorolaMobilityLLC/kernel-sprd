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
#define pr_fmt(fmt) "RLSC: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define	ISP_RLSC_BUF0		           0
#define	ISP_RLSC_BUF1		           1
#define ISP_PINGPANG_RLSC_NUM		   256

static int isp_k_rlsc_block(struct isp_io_param *param,
			struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_dev_radial_lsc_info rlsc_info;
	unsigned long dst_addr = 0;
	void *data_ptr = NULL;
	unsigned short *w_buff = NULL;

	memset(&rlsc_info, 0x00, sizeof(rlsc_info));
	ret = copy_from_user((void *)&rlsc_info, param->property_param,
				sizeof(rlsc_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_REG_MWR(idx, ISP_RLSC_CTRL, BIT_0, rlsc_info.bypass);
	if (rlsc_info.bypass) {
		isp_dbg_s_ori_byp(SBLK_BYPASS, raw_rlsc, idx);
		pr_debug("rlsc bypass.\n");
		return 0;
	}

	val = (rlsc_info.radius_step & 0x7) << 0x1;
	ISP_REG_MWR(idx, ISP_RLSC_CTRL, (0x7 << 0x1), val);

	val = ((rlsc_info.center_r0c0_pos.x & 0x1fff) << 12) |
			(rlsc_info.center_r0c0_pos.y & 0xfff);
	ISP_REG_WR(idx, ISP_RLSC_CFG0, val);

	val = ((rlsc_info.center_r0c1_pos.x & 0x1fff) << 12) |
			(rlsc_info.center_r0c1_pos.y & 0xfff);
	ISP_REG_WR(idx, ISP_RLSC_CFG1, val);

	val = ((rlsc_info.center_r1c0_pos.x & 0x1fff) << 12) |
			(rlsc_info.center_r1c0_pos.y & 0xfff);
	ISP_REG_WR(idx, ISP_RLSC_CFG2, val);

	val = ((rlsc_info.center_r1c1_pos.x & 0x1fff) << 12) |
			(rlsc_info.center_r1c1_pos.y & 0xfff);
	ISP_REG_WR(idx, ISP_RLSC_CFG3, val);

	val = ((rlsc_info.start_pos.x & 0xffff) << 16) |
			(rlsc_info.start_pos.y & 0xffff);
	ISP_REG_WR(idx, ISP_RLSC_CFG4, 0);

	val = ((rlsc_info.r_cfg.init_r0c1 & 0x1fff) << 13) |
			(rlsc_info.r_cfg.init_r0c0 & 0x1fff);
	ISP_REG_WR(idx, ISP_RLSC_R_CFG0, val);

	val = ((rlsc_info.r_cfg.init_r1c1 & 0x1fff) << 13) |
			(rlsc_info.r_cfg.init_r1c0 & 0x1fff);
	ISP_REG_WR(idx, ISP_RLSC_R_CFG1, val);

	ISP_REG_WR(idx, ISP_RLSC_R2_CFG0,
			(rlsc_info.r2_cfg.init_r0c0 & 0x3ffffff));
	ISP_REG_WR(idx, ISP_RLSC_R2_CFG1,
			(rlsc_info.r2_cfg.init_r0c1 & 0x3ffffff));
	ISP_REG_WR(idx, ISP_RLSC_R2_CFG2,
			(rlsc_info.r2_cfg.init_r1c0 & 0x3ffffff));
	ISP_REG_WR(idx, ISP_RLSC_R2_CFG3,
			(rlsc_info.r2_cfg.init_r1c1 & 0x3ffffff));

	ISP_REG_WR(idx, ISP_RLSC_DR2_CFG0,
			(rlsc_info.dr2_cfg.init_r0c0 & 0x3ffffff));
	ISP_REG_WR(idx, ISP_RLSC_DR2_CFG1,
			(rlsc_info.dr2_cfg.init_r0c1 & 0x3ffffff));
	ISP_REG_WR(idx, ISP_RLSC_DR2_CFG2,
			(rlsc_info.dr2_cfg.init_r1c0 & 0x3ffffff));
	ISP_REG_WR(idx, ISP_RLSC_DR2_CFG3,
			(rlsc_info.dr2_cfg.init_r1c1 & 0x3ffffff));

#ifdef CONFIG_64BIT
	data_ptr = (void *)(((unsigned long)rlsc_info.data_ptr[1] << 32)
					| rlsc_info.data_ptr[0]);
#else
	data_ptr = (void *)(rlsc_info.data_ptr[0]);
#endif

	if (ISP_GET_MID(idx) == ISP_AP_MODE) {

		if (isp_k_param->rlsc_buf_id == ISP_RLSC_BUF1) {
			dst_addr = ISP_BASE_ADDR(idx) +
					ISP_1D_LENS_GR_BUF0_CH0;
			isp_k_param->rlsc_buf_id = ISP_RLSC_BUF0;
		} else {
			dst_addr = ISP_BASE_ADDR(idx) +
					ISP_1D_LENS_GR_BUF1_CH0;
			isp_k_param->rlsc_buf_id = ISP_RLSC_BUF1;
		}
	} else {
	/*
	 * In CFG MODE, we don't need to select parameter buffer.
	 * Just let HW to pick the correct buffer by itself.
	 * DO NOT use buffer 1 or it will cause capture frame
	 * all black problem.
	 *
	 */
		dst_addr = ISP_BASE_ADDR(idx) +
				ISP_1D_LENS_GR_BUF0_CH0;
		isp_k_param->rlsc_buf_id = ISP_RLSC_BUF0;
	}

	w_buff = vzalloc(ISP_PINGPANG_RLSC_NUM * 4 * sizeof(unsigned int));
	if (w_buff == NULL)
		return -ENOMEM;

	ret = copy_from_user((void *)w_buff,
			(const void __user *)data_ptr,
			ISP_PINGPANG_RLSC_NUM * 4 * sizeof(unsigned int));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		vfree(w_buff);
		return -EPERM;
	}

	*(unsigned int *)dst_addr = (unsigned int)(*w_buff);
	vfree(w_buff);

	ISP_REG_MWR(idx, ISP_RLSC_CTRL, BIT_4, isp_k_param->rlsc_buf_id << 4);

	return ret;
}

int isp_k_cfg_rlsc(struct isp_io_param *param,
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
	case ISP_PRO_RLSC_BLOCK:
		ret = isp_k_rlsc_block(param, isp_k_param, idx);
		break;
	default:
		pr_err("fail to supported cmd = %d\n", param->property);
		break;
	}

	return ret;
}
