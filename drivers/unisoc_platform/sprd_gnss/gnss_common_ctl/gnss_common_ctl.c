/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/bug.h>
#include <linux/delay.h>
#include <misc/marlin_platform.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <misc/wcn_bus.h>

#include "gnss_common.h"

#define GNSS_DATA_BASE_TYPE_H  16
#define GNSS_MAX_STRING_LEN	10
#define GNSS_DUMP_DATA_START_UP 1
#define FIRMWARE_FILEPATHNAME_LENGTH_MAX 256

struct gnss_common_ctl {
	struct device *dev;
	unsigned long chip_ver;
	unsigned int gnss_status;
	unsigned int gnss_subsys;
	char firmware_path[FIRMWARE_FILEPATHNAME_LENGTH_MAX];
};

static struct gnss_common_ctl gnss_common_ctl_dev;

enum gnss_status_e {
	GNSS_STATUS_POWEROFF = 0,
	GNSS_STATUS_POWERON,
	GNSS_STATUS_ASSERT,
	GNSS_STATUS_POWEROFF_GOING,
	GNSS_STATUS_POWERON_GOING,
	GNSS_STATUS_MAX,
};
enum gnss_cp_status_subtype {
	GNSS_CP_STATUS_CALI = 1,
	GNSS_CP_STATUS_INIT = 2,
	GNSS_CP_STATUS_INIT_DONE = 3,
	GNSS_CP_STATUS_IDLEOFF = 4,
	GNSS_CP_STATUS_IDLEON = 5,
	GNSS_CP_STATUS_SLEEP = 6,
	GNSS_CP_STATUS__MAX,
};

static unsigned int gnssver = 0x22;
static const struct of_device_id gnss_common_ctl_of_match[] = {
	{.compatible = "sprd,gnss", .data = (void *)&gnssver},
	{},
};

static int isM3lite(void)
{
	int iRet = FALSE;

#ifdef CONFIG_UMW2652
	iRet = TRUE;
#endif
	return iRet;
}

static int isQogirl6(void)
{
	int iRet = FALSE;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
		iRet = TRUE;

	return iRet;
}

static void gnss_tsen_enable(int type)
{
	struct platform_device *pdev_regmap;
	struct device_node *regmap_np = NULL;
	static struct regmap *regmap;
	static unsigned int base;
	int ret;
	unsigned int value, temp;
	struct device *dev = gnss_common_ctl_dev.dev;

	dev_err(dev, "%s M3l=[%d],L6=[%d]\n", __func__, isM3lite(), isQogirl6());

	if ((isM3lite() == FALSE) && (isQogirl6() == FALSE)) {
		dev_err(dev, "%s not M3lite or L6\n", __func__);
		return;
	}

	if (base == 0) {
		regmap_np = of_find_compatible_node(NULL, NULL,
						    "sprd,sc27xx-syscon");
		if (!regmap_np) {
			dev_err(dev, "%s error np\n", __func__);
			return;
		}
		pdev_regmap = of_find_device_by_node(regmap_np);
		if (!pdev_regmap) {
			dev_err(dev, "%s error regmap\n", __func__);
			of_node_put(regmap_np);
			return;
		}
		regmap = dev_get_regmap(pdev_regmap->dev.parent, NULL);
		ret = of_property_read_u32_index(regmap_np, "reg", 0, &base);
		if (ret) {
			dev_err(dev, "%s error base\n", __func__);
			of_node_put(regmap_np);
		}
		of_node_put(regmap_np);
	}
	dev_err(dev, "%s base 0x%x\n", __func__, base);
	regmap_read(regmap, (REGS_ANA_APB_BASE + XTL_WAIT_CTRL0), &value);
	dev_err(dev, "%s XTL_WAIT_CTRL0 value read 0x%x\n", __func__, value);
	temp = value | BIT_XTL_EN;
	regmap_write(regmap, (REGS_ANA_APB_BASE + XTL_WAIT_CTRL0), temp);
	regmap_read(regmap, (REGS_ANA_APB_BASE + XTL_WAIT_CTRL0), &value);
	dev_err(dev, "%s 2nd read 0x%x\n", __func__, value);

	regmap_read(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL0), &value);
	dev_err(dev, "%s TSEN_CTRL0 value read 0x%x\n", __func__, value);
	temp = value | BIT_TSEN_CLK_SRC_SEL | BIT_TSEN_ADCLDO_EN;
	regmap_write(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL0), temp);
	regmap_read(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL0), &value);
	dev_err(dev, "%s 2nd read 0x%x\n", __func__, value);

	regmap_read(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL1), &value);
	dev_err(dev, "%s TSEN_CTRL1 value read 0x%x\n", __func__, value);
	temp = value | BIT_TSEN_SDADC_EN | BIT_TSEN_CLK_EN | BIT_TSEN_UGBUF_EN;
	regmap_write(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL1), temp);
	regmap_read(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL1), &value);
	dev_err(dev, "%s 2nd read 0x%x\n", __func__, value);

	regmap_read(regmap, (SC2730_PIN_REG_BASE + PTEST0), &value);
	dev_err(dev, "%s PTEST0 value read 0x%x\n", __func__, value);
	temp = (value & (~PTEST0_MASK)) | PTEST0_sel(2);
	regmap_write(regmap, (SC2730_PIN_REG_BASE + PTEST0), temp);
	regmap_read(regmap, (SC2730_PIN_REG_BASE + PTEST0), &value);
	dev_err(dev, "%s 2nd read 0x%x\n", __func__, value);

	regmap_read(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL3), &value);
	dev_err(dev, "%s TSEN_CTRL3 value read 0x%x\n", __func__, value);
	temp = (value | BIT_TSEN_EN) & (~BIT_TSEN_SEL_EN);
	temp = (temp & (~BIT_TSEN_TIME_SEL_MASK)) | BIT_TSEN_TIME_sel(2);
	regmap_write(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL3), temp);
	regmap_read(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL3), &value);
	dev_err(dev, "%s 2nd read 0x%x\n", __func__, value);

	if (type == TSEN_EXT)
		regmap_read(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL4), &value);
	else
		regmap_read(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL5), &value);
	dev_err(dev, "%s 0x%x read 0x%x\n", __func__, TSEN_CTRL4, value);

	of_node_put(regmap_np);
}

static void gnss_tsen_disable(int type)
{
	struct platform_device *pdev_regmap;
	struct device_node *regmap_np = NULL;
	static struct regmap *regmap;
	static unsigned int base;
	int ret;
	unsigned int value, temp;
	struct device *dev = gnss_common_ctl_dev.dev;

	dev_err(dev, "%s M3l=[%d],L6=[%d]\n", __func__, isM3lite(), isQogirl6());

	if ((isM3lite() == FALSE) && (isQogirl6() == FALSE)) {
		dev_err(dev, "%s not M3lite or L6\n", __func__);
		return;
	}
	if (base == 0) {
		regmap_np = of_find_compatible_node(NULL, NULL,
						    "sprd,sc27xx-syscon");
		if (!regmap_np) {
			dev_err(dev, "%s error np\n", __func__);
			return;
		}
		pdev_regmap = of_find_device_by_node(regmap_np);
		if (!pdev_regmap) {
			dev_err(dev, "%s error regmap\n", __func__);
			of_node_put(regmap_np);
			return;
		}
		regmap = dev_get_regmap(pdev_regmap->dev.parent, NULL);
		ret = of_property_read_u32_index(regmap_np, "reg", 0, &base);
		if (ret) {
			dev_err(dev, "%s error base\n", __func__);
			of_node_put(regmap_np);
		}
		of_node_put(regmap_np);
	}
	dev_err(dev, "%s base 0x%x\n", __func__, base);

	regmap_read(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL0), &value);
	dev_err(dev, "%s TSEN_CTRL0 value read 0x%x\n", __func__, value);
	temp = BIT_TSEN_CLK_SRC_SEL | BIT_TSEN_ADCLDO_EN;
	temp = value & (~temp);
	regmap_write(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL0), temp);
	regmap_read(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL0), &value);
	dev_err(dev, "%s 2nd read 0x%x\n", __func__, value);

	regmap_read(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL1), &value);
	dev_err(dev, "%s TSEN_CTRL1 value read 0x%x\n", __func__, value);
	temp = BIT_TSEN_SDADC_EN | BIT_TSEN_CLK_EN | BIT_TSEN_UGBUF_EN;
	temp = value & (~temp);
	regmap_write(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL1), temp);
	regmap_read(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL1), &value);
	dev_err(dev, "%s 2nd read 0x%x\n", __func__, value);

	regmap_read(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL3), &value);
	dev_err(dev, "%s TSEN_CTRL3 value read 0x%x\n", __func__, value);
	temp = (value & (~BIT_TSEN_EN)) | BIT_TSEN_SEL_EN;
	regmap_write(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL3), temp);
	regmap_read(regmap, (REGS_ANA_APB_BASE + TSEN_CTRL3), &value);
	dev_err(dev, "%s 2nd read 0x%x\n", __func__, value);

	of_node_put(regmap_np);
}

static void gnss_tcxo_enable(void)
{
	u32 val_buf = 0;
	u32 val_ctl = 0;
	struct device *dev = gnss_common_ctl_dev.dev;

	dev_err(dev, "%s M3l=[%d],L6=[%d]\n", __func__, isM3lite(), isQogirl6());

	if ((isM3lite() == FALSE) && (isQogirl6() == FALSE)) {
		dev_err(dev, "%s not M3lite or L6\n", __func__);
		return;
	}

	wcn_read_data_from_phy_addr(XTLBUF3_REL_CFG_ADDR, (void *)&val_buf, 4);
	wcn_read_data_from_phy_addr(WCN_XTL_CTRL_ADDR, (void *)&val_ctl, 4);
	dev_info(dev, "tcxo_en 1read buf=%x ctl=%x\n", val_buf, val_ctl);

	/* XTLBUF3_REL_CF bit[1:0]=01 */
	val_buf &= ~(0x1 << 1);
	val_buf |=  (0x1 << 0);
	/* WCN_XTL_CTRL bit[2:1]=01 */
	val_ctl &= ~(0x1 << 2);
	val_ctl |=  (0x1 << 1);
	dev_info(dev, "tcxo_en write buf=%x ctl=%x\n", val_buf, val_ctl);

	wcn_write_data_to_phy_addr(XTLBUF3_REL_CFG_ADDR, (void *)&val_buf, 4);
	wcn_write_data_to_phy_addr(WCN_XTL_CTRL_ADDR, (void *)&val_ctl, 4);

	wcn_read_data_from_phy_addr(XTLBUF3_REL_CFG_ADDR, (void *)&val_buf, 4);
	wcn_read_data_from_phy_addr(WCN_XTL_CTRL_ADDR, (void *)&val_ctl, 4);
	dev_info(dev, "tcxo_en 2read buf=%x ctl=%x\n", val_buf, val_ctl);
}
static void gnss_tcxo_disable(void)
{
	u32 val_buf = 0;
	u32 val_ctl = 0;
	struct device *dev = gnss_common_ctl_dev.dev;

	dev_err(dev, "%s M3l=[%d],L6=[%d]\n", __func__, isM3lite(), isQogirl6());

	if ((isM3lite() == FALSE) && (isQogirl6() == FALSE)) {
		dev_err(dev, "%s not M3lite or L6\n", __func__);
		return;
	}

	wcn_read_data_from_phy_addr(XTLBUF3_REL_CFG_ADDR, (void *)&val_buf, 4);
	wcn_read_data_from_phy_addr(WCN_XTL_CTRL_ADDR, (void *)&val_ctl, 4);
	dev_info(dev, "tcxo_dis 1read buf=%x ctl=%x\n", val_buf, val_ctl);

	/* bit[1:0] */
	val_buf &= ~(0x1 << 1);
	val_buf &= ~(0x1 << 0);
	/* bit[2:1] */
	val_ctl &= ~(0x1 << 2);
	val_ctl &= ~(0x1 << 1);
	dev_info(dev, "tcxo_dis write buf=%x ctl=%x\n", val_buf, val_ctl);

	wcn_write_data_to_phy_addr(XTLBUF3_REL_CFG_ADDR, (void *)&val_buf, 4);
	wcn_write_data_to_phy_addr(WCN_XTL_CTRL_ADDR, (void *)&val_ctl, 4);

	wcn_read_data_from_phy_addr(XTLBUF3_REL_CFG_ADDR, (void *)&val_buf, 4);
	wcn_read_data_from_phy_addr(WCN_XTL_CTRL_ADDR, (void *)&val_ctl, 4);
	dev_info(dev, "tcxo_dis 2read buf=%x ctl=%x\n", val_buf, val_ctl);
}

static void gnss_power_on(bool enable)
{
	int ret;
	struct device *dev = gnss_common_ctl_dev.dev;

	dev_info(dev, "%s en=%d,status=%d v3.2\n", __func__,
		 enable, gnss_common_ctl_dev.gnss_status);

	dev_info(dev, "%s clktp=%d\n", __func__, wcn_get_xtal_26m_clk_type());

	if (enable && gnss_common_ctl_dev.gnss_status == GNSS_STATUS_POWEROFF) {
		gnss_common_ctl_dev.gnss_status = GNSS_STATUS_POWERON_GOING;

		/* only marlin3lite and qogirl6 need */
		if (wcn_get_xtal_26m_clk_type() == WCN_CLOCK_TYPE_TSX)
			gnss_tsen_enable(TSEN_EXT);
		else if (wcn_get_xtal_26m_clk_type() == WCN_CLOCK_TYPE_TCXO)
			gnss_tcxo_enable();
		else
			dev_info(dev, "%s: unknown clk_type\n", __func__);

		ret = start_marlin(gnss_common_ctl_dev.gnss_subsys);
		if (ret != 0)
			dev_err(dev, "%s start marlin failed ret=%d\n",
				__func__, ret);
		else {
			gnss_common_ctl_dev.gnss_status = GNSS_STATUS_POWERON;
		}
	} else if (!enable && gnss_common_ctl_dev.gnss_status
			== GNSS_STATUS_POWERON) {
		gnss_common_ctl_dev.gnss_status = GNSS_STATUS_POWEROFF_GOING;

		/* only marlin3lite and qogirl6 need */
		if (wcn_get_xtal_26m_clk_type() == WCN_CLOCK_TYPE_TSX)
			gnss_tsen_disable(TSEN_EXT);
		else if (wcn_get_xtal_26m_clk_type() == WCN_CLOCK_TYPE_TCXO)
			gnss_tcxo_disable();
		else
			dev_info(dev, "%s: unknown clk_type\n", __func__);

		ret = stop_marlin(gnss_common_ctl_dev.gnss_subsys);
		if (ret != 0)
			dev_err(dev, "%s stop marlin failed ret=%d\n",
				__func__, ret);
		else
			gnss_common_ctl_dev.gnss_status = GNSS_STATUS_POWEROFF;
	} else {
		dev_err(dev, "%s status is not match\n", __func__);
	}
}

static ssize_t gnss_power_enable_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned long set_value;
	ssize_t ret = count;

	if (kstrtoul(buf, GNSS_MAX_STRING_LEN, &set_value)) {
		dev_err(dev, "%s Maybe store string is too long\n", __func__);
		return -EINVAL;
	}
	dev_info(dev, "%s set_value[%lu]\n", __func__, set_value);
	if (set_value == 1)
		gnss_power_on(1);
	else if (set_value == 0)
		gnss_power_on(0);
	else {
		ret = -EINVAL;
		dev_info(dev, "%s unknown control\n", __func__);
	}

	return ret;
}

static DEVICE_ATTR_WO(gnss_power_enable);

static ssize_t gnss_subsys_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned long set_value;
	ssize_t ret = count;

	if (kstrtoul(buf, GNSS_MAX_STRING_LEN, &set_value))
		return -EINVAL;

	dev_info(dev, "%s setVal=%lu\n", __func__, set_value);
#ifndef CONFIG_WCN_INTEG
	gnss_common_ctl_dev.gnss_subsys = MARLIN_GNSS;
#else
	if (set_value == WCN_GNSS)
		gnss_common_ctl_dev.gnss_subsys = WCN_GNSS;
	else if (set_value == WCN_GNSS_BD)
		gnss_common_ctl_dev.gnss_subsys  = WCN_GNSS_BD;
	else
		ret = -EINVAL;
#endif
	return ret;
}

static ssize_t gnss_subsys_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int i = 0;

	dev_info(dev, "%s\n", __func__);
	if (gnss_common_ctl_dev.gnss_status == GNSS_STATUS_POWERON) {
		memset(gnss_common_ctl_dev.firmware_path, 0x0,
		       FIRMWARE_FILEPATHNAME_LENGTH_MAX);
		strcpy(&gnss_common_ctl_dev.firmware_path[0],
		       gnss_firmware_path_get());

		dev_info(dev, "%s fpath=[%s]\n", __func__,
			 gnss_common_ctl_dev.firmware_path);

		i += scnprintf(buf + i, PAGE_SIZE - i, "%d:%s\n",
				gnss_common_ctl_dev.gnss_subsys,
				&gnss_common_ctl_dev.firmware_path[0]);
	} else {
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				gnss_common_ctl_dev.gnss_subsys);
	}

	return i;
}

static DEVICE_ATTR_RW(gnss_subsys);

#ifdef CONFIG_WCN_INTEG
static int gnss_status_get(void)
{
	phys_addr_t phy_addr;
	phys_addr_t addr_offset;
	u32 magic_value = 0;
	struct device *dev = gnss_common_ctl_dev.dev;

	if (isQogirl6() == TRUE)
		addr_offset = GNSS_QOGIRL6_STATUS_OFFSET;
	else
		addr_offset = GNSS_STATUS_OFFSET;

	phy_addr = wcn_get_gnss_base_addr() + addr_offset;

	wcn_read_data_from_phy_addr(phy_addr, &magic_value, sizeof(u32));
	dev_info(dev, "%s magic_value=%d\n", __func__, magic_value);

	return magic_value;
}

static void gnss_dump_mem_ctrl_co(char *trigStr)
{
	char flag = 0; /* 0: default, all, 1: only data, pmu, aon */
	unsigned int temp_status = 0;
	static char dump_flag;
	char triggerStr[64];
	struct device *dev = gnss_common_ctl_dev.dev;

	memset(triggerStr, 0, 64);
	strcpy(triggerStr, trigStr);

	dev_info(dev, "%s flag[%d],str[%s]\n", __func__, dump_flag, triggerStr);

	if (dump_flag == 1)
		return;

	dump_flag = 1;

	temp_status = gnss_common_ctl_dev.gnss_status;
	if ((temp_status == GNSS_STATUS_POWERON_GOING) ||
		((temp_status == GNSS_STATUS_POWERON) &&
		(gnss_status_get() != GNSS_CP_STATUS_SLEEP))) {
		flag = (temp_status == GNSS_STATUS_POWERON) ? 0 : 1;
		wcn_assert_interface(WCN_SOURCE_GNSS, triggerStr);
		gnss_common_ctl_dev.gnss_status = GNSS_STATUS_ASSERT;
	}
}
#else
static void gnss_dump_mem_ctrl(char *trigStr)
{
	static char dump_flag;
	char triggerStr[64];
	struct device *dev = gnss_common_ctl_dev.dev;

	memset(triggerStr, 0, 64);
	strcpy(triggerStr, trigStr);

	dev_info(dev, "%s flag[%d],str[%s]\n", __func__, dump_flag, triggerStr);

	if (dump_flag == 1)
		return;

	dump_flag = 1;

	if (gnss_common_ctl_dev.gnss_status == GNSS_STATUS_POWERON) {
		wcn_assert_interface(WCN_SOURCE_GNSS, triggerStr);
		gnss_common_ctl_dev.gnss_status = GNSS_STATUS_ASSERT;
	}

}
#endif
static ssize_t gnss_dump_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned long set_value;
	int ret = -1;
	int strlen = 0;
	char triggerStr[64];

	set_value = buf[0] - '0';

	dev_info(dev, "%s V2.0 val[%lu]\n", __func__, set_value);

	memset(triggerStr, 0, 64);
	strlen = ((count - 2) > 63) ? 63 : (count - 2);
	memcpy(triggerStr, &buf[2], strlen);

	dev_info(dev, "%s trigStr=%s\n", __func__, triggerStr);

	if (set_value == 1) {
#ifdef CONFIG_WCN_INTEG
		gnss_dump_mem_ctrl_co(triggerStr);
#else
		gnss_dump_mem_ctrl(triggerStr);
#endif
		ret = GNSS_DUMP_DATA_START_UP;
	} else
		ret = -EINVAL;

	return ret;
}

static DEVICE_ATTR_WO(gnss_dump);

static ssize_t gnss_status_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int i = 0;
	unsigned int status = gnss_common_ctl_dev.gnss_status;

	dev_info(dev, "%s status[%d]\n", __func__, status);
	i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", status);

	return i;
}
static DEVICE_ATTR_RO(gnss_status);

static ssize_t gnss_clktype_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int i = 0;
	enum wcn_clock_type clktype = WCN_CLOCK_TYPE_UNKNOWN;

	clktype = wcn_get_xtal_26m_clk_type();
	dev_info(dev, "%s clktyp[%d]\n", __func__, clktype);
	i = scnprintf(buf, PAGE_SIZE, "%d\n", clktype);

	return i;
}
static DEVICE_ATTR_RO(gnss_clktype);

bool gnss_delay_ctl(void)
{
	return (gnss_common_ctl_dev.gnss_status == GNSS_STATUS_POWERON);
}
EXPORT_SYMBOL_GPL(gnss_delay_ctl);

static struct attribute *gnss_common_ctl_attrs[] = {
	&dev_attr_gnss_power_enable.attr,
	&dev_attr_gnss_dump.attr,
	&dev_attr_gnss_status.attr,
	&dev_attr_gnss_subsys.attr,
	&dev_attr_gnss_clktype.attr,
	NULL,
};

static struct attribute_group gnss_common_ctl_group = {
	.name = NULL,
	.attrs = gnss_common_ctl_attrs,
};

static struct miscdevice gnss_common_ctl_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gnss_common_ctl",
	.fops = NULL,
};

static int gnss_common_ctl_probe(struct platform_device *pdev)
{
	int ret;
	const struct of_device_id *of_id;

	dev_info(&pdev->dev, "%s V3.2 entry\n", __func__);

	gnss_common_ctl_dev.dev = &pdev->dev;
	gnss_common_ctl_dev.gnss_status = GNSS_STATUS_POWEROFF;
	memset(gnss_common_ctl_dev.firmware_path, 0x0,
	       FIRMWARE_FILEPATHNAME_LENGTH_MAX);
#ifndef CONFIG_WCN_INTEG
	gnss_common_ctl_dev.gnss_subsys = MARLIN_GNSS;
#else
	gnss_common_ctl_dev.gnss_subsys = WCN_GNSS;
#endif
	/* considering backward compatibility, it's not use now  start */
	of_id = of_match_node(gnss_common_ctl_of_match,
		pdev->dev.of_node);
	if (!of_id) {
		dev_err(&pdev->dev,
			"get gnss_common_ctl of device id failed!\n");
		return -ENODEV;
	}
	gnss_common_ctl_dev.chip_ver = (unsigned long)(of_id->data);
	/* considering backward compatibility, it's not use now  end */

	platform_set_drvdata(pdev, &gnss_common_ctl_dev);
	ret = misc_register(&gnss_common_ctl_miscdev);
	if (ret) {
		dev_err(&pdev->dev, "%s failed to register\n",
			__func__);
		return ret;
	}

	ret = sysfs_create_group(&gnss_common_ctl_miscdev.this_device->kobj,
			&gnss_common_ctl_group);
	if (ret) {
		dev_err(&pdev->dev, "%s failed to create attributes\n",
			__func__);
		goto err_attr_failed;
	}

#ifdef GNSS_SINGLE_MODULE
	dev_err(&pdev->dev, "%s single ko\n", __func__);

	ret = gnss_pnt_ctl_init();
	if (ret != 0)
		dev_err(&pdev->dev, "%s gnss_pnt_ctl_init[%d]\n",
			__func__, ret);

	ret = gnss_gdb_init();
	if (ret != 0)
		dev_err(&pdev->dev, "%s gnss_gdb_init[%d]\n",
			__func__, ret);
#else
	dev_err(&pdev->dev, "%s multiple  ko\n", __func__);
#endif

	return 0;

err_attr_failed:
	misc_deregister(&gnss_common_ctl_miscdev);
	return ret;
}

static int gnss_common_ctl_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&gnss_common_ctl_miscdev.this_device->kobj,
				&gnss_common_ctl_group);

	misc_deregister(&gnss_common_ctl_miscdev);
#ifdef GNSS_SINGLE_MODULE
	gnss_pnt_ctl_cleanup();
	gnss_gdb_exit();
#endif
	return 0;
}
static struct platform_driver gnss_common_ctl_drv = {
	.driver = {
		   .name = "gnss_common_ctl",
		   .of_match_table = of_match_ptr(gnss_common_ctl_of_match),
		   },
	.probe = gnss_common_ctl_probe,
	.remove = gnss_common_ctl_remove
};

static int __init gnss_common_ctl_init(void)
{
	return platform_driver_register(&gnss_common_ctl_drv);
}

static void __exit gnss_common_ctl_exit(void)
{
	platform_driver_unregister(&gnss_common_ctl_drv);
}

module_init(gnss_common_ctl_init);
module_exit(gnss_common_ctl_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum Gnss Driver");
MODULE_AUTHOR("Jun.an<jun.an@spreadtrum.com>");
