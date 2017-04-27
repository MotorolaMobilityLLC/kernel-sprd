#ifndef _REGS_DEBUG_
#define _REGS_DEBUG_

#include <linux/smp.h>
#include <linux/threads.h>
#include <linux/jiffies.h>

struct sprd_debug_regs_access {
	unsigned long vaddr;
	unsigned long stack;
	unsigned long pc;
	unsigned long time;
	unsigned int status;
	u32 value;
};
#ifdef CONFIG_ARM64
#define sprd_debug_regs_read_start(a) ({  \
	unsigned long cpu_id, stack, lr, tmp;	\
	asm volatile(				\
		"	mrs %3,mpidr_el1\n"	\
		"	ands %0, %3, #0xf\n"	\
		"	lsr %3,%3,#8\n"		\
		"	ands %3, %3, #0xf\n"	\
		"	lsl %3,%3,#2\n"		\
		"	add %0,%0,%3\n"		\
		"	mov %2, sp\n"		\
		"	ldr %1, [x29, #8]\n"	\
		: "=&r" (cpu_id),		\
		  "=&r" (lr),			\
		  "=&r" (stack),		\
		  "=&r" (tmp)			\
		:				\
		: "memory");			\
	if (sprd_debug_last_regs_access) {	\
		sprd_debug_last_regs_access[cpu_id].value = 0;		\
		sprd_debug_last_regs_access[cpu_id].vaddr = (unsigned long)a; \
		sprd_debug_last_regs_access[cpu_id].stack = stack;	\
		sprd_debug_last_regs_access[cpu_id].pc = lr;		\
		sprd_debug_last_regs_access[cpu_id].status = 0; }	\
	})

#define sprd_debug_regs_write_start(v, a)	({		\
	unsigned long cpu_id, stack, lr, tmp;		\
	asm volatile(					\
		"	mrs %3,mpidr_el1\n"		\
		"	ands %0, %3, #0xf\n"		\
		"	lsr %3,%3,#8\n"			\
		"	ands %3, %3, #0xf\n"		\
		"	lsl %3,%3,#2\n"			\
		"	add %0,%0,%3\n"			\
		"	mov %2, sp\n"			\
		"	ldr %1, [x29, #8]\n"		\
		: "=&r" (cpu_id),			\
		  "=&r" (lr),				\
		  "=&r" (stack),			\
		  "=&r" (tmp)				\
		:					\
		: "memory");				\
	if (sprd_debug_last_regs_access) {		\
		sprd_debug_last_regs_access[cpu_id].value = (unsigned long)(v);\
		sprd_debug_last_regs_access[cpu_id].vaddr = (unsigned long)(a);\
		sprd_debug_last_regs_access[cpu_id].stack = stack;	\
		sprd_debug_last_regs_access[cpu_id].pc = lr;		\
		sprd_debug_last_regs_access[cpu_id].status = 0; }	\
	})

#define sprd_debug_regs_access_done()	({				\
	unsigned long cpu_id, lr, tmp;				\
	asm volatile(						\
		"	mrs %2,mpidr_el1\n"			\
		"	ands %0, %2, #0xf\n"			\
		"	lsr %2,%2,#8\n"				\
		"	ands %2, %2, #0xf\n"			\
		"	lsl %2,%2,#2\n"				\
		"	add %0,%0,%2\n"				\
		"	ldr %1, [x29, #8]\n"			\
		: "=&r" (cpu_id), "=&r" (lr), "=&r" (tmp)	\
		:						\
		: "memory");					\
	if (sprd_debug_last_regs_access) {			\
		sprd_debug_last_regs_access[cpu_id].time = jiffies;     \
		sprd_debug_last_regs_access[cpu_id].status = 1; }	\
	})

#else
#ifdef CONFIG_ARM
#define sprd_debug_regs_read_start(a)	({u32 cpu_id, stack, lr;	\
	asm volatile(						\
		"	mrc	p15, 0, %0, c0, c0, 5\n"	\
		"	ands %0, %0, #0xf\n"			\
		"	mov %2, r13\n"				\
		"	mov %1, pc\n"				\
		: "=&r" (cpu_id), "=&r" (lr), "=&r" (stack)	\
		:						\
		: "memory");					\
	if (sprd_debug_last_regs_access) {			\
		sprd_debug_last_regs_access[cpu_id].value = 0;		\
		sprd_debug_last_regs_access[cpu_id].vaddr = (u32)a;	\
		sprd_debug_last_regs_access[cpu_id].stack = stack;	\
		sprd_debug_last_regs_access[cpu_id].pc = lr;		\
		sprd_debug_last_regs_access[cpu_id].status = 0; }	\
	})

#define sprd_debug_regs_write_start(v, a)	({			\
	u32 cpu_id, stack, lr;					\
	asm volatile(						\
		"	mrc	p15, 0, %0, c0, c0, 5\n"	\
		"	ands %0, %0, #0xf\n"			\
		"	mov %2, r13\n"				\
		"	mov %1, pc\n"				\
		: "=&r" (cpu_id), "=&r" (lr), "=&r" (stack)	\
		:						\
		: "memory");					\
	if (sprd_debug_last_regs_access) {			\
		sprd_debug_last_regs_access[cpu_id].value = (u32)(v);	\
		sprd_debug_last_regs_access[cpu_id].vaddr = (u32)(a);	\
		sprd_debug_last_regs_access[cpu_id].stack = stack;	\
		sprd_debug_last_regs_access[cpu_id].pc = lr;		\
		sprd_debug_last_regs_access[cpu_id].status = 0; }	\
	})

#define sprd_debug_regs_access_done()	({u32 cpu_id, lr;		\
	asm volatile(						\
		"	mrc	p15, 0, %0, c0, c0, 5\n"	\
		"	ands %0, %0, #0xf\n"			\
		"	mov %1, pc\n"				\
		: "=&r" (cpu_id), "=&r" (lr)			\
		:						\
		: "memory");					\
	if (sprd_debug_last_regs_access) {			\
		sprd_debug_last_regs_access[cpu_id].time = jiffies;     \
		sprd_debug_last_regs_access[cpu_id].status = 1; }	\
	})

#else
#define sprd_debug_regs_read_start(a)
#define sprd_debug_regs_write_start(v, a)
#define sprd_debug_regs_access_done()
#endif
#endif
#endif
