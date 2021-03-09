// SPDX-License-Identifier: GPL-2.0-only
/*
 * Spreadtrum mailbox driver
 *
 * Copyright (c) 2020 Spreadtrum Communications Inc.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/seq_file.h>

#define SPRD_MBOX_ID		0x0
#define SPRD_MBOX_MSG_LOW	0x4
#define SPRD_MBOX_MSG_HIGH	0x8
#define SPRD_MBOX_TRIGGER	0xc
#define SPRD_MBOX_FIFO_RST	0x10
#define SPRD_MBOX_FIFO_STS	0x14
#define SPRD_MBOX_IRQ_STS	0x18
#define SPRD_MBOX_IRQ_MSK	0x1c
#define SPRD_MBOX_LOCK		0x20
#define SPRD_MBOX_FIFO_DEPTH	0x24
#define SPRD_MBOX_VERSION	0x28

/* Bit and mask definiation for inbox's SPRD_MBOX_FIFO_STS register */
#define SPRD_INBOX_FIFO_RECV_MASK		GENMASK(31, 24)
#define SPRD_INBOX_FIFO_RECV_SHIFT		24
#define SPRD_INBOX_FIFO_DELIVER_MASK		GENMASK(23, 16)
#define SPRD_INBOX_FIFO_DELIVER_SHIFT		16
#define SPRD_INBOX_FIFO_OVERLOW_MASK		GENMASK(15, 8)
#define SPRD_INBOX_FIFO_BLOCK_MASK		GENMASK(7, 0)

/* Bit and mask definiation for SPRD_MBOX_IRQ_STS register */
#define SPRD_MBOX_IRQ_CLR			BIT(0)

/* Bit and mask definiation for outbox's SPRD_MBOX_FIFO_STS register */
#define SPRD_OUTBOX_FIFO_FULL			BIT(2)
#define SPRD_OUTBOX_FIFO_WR_SHIFT		16
#define SPRD_OUTBOX_FIFO_RD_SHIFT		24
#define SPRD_OUTBOX_FIFO_POS_MASK		GENMASK(7, 0)

/* Bit and mask definiation for inbox's SPRD_MBOX_IRQ_MSK register */
#define SPRD_INBOX_FIFO_BLOCK_IRQ		BIT(0)
#define SPRD_INBOX_FIFO_OVERFLOW_IRQ		BIT(1)
#define SPRD_INBOX_FIFO_DELIVER_IRQ		BIT(2)
#define SPRD_INBOX_FIFO_IRQ_MASK		GENMASK(2, 0)

/* Bit and mask definiation for outbox's SPRD_MBOX_IRQ_MSK register */
#define SPRD_OUTBOX_FIFO_NOT_EMPTY_IRQ		BIT(0)
#define SPRD_OUTBOX_FIFO_IRQ_MASK		GENMASK(4, 0)

/* Bit and mask definiation for SPRD_MBOX_VERSION register */
#define SPRD_MBOX_VERSION_MASK		GENMASK(15, 0)

#define SPRD_MBOX_VER(r, p)		(u32)((r) * 0x100 + (p))

#define SPRD_INBOX_BASE_SPAN 0x1000
#define SPRD_OUTBOX_BASE_SPAN 0x1000

#define SPRD_INBOX_RECV_TIMEOUT	1000
#define SPRD_MBOX_RX_FIFO_LEN		64
#define SPRD_MBOX_TX_FIFO_LEN		64
#define SPRD_MBOX_CHAN_MAX		8

struct sprd_mbox_priv {
	struct mbox_controller	mbox;
	struct device		*dev;
	void __iomem		*inbox_base;
	void __iomem		*outbox_base;
	struct clk		*clk;
	u32			outbox_fifo_depth;
	u32			version;

	int			inbox_irq;
	int			outbox_irq;
	u8			started;

	/* out-of-band data */
	void __iomem		*oob_outbox_base;
	int			oob_irq;
	u8			oob_started;
	unsigned long		oob_id;

	struct mbox_chan	chan[SPRD_MBOX_CHAN_MAX];

#if defined(CONFIG_DEBUG_FS)
	struct dentry		*root_debugfs_dir;
#endif
};

struct  sprd_mbox_data {
	unsigned long core_id;
	u64 msg;
};

struct sprd_mbox_tx_data {
	u64 fifo[SPRD_MBOX_TX_FIFO_LEN];
	u32 wt_cnt;
	u32 rd_cnt;
};

static struct  sprd_mbox_data mbox_rx_fifo[SPRD_MBOX_RX_FIFO_LEN];
static u32 mbox_rx_fifo_cnt;

static struct sprd_mbox_tx_data tx_data[SPRD_MBOX_CHAN_MAX];
static u8 mbox_tx_mask;

static struct task_struct *mbox_deliver_thread;
static wait_queue_head_t deliver_thread_wait;

static spinlock_t mbox_lock;

#if defined(CONFIG_DEBUG_FS)
static void sprd_mbox_debug_putline(struct seq_file *m, char c, int n)
{
	int i;

	for (i = 0; i < n; i++)
		seq_putc(m, c);

	seq_putc(m, '\n');
}

static
int sprd_outbox_reg_read(struct sprd_mbox_priv *priv, int index, int offset)
{
	return readl(priv->outbox_base +
				SPRD_OUTBOX_BASE_SPAN * index + offset);
}

static
int sprd_inbox_reg_read(struct sprd_mbox_priv *priv, int index, int offset)
{
	return readl(priv->inbox_base + SPRD_INBOX_BASE_SPAN * index + offset);
}

static int sprd_mbox_debug_show(struct seq_file *m, void *private)
{
	struct sprd_mbox_priv *priv = m->private;
	int i;

	sprd_mbox_debug_putline(m, '-', 50);
	seq_printf(m, "Mbox inbox  irq :  %d\n", priv->inbox_irq);
	seq_printf(m, "Mbox outbox irq :  %d\n", priv->outbox_irq);
	seq_printf(m, "Mbox oob irq    :  %d\n", priv->oob_irq);
	seq_printf(m, "Mbox oob id     :  %ld\n", priv->oob_id);
	seq_printf(m, "Mbox version    :  0x%x\n", priv->version);

	/* Mailbox reg */
	seq_puts(m, "\nMailbox reg:\n");

	for (i = 0; i < SPRD_MBOX_CHAN_MAX; i++) {
		sprd_mbox_debug_putline(m, '-', 50);
		seq_printf(m, "Reg  name       | Outbox%d\t| Inbox%d  |\n"
				"Mbox id         : 0x%08x\t 0x%08x\n"
				"Mbox msg low    : 0x%08x\t 0x%08x\n"
				"Mbox msg high   : 0x%08x\t 0x%08x\n"
				"Mbox fifo reset : 0x%08x\t 0x%08x\n"
				"Mbox fifo status: 0x%08x\t 0x%08x\n"
				"Mbox irq  status: 0x%08x\t 0x%08x\n"
				"Mbox irq  mask  : 0x%08x\t 0x%08x\n"
				"Mbox depth      : 0x%08x\t 0x%08x\n",
			i, i,
			sprd_outbox_reg_read(priv, i, SPRD_MBOX_ID),
			sprd_inbox_reg_read(priv, i, SPRD_MBOX_ID),
			sprd_outbox_reg_read(priv, i, SPRD_MBOX_MSG_LOW),
			sprd_inbox_reg_read(priv, i, SPRD_MBOX_MSG_LOW),
			sprd_outbox_reg_read(priv, i, SPRD_MBOX_MSG_HIGH),
			sprd_inbox_reg_read(priv, i, SPRD_MBOX_MSG_HIGH),
			sprd_outbox_reg_read(priv, i, SPRD_MBOX_FIFO_RST),
			sprd_inbox_reg_read(priv, i, SPRD_MBOX_FIFO_RST),
			sprd_outbox_reg_read(priv, i, SPRD_MBOX_FIFO_STS),
			sprd_inbox_reg_read(priv, i, SPRD_MBOX_FIFO_STS),
			sprd_outbox_reg_read(priv, i, SPRD_MBOX_IRQ_STS),
			sprd_inbox_reg_read(priv, i, SPRD_MBOX_IRQ_STS),
			sprd_outbox_reg_read(priv, i, SPRD_MBOX_IRQ_MSK),
			sprd_inbox_reg_read(priv, i, SPRD_MBOX_IRQ_MSK),
			sprd_outbox_reg_read(priv, i, SPRD_MBOX_FIFO_DEPTH),
			sprd_inbox_reg_read(priv, i, SPRD_MBOX_FIFO_DEPTH));
	}

	sprd_mbox_debug_putline(m, '-', 50);

	return 0;
}

static int sprd_mbox_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_mbox_debug_show, inode->i_private);
}

static const struct file_operations sprd_mbox_debugfs_ops = {
	.open = sprd_mbox_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int sprd_mbox_debugfs(struct sprd_mbox_priv *priv)
{
	if (!debugfs_initialized())
		return 0;

	priv->root_debugfs_dir = debugfs_create_dir(dev_name(priv->dev), NULL);
	if (!priv->root_debugfs_dir) {
		dev_err(priv->dev, "Failed to create Mailbox debugfs\n");
		return -EINVAL;
	}

	debugfs_create_file("reg", 0444, priv->root_debugfs_dir,
						priv, &sprd_mbox_debugfs_ops);
	return 0;
}
#endif /* CONFIG_DEBUG_FS */

static struct sprd_mbox_priv *to_sprd_mbox_priv(struct mbox_controller *mbox)
{
	return container_of(mbox, struct sprd_mbox_priv, mbox);
}

static u32 sprd_mbox_get_fifo_len(struct sprd_mbox_priv *priv, u32 fifo_sts)
{
	u32 wr_pos = (fifo_sts >> SPRD_OUTBOX_FIFO_WR_SHIFT) &
		SPRD_OUTBOX_FIFO_POS_MASK;
	u32 rd_pos = (fifo_sts >> SPRD_OUTBOX_FIFO_RD_SHIFT) &
		SPRD_OUTBOX_FIFO_POS_MASK;
	u32 fifo_len;

	/*
	 * If the read pointer is equal with write pointer, which means the fifo
	 * is full or empty.
	 */
	if (wr_pos == rd_pos) {
		if (fifo_sts & SPRD_OUTBOX_FIFO_FULL)
			fifo_len = priv->outbox_fifo_depth;
		else
			fifo_len = 0;
	} else if (wr_pos > rd_pos) {
		fifo_len = wr_pos - rd_pos;
	} else {
		fifo_len = priv->outbox_fifo_depth - rd_pos + wr_pos;
	}

	return fifo_len;
}

static struct mbox_chan *sprd_mbox_index_xlate(
		struct mbox_controller *mbox, const struct of_phandle_args *sp)
{
	struct sprd_mbox_priv *priv;
	int ind = sp->args[0];
	int is_oob = sp->args[1];

	if (ind >= mbox->num_chans)
		return ERR_PTR(-EINVAL);

	if (is_oob) {
		priv = to_sprd_mbox_priv(mbox);
		priv->oob_outbox_base = priv->outbox_base +
			(SPRD_OUTBOX_BASE_SPAN * ind);
		priv->oob_id = ind;
	}

	return &mbox->chans[ind];
}

static irqreturn_t sprd_mbox_outbox_isr(int irq, void *data)
{
	struct sprd_mbox_priv *priv = data;
	struct mbox_chan *chan;
	u32 fifo_sts, fifo_len, msg[2];
	void __iomem *base;
	int i, id;

	if (irq == priv->oob_irq)
		base = priv->oob_outbox_base;
	else
		/* in-bounds isr */
		base = priv->outbox_base;

	fifo_sts = readl(base + SPRD_MBOX_FIFO_STS);

	fifo_len = sprd_mbox_get_fifo_len(priv, fifo_sts);
	if (!fifo_len)
		dev_warn_ratelimited(priv->dev, "spurious outbox interrupt\n");

	for (i = 0; i < fifo_len; i++) {
		msg[0] = readl(base + SPRD_MBOX_MSG_LOW);
		msg[1] = readl(base + SPRD_MBOX_MSG_HIGH);
		id = readl(base + SPRD_MBOX_ID);

		chan = &priv->chan[id];
		if (chan->cl)
			mbox_chan_received_data(chan, (void *)msg);
		else if (mbox_rx_fifo_cnt >= SPRD_MBOX_RX_FIFO_LEN)
			dev_err(priv->dev, "mbox_mbox_rx_fifo full, core_id = %d\n", id);
		else {
			/* Put message in a mbox_rx_fifo if client is not ready */
			mbox_rx_fifo[mbox_rx_fifo_cnt].core_id = id;
			mbox_rx_fifo[mbox_rx_fifo_cnt].msg = *(u64 *)msg;
			mbox_rx_fifo_cnt++;
		}

		/* Trigger to update outbox FIFO pointer */
		writel(0x1, base + SPRD_MBOX_TRIGGER);
	}

	/* Clear irq status after reading all message. */
	writel(SPRD_MBOX_IRQ_CLR, base + SPRD_MBOX_IRQ_STS);

	return IRQ_HANDLED;
}

static u8 get_tx_fifo_mask(u8 deliver_bit)
{
	u8 dst_bit, mask = 0;
	u32 id;

	spin_lock(&mbox_lock);

	for (id = 0; id < SPRD_MBOX_CHAN_MAX; id++) {
		dst_bit = (1 << id);
		if (deliver_bit & dst_bit) {
			if (tx_data[id].wt_cnt != tx_data[id].rd_cnt)
				mask |= (dst_bit);
		}
	}

	spin_unlock(&mbox_lock);

	return mask;
}

static irqreturn_t sprd_mbox_inbox_isr(int irq, void *data)
{
	struct sprd_mbox_priv *priv = data;
	u32 fifo_sts, block, deliver, irq_msk;

	fifo_sts = readl(priv->inbox_base + SPRD_MBOX_FIFO_STS);
	dev_dbg(priv->dev, "fifo_sts=%x\n", fifo_sts);

	/* Clear FIFO delivery and overflow status */
	writel(fifo_sts &
	       (SPRD_INBOX_FIFO_DELIVER_MASK | SPRD_INBOX_FIFO_OVERLOW_MASK),
	       priv->inbox_base + SPRD_MBOX_FIFO_RST);

	block = fifo_sts & SPRD_INBOX_FIFO_BLOCK_MASK;
	deliver = (fifo_sts & SPRD_INBOX_FIFO_DELIVER_MASK) >>
		   SPRD_INBOX_FIFO_DELIVER_SHIFT;
	irq_msk = readl(priv->inbox_base + SPRD_MBOX_IRQ_MSK) &
			SPRD_INBOX_FIFO_IRQ_MASK;

	/* When deliver success, and corresponding tx_fifo is not empty, start send tx_fifo */
	if (irq_msk == ((~(u32)SPRD_INBOX_FIFO_DELIVER_IRQ) &
	    SPRD_INBOX_FIFO_IRQ_MASK)) {
		mbox_tx_mask = get_tx_fifo_mask(deliver);
		if (mbox_tx_mask)
			wake_up(&deliver_thread_wait);
	}

	/* If block, enable inbox deliver irq, if not, enable inbox block & overflow irq */
	if (block)
		irq_msk = ~(u32)SPRD_INBOX_FIFO_DELIVER_IRQ;
	else
		irq_msk = ~(u32)(SPRD_INBOX_FIFO_BLOCK_IRQ | SPRD_INBOX_FIFO_OVERFLOW_IRQ);
	irq_msk &= SPRD_INBOX_FIFO_IRQ_MASK;
	writel(irq_msk, priv->inbox_base + SPRD_MBOX_IRQ_MSK);

	/* Clear irq status */
	writel(SPRD_MBOX_IRQ_CLR, priv->inbox_base + SPRD_MBOX_IRQ_STS);

	return IRQ_HANDLED;
}

static int check_mbox_chan_state(struct mbox_chan *chan)
{
	struct sprd_mbox_priv *priv = to_sprd_mbox_priv(chan->mbox);
	unsigned long id = (unsigned long)chan->con_priv;
	u32 fifo_sts, recv_flag, cnt = 0;

	fifo_sts = readl(priv->inbox_base + SPRD_MBOX_FIFO_STS);
	if (fifo_sts & (1 << id))
		return -ENOBUFS;

	/* Older version do not support recv_flag */
	if (priv->version >= SPRD_MBOX_VER(1, 1)) {
		/* Wait outbox recv flag, until flag is 0 */
		recv_flag = 1 << (id + SPRD_INBOX_FIFO_RECV_SHIFT);
		while (fifo_sts & recv_flag) {
			cnt++;
			fifo_sts = readl(priv->inbox_base + SPRD_MBOX_FIFO_STS);
			/* Recv flag will always be 1 when block */
			if (fifo_sts & (1 << id))
				return -ENOBUFS;
			if (cnt >= SPRD_INBOX_RECV_TIMEOUT)
				return -ETIMEDOUT;
		}
	}
	return 0;
}

static void put_msg_in_tx_fifo(unsigned long id, void *msg)
{
	u32 pos, wt, rd;

	pr_debug("mailbox: put msg in tx_fifo\n");
	wt = tx_data[id].wt_cnt;
	rd = tx_data[id].rd_cnt;
	pos = wt % SPRD_MBOX_TX_FIFO_LEN;

	if ((rd != wt) && (rd % SPRD_MBOX_TX_FIFO_LEN == pos)) {
		pr_err("mailbox: tx_fifo full, drop msg, dst = %ld, rd =%d, wt = %d\n",
		       id, rd, wt);
		return;
	}

	tx_data[id].fifo[pos] = *(u64 *)msg;
	tx_data[id].wt_cnt++;
}

static int sprd_mbox_try_send(struct mbox_chan *chan, void *msg)
{
	struct sprd_mbox_priv *priv = to_sprd_mbox_priv(chan->mbox);
	unsigned long id = (unsigned long)chan->con_priv;
	u32 *data = msg;
	int ret;

	ret = check_mbox_chan_state(chan);
	if (ret) {
		dev_dbg(priv->dev, "chan %ld is blocked\n", id);
		return ret;
	}

	/* Write data into inbox FIFO, and only support 8 bytes every time */
	writel(data[0], priv->inbox_base + SPRD_MBOX_MSG_LOW);
	writel(data[1], priv->inbox_base + SPRD_MBOX_MSG_HIGH);
	/* Set target core id */
	writel(id, priv->inbox_base + SPRD_MBOX_ID);
	/* Trigger remote request */
	writel(0x1, priv->inbox_base + SPRD_MBOX_TRIGGER);

	return 0;
}

static int sprd_mbox_deliver_thread(void *pdata)
{
	struct sprd_mbox_priv *priv = pdata;
	struct mbox_chan *chan;
	unsigned long flag;
	u32 dst, rd, pos;
	u64 msg;
	int ret;

	while (!kthread_should_stop()) {
		dev_dbg(priv->dev, "tx_fifo waiting deliver event...\n");
		ret = wait_event_interruptible(deliver_thread_wait, mbox_tx_mask);
		if (ret) {
			dev_dbg(priv->dev, "mbox_deliver_thread is interrupted\n");
			continue;
		}
		/* Event triggered, process all tx_fifo. */
		spin_lock_irqsave(&mbox_lock, flag);

		for (dst = 0; dst < SPRD_MBOX_CHAN_MAX; dst++) {
			if (!(mbox_tx_mask & (1 << dst)))
				continue;
			chan = &priv->chan[dst];
			rd = tx_data[dst].rd_cnt;
			while (tx_data[dst].wt_cnt != rd) {
				pos = rd % SPRD_MBOX_TX_FIFO_LEN;
				msg = tx_data[dst].fifo[pos];
				if (sprd_mbox_try_send(chan, &msg))
					/* Send failed, than next chan */
					break;
				tx_data[dst].rd_cnt = ++rd;
			}
		}
		mbox_tx_mask = 0;
		spin_unlock_irqrestore(&mbox_lock, flag);
	}

	return 0;
}

static int sprd_mbox_send_data(struct mbox_chan *chan, void *msg)
{
	unsigned long flag, core_id = (unsigned long)chan->con_priv;

	spin_lock_irqsave(&mbox_lock, flag);

	/* If tx_fifo is not empty, do not send, put msg in tx_fifo */
	if (tx_data[core_id].rd_cnt != tx_data[core_id].wt_cnt) {
		put_msg_in_tx_fifo(core_id, msg);
	/* Try send, if fail, put msg in tx_fifo */
	} else if (sprd_mbox_try_send(chan, msg)) {
		put_msg_in_tx_fifo(core_id, msg);
	}
	spin_unlock_irqrestore(&mbox_lock, flag);

	return 0;
}

static int sprd_mbox_flush(struct mbox_chan *chan, unsigned long timeout)
{
	struct sprd_mbox_priv *priv = to_sprd_mbox_priv(chan->mbox);
	unsigned long id = (unsigned long)chan->con_priv;
	u32 busy;

	timeout = jiffies + msecs_to_jiffies(timeout);

	while (time_before(jiffies, timeout)) {
		busy = readl(priv->inbox_base + SPRD_MBOX_FIFO_STS) &
			SPRD_INBOX_FIFO_BLOCK_MASK;
		if (!(busy & BIT(id))) {
			mbox_chan_txdone(chan, 0);
			return 0;
		}

		udelay(1);
	}

	return -ETIME;
}

static void sprd_mbox_process_rx_fifo(struct mbox_chan *chan)
{
	int i, cnt = 0;
	unsigned long target_id;

	for (i = 0; i < mbox_rx_fifo_cnt; i++) {
		target_id = mbox_rx_fifo[i].core_id;
		/* has been procced */
		if (target_id == SPRD_MBOX_CHAN_MAX) {
			cnt++;
			continue;
		}
		if (chan->cl) {
			mbox_chan_received_data(chan, &mbox_rx_fifo[i].msg);
			mbox_rx_fifo[i].core_id = SPRD_MBOX_CHAN_MAX;
			cnt++;
		} else
			pr_err("mailbox: client is NULL, id = %ld\n", target_id);
	}
	/* reset mbox_mbox_rx_fifo_cnt*/
	if (cnt == mbox_rx_fifo_cnt)
		mbox_rx_fifo_cnt = 0;
}

static int sprd_mbox_startup(struct mbox_chan *chan)
{
	struct sprd_mbox_priv *priv = to_sprd_mbox_priv(chan->mbox);
	u32 val;

	if (!(priv->started++)) {
		/* Select outbox FIFO mode and reset the outbox FIFO status */
		writel(0x0, priv->outbox_base + SPRD_MBOX_FIFO_RST);

		/* Enable inbox FIFO block and overflow, disable deliver irq */
		val = readl(priv->inbox_base + SPRD_MBOX_IRQ_MSK);
		val &= ~(SPRD_INBOX_FIFO_BLOCK_IRQ | SPRD_INBOX_FIFO_OVERFLOW_IRQ);
		val |= SPRD_INBOX_FIFO_DELIVER_IRQ;
		writel(val, priv->inbox_base + SPRD_MBOX_IRQ_MSK);

		/* Enable outbox FIFO not empty interrupt */
		val = readl(priv->outbox_base + SPRD_MBOX_IRQ_MSK);
		val &= ~SPRD_OUTBOX_FIFO_NOT_EMPTY_IRQ;
		writel(val, priv->outbox_base + SPRD_MBOX_IRQ_MSK);
	}

	if (priv->oob_outbox_base && !priv->oob_started) {
		/* Select outbox FIFO mode and reset the outbox FIFO status */
		writel(0x0, priv->oob_outbox_base + SPRD_MBOX_FIFO_RST);

		/* Enable outbox FIFO not empty interrupt */
		val = readl(priv->oob_outbox_base + SPRD_MBOX_IRQ_MSK);
		val &= ~SPRD_OUTBOX_FIFO_NOT_EMPTY_IRQ;
		writel(val, priv->oob_outbox_base + SPRD_MBOX_IRQ_MSK);

		priv->oob_started = 1;
	}

	/* Process messages received before startup */
	sprd_mbox_process_rx_fifo(chan);

	return 0;
}

static void sprd_mbox_shutdown(struct mbox_chan *chan)
{
	struct sprd_mbox_priv *priv = to_sprd_mbox_priv(chan->mbox);

	if (!(--priv->started)) {
		/* Disable inbox & outbox interrupt */
		writel(SPRD_INBOX_FIFO_IRQ_MASK, priv->inbox_base + SPRD_MBOX_IRQ_MSK);
		writel(SPRD_OUTBOX_FIFO_IRQ_MASK, priv->outbox_base + SPRD_MBOX_IRQ_MSK);
	}

	if ((priv->oob_id == (unsigned long)chan->con_priv) && priv->oob_started) {
		writel(SPRD_OUTBOX_FIFO_IRQ_MASK,
			priv->oob_outbox_base + SPRD_MBOX_IRQ_MSK);
		priv->oob_started = 0;
	}
}

static const struct mbox_chan_ops sprd_mbox_ops = {
	.send_data	= sprd_mbox_send_data,
	.flush		= sprd_mbox_flush,
	.startup	= sprd_mbox_startup,
	.shutdown	= sprd_mbox_shutdown,
};

static void sprd_mbox_disable(void *data)
{
	struct sprd_mbox_priv *priv = data;

	clk_disable_unprepare(priv->clk);
}

static int sprd_mbox_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sprd_mbox_priv *priv;
	int ret;
	unsigned long id;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;

	/*
	 * The Spreadtrum mailbox uses an inbox to send messages to the target
	 * core, and uses an outbox to receive messages from other cores.
	 *
	 * Thus the mailbox controller supplies 2 different register addresses
	 * and IRQ numbers for inbox and outbox.
	 */
	priv->inbox_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->inbox_base))
		return PTR_ERR(priv->inbox_base);

	priv->outbox_base = devm_platform_ioremap_resource(pdev, 1);
	if (IS_ERR(priv->outbox_base))
		return PTR_ERR(priv->outbox_base);

	priv->clk = devm_clk_get(dev, "enable");
	if (IS_ERR(priv->clk)) {
		dev_err(dev, "failed to get mailbox clock\n");
		return PTR_ERR(priv->clk);
	}

	ret = clk_prepare_enable(priv->clk);
	if (ret)
		return ret;

	ret = devm_add_action_or_reset(dev, sprd_mbox_disable, priv);
	if (ret) {
		dev_err(dev, "failed to add mailbox disable action\n");
		return ret;
	}

	priv->inbox_irq = platform_get_irq_byname(pdev, "inbox");
	if (priv->inbox_irq < 0)
		return priv->inbox_irq;

	ret = devm_request_irq(dev, priv->inbox_irq, sprd_mbox_inbox_isr,
			       IRQF_NO_SUSPEND, dev_name(dev), priv);
	if (ret) {
		dev_err(dev, "failed to request inbox IRQ: %d\n", ret);
		return ret;
	}

	priv->outbox_irq = platform_get_irq_byname(pdev, "outbox");
	if (priv->outbox_irq < 0)
		return priv->outbox_irq;

	ret = devm_request_irq(dev, priv->outbox_irq, sprd_mbox_outbox_isr,
			       IRQF_NO_SUSPEND, dev_name(dev), priv);
	if (ret) {
		dev_err(dev, "failed to request outbox IRQ: %d\n", ret);
		return ret;
	}

	priv->oob_irq = platform_get_irq_byname(pdev, "oob-outbox");
	if (priv->oob_irq < 0)
		dev_warn(dev, "failed to get oob_irq\n");
	else {
		ret = devm_request_irq(dev, priv->oob_irq,
			sprd_mbox_outbox_isr, 0, dev_name(dev), priv);
		if (ret)
			dev_err(dev, "failed to request oob_irq: %d\n", ret);
	}

	/* Get the default outbox FIFO depth */
	priv->outbox_fifo_depth =
		readl(priv->outbox_base + SPRD_MBOX_FIFO_DEPTH) + 1;
	priv->mbox.dev = dev;
	priv->mbox.chans = &priv->chan[0];
	priv->mbox.num_chans = SPRD_MBOX_CHAN_MAX;
	priv->mbox.ops = &sprd_mbox_ops;
	priv->mbox.txdone_irq = true;
	priv->mbox.of_xlate = sprd_mbox_index_xlate;
	priv->version = readl(priv->inbox_base + SPRD_MBOX_VERSION) &
		SPRD_MBOX_VERSION_MASK;

	for (id = 0; id < SPRD_MBOX_CHAN_MAX; id++)
		priv->chan[id].con_priv = (void *)id;

	ret = devm_mbox_controller_register(dev, &priv->mbox);
	if (ret) {
		dev_err(dev, "failed to register mailbox: %d\n", ret);
		return ret;
	}

	spin_lock_init(&mbox_lock);
	init_waitqueue_head(&deliver_thread_wait);
	/* Create a deliver thread for waiting if tx_fifo can be delivered */
	mbox_deliver_thread = kthread_create(sprd_mbox_deliver_thread,
		priv, "mbox-deliver-thread");
	wake_up_process(mbox_deliver_thread);

#if defined(CONFIG_DEBUG_FS)
	ret = sprd_mbox_debugfs(priv);
	if (ret)
		return ret;
#endif
	dev_info(priv->dev, "probe done, version=0x%x\n", priv->version);

	return 0;
}

static const struct of_device_id sprd_mbox_of_match[] = {
	{ .compatible = "unisoc,mailbox-r1p2", },
	{ },
};
MODULE_DEVICE_TABLE(of, sprd_mbox_of_match);

static struct platform_driver sprd_mbox_driver = {
	.driver = {
		.name = "unisoc-mailbox",
		.of_match_table = sprd_mbox_of_match,
	},
	.probe	= sprd_mbox_probe,
};
module_platform_driver(sprd_mbox_driver);

MODULE_AUTHOR("Baolin Wang <baolin.wang@unisoc.com>");
MODULE_DESCRIPTION("Unisoc mailbox driver");
MODULE_LICENSE("GPL v2");
