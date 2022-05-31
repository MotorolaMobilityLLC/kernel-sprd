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
#include <linux/kallsyms.h>

#include "sprd-asoc-card-utils.h"
#include "sprd-asoc-common.h"

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
	/* music select mode */
	CELL_PRIV_m,
	/* voice select mode */
	CELL_PRIV_v,
	/* fm speaker select mode */
	CELL_PRIV_f,
	/* share gpio with  */
	CELL_SHARE_GPIO,
	CELL_NUMBER,
};

enum {
	LEFT_CHANNEL = 0,
	RIRHT_CHANNEL = 1,
};

struct sprd_asoc_hook_spk_priv {
	int gpio[BOARD_FUNC_MAX];
	int priv_data_m[BOARD_FUNC_MAX];
	int priv_data_v[BOARD_FUNC_MAX];
	int priv_data_f[BOARD_FUNC_MAX];
	bool gpio_requested[BOARD_FUNC_MAX];
	spinlock_t lock;
};

static struct sprd_asoc_hook_spk_priv hook_spk_priv;

#define GENERAL_SPK_MODE 10

#define EN_LEVEL 1

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/spinlock.h>

#define FS1512N_START  300
#define FS1512N_PULSE_DELAY_US 20
#define FS1512N_T_WORK  300
#define FS1512N_T_PWD  260

static int det_type;
void fs15xx_shutdown(unsigned int gpio)
{
	spinlock_t *lock = &hook_spk_priv.lock;
	unsigned long flags;

	spin_lock_irqsave(lock, flags);
	gpio_set_value(gpio, 0);
	udelay(FS1512N_T_PWD);
	spin_unlock_irqrestore(lock, flags);
}

int hook_gpio_pulse_control_FS1512N(unsigned int gpio, int mode)
{
	unsigned long flags;
	spinlock_t *lock = &hook_spk_priv.lock;
	int count;
	int ret = 0;

	pr_info("mode: %d-->%d\n", gpio, mode);

	// switch mode online, need shut down pa firstly
	fs15xx_shutdown(gpio);

	// enable pa into work mode
	// make sure idle mode: gpio output low
	gpio_direction_output(gpio, 0);
	spin_lock_irqsave(lock, flags);
	// 1. send T-sta
	gpio_set_value(gpio, 1);
	udelay(FS1512N_START);
	gpio_set_value(gpio, 0);
	udelay(FS1512N_PULSE_DELAY_US); // < 140us

	// 2. send mode
	count = mode - 1;
	while (count > 0) { // count of pulse
		gpio_set_value(gpio, 1);
		udelay(FS1512N_PULSE_DELAY_US); // < 140us 10-150
		gpio_set_value(gpio, 0);
		udelay(FS1512N_PULSE_DELAY_US); // < 140us
		count--;
	}

	// 3. pull up gpio and delay, enable pa
	gpio_set_value(gpio, 1);
	// check_intervel_time(fs15xx, 500, 1950);
	spin_unlock_irqrestore(lock, flags);

	udelay(FS1512N_T_WORK); // pull up gpio > 220us
	//fs15xx->spc_mode = mode;

	return ret;
}

static int select_mode;
static u32 extral_iic_pa_en;

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


	ret = kstrtoul(buff, 10, &level);
	if (ret) {
		pr_err("%s kstrtoul failed!(%d)\n", __func__, ret);
		return len;
	}
	select_mode = level;
	pr_info("speaker ext pa select_mode = %d\n", select_mode);

	return len;
}

static ssize_t extpa_info_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buff)
{
	return sprintf(buff, "%s\n", det_type?"fs15xx":"awxxx");
}

static int ext_debug_sysfs_init(void)
{
	int ret;
	static struct kobject *ext_debug_kobj;
	static struct kobj_attribute ext_debug_attr =
		__ATTR(select_mode, 0644,
		select_mode_show,
		select_mode_store);

	static struct kobj_attribute extpa_info_attr =
		__ATTR(extpa_info, 0644,
		extpa_info_show,
		NULL);

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

	ret = sysfs_create_file(ext_debug_kobj, &extpa_info_attr.attr);
	if (ret) {
		pr_err("create sysfs failed. ret = %d\n", ret);
		return ret;
	}

	return ret;
}

/* audio_sense begin */
static int audio_sense;                   // 0:music; 1:voice; 2:fm speaker...
static int hook_state[BOARD_FUNC_MAX] = {0};  // record each hook on/off status

static int hook_general_spk(int id, int on);

int sprd_audio_sense_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int max = mc->max, i;
	unsigned int mask = (1 << fls(max)) - 1;
	int hook_state_tmp[BOARD_FUNC_MAX] = {0};

	if (audio_sense != (ucontrol->value.integer.value[0] & mask)) {
		audio_sense = (ucontrol->value.integer.value[0] & mask);
		sp_asoc_pr_info("%s put 0x%lx\n", __func__, audio_sense);

		/*
		 * if pa is already on,
		 * we need to change pa mode after audio_sense updated
		 */
		for (i = 0; i < BOARD_FUNC_MAX; i++) {
			if (hook_state[i]) {
				hook_state_tmp[i] = hook_state[i];
				hook_general_spk(i, 0);	// close pa firstly
			}
		}
		msleep(50);
		for (i = 0; i < BOARD_FUNC_MAX; i++) {
			if (hook_state_tmp[i]) {
				hook_general_spk(i, 1);	// reopen pa to set different mode
				sp_asoc_pr_info("%s id %d change mode\n", __func__, i);
			}
		}
	} else {
		sp_asoc_pr_info("%s put same 0x%lx\n", __func__, audio_sense);
	}
	return 0;
}

int sprd_audio_sense_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	sp_asoc_pr_info("%s get %d\n", __func__, audio_sense);
	ucontrol->value.enumerated.item[0] = audio_sense;
	return 0;
}
/* audio_sense end */

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
	int mode_m, mode_v, mode_f;

#if 0
	sp_asoc_pr_info("%s enter\n", __func__);
	if (extral_iic_pa_en == 1) {
		static int (*extral_i2c_pa_function)(int);

		extral_i2c_pa_function = (void *)kallsyms_lookup_name("aw87xxx_i2c_pa");
		if (!extral_i2c_pa_function) {
			sp_asoc_pr_info("%s extral_i2c_pa is not prepare\n", __func__);
		} else {
			sp_asoc_pr_info("%s extral_i2c_pa, on %d\n", __func__, on);
			extral_i2c_pa_function(on);
		}
		return HOOK_OK;
	} else if (extral_iic_pa_en == 2) {
		static int (*extral_i2c_pa_function)(int, int);

		extral_i2c_pa_function =
			(void *)kallsyms_lookup_name("aw87xxx_audio_scene_load");
		if (!extral_i2c_pa_function) {
			sp_asoc_pr_info("%s extral_i2c_pa is not prepare\n", __func__);
		} else {
			sp_asoc_pr_info("%s extral_i2c_pa, on %d\n", __func__, on);
			extral_i2c_pa_function(on, LEFT_CHANNEL);
			extral_i2c_pa_function(on, RIRHT_CHANNEL);
		}
		return HOOK_OK;
	}
#endif
	gpio = hook_spk_priv.gpio[id];
	if (gpio < 0) {
		pr_err("%s gpio is invalid!\n", __func__);
		return -EINVAL;
	}

	mode_m = hook_spk_priv.priv_data_m[id];
	if (mode_m > GENERAL_SPK_MODE)
		mode_m = 0;

	mode_v = hook_spk_priv.priv_data_v[id];
	if (mode_v > GENERAL_SPK_MODE)
		mode_v = 0;

	mode_f = hook_spk_priv.priv_data_f[id];
	if (mode_f > GENERAL_SPK_MODE)
		mode_f = 0;

	pr_info("%s id: %d, gpio: %d, mode(m/v/f): %d/%d/%d, on: %d\n",
		 __func__, id, gpio, mode_m, mode_v, mode_f, on);

	/* Off */
	if (!on) {
		gpio_set_value(gpio, !EN_LEVEL);
		msleep(1); // avoid handset/handsfree switched too fast
		hook_state[id] = 0;
		return HOOK_OK;
	}

	switch (audio_sense) {
	case 0:
		mode = mode_m;
		pr_info("%s id: %d MUSIC %d\n", __func__, id, mode);
		break;
	case 1:
		mode = mode_v;
		pr_info("%s id: %d VOICE %d\n", __func__, id, mode);
		break;
	case 2:
		mode = mode_f;
		pr_info("%s id: %d FM SPEAKER %d\n", __func__, id, mode);
		break;
	default:
		pr_info("%s id: %d unknow audio_sense %d, skipped!!\n", __func__, id, audio_sense);
		// not delay
		return HOOK_OK;
	}

	/* On */
	if (select_mode) {
		mode = select_mode;
		pr_info("%s, select_mode: %d\n", __func__, select_mode);
	}
	if (det_type) {
		pr_info("%s det_type: %d, fs19xx, mode %d\n", __func__, det_type, mode);
		hook_gpio_pulse_control_FS1512N(gpio, mode);
	} else {
		pr_info("%s det_type: %d, awxxx, mode %d\n", __func__, det_type, mode);
		hook_gpio_pulse_control(gpio, mode);
	}
	hook_state[id] = mode;

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
	const char *prop_pa_gpio = "sprd,spk-ext-pa-gpio";
	const char *extral_iic_pa_info = "extral-iic-pa";
	char prop_pa_info[32] = "sprd,spk-ext-pa-info";
	const char *prop_pa_id_pin = "sprd,spk-ext-pa-id-pin";
	int det_gpio;
	int det_flag = -1;
	u32 expect_det_flag;
	int spk_cnt, elem_cnt, i;
	int ret = 0;
	unsigned long gpio_flag;
	unsigned int ext_ctrl_type, share_gpio, hook_sel, priv_data;
	u32 *buf;
	u32 extral_iic_pa = 0;

	det_gpio = of_get_named_gpio_flags(np, prop_pa_id_pin, 0, NULL);
	if (det_gpio < 0) {
		dev_err(dev, "%s not exit! skip\n", prop_pa_id_pin);
	} else {
		// detect pa type
		det_type = 0;
		audio_sense = 0;

		gpio_flag = GPIOF_DIR_IN;
		ret = gpio_request_one(det_gpio, gpio_flag, "det_gpio");
		if (ret < 0) {
			dev_err(dev, "det_gpio det request failed:%d!\n", ret);
			return ret;
		}
		det_flag = gpio_get_value(det_gpio);
		dev_info(dev, "det_gpio det:%d\n", det_flag);

		ret = of_property_read_u32(np, "sprd,spk-ext-pa-expect-id-flag", &expect_det_flag);
		if (ret) {
			dev_err(dev, "sprd,spk-ext-pa-expect-id-flag not exit! skip\n");
		} else {
			if (det_flag == expect_det_flag) {
				det_type = 1;
				strncpy(prop_pa_info, "sprd,spk-ext-pa-info-fsm", 32);
				pr_info("fs1512n detect sucess!\n");
			}
		}
	}

	ret = of_property_read_u32(np, extral_iic_pa_info, &extral_iic_pa);
	if (!ret) {
		sp_asoc_pr_info("%s hook aw87xx iic pa!\n", __func__);
		extral_iic_pa_en = extral_iic_pa;
		ext_hook->ext_ctrl[BOARD_FUNC_SPK] = ext_hook_arr[BOARD_FUNC_SPK].hook;
		return 0;
	}

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
		priv_data = buf[CELL_PRIV_m + num];
		hook_spk_priv.priv_data_m[ext_ctrl_type] = priv_data;
		priv_data = buf[CELL_PRIV_v + num];
		hook_spk_priv.priv_data_v[ext_ctrl_type] = priv_data;
		priv_data = buf[CELL_PRIV_f + num];
		hook_spk_priv.priv_data_f[ext_ctrl_type] = priv_data;

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

		gpio_flag = GPIOF_DIR_OUT;
		gpio_flag |= ext_hook_arr[hook_sel].en_level ?
			GPIOF_INIT_HIGH : GPIOF_INIT_LOW;
		ret = gpio_request_one(hook_spk_priv.gpio[ext_ctrl_type],
				       gpio_flag, NULL);
		dev_info(dev, "Gpio request[%d] ret:%d! hook_p = %p, ext_ctrl_p = %p \n",
			ext_ctrl_type, ret, ext_hook, ext_hook->ext_ctrl[ext_ctrl_type]);
		if (ret == 0) {
			hook_spk_priv.gpio_requested[ext_ctrl_type] = true;
		} else if (ret < 0) {
			dev_err(dev, "Gpio request[%d] failed:%d!\n",
				ext_ctrl_type, ret);
			hook_spk_priv.gpio_requested[ext_ctrl_type] = false;
			ext_hook->ext_ctrl[ext_ctrl_type] = NULL;
			return ret;
		}
	}

	if (det_type) {
		spin_lock_irqsave(&hook_spk_priv.lock, gpio_flag);
		gpio_set_value(hook_spk_priv.gpio[0], 0);
		udelay(20);
		gpio_set_value(hook_spk_priv.gpio[0], 1);
		udelay(500);
		gpio_set_value(hook_spk_priv.gpio[0], 0);
		spin_unlock_irqrestore(&hook_spk_priv.lock, gpio_flag);
	}

	return 0;
}
int sprd_asoc_card_parse_ext_hook(struct device *dev,
				  struct sprd_asoc_ext_hook *ext_hook)
{
	ext_debug_sysfs_init();
	return sprd_asoc_card_parse_hook(dev, ext_hook);
}

int sprd_asoc_card_ext_hook_free_gpio(struct device *dev)
{
	struct device_node *np = dev->of_node;
	const char *prop_pa_info = "sprd,spk-ext-pa-info";
	int spk_cnt, elem_cnt, i;
	unsigned int ext_ctrl_type;
	int ret = 0;
	u32 *buf;
	const char *prop_pa_id_pin = "sprd,spk-ext-pa-id-pin";
	int det_gpio;

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

		pr_info("%s, spk cnt = %d, ext_ctrl_type = %d\n", __func__, spk_cnt, ext_ctrl_type);

		if (hook_spk_priv.gpio_requested[ext_ctrl_type]) {
			gpio_free(hook_spk_priv.gpio[ext_ctrl_type]);
			hook_spk_priv.gpio_requested[ext_ctrl_type] = false;
			pr_info("%s, gpio_freed\n", __func__);
		}
	}

	det_gpio = of_get_named_gpio_flags(np, prop_pa_id_pin, 0, NULL);
	if (det_gpio < 0) {
		dev_err(dev, "%s, %s not exit! skip\n", __func__, prop_pa_id_pin);
	} else {
		gpio_free(det_gpio);
	}

	return 0;
}

MODULE_ALIAS("platform:asoc-sprd-card");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASoC SPRD Sound Card Utils - Hooks");
MODULE_AUTHOR("Peng Lee <peng.lee@spreadtrum.com>");
