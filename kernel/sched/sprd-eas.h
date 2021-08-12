/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __SPRD_EAS_H__
#define __SPRD_EAS_H__

#ifdef CONFIG_SMP
struct pd_cache {
	unsigned long util;
	unsigned long util_est;
	unsigned long util_cfs;
	unsigned long util_irq;
	unsigned long util_rt;
	unsigned long util_dl;
	unsigned long bw_dl;
	unsigned long freq_util;
	unsigned long nrg_util;
};

DECLARE_PER_CPU(struct pd_cache __rcu *, pdc);
#endif

#endif
