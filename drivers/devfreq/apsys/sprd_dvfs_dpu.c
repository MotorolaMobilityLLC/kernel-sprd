/*
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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/devfreq.h>
#include <linux/devfreq-event.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include "../governor.h"

#include "sprd_dvfs_comm.h"
#include "sprd_dvfs_dpu.h"

LIST_HEAD(dpu_dvfs_head);
BLOCKING_NOTIFIER_HEAD(dpu_dvfs_chain);

static ssize_t get_dvfs_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct dpu_dvfs *dpu = dev_get_drvdata(devfreq->dev.parent);
	int ret;

	if (dpu)
		ret = sprintf(buf, "%d\n", dpu->dvfs_coffe.hw_dvfs_en);
	else
		ret = sprintf(buf, "undefined\n");

	return ret;
}

static ssize_t set_dvfs_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct dpu_dvfs *dpu = dev_get_drvdata(devfreq->dev.parent);
	u32 dvfs_en;
	int ret;

	ret = sscanf(buf, "%d\n", &dvfs_en);
	if (ret == 0)
		return -EINVAL;

	dpu->dvfs_coffe.hw_dvfs_en = dvfs_en;

	if (dpu->dvfs_ops && dpu->dvfs_ops->hw_dvfs_en)
		dpu->dvfs_ops->hw_dvfs_en(dpu->dvfs_coffe.hw_dvfs_en);
	else
		pr_info("%s: ip ops null\n", __func__);

	return count;
}

static ssize_t get_work_freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct dpu_dvfs *dpu = dev_get_drvdata(devfreq->dev.parent);
	u32 work_freq;
	int ret;

	if (dpu->dvfs_ops && dpu->dvfs_ops->get_work_freq) {
		work_freq = dpu->dvfs_ops->get_work_freq();
		ret = sprintf(buf, "%d\n", work_freq);
	} else
		ret = sprintf(buf, "undefined\n");

	return ret;
}

static ssize_t set_work_freq_store(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct dpu_dvfs *dpu = dev_get_drvdata(devfreq->dev.parent);
	u32 user_freq;
	int ret;

	mutex_lock(&devfreq->lock);

	ret = sscanf(buf, "%d\n", &user_freq);
	if (ret == 0) {
		mutex_unlock(&devfreq->lock);
		return -EINVAL;
	}

	dpu->work_freq = user_freq;
	dpu->freq_type = DVFS_WORK;
	ret = update_devfreq(devfreq);
	if (ret == 0)
		ret = count;

	mutex_unlock(&devfreq->lock);

	return ret;
}

static ssize_t get_idle_freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct dpu_dvfs *dpu = dev_get_drvdata(devfreq->dev.parent);
	u32 idle_freq;
	int ret;

	if (dpu->dvfs_ops && dpu->dvfs_ops->get_idle_freq) {
		idle_freq = dpu->dvfs_ops->get_idle_freq();
		ret = sprintf(buf, "%d\n", idle_freq);
	} else
		ret = sprintf(buf, "undefined\n");

	return ret;
}

static ssize_t set_idle_freq_store(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct dpu_dvfs *dpu = dev_get_drvdata(devfreq->dev.parent);
	u32 user_freq;
	int ret;

	mutex_lock(&devfreq->lock);

	ret = sscanf(buf, "%d\n", &user_freq);
	if (ret == 0) {
		mutex_unlock(&devfreq->lock);
		return -EINVAL;
	}

	dpu->idle_freq = user_freq;
	dpu->freq_type = DVFS_IDLE;
	ret = update_devfreq(devfreq);
	if (ret == 0)
		ret = count;

	mutex_unlock(&devfreq->lock);

	return ret;
}

static ssize_t get_work_index_show(struct device *dev,
		struct device_attribute *attr,  char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct dpu_dvfs *dpu = dev_get_drvdata(devfreq->dev.parent);
	int work_index, ret;

	if (dpu->dvfs_ops && dpu->dvfs_ops->get_work_index) {
		work_index = dpu->dvfs_ops->get_work_index();
		ret = sprintf(buf, "%d\n", work_index);
	} else
		ret = sprintf(buf, "undefined\n");

	return ret;
}

static ssize_t set_work_index_store(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct dpu_dvfs *dpu = dev_get_drvdata(devfreq->dev.parent);
	int work_index, ret;

	ret = sscanf(buf, "%d\n", &work_index);
	if (ret == 0) {
		mutex_unlock(&devfreq->lock);
		return -EINVAL;
	}

	if (dpu->dvfs_ops && dpu->dvfs_ops->set_work_index)
		dpu->dvfs_ops->set_work_index(work_index);
	else
		pr_info("%s: ip ops null\n", __func__);

	return count;
}

static ssize_t get_idle_index_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct dpu_dvfs *dpu = dev_get_drvdata(devfreq->dev.parent);
	int idle_index, ret;

	if (dpu->dvfs_ops && dpu->dvfs_ops->get_idle_index) {
		idle_index = dpu->dvfs_ops->get_idle_index();
		ret = sprintf(buf, "%d\n", idle_index);
	} else
		ret = sprintf(buf, "undefined\n");

	return ret;
}

static ssize_t set_idle_index_store(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct dpu_dvfs *dpu = dev_get_drvdata(devfreq->dev.parent);
	int idle_index, ret;

	ret = sscanf(buf, "%d\n", &idle_index);
	if (ret == 0) {
		mutex_unlock(&devfreq->lock);
		return -EINVAL;
	}

	if (dpu->dvfs_ops && dpu->dvfs_ops->set_idle_index)
		dpu->dvfs_ops->set_idle_index(idle_index);
	else
		pr_info("%s: ip ops null\n", __func__);

	return count;
}

static ssize_t get_dvfs_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct dpu_dvfs *dpu = dev_get_drvdata(devfreq->dev.parent);
	struct ip_dvfs_status dvfs_status;
	ssize_t len = 0;

	if (dpu->dvfs_ops && dpu->dvfs_ops->get_status)
		dpu->dvfs_ops->get_status(&dvfs_status);
	else
		pr_info("%s: dvfs_read_ops is null\n", __func__);

	len = sprintf(buf, "apsys_voltage\tvsp_vote\tdpu_vote\n");

	len += sprintf(buf + len, "%d\t\t%d\t\t%d\t\t\n",
			dvfs_status.ap_volt, dvfs_status.vsp_vote,
			dvfs_status.dpu_vote);

	return len;
}

static ssize_t get_dvfs_table_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct dpu_dvfs *dpu = dev_get_drvdata(devfreq->dev.parent);
	struct ip_dvfs_map_cfg dvfs_table[8];
	ssize_t len = 0;
	int i;

	if (dpu->dvfs_ops && dpu->dvfs_ops->get_dvfs_table)
		dpu->dvfs_ops->get_dvfs_table(dvfs_table);
	else
		pr_info("%s: ip ops null\n", __func__);

	len = sprintf(buf, "map_index\tvolt_level\tclk_level\tclk_rate\n");
	for (i = 0; i < 5; i++) {
		len += sprintf(buf+len, "%d\t\t%d\t\t%d\t\t%d\t\t\n",
				dvfs_table[i].map_index,
				dvfs_table[i].volt_level,
				dvfs_table[i].clk_level,
				dvfs_table[i].clk_rate);
	}

	return len;
}

static DEVICE_ATTR(hw_dvfs_en, 0644, get_dvfs_enable_show,
				   set_dvfs_enable_store);
static DEVICE_ATTR(work_freq, 0644, get_work_freq_show,
				   set_work_freq_store);
static DEVICE_ATTR(idle_freq, 0644, get_idle_freq_show,
				   set_idle_freq_store);
static DEVICE_ATTR(work_index, 0644, get_work_index_show,
				   set_work_index_store);
static DEVICE_ATTR(idle_index, 0644, get_idle_index_show,
				   set_idle_index_store);
static DEVICE_ATTR(dvfs_status, 0644, get_dvfs_status_show, NULL);
static DEVICE_ATTR(dvfs_table, 0644, get_dvfs_table_info_show, NULL);

static struct attribute *dev_entries[] = {
	&dev_attr_hw_dvfs_en.attr,
	&dev_attr_work_freq.attr,
	&dev_attr_idle_freq.attr,
	&dev_attr_work_index.attr,
	&dev_attr_idle_index.attr,
	&dev_attr_dvfs_status.attr,
	&dev_attr_dvfs_table.attr,
	NULL,
};

static struct attribute_group dev_attr_group = {
	.name	= "dpu_governor",
	.attrs	= dev_entries,
};

static int dpu_dvfs_target(struct device *dev, unsigned long *freq,
				u32 flags)
{
	struct dpu_dvfs *dpu = dev_get_drvdata(dev);
	struct dev_pm_opp *opp;
	u32 target_freq;

	pr_info("devfreq_dev_profile-->target\n");

	opp = devfreq_recommended_opp(dev, freq, flags);
	if (IS_ERR(opp)) {
		dev_err(dev, "failed to find opp for %lu KHz\n", *freq);
		return PTR_ERR(opp);
	}
	target_freq = dev_pm_opp_get_freq(opp);
	dev_pm_opp_put(opp);

	if (dpu->freq_type == DVFS_WORK) {
		if (dpu->dvfs_ops && dpu->dvfs_ops->set_work_freq)
			dpu->dvfs_ops->set_work_freq(target_freq);
	} else {
		if (dpu->dvfs_ops && dpu->dvfs_ops->set_idle_freq)
			dpu->dvfs_ops->set_idle_freq(target_freq);
	}

	pr_info("dvfs set freq:%u to dpu\n", target_freq);

	*freq = target_freq;

	return 0;
}

static int dpu_dvfs_get_dev_status(struct device *dev,
					 struct devfreq_dev_status *stat)
{
	struct dpu_dvfs *dpu = dev_get_drvdata(dev);
	struct devfreq_event_data edata;
	int ret = 0;

	pr_info("devfreq_dev_profile-->get_dev_status\n");

	ret = devfreq_event_get_event(dpu->edev, &edata);
	if (ret < 0)
		return ret;

	stat->current_frequency = dpu->work_freq;
	stat->busy_time = edata.load_count;
	stat->total_time = edata.total_count;

	return ret;
}

static int dpu_dvfs_get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct dpu_dvfs *dpu = dev_get_drvdata(dev);

	pr_info("devfreq_dev_profile-->get_cur_freq\n");

	if (dpu->freq_type == DVFS_WORK)
		*freq = dpu->work_freq;
	else
		*freq = dpu->idle_freq;

	return 0;
}

static struct devfreq_dev_profile dpu_dvfs_profile = {
	.polling_ms	= 200,
	.target             = dpu_dvfs_target,
	.get_dev_status     = dpu_dvfs_get_dev_status,
	.get_cur_freq       = dpu_dvfs_get_cur_freq,
};

static int userspace_init(struct devfreq *devfreq)
{
	int ret = 0;

	ret = sysfs_create_group(&devfreq->dev.kobj, &dev_attr_group);

	return ret;
}

static void userspace_exit(struct devfreq *devfreq)
{
	/*
	 * Remove the sysfs entry, unless this is being called after
	 * device_del(), which should have done this already via kobject_del().
	 */
	if (devfreq->dev.kobj.sd)
		sysfs_remove_group(&devfreq->dev.kobj, &dev_attr_group);

	sysfs_remove_group(&devfreq->dev.kobj, &dev_attr_group);
}

static int dpu_gov_get_target(struct devfreq *devfreq,
				     unsigned long *freq)
{
	struct dpu_dvfs *dpu = dev_get_drvdata(devfreq->dev.parent);
	u32 adjusted_freq = 0;

	pr_info("devfreq_governor-->get_target_freq\n");

	if (devfreq->max_freq && adjusted_freq > devfreq->max_freq)
		adjusted_freq = devfreq->max_freq;
	else if (devfreq->min_freq && adjusted_freq < devfreq->min_freq)
		adjusted_freq = devfreq->min_freq;
	else if (dpu->freq_type == DVFS_WORK)
		adjusted_freq = dpu->work_freq;
	else
		adjusted_freq = dpu->idle_freq;

	*freq = adjusted_freq;

	return 0;
}

static int dpu_gov_event_handler(struct devfreq *devfreq,
					unsigned int event, void *data)
{
	int ret = 0;

	pr_info("devfreq_governor-->event_handler(%d)\n", event);
	switch (event) {
	case DEVFREQ_GOV_START:
		ret = userspace_init(devfreq);
		break;
	case DEVFREQ_GOV_STOP:
		userspace_exit(devfreq);
		break;
	default:
		break;
	}

	return ret;
}

struct devfreq_governor dpu_devfreq_gov = {
	.name = "dpu_dvfs",
	.get_target_freq = dpu_gov_get_target,
	.event_handler = dpu_gov_event_handler,
};

static int dpu_dvfs_device_create(struct dpu_dvfs *dpu,
				struct device *parent)
{
	int ret;

	dpu->dev.class = dvfs_class;
	dpu->dev.parent = parent;
	dpu->dev.of_node = parent->of_node;
	dev_set_name(&dpu->dev, "dpu");
	dev_set_drvdata(&dpu->dev, dpu);

	ret = device_register(&dpu->dev);
	if (ret)
		pr_err("dpu dvfs device register failed\n");

	return ret;
}

static int dpu_dvfs_coffe_parse_dt(struct dpu_dvfs *dpu,
			      struct device_node *np)
{
	int ret = 0;

	ret = of_property_read_u32(np, "sprd,gfree-wait-delay",
			&dpu->dvfs_coffe.gfree_wait_delay);
	ret |= of_property_read_u32(np, "sprd,freq-upd-hdsk-en",
			&dpu->dvfs_coffe.freq_upd_hdsk_en);
	ret |= of_property_read_u32(np, "sprd,freq-upd-delay-en",
			&dpu->dvfs_coffe.freq_upd_delay_en);
	ret |= of_property_read_u32(np, "sprd,freq-upd-en-byp",
			&dpu->dvfs_coffe.freq_upd_en_byp);
	ret |= of_property_read_u32(np, "sprd,sw-trig-en",
			&dpu->dvfs_coffe.sw_trig_en);
	ret |= of_property_read_u32(np, "sprd,hw-dvfs-en",
			&dpu->dvfs_coffe.hw_dvfs_en);
	ret |= of_property_read_u32(np, "sprd,work-index-def",
			&dpu->dvfs_coffe.work_index_def);
	ret |= of_property_read_u32(np, "sprd,idle-index-def",
			&dpu->dvfs_coffe.idle_index_def);

	return ret;
}

static int dpu_dvfs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct dpu_dvfs *dpu;
	const char *str = NULL;
	int ret;

	pr_info("dpu-dvfs initialized\n");

	dpu = devm_kzalloc(dev, sizeof(*dpu), GFP_KERNEL);
	if (!dpu)
		return -ENOMEM;

	str = (char *)of_device_get_match_data(dev);

	dpu->dvfs_ops = dpu_dvfs_ops_attach(str);
	if (dpu->dvfs_ops == NULL) {
		pr_err("attach dpu dvfs ops %s failed\n", str);
		return -EINVAL;
	}

	dpu_dvfs_coffe_parse_dt(dpu, np);

	ret = dev_pm_opp_of_add_table(dev);
	if (ret) {
		dev_err(dev, "invalid operating-points in device tree.\n");
		return ret;
	}

	dpu_dvfs_device_create(dpu, dev);

	platform_set_drvdata(pdev, dpu);
	dpu->devfreq = devm_devfreq_add_device(dev,
						 &dpu_dvfs_profile,
						 "dpu_dvfs",
						 NULL);
	if (IS_ERR(dpu->devfreq)) {
		dev_err(dev,
			"failed to add devfreq dev with dpu-dvfs governor\n");
		ret = PTR_ERR(dpu->devfreq);
		goto err;
	}

	device_rename(&dpu->devfreq->dev, "dpu");

	if (dpu->dvfs_ops && dpu->dvfs_ops->dvfs_init)
		dpu->dvfs_ops->dvfs_init(dpu);

	return 0;
err:
	dev_pm_opp_of_remove_table(dev);

	return ret;
}

static int dpu_dvfs_remove(struct platform_device *pdev)
{
	pr_info("dpu_dvfs_remove\n");
	return 0;
}

static const struct of_device_id dpu_dvfs_of_match[] = {
	{ .compatible = "sprd,hwdvfs-dpu-sharkl5", .data = (void *)"sharkl5" },
	{ }
};

MODULE_DEVICE_TABLE(of, dpu_dvfs_of_match);

static struct platform_driver dpu_dvfs_driver = {
	.probe	= dpu_dvfs_probe,
	.remove	= dpu_dvfs_remove,
	.driver = {
		.name = "dpu-dvfs",
		.of_match_table = dpu_dvfs_of_match,
	},
};

static int __init dpu_dvfs_init(void)
{
	int ret = 0;

	ret = devfreq_add_governor(&dpu_devfreq_gov);
	if (ret) {
		pr_err("%s: failed to add governor: %d\n", __func__, ret);
		return ret;
	}

	ret = platform_driver_register(&dpu_dvfs_driver);
	if (ret)
		devfreq_remove_governor(&dpu_devfreq_gov);

	return ret;
}
module_init(dpu_dvfs_init);

static void __exit dpu_dvfs_exit(void)
{
	int ret = 0;

	platform_driver_unregister(&dpu_dvfs_driver);

	ret = devfreq_remove_governor(&dpu_devfreq_gov);
	if (ret)
		pr_err("%s: failed to remove governor: %d\n", __func__, ret);
}
module_exit(dpu_dvfs_exit)

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("sprd dpu devfreq driver");
MODULE_AUTHOR("Kevin Tang <kevin.tang@unisoc.com>");
