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

static struct vote_data sharkl5pro_vote_data[] = {
{"boost", 1333, 0},
{"lcdon", 768, 0},
{"lcdoff", 256, 0},
{"camlow", 384, 0},
{"camhigh", 512, 0},
{"camveryhigh", 0xbacd, 0},
{"faceid", 1333, 0},
{"top", 0xbacd, 0},
{NULL, 0, 0}
};

static struct vote_data *g_vote_data;
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
	point->flag = 1;
	spin_unlock(&lock);
}

static void reset_flag(struct vote_data *point)
{
	spin_lock(&lock);
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
	int err;

	point = g_vote_data;
	while (point->name != NULL) {
		if ((point->flag == 1) && (point->freq > target_freq))
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

	point = find_point(name);
	if (point == NULL)
		return -EINVAL;

	set_flag(point);

	return check_and_vote();
}

static int dvfs_unvote(const char *name)
{
	struct vote_data *point;

	point = find_point(name);
	if (point == NULL)
		return -EINVAL;

	reset_flag(point);

	return check_and_vote();
}

static int dvfs_set_point(const char *name, unsigned int freq)
{
	struct vote_data *point;

	point = find_point(name);
	if (point == NULL)
		return -EINVAL;

	reset_freq(point, freq);

	return check_and_vote();
}

static int dvfs_get_point_info(char **name, unsigned int *freq,
		unsigned int *flag, int index)
{
	struct vote_data *point = &g_vote_data[index];

	if (point->name == NULL)
		return -ENOENT;

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
	int err;

	g_vote_data = (struct vote_data *)of_device_get_match_data(&pdev->dev);
	if (!g_vote_data) {
		dev_err(&pdev->dev, "No matching driver data found\n");
		return -EINVAL;
	}

	spin_lock_init(&lock);

	err = dvfs_core_init(pdev);
	if (err < 0)
		return err;

	dvfs_core_hw_callback_register(&callbacks);
	return 0;
}

static int dvfs_remove(struct platform_device *pdev)
{
	dvfs_core_hw_callback_clear(&callbacks);
	dvfs_core_clear(pdev);
	return 0;
}

static const struct of_device_id dvfs_match[] = {
	{ .compatible = "sprd,sharkl5pro-ddr-dvfs", .data = &sharkl5pro_vote_data },
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
