// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kfifo.h>

#define FIFO_SIZE (4096 * 4)
#define MAX_LOG 128
#define SLOTS 12
#define ns_to_ms(v) ktime_to_ms(ns_to_ktime((v)))

struct rq_info {
	struct list_head list;
	char	name[DISK_NAME_LEN];
	ulong	cnt[2];		 /* 0: async, 1: sync */
	ulong	i2i[2][SLOTS+1]; /* i2i[SLOTS] for max value in ms */
	ulong	i2c[2][SLOTS+1];
	u64	total_i[2];
	u64	total_c[2];
};

struct rq_iic_context {
	struct proc_dir_entry *proc;
	spinlock_t lock;
	struct list_head list;
	u64 time;
	u64 cat;
	struct kfifo fifo;
	char *buff;
};
static struct rq_iic_context *sprd_rq_iic_context;

static const char help[] =
	"|No. of req in range[ <4ms 4ms+ 8ms+  16+  32+  64+ 128+ 256+ 512+  1s+  2s+  4s+]\n";

/*
 * old: from <1ms, to >=1024ms
 * now: from <4ms, to >=4096ms
 */
static inline unsigned int ms_to_slot(unsigned int ms)
{
	ms /= 4;
	return ms > 0 ? min((SLOTS - 1), ilog2(ms) + 1) : 0;
}

static struct rq_info *get_rq_info(struct list_head *head, struct request *rq)
{
	struct rq_info *info;

	/* todo: rq_disk NULL */
	if (!rq->rq_disk)
		return NULL;

	list_for_each_entry(info, head, list) {
		if (!strcmp(info->name, rq->rq_disk->disk_name))
			return info;
		if (!strncmp(info->name, "loop", 4))
			return info;
	}

	info = kzalloc(sizeof(*info), GFP_ATOMIC);
	if (!info)
		return NULL;

	INIT_LIST_HEAD(&info->list);
	if (!strncmp(rq->rq_disk->disk_name, "loop", 4))
		strcpy(info->name, "loopx");
	else
		strcpy(info->name, rq->rq_disk->disk_name);
	list_add_tail(&info->list, head);
	return info;
}

static void _update_rx(struct kfifo *fifo)
{
	int cnt = MAX_LOG;
	bool peek = true;
	char c;

	do {
		if (kfifo_peek(fifo, &c)) {
			if (c == '|' || c == '=')
				peek = false;
			else
				kfifo_skip(fifo);
			--cnt;
		}
	} while (cnt && peek);
}

static void io_log_in(struct kfifo *fifo, char *buf, int len)
{
	int avail = kfifo_avail(fifo);
	bool over = false;

	if (avail < len)
		over = true;

	while (avail++ < len)
		kfifo_skip(fifo);

	kfifo_in(fifo, buf, len);

	if (over)
		_update_rx(fifo);
}

static void io_log_save(struct kfifo *fifo, const char *fmt, ...)
{
	char buf[MAX_LOG];
	va_list va;
	int len;

	va_start(va, fmt);
	len = vsnprintf(buf, MAX_LOG, fmt, va);
	va_end(va);

	io_log_in(fifo, buf, len);
}

static void info_log(struct rq_iic_context *p, struct rq_info *info, bool i2c, bool sync)
{
	ulong (*c)[SLOTS + 1];
	char buf[MAX_LOG];
	u64 a;
	int n = 0;
	int i;

	if (i2c) {
		c = &info->i2c[sync];
		a = info->total_c[sync];
		info->total_c[sync] = 0; /* get then clear */
	} else {
		c = &info->i2i[sync];
		a = info->total_i[sync];
		info->total_i[sync] = 0;
	}

	do_div(a, 1000000UL);
	do_div(a, info->cnt[sync]);

	n += snprintf(&buf[n], MAX_LOG - n, "|_%s %7s %5s:[",
		      i2c ? "i2c" : "i2i", info->name, sync ? "sync" : "async");

	for (i = 0; i < SLOTS; i++)
		if ((*c)[i] == 0)
			n += snprintf(&buf[n], MAX_LOG - n, "%5s", "-");
		else
			n += snprintf(&buf[n], MAX_LOG - n, "%5ld", (*c)[i]);

	n += snprintf(&buf[n], MAX_LOG - n, "] m/a(ms):%5ld/%-4llu\n", (*c)[SLOTS], a);

	io_log_in(&p->fifo, buf, n);

	memset(*c, 0, sizeof(ulong) * (SLOTS + 1));
}

static void rq_iic_store(struct rq_iic_context *p, u64 complete)
{
	struct rq_info *info;
	u64 ts, ts2;
	ulong ns, ns2;

	ts = p->time;
	ns = do_div(ts, 1000000000);
	ts2 = complete;
	ns2 = do_div(ts2, 1000000000);

	io_log_save(&p->fifo, "=> [%5lu.%06lu] - [%5lu.%06lu]:\n",
			(ulong)ts, ns/1000, (ulong)ts2, ns2/1000);

	list_for_each_entry(info, &p->list, list) {
		if (info->cnt[0]) {
			info_log(p, info, false, false);
			info_log(p, info, true, false);
			info->cnt[0] = 0;
		}
		if (info->cnt[1]) {
			info_log(p, info, false, true);
			info_log(p, info, true, true);
			info->cnt[1] = 0;
		}
	}
	p->time = complete;
}

void sprd_monitor_rq_complete(struct request *rq)
{
	struct rq_iic_context *p = sprd_rq_iic_context;
	struct rq_info *info;
	u64 insert, issue, complete;
	unsigned long flags;
	unsigned int i2i, i2c, slot_i2i, slot_i2c;
	bool sync;

	if (!p)
		return;

	insert = rq->start_time_ns;
	issue = rq->io_start_time_ns;
	if (!insert || !issue)
		return;
	complete = ktime_get_ns();

	i2i = ns_to_ms(issue - insert);
	i2c = ns_to_ms(complete - issue);

	slot_i2i = ms_to_slot(i2i);
	slot_i2c = ms_to_slot(i2c);

	sync = rq_is_sync(rq);

	spin_lock_irqsave(&p->lock, flags);
	info = get_rq_info(&p->list, rq);
	if (!info) {
		spin_unlock_irqrestore(&p->lock, flags);
		return;
	}

	++info->cnt[sync];
	++info->i2i[sync][slot_i2i];
	++info->i2c[sync][slot_i2c];
	info->total_i[sync] += (issue - insert);
	info->total_c[sync] += (complete - issue);

	if (i2i > info->i2i[sync][SLOTS])
		info->i2i[sync][SLOTS] = i2i;
	if (i2c > info->i2c[sync][SLOTS])
		info->i2c[sync][SLOTS] = i2c;

	if (complete > (p->time + 1000000000ULL * 10))
		rq_iic_store(p, complete);

	spin_unlock_irqrestore(&p->lock, flags);
}
EXPORT_SYMBOL_GPL(sprd_monitor_rq_complete);

static int rq_iic_show(struct seq_file *m, void *v)
{
	struct rq_iic_context *p = sprd_rq_iic_context;
	u64 now = ktime_get_ns();
	unsigned long flags;
	int len;

	if (!p)
		return 0;

	spin_lock_irqsave(&p->lock, flags);
	rq_iic_store(p, now);

	len = kfifo_out(&p->fifo, p->buff, FIFO_SIZE);
	p->buff[len] = '\0';
	seq_printf(m, "%s", p->buff);

	if (now > (p->cat + 1000000000ULL * 100))
		io_log_in(&p->fifo, (char *)help, strlen(help));
	p->cat = now;

	spin_unlock_irqrestore(&p->lock, flags);
	return 0;
}

static int rq_iic_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, rq_iic_show, PDE_DATA(inode), FIFO_SIZE);
}

static const struct proc_ops rq_iic_fops = {
	.proc_open	= rq_iic_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

static int __init proc_io_iic_init(void)
{
	struct rq_iic_context *p;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	p->buff = kmalloc(FIFO_SIZE, GFP_KERNEL);
	if (!p->buff) {
		kfree(p);
		return -ENOMEM;
	}

	if (kfifo_alloc(&p->fifo, FIFO_SIZE, GFP_KERNEL)) {
		kfree(p->buff);
		kfree(p);
		return -ENOMEM;
	}

	spin_lock_init(&p->lock);
	INIT_LIST_HEAD(&p->list);

	p->proc = proc_mkdir("sprd_io", NULL);
	if (!p->proc) {
		kfifo_free(&p->fifo);
		kfree(p->buff);
		kfree(p);
		return -ENOMEM;
	}

	proc_create_data("rq_iic", 0444, p->proc, &rq_iic_fops, NULL);

	io_log_in(&p->fifo, (char *)help, strlen(help));
	sprd_rq_iic_context = p;
	return 0;
}

early_initcall(proc_io_iic_init);
