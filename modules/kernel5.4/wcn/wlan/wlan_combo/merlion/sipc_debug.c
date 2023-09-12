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


#include "sprdwl.h"
#include "sipc_debug.h"
#include "sipc_txrx_mm.h"

extern struct sprdwl_intf *g_intf;
void sipc_rx_list_dump(void)
{
	struct sipc_buf_mm *mm = g_intf->sipc_mm->rx_buf;

	if (!mm) {
		wl_err("List is NULL.\n");
		return;
	}

	wl_err("RX List:\n");
	wl_err("FreeList ref: %d\n", atomic_read(&mm->nlist.ref));
	wl_err("BusyList ref: %d\n", atomic_read(&mm->nlist.flow));
	wl_err("\n");
}

void sipc_tx_list_dump(void)
{
	struct sipc_buf_mm *mm = g_intf->sipc_mm->tx_buf;

	if (!mm) {
		wl_err("List is NULL.\n");
		return;
	}

	wl_err("Tx List:\n");
	wl_err("FreeList ref: %d\n", atomic_read(&mm->nlist.ref));
	wl_err("BusyList ref: %d\n", atomic_read(&mm->nlist.flow));
	wl_err("\n");
}

void sipc_rx_mem_dump(void)
{
    /*TODO*/
}

void sipc_tx_mem_dump(void)
{
    /*TODO*/
}

void sipc_tx_cmd_test(void)
{
	 //sprdwl_init_fw(g_dbg_vif);
}

void sipc_tx_data_test(void)
{

	//struct sprdwl_intf *intf = (struct sprdwl_intf *)g_dbg_vif->priv->hw_priv;

	//intf->loopback_n = 0;
	//sprdwl_intf_tx_data_fpga_test(intf, NULL, 0);

}

