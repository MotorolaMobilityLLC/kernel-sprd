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

#ifndef __MM_DVFS_TABLE_H____
#define __MM_DVFS_TABLE_H____

#include "mm_dvfs.h"
extern struct ip_dvfs_map_cfg isp_dvfs_config_table[8];
extern struct ip_dvfs_map_cfg cpp_dvfs_config_table[8];
extern struct ip_dvfs_map_cfg fd_dvfs_config_table[8];
extern struct ip_dvfs_map_cfg jpg_dvfs_config_table[8];
extern struct ip_dvfs_map_cfg dcam_dvfs_config_table[8];
extern struct ip_dvfs_map_cfg dcamaxi_dvfs_config_table[8];
extern struct ip_dvfs_map_cfg mtx_dvfs_config_table[8];

#endif
