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

#include <linux/types.h>
#include <linux/kernel.h>
#include <sprd_mm.h>
#include "isp_hw.h"
#include "sprd_img.h"

#include "cam_trusty.h"
#include "cam_scaler.h"
#include "cam_scaler_ex.h"
#include "isp_core.h"
#include "isp_path.h"
#include "isp_pyr_rec.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_PATH: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define ISP_PATH_DECI_FAC_MAX       4
#define ISP_SC_COEFF_UP_MAX         ISP_SCALER_UP_MAX
#define ISP_SC_COEFF_DOWN_MAX       ISP_SCALER_UP_MAX

static uint32_t isppath_deci_factor_get(uint32_t src_size, uint32_t dst_size)
{
	uint32_t factor = 0;

	if (0 == src_size || 0 == dst_size)
		return factor;

	/* factor: 0 - 1/2, 1 - 1/4, 2 - 1/8, 3 - 1/16 */
	for (factor = 0; factor < ISP_PATH_DECI_FAC_MAX; factor++) {
		if (src_size < (uint32_t) (dst_size * (1 << (factor + 1))))
			break;
	}

	return factor;
}

int isp_path_slw960_uinfo_set(struct isp_sw_context *pctx, void *param)
{
	struct isp_uinfo *uinfo = NULL;
	struct isp_ctx_base_desc *cfg_in = NULL;

	if (!pctx || !param) {
		pr_err("fail to get valid input ptr, pctx %p, param %p\n",
			pctx, param);
		return -EFAULT;
	}
	uinfo = &pctx->uinfo;
	cfg_in = (struct isp_ctx_base_desc *)param;
	if (uinfo->enable_slowmotion) {
		uinfo->stage_a_frame_num = cfg_in->slowmotion_stage_a_num;
		uinfo->stage_a_valid_count = cfg_in->slowmotion_stage_a_valid_num;
		uinfo->stage_b_frame_num = cfg_in->slowmotion_stage_b_num;
		uinfo->stage_c_frame_num = cfg_in->slowmotion_stage_a_num;
	}
	pr_debug("isp%d, slw enable %d, stage_a_frame_num %d, stage_a_valid_count %d, stage_b_frame_num %d\n",
		pctx->ctx_id, uinfo->enable_slowmotion, uinfo->stage_a_frame_num, uinfo->stage_a_valid_count,
		uinfo->stage_b_frame_num);
	return 0;
}

int isp_path_comn_uinfo_set(struct isp_sw_context *pctx, void *param)
{
	int ret = 0;
	struct isp_ctx_base_desc *cfg_in = NULL;
	struct isp_uinfo *uinfo = NULL;
	struct isp_ltm_ctx_desc *rgb_ltm = NULL;
	struct isp_ltm_ctx_desc *yuv_ltm = NULL;
	struct isp_rec_ctx_desc *rec_ctx = NULL;
	struct isp_gtm_ctx_desc *rgb_gtm = NULL;

	if (!pctx || !param) {
		pr_err("fail to get valid input ptr, pctx %p, param %p\n",
			pctx, param);
		return -EFAULT;
	}

	cfg_in = (struct isp_ctx_base_desc *)param;
	uinfo = &pctx->uinfo;
	rgb_ltm = (struct isp_ltm_ctx_desc *)pctx->rgb_ltm_handle;
	yuv_ltm = (struct isp_ltm_ctx_desc *)pctx->yuv_ltm_handle;
	rec_ctx = (struct isp_rec_ctx_desc *)pctx->rec_handle;
	rgb_gtm = (struct isp_gtm_ctx_desc *)pctx->rgb_gtm_handle;

	if (uinfo->enable_slowmotion) {
		uinfo->enable_slowmotion = cfg_in->enable_slowmotion;
		uinfo->slowmotion_count = cfg_in->slowmotion_count;
	}
	uinfo->in_fmt = cfg_in->in_fmt;
	uinfo->pack_bits = cfg_in->pack_bits;
	uinfo->bayer_pattern = cfg_in->bayer_pattern;
	uinfo->ltm_rgb = cfg_in->ltm_rgb;
	uinfo->ltm_yuv = cfg_in->ltm_yuv;
	uinfo->slw_state = cfg_in->slw_state;
	uinfo->mode_ltm = cfg_in->mode_ltm;
	uinfo->gtm_rgb = cfg_in->gtm_rgb;
	uinfo->mode_gtm = cfg_in->mode_gtm;
	uinfo->mode_3dnr = cfg_in->mode_3dnr;
	uinfo->is_pack = cfg_in->is_pack;
	uinfo->data_in_bits = cfg_in->data_in_bits;
	uinfo->pyr_data_in_bits = cfg_in->pyr_data_bits;
	uinfo->pyr_is_pack = cfg_in->pyr_is_pack;
	if (cfg_in->data_in_bits == DCAM_STORE_8_BIT)
		uinfo->is_pack = 0;
	if (rgb_ltm) {
		rgb_ltm->ltm_ops.core_ops.cfg_param(rgb_ltm, ISP_LTM_CFG_EB, &uinfo->ltm_rgb);
		rgb_ltm->ltm_ops.core_ops.cfg_param(rgb_ltm, ISP_LTM_CFG_MODE, &uinfo->mode_ltm);
		rgb_ltm->ltm_ops.sync_ops.set_status(rgb_ltm, 1);
	}
	if (yuv_ltm) {
		yuv_ltm->ltm_ops.core_ops.cfg_param(yuv_ltm, ISP_LTM_CFG_EB, &uinfo->ltm_yuv);
		yuv_ltm->ltm_ops.core_ops.cfg_param(yuv_ltm, ISP_LTM_CFG_MODE, &uinfo->mode_ltm);
		yuv_ltm->ltm_ops.sync_ops.set_status(yuv_ltm, 1);
	}
	if (rec_ctx)
		rec_ctx->ops.cfg_param(rec_ctx, ISP_REC_CFG_DEWARPING_EB, &uinfo->is_dewarping);
	if (rgb_gtm) {
		rgb_gtm->gtm_ops.cfg_param(rgb_gtm, ISP_GTM_CFG_EB, &uinfo->gtm_rgb);
		rgb_gtm->gtm_ops.cfg_param(rgb_gtm, ISP_GTM_CFG_MODE, &uinfo->mode_gtm);
	}

	pctx->ch_id = cfg_in->ch_id;
	pctx->uinfo.sn_size = cfg_in->sn_size;

	pr_debug("ctx%d, in_fmt 0x%x, %d %d mode_ltm %d ltm_eb %d, mode_gtm %d gtm_eb %d,slw_state %d 3dnr: %d\n",
		pctx->ctx_id, uinfo->in_fmt, uinfo->pack_bits, uinfo->bayer_pattern, uinfo->mode_ltm,
		uinfo->ltm_rgb, uinfo->mode_gtm, uinfo->gtm_rgb, uinfo->slw_state, uinfo->mode_3dnr);
	return ret;
}

int isp_path_scaler_param_calc(struct img_trim *in_trim,
	struct img_size *out_size, struct yuv_scaler_info *scaler,
	struct img_deci_info *deci)
{
	int ret = 0;
	unsigned int tmp_dstsize = 0;
	unsigned int align_size = 0;
	unsigned int d_max = ISP_SC_COEFF_DOWN_MAX;
	unsigned int u_max = ISP_SC_COEFF_UP_MAX;
	unsigned int f_max = ISP_PATH_DECI_FAC_MAX;

	pr_debug("in_trim_size_x:%d, in_trim_size_y:%d, out_size_w:%d,out_size_h:%d\n",
		in_trim->size_x, in_trim->size_y, out_size->w, out_size->h);
	/* check input crop limit with max scale up output size(2 bit aligned) */
	if (in_trim->size_x > (out_size->w * d_max * (1 << f_max)) ||
		in_trim->size_y > (out_size->h * d_max * (1 << f_max)) ||
		in_trim->size_x < ISP_DIV_ALIGN_W(out_size->w, u_max) ||
		in_trim->size_y < ISP_DIV_ALIGN_H(out_size->h, u_max)) {
		pr_err("fail to get in_trim %d %d. out _size %d %d, fmax %d, u_max %d\n",
				in_trim->size_x, in_trim->size_y,
				out_size->w, out_size->h, f_max, d_max);
		ret = -EINVAL;
	} else {
		scaler->scaler_factor_in = in_trim->size_x;
		scaler->scaler_ver_factor_in = in_trim->size_y;
		if (in_trim->size_x > out_size->w * d_max) {
			tmp_dstsize = out_size->w * d_max;
			deci->deci_x = isppath_deci_factor_get(in_trim->size_x, tmp_dstsize);
			deci->deci_x_eb = 1;
			align_size = (1 << (deci->deci_x + 1)) * ISP_PIXEL_ALIGN_WIDTH;
			in_trim->size_x = (in_trim->size_x) & ~(align_size - 1);
			in_trim->start_x = (in_trim->start_x) & ~(align_size - 1);
			scaler->scaler_factor_in = in_trim->size_x >> (deci->deci_x + 1);
		} else {
			deci->deci_x = 1;
			deci->deci_x_eb = 0;
		}

		if (in_trim->size_y > out_size->h * d_max) {
			tmp_dstsize = out_size->h * d_max;
			deci->deci_y = isppath_deci_factor_get(in_trim->size_y, tmp_dstsize);
			deci->deci_y_eb = 1;
			align_size = (1 << (deci->deci_y + 1)) * ISP_PIXEL_ALIGN_HEIGHT;
			in_trim->size_y = (in_trim->size_y) & ~(align_size - 1);
			in_trim->start_y = (in_trim->start_y) & ~(align_size - 1);
			scaler->scaler_ver_factor_in = in_trim->size_y >> (deci->deci_y + 1);
		} else {
			deci->deci_y = 1;
			deci->deci_y_eb = 0;
		}
		pr_debug("end out_size  w %d, h %d\n",
			out_size->w, out_size->h);

		scaler->scaler_factor_out = out_size->w;
		scaler->scaler_ver_factor_out = out_size->h;
		scaler->scaler_out_width = out_size->w;
		scaler->scaler_out_height = out_size->h;
	}

	return ret;
}

int isp_path_fetchsize_update(struct isp_sw_context *pctx, void *param)
{
	int ret = 0;
	int invalid = 0;
	struct isp_ctx_size_desc *cfg_in;
	struct img_size *src;
	struct img_trim *intrim;

	if (!pctx || !param) {
		pr_err("fail to get valid input ptr, pctx %p, param %p\n",
			pctx, param);
		return -EFAULT;
	}

	cfg_in = (struct isp_ctx_size_desc *)param;
	src = &cfg_in->src;
	intrim = &cfg_in->crop;
	if (((intrim->start_x + intrim->size_x) > src->w) ||
		((intrim->start_y + intrim->size_y) > src->h))
		invalid |= 1;
	invalid |= ((intrim->start_x & 1) | (intrim->start_y & 1));
	if (invalid) {
		pr_err("fail to get valid ctx size. src %d %d, crop %d %d %d %d\n",
			src->w, src->h, intrim->start_x, intrim->size_y,
			intrim->size_x, intrim->size_y);
		return -EINVAL;
	}

	pctx->pipe_src.src = cfg_in->src;
	pctx->pipe_src.crop = cfg_in->crop;

	pr_debug("isp %d src %d %d crop %d %d %d %d\n",
		pctx->ctx_id, pctx->pipe_src.src.w, pctx->pipe_src.src.h,
		pctx->pipe_src.crop.start_x, pctx->pipe_src.crop.start_y,
		pctx->pipe_src.crop.size_x, pctx->pipe_src.crop.size_y);

	return ret;
}

int isp_path_fetch_uinfo_set(struct isp_sw_context *pctx, void *param)
{
	int ret = 0;
	int invalid = 0;
	struct isp_ctx_size_desc *cfg_in = NULL;
	struct isp_uinfo *uinfo = NULL;
	struct img_size *src = NULL;
	struct img_trim *crop = NULL;

	if (!pctx || !param) {
		pr_err("fail to get valid input ptr, pctx %p, param %p\n",
			pctx, param);
		return -EFAULT;
	}

	cfg_in = (struct isp_ctx_size_desc *)param;
	uinfo = &pctx->uinfo;
	src = &cfg_in->src;
	crop = &cfg_in->crop;
	if (((crop->start_x + crop->size_x) > src->w) ||
		((crop->start_y + crop->size_y) > src->h))
		invalid |= 1;
	invalid |= ((crop->start_x & 1) | (crop->start_y & 1));
	if (invalid) {
		pr_err("fail to get valid ctx size. src %d %d, crop %d %d %d %d\n",
			src->w, src->h, crop->start_x, crop->size_y,
			crop->size_x, crop->size_y);
		return -EINVAL;
	}

	uinfo->src = *src;
	uinfo->crop = *crop;
	uinfo->ori_src = *src;
	pctx->isp_k_param.src_w = uinfo->src.w;
	pctx->isp_k_param.src_h = uinfo->src.h;
	pctx->zoom_conflict_with_ltm = cfg_in->zoom_conflict_with_ltm;

	pr_debug("isp %d src %d %d crop %d %d %d %d\n",
		pctx->ctx_id, uinfo->src.w, uinfo->src.h,
		uinfo->crop.start_x, uinfo->crop.start_y,
		uinfo->crop.size_x, uinfo->crop.size_y);

	return ret;
}

int isp_path_fetch_compress_uinfo_set(struct isp_sw_context *pctx,
		void *param)
{
	struct isp_ctx_compress_desc *compress = NULL;
	struct isp_uinfo *uinfo = NULL;

	if (!pctx || !param) {
		pr_err("fail to get valid input ptr, pctx %p, param %p\n",
			pctx, param);
		return -EFAULT;
	}

	compress = (struct isp_ctx_compress_desc *)param;
	uinfo = &pctx->uinfo;
	uinfo->fetch_path_sel = compress->fetch_fbd;
	uinfo->fetch_fbd_4bit_bypass = compress->fetch_fbd_4bit_bypass;
	uinfo->nr3_fbc_fbd = compress->nr3_fbc_fbd;

	pr_debug("ctx %u, fetch_fbd %d, compress_3dnr %d\n",
		pctx->ctx_id, uinfo->fetch_path_sel, uinfo->nr3_fbc_fbd);

	return 0;
}

int isp_path_fetchsync_uinfo_set(struct isp_sw_context *pctx, void *param)
{
	struct isp_uinfo *uinfo = NULL;

	if (!pctx || !param) {
		pr_err("fail to get valid input ptr, pctx %p, param %p\n",
			pctx, param);
		return -EFAULT;
	}

	uinfo = &pctx->uinfo;
	uinfo->uframe_sync |= *(uint32_t *)param;
	pr_debug("ctx %u, uframe_sync %u\n", pctx->ctx_id, uinfo->uframe_sync);

	return 0;
}

int isp_path_fetch_frm_set(struct isp_sw_context *pctx,
		struct camera_frame *frame)
{
	int ret = 0;
	int planes;
	unsigned long offset_u, offset_v, yuv_addr[3] = {0};
	struct isp_hw_fetch_info *fetch = NULL;

	if (!pctx || !frame) {
		pr_err("fail to get valid pctx %p, frame %p\n", pctx, frame);
		return -EINVAL;
	}

	if (!frame->is_compressed) {
		fetch = &pctx->pipe_info.fetch;
		if (fetch->fetch_fmt == ISP_FETCH_YUV422_3FRAME)
			planes = 3;
		else if ((fetch->fetch_fmt == ISP_FETCH_YUV422_2FRAME)
				|| (fetch->fetch_fmt == ISP_FETCH_YVU422_2FRAME)
				|| (fetch->fetch_fmt == ISP_FETCH_YUV420_2FRAME)
				|| (fetch->fetch_fmt == ISP_FETCH_YVU420_2FRAME)
				|| (fetch->fetch_fmt == ISP_FETCH_YVU420_2FRAME_MIPI)
				|| (fetch->fetch_fmt == ISP_FETCH_YUV420_2FRAME_MIPI))
			planes = 2;
		else
			planes = 1;

		yuv_addr[0] = frame->buf.iova[0];
		yuv_addr[1] = frame->buf.iova[1];
		yuv_addr[2] = frame->buf.iova[2];

		if (planes > 1) {
			offset_u = fetch->pitch.pitch_ch0 * fetch->src.h;
			yuv_addr[1] = yuv_addr[0] + offset_u;
		}

		if ((planes > 2)) {
			offset_v = fetch->pitch.pitch_ch1 * fetch->src.h;
			yuv_addr[2] = yuv_addr[1] + offset_v;
		}

		/* set the start address of source frame */
		fetch->addr.addr_ch0 = yuv_addr[0];
		fetch->addr.addr_ch1 = yuv_addr[1];
		fetch->addr.addr_ch2 = yuv_addr[2];
		yuv_addr[0] += fetch->trim_off.addr_ch0;
		yuv_addr[1] += fetch->trim_off.addr_ch1;
		yuv_addr[2] += fetch->trim_off.addr_ch2;
		fetch->addr_hw.addr_ch0 = yuv_addr[0];
		fetch->addr_hw.addr_ch1 = yuv_addr[1];
		fetch->addr_hw.addr_ch2 = yuv_addr[2];
	} else {
		fetch = &pctx->pipe_info.fetch;
		fetch->addr_hw.addr_ch0 = frame->buf.iova[0];
		fetch->addr_hw.addr_ch1 = fetch->addr_hw.addr_ch0;
	}
	if (pctx->dev->sec_mode == SEC_SPACE_PRIORITY)
		cam_trusty_isp_fetch_addr_set(yuv_addr[0], yuv_addr[1], yuv_addr[2]);

	pr_debug("camca  isp sec_mode=%d,  %lx %lx %lx\n", pctx->dev->sec_mode,
		yuv_addr[0],
		yuv_addr[1],
		yuv_addr[2]);

	return ret;
}

int isp_path_storecrop_update(struct isp_path_uinfo *path, void *param)
{
	int ret = 0;
	struct img_trim *crop = NULL;

	if (!path || !param) {
		pr_err("fail to get valid input ptr, path %p, param %p\n", path, param);
		return -EFAULT;
	}

	crop = (struct img_trim *)param;
	path->in_trim = *crop;

	return ret;
}

int isp_path_storecomn_uinfo_set(struct isp_path_uinfo *path, void *param)
{
	int ret = 0;
	struct isp_path_base_desc *cfg_in = NULL;

	if (!path || !param) {
		pr_err("fail to get valid input ptr, path %p, param %p\n", path, param);
		return -EFAULT;
	}
	cfg_in = (struct isp_path_base_desc *)param;

	path->out_fmt = cfg_in->out_fmt;
	path->data_endian = cfg_in->endian;
	path->bind_type = cfg_in->slave_type;
	path->regular_mode = cfg_in->regular_mode;
	path->slave_path_id = cfg_in->slave_path_id;
	path->dst = cfg_in->output_size;
	path->data_in_bits = cfg_in->data_bits;
	pr_debug("isp %d dst w %d h %d\n", path->dst.w, path->dst.h);

	return ret;
}

int isp_path_storecrop_uinfo_set(struct isp_path_uinfo *path, void *param)
{
	int ret = 0;
	struct img_trim *crop = NULL;

	if (!path || !param) {
		pr_err("fail to get valid input ptr, path %p, param %p\n", path, param);
		return -EFAULT;
	}

	crop = (struct img_trim *)param;
	path->in_trim = *crop;
	pr_debug("path crop info %d, %d %d %d\n", path->in_trim.start_x,
		path->in_trim.start_y, path->in_trim.size_x, path->in_trim.size_y);

	return ret;
}

int isp_path_store_compress_uinfo_set(struct isp_path_uinfo *path, void *param)
{
	int ret = 0;
	struct isp_path_compression_desc *cfg_in = NULL;

	if (!path || !param) {
		pr_err("fail to get valid input ptr, path %p, param %p\n", path, param);
		return -EFAULT;
	}

	cfg_in = (struct isp_path_compression_desc *)param;
	path->store_fbc = cfg_in->store_fbc;

	return ret;
}

int isp_path_storeframe_sync_set(struct isp_path_uinfo *path, void *param)
{
	path->uframe_sync = *(uint32_t *)param;
	pr_debug("path, uframe_sync %u\n", path->uframe_sync);

	return 0;
}

int isp_path_store_frm_set(
		struct isp_path_desc *path,
		struct camera_frame *frame)
{
	int ret = 0;
	int idx;
	int planes;
	unsigned long offset_u, offset_v, yuv_addr[3] = {0};
	struct isp_sw_context *pctx = NULL;
	struct isp_store_info *store = NULL;

	if (!path || !frame) {
		pr_err("fail to get valid input ptr, path %p, frame %p\n",
			path, frame);
		return -EINVAL;
	}
	pr_debug("enter.\n");
	pctx = path->attach_ctx;
	store = &pctx->pipe_info.store[path->spath_id].store;
	idx = pctx->ctx_id;

	if (store->color_fmt == ISP_STORE_UYVY)
		planes = 1;
	else if ((store->color_fmt == ISP_STORE_YUV422_3FRAME)
			|| (store->color_fmt == ISP_STORE_YUV420_3FRAME))
		planes = 3;
	else
		planes = 2;

	if (frame->buf.iova[0] == 0) {
		pr_err("fail to get valid iova address, fd = 0x%x\n",
			frame->buf.mfd[0]);
		return -EINVAL;
	}

	yuv_addr[0] = frame->buf.iova[0];
	yuv_addr[1] = frame->buf.iova[1];
	yuv_addr[2] = frame->buf.iova[2];

	pr_debug("sw %d , fmt %d, planes %d addr %lx %lx %lx, pitch:%d\n",
		pctx->ctx_id, store->color_fmt, planes,
		yuv_addr[0], yuv_addr[1], yuv_addr[2], store->pitch.pitch_ch0);

	if ((planes > 1) && yuv_addr[1] == 0) {
		if (!pctx->sw_slice_num) {
			offset_u = store->pitch.pitch_ch0 * store->size.h;
			yuv_addr[1] = yuv_addr[0] + offset_u;
		} else {
			yuv_addr[1] = frame->buf.iova[0] + store->total_size * 2 / 3;
		}
	}

	if ((planes > 2) && yuv_addr[2] == 0) {
		offset_v = store->pitch.pitch_ch1 * store->size.h;
		if (store->color_fmt == ISP_STORE_YUV420_3FRAME)
			offset_v >>= 1;
		yuv_addr[2] = yuv_addr[1] + offset_v;
	}

	if (pctx->sw_slice_num) {
		yuv_addr[0] += store->slice_offset.addr_ch0;
		yuv_addr[1] += store->slice_offset.addr_ch1;
		yuv_addr[2] += store->slice_offset.addr_ch2;
	}
	pr_debug("path %d planes %d addr %lx %lx %lx\n",
		path->spath_id, planes, yuv_addr[0], yuv_addr[1], yuv_addr[2]);

	store->addr.addr_ch0 = yuv_addr[0];
	store->addr.addr_ch1 = yuv_addr[1];
	store->addr.addr_ch2 = yuv_addr[2];
	pr_debug("sw %d , done %x %x %x\n", pctx->ctx_id, store->addr.addr_ch0,
		store->addr.addr_ch1, store->addr.addr_ch2);

	return ret;
}

int isp_path_afbc_store_frm_set(
		struct isp_path_desc *path,
		struct camera_frame *frame)
{
	int ret = 0;
	int idx = 0;
	unsigned long yuv_addr[2] = {0};
	struct isp_sw_context *pctx = NULL;
	struct isp_afbc_store_info *afbc_store = NULL;
	struct cam_hw_info *hw = NULL;

	if (!path || !frame) {
		pr_err("fail to get valid input ptr, path %p, frame %p\n",
			path, frame);
		return -EINVAL;
	}
	pr_debug("afbc enter.\n");
	hw = path->hw;
	pctx = path->attach_ctx;
	afbc_store = &pctx->pipe_info.afbc[path->spath_id].afbc_store;
	idx = pctx->ctx_id;

	yuv_addr[0] = frame->buf.iova[0];
	yuv_addr[1] = frame->buf.iova[1];
	if (yuv_addr[1] == 0)
		yuv_addr[1] = yuv_addr[0] + afbc_store->header_offset;

	afbc_store->yheader = yuv_addr[0];
	afbc_store->yaddr = yuv_addr[1];

	pr_debug("path %d afbc done 0x%x 0x%x\n", path->spath_id,
		afbc_store->yheader, afbc_store->yaddr);

	return ret;
}
