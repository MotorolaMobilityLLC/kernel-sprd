// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020 Spreadtrum Communications Inc.

#include <linux/cpumask.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/of.h>
#include <linux/kernel.h>

#define THM_NAME_LENGTH		20
#define PERIOD			5000
#define DEF_THMZONE		"board-thmzone"

struct shell_sensor {
	u16 sensor_id;
	int cur_temp;
	int init_flag;
	size_t nsensor;
	size_t ntemp;
	int index;
	int const_temp;
	int **coeff;
	int **hty_temp;
	struct sprd_thermal_zone *pzone;
	const char **sensor_names;
	struct thermal_zone_device **thm_zones;
	struct delayed_work read_temp_work;
};

struct sprd_thermal_zone {
	struct thermal_zone_device *therm_dev;
	struct mutex th_lock;
	struct device *dev;
	const struct thermal_zone_of_device_ops *ops;
	char name[THM_NAME_LENGTH];
	int id;
};

static int __sprd_get_temp(struct shell_sensor *psensor, int *temp)
{
	int i = 0, j = 0, ret = 0;
	int sum_temp = 0;
	int coeff_index = 0;
	struct thermal_zone_device *tz = NULL;
	int index = psensor->index;

	for (; i < psensor->nsensor; i++) {
		tz = psensor->thm_zones[i];
		if (!tz || IS_ERR(tz) || !tz->ops->get_temp) {
			pr_err("get thermal zone failed %d\n", i);
			return -ENODEV;
		}
		ret = tz->ops->get_temp(tz, &(psensor->hty_temp[i][index]));
		if (ret) {
			pr_err("get thermal %s temp failed\n", tz->type);
			return ret;
		}
	}
	if (psensor->init_flag) {
		tz = thermal_zone_get_zone_by_name(DEF_THMZONE);
		ret = tz->ops->get_temp(tz, temp);
		if (ret) {
			pr_err("get %s temp fail\n", tz->type);
			return ret;
		}
		goto out;
	}

	for (i = 0; i < psensor->nsensor; i++) {
		for (j = 0; j < psensor->ntemp; j++) {
			if (index + 1 + j < psensor->ntemp)
				coeff_index = index + 1 + j;
			else
				coeff_index = index + 1 + j - psensor->ntemp;
			sum_temp += psensor->coeff[i][j] * psensor->hty_temp[i][coeff_index];
		}
	}
	sum_temp = sum_temp/10000 + psensor->const_temp;
	*temp = sum_temp;
out:
	psensor->index++;
	if (index == psensor->ntemp - 1) {
		psensor->index = 0;
		psensor->init_flag = 0;
	}
	return 0;
}

static int sprd_temp_sensor_read(void *devdata, int *temp)
{
	int ret = -EINVAL;
	struct sprd_thermal_zone *pzone = devdata;
	struct shell_sensor *psensor = NULL;

	psensor = (struct shell_sensor *)dev_get_drvdata(pzone->dev);

	if (!psensor || !pzone || !temp)
		return ret;
	*temp = psensor->cur_temp;
	pr_debug("shell_sensor_id:%d, temp:%d\n", pzone->id, *temp);

	return 0;
}

static void sensor_read_temp_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct shell_sensor *psensor = container_of(dwork, struct shell_sensor, read_temp_work);
	int ret;

	ret = __sprd_get_temp(psensor, &psensor->cur_temp);
	if (ret)
		pr_debug("shell_sensor: %s; temp: %d", psensor->pzone->name,
			 psensor->cur_temp);
	schedule_delayed_work(&psensor->read_temp_work, msecs_to_jiffies(PERIOD));
}

const struct thermal_zone_of_device_ops sprd_shell_thm_ops = {
	.get_temp = sprd_temp_sensor_read,
};


static int sprd_temp_sen_parse_dt(struct device *dev, struct shell_sensor *psensor)
{
	int i, j, ret, offset = 0;
	int k = 0;
	size_t count;
	u32 temp_coeff[80];
	int **tmp_coeff;
	int **tmp_hty_temp;
	struct device_node *np = dev->of_node;

	if (!np) {
		dev_err(dev, "device node not found\n");
		return -EINVAL;
	}

	count = (size_t)of_property_count_strings(np, "sensor-names");
	if (count < 0) {
		dev_err(dev, "sensor names not found\n");
		return count;
	}

	psensor->thm_zones = devm_kmalloc_array(dev, count, sizeof(struct thermal_zone_device *),
						GFP_KERNEL);
	psensor->sensor_names = devm_kmalloc_array(dev, count, sizeof(char *), GFP_KERNEL);
	psensor->nsensor = count;
	for (i = 0; i < count; i++) {
		ret = of_property_read_string_index(np, "sensor-names", i,
						  &psensor->sensor_names[i]);
		if (ret) {
			dev_err(dev, "fail to get  sensor-names\n");
			return ret;
		}
	}

	count = (size_t)of_property_count_elems_of_size(np, "temp-coeff", sizeof(u32));
	if (count < 0) {
		dev_err(dev, "temp coeff not found\n");
		return count;
	}

	ret = of_property_read_u32_array(np, "temp-coeff", temp_coeff, count);
	if (ret) {
		dev_err(dev, "fail to get temp-coeff\n");
		return -EINVAL;
	}

	psensor->ntemp = count/psensor->nsensor;

	tmp_coeff = devm_kmalloc_array(dev, psensor->nsensor, sizeof(int *), GFP_KERNEL);
	tmp_hty_temp = devm_kmalloc_array(dev, psensor->nsensor, sizeof(int *), GFP_KERNEL);
	while (k < psensor->nsensor) {
		tmp_coeff[k] = devm_kmalloc_array(dev, psensor->ntemp, sizeof(int), GFP_KERNEL);
		tmp_hty_temp[k] = devm_kmalloc_array(dev, psensor->ntemp, sizeof(int), GFP_KERNEL);
		k++;
	}
	psensor->coeff = tmp_coeff;
	psensor->hty_temp = tmp_hty_temp;
	ret = of_property_read_u32(np, "coeff-offset", &offset);
	dev_info(dev, "coeff-offset: %d, ntemp=%ld\n", offset, psensor->ntemp);
	if (ret) {
		dev_err(dev, "fail to get coeff-offset\n");
		return -EINVAL;
	}
	for (i = 0; i < psensor->nsensor; i++) {
		for (j = 0; j < psensor->ntemp; j++)
			psensor->coeff[i][j] = (int)temp_coeff[i*psensor->ntemp+j] - offset;
	}
	ret = of_property_read_u32(np, "temp-const", &psensor->const_temp);
	if (ret) {
		dev_err(dev, "fail to get temp-const\n");
		return -EINVAL;
	}
	ret = of_property_read_u32(np, "const-offset", &offset);
	if (ret) {
		dev_err(dev, "fail to get const-offset\n");
		return -EINVAL;
	}
	psensor->const_temp -= offset;

	return ret;
}

static int sprd_shell_thm_resume(struct platform_device *pdev)
{
	struct shell_sensor *psensor = platform_get_drvdata(pdev);

	psensor->index = 0;
	psensor->init_flag = 1;
	queue_delayed_work(system_power_efficient_wq, &psensor->read_temp_work,
			   msecs_to_jiffies(PERIOD));

	return 0;
}

static int sprd_shell_thm_suspend(struct platform_device *pdev, pm_message_t
				    state)
{
	struct shell_sensor *psensor = platform_get_drvdata(pdev);

	cancel_delayed_work(&psensor->read_temp_work);
	return 0;
}

int sprd_thm_init(struct sprd_thermal_zone *pzone)
{
	pzone->therm_dev = thermal_zone_of_sensor_register(pzone->dev, pzone->id,
							   pzone, &sprd_shell_thm_ops);

	if (IS_ERR_OR_NULL(pzone->therm_dev)) {
		pr_err("Register thermal zone device failed.\n");
		return PTR_ERR(pzone->therm_dev);
	};
	thermal_zone_device_update(pzone->therm_dev, THERMAL_EVENT_UNSPECIFIED);
	return 0;
}

static int sprd_shell_thm_probe(struct platform_device *pdev)
{
	int i = 0;
	int ret = 0, sensor_id = 0;
	struct sprd_thermal_zone *pzone = NULL;
	struct shell_sensor *psensor = NULL;
	struct device_node *np = pdev->dev.of_node;

	if (!np) {
		dev_err(&pdev->dev, "device node not found\n");
		return -EINVAL;
	}

	psensor = devm_kzalloc(&pdev->dev, sizeof(*psensor), GFP_KERNEL);
	if (!psensor)
		return -ENOMEM;

	psensor->index = 0;
	psensor->init_flag = 1;
	psensor->cur_temp = 0;
	ret = sprd_temp_sen_parse_dt(&pdev->dev, psensor);
	if (ret) {
		dev_err(&pdev->dev, "not found ptrips\n");
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&psensor->read_temp_work, sensor_read_temp_work);
	for (i = 0; i < psensor->nsensor; i++) {
		psensor->thm_zones[i] = thermal_zone_get_zone_by_name(psensor->sensor_names[i]);
		if (IS_ERR(psensor->thm_zones[i])) {
			pr_err("get thermal zone %s failed\n", psensor->sensor_names[i]);
			return -EPROBE_DEFER;
		}
	}

	pzone = devm_kzalloc(&pdev->dev, sizeof(*pzone), GFP_KERNEL);
	if (!pzone)
		return -ENOMEM;

	mutex_init(&pzone->th_lock);

	pzone->dev = &pdev->dev;
	pzone->id = sensor_id;
	pzone->ops = &sprd_shell_thm_ops;
	strscpy(pzone->name, np->name, sizeof(pzone->name));

	ret = sprd_thm_init(pzone);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"virtual sensor sw init error id =%d\n", pzone->id);
		return ret;
	}

	psensor->pzone = pzone;
	platform_set_drvdata(pdev, psensor);

	schedule_delayed_work(&psensor->read_temp_work, msecs_to_jiffies(PERIOD));
	dev_info(&pdev->dev, "sprd_shell_thermal probe success\n");
	return 0;
}

static int sprd_shell_thm_remove(struct platform_device *pdev)
{
	struct shell_sensor *psensor = platform_get_drvdata(pdev);
	struct sprd_thermal_zone *pzone = psensor->pzone;

	thermal_zone_device_unregister(pzone->therm_dev);
	kfree(psensor->sensor_names);
	kfree(psensor->thm_zones);
	kfree(psensor->coeff);
	kfree(psensor->hty_temp);
	mutex_destroy(&pzone->th_lock);
	return 0;
}

static const struct of_device_id shell_thermal_of_match[] = {
	{ .compatible = "sprd,shell-thermal" },
	{},
};
MODULE_DEVICE_TABLE(of, shell_thermal_of_match);

static struct platform_driver sprd_shell_thermal_driver = {
	.probe = sprd_shell_thm_probe,
	.suspend = sprd_shell_thm_suspend,
	.resume = sprd_shell_thm_resume,
	.remove = sprd_shell_thm_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "shell-thermal",
		   .of_match_table = of_match_ptr(shell_thermal_of_match),
		   },
};

static int __init sprd_shell_thermal_init(void)
{
	return platform_driver_register(&sprd_shell_thermal_driver);
}

static void __exit sprd_shell_thermal_exit(void)
{
	platform_driver_unregister(&sprd_shell_thermal_driver);
}

device_initcall_sync(sprd_shell_thermal_init);
module_exit(sprd_shell_thermal_exit);

MODULE_DESCRIPTION("sprd thermal driver");
MODULE_LICENSE("GPL");
