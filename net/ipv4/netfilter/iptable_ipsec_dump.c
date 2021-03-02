// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * signed-off-by: Junjie.Wang <junjie.wang@spreadtrum.com>
 */

#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv4/ip_tables.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/export.h>
#include <net/xfrm.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>

static int xfrm_dec_tcpdump_opt = 1;

static bool get_vowifi_dec_status(void)
{
	return xfrm_dec_tcpdump_opt ? true : false;
}

static inline int deliver_skb(struct sk_buff *skb,
			      struct packet_type *pt_prev,
			      struct net_device *orig_dev)
{
	if (unlikely(skb_orphan_frags(skb, GFP_ATOMIC)))
		return -ENOMEM;
	refcount_inc(&skb->users);

	return pt_prev->func(skb, skb->dev, pt_prev, orig_dev);
}

static void nf_xfrm4_output_decode_cap_log(struct sk_buff *skb,
					   struct net_device *out_dev)
{
	struct net_device *orig_dev;
	struct net_device *pseudo_dev = NULL;
	struct sk_buff *copy_skb;
	struct net *net;

	copy_skb = pskb_copy(skb, GFP_ATOMIC);
	if (!copy_skb) {
		pr_err("pskb_copy failed,return!\n");
		return;
	}
	/* skb->mac_headr can not be zero,
	 * otherwise it will crash in fun eth_header_parse.
	 */
	skb_set_mac_header(copy_skb, 0);
	skb_reset_mac_len(copy_skb);
	orig_dev = out_dev;
	skb_reset_network_header(copy_skb);

	if (skb->sk) {
		net = sock_net(skb->sk);
		if (net) {
			/* Lo device is hold,when it used,it must be dev_put. */
			pseudo_dev  = dev_get_by_name(net, "dummy0");
			if (!pseudo_dev) {
				pr_err("no dummy0 netdevice found.\n");
				goto free_clone;
			}
			copy_skb->dev = pseudo_dev;
		} else {
			pr_err("no net found.\n");
			goto free_clone;
		}
	} else {
		pr_err("skb has no sk,so cannot dump\n");
		goto free_clone;
	}
	copy_skb->protocol = htons(ETH_P_IP);
	copy_skb->transport_header = copy_skb->network_header;
	copy_skb->pkt_type = PACKET_OUTGOING;
	dev_queue_xmit(copy_skb);
	return;

free_clone:
	/* Free clone skb. */
	kfree_skb(copy_skb);
	return;
}

static void *skb_ext_get_ptr2(struct skb_ext *ext, enum skb_ext_id id)
{
	return (void *)ext + (ext->offset[id] * 8);
}

static void nf_xfrm4_input_decode_cap_log(struct sk_buff *skb)
{
	struct net_device *pseudo_dev;
	struct sk_buff *copy_skb;
	struct xfrm_state *x;
	int cr_xfrm_depth;
	struct sec_path *sp;
	struct skb_ext *ext = skb->extensions;

	sp = skb_ext_get_ptr2(ext, SKB_EXT_SEC_PATH);
	if (!sp)
		return;

	cr_xfrm_depth = sp->len - 1;
	if (unlikely(cr_xfrm_depth < 0))
		return;

	/* If current is tunnel mode, no need to dump again. */
	x = sp->xvec[cr_xfrm_depth];
	if (x && x->props.mode == XFRM_MODE_TUNNEL)
		return;

	copy_skb = skb_clone(skb, GFP_ATOMIC);
	if (!copy_skb) {
		pr_err("clone failed,return!\n");
		return;
	}
	pseudo_dev  = dev_get_by_name(&init_net, "dummy0");
	if (!pseudo_dev) {
		pr_err("no dummy0 netdevice found.\n");
		goto free_clone1;
	}
	copy_skb->dev = pseudo_dev;
	dev_queue_xmit(copy_skb);
	return;

free_clone1:
	/* Free clone skb. */
	kfree_skb(copy_skb);
	return;
}

static unsigned int nf_ipv4_ipsec_dec_dump_in(void *priv,
					      struct sk_buff *skb,
					      const struct nf_hook_state *state)
{
	struct skb_ext *ext = skb->extensions;

	if (!get_vowifi_dec_status())
		return NF_ACCEPT;

	if (((skb->active_extensions & (1 << SKB_EXT_SEC_PATH))) && ext)
		nf_xfrm4_input_decode_cap_log(skb);

	return NF_ACCEPT;
}

static unsigned int
nf_ipv4_ipsec_dec_dump_out(void *priv,
			   struct sk_buff *skb,
			   const struct nf_hook_state *state)
{
	if (!get_vowifi_dec_status())
		return NF_ACCEPT;

	if (skb_dst(skb) && skb_dst(skb)->xfrm)
		nf_xfrm4_output_decode_cap_log(skb, state->out);

	return NF_ACCEPT;
}

static struct nf_hook_ops nf_ipv4_ipsec_dec_dump_ops[] __read_mostly = {
	/* Before do encryption,save the pkt. */
	{
	    .hook	= nf_ipv4_ipsec_dec_dump_out,
	    .pf	        = NFPROTO_IPV4,
	    .hooknum	= NF_INET_LOCAL_OUT,
	    .priority	= NF_IP_PRI_LAST,
	},
	/* Check whether pkt is the decrypted one,and save it. */
	{
	    .hook	= nf_ipv4_ipsec_dec_dump_in,
	    .pf	        = NFPROTO_IPV4,
	    .hooknum    = NF_INET_LOCAL_IN,
	    .priority   = NF_IP_PRI_LAST,
	},
};

#ifdef CONFIG_PROC_FS
static ssize_t tcpdump_opt_proc_write(struct file *file,
				      const char __user *buffer,
				      size_t count,
				      loff_t *pos)
{
	char mode;

	if (count > 0) {
		if (get_user(mode, buffer))
			return -EFAULT;
		xfrm_dec_tcpdump_opt = (mode != '0');
	}

	return count;
}

static int tcpdump_ct_proc_show(struct seq_file *seq, void *v)
{
	seq_puts(seq, xfrm_dec_tcpdump_opt ? "1\n" : "0\n");
	return 0;
}

static int tcpdump_opt_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, tcpdump_ct_proc_show, NULL);
}

static const struct file_operations xfrm_dec_tcpdump_fops = {
	.open  = tcpdump_opt_proc_open,
	.read  = seq_read,
	.write  = tcpdump_opt_proc_write,
	.llseek  = seq_lseek,
	.release = single_release,
};
#endif

static int __net_init nf_vowifi_dec_init(struct net *net)
{
	int err = 0;
	umode_t mode = 0666;

	err = nf_register_net_hooks(net, nf_ipv4_ipsec_dec_dump_ops,
				    ARRAY_SIZE(nf_ipv4_ipsec_dec_dump_ops));
	if (err < 0) {
		pr_err("nf_register_hooks 4_ipsec failed (%d).\n", err);
		return err;
	}
#ifdef CONFIG_PROC_FS
	if (!proc_create("nf_xfrm_dec_tcpdump", mode,
			 net->nf.proc_netfilter,
			 &xfrm_dec_tcpdump_fops)) {
		nf_unregister_net_hooks(net, nf_ipv4_ipsec_dec_dump_ops,
					ARRAY_SIZE(nf_ipv4_ipsec_dec_dump_ops));
		return -ENOMEM;
	}
#endif
	return err;
}

static void nf_vowifi_dec_exit(struct net *net)
{
#ifdef CONFIG_PROC_FS
	remove_proc_entry("nf_xfrm_dec_tcpdump", net->nf.proc_netfilter);
#endif
	nf_unregister_net_hooks(net, nf_ipv4_ipsec_dec_dump_ops,
				ARRAY_SIZE(nf_ipv4_ipsec_dec_dump_ops));
}

static struct pernet_operations nf_net_xfrm_dec_ops = {
	.init = nf_vowifi_dec_init,
	.exit = nf_vowifi_dec_exit,
};

static int __init iptable_vowifi_ipsec_dec_dump_init(void)
{
	return register_pernet_subsys(&nf_net_xfrm_dec_ops);
}

static void __exit iptable_vowifi_ipsec_dec_dump_exit(void)
{
	unregister_pernet_subsys(&nf_net_xfrm_dec_ops);
}

module_init(iptable_vowifi_ipsec_dec_dump_init);
module_exit(iptable_vowifi_ipsec_dec_dump_exit);
MODULE_LICENSE("GPL v2");
