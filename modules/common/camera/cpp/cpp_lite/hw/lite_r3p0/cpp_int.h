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

#ifndef _CPP_INT_H_
#define _CPP_INT_H_

enum cpp_irq_id {
	CPP_SCALE_DONE = 0,
	CPP_ROT_DONE,
	CPP_DMA_DONE,
#ifdef DEBUG_CPP_MMU
	CPP_MMU_0,
	CPP_MMU_1,
	CPP_MMU_2,
	CPP_MMU_3,
	CPP_MMU_4,
	CPP_MMU_5,
	CPP_MMU_6,
	CPP_MMU_7,
#endif
	CPP_IRQ_NUMBER
};

int cpp_int_irq_request(void *cpp_handle);
int cpp_int_irq_free(void *cpp_handle);
#endif
