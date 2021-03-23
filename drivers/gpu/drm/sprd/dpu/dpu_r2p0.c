// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#include "sprd_crtc.h"
#include "sprd_dpu.h"

#define XFBC8888_HEADER_SIZE(w, h) (ALIGN((w) * (h) / (8 * 8) / 2, 128))
#define XFBC8888_PAYLOAD_SIZE(w, h) (w * h * 4)
#define XFBC8888_BUFFER_SIZE(w, h) (XFBC8888_HEADER_SIZE(w, h) \
				+ XFBC8888_PAYLOAD_SIZE(w, h))

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

/* MMU control registers */
#define REG_MMU_EN			0x800
#define REG_MMU_VPN_RANGE		0x80C
#define REG_MMU_VAOR_ADDR_RD		0x818
#define REG_MMU_VAOR_ADDR_WR		0x81C
#define REG_MMU_INV_ADDR_RD		0x820
#define REG_MMU_INV_ADDR_WR		0x824
#define REG_MMU_PPN1			0x83C
#define REG_MMU_RANGE1			0x840
#define REG_MMU_PPN2			0x844
#define REG_MMU_RANGE2			0x848

/* Global control bits */
#define BIT_DPU_RUN			BIT(0)
#define BIT_DPU_STOP			BIT(1)
#define BIT_DPU_REG_UPDATE		BIT(2)
#define BIT_DPU_IF_EDPI			BIT(0)
#define BIT_DPU_COEF_NARROW_RANGE		BIT(4)
#define BIT_DPU_Y2R_COEF_ITU709_STANDARD	BIT(5)

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
#define BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3		(0x00 << 8)
#define BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0		(0x01 << 8)
#define BIT_DPU_LAY_NO_SWITCH			(0x00 << 10)
#define BIT_DPU_LAY_RB_OR_UV_SWITCH		(0x01 << 10)
#define BIT_DPU_LAY_PALLETE_EN			(0x01 << 12)
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
#define BIT_DPU_INT_MMU_VAOR_RD		BIT(16)
#define BIT_DPU_INT_MMU_VAOR_WR		BIT(17)
#define BIT_DPU_INT_MMU_INV_RD		BIT(18)
#define BIT_DPU_INT_MMU_INV_WR		BIT(19)

/* DPI control bits */
#define BIT_DPU_EDPI_TE_EN		BIT(8)
#define BIT_DPU_EDPI_FROM_EXTERNAL_PAD	BIT(10)
#define BIT_DPU_DPI_HALT_EN		BIT(16)

static bool panel_ready = true;

static void dpu_clean_all(struct dpu_context *ctx);
static void dpu_layer(struct dpu_context *ctx,
		    struct sprd_crtc_layer *hwlayer);

static void dpu_version(struct dpu_context *ctx)
{
	ctx->version = "dpu-r2p0";
}

static u32 check_mmu_isr(struct dpu_context *ctx, u32 reg_val)
{
	u32 mmu_mask = BIT_DPU_INT_MMU_VAOR_RD |
			BIT_DPU_INT_MMU_VAOR_WR |
			BIT_DPU_INT_MMU_INV_RD |
			BIT_DPU_INT_MMU_INV_WR;
	u32 val = reg_val & mmu_mask;
	int i, j;

	if (val) {
		pr_err("iommu interrupt err: 0x%04x\n", val);

		if (val & BIT_DPU_INT_MMU_INV_RD)
			pr_err("iommu invalid read error, addr: 0x%08x\n",
				DPU_REG_RD(ctx->base + REG_MMU_INV_ADDR_RD));
		if (val & BIT_DPU_INT_MMU_INV_WR)
			pr_err("iommu invalid write error, addr: 0x%08x\n",
				DPU_REG_RD(ctx->base + REG_MMU_INV_ADDR_WR));
		if (val & BIT_DPU_INT_MMU_VAOR_RD)
			pr_err("iommu va out of range read error, addr: 0x%08x\n",
				DPU_REG_RD(ctx->base + REG_MMU_VAOR_ADDR_RD));
		if (val & BIT_DPU_INT_MMU_VAOR_WR)
			pr_err("iommu va out of range write error, addr: 0x%08x\n",
				DPU_REG_RD(ctx->base + REG_MMU_VAOR_ADDR_WR));

		for (i = 0; i < 8; i++) {
			reg_val = DPU_REG_RD(ctx->base + DPU_LAY_REG(REG_LAY_CTRL, i));
			if (reg_val & 0x1) {
				for(j = 0; j < 3; j++) {
					pr_info("layer%d: 0x%08x 0x%08x 0x%08x ctrl: 0x%08x\n", i,
						DPU_REG_RD(ctx->base + DPU_LAY_PLANE_ADDR(
                                                	REG_LAY_BASE_ADDR, i, j)),
						DPU_REG_RD(ctx->base + DPU_LAY_PLANE_ADDR(
                                                	REG_LAY_BASE_ADDR, i, j)),
						DPU_REG_RD(ctx->base + DPU_LAY_PLANE_ADDR(
                                                	REG_LAY_BASE_ADDR, i, j)),
						DPU_REG_RD(ctx->base + DPU_LAY_REG(
							REG_LAY_CTRL, i)));
				}
			}
		}

		/* panic("iommu panic\n"); */
	}

	return val;
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

	/* dpu ifbc payload error isr */
	if (reg_val & BIT_DPU_INT_FBC_PLD_ERR) {
		int_mask |= BIT_DPU_INT_FBC_PLD_ERR;
		pr_err("dpu ifbc payload error\n");
	}

	/* dpu ifbc header error isr */
	if (reg_val & BIT_DPU_INT_FBC_HDR_ERR) {
		int_mask |= BIT_DPU_INT_FBC_HDR_ERR;
		pr_err("dpu ifbc header error\n");
	}

	int_mask |= check_mmu_isr(ctx, reg_val);

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
		if (!panel_ready) {
			dpu_wait_stop_done(ctx);
			/* wait for TE again */
			mdelay(20);
			panel_ready = true;
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

	reg_val = BIT_DPU_COEF_NARROW_RANGE | BIT_DPU_Y2R_COEF_ITU709_STANDARD;
	DPU_REG_WR(ctx->base + REG_DPU_CFG0, reg_val);
	DPU_REG_WR(ctx->base + REG_DPU_CFG1, 0x004466da);
	DPU_REG_WR(ctx->base + REG_DPU_CFG2, 0x00);

	if (ctx->stopped)
		dpu_clean_all(ctx);

	DPU_REG_WR(ctx->base + REG_MMU_EN, 0x00);
	DPU_REG_WR(ctx->base + REG_MMU_PPN1, 0x00);
	DPU_REG_WR(ctx->base + REG_MMU_RANGE1, 0xffff);
	DPU_REG_WR(ctx->base + REG_MMU_PPN2, 0x00);
	DPU_REG_WR(ctx->base + REG_MMU_RANGE2, 0xffff);
	DPU_REG_WR(ctx->base + REG_MMU_VPN_RANGE, 0x1ffff);

	DPU_REG_WR(ctx->base + REG_DPU_INT_CLR, 0xffff);

	return 0;
}

static void dpu_fini(struct dpu_context *ctx)
{
	DPU_REG_WR(ctx->base + REG_DPU_INT_EN, 0x00);
	DPU_REG_WR(ctx->base + REG_DPU_INT_CLR, 0xff);

	panel_ready = false;
}

enum {
	DPU_LAYER_FORMAT_YUV422_2PLANE,
	DPU_LAYER_FORMAT_YUV420_2PLANE,
	DPU_LAYER_FORMAT_YUV420_3PLANE,
	DPU_LAYER_FORMAT_ARGB8888,
	DPU_LAYER_FORMAT_RGB565,
	DPU_LAYER_FORMAT_XFBC_ARGB8888 = 8,
	DPU_LAYER_FORMAT_XFBC_RGB565,
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

static u32 dpu_img_ctrl(u32 format, u32 blending, u32 compression, u32 rotation)
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
		/* FALLTHRU */
	case DRM_FORMAT_ABGR8888:
		/* rb switch */
		reg_val |= BIT_DPU_LAY_RB_OR_UV_SWITCH;
		/* FALLTHRU */
	case DRM_FORMAT_ARGB8888:
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= BIT_DPU_LAY_FORMAT_XFBC_ARGB8888;
		else
			reg_val |= BIT_DPU_LAY_FORMAT_ARGB8888;
		break;
	case DRM_FORMAT_XBGR8888:
		/* rb switch */
		reg_val |= BIT_DPU_LAY_RB_OR_UV_SWITCH;
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
		reg_val |= BIT_DPU_LAY_RB_OR_UV_SWITCH;
		/* FALLTHRU */
	case DRM_FORMAT_RGB565:
		if (compression)
			/* XFBC-RGB565 */
			reg_val |= BIT_DPU_LAY_FORMAT_XFBC_RGB565;
		else
			reg_val |= BIT_DPU_LAY_FORMAT_RGB565;
		break;
	case DRM_FORMAT_NV12:
		/* 2-Lane: Yuv420 */
		reg_val |= BIT_DPU_LAY_FORMAT_YUV420_2PLANE;
		/* Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		/* UV endian */
		reg_val |= BIT_DPU_LAY_NO_SWITCH;
		break;
	case DRM_FORMAT_NV21:
		/* 2-Lane: Yuv420 */
		reg_val |= BIT_DPU_LAY_FORMAT_YUV420_2PLANE;
		/* Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		/* UV endian */
		reg_val |= BIT_DPU_LAY_RB_OR_UV_SWITCH;
		break;
	case DRM_FORMAT_NV16:
		/* 2-Lane: Yuv422 */
		reg_val |= BIT_DPU_LAY_FORMAT_YUV422_2PLANE;
		/* Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0;
		/* UV endian */
		reg_val |= BIT_DPU_LAY_RB_OR_UV_SWITCH;
		break;
	case DRM_FORMAT_NV61:
		/* 2-Lane: Yuv422 */
		reg_val |= BIT_DPU_LAY_FORMAT_YUV422_2PLANE;
		/* Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		/* UV endian */
		reg_val |= BIT_DPU_LAY_NO_SWITCH;
		break;
	case DRM_FORMAT_YUV420:
		reg_val |= BIT_DPU_LAY_FORMAT_YUV420_3PLANE;
		/* Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		/* UV endian */
		reg_val |= BIT_DPU_LAY_NO_SWITCH;
		break;
	case DRM_FORMAT_YVU420:
		reg_val |= BIT_DPU_LAY_FORMAT_YUV420_3PLANE;
		/* Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		/* UV endian */
		reg_val |= BIT_DPU_LAY_RB_OR_UV_SWITCH;
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
		/*Normal mode*/
		reg_val |= BIT_DPU_LAY_MODE_BLEND_NORMAL;
		break;
	case DRM_MODE_BLEND_PREMULTI:
		/* alpha mode select - combo alpha */
		reg_val |= BIT_DPU_LAY_COMBO_ALPHA;
		/*Pre-mult mode*/
		reg_val |= BIT_DPU_LAY_MODE_BLEND_PREMULT;
		break;
	default:
		/* alpha mode select - layer alpha */
		reg_val |= BIT_DPU_LAY_LAYER_ALPHA;
		break;
	}

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
	u32 addr, size, offset, ctrl, reg_val, pitch;
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
		addr = hwlayer->addr[i];

		/* dpu r2p0 just support xfbc-rgb */
		if (hwlayer->xfbc)
			addr += hwlayer->header_size_r;

		if (addr % 16)
			pr_err("layer addr[%d] is not 16 bytes align, it's 0x%08x\n",
				i, addr);
		DPU_REG_WR(ctx->base + DPU_LAY_PLANE_ADDR(REG_LAY_BASE_ADDR,
				hwlayer->index, i), addr);
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
		hwlayer->xfbc, hwlayer->rotation);

	/*
	 * if layer0 blend mode is premult mode, and layer alpha value
	 * is 0xff, use layer alpha.
	 */
	if (hwlayer->index == 0 &&
		(hwlayer->blending == DRM_MODE_BLEND_PREMULTI) &&
		(hwlayer->alpha == 0xff)) {
		ctrl &= ~BIT(3); // Fix it later
		ctrl |= BIT_DPU_LAY_LAYER_ALPHA;
	}

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
	 * Make sure the dpu is in stop status. DPU_R2P0 has no shadow
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
		  BIT_DPU_INT_FBC_HDR_ERR |
		  BIT_DPU_INT_MMU_VAOR_RD |
		  BIT_DPU_INT_MMU_VAOR_WR |
		  BIT_DPU_INT_MMU_INV_RD |
		  BIT_DPU_INT_MMU_INV_WR;
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

		/* select te from external pad */
		DPU_REG_SET(ctx->base + REG_DPI_CTRL, BIT_DPU_EDPI_FROM_EXTERNAL_PAD);

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
		/* enable write back done INT */
		int_mask |= BIT_DPU_INT_WB_DONE;
		/* enable write back fail INT */
		int_mask |= BIT_DPU_INT_WB_ERR;

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
	/* enable iommu va out of range read error INT */
	int_mask |= BIT_DPU_INT_MMU_VAOR_RD;
	/* enable iommu va out of range write error INT */
	int_mask |= BIT_DPU_INT_MMU_VAOR_WR;
	/* enable iommu invalid read error INT */
	int_mask |= BIT_DPU_INT_MMU_INV_RD;
	/* enable iommu invalid write error INT */
	int_mask |= BIT_DPU_INT_MMU_INV_WR;

	DPU_REG_WR(ctx->base + REG_DPU_INT_EN, int_mask);
}

static void enable_vsync(struct dpu_context *ctx)
{
	DPU_REG_SET(ctx->base + REG_DPU_INT_EN, BIT_DPU_INT_VSYNC);
}

static void disable_vsync(struct dpu_context *ctx)
{
	DPU_REG_CLR(ctx->base + REG_DPU_INT_EN, BIT_DPU_INT_VSYNC);
}

static const u32 primary_fmts[] = {
	DRM_FORMAT_XRGB8888, DRM_FORMAT_XBGR8888,
	DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGBA8888, DRM_FORMAT_BGRA8888,
	DRM_FORMAT_RGBX8888, DRM_FORMAT_BGRX8888,
	DRM_FORMAT_RGB565, DRM_FORMAT_BGR565,
	DRM_FORMAT_NV12, DRM_FORMAT_NV21,
	DRM_FORMAT_NV16, DRM_FORMAT_NV61,
	DRM_FORMAT_YUV420, DRM_FORMAT_YVU420,
};

static void dpu_capability(struct dpu_context *ctx,
			struct sprd_crtc_capability *cap)
{
	cap->max_layers = 6;
	cap->fmts_ptr = primary_fmts;
	cap->fmts_cnt = ARRAY_SIZE(primary_fmts);
}

const struct dpu_core_ops dpu_r2p0_core_ops = {
	.version = dpu_version,
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
};
