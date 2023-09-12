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

#ifndef _CAM_DEBUG_H
#define _CAM_DEBUG_H

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/fs.h>

#define ISP_SBLK_MAP_CNT 3 /* bitmaps of all isp_sblk */
#define CAM_DBG_FORCE_DUMP_REGS 1
#define CAM_DBG_SWCTRL_DUMP_REGS 0

enum dbg_sw_id {
	ISP_DBG_SW = 0x1000,
	ISP_FMCU_DBG_SW,
	ISP_INT_DBG_SW,
	ISP_DUMP_INPUT,
};

enum isp_sblk_byp_flag {
	SBLK_WORK,
	SBLK_BYPASS,
	SBLK_BYP_FLAG_CNT,
};

enum isp_sblk {
	/* RAW RGB */
	raw_pgg,
	raw_blc,
	raw_rgbg,
	raw_rgbd,
	raw_postblc,
	raw_nlc,
	raw_2dlsc,
	raw_bin,
	raw_awb,
	raw_aem,
	raw_bpc,
	raw_grgbc,
	raw_vst,
	raw_nlm,
	raw_ivst,
	raw_rlsc,
	raw_afm,
	all_raw,

	/* FULL RGB */
	full_cmc,
	full_gama,
	full_hsv,
	full_pstrz,
	full_uvd,
	all_full,

	/* YUV */
	yuv_afl,
	yuv_precdn,
	yuv_ynr,
	yuv_brta,
	yuv_cnta,
	yuv_hist,
	yuv_hist2,
	yuv_cdn,
	yuv_edge,
	yuv_csa,
	yuv_hua,
	yuv_postcdn,
	yuv_gama,
	yuv_iircnr,
	yuv_random,
	yuv_nf,
	all_yuv,

	ISP_SBLK_CNT,
};

void isp_dbg_s_ori_byp(uint32_t byp_flag, uint32_t bid, uint32_t iid);
void isp_dbg_bypass_sblk(void *isp_dev, uint32_t idx);
void isp_dbg_reg_trace(void *isp_dev, uint32_t idx);
void isp_dbg_dump_fmcu_cmd_q(void *isp_dev);
bool isp_dbg_check_switch_on(void *isp_dev, enum dbg_sw_id id);
int isp_dbg_dump_input_init(void *isp_dev);
int isp_dbg_dump_input_deinit(void *isp_dev);
void isp_dbg_trigger_dump_input(void *isp_dev, void *frame, uint32_t idx);
void dcam_dbg_reg_trace(void *cam_dev, uint32_t force_dump_flag);

int cam_dbg_init(void *miscdev);
int cam_dbg_deinit(void *miscdev);

extern struct attribute_group isp_dbg_sblk_attrs_group;
extern struct attribute_group isp_dbg_img_attrs_group;

#endif
