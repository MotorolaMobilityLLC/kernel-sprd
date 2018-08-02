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
#ifndef _GSP_DEBUG_H
#define _GSP_DEBUG_H

#include <linux/printk.h>

#define GSP_TAG "[gsp]"

#define GSP_DEBUG(fmt, ...) \
	pr_debug(GSP_TAG  " %s()-" pr_fmt(fmt), __func__, ##__VA_ARGS__)

#define GSP_ERR(fmt, ...) \
	pr_err(GSP_TAG  " %s()-" pr_fmt(fmt), __func__, ##__VA_ARGS__)

#define GSP_INFO(fmt, ...) \
	pr_info(GSP_TAG  "%s()-" pr_fmt(fmt), __func__, ##__VA_ARGS__)

#define GSP_DUMP(fmt, ...) \
	pr_info(GSP_TAG  "%s()-" pr_fmt(fmt), __func__, ##__VA_ARGS__)

#define GSP_WARN(fmt, ...) \
	pr_warn(GSP_TAG  "%s()-" pr_fmt(fmt), __func__, ##__VA_ARGS__)

#endif
