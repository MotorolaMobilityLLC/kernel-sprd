// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/component.h>
#include <linux/extcon.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_graph.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_connector.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_edid.h>
#include <linux/pm_runtime.h>
#include <linux/usb/typec_dp.h>

#include "disp_lib.h"
#include "sprd_drm.h"
#include "sprd_dpu1.h"
#include "sprd_dp.h"
#include "sysfs/sysfs_display.h"
#include "dp/dw_dptx.h"

#define encoder_to_dp(encoder) \
	container_of(encoder, struct sprd_dp, encoder)
#define connector_to_dp(connector) \
	container_of(connector, struct sprd_dp, connector)

/*
#define drm_connector_update_edid_property(connector, edid) \
	drm_mode_connector_update_edid_property(connector, edid)
*/

static int sprd_dp_resume(struct sprd_dp *dp)
{
	if (dp->glb && dp->glb->enable)
		dp->glb->enable(&dp->ctx);

	DRM_INFO("dp resume OK\n");
	return 0;
}

static int sprd_dp_suspend(struct sprd_dp *dp)
{
	if (dp->glb && dp->glb->disable)
		dp->glb->disable(&dp->ctx);

	DRM_INFO("dp suspend OK\n");
	return 0;
}

static int sprd_dp_detect(struct sprd_dp *dp, int hpd_status)
{
	if (dp->glb && dp->glb->detect)
		dp->glb->detect(&dp->ctx, hpd_status);

	DRM_INFO("dp detect\n");
	return 0;
}

static int sprd_dp_pd_event(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct sprd_dp *dp = container_of(nb, struct sprd_dp, dp_nb);
	struct extcon_dev *edev = dp->snps_dptx->edev;
	union extcon_property_value property;

	extcon_get_property(edev, EXTCON_DISP_DP,
				EXTCON_PROP_DISP_HPD, &property);

	if (dp->hpd_status == false &&
		property.intval ==  DP_HOT_PLUG) {
		pm_runtime_get_sync(dp->dev.parent);
		sprd_dp_resume(dp);
		sprd_dp_detect(dp, DP_HOT_PLUG);
		dp->hpd_status = true;
	} else if (dp->hpd_status == true &&
		(property.intval ==  DP_HOT_UNPLUG ||
		property.intval == DP_TYPE_DISCONNECT)) {
		sprd_dp_detect(dp, DP_HOT_UNPLUG);
		sprd_dp_suspend(dp);
		pm_runtime_put(dp->dev.parent);
		dp->hpd_status = false;
	} else if (dp->hpd_status == true &&
		property.intval ==  DP_HPD_IRQ) {
		sprd_dp_detect(dp, DP_HPD_IRQ);
	}

	DRM_INFO("%s() hpd_status:%d\n", __func__, property.intval);
	return NOTIFY_OK;
}

static void sprd_dp_timing_set(struct sprd_dp *dp)
{
	dptx_video_ts_calculate(dp->snps_dptx,
				dp->snps_dptx->link.lanes,
				dp->snps_dptx->link.rate,
				dp->snps_dptx->vparams.bpc,
				dp->snps_dptx->vparams.pix_enc,
				dp->snps_dptx->vparams.mdtd.pixel_clock);

	dptx_video_reset(dp->snps_dptx, 1, 0);
	dptx_video_reset(dp->snps_dptx, 0, 0);
	dptx_video_timing_change(dp->snps_dptx, 0);

	/* enable VSC if YCBCR420 is enabled */
	if (dp->snps_dptx->vparams.pix_enc == YCBCR420)
		dptx_vsc_ycbcr420_send(dp->snps_dptx, 1);

	DRM_INFO("%s() set mode %dx%d\n", __func__,
		dp->ctx.vm.hactive, dp->ctx.vm.vactive);
}

static void sprd_dp_encoder_enable(struct drm_encoder *encoder)
{

	struct sprd_dp *dp = encoder_to_dp(encoder);
	struct sprd_crtc *crtc = to_sprd_crtc(encoder->crtc);

	DRM_INFO("%s()\n", __func__);

	if (dp->ctx.enabled) {
		DRM_WARN("dp has already been enabled\n");
		return;
	}

	pm_runtime_get_sync(dp->dev.parent);

	sprd_dp_resume(dp);

	sprd_dp_timing_set(dp);

	sprd_dpu1_run(crtc->priv);

	dp->ctx.enabled = true;
}

static void sprd_dp_encoder_disable(struct drm_encoder *encoder)
{
	struct sprd_dp *dp = encoder_to_dp(encoder);
	struct sprd_crtc *crtc = to_sprd_crtc(encoder->crtc);

	DRM_INFO("%s()\n", __func__);

	if (!dp->ctx.enabled) {
		DRM_WARN("dp has already been disabled\n");
		return;
	}

	sprd_dpu1_stop(crtc->priv);

	dptx_disable_default_video_stream(dp->snps_dptx, 0);

	sprd_dp_suspend(dp);

	pm_runtime_put(dp->dev.parent);

	dp->ctx.enabled = false;
}

static void sprd_dp_encoder_mode_set(struct drm_encoder *encoder,
				 struct drm_display_mode *mode,
				 struct drm_display_mode *adj_mode)
{
	struct sprd_dp *dp = encoder_to_dp(encoder);
	struct sprd_crtc *crtc = to_sprd_crtc(encoder->crtc);
	struct sprd_dpu *dpu = crtc->priv;
	struct drm_display_info *info = &dp->connector.display_info;
	struct video_params *vparams = &dp->snps_dptx->vparams;

	dptx_timing_cfg(dp->snps_dptx, mode, info);

	if (dpu->ctx.bypass_mode) {
		switch (crtc->base.primary->state->fb->format->format) {
		case DRM_FORMAT_NV12:
			vparams->bpc = COLOR_DEPTH_8;
			vparams->pix_enc = YCBCR420;
			break;
		case DRM_FORMAT_P010:
			vparams->bpc = COLOR_DEPTH_10;
			vparams->pix_enc = YCBCR420;
			break;
		default:
			vparams->bpc = COLOR_DEPTH_8;
			vparams->pix_enc = RGB;
			break;
		}
	} else {
		/* TODO not support cts test */
		vparams->bpc = COLOR_DEPTH_8;
		vparams->pix_enc = RGB;
	}

	drm_display_mode_to_videomode(mode, &dp->ctx.vm);

	DRM_INFO("%s() set mode: %s\n", __func__, mode->name);
}

static int sprd_dp_encoder_atomic_check(struct drm_encoder *encoder,
					struct drm_crtc_state *crtc_state,
					struct drm_connector_state *conn_state)
{
	DRM_INFO("%s()\n", __func__);

	return 0;
}

static const struct drm_encoder_helper_funcs sprd_encoder_helper_funcs = {
	.atomic_check = sprd_dp_encoder_atomic_check,
	.mode_set = sprd_dp_encoder_mode_set,
	.enable = sprd_dp_encoder_enable,
	.disable = sprd_dp_encoder_disable,
};

static const struct drm_encoder_funcs sprd_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static int sprd_dp_encoder_init(struct drm_device *drm,
			       struct sprd_dp *dp)
{
	struct drm_encoder *encoder = &dp->encoder;
	int ret;

	ret = drm_encoder_init(drm, encoder, &sprd_encoder_funcs,
			       DRM_MODE_ENCODER_TMDS, NULL);

	if (ret) {
		DRM_ERROR("failed to init dp encoder\n");
		return ret;
	}

	ret = sprd_drm_set_possible_crtcs(encoder, SPRD_DISPLAY_TYPE_DP);
	if (ret) {
		DRM_ERROR("failed to find possible crtc\n");
		return ret;
	}

	drm_encoder_helper_add(encoder, &sprd_encoder_helper_funcs);

	return 0;
}

static int sprd_dp_connector_get_modes(struct drm_connector *connector)
{
	struct sprd_dp *dp = connector_to_dp(connector);
	struct edid *edid;
	int num_modes = 0;

	DRM_INFO("%s()\n", __func__);

	edid = drm_get_edid(connector, &dp->snps_dptx->aux_dev.ddc);
	if (edid) {
		dp->sink_has_audio = drm_detect_monitor_audio(edid);
		drm_connector_update_edid_property(&dp->connector, edid);
		num_modes += drm_add_edid_modes(&dp->connector, edid);
		//drm_edid_to_eld(connector, edid);
		kfree(edid);
	} else
		DRM_ERROR("%s() no edid\n", __func__);

	return num_modes;
}

static enum drm_mode_status
sprd_dp_connector_mode_valid(struct drm_connector *connector,
			 struct drm_display_mode *mode)
{
	struct sprd_dp *dp = connector_to_dp(connector);
	struct drm_display_info *info = &dp->connector.display_info;
	int vic;

	vic = drm_match_cea_mode(mode);

	if (mode->type & DRM_MODE_TYPE_PREFERRED)
		mode->type &= ~DRM_MODE_TYPE_PREFERRED;

	/* 1920x1080@60Hz is used by default */
	if (vic == 16 && mode->clock == 148500) {
		DRM_INFO("%s() mode: "DRM_MODE_FMT"\n", __func__,
			 DRM_MODE_ARG(mode));
		mode->type |= DRM_MODE_TYPE_PREFERRED;
	}

	/* 3840x2160@60Hz yuv420 bypass */
	if (vic == 97 && mode->clock == 594000 &&
		(info->color_formats & DRM_COLOR_FORMAT_YCRCB420)) {
		DRM_INFO("%s() mode: "DRM_MODE_FMT"\n", __func__,
			 DRM_MODE_ARG(mode));
		mode->type |= DRM_MODE_TYPE_USERDEF;
	}

	/* 640x480@60Hz is used for cts test */
	if (vic == 1 && mode->clock == 25175) {
		DRM_INFO("%s() mode: "DRM_MODE_FMT"\n", __func__,
			 DRM_MODE_ARG(mode));
		mode->type |= DRM_MODE_TYPE_USERDEF;
	}

	return MODE_OK;
}

static struct drm_encoder *
sprd_dp_connector_best_encoder(struct drm_connector *connector)
{
	struct sprd_dp *dp = connector_to_dp(connector);

	DRM_INFO("%s()\n", __func__);
	return &dp->encoder;
}

static int fill_hdr_info_packet(const struct drm_connector_state *state,
				void *out)
{
	struct hdmi_drm_infoframe frame;
	unsigned char buf[30]; /* 26 + 4 */
	ssize_t len;
	int ret;
	u8 *ptr = out;

	memset(out, 0, sizeof(*out));

	ret = drm_hdmi_infoframe_set_hdr_metadata(&frame, state);
	if (ret)
		return ret;

	len = hdmi_drm_infoframe_pack_only(&frame, buf, sizeof(buf));
	if (len < 0)
		return (int)len;

	/* Static metadata is a fixed 26 bytes + 4 byte header. */
	if (len != 30)
		return -EINVAL;

	switch (state->connector->connector_type) {
	case DRM_MODE_CONNECTOR_DisplayPort:
	case DRM_MODE_CONNECTOR_eDP:
		ptr[0] = 0x00; /* sdp id, zero */
		ptr[1] = 0x87; /* type */
		ptr[2] = 0x1D; /* payload len - 1 */
		ptr[3] = (0x13 << 2); /* sdp version */
		ptr[4] = 0x01; /* version */
		ptr[5] = 0x1A; /* length */
		break;
	default:
		return -EINVAL;
	}

	memcpy(&ptr[6], &buf[4], 26);

	return 0;
}

static bool
is_hdr_metadata_different(const struct drm_connector_state *old_state,
			  const struct drm_connector_state *new_state)
{
	struct drm_property_blob *old_blob = old_state->hdr_output_metadata;
	struct drm_property_blob *new_blob = new_state->hdr_output_metadata;

	if (old_blob != new_blob) {
		if (old_blob && new_blob &&
		    old_blob->length == new_blob->length)
			return memcmp(old_blob->data, new_blob->data,
				      old_blob->length);
		return true;
	}

	return false;
}

static int sprd_dp_connector_atomic_check(struct drm_connector *connector,
				 struct drm_atomic_state *state)
{
	struct drm_connector_state *new_con_state =
		drm_atomic_get_new_connector_state(state, connector);
	struct drm_connector_state *old_con_state =
		drm_atomic_get_old_connector_state(state, connector);
	struct sprd_dp *dp = connector_to_dp(connector);
	struct sprd_crtc *crtc = to_sprd_crtc(dp->encoder.crtc);
	struct sprd_dpu *dpu = crtc->priv;
	int ret;

	if (is_hdr_metadata_different(old_con_state, new_con_state)) {
		ret = fill_hdr_info_packet(new_con_state,
				dpu->ctx.hdr_static_metadata);
		if (ret)
			return ret;
		dpu->ctx.static_metadata_changed = true;
	}

	return 0;
}

static const struct drm_connector_helper_funcs sprd_dp_connector_helper_funcs = {
	.get_modes = sprd_dp_connector_get_modes,
	.mode_valid = sprd_dp_connector_mode_valid,
	.best_encoder = sprd_dp_connector_best_encoder,
	.atomic_check = sprd_dp_connector_atomic_check,
};

static enum drm_connector_status
sprd_dp_connector_detect(struct drm_connector *connector, bool force)
{
	struct sprd_dp *dp = connector_to_dp(connector);

	DRM_INFO("%s()\n", __func__);

	if (dp->snps_dptx->link.trained)
		return connector_status_connected;
	else
		return connector_status_disconnected;
}

static void sprd_dp_connector_destroy(struct drm_connector *connector)
{
	DRM_INFO("%s()\n", __func__);

	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const struct drm_connector_funcs sprd_dp_atomic_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = sprd_dp_connector_detect,
	.destroy = sprd_dp_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int sprd_dp_connector_init(struct drm_device *drm, struct sprd_dp *dp)
{
	struct drm_encoder *encoder = &dp->encoder;
	struct drm_connector *connector = &dp->connector;
	int ret;

	connector->polled = DRM_CONNECTOR_POLL_HPD;

	ret = drm_connector_init(drm, connector,
				 &sprd_dp_atomic_connector_funcs,
				 DRM_MODE_CONNECTOR_DisplayPort);
	if (ret) {
		DRM_ERROR("drm_connector_init() failed\n");
		return ret;
	}

	drm_object_attach_property(
			&connector->base,
			drm->mode_config.hdr_output_metadata_property, 0);

	drm_connector_helper_add(connector,
				 &sprd_dp_connector_helper_funcs);

	drm_connector_attach_encoder(connector, encoder);

	return 0;
}

static int sprd_dp_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct sprd_dp *dp = dev_get_drvdata(dev);
	struct video_params *vparams = NULL;
	struct device_node *np = dev->of_node;
	struct extcon_dev *extcon;
	int ret;

	ret = sprd_dp_encoder_init(drm, dp);
	if (ret)
		return ret;

	ret = sprd_dp_connector_init(drm, dp);
	if (ret)
		goto cleanup_encoder;

	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);
	sprd_dp_resume(dp);

	dp->snps_dptx = dptx_init(dev, drm);
	if (IS_ERR_OR_NULL(dp->snps_dptx)) {
		ret = PTR_ERR(dp->snps_dptx);
		goto cleanup_connector;
	}

	if (of_property_read_bool(np, "extcon")) {
		extcon = extcon_get_edev_by_phandle(dev, 0);
		if (IS_ERR_OR_NULL(extcon)) {
			DRM_ERROR("failed to find pd extcon device\n");
			return -EPROBE_DEFER;
		}
		dp->snps_dptx->edev = extcon;
	} else
		DRM_ERROR("failed to find extcon nodes");

	sprd_dp_suspend(dp);
	pm_runtime_put(dev);

	vparams = &dp->snps_dptx->vparams;

	dp->dp_nb.notifier_call = sprd_dp_pd_event;
	ret = devm_extcon_register_notifier(&dp->dev, dp->snps_dptx->edev,
						EXTCON_DISP_DP,
						&dp->dp_nb);
	if (ret)
		DRM_ERROR("register EXTCON_DISP_DP notifier err\n");

	return 0;

cleanup_connector:
	drm_connector_cleanup(&dp->connector);
cleanup_encoder:
	drm_encoder_cleanup(&dp->encoder);
	return ret;
}

static void sprd_dp_unbind(struct device *dev,
			struct device *master, void *data)
{
	struct sprd_dp *dp = dev_get_drvdata(dev);
	struct dptx *dptx = dp->snps_dptx;

	dptx_core_deinit(dptx);
}

static const struct component_ops dp_component_ops = {
	.bind = sprd_dp_bind,
	.unbind = sprd_dp_unbind,
};

static int sprd_dp_device_create(struct sprd_dp *dp,
				struct device *parent)
{
	int ret;

	dp->dev.class = display_class;
	dp->dev.parent = parent;
	dp->dev.of_node = parent->of_node;
	dev_set_name(&dp->dev, "dp");
	dev_set_drvdata(&dp->dev, dp);

	ret = device_register(&dp->dev);
	if (ret) {
		DRM_ERROR("dp device register failed\n");
		return ret;
	}

	return 0;
}

static int sprd_dp_context_init(struct sprd_dp *dp, struct device_node *np)
{
	struct dp_context *ctx = &dp->ctx;
	struct resource r;

	if (dp->glb && dp->glb->parse_dt)
		dp->glb->parse_dt(&dp->ctx, np);

	if (of_address_to_resource(np, 0, &r)) {
		DRM_ERROR("parse dp ctrl reg base failed\n");
		return -ENODEV;
	}
	ctx->base = ioremap_nocache(r.start, resource_size(&r));
	if (ctx->base == NULL) {
		DRM_ERROR("dp ctrl reg base ioremap failed\n");
		return -ENODEV;
	}

	return 0;
}

static const struct sprd_dp_ops qogirn6pro_dp = {
	.glb = &qogirn6pro_dp_glb_ops
};

static const struct of_device_id dp_match_table[] = {
	{.compatible = "sprd,dptx",
	 .data = &qogirn6pro_dp},
	{ }
};

static int sprd_dp_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const struct sprd_dp_ops *pdata;
	struct sprd_dp *dp;
	//const char *str;
	int ret;

	dp = devm_kzalloc(&pdev->dev, sizeof(*dp), GFP_KERNEL);
	if (!dp) {
		DRM_ERROR("failed to allocate dp data.\n");
		return -ENOMEM;
	}

	pdata = of_device_get_match_data(&pdev->dev);
	if (pdata) {
		dp->glb = pdata->glb;
	} else {
		DRM_ERROR("No matching driver data found\n");
		return -EINVAL;
	}

	ret = sprd_dp_context_init(dp, np);
	if (ret)
		return ret;

	sprd_dp_device_create(dp, &pdev->dev);
	platform_set_drvdata(pdev, dp);

	sprd_dp_audio_codec_init(&pdev->dev);

	return component_add(&pdev->dev, &dp_component_ops);
}

static int sprd_dp_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &dp_component_ops);

	return 0;
}

struct platform_driver sprd_dp_driver = {
	.probe = sprd_dp_probe,
	.remove = sprd_dp_remove,
	.driver = {
		   .name = "sprd-dp-drv",
		   .of_match_table = dp_match_table,
	},
};

MODULE_AUTHOR("Chen He <chen.he@unisoc.com>");
MODULE_DESCRIPTION("Unisoc DPTX Controller Driver");
MODULE_LICENSE("GPL v2");
