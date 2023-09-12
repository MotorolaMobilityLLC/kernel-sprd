// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Unisoc Inc.
 */

#ifndef __VHA_COOLING_H_
#define __VHA_COOLING_H_

void vha_cooling_register(struct devfreq *vha_devfreq);
void vha_cooling_unregister(void);

#endif /*_VHA_COOLING_H */

