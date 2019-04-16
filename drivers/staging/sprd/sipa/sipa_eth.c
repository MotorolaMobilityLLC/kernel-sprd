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

#define pr_fmt(fmt) "sipa_eth: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/if_arp.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include "sipa_eth.h"

/* Device status */
#define DEV_ON 1
#define DEV_OFF 0

#define SIPA_ETH_NAPI_WEIGHT 64

static struct dentry *root;
static int sipa_eth_debugfs_mknod(void *root, void *data);

static u32 gro_enable;

static inline void sipa_eth_dt_stats_init(struct sipa_eth_dtrans_stats *stats)
{
	memset(stats, 0, sizeof(*stats));
}

static inline void sipa_eth_rx_stats_update(
			struct sipa_eth_dtrans_stats *stats, u32 len)
{
	stats->rx_sum += len;
	stats->rx_cnt++;
}

static inline void sipa_eth_tx_stats_update(
			struct sipa_eth_dtrans_stats *stats, u32 len)
{
	stats->tx_sum += len;
	stats->tx_cnt++;
}

static void sipa_eth_prepare_skb(struct SIPA_ETH *sipa_eth, struct sk_buff *skb)
{
	int netid;
	struct iphdr *iph;
	struct net_device *dev;
	struct sipa_eth_init_data *pdata = sipa_eth->pdata;

	netid = pdata->netid;
	dev = sipa_eth->netdev;

	/* eth is rawip */
	if (netid == -1) {
		skb->protocol = eth_type_trans(skb, sipa_eth->netdev);
		skb_set_network_header(skb, ETH_HLEN);
	} else {
		skb_reset_network_header(skb);
		iph = ip_hdr(skb);
		if (iph->version == 4)
			skb->protocol = htons(ETH_P_IP);
		else
			skb->protocol = htons(ETH_P_IPV6);
	}

	/* TODO chechsum ... */
	skb->ip_summed = CHECKSUM_NONE;
	skb->dev = dev;
}

static int sipa_eth_rx(struct SIPA_ETH *sipa_eth, int budget)
{
	struct sk_buff *skb;
	struct net_device *dev;
	struct sipa_eth_dtrans_stats *dt_stats;
	int skb_cnt = 0;
	int rx_bytes = 0;
	int ret;

	dt_stats = &sipa_eth->dt_stats;

	if (!sipa_eth) {
		pr_err("no sipa_eth device\n");
		return -EINVAL;
	}

	dev = sipa_eth->netdev;

	while (skb_cnt < budget) {
		ret = sipa_nic_rx(sipa_eth->nic_id, &skb);

		if (ret) {
			switch (ret) {
			case -ENODEV:
				pr_err("fail to find dev");
				dt_stats->rx_fail++;
				break;
			case -ENODATA:
				pr_debug("no more skb to recv");
				break;
			}
			break;
		}

		if (!skb) {
			pr_err("recv skb is null\n");
			return -EINVAL;
		}

		sipa_eth_prepare_skb(sipa_eth, skb);
		rx_bytes += skb->len;
		sipa_eth_rx_stats_update(dt_stats, skb->len);

		if (gro_enable)
			napi_gro_receive(&sipa_eth->napi, skb);
		else
			netif_receive_skb(skb);

		skb_cnt++;
	}

	dev->stats.rx_packets += skb_cnt;
	dev->stats.rx_bytes += rx_bytes;

	return skb_cnt;
}

static int sipa_eth_rx_poll_handler(struct napi_struct *napi, int budget)
{
	struct SIPA_ETH *sipa_eth = container_of(napi, struct SIPA_ETH, napi);
	int pkts;

	pkts = sipa_eth_rx(sipa_eth, budget);

	/* If the number of pkt is more than weight(64),
	 * we cannot read them all with a single poll.
	 * When the return value of poll func equals to weight(64),
	 * napi structure invokes the poll func one more time by
	 * __raise_softirq_irqoff.(See napi_poll for details)
	 * So do not do napi_complete in that case.
	 */
	if (pkts < budget)
		napi_complete(napi);
	return pkts;
}

static void sipa_eth_rx_handler (void *priv)
{
	struct SIPA_ETH *sipa_eth = (struct SIPA_ETH *)priv;

	if (!sipa_eth) {
		pr_err("data is NULL\n");
		return;
	}
	napi_schedule(&sipa_eth->napi);
	/* Trigger a NET_RX_SOFTIRQ softirq directly, or there will be a delay */
	raise_softirq(NET_RX_SOFTIRQ);
}

static void sipa_eth_flowctrl_handler(void *priv, int flowctrl)
{
	struct SIPA_ETH *sipa_eth = (struct SIPA_ETH *)priv;
	struct net_device *dev = sipa_eth->netdev;

	if (flowctrl) {
		netif_stop_queue(dev);
	} else if (netif_queue_stopped(dev)) {
		netif_wake_queue(dev);
	}
}

static void sipa_eth_notify_cb(void *priv, enum sipa_evt_type evt,
			       unsigned long data)
{
	switch (evt) {
	case SIPA_RECEIVE:
		sipa_eth_rx_handler(priv);
		break;
	case SIPA_LEAVE_FLOWCTRL:
		pr_info("SIPA LEAVE FLOWCTRL\n");
		sipa_eth_flowctrl_handler(priv, 0);
		break;
	case SIPA_ENTER_FLOWCTRL:
		pr_info("SIPA ENTER FLOWCTRL\n");
		sipa_eth_flowctrl_handler(priv, 1);
		break;
	default:
		break;
	}
}

static int sipa_eth_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct SIPA_ETH *sipa_eth = netdev_priv(dev);
	struct sipa_eth_init_data *pdata = sipa_eth->pdata;
	struct sipa_eth_dtrans_stats *dt_stats;
	int ret = 0;
	int netid;

	dt_stats = &sipa_eth->dt_stats;
	if (sipa_eth->state != DEV_ON) {
		pr_err("called when %s is down\n", dev->name);
		dt_stats->tx_fail++;
		netif_carrier_off(dev);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	netid = pdata->netid;
	/* eth is rawip, so pull 14 bytes */
	if (netid >= 0)
		skb_pull_inline(skb, ETH_HLEN);

	ret = sipa_nic_tx(sipa_eth->nic_id, pdata->term_type, netid, skb);
	if (unlikely(ret != 0)) {
		pr_err("fail to send skb, ret %d\n", ret);
		if (ret == -ENOMEM || ret == -EAGAIN) {
			dt_stats->tx_fail++;
			netif_stop_queue(dev);
			return NETDEV_TX_BUSY;
		}
		goto err;
	}

	/* update netdev statistics */
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;
	sipa_eth_tx_stats_update(dt_stats, skb->len);

	return NETDEV_TX_OK;

err:
	sipa_eth->netdev->stats.tx_dropped++;
	dev_kfree_skb_any(skb);
	return NETDEV_TX_OK;
}

/* Open interface */
static int sipa_eth_open(struct net_device *dev)
{
	struct SIPA_ETH *sipa_eth = netdev_priv(dev);
	struct sipa_eth_init_data *pdata = sipa_eth->pdata;
	int ret = 0;

	pr_info("open %s netid %d term_type %d\n",
		dev->name, pdata->netid, pdata->term_type);
	ret = sipa_nic_open(
		pdata->term_type,
		pdata->netid,
		sipa_eth_notify_cb,
		(void *)sipa_eth);

	if (ret < 0)
		return -EINVAL;

	sipa_eth->nic_id = ret;
	sipa_eth->state = DEV_ON;

	sipa_eth_dt_stats_init(&sipa_eth->dt_stats);

	if (!netif_carrier_ok(sipa_eth->netdev)) {
		pr_info("set netif_carrier_on\n");
		netif_carrier_on(sipa_eth->netdev);
	}
	napi_enable(&sipa_eth->napi);
	netif_start_queue(dev);

	return 0;
}

/* Close interface */
static int sipa_eth_close(struct net_device *dev)
{
	struct SIPA_ETH *sipa_eth = netdev_priv(dev);

	pr_info("close %s!\n", dev->name);

	sipa_nic_close(sipa_eth->nic_id);
	sipa_eth->state = DEV_OFF;

	napi_disable(&sipa_eth->napi);
	netif_stop_queue(dev);

	return 0;
}

static struct net_device_stats *sipa_eth_get_stats(struct net_device *dev)
{
	struct SIPA_ETH *sipa_eth = netdev_priv(dev);

	return &sipa_eth->stats;
}

static const struct net_device_ops sipa_eth_ops = {
	.ndo_open = sipa_eth_open,
	.ndo_stop = sipa_eth_close,
	.ndo_start_xmit = sipa_eth_start_xmit,
	.ndo_get_stats = sipa_eth_get_stats,
};

static int sipa_eth_parse_dt(
	struct sipa_eth_init_data **init,
	struct device *dev)
{
	struct sipa_eth_init_data *pdata = NULL;
	struct device_node *np = dev->of_node;
	int ret;
	u32 data, id;

	if (!np)
		pr_err("dev of_node np is null\n");

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	id = of_alias_get_id(np, "eth");
	switch (id) {
	case 0 ... 7:
		snprintf(pdata->name, IFNAMSIZ, "sipa_eth%d", id);
		break;
	case 8:
		strncpy(pdata->name, "sipa_usb0", IFNAMSIZ);
		break;
	case 9:
		strncpy(pdata->name, "sipa_wlan0", IFNAMSIZ);
		break;
	default:
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "sprd,netid", &data);
	if (ret) {
		pr_err("read sprd,netid ret %d\n", ret);
		return ret;
	}
	/* dts reflect */
	pdata->netid = data - 1;

	ret = of_property_read_u32(np, "sprd,term-type", &data);
	if (ret) {
		pr_err("read sprd,term-type ret %d\n", ret);
		return ret;
	}

	pdata->term_type = data;
	*init = pdata;
	pr_debug("after dt parse, name %s netid %d term-type %d\n",
		 pdata->name, pdata->netid, pdata->term_type);
	return 0;
}

static void s_setup(struct net_device *dev)
{
	ether_setup(dev);
	/* avoid mdns to be send */
	dev->flags &= ~(IFF_BROADCAST | IFF_MULTICAST);
}

static inline void sipa_eth_destroy_pdata(struct sipa_eth_init_data **init)
{
	struct sipa_eth_init_data *pdata = *init;

	kfree(pdata);
	*init = NULL;
}

static int sipa_eth_probe(struct platform_device *pdev)
{
	struct sipa_eth_init_data *pdata = pdev->dev.platform_data;
	struct net_device *netdev;
	struct SIPA_ETH *sipa_eth;
	char ifname[IFNAMSIZ];
	int ret;

	if (pdev->dev.of_node && !pdata) {
		ret = sipa_eth_parse_dt(&pdata, &pdev->dev);
		if (ret) {
			pr_err("failed to parse device tree, ret=%d\n", ret);
			return ret;
		}
	}

	strlcpy(ifname, pdata->name, IFNAMSIZ);
	netdev = alloc_netdev(
		sizeof(struct SIPA_ETH),
		ifname,
		NET_NAME_UNKNOWN,
		strncmp("sipa_eth", ifname, 8) == 0 ? s_setup : ether_setup);

	if (!netdev) {
		pr_err("alloc_netdev() failed.\n");
		return -ENOMEM;
	}

	/*
	 * If net_device's type is ARPHRD_ETHER, the ipv6 interface identifier
	 * specified by the network will be covered by addrconf_ifid_eui48,
	 * this will casue ipv6 fail in some test environment.
	 * So set the sipa_eth net_device's type to ARPHRD_RAWIP here.
	 */
	if (!strncmp("sipa_eth", pdata->name, 8))
		netdev->type = ARPHRD_RAWIP;

	sipa_eth = netdev_priv(netdev);
	sipa_eth_dt_stats_init(&sipa_eth->dt_stats);
	sipa_eth->netdev = netdev;
	sipa_eth->pdata = pdata;
	netdev->netdev_ops = &sipa_eth_ops;
	netdev->watchdog_timeo = 1 * HZ;
	netdev->irq = 0;
	netdev->dma = 0;

	random_ether_addr(netdev->dev_addr);

	netif_napi_add(netdev,
		       &sipa_eth->napi,
		       sipa_eth_rx_poll_handler,
		       SIPA_ETH_NAPI_WEIGHT);

	/* Register new Ethernet interface */
	ret = register_netdev(netdev);
	if (ret) {
		pr_err("register_netdev() failed (%d)\n", ret);
		netif_napi_del(&sipa_eth->napi);
		free_netdev(netdev);
		sipa_eth_destroy_pdata(&pdata);
		return ret;
	}

	sipa_eth->state = DEV_OFF;
	/* Set link as disconnected */
	netif_carrier_off(netdev);
	platform_set_drvdata(pdev, sipa_eth);
	sipa_eth_debugfs_mknod(root, (void *)sipa_eth);
	return 0;
}

/* Cleanup Ethernet device driver. */
static int sipa_eth_remove(struct platform_device *pdev)
{
	struct SIPA_ETH *sipa_eth = platform_get_drvdata(pdev);
	struct sipa_eth_init_data *pdata = sipa_eth->pdata;

	netif_napi_del(&sipa_eth->napi);
	sipa_eth_destroy_pdata(&pdata);
	unregister_netdev(sipa_eth->netdev);
	free_netdev(sipa_eth->netdev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id sipa_eth_match_table[] = {
	{ .compatible = "sprd,sipa_eth"},
	{ }
};

static struct platform_driver sipa_eth_driver = {
	.probe = sipa_eth_probe,
	.remove = sipa_eth_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sipa_eth",
		.of_match_table = sipa_eth_match_table
	}
};

static int sipa_eth_debug_show(struct seq_file *m, void *v)
{
	struct SIPA_ETH *sipa_eth = (struct SIPA_ETH *)(m->private);
	struct sipa_eth_dtrans_stats *stats;
	struct sipa_eth_init_data *pdata;

	if (!sipa_eth) {
		pr_err("invalid data, sipa_eth is NULL\n");
		return -EINVAL;
	}
	pdata = sipa_eth->pdata;
	stats = &sipa_eth->dt_stats;

	seq_puts(m, "*************************************************\n");
	seq_printf(m, "DEVICE: %s, term_type %d, netid %d, state %s\n",
		   pdata->name, pdata->term_type, pdata->netid,
		   sipa_eth->state == DEV_ON ? "UP" : "DOWN");
	seq_puts(m, "\nRX statistics:\n");
	seq_printf(m, "rx_sum=%u, rx_cnt=%u\n",
		   stats->rx_sum,
		   stats->rx_cnt);
	seq_printf(m, "rx_fail=%u\n",
		   stats->rx_fail);

	seq_puts(m, "\nTX statistics:\n");
	seq_printf(m, "tx_sum=%u, tx_cnt=%u\n",
		   stats->tx_sum,
		   stats->tx_cnt);
	seq_printf(m, "tx_fail=%u\n",
		   stats->tx_fail);

	seq_puts(m, "*************************************************\n");

	return 0;
}

static int sipa_eth_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, sipa_eth_debug_show, inode->i_private);
}

static const struct file_operations sipa_eth_debug_fops = {
	.open = sipa_eth_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int debugfs_gro_enable_get(void *data, u64 *val)
{
	*val = gro_enable;
	return 0;
}

static int debugfs_gro_enable_set(void *data, u64 val)
{
	gro_enable = *(u32 *)val;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_gro_enable,
			debugfs_gro_enable_get,
			debugfs_gro_enable_set,
			"%llu\n");

static int sipa_eth_debugfs_mknod(void *root, void *data)
{
	struct SIPA_ETH *sipa_eth = (struct SIPA_ETH *)data;

	if (!sipa_eth)
		return -ENODEV;

	if (!root)
		return -ENXIO;

	debugfs_create_file("sipa_eth",
			    0444,
			    (struct dentry *)root,
			    data,
			    &sipa_eth_debug_fops);

	debugfs_create_file("gro_enable",
			    0600,
			    (struct dentry *)root,
			    &gro_enable,
			    &fops_gro_enable);

	return 0;
}

static void __init sipa_eth_debugfs_init(void)
{
	root = debugfs_create_dir("sipa_eth", NULL);
	if (!root)
		pr_err("failed to create sipa_eth debugfs dir\n");
}

static int __init sipa_eth_init(void)
{
	sipa_eth_debugfs_init();
	return platform_driver_register(&sipa_eth_driver);
}

static void __exit sipa_eth_exit(void)
{
	platform_driver_unregister(&sipa_eth_driver);
}

module_init(sipa_eth_init);
module_exit(sipa_eth_exit);

MODULE_AUTHOR("Wade.Shu");
MODULE_DESCRIPTION("Unisoc Ethernet device driver for IPA");
MODULE_LICENSE("GPL v2");
