// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/backlight.h>
#include <linux/dma-buf.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include "sprd_bl.h"
#include "sprd_dpu.h"
#include "dpu_enhance_param.h"
#include "sprd_dsi.h"
#include "sprd_crtc.h"
#include "sprd_plane.h"
#include "sprd_dsi_panel.h"

#define XFBC8888_HEADER_SIZE(w, h) (ALIGN((ALIGN((w), 16)) * \
				(ALIGN((h), 16)) / 16, 128))
#define XFBC8888_PAYLOAD_SIZE(w, h) (ALIGN((w), 16) * ALIGN((h), 16) * 4)
#define XFBC8888_BUFFER_SIZE(w, h) (XFBC8888_HEADER_SIZE(w, h) \
				+ XFBC8888_PAYLOAD_SIZE(w, h))

#define SLP_BRIGHTNESS_THRESHOLD 0x20

/* DPU registers size, 4 Bytes(32 Bits) */
#define DPU_REG_SIZE					0x04
/* Layer registers offset */
#define DPU_LAY_REG_OFFSET				0x10

#define DPU_MAX_REG_OFFSET				0x19AC

#define DSC_REG_OFFSET					0x1A00
#define DSC1_REG_OFFSET					0x1B00

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

#define DSC_REG(reg) (reg + DSC_REG_OFFSET)

#define DSC1_REG(reg) (reg + DSC1_REG_OFFSET)

/* DSC_PicW_PicH_SliceW_SliceH  */
#define DSC_1440_2560_720_2560	0
#define DSC_1080_2408_540_8	1
#define DSC_720_2560_720_8	2
#define DSC_1080_2400_540_2400	3

/*Global control registers */
#define REG_DPU_CTRL					0x08
#define REG_DPU_CFG0					0x0C
#define REG_DPU_CFG1					0x10
#define REG_PANEL_SIZE					0x18
#define REG_BLEND_SIZE					0x1C
#define REG_BG_COLOR					0x24

/* Layer enable */
#define REG_LAYER_ENABLE				0x2c

/* Layer0 control registers */
#define REG_LAY_BASE_ADDR				0x30
#define REG_LAY_CTRL					0x40
#define REG_LAY_DES_SIZE				0x44
#define REG_LAY_SRC_SIZE				0x48
#define REG_LAY_PITCH					0x4C
#define REG_LAY_POS					0x50
#define REG_LAY_ALPHA					0x54
#define REG_LAY_CK					0x58
#define REG_LAY_PALLETE					0x5C
#define REG_LAY_CROP_START				0x60

/* Write back config registers */
#define REG_WB_BASE_ADDR				0x230
#define REG_WB_CTRL					0x234
#define REG_WB_CFG					0x238
#define REG_WB_PITCH					0x23C

/* Interrupt control registers */
#define REG_DPU_INT_EN					0x250
#define REG_DPU_INT_CLR					0x254
#define REG_DPU_INT_STS					0x258
#define REG_DPU_INT_RAW					0x25C

/* DPI control registers */
#define REG_DPI_CTRL					0x260
#define REG_DPI_H_TIMING				0x264
#define REG_DPI_V_TIMING				0x268

/* DPU STS */
#define REG_DPU_STS_21					0x754
#define REG_DPU_STS_22					0x758

#define REG_DPU_MMU0_UPDATE				0x1808
#define REG_DPU_MODE					0x04

/* DPU SCL */
#define REG_DPU_SCL_EN					0x20

/* DPU ENHANCE */
#define REG_DPU_ENHANCE_CFG				0x500

/* DSC REG */
#define REG_DSC_CTRL					0x00
#define REG_DSC_PIC_SIZE				0x04
#define REG_DSC_GRP_SIZE				0x08
#define REG_DSC_SLICE_SIZE				0x0c
#define REG_DSC_H_TIMING				0x10
#define REG_DSC_V_TIMING				0x14
#define REG_DSC_CFG0					0x18
#define REG_DSC_CFG1					0x1c
#define REG_DSC_CFG2					0x20
#define REG_DSC_CFG3					0x24
#define REG_DSC_CFG4					0x28
#define REG_DSC_CFG5					0x2c
#define REG_DSC_CFG6					0x30
#define REG_DSC_CFG7					0x34
#define REG_DSC_CFG8					0x38
#define REG_DSC_CFG9					0x3c
#define REG_DSC_CFG10					0x40
#define REG_DSC_CFG11					0x44
#define REG_DSC_CFG12					0x48
#define REG_DSC_CFG13					0x4c
#define REG_DSC_CFG14					0x50
#define REG_DSC_CFG15					0x54
#define REG_DSC_CFG16					0x58
#define REG_DSC_STS0					0x5c
#define REG_DSC_STS1					0x60
#define REG_DSC_VERSION					0x64

/* Global control bits */
#define BIT_DPU_RUN					BIT(0)
#define BIT_DPU_STOP					BIT(1)
#define BIT_DPU_ALL_UPDATE				BIT(2)
#define BIT_DPU_REG_UPDATE				BIT(3)
#define BIT_LAY_REG_UPDATE				BIT(4)
#define BIT_DPU_IF_EDPI					BIT(0)

/* scaling config bits */
#define BIT_DPU_SCALING_EN		BIT(0)

/* Layer control bits */
// #define BIT_DPU_LAY_EN				BIT(0)
#define BIT_DPU_LAY_LAYER_ALPHA				(0x01 << 2)
#define BIT_DPU_LAY_COMBO_ALPHA				(0x01 << 3)
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
#define BIT_DPU_LAY_NO_SWITCH				(0x00 << 10)
#define BIT_DPU_LAY_RGB888_RB_SWITCH			(0x01 << 10)
#define BIT_DPU_LAY_RGB565_RB_SWITCH			(0x01 << 12)
#define BIT_DPU_LAY_PALLETE_EN				(0x01 << 13)
#define BIT_DPU_LAY_MODE_BLEND_NORMAL			(0x01 << 16)
#define BIT_DPU_LAY_MODE_BLEND_PREMULT			(0x01 << 16)

/*Interrupt control & status bits */
#define BIT_DPU_INT_DONE				BIT(0)
#define BIT_DPU_INT_TE					BIT(1)
#define BIT_DPU_INT_ERR					BIT(2)
#define BIT_DPU_INT_VSYNC_EN				BIT(4)
#define BIT_DPU_INT_WB_DONE_EN				BIT(5)
#define BIT_DPU_INT_WB_ERR_EN				BIT(6)
#define BIT_DPU_INT_FBC_PLD_ERR				BIT(7)
#define BIT_DPU_INT_FBC_HDR_ERR				BIT(8)
#define BIT_DPU_INT_DPU_ALL_UPDATE_DONE			BIT(16)
#define BIT_DPU_INT_DPU_REG_UPDATE_DONE			BIT(17)
#define BIT_DPU_INT_LAY_REG_UPDATE_DONE			BIT(18)
#define BIT_DPU_INT_PQ_REG_UPDATE_DONE			BIT(19)

/* DPI control bits */
#define BIT_DPU_EDPI_TE_EN				BIT(8)
#define BIT_DPU_EDPI_FROM_EXTERNAL_PAD			BIT(10)
#define BIT_DPU_DPI_HALT_EN				BIT(16)

#define BIT_DPU_STS_RCH_DPU_BUSY			BIT(15)

/* enhance config bits */
#define BIT_DPU_ENHANCE_EN		BIT(0)

struct wb_region {
	u32 index;
	u16 pos_x;
	u16 pos_y;
	u16 size_w;
	u16 size_h;
};

struct dpu_cfg1 {
	u8 arqos_low;
	u8 arqos_high;
	u8 awqos_low;
	u8 awqos_high;
};

struct layer_reg {
	u32 addr[4];
	u32 ctrl;
	u32 dst_size;
	u32 src_size;
	u32 pitch;
	u32 pos;
	u32 alpha;
	u32 ck;
	u32 pallete;
	u32 crop_start;
	u32 reserved[3];
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
	u8 cabc_endv;
	u8 cabc_startv;
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

struct cabc_para {
	u32 cabc_hist[32];
	u16 gain0;
	u16 gain1;
	u32 gain2;
	u8 p0;
	u8 p1;
	u8 p2;
	u16 bl_fix;
	u16 cur_bl;
	u8 video_mode;
	u8 slp_brightness;
	u8 slp_local_weight;
	u8 dci_en;
	u8 slp_en;
};

struct dpu_enhance {
	u32 enhance_en;
	int cabc_state;
	int frame_no;
	bool cabc_bl_set;

	struct hsv_lut hsv_copy;
	struct cm_cfg cm_copy;
	struct ltm_cfg ltm_copy;
	struct slp_cfg slp_copy;
	struct gamma_lut gamma_copy;
	struct epf_cfg epf_copy;
	struct threed_lut lut3d_copy;
	struct backlight_device *bl_dev;
	struct cabc_para cabc_para;
};

static struct dpu_cfg1 qos_cfg = {
	.arqos_low = 0x0a,
	.arqos_high = 0x0c,
	.awqos_low = 0x0a,
	.awqos_high = 0x0c,
};


struct dpu_dsc_cfg {
	char name[128];
	bool dual_dsi_en;
	bool dsc_en;
	int  dsc_mode;
};

/*
 * FIXME:
 * We don't know what's the best binding to link the panel with dpu dsc.
 * Fow now, we just add all panels that we support dsc, and search them
 */
static struct dpu_dsc_cfg dsc_cfg[] = {
	{
		.name = "lcd_nt35597_boe_mipi_qhd",
		.dual_dsi_en = 0,
		.dsc_en = 1,
		.dsc_mode = 0,
	},
	{
		.name = "lcd_nt57860_boe_mipi_qhd",
		.dual_dsi_en = 1,
		.dsc_en = 1,
		.dsc_mode = 2,
	},
	{
		.name = "lcd_nt36672c_truly_mipi_fhd",
		.dual_dsi_en = 0,
		.dsc_en = 1,
		.dsc_mode = 1,
	},
	{
		.name = "lcd_td4375_dijin_mipi_fhd",
		.dual_dsi_en = 0,
		.dsc_en = 1,
		.dsc_mode = 3,
	},
	{
		.name = "lcd_td4375_dijin_4lane_mipi_fhd",
		.dual_dsi_en = 0,
		.dsc_en = 1,
		.dsc_mode = 3,
	},
	{
		.name = "lcd_nt36672e_truly_mipi_fhd",
		.dual_dsi_en = 0,
		.dsc_en = 1,
		.dsc_mode = 1,
	},
};

static void dpu_sr_config(struct dpu_context *ctx);
static void dpu_clean_all(struct dpu_context *ctx);
static void dpu_layer(struct dpu_context *ctx,
		    struct sprd_layer_state *hwlayer);

static void dpu_version(struct dpu_context *ctx)
{
	ctx->version = "dpu-r6p0";
}

static int dpu_parse_dt(struct dpu_context *ctx,
				struct device_node *np)
{
	int ret = 0;
	struct device_node *qos_np;

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

	return 0;
}

static u32 dpu_isr(struct dpu_context *ctx)
{
	u32 reg_val, int_mask = 0;

	reg_val = DPU_REG_RD(ctx->base + REG_DPU_INT_STS);

	/* disable err interrupt */
	if (reg_val & BIT_DPU_INT_ERR)
		int_mask |= BIT_DPU_INT_ERR;

	/* dpu vsync isr */
	if (reg_val & BIT_DPU_INT_VSYNC_EN) {
		/* write back feature */
		if ((ctx->vsync_count == ctx->max_vsync_count) && ctx->wb_en)
			schedule_work(&ctx->wb_work);

		ctx->vsync_count++;
	}

	/* dpu update done isr */
	if (reg_val & BIT_DPU_INT_LAY_REG_UPDATE_DONE) {
		ctx->evt_update = true;
		wake_up_interruptible_all(&ctx->wait_queue);
	}

	if (reg_val & BIT_DPU_INT_DPU_ALL_UPDATE_DONE) {
		ctx->evt_all_update = true;
		wake_up_interruptible_all(&ctx->wait_queue);
	}

	// if (reg_val & DPU_INT_PQ_REG_UPDATE_DONE_MASK) {
		// ctx->evt_pq_update = true;
		// wake_up_interruptible_all(&wait_queue);
	// }

	/* dpu stop done isr */
	if (reg_val & BIT_DPU_INT_DONE) {
		ctx->evt_stop = true;
		wake_up_interruptible_all(&ctx->wait_queue);
	}

	/* dpu write back done isr */
	if (reg_val & BIT_DPU_INT_WB_DONE_EN) {
		/*
		 * The write back is a time-consuming operation. If there is a
		 * flip occurs before write back done, the write back buffer is
		 * no need to display. Otherwise the new frame will be covered
		 * by the write back buffer, which is not what we wanted.
		 */
		if (ctx->wb_en && (ctx->vsync_count > ctx->max_vsync_count)) {
			ctx->wb_en = false;
			schedule_work(&ctx->wb_work);
			/*reg_val |= DPU_INT_FENCE_SIGNAL_REQUEST;*/
		}

		pr_debug("wb done\n");
	}

	/* dpu write back error isr */
	if (reg_val & BIT_DPU_INT_WB_ERR_EN) {
		pr_err("dpu write back fail\n");
		/*give a new chance to write back*/
		// if (ctx->max_vsync_count > 0) {
		ctx->wb_en = true;
		ctx->vsync_count = 0;
		// }
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
	int rc, i;
	u32 dpu_sts_21, dpu_sts_22;

	if (ctx->stopped)
		return 0;

	/* wait for stop done interrupt */
	rc = wait_event_interruptible_timeout(ctx->wait_queue, ctx->evt_stop,
					       msecs_to_jiffies(500));
	ctx->evt_stop = false;

	ctx->stopped = true;

	if (!rc) {
		/* time out */
		pr_err("dpu wait for stop done time out!\n");
		return -1;
	}

	for (i = 1; i <= 3000; i++) {
		dpu_sts_21 = DPU_REG_RD(ctx->base + REG_DPU_STS_21);
		dpu_sts_22 = DPU_REG_RD(ctx->base + REG_DPU_STS_22);
		if ((dpu_sts_21 & BIT(15)) ||
		  (dpu_sts_22 & BIT(15)))
			mdelay(1);
		else {
			pr_info("dpu is idle now\n");
			break;
		}

		if (i == 3000)
			pr_err("wait for dpu idle timeout\n");
	}

	return 0;
}

static int dpu_wait_update_done(struct dpu_context *ctx)
{
	int rc;

	/* clear the event flag before wait */
	if (!ctx->stopped)
		ctx->evt_update = false;

	/* wait for reg update done interrupt */
	rc = wait_event_interruptible_timeout(ctx->wait_queue, ctx->evt_update,
					       msecs_to_jiffies(500));

	if (!rc) {
		/* time out */
		pr_err("dpu wait for reg update done time out!\n");
		return -1;
	}

	return 0;
}

static void dpu_stop(struct dpu_context *ctx)
{
	if (ctx->if_type == SPRD_DPU_IF_DPI)
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_STOP);

	dpu_wait_stop_done(ctx);
	ctx->evt_update = false;
	pr_info("dpu stop\n");
}

static void dpu_run(struct dpu_context *ctx)
{
	DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_RUN);
	DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_LAY_REG_UPDATE);
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


static void dpu_wb_trigger(struct dpu_context *ctx, u8 count, bool debug)
{
	int mode_width  = DPU_REG_RD(ctx->base + REG_BLEND_SIZE) & 0xFFFF;
	int mode_height = DPU_REG_RD(ctx->base + REG_BLEND_SIZE) >> 16;

	ctx->wb_layer.dst_w = mode_width;
	ctx->wb_layer.dst_h = mode_height;
	ctx->wb_layer.src_w = mode_width;
	ctx->wb_layer.src_h = mode_height;
	ctx->wb_layer.pitch[0] = ALIGN(mode_width, 16) * 4;
	ctx->wb_layer.fbc_hsize_r = XFBC8888_HEADER_SIZE(mode_width,
						mode_height) / 128;
	DPU_REG_WR(ctx->base + REG_WB_PITCH, ALIGN((mode_width), 16));

	ctx->wb_layer.xfbc = ctx->wb_xfbc_en;

	if (ctx->wb_xfbc_en) {
		DPU_REG_WR(ctx->base + REG_WB_CFG, (ctx->wb_layer.fbc_hsize_r << 16) | BIT(0));
		DPU_REG_WR(ctx->base + REG_WB_BASE_ADDR, ctx->wb_layer.addr[0] +
				ctx->wb_layer.fbc_hsize_r);
		}
	else {
		DPU_REG_WR(ctx->base + REG_WB_CFG, 0);
		DPU_REG_WR(ctx->base + REG_WB_BASE_ADDR, ctx->wb_layer.addr[0]);
		}

	DPU_REG_WR(ctx->base + REG_WB_PITCH, ctx->vm.hactive);

	if (debug)
		/* writeback debug trigger */
		DPU_REG_WR(ctx->base + REG_WB_CTRL, BIT(1));
	else
		DPU_REG_SET(ctx->base + REG_WB_CTRL, BIT(0));

	/* update trigger */
	DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT(4));
	dpu_wait_update_done(ctx);

	pr_debug("write back trigger\n");
}

static void dpu_wb_flip(struct dpu_context *ctx)
{
	dpu_clean_all(ctx);
	dpu_layer(ctx, &ctx->wb_layer);

	DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT(4));
	dpu_wait_update_done(ctx);
	pr_debug("write back flip\n");
}

static void dpu_wb_work_func(struct work_struct *data)
{
	struct dpu_context *ctx =
		container_of(data, struct dpu_context, wb_work);

	down(&ctx->lock);

	if (!ctx->enabled) {
		up(&ctx->lock);
		pr_err("dpu is not initialized\n");
		return;
	}

	if (ctx->flip_pending) {
		up(&ctx->lock);
		pr_warn("dpu flip is disabled\n");
		return;
	}

	if (ctx->wb_en && (ctx->vsync_count > ctx->max_vsync_count))
		dpu_wb_trigger(ctx, 1, false);
	else if (!ctx->wb_en)
		dpu_wb_flip(ctx);

	up(&ctx->lock);
}

static int dpu_write_back_config(struct dpu_context *ctx)
{
	static int need_config = 1;
	size_t wb_buf_size;
	struct sprd_dpu *dpu =
		(struct sprd_dpu *)container_of(ctx, struct sprd_dpu, ctx);
	struct drm_device *drm = dpu->crtc->base.dev;
	int mode_width  = DPU_REG_RD(ctx->base + REG_BLEND_SIZE) & 0xFFFF;

	if (!need_config) {
		DPU_REG_WR(ctx->base + REG_WB_BASE_ADDR, ctx->wb_addr_p);
		DPU_REG_WR(ctx->base + REG_WB_PITCH, ALIGN((mode_width), 16));
		if (ctx->wb_xfbc_en)
			DPU_REG_WR(ctx->base + REG_WB_CFG, ((ctx->wb_layer.fbc_hsize_r << 16) | BIT(0)));
		else
			DPU_REG_WR(ctx->base + REG_WB_CFG, 0);
		pr_debug("write back has configed\n");
		return 0;
	}

	wb_buf_size = XFBC8888_BUFFER_SIZE(dpu->mode->hdisplay,
						dpu->mode->vdisplay);
	pr_info("use wb_reserved memory for writeback, size:0x%zx\n", wb_buf_size);
	ctx->wb_addr_v = dma_alloc_wc(drm->dev, wb_buf_size, &ctx->wb_addr_p, GFP_KERNEL);
	if (!ctx->wb_addr_p) {
		ctx->max_vsync_count = 0;
		return -ENOMEM;
	}

	// ctx->wb_xfbc_en = 1;
	ctx->wb_layer.index = 7;
	ctx->wb_layer.planes = 1;
	ctx->wb_layer.alpha = 0xff;
	ctx->wb_layer.format = DRM_FORMAT_ABGR8888;
	ctx->wb_layer.addr[0] = ctx->wb_addr_p;
	DPU_REG_WR(ctx->base + REG_WB_BASE_ADDR, ctx->wb_addr_p);
	DPU_REG_WR(ctx->base + REG_WB_PITCH, ALIGN((mode_width), 16));
	if (ctx->wb_xfbc_en) {
		ctx->wb_layer.xfbc = ctx->wb_xfbc_en;
		DPU_REG_WR(ctx->base + REG_WB_CFG, ((ctx->wb_layer.fbc_hsize_r << 16) | BIT(0)));
	}

	ctx->max_vsync_count = 0;
	need_config = 0;

	INIT_WORK(&ctx->wb_work, dpu_wb_work_func);

	return 0;
}

/*
 * FIXME:
 * We don't know what's the best binding to link the panel with dpu dsc.
 * Fow now, we just hunt for all panels that we support, and get dsc cfg
 */
static void dpu_get_dsc_cfg(struct dpu_context *ctx)
{
	int index;
	struct sprd_dpu *dpu =
		(struct sprd_dpu *)container_of(ctx, struct sprd_dpu, ctx);

	for (index = 0; index < ARRAY_SIZE(dsc_cfg); index++) {
		if (!strcmp(dsc_cfg[index].name, dpu->dsi->ctx.lcd_name)) {
			ctx->dual_dsi_en = dsc_cfg[index].dual_dsi_en;
			ctx->dsc_en = dsc_cfg[index].dsc_en;
			ctx->dsc_mode = dsc_cfg[index].dsc_mode;
			return;
		}
	}
	pr_info("no found compatible, use dsc off\n");
}

static int dpu_config_dsc_param(struct dpu_context *ctx)
{
	u32 reg_val;
	struct sprd_dpu *dpu =
		(struct sprd_dpu *)container_of(ctx, struct sprd_dpu, ctx);

	if (ctx->dual_dsi_en) {
		reg_val = (ctx->vm.vactive << 16) |
			((ctx->vm.hactive >> 1)  << 0);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_PIC_SIZE), reg_val);
	} else {
		reg_val = (ctx->vm.vactive << 16) |
			(ctx->vm.hactive << 0);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_PIC_SIZE), reg_val);
	}
	if (ctx->dual_dsi_en) {
		reg_val = ((ctx->vm.hsync_len >> 1) << 0) |
			((ctx->vm.hback_porch  >> 1) << 8) |
			((ctx->vm.hfront_porch >> 1) << 20);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_H_TIMING), reg_val);
	} else {
		reg_val = (ctx->vm.hsync_len << 0) |
			(ctx->vm.hback_porch  << 8) |
			(ctx->vm.hfront_porch << 20);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_H_TIMING), reg_val);
	}
	reg_val = (ctx->vm.vsync_len << 0) |
			(ctx->vm.vback_porch  << 8) |
			(ctx->vm.vfront_porch << 20);
	DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_V_TIMING), reg_val);

	DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG0), 0x306c81db);
	DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG3), 0x12181800);
	DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG4), 0x003316b6);
	DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG5), 0x382a1c0e);
	DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG6), 0x69625446);
	DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG7), 0x7b797770);
	DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG8), 0x00007e7d);
	DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG9), 0x01000102);
	DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG10), 0x09be0940);
	DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG11), 0x19fa19fc);
	DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG12), 0x1a3819f8);
	DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG13), 0x1ab61a78);
	DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG14), 0x2b342af6);
	DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG15), 0x3b742b74);
	DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG16), 0x00006bf4);

	switch (ctx->dsc_mode) {
	case DSC_1440_2560_720_2560:
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_GRP_SIZE), 0x000000f0);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_SLICE_SIZE), 0x04096000);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG1), 0x000ae4bd);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG2), 0x0008000a);
		break;
	case DSC_1080_2408_540_8:
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_GRP_SIZE), 0x800b4);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_SLICE_SIZE), 0x050005a0);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG1), 0x7009b);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG2), 0xcb70db7);
		break;
	case DSC_720_2560_720_8:
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_GRP_SIZE), 0x800f0);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_SLICE_SIZE), 0x1000780);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG1), 0x000a00b1);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG2), 0x9890db7);
		if (ctx->dual_dsi_en) {
			reg_val = (ctx->vm.vactive << 16) |
				((ctx->vm.hactive >> 1) << 0);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_PIC_SIZE), reg_val);

			reg_val = ((ctx->vm.hsync_len >> 1) << 0) |
				((ctx->vm.hback_porch  >> 1) << 8) |
				((ctx->vm.hfront_porch >> 1) << 20);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_H_TIMING), reg_val);

			reg_val = (ctx->vm.vsync_len << 0) |
				(ctx->vm.vback_porch  << 8) |
				(ctx->vm.vfront_porch << 20);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_V_TIMING), reg_val);

			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_CFG0), 0x306c81db);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_CFG3), 0x12181800);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_CFG4), 0x003316b6);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_CFG5), 0x382a1c0e);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_CFG6), 0x69625446);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_CFG7), 0x7b797770);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_CFG8), 0x00007e7d);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_CFG9), 0x01000102);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_CFG10), 0x09be0940);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_CFG11), 0x19fa19fc);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_CFG12), 0x1a3819f8);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_CFG13), 0x1ab61a78);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_CFG14), 0x2b342af6);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_CFG15), 0x3b742b74);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_CFG16), 0x00006bf4);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_GRP_SIZE), 0x800f0);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_SLICE_SIZE), 0x1000780);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_CFG1), 0x000a00b1);
			DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_CFG2), 0x9890db7);
			if (dpu->dsi->ctx.work_mode == DSI_MODE_CMD)
				DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_CTRL), 0x2000010b);
			else
				DPU_REG_WR(ctx->base + DSC1_REG(REG_DSC_CTRL), 0x2000000b);
		}

		break;
	case DSC_1080_2400_540_2400:
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_GRP_SIZE), 0x000000b4);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_SLICE_SIZE), 0x04069780);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG0), 0x306c8200);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG1), 0x0007e13f);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG2), 0x000b000b);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG3), 0x10f01800);
		break;

	default:
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_GRP_SIZE), 0x000000f0);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_SLICE_SIZE), 0x04096000);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG1), 0x000ae4bd);
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CFG2), 0x0008000a);
		break;
	}

	if (dpu->dsi->ctx.work_mode == DSI_MODE_CMD)
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CTRL), 0x2000010b);
	else
		DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_CTRL), 0x2000000b);

	return 0;
}

static int dpu_init(struct dpu_context *ctx)
{
	u32 reg_val, size;
	struct sprd_dpu *dpu = (struct sprd_dpu *)container_of(ctx, struct sprd_dpu, ctx);

	dpu_get_dsc_cfg(ctx);

	if (ctx->dual_dsi_en)
		DPU_REG_WR(ctx->base + REG_DPU_MODE, BIT(0));

	if (ctx->dsc_en)
		dpu_config_dsc_param(ctx);

	/* set bg color */
	DPU_REG_WR(ctx->base + REG_BG_COLOR, 0x00);

	/* set dpu output size */
	size = (ctx->vm.vactive << 16) | ctx->vm.hactive;
	DPU_REG_WR(ctx->base + REG_PANEL_SIZE, size);
	DPU_REG_WR(ctx->base + REG_BLEND_SIZE, size);

	DPU_REG_WR(ctx->base + REG_DPU_CFG0, 0x00);
	if ((dpu->dsi->ctx.work_mode == DSI_MODE_CMD) && ctx->dsc_en) {
		DPU_REG_SET(ctx->base + REG_DPU_CFG0, BIT(1));
		ctx->is_single_run = true;
	}

	reg_val = (qos_cfg.awqos_high << 12) |
		(qos_cfg.awqos_low << 8) |
		(qos_cfg.arqos_high << 4) |
		(qos_cfg.arqos_low) | BIT(18) | BIT(22) | BIT(23);
	DPU_REG_WR(ctx->base + REG_DPU_CFG1, reg_val);;
	if (ctx->stopped)
		dpu_clean_all(ctx);

	DPU_REG_WR(ctx->base + REG_DPU_INT_CLR, 0xffff);

	dpu_write_back_config(ctx);

	ctx->base_offset[0] = 0x0;
	ctx->base_offset[1] = DPU_MAX_REG_OFFSET;

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

static u32 dpu_img_ctrl(u32 format, u32 blending, u32 compression, u32 y2r_coef,
		u32 rotation)
{
	int reg_val = 0;
	/* layer enable */
	// reg_val |= BIT_DPU_LAY_EN;

	switch (format) {
	case DRM_FORMAT_BGRA8888:
		/* BGRA8888 -> ARGB8888 */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0;
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= (BIT_DPU_LAY_FORMAT_XFBC_ARGB8888);
		else
			reg_val |= (BIT_DPU_LAY_FORMAT_ARGB8888);
		break;
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
		/* RGBA8888 -> ABGR8888 */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0;
		/* FALLTHRU */
	case DRM_FORMAT_ABGR8888:
		/* rb switch */
		reg_val |= BIT_DPU_LAY_RGB888_RB_SWITCH;
		/* FALLTHRU */
	case DRM_FORMAT_ARGB8888:
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= (BIT_DPU_LAY_FORMAT_XFBC_ARGB8888);
		else
			reg_val |= (BIT_DPU_LAY_FORMAT_ARGB8888);
		break;
	case DRM_FORMAT_XBGR8888:
		/* rb switch */
		reg_val |= BIT_DPU_LAY_RGB888_RB_SWITCH;
		/* FALLTHRU */
	case DRM_FORMAT_XRGB8888:
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= (BIT_DPU_LAY_FORMAT_XFBC_ARGB8888);
		else
			reg_val |= (BIT_DPU_LAY_FORMAT_ARGB8888);
		break;
	case DRM_FORMAT_BGR565:
		/* rb switch */
		reg_val |= BIT_DPU_LAY_RGB565_RB_SWITCH;
		/* FALLTHRU */
	case DRM_FORMAT_RGB565:
		if (compression)
			/* XFBC-RGB565 */
			reg_val |= (BIT_DPU_LAY_FORMAT_XFBC_RGB565);
		else
			reg_val |= (BIT_DPU_LAY_FORMAT_RGB565);
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
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0 << 2;
		break;
	case DRM_FORMAT_NV16:
		/*2-Lane: Yuv422 */
		reg_val |= BIT_DPU_LAY_FORMAT_YUV422_2PLANE;
		/*Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0;
		/*UV endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0 << 2;
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
		reg_val &= (~BIT_DPU_LAY_MODE_BLEND_NORMAL);
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

	DPU_REG_WR(ctx->base + REG_LAYER_ENABLE, 0);
}

static void dpu_bgcolor(struct dpu_context *ctx, u32 color)
{

	if (ctx->if_type == SPRD_DPU_IF_EDPI)
		dpu_wait_stop_done(ctx);

	DPU_REG_WR(ctx->base + REG_BG_COLOR, color);

	dpu_clean_all(ctx);

	if (ctx->is_single_run) {
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT(4));
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT(0));
	} else if (ctx->if_type == SPRD_DPU_IF_EDPI) {
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_RUN);
		ctx->stopped = false;
	} else if ((ctx->if_type == SPRD_DPU_IF_DPI) && !ctx->stopped) {
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_LAY_REG_UPDATE);
		dpu_wait_update_done(ctx);
	}
}

static void dpu_layer(struct dpu_context *ctx,
		    struct sprd_layer_state *hwlayer)
{
	const struct drm_format_info *info;
	struct layer_reg tmp = {};
	u32 dst_size, src_size, offset, wd, rot;
	int i;

	offset = (hwlayer->dst_x & 0xffff) | ((hwlayer->dst_y) << 16);
	src_size = (hwlayer->src_w & 0xffff) | ((hwlayer->src_h) << 16);
	dst_size = (hwlayer->dst_w & 0xffff) | ((hwlayer->dst_h) << 16);

	if (hwlayer->pallete_en) {
		tmp.pos = offset;
		tmp.src_size = src_size;
		tmp.dst_size = dst_size;
		tmp.alpha = hwlayer->alpha;
		tmp.pallete = hwlayer->pallete_color;

		/* pallete layer enable */
		tmp.ctrl = 0x2004;
		pr_debug("dst_x = %d, dst_y = %d, dst_w = %d, dst_h = %d, pallete:%d\n",
				hwlayer->dst_x, hwlayer->dst_y,
				hwlayer->dst_w, hwlayer->dst_h, tmp.pallete);
	} else {
		if (src_size != dst_size) {
			rot = to_dpu_rotation(hwlayer->rotation);
			if ((rot == DPU_LAYER_ROTATION_90) || (rot == DPU_LAYER_ROTATION_270) ||
					(rot == DPU_LAYER_ROTATION_90_M) || (rot == DPU_LAYER_ROTATION_270_M))
				dst_size = (hwlayer->dst_h & 0xffff) | ((hwlayer->dst_w) << 16);
			tmp.ctrl = 0;
		}

		for (i = 0; i < hwlayer->planes; i++) {
			if (hwlayer->addr[i] % 16)
				pr_err("layer addr[%d] is not 16 bytes align, it's 0x%08x\n",
						i, hwlayer->addr[i]);
			tmp.addr[i] = hwlayer->addr[i];
		}

		tmp.pos = offset;
		tmp.src_size = src_size;
		tmp.dst_size = dst_size;
		tmp.crop_start = (hwlayer->src_y << 16) | hwlayer->src_x;
		tmp.alpha = hwlayer->alpha;

		info = drm_format_info(hwlayer->format);
		wd = info->cpp[0];
		if (wd == 0) {
			pr_err("layer[%d] bytes per pixel is invalid\n", hwlayer->index);
			return;
		}

		if (hwlayer->planes == 3)
			/* UV pitch is 1/2 of Y pitch*/
			tmp.pitch = (hwlayer->pitch[0] / wd) |
				(hwlayer->pitch[0] / wd << 15);
		else
			tmp.pitch = hwlayer->pitch[0] / wd;

		tmp.ctrl |= dpu_img_ctrl(hwlayer->format, hwlayer->blending,
				hwlayer->xfbc, hwlayer->y2r_coef, hwlayer->rotation);
	}

	for (i = 0; i < hwlayer->planes; i++)
		DPU_REG_WR(ctx->base + DPU_LAY_PLANE_ADDR(REG_LAY_BASE_ADDR,
					hwlayer->index, i), tmp.addr[i]);

	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_POS,
			hwlayer->index), tmp.pos);
	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_SRC_SIZE,
			hwlayer->index), tmp.src_size);
	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_DES_SIZE,
			hwlayer->index), tmp.dst_size);
	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_CROP_START,
			hwlayer->index), tmp.crop_start);
	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_ALPHA,
			hwlayer->index), tmp.alpha);
	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_PITCH,
			hwlayer->index), tmp.pitch);
	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_CTRL,
			hwlayer->index), tmp.ctrl);
	DPU_REG_SET(ctx->base + REG_LAYER_ENABLE,
			(1 << hwlayer->index));
	// DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_PALLETE,
				// hwlayer->index), tmp.pallete);

	pr_debug("dst_x = %d, dst_y = %d, dst_w = %d, dst_h = %d\n",
				hwlayer->dst_x, hwlayer->dst_y,
				hwlayer->dst_w, hwlayer->dst_h);
	pr_debug("start_x = %d, start_y = %d, start_w = %d, start_h = %d\n",
				hwlayer->src_x, hwlayer->src_y,
				hwlayer->src_w, hwlayer->src_h);
}

static int dpu_vrr(struct dpu_context *ctx)
{
	struct sprd_dpu *dpu = (struct sprd_dpu *)container_of(ctx,
			struct sprd_dpu, ctx);
	u32 reg_val;

	if (ctx->fps_mode_changed) {
		dpu_stop(ctx);
		reg_val = (ctx->vm.vsync_len << 0) |
			(ctx->vm.vback_porch << 8) |
			(ctx->vm.vfront_porch << 20);
		DPU_REG_WR(ctx->base + REG_DPI_V_TIMING, reg_val);

		reg_val = (ctx->vm.hsync_len << 0) |
			(ctx->vm.hback_porch << 8) |
			(ctx->vm.hfront_porch << 20);
		DPU_REG_WR(ctx->base + REG_DPI_H_TIMING, reg_val);

		if (ctx->dsc_en) {
			reg_val = (ctx->vm.vsync_len << 0) |
				(ctx->vm.vback_porch  << 8) |
				(ctx->vm.vfront_porch << 20);
			DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_V_TIMING), reg_val);

			reg_val = (ctx->vm.hsync_len << 0) |
				(ctx->vm.hback_porch << 8) |
				(ctx->vm.hfront_porch << 20);
			DPU_REG_WR(ctx->base + DSC_REG(REG_DSC_H_TIMING), reg_val);
		}
		sprd_dsi_vrr_timing(dpu->dsi);
		reg_val = DPU_REG_RD(ctx->base + REG_DPU_CTRL);
		reg_val |= BIT(0) | BIT(4);
		DPU_REG_WR(ctx->base + REG_DPU_CTRL, reg_val);
		dpu_wait_update_done(ctx);
		ctx->stopped = false;
		DPU_REG_WR(ctx->base + REG_DPU_MMU0_UPDATE, 1);
		ctx->fps_mode_changed = false;
	}

	return 0;
}

static void dpu_scaling(struct dpu_context *ctx,
		struct sprd_plane planes[], u8 count)
{
	int i;
	u16 src_w;
	u16 src_h;
	u32 reg_val;
	struct sprd_layer_state *layer_state;
	struct sprd_plane_state *plane_state;
	struct scale_config_param *scale_cfg = &ctx->scale_cfg;

	if (scale_cfg->sr_mode_changed) {
		pr_debug("------------------------------------\n");
		for (i = 0; i < count; i++) {
			plane_state = to_sprd_plane_state(planes[i].base.state);
			layer_state = &plane_state->layer;
			pr_debug("layer[%d] : %dx%d --- (%d)\n", i,
					layer_state->dst_w, layer_state->dst_h,
					scale_cfg->in_w);
			if (layer_state->dst_w != scale_cfg->in_w) {
				scale_cfg->skip_layer_index = i;
				break;
			}
		}

		plane_state = to_sprd_plane_state(planes[count - 1].base.state);
		layer_state = &plane_state->layer;
		if  (layer_state->dst_w <= scale_cfg->in_w) {
			dpu_sr_config(ctx);
			scale_cfg->sr_mode_changed = false;
			pr_info("do scaling enhace, bottom layer(%dx%d)\n",
					layer_state->dst_w, layer_state->dst_h);
		}
	} else {
		if (count == 1) {
			plane_state = to_sprd_plane_state(planes[count - 1].base.state);
			layer_state = &plane_state->layer;
			// btm_layer = &layers[count - 1];
			if (layer_state->rotation & (DRM_MODE_ROTATE_90 |
						DRM_MODE_ROTATE_270)) {
				src_w = layer_state->src_h;
				src_h = layer_state->src_w;
			} else {
				src_w = layer_state->src_w;
				src_h = layer_state->src_h;
			}
			if (src_w == layer_state->dst_w
					&& src_h == layer_state->dst_h) {
				reg_val = (scale_cfg->in_h << 16) |
					scale_cfg->in_w;
				DPU_REG_WR(ctx->base + REG_BLEND_SIZE, reg_val);
				if (!scale_cfg->need_scale) {
					DPU_REG_CLR(ctx->base + REG_DPU_SCL_EN, BIT_DPU_SCALING_EN);
				} else {
					DPU_REG_SET(ctx->base + REG_DPU_SCL_EN, BIT_DPU_SCALING_EN);
				}
			} else {
				/*
				 * When the layer src size is not euqal to the
				 * dst size, screened by dpu hal,the single
				 * layer need to scaling-up. Regardless of
				 * whether the SR function is turned on, dpu
				 * blend size should be set to the layer src
				 * size.
				 */
				reg_val = (src_h << 16) | src_w;
				DPU_REG_WR(ctx->base + REG_BLEND_SIZE, reg_val);
				/*
				 * When the layer src size is equal to panel
				 * size, close dpu scaling-up function.
				 */
				if (src_h == ctx->vm.vactive &&
						src_w == ctx->vm.hactive) {
					DPU_REG_CLR(ctx->base + REG_DPU_SCL_EN, BIT_DPU_SCALING_EN);
				} else {
					DPU_REG_SET(ctx->base + REG_DPU_SCL_EN, BIT_DPU_SCALING_EN);
				}
			}
		} else {
			reg_val = (scale_cfg->in_h << 16) |
				scale_cfg->in_w;
			DPU_REG_WR(ctx->base + REG_BLEND_SIZE, reg_val);
			if (!scale_cfg->need_scale)
				DPU_REG_CLR(ctx->base + REG_DPU_SCL_EN, BIT_DPU_SCALING_EN);
			else
				DPU_REG_SET(ctx->base + REG_DPU_SCL_EN, BIT_DPU_SCALING_EN);
		}
	}
}

static void dpu_flip(struct dpu_context *ctx,
		     struct sprd_plane planes[], u8 count)
{
	int i;
	u32 reg_val;
	struct sprd_plane_state *state;
	struct sprd_dpu *dpu = container_of(ctx, struct sprd_dpu, ctx);

	ctx->vsync_count = 0;
	if (ctx->max_vsync_count > 0 && count > 1)
		ctx->wb_en = true;
	/*
	 * Make sure the dpu is in stop status. DPU_r6p0 has no shadow
	 * registers in EDPI mode. So the config registers can only be
	 * updated in the rising edge of DPU_RUN bit.
	 */
	if (ctx->if_type == SPRD_DPU_IF_EDPI)
		dpu_wait_stop_done(ctx);

	/* reset the bgcolor to black */
	DPU_REG_WR(ctx->base + REG_BG_COLOR, 0x00);

	 /* to check if dpu need change the frame rate */
	dpu_vrr(ctx);

	/* disable all the layers */
	dpu_clean_all(ctx);

	/* to check if dpu need scaling the frame for SR */
	if (!dpu->dsi->ctx.surface_mode)
		dpu_scaling(ctx, planes, count);

	/* start configure dpu layers */
	for (i = 0; i < count; i++) {
		state = to_sprd_plane_state(planes[i].base.state);
		dpu_layer(ctx, &state->layer);
	}
	/* update trigger and wait */
	if (ctx->is_single_run) {
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT(4));
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT(0));
	} else if (ctx->if_type == SPRD_DPU_IF_EDPI) {
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_RUN);
		ctx->stopped = false;
	} else if (ctx->if_type == SPRD_DPU_IF_DPI) {
		if (!ctx->stopped) {
			DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_LAY_REG_UPDATE);
			dpu_wait_update_done(ctx);
		}

		DPU_REG_SET(ctx->base + REG_DPU_INT_EN, BIT_DPU_INT_ERR);
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

		/* enable Halt function for SPRD DSI */
		DPU_REG_SET(ctx->base + REG_DPI_CTRL, BIT_DPU_DPI_HALT_EN);

		if (ctx->is_single_run)
			DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT(0));

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
		int_mask |= BIT_DPU_INT_DPU_ALL_UPDATE_DONE;
		int_mask |= BIT_DPU_INT_DPU_REG_UPDATE_DONE;
		int_mask |= BIT_DPU_INT_LAY_REG_UPDATE_DONE;
		int_mask |= BIT_DPU_INT_PQ_REG_UPDATE_DONE;
		/* enable dpu DONE  INT */
		int_mask |= BIT_DPU_INT_DONE;
		/* enable dpu dpi vsync */
		int_mask |= BIT_DPU_INT_VSYNC_EN;
		/* enable dpu TE INT */
		int_mask |= BIT_DPU_INT_TE;
		/* enable underflow err INT */
		int_mask |= BIT_DPU_INT_ERR;
		/* enable write back done INT */
		int_mask |= BIT_DPU_INT_WB_DONE_EN;
		/* enable write back fail INT */
		int_mask |= BIT_DPU_INT_WB_ERR_EN;

	} else if (ctx->if_type == SPRD_DPU_IF_EDPI) {
		/* use edpi as interface */
		DPU_REG_SET(ctx->base + REG_DPU_CFG0, BIT_DPU_IF_EDPI);

		/* use external te */
		DPU_REG_SET(ctx->base + REG_DPI_CTRL, BIT_DPU_EDPI_FROM_EXTERNAL_PAD);;

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
	DPU_REG_SET(ctx->base + REG_DPU_INT_EN, BIT_DPU_INT_VSYNC_EN);
}

static void disable_vsync(struct dpu_context *ctx)
{
	DPU_REG_CLR(ctx->base + REG_DPU_INT_EN, BIT_DPU_INT_VSYNC_EN);
}

static int dpu_context_init(struct dpu_context *ctx)
{
	struct dpu_enhance *enhance;

	enhance = kzalloc(sizeof(*enhance), GFP_KERNEL);
	if (!enhance) {
		pr_err("%s() enhance kzalloc failed!\n", __func__);
		return -ENOMEM;
	}
	ctx->enhance = enhance;

	ctx->base_offset[0] = 0x0;
	ctx->base_offset[1] = DPU_MAX_REG_OFFSET;

	return 0;
}

static void dpu_sr_config(struct dpu_context *ctx)
{
	struct scale_config_param *scale_cfg = &ctx->scale_cfg;
	u32 reg_val;

	reg_val = (scale_cfg->in_h << 16) | scale_cfg->in_w;
	DPU_REG_WR(ctx->base + REG_BLEND_SIZE, reg_val);
	if (scale_cfg->need_scale)
		DPU_REG_SET(ctx->base + REG_DPU_SCL_EN, BIT_DPU_SCALING_EN);
	else
		DPU_REG_CLR(ctx->base + REG_DPU_SCL_EN, BIT_DPU_SCALING_EN);
}

static int dpu_modeset(struct dpu_context *ctx,
		struct drm_display_mode *mode)
{
	struct scale_config_param *scale_cfg = &ctx->scale_cfg;
	struct sprd_dpu *dpu = container_of(ctx, struct sprd_dpu, ctx);
	struct sprd_dsi *dsi = dpu->dsi;
	static unsigned int now_vtotal;
	static unsigned int now_htotal;

	scale_cfg->in_w = mode->hdisplay;
	scale_cfg->in_h = mode->vdisplay;

	if ((mode->hdisplay != ctx->vm.hactive) ||
		(mode->vdisplay != ctx->vm.vactive)) {
		scale_cfg->need_scale = true;
		scale_cfg->sr_mode_changed = true;
	} else {
		if (!now_htotal && !now_vtotal) {
			now_htotal = ctx->vm.hactive + ctx->vm.hfront_porch +
				ctx->vm.hback_porch + ctx->vm.hsync_len;
			now_vtotal = ctx->vm.vactive + ctx->vm.vfront_porch +
				ctx->vm.vback_porch + ctx->vm.vsync_len;
		}

		if ((mode->vtotal + mode->htotal) !=
			(now_htotal + now_vtotal)) {
			drm_display_mode_to_videomode(mode, &ctx->vm);
			drm_display_mode_to_videomode(mode, &dsi->ctx.vm);
			now_htotal = ctx->vm.hactive + ctx->vm.hfront_porch +
				ctx->vm.hback_porch + ctx->vm.hsync_len;
			now_vtotal = ctx->vm.vactive + ctx->vm.vfront_porch +
				ctx->vm.vback_porch + ctx->vm.vsync_len;

			ctx->fps_mode_changed = true;
		} else {
			scale_cfg->sr_mode_changed = true;
		}

		scale_cfg->need_scale = false;
	}

	ctx->wb_size_changed = true;
	pr_info("begin switch to %u x %u\n", mode->hdisplay, mode->vdisplay);

	return 0;
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
	cap->max_layers = 6;
	cap->fmts_ptr = primary_fmts;
	cap->fmts_cnt = ARRAY_SIZE(primary_fmts);
}

const struct dpu_core_ops dpu_r6p0_core_ops = {
	.version = dpu_version,
	.parse_dt = dpu_parse_dt,
	.init = dpu_init,
	.fini = dpu_fini,
	.run = dpu_run,
	.stop = dpu_stop,
	.isr = dpu_isr,
	.ifconfig = dpu_dpi_init,
	.capability = dpu_capability,
	.bg_color = dpu_bgcolor,
	.flip = dpu_flip,
	.enable_vsync = enable_vsync,
	.disable_vsync = disable_vsync,
	.context_init = dpu_context_init,
	.modeset = dpu_modeset,
	.write_back = dpu_wb_trigger,
};
