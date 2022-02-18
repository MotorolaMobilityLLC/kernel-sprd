/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_DRM_H_
#define _SPRD_DRM_H_

#include <drm/drm_atomic.h>
#include <drm/drm_print.h>

struct sprd_drm {
	struct drm_device *drm;
	struct device *gsp_dev;
	struct drm_atomic_state *state;
};

extern struct platform_driver sprd_dummy_crtc_driver;
extern struct platform_driver sprd_dummy_connector_driver;

extern struct platform_driver sprd_dpu_driver;
extern struct platform_driver sprd_backlight_driver;

extern struct platform_driver sprd_dsi_driver;
extern struct platform_driver sprd_dphy_driver;
extern struct mipi_dsi_driver sprd_panel_driver;

#ifdef CONFIG_COMPAT
long sprd_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#endif

#endif /* _SPRD_DRM_H_ */
