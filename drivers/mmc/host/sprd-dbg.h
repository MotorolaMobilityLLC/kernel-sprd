/*
 * linux/drivers/mmc/host/sprd-dbg.c - Secure Digital Host Controller
 * Interface driver debug
 *
 * Copyright (C) 2018 Spreadtrum corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 */
#ifndef __SPRD_MMC_DEUBG__
#define __SPRD_MMC_DEUBG__
extern struct sprd_sdhc_host *host_emmc;
extern void dbg_add_host_log(struct mmc_host *mmc, int type,
		int cmd, int arg);
#endif
