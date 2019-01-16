/*
 * Copyright (C) 2015-2016 Spreadtrum Communications Inc.
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
#ifndef _CAM_COMMON_H_
#define _CAM_COMMON_H_

#define reg_rd(p, reg) \
	readl_relaxed((p)->io_base + (reg))
#define reg_wr(p, reg, val) \
	writel_relaxed((val), ((p)->io_base + (reg)))
#define reg_awr(p, reg, val) \
	writel_relaxed((readl_relaxed((p)->io_base + (reg)) & (val)), \
		       ((p)->io_base + (reg)))
#define reg_owr(p, reg, val) \
	writel_relaxed((readl_relaxed((p)->io_base + (reg)) | (val)), \
		       ((p)->io_base + (reg)))
#define reg_mwr(p, reg, mask, val) \
	do { \
		unsigned int v = readl_relaxed((p)->io_base + (reg)); \
		v &= ~(mask); \
		writel_relaxed((v | ((mask) & (val))), \
			       ((p)->io_base + (reg))); \
	} while (0)

#endif /* _CAM_COMMON_H_ */
