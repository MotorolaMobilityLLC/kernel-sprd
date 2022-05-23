// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc AGDSP power domain driver
 *
 * Copyright (C) 2019 Unisoc, Inc.
 */

#ifndef __SPRD_AGDSP_PD_H__
#define __SPRD_AGDSP_PD_H__
#include <linux/types.h>
#include <linux/mailbox_client.h>

int agdsp_can_access(void);
void agdsp_set_mboxchan(struct mbox_chan *mboxchan);

#endif
