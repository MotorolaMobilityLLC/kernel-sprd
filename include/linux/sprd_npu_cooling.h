/* SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2020 Spreadtrum Communications Inc.
 */

#ifndef _SPRD_NPU_COOLING_H_
#define _SPRD_NPU_COOLING_H_

int npu_cooling_device_register(struct devfreq *npudev);
int npu_cooling_device_unregister(void);

#endif
