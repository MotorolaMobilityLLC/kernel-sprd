/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_PLANE_H_
#define _SPRD_PLANE_H_

#define to_sprd_plane(x)		container_of(x, struct sprd_plane, plane)
#define to_sprd_plane_state(x)	container_of(x, struct sprd_plane_state, state)

struct sprd_plane_state {
	struct drm_plane_state state;
	u32 fbc_hsize_r;
	u32 fbc_hsize_y;
	u32 fbc_hsize_uv;
	u32 y2r_coef;
	u32 pallete_en;
	u32 pallete_color;
};

struct sprd_plane {
	struct drm_plane plane;
	struct drm_property *fbc_hsize_r_property;
	struct drm_property *fbc_hsize_y_property;
	struct drm_property *fbc_hsize_uv_property;
	struct drm_property *y2r_coef_property;
	struct drm_property *pallete_en_property;
	struct drm_property *pallete_color_property;
	u32 index;
};

struct drm_plane *sprd_plane_init(struct drm_device *dev,
					struct sprd_crtc_capability *cap,
					enum drm_plane_type type);

#endif /* _SPRD_PLANE_H_ */
