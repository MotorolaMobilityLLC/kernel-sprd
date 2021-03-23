// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#include "dpu_enhance_param.h"
#include "dpu_r4p0_corner_param.h"
#include "sprd_crtc.h"
#include "sprd_dpu.h"

#define XFBC8888_HEADER_SIZE(w, h) (ALIGN((ALIGN((w), 16)) * \
				(ALIGN((h), 16)) / 16, 128))
#define XFBC8888_PAYLOAD_SIZE(w, h) (ALIGN((w), 16) * ALIGN((h), 16) * 4)
#define XFBC8888_BUFFER_SIZE(w, h) (XFBC8888_HEADER_SIZE(w, h) \
				+ XFBC8888_PAYLOAD_SIZE(w, h))

#define SLP_BRIGHTNESS_THRESHOLD 0x20

/* DPU registers size, 4 Bytes(32 Bits) */
#define DPU_REG_SIZE	0x04
/* Layer registers offset */
#define DPU_LAY_REG_OFFSET	0x0C

#define DPU_REG_RD(reg) readl_relaxed(reg)

#define DPU_REG_WR(reg, mask) writel_relaxed(mask, reg)

#define DPU_REG_SET(reg, mask) \
	writel_relaxed(readl_relaxed(reg) | mask, reg)

#define DPU_REG_CLR(reg, mask) \
	writel_relaxed(readl_relaxed(reg) & ~mask, reg)

#define DPU_LAY_REG(reg, index) \
	(reg + index * DPU_LAY_REG_OFFSET * DPU_REG_SIZE)

#define DPU_LAY_PLANE_ADDR(reg, index, plane) \
	(reg + index * DPU_LAY_REG_OFFSET * DPU_REG_SIZE + plane * DPU_REG_SIZE)

/* Global control registers */
#define REG_DPU_CTRL	0x04
#define REG_DPU_CFG0	0x08
#define REG_DPU_CFG1	0x0C
#define REG_DPU_CFG2	0x10
#define REG_PANEL_SIZE	0x20
#define REG_BLEND_SIZE	0x24
#define REG_BG_COLOR	0x2C

/* Layer0 control registers */
#define REG_LAY_BASE_ADDR	0x30
#define REG_LAY_CTRL		0x40
#define REG_LAY_SIZE		0x44
#define REG_LAY_PITCH		0x48
#define REG_LAY_POS		0x4C
#define REG_LAY_ALPHA		0x50
#define REG_LAY_PALLETE		0x58
#define REG_LAY_CROP_START	0x5C

/* Write back control registers */
#define REG_WB_CTRL		0x1B4
#define REG_WB_CFG		0x1B8
#define REG_WB_PITCH		0x1BC

/* Interrupt control registers */
#define REG_DPU_INT_EN		0x1E0
#define REG_DPU_INT_CLR		0x1E4
#define REG_DPU_INT_STS		0x1E8

/* DPI control registers */
#define REG_DPI_CTRL		0x1F0
#define REG_DPI_H_TIMING	0x1F4
#define REG_DPI_V_TIMING	0x1F8

/* PQ Enhance config registers */
#define REG_DPU_ENHANCE_CFG	0x200
#define REG_EPF_EPSILON		0x210
#define REG_EPF_GAIN0_3		0x214
#define REG_EPF_GAIN4_7		0x218
#define REG_EPF_DIFF		0x21C
#define REG_HSV_LUT_ADDR	0x240
#define REG_HSV_LUT_WDATA	0x244
#define REG_HSV_LUT_RDATA	0x248
#define REG_CM_COEF01_00	0x280
#define REG_CM_COEF03_02	0x284
#define REG_CM_COEF11_10	0x288
#define REG_CM_COEF13_12	0x28C
#define REG_CM_COEF21_20	0x290
#define REG_CM_COEF23_22	0x294
#define REG_SLP_CFG0		0x2C0
#define REG_SLP_CFG1		0x2C4
#define REG_SLP_CFG2		0x2C8
#define REG_SLP_CFG3		0x2CC
#define REG_SLP_LUT_ADDR	0x2D0
#define REG_SLP_LUT_WDATA	0x2D4
#define REG_SLP_LUT_RDATA	0x2D8
#define REG_TREED_LUT_ADDR	0x2DC
#define REG_TREED_LUT_WDATA	0x2E0
#define REG_TREED_LUT_RDATA	0x2E4
#define REG_GAMMA_LUT_ADDR	0x300
#define REG_GAMMA_LUT_WDATA	0x304
#define REG_GAMMA_LUT_RDATA	0x308
#define REG_SLP_CFG4		0x3C0
#define REG_SLP_CFG5		0x3C4
#define REG_SLP_CFG6		0x3C8
#define REG_SLP_CFG7		0x3CC
#define REG_SLP_CFG8		0x3D0
#define REG_SLP_CFG9		0x3D4
#define REG_SLP_CFG10		0x3D8

/* Corner config registers */
#define REG_CORNER_CONFIG		0x500
#define REG_TOP_CORNER_LUT_ADDR		0x504
#define REG_TOP_CORNER_LUT_WDATA	0x508
#define REG_BOT_CORNER_LUT_ADDR		0x510
#define REG_BOT_CORNER_LUT_WDATA	0x518

/* Global control bits */
#define BIT_DPU_RUN			BIT(0)
#define BIT_DPU_STOP			BIT(1)
#define BIT_DPU_REG_UPDATE		BIT(2)
#define BIT_DPU_IF_EDPI			BIT(0)

/* Layer control bits */
#define BIT_DPU_LAY_EN				BIT(0)
#define BIT_DPU_LAY_LAYER_ALPHA			(0x01 << 2)
#define BIT_DPU_LAY_COMBO_ALPHA			(0x02 << 2)
#define BIT_DPU_LAY_FORMAT_YUV422_2PLANE		(0x00 << 4)
#define BIT_DPU_LAY_FORMAT_YUV420_2PLANE		(0x01 << 4)
#define BIT_DPU_LAY_FORMAT_YUV420_3PLANE		(0x02 << 4)
#define BIT_DPU_LAY_FORMAT_ARGB8888			(0x03 << 4)
#define BIT_DPU_LAY_FORMAT_RGB565			(0x04 << 4)
#define BIT_DPU_LAY_FORMAT_XFBC_ARGB8888		(0x08 << 4)
#define BIT_DPU_LAY_FORMAT_XFBC_RGB565			(0x09 << 4)
#define BIT_DPU_LAY_FORMAT_XFBC_YUV420			(0x0A << 4)
#define BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3		(0x00 << 8)
#define BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0		(0x01 << 8)
#define BIT_DPU_LAY_DATA_ENDIAN_B2B3B0B1		(0x02 << 8)
#define BIT_DPU_LAY_DATA_ENDIAN_B1B0B3B2		(0x03 << 8)
#define BIT_DPU_LAY_NO_SWITCH			(0x00 << 10)
#define BIT_DPU_LAY_RGB888_RB_SWITCH		(0x01 << 10)
#define BIT_DPU_LAY_RGB565_RB_SWITCH		(0x01 << 12)
#define BIT_DPU_LAY_PALLETE_EN			(0x01 << 13)
#define BIT_DPU_LAY_MODE_BLEND_NORMAL		(0x00 << 16)
#define BIT_DPU_LAY_MODE_BLEND_PREMULT		(0x01 << 16)

/* Interrupt control & status bits */
#define BIT_DPU_INT_DONE		BIT(0)
#define BIT_DPU_INT_TE			BIT(1)
#define BIT_DPU_INT_ERR			BIT(2)
#define BIT_DPU_INT_UPDATE_DONE		BIT(4)
#define BIT_DPU_INT_VSYNC		BIT(5)
#define BIT_DPU_INT_WB_DONE		BIT(6)
#define BIT_DPU_INT_WB_ERR		BIT(7)
#define BIT_DPU_INT_FBC_PLD_ERR		BIT(8)
#define BIT_DPU_INT_FBC_HDR_ERR		BIT(9)

/* DPI control bits */
#define BIT_DPU_EDPI_TE_EN		BIT(8)
#define BIT_DPU_EDPI_FROM_EXTERNAL_PAD	BIT(10)
#define BIT_DPU_DPI_HALT_EN		BIT(16)

/* Corner config bits */
#define BIT_TOP_CORNER_EN		BIT(0)
#define BIT_BOT_CORNER_EN		BIT(16)

struct dpu_cfg1 {
	u8 arqos_low;
	u8 arqos_high;
	u8 awqos_low;
	u8 awqos_high;
};

struct scale_cfg {
	u32 in_w;
	u32 in_h;
};

struct hsv_entry {
	u16 hue;
	u16 sat;
};

struct hsv_lut {
	struct hsv_entry table[360];
};

struct cm_cfg {
	u16 coef00;
	u16 coef01;
	u16 coef02;
	u16 coef03;
	u16 coef10;
	u16 coef11;
	u16 coef12;
	u16 coef13;
	u16 coef20;
	u16 coef21;
	u16 coef22;
	u16 coef23;
};

struct ltm_cfg {
	u16 limit_hclip;
	u16 limit_lclip;
	u16 limit_clip_step;
};

struct slp_cfg {
	u8 brightness;
	u16 brightness_step;
	u8 fst_max_bright_th;
	u8 fst_max_bright_th_step[5];
	u8 hist_exb_no;
	u8 hist_exb_percent;
	u16 mask_height;
	u8 fst_pth_index[4];
	u8 hist9_index[9];
	u8 glb_x[3];
	u16 glb_s[3];
	u8 fast_ambient_th;
	u8 scene_change_percent_th;
	u8 local_weight;
	u8 fst_pth;
};

struct gamma_lut {
	u16 r[256];
	u16 g[256];
	u16 b[256];
};

struct epf_cfg {
	u16 epsilon0;
	u16 epsilon1;
	u8 gain0;
	u8 gain1;
	u8 gain2;
	u8 gain3;
	u8 gain4;
	u8 gain5;
	u8 gain6;
	u8 gain7;
	u8 max_diff;
	u8 min_diff;
};

struct threed_lut {
	u32 value[729];
};

struct dpu_enhance {
	int enhance_en;
	bool sr_epf_ready;

	struct scale_cfg scale_copy;
	struct hsv_lut hsv_copy;
	struct cm_cfg cm_copy;
	struct ltm_cfg ltm_copy;
	struct slp_cfg slp_copy;
	struct gamma_lut gamma_copy;
	struct epf_cfg epf_copy;
	struct threed_lut lut3d_copy;
	struct epf_cfg sr_epf;
};

static struct epf_cfg epf = {
	.epsilon0 = 30,
	.epsilon1 = 1000,
	.gain0 = -8,
	.gain1 = 8,
	.gain2 = 32,
	.gain3 = 160,
	.gain4 = 24,
	.gain5 = 8,
	.gain6 = 32,
	.gain7 = 160,
	.max_diff = 80,
	.min_diff = 40,
};

static struct dpu_cfg1 qos_cfg = {
	.arqos_low = 0x1,
	.arqos_high = 0x7,
	.awqos_low = 0x1,
	.awqos_high = 0x7,
};

static void dpu_clean_all(struct dpu_context *ctx);
static void dpu_layer(struct dpu_context *ctx,
		    struct sprd_crtc_layer *hwlayer);
static void dpu_enhance_reload(struct dpu_context *ctx);

static void dpu_version(struct dpu_context *ctx)
{
	ctx->version = "dpu-r4p0";
}

static int dpu_parse_dt(struct dpu_context *ctx,
				struct device_node *np)
{
	int ret;
	struct device_node *qos_np;

	ret = of_property_read_u32(np, "sprd,corner-radius",
					&ctx->corner_radius);
	if (!ret)
		pr_info("round corner support, radius = %d.\n",
					ctx->corner_radius);

	qos_np = of_parse_phandle(np, "sprd,qos", 0);
	if (!qos_np)
		pr_warn("can't find dpu qos cfg node\n");

	ret = of_property_read_u8(qos_np, "arqos-low",
					&qos_cfg.arqos_low);
	if (ret)
		pr_warn("read arqos-low failed, use default\n");

	ret = of_property_read_u8(qos_np, "arqos-high",
					&qos_cfg.arqos_high);
	if (ret)
		pr_warn("read arqos-high failed, use default\n");

	ret = of_property_read_u8(qos_np, "awqos-low",
					&qos_cfg.awqos_low);
	if (ret)
		pr_warn("read awqos_low failed, use default\n");

	ret = of_property_read_u8(qos_np, "awqos-high",
					&qos_cfg.awqos_high);
	if (ret)
		pr_warn("read awqos-high failed, use default\n");

	return ret;
}

static void dpu_corner_init(struct dpu_context *ctx)
{
	int corner_radius = ctx->corner_radius;
	int i;

	DPU_REG_SET(ctx->base + REG_CORNER_CONFIG,
			corner_radius << 24 | corner_radius << 8);

	for (i = 0; i < corner_radius; i++) {
		DPU_REG_WR(ctx->base + REG_TOP_CORNER_LUT_ADDR, i);
		DPU_REG_WR(ctx->base + REG_TOP_CORNER_LUT_WDATA,
				corner_param[corner_radius][i]);
		DPU_REG_WR(ctx->base + REG_BOT_CORNER_LUT_ADDR, i);
		DPU_REG_WR(ctx->base + REG_BOT_CORNER_LUT_WDATA,
				corner_param[corner_radius][corner_radius - i - 1]);
	}

	DPU_REG_SET(ctx->base + REG_CORNER_CONFIG,
			BIT_TOP_CORNER_EN | BIT_BOT_CORNER_EN);
}

static u32 dpu_isr(struct dpu_context *ctx)
{
	u32 reg_val, int_mask = 0;

	reg_val = DPU_REG_RD(ctx->base + REG_DPU_INT_STS);

	/* disable err interrupt */
	if (reg_val & BIT_DPU_INT_ERR)
		int_mask |= BIT_DPU_INT_ERR;

	/* dpu update done isr */
	if (reg_val & BIT_DPU_INT_UPDATE_DONE) {
		ctx->evt_update = true;
		wake_up_interruptible_all(&ctx->wait_queue);
	}

	/* dpu stop done isr */
	if (reg_val & BIT_DPU_INT_DONE) {
		ctx->evt_stop = true;
		wake_up_interruptible_all(&ctx->wait_queue);
	}

	/* dpu afbc payload error isr */
	if (reg_val & BIT_DPU_INT_FBC_PLD_ERR) {
		int_mask |= BIT_DPU_INT_FBC_PLD_ERR;
		pr_err("dpu afbc payload error\n");
	}

	/* dpu afbc header error isr */
	if (reg_val & BIT_DPU_INT_FBC_HDR_ERR) {
		int_mask |= BIT_DPU_INT_FBC_HDR_ERR;
		pr_err("dpu afbc header error\n");
	}

	DPU_REG_WR(ctx->base + REG_DPU_INT_CLR, reg_val);
	DPU_REG_CLR(ctx->base + REG_DPU_INT_EN, int_mask);

	return reg_val;
}

static int dpu_wait_stop_done(struct dpu_context *ctx)
{
	int rc;

	if (ctx->stopped)
		return 0;

	rc = wait_event_interruptible_timeout(ctx->wait_queue, ctx->evt_stop,
						msecs_to_jiffies(500));
	ctx->evt_stop = false;

	ctx->stopped = true;

	if (!rc) {
		pr_err("dpu wait for stop done time out!\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int dpu_wait_update_done(struct dpu_context *ctx)
{
	int rc;

	ctx->evt_update = false;

	rc = wait_event_interruptible_timeout(ctx->wait_queue, ctx->evt_update,
						msecs_to_jiffies(500));

	if (!rc) {
		pr_err("dpu wait for reg update done time out!\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static void dpu_stop(struct dpu_context *ctx)
{
	if (ctx->if_type == SPRD_DPU_IF_DPI)
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_STOP);

	dpu_wait_stop_done(ctx);

	pr_info("dpu stop\n");
}

static void dpu_run(struct dpu_context *ctx)
{
	DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_RUN);

	ctx->stopped = false;

	pr_info("dpu run\n");

	if (ctx->if_type == SPRD_DPU_IF_EDPI) {
		/*
		 * If the panel read GRAM speed faster than
		 * DSI write GRAM speed, it will display some
		 * mass on screen when backlight on. So wait
		 * a TE period after flush the GRAM.
		 */
		if (!ctx->panel_ready) {
			dpu_wait_stop_done(ctx);
			/* wait for TE again */
			mdelay(20);
			ctx->panel_ready = true;
		}
	}
}

static int dpu_init(struct dpu_context *ctx)
{
	u32 reg_val, size;

	DPU_REG_WR(ctx->base + REG_BG_COLOR, 0x00);

	size = (ctx->vm.vactive << 16) | ctx->vm.hactive;

	DPU_REG_WR(ctx->base + REG_PANEL_SIZE, size);
	DPU_REG_WR(ctx->base + REG_BLEND_SIZE, size);

	DPU_REG_WR(ctx->base + REG_DPU_CFG0, 0x00);
	reg_val = (qos_cfg.awqos_high << 12) |
		(qos_cfg.awqos_low << 8) |
		(qos_cfg.arqos_high << 4) |
		(qos_cfg.arqos_low) | BIT(18) | BIT(22);
	DPU_REG_WR(ctx->base + REG_DPU_CFG1, reg_val);
	DPU_REG_WR(ctx->base + REG_DPU_CFG2, 0x14002);

	if (ctx->stopped)
		dpu_clean_all(ctx);

	DPU_REG_WR(ctx->base + REG_DPU_INT_CLR, 0xffff);

	dpu_enhance_reload(ctx);

	if (ctx->corner_radius)
		dpu_corner_init(ctx);

	return 0;
}

static void dpu_fini(struct dpu_context *ctx)
{
	DPU_REG_WR(ctx->base + REG_DPU_INT_EN, 0x00);
	DPU_REG_WR(ctx->base + REG_DPU_INT_CLR, 0xff);

	ctx->panel_ready = false;
}

enum {
	DPU_LAYER_FORMAT_YUV422_2PLANE,
	DPU_LAYER_FORMAT_YUV420_2PLANE,
	DPU_LAYER_FORMAT_YUV420_3PLANE,
	DPU_LAYER_FORMAT_ARGB8888,
	DPU_LAYER_FORMAT_RGB565,
	DPU_LAYER_FORMAT_XFBC_ARGB8888 = 8,
	DPU_LAYER_FORMAT_XFBC_RGB565,
	DPU_LAYER_FORMAT_XFBC_YUV420,
	DPU_LAYER_FORMAT_MAX_TYPES,
};

enum {
	DPU_LAYER_ROTATION_0,
	DPU_LAYER_ROTATION_90,
	DPU_LAYER_ROTATION_180,
	DPU_LAYER_ROTATION_270,
	DPU_LAYER_ROTATION_0_M,
	DPU_LAYER_ROTATION_90_M,
	DPU_LAYER_ROTATION_180_M,
	DPU_LAYER_ROTATION_270_M,
};

static u32 to_dpu_rotation(u32 angle)
{
	u32 rot = DPU_LAYER_ROTATION_0;

	switch (angle) {
	case 0:
	case DRM_MODE_ROTATE_0:
		rot = DPU_LAYER_ROTATION_0;
		break;
	case DRM_MODE_ROTATE_90:
		rot = DPU_LAYER_ROTATION_90;
		break;
	case DRM_MODE_ROTATE_180:
		rot = DPU_LAYER_ROTATION_180;
		break;
	case DRM_MODE_ROTATE_270:
		rot = DPU_LAYER_ROTATION_270;
		break;
	case (DRM_MODE_REFLECT_Y | DRM_MODE_ROTATE_0):
		rot = DPU_LAYER_ROTATION_180_M;
		break;
	case (DRM_MODE_REFLECT_Y | DRM_MODE_ROTATE_90):
		rot = DPU_LAYER_ROTATION_90_M;
		break;
	case (DRM_MODE_REFLECT_X | DRM_MODE_ROTATE_0):
		rot = DPU_LAYER_ROTATION_0_M;
		break;
	case (DRM_MODE_REFLECT_X | DRM_MODE_ROTATE_90):
		rot = DPU_LAYER_ROTATION_270_M;
		break;
	default:
		pr_err("rotation convert unsupport angle (drm)= 0x%x\n", angle);
		break;
	}

	return rot;
}

static u32 dpu_img_ctrl(u32 format, u32 blending, u32 compression, u32 y2r_coef,
		u32 rotation)
{
	int reg_val = 0;

	/* layer enable */
	reg_val |= BIT_DPU_LAY_EN;

	switch (format) {
	case DRM_FORMAT_BGRA8888:
		/* BGRA8888 -> ARGB8888 */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0;
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= BIT_DPU_LAY_FORMAT_XFBC_ARGB8888;
		else
			reg_val |= BIT_DPU_LAY_FORMAT_ARGB8888;
		break;
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
		/* RGBA8888 -> ABGR8888 */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0;
		fallthrough;
	case DRM_FORMAT_ABGR8888:
		/* rb switch */
		reg_val |= BIT_DPU_LAY_RGB888_RB_SWITCH;
		fallthrough;
	case DRM_FORMAT_ARGB8888:
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= BIT_DPU_LAY_FORMAT_XFBC_ARGB8888;
		else
			reg_val |= BIT_DPU_LAY_FORMAT_ARGB8888;
		break;
	case DRM_FORMAT_XBGR8888:
		/* rb switch */
		reg_val |= BIT_DPU_LAY_RGB888_RB_SWITCH;
		fallthrough;
		/* FALLTHRU */
	case DRM_FORMAT_XRGB8888:
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= BIT_DPU_LAY_FORMAT_XFBC_ARGB8888;
		else
			reg_val |= BIT_DPU_LAY_FORMAT_ARGB8888;
		break;
	case DRM_FORMAT_BGR565:
		/* rb switch */
		reg_val |= BIT_DPU_LAY_RGB565_RB_SWITCH;
		fallthrough;
	case DRM_FORMAT_RGB565:
		if (compression)
			/* XFBC-RGB565 */
			reg_val |= BIT_DPU_LAY_FORMAT_XFBC_RGB565;
		else
			reg_val |= BIT_DPU_LAY_FORMAT_RGB565;
		break;
	case DRM_FORMAT_NV12:
		if (compression)
			/*2-Lane: Yuv420 */
			reg_val |= BIT_DPU_LAY_FORMAT_XFBC_YUV420;
		else
			reg_val |= BIT_DPU_LAY_FORMAT_YUV420_2PLANE;
		/*Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		/*UV endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		break;
	case DRM_FORMAT_NV21:
		if (compression)
			/*2-Lane: Yuv420 */
			reg_val |= BIT_DPU_LAY_FORMAT_XFBC_YUV420;
		else
			reg_val |= BIT_DPU_LAY_FORMAT_YUV420_2PLANE;
		/*Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		/*UV endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B1B0B3B2;
		break;
	case DRM_FORMAT_NV16:
		/*2-Lane: Yuv422 */
		reg_val |= BIT_DPU_LAY_FORMAT_YUV422_2PLANE;
		/*Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0;
		/*UV endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B1B0B3B2;
		break;
	case DRM_FORMAT_NV61:
		/*2-Lane: Yuv422 */
		reg_val |= BIT_DPU_LAY_FORMAT_YUV422_2PLANE;
		/*Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		/*UV endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		break;
	case DRM_FORMAT_YUV420:
		reg_val |= BIT_DPU_LAY_FORMAT_YUV420_3PLANE;
		/*Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		/*UV endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		break;
	default:
		pr_err("error: invalid format %c%c%c%c\n", format,
						format >> 8,
						format >> 16,
						format >> 24);
		break;
	}

	switch (blending) {
	case DRM_MODE_BLEND_PIXEL_NONE:
		/* don't do blending, maybe RGBX */
		/* alpha mode select - layer alpha */
		reg_val |= BIT_DPU_LAY_LAYER_ALPHA;
		break;
	case DRM_MODE_BLEND_COVERAGE:
		/* alpha mode select - combo alpha */
		reg_val |= BIT_DPU_LAY_COMBO_ALPHA;
		/* blending mode select - normal mode */
		reg_val |= BIT_DPU_LAY_MODE_BLEND_NORMAL;
		break;
	case DRM_MODE_BLEND_PREMULTI:
		/* alpha mode select - combo alpha */
		reg_val |= BIT_DPU_LAY_COMBO_ALPHA;
		/* blending mode select - pre-mult mode */
		reg_val |= BIT_DPU_LAY_MODE_BLEND_PREMULT;
		break;
	default:
		/* alpha mode select - layer alpha */
		reg_val |= BIT_DPU_LAY_LAYER_ALPHA;
		break;
	}

	reg_val |= y2r_coef << 28;
	rotation = to_dpu_rotation(rotation);
	reg_val |= (rotation & 0x7) << 20;

	return reg_val;
}

static void dpu_clean_all(struct dpu_context *ctx)
{
	int i;

	for (i = 0; i < 8; i++)
		DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_CTRL, i), 0x00);
}

static void dpu_bgcolor(struct dpu_context *ctx, u32 color)
{
	if (ctx->if_type == SPRD_DPU_IF_EDPI)
		dpu_wait_stop_done(ctx);

	DPU_REG_WR(ctx->base + REG_BG_COLOR, color);

	dpu_clean_all(ctx);

	if ((ctx->if_type == SPRD_DPU_IF_DPI) && !ctx->stopped) {
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_REG_UPDATE);
		dpu_wait_update_done(ctx);
	} else if (ctx->if_type == SPRD_DPU_IF_EDPI) {
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_RUN);
		ctx->stopped = false;
	}
}

static void dpu_layer(struct dpu_context *ctx,
		    struct sprd_crtc_layer *hwlayer)
{
	const struct drm_format_info *info;
	u32 size, offset, ctrl, reg_val, pitch;
	int i;

	offset = (hwlayer->dst_x & 0xffff) | ((hwlayer->dst_y) << 16);

	if (hwlayer->pallete_en) {
		size = (hwlayer->dst_w & 0xffff) | ((hwlayer->dst_h) << 16);
		DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_POS,
				hwlayer->index), offset);
		DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_SIZE,
				hwlayer->index), size);
		DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_ALPHA,
				hwlayer->index), hwlayer->alpha);
		DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_PALLETE,
				hwlayer->index), hwlayer->pallete_color);

		/* pallete layer enable */
		reg_val = BIT_DPU_LAY_EN |
			  BIT_DPU_LAY_LAYER_ALPHA |
			  BIT_DPU_LAY_PALLETE_EN;
		DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_CTRL,
				hwlayer->index), reg_val);

		pr_debug("dst_x = %d, dst_y = %d, dst_w = %d, dst_h = %d, pallete:%d\n",
			hwlayer->dst_x, hwlayer->dst_y,
			hwlayer->dst_w, hwlayer->dst_h, hwlayer->pallete_color);
		return;
	}

	if (hwlayer->src_w && hwlayer->src_h)
		size = (hwlayer->src_w & 0xffff) | ((hwlayer->src_h) << 16);
	else
		size = (hwlayer->dst_w & 0xffff) | ((hwlayer->dst_h) << 16);

	for (i = 0; i < hwlayer->planes; i++) {
		if (hwlayer->addr[i] % 16)
			pr_err("layer addr[%d] is not 16 bytes align, it's 0x%08x\n",
				i, hwlayer->addr[i]);
		DPU_REG_WR(ctx->base + DPU_LAY_PLANE_ADDR(REG_LAY_BASE_ADDR,
				hwlayer->index, i), hwlayer->addr[i]);
	}

	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_POS,
			hwlayer->index), offset);
	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_SIZE,
			hwlayer->index), size);
	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_CROP_START,
			hwlayer->index), hwlayer->src_y << 16 | hwlayer->src_x);
	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_ALPHA,
			hwlayer->index), hwlayer->alpha);

	info = drm_format_info(hwlayer->format);
	if (hwlayer->planes == 3) {
		/* UV pitch is 1/2 of Y pitch*/
		pitch = (hwlayer->pitch[0] / info->cpp[0]) |
				(hwlayer->pitch[0] / info->cpp[0] << 15);
		DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_PITCH,
				hwlayer->index), pitch);
	} else {
		pitch = hwlayer->pitch[0] / info->cpp[0];
		DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_PITCH,
				hwlayer->index), pitch);
	}

	ctrl = dpu_img_ctrl(hwlayer->format, hwlayer->blending,
		hwlayer->xfbc, hwlayer->y2r_coef, hwlayer->rotation);

	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_CTRL,
			hwlayer->index), ctrl);

	pr_debug("dst_x = %d, dst_y = %d, dst_w = %d, dst_h = %d\n",
				hwlayer->dst_x, hwlayer->dst_y,
				hwlayer->dst_w, hwlayer->dst_h);
	pr_debug("start_x = %d, start_y = %d, start_w = %d, start_h = %d\n",
				hwlayer->src_x, hwlayer->src_y,
				hwlayer->src_w, hwlayer->src_h);
}

static void dpu_flip(struct dpu_context *ctx,
		     struct sprd_crtc_layer layers[], u8 count)
{
	int i;
	u32 reg_val;

	/*
	 * Make sure the dpu is in stop status. DPU_R4P0 has no shadow
	 * registers in EDPI mode. So the config registers can only be
	 * updated in the rising edge of DPU_RUN bit.
	 */
	if (ctx->if_type == SPRD_DPU_IF_EDPI)
		dpu_wait_stop_done(ctx);

	/* reset the bgcolor to black */
	DPU_REG_WR(ctx->base + REG_BG_COLOR, 0x00);

	/* disable all the layers */
	dpu_clean_all(ctx);

	/* start configure dpu layers */
	for (i = 0; i < count; i++)
		dpu_layer(ctx, &layers[i]);

	/* update trigger and wait */
	if (ctx->if_type == SPRD_DPU_IF_DPI) {
		if (!ctx->stopped) {
			DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_REG_UPDATE);
			dpu_wait_update_done(ctx);
		}

		DPU_REG_SET(ctx->base + REG_DPU_INT_EN, BIT_DPU_INT_ERR);
	} else if (ctx->if_type == SPRD_DPU_IF_EDPI) {
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_RUN);

		ctx->stopped = false;
	}

	/*
	 * If the following interrupt was disabled in isr,
	 * re-enable it.
	 */
	reg_val = BIT_DPU_INT_FBC_PLD_ERR |
		BIT_DPU_INT_FBC_HDR_ERR;
	DPU_REG_SET(ctx->base + REG_DPU_INT_EN, reg_val);
}

static void dpu_dpi_init(struct dpu_context *ctx)
{
	u32 int_mask = 0;
	u32 reg_val;

	if (ctx->if_type == SPRD_DPU_IF_DPI) {
		/* use dpi as interface */
		DPU_REG_CLR(ctx->base + REG_DPU_CFG0, BIT_DPU_IF_EDPI);

		/* disable Halt function for SPRD DSI */
		DPU_REG_CLR(ctx->base + REG_DPI_CTRL, BIT_DPU_DPI_HALT_EN);


		/* set dpi timing */
		reg_val = ctx->vm.hsync_len << 0 |
			  ctx->vm.hback_porch << 8 |
			  ctx->vm.hfront_porch << 20;
		DPU_REG_WR(ctx->base + REG_DPI_H_TIMING, reg_val);

		reg_val = ctx->vm.vsync_len << 0 |
			  ctx->vm.vback_porch << 8 |
			  ctx->vm.vfront_porch << 20;
		DPU_REG_WR(ctx->base + REG_DPI_V_TIMING, reg_val);

		if (ctx->vm.vsync_len + ctx->vm.vback_porch < 32)
			pr_warn("Warning: (vsync + vbp) < 32, "
				"underflow risk!\n");

		/* enable dpu update done INT */
		int_mask |= BIT_DPU_INT_UPDATE_DONE;
		/* enable dpu DONE  INT */
		int_mask |= BIT_DPU_INT_DONE;
		/* enable dpu dpi vsync */
		int_mask |= BIT_DPU_INT_VSYNC;
		/* enable dpu TE INT */
		int_mask |= BIT_DPU_INT_TE;
		/* enable underflow err INT */
		int_mask |= BIT_DPU_INT_ERR;

	} else if (ctx->if_type == SPRD_DPU_IF_EDPI) {
		/* use edpi as interface */
		DPU_REG_SET(ctx->base + REG_DPU_CFG0, BIT_DPU_IF_EDPI);

		/* use external te */
		DPU_REG_SET(ctx->base + REG_DPI_CTRL, BIT_DPU_EDPI_FROM_EXTERNAL_PAD);

		/* enable te */
		DPU_REG_SET(ctx->base + REG_DPI_CTRL, BIT_DPU_EDPI_TE_EN);

		/* enable stop DONE INT */
		int_mask |= BIT_DPU_INT_DONE;
		/* enable TE INT */
		int_mask |= BIT_DPU_INT_TE;
	}

	/* enable ifbc payload error INT */
	int_mask |= BIT_DPU_INT_FBC_PLD_ERR;
	/* enable ifbc header error INT */
	int_mask |= BIT_DPU_INT_FBC_HDR_ERR;


	DPU_REG_WR(ctx->base + REG_DPU_INT_EN, int_mask);
}

static void enable_vsync(struct dpu_context *ctx)
{
	DPU_REG_SET(ctx->base + REG_DPU_INT_EN, BIT_DPU_INT_VSYNC);
}

static void disable_vsync(struct dpu_context *ctx)
{
	//DPU_REG_CLR(ctx->base + REG_DPU_INT_EN, BIT_DPU_INT_VSYNC);
}

static int dpu_enhance_init(struct dpu_context *ctx)
{
	struct dpu_enhance *enhance;

	enhance = kzalloc(sizeof(*enhance), GFP_KERNEL);
	if (!enhance)
		return -ENOMEM;

	ctx->enhance = enhance;

	return 0;
}

static void dpu_epf_set(struct dpu_context *ctx, struct epf_cfg *epf)
{
	DPU_REG_WR(ctx->base + REG_EPF_EPSILON, (epf->epsilon1 << 16) | epf->epsilon0);
	DPU_REG_WR(ctx->base + REG_EPF_GAIN0_3, (epf->gain3 << 24) | (epf->gain2 << 16) |
				(epf->gain1 << 8) | epf->gain0);
	DPU_REG_WR(ctx->base + REG_EPF_GAIN4_7, (epf->gain7 << 24) | (epf->gain6 << 16) |
				(epf->gain5 << 8) | epf->gain4);
	DPU_REG_WR(ctx->base + REG_EPF_DIFF, (epf->max_diff << 8) | epf->min_diff);
}

static void dpu_enhance_backup(struct dpu_context *ctx, u32 id, void *param)
{
	struct dpu_enhance *enhance = ctx->enhance;
	u32 *p;

	switch (id) {
	case ENHANCE_CFG_ID_ENABLE:
		p = param;
		enhance->enhance_en |= *p;
		pr_info("enhance module enable backup: 0x%x\n", *p);
		break;
	case ENHANCE_CFG_ID_DISABLE:
		p = param;
		if (*p & BIT(1)) {
			if ((enhance->enhance_en & BIT(0)) && enhance->sr_epf_ready) {
				*p &= ~BIT(1);
				pr_info("enhance backup epf shouldn't be disabled\n");
			}
		}
		enhance->enhance_en &= ~(*p);
		pr_info("enhance module disable backup: 0x%x\n", *p);
		break;
	case ENHANCE_CFG_ID_SCL:
		memcpy(&enhance->scale_copy, param, sizeof(enhance->scale_copy));
		if (!(enhance->enhance_en & BIT(4)))
			enhance->enhance_en |= BIT(1);
		enhance->enhance_en |= BIT(0);
		pr_info("enhance scaling backup\n");
		break;
	case ENHANCE_CFG_ID_HSV:
		memcpy(&enhance->hsv_copy, param, sizeof(enhance->hsv_copy));
		enhance->enhance_en |= BIT(2);
		pr_info("enhance hsv backup\n");
		break;
	case ENHANCE_CFG_ID_CM:
		memcpy(&enhance->cm_copy, param, sizeof(enhance->cm_copy));
		enhance->enhance_en |= BIT(3);
		pr_info("enhance cm backup\n");
		break;
	case ENHANCE_CFG_ID_LTM:
		memcpy(&enhance->ltm_copy, param, sizeof(enhance->ltm_copy));
		enhance->enhance_en |= BIT(6);
		pr_info("enhance ltm backup\n");
		break;
	case ENHANCE_CFG_ID_SLP:
		memcpy(&enhance->slp_copy, param, sizeof(enhance->slp_copy));
		enhance->enhance_en |= BIT(4);
		pr_info("enhance slp backup\n");
		break;
	case ENHANCE_CFG_ID_GAMMA:
		memcpy(&enhance->gamma_copy, param, sizeof(enhance->gamma_copy));
		enhance->enhance_en |= BIT(5) | BIT(10);
		pr_info("enhance gamma backup\n");
		break;
	case ENHANCE_CFG_ID_EPF:
		memcpy(&enhance->epf_copy, param, sizeof(enhance->epf_copy));
		enhance->enhance_en |= BIT(1);
		pr_info("enhance epf backup\n");
		break;
	case ENHANCE_CFG_ID_LUT3D:
		memcpy(&enhance->lut3d_copy, param, sizeof(enhance->lut3d_copy));
		enhance->enhance_en |= BIT(9);
		pr_info("enhance lut3d backup\n");
		break;
	case ENHANCE_CFG_ID_SR_EPF:
		memcpy(&enhance->sr_epf, param, sizeof(enhance->sr_epf));
		/* valid range of gain3 is [128,255]; */
		if (enhance->sr_epf.gain3 == 0) {
			/* eye comfort and super resolution are enabled*/
			if (!(enhance->enhance_en & BIT(2)) && (enhance->enhance_en & BIT(0))) {
				enhance->enhance_en &= ~BIT(1);
				pr_info("enhance[ID_SR_EPF] backup disable epf\n");
			}
			enhance->sr_epf_ready = 0;
		} else {
			enhance->sr_epf_ready = 1;
			pr_info("enhance[ID_SR_EPF] epf backup\n");
		}
		break;
	default:
		break;
	}
}

static void dpu_enhance_set(struct dpu_context *ctx, u32 id, void *param)
{
	struct dpu_enhance *enhance = ctx->enhance;
	struct scale_cfg *scale;
	struct cm_cfg cm;
	struct slp_cfg *slp;
	struct ltm_cfg *ltm;
	struct gamma_lut *gamma;
	struct threed_lut *lut3d;
	struct hsv_lut *hsv;
	struct epf_cfg *epf_slp;
	bool dpu_stopped;
	u32 *p;
	int i;

	if (!ctx->enabled) {
		dpu_enhance_backup(ctx, id, param);
		return;
	}

	if (ctx->if_type == SPRD_DPU_IF_EDPI)
		dpu_wait_stop_done(ctx);

	switch (id) {
	case ENHANCE_CFG_ID_ENABLE:
		p = param;
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, *p);
		pr_info("enhance module enable: 0x%x\n", *p);
		break;
	case ENHANCE_CFG_ID_DISABLE:
		p = param;
		if (*p & BIT(1)) {
			if ((enhance->enhance_en & BIT(0)) && enhance->sr_epf_ready) {
				*p &= ~BIT(1);
				dpu_epf_set(ctx, &enhance->sr_epf);
				pr_info("enhance epf shouldn't be disabled\n");
			}
		}
		DPU_REG_CLR(ctx->base + REG_DPU_ENHANCE_CFG, *p);
		pr_info("enhance module disable: 0x%x\n", *p);
		break;
	case ENHANCE_CFG_ID_SCL:
		memcpy(&enhance->scale_copy, param, sizeof(enhance->scale_copy));
		scale = &enhance->scale_copy;
		DPU_REG_WR(ctx->base + REG_BLEND_SIZE, (scale->in_h << 16) | scale->in_w);
		DPU_REG_WR(ctx->base + REG_EPF_EPSILON, (epf.epsilon1 << 16) | epf.epsilon0);
		DPU_REG_WR(ctx->base + REG_EPF_GAIN0_3, (epf.gain3 << 24) | (epf.gain2 << 16) |
				(epf.gain1 << 8) | epf.gain0);
		DPU_REG_WR(ctx->base + REG_EPF_GAIN4_7, (epf.gain7 << 24) | (epf.gain6 << 16) |
				(epf.gain5 << 8) | epf.gain4);
		DPU_REG_WR(ctx->base + REG_EPF_DIFF, (epf.max_diff << 8) | epf.min_diff);
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(0) | BIT(1));
		pr_info("enhance scaling: %ux%u\n", scale->in_w, scale->in_h);
		break;
	case ENHANCE_CFG_ID_HSV:
		memcpy(&enhance->hsv_copy, param, sizeof(enhance->hsv_copy));
		hsv = &enhance->hsv_copy;
		for (i = 0; i < 360; i++) {
			DPU_REG_WR(ctx->base + REG_HSV_LUT_ADDR, i);
			udelay(1);
			DPU_REG_WR(ctx->base + REG_HSV_LUT_WDATA, (hsv->table[i].sat << 16) |
						hsv->table[i].hue);
		}
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(2));
		pr_info("enhance hsv set\n");
		break;
	case ENHANCE_CFG_ID_CM:
		memcpy(&enhance->cm_copy, param, sizeof(enhance->cm_copy));
		memcpy(&cm, &enhance->cm_copy, sizeof(struct cm_cfg));

		DPU_REG_WR(ctx->base + REG_CM_COEF01_00, (cm.coef01 << 16) | cm.coef00);
		DPU_REG_WR(ctx->base + REG_CM_COEF03_02, (cm.coef03 << 16) | cm.coef02);
		DPU_REG_WR(ctx->base + REG_CM_COEF11_10, (cm.coef11 << 16) | cm.coef10);
		DPU_REG_WR(ctx->base + REG_CM_COEF13_12, (cm.coef13 << 16) | cm.coef12);
		DPU_REG_WR(ctx->base + REG_CM_COEF21_20, (cm.coef21 << 16) | cm.coef20);
		DPU_REG_WR(ctx->base + REG_CM_COEF23_22, (cm.coef23 << 16) | cm.coef22);
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(3));
		pr_info("enhance cm set\n");
		break;
	case ENHANCE_CFG_ID_LTM:
		memcpy(&enhance->ltm_copy, param, sizeof(struct ltm_cfg));
		ltm = &enhance->ltm_copy;
		DPU_REG_WR(ctx->base + REG_SLP_CFG8, ((ltm->limit_hclip & 0x1ff) << 23) |
			((ltm->limit_lclip & 0x1ff) << 14) |
			((ltm->limit_clip_step & 0x1fff) << 0));
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(6));
		pr_info("enhance ltm set\n");
		break;
	case ENHANCE_CFG_ID_SLP:
		memcpy(&enhance->slp_copy, param, sizeof(enhance->slp_copy));
		slp = &enhance->slp_copy;
		DPU_REG_WR(ctx->base + REG_SLP_CFG0, (slp->brightness_step << 0) |
			((slp->brightness & 0x7f) << 16));
		DPU_REG_WR(ctx->base + REG_SLP_CFG1, ((slp->fst_max_bright_th & 0x7f) << 21) |
			((slp->fst_max_bright_th_step[0] & 0x7f) << 14) |
			((slp->fst_max_bright_th_step[1] & 0x7f) << 7) |
			((slp->fst_max_bright_th_step[2] & 0x7f) << 0));
		DPU_REG_WR(ctx->base + REG_SLP_CFG2, ((slp->fst_max_bright_th_step[3] & 0x7f) << 25) |
			((slp->fst_max_bright_th_step[4] & 0x7f) << 18) |
			((slp->hist_exb_no & 0x3) << 16) |
			((slp->hist_exb_percent & 0x7f) << 9));
		DPU_REG_WR(ctx->base + REG_SLP_CFG3, ((slp->mask_height & 0xfff) << 19) |
			((slp->fst_pth_index[0] & 0xf) << 15) |
			((slp->fst_pth_index[1] & 0xf) << 11) |
			((slp->fst_pth_index[2] & 0xf) << 7) |
			((slp->fst_pth_index[3] & 0xf) << 3));
		DPU_REG_WR(ctx->base + REG_SLP_CFG4, (slp->hist9_index[0] << 24) |
			(slp->hist9_index[1] << 16) | (slp->hist9_index[2] << 8) |
			(slp->hist9_index[3] << 0));
		DPU_REG_WR(ctx->base + REG_SLP_CFG5, (slp->hist9_index[4] << 24) |
			(slp->hist9_index[5] << 16) | (slp->hist9_index[6] << 8) |
			(slp->hist9_index[7] << 0));
		DPU_REG_WR(ctx->base + REG_SLP_CFG6, (slp->hist9_index[8] << 24) |
			(slp->glb_x[0] << 16) | (slp->glb_x[1] << 8) |
			(slp->glb_x[2] << 0));
		DPU_REG_WR(ctx->base + REG_SLP_CFG7, ((slp->glb_s[0] & 0x1ff) << 23) |
			((slp->glb_s[1] & 0x1ff) << 14) |
			((slp->glb_s[2] & 0x1ff) << 5));
		DPU_REG_WR(ctx->base + REG_SLP_CFG9, ((slp->fast_ambient_th & 0x7f) << 25) |
			(slp->scene_change_percent_th << 17) |
			((slp->local_weight & 0xf) << 13) |
			((slp->fst_pth & 0x7f) << 6));
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(4));
		pr_info("enhance slp set\n");
		break;
	case ENHANCE_CFG_ID_GAMMA:
		memcpy(&enhance->gamma_copy, param, sizeof(enhance->gamma_copy));
		gamma = &enhance->gamma_copy;
		for (i = 0; i < 256; i++) {
			DPU_REG_WR(ctx->base + REG_GAMMA_LUT_ADDR, i);
			udelay(1);
			DPU_REG_WR(ctx->base + REG_GAMMA_LUT_WDATA, (gamma->r[i] << 20) |
						(gamma->g[i] << 10) |
						gamma->b[i]);
			pr_debug("0x%02x: r=%u, g=%u, b=%u\n", i,
				gamma->r[i], gamma->g[i], gamma->b[i]);
		}
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(5) | BIT(10));
		pr_info("enhance gamma set\n");
		break;
	case ENHANCE_CFG_ID_EPF:
		memcpy(&enhance->epf_copy, param, sizeof(enhance->epf_copy));
		if (((enhance->enhance_en & BIT(4)) &&
			(enhance->slp_copy.brightness > SLP_BRIGHTNESS_THRESHOLD)) ||
			!(enhance->enhance_en & BIT(0)) || !enhance->sr_epf_ready) {
			epf_slp = &enhance->epf_copy;
			pr_info("enhance epf set\n");
		} else {
			epf_slp = &enhance->sr_epf;
			pr_info("enhance epf(sr) set\n");
		}

		dpu_epf_set(ctx, epf_slp);
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(1));
		break;
	case ENHANCE_CFG_ID_SR_EPF:
		memcpy(&enhance->sr_epf, param, sizeof(enhance->sr_epf));
		/* valid range of gain3 is [128,255]; */
		if (enhance->sr_epf.gain3 == 0) {
			enhance->sr_epf_ready = 0;

			if ((enhance->enhance_en & BIT(2)) && (enhance->enhance_en & BIT(0))) {
				epf_slp = &enhance->epf_copy;
				dpu_epf_set(ctx, epf_slp);
				DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(1));
				pr_info("enhance[ID_SR_EPF] epf set\n");
				break;
			} else if (enhance->enhance_en & BIT(0)) {
				DPU_REG_CLR(ctx->base + REG_DPU_ENHANCE_CFG, BIT(1));
				pr_info("enhance[ID_SR_EPF] disable epf\n");
				break;
			}
			return;
		}

		enhance->sr_epf_ready = 1;
		if ((enhance->enhance_en & BIT(0)) && (!(enhance->enhance_en & BIT(4)) ||
			(enhance->slp_copy.brightness <= SLP_BRIGHTNESS_THRESHOLD))) {
			epf_slp = &enhance->sr_epf;
			dpu_epf_set(ctx, epf_slp);
			DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(1));
			pr_info("enhance[ID_SR_EPF] epf(sr) set\n");
			break;
		}

		pr_info("enhance[ID_SR_EPF] epf(sr) set delay\n");
		return;
	case ENHANCE_CFG_ID_LUT3D:
		memcpy(&enhance->lut3d_copy, param, sizeof(enhance->lut3d_copy));
		lut3d = &enhance->lut3d_copy;
		/* Fixme: Not sure why temp variables are used here */
		dpu_stopped = ctx->stopped;

		dpu_stop(ctx);
		for (i = 0; i < 729; i++) {
			DPU_REG_WR(ctx->base + REG_TREED_LUT_ADDR, i);
			ndelay(1);
			DPU_REG_WR(ctx->base + REG_TREED_LUT_WDATA, lut3d->value[i]);
			pr_debug("0x%x:0x%x\n", i, lut3d->value[i]);
		}
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(9));
		if ((ctx->if_type == SPRD_DPU_IF_DPI) && !dpu_stopped)
			dpu_run(ctx);
		pr_info("enhance lut3d set\n");
		enhance->enhance_en = DPU_REG_RD(ctx->base + REG_DPU_ENHANCE_CFG);
		return;
	default:
		break;
	}

	if ((ctx->if_type == SPRD_DPU_IF_DPI) && !ctx->stopped) {
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT(2));
		dpu_wait_update_done(ctx);
	} else if ((ctx->if_type == SPRD_DPU_IF_EDPI) && ctx->panel_ready) {
		/*
		 * In EDPI mode, we need to wait panel initializatin
		 * completed. Otherwise, the dpu enhance settings may
		 * start before panel initialization.
		 */
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT(0));
		ctx->stopped = false;
	}

	enhance->enhance_en = DPU_REG_RD(ctx->base + REG_DPU_ENHANCE_CFG);
}

static void dpu_enhance_get(struct dpu_context *ctx, u32 id, void *param)
{
	struct scale_cfg *scale;
	struct epf_cfg *ep;
	struct slp_cfg *slp;
	struct ltm_cfg *ltm;
	struct gamma_lut *gamma;
	struct threed_lut *lut3d;
	u32 *p32;
	int i, val;

	switch (id) {
	case ENHANCE_CFG_ID_ENABLE:
		p32 = param;
		*p32 = DPU_REG_RD(ctx->base + REG_DPU_ENHANCE_CFG);
		pr_info("enhance module enable get\n");
		break;
	case ENHANCE_CFG_ID_SCL:
		scale = param;
		val = DPU_REG_RD(ctx->base + REG_BLEND_SIZE);
		scale->in_w = val & 0xffff;
		scale->in_h = val >> 16;
		pr_info("enhance scaling get\n");
		break;
	case ENHANCE_CFG_ID_EPF:
		ep = param;

		val = DPU_REG_RD(ctx->base + REG_EPF_EPSILON);
		ep->epsilon0 = val;
		ep->epsilon1 = val >> 16;

		val = DPU_REG_RD(ctx->base + REG_EPF_GAIN0_3);
		ep->gain0 = val;
		ep->gain1 = val >> 8;
		ep->gain2 = val >> 16;
		ep->gain3 = val >> 24;

		val = DPU_REG_RD(ctx->base + REG_EPF_GAIN4_7);
		ep->gain4 = val;
		ep->gain5 = val >> 8;
		ep->gain6 = val >> 16;
		ep->gain7 = val >> 24;

		val = DPU_REG_RD(ctx->base + REG_EPF_DIFF);
		ep->min_diff = val;
		ep->max_diff = val >> 8;
		pr_info("enhance epf get\n");
		break;
	case ENHANCE_CFG_ID_HSV:
		dpu_stop(ctx);
		p32 = param;
		for (i = 0; i < 360; i++) {
			DPU_REG_WR(ctx->base + REG_HSV_LUT_ADDR, i);
			udelay(1);
			*p32++ = DPU_REG_RD(ctx->base + REG_HSV_LUT_RDATA);
		}
		dpu_run(ctx);
		pr_info("enhance hsv get\n");
		break;
	case ENHANCE_CFG_ID_CM:
		p32 = param;
		*p32++ = DPU_REG_RD(ctx->base + REG_CM_COEF01_00);
		*p32++ = DPU_REG_RD(ctx->base + REG_CM_COEF03_02);
		*p32++ = DPU_REG_RD(ctx->base + REG_CM_COEF11_10);
		*p32++ = DPU_REG_RD(ctx->base + REG_CM_COEF13_12);
		*p32++ = DPU_REG_RD(ctx->base + REG_CM_COEF21_20);
		*p32++ = DPU_REG_RD(ctx->base + REG_CM_COEF23_22);
		pr_info("enhance cm get\n");
		break;
	case ENHANCE_CFG_ID_LTM:
		ltm = param;
		val = DPU_REG_RD(ctx->base + REG_SLP_CFG8);
		ltm->limit_hclip = (val >> 23) & 0x1ff;
		ltm->limit_lclip = (val >> 14) & 0x1ff;
		ltm->limit_clip_step = (val >> 0) & 0x1fff;
		pr_info("enhance ltm get\n");
		break;
	case ENHANCE_CFG_ID_SLP:
		slp = param;
		val = DPU_REG_RD(ctx->base + REG_SLP_CFG0);
		slp->brightness = (val >> 16) & 0x7f;
		slp->brightness_step = (val >> 0) & 0xffff;

		val = DPU_REG_RD(ctx->base + REG_SLP_CFG1);
		slp->fst_max_bright_th = (val >> 21) & 0x7f;
		slp->fst_max_bright_th_step[0] = (val >> 14) & 0x7f;
		slp->fst_max_bright_th_step[1] = (val >> 7) & 0x7f;
		slp->fst_max_bright_th_step[2] = (val >> 0) & 0x7f;

		val = DPU_REG_RD(ctx->base + REG_SLP_CFG2);
		slp->fst_max_bright_th_step[3] = (val >> 25) & 0x7f;
		slp->fst_max_bright_th_step[4] = (val >> 18) & 0x7f;
		slp->hist_exb_no = (val >> 16) & 0x3;
		slp->hist_exb_percent = (val >> 9) & 0x7f;

		val = DPU_REG_RD(ctx->base + REG_SLP_CFG3);
		slp->mask_height = (val >> 19) & 0xfff;
		slp->fst_pth_index[0] = (val >> 15) & 0xf;
		slp->fst_pth_index[1] = (val >> 11) & 0xf;
		slp->fst_pth_index[2] = (val >> 7) & 0xf;
		slp->fst_pth_index[3] = (val >> 3) & 0xf;

		val = DPU_REG_RD(ctx->base + REG_SLP_CFG4);
		slp->hist9_index[0] = (val >> 24) & 0xff;
		slp->hist9_index[1] = (val >> 16) & 0xff;
		slp->hist9_index[2] = (val >> 8) & 0xff;
		slp->hist9_index[3] = (val >> 0) & 0xff;

		val = DPU_REG_RD(ctx->base + REG_SLP_CFG5);
		slp->hist9_index[4] = (val >> 24) & 0xff;
		slp->hist9_index[5] = (val >> 16) & 0xff;
		slp->hist9_index[6] = (val >> 8) & 0xff;
		slp->hist9_index[7] = (val >> 0) & 0xff;

		val = DPU_REG_RD(ctx->base + REG_SLP_CFG6);
		slp->hist9_index[8] = (val >> 24) & 0xff;
		slp->glb_x[0] = (val >> 16) & 0xff;
		slp->glb_x[1] = (val >> 8) & 0xff;
		slp->glb_x[2] = (val >> 0) & 0xff;

		val = DPU_REG_RD(ctx->base + REG_SLP_CFG7);
		slp->glb_s[0] = (val >> 23) & 0x1ff;
		slp->glb_s[1] = (val >> 14) & 0x1ff;
		slp->glb_s[2] = (val >> 5) & 0x1ff;

		val = DPU_REG_RD(ctx->base + REG_SLP_CFG9);
		slp->fast_ambient_th = (val >> 25) & 0x7f;
		slp->scene_change_percent_th = (val >> 17) & 0xff;
		slp->local_weight = (val >> 13) & 0xf;
		slp->fst_pth = (val >> 6) & 0x7f;

		pr_info("enhance slp get\n");
		break;
	case ENHANCE_CFG_ID_GAMMA:
		dpu_stop(ctx);
		gamma = param;
		for (i = 0; i < 256; i++) {
			DPU_REG_WR(ctx->base + REG_GAMMA_LUT_ADDR, i);
			udelay(1);
			val = DPU_REG_RD(ctx->base + REG_GAMMA_LUT_RDATA);
			gamma->r[i] = (val >> 20) & 0x3FF;
			gamma->g[i] = (val >> 10) & 0x3FF;
			gamma->b[i] = val & 0x3FF;
			pr_debug("0x%02x: r=%u, g=%u, b=%u\n", i,
				gamma->r[i], gamma->g[i], gamma->b[i]);
		}
		dpu_run(ctx);
		pr_info("enhance gamma get\n");
		break;
	case ENHANCE_CFG_ID_SLP_LUT:
		dpu_stop(ctx);
		p32 = param;
		for (i = 0; i < 256; i++) {
			DPU_REG_WR(ctx->base + REG_SLP_LUT_ADDR, i);
			udelay(1);
			*p32++ = DPU_REG_RD(ctx->base + REG_SLP_LUT_RDATA);
		}
		dpu_run(ctx);
		pr_info("enhance slp lut get\n");
		break;
	case ENHANCE_CFG_ID_LUT3D:
		lut3d = param;
		dpu_stop(ctx);
		for (i = 0; i < 729; i++) {
			DPU_REG_WR(ctx->base + REG_TREED_LUT_ADDR, i);
			udelay(1);
			lut3d->value[i] = DPU_REG_RD(ctx->base + REG_TREED_LUT_RDATA);
			pr_debug("0x%02x: 0x%x\n", i, lut3d->value[i]);
		}
		dpu_run(ctx);
		pr_info("enhance lut3d get\n");
		break;
	default:
		break;
	}
}

static void dpu_enhance_reload(struct dpu_context *ctx)
{
	struct dpu_enhance *enhance = ctx->enhance;
	struct scale_cfg *scale;
	struct cm_cfg *cm;
	struct slp_cfg *slp;
	struct ltm_cfg *ltm;
	struct gamma_lut *gamma;
	struct hsv_lut *hsv;
	struct epf_cfg *epf;
	struct threed_lut *lut3d;
	int i;

	for (i = 0; i < 256; i++) {
		DPU_REG_WR(ctx->base + REG_SLP_LUT_ADDR, i);
		udelay(1);
		DPU_REG_WR(ctx->base + REG_SLP_LUT_WDATA, slp_lut[i]);
	}
	pr_info("enhance slp lut reload\n");

	if (enhance->enhance_en & BIT(0)) {
		scale = &enhance->scale_copy;
		DPU_REG_WR(ctx->base + REG_BLEND_SIZE, (scale->in_h << 16) | scale->in_w);
		pr_info("enhance scaling from %ux%u to %ux%u\n", scale->in_w,
			scale->in_h, ctx->vm.hactive, ctx->vm.vactive);
	}

	if (enhance->enhance_en & BIT(1)) {
		if (((enhance->enhance_en & BIT(4)) &&
			(enhance->slp_copy.brightness > SLP_BRIGHTNESS_THRESHOLD)) ||
			!(enhance->enhance_en & BIT(0)) || !enhance->sr_epf_ready) {
			epf = &enhance->epf_copy;
			pr_info("enhance epf reload\n");
		} else {
			epf = &enhance->sr_epf;
			pr_info("enhance epf(sr) reload\n");
		}

		dpu_epf_set(ctx, epf);
	}

	if (enhance->enhance_en & BIT(2)) {
		hsv = &enhance->hsv_copy;
		for (i = 0; i < 360; i++) {
			DPU_REG_WR(ctx->base + REG_HSV_LUT_ADDR, i);
			udelay(1);
			DPU_REG_WR(ctx->base + REG_HSV_LUT_WDATA, (hsv->table[i].sat << 16) |
						hsv->table[i].hue);
		}
		pr_info("enhance hsv reload\n");
	}

	if (enhance->enhance_en & BIT(3)) {
		cm = &enhance->cm_copy;
		DPU_REG_WR(ctx->base + REG_CM_COEF01_00, (cm->coef01 << 16) | cm->coef00);
		DPU_REG_WR(ctx->base + REG_CM_COEF03_02, (cm->coef03 << 16) | cm->coef02);
		DPU_REG_WR(ctx->base + REG_CM_COEF11_10, (cm->coef11 << 16) | cm->coef10);
		DPU_REG_WR(ctx->base + REG_CM_COEF13_12, (cm->coef13 << 16) | cm->coef12);
		DPU_REG_WR(ctx->base + REG_CM_COEF21_20, (cm->coef21 << 16) | cm->coef20);
		DPU_REG_WR(ctx->base + REG_CM_COEF23_22, (cm->coef23 << 16) | cm->coef22);
		pr_info("enhance cm reload\n");
	}

	if (enhance->enhance_en & BIT(4)) {
		slp = &enhance->slp_copy;
		DPU_REG_WR(ctx->base + REG_SLP_CFG0, (slp->brightness_step << 0) |
			((slp->brightness & 0x7f) << 16));
		DPU_REG_WR(ctx->base + REG_SLP_CFG1, ((slp->fst_max_bright_th & 0x7f) << 21) |
			((slp->fst_max_bright_th_step[0] & 0x7f) << 14) |
			((slp->fst_max_bright_th_step[1] & 0x7f) << 7) |
			((slp->fst_max_bright_th_step[2] & 0x7f) << 0));
		DPU_REG_WR(ctx->base + REG_SLP_CFG2, ((slp->fst_max_bright_th_step[3] & 0x7f) << 25) |
			((slp->fst_max_bright_th_step[4] & 0x7f) << 18) |
			((slp->hist_exb_no & 0x3) << 16) |
			((slp->hist_exb_percent & 0x7f) << 9));
		DPU_REG_WR(ctx->base + REG_SLP_CFG3, ((slp->mask_height & 0xfff) << 19) |
			((slp->fst_pth_index[0] & 0xf) << 15) |
			((slp->fst_pth_index[1] & 0xf) << 11) |
			((slp->fst_pth_index[2] & 0xf) << 7) |
			((slp->fst_pth_index[3] & 0xf) << 3));
		DPU_REG_WR(ctx->base + REG_SLP_CFG4, (slp->hist9_index[0] << 24) |
			(slp->hist9_index[1] << 16) | (slp->hist9_index[2] << 8) |
			(slp->hist9_index[3] << 0));
		DPU_REG_WR(ctx->base + REG_SLP_CFG5, (slp->hist9_index[4] << 24) |
			(slp->hist9_index[5] << 16) | (slp->hist9_index[6] << 8) |
			(slp->hist9_index[7] << 0));
		DPU_REG_WR(ctx->base + REG_SLP_CFG6, (slp->hist9_index[8] << 24) |
			(slp->glb_x[0] << 16) | (slp->glb_x[1] << 8) |
			(slp->glb_x[2] << 0));
		DPU_REG_WR(ctx->base + REG_SLP_CFG7, ((slp->glb_s[0] & 0x1ff) << 23) |
			((slp->glb_s[1] & 0x1ff) << 14) |
			((slp->glb_s[2] & 0x1ff) << 5));
		DPU_REG_WR(ctx->base + REG_SLP_CFG9, ((slp->fast_ambient_th & 0x7f) << 25) |
			(slp->scene_change_percent_th << 17) |
			((slp->local_weight & 0xf) << 13) |
			((slp->fst_pth & 0x7f) << 6));
		pr_info("enhance slp reload\n");
	}

	if (enhance->enhance_en & BIT(5)) {
		gamma = &enhance->gamma_copy;
		for (i = 0; i < 256; i++) {
			DPU_REG_WR(ctx->base + REG_GAMMA_LUT_ADDR, i);
			udelay(1);
			DPU_REG_WR(ctx->base + REG_GAMMA_LUT_WDATA, (gamma->r[i] << 20) |
						(gamma->g[i] << 10) |
						gamma->b[i]);
			pr_debug("0x%02x: r=%u, g=%u, b=%u\n", i,
				gamma->r[i], gamma->g[i], gamma->b[i]);
		}
		pr_info("enhance gamma reload\n");
	}

	if (enhance->enhance_en & BIT(6)) {
		ltm = &enhance->ltm_copy;
		DPU_REG_WR(ctx->base + REG_SLP_CFG8, ((ltm->limit_hclip & 0x1ff) << 23) |
			((ltm->limit_lclip & 0x1ff) << 14) |
			((ltm->limit_clip_step & 0x1fff) << 0));
		pr_info("enhance ltm reload\n");
	}

	if (enhance->enhance_en & BIT(9)) {
		lut3d = &enhance->lut3d_copy;
		for (i = 0; i < 729; i++) {
			DPU_REG_WR(ctx->base + REG_TREED_LUT_RDATA, i);
			udelay(1);
			DPU_REG_WR(ctx->base + REG_TREED_LUT_WDATA, lut3d->value[i]);
			pr_debug("0x%02x:0x%x\n", i, lut3d->value[i]);
		}
		pr_info("enhance lut3d reload\n");
	}

	DPU_REG_WR(ctx->base + REG_DPU_ENHANCE_CFG, enhance->enhance_en);
}

static const u32 primary_fmts[] = {
	DRM_FORMAT_XRGB8888, DRM_FORMAT_XBGR8888,
	DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGBA8888, DRM_FORMAT_BGRA8888,
	DRM_FORMAT_RGBX8888, DRM_FORMAT_BGRX8888,
	DRM_FORMAT_RGB565, DRM_FORMAT_BGR565,
	DRM_FORMAT_NV12, DRM_FORMAT_NV21,
	DRM_FORMAT_NV16, DRM_FORMAT_NV61,
	DRM_FORMAT_YUV420,
};

static void dpu_capability(struct dpu_context *ctx,
			struct sprd_crtc_capability *cap)
{
	cap->max_layers = 4;
	cap->fmts_ptr = primary_fmts;
	cap->fmts_cnt = ARRAY_SIZE(primary_fmts);
}

const struct dpu_core_ops dpu_r4p0_core_ops = {
	.version = dpu_version,
	.parse_dt = dpu_parse_dt,
	.init = dpu_init,
	.fini = dpu_fini,
	.run = dpu_run,
	.stop = dpu_stop,
	.isr = dpu_isr,
	.ifconfig = dpu_dpi_init,
	.capability = dpu_capability,
	.flip = dpu_flip,
	.bg_color = dpu_bgcolor,
	.enable_vsync = enable_vsync,
	.disable_vsync = disable_vsync,
	.enhance_init = dpu_enhance_init,
	.enhance_set = dpu_enhance_set,
	.enhance_get = dpu_enhance_get,
};
