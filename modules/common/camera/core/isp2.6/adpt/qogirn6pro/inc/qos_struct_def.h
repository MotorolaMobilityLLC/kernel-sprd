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

#ifndef __QOS_STRUCT_DEF_H__
#define __QOS_STRUCT_DEF_H__

struct qos_reg {
	const char	*reg_name;
	u32 	 base_addr;
	u32 	 mask_value;
	u32 	 set_value;
	};

#endif
