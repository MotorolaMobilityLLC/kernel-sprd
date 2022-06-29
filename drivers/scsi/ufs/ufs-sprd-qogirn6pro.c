/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
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
#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
#include <linux/sprd_sip_svc.h>
#endif
#include <linux/nvmem-consumer.h>
#include <linux/rpmb.h>
#include "ufshcd.h"
#include "ufs.h"
#include "ufshcd-pltfrm.h"
#include "ufshci.h"
#include "ufs-sprd-qogirn6pro.h"
#include "ufs_quirks.h"
#include "unipro.h"

/**
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

static int ufs_efuse_calib_data(struct platform_device *pdev,
				const char *cell_name)
{
	struct nvmem_cell *cell;
	void *buf;
	u32 calib_data;
	size_t len;

	if (!pdev)
		return -EINVAL;

	cell = nvmem_cell_get(&pdev->dev, cell_name);
	if (IS_ERR_OR_NULL(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	if (IS_ERR_OR_NULL(buf)) {
		nvmem_cell_put(cell);
		return PTR_ERR(buf);
	}

	memcpy(&calib_data, buf, min(len, sizeof(u32)));

	kfree(buf);
	nvmem_cell_put(cell);
	return calib_data;
}

void ufs_sprd_reset(struct ufs_sprd_host *host)
{
	dev_info(host->hba->dev, "ufs hardware reset!\n");

	regmap_update_bits(host->phy_sram_ext_ld_done.regmap,
			   host->phy_sram_ext_ld_done.reg,
			   host->phy_sram_ext_ld_done.mask,
			   host->phy_sram_ext_ld_done.mask);

	regmap_update_bits(host->phy_sram_bypass.regmap,
			   host->phy_sram_bypass.reg,
			   host->phy_sram_bypass.mask,
			   host->phy_sram_bypass.mask);

	regmap_update_bits(host->aon_apb_ufs_rst.regmap,
			   host->aon_apb_ufs_rst.reg,
			   host->aon_apb_ufs_rst.mask,
			   host->aon_apb_ufs_rst.mask);

	regmap_update_bits(host->ap_ahb_ufs_rst.regmap,
			   host->ap_ahb_ufs_rst.reg,
			   host->ap_ahb_ufs_rst.mask,
			   host->ap_ahb_ufs_rst.mask);

	mdelay(1);

	regmap_update_bits(host->aon_apb_ufs_rst.regmap,
			   host->aon_apb_ufs_rst.reg,
			   host->aon_apb_ufs_rst.mask,
			   0);

	regmap_update_bits(host->ap_ahb_ufs_rst.regmap,
			   host->ap_ahb_ufs_rst.reg,
			   host->ap_ahb_ufs_rst.mask,
			   0);
}

static int ufs_sprd_get_syscon_reg(struct device_node *np,
				   struct syscon_ufs *reg, const char *name)
{
	struct regmap *regmap;
	u32 syscon_args[2];

	regmap = syscon_regmap_lookup_by_phandle_args(np, name, 2, syscon_args);
	if (IS_ERR(regmap)) {
		pr_err("read ufs syscon %s regmap fail\n", name);
		reg->regmap = NULL;
		reg->reg = 0x0;
		reg->mask = 0x0;
		return -EINVAL;
	}
	reg->regmap = regmap;
	reg->reg = syscon_args[0];
	reg->mask = syscon_args[1];

	return 0;
}

/**
 * ufs_sprd_init - find other essential mmio bases
 * @hba: host controller instance
 * Returns 0 on success, non-zero value on failure
 */
static int ufs_sprd_init(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct ufs_sprd_host *host;
	int ret = 0;

#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
	struct sprd_sip_svc_handle *svc_handle;
#endif

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	host->hba = hba;
	ufshcd_set_variant(hba, host);

	host->ufs_lane_calib_data1 = ufs_efuse_calib_data(pdev,
							  "ufs_cali_lane1");
	if (host->ufs_lane_calib_data1 == -EPROBE_DEFER) {
		dev_err(&pdev->dev,
			"%s:get ufs_lane_calib_data1 failed!\n", __func__);
		ret =  -EPROBE_DEFER;
		goto out_variant_clear;
	}

	dev_err(&pdev->dev, "%s: ufs_lane_calib_data1: %x\n",
		__func__, host->ufs_lane_calib_data1);

	host->ufs_lane_calib_data0 = ufs_efuse_calib_data(pdev,
							  "ufs_cali_lane0");
	if (host->ufs_lane_calib_data0 == -EPROBE_DEFER) {
		dev_err(&pdev->dev,
			"%s:get ufs_lane_calib_data1 failed!\n", __func__);
		ret =  -EPROBE_DEFER;
		goto out_variant_clear;
	}

	dev_err(&pdev->dev, "%s: ufs_lane_calib_data0: %x\n",
		__func__, host->ufs_lane_calib_data0);

	host->vdd_mphy = devm_regulator_get(dev, "vdd-mphy");
	ret = regulator_enable(host->vdd_mphy);
	if (ret)
		return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->ap_ahb_ufs_rst,
				      "ap_ahb_ufs_rst");
	if (ret < 0)
		return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->aon_apb_ufs_rst,
				      "aon_apb_ufs_rst");
	if (ret < 0)
		return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->phy_sram_ext_ld_done,
				      "phy_sram_ext_ld_done");
	if (ret < 0)
		return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->phy_sram_bypass,
				      "phy_sram_bypass");
	if (ret < 0)
		return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->phy_sram_init_done,
				      "phy_sram_init_done");
	if (ret < 0)
		return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->aon_apb_ufs_clk_en,
				      "aon_apb_ufs_clk_en");
	if (ret < 0)
		 return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->ufsdev_refclk_en,
				      "ufsdev_refclk_en");
	if (ret < 0)
		return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node,
					&host->usb31pllv_ref2mphy_en,
				      "usb31pllv_ref2mphy_en");
	if (ret < 0)
		return -ENODEV;

	host->hclk = devm_clk_get(&pdev->dev, "ufs_hclk");
	if (IS_ERR(host->hclk)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: ufs_pclk\n");
			 host->hclk = NULL;
	}

	host->hclk_source = devm_clk_get(&pdev->dev, "ufs_hclk_source");
	if (IS_ERR(host->hclk_source)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: ufs_hclk_source\n");
			 host->hclk_source = NULL;
	}

	clk_set_parent(host->hclk, host->hclk_source);

#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
	regmap_update_bits(host->ap_ahb_ufs_rst.regmap,
					  host->ap_ahb_ufs_rst.reg,
					  host->ap_ahb_ufs_rst.mask,
					  host->ap_ahb_ufs_rst.mask);

	mdelay(1);

	regmap_update_bits(host->ap_ahb_ufs_rst.regmap,
					  host->ap_ahb_ufs_rst.reg,
					  host->ap_ahb_ufs_rst.mask,
					  0);

	ufshcd_writel(hba, CONTROLLER_ENABLE, REG_CONTROLLER_ENABLE);
	if ((ufshcd_readl(hba, REG_UFS_CCAP) & (1 << 27)))
		ufshcd_writel(hba, (CRYPTO_GENERAL_ENABLE | CONTROLLER_ENABLE),
			      REG_CONTROLLER_ENABLE);
	svc_handle = sprd_sip_svc_get_handle();
	if (!svc_handle) {
		pr_err("%s: failed to get svc handle\n", __func__);
		return -ENODEV;
	}

	ret = svc_handle->storage_ops.ufs_crypto_enable();
	pr_err("smc: enable cfg, ret:0x%x", ret);
#endif

	hba->quirks |= UFSHCD_QUIRK_BROKEN_UFS_HCI_VERSION |
		       UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS;

	hba->caps |= UFSHCD_CAP_CLK_GATING | UFSHCD_CAP_CRYPTO |
		     UFSHCD_CAP_HIBERN8_WITH_CLK_GATING | UFSHCD_CAP_WB_EN;

	return 0;

out_variant_clear:
	return ret;
}

/**
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
	int ret = 0;

	regmap_update_bits(host->aon_apb_ufs_clk_en.regmap,
			   host->aon_apb_ufs_clk_en.reg,
			   host->aon_apb_ufs_clk_en.mask,
			   0);

	ret = regulator_disable(host->vdd_mphy);
	if (ret)
		pr_err("disable vdd_mphy failed ret =0x%x!\n", ret);

	devm_kfree(dev, host);
	hba->priv = NULL;
}

static u32 ufs_sprd_get_ufs_hci_version(struct ufs_hba *hba)
{
	return UFSHCI_VERSION_30;
}

static int ufs_sprd_phy_sram_init_done(struct ufs_hba *hba)
{
	int ret = 0;
	uint32_t val = 0;
	uint32_t retry = 10;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	do {
		ret = regmap_read(host->phy_sram_init_done.regmap,
				  host->phy_sram_init_done.reg, &val);
		if (ret < 0)
			return ret;

		if ((val&0x1) == 0x1) {
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGADDRLSB), 0x1c);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGADDRMSB), 0x40);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGWRLSB), 0x04);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGWRMSB), 0x00);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGRDWRSEL), 0x01);
			ufshcd_dme_set(hba, UIC_ARG_MIB(VS_MPHYCFGUPDT), 0x01);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGADDRLSB), 0x1c);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGADDRMSB), 0x41);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGWRLSB), 0x04);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGWRMSB), 0x00);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGRDWRSEL), 0x01);
			ufshcd_dme_set(hba, UIC_ARG_MIB(VS_MPHYCFGUPDT), 0x01);

			return 0;
		} else {
			udelay(1000);
			retry--;
		}
	} while (retry > 0);
		return -1;
}

static int ufs_sprd_phy_init(struct ufs_hba *hba)
{
	int ret = 0;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	ufshcd_dme_set(hba, UIC_ARG_MIB(CBREFCLKCTRL2), 0x90);
	ufshcd_dme_set(hba, UIC_ARG_MIB(CBCRCTRL), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RXSQCONTROL,
		       UIC_ARG_MPHY_RX_GEN_SEL_INDEX(0)), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RXSQCONTROL,
		       UIC_ARG_MPHY_RX_GEN_SEL_INDEX(1)), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(VS_MPHYCFGUPDT), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(CBRATESEL), 0x01);

	ret = ufs_sprd_phy_sram_init_done(hba);
	if (ret)
		return ret;

	regmap_update_bits(host->phy_sram_ext_ld_done.regmap,
			   host->phy_sram_ext_ld_done.reg,
			   host->phy_sram_ext_ld_done.mask,
			   0);

	ufshcd_dme_set(hba, UIC_ARG_MIB(VS_MPHYCFGUPDT), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb0);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xaf);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118),
			    (host->ufs_lane_calib_data0 >> 24) & 0xff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb1);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb8);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb0);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x40);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118),
			    (host->ufs_lane_calib_data0 >> 24) & 0xff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb0);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xaf);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118),
			    (host->ufs_lane_calib_data1 >> 24) & 0xff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb1);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb8);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb0);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x41);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118),
			    (host->ufs_lane_calib_data1 >> 24) & 0xff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb0);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xaf);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118),
			    (host->ufs_lane_calib_data0 >> 16) & 0xff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb1);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x02);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb8);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb0);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x40);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118),
			    (host->ufs_lane_calib_data0 >> 16) & 0xff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb0);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xaf);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118),
			    (host->ufs_lane_calib_data1 >> 16) & 0xff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb1);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x02);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb8);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb0);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x41);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118),
			    (host->ufs_lane_calib_data1 >> 16) & 0xff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(VS_MPHYDISABLE), 0x0);

	return ret;
}

static int ufs_sprd_hce_enable_notify(struct ufs_hba *hba,
				      enum ufs_notify_change_status status)
{
	int err = 0;
#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
	int ret = 0;
	struct sprd_sip_svc_handle *svc_handle;
#endif

	switch (status) {
	case PRE_CHANGE:
		/* Do hardware reset before host controller enable. */
		ufs_sprd_hw_init(hba);
#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
		ufshcd_writel(hba, CONTROLLER_ENABLE, REG_CONTROLLER_ENABLE);
		svc_handle = sprd_sip_svc_get_handle();
		if (!svc_handle) {
			pr_err("%s: failed to get svc handle\n", __func__);
			return -ENODEV;
		}

		ret = svc_handle->storage_ops.ufs_crypto_enable();
		pr_err("smc: enable cfg, ret:0x%x", ret);
#endif
		break;
	case POST_CHANGE:
		 if (hba->vops->phy_initialization) {
			err = hba->vops->phy_initialization(hba);
			if (err) {
				dev_err(hba->dev, "Phy setup failed (%d)\n",
					err);
			}
		}
		break;
	default:
		dev_err(hba->dev, "%s: invalid status %d\n", __func__, status);
		err = -EINVAL;
		break;
	}

	return err;
}

static int ufs_sprd_link_startup_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status status)
{
	int err = 0;

	switch (status) {
	case PRE_CHANGE:
		break;
	case POST_CHANGE:
		break;
	default:
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
		memcpy(dev_req_params, dev_max_params,
				       sizeof(struct ufs_pa_layer_attr));
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
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	switch (status) {
	case PRE_CHANGE:
		if (cmd == UIC_CMD_DME_HIBER_ENTER) {
			ufshcd_writel(hba,
				0x0, REG_AUTO_HIBERNATE_IDLE_TIMER);
		}

		if (cmd == UIC_CMD_DME_HIBER_EXIT) {
			regmap_update_bits(host->ufsdev_refclk_en.regmap,
					   host->ufsdev_refclk_en.reg,
					   host->ufsdev_refclk_en.mask,
					   host->ufsdev_refclk_en.mask);

			regmap_update_bits(host->usb31pllv_ref2mphy_en.regmap,
					   host->usb31pllv_ref2mphy_en.reg,
					   host->usb31pllv_ref2mphy_en.mask,
					   host->usb31pllv_ref2mphy_en.mask);
		}
		break;
	case POST_CHANGE:
		if (cmd == UIC_CMD_DME_HIBER_EXIT) {
			ufshcd_writel(hba,
				AUTO_H8_IDLE_TIME_10MS,
				REG_AUTO_HIBERNATE_IDLE_TIMER);
		}

		if (cmd == UIC_CMD_DME_HIBER_ENTER) {
			regmap_update_bits(host->ufsdev_refclk_en.regmap,
					   host->ufsdev_refclk_en.reg,
					   host->ufsdev_refclk_en.mask,
					   0);

			regmap_update_bits(host->usb31pllv_ref2mphy_en.regmap,
					   host->usb31pllv_ref2mphy_en.reg,
					   host->usb31pllv_ref2mphy_en.mask,
					   0);
		}
		break;
	default:
		break;
	}
}

static void ufs_sprd_device_reset(struct ufs_hba *hba)
{
	return;
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
	.phy_initialization = ufs_sprd_phy_init,
	.hibern8_notify = ufs_sprd_hibern8_notify,
	.device_reset = ufs_sprd_device_reset,
};

/**
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

/**
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
	{ .compatible = "sprd,ufshc"},
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

MODULE_DESCRIPTION("SPRD Specific UFSHCI driver");
MODULE_LICENSE("GPL v2");
