// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/dma-buf.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_plane_helper.h>

#include "sprd_crtc.h"
#include "sprd_drm.h"
#include "sprd_gem.h"
#include "sprd_plane.h"

static int sprd_plane_prepare_fb(struct drm_plane *plane,
				struct drm_plane_state *new_state)
{
	struct drm_plane_state *curr_state = plane->state;
	struct sprd_crtc *crtc = to_sprd_crtc(new_state->crtc);

	if ((curr_state->fb == new_state->fb) || !new_state->fb)
		return 0;

	if (crtc->ops->prepare_fb)
		crtc->ops->prepare_fb(crtc, new_state);

	return 0;
}

static void sprd_plane_cleanup_fb(struct drm_plane *plane,
				struct drm_plane_state *old_state)
{
	struct drm_plane_state *curr_state = plane->state;
	struct sprd_crtc *crtc = to_sprd_crtc(old_state->crtc);

	if ((curr_state->fb == old_state->fb) || !old_state->fb)
		return;

	if (crtc->ops->cleanup_fb)
		crtc->ops->cleanup_fb(crtc, old_state);
}

static void sprd_plane_atomic_update(struct drm_plane *plane,
				    struct drm_plane_state *old_state)
{
	struct drm_plane_state *state = plane->state;
	struct drm_framebuffer *fb = plane->state->fb;
	struct drm_gem_object *obj;
	struct sprd_gem_obj *sprd_gem;
	struct sprd_crtc *crtc = to_sprd_crtc(plane->state->crtc);
	struct sprd_plane *p = to_sprd_plane(plane);
	struct sprd_plane_state *s = to_sprd_plane_state(state);
	struct sprd_crtc_layer *layer = &crtc->layers[p->index];
	int i;

	if (plane->state->crtc->state->active_changed) {
		DRM_DEBUG("resume or suspend, no need to update plane\n");
		return;
	}

	if (s->pallete_en) {
		layer->index = p->index;
		layer->dst_x = state->crtc_x;
		layer->dst_y = state->crtc_y;
		layer->dst_w = state->crtc_w;
		layer->dst_h = state->crtc_h;
		layer->alpha = state->alpha;
		layer->blending = state->pixel_blend_mode;
		layer->pallete_en = s->pallete_en;
		layer->pallete_color = s->pallete_color;
		crtc->pending_planes++;
		DRM_DEBUG("%s() pallete_color = %u, index = %u\n",
			__func__, layer->pallete_color, layer->index);
		return;
	}

	layer->index = p->index;
	layer->src_x = state->src_x >> 16;
	layer->src_y = state->src_y >> 16;
	layer->src_w = state->src_w >> 16;
	layer->src_h = state->src_h >> 16;
	layer->dst_x = state->crtc_x;
	layer->dst_y = state->crtc_y;
	layer->dst_w = state->crtc_w;
	layer->dst_h = state->crtc_h;
	layer->alpha = state->alpha;
	layer->rotation = state->rotation;
	layer->blending = state->pixel_blend_mode;
	layer->rotation = state->rotation;
	layer->planes = fb->format->num_planes;
	layer->format = fb->format->format;
	layer->xfbc = fb->modifier;
	layer->header_size_r = s->fbc_hsize_r;
	layer->header_size_y = s->fbc_hsize_y;
	layer->header_size_uv = s->fbc_hsize_uv;
	layer->y2r_coef = s->y2r_coef;
	layer->pallete_en = s->pallete_en;
	layer->pallete_color = s->pallete_color;

	DRM_DEBUG("%s() alpha = %u, blending = %u, rotation = %u, y2r_coef = %u\n",
		  __func__, layer->alpha, layer->blending, layer->rotation, s->y2r_coef);

	DRM_DEBUG("%s() xfbc = %u, hsize_r = %u, hsize_y = %u, hsize_uv = %u\n",
		  __func__, layer->xfbc, layer->header_size_r,
		  layer->header_size_y, layer->header_size_uv);

	for (i = 0; i < layer->planes; i++) {
		obj = drm_gem_fb_get_obj(fb, i);
		sprd_gem = to_sprd_gem_obj(obj);
		layer->addr[i] = sprd_gem->dma_addr + fb->offsets[i];
		layer->pitch[i] = fb->pitches[i];
	}

	crtc->pending_planes++;
}

static int sprd_plane_atomic_check(struct drm_plane *plane,
				  struct drm_plane_state *state)
{
	DRM_DEBUG("%s()\n", __func__);

	return 0;
}

static void sprd_plane_atomic_disable(struct drm_plane *plane,
				     struct drm_plane_state *old_state)
{
	struct sprd_plane *p = to_sprd_plane(plane);

	/*
	 * NOTE:
	 * The dpu->core->flip() will disable all the planes each time.
	 * So there is no need to impliment the atomic_disable() function.
	 * But this function can not be removed, because it will change
	 * to call atomic_update() callback instead. Which will cause
	 * kernel panic in sprd_plane_atomic_update().
	 *
	 * We do nothing here but just print a debug log.
	 */
	DRM_DEBUG("%s() layer_id = %u\n", __func__, p->index);
}

static const struct drm_plane_helper_funcs sprd_plane_helper_funcs = {
	.prepare_fb = sprd_plane_prepare_fb,
	.cleanup_fb = sprd_plane_cleanup_fb,
	.atomic_check = sprd_plane_atomic_check,
	.atomic_update = sprd_plane_atomic_update,
	.atomic_disable = sprd_plane_atomic_disable,
};

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
	s->state.alpha = 255;
	s->state.pixel_blend_mode = DRM_MODE_BLEND_PIXEL_NONE;
}

static struct drm_plane_state *
sprd_plane_atomic_duplicate_state(struct drm_plane *plane)
{
	struct sprd_plane_state *s;
	struct sprd_plane_state *old_state = to_sprd_plane_state(plane->state);

	DRM_DEBUG("%s()\n", __func__);

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane, &s->state);

	WARN_ON(s->state.plane != plane);

	s->fbc_hsize_r = old_state->fbc_hsize_r;
	s->fbc_hsize_y = old_state->fbc_hsize_y;
	s->fbc_hsize_uv = old_state->fbc_hsize_uv;
	s->y2r_coef = old_state->y2r_coef;
	s->pallete_en = old_state->pallete_en;
	s->pallete_color = old_state->pallete_color;

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

	if (property == p->fbc_hsize_r_property)
		s->fbc_hsize_r = val;
	else if (property == p->fbc_hsize_y_property)
		s->fbc_hsize_y = val;
	else if (property == p->fbc_hsize_uv_property)
		s->fbc_hsize_uv = val;
	else if (property == p->y2r_coef_property)
		s->y2r_coef = val;
	else if (property == p->pallete_en_property)
		s->pallete_en = val;
	else if (property == p->pallete_color_property)
		s->pallete_color = val;
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

	if (property == p->fbc_hsize_r_property)
		*val = s->fbc_hsize_r;
	else if (property == p->fbc_hsize_y_property)
		*val = s->fbc_hsize_y;
	else if (property == p->fbc_hsize_uv_property)
		*val = s->fbc_hsize_uv;
	else if (property == p->y2r_coef_property)
		*val = s->y2r_coef;
	else if (property == p->pallete_en_property)
		*val = s->pallete_en;
	else if (property == p->pallete_color_property)
		*val = s->pallete_color;
	else {
		DRM_ERROR("property %s is invalid\n", property->name);
		return -EINVAL;
	}

	return 0;
}

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

static int sprd_plane_create_properties(struct sprd_plane *p, int index)
{
	struct drm_property *prop;
	unsigned int supported_modes = BIT(DRM_MODE_BLEND_PIXEL_NONE) |
				       BIT(DRM_MODE_BLEND_PREMULTI) |
				       BIT(DRM_MODE_BLEND_COVERAGE);

	/* create rotation property */
	drm_plane_create_rotation_property(&p->plane,
					   DRM_MODE_ROTATE_0,
					   DRM_MODE_ROTATE_MASK |
					   DRM_MODE_REFLECT_MASK);

	/* create alpha property */
	drm_plane_create_alpha_property(&p->plane);

	/* create blend mode property */
	drm_plane_create_blend_mode_property(&p->plane, supported_modes);

	/* create zpos property */
	drm_plane_create_zpos_immutable_property(&p->plane, index);

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

	/* create y2r coef property */
	prop = drm_property_create_range(p->plane.dev, 0,
			"YUV2RGB coef", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&p->plane.base, prop, 0);
	p->y2r_coef_property = prop;

	/* create pallete enable property */
	prop = drm_property_create_range(p->plane.dev, 0,
			"pallete enable", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&p->plane.base, prop, 0);
	p->pallete_en_property = prop;

	/* create pallete color property */
	prop = drm_property_create_range(p->plane.dev, 0,
			"pallete color", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&p->plane.base, prop, 0);
	p->pallete_color_property = prop;

	return 0;
}

struct drm_plane *sprd_plane_init(struct drm_device *drm,
					struct sprd_crtc_capability *cap,
					enum drm_plane_type type)
{
	struct drm_plane *primary = NULL;
	struct sprd_plane *p = NULL;
	int err, i;

	for (i = 0; i < cap->max_layers; i++) {

		p = devm_kzalloc(drm->dev, sizeof(*p), GFP_KERNEL);
		if (!p)
			return ERR_PTR(-ENOMEM);

		err = drm_universal_plane_init(drm, &p->plane,
					       1 << drm->mode_config.num_crtc,
					       &sprd_plane_funcs, cap->fmts_ptr,
					       cap->fmts_cnt, NULL,
					       type, NULL);
		if (err) {
			DRM_ERROR("failed to initialize primary plane\n");
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
