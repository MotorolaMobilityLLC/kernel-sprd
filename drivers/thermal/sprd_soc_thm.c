// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020 Unisoc Inc.

#include <linux/cpu_cooling.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/of.h>
#include <linux/kernel.h>

struct sprd_soc_thm {
	struct thermal_zone_device *thm_dev;
	struct thm_handle_ops *ops;
	int id;
};

struct sprd_tz_list {
	int temp;
	struct thermal_zone_device *tz_dev;
};

struct sprd_thm_data {
	int nr_thm;
	struct sprd_tz_list *tz_list;
	struct sprd_soc_thm *soc_thm;
};

struct thm_handle_ops {
	int (*read_temp)(struct sprd_thm_data *, int *);
};

static int sprd_sys_temp_read(void *devdata, int *temp)
{
	struct sprd_thm_data *thm_data = devdata;
	struct sprd_soc_thm *soc_thm = thm_data->soc_thm;
	int ret = 0;

	if (!soc_thm->ops->read_temp)
		return -EINVAL;

	ret = soc_thm->ops->read_temp(thm_data, temp);

	return ret;
}

static void sprd_unregister_soc_thermal(struct platform_device *pdev)
{
	struct sprd_thm_data *data = platform_get_drvdata(pdev);
	struct sprd_soc_thm *soc_thm = data->soc_thm;

	thermal_zone_device_unregister(soc_thm->thm_dev);
}

static const struct thermal_zone_of_device_ops sprd_of_thermal_ops = {
	.get_temp = sprd_sys_temp_read,
};

int sprd_register_soc_thermal(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sprd_thm_data *data = platform_get_drvdata(pdev);
	struct sprd_soc_thm *soc_thm = data->soc_thm;

	soc_thm->thm_dev = thermal_zone_of_sensor_register(dev,
							   soc_thm->id, data,
							   &sprd_of_thermal_ops
							   );
	if (IS_ERR_OR_NULL(soc_thm->thm_dev))
		return PTR_ERR(soc_thm->thm_dev);

	thermal_zone_device_update(soc_thm->thm_dev, THERMAL_EVENT_UNSPECIFIED);

	return 0;
}

static int sprd_read_soc_thm_temp(struct sprd_thm_data *thm_data, int *temp)
{
	int i, ret = 0;
	int soc_temp = 0;
	int sum_temp = 0;
	struct thermal_zone_device *tz = NULL;
	struct sprd_tz_list *tz_list;

	if (!thm_data || !temp)
		return -EINVAL;

	for (i = 0; i < thm_data->nr_thm; i++) {
		tz_list = &thm_data->tz_list[i];
		tz = tz_list->tz_dev;
		if (!tz || IS_ERR(tz) || !tz->ops->get_temp)
			return -EINVAL;
		ret = tz->ops->get_temp(tz, &tz_list->temp);
		if (ret)
			return -EINVAL;

		soc_temp = max(soc_temp, tz_list->temp);
		sum_temp += tz_list->temp;
	}

	if (soc_temp >= 30000)
		*temp = soc_temp;
	else
		*temp = sum_temp / thm_data->nr_thm;

	return 0;
}

struct thm_handle_ops sprd_soc_thm_ops = {
	.read_temp = sprd_read_soc_thm_temp,
};

static int sprd_get_thm_zone_counts(struct device *dev)
{
	int count;
	struct device_node *np = dev->of_node;

	if (!np) {
		dev_err(dev, "device node not found\n");
		return -EINVAL;
	}

	count = of_property_count_strings(np, "thm-zone-names");
	if (count < 0) {
		dev_err(dev, "thm-zone-names not found\n");
		return -EINVAL;
	}

	return count;
}

static int sprd_get_thm_zone_device(struct platform_device *pdev)
{
	int i, ret = 0;
	const char *name;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct sprd_tz_list *tz_list;
	struct sprd_thm_data *data = platform_get_drvdata(pdev);

	for (i = 0; i < data->nr_thm; i++) {
		ret = of_property_read_string_index(np, "sensor-names",
						    i, &name);
		if (ret) {
			dev_err(dev, "fail to get thm names\n");
			return ret;
		}
		tz_list = &data->tz_list[i];
		tz_list->tz_dev = thermal_zone_get_zone_by_name(name);
		if (IS_ERR(tz_list->tz_dev)) {
			dev_err(dev, "failed to get thermal zone by name\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int sprd_soc_thm_probe(struct platform_device *pdev)
{
	int count = 0, ret = 0, id;
	struct sprd_thm_data *data;
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;

	if (!np) {
		dev_err(dev, "device node not found\n");
		return -EINVAL;
	}

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	count = sprd_get_thm_zone_counts(dev);
	if (count < 0) {
		dev_err(dev, "failed to get thm-zone count\n");
		return -EINVAL;
	}
	data->nr_thm = count;
	data->tz_list = devm_kzalloc(dev, sizeof(*data->tz_list) * data->nr_thm,
				     GFP_KERNEL);
	if (!data->tz_list)
		return -ENOMEM;

	data->soc_thm = devm_kzalloc(dev, sizeof(*data->soc_thm), GFP_KERNEL);
	if (!data->soc_thm)
		return -ENOMEM;

	platform_set_drvdata(pdev, data);
	ret = sprd_get_thm_zone_device(pdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to get thm zone device\n");
		return -EINVAL;
	}
	id = of_alias_get_id(np, "thm-sensor");
	if (id < 0) {
		dev_err(dev, "fail to get id\n");
		return -ENODEV;
	}
	data->soc_thm->id = id;
	data->soc_thm->ops = &sprd_soc_thm_ops;
	ret = sprd_register_soc_thermal(pdev);
	if (ret < 0) {
		dev_err(dev, "failed to register soc thermal\n");
		return -EINVAL;
	}

	return 0;
}

static int sprd_soc_thm_remove(struct platform_device *pdev)
{
	sprd_unregister_soc_thermal(pdev);

	return 0;
}

static const struct of_device_id soc_thermal_of_match[] = {
	{ .compatible = "sprd,soc-thermal" },
	{},
};
MODULE_DEVICE_TABLE(of, soc_thermal_of_match);

static struct platform_driver sprd_soc_thermal_driver = {
	.probe = sprd_soc_thm_probe,
	.remove = sprd_soc_thm_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "sprd_soc_thermal",
		   .of_match_table = of_match_ptr(soc_thermal_of_match),
		   },
};

static int __init sprd_soc_thermal_init(void)
{
	return platform_driver_register(&sprd_soc_thermal_driver);
}

static void __exit sprd_soc_thermal_exit(void)
{
	platform_driver_unregister(&sprd_soc_thermal_driver);
}

device_initcall_sync(sprd_soc_thermal_init);
module_exit(sprd_soc_thermal_exit);

MODULE_DESCRIPTION("sprd thermal driver");
MODULE_LICENSE("GPL v2");
