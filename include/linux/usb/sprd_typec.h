/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright 2015-2017 Google, Inc
 */

#ifndef __SPRD_TYPEC_H__
#define __SPRD_TYPEC_H__

enum sc27xx_typec_pd_swap {
	TYPEC_NO_SWAP,
	TYPEC_SOURCE_TO_SINK,
	TYPEC_SINK_TO_SOURCE,
	TYPEC_HOST_TO_DEVICE,
	TYPEC_DEVICE_TO_HOST,
};


#define	EXTCON_SINK         3
#define	EXTCON_SOURCE       4

extern int sc27xx_get_dr_swap_executing(void);
extern int sc27xx_get_current_status_detach_or_attach(void);
#endif /* __LINUX_USB_SPRD_TYPEC_H */
