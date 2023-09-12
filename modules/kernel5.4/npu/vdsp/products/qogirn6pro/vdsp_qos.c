/*
 * SPDX-FileCopyrightText: 2022 Unisoc (Shanghai) Technologies Co., Ltd
 * SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
 *
 * Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include "vdsp_qos.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: qos %d: %d %s:" \
	fmt, current->pid, __LINE__, __func__

typedef struct _QOS_REG_STRUCT
{
	const char *reg_name;
	unsigned int base_addr;
	unsigned int mask_value;
	unsigned int set_value;
} QOS_REG_T;

#define SIZE_OF(list) (sizeof(list) / sizeof(list[0]))

#define VDSP_MTX_QOS_BASE	(0x30050000)
#define MM_SYS_MTX_QOS_BASE	(0x30070000)
#define MM_AHB_BASE		(0x30000000)

QOS_REG_T nic400_vdsp_mst_mtx_m0_qos_list[] = {
	{ "REGU_AXQOS_GEN_EN",	VDSP_MTX_QOS_BASE + 0x60,
		0x80000003, 0x00000003},
	{ "REGU_AXQOS_GEN_CFG",	VDSP_MTX_QOS_BASE + 0x64,
		0x3fff3fff, 0x09960996},
	{ "REGU_URG_CNT_CFG",	VDSP_MTX_QOS_BASE + 0x68,
		0x00000701, 0x00000001},
};

QOS_REG_T nic400_vdsp_mst_mtx_m1_qos_list[] = {
	{ "REGU_AXQOS_GEN_EN",	VDSP_MTX_QOS_BASE + 0xE0,
		0x80000003, 0x00000003},
	{ "REGU_AXQOS_GEN_CFG",	VDSP_MTX_QOS_BASE + 0xE4,
		0x3fff3fff, 0x09960996},
	{ "REGU_URG_CNT_CFG",	VDSP_MTX_QOS_BASE + 0xE8,
		0x00000701, 0x00000001},
};

QOS_REG_T nic400_vdsp_mst_mtx_m2_qos_list[] = {
	{ "REGU_AXQOS_GEN_EN",	VDSP_MTX_QOS_BASE + 0x160,
		0x80000003, 0x00000003},
	{ "REGU_AXQOS_GEN_CFG",	VDSP_MTX_QOS_BASE + 0x164,
		0x3fff3fff, 0x09960996},
	{ "REGU_URG_CNT_CFG",	VDSP_MTX_QOS_BASE + 0x168,
		0x00000701, 0x00000001},
};

QOS_REG_T nic400_mm_sys_mtx_m0_qos_list[] = {
	{ "REGU_OT_CTRL_EN",	 MM_SYS_MTX_QOS_BASE,
		0x00000001, 0x00000001},
	{ "REGU_OT_CTRL_AW_CFG", MM_SYS_MTX_QOS_BASE + 0x4,
		0xffffffff, 0x08080402},
	{ "REGU_OT_CTRL_AR_CFG", MM_SYS_MTX_QOS_BASE + 0x08,
		0x3f3f3f3f, 0x02030101},
	{ "REGU_OT_CTRL_Ax_CFG", MM_SYS_MTX_QOS_BASE + 0x0C,
		0x3f3fffff, 0x04020808},
	{ "REGU_AXQOS_GEN_EN",	 MM_SYS_MTX_QOS_BASE + 0x60,
		0x80000003, 0x00000003},
	{ "REGU_AXQOS_GEN_CFG",	 MM_SYS_MTX_QOS_BASE + 0x64,
		0x3fff3fff, 0x07710888},
	{ "REGU_URG_CNT_CFG",	 MM_SYS_MTX_QOS_BASE + 0x68,
		0x00000701, 0x00000001},
};

QOS_REG_T camerasys_glb_rf_top_qos_list[] = {
	{ "MM_SYS_M0_LPC_CTRL",		MM_AHB_BASE + 0x3C,
		0x00010000, 0x00000000},
	{ "MM_SYS_M0_LPC_CTRL",		MM_AHB_BASE + 0x3C,
		0x00010000, 0x00010000},
	{ "MM_SYS_M1_LPC_CTRL",		MM_AHB_BASE + 0x40,
		0x00010000, 0x00000000},
	{ "MM_SYS_M1_LPC_CTRL",		MM_AHB_BASE + 0x40,
		0x00010000, 0x00010000},
	{ "MM_SYS_M2_LPC_CTRL",		MM_AHB_BASE + 0x44,
		0x00010000, 0x00000000},
	{ "MM_SYS_M2_LPC_CTRL",		MM_AHB_BASE + 0x44,
		0x00010000, 0x00010000},
	{ "MM_SYS_M3_LPC_CTRL",		MM_AHB_BASE + 0x48,
		0x00010000, 0x00000000},
	{ "MM_SYS_M3_LPC_CTRL",		MM_AHB_BASE + 0x48,
		0x00010000, 0x00010000},
	{ "MM_VDSP_MST_M0_LPC_CTRL",	MM_AHB_BASE + 0x80,
		0x00010000, 0x00000000},
	{ "MM_VDSP_MST_M0_LPC_CTRL",	MM_AHB_BASE + 0x80,
		0x00010000, 0x00010000},
	{ "MM_VDSP_MST_M1_LPC_CTRL",	MM_AHB_BASE + 0x84,
		0x00010000, 0x00000000},
	{ "MM_VDSP_MST_M1_LPC_CTRL",	MM_AHB_BASE + 0x84,
		0x00010000, 0x00010000},
	{ "MM_VDSP_MST_M2_LPC_CTRL",	MM_AHB_BASE + 0x88,
		0x00010000, 0x00000000},
	{ "MM_VDSP_MST_M2_LPC_CTRL",	MM_AHB_BASE + 0x88,
		0x00010000, 0x00010000},
};

static void set_reg(QOS_REG_T *list, int length, unsigned int reg_base,
		    void __iomem *map_addr)
{
	int i;
	void __iomem *temp_addr = NULL;
	uint32_t reg_val, temp;

	for(i = 0; i < length; i++) {
		temp_addr = list[i].base_addr - reg_base + map_addr;
		temp = readl_relaxed(temp_addr);
		reg_val = (temp & (~list[i].mask_value))
			| list[i].set_value;
		writel_relaxed(reg_val, temp_addr);
	}
}

#define QOS_FUNC(list, reg_base, map_addr) \
	(set_reg(list, SIZE_OF(list), reg_base, map_addr))

int set_qos(void *hw_arg)
{
	void __iomem *addr = NULL;

	(void)hw_arg;
	pr_debug("set qos\n");

	addr = ioremap(VDSP_MTX_QOS_BASE, 0x200);
	if (!addr) {
		pr_err("[error]:ioremap 0x%x failed\n", VDSP_MTX_QOS_BASE);
		return -ENOSYS;
	}
	QOS_FUNC(nic400_vdsp_mst_mtx_m0_qos_list, VDSP_MTX_QOS_BASE, addr);
	QOS_FUNC(nic400_vdsp_mst_mtx_m1_qos_list, VDSP_MTX_QOS_BASE, addr);
	QOS_FUNC(nic400_vdsp_mst_mtx_m2_qos_list, VDSP_MTX_QOS_BASE, addr);
	iounmap(addr);

	addr = ioremap(MM_AHB_BASE, 0x100);
	if (!addr) {
		pr_err("[error]:ioremap 0x%x failed\n", MM_AHB_BASE);
		return -ENOSYS;
	}
	QOS_FUNC(camerasys_glb_rf_top_qos_list, MM_AHB_BASE, addr);
	iounmap(addr);
	return 0;
}

