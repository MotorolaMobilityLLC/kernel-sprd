/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/notifier.h>

static BLOCKING_NOTIFIER_HEAD(e_show_mem_notify_list);

int register_e_show_mem_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&e_show_mem_notify_list, nb);
}
EXPORT_SYMBOL_GPL(register_e_show_mem_notifier);

int unregister_e_show_mem_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&e_show_mem_notify_list, nb);
}
EXPORT_SYMBOL_GPL(unregister_e_show_mem_notifier);

