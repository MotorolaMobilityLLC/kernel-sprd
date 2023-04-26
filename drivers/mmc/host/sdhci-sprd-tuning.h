/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SDHCI_SPRD_TUNING_H_
#define _SDHCI_SPRD_TUNING_H_

void sprd_host_tuning_info_dump(struct sdhci_host *host);
void sprd_host_tuning_info_update_intstatus(struct sdhci_host *host);
void sprd_host_tuning_info_update_index(struct sdhci_host *host, int index);
int mmc_send_tuning_cmd(struct mmc_host *host);
int mmc_send_tuning_read(struct mmc_host *host);

#endif
