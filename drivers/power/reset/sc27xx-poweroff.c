// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 * Copyright (C) 2018 Linaro Ltd.
 */

#include <linux/cpu.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/regmap.h>
#include <linux/syscore_ops.h>

#define SC2730_PWR_PD_HW	0x1820
#define SC2730_SLP_CTRL		0x1a48
#define SC2730_LDO_XTL_EN	BIT(2)
#define SC2731_PWR_PD_HW	0xc2c
#define SC2731_SLP_CTRL		0xdf0
#define SC2731_LDO_XTL_EN	BIT(3)
#define UMP9620_PWR_PD_HW	0x2020
#define UMP9620_SLP_CTRL	0x2248
#define UMP9620_LDO_XTL_EN	BIT(2)
#define SC27XX_PWR_OFF_EN	BIT(0)

struct sc27xx_poweroff_data {
	u32 poweroff_reg;
	u32 slp_ctrl_reg;
	u32 ldo_xtl_en;
};

static const struct sc27xx_poweroff_data sc2730_data = {
	.poweroff_reg = SC2730_PWR_PD_HW,
	.slp_ctrl_reg = SC2730_SLP_CTRL,
	.ldo_xtl_en = SC2730_LDO_XTL_EN,
};

static const struct sc27xx_poweroff_data ump9620_data = {
	.poweroff_reg = UMP9620_PWR_PD_HW,
	.slp_ctrl_reg = UMP9620_SLP_CTRL,
	.ldo_xtl_en = UMP9620_LDO_XTL_EN,
};

static struct regmap *regmap;
const struct sc27xx_poweroff_data *pdata;
/*
 * On Spreadtrum platform, we need power off system through external SC27xx
 * series PMICs, and it is one similar SPI bus mapped by regmap to access PMIC,
 * which is not fast io access.
 *
 * So before stopping other cores, we need release other cores' resource by
 * taking cpus down to avoid racing regmap or spi mutex lock when poweroff
 * system through PMIC.
 */
#define RETRY_CNT_MAX (5)
static void sc27xx_poweroff_shutdown(void)
{
#ifdef CONFIG_HOTPLUG_CPU
	int cpu, retry_cnt, ret, primary = 0;

	pr_info("hotpluging non-boot CPUs ......\n");
	if (!cpu_online(primary)) {
		primary = smp_processor_id();
		pr_info("primary cpu change to cpu%d\n", primary);
	}

	cpu_hotplug_enable();
	for_each_online_cpu(cpu) {
		if (cpu == primary)
			continue;

		retry_cnt = 0;
		while (retry_cnt < RETRY_CNT_MAX) {
			ret = remove_cpu(cpu);

			if (!ret)
				break;

			msleep(20);
			pr_err("%s: hotplug cpu%d fail, cnt %d\n", __func__, cpu, retry_cnt);
			retry_cnt++;
		}
	}
	cpu_hotplug_disable();
#endif
}

static struct syscore_ops poweroff_syscore_ops = {
	.shutdown = sc27xx_poweroff_shutdown,
};

static void sc27xx_poweroff_do_poweroff(void)
{
	/* Disable the external subsys connection's power firstly */
	regmap_update_bits(regmap, pdata->slp_ctrl_reg, pdata->ldo_xtl_en, 0);

	regmap_write(regmap, pdata->poweroff_reg, SC27XX_PWR_OFF_EN);
}

static int sc27xx_poweroff_probe(struct platform_device *pdev)
{
	pdata = of_device_get_match_data(&pdev->dev);
	if (!pdata) {
		dev_err(&pdev->dev, "No matching driver data found\n");
		return -EINVAL;
	}

	if (regmap)
		return -EINVAL;

	regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!regmap)
		return -ENODEV;

	pm_power_off = sc27xx_poweroff_do_poweroff;
	register_syscore_ops(&poweroff_syscore_ops);
	return 0;
}

static const struct of_device_id sc27xx_poweroff_of_match[] = {
	{ .compatible = "sprd,sc2730-poweroff", .data = &sc2730_data },
	{ .compatible = "sprd,ump9620-poweroff", .data = &ump9620_data},
	{ }
};

static struct platform_driver sc27xx_poweroff_driver = {
	.probe = sc27xx_poweroff_probe,
	.driver = {
		.name = "sc27xx-poweroff",
		.of_match_table = sc27xx_poweroff_of_match,
	},
};
module_platform_driver(sc27xx_poweroff_driver);

MODULE_DESCRIPTION("Power off driver for SC27XX PMIC Device");
MODULE_AUTHOR("Baolin Wang <baolin.wang@unisoc.com>");
MODULE_LICENSE("GPL v2");
