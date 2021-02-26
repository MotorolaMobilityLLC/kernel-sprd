/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include "apdu_r3p0.h"

static struct sock *sprd_apdu_nlsk;

static void sprd_apdu_int_en(void __iomem *base, u32 int_en)
{
	u32 all_int;

	all_int = readl_relaxed(base + APDU_INT_EN);
	all_int |= (int_en & APDU_INT_BITS);
	writel_relaxed(all_int, base + APDU_INT_EN);
}

static void sprd_apdu_int_dis(void __iomem *base, u32 int_dis)
{
	u32 all_int;

	all_int = readl_relaxed(base + APDU_INT_EN);
	all_int &= ~(int_dis & APDU_INT_BITS);
	writel_relaxed(all_int, base + APDU_INT_EN);
}

static void sprd_apdu_clear_int(void __iomem *base, u32 clear_int)
{
	writel_relaxed(clear_int & APDU_INT_BITS, base + APDU_INT_CLR);
}

static void sprd_apdu_inf_int_en(void __iomem *base, u32 int_en)
{
	u32 all_int;

	all_int = readl_relaxed(base + APDU_INF_INT_EN);
	all_int |= int_en;
	writel_relaxed(all_int, base + APDU_INF_INT_EN);
}

static void sprd_apdu_inf_int_dis(void __iomem *base, u32 int_dis)
{
	u32 all_int;

	all_int = readl_relaxed(base + APDU_INF_INT_EN);
	all_int &= ~int_dis;
	writel_relaxed(all_int, base + APDU_INF_INT_EN);
}

static void sprd_apdu_clear_inf_int(void __iomem *base, u32 clear_int)
{
	writel_relaxed(clear_int, base + APDU_INF_INT_CLR);
}

static void sprd_apdu_clear_cnt(void __iomem *base)
{
	writel_relaxed((BIT(0) | BIT(1)), base + APDU_CNT_CLR);
}

static void sprd_apdu_rst(void __iomem *base)
{
	/* missing reset apdu */
	sprd_apdu_clear_int(base, APDU_INT_BITS);
	sprd_apdu_clear_cnt(base);
	sprd_apdu_clear_inf_int(base, APDU_INF_INT_BITS);
}

static void sprd_apdu_clear_fifo(void __iomem *base)
{
	/* send clear fifo req to ISE, and then ISE will clear fifo*/
	writel_relaxed(BIT(4), base + APDU_CNT_CLR);
}

static void sprd_apdu_read_rx_fifo(void __iomem *base,
				   u32 *data_ptr, u32 len)
{
	u32 i;

	for (i = 0; i < len; i++) {
		data_ptr[i] = readl_relaxed(base + APDU_RX_FIFO);
		pr_debug("r_data[%d]:0x%08x ", i, data_ptr[i]);
	}
}

static void sprd_apdu_write_tx_fifo(void __iomem *base,
				    u32 *data_ptr, u32 len)
{
	u32 i;

	for (i = 0; i < len; i++) {
		writel_relaxed(data_ptr[i], base + APDU_TX_FIFO);
		pr_debug("w_data[%d]:0x%08x ", i, data_ptr[i]);
	}
}

static u32 sprd_apdu_get_rx_fifo_len(void __iomem *base)
{
	u32 fifo_status;

	fifo_status = readl_relaxed(base + APDU_STATUS0);
	fifo_status =
		(fifo_status >> APDU_FIFO_RX_OFFSET) & APDU_FIFO_LEN_MASK;

	return fifo_status;
}

static u32 sprd_apdu_get_rx_fifo_point(void __iomem *base)
{
	u32 fifo_status;

	fifo_status = readl_relaxed(base + APDU_STATUS0);
	fifo_status =
		(fifo_status >> APDU_FIFO_RX_POINT_OFFSET) & APDU_FIFO_LEN_MASK;

	return fifo_status;
}

static u32 sprd_apdu_get_rx_fifo_cnt(void __iomem *base)
{
	u32 fifo_status;

	fifo_status = readl_relaxed(base + APDU_STATUS1);
	fifo_status =
		(fifo_status >> APDU_FIFO_RX_OFFSET) & APDU_CNT_LEN_MASK;

	return fifo_status;
}

static u32 sprd_apdu_get_tx_fifo_len(void __iomem *base)
{
	u32 fifo_status;

	fifo_status = readl_relaxed(base + APDU_STATUS0);
	fifo_status &= APDU_FIFO_LEN_MASK;

	return fifo_status;
}

static u32 sprd_apdu_get_tx_fifo_point(void __iomem *base)
{
	u32 fifo_status;

	fifo_status = readl_relaxed(base + APDU_STATUS0);
	fifo_status =
		(fifo_status >> APDU_FIFO_TX_POINT_OFFSET) & APDU_FIFO_LEN_MASK;

	return fifo_status;
}

static u32 sprd_apdu_get_tx_fifo_cnt(void __iomem *base)
{
	u32 fifo_status;

	fifo_status = readl_relaxed(base + APDU_STATUS1);
	fifo_status = fifo_status & APDU_CNT_LEN_MASK;

	return fifo_status;
}

static long sprd_apdu_check_clr_fifo_done(struct sprd_apdu_device *apdu)
{
	u32 ret;

	ret = sprd_apdu_get_tx_fifo_len(apdu->base);
	ret |= sprd_apdu_get_tx_fifo_point(apdu->base);
	ret |= sprd_apdu_get_tx_fifo_cnt(apdu->base);
	ret |= sprd_apdu_get_rx_fifo_len(apdu->base);
	ret |= sprd_apdu_get_rx_fifo_point(apdu->base);
	ret |= sprd_apdu_get_rx_fifo_cnt(apdu->base);
	if (ret != 0)
		/* fifo not cleard */
		return 1;

	return 0;
}

static long sprd_apdu_set_med_high_addr(struct sprd_apdu_device *apdu,
					u64 med_base_addr)
{
	u64 high_addr;
	u32 base_addr;
	u32 bit_offset = apdu->pub_ise_bit_offset;
	long ret;

	/* med base addr need aligned to 64MB */
	if (((med_base_addr & GENMASK_ULL(25, 0)) != 0) ||
	    (med_base_addr < DDR_BASE)) {
		dev_err(apdu->dev, "error med base addr\n");
		return -EFAULT;
	}
	high_addr = (med_base_addr - DDR_BASE) & GENMASK_ULL(34, 26);
	ret = regmap_read(apdu->pub_reg_base,
			  apdu->pub_ise_reg_offset, &base_addr);
	if (ret < 0)
		return ret;

	base_addr &= ~(GENMASK(bit_offset + 8, bit_offset));
	base_addr |= (u32)((high_addr >> (26 - bit_offset)) &
			   GENMASK_ULL(bit_offset + 8, bit_offset));

	ret = regmap_write(apdu->pub_reg_base,
			   apdu->pub_ise_reg_offset, base_addr);
	if (ret < 0)
		return ret;

	return 0;
}

static void sprd_apdu_enable(struct sprd_apdu_device *apdu)
{
	/* pmu reg:REG_PMU_APB_PD_ISE_CFG_0
	 * should be pre-configured to power up ISE.
	 */

	sprd_apdu_clear_int(apdu->base, APDU_INT_BITS);
	sprd_apdu_int_dis(apdu->base, APDU_INT_BITS);
	sprd_apdu_inf_int_dis(apdu->base, APDU_INF_INT_BITS);
	sprd_apdu_int_en(apdu->base, APDU_INT_RX_EMPTY_TO_NOEMPTY);
	sprd_apdu_int_en(apdu->base, APDU_INT_MED_WR_DONE);
	sprd_apdu_int_en(apdu->base, APDU_INT_MED_WR_ERR);

	sprd_apdu_inf_int_en(apdu->base, APDU_INF_INT_GET_ATR);
	sprd_apdu_inf_int_en(apdu->base, APDU_INF_INT_FAULT);
}

static long sprd_apdu_power_on_check(struct sprd_apdu_device *apdu, u32 times)
{
	u32 status, timeout = times;

	/* It takes about 500us to power on ISE and apdu module */
	do {
		writel_relaxed(0x40, apdu->base + APDU_WATER_MARK);
		status = readl_relaxed(apdu->base + APDU_WATER_MARK);
		/* write reg succeed, apdu module enable complete */
		if (status == 0x40)
			return 0;
		/* AP has up to 30ms window to shake hands with ISE */
		usleep_range(10, 20);
	} while (--timeout);

	return -ENXIO;
}

static int sprd_apdu_read_data(struct sprd_apdu_device *apdu,
			       u32 *buf, u32 count)
{
	u32 rx_fifo_status;
	u32 once_read_len, left;
	u32 index = 0, loop = 0;
	u32 need_read = count;

	/* read need_read words */
	while (need_read > 0) {
		do {
			rx_fifo_status = sprd_apdu_get_rx_fifo_len(apdu->base);
			dev_dbg(apdu->dev,
				"rx_fifo_status:0x%x\n", rx_fifo_status);
			if (rx_fifo_status)
				break;

			if (++loop >= 200) {
				dev_err(apdu->dev, "apdu read data timeout!\n");
				return -EBUSY;
			}
			/* read timeout is less than 2~4s */
			usleep_range(10000, 20000);
		} while (1);

		once_read_len = (need_read < rx_fifo_status)
					? need_read : rx_fifo_status;
		sprd_apdu_read_rx_fifo(apdu->base, &buf[index], once_read_len);

		index += once_read_len;
		need_read -= once_read_len;
		if (need_read == 0) {
			left = sprd_apdu_get_rx_fifo_len(apdu->base);
			/* may another remaining packet in RX FIFO */
			if (left)
				dev_err(apdu->dev, "read left len:%d\n", left);
			break;
		}
	}
	return 0;
}

static int sprd_apdu_write_data(struct sprd_apdu_device *apdu,
				void *buf, u32 count)
{
	u32 *data_buffer = (u32 *)buf;
	u32 len, pad_len;
	u32 tx_fifo_free_space;
	u32 data_to_write;
	u32 index = 0, loop = 0, check = 0;
	char header[4] = {0x00, 0x00, 0xaa, 0x55};

	if (count > APDU_TX_MAX_SIZE) {
		dev_err(apdu->dev, "write len:%d exceed max:%d!\n",
			count, APDU_TX_MAX_SIZE);
		return -EFBIG;
	}

	pad_len = count % 4;
	if (pad_len) {
		memset(buf + count, 0, 4 - pad_len);
		len = count / 4 + 1;
	} else {
		len = count / 4;
	}

	/* clear fifo for further read operation */
	while (sprd_apdu_get_rx_fifo_len(apdu->base) != 0) {
		usleep_range(100, 200);
		check++;
		if (check == 300) {
			sprd_apdu_clear_fifo(apdu->base);
			dev_err(apdu->dev, "such rx fifo data discard\n");
		} else if (check > 1000) {
			dev_err(apdu->dev, "clear fifo timeout/fail\n");
			break;
		}
	}
	/* apdu workflow is send request first and rcv answer later */
	apdu->rx_done = 0;

	header[0] = count & 0xff;
	header[1] = (count >> 8) & 0xff;
	/* send protocol header */
	sprd_apdu_write_tx_fifo(apdu->base, (u32 *)header, 1);

	/* write len words */
	while (len > 0) {
		do {
			tx_fifo_free_space = APDU_FIFO_LENGTH -
				sprd_apdu_get_tx_fifo_len(apdu->base);
			if (tx_fifo_free_space)
				break;

			if (++loop >= 200) {
				sprd_apdu_clear_fifo(apdu->base);
				dev_err(apdu->dev,
					"wr data timeout!please resend\n");
				return -EBUSY;
			}
			/* write timeout is less than 2~4s */
			usleep_range(10000, 20000);
		} while (1);

		data_to_write = (len < tx_fifo_free_space)
					? len : tx_fifo_free_space;
		sprd_apdu_write_tx_fifo(apdu->base,
					&data_buffer[index], data_to_write);
		index += data_to_write;
		len -= data_to_write;
	}

	return 0;
}

static ssize_t sprd_apdu_read(struct file *fp, char __user *buf,
			      size_t count, loff_t *f_pos)
{
	struct sprd_apdu_device *apdu = fp->private_data;
	ssize_t r = count;
	u32 xfer, data_len, word_len, wait_time, header = 0;
	unsigned long wait_event_time;
	int ret;

	mutex_lock(&apdu->mutex);
	r = sprd_apdu_power_on_check(apdu, 1);
	if (r < 0) {
		dev_err(apdu->dev,
			"ISE has no power on or apdu has not release\n");
		goto end;
	}

	wait_event_time = msecs_to_jiffies(MAX_WAIT_TIME);
	wait_time = AP_WAIT_TIMES;
	do {
		if (!apdu->rx_done) {
			ret = wait_event_interruptible_timeout(apdu->read_wq, apdu->rx_done,
							       wait_event_time);
			if (ret < 0) {
				r = ret;
				goto end;
			} else if (ret == 0 && apdu->rx_done == 0) {
				dev_err(apdu->dev,
					"ap read timeout, busy(%d)\n",
					(AP_WAIT_TIMES - wait_time + 1));
				r = -ETIMEDOUT;
				goto end;
			}
		}

		/* check apdu packet valid */
		sprd_apdu_read_rx_fifo(apdu->base, &header, 1);
		if (((header >> 16) & 0xffff) != APDU_MAGIC_NUM) {
			sprd_apdu_clear_fifo(apdu->base);
			dev_err(apdu->dev,
				"read data magic error! clr fifo.\n");
			r = -ENODATA;
			goto end;
		}

		data_len = header & 0xffff;
		if (data_len > APDU_RX_MAX_SIZE) {
			sprd_apdu_clear_fifo(apdu->base);
			dev_err(apdu->dev, "rd len:%d, max:%d! clr fifo.\n",
				data_len, APDU_RX_MAX_SIZE);
			r = -EFBIG;
			goto end;
		}

		word_len = DIV_CEILING(data_len, 4);
		ret = sprd_apdu_read_data(apdu, (u32 *)apdu->rx_buf, word_len);
		if (ret < 0) {
			sprd_apdu_clear_fifo(apdu->base);
			dev_err(apdu->dev,
				"apdu read fifo fail(%d)\n", ret);
			r = ret;
			goto end;
		}

		if (((*(char *)apdu->rx_buf) == ISE_BUSY_STATUS) &&
		    (data_len == 0x1)) {
			apdu->rx_done = 0;
			dev_dbg(apdu->dev, "ise read busy(%d)",
				(AP_WAIT_TIMES - wait_time + 1));
		} else {
			break;
		}
	} while (--wait_time);

	if (((*(char *)apdu->rx_buf) == ISE_BUSY_STATUS) && (data_len == 0x1))
		dev_err(apdu->dev, "ISE busy, exceed AP max wait times\n");

	xfer = (data_len < count) ? data_len : count;
	r = xfer;
	if (copy_to_user(buf, apdu->rx_buf, xfer))
		r = -EFAULT;

end:
	mutex_unlock(&apdu->mutex);
	return r;
}

static ssize_t sprd_apdu_write(struct file *fp, const char __user *buf,
			       size_t count, loff_t *f_pos)
{
	struct sprd_apdu_device *apdu = fp->private_data;
	ssize_t r = 0;
	u32 xfer;
	int ret;

	mutex_lock(&apdu->mutex);
	r = sprd_apdu_power_on_check(apdu, 1);
	if (r < 0) {
		dev_err(apdu->dev,
			"ISE has no power on or apdu has not release\n");
		goto end;
	}
	r = count;

	while (count > 0) {
		if (count > APDU_TX_MAX_SIZE) {
			dev_err(apdu->dev, "write len:%d exceed max:%d!\n",
				count, APDU_TX_MAX_SIZE);
			r = -EFBIG;
			goto end;
		} else {
			xfer = count;
		}
		if (xfer && copy_from_user(apdu->tx_buf, buf, xfer)) {
			dev_err(apdu->dev, "get data fail!\n");
			r = -EFAULT;
			goto end;
		}

		ret = sprd_apdu_write_data(apdu, apdu->tx_buf, xfer);
		if (ret) {
			r = ret;
			goto end;
		}
		buf += xfer;
		count -= xfer;
	}

end:
	mutex_unlock(&apdu->mutex);
	return r;
}

static void sprd_apdu_get_atr(struct sprd_apdu_device *apdu)
{
	u32 word_len, atr_len, i;
	int ret;

	atr_len = APDU_ATR_DATA_MAX_SIZE + 4;
	memset((void *)apdu->atr, 0x0, atr_len);
	if (sprd_apdu_get_rx_fifo_len(apdu->base) == 0x0) {
		dev_err(apdu->dev, "get atr fail: rx fifo not data.\n");
		return;
	}

	sprd_apdu_read_rx_fifo(apdu->base, apdu->atr, 1);
	word_len = DIV_CEILING(((*apdu->atr) & 0xffff), 4);

	if (((((*apdu->atr) >> 16) & 0xffff) == APDU_MAGIC_NUM) &&
	    (word_len <= (APDU_ATR_DATA_MAX_SIZE / 4))) {
		ret = sprd_apdu_read_data(apdu, &(apdu->atr[1]), word_len);
		if (ret < 0) {
			sprd_apdu_clear_fifo(apdu->base);
			dev_err(apdu->dev, "get atr error(%d)\n", ret);
			return;
		}
		dev_dbg(apdu->dev, "get atr done, len:%d word\n", word_len);
		for (i = 0; i < (word_len + 1); i++)
			dev_dbg(apdu->dev, "0x%8x ", apdu->atr[i]);
	} else {
		sprd_apdu_clear_fifo(apdu->base);
		if ((((*apdu->atr) >> 16) & 0xffff) != APDU_MAGIC_NUM)
			/* may data left in rx fifo before receive atr */
			dev_err(apdu->dev, "not a effective header\n");
		else if (word_len > (APDU_ATR_DATA_MAX_SIZE / 4))
			dev_err(apdu->dev, "atr over size\n");
		else
			dev_err(apdu->dev, "get atr fail!\n");
	}
}

static long sprd_apdu_send_enter_apdu_loop_req(struct sprd_apdu_device *apdu)
{
	long ret;
	char cmd_enter_apdu_loop[4] = {0x00, 0xf5, 0x5a, 0xa5};

	if (!apdu)
		return -EINVAL;

	ret = sprd_apdu_write_data(apdu, (void *)cmd_enter_apdu_loop, 4);
	if (ret < 0)
		dev_err(apdu->dev,
			"enter apdu loop ins:write error(%d)\n", ret);
	return ret;
}

static int sprd_apdu_send_usrmsg(char *pbuf, uint16_t len)
{
	struct sk_buff *nl_skb;
	struct nlmsghdr *nlh;
	int ret;

	if (!sprd_apdu_nlsk) {
		pr_err("sprd-apdu:netlink uninitialized\n");
		return -EFAULT;
	}

	nl_skb = nlmsg_new(len, GFP_ATOMIC);
	if (!nl_skb) {
		pr_err("sprd-apdu:netlink alloc fail\n");
		return -EFAULT;
	}

	nlh = nlmsg_put(nl_skb, 0, 0, APDU_NETLINK, len, 0);
	if (!nlh) {
		pr_err("sprd-apdu:nlmsg_put fail\n");
		nlmsg_free(nl_skb);
		return -EFAULT;
	}

	memcpy(nlmsg_data(nlh), pbuf, len);
	ret = netlink_unicast(sprd_apdu_nlsk, nl_skb,
			      APDU_USER_PORT, MSG_DONTWAIT);

	return ret;
}

static void sprd_apdu_netlink_rcv_msg(struct sk_buff *skb)
{
	struct nlmsghdr *nlh = NULL;
	char *umsg = NULL;
	char *kmsg;
	char netlink_kmsg[30] = {0};

	if (skb->len >= nlmsg_total_size(0)) {
		kmsg = netlink_kmsg;
		nlh = nlmsg_hdr(skb);
		umsg = NLMSG_DATA(nlh);
		if (umsg)
			sprd_apdu_send_usrmsg(kmsg, strlen(kmsg));
	}
}

static struct netlink_kernel_cfg sprd_apdu_netlink_cfg = {
	.input = sprd_apdu_netlink_rcv_msg,
};

static long med_rewrite_info_parse(struct med_info_type *med_info,
				   u32 ise_side_data_offset,
				   u32 ise_side_data_len)
{
	u32 temp, temp2;

	pr_debug("sprd-apdu:input ise side offset:0x%x, len:0x%x\n",
		 ise_side_data_offset, ise_side_data_len);
	if (ise_side_data_offset >= MED_DDR_SIZE) {
		pr_err("sprd-apdu:error input offset:0x%x, limit: 0x%x\n",
		       ise_side_data_offset, MED_DDR_SIZE);
		return -EFAULT;
	}
	if ((ise_side_data_offset + ise_side_data_len) > MED_DDR_SIZE) {
		pr_err("sprd-apdu:error input range, offset:0x%x, len: 0x%x\n",
		       ise_side_data_offset, ise_side_data_len);
		return -EFAULT;
	}

	/* all 512Byte level1 rng need rewrite to flash*/
	med_info->level1_rng_offset = 0x400;
	med_info->level1_rng_length = 0x200;

	/* one block data (512KB) range change, rewrite 1024Byte lv2 rng/hash
	 * ap level2 offset default add 0x800
	 */
	temp = ise_side_data_offset / 0x80000;
	med_info->level2_rng_offset = temp * 0x400 + 0x800;
	temp2 = DIV_CEILING(ise_side_data_offset + ise_side_data_len, 0x80000);
	med_info->level2_rng_length = (temp2 - temp) * 0x400;

	/* one block data (4KB) range change, rewrite 1024Byte lv3 rng/hash
	 * ap level3 offset default add 0xE800
	 */
	temp = ise_side_data_offset / 0x1000;
	med_info->level3_rng_offset = temp * 0x400 + 0xe800;
	temp2 = DIV_CEILING(ise_side_data_offset + ise_side_data_len, 0x1000);
	med_info->level3_rng_length = (temp2 - temp) * 0x400;

	/* 32B ise data range change, ap rewrite 32B ciphertext  and 32B hash
	 * ap data offset default add 0x800000
	 */
	temp = (ise_side_data_offset & 0xffffffe0);
	med_info->ap_side_data_offset = temp * 2 + 0x800000;
	temp2 = DIV_CEILING(ise_side_data_offset + ise_side_data_len, 0x20);
	temp2 = (temp2 * 0x20) - temp;
	med_info->ap_side_data_length = temp2 * 2;

	return 0;
}

static void ise_fault_status_caching(struct sprd_apdu_device *apdu,
				     u32 ise_fault_status)
{
	if (ise_fault_status != apdu->ise_fault_buf[apdu->ise_fault_point - 1])
		apdu->ise_fault_buf[apdu->ise_fault_point++] = ise_fault_status;

	if (apdu->ise_fault_point >= ISE_ATTACK_BUFFER_SIZE) {
		apdu->ise_fault_point = (ISE_ATTACK_BUFFER_SIZE - 1);
		dev_err(apdu->dev, "a risk of ise fault state loss\n");
	}
}

static void sprd_apdu_get_med_rewrite_info(struct sprd_apdu_device *apdu)
{
	u32 word_len, med_rewrite_len, i, j;
	u32 msg_buf[APDU_MED_INFO_PARSE_SZ + 1] = {0};
	int ret;

	med_rewrite_len = APDU_MED_INFO_SIZE * 4 + 4;
	memset((void *)apdu->med_rewrite, 0x0, med_rewrite_len);
	if (sprd_apdu_get_rx_fifo_len(apdu->base) == 0x0) {
		dev_err(apdu->dev, "get med info fail:rx fifo not data.\n");
		return;
	}

	sprd_apdu_read_rx_fifo(apdu->base, apdu->med_rewrite, 1);
	word_len = DIV_CEILING(((*apdu->med_rewrite) & 0xffff), 4);

	if (((((*apdu->med_rewrite) >> 16) & 0xffff) == APDU_MAGIC_NUM) &&
	    (word_len <= APDU_MED_INFO_SIZE)) {
		ret = sprd_apdu_read_data(apdu, &(apdu->med_rewrite[1]),
					  word_len);
		if (ret < 0) {
			sprd_apdu_clear_fifo(apdu->base);
			dev_err(apdu->dev,
				"get med rewrite info error(%d)\n", ret);
			return;
		}
		dev_dbg(apdu->dev,
			"get med info done, len:%d word\n", word_len);
		for (i = 0; i < (word_len + 1); i++)
			dev_dbg(apdu->dev, "0x%8x ", apdu->med_rewrite[i]);

		msg_buf[0] = MESSAGE_HEADER_MED_INFO;
		for (i = 0; i < MED_INFO_MAX_BLOCK; i++) {
			/* ISE side rewrite offset and len array */
			if ((apdu->med_rewrite[i * 2 + 1] == 0) &&
			    (apdu->med_rewrite[i * 2 + 2] == 0))
				break;
			med_rewrite_info_parse((struct med_info_type *)&msg_buf[i * 8 + 1],
					       apdu->med_rewrite[i * 2 + 1],
					       apdu->med_rewrite[i * 2 + 2]);
		}
		if (i > 0) {
			word_len = (i * sizeof(struct med_info_type) + 4) / 4;
			dev_dbg(apdu->dev,
				"sending ap rewrite info to user space\n");
			for (j = 0; j < word_len; j++)
				dev_dbg(apdu->dev, "0x%x\n", msg_buf[j]);
			sprd_apdu_send_usrmsg((char *)msg_buf,
					      (i * sizeof(struct med_info_type) + 4));
		}
	} else {
		sprd_apdu_clear_fifo(apdu->base);
		if ((((*apdu->med_rewrite) >> 16) & 0xffff) != APDU_MAGIC_NUM)
			/* may data left before receive med rewrite info */
			dev_err(apdu->dev, "not a effective header\n");
		else if (word_len > APDU_MED_INFO_SIZE)
			dev_err(apdu->dev, "excend med info max buf size!\n");
		else
			dev_err(apdu->dev, "get med info fail!\n");
	}
}

static void sprd_apdu_send_fault_status(struct sprd_apdu_device *apdu)
{
	/* send ramining fault status */
	if (apdu->ise_fault_point > 1) {
		apdu->ise_fault_allow_to_send_flag = 0;
		apdu->ise_fault_buf[0] = MESSAGE_HEADER_FAULT;
		sprd_apdu_send_usrmsg((char *)apdu->ise_fault_buf,
				      (4 * apdu->ise_fault_point));
		memset(apdu->ise_fault_buf, 0, ISE_ATTACK_BUFFER_SIZE * 4);
		apdu->ise_fault_point = 1;
	} else {
		apdu->ise_fault_allow_to_send_flag = 1;
	}

	apdu->ise_fault_status = 0;
}

static long sprd_apdu_ioctl(struct file *fp, unsigned int code,
			    unsigned long value)
{
	struct sprd_apdu_device *apdu = fp->private_data;
	struct med_info_type med_info;
	long ret = 0;
	u64 rcv_data;

	mutex_lock(&apdu->mutex);
	switch (code) {
	case APDU_RESET:
		ret = sprd_apdu_power_on_check(apdu, 10);
		if (ret < 0) {
			dev_err(apdu->dev, "power on check fail\n");
			break;
		}
		sprd_apdu_rst(apdu->base);
		break;

	case APDU_CLR_FIFO:
		ret = sprd_apdu_power_on_check(apdu, 10);
		if (ret < 0) {
			dev_err(apdu->dev, "power on check fail\n");
			break;
		}
		sprd_apdu_clear_fifo(apdu->base);
		break;

	case APDU_CHECK_CLR_FIFO_DONE:
		ret = sprd_apdu_power_on_check(apdu, 10);
		if (ret < 0) {
			dev_err(apdu->dev, "power on check fail\n");
			break;
		}

		/* check ISE answer the request of sprd_apdu_clear_fifo
		 * return value: 0--TRUE; 1--FALSE
		 */
		ret = sprd_apdu_check_clr_fifo_done(apdu);
		break;

	case APDU_CHECK_MED_WR_ERROR_STATUS:
		ret = copy_to_user((void __user *)value, &apdu->med_wr_error,
				   sizeof(u32)) ? (-EFAULT) : 0;
		apdu->med_wr_error = 0;
		break;

	case APDU_CHECK_FAULT_STATUS:
		/* if return 0, means not any fault trigger */
		ret = copy_to_user((void __user *)value, apdu->ise_fault_buf,
				   apdu->ise_fault_point * 4) ? (-EFAULT) : 0;
		apdu->ise_fault_point = 1;
		memset(apdu->ise_fault_buf, 0, ISE_ATTACK_BUFFER_SIZE * 4);
		break;

	case APDU_GET_ATR_INF:
		ret = copy_to_user((void __user *)value, &(apdu->atr[1]),
				   ((*apdu->atr) & 0xffff)) ? (-EFAULT) : 0;
		break;

	case APDU_SET_MED_HIGH_ADDR:
		ret = copy_from_user(&rcv_data, (void __user *)value,
				     sizeof(u64)) ? (-EFAULT) : 0;
		if (ret < 0)
			break;
		ret = sprd_apdu_set_med_high_addr(apdu, rcv_data);
		break;

	case APDU_MED_REWRITE_INFO_PARSE:
		ret = copy_from_user(&rcv_data, (void __user *)value,
				     sizeof(u64)) ? (-EFAULT) : 0;
		if (ret < 0)
			break;
		ret = med_rewrite_info_parse(&med_info, (u32)rcv_data,
					     (u32)(rcv_data >> 32));
		if (ret < 0)
			break;
		ret = copy_to_user((void __user *)value, &med_info,
				   sizeof(struct med_info_type)
				   ) ? (-EFAULT) : 0;
		break;

	case APDU_NORMAL_PWR_ON_CFG:
		/* if ISE (apdu module) power on after apdu driver probe,
		 * need enable apdu interrupt again.
		 */
		ret = sprd_apdu_power_on_check(apdu, 300);
		if (ret < 0)
			break;
		memset(apdu->atr, 0, (APDU_ATR_DATA_MAX_SIZE + 4));
		memset(apdu->ise_fault_buf, 0, ISE_ATTACK_BUFFER_SIZE * 4);
		sprd_apdu_enable(apdu);
		break;

	case APDU_ENTER_APDU_LOOP:
		ret = sprd_apdu_power_on_check(apdu, 300);
		if (ret < 0)
			break;
		memset(apdu->atr, 0, (APDU_ATR_DATA_MAX_SIZE + 4));
		memset(apdu->ise_fault_buf, 0, ISE_ATTACK_BUFFER_SIZE * 4);
		/* send requset before interrupt enable */
		ret = sprd_apdu_send_enter_apdu_loop_req(apdu);
		sprd_apdu_enable(apdu);
		break;

	case APDU_FAULT_INT_RESOLVE_DONE:
		sprd_apdu_send_fault_status(apdu);
		break;

	default:
		ret = -EINVAL;
	}

	mutex_unlock(&apdu->mutex);
	return ret;
}

static int sprd_apdu_open(struct inode *inode, struct file *fp)
{
	struct sprd_apdu_device *apdu =
		container_of(fp->private_data, struct sprd_apdu_device, misc);

	fp->private_data = apdu;

	return 0;
}

static int sprd_apdu_release(struct inode *inode, struct file *fp)
{
	fp->private_data = NULL;

	return 0;
}

static const struct file_operations sprd_apdu_fops = {
	.owner = THIS_MODULE,
	.read = sprd_apdu_read,
	.write = sprd_apdu_write,
	.unlocked_ioctl = sprd_apdu_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = sprd_apdu_ioctl,
#endif
	.open = sprd_apdu_open,
	.release = sprd_apdu_release,
};

static irqreturn_t sprd_apdu_interrupt(int irq, void *data)
{
	u32 reg_int_status, reg_inf_int_status;
	struct sprd_apdu_device *apdu = (struct sprd_apdu_device *)data;

	reg_int_status = readl_relaxed(apdu->base + APDU_INT_MASK);
	writel_relaxed(reg_int_status, apdu->base + APDU_INT_CLR);

	reg_inf_int_status = readl_relaxed(apdu->base + APDU_INF_INT_MASK);
	writel_relaxed(reg_inf_int_status, apdu->base + APDU_INF_INT_CLR);

	if (reg_inf_int_status & APDU_INF_INT_FAULT) {
		/* ise fault status, need to be saved as soon as possible */
		ise_fault_status_caching(apdu,
					 (reg_inf_int_status & APDU_INF_INT_FAULT));
		apdu->ise_fault_status = 1;
	}

	if (reg_inf_int_status & APDU_INF_INT_GET_ATR) {
		reg_int_status &= (~APDU_INT_RX_EMPTY_TO_NOEMPTY);
		apdu->atr_rcv_status = 1;
	}
	if (reg_int_status & APDU_INT_MED_WR_DONE) {
		reg_int_status &= (~APDU_INT_RX_EMPTY_TO_NOEMPTY);
		apdu->med_wr_done = 1;
	}
	if (reg_int_status & APDU_INT_MED_WR_ERR)
		apdu->med_wr_error = 1;
	if (reg_int_status & APDU_INT_RX_EMPTY_TO_NOEMPTY) {
		apdu->rx_done = 1;
		wake_up(&apdu->read_wq);
	}

	if ((apdu->ise_fault_status && apdu->ise_fault_allow_to_send_flag) ||
	    apdu->atr_rcv_status || apdu->med_wr_done)
		return IRQ_WAKE_THREAD;
	else
		return IRQ_HANDLED;
}

static irqreturn_t sprd_apdu_irq_thread_fn(int irq, void *data)
{
	struct sprd_apdu_device *apdu = (struct sprd_apdu_device *)data;

	if (apdu->atr_rcv_status) {
		sprd_apdu_get_atr(apdu);
		apdu->atr_rcv_status = 0;
	}

	if (apdu->ise_fault_status && apdu->ise_fault_allow_to_send_flag) {
		apdu->ise_fault_allow_to_send_flag = 0;
		apdu->ise_fault_status = 0;
		apdu->ise_fault_buf[0] = MESSAGE_HEADER_FAULT;
		if (apdu->ise_fault_point > 1)
			sprd_apdu_send_usrmsg((char *)apdu->ise_fault_buf,
					      (4 * apdu->ise_fault_point));
		memset(apdu->ise_fault_buf, 0, ISE_ATTACK_BUFFER_SIZE * 4);
		apdu->ise_fault_point = 1;
	}

	if (apdu->med_wr_done) {
		sprd_apdu_get_med_rewrite_info(apdu);
		apdu->med_wr_done = 0;
	}

	return IRQ_HANDLED;
}

static void sprd_apdu_dump_data(u32 *buf, u32 len)
{
	int i;

	for (i = 0; i < len; i++)
		pr_info("0x%8x ", buf[i]);
	pr_info("\n");
}

static ssize_t get_random_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	u32 rep_data[0x10] = {0};
	unsigned long wait_event_time;
	int ret;
	char cmd_get_random[8] = {
		0x00, 0x84, 0x00, 0x00,
		0x04, 0x00, 0x00, 0x00
	};

	if (!apdu)
		return -EINVAL;
	if (sprd_apdu_power_on_check(apdu, 10) < 0) {
		dev_err(apdu->dev, "power on check fail\n");
		return -ENXIO;
	}

	apdu->rx_done = 0;
	ret = sprd_apdu_write_data(apdu, (void *)cmd_get_random, 8);
	if (ret < 0) {
		dev_err(apdu->dev,
			"get_random test:write error(%d)\n", ret);
		return ret;
	}

	wait_event_time = msecs_to_jiffies(MAX_WAIT_TIME);
	ret = wait_event_interruptible_timeout(apdu->read_wq, apdu->rx_done,
					       wait_event_time);
	if (ret < 0) {
		return ret;
	} else if (ret == 0 && apdu->rx_done == 0) {
		sprd_apdu_clear_fifo(apdu->base);
		dev_err(apdu->dev, "wait read random ready timeout\n");
		return -ETIMEDOUT;
	}
	/* get_random le=4 byte, return random data len = le + status(2 byte) */
	ret = sprd_apdu_read_data(apdu, rep_data, 3);
	if (ret < 0) {
		dev_err(apdu->dev, "get_random test:read error(%d)\n", ret);
		return ret;
	}
	sprd_apdu_dump_data(rep_data, 3);

	return 0;
}

static ssize_t get_atr_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	u32 word_len;

	if (!apdu)
		return -EINVAL;

	word_len = DIV_CEILING(((*apdu->atr) & 0xffff), 4);
	sprd_apdu_dump_data(&(apdu->atr[1]), word_len);
	if (apdu->atr[0] == 0x0)
		dev_err(apdu->dev,
			"did not get atr value, please check ISE status\n");

	return 0;
}

static ssize_t med_rewrite_info_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	u32 word_len;

	if (!apdu)
		return -EINVAL;

	word_len = DIV_CEILING(((*apdu->med_rewrite) & 0xffff), 4);
	sprd_apdu_dump_data(&(apdu->med_rewrite[1]), word_len);
	if (apdu->med_rewrite[0] == 0x0)
		dev_err(apdu->dev,
			"did not get med rewrite info, check ISE status\n");

	return 0;
}

static ssize_t get_fault_status_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);

	if (!apdu)
		return -EINVAL;

	sprd_apdu_dump_data(apdu->ise_fault_buf, ISE_ATTACK_BUFFER_SIZE);
	sprd_apdu_dump_data(&apdu->ise_fault_point, 1);
	return 0;
}

static ssize_t med_status_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);

	if (!apdu)
		return -EINVAL;

	sprd_apdu_dump_data(&apdu->med_wr_done, 1);
	sprd_apdu_dump_data(&apdu->med_wr_error, 1);

	return 0;
}

static ssize_t packet_send_rcv_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	int ret, i;
	char cmd[2050] = {
		0x00, 0x84, 0x00, 0x00,
		0x04, 0x00, 0x00, 0x00,
	};

	if (!apdu)
		return -EINVAL;
	if (sprd_apdu_power_on_check(apdu, 10) < 0) {
		dev_err(apdu->dev, "power on check fail\n");
		return -ENXIO;
	}

	for (i = 8; i < sizeof(cmd); i++)
		cmd[i] = (char)i;
	ret = sprd_apdu_write_data(apdu, (void *)cmd, 2050);
	if (ret < 0)
		dev_err(apdu->dev, "send error\n");

	return ((ret < 0) ? ret : count);
}

static ssize_t packet_send_rcv_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	u32 data_len, word_len, header = 0;
	int ret;

	if (!apdu)
		return -EINVAL;
	if (sprd_apdu_power_on_check(apdu, 10) < 0) {
		dev_err(apdu->dev, "power on check fail\n");
		return -ENXIO;
	}

	if (sprd_apdu_get_rx_fifo_len(apdu->base) == 0x0) {
		dev_err(apdu->dev, "rx fifo empty\n");
		return -EFAULT;
	}

	/* check apdu packet valid */
	sprd_apdu_read_rx_fifo(apdu->base, &header, 1);
	if (((header >> 16) & 0xffff) != APDU_MAGIC_NUM) {
		sprd_apdu_clear_fifo(apdu->base);
		dev_err(apdu->dev,
			"read data magic error! req clr fifo.\n");
		return -EINVAL;
	}

	data_len = header & 0xffff;
	if (data_len > APDU_RX_MAX_SIZE) {
		sprd_apdu_clear_fifo(apdu->base);
		dev_err(apdu->dev,
			"read len:%d exceed max:%d! req clr fifo.\n",
			data_len, APDU_RX_MAX_SIZE);
		return -EINVAL;
	}

	word_len = DIV_CEILING(data_len, 4);
	ret = sprd_apdu_read_data(apdu, (u32 *)apdu->rx_buf, word_len);
	if (ret < 0) {
		sprd_apdu_clear_fifo(apdu->base);
		dev_err(apdu->dev, "apdu read fifo fail(%d)\n", ret);
		return ret;
	}

	sprd_apdu_dump_data((u32 *)apdu->rx_buf, word_len);
	return ret;
}

static ssize_t set_med_high_addr_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	u64 med_ddr_base;
	int ret;

	if (!apdu)
		return -EINVAL;
	ret = kstrtou64(buf, 16, &med_ddr_base);
	if (ret) {
		dev_err(apdu->dev, "invalid value.\n");
		return -EINVAL;
	}

	ret = sprd_apdu_set_med_high_addr(apdu, med_ddr_base);

	return ((ret < 0) ? ret : count);
}

static ssize_t med_info_parse_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	struct med_info_type med_info;
	u32 data_offset, data_len;
	u64 temp_value;
	int ret;

	if (!apdu)
		return -EINVAL;
	ret = kstrtou64(buf, 16, &temp_value);
	if (ret) {
		dev_err(apdu->dev, "invalid value.\n");
		return -EINVAL;
	}

	/* Parse data as data_offset|data_len with each 4 byte */
	data_offset = (u32)(temp_value >> 32);
	data_len = (u32)temp_value;
	ret = med_rewrite_info_parse(&med_info, data_offset, data_len);
	sprd_apdu_dump_data((u32 *)&med_info,
			    (sizeof(struct med_info_type) / 4));

	return ((ret < 0) ? ret : count);
}

static ssize_t sprd_apdu_reset_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	long ret, timeout = 100;

	if (!apdu)
		return -EINVAL;
	if (sprd_apdu_power_on_check(apdu, 10) < 0) {
		dev_err(apdu->dev, "power on check fail\n");
		return -ENXIO;
	}

	sprd_apdu_clear_fifo(apdu->base);
	sprd_apdu_rst(apdu->base);

	while (timeout--) {
		usleep_range(100, 200);
		ret = sprd_apdu_check_clr_fifo_done(apdu);
		if (ret == 0)
			return 0;
	}

	dev_err(apdu->dev, "wait for ISE reset apdu timeout!\n");
	return -ETIMEDOUT;
}

static ssize_t sprd_apdu_reenable_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);

	if (!apdu)
		return -EINVAL;
	if (sprd_apdu_power_on_check(apdu, 10) < 0) {
		dev_err(apdu->dev, "power on check fail\n");
		return -ENXIO;
	}

	memset(apdu->atr, 0, (APDU_ATR_DATA_MAX_SIZE + 4));
	memset(apdu->ise_fault_buf, 0, ISE_ATTACK_BUFFER_SIZE * 4);
	sprd_apdu_enable(apdu);

	return 0;
}

static DEVICE_ATTR_RO(get_random);
static DEVICE_ATTR_RO(get_atr);
static DEVICE_ATTR_RO(get_fault_status);
static DEVICE_ATTR_RO(med_status);
static DEVICE_ATTR_RW(packet_send_rcv);
static DEVICE_ATTR_RO(med_rewrite_info);
static DEVICE_ATTR_WO(med_info_parse);
static DEVICE_ATTR_WO(set_med_high_addr);
static DEVICE_ATTR_RO(sprd_apdu_reset);
static DEVICE_ATTR_RO(sprd_apdu_reenable);

static struct attribute *sprd_apdu_attrs[] = {
	&dev_attr_get_random.attr,
	&dev_attr_get_atr.attr,
	&dev_attr_get_fault_status.attr,
	&dev_attr_med_status.attr,
	&dev_attr_packet_send_rcv.attr,
	&dev_attr_med_rewrite_info.attr,
	&dev_attr_set_med_high_addr.attr,
	&dev_attr_med_info_parse.attr,
	&dev_attr_sprd_apdu_reset.attr,
	&dev_attr_sprd_apdu_reenable.attr,
	NULL
};
ATTRIBUTE_GROUPS(sprd_apdu);

static int sprd_apdu_probe(struct platform_device *pdev)
{
	struct sprd_apdu_device *apdu;
	struct resource *res;
	u32 buf_tx_sz = APDU_TX_MAX_SIZE;
	u32 buf_rx_sz = APDU_RX_MAX_SIZE;
	u32 atr_sz = APDU_ATR_DATA_MAX_SIZE + 4;
	u32 med_info_sz = APDU_MED_INFO_SIZE * 4 + 4;
	u32 ise_fault_buf_sz = ISE_ATTACK_BUFFER_SIZE * 4;
	int ret;

	apdu = devm_kzalloc(&pdev->dev, sizeof(*apdu), GFP_KERNEL);
	if (!apdu)
		return -ENOMEM;

	apdu->irq = platform_get_irq(pdev, 0);
	if (apdu->irq < 0) {
		dev_err(&pdev->dev, "failed to get apdu interrupt.\n");
		return apdu->irq;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	apdu->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(apdu->base)) {
		dev_err(&pdev->dev, "no apdu base specified\n");
		return PTR_ERR(apdu->base);
	}

	apdu->pub_reg_base = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
							     "sprd,sys-pub-reg");
	if (IS_ERR(apdu->pub_reg_base)) {
		dev_err(&pdev->dev, "no pub reg base specified\n");
		return PTR_ERR(apdu->pub_reg_base);
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				   "sprd,pub-ise-reg-offset",
				   &apdu->pub_ise_reg_offset);
	if (ret) {
		dev_err(&pdev->dev, "get pub-ise-reg-offset failed\n");
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				   "sprd,pub-ise-bit-offset",
				   &apdu->pub_ise_bit_offset);
	if (ret) {
		dev_err(&pdev->dev, "get pub-ise-bit-offset failed\n");
		return ret;
	}

	apdu->tx_buf = devm_kzalloc(&pdev->dev, buf_tx_sz, GFP_KERNEL);
	if (!apdu->tx_buf)
		return -ENOMEM;
	apdu->rx_buf = devm_kzalloc(&pdev->dev, buf_rx_sz, GFP_KERNEL);
	if (!apdu->rx_buf)
		return -ENOMEM;
	apdu->atr = devm_kzalloc(&pdev->dev, atr_sz, GFP_KERNEL);
	if (!apdu->atr)
		return -ENOMEM;
	apdu->med_rewrite = devm_kzalloc(&pdev->dev, med_info_sz, GFP_KERNEL);
	if (!apdu->med_rewrite)
		return -ENOMEM;
	apdu->ise_fault_buf = devm_kzalloc(&pdev->dev,
					   ise_fault_buf_sz, GFP_KERNEL);
	if (!apdu->ise_fault_buf)
		return -ENOMEM;
	apdu->ise_fault_point = 1;
	apdu->ise_fault_allow_to_send_flag = 1;

	mutex_init(&apdu->mutex);
	init_waitqueue_head(&apdu->read_wq);

	apdu->misc.minor = MISC_DYNAMIC_MINOR;
	apdu->misc.name = APDU_DRIVER_NAME;
	apdu->misc.fops = &sprd_apdu_fops;
	ret = misc_register(&apdu->misc);
	if (ret) {
		dev_err(&pdev->dev, "misc_register FAILED\n");
		return ret;
	}

	apdu->dev = &pdev->dev;
	ret = devm_request_threaded_irq(apdu->dev, apdu->irq,
					sprd_apdu_interrupt,
					sprd_apdu_irq_thread_fn,
					0, APDU_DRIVER_NAME, apdu);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq %d\n", ret);
		misc_deregister(&apdu->misc);
		return ret;
	}

	ret = sysfs_create_groups(&apdu->dev->kobj, sprd_apdu_groups);
	if (ret)
		dev_warn(apdu->dev, "failed to create apdu attributes\n");

	/* Create netlink socket */
	sprd_apdu_nlsk =
		(struct sock *)netlink_kernel_create(&init_net, APDU_NETLINK,
						     &sprd_apdu_netlink_cfg);
	if (!sprd_apdu_nlsk) {
		dev_err(&pdev->dev, "netlink kernel create error!\n");
		return ret;
	}

	sprd_apdu_enable(apdu);
	if (sprd_apdu_power_on_check(apdu, 10) < 0)
		dev_warn(apdu->dev,
			 "power on check fail before driver probe\n");
	platform_set_drvdata(pdev, apdu);

	return 0;
}

static int sprd_apdu_remove(struct platform_device *pdev)
{
	struct sprd_apdu_device *apdu = platform_get_drvdata(pdev);

	misc_deregister(&apdu->misc);
	sysfs_remove_groups(&apdu->dev->kobj, sprd_apdu_groups);

	if (sprd_apdu_nlsk) {
		netlink_kernel_release(sprd_apdu_nlsk);
		sprd_apdu_nlsk = NULL;
	}

	return 0;
}

static const struct of_device_id sprd_apdu_match[] = {
	{.compatible = "sprd,qogirn6pro-apdu"},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_apdu_match);

static struct platform_driver sprd_apdu_driver = {
	.probe = sprd_apdu_probe,
	.remove = sprd_apdu_remove,
	.driver = {
		.name = "sprd-apdu",
		.of_match_table = sprd_apdu_match,
	},
};

module_platform_driver(sprd_apdu_driver);
