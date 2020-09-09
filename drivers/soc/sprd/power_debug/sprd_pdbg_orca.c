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
#include "sprd_pdbg_sc2730.h"

#define AP_INTC_PMIC_INDEX 173

struct power_debug_orca {
	struct power_debug *pdbg;
	struct platform_device *pdev;
	struct power_debug_cfg *pcfg;
};

static struct power_debug_orca pdbg_orca;

static struct pdm_info orca_pdm_info[] = {
	{
		.addr_offset = 0x0584,
		.pwd_bit_width = 8,
		.bit_index = {0, 8, 16, 24, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_AP_SYS", "PD_AP_VDSP",
			"PD_AP_VSP", "PD_NRCP_SYS"}
	},
	{
		.addr_offset = 0x0588,
		.pwd_bit_width = 8,
		.bit_index = {0, 8, 16, 24, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_NRCP_DSP0", "PD_NRCP_DSP1",
			"PD_NRCP_DL", "PD_NRCP_UL"}
	},
	{
		.addr_offset = 0x058C,
		.pwd_bit_width = 8,
		.bit_index = {0, 8, 16, 24, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_AUDCP_SYS", "PD_AUDCP_AUDDSP",
			"PD_PSCP_SYS", "PD_PUB_SYS"}
	},
	{
		.addr_offset = 0x0590,
		.pwd_bit_width = 8,
		.bit_index = {0, 8, 16, 24, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_V3_LWPROC", "PD_V3_WCE",
			"PD_V3_DPFEC", "PD_V3_LCE"}
	},
	{
		.addr_offset = 0x0594,
		.pwd_bit_width = 8,
		.bit_index = {0, 8, 16, 24, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_NRCP_SYNC", "PD_V3_MODEM",
			"PD_V3_PS", "PD_V3_PHY"}
	},
	{
		.addr_offset = 0x059C,
		.pwd_bit_width = 8,
		.bit_index = {0, 8, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_APCPU_C0", "PD_APCPU_C1"}
	},
	{
		.addr_offset = 0x05A4,
		.pwd_bit_width = 8,
		.bit_index = {0, 8, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_APCPU_TOP", "PD_V3_TD"}
	},
	{
		.addr_offset = 0x05B0,
		.pwd_bit_width = 8,
		.bit_index = {0, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"APCPU_CORE0", "APCPU_CORE1"}
	},
	{
		.addr_offset = 0x05B8,
		.pwd_bit_width = 8,
		.bit_index = {0, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"APCPU_CORINTH"}
	}
};

static struct reg_check orca_ap_ahb_reg[] = {
	{
		.addr_offset = 0x0000,
		.value_mask = 0x1FFF,
		.expect_value = 0x1,
		.preg_name = "REG_AP_AHB_RF_AHB_EB"
	},
	{
		.addr_offset = 0x0100,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_RF_MMTX_M0_LPC"
	},
	{
		.addr_offset = 0x0104,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_RF_MMTX_M1_LPC"
	},
	{
		.addr_offset = 0x0108,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_RF_MMTX_M2_LPC"
	},
	{
		.addr_offset = 0x010C,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_RF_MMTX_M3_LPC"
	},
	{
		.addr_offset = 0x0110,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_RF_MMTX_M4_LPC"
	},
	{
		.addr_offset = 0x0114,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_RF_MMTX_M5_LPC"
	},
	{
		.addr_offset = 0x0118,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_RF_MMTX_M6_LPC"
	},
	{
		.addr_offset = 0x0140,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_RF_MMTX_S0_LPC"
	},
	{
		.addr_offset = 0x0144,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_RF_MMTX_S1_LPC"
	},
	{
		.addr_offset = 0x0148,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_RF_MMTX_S2_LPC"
	},
	{
		.addr_offset = 0x014C,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_RF_MMTX_S3_LPC"
	},
	{
		.addr_offset = 0x0150,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_RF_MMTX_S4_LPC"
	},
	{
		.addr_offset = 0x0154,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_RF_MMTX_S5_LPC"
	},
	{
		.addr_offset = 0x0158,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_RF_MMTX_S6_LPC"
	}
};

static struct reg_check orca_ap_apb_reg[] = {
	{
		.addr_offset = 0x0000,
		.value_mask = 0x7FFFE,
		.expect_value = 0x00,
		.preg_name = "REG_AP_APB_RF_APB_EB"
	}
};

static struct reg_check orca_pmu_apb_reg[] = {
	{
		.addr_offset = 0x00C8,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00,
		.preg_name = "REG_PMU_APB_RF_PUB_SYS_AUTO_LIGHT_SLEEP_ENABLE"
	},
	{
		.addr_offset = 0x00CC,
		.value_mask = 0xFF,
		.expect_value = 0x00,
		.preg_name = "REG_PMU_APB_RF_SLEEP_CTRL"
	},
	{
		.addr_offset = 0x00D4,
		.value_mask = 0x0FFFFFFF,
		.expect_value = 0x00,
		.preg_name = "REG_PMU_APB_RF_SLEEP_STATUS"
	},
	{
		.addr_offset = 0x0230,
		.value_mask = 0x21FF7F,
		.expect_value = 0x00,
		.preg_name = "REG_PMU_APB_RF_LIGHT_SLEEP_ENABLE"
	},
	{
		.addr_offset = 0x05D0,
		.value_mask = 0x01,
		.expect_value = 0x01,
		.preg_name = "REG_PMU_APB_RF_AXI_LP_CTRL_DISABLE"
	}
};

static struct reg_check orca_aon_apb_reg[] = {
	{
		.addr_offset = 0x0000,
		.value_mask = 0xA0,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_RF_APB_EB0"
	},
	{
		.addr_offset = 0x0758,
		.value_mask = 0x01,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_RF_APCPU_DEBUG_PWR_LP_CTRL"
	},
	{
		.addr_offset = 0x0760,
		.value_mask = 0x01,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_RF_APCPU_CLUSTER_ATB_LPC_CTRL"
	},
	{
		.addr_offset = 0x0764,
		.value_mask = 0x01,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_RF_APCPU_CLUSTER_APB_LPC_CTRL"
	},
	{
		.addr_offset = 0x0768,
		.value_mask = 0x01,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_RF_APCPU_CLUSTER_GIC_LPC_CTRL"
	},
	{
		.addr_offset = 0x076c,
		.value_mask = 0x01,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_RF_APCPU_GIC600_GIC_LPC_CTRL"
	},
	{
		.addr_offset = 0x0770,
		.value_mask = 0x01,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_RF_APCPU_DBG_BLK_LPC_CTRL"
	},
	{
		.addr_offset = 0x0774,
		.value_mask = 0x01FF,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_RF_APCPU_TOP_MTX_M0_LPC_CTRL"
	},
	{
		.addr_offset = 0x0778,
		.value_mask = 0x01,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_RF_APCPU_CLUSTER_SCU_LPC_CTRL"
	},
	{
		.addr_offset = 0x077C,
		.value_mask = 0x1,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_RF_APCPU_DDR_AB_LPC_CTRL"
	}
};

static struct intc_info orca_pintc_info[] = {
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "null", "AP_UART0",
		"AP_DOORBELL", "AP_PCIE_MIX",
		"AP_PCIE_CFG_AER_RC_ERR_MSI", "AP_SPI0",
		"AP_SPI1", "AP_SPI2", "AP_SIM",
		"AP_I2C0", "AP_I2C1", "AP_I2C2", "AP_I2C3",
		"AP_I2C4", "AP_USB00", "AP_USB01",
		"AP_USB02", "AP_USB10", "AP_USB11", "AP_USB12",
		"AP_PCIE_MSI_CTRL", "AP_PAMU3_COMFIFO",
		"AP_PCIE_CFG_AER_RC_ERR", "PCIE_CFG_AER_MSG_NUM0",
		"PCIE_CFG_AER_MSG_NUM1", "PCIE_CFG_AER_MSG_NUM2",
		"PCIE_CFG_AER_MSG_NUM3", "PCIE_CFG_AER_MSG_NUM4",
		"AP_PCIE_RX_MSG", "AP_PCIE_ERR"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "CFG_LINK_AUTO_BW",
		"CFG_DISABLE", "PCIE_CFG_BW_MGT", "null",
		"AON_SYS_APB_BUSMON_S3", "AON_SYS_APB_BUSMON_S5",
		"AON_SYS_ACC_PROT_AON_APB",
		"AON_SYS_ACC_PROT_PMU_APB", "CFG_PME", "HP",
		"AP_IPA_CP1", "AP_IPA_CP0", "AP_IPA_MAP",
		"null", "AP_PAM_WIFI_TO_AP", "AP_PAMU3", "AP_DMA",
		"AP_SDIO_SLV_PUB_AP", "AP_DMA_SEC",
		"AP_SDIO_SLV_DEDICATE0", "AP_SDIO_SLV_DEDICATE1",
		"AP_PCIE_SYS", "BUSMON_APSYS", "AP_SDIO_MST",
		"AP_SDSLV_TO_CP0", "AP_NANDC", "AP_EMMC", "GPIO",
		"AON_SYS_THM0", "AON_SYS_THM1"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = { "null", "null", "AON_SYS_THM2",
		"AON_SYS_KPD", "AON_SYS_I2C", "null", "ADI",
		"AON_SYS_TMR", "AON_EIC_EIC", "AON_SYS_AP_TMR0",
		"AON_SYS_AP_TMR1", "AON_SYS_AP_TMR2",
		"AON_SYS_AP_TMR3", "AON_SYS_AP_TMR4",
		"AON_SYS_AP_SYST", "APCPU_WDG", "AON_SYS_AP_WDG",
		"BUSMON", "AON_SYS_MBOX_SRC_AP",
		"AON_SYS_MBOX_TAR_AP", "null", "PWR_UP_AP",
		"PWR_UP_PUB", "PWR_UP_ALL", "PWR_DOWN_ALL",
		"null", "null", "null", "null", "null", "null",
		"ETR_AXI_MON"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "null", "CLK_32K_DET",
		"AON_SYS_SCC", "null", "null",
		"AON_SYS_CHIP_RESET_CTRL_APCPU", "AON_SYST_SYST",
		"MEM_FW_PUB", "PUB_HARDWARE_DFS_EXIT",
		"PUB_DFS_ERROR", "PUB_DFS_COMPLETE", "PUB_PTM",
		"DFI_BUS_MONITOR_PUB", "DMC_MPU_VIO", "null",
		"AUD_CHN_START_CHN0", "AUD_CHN_START_CHN1",
		"AUD_CHN_START_CHN2", "AUD_CHN_START_CHN3",
		"AUD_DMA", "AUD_MCDT_AP", "AUD_VBC_AUDRCD",
		"AUD_VBC_AUDPLY", "AUD_WDG_RST", "null", "null",
		"AON_EIC_EIC_NON_LAT", "PUB_DFS_GIVEUP",
		"PUB_DFS_DENY", "DFS_VOTE_DONE"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "ANA", "null", "null",
		"null", "null", "null", "null", "null", "null",
		"null", "null", "null", "null", "null", "null",
		"null", "null", "null", "null", "null", "null",
		"null", "null", "null", "null", "null", "null",
		"null", "null", "null"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "null", "null", "null",
		"APCPU_BUSMON", "APCPU_PMU", "APCPU_ERR",
		"APCPU_FAULT", "null", "null", "NERRIRQ0",
		"NERRIRQ1", "NERRIRQ2", "NFAULTIRQ0",
		"NFAULTIRQ1", "NFAULTIRQ2", "NCLUSTERPMUIRQ",
		"NPMUIRQ0", "NPMUIRQ1", "NCOMMIRQ0", "NCOMMIRQ1",
		"NCTIIRQ0", "NCTIIRQ1", "NCNTHPIRQ0",
		"NCNTHPIRQ1", "NCNTVIRQ0", "NCNTVIRQ1",
		"NCNTPNSIRQ0", "NCNTPNSIRQ1", "NCNTPSIRQ0",
		"NCNTPSIRQ1"}
	}
};

static void orca_output_2nd_irq_source(void *pentry, u32 hw_irq_nr)
{
	struct power_debug *pdbg = (struct power_debug *)pentry;

	if (pdbg_orca.pdbg != pdbg)
		return;

	if (hw_irq_nr == AP_INTC_PMIC_INDEX)
		sc2730_output_irq_source();
}

static struct power_debug_desc orca_pdbg_desc = {
	.name = "orca-pdbg",

	.pmu_pdm_num = sizeof(orca_pdm_info)/sizeof(struct pdm_info),
	.ap_ahb_reg_num = sizeof(orca_ap_ahb_reg)/sizeof(struct reg_check),
	.ap_apb_reg_num = sizeof(orca_ap_apb_reg)/sizeof(struct reg_check),
	.pmu_apb_reg_num = sizeof(orca_pmu_apb_reg)/sizeof(struct reg_check),
	.aon_apb_reg_num = sizeof(orca_aon_apb_reg)/sizeof(struct reg_check),
	.ap_intc_num = sizeof(orca_pintc_info)/sizeof(struct intc_info),
	.irq_mask = {0, 0, 0, 0, 0, 0x2000, 0, 0},

	.ppdm_info = orca_pdm_info,
	.ap_ahb_reg = orca_ap_ahb_reg,
	.ap_apb_reg = orca_ap_apb_reg,
	.pmu_apb_reg = orca_pmu_apb_reg,
	.aon_apb_reg = orca_aon_apb_reg,
	.pintc_info = orca_pintc_info,
	.log_2nd_irq_source = orca_output_2nd_irq_source
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

	pcfg =  devm_kzalloc(dev, (sizeof(struct power_debug_cfg) +
			intc_num * sizeof(struct regmap *)), GFP_KERNEL);
	if (!pcfg)
		return ERR_PTR(-ENOMEM);

	result = of_property_read_u32(pnode, "sprd,enable", &pcfg->pdbg_enable);
	if (result)
		pcfg->pdbg_enable = 1;

	result = of_property_read_u32(pnode, "sprd,scan-interval",
		&pcfg->scan_interval);
	if (result)
		pcfg->scan_interval = 30;

	pcfg->ap_ahb = syscon_regmap_lookup_by_phandle(pnode,
		"sprd,sys-ap-ahb");
	if (IS_ERR(pcfg->ap_ahb)) {
		dev_warn(dev, "Not to get ap-ahb regmap\n");
		pcfg->ap_ahb = NULL;
	}

	pcfg->ap_apb = syscon_regmap_lookup_by_phandle(pnode,
		"sprd,sys-ap-apb");
	if (IS_ERR(pcfg->ap_apb)) {
		dev_warn(dev, "Not to get ap-apb regmap\n");
		pcfg->ap_apb = NULL;
	}

	pcfg->pmu_apb = syscon_regmap_lookup_by_phandle(pnode,
		"sprd,sys-pmu-apb");
	if (IS_ERR(pcfg->pmu_apb)) {
		dev_err(dev, "Failed to get pmu-apb regmap\n");
		devm_kfree(dev, pcfg);
		return ERR_PTR(-EINVAL);
	}

	pcfg->aon_apb = syscon_regmap_lookup_by_phandle(pnode,
		"sprd,sys-aon-apb");
	if (IS_ERR(pcfg->aon_apb)) {
		dev_warn(dev, "Not to get aon-apb regmap\n");
		pcfg->aon_apb = NULL;
	}

	pcfg->aon_sec = syscon_regmap_lookup_by_phandle(pnode,
		"sprd,sys-aon-sec");
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
static int sprd_pdbg_orca_probe(struct platform_device *pdev)
{
	struct power_debug_cfg *pcfg;

	dev_dbg(&pdev->dev, "##### Power debug driver init start #####\n");

	pcfg = sprd_pdbg_parse_cfg(&pdev->dev, orca_pdbg_desc.ap_intc_num);
	if (IS_ERR(pcfg))
		return (int)pcfg;

	pdbg_orca.pdbg = sprd_power_debug_register(&pdev->dev,
				&orca_pdbg_desc,	pcfg);
	if (!pdbg_orca.pdbg) {
		devm_kfree(&pdev->dev, pcfg);
		dev_dbg(&pdev->dev, "##### Power debug driver init failure #####\n");
		return -EFAULT;
	}
	pdbg_orca.pcfg = pcfg;
	pdbg_orca.pdev = pdev;

	dev_dbg(&pdev->dev, "##### Power debug driver init successfully #####\n");

	return 0;
}

/**
 * sprd_powerdebug_remove - remove the power debug driver
 */
static int sprd_pdbg_orca_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "##### Power debug driver remove #####\n");
	sprd_power_debug_unregister(pdbg_orca.pdbg);
	pdbg_orca.pdbg = NULL;

	devm_kfree(&pdev->dev, pdbg_orca.pcfg);
	pdbg_orca.pcfg = NULL;
	pdbg_orca.pdev = NULL;

	return 0;
}

static const struct of_device_id sprd_pdbg_orca_of_match[] = {
	{
		.compatible = "sprd,power-debug-orca",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_pdbg_orca_of_match);

static struct platform_driver sprd_pdbg_orca_driver = {
	.probe = sprd_pdbg_orca_probe,
	.remove = sprd_pdbg_orca_remove,
	.driver = {
		.name = "sprd-powerdebug",
		.of_match_table = sprd_pdbg_orca_of_match,
	},
};

module_platform_driver(sprd_pdbg_orca_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jamesj Chen<Jamesj.Chen@unisoc.com>");
MODULE_DESCRIPTION("sprd orca power debug driver");
