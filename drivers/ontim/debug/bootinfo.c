#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <linux/string.h>
#include <video/mmp_disp.h>
#include <linux/delay.h>


static struct kobject *bootinfo_kobj = NULL;
int gesture_dubbleclick_en =0;

static char front_cam_name[64] = "Unknown";
static char frontaux_cam_name[64] = "Unknown";
static char back_cam_name[64] = "Unknown";
static char backaux_cam_name[64] = "Unknown";
static char backaux2_cam_name[64] = "Unknown";

static char front_cam_efuse[64] = "Unknown";
static char frontaux_cam_efuse[64] = "Unknown";
static char back_cam_efuse[64] = "Unknown";
static char backaux_cam_efuse[64] = "Unknown";
static char backaux2_cam_efuse[64] = "Unknown";

static ssize_t gesture_enable_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	return (s - buf);
}

static ssize_t gesture_enable_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)

{
    int enable=0,ret;
    ret = kstrtouint(buf, 10, &enable);
	if(ret != 0)
		return 0;
    gesture_dubbleclick_en =enable;

	return n;
}
static struct kobj_attribute gesture_enable_attr = {
	.attr = {
		.name = "gesture_enable",
		.mode = 0644,
	},
	.show =&gesture_enable_show,
	.store= &gesture_enable_store,
};


static ssize_t i2c_devices_info_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	return (s - buf);
}
static struct kobj_attribute i2c_devices_info_attr = {
	.attr = {
		.name = "i2c_devices_probe_info",
		.mode = 0444,
	},
	.show =&i2c_devices_info_show,
};

static ssize_t sprd_get_back_sensor_name_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{

	pr_info("sprd_sensor: _sensor_type_info %s\n", back_cam_name);
	return scnprintf(buf, PAGE_SIZE, "%s\n",  back_cam_name);
}
static ssize_t sprd_get_backaux_sensor_name_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{

	pr_info("sprd_sensor: _sensor_type_info %s\n", backaux_cam_name);
	return scnprintf(buf, PAGE_SIZE, "%s\n",  backaux_cam_name);
}
static ssize_t sprd_get_backaux2_sensor_name_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{

	pr_info("sprd_sensor: _sensor_type_info %s\n", backaux2_cam_name);
	return scnprintf(buf, PAGE_SIZE, "%s\n",  backaux2_cam_name);
}
static ssize_t sprd_get_front_sensor_name_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{

	pr_info("sprd_sensor: _sensor_type_info %s\n", front_cam_name);
	return scnprintf(buf, PAGE_SIZE, "%s\n",  front_cam_name);
}
static ssize_t sprd_get_frontaux_sensor_name_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{

	pr_info("sprd_sensor: _sensor_type_info %s\n", frontaux_cam_name);
	return scnprintf(buf, PAGE_SIZE, "%s\n",  frontaux_cam_name);
}

static ssize_t sprd_set_back_sensor_name_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t size)
{
	if (strlen(buf) >= 255) {
		pr_err("out of the maxnum 255.\n");
		return -EINVAL;
	}

	pr_err("sensor name %s.\n", buf);
	memset(back_cam_name, 0, sizeof(back_cam_name));
	memcpy(back_cam_name, buf, strlen(buf));

	return size;

}
static ssize_t sprd_set_backaux_sensor_name_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t size)
{
	if (strlen(buf) >= 255) {
		pr_err("out of the maxnum 255.\n");
		return -EINVAL;
	}

	pr_err("sensor name %s.\n", buf);
	memset(backaux_cam_name, 0, sizeof(backaux_cam_name));
	memcpy(backaux_cam_name, buf, strlen(buf));

	return size;
}
static ssize_t sprd_set_backaux2_sensor_name_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t size)
{
	if (strlen(buf) >= 255) {
		pr_err("out of the maxnum 255.\n");
		return -EINVAL;
	}

	pr_err("sensor name %s.\n", buf);
	memset(backaux2_cam_name, 0, sizeof(backaux2_cam_name));
	memcpy(backaux2_cam_name, buf, strlen(buf));

	return size;
}

static ssize_t sprd_set_front_sensor_name_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t size)
{
	if (strlen(buf) >= 255) {
		pr_err("out of the maxnum 255.\n");
		return -EINVAL;
	}

	pr_err("sensor name %s.\n", buf);
	memset(front_cam_name, 0, sizeof(front_cam_name));
	memcpy(front_cam_name, buf, strlen(buf));

	return size;
}

static ssize_t sprd_set_frontaux_sensor_name_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t size)
{
	if (strlen(buf) >= 255) {
		pr_err("out of the maxnum 255.\n");
		return -EINVAL;
	}

	pr_err("sensor name %s.\n", buf);
	memset(frontaux_cam_name, 0, sizeof(frontaux_cam_name));
	memcpy(frontaux_cam_name, buf, strlen(buf));

	return size;
}

//camera efuse id
static ssize_t sprd_get_back_sensor_efuse_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{

	pr_info("sprd_sensor: _sensor_type_info %s\n", back_cam_efuse);
	return scnprintf(buf, PAGE_SIZE, "%s\n",  back_cam_efuse);
}
static ssize_t sprd_get_backaux_sensor_efuse_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{

	pr_info("sprd_sensor: _sensor_type_info %s\n", backaux_cam_efuse);
	return scnprintf(buf, PAGE_SIZE, "%s\n",  backaux_cam_efuse);
}
static ssize_t sprd_get_backaux2_sensor_efuse_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{

	pr_info("sprd_sensor: _sensor_type_info %s\n", backaux2_cam_efuse);
	return scnprintf(buf, PAGE_SIZE, "%s\n",  backaux2_cam_efuse);
}
static ssize_t sprd_get_front_sensor_efuse_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{

	pr_info("sprd_sensor: _sensor_type_info %s\n", front_cam_efuse);
	return scnprintf(buf, PAGE_SIZE, "%s\n",  front_cam_efuse);
}
static ssize_t sprd_get_frontaux_sensor_efuse_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{

	pr_info("sprd_sensor: _sensor_type_info %s\n", frontaux_cam_efuse);
	return scnprintf(buf, PAGE_SIZE, "%s\n",  frontaux_cam_efuse);
}

static ssize_t sprd_set_back_sensor_efuse_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t size)
{
	if (strlen(buf) >= 255) {
		pr_err("out of the maxnum 255.\n");
		return -EINVAL;
	}

	pr_err("sensor efuse %s.\n", buf);
	memset(back_cam_efuse, 0, sizeof(back_cam_efuse));
	memcpy(back_cam_efuse, buf, strlen(buf));

	return size;

}
static ssize_t sprd_set_backaux_sensor_efuse_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t size)
{
	if (strlen(buf) >= 255) {
		pr_err("out of the maxnum 255.\n");
		return -EINVAL;
	}

	pr_err("sensor efuse %s.\n", buf);
	memset(backaux_cam_efuse, 0, sizeof(backaux_cam_efuse));
	memcpy(backaux_cam_efuse, buf, strlen(buf));

	return size;
}
static ssize_t sprd_set_backaux2_sensor_efuse_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t size)
{
	if (strlen(buf) >= 255) {
		pr_err("out of the maxnum 255.\n");
		return -EINVAL;
	}

	pr_err("sensor efuse %s.\n", buf);
	memset(backaux2_cam_efuse, 0, sizeof(backaux2_cam_efuse));
	memcpy(backaux2_cam_efuse, buf, strlen(buf));

	return size;
}

static ssize_t sprd_set_front_sensor_efuse_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t size)
{
	if (strlen(buf) >= 255) {
		pr_err("out of the maxnum 255.\n");
		return -EINVAL;
	}

	pr_err("sensor efuse %s.\n", buf);
	memset(front_cam_efuse, 0, sizeof(front_cam_efuse));
	memcpy(front_cam_efuse, buf, strlen(buf));

	return size;
}

static ssize_t sprd_set_frontaux_sensor_efuse_hwinfo(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t size)
{
	if (strlen(buf) >= 255) {
		pr_err("out of the maxnum 255.\n");
		return -EINVAL;
	}

	pr_err("sensor efuse %s.\n", buf);
	memset(frontaux_cam_efuse, 0, sizeof(frontaux_cam_efuse));
	memcpy(frontaux_cam_efuse, buf, strlen(buf));

	return size;
}

static struct kobj_attribute back_cam_info_attr = {
	.attr = {
		.name = "back_cam_info",
		.mode = 0444,
	},
	.show =&sprd_get_back_sensor_name_hwinfo,
	.store = &sprd_set_back_sensor_name_hwinfo,
};
static struct kobj_attribute backaux_cam_info_attr = {
	.attr = {
		.name = "backaux_cam_info",
		.mode = 0444,
	},
	.show =&sprd_get_backaux_sensor_name_hwinfo,
	.store = &sprd_set_backaux_sensor_name_hwinfo,
};
static struct kobj_attribute backaux2_cam_info_attr = {
	.attr = {
		.name = "backaux2_cam_info",
		.mode = 0444,
	},
	.show =&sprd_get_backaux2_sensor_name_hwinfo,
	.store = &sprd_set_backaux2_sensor_name_hwinfo,
};
static struct kobj_attribute front_cam_info_attr = {
	.attr = {
		.name = "front_cam_info",
		.mode = 0444,
	},
	.show =&sprd_get_front_sensor_name_hwinfo,
	.store = &sprd_set_front_sensor_name_hwinfo,
};
static struct kobj_attribute frontaux_cam_info_attr = {
	.attr = {
		.name = "frontaux_cam_info",
		.mode = 0444,
	},
	.show =&sprd_get_frontaux_sensor_name_hwinfo,
	.store = &sprd_set_frontaux_sensor_name_hwinfo,
};

//cam efuse id
static struct kobj_attribute back_cam_efuse_attr = {
	.attr = {
		.name = "back_cam_efuse",
		.mode = 0444,
	},
	.show =&sprd_get_back_sensor_efuse_hwinfo,
	.store = &sprd_set_back_sensor_efuse_hwinfo,
};
static struct kobj_attribute backaux_cam_efuse_attr = {
	.attr = {
		.name = "backaux_cam_efuse",
		.mode = 0444,
	},
	.show =&sprd_get_backaux_sensor_efuse_hwinfo,
	.store = &sprd_set_backaux_sensor_efuse_hwinfo,
};
static struct kobj_attribute backaux2_cam_efuse_attr = {
	.attr = {
		.name = "backaux2_cam_efuse",
		.mode = 0444,
	},
	.show =&sprd_get_backaux2_sensor_efuse_hwinfo,
	.store = &sprd_set_backaux2_sensor_efuse_hwinfo,
};
static struct kobj_attribute front_cam_efuse_attr = {
	.attr = {
		.name = "front_cam_efuse",
		.mode = 0444,
	},
	.show =&sprd_get_front_sensor_efuse_hwinfo,
	.store = &sprd_set_front_sensor_efuse_hwinfo,
};
static struct kobj_attribute frontaux_cam_efuse_attr = {
	.attr = {
		.name = "frontaux_cam_efuse",
		.mode = 0444,
	},
	.show =&sprd_get_frontaux_sensor_efuse_hwinfo,
	.store = &sprd_set_frontaux_sensor_efuse_hwinfo,
};

static struct attribute * g[] = {
	&i2c_devices_info_attr.attr,//+add by liuwei
	&gesture_enable_attr.attr,
	&back_cam_info_attr.attr,
	&backaux_cam_info_attr.attr,
	&backaux2_cam_info_attr.attr,
	&front_cam_info_attr.attr,
	&frontaux_cam_info_attr.attr,
	&back_cam_efuse_attr.attr,
	&backaux_cam_efuse_attr.attr,
	&backaux2_cam_efuse_attr.attr,
	&front_cam_efuse_attr.attr,
	&frontaux_cam_efuse_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

static int __init bootinfo_init(void)
{
	int ret = -ENOMEM;

	//printk("%s,line=%d\n",__func__,__LINE__);

	bootinfo_kobj = kobject_create_and_add("ontim_bootinfo", NULL);

	if (bootinfo_kobj == NULL) {
		printk("bootinfo_init: kobject_create_and_add failed\n");
		goto fail;
	}

	ret = sysfs_create_group(bootinfo_kobj, &attr_group);
	if (ret) {
		printk("bootinfo_init: sysfs_create_group failed\n");
		goto sys_fail;
	}

	return ret;
sys_fail:
	kobject_del(bootinfo_kobj);
fail:
	return ret;

}

static void __exit bootinfo_exit(void)
{

	if (bootinfo_kobj) {
		sysfs_remove_group(bootinfo_kobj, &attr_group);
		kobject_del(bootinfo_kobj);
	}
}

arch_initcall(bootinfo_init);
module_exit(bootinfo_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Boot information collector");
