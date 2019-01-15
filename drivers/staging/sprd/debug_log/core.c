/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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

#include "core.h"
const char *ch_str[] = {
	"TRAINING",
	"WTL",
	"MDAR",
	"TPIU",
	"DBUS",
	"WCN",
};

static int dbg_log_init(struct dbg_log_device *dbg)
{
	DEBUG_LOG_PRINT("entry\n");
	if (dbg->is_inited)
		return 0;

	DEBUG_LOG_PRINT("start ops init\n");
	dbg->ops->init(dbg);

	dbg->is_inited = true;

	return 0;
}

static int dbg_log_exit(struct dbg_log_device *dbg)
{
	if (!dbg->is_inited)
		return 0;

	dbg->ops->exit(dbg);

	dbg->is_inited = false;

	return 0;
}

void dbg_log_channel_sel(struct dbg_log_device *dbg)
{
	DEBUG_LOG_PRINT("dbg->channel=%d\n", dbg->channel);
	if (dbg->channel) {
		dbg_log_init(dbg);
		dbg->ops->select(dbg);
	} else {
		dbg->ops->select(dbg);
		dbg_log_exit(dbg);
	}
}

bool dbg_log_is_freq_valid(struct dbg_log_device *dbg, unsigned int freq)
{
	if (dbg->ops->is_freq_valid)
		return dbg->ops->is_freq_valid(dbg, freq);
	return false;
}

int dbg_log_get_valid_channel(struct dbg_log_device *dbg, const char *buf)
{
	if (dbg->ops->get_valid_channel)
		return dbg->ops->get_valid_channel(dbg, buf);
	return -EINVAL;
}

struct dbg_log_device *dbg_log_device_register(struct device *parent,
					       struct dbg_log_ops *ops,
					       struct phy_ctx *phy)
{
	struct dbg_log_device *dbg;
	struct class *dbg_class;
	int i;

	DEBUG_LOG_PRINT("entry\n");

	dbg = devm_kzalloc(parent, sizeof(struct dbg_log_device), GFP_KERNEL);
	if (!dbg)
		return NULL;

	if (phy) {
		dbg->phy = phy;
	} else {
		dbg->phy = devm_kzalloc(parent, sizeof(*dbg->phy), GFP_KERNEL);
		if (!(dbg->phy))
			goto err;
	}
	dbg->ops = ops;

	dbg_class = class_create(THIS_MODULE, "modem");
	if (IS_ERR(dbg_class)) {
		pr_err("Unable to create modem class\n");
		goto phy_free;
	}

	dbg->dev.class = dbg_class;
	dbg->dev.parent = parent;
	dbg->dev.of_node = parent->of_node;
	dev_set_name(&dbg->dev, "debug-log");
	dev_set_drvdata(&dbg->dev, dbg);

	if (device_register(&dbg->dev)) {
		pr_err("modem dbg log device register failed\n");
		goto phy_free;
	}

	DEBUG_LOG_PRINT("start dbg_log_sysfs_init\n");
	if (dbg_log_sysfs_init(&dbg->dev)) {
		pr_err("create sysfs node failed\n");
		goto phy_free;
	}
	DEBUG_LOG_PRINT("end dbg_log_sysfs_init\n");

	if (!dbg->serdes.ch_str[0]) {
		dbg->serdes.ch_num = ARRAY_SIZE(ch_str);
		for (i = 0; i < ARRAY_SIZE(ch_str); i++)
			dbg->serdes.ch_str[i] = ch_str[i];
	}

	return dbg;

phy_free:
	if (!phy)
		kfree(dbg->phy);
err:
	kfree(dbg);
	return NULL;
}
