/* SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2020 Uniso Communications Inc.
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

#ifndef _UFS_SPRD_PWR_DEBUG_H_
#define _UFS_SPRD_PWR_DEBUG_H_
int ufs_sprd_pwr_change_compare(struct ufs_hba *hba,
		enum ufs_notify_change_status status,
		struct ufs_pa_layer_attr *final_params,
		int err);
#endif/* _UFS_SPRD_PWR_DEBUG_H_  */
