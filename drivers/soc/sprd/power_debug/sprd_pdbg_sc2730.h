/*
 * sprd_pdbg_sc2730.h -- unisoc Power Debug driver support.
 *
 * Copyright (C) 2020, 2021 unisoc.
 *
 * Author: James Chen <Jamesj.Chen@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Power Debug Driver Interface.
 */
#ifndef __LINUX_UNISOC_POWER_DEBUG_SC2730_H_
#define __LINUX_UNISOC_POWER_DEBUG_SC2730_H_

#define PMIC_INTC_OFFSET 0x0080
#define INTC_INTSTA_REG  0x0000
#define INTC_EIC_INDEX	 4
#define PMIC_EIC_OFFSET  0x0280
#define EIC_INTSTA_REG   0x0020

static char *intc_int_name[] = {
	"ADC_INT", "RTC_INT", "WDG_INT", "FGU_INT",
	"EIC_INT", "FAST_CHG_INT", "TMR_INT", "CAL_INT",
	"TYPEC_INT", "USB_PD_INT"};

static char *eic_int_name[] = {
	"CHGR_INT", "PBINT", "PBINT2", "BATDET_OK",
	"KEY2_TS_EXT_RSTN", "EXT_XTL0_EN", "AUD_INT_ALL", "ENDURA_OPTION"};

static void sc2730_output_irq_source(struct regmap *pmic_regmap)
{
	u32 i, j;
	u32 intc_state = 0, eic_state = 0;

	if (!pmic_regmap)
		return;

	regmap_read(pmic_regmap,
		PMIC_INTC_OFFSET + INTC_INTSTA_REG, &intc_state);
	for (i = 0; i < sizeof(intc_int_name)/sizeof(char *); i++) {
		if (intc_state & (1 << i)) {
			pr_info("	#--Wake up by %d(%s:%s)!\n",
				i, "PMIC_INTC", intc_int_name[i]);
			if (i == INTC_EIC_INDEX) {
				regmap_read(pmic_regmap,
					PMIC_EIC_OFFSET + EIC_INTSTA_REG,
					&eic_state);
				for (j = 0;
	j < sizeof(eic_int_name)/sizeof(char *); j++) {
					if (eic_state & (1 << j))
						pr_info("		#--Wake up by %d(%s:%s)!\n",
							j, "PMIC_EIC",
							eic_int_name[j]);
				}
			}
		}
	}

	local_irq_disable();
}

#endif
