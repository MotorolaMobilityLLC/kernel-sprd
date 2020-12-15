/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
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
#include <linux/cpu_pm.h>
#include <linux/kthread.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/regmap.h>
#include <linux/suspend.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include "sprd_debuglog.h"

#define DEBUGLOG_DIR			"sprd-debuglog"
#define SCAN_ENABLE			"scan-enable"
#define SCAN_INTERVAL			"scan-interval"

static struct task_struct *mon_task;
static struct debug_log *dbg_log;

static struct proc_dir_entry *proc_dir;
static struct mutex dbg_mtx;

/**
 * debug_notifier - Notification call back function
 */
static int debug_notifier(struct notifier_block *s, unsigned long cmd, void *v)
{
	struct debug_event *wkp;
	struct debug_event *slp;
	int ret;

	if (!dbg_log)
		return NOTIFY_STOP;

	if (!dbg_log->sleep || !dbg_log->wakeup) {
		dev_err(dbg_log->dev, "The sleep/wakeup is null.\n");
		return NOTIFY_BAD;
	}

	slp = dbg_log->sleep;
	wkp = dbg_log->wakeup;

	if (!slp->ph || !wkp->ph) {
		dev_err(dbg_log->dev, "The ph is null.\n");
		return NOTIFY_BAD;
	}

	switch (cmd) {
	case CPU_CLUSTER_PM_ENTER:
		ret = slp->ph(dbg_log->dev, slp->data, slp->num);
		if (ret)
			dev_warn(dbg_log->dev, "Condition check error.\n");
		break;
	case CPU_CLUSTER_PM_ENTER_FAILED:
		break;
	case CPU_CLUSTER_PM_EXIT:
		ret = wkp->ph(dbg_log->dev, wkp->data, wkp->num);
		if (ret)
			dev_warn(dbg_log->dev, "Wakeup source check error.\n");
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

/**
 * debug_notifier_block - Power debug notifier block
 */
static struct notifier_block debug_notifier_block = {
	.notifier_call = debug_notifier,
};

/**
 * debug_monitor_scan - the log output thread function
 */
static int debug_monitor_scan(void *data)
{
	struct debug_monitor *mon;
	struct debug_event *evt;
	struct debug_log *dbg;
	int ret;

	dbg = (struct debug_log *)data;
	if (!dbg)
		return -EINVAL;

	mon = dbg->monitor;
	if (!mon) {
		dev_err(dbg->dev, "The monitor is null.\n");
		return -EINVAL;
	}

	evt = &mon->event;

	for (;;) {
		if (kthread_should_stop())
			break;

		ret = evt->ph(dbg->dev, evt->data, evt->num);
		if (ret)
			dev_warn(dbg->dev, "Monitor scan error.\n");

		schedule_timeout(mon->interval * HZ);
		set_current_state(TASK_INTERRUPTIBLE);
	}

	return 0;
}

/**
 * debug_monitor_config - start or stop the log output mechanism
 */
static int debug_monitor_config(struct debug_log *dbg, unsigned int enable)
{
	struct debug_monitor *mon;
	struct device *dev;
	int ret;

	if (!dbg || !dbg->dev || !dbg->monitor)
		return -EINVAL;

	mon = dbg->monitor;
	dev = dbg->dev;

	if (enable) {
		if (mon_task) {
			dev_info(dev, "Debug monitor scan is running.\n");
			return 0;
		}

		mon_task = kthread_create(debug_monitor_scan,
						     dbg, "debug-monitor-scan");
		if (IS_ERR(mon_task)) {
			dev_err(dev, "Unable to start monitor scan task.\n");
			mon->enable = 0;
			return PTR_ERR(mon_task);
		}

		wake_up_process(mon_task);

		mon->enable = 1;
	} else {
		if (!mon_task) {
			dev_info(dev, "Debug monitor scan is not running.\n");
			return 0;
		}

		ret = kthread_stop(mon_task);
		if (ret) {
			dev_err(dev, "Stop task error.\n");
			mon->enable = 1;
			return -EINVAL;
		}

		mon_task = NULL;

		mon->enable = 0;
	}

	return 0;
}

static int proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static int proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show, NULL);
}

static ssize_t scan_enable_write(struct file *file,
			 const char __user *buffer, size_t count, loff_t *f_pos)
{
	int ret;
	u32 value;

	if (!dbg_log || !dbg_log->monitor || !count)
		return -EINVAL;

	ret = kstrtouint_from_user(buffer, count, 10, &value);
	if (ret < 0)
		return (ssize_t)ret;

	mutex_lock(&dbg_mtx);
	if (value)
		debug_monitor_config(dbg_log, 1);
	else
		debug_monitor_config(dbg_log, 0);
	mutex_unlock(&dbg_mtx);

	return (ssize_t)count;
}

static ssize_t scan_enable_read(struct file *file,
				      char __user *buf, size_t len, loff_t *off)
{
	char data[1];

	if (!dbg_log || !dbg_log->monitor || !len)
		return -EINVAL;

	if (file->f_pos || *off)
		return 0;

	if (dbg_log->monitor->enable)
		data[0] = '1';
	else
		data[0] = '0';

	if (copy_to_user(buf, data, 1))
		return -EFAULT;

	*off += 1;

	return 1;
}

static const struct file_operations scan_enable_fops = {
	.owner = THIS_MODULE,
	.open = proc_open,
	.release = single_release,
	.read = scan_enable_read,
	.llseek = seq_lseek,
	.write = scan_enable_write,
};

static ssize_t scan_interval_write(struct file *file,
			 const char __user *buffer, size_t count, loff_t *f_pos)
{
	int ret;
	u32 interval;

	if (!dbg_log || !dbg_log->monitor || !count)
		return -EINVAL;

	ret = kstrtouint_from_user(buffer, count, 10, &interval);
	if (ret < 0)
		return (ssize_t)ret;

	dbg_log->monitor->interval = clamp(interval, (u32)0x02, (u32)0x3E8);

	return (ssize_t)count;
}

static ssize_t scan_interval_read(struct file *file,
				      char __user *buf, size_t len, loff_t *off)
{
	size_t len_copy, ret;
	char data[9];

	if (!dbg_log || !dbg_log->monitor || !len)
		return -EINVAL;

	ret = (size_t)snprintf(data, 9, "%d", dbg_log->monitor->interval);
	if (!ret)
		return ret;

	if ((file->f_pos + len) > ret)
		len_copy = ret - file->f_pos;
	else
		len_copy = len;

	ret = (size_t)copy_to_user(buf, data + file->f_pos, len_copy);

	len_copy = len_copy - ret;

	*off += len_copy;

	return len_copy;
}

static const struct file_operations scan_interval_fops = {
	.owner = THIS_MODULE,
	.open = proc_open,
	.release = single_release,
	.read = scan_interval_read,
	.llseek = seq_lseek,
	.write = scan_interval_write,
};

/**
 * sprd_debug_log_register - debug log register interface
 */
int sprd_debug_log_register(struct debug_log *dbg)
{
	struct proc_dir_entry *file;
	struct device *dev;
	int ret;

	if (dbg_log || !dbg || !dbg->dev)
		goto err_out;

	dbg_log = dbg;
	dev = dbg->dev;

	dev_info(dev, "Register power debug.\n");

	mutex_init(&dbg_mtx);

	proc_dir = proc_mkdir(DEBUGLOG_DIR, NULL);
	if (!proc_dir) {
		dev_err(dev, "Proc dir create failed.\n");
		goto err_out;
	}

	file = proc_create(SCAN_ENABLE, 0777, proc_dir, &scan_enable_fops);
	if (!file) {
		dev_err(dev, "Enable proc file create failed.\n");
		goto err_proc;
	}

	file = proc_create(SCAN_INTERVAL, 0777, proc_dir, &scan_interval_fops);
	if (!file) {
		dev_err(dev, "Interval proc file create failed.\n");
		goto err_enable;
	}

	ret = debug_monitor_config(dbg_log, dbg_log->monitor->enable);
	if (ret) {
		dev_err(dev, "Debug monitor init error.\n");
		goto err_interval;
	}

	ret = cpu_pm_register_notifier(&debug_notifier_block);
	if (ret) {
		dev_err(dev, "Debug notifier register error.\n");
		goto err_interval;
	}

	return 0;

err_interval:
	remove_proc_entry(SCAN_INTERVAL, proc_dir);
err_enable:
	remove_proc_entry(SCAN_ENABLE, proc_dir);
err_proc:
	remove_proc_entry(DEBUGLOG_DIR, NULL);
err_out:

	return -EINVAL;
}

/**
 * sprd_debug_log_unregister - debug log unregister interface
 */
int sprd_debug_log_unregister(void)
{
	int ret;

	if (!dbg_log)
		return -EINVAL;

	cpu_pm_unregister_notifier(&debug_notifier_block);

	remove_proc_entry(SCAN_INTERVAL, proc_dir);
	remove_proc_entry(SCAN_ENABLE, proc_dir);
	remove_proc_entry(DEBUGLOG_DIR, NULL);

	proc_dir = NULL;

	ret = debug_monitor_config(dbg_log, 0);
	if (ret) {
		dev_err(dbg_log->dev, "Stop debug monitor error.\n");
		return -EINVAL;
	}

	dbg_log = NULL;

	return 0;
}
