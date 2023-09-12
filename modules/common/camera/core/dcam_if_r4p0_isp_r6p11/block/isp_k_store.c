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
#define pr_fmt(fmt) "STORE: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

struct isp_store_info_inner {

	uint32_t store_base_addr;

	/*
	* param
	*/
	uint32_t  bypass;/*store  module control -- 0: work ; 1: bypass*/

	/*
	 * the three value below not used in store path,
	 * only used in store_pre, store_cap, store_vid
	 */

	/* 1: max burst length='hf,
	 * 0: max='h7 (only 1p/2p normal output support burst16)
	 */
	uint32_t  max_len_sel;

	/* 1: 2P speed;
	 * 0: 1p speed (store_cap must be 1'b1)
	 */
	uint32_t	speed_2x;

	/* 0: normal;  1: mirror enable */
	uint32_t	mirror_en;

	/* 1: only output Y data, support mono sensor ; 0: normal case */
	uint32_t	mono_en;

	uint32_t  endian;		/*reference store Endian type*/

	/*
	 * 0: oneplane    UYVY   422
	 * 1: twoplane    Y,UV   422
	 * 2: twoplane    Y,VU   422
	 * 3: threeplane  Y,U,V  422
	 * 4: twoplane    Y,UV   420
	 * 5: twoplane    Y,VU   420
	 * 6: threeplane  Y,U,V  420
	 * 7: normal RAW10
	 * 8: full RGB8
	 */
	uint32_t  color_format; /* same as color_format of fetch */

	struct isp_img_size size;
	struct store_border border;
	struct isp_addr_fs addr;
	struct isp_pitch_fs pitch;

	/*
	 * shadow clear mode select(for shadow interrut):
	 *	0: shadow done cleared by store eof
	 *	1: shadow done cleared by shadow_clr(bit0)
	 */
	uint16_t  shadow_clr_sel;

	/*
	 * toggle register: clear ISP shadow status, then FW can start
	 * config next slice(or frame) registers.
	 */
	uint16_t  shadow_clr;

	uint16_t  rd_ctrl;			/*default :0*/
	uint16_t  store_res;		/*default :1*/

	/*struct isp_slice_store_info slice_info_array[SLICE_NUM_MAX];*/
};

static struct isp_store_info_inner s_isp_store_info = {
	ISP_STORE_BASE, 0, 0, 0, 0, 0,
	0/*LITTLE_ENDIAN*/, 0/*ISP_STORE_UYVY*/,
	{0, 0},
	{0, 0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	1, 1, 0, 1,
};
static int isp_k_store_get_base(unsigned int sub_block,
	unsigned int *base)
{
	switch (sub_block) {
	case ISP_BLOCK_STORE:
		*base = ISP_STORE_PRE_CAP_BASE;
		break;
	case ISP_BLOCK_STORE_VID:
		*base = ISP_STORE_VID_BASE;
		break;
	case ISP_BLOCK_STORE_OUT:
		*base = ISP_STORE_BASE;
		break;
	default:
		pr_err("fail to support cmd id = %d\n", sub_block);
		return -EINVAL;
	}

	return 0;
}

static inline void isp_store_overwrite_info(
	struct isp_store_info_inner *store_info_inner,
	struct isp_dev_store_info *store_info)
{
	store_info_inner->bypass = store_info->bypass;
	store_info_inner->border = store_info->border;
	store_info_inner->size = store_info->size;
	store_info_inner->endian = store_info->endian;
	store_info_inner->color_format = store_info->color_format;
	store_info_inner->addr = store_info->addr;
	store_info_inner->pitch = store_info->pitch;
	store_info_inner->rd_ctrl = store_info->rd_ctrl;
	store_info_inner->shadow_clr = store_info->shadow_clr;
	store_info_inner->shadow_clr_sel = store_info->shadow_clr_sel;
	store_info_inner->store_res = store_info->store_res;
}

static int isp_store_block(struct isp_io_param *param,
		enum isp_id idx,
		unsigned int is_raw_cap)
{
	int  ret = 0;
	unsigned int val = 0;
	struct isp_dev_store_info store_info;
	struct isp_store_info_inner store_info_inner = s_isp_store_info;
	unsigned int isp_base_addr = ISP_STORE_PRE_CAP_BASE;

	isp_k_store_get_base(param->sub_block, &isp_base_addr);
	memset(&store_info, 0x00, sizeof(store_info));
	ret = copy_from_user((void *)&store_info,
		param->property_param, sizeof(store_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	isp_store_overwrite_info(&store_info_inner, &store_info);

	if (unlikely(is_raw_cap)) {
		ISP_REG_MWR(idx, isp_base_addr+ISP_STORE_PARAM, BIT_1,
			(store_info_inner.max_len_sel << 1));
		ISP_REG_MWR(idx, isp_base_addr+ISP_STORE_PARAM, BIT_2,
			(store_info_inner.speed_2x << 2));
		ISP_REG_MWR(idx, isp_base_addr+ISP_STORE_PARAM, BIT_3,
			(store_info_inner.mirror_en << 3));

		ISP_REG_MWR(idx, isp_base_addr+ISP_STORE_PARAM, 0xF0,
			(store_info_inner.color_format << 4));
		ISP_REG_MWR(idx, isp_base_addr+ISP_STORE_PARAM, 0x300,
			(store_info_inner.endian << 8));
		ISP_REG_MWR(idx, isp_base_addr+ISP_STORE_PARAM, 0x400,
			(store_info_inner.mono_en << 10));

		val = ((store_info_inner.size.height & 0xFFFF) << 16) |
			   (store_info_inner.size.width & 0xFFFF);
		ISP_REG_WR(idx, isp_base_addr+ISP_STORE_SLICE_SIZE, val);

		val = ((store_info_inner.border.right_border & 0xFF) << 24) |
			 ((store_info_inner.border.left_border  & 0xFF) << 16) |
			 ((store_info_inner.border.down_border  & 0xFF) << 8) |
			 (store_info_inner.border.up_border	& 0xFF);

		ISP_REG_WR(idx, isp_base_addr+ISP_STORE_BORDER, val);
		ISP_REG_WR(idx, isp_base_addr+ISP_STORE_SLICE_Y_ADDR,
			store_info_inner.addr.chn0);
		ISP_REG_WR(idx, isp_base_addr+ISP_STORE_SLICE_U_ADDR,
			store_info_inner.addr.chn1);
		ISP_REG_WR(idx, isp_base_addr+ISP_STORE_SLICE_V_ADDR,
			store_info_inner.addr.chn2);

		ISP_REG_MWR(idx, isp_base_addr+ISP_STORE_Y_PITCH,
			0xFFFF, store_info_inner.pitch.chn0);
		ISP_REG_MWR(idx, isp_base_addr+ISP_STORE_U_PITCH,
			0xFFFF, store_info_inner.pitch.chn1);
		ISP_REG_MWR(idx, isp_base_addr+ISP_STORE_V_PITCH,
			0xFFFF, store_info_inner.pitch.chn2);
	}

	ISP_REG_MWR(idx, isp_base_addr+ISP_STORE_READ_CTRL, 0x3,
		store_info_inner.rd_ctrl);
	ISP_REG_MWR(idx, isp_base_addr+ISP_STORE_READ_CTRL, 0xFFFFFFFC,
		store_info_inner.store_res << 2);

	ISP_REG_WR(idx, isp_base_addr+ISP_STORE_SHADOW_CLR_SEL,
		store_info_inner.shadow_clr_sel << 1);
	ISP_REG_WR(idx, isp_base_addr+ISP_STORE_SHADOW_CLR,
		store_info_inner.shadow_clr);

	return ret;
}

static int isp_k_store_slice_size
	(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_img_size size = {0, 0};
	unsigned int base = 0;

	ret = isp_k_store_get_base(param->sub_block, &base);
	if (ret == -1) {
		pr_err("fail to support cmd id = %d\n", param->sub_block);
		return -EPERM;
	}

	ret = copy_from_user((void *)&size,
		param->property_param, sizeof(size));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	val = ((size.height & 0xFFFF) << 16) | (size.width & 0xFFFF);
	ISP_REG_WR(idx, base+ISP_STORE_SLICE_SIZE, val);

	return ret;
}

int isp_k_cfg_store(struct isp_io_param *param,
		enum isp_id idx, unsigned int is_raw_cap)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param.\n");
		return -EINVAL;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property param.\n");
		return -EINVAL;
	}

	switch (param->property) {
	case ISP_PRO_STORE_BLOCK:
		ret = isp_store_block(param, idx, is_raw_cap);
		break;
	case ISP_PRO_STORE_SLICE_SIZE:
		if (unlikely(is_raw_cap))
			ret = isp_k_store_slice_size(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}
