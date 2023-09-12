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

#include "isp_drv.h"
#include "dcam_drv.h"

extern unsigned long s_dcam_regbase[DCAM_MAX_COUNT];

int dcam_k_cfg_blc(struct isp_io_param *param, enum dcam_id idx);
int dcam_k_cfg_raw_aem(struct isp_io_param *param, enum dcam_id idx);
int dcam_k_cfg_pdaf(struct isp_io_param *param, enum dcam_id idx,
				struct sprd_pdaf_control *pdaf_ctrl);
int dcam_k_cfg_2d_lsc(struct isp_io_param *param,
			struct isp_k_block *isp_k_param, enum dcam_id idx);

