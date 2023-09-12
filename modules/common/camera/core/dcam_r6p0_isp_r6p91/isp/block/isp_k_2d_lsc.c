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
#include <asm/cacheflush.h>
#include <linux/delay.h>
#include "isp_block.h"


#define ISP_LSC_TIME_OUT_MAX        5
#define ISP_LSC_BUF0                0
#define ISP_LSC_BUF1                1
#define  ISP_INT_EVT_LSC_LOAD (1<<7)

static int32_t isp_k_2d_lsc_param_load(struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;
	uint32_t time_out_cnt = 0;
	uint32_t reg_value = 0;
	reg_value = ISP_REG_RD(ISP_LENS_PARAM);
	isp_k_param->lsc_load_buf_id = !((reg_value >> 1) & 1);
	ISP_REG_MWR(ISP_LENS_LOAD_BUF, BIT_0, isp_k_param->lsc_load_buf_id);
	ISP_REG_WR(ISP_LENS_PARAM_ADDR, isp_k_param->lsc_buf_phys_addr);
	ISP_REG_OWR(ISP_LENS_LOAD_EB, BIT_0);

	isp_k_param->lsc_update_buf_id = isp_k_param->lsc_load_buf_id;

	reg_value = ISP_REG_RD(ISP_INT_RAW0);

	while ((0x00 == (reg_value & ISP_INT_EVT_LSC_LOAD))
		&& (time_out_cnt < ISP_LSC_TIME_OUT_MAX)) {
		udelay(1000);
		reg_value = ISP_REG_RD(ISP_INT_RAW0);
		time_out_cnt++;
	}
	if (time_out_cnt >= ISP_LSC_TIME_OUT_MAX) {
		ret = -1;
		pr_err("isp_k_2d_lsc_param_load: lsc load time out error.\n");
	}
	ISP_REG_OWR(ISP_INT_CLR0, ISP_INT_EVT_LSC_LOAD);

	ISP_REG_MWR(ISP_LENS_PARAM, BIT_1,
		isp_k_param->lsc_load_buf_id << 1);

	return ret;
}

static int32_t isp_k_2d_lsc_block(struct isp_io_param *param,
		struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t i = 0;
	struct isp_dev_2d_lsc_info lens_info;

	if (param == NULL || isp_k_param == NULL) {
            pr_err("isp_k_2d_lsc_block: param or isp_k_param null\n");
            return ret;
	}
	memset(&lens_info, 0x00, sizeof(lens_info));
	if (atomic_read(&isp_k_param->lsc_updated) == 1) {
		pr_debug("already updated for current frame\n");
		return 0;
	}

	ret = copy_from_user((void *)&lens_info,
			param->property_param, sizeof(lens_info));
	if (ret != 0) {
		pr_err("isp_k_2d_lsc_block: copy error, ret=0x%x\n",
			(uint32_t)ret);
		return -1;
	}
	ISP_REG_MWR(ISP_LENS_PARAM, BIT_0, lens_info.bypass);
	if (lens_info.bypass)
		return 0;

	val = ((lens_info.offset_y & 0xFFFF) << 16) |
		(lens_info.offset_x & 0xFFFF);
	ISP_REG_WR(ISP_LENS_SLICE_POS, val);

	val = lens_info.grid_pitch & 0x1FF;
	ISP_REG_MWR(ISP_LENS_GRID_PITCH, 0x1FF, val);

	val = (lens_info.grid_width & 0xFF) << 16;
	ISP_REG_MWR(ISP_LENS_GRID_PITCH, 0xFF0000, val);

	val = ((lens_info.grid_num_t & 0xFFFF) << 16) |
		((lens_info.grid_y_num & 0xFF) << 8) |
		(lens_info.grid_x_num & 0xFF);
	ISP_REG_WR(ISP_LENS_GRID_SIZE, val);

	ISP_REG_MWR(ISP_LENS_MISC, (BIT_1 | BIT_0), (lens_info.endian & 0x03));

	val = ((lens_info.slice_size.height & 0xFFFF) << 16) |
		(lens_info.slice_size.width & 0xFFFF);
	ISP_REG_WR(ISP_LENS_SLICE_SIZE, val);

	val = ((lens_info.relative_y & 0xFF) << 8) |
		(lens_info.relative_x & 0xFF);
	ISP_REG_WR(ISP_LENS_INIT_COOR, val);

	if (isp_k_param->lsc_buf_phys_addr == 0) {
		pr_err("the lsc buf has been unmap!\n");
		return -1;
	}
	ret = isp_k_2d_lsc_param_load(isp_k_param);
	if (ret != 0) {
		pr_err("isp_k_2d_lsc_block: lsc load para error, ret=0x%x\n",
			(uint32_t)ret);
		ISP_REG_MWR(ISP_LENS_PARAM, BIT_0, ISP_BYPASS_EB);
		return -1;
	}

	for (i = 0; i < 5; i++) {
		val = ((lens_info.q_value[0][i] & 0x3FFF) << 16) |
			(lens_info.q_value[1][i] & 0x3FFF);
		ISP_REG_WR(ISP_LENS_Q0_VALUE + i * 4, val);
	}

	atomic_set(&isp_k_param->lsc_updated, 1);

	return ret;

}

static int32_t isp_k_2d_lsc_param_update(struct isp_io_param *param,
		struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;
	return ret;
}

static int32_t isp_k_2d_lsc_pos(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_offset offset = {0, 0};

	ret = copy_from_user((void *)&offset, param->property_param,
		sizeof(offset));
	if (ret != 0) {
		pr_err("isp_k_lens_pos: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((offset.y & 0xFFFF) << 16) | (offset.x & 0xFFFF);

	ISP_REG_WR(ISP_LENS_SLICE_POS, val);

	return ret;
}

static int32_t isp_k_2d_lsc_grid_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t grid_total = 0;
	struct isp_img_size size = {0, 0};

	ret = copy_from_user((void *)&size, param->property_param,
		sizeof(size));
	if (ret != 0) {
		pr_err("isp_k_2d_lsc_grid_size: copy error, ret=0x%x\n",
			(uint32_t)ret);
		return -1;
	}

	grid_total = (size.height + 2) * (size.width + 2);
	val = ((grid_total & 0xFFFF) << 16) | ((size.height & 0xFF) << 8) |
		(size.width & 0xFF);
	ISP_REG_WR(ISP_LENS_GRID_SIZE, val);

	return ret;
}

static int32_t isp_k_2d_lsc_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size size = {0, 0};

	ret = copy_from_user((void *)&size, param->property_param,
		sizeof(size));
	if (ret != 0) {
		pr_err("isp_k_2d_lsc_slice_size: copy error, ret=0x%x\n",
			(uint32_t)ret);
		return -1;
	}

	val = ((size.height & 0xFFFF) << 16) | (size.width & 0xFFFF);

	ISP_REG_WR(ISP_LENS_SLICE_SIZE, val);

	return ret;
}

static int32_t isp_k_2d_lsc_transaddr(struct isp_io_param *param,
			struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;
	struct isp_lsc_addr lsc_addr;
	struct isp_statis_buf lsc_remap_buf;

	memset(&lsc_addr, 0x00, sizeof(struct isp_lsc_addr));
	memset(&lsc_remap_buf, 0x00, sizeof(struct isp_statis_buf));
	ret = copy_from_user((void *)&lsc_addr, param->property_param,
			sizeof(struct isp_lsc_addr));

	lsc_remap_buf.pfinfo.dev = &isp_k_param->isp_pdev->dev;
	lsc_remap_buf.pfinfo.mfd[0] = lsc_addr.fd;

		/*mapping iommu buffer*/
	ret = pfiommu_get_sg_table(&lsc_remap_buf.pfinfo);
	if (ret) {
		pr_err("map iommu lsc buffer failed.\n");
		return -EFAULT;
	}

	ret = pfiommu_get_addr(&lsc_remap_buf.pfinfo);
	if (ret) {
		pr_err("map iommu lsc get addr failed.\n");
		return -EFAULT;
	}

	isp_k_param->lsc_buf_phys_addr =
		lsc_remap_buf.pfinfo.iova[0];
	isp_k_param->lsc_pfinfo.iova[0] =
		lsc_remap_buf.pfinfo.iova[0];
	isp_k_param->lsc_pfinfo.size[0] = lsc_remap_buf.pfinfo.size[0];
	isp_k_param->lsc_pfinfo.dev = &isp_k_param->isp_pdev->dev;

	pr_debug("lsc phys addr 0x%x,\n", isp_k_param->lsc_buf_phys_addr);

	return 0;
}

int32_t isp_k_cfg_2d_lsc(struct isp_io_param *param,
			struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;

	if (!param) {
		pr_err("isp_k_cfg_2d_lsc: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_err("isp_k_cfg_2d_lsc: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_2D_LSC_BLOCK:
		ret = isp_k_2d_lsc_block(param, isp_k_param);
		break;
	case ISP_PRO_2D_LSC_PARAM_UPDATE:
		ret = isp_k_2d_lsc_param_update(param, isp_k_param);
		break;
	case ISP_PRO_2D_LSC_POS:
		ret = isp_k_2d_lsc_pos(param);
		break;
	case ISP_PRO_2D_LSC_GRID_SIZE:
		ret = isp_k_2d_lsc_grid_size(param);
		break;
	case ISP_PRO_2D_LSC_SLICE_SIZE:
		ret = isp_k_2d_lsc_slice_size(param);
		break;
	case ISP_PRO_2D_LSC_TRANSADDR:
		ret = isp_k_2d_lsc_transaddr(param, isp_k_param);
		break;
	default:
		pr_info("isp_k_cfg_2d_lsc: fail cmd id:%d, not supported.\n",
			param->property);
		break;
	}

	return ret;
}
