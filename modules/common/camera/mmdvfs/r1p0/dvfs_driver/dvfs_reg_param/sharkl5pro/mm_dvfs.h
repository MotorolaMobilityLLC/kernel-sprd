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

#ifndef __MM_DVFS_REG_H___
#define __MM_DVFS_REG_H___

#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/of.h>
//#include <video/sprd_mm.h>
#include <sprd_mm.h>
#include <linux/dma-buf.h>
#include <linux/scatterlist.h>
#include <linux/sprd_iommu.h>
#include <linux/sprd_ion.h>

#include "mm_dvfs_reg.h"
#include "top_dvfs_reg.h"

enum {
	VOLT70 = 0, //0.7
	VOLT75, //0.75
	VOLT80, //0.8
};

enum {
	CPP_CLK768 = 76800000, //76.8
	CPP_CLK1280 = 128000000, //128
	CPP_CLK2560 = 256000000, //256
	CPP_CLK3840 = 384000000, //384
};

enum {
	ISP_CLK2560 = 256000000, //76.8
	ISP_CLK3072 = 307200000, //307.2
	ISP_CLK3840 = 384000000, //384
	ISP_CLK4680 = 468000000, //468
	ISP_CLK5120 = 512000000, //512
};

enum {
	JPG_CLK768 = 76800000, //76.8
	JPG_CLK1280 = 128000000, //128
	JPG_CLK2560 = 256000000, //256
	JPG_CLK3840 = 384000000, //256
};

enum {
	FD_CLK768 = 76800000, //76.8
	FD_CLK1920 = 192000000, //192
	FD_CLK3072 = 307200000, //307
	FD_CLK3840 = 384000000, //384
};

enum {
	MTX_CLK768 = 76800000, //76.8
	MTX_CLK1280 = 128000000, //128
	MTX_CLK2560 = 256000000, //256 ---2
	MTX_CLK3072 = 307200000, //307.2
	MTX_CLK3840 = 384000000, //384----4
	MTX_CLK4680 = 468000000, //468
	MTX_CLK5120 = 512000000,
};

enum {
	DCAM_CLK48   = 48000000, //76.8
	DCAM_CLK64   = 64000000,
	DCAM_CLK768  = 76800000, //307.2
	DCAM_CLK1920 = 192000000, //384
	DCAM_CLK2560 = 256000000, //468
	DCAM_CLK3072 = 307200000, //384
	DCAM_CLK3840 = 384000000, //468
	DCAM_CLK4680 = 468000000, //468
};

enum {
	DCAMAXI_CLK2560 = 256000000, //307.2
	DCAMAXI_CLK3072 = 307200000, //384
	DCAMAXI_CLK3840 = 384000000, //384
	DCAMAXI_CLK4680 = 468000000, //384
};

enum {
	CPP_CLK_INDEX_768 = 0,//76.8
	CPP_CLK_INDEX_1280 = 1, //128
	CPP_CLK_INDEX_2560 = 2, //256
	CPP_CLK_INDEX_3840 = 3, //384
};

enum {
	ISP_CLK_INDEX_2560 = 0, //76.8
	ISP_CLK_INDEX_3072 = 1, //307.2
	ISP_CLK_INDEX_3840 = 2, //384
	ISP_CLK_INDEX_4680 = 3, //468
	ISP_CLK_INDEX_5120 = 4, //512
};

enum {
	JPG_CLK_INDEX_768 = 0, //76.8
	JPG_CLK_INDEX_1280 = 1, //128
	JPG_CLK_INDEX_2560 = 2, //256
	JPG_CLK_INDEX_3840 = 3, //256
};

enum {
	FD_CLK_INDEX_768 = 0, //76.8
	FD_CLK_INDEX_1920 = 1, //192
	FD_CLK_INDEX_3072 = 2, //307
	FD_CLK_INDEX_3840 = 3, //384
};

enum {
	MTX_CLK_INDEX_768 = 0, //76.8
	MTX_CLK_INDEX_1280 = 1, //128
	MTX_CLK_INDEX_2560 = 2, //256 ---2
	MTX_CLK_INDEX_3072 = 3, //307.2
	MTX_CLK_INDEX_3840 = 4, //384----4
	MTX_CLK_INDEX_4680 = 5, //468
	MTX_CLK_INDEX_5120 = 6,
};

enum {
	DCAM_CLK_INDEX_1920 = 0,
	DCAM_CLK_INDEX_2560,
	DCAM_CLK_INDEX_3072,
	DCAM_CLK_INDEX_3840,
	DCAM_CLK_INDEX_4680,
};

enum {
	DCAMAXI_CLK_INDEX_2560 = 0, //307.2
	DCAMAXI_CLK_INDEX_3072 = 1, //384
	DCAMAXI_CLK_INDEX_3840 = 2, //384
	DCAMAXI_CLK_INDEX_4680 = 3, //384
};


#define DVFS_REG_RD(reg)       (REG_RD(REGS_MM_DVFS_AHB_BASE+reg))
#define DVFS_REG_WR(reg, val)  (REG_WR((REGS_MM_DVFS_AHB_BASE+reg), (val)))

#define DVFS_REG_RD_AHB(reg)       (REG_RD(REGS_MM_AHB_BASE+reg))
#define DVFS_REG_WR_AHB(reg, val)  (REG_WR((REGS_MM_AHB_BASE+reg), (val)))

#define DVFS_REG_RD_ABS(reg)       (REG_RD(REGS_MM_TOP_DVFS_BASE+reg))
#define DVFS_REG_WR_ABS(reg, val)  (REG_WR((REGS_MM_TOP_DVFS_BASE+reg), (val)))

#define DVFS_REG_RD_ON(reg)       (REG_RD(REGS_MM_POWER_ON_BASE+reg))
#define DVFS_REG_WR_ON(reg, val)  (REG_WR((REGS_MM_POWER_ON_BASE+reg), (val)))
//0x327d0000
#define DVFS_REG_RD_POWER(reg)       (REG_RD(REGS_MM_POWER_BASE+reg))
#define DVFS_REG_WR_POWER(reg, val)  (REG_WR((REGS_MM_POWER_BASE+reg), (val)))

#define DVFS_REG_WR_OFF(ipbase, reg, val) (REG_WR(ipbase + reg, val))
#define DVFS_REG_RD_OFF(ipbase, reg)      (REG_RD(ipbase+reg))

#define DVFS_REG_MWR(reg, msk, val)  DVFS_REG_WR(reg, \
		((val)&(msk))|(DVFS_REG_RD(reg)&(~(msk))))
#define DVFS_REG_OWR(reg, val)  \
		DVFS_REG_WR(reg, (DVFS_REG_RD(reg)|val))

#endif /* __MM_DVFS_REG_H___ */
