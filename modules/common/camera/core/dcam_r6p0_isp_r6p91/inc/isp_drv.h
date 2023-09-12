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

#include <linux/completion.h>
#include <linux/spinlock_types.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include "sprd_isp_r6p91.h"
#include "dcam_drv.h"
#include "isp_reg.h"

/*irq line number in system*/
#define IRQ_ISP_INT 12
#define ISP_IRQ                            IRQ_ISP_INT
#define ISP_IRQ_HW_MASK                    (0xFFFFFFFF)

#define ISP_QUEUE_LENGTH 16
#define ISP_BING4AWB_NUM 2

#define ISP_BUF_QUEUE_LENGTH		16
#define ISP_BING4AWB_NUM			2
#define ISP_STATISTICS_BUF_MAX		4
#define ISP_STATISTICS_QUEUE_LEN	8
#define ISP_IMG_OUTPUT_PATH_MAX		3
#define ISP_FRM_QUEUE_LENGTH		8
#define ISP_PIXEL_ALIGN_WIDTH		4
#define ISP_PIXEL_ALIGN_HEIGHT		2
#define ISP_RAW_AFM_ITEM		40

#define ISP_LOWEST_ADDR            0x800
#define ISP_ADDR_INVALID(addr)     \
			((unsigned long)(addr) < (unsigned long)ISP_LOWEST_ADDR)


#define STORE_WR_CH_ID (BIT_0)
#define STATIS_WR_CH_ID (BIT_2 | BIT_4 | BIT_5)
#define FETCH_RD_CH_ID (BIT_0)
#define LENS_RD_CH_ID (BIT_2)
#define BPC_RD_CH_ID (BIT_3)



#define ISP_IRQ_ERR_MASK0 \
	((1<<ISP_INT_ISP2DCAM_AFIFO_FULL) | \
	(1<<ISP_INT_STORE_BUF_FULL) | \
	(1<<ISP_INT_DCAM_BUF_FULL))

#define ISP_IRQ_ERR_MASK1 \
	((1<<ISP_INT_BPC_ERR0) | \
	(1<<ISP_INT_BPC_ERR1) | \
	(1<<ISP_INT_BPC_ERR2) | \
	(1<<ISP_INT_BINNING_ERR0) | \
	(1<<ISP_INT_BINNING_ERR1) | \
	(1<<ISP_INT_AWBM_ERR) | \
	(1<<ISP_INT_DISPATCH_BUF_FULL))

#define ISP_MMU_IRQ_ERR_MASK_R \
	((1 << RAW_OUT_OF_RNGE_R) | \
	(1 << RAW_UNSECURE_R) | \
	(1 << RAW_INVALID_PAGE_R) | \
	(1 << RAW_INVALID_ID_R))

#define ISP_MMU_IRQ_ERR_MASK_W \
	((1 << RAW_OUT_OF_RNGE_W) | \
	(1 << RAW_UNSECURE_W) | \
	(1 << RAW_INVALID_PAGE_W) | \
	(1 << RAW_INVALID_ID_W))

#define ISP_MMU_IRQ_ERR_MASK \
	(ISP_MMU_IRQ_ERR_MASK_R | \
	ISP_MMU_IRQ_ERR_MASK_W)

#define ISP_INT_LINE_MASK0 \
	((1<<ISP_INT_ISP_ALL_DONE) | \
	(1<<ISP_INT_SHADOW_DONE) | \
	(1<<ISP_INT_STORE_DONE) | \
	(1<<ISP_INT_AEM_START) | \
	(1<<ISP_INT_AEM_DONE) | \
	(1<<ISP_INT_AFM_RGB_START) | \
	(1<<ISP_INT_AFM_RGB_DONE) | \
	(1<<ISP_INT_BINNING_DONE) | \
	(1<<ISP_INT_BINNING_START) | \
	(1<<ISP_INT_AFL_START) | \
	(1<<ISP_INT_AFL_DONE) | \
	(1<<ISP_INT_DCAM_SOF) | \
	(1<<ISP_INT_DCAM_EOF) | \
	(1<<ISP_INT_HIST_START) | \
	(1<<ISP_INT_HIST_DONE) | \
	(1<<ISP_INT_HIST2_START) | \
	(1<<ISP_INT_HIST2_DONE) | \
	(1<<ISP_INT_HIST2_WIN0_DONE) | \
	(1<<ISP_INT_HIST2_WIN1_DONE) | \
	(1<<ISP_INT_HIST2_WIN2_DONE) | \
	(1<<ISP_INT_HIST2_WIN3_DONE) | \
	ISP_IRQ_ERR_MASK0)

#define ISP_INT_LINE_MASK1 \
	(ISP_IRQ_ERR_MASK1)

#define ISP_INT_LINE_MASK2 \
	((1<<ISP_INT_AFL_NEW_SRAM_DONE) | \
	(1<<ISP_INT_AFL_NEW_SRAM_START) | \
	(1<<ISP_INT_AFL_NEW_DDR_DONE) | \
	(1<<ISP_INT_AFL_NEW_DDR_START))

#define ISP_INT_LINE_MASK3 \
	((1<<ISP_INT_YUV_DONE) | \
	(1<<ISP_INT_YUV_POSTCDN_DONE) | \
	(1<<ISP_INT_YUV_CDN_DONE) | \
	(1<<ISP_INT_YUV_EE_DONE) | \
	(1<<ISP_INT_YUV_NLM_DONE) | \
	(1<<ISP_INT_YUV_PRFUV_DONE) | \
	(1<<ISP_INT_YUV_PRFY_DONE) | \
	(1<<ISP_INT_YUV_START) | \
	(1<<ISP_INT_FRGB_CCE_DONE) | \
	(1<<ISP_INT_FRGB_CCE_START) | \
	(1<<ISP_INT_FRGB_DONE) | \
	(1<<ISP_INT_FRGB_START) | \
	(1<<ISP_INT_FRGB_CFCE_DONE) | \
	(1<<ISP_INT_FRGB_CFCE_START) | \
	(1<<ISP_INT_RRGB_DONE) | \
	(1<<ISP_INT_RRGB_BDN_DONE) | \
	(1<<ISP_INT_RRGB_PWD_DONE) | \
	(1<<ISP_INT_RRGB_BPC_DONE) | \
	(1<<ISP_INT_RRGB_LENS_DONE) | \
	(1<<ISP_INT_RRGB_NLM_DONE) | \
	(1<<ISP_INT_RRGB_START))

enum isp_drv_rtn {
	ISP_RTN_SUCCESS = 0,
	ISP_RTN_PARA_ERR = 0x10,
	ISP_RTN_IRQ_NUM_ERR,
	ISP_RTN_MAX
};

enum isp_irq_id {
	ISP_AEM_DONE,
	ISP_AFL_DONE,
	ISP_AFM_DONE,
	ISP_BINNING_DONE,
	ISP_HIST_DONE,
	ISP_SHADOW_DONE,
	ISP_DCAM_SOF,
	ISP_STORE_DONE,
	ISP_IMG_MAX,
};

struct isp_clk_tag {
	char *clock;
	char *clk_name;
};

struct isp_statis_buf_node {
	unsigned long buf_size;
	unsigned int  k_addr;
	unsigned int  u_addr;
	unsigned int  mfd;
};

struct isp_k_block {
	struct platform_device *isp_pdev;
	unsigned int is_raw_capture;
	unsigned int lsc_load_buf_id;
	unsigned int lsc_update_buf_id;
	unsigned int full_gamma_buf_id;
	unsigned int yuv_ygamma_buf_id;
	unsigned long full_gamma_buf_addr;
	unsigned int lsc_buf_phys_addr;
	unsigned int anti_flicker_buf_phys_addr;
	unsigned int raw_nlm_buf_id;
	unsigned int lsc_2d_weight_en;
	unsigned int fetch_raw_phys_addr;
	unsigned long fetch_mfd;
	unsigned long lsc_mfd;
	struct pfiommu_info fetch_pfinfo;
	struct pfiommu_info store_pfinfo;
	struct pfiommu_info lsc_pfinfo;
	unsigned int is_lsc_param_init_flag;
	unsigned int lsc_param_load_count;
	atomic_t lsc_updated;
};

struct isp_statis_buf {
	unsigned int buf_size;
	int buf_property;
	unsigned long phy_addr;
	unsigned long vir_addr;
	unsigned long addr_offset;
	unsigned long kaddr[2];
	unsigned long mfd;
	struct pfiommu_info pfinfo;
	void *ion;
	unsigned long dev_fd;
};

struct isp_statis_frm_queue {
	struct isp_statis_buf buf_array[ISP_STATISTICS_QUEUE_LEN];
	unsigned int valid_cnt;
};

struct isp_statis_buf_queue {
	struct isp_statis_buf buff[ISP_STATISTICS_QUEUE_LEN];
	struct isp_statis_buf *write;
	struct isp_statis_buf *read;
	spinlock_t lock;
};

struct isp_buf_queue {
	struct isp_buf_node node[DCAM_FRM_CNT_MAX];
	struct isp_buf_node *write;
	struct isp_buf_node *read;
	uint32_t cnt;
};

struct isp_statis_module {
	struct isp_statis_frm_queue aem_statis_frm_queue;
	unsigned int aem_statis_cnt;
	struct isp_statis_frm_queue afl_statis_frm_queue;
	unsigned int afl_statis_cnt;
	struct isp_statis_frm_queue afm_statis_frm_queue;
	unsigned int afm_statis_cnt;
	struct isp_statis_frm_queue binning_statis_frm_queue;
	unsigned int binning_statis_cnt;
	struct isp_statis_frm_queue hist_statis_frm_queue;
	unsigned int hist_statis_cnt;
	struct isp_statis_buf aem_buf_reserved;
	struct isp_statis_buf afl_buf_reserved;
	struct isp_statis_buf afm_buf_reserved;
	struct isp_statis_buf binning_buf_reserved;
	struct isp_statis_buf hist_buf_reserved;
	struct isp_statis_buf_queue aem_statis_queue; /*for irq read*/
	struct isp_statis_buf_queue afl_statis_queue; /*for irq read*/
	struct isp_statis_buf_queue afm_statis_queue; /*for irq read*/
	struct isp_statis_buf_queue binning_statis_queue; /*for irq read*/
	struct isp_statis_buf_queue hist_statis_queue; /*for irq read*/
	struct isp_statis_buf buf_node;
	struct isp_statis_buf img_statis_buf;
};

struct isp_pipe_dev {
	struct mutex isp_mutex;
	struct isp_k_block isp_k_param;
	struct isp_statis_module statis_module_info;
};

typedef void(*isp_isr)(void *param);
typedef int(*isp_isr_func)(struct camera_frame *frame, void *param);

struct isp_node {
	uint32_t irq_val0;
	uint32_t irq_val1;
	uint32_t irq_val2;
	uint32_t irq_val3;
	uint32_t reserved;
	struct isp_time time;
};

int32_t isp_capability(void *param);
int32_t isp_cfg_param(void  *param, struct isp_k_block *isp_k_param);
int sprd_isp_reg_isr(enum isp_irq_id irq_id, isp_isr_func user_func,
			void *user_data);
int sprd_isp_parse_dt(struct device_node *dn);
void sprd_isp_drv_init(void);
int sprd_isp_dev_init(void **isp_pipe_dev_handle);
int isp_axi_waiting(void);
int sprd_isp_unmap_buf(void *isp_pipe_dev_handle);
int isp_set_statis_buf(void *isp_pipe_dev_handle);
int sprd_isp_dev_deinit(void *isp_pipe_dev_handle);
int sprd_isp_k_ioctl(void *isp_pipe_dev_handle, unsigned int cmd,
	unsigned long param);
void print_isp_regs(void);
int isp_reset(void);
int switch_isp_clk(unsigned char high);

#endif
