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

#ifndef _DCAM_DRV_H_
#define _DCAM_DRV_H_

#include <linux/platform_device.h>

#include "sprd_img.h"
#include "sprd_isp_hw.h"
#include "dcam_reg.h"
#include "cam_iommu.h"

/* #define DCAM_DEBUG */
#ifdef DCAM_DEBUG
#define DCAM_TRACE				pr_info
#else
#define DCAM_TRACE				pr_debug
#endif
/* log in interrupt */
/* #define DCAM_DEBUG_INT */
#ifdef DCAM_DEBUG_INT
#define DCAM_TRACE_INT(a, b...)			pr_info(a, ##b)
#else
#define DCAM_TRACE_INT(a, b...)			pr_debug(a, ##b)
#endif

#define BIT_CAM_AHB_AXIM_SOFT_RST			(BIT(1))
#define BIT_CAM_AHB_DCAM0_SOFT_RST			(BIT(2))
#define BIT_CAM_AHB_DCAM1_SOFT_RST			(BIT(3))


#define DCAM_LOWEST_ADDR			0x800
#define DCAM_ADDR_INVALID(addr) \
	((unsigned long)(addr) < DCAM_LOWEST_ADDR)
#define DCAM_YUV_ADDR_INVALID(y, u, v) \
	(DCAM_ADDR_INVALID(y) && \
	 DCAM_ADDR_INVALID(u) && \
	 DCAM_ADDR_INVALID(v))
#define DCAM_PHYADDR_INVALID(phy_addr) \
	(((unsigned long)(phy_addr) < DCAM_LOWEST_ADDR) || \
	 (phy_addr) > 0xFFFFFF00)

#define DCAM_WAIT_FOREVER			0xFFFFFFFF

#define DCAM_PATH_3_FRM_CNT_MAX			32
#define DCAM_PATH_2_FRM_CNT_MAX			32
#define DCAM_PATH_1_FRM_CNT_MAX			32
#define DCAM_PATH_0_FRM_CNT_MAX			48
/* max between path_1_frm_cnt and path_2_frm_cnt */
#define DCAM_FRM_CNT_MAX			48
#define DCAM_HEIGHT_MIN				4
/* 640X480  div 10 */
#define DCAM_JPEG_LENGTH_MIN			30720

#define SHRINK_Y_UP_TH				235
#define SHRINK_Y_DN_TH				16
#define SHRINK_UV_UP_TH				240
#define SHRINK_UV_DN_TH				16
#define SHRINK_Y_OFFSET				16
#define SHRINK_Y_RANGE				2
#define SHRINK_C_OFFSET				16
#define SHRINK_C_RANGE				6

#define ISP_OVERLAP_ALIGN_16			128
#define DCAM_MAX_COUNT				2

#define DCAM_TIME_NULL		0
#define DCAM_TIME_SOF		1
#define DCAM_TIME_TX_DONE	2
#define DCAM_SOF_TIME_QUEUE_LENGTH     8

enum dcam_swtich_status {
	DCAM_SWITCH_IDLE = 0,
	DCAM_SWITCH_PAUSE,
	DCAM_SWITCH_DONE,
	DCAM_SWITCH_MAX
};

enum dcam_drv_rtn {
	DCAM_RTN_SUCCESS = 0,
	DCAM_RTN_PARA_ERR = 0x10,
	DCAM_RTN_IO_ID_ERR,
	DCAM_RTN_ISR_ID_ERR,
	DCAM_RTN_MASTER_SEL_ERR,
	DCAM_RTN_MODE_ERR,
	DCAM_RTN_TIMEOUT,

	DCAM_RTN_CAP_FRAME_SEL_ERR = 0x20,
	DCAM_RTN_CAP_IN_FORMAT_ERR,
	DCAM_RTN_CAP_IN_BITS_ERR,
	DCAM_RTN_CAP_IN_YUV_ERR,
	DCAM_RTN_CAP_SYNC_POL_ERR,
	DCAM_RTN_CAP_SKIP_FRAME_ERR,
	DCAM_RTN_CAP_FRAME_DECI_ERR,
	DCAM_RTN_CAP_XY_DECI_ERR,
	DCAM_RTN_CAP_FRAME_SIZE_ERR,
	DCAM_RTN_CAP_SENSOR_MODE_ERR,
	DCAM_RTN_CAP_JPEG_BUF_LEN_ERR,
	DCAM_RTN_CAP_IF_MODE_ERR,

	DCAM_RTN_PATH_SRC_SIZE_ERR = 0x30,
	DCAM_RTN_PATH_TRIM_SIZE_ERR,
	DCAM_RTN_PATH_DES_SIZE_ERR,
	DCAM_RTN_PATH_IN_FMT_ERR,
	DCAM_RTN_PATH_OUT_FMT_ERR,
	DCAM_RTN_PATH_SC_ERR,
	DCAM_RTN_PATH_SUB_SAMPLE_ERR,
	DCAM_RTN_PATH_ADDR_ERR,
	DCAM_RTN_PATH_FRAME_TOO_MANY,
	DCAM_RTN_PATH_FRAME_LOCKED,
	DCAM_RTN_PATH_NO_MEM,
	DCAM_RTN_PATH_GEN_COEFF_ERR,
	DCAM_RTN_PATH_SRC_ERR,
	DCAM_RTN_PATH_ENDIAN_ERR,
	DCAM_RTN_PATH_FRM_DECI_ERR,
	DCAM_RTN_MAX
};

enum dcam_irq_id {
	DCAM_SN_SOF = 0,
	DCAM_SN_EOF,
	DCAM_CAP_SOF,
	DCAM_CAP_EOF,
	DCAM_OVF,
	DCAM_AEM_HOLD_OVF,
	DCAM_ISP_ENABLE_PULSE,
	DCAM_IRQ_RESERVE_1,
	DCAM_IRQ_RESERVE_2,
	DCAM_IRQ_RESERVE_3,
	DCAM_CAP_LINE_ERR,
	DCAM_CAP_FRM_ERR,
	DCAM_FULL_PATH_END,
	DCAM_BIN_PATH_END,
	DCAM_AEM_PATH_END,
	DCAM_PDAF_PATH_END,
	DCAM_VCH2_PATH_END,
	DCAM_VCH3_PATH_END,
	DCAM_FULL_PATH_TX_DONE,
	DCAM_BIN_PATH_TX_DONE,
	DCAM_AEM_PATH_TX_DONE,
	DCAM_PDAF_PATH_TX_DONE,
	DCAM_VCH2_PATH_TX_DONE,
	DCAM_VCH3_PATH_TX_DONE,
	DCAM_MMU_INT,

	DCAM_IRQ_RESERVE_4,
	DCAM_IRQ_RESERVE_5,
	DCAM_IRQ_RESERVE_6,
	DCAM_IRQ_RESERVE_7,
	DCAM_IRQ_RESERVE_8,
	DCAM_IRQ_RESERVE_9,
	DCAM_IRQ_RESERVE_10,
	DCAM_IRQ_NUMBER
};

enum dcam_cfg_id {
	DCAM_CAP_INTERFACE = 0,
	DCAM_CAP_SENSOR_MODE,
	DCAM_CAP_BAYER_PATTERN,
	DCAM_CAP_YUV_TYPE,
	DCAM_CAP_SYNC_POL,
	DCAM_CAP_DATA_BITS,
	DCAM_CAP_DATA_PACKET,
	DCAM_CAP_PRE_SKIP_CNT,
	DCAM_CAP_FRM_DECI,
	DCAM_CAP_FRM_COUNT_CLR,
	DCAM_CAP_FRM_COUNT_GET,
	DCAM_CAP_FRM_PULSE_LINE,
	DCAM_CAP_INPUT_RECT,
	DCAM_CAP_IMAGE_XY_DECI,
	DCAM_CAP_SAMPLE_MODE,
	DCAM_CAP_ZOOM_MODE,

	DCAM_PATH_INPUT_RECT,
	DCAM_PATH_INPUT_ADDR,
	DCAM_PATH_OUTPUT_SIZE,
	DCAM_PATH_OUTPUT_FORMAT,
	DCAM_PATH_OUTPUT_ADDR,
	DCAM_PATH_OUTPUT_RESERVED_ADDR,
	DCAM_PATH_PDAF_OUTPUT_ADDR,
	DCAM_PATH_FRAME_BASE_ID,
	DCAM_PATH_IS_SCALE_EN,
	DCAM_PATH_DATA_ENDIAN,
	DCAM_PATH_SRC_SEL,
	DCAM_PATH_ENABLE,
	DCAM_PATH_FRAME_TYPE,
	DCAM_PATH_FRM_DECI,
	DCAM_PATH_BIN_RATIO,
	DCAM_PDAF_CONTROL,
	DCAM_PDAF_EXTR,
	DCAM_EBD_CONTROL,
	DCAM_CFG_ID_E_MAX
};

enum dcam_path_src_sel {
	DCAM_PATH_FROM_CAP = 0,
	DCAM_PATH_FROM_ISP,
	DCAM_PATH_FROM_NONE
};

enum dcam_data_endian {
	DCAM_ENDIAN_LITTLE = 0,
	DCAM_ENDIAN_BIG,
	DCAM_ENDIAN_HALFBIG,
	DCAM_ENDIAN_HALFLITTLE,
	DCAM_ENDIAN_MAX
};

enum{
	NO_RAW_CAPTURE = 0x0,
	FULL_RAW_CAPTURE,
	BIN_RAW_CAPTURE,
	RAW_CALLBACK,
};

enum dcam_glb_reg_id {
	DCAM_CFG_REG = 0,
	DCAM_CONTROL_REG,
	DCAM_INIT_MASK_REG,
	DCAM_INIT_CLR_REG,
	DCAM_AXI_STS_REG,
	DCAM_ENDIAN_REG,
	DCAM_AXI_ENDIAN_REG,
	DCAM_PATH_STOP_REG,
	DCAM_REG_MAX
};

enum dcam_v4l2_wtite_cmd_id {
	DCAM_V4L2_WRITE_STOP = 0x5AA5,
	DCAM_V4L2_WRITE_FREE_FRAME = 0xA55A,
	DCAM_V4L2_WRITE_MAX
};

enum crop_path {
	CROP_FULL_PATH,
	CROP_BIN_PATH,
};

/* pdaf, for set pdaf reg */
enum e_pdaf_mode {
	E_PDAF_NONE = 0,
	E_PDAF_TYPE1 = 1,
	E_PDAF_TYPE2 = 2,
	E_PDAF_TYPE3 = 3,
};
struct pdaf_extr_ctrl {
	uint32_t size_x : 2;
	uint32_t size_y : 2;
};
struct pdaf_skip_frm {
	uint32_t frm_mode : 1;
	uint32_t sgl_start : 1;
	uint32_t mul_en : 1;
	uint32_t skip_num : 4;
};
struct pdaf_extr_roi_start {
	uint32_t x : 13;
	uint32_t y : 13;
};
struct pdaf_extr_roi_size {
	uint32_t w : 13;
	uint32_t h : 13;
};
/* pdaf --end */

struct dcam_cap_sync_pol {
	unsigned char vsync_pol;
	unsigned char hsync_pol;
	unsigned char pclk_pol;
	unsigned char need_href;
	unsigned char pclk_src;
	unsigned char reserved[3];
};

struct camera_endian_sel {
	unsigned char y_endian;
	unsigned char uv_endian;
	unsigned char reserved0;
	unsigned char reserved1;
};

struct dcam_cap_dec {
	unsigned char x_factor;
	unsigned char y_factor;
	unsigned char x_mode;
	unsigned char reserved;
};

struct camera_path_dec {
	unsigned char x_factor;
	unsigned char y_factor;
	unsigned char reserved[2];
};

struct camera_size {
	uint32_t w;
	uint32_t h;
};

struct camera_rect {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
};

struct size_transfer {
	struct camera_size *pin; /* dcam in */
	struct camera_size *pout; /* dcam out */
	struct camera_rect *prect; /* for isp trim */
};

struct camera_addr {
	uint32_t yaddr;
	uint32_t uaddr;
	uint32_t vaddr;
	uint32_t yaddr_vir;
	uint32_t uaddr_vir;
	uint32_t vaddr_vir;
	uint32_t mfd_y;
	uint32_t mfd_u;
	uint32_t mfd_v;
	uint32_t user_fid;
};

struct camera_sc_tap {
	uint32_t y_tap;
	uint32_t uv_tap;
};

struct camera_deci {
	uint32_t deci_x_en;
	uint32_t deci_x;
	uint32_t deci_y_en;
	uint32_t deci_y;
};

struct frm_timestamp {
	struct timeval time; /* time without suspend */
	ktime_t boot_time; /* time from boot, including suspend */
};

struct camera_frame {
	uint32_t type;
	uint32_t lock;
	uint32_t flags;
	uint32_t fid;
	uint32_t width;
	uint32_t height;
	uint32_t yaddr;
	uint32_t uaddr;
	uint32_t vaddr;
	uint32_t yaddr_vir;
	uint32_t uaddr_vir;
	uint32_t vaddr_vir;
	uint32_t phy_addr;
	uint32_t vir_addr;
	uint32_t kaddr[2];
	uint32_t addr_offset;
	uint32_t buf_size;
	uint32_t irq_type;
	uint32_t irq_property;
	uint32_t frame_id;
	uint32_t user_fid;
	int frame_invalid;
	struct pfiommu_info pfinfo;
	struct frm_timestamp sof_ts; /* ts without/with suspend @SOF DONE */
	struct frm_timestamp btu_ts; /* ts without/with suspend @BACKtoUser */
	void *kva; /* kernel virtual address */
	uint32_t cam_id; /* from which cam, which scene */
};

struct camera_get_path_id {
	uint32_t fourcc;
	uint32_t is_path_work[CAMERA_MAX_PATH];
	uint32_t need_isp_tool;
	uint32_t need_isp;
	uint32_t rt_refocus;
	struct camera_size input_size;
	struct camera_rect input_trim;
	struct camera_size output_size;
	uint32_t raw_callback;
};

struct dcam_path_info {
	uint32_t line_buf;
	uint32_t support_yuv;
	uint32_t support_raw;
	uint32_t support_jpeg;
	uint32_t support_scaling;
	uint32_t support_trim;
	uint32_t is_scaleing_path;
};

struct isp_path_info {
	void *fmcu_addr_vir;
	uint32_t is_slow_motion;
};

struct dcam_path_capability {
	uint32_t count;
	uint32_t support_3dnr_mode;
	struct dcam_path_info path_info[CAMERA_MAX_PATH];
};

struct log_frame_cnt {
	char tx_done_bin;
	char tx_done_full;
};

struct dcam_path_time_queue {
	struct frm_timestamp t[DCAM_SOF_TIME_QUEUE_LENGTH+1];
	struct frm_timestamp *write;
	struct frm_timestamp *read;
	int w_index;
	int r_index;
	spinlock_t lock;
};

struct dcam_time_queue {
	struct dcam_path_time_queue sof_t;
};

struct dcam_sof_time_queue {
	struct dcam_time_queue tq[2];
};

typedef int (*dcam_isr_func) (struct camera_frame *frame, void *u_data);
typedef int (*dcam_unmap_func) (void *isp_handle);

int sprd_dcam_module_init(enum dcam_id idx, void *dev_handle);
int sprd_dcam_module_deinit(enum dcam_id idx);
int sprd_dcam_module_en(enum dcam_id idx);
int sprd_dcam_module_dis(enum dcam_id idx);
int sprd_dcam_start(enum dcam_id idx, struct camera_frame *frame,
			void *statis_module);
int sprd_dcam_stop(enum dcam_id idx, int is_irq, void *func, void *param);
int sprd_dcam_reg_isr(enum dcam_id idx, enum dcam_irq_id id,
		      dcam_isr_func user_func, void *u_data);
int set_dcam_cap_cfg(enum dcam_id idx, enum dcam_cfg_id id, void *param);
int sprd_dcam_cap_get_info(enum dcam_id idx, enum dcam_cfg_id id, void *param);
int sprd_dcam_read_registers(enum dcam_id idx, uint32_t *reg_buf,
			     uint32_t *buf_len);
void sprd_dcam_glb_reg_awr(enum dcam_id idx, unsigned long addr,
			   uint32_t val, uint32_t reg_id);
void sprd_dcam_glb_reg_owr(enum dcam_id idx, unsigned long addr,
			   uint32_t val, uint32_t reg_id);
void sprd_dcam_glb_reg_mwr(enum dcam_id idx, unsigned long addr,
			   uint32_t mask, uint32_t val,
			   uint32_t reg_id);
int sprd_dcam_drv_init(struct platform_device *pdev);
void sprd_dcam_drv_deinit(void);
int sprd_camera_get_path_id(struct camera_get_path_id *path_id,
	uint32_t *channel_id, uint32_t scene_mode);
int sprd_dcam_get_path_capability(struct dcam_path_capability *capacity);
int sprd_dcam_parse_dt(struct device_node *dn, uint32_t *dcam_count);
int set_dcam_raw_path_cfg(enum dcam_id idx, enum dcam_cfg_id id, void *param);
int set_dcam_full_path_cfg(enum dcam_id idx, enum dcam_cfg_id id, void *param);
int set_dcam_bin_path_cfg(enum dcam_id idx, enum dcam_cfg_id id, void *param);
int set_dcam_full_path_cfg(enum dcam_id idx, enum dcam_cfg_id id, void *param);
int set_dcam_pdaf_path_cfg(enum dcam_id idx, enum dcam_cfg_id id, void *param);
int set_dcam_aem_path_cfg(enum dcam_id idx, enum dcam_cfg_id id, void *param);
int set_dcam_ebd_path_cfg(enum dcam_id idx, enum dcam_cfg_id id, void *param);
int set_dcam_crop(enum dcam_id idx, struct camera_rect rect,
			enum crop_path path);
int dcam_aem_set_next_frm(void *statis_module,
			enum dcam_id idx, enum isp_3a_block_id block_index);
void sprd_dcam_enable_int(enum dcam_id idx);
int dcam_pdaf_set_next_frm(void *statis_module,
			enum dcam_id idx, enum isp_3a_block_id block_index);
int dcam_ebd_set_next_frm(void *statis_module,
			enum dcam_id idx, enum isp_3a_block_id block_index);
int dcam_raw_rt_set_next_frm(void *statis_module,
			enum dcam_id idx, enum isp_3a_block_id block_index);

void dcam_path_pause(int id);
void dcam_path_resume(int id);
/* log for debug */
void dcam_reg_trace(enum dcam_id idx, unsigned long reg_start,
			unsigned long reg_end);
void align_for_bin(struct camera_size *p, uint32_t r);
uint32_t cal_bin_ratio(struct camera_size src,
			struct camera_size dst);
void refresh_bin_trim_size(struct camera_rect *p, uint32_t ratio);
void update_trim_size(enum dcam_id idx, struct camera_rect *p,
	uint32_t channel_id);
void log_frame_init(void);


extern atomic_t dcam_full_path_time_flag;
#endif /* _DCAM_DRV_H_ */
