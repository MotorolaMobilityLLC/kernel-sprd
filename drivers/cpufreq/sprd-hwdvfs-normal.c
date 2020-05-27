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
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/random.h>
#include <linux/topology.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/mfd/syscon.h>
#include <linux/printk.h>
#include "sprd-hwdvfs-normal.h"

static const struct of_device_id sprd_cpudvfs_of_match[] = {
#ifdef CONFIG_ARM_SPRD_HW_CPUFREQ_ARCH_UMS312
	{
		.compatible = "sprd,sharkl5-cpudvfs",
		.data = &ums312_dvfs_private_data,
	},
#endif
#ifdef CONFIG_ARM_SPRD_HW_CPUFREQ_ARCH_UMS512
	{
		.compatible = "sprd,sharkl5pro-cpudvfs",
		.data = &ums512_dvfs_private_data,
	},
#endif
	{
		/* Sentinel */
	},
};
MODULE_DEVICE_TABLE(of, sprd_cpudvfs_of_match);

static int cpudvfs_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct platform_device *platdev;
	struct cpudvfs_device *pdev;
	struct device_node *np, *attr_node;
	u32 dcdc;
	int ret = 0;


	np = client->dev.of_node;
	if (!np) {
		dev_err(&client->dev, "no i2c node found.\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "dvfs-dcdc-i2c", &dcdc);
	if (ret) {
		dev_err(&client->dev, "no dvfs-dcdc property found\n");
		goto parent_node_put;
	}

	attr_node = of_parse_phandle(np, "sprd,attributed-device", 0);
	if (!attr_node) {
		dev_err(&client->dev,
			"no associated cpu dvfs deivice appointed\n");
		ret = -EINVAL;
		goto parent_node_put;
	}

	platdev = of_find_device_by_node(attr_node);
	if (!platdev) {
		ret = -EPROBE_DEFER;
		goto child_node_put;
	}

	dev_dbg(&client->dev, "found the associated cpu dvfs device\n");

	pdev = platform_get_drvdata(platdev);
	if (!pdev) {
		dev_err(&platdev->dev, "no private driver data set\n");
		ret = -EINVAL;
		goto child_node_put;
	}

	if (dcdc >= pdev->dcdc_num) {
		dev_err(pdev->dev, "the dcdc id that used i2c channel"
			"is overflowing\n");
		ret = -EINVAL;
		goto child_node_put;
	}

	pdev->pwr[dcdc].i2c_client = client;

	dev_dbg(pdev->dev, "probe an i2c device for dcdc%d\n", dcdc);

child_node_put:
	of_node_put(attr_node);
parent_node_put:
	of_node_put(np);

	return ret;
}

static const struct of_device_id cpudvfs_dcdc_cpu0_i2c_of_match[] = {
	{.compatible = "sprd,cpudvfs-regulator-dcdc-cpu0-roc1",},
	{},
};
MODULE_DEVICE_TABLE(of, cpudvfs_dcdc_cpu0_i2c_of_match);

static const struct of_device_id cpudvfs_dcdc_cpu1_i2c_of_match[] = {
	{.compatible = "sprd,cpudvfs-regulator-sharkl5pro",},
	{.compatible = "sprd,cpudvfs-regulator-sharkl5",},
	{},
};
MODULE_DEVICE_TABLE(of, cpudvfs_dcdc_cpu1_i2c_of_match);

static struct i2c_driver sprd_cpudvfs_i2c_driver[] = {
	{
		.driver = {
			.name = "cpudvfs_dcdc_cpu0_i2c_drv",
			.owner = THIS_MODULE,
			.of_match_table = cpudvfs_dcdc_cpu0_i2c_of_match,
		},
		.probe = cpudvfs_i2c_probe,
	},
	{
		.driver = {
			.name = "cpudvfs_dcdc_cpu1_i2c_drv",
			.owner = THIS_MODULE,
			.of_match_table = cpudvfs_dcdc_cpu1_i2c_of_match,
		},
		.probe = cpudvfs_i2c_probe,
	}
};

static
void cpudvfs_bits_update(struct cpudvfs_device *pdev, u32 reg, u32 msk, u32 val)
{
	u32 tmp;

	tmp = readl((pdev->membase + reg)) & ~msk;
	tmp |= val & msk;

	writel(tmp, (pdev->membase + reg));
}

int default_dcdc_volt_update(struct regmap *map, struct reg_info *regs,
			     void *data, unsigned long u_volt, int index,
			     int count)
{
	u32 reg, off, msk, val;
	struct pmic_data *pm = (struct pmic_data *)data;

	if (index < 0 || index > count) {
		pr_err("%s: incorrcet voltage gear table index, index: %d, "
		       "count = %d\n", __func__, index, count);
		return -EINVAL;
	}

	if (!pm) {
		pr_err("%s: the pmic voltage grade table is NULL\n", __func__);
		return -ENODEV;
	}

	reg = regs[index].reg;
	off = regs[index].off;
	msk = regs[index].msk;

	if ((u_volt - pm->volt_base) % pm->per_step)
		u_volt += pm->per_step;

	val = (u_volt - pm->volt_base) / pm->per_step;

	pr_debug("TOP_DVFS_VOL_GRADE_TBL[%d]: reg = 0x%x, off = %d, msk = 0x%x,"
		 " val = 0x%x\n", index, reg, off, msk, val);

	return regmap_update_bits(map, reg, msk << off, val << off);
}

u32 default_cycle_calculate(u32 max_val_uV, u32 slew_rate,
			    u32 module_clk_hz, u32 margin_us)
{
	pr_debug("max_val_uV = %d, slew_rate = %d, module_clk_hz = %d "
		 "margin = %d\n", max_val_uV, slew_rate, module_clk_hz,
		 margin_us);

	return (max_val_uV / slew_rate + margin_us) * module_clk_hz / 1000;
}

static
void  fill_in_dvfs_tbl_entry(struct dvfs_cluster *clu, int entry_num,
			     u32 *entry_data, u32 column_size)
{
	struct cpudvfs_device *pdev = (struct cpudvfs_device *)clu->parent;
	struct cpudvfs_freq_manager *manager = pdev->priv->freq_manager;
	struct dvfs_index_entry_info *entry;
	struct reg_info *regs;
	u32 col = 0, val = 0;
	u32 off, msk;

	if (clu->is_host) {
		entry = manager->host_cluster_index_tbl[clu->id].entry_info;
		regs = manager->host_cluster_index_tbl[clu->id].regs;
	} else {
		entry = manager->slave_cluster_index_tbl[clu->id].entry_info;
		regs = manager->slave_cluster_index_tbl[clu->id].regs;
	}

	for (col = 0; col < column_size; ++col) {
		off = entry[col].off;
		msk = entry[col].msk;
		val &= ~(msk << off);
		val |= (entry_data[col] & msk) << off;

		dev_dbg(pdev->dev, "DTS_TBL[%d][%d]: %d(%s, %d, 0x%x)",
			entry_num, col, entry_data[col], entry[col].name,
			off, msk);
	}

	dev_dbg(pdev->dev, "INDEX[%d]: 0x%x, Reg[0x%x]\n", entry_num, val,
		regs[entry_num].reg);

	writel(val, pdev->membase + regs[entry_num].reg);
}

static int dvfs_map_tbl_init(struct cpudvfs_device *pdev,
			     struct dvfs_cluster *clu)
{
	u32 row, column, num;
	int ret;
	u32 *tbl;

	if (!clu->opp_map_tbl) {
		num = clu->tbl_row_num * clu->tbl_column_num;
		tbl = kcalloc(num, sizeof(u32), GFP_KERNEL);
		if (!tbl)
			return -ENOMEM;
		clu->opp_map_tbl = tbl;
	} else {
		tbl = clu->opp_map_tbl;
	}

	for (row = 0; row < clu->tbl_row_num; ++row) {
		for (column = 0; column < clu->tbl_column_num; ++column) {
			ret = of_property_read_u32_index(clu->of_node,
							 clu->dts_tbl_name,
					row * clu->tbl_column_num + column,
			(u32 *)&(tbl + row * clu->tbl_column_num)[column]);
			if (ret) {
				dev_err(pdev->dev,
					"error in parsing dts data\n");
				goto table_free;
			}
		}
		fill_in_dvfs_tbl_entry(clu, row,
				       (tbl + row * clu->tbl_column_num),
				       clu->tbl_column_num);
	}

	return 0;

table_free:
	kfree(clu->opp_map_tbl);

	return ret;
}

static void find_maximum_vol_diff(unsigned long *vol, int *max_diff_val,
				  int vol_size, int n)
{
	int i, j;
	u32 tmp_diff, max_diff = 0;
	int max_i = 0, max_j = 0;

	if (n == vol_size - 1)
		return;

	for (i = 0, j = i + n + 1; j < vol_size; ++i, ++j) {
		tmp_diff = vol[j] - vol[i];
		if (max_diff < tmp_diff) {
			max_i = i;
			max_j = j;
			max_diff = tmp_diff;
		}
	}

	pr_debug("j = %d, i = %d, vol[%d](%ld) - vol[%d](%ld) = %d\n",
		 max_j, max_i, max_j, vol[max_j], max_i, vol[max_i], max_diff);
	*max_diff_val = max_diff;

	return;
}

static int sprd_dvfs_module_eb(struct cpudvfs_device *pdev)
{
	int ret;
	struct device_node *node = pdev->of_node;
	u32 args[2], enable_reg, dvfs_eb_msk;

	pdev->aonmap = syscon_regmap_lookup_by_name(node, "dvfs-module-eb");
	if (IS_ERR(pdev->aonmap)) {
		dev_err(pdev->dev, "no dvfs module enable reg\n");
		return PTR_ERR(pdev->aonmap);
	}

	ret = syscon_get_args_by_name(node, "dvfs-module-eb", 2, args);
	if (ret != 2) {
		dev_err(pdev->dev, "failed to parse dvfs module enable reg\n");
		ret = -EINVAL;
		goto err_out;
	}

	enable_reg = args[0];
	dvfs_eb_msk = args[1];
	ret = regmap_update_bits(pdev->aonmap, enable_reg, dvfs_eb_msk,
				 dvfs_eb_msk);

	if (ret) {
		dev_err(pdev->dev, "failed to enable dvfs module\n");
		goto err_out;
	}

	dev_dbg(pdev->dev, "finish to enable dvfs module\n");

	return 0;

err_out:
	return ret;
}

static
int sprd_dvfs_third_pmic_enable(struct cpudvfs_device *pdev, u32 num)
{
	struct topdvfs_volt_manager *manager = pdev->priv->volt_manager;
	struct reg_info *regdata;
	int ret;

	if (num >= pdev->dcdc_num) {
		dev_err(pdev->dev, "incorrect dcdc id(%d)\n", num);
		return -EINVAL;
	}

	if (!pdev->pwr[num].third_pmic_used)
		return 0;

	regdata = &manager->third_pmic_cfg[num];

	ret = regmap_update_bits(pdev->topdvfs_map, regdata->reg,
				 BIT(regdata->off), BIT(regdata->off));
	if (ret)
		return ret;

	return 0;
}

static
int sprd_dvfs_block_dcdc_shutdown_enable(struct cpudvfs_device *pdev, u32 num)
{
	struct device_node *dcdc_node, *parent;
	struct regmap *blk_sd_map, *map;
	u32 syscon_args[2], reg, bits;
	int ret;

	if (num >= pdev->dcdc_num) {
		dev_err(pdev->dev, "incorrect dcdc id(%d)\n", num);
		return -EINVAL;
	}

	map = pdev->topdvfs_map;
	parent = pdev->topdvfs_of_node;

	dcdc_node = of_parse_phandle(parent, "cpu-dcdc-cells", num);
	if (!dcdc_node) {
		dev_err(pdev->dev, "no cpu-dcdc-cell%d found\n", num);
		of_node_put(parent);
		return -EINVAL;
	}
	blk_sd_map = syscon_regmap_lookup_by_name(dcdc_node,
							  "dvfs-blk-dcdc-sd");
	if (IS_ERR(blk_sd_map)) {
		dev_err(pdev->dev, "no regmap for dvfs block shutdown cfg\n");
		ret = PTR_ERR(blk_sd_map);
		goto out;
	}

	ret = syscon_get_args_by_name(dcdc_node, "dvfs-blk-dcdc-sd", 2,
				      syscon_args);
	if (ret != 2) {
		dev_err(pdev->dev,
			"failed to parse dvfs-blk-dcdc-sd syscon(%d)\n", ret);
		return -EINVAL;
	}

	reg = syscon_args[0];
	bits = syscon_args[1];

	ret = regmap_update_bits(blk_sd_map, reg, bits, bits);

out:
	of_node_put(dcdc_node);
	of_node_put(parent);

	return ret;
}

static
int sprd_mpll_output_source_switch(struct cpudvfs_device *pdev, u32 num)
{
	int ret;

	if (num >= pdev->mpll_num) {
		dev_err(pdev->dev, "invalid mpll id(%d)\n", num);
		return -EINVAL;
	}

	dev_dbg(pdev->dev, "MPLL%d_debug_sel_cfg: reg: 0x%x, sel: 0x%x\n", num,
		pdev->mplls[num].anag_reg, pdev->mplls[num].dbg_sel);

	ret = regmap_update_bits(pdev->mplls[num].anag_map,
				 pdev->mplls[num].anag_reg,
				 pdev->mplls[num].dbg_sel,
				 0);
	if (ret)
		return ret;

	/* Need to delay to wait for finishing analog configuration */
	udelay(50);

	pdev->mplls[num].output_switch_done = true;

	return 0;
}

static
int sprd_mpll_relock_enable(struct cpudvfs_device *pdev, u32 num)
{
	struct mpll_freq_manager *manager = pdev->priv->mpll_manager;
	struct reg_info *regdata;
	u32 enable;

	if (num >= pdev->mpll_num) {
		dev_err(pdev->dev, "invalid mpll number(%d)\n", num);
		return -EINVAL;
	}

	if (!pdev->mplls[num].output_switch_done) {
		dev_err(pdev->dev, "the output of mpll%d has not be switched to"
			"DVFS , please switch it first\n", num);
		return -EINVAL;
	}

	regdata = &manager->auto_relock_cfg[num];
	if (!regdata->reg && !regdata->off) {
		dev_err(pdev->dev, "MPLL%d auto relock reg info is empty\n",
			num);
		return -ENODEV;
	}

	dev_dbg(pdev->dev, "MPLL%d_relock_cfg: reg = 0x%x, off = 0x%x\n", num,
		regdata->reg, regdata->off);

	enable = regdata->val;
	if (enable)
		cpudvfs_bits_update(pdev, regdata->reg, BIT(regdata->off),
				     BIT(regdata->off));
	else
		cpudvfs_bits_update(pdev, regdata->reg, BIT(regdata->off),
				     ~BIT(regdata->off));

	dev_dbg(pdev->dev, "enable mpll%d to auto relock\n", num);

	return 0;
}

static
int sprd_mpll_pd_enable(struct cpudvfs_device *pdev, u32 num)
{
	struct mpll_freq_manager *manager = pdev->priv->mpll_manager;
	struct reg_info *regdata;
	u32 enable;

	if (num >= pdev->mpll_num) {
		dev_err(pdev->dev, "invalid mpll number\n");
		return -EINVAL;
	}

	if (!pdev->mplls[num].output_switch_done) {
		dev_err(pdev->dev, "the output of mpll%d has not be switched to"
			"DVFS , please switch it first\n", num);
		return -EINVAL;
	}

	regdata = &manager->auto_pd_cfg[num];
	if (!regdata->reg && !regdata->off) {
		dev_err(pdev->dev, "MPLL%d_auto_pd_cfg, reg info is empty\n",
			num);
		return -ENODEV;
	}

	dev_dbg(pdev->dev, "MPLL%d_auto_pd_cfg: reg = 0x%x, off = 0x%x\n", num,
		regdata->reg, regdata->off);


	enable = regdata->val;
	if (enable)
		cpudvfs_bits_update(pdev, regdata->reg, BIT(regdata->off),
				     BIT(regdata->off));
	else
		cpudvfs_bits_update(pdev, regdata->reg, BIT(regdata->off),
				     ~BIT(regdata->off));

	dev_dbg(pdev->dev, "enable mpll%d to auto power down\n", num);

	return 0;
}

static
int sprd_host_cluster_dvfs_enable(struct cpudvfs_device *pdev, u32 clu_id,
				  bool enable)
{
	struct topdvfs_volt_manager *manager = pdev->priv->volt_manager;
	struct dvfs_cluster *clu;
	struct reg_info *regdata;
	int ret;

	if (clu_id >= pdev->host_cluster_num) {
		dev_err(pdev->dev, "the host cluster id(%d) is overflowing\n",
			clu_id);
		return -EINVAL;
	}

	clu = pdev->hcluster[clu_id];
	if (!clu) {
		dev_err(pdev->dev, "the host-cluster%d is null\n", clu_id);
		return -ENODEV;
	}

	if (clu->dcdc >= pdev->dcdc_num) {
		dev_err(pdev->dev, "the cluster's dcdc id(%d) is overflowing\n",
			clu->dcdc);
		return -EINVAL;
	}

	dev_dbg(pdev->dev, "cluster%d's hardware dvfs function is %s\n",
		clu_id, (enable > 0 ? "enabled" : "disabled"));

	/* Enable TOP DVFS to change voltage dynamically */
	regdata = &manager->host_vol_auto_tune_cfg[clu->dcdc];

	if (enable && !regdata->val) {
		ret = regmap_update_bits(pdev->topdvfs_map, regdata->reg,
					 BIT(regdata->off), ~BIT(regdata->off));
		if (ret)
			return ret;
	}

	/* Enable Subsys DVFS to change frequency dynamically */
	regdata = &manager->host_freq_auto_tune_cfg[clu_id];
	if (enable && !regdata->val) {
		ret = regmap_update_bits(pdev->topdvfs_map, regdata->reg,
					 BIT(regdata->off), ~BIT(regdata->off));
		if (ret)
			return ret;
	}

	/* Nothing to do when enable is false */

	dev_dbg(pdev->dev, "enable cpu host cluster%d dvfs\n", clu_id);

	return 0;
}

static
int sprd_slave_cluster_auto_tune_enable(struct cpudvfs_device *pdev, u32 clu_id,
					bool enable)
{
	struct dvfs_cluster *clu;
	struct reg_info *regdata;

	clu = pdev->scluster[clu_id];
	if (!clu) {
		dev_err(pdev->dev, "the slave-cluster%d is null\n", clu_id);
		return -ENODEV;
	}

	regdata = &pdev->priv->freq_manager->slave_freq_auto_tune_cfg[clu->id];

	if (enable && regdata->val) /* enable */
		cpudvfs_bits_update(pdev, regdata->reg, BIT(regdata->off),
				    BIT(regdata->off));
	else
		cpudvfs_bits_update(pdev, regdata->reg, BIT(regdata->off),
				    ~BIT(regdata->off));

	dev_dbg(pdev->dev, "enable cpu slave cluster%d dvfs\n", clu->id);

	return 0;
}

static
int sprd_cpudvfs_cluster_dvfs_enable(void *data, u32 clu_id, bool enable,
				     bool is_host)
{
	struct cpudvfs_device *pdev = (struct cpudvfs_device *)data;
	int ret;

	if (!pdev) {
		pr_err("no cpu dvfs device found\n");
		return -ENODEV;
	}

	if (is_host)
		ret = sprd_host_cluster_dvfs_enable(pdev, clu_id, enable);
	else
		ret = sprd_slave_cluster_auto_tune_enable(pdev, clu_id, enable);

	return ret;
}

static int sprd_dvfs_idle_disable(struct cpudvfs_device *pdev, u32 device_id)
{
	struct cpudvfs_freq_manager *manager = pdev->priv->freq_manager;
	struct reg_info *regdata;

	if (device_id >= manager->dvfs_device_num) {
		dev_err(pdev->dev, "the dvfs device id(%d) is overflowing\n",
			device_id);
		return -EINVAL;
	}

	regdata = &manager->dev_idle_disable_cfg[device_id];

	if (regdata->val) /* disable dvfs idle */
		cpudvfs_bits_update(pdev, regdata->reg, BIT(regdata->off),
				    BIT(regdata->off));
	else
		cpudvfs_bits_update(pdev, regdata->reg, BIT(regdata->off),
				    ~BIT(regdata->off));

	return 0;
}

static int sprd_mpll_table_init(struct cpudvfs_device *pdev, u32 mpll_num)
{
	struct mpll_freq_manager *manager;
	int i = 0, ret;
	struct regmap *map;
	struct reg_info *regdata;
	struct output_parameter *en;

	manager = pdev->priv->mpll_manager;
	if (!manager || !manager->mpll_tbl) {
		dev_err(pdev->dev, "empty mpll table\n");
		return -EINVAL;
	}

	if (mpll_num >= manager->mpll_num) {
		dev_err(pdev->dev, "invalid mpll id\n");
		return -EINVAL;
	}

	map = pdev->mplls[mpll_num].anag_map;
	en = &manager->mpll_tbl[mpll_num].entry[0];
	regdata = &en->icp;

	while (!(!regdata->reg && !regdata->off && !regdata->msk &&
		 !regdata->val)){
		dev_dbg(pdev->dev,
			"MPLL%d-index%d-icp: reg = 0x%x, msk = 0x%x, "
			"off = %d, val = 0x%x\n", mpll_num, i, regdata->reg,
			regdata->msk, regdata->off, regdata->val);

		ret = regmap_update_bits(map, regdata->reg,
					 regdata->msk << regdata->off,
					 regdata->val << regdata->off);
		if (ret)
			return ret;

		regdata = &en->postdiv;

		dev_dbg(pdev->dev,
			"MPLL%d-index%d-postdiv: reg = 0x%x, msk = 0x%x, "
			"off = %d, val = 0x%x\n", mpll_num, i, regdata->reg,
			regdata->msk, regdata->off, regdata->val);

		ret = regmap_update_bits(map, regdata->reg,
					 regdata->msk << regdata->off,
					 regdata->val << regdata->off);
		if (ret)
			return ret;

		regdata = &en->n;

		dev_dbg(pdev->dev,
			"MPLL%d-index%d-n: reg = 0x%x, msk = 0x%x, "
			"off = %d, val = 0x%x\n", mpll_num, i, regdata->reg,
			regdata->msk, regdata->off, regdata->val);

		ret = regmap_update_bits(map, regdata->reg,
					 regdata->msk << regdata->off,
					 regdata->val << regdata->off);
		if (ret)
			return ret;

		if (++i >= MAX_MPLL_INDEX_NUM)
			break;

		en = &manager->mpll_tbl[mpll_num].entry[i];
		regdata = &en->icp;
	}

	return 0;
}

static int sprd_hw_dvfs_misc_config(struct cpudvfs_device *pdev)
{
	struct topdvfs_volt_manager *volt_manager;
	struct cpudvfs_freq_manager *freq_manager;
	struct reg_info *en;
	struct regmap *map;
	int i = 0, j = 0, ret;

	volt_manager = pdev->priv->volt_manager;
	freq_manager = pdev->priv->freq_manager;

	if (!volt_manager || !freq_manager) {
		pr_err("Voltage or frequency manger is NULL\n");
		return -EINVAL;
	}

	map = pdev->topdvfs_map;
	/* Voltage related configurations */
	en = &volt_manager->misc_cfg[0];

	while (!(!en->reg && !en->off && !en->msk && !en->val)) {
		dev_dbg(pdev->dev,
			"TOP_DVFS_MISC_CFG[%d]: reg = 0x%x, msk = 0x%x, "
			"off = %d, val = 0x%x\n", i, en->reg, en->msk, en->off,
			en->val);

		ret = regmap_update_bits(map, en->reg, en->msk << en->off,
					 en->val << en->off);
		if (ret)
			return ret;

		if (++i >= MAX_TOP_DVFS_MISC_CFG_ENTRY)
			break;

		en = &volt_manager->misc_cfg[i];
	}

	/* Frequency related configurations */
	en = &freq_manager->misc_cfg[0];

	while (!(!en->reg && !en->off && !en->msk && !en->val)) {
		dev_dbg(pdev->dev,
			"APCPU_DVFS_MISC_CFG[%d]: reg = 0x%x, msk = 0x%x, "
			"off = %d, val = 0x%x\n", j, en->reg, en->msk, en->off,
			en->val);

		cpudvfs_bits_update(pdev, en->reg, en->msk << en->off,
				     en->val << en->off);

		if (++j >= MAX_APCPU_DVFS_MISC_CFG_ENTRY)
			break;

		en = &freq_manager->misc_cfg[j];
	}

	return 0;
}

static int sprd_setup_i2c_channel(struct cpudvfs_device *pdev, u32 dcdc_nr)
{
	int ret;

	if (!pdev->pwr) {
		dev_err(pdev->dev, "no DCDC Power domain found\n");
		return -EINVAL;
	}

	if (dcdc_nr >= pdev->dcdc_num) {
		dev_err(pdev->dev, "incorrect dcdc id(%d)\n", dcdc_nr);
		return -EINVAL;
	}

	if (pdev->pwr[dcdc_nr].i2c_used && pdev->pwr[dcdc_nr].i2c_shared) {
		ret = i2c_add_driver(&sprd_cpudvfs_i2c_driver[dcdc_nr]);
		if (ret)
			dev_err(pdev->dev, "failed to add an i2c driver\n");
		else
			dev_info(pdev->dev, "the dcdc%d shares the i2c channel "
				 "with other devices\n", dcdc_nr);
	} else if (pdev->pwr[dcdc_nr].i2c_used &&
		   !pdev->pwr[dcdc_nr].i2c_shared) {
		dev_info(pdev->dev, "the dcdc%d use the i2c channel alone\n",
			 dcdc_nr);
	} else {
		dev_info(pdev->dev, "the dcdc%d does not need an i2c channel\n",
			 dcdc_nr);
	}

	return 0;
}

/*
 * sprd_dvfs_device_init - configure hardware dvfs,
 * not including enabling hardware dvfs for every host cpu cluster
 */
static int sprd_dvfs_device_init(struct cpudvfs_device *pdev)
{
	int ret, ix;

	ret = sprd_dvfs_module_eb(pdev);
	if (ret) {
		dev_err(pdev->dev, "failed to enable dvfs module(%d)\n", ret);
		return ret;
	}

	for (ix = 0; ix < pdev->mpll_num; ++ix) {
		/* Let DVFS control the output of MPLL */
		ret = sprd_mpll_output_source_switch(pdev, ix);
		if (ret) {
			dev_err(pdev->dev,
				"failed to let dvfs control output of mpll\n");
			return ret;
		}

		/* Enable mpll auto relock */
		ret = sprd_mpll_relock_enable(pdev, ix);
		if (ret) {
			dev_err(pdev->dev,
				"failed to enable mpll auto relock\n");
			return ret;
		}

		/* Enable mpll auto power down */
		ret = sprd_mpll_pd_enable(pdev, ix);
		if (ret) {
			dev_err(pdev->dev, "failed to enable mpll auto pd\n");
			return ret;
		}

		/* Need to initialize the mpll index table if necessary */
		ret = sprd_mpll_table_init(pdev, ix);
		if (ret) {
			dev_err(pdev->dev,
				"failed to initialize the mpll index table\n");
			return ret;
		}
	}

	for (ix = 0; ix < pdev->dcdc_num; ++ix) {
		ret = sprd_setup_i2c_channel(pdev, ix);
		if (ret) {
			dev_err(pdev->dev, "failed to setup i2c channel%d\n",
				ix);
			return ret;
		}

		ret = sprd_dvfs_block_dcdc_shutdown_enable(pdev, ix);
		if (ret) {
			dev_err(pdev->dev, "failed to enable dvfs to block "
				"dcdc%d to shutdown\n", ix);
			return ret;
		}

		ret = sprd_dvfs_third_pmic_enable(pdev, ix);
		if (ret) {
			dev_err(pdev->dev, "failed to select the third pmic "
				"when the third pmic is used for dcdc%d\n", ix);
			return ret;
		}
	}

	for (ix = 0; ix < pdev->dvfs_device_num; ++ix) {
		ret = sprd_dvfs_idle_disable(pdev, ix);
			if (ret) {
			dev_err(pdev->dev, "failed to set dvfs idle\n");
			return ret;
		}
	}

	/* Other common board dvfs configurations */
	ret = sprd_hw_dvfs_misc_config(pdev);
	if (ret) {
		dev_err(pdev->dev, "error in setting misc configurations\n");
		return ret;
	}

	return 0;
}

static int sprd_voltage_grade_value_update(struct dvfs_cluster *clu,
					   unsigned long volt, int opp_idx)
{
	struct cpudvfs_device *pdev = (struct cpudvfs_device *)clu->parent;
	struct reg_info *regdata;
	struct pmic_data *pm;
	int ret, count, grade_index = clu->dcdc;
	u32 pmic_num, grade_id;

	if (!pdev->priv->volt_manager || !pdev->priv->volt_manager->grade_tbl ||
	    !pdev->priv->pmic) {
		dev_err(pdev->dev, "the voltage grade table is NULL\n");
		return -EINVAL;
	}

	if (opp_idx == 0) {
		clu->pre_grade_volt = 0;
		clu->tmp_vol_grade  = 0;
		clu->max_vol_grade  = 0;
	}

	if (clu->pre_grade_volt == volt)
		return 0;

	if (clu->pre_grade_volt > volt) {
		dev_err(pdev->dev, "the voltages are not in ascending order\n");
		return -EINVAL;
	}

	/*
	 * The registers to store voltage grade values in pmic are different,
	 * if the way to adjust voltage is different, so get the right registers
	 */
	if (pdev->pwr[clu->dcdc].i2c_used)
		grade_index += pdev->priv->volt_manager->vir_dcdc_adi_num;

	regdata = pdev->priv->volt_manager->grade_tbl[grade_index].regs_array;
	if (!regdata) {
		dev_err(pdev->dev, "empty voltage grade reg info\n");
		return -EINVAL;
	}

	/* Get the count of voltage grade */
	count = pdev->priv->volt_manager->grade_tbl[grade_index].grade_count;

	dev_dbg(pdev->dev, "grade_index in volt grade table array is %d\n",
		grade_index);

	/*
	 * Get the current voltage grade id and store the corresponding
	 * voltage value
	 */
	grade_id = clu->tmp_vol_grade++;
	if (grade_id >= MAX_VOLT_GRADE_NUM) {
		dev_err(pdev->dev, "the volt grade number(%d) is beyond the "
			"maximun(%d)\n", grade_id, MAX_VOLT_GRADE_NUM);
		return -EINVAL;
	}
	pdev->pwr[clu->dcdc].grade_volt_val_array[grade_id] = volt;

	/* Update the max volage grade */
	if (clu->max_vol_grade < grade_id)
		clu->max_vol_grade = grade_id;

	dev_dbg(pdev->dev, "voltage grade[%d]: vol[%ld]uV\n", grade_id, volt);

	/* Get pmic id */
	pmic_num = pdev->pwr[clu->dcdc].pmic_num;
	if (pmic_num >= pdev->pmic_type_sum) {
		dev_err(pdev->dev, "incorrect pmic sequenc number\n");
		return -EINVAL;
	}

	/* Find the pmic */
	pm = &pdev->priv->pmic[pmic_num];
	if (!pm) {
		dev_err(pdev->dev, "empty pmic info\n");
		return -EINVAL;
	}

	/*
	 * Update the voltage value in pmic that is matched with current
	 * voltage grade id
	 */
	ret = pm->update(pdev->topdvfs_map, regdata, pm, volt, grade_id, count);
	if (ret) {
		dev_err(pdev->dev, "error in updating volt gear values for "
			"dcdc%d\n", clu->dcdc);
		return ret;
	}

	clu->pre_grade_volt = volt;

	return 0;
}

static
int sprd_cpudvfs_opp_table_update(void *data, u32 clu_id, unsigned long freq_hz,
				  unsigned long volt_uV, int opp_idx)
{
	struct cpudvfs_device *pdev = (struct cpudvfs_device *)data;
	struct dvfs_cluster *clu;
	int ret;

	if (clu_id >= pdev->host_cluster_num) {
		dev_err(pdev->dev, "cluster id(%d) is overflowing\n", clu_id);
		return -EINVAL;
	}

	clu = pdev->hcluster[clu_id];
	if (!clu) {
		dev_err(pdev->dev, "cluster device is null\n");
		return -EINVAL;
	}

	clu->freqvolt[opp_idx].freq = freq_hz;
	clu->freqvolt[opp_idx].volt = volt_uV;

	if (clu->map_idx_max < opp_idx)
		clu->map_idx_max = opp_idx;

	/* Fill in binning voltages dynamically*/
	ret = sprd_voltage_grade_value_update(clu, volt_uV, opp_idx);
	if (ret)
		dev_err(pdev->dev, "failed to update opp table in arch dev\n");

	return ret;
}

static
int sprd_cpudvfs_time_udelay_update(void *data, u32 clu_id)
{
	struct cpudvfs_device *pdev = (struct cpudvfs_device *)data;
	struct dvfs_cluster *clu;
	int ret = 0, i;
	u32 max_value = 0;
	struct pmic_data *pm;
	u32 slew_rate, module_clk_khz, margin;
	u32 cycle, reg, off, msk, pmic_id;
	struct topdvfs_volt_manager *manager;

	if (clu_id >= pdev->host_cluster_num) {
		dev_err(pdev->dev, "cluster id (%d) is overflowing\n", clu_id);
		return -EINVAL;
	}

	clu = pdev->hcluster[clu_id];
	if (!clu) {
		dev_err(pdev->dev, "the cluster%d to get is null\n", clu_id);
		return -ENODEV;
	}

	dev_dbg(pdev->dev, "Update voltage delay time for dcdc%d\n", clu->dcdc);

	pmic_id = pdev->pwr[clu->dcdc].pmic_num;
	if (pmic_id >= pdev->pmic_type_sum) {
		dev_err(pdev->dev, "incorrect pmic sequenc number\n");
		return -EINVAL;
	}

	pm = &pdev->priv->pmic[pmic_id];
	if (!pm) {
		dev_err(pdev->dev, "empty private data\n");
		return -EINVAL;
	}

	module_clk_khz = pdev->priv->module_clk_khz;
	margin = pdev->priv->pmic[pmic_id].margin_us;
	manager = pdev->priv->volt_manager;
	slew_rate = pdev->pwr[clu->dcdc].slew_rate;

	for (i = 0; i <= clu->max_vol_grade; ++i) {
		find_maximum_vol_diff(pdev->pwr[clu->dcdc].grade_volt_val_array,
				      &max_value, clu->max_vol_grade + 1, i);

		cycle = pm->up_cycle_calculate(max_value, slew_rate,
					       module_clk_khz, margin);
		dev_dbg(pdev->dev, "up udelay[%d] = 0x%x\n", i, cycle);

		reg = manager->up_udelay_tbl[clu->dcdc].tbl[i].reg;
		off = manager->up_udelay_tbl[clu->dcdc].tbl[i].off;
		msk = manager->up_udelay_tbl[clu->dcdc].tbl[i].msk;

		ret = regmap_update_bits(pdev->topdvfs_map, reg, msk << off,
					 cycle << off);
		if (ret)
			return ret;

		cycle = pm->down_cycle_calculate(max_value, slew_rate,
						 module_clk_khz, margin);
		dev_dbg(pdev->dev, "down udelay[%d] = 0x%x\n", i, cycle);

		reg = manager->down_udelay_tbl[clu->dcdc].tbl[i].reg;
		off = manager->down_udelay_tbl[clu->dcdc].tbl[i].off;
		msk = manager->down_udelay_tbl[clu->dcdc].tbl[i].msk;

		ret = regmap_update_bits(pdev->topdvfs_map, reg, msk << off,
					 cycle << off);
		if (ret)
			return ret;
	}

	return 0;
}

static
int sprd_cpudvfs_index_map_table_update(void *data, char *opp_name, u32 clu_id,
					char *cpudiff_str, char *cpubin_str,
					int curr_temp_threshold)
{
	struct cpudvfs_device *pdev = (struct cpudvfs_device *)data;
	struct dvfs_cluster *clu, *slave;
	char temp[32];
	char postfix[32] = "";
	int ret, idx = 0;

	if (clu_id >= pdev->host_cluster_num) {
		dev_err(pdev->dev, "the host cluster id is overflowing\n");
		return -EINVAL;
	}

	if (!opp_name || !cpudiff_str || !cpubin_str) {
		dev_err(pdev->dev, "empty parameters!\n");
		return -EINVAL;
	}

	/* optional */
	if (strcmp(cpudiff_str, "")) {
		sprintf(temp, "_%s", cpudiff_str);
		strcat(postfix, temp);
	}

	/* optional */
	if (!strcmp(cpubin_str, ""))
		goto init_map;

	sprintf(temp, "_%s", cpubin_str);
	strcat(postfix, temp);

	/* cpu binning value is required when we switch temperature table */
	if (curr_temp_threshold) {
		sprintf(temp, "_%d", curr_temp_threshold);
		strcat(postfix, temp);
	}

	dev_dbg(pdev->dev, "the postfix: %s\n", postfix);

init_map:
	clu = pdev->hcluster[clu_id];
	strcpy(clu->dts_tbl_name, clu->default_tbl_name);
	strcat(clu->dts_tbl_name, postfix);

	/*
	 * Use the default dts_tbl_name, if opp name is "operating-points",
	 * otherwise change the dts_tbl_name.
	 */
	dev_dbg(pdev->dev, "update dvfs index map table for host cluster%d "
		"host-tbl-name: %s\n", clu_id, clu->dts_tbl_name);

	ret = dvfs_map_tbl_init(pdev, clu);
	if (ret < 0)
		return ret;

	/*
	 * Initialize the slave clusters whose power domain is same with the
	 * current host cluster.
	 */
	slave = pdev->scluster[0];

	while (slave) {
		if (slave->dcdc == clu->dcdc) {
			strcpy(slave->dts_tbl_name, slave->default_tbl_name);
			strcat(slave->dts_tbl_name, postfix);

			dev_dbg(pdev->dev,
				"update dvfs index map tbl for slave cluster%d"
				"(%s)that belong to the host cluster%d\n", idx,
				slave->dts_tbl_name, clu->id);

			ret = dvfs_map_tbl_init(pdev, slave);
			if (ret < 0)
				return ret;
		}
		slave = pdev->scluster[++idx];
	}

	return 0;
}

/*
 * Set dvfs idle pd voltage, it's needed to set pd voltage as the max
 * grade when dvfs idle is disabled in sharkl5.
 */
static
int sprd_cpudvfs_idle_pd_volt_update(void *data, u32 clu_id)
{
	struct cpudvfs_device *pdev = (struct cpudvfs_device *)data;
	struct topdvfs_volt_manager *manager = pdev->priv->volt_manager;
	struct reg_info *regdata;
	struct dvfs_cluster *clu;
	u32 value, vol_max;
	int ret;

	if (clu_id >= pdev->host_cluster_num) {
		dev_err(pdev->dev, "the cluster number is overflowing\n");
		return -EINVAL;
	}

	clu = pdev->hcluster[clu_id];

	if (clu->dcdc >= pdev->dcdc_num) {
		dev_err(pdev->dev, "the dcdc id(%d) is overflowing\n",
			clu->dcdc);
		return -EINVAL;
	}

	/* Some platforms do not need to set idle pd voltage,
	 * then return 0
	 */
	regdata = &manager->idle_vol_cfg[clu->dcdc];
	if (!regdata->val)
		return 0;

	vol_max = clu->max_vol_grade;

	/*
	 * Since the registers about idle voltage don't support set/clr, we
	 * cannot use regmap_update_bits().
	 */
	ret = regmap_read(pdev->topdvfs_map, regdata->reg, &value);
	if (ret)
		return ret;

	value &= ~(regdata->msk << regdata->off);
	value |= vol_max << regdata->off;

	ret = regmap_write(pdev->topdvfs_map, regdata->reg, value);
	if (ret) {
		dev_err(pdev->dev, "failed to set idle voltage for "
			"dcdc-cpu%d\n", clu->dcdc);
		return ret;
	}

	return 0;
}

static
int sprd_cpudvfs_dvfs_target_set(void *data, u32 clu_id, u32 opp_idx)
{
	struct cpudvfs_device *pdev = (struct cpudvfs_device *)data;
	struct cpudvfs_freq_manager *manager = pdev->priv->freq_manager;
	struct dvfs_cluster *clu;
	struct i2c_client *client;
	struct reg_info *regdata;
	int hw_map_opp_idx = 0;
	int i2c_flag = 0;

	if (clu_id >= pdev->host_cluster_num) {
		dev_err(pdev->dev, "the host cluster id is overflowing\n");
		return -EINVAL;
	}

	/* Consider the default first entry 'XTL_26M' in hw dvfs table */
	hw_map_opp_idx = opp_idx + 1;

	clu = pdev->hcluster[clu_id];
	if (opp_idx >= clu->tbl_row_num) {
		dev_err(pdev->dev, "invalid dvfs tbl index of host-cluster%d\n",
			clu_id);
		return -EINVAL;
	}


	if (pdev->pwr[clu->dcdc].i2c_used && pdev->pwr[clu->dcdc].i2c_shared) {
		client = pdev->pwr[clu->dcdc].i2c_client;
		if (!client) {
			dev_err(pdev->dev, "no i2c device found for dcdc%d\n",
				clu->dcdc);
			return -ENODEV;
		}
		i2c_lock_adapter(client->adapter);
		i2c_flag = 1;
	}

	regdata = &manager->hcluster_work_index_cfg[clu_id];

	writel(hw_map_opp_idx, pdev->membase + regdata->reg);

	/* Delay here to wait for finishing dvfs operations by hardware */
	udelay(pdev->pwr[clu->dcdc].tuning_latency_us);

	if (i2c_flag)
		i2c_unlock_adapter(client->adapter);

	return 0;
}

static
unsigned int sprd_cpudvfs_curr_freq_get(void *data, u32 clu_id)
{
	struct cpudvfs_device *pdev = (struct cpudvfs_device *)data;
	struct cpudvfs_freq_manager *manager = pdev->priv->freq_manager;
	struct reg_info *regdata;
	struct dvfs_cluster *clu;
	int hw_opp_index, sw_opp_index;

	clu = pdev->hcluster[clu_id];
	if (!clu) {
		dev_err(pdev->dev, "the cluster to access is null\n");
		return 0;
	}

	regdata = &manager->hcluster_work_index_cfg[clu_id];

	hw_opp_index = (readl(pdev->membase + regdata->reg) >> regdata->off) &
		regdata->msk;

	sw_opp_index = hw_opp_index - 1;

	return clu->freqvolt[sw_opp_index].freq / 1000;
}

/*
 * Special hardware dvfs operations in SharkL5 family SOCs
 */
struct cpudvfs_phy_ops sprd_cpudvfs_phy_ops = {
	.dvfs_enable = sprd_cpudvfs_cluster_dvfs_enable,
	.volt_grade_table_update = sprd_cpudvfs_opp_table_update,
	.udelay_update = sprd_cpudvfs_time_udelay_update,
	.index_map_table_update = sprd_cpudvfs_index_map_table_update,
	.idle_pd_volt_update = sprd_cpudvfs_idle_pd_volt_update,
	.target_set = sprd_cpudvfs_dvfs_target_set,
	.freq_get = sprd_cpudvfs_curr_freq_get,
};

static  int sprd_mpll_dt_parse(struct cpudvfs_device *pdev)
{
	u32 idx = 0, num;
	struct device_node *node = NULL;
	struct regmap *map;
	int ret = 0;
	u32 args[2];

	if (!of_find_property(pdev->of_node, "mpll-cells", &num)) {
		dev_err(pdev->dev, "no mpll-cells node found\n");
		of_node_put(pdev->of_node);
		return -ENODEV;
	}

	num = num / sizeof(u32);
	if (num > MAX_MPLL) {
		dev_err(pdev->dev, "the number of mpll(%d) is beyond the "
			"maximum value(%d)", num, MAX_MPLL);
		return -EINVAL;
	}

	pdev->mplls = kcalloc(num, sizeof(struct mpll_cfg), GFP_KERNEL);
	if (!pdev->mplls) {
		of_node_put(pdev->of_node);
		return -ENOMEM;
	}

	for (idx = 0; idx < num; ++idx) {
		struct mpll_freq_manager *man = pdev->priv->mpll_manager;

		if (!man) {
			dev_err(pdev->dev, "empty mpll frequency manager\n");
			ret = -EINVAL;
			goto mpll_free;
		}

		node =	of_parse_phandle(pdev->of_node, "mpll-cells", idx);
		if (!node) {
			dev_err(pdev->dev, "failed to get mpll%d node\n", idx);
			ret = -EINVAL;
			goto mpll_free;
		}

		map = syscon_regmap_lookup_by_name(node, "mpll-dbg-sel");
		if (IS_ERR(map)) {
			dev_err(pdev->dev, "no mpll-dbg-sel found\n");
			ret = PTR_ERR(map);
			goto mpll_free;
		}
		pdev->mplls[idx].anag_map = map;

		ret = syscon_get_args_by_name(node, "mpll-dbg-sel", 2, args);
		if (ret != 2) {
			dev_err(pdev->dev, "invalid mpll-dbg-sel parameter "
				"number(%d)\n", ret);
			ret = -EINVAL;
			goto mpll_free;
		}
		pdev->mplls[idx].anag_reg = args[0];
		pdev->mplls[idx].dbg_sel = args[1];

		of_node_put(node);
	}

	pdev->mpll_num = num;

	of_node_put(pdev->of_node);

	dev_dbg(pdev->dev, "finish to parse mpll information in dts\n");

	return 0;

mpll_free:
	if (node)
		of_node_put(node);
	of_node_put(pdev->of_node);
	kfree(pdev->mplls);
	pdev->mplls = NULL;

	return ret;
}

static int sprd_dcdc_dt_parse(struct cpudvfs_device *pdev)
{
	struct device_node *node, *dcdc_node = NULL, *cfg_node = NULL;
	u32 nr, ix;
	struct regmap *map;
	int ret;

	node = of_parse_phandle(pdev->of_node, "topdvfs-controller", 0);
	if (!node) {
		dev_err(pdev->dev, "no topdvfs-controller found\n");
		ret = -EINVAL;
		goto err_out;
	}

	pdev->topdvfs_of_node = node;

	map = syscon_node_to_regmap(node);
	if (IS_ERR(map)) {
		dev_err(pdev->dev, "no regmap for syscon topdvfs\n");
		ret = -ENODEV;
		goto err_out;
	}

	pdev->topdvfs_map = map;

	if (!of_find_property(node, "cpu-dcdc-cells", &nr)) {
		dev_err(pdev->dev, "no cpu-dcdc-cells found\n");
		ret = -EINVAL;
		goto err_out;
	}

	nr = nr / sizeof(u32);

	if (nr != pdev->priv->volt_manager->dcdc_num) {
		dev_err(pdev->dev, "the total dcdc number is not matched\n");
		ret = -EINVAL;
		goto err_out;
	}

	pdev->dcdc_num = nr;

	dev_dbg(pdev->dev, "the dcdc number: %d\n", pdev->dcdc_num);

	pdev->pwr = kcalloc(nr, sizeof(struct dcdc_pwr), GFP_KERNEL);
	if (!pdev->pwr) {
		ret = -ENOMEM;
		goto err_out;
	}

	for (ix = 0; ix < pdev->dcdc_num; ix++) {
		dcdc_node = of_parse_phandle(node, "cpu-dcdc-cells", ix);
		if (!dcdc_node) {
			dev_err(pdev->dev, "no cpu-dcdc-cell%d found\n", ix);
			ret = -EINVAL;
			goto err_power_free;
		}

		cfg_node = of_parse_phandle(dcdc_node, "dcdc-cpu-dvfs-cfg", 0);
		if (!cfg_node) {
			dev_err(pdev->dev, "no dvfs dcdc configure found\n");
			ret = -EINVAL;
			goto err_power_free;
		}

		if (of_property_read_bool(cfg_node, "third-pmic-used"))
			pdev->pwr[ix].third_pmic_used = true;

		ret = of_property_read_u32(cfg_node, "pmic-type-num",
					   &pdev->pwr[ix].pmic_num);
		if (ret) {
			dev_err(pdev->dev, "no kind of pmic found\n");
			goto err_power_free;
		}

		ret = of_property_read_u32(cfg_node, "slew-rate",
					   &pdev->pwr[ix].slew_rate);
		if (ret) {
			dev_err(pdev->dev, "no slew rate of pmic found\n");
			goto err_power_free;
		}

		ret = of_property_read_u32(cfg_node, "tuning-latency-us",
					   &pdev->pwr[ix].tuning_latency_us);
		if (ret) {
			dev_err(pdev->dev, "no tune latency of dvfs found\n");
			goto err_power_free;
		}

		if (!of_property_read_bool(cfg_node, "chnl-i2c-used")) {
			pdev->pwr[ix].i2c_used = false;
		} else {
			pdev->pwr[ix].i2c_used = true;

			if (of_property_read_bool(cfg_node, "chnl-i2c-shared"))
				pdev->pwr[ix].i2c_shared = true;
			else
				pdev->pwr[ix].i2c_shared = false;
		}

		of_node_put(cfg_node);
		of_node_put(dcdc_node);
	}

	of_node_put(node);
	of_node_put(pdev->of_node);

	return 0;

err_power_free:
	if (cfg_node)
		of_node_put(cfg_node);
	if (dcdc_node)
		of_node_put(dcdc_node);
	kfree(pdev->pwr);
	pdev->pwr = NULL;

err_out:
	if (node)
		of_node_put(node);
	of_node_put(pdev->of_node);

	return ret;
}

static int sprd_cluster_detail_parse(struct dvfs_cluster *clu)
{
	struct cpudvfs_device *pdev = (struct cpudvfs_device *)clu->parent;
	struct cpudvfs_freq_manager *manager = pdev->priv->freq_manager;
	char *clu_name;
	struct device_node *node;
	struct dvfs_index_tbl *index_tbl;
	u32 dcdc_cell;
	int ret;

	if (clu->is_host)
		clu_name = "host-cluster-cells";
	else
		clu_name = "slave-cluster-cells";

	node = of_parse_phandle(pdev->of_node, clu_name, clu->id);
	if (!node) {
		dev_err(pdev->dev, "%s[%d] node is not defined in dts\n",
			clu_name, clu->id);
		ret =  -EINVAL;
		goto parent_node_put;
	}

	if (!of_property_read_u32(node, "belong-dcdc-cell", &dcdc_cell)) {
		clu->dcdc = dcdc_cell;
	} else {
		dev_err(pdev->dev, "no belong-dcdc-cell found\n");
		ret = -EINVAL;
		goto child_node_put;
	}

	clu->of_node = node;

	if (clu->is_host)
		index_tbl = &manager->host_cluster_index_tbl[clu->id];
	else
		index_tbl = &manager->slave_cluster_index_tbl[clu->id];

	clu->tbl_row_num = index_tbl->tbl_row_num;
	clu->tbl_column_num = index_tbl->tbl_column_num;

	ret = clu->tbl_row_num;

child_node_put:
	of_node_put(node);
parent_node_put:
	of_node_put(pdev->of_node);

	return ret;
}

static void sprd_cluster_plat_opp_release(struct cpudvfs_device *pdev)
{
	struct dvfs_cluster *clu, **clu_array;
	struct plat_opp *opp;
	int kind = 2, clu_num, clu_id;

	clu = pdev->phost_cluster;
	clu_num = pdev->host_cluster_num;
	clu = pdev->phost_cluster;
	clu_array = pdev->hcluster;

	while (kind--) {
		for (clu_id = 0; clu_id < clu_num; ++clu_id) {
			opp = clu[clu_id].freqvolt;
			if (opp) {
				kfree(opp);
				clu[clu_id].freqvolt = NULL;
				clu_array[clu_id] = NULL;
			}
		}

		clu = pdev->pslave_cluster;
		clu_num = pdev->slave_cluster_num;
		clu_array = pdev->scluster;
	}
}

static int sprd_cluster_dt_parse(struct cpudvfs_device *pdev)
{
	struct cluster_info {
		const char *name;
		u32 num;
		struct dvfs_cluster *pcluster;
		bool is_host;
		struct dvfs_cluster **ppcluster;
	};
	int ret, i;

	struct cluster_info info_array[2] = {
		{
			"host-cluster-cells",
			pdev->host_cluster_num,
			pdev->phost_cluster,
			true,
			pdev->hcluster,
		},
		{
			"slave-cluster-cells",
			pdev->slave_cluster_num,
			pdev->pslave_cluster,
			false,
			pdev->scluster,
		},
	};

	for (i = 0; i < 2; i++) {
		const char *name = info_array[i].name;
		u32 clu_num = info_array[i].num;
		u32 dt_clu_num, id;
		struct dvfs_cluster *cluster;
		struct plat_opp *opp;
		int row_num;

		if (!of_find_property(pdev->of_node, name, &dt_clu_num)) {
			dev_err(pdev->dev, "no %s defined in dts\n", name);
			ret = -EINVAL;
			goto err_opp_free;
		}

		dt_clu_num = dt_clu_num / sizeof(u32);
		if (dt_clu_num != clu_num) {
			dev_err(pdev->dev, "the number of %s defined in dts "
				"is incorrect\n", name);
			ret = -EINVAL;
			goto err_opp_free;
		}

		cluster = info_array[i].pcluster;
		for (id = 0; id < clu_num; ++id) {
			cluster[id].is_host = info_array[i].is_host;
			cluster[id].id = id;
			cluster[id].parent = pdev;

			row_num = sprd_cluster_detail_parse(&cluster[id]);
			if (row_num < 0) {
				dev_err(pdev->dev, "fail to parse %s\n", name);
				ret = row_num;
				goto err_opp_free;
			}

			opp = kcalloc(row_num, sizeof(*opp), GFP_KERNEL);
			if (!opp) {
				ret = -ENOMEM;
				goto err_opp_free;
			}

			cluster[id].freqvolt = opp;
			info_array[i].ppcluster[id] = &cluster[id];
		}
		of_node_put(pdev->of_node);
	}

	return 0;

err_opp_free:
	sprd_cluster_plat_opp_release(pdev);
	of_node_put(pdev->of_node);

	return ret;
}

static int sprd_dvfs_device_dt_parse(struct cpudvfs_device *pdev)
{
	int ret;

	ret = sprd_cluster_dt_parse(pdev);
	if (ret)
		return ret;

	ret = sprd_mpll_dt_parse(pdev);
	if (ret)
		return ret;

	ret = sprd_dcdc_dt_parse(pdev);
	if (ret)
		return ret;

	dev_info(pdev->dev, "finish to parse the cpu hardware dvfs device\n");

	return 0;
}

static int sprd_cpudvfs_probe(struct platform_device *pdev)
{
	const struct dvfs_private_data *pdata;
	struct cpudvfs_device *parchdev;
	struct device_node *np;
	struct resource	*res;
	void __iomem *base;
	int ret;

	parchdev = devm_kzalloc(&pdev->dev,
				sizeof(struct cpudvfs_device), GFP_KERNEL);
	if (!parchdev)
		return -ENOMEM;

	np = pdev->dev.of_node;
	if (!np) {
		dev_err(&pdev->dev, "have not found device node!\n");
		ret = -ENODEV;
		goto err_out;
	}

	parchdev->of_node = np;

	pdata = of_device_get_match_data(&pdev->dev);
	if (!pdata || !pdata->volt_manager || !pdata->freq_manager ||
	    !pdata->mpll_manager) {
		dev_err(&pdev->dev, "no matched private driver data found\n");
		ret = -EINVAL;
		goto err_out;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (!base) {
		dev_err(&pdev->dev, "failed to remap the top dvfs register\n");
		ret = -ENOMEM;
		goto err_out;
	}

	parchdev->membase = base;
	parchdev->dev = &pdev->dev;
	parchdev->phy_ops = &sprd_cpudvfs_phy_ops;
	parchdev->phost_cluster = global_host_cluster;
	parchdev->pslave_cluster = global_slave_cluster;

	parchdev->priv = pdata;
	parchdev->host_cluster_num =
		parchdev->priv->freq_manager->host_cluster_num;
	parchdev->slave_cluster_num =
		parchdev->priv->freq_manager->slave_cluster_num;
	parchdev->dvfs_device_num =
		parchdev->priv->freq_manager->dvfs_device_num;
	parchdev->pmic_type_sum = MAX_PMIC_TYPE_NUM;

	ret = sprd_dvfs_device_dt_parse(parchdev);
	if (ret)
		goto err_mem_unmap;

	platform_set_drvdata(pdev, parchdev);

	ret = sprd_dvfs_device_init(parchdev);
	if (ret) {
		dev_err(&pdev->dev,
			"failed to initialize the cpu hw dvfs device\n");
		return ret;
	}

	dev_info(&pdev->dev, "finish to probe the cpu hardware dvfs device\n");

	return 0;

err_mem_unmap:
	devm_iounmap(&pdev->dev, parchdev->membase);
	parchdev->membase = NULL;

err_out:
	return ret;
}

static int sprd_cpudvfs_remove(struct platform_device *pdev)
{
	int ix;
	struct cpudvfs_device *parch = platform_get_drvdata(pdev);

	for (ix = 0; ix < parch->dcdc_num; ix++) {
		if (parch->pwr[ix].i2c_used && parch->pwr[ix].i2c_client &&
		    parch->pwr[ix].i2c_shared)
			i2c_del_driver(&sprd_cpudvfs_i2c_driver[ix]);
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sprd_cpudvfs_resume(struct device *dev)
{
	struct cpudvfs_device *pdev = dev_get_drvdata(dev);
	int ret, ix;

	for (ix = 0; ix < pdev->dvfs_device_num; ++ix) {
		ret = sprd_dvfs_idle_disable(pdev, ix);
		if (ret) {
			dev_err(pdev->dev, "failed to set dvfs idle\n");
			return ret;
		}
	}

	return 0;
}
#endif

static const struct dev_pm_ops sprd_cpudvfs_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(NULL, sprd_cpudvfs_resume)
};

static struct platform_driver sprd_cpudvfs_driver = {
	.probe = sprd_cpudvfs_probe,
	.remove = sprd_cpudvfs_remove,
	.driver = {
		.name = "sprd,hwdvfs-drv",
		.pm = &sprd_cpudvfs_pm_ops,
		.of_match_table = sprd_cpudvfs_of_match,
	},
};

static int __init sprd_cpudvfs_init(void)
{
	return platform_driver_register(&sprd_cpudvfs_driver);
}

static void __exit sprd_cpudvfs_exit(void)
{
	platform_driver_unregister(&sprd_cpudvfs_driver);
}

device_initcall(sprd_cpudvfs_init);
module_exit(sprd_cpudvfs_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jack Liu<Jack.Liu@unisoc.com>");
MODULE_DESCRIPTION("sprd hardware dvfs driver");
