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

#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

struct sprd_panel {
	struct drm_panel base;
	struct mipi_dsi_device *dsi;

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

static const struct drm_display_mode default_mode = {
	.clock = 153600, /*153.6Mhz*/

	.hdisplay = 1080,
	.hsync_start = 1080 + 176,
	.hsync_end = 1080 + 176 + 10,
	.htotal = 1080 + 176 + 10 + 16,

	.vdisplay = 1920,
	.vsync_start = 1920 + 32,
	.vsync_end = 1920 + 32 + 4,
	.vtotal = 1920 + 32 + 4 + 32,
};

static int sprd_panel_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;

	DRM_INFO("drm_panel_funcs->get_modes()\n");

	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		DRM_ERROR("failed to add mode %ux%ux@%u\n",
			  default_mode.hdisplay, default_mode.vdisplay,
			  default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	drm_mode_probed_add(panel->connector, mode);

	panel->connector->display_info.width_mm = 68;
	panel->connector->display_info.height_mm = 121;

	return 1;
}

static const struct drm_panel_funcs sprd_panel_funcs = {
	.get_modes = sprd_panel_get_modes,
	.enable = sprd_panel_enable,
	.disable = sprd_panel_disable,
	.prepare = sprd_panel_prepare,
	.unprepare = sprd_panel_unprepare,
};

static int sprd_panel_parse_dt(struct sprd_panel *panel)
{

	return 0;
}

static int sprd_panel_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct sprd_panel *panel;
	int ret;

	panel = devm_kzalloc(dev, sizeof(*panel), GFP_KERNEL);
	if (!panel)
		return -ENOMEM;

	panel->dsi = dsi;
	ret = sprd_panel_parse_dt(panel);
	if (ret)
		return ret;

	drm_panel_init(&panel->base);

	panel->base.dev = dev;
	panel->base.funcs = &sprd_panel_funcs;
	ret = drm_panel_add(&panel->base);
	if (ret) {
		DRM_INFO("drm_panel_add() failed\n");
		return ret;
	}

//	dsi->phy_clock = 1000000; /* in kbps */
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO |
			  MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_VIDEO_HSE |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS |
			  MIPI_DSI_MODE_LPM;

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
