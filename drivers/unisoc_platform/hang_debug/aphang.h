/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __SPRD_HANG_DEBUG_H__

extern int sprd_wdh_atf_init(void);
#if IS_ENABLED(CONFIG_SPRD_HANG_TRIGGER)
extern int send_ipi_init(void);
extern void send_ipi_exit(void);
#else
static inline int send_ipi_init(void)
{
	return 0;
}
static inline void send_ipi_exit(void) {}
#endif

#endif  /* __SPRD_HANG_DEBUG_H__ */
