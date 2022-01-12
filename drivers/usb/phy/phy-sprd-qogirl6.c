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

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/usb/phy.h>
#include <linux/usb/otg.h>
#include <uapi/linux/usb/charger.h>
#include <dt-bindings/soc/sprd,qogirl6-mask.h>
#include <dt-bindings/soc/sprd,qogirl6-regs.h>
#include <uapi/linux/usb/charger.h>

#define SC2730_CHARGE_STATUS		0x1b9c
#define BIT_CHG_DET_DONE		BIT(11)
#define BIT_SDP_INT			BIT(7)
#define BIT_DCP_INT			BIT(6)
#define BIT_CDP_INT			BIT(5)

/*analog2 defined*/
#define REG_ANLG_PHY_G2_ANALOG_USB20_USB20_BATTER_PLL			0x0008
#define REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL2			0x000c
#define REG_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW			0x001c
#define REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1                    0x0004
#define REG_ANLG_PHY_G2_ANALOG_USB20_USB20_TRIMMING                     0x0010
#define REG_ANLG_PHY_G2_ANALOG_USB20_REG_SEL_CFG_0			0x0020

/*mask defined*/
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_S                     0x10
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_L                     0x8
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW_EN                   0x1
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_VBUSVLDEXT                  0x10000
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DATABUS16_8                 0x10000000
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_TUNEHSAMP                   0x6000000
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_TFREGRES                    0x1f80000
#define MASK_ANLG_PHY_G2_DBG_SEL_ANALOG_USB20_USB20_DPPULLDOWN          0x4
#define MASK_ANLG_PHY_G2_DBG_SEL_ANALOG_USB20_USB20_DMPULLDOWN          0x2
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DPPULLDOWN                  0x10
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DMPULLDOWN                  0x8
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_RESERVED                    0xffff

#define TUNEHSAMP_2_6MA			(3 << 25)
#define TFREGRES_TUNE_VALUE		(0xe << 19)
#define DEFAULT_HOST_EYE_PATTERN					0x04f3d1c0
#define DEFAULT_DEVICE_EYE_PATTERN					0x04f3d1c0
struct sprd_hsphy {
	struct device		*dev;
	struct usb_phy		phy;
	struct regulator	*vdd;
	struct regmap           *hsphy_glb;
	struct regmap           *ana_g2;
	struct regmap           *pmic;
	struct wakeup_source	*wake_lock;
	struct work_struct		work;
	unsigned long event;
	u32			vdd_vol;
	u32			host_eye_pattern;
	u32			device_eye_pattern;
	atomic_t		reset;
	atomic_t		inited;
	bool			is_host;
};

#define FULLSPEED_USB33_TUNE		2700000

static enum usb_charger_type sc27xx_charger_detect(struct regmap *regmap)
{
	enum usb_charger_type type;
	u32 status = 0, val;
	int ret, cnt = 10;

	do {
		ret = regmap_read(regmap, SC2730_CHARGE_STATUS, &val);
		if (ret)
			return UNKNOWN_TYPE;

		if (val & BIT_CHG_DET_DONE) {
			status = val & (BIT_CDP_INT | BIT_DCP_INT | BIT_SDP_INT);
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
	u32 reg, msk;

	/* Reset PHY */
	reg = msk = MASK_AON_APB_OTG_PHY_SOFT_RST |
				MASK_AON_APB_OTG_UTMI_SOFT_RST;

	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_RST1,
		msk, reg);

	/* USB PHY reset need to delay 20ms~30ms */
	usleep_range(20000, 30000);
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_RST1, msk, 0);

}

static int sprd_hostphy_set(struct usb_phy *x, int on)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 reg, msk;
	int ret = 0;

	if (on) {
		reg = phy->host_eye_pattern;
		regmap_write(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_TRIMMING, reg);

		msk = MASK_AON_APB_USB2_PHY_IDDIG;
		ret |= regmap_update_bits(phy->hsphy_glb,
			REG_AON_APB_OTG_PHY_CTRL, msk, 0);

		msk = MASK_ANLG_PHY_G2_DBG_SEL_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_G2_DBG_SEL_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_REG_SEL_CFG_0,
			msk, msk);

		msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL2,
			msk, msk);

		reg = 0x200;
		msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_RESERVED;
		ret |= regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1,
			msk, reg);
		phy->is_host = true;
	} else {
		reg = phy->device_eye_pattern;
		regmap_write(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_TRIMMING, reg);

		reg = msk = MASK_AON_APB_USB2_PHY_IDDIG;
		ret |= regmap_update_bits(phy->hsphy_glb,
			REG_AON_APB_OTG_PHY_CTRL, msk, reg);

		msk = MASK_ANLG_PHY_G2_DBG_SEL_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_G2_DBG_SEL_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_REG_SEL_CFG_0,
			msk, msk);

		msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL2,
			msk, 0);

		msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_RESERVED;
		ret |= regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1,
			msk, 0);
		phy->is_host = false;
	}

	return ret;
}

static int sprd_hsphy_init(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 reg, msk;
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

	/* usb enable */
	reg = msk = MASK_AON_APB_OTG_UTMI_EB;
	regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_APB_EB1, msk, reg);

	reg = msk = MASK_AON_APB_CGM_OTG_REF_EN |
		MASK_AON_APB_CGM_DPHY_REF_EN;
	regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_CGM_REG1, msk, reg);

	regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW,
		MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW_EN, 0);

	/* usb phy power */
	msk = (MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_L |
		MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_S);
	regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_BATTER_PLL, msk, 0);

	/* usb vbus valid */
	reg = msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
	regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_OTG_PHY_TEST, msk, reg);

	reg = msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_VBUSVLDEXT;
	regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1,	msk, reg);

	/* for SPRD phy utmi_width sel */
	reg = msk = MASK_AON_APB_UTMI_WIDTH_SEL;
	regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_OTG_PHY_CTRL, msk, reg);

	reg = msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DATABUS16_8;
	regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1,
		msk, reg);

	reg = phy->device_eye_pattern;
	regmap_write(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_TRIMMING, reg);

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
	u32 reg, msk;

	if (!atomic_read(&phy->inited)) {
		dev_dbg(x->dev, "%s is already shut down\n", __func__);
		return;
	}

	/* usb vbus */
	msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_OTG_PHY_TEST, msk, 0);
	msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_VBUSVLDEXT;
	regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1, msk, 0);

	/* usb power down */
	reg = msk = (MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_L |
		MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_S);
	regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_BATTER_PLL, msk, reg);
	reg = msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW_EN;
	regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW,
		msk, reg);

	/* usb cgm ref */
	msk = MASK_AON_APB_CGM_OTG_REF_EN |
		MASK_AON_APB_CGM_DPHY_REF_EN;
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_CGM_REG1, msk, 0);

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

static ssize_t hsphy_device_eye_pattern_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct sprd_hsphy *x = dev_get_drvdata(dev);

	if (!x)
		return -EINVAL;


	return sprintf(buf, "0x%x\n", x->device_eye_pattern);
}

static ssize_t hsphy_device_eye_pattern_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct sprd_hsphy *x = dev_get_drvdata(dev);

	if (!x)
		return -EINVAL;

	if (kstrtouint(buf, 16, &x->device_eye_pattern) < 0)
		return -EINVAL;

	return size;
}
static DEVICE_ATTR_RW(hsphy_device_eye_pattern);

static ssize_t hsphy_host_eye_pattern_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct sprd_hsphy *x = dev_get_drvdata(dev);

	if (!x)
		return -EINVAL;


	return sprintf(buf, "0x%x\n", x->host_eye_pattern);
}

static ssize_t hsphy_host_eye_pattern_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct sprd_hsphy *x = dev_get_drvdata(dev);

	if (!x)
		return -EINVAL;

	if (kstrtouint(buf, 16, &x->host_eye_pattern) < 0)
		return -EINVAL;

	return size;
}

static DEVICE_ATTR_RW(hsphy_host_eye_pattern);

static struct attribute *usb_hsphy_attrs[] = {
	&dev_attr_vdd_voltage.attr,
	&dev_attr_hsphy_device_eye_pattern.attr,
	&dev_attr_hsphy_host_eye_pattern.attr,
	NULL
};
ATTRIBUTE_GROUPS(usb_hsphy);

static int sprd_hsphy_vbus_notify(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	struct usb_phy *usb_phy = container_of(nb, struct usb_phy, vbus_nb);
	struct sprd_hsphy *phy = container_of(usb_phy, struct sprd_hsphy, phy);
	u32 reg, msk;

	if (phy->is_host) {
		dev_info(phy->dev, "USB PHY is host mode\n");
		return 0;
	}

	pm_wakeup_event(phy->dev, 400);

	if (event) {
		/* usb vbus valid */
		reg = msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
		regmap_update_bits(phy->hsphy_glb, REG_AON_APB_OTG_PHY_TEST, msk, reg);

		reg = msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_VBUSVLDEXT;
		regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1, msk, reg);

		reg = msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DATABUS16_8;
		regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1,
			msk, reg);

		usb_phy_set_charger_state(usb_phy, USB_CHARGER_PRESENT);
	} else {
		/* usb vbus invalid */
		msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
		regmap_update_bits(phy->hsphy_glb, REG_AON_APB_OTG_PHY_TEST, msk, 0);

		msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_VBUSVLDEXT;
		regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1, msk, 0);

		usb_phy_set_charger_state(usb_phy, USB_CHARGER_ABSENT);
	}

	phy->event = event;
	queue_work(system_unbound_wq, &phy->work);

	return 0;
}

static enum usb_charger_type sprd_hsphy_charger_detect(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);

	return sc27xx_charger_detect(phy->pmic);
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
	int ret = 0, calimode = 0;
	u32 reg, msk;
	struct usb_otg *otg;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(dev, "unable to get syscon platform device\n");
		ret = -ENODEV;
		goto device_node_err;
	}

	phy->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!phy->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		ret = -ENODEV;
		goto platform_device_err;
	}

	ret = of_property_read_u32(dev->of_node, "sprd,vdd-voltage",
				   &phy->vdd_vol);
	if (ret < 0) {
		dev_err(dev, "unable to read ssphy vdd voltage\n");
		goto platform_device_err;
	}

	calimode = sprd_hsphy_cali_mode();
	if (calimode) {
		phy->vdd_vol = FULLSPEED_USB33_TUNE;
		dev_info(dev, "calimode vdd_vol:%d\n", phy->vdd_vol);
	}

	phy->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(phy->vdd)) {
		dev_err(dev, "unable to get ssphy vdd supply\n");
		ret = PTR_ERR(phy->vdd);
		goto platform_device_err;
	}

	ret = regulator_set_voltage(phy->vdd, phy->vdd_vol, phy->vdd_vol);
	if (ret < 0) {
		dev_err(dev, "fail to set ssphy vdd voltage at %dmV\n",
			phy->vdd_vol);
		goto platform_device_err;
	}

	otg = devm_kzalloc(&pdev->dev, sizeof(*otg), GFP_KERNEL);
	if (!otg) {
		ret = -ENOMEM;
		goto platform_device_err;
	}

	phy->ana_g2 = syscon_regmap_lookup_by_phandle(dev->of_node,
				 "sprd,syscon-anag2");
	if (IS_ERR(phy->ana_g2)) {
		dev_err(&pdev->dev, "ap USB anag2 syscon failed!\n");
		ret = PTR_ERR(phy->ana_g2);
		goto platform_device_err;
	}

	phy->hsphy_glb = syscon_regmap_lookup_by_phandle(dev->of_node,
				 "sprd,syscon-enable");
	if (IS_ERR(phy->hsphy_glb)) {
		dev_err(&pdev->dev, "ap USB aon apb syscon failed!\n");
		ret = PTR_ERR(phy->hsphy_glb);
		goto platform_device_err;
	}

	ret = of_property_read_u32(dev->of_node, "hsphy-device-eye-pattern",
					&phy->device_eye_pattern);
	if (ret < 0) {
		dev_err(dev, "unable to get hsphy-device-eye-pattern node\n");
		phy->device_eye_pattern = DEFAULT_DEVICE_EYE_PATTERN;
	}

	ret = of_property_read_u32(dev->of_node, "hsphy-host-eye-pattern",
					&phy->host_eye_pattern);
	if (ret < 0) {
		dev_err(dev, "unable to get hsphy-host-eye-pattern node\n");
		phy->host_eye_pattern = DEFAULT_HOST_EYE_PATTERN;
	}

	/* enable usb module */
	reg = msk = (MASK_AON_APB_OTG_UTMI_EB | MASK_AON_APB_ANA_EB);
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_EB1, msk, reg);

	reg = msk = MASK_AON_APB_CGM_OTG_REF_EN | MASK_AON_APB_CGM_DPHY_REF_EN;
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_CGM_REG1, msk, reg);

	/* usb power down */
	reg = msk = (MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_L |
		MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_S);
	regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_BATTER_PLL, msk, reg);

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
	otg->usb_phy = &phy->phy;

	device_init_wakeup(phy->dev, true);

	phy->wake_lock = wakeup_source_register(phy->dev, "sprd-hsphy");
	if (!phy->wake_lock) {
		dev_err(dev, "fail to register wakeup lock.\n");
		goto platform_device_err;
	}

	INIT_WORK(&phy->work, sprd_hsphy_charger_detect_work);

	platform_set_drvdata(pdev, phy);

	ret = usb_add_phy_dev(&phy->phy);
	if (ret) {
		dev_err(dev, "fail to add phy\n");
		goto  platform_device_err;
	}

	ret = sysfs_create_groups(&dev->kobj, usb_hsphy_groups);
	if (ret)
		dev_warn(dev, "failed to create usb hsphy attributes\n");

	if (extcon_get_state(phy->phy.edev, EXTCON_USB) > 0)
		usb_phy_set_charger_state(&phy->phy, USB_CHARGER_PRESENT);

	dev_dbg(dev, "sprd usb phy probe ok !\n");

platform_device_err:
	of_dev_put(regmap_pdev);
device_node_err:
	of_node_put(regmap_np);

	return ret;
}

static int sprd_hsphy_remove(struct platform_device *pdev)
{
	struct sprd_hsphy *phy = platform_get_drvdata(pdev);

	sysfs_remove_groups(&pdev->dev.kobj, usb_hsphy_groups);
	usb_remove_phy(&phy->phy);
	if (regulator_is_enabled(phy->vdd))
		regulator_disable(phy->vdd);

	return 0;
}

static const struct of_device_id sprd_hsphy_match[] = {
	{ .compatible = "sprd,qogirl6-phy" },
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
