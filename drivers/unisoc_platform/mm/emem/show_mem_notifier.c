// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Unisoc Communications Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/android_debug_symbols.h>
#include <linux/mm.h>
#include <linux/mm/emem.h>
#include <linux/module.h>
#include <linux/swap.h>

void unisoc_enhanced_show_mem(void)
{
	struct sysinfo si;
	void (*fun)(unsigned int filter, nodemask_t *nodemask);

	pr_info("Enhanced Mem-Info:E_SHOW_MEM_ALL\n");
	fun = android_debug_symbol(ADS_SHOW_MEM);
	if (fun != NULL)
		(*fun)(0, NULL);
	si_meminfo(&si);
	pr_info("MemTotal:       %8lu kB\n"
		"Buffers:        %8lu kB\n"
		"SwapCached:     %8lu kB\n",
		(si.totalram) << (PAGE_SHIFT - 10),
		(si.bufferram) << (PAGE_SHIFT - 10),
		total_swapcache_pages() << (PAGE_SHIFT - 10));
}
