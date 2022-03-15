// SPDX-License-Identifier: GPL-2.0
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
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/iio/consumer.h>
#include <linux/of.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/usb/phy.h>
#include <linux/usb/otg.h>
#include <dt-bindings/soc/sprd,sharkl3-mask.h>
#include <dt-bindings/soc/sprd,sharkl3-regs.h>
#include <uapi/linux/usb/charger.h>

#define SC2721_CHARGE_STATUS		0xec8
#define BIT_CHG_DET_DONE		BIT(11)
#define BIT_SDP_INT			BIT(7)
#define BIT_DCP_INT			BIT(6)
#define BIT_CDP_INT			BIT(5)

/* Pls keep the same definition as musb_sprd */
#define CHARGER_2NDDETECT_ENABLE	BIT(30)
#define CHARGER_2NDDETECT_SELECT	BIT(31)

struct sprd_hsphy {
	struct device		*dev;
	struct usb_phy		phy;
	void __iomem		*base;
	struct regulator	*vdd;
	struct regmap           *hsphy_glb;
	struct regmap           *apahb;
	struct regmap           *ana_g2;
	struct regmap           *ana_g4;
	struct regmap           *pmic;
	struct wakeup_source	*wake_lock;
	struct work_struct		work;
	unsigned long event;
	u32			vdd_vol;
	atomic_t		reset;
	atomic_t		inited;
	bool			is_host;
	struct iio_channel	*dp;
	struct iio_channel	*dm;
};

#define FULLSPEED_USB33_TUNE		2700000
#define SC2721_CHARGE_DET_FGU_CTRL      0xecc
#define BIT_DP_DM_AUX_EN                BIT(1)
#define BIT_DP_DM_BC_ENB                BIT(0)
#define VOLT_LO_LIMIT                   1150
#define VOLT_HI_LIMIT                   600

static enum usb_charger_type sc27xx_charger_detect(struct regmap *regmap)
{
	enum usb_charger_type type;
	u32 status = 0, val;
	int ret, cnt = 10;

	do {
		ret = regmap_read(regmap, SC2721_CHARGE_STATUS, &val);
		if (ret)
			return UNKNOWN_TYPE;

		if (val & BIT_CHG_DET_DONE) {
			status = val &
				 (BIT_CDP_INT | BIT_DCP_INT | BIT_SDP_INT);
			break;
		}

		msleep(200);
	} while (--cnt > 0);

	switch (status) {
	case BIT_CDP_INT:
		type = CDP_TYPE;
		break;
	case BIT_DCP_INT:
		type = DCP_TYPE;
		break;
	case BIT_SDP_INT:
		type = SDP_TYPE;
		break;
	default:
		type = UNKNOWN_TYPE;
	}

	return type;
}

static void sprd_hsphy_charger_detect_work(struct work_struct *work)
{
	struct sprd_hsphy *phy = container_of(work, struct sprd_hsphy, work);
	struct usb_phy *usb_phy = &phy->phy;

	__pm_stay_awake(phy->wake_lock);
	if (phy->event)
		usb_phy_set_charger_state(usb_phy, USB_CHARGER_PRESENT);
	else
		usb_phy_set_charger_state(usb_phy, USB_CHARGER_ABSENT);
	__pm_relax(phy->wake_lock);
}

static inline void sprd_hsphy_reset_core(struct sprd_hsphy *phy)
{
	u32 msk1, msk2;

	/* Reset PHY */
	msk1 = MASK_AP_AHB_OTG_UTMI_SOFT_RST | MASK_AP_AHB_OTG_SOFT_RST;
	msk2 = MASK_AON_APB_OTG_PHY_SOFT_RST;
	regmap_update_bits(phy->apahb, REG_AP_AHB_AHB_RST,
		msk1, msk1);
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_RST2,
		msk2, msk2);
	usleep_range(20000, 30000);
	regmap_update_bits(phy->apahb, REG_AP_AHB_AHB_RST,
		msk1, 0);
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_RST2,
		msk2, 0);
}

static int sprd_hostphy_set(struct usb_phy *x, int on)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 reg, msk;

	if (on) {
		msk = MASK_ANLG_PHY_G4_ANALOG_USB20_UTMIOTG_IDDG;
		regmap_update_bits(phy->ana_g4,
			REG_ANLG_PHY_G4_ANALOG_USB20_IDDG, msk, 0);

		msk = MASK_ANLG_PHY_G4_DBG_SEL_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_G4_DBG_SEL_ANALOG_USB20_USB20_DPPULLDOWN;
		regmap_update_bits(phy->ana_g4,
			REG_ANLG_PHY_G4_ANALOG_USB20_REG_SEL_CFG_0,
			msk, msk);

		msk = MASK_ANLG_PHY_G4_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_G4_ANALOG_USB20_USB20_DPPULLDOWN;
		regmap_update_bits(phy->ana_g4,
			REG_ANLG_PHY_G4_ANALOG_USB20_USB20_UTMI_CTL2,
			msk, msk);

		msk = 0x200;
		regmap_update_bits(phy->ana_g4,
			REG_ANLG_PHY_G4_ANALOG_USB20_USB20_UTMI_CTL1,
			msk, msk);
		phy->is_host = true;
	} else {
		reg = msk = MASK_ANLG_PHY_G4_ANALOG_USB20_UTMIOTG_IDDG;
		regmap_update_bits(phy->ana_g4,
			REG_ANLG_PHY_G4_ANALOG_USB20_IDDG, msk, reg);

		msk = MASK_ANLG_PHY_G4_DBG_SEL_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_G4_DBG_SEL_ANALOG_USB20_USB20_DPPULLDOWN;
		regmap_update_bits(phy->ana_g4,
			REG_ANLG_PHY_G4_ANALOG_USB20_REG_SEL_CFG_0,
			msk, msk);

		msk = MASK_ANLG_PHY_G4_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_G4_ANALOG_USB20_USB20_DPPULLDOWN;
		regmap_update_bits(phy->ana_g4,
			REG_ANLG_PHY_G4_ANALOG_USB20_USB20_UTMI_CTL2,
			msk, 0);

		msk = 0x200;
		regmap_update_bits(phy->ana_g4,
			REG_ANLG_PHY_G4_ANALOG_USB20_USB20_UTMI_CTL1,
			msk, 0);
		phy->is_host = false;
	}
	return 0;
}

static int sprd_hsphy_init(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 value, reg, msk;
	int ret;

	if (atomic_read(&phy->inited)) {
		dev_dbg(x->dev, "%s is already inited!\n", __func__);
		return 0;
	}

	/* Turn On VDD */
	regulator_set_voltage(phy->vdd, phy->vdd_vol, phy->vdd_vol);
	if (!regulator_is_enabled(phy->vdd)) {
		ret = regulator_enable(phy->vdd);
		if (ret)
			return ret;
	}
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_EB2,
		MASK_AON_APB_OTG_REF_EB, MASK_AON_APB_OTG_REF_EB);

	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_PWR_CTRL,
		MASK_AON_APB_USB_PHY_PD_L, 0);
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_PWR_CTRL,
		MASK_AON_APB_USB_PHY_PD_S, 0);
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_PWR_CTRL,
		MASK_AON_APB_USB_ISO_SW_EN, 0);

	/* usb vbus valid */
	value = readl_relaxed(phy->base + REG_AP_AHB_OTG_PHY_TEST);
	value |= MASK_AP_AHB_OTG_VBUS_VALID_PHYREG;
	writel_relaxed(value, phy->base + REG_AP_AHB_OTG_PHY_TEST);

	reg = msk = MASK_ANLG_PHY_G4_ANALOG_USB20_USB20_VBUSVLDEXT;
	regmap_update_bits(phy->ana_g4,
		REG_ANLG_PHY_G4_ANALOG_USB20_USB20_UTMI_CTL1, msk, reg);

	reg = msk = MASK_ANLG_PHY_G4_ANALOG_USB20_USB20_DATABUS16_8;
	regmap_update_bits(phy->ana_g4,
		REG_ANLG_PHY_G4_ANALOG_USB20_USB20_UTMI_CTL1, msk, reg);

	/* for SPRD phy utmi_width sel */
	value = readl_relaxed(phy->base + REG_AP_AHB_OTG_PHY_CTRL);
	value |= MASK_AP_AHB_UTMI_WIDTH_SEL;
	writel_relaxed(value, phy->base + REG_AP_AHB_OTG_PHY_CTRL);

	reg = msk = MASK_ANLG_PHY_G4_ANALOG_USB20_USB20_TUNEHSAMP;
	regmap_update_bits(phy->ana_g4,
		REG_ANLG_PHY_G4_ANALOG_USB20_USB20_TRIMMING, msk, reg);

	if (!atomic_read(&phy->reset)) {
		sprd_hsphy_reset_core(phy);
		atomic_set(&phy->reset, 1);
	}

	atomic_set(&phy->inited, 1);

	return 0;
}

static void sprd_hsphy_shutdown(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 value, reg, msk;

	if (!atomic_read(&phy->inited)) {
		dev_dbg(x->dev, "%s is already shut down\n", __func__);
		return;
	}

	/* usb vbus */
	value = readl_relaxed(phy->base + REG_AP_AHB_OTG_PHY_TEST);
	value &= ~MASK_AP_AHB_OTG_VBUS_VALID_PHYREG;
	writel_relaxed(value, phy->base + REG_AP_AHB_OTG_PHY_TEST);

	reg = msk = MASK_ANLG_PHY_G4_ANALOG_USB20_USB20_VBUSVLDEXT;
	regmap_update_bits(phy->ana_g4,
		REG_ANLG_PHY_G4_ANALOG_USB20_USB20_UTMI_CTL1, msk, 0);

	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_PWR_CTRL,
		MASK_AON_APB_USB_ISO_SW_EN, MASK_AON_APB_USB_ISO_SW_EN);
	usleep_range(10000, 15000);
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_PWR_CTRL,
		MASK_AON_APB_USB_PHY_PD_L, MASK_AON_APB_USB_PHY_PD_L);
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_PWR_CTRL,
		MASK_AON_APB_USB_PHY_PD_S, MASK_AON_APB_USB_PHY_PD_S);

	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_EB2,
		MASK_AON_APB_OTG_REF_EB, 0);

	if (regulator_is_enabled(phy->vdd))
		regulator_disable(phy->vdd);

	atomic_set(&phy->inited, 0);
	atomic_set(&phy->reset, 0);
}

static ssize_t vdd_voltage_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct sprd_hsphy *x = dev_get_drvdata(dev);

	if (!x)
		return -EINVAL;

	return sprintf(buf, "%d\n", x->vdd_vol);
}

static ssize_t vdd_voltage_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct sprd_hsphy *x = dev_get_drvdata(dev);
	u32 vol;

	if (!x)
		return -EINVAL;

	if (kstrtouint(buf, 16, &vol) < 0)
		return -EINVAL;

	if (vol < 1200000 || vol > 3750000) {
		dev_err(dev, "Invalid voltage value %d\n", vol);
		return -EINVAL;
	}
	x->vdd_vol = vol;

	return size;
}
static DEVICE_ATTR_RW(vdd_voltage);

static struct attribute *usb_hsphy_attrs[] = {
	&dev_attr_vdd_voltage.attr,
	NULL
};
ATTRIBUTE_GROUPS(usb_hsphy);

static int sprd_hsphy_vbus_notify(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	struct usb_phy *usb_phy = container_of(nb, struct usb_phy, vbus_nb);
	struct sprd_hsphy *phy = container_of(usb_phy, struct sprd_hsphy, phy);
	u32 value, reg, msk;

	if (phy->is_host) {
		dev_info(phy->dev, "USB PHY is host mode\n");
		return 0;
	}

	pm_wakeup_event(phy->dev, 400);

	if (event) {
		/* usb vbus valid */
		value = readl_relaxed(phy->base + REG_AP_AHB_OTG_PHY_TEST);
		value |= MASK_AP_AHB_OTG_VBUS_VALID_PHYREG;
		writel_relaxed(value, phy->base + REG_AP_AHB_OTG_PHY_TEST);

		reg = msk = MASK_ANLG_PHY_G4_ANALOG_USB20_USB20_VBUSVLDEXT;
		regmap_update_bits(phy->ana_g4,
			REG_ANLG_PHY_G4_ANALOG_USB20_USB20_UTMI_CTL1, msk, reg);

		reg = msk = MASK_ANLG_PHY_G4_ANALOG_USB20_USB20_DATABUS16_8;
		regmap_update_bits(phy->ana_g4,
			REG_ANLG_PHY_G4_ANALOG_USB20_USB20_UTMI_CTL1, msk, reg);
	} else {
		/* usb vbus invalid */
		value = readl_relaxed(phy->base + REG_AP_AHB_OTG_PHY_TEST);
		value &= ~MASK_AP_AHB_OTG_VBUS_VALID_PHYREG;
		writel_relaxed(value, phy->base + REG_AP_AHB_OTG_PHY_TEST);

		reg = msk = MASK_ANLG_PHY_G4_ANALOG_USB20_USB20_VBUSVLDEXT;
		regmap_update_bits(phy->ana_g4,
			REG_ANLG_PHY_G4_ANALOG_USB20_USB20_UTMI_CTL1, msk, 0);
	}

	phy->event = event;
	queue_work(system_unbound_wq, &phy->work);

	return 0;
}

static enum usb_charger_type sprd_hsphy_retry_charger_detect(struct usb_phy *x);

static enum usb_charger_type sprd_hsphy_charger_detect(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);

	if (x->flags&CHARGER_2NDDETECT_SELECT)
		return sprd_hsphy_retry_charger_detect(x);

	return sc27xx_charger_detect(phy->pmic);
}

static enum usb_charger_type sprd_hsphy_retry_charger_detect(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	enum usb_charger_type type = UNKNOWN_TYPE;
	int dm_voltage, dp_voltage;
	int cnt = 20;

	if (!phy->dm || !phy->dp) {
		dev_err(x->dev, " phy->dp:%p, phy->dm:%p\n",
			phy->dp, phy->dm);
		return UNKNOWN_TYPE;
	}

	regmap_update_bits(phy->pmic, SC2721_CHARGE_DET_FGU_CTRL,
			   BIT_DP_DM_AUX_EN | BIT_DP_DM_BC_ENB,
			   BIT_DP_DM_AUX_EN);
	msleep(300);
	iio_read_channel_processed(phy->dp, &dp_voltage);
	if (dp_voltage > VOLT_LO_LIMIT) {
		do {
			iio_read_channel_processed(phy->dm, &dm_voltage);
			if (dm_voltage > VOLT_LO_LIMIT) {
				type = DCP_TYPE;
				break;
			}
			msleep(100);
			cnt--;
			iio_read_channel_processed(phy->dp, &dp_voltage);
			if (dp_voltage  < VOLT_HI_LIMIT) {
				type = SDP_TYPE;
				break;
			}
		} while ((x->chg_state == USB_CHARGER_PRESENT) && cnt > 0);
	}
	regmap_update_bits(phy->pmic, SC2721_CHARGE_DET_FGU_CTRL,
			   BIT_DP_DM_AUX_EN | BIT_DP_DM_BC_ENB, 0);
	dev_info(x->dev, "correct type is %x\n", type);
	if (type != UNKNOWN_TYPE) {
		x->chg_type = type;
		schedule_work(&x->chg_work);
	}
	return type;
}

int sprd_hsphy_cali_mode(void)
{
	struct device_node *cmdline_node;
	const char *cmdline, *mode;
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmdline);

	if (ret) {
		pr_err("Can't not parse bootargs\n");
		return 0;
	}

	mode = strstr(cmdline, "androidboot.mode=cali");

	if (mode)
		return 1;
	else
		return 0;
}

static int sprd_hsphy_probe(struct platform_device *pdev)
{
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	struct sprd_hsphy *phy;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret = 0, calimode = 0;
	struct usb_otg *otg;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "phy_glb_regs");
	if (!res) {
		dev_err(dev, "missing USB PHY registers resource\n");
		return -ENODEV;
	}

	phy->base = devm_ioremap_nocache(dev, res->start, resource_size(res));
	if (IS_ERR(phy->base))
		return PTR_ERR(phy->base);

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(dev, "unable to get syscon platform device\n");
		return -ENODEV;
	}

	phy->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!phy->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(dev->of_node, "sprd,vdd-voltage",
				   &phy->vdd_vol);
	if (ret < 0) {
		dev_err(dev, "unable to read ssphy vdd voltage\n");
		return ret;
	}

	calimode = sprd_hsphy_cali_mode();
	if (calimode) {
		phy->vdd_vol = FULLSPEED_USB33_TUNE;
		dev_info(dev, "calimode vdd_vol:%d\n", phy->vdd_vol);
	}

	phy->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(phy->vdd)) {
		dev_err(dev, "unable to get ssphy vdd supply\n");
		return PTR_ERR(phy->vdd);
	}

	ret = regulator_set_voltage(phy->vdd, phy->vdd_vol, phy->vdd_vol);
	if (ret < 0) {
		dev_err(dev, "fail to set ssphy vdd voltage at %dmV\n",
			phy->vdd_vol);
		return ret;
	}

	otg = devm_kzalloc(&pdev->dev, sizeof(*otg), GFP_KERNEL);
	if (!otg)
		return -ENOMEM;

	phy->apahb = syscon_regmap_lookup_by_phandle(dev->of_node,
				 "sprd,syscon-apahb");

	if (IS_ERR(phy->apahb)) {
		dev_err(&pdev->dev, "ap USB apahb syscon failed!\n");
		return PTR_ERR(phy->apahb);
	}

	phy->ana_g4 = syscon_regmap_lookup_by_phandle(dev->of_node,
				 "sprd,syscon-anag4");
	if (IS_ERR(phy->ana_g4)) {
		dev_err(&pdev->dev, "ap USB anag4 syscon failed!\n");
		return PTR_ERR(phy->ana_g4);
	}

	phy->hsphy_glb = syscon_regmap_lookup_by_phandle(dev->of_node,
				 "sprd,syscon-enable");
	if (IS_ERR(phy->hsphy_glb)) {
		dev_err(&pdev->dev, "ap USB aon apb syscon failed!\n");
		return PTR_ERR(phy->hsphy_glb);
	}

	phy->dp = devm_iio_channel_get(dev, "dp");
	phy->dm = devm_iio_channel_get(dev, "dm");
	if (IS_ERR(phy->dp)) {
		phy->dp = NULL;
		dev_warn(dev, "failed to get dp or dm channel\n");
	}
	if (IS_ERR(phy->dm)) {
		phy->dm = NULL;
		dev_warn(dev, "failed to get dp or dm channel\n");
	}

	/* enable usb module */
	regmap_update_bits(phy->apahb, REG_AP_AHB_AHB_EB,
		MASK_AP_AHB_OTG_EB, MASK_AP_AHB_OTG_EB);

	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_EB2,
		MASK_AON_APB_ANLG_APB_EB | MASK_AON_APB_ANLG_EB |
		MASK_AON_APB_OTG_REF_EB,
		MASK_AON_APB_ANLG_APB_EB | MASK_AON_APB_ANLG_EB |
		MASK_AON_APB_OTG_REF_EB);

	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_PWR_CTRL,
		MASK_AON_APB_USB_PHY_PD_L, MASK_AON_APB_USB_PHY_PD_L);

	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_PWR_CTRL,
		MASK_AON_APB_USB_PHY_PD_S, MASK_AON_APB_USB_PHY_PD_S);

	phy->dev = dev;
	phy->phy.dev = dev;
	phy->phy.label = "sprd-hsphy";
	phy->phy.otg = otg;
	phy->phy.init = sprd_hsphy_init;
	phy->phy.shutdown = sprd_hsphy_shutdown;
	phy->phy.set_vbus = sprd_hostphy_set;
	phy->phy.type = USB_PHY_TYPE_USB2;
	phy->phy.vbus_nb.notifier_call = sprd_hsphy_vbus_notify;
	phy->phy.charger_detect = sprd_hsphy_charger_detect;
	phy->phy.flags |= CHARGER_2NDDETECT_ENABLE;
	otg->usb_phy = &phy->phy;

	device_init_wakeup(phy->dev, true);

	phy->wake_lock = wakeup_source_register(phy->dev, "sprd-hsphy");
	if (!phy->wake_lock) {
		dev_err(dev, "fail to register wakeup lock.\n");
		return -ENOMEM;
	}

	INIT_WORK(&phy->work, sprd_hsphy_charger_detect_work);

	platform_set_drvdata(pdev, phy);

	ret = usb_add_phy_dev(&phy->phy);
	if (ret) {
		dev_err(dev, "fail to add phy\n");
		return ret;
	}

	ret = sysfs_create_groups(&dev->kobj, usb_hsphy_groups);
	if (ret)
		dev_err(dev, "failed to create usb hsphy attributes\n");

	if (extcon_get_state(phy->phy.edev, EXTCON_USB) > 0)
		usb_phy_set_charger_state(&phy->phy, USB_CHARGER_PRESENT);

	dev_info(dev, "sprd usb phy probe ok\n");

	return 0;
}

static int sprd_hsphy_remove(struct platform_device *pdev)
{
	struct sprd_hsphy *phy = platform_get_drvdata(pdev);

	sysfs_remove_groups(&pdev->dev.kobj, usb_hsphy_groups);
	usb_remove_phy(&phy->phy);
	regulator_disable(phy->vdd);

	return 0;
}

static const struct of_device_id sprd_hsphy_match[] = {
	{ .compatible = "sprd,sharkl3-phy" },
	{},
};

MODULE_DEVICE_TABLE(of, sprd_hsphy_match);

static struct platform_driver sprd_hsphy_driver = {
	.probe = sprd_hsphy_probe,
	.remove = sprd_hsphy_remove,
	.driver = {
		.name = "sprd-hsphy",
		.of_match_table = sprd_hsphy_match,
	},
};

static int __init sprd_hsphy_driver_init(void)
{
	return platform_driver_register(&sprd_hsphy_driver);
}

static void __exit sprd_hsphy_driver_exit(void)
{
	platform_driver_unregister(&sprd_hsphy_driver);
}

late_initcall(sprd_hsphy_driver_init);
module_exit(sprd_hsphy_driver_exit);

MODULE_DESCRIPTION("UNISOC USB PHY driver");
MODULE_LICENSE("GPL v2");
