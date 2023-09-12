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

#include "isp_reg.h"
#include "isp_pyr_rec.h"
#include "isp_slice.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "REC: %d %d %s : "fmt, current->pid, __LINE__, __func__

static void isppyrrec_cfg_fetch(uint32_t idx,
		struct isp_rec_fetch_info *rec_fetch, uint32_t fetch_block)
{
	unsigned int val = 0;
	unsigned int base = 0;
	uint32_t color_format = 0;

	if (!rec_fetch) {
		pr_err("fail to get invalid ptr\n");
		return;
	}

	switch (fetch_block) {
	case ISP_PYR_REC_CUR:
		base = PYR_REC_CUR_FETCH_BASE;
		break;
	case ISP_PYR_REC_REF:
		base = ISP_FETCH_BASE;
		break;
	default:
		pr_err("fail to support rec fetch %d.\n", fetch_block);
		return;
	}

	ISP_REG_MWR(idx, base + ISP_FETCH_PARAM0, BIT_0, rec_fetch->bypass);
	if (rec_fetch->bypass)
		return;

	switch (rec_fetch->color_format) {
	case ISP_FETCH_YVU420_2FRAME_10:
	case ISP_FETCH_YVU420_2FRAME_MIPI:
		color_format = 1;
		break;
	case ISP_FETCH_YVU420_2FRAME:
		color_format = 3;
		break;
	case ISP_FETCH_YUV420_2FRAME_10:
	case ISP_FETCH_YUV420_2FRAME_MIPI:
		color_format = 0;
		break;
	case ISP_FETCH_YUV420_2FRAME:
		color_format = 2;
		break;
	case ISP_FETCH_FULL_RGB10:
		color_format = 4;
		break;
	default:
		pr_err("fail to get isp fetch format:%d\n", rec_fetch->color_format);
		break;
	}

	val = ((rec_fetch->chk_sum_clr_en & 0x1) << 11) |
		((rec_fetch->ft0_axi_reorder_en & 0x1) << 9) |
		((rec_fetch->ft0_axi_reorder_en & 0x1) << 8)|
		((color_format & 0x7) << 4) |
		((rec_fetch->substract & 0x1) << 1);
	ISP_REG_MWR(idx,base + ISP_FETCH_PARAM0, 0xB72 ,val);
	ISP_REG_WR(idx, base + ISP_FETCH_SLICE_Y_PITCH, rec_fetch->pitch[0]);
	ISP_REG_WR(idx, base + ISP_FETCH_SLICE_U_PITCH, rec_fetch->pitch[1]);
	val = ((rec_fetch->ft1_max_len_sel & 0x1) << 15) |
		((rec_fetch->ft1_retain_num & 0x7F) << 8)|
		((rec_fetch->ft0_max_len_sel & 0x1) << 7) |
		(rec_fetch->ft0_retain_num & 0x7F);
	ISP_REG_MWR(idx, base + ISP_FETCH_PARAM1, 0xFFFF ,val);

	val = ((rec_fetch->height & 0xFFFF) << 16) | (rec_fetch->width & 0xFFFF);
	ISP_REG_WR(idx, base + ISP_FETCH_MEM_SLICE_SIZE, val);
	ISP_REG_WR(idx, base + ISP_FETCH_SLICE_Y_ADDR, rec_fetch->addr[0]);
	ISP_REG_WR(idx, base + ISP_FETCH_SLICE_U_ADDR, rec_fetch->addr[1]);

	val = (rec_fetch->mipi_word & 0xFFFF) |
		((rec_fetch->mipi_byte & 0xF) << 16) |
		((rec_fetch->mipi10_en & 0x1) << 20);
	ISP_REG_WR(idx, base + ISP_FETCH_MIPI_PARAM, val);
	ISP_REG_WR(idx, base + ISP_FETCH_MIPI_PARAM_UV, val);
}

static void isppyrrec_cfg_ynr(uint32_t idx, struct isp_rec_ynr_info *rec_ynr)
{
	unsigned int val = 0;
	uint32_t i = 0;
	struct isp_dev_ynr_info_v3 *pyr_ynr = NULL;

	if (!rec_ynr) {
		pr_err("fail to get invalid ptr\n");
		return;
	}

	pyr_ynr = rec_ynr->pyr_ynr;
	if (pyr_ynr->bypass)
		return;

	ISP_REG_MWR(idx, ISP_YUV_REC_YNR_CONTRL0, BIT_0, rec_ynr->rec_ynr_bypass);
	for(i = 0; i < 5; i ++) {
		val = ((rec_ynr->ynr_cfg_layer[i].gf_rnr_offset & 0x3ff) << 8) |
			((rec_ynr->ynr_cfg_layer[i].gf_radius & 0x3) << 1) |
			(rec_ynr->ynr_cfg_layer[i].gf_enable & 0x1);
		ISP_REG_WR(idx, ISP_YUV_REC_YNR_L1_CFG0 + i * 0x18, val);
		val = ((rec_ynr->ynr_cfg_layer[i].gf_epsilon_low & 0x1fff) << 16) |
			((rec_ynr->ynr_cfg_layer[i].lum_thresh1 & 0xff) << 8) |
			(rec_ynr->ynr_cfg_layer[i].lum_thresh0 & 0xff);
		ISP_REG_WR(idx, ISP_YUV_REC_YNR_L1_CFG1 + i * 0x18, val);
		val = ((rec_ynr->ynr_cfg_layer[i].gf_epsilon_high & 0x1fff) << 16) |
			(rec_ynr->ynr_cfg_layer[i].gf_epsilon_mid & 0x1fff);
		ISP_REG_WR(idx, ISP_YUV_REC_YNR_L1_CFG2 + i * 0x18, val);
		val = ((rec_ynr->ynr_cfg_layer[i].gf_addback_en & 0x1) << 24) |
			((rec_ynr->ynr_cfg_layer[i].gf_addback_clip & 0xff) << 16) |
			((rec_ynr->ynr_cfg_layer[i].gf_addback_ratio & 0xff) << 8) |
			(rec_ynr->ynr_cfg_layer[i].gf_rnr_ratio & 0xff);
		ISP_REG_WR(idx, ISP_YUV_REC_YNR_L1_CFG3 + i * 0x18, val);
		val = ((rec_ynr->ynr_cfg_layer[i].max_dist & 0xffff) << 16) |
			(rec_ynr->ynr_cfg_layer[i].ynr_radius & 0xffff << 0);
		ISP_REG_WR(idx, ISP_YUV_REC_YNR_L1_CFG4 + i * 0x18, val);
		val = ((rec_ynr->ynr_cfg_layer[i].imgcenter.h & 0xffff) << 16) |
			(rec_ynr->ynr_cfg_layer[i].imgcenter.w & 0xffff);
		ISP_REG_WR(idx, ISP_YUV_REC_YNR_L1_CFG5 + i * 0x18, val);
	}
}

static void isppyrrec_cfg_cnr(uint32_t idx, struct isp_rec_cnr_info *rec_cnr)
{
	struct isp_dev_cnr_h_info *pyr_cnr = NULL;

	if (!rec_cnr) {
		pr_err("fail to get invalid ptr\n");
		return;
	}

	pyr_cnr = rec_cnr->pyr_cnr;
	ISP_REG_MWR(idx, ISP_YUV_REC_CNR_CONTRL0, BIT_0, pyr_cnr->bypass);
	if (pyr_cnr->bypass)
		return;
}

static void isppyrrec_cfg_reconstruct(uint32_t idx,
		struct isp_pyr_rec_info *pyr_rec)
{
	unsigned int val = 0;

	if (!pyr_rec) {
		pr_err("fail to get invalid ptr\n");
		return;
	}

	ISP_REG_MWR(idx, ISP_REC_PARAM, BIT_0, pyr_rec->reconstruct_bypass);
	if (pyr_rec->reconstruct_bypass)
		return;

	val = ((pyr_rec->drop_en & 0x1)<<1) | ((pyr_rec->layer_num & 0xF) <<2);
	ISP_REG_MWR(idx, ISP_REC_PARAM, 0x3E, val);
	val = (pyr_rec->out_width & 0xFFFF) | ((pyr_rec->out_height & 0xFFFF) << 16);
	ISP_REG_WR(idx, ISP_REC_PARAM1, val);
	val = ((pyr_rec->fifo1_nfull_num & 0xffff) << 16) | (pyr_rec->fifo0_nfull_num & 0xffff);
	ISP_REG_WR(idx, ISP_REC_PARAM2, val);
	val = (pyr_rec->pre_layer_width & 0xFFFF) |
		((pyr_rec->pre_layer_height & 0xFFFF)<<16);
	ISP_REG_WR(idx, ISP_REC_PARAM3, val);
	val = ((pyr_rec->fifo3_nfull_num & 0xffff) << 16) |
		(pyr_rec->fifo2_nfull_num & 0xffff);
	ISP_REG_WR(idx, ISP_REC_PARAM4, val);
	val = ((pyr_rec->fifo5_nfull_num & 0xffff) << 16) |
		(pyr_rec->fifo4_nfull_num & 0xffff);
	ISP_REG_WR(idx, ISP_REC_PARAM5, val);
	val = (pyr_rec->hblank_num & 0xffff << 0);
	ISP_REG_WR(idx, ISP_REC_PARAM6, val);
	val = (pyr_rec->hor_padding_en & 0x1) |
		((pyr_rec->hor_padding_num & 0x7FFF)<<1) |
		((pyr_rec->ver_padding_en & 0x1)<<16) |
		((pyr_rec->ver_padding_num & 0x7FFF)<<17);
	ISP_REG_WR(idx, ISP_REC_PARAM7, val);
	val = (pyr_rec->cur_layer_width & 0xFFFF) |
		((pyr_rec->cur_layer_height & 0xFFFF)<<16);
	ISP_REG_WR(idx, ISP_REC_PARAM8, val);
	val = (pyr_rec->reduce_flt_hblank & 0xFFFF) |
		((pyr_rec->reduce_flt_vblank & 0xFFFF)<<16);
	ISP_REG_WR(idx, ISP_REC_PARAM9, val);

	ISP_REG_MWR(idx, ISP_COMMON_SCL_PATH_SEL, BIT_13, BIT_13);
}

static void isppyrrec_cfg_store(uint32_t idx,
		struct isp_rec_store_info *rec_store)
{
	unsigned int val = 0;
	uint32_t color_format = 0, data_10b = 0;
	unsigned int base = PYR_REC_STORE_BASE;

	if (!rec_store) {
		pr_err("fail to get invalid ptr\n");
		return;
	}

	ISP_REG_MWR(idx, base + ISP_STORE_PARAM, BIT_0, rec_store->bypass);
	if (rec_store->bypass)
		return;

	switch (rec_store->color_format) {
	case ISP_FETCH_YUV420_2FRAME_MIPI:
		color_format = 0xC;
		data_10b = 1;
		break;
	case ISP_FETCH_YVU420_2FRAME_MIPI:
		color_format = 0xD;
		data_10b = 1;
		break;
	case ISP_FETCH_YUV420_2FRAME_10:
		color_format = 0x4;
		data_10b = 1;
		break;
	case ISP_FETCH_YVU420_2FRAME_10:
		color_format = 0x5;
		data_10b = 1;
		break;
	default:
		data_10b = 0;
		pr_err("fail to support color foramt %d.\n", rec_store->color_format);
	}

	val = ((rec_store->burst_len & 1) << 1) |
		((rec_store->speed2x & 1) << 2) |
		((rec_store->mirror_en & 1) << 3) |
		((color_format & 0xf) << 4) |
		((rec_store->mipi_en & 1) << 7) |
		((rec_store->endian & 3) << 8) |
		((rec_store->mono_en & 1) << 10) |
		((data_10b & 1) << 11) |
		((rec_store->flip_en & 1) << 12) |
		((rec_store->last_frm_en & 1) << 13);
	ISP_REG_MWR(idx, base + ISP_STORE_PARAM, 0x7FFE, val);
	ISP_REG_WR(idx, base + ISP_STORE_Y_PITCH, rec_store->pitch[0]);
	ISP_REG_WR(idx, base + ISP_STORE_U_PITCH, rec_store->pitch[1]);
	ISP_REG_WR(idx, base + ISP_STORE_SLICE_Y_ADDR, rec_store->addr[0]);
	ISP_REG_WR(idx, base + ISP_STORE_SLICE_U_ADDR, rec_store->addr[1]);
	ISP_REG_MWR(idx, base + ISP_STORE_SHADOW_CLR_SEL, BIT_1,
		((rec_store->shadow_clr_sel & 1) << 1));
	val = ((rec_store->height & 0xFFFF) << 16) | (rec_store->width & 0xFFFF);
	ISP_REG_WR(idx, base + ISP_STORE_SLICE_SIZE, val);
	val = (rec_store->border_left & 0xFFFF) |
		((rec_store->border_right & 0xFFFF) << 16);
	ISP_REG_WR(idx, base + ISP_STORE_BORDER, val);
	val = (rec_store->border_up & 0xFFFF) |
		((rec_store->border_down & 0xFFFF) << 16);
	ISP_REG_WR(idx, base + ISP_STORE_BORDER_1, val);
	ISP_REG_WR(idx, base + ISP_STORE_SHADOW_CLR, rec_store->shadow_clr);
	ISP_REG_MWR(idx, base + ISP_STORE_READ_CTRL, 0x3, rec_store->rd_ctrl & 0x3);

	ISP_REG_MWR(idx, ISP_REC_UVDELAY_PARAM, BIT_0, rec_store->uvdelay_bypass);
	val = (rec_store->uvdelay_slice_width & 0x1fff) << 16;
	ISP_REG_MWR(idx, ISP_REC_UVDELAY_STEP, 0x1fff0000, val);
}

static int isppyrrec_fetch_slice_set(struct slice_fetch_info *rec_fetch,
	void *in_ptr, uint32_t fetch_block)
{
	uint32_t addr = 0, cmd = 0;
	unsigned int base = 0;
	struct isp_fmcu_ctx_desc *fmcu = NULL;

	fmcu = (struct isp_fmcu_ctx_desc *)in_ptr;

	switch (fetch_block) {
	case ISP_PYR_REC_CUR:
		base = PYR_REC_CUR_FETCH_BASE;
		break;
	case ISP_PYR_REC_REF:
		base = ISP_FETCH_BASE;
		break;
	default:
		pr_err("fail to support rec fetch %d.\n", fetch_block);
		return -EFAULT;
	}

	addr = ISP_GET_REG(ISP_FETCH_MEM_SLICE_SIZE) + base;
	cmd = ((rec_fetch->size.h & 0xFFFF) << 16) | (rec_fetch->size.w & 0xFFFF);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_FETCH_SLICE_Y_ADDR) + base;
	cmd = rec_fetch->addr.addr_ch0;
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_FETCH_SLICE_U_ADDR) + base;
	cmd = rec_fetch->addr.addr_ch1;
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_FETCH_MIPI_PARAM) + base;
	cmd = (rec_fetch->mipi_word_num & 0xFFFF) |
		((rec_fetch->mipi_byte_rel_pos & 0xF) << 16) |
		((rec_fetch->mipi10_en & 0x1) << 20);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_FETCH_MIPI_PARAM_UV) + base;
	FMCU_PUSH(fmcu, addr, cmd);

	/* dispatch size same as ref fetch size */
	if (base == ISP_FETCH_BASE) {
		addr = ISP_GET_REG(ISP_DISPATCH_BASE + ISP_DISPATCH_CH0_SIZE);
		cmd = ((rec_fetch->size.h & 0xFFFF) << 16)
			| (rec_fetch->size.w & 0xFFFF);
		FMCU_PUSH(fmcu, addr, cmd);
	}

	return 0;
}

static int isppyrrec_fbd_fetch_slice_set(struct slice_fetch_info *rec_fetch, void *in_ptr)
{
	uint32_t addr = 0, cmd = 0;
	unsigned int base = 0;
	struct isp_fmcu_ctx_desc *fmcu = NULL;

	if (rec_fetch == NULL || in_ptr == NULL) {
		pr_info("fail to get param pointer.\n");
		return 0;
	}
	fmcu = (struct isp_fmcu_ctx_desc *)in_ptr;
	base = ISP_YUV_AFBD_FETCH_BASE;

	addr = ISP_GET_REG(ISP_AFBD_FETCH_SEL) + base;
	cmd = (rec_fetch->fetch_fbd.fetch_fbd_bypass & 0x1) |
		(0x1 << 1) | (0x1 << 3) | ((rec_fetch->fetch_fbd.afbc_mode & 0x1F) << 4);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_AFBD_FETCH_HBLANK_TILE_PITCH ) + base;
	cmd = ((rec_fetch->fetch_fbd.tile_num_pitch & 0x7FF) << 16) | (0x8000);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_AFBD_FETCH_SLICE_SIZE) + base;
	cmd = ((rec_fetch->fetch_fbd.slice_size.h & 0xFFFF) << 16) |
		(rec_fetch->fetch_fbd.slice_size.w & 0xFFFF);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_AFBD_FETCH_PARAM0) + base;
	cmd = rec_fetch->fetch_fbd.slice_start_pxl_xpt |
		rec_fetch->fetch_fbd.slice_start_pxl_ypt << 16;
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_AFBD_FETCH_PARAM1) + base;
	cmd = rec_fetch->fetch_fbd.frame_header_base_addr;
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_AFBD_FETCH_PARAM2) + base;
	cmd = rec_fetch->fetch_fbd.slice_start_header_addr;
	FMCU_PUSH(fmcu, addr, cmd);

	return 0;
}

static int isppyrrec_ynr_slice_set(struct slice_pos_info *pyr_ynr, void *in_ptr)
{
	uint32_t addr = 0, cmd = 0;
	struct isp_fmcu_ctx_desc *fmcu = NULL;

	fmcu = (struct isp_fmcu_ctx_desc *)in_ptr;

	addr = ISP_GET_REG(ISP_YUV_REC_YNR_CFG1);
	cmd = ((pyr_ynr->start_row & 0xffff) << 16) | (pyr_ynr->start_col & 0xffff);
	FMCU_PUSH(fmcu, addr, cmd);

	return 0;
}

static int isppyrrec_cnr_slice_set(struct slice_pos_info *pyr_cnr, void *in_ptr)
{
	uint32_t addr = 0, cmd = 0;
	struct isp_fmcu_ctx_desc *fmcu = NULL;

	fmcu = (struct isp_fmcu_ctx_desc *)in_ptr;

	addr = ISP_GET_REG(ISP_YUV_REC_CNR_CONTRL1);
	cmd = (((pyr_cnr->start_row / 2) & 0xFFFF) << 16) | ((pyr_cnr->start_col / 2) & 0xFFFF);
	FMCU_PUSH(fmcu, addr, cmd);

	return 0;
}

static int isppyrrec_reconstruct_slice_set(struct slice_pyr_rec_info *pyr_rec, void *in_ptr)
{
	uint32_t addr = 0, cmd = 0;
	struct isp_fmcu_ctx_desc *fmcu = NULL;

	fmcu = (struct isp_fmcu_ctx_desc *)in_ptr;

	addr = ISP_GET_REG(ISP_REC_PARAM1);
	cmd = (pyr_rec->out.w & 0xFFFF) | ((pyr_rec->out.h & 0xFFFF) << 16);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_REC_PARAM3);
	cmd = (pyr_rec->pre_layer.w & 0xFFFF) | ((pyr_rec->pre_layer.h & 0xFFFF) << 16);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_REC_PARAM7);
	cmd = (pyr_rec->hor_padding_en & 0x1) |
		((pyr_rec->hor_padding_num & 0x7FFF) << 1) |
		((pyr_rec->ver_padding_en & 0x1)<<16) |
		((pyr_rec->ver_padding_num & 0x7FFF) << 17);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_REC_PARAM8);
	cmd = (pyr_rec->cur_layer.w & 0xFFFF) | ((pyr_rec->cur_layer.h & 0xFFFF) << 16);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_REC_PARAM9);
	cmd = (pyr_rec->reduce_flt_hblank & 0xFFFF) |
		((pyr_rec->reduce_flt_vblank & 0xFFFF)<<16);
	FMCU_PUSH(fmcu, addr, cmd);

	/* uvdelay size must <= 2592 */
	addr = ISP_GET_REG(ISP_REC_UVDELAY_STEP);
	cmd = (pyr_rec->out.w & 0xFFFF) << 16;
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_DISPATCH_BASE + ISP_DISPATCH_DLY);
	cmd = ((pyr_rec->dispatch_dly_height_num & 0xFFFF) << 16)
		| (pyr_rec->dispatch_dly_width_num & 0xFFFF);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_DISPATCH_BASE + ISP_DISPATCH_LINE_DLY1);
	cmd = (pyr_rec->width_flash_mode << 31)
		| (pyr_rec->dispatch_mode << 30)
		| ((pyr_rec->yuv_start_row_num & 0xFF) << 20)
		| (pyr_rec->width_dly_num_flash & 0xFFFF);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_DISPATCH_BASE + ISP_DISPATCH_PIPE_BUF_CTRL_CH0);
	cmd = ((pyr_rec->dispatch_pipe_full_num & 0x7FF) << 16) | (0x43c & 0xFFFF);
	FMCU_PUSH(fmcu, addr, cmd);

	return 0;
}

static int isppyrrec_store_slice_set(struct slice_store_info *rec_store, void *in_ptr)
{
	uint32_t addr = 0, cmd = 0;
	unsigned int base = PYR_REC_STORE_BASE;
	struct isp_fmcu_ctx_desc *fmcu = NULL;

	fmcu = (struct isp_fmcu_ctx_desc *)in_ptr;

	addr = ISP_GET_REG(ISP_STORE_SLICE_SIZE) + base;
	cmd = ((rec_store->size.h & 0xFFFF) << 16) | (rec_store->size.w & 0xFFFF);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_STORE_BORDER) + base;
	cmd =  (rec_store->border.left_border & 0xFFFF) |
		((rec_store->border.right_border & 0xFFFF) << 16);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_STORE_BORDER_1) + base;
	cmd = (rec_store->border.up_border & 0xFFFF) |
		((rec_store->border.down_border & 0xFFFF) << 16);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_STORE_SLICE_Y_ADDR) + base;
	cmd = rec_store->addr.addr_ch0;
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_STORE_SLICE_U_ADDR) + base;
	cmd = rec_store->addr.addr_ch1;
	FMCU_PUSH(fmcu, addr, cmd);

	return 0;
}

int isp_pyr_rec_bypass(void *handle)
{
	int idx, bypass = 0;
	uint32_t cmd = 0;
	unsigned int addr = 0;
	struct isp_fmcu_ctx_desc *fmcu = NULL;
	struct isp_rec_ctx_desc *ctx = NULL;

	if (!handle) {
		pr_err("fail to rec_config_reg parm NULL\n");
		return -EFAULT;
	}

	ctx = (struct isp_rec_ctx_desc *)handle;
	fmcu = (struct isp_fmcu_ctx_desc *)ctx->fmcu_handle;
	idx = ctx->ctx_id;
	bypass = ctx->pyr_rec.reconstruct_bypass;

	ISP_REG_MWR(idx, PYR_REC_STORE_BASE + ISP_STORE_SHADOW_CLR_SEL, BIT_1, 0);

	addr = ISP_GET_REG(ISP_COMMON_SCL_PATH_SEL);
	cmd = ISP_REG_RD(idx, ISP_COMMON_SCL_PATH_SEL);
	cmd &= ~(((bypass & 0x1) << 13) & 0x2000);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = PYR_REC_STORE_BASE + ISP_GET_REG(ISP_STORE_SHADOW_CLR);
	cmd = (~bypass) & 0x1;
	FMCU_PUSH(fmcu, addr, cmd);

	cmd = bypass & 0x1;
	addr = PYR_REC_CUR_FETCH_BASE + ISP_GET_REG(ISP_FETCH_PARAM0);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_YNR_DCT_PARAM);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_REC_PARAM);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_YUV_REC_CNR_CONTRL0);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_YUV_REC_YNR_CONTRL0);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_REC_UVDELAY_PARAM);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = PYR_REC_STORE_BASE + ISP_GET_REG(ISP_STORE_PARAM);
	FMCU_PUSH(fmcu, addr, cmd);

	return 0;
}

int isp_pyr_rec_share_config(void *handle)
{
	int ret = 0;
	uint32_t idx = 0;
	struct isp_rec_ctx_desc *ctx = NULL;
	struct isp_hw_fmcu_cfg fmcu_cfg;

	if (!handle) {
		pr_err("fail to rec_config_reg parm NULL\n");
		return -EFAULT;
	}

	ctx = (struct isp_rec_ctx_desc *)handle;
	idx = ctx->ctx_id;

	isppyrrec_cfg_fetch(idx, &ctx->cur_fetch, ISP_PYR_REC_CUR);
	isppyrrec_cfg_fetch(idx, &ctx->ref_fetch, ISP_PYR_REC_REF);
	isppyrrec_cfg_ynr(idx, &ctx->rec_ynr);
	isppyrrec_cfg_cnr(idx, &ctx->rec_cnr);
	isppyrrec_cfg_reconstruct(idx, &ctx->pyr_rec);
	isppyrrec_cfg_store(idx, &ctx->rec_store);

	if (ctx->wmode == ISP_CFG_MODE) {
		fmcu_cfg.fmcu = ctx->fmcu_handle;
		fmcu_cfg.ctx_id = ctx->hw_ctx_id;
		ctx->hw->isp_ioctl(ctx->hw, ISP_HW_CFG_FMCU_CFG, &fmcu_cfg);
	}

	return ret;
}

int isp_pyr_rec_frame_config(void *handle)
{
	int ret = 0, i = 0;
	struct isp_rec_ctx_desc *ctx = NULL;
	uint32_t addr = 0, cmd = 0;
	uint32_t idx = 0;
	struct isp_fmcu_ctx_desc *fmcu = NULL;
	struct isp_dev_cnr_h_info *pyr_cnr = NULL;
	struct isp_cnr_h_info *layer_cnr_h = NULL;
	struct isp_rec_ynr_info *ynr_info = NULL;
	struct isp_rec_fetch_info *rec_fetch = NULL;
	uint32_t color_format = 0;

	if (!handle) {
		pr_err("fail to rec_config_reg parm NULL\n");
		return -EFAULT;
	}

	ctx = (struct isp_rec_ctx_desc *)handle;
	fmcu = (struct isp_fmcu_ctx_desc *)ctx->fmcu_handle;
	pyr_cnr = ctx->rec_cnr.pyr_cnr;
	layer_cnr_h = &pyr_cnr->layer_cnr_h[ctx->cur_layer];
	ynr_info = &ctx->rec_ynr;
	idx = ctx->ctx_id;

	if (ctx->cur_layer == 0)
		rec_fetch = &ctx->cur_fetch;
	else
		rec_fetch = &ctx->ref_fetch;

	switch (rec_fetch->color_format) {
	case ISP_FETCH_YVU420_2FRAME_10:
	case ISP_FETCH_YVU420_2FRAME_MIPI:
		color_format = 1;
		break;
	case ISP_FETCH_YVU420_2FRAME:
		color_format = 3;
		break;
	case ISP_FETCH_YUV420_2FRAME_10:
	case ISP_FETCH_YUV420_2FRAME_MIPI:
		color_format = 0;
		break;
	case ISP_FETCH_YUV420_2FRAME:
		color_format = 2;
		break;
	case ISP_FETCH_FULL_RGB10:
		color_format = 4;
		break;
	default:
		pr_err("fail to get isp fetch format:%d\n", rec_fetch->color_format);
		break;
	}
	addr = ISP_GET_REG(ISP_FETCH_PARAM0) + PYR_REC_CUR_FETCH_BASE;
	cmd = ((rec_fetch->chk_sum_clr_en & 0x1) << 11) |
		((rec_fetch->ft0_axi_reorder_en & 0x1) << 9) |
		((rec_fetch->ft0_axi_reorder_en & 0x1) << 8)|
		((color_format & 0x7) << 4) |
		((rec_fetch->substract & 0x1) << 1);
	FMCU_PUSH(fmcu, addr, cmd);
	ISP_REG_WR(idx, PYR_REC_CUR_FETCH_BASE + ISP_FETCH_PARAM0, cmd);

	addr = ISP_GET_REG(ISP_FETCH_SLICE_Y_PITCH) + PYR_REC_CUR_FETCH_BASE;
	cmd = ctx->cur_fetch.pitch[0];
	FMCU_PUSH(fmcu, addr, cmd);
	ISP_REG_WR(idx, PYR_REC_CUR_FETCH_BASE + ISP_FETCH_SLICE_Y_PITCH, cmd);
	addr = ISP_GET_REG(ISP_FETCH_SLICE_U_PITCH) + PYR_REC_CUR_FETCH_BASE;
	cmd = ctx->cur_fetch.pitch[1];
	FMCU_PUSH(fmcu, addr, cmd);
	ISP_REG_WR(idx, PYR_REC_CUR_FETCH_BASE + ISP_FETCH_SLICE_U_PITCH, cmd);

	addr = ISP_GET_REG(ISP_FETCH_SLICE_Y_PITCH) + ISP_FETCH_BASE;
	cmd = ctx->ref_fetch.pitch[0];
	FMCU_PUSH(fmcu, addr, cmd);
	ISP_REG_WR(idx, ISP_FETCH_BASE + ISP_FETCH_SLICE_Y_PITCH, cmd);
	addr = ISP_GET_REG(ISP_FETCH_SLICE_U_PITCH) + ISP_FETCH_BASE;
	cmd = ctx->ref_fetch.pitch[1];
	FMCU_PUSH(fmcu, addr, cmd);
	ISP_REG_WR(idx, ISP_FETCH_BASE + ISP_FETCH_SLICE_U_PITCH, cmd);

	addr = ISP_GET_REG(ISP_REC_PARAM);
	cmd = ((ctx->pyr_rec.reconstruct_bypass & 0x1) << 0) |
		((ctx->pyr_rec.drop_en & 0x1) << 1) |
		((ctx->pyr_rec.layer_num & 0xF) << 2);
	FMCU_PUSH(fmcu, addr, cmd);
	ISP_REG_WR(idx, ISP_REC_PARAM, cmd);

	addr = ISP_GET_REG(ISP_STORE_Y_PITCH) + PYR_REC_STORE_BASE;
	cmd = ctx->rec_store.pitch[0];
	FMCU_PUSH(fmcu, addr, cmd);
	addr = ISP_GET_REG(ISP_STORE_U_PITCH) + PYR_REC_STORE_BASE;
	cmd = ctx->rec_store.pitch[1];
	FMCU_PUSH(fmcu, addr, cmd);

	/* ynr frame param cfg */
	addr = ISP_GET_REG(ISP_YUV_REC_YNR_CONTRL0);
	cmd = ((ynr_info->layer_num & 0x7) << 1) | ynr_info->rec_ynr_bypass;
	FMCU_PUSH(fmcu, addr, cmd);
	ISP_REG_WR(idx, ISP_YUV_REC_YNR_CONTRL0, cmd);

	addr = ISP_GET_REG(ISP_YUV_REC_YNR_CFG0);
	cmd = ((ynr_info->img.h & 0xffff) << 16) | (ynr_info->img.w & 0xffff);
	FMCU_PUSH(fmcu, addr, cmd);
	ISP_REG_WR(idx, ISP_YUV_REC_YNR_CFG0, cmd);

	addr = ISP_GET_REG(ISP_YUV_REC_YNR_CFG1);
	cmd = ((ynr_info->start.h & 0xffff) << 16) | (ynr_info->start.w & 0xffff);
	FMCU_PUSH(fmcu, addr, cmd);
	ISP_REG_WR(idx, ISP_YUV_REC_YNR_CFG1, cmd);

	/* cnr frame param cfg */
	addr = ISP_GET_REG(ISP_YUV_REC_CNR_CONTRL0);
	cmd = ((ctx->rec_cnr.layer_num & 0x7) << 1) | ctx->rec_cnr.rec_cnr_bypass;
	FMCU_PUSH(fmcu, addr, cmd);
	ISP_REG_WR(idx, ISP_YUV_REC_CNR_CONTRL0, cmd);

	addr = ISP_GET_REG(ISP_YUV_REC_CNR_CFG0);
	cmd = ((layer_cnr_h->radius & 0xFFFF) << 16) |
		((layer_cnr_h->minRatio & 0x3FF) << 2) |
		((layer_cnr_h->denoise_radial_en & 0x1) << 1) |
		(layer_cnr_h->lowpass_filter_en & 0x1);
	FMCU_PUSH(fmcu, addr, cmd);
	ISP_REG_WR(idx, ISP_YUV_REC_CNR_CFG0, cmd);

	addr = ISP_GET_REG(ISP_YUV_REC_CNR_CFG1);
	cmd = ((ctx->rec_cnr.img_center.h & 0xFFFF) << 16) |
		(ctx->rec_cnr.img_center.w & 0xFFFF);
	FMCU_PUSH(fmcu, addr, cmd);
	ISP_REG_WR(idx, ISP_YUV_REC_CNR_CFG1, cmd);

	addr = ISP_GET_REG(ISP_YUV_REC_CNR_CFG2);
	cmd = ((layer_cnr_h->filter_size & 0x3) << 28) |
		((layer_cnr_h->slope & 0xFFF) << 16) |
		((layer_cnr_h->luma_th[1] & 0xFF) << 8) |
		(layer_cnr_h->luma_th[0] & 0xFF);
	FMCU_PUSH(fmcu, addr, cmd);
	ISP_REG_WR(idx, ISP_YUV_REC_CNR_CFG2, cmd);

	for (i = 0; i < 18; i++) {
		addr = ISP_GET_REG(ISP_YUV_REC_CNR_Y_L0_WHT0 + 4 * i);
		cmd = ((layer_cnr_h->weight_y[0][4 * i + 3] & 0xFF) << 24) |
			((layer_cnr_h->weight_y[0][4 * i + 2] & 0xFF) << 16) |
			((layer_cnr_h->weight_y[0][4 * i + 1] & 0xFF) << 8) |
			(layer_cnr_h->weight_y[0][4 * i] & 0xFF);
		FMCU_PUSH(fmcu, addr, cmd);
		ISP_REG_WR(idx, ISP_YUV_REC_CNR_Y_L0_WHT0 + 4 * i, cmd);

		addr = ISP_GET_REG(ISP_YUV_REC_CNR_Y_L1_WHT0 + 4 * i);
		cmd = ((layer_cnr_h->weight_y[1][4 * i + 3] & 0xFF) << 24) |
			((layer_cnr_h->weight_y[1][4 * i + 2] & 0xFF) << 16) |
			((layer_cnr_h->weight_y[1][4 * i + 1] & 0xFF) << 8) |
			(layer_cnr_h->weight_y[1][4 * i] & 0xFF);
		FMCU_PUSH(fmcu, addr, cmd);
		ISP_REG_WR(idx, ISP_YUV_REC_CNR_Y_L1_WHT0 + 4 * i, cmd);

		addr = ISP_GET_REG(ISP_YUV_REC_CNR_Y_L2_WHT0 + 4 * i);
		cmd = ((layer_cnr_h->weight_y[2][4 * i + 3] & 0xFF) << 24) |
			((layer_cnr_h->weight_y[2][4 * i + 2] & 0xFF) << 16) |
			((layer_cnr_h->weight_y[2][4 * i + 1] & 0xFF) << 8) |
			(layer_cnr_h->weight_y[2][4 * i] & 0xFF);
		FMCU_PUSH(fmcu, addr, cmd);
		ISP_REG_WR(idx, ISP_YUV_REC_CNR_Y_L2_WHT0 + 4 * i, cmd);

		addr = ISP_GET_REG(ISP_YUV_REC_CNR_UV_L0_WHT0 + 4 * i);
		cmd = ((layer_cnr_h->weight_uv[0][4 * i + 3] & 0xFF) << 24) |
			((layer_cnr_h->weight_uv[0][4 * i + 2] & 0xFF) << 16) |
			((layer_cnr_h->weight_uv[0][4 * i + 1] & 0xFF) << 8) |
			(layer_cnr_h->weight_uv[0][4 * i] & 0xFF);
		FMCU_PUSH(fmcu, addr, cmd);
		ISP_REG_WR(idx, ISP_YUV_REC_CNR_UV_L0_WHT0 + 4 * i, cmd);

		addr = ISP_GET_REG(ISP_YUV_REC_CNR_UV_L1_WHT0 + 4 * i);
		cmd = ((layer_cnr_h->weight_uv[1][4 * i + 3] & 0xFF) << 24) |
			((layer_cnr_h->weight_uv[1][4 * i + 2] & 0xFF) << 16) |
			((layer_cnr_h->weight_uv[1][4 * i + 1] & 0xFF) << 8) |
			(layer_cnr_h->weight_uv[1][4 * i] & 0xFF);
		FMCU_PUSH(fmcu, addr, cmd);
		ISP_REG_WR(idx, ISP_YUV_REC_CNR_UV_L1_WHT0 + 4 * i, cmd);

		addr = ISP_GET_REG(ISP_YUV_REC_CNR_UV_L2_WHT0 + 4 * i);
		cmd = ((layer_cnr_h->weight_uv[2][4 * i + 3] & 0xFF) << 24) |
			((layer_cnr_h->weight_uv[2][4 * i + 2] & 0xFF) << 16) |
			((layer_cnr_h->weight_uv[2][4 * i + 1] & 0xFF) << 8) |
			(layer_cnr_h->weight_uv[2][4 * i] & 0xFF);
		FMCU_PUSH(fmcu, addr, cmd);
		ISP_REG_WR(idx, ISP_YUV_REC_CNR_UV_L2_WHT0 + 4 * i, cmd);
	}

	return ret;
}

int isp_pyr_rec_slice_common_config(void *handle)
{
	int ret = 0;
	uint32_t addr = 0, cmd = 0;
	struct isp_fmcu_ctx_desc *fmcu = NULL;
	struct isp_rec_ctx_desc *ctx = NULL;
	struct isp_rec_slice_desc *cur_rec_slc = NULL;

	if (!handle) {
		pr_err("fail to rec_config_reg parm NULL\n");
		return -EFAULT;
	}

	ctx = (struct isp_rec_ctx_desc *)handle;
	cur_rec_slc = &ctx->slices[ctx->cur_slice_id];
	fmcu = (struct isp_fmcu_ctx_desc *)ctx->fmcu_handle;
	cur_rec_slc->slice_cur_fetch.cur_layer = ctx->cur_layer;
	cur_rec_slc->slice_cur_fetch.fetch_path_sel = ctx->fetch_path_sel;
	memcpy(&cur_rec_slc->slice_cur_fetch.fetch_fbd, &ctx->fetch_fbd, sizeof(struct isp_fbd_yuv_info));
	if ((cur_rec_slc->slice_cur_fetch.cur_layer == 0) && (cur_rec_slc->slice_cur_fetch.fetch_path_sel == 1))
		isppyrrec_fbd_fetch_slice_set(&cur_rec_slc->slice_cur_fetch, ctx->fmcu_handle);
	else
		isppyrrec_fetch_slice_set(&cur_rec_slc->slice_cur_fetch, ctx->fmcu_handle, ISP_PYR_REC_CUR);
	isppyrrec_fetch_slice_set(&cur_rec_slc->slice_ref_fetch, ctx->fmcu_handle, ISP_PYR_REC_REF);
	isppyrrec_ynr_slice_set(&cur_rec_slc->slice_fetch0_pos, ctx->fmcu_handle);
	isppyrrec_cnr_slice_set(&cur_rec_slc->slice_fetch0_pos, ctx->fmcu_handle);
	isppyrrec_reconstruct_slice_set(&cur_rec_slc->slice_pyr_rec, ctx->fmcu_handle);

	if ((ctx->cur_layer != 0) || (ctx->fetch_path_sel != 1)) {
		addr = ISP_GET_REG(ISP_COMMON_SCL_PATH_SEL);
		cmd = ISP_REG_RD(ctx->ctx_id, ISP_COMMON_SCL_PATH_SEL);
		cmd = cmd & 0xFFFFF3FF;
		cmd = cmd | ((cur_rec_slc->slice_pyr_rec.rec_path_sel & 1) << 7) | BIT_13;
		FMCU_PUSH(fmcu, addr, cmd);
	} else {
		addr = ISP_GET_REG(ISP_COMMON_SCL_PATH_SEL);
		cmd = ISP_REG_RD(ctx->ctx_id, ISP_COMMON_SCL_PATH_SEL);
		cmd = ((cur_rec_slc->slice_cur_fetch.fetch_path_sel << 12) |
			cmd | BIT_7 | BIT_13) & 0xFFFFF3FF;
		FMCU_PUSH(fmcu, addr, cmd);
	}

	addr = ISP_GET_REG(PYR_REC_STORE_BASE + ISP_STORE_PARAM);
	cmd = ISP_REG_RD(ctx->ctx_id, PYR_REC_STORE_BASE + ISP_STORE_PARAM);
	cmd = cmd | (cur_rec_slc->slice_pyr_rec.rec_path_sel & 1);
	FMCU_PUSH(fmcu, addr, cmd);

	return ret;
}

int isp_pyr_rec_slice_config(void *handle)
{
	int ret = 0;
	struct isp_rec_ctx_desc *ctx = NULL;
	struct isp_rec_slice_desc *cur_rec_slc = NULL;
	struct isp_hw_slices_fmcu_cmds parg;

	if (!handle) {
		pr_err("fail to rec_config_reg parm NULL\n");
		return -EFAULT;
	}

	ctx = (struct isp_rec_ctx_desc *)handle;
	cur_rec_slc = &ctx->slices[ctx->cur_slice_id];

	isp_pyr_rec_slice_common_config(handle);
	isppyrrec_store_slice_set(&cur_rec_slc->slice_rec_store, ctx->fmcu_handle);

	parg.wmode = ctx->wmode;
	parg.hw_ctx_id = ctx->hw_ctx_id;
	parg.fmcu = ctx->fmcu_handle;
	ctx->hw->isp_ioctl(ctx->hw, ISP_HW_CFG_SLICE_FMCU_PYR_REC_CMD, &parg);

	return ret;
}
