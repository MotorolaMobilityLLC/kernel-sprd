/*
 *  FM Drivr for Connectivity chip of Spreadtrum.
 *
 *  FM operations module header.
 *
 *  Copyright (C) 2015 Spreadtrum Company
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 */

#ifndef _FMDRV_OPS_H
#define _FMDRV_OPS_H

extern struct wakeup_source *fm_wakelock;

#define SPRD_FM_DST		3
#define SPRD_FM_TX_CHANNEL	4
#define SPRD_FM_RX_CHANNEL	4
#define SPRD_FM_TX_BUFID	14
#define SPRD_FM_RX_BUFID	13

extern struct fmdrv_ops *fmdev;
int  fm_device_init_driver(void);
void fm_device_exit_driver(void);

struct fm_init_data {
	char		*name;
	uint8_t		dst;
	uint8_t		tx_channel;
	uint8_t		rx_channel;
	uint32_t	tx_bufid;
    uint32_t	rx_bufid;
	uint32_t	lna_gpio;
	uint32_t	ana_inner;
};

#endif
