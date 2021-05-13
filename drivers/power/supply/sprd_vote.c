/*
 * Copyright (C) 2011 Unisoc Co., Ltd.
 * Jinfeng.Lin <Jinfeng.Lin1@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/power/sprd_vote.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>

static void sprd_vote_lock(struct sprd_vote *vote_gov)
{
	mutex_lock(&vote_gov->lock);
}

static void sprd_vote_unlock(struct sprd_vote *vote_gov)
{
	mutex_unlock(&vote_gov->lock);
}

static void sprd_vote_set_enable_prop(struct sprd_vote_client *vote_client, int len, bool enable)
{
	int i;

	for (i = 0; i < len; i++)
		vote_client[i].enable = enable;

}

static int sprd_vote_all(struct sprd_vote *vote_gov, int vote_type_id, int vote_cmd,
			 int value, int *target_value, bool enable)
{
	sprd_vote_set_enable_prop(vote_gov->ibat_client, SPRD_VOTE_TYPE_IBAT_ID_MAX, enable);
	sprd_vote_set_enable_prop(vote_gov->ibus_client, SPRD_VOTE_TYPE_IBUS_ID_MAX, enable);
	sprd_vote_set_enable_prop(vote_gov->cccv_client, SPRD_VOTE_TYPE_CCCV_ID_MAX, enable);

	return true;
}

static int sprd_vote_select(struct sprd_vote_client *vote_client, int vote_type,
			    int id, int vote_type_id_len, int vote_cmd,
			    int value, bool enable, int *target_value)
{
	int i, min = 0x7fffffff, max = -1;
	int ret = 0;

	if (vote_client[id].value == value && vote_client[id].enable == enable) {
		pr_info("vote_gov:%s vote the same vote_client[%d].value = %d,"
			" value = %d; vote_client[%d].enable = %d, enable = %d\n",
			vote_type_names[vote_type], id, vote_client[id].value,
			value, id, vote_client[id].enable, enable);
		ret = -EINVAL;
		return ret;
	}
	vote_client[id].value = value;
	vote_client[id].enable = enable;

	for (i = 0; i < vote_type_id_len; i++) {
		if (!vote_client[i].enable)
			continue;
		if (min > vote_client[i].value)
			min = vote_client[i].value;
		if (max < vote_client[i].value)
			max = vote_client[i].value;
	}

	switch (vote_cmd) {
	case SPRD_VOTE_CMD_MIN:
		*target_value = min;
		break;
	case SPRD_VOTE_CMD_MAX:
		*target_value = max;
		break;
	default:
		ret = -EINVAL;
		pr_err("vote_gov: vote_cmd[%d] error!!!\n", vote_cmd);
		break;
	}

	return ret;
}

static int sprd_vote_func(struct sprd_vote *vote_gov, bool enable, int vote_type,
			  int vote_type_id, int vote_cmd, int value, void *data)
{
	int target_value = 0, len;
	struct sprd_vote_client *vote_client = NULL;
	int ret = 0;

	sprd_vote_lock(vote_gov);

	switch (vote_type) {
	case SPRD_VOTE_TYPE_IBAT:
		len = SPRD_VOTE_TYPE_IBAT_ID_MAX;
		vote_client = vote_gov->ibat_client;
		break;
	case SPRD_VOTE_TYPE_IBUS:
		len = SPRD_VOTE_TYPE_IBUS_ID_MAX;
		vote_client = vote_gov->ibus_client;
		break;
	case SPRD_VOTE_TYPE_CCCV:
		len = SPRD_VOTE_TYPE_CCCV_ID_MAX;
		vote_client = vote_gov->cccv_client;
		break;
	case SPRD_VOTE_TYPE_ALL:
		pr_info("vote_gov: vote_all[%s]\n", enable ? "enable" : "disable");
		ret = sprd_vote_all(vote_gov, vote_type_id, vote_cmd,
				    value, &target_value, enable);
		break;
	default:
		pr_err("vote_gov: vote_type[%d] error!!!\n", vote_type);
		ret = -EINVAL;
		break;
	}

	if (ret)
		goto vote_unlock;

	if (vote_type_id >= len) {
		pr_err("vote_gov: vote_type[%d]: vote_type_id[%d] out of range",
		       vote_type, vote_type_id);
		ret = -EINVAL;
		goto vote_unlock;
	}

	ret = sprd_vote_select(vote_client, vote_type, vote_type_id, len,
			       vote_cmd, value, enable, &target_value);
	if (ret)
		goto vote_unlock;

	vote_gov->cb(vote_gov, vote_type, target_value, data);

vote_unlock:
	sprd_vote_unlock(vote_gov);
	return ret;
}

static void sprd_vote_destroy(struct sprd_vote *vote_gov)
{
	if (!vote_gov)
		return;

	kfree(vote_gov->name);
	kfree(vote_gov);
}

struct sprd_vote *sprd_charge_vote_register(char *name,
					    void (*cb)(struct sprd_vote *vote_gov,
						       int vote_type, int value,
						       void *data),
					    void *data)
{
	struct sprd_vote *vote_gov = NULL;

	vote_gov = kzalloc(sizeof(struct sprd_vote), GFP_KERNEL);
	if (!vote_gov)
		return ERR_PTR(-ENOMEM);

	vote_gov->name = kstrdup(name, GFP_KERNEL);
	if (!vote_gov->name) {
		pr_err("vote_gov: fail to dum name %s\n", name);
		kfree(vote_gov);
		return ERR_PTR(-ENOMEM);
	}

	vote_gov->cb = cb;
	vote_gov->data = data;
	vote_gov->vote = sprd_vote_func;
	vote_gov->destroy = sprd_vote_destroy;
	mutex_init(&vote_gov->lock);

	return vote_gov;
}
EXPORT_SYMBOL(sprd_charge_vote_register);
MODULE_LICENSE("GPL");
