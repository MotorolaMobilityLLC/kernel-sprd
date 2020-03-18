// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#include "sprd_crtc.h"
#include "sprd_dpu.h"

#define DISPC_INT_FBC_PLD_ERR_MASK	BIT(8)
#define DISPC_INT_FBC_HDR_ERR_MASK	BIT(9)

#define DISPC_INT_MMU_INV_WR_MASK	BIT(19)
#define DISPC_INT_MMU_INV_RD_MASK	BIT(18)
#define DISPC_INT_MMU_VAOR_WR_MASK	BIT(17)
#define DISPC_INT_MMU_VAOR_RD_MASK	BIT(16)

#define XFBC8888_HEADER_SIZE(w, h) (ALIGN((w) * (h) / (8 * 8) / 2, 128))
#define XFBC8888_PAYLOAD_SIZE(w, h) (w * h * 4)
#define XFBC8888_BUFFER_SIZE(w, h) (XFBC8888_HEADER_SIZE(w, h) \
				+ XFBC8888_PAYLOAD_SIZE(w, h))

struct layer_reg {
	u32 addr[4];
	u32 ctrl;
	u32 size;
	u32 pitch;
	u32 pos;
	u32 alpha;
	u32 ck;
	u32 pallete;
	u32 crop_start;
};

struct wb_region_reg {
	u32 pos;
	u32 size;
};

struct dpu_reg {
	u32 dpu_version;
	u32 dpu_ctrl;
	u32 dpu_cfg0;
	u32 dpu_cfg1;
	u32 dpu_cfg2;
	u32 dpu_secure;
	u32 reserved_0x0018_0x001C[2];
	u32 panel_size;
	u32 blend_size;
	u32 reserved_0x0028;
	u32 bg_color;
	struct layer_reg layers[8];
	u32 wb_base_addr;
	u32 wb_ctrl;
	u32 wb_cfg;
	u32 wb_pitch;
	struct wb_region_reg region[3];
	u32 reserved_0x01D8_0x01DC[2];
	u32 dpu_int_en;
	u32 dpu_int_clr;
	u32 dpu_int_sts;
	u32 dpu_int_raw;
	u32 dpi_ctrl;
	u32 dpi_h_timing;
	u32 dpi_v_timing;
	u32 reserved_0x01FC;
	u32 dpu_enhance_cfg;
	u32 reserved_0x0204_0x020C[3];
	u32 epf_epsilon;
	u32 epf_gain0_3;
	u32 epf_gain4_7;
	u32 epf_diff;
	u32 reserved_0x0220_0x023C[8];
	u32 hsv_lut_addr;
	u32 hsv_lut_wdata;
	u32 hsv_lut_rdata;
	u32 reserved_0x024C_0x027C[13];
	u32 cm_coef01_00;
	u32 cm_coef03_02;
	u32 cm_coef11_10;
	u32 cm_coef13_12;
	u32 cm_coef21_20;
	u32 cm_coef23_22;
	u32 reserved_0x0298_0x02BC[10];
	u32 slp_cfg0;
	u32 slp_cfg1;
	u32 reserved_0x02C8_0x02FC[14];
	u32 gamma_lut_addr;
	u32 gamma_lut_wdata;
	u32 gamma_lut_rdata;
	u32 reserved_0x030C_0x033C[13];
	u32 checksum_en;
	u32 checksum0_start_pos;
	u32 checksum0_end_pos;
	u32 checksum1_start_pos;
	u32 checksum1_end_pos;
	u32 checksum0_result;
	u32 checksum1_result;
	u32 reserved_0x035C;
	u32 dpu_sts[18];
	u32 reserved_0x03A8_0x03AC[2];
	u32 dpu_fbc_cfg0;
	u32 dpu_fbc_cfg1;
	u32 reserved_0x03B8_0x03EC[14];
	u32 rf_ram_addr;
	u32 rf_ram_rdata_low;
	u32 rf_ram_rdata_high;
	u32 reserved_0x03FC_0x07FC[257];
	u32 mmu_en;
	u32 mmu_update;
	u32 mmu_min_vpn;
	u32 mmu_vpn_range;
	u32 mmu_pt_addr;
	u32 mmu_default_page;
	u32 mmu_vaor_addr_rd;
	u32 mmu_vaor_addr_wr;
	u32 mmu_inv_addr_rd;
	u32 mmu_inv_addr_wr;
	u32 mmu_uns_addr_rd;
	u32 mmu_uns_addr_wr;
	u32 mmu_miss_cnt;
	u32 mmu_pt_update_qos;
	u32 mmu_version;
	u32 mmu_min_ppn1;
	u32 mmu_ppn_range1;
	u32 mmu_min_ppn2;
	u32 mmu_ppn_range2;
	u32 mmu_vpn_paor_rd;
	u32 mmu_vpn_paor_wr;
	u32 mmu_ppn_paor_rd;
	u32 mmu_ppn_paor_wr;
	u32 mmu_reg_au_manage;
	u32 mmu_page_rd_ch;
	u32 mmu_page_wr_ch;
	u32 mmu_read_page_cmd_cnt;
	u32 mmu_read_page_latency_cnt;
	u32 mmu_page_max_latency;
};

static bool panel_ready = true;

static void dpu_clean_all(struct sprd_crtc_context *ctx);
static void dpu_layer(struct sprd_crtc_context *ctx,
		    struct sprd_crtc_layer *hwlayer);

static u32 check_mmu_isr(struct sprd_crtc_context *ctx, u32 reg_val)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 mmu_mask = (DISPC_INT_MMU_VAOR_RD_MASK |
			DISPC_INT_MMU_VAOR_WR_MASK |
			DISPC_INT_MMU_INV_RD_MASK |
			DISPC_INT_MMU_INV_WR_MASK);
	u32 val = reg_val & mmu_mask;
	int i;

	if (val) {
		pr_err("iommu interrupt err: 0x%04x\n", val);

		if (val & DISPC_INT_MMU_INV_RD_MASK)
			pr_err("iommu invalid read error, addr: 0x%08x\n",
				reg->mmu_inv_addr_rd);
		if (val & DISPC_INT_MMU_INV_WR_MASK)
			pr_err("iommu invalid write error, addr: 0x%08x\n",
				reg->mmu_inv_addr_wr);
		if (val & DISPC_INT_MMU_VAOR_RD_MASK)
			pr_err("iommu va out of range read error, addr: 0x%08x\n",
				reg->mmu_vaor_addr_rd);
		if (val & DISPC_INT_MMU_VAOR_WR_MASK)
			pr_err("iommu va out of range write error, addr: 0x%08x\n",
				reg->mmu_vaor_addr_wr);

		for (i = 0; i < 8; i++) {
			if (reg->layers[i].ctrl & 0x1)
				pr_info("layer%d: 0x%08x 0x%08x 0x%08x ctrl: 0x%08x\n",
					i, reg->layers[i].addr[0],
					reg->layers[i].addr[1],
					reg->layers[i].addr[2],
					reg->layers[i].ctrl);
		}

		/* panic("iommu panic\n"); */
	}

	return val;
}

static u32 dpu_isr(struct sprd_crtc_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 reg_val, int_mask = 0;

	reg_val = reg->dpu_int_sts;

	/* disable err interrupt */
	if (reg_val & DISPC_INT_ERR_MASK)
		int_mask |= DISPC_INT_ERR_MASK;

	/* dpu update done isr */
	if (reg_val & DISPC_INT_UPDATE_DONE_MASK) {
		ctx->evt_update = true;
		wake_up_interruptible_all(&ctx->wait_queue);
	}

	/* dpu stop done isr */
	if (reg_val & DISPC_INT_DONE_MASK) {
		ctx->evt_stop = true;
		wake_up_interruptible_all(&ctx->wait_queue);
	}

	/* dpu ifbc payload error isr */
	if (reg_val & DISPC_INT_FBC_PLD_ERR_MASK) {
		int_mask |= DISPC_INT_FBC_PLD_ERR_MASK;
		pr_err("dpu ifbc payload error\n");
	}

	/* dpu ifbc header error isr */
	if (reg_val & DISPC_INT_FBC_HDR_ERR_MASK) {
		int_mask |= DISPC_INT_FBC_HDR_ERR_MASK;
		pr_err("dpu ifbc header error\n");
	}

	int_mask |= check_mmu_isr(ctx, reg_val);

	reg->dpu_int_clr = reg_val;
	reg->dpu_int_en &= ~int_mask;

	return reg_val;
}

static int dpu_wait_stop_done(struct sprd_crtc_context *ctx)
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

static int dpu_wait_update_done(struct sprd_crtc_context *ctx)
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

static void dpu_stop(struct sprd_crtc_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	if (ctx->if_type == SPRD_DISPC_IF_DPI)
		reg->dpu_ctrl |= BIT(1);

	dpu_wait_stop_done(ctx);

	pr_info("dpu stop\n");
}

static void dpu_run(struct sprd_crtc_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->dpu_ctrl |= BIT(0);

	ctx->stopped = false;

	pr_info("dpu run\n");

	if (ctx->if_type == SPRD_DISPC_IF_EDPI) {
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

static int dpu_init(struct sprd_crtc_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 size;

	reg->bg_color = 0;

	size = (ctx->vm.vactive << 16) | ctx->vm.hactive;

	reg->panel_size = size;
	reg->blend_size = size;

	reg->dpu_cfg0 = BIT(4) | BIT(5);
	reg->dpu_cfg1 = 0x004466da;
	reg->dpu_cfg2 = 0;

	if (ctx->stopped)
		dpu_clean_all(ctx);

	/* Fixme later */
	reg->mmu_en = 0;
	//reg->mmu_min_ppn1 = 0;
	//reg->mmu_ppn_range1 = 0xffff;
	//reg->mmu_min_ppn2 = 0;
	//reg->mmu_ppn_range2 = 0xffff;
	reg->mmu_vpn_range = 0x1ffff;

	reg->dpu_int_clr = 0xffff;

	return 0;
}

static void dpu_fini(struct sprd_crtc_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->dpu_int_en = 0;
	reg->dpu_int_clr = 0xff;

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
	case DRM_MODE_REFLECT_Y:
		rot = DPU_LAYER_ROTATION_180_M;
		break;
	case (DRM_MODE_REFLECT_Y | DRM_MODE_ROTATE_90):
		rot = DPU_LAYER_ROTATION_90_M;
		break;
	case DRM_MODE_REFLECT_X:
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
	reg_val |= BIT(0);

	switch (format) {
	case DRM_FORMAT_BGRA8888:
		/* BGRA8888 -> ARGB8888 */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 8;
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= (DPU_LAYER_FORMAT_XFBC_ARGB8888 << 4);
		else
			reg_val |= (DPU_LAYER_FORMAT_ARGB8888 << 4);
		break;
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
		/* RGBA8888 -> ABGR8888 */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 8;
		/* FALLTHRU */
	case DRM_FORMAT_ABGR8888:
		/* rb switch */
		reg_val |= BIT(10);
		/* FALLTHRU */
	case DRM_FORMAT_ARGB8888:
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= (DPU_LAYER_FORMAT_XFBC_ARGB8888 << 4);
		else
			reg_val |= (DPU_LAYER_FORMAT_ARGB8888 << 4);
		break;
	case DRM_FORMAT_XBGR8888:
		/* rb switch */
		reg_val |= BIT(10);
		/* FALLTHRU */
	case DRM_FORMAT_XRGB8888:
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= (DPU_LAYER_FORMAT_XFBC_ARGB8888 << 4);
		else
			reg_val |= (DPU_LAYER_FORMAT_ARGB8888 << 4);
		break;
	case DRM_FORMAT_BGR565:
		/* rb switch */
		reg_val |= BIT(10);
		/* FALLTHRU */
	case DRM_FORMAT_RGB565:
		if (compression)
			/* XFBC-RGB565 */
			reg_val |= (DPU_LAYER_FORMAT_XFBC_RGB565 << 4);
		else
			reg_val |= (DPU_LAYER_FORMAT_RGB565 << 4);
		break;
	case DRM_FORMAT_NV12:
		/* 2-Lane: Yuv420 */
		reg_val |= DPU_LAYER_FORMAT_YUV420_2PLANE << 4;
		/* Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/* UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 10;
		break;
	case DRM_FORMAT_NV21:
		/* 2-Lane: Yuv420 */
		reg_val |= DPU_LAYER_FORMAT_YUV420_2PLANE << 4;
		/* Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/* UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 10;
		break;
	case DRM_FORMAT_NV16:
		/* 2-Lane: Yuv422 */
		reg_val |= DPU_LAYER_FORMAT_YUV422_2PLANE << 4;
		/* Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 8;
		/* UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 10;
		break;
	case DRM_FORMAT_NV61:
		/* 2-Lane: Yuv422 */
		reg_val |= DPU_LAYER_FORMAT_YUV422_2PLANE << 4;
		/* Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/* UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 10;
		break;
	case DRM_FORMAT_YUV420:
		reg_val |= DPU_LAYER_FORMAT_YUV420_3PLANE << 4;
		/* Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/* UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 10;
		break;
	case DRM_FORMAT_YVU420:
		reg_val |= DPU_LAYER_FORMAT_YUV420_3PLANE << 4;
		/* Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/* UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 10;
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
		reg_val |= BIT(2);
		break;
	case DRM_MODE_BLEND_COVERAGE:
		/* alpha mode select - combo alpha */
		reg_val |= BIT(3);
		/*Normal mode*/
		reg_val &= (~BIT(16));
		break;
	case DRM_MODE_BLEND_PREMULTI:
		/* alpha mode select - combo alpha */
		reg_val |= BIT(3);
		/*Pre-mult mode*/
		reg_val |= BIT(16);
		break;
	default:
		/* alpha mode select - layer alpha */
		reg_val |= BIT(2);
		break;
	}

	rotation = to_dpu_rotation(rotation);
	reg_val |= (rotation & 0x7) << 20;

	return reg_val;
}

static void dpu_clean_all(struct sprd_crtc_context *ctx)
{
	int i;
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	for (i = 0; i < 8; i++)
		reg->layers[i].ctrl = 0;
}

static void dpu_bgcolor(struct sprd_crtc_context *ctx, u32 color)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	if (ctx->if_type == SPRD_DISPC_IF_EDPI)
		dpu_wait_stop_done(ctx);

	reg->bg_color = color;

	dpu_clean_all(ctx);

	if ((ctx->if_type == SPRD_DISPC_IF_DPI) && !ctx->stopped) {
		reg->dpu_ctrl |= BIT(2);
		dpu_wait_update_done(ctx);
	} else if (ctx->if_type == SPRD_DISPC_IF_EDPI) {
		reg->dpu_ctrl |= BIT(0);
		ctx->stopped = false;
	}
}

static void dpu_layer(struct sprd_crtc_context *ctx,
		    struct sprd_crtc_layer *hwlayer)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	const struct drm_format_info *info;
	struct layer_reg *layer;
	u32 addr, size, offset, ctrl;
	int i;

	layer = &reg->layers[hwlayer->index];
	offset = (hwlayer->dst_x & 0xffff) | ((hwlayer->dst_y) << 16);

	if (hwlayer->pallete_en) {
		size = (hwlayer->dst_w & 0xffff) | ((hwlayer->dst_h) << 16);
		layer->pos = offset;
		layer->size = size;
		layer->alpha = hwlayer->alpha;
		layer->pallete = hwlayer->pallete_color;

		/* pallete layer enable */
		layer->ctrl = 0x1005;

		pr_debug("dst_x = %d, dst_y = %d, dst_w = %d, dst_h = %d\n",
			hwlayer->dst_x, hwlayer->dst_y,
			hwlayer->dst_w, hwlayer->dst_h);
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
		layer->addr[i] = addr;
	}

	layer->pos = offset;
	layer->size = size;
	layer->crop_start = (hwlayer->src_y << 16) | hwlayer->src_x;
	layer->alpha = hwlayer->alpha;

	info = drm_format_info(hwlayer->format);
	if (hwlayer->planes == 3)
		/* UV pitch is 1/2 of Y pitch*/
		layer->pitch = (hwlayer->pitch[0] / info->cpp[0]) |
				(hwlayer->pitch[0] / info->cpp[0] << 15);
	else
		layer->pitch = hwlayer->pitch[0] / info->cpp[0];

	ctrl = dpu_img_ctrl(hwlayer->format, hwlayer->blending,
		hwlayer->xfbc, hwlayer->rotation);

	/*
	 * if layer0 blend mode is premult mode, and layer alpha value
	 * is 0xff, use layer alpha.
	 */
	if (hwlayer->index == 0 &&
		(hwlayer->blending == DRM_MODE_BLEND_PREMULTI) &&
		(hwlayer->alpha == 0xff)) {
		ctrl &= ~BIT(3);
		ctrl |= BIT(2);
	}

	layer->ctrl = ctrl;

	pr_debug("dst_x = %d, dst_y = %d, dst_w = %d, dst_h = %d\n",
				hwlayer->dst_x, hwlayer->dst_y,
				hwlayer->dst_w, hwlayer->dst_h);
	pr_debug("start_x = %d, start_y = %d, start_w = %d, start_h = %d\n",
				hwlayer->src_x, hwlayer->src_y,
				hwlayer->src_w, hwlayer->src_h);
}

static void dpu_flip(struct sprd_crtc_context *ctx,
		     struct sprd_crtc_layer layers[], u8 count)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	int i;

	/*
	 * Make sure the dpu is in stop status. DPU_R2P0 has no shadow
	 * registers in EDPI mode. So the config registers can only be
	 * updated in the rising edge of DPU_RUN bit.
	 */
	if (ctx->if_type == SPRD_DISPC_IF_EDPI)
		dpu_wait_stop_done(ctx);

	/* reset the bgcolor to black */
	reg->bg_color = 0;

	/* disable all the layers */
	dpu_clean_all(ctx);

	/* start configure dpu layers */
	for (i = 0; i < count; i++)
		dpu_layer(ctx, &layers[i]);

	/* update trigger and wait */
	if (ctx->if_type == SPRD_DISPC_IF_DPI) {
		if (!ctx->stopped) {
			reg->dpu_ctrl |= BIT(2);
			dpu_wait_update_done(ctx);
		}

		reg->dpu_int_en |= DISPC_INT_ERR_MASK;

	} else if (ctx->if_type == SPRD_DISPC_IF_EDPI) {
		reg->dpu_ctrl |= BIT(0);

		ctx->stopped = false;
	}

	/*
	 * If the following interrupt was disabled in isr,
	 * re-enable it.
	 */
	reg->dpu_int_en |= DISPC_INT_FBC_PLD_ERR_MASK |
			   DISPC_INT_FBC_HDR_ERR_MASK |
			   DISPC_INT_MMU_VAOR_RD_MASK |
			   DISPC_INT_MMU_VAOR_WR_MASK |
			   DISPC_INT_MMU_INV_RD_MASK |
			   DISPC_INT_MMU_INV_WR_MASK;
}

static void dpu_dpi_init(struct sprd_crtc_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 int_mask = 0;

	if (ctx->if_type == SPRD_DISPC_IF_DPI) {
		/* use dpi as interface */
		reg->dpu_cfg0 &= ~BIT(0);

		/* disable Halt function for SPRD DSI */
		reg->dpi_ctrl &= ~BIT(16);

		/* select te from external pad */
		reg->dpi_ctrl |= BIT(10);

		/* set dpi timing */
		reg->dpi_h_timing = (ctx->vm.hsync_len << 0) |
				    (ctx->vm.hback_porch << 8) |
				    (ctx->vm.hfront_porch << 20);
		reg->dpi_v_timing = (ctx->vm.vsync_len << 0) |
				    (ctx->vm.vback_porch << 8) |
				    (ctx->vm.vfront_porch << 20);
		if (ctx->vm.vsync_len + ctx->vm.vback_porch < 32)
			pr_warn("Warning: (vsync + vbp) < 32, "
				"underflow risk!\n");

		/* enable dpu update done INT */
		int_mask |= DISPC_INT_UPDATE_DONE_MASK;
		/* enable dpu DONE  INT */
		int_mask |= DISPC_INT_DONE_MASK;
		/* enable dpu dpi vsync */
		int_mask |= DISPC_INT_DPI_VSYNC_MASK;
		/* enable dpu TE INT */
		int_mask |= DISPC_INT_TE_MASK;
		/* enable underflow err INT */
		int_mask |= DISPC_INT_ERR_MASK;
		/* enable write back done INT */
		int_mask |= DISPC_INT_WB_DONE_MASK;
		/* enable write back fail INT */
		int_mask |= DISPC_INT_WB_FAIL_MASK;

	} else if (ctx->if_type == SPRD_DISPC_IF_EDPI) {
		/* use edpi as interface */
		reg->dpu_cfg0 |= BIT(0);

		/* use external te */
		reg->dpi_ctrl |= BIT(10);

		/* enable te */
		reg->dpi_ctrl |= BIT(8);

		/* enable stop DONE INT */
		int_mask |= DISPC_INT_DONE_MASK;
		/* enable TE INT */
		int_mask |= DISPC_INT_TE_MASK;
	}

	/* enable ifbc payload error INT */
	int_mask |= DISPC_INT_FBC_PLD_ERR_MASK;
	/* enable ifbc header error INT */
	int_mask |= DISPC_INT_FBC_HDR_ERR_MASK;
	/* enable iommu va out of range read error INT */
	int_mask |= DISPC_INT_MMU_VAOR_RD_MASK;
	/* enable iommu va out of range write error INT */
	int_mask |= DISPC_INT_MMU_VAOR_WR_MASK;
	/* enable iommu invalid read error INT */
	int_mask |= DISPC_INT_MMU_INV_RD_MASK;
	/* enable iommu invalid write error INT */
	int_mask |= DISPC_INT_MMU_INV_WR_MASK;

	reg->dpu_int_en = int_mask;
}

static void enable_vsync(struct sprd_crtc_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->dpu_int_en |= DISPC_INT_DPI_VSYNC_MASK;
}

static void disable_vsync(struct sprd_crtc_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->dpu_int_en &= ~DISPC_INT_DPI_VSYNC_MASK;
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

static void dpu_capability(struct sprd_crtc_context *ctx,
			struct sprd_crtc_capability *cap)
{
	cap->max_layers = 6;
	cap->fmts_ptr = primary_fmts;
	cap->fmts_cnt = ARRAY_SIZE(primary_fmts);
}

const struct sprd_crtc_core_ops sharkl3_dpu_core_ops = {
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
