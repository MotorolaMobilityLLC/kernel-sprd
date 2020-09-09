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
#include "sprd_pdbg_sc2721.h"

#define AP_INTC_PMIC_INDEX 38

struct power_debug_sharkl3 {
	struct power_debug *pdbg;
	struct platform_device *pdev;
	struct power_debug_cfg *pcfg;
};

static struct power_debug_sharkl3 pdbg_sharkl3;

static struct pdm_info sharkl3_pdm_info[] = {
	{
		.addr_offset = 0x00bc,
		.pwd_bit_width = 5,
		.bit_index = {0, 5, 10, 17, 22, 27, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_CPU_TOP_STATE", "PD_CA53_LIT_MP2_STATE",
			"PD_CA53_BIG_MP2_STATE", "PD_AP_SYS_STATE",
			"PD_GPU_TOP_STATE", "PD_MM_TOP_STATE"}
	},
	{
		.addr_offset = 0x00c0,
		.pwd_bit_width = 5,
		.bit_index = {0, 5, 10, 15, 20, 25, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_WTLCP_HU3GE_B_STATE", "PD_WTLCP_HU3GE_A_STATE",
			"PD_WTLCP_TGDSP_STATE", "PD_WTLCP_LDSP_STATE",
			"PD_WTLCP_LTE_P2_STATE", "PD_WTLCP_LTE_P1_STATE"}
	},
	{
		.addr_offset = 0x00c4,
		.pwd_bit_width = 5,
		.bit_index = {0, 5, 10, 15, 20, 25, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_WTLCP_SYS", "PD_PUBCP_SYS",
			"PD_WTLCP_LTE_P3", "PD_DISP",
			"PD_PUB_SYS", "PD_WTLCP_TD"}
	},
	{
		.addr_offset = 0x048c,
		.pwd_bit_width = 5,
		.bit_index = {0, 5, 10, 15, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_CPU_LIT_C0", "PD_CPU_LIT_C1",
			"PD_CPU_BIG_C0", "PD_CPU_BIG_C1"}
	},
	{
		.addr_offset = 0x0490,
		.pwd_bit_width = 5,
		.bit_index = {0, 5, 10, 15, 20, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_DBG_SYS", "PD_CPU_MP8",
			"PD_WCN_SYS", "PD_WIFI_WRAP",
			"PD_GNSS_WRAP"}
	},
	{
		.addr_offset = 0x0128,
		.pwd_bit_width = 5,
		.bit_index = {0, 5, 10, 15, 20, 25, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_MM_VSP", "PD_GPU_CORE",
			"PD_CPU_LIT_C2", "PD_CPU_LIT_C3",
			"PD_CPU_BIG_C2", "PD_CPU_BIG_C3"}
	}
};

static struct reg_check sharkl3_ap_ahb_reg[] = {
	{
		.addr_offset = 0x0000,
		.value_mask = 0xF8000FF,
		.expect_value = 0x00000000,
		.preg_name = "REG_AP_AHB_AHB_EB"
	}
};

static struct reg_check sharkl3_ap_apb_reg[] = {
	{
		.addr_offset = 0x0000,
		.value_mask = 0x3FFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_AP_APB_APB_EB"
	}
};

static struct reg_check sharkl3_pmu_apb_reg[] = {
	{
		.addr_offset = 0x00CC,
		.value_mask = 0x7F3F3F3F,
		.expect_value = 0x7F3F3F3F,
		.preg_name = "REG_PMU_APB_SLEEP_CTRL"
	},
	{
		.addr_offset = 0x00D4,
		.value_mask = 0xF0FFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_PMU_APB_SLEEP_STATUS"
	},
	{
		.addr_offset = 0x0270,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0xFFFFFFFF,
		.preg_name = "REG_PMU_APB_PMU_DEBUG"
	},
	{
		.addr_offset = 0x00D0,
		.value_mask = 0xFFC11D77,
		.expect_value = 0x00000000,
		.preg_name = "REG_PMU_APB_DDR_SLEEP_CTRL"
	},
	{
		.addr_offset = 0x02A0,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_PMU_APB_SLEEP_CNT0"
	},
	{
		.addr_offset = 0x02A4,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_PMU_APB_SLEEP_CNT1"
	},
	{
		.addr_offset = 0x02A8,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_PMU_APB_SLEEP_CNT2"
	},
	{
		.addr_offset = 0x02AC,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_PMU_APB_SLEEP_CNT3"
	},
	{
		.addr_offset = 0x02B0,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_PMU_APB_SLEEP_CNT4"
	},
	{
		.addr_offset = 0x02B4,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_PMU_APB_SLEEP_CNT5"
	},
	{
		.addr_offset = 0x02B8,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_PMU_APB_SLEEP_CNT6"
	},
	{
		.addr_offset = 0x02BC,
		.value_mask = 0xFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_PMU_APB_SLEEP_CNT7"
	},
	{
		.addr_offset = 0x02C0,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_PMU_APB_SLEEP_CNT8"
	},
	{
		.addr_offset = 0x02C4,
		.value_mask = 0xFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_PMU_APB_SLEEP_CNT9"
	}
};

static struct reg_check sharkl3_aon_apb_reg[] = {
	{
		.addr_offset = 0x0000,
		.value_mask = 0xF7FFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_AON_APB_APB_EB0"
	},
	{
		.addr_offset = 0x0004,
		.value_mask = 0xFFFFFFBF,
		.expect_value = 0x00000000,
		.preg_name = "REG_AON_APB_APB_EB1"
	},
	{
		.addr_offset = 0x00B0,
		.value_mask = 0x1FFFEFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_AON_APB_APB_EB2"
	}
};

static struct intc_info sharkl3_pintc_info[] = {
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "AP_UART0", "AP_UART1",
		"AP_UART2", "AP_UART3", "AP_UART4", "AP_SPI0",
		"AP_SPI1", "AP_SPI2", "AP_SIM", "AP_I2C0",
		"AP_I2C1", "AP_I2C2", "AP_I2C3", "AP_I2C4",
		"AP_IIS0", "AP_IIS1", "AP_IIS2", "AP_SPI3",
		"AUD", "AON_VBC_DAC23", "AON_VBC_DAC01",
		"AON_VBC_ADC01", "null", "ADI", "THM0",
		"AON_TMR0", "AON_TMR1", "AON_TMR2",
		"AON_FOR_AP", "AP_SYST"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "I2C", "GPIO",
		"KPD", "EIC", "ANA", "GPU", "CSI2_R1",
		"CSI2_R2", "JPG", "VSP", "ISP_CH0", "DCAM0",
		"AP_DISPC", "AP_DSI_PLL", "AP_DSI_0",
		"AP_DSI_1", "AP_DMA", "AP_GSP", "AP_SEC_DMA",
		"AP_CE_SEC", "AP_CE_PUB", "AP_OTG",
		"AON_SEC_CNT", "AP_SDIO0", "AP_SDIO1",
		"AP_SDIO2", "AP_EMMC", "MPU_VIO",
		"SEC_EIC", "SEC_GPIO"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "AP_NANDC", "null",
		"MBOX_SRC_AP", "MBOX_TAR_AP", "AON_DMA_AP",
		"CSI2_R2_S", "CSI2_R1_S", "SEC_RME2",
		"SEC_TMR1", "SEC_TMR0", "CSI_CAL_DONE",
		"null", "CHN_START_CHN_0", "CHN_START_CHN_1",
		"CHN_START_CHN_2", "CHN_START_CHN_3",
		"SEC_REQ_DMA", "PCP_WDG", "CP1_WDG",
		"SEC_WDG", "PUB_BUSMON", "APCPU_BUSMON",
		"null", "null", "WTLCP_BUSMON", "PUBCP_BUSMON",
		"AON_ANY_BUSMON", "BUSMON_AXI_BDG",
		"GPU_BUSMON", "CSI2_CAL_FAILED"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "NCOMMIR_0",
		"NCOMMIR_1", "NCOMMIR_2", "NCOMMIR_3",
		"NCOMMIR_4", "NCOMMIR_5", "NCOMMIR_6",
		"NCOMMIR_7", "NCTIIRQ_0", "NCTIIRQ_1",
		"NCTIIRQ_2", "NCTIIRQ_3", "NCTIIRQ_4",
		"NCTIIRQ_5", "NCTIIRQ_6", "NCTIIRQ_7",
		"WDG_WCN_GNSS", "WDG_WCN_BTWF", "AP_I2C5",
		"AP_I2C6", "null", "MBOX_TAR_AP_UNWAKE",
		"null", "DVFS_IRQ_LVL", "SCC", "AP_WDG",
		"CA53_WDG", "ISP_CH1", "CPP", "CLK_32K_DET"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "GPIO_PLUS_AP",
		"GPIO_PLUS_AP_SEC", "null", "null", "null",
		"null", "NCNTVIRQ_0", "NCNTVIRQ_1",
		"NCNTVIRQ_2", "NCNTVIRQ_3", "NCNTVIRQ_4",
		"NCNTVIRQ_5", "NCNTVIRQ_6", "NCNTVIRQ_7",
		"NPMUIRQ_0", "NPMUIRQ_1", "NPMUIRQ_2",
		"NPMUIRQ_3", "NPMUIRQ_4", "NPMUIRQ_5",
		"NPMUIRQ_6", "NPMUIRQ_7", "SLV_FW", "MEM_FW",
		"NCLUSTERPMUIRQ", "SYSMON_EVENT_IRQ_AP",
		"CSI2_R2_T", "CSI2_R1_T", "DCAM1", "DCAM2"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "APCPU_GIC_PMU_INT",
		"APCPU_GIC_FAULT_INT", "NCNTHPIRQ_0",
		"NCNTHPIRQ_1", "NCNTHPIRQ_2", "NCNTHPIRQ_3",
		"NCNTHPIRQ_4", "NCNTHPIRQ_5", "NCNTHPIRQ_6",
		"NCNTHPIRQ_7", "NCNTPSIRQ_0", "NCNTPSIRQ_1",
		"NCNTPSIRQ_2", "NCNTPSIRQ_3", "NCNTPSIRQ_4",
		"NCNTPSIRQ_5", "NCNTPSIRQ_6", "NCNTPSIRQ_7",
		"NCNTPNSIRQ_0", "NCNTPNSIRQ_1", "NCNTPNSIRQ_2",
		"NCNTPNSIRQ_3", "NCNTPNSIRQ_4", "NCNTPNSIRQ_5",
		"NCNTPNSIRQ_6", "NCNTPNSIRQ_7",
		"APCPU_GIC_ERR_INT", "APCPU_CLUSTER_NFAULT",
		"APCPU_CLUSTER_NERRIRQ", "null"}
	}
};

static void sharkl3_output_2nd_irq_source(void *pentry, u32 hw_irq_nr)
{
	struct power_debug *pdbg = (struct power_debug *)pentry;

	if (pdbg_sharkl3.pdbg != pdbg)
		return;

	if (hw_irq_nr == AP_INTC_PMIC_INDEX)
		sc2721_output_irq_source();
}

static struct power_debug_desc sharkl3_pdbg_desc = {
	.name = "sharkl3-pdbg",

	.pmu_pdm_num = sizeof(sharkl3_pdm_info)/sizeof(struct pdm_info),
	.ap_ahb_reg_num = sizeof(sharkl3_ap_ahb_reg)/sizeof(struct reg_check),
	.ap_apb_reg_num = sizeof(sharkl3_ap_apb_reg)/sizeof(struct reg_check),
	.pmu_apb_reg_num = sizeof(sharkl3_pmu_apb_reg)/sizeof(struct reg_check),
	.aon_apb_reg_num = sizeof(sharkl3_aon_apb_reg)/sizeof(struct reg_check),
	.ap_intc_num = sizeof(sharkl3_pintc_info)/sizeof(struct intc_info),
	.irq_mask = {0, 0x40, 0, 0, 0, 0, 0, 0},

	.ppdm_info = sharkl3_pdm_info,
	.ap_ahb_reg = sharkl3_ap_ahb_reg,
	.ap_apb_reg = sharkl3_ap_apb_reg,
	.pmu_apb_reg = sharkl3_pmu_apb_reg,
	.aon_apb_reg = sharkl3_aon_apb_reg,
	.pintc_info = sharkl3_pintc_info,
	.log_2nd_irq_source = sharkl3_output_2nd_irq_source
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
static int sprd_pdbg_sharkl3_probe(struct platform_device *pdev)
{
	struct power_debug_cfg *pcfg;

	dev_dbg(&pdev->dev, "##### Power debug driver init start #####\n");

	pcfg = sprd_pdbg_parse_cfg(&pdev->dev, sharkl3_pdbg_desc.ap_intc_num);
	if (IS_ERR(pcfg))
		return (int)pcfg;

	pdbg_sharkl3.pdbg = sprd_power_debug_register(&pdev->dev,
				&sharkl3_pdbg_desc,	pcfg);
	if (!pdbg_sharkl3.pdbg) {
		devm_kfree(&pdev->dev, pcfg);
		dev_dbg(&pdev->dev, "##### Power debug driver init failure #####\n");
		return -EFAULT;
	}
	pdbg_sharkl3.pcfg = pcfg;
	pdbg_sharkl3.pdev = pdev;

	dev_dbg(&pdev->dev, "##### Power debug driver init successfully #####\n");

	return 0;
}

/**
 * sprd_powerdebug_remove - remove the power debug driver
 */
static int sprd_pdbg_sharkl3_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "##### Power debug driver remove #####\n");
	sprd_power_debug_unregister(pdbg_sharkl3.pdbg);
	pdbg_sharkl3.pdbg = NULL;

	devm_kfree(&pdev->dev, pdbg_sharkl3.pcfg);
	pdbg_sharkl3.pcfg = NULL;
	pdbg_sharkl3.pdev = NULL;

	return 0;
}

static const struct of_device_id sprd_pdbg_sharkl3_of_match[] = {
	{
		.compatible = "sprd,power-debug-sharkl3",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_pdbg_sharkl3_of_match);

static struct platform_driver sprd_pdbg_sharkl3_driver = {
	.probe = sprd_pdbg_sharkl3_probe,
	.remove = sprd_pdbg_sharkl3_remove,
	.driver = {
		.name = "sprd-powerdebug",
		.of_match_table = sprd_pdbg_sharkl3_of_match,
	},
};

module_platform_driver(sprd_pdbg_sharkl3_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jamesj Chen<Jamesj.Chen@unisoc.com>");
MODULE_DESCRIPTION("sprd sharkl3 power debug driver");
