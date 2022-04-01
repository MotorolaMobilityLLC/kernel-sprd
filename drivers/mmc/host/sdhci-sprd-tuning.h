// SPDX-License-Identifier: GPL-2.0
#ifndef _SDHCI_SPRD_TUNING_H_
#define _SDHCI_SPRD_TUNING_H_

int mmc_send_tuning_cmd(struct mmc_host *host);
int mmc_send_tuning_read(struct mmc_host *host);

#endif