/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
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

#ifndef _DISP_TRUSTY_H_
#define _DISP_TRUSTY_H_

int disp_ca_connect(void);
void disp_ca_disconnect(void);
ssize_t disp_ca_read(void *buf, size_t max_len);
ssize_t disp_ca_write(void *buf, size_t len);
int disp_ca_wait_response(void);

#endif
