/*
 * Spreadtrum Generic PM Domain support.
 *
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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

#include <linux/err.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_platform.h>
#include <linux/pm_domain.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#define SPRD_PD_WAKEUP 0x0
#define SPRD_PD_SHUTDOWN 0x7

#define SPRD_PD_POLL_DELAY		100 /* us */
#define SPRD_PD_POLL_TIMEOUT		(1000 * 1000) /* us */

#undef dev_dbg
#define dev_dbg dev_err

enum {
	FORCE_PD, /* Only effective when AUTO_PD de-asserted. */
	AUTO_PD,
	STATE,
	SYS_EB,
	REG_MAX,
};

static char * const sprd_reg_names[REG_MAX] = {
	[FORCE_PD] = "force_pd",
	[AUTO_PD] = "auto_pd",
	[STATE] = "state",
	[SYS_EB] = "sys_eb",
};

struct sprd_pm_domain {
	struct device *dev;
	struct generic_pm_domain gpd;
	struct regmap *regmap[REG_MAX];
	unsigned int reg[REG_MAX];
	unsigned int mask[REG_MAX];
	unsigned int force_pd;
};

/* Get bits value of a given value */
static inline unsigned int sprd_domain_bitval(unsigned int val,
					      unsigned int mask)
{
	return (val & mask) >> (ffs(mask) - 1);
}

static int sprd_domain_power_on(struct generic_pm_domain *domain)
{
	int sts, mask, ret;
	struct sprd_pm_domain *spd = container_of(domain, struct sprd_pm_domain,
						  gpd);

	ret = regmap_update_bits(spd->regmap[FORCE_PD], spd->reg[FORCE_PD],
				 spd->mask[FORCE_PD], ~spd->mask[FORCE_PD]);
	if (ret) {
		dev_err(spd->dev, "%s: failed to clear force shutdown\n",
			spd->gpd.name);
		return ret;
	}
	ret = regmap_update_bits(spd->regmap[AUTO_PD], spd->reg[AUTO_PD],
				 spd->mask[AUTO_PD], ~spd->mask[AUTO_PD]);
	if (ret) {
		dev_err(spd->dev, "%s: failed to set auto shutdown\n",
			spd->gpd.name);
		return ret;
	}

	mask = spd->mask[STATE];
	ret = regmap_read_poll_timeout(spd->regmap[STATE], spd->reg[STATE], sts,
				       (sprd_domain_bitval(sts, mask) == SPRD_PD_WAKEUP),
				       SPRD_PD_POLL_DELAY,
				       SPRD_PD_POLL_TIMEOUT);
	if (ret) {
		dev_err(spd->dev,
			"%s: power on polling timeout. state = 0x%x\n",
			spd->gpd.name, sprd_domain_bitval(sts, mask));
		return ret;
	}

	ret = regmap_update_bits(spd->regmap[SYS_EB], spd->reg[SYS_EB],
				 spd->mask[SYS_EB], spd->mask[SYS_EB]);
	if (ret) {
		dev_err(spd->dev, "%s: failed to set system enable\n",
			spd->gpd.name);
		return ret;
	}

	dev_dbg(spd->dev, "%s: power on\n", spd->gpd.name);

	return 0;
}

static int sprd_domain_power_off(struct generic_pm_domain *domain)
{
	int sts, mask, ret;
	struct sprd_pm_domain *spd = container_of(domain, struct sprd_pm_domain,
						  gpd);

	ret = regmap_update_bits(spd->regmap[SYS_EB], spd->reg[SYS_EB],
				 spd->mask[SYS_EB], ~spd->mask[SYS_EB]);

	if (spd->force_pd) {
		ret |= regmap_update_bits(spd->regmap[FORCE_PD],
					  spd->reg[FORCE_PD],
					  spd->mask[FORCE_PD],
					  spd->mask[FORCE_PD]);

		ret |= regmap_update_bits(spd->regmap[AUTO_PD],
					  spd->reg[AUTO_PD], spd->mask[AUTO_PD],
					  ~spd->mask[AUTO_PD]);
	} else {
		ret |= regmap_update_bits(spd->regmap[FORCE_PD],
					  spd->reg[FORCE_PD],
					  spd->mask[FORCE_PD],
					  ~spd->mask[FORCE_PD]);

		ret |= regmap_update_bits(spd->regmap[AUTO_PD],
					  spd->reg[AUTO_PD], spd->mask[AUTO_PD],
					  spd->mask[AUTO_PD]);
	}
	if (ret) {
		dev_err(spd->dev, "%s: failed to set power off\n",
			spd->gpd.name);
		return ret;
	}

	mask = spd->mask[STATE];
	ret = regmap_read_poll_timeout(spd->regmap[STATE], spd->reg[STATE], sts,
				       (sprd_domain_bitval(sts, mask) == SPRD_PD_SHUTDOWN),
				       SPRD_PD_POLL_DELAY,
				       SPRD_PD_POLL_TIMEOUT);
	if (ret) {
		dev_err(spd->dev,
			"%s: power off polling timeout. state = 0x%x\n",
			spd->gpd.name, sprd_domain_bitval(sts, mask));
		return ret;
	}

	dev_dbg(spd->dev, "%s: power off\n", spd->gpd.name);

	return 0;
}

static int sprd_domain_probe(struct platform_device *pdev)
{
	struct device_node *root = pdev->dev.of_node;
	struct device_node *np;

	for_each_child_of_node(root, np) {
		struct sprd_pm_domain *spd;
		unsigned int syscon_args[2];
		int ret, i, val;

		spd = devm_kzalloc(&pdev->dev, sizeof(*spd), GFP_KERNEL);
		if (!spd) {
			of_node_put(np);
			return -ENOMEM;
		}

		spd->dev = &pdev->dev;
		spd->gpd.name = kstrdup_const(np->name, GFP_KERNEL);
		spd->gpd.power_off = sprd_domain_power_off;
		spd->gpd.power_on = sprd_domain_power_on;

		for (i = 0; i < REG_MAX; i++) {
			spd->regmap[i] = syscon_regmap_lookup_by_phandle_args(np,
								   sprd_reg_names[i],
								   2,
								   syscon_args);
			if (IS_ERR(spd->regmap[i])) {
				dev_err(&pdev->dev,
					"%s failed to parse %s regmap\n",
					spd->gpd.name, sprd_reg_names[i]);
				ret = PTR_ERR(spd->regmap[i]);
				break;
			}

			spd->reg[i] = syscon_args[0];
			spd->mask[i] = syscon_args[1];
			ret = 0;
		}

		if (ret) {
			kfree_const(spd->gpd.name);
			continue;
		}

		spd->force_pd = of_property_read_bool(np,
						      "sprd,force-shutdown");

		for (i = 0; i < REG_MAX; i++) {
			regmap_read(spd->regmap[i], spd->reg[i], &val);
			dev_dbg(&pdev->dev,
				 "%s %s reg = 0x%x value = 0x%x mask = 0x%x\n",
				 spd->gpd.name, sprd_reg_names[i], spd->reg[i],
				 val, spd->mask[i]);
		}

		pm_genpd_init(&spd->gpd, NULL, true);
		of_genpd_add_provider_simple(np, &spd->gpd);
	}

	for_each_child_of_node(root, np) {
		struct of_phandle_args child, parent;

		child.np = np;
		child.args_count = 0;

		if (of_parse_phandle_with_args(np, "power-domains",
					       "#power-domain-cells", 0,
					       &parent) != 0)
			continue;

		if (of_genpd_add_subdomain(&parent, &child))
			dev_warn(&pdev->dev,
				 "%pOF failed to add subdomain: %pOF\n",
				 parent.np, child.np);
		else
			dev_dbg(&pdev->dev,
				 "%pOF has as child subdomain: %pOF\n",
				 parent.np, child.np);
	}

	return 0;
}

static int sprd_domain_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id sprd_domain_of_match[] = {
	{ .compatible = "sprd,sharkl5pro-power-domain", },
	{ },
};

static struct platform_driver sprd_domain_driver = {
	.probe = sprd_domain_probe,
	.remove = sprd_domain_remove,
	.driver = {
		.name   = "sprd-pm-domain",
		.of_match_table = sprd_domain_of_match,
	},
};

module_platform_driver(sprd_domain_driver);

MODULE_AUTHOR("Sky Li <sky.li@unisoc.com>");
MODULE_DESCRIPTION("Spreadtrum power domain driver");
MODULE_LICENSE("GPL v2");
