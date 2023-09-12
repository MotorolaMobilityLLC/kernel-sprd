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
#include <linux/slab.h>

#include "sprd_mm.h"
#include "sprd_isp_hw.h"
#include "dcam_block.h"
#include "dcam_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "PDAF: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

#define PDAF_PATTERN_PIXEL_NUM 4
#define PD_TABLE_SIZE 1024
#define PIXEL_PER_ADDR 16

static void write_pd_table(struct pdaf_ppi_info *pdaf_info, enum dcam_id idx)
{
	int i;
	int line_start_num, inner_linenum, line_offset;
	int col, row, is_PD, is_right, bit_start;
	int x = 16;
	int pdafinfo[16*16*2/8];

	memset(pdafinfo, 0, sizeof(int) * (16*16*2/8));
	for (i = 0; i < PDAF_PATTERN_PIXEL_NUM; i++) {
		col = pdaf_info->pattern_pixel_col[i];
		row = pdaf_info->pattern_pixel_row[i];
		is_right = pdaf_info->pattern_pixel_is_right[i] & 0x01;
		is_PD = 1;
		line_start_num = row*(x/16);
		inner_linenum = (col/16);
		line_offset = (col%16);
		bit_start = 30 - 2 * line_offset;
		pdafinfo[line_start_num + inner_linenum] |=
				((is_PD | (is_right << 1)) << bit_start);
	}
	for (i = 0; i < 64; i++)
		DCAM_REG_WR(idx, DCAM0_PDAF_EXTR_POS+(i*4), pdafinfo[i]);
}

static int pd_info_to_ppi_info(struct isp_dev_pdaf_info *pdaf_info,
		struct pdaf_ppi_info *ppi_info)
{
	if (!pdaf_info || !ppi_info)
		return -1;
	memcpy(&ppi_info->pattern_pixel_is_right[0],
			&pdaf_info->pattern_pixel_is_right[0],
			sizeof(pdaf_info->pattern_pixel_is_right));
	memcpy(&ppi_info->pattern_pixel_row[0],
			&pdaf_info->pattern_pixel_row[0],
			sizeof(pdaf_info->pattern_pixel_row));
	memcpy(&ppi_info->pattern_pixel_col[0],
			&pdaf_info->pattern_pixel_col[0],
			sizeof(pdaf_info->pattern_pixel_col));
	return 0;
}

static int dcam_k_pdaf_block(struct isp_io_param *param, enum dcam_id idx,
					struct sprd_pdaf_control *pdaf_ctrl)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_dev_pdaf_info pdaf_info;
	struct pdaf_ppi_info ppi_info;

	memset(&pdaf_info, 0x00, sizeof(pdaf_info));
	ret = copy_from_user((void *)&pdaf_info,
		param->property_param, sizeof(pdaf_info));
	if (ret != 0) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return -EPERM;
	}
	val = !pdaf_info.bypass;
	DCAM_REG_MWR(idx, DCAM0_CFG, BIT_3, val);
	if (pdaf_ctrl->mode == E_PDAF_TYPE3)
		DCAM_REG_MWR(idx, DCAM0_CFG, BIT_4, val);

	val = ((pdaf_info.block_size.height & 0x3) << 3)
		| ((pdaf_info.block_size.width & 0x3) << 1);
	DCAM_REG_MWR(idx, DCAM0_PDAF_EXTR_CTRL, 0x1E, val);

	val = ((pdaf_info.win.start_y & 0x1FFF) << 13) |
		(pdaf_info.win.start_x & 0x1FFF);
	DCAM_REG_WR(idx, DCAM0_PDAF_EXTR_ROI_ST, val);

	val = (((pdaf_info.win.end_y - pdaf_info.win.start_y) & 0x1FFF) << 13) |
		((pdaf_info.win.end_x - pdaf_info.win.start_x) & 0x1FFF);
	DCAM_REG_WR(idx, DCAM0_PDAF_EXTR_ROI_SIZE, val);

	DCAM_REG_WR(idx, DCAM0_PDAF_BASE_WADDR, pdaf_info.phase_left_addr);
	if (pdaf_ctrl->mode == E_PDAF_TYPE3) {
		DCAM_REG_WR(idx, DCAM0_VCH2_BASE_WADDR,
						pdaf_info.phase_right_addr);
	}
	ret = pd_info_to_ppi_info(&pdaf_info, &ppi_info);
	if (!ret)
		write_pd_table(&ppi_info, idx);

	return ret;
}
static int dcam_k_pdaf_bypass(struct isp_io_param *param,
			enum dcam_id idx, struct sprd_pdaf_control *pdaf_ctrl)
{
	int ret = 0;
	unsigned int bypass = 0;
	unsigned int val = 0;

	ret = copy_from_user((void *)&bypass,
		param->property_param, sizeof(unsigned int));
	if (ret != 0) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return -EPERM;
	}
	val = !bypass;

	DCAM_REG_MWR(idx, DCAM0_CFG, BIT_3, val);
	DCAM_REG_MWR(idx, DCAM0_CONTROL, BIT_14, 1);

	if (pdaf_ctrl->mode == E_PDAF_TYPE3) {
		DCAM_REG_MWR(idx, DCAM0_CFG, BIT_4, val);
		DCAM_REG_MWR(idx, DCAM0_CONTROL, BIT_16, 1);
	}

	return ret;
}
static int dcam_k_pdaf_addr(struct isp_io_param *param,	enum dcam_id idx,
					struct sprd_pdaf_control *pdaf_ctrl)
{
	int ret = 0;
	struct pdaf_addr_info pdaf_addr;

	memset(&pdaf_addr, 0x00, sizeof(pdaf_addr));

	ret = copy_from_user((void *)&pdaf_addr,
		param->property_param, sizeof(struct pdaf_addr_info));
	if (ret != 0) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return -EPERM;
	}

	DCAM_REG_WR(idx, DCAM0_PDAF_BASE_WADDR, pdaf_addr.addr_l);
	if (pdaf_ctrl->mode == E_PDAF_TYPE3)
		DCAM_REG_WR(idx, DCAM0_VCH2_BASE_WADDR, pdaf_addr.addr_r);

	return ret;
}

static int dcam_k_pdaf_set_ppi_info(struct isp_io_param *param,
		enum dcam_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct pdaf_ppi_info pdaf_info;

	memset(&pdaf_info, 0x00, sizeof(pdaf_info));
	ret = copy_from_user((void *)&pdaf_info,
		param->property_param, sizeof(pdaf_info));
	if (ret != 0) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return -EPERM;
	}
	val = ((pdaf_info.block_size.height & 0x3) << 3)
		| ((pdaf_info.block_size.width & 0x3) << 1);
	DCAM_REG_MWR(idx, DCAM0_PDAF_EXTR_CTRL, 0x1E, val);
	write_pd_table(&pdaf_info, idx);

	return ret;
}
static int dcam_k_pdaf_set_roi(struct isp_io_param *param, enum dcam_id idx)
{

	int ret = 0;
	unsigned int val = 0;
	struct pdaf_roi_info roi_info;

	memset(&roi_info, 0x00, sizeof(roi_info));
	ret = copy_from_user((void *)&roi_info,
		param->property_param, sizeof(roi_info));
	if (ret != 0) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return -EPERM;
	}
	val = ((roi_info.win.start_y & 0x1FFF) << 13) |
		(roi_info.win.start_x & 0x1FFF);
	DCAM_REG_WR(idx, DCAM0_PDAF_EXTR_ROI_ST, val);

	val = (((roi_info.win.end_y - roi_info.win.start_y) & 0x1FFF) << 13) |
		((roi_info.win.end_x - roi_info.win.start_x) & 0x1FFF);
	DCAM_REG_WR(idx, DCAM0_PDAF_EXTR_ROI_SIZE, val);

	return ret;
}
static int dcam_k_pdaf_set_skip_num(struct isp_io_param *param,
		enum dcam_id idx)
{
	int ret = 0;
	unsigned int skip_num = 0;

	ret = copy_from_user((void *)&skip_num,
			param->property_param, sizeof(skip_num));
	if (ret != 0) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return -EPERM;
	}

	DCAM_REG_MWR(idx, DCAM0_PDAF_SKIP_FRM,
				BIT_6 | BIT_5 | BIT_4 | BIT_3, skip_num);

	return ret;
}

static int dcam_k_pdaf_set_mode(struct isp_io_param *param, enum dcam_id idx)
{
	int ret = 0;
	int val = 0;
	unsigned int work_mode = 0;

	ret = copy_from_user((void *)&work_mode,
			param->property_param, sizeof(work_mode));
	if (ret != 0) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return -EPERM;
	}
	val = (work_mode & 0x3) | BIT_2;

	DCAM_REG_MWR(idx, DCAM0_PDAF_SKIP_FRM, BIT_0 | BIT_1 | BIT_2, val);

	return ret;
}

int dcam_k_cfg_pdaf(struct isp_io_param *param, enum dcam_id idx,
					struct sprd_pdaf_control *pdaf_ctrl)
{
	int ret = 0;

	if (!param || !param->property_param) {
		pr_err("fail to get param: %p or property param.\n", param);
		return -EPERM;
	}

	switch (param->property) {
	case DCAM_PRO_PDAF_BLOCK:
		ret = dcam_k_pdaf_block(param, idx, pdaf_ctrl);
		break;
	case DCAM_PRO_PDAF_BYPASS:
	case DCAM_PRO_PDAF_SET_EXTRACTOR_BYPASS:
		ret = dcam_k_pdaf_bypass(param, idx, pdaf_ctrl);
		break;
	case DCAM_PRO_PDAF_ADDR:
		ret = dcam_k_pdaf_addr(param, idx, pdaf_ctrl);
		break;
	case DCAM_PRO_PDAF_SET_MODE:
		ret = dcam_k_pdaf_set_mode(param, idx);
		break;
	case DCAM_PRO_PDAF_SET_SKIP_NUM:
		ret = dcam_k_pdaf_set_skip_num(param, idx);
		break;
	case DCAM_PRO_PDAF_SET_ROI:
		ret = dcam_k_pdaf_set_roi(param, idx);
		break;
	case DCAM_PRO_PDAF_SET_PPI_INFO:
		ret = dcam_k_pdaf_set_ppi_info(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}
