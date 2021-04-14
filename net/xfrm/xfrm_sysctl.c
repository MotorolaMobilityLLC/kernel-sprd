// SPDX-License-Identifier: GPL-2.0
#include <linux/sysctl.h>
#include <linux/slab.h>
#include <net/net_namespace.h>
#include <net/xfrm.h>

#ifdef CONFIG_XFRM_FRAGMENT
static int enable_xfrm_fragment __read_mostly = 1;
static struct ctl_table *sprd_sys_ctl_table;
static struct ctl_table_header	*sprd_xfrm_hdr;
#endif

static void __net_init __xfrm_sysctl_init(struct net *net)
{
	net->xfrm.sysctl_aevent_etime = XFRM_AE_ETIME;
	net->xfrm.sysctl_aevent_rseqth = XFRM_AE_SEQT_SIZE;
	net->xfrm.sysctl_larval_drop = 1;
	net->xfrm.sysctl_acq_expires = 30;
}

#ifdef CONFIG_SYSCTL
static struct ctl_table xfrm_table[] = {
	{
		.procname	= "xfrm_aevent_etime",
		.maxlen		= sizeof(u32),
		.mode		= 0644,
		.proc_handler	= proc_douintvec
	},
	{
		.procname	= "xfrm_aevent_rseqth",
		.maxlen		= sizeof(u32),
		.mode		= 0644,
		.proc_handler	= proc_douintvec
	},
	{
		.procname	= "xfrm_larval_drop",
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{
		.procname	= "xfrm_acq_expires",
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{}
};

#ifdef CONFIG_XFRM_FRAGMENT
static struct ctl_table sprd_net_sys_table[] = {
	{
		.procname	= "enable_xfrm_fragment",
		.maxlen		= sizeof(int),
		.mode		= 0660,
		.proc_handler	= proc_dointvec
	},
	{}
};

int get_xfrm_fragment(void)
{
	return enable_xfrm_fragment;
}
#endif

int __net_init xfrm_sysctl_init(struct net *net)
{
	struct ctl_table *table;

	__xfrm_sysctl_init(net);

	table = kmemdup(xfrm_table, sizeof(xfrm_table), GFP_KERNEL);
	if (!table)
		goto out_kmemdup;
	table[0].data = &net->xfrm.sysctl_aevent_etime;
	table[1].data = &net->xfrm.sysctl_aevent_rseqth;
	table[2].data = &net->xfrm.sysctl_larval_drop;
	table[3].data = &net->xfrm.sysctl_acq_expires;

	/* Don't export sysctls to unprivileged users */
	if (net->user_ns != &init_user_ns)
		table[0].procname = NULL;

	net->xfrm.sysctl_hdr = register_net_sysctl(net, "net/core", table);
	if (!net->xfrm.sysctl_hdr)
		goto out_register;

#ifdef CONFIG_XFRM_FRAGMENT
	sprd_sys_ctl_table = kmemdup(sprd_net_sys_table,
				     sizeof(sprd_net_sys_table), GFP_KERNEL);
	if (!sprd_sys_ctl_table)
		goto out_kmemdup;

	sprd_sys_ctl_table[0].data = &enable_xfrm_fragment;
	sprd_xfrm_hdr = register_net_sysctl(net, "net/core",
					    sprd_sys_ctl_table);
	if (!sprd_xfrm_hdr)
		goto out_register2;
#endif
	return 0;

#ifdef CONFIG_XFRM_FRAGMENT
out_register2:
	kfree(sprd_sys_ctl_table);
	sprd_sys_ctl_table = NULL;
#endif

out_register:
	kfree(table);
out_kmemdup:
	return -ENOMEM;
}

void __net_exit xfrm_sysctl_fini(struct net *net)
{
	struct ctl_table *table;

	table = net->xfrm.sysctl_hdr->ctl_table_arg;
	unregister_net_sysctl_table(net->xfrm.sysctl_hdr);
	kfree(table);
#ifdef CONFIG_XFRM_FRAGMENT
	if (sprd_xfrm_hdr) {
		unregister_net_sysctl_table(sprd_xfrm_hdr);
		kfree(sprd_sys_ctl_table);
	}
#endif
}
#else
int __net_init xfrm_sysctl_init(struct net *net)
{
	__xfrm_sysctl_init(net);
	return 0;
}
#endif
