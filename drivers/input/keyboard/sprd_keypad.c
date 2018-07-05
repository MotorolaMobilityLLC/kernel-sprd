/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/input/matrix_keypad.h>
#include <linux/sysrq.h>
#include <linux/sched.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#define DEBUG_KEYPAD	0

#define KPD_CTRL			(0x00)
#define KPD_INT_EN			(0x04)
#define KPD_INT_RAW_STATUS		(0x08)
#define KPD_INT_MASK_STATUS		(0x0C)
#define KPD_INT_CLR			(0x10)
#define KPD_POLARITY			(0x18)
#define KPD_DEBOUNCE_CNT		(0x1C)
#define KPD_LONG_KEY_CNT		(0x20)
#define KPD_SLEEP_CNT			(0x24)
#define KPD_CLK_DIV_CNT			(0x28)
#define KPD_KEY_STATUS			(0x2C)
#define KPD_SLEEP_STATUS		(0x30)
#define KPD_DEBUG_STATUS1		(0x34)
#define KPD_DEBUG_STATUS2		(0x38)

#define KPD_EN				(0x01 << 0)
#define KPD_SLEEP_EN			(0x01 << 1)
#define KPD_LONG_KEY_EN			(0x01 << 2)

#define KPDCTL_ROW_MSK_V0		(0x3f << 18)	/* enable rows 2 - 7 */
#define KPDCTL_COL_MSK_V0		(0x3f << 10)	/* enable cols 2 - 7 */

#define KPDCTL_ROW_MSK_V1		(0xff << 16)	/* enable rows 0 - 7 */
#define KPDCTL_COL_MSK_V1		(0xff << 8)	/* enable cols 0 - 7 */

#define KPD_INT_ALL			(0xfff)
#define KPD_INT_DOWNUP			(0x0ff)
#define KPD_INT_LONG			(0xf00)
#define KPD_PRESS_INT0			(1 << 0)
#define KPD_PRESS_INT1			(1 << 1)
#define KPD_PRESS_INT2			(1 << 2)
#define KPD_PRESS_INT3			(1 << 3)
#define KPD_RELEASE_INT0		(1 << 4)
#define KPD_RELEASE_INT1		(1 << 5)
#define KPD_RELEASE_INT2		(1 << 6)
#define KPD_RELEASE_INT3		(1 << 7)
#define KPD_LONG_KEY_INT0		(1 << 8)
#define KPD_LONG_KEY_INT1		(1 << 9)
#define KPD_LONG_KEY_INT2		(1 << 10)
#define KPD_LONG_KEY_INT3		(1 << 11)

#define KPD_CFG_ROW_POLARITY		(0xff)
#define KPD_CFG_COL_POLARITY		(0xff00)
#define CFG_ROW_POLARITY		(KPD_CFG_ROW_POLARITY & 0x00ff)
#define CFG_COL_POLARITY		(KPD_CFG_COL_POLARITY & 0xff00)

#define KPD_SLEEP_CNT_VALUE(_X_MS_)	(_X_MS_ * 32.768 - 1)

#define KPD_INT0_COL(_X_)		(((_X_) >> 0) & 0x7)
#define KPD_INT0_ROW(_X_)		(((_X_) >> 4) & 0x7)
#define KPD_INT0_DOWN(_X_)		(((_X_) >> 7) & 0x1)
#define KPD_INT1_COL(_X_)		(((_X_) >> 8) & 0x7)
#define KPD_INT1_ROW(_X_)		(((_X_) >> 12) & 0x7)
#define KPD_INT1_DOWN(_X_)		(((_X_) >> 15) & 0x1)
#define KPD_INT2_COL(_X_)		(((_X_) >> 16) & 0x7)
#define KPD_INT2_ROW(_X_)		(((_X_) >> 20) & 0x7)
#define KPD_INT2_DOWN(_X_)		(((_X_) >> 23) & 0x1)
#define KPD_INT3_COL(_X_)		(((_X_) >> 24) & 0x7)
#define KPD_INT3_ROW(_X_)		(((_X_) >> 28) & 0x7)
#define KPD_INT3_DOWN(_X_)		(((_X_) >> 31) & 0x1)

enum sprd_kpd_ver_type {
	SPRD_KPD_VER0,
	SPRD_KPD_VER1,
};

struct sprd_keypad_platform_data {
	int rows_choose_hw;	/* choose chip keypad controller rows */
	int cols_choose_hw;	/* choose chip keypad controller cols */
	int rows_number;	/*How many rows are there in board. */
	int cols_number;	/*How many cols are there in board. */
	const struct matrix_keymap_data *keymap_data;
	int support_long_key;
	unsigned short repeat;
	unsigned int debounce_time;	/* in ns */
	int wakeup;
	unsigned int glb_reset_off;
	unsigned int glb_eb_off;
	unsigned int glb_rtc_eb_off;
	unsigned int bit_kpd_soft_rst;
	unsigned int bit_kpd_eb;
	unsigned int bit_kpd_rtc_eb;
};

struct sprd_keypad_t {
	struct sprd_keypad_platform_data *pdata;
	struct input_dev *input_dev;
	struct regmap *aon_apb_glb;
	unsigned long base;
	int irq;
	int cols;
};

static struct sprd_keypad_t *sprd_kpd;

static inline unsigned int aon_kpd_reg_read(unsigned long reg)
{
	return readl_relaxed((void __iomem *)(reg + sprd_kpd->base));
}

static inline void aon_kpd_reg_write(unsigned int or_val, unsigned long reg)
{
	writel_relaxed(or_val, (void __iomem *)(reg + sprd_kpd->base));
}

static void keypad_enable(void)
{
	struct sprd_keypad_platform_data *pdata = sprd_kpd->pdata;

	regmap_update_bits(sprd_kpd->aon_apb_glb, pdata->bit_kpd_soft_rst,
			pdata->bit_kpd_soft_rst, pdata->bit_kpd_soft_rst);
	udelay(5); /* add 5us here */
	regmap_update_bits(sprd_kpd->aon_apb_glb, pdata->bit_kpd_soft_rst,
			pdata->bit_kpd_soft_rst, ~pdata->bit_kpd_soft_rst);
	udelay(5); /* add 5us here */
	regmap_update_bits(sprd_kpd->aon_apb_glb, pdata->glb_rtc_eb_off,
			pdata->bit_kpd_rtc_eb, pdata->bit_kpd_rtc_eb);
	udelay(5); /* add 5us here */
	regmap_update_bits(sprd_kpd->aon_apb_glb, pdata->glb_eb_off,
			pdata->bit_kpd_eb, pdata->bit_kpd_eb);
}

static void keypad_disable(void)
{
	struct sprd_keypad_platform_data *pdata = sprd_kpd->pdata;

	regmap_update_bits(sprd_kpd->aon_apb_glb, pdata->glb_rtc_eb_off,
			pdata->bit_kpd_rtc_eb, ~pdata->bit_kpd_rtc_eb);
	regmap_update_bits(sprd_kpd->aon_apb_glb, pdata->glb_eb_off,
			pdata->bit_kpd_eb, ~pdata->bit_kpd_eb);
}

#if DEBUG_KEYPAD
static void dump_keypad_register(void)
{
	pr_info("REG_KPD_CTRL = 0x%08x\n",
		aon_kpd_reg_read(KPD_CTRL));
	pr_info("REG_KPD_INT_EN = 0x%08x\n",
		aon_kpd_reg_read(KPD_INT_EN));
	pr_info("REG_KPD_INT_RAW_STATUS = 0x%08x\n",
		aon_kpd_reg_read(KPD_INT_RAW_STATUS));
	pr_info("REG_KPD_INT_MASK_STATUS = 0x%08x\n",
		aon_kpd_reg_read(KPD_INT_MASK_STATUS));
	pr_info("REG_KPD_INT_CLR = 0x%08x\n",
		aon_kpd_reg_read(KPD_INT_CLR));
	pr_info("REG_KPD_POLARITY = 0x%08x\n",
		aon_kpd_reg_read(KPD_POLARITY));
	pr_info("REG_KPD_DEBOUNCE_CNT = 0x%08x\n",
		aon_kpd_reg_read(KPD_DEBOUNCE_CNT));
	pr_info("REG_KPD_LONG_KEY_CNT = 0x%08x\n",
		aon_kpd_reg_read(KPD_LONG_KEY_CNT));
	pr_info("REG_KPD_SLEEP_CNT = 0x%08x\n",
		aon_kpd_reg_read(KPD_SLEEP_CNT));
	pr_info("REG_KPD_CLK_DIV_CNT = 0x%08x\n",
		aon_kpd_reg_read(KPD_CLK_DIV_CNT));
	pr_info("REG_KPD_KEY_STATUS = 0x%08x\n",
		aon_kpd_reg_read(KPD_KEY_STATUS));
	pr_info("REG_KPD_SLEEP_STATUS = 0x%08x\n",
		aon_kpd_reg_read(KPD_SLEEP_STATUS));
}
#else
static void dump_keypad_register(void)
{
}
#endif

static irqreturn_t sprd_keypad_isr(int irq, void *dev_id)
{
	unsigned short key = 0;
	unsigned long int_status = aon_kpd_reg_read(KPD_INT_MASK_STATUS);
	unsigned long key_status = aon_kpd_reg_read(KPD_KEY_STATUS);
	unsigned short *keycodes = sprd_kpd->input_dev->keycode;
	unsigned int row_shift = get_count_order(sprd_kpd->cols);
	int col, row;

	aon_kpd_reg_write(KPD_INT_ALL, KPD_INT_CLR);

	if (int_status & KPD_PRESS_INT0) {
		col = KPD_INT0_COL(key_status);
		row = KPD_INT0_ROW(key_status);
		key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];
		input_report_key(sprd_kpd->input_dev, key, 1);
		input_sync(sprd_kpd->input_dev);
		pr_info("%03dD\n", key);
	}

	if (int_status & KPD_RELEASE_INT0) {
		col = KPD_INT0_COL(key_status);
		row = KPD_INT0_ROW(key_status);
		key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];
		input_report_key(sprd_kpd->input_dev, key, 0);
		input_sync(sprd_kpd->input_dev);
		pr_info("%03dU\n", key);
	}

	if (int_status & KPD_PRESS_INT1) {
		col = KPD_INT1_COL(key_status);
		row = KPD_INT1_ROW(key_status);
		key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];
		input_report_key(sprd_kpd->input_dev, key, 1);
		input_sync(sprd_kpd->input_dev);
		pr_info("%03dD\n", key);
	}

	if (int_status & KPD_RELEASE_INT1) {
		col = KPD_INT1_COL(key_status);
		row = KPD_INT1_ROW(key_status);
		key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];
		input_report_key(sprd_kpd->input_dev, key, 0);
		input_sync(sprd_kpd->input_dev);
		pr_info("%03dU\n", key);
	}

	if (int_status & KPD_PRESS_INT2) {
		col = KPD_INT2_COL(key_status);
		row = KPD_INT2_ROW(key_status);
		key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];
		input_report_key(sprd_kpd->input_dev, key, 1);
		input_sync(sprd_kpd->input_dev);
		pr_info("%03d\n", key);
	}

	if (int_status & KPD_RELEASE_INT2) {
		col = KPD_INT2_COL(key_status);
		row = KPD_INT2_ROW(key_status);
		key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];
		input_report_key(sprd_kpd->input_dev, key, 0);
		input_sync(sprd_kpd->input_dev);
		pr_info("%03d\n", key);
	}

	if (int_status & KPD_PRESS_INT3) {
		col = KPD_INT3_COL(key_status);
		row = KPD_INT3_ROW(key_status);
		key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];
		input_report_key(sprd_kpd->input_dev, key, 1);
		input_sync(sprd_kpd->input_dev);
		pr_info("%03d\n", key);
	}

	if (int_status & KPD_RELEASE_INT3) {
		col = KPD_INT3_COL(key_status);
		row = KPD_INT3_ROW(key_status);
		key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];
		input_report_key(sprd_kpd->input_dev, key, 0);
		input_sync(sprd_kpd->input_dev);
		pr_info("%03d\n", key);
	}

	return IRQ_HANDLED;
}

static struct sprd_keypad_platform_data *sprd_keypad_parse_dt(
			struct device *dev)
{
	struct sprd_keypad_platform_data *pdata;
	struct device_node *np = dev->of_node, *key_np;
	uint32_t num_rows, num_cols;
	uint32_t rows_choose_hw, cols_choose_hw;
	uint32_t debounce_time;
	struct matrix_keymap_data *keymap_data;
	uint32_t *keymap, key_count;
	int ret;
	unsigned int glb_reg = 0;
	struct regmap *kpd_glb = NULL;

	kpd_glb = syscon_regmap_lookup_by_phandle(np, "sprd,syscon-enable");
	if (IS_ERR(kpd_glb)) {
		dev_err(dev, "ap kpd syscon failed!\n");
		return NULL;
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	sprd_kpd->aon_apb_glb = kpd_glb;

	ret = of_property_read_u32(np, "sprd,glb-rst-off", &glb_reg);
	if (ret) {
		dev_err(dev, "no sprd,glb-rst-off of property specified\n");
		goto fail;
	}
	pdata->glb_reset_off = glb_reg;

	ret = of_property_read_u32(np, "sprd,glb-eb-off", &glb_reg);
	if (ret) {
		dev_err(dev, "no sprd,glb-eb-off of property specified\n");
		goto fail;
	}
	pdata->glb_eb_off = glb_reg;

	ret = of_property_read_u32(np, "sprd,glb-rtc-eb-off", &glb_reg);
	if (ret) {
		dev_err(dev, "no sprd,glb-rtc-eb-off of property specified\n");
		goto fail;
	}
	pdata->glb_rtc_eb_off = glb_reg;

	ret = of_property_read_u32(np, "sprd,bit-kpd-soft-rst", &glb_reg);
	if (ret) {
		dev_err(dev, "no sprd,bit-kpd-soft-rst of property specified\n");
		goto fail;
	}
	pdata->bit_kpd_soft_rst = glb_reg;

	ret = of_property_read_u32(np, "sprd,bit-kpd-eb", &glb_reg);
	if (ret) {
		dev_err(dev, "no sprd,bit-kpd-eb of property specified\n");
		goto fail;
	}
	pdata->bit_kpd_eb = glb_reg;

	ret = of_property_read_u32(np, "sprd,bit-kpd-rtc-eb", &glb_reg);
	if (ret) {
		dev_err(dev, "no sprd,bit-kpd-rtc-eb of property specified\n");
		goto fail;
	}
	pdata->bit_kpd_rtc_eb = glb_reg;

	ret = of_property_read_u32(np, "sprd,num-rows", &num_rows);
	if (ret) {
		dev_err(dev, "no sprd,num-rows of property specified\n");
		goto fail;
	}
	pdata->rows_number = num_rows;

	ret = of_property_read_u32(np, "sprd,num-columns", &num_cols);
	if (ret) {
		dev_err(dev, "no sprd,num-columns of property specified\n");
		goto fail;
	}
	pdata->cols_number = num_cols;

	ret = of_property_read_u32(np, "sprd,debounce_time", &debounce_time);
	if (ret)
		debounce_time = 5;

	pdata->debounce_time = debounce_time;

	if (of_get_property(np, "linux,repeat", NULL))
		pdata->repeat = true;
	if (of_get_property(np, "sprd,support_long_key", NULL))
		pdata->support_long_key = true;
	if (of_get_property(np, "linux,input-wakeup", NULL))
		pdata->wakeup = true;

	ret = of_property_read_u32(np, "sprd,rows-choose-hw",
				&rows_choose_hw);
	if (ret) {
		dev_err(dev, "no sprd,rows-choose-hw of property specified\n");
		goto fail;
	}
	pdata->rows_choose_hw = rows_choose_hw;

	ret = of_property_read_u32(np, "sprd,cols-choose-hw",
				&cols_choose_hw);
	if (ret) {
		dev_err(dev, "no sprd,cols-choose-hw of property specified\n");
		goto fail;
	}
	pdata->cols_choose_hw = cols_choose_hw;

	keymap_data = devm_kzalloc(dev, sizeof(*keymap_data), GFP_KERNEL);
	if (!keymap_data)
		goto fail;

	pdata->keymap_data = keymap_data;

	key_count = of_get_child_count(np);
	keymap_data->keymap_size = key_count;
	keymap = devm_kcalloc(dev, key_count, sizeof(uint32_t), GFP_KERNEL);
	if (!keymap) {
		dev_err(dev, "could not allocate memory for keymap\n");
		goto fail_keymap;
	}
	keymap_data->keymap = keymap;

	for_each_child_of_node(np, key_np) {
		u32 row, col, key_code;

		ret = of_property_read_u32(key_np, "sprd,row", &row);
		if (ret)
			goto fail_parse_keymap;
		ret = of_property_read_u32(key_np, "sprd,column", &col);
		if (ret)
			goto fail_parse_keymap;
		ret = of_property_read_u32(key_np, "linux,code", &key_code);
		if (ret)
			goto fail_parse_keymap;
		*keymap++ = KEY(row, col, key_code);
		pr_info("sprd_keypad_parse_dt: %u, %u, %u\n",
			row, col, key_code);
	}

	return pdata;

fail_parse_keymap:
	dev_err(dev, "failed parsing keymap\n");
	devm_kfree(dev, keymap);
	keymap_data->keymap = NULL;
fail_keymap:
	devm_kfree(dev, keymap_data);
	pdata->keymap_data = NULL;
fail:
	devm_kfree(dev, pdata);
	return NULL;
}

static const struct of_device_id keypad_match_table[] = {
	{ .compatible = "sprd,v0-keypad", .data = (void *)SPRD_KPD_VER0, },
	{ .compatible = "sprd,v1-keypad", .data = (void *)SPRD_KPD_VER1, },
	{ },
};
MODULE_DEVICE_TABLE(of, keypad_match_table);

static int sprd_keypad_probe(struct platform_device *pdev)
{
	struct input_dev *input_dev;
	struct sprd_keypad_platform_data *pdata = pdev->dev.platform_data;
	int error;
	unsigned long value;
	struct resource *res = NULL;
	void __iomem *kpd_base = NULL;
	const struct of_device_id *of_id;
	unsigned char kpd_type;

	of_id = of_match_node(keypad_match_table, pdev->dev.of_node);
	if (!of_id) {
		dev_err(&pdev->dev, "get id of ap efuse failed!\n");
		return -ENODEV;
	}
	kpd_type = (enum sprd_kpd_ver_type)of_id->data;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	kpd_base = devm_ioremap_resource(&pdev->dev, res);
	if (!kpd_base) {
		dev_err(&pdev->dev, "remap kpd address failed!\n");
		return -ENODEV;
	}

	sprd_kpd = devm_kzalloc(&pdev->dev, sizeof(struct sprd_keypad_t)
			, GFP_KERNEL);
	if (!sprd_kpd)
		return -ENOMEM;

	if (pdev->dev.of_node && !pdata) {
		pdata = sprd_keypad_parse_dt(&pdev->dev);
		if (pdata)
			pdev->dev.platform_data = pdata;
	}
	if (!pdata) {
		dev_err(&pdev->dev, "sprd_keypad get platform_data failed!\n");
		return -EINVAL;
	}

	input_dev = devm_input_allocate_device(&pdev->dev);
	if (!input_dev) {
		dev_err(&pdev->dev, "failed to allocate input device\n");
		return -ENOMEM;
	}

	sprd_kpd->base = (unsigned long)kpd_base;
	sprd_kpd->pdata = pdata;
	sprd_kpd->input_dev = input_dev;
	sprd_kpd->cols = pdata->cols_number;

	keypad_enable();
	aon_kpd_reg_write(KPD_INT_ALL, KPD_INT_CLR);
	aon_kpd_reg_write(CFG_ROW_POLARITY | CFG_COL_POLARITY, KPD_POLARITY);
	aon_kpd_reg_write(1, KPD_CLK_DIV_CNT);
	aon_kpd_reg_write(0xc, KPD_LONG_KEY_CNT);
	aon_kpd_reg_write(pdata->debounce_time, KPD_DEBOUNCE_CNT);

	sprd_kpd->irq = platform_get_irq(pdev, 0);
	if (sprd_kpd->irq < 0) {
		dev_err(&pdev->dev, "Get irq number error,Keypad Module\n");
		return -ENODEV;
	}

	error = devm_request_irq(&pdev->dev, sprd_kpd->irq, sprd_keypad_isr,
			IRQF_NO_SUSPEND, "sprd-keypad", sprd_kpd);
	if (error) {
		dev_err(&pdev->dev, "unable to claim irq %d\n", sprd_kpd->irq);
		return error;
	}

	input_dev->name = "sprd-keypad";
	input_dev->phys = "sprd-key/input0";
	input_dev->dev.parent = &pdev->dev;
	input_set_drvdata(input_dev, sprd_kpd);

	input_dev->id.bustype = BUS_HOST;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;

	matrix_keypad_build_keymap(pdata->keymap_data, NULL,
				pdata->rows_number, pdata->cols_number,
				NULL, input_dev);

	/* there are keys from hw other than keypad controller */
	__set_bit(KEY_POWER, input_dev->keybit);
	__set_bit(EV_KEY, input_dev->evbit);
	if (pdata->repeat)
		__set_bit(EV_REP, input_dev->evbit);

	error = input_register_device(input_dev);
	if (error) {
		dev_err(&pdev->dev, "unable to register input device\n");
		return error;
	}
	device_init_wakeup(&pdev->dev, 1);
	platform_set_drvdata(pdev, sprd_kpd);

	value = KPD_INT_DOWNUP;
	if (pdata->support_long_key)
		value |= KPD_INT_LONG;
	aon_kpd_reg_write(value, KPD_INT_EN);
	value = KPD_SLEEP_CNT_VALUE(1000);
	aon_kpd_reg_write(value, KPD_SLEEP_CNT);

	if (kpd_type == SPRD_KPD_VER0) {
		if ((pdata->rows_choose_hw & ~KPDCTL_ROW_MSK_V0)
			|| (pdata->cols_choose_hw & ~KPDCTL_COL_MSK_V0)) {
			pr_warn("Error rows_choose_hw Or cols_choose_hw\n");
		} else {
			pdata->rows_choose_hw &= KPDCTL_ROW_MSK_V0;
			pdata->cols_choose_hw &= KPDCTL_COL_MSK_V0;
		}
	} else if (kpd_type == SPRD_KPD_VER1) {
		if ((pdata->rows_choose_hw & ~KPDCTL_ROW_MSK_V1)
			|| (pdata->cols_choose_hw & ~KPDCTL_COL_MSK_V1)) {
			pr_warn("Error rows_choose_hw\n");
		} else {
			pdata->rows_choose_hw &= KPDCTL_ROW_MSK_V1;
			pdata->cols_choose_hw &= KPDCTL_COL_MSK_V1;
		}
	} else {
		pr_warn
			("Don't support this keypad controller now\n");
	}

	value = KPD_EN | KPD_SLEEP_EN |
		pdata->rows_choose_hw | pdata->cols_choose_hw;
	if (pdata->support_long_key)
		value |= KPD_LONG_KEY_EN;
	aon_kpd_reg_write(value, KPD_CTRL);

	dump_keypad_register();
	pr_info("sc_keypad probe success!!");

	return 0;
}

static int sprd_keypad_remove(struct platform_device *pdev)
{
	unsigned long value;
	/* disable sprd keypad controller */
	aon_kpd_reg_write(KPD_INT_ALL, KPD_INT_CLR);
	value = aon_kpd_reg_read(KPD_CTRL);
	value &= ~(1 << 0);
	aon_kpd_reg_write(value, KPD_CTRL);
	keypad_disable();

	free_irq(sprd_kpd->irq, pdev);
	input_unregister_device(sprd_kpd->input_dev);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sprd_keypad_suspend(struct device *dev)
{
	return 0;
}
static int sprd_keypad_resume(struct device *dev)
{
	struct sprd_keypad_platform_data *pdata = sprd_kpd->pdata;
	unsigned long value;

	keypad_enable();
	aon_kpd_reg_write(KPD_INT_ALL, KPD_INT_CLR);
	aon_kpd_reg_write(CFG_ROW_POLARITY | CFG_COL_POLARITY, KPD_POLARITY);
	aon_kpd_reg_write(1, KPD_CLK_DIV_CNT);
	aon_kpd_reg_write(0xc, KPD_LONG_KEY_CNT);
	aon_kpd_reg_write(pdata->debounce_time, KPD_DEBOUNCE_CNT);

	value = KPD_INT_DOWNUP;
	if (pdata->support_long_key)
		value |= KPD_INT_LONG;
	aon_kpd_reg_write(value, KPD_INT_EN);
	value = KPD_SLEEP_CNT_VALUE(1000);
	aon_kpd_reg_write(value, KPD_SLEEP_CNT);

	value = KPD_EN | KPD_SLEEP_EN |
		pdata->rows_choose_hw | pdata->cols_choose_hw;
	if (pdata->support_long_key)
		value |= KPD_LONG_KEY_EN;
	aon_kpd_reg_write(value, KPD_CTRL);

	return 0;
}
#else
#define sprd_keypad_suspend	NULL
#define sprd_keypad_resume	NULL
#endif

static SIMPLE_DEV_PM_OPS(sprd_keypad_pm_ops,
		sprd_keypad_suspend, sprd_keypad_resume);

static struct platform_driver sprd_keypad_driver = {
	.probe		=	sprd_keypad_probe,
	.remove		=	sprd_keypad_remove,
	.driver		= {
		.name	=		"sprd-keypad",
		.pm	=		&sprd_keypad_pm_ops,
		.of_match_table	=	of_match_ptr(keypad_match_table),
		},
};

module_platform_driver(sprd_keypad_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xiaotong.lu@spreadtrum.com");
MODULE_DESCRIPTION("Keypad driver for spreadtrum:questions contact Xiaotong Lu");
