/*
 * sipa  driver debugfs support
 *
 * Copyright (C) 2018-2019 Unisoc Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include<linux/pm_runtime.h>
#include<linux/regmap.h>

#include "sipa_debug.h"

static int sipa_regdump_show(struct seq_file *s, void *unused)
{
	struct sipa_plat_drv_cfg *sipa = &s_sipa_cfg;
	struct sipa_hal_context *hal_cfg = &sipa_hal_ctx;
	void __iomem *glbbase = hal_cfg->phy_virt_res.glb_base;
	const struct sipa_register_data *sipa_regmap = sipa->debugfs_data;
	unsigned int i, val;
	int ret;

	seq_puts(s, "Sipa Ahb Register\n");
	ret = pm_runtime_get_sync(sipa->dev);
	if (ret < 0)
		return ret;

	for (i = 0; i < MAX_REG; i++) {
		switch (sipa_regmap->ahb_reg[i].size) {
		case 8:
			regmap_read(sipa->sys_regmap,
				    sipa_regmap->ahb_reg[i].offset, &val);
			seq_printf(s, "%-12s: %02x\n",
				   sipa_regmap->ahb_reg[i].name, val);
			break;
		case 16:
			regmap_read(sipa->sys_regmap,
				    sipa_regmap->ahb_reg[i].offset, &val);
			seq_printf(s, "%-12s: %02x\n",
				   sipa_regmap->ahb_reg[i].name, val);
			break;
		case 32:
			regmap_read(sipa->sys_regmap,
				    sipa_regmap->ahb_reg[i].offset, &val);
			seq_printf(s, "%-12s:    0x%02x\n",
				   sipa_regmap->ahb_reg[i].name, val);
			break;
		}
	}
	seq_puts(s,
		 "****************Sipa Glb Register ***********************\n");

	for (i = 0; i < ARRAY_SIZE(sipa_glb_regmap); i++) {
		val = readl_relaxed(glbbase + sipa_glb_regmap[i].offset);
		seq_printf(s, "%-12s:	0x%02x\n", sipa_glb_regmap[i].name,
			   val);
	}

	pm_runtime_mark_last_busy(sipa->dev);
	pm_runtime_put_autosuspend(sipa->dev);
	return 0;
}

static int sipa_regdump_open(struct inode *inode, struct file *file)
{
	return single_open(file, sipa_regdump_show, inode->i_private);
}

static const struct file_operations sipa_regdump_fops = {
	.open = sipa_regdump_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int sipa_commonfifo_show(struct seq_file *s, void *unused)
{
	struct sipa_plat_drv_cfg *sipa = &s_sipa_cfg;
	struct sipa_hal_context *hal_cfg = &sipa_hal_ctx;
	void __iomem *glbbase = hal_cfg->phy_virt_res.glb_base;
	unsigned int i, j, val;
	int ret;

	seq_puts(s, "Sipa commonfifo Register\n");
	ret = pm_runtime_get_sync(sipa->dev);
	if (ret < 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(sipa_common_fifo_map); i++) {
		seq_printf(s, "%-12s*******************************\n",
			   sipa_common_fifo_map[i].name);
		for (j = 0; j < ARRAY_SIZE(sipa_fifo_iterm_map); j++) {
			val = readl_relaxed(glbbase +
					    sipa_common_fifo_map[i].offset +
					    sipa_fifo_iterm_map[j].offset);
			seq_printf(s, "%-12s:	0x%02x\n",
				   sipa_fifo_iterm_map[j].name, val);
		}
	}

	pm_runtime_mark_last_busy(sipa->dev);
	pm_runtime_put_autosuspend(sipa->dev);
	return 0;
}

static int sipa_commonfifo_open(struct inode *inode, struct file *file)
{
	return single_open(file, sipa_commonfifo_show, inode->i_private);
}

static const struct file_operations sipa_commonfifo_fops = {
	.open = sipa_commonfifo_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int sipa_init_debugfs(struct sipa_plat_drv_cfg *sipa)
{
	struct dentry *root;
	struct dentry *file;
	int ret;

	root = debugfs_create_dir(dev_name(sipa->dev), NULL);
	if (!root)
		return -ENOMEM;

	file = debugfs_create_file("regdump", 0444, root, sipa,
				   &sipa_regdump_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}

	file = debugfs_create_file("commonfifo_reg", 0444, root, sipa,
				   &sipa_commonfifo_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}

	sipa->debugfs_root = root;

	return 0;

err1:
	debugfs_remove_recursive(root);

	return ret;
}

void sipa_exit_debugfs(struct sipa_plat_drv_cfg *sipa)
{
	debugfs_remove_recursive(sipa->debugfs_root);
}
