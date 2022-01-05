/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/module.h>
#include "sprd_corner.h"

#define STEP (256)

#define USE_EXTERNAL_SOURCE 0

#if (USE_EXTERNAL_SOURCE)
static unsigned char layer_top_header[] = {
#include "lcd_top_corner.h"
};
static unsigned char layer_bottom_header[] = {
#include "lcd_bottom_corner.h"
};
#endif

struct sprd_layer_state corner_layer_top = {
	.planes = 1,
	.xfbc = 0,
	.format = DRM_FORMAT_ABGR8888,
	.blending = DRM_MODE_BLEND_COVERAGE,
	.alpha = 0xff,
};

struct sprd_layer_state corner_layer_bottom = {
	.planes = 1,
	.xfbc = 0,
	.format = DRM_FORMAT_ABGR8888,
	.blending = DRM_MODE_BLEND_COVERAGE,
	.alpha = 0xff,
};

static int sprd_corner_create(struct dpu_context *ctx)
{
	int buf_size;

	buf_size = ctx->vm.vactive * ctx->vm.hactive * 4;
	ctx->layer_top = (u32 *)__get_free_pages(GFP_KERNEL |
		GFP_DMA | __GFP_ZERO, get_order(buf_size));
	ctx->layer_bottom = (u32 *)__get_free_pages(GFP_KERNEL |
		GFP_DMA | __GFP_ZERO, get_order(buf_size));
	if (NULL == ctx->layer_top || NULL == ctx->layer_bottom) {
		DRM_ERROR("%s(): get_free_pages is NULL\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

void sprd_corner_destroy(struct dpu_context *ctx)
{
	kfree(ctx->layer_top);
	kfree(ctx->layer_bottom);
}

static unsigned int gdi_sqrt(unsigned int x)
{
	unsigned int root = 0;
	unsigned int seed = (1 << 30);
	while (seed > x) {
		seed >>= 2;
	}

	while (seed != 0) {
		if (x >= seed + root) {
			x -= seed + root;
			root += seed * 2;
		}
		root >>= 1;
		seed >>= 2;
	}

	return root;
}

static void draw_sector(unsigned int *alpha, int width, int height,
		int center_x, int center_y, int radius)
{
	int x = 0;
	int y = 0;
	int line = 0;
	unsigned int *puiAlpha = NULL;

	for (y = max(0, center_y + 1); y < min(center_y + radius * 707 / 1000 + 1,
				height); y++) {
		line = gdi_sqrt((radius * STEP) * (radius * STEP) - ((y - center_y) * STEP)
				* ((y - center_y) * STEP));
		puiAlpha = &alpha[y * width + center_x + line / STEP];
		if (center_x + line / STEP >= 0 && center_x + line / STEP < width) {
			*puiAlpha = (0xff - ((line % STEP) * 0x01)) << 24;
			puiAlpha++;
		}

		x = max(0, center_x + line / STEP + 1);
		puiAlpha = &alpha[y * width + x];
		for (; x < min(center_x + radius, width); x++) {
			*puiAlpha = (0xff - 0) << 24;
			puiAlpha++;
		}
	}

	for (x = max(0, center_x + 1); x < min(center_x + radius * 707 / 1000 + 1,
				width); x++) {
		line = gdi_sqrt((radius * STEP) * (radius * STEP) - ((x - center_x) * STEP)
				* ((x - center_x) * STEP));
		puiAlpha = &alpha[(center_y + line / STEP) * width + x];
		if (center_y + line / STEP >= 0 && center_y + line / STEP < height) {
			*puiAlpha = (0xff - ((line % STEP) * 0x01)) << 24;
			puiAlpha += width;
		}

		y = max(0, center_y + line / STEP + 1);
		puiAlpha = &alpha[y * width + x];
		for (; y < min(center_y + radius, height); y++) {
			*puiAlpha = (0xff - 0) << 24;
			puiAlpha += width;
		}
	}

	for (y = max(center_y + radius * 707 / 1000 + 1, 0); y < min(center_y + radius,
				height); y++) {
		x = max(center_x + radius * 707 / 1000 + 1, 0);
		puiAlpha = &alpha[y * width + x];
		for (; x < min(center_x + radius, width); x++) {
			*puiAlpha = (0xff - 0) << 24;
			puiAlpha++;
		}
	}
}

void  sprd_corner_draw(unsigned int *corner, int radius, int width)
{
	int i, j;

	draw_sector(corner, width, radius, width - radius, 0, radius);
	for (i = 0; i < radius; i++) {
		for (j = 0; j < radius; j++)
			corner[i * width + j] = corner[(i + 1) * width - j - 1];
	}
}

void sprd_corner_x_mirrored(unsigned int *dst, unsigned int *src,
		int width, int radius)
{
	int i, j;
	for (i = 0; i < radius; i++) {
		for (j = 0; j < radius; j++)
			dst[(radius - 1 - i) * width + j] = src[i * width + j];
	}

	for (i = 0; i < radius; i++) {
		for (j = width - radius; j < width; j++)
			dst[(radius - 1 - i) * width + j] = src[i * width + j];
	}
}

int sprd_corner_hwlayer_init(struct dpu_context *ctx)
{
	int ret;
	int corner_radius = ctx->sprd_corner_radius;
	unsigned int *layer_top = ctx->layer_top;
	unsigned int *layer_bottom = ctx->layer_bottom;

	ret = sprd_corner_create(ctx);
	if (ret < 0) {
		DRM_ERROR("%s(): sprd_corner_create failed\n", __func__);
		return -ENOMEM;
	}

#if USE_EXTERNAL_SOURCE
	memcpy(layer_top, layer_top_header, ctx->vm.hactive * corner_radius * 4);
	memcpy(layer_bottom, layer_bottom_header, ctx->vm.hactive * corner_radius * 4);
#else
	sprd_corner_draw(layer_bottom, corner_radius, ctx->vm.hactive);
	sprd_corner_x_mirrored(layer_top, layer_bottom, ctx->vm.hactive, corner_radius);
#endif

	corner_layer_top.dst_x = 0;
	corner_layer_top.dst_y = 0;
	corner_layer_top.dst_w = ctx->vm.hactive;
	corner_layer_top.dst_h = corner_radius;
	corner_layer_top.pitch[0] = ctx->vm.hactive * 4;
	corner_layer_top.addr[0] = (u32)virt_to_phys(layer_top);

	corner_layer_bottom.dst_x = 0;
	corner_layer_bottom.dst_y = ctx->vm.vactive - corner_radius;
	corner_layer_bottom.dst_w = ctx->vm.hactive;
	corner_layer_bottom.dst_h = corner_radius;
	corner_layer_bottom.pitch[0] = ctx->vm.hactive * 4;
	corner_layer_bottom.addr[0] = (u32)virt_to_phys(layer_bottom);

	return 0;
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("infi.chen <infi.chen@unisoc.com>");
MODULE_AUTHOR("shenhui.sun <shenhui.sun@unisoc.com>");
