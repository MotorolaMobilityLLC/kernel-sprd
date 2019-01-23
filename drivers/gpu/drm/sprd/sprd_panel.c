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

#include <drm/drm_atomic_helper.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <video/mipi_display.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>

#include "sprd_panel.h"
#include "dsi/sprd_dsi_api.h"
#include "sysfs/sysfs_display.h"

#define SPRD_MIPI_DSI_FMT_DSC 0xff

const char *lcd_name;
static int __init lcd_name_get(char *str)
{
	if (str != NULL)
		lcd_name = str;
	DRM_INFO("lcd name from uboot: %s\n", lcd_name);
	return 0;
}
__setup("lcd_name=", lcd_name_get);

static inline struct sprd_panel *to_sprd_panel(struct drm_panel *panel)
{
	return container_of(panel, struct sprd_panel, base);
}

static int sprd_panel_send_cmds(struct mipi_dsi_device *dsi,
				const void *data, int size)
{
	struct sprd_panel *panel = mipi_dsi_get_drvdata(dsi);
	const struct dsi_cmd_desc *cmds = data;
	u16 len;

	if ((cmds == NULL) || (dsi == NULL))
		return -EINVAL;

	while (size > 0) {
		len = (cmds->wc_h << 8) | cmds->wc_l;

		if (panel->info.use_dcs)
			mipi_dsi_dcs_write_buffer(dsi, cmds->payload, len);
		else
			mipi_dsi_generic_write(dsi, cmds->payload, len);

		if (cmds->wait)
			msleep(cmds->wait);
		cmds = (const struct dsi_cmd_desc *)(cmds->payload + len);
		size -= (len + 4);
	}

	return 0;
}

static int sprd_panel_unprepare(struct drm_panel *p)
{
	struct sprd_panel *panel = to_sprd_panel(p);
	struct gpio_timing *timing;
	int items, i;

	DRM_INFO("%s()\n", __func__);

	if (panel->info.avee_gpio) {
		gpiod_direction_output(panel->info.avee_gpio, 0);
		mdelay(5);
	}

	if (panel->info.avdd_gpio) {
		gpiod_direction_output(panel->info.avdd_gpio, 0);
		mdelay(5);
	}

	if (panel->info.reset_gpio) {
		items = panel->info.rst_off_seq.items;
		timing = panel->info.rst_off_seq.timing;
		for (i = 0; i < items; i++) {
			gpiod_direction_output(panel->info.reset_gpio,
						timing[i].level);
			mdelay(timing[i].delay);
		}
	}

	regulator_disable(panel->supply);

	return 0;
}

static int sprd_panel_prepare(struct drm_panel *p)
{
	struct sprd_panel *panel = to_sprd_panel(p);
	struct gpio_timing *timing;
	int items, i, ret;

	DRM_INFO("%s()\n", __func__);

	ret = regulator_enable(panel->supply);
	if (ret < 0)
		DRM_ERROR("enable lcd regulator failed\n");

	if (panel->info.avdd_gpio) {
		gpiod_direction_output(panel->info.avdd_gpio, 1);
		mdelay(5);
	}

	if (panel->info.avee_gpio) {
		gpiod_direction_output(panel->info.avee_gpio, 1);
		mdelay(5);
	}

	if (panel->info.reset_gpio) {
		items = panel->info.rst_on_seq.items;
		timing = panel->info.rst_on_seq.timing;
		for (i = 0; i < items; i++) {
			gpiod_direction_output(panel->info.reset_gpio,
						timing[i].level);
			mdelay(timing[i].delay);
		}
	}

	return 0;
}

static int sprd_panel_disable(struct drm_panel *p)
{
	struct sprd_panel *panel = to_sprd_panel(p);

	DRM_INFO("%s()\n", __func__);

	/*
	 * FIXME:
	 * The cancel work should be executed before DPU stop,
	 * otherwise the esd check will be failed if the DPU
	 * stopped in video mode and the DSI has not change to
	 * CMD mode yet. Since there is no VBLANK timing for
	 * LP cmd transmission.
	 */
	if (panel->esd_work_pending) {
		cancel_delayed_work_sync(&panel->esd_work);
		panel->esd_work_pending = false;
	}

	sprd_panel_send_cmds(panel->slave,
			     panel->info.cmds[CMD_CODE_SLEEP_IN],
			     panel->info.cmds_len[CMD_CODE_SLEEP_IN]);

	return 0;
}

static int sprd_panel_enable(struct drm_panel *p)
{
	struct sprd_panel *panel = to_sprd_panel(p);

	DRM_INFO("%s()\n", __func__);

	sprd_panel_send_cmds(panel->slave,
			     panel->info.cmds[CMD_CODE_INIT],
			     panel->info.cmds_len[CMD_CODE_INIT]);

	if (panel->info.esd_check_en) {
		schedule_delayed_work(&panel->esd_work,
				      msecs_to_jiffies(1000));
		panel->esd_work_pending = true;
	}

	return 0;
}

static int sprd_panel_get_modes(struct drm_panel *p)
{
	struct drm_display_mode *mode;
	struct sprd_panel *panel = to_sprd_panel(p);

	DRM_INFO("%s()\n", __func__);

	mode = drm_mode_duplicate(p->drm, &panel->info.mode);
	if (!mode) {
		DRM_ERROR("failed to add mode %ux%ux@%u\n",
			  panel->info.mode.hdisplay,
			  panel->info.mode.vdisplay,
			  panel->info.mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(p->connector, mode);

	p->connector->display_info.width_mm = panel->info.mode.width_mm;
	p->connector->display_info.height_mm = panel->info.mode.height_mm;

	return 1;
}

static const struct drm_panel_funcs sprd_panel_funcs = {
	.get_modes = sprd_panel_get_modes,
	.enable = sprd_panel_enable,
	.disable = sprd_panel_disable,
	.prepare = sprd_panel_prepare,
	.unprepare = sprd_panel_unprepare,
};

static int sprd_panel_esd_check(struct sprd_panel *panel)
{
	struct panel_info *info = &panel->info;
	u8 read_val = 0;

	/* FIXME: we should enable HS cmd tx here */
	mipi_dsi_set_maximum_return_packet_size(panel->slave, 1);
	mipi_dsi_dcs_read(panel->slave, info->esd_check_reg,
			  &read_val, 1);

	/*
	 * TODO:
	 * Should we support multi-registers check in the future?
	 */
	if (read_val != info->esd_check_val) {
		DRM_ERROR("esd check failed, read value = 0x%02x\n",
			  read_val);
		return -EINVAL;
	}

	return 0;
}

static void sprd_panel_esd_work_func(struct work_struct *work)
{
	struct sprd_panel *panel = container_of(work, struct sprd_panel,
						esd_work.work);
	struct panel_info *info = &panel->info;
	int ret;

	/*
	 * TODO:
	 * Currently, we just supprot esd_check_mode = 0, which
	 * is DDIC status register check. Please add TE check
	 * support for esd_check_mode = 1 in the future.
	 */
	ret = sprd_panel_esd_check(panel);
	if (ret) {
		const struct drm_encoder_helper_funcs *funcs;
		struct drm_encoder *encoder;

		encoder = panel->base.connector->encoder;
		funcs = encoder->helper_private;
		panel->esd_work_pending = false;

		DRM_INFO("====== esd recovery start ========\n");
		funcs->disable(encoder);
		funcs->enable(encoder);
		DRM_INFO("======= esd recovery end =========\n");
	} else
		schedule_delayed_work(&panel->esd_work,
			msecs_to_jiffies(info->esd_check_period));
}

static int sprd_panel_gpio_request(struct device *dev,
			struct sprd_panel *panel)
{
	panel->info.avdd_gpio = devm_gpiod_get_optional(dev,
					"avdd", GPIOD_ASIS);
	if (IS_ERR_OR_NULL(panel->info.avdd_gpio))
		DRM_WARN("can't get panel avdd gpio: %ld\n",
				 PTR_ERR(panel->info.avdd_gpio));

	panel->info.avee_gpio = devm_gpiod_get_optional(dev,
					"avee", GPIOD_ASIS);
	if (IS_ERR_OR_NULL(panel->info.avee_gpio))
		DRM_WARN("can't get panel avee gpio: %ld\n",
				 PTR_ERR(panel->info.avee_gpio));

	panel->info.reset_gpio = devm_gpiod_get_optional(dev,
					"reset", GPIOD_ASIS);
	if (IS_ERR_OR_NULL(panel->info.reset_gpio))
		DRM_WARN("can't get panel reset gpio: %ld\n",
				 PTR_ERR(panel->info.reset_gpio));

	return 0;
}

static int of_parse_reset_seq(struct device_node *np,
				struct panel_info *info)
{
	struct property *prop;
	int bytes, rc;
	u32 *p;

	prop = of_find_property(np, "sprd,reset-on-sequence", &bytes);
	if (!prop) {
		DRM_ERROR("sprd,reset-on-sequence property not found\n");
		return -EINVAL;
	}

	p = kzalloc(bytes, GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	rc = of_property_read_u32_array(np, "sprd,reset-on-sequence",
					p, bytes / 4);
	if (rc) {
		DRM_ERROR("parse sprd,reset-on-sequence failed\n");
		kfree(p);
		return rc;
	}

	info->rst_on_seq.items = bytes / 8;
	info->rst_on_seq.timing = (struct gpio_timing *)p;

	prop = of_find_property(np, "sprd,reset-off-sequence", &bytes);
	if (!prop) {
		DRM_ERROR("sprd,reset-off-sequence property not found\n");
		return -EINVAL;
	}

	p = kzalloc(bytes, GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	rc = of_property_read_u32_array(np, "sprd,reset-off-sequence",
					p, bytes / 4);
	if (rc) {
		DRM_ERROR("parse sprd,reset-off-sequence failed\n");
		kfree(p);
		return rc;
	}

	info->rst_off_seq.items = bytes / 8;
	info->rst_off_seq.timing = (struct gpio_timing *)p;

	return 0;
}

static int sprd_panel_parse_dt(struct device_node *np, struct sprd_panel *panel)
{
	u32 val;
	struct device_node *lcd_node;
	struct panel_info *info = &panel->info;
	int bytes, rc;
	const void *p;
	const char *str;
	char lcd_path[60];

	rc = of_property_read_string(np, "sprd,force-attached", &str);
	if (!rc)
		lcd_name = str;

	sprintf(lcd_path, "/lcds/%s", lcd_name);
	lcd_node = of_find_node_by_path(lcd_path);
	if (!lcd_node) {
		DRM_ERROR("%pOF: could not find %s node\n", np, lcd_name);
		return -ENODEV;
	}
	info->of_node = lcd_node;

	rc = of_property_read_u32(lcd_node, "sprd,dsi-work-mode", &val);
	if (!rc) {
		if (val == SPRD_DSI_MODE_CMD)
			info->mode_flags = 0;
		else if (val == SPRD_DSI_MODE_VIDEO_BURST)
			info->mode_flags = MIPI_DSI_MODE_VIDEO |
					   MIPI_DSI_MODE_VIDEO_BURST;
		else if (val == SPRD_DSI_MODE_VIDEO_SYNC_PULSE)
			info->mode_flags = MIPI_DSI_MODE_VIDEO |
					   MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
		else if (val == SPRD_DSI_MODE_VIDEO_SYNC_EVENT)
			info->mode_flags = MIPI_DSI_MODE_VIDEO;
	} else {
		DRM_ERROR("dsi work mode is not found! use video mode\n");
		info->mode_flags = MIPI_DSI_MODE_VIDEO |
				   MIPI_DSI_MODE_VIDEO_BURST;
	}

	if (of_property_read_bool(lcd_node, "sprd,dsi-non-continuous-clock"))
		info->mode_flags |= MIPI_DSI_CLOCK_NON_CONTINUOUS;

	rc = of_property_read_u32(lcd_node, "sprd,dsi-lane-number", &val);
	if (!rc)
		info->lanes = val;
	else
		info->lanes = 4;

	rc = of_property_read_string(lcd_node, "sprd,dsi-color-format", &str);
	if (rc)
		info->format = MIPI_DSI_FMT_RGB888;
	else if (!strcmp(str, "rgb888"))
		info->format = MIPI_DSI_FMT_RGB888;
	else if (!strcmp(str, "rgb666"))
		info->format = MIPI_DSI_FMT_RGB666;
	else if (!strcmp(str, "rgb666_packed"))
		info->format = MIPI_DSI_FMT_RGB666_PACKED;
	else if (!strcmp(str, "rgb565"))
		info->format = MIPI_DSI_FMT_RGB565;
	else if (!strcmp(str, "dsc"))
		info->format = SPRD_MIPI_DSI_FMT_DSC;
	else
		DRM_ERROR("dsi-color-format (%s) is not supported\n", str);

	rc = of_property_read_u32(lcd_node, "sprd,width-mm", &val);
	if (!rc)
		info->mode.width_mm = val;
	else
		info->mode.width_mm = 68;

	rc = of_property_read_u32(lcd_node, "sprd,height-mm", &val);
	if (!rc)
		info->mode.height_mm = val;
	else
		info->mode.height_mm = 121;

	rc = of_property_read_u32(lcd_node, "sprd,esd-check-enable", &val);
	if (!rc)
		info->esd_check_en = val;

	rc = of_property_read_u32(lcd_node, "sprd,esd-check-mode", &val);
	if (!rc)
		info->esd_check_mode = val;
	else
		info->esd_check_mode = 1;

	rc = of_property_read_u32(lcd_node, "sprd,esd-check-period", &val);
	if (!rc)
		info->esd_check_period = val;
	else
		info->esd_check_period = 1000;

	rc = of_property_read_u32(lcd_node, "sprd,esd-check-register", &val);
	if (!rc)
		info->esd_check_reg = val;
	else
		info->esd_check_reg = 0x0A;

	rc = of_property_read_u32(lcd_node, "sprd,esd-check-value", &val);
	if (!rc)
		info->esd_check_val = val;
	else
		info->esd_check_val = 0x9C;

	if (of_property_read_bool(lcd_node, "sprd,use-dcs-write"))
		info->use_dcs = true;
	else
		info->use_dcs = false;

	rc = of_parse_reset_seq(lcd_node, info);
	if (rc)
		DRM_ERROR("parse lcd reset sequence failed\n");

	p = of_get_property(lcd_node, "sprd,initial-command", &bytes);
	if (p) {
		info->cmds[CMD_CODE_INIT] = p;
		info->cmds_len[CMD_CODE_INIT] = bytes;
	} else
		DRM_ERROR("can't find sprd,initial-command property\n");

	p = of_get_property(lcd_node, "sprd,sleep-in-command", &bytes);
	if (p) {
		info->cmds[CMD_CODE_SLEEP_IN] = p;
		info->cmds_len[CMD_CODE_SLEEP_IN] = bytes;
	} else
		DRM_ERROR("can't find sprd,sleep-in-command property\n");

	p = of_get_property(lcd_node, "sprd,sleep-out-command", &bytes);
	if (p) {
		info->cmds[CMD_CODE_SLEEP_OUT] = p;
		info->cmds_len[CMD_CODE_SLEEP_OUT] = bytes;
	} else
		DRM_ERROR("can't find sprd,sleep-out-command property\n");

	rc = of_get_drm_display_mode(lcd_node, &info->mode, 0,
				     OF_USE_NATIVE_MODE);
	if (rc) {
		DRM_ERROR("get display timing failed\n");
		return rc;
	}

	info->mode.vrefresh = drm_mode_vrefresh(&info->mode);

	return 0;
}

static int sprd_panel_device_create(struct device *parent,
				    struct sprd_panel *panel)
{
	panel->dev.class = display_class;
	panel->dev.parent = parent;
	panel->dev.of_node = panel->info.of_node;
	dev_set_name(&panel->dev, "panel0");
	dev_set_drvdata(&panel->dev, panel);

	return device_register(&panel->dev);
}

static int sprd_panel_probe(struct mipi_dsi_device *slave)
{
	int ret;
	struct sprd_panel *panel;
	struct device_node *bl_node;

	panel = devm_kzalloc(&slave->dev, sizeof(*panel), GFP_KERNEL);
	if (!panel)
		return -ENOMEM;

	bl_node = of_parse_phandle(slave->dev.of_node,
					"sprd,backlight", 0);
	if (bl_node) {
		panel->backlight = of_find_backlight_by_node(bl_node);
		of_node_put(bl_node);

		if (panel->backlight) {
			panel->backlight->props.state &= ~BL_CORE_FBBLANK;
			panel->backlight->props.power = FB_BLANK_UNBLANK;
			backlight_update_status(panel->backlight);
		} else {
			DRM_WARN("backlight is not ready, panel probe deferred\n");
			return -EPROBE_DEFER;
		}
	} else
		DRM_WARN("backlight node not found\n");

	panel->supply = devm_regulator_get(&slave->dev, "power");
	if (IS_ERR(panel->supply))
		DRM_ERROR("get lcd regulator failed\n");

	INIT_DELAYED_WORK(&panel->esd_work, sprd_panel_esd_work_func);

	ret = sprd_panel_parse_dt(slave->dev.of_node, panel);
	if (ret) {
		DRM_ERROR("parse panel info failed\n");
		return ret;
	}

	ret = sprd_panel_gpio_request(&slave->dev, panel);
	if (ret) {
		DRM_WARN("gpio is not ready, panel probe deferred\n");
		return -EPROBE_DEFER;
	}

	ret = sprd_panel_device_create(&slave->dev, panel);
	if (ret) {
		DRM_ERROR("panel device create failed\n");
		return ret;
	}

	panel->base.dev = &panel->dev;
	panel->base.funcs = &sprd_panel_funcs;
	drm_panel_init(&panel->base);

	ret = drm_panel_add(&panel->base);
	if (ret) {
		DRM_ERROR("drm_panel_add() failed\n");
		return ret;
	}

	slave->lanes = panel->info.lanes;
	slave->format = panel->info.format;
	slave->mode_flags = panel->info.mode_flags;

	ret = mipi_dsi_attach(slave);
	if (ret) {
		DRM_ERROR("failed to attach dsi panel to host\n");
		drm_panel_remove(&panel->base);
		return ret;
	}
	panel->slave = slave;

	sprd_panel_sysfs_init(&panel->dev);
	mipi_dsi_set_drvdata(slave, panel);

	/*
	 * FIXME:
	 * The esd check work should not be scheduled in probe
	 * function. It should be scheduled in the enable()
	 * callback function. But the dsi encoder will not call
	 * drm_panel_enable() the first time in encoder_enable().
	 */
	if (panel->info.esd_check_en) {
		schedule_delayed_work(&panel->esd_work,
				      msecs_to_jiffies(2000));
		panel->esd_work_pending = true;
	}

	DRM_INFO("panel driver probe success\n");

	return 0;
}

static int sprd_panel_remove(struct mipi_dsi_device *slave)
{
	struct sprd_panel *panel = mipi_dsi_get_drvdata(slave);
	int ret;

	DRM_INFO("%s()\n", __func__);

	sprd_panel_disable(&panel->base);
	sprd_panel_unprepare(&panel->base);

	ret = mipi_dsi_detach(slave);
	if (ret < 0)
		DRM_ERROR("failed to detach from DSI host: %d\n", ret);

	drm_panel_detach(&panel->base);
	drm_panel_remove(&panel->base);

	return 0;
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
};
module_mipi_dsi_driver(sprd_panel_driver);

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("SPRD MIPI DSI Panel Driver");
MODULE_LICENSE("GPL v2");
