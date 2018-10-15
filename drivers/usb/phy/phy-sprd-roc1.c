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

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/usb/otg.h>
#include <linux/usb/phy.h>
#include <dt-bindings/soc/sprd,roc1-mask.h>
#include <dt-bindings/soc/sprd,roc1-regs.h>

struct sprd_hsphy {
	struct device		*dev;
	struct usb_phy		phy;
	void __iomem		*base;
	struct regulator	*vdd;
	struct regmap           *hsphy_glb;
	struct regmap           *apahb;
	struct regmap           *ana_g2;
	struct regmap           *ana_g3;
	struct regmap           *ana_g4;
	u32			vdd_vol;
	atomic_t		reset;
	atomic_t		inited;
};

#define TUNEHSAMP_2_6MA (2 << 25)

static inline void sprd_hsphy_reset_core(struct sprd_hsphy *phy)
{
	u32 reg, msk;

	/* Reset PHY */
	reg = msk = MASK_AON_APB_OTG_PHY_SOFT_RST |
			MASK_AON_APB_OTG_UTMI_SOFT_RST;

	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_RST1,
		msk, reg);

	/* USB PHY reset need to delay 20ms~30ms */
	usleep_range(20000, 30000);
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_RST1,
		msk, 0);
}

static int sprd_hsphy_reset(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);

	sprd_hsphy_reset_core(phy);
	return 0;
}

static int sprd_hostphy_set(struct usb_phy *x, int on)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 reg, msk;
	int ret = 0;

	if (on) {
		/* set USB connector type is A-type*/
		msk = MASK_AON_APB_USB2_PHY_IDDIG;
		ret |= regmap_update_bits(phy->hsphy_glb,
			REG_AON_APB_OTG_PHY_CTRL, msk, 0);

		/* the pull down resistance on D-/D+ enable */
		msk = MASK_AON_APB_OTG_DMPULLDOWN | MASK_AON_APB_OTG_DPPULLDOWN;
		ret |= regmap_update_bits(phy->ana_g3,
			REG_ANLG_PHY_G3_ANALOG_USB20_USB20_UTMI_CTL2,
			msk, msk);

		reg = 0x200;
		msk = MASK_ANLG_PHY_G3_ANALOG_USB20_USB20_RESERVED;
		ret |= regmap_update_bits(phy->ana_g3,
			REG_ANLG_PHY_G3_ANALOG_USB20_USB20_UTMI_CTL1,
			msk, reg);
	} else {
		reg = msk = MASK_AON_APB_USB2_PHY_IDDIG;
		ret |= regmap_update_bits(phy->hsphy_glb,
			REG_AON_APB_OTG_PHY_CTRL, msk, reg);

		msk = MASK_AON_APB_OTG_DMPULLDOWN | MASK_AON_APB_OTG_DPPULLDOWN;
		ret |= regmap_update_bits(phy->ana_g3,
			REG_ANLG_PHY_G3_ANALOG_USB20_USB20_UTMI_CTL2,
			msk, 0);

		msk = MASK_ANLG_PHY_G3_ANALOG_USB20_USB20_RESERVED;
		ret |= regmap_update_bits(phy->ana_g3,
			REG_ANLG_PHY_G3_ANALOG_USB20_USB20_UTMI_CTL1,
			msk, 0);
	}

	return ret;
}

static void sprd_hsphy_emphasis_set(struct usb_phy *x, bool enabled)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 msk, reg;

	if (!phy)
		return;

	reg = TUNEHSAMP_2_6MA;
	msk = MASK_ANLG_PHY_G3_ANALOG_USB20_USB20_TUNEHSAMP;
	if (enabled)
		regmap_update_bits(phy->ana_g3,
			REG_ANLG_PHY_G3_ANALOG_USB20_USB20_TRIMMING,
			msk, reg);
	else
		regmap_update_bits(phy->ana_g3,
			REG_ANLG_PHY_G3_ANALOG_USB20_USB20_TRIMMING,
			msk, 0);
}

static int sprd_hsphy_init(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	int ret, reg, msk;

	if (atomic_read(&phy->inited)) {
		dev_dbg(x->dev, "%s is already inited!\n", __func__);
		return 0;
	}

	/* Turn On VDD */
	regulator_set_voltage(phy->vdd, phy->vdd_vol, phy->vdd_vol);
	ret = regulator_enable(phy->vdd);
	if (ret)
		return ret;

	/* Enable USB UTMI controller */
	reg = msk = MASK_AON_APB_OTG_UTMI_EB;
	ret |= regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_EB1, msk, reg);

	/* Enable USB PHY controller */
	reg = msk = MASK_AON_APB_OTG_PHY_EB;
	ret |= regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_EB1, msk, reg);

	/* enable cgm otg ref */
	reg = msk = MASK_AON_APB_CGM_OTG_REF_EN;
	ret |= regmap_update_bits(phy->hsphy_glb, REG_AON_APB_CGM_REG1, msk, reg);

	/* USB PHY power */
	reg = msk = MASK_AON_APB_C2G_ANALOG_USB20_USB20_PS_PD_S |
		MASK_AON_APB_C2G_ANALOG_USB20_USB20_PS_PD_L;
	ret |= regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_MIPI_CSI_POWER_CTRL, msk, 0);

	/* USB vbus valid */
	reg = msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
	ret |= regmap_update_bits(phy->hsphy_glb, REG_AON_APB_OTG_PHY_TEST, msk, reg);
	reg = msk = MASK_ANLG_PHY_G3_ANALOG_USB20_USB20_VBUSVLDEXT;
	ret |= regmap_update_bits(phy->ana_g3,
		REG_ANLG_PHY_G3_ANALOG_USB20_USB20_UTMI_CTL1, msk, reg);

	/* For SPRD phy utmi_width sel, set MUTI 16bit */
	reg = msk = MASK_AON_APB_UTMI_WIDTH_SEL;
	ret |= regmap_update_bits(phy->hsphy_glb,
			REG_AON_APB_OTG_PHY_CTRL, msk, reg);

	reg = msk = MASK_ANLG_PHY_G3_ANALOG_USB20_USB20_DATABUS16_8;
	ret |= regmap_update_bits(phy->ana_g3,
			REG_ANLG_PHY_G3_ANALOG_USB20_USB20_UTMI_CTL1,
			msk, reg);

	if (!atomic_read(&phy->reset)) {
		sprd_hsphy_reset_core(phy);
		atomic_set(&phy->reset, 1);
	}

	atomic_set(&phy->inited, 1);
	regmap_read(phy->hsphy_glb, REG_AON_APB_CGM_REG1, &reg);

	return ret;
}

static void sprd_hsphy_shutdown(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 reg, msk;

	if (!atomic_read(&phy->inited)) {
		dev_dbg(x->dev, "%s is already shut down\n", __func__);
		return;
	}

	/* usb vbus */
	msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_OTG_PHY_TEST, msk, 0);
	msk = MASK_ANLG_PHY_G3_ANALOG_USB20_USB20_VBUSVLDEXT;
	regmap_update_bits(phy->ana_g3,
		REG_ANLG_PHY_G3_ANALOG_USB20_USB20_UTMI_CTL1, msk, 0);

	/* usb power down */
	reg = msk = MASK_AON_APB_C2G_ANALOG_USB20_USB20_PS_PD_S |
		MASK_AON_APB_C2G_ANALOG_USB20_USB20_PS_PD_L;
	regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_MIPI_CSI_POWER_CTRL, msk, reg);

	/* usb cgm ref */
	msk = MASK_AON_APB_CGM_OTG_REF_EN;
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_CGM_REG1, msk, 0);

	regulator_disable(phy->vdd);

	atomic_set(&phy->inited, 0);
	atomic_set(&phy->reset, 0);
}

static int sprd_hsphy_post_init(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);

	regulator_disable(phy->vdd);

	return 0;
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

static int sprd_hsphy_probe(struct platform_device *pdev)
{
	struct sprd_hsphy *phy;
	struct device *dev = &pdev->dev;
	int ret, reg, msk;
	struct usb_otg *otg;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	ret = of_property_read_u32(dev->of_node, "sprd,vdd-voltage",
				   &phy->vdd_vol);
	if (ret < 0) {
		dev_err(dev, "unable to read ssphy vdd voltage\n");
		return ret;
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

	phy->ana_g3 = syscon_regmap_lookup_by_phandle(dev->of_node,
				 "sprd,syscon-anag3");
	if (IS_ERR(phy->ana_g3)) {
		dev_err(&pdev->dev, "ap USB anag3 syscon failed!\n");
		return PTR_ERR(phy->ana_g3);
	}

	phy->hsphy_glb = syscon_regmap_lookup_by_phandle(dev->of_node,
				 "sprd,syscon-enable");
	if (IS_ERR(phy->hsphy_glb)) {
		dev_err(&pdev->dev, "ap USB aon apb syscon failed!\n");
		return PTR_ERR(phy->hsphy_glb);
	}

	/* enable usb module */
	reg = msk = MASK_AON_APB_OTG_UTMI_EB | MASK_AON_APB_ANA_EB;
	ret |= regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_EB1, msk, reg);
	reg = msk = MASK_AON_APB_CGM_OTG_REF_EN;
	ret |= regmap_update_bits(phy->hsphy_glb, REG_AON_APB_CGM_REG1, msk, reg);

	/* usb power down */
	reg = msk = MASK_AON_APB_C2G_ANALOG_USB20_USB20_PS_PD_S |
		MASK_AON_APB_C2G_ANALOG_USB20_USB20_PS_PD_L;
	ret |= regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_MIPI_CSI_POWER_CTRL, msk, reg);
	if (ret)
		return ret;

	phy->phy.dev = dev;
	phy->phy.label = "sprd-hsphy";
	phy->phy.otg = otg;
	phy->phy.init = sprd_hsphy_init;
	phy->phy.shutdown = sprd_hsphy_shutdown;
	phy->phy.post_init = sprd_hsphy_post_init;
	phy->phy.reset_phy = sprd_hsphy_reset;
	phy->phy.set_vbus = sprd_hostphy_set;
	phy->phy.set_emphasis = sprd_hsphy_emphasis_set;
	phy->phy.type = USB_PHY_TYPE_USB2;
	otg->usb_phy = &phy->phy;

	platform_set_drvdata(pdev, phy);

	ret = usb_add_phy_dev(&phy->phy);
	if (ret) {
		dev_err(dev, "fail to add phy\n");
		return ret;
	}

	ret = sysfs_create_groups(&dev->kobj, usb_hsphy_groups);
	if (ret)
		dev_warn(dev, "failed to create usb hsphy attributes\n");

	dev_dbg(dev, "sprd usb phy probe ok !\n");

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
	{ .compatible = "sprd,roc1-phy" },
	{},
};

MODULE_DEVICE_TABLE(of, sprd_ssphy_match);

static struct platform_driver sprd_hsphy_driver = {
	.probe = sprd_hsphy_probe,
	.remove = sprd_hsphy_remove,
	.driver = {
		.name = "sprd-hsphy",
		.of_match_table = sprd_hsphy_match,
	},
};

module_platform_driver(sprd_hsphy_driver);
