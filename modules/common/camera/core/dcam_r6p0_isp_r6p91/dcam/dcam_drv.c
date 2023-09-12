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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <sprd_mm.h>
#include <linux/vmalloc.h>
#include <linux/videodev2.h>
//#include <linux/wakelock.h>
#include <linux/version.h>
#include "dcam_drv.h"
#include "gen_scale_coef.h"
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <linux/pm_runtime.h>
#else
#include <video/sprd_mmsys_pw_domain.h>
#endif
#include "csi_api.h"
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
//#include <linux/mfd/syscon/sprd-glb.h>
#include "isp_reg.h"
#include "isp_drv.h"
#include <dt-bindings/soc/sprd,pike2-mask.h>
#include <dt-bindings/soc/sprd,pike2-regs.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "DCAM_DRV: %d " fmt, __LINE__

#define DCAM_LOWEST_ADDR                               0x800
#define DCAM_ADDR_INVALID(addr)       ((unsigned long)(addr) < DCAM_LOWEST_ADDR)
#define DCAM_YUV_ADDR_INVALID(y, u, v) \
	(DCAM_ADDR_INVALID(y) && \
	DCAM_ADDR_INVALID(u) && \
	DCAM_ADDR_INVALID(v))

#define DCAM_SC1_H_TAB_OFFSET                          0x400
#define DCAM_SC1_V_TAB_OFFSET                          0x4F0
#define DCAM_SC1_V_CHROMA_TAB_OFFSET                   0x8F0

#define DCAM_SC2_H_TAB_OFFSET                          0x1400
#define DCAM_SC2_V_TAB_OFFSET                          0x14F0
#define DCAM_SC2_V_CHROMA_TAB_OFFSET                   0x18F0

#define DCAM_SC_COEFF_BUF_SIZE                         (24 << 10)
#define DCAM_SC_COEFF_COEF_SIZE                        (1 << 10)
#define DCAM_SC_COEFF_TMP_SIZE                         (21 << 10)
#define DCAM_SC_COEFF_BUF_COUNT                        2


#define DCAM_SC_H_COEF_SIZE                            (0xC0)
#define DCAM_SC_V_COEF_SIZE                            (0x210)
#define DCAM_SC_V_CHROM_COEF_SIZE                      (0x210)

#define DCAM_SC_COEFF_H_NUM                        (DCAM_SC_H_COEF_SIZE/4)
#define DCAM_SC_COEFF_V_NUM                        (DCAM_SC_V_COEF_SIZE/4)
#define DCAM_SC_COEFF_V_CHROMA_NUM                 (DCAM_SC_V_CHROM_COEF_SIZE/4)

#define DCAM_AXI_STOP_TIMEOUT                          1000

#ifdef CONFIG_SC_FPGA
#define DCAM_PATH_TIMEOUT                              msecs_to_jiffies(500*10)
#else
#define DCAM_PATH_TIMEOUT                              msecs_to_jiffies(500)
#endif

#define DCAM_FRM_QUEUE_LENGTH                          4

#define DCAM_STATE_QUICKQUIT                           0x01


typedef void (*dcam_isr)(void);

enum {
	DCAM_FRM_UNLOCK = 0,
	DCAM_FRM_LOCK_WRITE = 0x10011001,
	DCAM_FRM_LOCK_READ = 0x01100110
};

enum {
	DCAM_ST_STOP = 0,
	DCAM_ST_START,
};

struct dcam_cap_desc {
	uint32_t                   interface;
	uint32_t                   input_format;
	uint32_t                   frame_deci_factor;
	uint32_t                   img_x_deci_factor;
	uint32_t                   img_y_deci_factor;
};

struct dcam_path_valid {
	uint32_t                   input_size    :1;
	uint32_t                   input_rect    :1;
	uint32_t                   output_size   :1;
	uint32_t                   output_format :1;
	uint32_t                   src_sel       :1;
	uint32_t                   data_endian   :1;
	uint32_t                   frame_deci    :1;
	uint32_t                   scale_tap     :1;
	uint32_t                   v_deci        :1;
	uint32_t                   rot_mode      :1;
	uint32_t                   shrink        :1;
	uint32_t                   pdaf_ctrl     :1;

};

struct dcam_frm_queue {
	struct camera_frame        frm_array[DCAM_FRM_QUEUE_LENGTH];
	uint32_t                   valid_cnt;
};

struct dcam_buf_queue {
	struct camera_frame         frame[DCAM_FRM_CNT_MAX];
	struct camera_frame         *write;
	struct camera_frame         *read;
	spinlock_t                  lock;
};

struct dcam_path_desc {
	struct camera_size         input_size;
	struct camera_rect         input_rect;
	struct camera_size         sc_input_size;
	struct camera_size         output_size;
	struct camera_frame        input_frame;
	struct dcam_frm_queue      frame_queue;
	struct dcam_buf_queue      buf_queue;
	struct dcam_endian_sel     data_endian;
	struct dcam_sc_tap         scale_tap;
	struct dcam_deci           deci_val;
	struct dcam_path_valid     valid_param;
	uint32_t                   frame_base_id;
	uint32_t                   output_frame_count;
	uint32_t                   output_format;
	uint32_t                   src_sel;
	uint32_t                   rot_mode;
	uint32_t                   frame_deci;
	uint32_t                   valid;
	uint32_t                   status;
	uint32_t                   wait_for_done;
	uint32_t                   is_update;
	uint32_t                   wait_for_sof;
	uint32_t                   need_stop;
	uint32_t                   need_wait;
	int32_t                    sof_cnt;

	struct completion          tx_done_com;
	struct completion          sof_com;

	struct dcam_regular_desc   regular_desc;
	struct sprd_pdaf_control   pdaf_ctrl;
};

struct dcam_module {
	uint32_t                   dcam_mode;
	uint32_t                   module_addr;
	struct dcam_cap_desc       dcam_cap;
	struct dcam_path_desc      dcam_path0;
	struct dcam_path_desc      dcam_path1;
	struct dcam_path_desc      dcam_path2;
	struct camera_frame        path0_frame[DCAM_PATH_0_FRM_CNT_MAX];
	struct camera_frame        path1_frame[DCAM_PATH_1_FRM_CNT_MAX];
	struct camera_frame        path2_frame[DCAM_PATH_2_FRM_CNT_MAX];

	struct camera_frame        path_reserved_frame[DCAM_PATH_MAX];

	struct completion          stop_com;
	struct completion          resize_done_com;
	struct completion          rotation_done_com;
	struct completion          scale_coeff_mem_com;
	uint32_t                   wait_stop;
	uint32_t                   wait_resize_done;
	uint32_t                   wait_rotation_done;
	uint32_t                   err_happened;
	uint32_t                   state;
	void                       *isp_dev_handle;
};

struct dcam_sc_coeff {
	uint32_t                   buf[DCAM_SC_COEFF_BUF_SIZE];
	uint32_t                   flag;
	struct dcam_path_desc      dcam_path1;
};

struct dcam_sc_array {
	struct dcam_sc_coeff      scaling_coeff[DCAM_SC_COEFF_BUF_COUNT];
	struct dcam_sc_coeff      *scaling_coeff_queue[DCAM_SC_COEFF_BUF_COUNT];
	uint32_t                   valid_cnt;
	uint32_t                   is_smooth_zoom;
};

static struct platform_device *s_dcam_pdev;
static atomic_t                 s_dcam_users = ATOMIC_INIT(0);
static atomic_t                 s_resize_flag = ATOMIC_INIT(0);
static atomic_t                 s_rotation_flag = ATOMIC_INIT(0);
static struct dcam_module      *s_p_dcam_mod;
static dcam_isr_func            s_user_func[DCAM_IRQ_NUMBER];
static void                     *s_user_data[DCAM_IRQ_NUMBER];
static struct dcam_sc_array    *s_dcam_sc_array;
static struct wakeup_source         *dcam_wakelock;

unsigned long            s_dcam_regbase;
unsigned long            s_dcam_ahbbase;
static int                      s_dcam_irq;

static struct completion dcam_resizer_com;
static DEFINE_MUTEX(dcam_scale_sema);
static DEFINE_MUTEX(dcam_module_sema);
static DEFINE_SPINLOCK(dcam_lock);
static DEFINE_SPINLOCK(dcam_glb_reg_cfg_lock);
static DEFINE_SPINLOCK(dcam_glb_reg_control_lock);
static DEFINE_SPINLOCK(dcam_glb_reg_mask_lock);
static DEFINE_SPINLOCK(dcam_glb_reg_clr_lock);
static DEFINE_SPINLOCK(dcam_glb_reg_ahbm_sts_lock);
static DEFINE_SPINLOCK(dcam_glb_reg_endian_lock);

static struct clk *dcam0_clk;
static struct clk *dcam0_clk_parent;
static struct clk *dcam0_clk_default;
static struct clk *dcam0_eb;
static struct clk *dcam0_axi_clk;
static struct clk *dcam0_axi_clk_parent;
static struct clk *dcam0_axi_clk_default;
static struct clk *dcam0_axi_eb;
static struct regmap *cam_ahb_gpr;
static struct regmap *aon_apb_gpr;

void dcam_glb_reg_awr(unsigned long addr, uint32_t val, uint32_t reg_id)
{
	unsigned long flag;

	switch (reg_id) {
	case DCAM_CFG_REG:
		spin_lock_irqsave(&dcam_glb_reg_cfg_lock, flag);
		REG_WR(addr, REG_RD(addr) & (val));
		spin_unlock_irqrestore(&dcam_glb_reg_cfg_lock, flag);
		break;
	case DCAM_CONTROL_REG:
		spin_lock_irqsave(&dcam_glb_reg_control_lock, flag);
		REG_WR(addr, REG_RD(addr) & (val));
		spin_unlock_irqrestore(&dcam_glb_reg_control_lock, flag);
		break;
	case DCAM_INIT_MASK_REG:
		spin_lock_irqsave(&dcam_glb_reg_mask_lock, flag);
		REG_WR(addr, REG_RD(addr) & (val));
		spin_unlock_irqrestore(&dcam_glb_reg_mask_lock, flag);
		break;
	case DCAM_INIT_CLR_REG:
		spin_lock_irqsave(&dcam_glb_reg_clr_lock, flag);
		REG_WR(addr, REG_RD(addr) & (val));
		spin_unlock_irqrestore(&dcam_glb_reg_clr_lock, flag);
		break;
	case DCAM_AHBM_STS_REG:
		spin_lock_irqsave(&dcam_glb_reg_ahbm_sts_lock, flag);
		REG_WR(addr, REG_RD(addr) & (val));
		spin_unlock_irqrestore(&dcam_glb_reg_ahbm_sts_lock, flag);
		break;
	case DCAM_ENDIAN_REG:
		spin_lock_irqsave(&dcam_glb_reg_endian_lock, flag);
		REG_WR(addr, REG_RD(addr) & (val));
		spin_unlock_irqrestore(&dcam_glb_reg_endian_lock, flag);
		break;
	default:
		REG_WR(addr, REG_RD(addr) & (val));
		break;
	}
}

void dcam_glb_reg_owr(unsigned long addr, uint32_t val, uint32_t reg_id)
{
	unsigned long flag;

	switch (reg_id) {
	case DCAM_CFG_REG:
		spin_lock_irqsave(&dcam_glb_reg_cfg_lock, flag);
		REG_WR(addr, REG_RD(addr) | (val));
		spin_unlock_irqrestore(&dcam_glb_reg_cfg_lock, flag);
		break;
	case DCAM_CONTROL_REG:
		spin_lock_irqsave(&dcam_glb_reg_control_lock, flag);
		REG_WR(addr, REG_RD(addr) | (val));
		spin_unlock_irqrestore(&dcam_glb_reg_control_lock, flag);
		break;
	case DCAM_INIT_MASK_REG:
		spin_lock_irqsave(&dcam_glb_reg_mask_lock, flag);
		REG_WR(addr, REG_RD(addr) | (val));
		spin_unlock_irqrestore(&dcam_glb_reg_mask_lock, flag);
		break;
	case DCAM_INIT_CLR_REG:
		spin_lock_irqsave(&dcam_glb_reg_clr_lock, flag);
		REG_WR(addr, REG_RD(addr) | (val));
		spin_unlock_irqrestore(&dcam_glb_reg_clr_lock, flag);
		break;
	case DCAM_AHBM_STS_REG:
		spin_lock_irqsave(&dcam_glb_reg_ahbm_sts_lock, flag);
		REG_WR(addr, REG_RD(addr) | (val));
		spin_unlock_irqrestore(&dcam_glb_reg_ahbm_sts_lock, flag);
		break;
	case DCAM_ENDIAN_REG:
		spin_lock_irqsave(&dcam_glb_reg_endian_lock, flag);
		REG_WR(addr, REG_RD(addr) | (val));
		spin_unlock_irqrestore(&dcam_glb_reg_endian_lock, flag);
		break;
	default:
		REG_WR(addr, REG_RD(addr) | (val));
		break;
	}
}

void dcam_glb_reg_mwr(unsigned long addr, uint32_t mask,
		      uint32_t val, uint32_t reg_id)
{
	unsigned long flag;
	uint32_t tmp = 0;

	switch (reg_id) {
	case DCAM_CFG_REG:
		spin_lock_irqsave(&dcam_glb_reg_cfg_lock, flag);
		{
			tmp = REG_RD(addr);
			tmp &= ~(mask);
			REG_WR(addr, tmp | ((mask) & (val)));
		}
		spin_unlock_irqrestore(&dcam_glb_reg_cfg_lock, flag);
		break;
	case DCAM_CONTROL_REG:
		spin_lock_irqsave(&dcam_glb_reg_control_lock, flag);
		{
			tmp = REG_RD(addr);
			tmp &= ~(mask);
			REG_WR(addr, tmp | ((mask) & (val)));
		}
		spin_unlock_irqrestore(&dcam_glb_reg_control_lock, flag);
		break;
	case DCAM_INIT_MASK_REG:
		spin_lock_irqsave(&dcam_glb_reg_mask_lock, flag);
		{
			tmp = REG_RD(addr);
			tmp &= ~(mask);
			REG_WR(addr, tmp | ((mask) & (val)));
		}
		spin_unlock_irqrestore(&dcam_glb_reg_mask_lock, flag);
		break;
	case DCAM_INIT_CLR_REG:
		spin_lock_irqsave(&dcam_glb_reg_clr_lock, flag);
		{
			tmp = REG_RD(addr);
			tmp &= ~(mask);
			REG_WR(addr, tmp | ((mask) & (val)));
		}
		spin_unlock_irqrestore(&dcam_glb_reg_clr_lock, flag);
		break;
	case DCAM_AHBM_STS_REG:
		spin_lock_irqsave(&dcam_glb_reg_ahbm_sts_lock, flag);
		{
			tmp = REG_RD(addr);
			tmp &= ~(mask);
			REG_WR(addr, tmp | ((mask) & (val)));
		}
		spin_unlock_irqrestore(&dcam_glb_reg_ahbm_sts_lock, flag);
		break;
	case DCAM_ENDIAN_REG:
		spin_lock_irqsave(&dcam_glb_reg_endian_lock, flag);
		{
			tmp = REG_RD(addr);
			tmp &= ~(mask);
			REG_WR(addr, tmp | ((mask) & (val)));
		}
		spin_unlock_irqrestore(&dcam_glb_reg_endian_lock, flag);
		break;
	default:
		{
			tmp = REG_RD(addr);
			tmp &= ~(mask);
			REG_WR(addr, tmp | ((mask) & (val)));
		}
		break;
	}
}

static void _dcam_reg_trace(void)
{
	unsigned long                addr = 0;

	pr_info("Register list");
	for (addr = DCAM_CFG; addr <= DCAM_IP_REVISION; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			REG_RD(addr),
			REG_RD(addr + 4),
			REG_RD(addr + 8),
			REG_RD(addr + 12));
	}

}

static void dcam_mmu_reg_trace(void)
{
	unsigned long addr = 0;

	pr_info("DCAM:mmu Register list");
	for (addr = MMU_EN; addr <= MMU_REG_AU_MANAGE; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			REG_RD(addr),
			REG_RD(addr + 4),
			REG_RD(addr + 8),
			REG_RD(addr + 12));
	}
}

struct isp_reg_info {
	uint32_t base;
	uint32_t len;
};

static struct isp_reg_info g_isp_reg_info[] = {
	{0x0000, 0x60},
	{0x0100, 0x50},
	{0x0200, 0x40},
	{0x0300, 0x60},
	{0x0400, 0x30},
	{0x0500, 0x30},
	{0x0700, 0x70},
	{0x1000, 0x20},
	{0x1100, 0x20},
	{0x1200, 0x20},
	{0x1500, 0x50},
	{0x1600, 0x20},
	{0x1800, 0x30},
	{0x1900, 0x30},
	{0x1A00, 0x50},
	{0x1B00, 0x20},
	{0x3300, 0x20},
	{0x3700, 0x50},
	{0x6000, 0x20},
};

void print_isp_regs(void)
{
	unsigned long addr_base = 0;
	unsigned int i = 0, j = 0;

	pr_info("begin to dump isp regs:\n");
	if (ISP_BASE_ADDR == 0) {
		for (j = 0; j < ARRAY_SIZE(g_isp_reg_info); j++) {
			addr_base = (unsigned long)
				ioremap_nocache(0x60a00000 +
				g_isp_reg_info[j].base, g_isp_reg_info[j].len);
			for (i = 0; i < g_isp_reg_info[j].len/10; i++) {
				pr_info("0x%.3x: 0x%.8x 0x%.8x 0x%.8x 0x%.8x\n",
				0x60a00000 + g_isp_reg_info[j].base + 0x10*i,
				readl_relaxed((void *)
					(addr_base + 0x10*i + 0x0)),
				readl_relaxed((void *)
					(addr_base + 0x10*i + 0x4)),
				readl_relaxed((void *)
					(addr_base + 0x10*i + 0x8)),
				readl_relaxed((void *)
					(addr_base + 0x10*i + 0xc)));
			}
			iounmap((void __iomem *)addr_base);
		}
	} else {
		for (j = 0; j < ARRAY_SIZE(g_isp_reg_info); j++) {
			addr_base = g_isp_reg_info[j].base;
			for (i = 0; i < g_isp_reg_info[j].len/10; i++) {
				pr_info("0x%.8x: 0x%.8x 0x%.8x 0x%.8x 0x%.8x\n",
				(unsigned int)(0x60a00000 + addr_base + 0x10*i),
				ISP_REG_RD(addr_base + 0x10*i + 0x0),
				ISP_REG_RD(addr_base + 0x10*i + 0x4),
				ISP_REG_RD(addr_base + 0x10*i + 0x8),
				ISP_REG_RD(addr_base + 0x10*i + 0xc));
			}
		}
	}
}

static int dcam_enable_clk(void)
{
	int ret = DCAM_RTN_SUCCESS;

	pr_info("clk enable! %d", atomic_read(&s_dcam_users));
	if (atomic_read(&s_dcam_users) == 1) {
		ret = clk_set_parent(dcam0_clk, dcam0_clk_parent);
		if (ret) {
			pr_err("set clk parent fail\n");
			return ret;
		}
		ret = clk_prepare_enable(dcam0_clk);
		if (ret) {
			pr_err("enable clk fail\n");
			return ret;
		}

		ret = clk_set_parent(dcam0_axi_clk, dcam0_axi_clk_default);
		if (ret) {
			pr_err("set axi clk fail\n");
			return ret;
		}
		ret = clk_prepare_enable(dcam0_axi_clk);
		if (ret) {
			pr_err("enable axi clk fail\n");
			return ret;
		}

		ret = clk_prepare_enable(dcam0_eb);
		if (ret) {
			pr_err("enable dcam fail\n");
			return ret;
		}
		ret = clk_prepare_enable(dcam0_axi_eb);
		if (ret) {
			pr_err("enable dcam axi fail\n");
			return ret;
		}
	}

	return ret;
}

static int dcam_disable_clk(void)
{
	pr_info("clk disable! %d", atomic_read(&s_dcam_users));
	if (atomic_read(&s_dcam_users) == 0) {
		clk_disable_unprepare(dcam0_eb);
		clk_disable_unprepare(dcam0_axi_eb);

		clk_set_parent(dcam0_clk, dcam0_clk_default);
		clk_disable_unprepare(dcam0_clk);

		clk_set_parent(dcam0_axi_clk, dcam0_axi_clk_default);
		clk_disable_unprepare(dcam0_axi_clk);

	}
	return 0;
}

static void _dcam_buf_queue_init(struct dcam_buf_queue *queue)
{
	if (DCAM_ADDR_INVALID(queue)) {
		pr_err("invalid heap %p.\n", queue);
		return;
	}

	memset((void *)queue, 0, sizeof(struct dcam_buf_queue));
	queue->write = &queue->frame[0];
	queue->read  = &queue->frame[0];
	spin_lock_init(&queue->lock);
}

static int32_t _dcam_buf_queue_write(struct dcam_buf_queue *queue,
				    struct camera_frame *frame)
{
	int                      ret = DCAM_RTN_SUCCESS;
	struct camera_frame      *ori_frame;
	unsigned long            flag;

	if (DCAM_ADDR_INVALID(queue) || DCAM_ADDR_INVALID(frame)) {
		pr_err("enq, invalid param.\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&queue->lock, flag);
	ori_frame = queue->write;
	DCAM_TRACE("_dcam_buf_queue_write.\n");
	*queue->write++ = *frame;
	if (queue->write > &queue->frame[DCAM_FRM_CNT_MAX-1])
		queue->write = &queue->frame[0];

	if (queue->write == queue->read) {
		queue->write = ori_frame;
		pr_info("warning, queue is full, cannot write 0x%x.\n",
			frame->yaddr);
	}
	spin_unlock_irqrestore(&queue->lock, flag);
	DCAM_TRACE("_dcam_buf_queue_write type %d.\n", frame->type);
	return ret;
}

static int32_t _dcam_buf_queue_read(struct dcam_buf_queue *queue,
				   struct camera_frame *frame)
{
	int                      ret = DCAM_RTN_SUCCESS;
	unsigned long            flag;

	if (DCAM_ADDR_INVALID(queue) || DCAM_ADDR_INVALID(frame)) {
		pr_err("deq, invalid param %p, %p.\n",
			queue,
			frame);
		return -EINVAL;
	}

	DCAM_TRACE("_dcam_buf_queue_read.\n");

	spin_lock_irqsave(&queue->lock, flag);
	if (queue->read != queue->write) {
		*frame = *queue->read++;
		if (queue->read > &queue->frame[DCAM_FRM_CNT_MAX-1])
			queue->read = &queue->frame[0];
	} else {
		ret = EAGAIN;
	}
	spin_unlock_irqrestore(&queue->lock, flag);
	DCAM_TRACE("_dcam_buf_queue_read type %d.\n", frame->type);
	return ret;
}

static void _dcam_frm_queue_clear(struct dcam_frm_queue *queue)
{
	if (DCAM_ADDR_INVALID(queue)) {
		pr_err("invalid heap %p.\n", queue);
		return;
	}

	memset((void *)queue, 0, sizeof(struct dcam_frm_queue));
}

static int32_t _dcam_frame_enqueue(struct dcam_frm_queue *queue,
				  struct camera_frame *frame)
{
	if (DCAM_ADDR_INVALID(queue) || DCAM_ADDR_INVALID(frame)) {
		pr_err("enq, invalid param %p, %p.\n",
			queue,
			frame);
		return -1;
	}
	if (queue->valid_cnt >= DCAM_FRM_QUEUE_LENGTH) {
		pr_err("q over flow.\n");
		return -1;
	}
	/*queue->frm_array[queue->valid_cnt] = frame;*/
	memcpy(&queue->frm_array[queue->valid_cnt], frame,
			sizeof(struct camera_frame));
	queue->valid_cnt++;
	DCAM_TRACE("en queue, %d, %d, 0x%x, 0x%x.\n",
		(0xF & frame->fid),
		queue->valid_cnt,
		frame->yaddr,
		frame->uaddr);
	return 0;
}

static int32_t _dcam_frame_dequeue(struct dcam_frm_queue *queue,
				  struct camera_frame *frame)
{
	uint32_t i = 0;

	if (DCAM_ADDR_INVALID(queue) || DCAM_ADDR_INVALID(frame)) {
		pr_err("deq, invalid param %p, %p.\n",
			queue,
			frame);
		return -1;
	}
	if (queue->valid_cnt == 0) {
		pr_err("q under flow.\n");
		return -1;
	}

	memcpy(frame, &queue->frm_array[0], sizeof(struct camera_frame));
	queue->valid_cnt--;
	for (i = 0; i < queue->valid_cnt; i++) {
		memcpy(&queue->frm_array[i], &queue->frm_array[i+1],
					     sizeof(struct camera_frame));
	}
	DCAM_TRACE("de queue, %d, %d.\n",
		(0xF & (frame)->fid),
		queue->valid_cnt);
	return 0;
}

static void _dcam_frm_clear(enum dcam_path_index path_index)
{
	struct camera_frame frame, *res_frame;
	struct dcam_path_desc *path;
	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	if (DCAM_PATH_IDX_0 & path_index) {
		path = &s_p_dcam_mod->dcam_path0;
		while (path->frame_queue.valid_cnt > 0) {
			if (_dcam_frame_dequeue(&path->frame_queue, &frame))
				break;
			pfiommu_free_addr(&frame.pfinfo);
		}

		_dcam_frm_queue_clear(&path->frame_queue);
		_dcam_buf_queue_init(&path->buf_queue);
		res_frame = &s_p_dcam_mod->path_reserved_frame[DCAM_PATH0];
		pfiommu_free_addr(&res_frame->pfinfo);
		memset((void *)res_frame, 0, sizeof(struct camera_frame));
	}

	if (DCAM_PATH_IDX_1 & path_index) {
		path = &s_p_dcam_mod->dcam_path1;
		while (path->frame_queue.valid_cnt > 0) {
			if (_dcam_frame_dequeue(&path->frame_queue, &frame))
				break;
			pfiommu_free_addr(&frame.pfinfo);
		}

		_dcam_frm_queue_clear(&path->frame_queue);
		_dcam_buf_queue_init(&path->buf_queue);
		res_frame = &s_p_dcam_mod->path_reserved_frame[DCAM_PATH1];
		pfiommu_free_addr(&res_frame->pfinfo);
		memset((void *)res_frame, 0, sizeof(struct camera_frame));
	}

	if (DCAM_PATH_IDX_2 & path_index) {
		path = &s_p_dcam_mod->dcam_path2;
		while (path->frame_queue.valid_cnt > 0) {
			if (_dcam_frame_dequeue(&path->frame_queue, &frame))
				break;
			pfiommu_free_addr(&frame.pfinfo);
		}

		_dcam_frm_queue_clear(&path->frame_queue);
		_dcam_buf_queue_init(&path->buf_queue);
		res_frame = &s_p_dcam_mod->path_reserved_frame[DCAM_PATH2];
		pfiommu_free_addr(&res_frame->pfinfo);
		memset((void *)res_frame, 0, sizeof(struct camera_frame));
	}
}

static int32_t _dcam_path_set_next_frm(enum dcam_path_index path_index,
				      uint32_t is_1st_frm)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct camera_frame     frame;
	struct camera_frame     *reserved_frame = NULL;
	struct dcam_path_desc   *path = NULL;
	unsigned long           yuv_reg[3] = {0};
	unsigned int            yuv_addr[3] = {0};
	uint32_t                path_max_frm_cnt;
	struct dcam_frm_queue   *p_heap = NULL;
	struct dcam_buf_queue   *p_buf_queue = NULL;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return -rtn;
	}

	if (path_index == DCAM_PATH_IDX_0) {
		reserved_frame = &s_p_dcam_mod->path_reserved_frame[DCAM_PATH0];
		path = &s_p_dcam_mod->dcam_path0;
		yuv_reg[0] = DCAM_FRM_ADDR0;
		yuv_reg[1] = DCAM_FRM_ADDR12;
		yuv_reg[2] = DCAM_FRM_ADDR13;
		path_max_frm_cnt = DCAM_PATH_0_FRM_CNT_MAX;
		p_heap = &s_p_dcam_mod->dcam_path0.frame_queue;
		p_buf_queue = &s_p_dcam_mod->dcam_path0.buf_queue;
	} else if (path_index == DCAM_PATH_IDX_1) {
		reserved_frame = &s_p_dcam_mod->path_reserved_frame[DCAM_PATH1];
		path = &s_p_dcam_mod->dcam_path1;
		yuv_reg[0] = DCAM_FRM_ADDR1;
		yuv_reg[1] = DCAM_FRM_ADDR2;
		yuv_reg[2] = DCAM_FRM_ADDR3;
		path_max_frm_cnt = DCAM_PATH_1_FRM_CNT_MAX;
		p_heap = &s_p_dcam_mod->dcam_path1.frame_queue;
		p_buf_queue = &s_p_dcam_mod->dcam_path1.buf_queue;
	} else if (path_index == DCAM_PATH_IDX_2) {
		reserved_frame = &s_p_dcam_mod->path_reserved_frame[DCAM_PATH2];
		path = &s_p_dcam_mod->dcam_path2;
		yuv_reg[0] = DCAM_FRM_ADDR4;
		yuv_reg[1] = DCAM_FRM_ADDR5;
		yuv_reg[2] = DCAM_FRM_ADDR6;
		path_max_frm_cnt = DCAM_PATH_2_FRM_CNT_MAX;
		p_heap = &s_p_dcam_mod->dcam_path2.frame_queue;
		p_buf_queue = &s_p_dcam_mod->dcam_path2.buf_queue;
	} else {
		pr_err("path_index 0x%x error\n", path_index);
		return -1;
	}

	/* iommu get addr */
	memset(&frame, 0, sizeof(struct camera_frame));
	if (is_1st_frm == false &&
	    _dcam_buf_queue_read(p_buf_queue, &frame) == 0 &&
		(frame.pfinfo.mfd[0] != 0)) {
		path->output_frame_count--;
	} else {
		DCAM_TRACE("No freed frame path_index %d cnt %d\n",
			path_index, path->output_frame_count);
		if (reserved_frame->pfinfo.mfd[0] == 0) {
			pr_err("path idx 0x%x, mfd 0\n", path_index);
			return -1;
		}
		memcpy(&frame, reserved_frame, sizeof(struct camera_frame));
	}

	DCAM_TRACE("y 0x%x mfd 0x%x path_index %d\n",
		frame.yaddr, frame.pfinfo.mfd[0], path_index);

	if (pfiommu_check_addr(&frame.pfinfo)) {
		pr_err("the frame has been broken!\n");
		return -1;
	}

	if (frame.pfinfo.dev == NULL) {
		pr_err("get frame address failed: %p\n", frame.pfinfo.dev);
		return -1;
	}

	if (frame.pfinfo.mfd[0] == reserved_frame->pfinfo.mfd[0]) {
		DCAM_TRACE("path 0x%x, fd 0x%x, {0x%x,0x%x,0x%x}\n",
			path_index, frame.pfinfo.mfd[0], frame.pfinfo.size[0],
			frame.pfinfo.size[1],frame.pfinfo.size[2]);
		if (pfiommu_get_single_page_addr(&frame.pfinfo)) {
			pr_err("get a page failed, fd 0x%x, size {0x%x,0x%x,%d}\n",
				frame.pfinfo.mfd[0], frame.pfinfo.size[0],
				frame.pfinfo.size[1],frame.pfinfo.size[2]);
			return -1;
		}
	} else {
		if (pfiommu_get_addr(&frame.pfinfo)) {
			pr_err("get frame address failed!\n");
			return -1;
		}
	}

	if (frame.pfinfo.iova[0] == 0) {
		pr_err("invalid iova: 0!\n");
		return -1;
	}

/* TODO depond on HAL*/
	yuv_addr[0] = frame.pfinfo.iova[0] + frame.yaddr;
	yuv_addr[1] = frame.pfinfo.iova[0] + frame.uaddr;
	yuv_addr[2] = frame.pfinfo.iova[2] + frame.vaddr;

	if (yuv_addr[0] == 0x0 || yuv_addr[1] == 0x0) {
		pr_err("error addr y 0x%x uv 0x%x.\n",
						yuv_addr[0], yuv_addr[1]);
		rtn = -DCAM_RTN_PATH_NO_MEM;
		dump_stack();
		panic("YUV addr error!");
	} else {
		REG_WR(yuv_reg[0], yuv_addr[0]);
		/*Pike2 path0/path1/path2 support yuv420/422/422-3plane*/
		if (path->output_format < DCAM_YUV400) {
			REG_WR(yuv_reg[1], yuv_addr[1]);
			if (path->output_format == DCAM_YUV420_3FRAME)
				REG_WR(yuv_reg[2], yuv_addr[2]);

		}
		dcam_frame_lock(&frame);
		if (_dcam_frame_enqueue(p_heap, &frame) == 0) {
			DCAM_TRACE("success to enq frame buf.\n");
		} else {
			dcam_frame_unlock(&frame);
			rtn = -DCAM_RTN_PATH_FRAME_LOCKED;
		}
	}

	return -rtn;
}

static void _dcam_force_copy_ext(enum dcam_path_index path_index,
				uint32_t path_copy, uint32_t coef_copy)
{
	uint32_t         reg_val = 0;

	if (path_index == DCAM_PATH_IDX_1) {
		if (path_copy)
			reg_val |= BIT_10;
		if (coef_copy)
			reg_val |= BIT_14;

		dcam_glb_reg_mwr(DCAM_CONTROL, reg_val, reg_val,
						DCAM_CONTROL_REG);
	} else if (path_index == DCAM_PATH_IDX_2) {
		if (path_copy)
			reg_val |= BIT_12;
		if (coef_copy)
			reg_val |= BIT_16;

		dcam_glb_reg_mwr(DCAM_CONTROL, reg_val, reg_val,
						DCAM_CONTROL_REG);
	} else {
		DCAM_TRACE("_dcam_force_copy_ext invalid path index: %d.\n",
								path_index);
	}
}

static void _dcam_auto_copy_ext(enum dcam_path_index path_index,
				uint32_t path_copy, uint32_t coef_copy)
{
	uint32_t         reg_val = 0;

	if (path_index == DCAM_PATH_IDX_0) {
		if (path_copy)
			reg_val |= BIT_9;
		dcam_glb_reg_mwr(DCAM_CONTROL, reg_val, reg_val,
						DCAM_CONTROL_REG);
	} else if (path_index == DCAM_PATH_IDX_1) {
		if (path_copy)
			reg_val |= BIT_11;
		if (coef_copy)
			reg_val |= BIT_15;
		dcam_glb_reg_mwr(DCAM_CONTROL, reg_val, reg_val,
						DCAM_CONTROL_REG);
	} else if (path_index == DCAM_PATH_IDX_2) {
		if (path_copy)
			reg_val |= BIT_13;
		if (coef_copy)
			reg_val |= BIT_17;
		dcam_glb_reg_mwr(DCAM_CONTROL, reg_val, reg_val,
						DCAM_CONTROL_REG);
	} else {
		DCAM_TRACE("_dcam_auto_copy_ext invalid path index:%d.\n",
				path_index);
	}

}

static void _dcam_force_copy(enum dcam_path_index path_index)
{
	if (path_index == DCAM_PATH_IDX_0) {
		dcam_glb_reg_mwr(DCAM_CONTROL, BIT_8, 1 << 8,
							DCAM_CONTROL_REG);
	} else if (path_index == DCAM_PATH_IDX_1) {
		dcam_glb_reg_mwr(DCAM_CONTROL, BIT_10, 1 << 10,
							DCAM_CONTROL_REG);
	} else if (path_index == DCAM_PATH_IDX_2) {
		dcam_glb_reg_mwr(DCAM_CONTROL, BIT_12, 1 << 12,
							DCAM_CONTROL_REG);
	} else {
		DCAM_TRACE("_dcam_force_copy invalid path index: %d.\n",
								path_index);
	}
}

static void _dcam_auto_copy(enum dcam_path_index path_index)
{
	if (path_index == DCAM_PATH_IDX_0) {
		dcam_glb_reg_mwr(DCAM_CONTROL, BIT_9, 1 << 9,
							DCAM_CONTROL_REG);
	} else if (path_index == DCAM_PATH_IDX_1) {
		dcam_glb_reg_mwr(DCAM_CONTROL, BIT_11, 1 << 11,
							DCAM_CONTROL_REG);
	} else if (path_index == DCAM_PATH_IDX_2) {
		dcam_glb_reg_mwr(DCAM_CONTROL, BIT_13, 1 << 13,
							DCAM_CONTROL_REG);
	} else {
		DCAM_TRACE("_dcam_auto_copy invalid path index: %d.\n",
								path_index);
	}
}

static void _dcam_path_done_notice(enum dcam_path_index path_index)
{
	struct dcam_path_desc   *p_path = NULL;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	if (path_index == DCAM_PATH_IDX_0) {
		p_path = &s_p_dcam_mod->dcam_path0;
	} else if (path_index == DCAM_PATH_IDX_1) {
		p_path = &s_p_dcam_mod->dcam_path1;
	} else if (path_index == DCAM_PATH_IDX_2) {
		p_path = &s_p_dcam_mod->dcam_path2;
	} else {
		pr_info("Wrong index 0x%x.\n", path_index);
		return;
	}

	if (p_path->wait_for_done) {
		complete(&p_path->tx_done_com);
		p_path->wait_for_done = 0;
	}
}

static void _dcam_path_shrink_set(void *input_info,
				 enum dcam_path_index path_index)
{
	struct dcam_regular_desc *regular_desc =
			(struct dcam_regular_desc *)input_info;
	enum dcam_regular_mode mode;
	union dcam_regular_value *v;
	uint32_t reg_val = 0;
	uint32_t addr    = 0;

	if (DCAM_PATH_IDX_1 & path_index)
		addr = DCAM_PATH1_CFG;
	else if (DCAM_PATH_IDX_2 & path_index)
		addr = DCAM_PATH2_CFG;

	/* current shrink range, y: (16, 235) uv: (16, 240) */
	if (regular_desc->regular_mode == DCAM_REGULAR_SHRINK) {
		reg_val = SHRINK_Y_UP_TH << 0 |
			SHRINK_Y_DN_TH << 8 |
			SHRINK_UV_UP_TH << 16 |
			SHRINK_UV_DN_TH << 24;
		REG_WR(YUV_SHRINK_CFG, reg_val);
		reg_val = (SHRINK_Y_OFFSET & 0x1f) << 0 |
			(SHRINK_Y_RANGE & 0xf) << 8 |
			(SHRINK_C_OFFSET & 0x1f) << 16 |
			(SHRINK_C_RANGE & 0xf) << 24;
		REG_WR(YUV_REGULAR_CFG, reg_val);
	} else if (regular_desc->regular_mode == DCAM_REGULAR_CUT) {
		v = &regular_desc->regular_value;
		reg_val =
			v->shrink_val.y_up_threshold << 0 |
			v->shrink_val.y_dn_threshold << 8 |
			v->shrink_val.uv_up_threshold << 16 |
			v->shrink_val.uv_dn_threshold << 24;
		REG_WR(YUV_SHRINK_CFG, reg_val);
	} else if (regular_desc->regular_mode == DCAM_REGULAR_EFFECT) {
		v = &regular_desc->regular_value;
		reg_val =
			v->effect_val.y_special_threshold << 0 |
			v->effect_val.u_special_threshold << 8 |
			v->effect_val.v_special_threshold << 16;
		REG_WR(YUV_EFFECT_CFG, reg_val);
	}

	mode = regular_desc->regular_mode;
	dcam_glb_reg_mwr(addr, BIT_25 | BIT_26, mode << 25, DCAM_REG_MAX);

	DCAM_TRACE("path %d: shrink, %d.\n", ffs(path_index) - 1,
		   regular_desc->regular_mode);

}

static unsigned int pike2_get_path0_set_value(enum dcam_fmt fmt)
{
	switch (fmt) {
	case DCAM_YUV422:
		return 0x0;
	case DCAM_YUV420:
		return 0x1;
	case DCAM_RAWRGB:
		return 0x2;
	default:
		return 0x0;
	}
	return 0x0;
}

static void _dcam_path0_set(void)
{
	uint32_t                reg_val = 0;
	struct dcam_path_desc   *path = NULL;
	uint32_t  format = 0;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}
	path = &s_p_dcam_mod->dcam_path0;
	if (path->valid_param.input_size) {
		reg_val = path->input_size.w | (path->input_size.h << 16);
		REG_WR(DCAM_PATH0_SRC_SIZE, reg_val);
		DCAM_TRACE("path0 set: src {%d %d}.\n",
			path->input_size.w, path->input_size.h);
	}

	if (path->valid_param.input_rect) {
		reg_val = path->input_rect.x | (path->input_rect.y << 16);
		REG_WR(DCAM_PATH0_TRIM_START, reg_val);
		reg_val = path->input_rect.w | (path->input_rect.h << 16);
		REG_WR(DCAM_PATH0_TRIM_SIZE, reg_val);
		DCAM_TRACE("path0 set: rect {%d %d %d %d}.\n",
			path->input_rect.x,
			path->input_rect.y,
			path->input_rect.w,
			path->input_rect.h);
	}

	if (path->valid_param.src_sel) {
		REG_MWR(DCAM_CFG, BIT_10, path->src_sel << 10);
		DCAM_TRACE("path 0: src_sel=0x%x.\n", path->src_sel);
	}

	if (path->valid_param.frame_deci) {
		REG_MWR(DCAM_PATH0_CFG, BIT_23 | BIT_24,
				path->frame_deci << 23);
	}

	if (path->valid_param.output_format) {
		format = path->output_format;
		if (format == DCAM_RAWRGB)
			REG_MWR(DCAM_CFG, BIT_3, 1 << 3);
		else
			REG_MWR(DCAM_CFG, BIT_3, 0 << 3);
		format = pike2_get_path0_set_value(format);
		REG_MWR(DCAM_PATH0_CFG, BIT_6 | BIT_7, format << 6);
	}

	if (path->valid_param.data_endian) {
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_5 | BIT_4,
			path->data_endian.y_endian << 4, DCAM_ENDIAN_REG);
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_20 | BIT_21,
			path->data_endian.uv_endian << 20, DCAM_ENDIAN_REG);

		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_18, BIT_18,
				 DCAM_ENDIAN_REG); /* axi write*/
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_19, BIT_19,
				 DCAM_ENDIAN_REG); /* axi read*/
		DCAM_TRACE("path 0: data_endian y=0x%x, uv=0x%x.\n",
			path->data_endian.y_endian,
			path->data_endian.uv_endian);
	}

	if (path->valid_param.pdaf_ctrl) {
		REG_MWR(DCAM_CFG, BIT_4, 1 << 4);
		REG_MWR(IMAGE_CONTROL, BIT_1 | BIT_0,
				path->pdaf_ctrl.mode << 0);
		REG_MWR(IMAGE_CONTROL, BIT_16 | BIT_17,
				path->pdaf_ctrl.image_vc << 16);
		REG_MWR(IMAGE_CONTROL, 0x3F00,
				path->pdaf_ctrl.image_dt << 8);
		DCAM_TRACE("path 0: PDAF mode:%d, vc:0x%x, dt:0x%x.\n",
				path->pdaf_ctrl.mode,
				path->pdaf_ctrl.image_vc,
				path->pdaf_ctrl.image_dt);
	}

	if (path->valid_param.rot_mode) {
		path->valid_param.rot_mode = 0;
		REG_MWR(DCAM_PATH0_CFG, BIT_10 | BIT_9, path->rot_mode << 9);
		DCAM_TRACE("dcam_path0_set 1 rot_mod :%d reg:%x.\n",
				path->rot_mode, REG_RD(DCAM_PATH0_CFG));
	}
}
static void _dcam_path1_set(struct dcam_path_desc *path)
{
	uint32_t                reg_val = 0;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}
	if (DCAM_ADDR_INVALID(path)) {
		pr_err("zero pointer\n");
		return;
	}

	if (path->valid_param.input_size) {
		reg_val = path->input_size.w | (path->input_size.h << 16);
		REG_WR(DCAM_PATH1_SRC_SIZE, reg_val);
		DCAM_TRACE("path1 set: src {%d %d}.\n",
			path->input_size.w, path->input_size.h);
	}

	if (path->valid_param.input_rect) {
		reg_val = path->input_rect.x | (path->input_rect.y << 16);
		REG_WR(DCAM_PATH1_TRIM_START, reg_val);
		reg_val = path->input_rect.w | (path->input_rect.h << 16);
		REG_WR(DCAM_PATH1_TRIM_SIZE, reg_val);
		DCAM_TRACE("path1 set: rect {%d %d %d %d}.\n",
			path->input_rect.x,
			path->input_rect.y,
			path->input_rect.w,
			path->input_rect.h);
	}

	if (path->valid_param.output_size) {
		reg_val = path->output_size.w | (path->output_size.h << 16);
		REG_WR(DCAM_PATH1_DST_SIZE, reg_val);
		DCAM_TRACE("path1 set: dst {%d %d}.\n",
			path->output_size.w, path->output_size.h);
	}

	if (path->valid_param.output_format) {
		enum dcam_fmt format = path->output_format;

		if (format == DCAM_YUV422)
			REG_MWR(DCAM_PATH1_CFG, BIT_7 | BIT_6, 0 << 6);
		else if (format == DCAM_YUV420)
			REG_MWR(DCAM_PATH1_CFG, BIT_7 | BIT_6, 1 << 6);
		else if (format == DCAM_YUV420_3FRAME)
			REG_MWR(DCAM_PATH1_CFG, BIT_7 | BIT_6, 3 << 6);
		else
			DCAM_TRACE("invalid path1 outputformat %d.\n", format);

		DCAM_TRACE("path 1: output_format=0x%x.\n", format);
	}

	if (path->valid_param.src_sel) {
		REG_MWR(DCAM_CFG,  BIT_11, path->src_sel << 11);
		DCAM_TRACE("path 1: src_sel=0x%x.\n", path->src_sel);
	}

	if (path->valid_param.data_endian) {
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_7 | BIT_6,
			    path->data_endian.y_endian << 6, DCAM_ENDIAN_REG);
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_9 | BIT_8,
			    path->data_endian.uv_endian << 8, DCAM_ENDIAN_REG);
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_18, BIT_18,
			    DCAM_ENDIAN_REG); /* axi write*/
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_19, BIT_19,
			    DCAM_ENDIAN_REG); /* axi read */
		DCAM_TRACE("path 1: data_endian y=0x%x, uv=0x%x.\n",
		       path->data_endian.y_endian, path->data_endian.uv_endian);
	}

	if (path->valid_param.frame_deci) {
		REG_MWR(DCAM_PATH1_CFG, BIT_24 | BIT_23,
				path->frame_deci << 23);
		DCAM_TRACE("path 1: frame_deci=0x%x.\n",
				path->frame_deci);
	}

	if (path->valid_param.scale_tap) {
		path->valid_param.scale_tap = 0;
		REG_MWR(DCAM_PATH1_CFG, BIT_19 | BIT_18 | BIT_17 | BIT_16,
				(path->scale_tap.y_tap & 0x0F) << 16);
		REG_MWR(DCAM_PATH1_CFG, BIT_15 | BIT_14 | BIT_13 |
			BIT_12 | BIT_11, (path->scale_tap.uv_tap & 0x1F) << 11);
		DCAM_TRACE("path 1: scale_tap, y=0x%x, uv=0x%x.\n",
			path->scale_tap.y_tap, path->scale_tap.uv_tap);
	}

	if (path->valid_param.v_deci) {
		path->valid_param.v_deci = 0;
		REG_MWR(DCAM_PATH1_CFG, BIT_2, path->deci_val.deci_x_en << 2);
		REG_MWR(DCAM_PATH1_CFG, BIT_1 | BIT_0, path->deci_val.deci_x);

		REG_MWR(DCAM_PATH1_CFG, BIT_5, path->deci_val.deci_y_en << 5);
		REG_MWR(DCAM_PATH1_CFG, BIT_4 | BIT_3,
					path->deci_val.deci_y << 3);
		DCAM_TRACE("path1: deci, x_en=%d, x=%d, y_en=%d, y=%d.\n",
			path->deci_val.deci_x_en, path->deci_val.deci_x,
			path->deci_val.deci_y_en, path->deci_val.deci_y);
	}

	if (path->valid_param.rot_mode) {
		path->valid_param.rot_mode = 0;
		REG_MWR(DCAM_PATH1_CFG, BIT_10 | BIT_9, path->rot_mode << 9);
		DCAM_TRACE("dcam_path1_set rot_mod :%d reg:%x.\n",
				path->rot_mode, REG_RD(DCAM_PATH1_CFG));
	}

	if (path->valid_param.shrink) {
		path->valid_param.shrink = 0;
		_dcam_path_shrink_set(&path->regular_desc, DCAM_PATH_IDX_1);
	}
}

static void _dcam_path2_set(void)
{
	struct dcam_path_desc   *path = NULL;
	uint32_t                reg_val = 0;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}
	path = &s_p_dcam_mod->dcam_path2;
	if (path->valid_param.input_size) {
		reg_val = path->input_size.w | (path->input_size.h << 16);
		REG_WR(DCAM_PATH2_SRC_SIZE, reg_val);
		DCAM_TRACE("path2 set: src {%d %d}.\n",
			path->input_size.w, path->input_size.h);
	}

	if (path->valid_param.input_rect) {
		reg_val = path->input_rect.x | (path->input_rect.y << 16);
		REG_WR(DCAM_PATH2_TRIM_START, reg_val);
		reg_val = path->input_rect.w | (path->input_rect.h << 16);
		REG_WR(DCAM_PATH2_TRIM_SIZE, reg_val);
		DCAM_TRACE("path2 set: rect {%d %d %d %d}.\n",
			path->input_rect.x,
			path->input_rect.y,
			path->input_rect.w,
			path->input_rect.h);
	}

	if (path->valid_param.output_size) {
		reg_val = path->output_size.w | (path->output_size.h << 16);
		REG_WR(DCAM_PATH2_DST_SIZE, reg_val);
		DCAM_TRACE("path2 set: dst {%d %d}.\n",
			    path->output_size.w, path->output_size.h);
	}

	if (path->valid_param.output_format) {
		enum dcam_fmt format = path->output_format;

		if (format == DCAM_YUV422)
			REG_MWR(DCAM_PATH2_CFG, BIT_7 | BIT_6, 0 << 6);
		else if (format == DCAM_YUV420)
			REG_MWR(DCAM_PATH2_CFG, BIT_7 | BIT_6, 1 << 6);
		else if (format == DCAM_YUV420_3FRAME)
			REG_MWR(DCAM_PATH2_CFG, BIT_7 | BIT_6, 3 << 6);
		else
			DCAM_TRACE("invalid path2 output format %d.\n", format);


		DCAM_TRACE("path 2: output_format=0x%x.\n", format);
	}

	if (path->valid_param.src_sel)
		REG_MWR(DCAM_CFG, BIT_14 | BIT_13, path->src_sel << 13);

	if (path->valid_param.data_endian) {
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_11 | BIT_10,
			    path->data_endian.y_endian << 10, DCAM_ENDIAN_REG);
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_13 | BIT_12,
			    path->data_endian.uv_endian << 12, DCAM_ENDIAN_REG);
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_18, BIT_18,
			    DCAM_ENDIAN_REG); /*axi write*/
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_19, BIT_19,
			    DCAM_ENDIAN_REG); /*axi read*/

		DCAM_TRACE("path 2: data_endian y=0x%x, uv=0x%x.\n",
			path->data_endian.y_endian,
			path->data_endian.uv_endian);
	}

	if (path->valid_param.frame_deci) {
		REG_MWR(DCAM_PATH2_CFG, BIT_24 | BIT_23,
				path->frame_deci << 23);

		DCAM_TRACE("path 2: frame_deci=0x%x.\n", path->frame_deci);
	}

	if (path->valid_param.scale_tap) {
		path->valid_param.scale_tap = 0;
		REG_MWR(DCAM_PATH2_CFG, BIT_19 | BIT_18 | BIT_17 | BIT_16,
			(path->scale_tap.y_tap & 0x0F) << 16);
		REG_MWR(DCAM_PATH2_CFG, BIT_15 | BIT_14 | BIT_13 |
			BIT_12 | BIT_11, (path->scale_tap.uv_tap & 0x1F) << 11);

		DCAM_TRACE("path 2: scale_tap, y=0x%x, uv=0x%x.\n",
			path->scale_tap.y_tap, path->scale_tap.uv_tap);
	}

	if (path->valid_param.v_deci) {
		path->valid_param.v_deci = 0;
		REG_MWR(DCAM_PATH2_CFG, BIT_2, path->deci_val.deci_x_en << 2);
		REG_MWR(DCAM_PATH2_CFG, BIT_1 | BIT_0, path->deci_val.deci_x);
		REG_MWR(DCAM_PATH2_CFG, BIT_5, path->deci_val.deci_y_en << 5);
		REG_MWR(DCAM_PATH2_CFG, BIT_4 | BIT_3,
				    path->deci_val.deci_y << 3);

		DCAM_TRACE("path 2: deci, x_en=%d, x=%d, y_en=%d, y=%d.\n",
			path->deci_val.deci_x_en, path->deci_val.deci_x,
			path->deci_val.deci_y_en, path->deci_val.deci_y);
	}

	if (path->valid_param.rot_mode) {
		path->valid_param.rot_mode = 0;
		REG_MWR(DCAM_PATH2_CFG, BIT_10 | BIT_9, path->rot_mode << 9);
		DCAM_TRACE("dcam_path2_set  rot_mod :%d reg:%x.\n",
				path->rot_mode, REG_RD(DCAM_PATH2_CFG));
	}

	if (path->valid_param.shrink) {
		path->valid_param.shrink = 0;
		_dcam_path_shrink_set(&path->regular_desc, DCAM_PATH_IDX_2);
	}
}

void dcam_scale_coeff_free(void)
{
	if (s_dcam_sc_array) {
		vfree(s_dcam_sc_array);
		s_dcam_sc_array = NULL;
	}
}

static uint32_t *dcam_get_scale_coeff_addr(uint32_t *index)
{
	uint32_t i;

	if (DCAM_ADDR_INVALID(s_dcam_sc_array)) {
		pr_info("scale addr, invalid param %p.\n", s_dcam_sc_array);
		return NULL;
	}

	for (i = 0; i < DCAM_SC_COEFF_BUF_COUNT; i++) {
		if (s_dcam_sc_array->scaling_coeff[i].flag == 0) {
			*index = i;
			DCAM_TRACE("dcam: get buf index %d.\n", i);
			return s_dcam_sc_array->scaling_coeff[i].buf;
		}
	}
	pr_info("dcam: get buf index %d.\n", i);

	return NULL;
}

static void    _dcam_wait_for_channel_stop(enum dcam_path_index path_index)
{
	int                     time_out = 5000;
	uint32_t                ret = 0;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	/* wait for AHB path busy cleared */
	while (time_out) {
		if (s_p_dcam_mod->dcam_path1.valid &&
		    (DCAM_PATH_IDX_1 & path_index)) {
			ret = REG_RD(DCAM_AHBM_STS) & BIT_17;
		} else if (s_p_dcam_mod->dcam_path2.valid &&
		    (DCAM_PATH_IDX_2 & path_index)) {
			ret = REG_RD(DCAM_AHBM_STS) & BIT_18;
		} else if (s_p_dcam_mod->dcam_path0.valid &&
		    (DCAM_PATH_IDX_0 & path_index)) {
			ret = REG_RD(DCAM_AHBM_STS) & BIT_19;
		}
		if (!ret)
			break;
		time_out--;
	}

	if (time_out <= 0)
		pr_err("wait channel stop %d %d.\n", ret, time_out);
}

static void    _dcam_quickstop_set(enum dcam_path_index path_index,
	uint32_t path_rst, uint32_t cfg_bit, uint32_t ahbm_bit)
{
	/* stop path0/path1/path2 */
	dcam_glb_reg_owr(DCAM_AHBM_STS, ahbm_bit, DCAM_AHBM_STS_REG);
	/* wait until path0/path1/path2 stopped */
	_dcam_wait_for_channel_stop(path_index);
	/* clear the path0/path1/path2 eb */
	dcam_glb_reg_mwr(DCAM_CFG, cfg_bit, ~cfg_bit, DCAM_CFG_REG);
	/* path0/path1/path2 force copy */
	_dcam_force_copy(path_index);
	/* reset path0/path1/path2 */
	dcam_reset(path_rst);
	/* disable path0/path1/path2 stop */
	dcam_glb_reg_awr(DCAM_AHBM_STS, ~ahbm_bit, DCAM_AHBM_STS_REG);
}

static void _dcam_quickstop_set_all(void)
{
	unsigned int time_out = 0;

	/* stop cap_eb first */
	dcam_glb_reg_awr(DCAM_CONTROL, ~(BIT_2),
			DCAM_CONTROL_REG);
	/* stop (path0 + path1 + path2) */
	dcam_glb_reg_owr(DCAM_AHBM_STS, BIT_3 | BIT_4 | BIT_5,
			DCAM_AHBM_STS_REG);
	/* wait until (path0 + path1 + path2) stopped */
	_dcam_wait_for_channel_stop(DCAM_PATH_IDX_0);
	_dcam_wait_for_channel_stop(DCAM_PATH_IDX_1);
	_dcam_wait_for_channel_stop(DCAM_PATH_IDX_2);
	/* stop axi (write +read) */
	dcam_glb_reg_owr(DCAM_AHBM_STS, BIT_6 | BIT_7,
			DCAM_AHBM_STS_REG);
	/* clear the (path0 + path1 + path2) eb */
	dcam_glb_reg_awr(DCAM_CFG, ~(BIT_0 | BIT_1 | BIT_2), DCAM_CFG_REG);
	/* (path0 + path1 + path2) force copy */
	dcam_glb_reg_owr(DCAM_CONTROL, BIT_8 | BIT_10 | BIT_12,
			DCAM_CONTROL_REG);

	/* wait until axi stopped */
	while (++time_out < DCAM_AXI_STOP_TIMEOUT) {
		if ((REG_RD(DCAM_AHBM_STS) & BIT_0) == 0)
			break;
	}
	if (time_out >= DCAM_AXI_STOP_TIMEOUT)
		pr_err("wait for dcam axi stop failed: 0x%x!\n",
		REG_RD(DCAM_AHBM_STS));
}

static void _dcam_wait_for_quickstop(enum dcam_path_index path_index)
{
	int time_out = 5000;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	DCAM_TRACE("state before stop 0x%x.\n", s_p_dcam_mod->state);

	if (path_index == DCAM_PATH_IDX_ALL) {
		_dcam_quickstop_set_all();
	} else {
		if (s_p_dcam_mod->dcam_path0.valid &&
		    (DCAM_PATH_IDX_0 & path_index)) {
			_dcam_quickstop_set(DCAM_PATH_IDX_0, DCAM_RST_PATH0,
				BIT_0, BIT_3);
		}
		if (s_p_dcam_mod->dcam_path1.valid &&
			(DCAM_PATH_IDX_1 & path_index)) {
			_dcam_quickstop_set(DCAM_PATH_IDX_1, DCAM_RST_PATH1,
				BIT_1, BIT_4);
		}
		if (s_p_dcam_mod->dcam_path2.valid &&
			(DCAM_PATH_IDX_2 & path_index)) {
			_dcam_quickstop_set(DCAM_PATH_IDX_2, DCAM_RST_PATH2,
				BIT_2, BIT_5);
		}
	}

	DCAM_TRACE("exit _dcam_wait_for_quickstop %d state: 0x%x.\n ",
		    time_out, s_p_dcam_mod->state);
}

static void _dcam_path_pause(enum dcam_path_index path_index)
{
	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	if (s_p_dcam_mod->dcam_path0.valid &&
	    (DCAM_PATH_IDX_0 & path_index)) {
		dcam_glb_reg_mwr(DCAM_CFG, BIT_0, 0, DCAM_CFG_REG);
	}
	if (s_p_dcam_mod->dcam_path1.valid &&
		(DCAM_PATH_IDX_1 & path_index)) {
		dcam_glb_reg_mwr(DCAM_CFG, BIT_1, 0, DCAM_CFG_REG);
	}
	if (s_p_dcam_mod->dcam_path2.valid &&
		(DCAM_PATH_IDX_2 & path_index)) {
		dcam_glb_reg_mwr(DCAM_CFG, BIT_2, 0, DCAM_CFG_REG);
	}
	_dcam_auto_copy(path_index);
}

int32_t dcam_reg_isr(enum dcam_irq_id id, dcam_isr_func user_func,
					  void *user_data)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	unsigned long           flag;

	if (id >= DCAM_IRQ_NUMBER) {
		rtn = DCAM_RTN_ISR_ID_ERR;
	} else {
		spin_lock_irqsave(&dcam_lock, flag);
		s_user_func[id] = user_func;
		s_user_data[id] = user_data;
		spin_unlock_irqrestore(&dcam_lock, flag);
	}
	return -rtn;
}

int32_t dcam_cap_cfg(enum dcam_cfg_id id, void *param)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_cap_desc    *cap_desc = NULL;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return -rtn;
	}

	if (DCAM_ADDR_INVALID(s_dcam_sc_array)) {
		pr_err("zero pointer\n");
		return -rtn;
	}

	cap_desc = &s_p_dcam_mod->dcam_cap;
	switch (id) {
	case DCAM_CAP_SYNC_POL:
	{
		struct dcam_cap_sync_pol *sync_pol =
			(struct dcam_cap_sync_pol *)param;

		if (sync_pol->need_href)
			REG_MWR(MIPI_CAP_CFG, BIT_5, 1 << 5);
		else
			REG_MWR(MIPI_CAP_CFG, BIT_5, 0 << 5);
		break;
	}

	case DCAM_CAP_DATA_BITS:
	{
		enum dcam_cap_data_bits bits =
			*(enum dcam_cap_data_bits *)param;


		if (bits == DCAM_CAP_12_BITS)
			REG_MWR(MIPI_CAP_CFG,  BIT_4 | BIT_3, 2 << 3);
		else if (bits == DCAM_CAP_10_BITS)
			REG_MWR(MIPI_CAP_CFG,  BIT_4 | BIT_3, 1 << 3);
		else if (bits == DCAM_CAP_8_BITS)
			REG_MWR(MIPI_CAP_CFG,  BIT_4 | BIT_3, 0 << 3);
		else
			rtn = DCAM_RTN_CAP_IN_BITS_ERR;
		break;
	}

	case DCAM_CAP_YUV_TYPE:
	{
		enum dcam_cap_pattern pat = *(enum dcam_cap_pattern *)param;


		if (pat < DCAM_PATTERN_MAX)
			REG_MWR(MIPI_CAP_CFG, BIT_8 | BIT_7, pat << 7);
		else
			rtn = DCAM_RTN_CAP_IN_YUV_ERR;
		break;
	}

	case DCAM_CAP_PRE_SKIP_CNT:
	{
		uint32_t skip_num = *(uint32_t *)param;


		if (skip_num > DCAM_CAP_SKIP_FRM_MAX) {
			rtn = DCAM_RTN_CAP_SKIP_FRAME_ERR;
		} else {
			REG_MWR(MIPI_CAP_FRM_CTRL,
				BIT_3 | BIT_2 | BIT_1 | BIT_0, skip_num);
		}
		break;
	}

	case DCAM_CAP_FRM_DECI:
	{
		uint32_t deci_factor = *(uint32_t *)param;


		if (deci_factor < DCAM_FRM_DECI_FAC_MAX) {
			REG_MWR(MIPI_CAP_FRM_CTRL, BIT_5 | BIT_4,
				deci_factor << 4);
		} else {
			rtn = DCAM_RTN_CAP_FRAME_DECI_ERR;
		}
		break;
	}

	case DCAM_CAP_FRM_COUNT_CLR:
		REG_MWR(MIPI_CAP_FRM_CTRL, BIT_22, 1 << 22);
		break;

	case DCAM_CAP_INPUT_RECT:
	{
		struct camera_rect *rect = (struct camera_rect *)param;
		uint32_t         tmp = 0;


		if (rect->x > DCAM_CAP_FRAME_WIDTH_MAX  ||
		    rect->y > DCAM_CAP_FRAME_HEIGHT_MAX ||
		    rect->w > DCAM_CAP_FRAME_WIDTH_MAX  ||
		    rect->h > DCAM_CAP_FRAME_HEIGHT_MAX) {

			rtn = DCAM_RTN_CAP_FRAME_SIZE_ERR;
			return -rtn;
		}

		tmp = rect->x | (rect->y << 16);
		REG_WR(MIPI_CAP_START, tmp);
		tmp = (rect->x + rect->w - 1);
		tmp |= (rect->y + rect->h - 1) << 16;
		REG_WR(MIPI_CAP_END, tmp);
		break;
	}

	case DCAM_CAP_IMAGE_XY_DECI:
	{
		struct dcam_cap_dec *cap_dec = (struct dcam_cap_dec *)param;


		if (cap_dec->x_factor > DCAM_CAP_X_DECI_FAC_MAX ||
		    cap_dec->y_factor > DCAM_CAP_Y_DECI_FAC_MAX) {
			rtn = DCAM_RTN_CAP_XY_DECI_ERR;
		} else {
			if (cap_desc->input_format == DCAM_CAP_MODE_RAWRGB) {
				if (cap_dec->x_factor > 1 ||
				    cap_dec->y_factor > 1) {
					rtn = DCAM_RTN_CAP_XY_DECI_ERR;
				}
			}
			if (cap_desc->input_format == DCAM_CAP_MODE_RAWRGB) {
			/*raw: bit0:1 path bit:1 isp  */
				REG_MWR(MIPI_CAP_IMG_DECI, BIT_1,
					cap_dec->x_factor << 1);
			} else {
				REG_MWR(MIPI_CAP_IMG_DECI, BIT_1 | BIT_0,
					cap_dec->x_factor);
				REG_MWR(MIPI_CAP_IMG_DECI, BIT_3 | BIT_2,
					cap_dec->y_factor << 2);
			}
		}
		break;
	}


	case DCAM_CAP_TO_ISP:
	{
		uint32_t need_isp = *(uint32_t *)param;

		if (need_isp)
			dcam_glb_reg_mwr(DCAM_CFG, BIT_7, 1 << 7, DCAM_CFG_REG);
		else
			dcam_glb_reg_mwr(DCAM_CFG, BIT_7, 0 << 7, DCAM_CFG_REG);
		break;
	}

	case DCAM_CAP_DATA_PACKET:
	{
		uint32_t is_loose = *(uint32_t *)param;

		if (cap_desc->interface == DCAM_CAP_IF_CSI2 &&
			cap_desc->input_format == DCAM_CAP_MODE_RAWRGB) {
			if (is_loose)
				REG_MWR(MIPI_CAP_CFG, BIT_0, 1);
			else
				REG_MWR(MIPI_CAP_CFG, BIT_0, 0);
		} else {
			rtn = DCAM_RTN_MODE_ERR;
		}

		break;
	}

	case DCAM_CAP_DATA_TYPE:
	{
		uint32_t data_type = *(uint32_t *)param;

		if (data_type == DCAM_CAP_MODE_RAWRGB) {
			REG_MWR(IMAGE_CONTROL, MATCH_TYPE_SET_BIT,
				MATCH_BY_DATA_TYPE << MATCH_TYPE_OFFSET);
			REG_MWR(IMAGE_CONTROL, DATA_TYPE_SET_BIT,
				DATA_TYPE_RAW10 << DATA_TYPE_OFFSET);
		} else if (data_type == DCAM_CAP_MODE_YUV) {
			REG_MWR(IMAGE_CONTROL, MATCH_TYPE_SET_BIT,
				MATCH_BY_DATA_TYPE << MATCH_TYPE_OFFSET);
			REG_MWR(IMAGE_CONTROL, DATA_TYPE_SET_BIT,
				DATA_TYPE_YUV422_8BIT << DATA_TYPE_OFFSET);
		} else {
			rtn = DCAM_RTN_PARA_ERR;
		}

		break;
	}

	case DCAM_CAP_DATA_MODE:
	{
		uint32_t data_type = *(uint32_t *)param;

		if (data_type == DCAM_CAP_MODE_RAWRGB)
			REG_MWR(MIPI_CAP_CFG, BIT_1, 1 << 1);
		else if (data_type == DCAM_CAP_MODE_YUV)
			REG_MWR(MIPI_CAP_CFG, BIT_1, 0 << 1);
		else {
			pr_err("cap cfg data mode err\n");
			rtn = DCAM_RTN_PARA_ERR;
		}

		break;
	}


	case DCAM_CAP_SAMPLE_MODE:
	{
		enum dcam_capture_mode samp_mode =
			*(enum dcam_capture_mode *)param;

		if (samp_mode >= DCAM_CAPTURE_MODE_MAX) {
			rtn = DCAM_RTN_MODE_ERR;
		} else {
			REG_MWR(MIPI_CAP_CFG, BIT_6, samp_mode << 6);
			s_p_dcam_mod->dcam_mode = samp_mode;
		}
		break;
	}

	case DCAM_CAP_ZOOM_MODE:
	{
		uint32_t zoom_mode = *(uint32_t *)param;

		s_dcam_sc_array->is_smooth_zoom = zoom_mode;
		break;
	}
	default:
		rtn = DCAM_RTN_IO_ID_ERR;
		break;

	}

	return -rtn;
}

int32_t dcam_path_cfg(enum dcam_path_index path_index,
			enum dcam_cfg_id id, void *param)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc   *path = NULL;
	uint32_t                path_id = DCAM_PATH_MAX;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return -rtn;
	}

	if (DCAM_PATH_IDX_0 & path_index) {
		path = &s_p_dcam_mod->dcam_path0;
		path_id = DCAM_PATH0;
	} else if (DCAM_PATH_IDX_1 & path_index) {
		path = &s_p_dcam_mod->dcam_path1;
		path_id = DCAM_PATH1;
	} else if (DCAM_PATH_IDX_2 & path_index) {
		path = &s_p_dcam_mod->dcam_path2;
		path_id = DCAM_PATH2;
	} else {
		pr_err("%s error", __func__);
		return -DCAM_RTN_IO_ID_ERR;
	}

	switch (id) {
	case DCAM_PATH_INPUT_SIZE:
	{
		struct camera_size *size = (struct camera_size *)param;


		DCAM_TRACE("path %d, INPUT_SIZE {%d %d}..\n",
			path_index, size->w, size->h);

		if ((path_index & DCAM_PATH_IDX_1) &&
		    (size->w > DCAM_PATH_FRAME_WIDTH_MAX ||
		     size->h > DCAM_PATH_FRAME_HEIGHT_MAX)) {
			rtn = DCAM_RTN_PATH_SRC_SIZE_ERR;
		} else if ((path_index & DCAM_PATH_IDX_2) &&
		   (size->w > DCAM_PATH2_FRAME_WIDTH_MAX ||
		    size->h > DCAM_PATH2_FRAME_HEIGHT_MAX)) {
			rtn = DCAM_RTN_PATH_SRC_SIZE_ERR;
		} else {
			path->input_size.w = size->w;
			path->input_size.h = size->h;
			path->valid_param.input_size = 1;
		}
		break;
	}

	case DCAM_PATH_INPUT_RECT:
	{
		struct camera_rect *rect = (struct camera_rect *)param;


		DCAM_TRACE("PATH:%d INPUT_RECT {%d %d %d %d}.\n", path_index,
			rect->x,
			rect->y,
			rect->w,
			rect->h);

		if ((path_index & DCAM_PATH_IDX_1) &&
			(rect->x > DCAM_PATH1_FRAME_WIDTH_MAX ||
			rect->y > DCAM_PATH1_FRAME_HEIGHT_MAX ||
			rect->w > DCAM_PATH1_FRAME_WIDTH_MAX ||
			rect->h > DCAM_PATH1_FRAME_HEIGHT_MAX)) {
			rtn = DCAM_RTN_PATH_TRIM_SIZE_ERR;
			pr_info("path1 in_rect err\n");
		} else if ((path_index & DCAM_PATH_IDX_2) &&
			(rect->x > DCAM_PATH2_FRAME_WIDTH_MAX ||
			rect->y > DCAM_PATH2_FRAME_HEIGHT_MAX ||
			rect->w > DCAM_PATH2_FRAME_WIDTH_MAX ||
			rect->h > DCAM_PATH2_FRAME_HEIGHT_MAX)) {
			rtn = DCAM_RTN_PATH_TRIM_SIZE_ERR;
			pr_info("path2 in_rect err\n");
		} else {
			memcpy((void *)&path->input_rect,
				(void *)rect,
				sizeof(struct camera_rect));
			path->valid_param.input_rect = 1;
		}
		break;
	}

	case DCAM_PATH_OUTPUT_SIZE:
	{
		struct camera_size *size = (struct camera_size *)param;


		if ((DCAM_PATH_IDX_1 | DCAM_PATH_IDX_2) & path_index) {
			DCAM_TRACE("path %d out size {%d %d}.\n",
				ffs(path_index) - 1, size->w, size->h);
			if ((path_index & DCAM_PATH_IDX_1) &&
			    (size->w > DCAM_PATH1_FRAME_WIDTH_MAX ||
			     size->h > DCAM_PATH1_FRAME_HEIGHT_MAX)) {
				rtn = DCAM_RTN_PATH_SRC_SIZE_ERR;
			} else if ((path_index & DCAM_PATH_IDX_2) &&
			    (size->w > DCAM_PATH2_FRAME_WIDTH_MAX ||
			     size->h > DCAM_PATH2_FRAME_HEIGHT_MAX)) {
				rtn = DCAM_RTN_PATH_SRC_SIZE_ERR;
			} else {
				path->output_size.w = size->w;
				path->output_size.h = size->h;
				path->valid_param.output_size = 1;
			}
		}
		break;
	}

	case DCAM_PATH_OUTPUT_FORMAT:
	{
		enum dcam_fmt format = *(enum dcam_fmt *)param;


		if (((DCAM_PATH_IDX_0 & path_index) &&
		     (format == DCAM_RAWRGB)) ||
		    (((DCAM_PATH_IDX_0 | DCAM_PATH_IDX_1 | DCAM_PATH_IDX_2) &
		      path_index) &&
		     (format == DCAM_YUV422 ||
		      format == DCAM_YUV420 ||
		      format == DCAM_YUV420_3FRAME))) {
			path->output_format = format;
			path->valid_param.output_format = 1;
		} else {
			rtn = DCAM_RTN_PATH_OUT_FMT_ERR;
			path->output_format = DCAM_FTM_MAX;
		}
		break;
	}

	case DCAM_PATH_OUTPUT_ADDR:
	{
		struct dcam_addr *p_addr = NULL;

		p_addr = (struct dcam_addr *)param;

		if (DCAM_YUV_ADDR_INVALID(p_addr->yaddr,
			p_addr->uaddr, p_addr->vaddr) && p_addr->mfd_y == 0) {
			rtn = DCAM_RTN_PATH_ADDR_ERR;
		} else {
			struct camera_frame frame;

			memset((void *)&frame, 0, sizeof(struct camera_frame));
			frame.yaddr = p_addr->yaddr;
			frame.uaddr = p_addr->uaddr;
			frame.vaddr = p_addr->vaddr;
			frame.yaddr_vir = p_addr->yaddr_vir;
			frame.uaddr_vir = p_addr->uaddr_vir;
			frame.vaddr_vir = p_addr->vaddr_vir;
			frame.type  = path_id;
			frame.fid   = path->frame_base_id;
			frame.zsl_private = p_addr->zsl_private;
			/*iommu get table*/
			frame.pfinfo.dev = &s_dcam_pdev->dev;
			frame.pfinfo.mfd[0] = p_addr->mfd_y;
			frame.pfinfo.mfd[1] = p_addr->mfd_u;
			frame.pfinfo.mfd[2] = p_addr->mfd_v;
			DCAM_TRACE("addr,i=%d,mfd[0]=0x%x,mfd[1]=0x%x\n",
				path->output_frame_count, frame.pfinfo.mfd[0],
				frame.pfinfo.mfd[1]);
			/*may need update iommu here*/
			rtn = pfiommu_get_sg_table(&frame.pfinfo);
			DCAM_TRACE("addr,size[0]=0x%x, size[1]=0x%x.\n",
				frame.pfinfo.size[0], frame.pfinfo.size[1]);
			if (rtn) {
				pr_err("cfg output addr failed!\n");
				rtn = DCAM_RTN_PATH_ADDR_ERR;
				break;
			}

			frame.pfinfo.offset[0] = 0;
			frame.pfinfo.offset[1] = 0;
			frame.pfinfo.offset[2] = 0;

			if (!_dcam_buf_queue_write(&path->buf_queue, &frame))
				path->output_frame_count++;
			DCAM_TRACE("y=0x%x u=0x%x v=0x%x mfd=0x%x, count: %d\n",
				   p_addr->yaddr, p_addr->uaddr,
				   p_addr->vaddr, frame.pfinfo.mfd[0],
				    path->output_frame_count);
		}
		break;
	}

	case DCAM_PATH_OUTPUT_RESERVED_ADDR:
	{/*TODO: kinlin*/
		struct dcam_addr *p_addr = (struct dcam_addr *)param;

		if (DCAM_YUV_ADDR_INVALID(p_addr->yaddr, p_addr->uaddr,
			p_addr->vaddr) && p_addr->mfd_y == 0) {
			rtn = DCAM_RTN_PATH_ADDR_ERR;
		} else {
			struct camera_frame *frame =
				&s_p_dcam_mod->path_reserved_frame[path_id];
			frame->yaddr = p_addr->yaddr;
			frame->uaddr = p_addr->uaddr;
			frame->vaddr = p_addr->vaddr;
			frame->yaddr_vir = p_addr->yaddr_vir;
			frame->uaddr_vir = p_addr->uaddr_vir;
			frame->vaddr_vir = p_addr->vaddr_vir;
			/*iommu get table*/
			frame->pfinfo.dev = &s_dcam_pdev->dev;
			frame->pfinfo.mfd[0] = p_addr->mfd_y;
			frame->pfinfo.mfd[1] = p_addr->mfd_u;
			frame->pfinfo.mfd[2] = p_addr->mfd_v;
			DCAM_TRACE("addr,i=%d,mfd[0]=0x%x,mfd[1]=0x%x\n",
				path->output_frame_count,
				frame->pfinfo.mfd[0],
				frame->pfinfo.mfd[1]);
			/*may need update iommu here*/
			rtn = pfiommu_get_sg_table(&frame->pfinfo);
			if (rtn) {
				pr_err("cfg output addr failed!\n");
				rtn = DCAM_RTN_PATH_ADDR_ERR;
				break;
			}

			frame->pfinfo.offset[0] = 0;
			frame->pfinfo.offset[1] = 0;
			frame->pfinfo.offset[2] = 0;

			DCAM_TRACE("Path1 R ADDR,i=%d,y=0x%x,u=0x%x\n",
				path->output_frame_count,
				p_addr->yaddr, p_addr->uaddr);
		}
		break;
	}

	case DCAM_PATH_SRC_SEL:
	{
		uint32_t       src_sel = *(uint32_t *)param;

		if (src_sel >= DCAM_PATH_FROM_NONE) {
			rtn = DCAM_RTN_PATH_SRC_ERR;
		} else {
			path->src_sel = src_sel;
			path->valid_param.src_sel = 1;

		}
		break;
	}

	case DCAM_PATH_FRAME_BASE_ID:
	{
		uint32_t          base_id = *(uint32_t *)param;

		DCAM_TRACE("DCAM_PATH_FRAME_BASE_ID 0x%x.\n", base_id);
		path->frame_base_id = base_id;
		break;
	}

	case DCAM_PATH_DATA_ENDIAN:
	{
		struct dcam_endian_sel *endian =
				(struct dcam_endian_sel *)param;


		if (endian->y_endian >= DCAM_ENDIAN_MAX ||
			endian->uv_endian >= DCAM_ENDIAN_MAX) {
			rtn = DCAM_RTN_PATH_ENDIAN_ERR;
		} else {
			path->data_endian.y_endian  = endian->y_endian;
			path->data_endian.uv_endian = endian->uv_endian;
			path->valid_param.data_endian = 1;
		}
		break;
	}

	case DCAM_PATH_ENABLE:
	{
		path->valid = *(uint32_t *)param;

		break;
	}

	case DCAM_PATH_FRAME_TYPE:
	{
		struct camera_frame *frame  = &path->buf_queue.frame[0];
		uint32_t          frm_type = *(uint32_t *)param;
		int               cnt = 0;
		uint32_t          i = 0;


		if (path_id == DCAM_PATH0)
			cnt = DCAM_PATH_0_FRM_CNT_MAX;
		else if (path_id == DCAM_PATH1)
			cnt = DCAM_PATH_1_FRM_CNT_MAX;
		else if (path_id == DCAM_PATH2)
			cnt = DCAM_PATH_2_FRM_CNT_MAX;

		DCAM_TRACE("DCAM_PATH_FRAME_TYPE 0x%x.\n", frm_type);
		for (i = 0; i < cnt; i++)
			(frame+i)->type = frm_type;
		break;
	}

	case DCAM_PATH_FRM_DECI:
	{
		uint32_t deci_factor = *(uint32_t *)param;


		if (deci_factor >= DCAM_FRM_DECI_FAC_MAX) {
			rtn = DCAM_RTN_PATH_FRM_DECI_ERR;
		} else {
			path->frame_deci = deci_factor;
			path->valid_param.frame_deci = 1;
		}
		break;
	}

	case DCAM_PATH_ROT_MODE:
	{
	/*Pike2 path0/path2/path3 support rotation.*/
	/*0x00:no rot; 0x01:flip; 0x10:180; 0x11:mirror*/
		uint32_t rot_mode = *(uint32_t *)param;

		if (rot_mode >= DCAM_PATH_FRAME_ROT_MAX) {
			rtn = DCAM_RTN_PATH_FRM_DECI_ERR;
		} else {
			path->rot_mode = rot_mode;
			path->valid_param.rot_mode = 1;
			DCAM_TRACE("path%d rot %d.\n", path_id, path->rot_mode);
		}
		break;
	}

	case DCAM_PATH_SHRINK:
		if ((DCAM_PATH1 | DCAM_PATH2) & path_id) {
			struct dcam_regular_desc *regular_desc =
				(struct dcam_regular_desc *)param;

			memcpy(&path->regular_desc, param,
				sizeof(struct dcam_regular_desc));
			if (regular_desc->regular_mode == DCAM_REGULAR_BYPASS)
				path->valid_param.shrink = 0;
			else
				path->valid_param.shrink = 1;
		} else {
			pr_info("Only path%d not support shrink", path_id);
		}
		break;

	case DCAM_PDAF_CONTROL:
		if (path_id == DCAM_PATH0) {
			memcpy(&path->pdaf_ctrl, param,
			       sizeof(struct sprd_pdaf_control));
			path->valid_param.pdaf_ctrl = 1;
		}
		break;

	default:
		break;
	}

	return -rtn;
}

int32_t dcam_path0_cfg(enum dcam_cfg_id id, void *param)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;

	rtn = dcam_path_cfg(DCAM_PATH_IDX_0, id, param);
	return rtn;
}

int32_t dcam_path1_cfg(enum dcam_cfg_id id, void *param)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;

	rtn = dcam_path_cfg(DCAM_PATH_IDX_1, id, param);
	return rtn;
}

int32_t dcam_path2_cfg(enum dcam_cfg_id id, void *param)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;

	rtn = dcam_path_cfg(DCAM_PATH_IDX_2, id, param);
	return rtn;
}

int32_t    dcam_get_resizer(void)
{
	wait_for_completion(&dcam_resizer_com);
	pr_info("%s: %d\n", __func__, current->pid);
	return 0;
}

int32_t    dcam_rel_resizer(void)
{
	pr_info("%s:%d.\n", __func__, current->pid);
	complete(&dcam_resizer_com);
	return 0;
}

int32_t    dcam_resize_start(void)
{
	mutex_lock(&dcam_scale_sema);
	atomic_inc(&s_resize_flag);
	return 0;
}

int32_t    dcam_resize_end(void)
{
	atomic_dec(&s_resize_flag);
	mutex_unlock(&dcam_scale_sema);
	return 0;
}

void dcam_int_en(void)
{
	if (atomic_read(&s_dcam_users) == 1)
		enable_irq(DCAM_IRQ);
}

void dcam_int_dis(void)
{
	if (atomic_read(&s_dcam_users) == 1)
		disable_irq(DCAM_IRQ);
}

int32_t dcam_frame_is_locked(struct camera_frame *frame)
{
	uint32_t                rtn = 0;
	unsigned long           flags;

	/*To disable irq*/
	local_irq_save(flags);
	if (frame)
		rtn = frame->lock == DCAM_FRM_LOCK_WRITE ? 1 : 0;
	local_irq_restore(flags);
	/*To enable irq*/

	return -rtn;
}

int32_t dcam_frame_lock(struct camera_frame *frame)
{
	uint32_t                rtn = 0;
	unsigned long           flags;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	DCAM_TRACE("lock %d.\n", (uint32_t)(0xF&frame->fid));

	/*To disable irq*/
	local_irq_save(flags);
	if (likely(frame))
		frame->lock = DCAM_FRM_LOCK_WRITE;
	else
		rtn = DCAM_RTN_PARA_ERR;
	local_irq_restore(flags);
	/*To enable irq*/

	return -rtn;
}

int32_t dcam_frame_unlock(struct camera_frame *frame)
{
	uint32_t                rtn = 0;
	unsigned long           flags;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	DCAM_TRACE("unlock %d.\n", (uint32_t)(0xF&frame->fid));

	/*To disable irq*/
	local_irq_save(flags);
	if (likely(frame))
		frame->lock = DCAM_FRM_UNLOCK;
	else
		rtn = DCAM_RTN_PARA_ERR;
	local_irq_restore(flags);
	/*To enable irq*/

	return -rtn;
}

int32_t    dcam_read_registers(uint32_t *reg_buf, uint32_t *buf_len)
{
	uint32_t                *reg_addr = (uint32_t *)DCAM_BASE;

	if (reg_buf == NULL || buf_len == NULL || (*buf_len % 4) != 0)
		return -1;

	if (atomic_read(&s_dcam_users) == 1) {
		while (*buf_len != 0 && (unsigned long)reg_addr < DCAM_END) {
			*reg_buf++ = REG_RD(reg_addr);
			reg_addr++;
			*buf_len -= 4;
		}
		*buf_len = (unsigned long)reg_addr - DCAM_BASE;
	}

	return 0;
}

int32_t    dcam_get_path_id(struct dcam_get_path_id *path_id,
			    uint32_t *channel_id)
{
	int                      ret = DCAM_RTN_SUCCESS;

	if (path_id == NULL || channel_id == NULL)
		return -1;

	pr_info("fourcc 0x%x, input w:%d, h:%d, output w:%d, h:%d\n",
		path_id->fourcc,
		path_id->input_size.w, path_id->input_size.h,
		path_id->output_size.w, path_id->output_size.h);

	if (path_id->need_isp_tool) {
		*channel_id = DCAM_PATH0;
	} else if (path_id->fourcc == V4L2_PIX_FMT_GREY &&
		!path_id->is_path_work[DCAM_PATH0]) {
		*channel_id = DCAM_PATH0;
	} else if (path_id->need_interp &&
		!path_id->is_path_work[DCAM_PATH0] &&
		path_id->output_size.w == path_id->input_trim.w &&
		path_id->output_size.h == path_id->input_trim.h) {
		*channel_id = DCAM_PATH0;
	} else if (path_id->output_size.w <= DCAM_PATH1_LINE_BUF_LENGTH &&
		!path_id->is_path_work[DCAM_PATH1]) {
		*channel_id = DCAM_PATH1;
	} else if (path_id->output_size.w <= DCAM_PATH2_LINE_BUF_LENGTH  &&
		!path_id->is_path_work[DCAM_PATH2]) {
		*channel_id = DCAM_PATH2;
	} else {
		*channel_id = DCAM_PATH0;
	}
	pr_info("path id %d, isp_tool %d, interp %d, work {%d, %d, %d}\n",
		*channel_id, path_id->need_isp_tool, path_id->need_interp,
		path_id->is_path_work[DCAM_PATH0],
		path_id->is_path_work[DCAM_PATH1],
		path_id->is_path_work[DCAM_PATH2]);

	return ret;
}

int32_t    dcam_get_path_capability(struct dcam_path_capability *capacity)
{
	int                      ret = DCAM_RTN_SUCCESS;

	if (capacity == NULL)
		return -1;

	capacity->count = 3;
	capacity->path_info[DCAM_PATH0].line_buf = 0;
	capacity->path_info[DCAM_PATH0].support_yuv = 1;
	capacity->path_info[DCAM_PATH0].support_raw = 1;
	capacity->path_info[DCAM_PATH0].support_jpeg = 0;
	capacity->path_info[DCAM_PATH0].support_scaling = 0;
	capacity->path_info[DCAM_PATH0].support_trim = 1;
	capacity->path_info[DCAM_PATH0].is_scaleing_path = 0;

	capacity->path_info[DCAM_PATH1].line_buf = DCAM_PATH1_LINE_BUF_LENGTH;
	capacity->path_info[DCAM_PATH1].support_yuv = 1;
	capacity->path_info[DCAM_PATH1].support_raw = 0;
	capacity->path_info[DCAM_PATH1].support_jpeg = 0;
	capacity->path_info[DCAM_PATH1].support_scaling = 1;
	capacity->path_info[DCAM_PATH1].support_trim = 1;
	capacity->path_info[DCAM_PATH1].is_scaleing_path = 0;

	capacity->path_info[DCAM_PATH2].line_buf = DCAM_PATH2_LINE_BUF_LENGTH;
	capacity->path_info[DCAM_PATH2].support_yuv = 1;
	capacity->path_info[DCAM_PATH2].support_raw = 0;
	capacity->path_info[DCAM_PATH2].support_jpeg = 0;
	capacity->path_info[DCAM_PATH2].support_scaling = 1;
	capacity->path_info[DCAM_PATH2].support_trim = 1;
	capacity->path_info[DCAM_PATH2].is_scaleing_path = 1;

	return ret;
}

static uint32_t _dcam_get_path_deci_factor(uint32_t src_size, uint32_t dst_size)
{
	uint32_t                 factor = 0;

	if (src_size == 0 || dst_size == 0)
		return factor;

	/* factor: 0 - 1/2, 1 - 1/4, 2 - 1/8, 3 - 1/16 */
	for (factor = 0; factor < DCAM_PATH_DECI_FAC_MAX; factor++) {
		if (src_size < (uint32_t)(dst_size * (1 << (factor + 1))))
			break;
	}

	return factor;
}

static void _dcam_path_updated_notice(enum dcam_path_index path_index)
{
	struct dcam_path_desc   *p_path = NULL;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	if (path_index == DCAM_PATH_IDX_0) {
		p_path = &s_p_dcam_mod->dcam_path0;
	} else if (path_index == DCAM_PATH_IDX_1) {
		p_path = &s_p_dcam_mod->dcam_path1;
	} else if (path_index == DCAM_PATH_IDX_2) {
		p_path = &s_p_dcam_mod->dcam_path2;
	} else {
		pr_info("Wrong index 0x%x.\n", path_index);
		return;
	}
	if (p_path->wait_for_sof) {
		complete(&p_path->sof_com);
		p_path->wait_for_sof = 0;
	}
}


static int32_t _dcam_get_valid_sc_coeff(struct dcam_sc_array *sc,
				       struct dcam_sc_coeff **sc_coeff)
{
	if (DCAM_ADDR_INVALID(sc) || DCAM_ADDR_INVALID(sc_coeff)) {
		pr_info("get valid sc, invalid param %p, %p.\n",
			sc,
			sc_coeff);
		return -1;
	}
	if (sc->valid_cnt == 0) {
		pr_info("valid cnt 0.\n");
		return -1;
	}

	*sc_coeff  = sc->scaling_coeff_queue[0];
	DCAM_TRACE("get valid sc, %d.\n", sc->valid_cnt);
	return 0;
}


static int32_t _dcam_push_sc_buf(struct dcam_sc_array *sc, uint32_t index)
{
	if (DCAM_ADDR_INVALID(sc)) {
		pr_info("push sc, invalid param %p.\n", sc);
		return -1;
	}
	if (sc->valid_cnt >= DCAM_SC_COEFF_BUF_COUNT) {
		pr_info("valid cnt %d.\n", sc->valid_cnt);
		return -1;
	}

	sc->scaling_coeff[index].flag = 1;
	sc->scaling_coeff_queue[sc->valid_cnt] = &sc->scaling_coeff[index];
	sc->valid_cnt++;

	DCAM_TRACE("push sc, %d.\n", sc->valid_cnt);

	return 0;
}

static int32_t _dcam_pop_sc_buf(struct dcam_sc_array *sc,
				struct dcam_sc_coeff **sc_coeff)
{
	uint32_t                i = 0;

	if (DCAM_ADDR_INVALID(sc) || DCAM_ADDR_INVALID(sc_coeff)) {
		pr_info("pop sc, invalid param %p, %p.\n",
			sc,
			sc_coeff);
		return -1;
	}
	if (sc->valid_cnt == 0) {
		pr_info("valid cnt 0.\n");
		return -1;
	}
	sc->scaling_coeff_queue[0]->flag = 0;
	*sc_coeff  = sc->scaling_coeff_queue[0];
	sc->valid_cnt--;
	for (i = 0; i < sc->valid_cnt; i++)
		sc->scaling_coeff_queue[i] = sc->scaling_coeff_queue[i+1];
	DCAM_TRACE("pop sc, %d.\n", sc->valid_cnt);
	return 0;
}

static int32_t _dcam_write_sc_coeff(enum dcam_path_index path_index)
{
	int32_t                 ret = 0;
	struct dcam_path_desc   *path = NULL;
	uint32_t                i = 0;
	unsigned long           h_coeff_addr = DCAM_BASE;
	unsigned long           v_coeff_addr  = DCAM_BASE;
	unsigned long           v_chroma_coeff_addr  = DCAM_BASE;
	uint32_t                *tmp_buf = NULL;
	uint32_t                *h_coeff = NULL;
	uint32_t                *v_coeff = NULL;
	uint32_t                *v_chroma_coeff = NULL;
	uint32_t                scale2yuv420 = 0;
	struct dcam_sc_coeff    *sc_coeff;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	if (DCAM_ADDR_INVALID(s_dcam_sc_array)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	if (path_index != DCAM_PATH_IDX_1 && path_index != DCAM_PATH_IDX_2)
		return -DCAM_RTN_PARA_ERR;

	ret = _dcam_get_valid_sc_coeff(s_dcam_sc_array, &sc_coeff);
	if (ret)
		return -DCAM_RTN_PATH_NO_MEM;
	tmp_buf = sc_coeff->buf;
	if (tmp_buf == NULL)
		return -DCAM_RTN_PATH_NO_MEM;

	h_coeff = tmp_buf;
	v_coeff = tmp_buf + (DCAM_SC_COEFF_COEF_SIZE/4);
	v_chroma_coeff = v_coeff + (DCAM_SC_COEFF_COEF_SIZE/4);

	if (path_index == DCAM_PATH_IDX_1) {
		path = &sc_coeff->dcam_path1;
		h_coeff_addr += DCAM_SC1_H_TAB_OFFSET;
		v_coeff_addr += DCAM_SC1_V_TAB_OFFSET;
		v_chroma_coeff_addr += DCAM_SC1_V_CHROMA_TAB_OFFSET;
	} else if (path_index == DCAM_PATH_IDX_2) {
		path = &s_p_dcam_mod->dcam_path2;
		h_coeff_addr += DCAM_SC2_H_TAB_OFFSET;
		v_coeff_addr += DCAM_SC2_V_TAB_OFFSET;
		v_chroma_coeff_addr += DCAM_SC2_V_CHROMA_TAB_OFFSET;
	}

	if (path->output_format == DCAM_YUV420)
		scale2yuv420 = 1;

	DCAM_TRACE("_dcam_write_sc_coeff {%d %d %d %d}, 420=%d.\n",
		path->sc_input_size.w,
		path->sc_input_size.h,
		path->output_size.w,
		path->output_size.h, scale2yuv420);

	for (i = 0; i < DCAM_SC_COEFF_H_NUM; i++) {
		REG_WR(h_coeff_addr, *h_coeff);
		h_coeff_addr += 4;
		h_coeff++;
	}

	for (i = 0; i < DCAM_SC_COEFF_V_NUM; i++) {
		REG_WR(v_coeff_addr, *v_coeff);
		v_coeff_addr += 4;
		v_coeff++;
	}

	for (i = 0; i < DCAM_SC_COEFF_V_CHROMA_NUM; i++) {
		REG_WR(v_chroma_coeff_addr, *v_chroma_coeff);
		v_chroma_coeff_addr += 4;
		v_chroma_coeff++;
	}

	DCAM_TRACE("_dcam_write_sc_coeff E.\n");

	return ret;
}

static int32_t _dcam_calc_sc_coeff(enum dcam_path_index path_index)
{
	unsigned long           flag;
	struct dcam_path_desc   *path = NULL;
	unsigned long           h_coeff_addr = DCAM_BASE;
	unsigned long           v_coeff_addr  = DCAM_BASE;
	unsigned long           v_chroma_coeff_addr  = DCAM_BASE;
	uint32_t                *tmp_buf = NULL;
	uint32_t                *h_coeff = NULL;
	uint32_t                *v_coeff = NULL;
	uint32_t                *v_chroma_coeff = NULL;
	uint32_t                scale2yuv420 = 0;
	uint8_t                 y_tap = 0;
	uint8_t                 uv_tap = 0;
	uint32_t                index = 0;
	struct dcam_sc_coeff    *sc_coeff;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	if (DCAM_ADDR_INVALID(s_dcam_sc_array)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	if (path_index != DCAM_PATH_IDX_1 && path_index != DCAM_PATH_IDX_2)
		return -DCAM_RTN_PARA_ERR;

	if (path_index == DCAM_PATH_IDX_1) {
		path = &s_p_dcam_mod->dcam_path1;
		h_coeff_addr += DCAM_SC1_H_TAB_OFFSET;
		v_coeff_addr += DCAM_SC1_V_TAB_OFFSET;
		v_chroma_coeff_addr += DCAM_SC1_V_CHROMA_TAB_OFFSET;
	} else if (path_index == DCAM_PATH_IDX_2) {
		path = &s_p_dcam_mod->dcam_path2;
		h_coeff_addr += DCAM_SC2_H_TAB_OFFSET;
		v_coeff_addr += DCAM_SC2_V_TAB_OFFSET;
		v_chroma_coeff_addr += DCAM_SC2_V_CHROMA_TAB_OFFSET;
	}

	if (path->output_format == DCAM_YUV420)
		scale2yuv420 = 1;

	DCAM_TRACE("_dcam_calc_sc_coeff {%d %d %d %d}, 420=%d.\n",
		path->sc_input_size.w,
		path->sc_input_size.h,
		path->output_size.w,
		path->output_size.h, scale2yuv420);

	wait_for_completion(&s_p_dcam_mod->scale_coeff_mem_com);

	spin_lock_irqsave(&dcam_lock, flag);
	tmp_buf = dcam_get_scale_coeff_addr(&index);
	if (tmp_buf == NULL) {
		_dcam_pop_sc_buf(s_dcam_sc_array, &sc_coeff);
		tmp_buf = dcam_get_scale_coeff_addr(&index);
	}
	spin_unlock_irqrestore(&dcam_lock, flag);

	if (tmp_buf == NULL)
		return -DCAM_RTN_PATH_NO_MEM;

	h_coeff = tmp_buf;
	v_coeff = tmp_buf + (DCAM_SC_COEFF_COEF_SIZE/4);
	v_chroma_coeff = v_coeff + (DCAM_SC_COEFF_COEF_SIZE/4);

	if (!(dcam_gen_scale_coeff((int16_t)path->sc_input_size.w,
		(int16_t)path->sc_input_size.h,
		(int16_t)path->output_size.w,
		(int16_t)path->output_size.h,
		h_coeff,
		v_coeff,
		v_chroma_coeff,
		scale2yuv420,
		&y_tap,
		&uv_tap,
		tmp_buf + (DCAM_SC_COEFF_COEF_SIZE*3/4),
		DCAM_SC_COEFF_TMP_SIZE))) {
		pr_info("_dcam_calc_sc_coeff Dcam_GenScaleCoeff error!\n");
		complete(&s_p_dcam_mod->scale_coeff_mem_com);
		return -DCAM_RTN_PATH_GEN_COEFF_ERR;
	}
	path->scale_tap.y_tap = y_tap;
	path->scale_tap.uv_tap = uv_tap;
	path->valid_param.scale_tap = 1;
	memcpy(&s_dcam_sc_array->scaling_coeff[index].dcam_path1, path,
			sizeof(struct dcam_path_desc));
	spin_lock_irqsave(&dcam_lock, flag);
	_dcam_push_sc_buf(s_dcam_sc_array, index);
	spin_unlock_irqrestore(&dcam_lock, flag);

	complete(&s_p_dcam_mod->scale_coeff_mem_com);
	DCAM_TRACE("_dcam_calc_sc_coeff E.\n");

	return DCAM_RTN_SUCCESS;
}

static int32_t _dcam_set_sc_coeff(enum dcam_path_index path_index)
{
	struct dcam_path_desc   *path = NULL;
	uint32_t                i = 0;
	unsigned long           h_coeff_addr = DCAM_BASE;
	unsigned long           v_coeff_addr  = DCAM_BASE;
	unsigned long           v_chroma_coeff_addr  = DCAM_BASE;
	uint32_t                *tmp_buf = NULL;
	uint32_t                *h_coeff = NULL;
	uint32_t                *v_coeff = NULL;
	uint32_t                *v_chroma_coeff = NULL;
	uint32_t                scale2yuv420 = 0;
	uint8_t                 y_tap = 0;
	uint8_t                 uv_tap = 0;
	uint32_t                index = 0;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	if (path_index != DCAM_PATH_IDX_1 && path_index != DCAM_PATH_IDX_2)
		return -DCAM_RTN_PARA_ERR;

	if (path_index == DCAM_PATH_IDX_1) {
		path = &s_p_dcam_mod->dcam_path1;
		h_coeff_addr += DCAM_SC1_H_TAB_OFFSET;
		v_coeff_addr += DCAM_SC1_V_TAB_OFFSET;
		v_chroma_coeff_addr += DCAM_SC1_V_CHROMA_TAB_OFFSET;
	} else if (path_index == DCAM_PATH_IDX_2) {
		path = &s_p_dcam_mod->dcam_path2;
		h_coeff_addr += DCAM_SC2_H_TAB_OFFSET;
		v_coeff_addr += DCAM_SC2_V_TAB_OFFSET;
		v_chroma_coeff_addr += DCAM_SC2_V_CHROMA_TAB_OFFSET;
	}

	if (path->output_format == DCAM_YUV420)
		scale2yuv420 = 1;

	DCAM_TRACE("_dcam_set_sc_coeff {%d %d %d %d}, 420=%d.\n",
		path->sc_input_size.w,
		path->sc_input_size.h,
		path->output_size.w,
		path->output_size.h, scale2yuv420);


	tmp_buf = dcam_get_scale_coeff_addr(&index);

	if (tmp_buf == NULL)
		return -DCAM_RTN_PATH_NO_MEM;

	h_coeff = tmp_buf;
	v_coeff = tmp_buf + (DCAM_SC_COEFF_COEF_SIZE/4);
	v_chroma_coeff = v_coeff + (DCAM_SC_COEFF_COEF_SIZE/4);

	wait_for_completion(&s_p_dcam_mod->scale_coeff_mem_com);

	if (!(dcam_gen_scale_coeff((int16_t)path->sc_input_size.w,
		(int16_t)path->sc_input_size.h,
		(int16_t)path->output_size.w,
		(int16_t)path->output_size.h,
		h_coeff,
		v_coeff,
		v_chroma_coeff,
		scale2yuv420,
		&y_tap,
		&uv_tap,
		tmp_buf + (DCAM_SC_COEFF_COEF_SIZE*3/4),
		DCAM_SC_COEFF_TMP_SIZE))) {
		pr_info("_dcam_set_sc_coeff Dcam_GenScaleCoeff error!.\n");
		complete(&s_p_dcam_mod->scale_coeff_mem_com);
		return -DCAM_RTN_PATH_GEN_COEFF_ERR;
	}

	for (i = 0; i < DCAM_SC_COEFF_H_NUM; i++) {
		REG_WR(h_coeff_addr, *h_coeff);
		h_coeff_addr += 4;
		h_coeff++;
	}

	for (i = 0; i < DCAM_SC_COEFF_V_NUM; i++) {
		REG_WR(v_coeff_addr, *v_coeff);
		v_coeff_addr += 4;
		v_coeff++;
	}

	for (i = 0; i < DCAM_SC_COEFF_V_CHROMA_NUM; i++) {
		REG_WR(v_chroma_coeff_addr, *v_chroma_coeff);
		v_chroma_coeff_addr += 4;
		v_chroma_coeff++;
	}

	path->scale_tap.y_tap = y_tap;
	path->scale_tap.uv_tap = uv_tap;
	path->valid_param.scale_tap = 1;

	complete(&s_p_dcam_mod->scale_coeff_mem_com);

	return DCAM_RTN_SUCCESS;
}

static int32_t _dcam_calc_sc_size(enum dcam_path_index path_index)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc   *path = NULL;
	unsigned long           cfg_reg = 0;
	uint32_t                tmp_dstsize = 0;
	uint32_t                align_size = 0;
	uint32_t		raw_w = 0;
	uint32_t		raw_h = 0;
	uint32_t                d_max = DCAM_SC_COEFF_DOWN_MAX;
	uint32_t                u_max = DCAM_SC_COEFF_UP_MAX;
	uint32_t                f_max = DCAM_PATH_DECI_FAC_MAX;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	if (path_index == DCAM_PATH_IDX_1) {
		path = &s_p_dcam_mod->dcam_path1;
		cfg_reg = DCAM_PATH1_CFG;
	} else if (path_index == DCAM_PATH_IDX_2) {
		path = &s_p_dcam_mod->dcam_path2;
		cfg_reg = DCAM_PATH2_CFG;
	}

	if (DCAM_ADDR_INVALID(path)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}
	if (path->input_rect.w > (path->output_size.w * d_max *  (1<<f_max)) ||
	    path->input_rect.h > (path->output_size.h * d_max *  (1<<f_max)) ||
	    path->input_rect.w * u_max < path->output_size.w ||
	    path->input_rect.h * u_max < path->output_size.h) {
		rtn = DCAM_RTN_PATH_SC_ERR;
	} else {
		path->sc_input_size.w = path->input_rect.w;
		path->sc_input_size.h = path->input_rect.h;
		if (path->input_rect.w > path->output_size.w * d_max) {
			tmp_dstsize = path->output_size.w * d_max;
			path->deci_val.deci_x =
				_dcam_get_path_deci_factor(path->input_rect.w,
							   tmp_dstsize);
			path->deci_val.deci_x_en = 1;
			path->valid_param.v_deci = 1;
			align_size = (1 << (path->deci_val.deci_x+1)) *
					DCAM_PIXEL_ALIGN_WIDTH;
			raw_w = path->input_rect.w;
			path->input_rect.w = (path->input_rect.w) &
						    ~(align_size-1);
			path->input_rect.x += (raw_w - path->input_rect.w) >> 1;
			path->input_rect.x &= ~1;
			path->sc_input_size.w = path->input_rect.w >>
						    (path->deci_val.deci_x+1);
		} else {
			path->deci_val.deci_x = 0;
			path->deci_val.deci_x_en = 0;
			path->valid_param.v_deci = 1;
		}

		if (path->input_rect.h > path->output_size.h * d_max) {
			tmp_dstsize = path->output_size.h * d_max;
			path->deci_val.deci_y =
				_dcam_get_path_deci_factor(path->input_rect.h,
							tmp_dstsize);
			path->deci_val.deci_y_en = 1;
			path->valid_param.v_deci = 1;
			raw_h = path->input_rect.h;
			align_size = (1 << (path->deci_val.deci_y+1))*
							DCAM_PIXEL_ALIGN_HEIGHT;
			path->input_rect.h = (path->input_rect.h) &
							~(align_size-1);
			path->input_rect.y += (raw_h - path->input_rect.h)>>1;
			path->input_rect.y &= ~1;
			path->sc_input_size.h = path->input_rect.h >>
						    (path->deci_val.deci_y+1);
		} else {
			path->deci_val.deci_y = 0;
			path->deci_val.deci_y_en = 0;
			path->valid_param.v_deci = 1;
		}

	}

	DCAM_TRACE("calc_sc,path=%d,x_en=%d,deci_x=%d,y_en=%d, deci_y=%d.\n",
			path_index,
			path->deci_val.deci_x_en, path->deci_val.deci_x,
			path->deci_val.deci_y_en, path->deci_val.deci_y);

	return -rtn;
}

static int32_t _dcam_path_scaler(enum dcam_path_index path_index)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc   *path = NULL;
	unsigned long           cfg_reg = 0;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	if (DCAM_ADDR_INVALID(s_dcam_sc_array)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	if (path_index == DCAM_PATH_IDX_1) {
		path = &s_p_dcam_mod->dcam_path1;
		cfg_reg = DCAM_PATH1_CFG;
	} else if (path_index == DCAM_PATH_IDX_2) {
		path = &s_p_dcam_mod->dcam_path2;
		cfg_reg = DCAM_PATH2_CFG;
	}

	if (DCAM_ADDR_INVALID(path)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}
	if (path->output_format == DCAM_RAWRGB ||
		path->output_format == DCAM_JPEG) {
		DCAM_TRACE("path_scaler out format is %d, no need scaler.\n",
			    path->output_format);
		return DCAM_RTN_SUCCESS;
	}

	rtn = _dcam_calc_sc_size(path_index);
	if (rtn != DCAM_RTN_SUCCESS)
		return rtn;

	if (path->sc_input_size.w == path->output_size.w &&
		path->sc_input_size.h == path->output_size.h &&
		path->output_format == DCAM_YUV422) {
		/* bypass scaler if the output size equals
		 * input size for YUV422 format
		 */
		REG_MWR(cfg_reg, BIT_20, 1 << 20);
	} else {
		REG_MWR(cfg_reg, BIT_20, 0 << 20);
		if (s_dcam_sc_array->is_smooth_zoom
			&& path_index == DCAM_PATH_IDX_1
			&& path->status == DCAM_ST_START) {
			rtn = _dcam_calc_sc_coeff(path_index);
		} else {
			rtn = _dcam_set_sc_coeff(path_index);
		}
	}

	return rtn;
}

static void _dcam_stopped_notice(void)
{
	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	if (s_p_dcam_mod->wait_stop) {
		complete(&s_p_dcam_mod->stop_com);
		s_p_dcam_mod->wait_stop = 0;
	}
}

static void _dcam_wait_path_stopped(void)
{
	int ret = 0;
	unsigned long flag;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	if (s_p_dcam_mod->err_happened)
		return;

	spin_lock_irqsave(&dcam_lock, flag);
	s_p_dcam_mod->wait_stop = 1;
	spin_unlock_irqrestore(&dcam_lock, flag);
	ret = wait_for_completion_timeout(&s_p_dcam_mod->stop_com,
					DCAM_PATH_TIMEOUT);
	if (ret <= 0) {
		_dcam_reg_trace();
		pr_info("Failed to wait stop com.\n");
	}
}


#if 0
static void    _dcam_sensor_sof(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_SN_SOF];
	void                    *data = s_user_data[DCAM_SN_SOF];

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	DCAM_TRACE("DCAM: _sn_sof.\n");

	if (user_func)
		(*user_func)(NULL, data);
}
#endif


static void    _dcam_sensor_eof(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_SN_EOF];
	void                    *data = s_user_data[DCAM_SN_EOF];

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	if (user_func)
		(*user_func)(NULL, data);
}

static void    _dcam_cap_sof(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_CAP_SOF];
	void                    *data = s_user_data[DCAM_CAP_SOF];

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}
	DCAM_TRACE("cap sof\n");

	_dcam_stopped_notice();

	if (user_func)
		(*user_func)(NULL, data);
}

static void    _dcam_cap_eof(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_CAP_EOF];
	void                    *data = s_user_data[DCAM_CAP_EOF];

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	if (user_func)
		(*user_func)(NULL, data);
}

static void    _dcam_path0_done(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	dcam_isr_func           user_func = s_user_func[DCAM_PATH0_DONE];
	void                    *data = s_user_data[DCAM_PATH0_DONE];
	struct dcam_path_desc   *path;
	struct camera_frame       frame;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}
	path = &s_p_dcam_mod->dcam_path0;
	if (path->valid == 0) {
		pr_info("path0 not valid.\n");
		return;
	}
	if (path->need_stop) {
		dcam_glb_reg_awr(DCAM_CFG, ~BIT_0, DCAM_CFG_REG);
		path->need_stop = 0;
	}
	_dcam_path_done_notice(DCAM_PATH_IDX_0);
	DCAM_TRACE("path 0 done.\n");
	if (path->sof_cnt < 1) {
		pr_info("path0 done cnt %d.\n", path->sof_cnt);
		path->need_wait = 0;
		return;
	}
	path->sof_cnt = 0;

	if (path->need_wait) {
		path->need_wait = 0;
	} else {
	/*TODO: kinlin*/
		rtn = _dcam_frame_dequeue(&path->frame_queue, &frame);

		if (pfiommu_check_addr(&frame.pfinfo)) {
			pr_err("the frame has been broken!\n");
			return;
		}
		if (frame.pfinfo.dev == NULL)
			pr_info("DCAM%d done dev NULL %p\n",
				DCAM_PATH0, frame.pfinfo.dev);

		pfiommu_free_addr(&frame.pfinfo);

		if (rtn == 0 && frame.pfinfo.mfd[0] !=
		    s_p_dcam_mod->path_reserved_frame[DCAM_PATH0].pfinfo.mfd[0]
		    && dcam_frame_is_locked(&frame) != 0) {
			frame.width = path->output_size.w;
			frame.height = path->output_size.h;
			frame.irq_type = CAMERA_IRQ_IMG;
			DCAM_TRACE("path0 frame 0x%x, y uv, 0x%x 0x%x.\n",
				(int)&frame, frame.yaddr, frame.uaddr);
			if (user_func)
				(*user_func)(&frame, data);
		} else {
			DCAM_TRACE("path0_reserved_frame.\n");
			s_p_dcam_mod->
			    path_reserved_frame[DCAM_PATH0].pfinfo.iova[0] = 0;
			s_p_dcam_mod->
			    path_reserved_frame[DCAM_PATH0].pfinfo.iova[1] = 0;
		}
	}
}

static void    _dcam_path0_overflow(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_PATH0_OV];
	void                    *data = s_user_data[DCAM_PATH0_OV];
	struct dcam_path_desc   *path;
	struct camera_frame       frame;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	pr_info("_path0_overflow.\n");
	path = &s_p_dcam_mod->dcam_path0;
	_dcam_frame_dequeue(&path->frame_queue, &frame);

	if (user_func)
		(*user_func)(&frame, data);
}

static void    _dcam_path1_done(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	dcam_isr_func           user_func = s_user_func[DCAM_PATH1_DONE];
	void                    *data = s_user_data[DCAM_PATH1_DONE];
	struct dcam_path_desc   *path;
	struct camera_frame       frame;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}
	path = &s_p_dcam_mod->dcam_path1;
	if (path->valid == 0) {
		pr_info("path1 not valid.\n");
		return;
	}

	DCAM_TRACE("path 1 d.\n");

	if (path->need_stop) {
		dcam_glb_reg_awr(DCAM_CFG, ~BIT_1, DCAM_CFG_REG);
		path->need_stop = 0;
	}
	_dcam_path_done_notice(DCAM_PATH_IDX_1);

	if (path->sof_cnt < 1) {
		pr_info("path1 done cnt %d.\n", path->sof_cnt);
		path->need_wait = 0;
		return;
	}
	path->sof_cnt = 0;

	if (path->need_wait) {
		path->need_wait = 0;
	} else {
		rtn = _dcam_frame_dequeue(&path->frame_queue, &frame);
		if (pfiommu_check_addr(&frame.pfinfo)) {
			pr_err("the frame has been broken!\n");
			return;
		}
		if (frame.pfinfo.dev == NULL)
			pr_info("DCAM%d done dev NULL %p\n",
				DCAM_PATH1, frame.pfinfo.dev);

		pfiommu_free_addr(&frame.pfinfo);

		if (rtn == 0 && frame.pfinfo.mfd[0] !=
		    s_p_dcam_mod->path_reserved_frame[DCAM_PATH1].pfinfo.mfd[0]
		    && dcam_frame_is_locked(&frame) != 0) {
			frame.width = path->output_size.w;
			frame.height = path->output_size.h;
			frame.irq_type = CAMERA_IRQ_IMG;
			DCAM_TRACE("path1 frame 0x%x, y uv, 0x%x 0x%x.\n",
				(int)&frame, frame.yaddr, frame.uaddr);
			if (user_func)
				(*user_func)(&frame, data);
		} else {
			DCAM_TRACE("path1_reserved_frame.\n");
			s_p_dcam_mod->
			    path_reserved_frame[DCAM_PATH1].pfinfo.iova[0] = 0;
			s_p_dcam_mod->
			    path_reserved_frame[DCAM_PATH1].pfinfo.iova[1] = 0;

		}
	}
}

static void    _dcam_path1_overflow(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_PATH1_OV];
	void                    *data = s_user_data[DCAM_PATH1_OV];
	struct dcam_path_desc   *path;
	struct camera_frame       frame;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	pr_info("_path1_overflow.\n");
	path = &s_p_dcam_mod->dcam_path1;
	_dcam_frame_dequeue(&path->frame_queue, &frame);

	if (user_func)
		(*user_func)(&frame, data);
}

static void    _dcam_path2_done(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	dcam_isr_func           user_func = s_user_func[DCAM_PATH2_DONE];
	void                    *data = s_user_data[DCAM_PATH2_DONE];
	struct dcam_path_desc   *path;
	struct camera_frame     frame;
	struct camera_frame     reserved_frame;

	if (atomic_read(&s_resize_flag)) {
		memset(&frame, 0, sizeof(struct camera_frame));
		if (user_func)
			(*user_func)(&frame, data);

		return;
	}
	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}
	path = &s_p_dcam_mod->dcam_path2;
	reserved_frame = s_p_dcam_mod->path_reserved_frame[DCAM_PATH2];

	DCAM_TRACE("PATH2, %d %d.\n", path->need_stop, path->need_wait);
	if (path->status == DCAM_ST_START) {
		if (path->need_stop) {
			dcam_glb_reg_awr(DCAM_CFG, ~BIT_2,
					 DCAM_CFG_REG);
			path->need_stop = 0;
		}
		_dcam_path_done_notice(DCAM_PATH_IDX_2);

		if (path->sof_cnt < 1) {
			pr_info("path2 done cnt %d.\n", path->sof_cnt);
			path->need_wait = 0;
			return;
		}
		path->sof_cnt = 0;

		if (path->need_wait) {
			path->need_wait = 0;
			return;
		}
		rtn = _dcam_frame_dequeue(&path->frame_queue,
						&frame);

		if (pfiommu_check_addr(&frame.pfinfo)) {
			pr_err("the frame has been broken!\n");
			return;
		}
		if (frame.pfinfo.dev == NULL)
			pr_info("DCAM%d done dev NULL %p\n",
				DCAM_PATH2, frame.pfinfo.dev);

		pfiommu_free_addr(&frame.pfinfo);

		if (rtn == 0 &&
		    frame.pfinfo.mfd[0] !=
			reserved_frame.pfinfo.mfd[0]
		    && dcam_frame_is_locked(&frame) != 0) {
			frame.width = path->output_size.w;
			frame.height = path->output_size.h;
			frame.irq_type = CAMERA_IRQ_IMG;
			if (user_func)
				(*user_func)(&frame, data);
		} else {
			DCAM_TRACE("path2_reserved_frame.\n");
			s_p_dcam_mod->
			    path_reserved_frame[DCAM_PATH2].pfinfo.iova[0] = 0;
			s_p_dcam_mod->
			    path_reserved_frame[DCAM_PATH2].pfinfo.iova[1] = 0;
		}
	}
}


static void    _dcam_path2_ov(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_PATH2_OV];
	void                    *data = s_user_data[DCAM_PATH2_OV];
	struct dcam_path_desc   *path;
	struct camera_frame       frame;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	pr_info("_path2_overflow.\n");
	path = &s_p_dcam_mod->dcam_path2;
	_dcam_frame_dequeue(&path->frame_queue, &frame);

	if (user_func)
		(*user_func)(&frame, data);
}

static void    _dcam_sensor_line_err(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_SN_LINE_ERR];
	void                    *data = s_user_data[DCAM_SN_LINE_ERR];

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	pr_info("_line_err.\n");

	if (user_func)
		(*user_func)(NULL, data);
}

static void    _dcam_sensor_frame_err(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_SN_FRAME_ERR];
	void                    *data = s_user_data[DCAM_SN_FRAME_ERR];

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	pr_info("_frame_err.\n");

	if (user_func)
		(*user_func)(NULL, data);
}

static void    _dcam_isp_ov(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_ISP_OV];
	void                    *data = s_user_data[DCAM_ISP_OV];

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	pr_info("_isp_overflow.\n");

	if (user_func)
		(*user_func)(NULL, data);
}

static void    _dcam_mipi_ov(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_MIPI_OV];
	void                    *data = s_user_data[DCAM_MIPI_OV];

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}


	if (user_func)
		(*user_func)(NULL, data);

	pr_info("_mipi_overflow.\n");
}

static void    _dcam_rot_done(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_ROT_DONE];
	void                    *data = s_user_data[DCAM_ROT_DONE];

	DCAM_TRACE("rot_done.\n");

	if (user_func)
		(*user_func)(NULL, data);
}

static void    _dcam_path0_sof(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	dcam_isr_func           user_func = s_user_func[DCAM_PATH0_SOF];
	void                    *data = s_user_data[DCAM_PATH0_SOF];
	struct dcam_path_desc   *path;

	DCAM_TRACE("path 0 sof 1.\n");

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}
	if (DCAM_ADDR_INVALID(s_dcam_sc_array)) {
		pr_err("zero pointer\n");
		return;
	}

	if (s_p_dcam_mod->dcam_path0.status == DCAM_ST_START) {
		DCAM_TRACE("path 0 sof 2.\n");
		path = &s_p_dcam_mod->dcam_path0;
		if (path->valid == 0) {
			pr_info("path0 not valid.\n");
			return;
		}

		if (path->sof_cnt > 0) {
			pr_info("path0 sof %d.\n", path->sof_cnt);
			return;
		}
		path->sof_cnt++;
		rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_0, false);
		if (path->is_update) {
			_dcam_path0_set();
			path->is_update = 0;
			DCAM_TRACE("path0 updated.\n");
			_dcam_auto_copy_ext(DCAM_PATH_IDX_0, true, true);
		} else {
			if (rtn)
				DCAM_TRACE("path0 updated.\n");
			else
				_dcam_auto_copy(DCAM_PATH_IDX_0);
		}

		_dcam_path_updated_notice(DCAM_PATH_IDX_0);

		if (rtn) {
			path->need_wait = 1;
			pr_info("path 0 wait.\n");
			return;
		}

		if (user_func)
			(*user_func)(NULL, data);
	}
}

static void    _dcam_path1_sof(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	enum dcam_drv_rtn       ret = DCAM_RTN_SUCCESS;
	dcam_isr_func           user_func = s_user_func[DCAM_PATH1_SOF];
	void                    *data = s_user_data[DCAM_PATH1_SOF];
	struct dcam_path_desc   *path;
	struct dcam_sc_coeff    *sc_coeff;

	DCAM_TRACE("path 1 sof.\n");

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}
	if (DCAM_ADDR_INVALID(s_dcam_sc_array)) {
		pr_err("zero pointer\n");
		return;
	}

	if (s_p_dcam_mod->dcam_path1.status == DCAM_ST_START) {

		path = &s_p_dcam_mod->dcam_path1;
		if (path->valid == 0) {
			pr_info("path1 not valid.\n");
			return;
		}

		if (path->sof_cnt > 0) {
			pr_info("path1 sof %d.\n", path->sof_cnt);
			return;
		}
		path->sof_cnt++;

		if (path->is_update) {
			rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_1, true);
			if (s_dcam_sc_array->is_smooth_zoom) {
				ret = _dcam_get_valid_sc_coeff(s_dcam_sc_array,
							       &sc_coeff);
				if (!ret) {
					_dcam_write_sc_coeff(DCAM_PATH_IDX_1);
					_dcam_path1_set(&sc_coeff->dcam_path1);
					_dcam_pop_sc_buf(s_dcam_sc_array,
							 &sc_coeff);
				}
			} else {
				rtn = _dcam_path_scaler(DCAM_PATH_IDX_1);
				if (rtn)
					pr_err("%s err, code %d", __func__,
					       rtn);
				_dcam_path1_set(path);
			}
			path->is_update = 0;
			DCAM_TRACE("path1 updated.\n");
			_dcam_auto_copy_ext(DCAM_PATH_IDX_1, true, true);
		} else {
			rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_1, false);
			if (rtn)
				DCAM_TRACE("path1 updated.\n");
			else
				_dcam_auto_copy(DCAM_PATH_IDX_1);
		}

		_dcam_path_updated_notice(DCAM_PATH_IDX_1);

		if (rtn) {
			path->need_wait = 1;
			pr_info("path 1 wait.\n");
			return;
		}
	}

	if (user_func)
		(*user_func)(NULL, data);
}

static void    _dcam_path2_sof(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	dcam_isr_func           user_func = s_user_func[DCAM_PATH2_SOF];
	void                    *data = s_user_data[DCAM_PATH2_SOF];
	struct dcam_path_desc   *path;

	DCAM_TRACE("path 2 sof.\n");

	if (atomic_read(&s_resize_flag)) {
		pr_info("path 2  sof, review now.\n");
	} else {
		if (DCAM_ADDR_INVALID(s_p_dcam_mod))
			return;

		if (s_p_dcam_mod->dcam_path2.status == DCAM_ST_START) {

			path = &s_p_dcam_mod->dcam_path2;
			if (path->valid == 0) {
				pr_info("path2 not valid.\n");
				return;
			}

			if (path->sof_cnt > 0) {
				pr_info("path2 sof %d.\n", path->sof_cnt);
				return;
			}
			path->sof_cnt++;

			if (path->is_update) {
				rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_2,
							      true);
				rtn = _dcam_path_scaler(DCAM_PATH_IDX_2);
				if (rtn)
					pr_err("%s err, code %d", __func__,
					       rtn);
				_dcam_path2_set();
				path->is_update = 0;
				DCAM_TRACE("path2 updated.\n");
				_dcam_auto_copy_ext(DCAM_PATH_IDX_2,
						    true, true);
			} else {
				rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_2,
							      false);
				if (rtn)
					DCAM_TRACE("path2 updated.\n");
				else
					_dcam_auto_copy(DCAM_PATH_IDX_2);
			}

			_dcam_path_updated_notice(DCAM_PATH_IDX_2);

			if (rtn) {
				path->need_wait = 1;
				pr_info("2 wait.\n");
				return;
			}
		}
	}

	if (user_func)
		(*user_func)(NULL, data);
}


static void    _dcam_stopped(void)
{
	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	DCAM_TRACE("stopped, %d.\n", s_p_dcam_mod->wait_stop);

	_dcam_path_done_notice(DCAM_PATH_IDX_0);
	_dcam_path_done_notice(DCAM_PATH_IDX_1);
	_dcam_path_done_notice(DCAM_PATH_IDX_2);
	_dcam_stopped_notice();
}

static int32_t    _dcam_err_pre_proc(void)
{
	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	DCAM_TRACE("state in err_pre_proc  0x%x,", s_p_dcam_mod->state);
	if (s_p_dcam_mod->state & DCAM_STATE_QUICKQUIT)
		return -1;

	s_p_dcam_mod->err_happened = 1;
	pr_info("err, 0x%x.\n", REG_RD(DCAM_INT_STS));

	csi_api_reg_trace();
	_dcam_reg_trace();
	print_isp_regs();
	dcam_stop(1);
	dcam_irq_clear(DCAM_IRQ_LINE_MASK);
	_dcam_stopped();
	return 0;
}

static int dcam_mmu_err_pre_proc(unsigned int irq_status)
{
	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	DCAM_TRACE("DCAM: mmu state in err_pre_proc 0x%x\n",
		s_p_dcam_mod->state);
	if (s_p_dcam_mod->state & DCAM_STATE_QUICKQUIT)
		return -1;

	s_p_dcam_mod->err_happened = 1;

	pr_info("DCAM: mmu err, 0x%x\n", irq_status);

	dcam_mmu_reg_trace();

	panic("fatal dcam iommu error!");

	return 0;
}


static const dcam_isr isr_list[DCAM_IRQ_NUMBER] = {
	NULL,/*_dcam_isp_root,*/
	_dcam_sensor_eof,
	_dcam_cap_sof,
	_dcam_cap_eof,
	_dcam_path0_done,

	_dcam_path0_overflow,
	_dcam_path1_done,
	_dcam_path1_overflow,
	_dcam_path2_done,
	_dcam_path2_ov,

	_dcam_sensor_line_err,
	_dcam_sensor_frame_err,
	NULL,
	_dcam_isp_ov,
	_dcam_mipi_ov,

	_dcam_rot_done,
	NULL, /*PDAF*/
	NULL, /*reserved*/
	_dcam_path0_sof,
	_dcam_path1_sof,

	_dcam_path2_sof,
	NULL,
	NULL,
	NULL,
};

static irqreturn_t _dcam_isr_root(int irq, void *dev_id)
{
	uint32_t                irq_line, status;
	unsigned long           flag;
	int32_t                 i;

	status = REG_RD(DCAM_INT_STS) & DCAM_IRQ_LINE_MASK;
	if (unlikely(status == 0))
		return IRQ_NONE;

	irq_line = status;
	if (unlikely(DCAM_MMU_IRQ_ERR_MASK & irq_line))
		if (dcam_mmu_err_pre_proc(irq_line))
			return IRQ_HANDLED;

	if (unlikely(DCAM_IRQ_ERR_MASK & status)) {
		if (_dcam_err_pre_proc())
			return IRQ_NONE;
	}

	REG_WR(DCAM_INT_CLR, status);

	spin_lock_irqsave(&dcam_lock, flag);

	for (i = 0; i < DCAM_IRQ_NUMBER; i++) {
		if (irq_line & (1 << (uint32_t)i)) {
			if (isr_list[i])
				isr_list[i]();
		}
		/*clear the interrupt flag*/
		irq_line &= ~(uint32_t)(1 << (uint32_t)i);
		if (!irq_line) /*no interrupt source left*/
			break;
	}

	spin_unlock_irqrestore(&dcam_lock, flag);

	return IRQ_HANDLED;
}

static void _dcam_wait_path_done(enum dcam_path_index path_index,
				uint32_t *p_flag)
{
	int                     ret = 0;
	struct dcam_path_desc   *p_path = NULL;
	unsigned long           flag;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	if (s_p_dcam_mod->err_happened)
		return;

	if (s_p_dcam_mod->dcam_mode == DCAM_CAPTURE_MODE_SINGLE)
		return;

	if (path_index == DCAM_PATH_IDX_0) {
		p_path = &s_p_dcam_mod->dcam_path0;
	} else if (path_index == DCAM_PATH_IDX_1) {
		p_path = &s_p_dcam_mod->dcam_path1;
	} else if (path_index == DCAM_PATH_IDX_2) {
		p_path = &s_p_dcam_mod->dcam_path2;
	} else {
		pr_info("Wrong index 0x%x.\n", path_index);
		return;
	}

	spin_lock_irqsave(&dcam_lock, flag);
	if (p_flag)
		*p_flag = 1;

	p_path->wait_for_done = 1;
	spin_unlock_irqrestore(&dcam_lock, flag);
	ret = wait_for_completion_timeout(&p_path->tx_done_com,
					DCAM_PATH_TIMEOUT);
	if (ret <= 0) {
		_dcam_reg_trace();
		pr_info("Failed to wait path 0x%x done.\n", path_index);
	}
}

static void _dcam_wait_update_done(enum dcam_path_index path_index,
				  uint32_t *p_flag)
{
	int                     ret = 0;
	struct dcam_path_desc   *p_path = NULL;
	unsigned long           flag;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return;
	}

	if (s_p_dcam_mod->err_happened)
		return;

	if (s_p_dcam_mod->dcam_mode == DCAM_CAPTURE_MODE_SINGLE)
		return;

	if (path_index == DCAM_PATH_IDX_0) {
		p_path = &s_p_dcam_mod->dcam_path0;
	} else if (path_index == DCAM_PATH_IDX_1) {
		p_path = &s_p_dcam_mod->dcam_path1;
	} else if (path_index == DCAM_PATH_IDX_2) {
		p_path = &s_p_dcam_mod->dcam_path2;
	} else {
		pr_info("Wrong index 0x%x.\n", path_index);
		return;
	}

	spin_lock_irqsave(&dcam_lock, flag);
	if (p_flag)
		*p_flag = 1;
	p_path->wait_for_sof = 1;
	spin_unlock_irqrestore(&dcam_lock, flag);
	ret = wait_for_completion_timeout(&p_path->sof_com, DCAM_PATH_TIMEOUT);
	if (ret <= 0) {
		_dcam_reg_trace();
		pr_info("Failed to wait update path 0x%x done.\n",
			path_index);
	}
}

static int32_t _dcam_path_check_deci(enum dcam_path_index path_index,
				    uint32_t *is_deci)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc   *path = NULL;
	uint32_t	w_deci = 0;
	uint32_t	h_deci = 0;
	uint32_t	d_max = DCAM_SC_COEFF_DOWN_MAX;
	uint32_t	u_max = DCAM_SC_COEFF_UP_MAX;
	uint32_t	f_max = DCAM_PATH_DECI_FAC_MAX;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	if (path_index == DCAM_PATH_IDX_1) {
		path = &s_p_dcam_mod->dcam_path1;
	} else if (path_index == DCAM_PATH_IDX_2) {
		path = &s_p_dcam_mod->dcam_path2;
	} else {
		rtn = DCAM_RTN_PATH_SC_ERR;
		goto dcam_path_err;
	}

	if (path->input_rect.w > (path->output_size.w * d_max * (1<<f_max)) ||
	    path->input_rect.h > (path->output_size.h * d_max * (1<<f_max)) ||
	    path->input_rect.w * u_max < path->output_size.w ||
	    path->input_rect.h * u_max < path->output_size.h) {
		rtn = DCAM_RTN_PATH_SC_ERR;
	} else {
		if (path->input_rect.w > path->output_size.w * d_max)
			w_deci = 1;
		if (path->input_rect.h > path->output_size.h * d_max)
			h_deci = 1;

		if (w_deci || h_deci)
			*is_deci = 1;
		else
			*is_deci = 0;
	}

dcam_path_err:
	DCAM_TRACE("path_check_deci: path_index=%d, is_deci=%d, rtn=%d.\n",
		    path_index, *is_deci, rtn);

	return rtn;
}

static int  _dcam_internal_init(void)
{
	int                     ret = 0;

	s_p_dcam_mod = vzalloc(sizeof(struct dcam_module));
	pr_info("s_p_dcam_mod: 0x%x", (unsigned int)s_p_dcam_mod);

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	init_completion(&s_p_dcam_mod->stop_com);
	init_completion(&s_p_dcam_mod->dcam_path0.tx_done_com);
	init_completion(&s_p_dcam_mod->dcam_path1.tx_done_com);
	init_completion(&s_p_dcam_mod->dcam_path2.tx_done_com);
	init_completion(&s_p_dcam_mod->dcam_path0.sof_com);
	init_completion(&s_p_dcam_mod->dcam_path1.sof_com);
	init_completion(&s_p_dcam_mod->dcam_path2.sof_com);
	init_completion(&s_p_dcam_mod->resize_done_com);
	init_completion(&s_p_dcam_mod->rotation_done_com);
	init_completion(&s_p_dcam_mod->scale_coeff_mem_com);
	complete(&s_p_dcam_mod->scale_coeff_mem_com);
	memset((void *)s_dcam_sc_array, 0, sizeof(struct dcam_sc_array));
	return ret;
}
static void _dcam_internal_deinit(void)
{
	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_info("Invalid addr, %p", s_p_dcam_mod);
	} else {
		vfree(s_p_dcam_mod);
		s_p_dcam_mod = NULL;
	}
}

int32_t dcam_module_init(enum dcam_cap_if_mode if_mode,
			enum dcam_cap_sensor_mode sn_mode,
			void *isp_dev_handle)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_cap_desc    *cap_desc = NULL;
	struct dcam_buf_queue   *p_buf_queue0 = NULL;
	struct dcam_buf_queue   *p_buf_queue1 = NULL;
	struct dcam_buf_queue   *p_buf_queue2 = NULL;

	if (if_mode >= DCAM_CAP_IF_MODE_MAX) {
		rtn = -DCAM_RTN_CAP_IF_MODE_ERR;
	} else {
		if (sn_mode >= DCAM_CAP_MODE_MAX) {
			rtn = -DCAM_RTN_CAP_SENSOR_MODE_ERR;
		} else {
			_dcam_internal_init();
			s_p_dcam_mod->isp_dev_handle = isp_dev_handle;
			dcam_irq_set(DCAM_IRQ_LINE_MASK);
			p_buf_queue0 = &(s_p_dcam_mod->dcam_path0.buf_queue);
			p_buf_queue1 = &(s_p_dcam_mod->dcam_path1.buf_queue);
			p_buf_queue2 = &(s_p_dcam_mod->dcam_path2.buf_queue);
			/*set default base frame index as 0*/
			_dcam_buf_queue_init(p_buf_queue0);
			_dcam_buf_queue_init(p_buf_queue1);
			_dcam_buf_queue_init(p_buf_queue2);
			cap_desc = &s_p_dcam_mod->dcam_cap;
			cap_desc->interface = if_mode;
			cap_desc->input_format = sn_mode;
			rtn = DCAM_RTN_SUCCESS;
		}
	}

	return -rtn;
}

int32_t dcam_module_deinit(enum dcam_cap_if_mode if_mode,
			   enum dcam_cap_sensor_mode sn_mode)
{
	if (atomic_read(&s_resize_flag) == 0)
		dcam_irq_clear(DCAM_IRQ_LINE_MASK);
	else
		dcam_irq_clear(DCAM_IRQ_LINE_MASK & ~PATH2_IRQ_LINE_MASK);
	s_p_dcam_mod->isp_dev_handle = NULL;
	_dcam_internal_deinit();

	return -DCAM_RTN_SUCCESS;
}

int sprd_dcam_drv_init(struct platform_device *pdev)
{
	int ret = 0;

	s_dcam_pdev = pdev;

	if (s_dcam_sc_array == NULL) {
		s_dcam_sc_array = vzalloc(sizeof(struct dcam_sc_array));
		if (s_dcam_sc_array == NULL) {
			ret = -ENOMEM;
		}
	}

	return ret;
}

int sprd_dcam_drv_deinit(void)
{
	if (s_dcam_sc_array) {
		vfree(s_dcam_sc_array);
		s_dcam_sc_array = NULL;
	}

	return 0;
}

struct platform_device *sprd_dcam_get_pdev(void)
{
	return s_dcam_pdev;
}

int32_t    dcam_module_en(struct device_node *dn)
{
	int ret = 0;

	DCAM_TRACE("dcam_module_en, In %d.\n", s_dcam_users.counter);

	mutex_lock(&dcam_module_sema);
/*TODO: Dcam PowerOn should be adjust and the wake_lock USAGE!
 */
	if (atomic_inc_return(&s_dcam_users) == 1) {
		dcam_wakelock = wakeup_source_create("Camera Sys Wakelock");
		wakeup_source_add(dcam_wakelock);

		__pm_stay_awake(dcam_wakelock);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
		sprd_cam_pw_on();
		sprd_cam_domain_eb();
#endif
		ret = dcam_enable_clk();
		if (ret) {
			pr_err("enable clk fail\n");
			return ret;
		}

		ret = request_irq(s_dcam_irq, _dcam_isr_root,
			  IRQF_SHARED, "DCAM",
			  (void *)&s_dcam_irq);
		if (ret) {
			pr_err("fail to install IRQ %d\n", ret);
			mutex_unlock(&dcam_module_sema);
			return ret;
		}
		atomic_set(&s_resize_flag, 0);
		atomic_set(&s_rotation_flag, 0);
		memset((void *)s_user_func, 0, sizeof(s_user_func));
		memset((void *)s_user_data, 0, sizeof(s_user_data));
		pr_info("register isr, 0x%x.\n", REG_RD(DCAM_INT_MASK));
		init_completion(&dcam_resizer_com);
		complete(&dcam_resizer_com);

		DCAM_TRACE("dcam_module_en end.\n");
	}
	DCAM_TRACE("dcam_module_en, Out %d.\n", s_dcam_users.counter);
	mutex_unlock(&dcam_module_sema);
	return 0;
#if 0
fail_exit:
	wake_unlock(&dcam_wakelock);
	wake_lock_destroy(&dcam_wakelock);
	atomic_dec(&s_dcam_users);
	mutex_unlock(&dcam_module_sema);
	return ret;
#endif
}

int32_t dcam_module_dis(struct device_node *dn)
{
	DCAM_TRACE("dcam_module_dis, In %d.\n", s_dcam_users.counter);

	if (atomic_read(&s_dcam_users) == 0)
		return DCAM_RTN_SUCCESS;

	mutex_lock(&dcam_module_sema);
	if (atomic_dec_return(&s_dcam_users) == 0) {
		dcam_disable_clk();
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
		sprd_cam_domain_disable();
		sprd_cam_pw_off();
#endif
		free_irq(s_dcam_irq, (void *)&s_dcam_irq);
		__pm_relax(dcam_wakelock);
		wakeup_source_remove(dcam_wakelock);
		wakeup_source_destroy(dcam_wakelock);
	}

	DCAM_TRACE("dcam_module_dis, Out %d.\n", s_dcam_users.counter);
	mutex_unlock(&dcam_module_sema);

	return DCAM_RTN_SUCCESS;
}

void dcam_irq_set(uint32_t int_mask)
{
	dcam_glb_reg_owr(DCAM_INT_CLR,
		int_mask, DCAM_INIT_CLR_REG);
	dcam_glb_reg_owr(DCAM_INT_MASK,
		int_mask, DCAM_INIT_MASK_REG);
}

void dcam_irq_clear(uint32_t int_mask)
{
	dcam_glb_reg_awr(DCAM_INT_MASK,
		~int_mask, DCAM_INIT_MASK_REG);
	dcam_glb_reg_owr(DCAM_INT_CLR,
		int_mask, DCAM_INIT_CLR_REG);
}

int32_t dcam_reset(enum dcam_rst_mode reset_mode)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	uint32_t                time_out = 0;

	DCAM_TRACE("reset: %d.\n", reset_mode);

	if (reset_mode == DCAM_RST_ALL) {
		if (atomic_read(&s_dcam_users)) {
			/* firstly, stop AXI write + read */
			dcam_glb_reg_owr(DCAM_AHBM_STS, BIT_6 | BIT_7,
					 DCAM_AHBM_STS_REG);
		}

		/* then wait for AHB busy cleared */
		while (++time_out < DCAM_AXI_STOP_TIMEOUT) {
			if ((REG_RD(DCAM_AHBM_STS) & BIT_0) == 0)
				break;
			udelay(10);
		}
		if (time_out >= DCAM_AXI_STOP_TIMEOUT) {
			pr_info("reset TO: %d.\n", time_out);
			return DCAM_RTN_TIMEOUT;
		}
	}
	pr_info("reset_mode %d\n", reset_mode);
	/* do reset action */
	switch (reset_mode) {
	case DCAM_RST_PATH0:
		regmap_update_bits(cam_ahb_gpr,
				REG_MM_AHB_AHB_RST,
				MASK_MM_AHB_DCAM_CAM0_SOFT_RST,
				MASK_MM_AHB_DCAM_CAM0_SOFT_RST);
		regmap_update_bits(cam_ahb_gpr,
				REG_MM_AHB_AHB_RST,
				MASK_MM_AHB_DCAM_CAM0_SOFT_RST,
				~(unsigned int)
				MASK_MM_AHB_DCAM_CAM0_SOFT_RST);
		DCAM_TRACE("reset path0.\n");
		break;

	case DCAM_RST_PATH1:
		regmap_update_bits(cam_ahb_gpr,
				REG_MM_AHB_AHB_RST,
				MASK_MM_AHB_DCAM_CAM1_SOFT_RST,
				MASK_MM_AHB_DCAM_CAM1_SOFT_RST);
		regmap_update_bits(cam_ahb_gpr,
				REG_MM_AHB_AHB_RST,
				MASK_MM_AHB_DCAM_CAM1_SOFT_RST,
				~(unsigned int)
				MASK_MM_AHB_DCAM_CAM1_SOFT_RST);
		DCAM_TRACE("reset path1.\n");
		break;

	case DCAM_RST_PATH2:
		regmap_update_bits(cam_ahb_gpr,
				REG_MM_AHB_AHB_RST,
				MASK_MM_AHB_DCAM_CAM2_SOFT_RST,
				MASK_MM_AHB_DCAM_CAM2_SOFT_RST);
		udelay(50);
		regmap_update_bits(cam_ahb_gpr,
				REG_MM_AHB_AHB_RST,
				MASK_MM_AHB_DCAM_CAM2_SOFT_RST,
				~(unsigned int)
				MASK_MM_AHB_DCAM_CAM2_SOFT_RST);
		DCAM_TRACE("reset path2.\n");
		break;

	case DCAM_RST_ALL:
		regmap_update_bits(cam_ahb_gpr,
				REG_MM_AHB_AHB_RST,
				MASK_MM_AHB_DCAM_SOFT_RST |
				MASK_MM_AHB_ISP_LOG_SOFT_RST|
				MASK_MM_AHB_ISP_CFG_SOFT_RST|
				MASK_MM_AHB_DCAM_CAM0_SOFT_RST |
				MASK_MM_AHB_DCAM_CAM1_SOFT_RST |
				MASK_MM_AHB_DCAM_CAM2_SOFT_RST |
				MASK_MM_AHB_AXI_CAM_MTX_SOFT_RST,
				MASK_MM_AHB_DCAM_SOFT_RST |
				MASK_MM_AHB_ISP_LOG_SOFT_RST|
				MASK_MM_AHB_ISP_CFG_SOFT_RST|
				MASK_MM_AHB_DCAM_CAM0_SOFT_RST |
				MASK_MM_AHB_DCAM_CAM1_SOFT_RST |
				MASK_MM_AHB_DCAM_CAM2_SOFT_RST |
				MASK_MM_AHB_AXI_CAM_MTX_SOFT_RST);
		udelay(10);
		regmap_update_bits(cam_ahb_gpr,
				REG_MM_AHB_AHB_RST,
				MASK_MM_AHB_DCAM_SOFT_RST |
				MASK_MM_AHB_ISP_LOG_SOFT_RST|
				MASK_MM_AHB_ISP_CFG_SOFT_RST|
				MASK_MM_AHB_DCAM_CAM0_SOFT_RST |
				MASK_MM_AHB_DCAM_CAM1_SOFT_RST |
				MASK_MM_AHB_DCAM_CAM2_SOFT_RST |
				MASK_MM_AHB_AXI_CAM_MTX_SOFT_RST,
				~(unsigned int)
				(MASK_MM_AHB_DCAM_SOFT_RST |
				MASK_MM_AHB_ISP_LOG_SOFT_RST|
				MASK_MM_AHB_ISP_CFG_SOFT_RST|
				MASK_MM_AHB_DCAM_CAM0_SOFT_RST |
				MASK_MM_AHB_DCAM_CAM1_SOFT_RST |
				MASK_MM_AHB_DCAM_CAM2_SOFT_RST |
				MASK_MM_AHB_AXI_CAM_MTX_SOFT_RST));
		pr_info("reset all.\n");
		sprd_iommu_restore(&s_dcam_pdev->dev);
		break;
	default:
		rtn = DCAM_RTN_PARA_ERR;
		break;
	}

	if (reset_mode == DCAM_RST_ALL) {
		if (atomic_read(&s_dcam_users)) {
			/* enable AXI read + write, path0 + path1 + path2  */
			dcam_glb_reg_awr(DCAM_AHBM_STS,
				 ~(BIT_3 | BIT_4 | BIT_5 | BIT_6 | BIT_7),
				 DCAM_AHBM_STS_REG);
		}
	}

	DCAM_TRACE("reset_mode=%x  end.\n", reset_mode);

	return -rtn;
}

int32_t dcam_update_path(enum dcam_path_index path_index,
			 struct camera_size *in_size,
			 struct camera_rect *in_rect,
			 struct camera_size *out_size)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	unsigned long           flags;
	uint32_t                is_deci = 0;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	if (DCAM_ADDR_INVALID(s_dcam_sc_array)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	DCAM_TRACE("update path.\n");
	DCAM_TRACE("path_index %d s_p_dcam_mod->dcam_path0.valid %d.\n",
		    path_index, s_p_dcam_mod->dcam_path0.valid);
	if ((DCAM_PATH_IDX_0 & path_index) && s_p_dcam_mod->dcam_path0.valid) {
		local_irq_save(flags);
		if (s_p_dcam_mod->dcam_path0.is_update) {
			local_irq_restore(flags);
			DCAM_TRACE("update path 0:  updating return.\n");
			return rtn;
		}
		local_irq_restore(flags);

		rtn = dcam_path_cfg(DCAM_PATH_IDX_0,
				DCAM_PATH_INPUT_SIZE, in_size);
		if (rtn) {
			pr_err("%s err, code %d", __func__, rtn);
			return -(rtn);
		}

		rtn = dcam_path_cfg(DCAM_PATH_IDX_0,
				DCAM_PATH_INPUT_RECT, in_rect);
		if (rtn) {
			pr_err("%s err, code %d", __func__, rtn);
			return -(rtn);
		}

		rtn = dcam_path_cfg(DCAM_PATH_IDX_0,
				DCAM_PATH_OUTPUT_SIZE, out_size);
		if (rtn) {
			pr_err("%s err, code %d", __func__, rtn);
			return -(rtn);
		}
		DCAM_TRACE("To update path0.\n");

		local_irq_save(flags);
		s_p_dcam_mod->dcam_path0.is_update = 1;
		local_irq_restore(flags);
	}

	if ((DCAM_PATH_IDX_1 & path_index) && s_p_dcam_mod->dcam_path1.valid) {
		rtn = dcam_path_cfg(DCAM_PATH_IDX_1,
				DCAM_PATH_INPUT_SIZE, in_size);
		if (rtn) {
			pr_err("%s err, code %d", __func__, rtn);
			return -(rtn);
		}

		rtn = dcam_path_cfg(DCAM_PATH_IDX_1,
				DCAM_PATH_INPUT_RECT, in_rect);
		if (rtn) {
			pr_err("%s err, code %d", __func__, rtn);
			return -(rtn);
		}

		rtn = dcam_path_cfg(DCAM_PATH_IDX_1,
				DCAM_PATH_OUTPUT_SIZE, out_size);
		if (rtn) {
			pr_err("%s err, code %d", __func__, rtn);
			return -(rtn);
		}

		rtn = _dcam_path_check_deci(DCAM_PATH_IDX_1, &is_deci);
		if (rtn) {
			pr_err("%s err, code %d", __func__, rtn);
			return -(rtn);
		}

		if (s_dcam_sc_array->is_smooth_zoom) {
			spin_lock_irqsave(&dcam_lock, flags);
			s_p_dcam_mod->dcam_path1.is_update = 1;
			spin_unlock_irqrestore(&dcam_lock, flags);
		} else {
			_dcam_wait_update_done(DCAM_PATH_IDX_1,
				&s_p_dcam_mod->dcam_path1.is_update);
		}
		if (!s_dcam_sc_array->is_smooth_zoom)
			_dcam_wait_update_done(DCAM_PATH_IDX_1, NULL);
	}

	if ((DCAM_PATH_IDX_2 & path_index) && s_p_dcam_mod->dcam_path2.valid) {
		local_irq_save(flags);
		if (s_p_dcam_mod->dcam_path2.is_update) {
			local_irq_restore(flags);
			DCAM_TRACE("update path 2:  updating return.\n");
			return rtn;
		}
		local_irq_restore(flags);

		rtn = dcam_path_cfg(DCAM_PATH_IDX_2,
				DCAM_PATH_INPUT_SIZE, in_size);
		if (rtn) {
			pr_err("%s err, code %d", __func__, rtn);
			return -(rtn);
		}
		rtn = dcam_path_cfg(DCAM_PATH_IDX_2,
				DCAM_PATH_INPUT_RECT, in_rect);
		if (rtn) {
			pr_err("%s err, code %d", __func__, rtn);
			return -(rtn);
		}
		rtn = dcam_path_cfg(DCAM_PATH_IDX_2,
				DCAM_PATH_OUTPUT_SIZE, out_size);
		if (rtn) {
			pr_err("%s err, code %d", __func__, rtn);
			return -(rtn);
		}

		local_irq_save(flags);
		s_p_dcam_mod->dcam_path2.is_update = 1;
		local_irq_restore(flags);
		_dcam_wait_update_done(DCAM_PATH_IDX_2, NULL);
	}

	DCAM_TRACE("dcam_update_path: done.\n");

	return -rtn;
}

int32_t dcam_start_path(enum dcam_path_index path_index)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	uint32_t                cap_en = 0;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}
	pr_info("%s: path %d, mode %x path 0,1,2 {%d %d %d}.\n",
		__func__,
		path_index,
		s_p_dcam_mod->dcam_mode,
		s_p_dcam_mod->dcam_path0.valid,
		s_p_dcam_mod->dcam_path1.valid,
		s_p_dcam_mod->dcam_path2.valid);

	/*aiden add: write arbit mode*/
	dcam_glb_reg_owr(DCAM_AHBM_STS, BIT_8, DCAM_AHBM_STS_REG);
	REG_WR(AXIM_AWQOS, 0x6f1);
	if (sprd_get_ver_id())
		REG_MWR(SPARE_REG_EN, BIT_0, 1);

	cap_en = REG_RD(DCAM_CONTROL) & BIT_2;
	DCAM_TRACE("cap_eb %d.\n", cap_en);
	if ((DCAM_PATH_IDX_0 & path_index) && s_p_dcam_mod->dcam_path0.valid) {
		_dcam_path0_set();
		rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_0, false);
		if (rtn) {
			pr_err("%s err, code %d", __func__, rtn);
			return rtn;
		}
		_dcam_force_copy(DCAM_PATH_IDX_0);
	}

	if ((DCAM_PATH_IDX_1 & path_index) && s_p_dcam_mod->dcam_path1.valid) {
		rtn = _dcam_path_scaler(DCAM_PATH_IDX_1);
		if (rtn) {
			pr_err("%s err, code %d", __func__, rtn);
			return -(rtn);
		}

		_dcam_path1_set(&s_p_dcam_mod->dcam_path1);
		DCAM_TRACE("start path: path_control=%x.\n",
							REG_RD(DCAM_CONTROL));
		rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_1, false);
		if (rtn) {
			pr_err("%s err, code %d", __func__, rtn);
			return -(rtn);
		}
		_dcam_force_copy_ext(DCAM_PATH_IDX_1, true, true);
		DCAM_TRACE("int= %x.\n", REG_RD(DCAM_INT_STS));
		dcam_glb_reg_owr(DCAM_CFG, BIT_1, DCAM_CFG_REG);
	}

	if ((DCAM_PATH_IDX_2 & path_index) && s_p_dcam_mod->dcam_path2.valid) {
		rtn = _dcam_path_scaler(DCAM_PATH_IDX_2);
		if (rtn) {
			pr_err("%s err, code %d", __func__, rtn);
			return rtn;
		}

		_dcam_path2_set();

		rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_2, false);
		if (rtn) {
			pr_err("%s err, code %d", __func__, rtn);
			return rtn;
		}
		_dcam_force_copy_ext(DCAM_PATH_IDX_2, true, true);
		/*TODO: kinlin  */
		if ((DCAM_PATH_IDX_1 & path_index) &&
		     s_p_dcam_mod->dcam_path1.valid) {
			if ((uint32_t)(s_p_dcam_mod->dcam_path1.output_size.w *
			    s_p_dcam_mod->dcam_path1.output_size.h) >=
			    (uint32_t)(s_p_dcam_mod->dcam_path2.output_size.w *
			    s_p_dcam_mod->dcam_path2.output_size.h)) {
				REG_WR(DCAM_BURST_GAP, 0x100000);
			}
		}
	}

	if ((DCAM_PATH_IDX_0 & path_index) && s_p_dcam_mod->dcam_path0.valid) {
		s_p_dcam_mod->dcam_path0.need_wait = 0;
		s_p_dcam_mod->dcam_path0.status = DCAM_ST_START;
		dcam_glb_reg_owr(DCAM_CFG, BIT_0, DCAM_CFG_REG);/*Enable Path0*/
		dcam_irq_set(PATH0_IRQ_LINE_MASK);
	}

	if ((DCAM_PATH_IDX_1 & path_index) && s_p_dcam_mod->dcam_path1.valid) {
		s_p_dcam_mod->dcam_path1.need_wait = 0;
		s_p_dcam_mod->dcam_path1.status = DCAM_ST_START;
		dcam_glb_reg_owr(DCAM_CFG, BIT_1, DCAM_CFG_REG);
		dcam_irq_set(PATH1_IRQ_LINE_MASK);
	}

	if ((DCAM_PATH_IDX_2 & path_index) && s_p_dcam_mod->dcam_path2.valid) {
		s_p_dcam_mod->dcam_path2.need_wait = 0;
		s_p_dcam_mod->dcam_path2.status = DCAM_ST_START;
		dcam_glb_reg_owr(DCAM_CFG, BIT_2, DCAM_CFG_REG);
		dcam_irq_set(PATH2_IRQ_LINE_MASK);
	}

	if (cap_en == 0) {
#if  0  /*def DCAM_DEBUG*/
		REG_MWR(CAP_MIPI_FRM_CTRL, BIT_5 | BIT_4, 1 << 4);
		REG_MWR(CAP_MIPI_FRM_CTRL, BIT_3|BIT_2|BIT_1|BIT_0, 0x07);
#endif
		/* Cap force copy */
		dcam_glb_reg_mwr(DCAM_CONTROL, BIT_0, 1 << 0, DCAM_CONTROL_REG);
		 /* Cap auto  copy */
		/*REG_MWR(DCAM_CONTROL, BIT_1, 1 << 1);*/
		 /* Cap Enable */
		dcam_glb_reg_mwr(DCAM_CONTROL, BIT_2, 1 << 2, DCAM_CONTROL_REG);
	}

	if (path_index != DCAM_PATH_IDX_ALL) {
		if ((DCAM_PATH_IDX_0 & path_index))
			_dcam_wait_path_done(DCAM_PATH_IDX_0, NULL);
		else if ((DCAM_PATH_IDX_1 & path_index))
			_dcam_wait_path_done(DCAM_PATH_IDX_1, NULL);
		else if ((DCAM_PATH_IDX_2 & path_index))
			_dcam_wait_path_done(DCAM_PATH_IDX_2, NULL);
	}

	_dcam_reg_trace();

	pr_info("%s end\n", __func__);
	return -rtn;
}

int32_t dcam_start(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	int                     ret = 0;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	ret = dcam_start_path(DCAM_PATH_IDX_ALL);
	return -rtn;
}

int32_t _dcam_stop_path(enum dcam_path_index path_index)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc   *p_path = NULL;
	unsigned long           flag;

	spin_lock_irqsave(&dcam_lock, flag);

	if (path_index == DCAM_PATH_IDX_0) {
		p_path = &s_p_dcam_mod->dcam_path0;
	} else if (path_index == DCAM_PATH_IDX_1) {
		p_path = &s_p_dcam_mod->dcam_path1;
	} else if (path_index == DCAM_PATH_IDX_2) {
		p_path = &s_p_dcam_mod->dcam_path2;
	} else {
		pr_info("stop path Wrong index 0x%x..\n", path_index);
		spin_unlock_irqrestore(&dcam_lock, flag);
		return -rtn;
	}
	if (0)
		_dcam_path_pause(path_index);

	_dcam_wait_for_quickstop(path_index);
	p_path->status = DCAM_ST_STOP;
	p_path->sof_cnt = 0;
	p_path->valid = 0;
	_dcam_frm_clear(path_index);

	if (path_index == DCAM_PATH_IDX_0)
		dcam_irq_clear(PATH0_IRQ_LINE_MASK);
	 else if (path_index == DCAM_PATH_IDX_1)
		dcam_irq_clear(PATH1_IRQ_LINE_MASK);
	 else if (path_index == DCAM_PATH_IDX_2)
		dcam_irq_clear(PATH2_IRQ_LINE_MASK);

	spin_unlock_irqrestore(&dcam_lock, flag);

	return rtn;
}

int32_t dcam_stop_path(enum dcam_path_index path_index)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;

	pr_info("%s start, 0x%x.\n", __func__, path_index);
	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	if (path_index < DCAM_PATH_IDX_0 ||
		path_index >= DCAM_PATH_IDX_ALL) {
		pr_info("error path_index.\n");
		return -rtn;
	}

	if ((DCAM_PATH_IDX_0 & path_index) && s_p_dcam_mod->dcam_path0.valid) {
		_dcam_wait_path_done(DCAM_PATH_IDX_0,
				     &s_p_dcam_mod->dcam_path0.need_stop);
		_dcam_stop_path(DCAM_PATH_IDX_0);
	}

	if ((DCAM_PATH_IDX_1 & path_index) && s_p_dcam_mod->dcam_path1.valid) {
		_dcam_wait_path_done(DCAM_PATH_IDX_1,
				     &s_p_dcam_mod->dcam_path1.need_stop);
		_dcam_stop_path(DCAM_PATH_IDX_1);
	}

	if ((DCAM_PATH_IDX_2 & path_index) && s_p_dcam_mod->dcam_path2.valid) {
		DCAM_TRACE("DCAM: stop path2 In.\n");
		_dcam_wait_path_done(DCAM_PATH_IDX_2,
				     &s_p_dcam_mod->dcam_path2.need_stop);
		_dcam_stop_path(DCAM_PATH_IDX_2);
	}

	_dcam_wait_path_stopped();
	pr_info("%s end, 0x%x.\n", __func__, path_index);

	return -rtn;
}

int32_t dcam_stop(unsigned char is_isr)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	unsigned long           flag;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	s_p_dcam_mod->state |= DCAM_STATE_QUICKQUIT;
	pr_info("dcam_stop In.\n");

	if (is_isr == 0)
		mutex_lock(&dcam_scale_sema);
	spin_lock_irqsave(&dcam_lock, flag);
	_dcam_wait_for_quickstop(DCAM_PATH_IDX_ALL);
	s_p_dcam_mod->dcam_path0.status = DCAM_ST_STOP;
	s_p_dcam_mod->dcam_path0.valid = 0;
	s_p_dcam_mod->dcam_path0.sof_cnt = 0;
	s_p_dcam_mod->dcam_path1.status = DCAM_ST_STOP;
	s_p_dcam_mod->dcam_path1.valid = 0;
	s_p_dcam_mod->dcam_path1.sof_cnt = 0;
	s_p_dcam_mod->dcam_path2.status = DCAM_ST_STOP;
	s_p_dcam_mod->dcam_path2.valid = 0;
	s_p_dcam_mod->dcam_path2.sof_cnt = 0;
	_dcam_frm_clear(DCAM_PATH_IDX_0);
	_dcam_frm_clear(DCAM_PATH_IDX_1);
	_dcam_frm_clear(DCAM_PATH_IDX_2);
	spin_unlock_irqrestore(&dcam_lock, flag);

	isp_axi_waiting();
	if (s_p_dcam_mod->isp_dev_handle)
		sprd_isp_unmap_buf(s_p_dcam_mod->isp_dev_handle);
	dcam_reset(DCAM_RST_ALL);
	s_p_dcam_mod->state &= ~DCAM_STATE_QUICKQUIT;
	if (is_isr == 0)
		mutex_unlock(&dcam_scale_sema);
	pr_info("dcam_stop Out.\n");

	return -rtn;
}

int32_t dcam_stop_sc_coeff(void)
{
	uint32_t zoom_mode;

	if (DCAM_ADDR_INVALID(s_dcam_sc_array)) {
		pr_err("zero pointer\n");
		return -EFAULT;
	}

	zoom_mode = s_dcam_sc_array->is_smooth_zoom;
	/*memset((void*)s_dcam_sc_array, 0, sizeof(struct dcam_sc_array));*/
	s_dcam_sc_array->is_smooth_zoom = zoom_mode;
	s_dcam_sc_array->valid_cnt = 0;
	memset(&s_dcam_sc_array->scaling_coeff_queue, 0,
		DCAM_SC_COEFF_BUF_COUNT*sizeof(struct dcam_sc_coeff *));

	return 0;
}

int sprd_get_ver_id(void)
{
	unsigned int chipid = 0;
	int ret = 0;

	ret = regmap_read(aon_apb_gpr, REG_AON_APB_AON_VER_ID, &chipid);
	if (ret < 0) {
		chipid = 0x1;
	}

	chipid = chipid & 0xff;
	if (chipid == 0x1)
		return 1;
	else
		return 0;
}

int sprd_dcam_parse_clk(struct platform_device *pdev)
{
#ifdef CONFIG_FPGA
	void __iomem *reg_base;

	reg_base = devm_ioremap_nocache(&pdev->dev, 0x60d00000,
					0x1000);
	if (IS_ERR(reg_base))
		return PTR_ERR(reg_base);
	s_dcam_ahbbase = (unsigned long)reg_base;
	pr_err("s_dcam_ahbbase 0x%lx\n", s_dcam_ahbbase);
	return 0;
#else

	dcam0_eb = devm_clk_get(&pdev->dev, "dcam_eb");
	if (IS_ERR(dcam0_eb)) {
		pr_err("failed to get dcam0_eb\n");
		return PTR_ERR(dcam0_eb);
	}

	dcam0_axi_eb = devm_clk_get(&pdev->dev, "dcam_axi_eb");
	if (IS_ERR(dcam0_axi_eb)) {
		pr_err("failed to get dcam0_axi_eb\n");
		return PTR_ERR(dcam0_axi_eb);
	}
	dcam0_clk = devm_clk_get(&pdev->dev, "dcam_clk");
	if (IS_ERR(dcam0_clk)) {
		pr_err("failed to get dcam_clk\n");
		return PTR_ERR(dcam0_clk);
	}

	dcam0_clk_parent = devm_clk_get(&pdev->dev, "dcam_clk_parent");
	if (IS_ERR(dcam0_clk_parent)) {
		pr_err("failed to get dcam_clk_parent\n");
		return PTR_ERR(dcam0_clk_parent);
	}

	dcam0_axi_clk = devm_clk_get(&pdev->dev, "dcam_axi_clk");
	if (IS_ERR(dcam0_axi_clk)) {
		pr_err("failed to get dcam_axi_clk\n");
		return PTR_ERR(dcam0_axi_clk);
	}

	dcam0_axi_clk_parent = devm_clk_get(&pdev->dev, "dcam_axi_clk_parent");
	if (IS_ERR(dcam0_axi_clk_parent)) {
		pr_err("failed to get dcam_axi_clk\n");
		return PTR_ERR(dcam0_axi_clk_parent);
	}

	dcam0_clk_default = clk_get_parent(dcam0_clk);
	if (IS_ERR(dcam0_clk_default)) {
		pr_err("failed to get dcam0_clk_default\n");
		return PTR_ERR(dcam0_clk_default);
	}

	dcam0_axi_clk_default = clk_get_parent(dcam0_axi_clk);
	if (IS_ERR(dcam0_axi_clk_default)) {
		pr_err("failed to get dcam0_axi_clk_default\n");
		return PTR_ERR(dcam0_axi_clk_default);
	}

	cam_ahb_gpr = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						"sprd,cam-ahb-syscon");
	if (IS_ERR(cam_ahb_gpr)) {
		pr_err("failed to get cam_ahb_gpr\n");
		return PTR_ERR(cam_ahb_gpr);
	}

	aon_apb_gpr = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
					"sprd,aon-apb-syscon");
	if (IS_ERR(aon_apb_gpr)) {
		pr_err("failed to get aon_apb_gpr\n");
		return PTR_ERR(aon_apb_gpr);
	}

	pr_info("dcam parse clk ok!");
	return 0;
#endif
}

int sprd_dcam_parse_irq(struct platform_device *pdev)
{
	int ret = 0, irq;

	irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (irq <= 0) {
		pr_err("failed to get IRQ\n");
		ret = -ENXIO;
		goto exit;
	}
	s_dcam_irq = irq;

exit:
	return ret;
}

int sprd_dcam_parse_regbase(struct platform_device *pdev)
{
	void __iomem *reg_base;
	struct resource *res = NULL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	reg_base = devm_ioremap_nocache(&pdev->dev, res->start,
					resource_size(res));
	if (IS_ERR(reg_base))
		return PTR_ERR(reg_base);
	s_dcam_regbase = (unsigned long)reg_base;
	pr_info("dcam regbase start 0x%x\n", res->start);
	return 0;
}
