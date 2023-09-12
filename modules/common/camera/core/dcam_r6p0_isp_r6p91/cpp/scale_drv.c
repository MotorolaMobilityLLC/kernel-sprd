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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/io.h>

#include "scale_drv.h"
#include "gen_scale_coef.h"
#include "dcam_drv.h"
#include <sprd_mm.h>

#define SCALE_LOWEST_ADDR 0x800
#define SCALE_ADDR_INVALID(addr) ((addr) < SCALE_LOWEST_ADDR)
#define SCALE_YUV_ADDR_INVALID(y, u, v) \
	(SCALE_ADDR_INVALID(y) && \
	SCALE_ADDR_INVALID(u) && \
	SCALE_ADDR_INVALID(v))
#define SC_COEFF_H_TAB_OFFSET 0x1400
#define SC_COEFF_V_TAB_OFFSET 0x14F0
#define SC_COEFF_V_CHROMA_TAB_OFFSET 0x18F0
#define SC_COEFF_COEF_SIZE (1 << 10)
#define SC_COEFF_TMP_SIZE (21 << 10)
#define SC_H_COEF_SIZE (0xC0)
#define SC_V_COEF_SIZE (0x210)
#define SC_V_CHROM_COEF_SIZE (0x210)
#define SC_COEFF_H_NUM (SC_H_COEF_SIZE / 4)
#define SC_COEFF_V_NUM (SC_V_COEF_SIZE / 4)
#define SC_COEFF_V_CHROMA_NUM (SC_V_CHROM_COEF_SIZE / 4)
#define SCALE_PIXEL_ALIGNED 4
#define SCALE_SLICE_HEIGHT_ALIGNED 4

static struct platform_device *s_scale_pdev;
int scale_k_module_en(struct device_node *dn)
{
	int ret = 0;

	ret = dcam_module_en(dn);
	if (ret) {
		pr_err("%s, failed: %d\n", __func__, ret);
		goto dcam_eb_fail;
	}
	dcam_get_resizer();

dcam_eb_fail:
	return ret;
}

int scale_k_module_dis(struct device_node *dn)
{
	int ret = 0;

	dcam_rel_resizer();
	ret = dcam_module_dis(dn);
	if (ret)
		pr_err("%s, failed: %d\n", __func__, ret);

	return ret;
}

static int scale_k_check_deci_slice_mode(uint32_t deci_val, uint32_t slice_h)
{
	int rtn = 0;

	if (deci_val > 0) {
		if ((slice_h >= deci_val) && (0 == (slice_h % deci_val)))
			rtn = 0;
		else
			rtn = -1;
	}
	return rtn;
}

static int scale_k_set_sc_coeff(struct scale_path_info *path_info_ptr)
{
	uint32_t i = 0;
	unsigned long h_coeff_addr = SCALE_BASE;
	unsigned long v_coeff_addr = SCALE_BASE;
	unsigned long v_chroma_coeff_addr = SCALE_BASE;
	uint32_t *tmp_buf = NULL;
	uint32_t *h_coeff = NULL;
	uint32_t *v_coeff = NULL;
	uint32_t *v_chroma_coeff = NULL;
	uint32_t scale2yuv420 = 0;
	uint8_t y_tap = 0;
	uint8_t uv_tap = 0;

	if (!path_info_ptr) {
		pr_err("scale_k_set_sc_coeff error: path_info_ptr null\n");
		return -1;
	}

	tmp_buf = (uint32_t *)path_info_ptr->coeff_addr;
	if (tmp_buf == NULL) {
		pr_err("scale_k_set_sc_coeff error: coeff mem null\n");
		return -1;
	}

	h_coeff_addr += SC_COEFF_H_TAB_OFFSET;
	v_coeff_addr += SC_COEFF_V_TAB_OFFSET;
	v_chroma_coeff_addr += SC_COEFF_V_CHROMA_TAB_OFFSET;

	if (path_info_ptr->output_format == SCALE_YUV420)
		scale2yuv420 = 1;

	h_coeff = tmp_buf;
	v_coeff = tmp_buf + (SC_COEFF_COEF_SIZE/4);
	v_chroma_coeff = v_coeff + (SC_COEFF_COEF_SIZE/4);
	/* dcam_gen_scale_coeff */
	if (!(dcam_gen_scale_coeff((int16_t)path_info_ptr->sc_input_size.w,
		(int16_t)path_info_ptr->sc_input_size.h,
		(int16_t)path_info_ptr->output_size.w,
		(int16_t)path_info_ptr->output_size.h,
		h_coeff,
		v_coeff,
		v_chroma_coeff,
		scale2yuv420,
		&y_tap,
		&uv_tap,
		tmp_buf + (SC_COEFF_COEF_SIZE*3/4),
		SC_COEFF_TMP_SIZE))) {
		pr_err("scale_k_set_sc_coeff error: gen scale coeff\n");
		return -1;
	}

	for (i = 0; i < SC_COEFF_H_NUM; i++) {
		REG_WR(h_coeff_addr, *h_coeff);
		h_coeff_addr += 4;
		h_coeff++;
	}

	for (i = 0; i < SC_COEFF_V_NUM; i++) {
		REG_WR(v_coeff_addr, *v_coeff);
		v_coeff_addr += 4;
		v_coeff++;
	}

	for (i = 0; i < SC_COEFF_V_CHROMA_NUM; i++) {
		REG_WR(v_chroma_coeff_addr, *v_chroma_coeff);
		v_chroma_coeff_addr += 4;
		v_chroma_coeff++;
	}

	REG_MWR(SCALE_CFG, (BIT_19 | BIT_18 | BIT_17 | BIT_16),
		((y_tap & 0x0F) << 16));
	REG_MWR(SCALE_CFG, (BIT_15 | BIT_14 | BIT_13 | BIT_12 | BIT_11),
		((uv_tap & 0x1F) << 11));

	return 0;
}

static int scale_k_calc_sc_size(struct scale_path_info *path_info_ptr)
{
	int rtn = 0;
	uint32_t reg_val = 0;
	uint32_t div_factor = 1;
	uint32_t i = 0, pixel_aligned_num = 0;

	if (!path_info_ptr) {
		pr_err("scale_k_calc_sc_size error: path_info_ptr null\n");
		return -1;
	}

	if (path_info_ptr->input_rect.w > (path_info_ptr->output_size.w *
		SCALE_SC_COEFF_MAX * (1 << SCALE_DECI_FAC_MAX)) ||
		path_info_ptr->input_rect.h > (path_info_ptr->output_size.h *
		SCALE_SC_COEFF_MAX * (1 << SCALE_DECI_FAC_MAX)) ||
		path_info_ptr->input_rect.w * SCALE_SC_COEFF_MAX <
		path_info_ptr->output_size.w ||
		path_info_ptr->input_rect.h * SCALE_SC_COEFF_MAX <
		path_info_ptr->output_size.h) {
		pr_err("scale_k_cl_sc_size error:input{%d %d}, output{%d %d}\n",
		    path_info_ptr->input_rect.w, path_info_ptr->input_rect.h,
		    path_info_ptr->output_size.w, path_info_ptr->output_size.h);
		rtn = -1;
	} else {
		path_info_ptr->sc_input_size.w = path_info_ptr->input_rect.w;
		path_info_ptr->sc_input_size.h = path_info_ptr->input_rect.h;
		if (path_info_ptr->input_rect.w >
			path_info_ptr->output_size.w * SCALE_SC_COEFF_MAX ||
			path_info_ptr->input_rect.h >
			path_info_ptr->output_size.h * SCALE_SC_COEFF_MAX) {
			for (i = 0; i < SCALE_DECI_FAC_MAX; i++) {
				div_factor =
				(uint32_t)(SCALE_SC_COEFF_MAX * (1 << (1 + i)));
				if (path_info_ptr->input_rect.w <=
				  (path_info_ptr->output_size.w * div_factor) &&
				  path_info_ptr->input_rect.h <=
				  (path_info_ptr->output_size.h * div_factor)) {
					break;
				}
			}
			path_info_ptr->sc_deci_val = (1 << (1 + i));
			pixel_aligned_num =
			   (path_info_ptr->sc_deci_val >= SCALE_PIXEL_ALIGNED) ?
			   path_info_ptr->sc_deci_val : SCALE_PIXEL_ALIGNED;
			path_info_ptr->sc_input_size.w =
				path_info_ptr->input_rect.w >> (1 + i);
			path_info_ptr->sc_input_size.h =
				path_info_ptr->input_rect.h >> (1 + i);
			if ((path_info_ptr->sc_input_size.w %
			     pixel_aligned_num) ||
			    (path_info_ptr->sc_input_size.h %
			     pixel_aligned_num)) {
				path_info_ptr->sc_input_size.w =
				    path_info_ptr->sc_input_size.w /
				    pixel_aligned_num * pixel_aligned_num;
				path_info_ptr->sc_input_size.h =
				  path_info_ptr->sc_input_size.h /
				  pixel_aligned_num *  pixel_aligned_num;
				path_info_ptr->input_rect.w =
				  path_info_ptr->sc_input_size.w << (1 + i);
				path_info_ptr->input_rect.h =
				  path_info_ptr->sc_input_size.h << (1 + i);
			}

			REG_WR(SCALE_TRIM_START, 0);
			reg_val = path_info_ptr->sc_input_size.w |
				(path_info_ptr->sc_input_size.h << 16);
			REG_WR(SCALE_TRIM_SIZE, reg_val);
			REG_WR(SCALE_SRC_SIZE, reg_val);

			REG_OWR(SCALE_REV_BURST_IN_CFG,
				SCALE_BURST_SUB_EB_BIT);
			REG_MWR(SCALE_REV_BURST_IN_CFG,
				SCALE_BURST_SUB_SAMPLE_MASK, (i << 13));
			REG_MWR(SCALE_REV_BURST_IN_CFG,
				SCALE_BURST_SRC_WIDTH_MASK,
				path_info_ptr->input_size.w);
			reg_val = path_info_ptr->input_rect.x |
				(path_info_ptr->input_rect.y << 16);
			REG_WR(SCALE_REV_BURST_IN_TRIM_START, reg_val);
			reg_val = path_info_ptr->input_rect.w |
				(path_info_ptr->input_rect.h << 16);
			REG_WR(SCALE_REV_BURST_IN_TRIM_SIZE, reg_val);
		}

	}

	return rtn;
}

static int scale_k_cfg_scaler(struct scale_path_info *path_info_ptr)
{
	int rtn = 0;

	if (!path_info_ptr) {
		pr_err("scale_k_cfg_scaler error: path_info_ptr null\n");
		return -1;
	}

	rtn = scale_k_calc_sc_size(path_info_ptr);
	if (rtn) {
		pr_err("scale_k_cfg_scaler error: calc\n");
		return rtn;
	}

	if (path_info_ptr->sc_input_size.w != path_info_ptr->output_size.w ||
	path_info_ptr->sc_input_size.h != path_info_ptr->output_size.h ||
	path_info_ptr->output_format == SCALE_YUV420) {
		REG_MWR(SCALE_CFG, SCALE_BYPASS_BIT, 0);
		rtn = scale_k_set_sc_coeff(path_info_ptr);
		if (rtn)
			pr_err("scale_k_cfg_scaler error: coeff\n");
	} else {
		REG_OWR(SCALE_CFG, SCALE_BYPASS_BIT);
	}

	return rtn;
}

int scale_k_slice_cfg(struct scale_slice_param_t *cfg_ptr,
	       struct scale_path_info *path_info_ptr)
{
	int rtn = 0;
	uint32_t reg_val = 0;

	if (!cfg_ptr || !path_info_ptr) {
		pr_err("scale_k_io_cfg error: cfg_ptr=%p path_info_ptr=%p",
			cfg_ptr, path_info_ptr);
		return -1;
	}

	/*set slice scale height*/
	if (cfg_ptr->slice_height > SCALE_FRAME_HEIGHT_MAX
		|| (cfg_ptr->slice_height  % SCALE_SLICE_HEIGHT_ALIGNED)) {
		pr_err("scale_k_io_cfg error: slice_height:%d\n",
			cfg_ptr->slice_height);
		rtn = -1;
		goto cfg_exit;
	} else {
		REG_MWR(SCALE_REV_SLICE_CFG,
			SCALE_INPUT_SLICE_HEIGHT_MASK, cfg_ptr->slice_height);
		path_info_ptr->slice_height = cfg_ptr->slice_height;
	}

	/*set input rect*/
	if (cfg_ptr->input_rect.x > SCALE_FRAME_WIDTH_MAX
		|| cfg_ptr->input_rect.y > SCALE_FRAME_HEIGHT_MAX
		|| cfg_ptr->input_rect.w > SCALE_FRAME_WIDTH_MAX
		|| cfg_ptr->input_rect.h > SCALE_FRAME_HEIGHT_MAX) {
		pr_err("scale_k_io_cfg error: input_rect {%d %d %d %d}\n",
			cfg_ptr->input_rect.x, cfg_ptr->input_rect.y,
			cfg_ptr->input_rect.w, cfg_ptr->input_rect.h);
		rtn = -1;
		goto cfg_exit;
	} else {
		reg_val = cfg_ptr->input_rect.x | (cfg_ptr->input_rect.y << 16);
		REG_WR(SCALE_REV_BURST_IN_TRIM_START, reg_val);
		REG_WR(SCALE_TRIM_START, 0);
		reg_val = cfg_ptr->input_rect.w | (cfg_ptr->input_rect.h << 16);
		REG_WR(SCALE_REV_BURST_IN_TRIM_SIZE, reg_val);
		REG_WR(SCALE_TRIM_SIZE, reg_val);
		REG_WR(SCALE_SRC_SIZE, reg_val);
		memcpy((void *)&path_info_ptr->input_rect,
			(void *)&cfg_ptr->input_rect,
			sizeof(struct scale_rect_t));
	}

	/*set input address*/
	if (SCALE_YUV_ADDR_INVALID(cfg_ptr->input_addr.yaddr,
		cfg_ptr->input_addr.uaddr, cfg_ptr->input_addr.vaddr)) {
		pr_err("scale_k_io_cfg error: input_addr {%x %x %x}\n",
			cfg_ptr->input_addr.yaddr, cfg_ptr->input_addr.uaddr,
			cfg_ptr->input_addr.vaddr);
		rtn = -1;
		goto cfg_exit;
	} else {
		REG_WR(SCALE_FRM_IN_Y, cfg_ptr->input_addr.yaddr);
		REG_WR(SCALE_FRM_IN_U, cfg_ptr->input_addr.uaddr);
		REG_WR(SCALE_FRM_IN_V, cfg_ptr->input_addr.vaddr);
		path_info_ptr->input_addr.yaddr = cfg_ptr->input_addr.yaddr;
		path_info_ptr->input_addr.uaddr = cfg_ptr->input_addr.uaddr;
		path_info_ptr->input_addr.vaddr = cfg_ptr->input_addr.vaddr;
	}

	/*set output address*/
	if (SCALE_YUV_ADDR_INVALID(cfg_ptr->output_addr.yaddr,
		cfg_ptr->output_addr.uaddr, cfg_ptr->output_addr.vaddr)) {
		pr_err("scale_k_io_cfg error: output_addr {%x %x %x}\n",
			cfg_ptr->output_addr.yaddr, cfg_ptr->output_addr.uaddr,
			cfg_ptr->output_addr.vaddr);
		rtn = -1;
		goto cfg_exit;
	} else {
		REG_WR(SCALE_FRM_OUT_Y, cfg_ptr->output_addr.yaddr);
		REG_WR(SCALE_FRM_OUT_U, cfg_ptr->output_addr.uaddr);
		REG_WR(SCALE_FRM_OUT_V, cfg_ptr->output_addr.vaddr);
		path_info_ptr->output_addr.yaddr = cfg_ptr->output_addr.yaddr;
		path_info_ptr->output_addr.uaddr = cfg_ptr->output_addr.uaddr;
		path_info_ptr->output_addr.vaddr = cfg_ptr->output_addr.vaddr;
	}

cfg_exit:
	return rtn;
}

int scale_k_frame_cfg(struct scale_frame_param_t *cfg_ptr,
		struct scale_path_info *path_info_ptr)
{
	int rtn = 0;
	uint32_t reg_val = 0;
	unsigned int            src_addr[3] = {0};
	unsigned int            dst_addr[3] = {0};

	if (!cfg_ptr || !path_info_ptr) {
		pr_err("scale_k_io_cfg error: cfg_ptr=%p path_info_ptr=%p",
			cfg_ptr, path_info_ptr);
		return -1;
	}

	SCALE_TRACE("scale_k_io_cfg :  input_size {%d %d}\n",
		cfg_ptr->input_size.w, cfg_ptr->input_size.h);
	/*set input size*/
	if (cfg_ptr->input_size.w > SCALE_FRAME_WIDTH_MAX
		|| cfg_ptr->input_size.h > SCALE_FRAME_HEIGHT_MAX) {
		pr_err("scale_k_io_cfg error:  input_size {%d %d}\n",
			cfg_ptr->input_size.w, cfg_ptr->input_size.h);
		rtn = -1;
		goto cfg_exit;
	} else {
		REG_MWR(SCALE_REV_BURST_IN_CFG, SCALE_BURST_SUB_EB_BIT, 0);
		REG_MWR(SCALE_REV_BURST_IN_CFG, SCALE_BURST_SUB_SAMPLE_MASK, 0);
		REG_MWR(SCALE_REV_BURST_IN_CFG, SCALE_BURST_SRC_WIDTH_MASK,
			cfg_ptr->input_size.w);
		path_info_ptr->input_size.w = cfg_ptr->input_size.w;
		path_info_ptr->input_size.h = cfg_ptr->input_size.h;
	}

	SCALE_TRACE("scale_k_io_cfg : input_rect {%d %d %d %d}\n",
		cfg_ptr->input_rect.x, cfg_ptr->input_rect.y,
		cfg_ptr->input_rect.w, cfg_ptr->input_rect.h);
	/*set input rect*/
	if (cfg_ptr->input_rect.x > SCALE_FRAME_WIDTH_MAX
		|| cfg_ptr->input_rect.y > SCALE_FRAME_HEIGHT_MAX
		|| cfg_ptr->input_rect.w > SCALE_FRAME_WIDTH_MAX
		|| cfg_ptr->input_rect.h > SCALE_FRAME_HEIGHT_MAX) {
		pr_err("scale_k_io_cfg error: input_rect {%d %d %d %d}\n",
			cfg_ptr->input_rect.x, cfg_ptr->input_rect.y,
			cfg_ptr->input_rect.w, cfg_ptr->input_rect.h);
		rtn = -1;
		goto cfg_exit;
	} else {
		reg_val = cfg_ptr->input_rect.x | (cfg_ptr->input_rect.y << 16);
		REG_WR(SCALE_REV_BURST_IN_TRIM_START, reg_val);
		REG_WR(SCALE_TRIM_START, 0);
		cfg_ptr->input_rect.w = (cfg_ptr->input_rect.w&(~(8 - 1)));
		cfg_ptr->input_rect.h = (cfg_ptr->input_rect.h&(~(4 - 1)));
		reg_val = cfg_ptr->input_rect.w | (cfg_ptr->input_rect.h << 16);
		REG_WR(SCALE_REV_BURST_IN_TRIM_SIZE, reg_val);
		REG_WR(SCALE_TRIM_SIZE, reg_val);
		REG_WR(SCALE_SRC_SIZE, reg_val);
		memcpy((void *)&path_info_ptr->input_rect,
			(void *)&cfg_ptr->input_rect,
			sizeof(struct scale_rect_t));
	}

	SCALE_TRACE("scale_k_io_cfg : input_format:%d\n",
		cfg_ptr->input_format);
	/*set input foramt*/
	path_info_ptr->input_format = cfg_ptr->input_format;
	if (cfg_ptr->input_format == SCALE_YUV422
		|| cfg_ptr->input_format == SCALE_YUV420 ||
		cfg_ptr->input_format == SCALE_YUV420_3FRAME) {
		REG_MWR(SCALE_REV_BURST_IN_CFG, SCALE_BURST_INPUT_MODE_MASK,
			(cfg_ptr->input_format << 16));
	} else {
		pr_err("scale_k_io_cfg error: input_format:%d\n",
			cfg_ptr->input_format);
		rtn = -1;
		goto cfg_exit;
	}

	SCALE_TRACE("scale_k_io_cfg : input_addr {%x %x %x}\n",
		cfg_ptr->input_addr.yaddr, cfg_ptr->input_addr.uaddr,
		cfg_ptr->input_addr.vaddr);
	/*set input address*/

	if (cfg_ptr->input_addr.mfd[0] == 0 ||
		cfg_ptr->input_addr.mfd[1] == 0) {
		pr_info("invalid input mfd %d %d %d\n",
			cfg_ptr->input_addr.mfd[0],
			cfg_ptr->input_addr.mfd[1],
			cfg_ptr->input_addr.mfd[2]);
	} else {
		memcpy(path_info_ptr->scale_iommu_src.mfd,
			cfg_ptr->input_addr.mfd, 3 * sizeof(unsigned int));
		path_info_ptr->scale_iommu_src.dev = &s_scale_pdev->dev;
		rtn = pfiommu_get_sg_table(&path_info_ptr->scale_iommu_src);
		if (rtn) {
			pr_err("scale src get sg table failed, fd 0x%x\n",
				path_info_ptr->scale_iommu_src.mfd[0]);
			rtn = DCAM_RTN_PATH_ADDR_ERR;
			goto cfg_exit;
		}
		path_info_ptr->scale_iommu_src.offset[0] = 0;
		path_info_ptr->scale_iommu_src.offset[1] = 0;
		path_info_ptr->scale_iommu_src.offset[2] = 0;

		rtn = pfiommu_get_addr(&path_info_ptr->scale_iommu_src);
		if (rtn) {
			pr_err("scale src get addr failed, fd 0x%x\n",
				path_info_ptr->scale_iommu_src.mfd[0]);
			rtn = DCAM_RTN_PATH_ADDR_ERR;
			goto cfg_exit;
		}
		src_addr[0] = path_info_ptr->scale_iommu_src.iova[0] +
					cfg_ptr->input_addr.yaddr;
		src_addr[1] = path_info_ptr->scale_iommu_src.iova[0] +
					cfg_ptr->input_addr.uaddr;
		src_addr[2] = path_info_ptr->scale_iommu_src.iova[0] +
					cfg_ptr->input_addr.vaddr;

		if (src_addr[0] == 0x0 || src_addr[1] == 0x0) {
			pr_err("error scale src addr y 0x%x uv 0x%x.\n",
						src_addr[0], src_addr[1]);
			rtn = DCAM_RTN_PATH_NO_MEM;
		} else {
			REG_WR(SCALE_FRM_IN_Y, src_addr[0]);
			REG_WR(SCALE_FRM_IN_U, src_addr[1]);
			if (cfg_ptr->input_format == SCALE_YUV420_3FRAME)
				REG_WR(SCALE_FRM_IN_V, src_addr[2]);
		}

		SCALE_TRACE("iommu_src y:0x%x, uv:0x%x\n", src_addr[0],
			src_addr[1]);
	}

	SCALE_TRACE("scale_k_io_cfg : input_endian {%d %d}\n",
		cfg_ptr->input_endian.y_endian,
		cfg_ptr->input_endian.uv_endian);
	/*set input endian*/
	if (cfg_ptr->input_endian.y_endian >= SCALE_ENDIAN_MAX
		|| cfg_ptr->input_endian.uv_endian >= SCALE_ENDIAN_MAX) {
		pr_err("scale_k_io_cfg error: input_endian {%d %d}\n",
			cfg_ptr->input_endian.y_endian,
			cfg_ptr->input_endian.uv_endian);
		rtn = -1;
		goto cfg_exit;
	} else {
		dcam_glb_reg_owr(SCALE_ENDIAN_SEL, (SCALE_AXI_RD_ENDIAN_BIT |
			SCALE_AXI_WR_ENDIAN_BIT), DCAM_ENDIAN_REG);
		dcam_glb_reg_mwr(SCALE_ENDIAN_SEL, SCALE_INPUT_Y_ENDIAN_MASK,
			cfg_ptr->input_endian.y_endian, DCAM_ENDIAN_REG);
		dcam_glb_reg_mwr(SCALE_ENDIAN_SEL, SCALE_INPUT_UV_ENDIAN_MASK,
			(cfg_ptr->input_endian.uv_endian << 2),
			DCAM_ENDIAN_REG);
		SCALE_TRACE("scale_k_io_cfg: output_endian {%d %d} 0x%x\n",
			cfg_ptr->input_endian.y_endian,
			cfg_ptr->input_endian.uv_endian,
			REG_RD(SCALE_ENDIAN_SEL));
	}

	SCALE_TRACE("scale_k_io_cfg: output_size {%d %d}\n",
		cfg_ptr->output_size.w, cfg_ptr->output_size.h);

	/*set output size*/
	if (cfg_ptr->output_size.w > SCALE_FRAME_WIDTH_MAX ||
		cfg_ptr->output_size.h > SCALE_FRAME_HEIGHT_MAX) {
		pr_err("scale_k_io_cfg error: output_size {%d %d}\n",
			cfg_ptr->output_size.w, cfg_ptr->output_size.h);
		rtn = -1;
		goto cfg_exit;
	} else {
		reg_val = cfg_ptr->output_size.w |
			(cfg_ptr->output_size.h << 16);
		REG_WR(SCALE_DST_SIZE, reg_val);
		path_info_ptr->output_size.w = cfg_ptr->output_size.w;
		path_info_ptr->output_size.h = cfg_ptr->output_size.h;
	}

	SCALE_TRACE("scale_k_io_cfg: output_format:%d\n",
		cfg_ptr->output_format);

	/*set output format*/
	path_info_ptr->output_format = cfg_ptr->output_format;
	if (cfg_ptr->output_format == SCALE_YUV422) {
		REG_MWR(SCALE_CFG, SCALE_OUTPUT_MODE_MASK, (0 << 6));
	} else if (cfg_ptr->output_format == SCALE_YUV420) {
		REG_MWR(SCALE_CFG, SCALE_OUTPUT_MODE_MASK, (1 << 6));
	} else if (cfg_ptr->output_format == SCALE_YUV420_3FRAME) {
		REG_MWR(SCALE_CFG, SCALE_OUTPUT_MODE_MASK, (3 << 6));
	} else {
		pr_err("scale_k_io_cfg error: output_format:%d\n",
			cfg_ptr->output_format);
		rtn = -1;
		goto cfg_exit;
	}

	SCALE_TRACE("scale_k_io_cfg : output_addr {%x %x %x}\n",
		cfg_ptr->output_addr.yaddr, cfg_ptr->output_addr.uaddr,
		cfg_ptr->output_addr.vaddr);

	/*set output address*/
	if (cfg_ptr->output_addr.mfd[0] == 0 ||
		cfg_ptr->output_addr.mfd[1] == 0) {
		pr_info("invalid output mfd %d %d %d\n",
			cfg_ptr->output_addr.mfd[0],
			cfg_ptr->output_addr.mfd[1],
			cfg_ptr->output_addr.mfd[2]);
	} else {
		memcpy(path_info_ptr->scale_iommu_dst.mfd,
			cfg_ptr->output_addr.mfd,
			3 * sizeof(unsigned int));
		path_info_ptr->scale_iommu_dst.dev = &s_scale_pdev->dev;
		rtn = pfiommu_get_sg_table(&path_info_ptr->scale_iommu_dst);
		if (rtn) {
			pr_err("scale dst sg table failed, fd 0x%x\n",
				path_info_ptr->scale_iommu_dst.mfd[0]);
			rtn = DCAM_RTN_PATH_ADDR_ERR;
			goto cfg_exit;
		}
		path_info_ptr->scale_iommu_dst.offset[0] = 0;
		path_info_ptr->scale_iommu_dst.offset[1] = 0;
		path_info_ptr->scale_iommu_dst.offset[2] = 0;
		rtn = pfiommu_get_addr(&path_info_ptr->scale_iommu_dst);
		if (rtn) {
			pr_err("scale dst get add fail, fd 0x%x\n",
				path_info_ptr->scale_iommu_dst.mfd[0]);
			rtn = DCAM_RTN_PATH_ADDR_ERR;
			goto cfg_exit;
		}
		dst_addr[0] = path_info_ptr->scale_iommu_dst.iova[0] +
					cfg_ptr->output_addr.yaddr;
		dst_addr[1] = path_info_ptr->scale_iommu_dst.iova[0] +
					cfg_ptr->output_addr.uaddr;
		dst_addr[2] = path_info_ptr->scale_iommu_dst.iova[0] +
					cfg_ptr->output_addr.vaddr;

		if (dst_addr[0] == 0x0 || dst_addr[1] == 0x0) {
			pr_err("error scale dst addr y 0x%x uv 0x%x.\n",
					dst_addr[0], dst_addr[1]);
			rtn = DCAM_RTN_PATH_NO_MEM;
		} else {
			REG_WR(SCALE_FRM_OUT_Y, dst_addr[0]);
			REG_WR(SCALE_FRM_OUT_U, dst_addr[1]);
			if (cfg_ptr->output_format == SCALE_YUV420_3FRAME)
				REG_WR(SCALE_FRM_OUT_V, dst_addr[2]);
		}

		SCALE_TRACE("iommu_dst y:0x%x, uv:0x%x\n", dst_addr[0],
			dst_addr[1]);
	}

	SCALE_TRACE("scale_k_io_cfg: output_endian {%d %d}\n",
		cfg_ptr->output_endian.y_endian,
		cfg_ptr->output_endian.uv_endian);

	/*set output endian*/
	if (cfg_ptr->output_endian.y_endian >= SCALE_ENDIAN_MAX ||
		cfg_ptr->output_endian.uv_endian >= SCALE_ENDIAN_MAX) {
		pr_err("scale_k_io_cfg error: output_endian {%d %d}\n",
			cfg_ptr->output_endian.y_endian,
			cfg_ptr->output_endian.uv_endian);
		rtn = -1;
		goto cfg_exit;
	} else {
		dcam_glb_reg_owr(SCALE_ENDIAN_SEL,
			(SCALE_AXI_RD_ENDIAN_BIT|SCALE_AXI_WR_ENDIAN_BIT),
			DCAM_ENDIAN_REG);
		dcam_glb_reg_mwr(SCALE_ENDIAN_SEL,
			SCALE_OUTPUT_Y_ENDIAN_MASK,
			(cfg_ptr->output_endian.y_endian << 10),
			DCAM_ENDIAN_REG);
		dcam_glb_reg_mwr(SCALE_ENDIAN_SEL,
			SCALE_OUTPUT_UV_ENDIAN_MASK,
			(cfg_ptr->output_endian.uv_endian << 12),
			DCAM_ENDIAN_REG);
		SCALE_TRACE("scale_k_io_cfg: output_endian {%d %d} 0x%x\n",
			cfg_ptr->output_endian.y_endian,
			cfg_ptr->output_endian.uv_endian,
			REG_RD(SCALE_ENDIAN_SEL));
	}

	SCALE_TRACE("scale_k_io_cfg: scale_mode:%d\n",
		cfg_ptr->scale_mode);

	/*set scale mode*/
	if (cfg_ptr->scale_mode >= SCALE_MODE_MAX) {
		pr_err("scale_k_io_cfg error: scale_mode:%d\n",
			cfg_ptr->scale_mode);
		rtn = -1;
		goto cfg_exit;
	} else {
		if (cfg_ptr->scale_mode == SCALE_MODE_NORMAL) {
			REG_MWR(SCALE_CFG, SCALE_MODE_MASK,
				SCALE_MODE_NORMAL_TYPE);
		} else if (cfg_ptr->scale_mode == SCALE_MODE_SLICE) {
			REG_MWR(SCALE_CFG, SCALE_MODE_MASK,
				SCALE_MODE_SLICE_TYPE);
			REG_MWR(SCALE_REV_SLICE_CFG, SCALE_SLICE_TYPE_BIT, 0);
		} else {
			REG_MWR(SCALE_CFG, SCALE_MODE_MASK,
				SCALE_MODE_SLICE_TYPE);
			REG_OWR(SCALE_REV_SLICE_CFG, SCALE_SLICE_TYPE_BIT);
		}
		path_info_ptr->scale_mode = cfg_ptr->scale_mode;
	}

	/*set slice scale height*/
	if (cfg_ptr->scale_mode == SCALE_MODE_SLICE
		|| cfg_ptr->scale_mode == SCALE_MODE_SLICE_READDR) {
		if (cfg_ptr->slice_height > SCALE_FRAME_HEIGHT_MAX ||
			(cfg_ptr->slice_height  % SCALE_SLICE_HEIGHT_ALIGNED)) {
			pr_err("scale_k_io_cfg error: slice_height:%d\n",
				cfg_ptr->slice_height);
			rtn = -1;
			goto cfg_exit;
		} else {
			REG_MWR(SCALE_REV_SLICE_CFG,
				SCALE_INPUT_SLICE_HEIGHT_MASK,
				cfg_ptr->slice_height);
			path_info_ptr->slice_height = cfg_ptr->slice_height;
		}
	}

cfg_exit:
	return rtn;
}

static void _scale_reg_trace(void)
{
	unsigned long                addr = 0;

	pr_info("scale register list");
	for (addr = SCALE_BASE; addr <= SCALE_BASE + 0xd0; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			REG_RD(addr),
			REG_RD(addr + 4),
			REG_RD(addr + 8),
			REG_RD(addr + 12));
	}
}

int scale_k_start(struct scale_frame_param_t *cfg_ptr,
	struct scale_path_info *path_info_ptr)
{
	int rtn = 0;

	if (!path_info_ptr) {
		pr_err("scale_k_start error: path_info_ptr null\n");
		return -1;
	}

	dcam_resize_start();

	rtn = scale_k_frame_cfg(cfg_ptr, path_info_ptr);
	if (rtn) {
		pr_err("scale_k_start error: frame cfg\n");
		goto start_exit;
	}

	if (path_info_ptr->scale_mode == SCALE_MODE_NORMAL) {
		if (path_info_ptr->output_size.w > SCALE_FRAME_WIDTH_MAX) {
			rtn = -1;
			pr_err("scale_k_start error: frame output_size_w=%d\n",
				path_info_ptr->output_size.w);
			goto start_exit;
		}
		if (path_info_ptr->output_size.w > SCALE_LINE_BUF_MAX) {
			REG_MWR(SCALE_BASE, BIT_15, BIT_15);
			pr_info("sc buf merge\n");
		}
	} else {
		if (path_info_ptr->output_size.w > SCALE_LINE_BUF_LENGTH) {
			rtn = -1;
			pr_err("scale_k_start error: frame output_size_w=%d\n",
				path_info_ptr->output_size.w);
			goto start_exit;
		}
	}

	path_info_ptr->slice_in_height = 0;
	path_info_ptr->slice_out_height = 0;
	path_info_ptr->is_last_slice = 0;
	path_info_ptr->sc_deci_val = 0;

	REG_MWR(SCALE_CFG, (SCALE_DEC_X_EB_BIT|SCALE_DEC_Y_EB_BIT), 0);

	rtn = scale_k_cfg_scaler(path_info_ptr);
	if (rtn) {
		pr_err("scale_k_start error: cfg\n");
		goto start_exit;
	}

	if (path_info_ptr->scale_mode != SCALE_MODE_NORMAL) {
		path_info_ptr->slice_in_height += path_info_ptr->slice_height;
		rtn = scale_k_check_deci_slice_mode(path_info_ptr->sc_deci_val,
			path_info_ptr->slice_height);
		if (rtn)
			goto start_exit;
	}
	dcam_glb_reg_mwr(SCALE_BASE, SCALE_PATH_MASK,
		SCALE_PATH_SELECT, DCAM_CFG_REG);

	dcam_glb_reg_mwr(SCALE_CFG, BIT_10 | BIT_9, 0, DCAM_CFG_REG);
	dcam_glb_reg_owr(SCALE_BASE, SCALE_PATH_EB_BIT, DCAM_CFG_REG);
	dcam_glb_reg_owr(SCALE_CTRL,
		SCALE_COEFF_FRC_COPY_BIT, DCAM_CONTROL_REG);
	REG_WR(SCALE_BURST_GAP, 0x10000000);
	dcam_irq_set(SCALER_IRQ_LINE_MASK);
	_scale_reg_trace();
	REG_OWR(SCALE_REV_BURST_IN_CFG, SCALE_START_BIT);
	pr_err("scale_k_start\n");
	return rtn;
start_exit:
	dcam_resize_end();
	pr_info("scale_k_start error: ret=%d\n", rtn);
	return rtn;
}

int scale_k_continue(struct scale_slice_param_t *cfg_ptr,
	struct scale_path_info *path_info_ptr)
{
	int rtn = 0;
	uint32_t slice_h = 0;

	if (!path_info_ptr) {
		pr_err("scale_k_continue error: path_info_ptr null\n");
		return -1;
	}

	rtn = scale_k_slice_cfg(cfg_ptr, path_info_ptr);
	if (rtn) {
		pr_err("rot_k_ioctl error: slice cfg\n");
		return -1;
	}


	SCALE_TRACE("scale_k_continue: { %d, %d, %d}\n",
		path_info_ptr->slice_height, path_info_ptr->slice_in_height,
		path_info_ptr->scale_mode);

	if (path_info_ptr->scale_mode != SCALE_MODE_NORMAL) {
		if (path_info_ptr->slice_in_height +
		   path_info_ptr->slice_height >= path_info_ptr->input_rect.h) {
			slice_h = path_info_ptr->input_rect.h -
				path_info_ptr->slice_in_height;
			if (scale_k_check_deci_slice_mode(
				path_info_ptr->sc_deci_val, slice_h)) {
				pr_err("scale_k_continue error: check deci\n");
				return -1;
			}
			path_info_ptr->is_last_slice = 1;
			REG_MWR(SCALE_REV_SLICE_CFG,
				SCALE_INPUT_SLICE_HEIGHT_MASK, slice_h);
			REG_OWR(SCALE_REV_SLICE_CFG, SCALE_IS_LAST_SLICE_BIT);
		} else {
			path_info_ptr->is_last_slice = 0;
			REG_MWR(SCALE_REV_SLICE_CFG,
				SCALE_IS_LAST_SLICE_BIT, 0);
		}
		path_info_ptr->slice_in_height += path_info_ptr->slice_height;
	}

	dcam_glb_reg_owr(SCALE_CTRL, SCALE_START_BIT, DCAM_CONTROL_REG);

	return rtn;
}

int scale_k_stop(void)
{
	int rtn = 0;
	unsigned int reg_val[2];
	unsigned int cnt = 0;

	udelay(100);
	while (cnt < 100) {
		reg_val[0] = REG_RD(SCALE_AMBA_STS);
		reg_val[1] = REG_RD(SCALE_AXIM_DBG_STS);
		pr_debug("scale status:%d 0x%x 0x%x\n",
			cnt, reg_val[0], reg_val[1]);

		if ((reg_val[0] & BIT(2)) || (reg_val[1] & (BIT(1)|BIT(3)))) {
			udelay(100);
			cnt++;
		} else
			break;
	}
	if (cnt == 100) {
		pr_err("scale can't stop, 0x%x 0x%x!\n",
			reg_val[0], reg_val[1]);
		return -1;
	}

	dcam_glb_reg_mwr(SCALE_BASE, SCALE_PATH_EB_BIT, 0, DCAM_CFG_REG);
	dcam_glb_reg_mwr(SCALE_BASE, SCALE_PATH_MASK,
		0, DCAM_CFG_REG);
	dcam_irq_clear(SCALER_IRQ_LINE_MASK);
	dcam_reset(DCAM_RST_PATH2);
	REG_MWR(SCALE_BASE, BIT_15, 0);

	return rtn;
}

int scale_k_isr(struct camera_frame *dcam_frm, void *u_data)
{
	unsigned long flag;
	scale_isr_func user_isr_func;
	struct scale_drv_private *private = (struct scale_drv_private *)u_data;
	struct scale_path_info *path_info_ptr;
	struct scale_frame_info_t *frm_info_ptr;

	if (!private) {
		pr_err("scale_k_isr error: private null\n");
		goto isr_exit;
	}

	user_isr_func = private->user_isr_func;
	if (!user_isr_func) {
		pr_err("scale_k_isr error: user_isr_func null\n");
		goto isr_exit;
	}
	path_info_ptr = &private->path_info;
	frm_info_ptr = &private->frm_info;

	spin_lock_irqsave(&private->scale_drv_lock, flag);

	memset(frm_info_ptr, 0, sizeof(struct scale_frame_info_t));
	frm_info_ptr->yaddr = path_info_ptr->output_addr.yaddr;
	frm_info_ptr->uaddr = path_info_ptr->output_addr.uaddr;
	frm_info_ptr->vaddr = path_info_ptr->output_addr.vaddr;
	frm_info_ptr->width = path_info_ptr->output_size.w;
	if (path_info_ptr->scale_mode != SCALE_MODE_NORMAL) {
		frm_info_ptr->height_uv = REG_RD(SCALE_REV_SLICE_O_VCNT);
		frm_info_ptr->height_uv = (frm_info_ptr->height_uv >> 16) &
			SCALE_OUTPUT_SLICE_HEIGHT_MASK;
		frm_info_ptr->height = REG_RD(SCALE_REV_SLICE_O_VCNT);
		frm_info_ptr->height =
			frm_info_ptr->height & SCALE_OUTPUT_SLICE_HEIGHT_MASK;
		path_info_ptr->slice_out_height += frm_info_ptr->height;
	} else {
		frm_info_ptr->height = path_info_ptr->output_size.h;
	}

	user_isr_func(private->scale_fd);
	spin_unlock_irqrestore(&private->scale_drv_lock, flag);

isr_exit:
	return 0;
}

int scale_k_isr_reg(scale_isr_func user_func,
	struct scale_drv_private *drv_private)
{
	int rtn = 0;
	unsigned long flag;

	if (user_func) {
		if (!drv_private) {
			rtn = -1;
			pr_err("scale_reg_isr Failed\n");
			goto reg_exit;
		}

		spin_lock_irqsave(&drv_private->scale_drv_lock, flag);
		drv_private->user_isr_func = user_func;
		spin_unlock_irqrestore(&drv_private->scale_drv_lock, flag);
		dcam_reg_isr(DCAM_PATH2_DONE, scale_k_isr, (void *)drv_private);
	} else {
		dcam_reg_isr(DCAM_PATH2_DONE, NULL, NULL);
	}

reg_exit:
	return rtn;
}

int sprd_scale_drv_init(struct platform_device *pdev)
{

	s_scale_pdev = pdev;

	return 0;
}

