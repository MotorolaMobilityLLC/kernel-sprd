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
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/mfd/syscon/sprd-glb.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include "csi_access.h"
#include "csi_api.h"
#include "csi_driver.h"

static int csi_core_initialized[ADDR_COUNT] = { 0 };

static const struct csi_pclk_cfg csi_pclk_setting[CSI_PCLK_CFG_COUNTER] = {
	{80, 90, 0x00, 4},
	{90, 100, 0x10, 4},
	{100, 110, 0x20, 4},
	{110, 130, 0x01, 6},
	{130, 140, 0x11, 6},
	{140, 150, 0x21, 6},
	{150, 170, 0x02, 9},
	{170, 180, 0x12, 9},
	{180, 200, 0x22, 9},
	{200, 220, 0x03, 10},
	{220, 240, 0x13, 10},
	{240, 250, 0x23, 10},
	{250, 270, 0x04, 13},
	{270, 300, 0x14, 13},
	{300, 330, 0x05, 17},
	{330, 360, 0x15, 17},
	{360, 400, 0x25, 17},
	{400, 450, 0x06, 25},
	{450, 500, 0x16, 26},
#if 0
	{400, 450, 0x06, 23},
	{450, 500, 0x16, 23},
#endif
	{500, 550, 0x07, 28},
	{550, 600, 0x17, 28},
	{600, 650, 0x08, 33},
	{650, 700, 0x18, 33},
	{700, 750, 0x09, 38},
	{750, 800, 0x19, 38},
	{800, 850, 0x29, 38},
	{850, 900, 0x39, 38},
	{900, 950, 0x0A, 52},
	{950, 1000, 0x1A, 52},
	{1000, 1050, 0x2A, 52},
	{1050, 1100, 0x3A, 52},
	{1100, 1150, 0x0B, 68},
	{1150, 1200, 0x1B, 68},
	{1200, 1250, 0x2B, 68},
};

static void dpy_ab_clr(void)
{
}

#if 0
static void dpy_a_enable(struct csi_phy_info *phy)
{

	unsigned int mask = BIT_CAM_AHB_MIPI_CPHY0_SEL;

	regmap_update_bits(phy->cam_ahb_syscon,
			   REG_CAM_AHB_MIPI_CSI_CTRL, mask, ~mask);
}

static void dpy_b_enable(struct csi_phy_info *phy)
{
	unsigned int mask = BIT_CAM_AHB_MIPI_CPHY1_SEL;

	regmap_update_bits(phy->cam_ahb_syscon,
			   REG_CAM_AHB_MIPI_CSI_CTRL, mask, mask);
}
#endif

static uint8_t csi_core_write_part(enum csi_registers_t address, u32 data,
				   u8 shift, u8 width, int32_t idx)
{
	uint32_t mask = (1 << width) - 1;
	uint32_t temp = csi_core_read(address, idx);

	temp &= ~(mask << shift);
	temp |= (data & mask) << shift;
	return csi_core_write(address, temp, idx);
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

static void dphy_init_common(uint32_t bps_per_lane, uint32_t phy_id,
			     uint32_t rx_mode, int32_t idx)
{
	csi_core_write_part(PHY_SHUTDOWNZ, 0, 0, 1, idx);
	csi_core_write_part(DPHY_RSTZ, 0, 0, 1, idx);
	csi_core_write_part(PHY_TST_CRTL0, 1, PHY_TESTCLR, 1, idx);
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL0, 0, PHY_TESTCLR, 1, idx);
	udelay(1);
	dphy_cfg_start(idx);
	/* sprd phy config just for whale2 bringup */
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

void csi_phy_power_down(struct csi_phy_info *phy, unsigned int csi_id,
			int is_eb)
{
	uint32_t ps_pd, shutdownz;
	uint32_t reg;

	if (!phy)
		return;

	switch (csi_id) {
	case 0:
		ps_pd = BIT_ANA_APB_MIPI_CSI0_PS_PD_L |
			BIT_ANA_APB_MIPI_CSI0_PS_PD_S;
		shutdownz = BIT_ANA_APB_FORCE_CSI0_PHY_SHUTDOWNZ;
		reg = REG_ANA_APB_PWR_CTRL1;
		break;
	case 1:
		ps_pd = BIT_ANA_APB_MIPI_CSI1_PS_PD_L |
			BIT_ANA_APB_MIPI_CSI1_PS_PD_S;
		shutdownz = BIT_ANA_APB_FORCE_CSI1_PHY_SHUTDOWNZ;
		reg = REG_ANA_APB_PWR_CTRL1;
		break;
	case 2:
		ps_pd = BIT_ANA_APB_MIPI_CSI2_PS_PD_L |
			BIT_ANA_APB_MIPI_CSI2_PS_PD_S;
		shutdownz = BIT_ANA_APB_FORCE_CSI2_PHY_SHUTDOWNZ;
		reg = REG_ANA_APB_MIPI_PHY_CTRL5;
		break;
	default:
		pr_info("invalid phy id %d\n", csi_id);
		return;
	}

	if (is_eb) {
		regmap_update_bits(phy->ana_apb_syscon,
				   reg,
				   ps_pd,
				   ps_pd);
		regmap_update_bits(phy->ana_apb_syscon,
				   reg,
				   shutdownz,
				   ~shutdownz);
	} else {
		regmap_update_bits(phy->ana_apb_syscon,
				   reg,
				   ps_pd,
				   ~ps_pd);
		regmap_update_bits(phy->ana_apb_syscon,
				   reg,
				   shutdownz,
				   shutdownz);
	}
}

void csi_enable(struct csi_phy_info *phy, unsigned int csi_id)
{
#if IS_ENABLED(CONFIG_SPRD_CAMERA_ISPG2V1)
	unsigned int mask;
	static int flag;

	pr_info("%s csi, id %d dphy %d\n", __func__, csi_id, phy->phy_id);

	if (!flag) {
		mask = BIT_CAM_AHB_CSI0_EB;
		regmap_update_bits(phy->cam_ahb_syscon, REG_CAM_AHB_AHB_EB,
				   mask, mask);

		mask = BIT_CAM_AHB_CSI0_SOFT_RST;
		regmap_update_bits(phy->cam_ahb_syscon, REG_CAM_AHB_AHB_RST,
				   mask, mask);
		udelay(1);
		regmap_update_bits(phy->cam_ahb_syscon, REG_CAM_AHB_AHB_RST,
				   mask, ~mask);

		mask = BIT_CAM_AHB_CSI1_EB;
		regmap_update_bits(phy->cam_ahb_syscon, REG_CAM_AHB_AHB_EB,
				   mask, mask);

		mask = BIT_CAM_AHB_CSI1_SOFT_RST;
		regmap_update_bits(phy->cam_ahb_syscon, REG_CAM_AHB_AHB_RST,
				   mask, mask);
		udelay(1);
		regmap_update_bits(phy->cam_ahb_syscon, REG_CAM_AHB_AHB_RST,
				   mask, ~mask);

		flag = 1;
	}
#endif
}

uint8_t csi_init(unsigned long base_address, uint32_t version, int32_t idx)
{
	enum csi_error_t e = SUCCESS;

	do {
		if (csi_core_initialized[idx] == 0) {
			access_init((uint32_t *) base_address, idx);
			if (csi_core_read(VERSION, idx) == version) {
				pr_err("CSI Driver init ok\n");
				csi_core_initialized[idx] = 1;
				break;
			}
			pr_err("CSI Driver not compatible with core\n");
			e = ERR_NOT_COMPATIBLE;
			break;
		}
		pr_err("CSI driver already initialised\n");
		e = ERR_ALREADY_INIT;
		break;
	} while (0);

	return e;
}

uint32_t csi_core_read(enum csi_registers_t address, int32_t idx)
{
	return access_read(address >> 2, idx);
}

uint8_t csi_core_write(enum csi_registers_t address, uint32_t data, int32_t idx)
{
	if (csi_core_initialized[idx] == 0) {
		pr_err("CSI driver not initialised\n");
		return ERR_NOT_INIT;
	}
	pr_debug("%s: data %d addr%d\n", __func__, data, address >> 2);
	access_write(data, address >> 2, idx);

	return SUCCESS;
}

uint32_t csi_core_read_part(enum csi_registers_t address, uint8_t shift,
			    uint8_t width, int32_t idx)
{
	return (csi_core_read(address, idx) >> shift) & ((1 << width) - 1);
}

void dphy_init(struct csi_phy_info *phy, uint32_t bps_per_lane,
	       uint32_t phy_id, int32_t idx)
{
	dpy_ab_clr();

	dphy_init_common(bps_per_lane, phy_id, 0, idx);
#if 0
	if (idx == 1) {
		dpy_b_enable(phy);
		dphy_init_common(bps_per_lane, phy_id, 1, idx);
	}
#endif
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
		for (i = 0; i < 5; i++)
			pr_info("0x%.8x: 0x%.8x 0x%.8x 0x%.8x 0x%.8x\n",
			16*i,
			access_read(4*i, idx),
			access_read(4*i + 1, idx),
			access_read(4*i + 2, idx),
			access_read(4*i + 3, idx));
	}
}
EXPORT_SYMBOL(csi_dump_reg);

uint8_t csi_shut_down_phy(uint8_t shutdown, int32_t idx)
{
	pr_debug("%s:shutdown %d\n", __func__, shutdown);
	/* active low - bit 0 */
	return csi_core_write_part(PHY_SHUTDOWNZ, shutdown ? 0 : 1, 0, 1, idx);
}

enum csi_lane_state_t csi_lane_module_state(uint8_t lane, int32_t idx)
{
	if (lane < csi_core_read_part(N_LANES, 0, 2, idx) + 1) {
		if (csi_core_read_part(PHY_STATE, lane, 1, idx))
			return CSI_LANE_ULTRA_LOW_POWER;
		else if (csi_core_read_part(PHY_STATE, (lane + 4), 1, idx))
			return CSI_LANE_STOP;
		return CSI_LANE_ON;
	}
	pr_warn("CSI Lane switched off\n");
	return CSI_LANE_OFF;
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
		pr_err("%s: Driver not initialized\n", __func__);
		return retVal;
	default:
		pr_err("%s Undefined error\n", __func__);
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
		pr_err("%s: Driver not initialized\n", __func__);
		return retVal;
	default:
		pr_err("%s: Undefined error\n", __func__);
		return ERR_UNDEFINED;
	}
}

uint8_t csi_event_enable(uint32_t mask, uint32_t err_reg_no, int32_t idx)
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

uint8_t csi_event_disable(uint32_t mask, uint8_t err_reg_no, int32_t idx)
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

uint32_t csi_event_get_source(uint8_t err_reg_no, int32_t idx)
{
	switch (err_reg_no) {
	case 1:
		return csi_core_read(ERR1, idx);
	case 2:
		return csi_core_read(ERR2, idx);
	default:
		return ERR_OUT_OF_BOUND;
	}
}

enum csi_lane_state_t csi_clk_state(int32_t idx)
{
	if (!csi_core_read_part(PHY_STATE, 9, 1, idx))
		return CSI_LANE_ULTRA_LOW_POWER;
	else if (csi_core_read_part(PHY_STATE, 10, 1, idx))
		return CSI_LANE_STOP;
	else if (csi_core_read_part(PHY_STATE, 8, 1, idx))
		return CSI_LANE_HIGH_SPEED;

	return CSI_LANE_ON;
}

uint8_t csi_get_registered_line_event(uint8_t offset, int32_t idx)
{
	enum csi_registers_t reg_offset = 0;

	pr_debug("%s\n", __func__);
	if (offset > 8)
		return ERR_OUT_OF_BOUND;
	reg_offset = ((offset / 4) == 1) ? DATA_IDS_2 : DATA_IDS_1;
	return (u8) csi_core_read_part(reg_offset, (offset * 8), 8, idx);
}

uint8_t csi_register_line_event(u8 virtual_channel_no,
				enum csi_data_type_t data_type, u8 offset,
				int32_t idx)
{
	u8 id = 0;
	enum csi_registers_t reg_offset = 0;

	pr_debug("%s\n", __func__);
	if ((virtual_channel_no > 4) || (offset > 8))
		return ERR_OUT_OF_BOUND;
	id = (virtual_channel_no << 6) | data_type;

	reg_offset = ((offset / 4) == 1) ? DATA_IDS_2 : DATA_IDS_1;

	return csi_core_write_part(reg_offset, id, (offset * 8), 8, idx);
}

uint8_t csi_unregister_line_event(uint8_t offset, int32_t idx)
{
	enum csi_registers_t reg_offset = 0;

	pr_debug("%s\n", __func__);
	if (offset > 8)
		return ERR_OUT_OF_BOUND;
	reg_offset = ((offset / 4) == 1) ? DATA_IDS_2 : DATA_IDS_1;
	return csi_core_write_part(reg_offset, 0x00, (offset * 8), 8, idx);
}

uint8_t csi_payload_bypass(uint8_t on, int32_t idx)
{
	return csi_core_write_part(PHY_STATE, on ? 1 : 0, 11, 1, idx);
}

uint8_t csi_close(int32_t idx)
{
	uint8_t ret = 0;

	ret = csi_shut_down_phy(1, idx);
	ret = csi_reset_controller(idx);
	csi_core_initialized[idx] = 0;
	return ret;
}
