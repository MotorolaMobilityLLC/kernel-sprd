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
 
#include "vdsp_uart.h"

static int uart_enable(struct vdsp_uart_desc *ctx)
{

	return 0;
}

static int uart_disable(struct vdsp_uart_desc *ctx)
{

	return 0;
}

struct vdsp_uart_ops vdsp_uart_ops = {
	.enable = uart_enable,
	.disable = uart_disable,

};

static struct vdsp_uart_desc sub_uart_desc = {
	.ops = &vdsp_uart_ops,
};

struct vdsp_uart_desc *get_vdsp_uart_desc(void)
{
	return &sub_uart_desc;
}
