/* SPDX-License-Identifier: GPL-2.0
 *
 * UNISOC APCPU POWER DEBUG driver
 *
 * Copyright (C) 2020 Unisoc, Inc.
 */

#ifndef __SPRD_PDBG_DRV_H__
#define __SPRD_PDBG_DRV_H__

enum {
	SPRD_PDBG_WS_DOMAIN_ID_GIC,
	SPRD_PDBG_WS_DOMAIN_ID_GPIO,
	SPRD_PDBG_WS_DOMAIN_ID_ANA,
	SPRD_PDBG_WS_DOMAIN_ID_ANA_EIC,
	SPRD_PDBG_WS_DOMAIN_ID_AP_EIC_DBNC,
	SPRD_PDBG_WS_DOMAIN_ID_AP_EIC_LATCH,
	SPRD_PDBG_WS_DOMAIN_ID_AP_EIC_ASYNC,
	SPRD_PDBG_WS_DOMAIN_ID_AP_EIC_SYNC,
	SPRD_PDBG_WS_DOMAIN_ID_MAX,
};

enum {
	PDBG_R_SLP,
	PDBG_R_EB,
	PDBG_R_PD,
	PDBG_R_DCNT,
	PDBG_R_LCNT,
	PDBG_R_LPC,
	PDBG_WS,
	PDBG_INFO_MAX
};

extern void pm_get_active_wakeup_sources(char *pending_wakeup_source, size_t max);

#if IS_ENABLED(CONFIG_SPRD_POWER_DEBUG)
void sprd_pdbg_msg_print(const char *format, ...);
#else
void sprd_pdbg_msg_print(const char *format, ...) { }
#endif//CONFIG_SPRD_POWER_DEBUG

#endif /* __SPRD_PDBG_DRV_H__ */
