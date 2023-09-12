/*
 * SPDX-FileCopyrightText: 2019-2022 Unisoc (Shanghai) Technologies Co., Ltd
 * SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
 *
 * Copyright 2019-2022 Unisoc (Shanghai) Technologies Co., Ltd.
 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */

#include <linux/hashtable.h>
#include <linux/kthread.h>
#include "dvfs_adpt.h"
#include "vdsp_dvfs.h"
#include "vdsp_hw.h"
#include "xrp_internal.h"
#include "xvp_main.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: dvfs %d: %d %s:" \
	fmt, current->pid, __LINE__, __func__


static vdsp_power_level convert_to_power_level(int32_t data)
{
	switch (data) {
	case -1:
		return VDSP_PWR_DVFS;
	case 0:
		return VDSP_PWR_MIN;
	case 1:
		return VDSP_PWR_2;
	case 2:
		return VDSP_PWR_3;
	case 3:
		return VDSP_PWR_4;
	case 4:
		return VDSP_PWR_5;
	case 5:
		return VDSP_PWR_MAX;
	default:
		return VDSP_PWR_MAX;
	}
}

static void dvfs_enable(void *hw_arg)
{
	struct dvfs_adpt_ops *adpt = get_dvfs_adpt_ops();
	adpt->enable_adpt(hw_arg);
}

static void dvfs_disable(void *hw_arg)
{
	struct dvfs_adpt_ops *adpt = get_dvfs_adpt_ops();
	adpt->disable_adpt(hw_arg);
}
static void dvfs_set(void *hw_arg, uint32_t level)
{
	struct dvfs_adpt_ops *adpt = get_dvfs_adpt_ops();

	adpt->setdvfs_adpt(level);
}

static void dvfs_strategy(uint32_t *level, uint32_t max_level,
	uint32_t percent, uint32_t last_percent)
{
	struct dvfs_adpt_ops *adpt = get_dvfs_adpt_ops();

	adpt->strategy_adpt(level, max_level, percent, last_percent);
}

static void dvfs_powerhint_record(struct file *filp, vdsp_power_level level,
	vdsp_powerhint_flag flag)
{
	struct xvp_file *xvp_file = (struct xvp_file*)filp->private_data;

	if (flag == POWERHINT_ENABLE) {
		xvp_file->powerhint.record[level]++;
	} else if (flag == POWERHINT_DISABLE) {
		if (unlikely(xvp_file->powerhint.record[level] == 0)) {
			pr_warn("power hint record already 0, more disable\n");
		} else {
			xvp_file->powerhint.record[level]--;
		}
	} else {
		pr_err("invalid parameters:%d\n", flag);
	}
	pr_debug("flag:%d, level:%d, count:%d\n", flag, level, xvp_file->powerhint.record[level]);
}

/*
*	only check VDSP_PWR_MIN->VDSP_PWR_MAX whether exit?, get the max one.
*	if no reocrd, return VDSP_PWR_DVFS, auto dvfs.
*
*/
static vdsp_power_level dvfs_max_record_power_level(struct file *filp)
{
	int32_t i;
	unsigned long bkt;
	struct xvp *xvp = ((struct xvp_file*)(filp->private_data))->xvp;
	struct xvp_file * tmp_xvpfile;
	struct xrp_known_file *p;
	vdsp_power_level max_level = VDSP_PWR_DVFS;
	vdsp_power_level tmp_level;

	/*check every file*/
	mutex_lock(&xvp->xrp_known_files_lock);
	hash_for_each(xvp->xrp_known_files, bkt, p, node) {
		tmp_xvpfile = (struct xvp_file*)(((struct file*)(p->filp))->private_data);
		tmp_level = VDSP_PWR_DVFS;
		for (i = VDSP_PWR_MAX; i >= VDSP_PWR_MIN; i--) {
			if (tmp_xvpfile->powerhint.record[i] != 0) {
				tmp_level = i;
				break;
			}
		}
		if (tmp_level > max_level) {
			max_level = tmp_level;
		}
	}
	mutex_unlock(&xvp->xrp_known_files_lock);

	pr_debug("max record power level:%d\n", max_level);
	return max_level;
}

/*translate powerhint flag fromuser*/
static vdsp_powerhint_flag convert_to_powerhint_flag(uint32_t flag)
{
	switch (flag)
	{
	case 0:
		return POWERHINT_ENABLE;
	case 1:
		return POWERHINT_DISABLE;
	default:
		return POWERHINT_INVALID;
	}
}

static void dvfs_preprocess(void *data)
{
	struct xvp* xvp = (struct xvp*)data;

	mutex_lock(&xvp->dvfs_info.dvfs_lock);
	if (unlikely(xvp->dvfs_info.dvfs_init != 1)) {
		pr_warn("dvfs init:%d\n", xvp->dvfs_info.dvfs_init);
		mutex_unlock(&xvp->dvfs_info.dvfs_lock);
		return;
	}
	mutex_lock(&xvp->dvfs_info.timepiece_lock);
	if (0 == xvp->dvfs_info.count)
		xvp->dvfs_info.piece_starttime = ktime_get();

	xvp->dvfs_info.count++;
	mutex_unlock(&xvp->dvfs_info.timepiece_lock);
	mutex_unlock(&xvp->dvfs_info.dvfs_lock);
}

static void dvfs_postprocess(void *data)
{
	ktime_t realstarttime;
	struct xvp *xvp = (struct xvp*)data;

	mutex_lock(&xvp->dvfs_info.dvfs_lock);
	if (unlikely(xvp->dvfs_info.dvfs_init != 1)) {
		pr_warn("dvfs init:%d\n", xvp->dvfs_info.dvfs_init);
		mutex_unlock(&xvp->dvfs_info.dvfs_lock);
		return;
	}
	mutex_lock(&xvp->dvfs_info.timepiece_lock);
	xvp->dvfs_info.count--;
	if (0 == xvp->dvfs_info.count) {
		realstarttime = (ktime_compare(xvp->dvfs_info.piece_starttime, xvp->dvfs_info.starttime) > 0)
			? xvp->dvfs_info.piece_starttime : xvp->dvfs_info.starttime;
		xvp->dvfs_info.totaltime += ktime_sub(ktime_get(), realstarttime);
	}
	pr_debug("count:%d", xvp->dvfs_info.count);
	mutex_unlock(&xvp->dvfs_info.timepiece_lock);
	mutex_unlock(&xvp->dvfs_info.dvfs_lock);
}

static int32_t dvfs_set_powerhint(void *data, int32_t power, uint32_t flag)
{
	struct file *filp = (struct file*)data;
	struct xvp *xvp = ((struct xvp_file*)(filp->private_data))->xvp;
	vdsp_power_level user_level, max_level;
	vdsp_powerhint_flag user_flag;

	/* 1. user input data check and convert */
	pr_debug("[IN]power:%d, flag:%d\n", power, flag);
	user_level = convert_to_power_level(power);
	user_flag = convert_to_powerhint_flag(flag);

	if (unlikely(user_flag == POWERHINT_INVALID)){
		pr_err("invalid power hint flag[%d]\n", user_flag);
		return -EINVAL;
	}

	/* 2. dvfs status check, whether init before */
	mutex_lock(&xvp->dvfs_info.dvfs_lock);
	if (unlikely(xvp->dvfs_info.dvfs_init != 1)) {
		pr_warn("dvfs not init:%d\n", xvp->dvfs_info.dvfs_init);
		mutex_unlock(&xvp->dvfs_info.dvfs_lock);
		return -ESRCH;
	}
	mutex_lock(&xvp->dvfs_info.powerhint_lock);

	/* 3. get user dvfs set level max one, first store, then foreach */
	dvfs_powerhint_record(filp, user_level, user_flag);
	max_level = dvfs_max_record_power_level(filp);
	pr_debug("flag:%d, user level:%d, max level:%d, last level:%d\n",
		flag, user_level, max_level, xvp->dvfs_info.last_powerhint_level);

	/* 4. if fixed freq set (max_level != dvfs )
	*     and current freq is not the expected , then directly set freq&voltage
	*     else dynamic freq, store power level before.
	*/
	if ((max_level != VDSP_PWR_DVFS) && (xvp->dvfs_info.last_powerhint_level != max_level)) {
		pr_debug("set dvfs level:%d\n", max_level);
		dvfs_set(xvp->hw_arg, max_level);
		xvp->dvfs_info.last_powerhint_level = max_level;

	} else if (max_level == VDSP_PWR_DVFS){
		xvp->dvfs_info.last_dvfs_level = xvp->dvfs_info.last_powerhint_level;
		xvp->dvfs_info.last_powerhint_level = max_level;
	}
	mutex_unlock(&xvp->dvfs_info.powerhint_lock);
	mutex_unlock(&xvp->dvfs_info.dvfs_lock);
	pr_debug("exit\n");
	return 0;
}

static uint32_t calculate_vdsp_usage(void *data, ktime_t fromtime)
{
	int32_t percent;
	ktime_t current_time = ktime_get();
	struct xvp * xvp = (struct xvp*)data;

	mutex_lock(&xvp->dvfs_info.timepiece_lock);
	if (xvp->dvfs_info.count != 0) {
		/*now some piece may executing*/
		if (ktime_compare(xvp->dvfs_info.piece_starttime, fromtime) <= 0)
			xvp->dvfs_info.totaltime = ktime_sub(current_time, fromtime);
		else
			xvp->dvfs_info.totaltime += ktime_sub(current_time, xvp->dvfs_info.piece_starttime);
	}
	percent = (xvp->dvfs_info.totaltime * 100) / ktime_sub(current_time, fromtime);
	pr_debug("totaltime:%d ms, timeeclapse:%d ms, percent:%d",
		(int)(xvp->dvfs_info.totaltime / 1000000),
		(int)(ktime_sub(current_time, fromtime) / 1000000),
		percent);
	xvp->dvfs_info.starttime = ktime_get();
	xvp->dvfs_info.totaltime = 0;
	mutex_unlock(&xvp->dvfs_info.timepiece_lock);
	return percent;
}

static vdsp_power_level dvfs_stragegy_power_level(struct xvp* xvp, uint32_t percent)
{
	static uint32_t last_percent = 0;
	vdsp_power_level level = VDSP_PWR_MIN;
	vdsp_power_level max_level = VDSP_PWR_MAX;

	dvfs_strategy((uint32_t *)&level, (uint32_t)max_level, percent, last_percent);
	last_percent = percent;

	return level;
}

/* dvfs theard : only work in dvfs on state, (last_powerhint_level == VDSP_PWR_DVFS) */
static int vdsp_dvfs_thread(void* data)
{
	uint32_t percentage;
	uint32_t firstcycle = 1;
	long ret = 0;
	vdsp_power_level level;
	struct xvp *xvp = (struct xvp *)data;

	while (!kthread_should_stop()) {
		mutex_lock(&xvp->dvfs_info.powerhint_lock);
		if (VDSP_PWR_DVFS == xvp->dvfs_info.last_powerhint_level) {
			percentage = calculate_vdsp_usage(xvp, xvp->dvfs_info.starttime);
			if (firstcycle == 1) {
				level = VDSP_PWR_MAX;
				firstcycle = 0;
			} else {
				level = dvfs_stragegy_power_level(xvp, percentage);
			}
			pr_debug("percentage:%d, power level:%d, last level:%d\n",
				percentage, level, xvp->dvfs_info.last_dvfs_level);
			if (level != xvp->dvfs_info.last_dvfs_level) {
				dvfs_set(xvp->hw_arg, level);
				xvp->dvfs_info.last_dvfs_level = level;
			}
		}
		mutex_unlock(&xvp->dvfs_info.powerhint_lock);
		ret = wait_event_interruptible_timeout(xvp->dvfs_info.wait_q,
			kthread_should_stop(), msecs_to_jiffies(1000));
		pr_debug("wait_event_interruptible_timeout ret:%ld\n", ret);
	}
	pr_debug("dvfs exit\n");
	return 0;
}

static int dvfs_init(void *data)
{
	struct xvp* xvp = (struct xvp*)data;

	mutex_lock(&(xvp->dvfs_info.dvfs_lock));
	mutex_init(&xvp->dvfs_info.timepiece_lock);
	mutex_init(&xvp->dvfs_info.powerhint_lock);
	init_waitqueue_head(&xvp->dvfs_info.wait_q);

	xvp->dvfs_info.starttime = ktime_get();
	xvp->dvfs_info.totaltime = 0;
	xvp->dvfs_info.piece_starttime = 0;
	xvp->dvfs_info.last_powerhint_level = VDSP_PWR_DVFS;
	xvp->dvfs_info.last_dvfs_level = 0;

	/*when open init to max freq*/
	dvfs_set(xvp->hw_arg, VDSP_PWR_MAX);
	xvp->dvfs_info.last_dvfs_level = VDSP_PWR_MAX;

	xvp->dvfs_info.dvfs_thread = kthread_run(vdsp_dvfs_thread, xvp, "vdsp_dvfs_thread");
	if (IS_ERR(xvp->dvfs_info.dvfs_thread)) {
		pr_err("kthread_run err\n");
		mutex_unlock(&(xvp->dvfs_info.dvfs_lock));
		return -1;
	}
	xvp->dvfs_info.dvfs_init = 1;

	mutex_unlock(&(xvp->dvfs_info.dvfs_lock));
	return 0;
}

static void dvfs_deinit(void *data)
{
	struct xvp* xvp = (struct xvp*)data;

	mutex_lock(&(xvp->dvfs_info.dvfs_lock));
	if (xvp->dvfs_info.dvfs_thread) {
		kthread_stop(xvp->dvfs_info.dvfs_thread);
		xvp->dvfs_info.dvfs_thread = NULL;
		pr_debug("kthread_stop\n");
	}
	dvfs_set(xvp->hw_arg, VDSP_PWR_MIN);
	mutex_destroy(&xvp->dvfs_info.timepiece_lock);
	mutex_destroy(&xvp->dvfs_info.powerhint_lock);
	xvp->dvfs_info.dvfs_init = 0;
	mutex_unlock(&(xvp->dvfs_info.dvfs_lock));

	pr_debug("dvfs deinit exit\n");

	return;
}

static void dvfs_release_powerhint(void *data)
{
	int32_t i;
	vdsp_power_level max_level;
	struct file *filp = (struct file*) data;
	struct xvp_file *xvp_file = (struct xvp_file*)filp->private_data;
	struct xvp *xvp = ((struct xvp_file*)(filp->private_data))->xvp;

	mutex_lock(&(xvp->dvfs_info.dvfs_lock));

	/* 1. dvfs init status check */
	if (xvp->dvfs_info.dvfs_init != 1) {
		pr_warn("dvfs init:%d\n", xvp->dvfs_info.dvfs_init);
		mutex_unlock(&(xvp->dvfs_info.dvfs_lock));
		return;
	}

	/* 2. check every file */
	mutex_lock(&xvp->dvfs_info.powerhint_lock);

	/* 2.1 init current file*/
	for (i = 0; i < VDSP_PWR_NUM; i++)
		xvp_file->powerhint.record[i] = 0;

	/* 2.2 set other file, max reocrd power */
	max_level = dvfs_max_record_power_level(filp);
	if (max_level == VDSP_PWR_DVFS) {
		/*restore dvfs*/
		xvp->dvfs_info.last_dvfs_level = xvp->dvfs_info.last_powerhint_level;
		xvp->dvfs_info.last_powerhint_level = VDSP_PWR_DVFS;
	} else if (max_level != xvp->dvfs_info.last_powerhint_level) {
		dvfs_set(xvp->hw_arg, max_level);
		xvp->dvfs_info.last_powerhint_level = max_level;
	}
	mutex_unlock(&xvp->dvfs_info.powerhint_lock);
	mutex_unlock(&(xvp->dvfs_info.dvfs_lock));
	return;
}

static struct vdsp_dvfs_ops vdsp_dvfs_ops = {
	.init = dvfs_init,
	.deinit = dvfs_deinit,
	.set_powerhint = dvfs_set_powerhint,
	.preprocess = dvfs_preprocess,
	.postprocess = dvfs_postprocess,
	.release_powerhint = dvfs_release_powerhint,
	.setdvfs = dvfs_set,
	.enable = dvfs_enable,
	.disable = dvfs_disable,
};

static struct vdsp_dvfs_desc sub_dvfs_desc = {
	.ops = &vdsp_dvfs_ops,
};

struct vdsp_dvfs_desc *get_vdsp_dvfs_desc(void)
{
	return &sub_dvfs_desc;
}

