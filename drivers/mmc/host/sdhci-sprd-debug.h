/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SDHCI_SPRD_DEBUG_H
#define _SDHCI_SPRD_DEBUG_H

struct mmc_debug_info;

void mmc_debug_update(struct sdhci_host *host, struct mmc_command *cmd, u32 intmask);
void mmc_get_debug_info(u8 index, struct mmc_debug_info *info);

#endif

