/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <video/mipi_display.h>
#include <video/videomode.h>
#include <video/display_timing.h>
#include <video/of_display_timing.h>

#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

static uint32_t lcd_id_from_uboot;
static int __init lcd_id_get(char *str)
{
	int len = 0;

	if ((str != NULL) && (str[0] == 'I') && (str[1] == 'D'))
		len = kstrtou32(str+2, 16, &lcd_id_from_uboot);
	pr_info("LCD ID from uboot: 0x%x\n", lcd_id_from_uboot);
	return 0;
}
__setup("lcd_id=", lcd_id_get);

struct rgb_timing {
	uint16_t hfp;
	uint16_t hbp;
	uint16_t hsync;
	uint16_t vfp;
	uint16_t vbp;
	uint16_t vsync;
};

struct panel_info {
	/* common parameters */
	const char *lcd_name;
	uint16_t width;
	uint16_t height;
	uint16_t width_mm;
	uint16_t height_mm;

	/* DPI specific parameters */
	struct display_timing display_timing;
	struct rgb_timing rgb_timing;
	struct videomode video_mode;
	struct drm_display_mode drm_mode;

	/* MIPI DSI specific parameters */
	uint32_t phy_freq;
	uint8_t lane_num;
};

struct sprd_panel {
	struct drm_panel base;
	struct mipi_dsi_device *dsi;
	struct panel_info *info;

	bool prepared;
	bool enabled;
};

static int nt35695_power(int on)
{
	return 0;
}

static inline struct sprd_panel *to_sprd_panel(struct drm_panel *panel)
{
	return container_of(panel, struct sprd_panel, base);
}

static int sprd_panel_unprepare(struct drm_panel *p)
{
	struct sprd_panel *panel = to_sprd_panel(p);

	DRM_INFO("drm_panel_funcs->unprepare()\n");
	if (!panel->prepared)
		return 0;

	nt35695_power(false);

	panel->prepared = false;

	return 0;
}

static int sprd_panel_prepare(struct drm_panel *p)
{
	struct sprd_panel *panel = to_sprd_panel(p);
	int ret = 0;

	DRM_INFO("drm_panel_funcs->prepare()\n");
	if (panel->prepared)
		return 0;

	nt35695_power(true);

	if (ret < 0)
		return ret;

	panel->prepared = true;

	return 0;
}

static int sprd_panel_disable(struct drm_panel *p)
{
	struct sprd_panel *panel = to_sprd_panel(p);
	int ret = 0;

	DRM_INFO("drm_panel_funcs->disable()\n");
	if (!panel->enabled)
		return 0;

//	mipi_panel_sleep_in(panel);

	if (ret < 0)
		return ret;

	panel->enabled = false;

	return 0;
}

static int sprd_panel_enable(struct drm_panel *p)
{
	struct sprd_panel *panel = to_sprd_panel(p);

	DRM_INFO("drm_panel_funcs->enable()\n");
	if (panel->enabled)
		return 0;

	/* init the panel */
//	mipi_panel_init(panel);

	panel->enabled = true;

	return 0;
}

static int sprd_panel_get_modes(struct drm_panel *p)
{
	struct drm_display_mode *mode;
	struct sprd_panel *panel = to_sprd_panel(p);

	DRM_INFO("drm_panel_funcs->get_modes()\n");

	mode = drm_mode_duplicate(p->drm, &panel->info->drm_mode);
	if (!mode) {
		DRM_ERROR("failed to add mode %ux%ux@%u\n",
			  panel->info->drm_mode.hdisplay,
			  panel->info->drm_mode.vdisplay,
			  panel->info->drm_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	drm_mode_probed_add(p->connector, mode);

	p->connector->display_info.width_mm = panel->info->drm_mode.width_mm;
	p->connector->display_info.height_mm = panel->info->drm_mode.width_mm;

	return 1;
}

static const struct drm_panel_funcs sprd_panel_funcs = {
	.get_modes = sprd_panel_get_modes,
	.enable = sprd_panel_enable,
	.disable = sprd_panel_disable,
	.prepare = sprd_panel_prepare,
	.unprepare = sprd_panel_unprepare,
};

static struct panel_info *sprd_panel_parse_dt(struct device_node *np)
{
	int rc;
	unsigned int temp;
	struct panel_info *info;
	const char *panel_name;
	int panel_id = lcd_id_from_uboot;

	if (!of_property_read_u32(np, "force-id", &temp)) {
		DRM_ERROR("warning: use force id 0x%x\n", temp);
		panel_id = temp;
	}

	if (panel_id == 0) {
		DRM_ERROR("LCD_ID from uboot is 0, enter Calibration Mode\n");
		return NULL;
	}

	info = kzalloc(sizeof(struct panel_info), GFP_KERNEL);
	if (info == NULL)
		return NULL;

	rc = of_property_read_u32(np, "sprd,lane-number", &temp);
	if (!rc)
		info->lane_num = temp;
	else
		info->lane_num = 4;

	rc = of_property_read_u32(np, "sprd,width-mm", &temp);
	if (!rc)
		info->width_mm = temp;
	else
		info->width_mm = 68;

	rc = of_property_read_u32(np, "sprd,height-mm", &temp);
	if (!rc)
		info->height_mm = temp;
	else
		info->height_mm = 121;

	rc = of_property_read_string(np, "sprd,panel-name", &panel_name);
	if (!rc)
		info->lcd_name = panel_name;

	if (of_get_display_timing(np, "display-timings",
				  &info->display_timing))
		DRM_ERROR("get display timing failed\n");
	else {
		struct display_timing *dt = &info->display_timing;
		struct rgb_timing *t = &info->rgb_timing;

		t->hfp = dt->hfront_porch.typ;
		t->hbp = dt->hback_porch.typ;
		t->hsync = dt->hsync_len.typ;

		t->vfp = dt->vfront_porch.typ;
		t->vbp = dt->vback_porch.typ;
		t->vsync = dt->vsync_len.typ;

		info->phy_freq = dt->pixelclock.typ;
		info->width = dt->hactive.typ;
		info->height = dt->vactive.typ;
	}

	DRM_INFO("lcd_name = %s\n", info->lcd_name);
	DRM_INFO("lane_number = %d\n", info->lane_num);
	DRM_INFO("phy_freq = %d\n", info->phy_freq);
	DRM_INFO("resolution: %d x %d\n", info->width, info->height);

	return info;
}

static int sprd_panel_probe(struct mipi_dsi_device *dsi)
{
	int ret;
	struct device *dev = &dsi->dev;
	struct sprd_panel *panel;
	struct panel_info *info;

	panel = devm_kzalloc(dev, sizeof(struct sprd_panel), GFP_KERNEL);
	if (!panel)
		return -ENOMEM;

	info = sprd_panel_parse_dt(dev->of_node);
	if (info == NULL) {
		DRM_ERROR("parse panel info failed\n");
		return -EINVAL;
	}

	panel->info = info;
	panel->dsi = dsi;
	drm_panel_init(&panel->base);

	panel->base.dev = dev;
	panel->base.funcs = &sprd_panel_funcs;
	ret = drm_panel_add(&panel->base);
	if (ret) {
		DRM_INFO("drm_panel_add() failed\n");
		return ret;
	}

	dsi->mode_flags = MIPI_DSI_MODE_VIDEO |
			  MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_VIDEO_HSE |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS |
			  MIPI_DSI_MODE_LPM;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->lanes = info->lane_num;

	videomode_from_timing(&info->display_timing, &info->video_mode);
	info->drm_mode.clock = info->video_mode.pixelclock;
	info->drm_mode.hdisplay = info->video_mode.hactive;
	info->drm_mode.hsync_start = info->drm_mode.hdisplay +
		info->video_mode.hfront_porch;
	info->drm_mode.hsync_end = info->drm_mode.hsync_start +
		info->video_mode.hsync_len;
	info->drm_mode.htotal = info->drm_mode.hsync_end +
		info->video_mode.hback_porch;
	info->drm_mode.vdisplay = info->video_mode.vactive;
	info->drm_mode.vsync_start = info->drm_mode.vdisplay +
		info->video_mode.vfront_porch;
	info->drm_mode.vsync_end = info->drm_mode.vsync_start +
		info->video_mode.vsync_len;
	info->drm_mode.vtotal = info->drm_mode.vsync_end +
		info->video_mode.vback_porch;
	info->drm_mode.width_mm = info->width_mm;
	info->drm_mode.height_mm = info->height_mm;

	ret = mipi_dsi_attach(dsi);
	if (ret) {
		DRM_ERROR("failed to attach dsi to host\n");
		drm_panel_remove(&panel->base);
		return ret;
	}

	mipi_dsi_set_drvdata(dsi, panel);

	DRM_INFO("panel probe ok\n");
	return 0;
}

static int sprd_panel_remove(struct mipi_dsi_device *dsi)
{
	struct sprd_panel *panel = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = sprd_panel_disable(&panel->base);
	if (ret < 0)
		DRM_ERROR("failed to disable panel: %d\n", ret);

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		DRM_ERROR("failed to detach from DSI host: %d\n", ret);

	drm_panel_detach(&panel->base);
	drm_panel_remove(&panel->base);

	return 0;
}

static void sprd_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct sprd_panel *panel = mipi_dsi_get_drvdata(dsi);

	sprd_panel_disable(&panel->base);
}

static const struct of_device_id panel_of_match[] = {
	{ .compatible = "sprd,generic-mipi-panel", },
	{ }
};
MODULE_DEVICE_TABLE(of, panel_of_match);

static struct mipi_dsi_driver sprd_panel_driver = {
	.driver = {
		.name = "sprd-mipi-panel-drv",
		.of_match_table = panel_of_match,
	},
	.probe = sprd_panel_probe,
	.remove = sprd_panel_remove,
	.shutdown = sprd_panel_shutdown,
};
module_mipi_dsi_driver(sprd_panel_driver);

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("SPRD MIPI DSI Panel Driver");
MODULE_LICENSE("GPL v2");
