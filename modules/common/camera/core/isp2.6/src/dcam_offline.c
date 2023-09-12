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

#include <sprd_mm.h>
#include <linux/delay.h>

#include "dcam_core.h"
#include "dcam_path.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "DCAM_OFFLINE: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

struct camera_frame *dcam_offline_cycle_frame(struct dcam_sw_context *pctx)
{
	struct camera_frame *pframe = NULL;
	int loop = 0;
	int ret = 0;
	struct dcam_path_desc *path = NULL;
	struct cam_hw_info *hw = NULL;

	if (!pctx) {
		pr_err("fail to get valid sw context\n");
		return NULL;
	}

	pframe = cam_queue_dequeue(&pctx->in_queue, struct camera_frame, list);
	if (pframe == NULL) {
		pr_err("fail to get input frame (%p) for ctx %d\n", pframe, pctx->sw_ctx_id);
		return NULL;
	}

	/* for L3 DCAM1 */
	if (DCAM_FETCH_TWICE(pctx)) {
		hw = pctx->dev->hw;
		pctx->raw_fetch_count++;
		if (!DCAM_FIRST_FETCH(pctx)) {
			struct camera_frame *frame = NULL;

			path = &pctx->path[hw->ip_dcam[DCAM_HW_CONTEXT_1]->aux_dcam_path];
			frame = cam_queue_dequeue_peek(&path->out_buf_queue, struct camera_frame, list);
			if (frame == NULL) {
				pr_err("fail to get out_buf_queue frame\n");
				return NULL;
			}
			pframe->endian = frame->endian;
			pframe->pattern = frame->pattern;
			pframe->width = frame->width;
			pframe->height = frame->height;
		}
	}

	pr_debug("frame %p, dcam%d ch_id %d.  buf_fd %d\n", pframe,
		pctx->hw_ctx_id, pframe->channel_id, pframe->buf.mfd[0]);
	pr_debug("size %d %d,  endian %d, pattern %d\n",
		pframe->width, pframe->height, pframe->endian, pframe->pattern);

	ret = cam_buf_iommu_map(&pframe->buf, CAM_IOMMUDEV_DCAM);
	if (ret) {
		pr_err("fail to map buf to DCAM iommu. cxt %d\n", pctx->sw_ctx_id);
		goto map_err;
	}

	loop = 0;
	do {
		ret = cam_queue_enqueue(&pctx->proc_queue, &pframe->list);
		if (ret == 0)
			break;
		pr_info_ratelimited("wait for proc queue. loop %d\n", loop);
		/* wait for previous frame proccessed done.*/
		usleep_range(600, 2000);
	} while (loop++ < 500);

	if (ret) {
		pr_err("fail to get proc queue, timeout.\n");
		ret = -EINVAL;
		goto inq_overflow;
	}
	return pframe;

inq_overflow:
	cam_buf_iommu_unmap(&pframe->buf);

map_err:
	pctx->dcam_cb_func(DCAM_CB_RET_SRC_BUF, pframe, pctx->cb_priv_data);

	return NULL;
}

uint32_t dcam_offline_slice_needed(struct cam_hw_info *hw, struct dcam_sw_context *pctx, uint32_t *dev_lbuf, uint32_t in_width)
{
	struct cam_hw_lbuf_share lbufarg;
	struct cam_hw_lbuf_share camarg;
	uint32_t out_width = 0;

	lbufarg.idx = pctx->hw_ctx_id;
	lbufarg.width = 0;
	hw->dcam_ioctl(hw, DCAM_HW_CFG_LBUF_SHARE_GET, &lbufarg);
	out_width = lbufarg.width;

	camarg.idx = pctx->hw_ctx_id;
	camarg.width = in_width;
	camarg.offline_flag = 1;
	pr_debug("Dcam%d, sw_ctx%d, new width %d old linebuf %d\n", camarg.idx, pctx->sw_ctx_id, camarg.width, lbufarg.width);
	if (hw->ip_dcam[pctx->hw_ctx_id]->lbuf_share_support && (lbufarg.width < camarg.width)) {
		hw->dcam_ioctl(hw, DCAM_HW_CFG_LBUF_SHARE_SET, &camarg);
		lbufarg.idx = pctx->hw_ctx_id;
		lbufarg.width = 0;
		hw->dcam_ioctl(hw, DCAM_HW_CFG_LBUF_SHARE_GET, &lbufarg);
		out_width = lbufarg.width;
	}
	if (!out_width)
		*dev_lbuf = DCAM_PATH_WMAX * 2;
	else
		*dev_lbuf = out_width;
	if (in_width > *dev_lbuf)
		return 1;
	else
		return 0;

}

int dcamoffline_fetch_info_get(struct dcam_sw_context *pctx, struct camera_frame *pframe)
{
	int i = 0;
	uint32_t loose_val = 0;
	uint32_t val_4in1 = 0;
	struct dcam_path_desc *path = NULL;
	struct dcam_fetch_info *fetch = NULL;

	for (i = 0; i < DCAM_PATH_MAX; i++) {
		path = &pctx->path[i];
		if (atomic_read(&path->user_cnt) < 1 || atomic_read(&path->is_shutoff) > 0)
			continue;
		if (i == DCAM_PATH_FULL || i == DCAM_PATH_BIN) {
			if (pctx->rps == 1 && !pctx->virtualsensor)
				loose_val = (loose_val | (path->pack_bits));
			else
				loose_val = path->pack_bits;
		}
		path->pack_bits = loose_val;
		val_4in1 = ((pctx->is_4in1) | (path->is_4in1));
	}

	fetch = &pctx->fetch;
	fetch->pack_bits = pctx->pack_bits;
	if (val_4in1 == 1) {
		if (pctx->rps == 1)
			fetch->pack_bits = loose_val;
		else
			fetch->pack_bits = 0;
	}
	pr_info("pack_bits =%d\n",fetch->pack_bits);
	fetch->endian = pframe->endian;
	fetch->pattern = pframe->pattern;
	fetch->size.w = pframe->width;
	fetch->size.h = pframe->height;
	fetch->trim.start_x = 0;
	fetch->trim.start_y = 0;
	fetch->trim.size_x = pframe->width;
	fetch->trim.size_y = pframe->height;
	fetch->addr.addr_ch0 = (uint32_t)pframe->buf.iova[0];

	return 0;
}

int dcam_offline_param_get(struct cam_hw_info *hw, struct dcam_sw_context *pctx,
			struct camera_frame *pframe)
{
	int ret = 0;

	ret = dcamoffline_fetch_info_get(pctx, pframe);

	return ret;
}

int dcam_offline_param_set(struct cam_hw_info *hw, struct dcam_sw_context *pctx, struct dcam_dev_param *pm)
{
	int i = 0, ret = 0;
	struct dcam_hw_fbc_ctrl fbc_arg;
	struct dcam_path_desc *path = NULL;
	struct dcam_hw_path_start patharg;
	struct dcam_hw_fetch_set fetcharg;
	struct dcam_fetch_info *fetch = NULL;
	struct dcam_hw_binning_4in1 binning;
	uint32_t multi_cxt = 0;

	fetch = &pctx->fetch;

	if (pctx->rps == 1) {
		pr_debug("hwsim:offline enable aem\n");
		pctx->ctx[pctx->cur_ctx_id].blk_pm.non_zsl_cap = 1;
		if (pm->aem.bypass == 0)
			atomic_set(&pctx->path[DCAM_PATH_AEM].user_cnt, 1);/* hwsim first loop need aem statis */
		if (pm->pdaf.bypass == 0)
			atomic_set(&pctx->path[DCAM_PATH_PDAF].user_cnt, 1);
		if (pm->afm.bypass == 0)
			atomic_set(&pctx->path[DCAM_PATH_AFM].user_cnt, 1);
		if (pm->afl.afl_info.bypass == 0)
			atomic_set(&pctx->path[DCAM_PATH_AFL].user_cnt, 0);
		if (pm->hist.bayerHist_info.hist_bypass == 0)
			atomic_set(&pctx->path[DCAM_PATH_HIST].user_cnt, 1);
		if (pm->lscm.bypass == 0)
			atomic_set(&pctx->path[DCAM_PATH_LSCM].user_cnt, 1);
		if ((pm->gtm[DCAM_GTM_PARAM_PRE].gtm_info.bypass_info.gtm_mod_en || pm->gtm[DCAM_GTM_PARAM_CAP].gtm_info.bypass_info.gtm_mod_en)
			&& (pm->gtm[DCAM_GTM_PARAM_PRE].gtm_calc_mode == GTM_SW_CALC))
			atomic_set(&pctx->path[DCAM_PATH_GTM_HIST].user_cnt, 1);
		if ((pm->rgb_gtm[DCAM_GTM_PARAM_PRE].rgb_gtm_info.bypass_info.gtm_mod_en || pm->rgb_gtm[DCAM_GTM_PARAM_CAP].rgb_gtm_info.bypass_info.gtm_mod_en)
			&& (pm->rgb_gtm[DCAM_GTM_PARAM_PRE].gtm_calc_mode == GTM_SW_CALC))
			atomic_set(&pctx->path[DCAM_PATH_GTM_HIST].user_cnt, 1);
	} else {
		atomic_set(&pctx->path[DCAM_PATH_AEM].user_cnt, 0);
		atomic_set(&pctx->path[DCAM_PATH_PDAF].user_cnt, 0);
		atomic_set(&pctx->path[DCAM_PATH_AFM].user_cnt, 0);
		atomic_set(&pctx->path[DCAM_PATH_AFL].user_cnt, 0);
		atomic_set(&pctx->path[DCAM_PATH_HIST].user_cnt, 0);
		atomic_set(&pctx->path[DCAM_PATH_LSCM].user_cnt, 0);
		if(pctx->dev->hw->ip_isp->rgb_gtm_support == 0)
			atomic_set(&pctx->path[DCAM_PATH_GTM_HIST].user_cnt, 0);
	}

	/*use for offline 4in1*/
	binning.binning_4in1_en = 0;
	binning.idx = pctx->hw_ctx_id;
	hw->dcam_ioctl(hw, DCAM_HW_CFG_BINNING_4IN1_SET, &binning);

	for (i = 0; i < DCAM_PATH_MAX; i++) {
		path = &pctx->path[i];
		if (atomic_read(&path->user_cnt) < 1 || atomic_read(&path->is_shutoff) > 0 || (pctx->virtualsensor && (i == DCAM_PATH_RAW && (pctx->need_dcam_raw || pctx->raw_alg_type  == RAW_ALG_AI_SFNR))))
			continue;
		path->size_update = 1;
		ret = dcam_path_store_frm_set(pctx, path); /* TODO: */
		if (ret == 0) {
			/* interrupt need > 1 */
			atomic_set(&path->set_frm_cnt, 1);
			atomic_inc(&path->set_frm_cnt);
			patharg.path_id = i;
			patharg.idx = pctx->hw_ctx_id;
			patharg.slowmotion_count = pctx->slowmotion_count;
			patharg.pdaf_path_eb = 0;
			patharg.cap_info = pctx->cap_info;
			patharg.cap_info.format = DCAM_CAP_MODE_MAX;
			patharg.pack_bits = pctx->path[i].pack_bits;
			patharg.src_sel = pctx->path[i].src_sel;
			patharg.bayer_pattern = pctx->path[i].bayer_pattern;
			patharg.in_trim = pctx->path[i].in_trim;
			patharg.endian = pctx->path[i].endian;
			patharg.out_fmt = pctx->path[i].out_fmt;
			patharg.is_pack = pctx->path[i].is_pack;
			patharg.data_bits = pctx->path[i].data_bits;
			hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_START, &patharg);

			if (pctx->path[i].fbc_mode) {
				fbc_arg.idx = pctx->hw_ctx_id;
				fbc_arg.path_id = i;
				fbc_arg.fmt = pctx->path[i].out_fmt;
				fbc_arg.data_bits = pctx->path[i].data_bits;
				fbc_arg.fbc_mode = pctx->path[i].fbc_mode;
				hw->dcam_ioctl(hw, DCAM_HW_CFG_FBC_CTRL, &fbc_arg);
			}
		} else {
			pr_err("fail to set dcam%d path%d store frm\n",
				pctx->hw_ctx_id, path->path_id);
			return -EFAULT;
		}
	}

	multi_cxt = atomic_read(&pctx->dev->user_cnt);
	multi_cxt = (multi_cxt < 2) ? 0 : 1;
	/* bypass all blks and then set all blks to current pm */
	hw->dcam_ioctl(hw, DCAM_HW_CFG_BLOCKS_SETSTATIS, pm);
	hw->dcam_ioctl(hw, DCAM_HW_CFG_BLOCKS_SETALL, pm);

	/* for L3 DCAM1 */
	if (DCAM_FETCH_TWICE(pctx)) {
		struct dcam_hw_fetch_block blockarg;
		blockarg.idx = pctx->hw_ctx_id;
		blockarg.raw_fetch_count = pctx->raw_fetch_count;
		hw->dcam_ioctl(hw, DCAM_HW_CFG_FETCH_BLOCK_SET, &blockarg);
	}

	fetcharg.idx = pctx->hw_ctx_id;
	fetcharg.fetch_info = fetch;
	ret = hw->dcam_ioctl(hw, DCAM_HW_CFG_FETCH_SET, &fetcharg);
	return ret;
}

int dcam_offline_slices_proc(struct cam_hw_info *hw, struct dcam_sw_context *pctx,
	struct camera_frame *pframe, struct dcam_dev_param *pm)
{
	int i = 0, ret = 0;
	uint32_t force_ids = DCAM_CTRL_ALL;
	struct cam_hw_reg_trace trace;
	struct dcam_hw_slice_fetch slicearg;
	struct dcam_hw_force_copy copyarg;
	struct dcam_fetch_info *fetch = NULL;

	fetch = &pctx->fetch;
	for (i = 0; i < pctx->slice_num; i++) {
		ret = wait_for_completion_interruptible_timeout(
			&pctx->slice_done, DCAM_OFFLINE_TIMEOUT);
		if (ret <= 0) {
			pr_err("fail to wait as dcam%d offline timeout. ret: %d\n", pctx->hw_ctx_id, ret);
			trace.type = NORMAL_REG_TRACE;
			trace.idx = pctx->hw_ctx_id;
			hw->isp_ioctl(hw, ISP_HW_CFG_REG_TRACE, &trace);
			dcam_core_offline_debug_dump(pctx, pm, pframe);
			return -EFAULT;
		}

		pctx->cur_slice = &pctx->slice_info.slice_trim[i];
		pr_info("slice%d/%d proc start\n", i, pctx->slice_num);

		if (hw->ip_dcam[pctx->hw_ctx_id]->offline_slice_support && pctx->slice_num > 1) {
			slicearg.idx = pctx->hw_ctx_id;
			slicearg.fetch = fetch;
			slicearg.cur_slice = pctx->cur_slice;
			slicearg.slice_trim = pctx->slice_trim;
			slicearg.dcam_slice_mode = pctx->dcam_slice_mode;
			slicearg.slice_num = pctx->slice_num;
			slicearg.slice_count = pctx->slice_count;
			slicearg.st_pack = pctx->path[DCAM_PATH_BIN].is_pack;
			slicearg.fbc_info = pctx->fbc_info;
			pr_info("slc%d, (%d %d %d %d)\n", i,
				pctx->slice_info.slice_trim[i].start_x, pctx->slice_info.slice_trim[i].start_y,
				pctx->slice_info.slice_trim[i].size_x, pctx->slice_info.slice_trim[i].size_y);

			hw->dcam_ioctl(hw, DCAM_HW_CFG_SLICE_FETCH_SET, &slicearg);
		}

		mutex_lock(&pm->lsc.lsc_lock);
		if (i == 0) {
			ret = dcam_init_lsc(pm, 0);
			if (ret < 0) {
				mutex_unlock(&pm->lsc.lsc_lock);
				return ret;
			}
		} else
			dcam_init_lsc_slice(pm, 0);
		mutex_unlock(&pm->lsc.lsc_lock);

		/* DCAM_CTRL_COEF will always set in dcam_init_lsc() */
		copyarg.id = force_ids;
		copyarg.idx = pctx->hw_ctx_id;
		copyarg.glb_reg_lock = pctx->glb_reg_lock;
		hw->dcam_ioctl(hw, DCAM_HW_CFG_FORCE_COPY, &copyarg);
		udelay(500);
		pctx->iommu_status = (uint32_t)(-1);
		pctx->err_count = 1;
		atomic_set(&pctx->state, STATE_RUNNING);

		if (i == 0) {
			trace.type = NORMAL_REG_TRACE;
			trace.idx = pctx->hw_ctx_id;
			hw->isp_ioctl(hw, ISP_HW_CFG_REG_TRACE, &trace);
			dcam_core_offline_debug_dump(pctx, pm, pframe);
		}

		/* start fetch */
		if (pctx->dcamsec_eb)
			pr_warn("warning: camca : dcam%d sec_eb= %d, fetch disable\n",
				pctx->hw_ctx_id, pctx->dcamsec_eb);
		else
			hw->dcam_ioctl(hw, DCAM_HW_CFG_FETCH_START, hw);
	}
	return ret;
}

int dcam_offline_slice_info_cal(struct dcam_sw_context *pctx, struct camera_frame *pframe, uint32_t lbuf_width)
{
	int ret = 0;
	uint32_t slice_no = 0;

	if (pctx->dcam_slice_mode != CAM_OFFLINE_SLICE_SW)
		pctx->dcam_slice_mode = CAM_OFFLINE_SLICE_HW;

	if (pctx->dcam_slice_mode == CAM_OFFLINE_SLICE_HW)
		ret = dcam_core_hw_slices_set(pctx, pframe, lbuf_width);
	if (pctx->dcam_slice_mode == CAM_OFFLINE_SLICE_SW) {
		for (slice_no = 0; slice_no < pctx->slice_num; slice_no++)
			dcam_core_slice_trim_get(pframe->width, pframe->height, pctx->slice_num,
				slice_no, &pctx->slice_info.slice_trim[slice_no]);

	}
	pctx->slice_count = pctx->slice_num;
	return 0;
}

/* prepare cmd queue for fmcu slice */
int dcam_offline_slice_fmcu_cmds_set(struct dcam_fmcu_ctx_desc *fmcu, struct dcam_sw_context *pctx)
{
	return 0;
}
