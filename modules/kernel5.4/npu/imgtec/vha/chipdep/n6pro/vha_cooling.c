// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Unisoc Inc.
 */

#include <linux/devfreq.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include "vha_cooling.h"

#ifdef SPRD_NPU_COOLING
#include <linux/sprd_npu_cooling.h>
#endif /*CONFIG_SPRD_NPU_COOLING_DEVICE*/

#ifdef UNISOC_NPU_COOLING
#include <linux/unisoc_npu_cooling.h>
#endif /*CONFIG_UNISOC_NPU_COOLING_DEVICE*/

static bool is_cooling = false;

void vha_cooling_register(struct devfreq *vha_devfreq)
{
#if defined SPRD_NPU_COOLING || defined UNISOC_NPU_COOLING
	if (npu_cooling_device_register(vha_devfreq)) {
		dev_warn(&vha_devfreq->dev, "Failed to register npu cooling device\n");
		return;
	}
	is_cooling = true;
#endif
}

void vha_cooling_unregister(void)
{
#if defined SPRD_NPU_COOLING || defined UNISOC_NPU_COOLING
	if (is_cooling) {
		npu_cooling_device_unregister();
		is_cooling = false;
	}
#endif
}

