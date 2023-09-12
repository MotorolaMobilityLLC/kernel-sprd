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

#include "cam_types.h"
#include "cam_block.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "blk_comm: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int init_dcam_pm(struct dcam_dev_param *blk_pm_ctx)
{
	/* bypass all blocks by default */
	blk_pm_ctx->lsc.lens_info.bypass = 1;
	blk_pm_ctx->blc.blc_info.bypass = 1;
	blk_pm_ctx->rgb.gain_info.bypass = 1;
	blk_pm_ctx->rgb.rgb_dither.random_bypass = 1;
	blk_pm_ctx->hist.bayerHist_info.hist_bypass = 1;
	blk_pm_ctx->aem.bypass = 1;
	blk_pm_ctx->afl.afl_info.bypass = 1;
	blk_pm_ctx->awbc.awbc_info.awbc_bypass = 1;
	blk_pm_ctx->pdaf.ppe_ppc_info.ppi_bypass = 1;
	blk_pm_ctx->bpc.bpc_param.bpc_info.bpc_bypass = 1;
	blk_pm_ctx->bpc.bpc_param.bpc_info_l3.bpc_bypass = 1;
	blk_pm_ctx->grgb.grgb_info.bypass = 1;
	blk_pm_ctx->nr3.nr3_me.bypass = 1;
	blk_pm_ctx->afm.bypass = 1;
	blk_pm_ctx->lscm.bypass = 1;
	blk_pm_ctx->pdaf.bypass = 1;
	blk_pm_ctx->gtm[0].update_en = 0;
	blk_pm_ctx->gtm[0].gtm_info.bypass_info.gtm_mod_en = 0;
	blk_pm_ctx->gtm[0].gtm_info.bypass_info.gtm_map_bypass = 1;
	blk_pm_ctx->gtm[0].gtm_info.bypass_info.gtm_hist_stat_bypass = 1;
	blk_pm_ctx->gtm[1].update_en = 0;
	blk_pm_ctx->gtm[1].gtm_info.bypass_info.gtm_mod_en = 0;
	blk_pm_ctx->gtm[1].gtm_info.bypass_info.gtm_map_bypass = 1;
	blk_pm_ctx->gtm[1].gtm_info.bypass_info.gtm_hist_stat_bypass = 1;

	return 0;
}

int dcam_k_dump_pm(void *pdst, void *psrc)
{
	int size;
	struct dcam_param_data_l3 *pm_data;
	struct dcam_dev_param *blk_dcam_pm;

	if (!pdst || !psrc)
		return -1;
	pm_data = (struct dcam_param_data_l3 *)pdst;
	blk_dcam_pm = (struct dcam_dev_param *)psrc;

	memcpy(&pm_data->lens_info,
		&blk_dcam_pm->lsc.lens_info,
		sizeof(struct dcam_dev_lsc_info));
	memcpy(&pm_data->weight_tab[0],
		blk_dcam_pm->lsc.weight_tab,
		blk_dcam_pm->lsc.weight_tab_size);
	memcpy(&pm_data->lsc_tab,
		(void *)blk_dcam_pm->lsc.buf.addr_k[0],
		blk_dcam_pm->lsc.lens_info.gridtab_len);

	memcpy(&pm_data->blc_info,
		&blk_dcam_pm->blc.blc_info,
		sizeof(struct dcam_dev_blc_info));

	memcpy(&pm_data->gain_info,
		&blk_dcam_pm->rgb.gain_info,
		sizeof(struct dcam_dev_rgb_gain_info));

	memcpy(&pm_data->rgb_dither,
		&blk_dcam_pm->rgb.rgb_dither,
		sizeof(struct dcam_dev_rgb_dither_info));

	memcpy(&pm_data->awbc_info,
		&blk_dcam_pm->awbc.awbc_info,
		sizeof(struct dcam_dev_awbc_info));

	memcpy(&pm_data->bpc_info_l3,
		&blk_dcam_pm->bpc.bpc_param.bpc_info_l3,
		sizeof(struct dcam_dev_bpc_info_l3));

	memcpy(&pm_data->nr3_me,
		&blk_dcam_pm->nr3.nr3_me,
		sizeof(struct dcam_dev_3dnr_me));

	memcpy(&pm_data->grgb_info,
		&blk_dcam_pm->grgb.grgb_info,
		sizeof(struct isp_dev_grgb_info));

	size = (int)sizeof(struct dcam_param_data_l3);
	size = ((size + 15) & (~15));

	return size;
}

int init_isp_pm(struct isp_k_block *isp_k_param)
{
	isp_k_param->nlm_info_base.bypass = 1;
	isp_k_param->nr3_info_base.blend.bypass = 1;

	/* sharkl3 only */
	isp_k_param->brightness_info.bypass = 1;
	isp_k_param->contrast_info.bypass = 1;
	isp_k_param->csa_info.bypass = 1;
	isp_k_param->hue_info.bypass = 1;
	/* sharkl3 diff */
	isp_k_param->pstrz_info.bypass = 1;
	isp_k_param->uvd_info.bypass = 1;
	isp_k_param->ynr_info.bypass = 1;

	/* common */
	isp_k_param->nr3d_info.blend.bypass = 1;
	isp_k_param->cce_info.bypass = 1;
	isp_k_param->pre_cdn_info.bypass = 1;
	isp_k_param->cdn_info.bypass = 1;
	isp_k_param->post_cdn_info.bypass = 1;
	isp_k_param->cfa_info.bypass = 1;
	isp_k_param->cmc10_info.bypass = 1;
	isp_k_param->edge_info.bypass = 1;
	isp_k_param->gamma_info.bypass = 1;
	isp_k_param->hsv_info.bypass = 1;
	isp_k_param->iircnr_info.bypass = 1;
	isp_k_param->nlm_info.bypass = 1;
	isp_k_param->ygamma_info.bypass = 1;
	isp_k_param->yrandom_info.bypass = 1;
	isp_k_param->nf_info.yrandom_bypass = 1;

	return 0;
}

int isp_k_dump_pm(void *pdst, void *psrc)
{
	int size;
	struct isp_param_data_l3 *pm_data;
	struct isp_k_block *isp_k_param;

	pm_data = (struct isp_param_data_l3 *)pdst;
	isp_k_param = (struct isp_k_block *)psrc;

	/* sharkl3 only */
	memcpy(&pm_data->brightness_info,
		&isp_k_param->brightness_info,
		sizeof(struct isp_dev_brightness_info));

	memcpy(&pm_data->contrast_info,
		&isp_k_param->contrast_info,
		sizeof(struct isp_dev_contrast_info));

	memcpy(&pm_data->csa_info,
		&isp_k_param->csa_info,
		sizeof(struct isp_dev_csa_info));

	memcpy(&pm_data->hue_info,
		&isp_k_param->hue_info,
		sizeof(struct isp_dev_hue_info_l3));

	/* sharkl3 diff */
	memcpy(&pm_data->pstrz_info,
		&isp_k_param->pstrz_info,
		sizeof(struct isp_dev_posterize_info));

	memcpy(&pm_data->uvd_info,
		&isp_k_param->uvd_info,
		sizeof(struct isp_dev_uvd_info));

	memcpy(&pm_data->ynr_info,
		&isp_k_param->ynr_info,
		sizeof(struct isp_dev_ynr_info));

	/* common */
	memcpy(&pm_data->nr3d_info,
		&isp_k_param->nr3d_info,
		sizeof(struct isp_dev_3dnr_info));

	memcpy(&pm_data->cce_info,
		&isp_k_param->cce_info,
		sizeof(struct isp_dev_cce_info));

	memcpy(&pm_data->pre_cdn_info,
		&isp_k_param->pre_cdn_info,
		sizeof(struct isp_dev_pre_cdn_info));

	memcpy(&pm_data->cdn_info,
		&isp_k_param->cdn_info,
		sizeof(struct isp_dev_cdn_info));

	memcpy(&pm_data->post_cdn_info,
		&isp_k_param->post_cdn_info,
		sizeof(struct isp_dev_post_cdn_info));

	memcpy(&pm_data->cfa_info,
		&isp_k_param->cfa_info,
		sizeof(struct isp_dev_cfa_info));

	memcpy(&pm_data->cmc10_info,
		&isp_k_param->cmc10_info,
		sizeof(struct isp_dev_cmc10_info));

	memcpy(&pm_data->edge_info,
		&isp_k_param->edge_info,
		sizeof(struct isp_dev_edge_info_v2));

	memcpy(&pm_data->gamma_info,
		&isp_k_param->gamma_info,
		sizeof(struct isp_dev_gamma_info));

	memcpy(&pm_data->hsv_info,
		&isp_k_param->hsv_info,
		sizeof(struct isp_dev_hsv_info_v2));

	memcpy(&pm_data->iircnr_info,
		&isp_k_param->iircnr_info,
		sizeof(struct isp_dev_iircnr_info));

	memcpy(&pm_data->nlm_info,
		&isp_k_param->nlm_info,
		sizeof(struct isp_dev_nlm_info_v2));

	memcpy(&pm_data->ygamma_info,
		&isp_k_param->ygamma_info,
		sizeof(struct isp_dev_ygamma_info));

	memcpy(&pm_data->yrandom_info,
		&isp_k_param->yrandom_info,
		sizeof(struct isp_dev_yrandom_info));

	memcpy(&pm_data->nf_info,
		&isp_k_param->nf_info,
		sizeof(struct isp_dev_noise_filter_info));

	memcpy(pm_data->vst_buf, isp_k_param->vst_buf, sizeof(pm_data->vst_buf));
	memcpy(pm_data->ivst_buf, isp_k_param->ivst_buf, sizeof(pm_data->ivst_buf));

	size = (int)sizeof(struct isp_param_data_l3);
	size = ((size + 15) & (~15));

	return size;
}
