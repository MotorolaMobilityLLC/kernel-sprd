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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>
#include <linux/kthread.h>
#include <video/sprd_img.h>

#include "isp_buf.h"
#include "isp_int.h"
#include "isp_path.h"
#include "isp_slice.h"
#include "cam_pw_domain.h"
#include "cam_gen_scale_coef.h"
#include "isp_3dnr_cap.h"
#include "isp_3dnr_drv.h"
#include "cam_common.h"


#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_DRV: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#ifndef FPGA_BRINGUP
#define BIT_MM_AHB_ISP_EB                BIT(2)
#define REG_MM_AHB_AHB_EB                (0x0000)
#endif

struct platform_device *s_isp_pdev;
struct isp_group s_isp_group;
static atomic_t s_isp_users[ISP_MAX_COUNT];
static atomic_t s_isp_total_users;
static atomic_t s_isp_total_runer;
static uint32_t s_isp_count;
unsigned long s_isp_regbase[ISP_MAX_COUNT];
unsigned long isp_phys_base[ISP_MAX_COUNT];

static struct clk *isp_clk;
static struct clk *isp_clk_parent;
static struct clk *isp_clk_default;
static struct clk *isp_axi_eb;
static struct clk *isp_eb;

struct isp_ch_irq s_isp_irq[ISP_MAX_COUNT];
static struct mutex isp_module_sema[ISP_MAX_COUNT];
static struct regmap *cam_ahb_gpr;

static spinlock_t isp_glb_reg_axi_lock[ISP_MAX_COUNT];
static spinlock_t isp_glb_reg_mask_lock[ISP_MAX_COUNT];
static spinlock_t isp_glb_reg_clr_lock[ISP_MAX_COUNT];
spinlock_t isp_mod_lock;
#define ISP_AXI_STOP_TIMEOUT           1000
#define ISP_HIST_ENABLE                0

static void sprd_ispdrv_glb_reg_awr(uint32_t idx, unsigned long addr,
			uint32_t val, uint32_t reg_id)
{
	unsigned long flag;
	enum isp_id id = 0;

	id = ISP_GET_ISP_ID(idx);

	switch (reg_id) {
	case ISP_AXI_REG:
		spin_lock_irqsave(&isp_glb_reg_axi_lock[id], flag);
		ISP_HREG_WR(id, addr, ISP_REG_RD(id, addr) & (val));
		spin_unlock_irqrestore(&isp_glb_reg_axi_lock[id], flag);
		break;
	default:
		ISP_HREG_WR(id, addr, ISP_REG_RD(id, addr) & (val));
		break;
	}
}

static void sprd_ispdrv_glb_reg_owr(uint32_t idx, unsigned long addr,
			uint32_t val, uint32_t reg_id)
{
	unsigned long flag = 0;
	enum isp_id id = 0;

	id = ISP_GET_ISP_ID(idx);

	switch (reg_id) {
	case ISP_AXI_REG:
		spin_lock_irqsave(&isp_glb_reg_axi_lock[id], flag);
		ISP_HREG_WR(id, addr, ISP_REG_RD(id, addr) | (val));
		spin_unlock_irqrestore(&isp_glb_reg_axi_lock[id], flag);
		break;
	case ISP_INIT_MASK_REG:
		spin_lock_irqsave(&isp_glb_reg_mask_lock[id], flag);
		ISP_HREG_WR(id, addr, ISP_REG_RD(id, addr) | (val));
		spin_unlock_irqrestore(&isp_glb_reg_mask_lock[id], flag);
		break;
	case ISP_INIT_CLR_REG:
		spin_lock_irqsave(&isp_glb_reg_clr_lock[id], flag);
		ISP_HREG_WR(id, addr, ISP_REG_RD(id, addr) | (val));
		spin_unlock_irqrestore(&isp_glb_reg_clr_lock[id], flag);
		break;
	default:
		ISP_HREG_WR(id, addr, ISP_REG_RD(id, addr) | (val));
		break;
	}
}

static void sprd_ispdrv_glb_reg_wr(uint32_t idx, unsigned long addr,
			uint32_t val, uint32_t reg_id)
{
	unsigned long flag = 0;
	enum isp_id id = 0;

	id = ISP_GET_ISP_ID(idx);

	switch (reg_id) {
	case ISP_INIT_MASK_REG:
		spin_lock_irqsave(&isp_glb_reg_mask_lock[id], flag);
		ISP_HREG_WR(id, addr, val);
		spin_unlock_irqrestore(&isp_glb_reg_mask_lock[id], flag);
		break;
	case ISP_INIT_CLR_REG:
		spin_lock_irqsave(&isp_glb_reg_clr_lock[id], flag);
		ISP_HREG_WR(id, addr, val);
		spin_unlock_irqrestore(&isp_glb_reg_clr_lock[id], flag);
		break;
	default:
		ISP_HREG_WR(id, addr, val);
		break;
	}
}

static void sprd_ispdrv_irq_mask_en(uint32_t idx)
{
	sprd_ispdrv_glb_reg_wr(idx, ISP_P0_INT_BASE + ISP_INT_EN0,
		ISP_INT_LINE_MASK_P0, ISP_INIT_MASK_REG);

	sprd_ispdrv_glb_reg_wr(idx, ISP_P1_INT_BASE + ISP_INT_EN0,
		ISP_INT_LINE_MASK_P1, ISP_INIT_MASK_REG);

	sprd_ispdrv_glb_reg_wr(idx, ISP_C0_INT_BASE + ISP_INT_EN0,
		ISP_INT_LINE_MASK_C0, ISP_INIT_MASK_REG);

	sprd_ispdrv_glb_reg_wr(idx, ISP_C1_INT_BASE + ISP_INT_EN0,
		ISP_INT_LINE_MASK_C1, ISP_INIT_MASK_REG);

	sprd_ispdrv_glb_reg_wr(idx, ISP_MMU_INT_BASE + ISP_MMU_INT_EN,
		ISP_INT_LINE_MASK_MMU, ISP_INIT_MASK_REG);
}

static void sprd_ispdrv_irq_mask_dis(uint32_t idx)
{
	sprd_ispdrv_glb_reg_wr(idx, ISP_P0_INT_BASE + ISP_INT_EN0,
		0x0, ISP_INIT_MASK_REG);
	sprd_ispdrv_glb_reg_wr(idx, ISP_P0_INT_BASE + ISP_INT_EN1,
		0x0, ISP_INIT_MASK_REG);

	sprd_ispdrv_glb_reg_wr(idx, ISP_P1_INT_BASE + ISP_INT_EN0,
		0x0, ISP_INIT_MASK_REG);
	sprd_ispdrv_glb_reg_wr(idx, ISP_P1_INT_BASE + ISP_INT_EN1,
		0x0, ISP_INIT_MASK_REG);

	sprd_ispdrv_glb_reg_wr(idx, ISP_C0_INT_BASE + ISP_INT_EN0,
		0x0, ISP_INIT_MASK_REG);
	sprd_ispdrv_glb_reg_wr(idx, ISP_C0_INT_BASE + ISP_INT_EN1,
		0x0, ISP_INIT_MASK_REG);

	sprd_ispdrv_glb_reg_wr(idx, ISP_C1_INT_BASE + ISP_INT_EN0,
		0x0, ISP_INIT_MASK_REG);
	sprd_ispdrv_glb_reg_wr(idx, ISP_C1_INT_BASE + ISP_INT_EN1,
		0x0, ISP_INIT_MASK_REG);

	sprd_ispdrv_glb_reg_wr(idx, ISP_MMU_INT_BASE + ISP_MMU_INT_EN,
		0x0, ISP_INIT_MASK_REG);
}

static void sprd_ispdrv_irq_clear(uint32_t idx)
{
	sprd_ispdrv_glb_reg_wr(idx, ISP_P0_INT_BASE + ISP_INT_CLR0,
		0xFFFFFFFF, ISP_INIT_CLR_REG);
	sprd_ispdrv_glb_reg_wr(idx, ISP_P0_INT_BASE + ISP_INT_CLR1,
		0xFFFFFFFF, ISP_INIT_CLR_REG);

	sprd_ispdrv_glb_reg_wr(idx, ISP_P1_INT_BASE + ISP_INT_CLR0,
		0xFFFFFFFF, ISP_INIT_CLR_REG);
	sprd_ispdrv_glb_reg_wr(idx, ISP_P1_INT_BASE + ISP_INT_CLR1,
		0xFFFFFFFF, ISP_INIT_CLR_REG);

	sprd_ispdrv_glb_reg_wr(idx, ISP_C0_INT_BASE + ISP_INT_CLR0,
		0xFFFFFFFF, ISP_INIT_CLR_REG);
	sprd_ispdrv_glb_reg_wr(idx, ISP_C0_INT_BASE + ISP_INT_CLR1,
		0xFFFFFFFF, ISP_INIT_CLR_REG);

	sprd_ispdrv_glb_reg_wr(idx, ISP_C1_INT_BASE + ISP_INT_CLR0,
		0xFFFFFFFF, ISP_INIT_CLR_REG);
	sprd_ispdrv_glb_reg_wr(idx, ISP_C1_INT_BASE + ISP_INT_CLR1,
		0xFFFFFFFF, ISP_INIT_CLR_REG);

	sprd_ispdrv_glb_reg_wr(idx, ISP_MMU_INT_BASE + ISP_MMU_INT_CLR,
		0xFFFFFFFF, ISP_INIT_CLR_REG);
}

static int sprd_ispdrv_reset(uint32_t idx)
{
	uint32_t flag = 0, time_out = 0;
	enum isp_id id = ISP_ID_0;
	enum dcam_drv_rtn rtn = ISP_RTN_SUCCESS;

	id = ISP_GET_ISP_ID(idx);

	/* then wait for AHB busy cleared */
	while (++time_out < ISP_AXI_STOP_TIMEOUT) {
		if (1 == ((ISP_REG_RD(id,  ISP_INT_STATUS) & BIT_3) >> 3))
			break;
	}

	if (time_out >= ISP_AXI_STOP_TIMEOUT) {
		pr_err("fail to reset ISP%d: timeout %d\n", id, time_out);
		return ISP_RTN_TIME_OUT;
	}

	flag = MASK_MM_AHB_RF_ISP_SOFT_RST;
	regmap_update_bits(cam_ahb_gpr,
		REG_MM_AHB_RF_AHB_RST, flag, flag);
	udelay(1);
	regmap_update_bits(cam_ahb_gpr,
		REG_MM_AHB_RF_AHB_RST, flag, ~flag);

	flag = MASK_MM_AHB_RF_ISP_AHB_SOFT_RST;
	regmap_update_bits(cam_ahb_gpr,
		REG_MM_AHB_RF_AHB_RST, flag, flag);
	udelay(1);
	regmap_update_bits(cam_ahb_gpr,
		REG_MM_AHB_RF_AHB_RST, flag, ~flag);

	flag = MASK_MM_AHB_RF_ISP_VAU_SOFT_RST;
	regmap_update_bits(cam_ahb_gpr,
		REG_MM_AHB_RF_AHB_RST, flag, flag);
	udelay(1);
	regmap_update_bits(cam_ahb_gpr,
		REG_MM_AHB_RF_AHB_RST, flag, ~flag);

	sprd_ispdrv_glb_reg_awr(idx,
		ISP_AXI_ITI2AXIM_CTRL, ~BIT_26, ISP_AXI_REG);

	return -rtn;
}

static void sprd_ispdrv_quickstop(uint32_t idx)
{
	enum isp_id id = 0;

	id = ISP_GET_ISP_ID(idx);

	sprd_ispdrv_glb_reg_owr(idx,
		ISP_AXI_ITI2AXIM_CTRL, BIT_26, ISP_AXI_REG);
	udelay(10);
}

static int sprd_ispdrv_slice_init_scaler_get(struct slice_scaler_path *scaler,
	struct isp_path_desc *path)
{
	if (!scaler || !path) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	scaler->trim0_size_x = path->trim0_info.size_x;
	scaler->trim0_size_y = path->trim0_info.size_y;
	scaler->trim0_start_x = path->trim0_info.start_x;
	scaler->trim0_start_y = path->trim0_info.start_y;

	if (path->deci_info.deci_x_eb)
		scaler->deci_x = 1 << (path->deci_info.deci_x + 1);
	else
		scaler->deci_x = 1;

	if (path->deci_info.deci_y_eb)
		scaler->deci_y = 1 << (path->deci_info.deci_y + 1);
	else
		scaler->deci_y = 1;

	scaler->odata_mode = path->odata_mode;
	scaler->scaler_bypass = path->scaler_bypass;
	scaler->scaler_out_width = path->dst.w;
	scaler->scaler_out_height = path->dst.h;
	scaler->scaler_factor_in = path->scaler_info.scaler_factor_in;
	scaler->scaler_ver_factor_in = path->scaler_info.scaler_ver_factor_in;
	scaler->scaler_factor_out = path->scaler_info.scaler_factor_out;
	scaler->scaler_ver_factor_out = path->scaler_info.scaler_ver_factor_out;
	scaler->scaler_y_ver_tap = path->scaler_info.scaler_y_ver_tap;
	scaler->scaler_uv_ver_tap = path->scaler_info.scaler_uv_ver_tap;

	return 0;
}

static int sprd_ispdrv_slice_init_param_get(struct slice_param_in *in_ptr,
	struct isp_pipe_dev *dev)
{
	enum isp_drv_rtn ret = ISP_RTN_SUCCESS;
	struct isp_path_desc *path_cap = NULL;
	struct slice_scaler_path *scaler = NULL;
	struct slice_store_path *store = NULL;
	struct slice_store_fbc_path *store_fbc = NULL;
	struct isp_module *module = NULL;
	struct camera_frame *frame = NULL;
	struct isp_nr3_param *nr3_info  = NULL;
	struct slice_3dnr_info *slice_3dnr_in = NULL;

	if (!dev || !in_ptr) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	module = &dev->module_info;
	path_cap = &module->isp_path[ISP_SCL_CAP];
	nr3_info = &path_cap->nr3_param;

	in_ptr->com_idx = module->com_idx;
	ISP_SET_SCENE_ID(in_ptr->com_idx, ISP_SCENE_CAP);
	frame = &dev->offline_frame[ISP_SCENE_CAP];

	pr_debug("fmcu yaddr 0x%x yaddr_vir 0x%x  fd 0x%x 0x%x %p %p\n",
		frame->yaddr, frame->yaddr_vir, frame->buf_info.mfd[0],
		frame->yaddr, frame->buf_info.client[0],
		frame->buf_info.handle[0]);

	in_ptr->fetch_addr.chn0 = frame->buf_info.iova[1] + frame->yaddr;
	in_ptr->fetch_addr.chn1 = frame->buf_info.iova[1] + frame->uaddr;
	in_ptr->fetch_addr.chn2 = frame->buf_info.iova[1] + frame->vaddr;
	in_ptr->img_size.width = frame->width;
	in_ptr->img_size.height = frame->height;
	in_ptr->fetch_format = ISP_FETCH_CSI2_RAW10;/*TBD*/
	in_ptr->fmcu_addr_vir = (uint32_t *)dev->fmcu_slice.buf_info.kaddr[0];

	if (nr3_info->need_3dnr) {
		slice_3dnr_in = &in_ptr->nr3_info;
		slice_3dnr_in->need_slice = 1;
		slice_3dnr_in->fetch_3dnr_frame.format =
			nr3_info->fetch_format;
		slice_3dnr_in->store_3dnr_frame.format =
			nr3_info->store_format;
		if (ISP_3DNR_NUM - module->full_zsl_queue.valid_cnt) {
			slice_3dnr_in->cur_frame_num = ISP_3DNR_NUM
				- module->full_zsl_queue.valid_cnt;
		} else {
			pr_err("fail to get right zsl cnt %d\n",
				module->full_zsl_queue.valid_cnt);
			return -EFAULT;
		}
		nr3_info->cur_cap_frame = slice_3dnr_in->cur_frame_num;
		if (nr3_info->cur_cap_frame != ISP_3DNR_NUM)
			ISP_REG_MWR(module->com_idx,
				ISP_STORE_PRE_CAP_BASE + ISP_STORE_PARAM,
				BIT_0, 1);
		slice_3dnr_in->mv_x = frame->mv.mv_x;
		slice_3dnr_in->mv_y = frame->mv.mv_y;
		if (slice_3dnr_in->cur_frame_num % 2) {
			slice_3dnr_in->store_3dnr_frame.addr.chn0 =
				nr3_info->buf_info.iova[1];
			slice_3dnr_in->store_3dnr_frame.addr.chn1 =
				nr3_info->buf_info.iova[1] +
				frame->width * frame->height;
			slice_3dnr_in->fetch_3dnr_frame.addr.chn0 =
				nr3_info->buf_info.iova[0];
			slice_3dnr_in->fetch_3dnr_frame.addr.chn1 =
				nr3_info->buf_info.iova[0] +
				frame->width * frame->height;
		} else {
			slice_3dnr_in->store_3dnr_frame.addr.chn0 =
				nr3_info->buf_info.iova[0];
			slice_3dnr_in->store_3dnr_frame.addr.chn1 =
				nr3_info->buf_info.iova[0] +
				frame->width * frame->height;
			slice_3dnr_in->fetch_3dnr_frame.addr.chn0 =
				nr3_info->buf_info.iova[1];
			slice_3dnr_in->fetch_3dnr_frame.addr.chn1 =
				nr3_info->buf_info.iova[1] +
				frame->width * frame->height;
		}
	}

	if (path_cap->valid) {
		in_ptr->cap_slice_need = 1;
		scaler = &in_ptr->scaler_frame[SLICE_PATH_CAP];
		ret = sprd_ispdrv_slice_init_scaler_get(scaler, path_cap);
		if (!nr3_info->need_3dnr || (nr3_info->need_3dnr
			&& nr3_info->cur_cap_frame == ISP_3DNR_NUM)) {
			ret = sprd_isp_path_next_frm_set(module,
				ISP_PATH_IDX_CAP, frame);
		}
		store = &in_ptr->store_frame[SLICE_PATH_CAP];
		store_fbc = &in_ptr->store_fbc_frame[SLICE_PATH_CAP];
		store->format = path_cap->store_info.color_format;
		store->size.width = path_cap->dst.w;
		store->size.height = path_cap->dst.h;
		store->addr.chn0 = path_cap->store_info.addr.chn0;
		store->addr.chn1 = path_cap->store_info.addr.chn1;
		store->addr.chn2 = path_cap->store_info.addr.chn2;
		if (!path_cap->store_fbc_info.bypass) {
			store_fbc->yaddr = path_cap->store_fbc_info.yaddr;
			store_fbc->yheader_addr =
				path_cap->store_fbc_info.yheader;
			store_fbc->header_offset =
				path_cap->store_fbc_info.header_offset;
		}
	}

	in_ptr->nlm_col_center = dev->isp_k_param.nlm_col_center;
	in_ptr->nlm_row_center = dev->isp_k_param.nlm_row_center;
	in_ptr->ynr_center_x = dev->isp_k_param.ynr_center_x;
	in_ptr->ynr_center_y = dev->isp_k_param.ynr_center_y;
	in_ptr->is_raw_capture = dev->is_raw_capture;

	return ret;
}

static void sprd_ispdrv_fmcu_cmd_trace(
	struct isp_fmcu_slice_desc *fmcu_slice)
{
#ifdef ISP_DRV_DEBUG
	unsigned int i = 0;
	unsigned long addr = 0;

	addr = (unsigned long)fmcu_slice->buf_info.kaddr[0];

	pr_info("fmcu slice cmd num %d\n", fmcu_slice->fmcu_num);
	for (i = 0; i <= fmcu_slice->fmcu_num; i += 2) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			REG_RD(addr),
			REG_RD(addr + 4),
			REG_RD(addr + 8),
			REG_RD(addr + 12));
		addr += 16;
	}
#endif
}

static int sprd_ispdrv_fmcu_pre_proc(struct isp_pipe_dev *dev)
{
	uint32_t fmcu_num = 0;
	enum isp_id idx = ISP_ID_0;
	enum isp_drv_rtn ret = ISP_RTN_SUCCESS;
	struct isp_fmcu_slice_desc *fmcu_slice = NULL;
	struct slice_param_in slice_in = {0};
	struct isp_module *module = NULL;

	fmcu_slice = &dev->fmcu_slice;
	module = &dev->module_info;
	idx = ISP_GET_ISP_ID(dev->com_idx);

	ret = sprd_isp_3dnr_cap_frame_proc(dev,
		&dev->offline_frame[ISP_SCENE_CAP]);
	if (ret) {
		ret = ISP_RTN_PARA_ERR;
		pr_err("fail to cfg 3dnr cap\n");
		goto exit;
	}

	ret = sprd_ispdrv_slice_init_param_get(&slice_in, dev);
	if (ret) {
		ret = ISP_RTN_PARA_ERR;
		pr_err("fail to get slice init param and need new frame\n");
		goto exit;
	}

	ret = sprd_isp_slice_fmcu_slice_cfg(fmcu_slice->slice_handle,
		&slice_in, &fmcu_num);
	if (ret) {
		pr_err("fail to cfg fmcu slice\n");
		goto exit;
	}
	fmcu_slice->fmcu_num = fmcu_num;

	sprd_ispdrv_fmcu_cmd_trace(fmcu_slice);

#ifdef CONFIG_64BIT
	__flush_dcache_area(fmcu_slice->buf_info.kaddr[0], PAGE_SIZE);
#else
	flush_kernel_vmap_range(fmcu_slice->buf_info.kaddr[0], PAGE_SIZE);
#endif

	ISP_HREG_WR(idx, ISP_FMCU_DDR_ADDR, fmcu_slice->buf_info.iova[0]);
	ISP_HREG_MWR(idx, ISP_FMCU_CTRL, 0xFFFF0000,
		fmcu_slice->fmcu_num << 16);
	ISP_HREG_MWR(idx, ISP_FMCU_CTRL, 0x8, 1 << 3);
	ISP_HREG_WR(idx, ISP_FMCU_ISP_REG_REGION, 0x06300620);

exit:
	return ret;
}

static void sprd_ispdrv_common_cfg(uint32_t com_idx)
{
	enum isp_id id = 0;

	id = ISP_GET_ISP_ID(com_idx);

	ISP_HREG_MWR(id, ISP_AXI_ISOLATION, BIT_0, 0);
	ISP_HREG_MWR(id, ISP_ARBITER_ENDIAN_CH0, BIT_0, 0);
	ISP_HREG_WR(id, ISP_ARBITER_CHK_SUM_CLR, 0xF10);
	ISP_HREG_WR(id, ISP_ARBITER_CHK_SUM0, 0x0);

	ISP_REG_MWR(com_idx, ISP_BWU_PARAM, BIT_0, 0);
	ISP_REG_WR(com_idx, ISP_BWU_PARAM, (0x4 & 0x7) << 16);

	ISP_REG_MWR(com_idx, ISP_CFAE_NEW_CFG0, BIT_0, 0);
	ISP_REG_WR(com_idx, ISP_CFAE_INTP_CFG0, (500 & 0xFFFF));
	ISP_REG_WR(com_idx, ISP_CFAE_INTP_CFG1,
		(((1 & 0x1) << 31) | ((20 & 0xFFF) << 12)
		| ((127 & 0xFF) << 4)));
	ISP_REG_WR(com_idx, ISP_CFAE_INTP_CFG2, ((8 & 0x3F)
		| ((0 & 0x1FFFF) << 8)));
	ISP_REG_WR(com_idx, ISP_CFAE_INTP_CFG3,
		(((8 & 0x1F) << 20) | ((8 & 0x1F) << 12) | (280 & 0x3FF)));
	ISP_REG_WR(com_idx, ISP_CFAE_INTP_CFG4, ((100 & 0xFFF)
		| ((100 & 0x3FF) << 16)));
	ISP_REG_WR(com_idx, ISP_CFAE_INTP_CFG5, ((200 & 0xFFF)
		| ((200 & 0x3FF) << 16)));

	ISP_REG_MWR(com_idx, ISP_CCE_PARAM, BIT_0, 0);
	ISP_REG_WR(com_idx, ISP_CCE_MATRIX0, (((0x96 & 0x7FF) << 11)
	    | (0x4D & 0x7FF)));
	ISP_REG_WR(com_idx, ISP_CCE_MATRIX1, (((0x7D5 & 0x7FF) << 11)
	    | (0x1D & 0x7FF)));
	ISP_REG_WR(com_idx, ISP_CCE_MATRIX2, (((0x80 & 0x7FF) << 11
	    | (0x7AB & 0x7FF))));
	ISP_REG_WR(com_idx, ISP_CCE_MATRIX3, (((0x795 & 0x7FF) << 11)
	    | (0x80 & 0x7FF)));
	ISP_REG_WR(com_idx, ISP_CCE_MATRIX4, ((0x7E8) & 0x7FF));
	ISP_REG_WR(com_idx, ISP_CCE_SHIFT,
		((0 & 0x1FF) | ((0 & 0x1FF) << 9)
		| (((0 & 0x1FF) << 18) & 0x7FFFFFF)));

	ISP_REG_MWR(com_idx, ISP_VST_PARA, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_NLM_PARA, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_IVST_PARA, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_GC2_CTRL, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_GRGB_CTRL, BIT_0, 1);

	ISP_REG_MWR(com_idx, ISP_CMC10_PARAM, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_LTM_STAT_RGB_BASE + ISP_LTM_PARAMETERS,
		BIT_0, 0x1);
	ISP_REG_MWR(com_idx, ISP_LTM_MAP_RGB_BASE + ISP_LTM_MAP_PARAM0,
		BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_GAMMA_PARAM, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_HSV_PARAM, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_PSTRZ_PARA, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_HIST_PARAM, BIT_0, 1);

	ISP_REG_MWR(com_idx, ISP_UVD_PARAM, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_LTM_STAT_YUV_BASE + ISP_LTM_PARAMETERS,
		BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_LTM_MAP_YUV_BASE + ISP_LTM_MAP_PARAM0,
		BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_PRECDN_PARAM, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_YNR_CONTRL0, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_EE_PARAM, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_YGAMMA_PARAM, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_CDN_PARAM, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_POSTCDN_COMMON_CTRL, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_YDELAY_PARAM, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_IIRCNR_PARAM, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_YUV_YRANDOM_PARAM1, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_BCHS_PARAM, BIT_0, 1);

	ISP_REG_WR(com_idx, ISP_SCALER_PRE_CAP_BASE + ISP_SCALER_HBLANK,
		0x4040);
	ISP_REG_WR(com_idx, ISP_SCALER_PRE_CAP_BASE + ISP_SCALER_RES, 0xFF);
	ISP_REG_WR(com_idx, ISP_SCALER_PRE_CAP_BASE + ISP_SCALER_DEBUG, 1);
	ISP_REG_MWR(com_idx, ISP_STORE_BASE + ISP_STORE_PARAM, BIT_0, 1);

	ISP_REG_MWR(com_idx, ISP_FBD_RAW_SEL, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_FBC_3DNR_STORE_PARAM, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_NF_CTRL, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_THMB_CFG, BIT_0, 1);
	ISP_REG_MWR(com_idx, ISP_STORE_THU_BASE + ISP_STORE_PARAM, BIT_0, 1);
}

static int sprd_ispdrv_sel_cap_frame(void *handle)
{
	enum isp_drv_rtn ret = ISP_RTN_SUCCESS;
	struct isp_pipe_dev *dev = NULL;
	struct isp_module *module = NULL;
	struct camera_frame frame;
	int32_t frame_diff = 0, frame_index[2] = {0, 0}, i = 0, j = 0;
	s64 timestamp[2][ISP_ZSL_BUF_NUM], cycle[2], min_cycle = 0,
		time_diff = 0, min_time_diff = 0;
	uint32_t idx = 0, zsl_queue_nodes[2], frame_id[2][ISP_ZSL_BUF_NUM];

	if (!handle) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	dev = (struct isp_pipe_dev *)handle;
	module = &dev->module_info;
	idx = ISP_GET_ISP_ID(dev->com_idx);

	memset((void *)&frame, 0x00, sizeof(frame));
	if (s_isp_group.dual_cam && !dev->is_raw_capture) {
		if (!s_isp_group.dual_sel_cnt) {
			for (j = 0; j < 2; j++) {
				dev = s_isp_group.isp_dev[j];
				zsl_queue_nodes[j] =
					sprd_cam_queue_frm_cur_nodes
					(&dev->module_info.full_zsl_queue);
				i = 0;
				while (i < zsl_queue_nodes[j]) {
					sprd_cam_queue_frm_dequeue(&dev
						->module_info.full_zsl_queue,
						&frame);
					frame_id[j][i] = frame.frame_id;
					timestamp[j][i] =
						frame.time.boot_time;
					sprd_cam_queue_frm_enqueue(&dev
						->module_info.full_zsl_queue,
						&frame);
					i++;
				}
				if (zsl_queue_nodes[j] > 1)
					cycle[j] = (timestamp[j][i - 1]
						- timestamp[j][0])
						/ (frame_id[j][i - 1]
						- frame_id[j][0]);
				else
					cycle[j] = 0x7FFFFFFFFFFFFFFF;
			}
			min_cycle = min(cycle[0], cycle[1]);
			min_time_diff = 0x7FFFFFFFFFFFFFFF;

			pr_debug("%d %d %012lld, %012lld %012lld %012lld, %012lld %012lld %012lld\n",
				zsl_queue_nodes[0], zsl_queue_nodes[1],
				min_cycle,
				timestamp[0][0], timestamp[0][1], cycle[0],
				timestamp[1][0], timestamp[1][1], cycle[1]);

			for (j = zsl_queue_nodes[0] - 1; j >= 0; --j) {
				for (i = zsl_queue_nodes[1] - 1; i >= 0; --i) {
					time_diff = abs(timestamp[0][j]
						- timestamp[1][i]);
					if (time_diff < (min_cycle >> 1)) {
						frame_index[0] = j;
						frame_index[1] = i;
						min_time_diff = time_diff;
						pr_debug("%d %d\n",
							frame_index[0],
							frame_index[1]);
						goto sel_ok;
					} else if (time_diff < min_time_diff) {
						frame_index[0] = j;
						frame_index[1] = i;
						min_time_diff = time_diff;
						pr_debug("%d %d %012lld\n",
							frame_index[0],
							frame_index[1],
							time_diff);
					}
				}
			}

sel_ok:
			if (s_isp_group.dual_frame_gap
				&& (min_time_diff < min_cycle)) {
				if (zsl_queue_nodes[0] > 1) {
					if (frame_index[0] > 0)
						--frame_index[0];
					else
						++frame_index[0];
				} else if (zsl_queue_nodes[1] > 1) {
					if (frame_index[1] > 0)
						--frame_index[1];
					else
						++frame_index[1];
				}
				pr_debug("%d %d\n", frame_index[0],
					frame_index[1]);
			}
			s_isp_group.frame_index[0] = frame_index[0];
			s_isp_group.frame_index[1] = frame_index[1];
		}

		ret = sprd_cam_queue_frm_dequeue_n(&module->full_zsl_queue,
			s_isp_group.frame_index[idx], &frame);
		if (ret) {
			ret = -1;
			pr_err("fail to dequeue capture frame\n");
			goto exit;
		}

		s_isp_group.timestamp[idx] = frame.time.boot_time;
		if (++s_isp_group.dual_sel_cnt == 2) {
			struct isp_pipe_dev *dev0 = s_isp_group.isp_dev[0];
			struct isp_pipe_dev *dev1 = s_isp_group.isp_dev[1];
			struct dcam_group *dcam_group = NULL;

			dcam_group = sprd_dcam_drv_group_get();
			if (!dcam_group || !dev0 || !dev1) {
				pr_err("fail to get valid input ptr\n");
				return -EFAULT;
			}

			frame_diff = dev0->frame_id - dev1->frame_id;
			pr_debug("frame %d %d %d %d %d\n",
				dev0->frame_id, dev1->frame_id,
				frame_diff,
				s_isp_group.frame_index[0],
				s_isp_group.frame_index[1]);
			time_diff = s_isp_group.timestamp[0] -
					s_isp_group.timestamp[1];
			time_diff = time_diff > 0 ? time_diff : -time_diff;
			pr_debug("sel %12lld %12lld %12lld %012lld\n",
			s_isp_group.capture_param.timestamp,
			s_isp_group.timestamp[0], s_isp_group.timestamp[1],
			time_diff);
		}
	} else {
		ret = sprd_cam_queue_frm_dequeue(
			&module->full_zsl_queue, &frame);
		if (ret) {
			ret = -ENOENT;
			pr_err("fail to dequeue full_zsl_queue\n");
			goto exit;
		}
	}

	dev = (struct isp_pipe_dev *)handle;
	frame.buf_info.dev = &s_isp_pdev->dev;
	ret = sprd_cam_buf_addr_map(&frame.buf_info);
	if (ret) {
		ret = -1;
		pr_err("fail to map full_path buf addr\n");
		goto exit;
	}

	memcpy(&dev->offline_frame[ISP_SCENE_CAP], &frame,
		sizeof(struct camera_frame));

exit:
	return ret;
}

static uint32_t sprd_ispdrv_path_deci_factor_get(uint32_t src_size,
				uint32_t dst_size)
{
	uint32_t factor = 0;

	if (0 == src_size || 0 == dst_size)
		return factor;

	/* factor: 0 - 1/2, 1 - 1/4, 2 - 1/8, 3 - 1/16 */
	for (factor = 0; factor < CAMERA_PATH_DECI_FAC_MAX; factor++) {
		if (src_size < (uint32_t) (dst_size * (1 << (factor + 1))))
			break;
	}

	return factor;
}

static int sprd_ispdrv_sc_size_calc(struct isp_path_desc *path)
{
	uint32_t tmp_dstsize = 0, align_size = 0;
	uint32_t d_max = CAMERA_SC_COEFF_DOWN_MAX;
	uint32_t u_max = CAMERA_SC_COEFF_UP_MAX;
	uint32_t f_max = CAMERA_PATH_DECI_FAC_MAX;
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	struct isp_trim_info *in_trim;
	struct camera_size *out_size;

	if (path == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	in_trim = &path->trim0_info;
	out_size = &path->dst;
	if (in_trim->size_x > (out_size->w * d_max * (1 << f_max)) ||
		in_trim->size_y > (out_size->h * d_max * (1 << f_max)) ||
		in_trim->size_x * u_max < out_size->w ||
		in_trim->size_y * u_max < out_size->h) {
		rtn = ISP_RTN_PATH_SC_ERR;
	} else {
		path->scaler_info.scaler_factor_in = in_trim->size_x;
		path->scaler_info.scaler_ver_factor_in = in_trim->size_y;
		if (in_trim->size_x > out_size->w * d_max) {
			tmp_dstsize = out_size->w * d_max;
			path->deci_info.deci_x =
				sprd_ispdrv_path_deci_factor_get(
						in_trim->size_x, tmp_dstsize);
			path->deci_info.deci_x_eb = 1;
			align_size = (1 << (path->deci_info.deci_x + 1)) *
				ISP_PIXEL_ALIGN_WIDTH;
			in_trim->size_x = (in_trim->size_x)
				& ~(align_size - 1);
			in_trim->start_x = (in_trim->start_x)
				& ~(align_size - 1);
			path->scaler_info.scaler_factor_in =
				in_trim->size_x >> (path->deci_info.deci_x + 1);
		} else {
			path->deci_info.deci_x = 0;
			path->deci_info.deci_x_eb = 0;
		}

		if (in_trim->size_y > out_size->h * d_max) {
			tmp_dstsize = out_size->h * d_max;
			path->deci_info.deci_y =
				sprd_ispdrv_path_deci_factor_get(
						in_trim->size_y, tmp_dstsize);
			path->deci_info.deci_y_eb = 1;
			align_size = (1 << (path->deci_info.deci_y + 1)) *
				ISP_PIXEL_ALIGN_HEIGHT;
			in_trim->size_y = (in_trim->size_y)
				& ~(align_size - 1);
			in_trim->start_y = (in_trim->start_y)
				& ~(align_size - 1);
			path->scaler_info.scaler_ver_factor_in =
				in_trim->size_y >> (path->deci_info.deci_y + 1);
		} else {
			path->deci_info.deci_y = 0;
			path->deci_info.deci_y_eb = 0;
		}

		path->scaler_info.scaler_ver_factor_out = path->dst.h;
		path->scaler_info.scaler_factor_out = path->dst.w;
	}

	return -rtn;
}

static int sprd_ispdrv_sc_coeff_set(struct isp_module *module,
			enum isp_path_index path_index,
			struct isp_path_desc *path,
			struct isp_coeff *coeff)
{
	uint32_t scale2yuv420 = 0;
	uint32_t *tmp_buf = NULL;
	uint32_t *h_coeff = NULL;
	uint32_t *v_coeff = NULL;
	uint32_t *v_chroma_coeff = NULL;
	unsigned char y_tap = 0;
	unsigned char uv_tap = 0;

	if (!module || !path || !coeff) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	if (path->output_format == DCAM_YUV420)
		scale2yuv420 = 1;

	tmp_buf = coeff->coeff_buf;
	if (tmp_buf == NULL) {
		pr_err("fail to get valid coeff_buf\n");
		return -EFAULT;
	}

	h_coeff = tmp_buf;
	v_coeff = tmp_buf + (ISP_SC_COEFF_COEF_SIZE / 4);
	v_chroma_coeff = v_coeff + (ISP_SC_COEFF_COEF_SIZE / 4);

	wait_for_completion_interruptible(&module->scale_coeff_mem_com);
	if (!(cam_gen_scale_coeff((short)path->scaler_info.scaler_factor_in,
				(short)path->scaler_info.scaler_ver_factor_in,
				(short)path->dst.w,
				(short)path->dst.h,
				h_coeff,
				v_coeff,
				v_chroma_coeff,
				scale2yuv420,
				&y_tap,
				&uv_tap,
				tmp_buf + (ISP_SC_COEFF_COEF_SIZE * 3 / 4),
				ISP_SC_COEFF_TMP_SIZE))) {
		pr_err("fail to call cam_gen_scale_coeff\n");
		complete(&module->scale_coeff_mem_com);
		return -DCAM_RTN_PATH_GEN_COEFF_ERR;
	}

	path->scaler_info.scaler_y_ver_tap = y_tap;
	path->scaler_info.scaler_uv_ver_tap = uv_tap;

	complete(&module->scale_coeff_mem_com);

	return ISP_RTN_SUCCESS;
}

static int sprd_ispdrv_path_scaler(struct isp_module *module,
		enum isp_path_index path_index,
		struct isp_path_desc *path)
{
	uint32_t idx = 0;
	unsigned long cfg_reg = 0;
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	struct isp_coeff *coeff = NULL;

	if (!module || !path) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	idx = module->com_idx;

	if (path_index == ISP_PATH_IDX_PRE)
		cfg_reg = ISP_SCALER_PRE_CAP_BASE + ISP_SCALER_CFG;
	else if (path_index == ISP_PATH_IDX_VID)
		cfg_reg = ISP_SCALER_VID_BASE + ISP_SCALER_CFG;
	else if (path_index == ISP_PATH_IDX_CAP)
		cfg_reg = ISP_SCALER_PRE_CAP_BASE + ISP_SCALER_CFG;
	else
		return -EFAULT;

	if (path->output_format == DCAM_RAWRGB ||
		path->output_format == DCAM_JPEG) {
		pr_debug("out format is %d, no need scaler\n",
			path->output_format);
		return ISP_RTN_SUCCESS;
	}

	rtn = sprd_ispdrv_sc_size_calc(path);
	if (rtn)
		return rtn;

	if (path->scaler_info.scaler_factor_in ==
		path->scaler_info.scaler_factor_out &&
		path->scaler_info.scaler_ver_factor_in ==
		path->scaler_info.scaler_ver_factor_out &&
		path->output_format == DCAM_YUV422 &&
		path_index != ISP_PATH_IDX_0) {
		path->scaler_bypass = 1;
		ISP_REG_MWR(idx, cfg_reg, BIT_20, 1 << 20);
	} else {
		path->scaler_bypass = 0;
		ISP_REG_MWR(idx, cfg_reg, BIT_20, 0 << 20);
		coeff = &path->coeff_latest;
		rtn = sprd_ispdrv_sc_coeff_set(module, path_index, path, coeff);
	}

	return rtn;
}

static int sprd_isppath_afbc_store(struct isp_path_desc *path)
{
	uint32_t w_tile_num = 0, h_tile_num = 0;
	uint32_t pad_width = 0, pad_height = 0;
	uint32_t header_size = 0, tile_data_size = 0;
	uint32_t header_addr = 0;
	struct isp_store_fbc_info *store_fbc_info = NULL;

	if (!path) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	store_fbc_info = &path->store_fbc_info;
	store_fbc_info->size.w = path->dst.w;
	store_fbc_info->size.h = path->dst.h;

	pad_width = (store_fbc_info->size.w + 32 - 1) / 32 * 32;
	pad_height = (store_fbc_info->size.h + 8 - 1) / 8 * 8;

	store_fbc_info->pad_w = pad_width;
	store_fbc_info->pad_h = pad_height;

	w_tile_num = pad_width / 32;
	h_tile_num = pad_height / 8;

	header_size = w_tile_num * h_tile_num * 16;
	tile_data_size = w_tile_num * h_tile_num * 384;
	header_addr = store_fbc_info->yheader;

	store_fbc_info->header_offset = (header_size + 1024 - 1) / 1024 * 1024;

	store_fbc_info->yheader = header_addr;

	store_fbc_info->yaddr = store_fbc_info->yheader +
		store_fbc_info->header_offset;

	store_fbc_info->tile_number_pitch = w_tile_num;

	return 0;
}

static int sprd_ispdrv_path_common_start(struct isp_module *module,
	struct isp_path_desc *cur_path,
	enum isp_path_index cur_path_idx,
	struct camera_frame *dcam_frame)
{
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	struct isp_zoom_param zoom_param;

	memset((void *)&zoom_param, 0x00, sizeof(zoom_param));
	rtn = sprd_cam_queue_buf_read(&cur_path->coeff_queue, &zoom_param);
	if (!rtn) {
		memcpy(&cur_path->coeff_latest.param,
				&zoom_param, sizeof(struct isp_zoom_param));
		sprd_isp_path_param_cfg(cur_path);
	}

	if (module->need_downsizer) {
		if (cur_path_idx == ISP_PATH_IDX_PRE ||
			cur_path_idx == ISP_PATH_IDX_VID) {
			zoom_param.in_size.w = dcam_frame->width;
			zoom_param.in_size.h = dcam_frame->height;
			zoom_param.in_rect.x = 0;
			zoom_param.in_rect.y = 0;
			zoom_param.in_rect.w = dcam_frame->width;
			zoom_param.in_rect.h = dcam_frame->height;
			zoom_param.out_size = cur_path->out_size;
			memcpy(&cur_path->coeff_latest.param, &zoom_param,
				sizeof(struct isp_zoom_param));
			sprd_isp_path_param_cfg(cur_path);
		}
	}
	pr_debug("path_idx %d, in_size %d, %d, in_rect %d, %d, %d, %d, out_size %d, %d\n",
		cur_path_idx,
		zoom_param.in_size.w,
		zoom_param.in_size.h,
		zoom_param.in_rect.x,
		zoom_param.in_rect.y,
		zoom_param.in_rect.w,
		zoom_param.in_rect.h,
		zoom_param.out_size.w,
		zoom_param.out_size.h);
	rtn = sprd_ispdrv_path_scaler(module, cur_path_idx, cur_path);
	if (rtn) {
		pr_err("fail to set pre path scaler\n");
		return rtn;
	}

	if (!cur_path->store_fbc_info.bypass) {
		rtn = sprd_isppath_afbc_store(cur_path);
		if (rtn) {
			pr_err("fail to set cap path scaler\n");
			return rtn;
		}
	}

	if (cur_path_idx != ISP_PATH_IDX_CAP) {
		rtn = sprd_isp_path_next_frm_set(module,
			cur_path_idx, dcam_frame);
		if (rtn) {
			pr_err("fail to set next frame\n");
			return rtn;
		}
	}

	sprd_isp_path_pathset(module, cur_path, cur_path_idx);

	return rtn;
}

static int sprd_ispdrv_path_start(void *isp_handle,
	enum isp_path_index path_index, struct camera_frame *frame)
{
	enum isp_id id = ISP_ID_0;
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	enum isp_path_index cur_path_idx = ISP_PATH_IDX_0;
	struct isp_pipe_dev *dev = NULL;
	struct isp_module *module = NULL;
	struct isp_path_desc *pre = NULL;
	struct isp_path_desc *vid = NULL;
	struct isp_path_desc *cap = NULL;
	struct isp_path_desc *cur_path = NULL;
	struct isp_fmcu_slice_desc *fmcu_slice = NULL;

	if (!isp_handle) {
		pr_err("fail to get valid input ptr\n");
		return -ISP_RTN_PARA_ERR;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	id = ISP_GET_ISP_ID(dev->com_idx);
	module = &dev->module_info;
	fmcu_slice = &dev->fmcu_slice;
	pre = &module->isp_path[ISP_SCL_PRE];
	vid = &module->isp_path[ISP_SCL_VID];
	cap = &module->isp_path[ISP_SCL_CAP];

	if ((ISP_PATH_IDX_PRE & path_index) &&
		module->isp_path[ISP_SCL_PRE].valid) {
		cur_path = pre;
		cur_path_idx = ISP_PATH_IDX_PRE;
		sprd_ispdrv_path_common_start(module, cur_path,
			cur_path_idx, frame);
		if (!cur_path->is_reserved)
			sprd_isp_int_path_sof(id, ISP_PATH_IDX_PRE, dev);
#if ISP_HIST_ENABLE
		/* set isp statis buf before stream on */
		rtn = sprd_cam_statistic_next_buf_set(
			&module->statis_module_info,
			ISP_HIST_BLOCK,
			frame->frame_id);
		if (rtn)
			pr_debug("fail to set hist statis buf\n");
#endif
	}

	if ((ISP_PATH_IDX_VID & path_index) &&
		module->isp_path[ISP_SCL_VID].valid) {
		cur_path = vid;
		cur_path_idx = ISP_PATH_IDX_VID;
		sprd_ispdrv_path_common_start(module, cur_path,
			cur_path_idx, frame);
	}

	if ((ISP_PATH_IDX_CAP & path_index) &&
		module->isp_path[ISP_SCL_CAP].valid) {
		cur_path = cap;
		cur_path_idx = ISP_PATH_IDX_CAP;
		dev->dcam_full_path_stop = 0;
		if (dev->need_4in1)
			rtn = sprd_isp_path_4in1_scaler_update(dev);
		if (rtn) {
			pr_err("fail to update 4in1 scaler info\n");
			return rtn;
		}
		sprd_ispdrv_path_common_start(module, cur_path,
			cur_path_idx, frame);
	}

	if ((pre->nr3_param.need_3dnr && frame != NULL)
		|| (vid->nr3_param.need_3dnr && frame != NULL)) {
		if (((ISP_PATH_IDX_PRE & path_index)
			|| (ISP_PATH_IDX_VID & path_index))
			&& frame->zoom_info.zoom_work) {
			isp_3dnr_default_param(dev->com_idx);
			pre->nr3_param.blending_cnt = 0;
			vid->nr3_param.blending_cnt = 0;
		} else {
			sprd_isp_3dnr_conversion_mv(&pre->nr3_param, frame);
			sprd_isp_3dnr_param_get(&pre->nr3_param, frame);
			isp_3dnr_config_param(dev->com_idx,
				ISP_OPERATE_PRE, &pre->nr3_param);
		}
	} else {
		isp_3dnr_default_param(dev->com_idx);
	}

	if (cur_path->input_format == DCAM_CAP_MODE_YUV)
		ISP_HREG_MWR(id, ISP_COMMON_SPACE_SEL, 0x0F, 0x0A);

	sprd_ispdrv_common_cfg(module->com_idx);
	sprd_ispdrv_irq_mask_en(module->com_idx);

	return rtn;
}

static void sprd_ispdrv_default_regvalue_set(uint32_t idx)
{
	uint32_t i = 0;

	for (i = ISP_CFAE_NEW_CFG0; i < ISP_CFAE_CSS_CFG11; i += 4)
		ISP_REG_WR(idx, i, 0);
	for (i = ISP_NLM_PARA; i < ISP_NLM_RADIAL_1D_ADDBACK23; i += 4)
		ISP_REG_WR(idx, i, 0);
	for (i = ISP_HSV_PARAM; i < ISP_HSV_CFG14; i += 4)
		ISP_REG_WR(idx, i, 0);
	for (i = ISP_PRECDN_PARAM; i < ISP_PRECDN_DISTW3; i += 4)
		ISP_REG_WR(idx, i, 0);
	for (i = ISP_YNR_CONTRL0; i < ISP_YNR_CFG17; i += 4)
		ISP_REG_WR(idx, i, 0);
	for (i = ISP_CDN_PARAM; i < ISP_CDN_V_RANWEI_7; i += 4)
		ISP_REG_WR(idx, i, 0);
	for (i = ISP_EE_PARAM; i < ISP_EE_LUM_CFG18; i += 4)
		ISP_REG_WR(idx, i, 0);
	for (i = ISP_POSTCDN_COMMON_CTRL;
		i < ISP_POSTCDN_START_ROW_MOD4; i += 4)
		ISP_REG_WR(idx, i, 0);
	for (i = ISP_IIRCNR_PARAM; i < ISP_YUV_IIRCNR_NEW_39; i += 4)
		ISP_REG_WR(idx, i, 0);
	for (i = (ISP_SCALER_PRE_CAP_BASE + ISP_SCALER_CFG);
		i < (ISP_SCALER_PRE_CAP_BASE + ISP_SCALER_RES); i += 4)
		ISP_REG_WR(idx, i, 0);
	for (i = (ISP_SCALER_VID_BASE + ISP_SCALER_CFG);
		i < (ISP_SCALER_VID_BASE + ISP_SCALER_REGULAR_CFG); i += 4)
		ISP_REG_WR(idx, i, 0);
}

static int32_t sprd_ispdrv_block_buf_alloc(struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;
	uint32_t buf_len = 0;

	buf_len = ISP_FRGB_GAMMA_BUF_SIZE;
	isp_k_param->full_gamma_buf_addr = vzalloc(buf_len);
	if (isp_k_param->full_gamma_buf_addr == NULL) {
		ret = -1;
		pr_err("fail to get valid memory.\n");
	}

	buf_len = ISP_LSC_2D_BUF_SIZE;
	isp_k_param->lsc_2d_weight_addr = vzalloc(buf_len);
	if (isp_k_param->lsc_2d_weight_addr == NULL) {
		ret = -1;
		pr_err("fail to get valid memory.\n");
	}

	buf_len = ISP_NLM_BUF_SIZE;
	isp_k_param->nlm_vst_addr = vzalloc(buf_len);
	if (isp_k_param->nlm_vst_addr == NULL) {
		ret = -1;
		pr_err("fail to get valid memory.\n");
	}

	isp_k_param->nlm_ivst_addr = vzalloc(buf_len);
	if (isp_k_param->nlm_ivst_addr == NULL) {
		ret = -1;
		pr_err("fail to get valid memory.\n");
	}

	return ret;
}

static int32_t sprd_ispdrv_block_buf_free(struct isp_k_block *isp_k_param)
{
	if (isp_k_param->full_gamma_buf_addr != NULL) {
		vfree(isp_k_param->full_gamma_buf_addr);
		isp_k_param->full_gamma_buf_addr = NULL;
	}

	if (isp_k_param->lsc_2d_weight_addr != NULL) {
		vfree(isp_k_param->lsc_2d_weight_addr);
		isp_k_param->lsc_2d_weight_addr = NULL;
	}

	if (isp_k_param->nlm_vst_addr != NULL) {
		vfree(isp_k_param->nlm_vst_addr);
		isp_k_param->nlm_vst_addr = NULL;
	}

	if (isp_k_param->nlm_ivst_addr != NULL) {
		vfree(isp_k_param->nlm_ivst_addr);
		isp_k_param->nlm_ivst_addr = NULL;
	}

	return 0;
}

static int sprd_ispdrv_k_buffer_alloc(void *isp_handle)
{
	uint32_t work_mode = 0;
	enum isp_drv_rtn ret = ISP_RTN_SUCCESS;
	struct isp_pipe_dev *dev = NULL;
	struct isp_k_block *isp_k_param = NULL;
	struct isp_cfg_ctx_desc *cfg_ctx = NULL;

	dev = (struct isp_pipe_dev *)isp_handle;
	isp_k_param = &dev->isp_k_param;
	cfg_ctx = &dev->module_info.isp_cfg_contex;
	work_mode = ISP_GET_MODE_ID(dev->com_idx);

	ret = sprd_ispdrv_block_buf_alloc(isp_k_param);
	if (ret != 0)
		sprd_ispdrv_block_buf_free(isp_k_param);

	if (work_mode == ISP_CFG_MODE) {
		ret = sprd_isp_cfg_ctx_buf_init(cfg_ctx, 0);
		if (ret) {
			pr_err("fail to config CFG.\n");
			return ret;
		}
	}

	return ret;
}

static int sprd_ispdrv_clk_en(void)
{
	int ret = 0;
	uint32_t flag = 0;

	/*set isp clock to max value*/
	ret = clk_set_parent(isp_clk, isp_clk_parent);
	if (ret) {
		pr_err("fail to set isp_clk_parent.\n");
		clk_set_parent(isp_clk, isp_clk_default);
		clk_disable_unprepare(isp_clk);
		goto exit;
	}

	ret = clk_prepare_enable(isp_clk);
	if (ret) {
		pr_err("fail to enable isp_clk.\n");
		clk_set_parent(isp_clk, isp_clk_default);
		clk_disable_unprepare(isp_clk);
		goto exit;
	}

	/*isp enable*/
	ret = clk_prepare_enable(isp_eb);
	if (ret) {
		pr_err("fail to enable isp_eb.\n");
		goto exit;
	}

	ret = clk_prepare_enable(isp_axi_eb);
	if (ret) {
		pr_err("fail to enable isp_axi_eb.\n");
		goto exit;
	}

	flag = BIT_MM_AHB_ISP_EB;
	regmap_update_bits(cam_ahb_gpr,
		REG_MM_AHB_AHB_EB, flag, flag);

exit:

	return ret;
}

static int sprd_ispdrv_clk_dis(void)
{
	uint32_t flag = 0;

	clk_disable_unprepare(isp_eb);
	clk_disable_unprepare(isp_axi_eb);

	/* set isp clock to default value before power off */
	clk_set_parent(isp_clk, isp_clk_default);
	clk_disable_unprepare(isp_clk);
	flag = BIT_MM_AHB_ISP_EB;
	regmap_update_bits(cam_ahb_gpr,
		REG_MM_AHB_AHB_EB, flag, ~flag);

	return 0;
}

static int sprd_ispdrv_module_init(struct isp_module *module_info)
{
	int i = 0, ret = 0;

	if (!module_info) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	/* cfg context struct init */
	ret = sprd_isp_cfg_ctx_init(&module_info->isp_cfg_contex,
		module_info->com_idx);
	if (unlikely(ret != 0)) {
		pr_err("fail to init cfg context.\n");
		ret = -EIO;
		return ret;
	}

	ret = sprd_cam_queue_frm_init(
		&module_info->bin_frm_queue,
		sizeof(struct camera_frame),
		DCAM_FRM_QUEUE_LENGTH + 1,
		"binning frm queue");
	ret = sprd_cam_queue_frm_init(
		&module_info->bin_zsl_queue,
		sizeof(struct camera_frame),
		DCAM_FRM_QUEUE_LENGTH + 1,
		"binning zsl queue");
	ret = sprd_cam_queue_buf_init(
		&module_info->bin_buf_queue,
		sizeof(struct camera_frame),
		DCAM_FRM_QUEUE_LENGTH + 1,
		"binning buf queue");
	ret = sprd_cam_queue_frm_init(
		&module_info->full_zsl_queue,
		sizeof(struct camera_frame),
		DCAM_FRM_QUEUE_LENGTH + 1,
		"zsl frm queue");

	if (ret) {
		pr_err("fail to init frm queue.\n");
		return ret;
	}

	for (i = ISP_SCL_PRE; i <= ISP_SCL_CAP; i++) {
		/* isp store output buffer */
		ret = sprd_cam_queue_buf_init(
			&module_info->isp_path[i].buf_queue,
			sizeof(struct camera_frame),
			DCAM_BUF_QUEUE_LENGTH,
			"isp path buffer queue");
		ret = sprd_cam_queue_frm_init(
			&module_info->isp_path[i].frame_queue,
			sizeof(struct camera_frame),
			DCAM_FRM_QUEUE_LENGTH + 1,
			"isp path frm queue");

		if (ret) {
			pr_err("fail to init buf queue.\n");
			return ret;
		}

		ret = sprd_cam_queue_buf_init(
			&module_info->isp_path[i].coeff_queue,
			sizeof(struct isp_zoom_param),
			ISP_SC_COEFF_BUF_COUNT,
			"isp path coeff queue");

		if (ret) {
			pr_err("fail to init coeff queue.\n");
			return ret;
		}
	}

	sprd_cam_queue_frm_clear(&module_info->bin_frm_queue);
	sprd_cam_queue_frm_clear(&module_info->bin_zsl_queue);
	sprd_cam_queue_frm_clear(&module_info->full_zsl_queue);

	init_completion(&module_info->scale_coeff_mem_com);
	complete(&module_info->scale_coeff_mem_com);

	return ret;
}

static int sprd_ispdrv_module_deinit(struct isp_module *module_info)
{
	int i = 0, ret = 0;

	if (!module_info) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	for (i = ISP_SCL_PRE; i <= ISP_SCL_CAP; i++) {
		sprd_cam_queue_buf_deinit(
			&module_info->isp_path[i].buf_queue);
		sprd_cam_queue_frm_deinit(
			&module_info->isp_path[i].frame_queue);
		sprd_cam_queue_buf_deinit(
			&module_info->isp_path[i].coeff_queue);
	}

	sprd_cam_queue_frm_clear(&module_info->bin_zsl_queue);
	sprd_cam_queue_frm_clear(&module_info->bin_frm_queue);
	sprd_cam_queue_frm_clear(&module_info->full_zsl_queue);

	sprd_cam_queue_frm_deinit(&module_info->bin_zsl_queue);
	sprd_cam_queue_frm_deinit(&module_info->bin_frm_queue);
	sprd_cam_queue_buf_deinit(&module_info->bin_buf_queue);
	sprd_cam_queue_frm_deinit(&module_info->full_zsl_queue);

	return ret;
}

static void sprd_ispdrv_update_offline_pipe(
	struct zoom_info_t *io_zoom_info, int zoom_mode)
{
	struct zoom_info_t *zoom_info = io_zoom_info;

	if (zoom_mode == ZOOM_BINNING) {
		zoom_info->base_width = zoom_info->base_width / 2;
		zoom_info->base_height = zoom_info->base_height / 2;
		zoom_info->crop_startx = zoom_info->crop_startx / 2;
		zoom_info->crop_starty = zoom_info->crop_starty / 2;
		zoom_info->crop_width = zoom_info->crop_width / 2;
		zoom_info->crop_height = zoom_info->crop_height / 2;
	}
}

static void sprd_ispdrv_clac_nlm_zoom_param(uint32_t idx, int new_width,
	int old_width, int crop_start_x, int crop_start_y,
	int crop_end_x, int crop_end_y)
{
	int i = 0, j = 0, center_x = 0, center_y = 0, radius_threshold = 0;
	uint32_t val = 0;
	unsigned short coef2[3][FREQ_NUM];
	int flat_thresh_coef[3][3];
	int radius_threshold_filter_ratio[3][4];
	int scl_ratio = 0;

	scl_ratio = FIX_DIV(new_width, old_width);
	val = ISP_WORK_REG_CFG_RD(idx, ISP_NLM_RADIAL_1D_DIST);
	center_x = (int)(val & 0x3FFF);
	center_y = (int)((val >> 16) & 0x3FFF);

	if ((center_x < crop_start_x) ||
		(center_y < crop_start_y) ||
		(center_x > crop_end_x) ||
		(center_y > crop_end_y)) {
		pr_debug("fail to check param center{%d,%d},crop{%d,%d,%d,%d}\n",
			center_x, center_y, crop_start_x, crop_start_y,
			crop_end_x, crop_end_y);
		return;
	}

	center_x -= crop_start_x;
	center_y -= crop_start_y;

	center_x *= scl_ratio;
	center_x = FIX_UNCAST(center_x);

	center_y *= scl_ratio;
	center_y = FIX_UNCAST(center_y);
	val = ((center_y & 0x3FFF) << 16) |
		(center_x & 0x3FFF);
	ISP_WORK_REG_WR(idx, ISP_NLM_RADIAL_1D_DIST, val);

	val = ISP_WORK_REG_CFG_RD(idx, ISP_NLM_RADIAL_1D_THRESHOLD);

	radius_threshold = (int)(val & 0x7FFF);
	radius_threshold *= scl_ratio;
	radius_threshold = FIX_UNCAST(radius_threshold);
	ISP_WORK_REG_MWR(idx, ISP_NLM_RADIAL_1D_THRESHOLD,
		0x7FFF, radius_threshold);

	for (i = 0; i < 3; i++) {
		for (j = 0; j < FREQ_NUM; j++) {
			val = ISP_WORK_REG_CFG_RD(idx,
				ISP_NLM_RADIAL_1D_ADDBACK00 + i*16 + j*4);

			radius_threshold_filter_ratio[i][j] =
				(int)((val >> 17) & 0x7FFF);
			radius_threshold_filter_ratio[i][j] *= scl_ratio;
			radius_threshold_filter_ratio[i][j] =
				FIX_UNCAST(radius_threshold_filter_ratio[i][j]);

			ISP_WORK_REG_MWR(idx,
				ISP_NLM_RADIAL_1D_ADDBACK00 + i*16 + j*4,
				0xFFFE0000,
				radius_threshold_filter_ratio[i][j] << 17);

			val = ISP_WORK_REG_CFG_RD(idx,
				ISP_NLM_RADIAL_1D_RATIO + i*16 + j*4);
			coef2[i][j] = (val << 16) & 0xFFFF;
			coef2[i][j] =  FIX_DIV(coef2[i][j], scl_ratio);
			ISP_WORK_REG_MWR(idx,
				ISP_NLM_RADIAL_1D_RATIO + i*16 + j*4,
				0xFFFF0000, coef2[i][j] << 16);
		}

		for (j = 0; j < FREQ_PARAM_NUM; j++) {
			val = ISP_WORK_REG_CFG_RD(idx,
				ISP_NLM_RADIAL_1D_THR0 + i*12 + j*4);

			flat_thresh_coef[i][j] = val & 0x7FFF;
			flat_thresh_coef[i][j] =
				FIX_DIV(flat_thresh_coef[i][j], scl_ratio);
			ISP_WORK_REG_MWR(idx,
				ISP_NLM_RADIAL_1D_THR0 + i*12 + j*4,
				0x7FFF, flat_thresh_coef[i][j]);
		}
	}
}

static void sprd_ispdrv_update_nlm_zoom_param(uint32_t idx,
	const struct zoom_info_t *zoom_info)
{
	int new_width = zoom_info->cur_width;
	int crop_width = zoom_info->crop_width;
	int crop_height = zoom_info->crop_height;
	int crop_start_x = zoom_info->crop_startx;
	int crop_start_y = zoom_info->crop_starty;
	int crop_end_x = crop_start_x + crop_width - 1;
	int crop_end_y = crop_start_y + crop_height - 1;

	sprd_ispdrv_clac_nlm_zoom_param(idx, new_width, crop_width,
		crop_start_x, crop_start_y, crop_end_x, crop_end_y);
}

static void sprd_ispdrv_calc_3dnr_zoom_param(uint32_t idx,
	int new_width, int old_width)
{
	int r1_circle = 0, r2_circle = 0, r3_circle = 0;
	int scl_ratio = 0;
	uint32_t val = 0;

	scl_ratio = FIX_DIV(new_width, old_width);

	val = ISP_WORK_REG_CFG_RD(idx, ISP_YUV_3DNR_CFG23);

	r1_circle = (int)val & 0xFFF;
	r1_circle *= scl_ratio;
	r1_circle = FIX_UNCAST(r1_circle);
	ISP_WORK_REG_MWR(idx, ISP_YUV_3DNR_CFG23, 0xFFF, r1_circle);

	val = ISP_WORK_REG_CFG_RD(idx, ISP_YUV_3DNR_CFG24);

	r2_circle = (int)((val >> 16) & 0xFFF);
	r2_circle *= scl_ratio;
	r2_circle = FIX_UNCAST(r2_circle);

	r3_circle = (int)(val & 0xFFF);
	r3_circle *= scl_ratio;
	r3_circle = FIX_UNCAST(r3_circle);
	val = ((r2_circle & 0xFFF) << 16)
		| (r3_circle & 0xFFF);
	ISP_WORK_REG_WR(idx, ISP_YUV_3DNR_CFG24, val);
}

static void sprd_ispdrv_update_3dnr_zoom_param(uint32_t idx,
	const struct zoom_info_t *zoom_info)
{
	int new_width = zoom_info->cur_width;
	int crop_width = zoom_info->crop_width;

	sprd_ispdrv_calc_3dnr_zoom_param(idx, new_width, crop_width);
}

static void sprd_ispdrv_calc_ynr_zoom_param(uint32_t idx, int new_width,
	int old_width, int crop_start_x, int crop_start_y,
	int crop_end_x, int crop_end_y)
{
	int center_x = 0, center_y = 0, Radius = 0;
	int maxRadius = 0, dist_interval = 0;
	int scl_ratio = 0;
	uint32_t val = 0;

	scl_ratio = FIX_DIV(new_width, old_width);
	val = ISP_WORK_REG_CFG_RD(idx, ISP_YNR_CFG12);
	center_x = (val >> 16) & 0xFFFF;
	center_y = val & 0xFFFF;
	if ((center_x < crop_start_x) ||
		(center_y < crop_start_y) ||
		(center_x > crop_end_x) ||
		(center_y > crop_end_y)) {
		pr_debug("fail to check param center{%d,%d},crop{%d,%d,%d,%d}\n",
			center_x, center_y, crop_start_x, crop_start_y,
			crop_end_x, crop_end_y);
		return;
	}
	center_x -= crop_start_x;
	center_y -= crop_start_y;

	center_x *= scl_ratio;
	center_x = FIX_UNCAST(center_x);

	center_y *= scl_ratio;
	center_y = FIX_UNCAST(center_y);

	val = (center_y & 0xFFFF)
		| ((center_x & 0xFFFF) << 16);
	ISP_WORK_REG_WR(idx, ISP_YNR_CFG12, val);

	val = ISP_WORK_REG_CFG_RD(idx, ISP_YNR_CFG13);

	Radius = (int)((val >> 16) & 0xFFFF);
	Radius *= scl_ratio;
	Radius = FIX_UNCAST(Radius);

	dist_interval = (int)(val & 0xFFFF);
	maxRadius = (dist_interval + Radius) << 2;
	maxRadius *= scl_ratio;
	maxRadius = FIX_UNCAST(maxRadius);

	val = (((maxRadius - Radius) >> 2) & 0xFFFF)
		| ((Radius & 0xFFFF) << 16);
	ISP_REG_WR(idx, ISP_YNR_CFG13, val);
}

static void sprd_ispdrv_update_ynr_zoom_param(uint32_t idx,
	const struct zoom_info_t *zoom_info)
{
	int new_width = zoom_info->cur_width;
	int crop_width = zoom_info->crop_width;
	int crop_height = zoom_info->crop_height;
	int crop_start_x = zoom_info->crop_startx;
	int crop_start_y = zoom_info->crop_starty;
	int crop_end_x = crop_start_x + crop_width - 1;
	int crop_end_y = crop_start_y + crop_height - 1;

	sprd_ispdrv_calc_ynr_zoom_param(idx, new_width, crop_width,
		crop_start_x, crop_start_y, crop_end_x, crop_end_y);

}

static int sprd_ispdrv_update_rds_param(uint32_t idx,
	struct zoom_info_t *zoom_info)
{
	sprd_ispdrv_update_offline_pipe(zoom_info, zoom_info->zoom_mode);
	sprd_ispdrv_update_nlm_zoom_param(idx, zoom_info);
	sprd_ispdrv_update_3dnr_zoom_param(idx, zoom_info);
	sprd_ispdrv_update_ynr_zoom_param(idx, zoom_info);

	return ISP_RTN_SUCCESS;
}

static int sprd_ispdrv_pre_frame_pre_proc(struct isp_pipe_dev *dev)
{
	uint32_t rtn = ISP_RTN_SUCCESS;
	struct isp_module *module = NULL;
	struct camera_frame frame;

	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	module = &dev->module_info;
	memset((void *)&frame, 0x00, sizeof(frame));
	if (s_isp_group.dual_cam &&
		sprd_cam_queue_frm_cur_nodes(&module->bin_frm_queue)
		>= 1) {

		while (sprd_cam_queue_buf_cur_nodes(&module->bin_buf_queue)
			> 1) {
			rtn = sprd_cam_queue_buf_read(&module->bin_buf_queue,
					&frame);
			if (!rtn)
				sprd_cam_ioctl_addr_write_back(
					dev->isp_handle_addr,
					CAMERA_BIN_PATH, &frame);
		}
		return -EINVAL;
	}

	if (!s_isp_group.dual_cam && atomic_read(&dev->shadow_done) > 0) {
		/*skip convert next prev frame when current is in progress*/
		dev->pre_flag++;
		return -EINVAL;
	}

	if (sprd_cam_queue_buf_read(&module->bin_buf_queue, &frame))
		return -EINVAL;

	frame.buf_info.dev = &s_isp_pdev->dev;
	rtn = sprd_cam_buf_addr_map(&frame.buf_info);
	if (rtn) {
		pr_err("fail to map dcam bin_path addr\n");
		return rtn;
	}

	if (module->isp_path[ISP_SCL_PRE].valid
		&& module->isp_path[ISP_SCL_VID].valid) {
		if (module->isp_path[ISP_SCL_PRE].buf_cnt == 0)
			atomic_set(&frame.usr_cnt, 2);
		else
			atomic_set(&frame.usr_cnt, 1);
		module->isp_path[ISP_SCL_PRE].buf_cnt++;
		if (module->isp_path[ISP_SCL_PRE].buf_cnt >
			module->isp_path[ISP_SCL_PRE].frm_deci)
			module->isp_path[ISP_SCL_PRE].buf_cnt = 0;
	} else
		atomic_set(&frame.usr_cnt, 1);

	if (sprd_cam_queue_frm_enqueue(
		&module->bin_frm_queue, &frame)) {
		pr_err("fail to enqueue preview frame\n");
		return -EFAULT;
	}
	memcpy(&dev->offline_frame[ISP_SCENE_PRE], &frame, sizeof(frame));

	return rtn;
}

static void sprd_ispdrv_start(uint32_t idx,
	enum isp_id id, enum isp_scene_id scene_id)
{
	ISP_HREG_MWR(id, ISP_CFG_PAMATER, BIT_0, 1);
	ISP_REG_WR(idx, ISP_FETCH_START, 1);
}

static int sprd_ispdrv_offline_proc(void *handle)
{
	uint32_t idx = 0, work_mode = 0;
	unsigned long flag = 0;
	enum isp_id id = ISP_ID_0;
	enum isp_drv_rtn ret = ISP_RTN_SUCCESS;
	enum isp_scene_id scene_id = ISP_SCENE_PRE;
	struct isp_pipe_dev *dev = NULL;
	struct isp_module *module = NULL;
	struct isp_cfg_ctx_desc *cfg_ctx = NULL;
	uint32_t cur_status = 0;
	uint32_t cap_state_clr = 0;
	uint32_t cap_ready = 0;
	struct zoom_info_t zoom_info;

	if (!handle) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	memset((void *)&zoom_info, 0x00, sizeof(zoom_info));
	dev = (struct isp_pipe_dev *)handle;
	mutex_lock(&dev->offline_thread_mutex);
	if (dev->isp_offline_thread_flag == 1) {
		pr_info("isp_offline_thread_flag %d.\n",
			dev->isp_offline_thread_flag);
		goto exit;
	}

	module = &dev->module_info;
	cfg_ctx = &module->isp_cfg_contex;
	idx = dev->com_idx;
	id = ISP_GET_ISP_ID(idx);
	work_mode = ISP_GET_MODE_ID(idx);

	/* preview frame proc */
	if ((module->isp_path[ISP_SCL_PRE].valid ||
		module->isp_path[ISP_SCL_VID].valid) &&
		!dev->isp_busy) {
		if (sprd_ispdrv_pre_frame_pre_proc(dev))
			goto capture_proc;

		scene_id = ISP_SCENE_PRE;
		cfg_ctx->cur_scene_id = scene_id;
		ISP_SET_SCENE_ID(idx, scene_id);
		module->com_idx = idx;
		ret = sprd_ispdrv_path_start(handle, ISP_PATH_IDX_PRE |
			ISP_PATH_IDX_VID, &dev->offline_frame[ISP_SCENE_PRE]);

		if (work_mode == ISP_CFG_MODE) {
			zoom_info = dev->offline_frame[ISP_SCENE_PRE].zoom_info;
			ret = sprd_isp_cfg_buf_update(cfg_ctx);
			if (module->need_downsizer)
				ret = sprd_ispdrv_update_rds_param(idx,
					&zoom_info);
			ret = sprd_isp_cfg_block_config(cfg_ctx);
			pr_info("start isp%d in CFG mode, scene_idx 0x%x\n",
					id, scene_id);
			atomic_set(&dev->shadow_done, 1);
			sprd_isp_cfg_isp_start(cfg_ctx);
		} else {
			pr_info("start isp%d in AP mode, scene_idx 0x%x\n",
			id, scene_id);
			sprd_ispdrv_start(idx, id, scene_id);
			dev->isp_busy = 1;
		}
	}

capture_proc:
	spin_lock_irqsave(&isp_mod_lock, flag);
	cur_status = s_isp_group.dual_cap_sts;
	if (dev->offline_proc_cap == 1) {
		if (cur_status == 0) {
			s_isp_group.dual_cap_sts = 1;
			cap_ready = 1;
			dev->offline_proc_cap = 0;
		} else if (s_isp_group.dual_cam &&
				s_isp_group.dual_cap_cnt == 0) {
			pr_info("wait dual first frame done.\n");
			s_isp_group.first_need_wait = 1;
		}
	}
	spin_unlock_irqrestore(&isp_mod_lock, flag);

	if (cap_ready) {
		ret = sprd_ispdrv_sel_cap_frame(handle);
		if (ret) {
			if (ret == -ENOENT) {
				cap_state_clr = 1;
				ret = 0;
			} else
				pr_err("fail to sprd_ispdrv_sel_cap_frame\n");
			goto cap_exit;
		}

		scene_id = ISP_SCENE_CAP;
		cfg_ctx->cur_scene_id = scene_id;
		ISP_SET_SCENE_ID(idx, scene_id);
		module->com_idx = idx;

		ret = sprd_ispdrv_path_start(dev, ISP_PATH_IDX_CAP,
			&dev->offline_frame[ISP_SCENE_CAP]);
		if (ret) {
			ret = ISP_RTN_PARA_ERR;
			pr_err("fail to sprd_ispdrv_path_start\n");
			goto cap_exit;
		}

		ret = sprd_ispdrv_fmcu_pre_proc(dev);
		if (ret) {
			pr_err("fail to get capture frame\n");
			goto cap_exit;
		}

		if (work_mode == ISP_CFG_MODE) {
			ret = sprd_isp_cfg_buf_update(cfg_ctx);
			ret = sprd_isp_cfg_block_config(cfg_ctx);
		} else
			ISP_HREG_MWR(id, ISP_CFG_PAMATER, BIT_0, 1);

		ISP_HREG_WR(id, ISP_FMCU_START, 1);
		pr_info("isp%d fmcu start\n", id);

cap_exit:
		if (ret || cap_state_clr) {
			spin_lock_irqsave(&isp_mod_lock, flag);
			s_isp_group.dual_cap_sts = 0;
			spin_unlock_irqrestore(&isp_mod_lock, flag);
		}
	}

exit:
	mutex_unlock(&dev->offline_thread_mutex);

	return ret;
}

static int sprd_ispdrv_offline_thread_loop(void *arg)
{
	struct isp_pipe_dev *dev = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}

	dev = (struct isp_pipe_dev *)arg;
	while (1) {
		if (wait_for_completion_interruptible(
			&dev->offline_thread_com) == 0) {
			if (dev->is_offline_thread_stop)
				break;

			if (sprd_ispdrv_offline_proc(arg))
				pr_err("fail to start slice capture\n");

		} else {
			break;
		}
	}

	dev->is_offline_thread_stop = 0;

	return 0;
}

static int sprd_ispdrv_offline_thread_create(void *param)
{
	char thread_name[20] = { 0 };
	enum isp_id id = ISP_ID_0;
	struct isp_pipe_dev *dev = (struct isp_pipe_dev *)param;

	if (dev == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}
	id = ISP_GET_ISP_ID(dev->com_idx);

	dev->is_offline_thread_stop = 0;
	init_completion(&dev->offline_thread_com);
	sprintf(thread_name, "isp%d_offline_thread", id);
	dev->offline_thread = kthread_run(sprd_ispdrv_offline_thread_loop,
					param, thread_name);
	if (IS_ERR(dev->offline_thread)) {
		pr_err("fail to create_offline_thread %ld\n",
				PTR_ERR(dev->offline_thread));
		dev->offline_thread = NULL;
		return -1;
	}

	return 0;
}

static int sprd_ispdrv_offline_thread_stop(void *param)
{
	struct isp_pipe_dev *dev = (struct isp_pipe_dev *)param;

	if (dev == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}

	if (dev->offline_thread) {
		dev->is_offline_thread_stop = 1;
		complete(&dev->offline_thread_com);
		if (dev->is_offline_thread_stop != 0) {
			while (dev->is_offline_thread_stop)
				udelay(1000);
		}
		dev->offline_thread = NULL;
	}

	return 0;
}

struct isp_group *sprd_isp_drv_group_get(void)
{
	return &s_isp_group;
}

int sprd_isp_drv_module_en(void *isp_handle)
{
	int ret = 0;
	struct isp_pipe_dev *dev = NULL;
	enum isp_id id = 0;

	if (!isp_handle) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	id = ISP_GET_ISP_ID(dev->com_idx);

	mutex_lock(&isp_module_sema[id]);
	if (atomic_inc_return(&s_isp_users[id]) == 1) {
		if (atomic_read(&s_isp_total_users) == 0) {
			ret = sprd_cam_pw_on();
			if (ret != 0) {
				pr_err("fail to power on sprd cam\n");
				mutex_unlock(&isp_module_sema[id]);
				return ret;
			}

			sprd_cam_domain_eb();
			sprd_ispdrv_clk_en();
			sprd_ispdrv_reset(dev->com_idx);
			sprd_ispdrv_irq_mask_dis(dev->com_idx);
			sprd_ispdrv_irq_clear(dev->com_idx);
			sprd_ispdrv_irq_mask_en(dev->com_idx);
		}

		ret = sprd_isp_int_irq_request(&s_isp_pdev->dev,
			&s_isp_irq[id], dev);
		if (ret) {
			pr_err("fail to install isp IRQ %d\n", ret);
			mutex_unlock(&isp_module_sema[id]);
			return ret;
		}

		ret = sprd_ispdrv_k_buffer_alloc(dev);
		if (ret) {
			pr_err("fail to alloc buffer\n");
			mutex_unlock(&isp_module_sema[id]);
			return ret;
		}

		sprd_ispdrv_default_regvalue_set(dev->com_idx);

	}
	atomic_inc(&s_isp_total_users);
	mutex_unlock(&isp_module_sema[id]);

	return ret;
}

int sprd_isp_drv_module_dis(void *isp_handle)
{
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	struct isp_pipe_dev *dev = NULL;
	struct isp_module *module = NULL;
	enum isp_id id = 0;

	if (!isp_handle) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->module_info;
	id = ISP_GET_ISP_ID(dev->com_idx);

	if (atomic_read(&s_isp_users[id]) == 0)
		return rtn;

	mutex_lock(&isp_module_sema[id]);
	if (atomic_dec_return(&s_isp_users[id]) == 0) {
		if (atomic_read(&s_isp_total_users) == 1) {
			sprd_ispdrv_clk_dis();
			sprd_cam_domain_disable();
			rtn = sprd_cam_pw_off();
			if (rtn != 0) {
				pr_err("fail to power off sprd cam\n");
				mutex_unlock(&isp_module_sema[id]);
				return rtn;
			}
		}

		rtn = sprd_isp_int_irq_free(&s_isp_irq[id], dev);
		if (rtn)
			pr_err("fail to free isp IRQ %d\n", rtn);
	}
	atomic_dec(&s_isp_total_users);
	mutex_unlock(&isp_module_sema[id]);

	return rtn;
}

int sprd_isp_drv_dev_init(void **isp_pipe_dev_handle, enum isp_id id)
{
	int ret = 0;
	struct isp_pipe_dev *dev = NULL;

	if (!isp_pipe_dev_handle) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	dev = vzalloc(sizeof(*dev));
	if (dev == NULL)
		return -ENOMEM;

	dev->com_idx = 0;
	if (ISP_CFG_ON != 1)
		ISP_SET_MODE_ID(dev->com_idx, ISP_AP_MODE);
	ISP_SET_ISP_ID(dev->com_idx, id);
	dev->is_raw_capture = 0;
	mutex_init(&dev->offline_thread_mutex);

	dev->module_info.com_idx = dev->com_idx;
	ret = sprd_ispdrv_module_init(&dev->module_info);
	if (unlikely(ret != 0)) {
		pr_err("fail to init isp module info\n");
		ret = -EIO;
		goto exit;
	}

	ret = sprd_isp_slice_fmcu_init(&dev->fmcu_slice.slice_handle);
	if (unlikely(ret != 0)) {
		pr_err("fail to init fmcu slice\n");
		ret = -EIO;
		goto fmcu_slice_exit;
	}
	ret = sprd_ispdrv_offline_thread_create(dev);
	if (unlikely(ret != 0)) {
		pr_err("fail to create offline thread\n");
		sprd_ispdrv_offline_thread_stop(dev);
		ret = -EINVAL;
		goto offline_thread_exit;
	}

	*isp_pipe_dev_handle = (void *)dev;
	dev->isp_handle_addr = isp_pipe_dev_handle;
	if (!s_isp_group.isp_dev[id])
		s_isp_group.isp_dev[id] = dev;
	else
		s_isp_group.isp_dev[id + 1] = dev;
	s_isp_group.dual_cap_sts = 0;
	s_isp_group.dual_fullpath_stop = 0;
	s_isp_group.first_need_wait = 0;
	s_isp_group.dual_cap_cnt = 0;
	s_isp_group.dual_sel_cnt = 0;
	init_completion(&s_isp_group.first_wait_com);

	return ret;
offline_thread_exit:
	sprd_isp_slice_fmcu_deinit(dev->fmcu_slice.slice_handle);
fmcu_slice_exit:
	sprd_ispdrv_module_deinit(&dev->module_info);
exit:
	vfree(dev);
	pr_err("fail to init dev[%d]!\n", id);

	return ret;
}

int sprd_isp_drv_dev_deinit(void *isp_pipe_dev_handle)
{
	int ret = 0;
	enum isp_work_mode work_mode = ISP_CFG_MODE;
	struct isp_pipe_dev *dev = NULL;
	struct isp_k_block *isp_k_param = NULL;
	uint32_t idx = 0;

	if (!isp_pipe_dev_handle) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	dev = (struct isp_pipe_dev *)isp_pipe_dev_handle;
	isp_k_param = &dev->isp_k_param;
	work_mode = ISP_GET_MODE_ID(dev->com_idx);
	idx = ISP_GET_ISP_ID(dev->com_idx);

	ret = sprd_ispdrv_module_deinit(&dev->module_info);
	if (unlikely(ret != 0))
		pr_err("fail to init queue\n");

	ret = sprd_isp_slice_fmcu_deinit(dev->fmcu_slice.slice_handle);
	if (unlikely(ret != 0))
		pr_err("fail to deinit fmcu slice\n");

	ret = sprd_ispdrv_offline_thread_stop(dev);
	if (unlikely(ret != 0))
		pr_err("fail to stop offline thread\n");

	mutex_destroy(&dev->offline_thread_mutex);

	sprd_ispdrv_block_buf_free(isp_k_param);

	if (s_isp_group.isp_dev[idx + 1] == dev)
		s_isp_group.isp_dev[idx + 1] = NULL;
	else
		s_isp_group.isp_dev[idx] = NULL;
	vfree(dev);

	return ret;
}

int sprd_isp_drv_init(struct platform_device *pdev)
{
	int i = 0;

	for (i = 0; i < s_isp_count; i++) {
		atomic_set(&s_isp_users[i], 0);
		mutex_init(&isp_module_sema[i]);
		isp_glb_reg_axi_lock[i] =
			__SPIN_LOCK_UNLOCKED(&isp_glb_reg_axi_lock[i]);
		isp_glb_reg_mask_lock[i] =
			__SPIN_LOCK_UNLOCKED(&isp_glb_reg_mask_lock[i]);
		isp_glb_reg_clr_lock[i] =
			__SPIN_LOCK_UNLOCKED(&isp_glb_reg_clr_lock[i]);
	}
	isp_mod_lock = __SPIN_LOCK_UNLOCKED(&isp_mod_lock);
	atomic_set(&s_isp_total_users, 0);
	atomic_set(&s_isp_total_runer, 0);

	return 0;
}

void sprd_isp_drv_deinit(void)
{
	int i = 0;

	for (i = 0; i < s_isp_count; i++) {
		atomic_set(&s_isp_users[i], 0);
		s_isp_irq[i].irq0 = 0;
		s_isp_irq[i].irq1 = 0;
		mutex_init(&isp_module_sema[i]);
		isp_glb_reg_axi_lock[i] =
			__SPIN_LOCK_UNLOCKED(&isp_glb_reg_axi_lock[i]);
		isp_glb_reg_mask_lock[i] =
			__SPIN_LOCK_UNLOCKED(&isp_glb_reg_mask_lock[i]);
		isp_glb_reg_clr_lock[i] =
			__SPIN_LOCK_UNLOCKED(&isp_glb_reg_clr_lock[i]);
	}
	isp_mod_lock = __SPIN_LOCK_UNLOCKED(&isp_mod_lock);
	atomic_set(&s_isp_total_users, 0);
	atomic_set(&s_isp_total_runer, 0);
}

int sprd_isp_drv_dt_parse(struct device_node *dn, uint32_t *isp_count)
{
	int i = 0;
	uint32_t count = 0;
	void __iomem *reg_base;
	struct device_node *isp_node = NULL;
	struct resource res = {0};

	isp_node = of_parse_phandle(dn, "sprd,isp", 0);
	if (isp_node == NULL) {
		pr_err("fail to parse the property of sprd,isp\n");
		return -EFAULT;
	}

	s_isp_pdev = of_find_device_by_node(isp_node);
	if (of_device_is_compatible(isp_node, "sprd,isp")) {
		if (of_property_read_u32_index(isp_node,
			"sprd,isp-count", 0, &count)) {
			pr_err("fail to parse the property of sprd,isp-count\n");
			return -EINVAL;
		}
		count++;
		s_isp_count = count;
		*isp_count = count;

#ifdef FPGA_BRIPNUP
		isp_eb = of_clk_get_by_name(isp_node, "isp_eb");
		if (IS_ERR_OR_NULL(isp_eb)) {
			pr_err("fail to get isp_eb\n");
			return PTR_ERR(isp_eb);
		}

		isp_axi_eb = of_clk_get_by_name(isp_node,
			"isp_axi_eb");
		if (IS_ERR_OR_NULL(isp_axi_eb)) {
			pr_err("fail to get isp_axi_eb\n");
			return PTR_ERR(isp_axi_eb);
		}

		isp_clk = of_clk_get_by_name(isp_node, "isp_clk");
		if (IS_ERR_OR_NULL(isp_clk)) {
			pr_err("fail to get isp_clk\n");
			return PTR_ERR(isp_clk);
		}

		isp_clk_parent = of_clk_get_by_name(isp_node,
			"isp_clk_parent");
		if (IS_ERR_OR_NULL(isp_clk_parent)) {
			pr_err("fail to get isp_clk_parent\n");
			return PTR_ERR(isp_clk_parent);
		}

		cam_ahb_gpr = syscon_regmap_lookup_by_phandle(isp_node,
			"sprd,cam-ahb-syscon");
		if (IS_ERR_OR_NULL(cam_ahb_gpr))
			return PTR_ERR(cam_ahb_gpr);

		aon_apb_gpr = syscon_regmap_lookup_by_phandle(isp_node,
			"sprd,aon-apb-syscon");
		if (IS_ERR_OR_NULL(aon_apb_gpr))
			return PTR_ERR(aon_apb_gpr);

		isp_clk_default = clk_get_parent(isp_clk);
		if (IS_ERR_OR_NULL(isp_clk_default)) {
			pr_err("fail to get isp_clk_default\n");
			return PTR_ERR(isp_clk_default);
		}
#endif
		cam_ahb_gpr = syscon_regmap_lookup_by_phandle(isp_node,
			"sprd,cam-ahb-syscon");
		if (IS_ERR_OR_NULL(cam_ahb_gpr))
			return PTR_ERR(cam_ahb_gpr);

		if (of_address_to_resource(isp_node, 0, &res))
			pr_err("fail to get isp phys addr\n");

		isp_phys_base[0] = (unsigned long)res.start;
		reg_base = of_iomap(isp_node, 0);
		if (!reg_base) {
			pr_err("fail to get isp reg_base %d\n", 0);
			return -ENXIO;
		}

		s_isp_regbase[0] = (unsigned long)reg_base;
		s_isp_irq[0].irq0 = irq_of_parse_and_map(isp_node, 0);
		s_isp_irq[0].irq1 = irq_of_parse_and_map(isp_node, 1);
		if (s_isp_irq[0].irq0 <= 0 || s_isp_irq[0].irq1 <= 0) {
			pr_err("fail to get isp irq %d\n", 0);
			return -EFAULT;
		}

		pr_info("ISP dts OK! base %lx, irq0 %d, irq1 %d\n",
			s_isp_regbase[0], s_isp_irq[0].irq0,
			s_isp_irq[0].irq1);

		for (i = 1; i < count; i++) {
			s_isp_regbase[i] = s_isp_regbase[0];
			isp_phys_base[i] = isp_phys_base[0];
			s_isp_irq[i].irq0 = s_isp_irq[0].irq0;
			s_isp_irq[i].irq1 = s_isp_irq[0].irq1;
		}
	} else {
		pr_err("fail to match isp device node\n");
		return -EINVAL;
	}

	return 0;
}

int sprd_isp_drv_start(void *isp_handle)
{
	uint32_t i = 0, work_mode = 0, idx = 0;
	uint32_t size = 0;
	uint32_t wqos_val = 0;
	uint32_t rqos_val = 0;
	uint32_t r_ostd_val = 0;
	enum chip_id id = SHARKL3;
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	struct isp_pipe_dev *dev = NULL;
	struct isp_module *module = NULL;
	struct isp_cfg_ctx_desc *cfg_ctx = NULL;
	struct isp_path_desc *pre = NULL;
	struct isp_path_desc *vid = NULL;
	struct isp_path_desc *cap = NULL;
	struct isp_nr3_param *nr3_dev = NULL;
	struct cam_buf_info *nr3_buf = NULL;
	struct isp_k_block *isp_k_param = NULL;
	struct isp_zoom_param *zoom_4in1 = NULL;

	if (!isp_handle) {
		pr_err("fail to get valid input ptr\n");
		return -ISP_RTN_PARA_ERR;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->module_info;
	cfg_ctx = &module->isp_cfg_contex;
	pre = &module->isp_path[ISP_SCL_PRE];
	vid = &module->isp_path[ISP_SCL_VID];
	cap = &module->isp_path[ISP_SCL_CAP];
	zoom_4in1 = &module->zoom_4in1;
	work_mode = ISP_GET_MODE_ID(dev->com_idx);
	idx = ISP_GET_ISP_ID(dev->com_idx);

	isp_k_param = &dev->isp_k_param;
	isp_k_param->isp_status = ISP_START;

	/* fmcu cmdq buf */
	rtn = sprd_cam_buf_alloc(&dev->fmcu_slice.buf_info, idx,
		&s_isp_pdev->dev, PAGE_SIZE,
		1, CAM_BUF_KERNEL_TYPE);
	if (rtn)
		pr_err("fail to get fmcu buf\n");

	rtn = sprd_cam_buf_addr_map(&dev->fmcu_slice.buf_info);
	if (rtn) {
		pr_err("fail to map fmcu\n");
		return rtn;
	}

	rtn = sprd_isp_path_pre_proc_start(pre, vid, cap);
	if (rtn) {
		pr_err("fail to start isp pre proc\n");
		return -rtn;
	}

	if (dev->need_4in1 && cap->valid) {
		zoom_4in1->in_size.w = cap->src.w;
		zoom_4in1->in_size.h = cap->src.h;
		zoom_4in1->in_rect.w = cap->trim0_info.size_x;
		zoom_4in1->in_rect.h = cap->trim0_info.size_y;
		zoom_4in1->in_rect.x = cap->trim0_info.start_x;
		zoom_4in1->in_rect.y = cap->trim0_info.start_y;
	}

	if (work_mode == ISP_CFG_MODE) {
		/*alloc cfg context buffer*/
		rtn = sprd_cam_buf_alloc(&cfg_ctx->buf_info, idx,
			&s_isp_pdev->dev, ISP_CFG_BUF_SIZE_ALL,
			1, CAM_BUF_KERNEL_TYPE);
		if (rtn) {
			pr_err("fail to get cfg buffer\n");
			return rtn;
		}

		rtn = sprd_isp_buf_cfg_iommu_map(cfg_ctx);
		if (rtn) {
			pr_err("fail to map cmd buffer\n");
			return rtn;
		}

		memcpy(cfg_ctx->cfg_buf[0].cmd_buf[0].vir_addr,
			&cfg_ctx->temp_cfg_buf[0][0], ISP_REG_SIZE);
		memcpy(cfg_ctx->cfg_buf[1].cmd_buf[0].vir_addr,
			&cfg_ctx->temp_cfg_buf[1][0], ISP_REG_SIZE);

		rtn = sprd_isp_cfg_map_init(cfg_ctx);
		if (rtn) {
			pr_err("fail to init CFG map.\n");
			return rtn;
		}

		rtn = sprd_isp_cfg_ctx_buf_init(cfg_ctx, 1);
		if (rtn) {
			pr_err("fail to config CFG.\n");
			return rtn;
		}
	}

	for (i = 0; i < ISP_SCL_MAX ; i++) {
		if (module->isp_path[i].valid) {
			module->isp_path[i].status = ISP_ST_START;
			module->isp_path[i].frm_cnt = 0;
			module->isp_path[i].buf_cnt = 0;
		}
	}

	dev->isp_offline_state = ISP_ST_START;
	dev->cap_cur_cnt = 0;
	dev->isp_offline_thread_flag = 0;
	dev->pre_flag = 0;
	atomic_set(&dev->shadow_done, 0);

	/* 3dnr memory alloc */
	if ((pre->valid && pre->nr3_param.need_3dnr)
		|| (vid->valid && vid->nr3_param.need_3dnr)) {
		nr3_dev = (struct isp_nr3_param *)&pre->nr3_param;
		nr3_buf = (struct cam_buf_info *)&nr3_dev->buf_info;
		size = nr3_dev->prev_size.width *
				nr3_dev->prev_size.height * 3 / 2;
		rtn = sprd_cam_buf_alloc(nr3_buf, idx, &s_isp_pdev->dev,
			size, ISP_NR3_BUFFER_NUM,
			CAM_BUF_KERNEL_TYPE);
		if (rtn) {
			pr_err("fail to 3ndr prev alloc buf\n");
			return rtn;
		}
		rtn = sprd_cam_buf_addr_map(nr3_buf);
		if (rtn) {
			pr_err("fail to 3ndr prev map buf\n");
			return rtn;
		}
	}

	if (cap->valid && cap->nr3_param.need_3dnr) {
		nr3_dev = (struct isp_nr3_param *)&cap->nr3_param;
		nr3_buf = (struct cam_buf_info *)&nr3_dev->buf_info;
		size = nr3_dev->sns_max_size.width *
			nr3_dev->sns_max_size.height * 3 / 2;
		rtn = sprd_cam_buf_alloc(nr3_buf, idx, &s_isp_pdev->dev,
			size, ISP_NR3_BUFFER_NUM,
			CAM_BUF_KERNEL_TYPE);
		if (rtn) {
			pr_err("fail to 3ndr capture alloc buf\n");
			return rtn;
		}
		rtn = sprd_cam_buf_addr_map(nr3_buf);
		if (rtn) {
			pr_err("fail to 3ndr capture map buf\n");
			return rtn;
		}
	}

	r_ostd_val = 1 << 8;
	ISP_HREG_MWR(idx, ISP_AXI_ITI2AXIM_CTRL,
			ISP_AXI_ITI2AXIM_ISP_ROSTD_MASK,
			r_ostd_val);

	id = sprd_dcam_drv_chip_id_get();
	if (id == SHARKL3) {
		wqos_val = (0x1 << 13) | (0x0 << 12) |
				(0x4 << 8) | (0x6 << 4) | 0x6;
		rqos_val = (0x0 << 8) | (0x6 << 4) | 0x6;
	} else {
		wqos_val = (0x0 << 13) | (0x0 << 12) |
				(0x4 << 8) | (0xA << 4) | 0xA;
		rqos_val = (0x0 << 8) | (0xA << 4) | 0x8;
	}

	ISP_HREG_MWR(idx, ISP_AXI_ARBITER_WQOS,
					ISP_AXI_ARBITER_WQOS_MASK,
					wqos_val);
	ISP_HREG_MWR(idx, ISP_AXI_ARBITER_RQOS,
					ISP_AXI_ARBITER_RQOS_MASK,
					rqos_val);
	ISP_HREG_MWR(idx, ISP_PMU_EN, BIT_22, (0x1 << 22));

	atomic_inc(&s_isp_total_runer);

	return rtn;
}

int sprd_isp_drv_stop(void *isp_handle, int is_irq)
{
	uint32_t i = 0;
	unsigned long flag = 0;
	enum isp_work_mode work_mode = 0;
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	struct isp_pipe_dev *dev = NULL;
	struct isp_module *module = NULL;
	struct isp_cfg_ctx_desc *cfg_ctx = NULL;
	struct isp_k_block *isp_k_param = NULL;
	struct camera_frame frame;

	if (isp_handle == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->module_info;
	cfg_ctx = &module->isp_cfg_contex;
	isp_k_param = &dev->isp_k_param;
	work_mode = ISP_GET_MODE_ID(dev->com_idx);
	isp_k_param->isp_status = ISP_STOP;

	if (!is_irq) {
		mutex_lock(&dev->offline_thread_mutex);
		spin_lock_irqsave(&isp_mod_lock, flag);
	}

	if (atomic_read(&s_isp_total_runer) == 1)
		sprd_ispdrv_quickstop(dev->com_idx);

	sprd_ispdrv_irq_mask_dis(dev->com_idx);
	sprd_ispdrv_irq_clear(dev->com_idx);

	for (i = ISP_SCL_PRE; i < ISP_SCL_MAX; i++) {
		module->isp_path[i].status = ISP_ST_STOP;
		module->isp_path[i].valid = 0;
		module->isp_path[i].skip_num = 0;
		module->isp_path[i].frm_cnt = 0;
		module->isp_path[i].buf_cnt = 0;
		sprd_isp_buf_frm_clear(dev, 1 << i);
	}

	module->frm_cnt = 0;
	dev->fmcu_slice.capture_state = ISP_ST_STOP;
	dev->clr_queue = 0;
	dev->cap_cur_cnt = 0;
	dev->cap_flag = DCAM_CAPTURE_STOP;
	dev->isp_offline_thread_flag = 1;
	dev->offline_proc_cap = 0;
	dev->pre_flag = 0;
	atomic_set(&dev->shadow_done, 0);
	s_isp_group.dual_cap_sts = 0;
	s_isp_group.dual_cap_cnt = 0;
	s_isp_group.dual_sel_cnt = 0;

	sprd_isp_3dnr_release(dev);
	while (!sprd_cam_queue_buf_read(&module->bin_buf_queue, &frame))
		sprd_cam_buf_addr_unmap(&frame.buf_info);
	while (!sprd_cam_queue_frm_dequeue(&module->bin_frm_queue, &frame))
		sprd_cam_buf_addr_unmap(&frame.buf_info);

	sprd_cam_queue_frm_clear(&module->full_zsl_queue);
	sprd_cam_queue_buf_clear(&module->bin_buf_queue);
	sprd_cam_queue_frm_clear(&module->bin_frm_queue);
	sprd_cam_queue_frm_clear(&module->bin_zsl_queue);

	sprd_cam_buf_addr_unmap(&dev->fmcu_slice.buf_info);
	if (work_mode == ISP_CFG_MODE) {
		sprd_isp_cfg_buf_reset(cfg_ctx);
		sprd_isp_cfg_ctx_buf_init(cfg_ctx, 0);
		sprd_cam_buf_addr_unmap(&cfg_ctx->buf_info);
	}
	sprd_cam_statistic_queue_clear(
			&module->statis_module_info);
	sprd_cam_statistic_queue_deinit(
		&module->statis_module_info);

	if (isp_k_param->lsc_buf_phys_addr != 0x00) {
		isp_k_param->lsc_pfinfo.dev = &s_dcam_pdev->dev;
		sprd_cam_buf_addr_unmap(&isp_k_param->lsc_pfinfo);
	}

	if (!is_irq) {
		spin_unlock_irqrestore(&isp_mod_lock, flag);
		mutex_unlock(&dev->offline_thread_mutex);
	}

	if (atomic_read(&s_isp_total_runer) == 1)
		sprd_ispdrv_reset(dev->com_idx);

	sprd_ispdrv_irq_mask_dis(dev->com_idx);
	sprd_ispdrv_irq_clear(dev->com_idx);
	atomic_dec(&s_isp_total_runer);

	return -rtn;
}

int sprd_isp_drv_cap_start(void *handle,
	struct sprd_img_capture_param param, uint32_t is_irq)
{
	unsigned long flag = 0;
	enum isp_drv_rtn ret = ISP_RTN_SUCCESS;
	enum dcam_id idx = DCAM_ID_0;
	struct isp_fmcu_slice_desc *fmcu_slice = NULL;
	struct isp_pipe_dev *dev = NULL;
	struct isp_module *module = NULL;
	uint32_t cap_flag = param.type;
	uint32_t zsl_num = 0;

	if (!handle) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	dev = (struct isp_pipe_dev *)handle;
	idx = ISP_GET_ISP_ID(dev->com_idx);

	if (!is_irq)
		spin_lock_irqsave(&isp_mod_lock, flag);

	fmcu_slice = &dev->fmcu_slice;
	module = &dev->module_info;
	fmcu_slice->capture_state = ISP_ST_START;
	if (s_isp_group.dual_cam
		&& s_isp_group.dual_fullpath_stop == 0)
		s_isp_group.capture_param = param;

	if (!is_irq)
		dev->cap_flag = cap_flag;

	zsl_num = sprd_cam_queue_frm_cur_nodes(&module->full_zsl_queue);
	if (zsl_num == 0
		|| cap_flag == DCAM_CAPTURE_START_WITH_FLASH
		|| cap_flag == DCAM_CAPTURE_START_HDR
		|| cap_flag == DCAM_CAPTURE_START_3DNR) {
		dev->clr_queue = 1;
		dev->cap_cur_cnt = 0;
		dev->dcam_full_path_stop = 0;
		ret = ISP_RTN_SUCCESS;
		goto exit;
	}

	dev->isp_offline_state = ISP_ST_STOP;
	if (s_isp_group.dual_cam) {
		if (s_isp_group.dual_fullpath_stop == 0) {
			enum isp_id id = ISP_GET_ISP_ID(dev->com_idx);
			struct isp_pipe_dev *dev_dual = (struct isp_pipe_dev *)
						s_isp_group.isp_dev[id ^ 1];

			s_isp_group.dual_fullpath_stop = 1;
			sprd_dcam_drv_path_pause(DCAM_ID_0, CAMERA_FULL_PATH);
			sprd_dcam_drv_path_pause(DCAM_ID_1, CAMERA_FULL_PATH);
			module->isp_state |= ISP_ZSL_QUEUE_LOCK;
			dev_dual->module_info.isp_state |= ISP_ZSL_QUEUE_LOCK;
		}
	} else {
		if (!dev->dcam_full_path_stop) {
			dev->dcam_full_path_stop = 1;
			sprd_dcam_drv_path_pause(idx, CAMERA_FULL_PATH);
		}
	}
	dev->offline_proc_cap = 1;
	complete(&dev->offline_thread_com);

exit:
	if (!is_irq)
		spin_unlock_irqrestore(&isp_mod_lock, flag);

	return ret;
}

int sprd_isp_drv_fmcu_slice_stop(void *handle)
{
	unsigned long flag = 0;
	enum isp_drv_rtn ret = ISP_RTN_SUCCESS;
	struct isp_fmcu_slice_desc *fmcu_slice = NULL;
	struct isp_pipe_dev *dev = NULL;

	if (!handle) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	dev = (struct isp_pipe_dev *)handle;
	fmcu_slice = &dev->fmcu_slice;

	spin_lock_irqsave(&isp_mod_lock, flag);
	dev->module_info.isp_state &= ~ISP_ZSL_QUEUE_LOCK;
	fmcu_slice->capture_state = ISP_ST_STOP;
	dev->clr_queue = 0;
	dev->cap_cur_cnt = 0;
	dev->cap_flag = DCAM_CAPTURE_STOP;
	if (!s_isp_group.dual_cam)
		s_isp_group.dual_cap_sts = 0;
	spin_unlock_irqrestore(&isp_mod_lock, flag);

	return ret;
}


void sprd_isp_drv_path_clear(struct isp_pipe_dev *dev)
{
	struct isp_pipe_dev *isp_handle = NULL;
	struct isp_module *module = NULL;

	isp_handle = (struct isp_pipe_dev *)dev;
	module = &isp_handle->module_info;

	sprd_cam_queue_buf_clear(&module->bin_buf_queue);
	sprd_cam_queue_frm_clear(&module->bin_frm_queue);
	sprd_cam_queue_frm_clear(&module->bin_zsl_queue);
	sprd_cam_queue_frm_clear(&module->full_zsl_queue);
}

int sprd_isp_drv_path_cfg_set(void *isp_handle,
	enum isp_path_index path_index,
	enum isp_config_param id, void *param)
{
	uint32_t frm_deci = 0, format = 0, skip_num = 0;
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	enum isp_scl_id path_id;
	struct isp_path_desc *path = NULL;
	struct isp_regular_info *regular_info = NULL;
	struct isp_endian_sel *endian = NULL;
	struct isp_module *module = NULL;
	struct camera_size *size = NULL;
	struct camera_rect *rect = NULL;
	struct camera_addr *p_addr = NULL;
	struct isp_pipe_dev *dev = NULL;
	enum isp_id idx = ISP_ID_0;
	int cam_path_id = 0;
	struct isp_nr3_param *nr3_dev = NULL;

	if (!isp_handle || !param) {
		pr_err("fail to get valid input ptr\n");
		return -ISP_RTN_PARA_ERR;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	idx = ISP_GET_ISP_ID(dev->com_idx);
	if (ISP_PATH_IDX_PRE & path_index) {
		path_id = ISP_SCL_PRE;
		cam_path_id = CAMERA_PRE_PATH;
	} else if (ISP_PATH_IDX_VID & path_index) {
		path_id = ISP_SCL_VID;
		cam_path_id = CAMERA_VID_PATH;
	} else if (ISP_PATH_IDX_CAP & path_index) {
		path_id = ISP_SCL_CAP;
		cam_path_id = CAMERA_CAP_PATH;
	} else {
		pr_err("fail to select path\n");
		return -ISP_RTN_PARA_ERR;
	}

	module = &dev->module_info;
	path = &module->isp_path[path_id];
	nr3_dev = &path->nr3_param;
	path->input_format = dev->sn_mode;

	switch (id) {
	case ISP_PATH_INPUT_SIZE:
		size = (struct camera_size *)param;

		if (size->w > ISP_PATH_FRAME_WIDTH_MAX ||
			size->h > ISP_PATH_FRAME_HEIGHT_MAX) {
			rtn = ISP_RTN_PATH_IN_SIZE_ERR;
		} else {
			path->coeff_latest.param.in_size.w = size->w;
			path->coeff_latest.param.in_size.h = size->h;
			nr3_dev->prev_size.width = size->w;
			nr3_dev->prev_size.height = size->h;
		}
		break;
	case ISP_PATH_INPUT_RECT:
		rect = (struct camera_rect *)param;

		if (rect->x > ISP_PATH_FRAME_WIDTH_MAX ||
			rect->y > ISP_PATH_FRAME_HEIGHT_MAX ||
			rect->w > ISP_PATH_FRAME_WIDTH_MAX ||
			rect->h > ISP_PATH_FRAME_HEIGHT_MAX) {
			rtn = ISP_RTN_PATH_TRIM_SIZE_ERR;
		} else {
			path->coeff_latest.param.in_rect.x = rect->x;
			path->coeff_latest.param.in_rect.y = rect->y;
			path->coeff_latest.param.in_rect.w = rect->w;
			path->coeff_latest.param.in_rect.h = rect->h;
		}
		break;
	case ISP_PATH_OUTPUT_SIZE:
		size = (struct camera_size *)param;

		if (size->w > ISP_PATH_FRAME_WIDTH_MAX ||
			size->h > ISP_PATH_FRAME_HEIGHT_MAX) {
			rtn = ISP_RTN_PATH_OUT_SIZE_ERR;
		} else {
			path->coeff_latest.param.out_size.w = size->w;
			path->out_size = *size;
			path->coeff_latest.param.out_size.h = size->h;
		}
		break;
	case ISP_PATH_OUTPUT_FORMAT:
		format = *(uint32_t *)param;
		if ((format == DCAM_YUV422) || (format == DCAM_YUV420) ||
			(format == DCAM_YVU420) ||
			(format == DCAM_YUV420_3FRAME))
			path->output_format = format;
		else {
			rtn = ISP_RTN_OUT_FMT_ERR;
			path->output_format = DCAM_FTM_MAX;
		}
		break;
	case ISP_PATH_OUTPUT_ADDR:
		p_addr = (struct camera_addr *)param;

		if (DCAM_YUV_ADDR_INVALID(p_addr->yaddr,
			p_addr->uaddr, p_addr->vaddr)
			&& p_addr->mfd_y == 0) {
			pr_err("fail to get valid addr!\n");
			rtn = ISP_RTN_PATH_ADDR_ERR;
		} else {
			struct camera_frame frame;

			memset((void *)&frame, 0x00, sizeof(frame));
			frame.yaddr = p_addr->yaddr;
			frame.uaddr = p_addr->uaddr;
			frame.vaddr = p_addr->vaddr;
			frame.yaddr_vir = p_addr->yaddr_vir;
			frame.uaddr_vir = p_addr->uaddr_vir;
			frame.vaddr_vir = p_addr->vaddr_vir;

			frame.type = cam_path_id;
			frame.fid = path->frame_base_id;

			frame.buf_info.idx = idx;
			frame.buf_info.dev = &s_isp_pdev->dev;
			frame.buf_info.num = 2;
			frame.buf_info.type = CAM_BUF_USER_TYPE;
			frame.buf_info.mfd[0] = p_addr->mfd_y;
			frame.buf_info.mfd[1] = p_addr->mfd_u;
			frame.buf_info.mfd[2] = p_addr->mfd_v;

			/*may need update iommu here*/
			rtn = sprd_cam_buf_sg_table_get(&frame.buf_info);
			if (rtn) {
				pr_err("fail to config output addr!\n");
				rtn = DCAM_RTN_PATH_ADDR_ERR;
				break;
			}

			if (!sprd_cam_queue_buf_write(&path->buf_queue,
				&frame))
				path->output_frame_count++;

			pr_debug("isp%d path%d, y=0x%x u=0x%x v=0x%x mfd=0x%x 0x%x\n",
				idx, cam_path_id,
				p_addr->yaddr, p_addr->uaddr,
				p_addr->vaddr, frame.buf_info.mfd[0],
				frame.buf_info.mfd[1]);
		}
		break;
	case ISP_PATH_OUTPUT_RESERVED_ADDR:
		p_addr = (struct camera_addr *)param;

		if (DCAM_YUV_ADDR_INVALID(p_addr->yaddr,
			p_addr->uaddr, p_addr->vaddr)
			&& p_addr->mfd_y == 0) {
			pr_err("fail to get valid addr!\n");
			rtn = ISP_RTN_PATH_ADDR_ERR;
		} else {
			uint32_t output_frame_count = 0;
			struct camera_frame *frame = NULL;

			frame = &module->isp_path[path_id].path_reserved_frame;
			output_frame_count = path->output_frame_count;

			memset((void *)frame, 0x00, sizeof(*frame));
			frame->yaddr = p_addr->yaddr;
			frame->uaddr = p_addr->uaddr;
			frame->vaddr = p_addr->vaddr;
			frame->yaddr_vir = p_addr->yaddr_vir;
			frame->uaddr_vir = p_addr->uaddr_vir;
			frame->vaddr_vir = p_addr->vaddr_vir;

			frame->buf_info.idx = idx;
			frame->buf_info.dev = &s_isp_pdev->dev;
			frame->buf_info.num = 2;
			frame->buf_info.mfd[0] = p_addr->mfd_y;
			frame->buf_info.mfd[1] = p_addr->mfd_u;
			frame->buf_info.mfd[2] = p_addr->mfd_u;
			/*may need update iommu here*/
			rtn = sprd_cam_buf_sg_table_get(&frame->buf_info);
			if (rtn) {
				pr_err("fail to cfg reserved output addr!\n");
				rtn = DCAM_RTN_PATH_ADDR_ERR;
				break;
			}
			pr_debug("isp%d path%d, y=0x%x u=0x%x v=0x%x mfd=0x%x 0x%x\n",
				idx, cam_path_id,
				p_addr->yaddr, p_addr->uaddr,
				p_addr->vaddr, frame->buf_info.mfd[0],
				frame->buf_info.mfd[1]);
		}
		break;
	case ISP_PATH_FRM_DECI:
		frm_deci = *(uint32_t *)param;
		if (frm_deci >= DCAM_FRM_DECI_FAC_MAX)
			rtn = ISP_RTN_FRM_DECI_ERR;
		else
			path->frm_deci = frm_deci;
		break;
	case ISP_PATH_ENABLE:
		path->valid = *(uint32_t *)param;
		break;
	case ISP_PATH_SHRINK:
		regular_info = (struct isp_regular_info *)param;
		memcpy(&path->regular_info, regular_info,
		sizeof(struct isp_regular_info));
		break;
	case ISP_PATH_DATA_ENDIAN:
		endian = (struct isp_endian_sel *)param;
		if (endian->y_endian >= DCAM_ENDIAN_MAX) {
			rtn = ISP_RTN_PATH_ENDIAN_ERR;
		} else {
			path->data_endian.y_endian = endian->y_endian;
			if ((ISP_PATH_IDX_PRE |
			ISP_PATH_IDX_CAP |
			ISP_PATH_IDX_VID) & path_index) {
				path->data_endian.uv_endian = endian->uv_endian;
			}
		}
		break;
	case ISP_PATH_SKIP_NUM:
		skip_num = *(uint32_t *)param;
		if (skip_num >= DCAM_CAP_SKIP_FRM_MAX)
			rtn = ISP_RTN_FRM_DECI_ERR;
		else
			path->skip_num = skip_num;
		break;
	case ISP_NR3_ENABLE:
		nr3_dev->need_3dnr = *(uint32_t *)param;
		break;
	case ISP_SNS_MAX_SIZE:
		size = (struct camera_size *)param;
		nr3_dev->sns_max_size.width = size->w;
		nr3_dev->sns_max_size.height = size->h;
		break;
	case ISP_NR3_ME_CONV_SIZE:
		size = (struct camera_size *)param;
		nr3_dev->me_conv.input_width = size->w;
		nr3_dev->me_conv.input_height = size->h;
		break;
	case ISP_DUAL_CAM_EN:
		s_isp_group.dual_cam = *(uint32_t *)param;
		break;
	case ISP_PATH_SUPPORT_4IN1:
		dev->need_4in1 = *(uint32_t *)param;
		break;
	case ISP_PATH_ASSOC:
		path->assoc_idx = *(uint32_t *)param;
		break;
	default:
		pr_err("fail to get valid isp cfg id %d\n", id);
		break;
	}

	return -rtn;
}

int sprd_isp_drv_raw_cap_proc(void *isp_handle,
	struct isp_raw_proc_info *raw_cap)
{
	enum isp_drv_rtn ret = ISP_RTN_SUCCESS;
	struct isp_module *module = NULL;
	struct isp_path_desc *cap = NULL;
	struct camera_addr frm_addr = {0};
	struct isp_fmcu_slice_desc *fmcu_slice = NULL;
	struct isp_pipe_dev *dev = (struct isp_pipe_dev *)isp_handle;

	if (!dev || !raw_cap) {
		ret = -EFAULT;
		pr_err("fail to get valid input ptr\n");
		goto exit;
	}

	module = &dev->module_info;
	cap = &module->isp_path[ISP_SCL_CAP];
	fmcu_slice = &dev->fmcu_slice;

	dev->is_raw_capture = 1;
	fmcu_slice->capture_state = ISP_ST_START;
	cap->coeff_latest.param.in_size.w = raw_cap->in_size.width;
	cap->coeff_latest.param.in_size.h = raw_cap->in_size.height;
	cap->coeff_latest.param.out_size.w = raw_cap->out_size.width;
	cap->coeff_latest.param.out_size.h = raw_cap->out_size.height;
	cap->coeff_latest.param.in_rect.x = 0;
	cap->coeff_latest.param.in_rect.y = 0;
	cap->coeff_latest.param.in_rect.w = raw_cap->in_size.width;
	cap->coeff_latest.param.in_rect.h = raw_cap->in_size.height;
	cap->output_format = DCAM_YUV420;
	cap->valid = 1;

	frm_addr.yaddr = raw_cap->img_offset.chn0;
	frm_addr.uaddr = raw_cap->img_offset.chn1;
	frm_addr.vaddr = raw_cap->img_offset.chn2;
	frm_addr.yaddr_vir = raw_cap->img_vir.chn0;
	frm_addr.uaddr_vir = raw_cap->img_vir.chn1;
	frm_addr.vaddr_vir = raw_cap->img_vir.chn2;
	frm_addr.mfd_y = raw_cap->img_fd;

	ret = sprd_isp_drv_path_cfg_set((void *)dev, ISP_PATH_IDX_CAP,
		ISP_PATH_OUTPUT_ADDR, &frm_addr);

	ret = sprd_isp_drv_start((void *)dev);

exit:
	return ret;
}

int sprd_isp_drv_zoom_param_update(void *isp_handle,
			enum isp_path_index path_index,
			struct camera_size *in_size,
			struct camera_rect *in_rect,
			struct camera_size *out_size)
{
	enum isp_id id = ISP_ID_0;
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	struct isp_pipe_dev *dev = NULL;
	struct isp_module *module = NULL;
	struct isp_path_desc *pre = NULL;
	struct isp_path_desc *vid = NULL;
	struct isp_path_desc *cap = NULL;
	struct isp_zoom_param zoom_param;

	if (isp_handle == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->module_info;
	id = ISP_GET_ISP_ID(dev->com_idx);

	pre = &module->isp_path[ISP_SCL_PRE];
	vid = &module->isp_path[ISP_SCL_VID];
	cap = &module->isp_path[ISP_SCL_CAP];

	zoom_param.in_size.w = in_size->w;
	zoom_param.in_size.h = in_size->h;
	zoom_param.in_rect.x = in_rect->x;
	zoom_param.in_rect.y = in_rect->y;
	zoom_param.in_rect.w = in_rect->w;
	zoom_param.in_rect.h = in_rect->h;
	zoom_param.out_size.w = out_size->w;
	zoom_param.out_size.h = out_size->h;

	if (ISP_PATH_IDX_PRE & path_index) {
		rtn = sprd_cam_queue_buf_write(&pre->coeff_queue, &zoom_param);
		if (rtn) {
			pr_err("fail to write isp coeff node\n");
			return -rtn;
		}
	}

	if (ISP_PATH_IDX_VID & path_index) {
		rtn = sprd_cam_queue_buf_write(&vid->coeff_queue, &zoom_param);
		if (rtn) {
			pr_err("fail to write isp coeff node\n");
			return -rtn;
		}
	}
	if (ISP_PATH_IDX_CAP & path_index) {
		rtn = sprd_cam_queue_buf_write(&cap->coeff_queue, &zoom_param);
		if (rtn) {
			pr_err("fail to write isp coeff node\n");
			return -rtn;
		}
		if (sprd_cam_queue_buf_cur_nodes(&cap->coeff_queue)
			> 1) {
			rtn = sprd_cam_queue_buf_read(&cap->coeff_queue,
				&zoom_param);
			if (rtn) {
				pr_err("fail to read isp coeff node\n");
				return -rtn;
			}
		}
	}

	return -rtn;
}

int sprd_isp_drv_reg_isr(enum isp_id id, enum isp_irq_id irq_id,
	isp_isr_func user_func, void *user_data)
{
	int ret = 0;

	if (irq_id >= ISP_IMG_MAX) {
		pr_err("fail to get isp IRQ.\n");
		ret = ISP_RTN_IRQ_NUM_ERR;
	} else {
		ret = sprd_isp_int_irq_callback(id, irq_id,
			user_func, user_data);
		if (ret)
			pr_err("fail to Register isp callback\n");
	}

	return ret;
}

void sprd_isp_drv_reg_trace(enum isp_id idx)
{
#ifdef ISP_DRV_DEBUG
	unsigned long addr = 0;

	pr_info("ISP%d: Register list:\n", idx);
	for (addr = ISP_INT_EN0;
		addr <= ISP_INT_ALL_DONE_SRC_CTRL; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(idx, addr),
			ISP_REG_RD(idx, addr + 4),
			ISP_REG_RD(idx, addr + 8),
			ISP_REG_RD(idx, addr + 12));
	}
#endif
}
EXPORT_SYMBOL(sprd_isp_drv_reg_trace);
