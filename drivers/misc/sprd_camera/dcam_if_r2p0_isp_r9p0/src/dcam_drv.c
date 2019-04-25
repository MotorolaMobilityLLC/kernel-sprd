/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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

#include <dt-bindings/soc/sprd,sharkl5pro-regs.h>
#include <dt-bindings/soc/sprd,sharkl5pro-mask.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/completion.h>
#include <video/sprd_mm.h>

#include "dcam_drv.h"
#include "cam_pw_domain.h"
#include "isp_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "DCAM_DRV: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define DCAM_DRV_DEBUG
#define DCAM_CLK_NUM                   4
#define DCAM_AXI_STOP_TIMEOUT          1000
#define DCAM_STATE_QUICKQUIT           0x01
#define SHAR                           0x53686172
#define KL3                            0x6B4C3300
#define IMG_TYPE_RAW                   0x2B
#define IMG_TYPE_YUV                   0x1E
#define DCAM_FULL_PATH_PAUSE           1

struct platform_device *s_dcam_pdev;
static struct mutex dcam_module_sema[DCAM_MAX_COUNT];
static atomic_t s_dcam_users[DCAM_MAX_COUNT];
static atomic_t s_dcam_total_users;
static struct dcam_module *s_p_dcam_mod[DCAM_MAX_COUNT];
static spinlock_t dcam_glb_reg_cfg_lock[DCAM_MAX_COUNT];
static spinlock_t dcam_glb_reg_control_lock[DCAM_MAX_COUNT];
static spinlock_t dcam_glb_reg_mask_lock[DCAM_MAX_COUNT];
static spinlock_t dcam_glb_reg_clr_lock[DCAM_MAX_COUNT];
static spinlock_t dcam_glb_reg_ahbm_sts_lock[DCAM_MAX_COUNT];
static spinlock_t dcam_glb_reg_endian_lock[DCAM_MAX_COUNT];

#ifdef FPGA_BRINGUP
static struct clk *dcam_clk;
static struct clk *dcam_clk_parent;
static struct clk *dcam_clk_default;
static struct clk *dcam0_bpc_clk;
static struct clk *dcam0_bpc_clk_parent;
static struct clk *dcam0_bpc_clk_default;
static struct clk *dcam_axi_eb;
static struct clk *dcam_eb;
static struct regmap *cam_ahb_gpr;
static struct regmap *aon_apb;
#endif
static unsigned int chip_id0;
static unsigned int chip_id1;

unsigned long s_dcam_regbase[DCAM_MAX_COUNT];
unsigned long s_dcam_aximbase;
unsigned long s_dcam_mmubase;
static uint32_t s_dcam_count;
int s_dcam_irq[DCAM_MAX_COUNT];
spinlock_t dcam_lock[DCAM_MAX_COUNT];
static struct dcam_group s_dcam_group;


static int sprd_dcamdrv_clk_en(enum dcam_id idx)
{
	int ret = 0;
#ifdef FPGA_BRINGUP
	uint32_t flag = 0;

	if (atomic_read(&s_dcam_total_users) != 0)
		return ret;

	/*set dcam_if clock to max value*/
	ret = clk_set_parent(dcam_clk, dcam_clk_parent);
	if (ret) {
		pr_err("fail to set dcam_clk_parent\n");
		clk_set_parent(dcam_clk, dcam_clk_default);
		clk_disable_unprepare(dcam_eb);
		goto exit;
	}

	ret = clk_prepare_enable(dcam_clk);
	if (ret) {
		pr_err("fail to enable dcam_clk\n");
		clk_set_parent(dcam_clk, dcam_clk_default);
		clk_disable_unprepare(dcam_eb);
		goto exit;
	}

	ret = clk_set_parent(dcam0_bpc_clk, dcam0_bpc_clk_parent);
	if (ret) {
		pr_err("fail to set dcam0_bpc_clk_parent\n");
		clk_set_parent(dcam0_bpc_clk, dcam0_bpc_clk_parent);
		goto exit;
	}

	ret = clk_prepare_enable(dcam0_bpc_clk);
	if (ret) {
		pr_err("fail to enable dcam0_bpc_clk\n");
		clk_set_parent(dcam0_bpc_clk, dcam0_bpc_clk_default);
		goto exit;
	}

	if (sprd_dcam_drv_chip_id_get() != SHARKL3)
		regmap_update_bits(cam_ahb_gpr, REG_MM_AHB_AHB_EB,
			BPC_CLK_EB_LEP, BPC_CLK_EB_LEP);
	/*dcam enable*/
	ret = clk_prepare_enable(dcam_eb);
	if (ret) {
		pr_err("fail to enable dcam0.\n");
		goto exit;
	}

	if (sprd_dcam_drv_chip_id_get() == SHARKL3) {
		ret = clk_prepare_enable(dcam_axi_eb);
		if (ret) {
			pr_err("fail to enable dcam_axi_eb.\n");
			goto exit;
		}
	}
	flag = BIT_MM_AHB_DCAM_AXIM_SOFT_RST |
			BIT_MM_AHB_DCAM_ALL_SOFT_RST;
	regmap_update_bits(cam_ahb_gpr,
		REG_MM_AHB_AHB_RST, flag, flag);
	udelay(1);
	regmap_update_bits(cam_ahb_gpr,
		REG_MM_AHB_AHB_RST, flag, ~flag);
exit:
#endif

	return ret;
}

static int sprd_dcamdrv_clk_dis(enum dcam_id idx)
{
	if (atomic_read(&s_dcam_total_users) != 1)
		return 0;
#ifdef FPGA_BRINGUP
	if (sprd_dcam_drv_chip_id_get() == SHARKL3)
		clk_disable_unprepare(dcam_axi_eb);
	clk_disable_unprepare(dcam_eb);

	/* set dcam_if clock to default value before power off */
	clk_set_parent(dcam_clk, dcam_clk_default);
	clk_disable_unprepare(dcam_clk);
	clk_set_parent(dcam0_bpc_clk, dcam0_bpc_clk_default);
	clk_disable_unprepare(dcam0_bpc_clk);
#endif

	return 0;
}

static int sprd_dcamdrv_3dnr_me_crop(int len, int max_len,
	int *x_y, int *out_len)
{
	if (len <= max_len) {
		*out_len = len;
		*x_y = 0;
	} else {
		*out_len = max_len;
		*x_y = (len - max_len) / 2;
		if ((*x_y%2) != 0)
			*x_y = *x_y - 1;
	}

	return 0;
}

struct dcam_group *sprd_dcam_drv_group_get(void)
{
	return &s_dcam_group;
}

struct dcam_module *sprd_dcam_drv_module_get(enum dcam_id idx)
{
	if (DCAM_ADDR_INVALID(s_p_dcam_mod[idx])) {
		pr_err("fail to get valid input ptr\n");
		return NULL;
	}
	return s_p_dcam_mod[idx];
}

struct dcam_cap_desc *sprd_dcam_drv_cap_get(enum dcam_id idx)
{
	if (DCAM_ADDR_INVALID(s_p_dcam_mod[idx])) {
		pr_err("fail to get valid input ptr\n");
		return NULL;
	}
	return &s_p_dcam_mod[idx]->dcam_cap;
}

struct dcam_path_desc *sprd_dcam_drv_full_path_get(enum dcam_id idx)
{
	if (DCAM_ADDR_INVALID(s_p_dcam_mod[idx])) {
		pr_err("fail to get valid input ptr\n");
		return NULL;
	}
	return &s_p_dcam_mod[idx]->full_path;
}

struct dcam_path_desc *sprd_dcam_drv_bin_path_get(enum dcam_id idx)
{
	if (DCAM_ADDR_INVALID(s_p_dcam_mod[idx])) {
		pr_err("fail to get valid input ptr\n");
		return NULL;
	}
	return &s_p_dcam_mod[idx]->bin_path;
}

struct dcam_fetch_desc *sprd_dcam_drv_fetch_get(enum dcam_id idx)
{
	if (DCAM_ADDR_INVALID(s_p_dcam_mod[idx])) {
		pr_err("fail to get valid input ptr\n");
		return NULL;
	}
	return &s_p_dcam_mod[idx]->dcam_fetch;
}

void sprd_dcam_drv_glb_reg_awr(enum dcam_id idx, unsigned long addr,
			uint32_t val, uint32_t reg_id)
{
	unsigned long flag = 0;

	switch (reg_id) {
	case DCAM_CFG_REG:
		spin_lock_irqsave(&dcam_glb_reg_cfg_lock[idx], flag);
		DCAM_REG_WR(idx, addr, DCAM_REG_RD(idx, addr) & (val));
		spin_unlock_irqrestore(&dcam_glb_reg_cfg_lock[idx], flag);
		break;
	case DCAM_CONTROL_REG:
		spin_lock_irqsave(&dcam_glb_reg_control_lock[idx], flag);
		DCAM_REG_WR(idx, addr, DCAM_REG_RD(idx, addr) & (val));
		spin_unlock_irqrestore(&dcam_glb_reg_control_lock[idx], flag);
		break;
	case DCAM_INIT_MASK_REG:
		spin_lock_irqsave(&dcam_glb_reg_mask_lock[idx], flag);
		DCAM_REG_WR(idx, addr, DCAM_REG_RD(idx, addr) & (val));
		spin_unlock_irqrestore(&dcam_glb_reg_mask_lock[idx], flag);
		break;
	case DCAM_INIT_CLR_REG:
		spin_lock_irqsave(&dcam_glb_reg_clr_lock[idx], flag);
		DCAM_REG_WR(idx, addr, DCAM_REG_RD(idx, addr) & (val));
		spin_unlock_irqrestore(&dcam_glb_reg_clr_lock[idx], flag);
		break;
	case DCAM_AHBM_STS_REG:
		spin_lock_irqsave(&dcam_glb_reg_ahbm_sts_lock[idx], flag);
		REG_WR(addr, REG_RD(addr) & (val));
		spin_unlock_irqrestore(&dcam_glb_reg_ahbm_sts_lock[idx], flag);
		break;
	case DCAM_AXIM_REG:
		spin_lock_irqsave(&dcam_glb_reg_ahbm_sts_lock[idx], flag);
		DCAM_AXIM_WR(addr, DCAM_AXIM_RD(addr) & (val));
		spin_unlock_irqrestore(&dcam_glb_reg_ahbm_sts_lock[idx], flag);
		break;
	case DCAM_ENDIAN_REG:
		spin_lock_irqsave(&dcam_glb_reg_endian_lock[idx], flag);
		DCAM_REG_WR(idx, addr, DCAM_REG_RD(idx, addr) & (val));
		spin_unlock_irqrestore(&dcam_glb_reg_endian_lock[idx], flag);
		break;
	default:
		DCAM_REG_WR(idx, addr, DCAM_REG_RD(idx, addr) & (val));
		break;
	}
}

void sprd_dcam_drv_glb_reg_owr(enum dcam_id idx, unsigned long addr,
			uint32_t val, uint32_t reg_id)
{
	unsigned long flag = 0;

	switch (reg_id) {
	case DCAM_CFG_REG:
		spin_lock_irqsave(&dcam_glb_reg_cfg_lock[idx], flag);
		DCAM_REG_WR(idx, addr, DCAM_REG_RD(idx, addr) | (val));
		spin_unlock_irqrestore(&dcam_glb_reg_cfg_lock[idx], flag);
		break;
	case DCAM_CONTROL_REG:
		spin_lock_irqsave(&dcam_glb_reg_control_lock[idx], flag);
		DCAM_REG_WR(idx, addr, DCAM_REG_RD(idx, addr) | (val));
		spin_unlock_irqrestore(&dcam_glb_reg_control_lock[idx], flag);
		break;
	case DCAM_INIT_MASK_REG:
		spin_lock_irqsave(&dcam_glb_reg_mask_lock[idx], flag);
		DCAM_REG_WR(idx, addr, DCAM_REG_RD(idx, addr) | (val));
		spin_unlock_irqrestore(&dcam_glb_reg_mask_lock[idx], flag);
		break;
	case DCAM_INIT_CLR_REG:
		spin_lock_irqsave(&dcam_glb_reg_clr_lock[idx], flag);
		DCAM_REG_WR(idx, addr, DCAM_REG_RD(idx, addr) | (val));
		spin_unlock_irqrestore(&dcam_glb_reg_clr_lock[idx], flag);
		break;
	case DCAM_AHBM_STS_REG:
		spin_lock_irqsave(&dcam_glb_reg_ahbm_sts_lock[idx], flag);
		REG_WR(addr, REG_RD(addr) | (val));
		spin_unlock_irqrestore(&dcam_glb_reg_ahbm_sts_lock[idx], flag);
		break;
	case DCAM_AXIM_REG:
		spin_lock_irqsave(&dcam_glb_reg_ahbm_sts_lock[idx], flag);
		DCAM_AXIM_WR(addr, DCAM_AXIM_RD(addr) | (val));
		spin_unlock_irqrestore(&dcam_glb_reg_ahbm_sts_lock[idx], flag);
		break;
	case DCAM_ENDIAN_REG:
		spin_lock_irqsave(&dcam_glb_reg_endian_lock[idx], flag);
		DCAM_REG_WR(idx, addr, DCAM_REG_RD(idx, addr) | (val));
		spin_unlock_irqrestore(&dcam_glb_reg_endian_lock[idx], flag);
		break;
	default:
		DCAM_REG_WR(idx, addr, DCAM_REG_RD(idx, addr) | (val));
		break;
	}
}

void sprd_dcam_drv_glb_reg_mwr(enum dcam_id idx,
	unsigned long addr, uint32_t mask, uint32_t val,
	uint32_t reg_id)
{
	unsigned long flag = 0;
	uint32_t tmp = 0;

	switch (reg_id) {
	case DCAM_CFG_REG:
		spin_lock_irqsave(&dcam_glb_reg_cfg_lock[idx], flag);
		tmp = DCAM_REG_RD(idx, addr);
		tmp &= ~(mask);
		DCAM_REG_WR(idx, addr, tmp | ((mask) & (val)));
		spin_unlock_irqrestore(&dcam_glb_reg_cfg_lock[idx], flag);
		break;
	case DCAM_CONTROL_REG:
		spin_lock_irqsave(&dcam_glb_reg_control_lock[idx], flag);
		tmp = DCAM_REG_RD(idx, addr);
		tmp &= ~(mask);
		DCAM_REG_WR(idx, addr, tmp | ((mask) & (val)));
		spin_unlock_irqrestore(&dcam_glb_reg_control_lock[idx], flag);
		break;
	case DCAM_INIT_MASK_REG:
		spin_lock_irqsave(&dcam_glb_reg_mask_lock[idx], flag);
		tmp = DCAM_REG_RD(idx, addr);
		tmp &= ~(mask);
		DCAM_REG_WR(idx, addr, tmp | ((mask) & (val)));
		spin_unlock_irqrestore(&dcam_glb_reg_mask_lock[idx], flag);
		break;
	case DCAM_INIT_CLR_REG:
		spin_lock_irqsave(&dcam_glb_reg_clr_lock[idx], flag);
		tmp = DCAM_REG_RD(idx, addr);
		tmp &= ~(mask);
		DCAM_REG_WR(idx, addr, tmp | ((mask) & (val)));
		spin_unlock_irqrestore(&dcam_glb_reg_clr_lock[idx], flag);
		break;
	case DCAM_AHBM_STS_REG:
		spin_lock_irqsave(&dcam_glb_reg_ahbm_sts_lock[idx], flag);
		tmp = REG_RD(addr);
		tmp &= ~(mask);
		REG_WR(addr, tmp | ((mask) & (val)));
		spin_unlock_irqrestore(&dcam_glb_reg_ahbm_sts_lock[idx], flag);
		break;
	case DCAM_AXIM_REG:
		spin_lock_irqsave(&dcam_glb_reg_ahbm_sts_lock[idx], flag);
		tmp = DCAM_AXIM_RD(addr);
		tmp &= ~(mask);
		DCAM_AXIM_WR(addr, tmp | ((mask) & (val)));
		spin_unlock_irqrestore(&dcam_glb_reg_ahbm_sts_lock[idx], flag);
		break;

	case DCAM_ENDIAN_REG:
		spin_lock_irqsave(&dcam_glb_reg_endian_lock[idx], flag);
		tmp = DCAM_REG_RD(idx, addr);
		tmp &= ~(mask);
		DCAM_REG_WR(idx, addr, tmp | ((mask) & (val)));
		spin_unlock_irqrestore(&dcam_glb_reg_endian_lock[idx], flag);
		break;
	default:
		pr_err("DCAM%d: fail to wr no global register 0x%0x:\n",
			idx, reg_id);
		break;
	}
}

void sprd_dcam_drv_force_copy(enum dcam_id idx,
	enum camera_copy_id copy_id)
{
	uint32_t reg_val = 0;

	if (copy_id & CAP_COPY)
		reg_val |= BIT_0;
	if (copy_id & COEF_COPY)
		reg_val |= BIT_4;
	if (copy_id & RDS_COPY)
		reg_val |= BIT_6;
	if (copy_id & FULL_COPY)
		reg_val |= BIT_8;
	if (copy_id & BIN_COPY)
		reg_val |= BIT_10;
	if (copy_id & PDAF_COPY)
		reg_val |= BIT_12;
	if (copy_id & VCH2_COPY)
		reg_val |= BIT_14;
	if (copy_id & VCH3_COPY)
		reg_val |= BIT_16;

	reg_val = 0x15551;
	sprd_dcam_drv_glb_reg_mwr(idx, DCAM_CONTROL, reg_val,
			reg_val, DCAM_CONTROL_REG);
}

void sprd_dcam_drv_auto_copy(enum dcam_id idx, enum camera_copy_id copy_id)
{
	uint32_t reg_val = 0;

	if (copy_id & CAP_COPY)
		reg_val |= BIT_1;
	if (copy_id & COEF_COPY)
		reg_val |= BIT_5;
	if (copy_id & RDS_COPY)
		reg_val |= BIT_7;
	if (copy_id & FULL_COPY)
		reg_val |= BIT_9;
	if (copy_id & BIN_COPY)
		reg_val |= BIT_11;
	if (copy_id & PDAF_COPY)
		reg_val |= BIT_13;
	if (copy_id & VCH2_COPY)
		reg_val |= BIT_15;
	if (copy_id & VCH3_COPY)
		reg_val |= BIT_17;
	reg_val = 0x2aaa2;
	sprd_dcam_drv_glb_reg_mwr(idx, DCAM_CONTROL, reg_val,
			reg_val, DCAM_CONTROL_REG);
}

void sprd_dcam_drv_irq_mask_en(enum dcam_id idx)
{
	sprd_dcam_drv_glb_reg_owr(idx, DCAM_INT_CLR,
		DCAM_IRQ_LINE_MASK, DCAM_INIT_CLR_REG);
	sprd_dcam_drv_glb_reg_owr(idx, DCAM_INT_EN,
		DCAM_IRQ_LINE_MASK, DCAM_INIT_MASK_REG);
}

void sprd_dcam_drv_irq_mask_dis(enum dcam_id idx)
{
	sprd_dcam_drv_glb_reg_awr(idx, DCAM_INT_EN,
		~DCAM_IRQ_LINE_MASK, DCAM_INIT_MASK_REG);
	sprd_dcam_drv_glb_reg_owr(idx, DCAM_INT_CLR,
		DCAM_IRQ_LINE_MASK, DCAM_INIT_CLR_REG);
}

static void reg_owr(unsigned int addr, unsigned int val)
{
	void __iomem *io_tmp = NULL;
	u32 tmp = 0;

	io_tmp = ioremap_nocache(addr, 0x4);
	tmp = __raw_readl(io_tmp);
	 __raw_writel(tmp|val, io_tmp);
	mb(); /* asm/barrier.h */
	val = __raw_readl(io_tmp);
	iounmap(io_tmp);
}

static void reg_awr(unsigned int addr, unsigned int val)
{
	void __iomem *io_tmp = NULL;
	unsigned int tmp = 0;

	io_tmp = ioremap_nocache(addr, 0x4);
	tmp = __raw_readl(io_tmp);
	__raw_writel(tmp&val, io_tmp);
	mb(); /* asm/barrier.h */
	val = __raw_readl(io_tmp);
	iounmap(io_tmp);
}

int sprd_dcam_drv_reset(enum dcam_id idx, int is_irq)
{
#ifdef FPGA_BRINGUP
	int i = 0;
	uint32_t time_out = 0, flag = 0;
#else
	uint32_t time_out = 0;
#endif
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;

	if (atomic_read(&s_dcam_total_users) == 1) {
		/* firstly, stop AXI writing */
		sprd_dcam_drv_glb_reg_owr(idx, DCAM_AXIM_CTRL, BIT_24 | BIT_23,
			DCAM_AXIM_REG);

		/* then wait for AHB busy cleared */
		while (++time_out < DCAM_AXI_STOP_TIMEOUT) {
			if (0 == (DCAM_AXIM_RD(DCAM_AXIM_DBG_STS) & BIT_12))
				break;
		}

		if (time_out >= DCAM_AXI_STOP_TIMEOUT) {
			pr_err("fail to reset DCAM%d timeout, axim status 0x%x\n",
				idx, DCAM_AXIM_RD(DCAM_AXIM_DBG_STS));
		}
	}
#ifdef FPGA_BRINGUP
	if (idx == DCAM_ID_0)
		flag = BIT_MM_AHB_DCAM0_SOFT_RST;
	else if (idx == DCAM_ID_1)
		flag = BIT_MM_AHB_DCAM1_SOFT_RST;
	if (sprd_dcam_drv_chip_id_get() == SHARKL3 && idx == DCAM_ID_2)
		flag = DCAM2_SOFT_RST;

	if (is_irq && atomic_read(&s_dcam_total_users) == 1)
		flag = BIT_MM_AHB_DCAM_ALL_SOFT_RST;

	regmap_update_bits(cam_ahb_gpr,
		REG_MM_AHB_AHB_RST, flag, flag);
	udelay(1);
	regmap_update_bits(cam_ahb_gpr,
		REG_MM_AHB_AHB_RST, flag, ~flag);

	for (i = 0x200; i < 0x400; i += 4)
		DCAM_REG_WR(idx, i, 0);
	DCAM_REG_MWR(idx, ISP_LENS_LOAD_EB, BIT_0, 1);
	DCAM_REG_MWR(idx, ISP_AEM_PARAM, BIT_0, 1);
	DCAM_REG_MWR(idx, ISP_BPC_GC_CFG, 0x07, 0x01);
	DCAM_REG_MWR(idx, ISP_BPC_PARAM, 0x0F, 0x0F);
	DCAM_REG_MWR(idx, ISP_GRGB_CTRL, BIT_0, 1);
	DCAM_REG_MWR(idx, ISP_RAW_AFM_FRAM_CTRL, BIT_0, 1);
	DCAM_REG_MWR(idx, ISP_ANTI_FLICKER_FRAM_CTRL, BIT_0, 1);
	DCAM_REG_MWR(idx, DCAM_NR3_PARA1, BIT_0, 1);
	DCAM_REG_MWR(idx, ISP_RGBG_PARAM, BIT_0, 1);
	DCAM_REG_MWR(idx, ISP_AWBC_PARAM, BIT_0, 1);
	sprd_dcam_drv_glb_reg_awr(idx, DCAM_CFG, ~0x3f, DCAM_CFG_REG);
#endif
	reg_owr(0x62200004, BIT_23);
	usleep_range(300, 350);

	reg_awr(0x62200004, ~BIT_23);
	usleep_range(300, 350);
	/* the end, enable AXI writing */
	if (atomic_read(&s_dcam_total_users) == 1)
		sprd_dcam_drv_glb_reg_awr(idx,
		DCAM_AXIM_CTRL, ~(BIT_24 | BIT_23),
			DCAM_AXIM_REG);

	return -rtn;
}

int sprd_dcam_drv_path_pause(enum dcam_id idx, uint32_t channel_id)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc *path = NULL;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod[idx])) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	if (channel_id == CAMERA_FULL_PATH) {
#if DCAM_FULL_PATH_PAUSE
		DCAM_REG_MWR(idx, DCAM_FULL_CFG, BIT_0, 0);
		sprd_dcam_drv_auto_copy(idx, FULL_COPY);
		path = &s_p_dcam_mod[idx]->full_path;
#else
		return -rtn;
#endif
	} else {
		DCAM_REG_MWR(idx, DCAM_CAM_BIN_CFG, BIT_0, 0);
		sprd_dcam_drv_auto_copy(idx, BIN_COPY);
		path = &s_p_dcam_mod[idx]->bin_path;
	}
	path->status = DCAM_ST_PAUSE;
	return -rtn;
}

int sprd_dcam_drv_path_resume(enum dcam_id idx, uint32_t channel_id)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc *path = NULL;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod[idx])) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	if (channel_id == CAMERA_FULL_PATH) {
#if DCAM_FULL_PATH_PAUSE
		DCAM_REG_MWR(idx, DCAM_FULL_CFG, BIT_0, 1);
		sprd_dcam_drv_auto_copy(idx, FULL_COPY);
		path = &s_p_dcam_mod[idx]->full_path;
#else
		return -rtn;
#endif
	} else {
		DCAM_REG_MWR(idx, DCAM_CAM_BIN_CFG, BIT_0, 1);
		path = &s_p_dcam_mod[idx]->bin_path;
	}
	path->status = DCAM_ST_START;
	return -rtn;
}

static int sprd_dcamdrv_bypass_ispblock_for_yuv_sensor(enum dcam_id idx)
{
	if (DCAM_ADDR_INVALID(s_p_dcam_mod[idx])) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
#ifdef FPGA_BRINGUP
	DCAM_REG_MWR(idx, DCAM_MIPI_CAP_CFG, BIT_18, BIT_18);
	DCAM_REG_MWR(idx, ISP_RGBG_PARAM, BIT_0, BIT_0);
	DCAM_REG_MWR(idx, ISP_RGBG_PARAM, 0xFFFF << 16, 0xFFFF << 16);
	DCAM_REG_MWR(idx, ISP_LENS_LOAD_EB, BIT_0, BIT_0);
	DCAM_REG_MWR(idx, ISP_AEM_PARAM, BIT_0, 1);
	DCAM_REG_MWR(idx, ISP_ANTI_FLICKER_PARAM0, BIT_0, BIT_0);
	DCAM_REG_MWR(idx, ISP_ANTI_FLICKER_FRAM_CTRL, BIT_0, 1);
	DCAM_REG_MWR(idx, ISP_AWBC_PARAM, BIT_0, BIT_0);
	DCAM_REG_MWR(idx, ISP_BPC_PARAM, 0x0F, 0x0F);
	DCAM_REG_MWR(idx, ISP_GRGB_CTRL, BIT_0, BIT_0);
	DCAM_REG_MWR(idx, ISP_RAW_AFM_FRAM_CTRL, BIT_0, BIT_0);
	DCAM_REG_MWR(idx, DCAM_NR3_PARA1, BIT_0, BIT_0);
#endif

	return 0;
}

static int sprd_dcamdrv_bypass_ispblock(enum dcam_id idx)
{

	/*bypass pre-binning*/
	DCAM_REG_MWR(idx, DCAM_BAYER_INFO_CFG, BIT_0, 1);

	/*bypass blc*/
	DCAM_REG_MWR(idx, DCAM_BLC_PARA_R_B,
		BIT_31,
		1 << 31);

	/*bypass RGBG*/
	DCAM_REG_MWR(idx, ISP_RGBG_YRANDOM_PARAMETER0,
		BIT_1,
		1 << 1);
	DCAM_REG_MWR(idx, ISP_RGBG_YRANDOM_PARAMETER0,
		BIT_0,
		1);

	/*bypas ppi*/
	DCAM_REG_MWR(idx, ISP_PPI_PARAM,
		BIT_0,
		1);

	/*bypass LSCM*/
	DCAM_REG_MWR(idx, DCAM_LSCM_FRM_CTRL0,
		BIT_0,
		1);

	/*bypass lsc*/
	DCAM_REG_MWR(idx, DCAM_LENS_LOAD_ENABLE,
		BIT_0,
		1);

	/*bypass aem*/
	DCAM_REG_MWR(idx, DCAM_AEM_FRM_CTRL0,
		BIT_0,
		1);

	/*bypass hist*/
	DCAM_REG_MWR(idx, DCAM_HIST_FRM_CTRL0,
		BIT_0,
		1);

	/*bypass bayer2y*/
	DCAM_REG_MWR(idx, ISP_AFL_PARAM0,
	BIT_1,
	1 << 1);

	/*bypass afl*/
	DCAM_REG_MWR(idx, ISP_AFL_FRM_CTRL0,
		BIT_0,
		1);

	/*bypass Crop0*/
	DCAM_REG_MWR(idx, DCAM_CROP0_START, BIT_31, 0 << 31);

	/*bypass awbc*/
	DCAM_REG_MWR(idx, ISP_AWBC_GAIN0,
		BIT_31,
		1 << 31);

	/*bypass bpc*/
	DCAM_REG_MWR(idx, ISP_BPC_PARAM,
		BIT_0,
		1);

	/*bypass afm*/
	DCAM_REG_MWR(idx, ISP_AFM_FRM_CTRL,
		BIT_0,
		1);

	/*bypass nr3*/
	DCAM_REG_MWR(idx, DCAM_NR3_FAST_ME_PARAM,
		BIT_0,
		1);

	/*bypass GTM*/
	DCAM_REG_MWR(idx, DCAM_GTM_GLB_CTRL,
		BIT_0,
		0);

	/*fbc bypass*/
	DCAM_REG_MWR(idx, DCAM_FBC_CTRL,
		BIT_0,
		0);

	return 0;
}
int sprd_dcam_drv_start(enum dcam_id idx)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	uint32_t cap_en = 0;
	unsigned int reg_val = 0;
	unsigned long image_vc = 0;
	unsigned long image_data_type = IMG_TYPE_RAW;
	unsigned long image_mode = 1;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod[idx])) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	sprd_dcam_full_path_start(idx);
	sprd_dcam_bin_path_start(idx);

	/* set statis buf before stream on */
	rtn = sprd_cam_statistic_buf_set(
		&s_p_dcam_mod[idx]->statis_module_info);
	if (rtn) {
		pr_err("fail to start isp set statis buf\n");
		return -rtn;
	}

	cap_en = DCAM_REG_RD(idx, DCAM_MIPI_CAP_CFG) & BIT_0;

	if (s_p_dcam_mod[idx]->full_path.output_format == DCAM_YUV420) {
		sprd_dcamdrv_bypass_ispblock_for_yuv_sensor(idx);
		image_data_type = IMG_TYPE_YUV;
	}
	sprd_dcamdrv_bypass_ispblock(idx);
	reg_val = ((image_vc & 0x3) << 16) |
		((image_data_type & 0x3F) << 8) | (image_mode & 0x3);

	if (cap_en == 0) {
		DCAM_REG_WR(idx, DCAM_IMAGE_CONTROL, reg_val);

		sprd_dcam_drv_irq_mask_en(idx);
		if (s_p_dcam_mod[idx]->full_path.valid
			&& (!s_p_dcam_mod[idx]->bin_path.valid)
			&& (s_p_dcam_mod[idx]->full_path.output_format
				== DCAM_RAWRGB)
			&&  idx != DCAM_ID_2) {

			DCAM_REG_WR(idx, DCAM_BIN_BASE_WADDR0,
				s_p_dcam_mod[idx]->full_path.reserved_frame
				.buf_info.iova[0]);
			sprd_dcam_drv_glb_reg_awr(idx, DCAM_INT_EN,
				~(1 << DCAM_BIN_PATH_TX_DONE),
				DCAM_INIT_MASK_REG);
		}

		/* Cap force copy */
		sprd_dcam_drv_force_copy(idx, ALL_COPY);
		/* Cap Enable */
		DCAM_REG_MWR(idx, DCAM_MIPI_CAP_CFG, BIT_0, 1);
		pr_info("mipi_cap_start");
	}

	if (s_p_dcam_mod[idx]->need_nr3) {
		struct camera_size size = {0};

		size.w = s_p_dcam_mod[idx]->me_param.cap_in_size_w;
		size.h = s_p_dcam_mod[idx]->me_param.cap_in_size_h;
		rtn = sprd_dcam_drv_3dnr_me_set(idx, &size);
		if (rtn) {
			pr_err("fail to get_fast_me param\n");
			return -rtn;
		}
		DCAM_REG_WR(idx, ISP_NR3_WADDR,
			s_p_dcam_mod[idx]->statis_module_info
			.aem_buf_reserved.phy_addr);
	}

	if (sprd_dcam_drv_chip_id_get() == SHARKL3)
		reg_val = (0x0 << 20) | (0xA << 12) | (0x8 << 8) |
					(0xD << 4) | 0xA;
	else
		reg_val = (0x0 << 20) | (0xB << 12) | (0x2 << 8) |
					(0xE << 4) | 0xB;
	sprd_dcam_drv_glb_reg_mwr(idx,
			DCAM_AXIM_CTRL,
			DCAM_AXIM_AQOS_MASK,
			reg_val,
			DCAM_AXIM_REG);

	return -rtn;
}

int sprd_dcam_drv_stop(enum dcam_id idx, int is_irq)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	unsigned long flag;
	int32_t ret = 0;
	int time_out = 5000;
	struct dcam_path_desc *bin_path = sprd_dcam_drv_bin_path_get(idx);

	if (DCAM_ADDR_INVALID(s_p_dcam_mod[idx])) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	s_p_dcam_mod[idx]->state |= DCAM_STATE_QUICKQUIT;
	s_p_dcam_mod[idx]->full_path.status = DCAM_ST_STOP;
	s_p_dcam_mod[idx]->bin_path.status = DCAM_ST_STOP;

	if (!is_irq)
		spin_lock_irqsave(&dcam_lock[idx], flag);

	sprd_dcam_drv_glb_reg_owr(idx, DCAM_PATH_STOP, 0x3F, DCAM_CONTROL_REG);
	udelay(1000);
#ifdef FPGA_BRINGUP
	/*stop all data path*/
	sprd_dcam_drv_glb_reg_awr(idx, DCAM_CFG, ~0x3f, DCAM_CFG_REG);
#endif
	/* wait for AHB path busy cleared */
	while (time_out) {
		ret = DCAM_REG_RD(idx, DCAM_PATH_BUSY) & 0xFFF;
		if (!ret)
			break;
		time_out--;
	}
	if (!time_out)
		pr_info("DCAM%d: stop path timeout 0x%x\n", idx, ret);

	if (!is_irq)
		spin_unlock_irqrestore(&dcam_lock[idx], flag);

	rtn = sprd_dcam_drv_reset(idx, is_irq);
	if (!is_irq)
		sprd_dcam_drv_irq_mask_dis(idx);
	ret = sprd_cam_queue_buf_clear(&bin_path->coeff_queue);

	s_p_dcam_mod[idx]->need_nr3 = 0;
	s_p_dcam_mod[idx]->state &= ~DCAM_STATE_QUICKQUIT;

	return -rtn;
}

int sprd_dcam_drv_dev_init(enum dcam_id idx)
{
	int ret = 0;

	s_p_dcam_mod[idx] = vzalloc(sizeof(struct dcam_module));
	if (DCAM_ADDR_INVALID(s_p_dcam_mod[idx])) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	sprd_dcam_full_path_init(idx);
	sprd_dcam_bin_path_init(idx);

	ret = sprd_cam_statistic_queue_init(
		&s_p_dcam_mod[idx]->statis_module_info,
		idx, DCAM_DEV_STATIS);
	s_p_dcam_mod[idx]->frame_id = 0;
	s_p_dcam_mod[idx]->id = idx;
	s_dcam_group.dcam[idx] = s_p_dcam_mod[idx];

	return ret;
}

int sprd_dcam_drv_dev_deinit(enum dcam_id idx)
{
	sprd_dcam_full_path_deinit(idx);
	sprd_dcam_bin_path_deinit(idx);

	sprd_cam_statistic_queue_clear(
		&s_p_dcam_mod[idx]->statis_module_info);
	sprd_cam_statistic_queue_deinit(
		&s_p_dcam_mod[idx]->statis_module_info);
	s_p_dcam_mod[idx]->need_nr3 = 0;
	s_p_dcam_mod[idx]->frame_id = 0;

	if (DCAM_ADDR_INVALID(s_p_dcam_mod[idx])) {
		pr_err("fail to get DCAM%d valid addr %p\n",
			idx, s_p_dcam_mod[idx]);
	} else {
		vfree(s_p_dcam_mod[idx]);
		s_p_dcam_mod[idx] = NULL;
		s_dcam_group.dcam[idx] = NULL;
	}

	return 0;
}

int sprd_dcam_drv_path_unmap(enum dcam_id idx)
{
	int ret = 0;

	ret = sprd_dcam_full_path_unmap(idx);
	if (unlikely(ret)) {
		pr_err("fail to unmap dcam full path\n");
		return -EFAULT;
	}
	ret = sprd_dcam_bin_path_unmap(idx);
	if (unlikely(ret)) {
		pr_err("fail to unmap dcam bin path\n");
		return -EFAULT;
	}
	sprd_cam_statistic_unmap(
		&s_p_dcam_mod[idx]->statis_module_info.img_statis_buf);

	return ret;
}

void sprd_dcam_drv_path_clear(enum dcam_id idx)
{
	struct dcam_module *dcam_dev
		= sprd_dcam_drv_module_get(idx);

	sprd_dcam_bin_path_clear(idx);
	sprd_dcam_full_path_clear(idx);
	sprd_cam_statistic_queue_clear(
		&dcam_dev->statis_module_info);
	memset(&dcam_dev->fast_me, 0, sizeof(dcam_dev->fast_me));
	memset(&dcam_dev->me_param, 0, sizeof(dcam_dev->me_param));
	dcam_dev->frame_id = 0;
	dcam_dev->time_index = 0;
	memset(&dcam_dev->time, 0, sizeof(dcam_dev->time));
	memset(&dcam_dev->dual_info, 0, sizeof(dcam_dev->dual_info));
}

int sprd_dcam_drv_module_en(enum dcam_id idx)
{
	int ret = 0;

	mutex_lock(&dcam_module_sema[idx]);
	if (atomic_inc_return(&s_dcam_users[idx]) == 1) {
		ret = sprd_cam_pw_on();
		if (ret != 0) {
			pr_err("fail to power on sprd cam\n");
			goto cam_pw_on_exit;
		}
		sprd_cam_domain_eb();
		sprd_dcamdrv_clk_en(idx);
		sprd_dcam_drv_reset(idx, 0);

		ret = sprd_dcam_int_irq_request(idx,
			s_p_dcam_mod[idx]);
		if (ret) {
			pr_err("fail to install IRQ %d\n", ret);
			goto cam_irq_exit;
		}
	}
	atomic_inc(&s_dcam_total_users);
	mutex_unlock(&dcam_module_sema[idx]);

	return ret;

cam_irq_exit:
#ifndef FPGA_BRINGUP
	atomic_dec_return(&s_dcam_users[idx]);
	mutex_unlock(&dcam_module_sema[idx]);
	return 0;
#endif
	sprd_dcamdrv_clk_dis(idx);
	sprd_cam_domain_disable();
cam_pw_on_exit:
	atomic_dec_return(&s_dcam_users[idx]);
	mutex_unlock(&dcam_module_sema[idx]);
	return ret;
}

int sprd_dcam_drv_module_dis(enum dcam_id idx)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;

	if (atomic_read(&s_dcam_users[idx]) == 0)
		return rtn;

	mutex_lock(&dcam_module_sema[idx]);
	if (atomic_dec_return(&s_dcam_users[idx]) == 0) {
		sprd_dcam_int_irq_free(idx, s_p_dcam_mod[idx]);
		sprd_dcamdrv_clk_dis(idx);
		sprd_cam_domain_disable();
		rtn = sprd_cam_pw_off();
		if (rtn != 0) {
			pr_err("fail to power off sprd cam\n");
			mutex_unlock(&dcam_module_sema[idx]);
			return rtn;
		}
	}
	atomic_dec(&s_dcam_total_users);
	mutex_unlock(&dcam_module_sema[idx]);

	return rtn;
}

int sprd_dcam_drv_init(struct platform_device *p_dev)
{
	int i = 0;

	s_dcam_pdev = p_dev;
	for (i = 0; i < s_dcam_count; i++) {
		atomic_set(&s_dcam_users[i], 0);
		s_p_dcam_mod[i] = NULL;

		mutex_init(&dcam_module_sema[i]);

		dcam_lock[i] = __SPIN_LOCK_UNLOCKED(dcam_lock);
		dcam_glb_reg_cfg_lock[i] =
			__SPIN_LOCK_UNLOCKED(dcam_glb_reg_cfg_lock);
		dcam_glb_reg_control_lock[i] =
			__SPIN_LOCK_UNLOCKED(dcam_glb_reg_control_lock);
		dcam_glb_reg_mask_lock[i] =
			__SPIN_LOCK_UNLOCKED(dcam_glb_reg_mask_lock);
		dcam_glb_reg_clr_lock[i] =
			__SPIN_LOCK_UNLOCKED(dcam_glb_reg_clr_lock);
		dcam_glb_reg_ahbm_sts_lock[i] =
			__SPIN_LOCK_UNLOCKED(dcam_glb_reg_ahbm_sts_lock);
		dcam_glb_reg_endian_lock[i] =
			__SPIN_LOCK_UNLOCKED(dcam_glb_reg_endian_lock);
	}
	atomic_set(&s_dcam_total_users, 0);

	return 0;
}

void sprd_dcam_drv_deinit(void)
{
	int i = 0;

	for (i = 0; i < s_dcam_count; i++) {
		atomic_set(&s_dcam_users[i], 0);
		s_p_dcam_mod[i] = NULL;
		s_dcam_irq[i] = 0;

		mutex_init(&dcam_module_sema[i]);

		dcam_lock[i] = __SPIN_LOCK_UNLOCKED(dcam_lock);
		dcam_glb_reg_cfg_lock[i] =
			__SPIN_LOCK_UNLOCKED(dcam_glb_reg_cfg_lock);
		dcam_glb_reg_control_lock[i] =
			__SPIN_LOCK_UNLOCKED(dcam_glb_reg_control_lock);
		dcam_glb_reg_mask_lock[i] =
			__SPIN_LOCK_UNLOCKED(dcam_glb_reg_mask_lock);
		dcam_glb_reg_clr_lock[i] =
			__SPIN_LOCK_UNLOCKED(dcam_glb_reg_clr_lock);
		dcam_glb_reg_ahbm_sts_lock[i] =
			__SPIN_LOCK_UNLOCKED(dcam_glb_reg_ahbm_sts_lock);
		dcam_glb_reg_endian_lock[i] =
			__SPIN_LOCK_UNLOCKED(dcam_glb_reg_endian_lock);
	}
	atomic_set(&s_dcam_total_users, 0);
}

int sprd_dcam_drv_chip_id_get(void)
{
	enum chip_id idx = 0;

	if (chip_id0 == KL3 && chip_id1 == SHAR)
		idx = SHARKL3;
	else
		idx = SHARKLEP;

	return idx;
}

extern int csi_init_reg_base(unsigned long reg_base, int32_t idx);

int sprd_dcam_drv_dt_parse(struct platform_device *pdev, uint32_t *dcam_count)
{
	int i = 0, ret = 0;
	uint32_t count = 0;
	void __iomem *reg_base;
	/*struct device_node *iommu_dcam_node = NULL;*/
	struct device_node *dn = NULL;
	struct device *dev = NULL;
	int index = 0;
	unsigned long csi_reg_base;

	if (!pdev || !dcam_count) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	dev = &pdev->dev;
	dn = dev->of_node;

	if (of_property_read_u32_index(dn, "sprd,dcam-count", 0, &count)) {
		pr_err("fail to parse the property of sprd,dcam-count\n");
		return -EINVAL;
	}

	s_dcam_count = count;
	*dcam_count = count;

#ifdef FPGA_BRINGUP

	dcam_eb = of_clk_get_by_name(dn, "dcam_eb");
	if (IS_ERR_OR_NULL(dcam_eb)) {
		pr_err("fail to get dcam_eb\n");
		return PTR_ERR(dcam_eb);
	}

	dcam_clk = of_clk_get_by_name(dn, "dcam_clk");
	if (IS_ERR_OR_NULL(dcam_clk)) {
		pr_err("fail to get dcam_clk\n");
		return PTR_ERR(dcam_clk);
	}

	dcam_clk_parent = of_clk_get_by_name(dn, "dcam_clk_parent");
	if (IS_ERR_OR_NULL(dcam_clk_parent)) {
		pr_err("fail to get dcam_clk_parent\n");
		return PTR_ERR(dcam_clk_parent);
	}

	dcam_clk_default = clk_get_parent(dcam_clk);
	if (IS_ERR_OR_NULL(dcam_clk_default)) {
		pr_err("fail to get dcam_clk_default\n");
		return PTR_ERR(dcam_clk_default);
	}

	dcam0_bpc_clk = of_clk_get_by_name(dn, "dcam_bpc_clk");
	if (IS_ERR_OR_NULL(dcam0_bpc_clk)) {
		pr_err("fail to get dcam0_bpc_clk\n");
		return PTR_ERR(dcam0_bpc_clk);
	}

	dcam0_bpc_clk_parent = of_clk_get_by_name(dn, "dcam_bpc_clk_parent");
	if (IS_ERR_OR_NULL(dcam0_bpc_clk_parent)) {
		pr_err("fail to get dcam0_bpc_clk_parent\n");
		return PTR_ERR(dcam0_bpc_clk_parent);
	}

	dcam0_bpc_clk_default = clk_get_parent(dcam0_bpc_clk);
	if (IS_ERR_OR_NULL(dcam0_bpc_clk_default)) {
		pr_err("fail to get dcam0_bpc_clk_default\n");
		return PTR_ERR(dcam0_bpc_clk_default);
	}

	cam_ahb_gpr = syscon_regmap_lookup_by_phandle(dn,
		"sprd,cam-ahb-syscon");
	if (IS_ERR_OR_NULL(cam_ahb_gpr))
		return PTR_ERR(cam_ahb_gpr);

	aon_apb = syscon_regmap_lookup_by_phandle(dn,
		"sprd,aon-apb-syscon");
	if (IS_ERR_OR_NULL(aon_apb))
		return PTR_ERR(aon_apb);

	ret = regmap_read(aon_apb, REG_AON_APB_AON_CHIP_ID0, &chip_id0);
	if (unlikely(ret))
		goto exit;
	ret = regmap_read(aon_apb, REG_AON_APB_AON_CHIP_ID1, &chip_id1);
	if (unlikely(ret))
		goto exit;
	if (sprd_dcam_drv_chip_id_get() == SHARKL3) {
		dcam_axi_eb = of_clk_get_by_name(dn, "dcam_axi_eb");
		if (IS_ERR_OR_NULL(dcam_axi_eb)) {
			pr_err("fail to get dcam_axi_eb\n");
			return PTR_ERR(dcam_axi_eb);
		}
	}
#endif
	for (i = 0; i < count; i++) {
		s_dcam_irq[i] = irq_of_parse_and_map(dn, i);
		if (s_dcam_irq[i] <= 0) {
			pr_err("fail to get dcam irq %d\n", i);
			return -EFAULT;
		}

		reg_base = of_iomap(dn, i);
		if (!reg_base) {
			pr_err("fail to get dcam reg_base %d\n", i);
			return -ENXIO;
		}
		s_dcam_regbase[i] = (unsigned long)reg_base;

		pr_info("Dcam dts OK! base %lx, irq %d\n", s_dcam_regbase[i],
			s_dcam_irq[i]);
	}

	index = device_property_match_string(dev, "reg-names",
		"axi_ctrl_reg");
	if (index < 0) {
		pr_err("fail to get index %d\n", index);
		/*return -ENODATA;*/
	}
	reg_base = of_iomap(dn, index);
	if (!reg_base) {
		pr_err("fail to get dcam axim_base %d\n", i);
		/*return -ENXIO;*/
	}
	s_dcam_aximbase = (unsigned long)reg_base;
	index = device_property_match_string(dev, "reg-names",
		"csi_reg");
	if (index < 0) {
		pr_err("fail to get index %d\n", index);
		/*return -ENODATA;*/
	}
	reg_base = of_iomap(dn, index);
	if (!reg_base) {
		pr_err("fail to get dcam axim_base %d\n", i);
		/*return -ENXIO;*/
	}
	csi_reg_base = (unsigned long)reg_base;
	csi_init_reg_base(csi_reg_base, 0);
	pr_err("csi_reg_base = 0x%lx\n", csi_reg_base);

	index = device_property_match_string(dev, "reg-names",
		"csi1_reg");
	if (index < 0) {
		pr_err("fail to get index %d\n", index);
		/*return -ENODATA;*/
	}
	reg_base = of_iomap(dn, index);
	if (!reg_base) {
		pr_err("fail to get dcam axim_base %d\n", i);
		/*return -ENXIO;*/
	}
	csi_reg_base = (unsigned long)reg_base;
	csi_init_reg_base(csi_reg_base, 1);
	pr_err("csi1_reg_base = 0x%lx\n", csi_reg_base);

	index = device_property_match_string(dev, "reg-names",
		"csi2_reg");
	if (index < 0) {
		pr_err("fail to get index %d\n", index);
		/*return -ENODATA;*/
	}
	reg_base = of_iomap(dn, index);
	if (!reg_base) {
		pr_err("fail to get dcam axim_base %d\n", i);
		/*return -ENXIO;*/
	}
	csi_reg_base = (unsigned long)reg_base;
	csi_init_reg_base(csi_reg_base, 2);
	pr_err("csi2_reg_base = 0x%lx\n", csi_reg_base);

#ifdef FPGA_BRINGUP
	iommu_dcam_node = of_find_compatible_node(NULL, NULL,
		"sprd,iommuexl3-dcam");
	if (iommu_dcam_node == NULL) {
		pr_err("fail to parse the property of iommu_dcam\n");
		return -EFAULT;
	}
	s_dcam_mmubase = (unsigned long)of_iomap(iommu_dcam_node, 1);
#endif
	return 0;
#ifdef FPGA_BRINGUP
exit:
#endif
	return ret;
}

struct dcam_fast_me_param *sprd_dcam_drv_3dnr_me_param_get(
	enum dcam_id idx)
{
	if (DCAM_ADDR_INVALID(s_p_dcam_mod[idx])) {
		pr_err("fail to get valid input ptr\n");
		return NULL;
	}
	return &s_p_dcam_mod[idx]->me_param;
}

int sprd_dcam_drv_3dnr_me_set(uint32_t idx, void *size)
{
	unsigned int val = 0;
	int roi_start_x = 0, roi_start_y = 0, roi_width = 0, roi_height = 0;
	int nr3_channel_sel = 0, nr3_project_mode = 0;
	struct camera_size *p_size = NULL;
	int roi_width_max = 0;
	int roi_height_max = 0;
	struct dcam_fast_me_param *me_param =
		sprd_dcam_drv_3dnr_me_param_get(idx);

	if (!size) {
		pr_err("fail to nr3_fast_me get valid input ptr\n");
		return -DCAM_RTN_PARA_ERR;
	}

	p_size = (struct camera_size *)size;
	val = DCAM_REG_RD(idx, DCAM_NR3_FAST_ME_PARAM);
	nr3_project_mode = val & 0x3;
	nr3_channel_sel = (val >> 2) & 0x3;

	if (idx == 0) {
		if (nr3_project_mode == 0) {
			roi_width_max = DCAM0_3DNR_ME_WIDTH_MAX / 2;
			roi_height_max = DCAM0_3DNR_ME_HEIGHT_MAX / 2;
		} else {
			roi_width_max = DCAM0_3DNR_ME_WIDTH_MAX;
			roi_height_max = DCAM0_3DNR_ME_HEIGHT_MAX;
		}
	} else {
		if (nr3_project_mode == 0) {
			roi_width_max = DCAM1_3DNR_ME_WIDTH_MAX / 2;
			roi_height_max = DCAM1_3DNR_ME_HEIGHT_MAX / 2;
		} else {
			roi_width_max = DCAM1_3DNR_ME_WIDTH_MAX;
			roi_height_max = DCAM1_3DNR_ME_HEIGHT_MAX;
		}
	}
	sprd_dcamdrv_3dnr_me_crop(p_size->w,
		roi_width_max,
		&roi_start_x,
		&roi_width);
	sprd_dcamdrv_3dnr_me_crop(p_size->h,
		roi_height_max,
		&roi_start_y,
		&roi_height);

	me_param->nr3_bypass = 0;
	me_param->nr3_channel_sel = nr3_channel_sel;
	me_param->nr3_project_mode = nr3_project_mode;
	me_param->roi_start_x = roi_start_x;
	me_param->roi_start_y = roi_start_y;
	me_param->roi_width = roi_width & 0x1FF8;
	me_param->roi_height = (roi_height - 40) & 0x1FF8;

	DCAM_REG_MWR(idx, DCAM_NR3_FAST_ME_PARAM, BIT_0,
		me_param->nr3_bypass);
	val = ((me_param->roi_start_x & 0x1FFF) << 16)
		| (me_param->roi_start_y & 0x1FFF);
	DCAM_REG_MWR(idx, DCAM_NR3_FAST_ME_ROI_PARAM0,
			0x1FFF1FFF, val);
	val = ((me_param->roi_width & 0x1FFF) << 16)
		| (me_param->roi_height & 0x1FFF);
	DCAM_REG_MWR(idx, DCAM_NR3_FAST_ME_ROI_PARAM1,
		0x1FFF1FFF, val);

	return 0;
}

int sprd_dcam_drv_3dnr_fast_me_info_get(enum dcam_id idx,
	uint32_t need_nr3, struct camera_size *size)
{
	if (!size) {
		pr_err("fail to nr3_fast_me get valid input ptr\n");
		return -DCAM_RTN_PARA_ERR;
	}

	sprd_dcam_drv_module_get(idx)->need_nr3 = need_nr3;
	sprd_dcam_drv_module_get(idx)->me_param.cap_in_size_w = size->w;
	sprd_dcam_drv_module_get(idx)->me_param.cap_in_size_h = size->h;

	return 0;
}

void sprd_dcam_drv_4in1_info_get(uint32_t need_4in1)
{
	sprd_dcam_drv_module_get(DCAM_ID_0)->need_4in1 = need_4in1;
	sprd_dcam_drv_module_get(DCAM_ID_1)->need_4in1 = need_4in1;
}

void sprd_dcam_drv_reg_trace(enum dcam_id idx)
{
#ifdef DCAM_DRV_DEBUG
	unsigned long addr = 0;

	pr_info("cb: %pS\n", __builtin_return_address(0));

	pr_info("DCAM%d: Register list\n", idx);
	for (addr = DCAM_IP_REVISION; addr <= DCAM_GTM_HIST_XPTS_126;
		addr += 16) {
		pr_info("0x%03lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			DCAM_REG_RD(idx, addr),
			DCAM_REG_RD(idx, addr + 4),
			DCAM_REG_RD(idx, addr + 8),
			DCAM_REG_RD(idx, addr + 12));
	}
#ifdef FPGA_BRINGUP
	pr_info("AXIM: Register list\n");
	for (addr = DCAM_AXIM_CTRL; addr <= REG_DCAM_IMG_FETCH_RADDR;
		addr += 16) {
		pr_info("0x%03lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			DCAM_AXIM_RD(addr),
			DCAM_AXIM_RD(addr + 4),
			DCAM_AXIM_RD(addr + 8),
			DCAM_AXIM_RD(addr + 12));
	}
#endif
#endif
}

int sprd_dcam_drv_zoom_param_update(struct dcam_zoom_param zoom_param,
		uint32_t is_zoom_min,
		enum dcam_id idx)
{
	int32_t ret = 0;
	struct dcam_path_desc *bin_path = sprd_dcam_drv_bin_path_get(idx);

	sprd_dcam_raw_sizer_coeff_gen(
		zoom_param.in_rect.w,
		zoom_param.in_rect.h,
		zoom_param.out_size.w,
		zoom_param.out_size.h,
		zoom_param.coeff_ptr,
		is_zoom_min);
	ret = sprd_cam_queue_buf_write(&bin_path->coeff_queue,
			&zoom_param);
	if (ret) {
		pr_err("fail to write isp coeff node\n");
		return -ret;
	}

	return ret;
}

void sprd_dcam_drv_init_online_pipe(struct zoom_info_t *o_zoom_info,
	int sensor_width, int sensor_height,
	int output_width, int output_height)
{
	struct zoom_info_t *zoom_info = o_zoom_info;

	zoom_info->base_width = sensor_width;
	zoom_info->base_height = sensor_height;
	zoom_info->crop_startx = 0;
	zoom_info->crop_starty = 0;
	zoom_info->crop_width = sensor_width;
	zoom_info->crop_height = sensor_height;
	zoom_info->cur_width = sensor_width;
	zoom_info->cur_height = sensor_height;
	zoom_info->zoom_width = output_width;
	zoom_info->zoom_height = output_height;

	pr_debug("base{%d,%d},crop{%d,%d,%d,%d},cur {%d,%d},zoom{%d,%d}\n",
		zoom_info->base_width, zoom_info->base_height,
		zoom_info->crop_startx, zoom_info->crop_starty,
		zoom_info->crop_width, zoom_info->crop_height,
		zoom_info->cur_width, zoom_info->cur_height,
		zoom_info->zoom_width, zoom_info->zoom_height);
}

static void sprd_dcam_drv_update_zoom_info(const struct crop_info_t *crop_info,
		struct zoom_info_t *zoom_info)
{
	zoom_info->crop_startx += crop_info->crop_startx;
	zoom_info->crop_starty += crop_info->crop_starty;
	zoom_info->crop_width = crop_info->crop_width;
	zoom_info->crop_height = crop_info->crop_height;
	zoom_info->cur_width = crop_info->crop_width;
	zoom_info->cur_height = crop_info->crop_height;
	pr_debug("zoom_info{%d,%d,%d,%d},cur_size{%d,%d}\n",
		zoom_info->crop_startx, zoom_info->crop_starty,
		zoom_info->crop_width, zoom_info->crop_height,
		zoom_info->cur_width, zoom_info->cur_height);
}
void sprd_dcam_drv_update_crop_param(struct crop_param_t *o_crop_param,
	struct zoom_info_t *io_zoom_info, int zoom_ratio)
{
	struct crop_param_t *crop_param = o_crop_param;
	struct zoom_info_t *zoom_info = io_zoom_info;
	struct crop_info_t _crop_info, *crop_info = &_crop_info;

	const int src_width = zoom_info->cur_width;
	const int src_height = zoom_info->cur_height;
	const int zoom_width = zoom_info->zoom_width;
	const int zoom_height = zoom_info->zoom_height;
	int crop_width = src_width;
	int crop_height = src_height;

	zoom_ratio = ((zoom_ratio + 50) / 100 * 256 + 5) / 10;
	pr_debug("zoom_info base {%d, %d}, crop{%d,%d,%d,%d},cur{%d,%d},zoom{%d,%d}, zoom_ratio = %d\n",
		zoom_info->base_width, zoom_info->base_height,
		zoom_info->crop_startx, zoom_info->crop_starty,
		zoom_info->crop_width, zoom_info->crop_height,
		zoom_info->cur_width, zoom_info->cur_height,
		zoom_info->zoom_width, zoom_info->zoom_height,
		zoom_ratio);
	if (src_width * zoom_height >= zoom_width * src_height)
		crop_width = (zoom_width * src_height + zoom_height/2)
			/ zoom_height;
	else
		crop_height = (zoom_height * src_width + zoom_width/2)
			/ zoom_width;

	crop_width = FIX_UNCAST(FIX_DIV(FIX_CAST(crop_width), zoom_ratio));
	crop_height = FIX_UNCAST(FIX_DIV(FIX_CAST(crop_height), zoom_ratio));
	crop_width = crop_width < src_width ? crop_width : src_width;
	crop_height = crop_height < src_height ? crop_height : src_height;
	crop_width = (crop_width) >> 2 << 2;
	crop_height = (crop_height) >> 2 << 2;

	crop_info->crop_width = crop_width;
	crop_info->crop_height = crop_height;
	crop_info->crop_startx = (src_width - crop_width) >> 3 << 2;
	crop_info->crop_starty = (src_height - crop_height) >> 3 << 2;
	pr_debug("crop_info{%d,%d,%d,%d}\n",
		crop_info->crop_startx, crop_info->crop_starty,
		crop_info->crop_width, crop_info->crop_height);
	sprd_dcam_drv_update_zoom_info(crop_info, zoom_info);

	if (src_width == zoom_info->cur_width &&
		src_height == zoom_info->cur_height) {
		crop_param->bypass = 1;
		pr_debug("crop_param->bypass = %d\n", crop_param->bypass);
	} else {
		crop_param->bypass = 0;
		crop_param->start_x = crop_info->crop_startx;
		crop_param->start_y = crop_info->crop_starty;
		crop_param->end_x = crop_param->start_x
				+ crop_info->crop_width;
		crop_param->end_y = crop_param->start_y
				+ crop_info->crop_height;
		pr_debug("bypass = %d crop_param{%d,%d,%d,%d}\n",
			crop_param->bypass, crop_param->start_x,
			crop_param->start_y, crop_param->end_x,
			crop_param->end_y);
	}
}

static void sprd_dcam_drv_calc_zoom_rawscl_param(int input_size, int zoom_size,
	int min_scl_ratio, int max_scl_ratio, int *o_size)
{
	int output_size;

	if (input_size * min_scl_ratio > FIX_CAST(zoom_size)) {
		output_size = FIX_UNCAST(input_size * min_scl_ratio);
		output_size = (output_size) >> 2 << 2;
	} else if (input_size * max_scl_ratio < FIX_CAST(zoom_size)) {
		output_size = FIX_UNCAST(input_size * max_scl_ratio);
		output_size = (output_size) >> 2 << 2;
	} else {
		output_size = zoom_size;
	}

	*o_size = output_size;
}

void sprd_dcam_drv_update_rawsizer_param(
	struct rawsizer_param_t *o_rawsizer_param,
	struct zoom_info_t *io_zoom_info)
{
	struct rawsizer_param_t *rawsizer_param = o_rawsizer_param;
	struct zoom_info_t *zoom_info = io_zoom_info;

	const int input_width = zoom_info->cur_width;
	const int input_height = zoom_info->cur_height;
	const int zoom_width = zoom_info->zoom_width;
	const int zoom_height = zoom_info->zoom_height;
	const int min_scl_ratio = FIX_DIV(1, 2);
	const int max_scl_ratio = FIX_DIV(1, 1);
	int new_min_scl_ratio = 0;
	int output_width, output_height;
	int max_input_width = 0;
	int max_width_buffer = 2304;

	pr_debug("zoom_info base {%d, %d}, crop{%d,%d,%d,%d},cur{%d,%d},zoom{%d,%d}\n",
		zoom_info->base_width, zoom_info->base_height,
		zoom_info->crop_startx, zoom_info->crop_starty,
		zoom_info->crop_width, zoom_info->crop_height,
		zoom_info->cur_width, zoom_info->cur_height,
		zoom_info->zoom_width, zoom_info->zoom_height);
	if (input_width*min_scl_ratio > FIX_CAST(zoom_width))
		rawsizer_param->is_zoom_min = 1;
	else
		rawsizer_param->is_zoom_min = 0;

	sprd_dcam_drv_calc_zoom_rawscl_param(input_width,
		zoom_width, min_scl_ratio,
		max_scl_ratio, &output_width);
	sprd_dcam_drv_calc_zoom_rawscl_param(input_height,
		zoom_height, min_scl_ratio,
		max_scl_ratio, &output_height);
	pr_debug("input{%d,%d},zoom{%d,%d},output{%d,%d}\n",
		input_width, input_height,
		zoom_width, zoom_height,
		output_width, output_height);

	if (output_width > max_width_buffer) {
		max_input_width = 4672;
		new_min_scl_ratio = FIX_DIV(2304, 4672);
		if (input_width > 4690)
			max_input_width = 4900;
		else if (input_width > 4672)
			max_input_width = 4690;
		new_min_scl_ratio = FIX_DIV(2304, (max_input_width));
		sprd_dcam_drv_calc_zoom_rawscl_param(input_width,
			zoom_width, new_min_scl_ratio,
			max_scl_ratio, &output_width);
		sprd_dcam_drv_calc_zoom_rawscl_param(input_height,
			zoom_height, new_min_scl_ratio,
			max_scl_ratio, &output_height);
		rawsizer_param->is_zoom_min = 1;
	}

	zoom_info->cur_width = output_width;
	zoom_info->cur_height = output_height;

	if (input_width == zoom_info->cur_width &&
		input_height == zoom_info->cur_height) {
		rawsizer_param->bypass = 1;
		rawsizer_param->output_width = zoom_info->cur_width;
		rawsizer_param->output_height = zoom_info->cur_height;
		pr_debug("rawsizer_param->bypass = %d\n",
			rawsizer_param->bypass);
	} else {
		rawsizer_param->bypass = 0;
		rawsizer_param->crop_en = 0;
		rawsizer_param->output_width = zoom_info->cur_width;
		rawsizer_param->output_height = zoom_info->cur_height;
		pr_debug("bypass = %d,crop_en = %d, output{%d, %d}",
			rawsizer_param->bypass, rawsizer_param->crop_en,
			rawsizer_param->output_height,
			rawsizer_param->output_width);
	}
}
