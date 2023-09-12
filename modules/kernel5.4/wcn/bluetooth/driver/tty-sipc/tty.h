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

#ifndef __STTY_H
#define __STTY_H

#define SPRD_BT_DST		3
#define SPRD_BT_CHANNEL	4
#define SPRD_BT_TX_BUFID	11
#define SPRD_BT_RX_BUFID	10

struct stty_init_data {
	char		*name;
	uint8_t		dst;
	uint8_t		channel;
	uint32_t	tx_bufid;
	uint32_t	rx_bufid;
};

enum mtty_log_level{
	MTTY_LOG_LEVEL_NONE,
	MTTY_LOG_LEVEL_DEBUG,
	MTTY_LOG_LEVEL_VER,
};

#endif
