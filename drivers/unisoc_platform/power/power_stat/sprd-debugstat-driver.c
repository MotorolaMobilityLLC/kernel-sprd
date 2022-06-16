// SPDX-License-Identifier: GPL-2.0
//
// UNISOC APCPU POWER STAT driver
//
// Copyright (C) 2020 Unisoc, Inc.

#include <linux/cpu_pm.h>
#include <linux/cpumask.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mfd/syscon.h>
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
#include <linux/soc/sprd/sprd_pdbg.h>
#include <linux/sprd_sip_svc.h>
#include "sprd-debugstat.h"

struct ap_subsys_sleep_info {
	struct sprd_sip_svc_pwr_ops *pwr_ops;
	struct subsys_sleep_info *ap_sleep_info;
};

struct soc_subsys_sleep_info {
	struct subsys_sleep_info *soc_sleep_info;
	struct regmap *slp_cnt_regmap;
	u32 slp_cnt_offset;
	u32 slp_cnt_mask;
};

struct debugstat_data {
	struct device *dev;
	struct ap_subsys_sleep_info apsys_sleep_info;
	struct soc_subsys_sleep_info socsys_sleep_info;
};

static struct subsys_sleep_info *ap_sleep_info_get(void *data)
{
	const struct cpumask *cpu = cpu_online_mask;
	u64 major, intc_num, intc_bit;
	struct ap_subsys_sleep_info *ap_inst;
	struct device *dev = data;
	struct debugstat_data *dbgstat_data = dev_get_drvdata(dev);

	if (!dbgstat_data || !dbgstat_data->apsys_sleep_info.ap_sleep_info)
		return NULL;

	ap_inst = &dbgstat_data->apsys_sleep_info;
	ap_inst->ap_sleep_info->total_duration = ktime_get_boot_fast_ns();
	do_div(ap_inst->ap_sleep_info->total_duration, 1000000000);

	ap_inst->ap_sleep_info->active_core = (uint32_t)(cpu->bits[0]);

	ap_inst->pwr_ops->get_pdbg_info(PDBG_WS, 0, &major, NULL, NULL, NULL);
	intc_num = (major >> 16) & 0xFFFF;
	intc_bit = major & 0xFFFF;

	ap_inst->ap_sleep_info->wakeup_reason = (uint32_t)((intc_num << 5) + intc_bit);

	return ap_inst->ap_sleep_info;
}

static struct subsys_sleep_info *soc_sleep_info_get(void *data)
{
	unsigned int slp_cnt = 0;
	int ret;
	struct soc_subsys_sleep_info *soc_inst;
	struct device *dev = data;
	struct debugstat_data *dbgstat_data = dev_get_drvdata(dev);

	if (!dbgstat_data || !dbgstat_data->socsys_sleep_info.soc_sleep_info)
		return NULL;

	soc_inst = &dbgstat_data->socsys_sleep_info;

	ret = regmap_read(soc_inst->slp_cnt_regmap, soc_inst->slp_cnt_offset, &slp_cnt);
	if (ret < 0) {
		soc_inst->soc_sleep_info->slp_cnt = -1;
		return soc_inst->soc_sleep_info;
	}

	slp_cnt &= soc_inst->slp_cnt_mask;
	soc_inst->soc_sleep_info->slp_cnt = slp_cnt;

	return soc_inst->soc_sleep_info;
}
#ifdef CONFIG_PM_SLEEP
static int sprd_debugstat_suspend(struct device *dev)
{
	struct ap_subsys_sleep_info *ap_inst;
	struct debugstat_data *dbgstat_data = dev_get_drvdata(dev);
	u64 last_sleep_duration = ktime_get_boot_fast_ns();

	if (!dbgstat_data || !dbgstat_data->apsys_sleep_info.ap_sleep_info)
		return 0;

	ap_inst = &dbgstat_data->apsys_sleep_info;
	do_div(last_sleep_duration, 1000000000);
	ap_inst->ap_sleep_info->last_sleep_duration = (u32)last_sleep_duration;

	return 0;
}

static int sprd_debugstat_resume(struct device *dev)
{
	struct ap_subsys_sleep_info *ap_inst;
	u32 sleep, wakeup;
	u64 last_wakeup_duration = ktime_get_boot_fast_ns();
	struct debugstat_data *dbgstat_data = dev_get_drvdata(dev);

	if (!dbgstat_data || !dbgstat_data->apsys_sleep_info.ap_sleep_info)
		return 0;

	ap_inst = &dbgstat_data->apsys_sleep_info;

	do_div(last_wakeup_duration, 1000000000);
	ap_inst->ap_sleep_info->last_wakeup_duration = (u32)last_wakeup_duration;

	sleep = ap_inst->ap_sleep_info->last_sleep_duration;
	wakeup = ap_inst->ap_sleep_info->last_wakeup_duration;

	ap_inst->ap_sleep_info->sleep_duration_total += wakeup - sleep;

	return 0;
}
#endif

static int sprd_debugstat_apsys_init(struct debugstat_data *dbgstat_data)
{
	struct sprd_sip_svc_handle *sip;
	int ret;
	struct device *dev = dbgstat_data->dev;
	struct ap_subsys_sleep_info *ap_inst = &dbgstat_data->apsys_sleep_info;

	sip = sprd_sip_svc_get_handle();
	if (!sip) {
		pr_err("%s: Get wakeup sip error\n", __func__);
		return -EINVAL;
	}

	ap_inst->pwr_ops = &sip->pwr_ops;

	ap_inst->ap_sleep_info = devm_kzalloc(dev, sizeof(struct subsys_sleep_info), GFP_KERNEL);
	if (!ap_inst->ap_sleep_info) {
		pr_err("%s: Sleep info alloc error\n", __func__);
		return -ENOMEM;
	}

	strcpy(ap_inst->ap_sleep_info->subsystem_name, "AP");
	ap_inst->ap_sleep_info->current_status = 1;
	ap_inst->ap_sleep_info->irq_to_ap_count = 0;
	ap_inst->ap_sleep_info->slp_cnt = 0;

	ret = stat_info_register("ap_sys", ap_sleep_info_get, dev);
	if (ret) {
		pr_err("%s: Register ap sleep info get error\n", __func__);
		return -ENXIO;
	}

	return 0;
}

static int sprd_debugstat_socsys_init(struct debugstat_data *dbgstat_data)
{
	int ret;
	unsigned int dts_args[2];
	struct device *dev = dbgstat_data->dev;
	struct soc_subsys_sleep_info *soc_inst = &dbgstat_data->socsys_sleep_info;

	soc_inst->soc_sleep_info = devm_kzalloc(dev, sizeof(struct subsys_sleep_info), GFP_KERNEL);
	if (!soc_inst->soc_sleep_info) {
		pr_err("%s: Sleep info alloc error\n", __func__);
		return -ENOMEM;
	}

	strcpy(soc_inst->soc_sleep_info->subsystem_name, "SOC");
	soc_inst->soc_sleep_info->slp_cnt = 0;

	soc_inst->slp_cnt_regmap =
		syscon_regmap_lookup_by_phandle_args(dev->of_node, "soc_slp_cnt_reg", 2, dts_args);

	if (IS_ERR_OR_NULL(soc_inst->slp_cnt_regmap)) {
		pr_err("%s: slp_cnt_regmap error, check dts.\n", __func__);
		return -ENOMEM;
	}

	soc_inst->slp_cnt_offset = dts_args[0];
	soc_inst->slp_cnt_mask = dts_args[1];

	ret = stat_info_register("soc_sys", soc_sleep_info_get, dev);
	if (ret) {
		pr_err("%s: Register soc sleep info get error\n", __func__);
		return -ENXIO;
	}

	return 0;
}

static int sprd_debugstat_driver_probe(struct platform_device *pdev)
{
	int ret;
	struct debugstat_data *dbgstat_data;
	struct device *dev;

	if (!pdev) {
		pr_err("%s: Get debug device error\n", __func__);
		return -EINVAL;
	}

	dev = &pdev->dev;
	dbgstat_data = devm_kzalloc(dev, sizeof(struct debugstat_data), GFP_KERNEL);
	if (!dbgstat_data) {
		pr_err("%s: dbgstat_data alloc error\n", __func__);
		return -ENOMEM;
	}
	dbgstat_data->dev = dev;

	ret = sprd_debugstat_apsys_init(dbgstat_data);
	if (ret) {
		pr_err("%s: sprd_debugstat_apsys_init error\n", __func__);
		return ret;
	}

	ret = sprd_debugstat_socsys_init(dbgstat_data);
	if (ret) {
		pr_err("%s: sprd_debugstat_socsys_init error\n", __func__);
		return ret;
	}

	ret = sprd_debugstat_core_init();
	if (ret) {
		pr_err("%s: sprd_debugstat_core_init error\n", __func__);
		return ret;
	}

	dev_set_drvdata(dev, dbgstat_data);

	return 0;
}

/**
 * sprd_debugstat_driver_remove - remove the debug log driver
 */
static int sprd_debugstat_driver_remove(struct platform_device *pdev)
{
	int ret;

	ret = stat_info_unregister("ap_sys");
	ret |= stat_info_unregister("soc_sys");

	return ret;
}

static const struct dev_pm_ops sprd_debugstat_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sprd_debugstat_suspend, sprd_debugstat_resume)
};

static const struct of_device_id sprd_debugstat_of_match[] = {
	{.compatible = "sprd,debugstat",},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_debugstat_of_match);

static struct platform_driver sprd_debugstat_driver = {
	.driver = {
		.name = "sprd-debugstat",
		.of_match_table = sprd_debugstat_of_match,
		.pm = &sprd_debugstat_pm_ops,
	},
	.probe = sprd_debugstat_driver_probe,
	.remove = sprd_debugstat_driver_remove,
};

module_platform_driver(sprd_debugstat_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("sprd debug stat driver");
