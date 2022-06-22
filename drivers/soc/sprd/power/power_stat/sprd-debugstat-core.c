// SPDX-License-Identifier: GPL-2.0
//
// UNISOC APCPU POWER STAT driver
//
// Copyright (C) 2020 Unisoc, Inc.

#include <linux/cpu_pm.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/regmap.h>
#include <linux/suspend.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/timekeeping.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/alarmtimer.h>
#include <linux/mfd/syscon.h>
#include "sprd-debugstat.h"

#define INFO_STR_LEN		128

struct info_node {
	char *name;
	struct subsys_sleep_info *info;
	get_info_t *get;
	void *data;
	struct list_head list;
};

LIST_HEAD(info_node_list);
DEFINE_MUTEX(info_mutex);

static int stat_print_name(struct seq_file *m)
{
	char str[INFO_STR_LEN];
	struct info_node *pos;
	int num;

	num = sprintf(str, "%22s:", "subsystem_name");
	list_for_each_entry(pos, &info_node_list, list)
		num += sprintf(str + num, "%10s", pos->info->subsystem_name);
	seq_printf(m, "%s\n", str);

	return 0;
}

static int stat_print_total(struct seq_file *m)
{
	char str[INFO_STR_LEN];
	struct info_node *pos;
	int num;

	num = sprintf(str, "%22s:", "total_duration");
	list_for_each_entry(pos, &info_node_list, list)
		num += sprintf(str + num, "%10llu", pos->info->total_duration);
	seq_printf(m, "%s\n", str);

	return 0;
}

static int stat_print_sleep(struct seq_file *m)
{
	char str[INFO_STR_LEN];
	struct info_node *pos;
	int num;

	num = sprintf(str, "%22s:", "sleep_duration_total");
	list_for_each_entry(pos, &info_node_list, list)
		num += sprintf(str + num, "%10llu", pos->info->sleep_duration_total);
	seq_printf(m, "%s\n", str);

	return 0;
}

static int stat_print_idle(struct seq_file *m)
{
	char str[INFO_STR_LEN];
	struct info_node *pos;
	int num;

	num = sprintf(str, "%22s:", "idle_duration_total");
	list_for_each_entry(pos, &info_node_list, list)
		num += sprintf(str + num, "%10llu", pos->info->idle_duration_total);
	seq_printf(m, "%s\n", str);

	return 0;
}

static int stat_print_state(struct seq_file *m)
{
	char str[INFO_STR_LEN];
	struct info_node *pos;
	int num;

	num = sprintf(str, "%22s:", "current_status");
	list_for_each_entry(pos, &info_node_list, list)
		num += sprintf(str + num, "%10u", pos->info->current_status);
	seq_printf(m, "%s\n", str);

	return 0;
}

static int stat_print_reboot(struct seq_file *m)
{
	char str[INFO_STR_LEN];
	struct info_node *pos;
	int num;

	num = sprintf(str, "%22s:", "subsystem_reboot_count");
	list_for_each_entry(pos, &info_node_list, list)
		num += sprintf(str + num, "%10u", pos->info->subsystem_reboot_count);
	seq_printf(m, "%s\n", str);

	return 0;
}

static int stat_print_wakeup_reason(struct seq_file *m)
{
	char str[INFO_STR_LEN];
	struct info_node *pos;
	int num;

	num = sprintf(str, "%22s:", "wakeup_reason");
	list_for_each_entry(pos, &info_node_list, list)
		num += sprintf(str + num, "%10u", pos->info->wakeup_reason);
	seq_printf(m, "%s\n", str);

	return 0;
}

static int stat_print_last_wakeup(struct seq_file *m)
{
	char str[INFO_STR_LEN];
	struct info_node *pos;
	int num;

	num = sprintf(str, "%22s:", "last_wakeup_duration");
	list_for_each_entry(pos, &info_node_list, list)
		num += sprintf(str + num, "%10u", pos->info->last_wakeup_duration);
	seq_printf(m, "%s\n", str);

	return 0;
}

static int stat_print_last_sleep(struct seq_file *m)
{
	char str[INFO_STR_LEN];
	struct info_node *pos;
	int num;

	num = sprintf(str, "%22s:", "last_sleep_duration");
	list_for_each_entry(pos, &info_node_list, list)
		num += sprintf(str + num, "%10u", pos->info->last_sleep_duration);
	seq_printf(m, "%s\n", str);

	return 0;
}

static int stat_print_active_core(struct seq_file *m)
{
	char str[INFO_STR_LEN];
	struct info_node *pos;
	int num;

	num = sprintf(str, "%22s:", "active_core");
	list_for_each_entry(pos, &info_node_list, list)
		num += sprintf(str + num, "%#10x", pos->info->active_core);
	seq_printf(m, "%s\n", str);

	return 0;
}

static int stat_print_irq_count(struct seq_file *m)
{
	char str[INFO_STR_LEN];
	struct info_node *pos;
	int num;

	num = sprintf(str, "%22s:", "internal_irq_count");
	list_for_each_entry(pos, &info_node_list, list)
		num += sprintf(str + num, "%10u", pos->info->internal_irq_count);
	seq_printf(m, "%s\n", str);

	return 0;
}

static int stat_print_irq_to_ap(struct seq_file *m)
{
	char str[INFO_STR_LEN];
	struct info_node *pos;
	int num;

	num = sprintf(str, "%22s:", "irq_to_ap_count");
	list_for_each_entry(pos, &info_node_list, list)
		num += sprintf(str + num, "%10u", pos->info->irq_to_ap_count);
	seq_printf(m, "%s\n", str);

	return 0;
}

/* Information print interface */
static int (*stat_print[])(struct seq_file *m) = {
	stat_print_name, stat_print_total, stat_print_sleep,
	stat_print_idle, stat_print_state, stat_print_reboot,
	stat_print_wakeup_reason, stat_print_last_wakeup,
	stat_print_last_sleep, stat_print_active_core,
	stat_print_irq_count, stat_print_irq_to_ap,
	NULL
};

static int stat_read(struct seq_file *m, void *v)
{
	struct info_node *pos;
	struct subsys_sleep_info *info_src;
	int i;

	seq_printf(m, "Subsys information:\n");

	list_for_each_entry(pos, &info_node_list, list) {
		info_src = pos->get(pos->data);
		if (info_src != NULL)
			*(pos->info) = *info_src;
	}

	for (i = 0; stat_print[i]; ++i)
		(stat_print[i])(m);

	return 0;
}

static int stat_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, stat_read, NULL);
}

static const struct file_operations stat_fops = {
	.owner		= THIS_MODULE,
	.open		= stat_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int slp_cnt_read(struct seq_file *m, void *v)
{
	struct info_node *pos;
	int slp_cnt = -1;

	list_for_each_entry(pos, &info_node_list, list) {
		if (strcmp(pos->name, "soc_sys"))
			continue;

		pos->info = pos->get(pos->data);
		if (!pos->info) {
			pr_err("%s: Get %s error\n", __func__, pos->name);
			return -ENXIO;
		}

		if (pos->info->slp_cnt == -1)
			return -EINVAL;

		slp_cnt = (int)pos->info->slp_cnt;
		break;
	}

	seq_printf(m, "%d\n", slp_cnt);

	return 0;
}

static int slp_cnt_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, slp_cnt_read, NULL);
}

static const struct file_operations slp_cnt_fops = {
	.owner		= THIS_MODULE,
	.open		= slp_cnt_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int stat_info_register(char *name, get_info_t *get, void *data)
{
	struct info_node *pos;
	struct info_node *pn;

	if (!name || !get) {
		pr_err("%s: Parameter is error\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&info_mutex);

	list_for_each_entry(pos, &info_node_list, list) {
		if (strcmp(pos->name, name))
			continue;
		pr_warn("%s: The node(%s) is exsit\n", __func__, name);
		mutex_unlock(&info_mutex);
		return 0;
	}

	pn = kzalloc(sizeof(struct info_node), GFP_KERNEL);
	if (!pn) {
		pr_err("%s: Node allock error\n", __func__);
		mutex_unlock(&info_mutex);
		return -ENOMEM;
	}

	pn->info = kzalloc(sizeof(struct subsys_sleep_info), GFP_KERNEL);
	if (!pn->info) {
		kfree(pn);
		mutex_unlock(&info_mutex);
		return -ENOMEM;
	}

	pn->name = name;
	pn->get = get;
	pn->data = data;

	list_add_tail(&pn->list, &info_node_list);

	mutex_unlock(&info_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(stat_info_register);

int stat_info_unregister(char *name)
{
	struct info_node *pos;

	if (!name) {
		pr_err("%s: Parameter is error\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&info_mutex);

	list_for_each_entry(pos, &info_node_list, list) {
		if (strcmp(pos->name, name))
			continue;
		list_del(&pos->list);
		kfree(pos->info);
		kfree(pos);
		mutex_unlock(&info_mutex);
		return 0;
	}

	mutex_unlock(&info_mutex);

	pr_err("%s: The node(%s) is not found\n", __func__, name);

	return 0;
}
EXPORT_SYMBOL_GPL(stat_info_unregister);

/**
 * sprd_debugstat_init - add the debug stat
 */
int sprd_debugstat_core_init(void)
{
	struct proc_dir_entry *dir;
	struct proc_dir_entry *fle;

	dir = proc_mkdir("sprd-debugstat", NULL);
	if (!dir) {
		pr_err("%s: Proc dir create failed\n", __func__);
		return -EINVAL;
	}

	fle = proc_create("stat", 0444, dir, &stat_fops);
	if (!fle) {
		pr_err("%s: Proc file create failed\n", __func__);
		remove_proc_entry("sprd-debugstat", NULL);
		return -EINVAL;
	}

	fle = proc_create("slp_cnt", 0644, dir, &slp_cnt_fops);
	if (!fle) {
		pr_err("%s: Proc file slp_cnt create failed\n", __func__);
		remove_proc_entry("sprd-debugstat", NULL);
		return -EINVAL;
	}

	return 0;
}
