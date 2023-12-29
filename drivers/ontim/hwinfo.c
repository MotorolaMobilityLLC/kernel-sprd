/* hwinfo simplified as follow:
 A. bootloader created basic hwinfo in /sys/firmware/devicetree/base/hwinfo/, you can call hwinfo_get_prop() to get the value
 B. if you want to create simple sysnode, please refer to below version_of_hwinfo
 C. if you want to create complex sysnode, please refer to below card_present
 Note: in order to reduce the dependency among modules, it's better to clone above code in your module
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/err.h>
#include <linux/kobject.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/errno.h>
#include "hwinfo.h"

#undef  pr_fmt
#define pr_fmt(fmt) "hwinfo: " fmt

struct kobject *hwinfo = NULL;
EXPORT_SYMBOL(hwinfo);

static char* version_of_hwinfo = VERSION_OF;
module_param(version_of_hwinfo, charp, 0444);

int is_manila_dts(void)
{
	struct device_node *node;
	const char *prop;
	static int rc = -1;

	if (rc >= 0) return rc;
	rc = 0;
	if (!(node = of_find_node_by_path("/"))) return rc; 
	if (of_property_read_string(node, "sprd,sc-id", &prop)) return rc;
	rc = !!strstr(prop, "manila");
	return rc;
}
EXPORT_SYMBOL(is_manila_dts);

const char *hwinfo_get_prop(const char *prop_name)
{
	static struct device_node *node = NULL;
	const char *prop;
	int rc = -1;
	if (!node)
		node = of_find_node_by_path("/hwinfo");
	if (node)
		rc = of_property_read_string(node, prop_name, &prop);
	return rc ? "error" : prop;
}
EXPORT_SYMBOL(hwinfo_get_prop);

static ssize_t RF_GPIO_show(struct kobject *dev, struct kobj_attribute *attr, char *buf)
{
	int val = 0;
	char str[5] = {0};

	val = gpio_get_value(188);
	pr_info("%s: gapio140 val = %d\n", __func__, val);
	str[3] = val ? '1' : '0';
	
	val = gpio_get_value(226);
        pr_info("%s: gapio178 val = %d\n", __func__, val);
	str[2] = val ? '1' : '0';

	val = gpio_get_value(227);
        pr_info("%s: gapio179 val = %d\n", __func__, val);
	str[1] = val ? '1' : '0';

	val = gpio_get_value(228);
        pr_info("%s: gapio180 val = %d\n", __func__, val);
	str[0] = val ? '1' : '0';

	return sysfs_emit(buf, "%s\n", str);
}
static KOBJ_ATTR_RO(RF_GPIO);

static ssize_t cable_gpio_show(struct kobject *dev, struct kobj_attribute *attr, char *buf)
{
	int val = 0;
	const char *msg = "unknown";

	val = gpio_get_value(226);
	if(0 == val)
		gpio_get_value(227);
	pr_info("%s: val = %d\n", __func__, val);
	msg = val ? "0" : "1";

	return sysfs_emit(buf, "%s\n", msg);
}
static KOBJ_ATTR_RO(cable_gpio);

static ssize_t card_present_show(struct kobject *dev, struct kobj_attribute *attr, char *buf)
{
	struct device_node *dn;
	int gpio, val;
	const char *msg = "unknown";
	enum of_gpio_flags flags = OF_GPIO_ACTIVE_LOW;

	dn = of_find_node_with_property(NULL, "cd-gpios");
	if (!dn) {
		pr_err("not found cd-gpios\n");
	} else {
		gpio = of_get_named_gpio_flags(dn, "cd-gpios", 0, &flags);
		val = gpio_get_value(gpio);
		pr_info("%s: gpio(%d) val = %d flag=0x%x\n", __func__, gpio, val, flags);
		if (flags & OF_GPIO_ACTIVE_LOW)
			val = !val;
		msg = val ? "yes" : "no";
	}
	return sysfs_emit(buf, "%s\n", msg);
}

static KOBJ_ATTR_RO(card_present);


#define GPIO_USAGE "Usage: gpio dir val name\n  dir: 0 in; 1 out; val: 0 low; 1 high;\nExample: 64 1 1 tp_reset\n"

static ssize_t gpio_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	return sysfs_emit(buf, "%s\n", GPIO_USAGE);
}

static ssize_t gpio_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	int gpio=0, dir=0, val=0;
	char name[32]="no_name";
#if !IS_ENABLED(CONFIG_UPDATE_GPIOS_NO_LIMIT)
	static const int allowed_gpios[] = {235};
	int i;
#endif
	int ret = sscanf(buf, "%d %d %d %30s", &gpio, &dir, &val, name);
        pr_err("%s: sscanf=%d gpio=%d dir=%d val=%d name=%s\n", __func__, ret, gpio, dir, val, name);
	if ((ret < 3) || (gpio < 0) || (dir < 0) || (dir > 1) || (val < 0) || (val > 2)) {
		pr_err("gpio_store: Invalid parameter!!\n" GPIO_USAGE);
		return -EINVAL;
	}
#if !IS_ENABLED(CONFIG_UPDATE_GPIOS_NO_LIMIT)
	for (i = 0; i < ARRAY_SIZE(allowed_gpios); i++)
		if (allowed_gpios[i] == gpio)
			break;
	if (i == ARRAY_SIZE(allowed_gpios)) {
		pr_err("gpio_store: not allowed gpio\n");
		return -EINVAL;
	}
#endif
	ret = gpio_is_valid(gpio);
	if (!ret) {
		pr_err("%s: Invalid gpio: %d\n", __func__, gpio);
		return -EINVAL;
	}
	ret = gpio_request(gpio, name);
	switch (ret) {
	case -EBUSY:
		pr_err("%s: GPIO %d is already requested\n", __func__, gpio);
		break;
	case 0:
		break;
	default:
		pr_err("%s: Failed to request GPIO %d, ret=%d\n", __func__, gpio, ret);
	}
	if (dir)
		ret = gpio_direction_output(gpio, val);
	else
		ret = gpio_direction_input(gpio);
	pr_err("%s: set ret=%d\n", __func__, ret);
	if (ret < 0)
		return ret;
	return n;
}

static KOBJ_ATTR_RW(gpio);

static struct attribute * hwinfo_attrs[] = {
	&dev_attr_card_present.attr,
	&dev_attr_gpio.attr,
	&dev_attr_RF_GPIO.attr,
	&dev_attr_cable_gpio.attr,
	NULL,
};

static struct attribute_group hwinfo_group = {
	.attrs = hwinfo_attrs,
};

static int __init hwinfo_init(void)
{
	int res = -1;

	pr_err("hwinfo_init\n");
	pr_err("version_of_hwinfo=%s\n", version_of_hwinfo);
	pr_err("version_of_lk=%s\n", hwinfo_get_prop("version_of_lk"));

	if (hwinfo) {
		pr_err("Already exist\n");
		return -EEXIST;
	}

	hwinfo = kobject_create_and_add("hwinfo", NULL);
	if (!hwinfo) {
		pr_err("add hwinfo fail\n");
		return -ENOMEM;
	}

	res= sysfs_create_group(hwinfo, &hwinfo_group);
	if (res)
		goto put_hwinfo;

	return 0;

put_hwinfo:
	pr_err("create sysfs fail\n");
	kobject_put(hwinfo);
	return res;
}

static void __exit hwinfo_exit(void)
{
	if (hwinfo)
		kobject_put(hwinfo);
	hwinfo = NULL;
	return ;
}

fs_initcall_sync(hwinfo_init);
module_exit(hwinfo_exit);

MODULE_AUTHOR("bsp@ontim");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Product Hardward Info");
