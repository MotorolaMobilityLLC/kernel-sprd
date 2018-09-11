/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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
#ifndef _SPRD_IMG_H_
#define _SPRD_IMG_H_

#include <linux/ioctl.h>

/* SPRD_IMG_IO_CHECK_FMT fourcc Four-character-code(FOURCC) */
#define img_fourcc(a, b, c, d)\
	((uint32_t)(a) | ((uint32_t)(b) << 8) |\
	 ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

/* RGB formats */
/* 16  RGB-5-6-5    */
#define IMG_PIX_FMT_RGB565  img_fourcc('R', 'G', 'B', 'P')
/* 16  RGB-5-6-5 BE  */
#define IMG_PIX_FMT_RGB565X img_fourcc('R', 'G', 'B', 'R')

/* Grey formats */
#define IMG_PIX_FMT_GREY    img_fourcc('G', 'R', 'E', 'Y') /*  8  Greyscale   */
#define IMG_PIX_FMT_PDA3    img_fourcc('P', 'D', 'A', '3') /*  8  Greyscale   */

/* Luminance+Chrominance formats */
#define IMG_PIX_FMT_YVU420  img_fourcc('Y', 'V', '1', '2') /* 12  YVU 4:2:0   */
#define IMG_PIX_FMT_YUYV    img_fourcc('Y', 'U', 'Y', 'V') /* 16  YUV 4:2:2   */
#define IMG_PIX_FMT_YVYU    img_fourcc('Y', 'V', 'Y', 'U') /* 16 YVU 4:2:2 */
#define IMG_PIX_FMT_UYVY    img_fourcc('U', 'Y', 'V', 'Y') /* 16  YUV 4:2:2   */
#define IMG_PIX_FMT_VYUY    img_fourcc('V', 'Y', 'U', 'Y') /* 16  YUV 4:2:2   */
/* 16  YVU422 planar */
#define IMG_PIX_FMT_YUV422P img_fourcc('4', '2', '2', 'P')
#define IMG_PIX_FMT_YUV420  img_fourcc('Y', 'U', '1', '2') /* 12  YUV 4:2:0   */

/* two planes -- one Y, one Cr + Cb interleaved  */
/* 12  Y/CbCr 4:2:0  */
#define IMG_PIX_FMT_NV12    img_fourcc('N', 'V', '1', '2')
/* 12  Y/CrCb 4:2:0  */
#define IMG_PIX_FMT_NV21    img_fourcc('N', 'V', '2', '1')

/* compressed formats */
#define IMG_PIX_FMT_JPEG     img_fourcc('J', 'P', 'E', 'G') /* JFIF JPEG     */

#define SPRD_IMG_PATH_MAX    6

#define SPRD_FLASH_MAX_CELL  40

#define IMG_PATH_BUFFER_COUNT 24


enum {
	IMG_TX_DONE       = 0x00,
	IMG_NO_MEM        = 0x01,
	IMG_TX_ERR        = 0x02,
	IMG_CSI2_ERR      = 0x03,
	IMG_SYS_BUSY      = 0x04,
	IMG_CANCELED_BUF  = 0x05,
	IMG_TIMEOUT       = 0x10,
	IMG_TX_STOP       = 0xFF
};

enum {
	IMG_ENDIAN_BIG = 0,
	IMG_ENDIAN_LITTLE,
	IMG_ENDIAN_HALFBIG,
	IMG_ENDIAN_HALFLITTLE,
	IMG_ENDIAN_MAX
};

enum {
	CAMERA_IRQ_IMG = 1,
	CAMERA_IRQ_STATIS,
	CAMERA_IRQ_DONE,
	CAMERA_IRQ_3DNR_DONE,
	CAMERA_IRQ_POST_YNR_DONE,
	CAMERA_IRQ_4IN1_DONE,
	CAMERA_IRQ_PATH_SOF,
	CAMERA_IRQ_TX_RESERVED,
	CAMERA_IRQ_MAX
};

/*enum for  SPRD_IMG_IO_SET_SENSOR_IF/struct sprd_img_sensor_if*/
enum if_status {
	IF_OPEN = 0,
	IF_CLOSE
};

enum dcam_cap_if_mode {
	DCAM_CAP_IF_CCIR = 0,
	DCAM_CAP_IF_CSI2,
	DCAM_CAP_IF_MODE_MAX
};

enum dcam_cap_sensor_mode {
	DCAM_CAP_MODE_YUV = 0,
	DCAM_CAP_MODE_JPEG = 2,
	DCAM_CAP_MODE_RAWRGB = 3,
	DCAM_CAP_MODE_MAX
};

enum dcam_cap_pattern {
	DCAM_YUYV  = 0,
	DCAM_YVYU,
	DCAM_UYVY,
	DCAM_VYUY,
	DCAM_PATTERN_MAX
};

enum dcam_cap_data_bits {
	DCAM_CAP_12_BITS = 12,
	DCAM_CAP_10_BITS = 10,
	DCAM_CAP_8_BITS = 8,
	DCAM_CAP_4_BITS = 4,
	DCAM_CAP_2_BITS = 2,
	DCAM_CAP_1_BITS = 1,
	DCAM_CAP_BITS_MAX = 0xFF
};

/*enum for  SPRD_IMG_IO_SET_MODE*/
enum dcam_capture_mode {
	DCAM_CAPTURE_MODE_SINGLE = 0,
	DCAM_CAPTURE_MODE_MULTIPLE,
	DCAM_CAPTURE_MODE_MAX
};

/*enum for  SPRD_IMG_IO_SET_SHRINK*/
enum dcam_regular_mode {
	DCAM_REGULAR_BYPASS = 0,
	DCAM_REGULAR_SHRINK = 1,
	DCAM_REGULAR_CUT = 2,
	DCAM_REGULAR_EFFECT = 3,
	DCAM_REGULAR_MAX,
};


/*dcam path output logical format*/
enum dcam_fmt {
	DCAM_YUV422 = 0,
	DCAM_YUV420,
	DCAM_YVU420,
	DCAM_YUV420_3FRAME = 0x03,
	DCAM_YUV400,
	DCAM_RGB565,
	DCAM_RGB888,
	DCAM_JPEG = 0x10,
	DCAM_RAWRGB,
	DCAM_FTM_MAX
};


enum {
	SPRD_IMG_GET_SCALE_CAP = 0,
	SPRD_IMG_GET_FRM_BUFFER,
	SPRD_IMG_STOP_DCAM,
	SPRD_IMG_FREE_FRAME,
	SPRD_IMG_GET_PATH_CAP
};

enum sprd_flash_type {
	FLASH_TYPE_PREFLASH,
	FLASH_TYPE_MAIN,
	FLASH_TYPE_TORCH,
	FLASH_TYPE_MAX
};

enum sprd_flash_io_id {
	FLASH_IOID_GET_CHARGE,
	FLASH_IOID_GET_TIME,
	FLASH_IOID_GET_MAX_CAPACITY,
	FLASH_IOID_SET_CHARGE,
	FLASH_IOID_SET_TIME,
	FLASH_IOID_MAX
};

enum sprd_buf_flag {
	IMG_BUF_FLAG_INIT,
	IMG_BUF_FLAG_RUNNING,
	IMG_BUF_FLAG_MAX
};

enum sprd_flash_status {
	FLASH_CLOSE = 0x0,
	FLASH_OPEN = 0x1,
	FLASH_TORCH = 0x2,/*user only set flash to close/open/torch state */
	FLASH_AUTO = 0x3,
	FLASH_CLOSE_AFTER_OPEN = 0x10,/* following is set to sensor */
	FLASH_HIGH_LIGHT = 0x11,
	FLASH_OPEN_ON_RECORDING = 0x22,
	FLASH_CLOSE_AFTER_AUTOFOCUS = 0x30,
	FLASH_NEED_QUIT = 0x31,
	FLASH_AF_DONE = 0x40,
	FLASH_WAIT_TO_CLOSE = 0x41,
	FLASH_CURRENT_LEVEL_SET = 0x42,
	FLASH_STATUS_MAX
};

/*use to record scene work mode*/
enum dcam_scene_mode {
	DCAM_SCENE_MODE_PREVIEW = 0,
	DCAM_SCENE_MODE_CAPTURE,
	DCAM_SCENE_MODE_RECORDING,
	DCAM_SCENE_MODE_CAPTURE_CALLBACK,
	DCAM_SCENE_MODE_MAX
};

enum dcam_capture_status {
	DCAM_CAPTURE_STOP = 0,
	DCAM_CAPTURE_START,
	DCAM_CAPTURE_START_WITH_FLASH,
	DCAM_CAPTURE_START_HDR,
	DCAM_CAPTURE_START_3DNR,
	DCAM_CAPTURE_START_WITH_TIMESTAMP,
	DCAM_CAPTURE_START_4IN1_LOWLUX,
	DCAM_CAPTURE_NONE,
	DCAM_CAPTURE_MAX
};

enum {
	ISP_VIDEO_CLOSE = 0,
	ISP_NORMAL_VIDEO,
	ISP_SLW_VIDEO,
};

enum {
	SPRD_SBS_MODE_OFF = 0,
	SPRD_SBS_MODE_ON,
	SPRD_SBS_MODE_LEFT,
	SPRD_SBS_MODE_RIGHT,
	SPRD_SBS_MODE_MAX
};

/*use to record 3dnr support mode*/
enum sprd_support_3dnr_mode {
	SPRD_3DNR_SW,
	SPRD_3DNR_HW,
	SPRD_3DNR_MAX,
};

#define DCAM_RES_DCAM0_CAP			0x1
#define DCAM_RES_DCAM0_PATH			0x2
#define DCAM_RES_DCAM1_CAP			0x4
#define DCAM_RES_DCAM1_PATH			0x8
#define DCAM_RES_DCAM2_CAP			0x10
#define DCAM_RES_DCAM2_PATH			0x20

struct sprd_img_size {
	uint32_t w;
	uint32_t h;
};

struct sprd_img_rect {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
};

struct sprd_img_frm_addr {
	uint32_t y;
	uint32_t u;
	uint32_t v;
};

union dcam_regular_value {
	struct dcam_shrink_val {
	uint8_t               y_up_threshold;
	uint8_t               y_dn_threshold;
	uint8_t               uv_up_threshold;
	uint8_t               uv_dn_threshold;
	} shrink_val;
	struct dcam_effect_val {
		uint8_t               y_special_threshold;
		uint8_t               u_special_threshold;
		uint8_t               v_special_threshold;
		uint8_t               reserved;
	} effect_val;
};

struct dcam_regular_desc {
	enum dcam_regular_mode regular_mode;
	union dcam_regular_value regular_value;
};

struct dcam_jpegls_desc {
	uint32_t is_jpegls;
	uint32_t jpegls_thd[3];
};

struct sprd_pdaf_control {
	uint32_t mode;
	uint32_t phase_data_type;
	uint32_t image_vc;
	uint32_t image_dt;
	uint32_t isp_tool_mode;
};

struct sprd_ebd_control {
	uint32_t mode;
	uint32_t image_vc;
	uint32_t image_dt;
};

struct sprd_aem_offset {
	uint32_t x : 13;
	uint32_t y : 13;
};

struct sprd_aem_blk_size {
	uint32_t width : 8;
	uint32_t height : 8;
};

struct sprd_aem_avgshf {
	uint32_t aem_h_avgshf : 2;
	uint32_t aem_l_avgshf : 2;
	uint32_t aem_m_avgshf : 2;
};

struct sprd_threshold {
	uint32_t high : 10;
	uint32_t low : 10;
};

struct sprd_aem_info {
	uint32_t skip_num;
	uint32_t mode;
	struct sprd_aem_offset offset;
	struct sprd_aem_blk_size blk_size;
	struct sprd_aem_avgshf aem_avgshf;
	struct sprd_threshold red_thr;
	struct sprd_threshold blue_thr;
	struct sprd_threshold green_thr;
};

struct sprd_blc_info {
	uint32_t bypass;
	uint32_t mode;
	uint32_t r;
	uint32_t b;
	uint32_t gr;
	uint32_t gb;
};

#pragma pack(push, 4)
struct sprd_img_vcm_dac_info {
	uint32_t pulse_line;
	uint32_t dac;
	uint32_t pulse_sec;
	uint32_t pulse_usec;
	uint32_t vcm_mv_sec;
	uint32_t vcm_mv_usec;
	uint32_t frame_id;
};
#pragma pack(pop)

struct sprd_img_statis_info {
	uint32_t irq_type;
	uint32_t irq_property;
	uint32_t phy_addr;
	uint32_t vir_addr;
	uint32_t addr_offset;
	uint32_t kaddr[2];
	uint32_t buf_size;
	uint32_t mfd;
	uint32_t sec;
	uint32_t usec;
	uint32_t frame_id;
	struct sprd_img_vcm_dac_info dac_info;
	uint32_t zoom_ratio;
};

struct sprd_irq_info {
	uint32_t irq_type;
	uint32_t irq_property;
	uint32_t sec;
	uint32_t usec;
	uint32_t frame_id;
};

struct sprd_img_parm {
	uint32_t                  channel_id;
	uint32_t                  frame_base_id;
	uint32_t                  sn_fmt;
	uint32_t                  sensor_id;
	uint32_t                  pixel_fmt;
	uint32_t                  need_isp_tool;
	uint32_t                  rt_refocus;
	uint32_t                  deci;
	uint32_t                  scene_mode;
	uint32_t                  slowmotion;
	uint32_t                  skip_num;
	struct dcam_regular_desc regular_desc;
	struct dcam_jpegls_desc jpegls_desc;
	uint32_t                  index;
	uint32_t                  need_isp;
	uint32_t                  is_reserved_buf;
	uint32_t                  buf_flag;
	uint32_t		  irq_type;
	uint32_t		  phy_addr;
	uint32_t		  vir_addr;
	uint32_t		  buf_size;
	uint32_t		  buf_property;
	uint32_t                  is_statis_buf_reserved;
	struct sprd_pdaf_control  pdaf_ctrl;
	struct sprd_img_rect      crop_rect;
	struct sprd_img_size      dst_size;
	struct sprd_img_frm_addr  frame_addr;
	struct sprd_img_frm_addr  frame_addr_vir;
	struct sprd_img_frm_addr  frame_addr_array[IMG_PATH_BUFFER_COUNT];
	struct sprd_img_frm_addr  frame_addr_vir_array[IMG_PATH_BUFFER_COUNT];
	uint32_t                  fd_array[IMG_PATH_BUFFER_COUNT];
	uint32_t                  buffer_count;
	struct sprd_img_statis_info img_statis_info;
	uint32_t jpegls_length[3];
	struct sprd_ebd_control   ebd_ctrl;
	uint32_t                  is_high_fps;
	uint32_t                  high_fps_skip_num;
	uint32_t                  reserved[4];
};

#pragma pack(push, 4)
struct sprd_img_function_mode {
	uint32_t need_4in1;
	uint32_t need_3dnr;
	uint32_t dual_cam;
};
#pragma pack(pop)

struct sprd_img_ccir_if {
	uint32_t v_sync_pol;
	uint32_t h_sync_pol;
	uint32_t pclk_pol;
	uint32_t res1;
	uint32_t padding;
};

struct sprd_img_mipi_if {
	uint32_t use_href;
	uint32_t bits_per_pxl;
	uint32_t is_loose;
	uint32_t lane_num;
	uint32_t pclk;
};

struct sprd_img_sensor_if {
	uint32_t if_type;
	uint32_t img_fmt;
	uint32_t img_ptn;
	uint32_t frm_deci;
	uint32_t res[4];
	union {
		struct sprd_img_ccir_if ccir;
		struct sprd_img_mipi_if mipi;
	} if_spec;
};

#pragma pack(push, 4)
struct sprd_img_frm_info {
	uint32_t channel_id;
	uint32_t height;
	uint32_t length;
	uint32_t sec;
	uint32_t usec;
	uint32_t frm_base_id;
	uint32_t index;
	uint32_t real_index;
	uint32_t img_fmt;
	uint32_t yaddr;
	uint32_t uaddr;
	uint32_t vaddr;
	uint32_t yaddr_vir;
	uint32_t uaddr_vir;
	uint32_t vaddr_vir;
	uint32_t irq_type;
	uint32_t irq_property;
	uint32_t frame_id;
	uint32_t phy_addr;
	uint32_t vir_addr;
	uint32_t addr_offset;
	uint32_t kaddr[2];
	uint32_t buf_size;
	int64_t  monoboottime;
	uint32_t mfd;
	uint32_t zoom_ratio;
	struct sprd_img_vcm_dac_info dac_info;
	uint32_t reserved[4];
};
#pragma pack(pop)

struct sprd_img_path_info {
	uint32_t               line_buf;
	uint32_t               support_yuv;
	uint32_t               support_raw;
	uint32_t               support_jpeg;
	uint32_t               support_scaling;
	uint32_t               support_trim;
	uint32_t               is_scaleing_path;
};

struct sprd_img_path_capability {
	uint32_t count;
	uint32_t support_3dnr_mode;
	uint32_t support_4in1;
	struct sprd_img_path_info path_info[SPRD_IMG_PATH_MAX];
	uint32_t reserved;
};


struct sprd_img_write_op {
	uint32_t cmd;
	uint32_t channel_id;
	uint32_t index;
	uint32_t sensor_id;
};

struct sprd_img_read_op {
	uint32_t cmd;
	uint32_t evt;
	uint32_t sensor_id;
	union {
		struct sprd_img_frm_info frame;
		struct sprd_img_path_capability capability;
		uint32_t reserved[20];
	} parm;
};

struct sprd_img_get_fmt {
	uint32_t index;
	uint32_t fmt;
};

struct sprd_img_time {
	uint32_t sec;
	uint32_t usec;
};

struct sprd_img_endian {
	uint8_t y_endian;
	uint8_t uv_endian;
};

struct sprd_img_format {
	uint32_t channel_id;
	uint32_t width;
	uint32_t height;
	uint32_t fourcc;
	uint32_t need_isp;
	uint32_t need_binning;
	uint32_t bytesperline;
	uint32_t is_lightly;
	uint32_t flip_on;
	struct sprd_img_endian endian;
	uint32_t buffer_cfg_isp;
	uint32_t reserved[4];
};

struct sprd_flash_capacity {
	uint16_t max_charge;
	uint16_t max_time;
	uint16_t max_torch_current;
	uint16_t max_preflash_current;
	uint16_t max_highlight_current;
	float torch_step_current;
	float preflash_step_current;
	float highlight_step_current;
	uint16_t max_torch_current_total;
	uint16_t max_preflash_current_total;
	uint16_t max_highlight_current_total;
	uint16_t torch_steps;
	uint16_t preflash_steps;
	uint16_t highlight_steps;
};

struct sprd_img_set_flash {
	uint32_t led0_ctrl;
	uint32_t led1_ctrl;
	uint32_t led0_status;
	uint32_t led1_status;
	uint32_t flash_index;
};

struct sprd_img_sbs_info {
	uint32_t sbs_mode;
	uint32_t reserved[4];
};

struct sprd_flash_element {
	uint16_t index;
	uint16_t val;
};

struct sprd_flash_cell {
	uint8_t type;
	uint8_t count;
	uint8_t def_val;
	uint8_t led_idx;
	struct sprd_flash_element element[SPRD_FLASH_MAX_CELL];
};

struct sprd_flash_cfg_param {
	uint32_t io_id;
	uint8_t flash_idx;
	struct sprd_flash_cell real_cell;
};

struct sprd_img_iova {
	int fd;
	size_t size;
	void *sg_table;
};

struct sprd_img_res {
	uint32_t width;
	uint32_t height;
	uint32_t sensor_id;
	uint32_t flag;
	uint32_t reserved[4];
};

struct sprd_isp_capability {
	uint32_t isp_id;
	uint32_t index;
	void *property_param;
};

struct isp_k_time {
	uint32_t sec;
	uint32_t usec;
};

struct sprd_isp_irq {
	uint32_t irq_val0;
	uint32_t irq_val1;
	uint32_t irq_val2;
	uint32_t irq_val3;
	uint32_t reserved;
	int32_t ret_val;
	struct isp_k_time time;
};

struct sprd_isp_reg_param {
	unsigned long reg_param;
	uint32_t counts;
};

struct sprd_isp_io_param {
	uint32_t isp_id;
	uint32_t sub_block;
	uint32_t property;
	void *property_param;
};

struct sprd_isp_reg_bits {
	unsigned long reg_addr;
	unsigned long reg_value;
};

struct isp_3dnr_param {
	signed char mv_x;
	signed char mv_y;
	unsigned int fetch_cur_addr;
	unsigned int fetch_ref_addr;
	unsigned int store_ref_addr;
	signed int fetch_cur_addr_fd;
	signed int fetch_ref_addr_fd;
	signed int store_ref_addr_fd;
	unsigned int fetch_cur_endian;
	unsigned int fetch_ref_endian;
	unsigned int store_ref_endian;
	int image_width;
	int image_height;
	int blending_no;
};

struct sprd_isp_post_ynr_param {
	unsigned int ydenoise_lowlux_bypass;
	unsigned int ydenoise_flat[7];
	unsigned int ydenoise_lut_thresh[7];
	unsigned int ydenoise_subthresh[9];
	unsigned int ydenoise_addback[9];
	unsigned int ydenoise_sedgethresh;
	unsigned int ydenoise_txtthresh;
	unsigned int ydenoise_l1_txt_thresh1;
	unsigned int ydenoise_l1_txt_thresh0;
	unsigned int ydenoise_l0_lut_thresh1;
	unsigned int ydenoise_l0_lut_thresh0;
	unsigned int ydenoise_l1_eurodist[3];
	unsigned int ydenoise_l3_wfindex;
	unsigned int ydenoise_l2_wfindex;
	unsigned int ydenoise_l1_wfindex;
	unsigned int ydenoise_l2_eurodist[3];
	unsigned int ydenoise_l3_eurodist[3];
	unsigned int ydenoise_wv_nr_enable;
	unsigned int ydenoise_l1_blf_enable;
	unsigned int ydenoise_l2_blf_enable;
	unsigned int ydenoise_l3_blf_enable;
	unsigned int wltt[24];
	unsigned int freqratio[24];
	unsigned int dist_interval;
	unsigned int ydenoise_radius;
	unsigned int ydenoise_imgcenterx;
	unsigned int ydenoise_imgcentery;
	unsigned int ydenoise_sal_nr_str[8];
	unsigned int ydenoise_sal_offset[8];
};

struct sprd_isp_ynr_param {
	unsigned int src_img_w;
	unsigned int src_img_h;
	unsigned int dst_img_w;
	unsigned int dst_img_h;
	unsigned int src_buf_fd;
	unsigned int dst_buf_fd;
	struct sprd_isp_post_ynr_param ynr_param;
};

struct sprd_img_3dnr_param {
	unsigned int w;
	unsigned int h;
	unsigned int is_3dnr;
};

#pragma pack(push, 4)
struct sprd_img_capture_param {
	uint32_t type;
	uint32_t cnr_cnt;
	int64_t  timestamp;
};
#pragma pack(pop)

#pragma pack(push, 4)
struct sprd_img_vcm_param {
	uint8_t vcm_i2c_data[8];
	int32_t vcm_i2c_count;
	int32_t vcm_slave_addr;
	int32_t next_vcm_pos;
};
#pragma pack(pop)

#pragma pack(push, 4)
struct sprd_dcam_path_size {
	uint32_t dcam_in_w;
	uint32_t dcam_in_h;
	uint32_t pre_dst_w;
	uint32_t pre_dst_h;
	uint32_t vid_dst_w;
	uint32_t vid_dst_h; /* to kernel */
	uint32_t dcam_out_w;
	uint32_t dcam_out_h; /* report to hal */
	uint32_t reserved[2];
};
#pragma pack(pop)

#define SPRD_IMG_IO_MAGIC            'Z'
#define SPRD_IMG_IO_SET_MODE          _IOW(SPRD_IMG_IO_MAGIC, 0, uint32_t)
#define SPRD_IMG_IO_SET_CAP_SKIP_NUM  _IOW(SPRD_IMG_IO_MAGIC, 1, uint32_t)
#define SPRD_IMG_IO_SET_SENSOR_SIZE   _IOW(SPRD_IMG_IO_MAGIC, 2,\
					   struct sprd_img_size)
#define SPRD_IMG_IO_SET_SENSOR_TRIM   _IOW(SPRD_IMG_IO_MAGIC, 3,\
					   struct sprd_img_rect)
#define SPRD_IMG_IO_SET_FRM_ID_BASE   _IOW(SPRD_IMG_IO_MAGIC, 4,\
					   struct sprd_img_parm)
#define SPRD_IMG_IO_SET_CROP          _IOW(SPRD_IMG_IO_MAGIC, 5,\
					   struct sprd_img_parm)
#define SPRD_IMG_IO_SET_FLASH         _IOW(SPRD_IMG_IO_MAGIC, 6,\
					   struct sprd_img_set_flash)
#define SPRD_IMG_IO_SET_OUTPUT_SIZE   _IOW(SPRD_IMG_IO_MAGIC, 7,\
					   struct sprd_img_parm)
#define SPRD_IMG_IO_SET_ZOOM_MODE     _IOW(SPRD_IMG_IO_MAGIC, 8, uint32_t)
#define SPRD_IMG_IO_SET_SENSOR_IF     _IOW(SPRD_IMG_IO_MAGIC, 9,\
					   struct sprd_img_sensor_if)
#define SPRD_IMG_IO_SET_FRAME_ADDR    _IOW(SPRD_IMG_IO_MAGIC, 10,\
					   struct sprd_img_parm)
#define SPRD_IMG_IO_PATH_FRM_DECI     _IOW(SPRD_IMG_IO_MAGIC, 11,\
					   struct sprd_img_parm)
#define SPRD_IMG_IO_PATH_PAUSE        _IOW(SPRD_IMG_IO_MAGIC, 12,\
					   struct sprd_img_parm)
#define SPRD_IMG_IO_PATH_RESUME       _IOW(SPRD_IMG_IO_MAGIC, 13, uint32_t)
#define SPRD_IMG_IO_STREAM_ON         _IOW(SPRD_IMG_IO_MAGIC, 14, uint32_t)
#define SPRD_IMG_IO_STREAM_OFF        _IOW(SPRD_IMG_IO_MAGIC, 15, uint32_t)
#define SPRD_IMG_IO_GET_FMT           _IOR(SPRD_IMG_IO_MAGIC, 16,\
					   struct sprd_img_get_fmt)
#define SPRD_IMG_IO_GET_CH_ID         _IOR(SPRD_IMG_IO_MAGIC, 17, uint32_t)
#define SPRD_IMG_IO_GET_TIME          _IOR(SPRD_IMG_IO_MAGIC, 18,\
					   struct sprd_img_time)
#define SPRD_IMG_IO_CHECK_FMT         _IOWR(SPRD_IMG_IO_MAGIC, 19,\
					    struct sprd_img_format)
#define SPRD_IMG_IO_SET_SHRINK        _IOW(SPRD_IMG_IO_MAGIC, 20, uint32_t)
#define SPRD_IMG_IO_SET_FREQ_FLAG     _IOW(SPRD_IMG_IO_MAGIC, 21, uint32_t)
#define SPRD_IMG_IO_CFG_FLASH         _IOW(SPRD_IMG_IO_MAGIC, 22,\
					   struct sprd_flash_cfg_param)
#define SPRD_IMG_IO_PDAF_CONTROL      _IOW(SPRD_IMG_IO_MAGIC, 23,\
					   struct sprd_pdaf_control)
#define SPRD_IMG_IO_GET_IOMMU_STATUS    _IOR(SPRD_IMG_IO_MAGIC, 24, uint32_t)
#define SPRD_IMG_IO_DISABLE_MODE      _IOW(SPRD_IMG_IO_MAGIC, 25, uint32_t)
#define SPRD_IMG_IO_ENABLE_MODE       _IOW(SPRD_IMG_IO_MAGIC, 26, uint32_t)
#define SPRD_IMG_IO_START_CAPTURE      _IOW(SPRD_IMG_IO_MAGIC, 27,\
					    struct sprd_img_capture_param)
#define SPRD_IMG_IO_STOP_CAPTURE       _IOW(SPRD_IMG_IO_MAGIC, 28,\
						     uint32_t)
#define SPRD_IMG_IO_SET_PATH_SKIP_NUM  _IOW(SPRD_IMG_IO_MAGIC, 29,\
					   struct sprd_img_parm)
#define SPRD_IMG_IO_SBS_MODE           _IOW(SPRD_IMG_IO_MAGIC, 30,\
					    struct sprd_img_sbs_info)
#define SPRD_IMG_IO_DCAM_PATH_SIZE	_IOW(SPRD_IMG_IO_MAGIC, 31,\
						struct sprd_dcam_path_size)
#define SPRD_IMG_IO_SET_SENSOR_MAX_SIZE	_IOW(SPRD_IMG_IO_MAGIC, 32,\
							uint32_t)
#define SPRD_ISP_IO_CAPABILITY    _IOR(SPRD_IMG_IO_MAGIC, 33,\
	struct sprd_isp_capability)
#define SPRD_ISP_IO_IRQ           _IOR(SPRD_IMG_IO_MAGIC, 34,\
	struct sprd_isp_irq)
#define SPRD_ISP_IO_READ          _IOR(SPRD_IMG_IO_MAGIC, 35,\
	struct sprd_isp_reg_param)
#define SPRD_ISP_IO_WRITE         _IOW(SPRD_IMG_IO_MAGIC, 36,\
	struct sprd_isp_reg_param)
#define SPRD_ISP_IO_RST    _IOW(SPRD_IMG_IO_MAGIC, 37, uint32_t)
#define SPRD_ISP_IO_STOP          _IOW(SPRD_IMG_IO_MAGIC, 38, uint32_t)
#define SPRD_ISP_IO_INT           _IOW(SPRD_IMG_IO_MAGIC, 39, uint32_t)
#define SPRD_ISP_IO_SET_STATIS_BUF	_IOW(SPRD_IMG_IO_MAGIC, 40,\
					     struct isp_statis_buf_input)
#define SPRD_ISP_IO_CFG_PARAM     _IOWR(SPRD_IMG_IO_MAGIC, 41,\
	struct isp_io_param)
#define SPRD_ISP_REG_READ	_IOR(SPRD_IMG_IO_MAGIC, 42,\
	struct sprd_isp_reg_bits)
#define SPRD_ISP_IO_POST_3DNR       _IOW(SPRD_IMG_IO_MAGIC, 43,\
	struct isp_3dnr_param)
#define SPRD_STATIS_IO_CFG_PARAM       _IOW(SPRD_IMG_IO_MAGIC, 44,\
	struct isp_io_param)
#define SPRD_ISP_IO_RAW_CAP	_IOR(SPRD_IMG_IO_MAGIC, 45,\
	struct isp_raw_proc_info)
#define SPRD_IMG_IO_GET_DCAM_RES       _IOW(SPRD_IMG_IO_MAGIC, 46, uint32_t)
#define SPRD_IMG_IO_PUT_DCAM_RES       _IOW(SPRD_IMG_IO_MAGIC, 47, uint32_t)
#define SPRD_ISP_IO_SET_PULSE_LINE     _IOW(SPRD_IMG_IO_MAGIC, 48, uint32_t)
#define SPRD_ISP_IO_CFG_START    _IOW(SPRD_IMG_IO_MAGIC, 49, uint32_t)
#define SPRD_ISP_IO_POST_YNR       _IOW(SPRD_IMG_IO_MAGIC, 50,\
	struct sprd_isp_ynr_param)
#define SPRD_ISP_IO_SET_NEXT_VCM_POS   _IOW(SPRD_IMG_IO_MAGIC, 51,\
	struct sprd_img_vcm_param)
#define SPRD_ISP_IO_SET_VCM_LOG        _IOW(SPRD_IMG_IO_MAGIC, 52, uint32_t)

#define SPRD_IMG_IO_SET_3DNR		 _IOW(SPRD_IMG_IO_MAGIC, 53, \
	struct sprd_img_3dnr_param)
#define SPRD_IMG_IO_SET_FUNCTION_MODE       _IOW(SPRD_IMG_IO_MAGIC, 54, \
	struct sprd_img_function_mode)
#define SPRD_ISP_IO_MASK_3A           _IOW(SPRD_IMG_IO_MAGIC, 55, uint32_t)

#define SPRD_IMG_IO_GET_FLASH_INFO       _IOW(SPRD_IMG_IO_MAGIC, 56,\
					   struct sprd_img_set_flash)
#define SPRD_IMG_IO_MAP_IOVA       _IOW(SPRD_IMG_IO_MAGIC, 57,\
					   struct sprd_img_iova)
#define SPRD_IMG_IO_UNMAP_IOVA       _IOW(SPRD_IMG_IO_MAGIC, 58,\
					   struct sprd_img_iova)
#define SPRD_IMG_IO_GET_SG       _IOW(SPRD_IMG_IO_MAGIC, 59,\
					   struct sprd_img_iova)
#define SPRD_ISP_IO_UPDATE_PARAM_START    _IOW(SPRD_IMG_IO_MAGIC, 60, uint32_t)
#define SPRD_ISP_IO_UPDATE_PARAM_END    _IOW(SPRD_IMG_IO_MAGIC, 61, uint32_t)
#define SPRD_ISP_IO_REG_ISP_ISR _IOW(SPRD_IMG_IO_MAGIC, 62, uint32_t)
#define SPRD_IMG_IO_EBD_CONTROL      _IOW(SPRD_IMG_IO_MAGIC, 63,\
					   struct sprd_ebd_control)
#define SPRD_IMG_IO_SET_4IN1_ADDR    _IOW(SPRD_IMG_IO_MAGIC, 64,\
					   struct sprd_img_parm)
#define SPRD_IMG_IO_4IN1_POST_PROC         _IOW(SPRD_IMG_IO_MAGIC, 65,\
					   struct sprd_img_parm)

/*
* Dump dcam register.
* buf:      input dump buffer addr
* buf_len:  input dump buffer size(>=0x400), and buf_len=0x400 is ok
* return    real dump size
*/
int32_t sprd_dcam_registers_dump(void *buf, uint32_t buf_len);
#endif /*_SPRD_V4L2_H_*/
