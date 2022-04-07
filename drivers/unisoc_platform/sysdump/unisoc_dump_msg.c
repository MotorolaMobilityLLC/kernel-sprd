// SPDX-License-Identifier: GPL-2.0
/*
 * Sysdump sched stats for Spreadtrum SoCs
 *
 * Copyright (C) 2021 Spreadtrum corporation. http://www.unisoc.com
 */

#define pr_fmt(fmt)  "sprd-sysdump-msg: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kmsg_dump.h>
#include <linux/seq_buf.h>
#include <linux/mm.h>
#include <linux/stacktrace.h>
#include <asm/stacktrace.h>
#include <linux/kallsyms.h>
#include <linux/soc/sprd/sprd_sysdump.h>
#include "../../../../../kernel/sched/sched.h"
#ifdef CONFIG_ARM64
#include "sysdump64.h"
#endif

/* GKI requires the NR_CPUS is 32 */
#if NR_CPUS >= 8
#define SPRD_NR_CPUS		8
#else
#define SPRD_NR_CPUS		NR_CPUS
#endif
#define SPRD_DUMP_RQ_SIZE	(2000 * SPRD_NR_CPUS)
#define SPRD_DUMP_MAX_TASK	3000
#define SPRD_DUMP_TASK_SIZE	(160 * (SPRD_DUMP_MAX_TASK + 2))
#define SPRD_DUMP_STACK_SIZE	(2048 * SPRD_NR_CPUS)
#define SPRD_DUMP_MEM_SIZE	12288 /* 12k */
#define SPRD_DUMP_IRQ_SIZE	12288
#define MAX_CALLBACK_LEVEL	16
#define MAX_NAME_LEN		16

#define SEQ_printf(m, x...)			\
do {						\
	if (m)					\
		seq_buf_printf(m, x);		\
	else					\
		pr_debug(x);			\
} while (0)

#ifdef CONFIG_ARM64
#define sprd_virt_addr_valid(kaddr) ((((void *)(kaddr) >= (void *)PAGE_OFFSET && \
					(void *)(kaddr) < (void *)high_memory) || \
					((void *)(kaddr) >= (void *)KIMAGE_VADDR && \
					(void *)(kaddr) < (void *)_end)) && \
					pfn_valid(__pa(kaddr) >> PAGE_SHIFT))
#else
#define sprd_virt_addr_valid(kaddr) ((void *)(kaddr) >= (void *)PAGE_OFFSET && \
					(void *)(kaddr) < (void *)high_memory && \
					pfn_valid(__pa(kaddr) >> PAGE_SHIFT))
#endif

static char *sprd_task_buf;
static struct seq_buf *sprd_task_seq_buf;
static char *sprd_runq_buf;
static struct seq_buf *sprd_rq_seq_buf;
static char *sprd_stack_reg_buf;
static struct seq_buf *sprd_sr_seq_buf;
static char *sprd_meminfo_buf;
struct seq_buf *sprd_mem_seq_buf;
static char *sprd_irqstat_buf;
struct seq_buf *sprd_irqstat_seq_buf;

static DEFINE_RAW_SPINLOCK(dump_lock);

static int minidump_add_section(const char *name, long vaddr, int size)
{
	int ret;

	pr_info("%s in. vaddr : 0x%lx  len :0x%x\n", __func__, vaddr, size);
	ret = minidump_save_extend_information(name, __pa(vaddr),
						__pa(vaddr + size));
	if (!ret)
		pr_info("%s added to minidump section ok!!\n", name);

	return ret;
}

#if defined(CONFIG_ARM64)
static void minidump_add_irq_stack(void)
{
	int cpu;
	u64 irq_stack_base, sp;
	char name[MAX_NAME_LEN];
#ifdef CONFIG_VMAP_STACK
	int page_count;
	unsigned int i;
#endif

	for_each_possible_cpu(cpu) {
		irq_stack_base = (u64)per_cpu(irq_stack_ptr, cpu);
		if (!irq_stack_base)
			return;
#ifdef CONFIG_VMAP_STACK
		page_count = IRQ_STACK_SIZE / PAGE_SIZE;
		sp = irq_stack_base & ~(PAGE_SIZE - 1);
		for (i = 0; i < page_count; i++) {
			struct page *sp_page;
			unsigned long phys_addr;

			scnprintf(name, MAX_NAME_LEN, "irqstack%d_%d", cpu, i);
			sp_page = vmalloc_to_page((const void *) sp);
			phys_addr = page_to_phys(sp_page);
			if (minidump_save_extend_information(name, phys_addr, phys_addr + PAGE_SIZE))
				return;
			sp += PAGE_SIZE;
		}
#else
		sp = irq_stack_base;
		scnprintf(name, MAX_NAME_LEN, "irqstack%d", cpu);
		minidump_add_section(name, sp, IRQ_STACK_SIZE);
#endif
	}
}
#else
static inline void minidump_add_irq_stack(void) {}
#endif

static int currstack_inited;
static void minidump_add_current_stack(void)
{
	char name[MAX_NAME_LEN];
	int cpu;
#ifdef CONFIG_VMAP_STACK
	int i, page_count;

	page_count = THREAD_SIZE / PAGE_SIZE;
	for_each_possible_cpu(cpu)
		for (i = 0; i < page_count; i++) {
			scnprintf(name, MAX_NAME_LEN, "cpustack%d_%d", cpu, i);
			if (minidump_save_extend_information(name, 0, PAGE_SIZE))
				return;
		}
#else
	for_each_possible_cpu(cpu) {
		scnprintf(name, MAX_NAME_LEN, "cpustack%d", cpu);
		minidump_save_extend_information(name, 0, THREAD_SIZE);
	}
#endif
	currstack_inited = 1;
}
void minidump_update_current_stack(int cpu, struct pt_regs *regs)
{
	unsigned long sp;
	char name[MAX_NAME_LEN];
#ifdef CONFIG_VMAP_STACK
	struct vm_struct *stack_vm_area;
	int i, page_count;

	if (!currstack_inited || user_mode(regs) || is_idle_task(current))
		return;

	stack_vm_area = task_stack_vm_area(current);
	sp = (unsigned long)stack_vm_area->addr;

	sp &= ~(PAGE_SIZE - 1);
	page_count = THREAD_SIZE / PAGE_SIZE;
	for (i = 0; i < page_count; i++) {
		struct page *sp_page;
		unsigned long phys_addr;

		scnprintf(name, MAX_NAME_LEN, "cpustack%d_%d", cpu, i);
		sp_page = vmalloc_to_page((const void *) sp);
		phys_addr = page_to_phys(sp_page);
		if (minidump_change_extend_information(name, phys_addr, phys_addr + PAGE_SIZE))
			return;
		sp += PAGE_SIZE;
	}
#else
	if (!currstack_inited || user_mode(regs) || is_idle_task(current))
		return;

	sp = (unsigned long)current->stack;
	scnprintf(name, MAX_NAME_LEN, "cpustack%d", cpu);
	minidump_change_extend_information(name, __pa(sp), __pa(sp + THREAD_SIZE));
#endif
}
/*
 * Ease the printing of nsec fields:
 */
static long long nsec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000000);
		return -nsec;
	}
	do_div(nsec, 1000000000);

	return nsec;
}

static unsigned long nsec_low(unsigned long long nsec)
{
	if ((long long)nsec < 0)
		nsec = -nsec;

	return do_div(nsec, 1000000000);
}

static int align_offset;

static void dump_align(void)
{
	int tab_offset = align_offset;

	while (tab_offset--)
		SEQ_printf(sprd_rq_seq_buf, " | ");
	SEQ_printf(sprd_rq_seq_buf, " |--");
}

static void dump_task_info(struct task_struct *task, char *status,
			      struct task_struct *curr)
{
	struct sched_entity *se;

	dump_align();
	if (!task) {
		SEQ_printf(sprd_rq_seq_buf, "%s : None(0)\n", status);
		return;
	}

	se = &task->se;
	if (task == curr) {
		SEQ_printf(sprd_rq_seq_buf, "[status: curr] pid: %d comm: %s preempt: %#llx\n",
			task_pid_nr(task), task->comm,
			task_thread_info(task)->preempt_count);
		return;
	}

	SEQ_printf(sprd_rq_seq_buf, "[status: %s] pid: %d tsk: %#lx comm: %s stack: %#lx",
		status, task_pid_nr(task),
		(unsigned long)task,
		task->comm,
		(unsigned long)task->stack);
	SEQ_printf(sprd_rq_seq_buf, " prio: %d aff: %*pb",
		       task->prio, cpumask_pr_args(&task->cpus_mask));
	SEQ_printf(sprd_rq_seq_buf, " enqueue: %llu", task->last_enqueue_ts);
	SEQ_printf(sprd_rq_seq_buf, " last_sleep: %llu", task->last_sleep_ts);
	SEQ_printf(sprd_rq_seq_buf, " vrun: %llu exec_start: %llu sum_ex: %llu\n",
		se->vruntime, se->exec_start, se->sum_exec_runtime);
}

static void dump_cfs_rq(struct cfs_rq *cfs, struct task_struct *curr);

static void dump_cgroup_state(char *status, struct sched_entity *se_p,
				   struct task_struct *curr)
{
	struct task_struct *task;
	struct cfs_rq *my_q = NULL;
	unsigned int nr_running;

	if (!se_p) {
		dump_task_info(NULL, status, NULL);
		return;
	}
#ifdef CONFIG_FAIR_GROUP_SCHED
	my_q = se_p->my_q;
#endif
	if (!my_q) {
		task = container_of(se_p, struct task_struct, se);
		dump_task_info(task, status, curr);
		return;
	}
	nr_running = my_q->nr_running;
	dump_align();
	SEQ_printf(sprd_rq_seq_buf, "%s: %d process is grouping\n", status, nr_running);
	align_offset++;
	dump_cfs_rq(my_q, curr);
	align_offset--;
}

static void dump_cfs_node_func(struct rb_node *node,
				    struct task_struct *curr)
{
	struct sched_entity *se_p = container_of(node, struct sched_entity,
						 run_node);

	dump_cgroup_state("pend", se_p, curr);
}

static void rb_walk_cfs(struct rb_root_cached *rb_root_cached_p,
			     struct task_struct *curr)
{
	int max_walk = 100;	/* Bail out, in case of loop */
	struct rb_node *leftmost = rb_root_cached_p->rb_leftmost;
	struct rb_root *root = &rb_root_cached_p->rb_root;
	struct rb_node *rb_node = rb_first(root);

	if (!leftmost)
		return;
	while (rb_node && max_walk--) {
		dump_cfs_node_func(rb_node, curr);
		rb_node = rb_next(rb_node);
	}
}

static void dump_cfs_rq(struct cfs_rq *cfs, struct task_struct *curr)
{
	struct rb_root_cached *rb_root_cached_p = &cfs->tasks_timeline;

	dump_cgroup_state("curr", cfs->curr, curr);
	dump_cgroup_state("next", cfs->next, curr);
	dump_cgroup_state("last", cfs->last, curr);
	dump_cgroup_state("skip", cfs->skip, curr);
	rb_walk_cfs(rb_root_cached_p, curr);
}

static void dump_rt_rq(struct rt_rq  *rt_rq, struct task_struct *curr)
{
	struct rt_prio_array *array = &rt_rq->active;
	struct sched_rt_entity *rt_se;
	int idx;

	/* Lifted most of the below code from dump_throttled_rt_tasks() */
	if (bitmap_empty(array->bitmap, MAX_RT_PRIO))
		return;

	idx = sched_find_first_bit(array->bitmap);
	while (idx < MAX_RT_PRIO) {
		list_for_each_entry(rt_se, array->queue + idx, run_list) {
			struct task_struct *p;

#ifdef CONFIG_RT_GROUP_SCHED
			if (rt_se->my_q)
				continue;
#endif

			p = container_of(rt_se, struct task_struct, rt);
			dump_task_info(p, "pend", curr);
		}
		idx = find_next_bit(array->bitmap, MAX_RT_PRIO, idx + 1);
	}
}

void sprd_dump_runqueues(void)
{
	int cpu;
	struct rq *rq;
	struct rt_rq  *rt;
	struct cfs_rq *cfs;

	if (!sprd_runq_buf || !sprd_rq_seq_buf)
		return;

	for_each_possible_cpu(cpu) {
		rq = cpu_rq(cpu);
		rt = &rq->rt;
		cfs = &rq->cfs;
		SEQ_printf(sprd_rq_seq_buf, "CPU%d %d process is running\n",
					cpu, rq->nr_running);
		dump_task_info(cpu_curr(cpu), "curr", NULL);
		SEQ_printf(sprd_rq_seq_buf, " CFS %d process is pending\n", cfs->nr_running);
		dump_cfs_rq(cfs, cpu_curr(cpu));
		SEQ_printf(sprd_rq_seq_buf, " RT %d process is pending\n", rt->rt_nr_running);
		dump_rt_rq(rt, cpu_curr(cpu));
		SEQ_printf(sprd_rq_seq_buf, "\n");
	}
}

static void sprd_print_task_stats(int cpu, struct rq *rq, struct task_struct *p)
{
	struct seq_buf *task_seq_buf;

	task_seq_buf = sprd_task_seq_buf;

	SEQ_printf(task_seq_buf, "  %d ", cpu);
	if (rq->curr == p)
		SEQ_printf(task_seq_buf, ">R");
	else
		SEQ_printf(task_seq_buf, " %c", task_state_to_char(p));

	SEQ_printf(task_seq_buf, " %15s %5d %5d %13lld  ",
				p->comm, task_pid_nr(p), p->prio,
				(long long)(p->nvcsw + p->nivcsw));

#ifdef CONFIG_SCHED_INFO
	SEQ_printf(task_seq_buf, "%6lld.%09ld  %6lld.%09ld  %6lld.%09ld",
				nsec_high(p->sched_info.last_arrival),
				nsec_low(p->sched_info.last_arrival),
				nsec_high(p->sched_info.last_queued),
				nsec_low(p->sched_info.last_queued),
				nsec_high(p->sched_info.run_delay),
				nsec_low(p->sched_info.run_delay));
#endif
	SEQ_printf(task_seq_buf, "   %6lld.%09ld",
				nsec_high(p->se.sum_exec_runtime),
				nsec_low(p->se.sum_exec_runtime));
	SEQ_printf(task_seq_buf, "   %6lld.%09ld",
				nsec_high(p->last_enqueue_ts),
				nsec_low(p->last_enqueue_ts));
	SEQ_printf(task_seq_buf, "   %6lld.%09ld",
				nsec_high(p->last_sleep_ts),
				nsec_low(p->last_sleep_ts));

	SEQ_printf(task_seq_buf, "\n");
}

void sprd_dump_task_stats(void)
{
	struct task_struct *g, *p;
	int cpu;
	struct rq *rq;

	if (!sprd_task_buf || !sprd_task_seq_buf)
		return;

	SEQ_printf(sprd_task_seq_buf, "cpu  S       task_comm   PID  prio   num_of_exec");
#ifdef CONFIG_SCHED_INFO
	SEQ_printf(sprd_task_seq_buf, "   last_arrival_ts    last_queued_ts   total_wait_time ");
#endif
	SEQ_printf(sprd_task_seq_buf, "   total_exec_time");
	SEQ_printf(sprd_task_seq_buf, "    last_enqueue_ts");
	SEQ_printf(sprd_task_seq_buf, "      last_sleep_ts");
	SEQ_printf(sprd_task_seq_buf, "\n-------------------------------------------------------------------"
		"-------------------------------------------------------------------------------------------\n");

	for_each_process_thread(g, p) {
		cpu = task_cpu(p);
		rq = cpu_rq(cpu);

		sprd_print_task_stats(cpu, rq, p);
	}
}

#ifdef CONFIG_ARM64
static void sprd_dump_regs(struct pt_regs *regs)
{
	int i, top_reg;
	u64 lr, sp;

	if (compat_user_mode(regs)) {
		lr = regs->compat_lr;
		sp = regs->compat_sp;
		top_reg = 12;
	} else {
		lr = regs->regs[30];
		sp = regs->sp;
		top_reg = 29;
	}
	if (!user_mode(regs)) {
		SEQ_printf(sprd_sr_seq_buf, "pc : %pS\n", (void *)regs->pc);
		SEQ_printf(sprd_sr_seq_buf, "lr : %pS\n", (void *)lr);
	} else {
		SEQ_printf(sprd_sr_seq_buf, "pc : %016llx\n", regs->pc);
		SEQ_printf(sprd_sr_seq_buf, "lr : %016llx\n", lr);
	}
	SEQ_printf(sprd_sr_seq_buf, "sp : %016llx pstate : %08llx\n", sp, regs->pstate);

	if (system_uses_irq_prio_masking())
		SEQ_printf(sprd_sr_seq_buf, "pmr_save: %08llx\n", regs->pmr_save);

	i = top_reg;
	while (i >= 0) {
		SEQ_printf(sprd_sr_seq_buf, "x%-2d: %016llx ", i, regs->regs[i]);
		i--;

		if (i % 2 == 0) {
			SEQ_printf(sprd_sr_seq_buf, "x%-2d: %016llx ", i, regs->regs[i]);
			i--;
		}

		SEQ_printf(sprd_sr_seq_buf, "\n");
	}
	SEQ_printf(sprd_sr_seq_buf, "\n");
}
#else
static const char *processor_modes[] __maybe_unused = {
"USER_26", "FIQ_26", "IRQ_26",  "SVC_26",  "UK4_26",  "UK5_26",  "UK6_26",  "UK7_26",
"UK8_26",  "UK9_26", "UK10_26", "UK11_26", "UK12_26", "UK13_26", "UK14_26", "UK15_26",
"USER_32", "FIQ_32", "IRQ_32",  "SVC_32",  "UK4_32",  "UK5_32",  "MON_32",  "ABT_32",
"UK8_32",  "UK9_32", "HYP_32",  "UND_32",  "UK12_32", "UK13_32", "UK14_32", "SYS_32"
};

static const char *isa_modes[] __maybe_unused = {
"ARM", "Thumb", "Jazelle", "ThumbEE"
};

static void sprd_dump_regs(struct pt_regs *regs)
{
	unsigned long flags;
	char buf[64];
#ifndef CONFIG_CPU_V7M
	unsigned int domain, fs;
#ifdef CONFIG_CPU_SW_DOMAIN_PAN
	/*
	 * Get the domain register for the parent context. In user
	 * mode, we don't save the DACR, so lets use what it should
	 * be. For other modes, we place it after the pt_regs struct.
	 */
	if (user_mode(regs)) {
		domain = DACR_UACCESS_ENABLE;
		fs = get_fs();
	} else {
		domain = to_svc_pt_regs(regs)->dacr;
		fs = to_svc_pt_regs(regs)->addr_limit;
	}
#else
	domain = get_domain();
	fs = get_fs();
#endif
#endif
	SEQ_printf(sprd_sr_seq_buf, "pc : [<%08lx>]    lr : [<%08lx>]    psr: %08lx\n",
	       regs->ARM_pc, regs->ARM_lr, regs->ARM_cpsr);
	SEQ_printf(sprd_sr_seq_buf, "sp : %08lx  ip : %08lx  fp : %08lx\n",
	       regs->ARM_sp, regs->ARM_ip, regs->ARM_fp);
	SEQ_printf(sprd_sr_seq_buf, "r10: %08lx  r9 : %08lx  r8 : %08lx\n",
		regs->ARM_r10, regs->ARM_r9,
		regs->ARM_r8);
	SEQ_printf(sprd_sr_seq_buf, "r7 : %08lx  r6 : %08lx  r5 : %08lx  r4 : %08lx\n",
		regs->ARM_r7, regs->ARM_r6,
		regs->ARM_r5, regs->ARM_r4);
	SEQ_printf(sprd_sr_seq_buf, "r3 : %08lx  r2 : %08lx  r1 : %08lx  r0 : %08lx\n",
		regs->ARM_r3, regs->ARM_r2,
		regs->ARM_r1, regs->ARM_r0);

	flags = regs->ARM_cpsr;
	buf[0] = flags & PSR_N_BIT ? 'N' : 'n';
	buf[1] = flags & PSR_Z_BIT ? 'Z' : 'z';
	buf[2] = flags & PSR_C_BIT ? 'C' : 'c';
	buf[3] = flags & PSR_V_BIT ? 'V' : 'v';
	buf[4] = '\0';

#ifndef CONFIG_CPU_V7M
	{
		const char *segment;

		if ((domain & domain_mask(DOMAIN_USER)) ==
		    domain_val(DOMAIN_USER, DOMAIN_NOACCESS))
			segment = "none";
		else if (fs == KERNEL_DS)
			segment = "kernel";
		else
			segment = "user";

		SEQ_printf(sprd_sr_seq_buf, "Flags: %s  IRQs o%s  FIQs o%s  Mode %s  ISA %s  Segment %s\n",
			buf, interrupts_enabled(regs) ? "n" : "ff",
			fast_interrupts_enabled(regs) ? "n" : "ff",
			processor_modes[processor_mode(regs)],
			isa_modes[isa_mode(regs)], segment);
	}
#else
	SEQ_printf(sprd_sr_seq_buf, "xPSR: %08lx\n", regs->ARM_cpsr);
#endif

#ifdef CONFIG_CPU_CP15
	{
		unsigned int ctrl;

		buf[0] = '\0';
#ifdef CONFIG_CPU_CP15_MMU
		{
			unsigned int transbase;

			asm("mrc p15, 0, %0, c2, c0\n\t"
			    : "=r" (transbase));
			snprintf(buf, sizeof(buf), "  Table: %08x  DAC: %08x",
				transbase, domain);
		}
#endif
		asm("mrc p15, 0, %0, c1, c0\n" : "=r" (ctrl));

		SEQ_printf(sprd_sr_seq_buf, "Control: %08x%s\n", ctrl, buf);
	}
#endif
	SEQ_printf(sprd_sr_seq_buf, "\n");
}
#endif

void sprd_dump_stack_reg(int cpu, struct pt_regs *pregs)
{
	int i;
	struct stackframe frame;
	unsigned long sp;

	raw_spin_lock(&dump_lock);
	if (!sprd_stack_reg_buf || !sprd_sr_seq_buf)
		goto unlock;

	SEQ_printf(sprd_sr_seq_buf, "-----cpu%d stack info-----\n", cpu);

#ifdef CONFIG_ARM64
	frame.fp = pregs->regs[29];
	frame.pc = pregs->pc;
	sp = pregs->sp;
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	frame.graph = 0;
#endif
	bitmap_zero(frame.stacks_done, __NR_STACK_TYPES);
	frame.prev_fp = 0;
	frame.prev_type = STACK_TYPE_UNKNOWN;
#else
	frame.fp = pregs->ARM_fp;
	frame.sp = pregs->ARM_sp;
	frame.lr = pregs->ARM_lr;
	frame.pc = pregs->ARM_pc;
	sp = pregs->ARM_sp;
#endif

#ifdef CONFIG_VMAP_STACK
	if (!((sp >= VMALLOC_START) && (sp < VMALLOC_END))) {
		SEQ_printf(sprd_sr_seq_buf, "%s sp out of kernel addr space %08lx\n", __func__, sp);
		goto unlock;
	}
#else
	if (!sprd_virt_addr_valid(sp)) {
		SEQ_printf(sprd_sr_seq_buf, "invalid sp[%lx]\n", sp);
		goto unlock;
	}
#endif

	if (!sprd_virt_addr_valid(frame.pc)) {
		SEQ_printf(sprd_sr_seq_buf, "invalid pc\n");
		goto unlock;
	}
	SEQ_printf(sprd_sr_seq_buf, "callstack:\n");
	SEQ_printf(sprd_sr_seq_buf, "[<%08lx>] (%pS)\n", frame.pc, (void *)frame.pc);

	for (i = 0; i < MAX_CALLBACK_LEVEL; i++) {
		int urc;

#ifdef CONFIG_ARM64
		urc = unwind_frame(NULL, &frame);
#else
		urc = unwind_frame(&frame);
#endif
		if (urc < 0)
			break;

		if (!sprd_virt_addr_valid(frame.pc)) {
			SEQ_printf(sprd_sr_seq_buf, "i=%d, sprd_virt_addr_valid fail\n", i);
			break;
		}

		SEQ_printf(sprd_sr_seq_buf, "[<%08lx>] (%pS)\n", frame.pc, (void *)frame.pc);
	}

	SEQ_printf(sprd_sr_seq_buf, "\n-----cpu%d regs info-----\n", cpu);
	sprd_dump_regs(pregs);
unlock:
	raw_spin_unlock(&dump_lock);
}

static int sprd_add_task_stats(void)
{
	int ret = 0;

	sprd_task_buf = kzalloc(SPRD_DUMP_TASK_SIZE, GFP_KERNEL);
	if (!sprd_task_buf)
		return -ENOMEM;

	sprd_task_seq_buf = kzalloc(sizeof(*sprd_task_seq_buf), GFP_KERNEL);
	if (!sprd_task_seq_buf) {
		ret = -ENOMEM;
		goto err_task_seq;
	}

	if (minidump_add_section("task_stats", (unsigned long)(sprd_task_buf), SPRD_DUMP_TASK_SIZE)) {
		ret = -EINVAL;
		goto err_task_save;
	}

	seq_buf_init(sprd_task_seq_buf, sprd_task_buf, SPRD_DUMP_TASK_SIZE);

	return 0;

err_task_save:
	kfree(sprd_task_seq_buf);
err_task_seq:
	kfree(sprd_task_buf);

	return ret;
}

static void sprd_free_task_stats(void)
{
	kfree(sprd_task_buf);
	kfree(sprd_task_seq_buf);
}

static int sprd_add_runq_stats(void)
{
	int ret = 0;

	sprd_runq_buf = kzalloc(SPRD_DUMP_RQ_SIZE, GFP_KERNEL);
	if (!sprd_runq_buf)
		return -ENOMEM;

	sprd_rq_seq_buf = kzalloc(sizeof(*sprd_rq_seq_buf), GFP_KERNEL);
	if (!sprd_rq_seq_buf) {
		ret = -ENOMEM;
		goto err_rq_seq;
	}

	if (minidump_add_section("runqueue", (unsigned long)(sprd_runq_buf), SPRD_DUMP_RQ_SIZE)) {
		ret = -EINVAL;
		goto err_rq_save;
	}

	seq_buf_init(sprd_rq_seq_buf, sprd_runq_buf, SPRD_DUMP_RQ_SIZE);

	return 0;

err_rq_save:
	kfree(sprd_rq_seq_buf);
err_rq_seq:
	kfree(sprd_runq_buf);

	return ret;
}

static void sprd_free_runq_stats(void)
{
	kfree(sprd_runq_buf);
	kfree(sprd_rq_seq_buf);
}

static int sprd_add_stack_regs_stats(void)
{
	int ret = 0;

	sprd_stack_reg_buf = kzalloc(SPRD_DUMP_STACK_SIZE, GFP_KERNEL);
	if (!sprd_stack_reg_buf)
		return -ENOMEM;

	sprd_sr_seq_buf = kzalloc(sizeof(*sprd_sr_seq_buf), GFP_KERNEL);
	if (!sprd_sr_seq_buf) {
		ret = -ENOMEM;
		goto err_sr_seq;
	}

	if (minidump_add_section("stack_regs", (unsigned long)(sprd_stack_reg_buf), SPRD_DUMP_STACK_SIZE)) {
		ret = -EINVAL;
		goto err_sr_save;
	}

	seq_buf_init(sprd_sr_seq_buf, sprd_stack_reg_buf, SPRD_DUMP_STACK_SIZE);

	return 0;

err_sr_save:
	kfree(sprd_sr_seq_buf);
err_sr_seq:
	kfree(sprd_stack_reg_buf);

	return ret;
}

static void sprd_free_stack_regs_stats(void)
{
	kfree(sprd_stack_reg_buf);
	kfree(sprd_sr_seq_buf);
}

static int sprd_add_memory_stat(void)
{
	int ret = 0;

	sprd_meminfo_buf = kzalloc(SPRD_DUMP_MEM_SIZE, GFP_KERNEL);
	if (!sprd_meminfo_buf)
		return -ENOMEM;

	sprd_mem_seq_buf = kzalloc(sizeof(*sprd_mem_seq_buf), GFP_KERNEL);
	if (!sprd_mem_seq_buf) {
		ret = -ENOMEM;
		goto err_mem_seq;
	}
	if (minidump_add_section("memstat", (unsigned long)(sprd_meminfo_buf), SPRD_DUMP_MEM_SIZE)) {
		ret = -EINVAL;
		goto err_save;
	}

	seq_buf_init(sprd_mem_seq_buf, sprd_meminfo_buf, SPRD_DUMP_MEM_SIZE);

	return 0;

err_save:
	kfree(sprd_mem_seq_buf);
err_mem_seq:
	kfree(sprd_meminfo_buf);

	return ret;
}
static void sprd_free_memory_stat(void)
{
	kfree(sprd_meminfo_buf);
	kfree(sprd_mem_seq_buf);
}

void sprd_dump_mem_stat(void)
{
	if (!sprd_mem_seq_buf || !sprd_meminfo_buf)
		return;
	SEQ_printf(sprd_mem_seq_buf, "-----MemInfo-----\n\n");
	sprd_dump_meminfo();
	SEQ_printf(sprd_mem_seq_buf, "\n\n-----Vmstat-----\n\n");
	sprd_dump_vmstat();
	SEQ_printf(sprd_mem_seq_buf, "\n\n-----BuddyInfo-----\n\n");
	sprd_dump_buddyinfo();
	SEQ_printf(sprd_mem_seq_buf, "\n\n-----ZoneInfo-----\n\n");
	sprd_dump_zoneinfo();
	SEQ_printf(sprd_mem_seq_buf, "\n\n-----PagetypeInfo-----\n\n");
	sprd_dump_pagetypeinfo();
	flush_cache_all();
}

static int sprd_add_interrupt_stat(void)
{
	int ret = 0;

	sprd_irqstat_buf = kzalloc(SPRD_DUMP_IRQ_SIZE, GFP_KERNEL);
	if (!sprd_irqstat_buf)
		return -ENOMEM;

	sprd_irqstat_seq_buf = kzalloc(sizeof(*sprd_irqstat_seq_buf), GFP_KERNEL);
	if (!sprd_irqstat_seq_buf) {
		ret = -ENOMEM;
		goto err_irqstat_seq;
	}

	if (minidump_add_section("interruptes", (unsigned long)(sprd_irqstat_buf), SPRD_DUMP_IRQ_SIZE)) {
		ret = -EINVAL;
		goto err_save;
	}

	seq_buf_init(sprd_irqstat_seq_buf, sprd_irqstat_buf, SPRD_DUMP_IRQ_SIZE);

	return 0;

err_save:
	kfree(sprd_irqstat_seq_buf);
err_irqstat_seq:
	kfree(sprd_irqstat_buf);

	return ret;
}
static void sprd_free_interrupt_stat(void)
{
	kfree(sprd_irqstat_buf);
	kfree(sprd_irqstat_seq_buf);
}

static int sprd_kmsg_panic_event(struct notifier_block *self,
				  unsigned long val, void *reason)
{
	sprd_dump_runqueues();
	sprd_dump_task_stats();
	sprd_dump_mem_stat();
	sprd_dump_interrupts();

	return NOTIFY_DONE;
}

static struct notifier_block sprd_kmsg_panic_event_nb = {
	.notifier_call	= sprd_kmsg_panic_event,
	.priority	= INT_MAX - 1,
};

static int __init sprd_dump_msg_init(void)
{

	sprd_add_task_stats();
	sprd_add_runq_stats();
	sprd_add_stack_regs_stats();
	sprd_add_memory_stat();
	sprd_add_interrupt_stat();
	minidump_add_irq_stack();
	minidump_add_current_stack();

	/* register sched panic notifier */
	atomic_notifier_chain_register(&panic_notifier_list,
					&sprd_kmsg_panic_event_nb);

	pr_info("sprd_kmsg_panic_notifier_register success!\n");

	return 0;

}

static void __exit sprd_dump_msg_exit(void)
{
	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &sprd_kmsg_panic_event_nb);
	sprd_free_task_stats();
	sprd_free_runq_stats();
	sprd_free_stack_regs_stats();
	sprd_free_memory_stat();
	sprd_free_interrupt_stat();
}

late_initcall_sync(sprd_dump_msg_init);
module_exit(sprd_dump_msg_exit);

MODULE_DESCRIPTION("kernel kmsg stats for Unisoc");
MODULE_LICENSE("GPL");
