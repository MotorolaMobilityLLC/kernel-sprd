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
#include "isp_buf.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "FETCH: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

struct isp_fetch_info_inner {
	/* param0 */
	uint32_t  bypass;
	/* 0:nothing 1:subtract 128 */
	uint32_t  subtract;

	/*
	 * 0: threeplane    Y, U, V  422
	 * 1: oneplane    YUYV  422
	 * 2: oneplane    UYVY  422
	 * 3: oneplane    YVYU  422
	 * 4: oneplane    VYUY  422
	 * 5: twoplane    Y,UV   422
	 * 6: twoplane    Y,VU   422
	 * 7: normal RAW10
	 * 8: CSI-2 RAW10
	 * 9: full RGB10(only for channel 0)
	 * 10: twoplane    Y,UV   420
	 * 11: twoplane    Y,VU   420
	 */
	uint32_t  color_format;

	/*
	 * need description!!!!
	 * fetch2 of sharkl2
	 */
	uint32_t ft0_axi_reorder_en;
	uint32_t ft1_axi_reorder_en;
	uint32_t ft2_axi_reorder_en;
	uint32_t chk_sum_clr_en;
	uint32_t first_line_mode;
	uint32_t last_line_mode;

	struct isp_img_size size;
	struct isp_addr_fs addr;
	struct isp_pitch_fs pitch;

	/* the word number of a row in mipi input format */
	uint32_t  mipi_word_num;
	/* the relative position of 1st pixel in first 5 word of each row */
	uint32_t  mipi_byte_rel_pos;

	/* line dly ctrl */
	uint32_t  hblank_num;

	/* param1 */
	struct	isp_addr_fs retain_num;
	struct	isp_addr_fs max_len_sel;

	/* fetch start */
	uint32_t  start_isp;

	/*struct isp_slice_fetch_info slice_info_array[SLICE_NUM_MAX];*/
};

static struct isp_fetch_info_inner s_isp_fetch_info = {
	0, 0, 0/*ISP_FETCH_YUV422_3FRAME*/, 0, 0, 0, 0, 0, 0, {0, 0},
	{0, 0, 0},
	{0, 0, 0},
	0, 0, 0x200, {0, 0, 0}, {1, 1, 1}, 1,
};

static void isp_fetch_overwrite_info(
	struct isp_fetch_info_inner *fetch_info_inner,
	struct isp_dev_fetch_info *fetch_info)
{
	fetch_info_inner->addr.chn0 = fetch_info->addr.chn0;/*todo*/
	fetch_info_inner->addr.chn1 = fetch_info->addr.chn1;/*todo*/
	fetch_info_inner->addr.chn2 = fetch_info->addr.chn2;/*todo*/

	fetch_info_inner->addr = fetch_info->addr;/*todo*/
	fetch_info_inner->bypass = fetch_info->bypass;
	fetch_info_inner->color_format = fetch_info->color_format;
	fetch_info_inner->mipi_byte_rel_pos = fetch_info->mipi_byte_rel_pos;
	fetch_info_inner->mipi_word_num = fetch_info->mipi_word_num;
	fetch_info_inner->pitch = fetch_info->pitch;
	fetch_info_inner->subtract = fetch_info->subtract;
	fetch_info_inner->start_isp = fetch_info->start_isp;
	fetch_info_inner->size = fetch_info->size;
}

unsigned int isp_k_fetch_get_raw_info(unsigned int *width,
			unsigned int *height)
{

	*width = s_isp_fetch_info.size.width;
	*height = s_isp_fetch_info.size.height;
	return s_isp_fetch_info.addr.chn0;
}

static int isp_k_fetch_raw_block(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;
	struct isp_dev_fetch_info fetch_info;
	struct isp_fetch_info_inner fetch_info_inner = s_isp_fetch_info;
	unsigned int size = 0;
	uint32_t val;

	memset(&fetch_info, 0x00, sizeof(fetch_info));
	ret = copy_from_user((void *)&fetch_info,
		param->property_param, sizeof(fetch_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}
	fetch_info.addr.chn0 = isp_k_param->fetch_raw_phys_addr;
	s_isp_fetch_info.addr.chn0 = isp_k_param->fetch_raw_phys_addr;
	s_isp_fetch_info.size.height = fetch_info.size.height;
	s_isp_fetch_info.size.width = fetch_info.size.width;

	isp_fetch_overwrite_info(&fetch_info_inner, &fetch_info);

	ISP_REG_MWR(idx, ISP_FETCH_PARAM0, BIT_0, fetch_info_inner.bypass);
	if (fetch_info_inner.bypass) {
		return 0;
	}

	val =	((fetch_info_inner.last_line_mode & 0x1) << 13)|
			((fetch_info_inner.first_line_mode & 0x1) << 12) |
			((fetch_info_inner.chk_sum_clr_en & 0x1) << 11) |
			((fetch_info_inner.ft2_axi_reorder_en & 0x1) << 10) |
			((fetch_info_inner.ft1_axi_reorder_en & 0x1) << 9) |
			((fetch_info_inner.ft0_axi_reorder_en & 0x1) << 8) |
			((fetch_info_inner.color_format & 0xF) << 4) |
			((fetch_info_inner.subtract & 0x1) << 1);

	ISP_REG_WR(idx, ISP_FETCH_PARAM0, val);

	size = ((fetch_info_inner.size.height & 0xFFFF) << 16) |
			(fetch_info_inner.size.width & 0xFFFF);
	ISP_REG_WR(idx, ISP_FETCH_MEM_SLICE_SIZE, size);
	ISP_REG_WR(idx, ISP_FETCH_SLICE_Y_ADDR, fetch_info_inner.addr.chn0);
	ISP_REG_WR(idx, ISP_FETCH_Y_PITCH, fetch_info_inner.pitch.chn0);
	ISP_REG_WR(idx, ISP_FETCH_SLICE_U_ADDR, fetch_info_inner.addr.chn1);
	ISP_REG_WR(idx, ISP_FETCH_U_PITCH, fetch_info_inner.pitch.chn1);
	ISP_REG_WR(idx, ISP_FETCH_SLICE_V_ADDR, fetch_info_inner.addr.chn2);
	ISP_REG_WR(idx, ISP_FETCH_V_PITCH, fetch_info_inner.pitch.chn2);

	/*mipi param*/
	val =	((fetch_info_inner.mipi_byte_rel_pos & 0xF) << 16)|
			(fetch_info_inner.mipi_word_num & 0xFFFF);
	ISP_REG_WR(idx, ISP_FETCH_MIPI_PARAM, val);

	/*dly ctrl*/
	ISP_REG_MWR(idx, ISP_FETCH_LINE_DLY_CTRL, 0xffff,
		fetch_info_inner.hblank_num);

	/*param 1*/
	val =	((fetch_info_inner.max_len_sel.chn0 & 0x1) << 23)|
			((fetch_info_inner.retain_num.chn0 & 0x7F) << 16) |
			((fetch_info_inner.max_len_sel.chn1 & 0x1) << 15) |
			((fetch_info_inner.retain_num.chn1 & 0x7F) << 8) |
			((fetch_info_inner.max_len_sel.chn2 & 0x1) << 7) |
			(fetch_info_inner.retain_num.chn2 & 0x7F);

	ISP_REG_WR(idx, ISP_FETCH_PARAM1, val);

	return ret;
}

static int isp_k_fetch_start(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int start = 0;

	ret = copy_from_user((void *)&start,
		param->property_param, sizeof(unsigned int));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_REG_WR(idx, ISP_FETCH_START, start);

	return ret;
}

static int isp_k_fetch_slice_size(struct isp_io_param *param,
							enum isp_id idx)
{
	int ret = 0;
	unsigned int size = 0;
	struct isp_img_size slice_size = {0, 0};

	ret = copy_from_user((void *)&slice_size,
		param->property_param, sizeof(struct isp_img_size));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	size = ((slice_size.height & 0xFFFF) << 16)
		| (slice_size.width & 0xFFFF);

	ISP_REG_WR(idx, ISP_FETCH_MEM_SLICE_SIZE, size);

	return ret;
}

static int isp_k_fetch_transaddr(struct isp_io_param *param,
			struct isp_k_block *isp_k_param)
{
	int ret = 0;
	struct isp_dev_block_addr fetch_buf;
	struct isp_statis_buf fetch_remap_buf;

	memset(&fetch_buf, 0x00, sizeof(struct isp_dev_block_addr));
	memset(&fetch_remap_buf, 0x00, sizeof(struct isp_statis_buf));
	ret = copy_from_user(&fetch_buf, param->property_param,
				sizeof(fetch_buf));

	fetch_remap_buf.pfinfo.dev = &s_isp_pdev->dev;
	fetch_remap_buf.pfinfo.mfd[0] = fetch_buf.img_fd;
	/*mapping iommu buffer*/
	ret = pfiommu_get_sg_table(&fetch_remap_buf.pfinfo);
	if (ret) {
		pr_err("fail to map iommu fetch buffer!\n");
		ret = -1;
		return ret;
	}

	ret = pfiommu_get_addr(&fetch_remap_buf.pfinfo);
	if (ret) {
		pr_err("fail to get fetch remap buf!\n");
		return ret;
	}
	isp_k_param->fetch_pfinfo.iova[0] =
		fetch_remap_buf.pfinfo.iova[0];
	isp_k_param->fetch_raw_phys_addr = fetch_remap_buf.pfinfo.iova[0]
		+ fetch_buf.img_offset.chn0;
	isp_k_param->fetch_pfinfo.size[0] = fetch_remap_buf.pfinfo.size[0];
	isp_k_param->fetch_pfinfo.dev = &s_isp_pdev->dev;

	pr_debug("fetch addr 0x%lx, size 0x%zx\n",
		isp_k_param->fetch_pfinfo.iova[0],
		fetch_remap_buf.pfinfo.size[0]);

	return ret;
}

int isp_k_cfg_fetch(struct isp_io_param *param,
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
	case ISP_PRO_FETCH_RAW_BLOCK:
		ret = isp_k_fetch_raw_block(param, isp_k_param, idx);
		break;
	case ISP_PRO_FETCH_START:
		ret = isp_k_fetch_start(param, idx);
		break;
	case ISP_PRO_FETCH_SLICE_SIZE:
		ret = isp_k_fetch_slice_size(param, idx);
		break;
	case ISP_PRO_FETCH_TRANSADDR:
		ret = isp_k_fetch_transaddr(param, isp_k_param);
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}
