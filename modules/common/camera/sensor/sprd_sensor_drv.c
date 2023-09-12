/*
 * Copyright (C) 2021-2022 UNISOC Communications Inc.
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

#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/sched.h>

#include "sprd_mm.h"
#include "csi_api.h"
#include "sprd_sensor_core.h"
#include "sprd_sensor_drv.h"
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "SENSOR_DRV: %d: %s L %d: " fmt, \
	current->pid, __func__, __LINE__

static const char *const sprd_sensor_supply_names[] = {
	"vddio",
	"vddcama",
	"vddcamd",
	"vddcammot",
};

static const char *const sprd_sensor_gpio_names[] = {
	"reset-gpios",
	"power-down-gpios",
	"flash-gpios",
	"mipi-switch-en-gpios",
	"mipi-switch-mode-gpios",
	"switch-mode-gpios",
	"cam-id-gpios",
	"iovdd-gpios",
	"avdd-gpios",
	"dvdd-gpios",
	"mot-gpios",
};

static const struct sensor_mclk_tag c_sensor_mclk_tab[] = {
	{96, "clk_96m"},
#ifdef MCLK_NEW_PROCESS1
	{64, "clk_64m"},
	{51, "clk_51m2"},
#else
	{77, "clk_76m8"},
#endif
	{48, "clk_48m"},
	{26, "clk_26m"},
};

#define I2C_1M_FLAG_CAM         0X0080
#define I2C_400K_FLAG_CAM             0X0040

static int sprd_sensor_parse_clk_dt(struct device *dev,
				struct sprd_sensor_dev_info_tag
				*sensor_info)
{
	sensor_info->sensor_clk = of_clk_get_by_name(dev->of_node, "clk_src");
	pr_info("sensor_info->sensor_clk: %p id %d\n",
		sensor_info->sensor_clk, sensor_info->sensor_id);
	if (IS_ERR_OR_NULL(sensor_info->sensor_clk)) {
		pr_err("sensor clk config err\n");
		return -EINVAL;
	}

	sensor_info->sensor_clk_default =
		clk_get_parent(sensor_info->sensor_clk);
	if (IS_ERR_OR_NULL(sensor_info->sensor_clk_default))
		return -EINVAL;

	sensor_info->sensor_clk_default_rate =
		clk_get_rate(sensor_info->sensor_clk_default);

	sensor_info->sensor_eb =
		of_clk_get_by_name(dev->of_node, "sensor_eb");
	sensor_info->mclk_count = 0;
	sensor_info->mclk_freq = 0;
	if (IS_ERR_OR_NULL(sensor_info->sensor_eb)) {
		pr_err("sensor eb clk config err\n");
		return -EINVAL;
	}

	return 0;
}

static int sprd_sensor_parse_gpio_dt(struct device *dev,
				struct sprd_sensor_dev_info_tag
				*sensor_info)
{
	int i;
	int ret = 0;

	for (i = 0; i < SPRD_SENSOR_GPIO_TAG_MAX; i++) {
		sensor_info->gpio_tab[i] = of_get_named_gpio(dev->of_node,
						sprd_sensor_gpio_names[i], 0);
		if (gpio_is_valid(sensor_info->gpio_tab[i])) {
			ret = devm_gpio_request(dev, sensor_info->gpio_tab[i],
						sprd_sensor_gpio_names[i]);
		} else {
			pr_debug("invalid gpio: i=%d, gpio=%d, name=%s\n",
			       i, sensor_info->gpio_tab[i],
			       sprd_sensor_gpio_names[i]);
		}
	}

	return 0;
}

static int sprd_sensor_free_gpio(struct device *dev,
				struct sprd_sensor_dev_info_tag
				*sensor_info)
{
	int i;
	int ret = 0;

	for (i = 0; i < SPRD_SENSOR_GPIO_TAG_MAX; i++) {
		sensor_info->gpio_tab[i] = of_get_named_gpio(dev->of_node,
						sprd_sensor_gpio_names[i], 0);
		if (gpio_is_valid(sensor_info->gpio_tab[i]))
			devm_gpio_free(dev, sensor_info->gpio_tab[i]);

	}

	return ret;
}

#ifndef MCLK_NEW_PROCESS1

static uint32_t parse_dcam_id(struct device_node *dn,
			      struct sprd_sensor_dev_info_tag *sensor_info)
{
	int dcam_id = -1;

	sensor_info->attch_dcam_id = -1;
	if (of_property_read_u32(dn, "sprd,dcam-id", &dcam_id)) {
		pr_err("Get dcam id error\n");
		return -1;
	}
	pr_info("dcam id is %d\n", dcam_id);
	sensor_info->attch_dcam_id = dcam_id;
	return 0;
}
#endif
static int sprd_sensor_parse_dt(struct device *dev,
				struct sprd_sensor_dev_info_tag *sensor_info)
{

	if (sprd_sensor_parse_clk_dt(dev, sensor_info)) {
		pr_err("%s :clock parsing error\n", __func__);
		return -EINVAL;
	}

	if (sprd_sensor_parse_gpio_dt(dev, sensor_info)) {
		pr_err("%s :gpio parsing error\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static struct device_node *get_csi_port_node(struct device_node *dn)
{
	struct device_node *csi_node = NULL;

	csi_node = of_parse_phandle(dn, "csi", 0);
	if (!csi_node)
		pr_err("%s :error getting csi_node\n", __func__);

	return csi_node;
}

static int sprd_sensor_config(struct device *dev,
				struct sprd_sensor_dev_info_tag *sensor_info)
{
	struct device_node *csi_ep_node = NULL;
	unsigned int phy_id = 0;
	int ret = 0;

	ret = sprd_sensor_parse_dt(dev, sensor_info);
	if (ret) {
		pr_err("%s :parse dt error\n", __func__);
		goto exit;
	}

	sensor_info->dev_node = dev->of_node;

	phy_id = csi_api_mipi_phy_cfg_init(sensor_info->dev_node,
					   sensor_info->sensor_id);
	if (phy_id < 0U){
		pr_err("sprd,phyid is invalid\n");
		return -1;
	}

	csi_ep_node = get_csi_port_node(sensor_info->dev_node);
	if (csi_ep_node) {
		pr_info("%lu\n", (unsigned long)csi_ep_node);
		csi_api_dt_node_init(dev, csi_ep_node, sensor_info->sensor_id,
				     phy_id);
	} else
		pr_err("%s; csi sensor connection error\n", __func__);
#ifdef MCLK_NEW_PROCESS1
	sensor_info->attch_dcam_id = csi_api_get_dcam_id(sensor_info->dev_node,
					   sensor_info->sensor_id, phy_id);
#else
	if (parse_dcam_id(csi_ep_node, sensor_info)) {
		pr_err("%s :dcam id parsing error\n", __func__);
		return -EINVAL;
	}
#endif
exit:
	return ret;
}

static int sprd_sensor_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	int i, ret = 0;
	struct sprd_sensor_dev_info_tag *pdata = NULL;

	pr_info("dcam sensor probe start:device name:%s\n", id->name);

	if (!dev->of_node) {
		pr_err("no device node %s", __func__);
		return -ENODEV;
	}
	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	client->dev.platform_data = (void *)pdata;
	pdata->i2c_info = client;

	for (i = 0; i < SENSOR_REGULATOR_ID_MAX; i++)
		pdata->regulator_supply[i] = NULL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
#endif

	mutex_init(&pdata->sync_lock);
	mutex_init(&pdata->set_voltage_lock);
	atomic_set(&pdata->users, 0);
	pdata->sensor_id = SPRD_SENSOR_ID_MAX;
	if (of_device_is_compatible(dev->of_node, "sprd,sensor-main")) {
		pdata->sensor_id = SPRD_SENSOR_MAIN_ID_E;
		ret = sprd_sensor_config(dev, pdata);
		s_sensor_dev_data[SPRD_SENSOR_MAIN_ID_E] = pdata;
	} else if (of_device_is_compatible(dev->of_node, "sprd,sensor-sub")) {
		pdata->sensor_id = SPRD_SENSOR_SUB_ID_E;
		ret = sprd_sensor_config(dev, pdata);
		s_sensor_dev_data[SPRD_SENSOR_SUB_ID_E] = pdata;
	} else if (of_device_is_compatible(dev->of_node, "sprd,sensor-main2")) {
		pdata->sensor_id = SPRD_SENSOR_MAIN2_ID_E;
		ret = sprd_sensor_config(dev, pdata);
		s_sensor_dev_data[SPRD_SENSOR_MAIN2_ID_E] = pdata;
	} else if (of_device_is_compatible(dev->of_node, "sprd,sensor-sub2")) {
		pdata->sensor_id = SPRD_SENSOR_SUB2_ID_E;
		ret = sprd_sensor_config(dev, pdata);
		s_sensor_dev_data[SPRD_SENSOR_SUB2_ID_E] = pdata;
	} else if (of_device_is_compatible(dev->of_node, "sprd,sensor-main3")) {
		pdata->sensor_id = SPRD_SENSOR_MAIN3_ID_E;
		ret = sprd_sensor_config(dev, pdata);
		s_sensor_dev_data[SPRD_SENSOR_MAIN3_ID_E] = pdata;
	} else if (of_device_is_compatible(dev->of_node, "sprd,sensor-sub3")) {
		pdata->sensor_id = SPRD_SENSOR_SUB3_ID_E;
		ret = sprd_sensor_config(dev, pdata);
		s_sensor_dev_data[SPRD_SENSOR_SUB3_ID_E] = pdata;
	}

	return ret;
}

struct sprd_sensor_dev_info_tag *sprd_sensor_get_dev_context(int
								sensor_id)
{
	if (sensor_id >= SPRD_SENSOR_ID_MAX || sensor_id < 0) {
		pr_err("sensor_id %d error!\n", sensor_id);
		return NULL;
	}

	return s_sensor_dev_data[sensor_id];
}

static int sprd_sensor_regulator_enable(struct regulator *p_reg,
					struct sprd_sensor_dev_info_tag *p_dev,
					int type)
{
	int err = 0;

	err = regulator_enable(p_reg);

	if (err != 0)
		pr_err("Error in regulator_enable: err %d", err);
	else
		p_dev->power_on_count[type]++;

	return err;
}

static int sprd_sensor_regulator_disable(struct regulator *p_reg,
					struct sprd_sensor_dev_info_tag *p_dev,
					int type)
{
	int err = 0;

	while (p_dev->power_on_count[type] > 0) {
		err = regulator_disable(p_reg);

		if (err != 0)
			pr_err("Error in regulator_disable: err %d", err);
		else
			p_dev->power_on_count[type]--;
	}

	return err;
}

int sprd_sensor_set_voltage_by_gpio(int sensor_id, unsigned int val, int type)
{
	int ret = -EINVAL;
	struct sprd_sensor_dev_info_tag *p_dev;
	int gpio_id = 0;

	p_dev = sprd_sensor_get_dev_context(sensor_id);
	if (!p_dev) {
		pr_err("%s, error\n", __func__);
		return -EINVAL;
	}
	gpio_id = p_dev->gpio_tab[type];
	if (gpio_is_valid(gpio_id)) {

		ret = gpio_direction_output(gpio_id, val ? 1:0);
		if (ret)
			goto exit;

		gpio_set_value(gpio_id, val ? 1:0);
	}
	pr_debug("sensor %d vdd val %d\n", sensor_id, val);

exit:
	return ret;
}

int sprd_sensor_set_voltage(int sensor_id, unsigned int val, int type)
{
	struct regulator *p_regulator = NULL;
	int ret = 0;
	struct sprd_sensor_dev_info_tag *p_dev = NULL;
	struct device *dev = NULL;
	unsigned int tolerance = 0;

	p_dev = sprd_sensor_get_dev_context(sensor_id);
	if (p_dev == NULL) {
		pr_debug("p_dev %d invalid sensor_id%d\n", type, sensor_id);
		return -EINVAL;
	}
	mutex_lock(&p_dev->set_voltage_lock);
	if (p_dev->regulator_supply[type] == NULL) {
		dev = &p_dev->i2c_info->dev;
		p_dev->regulator_supply[type] = devm_regulator_get(dev,
			sprd_sensor_supply_names[type]);
	}
	p_regulator = p_dev->regulator_supply[type];
	if (p_regulator == NULL) {
		pr_debug("regulator %d invalid sensor_id%d\n", type, sensor_id);
		mutex_unlock(&p_dev->set_voltage_lock);
		return -EINVAL;
	}
	if (val) {
		tolerance = regulator_get_linear_step(p_regulator);
		ret = regulator_set_voltage(p_regulator, val - tolerance/2, val + tolerance/2);
		if (ret) {
			pr_err("regulator %s vol set %d fail ret%d\n",
			       sprd_sensor_supply_names[type], val, ret);
			goto exit;
		}
		ret = sprd_sensor_regulator_enable(p_regulator, p_dev, type);
		if (ret) {
			devm_regulator_put(p_regulator);
			p_dev->regulator_supply[type] = NULL;
			pr_err("regulator %s enable fail ret%d\n",
			       sprd_sensor_supply_names[type], ret);
			goto exit;
		}
	} else {
		if (p_regulator) {
			ret = sprd_sensor_regulator_disable(p_regulator,
				p_dev, type);
			if (ret) {
				pr_err("regulator disable fail ret%d\n", ret);
			} else {
				devm_regulator_put(p_regulator);
				p_dev->regulator_supply[type] = NULL;
			}
		} else {
			pr_err("regulator %s does not exist",
				sprd_sensor_supply_names[type]);
		}
	}

exit:
	mutex_unlock(&p_dev->set_voltage_lock);
	return ret;
}

static int sprd_sensor_remove(struct i2c_client *client)
{
	struct sprd_sensor_dev_info_tag *pdata = NULL;
	int sensor_id = 0;

	pdata = (struct sprd_sensor_dev_info_tag *)client->dev.platform_data;
	if (pdata) {
		mutex_destroy(&pdata->sync_lock);
		mutex_destroy(&pdata->set_voltage_lock);
		sensor_id = pdata->sensor_id;
		sprd_sensor_free_gpio(&client->dev, pdata);
		devm_kfree(&client->dev, pdata);
	}
	pdata = NULL;
	client->dev.platform_data = NULL;
	s_sensor_dev_data[sensor_id] = NULL;

	return 0;
}

static const struct of_device_id sprd_sensor_main_of_match_table[] = {
	{.compatible = "sprd,sensor-main"},
	{/* MUST end with empty struct */},
};

static const struct i2c_device_id sprd_sensor_main_ids[] = {
	{}
};

static struct i2c_driver sprd_main_sensor_driver = {
	.driver = {
		.of_match_table =
		of_match_ptr(sprd_sensor_main_of_match_table),
		.name = SPRD_SENSOR_MAIN_DRIVER_NAME,
		.owner = THIS_MODULE,
		},
	.probe = sprd_sensor_probe,
	.remove = sprd_sensor_remove,
	.id_table = sprd_sensor_main_ids,
};

static const struct of_device_id sprd_sensor_main2_of_match_table[] = {
	{.compatible = "sprd,sensor-main2"},
	{/* MUST end with empty struct */},
};

static const struct i2c_device_id sprd_sensor_main2_ids[] = {
	{}
};

static struct i2c_driver sprd_main2_sensor_driver = {
	.driver = {
		.of_match_table =
			of_match_ptr(sprd_sensor_main2_of_match_table),
		.name = SPRD_SENSOR_MAIN2_DRIVER_NAME,
		.owner = THIS_MODULE,
		},
	.probe = sprd_sensor_probe,
	.remove = sprd_sensor_remove,
	.id_table = sprd_sensor_main2_ids,
};

static const struct of_device_id sprd_sensor_main3_of_match_table[] = {
	{.compatible = "sprd,sensor-main3"},
	{/* MUST end with empty struct */},
};

static const struct i2c_device_id sprd_sensor_main3_ids[] = {
	{}
};

static struct i2c_driver sprd_main3_sensor_driver = {
	.driver = {
		.of_match_table =
			of_match_ptr(sprd_sensor_main3_of_match_table),
		.name = SPRD_SENSOR_MAIN3_DRIVER_NAME,
		.owner = THIS_MODULE,
		},
	.probe = sprd_sensor_probe,
	.remove = sprd_sensor_remove,
	.id_table = sprd_sensor_main3_ids,
};

static const struct i2c_device_id sprd_sensor_sub_ids[] = {
	{}
};

static const struct of_device_id sprd_sensor_sub_of_match_table[] = {
	{.compatible = "sprd,sensor-sub"},
	{/* MUST end with empty struct */},
};

static struct i2c_driver sprd_sub_sensor_driver = {
	.driver = {
		.of_match_table =
			of_match_ptr(sprd_sensor_sub_of_match_table),
		.name = SPRD_SENSOR_SUB_DRIVER_NAME,
		.owner = THIS_MODULE,
		},
	.probe = sprd_sensor_probe,
	.remove = sprd_sensor_remove,
	.id_table = sprd_sensor_sub_ids,
};

static const struct i2c_device_id sprd_sensor_sub2_ids[] = {
	{}
};

static const struct of_device_id sprd_sensor_sub2_of_match_table[] = {
	{.compatible = "sprd,sensor-sub2"},
	{/* MUST end with empty struct */},
};

static struct i2c_driver sprd_sub2_sensor_driver = {
	.driver = {
		.of_match_table =
			of_match_ptr(sprd_sensor_sub2_of_match_table),
		.name = SPRD_SENSOR_SUB2_DRIVER_NAME,
		.owner = THIS_MODULE,
		},
	.probe = sprd_sensor_probe,
	.remove = sprd_sensor_remove,
	.id_table = sprd_sensor_sub2_ids,
};

static const struct i2c_device_id sprd_sensor_sub3_ids[] = {
	{}
};

static const struct of_device_id sprd_sensor_sub3_of_match_table[] = {
	{.compatible = "sprd,sensor-sub3"},
	{/* MUST end with empty struct */},
};

static struct i2c_driver sprd_sub3_sensor_driver = {
	.driver = {
		.of_match_table =
			of_match_ptr(sprd_sensor_sub3_of_match_table),
		.name = SPRD_SENSOR_SUB3_DRIVER_NAME,
		.owner = THIS_MODULE,
		},
	.probe = sprd_sensor_probe,
	.remove = sprd_sensor_remove,
	.id_table = sprd_sensor_sub3_ids,
};

int sprd_sensor_register_driver(void)
{
	int ret = 0;

	memset(s_sensor_dev_data, 0, sizeof(s_sensor_dev_data));
	ret = i2c_add_driver(&sprd_main_sensor_driver);
	pr_info("register main sensor:%d\n", ret);
	usleep_range(10*1000, 11*1000);
	ret = i2c_add_driver(&sprd_sub_sensor_driver);
	pr_info("register sub sensor:%d\n", ret);
	usleep_range(10*1000, 11*1000);
	ret = i2c_add_driver(&sprd_main2_sensor_driver);
	pr_info("register main2 sensor:%d\n", ret);
	usleep_range(10*1000, 11*1000);
	ret = i2c_add_driver(&sprd_sub2_sensor_driver);
	pr_info("register  sub2 sensor:%d\n", ret);
	usleep_range(10*1000, 11*1000);
	ret = i2c_add_driver(&sprd_main3_sensor_driver);
	pr_info("register main3 sensor:%d\n", ret);
	usleep_range(10*1000, 11*1000);
	ret = i2c_add_driver(&sprd_sub3_sensor_driver);
	pr_info("register  sub3 sensor:%d\n", ret);

	return 0;
}

void sprd_sensor_unregister_driver(void)
{
	i2c_del_driver(&sprd_main_sensor_driver);
	i2c_del_driver(&sprd_sub_sensor_driver);
	i2c_del_driver(&sprd_main2_sensor_driver);
	i2c_del_driver(&sprd_sub2_sensor_driver);
	i2c_del_driver(&sprd_main3_sensor_driver);
	i2c_del_driver(&sprd_sub3_sensor_driver);
}

int select_sensor_mclk(uint8_t clk_set, char **clk_src_name, uint8_t *clk_div)
{
	unsigned char i = 0;
	unsigned char j = 0;
	unsigned char mark_src = 0;
	unsigned char mark_div = 0;
	unsigned char mark_src_tmp = 0;
	int clk_tmp = 0x7ffffff;
	int src_delta = 0x7ffffff;
	int src_delta_min = 0x7ffffff;
	int div_delta_min = 0x7ffffff;
	int mclk_count = ARRAY_SIZE(c_sensor_mclk_tab);

	if (clk_set > 96 || !clk_src_name || !clk_div)
		return -EPERM;

	for (i = 0; i < 8; i++) {
		clk_tmp = (int)(clk_set * (i + 1));
		src_delta_min = 0x7ffffff;
		for (j = 0; j < mclk_count; j++) {
			src_delta = c_sensor_mclk_tab[j].clock - clk_tmp;
			src_delta = (src_delta > 0) ?
				(src_delta) : (-src_delta);
			if (src_delta < src_delta_min) {
				src_delta_min = src_delta;
				mark_src_tmp = j;
			}
		}
		if (src_delta_min < div_delta_min) {
			div_delta_min = src_delta_min;
			mark_src = mark_src_tmp;
			mark_div = i;
		}
	}
	pr_debug("src %d, div=%d .\n", mark_src, mark_div);

	*clk_src_name = c_sensor_mclk_tab[mark_src].src_name;
	*clk_div = mark_div + 1;

	return 0;
}

int sprd_sensor_set_mclk(unsigned int *saved_clk, unsigned int set_mclk,
			 int sensor_id)
{
	int ret = 0;
	struct sprd_sensor_dev_info_tag *p_dev;
	char *clk_src_name = NULL;
	unsigned char clk_div = 0;
	struct clk *clk_parent = NULL;

	p_dev = sprd_sensor_get_dev_context(sensor_id);
	if (!p_dev) {
		pr_err("sensor clock no device %d\n", sensor_id);
		return -EINVAL;
	}
    pr_info("set_mclk %d\n", set_mclk);

	if (set_mclk == 0  && p_dev->mclk_count) {
		if (p_dev->mclk_freq) {
#ifndef MCLK_NEW_PROCESS
			if (p_dev->sensor_eb)
				clk_disable_unprepare(p_dev->sensor_eb);
			else if (p_dev->ccir_eb)
				clk_disable_unprepare(p_dev->ccir_eb);
#endif

			if (p_dev->sensor_clk) {
				ret = clk_set_parent(p_dev->sensor_clk,
					       p_dev->sensor_clk_default);
				if (ret) {
					pr_err("set_parent failed\n");
					goto exit;
				}
				ret = clk_set_rate(p_dev->sensor_clk,
					     p_dev->sensor_clk_default_rate);
				if (ret) {
					pr_err("set rate failed\n");
					goto exit;
				}
				clk_disable_unprepare(p_dev->sensor_clk);
			}
#ifdef MCLK_NEW_PROCESS
			if (p_dev->sensor_eb)
				clk_disable_unprepare(p_dev->sensor_eb);
			else if (p_dev->ccir_eb)
				clk_disable_unprepare(p_dev->ccir_eb);
#endif
			p_dev->mclk_count--;
		}
	} else if (!p_dev->mclk_count && set_mclk) {
		if (set_mclk > SPRD_SENSOR_MAX_MCLK)
			set_mclk = SPRD_SENSOR_MAX_MCLK;

		if (p_dev->sensor_eb && p_dev->sensor_clk) {
			ret = select_sensor_mclk(set_mclk, &clk_src_name,
						 &clk_div);
			pr_debug("clk_src_name %s\n", clk_src_name);
			if (ret != 0) {
				pr_err("select sensor mclk error\n");
				goto exit;
			}

			clk_parent =
			of_clk_get_by_name(p_dev->dev_node, clk_src_name);
			if (!clk_parent) {
				pr_err("get clk parent error\n");
				return -EINVAL;
			}
			pr_debug("set_mclk %d sensor_id %d\n",
				set_mclk, sensor_id);
#ifdef MCLK_NEW_PROCESS
			clk_prepare_enable(p_dev->sensor_eb);
#endif
			ret = clk_set_parent(p_dev->sensor_clk, clk_parent);
			if (ret) {
				pr_err("set parent failed\n");
				goto exit;
			}
			ret = clk_set_rate(p_dev->sensor_clk,
				     (set_mclk * SPRD_SENSOR_MCLK_VALUE));
			if (ret) {
				pr_err("set rate failed\n");
				goto exit;
			}
#ifndef MCLK_NEW_PROCESS

			clk_prepare_enable(p_dev->sensor_eb);
#endif
		} else if (p_dev->ccir_eb && p_dev->sensor_clk) {
			clk_prepare_enable(p_dev->ccir_eb);
		}

		clk_prepare_enable(p_dev->sensor_clk);
		p_dev->mclk_count++;
	} else if (p_dev->mclk_count && set_mclk != p_dev->mclk_freq) {
			ret = select_sensor_mclk(set_mclk, &clk_src_name,
						 &clk_div);
			pr_debug("clk_src_name %s\n", clk_src_name);
			if (ret != 0) {
				pr_err("select sensor mclk error\n");
				goto exit;
			}

			clk_parent =
			of_clk_get_by_name(p_dev->dev_node, clk_src_name);
			if (!clk_parent) {
				pr_err("get clk parent error\n");
				return -EINVAL;
			}
			pr_debug("set_mclk %d sensor_id %d\n",
				set_mclk, sensor_id);

			ret = clk_set_parent(p_dev->sensor_clk, clk_parent);
			if (ret) {
				pr_err("set parent failed\n");
				goto exit;
			}
			ret = clk_set_rate(p_dev->sensor_clk,
				     (set_mclk * SPRD_SENSOR_MCLK_VALUE));
			if (ret) {
				pr_err("set rate failed\n");
				goto exit;
			}
			pr_debug("set rate mclk count %d \n", p_dev->mclk_count);
	}
	p_dev->mclk_freq = set_mclk;
exit:
	pr_debug("sensor mclk %d ret= %d\n", set_mclk, ret);
	return ret;
}

void sprd_sensor_sync_lock(int sensor_id)
{
	struct sprd_sensor_dev_info_tag *p_dev;

	p_dev = sprd_sensor_get_dev_context(sensor_id);
	if (!p_dev)
		pr_err("%s, error\n", __func__);
	else
		mutex_lock(&p_dev->sync_lock);
}

void sprd_sensor_sync_unlock(int sensor_id)
{
	struct sprd_sensor_dev_info_tag *p_dev;

	p_dev = sprd_sensor_get_dev_context(sensor_id);
	if (!p_dev)
		pr_err("%s, error\n", __func__);
	else
		mutex_unlock(&p_dev->sync_lock);
}

int sprd_sensor_set_pd_level(int sensor_id, int power_level)
{
	int ret = 0;
	struct sprd_sensor_dev_info_tag *p_dev;
	int pwn_gpio_id = 0;

	p_dev = sprd_sensor_get_dev_context(sensor_id);
	if (!p_dev) {
		pr_err("%s, error\n", __func__);
		return -EINVAL;
	}
	pwn_gpio_id = p_dev->gpio_tab[SPRD_SENSOR_PWN_GPIO_TAG_E];
	if (gpio_is_valid(pwn_gpio_id)) {
		if (power_level == 0)
			ret = gpio_direction_output(pwn_gpio_id, 0);
		else
			ret = gpio_direction_output(pwn_gpio_id, 1);

		pr_debug("sensor %d set pd level %d\n", sensor_id, power_level);
	}

	return ret;
}

int sprd_sensor_reset(int sensor_id, unsigned int level, unsigned int width)
{
	int ret = 0;
	struct sprd_sensor_dev_info_tag *p_dev;
	int reset_gpio_id = 0;

	p_dev = sprd_sensor_get_dev_context(sensor_id);
	if (!p_dev) {
		pr_err("%s, error\n", __func__);
		return -EINVAL;
	}
	reset_gpio_id = p_dev->gpio_tab[SPRD_SENSOR_RESET_GPIO_TAG_E];
	if (gpio_is_valid(reset_gpio_id)) {
		ret = gpio_direction_output(reset_gpio_id, level);
		gpio_set_value(reset_gpio_id, level);
		mdelay(width);
		gpio_set_value(reset_gpio_id, !level);
	}
	pr_debug("sensor %d reset%d width %d\n", sensor_id, level, width);

	return ret;
}

int sprd_sensor_set_rst_level(int sensor_id, uint32_t rst_level)
{
	int ret = 0;
	struct sprd_sensor_dev_info_tag *p_dev;
	int reset_gpio_id = 0;

	p_dev = sprd_sensor_get_dev_context(sensor_id);
	if (!p_dev) {
		pr_err("%s, error\n", __func__);
		return -EINVAL;
	}
	reset_gpio_id = p_dev->gpio_tab[SPRD_SENSOR_RESET_GPIO_TAG_E];
	if (gpio_is_valid(reset_gpio_id)) {
		ret = gpio_direction_output(reset_gpio_id, rst_level);
		gpio_set_value(reset_gpio_id, rst_level);
	}
	pr_debug("sensor %d rst level %d\n", sensor_id, rst_level);

	return ret;
}

int sprd_sensor_set_mipi_level(int sensor_id, uint32_t plus_level)
{
	struct sprd_sensor_dev_info_tag *p_dev;
	int mipi_gpio_id = 0, mipi_switch_en = 0;
	int ret = 0;

	p_dev = sprd_sensor_get_dev_context(sensor_id);
	if (!p_dev) {
		pr_err("%s, error\n", __func__);
		return -EINVAL;
	}

	mipi_switch_en = p_dev->gpio_tab[SPRD_SENSOR_MIPI_SWITCH_EN_GPIO_TAG_E];
	mipi_gpio_id = p_dev->gpio_tab[SPRD_SENSOR_MIPI_SWITCH_MODE_GPIO_TAG_E];
	if (gpio_is_valid(mipi_gpio_id)) {
		ret = gpio_direction_output(mipi_switch_en, 0);
		gpio_set_value(mipi_switch_en, 0);
		ret = gpio_direction_output(mipi_gpio_id, plus_level);
		gpio_set_value(mipi_gpio_id, plus_level);
		pr_err("%s mipi_switch_en %d mipi_gpio_id %d\n", __func__,
			mipi_switch_en, mipi_gpio_id);
	}

	return ret;
}

int sprd_sensor_set_i2c_addr(int sensor_id, uint16_t i2c_addr)
{
	struct sprd_sensor_dev_info_tag *p_dev = NULL;

	p_dev = sprd_sensor_get_dev_context(sensor_id);
	if (!p_dev) {
		pr_err("%s, error\n", __func__);
		return -EINVAL;
	}
	p_dev->i2c_info->addr = (p_dev->i2c_info->addr & (~0xFF)) | i2c_addr;

	return 0;
}

int sprd_sensor_set_i2c_clk(int sensor_id, uint32_t clock)
{
	struct sprd_sensor_dev_info_tag *p_dev = NULL;

	p_dev = sprd_sensor_get_dev_context(sensor_id);
	if (!p_dev) {
		pr_err("%s, error\n", __func__);
		return -EINVAL;
	}

    p_dev->i2c_clock = clock;
	pr_debug("set i2c %d clk %d\n", p_dev->i2c_info->adapter->nr, clock);

	return 0;

}

int sprd_sensor_set_i2c_burst(int sensor_id, uint32_t burst_mode)
{
	struct sprd_sensor_dev_info_tag *p_dev = NULL;

	p_dev = sprd_sensor_get_dev_context(sensor_id);
	if (!p_dev) {
		pr_err("%s, error\n", __func__);
		return -EINVAL;
	}

    p_dev->i2c_burst = burst_mode;
	pr_debug("set sensor_id:%d, i2c_burst_mode:%d\n", sensor_id, burst_mode);

	return 0;

}

int sprd_sensor_read_reg(int sensor_id, struct sensor_reg_bits_tag *pReg)
{
	uint8_t cmd[2] = { 0 };
	uint16_t w_cmd_num = 0;
	uint16_t r_cmd_num = 0;
	uint8_t buf_r[2] = { 0 };
	int ret = -1;
	struct i2c_msg msg_r[2];
	uint16_t reg_addr;
	int i = 0;
	int cnt = 0;
	struct sprd_sensor_dev_info_tag *p_dev = NULL;

	p_dev = sprd_sensor_get_dev_context(sensor_id);
	if (!p_dev || !p_dev->i2c_info) {
		pr_err("%s, error\n", __func__);
		return -EINVAL;
	}
	reg_addr = pReg->reg_addr;
	if (SPRD_SENSOR_I2C_REG_16BIT ==
	(pReg->reg_bits & SPRD_SENSOR_I2C_REG_16BIT)) {
		cmd[w_cmd_num++] =
		(uint8_t) ((reg_addr >> 8) & SPRD_SENSOR_LOW_EIGHT_BIT);
		cmd[w_cmd_num++] =
		(uint8_t) (reg_addr & SPRD_SENSOR_LOW_EIGHT_BIT);
	} else {
		cmd[w_cmd_num++] = (uint8_t) reg_addr;
	}

	if (SPRD_SENSOR_I2C_VAL_16BIT ==
	(pReg->reg_bits & SPRD_SENSOR_I2C_VAL_16BIT))
		r_cmd_num = SPRD_SENSOR_CMD_BITS_16;
	else
		r_cmd_num = SPRD_SENSOR_CMD_BITS_8;

	for (i = 0; i < SPRD_SENSOR_I2C_OP_TRY_NUM; i++) {
		msg_r[0].addr = p_dev->i2c_info->addr;
		if (1000000 == p_dev->i2c_clock)
			msg_r[0].flags = 0 | I2C_1M_FLAG_CAM;
		else
			msg_r[0].flags = 0 | I2C_400K_FLAG_CAM;
		msg_r[0].buf = cmd;
		msg_r[0].len = w_cmd_num;
		msg_r[1].addr = p_dev->i2c_info->addr;
		if (1000000 == p_dev->i2c_clock)
			msg_r[1].flags = I2C_M_RD | I2C_1M_FLAG_CAM;
		else
			msg_r[1].flags = I2C_M_RD | I2C_400K_FLAG_CAM;
		msg_r[1].buf = buf_r;
		msg_r[1].len = r_cmd_num;
		cnt = i2c_transfer(p_dev->i2c_info->adapter, msg_r, 2);
		if (cnt != SPRD_SENSOR_I2C_READ_SUCCESS_CNT) {
			pr_err("%s fail, ret %d, addr 0x%x, reg_addr 0x%x\n",
			__func__, ret, p_dev->i2c_info->addr, reg_addr);
			usleep_range(1000, 1500);
		} else {
			pReg->reg_value =
			(r_cmd_num ==
			1) ? (uint16_t) buf_r[0] : (uint16_t) ((buf_r[0] <<
								8) +
								buf_r[1]);
			ret = 0;
			break;
		}
	}

	return ret;
}

int sprd_sensor_write_reg(int sensor_id, struct sensor_reg_bits_tag *pReg)
{
	uint8_t cmd[4] = { 0 };
	uint32_t index = 0;
	uint32_t cmd_num = 0;
	struct i2c_msg msg_w;
	int32_t ret = 0;
	uint16_t subaddr;
	uint16_t data;
	int i;
	int cnt = 0;
	struct sprd_sensor_dev_info_tag *p_dev = NULL;

	p_dev = sprd_sensor_get_dev_context(sensor_id);
	if (!p_dev || !p_dev->i2c_info) {
		pr_err("%s, error\n", __func__);
		return -EINVAL;
	}
	subaddr = pReg->reg_addr;
	data = pReg->reg_value;

	if (SPRD_SENSOR_I2C_REG_16BIT ==
		(pReg->reg_bits & SPRD_SENSOR_I2C_REG_16BIT)) {
		cmd[cmd_num++] =
		(uint8_t) ((subaddr >> 8) & SPRD_SENSOR_LOW_EIGHT_BIT);
		index++;
		cmd[cmd_num++] =
		(uint8_t) (subaddr & SPRD_SENSOR_LOW_EIGHT_BIT);
		index++;
	} else {
		cmd[cmd_num++] = (uint8_t) subaddr;
		index++;
	}

	if (SPRD_SENSOR_I2C_VAL_16BIT ==
		(pReg->reg_bits & SPRD_SENSOR_I2C_VAL_16BIT)) {
		cmd[cmd_num++] =
		(uint8_t) ((data >> 8) & SPRD_SENSOR_LOW_EIGHT_BIT);
		index++;
		cmd[cmd_num++] = (uint8_t) (data & SPRD_SENSOR_LOW_EIGHT_BIT);
		index++;
	} else {
		cmd[cmd_num++] = (uint8_t) data;
		index++;
	}

	/* pr_info("%s subaddr %x\n", __func__, subaddr); */

	if (subaddr != SPRD_SENSOR_WRITE_DELAY) {
		for (i = 0; i < SPRD_SENSOR_I2C_OP_TRY_NUM; i++) {
			msg_w.addr = p_dev->i2c_info->addr;
			if (1000000 == p_dev->i2c_clock)
				msg_w.flags = 0 | I2C_1M_FLAG_CAM;
			else
				msg_w.flags = 0 | I2C_400K_FLAG_CAM;
            msg_w.buf = cmd;
			msg_w.len = index;
			cnt = i2c_transfer(p_dev->i2c_info->adapter, &msg_w, 1);
			if (cnt != SPRD_SENSOR_I2C_WRITE_SUCCESS_CNT) {
				pr_err("%s fail to:\n"
					"i2cAddr=%x,\n"
					"addr=%x, value=%x, bit=%d\n",
					__func__, p_dev->i2c_info->addr,
					pReg->reg_addr, pReg->reg_value,
					pReg->reg_bits);
				ret = -1;
				continue;
			} else {
				ret = 0;
				break;
			}
		}
	} else {
		if (data >= 20)
			SLEEP_MS(data);
		else
			mdelay(data);
	}

	return ret;
}

int sprd_sensor_burst_write_samsung(struct sensor_reg_tag *p_reg_table,
				int sensor_id, uint32_t init_table_size,
				uint32_t reg_bits)
{
	int ret = 0;
	uint32_t i = 0;
	uint32_t written_num = 0;
	uint32_t wr_num_once = 0;
	uint8_t *p_reg_val_tmp = 0;
	struct i2c_msg msg_w;
	int cnt = 0;
	struct sprd_sensor_mem_tag p_mem = {0};
	struct sprd_sensor_dev_info_tag *p_dev = NULL;
	struct sensor_reg_bits_tag reg_bit;

	p_dev = sprd_sensor_get_dev_context(sensor_id);
	if (!p_dev || !p_dev->i2c_info) {
		pr_err("%s, error\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	ret = sprd_sensor_malloc(&p_mem,
			init_table_size * sizeof(struct sensor_reg_tag));
	if (ret) {
		pr_err("%s, error\n", __func__);
		goto exit;
	}
	p_reg_val_tmp = (uint8_t *) (p_mem.buf_ptr);

	while(written_num < init_table_size) {
		if (0x6004 == p_reg_table[written_num].reg_addr) {
			wr_num_once = 0;
			//first write , burst start signal& select page & set base-addr
			for(i = 0; i < 3; i++) {
				reg_bit.reg_addr = p_reg_table[written_num].reg_addr;
				reg_bit.reg_value = p_reg_table[written_num].reg_value;
				reg_bit.reg_bits = reg_bits;
				ret = sprd_sensor_write_reg(sensor_id, &reg_bit);
				if (ret) {
					pr_err("sprd_sensor_burst_write_samsung failed!\n");
					goto exit;
				}
				written_num ++;
			}
			//second write, burst reg
			while(p_reg_table[written_num].reg_addr != 0x6004) {
				if(0 == wr_num_once) {
						p_reg_val_tmp[wr_num_once++] =
							(uint8_t) ((p_reg_table[written_num].reg_addr >> 8) & 0xff);
						p_reg_val_tmp[wr_num_once++] =
							(uint8_t) (p_reg_table[written_num].reg_addr & 0xff);
						p_reg_val_tmp[wr_num_once++] =
							(uint8_t) ((p_reg_table[written_num].reg_value >> 8) & 0xff);
						p_reg_val_tmp[wr_num_once++] =
							(uint8_t) (p_reg_table[written_num].reg_value & 0xff);
				} else {
						p_reg_val_tmp[wr_num_once++] =
							(uint8_t) ((p_reg_table[written_num].reg_value >> 8) & 0xff);
						p_reg_val_tmp[wr_num_once++] =
							(uint8_t) (p_reg_table[written_num].reg_value & 0xff);
				}
				written_num ++;
			}

			for(i = 0; i < SPRD_SENSOR_I2C_OP_TRY_NUM; i++) {
				msg_w.addr = p_dev->i2c_info->addr;
				if (1000000 == p_dev->i2c_clock)
					msg_w.flags = 0 | I2C_1M_FLAG_CAM;
				else
					msg_w.flags = 0 | I2C_400K_FLAG_CAM;
				msg_w.buf = p_reg_val_tmp;
				msg_w.len = wr_num_once;
				cnt = i2c_transfer(p_dev->i2c_info->adapter, &msg_w, 1);
				if (cnt != SPRD_SENSOR_I2C_WRITE_SUCCESS_CNT) {
					if(i < (SPRD_SENSOR_I2C_OP_TRY_NUM - 1)) {
						continue;
					}
					pr_err("sprd_sensor_burst_write_samsung failed!\n");
					ret = -EINVAL;
					goto exit;
				} else {
					break;
				}
			}

			//last write, burst stop signal
			reg_bit.reg_addr = p_reg_table[written_num].reg_addr;
			reg_bit.reg_value = p_reg_table[written_num].reg_value;
			reg_bit.reg_bits = reg_bits;
			ret = sprd_sensor_write_reg(sensor_id, &reg_bit);
			if (ret) {
				pr_err("sprd_sensor_burst_write_samsung failed!\n");
				goto exit;
			}
			written_num ++;
		} else {
			reg_bit.reg_addr = p_reg_table[written_num].reg_addr;
			reg_bit.reg_value = p_reg_table[written_num].reg_value;
			reg_bit.reg_bits = reg_bits;
			ret = sprd_sensor_write_reg(sensor_id, &reg_bit);
			if (ret) {
				pr_err("sprd_sensor_burst_write_samsung failed!\n");
				goto exit;
			}
			written_num ++;
		}
	}

exit:
	if (p_mem.buf_ptr)
		sprd_sensor_free(&p_mem);

	return ret;
}

int sprd_sensor_burst_write_reg16_val8(struct sensor_reg_tag *p_reg_table,
				int sensor_id, uint32_t init_table_size,
				uint32_t reg_bits)
{
	int ret = 0;
	uint32_t i = 0;
	uint32_t written_num = 0;
	uint32_t wr_num_once = 0;
	uint8_t *p_reg_val_tmp = 0;
	struct i2c_msg msg_w;
	int cnt = 0;
	struct sprd_sensor_mem_tag p_mem = {0};
	struct sprd_sensor_dev_info_tag *p_dev = NULL;

	p_dev = sprd_sensor_get_dev_context(sensor_id);
	if (!p_dev || !p_dev->i2c_info) {
		pr_err("%s, error\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	ret = sprd_sensor_malloc(&p_mem,
			init_table_size * sizeof(struct sensor_reg_tag));
	if (ret) {
		pr_err("%s, error\n", __func__);
		goto exit;
	}
	p_reg_val_tmp = (uint8_t *) (p_mem.buf_ptr);

	while (written_num < init_table_size) {
		wr_num_once = 0;
		p_reg_val_tmp[wr_num_once++] =
			(uint8_t) ((p_reg_table[written_num].reg_addr >> 8) & 0xff);
		p_reg_val_tmp[wr_num_once++] =
			(uint8_t) (p_reg_table[written_num].reg_addr & 0xff);
		p_reg_val_tmp[wr_num_once++] =
			(uint8_t) (p_reg_table[written_num].reg_value & 0xff);
		written_num++;

		while ((written_num < init_table_size) &&
			(p_reg_table[written_num].reg_addr ==
			p_reg_table[written_num - 1].reg_addr + 1)) {
			p_reg_val_tmp[wr_num_once++] =
				(uint8_t) (p_reg_table[written_num].reg_value & 0xff);
			written_num++;
		}

		for(i = 0; i < SPRD_SENSOR_I2C_OP_TRY_NUM; i++) {
			msg_w.addr = p_dev->i2c_info->addr;
			if (1000000 == p_dev->i2c_clock)
				msg_w.flags = 0 | I2C_1M_FLAG_CAM;
			else
				msg_w.flags = 0 | I2C_400K_FLAG_CAM;
            msg_w.buf = p_reg_val_tmp;
			msg_w.len = wr_num_once;
			cnt = i2c_transfer(p_dev->i2c_info->adapter, &msg_w, 1);
			if (cnt != SPRD_SENSOR_I2C_WRITE_SUCCESS_CNT) {
				if(i < (SPRD_SENSOR_I2C_OP_TRY_NUM - 1)) {
					continue;
				}
				pr_err("sprd_sensor_burst_write_common failed!\n");
				ret = -EINVAL;
				goto exit;
			} else {
				break;
			}
		}

	}
exit:
	if (p_mem.buf_ptr)
		sprd_sensor_free(&p_mem);

	return ret;
}

int sprd_sensor_burst_write_reg16_val16(struct sensor_reg_tag *p_reg_table,
				int sensor_id, uint32_t init_table_size,
				uint32_t reg_bits)
{
	int ret = 0;
	uint32_t i = 0;
	uint32_t written_num = 0;
	uint32_t wr_num_once = 0;
	uint8_t *p_reg_val_tmp = 0;
	struct i2c_msg msg_w;
	int cnt = 0;
	struct sprd_sensor_mem_tag p_mem = {0};
	struct sprd_sensor_dev_info_tag *p_dev = NULL;

	p_dev = sprd_sensor_get_dev_context(sensor_id);
	if (!p_dev || !p_dev->i2c_info) {
		pr_err("%s, error\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	ret = sprd_sensor_malloc(&p_mem,
			init_table_size * sizeof(struct sensor_reg_tag));
	if (ret) {
		pr_err("%s, error\n", __func__);
		goto exit;
	}
	p_reg_val_tmp = (uint8_t *) (p_mem.buf_ptr);

	while (written_num < init_table_size) {
		wr_num_once = 0;
		p_reg_val_tmp[wr_num_once++] =
			(uint8_t) ((p_reg_table[written_num].reg_addr >> 8) & 0xff);
		p_reg_val_tmp[wr_num_once++] =
			(uint8_t) (p_reg_table[written_num].reg_addr & 0xff);
		p_reg_val_tmp[wr_num_once++] =
			(uint8_t) ((p_reg_table[written_num].reg_value >> 8) & 0xff);
		p_reg_val_tmp[wr_num_once++] =
			(uint8_t) (p_reg_table[written_num].reg_value & 0xff);
		written_num++;

		while ((written_num < init_table_size) &&
			(p_reg_table[written_num].reg_addr ==
			p_reg_table[written_num - 1].reg_addr + 2)) {
			p_reg_val_tmp[wr_num_once++] =
				(uint8_t) ((p_reg_table[written_num].reg_value >> 8) & 0xff);
			p_reg_val_tmp[wr_num_once++] =
				(uint8_t) (p_reg_table[written_num].reg_value & 0xff);
			written_num++;
		}

		for(i = 0; i < SPRD_SENSOR_I2C_OP_TRY_NUM; i++) {
			msg_w.addr = p_dev->i2c_info->addr;
			if (1000000 == p_dev->i2c_clock)
				msg_w.flags = 0 | I2C_1M_FLAG_CAM;
			else
				msg_w.flags = 0 | I2C_400K_FLAG_CAM;
            msg_w.buf = p_reg_val_tmp;
			msg_w.len = wr_num_once;
			cnt = i2c_transfer(p_dev->i2c_info->adapter, &msg_w, 1);
			if (cnt != SPRD_SENSOR_I2C_WRITE_SUCCESS_CNT) {
				if(i < (SPRD_SENSOR_I2C_OP_TRY_NUM - 1)) {
					continue;
				}
				pr_err("sprd_sensor_burst_write_common failed!\n");
				ret = -EINVAL;
				goto exit;
			} else {
				break;
			}
		}

	}
exit:
	if (p_mem.buf_ptr)
		sprd_sensor_free(&p_mem);

	return ret;
}


int sprd_sensor_write_regtab(struct sensor_reg_tab_tag *p_reg_table,
				int sensor_id)
{
	char *pBuff = NULL;
	uint32_t cnt = p_reg_table->reg_count;
	int ret = 0;
	uint32_t size;
	struct sensor_reg_tag *sensor_reg_ptr;
	struct sensor_reg_bits_tag reg_bit;
	uint32_t i;
	/*struct timeval time1 = {0}, time2 = {0};*/
	struct sprd_sensor_mem_tag p_mem = {0};

	size = cnt * sizeof(*sensor_reg_ptr);
	if(cnt != 0 && size/cnt != sizeof(*sensor_reg_ptr)) {
		ret = -EINVAL;
		pr_err("sensor w err: interger overflow occurs\n");
		goto exit;
	}
	ret = sprd_sensor_malloc(&p_mem, size);
	if (ret) {
		pr_err("SENSOR: wr reg tab alloc fail %d\n", ret);
		goto exit;
	}
	pBuff = (char *)p_mem.buf_ptr;

	ret = copy_from_user(pBuff, p_reg_table->sensor_reg_tab_ptr, size);
	if (ret) {
		pr_err("sensor w err:copy user fail, size %d\n", size);
		goto exit;
	}

	sensor_reg_ptr = (struct sensor_reg_tag *)pBuff;

	/*do_gettimeofday(&time1);*/

	switch (p_reg_table->burst_mode) {
	case SPRD_SENSOR_I2C_SINGLE_WRITE: {
		for (i = 0; i < cnt; i++) {
			reg_bit.reg_addr = sensor_reg_ptr[i].reg_addr;
			reg_bit.reg_value = sensor_reg_ptr[i].reg_value;
			reg_bit.reg_bits = p_reg_table->reg_bits;
			ret = sprd_sensor_write_reg(sensor_id, &reg_bit);
			if (ret) {
				pr_err("SENSOR WRITE REG TAB write reg fail\n");
				goto exit;
			}
		}
		break;
	}

	case SPRD_SENSOR_I2C_BURST_SAMSUNG: {
		ret = sprd_sensor_burst_write_samsung(sensor_reg_ptr,
						sensor_id,
						p_reg_table->reg_count,
						p_reg_table->reg_bits);
		if (ret) {
			pr_err("sprd_sensor_burst_write_samsung failed\n");
			goto exit;
		}
		break;
	}

	case SPRD_SENSOR_I2C_BURST_REG16_VAL8: {
		ret = sprd_sensor_burst_write_reg16_val8(sensor_reg_ptr,
						sensor_id,
						p_reg_table->reg_count,
						p_reg_table->reg_bits);
		if (ret) {
			pr_err("sprd_sensor_burst_write_common failed\n");
			goto exit;
		}
		break;
	}

	case SPRD_SENSOR_I2C_BURST_REG16_VAL16: {
		ret = sprd_sensor_burst_write_reg16_val16(sensor_reg_ptr,
						sensor_id,
						p_reg_table->reg_count,
						p_reg_table->reg_bits);
		if (ret) {
			pr_err("sprd_sensor_burst_write_common failed\n");
			goto exit;
		}
		break;
	}

	default: {
		pr_err("invalid burst mode, turn to single write mode\n");
		for (i = 0; i < cnt; i++) {
			reg_bit.reg_addr = sensor_reg_ptr[i].reg_addr;
			reg_bit.reg_value = sensor_reg_ptr[i].reg_value;
			reg_bit.reg_bits = p_reg_table->reg_bits;
			ret = sprd_sensor_write_reg(sensor_id, &reg_bit);
			if (ret) {
				pr_err("SENSOR WRITE REG TAB write reg fail\n");
				goto exit;
			}
		}
		break;
	}
	}
exit:
	if (p_mem.buf_ptr)
		sprd_sensor_free(&p_mem);
	/*do_gettimeofday(&time2);
	pr_debug("sensor w RegTab: done, ret %d, cnt %d, time %d us\n",
		ret, cnt,
		(uint32_t) ((time2.tv_sec - time1.tv_sec) * 1000000 +
			(time2.tv_usec - time1.tv_usec)));*/
	return ret;
}

int sprd_sensor_k_write_regtab(struct sensor_reg_tab_tag *p_reg_table,
				int sensor_id)
{
	char *pBuff = NULL;
	uint32_t cnt = p_reg_table->reg_count;
	int ret = 0;
	uint32_t size;
	struct sensor_reg_tag *sensor_reg_ptr;
	struct sensor_reg_bits_tag reg_bit;
	uint32_t i;
	/*struct timeval time1 = {0}, time2 = {0};*/
	struct sprd_sensor_mem_tag p_mem = {0};

	size = cnt * sizeof(*sensor_reg_ptr);
	ret = sprd_sensor_malloc(&p_mem, size);
	if (ret) {
		pr_err("SENSOR: wr reg tab alloc fail %d\n", ret);
		goto exit;
	}
	pBuff = (char *)p_mem.buf_ptr;

	memcpy(pBuff, p_reg_table->sensor_reg_tab_ptr, size);

	sensor_reg_ptr = (struct sensor_reg_tag *)pBuff;

	switch (p_reg_table->burst_mode) {
	case SPRD_SENSOR_I2C_SINGLE_WRITE: {
		for (i = 0; i < cnt; i++) {
			reg_bit.reg_addr = sensor_reg_ptr[i].reg_addr;
			reg_bit.reg_value = sensor_reg_ptr[i].reg_value;
			reg_bit.reg_bits = p_reg_table->reg_bits;
			ret = sprd_sensor_write_reg(sensor_id, &reg_bit);
			if (ret) {
				pr_err("SENSOR WRITE REG TAB write reg fail\n");
				goto exit;
			}
		}
		break;
	}

	case SPRD_SENSOR_I2C_BURST_SAMSUNG: {
		ret = sprd_sensor_burst_write_samsung(sensor_reg_ptr,
						sensor_id,
						p_reg_table->reg_count,
						p_reg_table->reg_bits);
		if (ret) {
			pr_err("sprd_sensor_burst_write_samsung failed\n");
			goto exit;
		}
		break;
	}

	case SPRD_SENSOR_I2C_BURST_REG16_VAL8: {
		ret = sprd_sensor_burst_write_reg16_val8(sensor_reg_ptr,
						sensor_id,
						p_reg_table->reg_count,
						p_reg_table->reg_bits);
		if (ret) {
			pr_err("sprd_sensor_burst_write_common failed\n");
			goto exit;
		}
		break;
	}

	case SPRD_SENSOR_I2C_BURST_REG16_VAL16: {
		ret = sprd_sensor_burst_write_reg16_val16(sensor_reg_ptr,
						sensor_id,
						p_reg_table->reg_count,
						p_reg_table->reg_bits);
		if (ret) {
			pr_err("sprd_sensor_burst_write_common failed\n");
			goto exit;
		}
		break;
	}

	default: {
		pr_err("invalid burst mode, turn to single write mode\n");
		for (i = 0; i < cnt; i++) {
			reg_bit.reg_addr = sensor_reg_ptr[i].reg_addr;
			reg_bit.reg_value = sensor_reg_ptr[i].reg_value;
			reg_bit.reg_bits = p_reg_table->reg_bits;
			ret = sprd_sensor_write_reg(sensor_id, &reg_bit);
			if (ret) {
				pr_err("SENSOR WRITE REG TAB write reg fail\n");
				goto exit;
			}
		}
		break;
	}
	}
exit:
	if (p_mem.buf_ptr)
		sprd_sensor_free(&p_mem);
	/*do_gettimeofday(&time2);
	pr_debug("sensor w RegTab: done, ret %d, cnt %d, time %d us\n",
		ret, cnt,
		(uint32_t) ((time2.tv_sec - time1.tv_sec) * 1000000 +
			(time2.tv_usec - time1.tv_usec)));*/
	return ret;
}

int sprd_sensor_write_i2c(struct sensor_i2c_tag *i2c_tab,
				int sensor_id)
{
	uint8_t cmd[64] = { 0 };
	struct i2c_msg msg_w;
	uint32_t cnt = i2c_tab->i2c_count;
	int ret = -1;
	struct i2c_client *i2c_client;
	struct sprd_sensor_dev_info_tag *p_dev = NULL;
	int i2c_cnt = 0;

	p_dev = sprd_sensor_get_dev_context(sensor_id);
	if (!p_dev) {
		pr_err("%s, error\n", __func__);
		return -EINVAL;
	}

	if(cnt > sizeof(cmd)/sizeof(cmd[0])) {
		pr_err("sensor W I2C ERR: i2c_count is out of boundary, i2c_count: %d", cnt);
		return -EINVAL;
	}

	ret = copy_from_user(cmd, i2c_tab->i2c_data, cnt);

	if (ret) {
		pr_err("sensor W I2C ERR: copy user fail, size %d\n", cnt);
		goto exit;
	}

	msg_w.addr = i2c_tab->slave_addr;
	if (1000000 == p_dev->i2c_clock)
		msg_w.flags = 0 | I2C_1M_FLAG_CAM;
	else
		msg_w.flags = 0 | I2C_400K_FLAG_CAM;
    msg_w.buf = cmd;
	msg_w.len = cnt;

	if (p_dev->vcm_i2c_client == NULL)
		i2c_client = p_dev->i2c_info;
	else
		i2c_client = p_dev->vcm_i2c_client;
	if (IS_ERR_OR_NULL(i2c_client)) {
		pr_err("%s error %d\n", __func__, __LINE__);
		ret = -1;
		goto exit;
	}
	i2c_cnt = i2c_transfer(i2c_client->adapter, &msg_w, 1);
	if (i2c_cnt != SPRD_SENSOR_I2C_WRITE_SUCCESS_CNT) {
		pr_err("SENSOR: w reg fail, i2c_cnt: %d, addr: 0x%x\n",
		i2c_cnt, msg_w.addr);
		ret = -1;
	} else
		ret = 0;

exit:
	pr_debug("sensor w done, ret %d\n", ret);
	return ret;
}
EXPORT_SYMBOL(sprd_sensor_write_i2c);

int sprd_sensor_write_muti_i2c(struct sensor_muti_aec_i2c_tag *muti_aec_i2c)
{
	int ret = 0;
	uint16_t sensor_id[AEC_I2C_SENSOR_MAX];
	uint16_t i2c_slave_addr[AEC_I2C_SENSOR_MAX];
	uint16_t addr_bits_type[AEC_I2C_SENSOR_MAX];
	uint16_t data_bits_type[AEC_I2C_SENSOR_MAX];
	struct sensor_reg_tag msettings[AEC_I2C_SETTINGS_MAX];
	struct sensor_reg_tag ssettings[AEC_I2C_SETTINGS_MAX];
	struct sensor_reg_tag ssettings_2[AEC_I2C_SETTINGS_MAX];
	struct sensor_reg_bits_tag reg_bit;
	uint32_t i;
	struct sprd_sensor_dev_info_tag *p_dev = NULL;
	ktime_t id0_end = 0;
	ktime_t id1_end = 0;
	ktime_t id2_end = 0;

	ret = copy_from_user(sensor_id,
			(uint16_t __user *)(unsigned long) muti_aec_i2c->sensor_id,
			sizeof(sensor_id));
	if (ret) {
		pr_err("fail to read sensor id\n");
		goto exit;
	}

	ret = copy_from_user(i2c_slave_addr,
			(uint16_t __user *)(unsigned long) muti_aec_i2c->i2c_slave_addr,
			sizeof(i2c_slave_addr));
	if (ret) {
		pr_err("fail to read slave addr\n");
		goto exit;
	}

	ret = copy_from_user(addr_bits_type,
			(uint16_t __user *)(unsigned long) muti_aec_i2c->addr_bits_type,
			sizeof(addr_bits_type));
	if (ret) {
		pr_err("fail to read addr bits\n");
		goto exit;
	}

	ret = copy_from_user(data_bits_type,
			(uint16_t __user *)(unsigned long) muti_aec_i2c->data_bits_type,
			sizeof(data_bits_type));
	if (ret) {
		pr_err("fail to read data bits\n");
		goto exit;
	}

	ret = copy_from_user(msettings,
			(uint16_t __user *)(unsigned long) muti_aec_i2c->master_i2c_tab,
			muti_aec_i2c->msize * sizeof(struct sensor_reg_tag));
	if (ret) {
		pr_err("fail to read msetting\n");
		goto exit;
	}

	ret = copy_from_user(ssettings,
			(uint16_t __user *)(unsigned long) muti_aec_i2c->slave_i2c_tab,
			muti_aec_i2c->ssize * sizeof(struct sensor_reg_tag));
	if (ret) {
		pr_err("fail to read ssetting\n");
		goto exit;
	}

	// TODO may cause kernel crash

	ret = copy_from_user(ssettings_2,
			(uint16_t __user *)(unsigned long) muti_aec_i2c->slave_i2c_tab_2,
			muti_aec_i2c->ssize_2 * sizeof(struct sensor_reg_tag));
	if (ret) {
		pr_err("fail to read ssetting\n");
		goto exit;
	}

#if 0
	for (i = 0; i < muti_aec_i2c->msize; i++) {
		pr_info("m reg_addr reg_value 0x%x 0x%x\n",
			msettings[i].reg_addr, msettings[i].reg_value);
	}
	for (i = 0; i < muti_aec_i2c->ssize; i++) {
		pr_info("s reg_addr reg_value 0x%x 0x%x\n",
			ssettings[i].reg_addr, ssettings[i].reg_value);
	}
#endif

	/* master aec info set to i2c */

	if (muti_aec_i2c->msize > 0) {
		p_dev = sprd_sensor_get_dev_context(sensor_id[0]);
		if (!p_dev) {
			pr_err("%s, error\n", __func__);
			return -EINVAL;
		}

		switch (p_dev->i2c_burst) {
		case SPRD_SENSOR_I2C_SINGLE_WRITE: {
			for (i = 0; i < muti_aec_i2c->msize; i++) {
				reg_bit.reg_addr = msettings[i].reg_addr;
				reg_bit.reg_value = msettings[i].reg_value;
				reg_bit.reg_bits = addr_bits_type[0] | data_bits_type[0];
				ret = sprd_sensor_write_reg(sensor_id[0], &reg_bit);
				if (ret) {
					pr_err("fail to write m reg, single\n");
					goto exit;
				}
			}
			break;
		}

		case SPRD_SENSOR_I2C_BURST_REG16_VAL8: {
			ret = sprd_sensor_burst_write_reg16_val8(msettings, sensor_id[0], muti_aec_i2c->msize, 0);
			if (ret) {
				pr_err("fail to write m reg, burst_reg16_val8\n");
				goto exit;
			}
			break;
		}

		case SPRD_SENSOR_I2C_BURST_REG16_VAL16: {
			ret = sprd_sensor_burst_write_reg16_val16(msettings, sensor_id[0], muti_aec_i2c->msize, 0);
			if (ret) {
				pr_err("fail to write m reg, burst_reg16_val16\n");
				goto exit;
			}
			break;
		}

		default: {
			for (i = 0; i < muti_aec_i2c->msize; i++) {
				reg_bit.reg_addr = msettings[i].reg_addr;
				reg_bit.reg_value = msettings[i].reg_value;
				reg_bit.reg_bits = addr_bits_type[0] | data_bits_type[0];
				ret = sprd_sensor_write_reg(sensor_id[0], &reg_bit);
				if (ret) {
					pr_err("fail to write m reg, default\n");
					goto exit;
				}
			}
			break;
		}
		}
	id0_end = ktime_get_boottime();
	muti_aec_i2c->master_end_time = id0_end;
	}

	/* slave aec info set to i2c */
	if (muti_aec_i2c->ssize > 0) {

		p_dev = sprd_sensor_get_dev_context(sensor_id[1]);
		if (!p_dev) {
			pr_err("%s, error\n", __func__);
			return -EINVAL;
		}

		switch (p_dev->i2c_burst) {
		case SPRD_SENSOR_I2C_SINGLE_WRITE: {
			for (i = 0; i < muti_aec_i2c->ssize; i++) {
				reg_bit.reg_addr = ssettings[i].reg_addr;
				reg_bit.reg_value = ssettings[i].reg_value;
				reg_bit.reg_bits = addr_bits_type[1] | data_bits_type[1];
				ret = sprd_sensor_write_reg(sensor_id[1], &reg_bit);
				if (ret) {
					pr_err("fail to write s reg, single\n");
					goto exit;
				}
			}
			break;
		}

		case SPRD_SENSOR_I2C_BURST_REG16_VAL8: {
			ret = sprd_sensor_burst_write_reg16_val8(ssettings, sensor_id[1], muti_aec_i2c->ssize, 0);
			if (ret) {
				pr_err("fail to write s reg, burst_reg16_val8\n");
				goto exit;
			}
			break;
		}

		case SPRD_SENSOR_I2C_BURST_REG16_VAL16: {
			ret = sprd_sensor_burst_write_reg16_val16(ssettings, sensor_id[1], muti_aec_i2c->ssize, 0);
			if (ret) {
				pr_err("fail to write s reg, burst_reg16_val16\n");
				goto exit;
			}
			break;
		}

		default: {
			for (i = 0; i < muti_aec_i2c->ssize; i++) {
				reg_bit.reg_addr = ssettings[i].reg_addr;
				reg_bit.reg_value = ssettings[i].reg_value;
				reg_bit.reg_bits = addr_bits_type[1] | data_bits_type[1];
				ret = sprd_sensor_write_reg(sensor_id[1], &reg_bit);
				if (ret) {
					pr_err("fail to write s reg, default\n");
					goto exit;
				}
			}
			break;
		}
		}

	id1_end = ktime_get_boottime();
	muti_aec_i2c->slave_end_time = id1_end;
	}

	/* slave_2 aec info set to i2c */
	if (muti_aec_i2c->ssize_2 > 0) {

		p_dev = sprd_sensor_get_dev_context(sensor_id[2]);
		if (!p_dev) {
			pr_err("%s, error\n", __func__);
			return -EINVAL;
		}

		switch (p_dev->i2c_burst) {
		case SPRD_SENSOR_I2C_SINGLE_WRITE: {
			for (i = 0; i < muti_aec_i2c->ssize_2; i++) {
				reg_bit.reg_addr = ssettings_2[i].reg_addr;
				reg_bit.reg_value = ssettings_2[i].reg_value;
				reg_bit.reg_bits = addr_bits_type[2] | data_bits_type[2];
				ret = sprd_sensor_write_reg(sensor_id[2], &reg_bit);
				if (ret) {
					pr_err("fail to write s_2 reg, single\n");
					goto exit;
				}
			}
			break;
		}

		case SPRD_SENSOR_I2C_BURST_REG16_VAL8: {
			ret = sprd_sensor_burst_write_reg16_val8(ssettings_2, sensor_id[2], muti_aec_i2c->ssize_2, 0);
			if (ret) {
				pr_err("fail to write s_2 reg, burst_reg16_val8\n");
				goto exit;
			}
			break;
		}

		case SPRD_SENSOR_I2C_BURST_REG16_VAL16: {
			ret = sprd_sensor_burst_write_reg16_val16(ssettings_2, sensor_id[2], muti_aec_i2c->ssize_2, 0);
			if (ret) {
				pr_err("fail to write s_2 reg, burst_reg16_val16\n");
				goto exit;
			}
			break;
		}

		default: {
			for (i = 0; i < muti_aec_i2c->ssize_2; i++) {
				reg_bit.reg_addr = ssettings_2[i].reg_addr;
				reg_bit.reg_value = ssettings_2[i].reg_value;
				reg_bit.reg_bits = addr_bits_type[2] | data_bits_type[2];
				ret = sprd_sensor_write_reg(sensor_id[2], &reg_bit);
				if (ret) {
					pr_err("fail to write s_2 reg, default\n");
					goto exit;
				}
			}
			break;
		}
		}

	id2_end = ktime_get_boottime();
	muti_aec_i2c->slave2_end_time = id2_end;
	}

exit:
	if (ret != 0)
		return -EFAULT;
	else
		return 0;
}

int sprd_sensor_read_i2c(struct sensor_i2c_tag *i2c_tab,
			int sensor_id)
{
	struct i2c_msg msg_r[2];
	int i = 0;
	uint8_t cmd[2] = { 0 };
	uint8_t buf_r[64] = { 0 };
	uint8_t *p_buf_r = NULL;
	struct sprd_sensor_mem_tag p_mem = {0};
	uint32_t cnt = i2c_tab->i2c_count;
	int ret = -1;
	uint16_t read_num = i2c_tab->read_len;
	struct i2c_client *i2c_client;
	int i2c_cnt = 0;
	struct sprd_sensor_dev_info_tag *p_dev = NULL;

	p_dev = sprd_sensor_get_dev_context(sensor_id);
	if (!p_dev) {
		pr_err("%s, error\n", __func__);
		return -EINVAL;
	}
	ret = copy_from_user(cmd, i2c_tab->i2c_data, cnt);
	if (ret) {
		pr_err("sensor W I2C ERR: copy user fail, size %d\n", cnt);
		goto exit;
	}
	if (read_num > sizeof(buf_r)) {
		ret = sprd_sensor_malloc(&p_mem, read_num);
		if (ret) {
			pr_err("SENSOR: read block data alloc fail %d\n", ret);
			goto exit;
		}
		p_buf_r = (uint8_t *)p_mem.buf_ptr;
	} else {
		p_buf_r = buf_r;
	}

	for (i = 0; i < SPRD_SENSOR_I2C_OP_TRY_NUM; i++) {
		msg_r[0].addr = i2c_tab->slave_addr;
		if (1000000 == p_dev->i2c_clock)
			msg_r[0].flags = 0 | I2C_1M_FLAG_CAM;
		else
			msg_r[0].flags = 0 | I2C_400K_FLAG_CAM;
        msg_r[0].buf = cmd;
		msg_r[0].len = cnt;
		msg_r[1].addr = i2c_tab->slave_addr;
		if (1000000 == p_dev->i2c_clock)
			msg_r[1].flags = I2C_M_RD | I2C_1M_FLAG_CAM;
		else
			msg_r[1].flags = I2C_M_RD | I2C_400K_FLAG_CAM;
        msg_r[1].buf = p_buf_r;
		msg_r[1].len = read_num;

		if (p_dev->vcm_i2c_client == NULL)
			i2c_client = p_dev->i2c_info;
		else
			i2c_client = p_dev->vcm_i2c_client;
		if (IS_ERR_OR_NULL(i2c_client)) {
			pr_err("%s error %d\n", __func__, __LINE__);
			ret = -1;
			goto exit;
		}
		i2c_cnt = i2c_transfer(i2c_client->adapter, msg_r, 2);
		if (i2c_cnt != SPRD_SENSOR_I2C_READ_SUCCESS_CNT) {
			pr_err("SENSOR:read reg fail, ret %d, addr 0x%x\n",
				ret, i2c_client->addr);
			SLEEP_MS(20);
		} else {
			ret = copy_to_user(i2c_tab->i2c_data,
					p_buf_r, read_num);
			if (ret) {
				pr_err
				("sensor W I2C ERR:\n"
					"copy user fail, size %d\n",
					cnt);
				goto exit;
			}
			break;
		}
	}

exit:
	if (p_mem.buf_ptr)
		sprd_sensor_free(&p_mem);
	pr_debug("%s ret %d\n", __func__, ret);
	return ret;
}

int sprd_sensor_find_dcam_id(int sensor_id)
{
	int i;

	for (i = 0; i < SPRD_SENSOR_ID_MAX; i++) {
		if (s_sensor_dev_data[i] &&
		    (sensor_id == s_sensor_dev_data[i]->sensor_id) &&
		    s_sensor_dev_data[i]->attch_dcam_id != -1) {
			pr_debug("find sensor %d attached dcam id %d\n",
				 sensor_id,
				 s_sensor_dev_data[i]->attch_dcam_id);
			return s_sensor_dev_data[i]->attch_dcam_id;
		}
	}
	pr_info("find sensor %d attached dcam id fail!\n", sensor_id);

	return -1;
}
EXPORT_SYMBOL(sprd_sensor_find_dcam_id);
