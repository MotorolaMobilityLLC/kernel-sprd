/*
 * sprd_pdbg.h -- unisoc Power Debug driver support.
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

#ifndef __LINUX_UNISOC_POWER_DEBUG_H_
#define __LINUX_UNISOC_POWER_DEBUG_H_

#include <linux/device.h>
#include <linux/sprd_sip_svc.h>

#define MAX_INTC_NUM 8
#define BIT_NUM_IN_PER_REG 0x20
#define MAX_STATES_NUM_PER_REG 8


struct reg_check {
	u32 addr_offset;
	u32 value_mask;
	u32 expect_value;
	char *preg_name;
};

struct pdm_info {
	u32 addr_offset;
	u32 pwd_bit_width;
	u32 bit_index[MAX_STATES_NUM_PER_REG];
	char *pdm_name[MAX_STATES_NUM_PER_REG];
};

struct intc_info {
	u32 addr_offset;
	char *pint_name[32];
};

/**
 * struct power_debug_desc - Static power_debug descriptor
 *
 * Each power_debug registered with the core is described with a
 * structure of this type and a struct regulator_config.  This
 * structure contains the non-varying parts of the regulator
 * description.
 *
 * @name: Identifying name for the power debug.
 *
 * @pmu_pdm_num: the number of entries in the following "@ppdm_info" array.
 * @ap_ahb_reg_num: the number of entries in the following "@ap_ahb_reg" array.
 * @ap_apb_reg_num: the number of entries in the following "@ap_apb_reg" array.
 * @pmu_apb_reg_num: the number of entries in the following "@pmu_apb_reg"
 *	array.
 * @aon_apb_reg_num: the number of entries in the following "@aon_sec_reg"
 *	array.
 * @aon_sec_reg_num: the number of entries in the following "@ppdm_info" array.
 * @ap_intc_num: the number of entries in the following "@pintc_info" array.
 * @irq_mask: The mask identify the interrupt which wanted to be recognized.
 *
 * @ppdm_info: the pointer of power domain info array.
 * @ap_ahb_reg: the pointer of ap-ahb register array which will be checked.
 * @ap_apb_reg: the pointer of ap-apb register array which will be checked.
 * @pmu_apb_reg: the pointer of pmu-apb register array which will be checked.
 * @aon_apb_reg: the pointer of aon-apb register array which will be checked.
 * @aon_sec_reg: the pointer of aon-sec register array which will be checked.
 * @pintc_info: the pointer of intc info array.
 * @log_2nd_irq_source: the function pointer which used to log the 2nd interrupt
 *	source.
 */
struct power_debug_desc {
	const char *name;

	u32 pmu_pdm_num;
	u32 ap_ahb_reg_num;
	u32 ap_apb_reg_num;
	u32 pmu_apb_reg_num;
	u32 aon_apb_reg_num;
	u32 aon_sec_reg_num;
	u32 ap_intc_num;
	u32 irq_mask[MAX_INTC_NUM];

	struct pdm_info *ppdm_info;
	struct reg_check *ap_ahb_reg;
	struct reg_check *ap_apb_reg;
	struct reg_check *pmu_apb_reg;
	struct reg_check *aon_apb_reg;
	struct reg_check *aon_sec_reg;
	struct intc_info *pintc_info;
	void (*log_2nd_irq_source)(void *pentry, u32 hw_irq_nr);
};

struct power_debug_cfg {
	struct device_node *pnode;
	u32 pdbg_enable;
	u32 scan_interval;

	struct regmap *ap_ahb;
	struct regmap *ap_apb;
	struct regmap *pmu_apb;
	struct regmap *aon_apb;
	struct regmap *aon_sec;
	struct regmap *ap_intc[0];
};

struct power_debug {
	u32 pdbg_enable;
	u32 scan_interval;
	struct task_struct *task;

	struct device *dev;
	struct power_debug_desc *pdesc;
	struct power_debug_cfg *pcfg;
	struct mutex conf_mutex;
	struct sprd_sip_svc_handle *svc_handle;
};

extern struct power_debug *sprd_power_debug_register(
				struct device *dev,
				struct power_debug_desc *pdesc,
				struct power_debug_cfg *pcfg);

extern void sprd_power_debug_unregister(struct power_debug *pdbg);

extern u32 sprd_pdb_read_pmic_register(u32 chip_id, u32 offset_addr);
#endif
