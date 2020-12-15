/*
 * sprd_debuglog.h -- Sprd Debug Power Log driver support.
 *
 * Copyright (C) 2020, 2021 unisoc.
 *
 * Author: James Chen <Jamesj.Chen@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Debug Power Log Driver Interface.
 */

#ifndef _SPRD_DEBUGLOG_H
#define _SPRD_DEBUGLOG_H

#include <linux/device.h>

/* Debug log event */
struct debug_event {
	int num;
	void *data;
	int (*ph)(struct device *dev, void *data, int num);
};

/* Debug monitor scan function */
struct debug_monitor {
	u32 enable;
	u32 interval;
	struct debug_event event;
};

/* Debug log all function */
struct debug_log {
	struct device *dev;
	struct debug_event *sleep;
	struct debug_event *wakeup;
	struct debug_monitor *monitor;
};

/* Debug log register */
extern int sprd_debug_log_register(struct debug_log *dbg);

/* Debug log unregister */
extern int sprd_debug_log_unregister(void);

#endif /* _SPRD_DEBUGLOG_H */
