/*copyright (C) 2019 Spreadtrum Communications Inc.
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
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/math64.h>

#define DMC_PHY_PROC_NAME "sprd_dmc_phy"
#define RELOCK_INFO_NAME "relock_info"
#define INVALID_RES_IDX  0xffffffff

#define RELOCK_TIMES_OFFSET	0x4
#define RELOCK_CNT_ALL_OFFSET	0xc
#define RELOCK_CNT_MAX_OFFSET	0x1c
#define RELOCK_CNT_LAST_OFFSET	0x34

static struct resource *relock_res;
static struct proc_dir_entry *phy_dir;
static struct proc_dir_entry *relock_info;
static struct platform_device *phy_dev;


#ifdef CONFIG_PROC_FS
static int relock_info_show(struct seq_file *m, void *v)
{
	void __iomem *io_addr;
	u32 times, cnt_all, cnt_max, cnt_last;

	io_addr = devm_ioremap_resource(&phy_dev->dev, relock_res);
	times = readl_relaxed(io_addr + RELOCK_TIMES_OFFSET);
	cnt_all = readl_relaxed(io_addr + RELOCK_CNT_ALL_OFFSET);
	cnt_max = readl_relaxed(io_addr + RELOCK_CNT_MAX_OFFSET);
	cnt_last = readl_relaxed(io_addr + RELOCK_CNT_LAST_OFFSET);
	iounmap(io_addr);

	seq_printf(m, "ddr get %u wakeup warning\n", times);
	seq_printf(m, "ddr try %u relocks\n", cnt_all);
	seq_printf(m, "ddr try %u relocks max in once waring\n", cnt_max);
	seq_printf(m, "ddr try %u relocks max in last waring\n", cnt_last);
	return 0;
}

static int relock_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, relock_info_show, PDE_DATA(inode));
}

static const struct file_operations relock_info_fops = {
	.open = relock_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int relock_proc_creat(void)
{
	phy_dir = proc_mkdir(DMC_PHY_PROC_NAME, NULL);
	if (!phy_dir)
		return -ENOMEM;

	relock_info = proc_create_data(RELOCK_INFO_NAME, 0444, phy_dir,
					     &relock_info_fops, NULL);
	if (!relock_info) {
		remove_proc_entry(DMC_PHY_PROC_NAME, NULL);
		return -ENOMEM;
	}
	return 0;
}
#endif

static int sprd_dmc_phy_probe(struct platform_device *pdev)
{
	relock_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!relock_res)
		return -ENODEV;
#ifdef CONFIG_PROC_FS
	relock_proc_creat();
#endif
	phy_dev = pdev;

	return 0;
}

static int sprd_dmc_phy_remove(struct platform_device *pdev)
{
	if (relock_info != NULL)
		remove_proc_entry(RELOCK_INFO_NAME, phy_dir);
	if (phy_dir != NULL)
		remove_proc_entry(DMC_PHY_PROC_NAME, NULL);
	return 0;
}

static const struct of_device_id sprd_dmc_phy_of_match[] = {
	{.compatible = "sprd,sharkl3-dmc-phy"},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_dmc_phy_of_match);

static struct platform_driver sprd_dmc_phy_driver = {
	.probe = sprd_dmc_phy_probe,
	.remove = sprd_dmc_phy_remove,
	.driver = {
		   .name = "sprd-dmc-phy",
		   .of_match_table = sprd_dmc_phy_of_match,
	},
};
module_platform_driver(sprd_dmc_phy_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("junqiang wang<junqiang.wang@spreadtrum.com>");
MODULE_DESCRIPTION("dmc drv for Spreadtrum");
