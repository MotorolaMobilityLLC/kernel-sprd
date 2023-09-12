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
#include "dcam_block.h"
#include "dcam_drv.h"
#include "dcam_buf.h"
#include "isp_drv.h"
#if defined(CONFIG_COMPAT)
#include "compat_isp_drv.h"
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "DCAM_2D_LSC: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

static int dcam_k_2d_lsc_block(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum dcam_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	unsigned int i = 0;
	void *data_ptr = NULL;
	unsigned short *w_buff = NULL;
	struct isp_dev_2d_lsc_info lens_info;
	unsigned int dst_w_num = 0;
	unsigned int *dst_addr = NULL;

	memset(&lens_info, 0x00, sizeof(lens_info));

	ret = copy_from_user((void *)&lens_info, param->property_param,
						sizeof(lens_info));
	if (unlikely(ret != 0)) {
		pr_err("fail to copy param from user, ret=%d\n", ret);
		return -EPERM;
	}

	sprd_dcam_glb_reg_mwr(idx, DCAM0_AEM_PARA,
		      BIT_0,
		      lens_info.bypass,
		      DCAM_REG_MAX);

	if (lens_info.bypass) {
		pr_debug("lens bypass.\n");
		return 0;
	}

	/* set the bit27 is 1 for default */
	val = (1 << 27) |
		((lens_info.grid_num_t & 0x7FF) << 16) |
		((lens_info.grid_y_num & 0xFF) << 8) |
		(lens_info.grid_x_num & 0xFF);
	sprd_dcam_glb_reg_mwr(idx, DCAM0_LENS_GRID_NUMBER,
			BIT_27 | (0x07FF << 16) | (0xFF << 8) | (0xFF),
			val,
			DCAM_REG_MAX);

	sprd_dcam_glb_reg_mwr(idx, DCAM0_LENS_LOAD_ENABLE,
	      BIT_0,
	      1,
	      DCAM_REG_MAX);

	DCAM_REG_WR(idx, DCAM0_LENS_BASE_RADDR, isp_k_param->lsc_buf_phys_addr);

	val = (lens_info.grid_width & 0x000001FF);
	sprd_dcam_glb_reg_mwr(idx, DCAM0_LENS_GRID_SIZE,
			0x000001FF,
			val,
			DCAM_REG_MAX);

#ifdef CONFIG_64BIT
	data_ptr = (void *)(((unsigned long)lens_info.data_ptr[1] << 32)
					| lens_info.data_ptr[0]);
#else
	data_ptr = (void *)(lens_info.data_ptr[0]);
#endif

	if (lens_info.weight_num >=
		LENS_W_BUF_SIZE) {
		pr_err("fail to weight_num:0x%x bigger than buf:0x%x.\n",
				lens_info.weight_num,
				LENS_W_BUF_SIZE);
		return -EPERM;
	}

	if (!isp_k_param->dcam_lens_w_addr) {
		pr_err("fail to get dcam lens weight addr!\n");
		return -EFAULT;
	}
	w_buff = (unsigned short *)isp_k_param->dcam_lens_w_addr;

	ret = copy_from_user((void *)w_buff,
		(const void __user *)data_ptr,
		lens_info.weight_num);

	if (unlikely(ret != 0)) {
		pr_err("fail to copy wtable from user, ret = %d\n", ret);
		goto exit;
	}

	dst_w_num = (lens_info.grid_width >> 1) + 1;
	dst_addr =
		vzalloc((dst_w_num << 1) * sizeof(unsigned int));
	if (unlikely(dst_addr == NULL)) {
		ret = -ENOMEM;
		goto exit;
	}

	for (i = 0; i < dst_w_num ; i++) {
		*((unsigned int *)dst_addr + i*2) =
			(((unsigned int)(*(w_buff+i*3))) & 0xFFFF) |
			((((unsigned int)(*(w_buff+i*3+1))) & 0xFFFF)
			<< 16);

		*((unsigned int *)dst_addr + i*2+1) =
			((unsigned int)(*(w_buff+i*3+2))) & 0xFFFF;
	}

	for (i = 0; i < (dst_w_num << 1); i++) {
		DCAM_REG_WR(idx, (DCAM0_AEM_WEI_TABLE + (i << 2)),
			*(dst_addr + i));
	}

	sprd_dcam_glb_reg_mwr(idx, DCAM0_APB_SRAM_CTRL,
		BIT_0,
		1,
		DCAM_REG_MAX);

exit:

	if (dst_addr != NULL)
		vfree(dst_addr);

	return ret;
}


static int dcam_k_2d_lsc_transaddr(struct isp_io_param *param,
			struct isp_k_block *isp_k_param)
{
	int ret = 0;
	struct isp_dev_block_addr lsc_buf;
	struct isp_statis_buf lsc_remap_buf;
	struct isp_dev_block_addr __user *data;

	memset(&lsc_buf, 0x00, sizeof(struct isp_dev_block_addr));
	memset(&lsc_remap_buf, 0x00, sizeof(struct isp_statis_buf));

	data = param->property_param;
	ret = copy_from_user(&lsc_buf,
		(const void __user *)data,
		sizeof(lsc_buf));
	if (unlikely(ret != 0)) {
		pr_err("fail to copy lsc table from user, ret = %d\n", ret);
		return -EPERM;
	}

	lsc_remap_buf.pfinfo.dev = &s_dcam_pdev->dev;
	lsc_remap_buf.pfinfo.mfd[0] = lsc_buf.img_fd;

	/*mapping iommu buffer*/
	ret = pfiommu_get_sg_table(&lsc_remap_buf.pfinfo);
	if (unlikely(ret)) {
		pr_err("fail to map iommu lsc buffer.\n");
		return ret;
	}

	ret = pfiommu_get_addr(&lsc_remap_buf.pfinfo);
	if (ret) {
		pr_err("fail to get 2d lsc addr.\n");
		return ret;
	}
	isp_k_param->lsc_pfinfo.iova[0] =
		lsc_remap_buf.pfinfo.iova[0];
	isp_k_param->lsc_buf_phys_addr = lsc_remap_buf.pfinfo.iova[0]
		+ lsc_buf.img_offset.chn0;
	isp_k_param->lsc_pfinfo.size[0] = lsc_remap_buf.pfinfo.size[0];
	isp_k_param->lsc_pfinfo.dev = lsc_remap_buf.pfinfo.dev;

	pr_debug("lsc addr 0x%lx, size 0x%zx\n",
		isp_k_param->lsc_pfinfo.iova[0],
		lsc_remap_buf.pfinfo.size[0]);

	return ret;
}

int dcam_k_cfg_2d_lsc(struct isp_io_param *param,
			struct isp_k_block *isp_k_param, enum dcam_id idx)
{
	int ret = 0;

	if (unlikely(!param || !param->property_param)) {
		pr_err("fail to get param is null.\n");
		return -EPERM;
	}

	switch (param->property) {
	case DCAM_PRO_2D_LSC_BLOCK:
		ret = dcam_k_2d_lsc_block(param, isp_k_param, idx);
		break;
	case DCAM_PRO_2D_LSC_TRANSADDR:
		ret = dcam_k_2d_lsc_transaddr(param, isp_k_param);
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}
