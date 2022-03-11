/*
* SPDX-FileCopyrightText: 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
* SPDX-License-Identifier: GPL-2.0
*
* Copyright 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of version 2 of the GNU General Public License
* as published by the Free Software Foundation.
*/

#ifndef __QOS_H__
#define __QOS_H__

#include "common/qos.h"

/* the pkt send order */
enum qos_index {
	/* other pkts queue (such as arp etc which is not ip packets */
	QOS_OTHER = 0,
	/* AC_VI_Q queue */
	QOS_AC_VO = 1,
	/* AC_VO_Q queue */
	QOS_AC_VI = 2,
	/* AC_BE_Q queue */
	QOS_AC_BE = 3,
	/* AC_BK_Q queue */
	QOS_AC_BK = 4
};

void sc2332_qos_init(struct sprd_qos_t *qos, struct sprd_msg_list *list);
int sc2332_qos_map(unsigned char *frame);
void sc2332_qos_reorder(struct sprd_qos_t *qos);
struct sprd_msg *sc2332_qos_peek_msg(struct sprd_qos_t *qos, int *switch_buf);
void sc2332_qos_update(struct sprd_qos_t *qos, struct sprd_msg *msg,
		       struct list_head *node);
void sc2332_qos_need_resch(struct sprd_qos_t *qos);

#endif
