// SPDX-License-Identifier: GPL-2.0
//
// Spreadtrum ddr dvfs driver
//
// Copyright (C) 2015~2020 Spreadtrum, Inc.
// Author: Mingmin Ling <mingmin.ling@unisoc.com>

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include "sprd_ddr_dvfs.h"


struct vote_data {
	const char *name;
	unsigned int freq;
	unsigned int flag;
};

static struct vote_data *g_vote_data;
static int scene_num;
spinlock_t lock;

static struct vote_data *find_point(const char *name)
{
	struct vote_data *point;

	point = g_vote_data;
	while (point->name != NULL) {
		if (!strcmp(point->name, name))
			return point;
		point++;
	}
	return NULL;
}

static void set_flag(struct vote_data *point)
{
	spin_lock(&lock);
	point->flag++;
	spin_unlock(&lock);
}

static void reset_flag(struct vote_data *point)
{
	spin_lock(&lock);
	if (point->flag > 0)
		point->flag--;
	else
		point->flag = 0;
	spin_unlock(&lock);
}

static void reset_freq(struct vote_data *point, unsigned int freq)
{
	spin_lock(&lock);
	point->freq = freq;
	spin_unlock(&lock);
}

static int check_and_vote(void)
{
	static unsigned int last_freq;
	unsigned int target_freq = 0;
	struct vote_data *point;
	int err, i;

	point = g_vote_data;
	for (i = 0; i < scene_num; i++) {
		if ((point->flag >= 1) && (point->freq > target_freq))
			target_freq = point->freq;
		point++;
	}

	if (target_freq != last_freq) {
		if (unlikely(target_freq == 0xbacd)) {
			err = force_top_freq();
			if (err)
				return err;
		} else {
			if (unlikely(last_freq == 0xbacd)) {
				err = dvfs_auto_enable();
				if (err)
					return err;
			}
			err = send_vote_request(target_freq);
			if (err)
				return err;

		}
		last_freq = target_freq;
	}
	return 0;
}

static int dvfs_vote(const char *name)
{
	struct vote_data *point;

	if (g_vote_data == NULL)
		return -EINVAL;
	point = find_point(name);
	if (point == NULL)
		return -EINVAL;

	set_flag(point);

	return check_and_vote();
}

static int dvfs_unvote(const char *name)
{
	struct vote_data *point;

	if (g_vote_data == NULL)
		return -EINVAL;
	point = find_point(name);
	if (point == NULL)
		return -EINVAL;

	reset_flag(point);

	return check_and_vote();
}

static int dvfs_set_point(const char *name, unsigned int freq)
{
	struct vote_data *point;

	if (g_vote_data == NULL)
		return -EINVAL;
	point = find_point(name);
	if (point == NULL)
		return -EINVAL;

	reset_freq(point, freq);

	return check_and_vote();
}

static int dvfs_get_point_info(char **name, unsigned int *freq, unsigned int *flag, int index)
{
	struct vote_data *point;

	if (g_vote_data == NULL || index >= scene_num)
		return -EINVAL;

	point = &g_vote_data[index];
	if (point->name == NULL)
		return -EINVAL;

	*name = (char *)point->name;
	*freq = point->freq;
	*flag = point->flag;
	return 0;
}

static struct dvfs_hw_callback callbacks = {
	.hw_dvfs_vote = dvfs_vote,
	.hw_dvfs_unvote = dvfs_unvote,
	.hw_dvfs_set_point = dvfs_set_point,
	.hw_dvfs_get_point_info = dvfs_get_point_info,
};

static int dvfs_probe(struct platform_device *pdev)
{
	int err, i;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;

	scene_num = of_property_count_strings(dev->of_node, "sprd-scene");
	if (scene_num <= 0) {
		dev_warn(dev, "failed read scene_num\n");
		scene_num = 0;
	} else {
		g_vote_data = devm_kzalloc(dev, sizeof(struct vote_data)*(unsigned int)scene_num,
					   GFP_KERNEL);
		if (g_vote_data == NULL) {
			err = -ENOMEM;
			return err;
		}
	}
	for (i = 0; i < scene_num; i++) {
		err = of_property_read_string_index(node, "sprd-scene", i,
						    (const char **)&g_vote_data[i].name);
		if (err < 0 || strlen(g_vote_data[i].name) == 0) {
			dev_err(dev, "failed parse sprd-scene\n");
			goto free_mem;
		}
		err = of_property_read_u32_index(node, "sprd-freq", i,
						 &g_vote_data[i].freq);
		if (err < 0) {
			dev_err(dev, "failed parse sprd-freq\n");
			goto free_mem;
		}
	}
	spin_lock_init(&lock);

	err = dvfs_core_init(pdev);
	if (err < 0)
		goto free_mem;

	dvfs_core_hw_callback_register(&callbacks);
	return 0;

free_mem:
	if (g_vote_data != NULL)
		devm_kfree(&pdev->dev, g_vote_data);
	return err;
}

static int dvfs_remove(struct platform_device *pdev)
{
	if (g_vote_data != NULL)
		devm_kfree(&pdev->dev, g_vote_data);
	dvfs_core_hw_callback_clear(&callbacks);
	dvfs_core_clear(pdev);
	return 0;
}

static const struct of_device_id dvfs_match[] = {
	{ .compatible = "sprd,ddr-dvfs"},
	{},
};
MODULE_DEVICE_TABLE(of, dvfs_match);

static struct platform_driver sprd_ddr_dvfs_drvier = {
	.probe = dvfs_probe,
	.remove = dvfs_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd-ddr-dvfs",
#ifdef CONFIG_OF
		.of_match_table = dvfs_match,
#endif
	},
};
module_platform_driver(sprd_ddr_dvfs_drvier);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("dvfs sp hardware");
