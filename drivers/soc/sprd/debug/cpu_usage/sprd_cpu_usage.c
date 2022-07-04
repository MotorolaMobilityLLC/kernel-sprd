// SPDX-License-Identifier: GPL-2.0
/*
 * sprd/debug/cpu_usage
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cpumask.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/threads.h>
#include <linux/rtc.h>
#include <asm/div64.h>
#include <../../../kernel/sched/sched.h>
#ifdef CONFIG_VM_EVENT_COUNTERS
#include <linux/vmstat.h>
#endif
#include "../sprd_debugfs.h"

#define NR_RECORD 3
#define HRTIMER_INTERVAL 10
#define MAX_SIZE_A_LOG	32
#define LOG_BUFF_SIZE	256
#define BUFF_ID(idx)	((idx) % (NR_RECORD))
#define NEXT_ID(idx)	BUFF_ID((idx) + 1)
#define ns_to_ms(time)	(do_div(time, NSEC_PER_MSEC))
#define ns_to_us(time)  (do_div(time, NSEC_PER_USEC))
#define unused(x) ((void)(x))

/*
 * struct task_struct defined in linux/sched.h
 *   u64  utime;
 *   u64  stime;
 */
struct sprd_thread_time {
	u64	ut;
	u64	st;
};

struct sprd_iowait {
	u64	start;
	u64	total[NR_RECORD];
	ulong	cnt[NR_RECORD];
	ulong	idx;
};

/* === thread info kept on stack === */
struct sprd_thread_info {
	struct sprd_thread_time	start;
	struct sprd_thread_time	delta[NR_RECORD];
	struct sprd_iowait io;
	ulong	idx;
};

struct sprd_cpu_stat {
	struct kernel_cpustat k;
	u64	sum;
	u64	nr_switches;
#ifdef CONFIG_VM_EVENT_COUNTERS
	unsigned long	nr_pgfault;
	unsigned long	nr_pgmajfault;
#endif
};

struct sprd_time_info {
	u64 ns_start;
	u64 ns_end;
	struct timespec ts_start;
	struct timespec ts_end;
	struct rtc_time rtc_start;
	struct rtc_time rtc_end;
};

struct sprd_cpu_info {
	struct sprd_cpu_stat cpu[NR_CPUS];
	struct sprd_cpu_stat all;
	struct sprd_time_info time;
};

/* === cpu usage struct === */
struct sprd_cpu_usage {
	spinlock_t lock;
	struct sprd_cpu_info info[NR_RECORD];
	struct sprd_cpu_info cat;

	/* idx++ when hrtimer interval */
	unsigned long idx;
	unsigned int interval;
	struct hrtimer hrtimer;

	struct sprd_cpu_stat cpu_last[NR_CPUS];
	struct timespec ts_last;
	u64 ns_last;

	char *buf;
	int offset;

	unsigned int cating;
	unsigned int ticking;
};

static char stat_table[] = {
	CPUTIME_IDLE,
	CPUTIME_USER,
	CPUTIME_SYSTEM,
	CPUTIME_NICE,
	CPUTIME_IOWAIT,
	CPUTIME_IRQ,
	CPUTIME_SOFTIRQ,
	CPUTIME_STEAL
};

static struct sprd_cpu_usage *p_sprd_cpu_usage;

static void _ratio_calc(u64 dividend, u64 divider, ulong *result)
{
	u64 tmp;

	if (divider == 0) {
		result[0] = result[1] = 0;
		return;
	}

	/*save result as xx.xx% */
#if BITS_PER_LONG == 64
	tmp = 10000 * dividend;
	tmp = tmp / divider;

	result[1] = (ulong)(tmp % 100);
	result[0] = (ulong)(tmp / 100);
#elif BITS_PER_LONG == 32
	/* convert ns to us, then 32 bits is enough for 10s */
	ns_to_us(dividend);
	ns_to_us(divider);

	tmp = 10000 * dividend;
	do_div(tmp, divider);

	result[1] = (ulong)(do_div(tmp, 100));
	result[0] = (ulong)tmp;
#endif
}

/*
 * new: new line
 */
static void sprd_cpu_log(bool new, const char *fmt, ...)
{
	va_list va;
	struct sprd_cpu_usage *p = p_sprd_cpu_usage;
	int len;

	if (new)
		p->offset = 0;
	else if ((p->offset + MAX_SIZE_A_LOG) >= LOG_BUFF_SIZE)
		return;

	va_start(va, fmt);
	len = vsnprintf(&p->buf[p->offset], MAX_SIZE_A_LOG, fmt, va);
	va_end(va);

	p->offset += len;
}

static void _clean_thread_iowait(struct sprd_iowait *io, ulong id)
{
	int clear = io->idx;
	unsigned long count = id - clear;

	if (count > NR_RECORD)
		count = NR_RECORD;

	while (count--) {
		clear = NEXT_ID(clear);
		io->total[clear] = 0;
		io->cnt[clear] = 0;
	}

	io->idx = id;
}

static void _clean_thread_info(struct sprd_thread_info *t, unsigned long id)
{
	int clear = t->idx;
	unsigned long count = id - clear;

	if (count > NR_RECORD)
		count = NR_RECORD;

	while (count--) {
		clear = NEXT_ID(clear);
		t->delta[clear].ut = 0;
		t->delta[clear].st = 0;
	}

	t->idx = id;
}

#ifdef CONFIG_THREAD_INFO_IN_TASK
#define T_OFFSET (16)
#else
#define T_OFFSET (sizeof(struct thread_info) + 8)
#endif

#define T_BUF(task) ((struct sprd_thread_info *)(task->stack + T_OFFSET))

/* update the thread utime and stime*/
void sprd_update_cpu_usage(struct task_struct *prev, struct task_struct *next)
{
	struct sprd_cpu_usage *p = p_sprd_cpu_usage;
	struct sprd_thread_info *t;
	struct sprd_thread_time *time;
	int i, idx;

	if (!p)
		return;

	/* global idx may change, just read once */
	idx = p->idx;
	i = BUFF_ID(idx);

	/* update prev */
	t = T_BUF(prev);
	if (t->idx != idx)
		_clean_thread_info(t, idx);

	time = &t->delta[i];
	time->ut += (prev->utime - t->start.ut);
	time->st += (prev->stime - t->start.st);

	if (prev->in_iowait)
		t->io.start = cpu_clock(0);

	/* update next */
	t = T_BUF(next);
	t->start.ut = next->utime;
	t->start.st = next->stime;

	if (next->in_iowait) {
		if (t->io.idx != idx)
			_clean_thread_iowait(&t->io, idx);
		t->io.total[i] += (cpu_clock(0) - t->io.start);
		t->io.cnt[i]++;
	}
}
EXPORT_SYMBOL(sprd_update_cpu_usage);

/* copy from fs/proc/stat.c */
static u64 get_idle_time(struct kernel_cpustat *kcs, int cpu)
{
	u64 idle, idle_usecs = -1ULL;

	if (cpu_online(cpu))
		idle_usecs = get_cpu_idle_time_us(cpu, NULL);

	if (idle_usecs == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.idle */
		idle = kcs->cpustat[CPUTIME_IDLE];
	else
		idle = idle_usecs * NSEC_PER_USEC;

	return idle;
}

static u64 get_iowait_time(struct kernel_cpustat *kcs, int cpu)
{
	u64 iowait, iowait_usecs = -1ULL;

	if (cpu_online(cpu))
		iowait_usecs = get_cpu_iowait_time_us(cpu, NULL);

	if (iowait_usecs == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.iowait */
		iowait = kcs->cpustat[CPUTIME_IOWAIT];
	else
		iowait = iowait_usecs * NSEC_PER_USEC;

	return iowait;
}

static void __get_cpu_stat(struct sprd_cpu_stat *stat, int cpu)
{
	struct kernel_cpustat *kcs = &kcpustat_cpu(cpu);
	int i;
#ifdef CONFIG_VM_EVENT_COUNTERS
	struct vm_event_state *this = &per_cpu(vm_event_states, cpu);

	stat->nr_pgfault = this->event[PGFAULT];
	stat->nr_pgmajfault = this->event[PGMAJFAULT];
#endif

	stat->nr_switches = cpu_rq(cpu)->nr_switches;

	stat->sum = 0;
	for (i = 0; i < NR_STATS; i++) {
		if (i == CPUTIME_IDLE)
			stat->k.cpustat[i] = get_idle_time(kcs, cpu);
		else if (i == CPUTIME_IOWAIT)
			stat->k.cpustat[i] = get_iowait_time(kcs, cpu);
		else
			stat->k.cpustat[i] = kcs->cpustat[i];

		/* todo: convert to clock_t base on USER_HZ? */
		stat->sum += stat->k.cpustat[i];
	}
}

static void __get_cpu_stat_delta(struct sprd_cpu_stat *delta,
				 struct sprd_cpu_stat *new,
				 struct sprd_cpu_stat *saved)
{
	int i;

	for (i = 0; i < NR_STATS; i++) {
		delta->k.cpustat[i] = new->k.cpustat[i] - saved->k.cpustat[i];
		if (new->k.cpustat[i] < saved->k.cpustat[i])
			printk(KERN_INFO "stat[%d]: <%lld> < save<%lld>\r\n",
			       i, new->k.cpustat[i], saved->k.cpustat[i]);
	}

	delta->sum = new->sum - saved->sum;
	delta->nr_switches = new->nr_switches - saved->nr_switches;
#ifdef CONFIG_VM_EVENT_COUNTERS
	delta->nr_pgfault = new->nr_pgfault - saved->nr_pgfault;
	delta->nr_pgmajfault = new->nr_pgmajfault - saved->nr_pgmajfault;
#endif
}

static void __get_all_cpu_stat_delta(struct sprd_cpu_info *info)
{
	int i, j;

	memset(&info->all, 0, sizeof(struct sprd_cpu_stat));

	for_each_possible_cpu(i) {
		/* for pclint */
		if (i >= NR_CPUS)
			continue;

		for (j = 0; j < NR_STATS; j++)
			info->all.k.cpustat[j] += info->cpu[i].k.cpustat[j];

		info->all.sum += info->cpu[i].sum;
		info->all.nr_switches += info->cpu[i].nr_switches;
#ifdef CONFIG_VM_EVENT_COUNTERS
		info->all.nr_pgfault += info->cpu[i].nr_pgfault;
		info->all.nr_pgmajfault += info->cpu[i].nr_pgmajfault;
#endif
	}
}

/*
 * update:
 *   true:  when hrtimer come
 *   false: when cat /d/sprd_debug/cpu/cpu_usage
 *
 *   update sprd_cpu_info of p->idx
 */
static ulong _update_cpu_usage(bool update)
{
	struct sprd_cpu_usage *p = p_sprd_cpu_usage;
	struct sprd_cpu_info *info;
	struct sprd_cpu_stat tmp;
	ulong flags, id;
	int i;

	id = p->idx;
	if (update)
		info = &p->info[BUFF_ID(id)];
	else
		info = &p->cat;

	/* record the start ns/timespec/rtc_time */
	info->time.ns_start = p->ns_last;
	memcpy(&info->time.ts_start, &p->ts_last, sizeof(struct timespec));
	rtc_time_to_tm(info->time.ts_start.tv_sec, &info->time.rtc_start);

	/* record the end */
	info->time.ns_end = cpu_clock(0);
	getnstimeofday(&info->time.ts_end);
	rtc_time_to_tm(info->time.ts_end.tv_sec, &info->time.rtc_end);

	/* update per-cpu struct sprd_cpu_stat */
	for_each_possible_cpu(i) {
		if (i >= NR_CPUS)
			continue;

		__get_cpu_stat(&tmp, i);
		__get_cpu_stat_delta(&info->cpu[i], &tmp, &p->cpu_last[i]);
		if (update)
			memcpy(&p->cpu_last[i], &tmp, sizeof(tmp));
	}

	/* update all-cpu struct sprd_cpu_stat */
	__get_all_cpu_stat_delta(info);

	if (update) {
		p->ns_last = info->time.ns_end;
		memcpy(&p->ts_last, &info->time.ts_end, sizeof(struct timespec));
		spin_lock_irqsave(&p->lock, flags);
		if (p->cating)
			/* idx++ when cating clear*/
			p->ticking++;
		else
			p->idx++;
		spin_unlock_irqrestore(&p->lock, flags);
	}

	return id;
}

static void _print_time_info(struct seq_file *m, struct sprd_cpu_info *info)
{
	u64 start_ms, end_ms;

	/* nano second */
	start_ms = info->time.ns_start;
	end_ms = info->time.ns_end;

	/* ns to ms for print */
	ns_to_ms(start_ms);
	ns_to_ms(end_ms);

	seq_printf(m, "\n\nCpu Core Count: %-6d\n", num_possible_cpus());
	seq_printf(m, "Timer Circle: %-llums.\n", (end_ms - start_ms));
	seq_printf(m,
		"  From time %llums(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC) "\
		"to %llums(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC).\n\n",
		start_ms,
		info->time.rtc_start.tm_year + 1900,
		info->time.rtc_start.tm_mon + 1,
		info->time.rtc_start.tm_mday,
		info->time.rtc_start.tm_hour,
		info->time.rtc_start.tm_min,
		info->time.rtc_start.tm_sec,
		info->time.ts_start.tv_nsec, end_ms,
		info->time.rtc_end.tm_year + 1900,
		info->time.rtc_end.tm_mon + 1,
		info->time.rtc_end.tm_mday,
		info->time.rtc_end.tm_hour,
		info->time.rtc_end.tm_min,
		info->time.rtc_end.tm_sec,
		info->time.ts_end.tv_nsec);
}

static void __add_cpu_stat_log(struct sprd_cpu_stat *cpu)
{
	ulong rati[2];
	int i, j;

	for (i = 0; i < sizeof(stat_table); i++) {
		j = stat_table[i];
		/* just for pc lint */
		if (j >= NR_STATS)
			continue;

		if (cpu->k.cpustat[j]) {
			_ratio_calc(cpu->k.cpustat[j], cpu->sum, rati);
			sprd_cpu_log(false, "%4lu.%02lu%% ", rati[0], rati[1]);
		} else
			sprd_cpu_log(false, "%8s ", "-----");
	}

	sprd_cpu_log(false, " 100.00%% |");
	sprd_cpu_log(false, " %15llu", cpu->nr_switches);
#ifdef CONFIG_VM_EVENT_COUNTERS
	sprd_cpu_log(false, " %15lu", cpu->nr_pgfault);
	sprd_cpu_log(false, " %15lu", cpu->nr_pgmajfault);
#endif
}

static void _print_cpu_stat(struct seq_file *m,
			    struct sprd_cpu_info *info, int id)
{
	int i;

	seq_printf(m, "%-87s   %-s\n", " * CPU USAGE:", " | * OTHER COUNTS:");
#ifdef CONFIG_VM_EVENT_COUNTERS
	seq_printf(m,
		" -%d-      %8s %8s %8s %8s %8s %8s %8s %8s %8s "\
		"| %15s %15s %15s\n",
		id, "IDLE", "USER", "SYSTEM", "NICE", "IOWAIT", "IRQ",
		"SOFTIRQ", "STEAL", "TOTAL", "CTXT_SWITCH",
		"FG_FAULT", "FG_MAJ_FAULT");
#else
	seq_printf(m,
		" -%d-      %8s %8s %8s %8s %8s %8s %8s %8s %8s | %15s\n",
		id, "IDLE", "USER", "SYSTEM", "NICE", "IOWAIT", "IRQ",
		"SOFTIRQ", "STEAL", "TOTAL", "CTXT_SWITCH");
#endif

	for_each_possible_cpu(i) {
		if (i >= NR_CPUS)
			continue;

		sprd_cpu_log(true, " cpu%d(%d): ", i, cpu_online(i));
		__add_cpu_stat_log(&info->cpu[i]);
		seq_printf(m, "%s\n", p_sprd_cpu_usage->buf);
	}

	seq_puts(m, " ==================\n");

	sprd_cpu_log(true, " Total:   ");
	__add_cpu_stat_log(&info->all);
	seq_printf(m, "%s\n", p_sprd_cpu_usage->buf);
}

static void __add_a_iowait(u64 ns, int cnt)
{
	if (cnt) {
		ns_to_ms(ns);
		sprd_cpu_log(false, "%5llu/%-6d|", ns, cnt);
	} else
		sprd_cpu_log(false, "%13s", "|");
}

static void __add_a_thread(u64 item, u64 sum)
{
	ulong ratio[2];

	if (item) {
		_ratio_calc(item, sum, ratio);
		sprd_cpu_log(false, "%4lu.%02lu%%", ratio[0], ratio[1]);
	} else
		sprd_cpu_log(false, "%8s", "-----");
}

struct _records {
	u64 time[NR_RECORD];
	u64 st[NR_RECORD];
	u64 ut[NR_RECORD];
	u64 st_all[NR_RECORD];
	u64 ut_all[NR_RECORD];
	u64 iowait[NR_RECORD];
	int cnt[NR_RECORD];
};

/* id: sprd_thread_info->delta[id]
 *     sprd_cpu_usage->info[id]
 */
static void _print_thread_info(struct seq_file *m, ulong id)
{
	struct sprd_cpu_usage *p = p_sprd_cpu_usage;
	struct sprd_cpu_info *info;
	struct task_struct *gp, *pp;
	struct sprd_thread_info *t;
	struct sprd_thread_time	*delta;
	struct _records *r;
	void *stack;
	int i, j;
	bool print;

	r = kzalloc(sizeof(struct _records), GFP_KERNEL);
	if (!r)
		return;

	id = BUFF_ID(id);
	/* record from (id+1)/3 to id */
	for (i = 0, j = NEXT_ID(id); i < NR_RECORD; i++, j = NEXT_ID(j)) {
		if (j == id)
			info = &p->cat;
		else
			info = &p->info[j];
		r->time[i] = info->time.ns_end - info->time.ns_start;
	}

	seq_puts(m, "\n* USAGE PER THREAD:\n");
	seq_printf(m, " %-6s%24s   %24s   %24s       %-15s\n", "PID", "USER",
		       "SYSTEM", "TOTAL", "NAME");

	read_lock(&tasklist_lock);
	do_each_thread(gp, pp) {
		stack = try_get_task_stack(pp);
		if (stack == NULL)
			continue;

		print = false;
		t = (struct sprd_thread_info *)(stack + T_OFFSET);
		if (t->idx != p->idx)
			_clean_thread_info(t, p->idx);

		if (t->io.idx != p->idx)
			_clean_thread_iowait(&t->io, p->idx);

		for (i = 0, j = NEXT_ID(id); i < NR_RECORD; i++, j = NEXT_ID(j)) {
			delta = &t->delta[j];
			r->st[i] = delta->st;
			r->ut[i] = delta->ut;

			if ((r->st[i] + r->ut[i]) != 0) {
				r->st_all[i] += r->st[i];
				r->ut_all[i] += r->ut[i];
				print = true;
			}

			r->iowait[i] = t->io.total[j];
			r->cnt[i] = t->io.cnt[j];
			if (r->cnt[i])
				print = true;
		}

		if (pp->in_iowait) {
			/* id: recent and last */
			r->iowait[NR_RECORD - 1] += (cpu_clock(0) - t->io.start);
			r->cnt[NR_RECORD - 1]++;
			print = true;
		}

		put_task_stack(pp);

		if (!print)
			continue;

		/*print a thread's info */
		sprd_cpu_log(true, " %-6d", pp->pid);
		/* add user time */
		for (i = 0; i < NR_RECORD; i++)
			__add_a_thread(r->ut[i], r->time[i]);

		sprd_cpu_log(false, "%s", " | ");
		/* add system time */
		for (i = 0; i < NR_RECORD; i++)
			__add_a_thread(r->st[i], r->time[i]);

		sprd_cpu_log(false, "%s", " | ");
		/* add total time */
		for (i = 0; i < NR_RECORD; i++)
			__add_a_thread((r->ut[i] + r->st[i]), r->time[i]);

		sprd_cpu_log(false, "%s    %-15s:", " | ", pp->comm);
		for (i = 0; i < NR_RECORD; i++)
			__add_a_iowait(r->iowait[i], r->cnt[i]);
		seq_printf(m, "%s\n", p->buf);
	} while_each_thread(gp, pp);
	read_unlock(&tasklist_lock);

	/* print total*/
	seq_puts(m, " ==================\n");
	sprd_cpu_log(true, " %-6s", "Total:");
	for (i = 0; i < NR_RECORD; i++)
		__add_a_thread(r->ut_all[i], r->time[i]);
	sprd_cpu_log(false, "%s", " | ");
	for (i = 0; i < NR_RECORD; i++)
		__add_a_thread(r->st_all[i], r->time[i]);
	sprd_cpu_log(false, "%s", " | ");
	for (i = 0; i < NR_RECORD; i++)
		__add_a_thread((r->ut_all[i] + r->st_all[i]), r->time[i]);
	sprd_cpu_log(false, "%s    %-15s", " | ", current->comm);
	seq_printf(m, "%s\n", p->buf);

	kfree(r);
}

static int show_cpu_usage(struct seq_file *m, void *v)
{
	struct sprd_cpu_usage *p = p_sprd_cpu_usage;
	int cnt = NR_RECORD;
	ulong flags, id;
	int i;

	unused(v);

	spin_lock_irqsave(&p->lock, flags);
	p->cating++;
	spin_unlock_irqrestore(&p->lock, flags);

	id = i = _update_cpu_usage(false);

	/* print from the earliest time */
	while (--cnt) {
		i = NEXT_ID(i);
		_print_time_info(m, &p->info[i]);
		_print_cpu_stat(m, &p->info[i], i);
	}

	/* print cat saved */
	i = BUFF_ID(id);
	_print_time_info(m, &p->cat);
	_print_cpu_stat(m, &p->cat, i);

	/* print NR_RECORD times thread info in one line */
	_print_thread_info(m, id);

	spin_lock_irqsave(&p->lock, flags);
	p->cating--;
	if (p->cating == 0 && p->ticking) {
		p->idx += p->ticking;
		p->ticking = 0;
	}
	spin_unlock_irqrestore(&p->lock, flags);
	return 0;
}

static int cpu_usage_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_cpu_usage, NULL);
}

/*
 * change the hrtimer interval
 */
static ssize_t cpu_usage_write(struct file *file, const char __user *buf,
		size_t len, loff_t *ppos)
{
	return len;
}

const struct file_operations cpu_usage_fops = {
	.open = cpu_usage_open,
	.read = seq_read,
	.write = cpu_usage_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static enum hrtimer_restart sprd_cpu_usage_hr_func(struct hrtimer *timer)
{
	ktime_t kt;

	_update_cpu_usage(true);

	kt = ms_to_ktime(p_sprd_cpu_usage->interval * MSEC_PER_SEC);
	hrtimer_forward_now(timer, kt);
	return HRTIMER_RESTART;
}

static int __init sprd_cpu_usage_init(void)
{
	struct sprd_cpu_usage *p;
	struct dentry *dentry;
	ktime_t kt;

	p = kzalloc(sizeof(struct sprd_cpu_usage), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	p->buf = kmalloc(LOG_BUFF_SIZE, GFP_KERNEL);
	if (!p->buf) {
		kfree(p);
		return -ENOMEM;
	}

	/* create /sys/kernel/debug/sprd_debug/cpu/cpu_usage */
	dentry = debugfs_create_file("cpu_usage",
				      0444,
				      sprd_debugfs_entry(CPU),
				      NULL,
				      &cpu_usage_fops);
	if (IS_ERR(dentry)) {
		kfree(p->buf);
		kfree(p);
		return -ENOMEM;
	}

	/* init the member */
	spin_lock_init(&p->lock);
	p->interval = HRTIMER_INTERVAL;
	kt = ms_to_ktime(p->interval * MSEC_PER_SEC);
	hrtimer_init(&p->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	p->hrtimer.function = sprd_cpu_usage_hr_func;
	hrtimer_start(&p->hrtimer, kt, HRTIMER_MODE_REL);

	p_sprd_cpu_usage = p;
	return 0;
}

static void __exit sprd_cpu_usage_exit(void)
{
	if (p_sprd_cpu_usage) {
		hrtimer_cancel(&p_sprd_cpu_usage->hrtimer);
		kfree(p_sprd_cpu_usage->buf);
		kfree(p_sprd_cpu_usage);
	}
}

subsys_initcall(sprd_cpu_usage_init);
module_exit(sprd_cpu_usage_exit);
