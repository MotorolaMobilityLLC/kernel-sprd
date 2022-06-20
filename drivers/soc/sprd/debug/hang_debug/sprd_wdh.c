// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * it's hardware watchdog interrupt handler code for Phoenix II
 */

#include <linux/init.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/percpu.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/smp.h>
#include <linux/delay.h>
#include <linux/smpboot.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/stacktrace.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/reboot.h>
#include <linux/irqchip/arm-gic-v3.h>
#include <asm/stacktrace.h>
#include <asm/io.h>
#include <asm/traps.h>
#include <linux/module.h>
#include <linux/soc/sprd/sprd_sysdump.h>
#include <linux/sprd_sip_svc.h>
#if IS_ENABLED(CONFIG_ARM)
#include <asm/cacheflush.h>
#else
#include "../sysdump/sysdump64.h"
#endif

#undef pr_fmt
#define pr_fmt(fmt) "sprd_wdh: " fmt

#define MAX_CALLBACK_LEVEL  16
#define SPRD_PRINT_BUF_LEN  32768
#define SPRD_STACK_SIZE 256
#define SPRD_NS_EL0 (0x0)
#define SPRD_NS_EL1 (0x1)
#define SPRD_NS_EL2 (0x2)
#define SPRD_CPUS 8

enum hand_dump_phase {
	SPRD_HANG_DUMP_ENTER = 1,
	SPRD_HANG_DUMP_CPU_REGS,
	SPRD_HANG_DUMP_CALL_STACK,
	SPRD_HANG_DUMP_GICC_REGS,
	SPRD_HANG_DUMP_SYSDUMP,
	SPRD_HANG_DUMP_GICD_REGS,
	SPRD_HANG_DUMP_INFO,
	SPRD_HANG_DUMP_END,
};

static int wdh_step[NR_CPUS];
static unsigned char wdh_cpu_state[NR_CPUS];
static raw_spinlock_t sprd_wdh_prlock;
static raw_spinlock_t sprd_wdh_wclock;
static atomic_t sprd_enter_wdh;
extern void sysdump_ipi(struct pt_regs *regs);
extern unsigned long gic_get_gicd_base(void);

static char *sprd_log_buf[SPRD_CPUS];
static int sprd_log_buf_pos[SPRD_CPUS];
static struct gicd_data *gicd_regs;
static struct gicc_data *gicc_regs;
struct cpumask *sprd_cpu_feed_mask;
struct cpumask *sprd_cpu_feed_bitmap;

#if IS_ENABLED(CONFIG_SPRD_HANG_WDF)
extern unsigned int cpu_feed_mask;
extern unsigned int cpu_feed_bitmap;
#else
static unsigned int cpu_feed_mask;
static unsigned int cpu_feed_bitmap;
#endif

struct gicc_data {
	u32 gicc_ctlr;	//nonsecure
	u32 gicc_pmr;	//nonsecure
	u32 gicc_bpr;	//nonsecure
	u32 gicc_rpr;	//nonsecure
	u32 gicc_hppir; //nonsecure
	u32 gicc_abpr;	//nonsecure
	u32 gicc_aiar;	//nonsecure
	u32 gicc_ahppir;	//nonsecure
	u32 gicc_statusr;	//nonsecure
	u32 gicc_iidr;	//nonsecure
};

struct gicd_data {
	u32 gicd_ctlr;     //nonsecure
	u32 gicd_typer;    //nonsecure
	u32 gicd_statusr;	//nonsecure
	u32 gicd_igroup[7];	//secure
	u32 gicd_isenabler[7];	//nonsecure
	u32 gicd_ispendr[7];	//nonsecure
	u32 gicd_igrpmodr[7];	//secure
};

enum smcc_regs_id {
	CPU_STATUS		= 0x00000000,
	CPU_PC,
	CPU_CPSR,
	CPU_SP_EL0,
	CPU_SP_EL1,
	GICD_IGROUP_0		= 0x00010000,
	GICD_IGROUP_1,
	GICD_IGROUP_2,
	GICD_IGROUP_3,
	GICD_IGROUP_4,
	GICD_IGROUP_5,
	GICD_IGROUP_6,
	GICD_IGRPMODR_0,
	GICD_IGRPMODR_1,
	GICD_IGRPMODR_2,
	GICD_IGRPMODR_3,
	GICD_IGRPMODR_4,
	GICD_IGRPMODR_5,
	GICD_IGRPMODR_6,
};

struct pt_regs cpu_context[NR_CPUS];
static struct sprd_sip_svc_handle *svc_handle;
#if IS_ENABLED(CONFIG_SPRD_HANG_DEBUG_UART)
extern void sprd_hangd_console_write(const char *s, unsigned int count);
#endif

#if IS_ENABLED(CONFIG_ARM64)
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

static void sprd_write_print_buf(bool console, char *source, int size, int cpu)
{
	unsigned char *log_buf;
	int log_buf_pos;

	if (!sprd_log_buf[cpu])
		goto out;

	log_buf = sprd_log_buf[cpu];
	log_buf_pos = sprd_log_buf_pos[cpu];

	if (log_buf_pos + size <= SPRD_PRINT_BUF_LEN) {
		memcpy(log_buf + log_buf_pos, source, size);
		sprd_log_buf_pos[cpu] = log_buf_pos + size;
	} else {
		memcpy(log_buf + log_buf_pos, source, (SPRD_PRINT_BUF_LEN - log_buf_pos));
		sprd_log_buf_pos[cpu] = log_buf_pos + size - SPRD_PRINT_BUF_LEN;
		memcpy(log_buf, source, log_buf_pos);
	}

out:
#if IS_ENABLED(CONFIG_SPRD_HANG_DEBUG_UART)
	if (console)
		sprd_hangd_console_write(source, size);
#endif
}

static char wdh_tmp_buf[256];
static void sprd_hang_debug_printf(bool console, const char *fmt, ...)
{
	int cpu = smp_processor_id();
	u64 boottime_us = ktime_get_boot_fast_ns();
	int size;
	u64 sec, usec;

	va_list args;

	raw_spin_lock(&sprd_wdh_prlock);

	do_div(boottime_us, NSEC_PER_USEC);

	sec = boottime_us;
	usec = do_div(sec, USEC_PER_SEC);

	/* add timestamp header */
	strcpy(wdh_tmp_buf, "[H ");
	sprintf(wdh_tmp_buf + strlen(wdh_tmp_buf), "%d", (int)sec);
	strcat(wdh_tmp_buf + strlen(wdh_tmp_buf), ".");
	sprintf(wdh_tmp_buf + strlen(wdh_tmp_buf), "%06d", (int)usec);

	/* add cpu num header */
	strcat(wdh_tmp_buf + strlen(wdh_tmp_buf), "] c");
	sprintf(wdh_tmp_buf + strlen(wdh_tmp_buf), "%d", cpu);
	strcat(wdh_tmp_buf + strlen(wdh_tmp_buf), " ");

	size = strlen(wdh_tmp_buf);

	va_start(args, fmt);
	size += vsprintf(wdh_tmp_buf + size, fmt, args);
	va_end(args);

	sprd_write_print_buf(console, wdh_tmp_buf, size, cpu);

	raw_spin_unlock(&sprd_wdh_prlock);
}

static int __nocfi set_panic_debug_entry(unsigned long func_addr,
					 unsigned long pgd,
					 unsigned long level)
{
#if IS_ENABLED(CONFIG_SPRD_SIP_FW)
	int ret;

	ret = svc_handle->dbg_ops.set_hang_hdl(func_addr, pgd, level);

	if (ret) {
		pr_err("failed to set panic debug entry\n");
		return -EPERM;
	}
#endif
	pr_emerg("func_addr = 0x%lx , pdg = 0x%lx level = 0x%lx\n", func_addr, pgd, level);
	return 0;
}

__weak void prepare_dump_info_for_wdh(struct pt_regs *regs, const char *reason)
{
	sprd_hang_debug_printf(true, "Not defined func %s, for %p, %s\n", __func__, regs, reason);
}

static unsigned long smcc_get_regs(enum smcc_regs_id id, int core_id)
{
	uintptr_t val = 0;
#if IS_ENABLED(CONFIG_SPRD_SIP_FW)
	int ret;

	ret = svc_handle->dbg_ops.get_hang_ctx(id, core_id, &val);
	if (ret) {
		sprd_hang_debug_printf(true, "%s: failed to get cpu statue\n", __func__);
		return EPERM;
	}
#endif
	return val;
}
static unsigned short smcc_get_cpu_state(int core_id)
{
	uintptr_t val = 0;
#if IS_ENABLED(CONFIG_SPRD_SIP_FW)
	int ret;

	ret = svc_handle->dbg_ops.get_hang_ctx(CPU_STATUS, core_id, &val);
	if (ret) {
		sprd_hang_debug_printf(true, "failed to get cpu statue\n");
		return EPERM;
	}
#endif
	return val;
}

static int is_el3_ret_last_cpu(int cpu, unsigned short n)
{
	int rc = 0, tmp = 0, rt = 0;
	int i;

	raw_spin_lock(&sprd_wdh_wclock);
	wdh_cpu_state[cpu] = 1;

	/* is all cpus come back? */
	for_each_possible_cpu(i) {
		if ((n >> (i * 2)) & 0x2)
			rc++;
		tmp += wdh_cpu_state[i];
	}
	raw_spin_unlock(&sprd_wdh_wclock);

	/* is atf checks cpus finished? */
	while (n != 0) {
		n &= (n-1);
		rt++;
	}

	return (rt >= num_online_cpus() ? 1 : 0) && (rc == tmp) ? 1 : 0;
}

static void print_step(int cpu)
{
	sprd_hang_debug_printf(true, "wdh_step = %d\n", wdh_step[cpu]);
}

static void show_data(unsigned long addr, int nbytes, const char *name)
{
	int	i, j;
	int	nlines;
	u32	*p;
	char str[sizeof(" 12345678") * 8 + 1];
	struct vm_struct *vaddr;

	/*
	 * don't attempt to dump non-kernel addresses or
	 * values that are probably just small negative numbers
	 */
#if IS_ENABLED(CONFIG_ARM64)
	if (addr < KIMAGE_VADDR || addr > -256UL)
		return;
#else
	if (addr < PAGE_OFFSET || addr > -256UL)
		return;
#endif
	if (addr > VMALLOC_START && addr < VMALLOC_END) {
		vaddr = find_vm_area_no_wait((const void *)addr);
		if (!vaddr || ((vaddr->flags & VM_IOREMAP) == VM_IOREMAP))
			return;
	}

	sprd_hang_debug_printf(false, "%s: %#lx:\n", name, addr + nbytes / 2);

	/*
	 * round address down to a 32 bit boundary
	 * and always dump a multiple of 32 bytes
	 */
	p = (u32 *)(addr & ~(sizeof(u32) - 1));
	nbytes += (addr & (sizeof(u32) - 1));
	nlines = (nbytes + 31) / 32;

	for (i = 0; i < nlines; i++) {
		/*
		 * just display low 16 bits of address to keep
		 * each line of the dump < 80 characters
		 */
		memset(str, ' ', sizeof(str));
		str[sizeof(str) - 1] = '\0';
		for (j = 0; j < 8; j++) {
			u32	data;
			if (probe_kernel_address(p, data)) {
				sprintf(str + j * 9, " ********");
			} else {
				sprintf(str + j * 9, " %08x", data);
			}
			++p;
		}
		sprd_hang_debug_printf(false, "%04lx:%s\n", (unsigned long)(p - 8) & 0xffff, str);
	}
}

static void show_extra_register_data(struct pt_regs *regs, int nbytes)
{
	mm_segment_t fs;
	unsigned int i;

	fs = get_fs();
	set_fs(KERNEL_DS);
#if IS_ENABLED(CONFIG_ARM64)
	show_data(regs->pc - nbytes, nbytes * 2, "PC");
	show_data(regs->regs[30] - nbytes, nbytes * 2, "LR");
	show_data(regs->sp - nbytes, nbytes * 2, "SP");
	for (i = 0; i < 30; i++) {
		char name[4];
		snprintf(name, sizeof(name), "X%u", i);
		show_data(regs->regs[i] - nbytes, nbytes * 2, name);
	}
#else
	show_data(regs->ARM_pc - nbytes, nbytes * 2, "PC");
	show_data(regs->ARM_lr - nbytes, nbytes * 2, "LR");
	show_data(regs->ARM_sp - nbytes, nbytes * 2, "SP");
	show_data(regs->ARM_ip - nbytes, nbytes * 2, "IP");
	show_data(regs->ARM_fp - nbytes, nbytes * 2, "FP");
	for (i = 0; i < 11; i++) {
		char name[4];
		snprintf(name, sizeof(name), "R%u", i);
		show_data(regs->uregs[i] - nbytes, nbytes * 2, name);
	}
#endif
	set_fs(fs);
}

static void cpu_regs_value_dump(int cpu)
{
	struct pt_regs *pregs = &cpu_context[cpu];

#if IS_ENABLED(CONFIG_ARM64)
	int i;

	sprd_hang_debug_printf(true, "pc : %016llx, lr : %016llx, pstate : %016llx, sp_el0 : %016llx, sp_el1 : %016llx\n",
				pregs->pc, pregs->regs[30], pregs->pstate,
				smcc_get_regs(CPU_SP_EL0, cpu), smcc_get_regs(CPU_SP_EL1, cpu));

	if (sprd_virt_addr_valid(pregs->pc))
		sprd_hang_debug_printf(true, "pc :(%ps)\n", (void *)pregs->pc);
	sprd_hang_debug_printf(true, "sp : %016llx\n", pregs->sp);

	for (i = 29; i >= 0; ) {
		sprd_hang_debug_printf(true, "x%-2d: %016llx x%-2d: %016llx\n", i, pregs->regs[i], i - 1,
			pregs->regs[i - 1]);
		i -= 2;
	}
#else
	sprd_hang_debug_printf(true, "pc  : %08lx, lr : %08lx, cpsr : %08lx, sp_usr : %08lx, sp_svc : %08lx\n",
				pregs->ARM_pc, pregs->ARM_lr, pregs->ARM_cpsr,
				smcc_get_regs(CPU_SP_EL0, cpu), smcc_get_regs(CPU_SP_EL1, cpu));
	if (sprd_virt_addr_valid(pregs->ARM_pc))
		sprd_hang_debug_printf(true, "pc :(%ps)\n", (void *)pregs->ARM_pc);
	sprd_hang_debug_printf(true, "sp  : %08lx, ip : %08lx, fp : %08lx\n",
		      pregs->ARM_sp, pregs->ARM_ip, pregs->ARM_fp);
	sprd_hang_debug_printf(true, "r10 : %08lx, r9 : %08lx, r8 : %08lx\n",
		      pregs->ARM_r10, pregs->ARM_r9, pregs->ARM_r8);
	sprd_hang_debug_printf(true, "r7  : %08lx, r6 : %08lx, r5 : %08lx\n",
		      pregs->ARM_r7, pregs->ARM_r6, pregs->ARM_r5);
	sprd_hang_debug_printf(true, "r4  : %08lx, r3 : %08lx, r2 : %08lx\n",
		      pregs->ARM_r4, pregs->ARM_r3, pregs->ARM_r2);
	sprd_hang_debug_printf(true, "r1  : %08lx, r0 : %08lx\n",
			  pregs->ARM_r1, pregs->ARM_r0);
#endif
	if (!user_mode(pregs))
		show_extra_register_data(pregs, 128);

	wdh_step[cpu] = SPRD_HANG_DUMP_CPU_REGS;
	print_step(cpu);
}

static void get_gicc_regs(struct gicc_data *c)
{
#if IS_ENABLED(CONFIG_ARM64)
	c->gicc_ctlr = (u32)read_sysreg_s(SYS_ICC_CTLR_EL1);
	c->gicc_pmr = (u32)read_sysreg_s(SYS_ICC_PMR_EL1);
	c->gicc_bpr = (u32)read_sysreg_s(SYS_ICC_BPR1_EL1);
	c->gicc_rpr = (u32)read_sysreg_s(SYS_ICC_RPR_EL1);
	c->gicc_hppir = (u32)read_sysreg_s(SYS_ICC_HPPIR1_EL1);
#else
	c->gicc_ctlr = (u32)read_sysreg(ICC_CTLR);
	c->gicc_pmr = (u32)read_sysreg(ICC_PMR);
	c->gicc_bpr = (u32)read_sysreg(ICC_BPR1);
	c->gicc_rpr = (u32)read_sysreg(ICC_RPR);
	c->gicc_hppir = (u32)read_sysreg(ICC_HPPIR1);
#endif

#if 0   //memory-mapped, do not find offset of gicc
	c->gicc_abpr = 0;
	c->gicc_aiar = 0;
	c->gicc_ahppir = 0;
	c->gicc_statusr = 0;
	c->gicc_iidr = 0;
#endif
	isb();
}

static int get_gicd_regs(struct gicd_data *d)
{
	int i;
	void __iomem *base = (void __iomem *)gic_get_gicd_base();

	if (base) {
		d->gicd_ctlr = readl_relaxed(base + GICD_CTLR);
		d->gicd_typer = readl_relaxed(base + GICD_TYPER);
		d->gicd_statusr = readl_relaxed(base + GICD_STATUSR);

		for (i = 0; i < 7; i++) {
			d->gicd_isenabler[i] = readl_relaxed(base + GICD_ISENABLER + i * 4);
			d->gicd_ispendr[i] = readl_relaxed(base + GICD_ISPENDR + i * 4);
		}
		return 0;
	}
	return -1;
}

static void gicc_regs_value_dump(int cpu)
{
	if (!gicc_regs) {
		sprd_hang_debug_printf(true, "gicc_regs allocation failed\n");
		return;
	}

	get_gicc_regs(gicc_regs);

	sprd_hang_debug_printf(true, "gicc_ctlr  : %08x, gicc_pmr : %08x, gicc_bpr : %08x\n",
		     gicc_regs->gicc_ctlr, gicc_regs->gicc_pmr, gicc_regs->gicc_bpr);
	sprd_hang_debug_printf(true, "gicc_rpr  : %08x, gicc_hppir0 : %08x, gicc_abpr : %08x\n",
		     gicc_regs->gicc_rpr, gicc_regs->gicc_hppir, gicc_regs->gicc_abpr);
	sprd_hang_debug_printf(true, "gicc_aiar  : %08x, gicc_ahppir : %08x, gicc_statusr : %08x\n",
		     gicc_regs->gicc_aiar, gicc_regs->gicc_ahppir, gicc_regs->gicc_statusr);
	sprd_hang_debug_printf(true, "gicc_iidr  : %08x\n", gicc_regs->gicc_iidr);

	wdh_step[cpu] = SPRD_HANG_DUMP_GICC_REGS;
	print_step(cpu);
}

static void gicd_regs_value_dump(int cpu)
{
	int i, ret;

	if (!gicd_regs) {
		sprd_hang_debug_printf(true, "gicd_regs allocation failed\n");
		return;
	}
	for (i = 0; i < 7; i++) {
		gicd_regs->gicd_igroup[i] = (u32)smcc_get_regs(GICD_IGROUP_0 + i, i);
		gicd_regs->gicd_igrpmodr[i] = (u32)smcc_get_regs(GICD_IGRPMODR_0 + i, i);
	}

	ret = get_gicd_regs(gicd_regs);
	if (!ret) {
		sprd_hang_debug_printf(true, "--- show the gicd regs ---\n");

		sprd_hang_debug_printf(true, "gicd_ctlr: %08x, gicd_typer : %08x, gicd_statusr : %08x\n",
			gicd_regs->gicd_ctlr,
			gicd_regs->gicd_typer,
			gicd_regs->gicd_statusr);

		sprd_hang_debug_printf(true, "gicd_igroup: %08x, %08x, %08x, %08x, %08x, %08x, %08x\n",
			gicd_regs->gicd_igroup[0],
			gicd_regs->gicd_igroup[1],
			gicd_regs->gicd_igroup[2],
			gicd_regs->gicd_igroup[3],
			gicd_regs->gicd_igroup[4],
			gicd_regs->gicd_igroup[5],
			gicd_regs->gicd_igroup[6]);

		sprd_hang_debug_printf(true, "gicd_isenabler: %08x, %08x, %08x, %08x, %08x, %08x, %08x\n",
			gicd_regs->gicd_isenabler[0],
			gicd_regs->gicd_isenabler[1],
			gicd_regs->gicd_isenabler[2],
			gicd_regs->gicd_isenabler[3],
			gicd_regs->gicd_isenabler[4],
			gicd_regs->gicd_isenabler[5],
			gicd_regs->gicd_isenabler[6]);

		sprd_hang_debug_printf(true, "gicd_ispendr: %08x, %08x, %08x, %08x, %08x, %08x, %08x\n",
			gicd_regs->gicd_ispendr[0],
			gicd_regs->gicd_ispendr[1],
			gicd_regs->gicd_ispendr[2],
			gicd_regs->gicd_ispendr[3],
			gicd_regs->gicd_ispendr[4],
			gicd_regs->gicd_ispendr[5],
			gicd_regs->gicd_ispendr[6]);

		sprd_hang_debug_printf(true, "gicd_igrpmodr: %08x, %08x, %08x, %08x, %08x, %08x, %08x\n",
			gicd_regs->gicd_igrpmodr[0],
			gicd_regs->gicd_igrpmodr[1],
			gicd_regs->gicd_igrpmodr[2],
			gicd_regs->gicd_igrpmodr[3],
			gicd_regs->gicd_igrpmodr[4],
			gicd_regs->gicd_igrpmodr[5],
			gicd_regs->gicd_igrpmodr[6]);
	} else {
		wdh_step[cpu] = -SPRD_HANG_DUMP_GICD_REGS;
	}

	wdh_step[cpu] = SPRD_HANG_DUMP_GICD_REGS;
	print_step(cpu);
}

static void sprd_unwind_backtrace_dump(int cpu)
{
	int i;
	struct stackframe frame;
	struct pt_regs *pregs = &cpu_context[cpu];
	unsigned long sp;

#if IS_ENABLED(CONFIG_ARM64)
	frame.fp = pregs->regs[29]; //x29
	frame.pc = pregs->pc;
	sp = pregs->sp;
#else
	frame.fp = pregs->ARM_fp;
	frame.sp = pregs->ARM_sp;
	frame.lr = pregs->ARM_lr;
	frame.pc = pregs->ARM_pc;
	sp = pregs->ARM_sp;
#endif

#if IS_ENABLED(CONFIG_VMAP_STACK)
	if (!((sp >= VMALLOC_START) && (sp < VMALLOC_END))) {
		sprd_hang_debug_printf(true, "%s sp out of kernel addr space %08lx\n", sp);
		return;
	}
#else
	if (!sprd_virt_addr_valid(sp)) {
		sprd_hang_debug_printf(true, "invalid sp[%lx]\n", sp);
		return;
	}
#endif
	if (!sprd_virt_addr_valid(frame.pc)) {
		sprd_hang_debug_printf(true, "invalid pc\n");
		return;
	}
	sprd_hang_debug_printf(true, "callstack:\n");
	sprd_hang_debug_printf(true, "[<%08lx>] (%pS)\n", frame.pc, (void *)frame.pc);

	for (i = 0; i < MAX_CALLBACK_LEVEL; i++) {
		int urc;
#if IS_ENABLED(CONFIG_ARM64)
		urc = unwind_frame(NULL, &frame);
#else
		urc = unwind_frame(&frame);
#endif
		if (urc < 0)
			break;

		if (!sprd_virt_addr_valid(frame.pc)) {
			sprd_hang_debug_printf(true, "i=%d, sprd_virt_addr_valid fail\n", i);
			break;
		}

		sprd_hang_debug_printf(true, "[<%08lx>] (%ps)\n", frame.pc, (void *)frame.pc);
	}

	wdh_step[cpu] = SPRD_HANG_DUMP_CALL_STACK;
	print_step(cpu);
}

asmlinkage __visible void wdh_atf_entry(struct pt_regs *data)
{
	int cpu, cpu_num;
	struct pt_regs *pregs;
	unsigned short cpu_state;
	int is_last_cpu;
	int wait_last_cnt = 5;

	cpu = smp_processor_id();
	wdh_step[cpu] = SPRD_HANG_DUMP_ENTER;
	sprd_hang_debug_printf(true, "---wdh enter!---\n");

#if IS_ENABLED(CONFIG_ARM64)
	if (sprd_cpu_feed_mask && sprd_cpu_feed_bitmap)
		sprd_hang_debug_printf(true, "cpu_feed_mask:%*pbl cpu_feed_bitmap:%*pbl\n",
					cpumask_pr_args(sprd_cpu_feed_mask),
					cpumask_pr_args(sprd_cpu_feed_bitmap));
#else
	sprd_hang_debug_printf(true, "cpu_feed_mask:0x%08x cpu_feed_bitmap:0x%08x\n",
					cpu_feed_mask, cpu_feed_bitmap);

#endif
	oops_in_progress = 1;
	pregs = &cpu_context[cpu];
	*pregs = *data;
#if IS_ENABLED(CONFIG_ARM64)
	pregs->pc = (unsigned long)smcc_get_regs(CPU_PC, cpu);
	pregs->pstate = (unsigned long)smcc_get_regs(CPU_CPSR, cpu);
#else
	pregs->ARM_pc = (unsigned long)smcc_get_regs(CPU_PC, cpu);
	pregs->ARM_cpsr = (unsigned long)smcc_get_regs(CPU_CPSR, cpu);
#endif

	cpu_state = smcc_get_cpu_state(cpu);
	is_last_cpu = is_el3_ret_last_cpu(cpu, cpu_state);

	cpu_regs_value_dump(cpu);
	sprd_unwind_backtrace_dump(cpu);
	gicc_regs_value_dump(cpu);

	sprd_hang_debug_printf(true, "cpu_state = 0x%04x\n", (unsigned int)cpu_state);

	if (user_mode(pregs) || atomic_xchg(&sprd_enter_wdh, 1)) {
		sprd_hang_debug_printf(true, "%s: goto panic idle\n", __func__);
		sprd_dump_stack_reg(cpu, pregs);
		minidump_update_current_stack(cpu, pregs);
		sysdump_ipi(pregs);
		wdh_step[cpu] = SPRD_HANG_DUMP_SYSDUMP;
		flush_cache_all();
		while (1)
			cpu_relax();
	}
	wdh_step[cpu] = SPRD_HANG_DUMP_SYSDUMP;
	gicd_regs_value_dump(cpu);

	/* wait for other cpu finised */
	while (wait_last_cnt-- && !is_last_cpu) {
		mdelay(50);
		cpu_state = smcc_get_cpu_state(cpu);
		is_last_cpu = is_el3_ret_last_cpu(cpu, cpu_state);
	}
	sprd_hang_debug_printf(true, "wait last cpu %s\n", is_last_cpu ? "ok" : "failed");

	if (!is_last_cpu)
		sprd_hang_debug_printf(true, "cpu_state = 0x%04x\n", (unsigned int)cpu_state);
	else {
		for_each_online_cpu(cpu_num) {
			wait_last_cnt = 10;
			while (wait_last_cnt--) {
				if (wdh_step[cpu_num] && (wdh_step[cpu_num] < SPRD_HANG_DUMP_SYSDUMP))
					mdelay(50);
				else
					break;
			}
		}
	}

	prepare_dump_info_for_wdh(pregs, "wdt fiq assert");

	wdh_step[cpu] = SPRD_HANG_DUMP_INFO;
	print_step(cpu);

	sprd_dump_task_stats();
	sprd_dump_runqueues();
	sprd_dump_stack_reg(cpu, pregs);
	sprd_dump_mem_stat();
	sprd_dump_interrupts();
	minidump_update_current_stack(cpu, pregs);
	sprd_dump_io();
	sysdump_ipi(pregs);

	wdh_step[cpu] = SPRD_HANG_DUMP_END;
	print_step(cpu);

	mdelay(100);
#if IS_ENABLED(CONFIG_SPRD_HANG_RESET)
	/* wait last cpu failed,wait 20s to trigger cm4 continue to handle ap watchdog reset */
	if (!is_last_cpu) {
		sprd_hang_debug_printf(true, "wait 20s,trigger cm4 handling ap watchdog reset\n");
		mdelay(20000);
	}
#endif
	do_kernel_restart("panic");
}

#if IS_ENABLED(CONFIG_ARM64)
static struct sprd_sip_svc_handle *(*sprd_sip_svc_get_handle_ptr)(void);
#endif

#if IS_ENABLED(CONFIG_ARM64)
asmlinkage void entry_for_wdh_el0(void);
#endif
asmlinkage void entry_for_wdh_el1(void);
asmlinkage void entry_for_wdh_el2(void);

#if IS_ENABLED(CONFIG_ARM64)
static int __nocfi sprd_module_notifier_fn(struct notifier_block *nb,
		unsigned long action, void *data)
{
	int ret = 0;
	struct module *module = data;

	if (action != MODULE_STATE_LIVE)
		return NOTIFY_DONE;

	/* return immediately if the func has been found */
	if (sprd_sip_svc_get_handle_ptr)
		return NOTIFY_DONE;

	if (!strncmp(module->name, "sprd_sip_svc", strlen("sprd_sip_svc"))) {
		sprd_sip_svc_get_handle_ptr = (void *)
			module_kallsyms_lookup_name("sprd_sip_svc_get_handle");
	}

	if (!sprd_sip_svc_get_handle_ptr)
		return NOTIFY_DONE;

	svc_handle = sprd_sip_svc_get_handle_ptr();
	if (!svc_handle) {
		pr_err("failed to get svc handle\n");
		return NOTIFY_DONE;
	}

	ret = set_panic_debug_entry((unsigned long)entry_for_wdh_el0, 0, SPRD_NS_EL0);
	if (ret)
		pr_emerg("init ATF entry_for_wdh0 error[%d]\n", ret);

	ret = set_panic_debug_entry((unsigned long)entry_for_wdh_el1, read_sysreg(ttbr1_el1), SPRD_NS_EL1);
	if (ret)
		pr_emerg("init ATF entry_for_wdh1 error[%d]\n", ret);

	ret = set_panic_debug_entry((unsigned long)__pa_symbol(entry_for_wdh_el2), 0, SPRD_NS_EL2);
	if (ret)
		pr_emerg("init ATF entry_for_wdh2 error[%d]\n", ret);

	return NOTIFY_DONE;
}

static struct notifier_block sprd_module_notifier = {
	.notifier_call = sprd_module_notifier_fn,
};

static int __nocfi sprd_getmask_notifier_fn(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct module *module = data;

	if (action != MODULE_STATE_LIVE)
		return NOTIFY_DONE;
	/* return immediately if the func has been found */
	if (sprd_cpu_feed_mask || sprd_cpu_feed_bitmap)
		return NOTIFY_DONE;

	if (!strncmp(module->name, "sprd_wdf", strlen("sprd_wdf"))) {
		sprd_cpu_feed_mask = (struct cpumask *)
			module_kallsyms_lookup_name("cpu_feed_mask");
		sprd_cpu_feed_bitmap = (struct cpumask *)
			module_kallsyms_lookup_name("cpu_feed_bitmap");

		if (!sprd_cpu_feed_mask || !sprd_cpu_feed_bitmap)
			pr_emerg("can not find cpu feed mask!\n");
	}

	return NOTIFY_DONE;
}

static struct notifier_block sprd_getmask_notifier = {
	.notifier_call = sprd_getmask_notifier_fn,
};
#else
static int sprd_wdh_sip_init(void)
{
	int ret = 0;

	svc_handle = sprd_sip_svc_get_handle();
	if (!svc_handle)
		pr_err("failed to get svc handle\n");

	ret = set_panic_debug_entry((unsigned long)entry_for_wdh_el1, 0, SPRD_NS_EL0);
	if (ret)
		pr_emerg("init ATF entry_for_wdh0 error[%d]\n", ret);

	ret = set_panic_debug_entry((unsigned long)entry_for_wdh_el1, 0, SPRD_NS_EL1);
	if (ret)
		pr_emerg("init ATF entry_for_wdh1 error[%d]\n", ret);

	ret = set_panic_debug_entry((unsigned long)__pa_symbol(entry_for_wdh_el2), 0, SPRD_NS_EL2);
	if (ret)
		pr_emerg("init ATF entry_for_wdh2 error[%d]\n", ret);

	return ret;
}
#endif

#ifdef CONFIG_SPRD_SYSDUMP
#define MAX_NAME_LEN 16

static void minidump_add_sprd_log_buf(void)
{
	int cpu;
	u64 buf_base;
	char name[MAX_NAME_LEN];

	for (cpu = 0; cpu < SPRD_CPUS; cpu++) {
		buf_base = (u64)sprd_log_buf[cpu];
		if (!buf_base)
			return;
		scnprintf(name, MAX_NAME_LEN, "sprd_log_buf%d", cpu);
		if (minidump_save_extend_information(name, __pa(buf_base),
						__pa(buf_base + SPRD_PRINT_BUF_LEN)))
			return;
	}
}
#else
static inline void minidump_add_sprd_log_buf(void) {}
#endif
static int sprd_wdh_atf_init(void)
{
	int i;
	int ret = 0;

	raw_spin_lock_init(&sprd_wdh_prlock);
	raw_spin_lock_init(&sprd_wdh_wclock);
	atomic_set(&sprd_enter_wdh, 0);

#if IS_ENABLED(CONFIG_ARM64)
	/* register a notifier which called when modules install */
	register_module_notifier(&sprd_module_notifier);
#else
	sprd_wdh_sip_init();
#endif

	gicc_regs = kmalloc(sizeof(struct gicc_data), GFP_KERNEL);
	if (!gicc_regs) {
		pr_err("alloc gicc_regs fail!\n");
		return -ENOMEM;
	}

	gicd_regs = kmalloc(sizeof(struct gicd_data), GFP_KERNEL);
	if (!gicd_regs) {
		pr_err("alloc gicd_regs fail!\n");
		ret = -ENOMEM;
		goto gic_fail;
	}

	for (i = 0; i < SPRD_CPUS; i++) {
		sprd_log_buf[i] = kzalloc(SPRD_PRINT_BUF_LEN, GFP_KERNEL);
		if (!sprd_log_buf[i]) {
			pr_err("alloc sprd_log_buf fail!\n");
			ret = -ENOMEM;
			goto out;
		}
	}

#if IS_ENABLED(CONFIG_ARM64)
	/* register a notifier which called when modules install */
	register_module_notifier(&sprd_getmask_notifier);
#endif

	minidump_add_sprd_log_buf();
	return ret;

out:
	for (i = 0; i < SPRD_CPUS; i++)
		kfree(sprd_log_buf[i]);

	kfree(gicd_regs);
gic_fail:
	kfree(gicc_regs);
	return ret;
}

late_initcall(sprd_wdh_atf_init);

MODULE_DESCRIPTION("sprd hang debug wdh driver");
MODULE_LICENSE("GPL v2");
