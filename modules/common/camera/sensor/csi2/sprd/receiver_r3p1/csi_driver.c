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

//#include <dt-bindings/soc/sprd,sharkl6-regs.h>
//#include <dt-bindings/soc/sprd,sharkl6-mask.h>
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

static unsigned long s_csi_regbase[SPRD_SENSOR_ID_MAX];
static unsigned long csi_dump_regbase[CSI_MAX_COUNT];
static spinlock_t csi_dump_lock[CSI_MAX_COUNT] = {
	__SPIN_LOCK_UNLOCKED(csi_dump_lock),
	__SPIN_LOCK_UNLOCKED(csi_dump_lock),
	__SPIN_LOCK_UNLOCKED(csi_dump_lock)
};
void phy_csi_path_clr_cfg(struct csi_dt_node_info *dt_info, int sensor_id);

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
						IPG_BAYER_PATTERN_BGGR);
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
	} else
		CSI_REG_MWR(idx, MODE_CFG, IPG_ENABLE_MASK, ~IPG_ENABLE);

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
	for (addr = IP_REVISION; addr <= 0x90; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			REG_RD(regbase + addr),
			REG_RD(regbase + addr + 4),
			REG_RD(regbase + addr + 8),
			REG_RD(regbase + addr + 12));
	}
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

#ifdef DEBUG_CHIP_TOP
		reg_dump_rd(0x645b0000, 0xff, "ana-g10");
		reg_dump_rd(0x30000000, 0xdf, "mm-ahb");
		reg_dump_rd(0x30010000, 0x100, "mm-clk");
		//reg_dump_rd(0x64900000, 0xff, "aon-apb");
		//reg_dump_rd(0x64910100, 0x1ff, "aon-pm");
#endif
	spin_unlock_irqrestore(&csi_dump_lock[idx], flag);
}
#if 0
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
#endif
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
#define ANALOG_G10_REG_BASE 0x645b0000
#define MM_AHB_REG_BASE 0x30000000

void csi_phy_testclr_init(struct csi_phy_info *phy)
{
	unsigned int mask = 0;

	mask = BIT_22 | BIT_19;//0x400000 | 0x80000;
	//MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_M_EN
	//|MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_S_EN


	regmap_update_bits(phy->anlg_phy_g10_syscon,0xb4,
	//REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST,
	//REG_ANLG_PHY_G10_RF_ANALOG_MIPI_CSI_COMBO_CSI_4L_BIST_TEST
		mask, mask);


	switch (phy->phy_id) {
//	case PHY_CPHY:
		/* phy: cphy phy */
//		break;
	case PHY_4LANE:
		/* phy: 4lane phy */
		break;
	case PHY_2P2RO:
		/* phy: 2lane phy */
		regmap_update_bits(phy->anlg_phy_g10_syscon,0x0c,BIT_4, BIT_4);
		break;
	case PHY_2P2RO_M:
	case PHY_2P2RO_S:
		regmap_update_bits(phy->anlg_phy_g10_syscon,0x0c,BIT_4, 0);
		break;
	case PHY_2P2:
		/* 2p2lane phy as a 4lane phy  */

		regmap_update_bits(phy->anlg_phy_g10_syscon,0x38,0x10,0x10);
			//REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CTRL_CSI_2P2L
			//REG_ANLG_PHY_G10_RF_ANALOG_MIPI_CSI_2P2LANE_CTRL_CSI_2P2L,


		break;
	case PHY_2P2_M:
	case PHY_2P2_S:
		/* 2p2lane phy as a 2lane phy  */

		regmap_update_bits(phy->anlg_phy_g10_syscon,0x38,0x10,(int)~0x10);
		//	REG_ANLG_PHY_G10_RF_ANALOG_MIPI_CSI_2P2LANE_CTRL_CSI_2P2L,
		//	MASK_ANLG_PHY_G10_RF_ANALOG_MIPI_CSI_2P2LANE_CSI_MODE_SEL,
		//(int)~MASK_ANLG_PHY_G10_RF_ANALOG_MIPI_CSI_2P2LANE_CSI_MODE_SEL);

		break;
	default:
		pr_err("fail to get valid phy id %d\n", phy->phy_id);
		return;
	}
}

void csi_phy_testclr(int sensor_id, struct csi_phy_info *phy)
{
	unsigned int mask = 0;

	switch (phy->phy_id) {
//	case PHY_CPHY:
	case PHY_4LANE:
	case PHY_2P2RO:
		CSI_REG_MWR(sensor_id, PHY_TEST_CRTL0, PHY_TESTCLR, 1);//TODO clr 2p2 ro/m/s
		CSI_REG_MWR(sensor_id, PHY_TEST_CRTL0, PHY_TESTCLR, 0);
		break;
	case PHY_2P2RO_M:
	case PHY_2P2RO_S:
	case PHY_2P2:
	case PHY_2P2_M:
	case PHY_2P2_S:
		mask = 0x1;//MASK_ANLG_PHY_G10_RF_ANALOG_MIPI_CSI_2P2LANE_DSI_TESTCLR_DB;

		regmap_update_bits(phy->anlg_phy_g10_syscon,
		0x7c,//REG_ANLG_PHY_G10_RF_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB,
		mask, mask);

		CSI_REG_MWR(sensor_id, PHY_TEST_CRTL0, PHY_TESTCLR, 1);

		regmap_update_bits(phy->anlg_phy_g10_syscon,
		0x7c,//REG_ANLG_PHY_G10_RF_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB,
		mask, ~mask);

		CSI_REG_MWR(sensor_id, PHY_TEST_CRTL0, PHY_TESTCLR, 0);
		break;
	default:
		pr_err("fail to get valid phy id %d\n", phy->phy_id);
		return;
	}
}

static void csi_2p2l_2lane_phy_testclr(struct csi_phy_info *phy)
{
	unsigned int mask_sel = 0;
	unsigned int mask_testclr = 0;
	unsigned int reg_val = 0;

	switch (phy->phy_id) {
//	case PHY_CPHY:
//		break;
	case PHY_4LANE:
		break;
	case PHY_2P2RO:
		break;
	case PHY_2P2:
		break;
	case PHY_2P2_S:
		mask_sel = 0x40000;
		//MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_S_SEL

		mask_testclr = 0x20000;
		//MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_S
		//MASK_ANLG_PHY_G10_RF_ANALOG_MIPI_CSI_COMBO_CSI_2P2L_TESTCLR_S;
		reg_val = 0x00B4;
		break;
	case PHY_2P2_M:
		mask_sel = 0x200000;
		//MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_M_SEL

		mask_testclr = 0x100000;
		//MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_M
		reg_val = 0x00B4;
		break;
	case PHY_2P2RO_S:
		mask_sel = BIT_1;
		//MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_RO_TESTCLR_S_SEL
		mask_testclr = BIT_0;
		//MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_RO_TESTCLR_S
		reg_val = 0x00C4;
		break;
	case PHY_2P2RO_M:
		mask_sel = BIT_4;
		//MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_RO_TESTCLR_M_SEL
		mask_testclr = BIT_3;
		//MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_RO_TESTCLR_M
		reg_val = 0x00C4;
		break;
	default:
		pr_err("fail to get valid phy id %d\n", phy->phy_id);
		return;
	}

	regmap_update_bits(phy->anlg_phy_g10_syscon, reg_val,
		//REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST
		mask_sel, mask_sel);
	regmap_update_bits(phy->anlg_phy_g10_syscon,reg_val,
		//REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST
		mask_testclr, mask_testclr);
	udelay(1);
	regmap_update_bits(phy->anlg_phy_g10_syscon,reg_val,
		//REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST
		mask_sel, ~mask_sel);
	regmap_update_bits(phy->anlg_phy_g10_syscon,reg_val,
		//REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST
		mask_testclr, ~mask_testclr);

}

void csi_phy_power_down(struct csi_dt_node_info *csi_info,
			unsigned int sensor_id, int is_eb)
{
	uint32_t shutdownz = 0;
	uint32_t reg = 0;
	struct csi_phy_info *phy = &csi_info->phy;

	if (!phy || !csi_info) {
		pr_err("fail to get valid phy ptr\n");
		return;
	}

	switch (csi_info->controller_id) {
	case CSI_RX0:
		shutdownz = 0x4000000;
		//MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_FORCE_CSI_PHY_SHUTDOWNZ
		reg = 0x00B4;
		//REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST
		break;
	case CSI_RX1:
		shutdownz = 0x1000000;
	//MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_FORCE_CSI_S_PHY_SHUTDOWNZ
		reg = 0x00B4;
		//REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST
		break;
	case CSI_RX2:
		shutdownz = 0x2000000;
		//MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_FORCE_CSI_PHY_SHUTDOWNZ//////////TODO no 2lane ?????
		reg = 0x00B4;
		//REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST//////////TODO no 2lane ?????
		break;
	default:
		pr_err("fail to get valid csi_rx id\n");
	}

	if (is_eb)
		regmap_update_bits(phy->anlg_phy_g10_syscon,//reg base:0x645b0000
		reg, shutdownz, ~shutdownz);
	else
		regmap_update_bits(phy->anlg_phy_g10_syscon,
		reg, shutdownz, shutdownz);
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
		flag = BIT_9;//MASK_MM_AHB_RF_MIPI_CSI0_SOFT_RST;
		break;
	case CSI_RX1:
		flag = BIT_8;//MASK_MM_AHB_RF_MIPI_CSI1_SOFT_RST;
		break;
	case CSI_RX2:
		flag = BIT_7;//MASK_MM_AHB_RF_MIPI_CSI2_SOFT_RST;
		break;
	default:
		pr_err("fail to get valid csi id %d\n", csi_id);
	}
	regmap_update_bits(phy->cam_ahb_syscon,
			0x04/*REG_MM_AHB_RF_AHB_RST*/, flag, flag);
	udelay(1);
	regmap_update_bits(phy->cam_ahb_syscon,
			0x04/*REG_MM_AHB_RF_AHB_RST*/, flag, ~flag);

	return 0;
}

void csi_controller_enable(struct csi_dt_node_info *dt_info)
{
	struct csi_phy_info *phy = NULL;
	uint32_t mask_eb = 0;
	unsigned long flag = 0;

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
		mask_eb = BIT_6;//MASK_MM_AHB_RF_CSI0_EB;
		break;
	}
	case CSI_RX1: {
		mask_eb = BIT_5;//MASK_MM_AHB_RF_CSI1_EB;
		break;
	}
	case CSI_RX2: {
		mask_eb = BIT_4;//MASK_MM_AHB_RF_CSI2_EB;
		break;
	}
	default:
		pr_err("fail to get valid csi id\n");
		return;
	}

	spin_lock_irqsave(&csi_dump_lock[dt_info->controller_id], flag);

	csi_dump_regbase[dt_info->controller_id] = dt_info->reg_base;
	regmap_update_bits(phy->cam_ahb_syscon, 0x00,//REG_MM_AHB_RF_AHB_EB,
			mask_eb, mask_eb);
	spin_unlock_irqrestore(&csi_dump_lock[dt_info->controller_id], flag);
}

void csi_controller_disable(struct csi_dt_node_info *dt_info, int32_t idx)
{
	struct csi_phy_info *phy = NULL;
	uint32_t mask_eb = 0;
	uint32_t mask_rst = 0;
	unsigned long flag = 0;

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
	phy_csi_path_clr_cfg(dt_info, idx);

	switch (dt_info->controller_id) {
	case CSI_RX0: {
		mask_eb = BIT_6;//MASK_MM_AHB_RF_CSI0_EB;
		mask_rst = BIT_9;//MASK_MM_AHB_RF_MIPI_CSI0_SOFT_RST;
		break;
	}
	case CSI_RX1: {
		mask_eb = BIT_5;//MASK_MM_AHB_RF_CSI1_EB;
		mask_rst = BIT_8;//MASK_MM_AHB_RF_MIPI_CSI1_SOFT_RST;
		break;
	}
	case CSI_RX2: {
		mask_eb = BIT_4;//MASK_MM_AHB_RF_CSI2_EB;
		mask_rst = BIT_7;//MASK_MM_AHB_RF_MIPI_CSI2_SOFT_RST;
		break;
	}
	default:
		pr_err("fail to get valid csi id\n");
		return;
	}

	spin_lock_irqsave(&csi_dump_lock[dt_info->controller_id], flag);

	csi_dump_regbase[dt_info->controller_id] = 0;

	//regmap_update_bits(phy->cam_ahb_syscon, 0x00,//REG_MM_AHB_RF_AHB_EB,
	//		mask_eb, mask_eb);
	regmap_update_bits(phy->cam_ahb_syscon, 0x04,//REG_MM_AHB_RF_AHB_RST,
			mask_rst, ~mask_rst);
	udelay(1);
	regmap_update_bits(phy->cam_ahb_syscon, 0x04,//REG_MM_AHB_RF_AHB_RST,
			mask_rst, ~mask_rst);

	spin_unlock_irqrestore(&csi_dump_lock[dt_info->controller_id], flag);
}

void phy_csi_path_cfg(struct csi_dt_node_info *dt_info, int sensor_id)
{
	struct csi_phy_info *phy = NULL;
	uint32_t phy_sel_mask;
	uint32_t phy_sel_val;
	uint32_t val = 0;

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

	switch (phy->phy_id) {
	case PHY_2P2: {
		phy_sel_val = (dt_info->controller_id + 1) & 0x03 ;		
		phy_sel_mask = 0x03;
		phy_sel_val += ((dt_info->controller_id + 1) & 0x03) << 2 ;
		phy_sel_mask += 0x03 << 2;
		break;
	}
	case PHY_2P2_M: {
		phy_sel_val = (dt_info->controller_id + 1) & 0x03;
		phy_sel_mask = 0x03;
		break;
	}
	case PHY_2P2_S: {
		phy_sel_val = ((dt_info->controller_id + 1) & 0x03) << 2;
		phy_sel_mask = 0x03 << 2;
		break;
	}
	case PHY_4LANE: {
		phy_sel_val = ((dt_info->controller_id + 1) & 0x03) << 4;
		phy_sel_mask = 0x03 << 4;
		break;
	}
	case PHY_2P2RO: {
		phy_sel_val = ((dt_info->controller_id + 1) & 0x03) << 6;
		phy_sel_mask = 0x03 << 6;
		phy_sel_val += ((dt_info->controller_id + 1) & 0x03) << 8;
		phy_sel_mask += 0x03 << 8;
	/*	if(dt_info->controller_id == 0){
			phy_sel_val += 0x00 << 4;
			phy_sel_mask += 0x03 << 4;
		}*/
		break;
	}
	case PHY_2P2RO_M: {
		phy_sel_val = ((dt_info->controller_id + 1) & 0x03) << 6;
		phy_sel_mask = 0x03 << 6;
		break;
	}
	case PHY_2P2RO_S: {
		phy_sel_val = ((dt_info->controller_id + 1) & 0x03) << 8;
		phy_sel_mask = 0x03 << 8;
		break;
	}
	default:
		pr_err("fail to get valid csi phy id\n");
		return;
	}
	regmap_update_bits(phy->cam_ahb_syscon,	0x30, phy_sel_mask, phy_sel_val);
	regmap_read(phy->cam_ahb_syscon, 0x30, &val);
	pr_info("%s csi, id %d phy %d reg val 0x%x\n", __func__, dt_info->controller_id,
		phy->phy_id, val);


}

void phy_csi_path_clr_cfg(struct csi_dt_node_info *dt_info, int sensor_id)
{
	struct csi_phy_info *phy = NULL;
	uint32_t phy_sel_val = 0;
	uint32_t phy_sel_mask = 0;

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

		switch (phy->phy_id) {
		case PHY_2P2: {
			phy_sel_val = 0 ; 	
			phy_sel_mask = 0x03;
			phy_sel_val += 0 ;
			phy_sel_mask += 0x03 << 2;
			break;
		}
		case PHY_2P2_M: {
			phy_sel_val = 0;
			phy_sel_mask = 0x03;
			break;
		}
		case PHY_2P2_S: {
			phy_sel_val = 0;
			phy_sel_mask = 0x03 << 2;
			break;
		}
		case PHY_4LANE: {
			phy_sel_val = 0;
			phy_sel_mask = 0x03 << 4;
			break;
		}
		case PHY_2P2RO: {
			phy_sel_val = 0;
			phy_sel_mask = 0x03 << 6;
			phy_sel_val += 0;
			phy_sel_mask += 0x03 << 8;
			break;
		}
		case PHY_2P2RO_M: {
			phy_sel_val = 0;
			phy_sel_mask = 0x03 << 6;
			break;
		}
		case PHY_2P2RO_S: {
			phy_sel_val = 0;
			phy_sel_mask = 0x03 << 8;
			break;
		}
		default:
			pr_err("fail to get valid csi phy id\n");
			return;
		}
		regmap_update_bits(phy->cam_ahb_syscon, 0x30, phy_sel_mask, phy_sel_val);//mm_ahb: 0x30000000
	

}



void csi_phy_init(struct csi_dt_node_info *dt_info, int32_t idx)
{
	struct csi_phy_info *phy = NULL;

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

	switch (phy->phy_id) {
	case PHY_4LANE:
	case PHY_2P2RO:
	case PHY_2P2:
		phy_csi_path_cfg(dt_info, idx);
		break;
	case PHY_2P2RO_M:
	case PHY_2P2RO_S:
	case PHY_2P2_M:
	case PHY_2P2_S:
		csi_2p2l_2lane_phy_testclr(phy);
		phy_csi_path_cfg(dt_info, idx);
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
	csi_reset_controller(idx);
	csi_reset_phy(idx);
}
