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

#include "isp_drv.h"

/* 3A */
int isp_k_cfg_awb(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_raw_aem(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_binning(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_rgb_afm(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);

/* RAW RGB */
int isp_k_cfg_pgg(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_blc(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_post_blc(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_nlm(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_rgb_gain(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_nlc(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_2d_lsc(struct isp_io_param *param,
			struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_rlsc(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_bpc(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_grgb(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_rgb_dither(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);

/* FRGB 8*/
int isp_k_cfg_pstrz(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_cce(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_uvd(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);

/* FRGB 10 */
int isp_k_cfg_cfa(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_cmc10(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_gamma(struct isp_io_param *param,
			struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_hsv(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);

/* YIQ */
int isp_k_cfg_anti_flicker_new(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);

/* YUV */
int isp_k_cfg_pre_cdn(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_yuv_cdn(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_post_cdn(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_ynr(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_brightness(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_contrast(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_hist(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_hist2(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_edge(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_ygamma(struct isp_io_param *param,
			struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_csa(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_hue(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_ydelay(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_iircnr(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_noise_filter(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);

/* Other */
int isp_k_cfg_fetch(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_common(struct isp_io_param *param,
			enum isp_id idx, uint32_t is_raw_cap);
int isp_k_cfg_dispatch(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_arbiter(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx);
int isp_k_cfg_store(struct isp_io_param *param,
			enum isp_id idx, uint32_t is_raw_cap);
