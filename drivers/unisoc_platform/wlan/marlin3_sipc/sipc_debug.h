/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 * Authors	:
 * baojie.cai <baojie.cai@unisoc.com>
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

#ifndef __SIPC_DEBUG_H__
#define __SIPC_DEBUG_H__

#include "core_sc2355.h"

enum {
	SPRDWL_SIPC_DBG_TX,
	SPRDWL_SIPC_DBG_RX,
	SPRDWL_SIPC_DBG_MEM,
	SPRDWL_SIPC_DBG_CMD_TX,
	SPRDWL_SIPC_DBG_DATA_TX,
	SPRDWL_SIPC_DBG_ALL,
	SPRDWL_SIPC_DBG_MAX
};

void sipc_rx_list_dump(void);
void sipc_tx_list_dump(void);
void sipc_rx_mem_dump(void);
void sipc_tx_mem_dump(void);
void sipc_tx_cmd_test(void);
void sipc_tx_data_test(void);
#endif /*__SIPC_DEBUG_H__*/
