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

enum {
	SPRD_DPU_IF_DBI = 0,
	SPRD_DPU_IF_DPI,
	SPRD_DPU_IF_EDPI,
	SPRD_DPU_IF_LIMIT
};

enum {
	ENHANCE_CFG_ID_ENABLE,
	ENHANCE_CFG_ID_DISABLE,
	ENHANCE_CFG_ID_SCL,
	ENHANCE_CFG_ID_EPF,
	ENHANCE_CFG_ID_HSV,
	ENHANCE_CFG_ID_CM,
	ENHANCE_CFG_ID_SLP,
	ENHANCE_CFG_ID_GAMMA,
	ENHANCE_CFG_ID_LTM,
	ENHANCE_CFG_ID_CABC,
	ENHANCE_CFG_ID_SLP_LUT,
	ENHANCE_CFG_ID_LUT3D,
	ENHANCE_CFG_ID_SR_EPF,
	ENHANCE_CFG_ID_MAX
};

struct dpu_context;

struct dpu_core_ops {
	int (*parse_dt)(struct dpu_context *ctx,
			struct device_node *np);
	void (*version)(struct dpu_context *ctx);
	int (*init)(struct dpu_context *ctx);
	void (*fini)(struct dpu_context *ctx);
	void (*run)(struct dpu_context *ctx);
	void (*stop)(struct dpu_context *ctx);
	void (*disable_vsync)(struct dpu_context *ctx);
	void (*enable_vsync)(struct dpu_context *ctx);
	u32 (*isr)(struct dpu_context *ctx);
	void (*ifconfig)(struct dpu_context *ctx);
	void (*flip)(struct dpu_context *ctx,
		     struct sprd_crtc_layer layers[], u8 count);
	void (*capability)(struct dpu_context *ctx,
			 struct sprd_crtc_capability *cap);
	void (*bg_color)(struct dpu_context *ctx, u32 color);
	int (*enhance_init)(struct dpu_context *ctx);
	void (*enhance_set)(struct dpu_context *ctx, u32 id, void *param);
	void (*enhance_get)(struct dpu_context *ctx, u32 id, void *param);
};

struct dpu_clk_ops {
	int (*parse_dt)(struct dpu_context *ctx,
			struct device_node *np);
	int (*init)(struct dpu_context *ctx);
	int (*uinit)(struct dpu_context *ctx);
	int (*enable)(struct dpu_context *ctx);
	int (*disable)(struct dpu_context *ctx);
	int (*update)(struct dpu_context *ctx, int clk_id, int val);
};

struct dpu_glb_ops {
	int (*parse_dt)(struct dpu_context *ctx,
			struct device_node *np);
	void (*enable)(struct dpu_context *ctx);
	void (*disable)(struct dpu_context *ctx);
	void (*reset)(struct dpu_context *ctx);
	void (*power)(struct dpu_context *ctx, int enable);
};

struct dpu_context {
	/* dpu common parameters */
	void __iomem *base;
	u32 base_offset[2];
	const char *version;
	int irq;
	u8 if_type;
	struct videomode vm;
	struct semaphore lock;
	bool enabled;
	bool stopped;
	wait_queue_head_t wait_queue;
	bool evt_update;
	bool evt_stop;
	irqreturn_t (*dpu_isr)(int irq, void *data);

	/* pq enhance parameters */
	void *enhance;
	int corner_radius;

	/* write back parameters */
	int wb_en;
	int wb_xfbc_en;

	/* other specific parameters */
	bool panel_ready;
};

struct sprd_dpu_ops {
	const struct dpu_core_ops *core;
	const struct dpu_clk_ops *clk;
	const struct dpu_glb_ops *glb;
};

struct sprd_dpu {
	struct device dev;
	struct sprd_crtc *crtc;
	struct dpu_context ctx;
	const struct dpu_core_ops *core;
	const struct dpu_clk_ops *clk;
	const struct dpu_glb_ops *glb;
	struct drm_display_mode *mode;
};

void sprd_dpu_run(struct sprd_dpu *dpu);
void sprd_dpu_stop(struct sprd_dpu *dpu);

extern const struct dpu_clk_ops sharkle_dpu_clk_ops;
extern const struct dpu_glb_ops sharkle_dpu_glb_ops;

extern const struct dpu_core_ops dpu_lite_r1p0_core_ops;
extern const struct dpu_clk_ops pike2_dpu_clk_ops;
extern const struct dpu_glb_ops pike2_dpu_glb_ops;

extern const struct dpu_core_ops dpu_r2p0_core_ops;
extern const struct dpu_clk_ops sharkl3_dpu_clk_ops;
extern const struct dpu_glb_ops sharkl3_dpu_glb_ops;

extern const struct dpu_core_ops dpu_r4p0_core_ops;
extern const struct dpu_clk_ops sharkl5pro_dpu_clk_ops;
extern const struct dpu_glb_ops sharkl5pro_dpu_glb_ops;

#endif /* _SPRD_DPU_H_ */
