/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, Unisoc. All rights reserved.
 */

#ifndef _UFS_SPRD_H_
#define _UFS_SPRD_H_

#define UFS_MAX_GENERAL_LUN	8

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
	struct scsi_device *sdev_ufs_lu[UFS_MAX_GENERAL_LUN];
	enum ufs_sprd_caps caps;
	void *ufs_priv_data;

	bool ffu_is_process;

	bool debug_en;
	/* Panic when UFS encounters an error. */
	bool err_panic;

	struct completion pwm_async_done;
	struct completion hs_async_done;
	/* gic enable register address */
	void __iomem *gic_reg_enable;

	/*
	 * Records the IOCTL CMD being executed, used for ums9230 to change pwr mode
	 * to LS during cali.
	 */
	u32 ioctl_status;

	int (*check_stat_after_suspend)(struct ufs_hba *hba,
					enum ufs_notify_change_status);
};

struct syscon_ufs {
	struct regmap *regmap;
	u32 reg;
	u32 mask;
};

#define UFS_PERF_ARRAY_SIZE 12

struct ufs_perf_s {
	u64 start;
	unsigned long r_d2c[UFS_PERF_ARRAY_SIZE];
	unsigned long w_d2c[UFS_PERF_ARRAY_SIZE];
};

/*
 * convert ms to index: (ilog2(ms) + 1)
 * array[6]++ means the time is: 32ms <= time < 64ms
 * [0] [1] [2] [3] [4] [5]  [6]  [7]  [8]   [9]   [10]  [11]
 * 0ms 1ms 2ms 4ms 8ms 16ms 32ms 64ms 128ms 256ms 512ms 1024ms
 */
#define ufs_lat_log(array, fmt, ...) \
	pr_err(fmt ":%5ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld\n", \
		##__VA_ARGS__, array[0], array[1], array[2], array[3], \
		array[4], array[5], array[6], array[7], \
		array[8], array[9], array[10], array[11])

static inline unsigned int ms_to_index(unsigned int ms)
{
	if (!ms)
		return 0;	/* less than 1ms */

	if (ms >= (1 << (UFS_PERF_ARRAY_SIZE - 2)))
		return (UFS_PERF_ARRAY_SIZE - 1);

	return min((UFS_PERF_ARRAY_SIZE - 1), ilog2(ms) + 1);
}


#define AUTO_H8_IDLE_TIME_10MS 0x1001

#define UFSHCI_VERSION_30	0x00000300 /* 3.0 */
#define UFSHCI_VERSION_21	0x00000210 /* 2.1 */

#define UFS_IOCTL_ENTER_MODE    0x5395
#define UFS_IOCTL_AFC_EXIT      0x5396

#define PWM_MODE_VAL    0x22
#define HS_MODE_VAL     0x11

int ufs_sprd_get_syscon_reg(struct device_node *np,
			    struct syscon_ufs *reg, const char *name);
void ufs_sprd_get_gic_reg(struct ufs_hba *hba);
void ufs_sprd_print_gic_reg(struct ufs_hba *hba);

extern const struct ufs_hba_variant_ops ufs_hba_sprd_ums9620_vops;
extern const struct ufs_hba_variant_ops ufs_hba_sprd_ums9621_vops;
extern const struct ufs_hba_variant_ops ufs_hba_sprd_ums9230_vops;

#endif/* _UFS_SPRD_H_ */
