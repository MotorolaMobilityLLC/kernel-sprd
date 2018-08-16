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

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <video/display_timing.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <drm/drm_gem_framebuffer_helper.h>

#include "sprd_drm.h"
#include "sprd_dpu.h"
#include "sprd_gem.h"

struct sprd_plane {
	struct drm_plane plane;
	u32 index;
};

LIST_HEAD(dpu_core_head);
LIST_HEAD(dpu_clk_head);
LIST_HEAD(dpu_glb_head);

static inline struct sprd_plane *to_sprd_plane(struct drm_plane *plane)
{
	return container_of(plane, struct sprd_plane, plane);
}

static int dpu_plane_atomic_check(struct drm_plane *plane,
				  struct drm_plane_state *state)
{
	DRM_DEBUG("drm_plane_helper_funcs->atomic_check()\n");

	return 0;
}

static void dpu_plane_atomic_update(struct drm_plane *plane,
				    struct drm_plane_state *old_state)
{
	struct drm_plane_state *state = plane->state;
	struct drm_framebuffer *fb = plane->state->fb;
	struct drm_gem_object *obj;
	struct sprd_gem_obj *sprd_gem;
	struct sprd_plane *sp = to_sprd_plane(plane);
	struct sprd_dpu *dpu = crtc_to_dpu(plane->state->crtc);
	struct sprd_dpu_layer layer = {};
	int i;

	DRM_DEBUG("drm_plane_helper_funcs->atomic_update()\n");

	layer.index = sp->index;
	layer.src_x = state->src_x >> 16;
	layer.src_y = state->src_y >> 16;
	layer.src_w = state->src_w >> 16;
	layer.src_h = state->src_h >> 16;
	layer.dst_x = state->crtc_x;
	layer.dst_y = state->crtc_y;
	layer.dst_w = state->crtc_w;
	layer.dst_h = state->crtc_h;
	layer.rotation = state->rotation;

	layer.planes = fb->format->num_planes;
	layer.format = fb->format->format;
	layer.alpha = 0xff;

	for (i = 0; i < layer.planes; i++) {
		obj = drm_gem_fb_get_obj(fb, i);
		sprd_gem = to_sprd_gem_obj(obj);
		layer.addr[i] = sprd_gem->dma_addr + fb->offsets[i];
		layer.pitch[i] = fb->pitches[i];
	}

	if (dpu->core && dpu->core->layer)
		dpu->core->layer(&dpu->ctx, &layer);
}

static void dpu_plane_atomic_disable(struct drm_plane *plane,
				     struct drm_plane_state *old_state)
{
	struct sprd_plane *sp = to_sprd_plane(plane);
	struct sprd_dpu *dpu = crtc_to_dpu(old_state->crtc);

	DRM_DEBUG("drm_plane_helper_funcs->atomic_disable(), layer_id = %u\n",
		sp->index);

	if (dpu->core && dpu->core->clean)
		dpu->core->clean(&dpu->ctx, sp->index);
}

static const struct drm_plane_helper_funcs dpu_plane_helper_funcs = {
	.atomic_check = dpu_plane_atomic_check,
	.atomic_update = dpu_plane_atomic_update,
	.atomic_disable = dpu_plane_atomic_disable,
};

static const struct drm_plane_funcs dpu_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane	= drm_atomic_helper_disable_plane,
	.destroy = drm_plane_cleanup,
	.reset = drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};

static struct drm_plane *dpu_plane_init(struct drm_device *drm,
					struct sprd_dpu *dpu)
{
	struct drm_plane *primary = NULL;
	struct sprd_plane *sp = NULL;
	struct dpu_capability cap = {};
	int err;
	int i;

	if (dpu->core && dpu->core->capability)
		dpu->core->capability(&dpu->ctx, &cap);

	for (i = 0; i < cap.max_layers; i++) {

		sp = kzalloc(sizeof(*sp), GFP_KERNEL);
		if (!sp)
			return ERR_PTR(-ENOMEM);

		err = drm_universal_plane_init(drm, &sp->plane, 1,
					       &dpu_plane_funcs, cap.fmts_ptr,
					       cap.fmts_cnt, NULL,
					       DRM_PLANE_TYPE_PRIMARY, NULL);
		if (err) {
			kfree(sp);
			DRM_ERROR("fail to init primary plane\n");
			return ERR_PTR(err);
		}

		drm_plane_helper_add(&sp->plane, &dpu_plane_helper_funcs);

		sp->index = i;
		if (i == 0)
			primary = &sp->plane;
	}

	if (sp)
		DRM_INFO("dpu plane init ok\n");

	return primary;
}

static void dpu_crtc_atomic_enable(struct drm_crtc *crtc,
				   struct drm_crtc_state *old_state)
{
	DRM_DEBUG("drm_crtc_helper_funcs->atomic_enable()\n");
}

static void dpu_crtc_atomic_disable(struct drm_crtc *crtc,
				    struct drm_crtc_state *old_state)
{
	DRM_DEBUG("drm_crtc_helper_funcs->atomic_disable()\n");
}

static int dpu_crtc_atomic_check(struct drm_crtc *crtc,
				 struct drm_crtc_state *state)
{
	DRM_DEBUG("drm_crtc_helper_funcs->atomic_check()\n");

	/* do nothing */
	return 0;
}

static void dpu_crtc_atomic_begin(struct drm_crtc *crtc,
				  struct drm_crtc_state *old_state)
{
	struct sprd_dpu *dpu = crtc_to_dpu(crtc);

	DRM_DEBUG("drm_crtc_helper_funcs->atomic_begin()\n");

	if (crtc->state->event) {
		crtc->state->event->pipe = drm_crtc_index(crtc);

		WARN_ON(drm_crtc_vblank_get(crtc) != 0);

		dpu->event = crtc->state->event;
		crtc->state->event = NULL;
	}
}

static void dpu_crtc_atomic_flush(struct drm_crtc *crtc,
				  struct drm_crtc_state *old_state)

{
	struct sprd_dpu *dpu = crtc_to_dpu(crtc);

	DRM_DEBUG("drm_crtc_helper_funcs->atomic_flush()\n");

	if (dpu->core && dpu->core->run)
		dpu->core->run(&dpu->ctx);
}

static int dpu_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct sprd_dpu *dpu = crtc_to_dpu(crtc);

	DRM_INFO("drm_crtc_funcs->enable_vblank()\n");

	if (dpu->core && dpu->core->enable_vsync)
		dpu->core->enable_vsync(&dpu->ctx);

	return 0;
}

static void dpu_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct sprd_dpu *dpu = crtc_to_dpu(crtc);

	DRM_INFO("drm_crtc_funcs->disable_vblank()\n");

	if (dpu->core && dpu->core->disable_vsync)
		dpu->core->disable_vsync(&dpu->ctx);
}

static const struct drm_crtc_helper_funcs dpu_crtc_helper_funcs = {
	.atomic_check	= dpu_crtc_atomic_check,
	.atomic_begin	= dpu_crtc_atomic_begin,
	.atomic_flush	= dpu_crtc_atomic_flush,
	.atomic_enable	= dpu_crtc_atomic_enable,
	.atomic_disable	= dpu_crtc_atomic_disable,
};

static const struct drm_crtc_funcs dpu_crtc_funcs = {
	.destroy	= drm_crtc_cleanup,
	.set_config	= drm_atomic_helper_set_config,
	.page_flip	= drm_atomic_helper_page_flip,
	.reset		= drm_atomic_helper_crtc_reset,
	.atomic_duplicate_state	= drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_crtc_destroy_state,
	.enable_vblank	= dpu_crtc_enable_vblank,
	.disable_vblank	= dpu_crtc_disable_vblank,
};

static int dpu_crtc_init(struct drm_device *drm, struct drm_crtc *crtc,
			 struct drm_plane *primary)
{
	struct device_node *port;
	int err;

	/*
	 * set crtc port so that drm_of_find_possible_crtcs call works
	 */
	port = of_parse_phandle(drm->dev->of_node, "ports", 0);
	if (!port) {
		DRM_ERROR("find 'ports' phandle of %s failed\n",
			  drm->dev->of_node->full_name);
		return -EINVAL;
	}
	of_node_put(port);
	crtc->port = port;

	err = drm_crtc_init_with_planes(drm, crtc, primary, NULL,
					&dpu_crtc_funcs, NULL);
	if (err) {
		DRM_ERROR("failed to init crtc.\n");
		return err;
	}

	drm_mode_crtc_set_gamma_size(crtc, 256);

	drm_crtc_helper_add(crtc, &dpu_crtc_helper_funcs);

	DRM_INFO("crtc init ok\n");
	return 0;
}

static int dpu_clk_update(struct sprd_dpu *dpu,
				u32 new_val, int howto)
{
	int err;
	struct dpu_context *ctx = &dpu->ctx;

	if (dpu->clk && dpu->clk->update) {
		err = dpu->clk->update(ctx, DISPC_CLK_ID_DPI, 153600000);
		if (err) {
			DRM_ERROR("Failed to set pixel clock.\n");
			return err;
		}
	}

	return 0;
}

static int32_t sprd_dpu_init(struct sprd_dpu *dpu)
{
	struct dpu_context *ctx = &dpu->ctx;

	if (dpu->glb && dpu->glb->power)
		dpu->glb->power(ctx, true);
	if (dpu->glb && dpu->glb->enable)
		dpu->glb->enable(ctx);

	if (dpu->clk && dpu->clk->init)
		dpu->clk->init(ctx);
	if (dpu->clk && dpu->clk->enable)
		dpu->clk->enable(ctx);

	dpu_clk_update(dpu, 60, SPRD_FORCE_FPS);

	if (dpu->core && dpu->core->init)
		dpu->core->init(ctx);
	if (dpu->core && dpu->core->ifconfig)
		dpu->core->ifconfig(ctx);

	/* for zebu/vdk, refresh immediately */
	if (dpu->core && dpu->core->run)
		dpu->core->run(ctx);

	ctx->is_inited = true;

	return 0;
}

static void dpu_crtc_finish_page_flip(struct sprd_dpu *dpu)
{
	struct drm_device *drm = dpu->crtc.dev;
	unsigned long flags;

	spin_lock_irqsave(&drm->event_lock, flags);
	if (dpu->event) {
		drm_crtc_send_vblank_event(&dpu->crtc, dpu->event);
		drm_crtc_vblank_put(&dpu->crtc);
		dpu->event = NULL;
	}
	spin_unlock_irqrestore(&drm->event_lock, flags);
}

static irqreturn_t sprd_dpu_isr(int irq, void *data)
{
	struct sprd_dpu *dpu = data;
	struct dpu_context *ctx = &dpu->ctx;
	u32 int_mask = 0;

	if (dpu->core && dpu->core->isr)
		int_mask = dpu->core->isr(ctx);

	if (int_mask & DISPC_INT_ERR_MASK)
		DRM_ERROR("Warning: dpu underflow!\n");

	if (int_mask & DISPC_INT_DPI_VSYNC_MASK) {
		drm_crtc_handle_vblank(&dpu->crtc);
		dpu_crtc_finish_page_flip(dpu);
	}

	return IRQ_HANDLED;
}

static int dpu_irq_request(struct sprd_dpu *dpu)
{
	int err;
	int irq_num;

	irq_num = irq_of_parse_and_map(dpu->dev.of_node, 0);
	if (!irq_num) {
		DRM_ERROR("error: dpu parse irq num failed\n");
		return -EINVAL;
	}
	DRM_INFO("dpu irq_num = %d\n", irq_num);

	err = request_irq(irq_num, sprd_dpu_isr, 0, "DISPC", dpu);
	if (err) {
		DRM_ERROR("error: dpu request irq failed\n");
		return -EINVAL;
	}

	return 0;
}

static int sprd_dpu_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct sprd_drm *sprd = drm->dev_private;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct drm_plane *plane;
	int err;

	DRM_INFO("component_ops->bind()\n");

	plane = dpu_plane_init(drm, dpu);
	if (IS_ERR_OR_NULL(plane)) {
		err = PTR_ERR(plane);
		return err;
	}

	err = dpu_crtc_init(drm, &dpu->crtc, plane);
	if (err)
		return err;

	sprd_dpu_init(dpu);
	dpu_irq_request(dpu);

	sprd->dpu_dev = dev;

	DRM_INFO("display controller init OK\n");

	return 0;
}

static void sprd_dpu_unbind(struct device *dev, struct device *master,
	void *data)
{
	struct sprd_dpu *dpu = dev_get_drvdata(dev);

	DRM_INFO("component_ops->unbind()\n");

	drm_crtc_cleanup(&dpu->crtc);
}

static const struct component_ops dpu_component_ops = {
	.bind = sprd_dpu_bind,
	.unbind = sprd_dpu_unbind,
};

static int dpu_context_init(struct sprd_dpu *dpu,
				struct device_node *np)
{
	uint32_t temp;
	struct resource r;
	struct dpu_context *ctx = &dpu->ctx;

	if (dpu->core && dpu->core->parse_dt)
		dpu->core->parse_dt(&dpu->ctx, np);
	if (dpu->clk && dpu->clk->parse_dt)
		dpu->clk->parse_dt(&dpu->ctx, np);
	if (dpu->glb && dpu->glb->parse_dt)
		dpu->glb->parse_dt(&dpu->ctx, np);

	if (!of_property_read_u32(np, "dev-id", &temp))
		ctx->id = temp;

	if (of_address_to_resource(np, 0, &r)) {
		DRM_ERROR("parse dt base address failed\n");
		return -ENODEV;
	}
	ctx->base = (unsigned long)ioremap_nocache(r.start,
					resource_size(&r));
	if (ctx->base == 0) {
		DRM_ERROR("ioremap base address failed\n");
		return -EFAULT;
	}

	ctx->is_stopped = true;
	ctx->disable_flip = false;
	sema_init(&ctx->refresh_lock, 1);
	init_waitqueue_head(&ctx->wait_queue);

	ctx->if_type = SPRD_DISPC_IF_DPI;
	ctx->vsync_report_rate = 60;
	ctx->vsync_ratio_to_panel = 1;

	return 0;
}

static int dpu_device_register(struct sprd_dpu *dpu,
				struct device *parent)
{
	int err;

//	dpu->dev.class = display_class;
	dpu->dev.parent = parent;
	dpu->dev.of_node = parent->of_node;
	dev_set_name(&dpu->dev, "dpu");
	dev_set_drvdata(&dpu->dev, dpu);

	err = device_register(&dpu->dev);
	if (err)
		DRM_ERROR("dpu device register failed\n");

	return err;
}

static int sprd_dpu_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct sprd_dpu *dpu;
	const char *str;

	dpu = devm_kzalloc(&pdev->dev, sizeof(*dpu), GFP_KERNEL);
	if (!dpu)
		return -ENOMEM;

	if (!of_property_read_string(np, "sprd,ip", &str))
		dpu->core = dpu_core_ops_attach(str);
	else
		DRM_ERROR("sprd,ip was not found\n");

	if (!of_property_read_string(np, "sprd,soc", &str)) {
		dpu->clk = dpu_clk_ops_attach(str);
		dpu->glb = dpu_glb_ops_attach(str);
	} else
		DRM_ERROR("sprd,soc was not found\n");

	if (dpu_context_init(dpu, np))
		return -EINVAL;

	dpu_device_register(dpu, &pdev->dev);
//	sprd_dpu_sysfs_init(&dpu->dev);
//	dpu_notifier_register(dpu);
	platform_set_drvdata(pdev, dpu);

//	pm_runtime_set_active(&pdev->dev);
//	pm_runtime_get_noresume(&pdev->dev);
//	pm_runtime_enable(&pdev->dev);

	DRM_INFO("dpu driver probe success\n");

	return component_add(&pdev->dev, &dpu_component_ops);
}

static int sprd_dpu_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &dpu_component_ops);
	return 0;
}

static const struct of_device_id dpu_match_table[] = {
	{ .compatible = "sprd,display-processor",},
	{},
};

static struct platform_driver sprd_dpu_driver = {
	.probe = sprd_dpu_probe,
	.remove = sprd_dpu_remove,
	.driver = {
		.name = "sprd-dpu-drv",
		.of_match_table = dpu_match_table,
	},
};
module_platform_driver(sprd_dpu_driver);

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("SPRD Display Controller Driver");
MODULE_LICENSE("GPL v2");
