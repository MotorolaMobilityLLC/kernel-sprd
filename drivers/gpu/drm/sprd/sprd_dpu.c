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
	struct drm_property *alpha_property;
	struct drm_property *blend_mode_property;
	u32 index;
};

struct sprd_plane_state {
	struct drm_plane_state state;
	u8 alpha;
	u8 blend_mode;
};

LIST_HEAD(dpu_core_head);
LIST_HEAD(dpu_clk_head);
LIST_HEAD(dpu_glb_head);

static int sprd_dpu_init(struct sprd_dpu *dpu);
static int sprd_dpu_uninit(struct sprd_dpu *dpu);

static inline struct sprd_plane *to_sprd_plane(struct drm_plane *plane)
{
	return container_of(plane, struct sprd_plane, plane);
}

static inline struct
sprd_plane_state *to_sprd_plane_state(const struct drm_plane_state *state)
{
	return container_of(state, struct sprd_plane_state, state);
}

static int sprd_plane_atomic_check(struct drm_plane *plane,
				  struct drm_plane_state *state)
{
	DRM_DEBUG("%s()\n", __func__);

	return 0;
}

static void sprd_plane_atomic_update(struct drm_plane *plane,
				    struct drm_plane_state *old_state)
{
	struct drm_plane_state *state = plane->state;
	struct drm_framebuffer *fb = plane->state->fb;
	struct drm_gem_object *obj;
	struct sprd_gem_obj *sprd_gem;
	struct sprd_plane *p = to_sprd_plane(plane);
	struct sprd_plane_state *s = to_sprd_plane_state(state);
	struct sprd_dpu *dpu = crtc_to_dpu(plane->state->crtc);
	struct sprd_dpu_layer layer = {};
	int i;

	layer.index = p->index;
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
	layer.alpha = s->alpha;
	layer.blending = s->blend_mode;

	DRM_DEBUG("%s() alpha = %u, blending = %u, rotation = %u\n",
		  __func__, layer.alpha, layer.blending, layer.rotation);

	for (i = 0; i < layer.planes; i++) {
		obj = drm_gem_fb_get_obj(fb, i);
		sprd_gem = to_sprd_gem_obj(obj);
		layer.addr[i] = sprd_gem->dma_addr + fb->offsets[i];
		layer.pitch[i] = fb->pitches[i];
	}

	if (dpu->core && dpu->core->layer)
		dpu->core->layer(&dpu->ctx, &layer);
}

static void sprd_plane_atomic_disable(struct drm_plane *plane,
				     struct drm_plane_state *old_state)
{
	struct sprd_plane *p = to_sprd_plane(plane);
	struct sprd_dpu *dpu = crtc_to_dpu(old_state->crtc);

	DRM_DEBUG("%s() layer_id = %u\n", __func__, p->index);

	if (dpu->core && dpu->core->clean)
		dpu->core->clean(&dpu->ctx, p->index);
}

static void sprd_plane_reset(struct drm_plane *plane)
{
	struct sprd_plane *p = to_sprd_plane(plane);
	struct sprd_plane_state *s;

	DRM_INFO("%s()\n", __func__);

	if (plane->state) {
		__drm_atomic_helper_plane_destroy_state(plane->state);

		s = to_sprd_plane_state(plane->state);
		memset(s, 0, sizeof(*s));
	} else {
		s = kzalloc(sizeof(*s), GFP_KERNEL);
		if (!s)
			return;
		plane->state = &s->state;
	}

	s->state.plane = plane;
	s->state.zpos = p->index;
	s->alpha = 255;
	s->blend_mode = DRM_MODE_BLEND_PIXEL_NONE;
}

static struct drm_plane_state *
sprd_plane_atomic_duplicate_state(struct drm_plane *plane)
{
	struct sprd_plane_state *s;

	DRM_DEBUG("%s()\n", __func__);

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane, &s->state);

	WARN_ON(s->state.plane != plane);

	s->alpha = 255;

	return &s->state;
}

static void sprd_plane_atomic_destroy_state(struct drm_plane *plane,
					    struct drm_plane_state *state)
{
	DRM_DEBUG("%s()\n", __func__);

	__drm_atomic_helper_plane_destroy_state(state);
	kfree(to_sprd_plane_state(state));
}

static int sprd_plane_atomic_set_property(struct drm_plane *plane,
					  struct drm_plane_state *state,
					  struct drm_property *property,
					  u64 val)
{
	struct sprd_plane *p = to_sprd_plane(plane);
	struct sprd_plane_state *s = to_sprd_plane_state(state);

	DRM_DEBUG("%s() name = %s, val = %llu\n",
		  __func__, property->name, val);

	if (property == p->alpha_property)
		s->alpha = val;
	else if (property == p->blend_mode_property)
		s->blend_mode = val;
	else {
		DRM_ERROR("property %s is invalid\n", property->name);
		return -EINVAL;
	}

	return 0;
}

static int sprd_plane_atomic_get_property(struct drm_plane *plane,
					  const struct drm_plane_state *state,
					  struct drm_property *property,
					  u64 *val)
{
	struct sprd_plane *p = to_sprd_plane(plane);
	const struct sprd_plane_state *s = to_sprd_plane_state(state);

	DRM_DEBUG("%s() name = %s\n", __func__, property->name);

	if (property == p->alpha_property)
		*val = s->alpha;
	else if (property == p->blend_mode_property)
		*val = s->blend_mode;
	else {
		DRM_ERROR("property %s is invalid\n", property->name);
		return -EINVAL;
	}

	return 0;
}

static int sprd_plane_create_properties(struct sprd_plane *p, int index)
{
	struct drm_property *prop;
	static const struct drm_prop_enum_list blend_mode_enum_list[] = {
		{ DRM_MODE_BLEND_PIXEL_NONE, "None" },
		{ DRM_MODE_BLEND_PREMULTI, "Pre-multiplied" },
		{ DRM_MODE_BLEND_COVERAGE, "Coverage" },
	};

	/* create rotation property */
	drm_plane_create_rotation_property(&p->plane,
					   DRM_MODE_ROTATE_0,
					   DRM_MODE_ROTATE_MASK |
					   DRM_MODE_REFLECT_MASK);

	/* create zpos property */
	drm_plane_create_zpos_immutable_property(&p->plane, index);

	/* create layer alpha property */
	prop = drm_property_create_range(p->plane.dev, 0, "alpha", 0, 255);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&p->plane.base, prop, 255);
	p->alpha_property = prop;

	/* create blend mode property */
	prop = drm_property_create_enum(p->plane.dev, DRM_MODE_PROP_ENUM,
					"pixel blend mode",
					blend_mode_enum_list,
					ARRAY_SIZE(blend_mode_enum_list));
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&p->plane.base, prop,
				   DRM_MODE_BLEND_PIXEL_NONE);
	p->blend_mode_property = prop;

	return 0;
}

static const struct drm_plane_helper_funcs sprd_plane_helper_funcs = {
	.atomic_check = sprd_plane_atomic_check,
	.atomic_update = sprd_plane_atomic_update,
	.atomic_disable = sprd_plane_atomic_disable,
};

static const struct drm_plane_funcs sprd_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane	= drm_atomic_helper_disable_plane,
	.destroy = drm_plane_cleanup,
	.reset = sprd_plane_reset,
	.atomic_duplicate_state = sprd_plane_atomic_duplicate_state,
	.atomic_destroy_state = sprd_plane_atomic_destroy_state,
	.atomic_set_property = sprd_plane_atomic_set_property,
	.atomic_get_property = sprd_plane_atomic_get_property,
};

static struct drm_plane *sprd_plane_init(struct drm_device *drm,
					struct sprd_dpu *dpu)
{
	struct drm_plane *primary = NULL;
	struct sprd_plane *p = NULL;
	struct dpu_capability cap = {};
	int err;
	int i;

	if (dpu->core && dpu->core->capability)
		dpu->core->capability(&dpu->ctx, &cap);

	for (i = 0; i < cap.max_layers; i++) {

		p = kzalloc(sizeof(*p), GFP_KERNEL);
		if (!p)
			return ERR_PTR(-ENOMEM);

		err = drm_universal_plane_init(drm, &p->plane, 1,
					       &sprd_plane_funcs, cap.fmts_ptr,
					       cap.fmts_cnt, NULL,
					       DRM_PLANE_TYPE_PRIMARY, NULL);
		if (err) {
			kfree(p);
			DRM_ERROR("fail to init primary plane\n");
			return ERR_PTR(err);
		}

		drm_plane_helper_add(&p->plane, &sprd_plane_helper_funcs);

		sprd_plane_create_properties(p, i);

		p->index = i;
		if (i == 0)
			primary = &p->plane;
	}

	if (p)
		DRM_INFO("dpu plane init ok\n");

	return primary;
}

static void sprd_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct sprd_dpu *dpu = crtc_to_dpu(crtc);

	DRM_INFO("%s()\n", __func__);

	if ((crtc->mode.hdisplay == crtc->mode.htotal) ||
	    (crtc->mode.vdisplay == crtc->mode.vtotal))
		dpu->ctx.if_type = SPRD_DISPC_IF_EDPI;
	else
		dpu->ctx.if_type = SPRD_DISPC_IF_DPI;

	drm_display_mode_to_videomode(&crtc->mode, &dpu->ctx.vm);
}

static void sprd_crtc_atomic_enable(struct drm_crtc *crtc,
				   struct drm_crtc_state *old_state)
{
	struct sprd_dpu *dpu = crtc_to_dpu(crtc);

	DRM_INFO("%s()\n", __func__);

	sprd_dpu_init(dpu);

	drm_crtc_vblank_on(crtc);
}

static void sprd_crtc_atomic_disable(struct drm_crtc *crtc,
				    struct drm_crtc_state *old_state)
{
	struct sprd_dpu *dpu = crtc_to_dpu(crtc);

	DRM_INFO("%s()\n", __func__);

	drm_crtc_vblank_off(crtc);

	sprd_dpu_uninit(dpu);
}

static int sprd_crtc_atomic_check(struct drm_crtc *crtc,
				 struct drm_crtc_state *state)
{
	DRM_DEBUG("%s()\n", __func__);

	return 0;
}

static void sprd_crtc_atomic_begin(struct drm_crtc *crtc,
				  struct drm_crtc_state *old_state)
{
	DRM_DEBUG("%s()\n", __func__);
}

static void sprd_crtc_atomic_flush(struct drm_crtc *crtc,
				  struct drm_crtc_state *old_state)

{
	struct sprd_dpu *dpu = crtc_to_dpu(crtc);
	struct drm_device *drm = dpu->crtc.dev;

	DRM_DEBUG("%s()\n", __func__);

	if (dpu->core && dpu->core->run && !dpu->ctx.is_stopped)
		dpu->core->run(&dpu->ctx);

	spin_lock_irq(&drm->event_lock);
	if (crtc->state->event) {
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		crtc->state->event = NULL;
	}
	spin_unlock_irq(&drm->event_lock);
}

static int sprd_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct sprd_dpu *dpu = crtc_to_dpu(crtc);

	DRM_INFO("%s()\n", __func__);

	if (dpu->core && dpu->core->enable_vsync)
		dpu->core->enable_vsync(&dpu->ctx);

	return 0;
}

static void sprd_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct sprd_dpu *dpu = crtc_to_dpu(crtc);

	DRM_INFO("%s()\n", __func__);

	if (dpu->core && dpu->core->disable_vsync)
		dpu->core->disable_vsync(&dpu->ctx);
}

static const struct drm_crtc_helper_funcs sprd_crtc_helper_funcs = {
	.mode_set_nofb	= sprd_crtc_mode_set_nofb,
	.atomic_check	= sprd_crtc_atomic_check,
	.atomic_begin	= sprd_crtc_atomic_begin,
	.atomic_flush	= sprd_crtc_atomic_flush,
	.atomic_enable	= sprd_crtc_atomic_enable,
	.atomic_disable	= sprd_crtc_atomic_disable,
};

static const struct drm_crtc_funcs sprd_crtc_funcs = {
	.destroy	= drm_crtc_cleanup,
	.set_config	= drm_atomic_helper_set_config,
	.page_flip	= drm_atomic_helper_page_flip,
	.reset		= drm_atomic_helper_crtc_reset,
	.atomic_duplicate_state	= drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_crtc_destroy_state,
	.enable_vblank	= sprd_crtc_enable_vblank,
	.disable_vblank	= sprd_crtc_disable_vblank,
};

static int sprd_crtc_init(struct drm_device *drm, struct drm_crtc *crtc,
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
					&sprd_crtc_funcs, NULL);
	if (err) {
		DRM_ERROR("failed to init crtc.\n");
		return err;
	}

	drm_mode_crtc_set_gamma_size(crtc, 256);

	drm_crtc_helper_add(crtc, &sprd_crtc_helper_funcs);

	DRM_INFO("%s() ok\n", __func__);
	return 0;
}

int sprd_dpu_run(struct sprd_dpu *dpu)
{
	struct dpu_context *ctx = &dpu->ctx;

	if (!ctx->is_stopped)
		return 0;

	if (dpu->core && dpu->core->run)
		dpu->core->run(ctx);

	ctx->is_stopped = false;

	return 0;
}

int sprd_dpu_stop(struct sprd_dpu *dpu)
{
	struct dpu_context *ctx = &dpu->ctx;

	if (ctx->is_stopped)
		return 0;

	if (dpu->core && dpu->core->stop)
		dpu->core->stop(ctx);

	ctx->is_stopped = true;

	return 0;
}

static int sprd_dpu_init(struct sprd_dpu *dpu)
{
	struct dpu_context *ctx = &dpu->ctx;

	if (dpu->ctx.is_inited)
		return 0;

	if (dpu->glb && dpu->glb->power)
		dpu->glb->power(ctx, true);
	if (dpu->glb && dpu->glb->enable)
		dpu->glb->enable(ctx);

	if (dpu->clk && dpu->clk->init)
		dpu->clk->init(ctx);
	if (dpu->clk && dpu->clk->enable)
		dpu->clk->enable(ctx);

	if (dpu->core && dpu->core->init)
		dpu->core->init(ctx);
	if (dpu->core && dpu->core->ifconfig)
		dpu->core->ifconfig(ctx);

	ctx->is_inited = true;

	return 0;
}

static int sprd_dpu_uninit(struct sprd_dpu *dpu)
{
	struct dpu_context *ctx = &dpu->ctx;

	if (!dpu->ctx.is_inited)
		return 0;

	if (dpu->core && dpu->core->uninit)
		dpu->core->uninit(ctx);
	if (dpu->clk && dpu->clk->disable)
		dpu->clk->disable(ctx);
	if (dpu->glb && dpu->glb->disable)
		dpu->glb->disable(ctx);
	if (dpu->glb && dpu->glb->power)
		dpu->glb->power(ctx, false);

	ctx->is_inited = false;

	return 0;
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

	if ((int_mask & DISPC_INT_DPI_VSYNC_MASK) && ctx->is_inited)
		drm_crtc_handle_vblank(&dpu->crtc);

	//sprd_crtc_finish_page_flip(dpu);

	return IRQ_HANDLED;
}

static int sprd_dpu_irq_request(struct sprd_dpu *dpu)
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
	dpu->ctx.irq = irq_num;

	return 0;
}

static int sprd_dpu_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct sprd_drm *sprd = drm->dev_private;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct drm_plane *plane;
	int err;

	DRM_INFO("%s()\n", __func__);

	plane = sprd_plane_init(drm, dpu);
	if (IS_ERR_OR_NULL(plane)) {
		err = PTR_ERR(plane);
		return err;
	}

	err = sprd_crtc_init(drm, &dpu->crtc, plane);
	if (err)
		return err;

	sprd_dpu_irq_request(dpu);

	sprd->dpu_dev = dev;

	return 0;
}

static void sprd_dpu_unbind(struct device *dev, struct device *master,
	void *data)
{
	struct sprd_dpu *dpu = dev_get_drvdata(dev);

	DRM_INFO("%s()\n", __func__);

	drm_crtc_cleanup(&dpu->crtc);
}

static const struct component_ops dpu_component_ops = {
	.bind = sprd_dpu_bind,
	.unbind = sprd_dpu_unbind,
};

static int sprd_dpu_device_create(struct sprd_dpu *dpu,
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

static int sprd_dpu_context_init(struct sprd_dpu *dpu,
				struct device_node *np)
{
	u32 temp;
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

	sema_init(&ctx->refresh_lock, 1);
	init_waitqueue_head(&ctx->wait_queue);

	ctx->vsync_report_rate = 60;
	ctx->vsync_ratio_to_panel = 1;

	return 0;
}

static int sprd_dpu_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *lcd_node;
	struct sprd_dpu *dpu;
	const char *str;
	int ret;

	lcd_node = platform_get_drvdata(pdev);
	if (!lcd_node) {
		DRM_INFO("panel is not attached, dpu probe deferred\n");
		return -EPROBE_DEFER;
	}

	dpu = devm_kzalloc(&pdev->dev, sizeof(*dpu), GFP_KERNEL);
	if (!dpu)
		return -ENOMEM;

	if (!of_property_read_string(np, "sprd,ip", &str))
		dpu->core = dpu_core_ops_attach(str);
	else
		DRM_WARN("sprd,ip was not found\n");

	if (!of_property_read_string(np, "sprd,soc", &str)) {
		dpu->clk = dpu_clk_ops_attach(str);
		dpu->glb = dpu_glb_ops_attach(str);
	} else
		DRM_WARN("sprd,soc was not found\n");

	ret = sprd_dpu_context_init(dpu, np);
	if (ret)
		return ret;

	sprd_dpu_device_create(dpu, &pdev->dev);
//	sprd_dpu_sysfs_init(&dpu->dev);
//	dpu_notifier_register(dpu);
	platform_set_drvdata(pdev, dpu);

//	pm_runtime_set_active(&pdev->dev);
//	pm_runtime_get_noresume(&pdev->dev);
//	pm_runtime_enable(&pdev->dev);

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
