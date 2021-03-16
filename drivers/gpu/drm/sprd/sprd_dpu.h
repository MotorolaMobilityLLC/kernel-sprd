/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_DPU_H_
#define _SPRD_DPU_H_

#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <video/videomode.h>

#include <uapi/drm/drm_mode.h>

#include "sprd_crtc.h"

struct sprd_dpu_ops {
	const struct sprd_crtc_core_ops *core;
	const struct sprd_crtc_clk_ops *clk;
	const struct sprd_crtc_glb_ops *glb;
};

struct sprd_dpu {
	struct device dev;
	struct sprd_crtc *crtc;
	struct sprd_crtc_context ctx;
	const struct sprd_crtc_core_ops *core;
	const struct sprd_crtc_clk_ops *clk;
	const struct sprd_crtc_glb_ops *glb;
	struct drm_display_mode *mode;
};

void sprd_dpu_run(struct sprd_dpu *dpu);
void sprd_dpu_stop(struct sprd_dpu *dpu);

extern const struct sprd_crtc_core_ops dpu_lite_r1p0_core_ops;
extern const struct sprd_crtc_clk_ops pike2_dpu_clk_ops;
extern const struct sprd_crtc_glb_ops pike2_dpu_glb_ops;

extern const struct sprd_crtc_clk_ops sharkle_dpu_clk_ops;
extern const struct sprd_crtc_glb_ops sharkle_dpu_glb_ops;

extern const struct sprd_crtc_core_ops dpu_r2p0_core_ops;
extern const struct sprd_crtc_clk_ops sharkl3_dpu_clk_ops;
extern const struct sprd_crtc_glb_ops sharkl3_dpu_glb_ops;

extern const struct sprd_crtc_core_ops dpu_r4p0_core_ops;
extern const struct sprd_crtc_clk_ops sharkl5pro_dpu_clk_ops;
extern const struct sprd_crtc_glb_ops sharkl5pro_dpu_glb_ops;

#endif /* _SPRD_DPU_H_ */
