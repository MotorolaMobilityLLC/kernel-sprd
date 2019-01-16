#ifndef _DCAM_PARAMS_H_
#define _DCAM_PARAMS_H_

/*
*image pattern
*PATTERN_RED        1
*PATTERN_GREEN      2
*PATTERN_BLUE       3
*PATTERN_BARS       4
*PATTERN_FADED_BARS 5

*image type
*TYPE_RGB 1
*TYPE_YUV 2

*bayer format
*BAYER_0 0
*BAYER_1 1
*BAYER_2 2
*BAYER_3 3
/////////////////////////////////////
// bayer mode     pixel arrangement
//     0               G R G R
//                     B G B G
//
//     1               R G R G
//                     G B G B
//
//     2               B G B G
//                     G R G R
//
//     3               G B G B
//                     R G R G
/////////////////////////////////////

*YUV format
*YUV_UYVY   1
*YUV_Y_UV   2
*YUV_Y_U_V  3
*/

#include "data_type.h"

#define YUV_PATH_NUM	3

#define CAPTURE_MODE	0
#define REVIEW_MODE	1

#define DECI_DIRECT	0
#define DECI_AVERAGE	1

#define YUV422		0
#define YUV420		1

#define PLANAR		0
#define SEMI_PLANAR	1

#define DISABLE		0
#define ENABLE		1

#define OUTPUT_YUV	1

struct CMDINFO {
	signed short y_hor_coef[8][8];
	signed short c_hor_coef[8][4];
	signed short y_ver_coef[9][16];
	signed short c_ver_coef[9][16];
};

struct Decimation_tag {
	unsigned char deci_x_en;
	unsigned char deci_y_en;
	unsigned char deci_x;
	unsigned char deci_y;
	unsigned char deci_cut_first_y;
	unsigned char deci_option;
	unsigned short reserved0;
};

struct Scaler_tag {
	unsigned char scaler_en;
	unsigned char scaler_tap;
	unsigned char scaler_y_ver_tap;
	unsigned char scaler_uv_ver_tap;
	unsigned char scaling2yuv420;
	unsigned char reserved0;

	unsigned short scaler_in_width;
	unsigned short scaler_in_height;
	unsigned short scaler_out_width;
	unsigned short scaler_out_height;
	short scaler_init_phase_int;
	unsigned short scaler_init_phase_rmd;
	short scaler_chroma_init_phase_int;
	unsigned short scaler_chroma_init_phase_rmd;
	unsigned short scaler_factor_in_hor;
	unsigned short scaler_factor_out_hor;
	unsigned short scaler_factor_in_ver;
	unsigned short scaler_factor_out_ver;
	unsigned short reserved1;
	int scaler_initial_phase;
	struct CMDINFO scaler_coef_info;
};

struct Trim_tag {
	unsigned short trim_start_x;
	unsigned short trim_start_y;
	unsigned short trim_size_x;
	unsigned short trim_size_y;
};

struct Crop_tag {
	unsigned char crop_en;
	unsigned char reserved0;
	unsigned short reserved1;
	unsigned short crop_start_x;
	unsigned short crop_start_y;
	unsigned short crop_end_x;
	unsigned short crop_end_y;
};

struct ROTATION_tag {
	unsigned char rot_en;
	unsigned char rot_dir;
	unsigned char uv_average_deci;
	unsigned char reserved0;
};

struct REGULAR_YUV_tag {
	unsigned char shrink_y_range;
	unsigned char shrink_uv_range;
	unsigned char shrink_y_offset;
	unsigned char shrink_uv_offset;

	unsigned char shrink_uv_dn_th;
	unsigned char shrink_uv_up_th;
	unsigned char shrink_y_dn_th;
	unsigned char shrink_y_up_th;
	unsigned char effect_v_th;
	unsigned char effect_u_th;
	unsigned char effect_y_th;
	unsigned char reserved0;
};

struct YUV_PATH_INFO_tag {
	unsigned char path_en;
	unsigned char outdata_mode;
	unsigned char outdata_format;
	unsigned char regular_mode;
	unsigned short src_size_x;
	unsigned short src_size_y;
	struct Trim_tag trim0_info;
	struct Decimation_tag deci_info;
	struct Scaler_tag scaler_info;
	struct Trim_tag trim1_info;
	struct ROTATION_tag rotation_info;
};

struct YUV_DUAL_INFO_tag {
	unsigned int			offset[2];
	unsigned short			stride[2];
};

struct DCAMINFO {
	unsigned short cowork_mode;
	unsigned short cowork_overlap;
	unsigned short dcam_in_width;
	unsigned short dcam_in_height;
	unsigned char dcam_mode;
	unsigned char reserved0;
	unsigned char dcam_path0_en;
	unsigned char dcam_path0_outdata_mode;

	struct Crop_tag cap_crop_info;
	struct Decimation_tag cap_deci_info;
	struct Trim_tag dcam_path0_trim_info;
	struct Decimation_tag dcam_path0_deci_info;
	struct Scaler_tag dcam_path0_scaler_info;
	struct ROTATION_tag dcam_path0_rotation_info;


	struct YUV_PATH_INFO_tag	dcam_yuv_path[YUV_PATH_NUM];
	struct YUV_DUAL_INFO_tag dcam_yuv_dual[YUV_PATH_NUM];
	struct YUV_DUAL_INFO_tag DualInfo[3];
	struct YUV_PATH_INFO_tag InputInfo[2][3];
	struct REGULAR_YUV_tag regular_yuv_info;

	unsigned char dcam_yuv2rgb_en;
	unsigned char dcam_cap_path2_yuv2rgb_en;
	unsigned char dcam_cap_path2_dithering_en;
	unsigned char dcam_dithering_en;
	unsigned char dcam_rgb2yuv_en;
	unsigned char dcam_rgb_format;
	unsigned char dcam_yuv_format;
	unsigned char dcam_review_deci_en;
	unsigned char dcam_review_deci_factor;

	unsigned char dcam_uv420_avg_en;
	unsigned char dcam_review_uv420_avg_en;
	unsigned char dcam_cap_path2_uv420_avg_en;
	unsigned char pattern_gen_sel;
	unsigned char pattern_model;
	unsigned char pattern_format;

	unsigned char bit_ext_mode;
	unsigned char mipi_raw_sel;
	unsigned char raw_bit_width;
	unsigned char mipi_data_format;
	unsigned char rawrgb_out_mode;
	unsigned char mipi_binning_en;

	unsigned char pattern_mode;
	unsigned char color_pattern_img_bayer_format;
	unsigned char color_pattern_img_yuv_format;

	unsigned char rotate_path_en;
	unsigned char rotate_path_uv_format;
	struct Trim_tag rotate_path_trim_info;
	struct ROTATION_tag rotate_path_rotation_info;
	unsigned short current_width;
	unsigned short current_height;
	unsigned short pattern_width;
	unsigned short pattern_height;
};

void InitDCAMParams(struct DCAMINFO *pDCAMInfo, const char *cfg_file);

#endif
