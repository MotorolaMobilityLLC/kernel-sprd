// SPDX-License-Identifier: GPL-2.0
/*
 * /proc/cpuload implementation
 */
#include "walt.h"

static int show_cpuload(struct seq_file *seq, void *v)
{
	int cpu;

	if (v == (void *)1) {
		seq_printf(seq, "timestamp %lu\n", jiffies);
		seq_printf(seq, "%-8s\t%-16s\t%-16s\t%-16s\n",
			"cpu", "cpu_load", "running_tasks", "iowait_tasks");
	} else {
		struct rq *rq;
		struct uni_rq *uni_rq;
		u64 prev_runnable_sum;

		cpu = (unsigned long)(v - 2);
		rq = cpu_rq(cpu);
		uni_rq = (struct uni_rq *) rq->android_vendor_data1;
		prev_runnable_sum = uni_rq->prev_runnable_sum;

		prev_runnable_sum <<= SCHED_CAPACITY_SHIFT;
		do_div(prev_runnable_sum, walt_ravg_window);
		seq_printf(seq, "%-8d\t%-16lu\t%-16u\t%-16u\n",
			cpu, min_t(unsigned long, prev_runnable_sum, capacity_orig_of(cpu)),
			rq->nr_running, atomic_read(&rq->nr_iowait));
	}
	return 0;
}

/*
 * This iterator needs some explanation.
 * It returns 1 for the header position.
 * This means 2 is cpu 0.
 * In a hotplugged system some CPUs, including cpu 0, may be missing so we have
 * to use cpumask_* to iterate over the CPUs.
 */
static void *cpuload_start(struct seq_file *file, loff_t *offset)
{
	unsigned long n = *offset;

	if (n == 0)
		return (void *) 1;

	n--;

	if (n > 0)
		n = cpumask_next(n - 1, cpu_online_mask);
	else
		n = cpumask_first(cpu_online_mask);

	*offset = n + 1;

	if (n < nr_cpu_ids)
		return (void *)(unsigned long)(n + 2);

	return NULL;
}

static void *cpuload_next(struct seq_file *file, void *data, loff_t *offset)
{
	(*offset)++;

	return cpuload_start(file, offset);
}

static void cpuload_stop(struct seq_file *file, void *data)
{
}

static const struct seq_operations cpuload_sops = {
	.start = cpuload_start,
	.next  = cpuload_next,
	.stop  = cpuload_stop,
	.show  = show_cpuload,
};

int proc_cpuload_init(void)
{
	proc_create_seq("cpuload", 0, NULL, &cpuload_sops);
	return 0;
}
