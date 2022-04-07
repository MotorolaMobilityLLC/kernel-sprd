/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_DPU1_H_
#define _SPRD_DPU1_H_

#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <video/videomode.h>

#include <uapi/drm/drm_mode.h>

#include "sprd_crtc.h"
#include "sprd_plane.h"
#include "disp_lib.h"
#include "disp_trusty.h"

#define DRM_FORMAT_P010		fourcc_code('P', '0', '1', '0')

enum {
	SPRD_DPU_IF_DBI = 0,
	SPRD_DPU_IF_DPI,
	SPRD_DPU_IF_EDPI,
	SPRD_DPU_IF_LIMIT
};

struct dpu_context;

struct dpu_core_ops {
	void (*version)(struct dpu_context *ctx);
	int (*init)(struct dpu_context *ctx);
	void (*fini)(struct dpu_context *ctx);
	void (*run)(struct dpu_context *ctx);
	void (*stop)(struct dpu_context *ctx);
	void (*disable_vsync)(struct dpu_context *ctx);
	void (*enable_vsync)(struct dpu_context *ctx);
	u32 (*isr)(struct dpu_context *ctx);
	void (*write_back)(struct dpu_context *ctx, u8 count, bool debug);
	void (*ifconfig)(struct dpu_context *ctx);
	void (*flip)(struct dpu_context *ctx,
		     struct sprd_plane planes[], u8 count);
	void (*capability)(struct dpu_context *ctx,
			 struct sprd_crtc_capability *cap);
	void (*bg_color)(struct dpu_context *ctx, u32 color);
	int (*context_init)(struct dpu_context *ctx, struct device_node *np);
	int (*modeset)(struct dpu_context *ctx, struct drm_display_mode *mode);
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

struct dpu_qos_cfg {
	u8 arqos_low;
	u8 arqos_high;
	u8 awqos_low;
	u8 awqos_high;
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
	bool flip_pending;
	wait_queue_head_t wait_queue;
	bool evt_update;
	bool evt_all_update;
	bool evt_stop;
	irqreturn_t (*dpu_isr)(int irq, void *data);

	/* write back parameters */
	int wb_en;
	int wb_xfbc_en;
	int max_vsync_count;
	int vsync_count;
	struct sprd_layer_state wb_layer;
	struct work_struct wb_work;
	dma_addr_t wb_addr_p;
	void *wb_addr_v;
	size_t wb_buf_size;
	bool wb_configed;

	/* te check parameters */
	wait_queue_head_t te_wq;
	bool te_check_en;
	bool evt_te;

	/* corner config parameters */
	u32 corner_size;
	int sprd_corner_radius;
	bool sprd_corner_support;

	unsigned int *layer_top;
	unsigned int *layer_bottom;

	/* other specific parameters */
	bool panel_ready;
	u32 prev_y2r_coef;
	u64 frame_count;
	int time;

	bool bypass_mode;
	u32 hdr_static_metadata[9];
	bool static_metadata_changed;

	/* qos config parameters */
	struct dpu_qos_cfg qos_cfg;
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

void sprd_dpu1_run(struct sprd_dpu *dpu);
void sprd_dpu1_stop(struct sprd_dpu *dpu);

extern const struct dpu_core_ops dpu_lite_r3p0_core_ops;
extern const struct dpu_clk_ops qogirn6pro_dpu1_clk_ops;
extern const struct dpu_glb_ops qogirn6pro_dpu1_glb_ops;


#endif /* _SPRD_DPU_H_ */
