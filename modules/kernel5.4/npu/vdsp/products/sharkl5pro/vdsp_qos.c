/*
 * SPDX-FileCopyrightText: 2019-2022 Unisoc (Shanghai) Technologies Co., Ltd
 * SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
 *
 * Copyright 2019-2022 Unisoc (Shanghai) Technologies Co., Ltd.
 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */

#include <linux/of_address.h>
#include <linux/regmap.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include "vdsp_hw.h"
#include "vdsp_qos.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: qos %d: %d %s:" \
	fmt, current->pid, __LINE__, __func__

void parse_qos(void *hw_arg, void *of_node)
{
	struct device_node *qos_node = NULL;
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	if ((NULL == hw_arg) || (NULL == of_node)) {
		pr_err("hw_arg:%lx , of_node:%lx\n", (unsigned long)hw_arg, (unsigned long)of_node);
		return;
	}
	qos_node = of_parse_phandle(of_node, "vdsp-qos", 0);
	if (qos_node) {
		if (of_property_read_u8(qos_node, "arqos-vdsp-msti", &hw->qos.ar_qos_vdsp_msti))
			hw->qos.ar_qos_vdsp_msti = 6;

		if (of_property_read_u8(qos_node, "awqos-vdsp-mstd", &hw->qos.aw_qos_vdsp_mstd))
			hw->qos.aw_qos_vdsp_mstd = 6;

		if (of_property_read_u8(qos_node, "arqos-vdsp-mstd", &hw->qos.ar_qos_vdsp_mstd))
			hw->qos.ar_qos_vdsp_mstd = 6;

		if (of_property_read_u8(qos_node, "arqos-vdsp-idma", &hw->qos.ar_qos_vdsp_idma))
			hw->qos.ar_qos_vdsp_idma = 1;

		if (of_property_read_u8(qos_node, "awqos-vdsp-idma", &hw->qos.aw_qos_vdsp_idma))
			hw->qos.aw_qos_vdsp_idma = 1;

		if (of_property_read_u8(qos_node, "arqos-vdma", &hw->qos.ar_qos_vdma))
			hw->qos.ar_qos_vdma = 1;

		if (of_property_read_u8(qos_node, "awqos-vdma", &hw->qos.aw_qos_vdma))
			hw->qos.aw_qos_vdma = 1;

		if (of_property_read_u8(qos_node, "arqos-threshold", &hw->qos.ar_qos_threshold))
			hw->qos.ar_qos_threshold = 0x0f;

		if (of_property_read_u8(qos_node, "awqos-threshold", &hw->qos.aw_qos_threshold))
			hw->qos.aw_qos_threshold = 0x0f;

	} else {
		hw->qos.ar_qos_vdsp_msti = 6;
		hw->qos.ar_qos_vdsp_mstd = 6;
		hw->qos.aw_qos_vdsp_mstd = 6;
		hw->qos.ar_qos_vdsp_idma = 1;
		hw->qos.aw_qos_vdsp_idma = 1;
		hw->qos.ar_qos_vdma = 1;
		hw->qos.aw_qos_vdma = 1;
		hw->qos.ar_qos_threshold = 0x0f;
		hw->qos.aw_qos_threshold = 0x0f;
	}
	return;
}

int set_qos(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	pr_debug("set qos\n");
	/*set qos threshold */
	vdsp_regmap_update_bits(hw->ahb_regmap, REG_QOS_THRESHOLD, (0xf << 28 | 0xf << 24),
		((hw->qos.ar_qos_threshold << 28) | (hw->qos.aw_qos_threshold << 24)), RT_APAHB);
	/*set qos 3 */
	vdsp_regmap_update_bits(hw->ahb_regmap, REG_QOS_3, 0xf0ffffff,
		((hw->qos.ar_qos_vdsp_msti << 28)
		| (hw->qos.ar_qos_vdsp_mstd << 20)
		| (hw->qos.aw_qos_vdsp_mstd << 16)
		| (hw->qos.ar_qos_vdsp_idma << 12)
		| (hw->qos.aw_qos_vdsp_idma << 8)
		| (hw->qos.ar_qos_vdma << 4)
		| (hw->qos.aw_qos_vdma)), RT_APAHB);

	/*set qos sel 3 */
	vdsp_regmap_update_bits(hw->ahb_regmap, REG_QOS_SEL3, 0x7f, 0x7f, RT_APAHB);

	return 0;
}

