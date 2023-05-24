/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __SHUTDOWN_DETECT__H__
#define __SHUTDOWN_DETECT__H__
#if IS_ENABLED(CONFIG_SPRD_SHUTDOWN_DETECT)
extern int get_shutdown_flag(void);
#else/* !CONFIG_SPRD_SHUTDOWN_DETECT */
static inline int get_shutdown_flag(void) { return 0; }
#endif/* CONFIG_SPRD_SHUTDOWN_DETECT */
#endif/* __SHUTDOWN_DETECT__H__ */
