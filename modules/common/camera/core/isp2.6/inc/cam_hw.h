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

#ifndef _CAM_HW_H_
#define _CAM_HW_H_

#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include "cam_types.h"
#include "isp_hw.h"
#include "alg_nr3_calc.h"

extern struct cam_hw_info sharkl3_hw_info;
extern struct cam_hw_info sharkl5_hw_info;
extern struct cam_hw_info sharkl5pro_hw_info;
extern struct cam_hw_info qogirl6_hw_info;
extern struct cam_hw_info qogirn6pro_hw_info;
extern struct cam_hw_info qogirn6l_hw_info;

typedef int (*hw_ioctl_fun)(void *handle, void *arg);
typedef int (*isp_k_blk_func)(void *handle);


/* The project id must keep same with the DT cfg
 * new added project should always added in the end
 */
enum cam_prj_id {
	SHARKL3,
	SHARKL5,
	ROC1,
	SHARKL5pro,
	QOGIRN6pro,
	QOGIRL6,
	QOGIRN6L,
	PROJECT_MAX
};

enum isp_default_type {
	ISP_HW_PARA,
	ISP_CFG_PARA,
	ISP_MAX_PARA
};

enum cam_block_type {
	DCAM_BLOCK_TYPE,
	ISP_BLOCK_TYPE,
	MAX_BLOCK_TYPE
};

enum isp_k_blk_idx {
	ISP_K_BLK_LTM,
	ISP_K_BLK_PYR_REC_BYPASS,
	ISP_K_BLK_PYR_REC_SHARE,
	ISP_K_BLK_PYR_REC_FRAME,
	ISP_K_BLK_PYR_REC_SLICE,
	ISP_K_BLK_PYR_REC_SLICE_COMMON,
	ISP_K_BLK_DEWARP_CACHE_CFG,
	ISP_K_BLK_DEWARP_CFG,
	ISP_K_BLK_DEWARP_SLICE,
	ISP_K_BLK_PYR_DEC_IRQ_FUNC,
	ISP_K_BLK_PYR_DEC_CFG,
	ISP_K_BLK_NLM_UPDATE,
	ISP_K_BLK_IMBLANCE_UPDATE,
	ISP_K_BLK_YNR_UPDATE,
	ISP_K_BLK_CNR_UPDATE,
	ISP_K_BLK_POST_CNR_UPDATE,
	ISP_K_BLK_EDGE_UPDATE,
	ISP_K_BLK_DCT_UPDATE,
	ISP_K_BLK_MAX
};

enum isp_k_gtm_blk_idx {
	ISP_K_GTM_LTM_EB,
	ISP_K_GTM_LTM_DIS,
	ISP_K_GTM_STATUS_GET,
	ISP_K_GTM_BLOCK_SET,
	ISP_K_GTM_MAPPING_GET,
	ISP_K_GTM_MAPPING_SET,
	ISP_K_GTM_SLICE_SET,
	ISP_K_GTM_BYPASS_SET,
	ISP_K_GTM_MAX
};

enum dcam_full_src_sel_type {
	ORI_RAW_SRC_SEL,
	PROCESS_RAW_SRC_SEL,
	LSC_RAW_SRC_SEL,
	BPC_RAW_SRC_SEL,
	NLM_RAW_SRC_SEL,
	MAX_RAW_SRC_SEL
};

/* 0 for normal dec, 1 for receive 4 yuv sensor data */
enum dcam_dec_path_sel {
	DACM_DEC_PATH_DEC,
	DCAM_DEC_PATH_YUV_DECI,
	DCAM_DEC_PATH_MAX,
};

enum cam_reg_trace_type {
	NORMAL_REG_TRACE,
	ABNORMAL_REG_TRACE,
	MAX_REG_TRACE
};

enum cam_bypass_type {
	DCAM_BYPASS_TYPE,
	ISP_BYPASS_TYPE,
	MAX_BYPASS_TYPE
};

enum dcam_fbc_mode_type {
	DCAM_FBC_DISABLE = 0,
	DCAM_FBC_FULL_10_BIT = 0x1,
	DCAM_FBC_BIN_10_BIT = 0x3,
	DCAM_FBC_FULL_14_BIT = 0x5,
	DCAM_FBC_BIN_14_BIT = 0x7,
	DCAM_FBC_RAW_10_BIT,
	DCAM_FBC_RAW_14_BIT,
};

enum isp_yuv_block_ctrl_type {
	ISP_YUV_BLOCK_CFG,
	ISP_YUV_BLOCK_DISABLE,
	ISP_YUV_BLOCK_ENABLE,
	ISP_YUV_BLOCK_MAX
};

enum isp_sub_path_id {
	ISP_SPATH_CP = 0,
	ISP_SPATH_VID,
	ISP_SPATH_FD,
	ISP_SPATH_NUM,
};

enum isp_afbc_path {
	AFBC_PATH_PRE = 0,
	AFBC_PATH_VID,
	AFBC_PATH_NUM,
};

enum dcam_fbc_path {
	DCAM_FBC_PATH_BIN,
	DCAM_FBC_PATH_FULL,
	DCAM_FBC_PATH_RAW,
	DCAM_FBC_PATH_NUM,
};

/* only temp for N6pro, after isp reset eco, need delete*/
enum isp_reset_flag {
	ISP_RESET_AFTER_POWER_ON,
	ISP_RESET_BEFORE_POWER_OFF,
	ISP_RESET_FLAG_MAX,
};

enum dcam_hw_cfg_cmd {
	DCAM_HW_CFG_ENABLE_CLK,
	DCAM_HW_CFG_DISABLE_CLK,
	DCAM_HW_CFG_INIT_AXI,
	DCAM_HW_CFG_SET_QOS,
	DCAM_HW_CFG_RESET,
	DCAM_HW_CFG_START,
	DCAM_HW_CFG_STOP,
	DCAM_HW_CFG_STOP_CAP_EB,
	DCAM_HW_CFG_FETCH_START,
	DCAM_HW_CFG_AUTO_COPY,
	DCAM_HW_CFG_FORCE_COPY,
	DCAM_HW_CFG_PATH_START,
	DCAM_HW_CFG_PATH_STOP,
	DCAM_HW_CFG_PATH_RESTART,
	DCAM_HW_CFG_PATH_CTRL,
	DCAM_HW_CFG_PATH_SRC_SEL,
	DCAM_HW_CFG_PATH_SIZE_UPDATE,
	DCAM_HW_CFG_CALC_RDS_PHASE_INFO,
	DCAM_HW_CFG_MIPI_CAP_SET,
	DCAM_HW_CFG_FETCH_SET,
	DCAM_HW_CFG_FETCH_BLOCK_SET,
	DCAM_HW_CFG_EBD_SET,
	DCAM_HW_CFG_BINNING_4IN1_SET,
	DCAM_HW_CFG_SRAM_CTRL_SET,
	DCAM_HW_CFG_LBUF_SHARE_SET,
	DCAM_HW_CFG_LBUF_SHARE_GET,
	DCAM_HW_CFG_SLICE_FETCH_SET,
	DCAM_HW_CFG_FBC_CTRL,
	DCAM_HW_CFG_FBC_ADDR_SET,
	DCAM_HW_CFG_BLOCK_FUNC_GET,
	DCAM_HW_CFG_BLOCKS_SETALL,
	DCAM_HW_CFG_BLOCKS_SETSTATIS,
	DCAM_HW_CFG_MIPICAP,
	DCAM_HW_CFG_START_FETCH,
	DCAM_HW_CFG_BIN_MIPI,
	DCAM_HW_CFG_BIN_PATH,
	DCAM_HW_DISCONECT_CSI,
	DCAM_HW_CONECT_CSI,
	DCAM_HW_FORCE_EN_CSI,
	DCAM_HW_FETCH_STATUS_GET,
	DCAM_HW_CFG_HIST_ROI_UPDATE,
	DCAM_HW_CFG_STORE_ADDR,
	DCAM_HW_CFG_FMCU_CMD,
	DCAM_HW_CFG_FMCU_START,
	DCAM_HW_FMCU_EBABLE,
	DCAM_HW_CFG_SLW_FMCU_CMDS,
	DCAM_HW_CFG_GTM_STATUS_GET,
	DCAM_HW_CFG_GTM_LTM_EB,
	DCAM_HW_CFG_GTM_LTM_DIS,
	DCAM_HW_CFG_GTM_UPDATE,
	DCAM_HW_CFG_DEC_ONLINE,
	DCAM_HW_BYPASS_DEC,
	DCAM_HW_CFG_DEC_STORE_ADDR,
	DCAM_HW_CFG_DEC_SIZE_UPDATE,
	DCAM_HW_CFG_GTM_HIST_GET,
	DCAM_HW_CFG_ALL_RESET,
	DCAM_HW_CFG_IRQ_DISABLE,
	DCAM_HW_CFG_MODULE_BYPASS,
	DCAM_HW_CFG_MAX
};

enum isp_hw_cfg_cmd {
	ISP_HW_CFG_ENABLE_CLK,
	ISP_HW_CFG_DISABLE_CLK,
	ISP_HW_CFG_RESET,
	ISP_HW_CFG_ENABLE_IRQ,
	ISP_HW_CFG_DISABLE_IRQ,
	ISP_HW_CFG_CLEAR_IRQ,
	ISP_HW_CFG_FETCH_SET,
	ISP_HW_CFG_FETCH_FBD_SET,
	ISP_HW_CFG_DEFAULT_PARA_SET,
	ISP_HW_CFG_BLOCK_FUNC_GET,
	ISP_HW_CFG_PARAM_BLOCK_FUNC_GET,
	ISP_HW_CFG_K_BLK_FUNC_GET,
	ISP_HW_CFG_CFG_MAP_INFO_GET,
	ISP_HW_CFG_FMCU_VALID_GET,
	ISP_HW_CFG_BYPASS_DATA_GET,
	ISP_HW_CFG_BYPASS_COUNT_GET,
	ISP_HW_CFG_REG_TRACE,
	ISP_HW_CFG_ISP_CFG_SUBBLOCK,
	ISP_HW_CFG_SET_PATH_STORE,
	ISP_HW_CFG_SET_PATH_SCALER,
	ISP_HW_CFG_SET_PATH_THUMBSCALER,
	ISP_HW_CFG_SLICE_SCALER,
	ISP_HW_CFG_SLICE_STORE,
	ISP_HW_CFG_AFBC_PATH_SET,
	ISP_HW_CFG_FBD_SLICE_SET,
	ISP_HW_CFG_FBD_ADDR_SET,
	ISP_HW_CFG_AFBC_FMCU_ADDR_SET,
	ISP_HW_CFG_AFBC_PATH_SLICE_SET,
	ISP_HW_CFG_LTM_SLICE_SET,
	ISP_HW_CFG_NR3_FBC_SLICE_SET,
	ISP_HW_CFG_NR3_FBD_SLICE_SET,
	ISP_HW_CFG_SLW_FMCU_CMDS,
	ISP_HW_CFG_FMCU_CFG,
	ISP_HW_CFG_SLICE_FETCH,
	ISP_HW_CFG_SLICE_NR_INFO,
	ISP_HW_CFG_SLICE_FMCU_CMD,
	ISP_HW_CFG_SLICE_FMCU_PYR_REC_CMD,
	ISP_HW_CFG_SLICE_FMCU_DEWARP_CMD,
	ISP_HW_CFG_SLICE_NOFILTER,
	ISP_HW_CFG_SLICE_3DNR_CROP,
	ISP_HW_CFG_SLICE_3DNR_STORE,
	ISP_HW_CFG_SLICE_3DNR_MEMCTRL,
	ISP_HW_CFG_SLICE_SPATH_STORE,
	ISP_HW_CFG_SLICE_SPATH_SCALER,
	ISP_HW_CFG_SLICE_SPATH_THUMBSCALER,
	ISP_HW_CFG_SET_SLICE_FETCH,
	ISP_HW_CFG_SET_SLICE_NR_INFO,
	ISP_HW_CFG_LTM_PARAM,
	ISP_HW_CFG_3DNR_PARAM,
	ISP_HW_CFG_GET_NLM_YNR,
	ISP_HW_CFG_STOP,
	ISP_HW_CFG_STORE_FRAME_ADDR,
	ISP_HW_CFG_FETCH_FRAME_ADDR,
	ISP_HW_CFG_MAP_INIT,
	ISP_HW_CFG_CMD_READY,
	ISP_HW_CFG_START_ISP,
	ISP_HW_CFG_UPDATE_HIST_ROI,
	ISP_HW_CFG_FETCH_START,
	ISP_HW_CFG_DEWARP_FETCH_START,
	ISP_HW_CFG_FMCU_CMD,
	ISP_HW_CFG_FMCU_START,
	ISP_HW_CFG_YUV_BLOCK_CTRL_TYPE,
	ISP_HW_CFG_SUBBLOCK_RECFG,
	ISP_HW_CFG_FMCU_CMD_ALIGN,
	ISP_HW_CFG_ALLDONE_CTRL,
	ISP_HW_CFG_GTM_FUNC_GET,
	ISP_HW_CFG_MAX
};

enum cam_hw_cfg_cmd {
	CAM_HW_GET_ALL_RST,
	CAM_HW_GET_AXI_BASE,
	CAM_HW_GET_DCAM_DTS_CLK,
	CAM_HW_GET_ISP_DTS_CLK,
	CAM_HW_GET_MAX
};

enum dcam_path_ctrl {
	HW_DCAM_PATH_PAUSE = 0,
	HW_DCAM_PATH_RESUME,
	HW_DCAM_PATH_MAX
};

enum isp_store_format {
	ISP_STORE_UYVY = 0x00,
	ISP_STORE_YUV422_2FRAME,
	ISP_STORE_YVU422_2FRAME,
	ISP_STORE_YUV422_3FRAME,
	ISP_STORE_YUV420_2FRAME,
	ISP_STORE_YVU420_2FRAME,
	ISP_STORE_YUV420_3FRAME,
	ISP_STORE_FULL_RGB,
	ISP_STORE_YUV420_2FRAME_10,
	ISP_STORE_YVU420_2FRAME_10,
	ISP_STORE_YUV420_2FRAME_MIPI,
	ISP_STORE_YVU420_2FRAME_MIPI,
	ISP_STORE_FORMAT_MAX
};

enum isp_fetch_format {
	ISP_FETCH_YUV422_3FRAME = 0,
	ISP_FETCH_YUYV,
	ISP_FETCH_UYVY,
	ISP_FETCH_YVYU,
	ISP_FETCH_VYUY,
	ISP_FETCH_YUV422_2FRAME,
	ISP_FETCH_YVU422_2FRAME,
	ISP_FETCH_RAW10,
	ISP_FETCH_CSI2_RAW10,
	ISP_FETCH_FULL_RGB10,
	ISP_FETCH_YUV420_2FRAME,
	ISP_FETCH_YVU420_2FRAME,
	ISP_FETCH_YUV420_2FRAME_10,
	ISP_FETCH_YVU420_2FRAME_10,
	ISP_FETCH_YUV420_2FRAME_MIPI,
	ISP_FETCH_YVU420_2FRAME_MIPI,
	ISP_FETCH_FORMAT_MAX
};

/*
 * Enumerating output paths in dcam_if device.
 *
 * @DCAM_PATH_FULL: with biggest line buffer, full path is often used as capture
 *                  path. Crop function available on this path.
 * @DCAM_PATH_BIN:  bin path is used as preview path. Crop and scale function
 *                  available on this path.
 * @DCAM_PATH_PDAF: this path is used to receive PDAF data
 * @DCAM_PATH_VCH2: can receive data according to data_type or
 *                  virtual_channel_id in a MIPI packet
 * @DCAM_PATH_VCH3: receive all data left
 * @DCAM_PATH_AEM:  output exposure by blocks
 * @DCAM_PATH_AFM:  output focus related data
 * @DCAM_PATH_AFL:  output anti-flicker data, including global data and region
 *                  data
 * @DCAM_PATH_HIST: output bayer histogram data in RGB channel
 * @DCAM_PATH_FRGB_HIST: n6pro only
 * @DCAM_PATH_3DNR: output noise reduction data
 * @DCAM_PATH_BPC:  output bad pixel data
 */
enum dcam_path_id {
	DCAM_PATH_FULL = 0,
	DCAM_PATH_BIN,
	DCAM_PATH_RAW,
	DCAM_PATH_PDAF,
	DCAM_PATH_VCH2,
	DCAM_PATH_VCH3,
	DCAM_PATH_AEM,
	DCAM_PATH_AFM,
	DCAM_PATH_AFL,
	DCAM_PATH_HIST,
	DCAM_PATH_FRGB_HIST,
	DCAM_PATH_3DNR,
	DCAM_PATH_BPC,
	DCAM_PATH_LSCM,
	DCAM_PATH_GTM_HIST,
	DCAM_PATH_MAX,
};

enum isp_fetch_path_select {
	ISP_FETCH_PATH_NORMAL = 0,
	ISP_FETCH_PATH_FBD,
	ISP_FETCH_PATH_DEWARP,
	ISP_FETCH_PATH_MAX
};

struct isp_fbd_raw_info {
	uint32_t ctx_id;
	/* ISP_FBD_RAW_SEL */
	uint32_t pixel_start_in_hor:6;
	uint32_t pixel_start_in_ver:2;
	uint32_t chk_sum_auto_clr:1;
	uint32_t fetch_fbd_bypass:1;
	uint32_t fetch_fbd_4bit_bypass:1;
	/* ISP_FBD_RAW_SLICE_SIZE */
	uint32_t height;
	uint32_t width;
	/* ISP_FBD_RAW_PARAM0 */
	uint32_t tiles_num_in_ver:11;
	uint32_t tiles_num_in_hor:6;
	/* ISP_FBD_RAW_PARAM1 */
	uint32_t time_out_th:8;
	uint32_t tiles_start_odd:1;
	uint32_t tiles_num_pitch:8;
	/* ISP_FBD_RAW_PARAM2 */
	uint32_t header_addr_init;
	/* ISP_FBD_RAW_PARAM3 */
	uint32_t tile_addr_init_x256;
	/* ISP_FBD_RAW_PARAM4 */
	uint32_t fbd_cr_ch0123_val0;
	/* ISP_FBD_RAW_PARAM5 */
	uint32_t fbd_cr_ch0123_val1;
	/* ISP_FBD_RAW_PARAM6 */
	uint32_t fbd_cr_uv_val1:8;
	uint32_t fbd_cr_y_val1:8;
	uint32_t fbd_cr_uv_val0:8;
	uint32_t fbd_cr_y_val0:8;
	/* ISP_FBD_RAW_LOW_PARAM0 */
	uint32_t low_bit_addr_init;
	/* ISP_FBD_RAW_LOW_PARAM1 */
	uint32_t low_bit_pitch:16;
	/* ISP_FBD_RAW_HBLANK */
	uint32_t hblank_en:1;
	uint32_t hblank_num:16;
	/* ISP_FBD_RAW_LOW_4BIT_PARAM0 */
	uint32_t low_4bit_addr_init;
	/* ISP_FBD_RAW_LOW_4BIT_PARAM1 */
	uint32_t low_4bit_pitch:16;
	/*
	 * For ISP trim feature. In capture channel, DCAM FULL crop is not used
	 * in zoom. ISP fetch trim is used instead.
	 * @size is normally same as @width and @height above.
	 */
	struct img_size size;
	struct img_trim trim;
	struct compressed_addr hw_addr;
	uint32_t header_addr_offset;
	uint32_t tile_addr_offset_x256;
	uint32_t low_bit_addr_offset;
	uint32_t low_4bit_addr_offset;
};

struct isp_fbd_yuv_info {
	uint32_t ctx_id;
	uint32_t data_bits;
	uint32_t hblank_en;
	uint32_t afbc_mode;
	uint32_t buffer_size;
	uint32_t hblank_num;
	uint32_t start_isp_afbd;
	uint32_t tile_num_pitch;
	uint32_t fetch_fbd_bypass;
	uint32_t chk_sum_auto_clr;
	uint32_t slice_start_pxl_xpt;
	uint32_t slice_start_pxl_ypt;
	uint32_t dout_req_signal_type;
	uint32_t slice_start_header_addr;
	uint32_t frame_header_base_addr;
	struct img_trim trim;
	struct img_size slice_size;
	struct compressed_addr hw_addr;
};

struct isp_hw_fetch_info {
	uint32_t ctx_id;
	uint32_t dispatch_color;
	enum isp_fetch_path_select fetch_path_sel;
	uint32_t pack_bits;
	uint32_t is_pack;
	uint32_t data_bits;
	uint32_t bayer_pattern;
	enum sprd_cam_sec_mode sec_mode;
	enum isp_fetch_format fetch_fmt;
	enum isp_fetch_format fetch_pyr_fmt;
	struct img_size src;
	struct img_trim in_trim;
	struct img_addr addr;
	struct img_addr trim_off;
	struct img_addr addr_hw;
	struct img_pitch pitch;
	uint32_t mipi_byte_rel_pos;
	uint32_t mipi_word_num;
	uint32_t mipi_byte_rel_pos_uv;
	uint32_t mipi_word_num_uv;
};

struct store_border {
	uint32_t up_border;
	uint32_t down_border;
	uint32_t left_border;
	uint32_t right_border;
};

struct isp_afbc_store_info {
	uint32_t bypass;
	uint32_t endian;
	uint32_t mirror_en;
	uint32_t color_format;
	uint32_t tile_number_pitch;
	uint32_t yaddr;
	uint32_t yheader;
	uint32_t header_offset;
	struct img_size size;
	struct store_border border;
};

struct isp_hw_afbc_path {
	uint32_t ctx_id;
	enum isp_sub_path_id spath_id;
	struct isp_afbc_store_info afbc_store;
};

struct isp_store_info {
	uint32_t bypass;
	uint32_t endian;
	uint32_t speed_2x;
	uint32_t need_bwd;
	uint32_t mirror_en;
	uint32_t max_len_sel;
	uint32_t shadow_clr;
	uint32_t store_res;
	uint32_t rd_ctrl;
	uint32_t shadow_clr_sel;
	uint32_t total_size;
	enum isp_store_format color_fmt;
	struct img_size size;
	struct img_addr addr;
	struct img_addr slice_offset;
	struct img_pitch pitch;
};

struct isp_hw_path_store {
	uint32_t ctx_id;
	enum isp_sub_path_id spath_id;
	struct isp_store_info store;
};

struct scaler_phase_info {
	int32_t scaler_init_phase[2];
	/* [hor/ver][luma/chroma] */
	int16_t scaler_init_phase_int[2][2];
	/* [hor/ver][luma/chroma] */
	uint16_t scaler_init_phase_rmd[2][2];
};

struct yuv_scaler_info {
	uint32_t scaler_bypass;
	uint32_t scaler_path_stop;
	uint32_t odata_mode;
	uint32_t scaler_y_hor_tap;
	uint32_t scaler_uv_hor_tap;
	uint32_t scaler_y_ver_tap;
	uint32_t scaler_uv_ver_tap;
	uint32_t scaler_ip_int;
	uint32_t scaler_ip_rmd;
	uint32_t scaler_cip_int;
	uint32_t scaler_cip_rmd;
	uint32_t scaler_factor_in;
	uint32_t scaler_factor_out;
	uint32_t scaler_ver_ip_int;
	uint32_t scaler_ver_ip_rmd;
	uint32_t scaler_ver_cip_int;
	uint32_t scaler_ver_cip_rmd;
	uint32_t scaler_ver_factor_in;
	uint32_t scaler_ver_factor_out;
	uint32_t scaler_out_width;
	uint32_t scaler_out_height;
	uint32_t work_mode;
	uint32_t coeff_buf[ISP_SC_COEFF_BUF_SIZE];
	struct scaler_phase_info init_phase_info;
};

struct isp_regular_info {
	uint32_t regular_mode;
	uint32_t shrink_uv_dn_th;
	uint32_t shrink_uv_up_th;
	uint32_t shrink_y_dn_th;
	uint32_t shrink_y_up_th;
	uint32_t effect_v_th;
	uint32_t effect_u_th;
	uint32_t effect_y_th;
	uint32_t shrink_c_range;
	uint32_t shrink_c_offset;
	uint32_t shrink_y_range;
	uint32_t shrink_y_offset;
};

struct img_deci_info {
	uint32_t deci_y_eb;
	uint32_t deci_y;
	uint32_t deci_x_eb;
	uint32_t deci_x;
};

struct isp_hw_path_scaler {
	uint32_t ctx_id;
	uint32_t uv_sync_v;
	uint32_t frm_deci;
	uint32_t path_sel;
	enum isp_sub_path_id spath_id;
	struct img_deci_info deci;
	struct img_size src;
	struct img_trim in_trim;
	struct img_trim out_trim;
	struct img_size dst;
	struct yuv_scaler_info scaler;
	struct isp_regular_info regular_info;
};

struct isp_hw_hist_roi {
	uint32_t ctx_id;
	struct isp_coord *hist_roi;
};

struct isp_hw_cfg_map {
	atomic_t map_cnt;
	struct isp_dev_cfg_info *s_cfg_settings;
};

struct isp_hw_set_slice_nr_info {
	uint32_t start_row_mod4;
	struct slice_nlm_info *slice_nlm;
	struct slice_ynr_info *slice_ynr;
	struct isp_fmcu_ctx_desc *fmcu;
	struct slice_postcnr_info *slice_postcnr_info;
	struct slice_edge_info *slice_edge;
};

struct isp_hw_set_slice_fetch {
	uint32_t ctx_id;
	struct isp_fmcu_ctx_desc *fmcu;
	struct slice_fetch_info *fetch_info;
};

struct isp_hw_slice_3dnr_memctrl {
	struct isp_fmcu_ctx_desc *fmcu;
	struct slice_3dnr_memctrl_info *mem_ctrl;
};

struct isp_hw_slice_3dnr_store {
	struct isp_fmcu_ctx_desc *fmcu;
	struct slice_3dnr_store_info *store;
};

struct isp_hw_slice_3dnr_crop {
	struct isp_fmcu_ctx_desc *fmcu;
	struct slice_3dnr_crop_info *crop;
};

struct isp_hw_slice_nofilter {
	struct slice_noisefilter_info *noisefilter_info;
	struct isp_fmcu_ctx_desc *fmcu;
};

struct dcam_hw_fetch_block {
	uint32_t idx;
	uint32_t raw_fetch_count;
};

struct dcam_hw_cfg_mipicap {
	uint32_t idx;
	uint32_t reg_val;
};

struct cam_hw_gtm_update {
	uint32_t gtm_idx;
	uint32_t idx;
	spinlock_t glb_reg_lock;
	struct dcam_dev_param *blk_dcam_pm;
	struct cam_hw_info *hw;
};

struct dcam_hw_gtm_hist {
	uint32_t idx;
	uint32_t hist_index;
	uint32_t value;
};

struct dcam_hw_slice_fetch {
	uint32_t idx;
	uint32_t slice_count;
	uint32_t slice_num;
	uint32_t dcam_slice_mode;
	uint32_t st_pack;
	uint32_t relative_offset;
	struct dcam_compress_info fbc_info;
	struct dcam_fetch_info *fetch;
	struct img_trim *cur_slice;
	struct img_trim slice_trim;
};

struct dcam_hw_sram_ctrl {
	uint32_t sram_ctrl_en;
	uint32_t idx;
};

struct dcam_hw_binning_4in1 {
	uint32_t idx;
	uint32_t binning_4in1_en;
};

struct dcam_hw_path_size {
	uint32_t idx;
	uint32_t auto_cpy_id;
	uint32_t size_x;
	uint32_t size_y;
	uint32_t path_id;
	uint32_t src_sel;
	uint32_t bin_ratio;
	uint32_t scaler_sel;
	uint32_t rds_coeff_size;
	uint32_t rds_init_phase_int1;
	uint32_t rds_init_phase_int0;
	uint32_t rds_init_phase_rdm1;
	uint32_t rds_init_phase_rdm0;
	uint32_t out_pitch;
	void *rds_coeff_buf;
	struct img_size in_size;
	struct img_trim in_trim;
	struct img_size out_size;
	struct dcam_compress_info compress_info;
	struct yuv_scaler_info *scaler_info;
};

struct dcam_hw_path_src_sel {
	uint32_t src_sel;
	uint32_t pack_bits;
	uint32_t idx;
};

struct dcam_hw_auto_copy {
	uint32_t id;
	uint32_t idx;
	spinlock_t glb_reg_lock;
};

struct dcam_hw_path_stop {
	uint32_t idx;
	uint32_t path_id;
	enum raw_alg_types raw_alg_type;
};

struct dcam_hw_path_restart {
	uint32_t idx;
	uint32_t path_id;
	enum raw_alg_types raw_alg_type;
};

struct dcam_fetch_info {
	uint32_t pack_bits;
	uint32_t fmt;
	uint32_t endian;
	uint32_t pattern;
	struct img_size size;
	struct img_trim trim;
	struct img_addr addr;
};

struct dcam_mipi_info {
	uint32_t sensor_if;
	uint32_t format;
	uint32_t mode;
	uint32_t data_bits;
	uint32_t pattern;
	uint32_t href;
	uint32_t frm_deci;
	uint32_t frm_skip;
	uint32_t x_factor;
	uint32_t y_factor;
	uint32_t is_4in1;
	uint32_t dcam_slice_mode;
	uint32_t is_cphy;
	struct img_trim cap_size;
};

struct dcam_hw_mipi_cap {
	uint32_t idx;
	struct dcam_mipi_info cap_info;
	uint32_t slowmotion_count;
};

struct dcam_hw_path_start {
	uint32_t path_id;
	uint32_t idx;
	uint32_t slowmotion_count;
	uint32_t pdaf_path_eb;
	/*out para*/
	uint32_t pack_bits;
	uint32_t out_fmt;
	uint32_t data_bits;
	uint32_t is_pack;
	struct img_endian endian;
	/*in para*/
	uint32_t src_sel;
	uint32_t bayer_pattern;
	uint32_t pdaf_type;
	struct img_trim in_trim;
	struct dcam_mipi_info cap_info;
};

struct dcam_hw_fetch_set {
	uint32_t idx;
	struct dcam_fetch_info *fetch_info;
};

struct dcam_hw_force_copy {
	uint32_t id;
	uint32_t idx;
	spinlock_t glb_reg_lock;
};

struct dcam_hw_start {
	uint32_t idx;
	uint32_t format;
	uint32_t raw_callback;
	void *dcam_sw_context;
};

struct reg_add_val_tag {
	unsigned int addr;
	unsigned int valid;
	unsigned int dvalue;
	unsigned int rw;
	unsigned int wc;
};

struct coeff_arg {
	uint32_t *h_coeff;
	uint32_t *h_chroma_coeff;
	uint32_t *v_coeff;
	uint32_t *v_chroma_coeff;
	uint32_t h_coeff_addr;
	uint32_t h_chroma_coeff_addr;
	uint32_t v_coeff_addr;
	uint32_t v_chroma_coeff_addr;
};

struct dcam_hw_path_ctrl {
	uint32_t path_id;
	uint32_t idx;
	enum dcam_path_ctrl type;
};

struct dcam_hw_ebd_set {
	struct sprd_ebd_control *p;
	uint32_t idx;
};

struct isp_hw_default_param {
	uint32_t type;
	uint32_t index;
};

struct isp_hw_block_func {
	uint32_t index;
	struct isp_cfg_entry *isp_entry;
};

struct isp_cfg_block_param {
	uint32_t index;
	struct isp_cfg_pre_param *isp_param;
};

struct isp_hw_k_blk_func {
	enum isp_k_blk_idx index;
	isp_k_blk_func k_blk_func;
};

struct isp_hw_gtm_func {
	enum isp_k_gtm_blk_idx index;
	isp_k_blk_func k_blk_func;
};

struct dcam_hw_block_func_get {
	struct dcam_cfg_entry *dcam_entry;
	uint32_t index;
};

struct isp_hw_thumbscaler_info {
	uint32_t idx;
	uint32_t scaler_bypass;
	uint32_t odata_mode;
	uint32_t frame_deci;
	uint32_t thumbscl_cal_version;

	struct img_deci_info y_deci;
	struct img_deci_info uv_deci;

	struct img_size y_factor_in;
	struct img_size y_factor_out;
	struct img_size uv_factor_in;
	struct img_size uv_factor_out;

	struct img_size src0;
	struct img_trim y_trim;
	struct img_size y_src_after_deci;
	struct img_size y_dst_after_scaler;
	struct img_size y_init_phase;

	struct img_trim uv_trim;
	struct img_size uv_src_after_deci;
	struct img_size uv_dst_after_scaler;
	struct img_size uv_init_phase;
};

struct cam_hw_bypass_data {
	struct bypass_tag *tag;
	enum cam_bypass_type type;
	uint32_t i;
};

struct cam_hw_reg_trace {
	int type;
	uint32_t idx;
};

struct dcam_store {
	uint32_t color_fmt;
	unsigned long  reg_addr;
	uint32_t is_compressed;
	struct img_addr store_addr;
};

struct dcam_dec_store {
	uint32_t pitch;
	struct img_addr addr;
	struct img_size size;
	struct img_border border;
};

struct dcam_hw_slw_fmcu_cmds {
	uint32_t ctx_id;
	uint32_t is_first_cycle;
	struct dcam_fmcu_ctx_desc *fmcu_handle;
	struct dcam_path_desc *dcam_path;
	struct dcam_store store_info[DCAM_PATH_MAX];
	uint32_t slw_id;
	uint32_t slw_cnt;
};

struct dcam_hw_fmcu_cfg {
	uint32_t ctx_id;
	struct dcam_fmcu_ctx_desc *fmcu;
};

struct isp_hw_slice_store {
	uint32_t path_en;
	uint32_t ctx_id;
	enum isp_sub_path_id spath_id;
	struct slice_store_info *slc_store;
};

struct isp_hw_slice_scaler {
	uint32_t path_en;
	uint32_t ctx_id;
	enum isp_sub_path_id spath_id;
	struct slice_scaler_info *slc_scaler;
};

struct isp_hw_nr3_fbd_slice {
	struct isp_fmcu_ctx_desc *fmcu_handle;
	struct slice_3dnr_fbd_fetch_info *fbd_fetch;
	struct slice_3dnr_memctrl_info *mem_ctrl;
};

struct isp_hw_nr3_fbc_slice {
	struct isp_fmcu_ctx_desc *fmcu_handle;
	struct slice_3dnr_fbc_store_info *fbc_store;
};

struct isp_hw_ltm_slice {
	struct isp_fmcu_ctx_desc *fmcu_handle;
	struct slice_ltm_map_info *map;
	uint32_t ltm_id;
};

struct isp_hw_gtm_slice{
	uint32_t idx;
	struct isp_fmcu_ctx_desc *fmcu_handle;
	struct slice_gtm_info *slice_param;
};

struct isp_hw_afbc_path_slice {
	struct isp_fmcu_ctx_desc *fmcu_handle;
	struct slice_afbc_store_info *slc_afbc_store;
	uint32_t path_en;
	uint32_t ctx_idx;
	uint32_t spath_id;
};

struct isp_hw_slw_fmcu_cmds {
	uint32_t ctx_id;
	uint32_t is_compressed;
	struct img_addr fetchaddr;
	struct isp_fmcu_ctx_desc *fmcu_handle;
	struct isp_afbc_store_info afbc_store[AFBC_PATH_NUM];
	struct isp_store_info store[ISP_SPATH_NUM];
	struct isp_path_desc *isp_path;
};

struct isp_hw_fmcu_cfg {
	uint32_t ctx_id;
	struct isp_fmcu_ctx_desc *fmcu;
};

struct isp_hw_slice_fetch {
	uint32_t ctx_id;
	struct slice_fetch_info *fetch_info;
};

struct isp_hw_slice_nr_info {
	uint32_t ctx_id;
	struct isp_slice_desc *cur_slc;
};

struct isp_hw_slices_fmcu_cmds {
	int hw_ctx_id;
	uint32_t wmode;
	struct isp_fmcu_ctx_desc *fmcu;
};

struct isp_hw_afbc_fmcu_addr {
	struct isp_fmcu_ctx_desc *fmcu;
	uint32_t yaddr;
	uint32_t yheader;
	int index;
};

struct isp_hw_afbc_addr {
	uint32_t idx;
	uint32_t spath_id;
	unsigned long *yuv_addr;
};

struct isp_hw_fbd_slice {
	struct isp_fmcu_ctx_desc *fmcu_handle;
	struct slice_fbd_raw_info *info;
	struct slice_fbd_yuv_info *yuv_info;
};

struct cam_hw_gtm_ltm_dis {
	uint32_t dcam_idx;
	uint32_t isp_idx;
};

struct cam_hw_gtm_ltm_eb {
	uint32_t dcam_idx;
	uint32_t isp_idx;
	struct dcam_dev_param *dcam_param;
};

struct dcam_hw_fbc_addr {
	uint32_t idx;
	unsigned long addr;
	uint32_t path_id;
	uint32_t data_bits;
	struct compressed_addr *fbc_addr;
};

struct dcam_hw_fbc_ctrl {
	uint32_t idx;
	uint32_t path_id;
	uint32_t fmt;
	uint32_t data_bits;
	int fbc_mode;
};

struct cam_hw_lbuf_share {
	enum dcam_id idx;
	uint32_t width;
	uint32_t offline_flag;
	uint32_t pdaf_share_flag;
};

struct dcam_hw_calc_rds_phase {
	struct dcam_rds_slice_ctrl *gphase;
	uint16_t slice_id;
	uint16_t slice_end0;
	uint16_t slice_end1;
};

struct isp_hw_slice_spath {
	uint32_t path_en;
	uint32_t ctx_idx;
	enum isp_sub_path_id spath_id;
	struct slice_store_info *slc_store;
	struct slice_scaler_info *slc_scaler;
	struct isp_fmcu_ctx_desc *fmcu;
};

struct isp_hw_slice_spath_thumbscaler {
	uint32_t path_en;
	uint32_t ctx_idx;
	struct isp_fmcu_ctx_desc *fmcu;
	struct slice_thumbscaler_info *slc_scaler;
};

struct isp_hw_ltm_3dnr_param {
	uint32_t idx;
	uint32_t val;
};

struct isp_hw_nlm_ynr {
	uint32_t val;
	uint32_t ctx_id;
	struct slice_cfg_input *slc_cfg_in;
};

struct dcam_hw_start_fetch {
	uint32_t start_x;
	uint32_t size_x;
	uint32_t fetch_pitch;
	uint32_t idx;
};

struct dcam_hw_cfg_bin_path {
	uint32_t idx;
	uint32_t start_x;
	uint32_t fetch_pitch;
};

struct dcam_hw_cfg_store_addr {
	uint32_t idx;
	uint32_t path_id;
	uint32_t frame_addr[3];
	uint32_t out_fmt;
	uint32_t in_fmt;
	uint32_t out_pitch;
	uint32_t reg_addr;
	struct dcam_dev_param *blk_param;
	struct img_size out_size;
};

struct dcam_hw_dec_store_cfg {
	uint32_t idx;
	uint32_t cur_layer;
	uint32_t layer_num;
	uint32_t bypass;
	uint32_t endian;
	uint32_t mono_en;
	uint32_t color_format;
	uint32_t burst_len;
	uint32_t mirror_en;
	uint32_t flip_en;
	uint32_t data_10b;
	uint32_t speed2x;
	uint32_t shadow_clr_sel;
	uint32_t pitch[3];
	uint32_t addr[3];
	uint32_t width;
	uint32_t height;
	uint32_t border_up;
	uint32_t border_down;
	uint32_t border_left;
	uint32_t border_right;
	uint32_t rd_ctrl;
	uint32_t store_res;
	uint32_t shadow_clr;
	uint32_t last_frm_en;
	uint32_t align_w;
	uint32_t align_h;
	struct img_size size_t[5];
	struct img_pitch pitch_t[5];
};

struct dcam_hw_dec_online_cfg {
	uint32_t idx;
	uint32_t layer_num;
	enum dcam_dec_path_sel path_sel;
	uint32_t chksum_clr_mode;
	uint32_t chksum_work_mode;
	uint32_t hor_padding_en;
	uint32_t hor_padding_num;
	uint32_t ver_padding_en;
	uint32_t ver_padding_num;
	uint32_t flush_hblank_num;
	uint32_t flush_line_num;
	uint32_t flust_width;
};

struct dcam_hw_fmcu_cmd {
	unsigned long base;
	unsigned long hw_addr;
	int cmd_num;
	uint32_t idx;
};

struct dcam_hw_fmcu_start {
	unsigned long base;
	unsigned long hw_addr;
	int cmd_num;
	uint32_t idx;
};

struct dcam_hw_fmcu_sel {
	uint32_t fmcu_id;
	uint32_t hw_idx;
};

struct dcam_fmcu_enable {
	uint32_t idx;
	uint32_t enable;
};

struct isp_hw_fmcu_cmd {
	unsigned long base;
	unsigned long hw_addr;
	int cmd_num;
};

struct isp_hw_fmcu_start {
	uint32_t fid;
	unsigned long hw_addr;
	int cmd_num;
};

struct isp_hw_fmcu_sel {
	uint32_t fmcu_id;
	uint32_t hw_idx;
};

struct isp_hw_yuv_block_ctrl {
	uint32_t idx;
	uint32_t type;
	struct isp_k_block *blk_param;
};

struct camhw_cal_fbc_size_para {
	uint32_t fmt;
	uint32_t data_bits;
	uint32_t width;
	uint32_t height;
	uint32_t bypass_4bit;
	struct dcam_compress_info fbc_info;
};

struct camhw_cal_fbc_addr_para {
	uint32_t fmt;
	uint32_t width;
	uint32_t height;
	uint32_t bypass_4bit;
	unsigned long in;
	struct compressed_addr *out;
	struct dcam_compress_info fbc_info;
};

enum isp_alldone_ctrl_int {
	ISP_ALLDONE_WAIT_DISPATCH,
	ISP_ALLDONE_WAIT_LTMHIST,
	ISP_ALLDONE_WAIT_REC,
	ISP_ALLDONE_WAIT_YUV_DONE,
	ISP_ALLDONE_WAIT_STORE,
};

struct isp_hw_alldone_ctrl {
	uint32_t hw_ctx_id;
	bool wait;
	enum isp_alldone_ctrl_int int_bit;
};

struct hw_io_ctrl_fun {
	uint32_t cmd;
	hw_ioctl_fun hw_ctrl;
};

struct glb_syscon {
	uint32_t rst;
	uint32_t rst_mask;
	uint32_t rst_ahb_mask;
	uint32_t rst_vau_mask;
	uint32_t all_rst;
	uint32_t all_rst_mask;
	uint32_t axi_rst_mask;
	uint32_t sys_soft_rst;
	uint32_t sys_h2p_db_soft_rst;
};

struct cam_hw_ip_info {
	uint32_t idx;
	uint32_t irq_no;
	uint32_t dec_irq_no;
	uint32_t max_height;
	uint32_t max_width;
	unsigned long phy_base;
	unsigned long reg_base;
	struct glb_syscon syscon;

	/* For dcam support info */
	uint32_t aux_dcam_path;
	uint32_t slm_path;
	uint32_t lbuf_share_support;
	uint32_t offline_slice_support;
	uint32_t superzoom_support;
	uint32_t dcam_full_fbc_mode;
	uint32_t dcam_bin_fbc_mode;
	uint32_t dcam_raw_fbc_mode;
	uint32_t dcam_offline_fbc_mode;
	unsigned long *store_addr_tab;
	uint32_t *path_ctrl_id_tab;
	uint32_t afl_gbuf_size;
	unsigned long pdaf_type3_reg_addr;
	uint32_t rds_en;
	uint32_t dcam_raw_path_id;
	uint32_t pyramid_support;
	uint32_t fmcu_support;
	uint32_t sensor_raw_fmt;
	uint32_t save_band_for_bigsize;
	uint32_t raw_fmt_support[DCAM_RAW_MAX];
	uint32_t dcam_zoom_mode;
	uint32_t dcam_output_support[DCAM_STORE_BIT_MAX];
	uint32_t recovery_support;

	/* For isp support info */
	uint32_t slm_cfg_support;
	uint32_t scaler_coeff_ex;
	uint32_t scaler_bypass_ctrl;
	uint32_t *ctx_fmcu_support;
	uint32_t rgb_ltm_support;
	uint32_t yuv_ltm_support;
	uint32_t pyr_rec_support;
	uint32_t pyr_dec_support;
	uint32_t fbd_raw_support;
	uint32_t fbd_yuv_support;
	uint32_t rgb_gtm_support;
	uint32_t dewarp_support;
	uint32_t frbg_hist_support;
	uint32_t nr3_mv_alg_version;
	uint32_t dyn_overlap_version;
	uint32_t fetch_raw_support;
	uint32_t nr3_compress_support;
	uint32_t capture_thumb_support;
	uint32_t thumb_scaler_cal_version;
};

struct cam_hw_soc_info {
	struct platform_device *pdev;
	uint32_t count;
	unsigned long axi_reg_base;
	unsigned long fmcu_reg_base;

	struct regmap *cam_ahb_gpr;
	struct regmap *aon_apb_gpr;
	struct regmap *cam_switch_gpr;
	rwlock_t cam_ahb_lock;
	struct clk *core_eb;
	struct clk *axi_eb;
	struct clk *mtx_en;
	struct clk *blk_cfg_en;
	struct clk *tck_en;
	struct clk *clk;
	struct clk *clk_parent;
	struct clk *clk_default;
	struct clk *bpc_clk;
	struct clk *bpc_clk_parent;
	struct clk *bpc_clk_default;
	struct clk *axi_clk;
	struct clk *axi_clk_parent;
	struct clk *axi_clk_default;
	struct clk *mtx_clk;
	struct clk *mtx_clk_parent;
	struct clk *mtx_clk_default;
	struct clk *blk_cfg_clk;
	struct clk *blk_cfg_clk_parent;
	struct clk *blk_cfg_clk_default;

	uint32_t arqos_high;
	uint32_t arqos_low;
	uint32_t awqos_high;
	uint32_t awqos_low;
};

struct isp_hw_cfg_info {
	uint32_t hw_ctx_id;
	uint32_t fmcu_enable;
	uint32_t hw_addr;
};

struct cam_hw_info {
	enum cam_prj_id prj_id;
	uint32_t csi_connect_type;
	spinlock_t isp_cfg_lock;
	struct platform_device *pdev;
	struct cam_hw_soc_info *soc_dcam;
	struct cam_hw_soc_info *soc_dcam_lite;
	struct cam_hw_soc_info *soc_isp;
	struct cam_hw_ip_info *ip_dcam[DCAM_ID_MAX];
	struct cam_hw_ip_info *ip_isp;
	int (*dcam_ioctl)(void *,enum dcam_hw_cfg_cmd, void *);
	int (*isp_ioctl)(void *,enum isp_hw_cfg_cmd, void *);
	int (*cam_ioctl)(void *,enum cam_hw_cfg_cmd, void *);
};

#endif
