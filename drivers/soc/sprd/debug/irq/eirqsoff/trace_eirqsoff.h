#ifndef __SPRD_EIRQSOFF_H
#define __SPRD_EIRQSOFF_H

#ifdef CONFIG_SPRD_EIRQSOFF
extern void notrace
start_eirqsoff_timing(unsigned long ip, unsigned long parent_ip);
extern void notrace
stop_eirqsoff_timing(unsigned long ip, unsigned long parent_ip);
#ifdef CONFIG_PREEMPT_TRACER
extern void notrace
start_epreempt_timing(unsigned long ip, unsigned long parent_ip);
extern void notrace
stop_epreempt_timing(unsigned long ip, unsigned long parent_ip);
#else
#define start_epreempt_timing(ip, parent_ip)  \
do { } while (0)
#define stop_epreempt_timing(ip, parent_ip)   \
do { } while (0)
#endif
#ifdef CONFIG_HARDLOCKUP_DETECTOR_OTHER_CPU
extern void notrace
start_irqsoff_panic_timing(void);
extern void notrace
stop_irqsoff_panic_timing(void);
#else
#define start_irqsoff_panic_timing()  \
do { } while (0)
#define stop_irqsoff_panic_timing()   \
do { } while (0)
#endif
#else
#define start_eirqsoff_timing(ip, parent_ip)  \
do { } while (0)
#define stop_eirqsoff_timing(ip, parent_ip)   \
do { } while (0)
#define start_irqsoff_panic_timing()  \
do { } while (0)
#define stop_irqsoff_panic_timing()   \
do { } while (0)
#define start_epreempt_timing(ip, parent_ip)  \
do { } while (0)
#define stop_epreempt_timing(ip, parent_ip)   \
do { } while (0)
#endif

#endif
