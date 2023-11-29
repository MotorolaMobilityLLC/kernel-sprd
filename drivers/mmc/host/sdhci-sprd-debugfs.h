/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SDHCI_SPRD_DEBUGFS_H
#define _SDHCI_SPRD_DEBUGFS_H

void sdhci_sprd_add_host_debugfs(struct sdhci_host *host);
void sdhci_sprd_add_host_debug(struct sdhci_host *host);

bool debug_en;

#endif

