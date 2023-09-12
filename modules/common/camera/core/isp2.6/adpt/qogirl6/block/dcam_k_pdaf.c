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
#include <sprd_mm.h>
#include "isp_hw.h"
#include "dcam_core.h"
#include "dcam_reg.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "PDAF: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__


static void write_pd_table(struct pdaf_ppi_info *pdaf_info, enum dcam_id idx)
{
	int i = 0;
	int col = 0, row = 0, is_right = 0;
	int pd_num = 0;
	int reg_pos = 0;
	uint32_t pdafinfo[32] = {0};

	pd_num = pdaf_info->pd_pos_size * 2;
	for (i = 0; i < pd_num; i++) {
		col = pdaf_info->pattern_pixel_col[i];
		row = pdaf_info->pattern_pixel_row[i];
		is_right = pdaf_info->pattern_pixel_is_right[i] & 0x01;
		pr_debug("col %d, row %d, right %d\n", col, row, is_right);
		if (i%2 == 0) {
			pdafinfo[reg_pos] = col | (row << 6) | (is_right << 12);
		} else if (i % 2 == 1) {
			pdafinfo[reg_pos] |= (col << 16) | (row << 22) | (is_right << 28);
			reg_pos++;
		}
	}

	DCAM_REG_MWR(idx, ISP_PPI_PARAM, 0x007f0000, pd_num << 16);

	for (i = 0; i < pd_num/2; i++)
		DCAM_REG_MWR(idx, ISP_PPI_PATTERN01 + (i * 4), 0x1fff1fff, pdafinfo[i]);
}

static int isp_k_pdaf_type1_block(
	struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;
	uint32_t idx = p->idx;
	struct dev_dcam_vc2_control *vch2_info = &p->pdaf.vch2_info;

	ret = copy_from_user((void *)vch2_info,
			param->property_param,
			sizeof(struct dev_dcam_vc2_control));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}
	p->pdaf.pdaf_type = DCAM_PDAF_TYPE1;
	if (idx == DCAM_HW_CONTEXT_MAX)
		return 0;

	DCAM_REG_WR(idx, DCAM_PDAF_CONTROL,
		(vch2_info->vch2_vc & 0x03) << 16
		|(vch2_info->vch2_data_type & 0x3f) << 8
		|(vch2_info->vch2_mode & 0x03));

	return ret;
}

static int isp_k_pdaf_type2_block(
	struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;
	uint32_t idx = p->idx;
	struct dev_dcam_vc2_control *vch2_info = &p->pdaf.vch2_info;

	ret = copy_from_user((void *)vch2_info,
			param->property_param,
			sizeof(struct dev_dcam_vc2_control));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}
	p->pdaf.pdaf_type = DCAM_PDAF_TYPE2;
	if (idx == DCAM_HW_CONTEXT_MAX)
		return 0;

	DCAM_REG_WR(idx, DCAM_PDAF_CONTROL,
		(vch2_info->vch2_vc & 0x03) << 16
		|(vch2_info->vch2_data_type & 0x3f) << 8
		|(vch2_info->vch2_mode & 0x03));

	return ret;
}

static int isp_k_pdaf_type3_block(
	struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;
	enum dcam_id idx;
	struct dev_dcam_vc2_control *vch2_info = &p->pdaf.vch2_info;

	ret = copy_from_user((void *)vch2_info,
			param->property_param,
			sizeof(struct dev_dcam_vc2_control));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	idx = p->idx;
	p->pdaf.pdaf_type = DCAM_PDAF_TYPE3;
	if (idx == DCAM_HW_CONTEXT_MAX)
		return 0;

	pr_debug("pdaf_info.vch2_mode = %d\n", vch2_info->vch2_mode);
	DCAM_REG_WR(idx, DCAM_PDAF_CONTROL,
		(vch2_info->vch2_vc & 0x03) << 16
		|(vch2_info->vch2_data_type & 0x3f) << 8
		|(vch2_info->vch2_mode & 0x03));

	return ret;
}

static int isp_k_dual_pdaf_block(
	struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;
	uint32_t idx = p->idx;
	struct dev_dcam_vc2_control *vch2_info = &p->pdaf.vch2_info;

	ret = copy_from_user((void *)vch2_info,
			param->property_param,
			sizeof(struct dev_dcam_vc2_control));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}
	p->pdaf.pdaf_type = DCAM_PDAF_DUAL;
	if (idx == DCAM_HW_CONTEXT_MAX)
		return 0;

	DCAM_REG_WR(idx, DCAM_PDAF_CONTROL,
		(vch2_info->vch2_vc & 0x03) << 16
		|(vch2_info->vch2_data_type & 0x3f) << 8
		|(vch2_info->vch2_mode & 0x03));

	return ret;
}

static int isp_k_pdaf_type3_set_info(
	struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;
	uint32_t val = 0;
	uint32_t idx = p->idx;
	struct isp_dev_pdaf_info *pdaf_info = &p->pdaf.pdaf_info;

	ret = copy_from_user((void *)pdaf_info,
			param->property_param,
			sizeof(struct isp_dev_pdaf_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	if (idx == DCAM_HW_CONTEXT_MAX)
		return 0;

	/* phase map corr en */
	val = pdaf_info->phase_map_corr_en;
	DCAM_REG_MWR(idx, ISP_PPI_PARAM, BIT_3, val << 3);

	/* ppi grid mode */
	val = pdaf_info->grid_mode;
	DCAM_REG_MWR(idx, ISP_PPI_PARAM, BIT_8, val << 8);

	return ret;
}

static int isp_k_pdaf_bypass(struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass,
			param->property_param, sizeof(uint32_t));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	bypass = !!bypass;
	p->pdaf.bypass = bypass;
	pr_info("pdaf bypass %d\n", bypass);

	return ret;
}

static int isp_k_pdaf_type3_set_ppi_info(
	struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;
	uint32_t val = 0;
	uint32_t idx = p->idx;
	struct pdaf_ppi_info *ppi_info = &p->pdaf.ppi_info;

	ret = copy_from_user((void *)ppi_info,
			param->property_param,
			sizeof(struct pdaf_ppi_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	pr_debug("idx %d, block area: (%d, %d) (%d, %d), block.w/h: %d, %d, ppi bypass %d\n", idx,
		ppi_info->block.start_x,
		ppi_info->block.start_y,
		ppi_info->block.end_x,
		ppi_info->block.end_y,
		ppi_info->block_size.width,
		ppi_info->block_size.height,
		ppi_info->bypass);

	if (idx == DCAM_HW_CONTEXT_MAX)
		return 0;

	DCAM_REG_MWR(p->idx, ISP_PPI_PARAM, BIT_0, ppi_info->bypass);
	if (ppi_info->bypass)
		return 0;

	write_pd_table(ppi_info, idx);

	/* ppi block col&row start,end */
	val = ppi_info->block.start_x
		| ppi_info->block.end_x << 16;
	DCAM_REG_MWR(idx, ISP_PPI_BLOCK_COL, 0xffffffff, val);
	val = 0;
	val = ppi_info->block.start_y
		| ppi_info->block.end_y << 16;
	DCAM_REG_MWR(idx, ISP_PPI_BLOCK_ROW, 0xffffffff, val);

	/* ppi block w*h */
	val = 0;
	val = (ppi_info->block_size.width<<4)
		| (ppi_info->block_size.height << 6);
	DCAM_REG_MWR(idx, ISP_PPI_PARAM, 0x000000f0, val);

	return ret;
}

static int isp_k_pdaf_type3_set_roi(
	struct isp_io_param *param, struct dcam_dev_param *p)
{

	int ret = 0;
	uint32_t val = 0;
	uint32_t idx = p->idx;
	struct pdaf_roi_info *roi_info = &p->pdaf.roi_info;

	pr_debug("E\n");
	ret = copy_from_user((void *)roi_info,
			param->property_param,
			sizeof(struct pdaf_roi_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	pr_debug("win start:(%d, %d) end:(%d, %d)\n", roi_info->win.start_x,
		roi_info->win.start_y, roi_info->win.end_x, roi_info->win.end_y);

	if (idx == DCAM_HW_CONTEXT_MAX)
		return 0;

	val = ((roi_info->win.start_y & 0xFFFF) << 16) |
		(roi_info->win.start_x & 0xFFFF);
	DCAM_REG_WR(idx, ISP_PPI_AF_WIN_START, val);

	val = ((roi_info->win.end_y & 0xFFFF) << 16) |
		(roi_info->win.end_x & 0xFFFF);
	DCAM_REG_WR(idx, ISP_PPI_AF_WIN_END, val);

	return ret;
}

static int isp_k_pdaf_type3_set_skip_num(
	struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;
	uint32_t idx = p->idx;
	uint32_t skip_num = 0;

	ret = copy_from_user((void *)&skip_num,
			param->property_param, sizeof(skip_num));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}
	p->pdaf.skip_num = skip_num;
	if (idx == DCAM_HW_CONTEXT_MAX)
		return 0;

	DCAM_REG_MWR(idx, DCAM_PPE_FRM_CTRL0, 0xF0,
		skip_num << 4);
	DCAM_REG_MWR(idx, DCAM_PPE_FRM_CTRL1, BIT_1, BIT_1);

	return ret;
}

static int isp_k_pdaf_type3_set_mode(
	struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;
	uint32_t idx = p->idx;
	uint32_t mode = 0;

	ret = copy_from_user((void *)&mode,
			param->property_param, sizeof(mode));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	/* single/multi mode */
	mode = !!mode;
	p->pdaf.mode = mode;

	if (idx == DCAM_HW_CONTEXT_MAX)
		return 0;

	DCAM_REG_MWR(idx, DCAM_PPE_FRM_CTRL0, BIT_2, mode << 2);
	DCAM_REG_MWR(idx, DCAM_PPE_FRM_CTRL0, BIT_3, mode << 3);

	return ret;
}

int dcam_k_pdaf(struct dcam_dev_param *p)
{
  	int i = 0;
	uint32_t val = 0, offset = 0;
	uint32_t idx = p->idx;
	uint32_t bypass = p->pdaf.bypass;
	uint32_t mode = p->pdaf.mode;
	uint32_t skip_num = p->pdaf.skip_num;
	struct dev_dcam_vc2_control *vch2_info = &p->pdaf.vch2_info;
	struct pdaf_ppi_info *ppi_info = &p->pdaf.ppi_info;
	struct pdaf_roi_info *roi_info = &p->pdaf.roi_info;
	struct dcam_ppe_ppc_info *ppe_ppc_info = &(p->pdaf.ppe_ppc_info);

	DCAM_REG_MWR(p->idx, ISP_PPI_PARAM, BIT_0, bypass);
	if (bypass) {
		pr_debug("dcam%d pdaf bypass\n", idx);
		return 0;
	}
	pr_info("dcam%d reconfigure pdaf param, type %d\n", idx, p->pdaf.pdaf_type);

	/* mode */
	DCAM_REG_MWR(idx, DCAM_PPE_FRM_CTRL0, BIT_2, mode << 2);
	DCAM_REG_MWR(idx, DCAM_PPE_FRM_CTRL0, BIT_3, mode << 3);

	/* skip num */
	DCAM_REG_MWR(idx, DCAM_PPE_FRM_CTRL0, 0xF0,
		skip_num << 4);
	DCAM_REG_MWR(idx, DCAM_PPE_FRM_CTRL1, BIT_1, BIT_1);

	/* phase map corr en */
	val = p->pdaf.pdaf_info.phase_map_corr_en;
	DCAM_REG_MWR(idx, ISP_PPI_PARAM, BIT_3, val << 3);

	/* ppi grid mode */
	val = p->pdaf.pdaf_info.grid_mode;
	DCAM_REG_MWR(idx, ISP_PPI_PARAM, BIT_8, val << 8);

	/* pdaf type */
	DCAM_REG_WR(idx, DCAM_PDAF_CONTROL,
		(vch2_info->vch2_vc & 0x03) << 16
		|(vch2_info->vch2_data_type & 0x3f) << 8
		|(vch2_info->vch2_mode & 0x03));

	/* pdaf roi */
	val = ((roi_info->win.start_y & 0xFFFF) << 16) |
		(roi_info->win.start_x & 0xFFFF);
	DCAM_REG_WR(idx, ISP_PPI_AF_WIN_START, val);

	val = ((roi_info->win.end_y & 0xFFFF) << 16) |
		(roi_info->win.end_x & 0xFFFF);
	DCAM_REG_WR(idx, ISP_PPI_AF_WIN_END, val);

	/* pdaf ppi */
	write_pd_table(ppi_info, idx);

	pr_debug("idx %d, block area: (%d, %d) (%d, %d), block.w/h: %d, %d, ppi bypass %d\n", idx,
		ppi_info->block.start_x,
		ppi_info->block.start_y,
		ppi_info->block.end_x,
		ppi_info->block.end_y,
		ppi_info->block_size.width,
		ppi_info->block_size.height,
		ppi_info->bypass);

	/* ppi block col&row start,end */
	val = ppi_info->block.start_x
		| ppi_info->block.end_x << 16;
	DCAM_REG_MWR(idx, ISP_PPI_BLOCK_COL, 0xffffffff, val);
	val = 0;
	val = ppi_info->block.start_y
		| ppi_info->block.end_y << 16;
	DCAM_REG_MWR(idx, ISP_PPI_BLOCK_ROW, 0xffffffff, val);

	val = ppi_info->block.start_y
		| ppi_info->block.end_y << 16;
	DCAM_REG_WR(idx, ISP_BPC_PPI_RANG, val);

	val = ppi_info->block.start_x
		| ppi_info->block.end_x << 16;
	DCAM_REG_WR(idx, ISP_BPC_PPI_RANG1, val);

	/* ppi block w*h */
	val = 0;
	val = (ppi_info->block_size.width<<4)
		| (ppi_info->block_size.height << 6);
	DCAM_REG_MWR(idx, ISP_PPI_PARAM, 0x000000f0, val);

	/* ppe ppc  block w*h */
	val = ((ppe_ppc_info->ppi_phase_map_corr_en & 1) << 3) | (ppe_ppc_info->ppi_bypass & 1);
	DCAM_REG_MWR(idx, ISP_PPI_PARAM, BIT_3 | BIT_0, val);

	if (!ppe_ppc_info->ppi_phase_map_corr_en)
		return 0;

	offset = PDAF_CORR_TABLE_START;
	for (i = 0; i < PDAF_PPI_GAIN_MAP_LEN; i++) {
		val = (ppe_ppc_info->ppc_l_gain_map[i] & 0x3FFF);
		val <<= 16;
		val |= (ppe_ppc_info->ppc_r_gain_map[i] & 0x3FFF);
		DCAM_REG_WR(idx, offset, val);
		offset += 4;
	}
	return 0;
}

int dcam_k_ppe_ppc_param(struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0, i = 0;
	uint32_t offset = 0, val = 0;
	uint32_t idx = p->idx;
	struct dcam_ppe_ppc_info *ppe_ppc_info = &(p->pdaf.ppe_ppc_info);

	if (p->offline == 0) {
		ret = copy_from_user((void *)ppe_ppc_info,
				param->property_param,
				sizeof(struct dcam_ppe_ppc_info));
		if (ret != 0) {
			pr_err("fail to copy from user, ret = %d\n", ret);
			return -1;
		}
	} else {
		mutex_lock(&p->param_lock);
		ret = copy_from_user((void *)ppe_ppc_info,
				param->property_param,
				sizeof(struct dcam_ppe_ppc_info));
		mutex_unlock(&p->param_lock);
		if (ret)
			pr_err("fail to copy from user, ret = %d\n", ret);
		return ret;
	}
	if (p->idx == DCAM_HW_CONTEXT_MAX)
		return 0;

	val = ((ppe_ppc_info->ppi_phase_map_corr_en & 1) << 3) | (ppe_ppc_info->ppi_bypass & 1);
	DCAM_REG_MWR(idx, ISP_PPI_PARAM, BIT_3 | BIT_0, val);

	if (!ppe_ppc_info->ppi_phase_map_corr_en)
		return 0;

	offset = PDAF_CORR_TABLE_START;
	for (i = 0; i < PDAF_PPI_GAIN_MAP_LEN; i++) {
		val = (ppe_ppc_info->ppc_l_gain_map[i] & 0x3FFF);
		val <<= 16;
		val |= (ppe_ppc_info->ppc_r_gain_map[i] & 0x3FFF);
		DCAM_REG_WR(idx, offset, val);
		offset += 4;
	}

	return ret;
}

int dcam_k_cfg_pdaf(struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;

	if (!param || !p) {
		pr_err("fail to get param\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param\n");
		return -1;
	}

	switch (param->property) {
	case DCAM_PDAF_BYPASS:
		ret = isp_k_pdaf_bypass(param, p);
		break;
	case DCAM_PDAF_TYPE3_SET_INFO:
		ret = isp_k_pdaf_type3_set_info(param, p);
		break;
	case DCAM_PDAF_TYPE3_SET_MODE:
		ret = isp_k_pdaf_type3_set_mode(param, p);
		break;
	case DCAM_PDAF_TYPE3_SET_SKIP_NUM:
		ret = isp_k_pdaf_type3_set_skip_num(param, p);
		break;
	case DCAM_PDAF_TYPE3_SET_ROI:
		ret = isp_k_pdaf_type3_set_roi(param, p);
		break;
	case DCAM_PDAF_TYPE3_SET_PPI_INFO:
		ret = isp_k_pdaf_type3_set_ppi_info(param, p);
		break;
	case DCAM_DUAL_PDAF_BLOCK:
		ret = isp_k_dual_pdaf_block(param, p);
		break;
	case DCAM_PDAF_TYPE1_BLOCK:
		ret = isp_k_pdaf_type1_block(param, p);
		break;
	case DCAM_PDAF_TYPE2_BLOCK:
		ret = isp_k_pdaf_type2_block(param, p);
		break;
	case DCAM_PDAF_TYPE3_BLOCK:
		ret = isp_k_pdaf_type3_block(param, p);
		break;
	case DCAM_PDAF_PPE_PPC_PARAM:
		ret = dcam_k_ppe_ppc_param(param, p);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}
	pr_debug("idx %d, Sub %d, ret %d\n", p->idx, param->property, ret);

	return ret;
}
