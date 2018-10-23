/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <video/videomode.h>

#include "disp_lib.h"
#include "sprd_panel.h"
#include "sysfs_display.h"

static ssize_t name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct panel_info *info = &panel->info;
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->of_node->name);

	return ret;
}
static DEVICE_ATTR_RO(name);

static ssize_t lane_num_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct panel_info *info = &panel->info;
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%u\n", info->lanes);

	return ret;
}
static DEVICE_ATTR_RO(lane_num);

static ssize_t dpi_freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct panel_info *info = &panel->info;
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%u\n", info->mode.clock * 1000);

	return ret;
}
static DEVICE_ATTR_RO(dpi_freq);

static ssize_t resolution_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct panel_info *info = &panel->info;
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%ux%u\n", info->mode.hdisplay,
						info->mode.vdisplay);

	return ret;
}
static DEVICE_ATTR_RO(resolution);

static ssize_t screen_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct panel_info *info = &panel->info;
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%umm x %umm\n", info->mode.width_mm,
						info->mode.height_mm);

	return ret;
}
static DEVICE_ATTR_RO(screen_size);

static ssize_t hporch_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct videomode vm;
	int ret;

	drm_display_mode_to_videomode(&panel->info.mode, &vm);
	ret = snprintf(buf, PAGE_SIZE, "hfp=%u hbp=%u hsync=%u\n",
				   vm.hfront_porch,
				   vm.hback_porch,
				   vm.hsync_len);

	return ret;
}

static ssize_t hporch_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct videomode vm;
	u32 val[4] = {0};
	int len;

	len = str_to_u32_array(buf, 0, val);
	drm_display_mode_to_videomode(&panel->info.mode, &vm);

	switch (len) {
	/* Fall through */
	case 3:
		vm.hsync_len = val[2];
	/* Fall through */
	case 2:
		vm.hback_porch = val[1];
	/* Fall through */
	case 1:
		vm.hfront_porch = val[0];
	/* Fall through */
	default:
		break;
	}

	drm_display_mode_from_videomode(&vm, &panel->info.mode);

	return count;
}
static DEVICE_ATTR_RW(hporch);

static ssize_t vporch_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct videomode vm;
	int ret;

	drm_display_mode_to_videomode(&panel->info.mode, &vm);
	ret = snprintf(buf, PAGE_SIZE, "vfp=%u vbp=%u vsync=%u\n",
				   vm.vfront_porch,
				   vm.vback_porch,
				   vm.vsync_len);

	return ret;
}

static ssize_t vporch_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct videomode vm;
	u32 val[4] = {0};
	int len;

	len = str_to_u32_array(buf, 0, val);
	drm_display_mode_to_videomode(&panel->info.mode, &vm);

	switch (len) {
	/* Fall through */
	case 3:
		vm.vsync_len = val[2];
	/* Fall through */
	case 2:
		vm.vback_porch = val[1];
	/* Fall through */
	case 1:
		vm.vfront_porch = val[0];
	/* Fall through */
	default:
		break;
	}

	drm_display_mode_from_videomode(&vm, &panel->info.mode);

	return count;
}
static DEVICE_ATTR_RW(vporch);

static ssize_t suspend_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	pm_runtime_put_sync(dev->parent);
	return count;
}
static DEVICE_ATTR_WO(suspend);

static ssize_t resume_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	pm_runtime_get_sync(dev->parent);
	return count;
}
static DEVICE_ATTR_WO(resume);

static struct attribute *panel_attrs[] = {
	&dev_attr_name.attr,
	&dev_attr_lane_num.attr,
	&dev_attr_dpi_freq.attr,
	&dev_attr_resolution.attr,
	&dev_attr_screen_size.attr,
	&dev_attr_hporch.attr,
	&dev_attr_vporch.attr,
	&dev_attr_suspend.attr,
	&dev_attr_resume.attr,
	NULL,
};
ATTRIBUTE_GROUPS(panel);

int sprd_panel_sysfs_init(struct device *dev)
{
	int rc;

	rc = sysfs_create_groups(&(dev->kobj), panel_groups);
	if (rc)
		pr_err("create panel attr node failed, rc=%d\n", rc);

	return rc;
}
EXPORT_SYMBOL(sprd_panel_sysfs_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("leon.he@spreadtrum.com");
MODULE_DESCRIPTION("Add panel attribute nodes for userspace");
