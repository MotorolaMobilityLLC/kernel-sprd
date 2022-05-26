#ifndef __SPRD_PLATFORM_SYSDUMP_H
#define __SPRD_PLATFORM_SYSDUMP_H

/* the SPRD_SYSDUMP_MAGIC indicates the ramdisk addr,
 * the ramdisk add maybe different in different boards
 * just for backup.
 */
#ifndef CONFIG_X86_64
#define SPRD_SYSDUMP_MAGIC      0x85500000
#else
#define SPRD_SYSDUMP_MAGIC      0x3B800000
#endif

#define SPRD_SYSDUMP_RESERVED	"sysdumpinfo-mem"

struct sysdump_mem {
	unsigned long paddr;
	unsigned long vaddr;
	unsigned long soff;
	unsigned long size;
	unsigned long type;
};

enum sysdump_type {
	SYSDUMP_RAM,
	SYSDUMP_MODEM,
	SYSDUMP_IOMEM,
};

#ifdef CONFIG_ARM
#include "sysdump32.h"

/* refer to sprd_wdh.c */
#define sprd_virt_addr_valid(kaddr) ((void *)(kaddr) >= (void *)PAGE_OFFSET && \
		(void *)(kaddr) < (void *)high_memory && \
		pfn_valid(__pa(kaddr) >> PAGE_SHIFT))
#endif

#ifdef CONFIG_ARM64
#include "sysdump64.h"

/* refer to sprd_wdh.c */
#define sprd_virt_addr_valid(kaddr) ((void *)(kaddr) >= (void *)PAGE_OFFSET)
#endif

#ifdef CONFIG_X86_64
#include "sysdump_x86_64.h"
#endif

#endif
