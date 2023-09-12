/*
 * Copyright (C) 2016 Spreadtrum Communications Inc.
 *
 * Authors	:
 * star.liu <star.liu@spreadtrum.com>
 * yifei.li <yifei.li@spreadtrum.com>
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

#include <linux/prefetch.h>
#include "rx_msg_sc2355.h"
#include "tx_msg_sc2355.h"
#include "cmdevt.h"
#include "work.h"
#include <linux/dma-mapping.h>
#ifdef SIPC_SUPPORT
#include "sipc_txrx_mm.h"
#endif

extern struct sprdwl_intf_sc2355 g_intf_sc2355;

#define GET_NEXT_ADDR_TRANS_VALUE(value, offset) \
	(struct sprdwl_addr_trans_value *)((unsigned char *)value + offset)

#define SKB_SHARED_INFO_OFFSET \
	SKB_DATA_ALIGN(SPRDWL_MAX_DATA_RXLEN + NET_SKB_PAD)

void check_mh_buffer(struct device *dev, void *buffer, dma_addr_t pa,
		     size_t size, enum dma_data_direction direction)
{
#define MAX_RETRY_NUM 8
	int retry = 0;

	if (direction == DMA_FROM_DEVICE) {
		struct rx_msdu_desc *desc = buffer + sizeof(struct rx_mh_desc);

		/* Check whether this buffer is ok to use */
		while ((desc->data_write_done != 1) &&
		       (retry < MAX_RETRY_NUM)) {
			wl_err("%s: hw still writing: 0x%lx, 0x%lx\n",
			       __func__, (unsigned long)buffer,
			       (unsigned long)pa);
			/* FIXME: Should we delay here? */
			dma_sync_single_for_device(dev, pa, size, direction);
			retry++;
		}
	}

	if (retry >= MAX_RETRY_NUM) {
		/* TODO: How to deal with this situation? */
		dma_sync_single_for_device(dev, pa, size, direction);
		wl_err("%s: hw still writing: 0x%lx, 0x%lx\n",
		       __func__, (unsigned long)buffer, (unsigned long)pa);
	}
}

void clear_mh_buffer(void *buffer)
{
	struct rx_msdu_desc *desc = buffer + sizeof(struct rx_mh_desc);

	desc->data_write_done = 0;
}

unsigned long mm_virt_to_phys(struct device *dev, void *buffer, size_t size,
			      enum dma_data_direction direction)
{
	dma_addr_t pa = 0;
	unsigned long pcie_addr = 0;

again:
	if (direction == DMA_FROM_DEVICE)
		clear_mh_buffer(buffer);

	mb();

	pa = dma_map_single(dev, buffer, size, direction);

#if 0
	printk("buffer: %lx\n", (unsigned long)buffer);
	printk("pa: %lx\n", (unsigned long)pa);
	printk("virt_to_phys: %lx\n", (unsigned long)virt_to_phys(buffer));
	//printk("virt_to_dma: %lx\n", (unsigned long)virt_to_dma(dev, buffer));
	printk("dma_to_phys: %lx\n", (unsigned long)dma_to_phys(dev, pa));
#endif
	if (likely(!dma_mapping_error(dev, pa)))
		pcie_addr = pa | SPRDWL_MH_ADDRESS_BIT;
	else {
		wl_err("%s: dma_mapping_error\n", __func__);
		goto again;
	}

	return pcie_addr;
}

void *mm_phys_to_virt(struct device *dev, unsigned long pcie_addr, size_t size,
		      enum dma_data_direction direction, bool is_mh)
{
	dma_addr_t pa = 0;
	void *buffer = NULL;

	pa = pcie_addr & (~(SPRDWL_MH_ADDRESS_BIT) & SPRDWL_PHYS_MASK);
	buffer = phys_to_virt(pa);

	if (is_mh)
		check_mh_buffer(dev, buffer, pa, size, direction);

	dma_unmap_single(dev, pa, size, direction);

	return buffer;
}

static inline struct sk_buff *mm_build_skb(void *data, int len, int buffer_type)
{
	return build_skb(data, (buffer_type ? len : 0));
}

static struct sk_buff
*mm_data2skb_process(struct sprdwl_mm *mm_entry, void *data, int len)
{
	struct sk_buff *skb = NULL;

	skb = dev_alloc_skb(len);
	if (likely(skb))
		memcpy(skb->data, data, len);

	return skb;
}

static inline void mm_free_addr_buf(struct sprdwl_mm *mm_entry)
{
	void *p = (mm_entry->hdr - mm_entry->hif_offset);

	kfree(p);
	mm_entry->hdr = NULL;
	mm_entry->addr_trans = NULL;
}

/* Don't use this func direct, use sprdwl_rx_flush_buffer instead */
void mm_flush_buffer(struct sprdwl_mm *mm_entry)
{
	/* TODO: Should we stop something here? */
	skb_queue_purge(&mm_entry->buffer_list);
	mm_free_addr_buf(mm_entry);
	atomic_set(&mm_entry->alloc_num, 0);
	wl_err("%s, %d, alloc_num set to 0\n", __func__, __LINE__);
}

static inline void mm_alloc_addr_buf(struct sprdwl_mm *mm_entry)
{
	struct sprdwl_addr_trans_value *value = NULL;
	struct sprdwl_addr_hdr *hdr = NULL;
	void *p = NULL;

	p = kzalloc((mm_entry->hif_offset + SPRDWL_ADDR_BUF_LEN), GFP_KERNEL);
	if (likely(p)) {
		hdr = (struct sprdwl_addr_hdr *)(p + mm_entry->hif_offset);
		value = (struct sprdwl_addr_trans_value *)hdr->paydata;

		/* Tell CP that this CMD is used to add MH buffer */
		hdr->common.reserv = 1;
		/* NOTE: CP do not care ctx_id & rsp */
		hdr->common.ctx_id = 0;
		hdr->common.rsp = 0;
		hdr->common.type = SPRDWL_TYPE_DATA_PCIE_ADDR;

		value->type = 0;
		value->num = 0;
	}

	mm_entry->hdr = (void *)hdr;
	mm_entry->addr_trans = (void *)value;
}

static inline int mm_do_addr_buf(struct sprdwl_mm *mm_entry)
{
	struct sprdwl_rx_if *rx_if =
			container_of(mm_entry, struct sprdwl_rx_if, mm_entry);
	struct sprdwl_addr_trans_value *value =
		(struct sprdwl_addr_trans_value *)mm_entry->addr_trans;
	struct sprdwl_intf  *intf = rx_if->intf;
	struct sprdwl_vif *vif;
	int ret = 0;
	int addr_trans_len = 0;
	enum sprdwl_mode mode = SPRDWL_MODE_STATION;
	u8 mode_found = 0;

	/* NOTE: addr_buf should be allocating after being sent,
	 *       JUST avoid addr_buf allocating fail after being sent here
	 */
	if (0 == intf->fw_awake) {
		wl_info("%s, fw power save, need to wake up it!\n", __func__);
		for (mode = SPRDWL_MODE_STATION; mode < SPRDWL_MODE_MAX; mode++) {
			if (intf->priv->fw_stat[mode] == SPRDWL_INTF_OPEN) {
				mode_found = 1;
				break;
			}
		}
		if (mode_found == 0)
			return -EIO;
		vif = mode_to_vif(intf->priv, mode);
		if (!vif)
			return -EIO;
		sprdwl_put_vif(vif);
		if (!sprdwl_cmd_host_wakeup_fw(vif->priv, vif->ctx_id))
			intf->fw_power_down = 0;
		else {
			wl_err("%s, wake up fw failed!!\n", __func__);
			return -EIO;
		}
	}

	if (unlikely(!value)) {
		wl_debug("%s: addr buf is NULL, re-alloc here\n", __func__);
		mm_alloc_addr_buf(mm_entry);
		if (unlikely(!mm_entry->addr_trans)) {
			wl_err("%s: alloc addr buf fail!\n", __func__);
			ret = -ENOMEM;
		}
	} else if (value->num >= SPRDWL_MAX_ADD_MH_BUF_ONCE) {
		/* FIXME: temporary solution, would TX supply API for us? */
		/* TODO: How to do with tx fail? */

		/*wl_err("first 40 bytes in addr trans!\n");
		for (i = 0; i < 40; i++) {
			wl_err("0x%x\n", *((unsigned char *)mm_entry->hdr + i));
		}*/

		addr_trans_len = sizeof(struct sprdwl_addr_hdr) +
			sizeof(struct sprdwl_addr_trans_value) +
			(value->num*SPRDWL_PHYS_LEN);

		ret = if_tx_addr_trans(rx_if->intf, mm_entry->hdr,
					addr_trans_len, false);
		if (!ret) {
			mm_alloc_addr_buf(mm_entry);
			if (unlikely(!mm_entry->addr_trans)) {
				wl_err("%s: alloc addr buf fail!\n", __func__);
				ret = -ENOMEM;
			}
		} else {
			ret = -EIO;
		}
	}

	return ret;
}

static int mm_w_addr_buf(struct sprdwl_mm *mm_entry, unsigned long pcie_addr)
{
	int ret = 0;
	struct sprdwl_addr_trans_value *value = NULL;

	ret = mm_do_addr_buf(mm_entry);
	if (!ret) {
		value = (struct sprdwl_addr_trans_value *)mm_entry->addr_trans;

		/* NOTE: MH is little endian */
		memcpy((void *)value->address[value->num],
		       &pcie_addr, SPRDWL_PHYS_LEN);
		value->num++;
		/* do not care the result here */
		mm_do_addr_buf(mm_entry);
	}

	return ret;
}

static int mm_single_buffer_alloc(struct sprdwl_mm *mm_entry)
{
	struct sprdwl_rx_if *rx_if =
			container_of(mm_entry, struct sprdwl_rx_if, mm_entry);
	struct sk_buff *skb = NULL;
	unsigned long pcie_addr = 0;
	int ret = -ENOMEM;
	void *buff = NULL;
#ifdef SIPC_SUPPORT
	struct sipc_buf_node *node = NULL;
	struct sipc_buf_mm *rx_mm = NULL;
	void *pad = NULL;
#endif

	skb = dev_alloc_skb(SPRDWL_MAX_DATA_RXLEN);
	if (!skb) {
		wl_err("%s: alloc skb failed\n", __func__);
		return ret;
	}

	buff = skb->data;
	if (SPRDWL_HW_SIPC == rx_if->intf->priv->hw_type) {
#ifdef SIPC_SUPPORT
		rx_mm = rx_if->intf->sipc_mm->rx_buf;
		node = sipc_rx_alloc_node_buf(rx_if->intf);
		if (node) {
			memcpy(skb->data, &node, sizeof(node));
			pad = node->buf + SPRDWL_MAX_DATA_RXLEN;
			memcpy_toio(pad, &node, sizeof(node));
			buff = node->buf;
			node->priv = (void *)skb;
		} else {
			wl_err("%s: Node list is NULL. \n", __func__);
			dev_kfree_skb(skb);
			return -1;
		}
#endif
	}

	SAVE_ADDR(skb->data, skb, sizeof(struct sk_buff *));
	/* transfer virt to phys */
	if (SPRDWL_HW_SC2355_PCIE == rx_if->intf->priv->hw_type)
		pcie_addr = mm_virt_to_phys(&rx_if->intf->pdev->dev,
				buff, SPRDWL_MAX_DATA_RXLEN,
				DMA_FROM_DEVICE);
	else if (SPRDWL_HW_SIPC == rx_if->intf->priv->hw_type) {
#ifdef SIPC_SUPPORT
		pcie_addr = ((unsigned long)buff - rx_mm->offset) | SPRDWL_MH_ADDRESS_BIT;
		pcie_addr = pcie_addr & SPRDWL_MH_SIPC_ADDRESS_BIT;
		wl_debug("%s: sipc_addr is 0x%lx. \n", __func__, pcie_addr);
#endif
	}

	if (likely(pcie_addr)) {
		ret = mm_w_addr_buf(mm_entry, pcie_addr);
		if (ret) {
			wl_err("%s: write addr buf fail: %d\n",
				__func__, ret);
			if (SPRDWL_HW_SIPC == rx_if->intf->priv->hw_type) {
#ifdef SIPC_SUPPORT
				/*free node*/
				node->priv = NULL;
				sipc_free_node_buf(node, &rx_mm->nlist);
#endif
			}
			dev_kfree_skb(skb);
		} else {
				skb_queue_tail(&mm_entry->buffer_list, skb);
				/*queue node to busylist*/
				if (SPRDWL_HW_SIPC == rx_if->intf->priv->hw_type) {
#ifdef SIPC_SUPPORT
					sipc_queue_node_buf(node, &rx_mm->nlist);
#endif
				}
		}
	}

	return ret;
}

int mm_buffer_alloc(struct sprdwl_mm *mm_entry, int need_num)
{
	int num = 0, ret = 0;

	for (num = 0; num < need_num; num++) {
		ret = mm_single_buffer_alloc(mm_entry);
		if (ret) {
			wl_err("%s: alloc num: %d, need num: %d, ret: %d\n",
			       __func__, num, need_num, ret);
			break;
		}
	}

	num = need_num - num;

	return num;
}

static struct sk_buff *mm_single_buffer_unlink(struct sprdwl_mm *mm_entry,
					       unsigned long pcie_addr)
{
	struct sprdwl_rx_if *rx_if =
			container_of(mm_entry, struct sprdwl_rx_if, mm_entry);
	struct sk_buff *skb = NULL;
	void *buffer = NULL;
#ifdef SIPC_SUPPORT
	struct sipc_buf_node *node = NULL;
	unsigned long phy_addr = 0;
	struct sipc_buf_mm *rx_buf = NULL;
#endif

	if (rx_if->intf->priv->hw_type == SPRDWL_HW_SC2355_PCIE)
		buffer = mm_phys_to_virt(&rx_if->intf->pdev->dev, pcie_addr,
				 SPRDWL_MAX_DATA_RXLEN, DMA_FROM_DEVICE, true);
	else if (rx_if->intf->priv->hw_type == SPRDWL_HW_SIPC) {
#ifdef SIPC_SUPPORT
		rx_buf = rx_if->intf->sipc_mm->rx_buf;
		phy_addr = pcie_addr & (~(SPRDWL_MH_ADDRESS_BIT) & SPRDWL_PHYS_MASK);
		phy_addr |= SPRDWL_MH_SIPC_ADDRESS_BASE;
		buffer = (void *)(phy_addr + rx_buf->offset);
#endif
	}

	if (SPRDWL_HW_SIPC == rx_if->intf->priv->hw_type) {
#ifdef SIPC_SUPPORT
		memcpy_fromio(&node, buffer + SPRDWL_MAX_DATA_RXLEN, sizeof(node));
		if (node && node->priv) {
			skb = node->priv;
			skb_unlink(skb, &mm_entry->buffer_list);
			CLEAR_ADDR(skb->data, sizeof(skb));
		} else {
			/*assert is better*/
			wl_err("%s node or addr is null, phy 0x%lx,\
				sipc address 0x%lx\n", __func__, phy_addr, pcie_addr);
			return NULL;
		}
#endif
	} else {
		RESTORE_ADDR(skb, buffer, sizeof(struct sk_buff *));
		skb_unlink(skb, &mm_entry->buffer_list);
		CLEAR_ADDR(skb->data, sizeof(skb));
	}

	return skb;
}

#if 0
static int mm_buffer_relink(struct sprdwl_mm *mm_entry,
			    struct sprdwl_addr_trans_value *value,
			    int total_len)
{
	int num = 0;
	unsigned long pcie_addr = 0;
	struct sk_buff *skb = NULL;
	int len = 0, ret = 0;

	for (num = 0; num < value->num; num++) {
		len += SPRDWL_PHYS_LEN;
		if (unlikely(len > total_len)) {
			wl_err("%s: total_len:%d < len:%d\n",
			       __func__, total_len, len);
			wl_err("%s: total %d pkts, relink %d pkts\n",
			       __func__, value->num, num);
			len = -EINVAL;
			break;
		}

		memcpy(&pcie_addr, value->address[num], SPRDWL_PHYS_LEN);
		pcie_addr &= SPRDWL_PHYS_MASK;
		wl_err("%s: pcie_addr=0x%lx", __func__, pcie_addr);

		ret = mm_w_addr_buf(mm_entry, pcie_addr);
		if (ret) {
			wl_err("%s: write addr buf fail: %d\n", __func__, ret);
			skb = mm_single_buffer_unlink(mm_entry, pcie_addr);
			if (likely(skb))
				dev_kfree_skb(skb);
			else
				wl_err("%s: unlink skb fail!\n", __func__);
		}

		skb = NULL;
		pcie_addr = 0;
	}

	return len;
}
#endif

static int mm_buffer_unlink(struct sprdwl_mm *mm_entry,
			    struct sprdwl_addr_trans_value *value,
			    int total_len)
{
	int num = 0;
	unsigned long long pcie_addr;
	struct sk_buff *skb = NULL;
	struct rx_msdu_desc *msdu_desc = NULL;
	int len = 0;
	unsigned short csum = 0;
	struct sprdwl_rx_if *rx_if =
			container_of(mm_entry, struct sprdwl_rx_if, mm_entry);

	if (atomic_add_return(value->num, &mm_entry->alloc_num) >=
		SPRDWL_MAX_ADD_MH_BUF_ONCE) {
		sprdwl_queue_rx_buff_work(rx_if->intf->priv,
					SPRDWL_PCIE_RX_ALLOC_BUF);
	}

	for (num = 0; num < value->num; num++) {
		len += SPRDWL_PHYS_LEN;
		if (unlikely(len > total_len)) {
			wl_err("%s: total_len:%d < len:%d\n",
			       __func__, total_len, len);
			wl_err("%s: total %d pkts, unlink %d pkts\n",
			       __func__, value->num, num);
			len = -EINVAL;
			break;
		}

		memcpy(&pcie_addr, value->address[num], SPRDWL_PHYS_LEN);
		pcie_addr &= SPRDWL_PHYS_MASK;
		wl_debug("%s: pcie_addr=0x%llx", __func__, pcie_addr);

		skb = mm_single_buffer_unlink(mm_entry, pcie_addr);
		if (SPRDWL_HW_SIPC == rx_if->intf->priv->hw_type) {
#ifdef SIPC_SUPPORT
			if (!skb) {
				wl_err("%s: Rx address buffer is valid.\n", __func__);
				continue;
			}
			sipc_rx_mm_buf_to_skb(rx_if->intf, skb);
#endif
		}
		if (likely(skb)) {
			if (sprdwl_debug_level >= L_DBG)
				sprdwl_hex_dump("rx_mh_desc rx:", skb->data, 500);
			csum = get_pcie_data_csum((void *)rx_if->intf,
						  skb->data);
			skb_reserve(skb, sizeof(struct rx_mh_desc));
			/* TODO: Would CP do this? */
			msdu_desc = (struct rx_msdu_desc *)skb->data;
			msdu_desc->msdu_offset -=
					sizeof(struct rx_mh_desc);
			/* TODO: Check whether prefetch work */
			prefetch(skb->data);

			if (likely(fill_skb_csum(skb, csum) >= 0))
				sprdwl_rx_process(rx_if, skb);
			else /* checksum error, free skb */
				dev_kfree_skb(skb);
		} else {
			wl_err("%s: unlink skb fail!\n", __func__);
		}

		skb = NULL;
		pcie_addr = 0;
	}

	return len;
}

inline bool is_compound_data(struct sprdwl_mm *mm_entry, void *data)
{
	struct rx_msdu_desc *msdu_desc =
		(struct rx_msdu_desc *)(data + mm_entry->hif_offset);

	wl_debug("%s: short_pkt_num: %d\n", __func__, msdu_desc->short_pkt_num);

	return (msdu_desc->short_pkt_num > 1);
}

static void
mm_compound_data_process(struct sprdwl_mm *mm_entry, void *compound_data,
			 int total_len, int buffer_type)
{
#define ALIGN_8BYTE(a) (((a) + 7) & ~7)
	void *pos_data = NULL;
	int num = 0, msdu_len = 0, len = 0;
	struct sk_buff *skb = NULL;
	struct sprdwl_rx_if *rx_if =
			container_of(mm_entry, struct sprdwl_rx_if, mm_entry);

	wl_debug("%s: num: %d, total_len: %d\n", __func__, num, total_len);

	pos_data = compound_data + mm_entry->hif_offset;
	total_len -= mm_entry->hif_offset;
	num = ((struct rx_msdu_desc *)pos_data)->short_pkt_num;

	while (num--) {
		msdu_len = msdu_total_len((struct rx_msdu_desc *)pos_data);
		len += ALIGN_8BYTE(msdu_len);
		if (unlikely(len > total_len)) {
			wl_err("%s: total_len:%d < len:%d, leave %d pkts\n",
			       __func__, total_len, len, (num + 1));
			break;
		}

		wl_debug("%s: msdu_len: %d, len: %d\n",
			 __func__, msdu_len, len);

		skb = mm_data2skb_process(mm_entry, pos_data, msdu_len);
		if (unlikely(!skb)) {
			wl_err("%s: alloc skb fail, leave %d pkts\n",
			       __func__, (num + 1));
			break;
		}

		sprdwl_rx_process(rx_if, skb);

		pos_data = (unsigned char *)pos_data +
			ALIGN_8BYTE(msdu_len + sizeof(struct rx_mh_desc));
		skb = NULL;
	}

	sprdwl_free_data(compound_data, buffer_type);
}

static void mm_normal_data_process(struct sprdwl_mm *mm_entry,
				   void *data, int len, int buffer_type)
{
	int skb_len = 0;
	unsigned short csum = 0;
	bool free_data = false;
	struct sk_buff *skb = NULL;
	struct rx_msdu_desc *msdu_desc =
		(struct rx_msdu_desc *)(data + mm_entry->hif_offset);
	struct sprdwl_rx_if *rx_if =
			container_of(mm_entry, struct sprdwl_rx_if, mm_entry);

	if (unlikely(len < sizeof(struct rx_msdu_desc))) {
		wl_err("%s: data len is %d, too short\n",
		       __func__, len);
		free_data = true;
	} else {
		csum = get_sdio_data_csum((void *)rx_if->intf, data);
		skb_len = SKB_DATA_ALIGN(sizeof(struct skb_shared_info)) +
			  SKB_DATA_ALIGN(msdu_total_len(msdu_desc) +
					 mm_entry->hif_offset);
		if (likely(skb_len <= len)) {
			/* Use len instead of skb_len
			 * because we could reserve more tailroom
			 */

			skb = mm_build_skb(data, skb_len, buffer_type);
		} else {
			/* Should not happen */
			wl_debug("%s: data len is %d, skb need %d\n",
			       __func__, len, skb_len);
			skb = mm_data2skb_process(mm_entry, data, len);
			free_data = true;
		}

		if (unlikely(!skb)) {
			wl_err("%s: alloc skb fail\n", __func__);
			free_data = true;
		} else {
			skb_reserve(skb, mm_entry->hif_offset);

			if (likely(fill_skb_csum(skb, csum) >= 0))
				sprdwl_rx_process(rx_if, skb);
			else /* checksum error, free skb */
				dev_kfree_skb(skb);
		}
	}

	if (free_data)
		sprdwl_free_data(data, buffer_type);
}

/* NOTE: This function JUST work when mm_w_addr_buf() work abnormal */
static inline void mm_refill_buffer(struct sprdwl_mm *mm_entry)
{
	int num = SPRDWL_MAX_MH_BUF - skb_queue_len(&mm_entry->buffer_list);

	wl_debug("%s: need to refill %d buffer\n", __func__, num);

	if (num > 0) {
		mm_buffer_alloc(mm_entry, num);
	} else if (num < 0) {
		/* Should never happen */
		wl_err("%s: %d > mx addr buf!\n", __func__, num);
	}
}

static int mm_single_event_process(struct sprdwl_mm *mm_entry,
				   struct sprdwl_addr_trans_value *value,
				   int len)
{
	int ret = 0;

	switch (value->type) {
	case SPRDWL_PROCESS_BUFFER:
		ret = mm_buffer_unlink(mm_entry, value, len);
		break;
	case SPRDWL_FREE_BUFFER:
		wl_err("%s: null for free buff\n", __func__);
		//ret = mm_buffer_relink(mm_entry, value, len);
		break;
	case SPRDWL_REQUEST_BUFFER:
		/* NOTE: Not need to do anything here */
		break;
	case SPRDWL_FLUSH_BUFFER:
		sprdwl_rx_flush_buffer(mm_entry);
		break;
	default:
		wl_err("%s: err type: %d\n", __func__, value->type);
		ret = -EINVAL;
	}

#if 0
	if (value->type < SPRDWL_FLUSH_BUFFER)
		mm_refill_buffer(mm_entry);
#endif

	return (ret < 0) ? ret : (ret + sizeof(*value));
}

/* PCIE DATA EVENT */
void mm_mh_data_event_process(struct sprdwl_mm *mm_entry, void *data,
			      int len, int buffer_type)
{
	int offset = 0;
	struct sprdwl_addr_hdr *hdr =
		(struct sprdwl_addr_hdr *)(data + mm_entry->hif_offset);
	struct sprdwl_addr_trans *addr_trans =
		(struct sprdwl_addr_trans *)hdr->paydata;
	struct sprdwl_addr_trans_value *value = addr_trans->value;
	unsigned char tlv_num = addr_trans->tlv_num;
	int remain_len = len - mm_entry->hif_offset -
					sizeof(*hdr) -
					sizeof(*addr_trans) -
					sizeof(*value);
	while (tlv_num--) {
		remain_len = remain_len - offset;
		if (remain_len < 0) {
			wl_err("%s: remain tlv num: %d\n", __func__, tlv_num);
			break;
		}

		value = GET_NEXT_ADDR_TRANS_VALUE(value, offset);
		offset = mm_single_event_process(mm_entry, value, remain_len);
		if (offset < 0) {
			wl_err("%s: do mh event fail: %d!\n",
			       __func__, offset);
			break;
		}
	}
}

/* NORMAL DATA */
void mm_mh_data_process(struct sprdwl_mm *mm_entry, void *data,
			int len, int buffer_type)
{
	if (is_compound_data(mm_entry, data))
		mm_compound_data_process(mm_entry, data, len, buffer_type);
	else
		mm_normal_data_process(mm_entry, data, len, buffer_type);
}

int sprdwl_mm_init(struct sprdwl_mm *mm_entry, void *intf)
{
	int ret = 0;
	enum sprdwl_hw_type hw_type =
		((struct sprdwl_intf *)intf)->priv->hw_type;

	mm_entry->hif_offset = ((struct sprdwl_intf *)intf)->hif_offset;
	if (SPRDWL_HW_SC2355_PCIE == hw_type ||
		SPRDWL_HW_SIPC == hw_type) {
		skb_queue_head_init(&mm_entry->buffer_list);
		atomic_set(&mm_entry->alloc_num, 0);
	}

	return ret;
}

int sprdwl_mm_deinit(struct sprdwl_mm *mm_entry, void *intf)
{
	enum sprdwl_hw_type hw_type =
			((struct sprdwl_intf *)intf)->priv->hw_type;

	if (SPRDWL_HW_SC2355_PCIE == hw_type ||
		SPRDWL_HW_SIPC == hw_type) {
		/* NOTE: pclint says kfree(NULL) is safe */
		kfree(mm_entry->hdr);
		mm_entry->hdr = NULL;
		mm_entry->addr_trans = NULL;
		mm_flush_buffer(mm_entry);
	}

	mm_entry->hif_offset = 0;
	return 0;
}
