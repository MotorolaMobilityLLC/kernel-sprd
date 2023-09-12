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

#ifndef _ISP_BLOCK_HEADER_H_
#define _ISP_BLOCK_HEADER_H_

#include "isp_drv.h"
#include <sprd_mm.h>

int32_t isp_k_cfg_pgg(struct isp_io_param *param);
int32_t isp_k_cfg_blc(struct isp_io_param *param);
int32_t isp_k_cfg_rgb_gain(struct isp_io_param *param);
int32_t isp_k_cfg_2d_lsc(struct isp_io_param *param,
	struct isp_k_block *isp_k_param);
int32_t isp_k_cfg_binning(struct isp_io_param *param);
int32_t isp_k_cfg_awbc(struct isp_io_param *param);
int32_t isp_k_cfg_raw_aem(struct isp_io_param *param);
int32_t isp_k_cfg_bpc(struct isp_io_param *param);
int32_t isp_k_cfg_bdn(struct isp_io_param *param);
int32_t isp_k_cfg_grgb(struct isp_io_param *param);
int32_t isp_k_cfg_nlm(struct isp_io_param *param,
	struct isp_k_block *isp_k_param);
int32_t isp_k_cfg_cfa(struct isp_io_param *param);
int32_t isp_k_cfg_cmc10(struct isp_io_param *param);
int32_t isp_k_cfg_gamma(struct isp_io_param *param,
	struct isp_k_block *isp_k_param);
int32_t isp_k_cfg_hsv(struct isp_io_param *param);
int32_t isp_k_cfg_rgb_afm(struct isp_io_param *param);
int32_t isp_k_cfg_cce(struct isp_io_param *param);
int32_t isp_k_cfg_pre_cdn_rgb(struct isp_io_param *param);
int32_t isp_k_cfg_yuv_precdn(struct isp_io_param *param);
int32_t isp_k_cfg_posterize(struct isp_io_param *param);
int32_t isp_k_cfg_anti_flicker(struct isp_io_param *param);
int isp_k_cfg_anti_flicker_new(struct isp_io_param *param);
int32_t isp_k_cfg_prefilter(struct isp_io_param *param);
int32_t isp_k_cfg_brightness(struct isp_io_param *param);
int32_t isp_k_cfg_hist(struct isp_io_param *param);
int32_t isp_k_cfg_hist2(struct isp_io_param *param);
int32_t isp_k_cfg_yuv_cdn(struct isp_io_param *param);
int32_t isp_k_cfg_edge(struct isp_io_param *param);
int32_t isp_k_cfg_css(struct isp_io_param *param);
int32_t isp_k_cfg_csa(struct isp_io_param *param);
int32_t isp_k_cfg_post_cdn(struct isp_io_param *param);
int32_t isp_k_cfg_hue(struct isp_io_param *param);
int32_t isp_k_cfg_ydelay(struct isp_io_param *param);
int32_t isp_k_cfg_ygamma(struct isp_io_param *param,
	struct isp_k_block *isp_k_param);
int32_t isp_k_cfg_iircnr(struct isp_io_param *param);
int32_t isp_k_cfg_fetch(struct isp_io_param *param,
	struct isp_k_block *isp_k_param);
int32_t isp_k_cfg_store(struct isp_io_param *param,
	struct isp_k_block *isp_k_param);
int32_t isp_k_cfg_dispatch(struct isp_io_param *param);
int32_t isp_k_cfg_arbiter(struct isp_io_param *param);
int32_t isp_k_cfg_common(struct isp_io_param *param);
enum isp_dev_afl_sel isp_k_common_afl_get(void);
int32_t isp_k_raw_afm_statistic_r6p9(char *afm_buf);
int32_t isp_k_hist_statistic_r6p9(uint64_t *addr);
void isp_k_ae_shadow_ctrl(void);
void isp_k_af_shadow_ctrl(void);
void isp_k_afl_shadow_ctrl(void);
#endif
