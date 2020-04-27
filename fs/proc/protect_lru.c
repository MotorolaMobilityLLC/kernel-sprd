/*
 * Protect lru of task support.It's between normal lru and mlock,
 * that means we will reclaim protect lru pages as late as possible.
 *
 * Copyright (C) 2001-2021, Huawei Tech.Co., Ltd. All rights reserved.
 *
 * Authors:
 * Shaobo Feng <fengshaobo@huawei.com>
 * Xishi Qiu <qiuxishi@huawei.com>
 * Yisheng Xie <xieyisheng1@huawei.com>
 * jing Xia <jing.xia@unisoc.com>
 *
 * This program is free software; you can redistibute it and/or modify
 * it under the t erms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_inline.h>
#include <linux/sched/mm.h>
#include <linux/sched/signal.h>
#include <linux/protect_lru.h>
#include <linux/swap.h>
#include <linux/module.h>
#include "internal.h"

void protect_lruvec_init(struct mem_cgroup *memcg, struct lruvec *lruvec)
{
	enum lru_list lru;
	struct page *page;
	int i;

	if (mem_cgroup_disabled())
		goto populate;

	if (!memcg)
		return;

#ifdef CONFIG_MEMCG
	if (!root_mem_cgroup)
		goto populate;
#endif

	return;

populate:
	lruvec->protect = true;

	for (i = 0; i < PROTECT_HEAD_MAX; i++) {
		for_each_evictable_lru(lru) {
			page = &lruvec->heads[i].protect_page[lru];

			init_page_count(page);
			page_mapcount_reset(page);
			SetPageReserved(page);
			SetPageLRU(page);
			set_page_protect_num(page, i + 1);
			INIT_LIST_HEAD(&page->lru);

			if (lru >= LRU_INACTIVE_FILE)
				list_add_tail(&page->lru, &lruvec->lists[lru]);
		}
		lruvec->heads[i].max_pages = 0;
		lruvec->heads[i].cur_pages = 0;
	}
}

EXPORT_SYMBOL(protect_lruvec_init);
