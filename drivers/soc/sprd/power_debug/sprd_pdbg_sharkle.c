/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
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
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include "sprd_pdbg.h"
#include "sprd_pdbg_sc2721G.h"

#define AP_INTC_PMIC_INDEX 38

struct power_debug_sharkle {
	struct power_debug *pdbg;
	struct platform_device *pdev;
	struct power_debug_cfg *pcfg;
};

static struct power_debug_sharkle pdbg_sharkle;

static struct pdm_info sharkle_pdm_info[] = {
	{
		.addr_offset = 0x00bc,
		.pwd_bit_width = 5,
		.bit_index = {0, 5, 10, 15, 20, 25},
		.pdm_name = {"PD_CA53_TOP", "PD_CA53_C0",
			"PD_CA53_C1", "PD_CA53_C2",
			"PD_CA53_C3", "PD_AP_SYS"}
	},
	{
		.addr_offset = 0x00c0,
		.pwd_bit_width = 5,
		.bit_index = {0, 5, 10, 15, 20, 25},
		.pdm_name = {"PD_WTLCP_HU3GE_A", "PD_WTLCP_TGDSP",
			"PD_WTLCP_LDSP", "PD_WFI_WRAP",
			"PD_WTLCP_LTE_P2", "PD_WTLCP_LTE_P1"}
	},
	{
		.addr_offset = 0x00c4,
		.pwd_bit_width = 5,
		.bit_index = {0, 5, 10, 15, 20, 25},
		.pdm_name = {"PD_WTLCP_SYS", "PD_PUBCP_SYS",
			"PD_WTLCP_LTE_P3", "PD_WTLCP_LTE_P4",
			"PD_PUB_SYS", "PD_GNSS_WRAP"}
	}
};

static struct reg_check sharkle_ap_ahb_reg[] = {
	{
		.addr_offset = 0x0000,
		.value_mask = 0x0F7F,
		.expect_value = 0x00000000,
		.preg_name = "REG_AP_AHB_AHB_EB"
	},
	{
		.addr_offset = 0x3038,
		.value_mask = 0x01,
		.expect_value = 0x00000000,
		.preg_name = "REG_AP_AHB_CE_SEC_EB"
	}
};

static struct reg_check sharkle_ap_apb_reg[] = {
	{
		.addr_offset = 0x0000,
		.value_mask = 0x785FA3,
		.expect_value = 0x00000000,
		.preg_name = "REG_AP_APB_APB_EB"
	}
};

static struct reg_check sharkle_pmu_apb_reg[] = {
	{
		.addr_offset = 0x00cc,
		.value_mask = 0x66,
		.expect_value = 0x00000000,
		.preg_name = "REG_PMU_APB_SLEEP_CTRL"
	}
};

static struct reg_check sharkle_aon_apb_reg[] = {
	{
		.addr_offset = 0x0000,
		.value_mask = 0xA000000,
		.expect_value = 0x00000000,
		.preg_name = "REG_AON_APB_APB_EB0"
	}
};

static struct intc_info sharkle_pintc_info[] = {
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "DFS_EXIT", "AP_UART1",
		"SLV_AON", "DFS_ERR", "DFS_COMPLETE", "AP_SPI0",
		"AP_SPI1", "AP_SPI2", "AP_SIM0", "AP_I2C0",
		"AP_I2C1", "AP_I2C2", "AP_I2C3", "AP_I2C4",
		"AP_IIS0", "SLV_AP", "MEM_FW_PUB", "MEM_FW_AON",
		"AUD", "VBC_ERR", "VBC_DA01", "VBC_AD01",
		"AP_MDAR", "ADI", "THM0", "ARM_INT",
		"AON_TMR", "AP_TMR0", "AON_SYST", "AP_SYST"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "BUSMON_AON", "GPIO",
		"KPD", "AON_EIC", "ANA", "GPU",
		"CSI0", "CSI1", "JPG", "VSP",
		"ISP_CH0", "DCAM0", "DISPC", "DSI_PLL",
		"DSI_0", "DSI_1", "AP_REQ_DMA", "GSP",
		"SEC_REQ_DMA", "CE_SEC", "CE_PUB", "OTG",
		"AON_SEC_RTC", "SDIO0", "SDIO1", "NANDC",
		"EMMC", "DMC_MPU_VIO", "SEC_EIC", "SEC_GPIO"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "DCAM1", "DJATG",
		"MBOX_SRC_AP", "MBOX_TAR_AP", "AON_DMA_AP", "CSI2_R2_S",
		"CSI2_R1_S", "AON_SEC_TMR2", "AON_SEC_TMR1", "AON_SEC_TMR0",
		"CP2_WDG", "THM1", "START_CHN0", "START_CHN1",
		"START_CHN2", "START_CHN3", "SEC_AON_REQ_DMA", "CP0_WDG",
		"CP1_WDG", "AON_SEC_WDG", "PUB_BUSMON", "null",
		"PWRUP_AP", "PWRUP_PUB", "PWRUP_ALL", "PWRDW_ALL",
		"NPMUIRQ0", "NPMUIRQ1", "NPMUIRQ2", "NPMUIRQ3"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "NCTIIRQ0", "NCTIIRQ1",
		"NCTIIRQ2", "NCTIIRQ3", "NCNTVIRQ0", "NCNTVIRQ1",
		"NCNTVIRQ2", "NCNTVIRQ3", "NCNTHPIRQ0", "NCNTHPIRQ1",
		"NCNTHPIRQ2", "NCNTHPIRQ3", "NCNTPNSIRQ0", "NCNTPNSIRQ1",
		"NCNTPNSIRQ2", "NCNTPNSIRQ3", "NCNTPSIRQ0", "NCNTPSIRQ1",
		"NCNTPSIRQ2", "NCNTPSIRQ3", "AP_TMR1", "AP_TMR2",
		"AP_TMR3", "AP_TMR4", "AVS", "AP_WDG",
		"CA53_WDG", "ISP_CH1", "nEXTRRIRQ", "DFI_BUSMON_PUB"}
	}
};

static void sharkle_output_2nd_irq_source(void *pentry, u32 hw_irq_nr)
{
	struct power_debug *pdbg = (struct power_debug *)pentry;

	if (pdbg_sharkle.pdbg != pdbg)
		return;

	if (hw_irq_nr == AP_INTC_PMIC_INDEX)
		sc2721G_output_irq_source();
}

static struct power_debug_desc sharkle_pdbg_desc = {
	.name = "Sharkle-pdbg",

	.pmu_pdm_num = sizeof(sharkle_pdm_info)/sizeof(struct pdm_info),
	.ap_ahb_reg_num = sizeof(sharkle_ap_ahb_reg)/sizeof(struct reg_check),
	.ap_apb_reg_num = sizeof(sharkle_ap_apb_reg)/sizeof(struct reg_check),
	.pmu_apb_reg_num = sizeof(sharkle_pmu_apb_reg)/sizeof(struct reg_check),
	.aon_apb_reg_num = sizeof(sharkle_aon_apb_reg)/sizeof(struct reg_check),
	.ap_intc_num = sizeof(sharkle_pintc_info)/sizeof(struct intc_info),
	.irq_mask = {0, 0x40, 0, 0, 0, 0, 0, 0},

	.ppdm_info = sharkle_pdm_info,
	.ap_ahb_reg = sharkle_ap_ahb_reg,
	.ap_apb_reg = sharkle_ap_apb_reg,
	.pmu_apb_reg = sharkle_pmu_apb_reg,
	.aon_apb_reg = sharkle_aon_apb_reg,
	.pintc_info = sharkle_pintc_info,
	.log_2nd_irq_source = sharkle_output_2nd_irq_source
};

/**
 * dev - Parse the dts node information of this driver, and
 * construct the core structure used in this driver.
 * intc_num - the intc number of ap in aon system
 */
static struct power_debug_cfg *sprd_pdbg_parse_cfg(struct device *dev,
					u32 intc_num)
{
	int result;
	u32 i;
	struct power_debug_cfg *pcfg;
	struct device_node *psub_node;
	struct device_node *pnode = dev->of_node;

	if (!pnode) {
		dev_err(dev, "Power debug device node not found\n");
		return ERR_PTR(-ENODEV);
	}

	pcfg = devm_kzalloc(dev, (sizeof(struct power_debug_cfg) +
			intc_num * sizeof(struct regmap *)), GFP_KERNEL);
	if (!pcfg)
		return ERR_PTR(-ENOMEM);

	result = of_property_read_u32(
		pnode, "sprd,enable", &pcfg->pdbg_enable);
	if (result)
		pcfg->pdbg_enable = 1;

	result = of_property_read_u32(
		pnode, "sprd,scan-interval", &pcfg->scan_interval);
	if (result)
		pcfg->scan_interval = 30;

	pcfg->ap_ahb = syscon_regmap_lookup_by_phandle(
		pnode, "sprd,sys-ap-ahb");
	if (IS_ERR(pcfg->ap_ahb)) {
		dev_warn(dev, "Not to get ap-ahb regmap\n");
		pcfg->ap_ahb = NULL;
	}

	pcfg->ap_apb = syscon_regmap_lookup_by_phandle(
		pnode, "sprd,sys-ap-apb");
	if (IS_ERR(pcfg->ap_apb)) {
		dev_warn(dev, "Not to get ap-apb regmap\n");
		pcfg->ap_apb = NULL;
	}

	pcfg->pmu_apb = syscon_regmap_lookup_by_phandle(
		pnode, "sprd,sys-pmu-apb");
	if (IS_ERR(pcfg->pmu_apb)) {
		dev_err(dev, "Failed to get pmu-apb regmap\n");
		devm_kfree(dev, pcfg);
		return ERR_PTR(-EINVAL);
	}

	pcfg->aon_apb = syscon_regmap_lookup_by_phandle(
		pnode, "sprd,sys-aon-apb");
	if (IS_ERR(pcfg->aon_apb)) {
		dev_warn(dev, "Not to get aon-apb regmap\n");
		pcfg->aon_apb = NULL;
	}

	pcfg->aon_sec = syscon_regmap_lookup_by_phandle(
		pnode, "sprd,sys-aon-sec");
	if (IS_ERR(pcfg->aon_sec)) {
		dev_warn(dev, "Not to get aon-sec regmap\n");
		pcfg->aon_sec = NULL;
	}

	for (i = 0; i < intc_num; i++) {
		psub_node = of_parse_phandle(pnode, "sprd,sys-ap-intc", i);
		if (psub_node) {
			pcfg->ap_intc[i] = syscon_node_to_regmap(psub_node);
			of_node_put(psub_node);
			if (IS_ERR(pcfg->ap_intc[i])) {
				dev_err(dev,
					"Failed to get ap-intc[%d] regmap\n",
					i);
				devm_kfree(dev, pcfg);
				return ERR_PTR(-EINVAL);
			}
		} else {
			devm_kfree(dev, pcfg);
			dev_err(dev,
				"Device node ap-intc[%d] not found\n", i);
			return ERR_PTR(-ENODEV);
		}
	}

	return pcfg;
}

/**
 * sprd_powerdebug_probe - add the power debug driver
 */
static int sprd_pdbg_sharkle_probe(struct platform_device *pdev)
{
	struct power_debug_cfg *pcfg;

	dev_dbg(&pdev->dev, "##### Power debug driver init start #####\n");

	pcfg = sprd_pdbg_parse_cfg(&pdev->dev, sharkle_pdbg_desc.ap_intc_num);
	if (IS_ERR(pcfg))
		return (int)pcfg;

	pdbg_sharkle.pdbg = sprd_power_debug_register(&pdev->dev,
				&sharkle_pdbg_desc,	pcfg);
	if (!pdbg_sharkle.pdbg) {
		devm_kfree(&pdev->dev, pcfg);
		dev_dbg(&pdev->dev, "##### Power debug driver init failure #####\n");
		return -EFAULT;
	}
	pdbg_sharkle.pcfg = pcfg;
	pdbg_sharkle.pdev = pdev;

	dev_dbg(&pdev->dev, "##### Power debug driver init successfully #####\n");

	return 0;
}

/**
 * sprd_powerdebug_remove - remove the power debug driver
 */
static int sprd_pdbg_sharkle_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "##### Power debug driver remove #####\n");
	sprd_power_debug_unregister(pdbg_sharkle.pdbg);
	pdbg_sharkle.pdbg = NULL;

	devm_kfree(&pdev->dev, pdbg_sharkle.pcfg);
	pdbg_sharkle.pcfg = NULL;
	pdbg_sharkle.pdev = NULL;

	return 0;
}

static const struct of_device_id sprd_pdbg_sharkle_of_match[] = {
	{
		.compatible = "sprd,power-debug-sharkle",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_pdbg_sharkle_of_match);

static struct platform_driver sprd_pdbg_sharkle_driver = {
	.probe = sprd_pdbg_sharkle_probe,
	.remove = sprd_pdbg_sharkle_remove,
	.driver = {
		.name = "sprd-powerdebug",
		.of_match_table = sprd_pdbg_sharkle_of_match,
	},
};

module_platform_driver(sprd_pdbg_sharkle_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jamesj Chen<Jamesj.Chen@unisoc.com>");
MODULE_DESCRIPTION("sprd sharkle power debug driver");
