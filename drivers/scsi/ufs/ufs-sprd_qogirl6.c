/* SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2020 Unisoc Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <asm/unaligned.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/time.h>
#include <linux/sprd_soc_id.h>
#include <dt-bindings/soc/sprd,qogirl6-regs.h>
#include <linux/rpmb.h>

#include "ufshcd.h"
#include "ufs.h"
#include "ufshcd-pltfrm.h"
#include "ufshci.h"
#include "ufs-sprd_qogirl6.h"
#include "ufs_quirks.h"
#include "unipro.h"


int syscon_get_args(struct device *dev, struct ufs_sprd_host *host)
{
	u32 args[2];
	struct device_node *np = dev->of_node;
	struct platform_device *pdev = to_platform_device(dev);

	host->aon_apb_ufs_en.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "aon_apb_ufs_en", 2, args);
	if (IS_ERR(host->aon_apb_ufs_en.regmap)) {
		pr_warn("failed to get apb ufs aon_apb_ufs_en\n");
		return PTR_ERR(host->aon_apb_ufs_en.regmap);
	} else {
		host->aon_apb_ufs_en.reg = args[0];
		host->aon_apb_ufs_en.mask = args[1];
	}

	pr_info("fangkuiufs host->aon_apb_ufs_en.regmap = %p",
		host->aon_apb_ufs_en.regmap);

	host->ap_ahb_ufs_clk.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "ap_ahb_ufs_clk", 2, args);
	if (IS_ERR(host->ap_ahb_ufs_clk.regmap)) {
		pr_err("failed to get apb ufs ap_ahb_ufs_clk\n");
		return PTR_ERR(host->ap_ahb_ufs_clk.regmap);
	} else {
		host->ap_ahb_ufs_clk.reg = args[0];
		host->ap_ahb_ufs_clk.mask = args[1];
	}

	host->ap_apb_ufs_en.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "ap_apb_ufs_en", 2, args);
	if (IS_ERR(host->ap_apb_ufs_en.regmap)) {
		pr_err("failed to get apb ufs ap_apb_ufs_en\n");
		return PTR_ERR(host->ap_apb_ufs_en.regmap);
	} else {
		host->ap_apb_ufs_en.reg = args[0];
		host->ap_apb_ufs_en.mask = args[1];
	}

	host->ap_apb_ufs_rst.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "ap_apb_ufs_rst", 2, args);
	if (IS_ERR(host->ap_apb_ufs_rst.regmap)) {
		pr_err("failed to get ap_apb_ufs_rst\n");
		return PTR_ERR(host->ap_apb_ufs_rst.regmap);
	} else {
		host->ap_apb_ufs_rst.reg = args[0];
		host->ap_apb_ufs_rst.mask = args[1];
	}

	host->ufs_refclk_on.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "ufs_refclk_on", 2, args);
	if (IS_ERR(host->ufs_refclk_on.regmap)) {
		pr_warn("failed to get ufs_refclk_on\n");
		return PTR_ERR(host->ufs_refclk_on.regmap);
	} else {
		host->ufs_refclk_on.reg = args[0];
		host->ufs_refclk_on.mask = args[1];
	}

	host->ahb_ufs_lp.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "ahb_ufs_lp", 2, args);
	if (IS_ERR(host->ahb_ufs_lp.regmap)) {
		pr_warn("failed to get ahb_ufs_lp\n");
		return PTR_ERR(host->ahb_ufs_lp.regmap);
	} else {
		host->ahb_ufs_lp.reg = args[0];
		host->ahb_ufs_lp.mask = args[1];
	}

	host->ahb_ufs_force_isol.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "ahb_ufs_force_isol", 2, args);
	if (IS_ERR(host->ahb_ufs_force_isol.regmap)) {
		pr_err("failed to get ahb_ufs_force_isol 1\n");
		return PTR_ERR(host->ahb_ufs_force_isol.regmap);
	} else {
		host->ahb_ufs_force_isol.reg = args[0];
		host->ahb_ufs_force_isol.mask = args[1];
	}

	host->ahb_ufs_cb.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "ahb_ufs_cb", 2, args);
	if (IS_ERR(host->ahb_ufs_cb.regmap)) {
		pr_err("failed to get ahb_ufs_cb\n");
		return PTR_ERR(host->ahb_ufs_cb.regmap);
	} else {
		host->ahb_ufs_cb.reg = args[0];
		host->ahb_ufs_cb.mask = args[1];
	}
	host->ahb_ufs_ies_en.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "ahb_ufs_ies_en", 2, args);
	if (IS_ERR(host->ahb_ufs_ies_en.regmap)) {
		pr_err("failed to get ahb_ufs_ies_en\n");
		return PTR_ERR(host->ahb_ufs_ies_en.regmap);
	} else {
		host->ahb_ufs_ies_en.reg = args[0];
		host->ahb_ufs_ies_en.mask = args[1];
	}

	host->ahb_ufs_cg_pclkreq.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "ahb_ufs_cg_pclkreq", 2, args);
	if (IS_ERR(host->ahb_ufs_cg_pclkreq.regmap)) {
		pr_err("failed to get ahb_ufs_cg_pclkreq\n");
		return PTR_ERR(host->ahb_ufs_cg_pclkreq.regmap);
	} else {
		host->ahb_ufs_cg_pclkreq.reg = args[0];
		host->ahb_ufs_cg_pclkreq.mask = args[1];
	}
	host->ap_apb_ufs_glb_rst.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "ap_apb_ufs_glb_rst", 2, args);
	if (IS_ERR(host->ap_apb_ufs_glb_rst.regmap)) {
		pr_err("failed to get ap_apb_ufs_glb_rst\n");
		return PTR_ERR(host->ap_apb_ufs_glb_rst.regmap);
	} else {
		host->ap_apb_ufs_glb_rst.reg = args[0];
		host->ap_apb_ufs_glb_rst.mask = args[1];
	}


	host->pclk = devm_clk_get(&pdev->dev, "ufs_pclk");
	if (IS_ERR(host->pclk)) {
		dev_warn(&pdev->dev,
			"can't get the clock dts config: ufs_pclk\n");
			host->pclk = NULL;
	}

	host->pclk_source = devm_clk_get(&pdev->dev, "ufs_pclk_source");
	if (IS_ERR(host->pclk_source)) {
		dev_warn(&pdev->dev,
			"can't get the clock dts config: ufs_pclk_source\n");
			host->pclk_source = NULL;
	}
	clk_set_parent(host->pclk, host->pclk_source);

	host->hclk = devm_clk_get(&pdev->dev, "ufs_hclk");
	if (IS_ERR(host->hclk)) {
		dev_warn(&pdev->dev,
			"can't get the clock dts config: ufs_hclk\n");
			host->hclk = NULL;
	}

	host->hclk_source = devm_clk_get(&pdev->dev, "ufs_hclk_source");
	if (IS_ERR(host->hclk_source)) {
		dev_warn(&pdev->dev,
			"can't get the clock dts config: ufs_hclk_source\n");
			host->hclk_source = NULL;
	}
	clk_set_parent(host->hclk, host->hclk_source);

	return 0;
}

/*
 * ufs_sprd_rmwl - read modify write into a register
 * @base - base address
 * @mask - mask to apply on read value
 * @val - actual value to write
 * @reg - register address
 */
static inline void ufs_sprd_rmwl(void __iomem *base, u32 mask, u32 val, u32 reg)
{
	u32 tmp;

	tmp = readl((base) + (reg));
	tmp &= ~mask;
	tmp |= (val & mask);
	writel(tmp, (base) + (reg));
}
static void ufs_remap_and(struct syscon_ufs *sysconufs)
{
	unsigned int value = 0;

	regmap_read(sysconufs->regmap,
		    sysconufs->reg, &value);
	value =	value & (~(sysconufs->mask));
	regmap_write(sysconufs->regmap,
		     sysconufs->reg, value);
}
static void ufs_remap_or(struct syscon_ufs *sysconufs)
{
	unsigned int value = 0;

	regmap_read(sysconufs->regmap,
		    sysconufs->reg, &value);
	value =	value | sysconufs->mask;
	regmap_write(sysconufs->regmap,
		     sysconufs->reg, value);
}
void ufs_sprd_reset_pre(struct ufs_sprd_host *host)
{
	ufs_remap_or(&(host->ap_ahb_ufs_clk));
	regmap_update_bits(host->aon_apb_ufs_en.regmap,
			   host->aon_apb_ufs_en.reg,
			   host->aon_apb_ufs_en.mask,
			   host->aon_apb_ufs_en.mask);
	ufs_remap_or(&(host->ahb_ufs_lp));
	ufs_remap_and(&(host->ahb_ufs_force_isol));

	if (readl(host->aon_apb_reg + REG_AON_APB_AON_VER_ID))
		ufs_remap_or(&(host->ahb_ufs_ies_en));
}

void ufs_sprd_reset(struct ufs_sprd_host *host)
{
	dev_info(host->hba->dev, "ufs hardware reset!\n");
	/* TODO: HW reset will be simple in next version. */

	ufs_remap_and(&(host->ap_apb_ufs_en));
	ufs_remap_or(&(host->ap_apb_ufs_glb_rst));
	udelay(10);
	ufs_remap_and(&(host->ap_apb_ufs_glb_rst));

	/* Configs need strict squence. */
	ufs_remap_or(&(host->ap_apb_ufs_en));
	/* ahb enable */
	ufs_remap_or(&(host->ap_ahb_ufs_clk));

	regmap_update_bits(host->aon_apb_ufs_en.regmap,
			   host->aon_apb_ufs_en.reg,
			   host->aon_apb_ufs_en.mask,
			   host->aon_apb_ufs_en.mask);

	/* cbline reset */
	ufs_remap_or(&(host->ahb_ufs_cb));

	/* apb reset */
	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_2T2R_APB_RESETN,
			0, MPHY_2T2R_APB_REG1);
	mdelay(1);
	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_2T2R_APB_RESETN,
			MPHY_2T2R_APB_RESETN, MPHY_2T2R_APB_REG1);


	/* phy config */
	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_CDR_MONITOR_BYPASS_MASK,
			MPHY_CDR_MONITOR_BYPASS_ENABLE, MPHY_DIG_CFG7_LANE0);
	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_CDR_MONITOR_BYPASS_MASK,
			MPHY_CDR_MONITOR_BYPASS_ENABLE, MPHY_DIG_CFG7_LANE1);
	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_RXOFFSETCALDONEOVR_MASK,
			MPHY_RXOFFSETCALDONEOVR_ENABLE, MPHY_DIG_CFG20_LANE0);
	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_RXOFFOVRVAL_MASK,
			MPHY_RXOFFOVRVAL_ENABLE, MPHY_DIG_CFG20_LANE0);
	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_RXCFGG1_MASK,
			MPHY_RXCFGG1_VAL, MPHY_DIG_CFG49_LANE0);
	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_RXCFGG1_MASK,
			MPHY_RXCFGG1_VAL, MPHY_DIG_CFG49_LANE1);
	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_RXCFGG3_MASK,
			MPHY_RXCFGG3_VAL, MPHY_DIG_CFG51_LANE0);
	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_RXCFGG3_MASK,
			MPHY_RXCFGG3_VAL, MPHY_DIG_CFG51_LANE1);
	ufs_sprd_rmwl(host->ufs_analog_reg, FIFO_ENABLE_MASK,
			FIFO_ENABLE_MASK, MPHY_LANE0_FIFO);
	ufs_sprd_rmwl(host->ufs_analog_reg, FIFO_ENABLE_MASK,
			FIFO_ENABLE_MASK, MPHY_LANE1_FIFO);
	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_TACTIVATE_TIME_200US,
			MPHY_TACTIVATE_TIME_200US, MPHY_TACTIVATE_TIME_LANE0);
	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_TACTIVATE_TIME_200US,
			MPHY_TACTIVATE_TIME_200US, MPHY_TACTIVATE_TIME_LANE1);
	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_RXHSG3SYNCCAP_MASK,
			MPHY_RXHSG3SYNCCAP_VAL, MPHY_DIG_CFG72_LANE0);
	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_RXHSG3SYNCCAP_MASK,
			MPHY_RXHSG3SYNCCAP_VAL, MPHY_DIG_CFG72_LANE1);

	/* add cdr count time */
	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_RX_STEP4_CYCLE_G3_MASK,
			MPHY_RX_STEP4_CYCLE_G3_VAL, MPHY_DIG_CFG60_LANE0);
	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_RX_STEP4_CYCLE_G3_MASK,
			MPHY_RX_STEP4_CYCLE_G3_VAL, MPHY_DIG_CFG60_LANE1);

	/* cbline reset */
	ufs_remap_and(&(host->ahb_ufs_cb));

	/* enable refclk */
	ufs_remap_or(&(host->ufs_refclk_on));
	ufs_remap_or(&(host->ahb_ufs_lp));
	ufs_remap_and(&(host->ahb_ufs_force_isol));
	ufs_remap_or(&(host->ap_apb_ufs_rst));
	ufs_remap_and(&(host->ap_apb_ufs_rst));

	ufs_remap_or(&(host->ahb_ufs_ies_en));
	ufs_remap_or(&(host->ahb_ufs_cg_pclkreq));

	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_ANR_MPHY_CTRL2_REFCLKON_MASK,
			MPHY_ANR_MPHY_CTRL2_REFCLKON_VAL, MPHY_ANR_MPHY_CTRL2);
	udelay(1);
	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_REG_SEL_CFG_0_REFCLKON_MASK,
			MPHY_REG_SEL_CFG_0_REFCLKON_VAL, MPHY_REG_SEL_CFG_0);
	udelay(1);
	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_APB_REFCLK_AUTOH8_EN_MASK,
			MPHY_APB_REFCLK_AUTOH8_EN_VAL, MPHY_DIG_CFG14_LANE0);
}

/*
 * ufs_sprd_init - find other essential mmio bases
 * @hba: host controller instance
 * Returns 0 on success, non-zero value on failure
 */
static int ufs_sprd_init(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct ufs_sprd_host *host;
	struct resource *res;

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	host->hba = hba;
	ufshcd_set_variant(hba, host);

	syscon_get_args(dev, host);

	hba->quirks |= UFSHCD_QUIRK_BROKEN_UFS_HCI_VERSION |
		       UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS;
	hba->caps |= UFSHCD_CAP_CLK_GATING | UFSHCD_CAP_CRYPTO |
		     UFSHCD_CAP_WB_EN;

	res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "ufs_analog_reg");
	if (!res) {
		dev_err(dev, "Missing ufs_analog_reg register resource\n");
		return -ENODEV;
	}
	host->ufs_analog_reg = devm_ioremap_nocache(dev, res->start,
	resource_size(res));
	if (IS_ERR(host->ufs_analog_reg)) {
		dev_err(dev, "%s: could not map ufs_analog_reg, err %ld\n",
			__func__, PTR_ERR(host->ufs_analog_reg));
		host->ufs_analog_reg = NULL;
		return -ENODEV;
	}

	res = platform_get_resource_byname(pdev,
			IORESOURCE_MEM, "aon_apb_reg");
	if (!res) {
		dev_err(dev, "Missing aon_apb_reg register resource\n");
		return -ENODEV;
	}
	host->aon_apb_reg = devm_ioremap_nocache(dev, res->start,
			resource_size(res));
	if (IS_ERR(host->aon_apb_reg)) {
		dev_err(dev, "%s: could not map aon_apb_reg, err %ld\n",
				__func__, PTR_ERR(host->aon_apb_reg));
		host->aon_apb_reg = NULL;
		return -ENODEV;
	}

	ufs_sprd_reset_pre(host);
	return 0;
}

/*
 * ufs_sprd_hw_init - controller enable and reset
 * @hba: host controller instance
 */
void ufs_sprd_hw_init(struct ufs_hba *hba)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	ufs_sprd_reset(host);
}

static void ufs_sprd_exit(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	devm_kfree(dev, host);
	hba->priv = NULL;
}

static u32 ufs_sprd_get_ufs_hci_version(struct ufs_hba *hba)
{
	return UFSHCI_VERSION_21;
}

static int ufs_sprd_hce_enable_notify(struct ufs_hba *hba,
				      enum ufs_notify_change_status status)
{
	int err = 0;

	switch (status) {
	case PRE_CHANGE:
		/* Do hardware reset before host controller enable. */
		ufs_sprd_hw_init(hba);
		ufshcd_writel(hba, CONTROLLER_ENABLE, REG_CONTROLLER_ENABLE);
		break;
	case POST_CHANGE:
		ufshcd_writel(hba, CLKDIV, HCLKDIV_REG);
		break;
	default:
		dev_err(hba->dev, "%s: invalid status %d\n", __func__, status);
		err = -EINVAL;
		break;
	}

	return err;
}

static int ufs_sprd_apply_dev_quirks(struct ufs_hba *hba)
{
	int ret = 0;
	u32 granularity, peer_granularity;
	u32 pa_tactivate, peer_pa_tactivate;
	u32 pa_tactivate_us, peer_pa_tactivate_us, max_pa_tactivate_us;
	u8 gran_to_us_table[] = {1, 4, 8, 16, 32, 100};
	u32 new_pa_tactivate, new_peer_pa_tactivate;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_GRANULARITY),
				  &granularity);
	if (ret)
		goto out;

	ret = ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_GRANULARITY),
				  &peer_granularity);
	if (ret)
		goto out;

	if ((granularity < PA_GRANULARITY_MIN_VAL) ||
	    (granularity > PA_GRANULARITY_MAX_VAL)) {
		dev_err(hba->dev, "%s: invalid host PA_GRANULARITY %d",
			__func__, granularity);
		return -EINVAL;
	}

	if ((peer_granularity < PA_GRANULARITY_MIN_VAL) ||
	    (peer_granularity > PA_GRANULARITY_MAX_VAL)) {
		dev_err(hba->dev, "%s: invalid device PA_GRANULARITY %d",
			__func__, peer_granularity);
		return -EINVAL;
	}

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_TACTIVATE), &pa_tactivate);
	if (ret)
		goto out;

	ret = ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_TACTIVATE),
				  &peer_pa_tactivate);
	if (ret)
		goto out;

	pa_tactivate_us = pa_tactivate * gran_to_us_table[granularity - 1];
	peer_pa_tactivate_us = peer_pa_tactivate *
			gran_to_us_table[peer_granularity - 1];
	max_pa_tactivate_us = (pa_tactivate_us > peer_pa_tactivate_us) ?
			pa_tactivate_us : peer_pa_tactivate_us;

	new_peer_pa_tactivate = (max_pa_tactivate_us + 400) /
			gran_to_us_table[peer_granularity - 1];

	ret = ufshcd_dme_peer_set(hba, UIC_ARG_MIB(PA_TACTIVATE),
				  new_peer_pa_tactivate);
	if (ret) {
		dev_err(hba->dev, "%s: peer_pa_tactivate set err ", __func__);
		goto out;
	}

	new_pa_tactivate = (max_pa_tactivate_us + 300) /
			gran_to_us_table[granularity - 1];
	ret = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TACTIVATE),
			     new_pa_tactivate);
	if (ret) {
		dev_err(hba->dev, "%s: pa_tactivate set err ", __func__);
		goto out;
	}

	dev_warn(hba->dev, "%s: %d,%d,%d,%d",
		 __func__, new_peer_pa_tactivate,
		 peer_granularity, new_pa_tactivate, granularity);

out:
	return ret;
}

static int ufs_sprd_link_startup_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status status)
{
	int err = 0;

	switch (status) {
	case PRE_CHANGE:
		/* UFS device needs 32us PA_Saveconfig Time */
		ufshcd_dme_set(hba, UIC_ARG_MIB(VS_DEBUGSAVECONFIGTIME), 0x13);

		/*
		 * Some UFS devices (and may be host) have issues if LCC is
		 * enabled. So we are setting PA_Local_TX_LCC_Enable to 0
		 * before link startup which will make sure that both host
		 * and device TX LCC are disabled once link startup is
		 * completed.
		 */
		if (ufshcd_get_local_unipro_ver(hba) != UFS_UNIPRO_VER_1_41)
			err = ufshcd_dme_set(hba,
					UIC_ARG_MIB(PA_LOCAL_TX_LCC_ENABLE),
					0);

		break;
	case POST_CHANGE:
		break;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static int ufs_sprd_pwr_change_notify(struct ufs_hba *hba,
				enum ufs_notify_change_status status,
				struct ufs_pa_layer_attr *dev_max_params,
				struct ufs_pa_layer_attr *dev_req_params)
{
	int err = 0;

	if (!dev_req_params) {
		pr_err("%s: incoming dev_req_params is NULL\n", __func__);
		err = -EINVAL;
		goto out;
	}

	switch (status) {
	case PRE_CHANGE:
		dev_req_params->gear_rx = UFS_HS_G3;
		dev_req_params->gear_tx = UFS_HS_G3;
		dev_req_params->lane_rx = 2;
		dev_req_params->lane_tx = 2;
		dev_req_params->pwr_rx = FAST_MODE;
		dev_req_params->pwr_tx = FAST_MODE;
		dev_req_params->hs_rate = PA_HS_MODE_B;
		break;
	case POST_CHANGE:
		/* Set auto h8 ilde time to 10ms */
		ufshcd_writel(hba,
			AUTO_H8_IDLE_TIME_10MS, REG_AUTO_HIBERNATE_IDLE_TIMER);
		break;
	default:
		err = -EINVAL;
		break;
	}

out:
	return err;
}

static void ufs_sprd_hibern8_notify(struct ufs_hba *hba,
				enum uic_cmd_dme cmd,
				enum ufs_notify_change_status status)
{
	switch (status) {
	case PRE_CHANGE:
		if (cmd == UIC_CMD_DME_HIBER_ENTER) {
			ufshcd_writel(hba,
				0x0, REG_AUTO_HIBERNATE_IDLE_TIMER);
		}
		break;
	case POST_CHANGE:
		if (cmd == UIC_CMD_DME_HIBER_EXIT) {
			ufshcd_writel(hba,
				AUTO_H8_IDLE_TIME_10MS,
				REG_AUTO_HIBERNATE_IDLE_TIMER);
		}
		break;
	default:
		break;
	}
}

static int ufs_sprd_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	hba->rpm_lvl = UFS_PM_LVL_1;
	hba->spm_lvl = UFS_PM_LVL_5;
	hba->uic_link_state = UIC_LINK_OFF_STATE;
	return 0;
}

static int ufs_sprd_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	return 0;
}

static inline u16 ufs_sprd_wlun_to_scsi_lun(u8 upiu_wlun_id)
{
	return (upiu_wlun_id & ~UFS_UPIU_WLUN_ID) | SCSI_W_LUN_BASE;
}

static int ufs_sprd_get_sdev(struct ufs_hba *hba, uint channel,
			     uint id, u64 lun)
{
	struct scsi_device *sdev_rpmb;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	int ret = 0;

	sdev_rpmb = __scsi_add_device(hba->host, channel, id, lun, NULL);
	if (IS_ERR(sdev_rpmb)) {
		ret = PTR_ERR(sdev_rpmb);
		return ret;
	}
	host->sdev_ufs_rpmb = sdev_rpmb;

	return ret;
}

static inline int ufs_sprd_read_geometry_desc_param(struct ufs_hba *hba,
			enum geometry_desc_param param_offset,
			u8 *param_read_buf, u32 param_size)
{
	return ufshcd_read_desc_param(hba, QUERY_DESC_IDN_GEOMETRY, 0,
				      param_offset, param_read_buf, param_size);
}

#define SEC_PROTOCOL_UFS  0xEC
#define SEC_SPECIFIC_UFS_RPMB 0x0001
#define SEC_PROTOCOL_CMD_SIZE 12
#define SEC_PROTOCOL_RETRIES 3
#define SEC_PROTOCOL_RETRIES_ON_RESET 10
#define SEC_PROTOCOL_TIMEOUT msecs_to_jiffies(1000)

static int ufs_sprd_rpmb_security_out(struct scsi_device *sdev,
				      struct rpmb_frame *frames, u32 cnt)
{
	struct scsi_sense_hdr sshdr;
	u32 trans_len = cnt * sizeof(struct rpmb_frame);
	int reset_retries = SEC_PROTOCOL_RETRIES_ON_RESET;
	int ret;
	u8 cmd[SEC_PROTOCOL_CMD_SIZE];
	char *sense = NULL;

	sense = kzalloc(SCSI_SENSE_BUFFERSIZE, GFP_NOIO);
	if (!sense) {
		pr_err("%s sense alloc failed\n", __func__);
		return -1;
	}

	memset(cmd, 0, SEC_PROTOCOL_CMD_SIZE);
	cmd[0] = SECURITY_PROTOCOL_OUT;
	cmd[1] = SEC_PROTOCOL_UFS;
	put_unaligned_be16(SEC_SPECIFIC_UFS_RPMB, cmd + 2);
	cmd[4] = 0;
	put_unaligned_be32(trans_len, cmd + 6);

	ret = scsi_test_unit_ready(sdev, SEC_PROTOCOL_TIMEOUT,
				   SEC_PROTOCOL_RETRIES, &sshdr);
	if (ret)
		dev_err(&sdev->sdev_gendev,
			"%s: rpmb scsi_test_unit_ready, ret=%d\n",
			__func__, ret);

retry:
	ret = __scsi_execute(sdev, cmd, DMA_TO_DEVICE, frames, trans_len,
			     sense, &sshdr, SEC_PROTOCOL_TIMEOUT,
			     SEC_PROTOCOL_RETRIES, 0, 0, NULL);

	if (ret && scsi_sense_valid(&sshdr) &&
	    sshdr.sense_key == UNIT_ATTENTION &&
	    sshdr.asc == 0x29 && sshdr.ascq == 0x00)
		/*
		 * Device reset might occur several times,
		 * give it one more chance
		 */
		if (--reset_retries > 0)
			goto retry;

	if (ret)
		dev_err(&sdev->sdev_gendev, "%s: failed with err %0x\n",
			__func__, ret);

	if (driver_byte(ret) & DRIVER_SENSE)
		scsi_print_sense_hdr(sdev, "rpmb: security out", &sshdr);

	kfree(sense);
	return ret;
}

static int ufs_sprd_rpmb_security_in(struct scsi_device *sdev,
				      struct rpmb_frame *frames, u32 cnt)
{
	struct scsi_sense_hdr sshdr;
	u32 alloc_len = cnt * sizeof(struct rpmb_frame);
	int reset_retries = SEC_PROTOCOL_RETRIES_ON_RESET;
	int ret;
	u8 cmd[SEC_PROTOCOL_CMD_SIZE];
	char *sense = NULL;

	sense = kzalloc(SCSI_SENSE_BUFFERSIZE, GFP_NOIO);
	if (!sense) {
		pr_err("%s sense alloc failed\n", __func__);
		return -1;
	}
	memset(cmd, 0, SEC_PROTOCOL_CMD_SIZE);
	cmd[0] = SECURITY_PROTOCOL_IN;
	cmd[1] = SEC_PROTOCOL_UFS;
	put_unaligned_be16(SEC_SPECIFIC_UFS_RPMB, cmd + 2);
	cmd[4] = 0;
	put_unaligned_be32(alloc_len, cmd + 6);

	ret = scsi_test_unit_ready(sdev, SEC_PROTOCOL_TIMEOUT,
				   SEC_PROTOCOL_RETRIES, &sshdr);
	if (ret)
		dev_err(&sdev->sdev_gendev,
			"%s: rpmb scsi_test_unit_ready, ret=%d\n",
			__func__, ret);

retry:
	ret = __scsi_execute(sdev, cmd, DMA_FROM_DEVICE, frames, alloc_len,
			     sense, &sshdr, SEC_PROTOCOL_TIMEOUT,
			     SEC_PROTOCOL_RETRIES, 0, 0, NULL);

	if (ret && scsi_sense_valid(&sshdr) &&
	    sshdr.sense_key == UNIT_ATTENTION &&
	    sshdr.asc == 0x29 && sshdr.ascq == 0x00)
		/*
		 * Device reset might occur several times,
		 * give it one more chance
		 */
		if (--reset_retries > 0)
			goto retry;

	if (ret)
		dev_err(&sdev->sdev_gendev, "%s: failed with err %0x\n",
			__func__, ret);

	if (driver_byte(ret) & DRIVER_SENSE)
		scsi_print_sense_hdr(sdev, "rpmb: security in", &sshdr);

	kfree(sense);
	return ret;
}

static int ufs_rpmb_cmd_seq(struct device *dev,
			    struct rpmb_cmd *cmds, u32 ncmds)
{
	unsigned long flags;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct scsi_device *sdev;
	struct rpmb_cmd *cmd;
	int i;
	int ret;

	spin_lock_irqsave(hba->host->host_lock, flags);
	sdev = host->sdev_ufs_rpmb;
	if (sdev) {
		ret = scsi_device_get(sdev);
		if (!ret && !scsi_device_online(sdev)) {
			ret = -ENODEV;
			scsi_device_put(sdev);
		}
	} else {
		ret = -ENODEV;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	if (ret)
		return ret;

	for (ret = 0, i = 0; i < ncmds && !ret; i++) {
		cmd = &cmds[i];
		if (cmd->flags & RPMB_F_WRITE)
			ret = ufs_sprd_rpmb_security_out(sdev, cmd->frames,
							 cmd->nframes);
		else
			ret = ufs_sprd_rpmb_security_in(sdev, cmd->frames,
							cmd->nframes);
	}
	scsi_device_put(sdev);

	return ret;
}

static struct rpmb_ops ufshcd_rpmb_dev_ops = {
	.cmd_seq = ufs_rpmb_cmd_seq,
	.type = RPMB_TYPE_UFS,
};

static inline void ufs_sprd_rpmb_add(struct ufs_hba *hba)
{
	struct rpmb_dev *rdev;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	u8 rw_size;
	int ret;

	ret = ufs_sprd_get_sdev(hba, 0, 0,
			ufs_sprd_wlun_to_scsi_lun(UFS_UPIU_RPMB_WLUN));
	if (ret) {
		dev_warn(hba->dev, "Cannot get rpmb dev!\n");
		return;
	}

	ret = ufs_sprd_read_geometry_desc_param(hba, GEOMETRY_DESC_PARAM_RPMB_RW_SIZE,
					&rw_size, sizeof(rw_size));
	if (ret) {
		dev_warn(hba->dev, "%s: cannot get rpmb rw limit %d\n",
			dev_name(hba->dev), ret);
		rw_size = 1;
	}

	ufshcd_rpmb_dev_ops.reliable_wr_cnt = rw_size;

	ret = scsi_device_get(host->sdev_ufs_rpmb);
	rdev = rpmb_dev_register(hba->dev, &ufshcd_rpmb_dev_ops);
	if (IS_ERR(rdev)) {
		dev_warn(hba->dev, "%s: cannot register to rpmb %ld\n",
			 dev_name(hba->dev), PTR_ERR(rdev));
		goto out_put_dev;
	}

	scsi_device_put(host->sdev_ufs_rpmb);
	return;

out_put_dev:
	scsi_device_put(host->sdev_ufs_rpmb);
	host->sdev_ufs_rpmb = NULL;
}

static inline void ufs_sprd_rpmb_remove(struct ufs_hba *hba)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	if (!host || !host->sdev_ufs_rpmb)
		return;

	rpmb_dev_unregister(hba->dev);
	scsi_device_put(host->sdev_ufs_rpmb);
	host->sdev_ufs_rpmb = NULL;
}

/*
 * struct ufs_hba_sprd_vops - UFS sprd specific variant operations
 *
 * The variant operations configure the necessary controller and PHY
 * handshake during initialization.
 */
static struct ufs_hba_variant_ops ufs_hba_sprd_vops = {
	.name = "sprd",
	.init = ufs_sprd_init,
	.exit = ufs_sprd_exit,
	.get_ufs_hci_version = ufs_sprd_get_ufs_hci_version,
	.hce_enable_notify = ufs_sprd_hce_enable_notify,
	.link_startup_notify = ufs_sprd_link_startup_notify,
	.pwr_change_notify = ufs_sprd_pwr_change_notify,
	.hibern8_notify = ufs_sprd_hibern8_notify,
	.apply_dev_quirks = ufs_sprd_apply_dev_quirks,
	.suspend = ufs_sprd_suspend,
	.resume = ufs_sprd_resume,
};

/*
 * ufs_sprd_probe - probe routine of the driver
 * @pdev: pointer to Platform device handle
 *
 * Return zero for success and non-zero for failure
 */
static int ufs_sprd_probe(struct platform_device *pdev)
{
	int err;
	struct device *dev = &pdev->dev;
	struct ufs_hba *hba;

	/* Perform generic probe */
	err = ufshcd_pltfrm_init(pdev, &ufs_hba_sprd_vops);
	if (err) {
		dev_err(dev, "ufshcd_pltfrm_init() failed %d\n", err);
		goto out;
	}

	hba = platform_get_drvdata(pdev);
	ufs_sprd_rpmb_add(hba);
out:
	return err;
}
/*
 * ufs_sprd_remove - set driver_data of the device to NULL
 * @pdev: pointer to platform device handle
 *
 * Always returns 0
 */
static int ufs_sprd_remove(struct platform_device *pdev)
{
	struct ufs_hba *hba =  platform_get_drvdata(pdev);

	pm_runtime_get_sync(&(pdev)->dev);
	ufs_sprd_rpmb_remove(hba);
	ufshcd_remove(hba);
	return 0;
}
/*
 * ufs_sprd_shutdown - set driver_data of the device to NULL
 * @pdev: pointer to platform device handle
 *
 * Always returns 0
 */
static void ufs_sprd_shutdown(struct platform_device *pdev)
{
	struct ufs_hba *hba =  platform_get_drvdata(pdev);

	ufs_sprd_rpmb_remove(hba);
	ufshcd_pltfrm_shutdown(pdev);
}
static const struct of_device_id ufs_sprd_of_match[] = {
	{.compatible = "sprd,ufshc"},
	{},
};

static const struct dev_pm_ops ufs_sprd_pm_ops = {
	.suspend = ufshcd_pltfrm_suspend,
	.resume = ufshcd_pltfrm_resume,
	.runtime_suspend = ufshcd_pltfrm_runtime_suspend,
	.runtime_resume = ufshcd_pltfrm_runtime_resume,
	.runtime_idle = ufshcd_pltfrm_runtime_idle,
};

static struct platform_driver ufs_sprd_pltform = {
	.probe = ufs_sprd_probe,
	.remove = ufs_sprd_remove,
	.shutdown = ufs_sprd_shutdown,
	.driver = {
		.name = "ufshcd-sprd",
		.pm = &ufs_sprd_pm_ops,
		.of_match_table = of_match_ptr(ufs_sprd_of_match),
	},
};
module_platform_driver(ufs_sprd_pltform);

MODULE_DESCRIPTION("Unisoc Specific UFSHCI driver");
MODULE_LICENSE("GPL v2");
