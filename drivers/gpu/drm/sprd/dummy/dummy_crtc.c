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

#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_vblank.h>
#include <linux/component.h>
#include <linux/hrtimer.h>
#include <linux/of.h>
#include <linux/time.h>

#include "sprd_drm.h"

#define DRM_MODE_BLEND_PREMULTI		2
#define DRM_MODE_BLEND_COVERAGE		1
#define DRM_MODE_BLEND_PIXEL_NONE	0

struct dummy_crtc {
	struct drm_crtc crtc;
	struct drm_pending_vblank_event *event;
	struct hrtimer vsync_timer;
};

struct dummy_plane {
	struct drm_plane plane;
	struct drm_property *alpha_property;
	struct drm_property *blend_mode_property;
	struct drm_property *fbc_hsize_r_property;
	struct drm_property *fbc_hsize_y_property;
	struct drm_property *fbc_hsize_uv_property;
};

struct dummy_plane_state {
	struct drm_plane_state state;
	u8 alpha;
	u8 blend_mode;
	u32 fbc_hsize_r;
	u32 fbc_hsize_y;
	u32 fbc_hsize_uv;
};

static inline struct dummy_crtc *crtc_to_dummy(struct drm_crtc *crtc)
{
	return crtc ? container_of(crtc, struct dummy_crtc, crtc) : NULL;
}

static inline struct dummy_plane *to_dummy_plane(struct drm_plane *plane)
{
	return container_of(plane, struct dummy_plane, plane);
}

static inline struct
dummy_plane_state *to_dummy_plane_state(const struct drm_plane_state *state)
{
	return container_of(state, struct dummy_plane_state, state);
}

static enum hrtimer_restart vsync_timer_func(struct hrtimer *timer)
{
	struct dummy_crtc *dummy = container_of(timer, struct dummy_crtc,
						vsync_timer);
	drm_crtc_handle_vblank(&dummy->crtc);

	hrtimer_forward_now(timer, ns_to_ktime(16666666));

	return HRTIMER_RESTART;
}

static void sprd_dummy_plane_atomic_update(struct drm_plane *plane,
				    struct drm_plane_state *old_state)
{
	DRM_DEBUG("\n");
}

static void sprd_dummy_plane_reset(struct drm_plane *plane)
{
	struct dummy_plane_state *s;

	DRM_INFO("%s()\n", __func__);

	if (plane->state) {
		__drm_atomic_helper_plane_destroy_state(plane->state);

		s = to_dummy_plane_state(plane->state);
		memset(s, 0, sizeof(*s));
	} else {
		s = kzalloc(sizeof(*s), GFP_KERNEL);
		if (!s)
			return;
		plane->state = &s->state;
	}

	s->state.plane = plane;
	s->state.zpos = 0;
	s->alpha = 255;
	s->blend_mode = DRM_MODE_BLEND_PIXEL_NONE;
}

static struct drm_plane_state *
sprd_dummy_plane_atomic_duplicate_state(struct drm_plane *plane)
{
	struct dummy_plane_state *s;

	DRM_DEBUG("%s()\n", __func__);

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane, &s->state);

	WARN_ON(s->state.plane != plane);

	s->alpha = 255;

	return &s->state;
}

static void sprd_dummy_plane_atomic_destroy_state(struct drm_plane *plane,
					    struct drm_plane_state *state)
{
	DRM_DEBUG("%s()\n", __func__);

	__drm_atomic_helper_plane_destroy_state(state);
	kfree(to_dummy_plane_state(state));
}

static int sprd_dummy_plane_atomic_set_property(struct drm_plane *plane,
					  struct drm_plane_state *state,
					  struct drm_property *property,
					  u64 val)
{
	struct dummy_plane *p = to_dummy_plane(plane);
	struct dummy_plane_state *s = to_dummy_plane_state(state);

	DRM_DEBUG("%s() name = %s, val = %llu\n",
		  __func__, property->name, val);

	if (property == p->alpha_property)
		s->alpha = val;
	else if (property == p->blend_mode_property)
		s->blend_mode = val;
	else if (property == p->fbc_hsize_r_property)
		s->fbc_hsize_r = val;
	else if (property == p->fbc_hsize_y_property)
		s->fbc_hsize_y = val;
	else if (property == p->fbc_hsize_uv_property)
		s->fbc_hsize_uv = val;
	else {
		DRM_ERROR("property %s is invalid\n", property->name);
		return -EINVAL;
	}

	return 0;
}

static int sprd_dummy_plane_atomic_get_property(struct drm_plane *plane,
					  const struct drm_plane_state *state,
					  struct drm_property *property,
					  u64 *val)
{
	struct dummy_plane *p = to_dummy_plane(plane);
	const struct dummy_plane_state *s = to_dummy_plane_state(state);

	DRM_DEBUG("%s() name = %s\n", __func__, property->name);

	if (property == p->alpha_property)
		*val = s->alpha;
	else if (property == p->blend_mode_property)
		*val = s->blend_mode;
	else if (property == p->fbc_hsize_r_property)
		*val = s->fbc_hsize_r;
	else if (property == p->fbc_hsize_y_property)
		*val = s->fbc_hsize_y;
	else if (property == p->fbc_hsize_uv_property)
		*val = s->fbc_hsize_uv;
	else {
		DRM_ERROR("property %s is invalid\n", property->name);
		return -EINVAL;
	}

	return 0;
}

static int sprd_dummy_plane_create_properties(struct dummy_plane *p)
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
	drm_plane_create_zpos_immutable_property(&p->plane, 0);

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

	/* create fbc header size property */
	prop = drm_property_create_range(p->plane.dev, 0,
			"FBC header size RGB", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&p->plane.base, prop, 0);
	p->fbc_hsize_r_property = prop;

	prop = drm_property_create_range(p->plane.dev, 0,
			"FBC header size Y", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&p->plane.base, prop, 0);
	p->fbc_hsize_y_property = prop;

	prop = drm_property_create_range(p->plane.dev, 0,
			"FBC header size UV", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&p->plane.base, prop, 0);
	p->fbc_hsize_uv_property = prop;

	return 0;
}

static const struct drm_plane_helper_funcs sprd_dummy_plane_helper_funcs = {
	.atomic_update = sprd_dummy_plane_atomic_update,
};

static const struct drm_plane_funcs sprd_dummy_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane	= drm_atomic_helper_disable_plane,
	.destroy = drm_plane_cleanup,
	.reset = sprd_dummy_plane_reset,
	.atomic_duplicate_state = sprd_dummy_plane_atomic_duplicate_state,
	.atomic_destroy_state = sprd_dummy_plane_atomic_destroy_state,
	.atomic_set_property = sprd_dummy_plane_atomic_set_property,
	.atomic_get_property = sprd_dummy_plane_atomic_get_property,
};

static struct drm_plane *sprd_dummy_plane_init(struct drm_device *drm,
					struct dummy_crtc *dummy)
{
	const u32 primary_fmts[] = {
		DRM_FORMAT_XRGB8888, DRM_FORMAT_XBGR8888,
		DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888,
	};
	struct dummy_plane *p;
	int err;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return ERR_PTR(-ENOMEM);

	err = drm_universal_plane_init(drm, &p->plane, 1,
				       &sprd_dummy_plane_funcs, primary_fmts,
				       ARRAY_SIZE(primary_fmts), NULL,
				       DRM_PLANE_TYPE_PRIMARY, NULL);
	if (err) {
		kfree(p);
		DRM_ERROR("fail to init primary plane\n");
		return ERR_PTR(err);
	}

	drm_plane_helper_add(&p->plane, &sprd_dummy_plane_helper_funcs);

	sprd_dummy_plane_create_properties(p);

	return &p->plane;
}

static void sprd_dummy_crtc_atomic_enable(struct drm_crtc *crtc,
				   struct drm_crtc_state *old_state)
{
	DRM_INFO("%s()\n", __func__);
}

static void sprd_dummy_crtc_atomic_disable(struct drm_crtc *crtc,
				    struct drm_crtc_state *old_state)
{
	DRM_INFO("%s()\n", __func__);
}

static void sprd_dummy_crtc_atomic_flush(struct drm_crtc *crtc,
				  struct drm_crtc_state *old_state)

{
	struct dummy_crtc *dummy = crtc_to_dummy(crtc);
	struct drm_device *drm = dummy->crtc.dev;

	DRM_DEBUG("%s()\n", __func__);

	spin_lock_irq(&drm->event_lock);
	if (crtc->state->event) {
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		crtc->state->event = NULL;
	}
	spin_unlock_irq(&drm->event_lock);
}

static int sprd_dummy_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct dummy_crtc *dummy = crtc_to_dummy(crtc);

	DRM_INFO("%s()\n", __func__);

	hrtimer_start(&dummy->vsync_timer, ns_to_ktime(16666666),
		      HRTIMER_MODE_REL);

	return 0;
}

static void sprd_dummy_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct dummy_crtc *dummy = crtc_to_dummy(crtc);

	DRM_INFO("%s()\n", __func__);

	hrtimer_cancel(&dummy->vsync_timer);
}

static const struct drm_crtc_helper_funcs sprd_dummy_crtc_helper_funcs = {
	.atomic_enable	= sprd_dummy_crtc_atomic_enable,
	.atomic_disable	= sprd_dummy_crtc_atomic_disable,
	.atomic_flush = sprd_dummy_crtc_atomic_flush,
};

static const struct drm_crtc_funcs sprd_dummy_crtc_funcs = {
	.destroy	= drm_crtc_cleanup,
	.set_config	= drm_atomic_helper_set_config,
	.page_flip	= drm_atomic_helper_page_flip,
	.reset		= drm_atomic_helper_crtc_reset,
	.atomic_duplicate_state	= drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_crtc_destroy_state,
	.enable_vblank	= sprd_dummy_crtc_enable_vblank,
	.disable_vblank	= sprd_dummy_crtc_disable_vblank,
};

static int sprd_dummy_crtc_init(struct drm_device *drm, struct drm_crtc *crtc,
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
					&sprd_dummy_crtc_funcs, NULL);
	if (err) {
		DRM_ERROR("failed to init crtc.\n");
		return err;
	}

	drm_crtc_helper_add(crtc, &sprd_dummy_crtc_helper_funcs);

	return 0;
}

static int sprd_dummy_crtc_bind(struct device *dev, struct device *master,
				void *data)
{
	struct drm_device *drm = data;
	struct dummy_crtc *dummy = dev_get_drvdata(dev);
	struct drm_plane *primary;
	int err;

	primary = sprd_dummy_plane_init(drm, dummy);
	if (IS_ERR_OR_NULL(primary)) {
		err = PTR_ERR(primary);
		return err;
	}

	err = sprd_dummy_crtc_init(drm, &dummy->crtc, primary);
	if (err)
		return err;

	return 0;
}

static void sprd_dummy_crtc_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct dummy_crtc *dummy = dev_get_drvdata(dev);

	drm_crtc_cleanup(&dummy->crtc);
}

static const struct component_ops dummy_component_ops = {
	.bind = sprd_dummy_crtc_bind,
	.unbind = sprd_dummy_crtc_unbind,
};

static int sprd_dummy_crtc_probe(struct platform_device *pdev)
{
	struct dummy_crtc *dummy;

	dummy = devm_kzalloc(&pdev->dev, sizeof(*dummy), GFP_KERNEL);
	if (!dummy)
		return -ENOMEM;

	hrtimer_init(&dummy->vsync_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	dummy->vsync_timer.function = vsync_timer_func;

	platform_set_drvdata(pdev, dummy);

	return component_add(&pdev->dev, &dummy_component_ops);
}

static int sprd_dummy_crtc_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &dummy_component_ops);
	return 0;
}

static const struct of_device_id dummy_crtc_match_table[] = {
	{ .compatible = "sprd,dummy-crtc",},
	{},
};

static struct platform_driver sprd_dummy_crtc_driver = {
	.probe = sprd_dummy_crtc_probe,
	.remove = sprd_dummy_crtc_remove,
	.driver = {
		.name = "sprd-dummy-crtc-drv",
		.of_match_table = dummy_crtc_match_table,
	},
};
module_platform_driver(sprd_dummy_crtc_driver);

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("Dummy CRTC Driver for SPRD SoC");
MODULE_LICENSE("GPL v2");
