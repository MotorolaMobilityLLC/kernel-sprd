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

#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <sprd_mm.h>

#include "cam_hw.h"
#include "cam_types.h"
#include "cam_queue.h"
#include "cam_buf.h"

#include "isp_interface.h"
#include "isp_reg.h"
#include "dcam_reg.h"
#include "isp_int.h"
#include "isp_core.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_INT: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

typedef void(*isp_isr)(enum isp_context_hw_id idx, void *param);

static const uint32_t isp_irq_process[] = {
	ISP_INT_SHADOW_DONE,
	ISP_INT_DISPATCH_DONE,
	ISP_INT_STORE_DONE_PRE,
	ISP_INT_STORE_DONE_VID,
	ISP_INT_NR3_ALL_DONE,
	ISP_INT_NR3_SHADOW_DONE,
	ISP_INT_STORE_DONE_THUMBNAIL,
	ISP_INT_RGB_LTMHISTS_DONE,
	ISP_INT_FMCU_LOAD_DONE,
	ISP_INT_FMCU_SHADOW_DONE,
	ISP_INT_ISP_ALL_DONE,
	ISP_INT_FMCU_STORE_DONE,
};

//#define ISP_INT_RECORD 1
#ifdef ISP_INT_RECORD
#define INT_RCD_SIZE 0x10000
static uint32_t isp_int_recorder[ISP_CONTEXT_HW_NUM][32][INT_RCD_SIZE];
static uint32_t int_index[ISP_CONTEXT_HW_NUM][32];
#endif

static uint32_t irq_done[ISP_CONTEXT_HW_NUM][32];
static uint32_t irq_done_sw[ISP_CONTEXT_SW_NUM][32];

static char *isp_dev_name[] = {"isp0",
				"isp1"
				};

static inline void ispint_isp_int_record(
	enum isp_context_id sw_cid,
	enum isp_context_hw_id c_id, uint32_t irq_line)
{
	uint32_t k;

	for (k = 0; k < 32; k++) {
		if (irq_line & (1 << k))
			irq_done[c_id][k]++;
	}

	for (k = 0; k < 32; k++) {
		if (irq_line & (1 << k))
			irq_done_sw[sw_cid][k]++;
	}

#ifdef ISP_INT_RECORD
	{
		uint32_t cnt, time, int_no;
		timespec cur_ts;

		ktime_get_ts(&cur_ts);
		time = (uint32_t)(cur_ts.tv_sec & 0xffff);
		time <<= 16;
		time |= (uint32_t)((cur_ts.tv_nsec / (NSEC_PER_USEC * 100)) & 0xffff);
		for (int_no = 0; int_no < 32; int_no++) {
			if (irq_line & BIT(int_no)) {
				cnt = int_index[c_id][int_no];
				isp_int_recorder[c_id][int_no][cnt] = time;
				cnt++;
				int_index[c_id][int_no] = (cnt & (INT_RCD_SIZE - 1));
			}
		}
	}
#endif
}

static int ispint_err_pre_proc(enum isp_context_hw_id hw_idx, void *isp_handle)
{
	unsigned long addr = 0;
	struct isp_pipe_dev *dev = NULL;
	struct isp_sw_context *pctx;
	uint32_t val[4];
	int idx = -1;

	dev = (struct isp_pipe_dev *)isp_handle;
	idx = isp_core_sw_context_id_get(hw_idx, dev);
	if (idx < 0) {
		pr_err("fail to get sw_id for hw_idx=%d\n", hw_idx);
		return 0;
	}
	pctx = dev->sw_ctx[idx];

	pr_info("isp sw %d hw %d\n", idx, hw_idx);
	pr_info("ISP: INT Register list\n");
	for (addr = 0x0; addr <= 0x3C; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
	}
	pr_info("ISP: Common Register list\n");
	for (addr = 0x700; addr <= 0x998; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
	}
	pr_info("ISP: pyr_rec Register list\n");
	for (addr = 0x9310; addr <= 0x9334; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
	}
	for (addr = 0x9510; addr <= 0x9534; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
	}
	for (addr = 0x9610; addr <= 0x963C; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
	}
	for (addr = 0x9B10; addr <= 0x9B14; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
	}

	pr_info("MMU register list info\n");
	for (addr = ISP_MMU_VERSION; addr <= ISP_MMU_INT_RAW; addr += 16) {
		val[0] = ISP_MMU_RD(addr);
		val[1] = ISP_MMU_RD(addr + 4);
		val[2] = ISP_MMU_RD(addr + 8);
		val[3] = ISP_MMU_RD(addr + 12);
		pr_err("offset=0x%04x: %08x %08x %08x %08x\n",
			addr, val[0], val[1], val[2], val[3]);
	}

	pr_info("ISP fetch: register list\n");
	for (addr = (ISP_FETCH_BASE + ISP_FETCH_PARAM0); addr <= (ISP_FETCH_BASE + ISP_FETCH_MIPI_PARAM_UV); addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));

		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(0, addr),
			ISP_REG_RD(0, addr + 4),
			ISP_REG_RD(0, addr + 8),
			ISP_REG_RD(0, addr + 12));
	}

	pr_info("ISP afbd fetch: register list\n");
	for (addr = (ISP_YUV_AFBD_FETCH_BASE + ISP_AFBD_FETCH_SEL); addr <= (ISP_YUV_AFBD_FETCH_BASE + ISP_AFBD_FETCH_PARAM2); addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));

		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(0, addr),
			ISP_REG_RD(0, addr + 4),
			ISP_REG_RD(0, addr + 8),
			ISP_REG_RD(0, addr + 12));
	}

	pr_info("ISP dispatch: register list\n");
	for (addr = (ISP_DISPATCH_BASE + ISP_DISPATCH_CH0_BAYER); addr <= (ISP_DISPATCH_BASE + ISP_DISPATCH_CHK_SUM);
		addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(0, addr),
			ISP_REG_RD(0, addr + 4),
			ISP_REG_RD(0, addr + 8),
			ISP_REG_RD(0, addr + 12));
	}

	pr_info("ISP axi: register list\n");
	for (addr = 0x510; addr <= 0x55C; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
	}

	for (addr = (ISP_SCALER_PRE_CAP_BASE + ISP_SCALER_CFG); addr <= (ISP_SCALER_PRE_CAP_BASE + ISP_SCALER_RES);
			addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(0, addr),
			ISP_REG_RD(0, addr + 4),
			ISP_REG_RD(0, addr + 8),
			ISP_REG_RD(0, addr + 12));
	}

	for (addr = (ISP_SCALER_VID_BASE + ISP_SCALER_CFG); addr <= (ISP_SCALER_VID_BASE + ISP_SCALER_RES);
		addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(0, addr),
			ISP_REG_RD(0, addr + 4),
			ISP_REG_RD(0, addr + 8),
			ISP_REG_RD(0, addr + 12));
	}

	pr_info("ISP store: register list\n");
	for (addr = (ISP_STORE_PRE_CAP_BASE + ISP_STORE_PARAM); addr <= (ISP_STORE_PRE_CAP_BASE + ISP_STORE_SHADOW_CLR);
			addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(0, addr),
			ISP_REG_RD(0, addr + 4),
			ISP_REG_RD(0, addr + 8),
			ISP_REG_RD(0, addr + 12));
	}

	pr_info("ISP store: register list\n");
	for (addr = (ISP_STORE_VID_BASE + ISP_STORE_PARAM); addr <= (ISP_STORE_VID_BASE + ISP_STORE_SHADOW_CLR);
			addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(0, addr),
			ISP_REG_RD(0, addr + 4),
			ISP_REG_RD(0, addr + 8),
			ISP_REG_RD(0, addr + 12));
	}

	pr_info("ISP 3dnr: register list\n");
	for (addr = 0x2010; addr <= 0x204C; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(0, addr),
			ISP_REG_RD(0, addr + 4),
			ISP_REG_RD(0, addr + 8),
			ISP_REG_RD(0, addr + 12));
	}

	pr_info("ISP 3dnr store: register list\n");
	for (addr = 0x2210; addr <= 0x223C; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(0, addr),
			ISP_REG_RD(0, addr + 4),
			ISP_REG_RD(0, addr + 8),
			ISP_REG_RD(0, addr + 12));
	}

	pr_info("ISP 3dnr crop: register list\n");
	for (addr = 0x2310; addr <= 0x231C; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(0, addr),
			ISP_REG_RD(0, addr + 4),
			ISP_REG_RD(0, addr + 8),
			ISP_REG_RD(0, addr + 12));
	}
	pctx->isp_cb_func(ISP_CB_DEV_ERR, dev, pctx->cb_priv_data);

	return 0;
}

static void ispint_all_done(enum isp_context_hw_id hw_idx, void *isp_handle)
{
	struct isp_pipe_dev *dev = NULL;
	struct isp_sw_context *pctx;
	struct isp_hw_context *pctx_hw;
	int idx = -1;

	dev = (struct isp_pipe_dev *)isp_handle;
	idx = isp_core_sw_context_id_get(hw_idx, dev);
	if (idx < 0) {
		pr_err("fail to get sw_id for hw_idx=%d\n", hw_idx);
		return;
	}

	pctx = dev->sw_ctx[idx];
	pctx_hw = &dev->hw_ctx[hw_idx];
	if (pctx_hw->fmcu_used) {
		pr_debug("fmcu started. skip all done, hw_idx %d\n ", hw_idx);
		return;
	}

	pr_debug("cxt_id:%d all done.\n", idx);
	if (pctx->multi_slice) {
		pr_debug("slice done. last %d\n", pctx->is_last_slice);
		if (!pctx->is_last_slice) {
			complete(&pctx->slice_done);
			return;
		}
		complete(&pctx->slice_done);
		pr_debug("frame done.\n");
	}

	pctx->post_type = POSTPROC_FRAME_DONE;
	complete(&pctx->postproc_thread.thread_com);
}

static void ispint_shadow_done(enum isp_context_hw_id idx, void *isp_handle)
{
	pr_debug("cxt_id:%d shadow done.\n", idx);
}

static void ispint_dispatch_done(enum isp_context_hw_id idx, void *isp_handle)
{
	pr_debug("cxt_id:%d done.\n", idx);
}

static void ispint_pre_store_done(enum isp_context_hw_id idx, void *isp_handle)
{
	pr_debug("cxt_id:%d done.\n", idx);
}

static void ispint_vid_store_done(enum isp_context_hw_id idx, void *isp_handle)
{
	pr_debug("cxt_id:%d done.\n", idx);
}

static void ispint_thumb_store_done(enum isp_context_hw_id idx, void *isp_handle)
{
	pr_debug("cxt_id:%d done.\n", idx);
}

static void ispint_fmcu_store_done(enum isp_context_hw_id hw_idx, void *isp_handle)
{
	struct isp_pipe_dev *dev = NULL;
	struct isp_sw_context *pctx;
	struct isp_hw_context *pctx_hw;
	int i;
	int idx = -1;

	dev = (struct isp_pipe_dev *)isp_handle;
	pctx_hw = &dev->hw_ctx[hw_idx];
	if (pctx_hw->fmcu_handle == NULL) {
		pr_warn("warning: no fmcu for hw %d\n", hw_idx);
		return;
	}

	idx = isp_core_sw_context_id_get(hw_idx, dev);
	if (idx < 0) {
		pr_err("fail to get sw_id for hw_idx=%d\n", hw_idx);
		return;
	}

	pctx = dev->sw_ctx[idx];
	pctx->post_type = POSTPROC_FRAME_DONE;
	pr_debug("fmcu done cxt_id:%d ch_id[%d]\n", idx, pctx->ch_id);
	complete(&pctx->postproc_thread.thread_com);

	if (pctx->uinfo.enable_slowmotion == 1) {
		isp_core_context_unbind(pctx);
		complete(&pctx->frm_done);
		for (i = 0; i < pctx->uinfo.slowmotion_count - 1; i++)
			complete(&pctx->postproc_thread.thread_com);
	}
}

static void ispint_fmcu_shadow_done(enum isp_context_hw_id hw_idx, void *isp_handle)
{
	int idx = -1;
	struct isp_pipe_dev *dev = NULL;
	struct isp_hw_context *pctx_hw;

	dev = (struct isp_pipe_dev *)isp_handle;
	pctx_hw = &dev->hw_ctx[hw_idx];
	if (pctx_hw->fmcu_handle == NULL) {
		pr_warn("warning: no fmcu for hw %d\n", hw_idx);
		return;
	}

	idx = isp_core_sw_context_id_get(hw_idx, dev);
	if (idx < 0) {
		pr_err("fail to get sw_id for hw_idx=%d\n", hw_idx);
		return;
	}
	pr_debug("cxt_id:%d done.\n", idx);
}

static void ispint_fmcu_load_done(enum isp_context_hw_id idx, void *isp_handle)
{
	pr_debug("cxt_id:%d done.\n", idx);
}

static void ispint_3dnr_all_done(enum isp_context_hw_id hw_idx, void *isp_handle)
{
	struct isp_sw_context *pctx;
	struct isp_pipe_dev *dev;
	int idx = -1;

	dev = (struct isp_pipe_dev *)isp_handle;
	idx = isp_core_sw_context_id_get(hw_idx, dev);
	if (idx < 0) {
		pr_err("fail to get sw_id for hw_idx=%d\n", hw_idx);
		return;
	}

	pctx = dev->sw_ctx[idx];

	pr_debug("3dnr all done. cxt_id:%d\n", idx);
}

static void ispint_3dnr_shadow_done(enum isp_context_hw_id hw_idx, void *isp_handle)
{
	struct isp_sw_context *pctx;
	struct isp_pipe_dev *dev;
	int idx = -1;

	dev = (struct isp_pipe_dev *)isp_handle;
	idx = isp_core_sw_context_id_get(hw_idx, dev);
	if (idx < 0) {
		pr_err("fail to get sw_id for hw_idx=%d\n", hw_idx);
		return;
	}

	pctx = dev->sw_ctx[idx];

	pr_debug("3dnr shadow done. cxt_id:%d\n", idx);

}

static void ispint_rgb_ltm_hists_done(enum isp_context_hw_id hw_idx, void *isp_handle)
{
	struct isp_sw_context *pctx;
	struct isp_pipe_dev *dev;
	struct isp_ltm_ctx_desc *rgb_ltm = NULL;
	int completion = 0;
	int idx = -1;

	dev = (struct isp_pipe_dev *)isp_handle;
	idx = isp_core_sw_context_id_get(hw_idx, dev);
	if (idx < 0) {
		pr_err("fail to get sw_id for hw_idx=%d\n", hw_idx);
		return;
	}

	pctx = dev->sw_ctx[idx];
	rgb_ltm = (struct isp_ltm_ctx_desc *)pctx->rgb_ltm_handle;
	if (!rgb_ltm)
		return;

	rgb_ltm->ltm_ops.sync_ops.set_frmidx(rgb_ltm);
	completion = rgb_ltm->ltm_ops.sync_ops.get_completion(rgb_ltm);
	if (completion && (rgb_ltm->fid >= completion))
		completion = rgb_ltm->ltm_ops.sync_ops.do_completion(rgb_ltm);
}

static isp_isr isp_isr_handler[32] = {
	[ISP_INT_ISP_ALL_DONE] = ispint_all_done,
	[ISP_INT_SHADOW_DONE] = ispint_shadow_done,
	[ISP_INT_DISPATCH_DONE] = ispint_dispatch_done,
	[ISP_INT_STORE_DONE_PRE] = ispint_pre_store_done,
	[ISP_INT_STORE_DONE_VID] = ispint_vid_store_done,
	[ISP_INT_NR3_ALL_DONE]	 = ispint_3dnr_all_done,
	[ISP_INT_NR3_SHADOW_DONE] = ispint_3dnr_shadow_done,
	[ISP_INT_STORE_DONE_THUMBNAIL] = ispint_thumb_store_done,
	[ISP_INT_FMCU_LOAD_DONE] = ispint_fmcu_load_done,
	[ISP_INT_FMCU_SHADOW_DONE] = ispint_fmcu_shadow_done,
	[ISP_INT_FMCU_STORE_DONE] = ispint_fmcu_store_done,
	[ISP_INT_RGB_LTMHISTS_DONE]  = ispint_rgb_ltm_hists_done,
};

struct isp_int_ctx {
	unsigned long reg_offset;
	uint32_t err_mask;
	uint32_t irq_numbers;
	const uint32_t *irq_vect;
} isp_int_ctxs[4] = {
		{ /* P0 */
			ISP_P0_INT_BASE,
			ISP_INT_LINE_MASK_ERR | ISP_INT_LINE_MASK_MMU,
			(uint32_t)ARRAY_SIZE(isp_irq_process),
			isp_irq_process,
		},
		{ /* C0 */
			ISP_C0_INT_BASE,
			ISP_INT_LINE_MASK_ERR | ISP_INT_LINE_MASK_MMU,
			(uint32_t)ARRAY_SIZE(isp_irq_process),
			isp_irq_process,
		},
		{ /* P1 */
			ISP_P1_INT_BASE,
			ISP_INT_LINE_MASK_ERR | ISP_INT_LINE_MASK_MMU,
			(uint32_t)ARRAY_SIZE(isp_irq_process),
			isp_irq_process,
		},
		{ /* C1 */
			ISP_C1_INT_BASE,
			ISP_INT_LINE_MASK_ERR | ISP_INT_LINE_MASK_MMU,
			(uint32_t)ARRAY_SIZE(isp_irq_process),
			isp_irq_process,
		},
};

static void ispint_iommu_regs_dump(void)
{
	uint32_t reg = 0;
	uint32_t val[4];

	for (reg = ISP_MMU_VERSION; reg <= ISP_MMU_INT_RAW; reg += 16) {
		val[0] = ISP_MMU_RD(reg);
		val[1] = ISP_MMU_RD(reg + 4);
		val[2] = ISP_MMU_RD(reg + 8);
		val[3] = ISP_MMU_RD(reg + 12);
		pr_err("offset=0x%04x: %08x %08x %08x %08x\n",
			reg, val[0], val[1], val[2], val[3]);
	}

	pr_err("fetch y %08x u %08x pyr_rec_fetch y %08x u %08x pyr_dec_fetch y %08x u %08x\n",
		ISP_HREG_RD(ISP_FETCH_BASE + ISP_FETCH_SLICE_Y_ADDR),
		ISP_HREG_RD(ISP_FETCH_BASE + ISP_FETCH_SLICE_U_ADDR),
		ISP_HREG_RD(PYR_REC_CUR_FETCH_BASE + ISP_FETCH_SLICE_Y_ADDR),
		ISP_HREG_RD(PYR_REC_CUR_FETCH_BASE + ISP_FETCH_SLICE_U_ADDR),
		ISP_HREG_RD(PYR_DEC_FETCH_BASE + ISP_FETCH_SLICE_Y_ADDR),
		ISP_HREG_RD(PYR_DEC_FETCH_BASE + ISP_FETCH_SLICE_U_ADDR));

	pr_err("afbd head normal/rec frame %08x slice %08x dec frame %08x slice %08x\n",
		ISP_HREG_RD(ISP_YUV_AFBD_FETCH_BASE + ISP_AFBD_FETCH_PARAM1),
		ISP_HREG_RD(ISP_YUV_AFBD_FETCH_BASE + ISP_AFBD_FETCH_PARAM2),
		ISP_HREG_RD(ISP_YUV_DEC_AFBD_FETCH_BASE + ISP_AFBD_FETCH_PARAM1),
		ISP_HREG_RD(ISP_YUV_DEC_AFBD_FETCH_BASE + ISP_AFBD_FETCH_PARAM2));

	pr_err("store pre cap y %08x u %08x afbc head y %08x y %08x offset %08x\n",
		ISP_HREG_RD(ISP_STORE_PRE_CAP_BASE + ISP_STORE_SLICE_Y_ADDR),
		ISP_HREG_RD(ISP_STORE_PRE_CAP_BASE + ISP_STORE_SLICE_U_ADDR),
		ISP_HREG_RD(ISP_CAP_FBC_STORE_BASE + ISP_FBC_STORE_SLICE_HEADER_BASE_ADDR),
		ISP_HREG_RD(ISP_CAP_FBC_STORE_BASE + ISP_FBC_STORE_SLICE_PAYLOAD_BASE_ADDR),
		ISP_HREG_RD(ISP_CAP_FBC_STORE_BASE + ISP_FBC_STORE_SLICE_PAYLOAD_OFFSET_ADDR));

	pr_err("store vid y %08x u %08x fbc head y %08x y %08x offset %08x\n",
		ISP_HREG_RD(ISP_STORE_VID_BASE + ISP_STORE_SLICE_Y_ADDR),
		ISP_HREG_RD(ISP_STORE_VID_BASE + ISP_STORE_SLICE_U_ADDR),
		ISP_HREG_RD(ISP_VID_FBC_STORE_BASE  + ISP_FBC_STORE_SLICE_HEADER_BASE_ADDR),
		ISP_HREG_RD(ISP_VID_FBC_STORE_BASE  + ISP_FBC_STORE_SLICE_PAYLOAD_BASE_ADDR),
		ISP_HREG_RD(ISP_VID_FBC_STORE_BASE  + ISP_FBC_STORE_SLICE_PAYLOAD_OFFSET_ADDR));

	pr_err("store thumb y %08x u %08x\n",
		ISP_HREG_RD(ISP_STORE_THUMB_BASE + ISP_STORE_SLICE_Y_ADDR),
		ISP_HREG_RD(ISP_STORE_THUMB_BASE + ISP_STORE_SLICE_U_ADDR));
}

static irqreturn_t ispint_isr_root(int irq, void *priv)
{
	unsigned long irq_offset;
	uint32_t iid;
	enum isp_context_hw_id c_id;
	uint32_t sid, k;
	uint32_t err_mask;
	uint32_t irq_line = 0, irq_line1 = 0;
	uint32_t irq_numbers = 0;
	const uint32_t *irq_vect = NULL;
	struct isp_pipe_dev *isp_handle = (struct isp_pipe_dev *)priv;
	struct isp_sw_context *ctx = NULL;

	if (!isp_handle) {
		pr_err("fail to get valid dev\n");
		return IRQ_HANDLED;
	}

	if (atomic_read(&isp_handle->enable) == 0) {
		pr_err("fail to get isp_handle enable\n");
		return IRQ_HANDLED;
	}

	if (irq == isp_handle->irq_no[0]) {
		iid = 0;
	} else if (irq == isp_handle->irq_no[1]) {
		iid = 1;
	} else {
		pr_err("fail to get irq %d mismatched\n", irq);
		return IRQ_NONE;
	}
	pr_debug("isp irq %d, priv %p, iid %d\n", irq, priv, iid);

	for (sid = 0; sid < 2; sid++) {
		int sw_ctx_id = -1;

		c_id = (iid << 1) | sid;

		irq_offset = isp_int_ctxs[c_id].reg_offset;
		err_mask = isp_int_ctxs[c_id].err_mask;
		irq_numbers = isp_int_ctxs[c_id].irq_numbers;
		irq_vect = isp_int_ctxs[c_id].irq_vect;

		irq_line = ISP_HREG_RD(irq_offset + ISP_INT_INT0);
		irq_line1 = ISP_HREG_RD(irq_offset + ISP_INT_INT1);
		if (unlikely((irq_line == 0) && (irq_line1 == 0))) {
			continue;
		}

		sw_ctx_id = isp_core_sw_context_id_get(c_id, isp_handle);
		pr_debug("sw %d, hw %d, irq_line: 0x%08x 0x%08x\n", sw_ctx_id, c_id, irq_line, irq_line1);

		if (sw_ctx_id < 0) {
			ISP_HREG_WR(irq_offset + ISP_INT_CLR0, irq_line);
			ISP_HREG_WR(irq_offset + ISP_INT_CLR1, irq_line1);
			if (irq_line & ISP_INT_LINE_MASK)
				pr_debug("get c_id, hw: %d has no sw_ctx_id, irq_line: %08x\n", c_id, irq_line);
			continue;
		}

		if (isp_handle->sw_ctx[sw_ctx_id] == NULL) {
			pr_err("fail to get sw_ctx\n");
			return IRQ_HANDLED;
		} else {
			ctx = isp_handle->sw_ctx[sw_ctx_id];
			ctx->in_irq_handler = 1;
		}

		ispint_isp_int_record(sw_ctx_id, c_id, irq_line);

		/*clear the interrupt*/
		ISP_HREG_WR(irq_offset + ISP_INT_CLR0, irq_line);
		ISP_HREG_WR(irq_offset + ISP_INT_CLR1, irq_line1);
		pr_debug("isp ctx %d irqno %d, INT: 0x%x\n", c_id, irq, irq_line);

		if (atomic_read(&ctx->user_cnt) < 1) {
			pr_info("contex %d is stopped\n", sw_ctx_id);
			ctx->in_irq_handler = 0;
			return IRQ_HANDLED;
		}

		if (unlikely(ctx->started == 0)) {
			pr_info("ctx %d not started. irq 0x%x\n", sw_ctx_id, irq_line);
			ctx->in_irq_handler = 0;
			return IRQ_HANDLED;
		}

		if (unlikely(err_mask & irq_line)) {
			pr_err("fail to get normal status ISP ctx%d 0x%x\n", sw_ctx_id, irq_line);
			if (irq_line & ISP_INT_LINE_MASK_MMU) {
				uint32_t val;

				val = ISP_MMU_RD(ISP_MMU_INT_STS);

				if (val != ctx->iommu_status) {
					ctx->iommu_status = val;
					ispint_iommu_regs_dump();
				}
			}

			/*handle the error here*/
			if (ispint_err_pre_proc(c_id, isp_handle)) {
				pr_err("fail to handle the error here c_id %d irq_line 0x%x\n", c_id, irq_line);
				ctx->in_irq_handler = 0;
				return IRQ_HANDLED;
			}
		}

		for (k = 0; k < irq_numbers; k++) {
			uint32_t irq_id = irq_vect[k];

			if (irq_line & (1 << irq_id)) {
				if (isp_isr_handler[irq_id]) {
					isp_isr_handler[irq_id](
						c_id, isp_handle);
				}
			}
			irq_line  &= ~(1 << irq_id);
			if (!irq_line)
				break;
		}
		ctx->in_irq_handler = 0;
	}

	return IRQ_HANDLED;
}

int isp_int_irq_request(struct device *p_dev,
		uint32_t *irq_no, void *isp_handle)
{
	int ret = 0;
	uint32_t  id;
	struct isp_pipe_dev *ispdev;

	if (!p_dev || !isp_handle || !irq_no) {
		pr_err("fail to get valid input ptr p_dev %p isp_handle %p irq_no %p\n",
			p_dev, isp_handle, irq_no);
		return -EFAULT;
	}
	ispdev = (struct isp_pipe_dev *)isp_handle;

	for (id = 0; id < ISP_LOGICAL_COUNT; id++) {
		if (id == (ISP_LOGICAL_COUNT - 1))
			break;
		ispdev->irq_no[id] = irq_no[id];
		ret = devm_request_irq(p_dev,
				ispdev->irq_no[id], ispint_isr_root,
				IRQF_SHARED, isp_dev_name[id], (void *)ispdev);
		if (ret) {
			pr_err("fail to install isp%d irq_no %d\n",
					id, ispdev->irq_no[id]);
			if (id == 1)
				free_irq(ispdev->irq_no[0], (void *)ispdev);
			return -EFAULT;
		}
		pr_info("install isp%d irq_no %d\n", id, ispdev->irq_no[id]);
	}

	memset(irq_done, 0, sizeof(irq_done));
	memset(irq_done_sw, 0, sizeof(irq_done_sw));

	return ret;
}

int isp_int_isp_irq_cnt_reset(int ctx_id)
{
	if (ctx_id < ISP_CONTEXT_HW_NUM)
		memset(irq_done[ctx_id], 0, sizeof(irq_done[ctx_id]));

#ifdef ISP_INT_RECORD
	if (ctx_id < ISP_CONTEXT_HW_NUM) {
		memset(isp_int_recorder[ctx_id][0], 0, sizeof(isp_int_recorder) / ISP_CONTEXT_HW_NUM);
		memset(int_index[ctx_id], 0, sizeof(int_index) / ISP_CONTEXT_HW_NUM);
	}
#endif
	return 0;
}

int isp_int_isp_irq_cnt_trace(int ctx_id)
{
	int i;

	if (ctx_id >= ISP_CONTEXT_HW_NUM)
		return 0;

	for (i = 0; i < 32; i++)
		if(irq_done[ctx_id][i])
			pr_info("done %d %d :   %d\n", ctx_id, i, irq_done[ctx_id][i]);

#ifdef ISP_INT_RECORD
	{
		uint32_t cnt, j;
		int idx = ctx_id;
		for (cnt = 0; cnt < (uint32_t)irq_done[idx][ISP_INT_SHADOW_DONE]; cnt += 4) {
			j = (cnt & (INT_RCD_SIZE - 1)); //rolling
			pr_info("isp%u j=%d, %03d.%04d, %03d.%04d, %03d.%04d, %03d.%04d, %03d.%04d, %03d.%04d\n",
			idx, j, (uint32_t)isp_int_recorder[idx][ISP_INT_ISP_ALL_DONE][j] >> 16,
			 (uint32_t)isp_int_recorder[idx][ISP_INT_ISP_ALL_DONE][j] & 0xffff,
			 (uint32_t)isp_int_recorder[idx][ISP_INT_SHADOW_DONE][j] >> 16,
			 (uint32_t)isp_int_recorder[idx][ISP_INT_SHADOW_DONE][j] & 0xffff,
			 (uint32_t)isp_int_recorder[idx][ISP_INT_DISPATCH_DONE][j] >> 16,
			 (uint32_t)isp_int_recorder[idx][ISP_INT_DISPATCH_DONE][j] & 0xffff,
			 (uint32_t)isp_int_recorder[idx][ISP_INT_STORE_DONE_PRE][j] >> 16,
			 (uint32_t)isp_int_recorder[idx][ISP_INT_STORE_DONE_PRE][j] & 0xffff,
			 (uint32_t)isp_int_recorder[idx][ISP_INT_STORE_DONE_VID][j] >> 16,
			 (uint32_t)isp_int_recorder[idx][ISP_INT_STORE_DONE_VID][j] & 0xffff,
			 (uint32_t)isp_int_recorder[idx][ISP_INT_FMCU_CONFIG_DONE][j] >> 16,
			 (uint32_t)isp_int_recorder[idx][ISP_INT_FMCU_CONFIG_DONE][j] & 0xffff);
		}
	}
#endif
	return 0;
}

int isp_int_isp_irq_sw_cnt_reset(int ctx_id)
{
	if (ctx_id < ISP_CONTEXT_SW_NUM)
		memset(irq_done_sw[ctx_id], 0, sizeof(irq_done_sw[ctx_id]));

	return 0;
}

int isp_int_isp_irq_sw_cnt_trace(int ctx_id)
{
	int i;

	if (ctx_id >= ISP_CONTEXT_SW_NUM)
		return 0;

	for (i = 0; i < 32; i++)
		if(irq_done_sw[ctx_id][i])
			pr_info("done %d %d :   %d\n", ctx_id, i, irq_done_sw[ctx_id][i]);
	return 0;
}

int isp_int_irq_free(struct device *p_dev, void *isp_handle)
{
	struct isp_pipe_dev *ispdev;

	ispdev = (struct isp_pipe_dev *)isp_handle;
	if (!ispdev) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	devm_free_irq(p_dev, ispdev->irq_no[0], (void *)ispdev);
	devm_free_irq(p_dev, ispdev->irq_no[1], (void *)ispdev);

	return 0;
}
