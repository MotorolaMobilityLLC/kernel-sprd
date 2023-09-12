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

#ifndef _CPP_DRV_H_
#define _CPP_DRV_H_

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/scatterlist.h>
#include <linux/sprd_iommu.h>
#include <linux/sprd_ion.h>
#include <linux/types.h>
#include <sprd_mm.h>
#include <linux/regmap.h>

#include "cpp_block.h"

typedef int (*cpp_hw_ioctl_fun)(void *arg);

struct hw_ioctrl_fun {
	uint32_t cmd;
	cpp_hw_ioctl_fun hw_ctrl;
};

typedef int (*cppdrv_ioctl_fun)(void *arg1, void *arg2);

struct cppdrv_ioctrl {
	uint32_t cmd;
	cppdrv_ioctl_fun hw_ctrl;
};

enum cppdrv_cfg_cmd {
	CPP_DRV_QOS_SET,
	CPP_DRV_MMU_SET,
	CPP_DRV_MODULE_RESET,
	CPP_DRV_SCL_SUPPORT_INFO_GET,
	CPP_DRV_SCL_MAX_SIZE_GET,
	CPP_DRV_SCL_START,
	CPP_DRV_SCL_STOP,
	CPP_DRV_SCL_CAPABILITY_GET,
	CPP_DRV_SCL_EB,
	CPP_DRV_SCL_CFG_PARAM_SET,
	CPP_DRV_SCL_REG_SET,
	CPP_DRV_SCL_SLICE_PARAM_CHECK,
	CPP_DRV_SCL_SLICE_PARAM_SET,
	CPP_DRV_SCL_PARAM_CHECK,
	CPP_DRV_SCL_SL_STOP,
	CPP_DRV_SCL_REG_TRACE,
	CPP_DRV_SCL_RESET,
	CPP_DRV_ROT_PARM_CHECK,
	CPP_DRV_ROT_END,
	CPP_DRV_ROT_Y_PARM_SET,
	CPP_DRV_ROT_UV_PARM_SET,
	CPP_DRV_ROT_START,
	CPP_DRV_ROT_STOP,
	CPP_DRV_ROT_REG_TRACE,
	CPP_DRV_ROT_RESET,
	CPP_DRV_CLK_EB,
	CPP_DRV_CLK_DIS,
	CPP_DRV_CFG_MAX,
	CPP_DRV_DMA_REG_TRACE,
	CPP_DRV_DMA_EB,
	CPP_DRV_DMA_SET_PARM,
	CPP_DRV_DMA_CFG_PARM,
	CPP_DRV_DMA_START,
	CPP_DRV_DMA_STOP,
	CPP_DRV_DMA_RESET
};


enum cpp_hw_cfg_cmd {
	CPP_HW_CFG_BP_SUPPORT,
	CPP_HW_CFG_SLICE_SUPPORT,
	CPP_HW_CFG_ZOOMUP_SUPPORT,
	CPP_HW_CFG_QOS_SET,
	CPP_HW_CFG_MMU_SET,
	CPP_HW_CFG_MODULE_RESET,
	CPP_HW_CFG_SCALE_RESET,
	CPP_HW_CFG_ROT_RESET,
	CPP_HW_CFG_CLK_EB,
	CPP_HW_CFG_CLK_DIS,
	CPP_HW_CFG_SC_REG_TRACE,
	CPP_HW_CFG_ROT_REG_TRACE,
	CPP_HW_CFG_ROT_EB,
	CPP_HW_CFG_ROT_DISABLE,
	CPP_HW_CFG_ROT_START,
	CPP_HW_CFG_ROT_STOP,
	CPP_HW_CFG_ROT_PARM_SET,
	CPP_HW_CFG_SCL_EB,
	CPP_HW_CFG_SCL_DISABLE,
	CPP_HW_CFG_SCL_START,
	CPP_HW_CFG_SCL_STOP,
	CPP_HW_CFG_SCL_CLK_SWITCH,
	CPP_HW_CFG_SCL_DES_PITCH_SET,
	CPP_HW_CFG_SCL_SRC_PITCH_SET,
	CPP_HW_CFG_SCL_IN_RECT_SET,
	CPP_HW_CFG_SCL_OUT_RECT_SET,
	CPP_HW_CFG_SCL_IN_FORMAT_SET,
	CPP_HW_CFG_SCL_OUT_FORMAT_SET,
	CPP_HW_CFG_SCL_DECI_SET,
	CPP_HW_CFG_SCL_IN_ENDIAN_SET,
	CPP_HW_CFG_SCL_OUT_ENDIAN_SET,
	CPP_HW_CFG_SCL_BURST_GAP_SET,
	CPP_HW_CFG_SCL_BPEN_SET,
	CPP_HW_CFG_SCL_INI_PHASE_SET,
	CPP_HW_CFG_SCL_TAP_SET,
	CPP_HW_CFG_SCL_OFFSET_SIZE_SET,
	CPP_HW_CFG_SCL_ADDR_SET,
	CPP_HW_CFG_SCL_LUMA_HCOEFF_SET,
	CPP_HW_CFG_SCL_CHRIMA_HCOEF_SET,
	CPP_HW_CFG_SCL_VCOEF_SET,
	CPP_HW_CFG_MAX,
	CPP_HW_CFG_DMA_EB,
	CPP_HW_CFG_DMA_DISABLE,
	CPP_HW_CFG_DMA_RESET,
	CPP_HW_CFG_DMA_START,
	CPP_HW_CFG_DMA_STOP,
	CPP_HW_CFG_DMA_PARM
};

#define ALIGNED_DOWN_2(w)   ((w) & ~(2 - 1))
#define ALIGNED_DOWN_4(w)   ((w) & ~(4 - 1))
#define ALIGNED_DOWN_8(w)   ((w) & ~(8 - 1))
#define MOD(x, a)           (x % a)
#define MOD2(x)             (x % 2)
#define OSIDE(x, a, b)      ((x < a) || (x > b))
#define CMP(x, a, b)        (x < (a + b))

struct rotif_device {
	atomic_t count;
	struct mutex rot_mutex;
	struct completion done_com;
	struct rot_drv_private drv_priv;
};

struct scif_device {
	atomic_t count;
	struct mutex sc_mutex;
	struct completion done_com;
	struct scale_drv_private drv_priv;
};

struct dmaif_device {
	atomic_t count;
	struct mutex dma_mutex;
	struct completion done_com;
	struct dma_drv_private drv_priv;
};

struct cppdrv_ops {
	int (*ioctl)(enum cppdrv_cfg_cmd cmd, void *arg1, void *arg2);
};

struct cpp_pipe_dev {
	unsigned int irq;
	void __iomem *io_base;
	struct platform_device *pdev;
	spinlock_t hw_lock;
	spinlock_t slock;
	struct rotif_device *rotif;
	struct scif_device *scif;
	struct dmaif_device *dmaif;
	struct cpp_hw_info *hw_info;
	struct cppdrv_ops *cppdrv_ops;
};

extern atomic_t cpp_dma_cnt;

int cpp_drv_get_cpp_res(struct cpp_pipe_dev *cppif, struct cpp_hw_info *hw);
#endif
