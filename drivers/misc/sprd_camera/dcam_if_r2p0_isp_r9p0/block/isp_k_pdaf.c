/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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
#include <video/sprd_mm.h>

#include "isp_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "PDAF: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define PDAF_PATTERN_PIXEL_NUM    4
#define PDAF_PATTERN_NUM                32

static void write_pd_table(struct pdaf_ppi_info *pdaf_info, enum dcam_id idx)
{
	int i = 0;
	int col = 0, row = 0, is_right = 0;
	int col2 = 0, row2 = 0, is_right2 = 0;
	int block_size = 16;
	uint32_t pdafinfo[PDAF_PATTERN_NUM] = {0};

	switch (pdaf_info->block_size.height) {
	case 0:
		block_size = 8;
		break;
	case 1:
		block_size = 16;
		break;
	case 2:
		block_size = 32;
		break;
	case 3:
		block_size = 64;
		break;
	default:
		pr_err("fail to check pd_block_num\n");
		break;
	}

	for (i = 0; i < PDAF_PATTERN_NUM * 2; i += 2) {
		col = pdaf_info->pattern_pixel_col[i];
		row = pdaf_info->pattern_pixel_row[i];
		is_right = pdaf_info->pattern_pixel_is_right[i] & 0x01;
		col2 = pdaf_info->pattern_pixel_col[i+1];
		row2 = pdaf_info->pattern_pixel_row[i+1];
		is_right2 = pdaf_info->pattern_pixel_is_right[i+1] & 0x01;

		pdafinfo[i/2] =
				(((is_right2 << 12 | row2 << 6 | col2) << 16)
				|(is_right << 12 | row << 6 | col));
	}

	for (i = 0; i < PDAF_PATTERN_NUM; i++)
		DCAM_REG_WR(idx, ISP_PPI_PATTERN01 + (i * 4), pdafinfo[i]);
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

static int isp_k_pdaf_type1_block(struct isp_io_param *param, enum dcam_id idx)
{
	int ret = 0;
	struct dev_dcam_vc2_control vch2_info;

	memset(&vch2_info, 0x00, sizeof(vch2_info));
	ret = copy_from_user((void *)&vch2_info,
		param->property_param, sizeof(vch2_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	sprd_dcam_drv_glb_reg_owr(idx, DCAM_VCH2_CONTROL, BIT_0, 1);

	DCAM_REG_WR(idx, DCAM_VCH2_CONTROL,
		(vch2_info.vch2_vc & 0x3) << 16
		|(vch2_info.vch2_data_type & 0x3f) << 8
		|(vch2_info.vch2_mode & 0x3) << 4);

	return ret;
}

static int isp_k_pdaf_type2_block(struct isp_io_param *param, enum dcam_id idx)
{
	int ret = 0;
	struct dev_dcam_vc2_control vch2_info;

	memset(&vch2_info, 0x00, sizeof(vch2_info));
	ret = copy_from_user((void *)&vch2_info,
		param->property_param, sizeof(vch2_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	sprd_dcam_drv_glb_reg_owr(idx, DCAM_VCH2_CONTROL, BIT_0, 1);

	DCAM_REG_WR(idx, DCAM_VCH2_CONTROL,
		(vch2_info.vch2_vc & 0x3) << 16
		|(vch2_info.vch2_data_type & 0x3f) << 8
		|(vch2_info.vch2_mode & 0x3) << 4);

	return ret;
}

static int isp_k_pdaf_block(struct isp_io_param *param, enum dcam_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_dev_pdaf_info pdaf_info;
	struct pdaf_ppi_info ppi_info;

	memset(&pdaf_info, 0x00, sizeof(pdaf_info));
	memset(&ppi_info, 0x00, sizeof(ppi_info));
	ret = copy_from_user((void *)&pdaf_info,
		param->property_param, sizeof(pdaf_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}
	val = !pdaf_info.bypass;

	DCAM_REG_MWR(idx, DCAM_PPE_FRM_CTRL0, BIT_0, val);
	DCAM_REG_MWR(idx, DCAM_VCH2_CONTROL, BIT_0, val);

	val = ((pdaf_info.block_size.height & 0x3) << 6) |
		((pdaf_info.block_size.width & 0x3) << 4);
	DCAM_REG_MWR(idx, ISP_PPI_PARAM, 0xF << 4, val);

	val = ((pdaf_info.win.y & 0x1FFF) << 16) |
		(pdaf_info.win.x & 0x1FFF);
	DCAM_REG_WR(idx, ISP_PPI_AF_WIN_START, val);

	val = (((pdaf_info.win.h + pdaf_info.win.y - 1) & 0x1FFF) << 16) |
		((pdaf_info.win.w + pdaf_info.win.x - 1) & 0x1FFF);
	DCAM_REG_WR(idx, ISP_PPI_AF_WIN_END, val);

	ret = pd_info_to_ppi_info(&pdaf_info, &ppi_info);
	if (!ret)
		write_pd_table(&ppi_info, idx);

	DCAM_REG_MWR(idx, DCAM_PPE_FRM_CTRL0,
			0xF << 4, pdaf_info.skip_num << 4);
	DCAM_REG_MWR(idx, DCAM_PPE_FRM_CTRL0, BIT_2, pdaf_info.mode << 2);

	if (pdaf_info.mode)
		DCAM_REG_MWR(idx, DCAM_PPE_FRM_CTRL0, BIT_3, (0x1 << 3));
	else
		DCAM_REG_MWR(idx, DCAM_PPE_FRM_CTRL1, BIT_0, 0x1);

	return ret;
}

static int isp_k_pdaf_bypass(struct isp_io_param *param, enum dcam_id idx)
{
	int ret = 0;
	unsigned int bypass = 0;

	ret = copy_from_user((void *)&bypass,
		param->property_param, sizeof(unsigned int));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	if (bypass)
		return ret;
	sprd_dcam_drv_glb_reg_owr(idx, DCAM_PPE_FRM_CTRL0, BIT_0, 1);
	sprd_dcam_drv_glb_reg_owr(idx, DCAM_VCH2_CONTROL, BIT_0, 1);

	DCAM_REG_MWR(idx, DCAM_PDAF_CONTROL, 0x3, 0x3);
	DCAM_REG_MWR(idx, DCAM_VCH2_CONTROL, 0x3 << 4, 0 << 4);

	return ret;
}

static int isp_k_pdaf_set_ppi_info(struct isp_io_param *param, enum dcam_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct pdaf_ppi_info pdaf_info;

	memset(&pdaf_info, 0x00, sizeof(pdaf_info));
	ret = copy_from_user((void *)&pdaf_info,
		param->property_param, sizeof(pdaf_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}
	val = ((pdaf_info.block_size.height & 0x3) << 6)
		| ((pdaf_info.block_size.width & 0x3) << 4);
	DCAM_REG_MWR(idx, ISP_PPI_PARAM, 0xF << 4, val);
	write_pd_table(&pdaf_info, idx);

	return ret;
}

static int isp_k_pdaf_set_roi(struct isp_io_param *param, enum dcam_id idx)
{

	int ret = 0;
	unsigned int val = 0;
	struct pdaf_roi_info roi_info;

	memset(&roi_info, 0x00, sizeof(roi_info));
	ret = copy_from_user((void *)&roi_info,
		param->property_param, sizeof(roi_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}
	val = ((roi_info.win.y & 0x1FFF) << 16) |
		(roi_info.win.x & 0x1FFF);
	DCAM_REG_WR(idx, ISP_PPI_AF_WIN_START, val);

	val = (((roi_info.win.h + roi_info.win.y - 1) & 0x1FFF) << 16) |
		((roi_info.win.w + roi_info.win.x - 1) & 0x1FFF);
	DCAM_REG_WR(idx, ISP_PPI_AF_WIN_END, val);

	sprd_dcam_drv_glb_reg_owr(idx, DCAM_PPE_FRM_CTRL0, BIT_0, 1);
	sprd_dcam_drv_glb_reg_owr(idx, DCAM_VCH2_CONTROL, BIT_0, 1);

	DCAM_REG_MWR(idx, DCAM_PDAF_CONTROL, 0x3, 0x3);
	DCAM_REG_MWR(idx, DCAM_VCH2_CONTROL, 0x3 << 4, 0 << 4);
	/* need modify when code bringup */
	sprd_dcam_drv_force_copy(idx, PDAF_COPY);
	sprd_dcam_drv_force_copy(idx, VCH2_COPY);

	return ret;
}

static int isp_k_pdaf_set_skip_num(struct isp_io_param *param, enum dcam_id idx)
{
	int ret = 0;
	unsigned int skip_num = 0;

	ret = copy_from_user((void *)&skip_num,
			param->property_param, sizeof(skip_num));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	DCAM_REG_MWR(idx, DCAM_PPE_FRM_CTRL0, 0xF << 4, skip_num << 4);

	return ret;
}

static int isp_k_pdaf_set_mode(struct isp_io_param *param, enum dcam_id idx)
{
	int ret = 0;
	uint32_t mode = 0;

	ret = copy_from_user((void *)&mode,
			param->property_param, sizeof(mode));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	DCAM_REG_MWR(idx, DCAM_PPE_FRM_CTRL0, BIT_2, mode << 2);
	if (mode)
		DCAM_REG_MWR(idx, DCAM_PPE_FRM_CTRL0, BIT_3, (0x1 << 3));
	else
		DCAM_REG_MWR(idx, DCAM_PPE_FRM_CTRL1, BIT_0, 0x1);

	return ret;
}

int isp_k_cfg_pdaf(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t com_idx)
{
	int ret = 0;
	enum isp_id idx;
	enum dcam_id id = ISP_ID_0;

	if (!param) {
		pr_err("fail to get param\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param\n");
		return -1;
	}

	idx = ISP_GET_ISP_ID(com_idx);
	id = (enum dcam_id)idx;
	switch (param->property) {
	case ISP_PRO_PDAF_BLOCK:
		ret = isp_k_pdaf_block(param, id);
		break;
	case ISP_PRO_PDAF_BYPASS:
		ret = isp_k_pdaf_bypass(param, id);
		break;
	case ISP_PRO_PDAF_SET_MODE:
		ret = isp_k_pdaf_set_mode(param, id);
		break;
	case ISP_PRO_PDAF_SET_SKIP_NUM:
		ret = isp_k_pdaf_set_skip_num(param, id);
		break;
	case ISP_PRO_PDAF_SET_ROI:
		ret = isp_k_pdaf_set_roi(param, id);
		break;
	case ISP_PRO_PDAF_SET_PPI_INFO:
		ret = isp_k_pdaf_set_ppi_info(param, id);
		break;
	case ISP_PRO_PDAF_TYPE1_BLOCK:
		ret = isp_k_pdaf_type1_block(param, id);
		break;
	case ISP_PRO_PDAF_TYPE2_BLOCK:
		ret = isp_k_pdaf_type2_block(param, id);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
