/*copyright (C) 2022 UNISOC Communications Inc.
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
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/soc/sprd/sprd_usbpinmux.h>

#define	JTAG_APWDG_VAL	(0x10000000)

static u32		__iomem *mux_reg;
static u32		mux_val;

static int usbmux_get_regs(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res) {
		dev_err(dev, "missing IOMEM\n");
		return -EINVAL;
	}

	mux_reg = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
	if (!mux_reg) {
		dev_err(&pdev->dev, "failed to remap mux_reg\n");
		return -ENXIO;
	} else {
		mux_val = readl_relaxed(mux_reg);
	}

	return 0;
}

int ums9620_usbmux_check_mode(void)
{
	if (mux_val && (mux_val != JTAG_APWDG_VAL)) {
		pr_info("USBPinMux open: mux_val = 0x%x\n", mux_val);
		return 1;
	}

	return 0;
}

static int sprd_ums9620_usbpinmux_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regulator	*vdd_on;
	u32			vdd_vol;
	int ret;

	dev_info(dev, "sprd_ums9620_usbpinmux_probe entry!\n");

	ret = usbmux_get_regs(pdev);
	if (!ret)
		dev_dbg(dev, "usbmux_get_regs ok!\n");

	usbmux_check_mode_func = ums9620_usbmux_check_mode;

	if (ums9620_usbmux_check_mode() > 0) {
		ret = of_property_read_u32(dev->of_node, "sprd,vdd-voltage",
				&vdd_vol);
		if (ret < 0) {
			dev_err(dev, "unable to read usbmux vdd voltage\n");
			return ret;
		}

		vdd_on = devm_regulator_get(dev, "vdd");
		if (IS_ERR(vdd_on)) {
			dev_err(dev, "unable to get usbmux vdd supply\n");
			return PTR_ERR(vdd_on);
		}

		ret = regulator_set_voltage(vdd_on, vdd_vol, vdd_vol);
		if (ret < 0) {
			dev_err(dev, "fail to set usbmux vdd voltage at %dmV\n",
				vdd_vol);
			return ret;
		}

		ret = regulator_enable(vdd_on);
		if (ret) {
			dev_err(dev, "fail to enable regulator!\n");
			return ret;
		}
	}

	dev_info(dev, "sprd_ums9620_usbpinmux_probe end!\n");

	return 0;
}

static const struct of_device_id sprd_ums9620_usbpinmux_of_match[] = {
	{ .compatible = "sprd,qogirn6pro-usbpinmux" },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, sprd_ums9620_usbpinmux_of_match);

static struct platform_driver sprd_ums9620_usbpinmux_driver = {
	.probe = sprd_ums9620_usbpinmux_probe,
	.driver = {
		.name = "sprd-ums9620-usbpinmux",
		.of_match_table = sprd_ums9620_usbpinmux_of_match,
	},
};

module_platform_driver(sprd_ums9620_usbpinmux_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Porter Xu<porter.xu@unisoc.com>");
MODULE_DESCRIPTION("unisoc platform ums9620 usbpinmux driver");
