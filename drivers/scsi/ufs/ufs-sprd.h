/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, Unisoc. All rights reserved.
 */

#ifndef _UFS_SPRD_H_
#define _UFS_SPRD_H_

enum ufs_sprd_caps {
	/*
	 * DWC Ufshc waits for the software to read the IS register and clear it,
	 * and then read HCS register. Only when the software has read these registers
	 * in proper sequence (clear IS register and then read HCS register), ufshc
	 * drives LP_pwr_gate to 1.
	 * This capability indicates that access to ufshc is forbidden after
	 * entering H8.
	 */
	UFS_SPRD_CAP_ACC_FORBIDDEN_AFTER_H8_EE         = 1 << 0,
};

struct ufs_sprd_host {
	struct ufs_hba *hba;
	struct scsi_device *sdev_ufs_rpmb;
	enum ufs_sprd_caps caps;
	void *ufs_priv_data;

	bool ffu_is_process;

	bool debug_en;
	/* Panic when UFS encounters an error. */
	bool err_panic;

	u32 times_pre_pwr;
	u32 times_pre_compare_fail;
	u32 times_post_pwr;
	u32 times_post_compare_fail;
	struct completion pwm_async_done;
	struct completion hs_async_done;
};

struct syscon_ufs {
	struct regmap *regmap;
	u32 reg;
	u32 mask;
};

#define AUTO_H8_IDLE_TIME_10MS 0x1001

#define UFSHCI_VERSION_30	0x00000300 /* 3.0 */
#define UFSHCI_VERSION_21	0x00000210 /* 2.1 */

#define UFS_IOCTL_ENTER_MODE    0x5395
#define UFS_IOCTL_AFC_EXIT      0x5396

#define PWM_MODE_VAL    0x22
#define HS_MODE_VAL     0x11

int ufs_sprd_get_syscon_reg(struct device_node *np,
			    struct syscon_ufs *reg, const char *name);

extern const struct ufs_hba_variant_ops ufs_hba_sprd_ums9620_vops;
extern const struct ufs_hba_variant_ops ufs_hba_sprd_ums9621_vops;
extern const struct ufs_hba_variant_ops ufs_hba_sprd_ums9230_vops;

#endif/* _UFS_SPRD_H_ */
