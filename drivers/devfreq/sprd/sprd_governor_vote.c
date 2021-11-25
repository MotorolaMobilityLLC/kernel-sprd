// SPDX-License-Identifier: GPL-2.0
//
// Spreadtrum ddr dvfs driver
//
// Copyright (C) 2015~2020 Spreadtrum, Inc.
// Author: Mingmin Ling <mingmin.ling@unisoc.com>

#include <linux/ctype.h>
#include <linux/devfreq.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "sprd_ddr_dvfs.h"

static unsigned int force_freq;
static int backdoor_status;
static struct devfreq *gov_devfreq;

static ssize_t scaling_force_ddr_freq_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	ssize_t count = 0;

	count = sprintf(buf, "%u\n", force_freq);
	return count;
}

static ssize_t scaling_force_ddr_freq_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int err;
	struct devfreq *devfreq = to_devfreq(dev);

	err = sscanf(buf, "%u\n", &force_freq);
	if (err < 1) {
		dev_warn(dev->parent, "get scaling force ddr freq err: %d", err);
		return count;
	}
	mutex_lock(&devfreq->lock);
	err = update_devfreq(devfreq);
	mutex_unlock(&devfreq->lock);
	if (err)
		dev_err(dev->parent, "force freq %u fail: %d", force_freq, err);
	return count;
}
static DEVICE_ATTR_RW(scaling_force_ddr_freq);

static ssize_t scaling_overflow_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned int i, freq_num;
	unsigned int data;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;

	err = gov_callback->get_freq_num(&freq_num);
	if (err < 0)
		freq_num = 0;
	for (i = 0; i < freq_num; i++) {
		err = gov_callback->get_overflow(&data, i);
		if (err < 0) {
			data = 0;
			dev_err(dev->parent, "get sel[%u] overflow fail: %d", i, err);
		}
		count += sprintf(&buf[count], "%u ", data);
	}
	count += sprintf(&buf[count], "\n");
	return count;
}

static ssize_t scaling_overflow_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int sel;
	unsigned int overflow;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;

	err = sscanf(buf, "%u %u\n", &sel, &overflow);
	if (err < 2) {
		dev_warn(dev->parent, "overflow para err: %d", err);
		return count;
	}
	err = gov_callback->set_overflow(overflow, sel);
	if (err)
		dev_err(dev->parent, "set sel[%u] overflow %u fail: %d", sel, overflow, err);
	return count;
}
static DEVICE_ATTR_RW(scaling_overflow);

static ssize_t scaling_underflow_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned int i, freq_num;
	unsigned int data;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;

	err = gov_callback->get_freq_num(&freq_num);
	if (err < 0)
		freq_num = 0;
	for (i = 0; i < freq_num; i++) {
		err = gov_callback->get_underflow(&data, i);
		if (err < 0) {
			data = 0;
			dev_err(dev->parent, "get sel[%u] underflow fail: %d", i, err);
		}
		count += sprintf(&buf[count], "%u ", data);
	}
	count += sprintf(&buf[count], "\n");
	return count;
}

static ssize_t scaling_underflow_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int sel;
	unsigned int underflow;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;

	err = sscanf(buf, "%u %u\n", &sel, &underflow);
	if (err < 2) {
		dev_warn(dev->parent, "underflow para err: %d", err);
		return count;
	}
	err = gov_callback->set_underflow(underflow, sel);
	if (err)
		dev_err(dev->parent, "set sel[%u] underflow %u fail: %d", sel, underflow, err);
	return count;
}
static DEVICE_ATTR_RW(scaling_underflow);

static ssize_t dfs_on_off_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned int data;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;

	err = gov_callback->get_dvfs_status(&data);
	if (err < 0) {
		data = 0;
		dev_err(dev->parent, "get ddr dfs status fail: %d ", err);
	}
	count = sprintf(buf, "%u\n", data);
	return count;
}

static ssize_t dfs_on_off_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int enable;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;

	err = sscanf(buf, "%u\n", &enable);
	if (err < 1) {
		dev_warn(dev->parent, "enable para err: %d", err);
		return count;
	}
	if (enable == 1)
		err = gov_callback->dvfs_enable();
	else if (enable == 0)
		err = gov_callback->dvfs_disable();
	else
		err = -EINVAL;
	if (err)
		dev_err(dev->parent, "ddr dfs enable[%u] fail: %d", enable, err);
	return count;
}
static DEVICE_ATTR_RW(dfs_on_off);

static ssize_t auto_dfs_on_off_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned int data;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;

	err = gov_callback->get_dvfs_auto_status(&data);
	if (err < 0) {
		data = 0;
		dev_err(dev->parent, "get ddr auto dfs status fail: %d", err);
	}
	count = sprintf(buf, "%u\n", data);
	return count;
}

static ssize_t auto_dfs_on_off_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int enable;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;

	err = sscanf(buf, "%u\n", &enable);
	if (err < 1) {
		dev_warn(dev->parent, "get auto dfs enable para err: %d", err);
		return count;
	}
	if (enable == 1)
		err = gov_callback->dvfs_auto_enable();
	else if (enable == 0)
		err = gov_callback->dvfs_auto_disable();
	else
		err = -EINVAL;
	if (err)
		dev_err(dev->parent, "ddr auto dfs enable[%u] fail: %d", enable, err);
	return count;
}
static DEVICE_ATTR_RW(auto_dfs_on_off);

static ssize_t ddrinfo_cur_freq_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned int data;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;

	err = gov_callback->get_cur_freq(&data);
	if (err < 0) {
		data = 0;
		dev_err(dev->parent, "get ddr cur freq fail: %d", err);
	}
	count = sprintf(buf, "%u\n", data);
	return count;
}
static DEVICE_ATTR_RO(ddrinfo_cur_freq);

static ssize_t ddrinfo_freq_table_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned int i, freq_num;
	unsigned int data;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;

	err = gov_callback->get_freq_num(&freq_num);
	if (err < 0)
		freq_num = 0;
	for (i = 0; i < freq_num; i++) {
		err = gov_callback->get_freq_table(&data, i);
		if (!err && (data > 0))
			count += sprintf(&buf[count], "%u ", data);
	}
	count += sprintf(&buf[count], "\n");
	return count;
}
static DEVICE_ATTR_RO(ddrinfo_freq_table);

static ssize_t scenario_dfs_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int err;
	unsigned int name_len;
	char *arg;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;

	arg = (char *)buf;
	while (*arg && !isspace(*arg))
		arg++;
	name_len = arg-buf;
	if (!name_len)
		return -EINVAL;
	arg = kzalloc(name_len * sizeof(char) + 1, GFP_KERNEL);
	if (arg == NULL)
		return -EINVAL;
	memcpy(arg, buf, name_len);
	err = gov_callback->governor_vote(arg);
	if (err)
		dev_err(dev->parent, "scene %s enter fail: %d", arg, err);
	kfree(arg);
	return count;
}
static DEVICE_ATTR_WO(scenario_dfs);

static ssize_t exit_scene_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int err;
	unsigned int name_len;
	char *arg;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;

	arg = (char *)buf;
	while (*arg && !isspace(*arg))
		arg++;
	name_len = arg-buf;
	if (!name_len)
		return -EINVAL;
	arg = kzalloc(name_len * sizeof(char) + 1, GFP_KERNEL);
	if (arg == NULL)
		return -EINVAL;
	memcpy(arg, buf, name_len);
	err = gov_callback->governor_unvote(arg);
	if (err)
		dev_err(dev->parent, "scene %s exit fail: %d", arg, err);
	kfree(arg);
	return count;
}
static DEVICE_ATTR_WO(exit_scene);

static ssize_t scene_freq_set_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int err;
	unsigned int name_len;
	char *arg;
	unsigned int freq;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;

	arg = (char *)buf;
	while (*arg && !isspace(*arg))
		arg++;
	name_len = arg-buf;
	if (!name_len)
		return -EINVAL;
	arg = kzalloc(name_len * sizeof(char) + 1, GFP_KERNEL);
	if (arg == NULL)
		return -EINVAL;
	memcpy(arg, buf, name_len);

	err = sscanf(&buf[name_len], "%u\n", &freq);
	if (err < 1) {
		dev_warn(dev->parent, "get set freq fail: %d", err);
		kfree(arg);
		return count;
	}

	err = gov_callback->governor_change_point(arg, freq);
	if (err)
		dev_err(dev->parent, "scene %s change freq %u fail: %d", arg, freq, err);
	kfree(arg);
	return count;
}
static DEVICE_ATTR_WO(scene_freq_set);

static ssize_t scene_boost_dfs_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int enable;
	unsigned int freq;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;

	err = sscanf(buf, "%u %u\n", &enable, &freq);
	if (err < 2) {
		dev_warn(dev->parent, "get boost para err: %d", err);
		return count;
	}
	if (enable == 1)
		err = gov_callback->governor_vote("boost");
	else if (enable == 0)
		err = gov_callback->governor_unvote("boost");
	else
		err = -EINVAL;
	if (err)
		dev_err(dev->parent, "scene boost enter[%u] fail: %d", enable, err);
	return count;
}
static DEVICE_ATTR_WO(scene_boost_dfs);

static ssize_t backdoor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", backdoor_status);
}


static ssize_t backdoor_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int err;
	int backdoor;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;

	err = sscanf(buf, "%d\n", &backdoor);
	if (err < 1) {
		dev_warn(dev->parent, "set backdoor err: %d", err);
		return count;
	}

	if (backdoor_status == backdoor)
		return count;

	if (backdoor == 1)
		err = gov_callback->governor_vote("top");
	else if (backdoor == 0)
		err = gov_callback->governor_unvote("top");
	else
		err = -EINVAL;

	if (err)
		dev_err(dev->parent, "set backdoor %d fail: %d", backdoor, err);
	else
		backdoor_status  = backdoor;

	return count;
}
static DEVICE_ATTR_RW(backdoor);

static ssize_t scene_dfs_list_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	char *name;
	unsigned int freq;
	unsigned int flag;
	int i = 0;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;

	int err;

	do {
		err =  gov_callback->get_point_info(&name, &freq, &flag, i);
		if (err == 0)
			count += sprintf(&buf[count],
				"%s freq %u  flag %u\n",
				name, freq, flag);
		i++;
	} while (!err);
	return count;
}
static DEVICE_ATTR_RO(scene_dfs_list);

static struct attribute *dev_entries[] = {
	&dev_attr_scaling_force_ddr_freq.attr,
	&dev_attr_scaling_overflow.attr,
	&dev_attr_scaling_underflow.attr,
	&dev_attr_dfs_on_off.attr,
	&dev_attr_auto_dfs_on_off.attr,
	&dev_attr_ddrinfo_cur_freq.attr,
	&dev_attr_ddrinfo_freq_table.attr,
	&dev_attr_scenario_dfs.attr,
	&dev_attr_exit_scene.attr,
	&dev_attr_scene_freq_set.attr,
	&dev_attr_scene_boost_dfs.attr,
	&dev_attr_scene_dfs_list.attr,
	&dev_attr_backdoor.attr,
	NULL,
};

static struct attribute_group gov_vote_attrs = {
	.name   = "sprd-governor",
	.attrs  = dev_entries,
};

int scene_dfs_request(char *scenario)
{
	int err;
	struct governor_callback *gov_callback;
	struct devfreq *devfreq = gov_devfreq;

	if (!devfreq)
		return -ENODEV;

	gov_callback = (struct governor_callback *)devfreq->last_status.private_data;

	err = gov_callback->governor_vote(scenario);
	if (err)
		dev_err(devfreq->dev.parent, "scene %s enter fail: %d", scenario, err);

	return err;
}
EXPORT_SYMBOL(scene_dfs_request);

int scene_exit(char *scenario)
{
	int err;
	struct governor_callback *gov_callback;
	struct devfreq *devfreq = gov_devfreq;

	if (!devfreq)
		return -ENODEV;

	gov_callback = (struct governor_callback *)devfreq->last_status.private_data;

	err = gov_callback->governor_unvote(scenario);
	if (err)
		dev_err(devfreq->dev.parent, "scene %s exit fail: %d", scenario, err);

	return err;
}
EXPORT_SYMBOL(scene_exit);

int change_scene_freq(char *scenario, unsigned int freq)
{
	int err;
	struct governor_callback *gov_callback;
	struct devfreq *devfreq = gov_devfreq;

	if (!devfreq)
		return -ENODEV;

	gov_callback = (struct governor_callback *)devfreq->last_status.private_data;

	err = gov_callback->governor_change_point(scenario, freq);
	if (err)
		dev_err(devfreq->dev.parent, "scene %s change freq %u fail: %d",
			scenario, freq, err);

	return err;
}
EXPORT_SYMBOL(change_scene_freq);

static int gov_vote_start(struct devfreq *devfreq)
{
	int err = 0;

	err = sysfs_create_group(&devfreq->dev.kobj, &gov_vote_attrs);
	if (err) {
		dev_err(devfreq->dev.parent, "dvfs sysfs create fail: %d", err);
		return err;
	}
	err = devfreq_update_stats(devfreq);
	if (err) {
		dev_err(devfreq->dev.parent, "dvfs update states fail: %d", err);
		return err;
	}
	gov_devfreq = devfreq;

	return err;
}

static int gov_vote_stop(struct devfreq *devfreq)
{
	return 0;
}


static int gov_vote_get_target_freq(struct devfreq *devfreq,
	unsigned long *freq)
{
	*freq = force_freq;
	return 0;
}


static int gov_vote_handler(struct devfreq *devfreq,
	unsigned int event, void *data)
{
	switch (event) {
	case DEVFREQ_GOV_START:
		return gov_vote_start(devfreq);
	case DEVFREQ_GOV_STOP:
		return gov_vote_stop(devfreq);
	case DEVFREQ_GOV_INTERVAL:
	case DEVFREQ_GOV_SUSPEND:
	case DEVFREQ_GOV_RESUME:
	default:
		return 0;
	}
}

struct devfreq_governor sprd_vote = {
	.name = "sprd-governor",
	.get_target_freq = gov_vote_get_target_freq,
	.event_handler = gov_vote_handler,
};
