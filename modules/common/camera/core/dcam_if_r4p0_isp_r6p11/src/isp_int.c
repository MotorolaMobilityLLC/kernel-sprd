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
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>

#include "cam_dbg.h"
#include "isp_int.h"
#include "isp_buf.h"
#include "isp_path.h"
#include "isp_block.h"
#include "isp_slw.h"
#include "isp_statis_buf.h"
#include "isp_reg.h"
#include "dcam_core.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <linux/ion.h>
#else
#include "ion.h"
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_INT: %d %s: "\
		fmt, __LINE__, __func__

static int s_int_cnt[ISP_ID_MAX][ISP_SCENE_NUM][INT_REG_SETS][INT_NUM_PER_SET];

static isp_isr_func p_user_func[ISP_ID_MAX][ISP_IMG_MAX];
static void *p_user_data[ISP_ID_MAX][ISP_IMG_MAX];
uint32_t int_reg_base[ISP_ID_MAX][ISP_SCENE_NUM] = {
	{
		ISP_P0_INT_BASE,
		ISP_C0_INT_BASE
	},
	{
		ISP_P1_INT_BASE,
		ISP_C1_INT_BASE
	}
};

/* strings pool for int source */
static const char * const int_source[] = {
	"CFG_CTX_P0",
	"CFG_CTX_P1",
	"CFG_CTX_C0",
	"CFG_CTX_C1"
};

static int isp_irq0_id_order[] = {
	ISP_INT_ISP_ALL_DONE,
	ISP_INT_SHADOW_DONE,
	ISP_INT_DISPATCH_SLICE_DONE,
	ISP_INT_ISP_START,
	ISP_INT_FETCH_RAW_DONE,
	ISP_INT_NULL0_5,
	ISP_INT_NULL0_6,
	ISP_INT_LENS_LOAD,
	ISP_INT_AEM_START,
	ISP_INT_AEM_SHADOW_DONE,
	ISP_INT_AEM_DONE,
	ISP_INT_AEM_ERROR,
	ISP_INT_AFL_START,
	ISP_INT_AFL_SHADOW_DONE,
	ISP_INT_AFL_DONE,
	ISP_INT_AFL_ERROR,
	ISP_INT_BINNING_START,
	ISP_INT_BINNING_SHADOW_DONE,
	ISP_INT_BINNING_DONE,
	ISP_INT_BINNING_ERROR,
	ISP_INT_NULL0_20,
	ISP_INT_NULL0_21,
	ISP_INT_NULL0_22,
	ISP_INT_NULL0_23,
	ISP_INT_NULL0_24,
	ISP_INT_NULL0_25,
	ISP_INT_NULL0_26,
	ISP_INT_NULL0_27,
	ISP_INT_DCAM_SOF,
	ISP_INT_DCAM_EOF,
	ISP_INT_STORE_DONE,
	ISP_INT_AFL_LAST_SOF,
	ISP_INT_NUMBER0,
};

static int isp_irq1_id_order[] = {
	ISP_INT_SHADOW_DONE_OUT, /* changed */
	ISP_INT_STORE_DONE_OUT, /* changed */
	ISP_INT_SHADOW_DONE_CAP_PRE, /* changed */
	ISP_INT_STORE_DONE_CAP_PRE, /* changed */
	ISP_INT_SHADOW_DONE_VID, /* changed */
	ISP_INT_STORE_DONE_VID, /* changed */
	ISP_INT_NULL1_6,
	ISP_INT_NULL1_7,
	ISP_INT_NULL1_8,
	ISP_INT_NULL1_9,
	ISP_INT_NULL1_10,
	ISP_INT_NULL1_11,
	ISP_INT_NULL1_12,
	ISP_INT_NULL1_13,
	ISP_INT_NULL1_14,
	ISP_INT_NULL1_15,
	ISP_INT_NULL1_16,
	ISP_INT_NULL1_17,
	ISP_INT_HIST2_SHADOW_DONE,
	ISP_INT_HIST_SHADOW_DONE,
	ISP_INT_HIST_START,
	ISP_INT_HIST_DONE,
	ISP_INT_HIST2_START,
	ISP_INT_HIST2_DONE,
	ISP_INT_BPC_STORE_MEM_DONE,
	ISP_INT_BPC_STORE_ERROR,
	ISP_INT_BPC_ERR2,
	ISP_INT_BPC_ERR1,
	ISP_INT_BPC_ERR0,
	ISP_INT_BPC_STORE_NOT_EMPTY_ERROR,
	ISP_INT_CFG_ERR,
	ISP_INT_NULL1_31,
	ISP_INT_NUMBER1,
};

static int isp_irq2_id_order[] = {
	ISP_INT_AFM_RGB_Win0,
	ISP_INT_AFM_RGB_Win1,
	ISP_INT_AFM_RGB_Win2,
	ISP_INT_AFM_RGB_Win3,
	ISP_INT_AFM_RGB_Win4,
	ISP_INT_AFM_RGB_Win5,
	ISP_INT_AFM_RGB_Win6,
	ISP_INT_AFM_RGB_Win7,
	ISP_INT_AFM_RGB_Win8,
	ISP_INT_AFM_RGB_Win9,
	ISP_INT_AFM_RGB_START,
	ISP_INT_AFM_RGB_DONE,
	ISP_INT_AFM_RGB_SHADOW_DONE,
	ISP_INT_NULL2_13,
	ISP_INT_NULL2_14,
	ISP_INT_NULL2_15,
	ISP_INT_NULL2_16,
	ISP_INT_NULL2_17,
	ISP_INT_NULL2_18,
	ISP_INT_NULL2_19,
	ISP_INT_NULL2_20,
	ISP_INT_FMCU_LOAD_DONE,
	ISP_INT_FMCU_CONFIG_DONE,
	ISP_INT_FMCU_SHADOW_DONE,
	ISP_INT_FMCU_CMD_X,
	ISP_INT_FMCU_TIMEOUT,
	ISP_INT_FMCU_CMD_ERROR,
	ISP_INT_FMCU_STOP_DONE,
	ISP_INT_NULL2_28,
	ISP_INT_NULL2_29,
	ISP_INT_NULL2_30,
	ISP_INT_NULL2_31,
	ISP_INT_NUMBER2,
};

static int isp_irq3_id_order[] = {
	ISP_INT_YUV_DONE,
	ISP_INT_YUV_START,
	ISP_INT_FRGB_DONE,
	ISP_INT_FRGB_START,
	ISP_INT_RRGB_DONE,
	ISP_INT_RRGB_START,
	ISP_INT_NUMBER3,
};

struct irq_reg irq_sets[INT_REG_SETS] = {
	[INT_REG_SET_0] = {
		.irq_status = 0,
		.offset  = {ISP_INT_EN0, ISP_INT_CLR0, ISP_INT_INT0},
		.msk_bmap = {ISP_MOD_PRE_EN0_MASK, ISP_MOD_CAP_EN0_MASK},
		.order = isp_irq0_id_order,
		.bits_idx_max = ARRAY_SIZE(isp_irq0_id_order),
	},
	[INT_REG_SET_1] = {
		.irq_status = 0,
		.offset  = {ISP_INT_EN1, ISP_INT_CLR1, ISP_INT_INT1},
		.msk_bmap = {ISP_MOD_PRE_EN1_MASK, ISP_MOD_CAP_EN1_MASK},
		.order = isp_irq1_id_order,
		.bits_idx_max = ARRAY_SIZE(isp_irq1_id_order),
	},
	[INT_REG_SET_2] = {
		.irq_status = 0,
		.offset  = {ISP_INT_EN2, ISP_INT_CLR2, ISP_INT_INT2},
		.msk_bmap = {ISP_MOD_PRE_EN2_MASK, ISP_MOD_CAP_EN2_MASK},
		.order = isp_irq2_id_order,
		.bits_idx_max = ARRAY_SIZE(isp_irq2_id_order),
	},
	[INT_REG_SET_3] = {
		.irq_status = 0,
		.offset  = {ISP_INT_EN3, ISP_INT_CLR3, ISP_INT_INT3},
		.msk_bmap = {ISP_MOD_PRE_EN3_MASK, ISP_MOD_CAP_EN3_MASK},
		.order = isp_irq3_id_order,
		.bits_idx_max = ARRAY_SIZE(isp_irq3_id_order),
	},
};

static void isp_path_updated_notice(struct isp_module *module,
				    enum isp_path_index path_index)
{
	struct isp_path_desc *p_path = NULL;
	enum isp_scl_id scl_path_id;
	struct isp_pipe_dev *dev = NULL;

	if (module == NULL) {
		pr_err("fail to get module,it's NULL\n");
		return;
	}
	dev = container_of(module, struct isp_pipe_dev, module_info);

	if (path_index >= ISP_PATH_IDX_PRE && path_index <= ISP_PATH_IDX_CAP) {
		scl_path_id = isp_get_scl_index(path_index);
		p_path = &module->isp_path[scl_path_id];
		if (p_path->wait_for_sof) {
			complete(&p_path->sof_com);
			p_path->wait_for_sof = 0;
		}
	} else {
		pr_err("ISP%d: fail to get path index 0x%x\n",
		       ISP_GET_IID(dev->com_idx), path_index);
		return;
	}
}

static void isp_path_done_notice(struct isp_module *module,
				 enum isp_path_index path_index)
{
	struct isp_path_desc *p_path = NULL;
	enum isp_scl_id scl_path_id;
	struct isp_pipe_dev *dev = NULL;

	if (module == NULL) {
		pr_err("fail to get module,it's NULL\n");
		return;
	}

	dev = container_of(module, struct isp_pipe_dev, module_info);

	if (path_index >= ISP_PATH_IDX_PRE && path_index <= ISP_PATH_IDX_CAP) {
		scl_path_id = isp_get_scl_index(path_index);
		p_path = &module->isp_path[scl_path_id];
		pr_debug("ISP%d: path done notice %d, %d\n",
			 ISP_GET_IID(dev->com_idx),
			 p_path->wait_for_done, p_path->tx_done_com.done);
		if (p_path->wait_for_done) {
			complete(&p_path->tx_done_com);
			pr_debug("release tx_done_com: %d\n",
				p_path->tx_done_com.done);
			p_path->wait_for_done = 0;
		}
	} else {
		pr_err("fail to get path index ISP%d:0x%x\n",
		       ISP_GET_IID(dev->com_idx), path_index);
		return;
	}
}

int isp_set_next_statis_buf(uint32_t com_idx,
			    struct isp_statis_module *module,
			    enum isp_3a_block_id block_index)
{
	int rtn = 0;
	struct isp_statis_frm_queue *statis_heap = NULL;
	struct isp_statis_buf_queue *p_buf_queue = NULL;
	struct isp_statis_buf *reserved_buf = NULL;
	struct isp_statis_buf node;

	memset(&node, 0, sizeof(struct isp_statis_buf));
	if (block_index == ISP_AFL_BLOCK) {
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
	} else {
		pr_err("fail to get valid statis block index %d\n", block_index);
		return -1;
	}

	/*read buf addr from in_buf_queue*/
	if (isp_statis_queue_read(p_buf_queue, &node) != 0) {
		pr_warn_ratelimited("statis NO free buf, block=%d\n",
			block_index);
		/*use reserved buffer*/
		if (reserved_buf->pfinfo.mfd[0] == 0)
			pr_info_ratelimited(
				"NO need to cfg statis buf prop %d\n",
				reserved_buf->buf_property);
		memcpy(&node, reserved_buf, sizeof(struct isp_statis_buf));
	}

	if (node.pfinfo.dev == NULL)
		pr_debug("dev is NULL.\n");

	/*enqueue the statis buf into the array*/
	rtn = isp_statis_enqueue(statis_heap, &node);
	if (rtn) {
		pr_err("fail to enqueue statis buf\n");
		return rtn;
	}

	pr_debug("phy_addr = %lx, index(%d), rtn=%d\n",
		node.phy_addr, block_index, rtn);

	/*update buf addr to isp ddr addr*/
	if (block_index == ISP_AEM_BLOCK) {
		ISP_REG_WR(com_idx, ISP_AEM_DDR_ADDR, node.phy_addr);
		ISP_REG_MWR(com_idx, ISP_AEM_CFG_READY, BIT_0, 1);
	} else if (block_index == ISP_AFL_BLOCK) {
		ISP_REG_WR(com_idx,
			ISP_ANTI_FLICKER_NEW_DDR_INIT_ADDR, node.phy_addr);

		ISP_REG_WR(com_idx, ISP_ANTI_FLICKER_NEW_REGION3,
			(node.phy_addr + node.buf_size / 2));

		ISP_REG_MWR(com_idx, ISP_ANTI_FLICKER_NEW_CFG_READY, BIT_0, 1);
	} else if (block_index == ISP_BINNING_BLOCK) {
		ISP_REG_WR(com_idx, ISP_BINNING_MEM_ADDR, node.phy_addr);
		ISP_REG_WR(com_idx, ISP_BINNING_CFG_READY, 1);
	}

	return rtn;
}

static void isp_irq_reg(enum isp_id iid, enum isp_irq_id irq_id, void *param)
{
	isp_isr_func user_func;
	void *user_data;
	struct camera_frame *frame_info = NULL;

	user_func = p_user_func[iid][irq_id];
	user_data = p_user_data[iid][irq_id];

	frame_info = (struct camera_frame *)param;
	if (!frame_info)
		return;

	if (user_func)
		(*user_func)(frame_info, user_data);
}

static void isp_afl_start(void *isp_handle)
{
	enum isp_drv_rtn rtn = 0;
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_module *module = NULL;

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->statis_module_info;
	if (module->afl_int_done != 1) {
		pr_debug("double start\n");
		return;
	}

	rtn = isp_set_next_statis_buf(dev->com_idx, module, ISP_AFL_BLOCK);
	if (rtn)
		pr_err("fail to set next afl statis buf,rtn %d\n", rtn);
	module->afl_int_done = 0;
}

static void isp_afl_done(void *isp_handle)
{
	enum isp_drv_rtn rtn = 0;
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_frm_queue *statis_heap = NULL;
	struct isp_statis_buf node;
	struct camera_frame frame_info;
	struct isp_statis_module *module = NULL;

	memset(&frame_info, 0x00, sizeof(frame_info));

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->statis_module_info;
	statis_heap = &module->afl_statis_frm_queue;
	module->afl_int_done = 1;

	/*dequeue the statis buf from a array*/
	rtn = isp_statis_dequeue(statis_heap, &node);
	if (rtn) {
		pr_err("fail to dequeue AFL buf\n");
		return;
	}

	if (node.phy_addr != module->afl_buf_reserved.phy_addr) {
		frame_info.buf_size = node.buf_size;
		memcpy(frame_info.pfinfo.mfd, node.pfinfo.mfd,
		       sizeof(uint32_t) * 3);
		frame_info.phy_addr = node.phy_addr;
		frame_info.vir_addr = node.vir_addr;
		frame_info.addr_offset = node.addr_offset;
		frame_info.irq_type = CAMERA_IRQ_STATIS;
		frame_info.irq_property = IRQ_AFL_STATIS;
		frame_info.frame_id = module->afl_statis_cnt;

		/*call_back func to write the buf addr to usr_buf_queue*/
		isp_irq_reg(ISP_GET_IID(dev->com_idx),
				ISP_AFL_DONE, &frame_info);
	}
	module->afl_statis_cnt++;
}

static void isp_afl_error(void *isp_handle)
{
	pr_err_ratelimited("fail to get stats INT\n");
}

static void isp_afm_rgb_start(void *isp_handle)
{
	enum isp_drv_rtn rtn = 0;
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_module *module = NULL;

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->statis_module_info;

	rtn = isp_set_next_statis_buf(dev->com_idx, module, ISP_AFM_BLOCK);
	if (rtn)
		pr_err("fail to set next afm buf rtn %d\n", rtn);
}

static int isp_get_afm_statistic(uint32_t com_idx, struct isp_statis_buf *node)
{
	int ret = 0;
	int i = 0;
	int max_item = ISP_AFM_WIN_NUM * 3;
	unsigned long FV0_LOW = ISP_RGB_AFM_ENHANCE_FV0_0_BUF0_LOW;
	uint32_t *afm_statis = NULL;
	enum isp_id iid = ISP_ID_0;

#ifdef CONFIG_64BIT
	afm_statis =
		(uint32_t *)(((unsigned long)node->kaddr[0]) |
		((unsigned long)(node->kaddr[1] << 32)));
#else
	afm_statis = (uint32_t *)(node->kaddr[0]);
#endif

	if (!afm_statis) {
		ret = -1;
		pr_err("fail to alloc memory\n");
		return ret;
	}

	iid = ISP_GET_IID(com_idx);
	FV0_LOW = (iid == ISP_ID_0
			? ISP_RGB_AFM_ENHANCE_FV0_0_BUF0_LOW
			: ISP_RGB_AFM_ENHANCE_FV0_0_BUF1_LOW);

	for (i = 0; i < max_item; i++)
		afm_statis[i] = ISP_HREG_RD(com_idx, FV0_LOW + i * 4);

	return ret;
}

/*
 * need log and timestamp to debug
 * depend on ALG
static void isp_afm_get_timestamp(void *isp_handle, struct camera_frame *frame)
{
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_module *module = NULL;
	struct timespec ts;

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->statis_module_info;
	ktime_get_ts(&ts);

	frame->time = ts.tv_sec * 1000000L + ts.tv_nsec / NSEC_PER_USEC;
	pr_info("afm id %04d time %012ld\n",
		module->afm_statis_cnt,
		frame->time);
}
*/

static void isp_afm_rgb_done(void *isp_handle)
{
	enum isp_drv_rtn rtn = 0;
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_frm_queue *statis_heap = NULL;
	struct isp_statis_buf node;
	struct camera_frame frame_info;
	struct isp_statis_module *module = NULL;

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->statis_module_info;
	statis_heap = &module->afm_statis_frm_queue;

	memset(&node, 0x00, sizeof(node));
	memset(&frame_info, 0x00, sizeof(frame_info));

	rtn = isp_statis_dequeue(statis_heap, &node);
	if (rtn) {
		pr_err("fail to dequeue AFM statis buf\n");
		return;
	}

	if (node.phy_addr != module->afm_buf_reserved.phy_addr) {
		rtn = isp_get_afm_statistic(dev->com_idx, &node);

		frame_info.buf_size = node.buf_size;
		memcpy(frame_info.pfinfo.mfd, node.pfinfo.mfd,
		       sizeof(uint32_t) * 3);
		frame_info.phy_addr = node.phy_addr;
		frame_info.vir_addr = node.vir_addr;
		frame_info.addr_offset = node.addr_offset;
		frame_info.kaddr[0] = node.kaddr[0];
		frame_info.kaddr[1] = node.kaddr[1];
		frame_info.irq_type = CAMERA_IRQ_STATIS;
		frame_info.irq_property = IRQ_AFM_STATIS;
		frame_info.frame_id = module->afm_statis_cnt;
		frame_info.frame_invalid = 1;

		/* isp_afm_get_timestamp(dev, &frame_info); */
		module->afm_frame_info = frame_info;
	} else {
		module->afm_frame_info.frame_invalid = -1;
	}
	module->afm_statis_cnt++;
}

static void isp_binning_start(void *isp_handle)
{
	enum isp_drv_rtn rtn = 0;
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_module *module = NULL;

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->statis_module_info;
	if (module->binning_int_done != 1) {
		pr_debug("double start\n");
		return;
	}

	rtn = isp_set_next_statis_buf(dev->com_idx, module, ISP_BINNING_BLOCK);
	if (rtn)
		pr_err("fail to set next binning statis buf rtn %d\n", rtn);
	module->binning_int_done = 0;
}

static void isp_binning_done(void *isp_handle)
{
	enum isp_drv_rtn rtn = 0;
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_frm_queue *statis_heap = NULL;
	struct isp_statis_buf node;
	struct camera_frame frame_info;
	struct isp_statis_module *module = NULL;

	memset(&frame_info, 0x00, sizeof(frame_info));

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->statis_module_info;
	statis_heap = &module->binning_statis_frm_queue;
	module->binning_int_done = 1;

	/*dequeue the statis buf from a array*/
	rtn = isp_statis_dequeue(statis_heap, &node);
	if (rtn) {
		pr_err("fail to dequeue binning statis buf\n");
		return;
	}

	/* TBD :should to determine whether or not to use the resverd buf */
	if (node.phy_addr != module->binning_buf_reserved.phy_addr) {
		frame_info.buf_size = node.buf_size;
		memcpy(frame_info.pfinfo.mfd, node.pfinfo.mfd,
			sizeof(uint32_t) * 3);
		frame_info.phy_addr = node.phy_addr;
		frame_info.vir_addr = node.vir_addr;
		frame_info.addr_offset = node.addr_offset;
		frame_info.irq_type = CAMERA_IRQ_STATIS;
		frame_info.irq_property = IRQ_BINNING_STATIS;
		frame_info.frame_id = module->binning_statis_cnt;

		/*call_back func to write the buf addr to usr_buf_queue*/
		isp_irq_reg(ISP_GET_IID(dev->com_idx),
				ISP_BINNING_DONE, &frame_info);
	}
	module->binning_statis_cnt++;
}

static void isp_binning_error(void *isp_handle)
{
	pr_info("fail to get stats INT\n");
}

void isp_fmcu_slw_shadow(enum isp_scl_id path_id, void *isp_handle)
{
	uint32_t idx;
	enum isp_slw_rtn rtn = 0;
	struct isp_pipe_dev *dev = NULL;

	if (!isp_handle) {
		pr_err("fail to get isp_handle ptr, it's NULL\n");
		return;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	idx = dev->com_idx;

	if (dev->fmcu_slw.status == ISP_ST_STOP) {
		pr_debug("fmcu_slw ISP_ST_STOP.\n");
		return;
	}

	if (get_slw_status(dev) == ISP_ST_START) {
		rtn = set_isp_fmcu_slw_cmd(isp_handle,
			ISP_SCL_VID, 0);
		if (unlikely(rtn)) {
			pr_err("fail to set fmcu slw cmd,rtn %d\n", rtn);
			return;
		}
	}
	rtn = set_isp_fmcu_cmd_reg(path_id, isp_handle);
	if (rtn) {
		pr_err("fail to set fmcu cmd reg,rtn %d\n", rtn);
		return;
	}

	ISP_REG_MWR(idx, ISP_FMCU_CMD_READY, BIT_0, 1);
}

void isp_fmcu_slw_config(enum isp_scl_id path_id, void *isp_handle)
{
	/*
	 * Remove it because slow motion using CFG mode now. Notice
	 * that current stack frame in this function is exceed kernel
	 * default limit since isp_slw_info contains large frame queue.
	 */
#if 0
	uint32_t i = 0;
	enum isp_slw_rtn rtn = 0;
	void *data;
	isp_isr_func user_func;
	enum isp_irq_id img_id;
	struct isp_slw_info p_from_fmcu;
	struct camera_frame frame;
	struct isp_pipe_dev *dev = NULL;
	struct isp_module *module = NULL;
	struct isp_path_desc *path = NULL;
	struct isp_fmcu_slw_desc *fmcu_slw = NULL;
	struct isp_fmcu_slw_info *slw_handle = NULL;
	struct camera_frame *reserved_frame = NULL;
	enum isp_id iid;

	if (!isp_handle) {
		pr_info("invalid img_id %d\n.", path_id);
		return;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	iid = ISP_GET_IID(dev->com_idx);
	fmcu_slw = &dev->fmcu_slw;
	slw_handle = fmcu_slw->slw_handle;
	module = &dev->module_info;
	path = &module->isp_path[path_id];

	if (dev->fmcu_slw.status == ISP_ST_STOP) {
		pr_debug("fmcu_slw ISP_ST_STOP.\n");
		return;
	}

	if (path_id == ISP_SCL_VID)
		img_id = ISP_PATH_VID_DONE;
	else
		return;

	user_func = p_user_func[iid][img_id];
	data = p_user_data[iid][img_id];

	rtn = slowmotion_frame_dequeue(&slw_handle->insert_queue, &p_from_fmcu);
	if (rtn)
		return;

	reserved_frame = &module->path_reserved_frame[path_id];
	if (likely(p_from_fmcu.is_reserved == 0)) {
		for (i = 0; i < ISP_SLW_FRM_NUM; i++) {
			rtn = isp_frame_dequeue(&p_from_fmcu.slw_queue, &frame);
			if (rtn)
				break;
			if (frame.pfinfo.mfd[0] ==
					reserved_frame->pfinfo.mfd[0])
				continue;

			frame.width = path->dst.w;
			frame.height = path->dst.h;
			frame.irq_type = CAMERA_IRQ_IMG;
			pr_debug("ISP: path%d frame %p mfd=%d\n",
				   path_id, &frame, frame.pfinfo.mfd[0]);
			if (user_func)
				(*user_func)(&frame, data);
		}
		p_from_fmcu.fmcu_num = 0;
		slowmotion_frame_enqueue(&slw_handle->empty_queue,
					&p_from_fmcu);
	} else {
		pr_info("isp: use reserved [%d]\n", path_id);
	}
#endif
}

static void isp_path_done(enum isp_scl_id path_id, void *isp_handle)
{
	int  ret = 0;
	enum isp_irq_id img_id;
	struct camera_frame frame;
	struct camera_frame *tmp_frame;
	struct isp_path_desc *path;
	enum isp_path_index path_idx = 0;
	isp_isr_func user_func;
	void *data;
	struct isp_pipe_dev *dev = NULL;
	struct isp_module *module = NULL;
	struct isp_fmcu_slw_desc *fmcu_slw = NULL;
	struct isp_fmcu_slice_desc *fmcu_slice = NULL;
	struct offline_buf_desc *buf_desc = NULL;
	enum isp_id iid;
	enum isp_scene_id sid;
	struct frm_timestamp sof_ts;

	if (!isp_handle) {
		pr_err("fail to get isp handle it's NULL.\n");
		return;
	}

	if (path_id >= ISP_SCL_MAX) {
		pr_err("fail to get invalid path_id %d\n.", path_id);
		return;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	iid = ISP_GET_IID(dev->com_idx);
	sid = ISP_GET_SID(dev->com_idx);
	module = &dev->module_info;
	path = &module->isp_path[path_id];
	fmcu_slw = &dev->fmcu_slw;
	fmcu_slice = &dev->fmcu_slice;
	buf_desc = isp_offline_sel_buf(&module->off_desc, ISP_OFF_BUF_BIN);

	if (sid == ISP_SCENE_CAP) {
		pr_debug("just return for cap\n");
		return;
	}

	if (path->valid == 0) {
		pr_info("isp path%d not valid\n", path_id);
		return;
	}

	if (path_id == ISP_SCL_PRE) {
		path_idx = ISP_PATH_IDX_PRE;
		img_id = ISP_PATH_PRE_DONE;
	} else if (path_id == ISP_SCL_VID) {
		path_idx = ISP_PATH_IDX_VID;
		img_id = ISP_PATH_VID_DONE;
	} else if (path_id == ISP_SCL_CAP) {
		path_idx = ISP_PATH_IDX_CAP;
		img_id = ISP_PATH_CAP_DONE;
	} else {
		pr_err("fail to get the isp path id %d\n", path_id);
		return;
	}

	user_func = p_user_func[iid][img_id];
	data = p_user_data[iid][img_id];

	if (path->need_stop)
		path->need_stop = 0;

	isp_path_done_notice(module, path_idx);

	if (dev->fmcu_slw.status == ISP_ST_START &&
	    path_id == ISP_SCL_VID) {
		pr_info("fmcu_slw ISP_ST_START.\n");
		return;
	}

	if (path->need_wait) {
		pr_info("isp wait.\n");
		path->need_wait = 0;
	} else {
		if (path->shadow_done_cnt < 1
		    && !fmcu_slw->slw_flags
		    && path_id != ISP_SCL_CAP) {
			pr_info("path%d shadow_done_cnt %d\n",
				path_id, path->shadow_done_cnt);
			return;
		}
		path->shadow_done_cnt = 0;

		/* release offline buffer */
		tmp_frame = &dev->offline_frame[ISP_OFF_BUF_BIN];

		sof_ts = tmp_frame->sof_ts;

		if (tmp_frame->yaddr_vir) {
			ret = isp_buf_queue_write(&buf_desc->tmp_buf_queue,
						  tmp_frame);
			if (ret) {
				pr_err("fail to retrieve off buf bin_path\n");
				return;
			}
			buf_desc->output_frame_count++;
			tmp_frame->yaddr_vir = 0;
		}

		if (dev->is_yuv_sn == true) {
			/* release offline buffer */
			buf_desc = isp_offline_sel_buf(&module->off_desc,
						       ISP_OFF_BUF_FULL);
			tmp_frame = &dev->offline_frame[ISP_OFF_BUF_FULL];
			if (tmp_frame->yaddr_vir) {
				ret = isp_buf_queue_write(&buf_desc->tmp_buf_queue,
							  tmp_frame);
				if (ret) {
					pr_err("fail to retrieve off buf bin_path\n");
					return;
				}
				buf_desc->output_frame_count++;
				tmp_frame->yaddr_vir = 0;
			}
		}

		ret = isp_frame_dequeue(&path->frame_queue, &frame);
		if (ret) {
			pr_err("fail to dequeue frame path id %d\n", path_id);
			return;
		}

		gen_frm_timestamp(&frame.btu_ts);

		if (frame.pfinfo.dev == NULL)
			pr_info("ISP done dev NULL\n");

		if (path_id == ISP_SCL_PRE || path_id == ISP_SCL_CAP)
			ret = pfiommu_free_addr_with_id(&frame.pfinfo,
						  ISP_IOMMU_CH_AW,
						  AW_ID_STORE_PRE_CAP_YUV);
		else if (path_id == ISP_SCL_VID)
			ret = pfiommu_free_addr_with_id(&frame.pfinfo,
						  ISP_IOMMU_CH_AW,
						  AW_ID_STORE_VID_YUV);
		else
			pr_err("fail to get unexpected path id %d\n", path_id);

		if (ret && (path_id > ISP_SCL_0) && (path_id < ISP_SCL_MAX))
			pr_err("fail to free frame, path id %d\n", path_id);

		if (frame.pfinfo.mfd[0] !=
		    module->path_reserved_frame[path_id].pfinfo.mfd[0]) {
			frame.width = path->dst.w;
			frame.height = path->dst.h;
			frame.sof_ts = sof_ts;
			frame.irq_type = CAMERA_IRQ_IMG;
			pr_debug("ISP%d path%d done, frame %p, mfd[0] 0x%x\n",
				 iid, path_id, &frame, frame.pfinfo.mfd[0]);
			if (user_func)
				(*user_func)(&frame, data);
		} else {
			pr_info_ratelimited("isp%d: use reserved [%d]\n",
					    iid, path_id);
			module->path_reserved_frame[path_id].pfinfo.iova[0] = 0;
			module->path_reserved_frame[path_id].pfinfo.iova[1] = 0;
		}
	}

	isp_dbg_reg_trace(dev, dev->com_idx);

	pr_debug("sprd_isp path done.\n");
}

static void isp_path_shadow_done(enum isp_scl_id path_id, void *isp_handle)
{
	enum isp_drv_rtn rtn = 0;
	struct isp_path_desc *path = NULL;
	enum isp_path_index path_index;
	struct isp_pipe_dev *dev = NULL;
	struct isp_module *module = NULL;
	struct isp_sc_array *scl_array = NULL;
	struct isp_sc_coeff *coeff = NULL;
	enum isp_id iid;

	if (!isp_handle) {
		pr_err("fail to get isp_handle is NULL\n");
		return;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->module_info;
	iid = ISP_GET_IID(dev->com_idx);
	scl_array = module->scl_array;

	if (scl_array == NULL) {
		pr_err("fail to get scl_array,it's NULL\n");
		return;
	}

	if (dev->fmcu_slw.status == ISP_ST_START &&
	    path_id == ISP_SCL_VID)
		return;

	if (path_id == ISP_SCL_PRE)
		path_index = ISP_PATH_IDX_PRE;
	else if (path_id == ISP_SCL_VID)
		path_index = ISP_PATH_IDX_VID;
	else if (path_id == ISP_SCL_CAP)
		path_index = ISP_PATH_IDX_CAP;
	else
		return;

	if (module->isp_path[path_id].status == ISP_ST_START) {
		pr_debug("ISP: shadow s %d\n", path_id);
		path = &module->isp_path[path_id];
		if (path->valid == 0) {
			pr_info("ISP: path%d not valid\n", path_id);
			return;
		}
		if (path->shadow_done_cnt == 1
		    && !dev->fmcu_slw.slw_flags
		    && path_id != ISP_SCL_CAP) {
			pr_info("path%d shadow_done_cnt %d\n",
				path_id, path->shadow_done_cnt);
			return;
		}
		path->shadow_done_cnt = 1;
		if (path_id == ISP_SCL_PRE)
			isp_coeff_get_valid_node(&scl_array->pre_queue,
						 &coeff, 1);
		else if (path_id == ISP_SCL_VID)
			isp_coeff_get_valid_node(&scl_array->vid_queue,
						 &coeff, 1);
		else /* have checked path_id is one of the three values */
			isp_coeff_get_valid_node(&scl_array->cap_queue,
						 &coeff, 1);
		/* see above else if, will return when other value
		 * else
		 *	return;
		 */
		if (coeff) {
			isp_path_set(module, &coeff->path, path_index);
			pr_debug("ISP: path%d updated\n", path_id);
		}
		if (path_id != ISP_SCL_CAP)
			rtn = isp_path_set_next_frm(module, path_index, NULL);

		pr_debug("shadow ctl\n");
		isp_path_updated_notice(module, path_index);

		if (rtn) {
			path->need_wait = 1;
			pr_info("ISP: path%d\n", path_id);
			return;
		}
	}
}

static void isp_store_vid_done(void *isp_handle)
{
	pr_debug("store vid done.\n");
	isp_path_done(ISP_SCL_VID, isp_handle);
}

static void isp_shadow_vid_done(void *isp_handle)
{
	isp_path_shadow_done(ISP_SCL_VID, isp_handle);
#if 0
	ISP_REG_MWR(ISP_ID_0,
		ISP_STORE_VID_BASE+ISP_STORE_SHADOW_CLR, BIT_0, 1);
#endif
	pr_debug("shadow vid done.\n");
}

static void isp_store_cap_pre_done(void *isp_handle)
{
	pr_debug("store pre done.\n");
	isp_path_done(ISP_SCL_PRE, isp_handle);
}

static void isp_shadow_cap_pre_done(void *isp_handle)
{
	struct isp_pipe_dev *isp_dev = NULL;
	struct isp_path_desc *path = NULL;
	struct isp_module *module = NULL;
	struct isp_fmcu_slw_desc *fmcu_slw = NULL;
	struct isp_fmcu_slice_desc *fmcu_slice = NULL;
	enum isp_scl_id path_id = ISP_SCL_PRE;

	pr_debug(" +\n");

	isp_dev = (struct isp_pipe_dev *)isp_handle;
	module = &isp_dev->module_info;
	fmcu_slw = &isp_dev->fmcu_slw;
	fmcu_slice = &isp_dev->fmcu_slice;

	if (ISP_GET_SID(isp_dev->com_idx) == ISP_SCENE_CAP) {
		pr_debug("shadow cap done, just return\n");
		return;
	}

	if (fmcu_slw->slw_flags) {
		path = &module->isp_path[path_id];
		if (fmcu_slw->vid_num == (path->frm_deci + 1) ||
				fmcu_slw->vid_num == 0) {
			fmcu_slw->vid_num = 0;
			isp_path_shadow_done(path_id, isp_handle);
		}
		fmcu_slw->vid_num++;
	} else
		isp_path_shadow_done(path_id, isp_handle);

	isp_dbg_reg_trace(isp_dev, isp_dev->com_idx);

	pr_debug(" -\n");
}

static void isp_all_done(void *isp_handle)
{
	struct isp_pipe_dev *dev = NULL;

	dev = (struct isp_pipe_dev *)isp_handle;

	if (dev && dev->pre_state == ISP_ST_START) {
		dev->pre_state = ISP_ST_STOP;
		isp_clk_pause(ISP_GET_IID(dev->com_idx), ISP_CLK_P_P);
		pr_debug("isp%d all done, pre_state start\n",
			 ISP_GET_IID(dev->com_idx));
	} else
		pr_err("fail to get isp_handle,it's NULL\n");
}

static void isp_fmcu_load_done(void *isp_handle)
{
	if (!isp_handle) {
		pr_err("fail to get isp_handle,it's NULL\n");
		return;
	}

	pr_debug("isr -\n");
}

static void isp_fmcu_shadow_done(void *isp_handle)
{
	if (!isp_handle) {
		pr_err("fail to get isp_handle,it's NULL\n");
		return;
	}

	pr_debug("isr -\n");
}

static void isp_fmcu_config_done(void *isp_handle)
{
	int ret = 0;
	struct isp_pipe_dev *dev = NULL;
	struct camera_frame *tmp_frame;
	struct offline_buf_desc *buf_desc = NULL;
	struct isp_fmcu_slice_desc *fmcu_slice = NULL;
	struct isp_path_desc *path = NULL;
	isp_isr_func user_func;
	void *data;
	struct camera_frame frame;
	struct isp_module *module = NULL;
	enum isp_scl_id path_id = ISP_SCL_CAP;
	enum isp_id iid;
	enum isp_scene_id sid;
	struct frm_timestamp sof_ts;
	int fid;

	if (!isp_handle) {
		pr_err("fail to get isp_handle,it's NULL\n");
		return;
	}
	dev = (struct isp_pipe_dev *)isp_handle;
	iid = ISP_GET_IID(dev->com_idx);
	isp_clk_pause(iid, ISP_CLK_P_C);
	sid = ISP_GET_SID(dev->com_idx);
	fmcu_slice = &dev->fmcu_slice;
	module = &dev->module_info;
	path = &module->isp_path[path_id];

	buf_desc = isp_offline_sel_buf(&module->off_desc,
				       ISP_OFF_BUF_FULL);
	tmp_frame = &dev->offline_frame[ISP_OFF_BUF_FULL];
	sof_ts = tmp_frame->sof_ts;
	fid = tmp_frame->fid;

	pr_debug("isp start fmcu config done, idx:0x%x\n", dev->com_idx);

	if (dev->fmcu_slw.status == ISP_ST_START &&
		path_id == ISP_SCL_VID) {
		pr_info("fmcu_slw ISP_ST_START.\n");
		return;
	}

	if (path->valid == 0) {
		pr_info("isp path_cap not valid\n");
		return;
	}

	user_func = p_user_func[iid][ISP_PATH_CAP_DONE];
	data = p_user_data[iid][ISP_PATH_CAP_DONE];

	if (path->need_wait) {
		path->need_wait = 0;
	} else {
		/* get output buffer */
		ret = isp_frame_dequeue(&path->frame_queue, &frame);
		if (ret) {
			pr_err("fail to dequeue frame iid%d ret %d\n",
			       iid, ret);
			return;
		}

		gen_frm_timestamp(&frame.btu_ts);

		/* return offline buffer */
		ret = isp_buf_queue_write(&buf_desc->tmp_buf_queue, tmp_frame);
		if (ret) {
			pr_err("fail to retrieve off buf full_path\n");
			return;
		}

		buf_desc->output_frame_count++;
		if (dev->is_yuv_sn == true) {
			/* release offline buffer */
			buf_desc = isp_offline_sel_buf(&module->off_desc,
					ISP_OFF_BUF_BIN);
			tmp_frame = &dev->offline_frame[ISP_OFF_BUF_BIN];
			if (tmp_frame->yaddr_vir) {
				ret = isp_buf_queue_write(&buf_desc->tmp_buf_queue,
					tmp_frame);
				if (ret) {
					pr_err("fail to retrieve off buf bin_path\n");
					return;
				}
				buf_desc->output_frame_count++;
			}
		}
		ret = pfiommu_free_addr_with_id(&frame.pfinfo, ISP_IOMMU_CH_AW,
					  AW_ID_STORE_PRE_CAP_YUV);
		if (ret)
			pr_err("fail to free frame\n");

		if (frame.pfinfo.mfd[0] !=
		    module->path_reserved_frame[path_id].pfinfo.mfd[0]) {
			frame.width = path->dst.w;
			frame.height = path->dst.h;
			frame.irq_type = CAMERA_IRQ_IMG;
			frame.sof_ts = sof_ts;
			frame.fid = fid;

			if (dev->is_raw_capture == 1) {
				frame.irq_type = CAMERA_IRQ_DONE;
				frame.irq_property = IRQ_RAW_CAP_DONE;
				frame.flags = ISP_OFF_BUF_NONE;
				dev->cap_on = 0;
			}
			pr_debug("mfd 0x%x\n", frame.pfinfo.mfd[0]);

			if (is_dual_cam) {
				pr_debug("[%d] sent frm cnt = %d\n",
					iid, frame.fid);
				pr_debug("time = %lu.%06lu\n",
					frame.sof_ts.time.tv_sec,
					frame.sof_ts.time.tv_usec);
			}

			if (dev->is_3dnr)
				pr_debug("isp%d sending 3dnr frame%d\n",
					iid, dev->frm_cnt_3dnr);
			if (user_func)
				(*user_func)(&frame, data);
		} else {
			pr_info_ratelimited("iid%d isp: use reserved cap\n",
					    iid);
			module->path_reserved_frame[path_id].pfinfo.iova[0] = 0;
			module->path_reserved_frame[path_id].pfinfo.iova[1] = 0;
		}
	}

	isp_dbg_reg_trace(dev, dev->com_idx);

	if (dev->cap_flag != DCAM_CAPTURE_START_WITH_TIMESTAMP) {
		spin_lock(&isp_mod_lock);
		fmcu_slice_capture_state = ISP_ST_STOP;
		spin_unlock(&isp_mod_lock);
	} else {
		spin_lock(&isp_mod_lock);
		fmcu_slice_capture_state_dual = ISP_ST_STOP;
		spin_unlock(&isp_mod_lock);
	}

	if (dev->is_raw_capture == 1) {
		module->off_desc.valid = 0;
		module->off_desc.status = ISP_ST_STOP;
		path->valid = 0;
		path->status = ISP_ST_STOP;
		dev->is_raw_capture = 0;
		ISP_SET_SID(dev->com_idx, ISP_SCENE_PRE);
		sprd_isp_stop(dev, 1);
		return;
	}

	dev->is_wait_fmcu = 0;
	complete(&dev->fmcu_com);
	pr_debug("isp end fmcu config done\n");
}

static void isp_fmcu_stop_done(void *isp_handle)
{
	if (!isp_handle) {
		pr_err("fail to get Input ptr is NULL\n");
		return;
	}

	pr_debug("isr -\n");
}

static void isp_fmcu_cmd_x(void *isp_handle)
{
	if (!isp_handle) {
		pr_err("fail to get Input ptr is NULL\n");
		return;
	}

	pr_debug("isr -\n");
}

static void isp_fmcu_cmd_error(void *isp_handle)
{
	pr_err("fail to cmd\n");
}

static void isp_fmcu_timeout(void *isp_handle)
{
	pr_err("fail to timeout\n");
}

static void isp_bpc_store_error(void *isp_handle)
{
	pr_err("fail to bpc store INT\n");
}

static void isp_bpc_err0(void *isp_handle)
{
	pr_err("fail to bpc INT\n");
}

static void isp_bpc_err1(void *isp_handle)
{
	pr_err("fail to bpc INT\n");
}

static void isp_bpc_err2(void *isp_handle)
{
	pr_err("fail to bpc INT\n");
}

static void isp_bpc_store_not_empty_error(void *isp_handle)
{
	pr_err("fail to bpc store not empty INT\n");
}

static void isp_hist_start(void *isp_handle)
{
	enum isp_drv_rtn rtn = 0;
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_module *module = NULL;

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->statis_module_info;

	rtn = isp_set_next_statis_buf(dev->com_idx, module, ISP_HIST_BLOCK);
	if (rtn)
		pr_err("fail to set next HIST statis buf,rtn %d\n", rtn);
}

static int isp_get_hist_statistic(uint32_t com_idx,
	struct isp_statis_buf *node, struct ion_buffer *ionbuffer)
{
	int ret = 0;
	int i = 0;
	int max_item = ISP_HIST_ITEMS;
	unsigned long HIST_BUF = ISP_HIST_CH0;
	uint64_t *hist_statis = NULL;
	uint32_t isp_core_pmu_en = 0;

#ifdef CONFIG_64BIT
	hist_statis =
		(uint64_t *)(((unsigned long)node->kaddr[0]) |
		((unsigned long)(node->kaddr[1] << 32)));
#else
	hist_statis = (uint64_t *)(node->kaddr[0]);
#endif

	if (!hist_statis) {
		ret = -1;
		pr_err("fail to get correct hist memory\n");
		return ret;
	}

	/*
	 * Close the CLK gate dynamic switch of ISP block
	 * to get the value of hist correctly
	 */
	isp_core_pmu_en = ISP_HREG_RD(com_idx, ISP_PMU_EN);
	ISP_HREG_WR(com_idx, ISP_PMU_EN, 0xffff0000);

	if (ionbuffer && (ionbuffer->kmap_cnt > 0)) {
		for (i = 0; i < max_item; i++)
			hist_statis[i] = ISP_HREG_RD(com_idx, HIST_BUF + i * 4);

		pr_debug("ISP%d hist %ld statis %lu %lu\n", com_idx, HIST_BUF,
			(unsigned long) hist_statis[0],
			(unsigned long) hist_statis[max_item-1]);
	} else {
		ret = -1;
		pr_err("fail to access hist memory ionbuffer %p\n", ionbuffer);
		return ret;
	}

	ISP_HREG_WR(com_idx, ISP_PMU_EN, isp_core_pmu_en);

	return ret;
}

static void isp_hist_done(void *isp_handle)
{
	enum isp_drv_rtn rtn = 0;
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_frm_queue *statis_heap = NULL;
	struct isp_statis_buf node;
	struct camera_frame frame_info;
	struct isp_statis_module *module = NULL;

	memset(&frame_info, 0x00, sizeof(frame_info));

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->statis_module_info;
	statis_heap = &module->hist_statis_frm_queue;

	/*dequeue the statis buf from a array*/
	rtn = isp_statis_dequeue(statis_heap, &node);
	if (rtn) {
		pr_err("fail to dequeue HIST buf\n");
		return;
	}

	if (node.phy_addr != module->hist_buf_reserved.phy_addr) {
		rtn = isp_get_hist_statistic(dev->com_idx, &node,
			module->img_statis_buf.pfinfo.buf[0]);

		frame_info.buf_size = node.buf_size;
		memcpy(frame_info.pfinfo.mfd, node.pfinfo.mfd,
		       sizeof(uint32_t) * 3);
		frame_info.phy_addr = node.phy_addr;
		frame_info.vir_addr = node.vir_addr;
		frame_info.addr_offset = node.addr_offset;
		frame_info.kaddr[0] = node.kaddr[0];
		frame_info.kaddr[1] = node.kaddr[1];
		frame_info.irq_type = CAMERA_IRQ_STATIS;
		frame_info.irq_property = IRQ_HIST_STATIS;
		frame_info.frame_id = module->hist_statis_cnt;
		pr_debug("current hist frame id %d\n", frame_info.frame_id);

		/*call_back func to write the buf addr to usr_buf_queue*/
		isp_irq_reg(ISP_GET_IID(dev->com_idx),
				ISP_HIST_DONE, &frame_info);
	}
	module->hist_statis_cnt++;
}

static void isp_cfg_error(void *isp_handle)
{
	pr_err("fail to cfg INT\n");
}

static isp_isr s_isp_int_isr_list[INT_REG_SETS][INT_NUM_PER_SET] = {
	[0][ISP_INT_ISP_ALL_DONE] = isp_all_done,
	[0][ISP_INT_AFL_START] = isp_afl_start,
	[0][ISP_INT_AFL_DONE] = isp_afl_done,
	[0][ISP_INT_AFL_ERROR] = isp_afl_error,
	[0][ISP_INT_BINNING_START] = isp_binning_start,
	[0][ISP_INT_BINNING_DONE] = isp_binning_done,
	[0][ISP_INT_BINNING_ERROR] = isp_binning_error,

	[1][ISP_INT_STORE_DONE_CAP_PRE] = isp_store_cap_pre_done,
	[1][ISP_INT_SHADOW_DONE_CAP_PRE] = isp_shadow_cap_pre_done,
	[1][ISP_INT_STORE_DONE_VID] = isp_store_vid_done,
	[1][ISP_INT_SHADOW_DONE_VID] = isp_shadow_vid_done,
	[1][ISP_INT_BPC_STORE_ERROR] = isp_bpc_store_error,
	[1][ISP_INT_BPC_ERR2] = isp_bpc_err2,
	[1][ISP_INT_BPC_ERR1] = isp_bpc_err1,
	[1][ISP_INT_BPC_ERR0] = isp_bpc_err0,
	[1][ISP_INT_BPC_STORE_NOT_EMPTY_ERROR] = isp_bpc_store_not_empty_error,
	[1][ISP_INT_HIST_START] = isp_hist_start,
	[1][ISP_INT_HIST_DONE] = isp_hist_done,
	[1][ISP_INT_CFG_ERR] = isp_cfg_error,

	[2][ISP_INT_AFM_RGB_START] = isp_afm_rgb_start,
	[2][ISP_INT_AFM_RGB_DONE] = isp_afm_rgb_done,
	[2][ISP_INT_FMCU_LOAD_DONE] = isp_fmcu_load_done,
	[2][ISP_INT_FMCU_CONFIG_DONE] = isp_fmcu_config_done,
	[2][ISP_INT_FMCU_SHADOW_DONE] = isp_fmcu_shadow_done,
	[2][ISP_INT_FMCU_CMD_X] = isp_fmcu_cmd_x,
	[2][ISP_INT_FMCU_TIMEOUT] = isp_fmcu_timeout,
	[2][ISP_INT_FMCU_CMD_ERROR] = isp_fmcu_cmd_error,
	[2][ISP_INT_FMCU_STOP_DONE] = isp_fmcu_stop_done,
};

static void __isp_isr_locked(uint32_t base_addr, struct isp_pipe_dev *dev)
{
	uint32_t set_idx;
	uint32_t bits_idx;
	uint32_t bidx_max;
	uint32_t irq_status;
	uint32_t reg_val;
	uint32_t idx = dev->com_idx;
	enum isp_id iid = ISP_GET_IID(dev->com_idx);
	enum isp_scene_id sid = ISP_GET_SID(dev->com_idx);
	uint32_t cid = ISP_GET_CFG_ID(sid, iid);
	int *order;
	int ordering;
	int pivot;

	for (set_idx = 0; set_idx < INT_REG_SETS; set_idx++) {
		reg_val = ISP_HREG_RD(idx,
			base_addr+irq_sets[set_idx].offset[INT_REG_OFF_INT]);

		/* record the int status */
		pivot = irq_sets[set_idx].pivot;
		irq_sets[set_idx].recent[pivot] = reg_val;
		irq_sets[set_idx].pivot = (pivot + 1) % NUM_MOST_RECENT_STATUS;

		irq_status = irq_sets[set_idx].msk_bmap[sid] & reg_val;
		irq_sets[set_idx].irq_status = irq_status;
		if (irq_status == 0)
			continue;

		ISP_HREG_WR(idx,
			    base_addr+irq_sets[set_idx].offset[INT_REG_OFF_CLR],
			    irq_status);

		bidx_max = irq_sets[set_idx].bits_idx_max;
		order = irq_sets[set_idx].order;
		for (bits_idx = 0; bits_idx < bidx_max; bits_idx++) {
			ordering = order[bits_idx];
			if ((irq_status & (1 << ordering)) &&
			    s_isp_int_isr_list[set_idx][ordering]) {
				pr_debug("isp_com 0x%x, %s, reg%d, %pF\n",
					 idx, int_source[cid],
					 set_idx,
					 s_isp_int_isr_list[set_idx][ordering]);

				s_isp_int_isr_list[set_idx][ordering](dev);
			}
		}
	}
}

static int printbinary(char *buf, unsigned long x, int nbits)
{
	unsigned long mask = 1UL << (nbits - 1);

	while (mask != 0) {
		*buf++ = (mask & x ? '1' : '0');
		mask >>= 1;
	}
	*buf = '\0';

	return nbits;
}

static void record_int_cnts(struct isp_pipe_dev *dev)
{
	int reg_val = 0;
	int iid, sid, set, i = 0;

	if (!isp_dbg_check_switch_on((void *)dev, ISP_INT_DBG_SW))
		return;

	for (iid = 0; iid < ISP_ID_MAX; iid++) { /*isp 01*/
		for (sid = 0; sid < ISP_SCENE_NUM; sid++) { /* scene 01*/
			for (set = 0; set < INT_REG_SETS; set++) {
				reg_val = ISP_HREG_RD(iid,
				int_reg_base[iid][sid] +
				irq_sets[set].offset[INT_REG_OFF_INT]);
				for (i = 0; i < INT_NUM_PER_SET; i++)
					if (reg_val & (1 << i))
						s_int_cnt[iid][sid][set][i]++;
			}
		}
	}
}

static void print_int_cnts(struct isp_pipe_dev *dev)
{
	int iid, sid, set, i = 0;

	if (!isp_dbg_check_switch_on((void *)dev, ISP_INT_DBG_SW))
		return;

	for (iid = 0; iid < ISP_ID_MAX; iid++) { /*isp 01*/
		for (sid = 0; sid < ISP_SCENE_NUM; sid++) { /* scene 01*/
			for (set = 0; set < INT_REG_SETS; set++) {
				for (i = 0; i < INT_NUM_PER_SET; i++)
					pr_info("int[%d][%d][%d][%d]: %d\n",
						iid, sid, set, i,
						s_int_cnt[iid][sid][set][i]);
			}
		}
	}
}

static void isp_isr_dump_reg(struct isp_pipe_dev *dev)
{
	int i, j;
	uint32_t addr, base;
	uint32_t begin, end;
	uint32_t reg_val;

	pr_warn("debug sub-block: ISP_ISP_INT --\n");

	for (i = 0; i < ISP_ID_MAX; i++) {
		for (j = 0; j < ISP_SCENE_NUM; j++) {
			base = int_reg_base[i][j];
			begin = base + ISP_INT_STATUS;
			end = base + ISP_INT_ALL_DONE_SRC_CTRL
				- ISP_INT_STATUS;
			for (addr = begin; addr <= end; addr += 4) {
				reg_val = ISP_HREG_RD(ISP_ID_0, addr);
				pr_warn("\t[%c%d] 0x%08X: %08X\n",
					j == 0 ? 'P':'C', i,
					addr, reg_val);
			}
		}
	}
}

static void isp_isr_dump_status_history(struct isp_pipe_dev *dev)
{
	int i, j;
	char bin[32+1] = {0};

	for (i = 0; i < INT_REG_SETS; i++) {
		pr_info("debug sub-block: reg %d, pivot = %d\n",
			i, irq_sets[i].pivot);
		for (j = 0; j < NUM_MOST_RECENT_STATUS; j++) {
			printbinary(bin, irq_sets[i].recent[j],
				    INT_NUM_PER_SET);
			pr_info("debug sub-block: %s\n", bin);
			irq_sets[i].recent[j] = 0;
		}

		irq_sets[i].pivot = 0;
	}
}

#define ISP_DETECT_COUNT		100 /* do not detect at every isr */
#define ISP_DETECT_PERIOD		5 /* second */
#define ISP_DETECT_NORMAL		600 /* 600 per sec is normal */
#define ISP_DETECT_MULTI		100 /* simply from experience */
#define ISP_DETECT_BOUND_PER_SEC \
	((ISP_DETECT_NORMAL) * (ISP_DETECT_PERIOD) * (ISP_DETECT_MULTI))

static unsigned long s_last_jiffies;
static unsigned long s_last_jiffies_cnt;
static int s_jif_cnt = 0;

static void isp_isr_flooding_detector(struct isp_pipe_dev *dev)
{
	if ((++dev->isr_count % ISP_DETECT_COUNT) == 0) {
		unsigned long now = jiffies;
		unsigned long checkpoint = dev->isr_last_time +
			msecs_to_jiffies(ISP_DETECT_PERIOD * 1000);
			/* pr_debug("debug detector: after %d ISR\n",
				ISP_DETECT_COUNT); */

		/* when isp isr flooding, cpu time tick may not be increased
		 * no longer. So we use the isp isr counter instead.
		 */
		if (s_last_jiffies == now)
			s_jif_cnt++;
		else
			s_jif_cnt = 0;

		if (time_after(now, checkpoint) || (s_jif_cnt == 50)) {
			/* start to detect */
			/* pr_debug("debug detector: after %d s\n",
				ISP_DETECT_PERIOD); */
			/*if (dev->isr_count % 3 == 0) {*/
			s_jif_cnt = 0;
			if (dev->isr_count > ISP_DETECT_BOUND_PER_SEC) {
				struct camera_dev *cam_dev = NULL;
				struct camera_group *cam_grp = NULL;

				pr_err("fail to get valid isr_count, ISP ISR flooding, reset H/W \
					(%d / %lu)\n",
					dev->isr_count,
					now - dev->isr_last_time);
				isp_isr_dump_reg(dev);
				isp_irq_ctrl(dev, false);
				isp_isr_dump_status_history(dev);
				print_int_cnts(dev);
				sprd_isp_reset(dev);

				cam_grp =  dev->cam_grp;
				if (cam_grp != NULL) {
					int iid = ISP_GET_IID(dev->com_idx);

					cam_dev = cam_grp->dev[iid];
					/*report_isp_err(cam_dev);*/
				} else {
					pr_err("fail to report isp err\n");
				}

				if (fmcu_slice_capture_state == ISP_ST_START) {
					pr_info("force stop pipeline\n");
					sprd_isp_force_stop_pipeline(dev);
				}
				dev->isr_count = 0;
			}
			dev->isr_last_time = now;
			dev->isr_count = 0;
		}
	} else {
		if ((++s_last_jiffies_cnt % 10000) == 0) {
			s_last_jiffies = jiffies;
			s_last_jiffies_cnt = 0;
		}
	}
}

static irqreturn_t isp_isr_root(int irq, void *priv)
{
	uint32_t isp_int_pre_reg_base;
	uint32_t isp_int_cap_reg_base;
	struct isp_pipe_dev *dev = (struct isp_pipe_dev *)priv;
	uint32_t sid_flag;
	enum isp_id iid = ISP_ID_0;

	if (!dev) {
		pr_err("fail to get invalid dev\n");
		return IRQ_NONE;
	}

	if (irq == s_isp_irq[ISP_ID_0].ch0)
		iid = ISP_ID_0;
	else if (irq == s_isp_irq[ISP_ID_1].ch0)
		iid = ISP_ID_1;
	else {
		pr_err("fail to get irq\n");
		return IRQ_HANDLED;
	}

	isp_int_pre_reg_base = int_reg_base[iid][ISP_SCENE_PRE];
	isp_int_cap_reg_base = int_reg_base[iid][ISP_SCENE_CAP];

	ISP_SAVE_SID(dev->com_idx, sid_flag);

	record_int_cnts(dev);
	isp_isr_flooding_detector(dev);

	/*
	 * Both pre and cap should handle the interrupt.
	 * For p0/p1, we have masked the fmcu related int,
	 * it means fmcu only used for cap for now.
	 */
	spin_lock(&dev->pre_lock);
	ISP_SET_SID(dev->com_idx, ISP_SCENE_PRE);
	__isp_isr_locked(isp_int_pre_reg_base, dev);
	spin_unlock(&dev->pre_lock);

	if (fmcu_slice_capture_state == ISP_ST_START) {
		spin_lock(&dev->cap_lock);
		ISP_SET_SID(dev->com_idx, ISP_SCENE_CAP);
		__isp_isr_locked(isp_int_cap_reg_base, dev);
		spin_unlock(&dev->cap_lock);
	}

	ISP_RESTORE_SID(dev->com_idx, sid_flag);

	return IRQ_HANDLED;
}

int isp_irq_request(struct device *p_dev, struct isp_ch_irq *irq,
                    struct isp_pipe_dev *dev)
{
	int ret = 0;
	enum isp_id iid;

	if (!p_dev || !irq || !dev) {
		pr_err("fail to get Input ptr is NULL\n");
		return -EFAULT;
	}

	iid = ISP_GET_IID(dev->com_idx);

	switch (iid) {
	case ISP_ID_0:
        memset(&s_int_cnt[ISP_ID_0], 0x00, sizeof(s_int_cnt)/2);
		ret = devm_request_irq(p_dev, irq->ch0, isp_isr_root,
				       IRQF_SHARED, "ISP0", (void *)dev);
		break;
	case ISP_ID_1:
        memset(&s_int_cnt[ISP_ID_1], 0x00, sizeof(s_int_cnt)/2);
		ret = devm_request_irq(p_dev, irq->ch0, isp_isr_root,
				       IRQF_SHARED, "ISP1", (void *)dev);
		break;
	default:
		pr_err("fail to isp context %d\n", iid);
		return -1;
	}
	if (ret)
		pr_err("fail to get irq isp%d register %d\n", iid, ret);
	return ret;
}

int isp_irq_free(struct isp_ch_irq *irq, struct isp_pipe_dev *dev)
{
	int ret = 0;

	if (!irq || !dev) {
		pr_err("fail to Input ptr is NULL\n");
		return -EFAULT;
	}

	pr_debug("isp%d\n", ISP_GET_IID(dev->com_idx));
	free_irq(irq->ch0, (void *)dev);

	return ret;
}

int isp_irq_callback(enum isp_id iid, enum isp_irq_id irq_id,
		     isp_isr_func user_func, void *user_data)
{
	unsigned long flag;

	if (!user_data) {
		pr_err("fail to get Input callback data is NULL\n");
		return -EFAULT;
	}

	spin_lock_irqsave(&isp_mod_lock, flag);
	p_user_func[iid][irq_id] = user_func;
	p_user_data[iid][irq_id] = user_data;
	spin_unlock_irqrestore(&isp_mod_lock, flag);

	return 0;
}

void isp_irq_ctrl(struct isp_pipe_dev *dev, bool enable)
{
	uint32_t idx = 0;
	enum isp_id iid = ISP_ID_0;
	enum isp_scene_id sid = ISP_SCENE_PRE;
	uint32_t base_addr = 0;
	uint32_t set_idx = 0;
	uint32_t clr_reg = 0;
	uint32_t en_reg = 0;
	uint32_t en_mask = 0;

	if (!dev)
		return;

	iid = ISP_GET_IID(dev->com_idx);
	idx = dev->com_idx;

	for (sid = ISP_SCENE_PRE; sid < ISP_SCENE_NUM; sid++) {
		base_addr = int_reg_base[iid][sid];
		for (set_idx = 0; set_idx < INT_REG_SETS; set_idx++) {
			clr_reg = irq_sets[set_idx].offset[INT_REG_OFF_CLR];
			en_reg = irq_sets[set_idx].offset[INT_REG_OFF_EN];
			en_mask = irq_sets[set_idx].msk_bmap[sid];

			if (enable) {
				sprd_isp_glb_reg_owr(idx,
						     base_addr + clr_reg,
						     0xFFFFFFFF,
						     ISP_INIT_CLR_REG);
				sprd_isp_glb_reg_owr(idx,
						     base_addr + en_reg,
						     en_mask,
						     ISP_INIT_MASK_REG);
			} else {
				sprd_isp_glb_reg_awr(idx,
						     base_addr + en_reg,
						     0,
						     ISP_INIT_MASK_REG);
				sprd_isp_glb_reg_owr(idx,
						     base_addr + clr_reg,
						     0xFFFFFFFF,
						     ISP_INIT_CLR_REG);
			}
		}
	}
}

