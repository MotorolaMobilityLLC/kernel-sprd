#include <linux/backlight.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/sysfs.h>

#include "sprd_bl.h"

static ssize_t max_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	unsigned long maxbrightness;
	struct backlight_device *bd = dev_get_drvdata(dev);

	rc = kstrtoul(buf, 0, &maxbrightness);
	if (rc)
		return rc;

	mutex_lock(&bd->ops_lock);
	if (bd->ops) {
		pr_debug("set max_brightness to %lu\n", maxbrightness);
		bd->props.max_brightness = maxbrightness;
		if (bd->props.brightness > maxbrightness) {
			bd->props.brightness = maxbrightness;
			backlight_update_status(bd);
		}
	}
	mutex_unlock(&bd->ops_lock);

	return count;
}

static ssize_t max_brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct backlight_device *bd = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", bd->props.max_brightness);
}
static DEVICE_ATTR_RW(max_brightness);

static struct attribute *backlight_attrs[] = {
	&dev_attr_max_brightness.attr,
	NULL,
};
ATTRIBUTE_GROUPS(backlight);

int sprd_backlight_sysfs_init(struct device *dev)
{
	int rc;

	rc = sysfs_create_groups(&dev->kobj, backlight_groups);
	if (rc)
		pr_err("create backlight attr node failed, rc=%d\n", rc);

	return rc;
}
EXPORT_SYMBOL(sprd_backlight_sysfs_init);

MODULE_AUTHOR("Gaofeng Sheng <gaofeng.sheng@unisoc.com>");
MODULE_DESCRIPTION("Add backlight attribute nodes for userspace");
MODULE_LICENSE("GPL v2");
