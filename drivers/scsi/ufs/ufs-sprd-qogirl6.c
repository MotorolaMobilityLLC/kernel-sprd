// SPDX-License-Identifier: GPL-2.0-only
/*
 * UFS Host Controller driver for Unisoc specific extensions
 *
 * Copyright (C) 2022 Unisoc, Inc.
 *
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
#include <linux/reset.h>

#include "ufshcd.h"
#include "ufs.h"
#include "ufshcd-pltfrm.h"
#include "ufshci.h"
#include "ufs-sprd.h"
#include "ufs-sprd-qogirl6.h"
#include "ufs_quirks.h"
#include "unipro.h"
#include "ufs-sprd-ioctl.h"
#ifdef CONFIG_SPRD_UFS_PROC_FS
#include "ufs-sprd-bootdevice.h"
#endif


int syscon_get_args(struct device *dev, struct ufs_sprd_host *host)
{
	u32 args[2];
	struct device_node *np = dev->of_node;
	struct platform_device *pdev = to_platform_device(dev);
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;

	priv->aon_apb_ufs_en.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "aon_apb_ufs_en", 2, args);
	if (IS_ERR(priv->aon_apb_ufs_en.regmap)) {
		pr_warn("failed to get apb ufs aon_apb_ufs_en\n");
		return PTR_ERR(priv->aon_apb_ufs_en.regmap);
	}
	priv->aon_apb_ufs_en.reg = args[0];
	priv->aon_apb_ufs_en.mask = args[1];

	pr_info("fangkuiufs priv->aon_apb_ufs_en.regmap = %p",
		priv->aon_apb_ufs_en.regmap);

	priv->ap_ahb_ufs_clk.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "ap_ahb_ufs_clk", 2, args);
	if (IS_ERR(priv->ap_ahb_ufs_clk.regmap)) {
		pr_err("failed to get apb ufs ap_ahb_ufs_clk\n");
		return PTR_ERR(priv->ap_ahb_ufs_clk.regmap);
	}
	priv->ap_ahb_ufs_clk.reg = args[0];
	priv->ap_ahb_ufs_clk.mask = args[1];

	priv->ap_apb_ufs_en.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "ap_apb_ufs_en", 2, args);
	if (IS_ERR(priv->ap_apb_ufs_en.regmap)) {
		pr_err("failed to get apb ufs ap_apb_ufs_en\n");
		return PTR_ERR(priv->ap_apb_ufs_en.regmap);
	}
	priv->ap_apb_ufs_en.reg = args[0];
	priv->ap_apb_ufs_en.mask = args[1];

	priv->ufs_refclk_on.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "ufs_refclk_on", 2, args);
	if (IS_ERR(priv->ufs_refclk_on.regmap)) {
		pr_warn("failed to get ufs_refclk_on\n");
		return PTR_ERR(priv->ufs_refclk_on.regmap);
	}
	priv->ufs_refclk_on.reg = args[0];
	priv->ufs_refclk_on.mask = args[1];

	priv->ahb_ufs_lp.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "ahb_ufs_lp", 2, args);
	if (IS_ERR(priv->ahb_ufs_lp.regmap)) {
		pr_warn("failed to get ahb_ufs_lp\n");
		return PTR_ERR(priv->ahb_ufs_lp.regmap);
	}
	priv->ahb_ufs_lp.reg = args[0];
	priv->ahb_ufs_lp.mask = args[1];

	priv->ahb_ufs_force_isol.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "ahb_ufs_force_isol", 2, args);
	if (IS_ERR(priv->ahb_ufs_force_isol.regmap)) {
		pr_err("failed to get ahb_ufs_force_isol 1\n");
		return PTR_ERR(priv->ahb_ufs_force_isol.regmap);
	}
	priv->ahb_ufs_force_isol.reg = args[0];
	priv->ahb_ufs_force_isol.mask = args[1];

	priv->ahb_ufs_cb.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "ahb_ufs_cb", 2, args);
	if (IS_ERR(priv->ahb_ufs_cb.regmap)) {
		pr_err("failed to get ahb_ufs_cb\n");
		return PTR_ERR(priv->ahb_ufs_cb.regmap);
	}
	priv->ahb_ufs_cb.reg = args[0];
	priv->ahb_ufs_cb.mask = args[1];

	priv->ahb_ufs_ies_en.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "ahb_ufs_ies_en", 2, args);
	if (IS_ERR(priv->ahb_ufs_ies_en.regmap)) {
		pr_err("failed to get ahb_ufs_ies_en\n");
		return PTR_ERR(priv->ahb_ufs_ies_en.regmap);
	}
	priv->ahb_ufs_ies_en.reg = args[0];
	priv->ahb_ufs_ies_en.mask = args[1];

	priv->ahb_ufs_cg_pclkreq.regmap =
			syscon_regmap_lookup_by_phandle_args(np, "ahb_ufs_cg_pclkreq", 2, args);
	if (IS_ERR(priv->ahb_ufs_cg_pclkreq.regmap)) {
		pr_err("failed to get ahb_ufs_cg_pclkreq\n");
		return PTR_ERR(priv->ahb_ufs_cg_pclkreq.regmap);
	}
	priv->ahb_ufs_cg_pclkreq.reg = args[0];
	priv->ahb_ufs_cg_pclkreq.mask = args[1];

	priv->pclk = devm_clk_get(&pdev->dev, "ufs_pclk");
	if (IS_ERR(priv->pclk)) {
		dev_warn(&pdev->dev,
			"can't get the clock dts config: ufs_pclk\n");
			priv->pclk = NULL;
	}

	priv->pclk_source = devm_clk_get(&pdev->dev, "ufs_pclk_source");
	if (IS_ERR(priv->pclk_source)) {
		dev_warn(&pdev->dev,
			"can't get the clock dts config: ufs_pclk_source\n");
			priv->pclk_source = NULL;
	}
	clk_set_parent(priv->pclk, priv->pclk_source);

	priv->hclk = devm_clk_get(&pdev->dev, "ufs_hclk");
	if (IS_ERR(priv->hclk)) {
		dev_warn(&pdev->dev,
			"can't get the clock dts config: ufs_hclk\n");
			priv->hclk = NULL;
	}

	priv->hclk_source = devm_clk_get(&pdev->dev, "ufs_hclk_source");
	if (IS_ERR(priv->hclk_source)) {
		dev_warn(&pdev->dev,
			"can't get the clock dts config: ufs_hclk_source\n");
			priv->hclk_source = NULL;
	}
	clk_set_parent(priv->hclk, priv->hclk_source);

	return 0;
}

static inline int ufs_sprd_mask(void __iomem *base, u32 mask, u32 reg)
{
	u32 tmp;

	tmp = readl((base) + (reg));
	if (tmp & mask)
		return 1;
	else
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
static void ufs_remap_or(struct syscon_ufs *sysconufs)
{
	unsigned int value = 0;

	regmap_read(sysconufs->regmap,
		    sysconufs->reg, &value);
	value =	value | sysconufs->mask;
	regmap_write(sysconufs->regmap,
		     sysconufs->reg, value);
}

static int ufs_sprd_priv_parse_dt(struct device *dev,
					struct ufs_hba *hba,
					struct ufs_sprd_host *host)
{
	struct resource *res;
	struct platform_device *pdev = to_platform_device(dev);
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;

	syscon_get_args(dev, host);

	priv->ap_apb_ufs_rst = devm_reset_control_get(dev, "ufs_rst");
	if (IS_ERR(priv->ap_apb_ufs_rst)) {
		dev_err(dev, "%s get ufs_rst failed, err%ld\n",
			__func__, PTR_ERR(priv->ap_apb_ufs_rst));
		priv->ap_apb_ufs_rst = NULL;
		return -ENODEV;
	}

	priv->ap_apb_ufs_glb_rst = devm_reset_control_get(dev, "ufs_glb_rst");
	if (IS_ERR(priv->ap_apb_ufs_glb_rst)) {
		dev_err(dev, "%s get ufs_glb_rst failed, err%ld\n",
			__func__, PTR_ERR(priv->ap_apb_ufs_glb_rst));
		priv->ap_apb_ufs_glb_rst = NULL;
		return -ENODEV;
	}

	res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "ufs_analog_reg");
	if (!res) {
		dev_err(dev, "Missing ufs_analog_reg register resource\n");
		return -ENODEV;
	}

	priv->ufs_analog_reg = devm_ioremap(dev, res->start,
	resource_size(res));
	if (IS_ERR(priv->ufs_analog_reg)) {
		dev_err(dev, "%s: could not map ufs_analog_reg, err %ld\n",
			__func__, PTR_ERR(priv->ufs_analog_reg));
		priv->ufs_analog_reg = NULL;
		return -ENODEV;
	}

	res = platform_get_resource_byname(pdev,
			IORESOURCE_MEM, "aon_apb_reg");
	if (!res) {
		dev_err(dev, "Missing aon_apb_reg register resource\n");
		return -ENODEV;
	}

	priv->aon_apb_reg = devm_ioremap(dev, res->start,
			resource_size(res));
	if (IS_ERR(priv->aon_apb_reg)) {
		dev_err(dev, "%s: could not map aon_apb_reg, err %ld\n",
				__func__, PTR_ERR(priv->aon_apb_reg));
		priv->aon_apb_reg = NULL;
		return -ENODEV;
	}

	priv->dbg_apb_reg = devm_ioremap(dev, REG_DEBUG_APB_BASE, 0x100);
	if (IS_ERR(priv->dbg_apb_reg)) {
		pr_err("error to ioremap ufs debug bus base.\n");
		priv->dbg_apb_reg = NULL;
	}

	return 0;
}

void ufs_sprd_reset_pre(struct ufs_sprd_host *host)
{
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;

	ufs_remap_or(&(priv->ap_ahb_ufs_clk));
	regmap_update_bits(priv->aon_apb_ufs_en.regmap,
			   priv->aon_apb_ufs_en.reg,
			   priv->aon_apb_ufs_en.mask,
			   priv->aon_apb_ufs_en.mask);
	regmap_update_bits(priv->ahb_ufs_lp.regmap,
			   priv->ahb_ufs_lp.reg,
			   priv->ahb_ufs_lp.mask,
			   priv->ahb_ufs_lp.mask);
	regmap_update_bits(priv->ahb_ufs_force_isol.regmap,
			   priv->ahb_ufs_force_isol.reg,
			   priv->ahb_ufs_force_isol.mask,
			   0);

	if (readl(priv->aon_apb_reg + REG_AON_APB_AON_VER_ID))
		regmap_update_bits(priv->ahb_ufs_ies_en.regmap,
				  priv->ahb_ufs_ies_en.reg,
				  priv->ahb_ufs_ies_en.mask,
				  priv->ahb_ufs_ies_en.mask);
}

int ufs_sprd_reset(struct ufs_sprd_host *host)
{
	int ret = 0;
	u32 aon_ver_id = 0;
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;

	sprd_get_soc_id(AON_VER_ID, &aon_ver_id, 1);

	dev_info(host->hba->dev, "ufs hardware reset!\n");
	/* TODO: HW reset will be simple in next version. */

	regmap_update_bits(priv->ap_apb_ufs_en.regmap,
			   priv->ap_apb_ufs_en.reg,
			   priv->ap_apb_ufs_en.mask,
			   0);

	/* ufs global reset */
	ret = reset_control_assert(priv->ap_apb_ufs_glb_rst);
	if (ret) {
		dev_err(host->hba->dev, "assert ufs_glb_rst failed, ret = %d!\n", ret);
		goto out;
	}
	usleep_range(10, 20);
	ret = reset_control_deassert(priv->ap_apb_ufs_glb_rst);
	if (ret) {
		dev_err(host->hba->dev, "deassert ufs_glb_rst failed, ret = %d!\n", ret);
		goto out;
	}

	/* Configs need strict squence. */
	regmap_update_bits(priv->ap_apb_ufs_en.regmap,
			   priv->ap_apb_ufs_en.reg,
			   priv->ap_apb_ufs_en.mask,
			   priv->ap_apb_ufs_en.mask);
	/* ahb enable */
	ufs_remap_or(&(priv->ap_ahb_ufs_clk));

	regmap_update_bits(priv->aon_apb_ufs_en.regmap,
			   priv->aon_apb_ufs_en.reg,
			   priv->aon_apb_ufs_en.mask,
			   priv->aon_apb_ufs_en.mask);

	/* cbline reset */
	regmap_update_bits(priv->ahb_ufs_cb.regmap,
			   priv->ahb_ufs_cb.reg,
			   priv->ahb_ufs_cb.mask,
			   priv->ahb_ufs_cb.mask);

	/* apb reset */
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_2T2R_APB_RESETN,
			0, MPHY_2T2R_APB_REG1);
	usleep_range(1000, 1100);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_2T2R_APB_RESETN,
			MPHY_2T2R_APB_RESETN, MPHY_2T2R_APB_REG1);


	/* phy config */
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_CDR_MONITOR_BYPASS_MASK,
			MPHY_CDR_MONITOR_BYPASS_ENABLE, MPHY_DIG_CFG7_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_CDR_MONITOR_BYPASS_MASK,
			MPHY_CDR_MONITOR_BYPASS_ENABLE, MPHY_DIG_CFG7_LANE1);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_RXOFFSETCALDONEOVR_MASK,
			MPHY_RXOFFSETCALDONEOVR_ENABLE, MPHY_DIG_CFG20_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_RXOFFOVRVAL_MASK,
			MPHY_RXOFFOVRVAL_ENABLE, MPHY_DIG_CFG20_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_RXCFGG1_MASK,
			MPHY_RXCFGG1_VAL, MPHY_DIG_CFG49_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_RXCFGG1_MASK,
			MPHY_RXCFGG1_VAL, MPHY_DIG_CFG49_LANE1);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_RXCFGG3_MASK,
			MPHY_RXCFGG3_VAL, MPHY_DIG_CFG51_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_RXCFGG3_MASK,
			MPHY_RXCFGG3_VAL, MPHY_DIG_CFG51_LANE1);
	ufs_sprd_rmwl(priv->ufs_analog_reg, FIFO_ENABLE_MASK,
			FIFO_ENABLE_MASK, MPHY_LANE0_FIFO);
	ufs_sprd_rmwl(priv->ufs_analog_reg, FIFO_ENABLE_MASK,
			FIFO_ENABLE_MASK, MPHY_LANE1_FIFO);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_TACTIVATE_TIME_200US,
			MPHY_TACTIVATE_TIME_200US, MPHY_TACTIVATE_TIME_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_TACTIVATE_TIME_200US,
			MPHY_TACTIVATE_TIME_200US, MPHY_TACTIVATE_TIME_LANE1);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_RXHSG3SYNCCAP_MASK,
			MPHY_RXHSG3SYNCCAP_VAL, MPHY_DIG_CFG72_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_RXHSG3SYNCCAP_MASK,
			MPHY_RXHSG3SYNCCAP_VAL, MPHY_DIG_CFG72_LANE1);

	/* add cdr count time */
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_RX_STEP4_CYCLE_G3_MASK,
			MPHY_RX_STEP4_CYCLE_G3_VAL, MPHY_DIG_CFG60_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_RX_STEP4_CYCLE_G3_MASK,
			MPHY_RX_STEP4_CYCLE_G3_VAL, MPHY_DIG_CFG60_LANE1);

	/* cbline reset */
	regmap_update_bits(priv->ahb_ufs_cb.regmap,
			  priv->ahb_ufs_cb.reg,
			  priv->ahb_ufs_cb.mask,
			  0);

	/* enable refclk */
	regmap_update_bits(priv->ufs_refclk_on.regmap,
			  priv->ufs_refclk_on.reg,
			  priv->ufs_refclk_on.mask,
			  priv->ufs_refclk_on.mask);
	regmap_update_bits(priv->ahb_ufs_lp.regmap,
			  priv->ahb_ufs_lp.reg,
			  priv->ahb_ufs_lp.mask,
			  priv->ahb_ufs_lp.mask);
	regmap_update_bits(priv->ahb_ufs_force_isol.regmap,
			  priv->ahb_ufs_force_isol.reg,
			  priv->ahb_ufs_force_isol.mask,
			  0);

	/* ufs soft reset */
	ret = reset_control_assert(priv->ap_apb_ufs_rst);
	if (ret) {
		dev_err(host->hba->dev, "assert ufs_glb_rst failed, ret = %d!\n", ret);
		goto out;
	}
	usleep_range(10, 20);
	ret = reset_control_deassert(priv->ap_apb_ufs_rst);
	if (ret) {
		dev_err(host->hba->dev, "deassert ufs_glb_rst failed, ret = %d!\n", ret);
		goto out;
	}

	regmap_update_bits(priv->ahb_ufs_ies_en.regmap,
			  priv->ahb_ufs_ies_en.reg,
			  priv->ahb_ufs_ies_en.mask,
			  priv->ahb_ufs_ies_en.mask);
	ufs_remap_or(&(priv->ahb_ufs_cg_pclkreq));

	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_ANR_MPHY_CTRL2_REFCLKON_MASK,
			MPHY_ANR_MPHY_CTRL2_REFCLKON_VAL, MPHY_ANR_MPHY_CTRL2);
	usleep_range(1, 2);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_REG_SEL_CFG_0_REFCLKON_MASK,
			MPHY_REG_SEL_CFG_0_REFCLKON_VAL, MPHY_REG_SEL_CFG_0);
	usleep_range(1, 2);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_APB_REFCLK_AUTOH8_EN_MASK,
			MPHY_APB_REFCLK_AUTOH8_EN_VAL, MPHY_DIG_CFG14_LANE0);

	usleep_range(1, 2);
	if (aon_ver_id == AON_VER_UFS) {
		ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_APB_PLLTIMER_MASK,
				MPHY_APB_PLLTIMER_VAL, MPHY_DIG_CFG18_LANE0);
		ufs_sprd_rmwl(priv->ufs_analog_reg,
				MPHY_APB_HSTXSCLKINV1_MASK,
				MPHY_APB_HSTXSCLKINV1_VAL,
				MPHY_DIG_CFG19_LANE0);
	}

out:
	return ret;
}

static int is_ufs_sprd_host_in_pwm(struct ufs_hba *hba)
{
	int ret = 0;
	u32 pwr_mode = 0;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_PWRMODE),
			     &pwr_mode);
	if (ret)
		goto out;
	if (((pwr_mode>>0)&0xf) == SLOWAUTO_MODE ||
		((pwr_mode>>0)&0xf) == SLOW_MODE     ||
		((pwr_mode>>4)&0xf) == SLOWAUTO_MODE ||
		((pwr_mode>>4)&0xf) == SLOW_MODE) {
		ret = SLOW_MODE | (SLOW_MODE << 4);
	}

out:
	return ret;
}

static int sprd_ufs_pwrchange(struct ufs_hba *hba)
{
	int ret;
	struct ufs_pa_layer_attr pwr_info;

	pwr_info.gear_rx = UFS_PWM_G1;
	pwr_info.gear_tx = UFS_PWM_G1;
	pwr_info.lane_rx = 1;
	pwr_info.lane_tx = 1;
	pwr_info.pwr_rx = SLOW_MODE;
	pwr_info.pwr_tx = SLOW_MODE;
	pwr_info.hs_rate = 0;

	ret = ufshcd_config_pwr_mode(hba, &(pwr_info));
	if (ret)
		goto out;
	if ((((hba->max_pwr_info.info.pwr_tx) << 4) |
		(hba->max_pwr_info.info.pwr_rx)) == HS_MODE_VAL)
		ret = ufshcd_config_pwr_mode(hba, &(hba->max_pwr_info.info));

out:
	return ret;

}

void read_ufs_debug_bus(struct ufs_hba *hba)
{
	u32 sigsel[] = {0x1, 0x16, 0x17, 0x1D, 0x1E, 0x1F, 0x20, 0x21};
	u32 debugbus_data;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;
	int i;

	if (!priv->dbg_apb_reg) {
		dev_warn(hba->dev, "can't get ufs debug bus base.\n");
		return;
	}

	/* read aon ufs mphy debugbus */
	dev_err(hba->dev, "No ufs mphy debugbus single.\n");

	/* read ap ufshcd debugbus */
	writel(0x0, priv->dbg_apb_reg + 0x18);
	dev_err(hba->dev, "ap ufshcd debugbus_data as follow(syssel:0x0):\n");
	for (i = 0; i < 8; i++) {
		writel(sigsel[i] << 8, priv->dbg_apb_reg + 0x1c);
		debugbus_data = readl(priv->dbg_apb_reg + 0x50);
		dev_err(hba->dev, "sig_sel: 0x%x. debugbus_data: 0x%x\n", sigsel[i], debugbus_data);
	}
	dev_err(hba->dev, "ap ufshcd debugbus_data end.\n");
}

/*
 * ufs_sprd_init - find other essential mmio bases
 * @hba: host controller instance
 * Returns 0 on success, non-zero value on failure
 */
static int ufs_sprd_init(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct ufs_sprd_host *host;
	int ret = 0;

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	host->ufs_priv_data = devm_kzalloc(dev,
			sizeof(struct ufs_sprd_ums9230_data), GFP_KERNEL);
	if (!host->ufs_priv_data)
		return -ENOMEM;

	host->hba = hba;
	ufshcd_set_variant(hba, host);

	ret = ufs_sprd_priv_parse_dt(dev, hba, host);
	if (ret < 0)
		return ret;

	hba->host->hostt->ioctl = ufshcd_sprd_ioctl;
#ifdef CONFIG_COMPAT
	hba->host->hostt->compat_ioctl = ufshcd_sprd_ioctl;
#endif

	hba->quirks |= UFSHCD_QUIRK_BROKEN_UFS_HCI_VERSION |
		       UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS;
	hba->caps |= UFSHCD_CAP_CLK_GATING | UFSHCD_CAP_CRYPTO |
		     UFSHCD_CAP_WB_EN | UFSHCD_CAP_HIBERN8_WITH_CLK_GATING;

	ufs_sprd_reset_pre(host);
	return 0;
}

/*
 * ufs_sprd_hw_init - controller enable and reset
 * @hba: host controller instance
 */
int ufs_sprd_hw_init(struct ufs_hba *hba)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	return ufs_sprd_reset(host);
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
	unsigned long flags;

	switch (status) {
	case PRE_CHANGE:
		/* Do hardware reset before host controller enable. */
		err = ufs_sprd_hw_init(hba);
		if (err) {
			dev_err(hba->dev, "%s: ufs hardware init failed!\n", __func__);
			return err;
		}

		spin_lock_irqsave(hba->host->host_lock, flags);
		ufshcd_writel(hba, 0, REG_AUTO_HIBERNATE_IDLE_TIMER);
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		hba->capabilities &= ~MASK_AUTO_HIBERN8_SUPPORT;
		hba->ahit = 0;

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
		hba->clk_gating.delay_ms = 10;
		break;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static int ufs_compare_dev_req_pwr_mode(struct ufs_hba *hba,
					struct ufs_pa_layer_attr *dev_req_params)
{
	struct ufs_pa_layer_attr  max_pwr_info_raw = {0};
	struct ufs_pa_layer_attr *max_pwr_info = &max_pwr_info_raw;
	struct ufs_pa_layer_attr *pwr_info = dev_req_params;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;


	/* Get the connected lane count */
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDRXDATALANES),
			&max_pwr_info->lane_rx);
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDTXDATALANES),
			&max_pwr_info->lane_tx);
	/*
	 *  First, get the maximum gears of HS speed.
	 *  If a zero value, it means there is no HSGEAR capability.
	 *  Then, get the maximum gears of PWM speed.
	 */
	if (pwr_info->pwr_tx == FAST_MODE)
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR),
				&max_pwr_info->gear_rx);
	else if (pwr_info->pwr_tx == SLOW_MODE)
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&max_pwr_info->gear_rx);

	if (pwr_info->pwr_rx == FAST_MODE)
		ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR),
				&max_pwr_info->gear_tx);
	else if (pwr_info->pwr_rx == SLOW_MODE)
		ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&max_pwr_info->gear_tx);

	memcpy(&(priv->dts_pwr_info), pwr_info, sizeof(struct ufs_pa_layer_attr));

	/* if already configured to the requested pwr_mode */
	if (max_pwr_info->gear_rx < pwr_info->gear_rx  ||
			max_pwr_info->gear_tx < pwr_info->gear_tx  ||
			max_pwr_info->lane_rx < pwr_info->lane_rx  ||
			max_pwr_info->lane_tx < pwr_info->lane_tx) {
		dev_err(hba->dev, "%s: the dev_req_pwr can not compare\n", __func__);
		return -EINVAL;
	}

	return 0;
}


static int ufs_compare_max_pwr_mode(struct ufs_hba *hba)
{
	struct ufs_pa_layer_attr  pwr_info_raw;
	struct ufs_pa_layer_attr *pwr_info = &pwr_info_raw;
	struct ufs_pa_layer_attr *max_pwr_info = &hba->max_pwr_info.info;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;

	if (!hba->max_pwr_info.is_valid && (max_pwr_info->pwr_tx != 1))
		return -EINVAL;

	pwr_info->pwr_tx = FAST_MODE;
	pwr_info->pwr_rx = FAST_MODE;
	pwr_info->hs_rate = PA_HS_MODE_B;

	/* Get the connected lane count */
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDRXDATALANES),
			&pwr_info->lane_rx);
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDTXDATALANES),
			&pwr_info->lane_tx);

	if (!pwr_info->lane_rx || !pwr_info->lane_tx) {
		dev_err(hba->dev, "%s: invalid connected lanes value. rx=%d, tx=%d\n",
				__func__,
				pwr_info->lane_rx,
				pwr_info->lane_tx);
		return -EINVAL;
	}

	/*
	 * First, get the maximum gears of HS speed.
	 * If a zero value, it means there is no HSGEAR capability.
	 * Then, get the maximum gears of PWM speed.
	 */
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR), &pwr_info->gear_rx);
	if (!pwr_info->gear_rx) {
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&pwr_info->gear_rx);
		if (!pwr_info->gear_rx) {
			dev_err(hba->dev, "%s: invalid max pwm rx gear read = %d\n",
					__func__, pwr_info->gear_rx);
			return -EINVAL;
		}
		pwr_info->pwr_rx = SLOW_MODE;
	}

	ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR),
			&pwr_info->gear_tx);
	if (!pwr_info->gear_tx) {
		ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&pwr_info->gear_tx);
		if (!pwr_info->gear_tx) {
			dev_err(hba->dev, "%s: invalid max pwm tx gear read = %d\n",
					__func__, pwr_info->gear_tx);
			return -EINVAL;
		}
		pwr_info->pwr_tx = SLOW_MODE;
	}
	memcpy(&(priv->dts_pwr_info), max_pwr_info, sizeof(struct ufs_pa_layer_attr));

	/* if already configured to the requested pwr_mode */
	if (pwr_info->gear_rx != max_pwr_info->gear_rx  ||
			pwr_info->gear_tx != max_pwr_info->gear_tx  ||
			pwr_info->lane_rx != max_pwr_info->lane_rx   ||
			pwr_info->lane_tx != max_pwr_info->lane_tx   ||
			pwr_info->pwr_rx != max_pwr_info->pwr_rx     ||
			pwr_info->pwr_tx != max_pwr_info->pwr_tx     ||
			pwr_info->hs_rate != max_pwr_info->hs_rate) {
		dev_err(hba->dev, "%s: the max can not compare\n", __func__);
		return -EINVAL;
	}
	return 0;
}
static int ufs_sprd_pwr_post_compare(struct ufs_hba *hba)
{
	struct ufs_pa_layer_attr  pwr_info_raw = {0};
	struct ufs_pa_layer_attr *pwr_mode = &pwr_info_raw;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;
	int ret = 0, pwr = 0;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_RXGEAR),
			&pwr_mode->gear_rx);
	if (ret)
		goto out;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_TXGEAR),
			&pwr_mode->gear_tx);
	if (ret)
		goto out;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_ACTIVERXDATALANES),
			&pwr_mode->lane_rx);
	if (ret)
		goto out;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_ACTIVETXDATALANES),
			&pwr_mode->lane_tx);
	if (ret)
		goto out;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_HSSERIES),
			&pwr_mode->hs_rate);
	if (ret)
		goto out;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_PWRMODE),
			&pwr);
	if (ret)
		goto out;
	pwr_mode->pwr_rx = (pwr >> 4) & 0xf;
	pwr_mode->pwr_tx = (pwr >> 0) & 0xf;
	if (pwr_mode->gear_rx == priv->dts_pwr_info.gear_rx &&
			pwr_mode->gear_tx == priv->dts_pwr_info.gear_tx &&
			pwr_mode->lane_rx == priv->dts_pwr_info.lane_rx &&
			pwr_mode->lane_tx == priv->dts_pwr_info.lane_tx &&
			pwr_mode->pwr_rx == priv->dts_pwr_info.pwr_rx &&
			pwr_mode->pwr_tx == priv->dts_pwr_info.pwr_tx &&
			pwr_mode->hs_rate == priv->dts_pwr_info.hs_rate){
		pr_err("%s: success.\n", __func__);
		return 0;
	}
out:
	return -1;
}

static int ufs_sprd_pwr_change_notify(struct ufs_hba *hba,
		enum ufs_notify_change_status status,
		struct ufs_pa_layer_attr *dev_max_params,
		struct ufs_pa_layer_attr *dev_req_params)
{
	int err = 0;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;

	if (!dev_req_params) {
		pr_err("%s: incoming dev_req_params is NULL\n", __func__);
		err = -EINVAL;
		goto out;
	}

	switch (status) {
	case PRE_CHANGE:
		priv->times_pre_pwr++;
		err = -EPERM;
		if (err == 0) {
			if (ufs_compare_dev_req_pwr_mode(hba, dev_req_params)) {
				priv->times_pre_compare_fail++;
				dev_err(hba->dev, "%s: err: compare_pwr\n", __func__);
#if defined(CONFIG_SPRD_DEBUG)
				panic("pre_compare_fail");
#endif
			} else
				dev_err(hba->dev, "%s: suc: compare_pwr\n", __func__);

		} else {
			if (ufs_compare_max_pwr_mode(hba)) {
				priv->times_pre_compare_fail++;
				dev_err(hba->dev, "%s: err:  compare_max_pwr\n", __func__);
#if defined(CONFIG_SPRD_DEBUG)
				panic("pre_compare_fail");
#endif
			} else
				dev_err(hba->dev, "%s: suc: compare_max_pwr\n", __func__);
		}
		break;
	case POST_CHANGE:
		priv->times_post_pwr++;
		/* if already configured to the requested pwr_mode */
		if (ufs_sprd_pwr_post_compare(hba)) {
			priv->times_post_compare_fail++;
			dev_err(hba->dev, "%s: power configured error\n", __func__);
#if defined(CONFIG_SPRD_DEBUG)
			panic("post_compare_fail");
#endif
		} else {
			dev_err(hba->dev, "%s: power already configured\n", __func__);
		}
		/* Set auto h8 ilde time to 10ms */
		//ufshcd_auto_hibern8_enable(hba);
		break;
	default:
		err = -EINVAL;
		break;
	}

out:
	return err;
}

void ufs_set_hstxsclk(struct ufs_hba *hba)
{
	int ret;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;

	ret = ufs_sprd_mask(priv->ufs_analog_reg,
			MPHY_APB_HSTXSCLKINV1_MASK,
			MPHY_DIG_CFG19_LANE0);
	if (!ret) {
		ufs_sprd_rmwl(priv->ufs_analog_reg,
				MPHY_APB_HSTXSCLKINV1_MASK,
				MPHY_APB_HSTXSCLKINV1_VAL,
				MPHY_DIG_CFG19_LANE0);
		pr_err("ufs_pwm2hs set hstxsclk\n");
	}

}
static int sprd_ufs_pwmmode_change(struct ufs_hba *hba)
{
	int ret;
	struct ufs_pa_layer_attr pwr_info;

	ret = is_ufs_sprd_host_in_pwm(hba);
	if (ret == (SLOW_MODE|(SLOW_MODE<<4)))
		return 0;

	pwr_info.gear_rx = UFS_PWM_G3;
	pwr_info.gear_tx = UFS_PWM_G3;
	pwr_info.lane_rx = 2;
	pwr_info.lane_tx = 2;
	pwr_info.pwr_rx = SLOW_MODE;
	pwr_info.pwr_tx = SLOW_MODE;
	pwr_info.hs_rate = 0;

	ret = ufshcd_config_pwr_mode(hba, &(pwr_info));

	return ret;
}

int hibern8_exit_check(struct ufs_hba *hba,
				enum uic_cmd_dme cmd,
				enum ufs_notify_change_status status)
{
	int ret;
	u32 aon_ver_id = 0;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;

	ret = is_ufs_sprd_host_in_pwm(hba);
	if (ret == (SLOW_MODE|(SLOW_MODE<<4))) {
		sprd_get_soc_id(AON_VER_ID, &aon_ver_id, 1);
		if (priv->ioctl_cmd == UFS_IOCTL_AFC_EXIT ||
				aon_ver_id == AON_VER_UFS) {
			ret = sprd_ufs_pwrchange(hba);
			if (ret) {
				pr_err("ufs_pwm2hs err");
			} else {
				ret = is_ufs_sprd_host_in_pwm(hba);
				if (ret == (SLOW_MODE|(SLOW_MODE<<4)) &&
						((((hba->max_pwr_info.info.pwr_tx) << 4) |
						  (hba->max_pwr_info.info.pwr_rx)) == HS_MODE_VAL))
					pr_err("ufs_pwm2hs fail");
				else {
					pr_err("ufs_pwm2hs succ\n");
					if (priv->ioctl_cmd ==
							UFS_IOCTL_AFC_EXIT)
						complete(&priv->hs_async_done);
				}
			}
		}
	}
	return 0;

}

static void ufs_sprd_hibern8_notify(struct ufs_hba *hba,
		enum uic_cmd_dme cmd,
		enum ufs_notify_change_status status)
{
	int ret;
	unsigned long flags;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;

	switch (status) {
	case PRE_CHANGE:
		if (cmd == UIC_CMD_DME_HIBER_ENTER) {
			spin_lock_irqsave(hba->host->host_lock, flags);
			ufshcd_writel(hba, 0, REG_AUTO_HIBERNATE_IDLE_TIMER);
			spin_unlock_irqrestore(hba->host->host_lock, flags);
		}
		break;
	case POST_CHANGE:
		if (cmd == UIC_CMD_DME_HIBER_EXIT) {
			hba->caps &= ~UFSHCD_CAP_CLK_GATING;
			if (priv->ioctl_cmd == UFS_IOCTL_ENTER_MODE) {
				ret = sprd_ufs_pwmmode_change(hba);
				if (ret)
					pr_err("change pwm mode failed!\n");
				else
					complete(&priv->pwm_async_done);
			} else {
				hibern8_exit_check(hba, cmd, status);
			}
			hba->caps |= UFSHCD_CAP_CLK_GATING;
			/* Set auto h8 ilde time to 10ms */
			//ufshcd_auto_hibern8_enable(hba);
		}
		break;
	default:
		break;
	}
}

static void ufs_sprd_fixup_dev_quirks(struct ufs_hba *hba)
{
#ifdef CONFIG_SPRD_UFS_PROC_FS
	/* vendor UFS UID info decode. */
	ufshcd_decode_ufs_uid(hba);
#endif
}

static int ufs_sprd_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op,
						enum ufs_notify_change_status status)
{
	hba->rpm_lvl = UFS_PM_LVL_1;
	hba->spm_lvl = UFS_PM_LVL_5;
	hba->uic_link_state = UIC_LINK_OFF_STATE;

	mdelay(30);
	return 0;
}

static int ufs_sprd_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	udelay(100);
	return 0;
}

static int ufs_sprd_device_reset(struct ufs_hba *hba)
{
	return 0;
}

void ufs_sprd_setup_xfer_req(struct ufs_hba *hba, int task_tag, bool scsi_cmd)
{
	struct ufshcd_lrb *lrbp;
	struct utp_transfer_req_desc *req_desc;
	u32 data_direction;
	u32 dword_0, crypto;

	lrbp = &hba->lrb[task_tag];
	req_desc = lrbp->utr_descriptor_ptr;
	dword_0 = le32_to_cpu(req_desc->header.dword_0);
	data_direction = dword_0 & (UTP_DEVICE_TO_HOST | UTP_HOST_TO_DEVICE);
	crypto = dword_0 & UTP_REQ_DESC_CRYPTO_ENABLE_CMD;
	if (!data_direction && crypto) {
		pr_err("ufs before dword_0 = %x,%x\n", dword_0, req_desc->header.dword_0);
		dword_0 &= ~(UTP_REQ_DESC_CRYPTO_ENABLE_CMD);
		req_desc->header.dword_0 = cpu_to_le32(dword_0);
		pr_err("ufs after dword_0 = %x,%x\n", dword_0, req_desc->header.dword_0);
	}
}

static void ufs_sprd_dbg_register_dump(struct ufs_hba *hba)
{
	read_ufs_debug_bus(hba);
}

/*
 * struct ufs_hba_sprd_vops - UFS sprd specific variant operations
 *
 * The variant operations configure the necessary controller and PHY
 * handshake during initialization.
 */
const struct ufs_hba_variant_ops ufs_hba_sprd_ums9230_vops = {
	.name = "sprd,ufshc-ums9230",
	.init = ufs_sprd_init,
	.exit = ufs_sprd_exit,
	.get_ufs_hci_version = ufs_sprd_get_ufs_hci_version,
	.hce_enable_notify = ufs_sprd_hce_enable_notify,
	.link_startup_notify = ufs_sprd_link_startup_notify,
	.pwr_change_notify = ufs_sprd_pwr_change_notify,
	.hibern8_notify = ufs_sprd_hibern8_notify,
	.setup_xfer_req = ufs_sprd_setup_xfer_req,
	.apply_dev_quirks = ufs_sprd_apply_dev_quirks,
	.fixup_dev_quirks = ufs_sprd_fixup_dev_quirks,
	.suspend = ufs_sprd_suspend,
	.resume = ufs_sprd_resume,
	.dbg_register_dump = ufs_sprd_dbg_register_dump,
	.device_reset = ufs_sprd_device_reset,
};
EXPORT_SYMBOL(ufs_hba_sprd_ums9230_vops);
