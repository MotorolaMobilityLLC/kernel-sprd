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

#ifndef _DVFS_MODULES_H
#define _DVFS_MODULES_H

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/devfreq.h>
#include <linux/devfreq-event.h>
#include <linux/interrupt.h>
//#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include "governor.h"
#include "mmsys_dvfs_comm.h"
//#include <mm_dvfs_queue.h>

struct module_dvfs {

    unsigned long dvfs_enable;
    uint32_t dvfs_wait_window;
    uint32_t dvfs_modules_opp_hz;
    uint32_t dvfs_modules_opp_microvolt;
    uint32_t dvfs_modules_opp_microamp;
    uint32_t dvfs_modules_clock_latency;
    unsigned long freq, target_freq;
    unsigned long volt, target_volt;
    unsigned long user_freq;
    struct clk *clk_dvfs_modules_core;
    struct devfreq *devfreq;
    struct devfreq_event_dev *edev;
    struct ip_dvfs_ops *dvfs_ops;
    struct ip_dvfs_para module_dvfs_para;
    set_freq_type user_freq_type;
    struct mutex lock;
    struct notifier_block pw_nb;
};

extern struct ip_dvfs_ops isp_dvfs_ops;
extern struct devfreq_governor isp_dvfs_gov;

extern struct ip_dvfs_ops fd_dvfs_ops;
extern struct devfreq_governor fd_dvfs_gov;

extern struct ip_dvfs_ops cpp_dvfs_ops;
extern struct devfreq_governor cpp_dvfs_gov;

extern struct ip_dvfs_ops dcam_if_dvfs_ops;
extern struct devfreq_governor dcam_if_dvfs_gov;

extern struct ip_dvfs_ops dcam_axi_dvfs_ops;
extern struct devfreq_governor dcam_axi_dvfs_gov;

extern struct ip_dvfs_ops mtx_dvfs_ops;
extern struct devfreq_governor mtx_dvfs_gov;

extern struct ip_dvfs_ops jpg_dvfs_ops;
extern struct devfreq_governor jpg_dvfs_gov;

int dvfs_target(struct device *dev, unsigned long *freq, u32 flags);
int dvfs_get_dev_status(struct device *dev,
                                   struct devfreq_dev_status *stat);
int dvfs_get_cur_freq(struct device *dev, unsigned long *freq);
int dvfs_notify_callback(struct notifier_block *nb,
                                    unsigned long type, void *data);


ssize_t get_dvfs_enable_show(struct device *dev,
                                    struct device_attribute *attr, char *buf);

ssize_t set_dvfs_enable_store(struct device *dev,
                                     struct device_attribute *attr,
                                     const char *buf, size_t count);

ssize_t get_auto_tune_en_show(struct device *dev,
                                     struct device_attribute *attr, char *buf);

ssize_t set_auto_tune_en_store(struct device *dev,
                                      struct device_attribute *attr,
                                      const char *buf, size_t count);

ssize_t get_work_freq_show(struct device *dev,
                                  struct device_attribute *attr, char *buf);

ssize_t set_work_freq_store(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count);

ssize_t get_idle_freq_show(struct device *dev,
                                  struct device_attribute *attr, char *buf);

ssize_t set_idle_freq_store(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count);


ssize_t get_work_index_show(struct device *dev,
                                   struct device_attribute *attr, char *buf);

ssize_t set_work_index_store(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count);

ssize_t get_idle_index_show(struct device *dev,
                                   struct device_attribute *attr, char *buf);

ssize_t set_idle_index_store(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count);

ssize_t get_fix_dvfs_show(struct device *dev,
                                 struct device_attribute *attr, char *buf);

ssize_t set_fix_dvfs_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, size_t count);

ssize_t get_dvfs_coffe_show(struct device *dev,
                                   struct device_attribute *attr, char *buf);

ssize_t set_dvfs_coffe_store(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count);

ssize_t get_ip_status_show(struct device *dev,
                                  struct device_attribute *attr, char *buf);

ssize_t get_ip_status_store(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count) ;
ssize_t get_dvfs_table_info_show(struct device *dev,
                                        struct device_attribute *attr,
                                        char *buf);

ssize_t set_dvfs_table_info_store(struct device *dev,
                                         struct device_attribute *attr,
                                         const char *buf, size_t count);

ssize_t get_gfree_wait_delay_show(struct device *dev,
                                         struct device_attribute *attr,
                                         char *buf);

ssize_t set_gfree_wait_delay_store(struct device *dev,
                                          struct device_attribute *attr,
                                          const char *buf, size_t count);
ssize_t set_freq_upd_hdsk_en_store(struct device *dev,
                                          struct device_attribute *attr,
                                          const char *buf, size_t count);

ssize_t get_freq_upd_hdsk_en_show(struct device *dev,
                                         struct device_attribute *attr,
                                         char *buf);

ssize_t set_freq_upd_delay_en_store(struct device *dev,
                                           struct device_attribute *attr,
                                           const char *buf, size_t count);

ssize_t get_freq_upd_delay_en_show(struct device *dev,
                                          struct device_attribute *attr,
                                          char *buf);

ssize_t set_freq_upd_en_byp_store(struct device *dev,
                                         struct device_attribute *attr,
                                         const char *buf, size_t count);

ssize_t get_freq_upd_en_byp_show(struct device *dev,
                                        struct device_attribute *attr,
                                        char *buf);

ssize_t set_sw_trig_en_store(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count);

ssize_t get_sw_trig_en_show(struct device *dev,
                                   struct device_attribute *attr, char *buf);


#endif
