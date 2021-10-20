// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc UMS512 VSP power domain driver
 *
 * Copyright (C) 2019 Unisoc, Inc.
 */

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/types.h>

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "sprd-vsp-pd: " fmt

enum {
	PMU_VSP_AUTO_SHUTDOWN = 0,
	PMU_VSP_FORCE_SHUTDOWN,
	PMU_PWR_STATUS,
	VSP_DOMAIN_EB,
	REG_MAX
};

static char * const vsp_pm_name[REG_MAX] = {
	[PMU_VSP_AUTO_SHUTDOWN] = "pmu-vsp-auto-shutdown-syscon",
	[PMU_VSP_FORCE_SHUTDOWN] = "pmu-vsp-force-shutdown-syscon",
	[PMU_PWR_STATUS] = "pmu-pwr-status-syscon",
	[VSP_DOMAIN_EB] = "vsp-domain-eb-syscon",
};

struct sprd_vsp_pd {
	struct device *dev;
	struct generic_pm_domain gpd;
	struct regmap *regmap[REG_MAX];
	unsigned int reg[REG_MAX];
	unsigned int mask[REG_MAX];
};

static int boot_mode_check(void)
{
	struct device_node *np;
	const char *cmd_line;
	int ret = 0;

	np = of_find_node_by_path("/chosen");
	if (!np)
		return 0;

	ret = of_property_read_string(np, "bootargs", &cmd_line);
	if (ret < 0)
		return 0;

	if (strstr(cmd_line, "androidboot.mode=cali"))
		ret = 1;

	return ret;
}

static int vsp_shutdown(struct sprd_vsp_pd *vsp_pd)
{
	int ret = 0;

	if (vsp_pd->regmap[VSP_DOMAIN_EB] != NULL) {
		pr_info("VSP_DOMAIN_EB shutdown\n");
		ret = regmap_update_bits(vsp_pd->regmap[VSP_DOMAIN_EB],
			vsp_pd->reg[VSP_DOMAIN_EB],
			vsp_pd->mask[VSP_DOMAIN_EB], 0);
		if (ret) {
			pr_err("cali regmap_update_bits failed %s, %d\n",
				__func__, __LINE__);
		}
		return ret;
	}

	ret = regmap_update_bits(vsp_pd->regmap[PMU_VSP_FORCE_SHUTDOWN],
		vsp_pd->reg[PMU_VSP_FORCE_SHUTDOWN],
		vsp_pd->mask[PMU_VSP_FORCE_SHUTDOWN],
		vsp_pd->mask[PMU_VSP_FORCE_SHUTDOWN]);
	if (ret) {
		pr_err("cali regmap_update_bits failed %s, %d\n",
			__func__, __LINE__);
		return ret;
	}
	return ret;
}

static int vsp_pw_on(struct generic_pm_domain *domain)
{
	int ret;
	u32 power_state;
	u32 read_count = 0;

	struct sprd_vsp_pd *vsp_pd = container_of(domain, struct sprd_vsp_pd, gpd);

	if (vsp_pd->regmap[PMU_VSP_AUTO_SHUTDOWN] == NULL) {
		pr_info("skip power on\n");
		ret = -EINVAL;
		goto pw_on_exit;
	}

	ret = regmap_update_bits(vsp_pd->regmap[PMU_VSP_AUTO_SHUTDOWN],
		vsp_pd->reg[PMU_VSP_AUTO_SHUTDOWN],
		vsp_pd->mask[PMU_VSP_AUTO_SHUTDOWN],
		~vsp_pd->mask[PMU_VSP_AUTO_SHUTDOWN]);
	if (ret) {
		pr_err("regmap_update_bits failed %s, %d\n",
				__func__, __LINE__);
		goto pw_on_exit;
	}

	ret = regmap_update_bits(vsp_pd->regmap[PMU_VSP_FORCE_SHUTDOWN],
		vsp_pd->reg[PMU_VSP_FORCE_SHUTDOWN],
		vsp_pd->mask[PMU_VSP_FORCE_SHUTDOWN],
		~vsp_pd->mask[PMU_VSP_FORCE_SHUTDOWN]);
	if (ret) {
		pr_err("regmap_update_bits failed %s, %d\n",
				__func__, __LINE__);
		goto pw_on_exit;
	}

	do {
		udelay(300);
		read_count++;
		ret = regmap_read(vsp_pd->regmap[PMU_PWR_STATUS],
				vsp_pd->reg[PMU_PWR_STATUS], &power_state);
		if (ret != 0) {
			pr_err("regmap_read failed %s, %d\n",
				__func__, __LINE__);
			goto pw_on_exit;
		}
		power_state &= vsp_pd->mask[PMU_PWR_STATUS];
	} while (power_state && read_count < 100);

	if (power_state) {
		pr_err("%s set failed 0x%x\n", __func__,
				power_state);
		return -ETIMEDOUT;
	}

	pr_debug("%s set OK\n", __func__);

pw_on_exit:
	return ret;
}

static int vsp_pw_off(struct generic_pm_domain *domain)
{
	int ret;
	struct sprd_vsp_pd *vsp_pd = container_of(domain, struct sprd_vsp_pd, gpd);

	if (vsp_pd->regmap[PMU_VSP_FORCE_SHUTDOWN] == NULL) {
		pr_info("skip power off\n");
		ret = -EINVAL;
		goto pw_off_exit;
	}

	ret = regmap_update_bits(
		vsp_pd->regmap[PMU_VSP_FORCE_SHUTDOWN],
		vsp_pd->reg[PMU_VSP_FORCE_SHUTDOWN],
		vsp_pd->mask[PMU_VSP_FORCE_SHUTDOWN],
		vsp_pd->mask[PMU_VSP_FORCE_SHUTDOWN]);
	if (ret)
		pr_err("regmap_update_bits failed %s, %d\n",
			__func__, __LINE__);

pw_off_exit:
	return ret;
}

static int vsp_pd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	char *pname;
	struct regmap *tregmap;
	uint32_t syscon_args[2];
	struct sprd_vsp_pd *pd;
	int i, ret;

	dev_info(dev, "%s, %d\n", __func__, __LINE__);
	pd = devm_kzalloc(dev, sizeof(*pd), GFP_KERNEL);
	if (!pd) {
		of_node_put(np);
		return -ENOMEM;
	}

	pd->dev = &pdev->dev;
	pd->gpd.name = kstrdup(np->name, GFP_KERNEL);
	pd->gpd.power_off = vsp_pw_off;
	pd->gpd.power_on = vsp_pw_on;

	for (i = 0; i < ARRAY_SIZE(vsp_pm_name); i++) {
		pname = vsp_pm_name[i];
		tregmap = syscon_regmap_lookup_by_phandle_args(np, pname, 2, syscon_args);
		if (IS_ERR(tregmap)) {
			dev_err(dev, "Read Vsp Dts %s regmap fail\n",
			pname);
			pd->regmap[i] = NULL;
			pd->reg[i] = 0x0;
			pd->mask[i] = 0x0;
			continue;
		}
		pd->regmap[i] = tregmap;
		pd->reg[i] = syscon_args[0];
		pd->mask[i] = syscon_args[1];
		dev_info(dev, "VSP syscon[%s]%p, offset 0x%x, mask 0x%x\n",
			pname, pd->regmap[i], pd->reg[i], pd->mask[i]);
	}

	if (boot_mode_check()) {
		ret = vsp_shutdown(pd);
		if (!ret) {
			pr_info("%s: calibration mode and not probe\n", __func__);
		} else {
			pr_err("%s: calibration mode vsp shutdown failed\n", __func__);
		}
		return 0;
	}

	pm_genpd_init(&pd->gpd, NULL, true);
	of_genpd_add_provider_simple(np, &pd->gpd);

	return 0;
}

static const struct of_device_id vsp_pd_of_match[] = {
	{ .compatible = "sprd,vsp-pd" },
	{ },
};

static struct platform_driver vsp_pd_driver = {
	.probe = vsp_pd_probe,
	.driver = {
		.name   = "sprd-vsp-pd",
		.of_match_table = vsp_pd_of_match,
	},
};
module_platform_driver(vsp_pd_driver);

MODULE_AUTHOR("Chunlei Guo <chunlei.guo@unisoc.com>");
MODULE_DESCRIPTION("UNISOC VSP power domain driver");
MODULE_LICENSE("GPL v2");
