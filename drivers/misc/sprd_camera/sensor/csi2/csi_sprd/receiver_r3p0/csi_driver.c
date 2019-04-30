/*
 * Copyright (C) 2019 Unisoc Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <dt-bindings/soc/sprd,sharkl5pro-regs.h>
#include <dt-bindings/soc/sprd,sharkl5pro-mask.h>
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
#include <video/sprd_mm.h>
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
#define IPG_COLOR_BAR_W_MASK		(0xff << 13)
#define IPG_IMAGE_W_MASK		(0x1FF << 4)
#define IPG_HSYNC_EN_MASK		BIT_3
#define IPG_COLOR_BAR_MODE_MASK	        BIT_2
#define IPG_IMAGE_MODE_MASK		BIT_1
#define IPG_ENABLE_MASK			BIT_0

#define IPG_IMAGE_W			3264
#define IPG_IMAGE_H			2448

#define IPG_IMAGE_H_REG			(((IPG_IMAGE_H)/8) << 21)
#define IPG_COLOR_BAR_W			(((IPG_IMAGE_W)/24) << 13)
#define IPG_IMAGE_W_REG			(((IPG_IMAGE_W)/16) << 4)
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

int csi_init_reg_base(unsigned long reg_base, int32_t idx)
{
	s_csi_regbase[idx] = reg_base;

	return 0;
}

void csi_ipg_mode_cfg(uint32_t idx, int enable)
{
	if (enable) {
		CSI_REG_MWR(idx, MODE_CFG,
			IPG_IMAGE_H_MASK, IPG_IMAGE_H_REG);
		CSI_REG_MWR(idx, MODE_CFG,
			IPG_COLOR_BAR_W_MASK, IPG_COLOR_BAR_W);
		CSI_REG_MWR(idx, MODE_CFG, IPG_IMAGE_W_MASK, IPG_IMAGE_W_REG);
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
		if (!IPG_IMAGE_MODE) {
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
	for (addr = IP_REVISION; addr <= IPG_OTHER_CFG0; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			REG_RD(regbase + addr),
			REG_RD(regbase + addr + 4),
			REG_RD(regbase + addr + 8),
			REG_RD(regbase + addr + 12));
	}
	spin_unlock_irqrestore(&csi_dump_lock[idx], flag);
}

/* phy testclear used to reset phy to right default state */
static void phy_cfg_clr(int32_t idx)
{
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLR, 1);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLR, 0);
	udelay(1);
}

static void phy_cfg_done(int32_t idx)
{
	CSI_REG_MWR(idx, PHY_TEST_CRTL1, PHY_TESTEN, 0 << 16);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLK, 1 << 1);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL1, PHY_TESTDIN, 0);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL1, PHY_TESTEN, 1 << 16);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLK, 0 << 1);
	udelay(1);
}

static void phy_cfg_start(int32_t idx)
{
	CSI_REG_MWR(idx, PHY_TEST_CRTL1, PHY_TESTEN, 0 << 16);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLK, 1 << 1);
	udelay(1);
}

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
	pr_debug("CPHY Read addr %x value = 0x%x.\r\n", code_in, temp);
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
	pr_debug("CPHY Read addr %x value = 0x%x.\r\n", code_in, temp);
}

static void csi_dphy_lane_cfg(unsigned int phy_id, int32_t idx,
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
				dphy_lane_setting[i].lane_cfg[0]);
			phy_write(idx, 0x5d,
				dphy_lane_setting[i].lane_cfg[1]);
			phy_write(idx, 0x6d,
				dphy_lane_setting[i].lane_cfg[2]);
			phy_write(idx, 0x7d,
				dphy_lane_setting[i].lane_cfg[3]);
			break;
		}
	}

	if (i == PHY_LANE_CFG_COUNTER) {
		pr_err("fail to get valid 4lane phy seq\n");
		return;
	}
}

static void csi_dphy_2p2_testclr_init(struct csi_phy_info *phy)
{
	unsigned int mask = 0;

	mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_2P2L_TESTCLR_S_EN;
	regmap_update_bits(phy->anlg_phy_g10_syscon,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_4L_BIST_TEST,
		mask, mask);

	mask =
	MASK_ANLG_PHY_G10_DBG_SEL_ANALOG_MIPI_CSI_2P2LANE_CSI_IF_SEL_S
	| MASK_ANLG_PHY_G10_DBG_SEL_ANALOG_MIPI_CSI_2P2LANE_CSI_IF_SEL_M
	|MASK_ANLG_PHY_G10_DBG_SEL_ANALOG_MIPI_CSI_2P2LANE_DSI_IF_SEL_DB;
	regmap_update_bits(phy->anlg_phy_g10_syscon,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_REG_SEL_CFG_0,
		mask, mask);

	mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_DSI_IF_SEL_DB;
	regmap_update_bits(phy->anlg_phy_g10_syscon,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_DB,
		mask, ~mask);

	mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_IF_SEL_S;
	regmap_update_bits(phy->anlg_phy_g10_syscon,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_S,
		mask, ~mask);

	mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_IF_SEL_M;
	regmap_update_bits(phy->anlg_phy_g10_syscon,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_M,
		mask, ~mask);

	mask =
	MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_2P2L_TESTCLR_M_SEL
	| MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_2P2L_TESTCLR_S_SEL;
	regmap_update_bits(phy->anlg_phy_g10_syscon,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_4L_BIST_TEST,
		mask, mask);
}

static void csi_dphy_2p2_testclr_set(struct csi_phy_info *phy)
{
	unsigned int mask = 0;

	mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_DSI_TESTCLR_DB;
	regmap_update_bits(phy->anlg_phy_g10_syscon,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB,
		mask, mask);
	udelay(1);
	mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_2P2L_TESTCLR_M
		| MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_2P2L_TESTCLR_S;
	regmap_update_bits(phy->anlg_phy_g10_syscon,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_4L_BIST_TEST,
		mask, mask);
}

static void csi_dphy_2p2_testclr_clear(struct csi_phy_info *phy)
{
	unsigned int mask = 0;

	mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_DSI_TESTCLR_DB;
	regmap_update_bits(phy->anlg_phy_g10_syscon,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB,
		mask, ~mask);
	mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_2P2L_TESTCLR_M
		| MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_2P2L_TESTCLR_S;
	regmap_update_bits(phy->anlg_phy_g10_syscon,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_4L_BIST_TEST,
		mask, ~mask);
}

static void csi_dphy_2p2_reset(struct csi_dt_node_info *csi_info)
{
	struct csi_phy_info *phy = NULL;
	uint32_t cphy_sel_mask;
	uint32_t cphy_sel_val;
	uint32_t mask = 0;

	if (!csi_info) {
		pr_err("fail to get valid dt_info ptr\n");
		return;
	}

	phy = &csi_info->phy;
	if (!phy) {
		pr_err("fail to get valid phy ptr\n");
		return;
	}

	switch (csi_info->controller_id) {
	case CSI_RX0:
	case CSI_RX1:
		cphy_sel_mask = 7 << (csi_info->controller_id * 6);
		cphy_sel_val  = 3 << csi_info->controller_id * 6;

		regmap_update_bits(phy->cam_ahb_syscon,
				REG_MM_AHB_MIPI_CSI_SEL_CTRL,
				cphy_sel_mask, cphy_sel_val);
		mask =
		MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_2P2L_TESTCLR_M
		| MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_2P2L_TESTCLR_S;
		regmap_update_bits(phy->anlg_phy_g10_syscon,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_4L_BIST_TEST,
		mask, mask);
		regmap_update_bits(phy->anlg_phy_g10_syscon,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_4L_BIST_TEST,
		mask, ~mask);
		regmap_update_bits(phy->cam_ahb_syscon,
				REG_MM_AHB_MIPI_CSI_SEL_CTRL,
				cphy_sel_mask, 0x0);
		break;
	case CSI_RX2:
		cphy_sel_mask = 7 << (csi_info->controller_id * 6);
		cphy_sel_val  = 1 << csi_info->controller_id * 6;

		regmap_update_bits(phy->cam_ahb_syscon,
				REG_MM_AHB_MIPI_CSI_SEL_CTRL,
				cphy_sel_mask, cphy_sel_val);

		mask =
		MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_2P2L_TESTCLR_S;
		regmap_update_bits(phy->anlg_phy_g10_syscon,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_4L_BIST_TEST,
		mask, mask);
		regmap_update_bits(phy->anlg_phy_g10_syscon,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_4L_BIST_TEST,
		mask, ~mask);
		regmap_update_bits(phy->cam_ahb_syscon,
				REG_MM_AHB_MIPI_CSI_SEL_CTRL,
				cphy_sel_mask, 0x0);
		cphy_sel_val  = 2 << csi_info->controller_id * 4;
		regmap_update_bits(phy->cam_ahb_syscon,
				REG_MM_AHB_MIPI_CSI_SEL_CTRL,
				cphy_sel_mask, cphy_sel_val);
		mask =
		MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_2P2L_TESTCLR_M;
		regmap_update_bits(phy->anlg_phy_g10_syscon,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_4L_BIST_TEST,
		mask, mask);
		regmap_update_bits(phy->anlg_phy_g10_syscon,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_4L_BIST_TEST,
		mask, ~mask);
		regmap_update_bits(phy->cam_ahb_syscon,
				REG_MM_AHB_MIPI_CSI_SEL_CTRL,
				cphy_sel_mask, 0x0);
		break;
	default:
		pr_err("fail to get valid csi_rx id\n");
	}
}

void csi_phy_power_down(struct csi_dt_node_info *csi_info,
			unsigned int sensor_id, int is_eb)
{

	uint32_t ps_pd_l = 0;
	uint32_t ps_pd_s = 0;
	uint32_t iso_sw = 0;
	uint32_t shutdownz = 0;
	uint32_t reg = 0;
	uint32_t ps_pd_reg = 0;
	uint32_t ios_sw_reg = 0;
	struct csi_phy_info *phy = &csi_info->phy;

	if (!phy || !csi_info) {
		pr_err("fail to get valid phy ptr\n");
		return;
	}

	switch (csi_info->controller_id) {
	case CSI_RX0:
		shutdownz =
		MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_FORCE_CSI_PHY_SHUTDOWNZ;
		reg = REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_4L_BIST_TEST;
		break;
	case CSI_RX1:
		shutdownz =
		MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_FORCE_CSI_S_PHY_SHUTDOWNZ;
		reg = REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_4L_BIST_TEST;
		break;
	case CSI_RX2:
		shutdownz =
		MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2LANE_FORCE_CSI_PHY_SHUTDOWNZ;
		reg = REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2LANE_MIPI_PHY_BIST_TEST;
		break;
	default:
		pr_err("fail to get valid csi_rx id\n");
	}

	switch (phy->phy_id) {
	case PHY_2P2:
		/* 2p2lane phy as a 4lane phy  */
		ps_pd_l =
		MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_PS_PD_L;
		ps_pd_s =
		MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_PS_PD_S;
		iso_sw =
		MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_ISO_SW_EN;
		ps_pd_reg =
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CTRL_CSI_2P2L;
		ios_sw_reg =
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2L_ISO_SW;
		regmap_update_bits(phy->anlg_phy_g10_syscon,
			REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CTRL_CSI_2P2L,
			MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_MODE_SEL,
			MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_MODE_SEL);
		break;
	case PHY_CPHY:
	case PHY_4LANE:
		/* phy: 4lane phy */
		ps_pd_l =
		MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_PS_PD_L;
		ps_pd_s =
		MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_PS_PD_S;
		iso_sw =
		MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_ISO_SW_EN;
		ps_pd_reg =
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CTRL_CSI_4L;
		ios_sw_reg =
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_COMBO_CSI_4L_ISO_SW;
		break;
	case PHY_2LANE:
		/* phy: 2lane phy */
		ps_pd_l = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2LANE_CSI_PS_PD_L;
		ps_pd_s = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2LANE_CSI_PS_PD_S;
		iso_sw = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2LANE_CSI_ISO_SW_EN;
		ps_pd_reg = REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2LANE_CTRL_CSI_2L;
		ios_sw_reg =
			REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2LANE_CSI_2L_ISO_SW;
		break;
	case PHY_2P2_M:
	case PHY_2P2_S:
		/* 2p2lane phy as a 2lane phy  */
		ps_pd_l = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_PS_PD_L;
		ps_pd_s = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_PS_PD_S;
		iso_sw =
			MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_ISO_SW_EN;
		ps_pd_reg =
			REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CTRL_CSI_2P2L;
		ios_sw_reg =
			REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2L_ISO_SW;
		regmap_update_bits(phy->anlg_phy_g10_syscon,
			REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CTRL_CSI_2P2L,
			MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_MODE_SEL,
		(int)~MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_MODE_SEL);
		break;
	default:
		pr_err("fail to get valid phy id %d\n", phy->phy_id);
		return;
	}

	if (is_eb) {
		regmap_update_bits(phy->aon_apb_syscon,
				ps_pd_reg,
				ps_pd_l | ps_pd_s,
				ps_pd_l | ps_pd_s);
		regmap_update_bits(phy->aon_apb_syscon,
				ios_sw_reg,
				iso_sw, iso_sw);
		regmap_update_bits(phy->anlg_phy_g10_syscon,
		reg, shutdownz, ~shutdownz);
	} else {
		/* According to the time sequence of CSI-DPHY INIT,
		 * need pull down POWER, DPHY-reset and CSI-2 controller reset
		 */
		csi_shut_down_phy(1, sensor_id);
		csi_reset_shut_down(1, sensor_id);
		if (phy->phy_id == PHY_2P2 ||
			phy->phy_id == PHY_2P2_M ||
			phy->phy_id == PHY_2P2_S) {
			csi_dphy_2p2_testclr_init(phy);
			csi_dphy_2p2_testclr_set(phy);
		} else if (phy->phy_id == PHY_4LANE ||
				phy->phy_id == PHY_2LANE ||
				phy->phy_id == PHY_CPHY)
			CSI_REG_MWR(sensor_id, PHY_TEST_CRTL0, PHY_TESTCLR, 1);
		udelay(1);

		regmap_update_bits(phy->aon_apb_syscon,
				ps_pd_reg,
				ps_pd_s,
				~ps_pd_s);
		udelay(200);

		regmap_update_bits(phy->aon_apb_syscon,
				ps_pd_reg,
				ps_pd_l,
				~ps_pd_l);
		regmap_update_bits(phy->aon_apb_syscon,
				ios_sw_reg,
				iso_sw,
				~iso_sw);

		/* set phy force shutdown */
		regmap_update_bits(phy->anlg_phy_g10_syscon,
		reg, shutdownz, shutdownz);
		if (phy->phy_id == PHY_2P2 ||
			phy->phy_id == PHY_2P2_M ||
			phy->phy_id == PHY_2P2_S) {
			csi_dphy_2p2_testclr_clear(phy);
			csi_dphy_2p2_reset(csi_info);
		} else if (phy->phy_id == PHY_4LANE ||
				phy->phy_id == PHY_2LANE ||
				phy->phy_id == PHY_CPHY) {
			CSI_REG_MWR(sensor_id, PHY_TEST_CRTL0, PHY_TESTCLR, 0);
			udelay(1);
		}
		/* According to the time sequence of CSI-DPHY INIT,
		 * need pull up POWER, DPHY-reset and CSI-2 controller reset
		 */
		csi_shut_down_phy(0, sensor_id);
		csi_reset_shut_down(0, sensor_id);
	}
}

int csi_ahb_reset(struct csi_phy_info *phy, unsigned int csi_id)
{
	unsigned int flag = 0;

	if (!phy) {
		pr_err("fail to get valid phy ptr\n");
		return -EINVAL;
	}
	pr_info("csi, id %d dphy %d\n", csi_id, phy->phy_id);

	switch (csi_id) {
	case CSI_RX0:
		flag = MASK_MM_AHB_MIPI_CSI0_SOFT_RST;
		break;
	case CSI_RX1:
		flag = MASK_MM_AHB_MIPI_CSI1_SOFT_RST;
		break;
	case CSI_RX2:
		flag = MASK_MM_AHB_MIPI_CSI2_SOFT_RST;
		break;
	default:
		pr_err("fail to get valid csi id %d\n", csi_id);
	}
	regmap_update_bits(phy->cam_ahb_syscon,
			REG_MM_AHB_AHB_RST, flag, flag);
	udelay(1);
	regmap_update_bits(phy->cam_ahb_syscon,
			REG_MM_AHB_AHB_RST, flag, ~flag);

	return 0;
}

void csi_controller_enable(struct csi_dt_node_info *dt_info, int32_t idx)
{
	struct csi_phy_info *phy = NULL;
	uint32_t phy_sel_mask;
	uint32_t phy_sel_val;
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

	pr_info("%s csi, id %d dphy %d\n", __func__, dt_info->controller_id,
		phy->phy_id);

	switch (dt_info->controller_id) {
	case CSI_RX0: {
		mask_eb = MASK_MM_AHB_CSI0_EB;
		mask_rst = MASK_MM_AHB_MIPI_CSI0_SOFT_RST;
		break;
	}
	case CSI_RX1: {
		mask_eb = MASK_MM_AHB_CSI1_EB;
		mask_rst = MASK_MM_AHB_MIPI_CSI1_SOFT_RST;
		break;
	}
	case CSI_RX2: {
		mask_eb = MASK_MM_AHB_CSI2_EB;
		mask_rst = MASK_MM_AHB_MIPI_CSI2_SOFT_RST;
		break;
	}
	default:
		pr_err("fail to get valid csi id\n");
		return;
	}

	spin_lock_irqsave(&csi_dump_lock[dt_info->controller_id], flag);

	csi_dump_regbase[dt_info->controller_id] = dt_info->reg_base;
	regmap_update_bits(phy->cam_ahb_syscon, REG_MM_AHB_AHB_EB,
			mask_eb, mask_eb);
	regmap_update_bits(phy->cam_ahb_syscon, REG_MM_AHB_AHB_RST,
			mask_rst, mask_rst);
	udelay(1);
	regmap_update_bits(phy->cam_ahb_syscon, REG_MM_AHB_AHB_RST,
			mask_rst, ~mask_rst);

	spin_unlock_irqrestore(&csi_dump_lock[dt_info->controller_id], flag);

	if (dt_info->controller_id == CSI_RX0 ||
			dt_info->controller_id == CSI_RX1) {
		/* cfg the value of  mipi_csi_dphy_c0/1_sel */
		phy_sel_mask = 0x3f << (dt_info->controller_id * 6);

		switch (phy->phy_id) {
		case PHY_2P2: {
			phy_sel_val = 0x1;
			break;
		}
		case PHY_CPHY:
		case PHY_4LANE: {
			phy_sel_val = 0x12;
			break;
		}
		case PHY_2LANE: {
			phy_sel_val = 0x24;
			break;
		}
		case PHY_2P2_S: {
			phy_sel_val = 0x20;
			break;
		}
		case PHY_2P2_M: {
			phy_sel_val = 0x21;
			break;
		}
		default:
			pr_err("fail to get valid csi phy id\n");
			return;
		}
		phy_sel_val  <<= dt_info->controller_id * 6;
	} else {
		/* cfg the value of  mipi_csi_dphy_c2_sel */
		phy_sel_mask = 7 << 12;

		switch (phy->phy_id) {
		case PHY_2LANE: {
			phy_sel_val = 4;
			break;
		}
		case PHY_2P2_S: {
			phy_sel_val = 0;
			break;
		}
		case PHY_2P2_M: {
			phy_sel_val = 1;
			break;
		}
		default:
			pr_err("fail to get valid csi phy id\n");
			return;
		}
		phy_sel_val  <<= 12;
	}
	regmap_update_bits(phy->cam_ahb_syscon,
			REG_MM_AHB_MIPI_CSI_SEL_CTRL,
			phy_sel_mask,
			phy_sel_val);
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

	pr_info("%s csi, id %d dphy %d\n", __func__, dt_info->controller_id,
		phy->phy_id);

	switch (dt_info->controller_id) {
	case CSI_RX0: {
		mask_eb = MASK_MM_AHB_CSI0_EB;
		mask_rst = MASK_MM_AHB_MIPI_CSI0_SOFT_RST;
		break;
	}
	case CSI_RX1: {
		mask_eb = MASK_MM_AHB_CSI1_EB;
		mask_rst = MASK_MM_AHB_MIPI_CSI1_SOFT_RST;
		break;
	}
	case CSI_RX2: {
		mask_eb = MASK_MM_AHB_CSI2_EB;
		mask_rst = MASK_MM_AHB_MIPI_CSI2_SOFT_RST;
		break;
	}
	default:
		pr_err("fail to get valid csi id\n");
		return;
	}

	spin_lock_irqsave(&csi_dump_lock[dt_info->controller_id], flag);

	csi_dump_regbase[dt_info->controller_id] = 0;

	regmap_update_bits(phy->cam_ahb_syscon, REG_MM_AHB_AHB_EB,
			mask_eb, mask_eb);
	regmap_update_bits(phy->cam_ahb_syscon, REG_MM_AHB_AHB_RST,
			mask_rst, ~mask_rst);
	udelay(1);
	regmap_update_bits(phy->cam_ahb_syscon, REG_MM_AHB_AHB_RST,
			mask_rst, ~mask_rst);

	spin_unlock_irqrestore(&csi_dump_lock[dt_info->controller_id], flag);
}

void dphy_init(struct csi_dt_node_info *dt_info, int32_t idx)
{
	if (!dt_info) {
		pr_err("fail to get valid phy ptr\n");
		return;
	}


	if (dt_info->phy.phy_id == PHY_4LANE ||
		dt_info->phy.phy_id == PHY_2LANE)
		phy_cfg_clr(idx);

	csi_dphy_lane_cfg(dt_info->phy.phy_id, idx,
		dt_info->lane_switch_eb, dt_info->lane_seq);
}

void cphy_osc_result_cal(int32_t idx)
{
	uint8_t temp1 = 0;
	uint8_t temp2 = 0;
	uint8_t n_count_tmp0 = 0;
	uint8_t n_count_tmp1 = 0;
	uint16_t n_count_result0 = 0;
	uint16_t n_count_result1 = 0;
	uint16_t n_count_result2 = 0;
	uint8_t n_win_set0 = 0;
	uint8_t n_win_set1 = 0;
	uint8_t n_win_set2 = 0;
	uint32_t n_cal_resultl0 = 0;
	uint32_t n_cal_resultl1 = 0;
	uint32_t n_cal_resultl2 = 0;
	uint32_t cphy_data_rate = 140;

	phy_read(idx, 0x9c, &n_count_tmp0);
	phy_read(idx, 0x9d, &n_count_tmp1);
	n_count_result0 = ((uint16_t)n_count_tmp0) |
		(((uint16_t)n_count_tmp1) << 8);

	phy_read(idx, 0xbc, &n_count_tmp0);
	phy_read(idx, 0xbd, &n_count_tmp1);
	n_count_result1 = ((uint16_t)n_count_tmp0) |
		(((uint16_t)n_count_tmp1) << 8);

	phy_read(idx, 0xdc, &n_count_tmp0);
	phy_read(idx, 0xdd, &n_count_tmp1);
	n_count_result2 = ((uint16_t)n_count_tmp0) |
		(((uint16_t)n_count_tmp1) << 8);

	phy_read(idx, 0x98, &n_win_set0);
	n_win_set0 = ((n_win_set0 & 0x6) >> 1) + 1;

	phy_read(idx, 0xb8, &n_win_set1);
	n_win_set1 = ((n_win_set1 & 0x6) >> 1) + 1;

	phy_read(idx, 0xd8, &n_win_set2);
	n_win_set2 = ((n_win_set2 & 0x6) >> 1) + 1;

	n_cal_resultl0 =
		(26 * 67 * 2 * n_count_result0) /
		(cphy_data_rate * 1024 * n_win_set0);
	n_cal_resultl1 =
		(26 * 67 * 2 * n_count_result1) /
		(cphy_data_rate * 1024 * n_win_set1);
	n_cal_resultl2 =
		(26 * 67 * 2 * n_count_result2) /
		(cphy_data_rate * 1024 * n_win_set2);

	if ((n_cal_resultl0/48) < 15) {
		temp1 = (uint8_t)(n_cal_resultl0 / 48);
		phy_write(idx, 0x94, temp1 << 4, BIT_7 | BIT_6 | BIT_5 | BIT_4);
		if ((n_cal_resultl0/48) != 0) {
			temp1 = (0x20 & (n_cal_resultl0 % 48 + 1)) >> 5;
			temp2 = ((0x1f & (n_cal_resultl0 % 48 + 1)) << 3) | 0x4;
			phy_write(idx, 0x98, temp1 << 7, BIT_7);
			phy_write(idx, 0x99, temp2, 0xff);
		} else {
			temp1 = (0x20 & n_cal_resultl0) >> 5;
			temp2 = ((0x1f & n_cal_resultl0) << 3) | 0x4;
			phy_write(idx, 0x98, temp1 << 7, BIT_7);
			phy_write(idx, 0x99, temp2, 0xff);
		}
	} else {
		phy_write(idx, 0x94, 15 << 4, BIT_7 | BIT_6 | BIT_5 | BIT_4);
		temp1 = (0x20 & (n_cal_resultl0-719)) >> 5;
		temp2 = ((0x1f & (n_cal_resultl0-719)) << 3) | 0x4;
		phy_write(idx, 0x98, temp1 << 7, BIT_7);
		phy_write(idx, 0x99, temp2, 0xff);
	}

	if ((n_cal_resultl1 / 48) < 15) {
		temp1 = (uint8_t)(n_cal_resultl1 / 48);
		phy_write(idx, 0xb4, temp1 << 4, BIT_7 | BIT_6 | BIT_5 | BIT_4);
		if ((n_cal_resultl1 / 48) != 0) {
			temp1 = (0x20 & (n_cal_resultl1 % 48 + 1)) >> 5;
			temp2 = ((0x1f & (n_cal_resultl1 % 48 + 1)) << 3) | 0x4;
			phy_write(idx, 0xb8, temp1 << 7, BIT_7);
			phy_write(idx, 0xb9, temp2, 0xff);
		} else {
			temp1 = (0x20 & n_cal_resultl1) >> 5;
			temp2 = ((0x1f & n_cal_resultl1) << 3) | 0x4;
			phy_write(idx, 0xb8, temp1 << 7, BIT_7);
			phy_write(idx, 0xb9, temp2, 0xff);
		}
	} else {
		phy_write(idx, 0xb4, 15 << 4, BIT_7 | BIT_6 | BIT_5 | BIT_4);
		temp1 = (0x20 & (n_cal_resultl1-719)) >> 5;
		temp2 = ((0x1f & (n_cal_resultl1-719)) << 3) | 0x4;
		phy_write(idx, 0xb8, temp1 << 7, BIT_7);
		phy_write(idx, 0xb9, temp2, 0xff);
	}

	if ((n_cal_resultl2/48) < 15) {
		temp1 = (uint8_t)(n_cal_resultl2 / 48);
		phy_write(idx, 0xd4, temp1 << 4, BIT_7 | BIT_6 | BIT_5 | BIT_4);
		if ((n_cal_resultl2 / 48) != 0) {
			temp1 = (0x20 & (n_cal_resultl2 % 48 + 1)) >> 5;
			temp2 = ((0x1f & (n_cal_resultl2 % 48 + 1)) << 3) | 0x4;
			phy_write(idx, 0xd8, temp1 << 7, BIT_7);
			phy_write(idx, 0xd9, temp2, 0xff);
		} else {
			temp1 = (0x20 & n_cal_resultl2) >> 5;
			temp2 = ((0x1f & n_cal_resultl2) << 3) | 0x4;
			phy_write(idx, 0xd8, temp1 << 7, BIT_7);
			phy_write(idx, 0xd9, temp2, 0xff);
		}
	} else {
		phy_write(idx, 0xd4, 15 << 4, BIT_7 | BIT_6 | BIT_5 | BIT_4);
		temp1 = (0x20 & (n_cal_resultl2-719)) >> 5;
		temp2 = ((0x1f & (n_cal_resultl2-719)) << 3) | 0x4;
		phy_write(idx, 0xd8, temp1 << 7, BIT_7);
		phy_write(idx, 0xd9, temp2, 0xff);
	}
}
void cphy_init(struct csi_dt_node_info *dt_info, int32_t idx)
{
	uint8_t readback = 0;

	CSI_REG_MWR(idx, PHY_SEL, BIT_0, 1);
	udelay(1);
	CSI_REG_MWR(idx, PHY_PD_N, BIT_0, 0);
	udelay(1);
	CSI_REG_MWR(idx, PHY_PD_N, BIT_0, 1);
	udelay(1);
	CSI_REG_MWR(idx, RST_DPHY_N, BIT_0, 0);
	udelay(1);
	CSI_REG_MWR(idx, RST_DPHY_N, BIT_0, 1);
	udelay(1);
	CSI_REG_MWR(idx, RST_CSI2_N, BIT_0, 0);
	udelay(1);
	CSI_REG_MWR(idx, RST_CSI2_N, BIT_0, 1);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLR, 1);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLR, 0);
	udelay(1);

	phy_cfg_start(idx);

	phy_write(idx, 0x62, BIT_0, BIT_0);
	phy_write(idx, 0x60, 0xf1, BIT_7 | BIT_6 | BIT_5 | BIT_4 | BIT_0);
	phy_write(idx, 0x68, BIT_0, BIT_0);
	phy_read(idx, 0x92, &readback);
	phy_read(idx, 0xb2, &readback);
	phy_read(idx, 0xd2, &readback);
	phy_read(idx, 0x60, &readback);
	udelay(10);
	phy_write(idx, 0x9e, BIT_2, BIT_2);
	phy_write(idx, 0xbe, BIT_2, BIT_2);
	phy_write(idx, 0xde, BIT_2, BIT_2);
	udelay(5);
	phy_write(idx, 0x98, BIT_0, BIT_0);
	phy_write(idx, 0xb8, BIT_0, BIT_0);
	phy_write(idx, 0xd8, BIT_0, BIT_0);
	readback = 0;
	while (readback != 1) {
		phy_read(idx, 0x9a, &readback);
		readback = (readback & BIT_1) >> 1;
	}
	readback = 0;
	while (readback != 1) {
		phy_read(idx, 0xba, &readback);
		readback = (readback & BIT_1) >> 1;
	}
	readback = 0;
	while (readback != 1) {
		phy_read(idx, 0xda, &readback);
		readback = (readback & BIT_1) >> 1;
	}
	//calculate osc counter result
	cphy_osc_result_cal(idx);
	udelay(10);
	phy_write(idx, 0x92, 0, BIT_1);
	phy_write(idx, 0xb2, 0, BIT_1);
	phy_write(idx, 0xd2, 0, BIT_1);

	phy_write(idx, 0x69, 1 << 3, BIT_3 | BIT_4);
	phy_write(idx, 0x6b, 1 << 5, BIT_5 | BIT_6);
	phy_read(idx, 0x02, &readback);
	phy_write(idx, 0x02, 0x3D,
		BIT_0 | BIT_1 | BIT_2 | BIT_3 | BIT_4 | BIT_5);

	phy_cfg_done(idx);
}

void csi_set_on_lanes(uint8_t lanes, int32_t idx)
{
	CSI_REG_MWR(idx, LANE_NUMBER, 0x7, (lanes - 1));
}

void csi_reset_shut_down(uint8_t shutdown, int32_t idx)
{
	/* DPHY reset output, active low */
	CSI_REG_MWR(idx, RST_DPHY_N, BIT_0, shutdown ? 0 : 1);
	/* CSI-2 controller reset output, active low */
	CSI_REG_MWR(idx, RST_CSI2_N, BIT_0, shutdown ? 0 : 1);
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
