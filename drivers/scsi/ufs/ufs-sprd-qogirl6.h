/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, Unisoc. All rights reserved.
 */

#ifndef _UFS_SPRD_QOGIRL6_H_
#define _UFS_SPRD_QOGIRL6_H_
#include <linux/bits.h>
#include <linux/sprd_soc_id.h>

struct ufs_sprd_ums9230_data {
	void __iomem *ufs_analog_reg;
	void __iomem *aon_apb_reg;
	struct syscon_ufs aon_apb_ufs_en;
	struct syscon_ufs ap_ahb_ufs_clk;
	struct syscon_ufs ap_apb_ufs_en;
	struct syscon_ufs ufs_refclk_on;
	struct syscon_ufs ahb_ufs_lp;
	struct syscon_ufs ahb_ufs_force_isol;
	struct syscon_ufs ahb_ufs_cb;
	struct syscon_ufs ahb_ufs_ies_en;
	struct syscon_ufs ahb_ufs_cg_pclkreq;
	struct clk *hclk_source;
	struct clk *pclk_source;
	struct clk *hclk;
	struct clk *pclk;
	struct reset_control *ap_apb_ufs_rst;
	struct reset_control *ap_apb_ufs_glb_rst;
	void __iomem *dbg_apb_reg;

	ktime_t last_linkup_time;
};

extern int sprd_get_soc_id(sprd_soc_id_type_t soc_id_type, u32 *id, int id_len);

#define AUTO_H8_IDLE_TIME_10MS 0x1001

/*Other attributes*/
#define VS_DEBUGSAVECONFIGTIME	0xD0A0

/* UFS analog registers */
#define MPHY_2T2R_APB_REG1 0x68
#define MPHY_2T2R_APB_RESETN (0x1 << 3)

#define FIFO_ENABLE_MASK (0x1 << 15)

/* UFS mphy registers */
#define MPHY_LANE0_FIFO 0xc08c
#define MPHY_LANE1_FIFO 0xc88c
#define MPHY_TACTIVATE_TIME_LANE0 0xc088
#define MPHY_TACTIVATE_TIME_LANE1 0xc888

#define FIFO_ENABLE_MASK (0x1 << 15)
#define MPHY_TACTIVATE_TIME_200US (0x1 << 17)

/* UFS HC register */
#define HCLKDIV_REG 0xFC
#define CLKDIV 0x100

#define	MPHY_DIG_CFG7_LANE0 0xC01c
#define	MPHY_DIG_CFG7_LANE1 0xC81c
#define	MPHY_CDR_MONITOR_BYPASS_MASK GENMASK(24, 24)
#define	MPHY_CDR_MONITOR_BYPASS_ENABLE BIT(24)

#define	MPHY_DIG_CFG20_LANE0 0xC050
#define	MPHY_RXOFFSETCALDONEOVR_MASK GENMASK(5, 4)
#define	MPHY_RXOFFSETCALDONEOVR_ENABLE (BIT(5) | BIT(4))
#define	MPHY_RXOFFOVRVAL_MASK GENMASK(11, 10)
#define	MPHY_RXOFFOVRVAL_ENABLE (BIT(11) | BIT(10))

#define	MPHY_DIG_CFG49_LANE0 0xC0C4
#define	MPHY_DIG_CFG49_LANE1 0xC8C4
#define	MPHY_RXCFGG1_MASK GENMASK(23, 0)
#define	MPHY_RXCFGG1_VAL (0x0C0C0C << 0)

#define	MPHY_DIG_CFG51_LANE0 0xC0CC
#define	MPHY_DIG_CFG51_LANE1 0xC8CC
#define	MPHY_RXCFGG3_MASK GENMASK(23, 0)
#define	MPHY_RXCFGG3_VAL (0x0D0D0D << 0)

#define	MPHY_DIG_CFG72_LANE0 0xC120
#define	MPHY_DIG_CFG72_LANE1 0xC920
#define	MPHY_RXHSG3SYNCCAP_MASK GENMASK(15, 8)
#define	MPHY_RXHSG3SYNCCAP_VAL (0x4B << 8)

#define	MPHY_DIG_CFG60_LANE0 0xC0F0
#define	MPHY_DIG_CFG60_LANE1 0xC8F0
#define	MPHY_RX_STEP4_CYCLE_G3_MASK GENMASK(31, 16)
#define	MPHY_RX_STEP4_CYCLE_G3_VAL  BIT(23)

#define	MPHY_DIG_CFG14_LANE0 0xC038
#define	MPHY_APB_REFCLK_AUTOH8_EN_MASK GENMASK(24, 24)
#define	MPHY_APB_REFCLK_AUTOH8_EN_VAL (0<<24)

#define	MPHY_REG_SEL_CFG_0 0xF0
#define	MPHY_REG_SEL_CFG_0_REFCLKON_MASK GENMASK(18, 18)
#define	MPHY_REG_SEL_CFG_0_REFCLKON_VAL BIT(18)

#define	MPHY_ANR_MPHY_CTRL2 0x40
#define	MPHY_ANR_MPHY_CTRL2_REFCLKON_MASK GENMASK(8, 8)
#define	MPHY_ANR_MPHY_CTRL2_REFCLKON_VAL BIT(8)

#define	MPHY_DIG_CFG18_LANE0  (0xc048)
#define	MPHY_APB_PLLTIMER_MASK  GENMASK(23, 16)
#define	MPHY_APB_PLLTIMER_VAL (0xd8<<16)

#define	MPHY_DIG_CFG19_LANE0 (0xc04c)
#define	MPHY_APB_HSTXSCLKINV1_MASK BIT(13)
#define	MPHY_APB_HSTXSCLKINV1_VAL BIT(13)

#define	MPHY_DIG_CFG1_LANE0  0xC004
#define	MPHY_DIG_CFG17_LANE0 0xC044
#define	MPHY_DIG_CFG32_LANE0 0xC080

#define	MPHY_DIG_CFG1_LANE1  0xC804
#define	MPHY_DIG_CFG17_LANE1 0xC844
#define	MPHY_DIG_CFG32_LANE1 0xC880

#define	MPHY_APB_RX_CFGRXBIASLSENVAL_MASK BIT(5)
#define	MPHY_APB_RX_CFGRXBIASLSENOVR_MASK BIT(21)
#define	MPHY_APB_OVR_REG_LS_LDO_STABLE_MASK BIT(28)
#define	MPHY_APB_REG_LS_LDO_STABLE_MASK BIT(17)

#define MPHY_DIG_CFG62_LANE0	0xC0F8
#define MPHY_DIG_CFG66_LANE0	0xC108
#define MPHY_DIG_CFG15_LANE0	0xC03C

#define MPHY_APB_REG_DCO_CTRLBIT	GENMASK(7, 0)
#define MPHY_APB_REG_DCO_VALUE		0x2C
#define MPHY_APB_OVR_REG_DCO_CTRLBIT	GENMASK(16, 16)
#define MPHY_APB_OVR_REG_DCO_VALUE	BIT(16)

#define WAIT_1MS_TIMEOUT	1000
#define APB_DCO_CAL_RESULT_RANGE	0xA

#define AON_VER_UFS 1

/* Define debug apb base register */
#define REG_DEBUG_APB_BASE	0x7C00A000

#endif/* _UFS_SPRD_QOGIRL6_H_ */
