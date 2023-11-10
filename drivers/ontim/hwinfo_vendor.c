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
#define pr_fmt(fmt) "hwinfo_vendor: " fmt

static char* version_of_hwinfo_vendor = VERSION_OF;
module_param(version_of_hwinfo_vendor, charp, 0444);

static int __init hwinfo_vendor_init(void)
{
	pr_err("hwinfo_vendor_init\n");
	pr_err("version_of_hwinfo_vendor=%s\n", version_of_hwinfo_vendor);
	pr_err("version_of_lk=%s\n", hwinfo_get_prop("version_of_lk"));

	return 0;
}

static void __exit hwinfo_vendor_exit(void)
{
	pr_err("hwinfo_vendor_exit\n");
	return ;
}

fs_initcall_sync(hwinfo_vendor_init);
module_exit(hwinfo_vendor_exit);

MODULE_AUTHOR("bsp@ontim");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Product Hardward Info in vendor");
