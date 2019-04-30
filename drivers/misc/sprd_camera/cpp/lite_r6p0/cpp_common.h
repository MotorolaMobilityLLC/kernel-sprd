/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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

#ifndef _CPP_COMMON_H_
#define _CPP_COMMON_H_

extern unsigned long g_cpp_base;
#define CPP_REG_BASE (g_cpp_base)

#define CPP_REG_WR(reg, val) (REG_WR((CPP_REG_BASE+(reg)), (val)))
#define CPP_REG_RD(reg) (REG_RD((CPP_REG_BASE+(reg))))
#define CPP_REG_AWR(reg, val) (REG_AWR((CPP_REG_BASE+(reg)), (val)))
#define CPP_REG_OWR(reg, val) (REG_OWR((CPP_REG_BASE+(reg)), (val)))
#define CPP_REG_MWR(reg, msk, val) \
			CPP_REG_WR(reg, \
			((val) & (msk)) | (CPP_REG_RD(reg) & (~(msk))))
#endif
