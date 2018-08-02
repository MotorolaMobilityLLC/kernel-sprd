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

#ifndef __SPRD_DRM_DRV_H__
#define __SPRD_DRM_DRV_H__

#include <drm/drmP.h>
//#include <linux/sprd_ion.h>

#include <drm/drm_crtc.h>
#include <drm/drm_fb_helper.h>

#define MAX_CRTC	2

#define to_sprd_fbdev(x) container_of(x, struct sprd_fbdev, fb_helper)

struct sprd_drm {
	struct drm_device *drm;
	struct drm_fb_helper *fb_helper;
	struct drm_fb_helper *fbdev;
	struct drm_crtc *crtc[MAX_CRTC];
	struct {
		struct drm_atomic_state *state;
		struct work_struct work;
		struct mutex lock;
	} commit;
	struct device *gsp_dev;
};

struct sprd_fbdev {
	struct drm_fb_helper fb_helper;
	struct drm_framebuffer *fb;

//	struct ion_client *ion_client;
//	struct ion_handle *ion_handle;
//	struct iommu_map_format iommu_format;
	void *screen_base;
	unsigned long smem_start;
	unsigned long screen_size;
	int shared_fd;
};

extern void sprd_dsi_set_output_client(struct drm_device *dev);

struct drm_framebuffer *sprd_framebuffer_init(struct drm_device *dev,
		struct drm_mode_fb_cmd2 *mode_cmd);
struct drm_fb_helper *sprd_drm_fbdev_init(struct drm_device *dev);
void sprd_drm_fbdev_fini(struct drm_device *dev);


#endif /* __SPRD_DRM_DRV_H__ */
