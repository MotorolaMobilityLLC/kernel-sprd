// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/component.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <uapi/drm/sprd_drm_gsp.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>

#include "sprd_drm.h"
#include "sprd_drm_gsp.h"
#include "sprd_gem.h"
#include "sysfs/sysfs_display.h"

#define DRIVER_NAME	"sprd"
#define DRIVER_DESC	"Spreadtrum SoCs' DRM Driver"
#define DRIVER_DATE	"20200201"
#define DRIVER_MAJOR	1
#define DRIVER_MINOR	0

/**
 * drm_crtc_commit_put - release a reference to the CRTC commmit
 * @commit: CRTC commit
 *
 * This releases a reference to @commit which is freed after removing the
 * final reference. No locking required and callable from any context.
 */
static inline void sprd_drm_crtc_commit_put(struct drm_crtc_commit *commit)
{
	kref_put(&commit->ref, __drm_crtc_commit_free);
}


/**
 * DOC: implementing nonblocking commit
 *
 * Nonblocking atomic commits have to be implemented in the following sequence:
 *
 * 1. Run drm_atomic_helper_prepare_planes() first. This is the only function
 * which commit needs to call which can fail, so we want to run it first and
 * synchronously.
 *
 * 2. Synchronize with any outstanding nonblocking commit worker threads which
 * might be affected the new state update. This can be done by either cancelling
 * or flushing the work items, depending upon whether the driver can deal with
 * cancelled updates. Note that it is important to ensure that the framebuffer
 * cleanup is still done when cancelling.
 *
 * Asynchronous workers need to have sufficient parallelism to be able to run
 * different atomic commits on different CRTCs in parallel. The simplest way to
 * achieve this is by running them on the &system_unbound_wq work queue. Note
 * that drivers are not required to split up atomic commits and run an
 * individual commit in parallel - userspace is supposed to do that if it cares.
 * But it might be beneficial to do that for modesets, since those necessarily
 * must be done as one global operation, and enabling or disabling a CRTC can
 * take a long time. But even that is not required.
 *
 * 3. The software state is updated synchronously with
 * drm_atomic_helper_swap_state(). Doing this under the protection of all modeset
 * locks means concurrent callers never see inconsistent state. And doing this
 * while it's guaranteed that no relevant nonblocking worker runs means that
 * nonblocking workers do not need grab any locks. Actually they must not grab
 * locks, for otherwise the work flushing will deadlock.
 *
 * 4. Schedule a work item to do all subsequent steps, using the split-out
 * commit helpers: a) pre-plane commit b) plane commit c) post-plane commit and
 * then cleaning up the framebuffers after the old framebuffer is no longer
 * being displayed.
 *
 * The above scheme is implemented in the atomic helper libraries in
 * drm_atomic_helper_commit() using a bunch of helper functions. See
 * drm_atomic_helper_setup_commit() for a starting point.
 */

static int sprd_stall_checks(struct drm_crtc *crtc, bool nonblock)
{
	struct drm_crtc_commit *commit, *stall_commit = NULL;
	bool completed = true;
	int i;
	long ret = 0;

	spin_lock(&crtc->commit_lock);
	i = 0;
	list_for_each_entry(commit, &crtc->commit_list, commit_entry) {
		if (i == 0) {
			completed = try_wait_for_completion(&commit->flip_done);
			/* Userspace is not allowed to get ahead of the previous
			 * commit with nonblocking ones. */
			if (!completed && nonblock) {
				spin_unlock(&crtc->commit_lock);
				return -EBUSY;
			}
		} else if (i == 1) {
			stall_commit = drm_crtc_commit_get(commit);
			break;
		}

		i++;
	}
	spin_unlock(&crtc->commit_lock);

	if (!stall_commit)
		return 0;

	/* We don't want to let commits get ahead of cleanup work too much,
	 * stalling on 2nd previous commit means triple-buffer won't ever stall.
	 */
	ret = wait_for_completion_interruptible_timeout(&stall_commit->cleanup_done,
							10*HZ);
	if (ret == 0)
		DRM_ERROR("[CRTC:%d:%s] cleanup_done timed out\n",
			  crtc->base.id, crtc->name);

	sprd_drm_crtc_commit_put(stall_commit);

	return ret < 0 ? ret : 0;
}

static void sprd_release_crtc_commit(struct completion *completion)
{
	struct drm_crtc_commit *commit = container_of(completion,
						      typeof(*commit),
						      flip_done);

	sprd_drm_crtc_commit_put(commit);
}

static void sprd_init_commit(struct drm_crtc_commit *commit, struct drm_crtc *crtc)
{
	init_completion(&commit->flip_done);
	init_completion(&commit->hw_done);
	init_completion(&commit->cleanup_done);
	INIT_LIST_HEAD(&commit->commit_entry);
	kref_init(&commit->ref);
	commit->crtc = crtc;
}

static struct drm_crtc_commit *
sprd_crtc_or_fake_commit(struct drm_atomic_state *state, struct drm_crtc *crtc)
{
	if (crtc) {
		struct drm_crtc_state *new_crtc_state;

		new_crtc_state = drm_atomic_get_new_crtc_state(state, crtc);

		return new_crtc_state->commit;
	}

	if (!state->fake_commit) {
		state->fake_commit = kzalloc(sizeof(*state->fake_commit), GFP_KERNEL);
		if (!state->fake_commit)
			return NULL;

		sprd_init_commit(state->fake_commit, NULL);
	}

	return state->fake_commit;
}

/**
 * Gki check failed, it takes a long time to join the Linux community.
 * here is a bypass scheme.
 * sprd_drm_atomic_helper_setup_commit - setup possibly nonblocking commit
 * @state: new modeset state to be committed
 * @nonblock: whether nonblocking behavior is requested.
 *
 * This function prepares @state to be used by the atomic helper's support for
 * nonblocking commits. Drivers using the nonblocking commit infrastructure
 * should always call this function from their
 * &drm_mode_config_funcs.atomic_commit hook.
 *
 * To be able to use this support drivers need to use a few more helper
 * functions. drm_atomic_helper_wait_for_dependencies() must be called before
 * actually committing the hardware state, and for nonblocking commits this call
 * must be placed in the async worker. See also drm_atomic_helper_swap_state()
 * and its stall parameter, for when a driver's commit hooks look at the
 * &drm_crtc.state, &drm_plane.state or &drm_connector.state pointer directly.
 *
 * Completion of the hardware commit step must be signalled using
 * drm_atomic_helper_commit_hw_done(). After this step the driver is not allowed
 * to read or change any permanent software or hardware modeset state. The only
 * exception is state protected by other means than &drm_modeset_lock locks.
 * Only the free standing @state with pointers to the old state structures can
 * be inspected, e.g. to clean up old buffers using
 * drm_atomic_helper_cleanup_planes().
 *
 * At the very end, before cleaning up @state drivers must call
 * drm_atomic_helper_commit_cleanup_done().
 *
 * This is all implemented by in drm_atomic_helper_commit(), giving drivers a
 * complete and easy-to-use default implementation of the atomic_commit() hook.
 *
 * The tracking of asynchronously executed and still pending commits is done
 * using the core structure &drm_crtc_commit.
 *
 * By default there's no need to clean up resources allocated by this function
 * explicitly: drm_atomic_state_default_clear() will take care of that
 * automatically.
 *
 * Returns:
 *
 * 0 on success. -EBUSY when userspace schedules nonblocking commits too fast,
 * -ENOMEM on allocation failures and -EINTR when a signal is pending.
 */
int sprd_drm_atomic_helper_setup_commit(struct drm_atomic_state *state,
				   bool nonblock)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	struct drm_connector *conn;
	struct drm_connector_state *old_conn_state, *new_conn_state;
	struct drm_plane *plane;
	struct drm_plane_state *old_plane_state, *new_plane_state;
	struct drm_crtc_commit *commit;
	int i, ret;

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, i) {
		commit = kzalloc(sizeof(*commit), GFP_KERNEL);
		if (!commit)
			return -ENOMEM;

		sprd_init_commit(commit, crtc);

		new_crtc_state->commit = commit;

		ret = sprd_stall_checks(crtc, nonblock);
		if (ret)
			return ret;

		/* Drivers only send out events when at least either current or
		 * new CRTC state is active. Complete right away if everything
		 * stays off. */
		if (!old_crtc_state->active && !new_crtc_state->active) {
			complete_all(&commit->flip_done);
			continue;
		}

		/* Legacy cursor updates are fully unsynced. */
		if (state->legacy_cursor_update) {
			complete_all(&commit->flip_done);
			continue;
		}

		if (!new_crtc_state->event) {
			commit->event = kzalloc(sizeof(*commit->event),
						GFP_KERNEL);
			if (!commit->event)
				return -ENOMEM;

			new_crtc_state->event = commit->event;
		}

		new_crtc_state->event->base.completion = &commit->flip_done;
		new_crtc_state->event->base.completion_release = sprd_release_crtc_commit;
		drm_crtc_commit_get(commit);

		commit->abort_completion = true;

		state->crtcs[i].commit = commit;
		drm_crtc_commit_get(commit);
	}

	for_each_oldnew_connector_in_state(state, conn, old_conn_state, new_conn_state, i) {
		/* Userspace is not allowed to get ahead of the previous
		 * commit with nonblocking ones. */
		if (nonblock && old_conn_state->commit &&
		    !try_wait_for_completion(&old_conn_state->commit->flip_done))
			return -EBUSY;

		/* Always track connectors explicitly for e.g. link retraining. */
		commit = sprd_crtc_or_fake_commit(state, new_conn_state->crtc ?: old_conn_state->crtc);
		if (!commit)
			return -ENOMEM;

		new_conn_state->commit = drm_crtc_commit_get(commit);
	}

	for_each_oldnew_plane_in_state(state, plane, old_plane_state, new_plane_state, i) {
		/* Userspace is not allowed to get ahead of the previous
		 * commit with nonblocking ones. */
		if (nonblock && old_plane_state->commit &&
		    !try_wait_for_completion(&old_plane_state->commit->flip_done))
			return -EBUSY;

		/* Always track planes explicitly for async pageflip support. */
		commit = sprd_crtc_or_fake_commit(state, new_plane_state->crtc ?: old_plane_state->crtc);
		if (!commit)
			return -ENOMEM;

		new_plane_state->commit = drm_crtc_commit_get(commit);
	}

	return 0;
}


static void sprd_commit_tail(struct drm_atomic_state *old_state)
{
	struct drm_device *dev = old_state->dev;

	drm_atomic_helper_wait_for_dependencies(old_state);

	drm_atomic_helper_wait_for_fences(dev, old_state, false);

	drm_atomic_helper_commit_modeset_disables(dev, old_state);

	drm_atomic_helper_commit_modeset_enables(dev, old_state);

	drm_atomic_helper_commit_planes(dev, old_state,
					DRM_PLANE_COMMIT_ACTIVE_ONLY);

	drm_atomic_helper_commit_hw_done(old_state);

	drm_atomic_helper_cleanup_planes(dev, old_state);

	drm_atomic_helper_commit_cleanup_done(old_state);

	drm_atomic_state_put(old_state);
}

static void sprd_commit_work(struct work_struct *work)
{
	struct drm_atomic_state *state = container_of(work,
						      struct drm_atomic_state,
						      commit_work);
	sprd_commit_tail(state);
}

int sprd_atomic_helper_commit(struct drm_device *dev,
			struct drm_atomic_state *state, bool nonblock)
{
	int ret;

	ret = sprd_drm_atomic_helper_setup_commit(state, false);
	if (ret)
		return ret;

	INIT_WORK(&state->commit_work, sprd_commit_work);

	ret = drm_atomic_helper_prepare_planes(dev, state);
	if (ret)
		return ret;

	ret = drm_atomic_helper_swap_state(state, true);
	if (ret)
		goto err;

	drm_atomic_state_get(state);
	if (nonblock)
		queue_work(system_unbound_wq, &state->commit_work);
	else
		sprd_commit_tail(state);

	return 0;

err:
	drm_atomic_helper_cleanup_planes(dev, state);
	return ret;
}

static const struct drm_mode_config_funcs sprd_drm_mode_config_funcs = {
	.fb_create = drm_gem_fb_create,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = sprd_atomic_helper_commit,
};

static void sprd_drm_mode_config_init(struct drm_device *drm)
{
	drm_mode_config_init(drm);

	drm->mode_config.min_width = 0;
	drm->mode_config.min_height = 0;
	drm->mode_config.max_width = 8192;
	drm->mode_config.max_height = 8192;
	drm->mode_config.allow_fb_modifiers = true;

	drm->mode_config.funcs = &sprd_drm_mode_config_funcs;
}

static const struct drm_ioctl_desc sprd_ioctls[] = {
	DRM_IOCTL_DEF_DRV(SPRD_GSP_GET_CAPABILITY,
			sprd_gsp_get_capability_ioctl, 0),
	DRM_IOCTL_DEF_DRV(SPRD_GSP_TRIGGER,
			sprd_gsp_trigger_ioctl, 0),
};

static const struct file_operations sprd_drm_fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.release	= drm_release,
	.unlocked_ioctl	= drm_ioctl,
	.compat_ioctl	= drm_compat_ioctl,
	.poll		= drm_poll,
	.read		= drm_read,
	.llseek		= no_llseek,
	.mmap		= sprd_gem_mmap,
};

static struct drm_driver sprd_drm_drv = {
	.driver_features	= DRIVER_GEM | DRIVER_MODESET | DRIVER_ATOMIC,
	.fops			= &sprd_drm_fops,
	.gem_vm_ops		= &drm_gem_cma_vm_ops,
	.gem_free_object_unlocked	= sprd_gem_free_object,
	.dumb_create		= sprd_gem_dumb_create,
	.prime_fd_to_handle	= drm_gem_prime_fd_to_handle,
	.prime_handle_to_fd	= drm_gem_prime_handle_to_fd,
	.gem_prime_import	= drm_gem_prime_import,
	.gem_prime_export	= drm_gem_prime_export,
	.gem_prime_import_sg_table = sprd_gem_prime_import_sg_table,
	.gem_prime_get_sg_table = drm_gem_cma_prime_get_sg_table,
	.gem_prime_vmap		= drm_gem_cma_prime_vmap,
	.gem_prime_vunmap	= drm_gem_cma_prime_vunmap,
	.gem_prime_mmap		= drm_gem_cma_prime_mmap,

	.ioctls			= sprd_ioctls,
	.num_ioctls		= ARRAY_SIZE(sprd_ioctls),

	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= DRIVER_DATE,
	.major			= DRIVER_MAJOR,
	.minor			= DRIVER_MINOR,
};

static int sprd_drm_bind(struct device *dev)
{
	struct drm_device *drm;
	struct sprd_drm *sprd;
	int err;

	DRM_INFO("%s()\n", __func__);

	drm = drm_dev_alloc(&sprd_drm_drv, dev);
	if (IS_ERR(drm))
		return PTR_ERR(drm);

	dev_set_drvdata(dev, drm);

	sprd = devm_kzalloc(drm->dev, sizeof(*sprd), GFP_KERNEL);
	if (!sprd) {
		err = -ENOMEM;
		goto err_free_drm;
	}

	drm->dev_private = sprd;

	/* get the optional framebuffer memory resource */
	err = of_reserved_mem_device_init_by_idx(drm->dev,
				drm->dev->of_node, 0);
	if (err && err != -ENODEV) {
		DRM_ERROR("failed to obtain reserved memory\n");
		goto err_free_drm;
	}

	sprd_drm_mode_config_init(drm);

	/* bind and init sub drivers */
	err = component_bind_all(drm->dev, drm);
	if (err) {
		DRM_ERROR("failed to bind all component.\n");
		goto err_dc_cleanup;
	}

	/* vblank init */
	err = drm_vblank_init(drm, drm->mode_config.num_crtc);
	if (err) {
		DRM_ERROR("failed to initialize vblank.\n");
		goto err_unbind_all;
	}
	/* with irq_enabled = true, we can use the vblank feature. */
	drm->irq_enabled = true;

	/* reset all the states of crtc/plane/encoder/connector */
	drm_mode_config_reset(drm);

	/* init kms poll for handling hpd */
	drm_kms_helper_poll_init(drm);

	err = drm_dev_register(drm, 0);
	if (err < 0)
		goto err_kms_helper_poll_fini;

	return 0;

err_kms_helper_poll_fini:
	drm_kms_helper_poll_fini(drm);
err_unbind_all:
	component_unbind_all(drm->dev, drm);
err_dc_cleanup:
	drm_mode_config_cleanup(drm);
	of_reserved_mem_device_release(drm->dev);
err_free_drm:
	drm_dev_put(drm);
	return err;
}

static void sprd_drm_unbind(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	DRM_INFO("%s()\n", __func__);

	drm_dev_unregister(drm);

	drm_kms_helper_poll_fini(drm);

	drm_mode_config_cleanup(drm);

	component_unbind_all(drm->dev, drm);

	of_reserved_mem_device_release(drm->dev);

	drm_dev_put(drm);
}

static const struct component_master_ops drm_component_ops = {
	.bind = sprd_drm_bind,
	.unbind = sprd_drm_unbind,
};

static int compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static int sprd_drm_component_probe(struct device *dev,
			const struct component_master_ops *m_ops)
{
	struct device_node *ep, *port, *remote;
	struct component_match *match = NULL;
	int i;

	if (!dev->of_node)
		return -EINVAL;

	/*
	 * Bind the crtc's ports first, so that drm_of_find_possible_crtcs()
	 * called from encoder's .bind callbacks works as expected
	 */
	for (i = 0; ; i++) {
		port = of_parse_phandle(dev->of_node, "ports", i);
		if (!port)
			break;

		if (!of_device_is_available(port->parent)) {
			of_node_put(port);
			continue;
		}

		component_match_add(dev, &match, compare_of, port->parent);
		of_node_put(port);
	}

	if (i == 0) {
		dev_err(dev, "missing 'ports' property\n");
		return -ENODEV;
	}

	if (!match) {
		dev_err(dev, "no available port\n");
		return -ENODEV;
	}

	/*
	 * For bound crtcs, bind the encoders attached to their remote endpoint
	 */
	for (i = 0; ; i++) {
		port = of_parse_phandle(dev->of_node, "ports", i);
		if (!port)
			break;

		if (!of_device_is_available(port->parent)) {
			of_node_put(port);
			continue;
		}

		for_each_child_of_node(port, ep) {
			remote = of_graph_get_remote_port_parent(ep);
			if (!remote || !of_device_is_available(remote)) {
				of_node_put(remote);
				continue;
			} else if (!of_device_is_available(remote->parent)) {
				dev_warn(dev, "parent device of %s is not available\n",
					 remote->full_name);
				of_node_put(remote);
				continue;
			}

			component_match_add(dev, &match, compare_of, remote);
			of_node_put(remote);
		}
		of_node_put(port);
	}

	if (IS_ENABLED(CONFIG_DRM_SPRD_GSP)) {
		for (i = 0; ; i++) {
			port = of_parse_phandle(dev->of_node, "gsp", i);
			if (!port)
				break;

			if (!of_device_is_available(port->parent)) {
				of_node_put(port);
				continue;
			}

			component_match_add(dev, &match, compare_of, port);
			of_node_put(port);
		}
	}

	return component_master_add_with_match(dev, m_ops, match);
}

static int sprd_drm_probe(struct platform_device *pdev)
{
	int ret;

	ret = dma_set_mask_and_coherent(&pdev->dev, ~0);
	if (ret) {
		DRM_ERROR("dma_set_mask_and_coherent failed (%d)\n", ret);
		return ret;
	}

//	ret = drm_of_component_probe(&pdev->dev, compare_of, &drm_component_ops);
//	if(ret) {
//		DRM_ERROR("sprd_drm_probe error: %d\n", ret);
//		return ret;
//	}

	return sprd_drm_component_probe(&pdev->dev, &drm_component_ops);
}

static int sprd_drm_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &drm_component_ops);
	return 0;
}

static void sprd_drm_shutdown(struct platform_device *pdev)
{
	struct drm_device *drm = platform_get_drvdata(pdev);

	if (!drm) {
		DRM_WARN("drm device is not available, no shutdown\n");
		return;
	}

	drm_atomic_helper_shutdown(drm);
}

static const struct of_device_id drm_match_table[] = {
	{ .compatible = "sprd,display-subsystem", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, drm_match_table);

static struct platform_driver sprd_drm_driver = {
	.probe = sprd_drm_probe,
	.remove = sprd_drm_remove,
	.shutdown = sprd_drm_shutdown,
	.driver = {
		.name = "sprd-drm-drv",
		.of_match_table = drm_match_table,
	},
};

static struct platform_driver *sprd_drm_drivers[]  = {
#ifdef CONFIG_DRM_SPRD_DUMMY
	&sprd_dummy_crtc_driver,
	&sprd_dummy_connector_driver,
#endif
#ifdef CONFIG_DRM_SPRD_DPU0
	&sprd_dpu_driver,
#endif
#ifdef CONFIG_DRM_SPRD_DSI
	&sprd_dsi_driver,
	&sprd_dphy_driver,
#endif
	&sprd_drm_driver,
};

static int __init sprd_drm_init(void)
{
	int ret;

	ret = sprd_display_class_init();
	if (ret)
		return ret;

	ret = platform_register_drivers(sprd_drm_drivers,
					ARRAY_SIZE(sprd_drm_drivers));
	if (ret)
		return ret;

#ifdef CONFIG_DRM_SPRD_DSI
	mipi_dsi_driver_register(&sprd_panel_driver);
#endif

	return 0;
}

static void __exit sprd_drm_exit(void)
{
#ifdef CONFIG_DRM_SPRD_DSI
	mipi_dsi_driver_unregister(&sprd_panel_driver);
#endif

	platform_unregister_drivers(sprd_drm_drivers,
				    ARRAY_SIZE(sprd_drm_drivers));

}

module_init(sprd_drm_init);
module_exit(sprd_drm_exit);

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_AUTHOR("Kevin Tang <kevin.tang@unisoc.com>");
MODULE_DESCRIPTION("Unisoc DRM KMS Master Driver");
MODULE_LICENSE("GPL v2");
