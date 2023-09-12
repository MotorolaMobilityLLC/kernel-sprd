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

#ifndef _ALG_NR3_CALC_H
#define _ALG_NR3_CALC_H

#include "cam_types.h"

/*
 * Before N6pro all use ALG_NR3_MV_VER_0
 * N6pro use ALG_NR3_MV_VER_1.
*/
enum alg_nr3_mv_version {
	ALG_NR3_MV_VER_0,
	ALG_NR3_MV_VER_1,
	ALG_NR3_MV_VER_MAX,
};

/*
 * INPUT:
 *   mv_x: input mv_x (3DNR GME output)
 *   mv_y: input mv_y (3DNR GME output)
 *   mode_projection: mode projection in 3DNR GME
 *:   1 - interlaced mode, 0 - step mode
 *   sub_me_bypass:
 *   1 - sub pixel motion estimation bypass
 *   0 - do sub pixel motion estimation
 *   input_width:  input image width for binning and bayer scaler
 *   output_width: output image width for binning and bayer scaler
 *   input_height:  input image width for binning and bayer scaler
 *   output_height: output image width for binning and bayer scaler
 *
 * OUTPUT:
 *   out_mv_x
 *   out_mv_y
 */
struct alg_nr3_mv_cfg {
	/*    input param    */
	enum alg_nr3_mv_version mv_version;
	uint32_t mode_projection;
	uint32_t sub_me_bypass;
	int mv_x;
	int mv_y;
	int iw;
	int ow;
	int ih;
	int oh;

	/*    output param    */
	int o_mv_x;
	int o_mv_y;
};

struct ImageRegion_Info {
	uint32_t region_start_col;
	uint32_t region_start_row;
	uint32_t region_end_col;
	uint32_t region_end_row;
	uint32_t region_width;
	uint32_t region_height;
	int mv_x;
	int mv_y;
	uint32_t yuv_mode;
	//RefImage position in full size image
	int Y_start_x;
	int Y_end_x;
	int Y_start_y;
	int Y_end_y;
	int UV_start_x;
	int UV_end_x;
	int UV_start_y;
	int UV_end_y;
	uint32_t skip_flag;
};


void nr3_mv_convert_ver(struct alg_nr3_mv_cfg *param_ptr);
int nr3d_fetch_ref_image_position(struct ImageRegion_Info *image_region_info,
	uint32_t frame_width, uint32_t frame_height);

#endif
