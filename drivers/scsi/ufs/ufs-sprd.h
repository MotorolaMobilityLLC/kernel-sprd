/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, Unisoc. All rights reserved.
 */

#ifndef _UFS_SPRD_H_
#define _UFS_SPRD_H_

struct ufs_sprd_host {
	struct ufs_hba *hba;
	void *ufs_priv_data;
	const struct sprd_ufs_comm_vops *comm_vops;

	bool ffu_is_process;
};

struct syscon_ufs {
	struct regmap *regmap;
	u32 reg;
	u32 mask;
};

struct sprd_ufs_comm_vops {
	const char *name;
	u32 caps;
	unsigned int quirks;

	int	(*parse_dt)(struct device *dev,
			    struct ufs_hba *hba, struct ufs_sprd_host *host);
	int	(*pre_init)(struct device *dev,
			    struct ufs_hba *hba, struct ufs_sprd_host *host);
	void	(*exit_notify)(struct device *dev,
			       struct ufs_hba *hba, struct ufs_sprd_host *host);
	u32	(*get_ufs_hci_ver)(struct ufs_hba *hba);
	int	(*hce_enable_pre_notify)(struct ufs_hba *hba);
	int	(*hce_enable_post_notify)(struct ufs_hba *hba);
	int	(*link_startup_pre_notify)(struct ufs_hba *hba);
	void	(*hibern8_pre_notify)(struct ufs_hba *hba,
				      enum uic_cmd_dme cmd);
	void    (*hibern8_post_notify)(struct ufs_hba *hba,
				       enum uic_cmd_dme cmd);
	int	(*apply_dev_quirks)(struct ufs_hba *hba);
};

#define AUTO_H8_IDLE_TIME_10MS 0x1001

#define UFSHCI_VERSION_30	0x00000300 /* 3.0 */

int ufs_sprd_plat_priv_data_init(struct device *dev,
				 struct ufs_hba *hba,
				 struct ufs_sprd_host *host);
int ufs_sprd_get_syscon_reg(struct device_node *np,
			    struct syscon_ufs *reg, const char *name);

#endif/* _UFS_SPRD_H_ */
