/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/mfd/syscon/sprd-glb.h>
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

#define CSI_MASK0			0x1FFFFFF
#define CSI_MASK1			0xFFFFFF

#define PHY_TESTCLR			BIT_0
#define PHY_TESTCLK			BIT_1
#define PHY_TESTDIN			0xFF
#define PHY_TESTDOUT			0xFF00
#define PHY_TESTEN			BIT_16

#define IPG_ENABLE_MASK			BIT_0
#define IPG_IMAGE_MODE_MASK		BIT_1
#define IPG_COLOR_BAR_ROTATION_MASK	BIT_2
#define IPG_HSYNC_EN_MASK		BIT_3
#define IPG_IMAGE_W_MASK		0x1FF0
#define IPG_COLOR_BAR_W_MASK		0x1FE000
#define IPG_IMAGE_H_MASK		0x3FE00000

#define IPG_IMAGE_W			4224
#define IPG_IMAGE_H			3136

#define IPG_ENABLE			1
#define IPG_IMAGE_MODE			2
#define IPG_COLOR_BAR_ROTATION		0xB
#define IPG_HSYNC_EN			7
#define IPG_IMAGE_W_REG			(((IPG_IMAGE_W)/16) << 4)
#define IPG_COLOR_BAR_W			(((IPG_IMAGE_W)/24) << 13)
#define IPG_IMAGE_H_REG			(((IPG_IMAGE_H)/8) << 21)

#define IPG_BAYER_PATTERN_MASK		0x3
#define IPG_BAYER_PATTERN_BGGR		0
#define IPG_BAYER_PATTERN_RGGB		1
#define IPG_BAYER_PATTERN_GBRG		2
#define IPG_BAYER_PATTERN_GRBG		3

#define IPG_BAYER_R_MASK		0x3FF
#define IPG_BAYER_G_MASK		0xFFC00
#define IPG_BAYER_B_MASK		0x3FF00000

#define IPG_RAW10_CFG0_R		0
#define IPG_RAW10_CFG0_G		0x3FC00
#define IPG_RAW10_CFG0_B		0x3FF00000

#define IPG_RAW10_CFG1_R		0xF
#define IPG_RAW10_CFG1_G		0xFFC00
#define IPG_RAW10_CFG1_B		0x3FF00000

#define IPG_RAW10_CFG2_R		0xFF
#define IPG_RAW10_CFG2_G		0x3FC00
#define IPG_RAW10_CFG2_B		0x3FF00000

#define IPG_H_BLANK_MASK		0x1FFF
#define IPG_V_BLANK_MASK		0x1FFE000
#define IPG_H_BLANK			0x1FFF
#define IPG_V_BLANK			0x1FFE000

unsigned long s_csi_regbase[SPRD_SENSOR_ID_MAX];
unsigned long csi_dump_regbase[CSI_MAX_COUNT];

void csi_ipg_mode_cfg(uint32_t idx)
{
	CSI_REG_MWR(idx, MODE_CFG, IPG_IMAGE_H_MASK, IPG_IMAGE_H_REG);
	CSI_REG_MWR(idx, MODE_CFG, IPG_COLOR_BAR_W_MASK, IPG_COLOR_BAR_W);
	CSI_REG_MWR(idx, MODE_CFG, IPG_IMAGE_W_MASK, IPG_IMAGE_W_REG);
	CSI_REG_MWR(idx, MODE_CFG, IPG_HSYNC_EN_MASK, IPG_HSYNC_EN);
	CSI_REG_MWR(idx, MODE_CFG, IPG_COLOR_BAR_ROTATION_MASK,
					IPG_COLOR_BAR_ROTATION);
	CSI_REG_MWR(idx, MODE_CFG, IPG_IMAGE_MODE_MASK, IPG_IMAGE_MODE);

	CSI_REG_MWR(idx, IPG_RAW10_CFG0, IPG_BAYER_R_MASK, IPG_RAW10_CFG0_R);
	CSI_REG_MWR(idx, IPG_RAW10_CFG0, IPG_BAYER_G_MASK, IPG_RAW10_CFG0_G);
	CSI_REG_MWR(idx, IPG_RAW10_CFG0, IPG_BAYER_B_MASK, IPG_RAW10_CFG0_B);
	CSI_REG_MWR(idx, IPG_RAW10_CFG1, IPG_BAYER_R_MASK, IPG_RAW10_CFG1_R);
	CSI_REG_MWR(idx, IPG_RAW10_CFG1, IPG_BAYER_G_MASK, IPG_RAW10_CFG1_G);
	CSI_REG_MWR(idx, IPG_RAW10_CFG1, IPG_BAYER_B_MASK, IPG_RAW10_CFG1_B);
	CSI_REG_MWR(idx, IPG_RAW10_CFG2, IPG_BAYER_R_MASK, IPG_RAW10_CFG2_R);
	CSI_REG_MWR(idx, IPG_RAW10_CFG2, IPG_BAYER_G_MASK, IPG_RAW10_CFG2_G);
	CSI_REG_MWR(idx, IPG_RAW10_CFG2, IPG_BAYER_B_MASK, IPG_RAW10_CFG2_B);

	CSI_REG_MWR(idx, IPG_RAW10_CFG3, IPG_BAYER_PATTERN_MASK,
					IPG_BAYER_PATTERN_RGGB);

	CSI_REG_MWR(idx, IPG_OTHER_CFG0, IPG_H_BLANK_MASK, IPG_H_BLANK);
	CSI_REG_MWR(idx, IPG_OTHER_CFG0, IPG_V_BLANK_MASK, IPG_H_BLANK);

	CSI_REG_MWR(idx, MODE_CFG, IPG_ENABLE_MASK, IPG_ENABLE);
}

void csi_reg_trace(unsigned int idx)
{
	unsigned long addr = 0;

	if (csi_dump_regbase[idx] == 0) {
		pr_info("CSI %d not used no need to dump\n", idx);
		return;
	}

	pr_info("CSI %d reg list\n", idx);
	for (addr = IP_REVISION; addr <= IPG_OTHER_CFG0; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			REG_RD(csi_dump_regbase[idx] + addr),
			REG_RD(csi_dump_regbase[idx] + addr + 4),
			REG_RD(csi_dump_regbase[idx] + addr + 8),
			REG_RD(csi_dump_regbase[idx] + addr + 12));
	}
}

/* phy testclear used to reset phy to right default state */
static void dphy_cfg_clr(int32_t idx)
{
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLR, 1);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLR, 0);
	udelay(1);
}

#ifdef DPHY_TEST_CFG_EN
static void dphy_cfg_start(int32_t idx)
{
	CSI_REG_MWR(idx, PHY_TEST_CRTL1, PHY_TESTEN, 0 << 16);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLK, 1 << 1);
	udelay(1);
}

static void dphy_cfg_done(int32_t idx)
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

static void dphy_cfg_write(int32_t idx, unsigned int code_in,
	unsigned int data_in, unsigned int *code_out, unsigned int *data_out)
{
	CSI_REG_MWR(idx, PHY_TEST_CRTL1, PHY_TESTEN, 0 << 16);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLK, 1 << 1);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL1, PHY_TESTDIN, code_in);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL1, PHY_TESTEN, 1 << 16);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLK, 0 << 1);
	udelay(1);
	*code_out = (CSI_REG_RD(idx, PHY_TEST_CRTL1) & PHY_TESTDOUT) >> 8;
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL1, PHY_TESTEN, 0 << 16);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL1, PHY_TESTDIN, data_in);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLK, 1 << 1);
	udelay(1);
	*data_out = (CSI_REG_RD(idx, PHY_TEST_CRTL1) & PHY_TESTDOUT) >> 8;
}

/* used to write testcode or testdata to phy */
static void dphy_write(int32_t idx, unsigned int test_code,
	unsigned int test_data, unsigned int *test_code_out,
	unsigned int *test_data_out)
{
	dphy_cfg_clr(idx);
	dphy_cfg_start(idx);
	dphy_cfg_write(idx, test_code, test_data,
		test_code_out, test_data_out);
	dphy_cfg_done(idx);
}
#endif

static void dphy_testclr_db_init(void)
{
	unsigned int val = 0;
	void __iomem *ana_apb = NULL;

	ana_apb = ioremap_nocache(REG_CPHY_TEST_CTRL, 0x4);
	if (ana_apb == NULL) {
		pr_err("fail to ioremap\n");
		return;
	}

	val = REG_RD(ana_apb);

	val |= BIT_CPHY_DBG_TEST_CLR;
	REG_WR(ana_apb, val);

	udelay(1);

	val = REG_RD(ana_apb);

	val &= ~(BIT_CPHY_DBG_TEST_CLR);
	REG_WR(ana_apb, val);

	udelay(1);

	iounmap(ana_apb);
}

static void dphy_init_common(int32_t idx)
{
	CSI_REG_MWR(idx, PHY_PD_N, BIT_0, 0);
	CSI_REG_MWR(idx, RST_DPHY_N, BIT_0, 0);
	dphy_cfg_clr(idx);
}

static void csi_dphy_2p2_testclr(struct csi_phy_info *phy)
{
	unsigned int s_test_clr = 0;
	unsigned int s_test_clr_sel = 0;
	unsigned int s_reg = 0;
	unsigned int m_test_clr = 0;
	unsigned int m_test_clr_sel = 0;
	unsigned int m_reg = 0;

	if (phy == NULL) {
		pr_err("%s: param is error\n", __func__);
		return;
	}

	s_reg = REG_AON_APB_CSI_2P2L_S_PHY_CTRL;
	s_test_clr = BIT_AON_APB_CSI_2P2L_TESTCLR_S;
	s_test_clr_sel = BIT_AON_APB_CSI_2P2L_TESTCLR_S_SEL;
	regmap_update_bits(phy->ana_apb_syscon,
			s_reg,
			s_test_clr | s_test_clr_sel,
			s_test_clr | s_test_clr_sel);

	m_reg = REG_AON_APB_CSI_2P2L_M_PHY_CTRL;
	m_test_clr = BIT_AON_APB_CSI_2P2L_TESTCLR_M;
	m_test_clr_sel = BIT_AON_APB_CSI_2P2L_TESTCLR_M_SEL;
	regmap_update_bits(phy->ana_apb_syscon,
			m_reg,
			m_test_clr | m_test_clr_sel,
			m_test_clr | m_test_clr_sel);

	udelay(2);

	regmap_update_bits(phy->ana_apb_syscon,
			s_reg,
			s_test_clr | s_test_clr_sel,
			~(s_test_clr | s_test_clr_sel));

	regmap_update_bits(phy->ana_apb_syscon,
			m_reg,
			m_test_clr | m_test_clr_sel,
			~(m_test_clr | m_test_clr_sel));

	udelay(1);
}

void csi_phy_power_down(struct csi_phy_info *phy, unsigned int csi_id,
			int is_eb)
{
	uint32_t ps_pd_l = 0;
	uint32_t ps_pd_s = 0;
	uint32_t shutdownz = 0;
	uint32_t reg = 0;
	uint32_t dphy_eb = 0;

	if (!phy) {
		pr_err("Input phy ptr is NULL\n");
		return;
	}

	switch (phy->phy_id) {
	case 0:
		/* phy0: use all lanes of 2p2lane as a 4lane phy  */
		ps_pd_l = BIT_AON_APB_MIPI_PHY0_PS_PD_L;
		ps_pd_s = BIT_AON_APB_MIPI_PHY0_PS_PD_S;
		shutdownz = BIT_AON_APB_FORCE_CSI0_PHY_SHUTDOWNZ;
		reg = REG_AON_APB_PWR_CTRL;

		regmap_update_bits(phy->ana_apb_syscon,
				REG_AON_APB_2P2L_PHY_CTRL,
				BIT_AON_APB_4LANE_PHY_MODE,
				BIT_AON_APB_4LANE_PHY_MODE);
		break;
	case 1:
		/* phy1: 2lane phy */
		ps_pd_l = BIT_AON_APB_MIPI_PHY1_PS_PD_L;
		ps_pd_s = BIT_AON_APB_MIPI_PHY1_PS_PD_S;
		shutdownz = BIT_AON_APB_FORCE_CSI0_PHY_SHUTDOWNZ;
		reg = REG_AON_APB_PWR_CTRL;
		break;
	case 2:
		/* phy2: use lane2 and lane3 of 2p2lane phy as a 2lane phy */
		ps_pd_l = BIT_AON_APB_MIPI_PHY0_PS_PD_L;
		ps_pd_s = BIT_AON_APB_MIPI_PHY0_PS_PD_S;
		shutdownz = BIT_AON_APB_FORCE_CSI1_PHY_SHUTDOWNZ;
		reg = REG_AON_APB_PWR_CTRL;

		regmap_update_bits(phy->ana_apb_syscon,
				REG_AON_APB_2P2L_PHY_CTRL,
				BIT_AON_APB_4LANE_PHY_MODE,
				~BIT_AON_APB_4LANE_PHY_MODE);
		break;
	default:
		pr_err("csi invalid phy id %d\n", csi_id);
		return;
	}

	dphy_eb = BIT_AON_APB_SERDES_DPHY_EB;
	if (is_eb) {
		regmap_update_bits(phy->ana_apb_syscon,
				reg,
				ps_pd_l | ps_pd_s,
				ps_pd_l | ps_pd_s);
		regmap_update_bits(phy->ana_apb_syscon,
				reg,
				shutdownz,
				~shutdownz);

		if (phy->phy_id == 0 || phy->phy_id == 2) {
			regmap_update_bits(phy->ana_apb_syscon,
					REG_AON_APB_EB1,
					dphy_eb, ~dphy_eb);
		}
	} else {
		regmap_update_bits(phy->ana_apb_syscon,
				reg,
				ps_pd_s,
				~ps_pd_s);
		udelay(200);

		regmap_update_bits(phy->ana_apb_syscon,
				reg,
				ps_pd_l,
				~ps_pd_l);

		regmap_update_bits(phy->ana_apb_syscon,
				reg,
				shutdownz,
				shutdownz);

		if (phy->phy_id == 0 || phy->phy_id == 2) {
			regmap_update_bits(phy->ana_apb_syscon,
					REG_AON_APB_EB1,
					dphy_eb, dphy_eb);
			csi_dphy_2p2_testclr(phy);
		}
	}
}

int csi_ahb_reset(struct csi_phy_info *phy, unsigned int csi_id)
{
	unsigned int flag = 0;

	if (!phy) {
		pr_err("Input phy ptr is NULL\n");
		return -EINVAL;
	}
	pr_info("%s csi, id %d dphy %d\n", __func__, csi_id, phy->phy_id);

	csi_dump_regbase[0] = 0;
	csi_dump_regbase[1] = 0;

	switch (csi_id) {
	case 0: {
		flag = BIT_MM_AHB_CSI0_SOFT_RST |
			BIT_MM_AHB_CSI_IDI_SW_SOFT_RST;
		break;
	}
	case 1: {
		flag = BIT_MM_AHB_CSI1_SOFT_RST |
			BIT_MM_AHB_CSI_IDI_SW_SOFT_RST;
		break;
	}
	default:
		flag = BIT_MM_AHB_CSI0_SOFT_RST |
			BIT_MM_AHB_CSI_IDI_SW_SOFT_RST;
		pr_err("csi id is invalid\n");
		break;
	}

	regmap_update_bits(phy->cam_ahb_syscon,
			   REG_MM_AHB_RST, flag, flag);
	udelay(1);
	regmap_update_bits(phy->cam_ahb_syscon,
			   REG_MM_AHB_RST, flag, ~flag);

	return 0;
}

void csi_controller_enable(struct csi_dt_node_info *dt_info, int32_t idx)
{
	unsigned int mask = 0;
	struct csi_phy_info *phy = NULL;

	if (!dt_info) {
		pr_err("Input dt_info ptr is NULL\n");
		return;
	}

	s_csi_regbase[idx] = dt_info->reg_base;
	phy = &dt_info->phy;

	if (!phy) {
		pr_err("Input phy ptr is NULL\n");
		return;
	}

	pr_info("%s csi, id %d dphy %d\n", __func__, dt_info->controller_id,
		phy->phy_id);

	switch (dt_info->controller_id) {
	case 0: {
		csi_dump_regbase[0] = dt_info->reg_base;
		mask = BIT_MM_AHB_CSI0_EB;
		regmap_update_bits(phy->cam_ahb_syscon, REG_MM_AHB_EB,
				mask, mask);

		mask = BIT_MM_AHB_FORCE_SWITCH;
		regmap_hwlock_update_bits(phy->cam_ahb_syscon,
				REG_MM_AHB_CSI_IDI_SW_CFG,
				mask, ~mask);
		break;
	}
	case 1: {
		csi_dump_regbase[1] = dt_info->reg_base;
		mask = BIT_MM_AHB_CSI1_EB;
		regmap_update_bits(phy->cam_ahb_syscon, REG_MM_AHB_EB,
				mask, mask);

		mask = BIT_MM_AHB_FORCE_SWITCH;
		regmap_hwlock_update_bits(phy->cam_ahb_syscon,
				REG_MM_AHB_CSI_IDI_SW_CFG,
				mask, mask);
		break;
	}
	default:
		pr_err("csi id is invalid\n");
		break;
	}
}

void dphy_init(struct csi_phy_info *phy, int32_t idx)
{
	unsigned int mask = 0;

	if (!phy) {
		pr_err("Input phy ptr is NULL\n");
		return;
	}

	if (phy->phy_id == 0 || phy->phy_id == 2) {
		mask = BIT_MM_AHB_MIPI_CPHY_SEL;
		regmap_hwlock_update_bits(phy->cam_ahb_syscon,
				REG_MM_AHB_MIPI_CSI2_CTRL,
				mask, ~mask);
	} else if (phy->phy_id == 1) {
		mask = BIT_MM_AHB_MIPI_CPHY_SEL;
		regmap_hwlock_update_bits(phy->cam_ahb_syscon,
				REG_MM_AHB_MIPI_CSI2_CTRL,
				mask, mask);
	}

	dphy_init_common(idx);
	if (phy->phy_id == 0 || phy->phy_id == 2)
		dphy_testclr_db_init();
}

void csi_idi_switch(struct csi_phy_info *phy, unsigned int csi_id)
{
	unsigned int mask;

	pr_info("%s csi, id %d dphy %d\n", __func__, csi_id, phy->phy_id);

	switch (csi_id) {
	case 0:
		mask = BIT_MM_AHB_FORCE_SWITCH;
		regmap_hwlock_update_bits(phy->cam_ahb_syscon,
					  REG_MM_AHB_CSI_IDI_SW_CFG,
					  mask, ~mask);
		break;
	case 1:
		mask = BIT_MM_AHB_FORCE_SWITCH;
		regmap_hwlock_update_bits(phy->cam_ahb_syscon,
					  REG_MM_AHB_CSI_IDI_SW_CFG,
					  mask, mask);
		break;
	default:
		pr_err("csi id is invalid\n");
		break;
	}
}

void csi_set_on_lanes(uint8_t lanes, int32_t idx)
{
	CSI_REG_MWR(idx, LANE_NUMBER, 0x7, (lanes - 1));
}

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
}

void csi_close(int32_t idx)
{
	csi_shut_down_phy(1, idx);
	csi_reset_controller(idx);
	csi_reset_phy(idx);
}
