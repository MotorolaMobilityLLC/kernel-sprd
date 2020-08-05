/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_CRTC_H_
#define _SPRD_CRTC_H_

#include <video/videomode.h>

#include <drm/drm_crtc.h>
#include <drm/drm_encoder.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_plane.h>
#include <drm/drm_print.h>

#include "sprd_gem.h"

#define to_sprd_crtc(x)			container_of(x, struct sprd_crtc, base)

#define BIT_DPU_INT_DONE_		BIT(0)
#define BIT_DPU_INT_TE			BIT(1)
#define BIT_DPU_INT_ERR			BIT(2)
#define BIT_DPU_INT_EDPI_TE		BIT(3)
#define BIT_DPU_INT_UPDATE_DONE		BIT(4)
#define BIT_DPU_INT_VSYNC		BIT(5)
#define BIT_DPU_INT_WB_DONE		BIT(6)
#define BIT_DPU_INT_WB_ERR		BIT(7)

enum sprd_crtc_output_type {
	SPRD_DISPLAY_TYPE_NONE,
	/* RGB or CPU Interface */
	SPRD_DISPLAY_TYPE_LCD,
	/* DisplayPort Interface */
	SPRD_DISPLAY_TYPE_DP,
	/* HDMI Interface */
	SPRD_DISPLAY_TYPE_HDMI,
};

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

struct sprd_crtc_capability {
	u32 max_layers;
	const u32 *fmts_ptr;
	u32 fmts_cnt;
};

struct sprd_crtc_layer {
	u8 index;
	u8 planes;
	u32 addr[4];
	u32 pitch[4];
	s16 src_x;
	s16 src_y;
	s16 src_w;
	s16 src_h;
	s16 dst_x;
	s16 dst_y;
	u16 dst_w;
	u16 dst_h;
	u32 format;
	u32 alpha;
	u32 blending;
	u32 rotation;
	u32 xfbc;
	u32 height;
	u32 header_size_r;
	u32 header_size_y;
	u32 header_size_uv;
	u32 y2r_coef;
	u8 pallete_en;
	u32 pallete_color;
};

struct sprd_crtc_context;

struct sprd_crtc_core_ops {
	int (*parse_dt)(struct sprd_crtc_context *ctx,
			struct device_node *np);
	void (*version)(struct sprd_crtc_context *ctx);
	int (*init)(struct sprd_crtc_context *ctx);
	void (*fini)(struct sprd_crtc_context *ctx);
	void (*run)(struct sprd_crtc_context *ctx);
	void (*stop)(struct sprd_crtc_context *ctx);
	void (*disable_vsync)(struct sprd_crtc_context *ctx);
	void (*enable_vsync)(struct sprd_crtc_context *ctx);
	u32 (*isr)(struct sprd_crtc_context *ctx);
	void (*ifconfig)(struct sprd_crtc_context *ctx);
	void (*flip)(struct sprd_crtc_context *ctx,
		     struct sprd_crtc_layer layers[], u8 count);
	void (*capability)(struct sprd_crtc_context *ctx,
			 struct sprd_crtc_capability *cap);
	void (*bg_color)(struct sprd_crtc_context *ctx, u32 color);
};

struct sprd_crtc_clk_ops {
	int (*parse_dt)(struct sprd_crtc_context *ctx,
			struct device_node *np);
	int (*init)(struct sprd_crtc_context *ctx);
	int (*uinit)(struct sprd_crtc_context *ctx);
	int (*enable)(struct sprd_crtc_context *ctx);
	int (*disable)(struct sprd_crtc_context *ctx);
	int (*update)(struct sprd_crtc_context *ctx, int clk_id, int val);
};

struct sprd_crtc_glb_ops {
	int (*parse_dt)(struct sprd_crtc_context *ctx,
			struct device_node *np);
	void (*enable)(struct sprd_crtc_context *ctx);
	void (*disable)(struct sprd_crtc_context *ctx);
	void (*reset)(struct sprd_crtc_context *ctx);
	void (*power)(struct sprd_crtc_context *ctx, int enable);
};

struct sprd_crtc_context {
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
};

struct sprd_crtc {
	struct drm_crtc base;
	enum sprd_crtc_output_type type;
	const struct sprd_crtc_ops *ops;
	struct sprd_crtc_context *ctx;
	struct sprd_crtc_layer *layers;
	u8 pending_planes;
	void *priv;
};

struct sprd_crtc_ops {
	void (*atomic_enable)(struct sprd_crtc *crtc);
	void (*atomic_disable)(struct sprd_crtc *crtc);
	int (*enable_vblank)(struct sprd_crtc *crtc);
	void (*disable_vblank)(struct sprd_crtc *crtc);
	enum drm_mode_status (*mode_valid)(struct sprd_crtc *crtc,
		const struct drm_display_mode *mode);
	void (*mode_set_nofb)(struct sprd_crtc *crtc);
	int (*atomic_check)(struct sprd_crtc *crtc,
			    struct drm_crtc_state *state);
	void (*atomic_begin)(struct sprd_crtc *crtc);
	void (*atomic_flush)(struct sprd_crtc *crtc);

	void (*prepare_fb)(struct sprd_crtc *crtc,
			  struct drm_plane_state *new_state);
	void (*cleanup_fb)(struct sprd_crtc *crtc,
			   struct drm_plane_state *old_state);
	void (*atomic_update)(struct sprd_crtc *crtc,
			     struct drm_plane *plane);
};

int sprd_crtc_iommu_map(struct device *dev, struct sprd_gem_obj *sprd_gem);
void sprd_crtc_iommu_unmap(struct device *dev, struct sprd_gem_obj *sprd_gem);
void sprd_crtc_wait_last_commit_complete(struct drm_crtc *crtc);
struct sprd_crtc *sprd_crtc_init(struct drm_device *drm,
					struct drm_plane *plane,
					enum sprd_crtc_output_type type,
					const struct sprd_crtc_ops *ops,
					struct sprd_crtc_context *ctx,
					void *priv);
int sprd_drm_set_possible_crtcs(struct drm_encoder *encoder,
		enum sprd_crtc_output_type out_type);

#endif /* _SPRD_CRTC_H_ */
