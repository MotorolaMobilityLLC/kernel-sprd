// SPDX-License-Identifier: GPL-2.0
//
// Spreadtrum ddr dvfs driver
//
// Copyright (C) 2015~2020 Spreadtrum, Inc.
// Author: Mingmin Ling <mingmin.ling@unisoc.com>

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/devfreq.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/sipc.h>
#include "sprd_ddr_dvfs.h"

enum dvfs_master_cmd {
	DVFS_CMD_NORMAL		= 0x0000,
	DVFS_CMD_ENABLE		= 0x0300,
	DVFS_CMD_DISABLE		= 0x0305,
	DVFS_CMD_AUTO_ENABLE	= 0x0310,
	DVFS_CMD_AUTO_DISABLE	= 0x0315,
	DVFS_CMD_AXI_ENABLE	  = 0x0320,
	DVFS_CMD_AXI_DISABLE	 = 0x0330,
	DVFS_CMD_INQ_DDR_FREQ	= 0x0500,
	DVFS_CMD_INQ_AP_FREQ	= 0x0502,
	DVFS_CMD_INQ_CP_FREQ	= 0x0503,
	DVFS_CMD_INQ_DDR_TABLE	= 0x0505,
	DVFS_CMD_INQ_COUNT	= 0x0507,
	DVFS_CMD_INQ_STATUS	= 0x050A,
	DVFS_CMD_INQ_AUTO_STATUS	= 0x050B,
	DVFS_CMD_INQ_OVERFLOW	= 0x0510,
	DVFS_CMD_INQ_UNDERFLOW	= 0x0520,
	DVFS_CMD_INQ_TIMER	= 0x0530,
	DVFS_CMD_INQ_AXI		 = 0x0540,
	DVFS_CMD_INQ_AXI_WLTC	= 0x0541,
	DVFS_CMD_INQ_AXI_RLTC	= 0x0542,
	DVFS_CMD_SET_DDR_FREQ	= 0x0600,
	DVFS_CMD_SET_CAL_FREQ	= 0x0603,
	DVFS_CMD_PARA_START	= 0x0700,
	DVFS_CMD_PARA_OVERFLOW	= 0x0710,
	DVFS_CMD_PARA_UNDERFLOW	= 0x0720,
	DVFS_CMD_PARA_TIMER	= 0x0730,
	DVFS_CMD_PARA_END	= 0x07FF,
	DVFS_CMD_SET_AXI_WLTC	= 0x0810,
	DVFS_CMD_SET_AXI_RLTC	= 0x0820,
	DVFS_CMD_DEBUG		= 0x0FFF
};

enum dvfs_slave_cmd {
	DVFS_RET_ADJ_OK		= 0x0000,
	DVFS_RET_ADJ_VER_FAIL	= 0x0001,
	DVFS_RET_ADJ_BUSY	= 0x0002,
	DVFS_RET_ADJ_NOCHANGE	= 0x0003,
	DVFS_RET_ADJ_FAIL	= 0x0004,
	DVFS_RET_DISABLE		= 0x0005,
	DVFS_RET_ON_OFF_SUCCEED	= 0x0300,
	DVFS_RET_ON_OFF_FAIL	= 0x0303,
	DVFS_RET_INQ_SUCCEED	= 0x0500,
	DVFS_RET_INQ_FAIL	= 0x0503,
	DVFS_RET_SET_SUCCEED	= 0x0600,
	DVFS_RET_SET_FAIL	= 0x0603,
	DVFS_RET_PARA_OK		= 0x070F,
	DVFS_RET_DEBUG_OK	= 0x0F00,
	DVFS_RET_INVALID_CMD	= 0x0F0F
};

struct freq_para {
	unsigned int overflow;
	unsigned int underflow;
	unsigned int vol;
};

struct dvfs_data {
	struct device *dev;
	struct devfreq *devfreq;
	struct devfreq_dev_profile *profile;
	struct task_struct *dvfs_smsg_ch_open;
	struct mutex sync_mutex;
	unsigned int freq_num;
	unsigned int *freq_table;
	struct freq_para *paras;
	unsigned int force_freq;
	struct dvfs_hw_callback *hw_callback;
	struct completion reg_callback_done;
	struct governor_callback *gov_callback;
	unsigned int init_done;
};
static struct dvfs_data *g_dvfs_data;
static char *default_governor = "sprd-governor";

static int dvfs_msg_recv(struct smsg *msg, int timeout)
{
	int err;
	struct device *dev = g_dvfs_data->dev;

	err = smsg_recv(SIPC_ID_PM_SYS, msg, timeout);
	if (err < 0) {
		dev_err(dev, "dvfs receive failed:%d\n", err);
		return err;
	}
	if (msg->channel == SMSG_CH_PM_CTRL &&
			msg->type == SMSG_TYPE_DFS_RSP)
		return 0;
	return -EINVAL;
}

static int dvfs_msg_send(struct smsg *msg, unsigned int cmd, int timeout,
				 unsigned int value)
{
	int err;
	struct device *dev = g_dvfs_data->dev;

	smsg_set(msg, SMSG_CH_PM_CTRL, SMSG_TYPE_DFS, cmd, value);
	err = smsg_send(SIPC_ID_PM_SYS, msg, timeout);
	dev_dbg(dev, "send: channel=%d, type=%d, flag=0x%x, value=0x%x\n",
		msg->channel, msg->type, msg->flag, msg->value);
	if (err < 0) {
		dev_err(dev, "dvfs send failed, freq:%d, cmd:%d\n",
			value, cmd);
		return err;
	}
	return 0;
}

static int dvfs_msg_parse_ret(struct smsg *msg)
{
	unsigned int err;
	struct device *dev = g_dvfs_data->dev;

	switch (msg->flag) {
	case DVFS_RET_ADJ_OK:
	case DVFS_RET_ON_OFF_SUCCEED:
	case DVFS_RET_INQ_SUCCEED:
	case DVFS_RET_SET_SUCCEED:
	case DVFS_RET_PARA_OK:
	case DVFS_RET_DEBUG_OK:
		err = 0;
		break;
	case DVFS_RET_ADJ_VER_FAIL:
		dev_info(dev, "dvfs verify fail!current freq:%d\n",
			 msg->value);
		err = -EIO;
		break;
	case DVFS_RET_ADJ_BUSY:
		dev_info(dev, "dvfs busy!current freq:%d\n",
			 msg->value);
		err = -EBUSY;
		break;
	case DVFS_RET_ADJ_NOCHANGE:
		dev_info(dev, "dvfs no change!current freq:%d\n",
			 msg->value);
		err = -EFAULT;
		break;
	case DVFS_RET_ADJ_FAIL:
		dev_info(dev, "dvfs fail!current freq:%d\n",
			 msg->value);
		err = -EFAULT;
		break;
	case DVFS_RET_DISABLE:
		dev_info(dev, "dvfs is disabled!current freq:%d\n",
			 msg->value);
		err = -EPERM;
		break;
	case DVFS_RET_ON_OFF_FAIL:
		dev_err(dev, "dvfs enable verify failed\n");
		err = -EINVAL;
		break;
	case DVFS_RET_INQ_FAIL:
		dev_err(dev, "dvfs inquire failed\n");
		err = -EINVAL;
		break;
	case DVFS_RET_SET_FAIL:
		dev_err(dev, "dvfs set failed\n");
		err = -EINVAL;
		break;
	case DVFS_RET_INVALID_CMD:
		dev_info(dev, "no this command\n");
		err = -EINVAL;
		break;
	default:
		dev_info(dev, "dvfs invalid cmd:%x!current freq:%d\n",
			 msg->flag, msg->value);
		err = -EINVAL;
		break;
	}

	return err;
}

static int dvfs_msg(unsigned int *data, unsigned int value,
						unsigned int cmd, unsigned int wait)
{
	int err = 0;
	struct smsg msg;

	err = dvfs_msg_send(&msg, cmd, msecs_to_jiffies(100), value);
	if (err < 0)
		return err;

	err = dvfs_msg_recv(&msg, msecs_to_jiffies(wait));
	if (err < 0)
		return err;

	err = dvfs_msg_parse_ret(&msg);
	*data = msg.value;
	return err;
}

static int dvfs_enable(void)
{
	int err = 0;
	unsigned int data;

	err = dvfs_msg(&data, 0, DVFS_CMD_ENABLE, 2000);

	return err;
}

static int dvfs_disable(void)
{
	int err = 0;
	unsigned int data;

	err = dvfs_msg(&data, 0, DVFS_CMD_DISABLE, 2000);
	return err;
}

int dvfs_auto_enable(void)
{
	int err = 0;
	unsigned int data;

	err = dvfs_msg(&data, 0, DVFS_CMD_AUTO_ENABLE, 2000);
	return err;
}

static int dvfs_auto_disable(void)
{
	int err = 0;
	unsigned int data;

	err = dvfs_msg(&data, 0, DVFS_CMD_AUTO_DISABLE, 2000);

	return err;
}

static int get_dvfs_status(unsigned int *data)
{
	int err;

	if (g_dvfs_data == NULL)
		return -EINVAL;
	err =  dvfs_msg(data, 0, DVFS_CMD_INQ_STATUS, 500);
	return err;
}

static int get_dvfs_auto_status(unsigned int *data)
{
	int err;

	if (g_dvfs_data == NULL)
		return -EINVAL;
	err = dvfs_msg(data, 0, DVFS_CMD_INQ_AUTO_STATUS, 500);
	return err;
}

static int force_freq_request(unsigned int freq)
{
	unsigned int data;
	int err;

	if ((g_dvfs_data == NULL) || (g_dvfs_data->init_done == 0))
		return -EINVAL;
	mutex_lock(&g_dvfs_data->sync_mutex);
	err = dvfs_msg(&data, freq, DVFS_CMD_SET_DDR_FREQ, 500);
	if (err == 0)
		g_dvfs_data->force_freq = freq;
	mutex_unlock(&g_dvfs_data->sync_mutex);
	return err;
}

int force_top_freq(void)
{
	if ((g_dvfs_data == NULL) || (g_dvfs_data->init_done == 0))
		return -EINVAL;
	return force_freq_request(g_dvfs_data->devfreq->max_freq);
}

int send_vote_request(unsigned int freq)
{
	int err;
	unsigned int data;

	if ((g_dvfs_data == NULL) || (g_dvfs_data->init_done == 0))
		return -EINVAL;
	mutex_lock(&g_dvfs_data->sync_mutex);
	err = dvfs_msg(&data, freq, DVFS_CMD_NORMAL, 500);
	mutex_unlock(&g_dvfs_data->sync_mutex);
	return err;
}

static int get_freq_table(unsigned int *data, unsigned int sel)
{
	int err;

	if (g_dvfs_data == NULL)
		return -EINVAL;
	if (g_dvfs_data->init_done != 1) {
		err = dvfs_msg(data, sel, DVFS_CMD_INQ_DDR_TABLE, 500);
	} else {
		*data = g_dvfs_data->freq_table[sel];
		err = 0;
	}
	return err;
}

static int get_cur_freq(unsigned int *data)
{
	int err;

	if (g_dvfs_data == NULL)
		return -EINVAL;
	err = dvfs_msg(data, 0, DVFS_CMD_INQ_DDR_FREQ, 500);
	return err;
}

static int get_overflow(unsigned int *data, unsigned int sel)
{
	int err;

	if (g_dvfs_data == NULL)
		return -EINVAL;
	if (g_dvfs_data->init_done != 1) {
		err = dvfs_msg(data, sel, DVFS_CMD_INQ_OVERFLOW, 500);
	} else {
		*data = g_dvfs_data->paras[sel].overflow;
		err = 0;
	}
	return err;
}

static int get_underflow(unsigned int *data, unsigned int sel)
{
	int err;

	if (g_dvfs_data == NULL)
		return -EINVAL;
	if (g_dvfs_data->init_done != 1) {
		err = dvfs_msg(data, sel, DVFS_CMD_INQ_UNDERFLOW, 500);
	} else {
		*data = g_dvfs_data->paras[sel].underflow;
		err = 0;
	}
	return err;
}

static int set_overflow(unsigned int value, unsigned int sel)
{
	int err;
	unsigned int data;

	if (g_dvfs_data == NULL || sel >= g_dvfs_data->freq_num)
		return -EINVAL;
	mutex_lock(&g_dvfs_data->sync_mutex);
	err = dvfs_msg(&data, value, DVFS_CMD_PARA_OVERFLOW+sel, 500);
	if ((err == 0) && (g_dvfs_data->init_done == 1))
		g_dvfs_data->paras[sel].overflow = value;
	mutex_unlock(&g_dvfs_data->sync_mutex);
	return err;
}

static int set_underflow(unsigned int value, unsigned int sel)
{
	int err;
	unsigned int data;

	if (g_dvfs_data == NULL || sel >= g_dvfs_data->freq_num)
		return -EINVAL;
	mutex_lock(&g_dvfs_data->sync_mutex);
	err = dvfs_msg(&data, value, DVFS_CMD_PARA_UNDERFLOW+sel, 500);
	if ((err == 0) && (g_dvfs_data->init_done == 1))
		g_dvfs_data->paras[sel].underflow = value;
	mutex_unlock(&g_dvfs_data->sync_mutex);
	return err;
}

static int get_freq_num(unsigned int *data)
{
	if (g_dvfs_data == NULL)
		return -EINVAL;
	*data = g_dvfs_data->freq_num;
	return 0;
}

static int gov_vote(const char *name)
{
	if ((g_dvfs_data == NULL) || (g_dvfs_data->init_done == 0))
		return -EINVAL;
	return g_dvfs_data->hw_callback->hw_dvfs_vote(name);
}

static int gov_unvote(const char *name)
{
	if ((g_dvfs_data == NULL) || (g_dvfs_data->init_done == 0))
		return -EINVAL;
	return g_dvfs_data->hw_callback->hw_dvfs_unvote(name);
}

static int gov_change_point(const char *name, unsigned int freq)
{
	if ((g_dvfs_data == NULL) || (g_dvfs_data->init_done == 0))
		return -EINVAL;
	return g_dvfs_data->hw_callback->hw_dvfs_set_point(name, freq);
}

static int get_point_info(char **name, unsigned int *freq,
			unsigned int *flag, int index)
{
	if (g_dvfs_data == NULL)
		return -EINVAL;
	return g_dvfs_data->hw_callback->hw_dvfs_get_point_info(name, freq, flag, index);
}

struct governor_callback g_gov_callback = {
	.governor_vote = gov_vote,
	.governor_unvote = gov_unvote,
	.governor_change_point = gov_change_point,
	.get_point_info = get_point_info,
	.get_freq_num = get_freq_num,
	.get_overflow = get_overflow,
	.set_overflow = set_overflow,
	.get_underflow = get_underflow,
	.set_underflow = set_underflow,
	.get_dvfs_status = get_dvfs_status,
	.dvfs_enable = dvfs_enable,
	.dvfs_disable = dvfs_disable,
	.get_dvfs_auto_status = get_dvfs_auto_status,
	.dvfs_auto_enable = dvfs_auto_enable,
	.dvfs_auto_disable = dvfs_auto_disable,
	.get_cur_freq = get_cur_freq,
	.get_freq_table = get_freq_table,
};

static int dvfs_freq_target(struct device *dev, unsigned long *freq,
								u32 flags)
{
	int err;

	err = force_freq_request(*freq);
	if (err < 0)
		dev_err(dev, "set freq fail: %d\n", err);
	return err;
}

static int dvfs_get_dev_status(struct device *dev,
		  struct devfreq_dev_status *state)
{
	int err;

	err = get_cur_freq((unsigned int *)&state->current_frequency);
	if (err < 0)
		dev_err(dev, "get cur freq fail: %d\n", err);
	state->private_data = (void *)g_dvfs_data->gov_callback;

	return err;
}

static int dvfs_get_cur_freq(struct device *dev, unsigned long *freq)
{
	int err;

	err = get_cur_freq((unsigned int *)freq);
	if (err < 0)
		dev_err(dev, "get cur freq fail: %d\n", err);
	return err;
}

static void dvfs_exit(struct device *dev)
{
	int err;

	err = dvfs_disable();
	if (err < 0)
		dev_err(dev, "disable fail: %d\n", err);
}


static void set_profile(struct devfreq_dev_profile *profile)
{
	profile->polling_ms = 0;
	profile->freq_table = (unsigned long *)g_dvfs_data->freq_table;
	profile->max_state = g_dvfs_data->freq_num;
	profile->target = dvfs_freq_target;
	profile->get_dev_status = dvfs_get_dev_status;
	profile->get_cur_freq = dvfs_get_cur_freq;
	profile->exit = dvfs_exit;
}

static int dvfs_smsg_thread(void *value)
{
	struct dvfs_data *data = (struct dvfs_data *)value;
	struct device *dev = data->dev;
	char *temp_name;
	int i, err;

	while (smsg_ch_open(SIPC_ID_PM_SYS, SMSG_CH_PM_CTRL, -1))
		msleep(500);
	while (dvfs_enable())
		msleep(500);

	for (i = 0; i < data->freq_num; i++) {
		err = get_freq_table(&data->freq_table[i], i);
		if (err < 0) {
			dev_err(dev, "failed to get frequence index: %d\n", i);
			return 0;
		}
		if (data->paras[i].overflow != 0) {
			err = set_overflow(data->paras[i].overflow, i);
			if (err < 0) {
				dev_err(dev, "failed to set overflow %d\n",
					data->paras[i].overflow);
				return 0;
			}
		} else {
			err = get_overflow(&data->paras[i].overflow, i);
			if (err < 0) {
				dev_err(dev, "failed to get overflow index: %d\n", i);
				return 0;
			}
		}
		if (data->paras[i].underflow != 0) {
			err = set_underflow(data->paras[i].underflow, i);
			if (err < 0) {
				dev_err(dev, "failed to set underflow %d\n",
					data->paras[i].underflow);
				return 0;
			}
		} else {
			err = get_underflow(&data->paras[i].underflow, i);
			if (err < 0) {
				dev_err(dev, "get_underflow err\n");
				return 0;
			}
		}
		/*fix me : now we do not have interface for vol*/
		if (data->paras[i].vol == 0)
			data->paras[i].vol = 750;
		if (data->freq_table[i] != 0) {
			err = dev_pm_opp_add(dev, data->freq_table[i], data->paras[i].vol);
			if (err < 0) {
				dev_err(dev, "failed to add opp: %uMHZ-%uuv\n",
					data->freq_table[i], data->paras[i].vol);
				return 0;
			}
		}
	}


	err = get_cur_freq((unsigned int *)(&data->profile->initial_freq));
	if (err < 0) {
		dev_err(dev, "failed to get initial freq\n");
		return 0;
	}

	err = sprd_dvfs_add_governor();
	if (err < 0) {
		dev_err(dev, "failed to add governor\n");
		return 0;
	}
	err = of_property_read_string(dev->of_node, "governor", (const char **)&temp_name);
	if (err != 0) {
		dev_warn(dev, "no governor sepecific\n");
		temp_name = default_governor;
	}

	data->devfreq = devfreq_add_device(dev, data->profile, temp_name, NULL);
	if (IS_ERR(data->devfreq)) {
		dev_err(dev, "add freq devices fail\n");
		goto remove_governor;
	}
	data->devfreq->min_freq = data->devfreq->scaling_min_freq;
	data->devfreq->max_freq = data->devfreq->scaling_max_freq;

	err = dvfs_auto_enable();
	if (err < 0) {
		dev_err(dev, "dvfs auto enable failed\n");
		goto remove_device;
	}
	wait_for_completion(&data->reg_callback_done);
	data->init_done = 1;
	return 0;

remove_device:
	devfreq_remove_device(data->devfreq);
remove_governor:
	sprd_dvfs_del_governor();
	return 0;
}

int dvfs_core_init(struct platform_device *pdev)
{
	unsigned int freq_num;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	unsigned int i;
	void *p;
	int err;

	if (g_dvfs_data != NULL) {
		dev_err(dev, "dvfs core can used by single device only\n");
		return -EINVAL;
	}

	err = of_property_read_u32(dev->of_node, "freq-num", &freq_num);
	if (err != 0) {
		dev_warn(dev, "failed read freqnum\n");
		freq_num = 8;
	}

	p = devm_kzalloc(dev, sizeof(struct dvfs_data)+sizeof(struct devfreq_dev_profile)
			 +(sizeof(unsigned int)+sizeof(struct freq_para))*freq_num, GFP_KERNEL);
	if (p == NULL) {
		err = -ENOMEM;
		return err;
	}

	g_dvfs_data = (struct dvfs_data *)p;
	p += sizeof(struct dvfs_data);
	g_dvfs_data->profile = (struct devfreq_dev_profile *)p;
	p += sizeof(struct devfreq_dev_profile);
	g_dvfs_data->freq_table = (unsigned int *)p;
	p += sizeof(unsigned int)*freq_num;
	g_dvfs_data->paras = (struct freq_para *)p;

	g_dvfs_data->dev = dev;
	g_dvfs_data->freq_num = freq_num;
	mutex_init(&g_dvfs_data->sync_mutex);
	init_completion(&g_dvfs_data->reg_callback_done);
	g_dvfs_data->gov_callback = &g_gov_callback;

	for (i = 0; i < g_dvfs_data->freq_num; i++) {
		err = of_property_read_u32_index(node, "overflow",
						 i, &g_dvfs_data->paras[i].overflow);
		if (err != 0) {
			dev_warn(dev, "failed parse freq overflow\n");
			break;
		}
		err = of_property_read_u32_index(node, "underflow",
						 i, &g_dvfs_data->paras[i].underflow);
		if (err != 0) {
			dev_warn(dev, "failed parse freq underflow\n");
			break;
		}
	}

	set_profile(g_dvfs_data->profile);
	g_dvfs_data->dvfs_smsg_ch_open = kthread_run(dvfs_smsg_thread, g_dvfs_data, "dvfs-init");
	if (IS_ERR(g_dvfs_data->dvfs_smsg_ch_open)) {
		err = -EINVAL;
		goto err_device;
	}

	platform_set_drvdata(pdev, g_dvfs_data);
	return 0;

err_device:
	devm_kfree(dev, p);
	return err;
}

int dvfs_core_clear(struct platform_device *pdev)
{
	devfreq_remove_device(g_dvfs_data->devfreq);
	sprd_dvfs_del_governor();
	devm_kfree(&pdev->dev, g_dvfs_data);
	return 0;
}

void dvfs_core_hw_callback_register(struct dvfs_hw_callback *hw_callback)
{
	g_dvfs_data->hw_callback = hw_callback;
	complete(&g_dvfs_data->reg_callback_done);
}

void dvfs_core_hw_callback_clear(struct dvfs_hw_callback *hw_callback)
{
	if (g_dvfs_data->hw_callback != hw_callback)
		dev_err(g_dvfs_data->dev, "call clear err!\n");
	g_dvfs_data->hw_callback = NULL;
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("dvfs driver for sprd ddrc v1 and later");
