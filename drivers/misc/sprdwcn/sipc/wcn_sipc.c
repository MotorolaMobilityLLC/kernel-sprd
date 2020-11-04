/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <misc/wcn_bus.h>

#include "bus_common.h"
#include "wcn_integrate.h"
#include "wcn_sipc.h"
#include "wcn_txrx.h"

#define WCN_SIPC_CHANNEL_MAX CHN_MAX_NUM
#define WCN_SIPC_CHANNEL_STATIC BITS_PER_LONG
#define WCN_SIPC_CHANNEL_DYNAMIC \
	(WCN_SIPC_CHANNEL_MAX - WCN_SIPC_CHANNEL_STATIC)

static struct wcn_sipc_info_t g_sipc_info = {0};
static DECLARE_BITMAP(wcn_chn_map, WCN_SIPC_CHANNEL_MAX);

#define SIPC_VALID_CHN(index) \
	((index < SIPC_CHN_NUM) ? test_bit(index, wcn_chn_map) : 0)
#define SIPC_INVALID_CHN(index) (!SIPC_VALID_CHN(index))
#define SIPC_TYPE(ops) (ops->chn_config.sipc_ch.type)
#define SIPC_CHN(ops) (&ops->chn_config.sipc_ch)

static int wcn_sipc_alloc_bit(unsigned long *bitmap, int size)
{
	int bit;

	do {
		bit = find_first_zero_bit(bitmap, size);
		if (bit >= size)
			return -EAGAIN;
	} while (test_and_set_bit(bit, bitmap));

	return bit;
}

static int wcn_sipc_recv(struct wcn_sipc_chn_info *sipc_chn, void *buf, int len)
{
	struct mbuf_t *head, *tail;
	struct mchn_ops_t *wcn_sipc_ops = NULL;

	wcn_sipc_ops = chn_ops(sipc_chn->index);
	if (unlikely(!wcn_sipc_ops))
		return -E_NULLPOINT;

	head = kzalloc(sizeof(*head), GFP_KERNEL);
	if (unlikely(!head))
		return -E_NOMEM;

	head->buf = buf;
	head->len = len;
	head->next = NULL;
	tail = head;
	wcn_sipc_ops->pop_link(sipc_chn->index, head, tail, 1);

	return 0;
}

static int wcn_sipc_sbuf_write(u8 index, void *buf, int len)
{
	int cnt = -1;
	struct wcn_sipc_chn_info *sipc_chn;
	struct mchn_ops_t *wcn_sipc_ops = NULL;

	if (SIPC_INVALID_CHN(index))
		return -E_INVALIDPARA;

	wcn_sipc_ops = chn_ops(index);
	if (unlikely(!wcn_sipc_ops))
		return -E_NULLPOINT;

	sipc_chn = SIPC_CHN(wcn_sipc_ops);
	cnt = sbuf_write(sipc_chn->dst, sipc_chn->channel,
			 sipc_chn->sbuf.bufid, buf, len, -1);
	WCN_INFO("sbuf chn[%s] write cnt=%d\n", sipc_chn->name, cnt);

	return cnt;
}

static void wcn_sipc_sbuf_rx_process(struct wcn_sipc_chn_info *pchn)
{
	int cnt, ret;
	u8 *buf;

	do {
		buf = kzalloc(pchn->sbuf.len, GFP_KERNEL);
		if (unlikely(!buf)) {
			WCN_ERR("[%s]:mem alloc fail!\n", __func__);
			return;
		}
		cnt = sbuf_read(pchn->dst, pchn->channel,
				pchn->sbuf.bufid,
				(void *)buf,
				pchn->sbuf.len, 0);
		if (cnt <= 0) {
			kfree(buf);
			return;
		}
		ret = wcn_sipc_recv(pchn, buf, cnt);
		if (ret < 0) {
			WCN_ERR("%s recv fail[%d]\n", pchn->name, ret);
			kfree(buf);
			return;
		}
	} while (cnt > 0);
}

static void wcn_sipc_sbuf_notifer(int event, void *data)
{
	struct wcn_sipc_chn_info *sipc_chn = (struct wcn_sipc_chn_info *)data;

	if (unlikely(!sipc_chn))
		return;

	switch (event) {
	case SBUF_NOTIFY_WRITE:
		break;
	case SBUF_NOTIFY_READ:
		wcn_sipc_sbuf_rx_process(sipc_chn);
		break;
	default:
		WCN_ERR("sbuf read event[%d] invalid\n", event);
	}
}

static int wcn_sipc_sblk_write(u8 index, void *buf, int len)
{
	int ret = -1;
	u8 *addr = NULL;
	struct swcnblk_blk blk;
	struct wcn_sipc_chn_info *sipc_chn;
	struct mchn_ops_t *wcn_sipc_ops = NULL;

	if (SIPC_INVALID_CHN(index))
		return -E_INVALIDPARA;

	wcn_sipc_ops = chn_ops(index);
	if (unlikely(!wcn_sipc_ops))
		return -E_NULLPOINT;

	sipc_chn = SIPC_CHN(wcn_sipc_ops);
	/* Get a free swcnblk. */
	ret = swcnblk_get(sipc_chn->dst, sipc_chn->channel, &blk, 0);
	if (ret) {
		WCN_ERR("[%s]:Failed to get free swcnblk(%d)!\n",
			sipc_chn->name, ret);
		return -ENOMEM;
	}
	if (blk.length < len) {
		WCN_ERR("[%s]:The size of swcnblk is so tiny!\n",
			sipc_chn->name);
		swcnblk_put(sipc_chn->dst, sipc_chn->channel, &blk);
		return E_INVALIDPARA;
	}
	addr = (u8 *)blk.addr + SIPC_SBLOCK_HEAD_RESERV;
	blk.length = len + SIPC_SBLOCK_HEAD_RESERV;
	memcpy(((u8 *)addr), buf, len);
	ret = swcnblk_send_prepare(sipc_chn->dst, sipc_chn->channel, &blk);
	if (ret) {
		WCN_ERR("[%s]:err:%d\n", sipc_chn->name, ret);
		swcnblk_put(sipc_chn->dst, sipc_chn->channel, &blk);
	}

	return ret;
}

static void wcn_sipc_sblk_recv(struct wcn_sipc_chn_info *sipc_chn)
{
	u32 length = 0;
	int ret = -1;
	struct swcnblk_blk blk;

	WCN_DEBUG("[%s]:recv sblock msg", sipc_chn->name);

	while (!swcnblk_receive(sipc_chn->dst, sipc_chn->channel, &blk, 0)) {
		length = blk.length - SIPC_SBLOCK_HEAD_RESERV;
		WCN_DEBUG("sblk length %d", length);
		wcn_sipc_recv(sipc_chn,
			      (u8 *)blk.addr + SIPC_SBLOCK_HEAD_RESERV, length);
		ret = swcnblk_release(sipc_chn->dst, sipc_chn->channel, &blk);
		if (ret)
			WCN_ERR("release swcnblk[%d] err:%d\n",
				sipc_chn->channel, ret);
	}
}

static void wcn_sipc_sblk_notifer(int event, void *data)
{
	struct wcn_sipc_chn_info *sipc_chn = (struct wcn_sipc_chn_info *)data;

	if (unlikely(!sipc_chn))
		return;
	switch (event) {
	case SBLOCK_NOTIFY_RECV:
		wcn_sipc_sblk_recv(sipc_chn);
		break;
	/* SBLOCK_NOTIFY_GET cmd not need process it */
	case SBLOCK_NOTIFY_GET:
		break;
	default:
		WCN_ERR("Invalid event swcnblk notify:%d\n", event);
		break;
	}
}

struct wcn_sipc_data_ops  sipc_data_ops[] = {
	{
		.sipc_write = wcn_sipc_sbuf_write,
		.sipc_notifer = wcn_sipc_sbuf_notifer,
	},
	{
		.sipc_write = wcn_sipc_sblk_write,
		.sipc_notifer = wcn_sipc_sblk_notifer,
	},
};

static int wcn_sipc_trans_notify(struct wcn_sipc_chn_info *pchn,
				 int init)
{
	int ret = 0;
	void (*handler)(int event, void *data) = NULL;
	void *data = NULL;

	if (pchn->type == WCN_SIPC_TRANS_SBUF) {
		if (init) {
			handler = sipc_data_ops[pchn->type].sipc_notifer;
			data = pchn;
		}
		ret = sbuf_register_notifier(
				pchn->dst, pchn->channel,
				pchn->sbuf.bufid,
				handler, data);
		if (ret < 0)
			WCN_ERR(
				"sbuf dst:%d chn:%d bufid:%d register:%d fail!\n",
				pchn->dst, pchn->channel,
				pchn->sbuf.bufid, init);
		else
			WCN_INFO(
				 "sbuf dst:%d chn:%d bufid:%d register:%d ok!\n",
				 pchn->dst, pchn->channel,
				 pchn->sbuf.bufid, init);
	} else if (pchn->type == WCN_SIPC_TRANS_SBLOCK) {
		if (init) {
			handler = sipc_data_ops[pchn->type].sipc_notifer;
			data = pchn;
		}
		ret = swcnblk_register_notifier(
				pchn->dst, pchn->channel,
				handler, data);
		if (ret < 0)
			WCN_ERR("sblock dst[%d] chn[%d] register:%d fail!\n",
				pchn->dst, pchn->channel, init);
		else
			WCN_INFO("sblock dst[%d] chn[%d] register:%d ok!\n",
				 pchn->dst, pchn->channel, init);
	}

	return ret;
}

static inline int wcn_sipc_trans_create(struct wcn_sipc_chn_info *pchn)
{
	int ret = 0;
	struct swcnblk_create_info blk_info;

	if (!(pchn->flag & WCN_CHN_CREATE))
		goto register_handle;

	if (pchn->type == WCN_SIPC_TRANS_SBUF) {
		ret = sbuf_create(pchn->dst, pchn->channel,
				  pchn->sbuf.bufnum,
				  pchn->sbuf.txbufsize,
				  pchn->sbuf.rxbufsize);
		if (ret < 0)
			WCN_ERR("sbuf dst:%d chn:%d bufid[%d] create fail!\n",
				pchn->dst, pchn->channel, pchn->sbuf.bufnum);
		else
			WCN_INFO("sbuf dst:%d chn:%d bufid[%d] create ok!\n",
				 pchn->dst, pchn->channel, pchn->sbuf.bufnum);
	} else if (pchn->type == WCN_SIPC_TRANS_SBLOCK) {
		memset(&blk_info, 0, sizeof(struct swcnblk_create_info));
		blk_info.dst = pchn->dst;
		blk_info.channel = pchn->channel;
		blk_info.txblocknum = pchn->sblk.txblocknum,
		blk_info.txblocksize = pchn->sblk.txblocksize,
		blk_info.rxblocknum = pchn->sblk.rxblocknum,
		blk_info.rxblocksize = pchn->sblk.rxblocksize,
		blk_info.basemem = pchn->sblk.basemem,
		blk_info.alignsize = pchn->sblk.alignsize,
		blk_info.mapped_smem_base = pchn->sblk.mapped_smem_base,
		ret = swcnblk_create(&blk_info, NULL, NULL);
		if (ret < 0)
			WCN_ERR("sblock dst:%d chn[%d] create fail!\n",
				pchn->dst, pchn->channel);
		else
			WCN_INFO("sblock dst:%d chn[%d] create fail!\n",
				 pchn->dst, pchn->channel);
	}

register_handle:
	if (pchn->flag & WCN_CHN_CALLBACK) {
		ret = wcn_sipc_trans_notify(pchn, 1);
		if (ret && (pchn->flag & WCN_CHN_CREATE)) {
			if (pchn->type == WCN_SIPC_TRANS_SBUF)
				sbuf_destroy(pchn->dst, pchn->channel);
			else if (pchn->type == WCN_SIPC_TRANS_SBLOCK)
				swcnblk_destroy(pchn->dst, pchn->channel);
		}
	}

	return ret;
}

static inline int wcn_sipc_trans_destroy(struct wcn_sipc_chn_info *pchn)
{
	if (pchn->flag & WCN_CHN_CALLBACK)
		wcn_sipc_trans_notify(pchn, 0);

	if (pchn->flag & WCN_CHN_CREATE) {
		if (pchn->type == WCN_SIPC_TRANS_SBUF)
			sbuf_destroy(pchn->dst, pchn->channel);
		else if (pchn->type == WCN_SIPC_TRANS_SBLOCK)
			swcnblk_destroy(pchn->dst, pchn->channel);
	}

	return 0;
}

static int wcn_sipc_chn_init(struct mchn_ops_t *ops)
{
	int index;
	int ret;
	struct wcn_sipc_chn_info *pchn;

	if (unlikely(!ops))
		return -EINVAL;

	pchn = &ops->chn_config.sipc_ch;

	if (pchn->flag & WCN_CHN_DYNAMIC) {
		index = wcn_sipc_alloc_bit(wcn_chn_map + 1,
					   WCN_SIPC_CHANNEL_DYNAMIC);
		if (unlikely(index < 0)) {
			WCN_ERR("%s chn is full\n", __func__);
			return index;
		}
		index += WCN_SIPC_CHANNEL_STATIC;
		ops->channel = index;
		pchn->index = index;
	} else if (ops->channel < WCN_SIPC_CHANNEL_STATIC &&
		   !test_and_set_bit(ops->channel, wcn_chn_map)) {
		index = ops->channel;
		pchn->index = ops->channel;
	} else {
		WCN_ERR("%s static chn:%d already allocked\n",
			__func__, ops->channel);
		return -EAGAIN;
	}

	ret = wcn_sipc_trans_create(pchn);
	if (ret) {
		clear_bit(index, wcn_chn_map);
		return -ENODEV;
	}

	bus_chn_init(ops, HW_TYPE_SIPC);

	return index;
}

static int wcn_sipc_chn_deinit(struct mchn_ops_t *ops)
{
	int index;
	struct wcn_sipc_chn_info *sipc_chn;

	if (SIPC_INVALID_CHN(ops->channel))
		return -E_INVALIDPARA;
	sipc_chn = SIPC_CHN(ops);
	wcn_sipc_trans_destroy(sipc_chn);
	index = ops->channel;

	bus_chn_deinit(ops);
	clear_bit(index, wcn_chn_map);

	return 0;
}

static inline int wcn_sipc_buf_list_alloc(int chn,
					  struct mbuf_t **head,
					  struct mbuf_t **tail,
					  int *num)
{
	return buf_list_alloc(chn, head, tail, num);
}

static inline int wcn_sipc_buf_list_free(int chn,
					 struct mbuf_t *head,
					 struct mbuf_t *tail,
					 int num)
{
	return buf_list_free(chn, head, tail, num);
}

static int wcn_sipc_buf_push(int index, struct mbuf_t *head,
			     struct mbuf_t *tail, int num)
{
	struct mchn_ops_t *wcn_sipc_ops = NULL;

	wcn_sipc_ops = chn_ops(index);
	if (unlikely(!wcn_sipc_ops))
		return -E_NULLPOINT;

	if (wcn_sipc_ops->inout == WCNBUS_TX) {
		sipc_data_ops[SIPC_TYPE(wcn_sipc_ops)].sipc_write(
					index, (void *)(head->buf), head->len);
		wcn_sipc_ops->pop_link(index, head, tail, num);
	} else if (wcn_sipc_ops->inout == WCNBUS_RX) {
		/* free buf mem */
		if (SIPC_TYPE(wcn_sipc_ops) == WCN_SIPC_TRANS_SBUF)
			kfree(head->buf);
		/* free buf head */
		kfree(head);
	} else {
		return -E_INVALIDPARA;
	}

	return 0;
}

static inline unsigned int wcn_sipc_get_status(void)
{
	return g_sipc_info.sipc_chn_status;
}

static inline void wcn_sipc_set_status(unsigned int flag)
{
	mutex_lock(&g_sipc_info.status_lock);
	g_sipc_info.sipc_chn_status = flag;
	mutex_unlock(&g_sipc_info.status_lock);
}

static unsigned long long wcn_sipc_get_rxcnt(void)
{
	return wcn_get_cp2_comm_rx_count();
}

static void wcn_sipc_module_init(void)
{
	mutex_init(&g_sipc_info.status_lock);
	WCN_INFO("sipc module init success\n");
}

static void wcn_sipc_module_deinit(void)
{
	mutex_destroy(&g_sipc_info.status_lock);
	WCN_INFO("sipc module deinit success\n");
}

static enum wcn_hard_intf_type wcn_sipc_get_hwintf_type(void)
{
	return HW_TYPE_SIPC;
}

static struct sprdwcn_bus_ops sipc_bus_ops = {
	.chn_init = wcn_sipc_chn_init,
	.chn_deinit = wcn_sipc_chn_deinit,
	.list_alloc = wcn_sipc_buf_list_alloc,
	.list_free = wcn_sipc_buf_list_free,
	.push_list = wcn_sipc_buf_push,
	.get_hwintf_type = wcn_sipc_get_hwintf_type,
	.get_carddump_status = wcn_sipc_get_status,
	.set_carddump_status = wcn_sipc_set_status,
	.get_rx_total_cnt = wcn_sipc_get_rxcnt,
};

void module_bus_init(void)
{
	wcn_sipc_module_init();
	module_ops_register(&sipc_bus_ops);
	WCN_INFO("sipc bus init success\n");
}
EXPORT_SYMBOL(module_bus_init);

void module_bus_deinit(void)
{
	module_ops_unregister();
	wcn_sipc_module_deinit();
	WCN_INFO("sipc bus deinit success\n");
}
EXPORT_SYMBOL(module_bus_deinit);
