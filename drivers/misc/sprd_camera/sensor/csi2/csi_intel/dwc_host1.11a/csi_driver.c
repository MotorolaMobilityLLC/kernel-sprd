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

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon/sprd-glb.h>
#include <linux/regmap.h>
#include "csi_access.h"
#include "csi_api.h"
#include "csi_driver.h"

#define CSI_DEBUG (0)
static int csi_core_initialized[ADDR_COUNT] = { 0 };

static int csi_core_write_part(enum csi_registers_t address, unsigned int data,
			       unsigned int shift, unsigned int width, int idx)
{
	unsigned int mask = (1 << width) - 1;
	unsigned int temp = csi_core_read(address, idx);

	temp &= ~(mask << shift);
	temp |= (data & mask) << shift;

	return csi_core_write(address, temp, idx);
}

static void dphy_cfg_start(int idx)
{
	csi_core_write_part(PHY_TST_CRTL1, 0, PHY_TESTEN, 1, idx);
	/* phy_testen = 0 */
	udelay(1);
	csi_core_write_part(PHY_TST_CRTL0, 1, PHY_TESTCLK, 1, idx);
	udelay(1);
}

static void dphy_cfg_done(int idx)
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

static void dphy_init_common(unsigned int bps_per_lane, unsigned int phy_id,
			     unsigned int rx_mode, int idx)
{
	csi_core_write_part(PHY_SHUTDOWNZ, 0, 0, 1, idx);
	csi_core_write_part(DPHY_RSTZ, 0, 0, 1, idx);
	csi_core_write_part(CSI2_RESETN, 0, 0, 1, idx);
	csi_core_write_part(PHY_TST_CRTL0, 1, PHY_TESTCLR, 1, idx);
	udelay(5);
	csi_core_write_part(PHY_TST_CRTL0, 0, PHY_TESTCLR, 1, idx);
	udelay(2);
	dphy_cfg_start(idx);
	dphy_cfg_done(idx);
}

void csi_phy_power_down(struct csi_phy_info *phy, unsigned int csi_id,
			int is_eb)
{
	unsigned int csi_ctrl_val;
	unsigned int csi_ctrl_reg;
	unsigned int pwr_ctrl_reg;
	unsigned int pwr_ctrl_val0;
	unsigned int pwr_ctrl_val1;
	unsigned int pwr_ctrl_val2;
	unsigned int pwr_ctrl_val3;

	pr_info("csi power_down phy id %d, en %d\n", phy->phy_id, is_eb);
	if (phy == NULL)
		return;

	/* pgen -> fwen_b -> pwrgood -> rst */
	switch (phy->phy_id) {
	case 0:
		csi_ctrl_val = BIT_ANLG_PHY_G8_ANALOG_CSI_0_I_CTL_ENABLE_CLN;
		csi_ctrl_reg = REG_ANLG_PHY_G8_ANALOG_CSI_0_PPI_CTRL_REG0;
		pwr_ctrl_val0 = BIT_ANLG_PHY_G8_ANALOG_CSI_0_I_CTL_PGEN;
		pwr_ctrl_val1 = BIT_ANLG_PHY_G8_ANALOG_CSI_0_I_CTL_FWEN_B;
		pwr_ctrl_val2 = BIT_ANLG_PHY_G8_ANALOG_CSI_0_I_DFX_PWRGOOD;
		pwr_ctrl_val3 = BIT_ANLG_PHY_G8_ANALOG_CSI_0_I_SYS_RST_B;
		pwr_ctrl_reg = REG_ANLG_PHY_G8_ANALOG_CSI_0_PWR_CTRL_REG0;
		break;
	case 1:
		csi_ctrl_val = BIT_ANLG_PHY_G8_ANALOG_CSI_1_I_CTL_ENABLE_CLN;
		csi_ctrl_reg = REG_ANLG_PHY_G8_ANALOG_CSI_1_PPI_CTRL_REG0;
		pwr_ctrl_val0 = BIT_ANLG_PHY_G8_ANALOG_CSI_1_I_CTL_PGEN;
		pwr_ctrl_val1 = BIT_ANLG_PHY_G8_ANALOG_CSI_1_I_CTL_FWEN_B;
		pwr_ctrl_val2 = BIT_ANLG_PHY_G8_ANALOG_CSI_1_I_DFX_PWRGOOD;
		pwr_ctrl_val3 = BIT_ANLG_PHY_G8_ANALOG_CSI_1_I_SYS_RST_B;
		pwr_ctrl_reg = REG_ANLG_PHY_G8_ANALOG_CSI_1_PWR_CTRL_REG0;
		break;
	default:
		pr_info("invalid phy id %d\n", phy->phy_id);
		return;
	}

	if (is_eb) {
		regmap_update_bits(phy->anlg_phy_g8_gpr,
				   pwr_ctrl_reg,
				   pwr_ctrl_val3,
				   ~pwr_ctrl_val3);
		regmap_update_bits(phy->anlg_phy_g8_gpr,
				   pwr_ctrl_reg,
				   pwr_ctrl_val2,
				   ~pwr_ctrl_val2);
		/* when chipID is 0/1, needs to force on */
		if (phy->chip_id == 2) {
			regmap_update_bits(phy->anlg_phy_g8_gpr,
					pwr_ctrl_reg,
					pwr_ctrl_val1,
					~pwr_ctrl_val1);
			regmap_update_bits(phy->anlg_phy_g8_gpr,
					pwr_ctrl_reg,
					pwr_ctrl_val0,
					~pwr_ctrl_val0);
		}
		regmap_update_bits(phy->anlg_phy_g8_gpr,
				   csi_ctrl_reg,
				   csi_ctrl_val,
				   ~csi_ctrl_val);
	} else {
		regmap_update_bits(phy->anlg_phy_g8_gpr,
				   csi_ctrl_reg,
				   csi_ctrl_val,
				   csi_ctrl_val);
		regmap_update_bits(phy->anlg_phy_g8_gpr,
				   pwr_ctrl_reg,
				   pwr_ctrl_val0,
				   pwr_ctrl_val0);
		regmap_update_bits(phy->anlg_phy_g8_gpr,
				   pwr_ctrl_reg,
				   pwr_ctrl_val1,
				   pwr_ctrl_val1);
		regmap_update_bits(phy->anlg_phy_g8_gpr,
				   pwr_ctrl_reg,
				   pwr_ctrl_val2,
				   pwr_ctrl_val2);
		regmap_update_bits(phy->anlg_phy_g8_gpr,
				   pwr_ctrl_reg,
				   pwr_ctrl_val3,
				   pwr_ctrl_val3);
	}
}

void csi_phy_init(struct csi_phy_info *phy)
{
	if (phy == NULL)
		return;

	/* init csi phy0 */
	regmap_update_bits(phy->anlg_phy_g8_gpr,
			   REG_ANLG_PHY_G8_ANALOG_CSI_0_PWR_CTRL_REG0,
			   BIT_ANLG_PHY_G8_ANALOG_CSI_0_I_CTL_PGEN,
			   BIT_ANLG_PHY_G8_ANALOG_CSI_0_I_CTL_PGEN);
	regmap_update_bits(phy->anlg_phy_g8_gpr,
			   REG_ANLG_PHY_G8_ANALOG_CSI_0_PWR_CTRL_REG0,
			   BIT_ANLG_PHY_G8_ANALOG_CSI_0_I_CTL_FWEN_B,
			   BIT_ANLG_PHY_G8_ANALOG_CSI_0_I_CTL_FWEN_B);

	/* init csi phy1 */
	regmap_update_bits(phy->anlg_phy_g8_gpr,
			   REG_ANLG_PHY_G8_ANALOG_CSI_1_PWR_CTRL_REG0,
			   BIT_ANLG_PHY_G8_ANALOG_CSI_1_I_CTL_PGEN,
			   BIT_ANLG_PHY_G8_ANALOG_CSI_1_I_CTL_PGEN);
	regmap_update_bits(phy->anlg_phy_g8_gpr,
			   REG_ANLG_PHY_G8_ANALOG_CSI_1_PWR_CTRL_REG0,
			   BIT_ANLG_PHY_G8_ANALOG_CSI_1_I_CTL_FWEN_B,
			   BIT_ANLG_PHY_G8_ANALOG_CSI_1_I_CTL_FWEN_B);
}

void csi_enable(struct csi_phy_info *phy, unsigned int csi_id)
{
	unsigned int mask;
	static int flag;

	pr_info("enable csi, id %d dphy %d\n", csi_id, phy->phy_id);

	if (!flag) {
		mask = BIT_CAM_AHB_CSI0_EB;
		regmap_update_bits(phy->cam_ahb_gpr, REG_CAM_AHB_AHB_EB,
				   mask, mask);

		mask = BIT_CAM_AHB_CSI0_SOFT_RST;
		regmap_update_bits(phy->cam_ahb_gpr, REG_CAM_AHB_AHB_RST,
				   mask, mask);
		udelay(1);
		regmap_update_bits(phy->cam_ahb_gpr, REG_CAM_AHB_AHB_RST,
				   mask, ~mask);

		mask = BIT_CAM_AHB_CSI1_EB;
		regmap_update_bits(phy->cam_ahb_gpr, REG_CAM_AHB_AHB_EB,
				   mask, mask);

		mask = BIT_CAM_AHB_CSI1_SOFT_RST;
		regmap_update_bits(phy->cam_ahb_gpr, REG_CAM_AHB_AHB_RST,
				   mask, mask);
		udelay(1);
		regmap_update_bits(phy->cam_ahb_gpr, REG_CAM_AHB_AHB_RST,
				   mask, ~mask);

		flag = 1;
	}
}

int csi_init(unsigned long base_address, unsigned int version, int idx)
{
	int ret = SUCCESS;

	do {
		if (csi_core_initialized[idx] == 0) {
			access_init((unsigned int *)base_address, idx);
			if (csi_core_read(VERSION, idx) == version) {
				pr_info("csi driver init successfully\n");
				csi_core_initialized[idx] = 1;
				break;
			}
			pr_info("csi driver not compatible with core\n");
			ret = ERR_NOT_COMPATIBLE;
			break;
		}
		pr_info("%s csi driver already initialised\n", __func__);
		ret = ERR_ALREADY_INIT;
		break;
	} while (0);

	return ret;
}

unsigned int csi_core_read(enum csi_registers_t address, int idx)
{
	return access_read(address >> 2, idx);
}

int csi_core_write(enum csi_registers_t address, unsigned int data, int idx)
{
	if (csi_core_initialized[idx] == 0) {
		pr_info("%s csi driver not initialised\n", __func__);
		return ERR_NOT_INIT;
	}
	pr_debug("%s data %d addr%d\n", __func__, data, address >> 2);
	access_write(data, address >> 2, idx);

	return SUCCESS;
}

unsigned int csi_core_read_part(enum csi_registers_t address,
				unsigned int shift, unsigned int width, int idx)
{
	return (csi_core_read(address, idx) >> shift) & ((1 << width) - 1);
}

void dphy_init(struct csi_phy_info *phy, unsigned int bps_per_lane,
	       unsigned int phy_id, int idx)
{
	dphy_init_common(bps_per_lane, phy_id, 0, idx);
}

int csi_set_on_lanes(unsigned int lanes, int idx)
{
	return csi_core_write_part(N_LANES, (lanes - 1), 0, 3, idx);
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

int csi_shut_down_phy(unsigned int shutdown, int idx)
{
	pr_debug("csi shutdown %d\n", shutdown);
	/* active low - bit 0 */
	return csi_core_write_part(PHY_SHUTDOWNZ, shutdown ? 0 : 1, 0, 1, idx);
}

int csi_reset_controller(int idx)
{
	/* active low - bit 0 */
	int ret = 0xffff;

	ret = csi_core_write_part(CSI2_RESETN, 0, 0, 1, idx);
	switch (ret) {
	case SUCCESS:
		return csi_core_write_part(CSI2_RESETN, 1, 0, 1, idx);
	case ERR_NOT_INIT:
		pr_info("%s csi driver not initialized\n", __func__);
		return ret;
	default:
		pr_err("%s undefined error\n", __func__);
		return ERR_UNDEFINED;
	}
}

int csi_reset_phy(int idx)
{
	/* active low - bit 0 */
	int ret = 0xffff;

	ret = csi_core_write_part(DPHY_RSTZ, 0, 0, 1, idx);
	switch (ret) {
	case SUCCESS:
		return csi_core_write_part(DPHY_RSTZ, 1, 0, 1, idx);
	case ERR_NOT_INIT:
		pr_info("%s csi driver not initialized\n", __func__);
		return ret;
	default:
		pr_err("%s undefined error\n", __func__);
		return ERR_UNDEFINED;
	}
}

int csi_event_enable(unsigned int mask, unsigned int err_reg_no, int idx)
{
	switch (err_reg_no) {
	case 1:
		/* PHY FATAL */
		return csi_core_write(INT_MSK_PHY_FATAL,
			(~mask) & csi_core_read(INT_MSK_PHY_FATAL, idx), idx);
	case 2:
		/* PKT FATAL */
		return csi_core_write(INT_MSK_PKT_FATAL,
			(~mask) & csi_core_read(INT_MSK_PKT_FATAL, idx), idx);
	case 3:
		/* FRAME FATAL */
		return csi_core_write(INT_MSK_FRAME_FATAL,
			(~mask) & csi_core_read(INT_MSK_FRAME_FATAL, idx), idx);
	case 4:
		/* PHY */
		return csi_core_write(INT_MSK_PHY,
			(~mask) & csi_core_read(INT_MSK_PHY, idx), idx);
	case 5:
		/* PKT */
		return csi_core_write(INT_MSK_PKT,
			(~mask) & csi_core_read(INT_MSK_PKT, idx), idx);
	case 6:
		/* LINE */
		return csi_core_write(INT_MSK_LINE,
			(~mask) & csi_core_read(INT_MSK_LINE, idx), idx);
	case 7:
		/* IPI */
		return csi_core_write(INT_MSK_IPI,
			(~mask) & csi_core_read(INT_MSK_IPI, idx), idx);
	default:
		return ERR_OUT_OF_BOUND;
	}
}

int csi_event_disable(unsigned int mask, unsigned int err_reg_no, int idx)
{
	switch (err_reg_no) {
	case 1:
		/* PHY FATAL */
		return csi_core_write(INT_MSK_PHY_FATAL,
			mask | csi_core_read(INT_MSK_PHY_FATAL, idx), idx);
	case 2:
		/* PKT FATAL */
		return csi_core_write(INT_MSK_PKT_FATAL,
			mask | csi_core_read(INT_MSK_PKT_FATAL, idx), idx);
	case 3:
		/* FRAME FATAL */
		return csi_core_write(INT_MSK_FRAME_FATAL,
			mask | csi_core_read(INT_MSK_FRAME_FATAL, idx), idx);
	case 4:
		/* PHY */
		return csi_core_write(INT_MSK_PHY,
			mask | csi_core_read(INT_MSK_PHY, idx), idx);
	case 5:
		/* PKT */
		return csi_core_write(INT_MSK_PKT,
			mask | csi_core_read(INT_MSK_PKT, idx), idx);
	case 6:
		/* LINE */
		return csi_core_write(INT_MSK_LINE,
			mask | csi_core_read(INT_MSK_LINE, idx), idx);
	case 7:
		/* IPI */
		return csi_core_write(INT_MSK_IPI,
			mask | csi_core_read(INT_MSK_IPI, idx), idx);
	default:
		return ERR_OUT_OF_BOUND;
	}
}

int csi_close(int idx)
{
	int ret = 0;

	ret = csi_shut_down_phy(1, idx);
	ret = csi_reset_controller(idx);
	csi_core_initialized[idx] = 0;

	return ret;
}

#if __INT_CSI__
irqreturn_t csi_api_event_handler(int irq, void *handle)
{
	unsigned int idx = *((unsigned int *)handle);
	unsigned int reg = csi_core_read(INT_ST_MAIN, idx);
#if CSI_DEBUG
	int i = 0;
#endif
	/* get main state value */
	pr_info("csi%d main: 0x%x\n", idx, reg);
#if CSI_DEBUG
	csi_core_write(INT_ST_MAIN, 0x00000000, idx);
	for (i = 0; i < 3; i++) {
		pr_info("csi%d reg 0x%x: 0x%x\n", idx,
			(INT_ST_PHY_FATAL + i*0x10),
			csi_core_read((INT_ST_PHY_FATAL + i*0x10), idx));
	}
#endif

	return 0;
}
#endif
