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
#include <asm/cacheflush.h>
#include <linux/delay.h>

#include "sprd_mm.h"
#include "sprd_isp_hw.h"
#include "isp_drv.h"
#include "isp_buf.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_2D_LSC: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

#define ISP_LSC_TIME_OUT_US	500
#define ISP_LSC_BUF0		0
#define ISP_LSC_BUF1		1
#define ISP_BYPASS_EB		1
#define ISP_INT_EVT_LSC_LOAD    (1 << 7)

static int isp_k_2d_lsc_param_load(struct isp_k_block *isp_k_param,
				   enum isp_id idx)
{
	int ret = 0;
	unsigned int time_out_cnt = 0;
	unsigned int reg_value = 0;

	reg_value = ISP_PAGE_REG_RD(idx, ISP_LENS_PARAM);
	isp_k_param->lsc_load_buf_id = !((reg_value >> 1) & 1);

	ISP_REG_MWR(idx, ISP_LENS_LOAD_BUF, BIT_0,
		    isp_k_param->lsc_load_buf_id);
	ISP_REG_WR(idx, ISP_LENS_PARAM_ADDR,
		(uintptr_t)isp_k_param->lsc_buf_info.hw_addr);

	ISP_REG_OWR(idx, ISP_LENS_LOAD_EB, BIT_0);

	isp_k_param->lsc_update_buf_id = isp_k_param->lsc_load_buf_id;
	reg_value = ISP_PAGE_REG_RD(idx, ISP_INT_RAW0);

	while (((reg_value & ISP_INT_EVT_LSC_LOAD) == 0x00)
		&& (time_out_cnt < ISP_LSC_TIME_OUT_US)) {
		udelay(1);
		reg_value = ISP_PAGE_REG_RD(idx, ISP_INT_RAW0);
		time_out_cnt++;
	}
	if (time_out_cnt >= ISP_LSC_TIME_OUT_US) {
		ret = -EPERM;
		pr_err("fail to load lsc timeout\n");
	}
	ISP_REG_OWR(idx, ISP_INT_CLR0, ISP_INT_EVT_LSC_LOAD);

	ISP_REG_MWR(idx, ISP_LENS_PARAM, BIT_1,
			isp_k_param->lsc_update_buf_id << 1);
	return ret;
}

static int isp_k_2d_lsc_cfg_load_shading(uint16_t *lsc_buff,
					 enum isp_id idx,
					 struct isp_dev_2d_lsc_info *lens_info)
{
	int ret = 0;
	unsigned int i = 0;
	uint32_t *dst_addr = NULL;
	uint16_t lsc_h = 0;
	uint16_t lsc_l = 0;

	dst_addr = (uint32_t *)(ISP_BASE_ADDR(idx) + ISP_LEN_BUF0_CH0);

	if (lsc_buff == NULL)
		return -EPERM;

	for (i = 0; i < lens_info->buf_len / 2; i += 4) {
		lsc_l = lsc_buff[i + 2];
		lsc_h = lsc_buff[i + 3];
		*dst_addr++ = ((lsc_h & 0x3FFF) << 14) | (0x3FFF & lsc_l);

		lsc_l = lsc_buff[i];
		lsc_h = lsc_buff[i + 1];
		*dst_addr++ = ((lsc_h & 0x3FFF) << 14) | (0x3FFF & lsc_l);
	}

	return ret;
}

static int isp_k_2d_lsc_load_weight(struct isp_k_block *isp_k_param,
	enum isp_id idx,
	struct isp_dev_2d_lsc_info *lens_info)
{
	int ret = 0;
	void __user *data = NULL;
	unsigned long dst_addr = 0;
	unsigned short *w_buff = NULL;
	unsigned int weight_num;
	unsigned int i = 0;

	if (!isp_k_param->lsc_2d_weight_en) {
#ifdef CONFIG_64BIT
		data = (void __user *)(((unsigned long)
					lens_info->data_ptr[1] << 32) |
				       lens_info->data_ptr[0]);
#else
		data = (void __user *)((unsigned long)lens_info->data_ptr[0]);
#endif
		isp_k_param->lsc_2d_weight_en = 0;
		dst_addr = ISP_BASE_ADDR(idx) + ISP_LENS_WEIGHT_ADDR;

		if (lens_info->weight_num >=
			LENS_W_BUF_SIZE) {
			pr_err("fail to weight_num:0x%x bigger than buf:0x%x.\n",
					lens_info->weight_num,
					LENS_W_BUF_SIZE);
			return -EPERM;
		}

		if (!isp_k_param->isp_lens_w_addr) {
			pr_err("fail to get isp lens weight addr!\n");
			return -EFAULT;
		}
		w_buff = (unsigned short *)isp_k_param->isp_lens_w_addr;

		ret = copy_from_user((void *)w_buff,
			data,
			lens_info->weight_num);

		if (ret != 0) {
			pr_err("fail to copy wtable from user, ret=%d\n", ret);
			return -EFAULT;
		}

		weight_num = lens_info->grid_width / 2 + 1;

		for (i = 0; i < weight_num ; i++) {
			*((unsigned int *)dst_addr + i*2) =
				(((unsigned int)(*(w_buff+i*3))) & 0xFFFF) |
				((((unsigned int)(*(w_buff+i*3+1))) & 0xFFFF)
				<< 16);
			*((unsigned int *)dst_addr + i*2+1) =
				((unsigned int)(*(w_buff+i*3+2))) & 0xFFFF;
		}
	}

	return ret;
}

static int isp_k_2d_lsc_block(struct isp_io_param *param,
			      struct isp_k_block *isp_k_param,
			      enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	unsigned int i = 0;
	void __user *lsc_buf_ptr = NULL;
	struct isp_dev_2d_lsc_info lens_info;
	struct isp_pipe_dev *dev = NULL;
	enum isp_id iid = ISP_GET_IID(idx);

	if (param == NULL || isp_k_param == NULL) {
            pr_err("isp_k_2d_lsc_block: param or isp_k_param null\n");
            return ret;
	}
	memset(&lens_info, 0x00, sizeof(lens_info));

	ret = copy_from_user((void *)&lens_info, param->property_param,
						sizeof(lens_info));
	if (ret != 0) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return -EPERM;
	}

	dev = g_isp_dev_parray[iid];
	if (!dev) {
		pr_err("fail to get isp dev null.\n");
		return -EPERM;
	}

	ISP_REG_MWR(idx, ISP_LENS_PARAM, BIT_0, lens_info.bypass);
	if (unlikely(lens_info.bypass)) {
		pr_debug("lsc bypass.\n");
		dev->isp_k_param.lsc_bypass = 0xF;
		isp_dbg_s_ori_byp(SBLK_BYPASS, raw_2dlsc, idx);
		return 0;
	} else {
		dev->isp_k_param.lsc_bypass = 0;
	}

	val = (((lens_info.offset_y & 0xFFFF) << 16) |
			(lens_info.offset_x & 0xFFFF));
	ISP_REG_WR(idx, ISP_LENS_SLICE_POS, val);

	val = ((lens_info.grid_width & 0x1FF) << 16) |
		(lens_info.grid_pitch & 0x1FF);
	ISP_REG_WR(idx, ISP_LENS_GRID_PITCH, val);

	val = ((lens_info.grid_num_t & 0xFFFF) << 16) |
		((lens_info.grid_y_num & 0xFF) << 8) |
		(lens_info.grid_x_num & 0xFF);
	ISP_REG_WR(idx, ISP_LENS_GRID_SIZE, val);

	ISP_REG_MWR(idx, ISP_LENS_MISC, (BIT_1 | BIT_0),
				(lens_info.endian & 0x03));

	val = ((lens_info.slice_size.height & 0xFFFF) << 16) |
		(lens_info.slice_size.width & 0xFFFF);
	ISP_REG_WR(idx, ISP_LENS_SLICE_SIZE, val);

	val = ((lens_info.relative_y & 0x3FF) << 16) |
		(lens_info.relative_x & 0x3FF);
	ISP_REG_WR(idx, ISP_LENS_INIT_COOR, val);

	pr_debug("offset_y,x %d,%d, grid_w_p %d,%d, grid_num_x_y,%d,%d,%d, slice_h_w:%d,%d, r_y_x:%d,%d\n",
		lens_info.offset_y, lens_info.offset_x,
		lens_info.grid_width, lens_info.grid_pitch,
		lens_info.grid_num_t, lens_info.grid_x_num,
		lens_info.grid_y_num,
		lens_info.slice_size.height,
		lens_info.slice_size.width,
		lens_info.relative_y, lens_info.relative_x);
#ifdef CONFIG_64BIT
	lsc_buf_ptr =
		(void __user *)(((unsigned long)lens_info.buf_addr[1] << 32) |
			lens_info.buf_addr[0]);
#else
	lsc_buf_ptr = (void __user *)((unsigned long)lens_info.buf_addr[0]);
#endif

	if (!isp_k_param->lsc_buf_info.sw_addr ||
	    (lens_info.buf_len > ISP_LSC_BUF_SIZE)) {
		pr_err("fail to get valid buf_len or addr %p len:%d",
			isp_k_param->lsc_buf_info.sw_addr, lens_info.buf_len);
		ISP_REG_MWR(idx, ISP_LENS_PARAM, BIT_0, ISP_BYPASS_EB);
		return -EPERM;
	}

	if (ISP_GET_MID(idx) == ISP_AP_MODE) {
		ret = isp_k_2d_lsc_param_load(isp_k_param, idx);
	} else {
		ret = copy_from_user((void *)isp_k_param->lsc_buf_info.sw_addr,
				     lsc_buf_ptr,
				     lens_info.buf_len);
		if (ret != 0) {
			pr_err("fail to copy lens from user, ret=%d\n", ret);
			return -EFAULT;
		} else {
			uint16_t *ptr = NULL;

			ptr = (uint16_t  *)isp_k_param->lsc_buf_info.sw_addr;
			pr_debug("lsc[0]: 0x%0x, 0x%0x, 0x%0x, 0x%0x",
				*ptr, *(ptr + 1), *(ptr + 2), *(ptr + 3));
			pr_debug("lsc[1]: 0x%0x, 0x%0x, 0x%0x, 0x%0x",
				*(ptr + 4), *(ptr + 5), *(ptr + 6), *(ptr + 7));
		}

		if (ISP_GET_SID(idx) == ISP_SCENE_CAP) {
			dev->isp_k_param.lsc_cap_grid_width =
					lens_info.grid_width & 0x1FF;
			dev->isp_k_param.lsc_cap_grid_pitch =
					lens_info.grid_pitch & 0x1FF;
			goto load_qvalue;
		}

		ret = isp_k_2d_lsc_cfg_load_shading(
					isp_k_param->lsc_buf_info.sw_addr,
					idx, &lens_info);
	}

	if (ret) {
		pr_err("fail to cfg lsc gain buf\n");
		return ret;
	}

load_qvalue:
	for (i = 0; i < 5; i++) {
		val = ((lens_info.q_value[0][i] & 0x3FFF) << 16) |
			(lens_info.q_value[1][i] & 0x3FFF);
		ISP_REG_WR(idx, ISP_LENS_Q0_VALUE + i * 4, val);
	}

	ret = isp_k_2d_lsc_load_weight(isp_k_param, idx, &lens_info);
	if (ret) {
		pr_err("fail to load lens weight buf\n");
		return ret;
	}

	return 0;
}

static int isp_k_2d_lsc_slice_size(struct isp_io_param *param,
				   enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_img_size size = {0, 0};

	ret = copy_from_user((void *)&size,
			param->property_param,
			sizeof(size));
	if (ret != 0) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return -EPERM;
	}

	val = ((size.height & 0xFFFF) << 16) | (size.width & 0xFFFF);

	ISP_REG_WR(idx, ISP_LENS_SLICE_SIZE, val);

	return ret;
}

int isp_k_cfg_2d_lsc(struct isp_io_param *param,
		     struct isp_k_block *isp_k_param,
		     enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param null.\n");
		return -EPERM;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param null.\n");
		return -EPERM;
	}

	switch (param->property) {
	case ISP_PRO_2D_LSC_BLOCK:
		ret = isp_k_2d_lsc_block(param, isp_k_param, idx);
		break;
	case ISP_PRO_2D_LSC_SLICE_SIZE:
		ret = isp_k_2d_lsc_slice_size(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
				param->property);
		break;
	}

	return ret;
}
