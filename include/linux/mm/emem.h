/* SPDX-License-Identifier: GPL-2.0*/

#ifndef __ENHANCE_MEMINFO
#define __ENHANCE_MEMINFO

void unisoc_enhanced_show_mem(void);
int register_unisoc_show_mem_notifier(struct notifier_block *nb);
int unregister_unisoc_show_mem_notifier(struct notifier_block *nb);

#endif
