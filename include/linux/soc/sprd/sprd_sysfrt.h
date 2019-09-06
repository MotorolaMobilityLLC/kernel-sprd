#ifndef __SPRD_SYSFRT_H__
#define __SPRD_SYSFRT_H__

#ifdef CONFIG_SPRD_TIMER_SYSFRT
extern u64 sprd_sysfrt_read(void);
#else
static inline u64 sprd_sysfrt_read(void)
{
     return 0;
}
#endif

#endif
