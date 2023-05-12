// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2021 Spreadtrum Communications Inc.

#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/regmap.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/platform_data/sprd_ump96xx_tsensor.h>

#define UMP96XX_TSEN_OSC_TEMP(h, l) \
	(((((h & GENMASK(5, 3)) << 16) | (l & GENMASK(15, 0))) << 4) & 0xFFFFF)
#define UMP96XX_TSEN_TSEN_TEMP(h, l) \
	(((((h & GENMASK(2, 0)) << 16) | (l & GENMASK(15, 0))) << 4) & 0xFFFFF)

#define UMP96XX_TSEN_INDEX_MASK		GENMASK(7, 0)
#define UMP96XX_TSEN_INDEX_SHIFT	12
#define UMP96XX_TSEN_FRAC_MASK		GENMASK(11, 0)
#define ABNORMAL_TEMP			160000

/* Temperature query table according to integral index */
/*26MHZ TSX
static const int v2t_table[256] = {
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 280421, 264933, 251238, 238928, 227748, 217492, 208038,
	199268, 191078, 183383, 176104, 169240, 162710, 156500, 150570, 144893, 139428, 134191,
	129138, 124253, 119525, 114943, 110500, 106187, 101990, 97905, 93919, 90026, 86224, 82506,
	78860, 75286, 71777, 68330, 64940, 61606, 58337, 55113, 51932, 48790, 45696, 42642, 39620,
	36627, 33675, 30750, 27848, 24974, 22128, 19300, 16490, 13707, 10937, 8180, 5444, 2717, 0,
	-2702, -5398, -8088, -10768, -13446, -16120, -18791, -21462, -24132, -26805, -29481, -32160,
	-34847, -37539, -40241, -42956, -45680, -48419, -51175, -53944, -56734, -59548, -62379,
	-65234, -68124, -71039, -73979, -76964, -79985, -83041, -86136, -89290, -92493, -95747,
	-99057, -102428, -105886, -109418, -113031, -116733, -120533, -124438,  -128458, -132609,
	-136903, -141357, -145991, -150823, -155881, -161216, -166830, -172781, -179140, -185969,
	-193354, -201441, -210423, -220528, -232181, -245984, -263191, -263191, -263191
};
*/

//52M TSX
static const int v2t_table[256] = {
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 280022, 264541, 250827, 238540, 227437, 217248, 207794,
	199075, 190918, 183219, 175974, 169165, 162632, 156430, 150512, 144824, 139369, 134151,
	129116, 124240, 119517, 114923, 110488, 106189, 101977, 97891, 93912, 90023, 86216, 82497,
	78868, 75287, 71782, 68342, 64943, 61612, 58346, 55122, 51944, 48810, 45706, 42657, 39637,
	36640, 33683, 30762, 27866, 24991, 22141, 19308, 16495, 13716, 10947, 8187, 5448, 2719, 0,
	-2676, -5364, -8063, -10745, -13423, -16096, -18774, -21457, -24149, -26796, -29446, -32123,
	-34809, -37502, -40214, -42926, -45643, -48372, -51122, -53890, -56690, -59510, -62348,
	-65214, -68106, -71023, -73966, -76944, -79974, -83043, -86145, -89309, -92532, -95805,
	-99133, -102537, -106000, -109565, -113208, -116941, -120778, -124722, -128784, -132989,
	-137339, -141848, -146539, -151424, -156567, -161964, -167642, -173677, -180078, -187046,
	-194557, -202751, -211799, -222049, -233806, -247837, -265250, -265250, -265250
};

static bool cali_mode, modem_flag, gnss_flag;
static DEFINE_MUTEX(tsen_mutex);
static DEFINE_MUTEX(modem_gnss_mtx);

char *modem_tsen = "tsen_temp";

struct tsen_manager {
	struct device *dev;
	struct regmap *regmap;
	u32 base;
} tsen_mager;

struct ump96xx_tsen {
	struct device *dev;
	struct thermal_zone_device *tz_dev;
	struct regmap *regmap;
	u32 base;
	int id;
};

static int ump96xx_tsensor_read_config(struct ump96xx_tsen *tsen)
{
	int ret;

	ret = regmap_update_bits(tsen->regmap, UMP96XX_XTL_WAIT_CTRL0,
				 UMP96XX_XTL_EN, UMP96XX_XTL_EN);
	if (ret)
		return ret;

	ret = regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL0,
				 UMP96XX_TSEN_CLK_SRC_SEL, UMP96XX_TSEN_CLK_SRC_SEL);
	if (ret)
		return ret;

	ret = regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL1,
				 UMP96XX_TSEN_26M_EN, UMP96XX_TSEN_26M_EN);
	if (ret)
		return ret;

	ret = regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_ADCLDO_EN, UMP96XX_TSEN_ADCLDO_EN);
	if (ret)
		return ret;

	ret = regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL1,
				 UMP96XX_TSEN_SDADC_EN, UMP96XX_TSEN_SDADC_EN);
	if (ret)
		return ret;

	ret = regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_UGBUF_EN, UMP96XX_TSEN_UGBUF_EN);
	if (ret)
		return ret;

	return regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL6,
				 UMP96XX_TSEN_SEL_EN, UMP96XX_TSEN_SEL_EN);
}

static int ump96xx_tsensor_osc_rawdata_read(struct ump96xx_tsen *tsen, int *rawdata)
{
	int ret, th, tl;

	ret = ump96xx_tsensor_read_config(tsen);
	if (ret)
		return ret;

	ret = regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_SEL_CH, UMP96XX_TSEN_SEL_CH);
	if (ret)
		return ret;

	ret = regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_EN, UMP96XX_TSEN_EN);
	if (ret)
		return ret;

	/*
	 * According to the requirements of design document,
	 * trigger a data sample and wait data ready need wait 21ms
	 */
	msleep(21);

	ret = regmap_read(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL5, &tl);
	if (ret)
		return ret;


	ret = regmap_read(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL6, &th);
	if (ret)
		return ret;

	*rawdata = UMP96XX_TSEN_OSC_TEMP(th, tl);

	return ret;
}

static int ump96xx_tsensor_osc_temp_read(struct ump96xx_tsen *tsen, int *temp)
{
	int ret, rawdata;

	mutex_lock(&tsen_mutex);
	ret = ump96xx_tsensor_osc_rawdata_read(tsen, &rawdata);
	mutex_unlock(&tsen_mutex);
	if (ret)
		return ret;

	/*
	 * According to the requirements of design document,
	 * tsensor osc temp = 3641500 - (4856 * rawdata) / 1000
	 */
	*temp = 3641500 - (((4856 * (rawdata >> 4)) / 1000) << 4);

	return ret;
}

static int ump96xx_tsensor_out_rawdata_read(struct ump96xx_tsen *tsen, int *rawdata)
{
	int ret, th, tl;

	ret = ump96xx_tsensor_read_config(tsen);
	if (ret)
		return ret;

	ret = regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_SEL_CH, 0x0);
	if (ret)
		return ret;

	ret = regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_EN, UMP96XX_TSEN_EN);
	if (ret)
		return ret;

	/*
	 * According to the requirements of design document,
	 * trigger a data sample and wait data ready need wait 21ms
	 */
	msleep(21);

	ret = regmap_read(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL4, &tl);
	if (ret)
		return ret;

	ret = regmap_read(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL6, &th);
	if (ret)
		return ret;

	*rawdata = UMP96XX_TSEN_TSEN_TEMP(th, tl);

	return ret;
}

static int ump96xx_tsensor_out_temp_read(struct ump96xx_tsen *tsen, int *temp)
{
	int ret, rawdata, index, frac, t;

	mutex_lock(&tsen_mutex);
	ret = ump96xx_tsensor_out_rawdata_read(tsen, &rawdata);
	mutex_unlock(&tsen_mutex);
	if (ret)
		return ret;

	index = (rawdata >> UMP96XX_TSEN_INDEX_SHIFT) & UMP96XX_TSEN_INDEX_MASK;
	frac = rawdata & UMP96XX_TSEN_FRAC_MASK;

	/*
	 * According to the requirements of design document,get the index accrding
	 * to the integral result, and query the current temperature value according to
	 * the index.
	 * t = (v2t_table[index] * (0x1000-frac) + v2t_table[index+1] * frac + 0x800) / 4096;
	 */
	if (index != sizeof(v2t_table) / 4 - 1)
		t = (v2t_table[index] * (0x1000 - frac) +
			v2t_table[index + 1] * frac + 0x800) / 4096;
	else
		t = v2t_table[index];

	/*
	 * According to the requirements of design document,
	 * tsensor out temp = (t * 1000) / 4096 + 25000
	 */
	*temp = (t * 1000) / 4096 + 25000;

	return ret;
}

static int ump96xx_tsensor_disable(struct regmap *regmap, u32 base)
{
	int ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL0,
				 UMP96XX_TSEN_CLK_SRC_SEL, 0);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_EN | UMP96XX_TSEN_SEL_CH, 0);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL6,
				 UMP96XX_TSEN_SEL_EN, 0);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL1,
				 UMP96XX_TSEN_SDADC_EN, 0);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_UGBUF_EN, 0);
	if (ret)
		return ret;

	return regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL3,
				  UMP96XX_TSEN_ADCLDO_EN, 0);
}

static int get_boot_mode(void)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (ret)
		return ret;

	if (strstr(cmd_line, "sprdboot.mode=cali"))
		cali_mode = true;
	else if (strstr(cmd_line, "androidboot.mode=cali"))
		cali_mode = true;
	else
		cali_mode = false;

	return 0;
}

static int ump96xx_tsensor_get_temp(void *data, int *temp)
{
	struct ump96xx_tsen *tsen = data;
	int ret;

	if (tsen->id == 0)
		ret = ump96xx_tsensor_osc_temp_read(tsen, temp);
	else if (tsen->id == 1)
		ret = ump96xx_tsensor_out_temp_read(tsen, temp);
	else
		ret = -ENOTSUPP;

	return ret;
}

static const struct thermal_zone_of_device_ops tsensor_thermal_ops = {
	.get_temp = ump96xx_tsensor_get_temp,
};

static int tsen_enable(struct regmap *regmap, unsigned int base)
{
	unsigned int value = 0, tmp = 0;
	int ret = 0;

	if (modem_flag | gnss_flag) {
		pr_info("one device has enabled tsen\n");
		return ret;
	}

	ret = regmap_update_bits(regmap, UMP96XX_XTL_WAIT_CTRL0,
				 UMP96XX_XTL_EN, UMP96XX_XTL_EN);
	regmap_read(regmap, UMP96XX_XTL_WAIT_CTRL0, &value);
	pr_info("addr=0x%x, wait_ctrl0=0x%x\n", UMP96XX_XTL_WAIT_CTRL0, value);
	if (ret) {
		pr_err("update XTL_WAIT_CTRL0 error\n");
		return ret;
	}

	ret = regmap_update_bits(regmap, (base + UMP96XX_TSEN_CTRL0),
				 UMP96XX_TSEN_CLK_SRC_SEL, UMP96XX_TSEN_CLK_SRC_SEL);
	regmap_read(regmap, base + UMP96XX_TSEN_CTRL0, &value);
	pr_info("addr=0x%x, tsen_ctrl0=0x%x\n", base + UMP96XX_TSEN_CTRL0, value);
	if (ret) {
		pr_err("update TSEN_CTRL0 error\n");
		return ret;
	}

	ret = regmap_read(regmap, (base + UMP96XX_TSEN_CTRL1), &value);
	if (ret) {
		pr_err("read TSEN_CTRL1 error\n");
		return ret;
	};
	tmp = value | UMP96XX_TSEN_26M_EN | UMP96XX_TSEN_SDADC_EN;
	ret = regmap_write(regmap, (base + UMP96XX_TSEN_CTRL1), tmp);
	regmap_read(regmap, base + UMP96XX_TSEN_CTRL1, &value);
	pr_info("addr=0x%x, tsen_ctrl1=0x%x\n", base + UMP96XX_TSEN_CTRL1,
	       value);
	if (ret) {
		pr_err("write TSEN_CTRL1 error\n");
		return ret;
	};

	ret = regmap_read(regmap, (base + UMP96XX_TSEN_CTRL3), &value);
	if (ret) {
		pr_err("read TSEN_CTRL3 error\n");
		return ret;
	}
	tmp = value | UMP96XX_TSEN_ADCLDO_EN | UMP96XX_TSEN_UGBUF_EN | UMP96XX_TSEN_EN;
	ret = regmap_write(regmap, (base + UMP96XX_TSEN_CTRL3), tmp);
	regmap_read(regmap, base + UMP96XX_TSEN_CTRL3, &value);
	pr_info("addr=0x%x, tsen_ctrl3=0x%x\n", base + UMP96XX_TSEN_CTRL3,
	       value);
	if (ret) {
		pr_err("write TSEN_CTRL3 error\n");
		return ret;
	}

	regmap_read(regmap, base + UMP96XX_TSEN_CTRL6, &value);
	tmp = value & (~UMP96XX_TSEN_SEL_EN);
	ret = regmap_write(regmap, base + UMP96XX_TSEN_CTRL6, tmp);
	regmap_read(regmap, base + UMP96XX_TSEN_CTRL6, &value);
	pr_info("%s, CTRL6=0x%x\n", __func__, value);
	if (ret)
		pr_err("write TSEN_CTRL6 error\n");
	return ret;
}

static int tsen_disable(struct regmap *regmap, unsigned int base)
{
	unsigned int value, tmp;
	int ret = 0;

	if (!modem_flag && !gnss_flag) {
		ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL0,
					 UMP96XX_TSEN_CLK_SRC_SEL, 0);
		if (ret) {
			pr_err("dis: update TSEN_CTRL0 error\n");
			return ret;
		}

		ret = regmap_read(regmap, (base + UMP96XX_TSEN_CTRL1), &value);
		if (ret) {
			pr_err("dis:read TSEN_CTRL1 error\n");
			return ret;
		}
		tmp = UMP96XX_TSEN_26M_EN | UMP96XX_TSEN_SDADC_EN;
		tmp = value & (~tmp);
		ret = regmap_write(regmap, (base + UMP96XX_TSEN_CTRL1), tmp);
		if (ret) {
			pr_err("dis:write TSEN_CTRL1 error\n");
			return ret;
		}

		ret = regmap_read(regmap, (base + UMP96XX_TSEN_CTRL3), &value);
		if (ret) {
			pr_err("dis:read TSEN_CTRL3 error\n");
			return ret;
		}
		tmp = UMP96XX_TSEN_ADCLDO_EN | UMP96XX_TSEN_UGBUF_EN | UMP96XX_TSEN_EN;
		tmp = value & (~tmp);
		ret = regmap_write(regmap, (base + UMP96XX_TSEN_CTRL3), tmp);
		if (ret) {
			pr_err("dis:write TSEN_CTRL3 error\n");
			return ret;
		}

		ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL6,
				UMP96XX_TSEN_SEL_EN, 0);
		if (ret) {
			pr_err("dis:write TSEN_CTRL6 error\n");
			return ret;
		}
	} else
		pr_debug("one of device need use tsen, do not close!\n");

	return ret;
}

int gnss_tsen_control(struct regmap *regmap, unsigned int base, bool en)
{
	int ret = -1;

	mutex_lock(&modem_gnss_mtx);
	if (en) {
		ret = tsen_enable(regmap, base);
		if (!ret)
			gnss_flag = true;
	} else {
		/*whether it close success or not, flag need to be false,
		 * then modem can close
		 */
		gnss_flag = false;
		ret = tsen_disable(regmap, base);
	}
	mutex_unlock(&modem_gnss_mtx);
	return ret;
}
EXPORT_SYMBOL_GPL(gnss_tsen_control);

static int modem_tsen_control(struct regmap *regmap, unsigned int base, bool en)
{
	int ret = -1;

	mutex_lock(&modem_gnss_mtx);
	if (en) {
		ret = tsen_enable(regmap, base);
		if (!ret)
			modem_flag = true;
	} else {
		modem_flag = false;
		ret = tsen_disable(regmap, base);
	}
	mutex_unlock(&modem_gnss_mtx);
	return ret;
}

static int modem_tsen_read(struct regmap *regmap, unsigned int base, int *temp)
{
	int ret = 0, th = 0, tl = 0, rawdata = 0;
	int index, frac, t, i = 0;

	mutex_lock(&tsen_mutex);
	do {
		mdelay(10);
		ret = regmap_read(regmap, (base + UMP96XX_TSEN_CTRL4), &tl);
		if (ret) {
			pr_err("read TSEN_CTRL4 error\n");
			return ret;
		}

		ret = regmap_read(regmap, (base +  UMP96XX_TSEN_CTRL6), &th);
		if (ret) {
			pr_err("read TSEN_CTRL6 error\n");
			return ret;
		}

		rawdata = UMP96XX_TSEN_TSEN_TEMP(th, tl);
		pr_debug("rawdata=%d, th=0x%x, tl=0x%x\n", rawdata, th, tl);

		index = (rawdata >> UMP96XX_TSEN_INDEX_SHIFT) & UMP96XX_TSEN_INDEX_MASK;
		frac = rawdata & UMP96XX_TSEN_FRAC_MASK;
		if (index != sizeof(v2t_table) / 4 - 1)
			t = (v2t_table[index] * (0x1000 - frac) +
				v2t_table[index + 1] * frac + 0x800) / 4096;
		else
			t = v2t_table[index];
		*temp = (t * 1000) / 4096 + 25000;

		i++;
		pr_info("i=%d, temp=%d\n", i, *temp);
	} while ((rawdata == 0) && (i < 5));
	mutex_unlock(&tsen_mutex);
	return ret;
}

static ssize_t modem_tsen_show(struct device_driver *dev, char *buf)
{
	int ret = -1, tsen_temp = ABNORMAL_TEMP;
	int len = 0;

	ret = modem_tsen_control(tsen_mager.regmap, tsen_mager.base, true);
	if (ret)
		return ret;

	ret = modem_tsen_read(tsen_mager.regmap, tsen_mager.base, &tsen_temp);

	ret = modem_tsen_control(tsen_mager.regmap, tsen_mager.base, false);
	if (!ret)
		pr_info("close success\n");
	len = sizeof(buf);

	return snprintf(buf, len, "%d\n", tsen_temp);
}

static DRIVER_ATTR_RO(modem_tsen);

static int ump96xx_tsen_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *sen_child;
	struct ump96xx_tsen *tsen;
	struct regmap *regmap;
	u32 base;
	int ret = 0;

	ret = get_boot_mode();
	if (ret) {
		pr_err("boot_mode can't not parse bootargs property\n");
		return ret;
	}

	regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!regmap) {
		dev_err(&pdev->dev, "failed to get efuse regmap\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "reg", &base);
	if (ret) {
		dev_err(&pdev->dev, "failed to get efuse base address\n");
		return ret;
	}

	tsen_mager.dev = &pdev->dev;
	tsen_mager.regmap = regmap;
	tsen_mager.base = base;

	if (cali_mode) {
		for_each_child_of_node(np, sen_child) {
			tsen = devm_kzalloc(&pdev->dev, sizeof(*tsen), GFP_KERNEL);
			if (!tsen)
				return -ENOMEM;

			tsen->regmap = regmap;
			tsen->base = base;

			ret = of_property_read_u32(sen_child, "reg", &tsen->id);
			if (ret) {
				dev_err(&pdev->dev, "get sensor reg failed");
				return ret;
			}
			tsen->tz_dev = thermal_zone_of_sensor_register(&pdev->dev, tsen->id,
								       tsen, &tsensor_thermal_ops);
			if (IS_ERR(tsen->tz_dev)) {
				ret = PTR_ERR(tsen->tz_dev);
				dev_err(&pdev->dev, "Thermal zone register failed\n");
				return ret;
			}
		}
		return ump96xx_tsensor_disable(regmap, base);
	}
	return ret;
}

//module_platform_driver(ump96xx_tsen_driver);
static const struct of_device_id ump96xx_tsen_of_match[] = {
	{ .compatible = "sprd,ump9622-tsensor",},
	{ }
};

static struct platform_driver ump96xx_tsen_driver = {
	.probe = ump96xx_tsen_probe,
	.driver = {
		.name = "ump96xx-tsensor",
		.of_match_table = ump96xx_tsen_of_match,
	},
};

static int __init ump96xx_tsen_init(void)
{
	int ret;

	ret = platform_driver_register(&ump96xx_tsen_driver);
	if (ret)
		return ret;
	ret = driver_create_file(&ump96xx_tsen_driver.driver, &driver_attr_modem_tsen);
	if (ret < 0) {
		pr_err("can not create sysfs file\n");
		platform_driver_unregister(&ump96xx_tsen_driver);
	}
	return ret;
}
module_init(ump96xx_tsen_init);

static void __exit ump96xx_tsen_exit(void)
{
	driver_remove_file(&ump96xx_tsen_driver.driver, &driver_attr_modem_tsen);
	platform_driver_unregister(&ump96xx_tsen_driver);
}

module_exit(ump96xx_tsen_exit);

MODULE_DESCRIPTION("Spreadtrum UMP96xx tsensor driver");
MODULE_LICENSE("GPL v2");
