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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>
#include <linux/semaphore.h>
#include <sprd_mm.h>

#include "csi_api.h"
#include "csi_driver.h"
#include "sprd_sensor_core.h"
#include "dphy_drv.h"

#include "mm_ahb.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "dphy_drv: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__


#define PHY_TESTCLR			BIT_0
#define PHY_TESTCLK			BIT_1
#define PHY_TESTDIN			0xFF
#define PHY_TESTDOUT		0xFF00
#define PHY_TESTEN			BIT_16

#define RX0_MSK  0x3F
#define RX1_MSK  0xFC0
#define RX2_MSK  0x7000

static char *phy_name[] = {
	"mipi-csi-phy0", /* 4Lane */
	"mipi-csi-phy1", /* 2p2l */
	"mipi-csi-phy2", /* 2lane */
	"mipi-csi-phy1-m", /*2p2l_m*/
	"mipi-csi-phy1-s", /*2p2l_s*/
	"mipi-csi-phy3", /*4lane_1*/
};

/* Temp varibale to know which project, 0 : SharkL5, 1 : ROC1 */
uint32_t g_project_id = 0;

static struct dphy_info *g_phy_info[5];

static unsigned int csi_addr[] = {
	0x62300000,
	0x62400000,
	0x62500000};
static int init_flag = 0;

static struct dphy_info *get_phy_info(int phyid)
{
	return g_phy_info[phyid];
}

/* phy testclear used to reset phy to right default state */
static void dphy_cfg_clr(int32_t idx)
{
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLR, 1);
	udelay(1);
	CSI_REG_MWR(idx, PHY_TEST_CRTL0, PHY_TESTCLR, 0);
	udelay(1);
}

static void csi_dphy_2p2_testclr_init(struct dphy_info *phy)
{
	unsigned int mask = 0;
	pr_info("entry");
#if 0
	regmap_update_bits(phy->anlg_phy, phy->testclr_s_en,
		phy->testclr_s_en_msk, phy->testclr_s_en_msk);

	/* config dbg_if_sel_m,s,db */
	regmap_update_bits(phy->anlg_phy, phy->cfg_0,
		phy->cfg_0_msk, phy->cfg_0_msk);

	/* set dsi_if_sel_db */
	regmap_update_bits(phy->anlg_phy, phy->dsi_if_sel_db,
		phy->dsi_if_sel_db_msk, ~(phy->dsi_if_sel_db_msk));

	/* set csi_if_sel_m */
	regmap_update_bits(phy->anlg_phy, phy->csi_if_sel_m,
		phy->csi_if_sel_m_msk, ~(phy->csi_if_sel_m_msk));

	/* set csi_if_sel_s */
	regmap_update_bits(phy->anlg_phy, phy->csi_if_sel_s,
		phy->csi_if_sel_s_msk, ~(phy->csi_if_sel_s_msk));

	/* set testclr_m/s_sel */
	regmap_update_bits(phy->anlg_phy, phy->testclr_m_sel,
		phy->testclr_m_sel_msk, phy->testclr_m_sel_msk);
	regmap_update_bits(phy->anlg_phy, phy->testclr_s_sel,
		phy->testclr_s_sel_msk, phy->testclr_s_sel_msk);
#else
	mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_S_EN;
	regmap_update_bits(phy->anlg_phy,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST,
		mask, mask);

	mask = MASK_ANLG_PHY_G10_DBG_SEL_ANALOG_MIPI_CSI_2P2LANE_CSI_IF_SEL_S
		| MASK_ANLG_PHY_G10_DBG_SEL_ANALOG_MIPI_CSI_2P2LANE_CSI_IF_SEL_M
		| MASK_ANLG_PHY_G10_DBG_SEL_ANALOG_MIPI_CSI_2P2LANE_DSI_IF_SEL_DB;
	regmap_update_bits(phy->anlg_phy,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_REG_SEL_CFG_0,
		mask, mask);

	mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_DSI_IF_SEL_DB;
	regmap_update_bits(phy->anlg_phy,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_DB,
		mask, ~mask);

	mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_IF_SEL_S;
	regmap_update_bits(phy->anlg_phy,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_S,
		mask, ~mask);

	mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_IF_SEL_M;
	regmap_update_bits(phy->anlg_phy,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_M,
		mask, ~mask);

	mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_M_SEL
		| MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_S_SEL;
	regmap_update_bits(phy->anlg_phy,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST,
		mask, mask);
#endif
}

static void csi_dphy_2p2_testclr_set(struct dphy_info *phy)
{
	unsigned int mask = 0;
	pr_info("entry");
#if 0
	/* set testclr_db/m/s */
	regmap_update_bits(phy->anlg_phy, phy->dsi_testclr_db,
		phy->dsi_testclr_db_msk, phy->dsi_testclr_db_msk);
	udelay(1);
	regmap_update_bits(phy->anlg_phy, phy->testclr_m,
		phy->testclr_m_msk, phy->testclr_m_msk);
	regmap_update_bits(phy->anlg_phy, phy->testclr_s,
		phy->testclr_s_msk, phy->testclr_s_msk);
#else
	mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_DSI_TESTCLR_DB;
	regmap_update_bits(phy->anlg_phy,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB,
		mask, mask);
	udelay(1);
	mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_M
		| MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_S;
	regmap_update_bits(phy->anlg_phy,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST,
		mask, mask);
#endif
}

static void csi_dphy_2p2_testclr_clear(struct dphy_info *phy)
{
	unsigned int mask = 0;
	pr_info("entry");

#if 0
	/* clear testclr_db/m/s */
	regmap_update_bits(phy->anlg_phy, phy->dsi_testclr_db,
			phy->dsi_testclr_db_msk, ~(phy->dsi_testclr_db_msk));
	regmap_update_bits(phy->anlg_phy, phy->testclr_m,
			phy->testclr_m_msk, ~(phy->testclr_m_msk));
		regmap_update_bits(phy->anlg_phy, phy->testclr_s,
			phy->testclr_s_msk, ~(phy->testclr_s_msk));
#else
	mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_DSI_TESTCLR_DB;
	regmap_update_bits(phy->anlg_phy,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB,
		mask, ~mask);
	mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_M
		| MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_S;
	regmap_update_bits(phy->anlg_phy,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST,
		mask, ~mask);
#endif
}

static void csi_dphy_2p2_reset(struct dphy_info *phy, int csiId)
{
	unsigned int cphy_sel_mask;
	unsigned int cphy_sel_val;
	unsigned int mask;
	pr_info("entry");

	if (!phy) {
		pr_err("fail to get valid phy ptr\n");
		return;
	}
#if 0
	switch (csiId) {
	case CSI_RX0:
	case CSI_RX1:
		regmap_update_bits(phy->anlg_phy, phy->testclr_m,
			phy->testclr_m_msk, phy->testclr_m_msk);
		regmap_update_bits(phy->anlg_phy, phy->testclr_m,
			phy->testclr_m_msk, ~(phy->testclr_m_msk));

		regmap_update_bits(phy->anlg_phy, phy->testclr_s,
			phy->testclr_s_msk, phy->testclr_s_msk);
		regmap_update_bits(phy->anlg_phy, phy->testclr_s,
			phy->testclr_s_msk, ~(phy->testclr_s_msk));
		break;
	case CSI_RX2:
		regmap_update_bits(phy->anlg_phy, phy->testclr_s,
			phy->testclr_s_msk, phy->testclr_s_msk);
		regmap_update_bits(phy->anlg_phy, phy->testclr_s,
			phy->testclr_s_msk, ~(phy->testclr_s_msk));

		regmap_update_bits(phy->anlg_phy, phy->testclr_m,
			phy->testclr_m_msk, phy->testclr_m_msk);
		regmap_update_bits(phy->anlg_phy, phy->testclr_m,
			phy->testclr_m_msk, ~(phy->testclr_m_msk));
		break;
	default:
		pr_err("fail to get valid csi_rx id\n");
	}
#else
	switch (csiId) {
	case CSI_RX0:
	case CSI_RX1:
		cphy_sel_mask = 7 << (csiId * 6);
		cphy_sel_val  = 3 << (csiId * 6);

		regmap_update_bits(phy->cam_ahb,
				REG_MM_AHB_MIPI_CSI_SEL_CTRL,
				cphy_sel_mask, cphy_sel_val);
		mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_M
		| MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_S;
		regmap_update_bits(phy->anlg_phy,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST,
		mask, mask);
		regmap_update_bits(phy->anlg_phy,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST,
		mask, ~mask);
		regmap_update_bits(phy->cam_ahb,
				REG_MM_AHB_MIPI_CSI_SEL_CTRL,
				cphy_sel_mask, 0x0);
		break;
	case CSI_RX2:
		cphy_sel_mask = 7 << (csiId * 6);
		cphy_sel_val  = 1 << (csiId * 6);

		regmap_update_bits(phy->cam_ahb,
				REG_MM_AHB_MIPI_CSI_SEL_CTRL,
				cphy_sel_mask, cphy_sel_val);

		mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_S;
		regmap_update_bits(phy->anlg_phy,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST,
		mask, mask);
		regmap_update_bits(phy->anlg_phy,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST,
		mask, ~mask);
		regmap_update_bits(phy->cam_ahb,
				REG_MM_AHB_MIPI_CSI_SEL_CTRL,
				cphy_sel_mask, 0x0);
		cphy_sel_val  = 2 << (csiId * 6);
		regmap_update_bits(phy->cam_ahb,
				REG_MM_AHB_MIPI_CSI_SEL_CTRL,
				cphy_sel_mask, cphy_sel_val);
		mask = MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_M;
		regmap_update_bits(phy->anlg_phy,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST,
		mask, mask);
		regmap_update_bits(phy->anlg_phy,
		REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST,
		mask, ~mask);
		regmap_update_bits(phy->cam_ahb,
				REG_MM_AHB_MIPI_CSI_SEL_CTRL,
				cphy_sel_mask, 0x0);
		break;
	default:
		pr_err("fail to get valid csi_rx id\n");
	}
#endif
}

int phy_parse_dt(int phyid, struct device *dev)
{
	u32 args[2] = {0};
	int ret = 0;
	int out_value;
	struct device_node *dn = NULL;
	struct device_node *par_dn = NULL;
	struct dphy_info *phy = NULL;

	if (phyid > PHY_MAX) {
		pr_err("invalid phyid: %d\n", phyid);
		goto err;
	}
	/* init flag for match register */
	init_flag = 0;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
		if (!phy)
			return -EINVAL;

	par_dn = of_find_compatible_node(NULL, NULL, "sprd,mipi-csi-phy");

	if (!par_dn) {
		pr_err("get phy node:%s failed\n", phy_name[phyid]);
		goto err;
	}
	dn = of_get_next_available_child(par_dn, NULL);
	while (dn) {
		of_property_read_u32(dn, "reg", &out_value);
		if (out_value == phyid) {
			break;
		}
		dn = of_get_next_available_child(par_dn, dn);

	}
	of_property_read_u32(dn, "sprd,project-id", &g_project_id);
	phy->phy_id = phyid;

	pr_info("phy_id :%d, reg:%d\n", phyid, out_value);

	phy->cam_ahb = syscon_regmap_lookup_by_phandle(dn, "sprd,cam-ahb-syscon");
	if (IS_ERR_OR_NULL(phy->cam_ahb)) {
		pr_err("get cam-ahb-syscon failed!\n");
		goto err;
	}

	/* parse anlg 10 */
	phy->anlg_phy = syscon_regmap_lookup_by_name(dn, "iso_sw_en");
	if (IS_ERR_OR_NULL(phy->anlg_phy)) {
		pr_err("get anlg phy failed\n");
		goto err;
	}

	ret = syscon_get_args_by_name(dn, "iso_sw_en", 2, args);
	if (ret == 2) {
		phy->iso_sw_en = args[0];
		phy->iso_sw_en_msk = args[1];
	} else {
		pr_err("get csi anlg iso_sw_en failed\n");
		goto err;
	}

	ret = syscon_get_args_by_name(dn, "ps_pd_s", 2, args);
	if (ret == 2) {
		phy->ps_pd_s = args[0];
		phy->ps_pd_s_msk = args[1];
	} else {
		pr_err("get csi anlg ps_pd_s failed\n");
		goto err;
	}

	ret = syscon_get_args_by_name(dn, "ps_pd_l", 2, args);
	if (ret == 2) {
		phy->ps_pd_l = args[0];
		phy->ps_pd_l_msk = args[1];
	} else {
		pr_err("get csi anlg ps_pd_l failed\n");
		goto err;
	}

	/* csi mode for 2p2l */
	if (phy->phy_id == PHY_2P2
		|| phy->phy_id == PHY_2P2_M
		|| phy->phy_id == PHY_2P2_S) {
		ret = syscon_get_args_by_name(dn, "csi_mode_sel", 2, args);
		if (ret == 2) {
			phy->csi_mode = args[0];
			phy->csi_mode_msk = args[1];
		} else {
			pr_err("get csi mode sel failed\n");
			goto err;
		}
#if 0
		ret = syscon_get_args_by_name(dn, "2p2l_cfg_0", 2, args);
		if (ret == 2) {
			phy->cfg_0 = args[0];
			phy->cfg_0_msk = args[1];
		} else {
			pr_err("get 2p2l_cfg_0 failed\n");
			goto err;
		}
		ret = syscon_get_args_by_name(dn, "dsi_if_sel_db", 2, args);
		if (ret == 2) {
			phy->dsi_if_sel_db = args[0];
			phy->dsi_if_sel_db_msk = args[1];
		} else {
			pr_err("get dsi_if_sel_db failed\n");
			goto err;
		}
		ret = syscon_get_args_by_name(dn, "dsi_testclr_db", 2, args);
		if (ret == 2) {
			phy->dsi_testclr_db = args[0];
			phy->dsi_testclr_db_msk = args[1];
		} else {
			pr_err("get dsi_testclr_db failed\n");
			goto err;
		}
		ret = syscon_get_args_by_name(dn, "csi_if_sel_s", 2, args);
		if (ret == 2) {
			phy->csi_if_sel_s = args[0];
			phy->csi_if_sel_s_msk = args[1];
		} else {
			pr_err("get csi_if_sel_s failed\n");
			goto err;
		}
		ret = syscon_get_args_by_name(dn, "csi_if_sel_m", 2, args);
		if (ret == 2) {
			phy->csi_if_sel_m = args[0];
			phy->csi_if_sel_m_msk = args[1];
		} else {
			pr_err("get csi_if_sel_m failed\n");
			goto err;
		}
		ret = syscon_get_args_by_name(dn, "testclr_m_sel", 2, args);
		if (ret == 2) {
			phy->testclr_m_sel = args[0];
			phy->testclr_m_sel_msk = args[1];
		} else {
			pr_err("testclr_m_sel failed\n");
			goto err;
		}
		ret = syscon_get_args_by_name(dn, "testclr_s_sel", 2, args);
		if (ret == 2) {
			phy->testclr_s_sel = args[0];
			phy->testclr_s_sel_msk = args[1];
		} else {
			pr_err("get testclr_s_sel failed\n");
			goto err;
		}
		ret = syscon_get_args_by_name(dn, "testclr_m", 2, args);
		if (ret == 2) {
			phy->testclr_m = args[0];
			phy->testclr_m_msk = args[1];
		} else {
			pr_err("get testclr_m failed\n");
			goto err;
		}
		ret = syscon_get_args_by_name(dn, "testclr_s", 2, args);
		if (ret == 2) {
			phy->testclr_s = args[0];
			phy->testclr_s_msk = args[1];
		} else {
			pr_err("get testclr_s failed\n");
			goto err;
		}
		ret = syscon_get_args_by_name(dn, "testclr_s_en", 2, args);
		if (ret == 2) {
			phy->testclr_s_en = args[0];
			phy->testclr_s_en_msk = args[1];
		} else {
			pr_err("get testclr_s_en failed\n");
			goto err;
		}
	#endif
	}

	g_phy_info[phyid] = phy;
	return 0;
err:
	pr_err("paser phy dts failed\n");
	return -1;
}

int reg_mwr(unsigned int reg, unsigned int msk, unsigned int value)
{
	void __iomem *reg_base = NULL;

	reg_base = ioremap_nocache(reg, 0x4);
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

	reg_base = ioremap_nocache(reg, 0x4);
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

	reg_base = ioremap_nocache(reg, 0x4);
	if (!reg_base) {
		pr_info("0x%x: ioremap failed\n", reg);
		return -1;
	}
	val = REG_RD(reg_base);
	iounmap(reg_base);
	return val;
}
void csi_phy_power_down(unsigned int phyid, int csiId, int sensor_id, int is_eb)
{
	unsigned int shutdownz = 0;
	unsigned int reg = 0;
	struct dphy_info *phy = get_phy_info(phyid);

	if (!phy) {
		pr_err("fail to get valid phy ptr\n");
		return;
	}

	pr_info("phyid: %d, csi_id:%d, is_eb:%d\n", phyid, csiId, is_eb);

	switch (csiId) {
	case CSI_RX0:
		shutdownz =
		MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_FORCE_CSI_PHY_SHUTDOWNZ;
		reg = REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST;
		break;
	case CSI_RX1:
		shutdownz =
		MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_FORCE_CSI_S_PHY_SHUTDOWNZ;
		reg = REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST;
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
	case PHY_4LANE:
	case PHY_2LANE:
		break;
	case PHY_2P2:
		/* 2p2lane phy as a 4lane phy
		 * 0:2L+2L, 1:4Lane
		 */
		regmap_update_bits(phy->anlg_phy, phy->csi_mode,
			phy->csi_mode_msk, phy->csi_mode_msk);
		break;
	case PHY_2P2_M:
	case PHY_2P2_S:
		/* 2p2lane phy as a 2lane phy
		 * 0:2L+2L, 1:4Lane
		 */
		regmap_update_bits(phy->anlg_phy, phy->csi_mode,
			phy->csi_mode_msk, ~(phy->csi_mode_msk));
		break;
	default:
		pr_err("fail to get valid phy id %d\n", phy->phy_id);
		return;
	}

	if (is_eb) {/* power down */
		regmap_update_bits(phy->anlg_phy, phy->iso_sw_en,
				phy->iso_sw_en_msk, phy->iso_sw_en_msk);
		udelay(100);
		regmap_update_bits(phy->anlg_phy, phy->ps_pd_l,
				phy->ps_pd_l_msk, phy->ps_pd_l_msk);
		udelay(100);
		regmap_update_bits(phy->anlg_phy, phy->ps_pd_s,
				phy->ps_pd_l_msk, phy->ps_pd_l_msk);
		udelay(100);
		/* set phy force shutdown */
		regmap_update_bits(phy->anlg_phy, reg,
				shutdownz, shutdownz);
	} else { /* power on */
		/* According to the time sequence of CSI-DPHY INIT,
		 * need pull down POWER, DPHY-reset and CSI-2 controller reset
		 */
		csi_shut_down_phy(1, sensor_id);
		csi_reset_shut_down(1, sensor_id);

		if (phy->phy_id == PHY_2P2
			|| phy->phy_id == PHY_2P2_M
			|| phy->phy_id == PHY_2P2_S) {
			csi_dphy_2p2_testclr_init(phy);
			csi_dphy_2p2_testclr_set(phy);
		} else if (phy->phy_id == PHY_4LANE
			|| phy->phy_id == PHY_2LANE){
				CSI_REG_MWR(sensor_id, PHY_TEST_CRTL0, PHY_TESTCLR, 1);
		}

		regmap_update_bits(phy->anlg_phy, phy->ps_pd_s,
				phy->ps_pd_s_msk, ~(phy->ps_pd_s_msk));
		udelay(100);
		regmap_update_bits(phy->anlg_phy, phy->ps_pd_l,
				phy->ps_pd_l_msk, ~(phy->ps_pd_l_msk));
		udelay(100);
		regmap_update_bits(phy->anlg_phy, phy->iso_sw_en,
				phy->iso_sw_en_msk, ~(phy->iso_sw_en_msk));

		/* set phy force shutdown */
		regmap_update_bits(phy->anlg_phy, reg,
				shutdownz, shutdownz);

		/* According to the time sequence of CSI-DPHY INIT,
		 * need pull up POWER, DPHY-reset and CSI-2 controller reset
		 */

		if (phy->phy_id == PHY_2P2
			|| phy->phy_id == PHY_2P2_M
			|| phy->phy_id == PHY_2P2_S) {
			csi_dphy_2p2_testclr_clear(phy);
			csi_dphy_2p2_reset(phy, sensor_id);
		} else if (phy->phy_id == PHY_4LANE
			|| phy->phy_id == PHY_2LANE){
				CSI_REG_MWR(sensor_id, PHY_TEST_CRTL0, PHY_TESTCLR, 0);
		}

		csi_shut_down_phy(0, sensor_id);
		csi_reset_shut_down(0, sensor_id);
	}
}

int dphy_csi_path_cfg(struct csi_dt_node_info *dt_info)
{
	/* uint32_t cphy_sel_mask; */
	uint32_t cphy_sel_val;

	if (!dt_info) {
		pr_err("input param is invalid\n");
		return -EINVAL;
	}

	switch (dt_info->phy_id) {
	case PHY_2P2: {
		cphy_sel_val = 0x1;
		break;
	}
	case PHY_4LANE: {
		cphy_sel_val = 0x12;
		break;
	}
	case PHY_2LANE: {
		cphy_sel_val = 0x24;
		break;
	}
	case PHY_2P2_S: {
		cphy_sel_val = 0x0;
		break;
	}
	case PHY_2P2_M: {
		cphy_sel_val = 0x1;
		break;
	}
	default:
		pr_err("fail to get valid csi phy id\n");
		return -1;
	}

	cphy_sel_val = cphy_sel_val << (dt_info->controller_id*6);
	pr_info("phyid:%d, csi_id:%d, select value:0x%x\n",
		dt_info->phy_id, dt_info->controller_id, cphy_sel_val);
	regmap_update_bits(dt_info->syscon.mm_ahb,
		dt_info->syscon.dphy_sel, dt_info->syscon.dphy_msk,
		cphy_sel_val);

	return 0;
}

/*
 * testen:1 + clk:falling => addr write operation
 * testen:0 + clk:rising  => data write operation
 * testout: (RO) read out
 * testin: phy internal register programme
 *
 * testclr ___/---\_____
 *
 * testen _________/------\______
 *
 * testclk ____________/--\____/--\____/--\___
 *
 * testin __________________/----\______/---\___
 *
 * testout ____________________/-----\_/-----\___
 *
 */
int phy_read(int idx, int addr)
{
	int temp = 0;

	/* testen = 1 */
	reg_wr(csi_addr[idx] + PHY_TEST_CRTL1, BIT_16);
	udelay(10);
	/* testclk = 1 */
	reg_wr(csi_addr[idx] + PHY_TEST_CRTL0, BIT_1);
	udelay(10);
	reg_wr(csi_addr[idx] + PHY_TEST_CRTL1, addr | BIT_16);
	udelay(10);
	/* testclk = 0 */
	reg_wr(csi_addr[idx] + PHY_TEST_CRTL0, 0);
	udelay(10);
	temp = reg_rd(csi_addr[idx] + PHY_TEST_CRTL1);

	return (temp);
}

void phy_write(int idx, int addr, int data)
{
	pr_debug("idx:%d, addr:0x%x, data:0x%x\n", idx, addr, data);

	/* testen = 1 */
	reg_wr(csi_addr[idx] + PHY_TEST_CRTL1, BIT_16);
	udelay(10);
	/* testclk = 1 */
	reg_wr(csi_addr[idx] + PHY_TEST_CRTL0, BIT_1);
	udelay(10);
	/* set reg addr */
	reg_wr(csi_addr[idx] + PHY_TEST_CRTL1, addr | BIT_16);
	udelay(10);
	/* testclk = 0 */
	reg_wr(csi_addr[idx] + PHY_TEST_CRTL0, 0);
	udelay(10);

	/* testen = 0 for data */
	reg_wr(csi_addr[idx] + PHY_TEST_CRTL1, /*addr*/0);
	udelay(10);
	reg_wr(csi_addr[idx] + PHY_TEST_CRTL1, data);
	udelay(10);
	/* testclk = 1 */
	reg_wr(csi_addr[idx] + PHY_TEST_CRTL0, BIT_1);
	reg_wr(csi_addr[idx] + PHY_TEST_CRTL0, 0);
}

void dphy_2p2l_init(int phy_id, int csi_id, int sensor_id)
{
	pr_info("phy_id: %d, csi_id:%d, sensor_id:%d\n", phy_id, csi_id, sensor_id);
	if (phy_id == PHY_2P2
		|| phy_id == PHY_2P2_M
		|| phy_id == PHY_2P2_S) {
		pr_info("set 2p2l work mode\n");
		/* M/S_EN */
		//reg_mwr(0x323f0058, BIT_24|BIT_21, BIT_24|BIT_21);
		/* set 2p2l mode 1:4lane 0:2lane */
		if (phy_id == PHY_2P2_M
			|| phy_id == PHY_2P2_S)
			reg_mwr(0x323f000c, BIT_4, ~BIT_4);
		else {
			//reg_mwr(0x323f000c, BIT_4, BIT_4);
		}

		pr_info("config phy debug module\n");
		switch (phy_id) {
		case PHY_2P2:
		case PHY_2P2_M:
		case PHY_2P2_S:
			reg_mwr(0x323f0000 + 0x60, BIT_3, BIT_3); //dbg_dsi_if_sel_db = 1
			reg_mwr(0x323f0000 + 0x4C, BIT_5, ~BIT_5); //DSI_IF_SEL_DB = 0
			reg_mwr(0x323f0000 + 0x50, BIT_0, BIT_0); //DSI_TESTCLR_DB = 1
			break;
		default:
			pr_info("this phy is not 2p2\n");
		}

		pr_info("reset phy from controller\n");
		dphy_cfg_clr(sensor_id);

		pr_info("set phy force shutdownz\n");
		reg_mwr(0x323f0000 + 0x58, BIT_26, BIT_26); //CSI force S shutdownz
		reg_mwr(0x323f0000 + 0x58, BIT_28, BIT_28); //CSI force shutdownz

		pr_info("init 2p2l_s to lane2/3\n");
		reg_wr(0x62200030, 0x1b<<(csi_id*6));
		dphy_cfg_clr(sensor_id);
		pr_info("0x62200030:0x%x\n", reg_rd(0x62200030));
		phy_write(csi_id, 0x4d, 0x48);
		pr_info("0x4d = 0x%d\n", phy_read(csi_id, 0x4d));
		phy_write(csi_id, 0x5d, 0x68);
		pr_info("0x5d = 0x%d\n", phy_read(csi_id, 0x5d));
	}else {
		pr_info("reset phy from controller\n");
		dphy_cfg_clr(sensor_id);

		pr_info("set phy force shutdownz\n");
		reg_mwr(0x323f0000 + 0x58, BIT_26, BIT_26); //CSI force S shutdownz
		reg_mwr(0x323f0000 + 0x58, BIT_28, BIT_28); //CSI force shutdownz
	}
}

int dphy_csi_match(struct csi_dt_node_info *dt_info)
{
	uint32_t cphy_sel_val;
	uint32_t rx2_sel_val;

	/* init phy->csi = 0 */
	if (!init_flag) {
		reg_mwr(0x62200030, 0x30007fff, 0);
		init_flag = 1;
	}

	/* Roc1 */
	if (!dt_info) {
		pr_err("input param is invalid\n");
		return -EINVAL;
	}

	switch (dt_info->phy_id) {
	case PHY_2P2: {
		cphy_sel_val = 0x12;
		rx2_sel_val = 0x20002000;
		break;
	}
	case PHY_4LANE: {
		cphy_sel_val = 0x0;
		rx2_sel_val = 0x0;
		break;
	}
	case PHY_4LANE_1: {
		cphy_sel_val = 0x9;
		rx2_sel_val = 0x10001000;
		break;
	}
	case PHY_2P2_S: {
		cphy_sel_val = 0x1B;
		rx2_sel_val = 0x30003000;
		break;
	}
	case PHY_2P2_M: {
		cphy_sel_val = 0x1A;
		rx2_sel_val = 0x30002000;
		break;
	}
	default:
		pr_err("fail to get valid csi phy id\n");
		return -1;
	}

	pr_info("phyid:%d, csi_id:%d, select value:0x%x\n",
					dt_info->phy_id, dt_info->controller_id, cphy_sel_val);

	if (dt_info->controller_id != 2)
		regmap_update_bits(dt_info->syscon.mm_ahb,
			dt_info->syscon.dphy_sel, dt_info->syscon.dphy_msk,
			(cphy_sel_val << (dt_info->controller_id*6)));
	else
		regmap_update_bits(dt_info->syscon.mm_ahb,
			dt_info->syscon.dphy_sel, dt_info->syscon.dphy_msk,
			rx2_sel_val);

	pr_info("0x62200030: 0x%x\n", reg_rd(0x62200030));

	return 0;
}

void dphy_init_state(unsigned int phyid, int csi_id, int sensor_id)
{
	struct dphy_info *phy = get_phy_info(phyid);
	if (!phy) {
		pr_err("fail to get valid phy ptr\n");
		return;
	}

	if (phy->phy_id == PHY_4LANE || phy->phy_id == PHY_2LANE)
		dphy_cfg_clr(sensor_id);

	if (phy->phy_id == PHY_2P2
		|| phy->phy_id == PHY_2P2_M
		|| phy->phy_id == PHY_2P2_S) {
		regmap_update_bits(phy->cam_ahb, REG_MM_AHB_MIPI_CSI_SEL_CTRL,
			0x7 << (csi_id*6), 0x0 << (csi_id*6));

		/* To init 2p2l_s on SharkL5 */
		phy_write(csi_id, 0x4d, 0x48);
		phy_write(csi_id, 0x5d, 0x68);
	}
}
