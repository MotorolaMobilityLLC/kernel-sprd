/*
 * ASoC SPRD sound card support
 *
 * Copyright (C) 2015 Renesas Solutions Corp.
 * Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt("BOARD")""fmt

#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include "sprd-asoc-card-utils.h"
#include "sprd-asoc-common.h"

static int hook_general_spk(int id, int on);

struct sprd_asoc_ext_hook_map {
	const char *name;
	sprd_asoc_hook_func hook;
	int en_level;
};

enum {
	/* ext_ctrl_type */
	CELL_CTRL_TYPE,
	/* pa type select */
	CELL_HOOK,
	/* select mode */
	CELL_PRIV,
	/* share gpio with  */
	CELL_SHARE_GPIO,
	CELL_NUMBER,
};

struct sprd_asoc_hook_spk_priv {
	int gpio[BOARD_FUNC_MAX];
	int priv_data[BOARD_FUNC_MAX];
	int pa_state[BOARD_FUNC_MAX];
	spinlock_t lock;
};

static struct sprd_asoc_hook_spk_priv hook_spk_priv;

#define GENERAL_SPK_MODE 10

#define EN_LEVEL 1

#define CONFIG_SND_FS1512N

#ifdef CONFIG_SND_FS1512N
#define FS1512N_START  300
#define FS1512N_PULSE_DELAY_US 20
#define FS1512N_T_WORK  300
#define FS1512N_T_PWD  100

static int det_type = 0;

void fs15xx_shutdown(unsigned int gpio)
{
	spinlock_t *lock = &hook_spk_priv.lock;
	unsigned long flags;

	spin_lock_irqsave(lock, flags);
	gpio_set_value( gpio, 0);
	mdelay(FS1512N_T_PWD);
	spin_unlock_irqrestore(lock, flags);

}

int hook_gpio_pulse_control_FS1512N(unsigned int gpio,int mode)
{
	unsigned long flags;
	spinlock_t *lock = &hook_spk_priv.lock;
	int count;
	int ret = 0;

	pr_info("%s : mode: %d-->%d\n", __func__,gpio, mode);

	// switch mode online, need shut down pa firstly
	fs15xx_shutdown(gpio);

	// enable pa into work mode
	// make sure idle mode: gpio output low
	gpio_direction_output(gpio, 0);
	spin_lock_irqsave(lock, flags);
	// 1. send T-sta
	gpio_set_value( gpio, 1);
	udelay(FS1512N_START);
	gpio_set_value( gpio, 0);
	udelay(FS1512N_PULSE_DELAY_US); // < 140us

	// 2. send mode
	count = mode - 1;
	while (count > 0) { // count of pulse
		gpio_set_value( gpio, 1);
		udelay(FS1512N_PULSE_DELAY_US); // < 140us 10-150
		gpio_set_value( gpio, 0);
		udelay(FS1512N_PULSE_DELAY_US); // < 140us
		count--;
	}

	// 3. pull up gpio and delay, enable pa
	gpio_set_value( gpio, 1);
	spin_unlock_irqrestore(lock, flags);
	udelay(FS1512N_T_WORK); // pull up gpio > 220us

	return ret;
}
#endif

static int select_mode;

static ssize_t select_mode_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buff)
{
	return sprintf(buff, "%d\n", select_mode);
}

static ssize_t select_mode_store(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 const char *buff, size_t len)
{
	unsigned long level;
	int ret;
	int i= 0;

	ret = kstrtoul(buff, 10, &level);
	if (ret) {
		pr_err("%s kstrtoul failed!(%d)\n", __func__, ret);
		return len;
	}
	if (select_mode == level) {
		pr_info("mode has already been %d, return\n", select_mode);
		return len;
	}
	select_mode = level;
	pr_info("speaker ext pa select_mode = %d\n", select_mode);
	for(i=0;i<BOARD_FUNC_MAX;i++){
		if(hook_spk_priv.pa_state[i] > 0){
			hook_general_spk(i, 0);
			hook_general_spk(i, 1);
			pr_info("reopen pa[%d] by select_mode:%d\n", i, select_mode);
		}
	}
	return len;
}

#ifdef CONFIG_SND_FS1512N
static ssize_t pa_info_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buff)
{
	return sprintf(buff, "%s\n", det_type?"awxxx":"fs15xx");
}
#endif

static int ext_debug_sysfs_init(void)
{
	int ret;
	static struct kobject *ext_debug_kobj;
	static struct kobj_attribute ext_debug_attr =
		__ATTR(select_mode, 0644,
		select_mode_show,
		select_mode_store);

#ifdef CONFIG_SND_FS1512N
	static struct kobj_attribute ext_info_attr =
		__ATTR(pa_info, 0644,
		pa_info_show,
		NULL);
#endif

	if (ext_debug_kobj)
		return 0;
	ext_debug_kobj = kobject_create_and_add("extpa", kernel_kobj);
	if (ext_debug_kobj == NULL) {
		ret = -ENOMEM;
		pr_err("register sysfs failed. ret = %d\n", ret);
		return ret;
	}

	ret = sysfs_create_file(ext_debug_kobj, &ext_debug_attr.attr);
	if (ret) {
		pr_err("create sysfs failed. ret = %d\n", ret);
		return ret;
	}

#ifdef CONFIG_SND_FS1512N
	ret = sysfs_create_file(ext_debug_kobj, &ext_info_attr.attr);
	if (ret) {
		pr_err("create sysfs failed. ret = %d\n", ret);
		return ret;
	}
#endif
	return ret;
}

static void hook_gpio_pulse_control(unsigned int gpio, unsigned int mode)
{
	int i = 1;
	spinlock_t *lock = &hook_spk_priv.lock;
	unsigned long flags;

	spin_lock_irqsave(lock, flags);
	for (i = 1; i < mode; i++) {
		gpio_set_value(gpio, EN_LEVEL);
		udelay(2);
		gpio_set_value(gpio, !EN_LEVEL);
		udelay(2);
	}

	gpio_set_value(gpio, EN_LEVEL);
	spin_unlock_irqrestore(lock, flags);
}

static int hook_general_spk(int id, int on)
{
	int gpio, mode;

	gpio = hook_spk_priv.gpio[id];
	if (gpio < 0) {
		pr_err("%s gpio is invalid!\n", __func__);
		return -EINVAL;
	}
	mode = hook_spk_priv.priv_data[id];
	if (mode > GENERAL_SPK_MODE)
		mode = 0;
	pr_info("%s id: %d, gpio: %d, mode: %d, on: %d\n",
		 __func__, id, gpio, mode, on);

	/* Off */
	if (!on) {
#ifdef CONFIG_SND_FS1512N
	if(!det_type){
		fs15xx_shutdown(gpio);
	}else{
		gpio_set_value(gpio,!EN_LEVEL);
	}
#else
		gpio_set_value(gpio,!EN_LEVEL);
#endif
		hook_spk_priv.pa_state[id] = 0;
		return HOOK_OK;
	}

	/* On */
	if (select_mode) {
		mode = select_mode;
		pr_info("%s mode: %d, select_mode: %d\n",
			__func__, mode, select_mode);
	}
	hook_spk_priv.pa_state[id] = mode;
#ifdef CONFIG_SND_FS1512N
	if(!det_type){
		hook_gpio_pulse_control_FS1512N(gpio, mode);
	}else{
		hook_gpio_pulse_control(gpio, mode);
	}
#else
	hook_gpio_pulse_control(gpio, mode);
#endif

	/* When the first time open speaker path and play a very short sound,
	 * the sound can't be heard. So add a delay here to make sure the AMP
	 * is ready.
	 */
	msleep(22);

	return HOOK_OK;
}

static struct sprd_asoc_ext_hook_map ext_hook_arr[] = {
	{"general_speaker", hook_general_spk, EN_LEVEL},
};

static int sprd_asoc_card_parse_hook(struct device *dev,
					 struct sprd_asoc_ext_hook *ext_hook)
{
	struct device_node *np = dev->of_node;
	const char *prop_pa_info = "sprd,spk-ext-pa-info";
	const char *prop_pa_gpio = "sprd,spk-ext-pa-gpio";
	const char *prop_pa_id = "sprd,spk-ext-pa-id-pin";
	int det_gpio;
	int spk_cnt, elem_cnt, i;
	int ret = 0;
	unsigned long gpio_flag;
	unsigned int ext_ctrl_type, share_gpio, hook_sel, priv_data;
	u32 *buf;

	elem_cnt = of_property_count_u32_elems(np, prop_pa_info);
	if (elem_cnt <= 0) {
		dev_info(dev,
			"Count '%s' failed!(%d)\n", prop_pa_info, elem_cnt);
		return -EINVAL;
	}

	if (elem_cnt % CELL_NUMBER) {
		dev_err(dev, "Spk pa info is not a multiple of %d.\n",
			CELL_NUMBER);
		return -EINVAL;
	}

	spk_cnt = elem_cnt / CELL_NUMBER;
	if (spk_cnt > BOARD_FUNC_MAX) {
		dev_warn(dev, "Speaker count %d is greater than %d!\n",
			 spk_cnt, BOARD_FUNC_MAX);
		spk_cnt = BOARD_FUNC_MAX;
	}

	spin_lock_init(&hook_spk_priv.lock);

	buf = devm_kmalloc(dev, elem_cnt * sizeof(u32), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, prop_pa_info, buf, elem_cnt);
	if (ret < 0) {
		dev_err(dev, "Read property '%s' failed!\n", prop_pa_info);
		//return ret;
	}

	for (i = 0; i < spk_cnt; i++) {
		int num = i * CELL_NUMBER;

		/* Get the ctrl type */
		ext_ctrl_type = buf[CELL_CTRL_TYPE + num];
		if (ext_ctrl_type >= BOARD_FUNC_MAX) {
			dev_err(dev, "Ext ctrl type %d is invalid!\n",
				ext_ctrl_type);
			return -EINVAL;
		}

		/* Get the selection of hook function */
		hook_sel = buf[CELL_HOOK + num];
		if (hook_sel >= ARRAY_SIZE(ext_hook_arr)) {
			dev_err(dev,
				"Hook selection %d is invalid!\n", hook_sel);
			return -EINVAL;
		}
		ext_hook->ext_ctrl[ext_ctrl_type] = ext_hook_arr[hook_sel].hook;

		/* Get the private data */
		priv_data = buf[CELL_PRIV + num];
		hook_spk_priv.priv_data[ext_ctrl_type] = priv_data;

		/* Process the shared gpio */
		share_gpio = buf[CELL_SHARE_GPIO + num];
		if (share_gpio > 0) {
			if (share_gpio > spk_cnt) {
				dev_err(dev, "share_gpio %d is bigger than spk_cnt!\n",
					share_gpio);
				ext_hook->ext_ctrl[ext_ctrl_type] = NULL;
				return -EINVAL;
			}
			hook_spk_priv.gpio[ext_ctrl_type] =
				hook_spk_priv.gpio[share_gpio - 1];
			continue;
		}

		ret = of_get_named_gpio_flags(np, prop_pa_gpio, i, NULL);
		if (ret < 0) {
			dev_err(dev, "Get gpio failed:%d!\n", ret);
			ext_hook->ext_ctrl[ext_ctrl_type] = NULL;
			return ret;
		}
		hook_spk_priv.gpio[ext_ctrl_type] = ret;

		pr_info("ext_ctrl_type %d hook_sel %d priv_data %d gpio %d",
			ext_ctrl_type, hook_sel, priv_data, ret);

		gpio_flag = GPIOF_DIR_OUT | GPIOF_INIT_LOW ;
		ret = gpio_request_one(hook_spk_priv.gpio[ext_ctrl_type],
				       gpio_flag, "audio:pa_ctrl");
		if (ret < 0) {
			dev_err(dev, "Gpio request[%d] failed:%d!\n",
				ext_ctrl_type, ret);
			ext_hook->ext_ctrl[ext_ctrl_type] = NULL;
			return ret;
		}
	hook_spk_priv.pa_state[ext_ctrl_type] = 0;
	}

#ifdef CONFIG_SND_FS1512N
		det_gpio = of_get_named_gpio_flags(np, prop_pa_id, 0, NULL);
		gpio_flag = GPIOF_DIR_IN;
		ret = gpio_request_one(det_gpio, gpio_flag, "audio:pa_id");
		if (ret < 0) {
			dev_err(dev, "det_gpio det request failed:%d!\n", ret);
			return ret;
		}

		det_type = gpio_get_value(det_gpio);
		dev_info(dev, "det_gpio det:%d\n", det_type);

		if(!det_type){
            spin_lock_irqsave(&hook_spk_priv.lock, gpio_flag);
            gpio_set_value(hook_spk_priv.gpio[0], 0);
            udelay(20);
            gpio_set_value(hook_spk_priv.gpio[0], 1);
            udelay(500);
            gpio_set_value(hook_spk_priv.gpio[0], 0);
            spin_unlock_irqrestore(&hook_spk_priv.lock, gpio_flag);
        }
#endif

	return 0;
}
int sprd_asoc_card_parse_ext_hook(struct device *dev,
				  struct sprd_asoc_ext_hook *ext_hook)
{
	ext_debug_sysfs_init();
	return sprd_asoc_card_parse_hook(dev, ext_hook);
}

MODULE_ALIAS("platform:asoc-sprd-card");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASoC SPRD Sound Card Utils - Hooks");
MODULE_AUTHOR("Peng Lee <peng.lee@spreadtrum.com>");
