// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Unisoc, Inc.
 */

#include "walt.h"

#define TOPAPP_INIT_LOAD_PCT	40

static inline struct walt_task_group *get_task_group(struct task_struct *p)
{
	struct cgroup_subsys_state *css;
	struct task_group *tg;
        struct walt_task_group *wtg;

	css = task_css(p, cpu_cgrp_id);
        if (!css) {
                return NULL;
        }
        tg = container_of(css, struct task_group, css);
        wtg = (struct walt_task_group *) tg->android_vendor_data1;

	return wtg;
}

void walt_init_tg(struct task_group *tg)
{
	struct walt_task_group *wtg;

	wtg = (struct walt_task_group *) tg->android_vendor_data1;

	wtg->boost = 0;
	wtg->account_wait_time= 1;
	wtg->init_task_load_pct = 0;
	wtg->prefer_active = 0;
}

void walt_init_topapp_tg(struct task_group *tg)
{
        struct walt_task_group *wtg;

        wtg = (struct walt_task_group *) tg->android_vendor_data1;

	wtg->boost = 0;
	wtg->account_wait_time= 1;
	wtg->init_task_load_pct = TOPAPP_INIT_LOAD_PCT;
	wtg->prefer_active = 0;
}

u32 tg_init_load_pct(struct task_struct *p)
{
	struct walt_task_group *wtg;
	u32 init_task_load_pct;

	/* Get init_task_load_pct value */
	rcu_read_lock();
	wtg = get_task_group(p);
	if (!wtg) {
		rcu_read_unlock();
		return 0;
	}
	init_task_load_pct = wtg->init_task_load_pct;
	rcu_read_unlock();

	return init_task_load_pct;
}

unsigned int tg_account_wait_time(struct task_struct *p)
{
	struct walt_task_group *wtg;
	unsigned int account_wait_time;

	/* Get init_task_load_pct value */
	rcu_read_lock();
	wtg = get_task_group(p);
	if (!wtg) {
		rcu_read_unlock();
		return 0;
	}
	account_wait_time = wtg->account_wait_time;
	rcu_read_unlock();

	return account_wait_time;
}

