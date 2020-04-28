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

static unsigned long zero;
static unsigned long one = 1;
unsigned long protect_lru_enable __read_mostly = 1;

static ssize_t protect_level_write(struct file *file, const char __user *buf,
				   size_t count, loff_t *ppos)
{
	char buffer[200];
	struct task_struct *task;
	struct mm_struct *mm;
	int protect_level;
	int err;

	memset(buffer, 0, sizeof(buffer));

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	err = kstrtoint(strstrip(buffer), 0, &protect_level);
	if (err)
		return -EINVAL;

	if (protect_level > PROTECT_HEAD_END || protect_level < 0)
		return -EINVAL;

	task = get_proc_task(file->f_path.dentry->d_inode);
	if (!task)
		return -ESRCH;

	mm = get_task_mm(task);
	if (!mm)
		goto out;

	down_write(&mm->mmap_sem);
	mm->protect = protect_level;
	up_write(&mm->mmap_sem);
	mmput(mm);

out:
	put_task_struct(task);
	return count;
}

static ssize_t protect_level_read(struct file *file, char __user *buf,
				  size_t count,	loff_t *ppos)
{
	struct task_struct *task;
	struct mm_struct *mm;
	char buffer[PROC_NUMBUF];
	int protect_level;
	size_t len;

	task = get_proc_task(file->f_path.dentry->d_inode);
	if (!task)
		return -ESRCH;

	mm = get_task_mm(task);
	if (!mm) {
		put_task_struct(task);
		return -ENOENT;
	}

	protect_level = mm->protect;
	mmput(mm);
	put_task_struct(task);

	len = snprintf(buffer, sizeof(buffer), "%d\n", protect_level);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

const struct file_operations proc_protect_level_operations = {
	.write	= protect_level_write,
	.read	= protect_level_read,
	.llseek = noop_llseek,
};

struct ctl_table protect_lru_table[] = {
	{
		.procname	= "protect_lru_enable",
		.data		= &protect_lru_enable,
		.maxlen		= sizeof(unsigned long),
		.mode		= 0640,
		.proc_handler	= proc_doulongvec_minmax,
		.extra1		= &zero,
		.extra2		= &one,
	},
	{},
};

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
