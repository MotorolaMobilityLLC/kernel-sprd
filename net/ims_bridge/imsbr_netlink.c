/*
 * Copyright (C) 2016 Spreadtrum Communications Inc.
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

#define pr_fmt(fmt) "imsbr: " fmt

#include <net/genetlink.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <uapi/linux/ims_bridge/ims_bridge.h>

#include "imsbr_core.h"
#include "imsbr_netlink.h"

static struct nla_policy imsbr_genl_policy[IMSBR_A_MAX + 1] = {
	[IMSBR_A_CALL_STATE]	 = { .type = NLA_U32 },
	[IMSBR_A_TUPLE]		 = { .len = sizeof(struct imsbr_tuple) },
	[IMSBR_A_SIMCARD]	 = { .type = NLA_U32 },
};

static int
imsbr_do_call_state(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *nla;
	u32 simcard = 0;
	u32 state;

	nla = info->attrs[IMSBR_A_CALL_STATE];
	if (!nla) {
		pr_err("call_state attr not exist!");
		return -EINVAL;
	}

	state = *((u32 *)nla_data(nla));

	if (state == IMSBR_CALLS_UNSPEC || state >= __IMSBR_CALLS_MAX) {
		pr_err("call_state %u is not supported\n", state);
		return -EINVAL;
	}

	nla = info->attrs[IMSBR_A_SIMCARD];
	if (nla)
		simcard = *((u32 *)nla_data(nla));
	if (simcard >= IMSBR_SIMCARD_NUM) {
		pr_err("simcard %d is out of range\n", simcard);
		return -EINVAL;
	}

	imsbr_set_callstate(state, simcard);
	return 0;
}

static void
imsbr_notify_aptuple(const char *cmd, struct imsbr_tuple *tuple)
{
	struct sblock blk;

	if (!imsbr_build_cmd(cmd, &blk, tuple, sizeof(*tuple)))
		imsbr_sblock_send(&imsbr_ctrl, &blk, sizeof(*tuple));
}

static int
imsbr_add_aptuple(struct sk_buff *skb, struct genl_info *info)
{
	struct nf_conntrack_tuple nft, nft_inv;
	const char *cmd = "aptuple-add";
	struct imsbr_tuple *tuple;
	struct nlattr *nla;

	nla = info->attrs[IMSBR_A_TUPLE];
	if (!nla) {
		pr_err("tuple attr not exist!");
		return -EINVAL;
	}

	tuple = (struct imsbr_tuple *)nla_data(nla);

	imsbr_tuple_dump(cmd, tuple);
	if (!imsbr_tuple_validate(cmd, tuple))
		return -EINVAL;

	imsbr_tuple2nftuple(tuple, &nft, false);
	imsbr_tuple2nftuple(tuple, &nft_inv, true);

	imsbr_flow_add(&nft, IMSBR_FLOW_APTUPLE, tuple);
	imsbr_flow_add(&nft_inv, IMSBR_FLOW_APTUPLE, tuple);

	imsbr_notify_aptuple(cmd, tuple);
	return 0;
}

static int
imsbr_del_aptuple(struct sk_buff *skb, struct genl_info *info)
{
	struct nf_conntrack_tuple nft, nft_inv;
	const char *cmd = "aptuple-del";
	struct imsbr_tuple *tuple;
	struct nlattr *nla;

	nla = info->attrs[IMSBR_A_TUPLE];
	if (!nla) {
		pr_err("tuple attr not exist!");
		return -EINVAL;
	}

	tuple = (struct imsbr_tuple *)nla_data(nla);

	imsbr_tuple_dump(cmd, tuple);
	if (!imsbr_tuple_validate(cmd, tuple))
		return -EINVAL;

	imsbr_tuple2nftuple(tuple, &nft, false);
	imsbr_tuple2nftuple(tuple, &nft_inv, true);

	imsbr_flow_del(&nft, IMSBR_FLOW_APTUPLE, tuple);
	imsbr_flow_del(&nft_inv, IMSBR_FLOW_APTUPLE, tuple);

	imsbr_notify_aptuple(cmd, tuple);
	return 0;
}

static void imsbr_notify_reset_aptuple(u32 simcard)
{
	struct sblock blk;

	if (!imsbr_build_cmd("aptuple-reset", &blk, &simcard, sizeof(simcard)))
		imsbr_sblock_send(&imsbr_ctrl, &blk, sizeof(simcard));
}

static int
imsbr_reset_aptuple(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *nla;
	u32 simcard = 0;

	nla = info->attrs[IMSBR_A_SIMCARD];
	if (nla)
		simcard = *((u32 *)nla_data(nla));
	if (simcard >= IMSBR_SIMCARD_NUM) {
		pr_err("simcard %d is out of range\n", simcard);
		return -EINVAL;
	}

	imsbr_flow_reset(IMSBR_FLOW_APTUPLE, simcard, false);

	imsbr_notify_reset_aptuple(simcard);
	return 0;
}

static struct genl_ops imsbr_genl_ops[] = {
	{
		.cmd = IMSBR_C_CALL_STATE,
		.flags = GENL_ADMIN_PERM,
		.policy = imsbr_genl_policy,
		.doit = imsbr_do_call_state,
	},
	{
		.cmd = IMSBR_C_ADD_TUPLE,
		.flags = GENL_ADMIN_PERM,
		.policy = imsbr_genl_policy,
		.doit = imsbr_add_aptuple,
	},
	{
		.cmd = IMSBR_C_DEL_TUPLE,
		.flags = GENL_ADMIN_PERM,
		.policy = imsbr_genl_policy,
		.doit = imsbr_del_aptuple,
	},
	{
		.cmd = IMSBR_C_RESET_TUPLE,
		.flags = GENL_ADMIN_PERM,
		.policy = imsbr_genl_policy,
		.doit = imsbr_reset_aptuple,
	},
};

static struct genl_family imsbr_genl_family = {
	.hdrsize	= 0,
	.name		= IMSBR_GENL_NAME,
	.version	= IMSBR_GENL_VERSION,
	.maxattr	= IMSBR_A_MAX,
	.ops		= imsbr_genl_ops,
	.n_ops		= ARRAY_SIZE(imsbr_genl_ops),
};

int __init imsbr_netlink_init(void)
{
	return genl_register_family(&imsbr_genl_family);
}

void imsbr_netlink_exit(void)
{
	genl_unregister_family(&imsbr_genl_family);
}
