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

#ifndef _ISP_PATH_H_
#define _ISP_PATH_H_

#define SHRINK_Y_UP_TH                  235
#define SHRINK_Y_DN_TH                  16
#define SHRINK_UV_UP_TH                 240
#define SHRINK_UV_DN_TH                 16
#define SHRINK_Y_OFFSET                 16
#define SHRINK_Y_RANGE                  3
#define SHRINK_C_OFFSET                 16
#define SHRINK_C_RANGE                  6

int isp_path_comn_uinfo_set(struct isp_sw_context *pctx, void *param);
int isp_path_fetch_uinfo_set(struct isp_sw_context *pctx, void *param);
int isp_path_fetch_compress_uinfo_set(struct isp_sw_context *pctx, void *param);
int isp_path_fetchsync_uinfo_set(struct isp_sw_context *pctx, void *param);

int isp_path_storecomn_uinfo_set(struct isp_path_uinfo *path, void *param);
int isp_path_storecrop_uinfo_set(struct isp_path_uinfo *path, void *param);
int isp_path_store_compress_uinfo_set(struct isp_path_uinfo *path, void *param);
int isp_path_storeframe_sync_set(struct isp_path_uinfo *path, void *param);

int isp_path_fetchsize_update(struct isp_sw_context *pctx, void *param);
int isp_path_storecrop_update(struct isp_path_uinfo *path, void *param);

int isp_path_fetch_frm_set(struct isp_sw_context *pctx, struct camera_frame *frame);
int isp_path_store_frm_set(struct isp_path_desc *path, struct camera_frame *frame);
int isp_path_afbc_store_frm_set(struct isp_path_desc *path, struct camera_frame *frame);

int isp_path_scaler_param_calc(struct img_trim *in_trim,
		struct img_size *out_size, struct yuv_scaler_info *scaler,
		struct img_deci_info *deci);
int isp_path_slw960_uinfo_set(struct isp_sw_context *pctx, void *param);

#endif
