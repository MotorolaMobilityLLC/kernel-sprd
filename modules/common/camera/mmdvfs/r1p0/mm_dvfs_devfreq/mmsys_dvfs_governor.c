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

#include "mmsys_dvfs.h"

ssize_t get_mmsys_power_show(struct device *dev, struct device_attribute *attr,
                             char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct mmsys_dvfs *mmsys;
    int err = MM_DVFS_SUCCESS;

    mmsys = dev_get_drvdata(devfreq->dev.parent);
    mutex_lock(&devfreq->lock);
    if (!mmsys->power_ctrl)
        err = sprintf(buf, "%lu\n", mmsys->power_ctrl);
    else
        err = sprintf(buf, "undefined\n");
    mutex_unlock(&devfreq->lock);

    return err;
}

ssize_t set_mmsys_power_store(struct device *dev, struct device_attribute *attr,
                              const char *buf, size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct mmsys_dvfs *mmsys;
    unsigned long power_ctrl;
    int err;

    pr_info("%s: enter mmsys_power_store\n", __func__);
    mmsys = dev_get_drvdata(devfreq->dev.parent);

    mutex_lock(&devfreq->lock);

    err = sscanf(buf, "%lu\n", &power_ctrl);
    if (err != 1) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }

    if (mmsys != NULL) {
        mmsys->power_ctrl = power_ctrl;
        if (mmsys->dvfs_ops != NULL &&
            mmsys->dvfs_ops->mmsys_power_ctrl != NULL)
            mmsys->dvfs_ops->mmsys_power_ctrl(devfreq, power_ctrl);
        else
            pr_err("%s: ip  ops null\n", __func__);;

    } else
        pr_err("%s: ip  ops null\n", __func__);;

    mutex_unlock(&devfreq->lock);

    return count;
}

ssize_t get_sys_status_show(struct device *dev, struct device_attribute *attr,
                            char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct mmsys_dvfs *mmsys;
    struct ip_dvfs_status ip_status;
    ssize_t len = 0;
    mmsys = dev_get_drvdata(devfreq->dev.parent);
    mutex_lock(&devfreq->lock);
    if (mmsys != NULL && mmsys->dvfs_ops != NULL &&
        mmsys->dvfs_ops->mmsys_power_ctrl != NULL &&
        (mmsys->dvfs_ops->get_ip_status(devfreq, &ip_status) > 0)) {

        len = sprintf(buf, "ip "
                           "volt(V):\nmtx_vote\tisp_vote\tfd_"
                           "vote\t\tcpp_vote\tjpg_vote\tdcam_vote\taxi_"
                           "vote\tmm_vote\t\ttop\n");

        len += sprintf(buf + len,
                       "%s\t\t%s\t\t%s\t\t%s\t\t%s\t\t%s\t\t%s\t\t%s\t\t%s\n",
                       ip_status.mtx_vote_volt, ip_status.isp_vote_volt,
                       ip_status.fd_vote_volt, ip_status.cpp_vote_volt,
                       ip_status.jpg_vote_volt, ip_status.dcam_if_vote_volt,
                       ip_status.dcam_axi_vote_volt, ip_status.mm_vote_volt,
                       ip_status.top_volt);

        len += sprintf(
            buf + len,
            "ip clock(Hz):\nmtx\t\tisp\t\tfd\t\tcpp\t\tjpg\t\tdcam\t\taxi\n");

        len += sprintf(buf + len, "%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
                       ip_status.mtx_clk, ip_status.isp_clk, ip_status.fd_clk,
                       ip_status.cpp_clk, ip_status.jpg_clk,
                       ip_status.dcam_if_clk, ip_status.dcam_axi_clk);
    } else {
        len = sprintf(buf, "Error:Get ip status at wrong time.\nCamera is "
                           "power off or ip ops null\n\n");
    }
    mutex_unlock(&devfreq->lock);
    return len;
}

ssize_t set_sys_status_store(struct device *dev, struct device_attribute *attr,
                             const char *buf, size_t count) {
    return MM_DVFS_SUCCESS;
}

ssize_t set_sys_dvfs_hold_en_store(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct mmsys_dvfs *mmsys;
    unsigned long sys_dvfs_hold_en;
    int err;

    mmsys = dev_get_drvdata(devfreq->dev.parent);

    mutex_lock(&devfreq->lock);

    err = sscanf(buf, "%lu\n", &sys_dvfs_hold_en);
    if (err != 1) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }
    mmsys->mmsys_dvfs_para.sys_dvfs_hold_en = sys_dvfs_hold_en;

    if (mmsys->dvfs_ops != NULL &&
        mmsys->dvfs_ops->mmsys_dvfs_hold_en != NULL) {

        mmsys->dvfs_ops->mmsys_dvfs_hold_en(
            devfreq, mmsys->mmsys_dvfs_para.sys_dvfs_hold_en);
    } else
        pr_err("%s: ip  ops null\n", __func__);;

    mutex_unlock(&devfreq->lock);

    return count;
}

ssize_t get_sys_dvfs_hold_en_show(struct device *dev,
                                  struct device_attribute *attr, char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct mmsys_dvfs *mmsys;
    int err = MM_DVFS_SUCCESS;

    mmsys = dev_get_drvdata(devfreq->dev.parent);
    mutex_lock(&devfreq->lock);
    if (!mmsys->mmsys_dvfs_para.sys_dvfs_hold_en)
        err = sprintf(buf, "%d\n", mmsys->mmsys_dvfs_para.sys_dvfs_hold_en);
    else
        err = sprintf(buf, "undefined\n");
    mutex_unlock(&devfreq->lock);

    return err;
}

ssize_t set_sys_sw_dvfs_en_store(struct device *dev,
                                 struct device_attribute *attr, const char *buf,
                                 size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct mmsys_dvfs *mmsys;
    unsigned long sys_sw_dvfs_en;
    int err;

    mmsys = dev_get_drvdata(devfreq->dev.parent);

    mutex_lock(&devfreq->lock);

    err = sscanf(buf, "%lu\n", &sys_sw_dvfs_en);
    if (err != 1) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }
    mmsys->mmsys_dvfs_para.sys_sw_dvfs_en = sys_sw_dvfs_en;

    if (mmsys->dvfs_ops != NULL && mmsys->dvfs_ops->mmsys_sw_dvfs_en != NULL) {

        mmsys->dvfs_ops->mmsys_sw_dvfs_en(
            devfreq, mmsys->mmsys_dvfs_para.sys_sw_dvfs_en);
    } else
        pr_err("%s: ip  ops null\n", __func__);;

    mutex_unlock(&devfreq->lock);

    return count;
}

ssize_t get_sys_sw_dvfs_en_show(struct device *dev,
                                struct device_attribute *attr, char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct mmsys_dvfs *mmsys;
    int err = MM_DVFS_SUCCESS;

    mmsys = dev_get_drvdata(devfreq->dev.parent);
    mutex_lock(&devfreq->lock);
    if (!mmsys->mmsys_dvfs_para.sys_sw_dvfs_en)
        err = sprintf(buf, "%d\n", mmsys->mmsys_dvfs_para.sys_sw_dvfs_en);
    else
        err = sprintf(buf, "undefined\n");
    mutex_unlock(&devfreq->lock);

    return err;
}

ssize_t set_sys_dvfs_clk_gate_ctrl_store(struct device *dev,
                                         struct device_attribute *attr,
                                         const char *buf, size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct mmsys_dvfs *mmsys;
    unsigned long sys_dvfs_clk_gate_ctrl;
    int err;

    mmsys = dev_get_drvdata(devfreq->dev.parent);

    mutex_lock(&devfreq->lock);

    err = sscanf(buf, "%lu\n", &sys_dvfs_clk_gate_ctrl);
    if (err != 1) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }
    mmsys->mmsys_dvfs_para.sys_dvfs_clk_gate_ctrl = sys_dvfs_clk_gate_ctrl;

    if (mmsys->dvfs_ops != NULL &&
        mmsys->dvfs_ops->mmsys_dvfs_clk_gate_ctrl != NULL) {

        mmsys->dvfs_ops->mmsys_dvfs_clk_gate_ctrl(
            devfreq, mmsys->mmsys_dvfs_para.sys_dvfs_clk_gate_ctrl);
    } else
        pr_err("%s: ip  ops null\n", __func__);;

    mutex_unlock(&devfreq->lock);

    return count;
}

ssize_t get_sys_dvfs_clk_gate_ctrl_show(struct device *dev,
                                        struct device_attribute *attr,
                                        char *buf)

{
    struct devfreq *devfreq = to_devfreq(dev);
    struct mmsys_dvfs *mmsys;
    int err = MM_DVFS_SUCCESS;

    mmsys = dev_get_drvdata(devfreq->dev.parent);
    mutex_lock(&devfreq->lock);
    if (!mmsys->mmsys_dvfs_para.sys_dvfs_clk_gate_ctrl)
        err =
            sprintf(buf, "%d\n", mmsys->mmsys_dvfs_para.sys_dvfs_clk_gate_ctrl);
    else
        err = sprintf(buf, "undefined\n");
    mutex_unlock(&devfreq->lock);

    return err;
}

ssize_t set_sys_dvfs_wait_window_store(struct device *dev,
                                       struct device_attribute *attr,
                                       const char *buf, size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct mmsys_dvfs *mmsys;
    unsigned long sys_dvfs_wait_window;
    int err;

    mmsys = dev_get_drvdata(devfreq->dev.parent);

    mutex_lock(&devfreq->lock);

    err = sscanf(buf, "%lu\n", &sys_dvfs_wait_window);
    if (err != 1) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }
    mmsys->mmsys_dvfs_para.sys_dvfs_wait_window = sys_dvfs_wait_window;

    if (mmsys->dvfs_ops != NULL &&
        mmsys->dvfs_ops->mmsys_dvfs_wait_window != NULL) {

        mmsys->dvfs_ops->mmsys_dvfs_wait_window(
            devfreq, mmsys->mmsys_dvfs_para.sys_dvfs_wait_window);
    } else
        pr_err("%s: ip  ops null\n", __func__);;

    mutex_unlock(&devfreq->lock);

    return count;
}

ssize_t get_sys_dvfs_wait_window_show(struct device *dev,
                                      struct device_attribute *attr,
                                      char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct mmsys_dvfs *mmsys;
    int err = MM_DVFS_SUCCESS;

    mmsys = dev_get_drvdata(devfreq->dev.parent);
    mutex_lock(&devfreq->lock);
    if (!mmsys->mmsys_dvfs_para.sys_dvfs_wait_window)
        err = sprintf(buf, "%d\n", mmsys->mmsys_dvfs_para.sys_dvfs_wait_window);
    else
        err = sprintf(buf, "undefined\n");
    mutex_unlock(&devfreq->lock);

    return err;
}

ssize_t set_sys_dvfs_min_volt_store(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct mmsys_dvfs *mmsys;
    unsigned long sys_dvfs_min_volt;
    int err;

    mmsys = dev_get_drvdata(devfreq->dev.parent);

    mutex_lock(&devfreq->lock);

    err = sscanf(buf, "%lu\n", &sys_dvfs_min_volt);
    if (err != 1) {
        mutex_unlock(&devfreq->lock);
        return -EINVAL;
    }
    mmsys->mmsys_dvfs_para.sys_dvfs_min_volt = sys_dvfs_min_volt;

    if (mmsys->dvfs_ops != NULL &&
        mmsys->dvfs_ops->mmsys_dvfs_min_volt != NULL) {

        mmsys->dvfs_ops->mmsys_dvfs_min_volt(
            devfreq, mmsys->mmsys_dvfs_para.sys_dvfs_min_volt);
    } else
        pr_info("%s: ip ops null\n", __func__);

    mutex_unlock(&devfreq->lock);

    return count;
}

ssize_t get_sys_dvfs_min_volt_show(struct device *dev,
                                   struct device_attribute *attr, char *buf) {
    struct devfreq *devfreq = to_devfreq(dev);
    struct mmsys_dvfs *mmsys;
    int err = MM_DVFS_SUCCESS;

    mmsys = dev_get_drvdata(devfreq->dev.parent);
    mutex_lock(&devfreq->lock);
    if (!mmsys->mmsys_dvfs_para.sys_sw_dvfs_en)
        err = sprintf(buf, "%d\n", mmsys->mmsys_dvfs_para.sys_sw_dvfs_en);
    else
        err = sprintf(buf, "undefined\n");
    mutex_unlock(&devfreq->lock);

    return err;
}

/*sys for gov_entries*/
static DEVICE_ATTR(mmsys_power_ctrl, 0644, get_mmsys_power_show,
                   set_mmsys_power_store);
static DEVICE_ATTR(get_sys_info, 0644, get_sys_status_show,
                   set_sys_status_store);
static DEVICE_ATTR(set_sys_sw_dvfs_en, 0644, get_sys_sw_dvfs_en_show,
                   set_sys_sw_dvfs_en_store);
static DEVICE_ATTR(set_sys_dvfs_hold_en, 0644, get_sys_dvfs_hold_en_show,
                   set_sys_dvfs_hold_en_store);
static DEVICE_ATTR(set_sys_dvfs_clk_gate, 0644, get_sys_dvfs_clk_gate_ctrl_show,
                   set_sys_dvfs_clk_gate_ctrl_store);
static DEVICE_ATTR(sys_dvfs_wait_window, 0644, get_sys_dvfs_wait_window_show,
                   set_sys_dvfs_wait_window_store);
static DEVICE_ATTR(set_sys_dvfs_min_volt, 0644, get_sys_dvfs_min_volt_show,
                   set_sys_dvfs_min_volt_store);
static DEVICE_ATTR(get_ip_status, 0644, get_sys_status_show, NULL);

static struct attribute *dev_entries[] = {

    &dev_attr_mmsys_power_ctrl.attr,
    &dev_attr_get_sys_info.attr,
    &dev_attr_set_sys_sw_dvfs_en.attr,
    &dev_attr_get_ip_status.attr,
    NULL,
};

static struct attribute_group dev_attr_group = {
    .name = "mmsys_governor", .attrs = dev_entries,
};

static struct attribute *coeff_entries[] = {

    &dev_attr_set_sys_dvfs_hold_en.attr,
    &dev_attr_set_sys_dvfs_clk_gate.attr,
    &dev_attr_sys_dvfs_wait_window.attr,
    &dev_attr_set_sys_dvfs_min_volt.attr,
    NULL,
};

static struct attribute_group coeff_attr_group = {
    .name = "mmsys_coeff", .attrs = coeff_entries,
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
    int err = MM_DVFS_SUCCESS;

    struct mmsys_dvfs *mmsys = dev_get_drvdata(devfreq->dev.parent);

    mmsys->dvfs_ops = get_ip_dvfs_ops("MMSYS_DVFS_OPS");

    err = sysfs_create_group(&devfreq->dev.kobj, &dev_attr_group);
    err = sysfs_create_group(&devfreq->dev.kobj, &coeff_attr_group);

    return err;
}

static int mmsys_dvfs_gov_get_target(struct devfreq *devfreq,
                                     unsigned long *freq) {
    struct mmsys_dvfs *mmsys = dev_get_drvdata(devfreq->dev.parent);

    pr_info("devfreq_governor-->get_target_freq\n");
    if (!mmsys->mmsys_dvfs_para.sys_sw_dvfs_en) {
        unsigned long adjusted_freq = *freq;

        if (devfreq->max_freq && adjusted_freq > devfreq->max_freq)
            adjusted_freq = devfreq->max_freq;

        if (devfreq->min_freq && adjusted_freq < devfreq->min_freq)
            adjusted_freq = devfreq->min_freq;
        *freq = adjusted_freq;
    } else {
        *freq = devfreq->max_freq; /* No user freq specified yet */
    }

    return MM_DVFS_SUCCESS;
}

static int mmsys_dvfs_gov_event_handler(struct devfreq *devfreq,
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

struct devfreq_governor mmsys_dvfs_gov = {
    .name = "mmsys_dvfs",
    .get_target_freq = mmsys_dvfs_gov_get_target,
    .event_handler = mmsys_dvfs_gov_event_handler,
};
