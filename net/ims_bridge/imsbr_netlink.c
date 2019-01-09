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
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/etherdevice.h>
#include <net/ip6_route.h>
#include <net/route.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_tuple.h>
#include <net/netfilter/nf_conntrack_l3proto.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <linux/netfilter/nf_conntrack_common.h>
#include <net/arp.h>
#include <linux/if_ether.h>

#include "imsbr_core.h"
#include "imsbr_netlink.h"

static struct nla_policy imsbr_genl_policy[IMSBR_A_MAX + 1] = {
	[IMSBR_A_CALL_STATE]    = { .type = NLA_U32 },
	[IMSBR_A_CALL_TYPE]	    = { .type = NLA_U32 },
	[IMSBR_A_TUPLE]         = { .len = sizeof(struct imsbr_tuple) },
	[IMSBR_A_SIMCARD]       = { .type = NLA_U32 },
	[IMSBR_A_LOCALMAC]      = { .type = NLA_STRING },
	[IMSBR_A_REMOTEADDR]    = { .len = sizeof(union imsbr_inet_addr) },
	[IMSBR_A_ISV4]          = { .type = NLA_U32 },
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

static int
imsbr_do_call_type(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *nla;
	u32 simcard = 0;
	u32 type;

	nla = info->attrs[IMSBR_A_CALL_TYPE];
	if (!nla) {
		pr_err("call_type attr not exist!");
		return -EINVAL;
	}

	type = *((u32 *)nla_data(nla));

	if (type == IMSBR_CALL_TYPE_UNSPEC || type >= __IMSBR_CALL_TYPE_MAX) {
		pr_err("call_type %u is not supported\n", type);
		return -EINVAL;
	}

	nla = info->attrs[IMSBR_A_SIMCARD];
	if (nla)
		simcard = *((u32 *)nla_data(nla));
	if (simcard >= IMSBR_SIMCARD_NUM) {
		pr_err("simcard %d is out of range\n", simcard);
		return -EINVAL;
	}

	imsbr_set_calltype(type, simcard);
	pr_info("now call type is %s on sim%d\n",
		type == IMSBR_AUDIO_CALL ? "audio-call" : "video-call",
		simcard);
	return 0;
}

static struct neighbour *
imsbr_dst_get_neighbour(struct dst_entry *dst, void *daddr, int is_v4)
{
	struct neighbour *neigh;
	struct in6_addr *nexthop6;
	u32 nexthop4;
	__be32 *be32ptr = (__be32 *)daddr;

	if (is_v4) {
		nexthop4 = (__force u32)rt_nexthop(
			(struct rtable *)dst,
			*be32ptr);
		neigh = __ipv4_neigh_lookup_noref(dst->dev, nexthop4);
	} else {
		nexthop6 = rt6_nexthop(
			(struct rt6_info *)dst,
			(struct in6_addr *)daddr);
		neigh = __ipv6_neigh_lookup_noref(dst->dev, nexthop6);
	}
	if (neigh)
		neigh_hold(neigh);

	return neigh;
}

static bool imsbr_get_mac_by_ipaddr(
	union imsbr_inet_addr *addr, u8 *mac_addr, int is_v4)
{
	struct neighbour *neigh;
	struct rtable *rt;
	struct dst_entry *dst;
	struct net_device *mac_dev;
	struct flowi6 fl6;

	if (is_v4) {
		pr_info("start with IP: %pI4\n", &addr->ip);
		rt = ip_route_output(&init_net, addr->ip, 0, 0, 0);
		if (unlikely(IS_ERR(rt))) {
			pr_err("ip_route_output %lu\n",
			       (unsigned long)(void *)rt);
			goto ret_fail;
		}
		dst = (struct dst_entry *)rt;
	} else {
		pr_info("start with IP: %pI6\n", addr->ip6);
		memset(&fl6, 0, sizeof(fl6));
		memcpy(&fl6.daddr, &addr->in6, sizeof(fl6.daddr));
		dst = ip6_route_output(&init_net, NULL, &fl6);
		if (dst->error != 0) {
			pr_err("ip6_route_output goto fail\n");
			goto ret_fail;
		}
	}

	rcu_read_lock();
	neigh = imsbr_dst_get_neighbour(dst, addr, is_v4);
	if (unlikely(!neigh)) {
		rcu_read_unlock();
		dst_release(dst);
		pr_err("imsbr_dst_get_neighbour\n");
		goto ret_fail;
	}

	if (unlikely(!(neigh->nud_state & NUD_VALID))) {
		rcu_read_unlock();
		neigh_release(neigh);
		dst_release(dst);
		pr_err("NUD_VALID\n");
		goto ret_fail;
	}

	mac_dev = neigh->dev;
	if (!mac_dev) {
		rcu_read_unlock();
		neigh_release(neigh);
		dst_release(dst);
		pr_err("mac_dev\n");
		goto ret_fail;
	}

	memcpy(mac_addr, neigh->ha, (size_t)mac_dev->addr_len);

	pr_info("mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
		mac_addr[0], mac_addr[1], mac_addr[2],
		mac_addr[3], mac_addr[4], mac_addr[5]);
	rcu_read_unlock();
	neigh_release(neigh);
	dst_release(dst);
	return true;

ret_fail:
	if (is_v4)
		pr_err("failed to find MAC address for IP: %pI4\n", &addr->ip);
	else
		pr_err("failed to find MAC address for IP: %pI6\n", addr->ip6);
	return false;
}

static int
imsbr_notify_remote_mac(struct sk_buff *skb, struct genl_info *info)
{
	const char *cmd = "remote-mac";
	struct sblock blk;
	u8 remote_mac[ETH_ALEN];
	struct nlattr *nla;
	int is_v4;
	union imsbr_inet_addr *addr;
	bool ret;

	nla = info->attrs[IMSBR_A_ISV4];
	if (!nla) {
		pr_err("attr IMSBR_A_ISV4 not exist!");
		return -EINVAL;
	}
	is_v4 = *((u32 *)nla_data(nla));

	nla = info->attrs[IMSBR_A_REMOTEADDR];
	if (!nla) {
		pr_err("attr IMSBR_A_REMOTEADDR not exist!");
		return -EINVAL;
	}
	addr = (union imsbr_inet_addr *)nla_data(nla);

	ret = imsbr_get_mac_by_ipaddr(addr, remote_mac, is_v4);
	if (ret && !imsbr_build_cmd(cmd, &blk, remote_mac, ETH_ALEN))
		imsbr_sblock_send(&imsbr_ctrl, &blk, sizeof(*remote_mac));

	return 0;
}

static int
imsbr_notify_local_mac(struct sk_buff *skb, struct genl_info *info)
{
	struct sblock blk;
	const char *cmd = "local-mac";
	struct nlattr *nla;
	unsigned char *localmac;

	nla = info->attrs[IMSBR_A_LOCALMAC];
	if (!nla) {
		pr_err("attr IMSBR_A_LOCALMAC not exist!");
		return -EINVAL;
	}

	localmac = (unsigned char *)nla_data(nla);

	pr_info("local mac %02x:%02x:%02x:%02x:%02x:%02x\n",
		localmac[0], localmac[1], localmac[2],
		localmac[3], localmac[4], localmac[5]);

	if (!imsbr_build_cmd(cmd, &blk, localmac, ETH_ALEN))
		imsbr_sblock_send(&imsbr_ctrl, &blk, ETH_ALEN);

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
		.cmd = IMSBR_C_CALL_TYPE,
		.flags = GENL_ADMIN_PERM,
		.policy = imsbr_genl_policy,
		.doit = imsbr_do_call_type,
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
	{
		.cmd = IMSBR_C_SEND_LOCALMAC,
		.flags = GENL_ADMIN_PERM,
		.policy = imsbr_genl_policy,
		.doit = imsbr_notify_local_mac,
	},
	{
		.cmd = IMSBR_C_SEND_REMOTEMAC,
		.flags = GENL_ADMIN_PERM,
		.policy = imsbr_genl_policy,
		.doit = imsbr_notify_remote_mac,
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
