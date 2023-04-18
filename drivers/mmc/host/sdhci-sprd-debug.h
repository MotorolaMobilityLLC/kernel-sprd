/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SDHCI_SPRD_DEBUG_H
#define _SDHCI_SPRD_DEBUG_H

void mmc_debug_update(struct sdhci_host *host, struct mmc_command *cmd, u32 intmask);

#endif

