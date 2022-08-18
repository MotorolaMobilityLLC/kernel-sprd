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
#include <linux/module.h>
#include <linux/soc/sprd/sprd_usbpinmux.h>

int sprd_usbmux_check_mode(void)
{
	int ret = 0;

	pr_info("sprd_usbmux_check_mode entry!\n");

	if(usbmux_check_mode_func != NULL) {
		ret = usbmux_check_mode_func();
		pr_info("check_mode_func: ret_val=0x%x\n", ret);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(sprd_usbmux_check_mode);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Porter Xu<porter.xu@unisoc.com>");
MODULE_DESCRIPTION("unisoc platform usbpinmux driver");

