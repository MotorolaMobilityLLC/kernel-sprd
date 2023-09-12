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

#ifndef _ISP_DEWARP_H_
#define _ISP_DEWARP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "cam_queue.h"
#include "dcam_interface.h"
#include "isp_interface.h"
#include "isp_slice.h"

#define DEWARPING_DST_MBLK_SIZE       16
#define DEWARPING_MAX_LINE_LENGTH     0x100
#define DEWARPING_GRID_BUF            (0x1180)
#define DEWARPING_GRID_DATA_SIZE      (1120)
#define MAX_GRID_SIZE_COEF1           192
#define MAX_DEWARP_INPUT_WIDTH        5184
#define MIN_DEWARP_INPUT_WIDTH        120

#define MAX_XY_DELTA                  (11)
#define MAX_GRID_SIZE                 (1 << 8)
/* 11:grid_size[1];7:grid_size[2~3];6:grid_size[4~5];5:grid_size[6~255];4:grid_size[256]; DST_MBLK_HEIGHT=8 */
#define TEMP_INTERP_ROW               (5)
/* dst block size */
#define DST_MBLK_SIZE                 (8)
/* 4 sides point of dst block:4*(DST_MBLK_SIZE-1) */
#define DST_MBLK_SIDE_POINT           ((DST_MBLK_SIZE - 1) << 2)

#define BLOCK_SCALE_THR               (4.0)
#define BLOCK_SCALE_USED              (BLOCK_SCALE_THR)
/* if more distortion, LXY_SHIFT maybe changed, but it defines 6 by now. */
#define LXY_SHIFT                     (3 + 3)
#define CXY_SHIFT                     (LXY_SHIFT + 1)
#define LXY_MULT                      (1 << LXY_SHIFT)
#define CXY_MULT                      (1 << CXY_SHIFT)
#define LXY_MASK                      (LXY_MULT - 1)
#define CXY_MASK                      (CXY_MULT - 1)

#define GRID_INTERP_PREC              (1)

#define BICUBIC_INTERP_PREC           (GRID_INTERP_PREC + 2 + 3)

#define BICUBIC_COEF_PREC             (GRID_INTERP_PREC + MAX_XY_DELTA)
#define BICUBIC_COEF_MULT             (1 << BICUBIC_COEF_PREC)

#define BICUBIC_DATA_PREC             (GRID_INTERP_PREC + 5)
#define BICUBIC_DATA_MULT             (1 << BICUBIC_DATA_PREC)
#define BICUBIC_DATA_MASK             (BICUBIC_DATA_MULT - 1)

#define PIXEL_INTERP_PREC_SHIFT       (10)
#define PIXEL_INTERP_PREC_MULT        (1 << PIXEL_INTERP_PREC_SHIFT)

#define SCALE_PREC                    (20)
#define CAMERA_K_PREC                 (28)

#define WARP_CLIP2(v, l, u)           (((l) == (u)) ? (l) : ((v) < (l)) ? (l) : ((v) >= (u)) ? ((u)-1) : (v))
#define WARP_CLIP1(v, m)              WARP_CLIP2(v, 0, m)
#define WARP_CLIP                     WARP_CLIP1

enum isp_dewarp_mode{
	ISP_DEWARPING_WARP_RECTIFY,
	ISP_DEWARPING_WARP_UNDISTORT,
	ISP_DEWARPING_WARP_PROJECTIVE,
	ISP_DEWARPING_WARP_OFF,
	ISP_DEWARPING_WARP_MAX,
};

enum isp_dewarp_cfg_cmd{
	ISP_DEWARPING_CALIB_COEFF,
	ISP_DEWARPING_MODE,
	ISP_DEWARPING_MAX,
};

struct isp_dewarp_input_info
{
	/*sensor fullsize width*/
	uint32_t fullsize_width;
	/*sensor fullsize height*/
	uint32_t fullsize_height;
	uint32_t input_width;
	uint32_t input_height;
	uint32_t crop_start_x;
	uint32_t crop_start_y;
	uint32_t crop_width;
	uint32_t crop_height;
	 /*binning mode 0: fullsize 1:binning*/
	uint32_t binning_mode;
};

struct isp_dewarp_calib_size
{
	uint64_t calib_width;
	uint64_t calib_height;
	uint64_t crop_start_x;
	uint64_t crop_start_y;
	uint64_t crop_width;
	uint64_t crop_height;
	uint64_t fov_scale;
};

/* for WARP_UNDISTORT */
struct isp_dewarp_calib_info
{
	struct isp_dewarp_calib_size calib_size;
	enum isp_dewarp_mode mode;
	/* fx, cx, 0, 0, fy, cy, 0, 0, 1 */
	int64_t camera_k[3][3];
	/* k1, k2, p1, p2, k3, k4, k5, k6, s1, s2, s3, s4, tauX, tauY */
	int64_t dist_coefs[14];
};

struct isp_dewarp_ops {
	int (*cfg_param)(void *dewarp_handle, enum isp_dewarp_cfg_cmd cmd, void *param);
	int (*pipe_proc)(void *dewarp_handle, void *param);
};

enum isp_dewarp_cache_format {
	ISP_DEWARP_YUV420_2FRAME = 0,
	ISP_DEWARP_YVU420_2FRAME,
	ISP_DEWARP_FORMAT_MAX
};

struct isp_dewarp_cache_info {
	uint32_t ctx_id;
	enum isp_fetch_path_select fetch_path_sel;
	uint32_t dewarp_cache_bypass;
	uint32_t dewarp_cache_endian;
	uint32_t dewarp_cache_prefetch_len;
	uint32_t dewarp_cache_mipi;
	enum isp_dewarp_cache_format yuv_format;
	struct img_addr addr;
	uint32_t frame_pitch;
};

struct isp_dewarp_in {
	uint32_t in_w;
	uint32_t in_h;
	uint32_t in_pitch[3];
	struct img_addr addr;
	struct img_trim in_trim;
	uint32_t yuv_format;
};

struct isp_dewarping_blk_info{
	uint32_t cxt_id;
	uint32_t dst_width;
	uint32_t dst_height;
	uint32_t grid_size;
	uint32_t grid_num_x;
	uint32_t grid_num_y;
	uint32_t pos_x;
	uint32_t pos_y;
	uint32_t src_width;
	uint32_t src_height;
	uint32_t start_mb_x;
	uint32_t mb_x_num;
	uint32_t mb_y_num;
	uint32_t init_start_row;
	uint32_t init_start_col;
	uint32_t crop_start_x;
	uint32_t crop_start_y;
	uint32_t crop_width;
	uint32_t crop_height;
	uint32_t grid_data_size;
	uint32_t dewarping_lbuf_ctrl_nfull_size;
	uint32_t chk_clr_mode;
	uint32_t chk_wrk_mode;
	/* default 1 : dewarping_mode:0:rectify 1:undistort 2:projective */
	enum isp_dewarp_mode dewarp_mode;
	/* default 0 :grid table mode:0:matrix_file 1:grid_xy_file */
	uint32_t grid_table_mode;
	uint32_t grid_x_ch0_buf[DEWARPING_GRID_DATA_SIZE];
	uint32_t grid_y_ch0_buf[DEWARPING_GRID_DATA_SIZE];
	/* PXL_COEF_CH0[LXY_MULT][3] */
	uint32_t pixel_interp_coef[LXY_MULT * 3];
	/* CORD_COEF_CH0[MAX_GRID_SIZE][3] */
	uint32_t bicubic_coef_i[MAX_GRID_SIZE * 3];
};

struct dewarping_slice_out {
	uint8_t dst_mblk_size ;
	uint32_t crop_info_w;
	uint32_t crop_info_h;
	uint32_t crop_info_x;
	uint32_t crop_info_y;
	uint16_t mb_row_start;
	uint16_t mb_col_start;
	uint16_t width;
	uint16_t height;
	uint16_t init_start_col;
	uint16_t init_start_row;
};

struct isp_dewarping_slice {
	struct slice_pos_info slice_pos;
	struct slice_dewarping_info dewarp_slice;
};

struct isp_dewarp_ctx_desc {
	uint32_t idx;
	uint32_t in_fmt;
	uint32_t hw_ctx_id;
	uint32_t slice_num;
	uint32_t cur_slice_id;
	enum isp_work_mode wmode;
	enum isp_dewarp_mode mode;
	struct img_addr fetch_addr;
	struct img_size src_size;
	struct img_size dst_size;
	struct camera_frame *buf_info;
	void *fmcu_handle;
	struct isp_dewarp_cache_info  dewarp_cache_info;
	struct isp_dewarping_blk_info dewarp_blk_info;
	struct isp_dewarp_calib_info dewarping_calib_info;
	struct isp_dewarping_slice slice_info[SLICE_NUM_MAX];
	struct isp_dewarp_ops ops;
	struct cam_hw_info *hw;
};

struct isp_dewarp_otp_info {
	/* header */
	uint32_t header[4];
	/* camera[3][3] */
	uint32_t cx;
	uint32_t cy;
	uint32_t fx;
	uint32_t fy;
	/* k1, k2, p1, p2, k3*/
	uint32_t dist_org_coef[5];
	uint32_t fov_scale;
	uint32_t calib_width;
	uint32_t calib_height;
	uint32_t crop_start_x;
	uint32_t crop_start_y;
	uint32_t crop_w;
	uint32_t crop_h;
	/*k4, k5, k6, s1, s2, s3, s4, taux, tauy*/
	uint32_t dist_new_coef[9];
};

void *isp_dewarping_ctx_get(uint32_t idx, void *hw);
void isp_dewarping_ctx_put(void *dewarp_handle);

#ifdef __cplusplus
}
#endif

#endif/* _ISP_DEWARP_H_ */

