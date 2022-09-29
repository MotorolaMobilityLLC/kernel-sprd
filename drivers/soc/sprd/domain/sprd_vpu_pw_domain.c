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
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/types.h>

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "sprd-vpu-pd: " fmt
#define __SPRD_VPU_TIMEOUT            (30 * 1000)
#define PM_RUNTIME_DELAY_MS 3000

enum {
	PMU_VPU_AUTO_SHUTDOWN = 0,
	PMU_VPU_FORCE_SHUTDOWN,
	PMU_PWR_STATUS,
	PMU_APB_PIXELPLL,
	VPU_DOMAIN_EB,
	REG_MAX
};

static char * const vpu_pm_name[REG_MAX] = {
	[PMU_VPU_AUTO_SHUTDOWN] = "pmu-vpu-auto-shutdown-syscon",
	[PMU_VPU_FORCE_SHUTDOWN] = "pmu-vpu-force-shutdown-syscon",
	[PMU_PWR_STATUS] = "pmu-pwr-status-syscon",
	[PMU_APB_PIXELPLL] = "pmu-apb-pixelpll-syscon",
	[VPU_DOMAIN_EB] = "vpu-domain-eb-syscon",
};

struct sprd_vpu_pd {
	struct device *dev;
	struct generic_pm_domain gpd;
	struct regmap *regmap[REG_MAX];
	struct delayed_work delay_work;
	unsigned int reg[REG_MAX];
	unsigned int mask[REG_MAX];
};

static const struct of_device_id vpu_pd_of_match[] = {
	{ .compatible = "sprd,vpu-pd"},
	{ },
};

static bool cali_mode_check(const char *str)
{
	struct device_node *calibration_mode;
	const char *cmd_line;
	int rc;

	calibration_mode = of_find_node_by_path("/chosen");
	rc = of_property_read_string(calibration_mode, "bootargs", &cmd_line);
	if (rc)
		return false;

	if (!strstr(cmd_line, str))
		return false;

	return true;
}
static int vpu_vsp_cali(struct sprd_vpu_pd *vpu_pd)
{
	int ret = 0;

	if (vpu_pd->regmap[VPU_DOMAIN_EB] == NULL) {
		pr_info("NO VPU_DOMAIN_EB,bypass\n");
		return ret;
	}

	ret = regmap_update_bits(vpu_pd->regmap[VPU_DOMAIN_EB],
		vpu_pd->reg[VPU_DOMAIN_EB],
		vpu_pd->mask[VPU_DOMAIN_EB],
		~vpu_pd->mask[VPU_DOMAIN_EB]);
	if (ret) {
		pr_err("cali regmap_update_bits failed %s, %d\n",
			__func__, __LINE__);
		return ret;
	}
	return ret;
}

static int vpu_pw_on(struct generic_pm_domain *domain)
{
	int ret = 0;
	u32 power_state;
	u32 read_count = 0;

	struct sprd_vpu_pd *vpu_pd = container_of(domain, struct sprd_vpu_pd, gpd);

	if (vpu_pd->regmap[PMU_APB_PIXELPLL] != NULL) {
		pr_info("PMU_APB_PIXELPLL pw on \n");
		ret = regmap_update_bits(vpu_pd->regmap[PMU_APB_PIXELPLL],
			vpu_pd->reg[PMU_APB_PIXELPLL],
			vpu_pd->mask[PMU_APB_PIXELPLL],
			vpu_pd->mask[PMU_APB_PIXELPLL]);
		if (ret) {
			pr_err("regmap_update_bits failed %s, %d\n",
				__func__, __LINE__);
			goto pw_on_exit;
		}
		udelay(300);
	}

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
		udelay(300);
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

	pr_info("%s: %s OK\n", domain->name, __func__);

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


	if (vpu_pd->regmap[PMU_APB_PIXELPLL] != NULL) {
		pr_info("PMU_APB_PIXELPLL pw off\n");
		ret = regmap_update_bits(vpu_pd->regmap[PMU_APB_PIXELPLL],
			vpu_pd->reg[PMU_APB_PIXELPLL],
			vpu_pd->mask[PMU_APB_PIXELPLL],
			~vpu_pd->mask[PMU_APB_PIXELPLL]);
		if (ret) {
			pr_err("regmap_update_bits failed %s, %d\n",
				__func__, __LINE__);
			goto pw_off_exit;
		}
	}

	pr_info("%s: %s OK\n", domain->name, __func__);
pw_off_exit:

	return ret;
}

static void vpu_delay_work(struct work_struct *work)
{
	struct delayed_work *dw = container_of(work, struct delayed_work, work);
	struct sprd_vpu_pd *pd = container_of(dw, struct sprd_vpu_pd, delay_work);

	pm_runtime_set_autosuspend_delay(pd->dev, PM_RUNTIME_DELAY_MS);

	dev_info(pd->dev, "vpu pd delay work done!\n");
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
	bool cali_mode;
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
		tregmap = syscon_regmap_lookup_by_phandle_args(np, pname, 2, syscon_args);
		if (IS_ERR(tregmap)) {
			dev_err(dev, "Read Vpu Dts %s regmap fail\n",
			pname);
			pd->regmap[i] = NULL;
			pd->reg[i] = 0x0;
			pd->mask[i] = 0x0;
			continue;
		}
		pd->regmap[i] = tregmap;
		pd->reg[i] = syscon_args[0];
		pd->mask[i] = syscon_args[1];
		dev_info(dev, "VPU syscon[%s]%p, offset 0x%x, mask 0x%x\n",
			pname, pd->regmap[i], pd->reg[i], pd->mask[i]);
	}

	cali_mode = cali_mode_check("androidboot.mode=cali");
	if (cali_mode) {
		pr_info("cali mode enter success!");
		ret = vpu_vsp_cali(pd);
		if (!ret)
			pr_info("%s: calibration mode and not probe\n", __func__);
		else
			pr_err("%s: calibration mode vpu shutdown failed\n", __func__);
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

	if (cali_mode) {
		pm_runtime_set_active(dev);
		pm_runtime_enable(dev);
		return 0;
	}

	pm_runtime_set_active(dev);
	pm_runtime_set_autosuspend_delay(dev, -1); //prevent auto suspend
	pm_runtime_use_autosuspend(dev);
	pm_runtime_enable(dev);

	INIT_DELAYED_WORK(&pd->delay_work, vpu_delay_work);
	schedule_delayed_work(&pd->delay_work, msecs_to_jiffies(10000)); //10s

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

