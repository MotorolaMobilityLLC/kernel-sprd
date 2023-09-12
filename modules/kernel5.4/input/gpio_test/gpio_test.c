#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/device.h>

static ssize_t export_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t len)
{
	long gpio;
	int status;

	status = kstrtol(buf, 0, &gpio);
	if (status < 0)
		goto done;

	gpio_request(gpio, "gpio_test");
	status = gpio_export(gpio, true);

done:
	if (status)
		pr_err("%s: status %d\n", __func__, status);
	return len;
}
static CLASS_ATTR_WO(export);

static ssize_t unexport_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t len)
{
	long gpio;
	int status;

	status = kstrtol(buf, 0, &gpio);
	if (status < 0)
		goto done;

	gpio_unexport(gpio);
done:
	if (status < 0)
		pr_err("%s: status %d\n", __func__, status);
	return len;
}
static CLASS_ATTR_WO(unexport);

static struct attribute *gpio_test_class_attrs[] = {
	&class_attr_export.attr,
	&class_attr_unexport.attr,
	NULL,
};
ATTRIBUTE_GROUPS(gpio_test_class);

static struct class gpio_test_class = {
	.name = "gpio_test",
	.owner = THIS_MODULE,
	.class_groups = gpio_test_class_groups,
};

static int __init sprd_gpio_test_init(void)
{
	int status;

	status = class_register(&gpio_test_class);
	if (status < 0) {
		pr_err("%s: register gpio_test_class failed(%d)\n", __func__, status);
		return status;
	}

	return 0;
}

static void __exit sprd_gpio_test_exit(void)
{
	class_unregister(&gpio_test_class);
}

module_init(sprd_gpio_test_init);
module_exit(sprd_gpio_test_exit);

MODULE_DESCRIPTION("Spreadtrum GPIO test driver");
MODULE_LICENSE("GPL");
