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

 int dvfs_target(struct device *dev, unsigned long *freq, u32 flags) {
     struct module_dvfs *module = dev_get_drvdata(dev);
     struct dev_pm_opp *opp;
     unsigned long target_freq = *freq, target_volt = 0;
     int err = 0;
     pr_info("devfreq_dev_profile-->target,freq=%lu module->freq = %lu\n", *freq,
             module->freq);
     if (!module->dvfs_enable) {
         pr_info("dvfs power off\n");
         return -EINVAL;
     }
     opp = devfreq_recommended_opp(dev, freq, flags);
     if (IS_ERR(opp)) {
         dev_err(dev, "Failed to find opp for %lu KHz\n", *freq);
         return PTR_ERR(opp);
     }
     target_freq = dev_pm_opp_get_freq(opp);
     target_volt = dev_pm_opp_get_voltage(opp);
     dev_pm_opp_put(opp);
     if (module->freq == *freq)
         return 0;

     mutex_lock(&module->lock);
     module->dvfs_ops->updata_target_freq(module->devfreq, target_volt, target_freq,
                                       FALSE);

     if (err) {
         dev_err(dev, "Cannot to set freq:%lu to module, err: %d\n", target_freq,
                 err);
         goto out;
     }

     module->freq = target_freq;
     module->volt = target_volt;

 out:
     mutex_unlock(&module->lock);
     return err;
 }

 int dvfs_get_dev_status(struct device *dev,
                             struct devfreq_dev_status *stat) {
     struct module_dvfs *module = dev_get_drvdata(dev);
     struct devfreq_event_data edata;
     int ret = 0;

     pr_info("devfreq_dev_profile-->get_dev_status\n");

     if (!module->dvfs_enable) {
         pr_info("dvfs power off\n");
         return -EINVAL;
     }

     ret = devfreq_event_get_event(module->edev, &edata);
     if (ret < 0)
         return ret;

     stat->current_frequency = module->freq;
     stat->busy_time = edata.load_count;
     stat->total_time = edata.total_count;

     return ret;
 }

 int dvfs_get_cur_freq(struct device *dev, unsigned long *freq) {
     struct module_dvfs *module = dev_get_drvdata(dev);
     if (!module->dvfs_enable) {
         pr_info("dvfs power off\n");
         return -EINVAL;
     }

     *freq = module->freq;
     pr_info("devfreq_dev_profile-->get_cur_freq,*freq=%lu\n", *freq);
     return 0;
 }

 int dvfs_notify_callback(struct notifier_block *nb,
                                     unsigned long type, void *data) {
     struct module_dvfs *module = container_of(nb, struct module_dvfs, pw_nb);

     switch (type) {

     case MMSYS_POWER_ON:
         if (module->dvfs_ops != NULL && module->dvfs_ops->power_on_nb != NULL)
             module->dvfs_ops->power_on_nb(module->devfreq);
         break;
     case MMSYS_POWER_OFF:
         if (module->dvfs_ops != NULL && module->dvfs_ops->power_off_nb != NULL)
             module->dvfs_ops->power_off_nb(module->devfreq);
         break;

     default:
         return -EINVAL;
     }

     return NOTIFY_OK;
 }


 ssize_t get_dvfs_enable_show(struct device *dev,
                                    struct device_attribute *attr, char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    int err = 0;

    module = dev_get_drvdata(devfreq->dev.parent);
    if (!module->dvfs_enable) {
        pr_info("dvfs power off\n");
        return -EINVAL;
    }
    mutex_lock(&devfreq->lock);
    if (module != NULL)
        err = sprintf(buf, "%d\n", module->module_dvfs_para.u_dvfs_en);
    else
        err = sprintf(buf, "undefined\n");
    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t set_dvfs_enable_store(struct device *dev,
                                     struct device_attribute *attr,
                                     const char *buf, size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    unsigned long dvfs_en;
    int err;

    module = dev_get_drvdata(devfreq->dev.parent);
    if (!module->dvfs_enable) {
        pr_info("dvfs power off\n");
        return -EINVAL;
    }

    mutex_lock(&devfreq->lock);

    err = sscanf(buf, "%lu\n", &dvfs_en);
    if (err != 1) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }
    pr_info("%s: err=%d, dvfs_en=%lu\n", __func__, err, dvfs_en);

    module->module_dvfs_para.u_dvfs_en = dvfs_en;
    module->dvfs_enable = dvfs_en;
    if (module->dvfs_ops != NULL && module->dvfs_ops->ip_hw_dvfs_en != NULL &&
        (err != 0)) {

        module->dvfs_ops->ip_hw_dvfs_en(devfreq, module->module_dvfs_para.u_dvfs_en);
    } else
        pr_err("%s: ip  ops null\n", __func__);;

    err = count;
    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t get_auto_tune_en_show(struct device *dev,
                                     struct device_attribute *attr, char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    int err = 0;

    module = dev_get_drvdata(devfreq->dev.parent);
    if (!module->dvfs_enable) {
        pr_info("dvfs power off\n");
        return -EINVAL;
    }
    mutex_lock(&devfreq->lock);
    if (module != NULL)
        err = sprintf(buf, "%d\n", module->module_dvfs_para.u_auto_tune_en);
    else
        err = sprintf(buf, "undefined\n");
    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t set_auto_tune_en_store(struct device *dev,
                                      struct device_attribute *attr,
                                      const char *buf, size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    unsigned long auto_tune_en;
    int err;

    module = dev_get_drvdata(devfreq->dev.parent);
    if (!module->dvfs_enable) {
        pr_info("dvfs power off\n");
        return -EINVAL;
    }

    mutex_lock(&devfreq->lock);

    err = sscanf(buf, "%lu\n", &auto_tune_en);
    if (err != 1) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }
    module->module_dvfs_para.u_auto_tune_en = auto_tune_en;

    if (module->dvfs_ops != NULL && module->dvfs_ops->ip_auto_tune_en != NULL) {

        module->dvfs_ops->ip_auto_tune_en(devfreq,
                                       module->module_dvfs_para.u_auto_tune_en);
    } else
        pr_err("%s: ip  ops null\n", __func__);;

    err = count;

    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t get_work_freq_show(struct device *dev,
                                  struct device_attribute *attr, char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    int err = 0;

    module = dev_get_drvdata(devfreq->dev.parent);
    mutex_lock(&devfreq->lock);

    if (module != NULL)
        err = sprintf(buf, "%d\n", module->module_dvfs_para.u_work_freq);
    else
        err = sprintf(buf, "undefined\n");

    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t set_work_freq_store(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    unsigned long user_freq;
    int err;

    module = dev_get_drvdata(devfreq->dev.parent);
    if (!module->dvfs_enable) {
        pr_info("dvfs power off\n");
        return -EINVAL;
    }
    mutex_lock(&devfreq->lock);
    err = sscanf(buf, "%lu\n", &user_freq);
    if (err != 1) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }
    pr_info("%s: dvfs freq %lu", __func__, user_freq);
    module->module_dvfs_para.u_work_freq = user_freq;

    err = update_devfreq(devfreq);
    if (err == 0)
        err = count;

    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t get_idle_freq_show(struct device *dev,
                                  struct device_attribute *attr, char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    int err = 0;

    module = dev_get_drvdata(devfreq->dev.parent);
    if (!module->dvfs_enable) {
        pr_info("dvfs power off\n");
        return -EINVAL;
    }
    mutex_lock(&devfreq->lock);
    if (module != NULL)
        err = sprintf(buf, "%d\n", module->module_dvfs_para.u_idle_freq);
    else
        err = sprintf(buf, "undefined\n");
    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t set_idle_freq_store(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    unsigned long idle_freq;
    int err;

    module = dev_get_drvdata(devfreq->dev.parent);
    if (!module->dvfs_enable) {
        pr_info("dvfs power off\n");
        return -EINVAL;
    }
    mutex_lock(&devfreq->lock);
    err = sscanf(buf, "%lu\n", &idle_freq);
    if (err == 0) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }

    pr_info("%s: ip  ops \n", __func__);

    // module->module_dvfs_para.u_idle_freq = idle_freq;
    // err = update_devfreq(devfreq);

    if (module->dvfs_ops != NULL && module->dvfs_ops->set_idle_freq != NULL &&
        (err != 0)) {

        module->module_dvfs_para.u_idle_freq = idle_freq;
        err = module->dvfs_ops->set_idle_freq(devfreq,
                                           module->module_dvfs_para.u_idle_freq);
    } else
        pr_err("%s: ip  ops null\n", __func__);;

    if (err == 0)
        err = count;

    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t get_work_index_show(struct device *dev,
                                   struct device_attribute *attr, char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    int err = 0;

    module = dev_get_drvdata(devfreq->dev.parent);
    if (!module->dvfs_enable) {
        pr_info("dvfs power off\n");
        return -EINVAL;
    }
    mutex_lock(&devfreq->lock);
    if (module != NULL)
        err = sprintf(buf, "%d\n", module->module_dvfs_para.u_work_index);
    else
        err = sprintf(buf, "undefined\n");
    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t set_work_index_store(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    unsigned long work_index;
    int err;

    module = dev_get_drvdata(devfreq->dev.parent);
    if (!module->dvfs_enable) {
        pr_info("dvfs power off\n");
        return -EINVAL;
    }

    mutex_lock(&devfreq->lock);

    err = sscanf(buf, "%lu\n", &work_index);

    if (err != 1) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }
    pr_info("%s: count=%d\n", __func__, (int)count);
    pr_info("%s: ip  ops null,work_index= %lu\n", __func__, work_index);

    if (module->dvfs_ops != NULL &&
        module->dvfs_ops->set_ip_dvfs_work_index != NULL && (err != 0)) {

        module->module_dvfs_para.u_work_index = work_index;
        module->dvfs_ops->set_ip_dvfs_work_index(devfreq,
                                              module->module_dvfs_para.u_work_index);
    } else
        pr_err("%s: ip  ops null\n", __func__);;

    err = count;
    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t get_idle_index_show(struct device *dev,
                                   struct device_attribute *attr, char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    int err = 0;

    module = dev_get_drvdata(devfreq->dev.parent);
    if (!module->dvfs_enable) {
        pr_info("dvfs power off\n");
        return -EINVAL;
    }
    mutex_lock(&devfreq->lock);
    if (module != NULL)
        err = sprintf(buf, "%d\n", module->module_dvfs_para.u_idle_index);
    else
        err = sprintf(buf, "undefined\n");
    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t set_idle_index_store(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    unsigned long idle_index;
    int err;

    module = dev_get_drvdata(devfreq->dev.parent);
    if (!module->dvfs_enable) {
        pr_info("dvfs power off\n");
        return -EINVAL;
    }

    mutex_lock(&devfreq->lock);

    err = sscanf(buf, "%lu\n", &idle_index);
    if (err != 1) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }
    module->module_dvfs_para.u_idle_index = idle_index;

    if (module->dvfs_ops != NULL &&
        module->dvfs_ops->set_ip_dvfs_idle_index != NULL && (err != 0)) {

        module->dvfs_ops->set_ip_dvfs_idle_index(devfreq,
                                              module->module_dvfs_para.u_idle_index);
    } else
        pr_err("%s: ip  ops null\n", __func__);;

    err = count;
    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t get_fix_dvfs_show(struct device *dev,
                                 struct device_attribute *attr, char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    int err = 0;

    module = dev_get_drvdata(devfreq->dev.parent);
    mutex_lock(&devfreq->lock);
    if (module != NULL)
        err = sprintf(buf, "%d\n", module->module_dvfs_para.u_fix_volt);
    else
        err = sprintf(buf, "undefined\n");
    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t set_fix_dvfs_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    unsigned long fix_volt;
    int err;

    module = dev_get_drvdata(devfreq->dev.parent);
    if (!module->dvfs_enable) {
        pr_info("dvfs power off\n");
        return -EINVAL;
    }

    mutex_lock(&devfreq->lock);

    err = sscanf(buf, "%lu\n", &fix_volt);
    if (err != 1) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }
    module->module_dvfs_para.u_fix_volt = fix_volt;

    if (module->dvfs_ops != NULL && module->dvfs_ops->set_fix_dvfs_value != NULL) {

        module->dvfs_ops->set_fix_dvfs_value(devfreq,
                                          module->module_dvfs_para.u_fix_volt);
    } else
        pr_err("%s: ip  ops null\n", __func__);;

    err = count;
    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t get_dvfs_coffe_show(struct device *dev,
                                   struct device_attribute *attr, char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    int len = 0;

    module = dev_get_drvdata(devfreq->dev.parent);
    mutex_lock(&devfreq->lock);

    len = sprintf(buf, "IP_dvfs_coffe_show\n");

    len += sprintf(buf + len, "%d\n", module->module_dvfs_para.ip_coffe.auto_tune);
    len += sprintf(buf + len, "%d\n",
                   module->module_dvfs_para.ip_coffe.freq_upd_delay_en);
    len +=
        sprintf(buf + len, "%d\n", module->module_dvfs_para.ip_coffe.freq_upd_en_byp);
    len += sprintf(buf + len, "%d\n",
                   module->module_dvfs_para.ip_coffe.freq_upd_hdsk_en);
    len += sprintf(buf + len, "%d\n",
                   module->module_dvfs_para.ip_coffe.gfree_wait_delay);
    len +=
        sprintf(buf + len, "%d\n", module->module_dvfs_para.ip_coffe.idle_index_def);
    len +=
        sprintf(buf + len, "%d\n", module->module_dvfs_para.ip_coffe.idle_index_def);
    len +=
        sprintf(buf + len, "%d\n", module->module_dvfs_para.ip_coffe.work_index_def);
    len += sprintf(buf + len, "%d\n", module->module_dvfs_para.ip_coffe.sw_trig_en);

    mutex_unlock(&devfreq->lock);

    return len;
}

 ssize_t set_dvfs_coffe_store(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    struct ip_dvfs_coffe dvfs_coffe;
    int err = 0;

    module = dev_get_drvdata(devfreq->dev.parent);
    if (!module->dvfs_enable) {
        pr_info("dvfs power off\n");
        return -EINVAL;
    }
    mutex_lock(&devfreq->lock);
    err = sscanf(buf, "%d,%d,%d,%d,%d,%d,%d,%d\n", &dvfs_coffe.gfree_wait_delay,
                 &dvfs_coffe.freq_upd_hdsk_en, &dvfs_coffe.freq_upd_delay_en,
                 &dvfs_coffe.freq_upd_en_byp, &dvfs_coffe.sw_trig_en,
                 &dvfs_coffe.auto_tune, &dvfs_coffe.work_index_def,
                 &dvfs_coffe.idle_index_def);
    module->module_dvfs_para.ip_coffe = dvfs_coffe;
    if (err != 8) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }

    if (module->dvfs_ops != NULL && module->dvfs_ops->set_ip_dvfs_coffe != NULL) {
        err = module->dvfs_ops->set_ip_dvfs_coffe(devfreq, &dvfs_coffe);
    } else
        pr_err("%s: ip  ops null\n", __func__);;

    err = count;

    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t get_ip_status_show(struct device *dev,
                                  struct device_attribute *attr, char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    struct ip_dvfs_status ip_status = {0};
    ssize_t len = 0;
    int ret = 0;
    unsigned int top_volt = 0, mm_volt = 0;

    module = dev_get_drvdata(devfreq->dev.parent);
    if (!module->dvfs_enable) {
        pr_info("dvfs power off\n");
        return -EINVAL;
    }

    mutex_lock(&devfreq->lock);
    if (module->dvfs_ops != NULL && module->dvfs_ops->get_ip_status != NULL) {
        ret = module->dvfs_ops->get_ip_status(devfreq, &ip_status);
    } else
        pr_info("%s: dvfs_read_ops is null\n", __func__);

    if (module->dvfs_ops != NULL && module->dvfs_ops->top_current_volt != NULL) {
        ret = module->dvfs_ops->top_current_volt(devfreq, &top_volt);
    } else
        pr_info("%s: dvfs_read_top_volt is null\n", __func__);

    if (module->dvfs_ops != NULL && module->dvfs_ops->mm_current_volt != NULL) {
        ret = module->dvfs_ops->mm_current_volt(devfreq, &mm_volt);
    } else
        pr_info("%s: dvfs_read_top_volt is null\n", __func__);

    len = sprintf(buf, "module_dvfs_read_clk=%d\n", ip_status.current_ip_clk);
    len += sprintf(buf + len, "module_dvfs_read_volt=%d\n",
                   ip_status.current_sys_volt);
    len += sprintf(buf + len, "module_dvfs_read_top_volt=%d\n", top_volt);
    len += sprintf(buf + len, "module_dvfs_read_mm_volt=%d\n", mm_volt);

    mutex_unlock(&devfreq->lock);

    return len;
}

 ssize_t get_ip_status_store(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count) {
    return 0;
}

 ssize_t get_dvfs_table_info_show(struct device *dev,
                                        struct device_attribute *attr,
                                        char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    struct ip_dvfs_map_cfg dvfs_table[8] = {{0}, {0}, {0}, {0},
                                            {0}, {0}, {0}, {0}};
    ssize_t len = 0;
    int err = 0, i = 0;

    module = dev_get_drvdata(devfreq->dev.parent);
    mutex_lock(&devfreq->lock);

    if (module->dvfs_ops != NULL && module->dvfs_ops->get_ip_dvfs_table != NULL) {

        err = module->dvfs_ops->get_ip_dvfs_table(devfreq, dvfs_table);

    } else
        pr_info("%s: ip ops null\n", __func__);

    for (i = 0; i < 8; i++) {
        len += sprintf(buf + len, "volt=%s\n", dvfs_table[i].volt_value);
        len += sprintf(buf + len, "clk=%d\n", dvfs_table[i].clk_freq);
        len += sprintf(buf + len, "map_index=%d\n", dvfs_table[i].map_index);
        len += sprintf(buf + len, "reg_add=%lu\n", dvfs_table[i].reg_add);
        len += sprintf(buf + len, "fdiv_denom=%d\n", dvfs_table[i].fdiv_denom);
        len += sprintf(buf + len, "fdiv_num=%d\n", dvfs_table[i].fdiv_num);
        len += sprintf(buf + len, "axi_index=%d\n", dvfs_table[i].axi_index);
        len += sprintf(buf + len, "mtx_index=%d\n\n", dvfs_table[i].mtx_index);
    }
    mutex_unlock(&devfreq->lock);
    return len;
}

 ssize_t set_dvfs_table_info_store(struct device *dev,
                                         struct device_attribute *attr,
                                         const char *buf, size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    struct ip_dvfs_map_cfg dvfs_table = {0};
    uint32_t map_index;
    int err = 0;

    module = dev_get_drvdata(devfreq->dev.parent);
    mutex_lock(&devfreq->lock);
    /* To do */
    err = sscanf(buf, " %d,%lu,%d,%d,%d,%d,%d,%d,\n", &dvfs_table.map_index,
                 &dvfs_table.reg_add, &dvfs_table.volt, &dvfs_table.clk,
                 &dvfs_table.fdiv_denom, &dvfs_table.fdiv_num,
                 &dvfs_table.axi_index, &dvfs_table.mtx_index);
    if (err != 8) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }

    map_index = dvfs_table.map_index;
    module->module_dvfs_para.ip_dvfs_map[map_index] = dvfs_table;

    if (module->dvfs_ops != NULL && module->dvfs_ops->set_ip_dvfs_table != NULL) {

        err = module->dvfs_ops->set_ip_dvfs_table(devfreq, &dvfs_table);
    } else
        pr_info("%s: ip ops null\n", __func__);

    err = count;

    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t get_gfree_wait_delay_show(struct device *dev,
                                         struct device_attribute *attr,
                                         char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    int err = 0;

    module = dev_get_drvdata(devfreq->dev.parent);
    mutex_lock(&devfreq->lock);
    if (module != NULL) {
        err =
            sprintf(buf, "%d\n", module->module_dvfs_para.ip_coffe.gfree_wait_delay);
    } else
        err = sprintf(buf, "undefined\n");
    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t set_gfree_wait_delay_store(struct device *dev,
                                          struct device_attribute *attr,
                                          const char *buf, size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    unsigned long gfree_wait_delay;
    int err;

    module = dev_get_drvdata(devfreq->dev.parent);
    if (!module->dvfs_enable) {
        pr_info("dvfs power off\n");
	return -EINVAL;
    }

    mutex_lock(&devfreq->lock);

    err = sscanf(buf, " %lu\n", &gfree_wait_delay);
    pr_info("%s:err=%d,gfree_wait_delay=%lu,count=%d", __func__, err,
            gfree_wait_delay, (int)count);

    if (err != 1) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }
    module->module_dvfs_para.ip_coffe.gfree_wait_delay = gfree_wait_delay;

    if (module->dvfs_ops != NULL &&
        module->dvfs_ops->set_ip_gfree_wait_delay != NULL) {

        module->dvfs_ops->set_ip_gfree_wait_delay(
            module->module_dvfs_para.ip_coffe.gfree_wait_delay);
    } else
        pr_info("%s: ip ops null\n", __func__);

    err = count;
    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t set_freq_upd_hdsk_en_store(struct device *dev,
                                          struct device_attribute *attr,
                                          const char *buf, size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    unsigned long freq_upd_hdsk_en;
    int err;

    module = dev_get_drvdata(devfreq->dev.parent);
    if (!module->dvfs_enable) {
        pr_info("dvfs power off\n");
        return -EINVAL;
    }

    mutex_lock(&devfreq->lock);

    err = sscanf(buf, "%lu\n", &freq_upd_hdsk_en);
    if (err != 1) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }
    module->module_dvfs_para.ip_coffe.freq_upd_hdsk_en = freq_upd_hdsk_en;

    if (module->dvfs_ops != NULL &&
        module->dvfs_ops->set_ip_freq_upd_hdsk_en != NULL) {

        module->dvfs_ops->set_ip_freq_upd_hdsk_en(
            module->module_dvfs_para.ip_coffe.freq_upd_hdsk_en);
    } else
        pr_info("%s: ip ops null\n", __func__);

    err = count;
    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t get_freq_upd_hdsk_en_show(struct device *dev,
                                         struct device_attribute *attr,
                                         char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    int err = 0;

    module = dev_get_drvdata(devfreq->dev.parent);
    mutex_lock(&devfreq->lock);
    if (module != NULL) {
        err =
            sprintf(buf, "%d\n", module->module_dvfs_para.ip_coffe.freq_upd_hdsk_en);
    } else
        err = sprintf(buf, "undefined\n");
    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t set_freq_upd_delay_en_store(struct device *dev,
                                           struct device_attribute *attr,
                                           const char *buf, size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    unsigned long freq_upd_delay_en;
    int err;

    module = dev_get_drvdata(devfreq->dev.parent);
    if (!module->dvfs_enable) {
        pr_info("dvfs power off\n");
        return -EINVAL;
    }

    mutex_lock(&devfreq->lock);

    err = sscanf(buf, " %lu\n", &freq_upd_delay_en);
    if (err != 1) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }
    module->module_dvfs_para.ip_coffe.freq_upd_delay_en = freq_upd_delay_en;

    if (module->dvfs_ops != NULL &&
        module->dvfs_ops->set_ip_freq_upd_delay_en != NULL) {

        module->dvfs_ops->set_ip_freq_upd_delay_en(
            module->module_dvfs_para.ip_coffe.freq_upd_delay_en);
    } else
        pr_info("%s: ip ops null\n", __func__);

    err = count;
    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t get_freq_upd_delay_en_show(struct device *dev,
                                          struct device_attribute *attr,
                                          char *buf)

{
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    int err = 0;

    module = dev_get_drvdata(devfreq->dev.parent);
    mutex_lock(&devfreq->lock);
    if (module != NULL) {
        err =
            sprintf(buf, "%d\n", module->module_dvfs_para.ip_coffe.freq_upd_delay_en);
    } else
        err = sprintf(buf, "undefined\n");
    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t set_freq_upd_en_byp_store(struct device *dev,
                                         struct device_attribute *attr,
                                         const char *buf, size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    unsigned long freq_upd_en_byp;
    int err;

    module = dev_get_drvdata(devfreq->dev.parent);
    if (!module->dvfs_enable) {
        pr_info("dvfs power off\n");
        return -EINVAL;
    }

    mutex_lock(&devfreq->lock);

    err = sscanf(buf, " %lu\n", &freq_upd_en_byp);
    if (err != 1) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }
    module->module_dvfs_para.ip_coffe.freq_upd_en_byp = freq_upd_en_byp;

    if (module->dvfs_ops != NULL &&
        module->dvfs_ops->set_ip_freq_upd_en_byp != NULL) {

        module->dvfs_ops->set_ip_freq_upd_en_byp(
            module->module_dvfs_para.ip_coffe.freq_upd_en_byp);
    } else
        pr_info("%s: ip ops null\n", __func__);

    err = count;
    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t get_freq_upd_en_byp_show(struct device *dev,
                                        struct device_attribute *attr,
                                        char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    int err = 0;

    module = dev_get_drvdata(devfreq->dev.parent);
    mutex_lock(&devfreq->lock);
    if (module != NULL) {
        err = sprintf(buf, "%d\n", module->module_dvfs_para.ip_coffe.freq_upd_en_byp);
    } else
        err = sprintf(buf, "undefined\n");
    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t set_sw_trig_en_store(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    unsigned long sw_trig_en;
    int err;

    module = dev_get_drvdata(devfreq->dev.parent);
    if (!module->dvfs_enable) {
        pr_info("dvfs power off\n");
        return -EINVAL;
    }

    mutex_lock(&devfreq->lock);

    err = sscanf(buf, "%lu\n", &sw_trig_en);
    if (err != 1) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }
    module->module_dvfs_para.ip_coffe.freq_upd_en_byp = sw_trig_en;

    if (module->dvfs_ops != NULL && module->dvfs_ops->set_ip_dvfs_swtrig_en != NULL)
        module->dvfs_ops->set_ip_dvfs_swtrig_en(sw_trig_en);
    else
        pr_info("%s: ip ops null\n", __func__);

    err = count;
    mutex_unlock(&devfreq->lock);

    return err;
}

 ssize_t get_sw_trig_en_show(struct device *dev,
                                   struct device_attribute *attr, char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct module_dvfs *module;
    int err = 0;

    module = dev_get_drvdata(devfreq->dev.parent);
    mutex_lock(&devfreq->lock);
    if (module != NULL) {
        err = sprintf(buf, "%d\n", module->module_dvfs_para.ip_coffe.sw_trig_en);
    } else
        err = sprintf(buf, "undefined\n");
    mutex_unlock(&devfreq->lock);

    return err;
}

