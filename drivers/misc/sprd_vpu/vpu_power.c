// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc QOGIRN6PRO VPU power driver
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

#define pr_fmt(fmt) "sprd-vpu-pd: " fmt
#define __SPRD_VPU_TIMEOUT            (30 * 1000)

enum {
	PMU_VPU_AUTO_SHUTDOWN = 0,
	PMU_VPU_FORCE_SHUTDOWN,
	PMU_PWR_STATUS,
	REG_MAX
};

static char * const vpu_pm_name[REG_MAX] = {
	[PMU_VPU_AUTO_SHUTDOWN] = "pmu_vpu_auto_shutdown",
	[PMU_VPU_FORCE_SHUTDOWN] = "pmu_vpu_force_shutdown",
	[PMU_PWR_STATUS] = "pmu_pwr_status",
};

struct sprd_vpu_pd {
	struct device *dev;
	struct generic_pm_domain gpd;
	struct regmap *regmap[REG_MAX];
	unsigned int reg[REG_MAX];
	unsigned int mask[REG_MAX];
};

static const struct of_device_id vpu_pd_of_match[] = {
	{ .compatible = "sprd,vpu-pd"},
	{ },
};

static int vpu_pw_on(struct generic_pm_domain *domain)
{
	int ret = 0;
	u32 power_state;
	u32 read_count = 0;

	struct sprd_vpu_pd *vpu_pd = container_of(domain, struct sprd_vpu_pd, gpd);

	if (vpu_pd->regmap[PMU_VPU_AUTO_SHUTDOWN] == NULL) {
		pr_info("skip power on\n");
		ret = -EINVAL;
		goto pw_on_exit;
	}

	ret = regmap_update_bits(vpu_pd->regmap[PMU_VPU_AUTO_SHUTDOWN],
		vpu_pd->reg[PMU_VPU_AUTO_SHUTDOWN],
		vpu_pd->mask[PMU_VPU_AUTO_SHUTDOWN],
		~vpu_pd->mask[PMU_VPU_AUTO_SHUTDOWN]);
	if (ret) {
		pr_err("regmap_update_bits failed %s, %d\n",
			__func__, __LINE__);
		goto pw_on_exit;
	}

	ret = regmap_update_bits(vpu_pd->regmap[PMU_VPU_FORCE_SHUTDOWN],
		vpu_pd->reg[PMU_VPU_FORCE_SHUTDOWN],
		vpu_pd->mask[PMU_VPU_FORCE_SHUTDOWN],
		~vpu_pd->mask[PMU_VPU_FORCE_SHUTDOWN]);
	if (ret) {
		pr_err("regmap_update_bits failed %s, %d\n",
			__func__, __LINE__);
		goto pw_on_exit;
	}

	do {
		usleep_range(200, 400);
		read_count++;
		regmap_read(vpu_pd->regmap[PMU_PWR_STATUS],
			vpu_pd->reg[PMU_PWR_STATUS], &power_state);
		power_state &= vpu_pd->mask[PMU_PWR_STATUS];
	} while (power_state && read_count < 100);

	if (power_state) {
		pr_err("%s set failed 0x%x\n", __func__,
			power_state);
		return -ETIMEDOUT;
	}

	dev_info(vpu_pd->dev, "%s OK\n", __func__);

pw_on_exit:

	return ret;
}

static int vpu_pw_off(struct generic_pm_domain *domain)
{
	int ret = 0;
	struct sprd_vpu_pd *vpu_pd = container_of(domain, struct sprd_vpu_pd, gpd);

	if (vpu_pd->regmap[PMU_VPU_FORCE_SHUTDOWN] == NULL) {
		pr_info("skip power off\n");
		ret = -EINVAL;
		goto pw_off_exit;
	}

	ret = regmap_update_bits(
		vpu_pd->regmap[PMU_VPU_FORCE_SHUTDOWN],
		vpu_pd->reg[PMU_VPU_FORCE_SHUTDOWN],
		vpu_pd->mask[PMU_VPU_FORCE_SHUTDOWN],
		vpu_pd->mask[PMU_VPU_FORCE_SHUTDOWN]);
	if (ret) {
		pr_err("regmap_update_bits failed %s, %d\n",
			__func__, __LINE__);
		goto pw_off_exit;
	}
	dev_info(vpu_pd->dev, "%s OK\n", __func__);
pw_off_exit:

	return ret;
}

static int vpu_pd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	char *pname;
	struct regmap *tregmap;
	uint32_t syscon_args[2];
	struct sprd_vpu_pd *pd;
	int i, ret;
	struct of_phandle_args child, parent;

	dev_info(dev, "%s, %d\n", __func__, __LINE__);
	pd = devm_kzalloc(dev, sizeof(*pd), GFP_KERNEL);
	if (!pd) {
		of_node_put(np);
		return -ENOMEM;
	}

	pd->dev = &pdev->dev;
	pd->gpd.name = kstrdup(np->name, GFP_KERNEL);
	pd->gpd.power_off = vpu_pw_off;
	pd->gpd.power_on = vpu_pw_on;

	for (i = 0; i < ARRAY_SIZE(vpu_pm_name); i++) {
		pname = vpu_pm_name[i];
		tregmap = syscon_regmap_lookup_by_name(np, pname);
		if (IS_ERR(tregmap)) {
			dev_err(dev, "Read Vsp Dts %s regmap fail\n",
				pname);
			pd->regmap[i] = NULL;
			pd->reg[i] = 0x0;
			pd->mask[i] = 0x0;
			continue;
		}

		ret = syscon_get_args_by_name(np, pname, 2, syscon_args);
		if (ret != 2) {
			dev_err(dev, "Read Vsp Dts %s args fail, ret = %d\n",
				pname, ret);
			continue;
		}
		pd->regmap[i] = tregmap;
		pd->reg[i] = syscon_args[0];
		pd->mask[i] = syscon_args[1];
		dev_info(dev, "VPU syscon[%s]%p, offset 0x%x, mask 0x%x\n",
				pname, pd->regmap[i], pd->reg[i], pd->mask[i]);
	}

	pm_genpd_init(&pd->gpd, NULL, true);
	of_genpd_add_provider_simple(np, &pd->gpd);

	child.np = np;
	child.args_count = 0;

	if (!of_parse_phandle_with_args(np, "power-domains",
				       "#power-domain-cells", 0,
				       &parent)) {
		if (of_genpd_add_subdomain(&parent, &child))
			pr_warn("%pOF failed to add subdomain: %pOF\n",
				parent.np, child.np);
		else
			pr_info("%pOF has as child subdomain: %pOF.\n",
				parent.np, child.np);
	}
	return 0;
}



static struct platform_driver vpu_pd_driver = {
	.probe = vpu_pd_probe,
	.driver = {
		.name   = "sprd-vpu-pd",
		.of_match_table = vpu_pd_of_match,
	},
};

module_platform_driver(vpu_pd_driver);
MODULE_AUTHOR("Chunlei Guo <chunlei.guo@unisoc.com>");
MODULE_DESCRIPTION("Spreadtrum vpu power domain driver");
MODULE_LICENSE("GPL v2");
