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


#include <linux/component.h>
#include <linux/mutex.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>

#include <drm/drmP.h>
#include <drm/drm_of.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>

#include "sprd_drm.h"

int sprd_drm_kms_cleanup(struct drm_device *drm)
{
	struct sprd_drm *sprd = drm->dev_private;

	if (sprd->fbdev) {
//		sprd_drm_fbdev_fini(drm);
		sprd->fbdev = NULL;
	}

	drm_kms_helper_poll_fini(drm);
	drm_atomic_helper_shutdown(drm);
	drm_mode_config_cleanup(drm);
	devm_kfree(drm->dev, sprd);
	drm->dev_private = NULL;

	return 0;
}

static void sprd_fbdev_output_poll_changed(struct drm_device *drm)
{
	struct sprd_drm *sprd = drm->dev_private;

	DRM_INFO("drm_mode_config_funcs->output_poll_changed()\n");

	//sprd_dsi_set_output_client(drm);

	if (sprd->fbdev) {
		DRM_INFO("call drm_fb_helper_hotplug_event()\n");
		drm_fb_helper_hotplug_event(sprd->fbdev);
	}
//	else
//		sprd->fbdev = sprd_drm_fbdev_init(drm);
}

static const struct drm_mode_config_funcs sprd_drm_mode_config_funcs = {
	.fb_create = drm_fb_cma_create,
	.output_poll_changed = sprd_fbdev_output_poll_changed,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static void sprd_drm_mode_config_init(struct drm_device *drm)
{
	drm_mode_config_init(drm);

	drm->mode_config.min_width = 0;
	drm->mode_config.min_height = 0;
	drm->mode_config.max_width = 2048;
	drm->mode_config.max_height = 2048;

	drm->mode_config.funcs = &sprd_drm_mode_config_funcs;
}

static int sprd_drm_load(struct drm_device *drm, unsigned long flags)
{
	struct sprd_drm *sprd;
	int ret;

	DRM_INFO("drm_driver->load()\n");

	sprd = devm_kzalloc(drm->dev, sizeof(*sprd), GFP_KERNEL);
	if (!sprd)
		return -ENOMEM;

	drm->dev_private = sprd;
	sprd->drm = drm;

	sprd_drm_mode_config_init(drm);

	/* bind and init sub drivers */
	ret = component_bind_all(drm->dev, drm);
	if (ret) {
		DRM_ERROR("failed to bind all component.\n");
		goto err_dc_cleanup;
	}

	/* vblank init */
	ret = drm_vblank_init(drm, drm->mode_config.num_crtc);
	if (ret) {
		DRM_ERROR("failed to initialize vblank.\n");
		goto err_unbind_all;
	}
	/* with irq_enabled = true, we can use the vblank feature. */
	drm->irq_enabled = true;

	/* reset all the states of crtc/plane/encoder/connector */
	drm_mode_config_reset(drm);

	/* init kms poll for handling hpd */
	drm_kms_helper_poll_init(drm);

	/* force detection after connectors init */
	(void)drm_helper_hpd_irq_event(drm);

	return 0;

err_unbind_all:
	component_unbind_all(drm->dev, drm);
err_dc_cleanup:
	drm_mode_config_cleanup(drm);
	devm_kfree(drm->dev, sprd);
	drm->dev_private = NULL;

	return ret;
}

static const struct file_operations sprd_drm_fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.release	= drm_release,
	.unlocked_ioctl	= drm_ioctl,
	.compat_ioctl	= drm_compat_ioctl,
	.poll		= drm_poll,
	.read		= drm_read,
	.llseek		= no_llseek,
	.mmap		= drm_gem_cma_mmap,
};

static struct drm_driver sprd_drm_drv = {
	.driver_features	= DRIVER_GEM | DRIVER_MODESET | DRIVER_PRIME |
				  DRIVER_ATOMIC | DRIVER_HAVE_IRQ,
	.load			= sprd_drm_load,
	.fops			= &sprd_drm_fops,

	.gem_free_object	= drm_gem_cma_free_object,
	.gem_vm_ops		= &drm_gem_cma_vm_ops,
	.dumb_create		= drm_gem_cma_dumb_create_internal,

	.prime_handle_to_fd	= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle	= drm_gem_prime_fd_to_handle,
	.gem_prime_export	= drm_gem_prime_export,
	.gem_prime_import	= drm_gem_prime_import,
	.gem_prime_get_sg_table = drm_gem_cma_prime_get_sg_table,
	.gem_prime_import_sg_table = drm_gem_cma_prime_import_sg_table,
	.gem_prime_vmap		= drm_gem_cma_prime_vmap,
	.gem_prime_vunmap	= drm_gem_cma_prime_vunmap,
	.gem_prime_mmap		= drm_gem_cma_prime_mmap,

	.name			= "sprd",
	.desc			= "Spreadtrum SoCs' DRM Driver",
	.date			= "20180501",
	.major			= 1,
	.minor			= 0,
};

static int sprd_drm_bind(struct device *dev)
{
	struct drm_device *drm;
	int err;

	DRM_INFO("component_master_ops->bind()\n");

	drm = drm_dev_alloc(&sprd_drm_drv, dev);
	if (IS_ERR(drm))
		return PTR_ERR(drm);

	dev_set_drvdata(dev, drm);

	err = drm_dev_register(drm, 0);
	if (err < 0)
		goto unref;

	return 0;

unref:
	drm_dev_unref(drm);
	return err;
}

static void sprd_drm_unbind(struct device *dev)
{
	DRM_INFO("component_master_ops->unbind()\n");
	drm_put_dev(dev_get_drvdata(dev));
}

static const struct component_master_ops drm_component_ops = {
	.bind = sprd_drm_bind,
	.unbind = sprd_drm_unbind,
};

static int compare_of(struct device *dev, void *data)
{
	struct device_node *np = data;

	if (!strcmp(np->name, "port"))
		return true;

	return dev->of_node == np;
}

static int sprd_drm_probe(struct platform_device *pdev)
{
	return drm_of_component_probe(&pdev->dev, compare_of,
					&drm_component_ops);
}

static int sprd_drm_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &drm_component_ops);
	return 0;
}

static const struct of_device_id drm_match_table[] = {
	{ .compatible = "sprd,display-subsystem",},
	{},
};
MODULE_DEVICE_TABLE(of, drm_match_table);

static struct platform_driver sprd_drm_driver = {
	.probe = sprd_drm_probe,
	.remove = sprd_drm_remove,
	.driver = {
		.name = "sprd-drm-drv",
		.of_match_table = drm_match_table,
	},
};

module_platform_driver(sprd_drm_driver);

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("SPRD DRM KMS Master Driver");
MODULE_LICENSE("GPL v2");
