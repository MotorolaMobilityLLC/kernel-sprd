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
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include "csi_access.h"
#include "csi_api.h"
#include "csi_driver.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CSI_DRV: %d %s: "\
	fmt, __LINE__, __func__

static int csi_core_initialized[ADDR_COUNT] = { 0 };

static uint8_t csi_core_write(enum csi_registers_t address, uint32_t data,
			      int32_t idx)
{
	if (csi_core_initialized[idx] == 0) {
		pr_err("fail to initialise CSI driver\n");
		return ERR_NOT_INIT;
	}
	pr_debug("data %d addr%d\n", data, address >> 2);
	access_write(data, address >> 2, idx);

	return SUCCESS;
}

static uint32_t csi_core_read(enum csi_registers_t address, int32_t idx)
{
	return access_read(address >> 2, idx);
}

static uint8_t csi_core_write_part(enum csi_registers_t address,
				   uint32_t data, uint8_t shift,
				   uint8_t width, int32_t idx)
{
	uint32_t mask = (1 << width) - 1;
	uint32_t temp = csi_core_read(address, idx);

	temp &= ~(mask << shift);
	temp |= (data & mask) << shift;
	return csi_core_write(address, temp, idx);
}

static uint32_t csi_core_read_part(enum csi_registers_t address,
				   uint8_t shift, uint8_t width, int32_t idx)
{
	return (csi_core_read(address, idx) >> shift) & ((1 << width) - 1);
}

static void dphy_cfg_start(int32_t idx)
{
	csi_core_write_part(PHY_TST_CRTL1, 0, PHY_TESTEN, 1, idx);
	/* phy_testen = 0 */
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL0, 1, PHY_TESTCLK, 1, idx);
	udelay(1);
}

static void dphy_cfg_done(int32_t idx)
{
	csi_core_write_part(PHY_TST_CRTL1, 0, PHY_TESTEN, 1, idx);
	/* phy_testen = 0 */
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL0, 1, PHY_TESTCLK, 1, idx);
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL1, 0, PHY_TESTDIN, PHY_TESTDIN_W, idx);
	/* phy_ testdin */
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL1, 1, PHY_TESTEN, 1, idx);
	/* phy_testen = 1 */
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL0, 0, PHY_TESTCLK, 1, idx);
	/* phy_testclk = 0 */
	udelay(1);
}

#ifdef SPRD_CSI_DPHY_TEST
static void dphy_write(uint8_t test_code, uint8_t test_data,
			uint8_t *test_out, int32_t idx)
{
	uint32_t temp = 0xffffff00;

	csi_core_write_part(PHY_TST_CRTL1, 0, PHY_TESTEN, 1, idx);
	/* phy_testen = 0 */
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL0, 1, PHY_TESTCLK, 1, idx);
	udelay(1);
	/* phy_testdin */
	csi_core_write_part(PHY_TST_CRTL1, test_code, PHY_TESTDIN,
			PHY_TESTDIN_W, idx);
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL1, 1, PHY_TESTEN, 1, idx);
	/* phy_testen = 1 */
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL0, 0, PHY_TESTCLK, 1, idx);
	/* phy_testclk = 0 */
	udelay(1);
	temp = csi_core_read_part(PHY_TST_CRTL1, PHY_TESTDOUT,
				PHY_TESTDOUT_W, idx);
	*test_out = (uint8_t) temp;
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL1, 0, PHY_TESTEN, 1, idx);
	/* phy_testen = 0 */
	udelay(1);
	/* phy_testdin */
	csi_core_write_part(PHY_TST_CRTL1, test_data, PHY_TESTDIN,
			PHY_TESTDIN_W, idx);
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL0, 1, PHY_TESTCLK, 1, idx);
	udelay(1);
}
#endif

static void dphy_testclr_db_init(struct csi_phy_info *phy)
{
	regmap_update_bits(phy->anlg_phy_g7_syscon,
			REG_ANALOG_2P2LANE_SEL_CFG_0,
			BIT_ANALOG_2P2LANE_DSI_IF_SEL_DB,
			BIT_ANALOG_2P2LANE_DSI_IF_SEL_DB);
}

static void dphy_init_common(struct csi_phy_info *phy, uint32_t bps_per_lane,
			uint32_t phy_id, uint32_t rx_mode, int32_t idx)
{
	csi_core_write_part(PHY_SHUTDOWNZ, 0, 0, 1, idx);
	csi_core_write_part(DPHY_RSTZ, 0, 0, 1, idx);
	csi_core_write_part(PHY_TST_CRTL0, 1, PHY_TESTCLR, 1, idx);
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL0, 0, PHY_TESTCLR, 1, idx);
	udelay(1);

	dphy_cfg_start(idx);

	/*
	 * dphy_write(0x04, 0x50, &temp, idx);
	 * dphy_write(0x37, 0x05, &temp, idx);
	 * dphy_write(0x47, 0x05, &temp, idx);
	 * dphy_write(0x57, 0x05, &temp, idx);
	 * dphy_write(0x67, 0x05, &temp, idx);
	 * dphy_write(0x77, 0x05, &temp, idx);
	 */

	dphy_cfg_done(idx);
}

/*
 * control testclr_db, testclr_m, testclr_s
 * If not do this, sometims phy work unnormal
 */
static void csi_dphy_2p2_testclr_set(struct csi_phy_info *phy)
{
	unsigned int mask = 0;

	dphy_testclr_db_init(phy);
	mask = BIT_ANALOG_CPHY_DBG_TEST_CLR;
	regmap_update_bits(phy->anlg_phy_g7_syscon,
			REG_ANALOG_CPHY_TEST_CTRL,
			mask, mask);
	mask = BIT_ANALOG_2P2L_TESTCLR_M_SEL | BIT_ANALOG_2P2L_TESTCLR_S_SEL;
	regmap_update_bits(phy->anlg_phy_g7_syscon,
			REG_ANALOG_CPHY_BIST_TEST,
			mask, mask);
	udelay(1);
	mask = BIT_ANALOG_2P2L_TESTCLR_M | BIT_ANALOG_2P2L_TESTCLR_S;
	regmap_update_bits(phy->anlg_phy_g7_syscon,
			REG_ANALOG_CPHY_BIST_TEST,
			mask, mask);
	udelay(1);
}

static void csi_dphy_2p2_testclr_clear(struct csi_phy_info *phy)
{
	unsigned int mask = 0;

	mask = BIT_ANALOG_CPHY_DBG_TEST_CLR;
	regmap_update_bits(phy->anlg_phy_g7_syscon,
			REG_ANALOG_CPHY_TEST_CTRL,
			mask, ~mask);
	mask = BIT_ANALOG_2P2L_TESTCLR_M | BIT_ANALOG_2P2L_TESTCLR_S;
	regmap_update_bits(phy->anlg_phy_g7_syscon,
			REG_ANALOG_CPHY_BIST_TEST,
			mask, ~mask);
}

void csi_phy_power_down(struct csi_phy_info *phy, unsigned int csi_id,
			int is_eb)
{
	uint32_t ps_pd_l = 0;
	uint32_t ps_pd_s = 0;
	uint32_t shutdownz = 0;
	uint32_t iso_sw = 0;
	uint32_t dphy_eb = 0;
	uint32_t reg = 0;
	unsigned int mask = 0;

	if (!phy)
		return;

	switch (csi_id) {
	case CSI_RX0:
		mask = BIT_ANALOG_FORCE_PHY_CSI_SHUTDOWN;
		regmap_update_bits(phy->anlg_phy_g7_syscon,
				   REG_ANALOG_CPHY_BIST_TEST,
				   mask, mask);
		break;
	case CSI_RX1:
		mask = BIT_ANALOG_FORCE_PHY_CSI_S_SHUTDOWN;
		regmap_update_bits(phy->anlg_phy_g7_syscon,
				   REG_ANALOG_CPHY_BIST_TEST,
				   mask, mask);
		break;
	default:
		pr_err("fail to get csi id\n");
		break;
	}

	switch (phy->phy_id) {
	case PHY_2P2:
		/* We will use 2P2L 4 Lane PHY as 4Lane PHY  */
		mask = BIT_ANALOG_2P2LANE_MODE_SEL;
		regmap_update_bits(phy->anlg_phy_g7_syscon,
				   REG_ANALOG_2P2LANE_CTRL_CSI_2P2L,
				   mask, mask);
		ps_pd_l = BIT_AON_APB_MIPI_CSI_2P2LANE_PS_PD_L;
		ps_pd_s = BIT_AON_APB_MIPI_CSI_2P2LANE_PS_PD_S;
		shutdownz = BIT_AON_APB_FORCE_CSI_2P2LANE_PHY_SHUTDOWNZ;
		iso_sw = BIT_AON_APB_CSI_ISO_SW_EN_2P2LANE;
		reg = REG_AON_APB_PWR_CTRL;

		break;
	case PHY_4LANE:
		ps_pd_l = BIT_AON_APB_MIPI_CSI_4LANE_PS_PD_L;
		ps_pd_s = BIT_AON_APB_MIPI_CSI_4LANE_PS_PD_S;
		shutdownz = BIT_AON_APB_FORCE_4LANE_PHY_SHUTDOWNZ;
		iso_sw = BIT_AON_APB_CSI_ISO_SW_EN_4LANE;
		reg = REG_AON_APB_PWR_CTRL;
		break;
	case PHY_2P2_M:
	case PHY_2P2_S:
		/* We will use 2P2L 4 Lane PHY as 2x2Lane PHY  */
		mask = BIT_ANALOG_2P2LANE_MODE_SEL;
		regmap_update_bits(phy->anlg_phy_g7_syscon,
				   REG_ANALOG_2P2LANE_CTRL_CSI_2P2L,
				   mask, ~mask);
		ps_pd_l = BIT_AON_APB_MIPI_CSI_2P2LANE_PS_PD_L;
		ps_pd_s = BIT_AON_APB_MIPI_CSI_2P2LANE_PS_PD_S;
		shutdownz = BIT_AON_APB_FORCE_CSI_2P2LANE_PHY_SHUTDOWNZ;
		iso_sw = BIT_AON_APB_CSI_ISO_SW_EN_2P2LANE;
		reg = REG_AON_APB_PWR_CTRL;

		break;
	default:
		pr_err("fail to get csi phy id %d\n", csi_id);
		return;
	}

	dphy_eb = BIT_AON_APB_SERDES_DPHY_EB;
	if (is_eb) {
		regmap_update_bits(phy->ana_apb_syscon,
				   reg,
				   ps_pd_l | ps_pd_s | iso_sw,
				   ps_pd_l | ps_pd_s | iso_sw);
		regmap_update_bits(phy->ana_apb_syscon,
				   reg,
				   shutdownz,
				   ~shutdownz);
		if (phy->phy_id == PHY_2P2 ||
		    phy->phy_id == PHY_2P2_M ||
		    phy->phy_id == PHY_2P2_S) {
			regmap_update_bits(phy->ana_apb_syscon,
					   REG_AON_APB_APB_EB1,
					   dphy_eb, ~dphy_eb);
		}
	} else {
		if (phy->phy_id == PHY_2P2 ||
		    phy->phy_id == PHY_2P2_M ||
		    phy->phy_id == PHY_2P2_S)
			csi_dphy_2p2_testclr_set(phy);
		regmap_update_bits(phy->ana_apb_syscon,
				   reg,
				   ps_pd_s,
				   ~ps_pd_s);
		udelay(100);

		regmap_update_bits(phy->ana_apb_syscon,
				   reg,
				   ps_pd_l,
				   ~ps_pd_l);

		regmap_update_bits(phy->ana_apb_syscon,
				   reg,
				   iso_sw,
				   ~iso_sw);

		regmap_update_bits(phy->ana_apb_syscon,
				   reg,
				   shutdownz,
				   shutdownz);

		if (phy->phy_id == PHY_2P2 ||
		    phy->phy_id == PHY_2P2_M ||
		    phy->phy_id == PHY_2P2_S) {
			regmap_update_bits(phy->ana_apb_syscon,
					   REG_AON_APB_APB_EB1,
					   dphy_eb, dphy_eb);
			csi_dphy_2p2_testclr_clear(phy);
		}
	}
}

int csi_ahb_reset(struct csi_phy_info *phy, unsigned int csi_id)
{
	unsigned int flag = 0;

	if (!phy) {
		pr_err("fail to get phy ptr is NULL\n");
		return -EINVAL;
	}
	pr_debug("csi, id %d dphy %d\n", csi_id, phy->phy_id);

	switch (csi_id) {
	case CSI_RX0: {
		flag = BIT_MM_AHB_CSI0_SOFT_RST;
		break;
	}
	case CSI_RX1: {
		flag = BIT_MM_AHB_CSI1_SOFT_RST;
		break;
	}
	default:
		flag = BIT_MM_AHB_CSI0_SOFT_RST;
		pr_err("fail to get csi id\n");
		break;
	}

	regmap_update_bits(phy->cam_ahb_syscon,
			   REG_MM_AHB_RST, flag, flag);
	udelay(1);
	regmap_update_bits(phy->cam_ahb_syscon,
			   REG_MM_AHB_RST, flag, ~flag);

	return 0;
}

static void csi_phy_pair(struct csi_phy_info *phy, unsigned int csi_id)
{
	uint32_t cphy_sel_mask = 0;
	uint32_t cphy_sel_val = 0;

	switch (phy->phy_id) {
	case PHY_2P2:
		cphy_sel_val = 0x03;
		break;
	case PHY_4LANE:
		cphy_sel_val = 0x02;
		break;
	case PHY_2P2_M:
		cphy_sel_val = 0x01;
		break;
	case PHY_2P2_S:
		cphy_sel_val = 0x00;
		break;
	default:
		pr_err("fail to get valid phy_id %d\n", phy->phy_id);
		break;
	}
	cphy_sel_mask = 0x03 << (csi_id * 2);
	cphy_sel_val <<= csi_id * 2;
	regmap_update_bits(phy->cam_ahb_syscon,
			REG_MM_AHB_MIPI_CSI2_CTRL,
			cphy_sel_mask, cphy_sel_val);
}

void csi_enable(struct csi_phy_info *phy, unsigned int csi_id,
			unsigned int sensor_id)
{
	uint32_t mask = 0;

	pr_info("csi, id %d dphy %d\n", csi_id, phy->phy_id);

	switch (csi_id) {
	case CSI_RX0: {
		mask = BIT_MM_AHB_CSI0_EB;
		regmap_update_bits(phy->cam_ahb_syscon, REG_MM_AHB_EB,
				mask, mask);

		mask = BIT_MM_AHB_CSI0_SOFT_RST;
		regmap_update_bits(phy->cam_ahb_syscon, REG_MM_AHB_RST,
				mask, mask);
		udelay(1);
		regmap_update_bits(phy->cam_ahb_syscon, REG_MM_AHB_RST,
				mask, ~mask);
		break;
	}
	case CSI_RX1: {
		mask = BIT_MM_AHB_CSI1_EB;
		regmap_update_bits(phy->cam_ahb_syscon, REG_MM_AHB_EB,
				mask, mask);

		mask = BIT_MM_AHB_CSI1_SOFT_RST;
		regmap_update_bits(phy->cam_ahb_syscon, REG_MM_AHB_RST,
				mask, mask);
		udelay(1);
		regmap_update_bits(phy->cam_ahb_syscon, REG_MM_AHB_RST,
				mask, ~mask);
		break;
	}
	default:
		pr_err("fail to get csi id\n");
		break;
	}
	csi_phy_pair(phy, csi_id);
}

uint8_t csi_init(unsigned long base_address, uint32_t version, int32_t idx)
{
	enum csi_error_t e = SUCCESS;

	do {
		if (csi_core_initialized[idx] == 0) {
			access_init((uint32_t *) base_address, idx);
			if (csi_core_read(VERSION, idx) == version) {
				pr_debug("CSI Driver init ok\n");
				csi_core_initialized[idx] = 1;
				break;
			}
			pr_err("fail to compatible CSI Driver with core\n");
			e = ERR_NOT_COMPATIBLE;
			break;
		}
		pr_info("CSI driver already initialised\n");
		e = ERR_ALREADY_INIT;
		break;
	} while (0);

	return e;
}

void dphy_init(struct csi_phy_info *phy, uint32_t bps_per_lane,
	       uint32_t phy_id, int32_t idx)
{
	dphy_init_common(phy, bps_per_lane, phy_id, 0, idx);
}

uint8_t csi_set_on_lanes(uint8_t lanes, int32_t idx)
{
	return csi_core_write_part(N_LANES, (lanes - 1), 0, 2, idx);
}

uint8_t csi_get_on_lanes(int32_t idx)
{
	return (csi_core_read_part(N_LANES, 0, 2, idx) + 1);
}

void csi_dump_reg(void)
{
	int i = 0;
	int idx = 0;

	for (idx = 0; idx < 2; idx++) {
		if (csi_core_initialized[idx] == 0)
			continue;
		pr_info("dump csi%d reg:\n", idx);
		for (i = 0; i < 7; i++) {
			pr_info("0x%.8x: 0x%.8x 0x%.8x 0x%.8x 0x%.8x\n",
			16*i,
			access_read(4*i, idx),
			access_read(4*i + 1, idx),
			access_read(4*i + 2, idx),
			access_read(4*i + 3, idx));
		}
		pr_info("0x%.8x: 0x%.8x\n", 16*i, access_read(4*i, idx));
	}
}
EXPORT_SYMBOL(csi_dump_reg);

uint8_t csi_shut_down_phy(uint8_t shutdown, int32_t idx)
{
	pr_debug("shutdown %d\n", shutdown);
	/* active low - bit 0 */
	return csi_core_write_part(PHY_SHUTDOWNZ, shutdown ? 0 : 1, 0, 1, idx);
}

uint8_t csi_reset_controller(int32_t idx)
{
	/* active low - bit 0 */
	int retVal = 0xffff;

	retVal = csi_core_write_part(CSI2_RESETN, 0, 0, 1, idx);
	switch (retVal) {
	case SUCCESS:
		return csi_core_write_part(CSI2_RESETN, 1, 0, 1, idx);
	case ERR_NOT_INIT:
		pr_err("fail to get driver status, Driver not initialized\n");
		return retVal;
	default:
		pr_err("fail to get defined val\n");
		return ERR_UNDEFINED;
	}
}

uint8_t csi_reset_phy(int32_t idx)
{
	/* active low - bit 0 */
	int retVal = 0xffff;

	retVal = csi_core_write_part(DPHY_RSTZ, 0, 0, 1, idx);
	switch (retVal) {
	case SUCCESS:
		return csi_core_write_part(DPHY_RSTZ, 1, 0, 1, idx);
	case ERR_NOT_INIT:
		pr_err("fail to get driver status, Driver not initialized\n");
		return retVal;
	default:
		pr_err("fail to get defined val\n");
		return ERR_UNDEFINED;
	}
}

uint8_t csi_event_enable(uint32_t mask, uint32_t err_reg_no,
				int32_t idx)
{
	switch (err_reg_no) {
	case 1:
		return csi_core_write(MASK1,
			(~mask) & csi_core_read(MASK1, idx), idx);
	case 2:
		return csi_core_write(MASK2,
			(~mask) & csi_core_read(MASK2, idx), idx);
	default:
		return ERR_OUT_OF_BOUND;
	}
}

uint8_t csi_event_disable(uint32_t mask, uint8_t err_reg_no,
				int32_t idx)
{
	switch (err_reg_no) {
	case 1:
		return csi_core_write(MASK1, mask | csi_core_read(MASK1, idx),
				      idx);
	case 2:
		return csi_core_write(MASK2, mask | csi_core_read(MASK2, idx),
				      idx);
	default:
		return ERR_OUT_OF_BOUND;
	}
}

uint8_t csi_close(int32_t idx)
{
	uint8_t ret = 0;

	ret = csi_shut_down_phy(1, idx);
	ret = csi_reset_controller(idx);
	ret = csi_reset_phy(idx);
	csi_core_initialized[idx] = 0;
	return ret;
}
