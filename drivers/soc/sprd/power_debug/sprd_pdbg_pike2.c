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
#include "sprd_pdbg_sc2720.h"

#define AP_INTC_PMIC_INDEX 38

struct power_debug_pike2 {
	struct power_debug *pdbg;
	struct platform_device *pdev;
	struct power_debug_cfg *pcfg;
};

static struct power_debug_pike2 pdbg_pike2;

static struct pdm_info pike2_pdm_info[] = {
	{
		.addr_offset = 0x00bc,
		.pwd_bit_width = 4,
		.bit_index = {0, 4, 8, 12, 16, 20, 24, 28},
		.pdm_name = {"PD_CA7_TOP_STATE", "PD_CA7_C0_STATE",
			"PD_CA7_C1_STATE", "PD_CA7_C2_STATE",
			"PD_CA7_C3_STATE", "PD_AP_SYS_STATE",
			"PD_GPU_TOP_STATE", "PD_MM_TOP_STATE"}
	},
	{
		.addr_offset = 0x00c0,
		.pwd_bit_width = 4,
		.bit_index = {0, 4, 8, 12, 16, 20, 24, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_CP_SYS_STATE", "PD_WTLCP_HU3GE_A_STATE",
			"PD_WTLCP_TGDSP_STATE", "PD_PUB_SYS_STATE",
			"PD_WCN_TOP_STATE", "PD_WCN_WIFI_STATE",
			"PD_WCN_GNSS_STATE"}
	},
	{
		.addr_offset = 0x00d4,
		.pwd_bit_width = 5,
		.bit_index = {0, 8, 12, 20, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"AP_SLP_STATUS", "CP_SLP_STATUS",
			"WCN_SLP_STATUS", "CM4_SLP_STATUS"}
	}
};

static struct reg_check pike2_ap_ahb_reg[] = {
	{
		.addr_offset = 0x0000,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_AP_AHB_AHB_EB"
	},
	{
		.addr_offset = 0x3008,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_AP_AHB_CA7_STANDBY_STATUS"
	}
};

static struct reg_check pike2_ap_apb_reg[] = {
	{
		.addr_offset = 0x0000,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_AP_APB_APB_EB"
	}
};

static struct reg_check pike2_pmu_apb_reg[] = {
	{
		.addr_offset = 0x00cc,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_PMU_APB_SLEEP_CTRL"
	},
	{
		.addr_offset = 0x00d4,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_PMU_APB_SLEEP_STATUS"
	},
	{
		.addr_offset = 0x00d0,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_PMU_APB_DDR_SLEEP_CTRL"
	},
	{
		.addr_offset = 0x00b4,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_PMU_APB_CP_SLP_STATUS_DBG0"
	},
	{
		.addr_offset = 0x0018,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_PMU_APB_PD_AP_SYS_CFG"
	},
	{
		.addr_offset = 0x005c,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_PMU_APB_PD_PUB_SYS_CFG"
	}
};

static struct reg_check pike2_aon_apb_reg[] = {
	{
		.addr_offset = 0x0000,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_AON_APB_APB_EB0"
	},
	{
		.addr_offset = 0x0004,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_AON_APB_APB_EB1"
	},
	{
		.addr_offset = 0x00b0,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_AON_APB_APB_EB2"
	},
	{
		.addr_offset = 0x0024,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00000000,
		.preg_name = "REG_AON_APB_PWR_CTRL"
	}
};

static struct intc_info pike2_pintc_info[] = {
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "uart0", "uart1",
		"busmon_wtlcp", "busmon_wcn", "busmon_pubcp", "spi0",
		"spi1", "busmon_ap", "sim", "i2c0",
		"i2c1", "i2c2", "slv_fw_aon", "slv_fw_ap0",
		"iis", "slv_fw_ap2", "null", "null",
		"aud", "vbc_afifo_err", "vbc_da", "vbc_ad01",
		"vbc_ad23", "adi", "thm", "all_memfirewall",
		"aon_tmr", "ap_tmr0", "aon_for_ap", "ap_syst"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "i2c", "gpio",
		"kpd", "eic", "ana", "gpu",
		"csi2_r1", "csi2_r2", "jpg", "vsp",
		"isp_chn0", "dcam", "dispc", "dsi_pll",
		"dsi[0]", "dsi[1]", "dma", "gsp",
		"sec_dma", "ce_sec", "ce_pub", "otg",
		"aon_sec_rtc", "sdio0", "null", "nandc",
		"emmc", "dmc_mpu_vio", "sec_eic", "sec_gpio"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "null", "djtag",
		"mbox_src_ap", "mbox_tar_ap", "aon_dma_ap", "csi2_r2_s",
		"csi2_r1_s", "int_req_gpu", "aon_sec_tmr2", "aon_sec_tmr1",
		"aon_sec_tmr0", "csi_idi_sw", "aon_dma_chn0", "aon_dma_chn1",
		"aon_dma_chn2", "aon_dma_chn3", "sec_aon_dma", "pubcp_wdg",
		"wtlcp_wdg", "aon_sec_wdg", "pub_busmon", "ca7_axierr_n",
		"nctiirq_o0", "nctiirq_o1", "nctiirq_o2", "nctiirq_o3",
		"npmuirq_o0", "npmuirq_o1", "npmuirq_o2", "npmuirq_o3"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "ca7_commtrx0", "ca7_commtrx1",
		"ca7_commtrx2", "ca7_commtrx3", "ncntvirq_o0", "ncntvirq_o1",
		"ncntvirq_o2", "ncntvirq_o3", "ncnthpirq_o0", "ncnthpirq_o1",
		"ncnthpirq_o2", "ncnthpirq_o3", "ncntpnirq_o0", "ncntpnirq_o1",
		"ncntpnirq_o2", "ncntpnirq_o3", "ncntpsirq_o0", "ncntpsirq_o1",
		"ncntpsirq_o2", "ncntpsirq_o3", "ap_tmr1", "ap_tmr2",
		"ap_tmr3", "ap_tmr4", "avs", "ap_wdg",
		"ca7_wdg", "isp_chn1", "cpp", "aon_clk_32k_det"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "ap_tmr",	"vbc",
		"aon_peri", "mm_top", "gpu", "cp_wdg",
		"slf_wdg", "lp_mon", "pub_busmon", "mbox_tar_cm4",
		"mbox_tar_ap", "dma", "sec", "wtlcp_busmon",
		"pubcp_busmon", "busmon_aon", "chn_start_chn0", "start_chn1",
		"start_chn2", "start_chn3", "sec_dma", "dfi_bus_monitor",
		"wcn_btwf_wdg", "wcn_gnss_wdg", "rf_wdg", "null",
		"sec_eic", "sec_eic_non_lat", "eic", "eic_non_lat"}
	}
};

static void pike2_output_2nd_irq_source(void *pentry, u32 hw_irq_nr)
{
	struct power_debug *pdbg = (struct power_debug *)pentry;

	if (pdbg_pike2.pdbg != pdbg)
		return;

	if (hw_irq_nr == AP_INTC_PMIC_INDEX)
		sc2720_output_irq_source();
}

static struct power_debug_desc pike2_pdbg_desc = {
	.name = "pike2-pdbg",

	.pmu_pdm_num = sizeof(pike2_pdm_info)/sizeof(struct pdm_info),
	.ap_ahb_reg_num = sizeof(pike2_ap_ahb_reg)/sizeof(struct reg_check),
	.ap_apb_reg_num = sizeof(pike2_ap_apb_reg)/sizeof(struct reg_check),
	.pmu_apb_reg_num = sizeof(pike2_pmu_apb_reg)/sizeof(struct reg_check),
	.aon_apb_reg_num = sizeof(pike2_aon_apb_reg)/sizeof(struct reg_check),
	.ap_intc_num = sizeof(pike2_pintc_info)/sizeof(struct intc_info),
	.irq_mask = {0, 0x40, 0, 0, 0, 0, 0, 0},

	.ppdm_info = pike2_pdm_info,
	.ap_ahb_reg = pike2_ap_ahb_reg,
	.ap_apb_reg = pike2_ap_apb_reg,
	.pmu_apb_reg = pike2_pmu_apb_reg,
	.aon_apb_reg = pike2_aon_apb_reg,
	.pintc_info = pike2_pintc_info,
	.log_2nd_irq_source = pike2_output_2nd_irq_source
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

	result = of_property_read_u32(pnode,
		"sprd,enable", &pcfg->pdbg_enable);
	if (result)
		pcfg->pdbg_enable = 1;

	result = of_property_read_u32(pnode,
		"sprd,scan-interval", &pcfg->scan_interval);
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
static int sprd_pdbg_pike2_probe(struct platform_device *pdev)
{
	struct power_debug_cfg *pcfg;

	dev_dbg(&pdev->dev, "##### Power debug driver init start #####\n");

	pcfg = sprd_pdbg_parse_cfg(&pdev->dev, pike2_pdbg_desc.ap_intc_num);
	if (IS_ERR(pcfg))
		return (int)pcfg;

	pdbg_pike2.pdbg = sprd_power_debug_register(&pdev->dev,
				&pike2_pdbg_desc,	pcfg);
	if (!pdbg_pike2.pdbg) {
		devm_kfree(&pdev->dev, pcfg);
		dev_dbg(&pdev->dev, "##### Power debug driver init failure #####\n");
		return -EFAULT;
	}
	pdbg_pike2.pcfg = pcfg;
	pdbg_pike2.pdev = pdev;

	dev_dbg(&pdev->dev, "##### Power debug driver init successfully #####\n");

	return 0;
}

/**
 * sprd_powerdebug_remove - remove the power debug driver
 */
static int sprd_pdbg_pike2_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "##### Power debug driver remove #####\n");
	sprd_power_debug_unregister(pdbg_pike2.pdbg);
	pdbg_pike2.pdbg = NULL;

	devm_kfree(&pdev->dev, pdbg_pike2.pcfg);
	pdbg_pike2.pcfg = NULL;
	pdbg_pike2.pdev = NULL;

	return 0;
}

static const struct of_device_id sprd_pdbg_pike2_of_match[] = {
	{
		.compatible = "sprd,power-debug-pike2",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_pdbg_pike2_of_match);

static struct platform_driver sprd_pdbg_pike2_driver = {
	.probe = sprd_pdbg_pike2_probe,
	.remove = sprd_pdbg_pike2_remove,
	.driver = {
		.name = "sprd-powerdebug",
		.of_match_table = sprd_pdbg_pike2_of_match,
	},
};

module_platform_driver(sprd_pdbg_pike2_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jamesj Chen<Jamesj.Chen@unisoc.com>");
MODULE_DESCRIPTION("sprd pike2 power debug driver");
