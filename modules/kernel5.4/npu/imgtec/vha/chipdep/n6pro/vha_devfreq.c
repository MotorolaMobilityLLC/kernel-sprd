/*
 * DVFS functions for qogirn6pro NPU.
 *
 * Copyright (C) 2018 Spreadtrum Communications Inc.
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

#include <linux/err.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/devfreq.h>
#include <linux/pm_opp.h>
#include <linux/sprd_sip_svc.h>
#include <linux/kernel.h>
#include <linux/thermal.h>
#include <linux/version.h>

#include "vha_common.h"
#include "vha_chipdep.h"
#include "vha_devfreq.h"
#include "vha_cooling.h"

#define VHA_POLL_MS           100
#define VHA_UPTHRESHOLD       85
#define VHA_DOWNDIFFERENTIAL  10
#define VHA_PM_TIME_SHIFT     8
#define NNA_DEFAULT_FREQ      307200
#define HIGH_TEMPERATURE      65000
#define LOW_TEMPERATURE       60000

struct vha_ops {
	int (*set_freq) (u32 freq_khz);
	int (*disable_idle) (void);
	int (*get_max_state) (u32 *max_state);
	int (*get_opp) (u32 index, u32 *freq_khz, u32 *volt);
	int (*set_volts) (u32 high_temp);
};

struct npu_dvfs_context {
	bool npu_on;
	int npu_dvfs_on;
	int max_on;
	bool dvfs_init;
	bool high_temp;
	struct semaphore *sem;
	struct vha_ops ops;
	uint32_t last_freq_khz;
	uint32_t max_freq_khz;
	uint32_t max_state;
	struct thermal_zone_device *npu_tz;
};

DEFINE_SEMAPHORE(npu_dvfs_sem);
static struct npu_dvfs_context npu_dvfs_ctx=
{
	.npu_on = false,
	.npu_dvfs_on = 0,
	.max_on = 0,
	.dvfs_init = false,
	.max_freq_khz = 0,
	.max_state = 0,
	.high_temp = false,

	.sem=&npu_dvfs_sem,
};

static struct devfreq_simple_ondemand_data *data;

static void vha_set_freq(unsigned long freq_khz)
{
	if (npu_dvfs_ctx.npu_on)
		npu_dvfs_ctx.ops.set_freq(freq_khz);
	npu_dvfs_ctx.last_freq_khz = freq_khz > npu_dvfs_ctx.max_freq_khz ?
					npu_dvfs_ctx.max_freq_khz : freq_khz;
}

static ssize_t force_npu_freq_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long force_freq_khz;

	if (npu_dvfs_ctx.npu_dvfs_on || npu_dvfs_ctx.max_on)
		return count;

	sscanf(buf, "%lu\n", &force_freq_khz);

	down(npu_dvfs_ctx.sem);
	vha_set_freq(force_freq_khz);
	up(npu_dvfs_ctx.sem);

	return count;
}

static ssize_t force_npu_freq_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	count = sprintf(buf, "%d\n", npu_dvfs_ctx.last_freq_khz);
	return count;
}

static ssize_t npu_max_on_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int max_on = 0;

	sscanf(buf, "%u\n", &max_on);
#ifdef VHA_DEVFREQ
	vha_set_max_freq(max_on);
#endif
	return count;
}

static ssize_t npu_max_on_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	count = sprintf(buf, "%d\n", npu_dvfs_ctx.max_on);
	return count;
}

static ssize_t npu_auto_dvfs_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	count = sprintf(buf, "%d\n", npu_dvfs_ctx.npu_dvfs_on);
	return count;
}

static ssize_t npu_auto_dvfs_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int enable;

	sscanf(buf, "%u\n", &enable);

	if (npu_dvfs_ctx.max_on)
		return count;

	down(npu_dvfs_ctx.sem);
	if (enable == 1)
		npu_dvfs_ctx.npu_dvfs_on = 1;
	else if (enable == 0)
		npu_dvfs_ctx.npu_dvfs_on = 0;
	up(npu_dvfs_ctx.sem);

	return count;
}

static DEVICE_ATTR(force_npu_freq, 0664,
		force_npu_freq_show, force_npu_freq_store);
static DEVICE_ATTR(npu_auto_dvfs, 0664,
		npu_auto_dvfs_show, npu_auto_dvfs_store);
static DEVICE_ATTR(npu_max_on, 0664,
		npu_max_on_show, npu_max_on_store);

static struct attribute *dev_entries[] = {
	&dev_attr_force_npu_freq.attr,
	&dev_attr_npu_auto_dvfs.attr,
	&dev_attr_npu_max_on.attr,
	NULL,
};

static struct attribute_group dev_attr_group = {
	.attrs  = dev_entries,
};

int vha_set_max_freq(int max)
{
	down(npu_dvfs_ctx.sem);
	if (npu_dvfs_ctx.max_on == max)
		goto out;
	npu_dvfs_ctx.max_on = max;

	if (max == 1) {
		npu_dvfs_ctx.npu_dvfs_on = 0;
		vha_set_freq(npu_dvfs_ctx.max_freq_khz);
	} else {
		npu_dvfs_ctx.npu_dvfs_on = 1;
	}
out:
	up(npu_dvfs_ctx.sem);
	return 0;
}

int vha_dvfs_ctx_init(struct device *dev)
{
	int ret = 0;
	uint32_t default_freq_khz;
	struct sprd_sip_svc_handle *sip;
	struct sprd_sip_svc_npu_ops *ops;

	sip = sprd_sip_svc_get_handle();
	if (!sip) {
		dev_err(dev, "npu sip handle get error\n");
		return -EINVAL;
	}

	ops = &sip->npu_ops;

	npu_dvfs_ctx.ops.set_freq = ops->set_freq;
	npu_dvfs_ctx.ops.disable_idle = ops->disable_idle;
	npu_dvfs_ctx.ops.get_max_state = ops->get_max_state;
	npu_dvfs_ctx.ops.get_opp = ops->get_opp;
	npu_dvfs_ctx.ops.set_volts = ops->set_volts;

	ret = sysfs_create_group(&(dev->kobj), &dev_attr_group);
	if (ret) {
		dev_err(dev, "sysfs create fail: %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "sprd,npu_freq_default", &default_freq_khz);
	if (ret) {
		dev_warn(dev, "read dvfs default fail: %d\n", ret);
		default_freq_khz = NNA_DEFAULT_FREQ;
	}

	npu_dvfs_ctx.last_freq_khz = default_freq_khz;

	ret = npu_dvfs_ctx.ops.get_max_state(&npu_dvfs_ctx.max_state);

	npu_dvfs_ctx.ops.set_volts(0);

	return ret;
}

int vha_dvfs_ctx_deinit(struct device *dev)
{
	sysfs_remove_group(&(dev->kobj), &dev_attr_group);

	return 0;
}

static void vha_pm_get_dvfs_utilisation(struct vha_dev *vha, ktime_t now)
{
	ktime_t diff;
	u32 ns_time;

	diff = ktime_sub(now, vha->cur_state.time_period_start);
	if (ktime_to_ns(diff) < 0)
		return;

	ns_time = (u32)(ktime_to_ns(diff) >> VHA_PM_TIME_SHIFT);
	if (vha->cur_state.vha_active)
		vha->cur_state.value.time_busy += ns_time;
	else
		vha->cur_state.value.time_idle += ns_time;

	vha->cur_state.time_period_start = now;
}

void vha_update_dvfs_state(struct vha_dev *vha, bool vha_active)
{
	ktime_t now;
	unsigned long flags;
	if (!vha)
		return;

	spin_lock_irqsave(&vha->cur_state.lock, flags);
	now = ktime_get();
	vha_pm_get_dvfs_utilisation(vha, now);
	vha->cur_state.vha_active = vha_active;

	spin_unlock_irqrestore(&vha->cur_state.lock, flags);
}

static void vha_devfreq_exit(struct device *dev)
{
	struct vha_dev *vha = vha_dev_get_drvdata(dev);
	struct devfreq_dev_profile *dp;

	if (!vha)
		return;

	dp = vha->devfreq->profile;
	kfree(dp->freq_table);
}

static int vha_devfreq_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct dev_pm_opp *opp;
	unsigned long nominal_freq;
	unsigned long exact_freq;

	nominal_freq = *freq;
	/* get appropriate opp*/
	opp = devfreq_recommended_opp(dev, &nominal_freq, flags);
	if (IS_ERR_OR_NULL(opp)) {
		dev_err(dev, "Failed to get opp (%ld)\n", PTR_ERR(opp));
		return PTR_ERR(opp);
	}
	/* get frequency*/
	exact_freq = dev_pm_opp_get_freq(opp);
	dev_pm_opp_put(opp);
	if (exact_freq == npu_dvfs_ctx.last_freq_khz * 1000UL)
		goto out;

	down(npu_dvfs_ctx.sem);
	if (npu_dvfs_ctx.npu_on == 0 || npu_dvfs_ctx.npu_dvfs_on == 0)
		goto up_out;

	vha_set_freq(exact_freq / 1000UL);
up_out:
	up(npu_dvfs_ctx.sem);
out:
	*freq = npu_dvfs_ctx.last_freq_khz * 1000UL;
	return 0;
}

static int vha_devfreq_cur_freq(struct device *dev, unsigned long *freq)
{
	*freq = npu_dvfs_ctx.last_freq_khz * 1000UL;
	return 0;
}

static void vha_get_dvfs_metrics(struct vha_dev *vha,
				struct vha_devfreq_metrics *last,
				struct vha_devfreq_metrics *diff)
{
	struct vha_devfreq_metrics *cur = &vha->cur_state.value;
	unsigned long flags;
	spin_lock_irqsave(&vha->cur_state.lock, flags);

	vha_pm_get_dvfs_utilisation(vha, ktime_get());
	memset(diff, 0, sizeof(*diff));

	diff->time_busy = cur->time_busy - last->time_busy;
	diff->time_idle = cur->time_idle - last->time_idle;

	*last = *cur;

	spin_unlock_irqrestore(&vha->cur_state.lock, flags);
	return;
}

static int vha_devfreq_status(struct device *dev, struct devfreq_dev_status *state)
{
	struct vha_dev *vha = vha_dev_get_drvdata(dev);
	struct vha_devfreq_metrics diff;
	int temp, ret;

	if (!vha)
		goto out;

	vha_get_dvfs_metrics(vha, &vha->last_devfreq_metrics, &diff);
	state->busy_time = diff.time_busy;
	state->total_time = diff.time_busy + diff.time_idle;
	state->current_frequency = npu_dvfs_ctx.last_freq_khz * 1000UL;
	state->private_data = NULL;

	if (IS_ERR_OR_NULL(npu_dvfs_ctx.npu_tz) ||
			!npu_dvfs_ctx.npu_tz->ops->get_temp)
		goto out;

	ret = npu_dvfs_ctx.npu_tz->ops->get_temp(npu_dvfs_ctx.npu_tz, &temp);
	if (ret) {
		dev_warn(dev, "failed to get npu temp\n");
		goto out;
	}
	dev_dbg(dev, "cur_temp = %d\n", temp);
	down(npu_dvfs_ctx.sem);
	if (temp >= HIGH_TEMPERATURE) {
		if (!npu_dvfs_ctx.high_temp) {
			npu_dvfs_ctx.ops.set_volts(1);
			npu_dvfs_ctx.high_temp = true;
		}
	} else if (temp <= LOW_TEMPERATURE) {
		if (npu_dvfs_ctx.high_temp) {
			npu_dvfs_ctx.ops.set_volts(0);
			npu_dvfs_ctx.high_temp = false;
		}
	}
	up(npu_dvfs_ctx.sem);
out:
	return 0;
}

int vha_devfreq_init(struct vha_dev *vha)
{
	struct devfreq_dev_profile *dp;
	struct devfreq_simple_ondemand_data *data;

	int ret = 0, i;
	uint32_t freq_khz, volt;

	npu_dvfs_ctx.npu_on = true;

	for (i = 0; i < npu_dvfs_ctx.max_state; i++) {
		npu_dvfs_ctx.ops.get_opp(i, &freq_khz, &volt);
		ret = dev_pm_opp_add(vha->dev, freq_khz * 1000UL, volt);
		if (ret) {
			dev_err(vha->dev, "failed to add dev_pm_opp: %d\n", ret);
			goto dev_pm_opp_add_failed;
		}
	}

	dp = &vha->devfreq_profile;
	dp->polling_ms = VHA_POLL_MS;
	dp->initial_freq = npu_dvfs_ctx.last_freq_khz * 1000UL;
	dp->target = vha_devfreq_target;
	dp->get_cur_freq = vha_devfreq_cur_freq;
	dp->get_dev_status = vha_devfreq_status;
	dp->exit = vha_devfreq_exit;

	data = vmalloc(sizeof(struct devfreq_simple_ondemand_data));
	data->upthreshold = VHA_UPTHRESHOLD;
	data->downdifferential = VHA_DOWNDIFFERENTIAL;

	vha->devfreq = devm_devfreq_add_device(vha->dev, dp, "simple_ondemand", data);
	if (IS_ERR(vha->devfreq)) {
		dev_err(vha->dev, "add devfreq device fail\n");
		ret = (int)PTR_ERR(vha->devfreq);
		goto devfreq_add_device_failed;
	}

	vha->devfreq->scaling_max_freq = dp->freq_table[dp->max_state - 1];
	vha->devfreq->scaling_min_freq = dp->freq_table[0];

	npu_dvfs_ctx.max_freq_khz = dp->freq_table[dp->max_state - 1] / 1000UL;
	npu_dvfs_ctx.npu_dvfs_on = 1;

	ret = devfreq_register_opp_notifier(vha->dev, vha->devfreq);
	if (ret) {
		dev_err(vha->dev, "Failed to register OPP notifier (%d)\n", ret);
		goto devfreq_register_opp_notifier_failed;
	}

	vha_cooling_register(vha->devfreq);

	npu_dvfs_ctx.npu_tz = thermal_zone_get_zone_by_name("ai0-thmzone");
	if (IS_ERR_OR_NULL(npu_dvfs_ctx.npu_tz)) {
		dev_err(vha->dev, "Failed to get ai thermal zone\n");
		ret = -EFAULT;
		goto get_ai_thermal_zone_failed;
	}

	npu_dvfs_ctx.dvfs_init = true;
	return ret;

get_ai_thermal_zone_failed:
	vha_cooling_unregister();
	devfreq_unregister_opp_notifier(vha->dev, vha->devfreq);
devfreq_register_opp_notifier_failed:
	devm_devfreq_remove_device(vha->dev, vha->devfreq);
	vha->devfreq = NULL;
	npu_dvfs_ctx.npu_dvfs_on = 0;
devfreq_add_device_failed:
	kfree(dp->freq_table);
	vfree(data);
dev_pm_opp_add_failed:
	npu_dvfs_ctx.npu_on = false;

	return ret;

}

void vha_devfreq_term(struct vha_dev *vha)
{
	dev_dbg(vha->dev, "Term NPU devfreq\n");

	vha_cooling_unregister();

	devfreq_unregister_opp_notifier(vha->dev, vha->devfreq);
	npu_dvfs_ctx.dvfs_init = false;
	npu_dvfs_ctx.npu_on = false;

	vfree(data);
}

void vha_devfreq_suspend(struct device *dev)
{
	struct vha_dev *vha = NULL;

	vha = vha_dev_get_drvdata(dev);
	if (!vha)
		return;

	if (npu_dvfs_ctx.dvfs_init) {
		devfreq_suspend_device(vha->devfreq);
		down(npu_dvfs_ctx.sem);
		npu_dvfs_ctx.npu_on = false;
		up(npu_dvfs_ctx.sem);
	}
	return;
}

void vha_devfreq_resume(struct device *dev)
{
	struct vha_dev *vha = NULL;

	vha = vha_dev_get_drvdata(dev);
	if (!vha)
		return;

	if (npu_dvfs_ctx.dvfs_init) {
		down(npu_dvfs_ctx.sem);
		npu_dvfs_ctx.npu_on = true;
		npu_dvfs_ctx.ops.disable_idle();
		vha_set_freq(npu_dvfs_ctx.last_freq_khz);
		up(npu_dvfs_ctx.sem);
		devfreq_resume_device(vha->devfreq);
	}
	return;
}
