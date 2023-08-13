/* SPDX-License-Identifier: GPL-2.0 */
//
// UNISOC APCPU POWER STAT driver
// Copyright (C) 2020 Unisoc, Inc.

#ifndef __SPRD_PDBG_COMM_H__
#define __SPRD_PDBG_COMM_H__

#include <linux/printk.h>
#include <linux/rtc.h>

extern int sprd_pdbg_log_force;

#define SPRD_PDBG_ERR(fmt, ...)  pr_err("[%s] "pr_fmt(fmt), "SPRD_PDBG", ##__VA_ARGS__)
#define SPRD_PDBG_DBG(fmt, ...)  pr_debug("[%s]"pr_fmt(fmt), "SPRD_PDBG", ##__VA_ARGS__)
#define SPRD_PDBG_INFO(fmt, ...)							\
	do {										\
		if (!sprd_pdbg_log_force)						\
			pr_info("[%s] "pr_fmt(fmt), "SPRD_PDBG", ##__VA_ARGS__);	\
		else									\
			pr_err("[%s] "pr_fmt(fmt), "SPRD_PDBG", ##__VA_ARGS__);		\
	} while (0)									\

enum PDBG_R_TYPE {
	PDBG_R_SLP,
	PDBG_R_EB,
	PDBG_R_PD,
	PDBG_R_DCNT,
	PDBG_R_LCNT,
	PDBG_R_LPC,
	PDBG_WS,
	PDBG_INFO_MAX
};

enum {
	SPRD_CPU_PM_ENTER,
	SPRD_CPU_PM_EXIT,
	SPRD_PM_ENTER,
	SPRD_PM_EXIT,
	SPRD_PM_MONITOR
};

#define ERROR_MAGIC           (0xaa99887766554433UL)
#define PDBG_IGNORE_MAGIC     (0xeeddccbbaa998877)
#define PDBG_INFO_NUM         (4)
#define DATA_INVALID          (0xffffffffU)
typedef void (*pdbg_notify_cb)(void *data, unsigned long cmd);

extern void pm_get_active_wakeup_sources(char *pending_wakeup_source, size_t max);
int sprd_pdbg_regs_get_once(u32 info_type, u64 *r_value, u64 *r_value_h);
void sprd_pdbg_time_get(struct rtc_time *time);
int pdbg_notifier_call_chain(unsigned long val, void *priv);
void sprd_pdbg_kernel_active_ws_show(void);
#endif /* __SPRD_PDBG_COMM_H__ */
