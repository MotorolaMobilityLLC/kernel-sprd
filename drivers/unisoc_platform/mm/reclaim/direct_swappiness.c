// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 Unisoc Communications Inc.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <trace/hooks/vmscan.h>
#include <linux/swap.h>

static void tune_swappiness(void *data, int *swappiness)
{

}

void register_direct_swappiness_vendor_hooks(void)
{
	register_trace_android_vh_tune_swappiness(tune_swappiness, NULL);
}

void unregister_direct_swappiness_vendor_hooks(void)
{
	unregister_trace_android_vh_tune_swappiness(tune_swappiness, NULL);
}

int unisoc_enhance_reclaim_init(void)
{
	register_direct_swappiness_vendor_hooks();

	pr_info("UNISOC enhance reclaim init succeed!\n");
	return 0;
}

void unisoc_enhance_reclaim_exit(void)
{
	unregister_direct_swappiness_vendor_hooks();
	pr_info("UNISOC enhance reclaim exit succeed!\n");
}
