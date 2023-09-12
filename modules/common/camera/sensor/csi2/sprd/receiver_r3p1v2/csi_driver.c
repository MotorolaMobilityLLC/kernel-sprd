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

//#include <dt-bindings/soc/sprd,sharkl6pro-regs.h>
//#include <dt-bindings/soc/sprd,sharkl6pro-mask.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mfd/syscon.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <sprd_mm.h>
#include "csi_api.h"
#include "csi_driver.h"
#include "sprd_sensor_core.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "csi_driver: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__


#define CSI_MASK0                       0x1FFFFFF
#define CSI_MASK1                       0xFFFFFF
#define CSI_MASK2                       0x7
#define PHY_TESTCLR                     BIT_0
#define PHY_TESTCLK                     BIT_1
#define PHY_REG_SEL                     BIT_2
#define PHY_TESTDIN                     0xFF
#define PHY_TESTDOUT                    0xFF00
#define PHY_TESTEN                      BIT_16
#define PHY_LANE_CFG_COUNTER            24

#define IPG_IMAGE_H_MASK		(0x1ff << 21)
#define IPG_IMAGE_H_HI_MASK		(0x3 << 2)
#define IPG_COLOR_BAR_W_MASK		(0xff << 13)
#define IPG_IMAGE_W_MASK		(0x1FF << 4)
#define IPG_IMAGE_W_HI_MASK		(0x3)
#define IPG_HSYNC_EN_MASK		BIT_3
#define IPG_COLOR_BAR_MODE_MASK	        BIT_2
#define IPG_IMAGE_MODE_MASK		BIT_1
#define IPG_ENABLE_MASK			BIT_0

#define IPG_IMAGE_W			4672//3264
#define IPG_IMAGE_H			3504//2448


#define IPG_IMAGE_H_REG			(((IPG_IMAGE_H)/8) << 21)
#define IPG_IMAGE_H_HI_REG		(((IPG_IMAGE_H/8) > 0x1ff)? ((IPG_IMAGE_H/8) >> 7) : (0 << 2))
#define IPG_COLOR_BAR_W			(((IPG_IMAGE_W)/24) << 13)
#define IPG_IMAGE_W_REG			(((IPG_IMAGE_W)/16) << 4)
#define IPG_IMAGE_W_HI_REG		(((IPG_IMAGE_W/16) > 0x1ff)? ((IPG_IMAGE_W/16) >> 9) : 0)

#define IPG_HSYNC_EN			(1 << 3)
#define IPG_COLOR_BAR_MODE		(0 << 2)
#define IPG_IMAGE_MODE			(1 << 1)   /*0: YUV 1:RAW*/
#define IPG_ENABLE			(1 << 0)

#define IPG_BAYER_PATTERN_MASK		0x3
#define IPG_BAYER_PATTERN_BGGR		0
#define IPG_BAYER_PATTERN_RGGB		1
#define IPG_BAYER_PATTERN_GBRG		2
#define IPG_BAYER_PATTERN_GRBG		3

#define IPG_BAYER_B_MASK		(0x3FF << 20)
#define IPG_BAYER_G_MASK		(0x3FF << 10)
#define IPG_BAYER_R_MASK		(0x3FF << 0)

#define IPG_RAW10_CFG0_B		(0 << 20)
#define IPG_RAW10_CFG0_G		(0 << 10)
#define IPG_RAW10_CFG0_R		0x3ff

#define IPG_RAW10_CFG1_B		(0 << 20)
#define IPG_RAW10_CFG1_G		(0x3FF << 10)
#define IPG_RAW10_CFG1_R		0

#define IPG_RAW10_CFG2_B		(0x3ff << 20)
#define IPG_RAW10_CFG2_G		(0 << 10)
#define IPG_RAW10_CFG2_R		0

#define IPG_YUV_CFG0_B		        (0x51 << 16)
#define IPG_YUV_CFG0_G		        (0x5f << 8)
#define IPG_YUV_CFG0_R		        0xf0

#define IPG_YUV_CFG1_B		        (0x91 << 16)
#define IPG_YUV_CFG1_G		        (0x36 << 8)
#define IPG_YUV_CFG1_R		        0x22

#define IPG_YUV_CFG2_B		        (0xd2 << 16)
#define IPG_YUV_CFG2_G		        (0x10 << 8)
#define IPG_YUV_CFG2_R		        0x92

#define IPG_V_BLANK_MASK		(0xFFF << 13)
#define IPG_H_BLANK_MASK		0x1FFF
#define IPG_V_BLANK			(0xFFF << 13)
#define IPG_H_BLANK			(0x1FFF)

unsigned long s_csi_regbase[SPRD_SENSOR_ID_MAX];
static unsigned long csi_dump_regbase[CSI_MAX_COUNT];
static spinlock_t csi_dump_lock[CSI_MAX_COUNT] = {
	__SPIN_LOCK_UNLOCKED(csi_dump_lock),
	__SPIN_LOCK_UNLOCKED(csi_dump_lock),
	__SPIN_LOCK_UNLOCKED(csi_dump_lock),
	__SPIN_LOCK_UNLOCKED(csi_dump_lock),
};
#define ANALOG_G4_REG_BASE 0x64318000
#define ANALOG_G4L_REG_BASE 0x6434C000
#define MM_AHB_REG_BASE 0x30000000
static void phy_write(int32_t idx, unsigned int code_in,
	unsigned int data_in, unsigned int mask);
static void csi_2p2l_2lane_phy_testclr(struct csi_phy_info *phy);

static const struct dphy_lane_cfg dphy_lane_setting[PHY_LANE_CFG_COUNTER] = {
	/* lane_seq:data lane connect sequence (default 0x0123)
	 * lane_cfg[4]:data lane(0~4) connect control
	 * for example:
	 * change lane_seq from 0x0123 to 0x1032
	 * rewrite lane connect control register data
	 * lane0:0x28,lane1:0x08,lane2:0x68,lane3:0x48
	 */
	{0x0123, {0x08, 0x28, 0x48, 0x68} },
	{0x1023, {0x28, 0x08, 0x48, 0x68} },
	{0x2103, {0x48, 0x28, 0x08, 0x68} },
	{0x3120, {0x68, 0x28, 0x48, 0x08} },
	{0x0213, {0x08, 0x48, 0x28, 0x68} },
	{0x0321, {0x08, 0x68, 0x48, 0x28} },
	{0x0132, {0x08, 0x28, 0x68, 0x48} },
	{0x1032, {0x28, 0x08, 0x68, 0x48} },
	{0x2301, {0x48, 0x68, 0x08, 0x28} },
	{0x3210, {0x68, 0x48, 0x28, 0x08} },
	{0x0231, {0x08, 0x48, 0x68, 0x28} },
	{0x0312, {0x08, 0x68, 0x28, 0x48} },
	{0x2130, {0x48, 0x28, 0x68, 0x08} },
	{0x3102, {0x68, 0x28, 0x08, 0x48} },
	{0x1320, {0x28, 0x68, 0x48, 0x08} },
	{0x3021, {0x68, 0x08, 0x48, 0x28} },
	{0x2013, {0x48, 0x08, 0x28, 0x68} },
	{0x1203, {0x28, 0x48, 0x08, 0x68} },
	{0x1230, {0x28, 0x48, 0x68, 0x08} },
	{0x1302, {0x28, 0x68, 0x08, 0x48} },
	{0x2310, {0x48, 0x68, 0x28, 0x08} },
	{0x2031, {0x48, 0x08, 0x68, 0x28} },
	{0x3012, {0x68, 0x08, 0x28, 0x48} },
	{0x3201, {0x68, 0x48, 0x08, 0x28} },
};

int csi_reg_base_save(struct csi_dt_node_info *dt_info, int32_t idx)
{
	if (!dt_info) {
		pr_err("fail to get valid dt_info ptr\n");
		return -EINVAL;
	}

	s_csi_regbase[idx] = dt_info->reg_base;
	csi_dump_regbase[dt_info->controller_id] = 0;

	return 0;
}

void csi_ipg_mode_cfg(uint32_t idx, int enable)
{
	int ipg_raw_mode = IPG_IMAGE_MODE;
	CSI_REG_MWR(idx, PHY_PD_N, BIT_0, 1);
	CSI_REG_MWR(idx, RST_DPHY_N, BIT_0, 1);
	CSI_REG_MWR(idx, RST_CSI2_N, BIT_0, 1);

	if (enable) {
		CSI_REG_MWR(idx, MODE_CFG,
			IPG_IMAGE_H_MASK, IPG_IMAGE_H_REG);
		CSI_REG_MWR(idx, PHY_IPG_CFG_ADD,
			IPG_IMAGE_H_HI_MASK, IPG_IMAGE_H_HI_REG);
		CSI_REG_MWR(idx, MODE_CFG,
			IPG_COLOR_BAR_W_MASK, IPG_COLOR_BAR_W);
		CSI_REG_MWR(idx, MODE_CFG, IPG_IMAGE_W_MASK, IPG_IMAGE_W_REG);
		CSI_REG_MWR(idx, PHY_IPG_CFG_ADD,
			IPG_IMAGE_W_HI_MASK, IPG_IMAGE_W_HI_REG);
		CSI_REG_MWR(idx, MODE_CFG, IPG_HSYNC_EN_MASK, IPG_HSYNC_EN);
		CSI_REG_MWR(idx, MODE_CFG, IPG_COLOR_BAR_MODE_MASK,
						IPG_COLOR_BAR_MODE);
		CSI_REG_MWR(idx, MODE_CFG, IPG_IMAGE_MODE_MASK, IPG_IMAGE_MODE);

		CSI_REG_MWR(idx, IPG_RAW10_CFG0,
			IPG_BAYER_B_MASK, IPG_RAW10_CFG0_B);
		CSI_REG_MWR(idx, IPG_RAW10_CFG0,
			IPG_BAYER_G_MASK, IPG_RAW10_CFG0_G);
		CSI_REG_MWR(idx, IPG_RAW10_CFG0,
			IPG_BAYER_R_MASK, IPG_RAW10_CFG0_R);
		CSI_REG_MWR(idx, IPG_RAW10_CFG1,
			IPG_BAYER_B_MASK, IPG_RAW10_CFG1_B);
		CSI_REG_MWR(idx, IPG_RAW10_CFG1,
			IPG_BAYER_G_MASK, IPG_RAW10_CFG1_G);
		CSI_REG_MWR(idx, IPG_RAW10_CFG1,
			IPG_BAYER_R_MASK, IPG_RAW10_CFG1_R);
		CSI_REG_MWR(idx, IPG_RAW10_CFG2,
			IPG_BAYER_B_MASK, IPG_RAW10_CFG2_B);
		CSI_REG_MWR(idx, IPG_RAW10_CFG2,
			IPG_BAYER_G_MASK, IPG_RAW10_CFG2_G);
		CSI_REG_MWR(idx, IPG_RAW10_CFG2,
			IPG_BAYER_R_MASK, IPG_RAW10_CFG2_R);

		CSI_REG_MWR(idx, IPG_RAW10_CFG3, IPG_BAYER_PATTERN_MASK,
						IPG_BAYER_PATTERN_GBRG);
		if (!ipg_raw_mode) {
			CSI_REG_MWR(idx, IPG_YUV422_8_CFG0,
				0x00FF0000, IPG_YUV_CFG0_B);
			CSI_REG_MWR(idx, IPG_YUV422_8_CFG0,
				0x0000FF00, IPG_YUV_CFG0_G);
			CSI_REG_MWR(idx, IPG_YUV422_8_CFG0,
				0x000000FF, IPG_YUV_CFG0_R);

			CSI_REG_MWR(idx, IPG_YUV422_8_CFG1,
				0x00FF0000, IPG_YUV_CFG1_B);
			CSI_REG_MWR(idx, IPG_YUV422_8_CFG1,
				0x0000FF00, IPG_YUV_CFG1_G);
			CSI_REG_MWR(idx, IPG_YUV422_8_CFG1,
				0x000000FF, IPG_YUV_CFG1_R);

			CSI_REG_MWR(idx, IPG_YUV422_8_CFG2,
				0x00FF0000, IPG_YUV_CFG2_B);
			CSI_REG_MWR(idx, IPG_YUV422_8_CFG2,
				0x0000FF00, IPG_YUV_CFG2_G);
			CSI_REG_MWR(idx, IPG_YUV422_8_CFG2,
				0x000000FF, IPG_YUV_CFG2_R);
		}

		CSI_REG_MWR(idx, IPG_OTHER_CFG0, IPG_V_BLANK_MASK, IPG_V_BLANK);
		CSI_REG_MWR(idx, IPG_OTHER_CFG0, IPG_H_BLANK_MASK, IPG_H_BLANK);

		CSI_REG_MWR(idx, MODE_CFG, IPG_ENABLE_MASK, IPG_ENABLE);
	} else{
		CSI_REG_MWR(idx, MODE_CFG, IPG_ENABLE_MASK, ~IPG_ENABLE);
	}
	pr_info("CSI IPG enable %d\n", enable);
}

void phy_reg_trace(unsigned int idx, unsigned int code_in,
	uint8_t *test_out){
	unsigned long regbase = 0;
	unsigned int temp = 0xffffff00;
	regbase = csi_dump_regbase[idx];
	if (regbase == 0) {
		pr_info("CSI %d not used no need to dump\n", idx);
		return;
	}

	REG_MWR(regbase + PHY_TEST_CRTL1, PHY_TESTEN, 1 << 16);
	udelay(1);
	REG_MWR(regbase + PHY_TEST_CRTL0, PHY_TESTCLK, 1 << 1);
	udelay(1);
	REG_MWR(regbase + PHY_TEST_CRTL1, PHY_TESTDIN, code_in);
	udelay(1);
	REG_MWR(regbase + PHY_TEST_CRTL0, PHY_TESTCLK, 0 << 1);
	udelay(1);
	REG_MWR(regbase + PHY_TEST_CRTL1, PHY_TESTEN, 0 << 16);
	udelay(1);
	temp = (REG_RD(regbase + PHY_TEST_CRTL1) & PHY_TESTDOUT) >> 8;
	udelay(1);
	*test_out = (uint8_t)temp;
	pr_debug("PHY Read addr %x value = 0x%x.\r\n", code_in, temp);

}

void phy_reg_trace_s(unsigned int idx, unsigned int code_in,
	uint8_t *test_out){
	unsigned long regbase = 0;
	unsigned int temp = 0xffffff00;
	regbase = csi_dump_regbase[idx];
	if (regbase == 0) {
		pr_info("CSI %d not used no need to dump\n", idx);
		return;
	}

	REG_MWR(regbase + PHY_TEST_S_CRTL1, PHY_TESTEN, 1 << 16);
	udelay(1);
	REG_MWR(regbase + PHY_TEST_S_CRTL0, PHY_TESTCLK, 1 << 1);
	udelay(1);
	REG_MWR(regbase + PHY_TEST_S_CRTL1, PHY_TESTDIN, code_in);
	udelay(1);
	REG_MWR(regbase + PHY_TEST_S_CRTL0, PHY_TESTCLK, 0 << 1);
	udelay(1);
	REG_MWR(regbase + PHY_TEST_S_CRTL1, PHY_TESTEN, 0 << 16);
	udelay(1);
	temp = (REG_RD(regbase + PHY_TEST_S_CRTL1) & PHY_TESTDOUT) >> 8;
	udelay(1);
	*test_out = (uint8_t)temp;
	pr_debug("PHY_S Read addr %x value = 0x%x.\r\n", code_in, temp);

}


int reg_mwr(unsigned int reg, unsigned int msk, unsigned int value)
{
	void __iomem *reg_base = NULL;

	reg_base = ioremap(reg, 0x4);
	if (!reg_base) {
		pr_info("0x%x: ioremap failed\n", reg);
		return -1;
	}
	REG_MWR(reg_base, msk, value);
	mb();
	iounmap(reg_base);
	return 0;
}

int reg_wr(unsigned int reg, unsigned int value)
{
	void __iomem *reg_base = NULL;

	reg_base = ioremap(reg, 0x4);
	if (!reg_base) {
		pr_info("0x%x: ioremap failed\n", reg);
		return -1;
	}
	REG_WR(reg_base,value);
	mb();
	iounmap(reg_base);
	return 0;
}

int reg_rd(unsigned int reg)
{
	void __iomem *reg_base = NULL;
	int val = 0;

	reg_base = ioremap(reg, 0x4);
	if (!reg_base) {
		pr_info("0x%x: ioremap failed\n", reg);
		return -1;
	}
	val = REG_RD(reg_base);
	pr_debug("0x%x: val %x\n", reg, val);
	iounmap(reg_base);
	return val;
}

int CSI_REG_MWR(unsigned int idx, unsigned int reg, unsigned int msk, unsigned int val){

	unsigned int reg_base = 0x3e200000 + idx * 0x100000 + reg;
/*	while((reg_rd(0x3000000c)&(1<<(12+idx)))==0x0){
		pr_info("%s csi%d need enable\n", __func__, idx);
		udelay(10);
		reg_mwr(0x3000000c,  (1<<(12+idx)), (1<<(12+idx)));
	}*/
	unsigned int temp = ((val) & (msk)) | (CSI_REG_RD(idx, reg) & (~(msk)));
	pr_debug("reg_base 0x%x 0x%x\n", reg_base,CSI_BASE(idx)+reg);
	writel_relaxed(temp, (volatile void __iomem *)(CSI_BASE(idx)+reg));
	return 0;
}


int reg_dump_rd(unsigned long reg, int len, char *reg_name)
{
	void __iomem *reg_base = NULL;
	unsigned long addr = 0;

	reg_base = ioremap(reg, len);
	if (!reg_base) {
		pr_info("0x%x: ioremap failed\n", reg);
		return -1;
	}else
		pr_info("0x%x: dump reg\n", reg);

	for (addr = 0; addr <= len; addr += 16) {
		pr_info("%s 0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			reg_name, addr,
			REG_RD(reg_base + addr),
			REG_RD(reg_base + addr + 4),
			REG_RD(reg_base + addr + 8),
			REG_RD(reg_base + addr + 12));
	}
	iounmap(reg_base);
	return 0;
}
void csi_reg_trace(unsigned int idx)
{
	unsigned long addr = 0;
	unsigned long flag = 0;
	unsigned long regbase = 0;

	spin_lock_irqsave(&csi_dump_lock[idx], flag);
	regbase = csi_dump_regbase[idx];
	if (regbase == 0) {
		pr_info("CSI %d not used no need to dump\n", idx);
		spin_unlock_irqrestore(&csi_dump_lock[idx], flag);
		return;
	}

	pr_info("CSI %d reg list\n", idx);
	for (addr = IP_REVISION; addr <= 0xff; addr += 16) {
		uint32_t tmp0 = REG_RD(regbase + addr);
		uint32_t tmp1 = REG_RD(regbase + addr + 4);
		uint32_t tmp2 = REG_RD(regbase + addr + 8);
		uint32_t tmp3 = REG_RD(regbase + addr + 12);
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr, tmp0, tmp1, tmp2, tmp3);
	}
#ifdef DEBUG_DPHY_2P2S
	if(idx > 1 && idx < 8){
		for (addr = 0; addr <= 0xff; addr += 4) {
			uint8_t tmp0 = 0;
			uint8_t tmp1 = 0;
			uint8_t tmp2 = 0;
			uint8_t tmp3 = 0;
			phy_reg_trace_s(idx, addr, &tmp0);
			phy_reg_trace_s(idx, addr + 1, &tmp1);
			phy_reg_trace_s(idx, addr + 2, &tmp2);
			phy_reg_trace_s(idx, addr + 3, &tmp3);

			pr_info("phy_s 0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
				addr, tmp0, tmp1, tmp2, tmp3);
		}
	}
#endif
#ifdef DEBUG_DPHY
	for (addr = 0; addr <= 0xff; addr += 4) {
			uint8_t tmp0 = 0;
			uint8_t tmp1 = 0;
			uint8_t tmp2 = 0;
			uint8_t tmp3 = 0;
			phy_reg_trace(idx, addr, &tmp0);
			phy_reg_trace(idx, addr + 1, &tmp1);
			phy_reg_trace(idx, addr + 2, &tmp2);
			phy_reg_trace(idx, addr + 3, &tmp3);

			pr_info("phy 0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
				addr, tmp0, tmp1, tmp2, tmp3);
		}
#endif
#ifdef DEBUG_CPHY
		 if(idx == 0 || idx == 1 || idx == 8 || idx == 9){
				CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_REG_SEL, 1 << 2);
				for (addr = 0; addr <= 0xff; addr += 4) {
						uint8_t tmp0 = 0;
						uint8_t tmp1 = 0;
						uint8_t tmp2 = 0;
						uint8_t tmp3 = 0;
						phy_reg_trace(idx, addr, &tmp0);
						phy_reg_trace(idx, addr + 1, &tmp1);
						phy_reg_trace(idx, addr + 2, &tmp2);
						phy_reg_trace(idx, addr + 3, &tmp3);

						pr_info("cphy 0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
								addr, tmp0, tmp1, tmp2, tmp3);
				}

				CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_REG_SEL, ~(1 << 2));
		}
#endif
#ifdef DEBUG_CHIP_TOP
		reg_dump_rd(ANALOG_G4_REG_BASE, 0xff, "ana-g4");
		reg_dump_rd(ANALOG_G4L_REG_BASE, 0xff, "ana-g4l");
		reg_dump_rd(0x30000000, 0xdf, "mm-ahb");
		reg_dump_rd(0x30010100, 0x100, "mm-clk");
		reg_dump_rd(0x64900000, 0xff, "aon-apb");
		reg_dump_rd(0x64910100, 0x1ff, "aon-pm");
#endif
#ifdef DEBUG_DCAM
	reg_dump_rd(0x3e000000, 0xff, "dcam");
	reg_dump_rd(0x3e000400, 0xff, "dcam-mipicap");
#endif
	spin_unlock_irqrestore(&csi_dump_lock[idx], flag);
}
#ifdef DEBUG_2P2S_DPHY

/* used to write testcode or testdata to cphy */
static void phy_write_s(int32_t idx, unsigned int code_in,
	unsigned int data_in, unsigned int mask)
{
	unsigned int temp = 0xffffff00;

	CSI_REG_MWR(idx, PHY_TEST_S_CRTL1, PHY_TESTEN, 1 << 16);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_S_CRTL0, PHY_TESTCLK, 1 << 1);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_S_CRTL1, PHY_TESTDIN, code_in);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLK, 0 << 1);
	udelay(1);
	temp = (CSI_REG_RD(idx, PHY_TEST_S_CRTL1) & PHY_TESTDOUT) >> 8;
	udelay(1);
	data_in = (temp & (~mask)) | (mask&data_in);
	CSI_REG_MWR(idx, PHY_TEST_S_CRTL1, PHY_TESTEN, 0 << 16);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_S_CRTL1, PHY_TESTDIN, data_in);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_S_CRTL0, PHY_TESTCLK, 1 << 1);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_S_CRTL0, PHY_TESTCLK, 0 << 1);
	udelay(1);
	temp = (CSI_REG_RD(idx, PHY_TEST_S_CRTL1) & PHY_TESTDOUT) >> 8;
	udelay(1);
	pr_debug("PHY write addr %x value = 0x%x.\r\n", code_in, temp);
}
/* used to read testcode or testdata to cphy */
static void phy_read_s(int32_t idx, unsigned int code_in,
	uint8_t *test_out)
{
	unsigned int temp = 0xffffff00;

	CSI_REG_MWR(idx, PHY_TEST_S_CRTL1, PHY_TESTEN, 1 << 16);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_S_CRTL0, PHY_TESTCLK, 1 << 1);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_S_CRTL1, PHY_TESTDIN, code_in);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_S_CRTL0, PHY_TESTCLK, 0 << 1);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_S_CRTL1, PHY_TESTEN, 0 << 16);
	udelay(1);
	temp = (CSI_REG_RD(idx, PHY_TEST_S_CRTL1) & PHY_TESTDOUT) >> 8;
	udelay(1);
	*test_out = (uint8_t)temp;
	pr_debug("PHY Read addr %x value = 0x%x.\r\n", code_in, temp);
}
#endif
/* used to write testcode or testdata to cphy */
static void phy_write(int32_t idx, unsigned int code_in,
	unsigned int data_in, unsigned int mask)
{
	unsigned int temp = 0xffffff00;

	CSI_REG_MWR(idx, PHY_TEST_CRTL1, PHY_TESTEN, 1 << 16);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLK, 1 << 1);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL1, PHY_TESTDIN, code_in);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLK, 0 << 1);
	udelay(1);
	temp = (CSI_REG_RD(idx, PHY_TEST_CRTL1) & PHY_TESTDOUT) >> 8;
	udelay(1);
	data_in = (temp & (~mask)) | (mask&data_in);
	CSI_REG_MWR(idx, PHY_TEST_CRTL1, PHY_TESTEN, 0 << 16);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL1, PHY_TESTDIN, data_in);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLK, 1 << 1);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLK, 0 << 1);
	udelay(1);
	temp = (CSI_REG_RD(idx, PHY_TEST_CRTL1) & PHY_TESTDOUT) >> 8;
	udelay(1);
	pr_debug("PHY write addr %x value = 0x%x.\r\n", code_in, temp);
}

/* used to read testcode or testdata to cphy */
static void phy_read(int32_t idx, unsigned int code_in,
	uint8_t *test_out)
{
	unsigned int temp = 0xffffff00;

	CSI_REG_MWR(idx, PHY_TEST_CRTL1, PHY_TESTEN, 1 << 16);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLK, 1 << 1);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL1, PHY_TESTDIN, code_in);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLK, 0 << 1);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL1, PHY_TESTEN, 0 << 16);
	udelay(1);
	temp = (CSI_REG_RD(idx, PHY_TEST_CRTL1) & PHY_TESTDOUT) >> 8;
	udelay(1);
	*test_out = (uint8_t)temp;
	pr_debug("PHY Read addr %x value = 0x%x.\r\n", code_in, temp);
}

#ifdef FPAG_BRINGUP
static void csi_phy_lane_cfg(unsigned int phy_id, int32_t idx,
	bool lane_switch_eb, uint64_t lane_seq)
{
	int i = 0;

	if (!lane_switch_eb)
		return;
	pr_info("csi lane_switch %d lane_seq 0x%llx\n",
		lane_switch_eb, lane_seq);

	switch (phy_id) {
	case PHY_2P2:
		if ((lane_seq != 0x0123) && (lane_seq != 0x0132) &&
			(lane_seq != 0x1023) && (lane_seq != 0x1032)) {
			pr_err("fail to get valid 2p2 4lane phy seq\n");
			return;
		}
	case PHY_4LANE:
		break;
	case PHY_2LANE:
	case PHY_2P2_S:
	case PHY_2P2_M:
		if ((lane_seq != 0x0123) && (lane_seq != 0x1023)) {
			pr_err("fail to get valid 2lane phy seq\n");
			return;
		}
		break;
	default:
		pr_err("fail to get valid csi phy id\n");
	}

	for (i = 0; i < PHY_LANE_CFG_COUNTER; i++) {
		if (lane_seq == dphy_lane_setting[i].lane_seq) {
			phy_write(idx, 0x4d,
				dphy_lane_setting[i].lane_cfg[0], 0xff);
			phy_write(idx, 0x5d,
				dphy_lane_setting[i].lane_cfg[1], 0xff);
			phy_write(idx, 0x6d,
				dphy_lane_setting[i].lane_cfg[2], 0xff);
			phy_write(idx, 0x7d,
				dphy_lane_setting[i].lane_cfg[3], 0xff);
			break;
		}
	}

	if (i == PHY_LANE_CFG_COUNTER) {
		pr_err("fail to get valid 4lane phy seq\n");
		return;
	}
}
#endif
//set testclr first  then mode sel
void csi_phy_testclr_init(struct csi_phy_info *phy)
{
	switch (phy->phy_id) {
	case PHY_CPHY:
		regmap_update_bits(phy->anlg_phy_g4l_syscon,0x30,BIT_0, BIT_0);//mode_sel
		break;
	case PHY_4LANE:
		regmap_update_bits(phy->anlg_phy_g4l_syscon,0x30,BIT_0, ~BIT_0);//mode_sel
		break;
	case PHY_CPHY1:
		regmap_update_bits(phy->anlg_phy_g4_syscon,0x30,BIT_0, BIT_0);//mode_sel
		break;
	case PHY_4LANE1:
		regmap_update_bits(phy->anlg_phy_g4_syscon,0x30,BIT_0, ~BIT_0);//mode_sel
		break;
	case PHY_2P2RO:
		regmap_update_bits(phy->anlg_phy_g4_syscon,0x5c,BIT_4, BIT_4);//mode_sel
		break;
	case PHY_2P2RO_M:
		regmap_update_bits(phy->anlg_phy_g4_syscon,0x5c,BIT_4, ~BIT_4);//mode_sel
		break;
	case PHY_2P2RO_S:
		regmap_update_bits(phy->anlg_phy_g4_syscon,0x5c,BIT_4, ~BIT_4);//mode_sel
		break;
	case PHY_2P2:
		regmap_update_bits(phy->anlg_phy_g4l_syscon,0x5c,BIT_4, BIT_4);//mode_sel
		break;
	case PHY_2P2_M:
		regmap_update_bits(phy->anlg_phy_g4l_syscon,0x5c,BIT_4, ~BIT_4);//mode_sel
		break;
	case PHY_2P2_S:
		regmap_update_bits(phy->anlg_phy_g4l_syscon,0x5c,BIT_4, ~BIT_4);//mode_sel
		break;
	default:
		pr_err("fail to get valid phy id %d\n", phy->phy_id);
		break;
	}
	return;
}

void csi_phy_testclr(int sensor_id, struct csi_phy_info *phy)
{
	unsigned int mask = 0;
	unsigned long flag = 0;
	struct regmap *anlg_phy_syscon;
    if (phy->phy_id == PHY_CPHY || phy->phy_id == PHY_4LANE || phy->phy_id == PHY_2P2 ||
		phy->phy_id == PHY_2P2_M || phy->phy_id == PHY_2P2_S){
		anlg_phy_syscon = phy->anlg_phy_g4l_syscon;
	}else{
		anlg_phy_syscon = phy->anlg_phy_g4_syscon;
	}
	spin_lock_irqsave(&csi_dump_lock[sensor_id], flag);

	switch (phy->phy_id) {
	case PHY_CPHY1:
	case PHY_4LANE1:
	case PHY_CPHY:
	case PHY_4LANE:
		CSI_REG_MWR(sensor_id, PHY_TEST_CRTL0, PHY_TESTCLR, 1);
		CSI_REG_MWR(sensor_id, PHY_TEST_CRTL0, PHY_TESTCLR, 0);
		break;
	case PHY_2P2:
	case PHY_2P2RO:
	case PHY_2P2_M:
	case PHY_2P2RO_M:
	case PHY_2P2_S:
	case PHY_2P2RO_S:
			mask = BIT_0;//MASK_ANLG_PHY_G10_RF_ANALOG_MIPI_CSI_2P2LANE_DSI_TESTCLR_DB;
			regmap_update_bits(anlg_phy_syscon,0xa8,mask, mask);
			//REG_ANLG_PHY_G10_RF_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB,
			CSI_REG_MWR(sensor_id, PHY_TEST_CRTL0, PHY_TESTCLR, 1);
			CSI_REG_MWR(sensor_id, PHY_TEST_S_CRTL0, PHY_TESTCLR, 1);

			regmap_update_bits(anlg_phy_syscon,0xa8,mask, ~mask);
			//anlg_phy_g10_syscon, 0x7c,//REG_ANLG_PHY_G10_RF_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB,
			CSI_REG_MWR(sensor_id, PHY_TEST_CRTL0, PHY_TESTCLR, 0);

			CSI_REG_MWR(sensor_id, PHY_TEST_S_CRTL0, PHY_TESTCLR, 0);
			break;

	default:
		pr_err("fail to get valid phy id %d\n", phy->phy_id);
		return;
	}
	spin_unlock_irqrestore(&csi_dump_lock[sensor_id], flag);
	return;

}

static void csi_2p2l_2lane_phy_testclr(struct csi_phy_info *phy)
{
	unsigned int mask_testclr = 0;
	unsigned int testclr_reg_val = 0;
	struct regmap *anlg_phy_syscon;
	//ANALOG G4 0X34:[23] CSI COMBO 1 testclr; [22] pd_s; [21] pd_l; [21] iso_sw_en; [19-16] rx ctrl(register triming single)
	//ANALOG G4 0X40:[6]  dbg CSI COMBO 1 testclr; [7] phy sel
	//ANALOG G4 0X30:[18:11] CSI COMBO 1 testdin; [10:3] testdout; [2] testen; [1] testclk; [0] phy_sel 0:dphy 1:cphy
	//ANALOG G4 0X58:[0] 2p2  1 testclr_m; [22] pd_s; [21] pd_l; [21] iso_sw_en; [19-16] rx ctrl(register triming single)
	//ANALOG G4 0X6c:[16] 2p2  1 testclr_s;
	//ANALOG G4 0X5c:[4] 2p2  1 mode sel:0--2p2 1--4lane;

	switch (phy->phy_id) {
	case PHY_CPHY1:
	case PHY_CPHY:
	case PHY_4LANE1:
	case PHY_4LANE:
		mask_testclr = BIT_23;
		testclr_reg_val = 0x34;
		break;
	case PHY_2P2RO:
	case PHY_2P2:
	case PHY_2P2RO_M:
	case PHY_2P2_M:
		mask_testclr = BIT_0;
		testclr_reg_val = 0x58;
		break;
	case PHY_2P2RO_S:
	case PHY_2P2_S:
		mask_testclr = BIT_16;
		testclr_reg_val = 0x6c;
		break;
	default:
		pr_err("fail to get valid phy id %d\n", phy->phy_id);
		return;
	}

    if (phy->phy_id == PHY_CPHY || phy->phy_id == PHY_4LANE || phy->phy_id == PHY_2P2 ||
		phy->phy_id == PHY_2P2_M || phy->phy_id == PHY_2P2_S){
		anlg_phy_syscon = phy->anlg_phy_g4l_syscon;
	}else{
		anlg_phy_syscon = phy->anlg_phy_g4_syscon;
	}


	regmap_update_bits(anlg_phy_syscon,testclr_reg_val,
		mask_testclr, mask_testclr);
	udelay(1);
	regmap_update_bits(anlg_phy_syscon,testclr_reg_val,
		mask_testclr, ~mask_testclr);
	if (phy->phy_id == PHY_2P2 || phy->phy_id == PHY_2P2RO){
		mask_testclr = BIT_16;
		testclr_reg_val = 0x6c;
		regmap_update_bits(anlg_phy_syscon,testclr_reg_val,
			//REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST
			//REG_ANLG_PHY_G10_RF_ANALOG_MIPI_CSI_COMBO_CSI_4L_BIST_TEST,
			mask_testclr, mask_testclr);
		udelay(1);
		regmap_update_bits(anlg_phy_syscon,testclr_reg_val,
			mask_testclr, ~mask_testclr);
	}

}

void csi_phy_power_down(struct csi_dt_node_info *csi_info,
			unsigned int sensor_id, int is_eb)
{
	uint32_t shutdownz = 0;
	uint32_t reg = 0;
	struct csi_phy_info *phy = &csi_info->phy;
	struct regmap *anlg_phy_syscon;
	uint32_t ps_pd_l = 0;
	uint32_t ps_pd_s = 0;
	uint32_t iso_sw = 0;
	uint32_t reg1 = 0;

	if (!phy || !csi_info) {
		pr_err("fail to get valid phy ptr\n");
		return;
	}
//ANALOG G4 0X2C:[6] CSI COMBO 1 SHUTDOWNZ; [5] RSTZ; [4:2] ENABLE 0-2; [1] BISTON; [0] BISTOK
//ANALOG G4 0X58: CSI_2P2_1_M 0X68:CSI_2P2_1_S
//ANALOG G4L 0X2C:[6] CSI COMBO 0 SHUTDOWNZ; [5] RSTZ; [4:2] ENABLE 0-2; [1] BISTON; [0] BISTOK
//ANALOG G4L 0X58: CSI_2P2_0_M 0X68:CSI_2P2_1_S

    if (phy->phy_id == PHY_CPHY || phy->phy_id == PHY_4LANE || phy->phy_id == PHY_2P2 ||
		phy->phy_id == PHY_2P2_M || phy->phy_id == PHY_2P2_S){
		anlg_phy_syscon = phy->anlg_phy_g4l_syscon;
	}else{
		anlg_phy_syscon = phy->anlg_phy_g4_syscon;
	}

	switch (phy->phy_id ) {
	case PHY_CPHY:
	case PHY_4LANE:
	case PHY_CPHY1:
	case PHY_4LANE1:
		ps_pd_l = BIT_22;
		ps_pd_s = BIT_21;
		iso_sw = BIT_20;
		reg1 = 0x34;
		shutdownz = BIT_6;
		reg = 0x2C;
		break;
	case PHY_2P2RO:
	case PHY_2P2:
		ps_pd_l = BIT_5;
		ps_pd_s = BIT_6;
		iso_sw = BIT_1;
		reg1 = 0xa8;
	case PHY_2P2_M:
	case PHY_2P2RO_M:
		shutdownz = BIT_7;
		reg = 0x58;
		break;
	case PHY_2P2_S:
	case PHY_2P2RO_S:
		shutdownz = BIT_6;
		reg = 0x68;
		break;
	default:
		pr_err("fail to get valid csi_rx id\n");
		break;
	}

	if (is_eb){
		regmap_update_bits(anlg_phy_syscon,
		reg, shutdownz, ~shutdownz);
		if(phy->phy_id  == PHY_2P2 || phy->phy_id  == PHY_2P2RO)
			regmap_update_bits(anlg_phy_syscon,0x0068, BIT_6, ~BIT_6);
	}else{
		regmap_update_bits(anlg_phy_syscon,
		reg, shutdownz, shutdownz);
		if(phy->phy_id  == PHY_2P2 || phy->phy_id  == PHY_2P2RO)
			regmap_update_bits(anlg_phy_syscon,0x0068, BIT_6, BIT_6);
	}
}

int csi_ahb_reset(struct csi_phy_info *phy, unsigned int csi_id)
{
	unsigned int flag = 0;

	if (!phy) {
		pr_err("fail to get valid phy ptr\n");
		return -EINVAL;
	}
	pr_info("csi, id %d phy %d\n", csi_id, phy->phy_id);

	switch (csi_id) {
	case CSI_RX0:
		flag = BIT_11;
		break;
	case CSI_RX1:
		flag = BIT_10;
		break;
	case CSI_RX2:
		flag = BIT_9;
		break;
	case CSI_RX3:
		flag = BIT_8;
		break;
	default:
		pr_err("fail to get valid csi id %d\n", csi_id);
		break;
	}
	regmap_update_bits(phy->cam_ahb_syscon,
			0xc8, flag, flag);
	udelay(1);
	regmap_update_bits(phy->cam_ahb_syscon,
			0xc8, flag, ~flag);

	return 0;
}

void csi_controller_enable(struct csi_dt_node_info *dt_info)
{
	struct csi_phy_info *phy = NULL;
	uint32_t mask_eb = 0;

	if (!dt_info) {
		pr_err("fail to get valid dt_info ptr\n");
		return;
	}

	phy = &dt_info->phy;
	if (!phy) {
		pr_err("fail to get valid phy ptr\n");
		return;
	}

	pr_info("%s csi, id %d phy %d\n", __func__, dt_info->controller_id,
		phy->phy_id);

	switch (dt_info->controller_id) {
	case CSI_RX0: {
		mask_eb = BIT_12;
		break;
	}
	case CSI_RX1: {
		mask_eb = BIT_13;
		break;
	}
	case CSI_RX2: {
		mask_eb = BIT_14;
		break;
	}
	case CSI_RX3: {
		mask_eb = BIT_15;
		break;
	}
	default:
		pr_err("fail to get valid csi id\n");
		return;
	}


	csi_dump_regbase[dt_info->controller_id] = dt_info->reg_base;
	do{
		pr_info("%s csi, id %d phy %d enable\n", __func__, dt_info->controller_id,
		phy->phy_id);
		regmap_update_bits(phy->cam_ahb_syscon, 0x0c,
			mask_eb, mask_eb);
		udelay(10);
	}while((reg_rd(0x3000000c)&mask_eb)==0x0);

}

void csi_controller_disable(struct csi_dt_node_info *dt_info, int32_t idx)
{
	struct csi_phy_info *phy = NULL;
	uint32_t mask_eb = 0;
	uint32_t mask_rst = 0;

	if (!dt_info) {
		pr_err("fail to get valid dt_info ptr\n");
		return;
	}

	phy = &dt_info->phy;
	if (!phy) {
		pr_err("fail to get valid phy ptr\n");
		return;
	}

	pr_info("%s csi, id %d phy %d\n", __func__, dt_info->controller_id,
		phy->phy_id);

	switch (dt_info->controller_id) {
	case CSI_RX0: {
		mask_eb = BIT_12;
		mask_rst = BIT_11;
		break;
	}
	case CSI_RX1: {
		mask_eb = BIT_13;
		mask_rst = BIT_10;
		break;
	}
	case CSI_RX2: {
		mask_eb = BIT_14;
		mask_rst = BIT_9;
		break;
	}
	case CSI_RX3: {
		mask_eb = BIT_15;
		mask_rst = BIT_8;
		break;
	}
	default:
		pr_err("fail to get valid csi id\n");
		return;
	}


	csi_dump_regbase[dt_info->controller_id] = 0;

	//regmap_update_bits(phy->cam_ahb_syscon, 0x0c,
	//		mask_eb, mask_eb);
	regmap_update_bits(phy->cam_ahb_syscon, 0xc8,
			mask_rst, ~mask_rst);
	udelay(1);
	regmap_update_bits(phy->cam_ahb_syscon, 0xc8,
			mask_rst, ~mask_rst);

}

void phy_csi_path_cfg(struct csi_dt_node_info *dt_info, int sensor_id)
{

	struct csi_phy_info *phy = NULL;

	if (!dt_info) {
		pr_err("fail to get valid dt_info ptr\n");
		return;
	}

	phy = &dt_info->phy;
	if (!phy) {
		pr_err("fail to get valid phy ptr\n");
		return;
	}

	pr_info("%s csi, id %d phy %d\n", __func__, dt_info->controller_id,
		phy->phy_id);

}

void phy_csi_path_clr_cfg(struct csi_dt_node_info *dt_info, int sensor_id)
{

	struct csi_phy_info *phy = NULL;

	if (!dt_info) {
		pr_err("fail to get valid dt_info ptr\n");
		return;
	}

	phy = &dt_info->phy;
	if (!phy) {
		pr_err("fail to get valid phy ptr\n");
		return;
	}

	pr_info("%s csi, id %d phy %d\n", __func__, dt_info->controller_id,
		phy->phy_id);


}

//t_win:  0.5f * 2048 * n_win_set0/(26 * 1000 * 1000);
//t_osc_cal :  t_win / (n_count_result0 * 67 * 4 * 4);
//ui: 1.0 / (bps_per_lane * 1000 * 1000) ;
//n_loop :	0.5f * ui / t_osc_cal - 19;
void cphy_osc_result_cal(int32_t idx, int bps_per_lane)
{
	uint8_t n_count_tmp0 = 0;
	uint8_t n_count_tmp1 = 0;
	uint16_t n_count_result0 = 0;
	uint8_t n_win_set0 = 0;
	uint16_t n_loop = 0;
	phy_read(idx, 0x3b, &n_win_set0);
	n_win_set0 = ((n_win_set0 & (BIT_7 | BIT_6)) >> 6) + 1;
	if(n_win_set0 == 3)
		n_win_set0 = 4;
	else if (n_win_set0 == 4)
		n_win_set0 = 8;
	pr_info("n_win_set0 %d bps_per_lane %d\n", n_win_set0, bps_per_lane);
	phy_read(idx, 0x48, &n_count_tmp0);
	phy_read(idx, 0x49, &n_count_tmp1);
	n_count_result0 = n_count_tmp0 | (n_count_tmp1 << 8);

	n_loop = (n_count_result0 * 67 * 4 * 4)*26 / (bps_per_lane  * 2048 * n_win_set0) - 19;
	phy_write(idx, 0x96, n_loop & 0x7f, 0x7f);
	phy_write(idx, 0xb6, n_loop & 0x7f, 0x7f);
	phy_write(idx, 0xd6, n_loop & 0x7f, 0x7f);
	pr_info("n_loop %d %d %d %d\n", n_loop, n_count_tmp0, n_count_tmp1, n_count_result0);

}

void cphy_cdr_init(struct csi_dt_node_info *dt_info, int32_t idx)
{
	uint8_t readback = 0;
	phy_write(idx, 0x6e, BIT_0, BIT_0);
	//udelay(10);
	phy_write(idx, 0x61, 0x82, 0xff);
	phy_write(idx, 0x3b, BIT_0, BIT_0);
	phy_write(idx, 0x62, BIT_0, BIT_0);
	phy_write(idx, 0x60, 0xf1, 0xff);
	phy_write(idx, 0x3c, BIT_3 | ~BIT_2, BIT_3 | BIT_2);
	phy_write(idx, 0x50, BIT_6, BIT_6);
	phy_write(idx, 0x50, BIT_2, BIT_2);
//	phy_write(idx, 0x3b, 0x2, BIT_6 | BIT_7);
	phy_write(idx, 0x50, BIT_0, BIT_0);
	phy_write(idx, 0x50, BIT_1, BIT_1);
	phy_write(idx, 0x63, BIT_3, BIT_3);
	phy_write(idx, 0x63, BIT_7, BIT_7);
	phy_write(idx, 0x3b, BIT_3, BIT_3);
	phy_write(idx, 0x92, BIT_3, BIT_3);
	phy_write(idx, 0x92, BIT_7, BIT_7);
	phy_write(idx, 0xb2, BIT_3, BIT_3);
	phy_write(idx, 0xb2, BIT_7, BIT_7);
	phy_write(idx, 0xd2, BIT_3, BIT_3);
	phy_write(idx, 0xd2, BIT_7, BIT_7);
	phy_write(idx, 0x90, BIT_6, BIT_6);
	phy_write(idx, 0x90, BIT_5, BIT_5);
	phy_write(idx, 0xb0, BIT_6, BIT_6);
	phy_write(idx, 0xb0, BIT_5, BIT_5);
	phy_write(idx, 0xd0, BIT_6, BIT_6);
	phy_write(idx, 0xd0, BIT_5, BIT_5);
	phy_write(idx, 0x68, BIT_0, BIT_0);
	//calculate osc counter result
	readback = 0;
	while (readback != 1) {
		phy_read(idx, 0x8f, &readback);
		readback = (readback & BIT_7) >> 7;
	}
	phy_write(idx, 0x90, ~BIT_5, BIT_5);

	readback = 0;
	while (readback != 1) {
		phy_read(idx, 0xaf, &readback);
		readback = (readback & BIT_7) >> 7;
	}
	phy_write(idx, 0xb0, ~BIT_5, BIT_5);

	readback = 0;
	while (readback != 1) {
		phy_read(idx, 0xcf, &readback);
		readback = (readback & BIT_7) >> 7;
	}
	phy_write(idx, 0xd0, ~BIT_5, BIT_5);
	phy_write(idx, 0x94, 0x10, 0x7f);
	phy_write(idx, 0xb4, 0x10, 0x7f);
	phy_write(idx, 0xd4, 0x10, 0x7f);
	phy_write(idx, 0x91, 0x06, 0x0f);
	phy_write(idx, 0xb1, 0x06, 0x0f);
	phy_write(idx, 0xd1, 0x06, 0x0f);
	phy_write(idx, 0x95, 0x40, 0x7f);
	phy_write(idx, 0xb5, 0x40, 0x7f);
	phy_write(idx, 0xd5, 0x40, 0x7f);

	phy_write(idx, 0x63, ~BIT_3, BIT_3);
	phy_write(idx, 0x63, ~BIT_7, BIT_7);
	phy_write(idx, 0x3b, ~BIT_3, BIT_3);
	phy_write(idx, 0x92, ~BIT_3, BIT_3);
	phy_write(idx, 0x92, ~BIT_7, BIT_7);
	phy_write(idx, 0xb2, ~BIT_3, BIT_3);
	phy_write(idx, 0xb2, ~BIT_7, BIT_7);
	phy_write(idx, 0xd2, ~BIT_3, BIT_3);
	phy_write(idx, 0xd2, ~BIT_7, BIT_7);
	readback = 0;
	while (readback != 1) {
		phy_read(idx, 0x3e, &readback);
		readback = (readback & BIT_4) >> 4;
	}
	cphy_osc_result_cal(idx, dt_info->bps_per_lane);


}

void dphy_afe_cali(struct csi_dt_node_info *dt_info, int32_t idx){
		phy_write(idx, 0x90, BIT_6 | BIT_5, BIT_6 | BIT_5);
		phy_write(idx, 0xb0, BIT_6 | BIT_5, BIT_6 | BIT_5);
		phy_write(idx, 0xd0, BIT_6 | BIT_5, BIT_6 | BIT_5);
		udelay(10);
		phy_write(idx, 0x90, 0, BIT_6 | BIT_5);
		phy_write(idx, 0xb0, 0, BIT_6 | BIT_5);
		phy_write(idx, 0xd0, 0, BIT_6 | BIT_5);

}

void csi_phy_init(struct csi_dt_node_info *dt_info, int32_t idx)
{
	struct csi_phy_info *phy = NULL;
	struct regmap *anlg_phy_syscon;

	if (!dt_info) {
		pr_err("fail to get valid phy ptr\n");
		return;
	}

	phy = &dt_info->phy;
	if (!phy) {
		pr_err("fail to get valid phy ptr\n");
		return;
	}

	csi_ahb_reset(phy, dt_info->controller_id);
	csi_reset_controller(idx);
	csi_shut_down_phy(0, idx);
	csi_reset_phy(idx);
    if (phy->phy_id == PHY_CPHY || phy->phy_id == PHY_4LANE || phy->phy_id == PHY_2P2 ||
		phy->phy_id == PHY_2P2_M || phy->phy_id == PHY_2P2_S){
		anlg_phy_syscon = phy->anlg_phy_g4l_syscon;
	}else{
		anlg_phy_syscon = phy->anlg_phy_g4_syscon;
	}
	csi_phy_testclr(dt_info->controller_id, &dt_info->phy);

	switch (phy->phy_id) {
	case PHY_4LANE:
	case PHY_4LANE1:
				CSI_REG_MWR(idx, PHY_SEL, BIT_0, 0);
				CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_REG_SEL, 1 << 2);
				phy_write(idx, 0x62, BIT_0, BIT_0);
				phy_write(idx, 0x6e, BIT_0, BIT_0);
				phy_write(idx, 0x61, 0x82, 0x82);
				phy_write(idx, 0x60, BIT_7 | BIT_0, BIT_7 | BIT_0);
				if(dt_info->lane_seq == 0xfffff){
					pr_err("combo dphy pn swap\n");
					//need swap for combo dphy0 using only for s5kgw1sp03/imx586
					//pn swap: clk 0x01[0]	0x3d[7]  lane3: 0x7d[7] lane2: 0x6d[7]  lane1: 0x5d[7]  lane0: 0x4d[7]
					//lane3-lane0 seq: 0xfe default: 0xe4=11100100
					//lane3:0x7d[6:5]=11 lane2:0x6d[6:5]=10 lane1:0x5d[6:5]=01 lane0:0x4d[6:5]=00
					phy_write(idx, 0x01, BIT_0, BIT_0);
					phy_write(idx, 0x3d, BIT_7, BIT_7);
					phy_write(idx, 0x4d, BIT_7, BIT_7);
					phy_write(idx, 0x5d, BIT_7, BIT_7);
					phy_write(idx, 0x6d, BIT_7, BIT_7);
					phy_write(idx, 0x7d, BIT_7, BIT_7);

				}
				dphy_afe_cali(dt_info, idx);
				CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_REG_SEL, ~(1 << 2));
		break;
	case PHY_CPHY1:
	case PHY_CPHY:
		/* phy: cphy phy */
		CSI_REG_MWR(idx, PHY_SEL, BIT_0, 1);
		CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_REG_SEL, 1 << 2);
		phy_write(idx, 0x3c, 0x08, 0xff);
		regmap_update_bits(anlg_phy_syscon,0x40, BIT_2, BIT_2);//enabel lane
		//TODO: need swap for combo cphy0 using imx586
/*		A2D:
		LANE0 base reg 8'h80  LANE1 base reg 8'ha0  LANE2 base reg 8'hc0
		8'h8e/ae/ce  [1:0] for lane swap: 2'b00: lane swap lane0  2'b01: lane swap lane1	2'b10: lane swap lane2
			              [3:2] for A swap: 2'b00: lane A swap A 2'b01: lane A swap B 2'b10: lane A swap C
			              [5:4] for B swap: 2'b00: lane B swap A 2'b01: lane B swap B 2'b10: lane B swap C
			              [7:6] for C swap: 2'b00: lane C swap A 2'b01: lane C swap B 2'b10: lane C swap C
		D2A:
		8'h41 [1:0] for lane0 swap: 00: lane0 swap lane0 01: lane0 swap lane1 10: lane0 swap lane2
			  [3:2] for lane1 swap: 00: lane1 swap lane0 01: lane1 swap lane1 10: lane1 swap lane2
			  [5:4] for lane2 swap: 00: lane2 swap lane0 	01: lane2 swap lane1 10: lane2 swap lane2
        */
		//eg: CPHY lane0 swap lane1:
		//8'h8e[1:0]=2'b01   : A2D phy lane0 swap sensor lane1
		//8'h41[1:0]=2'b01   : D2A phy lane0 swap sensor lane1
		//8'h8e[3:2]=2'b10(A->C) 8'h8e[5:4]=2'b00(B->A) 8'h8e[7:6]=2'b01(C->B)////A B C swap

		//8'hae[1:0]=2'b00   : A2D phy lane1 swap sensor lane0
		//8'h41[3:2]=2'b00   : D2A phy lane1 swap sensor lane0
		//8'hae[3:2]=2'b00 (A->A) 8'hae[5:4]=2'b01(B->B) 8'hae[7:6]=2'b10(C->C)
		cphy_cdr_init(dt_info, idx);
		phy_write(idx, 0x69, 0x0c, 0x0c);
		phy_write(idx, 0x3b, 0x09, 0x09);
		CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_REG_SEL, ~(1 << 2));
		break;
	case PHY_2P2RO:
	case PHY_2P2:
		/* 2p2lane/RO phy as a 4lane phy  */
		CSI_REG_MWR(idx, 0x70, BIT_5, ~BIT_5);
		CSI_REG_MWR(idx, PHY_PD_N, BIT_1, BIT_1);
		CSI_REG_MWR(idx, RST_DPHY_N, BIT_1, BIT_1);
		phy_csi_path_clr_cfg(dt_info, idx);
		csi_2p2l_2lane_phy_testclr(phy);
		phy_csi_path_cfg(dt_info, idx);
		break;
	case PHY_2P2RO_M:
	case PHY_2P2_M:
		/* 2p2lane phy as a 2lane M phy  */
		CSI_REG_MWR(idx, 0x70, BIT_6|BIT_5, BIT_6|BIT_5);//for 1lane eco set bit6
		phy_csi_path_clr_cfg(dt_info, idx);
		csi_2p2l_2lane_phy_testclr(phy);
		phy_csi_path_cfg(dt_info, idx);
		break;
	case PHY_2P2RO_S:
	case PHY_2P2_S:
		/* 2p2lane phy as a 2lane S phy  */
		CSI_REG_MWR(idx, 0x70, BIT_5, BIT_5);
		CSI_REG_MWR(idx, PHY_PD_N, BIT_1, BIT_1);
		CSI_REG_MWR(idx, RST_DPHY_N, BIT_1, BIT_1);
		phy_csi_path_clr_cfg(dt_info, idx);
		csi_2p2l_2lane_phy_testclr(phy);
		phy_csi_path_cfg(dt_info, idx);

		regmap_update_bits(anlg_phy_syscon,0x68,BIT_4 | BIT_3 | BIT_2, BIT_4 | BIT_3 | BIT_2);//enabel lane
		regmap_update_bits(anlg_phy_syscon,0xac,BIT_11 | BIT_10 | BIT_9, BIT_11 | BIT_10 | BIT_9);//enabel lane
		break;
	default:
		pr_err("fail to get valid phy id %d\n", phy->phy_id);
		return;
	}
}


void csi_set_on_lanes(uint8_t lanes, int32_t idx)
{
	CSI_REG_MWR(idx, LANE_NUMBER, 0x7, (lanes - 1));
}

/* PHY power down input, active low */
void csi_shut_down_phy(uint8_t shutdown, int32_t idx)
{
	CSI_REG_MWR(idx, PHY_PD_N, BIT_0, shutdown ? 0 : 1);
}

void csi_reset_controller(int32_t idx)
{
	CSI_REG_MWR(idx, RST_CSI2_N, BIT_0, 0);
	CSI_REG_MWR(idx, RST_CSI2_N, BIT_0, 1);
}

void csi_reset_phy(int32_t idx)
{
	CSI_REG_MWR(idx, RST_DPHY_N, BIT_0, 0);
	CSI_REG_MWR(idx, RST_DPHY_N, BIT_0, 1);
}

void csi_event_enable(int32_t idx)
{
	CSI_REG_WR(idx, MASK0, CSI_MASK0);
	CSI_REG_WR(idx, MASK1, CSI_MASK1);
	CSI_REG_WR(idx, CPHY_ERR2_MASK, CSI_MASK2);
}

void csi_close(int32_t idx)
{
	csi_shut_down_phy(1, idx);
	csi_reset_phy(idx);
	csi_reset_controller(idx);
}
