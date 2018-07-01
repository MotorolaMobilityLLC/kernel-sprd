/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#include <linux/kernel.h>
#include <linux/delay.h>

#include "sprd_dphy_hal.h"

static int dphy_wait_pll_locked(struct sprd_dphy *dphy)
{
	u32 i = 0;

	for (i = 0; i < 50000; i++) {
		if (dphy_hal_is_pll_locked(dphy))
			return 0;
		udelay(3);
	}

	pr_err("error: dphy pll can not be locked\n");
	return -1;
}

static int dphy_wait_datalane_stop_state(struct sprd_dphy *dphy, u8 mask)
{
	u32 i = 0;

	for (i = 0; i < 5000; i++) {
		if (dphy_hal_is_stop_state_datalane(dphy) == mask)
			return 0;
		udelay(10);
	}

	pr_err("datalane ulps exit wait time out\n");
	return -1;
}

static int dphy_wait_clklane_stop_state(struct sprd_dphy *dphy)
{
	u32 i = 0;

	for (i = 0; i < 5000; i++) {
		if (dphy_hal_is_stop_state_clklane(dphy))
			return 0;
		udelay(10);
	}

	pr_err("clklane ulps exit wait time out\n");
	return -1;
}

/**
 * Configure D-PHY and PLL module to desired operation mode
 * @param dphy: pointer to structure
 *  which holds information about the d-dphy module
 * @param no_of_lanes active
 * @param freq desired high speed frequency
 * @return error code
 */
int sprd_dphy_configure(struct sprd_dphy *dphy)
{
	struct dphy_pll_ops *pll = dphy->pll;
	struct dphy_context *ctx = &dphy->ctx;

	pr_info("lanes : %d\n", ctx->lanes);
	pr_info("freq : %d\n", ctx->freq);

	dphy_hal_rstz(dphy, 0);
	dphy_hal_shutdownz(dphy, 0);
	dphy_hal_clklane_en(dphy, 0);

	dphy_hal_test_clr(dphy, 0);
	dphy_hal_test_clr(dphy, 1);
	dphy_hal_test_clr(dphy, 0);

	pll->pll_config(ctx);
	pll->timing_config(ctx);

	dphy_hal_shutdownz(dphy, 1);
	dphy_hal_rstz(dphy, 1);
	dphy_hal_stop_wait_time(dphy, 0x1C);
	dphy_hal_clklane_en(dphy, 1);
	dphy_hal_datalane_en(dphy);

	if (dphy_wait_pll_locked(dphy))
		return -1;

	return 0;
}

void sprd_dphy_reset(struct sprd_dphy *dphy)
{
	dphy_hal_rstz(dphy, 0);
	udelay(10);
	dphy_hal_rstz(dphy, 1);
}

void sprd_dphy_shutdown(struct sprd_dphy *dphy)
{
	dphy_hal_shutdownz(dphy, 0);
	udelay(10);
	dphy_hal_shutdownz(dphy, 1);
}

int sprd_dphy_hop_config(struct sprd_dphy *dphy, int delta, int period)
{
	struct dphy_pll_ops *pll = dphy->pll;
	struct dphy_context *ctx = &dphy->ctx;

	if (pll->hop_config)
		return pll->hop_config(ctx, delta, period);

	return 0;
}

int sprd_dphy_ssc_en(struct sprd_dphy *dphy, bool en)
{
	struct dphy_pll_ops *pll = dphy->pll;
	struct dphy_context *ctx = &dphy->ctx;

	if (pll->ssc_en)
		return pll->ssc_en(ctx, en);

	return 0;
}

/**
 * Close and power down D-PHY module
 * @param dphy pointer to structure which holds information about the d-dphy
 * module
 * @return error code
 */
int sprd_dphy_close(struct sprd_dphy *dphy)
{
	if (!dphy)
		return -1;

	dphy_hal_rstz(dphy, 0);
	dphy_hal_shutdownz(dphy, 0);
	dphy_hal_rstz(dphy, 1);

	return 0;
}
/**
 * Wake up or make sure D-PHY PLL module is awake
 * This function must be called after going into ULPS and before exiting it
 * to force the DPHY PLLs to wake up. It will wait until the DPHY status is
 * locked. It follows the procedure described in the user guide.
 * This function should be used to make sure the PLL is awake, rather than
 * the force_pll above.
 * @param dphy pointer to structure which holds information about the d-dphy
 * module
 * @return error code
 * @note this function has an active wait
 */
int sprd_dphy_wakeup_pll(struct sprd_dphy *dphy)
{
	if (dphy_hal_is_pll_locked(dphy))
		return 0;

	dphy_hal_force_pll(dphy, 1);

	if (dphy_wait_pll_locked(dphy))
		return -1;

	return 0;
}

/**
 * ULPS mode request/exit on all active data lanes.
 * @param dphy pointer to structure which holds information about the d-dphy
 * module
 * @param enable (request 1/ exit 0)
 * @return error code
 * @note this is a blocking function. wait upon exiting the ULPS will exceed 1ms
 */
int sprd_dphy_data_ulps_en(struct sprd_dphy *dphy, int enable)
{
	u8 mask = 0;
	u16 lanes = dphy->ctx.lanes;

	if (enable)
		dphy_hal_datalane_ulps_rqst(dphy, 1);
	else {
		if (!dphy_hal_is_pll_locked(dphy)) {
			pr_err("dphy pll is not locked\n");
			return -1;
		}

		dphy_hal_datalane_ulps_exit(dphy, 1);
		switch (lanes) {
		/* Fall through */
		case 4:
			mask |= BIT(3);
		/* Fall through */
		case 3:
			mask |= BIT(2);
		/* Fall through */
		case 2:
			mask |= BIT(1);
		/* Fall through */
		case 1:
			mask |= BIT(0);
			break;
		default:
			break;
		}

		/* verify that the DPHY has left ULPM */
		dphy_wait_datalane_stop_state(dphy, mask);

		dphy_hal_datalane_ulps_rqst(dphy, 0);
		dphy_hal_datalane_ulps_exit(dphy, 0);
	}

	return 0;
}

/**
 * ULPS mode request/exit on Clock Lane.
 * @param dphy pointer to structure which holds information about the
 * d-dphy module
 * @param enable 1 or disable 0 of the Ultra Low Power State of the clock lane
 * @return error code
 * @note this is a blocking function. wait upon exiting the ULPS will exceed 1ms
 */
int sprd_dphy_clk_ulps_en(struct sprd_dphy *dphy, int enable)
{
	if (enable)
		dphy_hal_clklane_ulps_rqst(dphy, 1);
	else {
		if (!dphy_hal_is_pll_locked(dphy)) {
			pr_err("dphy pll is not locked\n");
			return -1;
		}

		dphy_hal_clklane_ulps_exit(dphy, 1);

		/* verify that the DPHY has left ULPM */
		dphy_wait_clklane_stop_state(dphy);

		dphy_hal_clklane_ulps_rqst(dphy, 0);
		dphy_hal_clklane_ulps_exit(dphy, 0);
	}

	return 0;
}

void sprd_dphy_hs_clk_en(struct sprd_dphy *dphy, int enable)
{
	dphy_hal_clk_hs_rqst(dphy, enable);

	dphy_wait_pll_locked(dphy);
}

/**
 * Write to D-PHY module (encapsulating the digital interface)
 * @param dphy pointer to structure which holds information about the d-dphy
 * module
 * @param address offset inside the D-PHY digital interface
 * @param data array of bytes to be written to D-PHY
 * @param data_length of the data array
 */
void sprd_dphy_test_write(struct sprd_dphy *dphy, u8 address, u8 data)
{
	dphy_hal_test_en(dphy, 1);

	dphy_hal_test_din(dphy, address);

	dphy_hal_test_clk(dphy, 1);
	dphy_hal_test_clk(dphy, 0);

	dphy_hal_test_en(dphy, 0);

	dphy_hal_test_din(dphy, data);

	dphy_hal_test_clk(dphy, 1);
	dphy_hal_test_clk(dphy, 0);
}

u8 sprd_dphy_test_read(struct sprd_dphy *dphy, u8 address)
{
	dphy_hal_test_en(dphy, 1);

	dphy_hal_test_din(dphy, address);

	dphy_hal_test_clk(dphy, 1);
	dphy_hal_test_clk(dphy, 0);

	dphy_hal_test_en(dphy, 0);

	udelay(1);

	return dphy_hal_test_dout(dphy);
}



