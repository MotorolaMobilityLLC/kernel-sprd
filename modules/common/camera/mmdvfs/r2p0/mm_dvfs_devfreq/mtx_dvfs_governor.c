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

#include "mm_devfreq_common.h"

static int dvfs_probe(struct platform_device *pdev);
static int dvfs_remove(struct platform_device *pdev);
static int dvfs_gov_get_target(struct devfreq *devfreq,
                                   unsigned long *freq);
static int dvfs_gov_event_handler(struct devfreq *devfreq,
                                      unsigned int event, void *data);

static const struct of_device_id mtx_data_dvfs_of_match[] = {
    {.compatible = "sprd,hwdvfs-mtx_data"}, {},
};
MODULE_DEVICE_TABLE(of, mtx_data_dvfs_of_match);

static struct platform_driver mtx_data_dvfs_driver = {
    .probe = dvfs_probe,
    .remove = dvfs_remove,
    .driver =
        {
            .name = "mtx_data-dvfs", .of_match_table = mtx_data_dvfs_of_match,
        },
};

static struct devfreq_dev_profile dvfs_profile = {
    .polling_ms = 200,
    .target = dvfs_target,
    .get_dev_status = dvfs_get_dev_status,
    .get_cur_freq = dvfs_get_cur_freq,
};

static int dvfs_gov_get_target(struct devfreq *devfreq,
                                   unsigned long *freq) {
    struct module_dvfs *module = dev_get_drvdata(devfreq->dev.parent);
    if (module->dvfs_enable) {
        unsigned long adjusted_freq = module->user_freq;

        if (module->user_freq_type == DVFS_WORK)
            adjusted_freq = module->module_dvfs_para.u_work_freq;
        else
            adjusted_freq = module->module_dvfs_para.u_idle_freq;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
        if (devfreq->max_freq && adjusted_freq > devfreq->max_freq)
            adjusted_freq = devfreq->max_freq;

        if (devfreq->min_freq && adjusted_freq < devfreq->min_freq)
            adjusted_freq = devfreq->min_freq;
#endif
        *freq = adjusted_freq;
    } else
        pr_err("mtx_data_dvfs_gov, %s: dvfs unenable", __func__);

    pr_info("mtx_data_dvfs_gov, %s: *freq:%lu", __func__, *freq);

    return MM_DVFS_SUCCESS;
}

static int dvfs_probe(struct platform_device *pdev) {
    struct device *dev = &pdev->dev;
    struct device_node *np = pdev->dev.of_node;
    struct module_dvfs *module;
    int ret = MM_DVFS_SUCCESS;

    pr_info("module-dvfs initialized\n");

    module = devm_kzalloc(dev, sizeof(*module), GFP_KERNEL);
    if (!module)
        return -ENOMEM;

    mutex_init(&module->lock);

#ifdef SUPPORT_SWDVFS

    module->clk_module_core = devm_clk_get(dev, "clk_module_core");
    if (IS_ERR(module->clk_module_core))
        dev_err(dev, "Cannot get the clk_module_core clk\n");

    if (of_property_read_u32(np, "sprd,dvfs-wait-window", &module->dvfs_wait_window))
        pr_err("np:sprd,dvfs-wait-window");
#endif
    ret = dev_pm_opp_of_add_table(dev);
    if (ret) {
    dev_err(dev, "dvfs: Invalid operating-points in device tree.\n");
        goto err;
    }

    if (of_property_read_u32(np, "sprd,dvfs-work-index-def",
                         &module->module_dvfs_para.ip_coffe.work_index_def))
        pr_err("np: the value of the of_property_read_u32\n");
    platform_set_drvdata(pdev, module);
    module->devfreq =
        devm_devfreq_add_device(dev, &dvfs_profile, "mtx_data_dvfs", NULL);
    if (IS_ERR(module->devfreq)) {
        dev_err(dev, "failed to add devfreq dev with module-dvfs governor\n");
        ret = PTR_ERR(module->devfreq);
        goto err;
    }

    module->pw_nb.priority = 0;
    module->pw_nb.notifier_call = dvfs_notify_callback;
    ret = mmsys_register_notifier(&module->pw_nb);
    if (ret) {
        pr_err("error: mmsys_register_notifier failed");
    }
    return ret;

err:
    dev_pm_opp_of_remove_table(dev);
    return -EINVAL;
}

static int dvfs_remove(struct platform_device *pdev) {
    pr_err("%s:\n", __func__);

    return MM_DVFS_SUCCESS;
}
/*sys for gov_entries*/
static DEVICE_ATTR(set_hw_dvfs_en, 0644, get_dvfs_enable_show,
                   set_dvfs_enable_store);
static DEVICE_ATTR(set_auto_tune_en, 0644, get_auto_tune_en_show,
                   set_auto_tune_en_store);
static DEVICE_ATTR(set_dvfs_coffe, 0644, get_dvfs_coffe_show,
                   set_dvfs_coffe_store);
static DEVICE_ATTR(get_ip_status, 0644, get_ip_status_show,
                   get_ip_status_store);
static DEVICE_ATTR(set_work_freq, 0644, get_work_freq_show,
                   set_work_freq_store);
static DEVICE_ATTR(set_idle_freq, 0644, get_idle_freq_show,
                   set_idle_freq_store);
static DEVICE_ATTR(set_work_index, 0644, get_work_index_show,
                   set_work_index_store);
static DEVICE_ATTR(set_idle_index, 0644, get_idle_index_show,
                   set_idle_index_store);
static DEVICE_ATTR(set_fix_dvfs_value, 0644, get_fix_dvfs_show,
                   set_fix_dvfs_store);
static DEVICE_ATTR(get_dvfs_table_info, 0644, get_dvfs_table_info_show,
                   set_dvfs_table_info_store);
/*sys for coeff_entries*/
static DEVICE_ATTR(set_gfree_wait_delay, 0644, get_gfree_wait_delay_show,
                   set_gfree_wait_delay_store);
static DEVICE_ATTR(set_freq_upd_hdsk_en, 0644, get_freq_upd_hdsk_en_show,
                   set_freq_upd_hdsk_en_store);
static DEVICE_ATTR(set_freq_upd_delay_en, 0644, get_freq_upd_delay_en_show,
                   set_freq_upd_delay_en_store);
static DEVICE_ATTR(set_freq_upd_en_byp, 0644, get_freq_upd_en_byp_show,
                   set_freq_upd_en_byp_store);
static DEVICE_ATTR(set_sw_trig_en, 0644, get_sw_trig_en_show,
                   set_sw_trig_en_store);
static DEVICE_ATTR(set_idle_def_index, 0644, get_idle_index_show,
                   set_idle_index_store);
static DEVICE_ATTR(set_work_def_index, 0644, get_work_index_show, NULL);

static struct attribute *dev_entries[] = {

    &dev_attr_set_hw_dvfs_en.attr,
    &dev_attr_set_auto_tune_en.attr,
    &dev_attr_set_dvfs_coffe.attr,
    &dev_attr_get_ip_status.attr,
    &dev_attr_set_work_freq.attr,
    &dev_attr_set_idle_freq.attr,
    &dev_attr_set_work_index.attr,
    &dev_attr_set_idle_index.attr,
    &dev_attr_set_fix_dvfs_value.attr,
    &dev_attr_get_dvfs_table_info.attr,
    NULL,
};

static struct attribute *coeff_entries[] = {

    &dev_attr_set_gfree_wait_delay.attr,
    &dev_attr_set_freq_upd_hdsk_en.attr,
    &dev_attr_set_freq_upd_delay_en.attr,
    &dev_attr_set_freq_upd_en_byp.attr,

    &dev_attr_set_sw_trig_en.attr,
    &dev_attr_set_work_def_index.attr,
    &dev_attr_set_idle_def_index.attr,
    &dev_attr_set_auto_tune_en.attr,
    NULL,
};

static struct attribute_group dev_attr_group = {
    .name = "mtx_data_governor", .attrs = dev_entries,
};

static struct attribute_group coeff_attr_group = {
    .name = "mtx_data_coeff", .attrs = coeff_entries,
};

struct devfreq_governor mtx_data_dvfs_gov = {
    .name = "mtx_data_dvfs",
    .get_target_freq = dvfs_gov_get_target,
    .event_handler = dvfs_gov_event_handler,
};

static void userspace_exit(struct devfreq *devfreq) {
    /*
     * Remove the sysfs entry, unless this is being called after
     * device_del(), which should have done this already via kobject_del().
     */
    if (devfreq->dev.kobj.sd) {
        sysfs_remove_group(&devfreq->dev.kobj, &dev_attr_group);
        sysfs_remove_group(&devfreq->dev.kobj, &coeff_attr_group);
    }
}

static int userspace_init(struct devfreq *devfreq) {
    int err;

    struct module_dvfs *module = dev_get_drvdata(devfreq->dev.parent);

    module->dvfs_ops = get_ip_dvfs_ops("MTXDA_DVFS_OPS");

    err = sysfs_create_group(&devfreq->dev.kobj, &dev_attr_group);
    err = sysfs_create_group(&devfreq->dev.kobj, &coeff_attr_group);

    return err;
}

static int dvfs_gov_event_handler(struct devfreq *devfreq,
                                      unsigned int event, void *data) {
    int ret = MM_DVFS_SUCCESS;

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

int mtx_data_dvfs_init(void) {
    int ret;
    ret = register_ip_dvfs_ops(&mtx_data_dvfs_ops);
    if (ret) {
        pr_err("%s: failed to add ops: %d\n", __func__, ret);
        return ret;
    }

    ret = devfreq_add_governor(&mtx_data_dvfs_gov);
    if (ret) {
        pr_err("%s: failed to add governor: %d\n", __func__, ret);
        return ret;
    }
    ret = platform_driver_register(&mtx_data_dvfs_driver);
    if (ret)
        devfreq_remove_governor(&mtx_data_dvfs_gov);
    return ret;
}

void mtx_data_dvfs_exit(void) {
    int ret;
    platform_driver_unregister(&mtx_data_dvfs_driver);

    ret = devfreq_remove_governor(&mtx_data_dvfs_gov);
    if (ret)
        pr_err("%s: failed to remove governor: %d\n", __func__, ret);

    ret = unregister_ip_dvfs_ops(&mtx_data_dvfs_ops);
    if (ret)
        pr_err("%s: failed to remove ops: %d\n", __func__, ret);

}

