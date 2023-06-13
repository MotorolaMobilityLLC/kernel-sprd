// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 Unisoc Communications Inc.
 */

#include <linux/module.h>
#include <trace/hooks/mm.h>
#include <trace/hooks/signal.h>
#include <trace/hooks/vmscan.h>
#include <linux/rwsem.h>

static void handle_failed_page_trylock(void *data, struct list_head *page_list)
{
}

static void page_trylock_set(void *data, struct page *page)
{
}

static void page_trylock_clear(void *data, struct page *page)
{
}

static void page_trylock_get_result(void *data, struct page *page, bool *trylock_fail)
{
}

static void do_page_trylock(void *data, struct page *page, struct rw_semaphore *sem,
		bool *got_lock, bool *success)
{
}

int kshrink_lruvec_init(void)
{
	register_trace_android_vh_handle_failed_page_trylock(handle_failed_page_trylock, NULL);
	register_trace_android_vh_page_trylock_set(page_trylock_set, NULL);
	register_trace_android_vh_page_trylock_clear(page_trylock_clear, NULL);
	register_trace_android_vh_page_trylock_get_result(page_trylock_get_result, NULL);
	register_trace_android_vh_do_page_trylock(do_page_trylock, NULL);

	return 0;
}

void kshrink_lruvec_exit(void)
{
	unregister_trace_android_vh_do_page_trylock(do_page_trylock, NULL);
	unregister_trace_android_vh_page_trylock_get_result(page_trylock_get_result, NULL);
	unregister_trace_android_vh_page_trylock_clear(page_trylock_clear, NULL);
	unregister_trace_android_vh_page_trylock_set(page_trylock_set, NULL);
	unregister_trace_android_vh_handle_failed_page_trylock(handle_failed_page_trylock, NULL);
}