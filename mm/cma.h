/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __MM_CMA_H__
#define __MM_CMA_H__

struct cma {
	unsigned long   base_pfn;
	unsigned long   count;
#ifdef CONFIG_SPRD_CMA_DEBUG
	unsigned long   free_count;
#endif
	unsigned long   *bitmap;
	unsigned int order_per_bit; /* Order of pages represented by one bit */
	struct mutex    lock;
#ifdef CONFIG_CMA_DEBUGFS
	struct hlist_head mem_head;
	spinlock_t mem_head_lock;
#endif
	const char *name;
};

#ifdef CONFIG_SPRD_CMA_DEBUG
struct sprd_cma_debug_info {
	unsigned long caller_addr;
	int alloc_pages;
	unsigned int alloc_cnt;
	unsigned long cost_us;
};

#define MAX_SPRD_CMA_DEBUG_NUM 512

struct sprd_cma_debug {
	struct sprd_cma_debug_info sprd_cma_info[MAX_SPRD_CMA_DEBUG_NUM];
	struct mutex lock;
	unsigned int sum_cnt;
};

extern int sysctl_sprd_cma_debug;
int sysctl_sprd_cma_debug_handler(struct ctl_table *table, int write,
			void __user *buffer, size_t *length, loff_t *ppos);
#endif

extern struct cma cma_areas[MAX_CMA_AREAS];
extern unsigned cma_area_count;

static inline unsigned long cma_bitmap_maxno(struct cma *cma)
{
	return cma->count >> cma->order_per_bit;
}

#endif
