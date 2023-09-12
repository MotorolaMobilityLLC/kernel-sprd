/*
 * Copyright (C) 2021-2022 UNISOC Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _ISP_DRIVER_H_
#define _ISP_DRIVER_H_

int isp_drv_hw_init(void *arg);
int isp_drv_hw_deinit(void *arg);
enum isp_fetch_format isp_drv_fetch_format_get(struct isp_uinfo *pipe_src);
enum isp_fetch_format isp_drv_fetch_pyr_format_get(struct isp_uinfo *pipe_src);
int isp_drv_pipeinfo_get(void *arg, void *frame);
int isp_drv_dt_parse(struct device_node *dn, struct cam_hw_info *hw_info, uint32_t *isp_count);

#endif
