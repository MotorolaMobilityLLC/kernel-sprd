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
#define pr_fmt(fmt) "COMMON: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

struct isp_common_info_inner {
	/*
	 * 	ISP_COMM_SPACE_SEL
	 */
	uint32_t fetch_color_space_sel;
	uint32_t store_color_space_sel;
	uint32_t ch0_path_ctrl;/*control for HW , auto_shadow */

	/*
	 *	ISP_COMM_BIN_AFL_CTRL
	 */
	/*AWBM&BINNING src data, 0:from 2d lens, 1:1d lens*/
	uint32_t  bin_pos_sel;
	uint32_t  afl_version_sel_ch0;

	/*
	 *	ISP_PMU_PMU_RAM_MASK
	 */
	uint32_t  ram_mask;/* enable PMU control of RAM */

	/*
	 *	ISP_GCLK_CTRL_0,1,2,3
	 */
	uint32_t  gclk_ctrl_0;
	uint32_t  gclk_ctrl_1;
	uint32_t  gclk_ctrl_2;
	uint32_t  gclk_ctrl_3;

	/*
	 *  ISP_SHADOW_CTRL_CH0
	 */
	uint32_t shadow_mctrl;

	/*
	 *	ISP_COMM_SCL_PATH_SEL
	 */
	uint32_t store_out_path_sel;
	uint32_t scl_pre_cap_path_sel;
	uint32_t scl_vid_path_sel;

	/*
	 *	ISP_COMM_SOFT_RST_CTRL
	 */
	uint32_t  isp_soft_rst;
	uint32_t  isp_cfg_sof_rst;

	uint32_t idx_mod_sel;
};

static struct isp_common_info_inner s_common_info_inner = {
	0, 0x0, 0, 1, 0,
	0, 0xffff0000, 0xffff0000, 0xffff0000, 0xff00,
	0x1,
	0, 0x3, 0x3,
	0,
};

static inline void isp_common_overwrite_info(
	struct isp_common_info_inner *common_info_inner,
	struct isp_dev_common_info *common_info)
{
	common_info_inner->fetch_color_space_sel =
		common_info->fetch_color_space_sel;

	common_info_inner->store_color_space_sel =
		common_info->store_color_space_sel;

}

static int isp_k_common_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val;
	struct isp_dev_common_info common_info;
	struct isp_common_info_inner common_info_inner = s_common_info_inner;

	memset(&common_info, 0x00, sizeof(common_info));
	ret = copy_from_user((void *)&common_info, param->property_param,
			sizeof(common_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	isp_common_overwrite_info(&common_info_inner, &common_info);

	val = ((common_info_inner.ch0_path_ctrl & 0x1) << 4) |
		  ((common_info_inner.store_color_space_sel & 0x3) << 2) |
		   (common_info_inner.fetch_color_space_sel & 0x3);
	ISP_HREG_WR(idx, ISP_COMMON_SPACE_SEL, val);

	val = common_info_inner.bin_pos_sel & 0x1;
	ISP_HREG_MWR(idx, ISP_COMMON_BIN_AFL_CTRL, BIT_0, val);

	ISP_HREG_MWR(idx, ISP_COMMON_PMU_RAM_MASK, BIT_0,
		common_info_inner.ram_mask);

	isp_clk_gt.g0 = common_info_inner.gclk_ctrl_0;
	isp_clk_gt.g1 = common_info_inner.gclk_ctrl_1;
	isp_clk_gt.g2 = common_info_inner.gclk_ctrl_2;
	isp_clk_gt.g3 = common_info_inner.gclk_ctrl_3;
	ISP_HREG_WR(idx, ISP_COMMON_GCLK_CTRL_0, common_info_inner.gclk_ctrl_0);
	ISP_HREG_WR(idx, ISP_COMMON_GCLK_CTRL_1, common_info_inner.gclk_ctrl_1);
	ISP_HREG_WR(idx, ISP_COMMON_GCLK_CTRL_2, common_info_inner.gclk_ctrl_2);
	ISP_HREG_WR(idx, ISP_COMMON_GCLK_CTRL_3, common_info_inner.gclk_ctrl_3);

	val = ((common_info_inner.store_out_path_sel & 0x3) << 6) |
		  ((common_info_inner.scl_vid_path_sel   & 0x3) << 2) |
		   (common_info_inner.scl_pre_cap_path_sel   & 0x3);
	ISP_HREG_MWR(idx, ISP_COMMON_SCL_PATH_SEL, 0xCF, val);

	ISP_HREG_MWR(idx, ISP_COMMON_SHADOW_CTRL_CH0, BIT_16,
		common_info_inner.shadow_mctrl << 16);

	return ret;
}

int isp_k_cfg_common(struct isp_io_param *param,
		enum isp_id idx, unsigned int is_raw_cap)
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
	case ISP_PRO_COMMON_BLOCK:
		if (unlikely(is_raw_cap))
			ret = isp_k_common_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}
