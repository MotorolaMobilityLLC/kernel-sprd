// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 Unisoc Communications Inc.
 */

#define pr_fmt(fmt) "unisoc_kshrink_slabd: " fmt

#include <linux/module.h>
#include <trace/hooks/vmscan.h>
#include <linux/types.h>
#include <linux/gfp.h>

static void should_shrink_async(void *data, gfp_t gfp_mask, int nid,
			struct mem_cgroup *memcg, int priority, bool *bypass)
{
}

static int register_shrink_slab_async_vendor_hooks(void)
{
	int ret = 0;

	ret = register_trace_android_vh_shrink_slab_bypass(should_shrink_async, NULL);
	if (ret != 0) {
		pr_err("register_trace_android_vh_shrink_slab_bypass failed! ret=%d\n", ret);
		goto out;
	}
out:
	return ret;
}

static void unregister_shrink_slab_async_vendor_hooks(void)
{
	unregister_trace_android_vh_shrink_slab_bypass(should_shrink_async, NULL);
}

int kshrink_slabd_async_init(void)
{
	int ret = 0;

	ret = register_shrink_slab_async_vendor_hooks();
	if (ret != 0)
		return ret;

	pr_info("kshrink_slabd_async succeed!\n");
	return 0;
}

void kshrink_slabd_async_exit(void)
{
	unregister_shrink_slab_async_vendor_hooks();

	pr_info("kshrink_slabd_async exit succeed!\n");
}

