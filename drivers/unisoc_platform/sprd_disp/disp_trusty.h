// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _DISP_TRUSTY_H_
#define _DISP_TRUSTY_H_

enum disp_command {
	TA_REG_SET = 1,
	TA_REG_CLR,
	TA_FIREWALL_SET,
	TA_FIREWALL_CLR
};

struct layer_reg {
	u32 addr[4];
	u32 ctrl;
	u32 size;
	u32 pitch;
	u32 pos;
	u32 alpha;
	u32 ck;
	u32 pallete;
	u32 crop_start;
};

struct disp_message {
	u8 cmd;
	struct layer_reg layer;
};

int disp_ca_connect(void);
void disp_ca_disconnect(void);
ssize_t disp_ca_read(void *buf, size_t max_len);
ssize_t disp_ca_write(void *buf, size_t len);
int disp_ca_wait_response(void);

#endif