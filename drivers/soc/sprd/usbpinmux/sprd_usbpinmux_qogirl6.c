/*copyright (C) 2022 Spreadtrum Communications Inc.
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

static struct regmap	*usbmux_cfg;
static u32		usb_uart_jtag_mux_reg;
static u32		usb_uart_jtag_mux_mask;

#define	JTAG_APWDG_VAL	0x1

static int usbmux_syscon_get_args(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	unsigned int args[2];

	usbmux_cfg = syscon_regmap_lookup_by_phandle_args(dev->of_node,
		"usb-mux-syscon", 2, args);
	if (IS_ERR(usbmux_cfg)) {
		dev_err(&pdev->dev, "get usbmux_cfg syscon failed!\n");
		return -EINVAL;
	} else {
		usb_uart_jtag_mux_reg = args[0];
		usb_uart_jtag_mux_mask = args[1];
		pr_debug("usb_uart_jtag_mux:reg:%x,mask:%x\n",
			args[0], args[1]);
	}
	return 0;
}

int sprd_usbmux_check_mode(void)
{
	u32 mux_val;
	int ret;

	if (!usbmux_cfg)
		return -EINVAL;

	ret = regmap_read(usbmux_cfg, usb_uart_jtag_mux_reg, &mux_val);
	if (ret) {
		pr_err("Failed to read mux_val\n");
		return -EINVAL;
	}

	if (mux_val && mux_val != JTAG_APWDG_VAL) {
		pr_debug("USBMux open: mux_val = 0x%x\n", mux_val);
		return 1;
	}

	return 0;
}
EXPORT_SYMBOL(sprd_usbmux_check_mode);

static int sprd_usbpinmux_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regulator	*vdd_on;
	u32			vdd_vol;
	int ret;

	ret = usbmux_syscon_get_args(pdev);
	if (!ret)
		dev_dbg(dev, "usbmux_syscon_get_args ok!\n");

	if (sprd_usbmux_check_mode() > 0) {
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

	return 0;
}

static const struct of_device_id sprd_usbpinmux_of_match[] = {
	{ .compatible = "sprd,qogirl6-usbpinmux" },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, sprd_usbpinmux_of_match);

static struct platform_driver sprd_usbpinmux_driver = {
	.probe = sprd_usbpinmux_probe,
	.driver = {
		.name = "sprd-usbpinmux",
		.of_match_table = sprd_usbpinmux_of_match,
	},
};

module_platform_driver(sprd_usbpinmux_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Porter Xu<porter.xu@unisoc.com>");
MODULE_DESCRIPTION("unisoc platform usbpinmux driver");
