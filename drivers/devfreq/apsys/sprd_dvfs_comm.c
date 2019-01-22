/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/export.h>
#include <linux/list.h>
#include <linux/notifier.h>
#include <linux/slab.h>

#include "sprd_dvfs_comm.h"

DEFINE_MUTEX(apsys_glb_reg_lock);

void *dvfs_ops_attach(const char *str, struct list_head *head)
{
	struct ops_list *list;
	const char *ver;

	list_for_each_entry(list, head, head) {
		ver = list->entry->ver;
		if (!strcmp(str, ver))
			return list->entry->ops;
	}

	pr_err("attach dvfs ops %s failed\n", str);

	return NULL;
}
EXPORT_SYMBOL_GPL(dvfs_ops_attach);

int dvfs_ops_register(struct ops_entry *entry, struct list_head *head)
{
	struct ops_list *list;

	list = kzalloc(sizeof(struct ops_list), GFP_KERNEL);
	if (!list)
		return -ENOMEM;

	list->entry = entry;
	list_add(&list->head, head);

	return 0;
}
EXPORT_SYMBOL_GPL(dvfs_ops_register);
