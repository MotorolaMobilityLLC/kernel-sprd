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

#ifndef _ISP_DRV_HEADER_
#define _ISP_DRV_HEADER_

#include <linux/bitops.h>
#include <linux/kfifo.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/types.h>

#include "sprd_img.h"
#include "sprd_isp_hw.h"
#include "isp_reg.h"
#include "dcam_drv.h"
#include "cam_iommu.h"
#include "cam_dbg.h"

#define ISP_PATH_FRAME_WIDTH_MAX                        4224
#define ISP_PATH_FRAME_HEIGHT_MAX                       3168
#define ISP_PATH1_LINE_BUF_LENGTH                       2592
#define ISP_PATH2_LINE_BUF_LENGTH                       2592
#define ISP_PATH3_LINE_BUF_LENGTH                       2592

#define ISP_FMCU_CMD_Q_SIZE                             0x4000
#define ISP_QUEUE_LENGTH                                16
#define ISP_SCL0_MAX_WIDTH                              2304
#define ISP_ONLINE_ZFACTOR_MAX                          2
#define ISP_BUF_QUEUE_LENGTH                            16
#define ISP_BING4AWB_NUM                                2
#define ISP_STATISTICS_BUF_MAX                          4
#define ISP_STATISTICS_QUEUE_LEN                        8
#define ISP_IMG_OUTPUT_PATH_MAX                         3
#define ISP_FRM_QUEUE_LENGTH                            7
#define ISP_PIXEL_ALIGN_WIDTH                           4
#define ISP_PIXEL_ALIGN_HEIGHT                          2
#define ISP_RAW_AFM_ITEM                                40
#define ISP_FRGB_GAMMA_BUF_SIZE                         (257 * 4 + 4)
#define ISP_NLM_BUF_SIZE                                (1024 * 4 + 4)
#define LENS_W_BUF_SIZE                                 (512)

#define ISP_LOWEST_ADDR                                 0x800
#define ISP_ADDR_INVALID(addr)     \
                ((unsigned long)(addr) < (unsigned long)ISP_LOWEST_ADDR)

#define ISP_PATH_TIMEOUT                        msecs_to_jiffies(500)

#define ISP_PATH_PRE_FRM_CNT_MAX                        32
#define ISP_PATH_CAP_FRM_CNT_MAX                        32
#define ISP_PATH_VID_FRM_CNT_MAX                        32
#define ISP_PATH_0_FRM_CNT_MAX                          48

#define ISP_SC_COEFF_BUF_COUNT                          20
#define ISP_SC_COEFF_BUF_SIZE                           (24 << 10)
#define ISP_SC_COEFF_COEF_SIZE                          (1 << 10)
#define ISP_SC_COEFF_TMP_SIZE                           (21 << 10)
#define ISP_PATH_SCL_COUNT                              5

#define ISP_SC_H_COEF_SIZE                              0xC0
#define ISP_SC_V_COEF_SIZE                              0x210
#define ISP_SC_V_CHROM_COEF_SIZE                        0x210

#define ISP_SC_COEFF_H_NUM                      (ISP_SC_H_COEF_SIZE/4)
#define ISP_SC_COEFF_V_NUM                      (ISP_SC_V_COEF_SIZE/4)
#define ISP_SC_COEFF_V_CHROMA_NUM               (ISP_SC_V_CHROM_COEF_SIZE/4)

#define ISP_LSC_BUF_SIZE                        (32 * 1024)

#define OFFLINE_BUFFER_NUM                      2

#define ISP_OFF_BUF_BIN                         0x00UL
#define ISP_OFF_BUF_FULL                        0x01UL
#define ISP_OFF_BUF_BOTH                        0x02UL
#define ISP_OFF_BUF_NONE                        0x03UL

/* Dynamic switch isp clk */
#define ISP_CLK_P_P             BIT_0   /* for normale,preview, video */
#define ISP_CLK_P_C             BIT_1   /* for capture */
#define ISP_CLK_P_S             BIT_2   /* for slow motion */

/* isp iommu channel r/w type */
#define ISP_IOMMU_CH_AR                         SPRD_IOMMU_EX_CH_READ
#define ISP_IOMMU_CH_AW                         SPRD_IOMMU_EX_CH_WRITE

/* isp outstanding mask */
#define ISP_AXI_ITI2AXIM_ROSTD_MASK             0x0000FF00

/* AXI ID for isp */
/* rd */
#define AR_ID_CFG                               (BIT_0)
#define AR_ID_BPC                               (BIT_1)
#define AR_ID_LSC                               (BIT_2)
#define AR_ID_FMCU                              (BIT_3)
#define AR_ID_FETCH_Y                           (BIT_4)
#define AR_ID_FETCH_U                           (BIT_5)
#define AR_ID_FETCH_V                           (BIT_6)

/* wr */
#define AW_ID_STORE_O_Y                         (BIT_0)
#define AW_ID_STORE_O_U                         (BIT_1)
#define AW_ID_STORE_O_V                         (BIT_2)
#define AW_ID_AEM                               (BIT_3)
#define AW_ID_AFL_GLB                           (BIT_4)
#define AW_ID_AFL_REGION                        (BIT_5)
#define AW_ID_BINNING                           (BIT_6)
#define AW_ID_BPC                               (BIT_7)
#define AW_ID_STORE_VID_Y                       (BIT_8)
#define AW_ID_STORE_VID_U                       (BIT_9)
#define AW_ID_STORE_VID_V                       (BIT_10)
#define AW_ID_STORE_VID_YUV     \
	(AW_ID_STORE_VID_Y | AW_ID_STORE_VID_U | AW_ID_STORE_VID_V)
#define AW_ID_STORE_PRE_CAP_Y                   (BIT_11)
#define AW_ID_STORE_PRE_CAP_U                   (BIT_12)
#define AW_ID_STORE_PRE_CAP_V                   (BIT_13)
#define AW_ID_STORE_PRE_CAP_YUV \
	(AW_ID_STORE_PRE_CAP_Y | AW_ID_STORE_PRE_CAP_U | AW_ID_STORE_PRE_CAP_V)

enum isp_wait_full_tx_done_state {
	WAIT_CLEAR,
	WAIT_BEGIN,
	WAIT_DONE,
};

/* isp CFG mode is default */
enum isp_work_mode {
	ISP_CFG_MODE,
	ISP_AP_MODE,
	ISP_WM_MAX
};

enum {
	ISP_ST_STOP = 0,
	ISP_ST_START,
};

enum isp_glb_reg_id {
	ISP_AXI_REG = 0,
	ISP_INIT_MASK_REG,
	ISP_INIT_CLR_REG,
	ISP_REG_MAX
};

enum isp_drv_rtn {
	ISP_RTN_SUCCESS = 0,
	ISP_RTN_PARA_ERR = 0x10,
	ISP_RTN_FRM_DECI_ERR,
	ISP_RTN_OUT_FMT_ERR,
	ISP_RTN_PATH_ADDR_ERR,
	ISP_RTN_PATH_FRAME_LOCKED,
	ISP_RTN_PATH_SC_ERR,
	ISP_RTN_PATH_IN_SIZE_ERR,
	ISP_RTN_PATH_TRIM_SIZE_ERR,
	ISP_RTN_PATH_OUT_SIZE_ERR,
	ISP_RTN_PATH_ENDIAN_ERR,
	ISP_RTN_IRQ_NUM_ERR,
	ISP_RTN_TIME_OUT,
	ISP_RTN_MAX
};

enum isp_path_mode {
	ISP_PRE_ONLINE = 0,
	ISP_PRE_OFFLINE,
	ISP_VID_ONLINE = 0x10,
	ISP_VID_ONLINE_CPP,
	ISP_VID_OFFLINE,
	ISP_CAP_OFFLINE,
	ISP_MODE_MAX,
};

/* definition of isp instance id */
enum isp_id {
	ISP_ID_0 = 0,
	ISP_ID_1,
	ISP_ID_MAX,
};

/* definition of scene id for each isp instance */
enum isp_scene_id {
	ISP_SCENE_PRE,
	ISP_SCENE_CAP,
	ISP_SCENE_NUM,
};

enum isp_scl_id {
	ISP_SCL_0 = 0,
	ISP_SCL_PRE,
	ISP_SCL_VID,
	ISP_SCL_CAP,
	ISP_SCL_MAX,
};

enum isp_path_id {
	ISP_PATH_PRE = 0,
	ISP_PATH_VID,
	ISP_PATH_CAP,
	ISP_PATH_MAX
};

enum isp_path_index {
	ISP_PATH_IDX_0 = 0x00,
	ISP_PATH_IDX_PRE = 0x01,
	ISP_PATH_IDX_VID = 0x02,
	ISP_PATH_IDX_CAP = 0x04,
	ISP_PATH_IDX_ALL = 0x07,
};

enum isp_cfg_id {
	ISP_PATH_INPUT_SIZE,
	ISP_PATH_INPUT_RECT,
	ISP_PATH_INPUT_FORMAT,
	ISP_PATH_OUTPUT_SIZE,
	ISP_PATH_OUTPUT_ADDR,
	ISP_PATH_OUTPUT_RESERVED_ADDR,
	ISP_PATH_STORE_CCE_ADDR,
	ISP_PATH_OUTPUT_FORMAT,
	ISP_PATH_FRM_DECI,
	ISP_PATH_WORK_MODE,
	ISP_PATH_ENABLE,
	ISP_PATH_SHRINK,
	ISP_PATH_DATA_ENDIAN,
	ISP_PATH_ZOOM_MODE,
	ISP_PATH_MODE,
	ISP_PATH_SN_MAX_SIZE,
	ISP_PATH_UFRAME_SYNC,
	ISP_CFG_MAX
};

enum isp_irq_id {
	ISP_PATH_PRE_DONE,
	ISP_PATH_VID_DONE,
	ISP_PATH_CAP_DONE,
	ISP_AEM_DONE,
	ISP_AEM_ERROR,
	ISP_AFL_DONE,
	ISP_AFM_DONE,
	ISP_BINNING_DONE,
	ISP_SHADOW_DONE,
	ISP_DCAM_SOF,
	ISP_HIST_DONE,
	ISP_HIST2_DONE,
	ISP_IMG_MAX,
};

enum isp_fetch_format {
	ISP_FETCH_YUV422_3FRAME = 0,
	ISP_FETCH_YUYV,
	ISP_FETCH_UYVY,
	ISP_FETCH_YVYU,
	ISP_FETCH_VYUY,
	ISP_FETCH_YUV422_2FRAME,
	ISP_FETCH_YVU422_2FRAME,
	ISP_FETCH_RAW_10,
	ISP_FETCH_CSI2_RAW_10,  /* MIPI RAW */
	ISP_FETCH_FULL_RGB,
	ISP_FETCH_YUV420_2FRAME = 10,
	ISP_FETCH_YVU420_2FRAME,
	ISP_FETCH_FORMAT_MAX
};

enum isp_store_format {
	ISP_STORE_UYVY = 0x00,
	ISP_STORE_YUV422_2FRAME,
	ISP_STORE_YVU422_2FRAME,
	ISP_STORE_YUV422_3FRAME,
	ISP_STORE_YUV420_2FRAME,
	ISP_STORE_YVU420_2FRAME,
	ISP_STORE_YUV420_3FRAME,
	ISP_STORE_RAW10,
	ISP_STORE_FULL_RGB8,
	ISP_STORE_FORMAT_MAX
};

enum isp_fmcu_cmd {
	P0_SDW_DONE = 0x10,	/* shadow done */
	P0_ALL_DONE,		/* all done */
	P0_LLD_DONE,		/* lens load done */
	C0_SDW_DONE,
	C0_ALL_DONE,
	C0_LLD_DONE,
	P1_SDW_DONE,
	P1_ALL_DONE,
	P1_LLD_DONE,
	C1_SDW_DONE,
	C1_ALL_DONE,
	C1_LLD_DONE,
	CFG_TRIGGER_PULSE,	/* CFG info FMCU page reg seting done*/
	SW_TRIGGER,		/* FPGA debug use*/
	FMCU_CMD_MAX
};

struct isp_clk_gate {
	uint32_t g0;
	uint32_t g1;
	uint32_t g2;
	uint32_t g3;
};

struct isp_ch_irq {
	int ch0;
	int ch1;
};

struct isp_raw_afm_statistic {
	uint32_t val[ISP_RAW_AFM_ITEM];
};

struct isp_node {
	uint32_t irq_val0;
	uint32_t irq_val1;
	uint32_t irq_val2;
	uint32_t irq_val3;
	uint32_t reserved;
	struct isp_k_time time;
};

struct isp_queue {
	struct isp_node node[ISP_QUEUE_LENGTH];
	struct isp_node *write;
	struct isp_node *read;
};

struct isp_deci_info {
	uint32_t deci_y_eb;
	uint32_t deci_y;
	uint32_t deci_x_eb;
	uint32_t deci_x;
};

struct isp_trim_info {
	uint32_t start_x;
	uint32_t start_y;
	uint32_t size_x;
	uint32_t size_y;
};

struct isp_endian_sel {
	unsigned char y_endian;
	unsigned char uv_endian;
};

struct isp_sc_tap {
	uint32_t y_tap;
	uint32_t uv_tap;
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

struct isp_scaler_info {
	uint32_t scaler_bypass;
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
	uint32_t scaler_in_width;
	uint32_t scaler_in_height;
	uint32_t scaler_out_width;
	uint32_t scaler_out_height;
	uint32_t *coeff_buf;
};

struct isp_b4awb_buf {
	uint32_t buf_id;
	uint32_t buf_flag;
	unsigned long buf_phys_addr;
};

struct isp_statis_buf_node {
	unsigned long buf_size;
	uint32_t  k_addr;
	uint32_t  u_addr;
	uint32_t  mfd;
};

#define ISP_BUF_SHORT_NAME_LEN	16

struct isp_buf_info {
	char name[ISP_BUF_SHORT_NAME_LEN+1];
	size_t size;
	struct dma_buf *dmabuf_p;
	void *buf;
	size_t buf_size;
	void *sw_addr;
	void *hw_addr;
};

struct isp_k_block {
	uint32_t lsc_bypass;
	uint32_t lsc_cap_grid_width;
	uint32_t lsc_cap_grid_pitch;
	uint32_t lsc_load_buf_id;
	uint32_t lsc_update_buf_id;
	uint32_t full_gamma_buf_id;
	uint32_t yuv_ygamma_buf_id;
	uint32_t lsc_buf_phys_addr;
	uint32_t anti_flicker_buf_phys_addr;
	uint32_t raw_nlm_buf_id;
/*  TODO: lsc_1_buf_id replaced with rlsc */
	uint32_t lsc_1d_buf_id;
	uint32_t rlsc_buf_id;
	uint32_t hsv_buf_id;
	uint32_t lsc_2d_weight_en;
	uint32_t fetch_raw_phys_addr;
	unsigned long full_gamma_buf_addr;
	void *nlm_vst_addr;
	void *nlm_ivst_addr;
	void *isp_lens_w_addr;
	void *dcam_lens_w_addr;
	unsigned long fetch_mfd;
	unsigned long lsc_mfd;
	struct isp_b4awb_buf b4awb_buf[ISP_BING4AWB_NUM];
	struct pfiommu_info fetch_pfinfo;
	struct pfiommu_info lsc_pfinfo;
	struct isp_buf_info lsc_buf_info;
};

struct isp_statis_buf {
	uint32_t buf_size;
	int buf_property;
	unsigned long phy_addr;
	unsigned long vir_addr;
	unsigned long addr_offset;
	unsigned long kaddr[2];
	struct pfiommu_info pfinfo;
};

struct isp_statis_buf_info {
	uint32_t buf_num;
	struct isp_img_size out_size;
	struct isp_statis_buf statis_buf[ISP_STATISTICS_BUF_MAX];
};

struct isp_statis_frm_queue {
	/* double the size because kfifo needs size as power of 2 */
	struct isp_statis_buf buf_array[ISP_STATISTICS_QUEUE_LEN * 2];
	struct kfifo fifo;
	spinlock_t lock;
};

struct isp_statis_buf_queue {
	struct isp_statis_buf buff[ISP_STATISTICS_QUEUE_LEN];
	struct isp_statis_buf *write;
	struct isp_statis_buf *read;
	spinlock_t lock;
};

struct isp_frm_queue {
	const char *owner;
	struct camera_frame frame[ISP_FRM_QUEUE_LENGTH + 1];
	int w_index;
	int r_index;
	uint32_t valid_cnt;
	spinlock_t lock;
};

struct isp_buf_queue {
	struct camera_frame frame[DCAM_FRM_CNT_MAX + 1];
	int w_index;
	int r_index;
	uint32_t valid_cnt;
	spinlock_t lock;
};

struct isp_store_info {
	uint32_t bypass;
	uint32_t endian;
	uint32_t speed_2x;
	uint32_t mirror_en;
	uint32_t color_format;
	uint32_t max_len_sel;
	uint32_t shadow_clr;
	uint32_t store_res;
	uint32_t rd_ctrl;
	uint32_t shadow_clr_sel;
	struct camera_size size;
	struct store_border border;
	struct isp_pitch_fs pitch;
};

struct isp_path_desc {
	uint32_t valid;
	uint32_t uv_sync_v;
	uint32_t scaler_bypass;
	uint32_t status;
	uint32_t path_mode;
	uint32_t frm_deci;
	uint32_t input_format;
	uint32_t output_format;
	uint32_t odata_mode;
	uint32_t frame_base_id;
	uint32_t output_frame_count;
	uint32_t path_sel;
	uint32_t uframe_sync;

	struct isp_buf_queue buf_queue;
	struct isp_frm_queue frame_queue;

	struct camera_size in_size;
	struct camera_rect in_rect;
	struct camera_size out_size;
	struct camera_size src;
	struct camera_size dst;
	struct isp_endian_sel data_endian;
	struct isp_deci_info deci_info;
	struct isp_trim_info trim0_info;
	struct isp_trim_info trim1_info;
	struct isp_regular_info regular_info;
	struct isp_scaler_info scaler_info;
	struct isp_store_info store_info;

	struct completion sof_com;
	struct completion tx_done_com;

	uint32_t wait_for_done;
	uint32_t is_update;
	uint32_t wait_for_sof;
	uint32_t need_stop;
	uint32_t need_wait;
	uint32_t shadow_done_cnt;
};

struct offline_ion_buf {
	struct dma_buf *dmabuf_p;
	void *buf;
	size_t buf_size;
	struct camera_addr addr;
	void *kva; /* kernel virtual address */
};

struct offline_buf_desc {
	size_t buf_len;
	struct offline_ion_buf ion_buf[ISP_FRM_QUEUE_LENGTH + 1];
	struct isp_buf_queue tmp_buf_queue;
	struct isp_frm_queue frame_queue;
	struct isp_frm_queue zsl_queue;
	uint32_t output_format;
	uint32_t output_frame_count;
};

struct isp_offline_desc {
	uint32_t valid;
	uint32_t status;
	struct offline_buf_desc buf_desc_bin;
	struct offline_buf_desc buf_desc_full;
	struct isp_endian_sel data_endian;
	struct camera_size src;
	struct camera_size dst;
	struct store_border border;
	struct isp_store_info store_info;
	uint32_t is_update;
	uint32_t shadow_done_cnt;
	uint32_t read_buf_err;
};

struct isp_fmcu_slice_desc {
	void *slice_handle;
	uint32_t fmcu_num;
	struct isp_buf_info cmdq_buf_info;
};

struct isp_fmcu_slw_desc {
	void *slw_handle;
	uint32_t *fmcu_addr_vir;
	uint32_t slw_flags;
	uint32_t status;
	uint32_t vid_num;
};

struct isp_sc_coeff {
	uint32_t buf[ISP_SC_COEFF_BUF_SIZE];
	uint32_t flag;
	struct isp_path_desc path;
};

struct isp_sc_coeff_queue {
	struct isp_sc_coeff coeff[ISP_SC_COEFF_BUF_COUNT];
	struct isp_sc_coeff *write;
	struct isp_sc_coeff *read;
	int w_index;
	int r_index;
	spinlock_t lock;
};

struct isp_sc_array {
	struct isp_sc_coeff_queue pre_queue;
	struct isp_sc_coeff_queue vid_queue;
	struct isp_sc_coeff_queue cap_queue;
	struct isp_sc_coeff coeff[ISP_SCL_MAX];
	uint32_t is_smooth_zoom;
	struct isp_offline_desc scl_off_desc;
};

struct isp_module {
	struct isp_path_desc isp_path[ISP_SCL_MAX];
	struct camera_frame path_reserved_frame[ISP_SCL_MAX];
	struct isp_offline_desc off_desc;
	struct isp_cctx_desc *cctx_desc;
	struct isp_sc_array *scl_array;
};

struct isp_statis_module {
	uint32_t statis_valid;
	struct isp_statis_frm_queue afl_statis_frm_queue;
	uint32_t afl_statis_cnt;
	uint32_t afl_int_done;
	struct isp_statis_frm_queue afm_statis_frm_queue;
	uint32_t afm_statis_cnt;
	struct isp_statis_frm_queue binning_statis_frm_queue;
	uint32_t binning_statis_cnt;
	uint32_t binning_int_done;
	struct isp_statis_frm_queue hist_statis_frm_queue;
	uint32_t hist_statis_cnt;
	struct isp_statis_frm_queue hist2_statis_frm_queue;
	uint32_t hist2_statis_cnt;
	struct isp_statis_buf aem_buf_reserved;
	struct isp_statis_buf afl_buf_reserved;
	struct isp_statis_buf afm_buf_reserved;
	struct isp_statis_buf binning_buf_reserved;
	struct isp_statis_buf hist_buf_reserved;
	struct isp_statis_buf hist2_buf_reserved;
	struct isp_statis_buf_queue aem_statis_queue; /*for irq read*/
	struct isp_statis_buf_queue afl_statis_queue; /*for irq read*/
	struct isp_statis_buf_queue afm_statis_queue; /*for irq read*/
	struct isp_statis_buf_queue binning_statis_queue; /*for irq read*/
	struct isp_statis_buf_queue hist_statis_queue; /*for irq read*/
	struct isp_statis_buf_queue hist2_statis_queue; /*for irq read*/
	struct camera_frame afm_frame_info;
	struct isp_statis_buf img_statis_buf;
};


struct isp_pipe_dev {
	/*
	 * composite idx(com_idx) is divided into 3 parts:
	 * @scene_id, pre(0) or cap(1)
	 * @isp_work_mode(0: cfg mode, 1:ap mode),
	 * @isp_id, isp instance id
	 * each part has 4bits.
	 *
	 * MSB                              LSB
	 * |----4-----|----4------|-----4------|
	 * | scene id | work_mode |   isp id   |
	 *
	 * i.e. idx = isp_id | work_mode << 4 | scene_id << 8
	 */
	uint32_t com_idx;
	uint32_t fmcu_owner; /* record the owner of fmcu */
	uint32_t cap_on;
	uint32_t cap_flag;
	uint32_t frm_cnt;
	uint32_t pre_state;
	uint32_t bin_path_miss_cnt; /* record the miss cnt of bin_tx done */
	uint32_t is_raw_capture;
	atomic_t cfg_map_lock;
	struct mutex isp_mutex;
	struct completion fmcu_com;
	struct completion irq_com;
	struct completion isr_done_lock;
	struct completion bin_stop;
	struct completion full_stop;
	struct isp_queue queue;
	struct isp_k_block isp_k_param;
	struct isp_fmcu_slice_desc fmcu_slice;
	struct isp_fmcu_slw_desc fmcu_slw;
	struct isp_module module_info;
	struct isp_statis_module statis_module_info;
	struct camera_frame offline_frame[2];
	struct completion offline_bin_thread_com;
	struct completion offline_full_thread_com;
	struct task_struct *offline_bin_thread;
	struct task_struct *offline_full_thread;
	uint32_t is_offline_bin_thread_stop;
	uint32_t is_offline_full_thread_stop;
	uint32_t *fmcu_addr_vir;
	unsigned long fmcu_addr_phy;
	uint32_t is_wait_fmcu;
	uint32_t is_3dnr_path_cfg; /* check if 3dnr path been configed */
	uint32_t is_3dnr; /* check if start capture of 3dnr coming */
	uint32_t is_hdr;
	uint32_t is_flash;
	/* set this flag when isp waiting for dcam full path tx done */
	enum isp_wait_full_tx_done_state wait_full_tx_done;
	uint32_t frm_cnt_3dnr;
	uint32_t frm_cnt_cap; // flash, hdr frame cap count
	struct camera_group *cam_grp;
	spinlock_t pre_lock;
	spinlock_t cap_lock;
	/*
	 * used to save the original bypass configs from hal/mw,
	 * each bit for one sub-block.
	 * 1, bypass; 0, work
	 */
	uint32_t sblk_ori_byp_map[ISP_SBLK_MAP_CNT];

	/* ISP flooding */
	int isr_count;
	unsigned long isr_last_time;

	/* distance between matched frame */
	int delta_full;

	bool is_yuv_sn;
};

typedef void(*isp_isr)(void *param);
typedef int(*isp_isr_func)(struct camera_frame *frame, void *param);
int sprd_isp_stop(void *isp_handle, int is_irq);
int sprd_isp_start_pipeline_bin(void *handle, uint32_t cap_flag);
int sprd_isp_start_pipeline_full(void *handle, uint32_t cap_flag);
int sprd_isp_start_pipeline_yuv(void *handle, unsigned int cap_flag);
int sprd_isp_stop_pipeline(void *handle);
int sprd_isp_force_stop_pipeline(void *handle);
int sprd_isp_start(void *isp_handle, struct camera_frame *frame);
int sprd_isp_get_afm_frame_info(void *isp_handle,
		struct camera_frame **out_frame);
int sprd_isp_get_offline_buffer(void *isp_handle,
	uint8_t off_type, struct camera_frame *out_frame);
int sprd_isp_set_offline_buffer(void *isp_handle, uint8_t off_type);
void sprd_isp_wait_for_buffers(void *isp_handle);
int sprd_isp_update_zoom_param(void *isp_handle,
			       enum isp_path_index path_index,
			       struct camera_size *in_size,
			       struct camera_rect *in_rect,
			       struct camera_size *out_size);
int set_isp_path_cfg(void *isp_handle, enum isp_path_index path_index,
	enum isp_cfg_id id, void *param);
int sprd_isp_module_en(void *isp_handle, enum isp_id iid);
int sprd_isp_module_dis(void *isp_handle, enum isp_id iid);
int sprd_isp_dev_init(void **isp_pipe_dev_handle, enum isp_id iid);
int sprd_isp_dev_deinit(void *isp_dev_handle, enum isp_id iid);
int sprd_isp_drv_init(struct platform_device *pdev);
void sprd_isp_drv_deinit(void);
int sprd_isp_parse_dt(struct device_node *dn, uint32_t *isp_count);
int sprd_isp_k_ioctl(void *isp_dev_handle, uint32_t cmd,
	unsigned long param);
int sprd_isp_reg_isr(enum isp_id iid,  enum isp_irq_id irq_id,
	isp_isr_func user_func, void *user_data);
int sprd_isp_slw_flags_init(void *isp_handle, struct isp_path_info *info);
void sprd_isp_drv_init_isp_cnt(void);
int isp_cfg_param(void *param,
                  struct isp_k_block *isp_k_param, struct isp_pipe_dev *dev);
int32_t isp_k_capability(void __user *param);
int isp_path_scaler(struct isp_module *module,
		    enum isp_path_index path_index,
		    struct isp_path_desc *path,
		    struct isp_sc_coeff *coeff);
void isp_wait_update_done(struct isp_module *module,
			  enum isp_path_index path_index, uint32_t *p_flag);
uint32_t isp_k_fetch_get_raw_phys_addr(void);
extern uint32_t int_reg_base[][ISP_SCENE_NUM];
uint32_t isp_k_fetch_get_raw_info(uint32_t *width,
	uint32_t *height);

int sprd_isp_external_unmap(void *isp_handle);
int isp_path_cap_with_vid_set_next_frm(struct isp_pipe_dev *dev);
void sprd_isp_glb_reg_awr(uint32_t idx, unsigned long addr,
                          uint32_t val, uint32_t reg_id);
void sprd_isp_glb_reg_owr(uint32_t idx, unsigned long addr,
                          uint32_t val, uint32_t reg_id);

extern uint32_t is_dual_cam;
/**is_dual_cam_dore :
 *BIT(0)  : if 1, it is under dual camera recovery processing.
 *BIT(5)  : if 1, it already set DCAM0 stream on under recovery.
 *BIT(8)  : if 1, DCAM1 is waiting for DCAM0 stream on under recovery.
 *BIT(9)  : if 1, it already stopped DCAM0 capture under recovery.
 *BIT(10) : if 1, it already stopped DCAM1 capture under recovery.
 *BIT(12) : if 1, DCAM1 is waiting for
			the finish of stopping DCAM0 capture under recovery.
 *BIT(13) : if 1, it already started dual camera capture
			with both DCAM0 and DCAM1 under recovery.
 *BIT(15) : if 1, an ioctl command is waiting for
			the finish of dual camera recovery.
*/
extern uint32_t is_dual_cam_dore;
extern uint32_t dual_cam_cap_sta;
extern bool has_dual_cap_started;
extern uint32_t fmcu_slice_capture_state;
extern uint32_t fmcu_slice_capture_state_dual;
extern uint32_t isp_frm_queue_len;
extern struct isp_pipe_dev *g_isp_dev_parray[ISP_MAX_COUNT];
extern struct isp_clk_gate isp_clk_gt;
void isp_clk_pause(enum isp_id iid, int i);
void isp_clk_resume(enum isp_id iid, int i);
void isp_handle_dcam_err(void *data);
int sprd_isp_reset(struct isp_pipe_dev *dev);

#endif
