/* SPDX-License-Identifier: GPL-2.0 */

/**
 * save extend debug information of modules in minidump, such as: cm4, iram...
 *
 * @name:	the name of the modules, and the string will be a part
 *		of the file name.
 *		note: special characters can't be included in the file name,
 *		such as:'?','*','/','\','<','>',':','"','|'.
 *
 * @paddr_start:the start paddr in memory of the modules debug information
 * @paddr_end:	the end paddr in memory of the modules debug information
 *
 * Return: 0 means success, -1 means fail.
 */
extern int minidump_save_extend_information(const char *name,
						unsigned long paddr_start,
						unsigned long paddr_end);

extern int minidump_change_extend_information(const char *name,
		unsigned long paddr_start,
		unsigned long paddr_end);

#ifdef CONFIG_SPRD_SYSDUMP
/*
 * save per-cpu's stack and regs in sysdump.
 *
 * @cpu:	the cpu number;
 *
 * @pregs:	pt_regs.
 */
extern void sprd_dump_stack_reg(int cpu, struct pt_regs *pregs);
extern void sprd_dump_task_stats(void);
extern void sprd_dump_runqueues(void);
extern void sprd_dump_io(void);
/*
 * save meminfo in sysdump.
 */
extern void sprd_dump_mem_stat(void);
extern void sprd_dump_meminfo(void);
extern void sprd_dump_vmstat(void);
extern void sprd_dump_buddyinfo(void);
extern void sprd_dump_zoneinfo(void);
extern void sprd_dump_pagetypeinfo(void);
extern struct seq_buf *sprd_mem_seq_buf;
/*
 * save proc/interrupts in sysdump.
 */
extern void sprd_dump_interrupts(void);
extern struct seq_buf *sprd_irqstat_seq_buf;
/*
 * update current task's stack's phy addr in sysdump.
 */
extern void minidump_update_current_stack(int cpu, struct pt_regs *regs);
#else
static inline void sprd_dump_stack_reg(int cpu, struct pt_regs *pregs) {}
static inline void sprd_dump_task_stats(void) {}
static inline void sprd_dump_runqueues(void) {}
static inline void sprd_dump_mem_stat(void) {}
static inline void sprd_dump_meminfo(void) {}
static inline void sprd_dump_vmstat(void) {}
static inline void sprd_dump_buddyinfo(void) {}
static inline void sprd_dump_zoneinfo(void) {}
static inline void sprd_dump_pagetypeinfo(void) {}
static inline void sprd_dump_interrupts(void) {}
static inline void sprd_dump_io(void) {}
static inline void minidump_update_current_stack(int cpu, struct pt_regs *regs) {}
#endif

