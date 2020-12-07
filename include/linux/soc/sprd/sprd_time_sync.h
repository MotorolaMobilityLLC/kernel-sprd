/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SPRD_TIME_SYNC_H
#define _SPRD_TIME_SYNC_H

#include <linux/notifier.h>

extern int sprd_time_sync_register_notifier(struct notifier_block *nb);

#endif /* _SPRD_TIME_SYNC_H */
