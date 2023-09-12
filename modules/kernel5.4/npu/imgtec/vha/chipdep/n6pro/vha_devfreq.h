// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Unisoc Inc.
 */

#ifndef __VHA_DEVFREQ_H_
#define __VHA_DEVFREQ_H_

#ifdef VHA_DEVFREQ
void vha_devfreq_suspend(struct device *dev);
void vha_devfreq_resume(struct device *dev);
int vha_dvfs_ctx_init(struct device *dev);
int vha_dvfs_ctx_deinit(struct device *dev);
int vha_devfreq_init(struct vha_dev *vha);
void vha_devfreq_term(struct vha_dev *vha);
void vha_update_dvfs_state(struct vha_dev *vha, bool vha_active);
int vha_set_max_freq(int max);
#endif

#endif
