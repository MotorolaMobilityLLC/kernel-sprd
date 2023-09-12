/*
 * SPDX-FileCopyrightText: 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
 * SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
 *
 * Copyright 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd.
 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */

#ifndef _SPRD_VDSP_MEM_TEST_DEBUG_
#define _SPRD_VDSP_MEM_TEST_DEBUG_
#include "sprd_vdsp_mem_xvpfile.h"
//debug
extern void debug_xvp_buf_show_all(struct xvp *xvp);
extern void debug_xvpfile_buf_show_all(struct xvp_file *xvp_file);
extern void debug_xvp_buf_print(struct xvp_buf *buf);
extern void debug_buffer_print(struct buffer *buf);
extern void debug_check_xvp_buf_leak(struct xvp_file *xvp_file);
extern char *debug_get_ioctl_cmd_name(unsigned int cmd);
extern void debug_print_xrp_ioctl_queue(struct xrp_ioctl_queue *q);
#endif //_SPRD_VDSP_MEM_TEST_DEBUG_
