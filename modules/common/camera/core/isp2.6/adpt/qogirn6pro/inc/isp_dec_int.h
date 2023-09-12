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

#ifndef _ISP_DEC_INT_H_
#define _ISP_DEC_INT_H_

enum isp_dec_irq_id {
	ISP_INT_DEC_ALL_DONE,
	ISP_INT_DEC_STORE_DONE,
	ISP_INT_DCT_STORE_DONE,
	ISP_INT_DEC_SHADOW_DONE,

	ISP_INT_DCT_SHADOW_DONE,
	ISP_INT_DEC_DISPATCH_DONE,
	ISP_INT_DEC_FMCU_LOAD_DONE,
	ISP_INT_DEC_FMCU_CONFIG_DONE,

	ISP_INT_DEC_FMCU_SHADOW_DONE,
	ISP_INT_DEC_FMCU_CMD_X,
	ISP_INT_DEC_FMCU_TIMEOUT,
	ISP_INT_DEC_FMCU_CMD_ERR,

	ISP_INT_DEC_FMCU_STOP_DONE,
	ISP_INT_DEC_NULL13,
	ISP_INT_DEC_NULL14,
	ISP_INT_DEC_NULL15,

	ISP_INT_DEC_FBD_HEADER_ERR,
	ISP_INT_DEC_FBD_PAYLOAD_ERR,
	ISP_INT_DEC_NULL18,
	ISP_INT_DEC_NULL19,

	ISP_INT_DEC_NULL20,
	ISP_INT_DEC_NULL21,
	ISP_INT_DEC_NULL22,
	ISP_INT_DEC_NULL23,

	ISP_INT_DEC_NULL24,
	ISP_INT_DEC_NULL25,
	ISP_INT_DEC_NULL26,
	ISP_INT_DEC_NULL27,

	ISP_INT_DEC_NULL28,
	ISP_INT_DEC_NULL29,
	ISP_INT_DEC_NULL30,
	ISP_INT_DEC_NULL31,
};

#define ISP_DEC_INT_LINE_MASK_ERR                \
	((1 << ISP_INT_DEC_FMCU_TIMEOUT) |       \
	(1 << ISP_INT_DEC_FMCU_CMD_ERR) |      \
	(1 << ISP_INT_DEC_FBD_HEADER_ERR) |       \
	(1 << ISP_INT_DEC_FBD_PAYLOAD_ERR))

#define ISP_DEC_INT_LINE_MASK                    \
	((1 << ISP_INT_DEC_FMCU_CONFIG_DONE) | ISP_DEC_INT_LINE_MASK_ERR)

#endif
