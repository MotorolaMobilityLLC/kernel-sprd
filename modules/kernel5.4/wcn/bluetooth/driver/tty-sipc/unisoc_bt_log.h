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

#ifndef __UNISOC_BT_LOG_H
#define __UNISOC_BT_LOG_H

#include <linux/device.h>
#define dev_unisoc_bt_err(dev, fmt, ...)        \
    do {                                        \
        if (dev) {                              \
            dev_err(dev, fmt, ##__VA_ARGS__);   \
        } else {                                \
            pr_err(fmt, ##__VA_ARGS__);         \
        }                                       \
    } while(0);

#define dev_unisoc_bt_warn(dev, fmt, ...)       \
    do {                                        \
        if (dev) {                              \
            dev_warn(dev, fmt, ##__VA_ARGS__);  \
        } else {                                \
            pr_warn(fmt, ##__VA_ARGS__);        \
        }                                       \
    } while(0);

#define dev_unisoc_bt_info(dev, fmt, ...)       \
    do {                                        \
        if (dev) {                              \
            dev_info(dev, fmt, ##__VA_ARGS__);  \
        } else {                                \
            pr_info(fmt, ##__VA_ARGS__);        \
        }                                       \
    } while(0);

#define dev_unisoc_bt_dbg(dev, fmt, ...)        \
    do {                                        \
        if (dev) {                              \
            ;                                   \
        } else {                                \
            pr_debug(fmt, ##__VA_ARGS__);       \
        }                                       \
    } while(0);

/*#define dev_unisoc_bt_dbg(dev, fmt, ...)        \
    do {                                        \
        if (dev) {                              \
            dev_dbg(dev, fmt, ##__VA_ARGS__);   \
        } else {                                \
            pr_debug(fmt, ##__VA_ARGS__);       \
        }                                       \
    } while(0);*/

#endif
