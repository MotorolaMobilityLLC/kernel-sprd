/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __UNISOC_DUMP_INFO_H__
#define __UNISOC_DUMP_INFO_H__

#include <linux/seq_buf.h>

/* GKI requires the NR_CPUS is 32 */
#if NR_CPUS >= 8
#define UNISOC_NR_CPUS            8
#else
#define UNISOC_NR_CPUS            NR_CPUS
#endif
#define UNISOC_DUMP_RQ_SIZE	(2000 * UNISOC_NR_CPUS)
#define UNISOC_DUMP_MAX_TASK	3000
#define UNISOC_DUMP_TASK_SIZE	(160 * (UNISOC_DUMP_MAX_TASK + 2))
#define UNISOC_DUMP_STACK_SIZE	(2048 * UNISOC_NR_CPUS)
#define UNISOC_DUMP_IRQ_SIZE	12288
#define MAX_CALLBACK_LEVEL	16
#define MAX_NAME_LEN		16

#define SEQ_printf(m, x...)			\
do {						\
	if (m)					\
		seq_buf_printf(m, x);		\
	else					\
		pr_debug(x);			\
} while (0)

#endif /* __UNISOC_DUMP_INFO_H */
