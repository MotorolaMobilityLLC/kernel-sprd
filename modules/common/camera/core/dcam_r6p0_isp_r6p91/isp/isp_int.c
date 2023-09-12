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

#include <linux/interrupt.h>
#include <linux/version.h>
#include "isp_block.h"
#include "isp_int.h"
#include "isp_buf.h"
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <linux/ion.h>
#else
#include "ion.h"
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_INT %d : " fmt, __LINE__


static isp_isr_func p_user_func[ISP_IMG_MAX];
static void *p_user_data[ISP_IMG_MAX];
static spinlock_t irq_ch_lock;

static const unsigned int isp_ch0_irq_vect0[] = {
	ISP_INT_ISP_ALL_DONE,
	ISP_INT_SHADOW_DONE,
	ISP_INT_STORE_DONE,
	ISP_INT_AEM_DONE,
	ISP_INT_AEM_START,
	ISP_INT_AFM_RGB_DONE,
	ISP_INT_AFM_RGB_START,
	ISP_INT_BINNING_DONE,
	ISP_INT_BINNING_START,
	ISP_INT_AFL_DONE,
	ISP_INT_AFL_START,
	ISP_INT_DCAM_SOF,
	ISP_INT_HIST_DONE,
	ISP_INT_HIST_START,
};

static const unsigned int isp_ch0_irq_vect1[] = {
	ISP_INT_DISPATCH_BUF_FULL,
	ISP_INT_AWBM_ERR,
	ISP_INT_BINNING_ERR1,
	ISP_INT_BINNING_ERR0,
	ISP_INT_BPC_ERR2,
	ISP_INT_BPC_ERR1,
	ISP_INT_BPC_ERR0,
};

static const unsigned int isp_ch0_irq_vect2[] = {
	ISP_INT_AFL_NEW_DDR_START,
	ISP_INT_AFL_NEW_DDR_DONE,
	ISP_INT_AFL_NEW_SRAM_START,
	ISP_INT_AFL_NEW_SRAM_DONE,
};

int isp_set_next_statis_buf(struct isp_statis_module *module,
			    enum isp_3a_block_id block_index)
{
	int rtn = 0;
	struct isp_statis_frm_queue *statis_heap = NULL;
	struct isp_statis_buf_queue *p_buf_queue = NULL;
	struct isp_statis_buf *reserved_buf = NULL;
	struct isp_statis_buf node;

	pr_debug("block_id: %d\n", block_index);
	memset(&node, 0, sizeof(struct isp_statis_buf));
	if (block_index == ISP_AEM_BLOCK) {
		p_buf_queue = &module->aem_statis_queue;
		statis_heap = &module->aem_statis_frm_queue;
		reserved_buf = &module->aem_buf_reserved;
	} else if (block_index == ISP_AFL_BLOCK) {
		p_buf_queue = &module->afl_statis_queue;
		statis_heap = &module->afl_statis_frm_queue;
		reserved_buf = &module->afl_buf_reserved;
	} else if (block_index == ISP_AFM_BLOCK) {
		p_buf_queue = &module->afm_statis_queue;
		statis_heap = &module->afm_statis_frm_queue;
		reserved_buf = &module->afm_buf_reserved;
	} else if (block_index == ISP_BINNING_BLOCK) {
		p_buf_queue = &module->binning_statis_queue;
		statis_heap = &module->binning_statis_frm_queue;
		reserved_buf = &module->binning_buf_reserved;
	} else if (block_index == ISP_HIST_BLOCK) {
		p_buf_queue = &module->hist_statis_queue;
		statis_heap = &module->hist_statis_frm_queue;
		reserved_buf = &module->hist_buf_reserved;
	}

	/*read buf addr from in_buf_queue*/
	if (isp_statis_queue_read(p_buf_queue, &node) != 0) {
		pr_debug("block: %d, use reserved buf\n", block_index);
		/*use reserved buffer*/
		if (reserved_buf->pfinfo.mfd[0] == 0)
			pr_info("block: %d, reserved mfd is 0!\n", block_index);

		memcpy(&node, reserved_buf, sizeof(struct isp_statis_buf));
	}

	/*enqueue the statis buf into the array*/
	rtn = isp_statis_enqueue(statis_heap, &node);
	if (rtn) {
		pr_err("block: %d, fail to enqueue statis buf\n", block_index);
		return rtn;
	}
	/*update buf addr to isp ddr addr*/
	if (block_index == ISP_AEM_BLOCK) {
		ISP_REG_WR(ISP_AEM_SLICE_INIT_ADDR, node.phy_addr);
	} else if (block_index == ISP_AFL_BLOCK) {
		ISP_REG_WR(
			ISP_ANTI_FLICKER_NEW_DDR_INIT_ADDR, node.phy_addr);
	} else if (block_index == ISP_BINNING_BLOCK) {
		ISP_REG_WR(ISP_BINNING_MEM_ADDR, node.phy_addr);
	}

	return rtn;
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
	{0x1500, 0x50},
	{0x1600, 0x20},
	{0x1900, 0x30},
	{0x4600, 0x30},
};

static void dump_isp_regs(void)
{
	unsigned long addr_base = 0;
	unsigned int i = 0, j = 0;

	pr_info("begin to dump isp regs:\n");
	for (j = 0; j < ARRAY_SIZE(g_isp_reg_info); j++) {
		addr_base = g_isp_reg_info[j].base;
		for (i = 0; i < g_isp_reg_info[j].len/0x10; i++) {
			pr_info("0x%.8x: 0x%.8x 0x%.8x 0x%.8x 0x%.8x\n",
			(unsigned int)(0x60a00000 + addr_base + 0x10*i),
			ISP_REG_RD(addr_base + 0x10*i + 0x0),
			ISP_REG_RD(addr_base + 0x10*i + 0x4),
			ISP_REG_RD(addr_base + 0x10*i + 0x8),
			ISP_REG_RD(addr_base + 0x10*i + 0xc));
		}
	}
}

static int isp_err_pre_proc(void *isp_handle)
{
	if (!isp_handle) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	return 0;
}

static void isp_mmu_error(void *isp_handle)
{
	unsigned long addr = 0;

	if (!isp_handle) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	pr_info("ISP: mmu error Register list\n");

	for (addr = ISP_MMU_INT_EN; addr <= ISP_MMU_REV ; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(addr),
			ISP_REG_RD(addr + 4),
			ISP_REG_RD(addr + 8),
			ISP_REG_RD(addr + 12));
	}

	dump_isp_regs();
}

static void isp_irq_reg(enum isp_irq_id irq_id, void *param)
{
	isp_isr_func user_func;
	void *user_data;
	struct camera_frame *frame_info = NULL;

	user_func = p_user_func[irq_id];
	user_data = p_user_data[irq_id];

	frame_info = (struct camera_frame *)param;
	if (!frame_info)
		return;

	if (user_func)
		(*user_func)(frame_info, user_data);
}

static void isp_store_done(void *isp_handle)
{
	struct isp_pipe_dev *dev = NULL;
	struct camera_frame frame = {0};
	isp_isr_func user_func;
	void *data;
	struct isp_k_block *isp_k_param = NULL;

	if (!isp_handle) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	if (!dev->isp_k_param.is_raw_capture)
		return;
	user_func = p_user_func[ISP_STORE_DONE];
	data = p_user_data[ISP_STORE_DONE];

	frame.irq_type = CAMERA_IRQ_DONE;
	frame.irq_property = IRQ_RAW_CAP_DONE;

	if (user_func)
		(user_func)(&frame, data);

	isp_k_param = &dev->isp_k_param;
	dev->isp_k_param.is_raw_capture = 0;

	pfiommu_free_addr(&isp_k_param->fetch_pfinfo);
	memset(&isp_k_param->fetch_pfinfo, 0, sizeof(struct pfiommu_info));

	pfiommu_free_addr(&isp_k_param->store_pfinfo);
	memset(&isp_k_param->store_pfinfo, 0, sizeof(struct pfiommu_info));

	pr_debug("isp store done.\n");
}

static void isp_aem_start(void *isp_handle)
{
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_module *module = NULL;

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->statis_module_info;

	rtn = isp_set_next_statis_buf(module, ISP_AEM_BLOCK);
	if (rtn)
		pr_err("fail to set AEM next statis buf\n");
	isp_k_ae_shadow_ctrl();
}

static void isp_aem_done(void *isp_handle)
{
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_frm_queue *statis_heap = NULL;
	struct isp_statis_buf node;
	struct camera_frame frame_info;
	struct isp_statis_module *module = NULL;

	memset(&frame_info, 0x00, sizeof(frame_info));
	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->statis_module_info;
	statis_heap = &module->aem_statis_frm_queue;

	/*dequeue the statis buf from a array*/
	rtn = isp_statis_dequeue(statis_heap, &node);
	if (rtn) {
		pr_err("ISP:fail to dequeue AEM buf error\n");
		return;
	}

	pr_debug("aem_addr: 0x%x, reserved_addr:0x%x\n",
		 (unsigned int)node.phy_addr,
		 (unsigned int)module->aem_buf_reserved.phy_addr);
	if (node.phy_addr != module->aem_buf_reserved.phy_addr) {
		frame_info.buf_size = node.buf_size;
		memcpy(frame_info.pfinfo.mfd, node.pfinfo.mfd,
		       sizeof(unsigned int) * 3);
		frame_info.phy_addr = node.phy_addr;
		frame_info.vir_addr = node.vir_addr;
		frame_info.addr_offset = node.addr_offset;
		frame_info.irq_type = CAMERA_IRQ_STATIS;
		frame_info.irq_property = IRQ_AEM_STATIS;
		frame_info.frame_id = module->aem_statis_cnt;

		/*call_back func to write the buf addr to usr_buf_queue*/
		isp_irq_reg(ISP_AEM_DONE, &frame_info);
	}

	module->aem_statis_cnt++;
}

static int isp_get_afm_statistic(struct isp_statis_buf *node)
{
	int ret = 0;

	ret = isp_k_raw_afm_statistic_r6p9((char *)(node->kaddr[0]));
	return ret;
}

static void isp_afm_rgb_start(void *isp_handle)
{
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_module *module = NULL;

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->statis_module_info;

	rtn = isp_set_next_statis_buf(module, ISP_AFM_BLOCK);
	if (rtn)
		pr_err("fail to set AFM next statis buf\n");
	isp_k_af_shadow_ctrl();
}

static void isp_afm_rgb_done(void *isp_handle)
{
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_frm_queue *statis_heap = NULL;
	struct isp_statis_buf node;
	struct camera_frame frame_info;
	struct isp_statis_module *module = NULL;
	struct ion_buffer *ion_buf = NULL;

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->statis_module_info;
	statis_heap = &module->afm_statis_frm_queue;
	ion_buf = (struct ion_buffer *)module->img_statis_buf.pfinfo.buf[0];

	memset(&node, 0x00, sizeof(node));
	memset(&frame_info, 0x00, sizeof(frame_info));
	/*dequeue the statis buf from a array*/
	rtn = isp_statis_dequeue(statis_heap, &node);
	if (rtn) {
		pr_err("ISP:fail to dequeue AFM statis buf\n");
		return;
	}

	if (node.kaddr[0] &&
		node.phy_addr != module->afm_buf_reserved.phy_addr) {
		/*add some protect for afm buffer*/
		if (ion_buf && ion_buf->kmap_cnt > 0) {
			rtn = isp_get_afm_statistic(&node);

			frame_info.buf_size = node.buf_size;
			memcpy(frame_info.pfinfo.mfd, node.pfinfo.mfd,
			       sizeof(unsigned int) * 3);
			frame_info.phy_addr = node.phy_addr;
			frame_info.vir_addr = node.vir_addr;
			frame_info.kaddr[0] = node.kaddr[0];
			frame_info.kaddr[1] = node.kaddr[1];
			frame_info.addr_offset = node.addr_offset;
			frame_info.irq_type = CAMERA_IRQ_STATIS;
			frame_info.irq_property = IRQ_AFM_STATIS;
			frame_info.frame_id = module->afm_statis_cnt;

			/*call_back func to write the buf addr to usr_buf_queue*/
			isp_irq_reg(ISP_AFM_DONE, &frame_info);
		}
	}
	module->afm_statis_cnt++;
}

static void isp_binning_start(void *isp_handle)
{
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_module *module = NULL;

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->statis_module_info;

	rtn = isp_set_next_statis_buf(module, ISP_BINNING_BLOCK);
	if (rtn)
		pr_err("fail to set Binning next statis buf\n");
}

static void isp_binning_done(void *isp_handle)
{
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_frm_queue *statis_heap = NULL;
	struct isp_statis_buf node;
	struct camera_frame frame_info;
	struct isp_statis_module *module = NULL;

	memset(&frame_info, 0x00, sizeof(frame_info));

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->statis_module_info;
	statis_heap = &module->binning_statis_frm_queue;

	/*dequeue the statis buf from a array*/
	rtn = isp_statis_dequeue(statis_heap, &node);
	if (rtn) {
		pr_err("ISP:fail to dequeue statis buf\n");
		return;
	}

	pr_debug("binning_addr: 0x%x, reserved_addr:0x%x\n",
		 (unsigned int)node.phy_addr,
		 (unsigned int)module->binning_buf_reserved.phy_addr);
	/* TBD :should to determine whether or not to use the resverd buf */
	if (node.phy_addr != module->binning_buf_reserved.phy_addr) {
		frame_info.buf_size = node.buf_size;
		memcpy(frame_info.pfinfo.mfd, node.pfinfo.mfd,
			sizeof(unsigned int) * 3);
		frame_info.phy_addr = node.phy_addr;
		frame_info.vir_addr = node.vir_addr;
		frame_info.addr_offset = node.addr_offset;
		frame_info.irq_type = CAMERA_IRQ_STATIS;
		frame_info.irq_property = IRQ_BINNING_STATIS;
		frame_info.frame_id = module->binning_statis_cnt;

		/*call_back func to write the buf addr to usr_buf_queue*/
		isp_irq_reg(ISP_BINNING_DONE, &frame_info);
	}

	module->binning_statis_cnt++;
}

static int isp_get_hist_statistic(struct isp_statis_buf *node)
{
	int ret = 0;

	ret = isp_k_hist_statistic_r6p9((uint64_t *)(node->kaddr[0]));
	return ret;
}

static void isp_hist_start(void *isp_handle)
{
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_module *module = NULL;

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->statis_module_info;

	rtn = isp_set_next_statis_buf(module, ISP_HIST_BLOCK);
	if (rtn)
		pr_err("fail to set HIST next statis buf\n");
}

static void isp_hist_done(void *isp_handle)
{
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_frm_queue *statis_heap = NULL;
	struct isp_statis_buf node;
	struct camera_frame frame_info;
	struct isp_statis_module *module = NULL;
	struct ion_buffer *ion_buf = NULL;

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->statis_module_info;
	statis_heap = &module->hist_statis_frm_queue;
	ion_buf = (struct ion_buffer *)module->img_statis_buf.pfinfo.buf[0];

	memset(&node, 0x00, sizeof(node));
	memset(&frame_info, 0x00, sizeof(frame_info));
	/*dequeue the statis buf from a array*/
	rtn = isp_statis_dequeue(statis_heap, &node);
	if (rtn) {
		pr_err("ISP:fail to dequeue HIST statis buf\n");
		return;
	}

	if (node.kaddr[0] &&
	    node.phy_addr != module->hist_buf_reserved.phy_addr) {
		/*add some protect for hist buffer*/
		if (ion_buf && ion_buf->kmap_cnt > 0) {
			rtn = isp_get_hist_statistic(&node);

			frame_info.buf_size = node.buf_size;
			memcpy(frame_info.pfinfo.mfd, node.pfinfo.mfd,
			       sizeof(unsigned int) * 3);
			frame_info.phy_addr = node.phy_addr;
			frame_info.vir_addr = node.vir_addr;
			frame_info.kaddr[0] = node.kaddr[0];
			frame_info.kaddr[1] = node.kaddr[1];
			frame_info.addr_offset = node.addr_offset;
			frame_info.irq_type = CAMERA_IRQ_STATIS;
			frame_info.irq_property = IRQ_HIST_STATIS;
			frame_info.frame_id = module->hist_statis_cnt;

			/*call_back func to write the buf addr to usr_buf_queue*/
			isp_irq_reg(ISP_HIST_DONE, &frame_info);
		}
	}

	module->hist_statis_cnt++;
}

static void isp_afl_start(void *isp_handle)
{
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_module *module = NULL;

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->statis_module_info;

	rtn = isp_set_next_statis_buf(module, ISP_AFL_BLOCK);
	if (rtn)
		pr_err("fail to set AFL next statis buf\n");
	isp_k_afl_shadow_ctrl();
}

static void isp_afl_done(void *isp_handle)
{
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_frm_queue *statis_heap = NULL;
	struct isp_statis_buf node;
	struct camera_frame frame_info;
	struct isp_statis_module *module = NULL;

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->statis_module_info;
	statis_heap = &module->afl_statis_frm_queue;

	/*dequeue the statis buf from a array*/
	rtn = isp_statis_dequeue(statis_heap, &node);
	if (rtn) {
		pr_err("ISP: fail to dequeue AFL buf error\n");
		return;
	}

	if (node.phy_addr != module->afl_buf_reserved.phy_addr) {
		/*pike2 afl not support hw bypass, workaround*/
		ISP_REG_MWR(ISP_ANTI_FLICKER_NEW_PARAM0, BIT_0, 1);
		memset(&frame_info, 0x00, sizeof(frame_info));

		frame_info.buf_size = node.buf_size;
		memcpy(frame_info.pfinfo.mfd, node.pfinfo.mfd,
		       sizeof(unsigned int) * 3);
		frame_info.phy_addr = node.phy_addr;
		frame_info.vir_addr = node.vir_addr;
		frame_info.addr_offset = node.addr_offset;
		frame_info.irq_type = CAMERA_IRQ_STATIS;
		frame_info.irq_property = IRQ_AFL_STATIS;
		frame_info.frame_id = module->afl_statis_cnt;

		/*call_back func to write the buf addr to usr_buf_queue*/
		isp_irq_reg(ISP_AFL_DONE, &frame_info);
	}
	module->afl_statis_cnt++;
}

static void isp_dcam_sof(void *isp_handle)
{
	struct isp_pipe_dev *dev = NULL;
	struct camera_frame frame = {0};
	isp_isr_func user_func;
	void *data;
	struct isp_k_block *isp_k_param = NULL;

	if (!isp_handle) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	user_func = p_user_func[ISP_DCAM_SOF];
	data = p_user_data[ISP_DCAM_SOF];

	frame.irq_type = CAMERA_IRQ_DONE;
	frame.irq_property = IRQ_DCAM_SOF;
	/*frame.frame_id = dev->frame_id;*/
	/*dev->frame_id++;*/

	if (user_func)
		(user_func)(&frame, data);

	isp_k_param = &dev->isp_k_param;
	atomic_set(&isp_k_param->lsc_updated, 0);

	pr_debug("isp dcam sof.\n");
}

static isp_isr isp_ch0_isr_list[4][32] = {
	[0][ISP_INT_ISP_ALL_DONE] = NULL,
	[0][ISP_INT_SHADOW_DONE] = NULL,
	[0][ISP_INT_STORE_DONE] = isp_store_done,
	[0][ISP_INT_ISP_STRAT] = NULL,
	[0][ISP_INT_DCAM_BUF_FULL] = NULL,
	[0][ISP_INT_STORE_BUF_FULL] = NULL,
	[0][ISP_INT_ISP2DCAM_AFIFO_FULL] = NULL,
	[0][ISP_INT_LSC_LOAD] = NULL,
	[0][ISP_INT_AEM_START] = isp_aem_start,
	[0][ISP_INT_AEM_DONE] = isp_aem_done,
	[0][ISP_INT_AFM_RGB_START] = isp_afm_rgb_start,
	[0][ISP_INT_AFM_RGB_DONE] = isp_afm_rgb_done,
	[0][ISP_INT_BINNING_DONE] = isp_binning_done,
	[0][ISP_INT_BINNING_START] = isp_binning_start,
	[0][ISP_INT_AFL_START] = NULL,
	[0][ISP_INT_AFL_DONE] = NULL,
	[0][ISP_INT_DCAM_SOF] = isp_dcam_sof,
	[0][ISP_INT_DCAM_EOF] = NULL,
	[0][ISP_INT_HIST_START] = isp_hist_start,
	[0][ISP_INT_HIST_DONE] = isp_hist_done,
	[0][ISP_INT_HIST2_START] = NULL,
	[0][ISP_INT_HIST2_DONE] = NULL,
	[1][ISP_INT_BPC_ERR0] = NULL,
	[1][ISP_INT_BPC_ERR1] = NULL,
	[1][ISP_INT_BPC_ERR2] = NULL,
	[1][ISP_INT_BINNING_ERR0] = NULL,
	[1][ISP_INT_BINNING_ERR1] = NULL,
	[1][ISP_INT_AWBM_ERR] = NULL,
	[1][ISP_INT_DISPATCH_BUF_FULL] = NULL,
	[2][ISP_INT_AFL_NEW_SRAM_DONE] = NULL,
	[2][ISP_INT_AFL_NEW_SRAM_START] = NULL,
	[2][ISP_INT_AFL_NEW_DDR_DONE] = isp_afl_done,
	[2][ISP_INT_AFL_NEW_DDR_START] = isp_afl_start,
	[3][ISP_INT_YUV_DONE] = NULL,
	[3][ISP_INT_YUV_POSTCDN_DONE] = NULL,
	[3][ISP_INT_YUV_CDN_DONE] = NULL,
	[3][ISP_INT_YUV_EE_DONE] = NULL,
	[3][ISP_INT_YUV_NLM_DONE] = NULL,
	[3][ISP_INT_YUV_PRFUV_DONE] = NULL,
	[3][ISP_INT_YUV_PRFY_DONE] = NULL,
	[3][ISP_INT_YUV_START] = NULL,
	[3][ISP_INT_FRGB_CCE_DONE] = NULL,
	[3][ISP_INT_FRGB_CCE_START] = NULL,
	[3][ISP_INT_FRGB_DONE] = NULL,
	[3][ISP_INT_FRGB_START] = NULL,
	[3][ISP_INT_FRGB_CFCE_DONE] = NULL,
	[3][ISP_INT_FRGB_CFCE_START] = NULL,
	[3][ISP_INT_RRGB_DONE] = NULL,
	[3][ISP_INT_RRGB_BDN_DONE] = NULL,
	[3][ISP_INT_RRGB_PWD_DONE] = NULL,
	[3][ISP_INT_RRGB_BPC_DONE] = NULL,
	[3][ISP_INT_RRGB_LENS_DONE] = NULL,
	[3][ISP_INT_RRGB_NLM_DONE] = NULL,
	[3][ISP_INT_RRGB_START] = NULL,
};

static irqreturn_t isp_isr_root_ch0(int irq, void *priv)
{
	unsigned int irq_line[4] = {0};
	unsigned int j, k;
	unsigned int vect = 0;
	unsigned long flag;
	unsigned long base_addr = s_isp_regbase;
	unsigned int irq_numbers[4] = {0};
	unsigned int mmu_irq_line = 0;
	struct isp_pipe_dev *isp_handle = NULL;

	isp_handle = (struct isp_pipe_dev *)priv;

	if (!isp_handle) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	mmu_irq_line = REG_RD(base_addr + ISP_MMU_INT_RAW_STS) &
		ISP_MMU_IRQ_ERR_MASK;
	if (unlikely(mmu_irq_line)) {
		REG_WR(base_addr + ISP_MMU_INT_CLR, mmu_irq_line);
		if (unlikely(ISP_MMU_IRQ_ERR_MASK_R & mmu_irq_line)) {
			pr_err("isp mmu read error, INT:0x%x\n",
					mmu_irq_line);
			isp_mmu_error(isp_handle);
		}
		if (unlikely(ISP_MMU_IRQ_ERR_MASK_W & mmu_irq_line)) {
			pr_err("isp mmu write error, INT:0x%x\n",
					mmu_irq_line);
			isp_mmu_error(isp_handle);
			panic("fatal isp iommu error!");
		}

		return IRQ_HANDLED;
	}

	irq_numbers[0] = ARRAY_SIZE(isp_ch0_irq_vect0);
	irq_numbers[1] = ARRAY_SIZE(isp_ch0_irq_vect1);
	irq_numbers[2] = ARRAY_SIZE(isp_ch0_irq_vect2);

	irq_line[0] = REG_RD(base_addr + ISP_INT_INT0) & ISP_INT_LINE_MASK0;
	irq_line[1] = REG_RD(base_addr + ISP_INT_INT1) & ISP_INT_LINE_MASK1;
	irq_line[2] = REG_RD(base_addr + ISP_INT_INT2) & ISP_INT_LINE_MASK2;
	irq_line[3] = REG_RD(base_addr + ISP_INT_INT3) & ISP_INT_LINE_MASK3;

	if (unlikely(irq_line[0] == 0 && irq_line[1] == 0 && irq_line[2] == 0
		     && irq_line[3] == 0))
		return IRQ_NONE;
	/*clear the interrupt*/
	REG_WR(base_addr + ISP_INT_CLR0, irq_line[0]);
	REG_WR(base_addr + ISP_INT_CLR1, irq_line[1]);
	REG_WR(base_addr + ISP_INT_CLR2, irq_line[2]);
	REG_WR(base_addr + ISP_INT_CLR3, irq_line[3]);

	if (unlikely(ISP_IRQ_ERR_MASK0 & irq_line[0])
		|| unlikely(ISP_IRQ_ERR_MASK1 & irq_line[1])) {
		pr_err("isp irq error:0x%x, 0x%x,0x%x, 0x%x\n",
			irq_line[0], irq_line[1], irq_line[2], irq_line[3]);
		/*handle the error here*/
		if (isp_err_pre_proc(isp_handle))
			return IRQ_HANDLED;
	}

	/*spin_lock_irqsave protect the isr_func*/
	spin_lock_irqsave(&isp_mod_lock, flag);
	for (j = 0; j < 3; j++) {
		for (k = 0; k < irq_numbers[j]; k++) {
			if (j == 0)
				vect = isp_ch0_irq_vect0[k];
			else if (j == 1)
				vect = isp_ch0_irq_vect1[k];
			else if (j == 2)
				vect = isp_ch0_irq_vect2[k];
			if (irq_line[j] & (1 << (unsigned int)vect)) {
				if (isp_ch0_isr_list[j][vect])
					isp_ch0_isr_list[j][vect](isp_handle);
			}
			irq_line[j] &= ~(1 << (unsigned int)vect);
			if (!irq_line[j])
				break;
		}
	}
	/*spin_unlock_irqrestore*/
	spin_unlock_irqrestore(&isp_mod_lock, flag);

	return IRQ_HANDLED;
}

int isp_irq_request(struct device *p_dev, unsigned int irq,
	struct isp_pipe_dev *ispdev)
{
	int ret = 0;

	if (!p_dev || !irq || !ispdev) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	irq_ch_lock = __SPIN_LOCK_UNLOCKED(&irq_ch_lock);
	pr_debug("irq: %d\n", irq);

	ret = devm_request_irq(p_dev, irq, isp_isr_root_ch0,
		IRQF_SHARED, "ISP_CH0", (void *)ispdev);
	if (ret) {
		pr_err("fail to install IRQ ch0 %d\n", ret);
		goto exit;
	}

exit:
	return ret;
}

int isp_irq_free(unsigned int irq, struct isp_pipe_dev *ispdev)
{
	int ret = 0;

	if (!irq || !ispdev) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	irq_ch_lock = __SPIN_LOCK_UNLOCKED(&irq_ch_lock);

	free_irq(irq, (void *)ispdev);

	return ret;
}

int isp_irq_callback(enum isp_irq_id irq_id, isp_isr_func user_func,
			void *user_data)
{
	unsigned long flag;

	if (!user_data) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	spin_lock_irqsave(&isp_mod_lock, flag);
	p_user_func[irq_id] = user_func;
	p_user_data[irq_id] = user_data;
	spin_unlock_irqrestore(&isp_mod_lock, flag);

	return 0;
}
