/*
 *Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 *This software is licensed under the terms of the GNU General Public
 *License version 2, as published by the Free Software Foundation, and
 *may be copied, distributed, and modified under those terms.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/gpio/consumer.h>
#include <linux/iopoll.h>
#include <video/mipi_display.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_graph.h>

#include "sprd_dsi.h"
#include "dsi/sprd_dsi_api.h"

#define encoder_to_dsi(encoder) \
	container_of(encoder, struct sprd_dsi, encoder)
#define host_to_dsi(host) \
	container_of(host, struct sprd_dsi, host)
#define connector_to_dsi(connector) \
	container_of(connector, struct sprd_dsi, connector)

LIST_HEAD(dsi_core_head);
LIST_HEAD(dsi_glb_head);

static void sprd_dsi_encoder_disable(struct drm_encoder *encoder)
{
	struct sprd_dsi *dsi = encoder_to_dsi(encoder);
//	struct sprd_dsi_hw_ctx *ctx = dsi->ctx;
//	void __iomem *base = ctx->base;

	DRM_INFO("drm_encoder_helper_funcs->disable()\n");

	/* turn off panel's backlight */
	if (dsi->panel && drm_panel_disable(dsi->panel))
		DRM_ERROR("failed to disable panel\n");

	/* turn off panel */
	if (dsi->panel && drm_panel_unprepare(dsi->panel))
		DRM_ERROR("failed to unprepare panel\n");
}


static void sprd_dsi_encoder_enable(struct drm_encoder *encoder)
{
	struct sprd_dsi *dsi = encoder_to_dsi(encoder);
//	struct sprd_dsi_hw_ctx *ctx = dsi->ctx;
//	int ret;

	DRM_INFO("drm_encoder_helper_funcs->enable()\n");

	/* turn on panel */
	if (dsi->panel && drm_panel_prepare(dsi->panel))
		DRM_ERROR("failed to prepare panel\n");

	/*sprd_dsi_set_mode(dsi, DSI_VIDEO_MODE);*/

	/* turn on panel's back light */
	if (dsi->panel && drm_panel_enable(dsi->panel))
		DRM_ERROR("failed to enable panel\n");
}

static void sprd_dsi_encoder_mode_set(struct drm_encoder *encoder,
				 struct drm_display_mode *mode,
				 struct drm_display_mode *adj_mode)
{
	struct sprd_dsi *dsi = encoder_to_dsi(encoder);

	DRM_INFO("drm_encoder_helper_funcs->mode_set()\n");
	drm_mode_copy(&dsi->cur_mode, adj_mode);
}

static int sprd_dsi_encoder_atomic_check(struct drm_encoder *encoder,
				    struct drm_crtc_state *crtc_state,
				    struct drm_connector_state *conn_state)
{
	DRM_INFO("drm_encoder_helper_funcs->atomic_check()\n");
	/* do nothing */
	return 0;
}

static const struct drm_encoder_helper_funcs sprd_encoder_helper_funcs = {
	.atomic_check	= sprd_dsi_encoder_atomic_check,
	.mode_set	= sprd_dsi_encoder_mode_set,
	.enable		= sprd_dsi_encoder_enable,
	.disable	= sprd_dsi_encoder_disable
};

static const struct drm_encoder_funcs sprd_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static int sprd_drm_encoder_init(struct device *dev,
			       struct drm_device *drm,
			       struct drm_encoder *encoder)
{
	int ret;
	u32 crtc_mask;

	crtc_mask = drm_of_find_possible_crtcs(drm, dev->of_node);
	if (!crtc_mask) {
		DRM_ERROR("failed to find crtc mask\n");
		return -EINVAL;
	}
	DRM_INFO("find possible crtcs success: 0x%08x\n", crtc_mask);

	encoder->possible_crtcs = crtc_mask;
	ret = drm_encoder_init(drm, encoder, &sprd_encoder_funcs,
			       DRM_MODE_ENCODER_DSI, NULL);
	if (ret) {
		DRM_ERROR("failed to init dsi encoder\n");
		return ret;
	}

	drm_encoder_helper_add(encoder, &sprd_encoder_helper_funcs);

	DRM_INFO("encoder init ok\n");
	return 0;
}

static int sprd_dsi_host_attach(struct mipi_dsi_host *host,
			   struct mipi_dsi_device *slave)
{
	struct sprd_dsi *dsi = host_to_dsi(host);
	struct drm_connector *connector = &dsi->connector;
	struct device_node *panel_node;
	struct drm_panel *panel;
	int ret;

	DRM_INFO("mipi_dsi_host_ops->attach()\n");

	dsi->ctx.lanes = slave->lanes;
//	dsi->ctx.format = slave->format;
//	dsi->ctx.mode_flags = slave->mode_flags;
//	dsi->ctx.phy_clock = slave->phy_clock;

	/* parse panel endpoint */
	panel_node = of_get_child_by_name(host->dev->of_node, "panel");
	panel = of_drm_find_panel(panel_node);
	if (!panel) {
		DRM_ERROR("of_drm_find_panel() failed\n");
		return -ENODEV;
	}
	dsi->panel = panel;

	ret = drm_panel_attach(dsi->panel, connector);
	if (ret) {
		DRM_INFO("drm_panel_attach() failed\n");
		return ret;
	}

	if (dsi->connector.dev)
		drm_helper_hpd_irq_event(dsi->connector.dev);

	return 0;
}

static int sprd_dsi_host_detach(struct mipi_dsi_host *host,
			   struct mipi_dsi_device *slave)
{
	DRM_INFO("mipi_dsi_host_ops->detach()\n");
	/* do nothing */
	return 0;
}

static ssize_t sprd_dsi_host_transfer(struct mipi_dsi_host *host,
				const struct mipi_dsi_msg *msg)
{
	struct sprd_dsi *dsi = host_to_dsi(host);
	const u8 *tx_buf = msg->tx_buf;

	if (msg->rx_buf && msg->rx_len) {
		u8 lsb = (msg->tx_len > 0) ? tx_buf[0] : 0;
		u8 msb = (msg->tx_len > 1) ? tx_buf[1] : 0;

		return sprd_dsi_rd_pkt(dsi, msg->channel, msg->type,
				msb, lsb, msg->rx_buf, msg->rx_len);
	}

	if (msg->tx_buf && msg->tx_len)
		return sprd_dsi_wr_pkt(dsi, msg->channel, msg->type,
					tx_buf, msg->tx_len);

	return 0;
}

static const struct mipi_dsi_host_ops sprd_dsi_host_ops = {
	.attach = sprd_dsi_host_attach,
	.detach = sprd_dsi_host_detach,
	.transfer = sprd_dsi_host_transfer,
};

static int sprd_dsi_bridge_init(struct drm_device *drm, struct sprd_dsi *dsi)
{
	struct drm_encoder *encoder = &dsi->encoder;
	struct drm_bridge *bridge = dsi->bridge;
	int ret;

	ret = drm_bridge_attach(encoder, bridge, NULL);
	if (ret) {
		DRM_ERROR("failed to attach external bridge\n");
		return ret;
	}

	DRM_INFO("call drm_bridge_attach() ok\n");

	return 0;
}

static int sprd_dsi_connector_get_modes(struct drm_connector *connector)
{
	struct sprd_dsi *dsi = connector_to_dsi(connector);

	DRM_INFO("drm_connector_helper_funcs->get_modes()\n");

	return drm_panel_get_modes(dsi->panel);
}

static enum drm_mode_status
sprd_dsi_connector_mode_valid(struct drm_connector *connector,
			 struct drm_display_mode *mode)
{
	enum drm_mode_status mode_status = MODE_OK;

	DRM_INFO("drm_connector_helper_funcs->mode_valid()\n");

	return mode_status;
}

static struct drm_encoder *
sprd_dsi_connector_best_encoder(struct drm_connector *connector)
{
	struct sprd_dsi *dsi = connector_to_dsi(connector);

	DRM_INFO("drm_connector_helper_funcs->best_encoder()\n");
	return &dsi->encoder;
}

static struct drm_connector_helper_funcs sprd_dsi_connector_helper_funcs = {
	.get_modes = sprd_dsi_connector_get_modes,
	.mode_valid = sprd_dsi_connector_mode_valid,
	.best_encoder = sprd_dsi_connector_best_encoder,
};

static enum drm_connector_status
sprd_dsi_connector_detect(struct drm_connector *connector, bool force)
{
	DRM_INFO("drm_connector_funcs->detect()\n");

	return connector_status_connected;
}

static void sprd_dsi_connector_destroy(struct drm_connector *connector)
{
	DRM_INFO("drm_connector_funcs->distory()\n");

	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const struct drm_connector_funcs sprd_dsi_atomic_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = sprd_dsi_connector_detect,
	.destroy = sprd_dsi_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int sprd_dsi_connector_init(struct drm_device *drm, struct sprd_dsi *dsi)
{
	struct drm_encoder *encoder = &dsi->encoder;
	struct drm_connector *connector = &dsi->connector;
	int ret;

	connector->polled = DRM_CONNECTOR_POLL_HPD;

	ret = drm_connector_init(drm, connector,
				 &sprd_dsi_atomic_connector_funcs,
				 DRM_MODE_CONNECTOR_DSI);
	if (ret) {
		DRM_INFO("drm_connector_init() failed\n");
		return ret;
	}

	drm_connector_helper_add(connector,
				 &sprd_dsi_connector_helper_funcs);

	drm_connector_register(connector);

	ret = drm_mode_connector_attach_encoder(connector, encoder);
	if (ret) {
		DRM_INFO("drm_mode_connector_attach_encoder() failed\n");
		return ret;
	}

	DRM_INFO("connector init ok\n");

	return 0;
}

static int sprd_dsi_glb_init(struct sprd_dsi *dsi)
{
	if (dsi->glb && dsi->glb->power)
		dsi->glb->power(&dsi->ctx, true);
	if (dsi->glb && dsi->glb->enable)
		dsi->glb->enable(&dsi->ctx);

	return 0;
}

static irqreturn_t sprd_dsi_isr(int irq, void *data)
{
	u32 status = 0;
	struct sprd_dsi *dsi = data;

	if (!dsi) {
		DRM_ERROR("dsi pointer is NULL\n");
		return IRQ_HANDLED;
	}

	if (dsi->ctx.irq0 == irq)
		status = sprd_dsi_int_status(dsi, 0);
	else if (dsi->ctx.irq1 == irq)
		status = sprd_dsi_int_status(dsi, 1);

	if (status & DSI_INT_STS_NEED_SOFT_RESET)
		sprd_dsi_state_reset(dsi);

	return IRQ_HANDLED;
}

static int sprd_dsi_irq_request(struct sprd_dsi *dsi)
{
	int ret;
	int irq0, irq1;
	struct dsi_context *ctx = &dsi->ctx;

	irq0 = irq_of_parse_and_map(dsi->host.dev->of_node, 0);
	if (irq0) {
		DRM_INFO("dsi irq0 num = %d\n", irq0);
		ret = request_irq(irq0, sprd_dsi_isr, 0, "DSI_INT0", dsi);
		if (ret) {
			DRM_ERROR("dsi failed to request irq int0!\n");
			return -EINVAL;
		}
	}
	ctx->irq0 = irq0;

	irq1 = irq_of_parse_and_map(dsi->host.dev->of_node, 1);
	if (irq1) {
		DRM_INFO("dsi irq1 num = %d\n", irq1);
		ret = request_irq(irq1, sprd_dsi_isr, 0, "DSI_INT1", dsi);
		if (ret) {
			DRM_ERROR("dsi failed to request irq int1!\n");
			return -EINVAL;
		}
	}
	ctx->irq1 = irq1;

	return 0;
}

static int sprd_dsi_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct sprd_dsi *dsi = dev_get_drvdata(dev);
	int ret;

	DRM_INFO("component_ops->bind()\n");

	if (IS_ERR_OR_NULL(data))
		DRM_ERROR("sprd dsi bind null drm_device\n");

	ret = sprd_drm_encoder_init(dev, drm, &dsi->encoder);
	if (ret)
		return ret;

	ret = sprd_dsi_connector_init(drm, dsi);
	if (ret)
		return ret;

	if (dsi->bridge) {
		DRM_INFO("start sprd dsi bridge init\n");
		ret = sprd_dsi_bridge_init(drm, dsi);
		if (ret)
			return ret;
	}

	ret = sprd_dsi_glb_init(dsi);
	if (ret)
		return ret;

	ret = sprd_dsi_irq_request(dsi);
	if (ret)
		return ret;

	DRM_INFO("sprd dsi bind ok\n");
	return 0;
}

static void sprd_dsi_unbind(struct device *dev,
			struct device *master, void *data)
{
	/* do nothing */
	DRM_INFO("component_ops->unbind()\n");
}

static const struct component_ops dsi_component_ops = {
	.bind	= sprd_dsi_bind,
	.unbind	= sprd_dsi_unbind,
};

static int dsi_device_register(struct sprd_dsi *dsi,
				struct device *parent)
{
	int ret;

//	dsi->dev.class = display_class;
	dsi->dev.parent = parent;
	dsi->dev.of_node = parent->of_node;
	dev_set_name(&dsi->dev, "dsi");
	dev_set_drvdata(&dsi->dev, dsi);

	ret = device_register(&dsi->dev);
	if (ret)
		DRM_ERROR("dsi device register failed\n");

	return ret;
}

static int dsi_context_init(struct sprd_dsi *dsi, struct device_node *np)
{
//	struct panel_info *panel = dsi->panel;
	struct dsi_context *ctx = &dsi->ctx;
	struct resource r;
	u32 tmp;

	if (dsi->glb && dsi->glb->parse_dt)
		dsi->glb->parse_dt(&dsi->ctx, np);

	if (of_address_to_resource(np, 0, &r)) {
		DRM_ERROR("parse dsi ctrl reg base failed\n");
		return -ENODEV;
	}
	ctx->base = (unsigned long)
	    ioremap_nocache(r.start, resource_size(&r));
	if (ctx->base == 0) {
		DRM_ERROR("dsi ctrl reg base ioremap failed\n");
		return -ENODEV;
	}

	if (!of_property_read_u32(np, "dev-id", &tmp))
		ctx->id = tmp;

	if (!of_property_read_u32(np, "sprd,max-read-time", &tmp))
		ctx->max_rd_time = tmp;
	else
		ctx->max_rd_time = 0x6000;

	if (!of_property_read_u32(np, "sprd,int0_mask", &tmp))
		ctx->int0_mask = tmp;
	else
		ctx->int0_mask = 0xffffffff;

	if (!of_property_read_u32(np, "sprd,int1_mask", &tmp))
		ctx->int1_mask = tmp;
	else
		ctx->int1_mask = 0xffffffff;

//	ctx->freq = panel->phy_freq;
//	ctx->lanes = panel->lane_num;
//	ctx->nc_clk_en = panel->nc_clk_en;

	return 0;
}

static int sprd_dsi_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct sprd_dsi *dsi;
	const char *str;
	int ret;

	dsi = devm_kzalloc(&pdev->dev, sizeof(*dsi), GFP_KERNEL);
	if (!dsi) {
		DRM_ERROR("failed to allocate dsi data.\n");
		return -ENOMEM;
	}

	dsi->host.dev = &pdev->dev;
	dsi->host.ops = &sprd_dsi_host_ops;
	ret = mipi_dsi_host_register(&dsi->host);
	if (ret) {
		DRM_ERROR("failed to register dsi host\n");
		return ret;
	}

	if (!of_property_read_string(np, "sprd,ip", &str))
		dsi->core = dsi_core_ops_attach(str);
	else
		DRM_ERROR("error: 'sprd,ip' was not found\n");

	if (!of_property_read_string(np, "sprd,soc", &str))
		dsi->glb = dsi_glb_ops_attach(str);
	else
		DRM_ERROR("error: 'sprd,soc' was not found\n");

	if (dsi_context_init(dsi, np))
		goto err;

	dsi_device_register(dsi, &pdev->dev);
	platform_set_drvdata(pdev, dsi);

	ret = component_add(&pdev->dev, &dsi_component_ops);
	if (ret) {
		DRM_INFO("component_add() failed");
		goto err;
	}

	DRM_INFO("sprd dsi probe ok\n");
	return 0;

err:
	mipi_dsi_host_unregister(&dsi->host);
	DRM_ERROR("dsi probe failed\n");
	return ret;
}

static int sprd_dsi_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &dsi_component_ops);

	return 0;
}

static const struct of_device_id sprd_dsi_of_match[] = {
	{.compatible = "sprd,dsi-host"},
	{ }
};
MODULE_DEVICE_TABLE(of, sprd_dsi_of_match);

static struct platform_driver sprd_dsi_driver = {
	.probe = sprd_dsi_probe,
	.remove = sprd_dsi_remove,
	.driver = {
		.name = "sprd-dsi-drv",
		.of_match_table = sprd_dsi_of_match,
	},
};

module_platform_driver(sprd_dsi_driver);

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("SPRD MIPI DSI HOST Controller Driver");
MODULE_LICENSE("GPL v2");
