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

#include <video/sprd_mm.h>

#include "dcam_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "dcam_cap: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int sprd_dcam_cap_cfg_set(enum dcam_id idx, enum dcam_cfg_id id, void *param)
{
	uint32_t reg = 0;
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_cap_desc *cap_desc = sprd_dcam_drv_cap_get(idx);

	if (DCAM_ADDR_INVALID(cap_desc)) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	pr_info("id = %d, reg_base = 0x%lx, id = %d\n",
		idx, DCAM_BASE(idx), id);
	switch (id) {
	case DCAM_CAP_SENSOR_MODE:
		{
			enum dcam_cap_sensor_mode cap_sensor_mode =
				*(uint32_t *)param;

			pr_info("cap_sensor_mode %d\n", cap_sensor_mode);
			if (cap_sensor_mode >= DCAM_CAP_MODE_MAX)
				rtn = DCAM_RTN_CAP_SENSOR_MODE_ERR;
			else {
				if (cap_sensor_mode == DCAM_CAP_MODE_RAWRGB)
					DCAM_REG_MWR(idx, DCAM_MIPI_CAP_CFG,
						BIT_1, 1 << 1);
				else
					DCAM_REG_MWR(idx, DCAM_MIPI_CAP_CFG,
						BIT_1, 0 << 1);
				cap_desc->input_format = cap_sensor_mode;
			}
			break;
		}
	case DCAM_CAP_SAMPLE_MODE:
		{
			enum dcam_capture_mode samp_mode =
				*(enum dcam_capture_mode *)param;

			if (samp_mode >= DCAM_CAPTURE_MODE_MAX) {
				rtn = DCAM_RTN_MODE_ERR;
			} else {
				DCAM_REG_MWR(idx, DCAM_MIPI_CAP_CFG, BIT_3,
					samp_mode << 3);
				cap_desc->cap_mode =
					samp_mode;
			}
			break;
		}
	case DCAM_CAP_HREF_SEL:
		{
			struct dcam_cap_sync_pol *sync_pol =
				(struct dcam_cap_sync_pol *)param;

			if (sync_pol->need_href)
				DCAM_REG_MWR(idx, DCAM_MIPI_CAP_CFG,
					BIT_13, 1 << 13);
			else
				DCAM_REG_MWR(idx, DCAM_MIPI_CAP_CFG,
					BIT_13, 0 << 13);
			break;
		}
	/*sharkl5pro only support 10bit sensor*/
	case DCAM_CAP_DATA_BITS:
		break;
	case DCAM_CAP_PRE_SKIP_CNT:
		{
			uint32_t skip_num = *(uint32_t *)param;

			if (skip_num > DCAM_CAP_SKIP_FRM_MAX)
				rtn = DCAM_RTN_CAP_SKIP_FRAME_ERR;
			else
				DCAM_REG_MWR(idx, DCAM_MIPI_CAP_CFG,
					0x0f << 8, skip_num << 8);
			break;
		}
	case DCAM_CAP_FRM_DECI:
		{
			uint32_t deci_factor = *(uint32_t *)param;

			if (deci_factor < DCAM_FRM_DECI_FAC_MAX)
				DCAM_REG_MWR(idx, DCAM_MIPI_CAP_CFG,
					BIT_7 | BIT_6, deci_factor << 6);
			else
				rtn = DCAM_RTN_CAP_FRAME_DECI_ERR;
			break;
		}
	case DCAM_CAP_FRM_COUNT_CLR:
		DCAM_REG_MWR(idx, DCAM_CAP_FRM_CLR, BIT_8, BIT_8);
		break;
	case DCAM_CAP_INPUT_RECT:
		{
			struct camera_rect *rect = (struct camera_rect *)param;
			uint32_t tmp = 0;

			if (idx == DCAM_ID_0 &&
				(rect->x > DCAM_CAP_FRAME_WIDTH_MAX ||
				rect->y > DCAM_CAP_FRAME_HEIGHT_MAX ||
				rect->w > DCAM_CAP_FRAME_WIDTH_MAX ||
				rect->h > DCAM_CAP_FRAME_HEIGHT_MAX)) {
				rtn = DCAM_RTN_CAP_FRAME_SIZE_ERR;
				return -rtn;
			} else if (idx == DCAM_ID_1 &&
				(rect->x > DCAM1_CAP_FRAME_WIDTH_MAX ||
				rect->y > DCAM1_CAP_FRAME_HEIGHT_MAX ||
				rect->w > DCAM1_CAP_FRAME_WIDTH_MAX ||
				rect->h > DCAM1_CAP_FRAME_HEIGHT_MAX)) {
				rtn = DCAM_RTN_CAP_FRAME_SIZE_ERR;
				return -rtn;
			} else if (idx == DCAM_ID_2 &&
				(rect->x > DCAM2_CAP_FRAME_WIDTH_MAX ||
				rect->y > DCAM2_CAP_FRAME_HEIGHT_MAX ||
				rect->w > DCAM2_CAP_FRAME_WIDTH_MAX ||
				rect->h > DCAM2_CAP_FRAME_HEIGHT_MAX)) {
				rtn = DCAM_RTN_CAP_FRAME_SIZE_ERR;
				return -rtn;
			}

			cap_desc->cap_rect = *rect;
			tmp = rect->x | (rect->y << 16);
			DCAM_REG_WR(idx, DCAM_MIPI_CAP_START, tmp);
			tmp = (rect->x + rect->w - 1);
			tmp |= (rect->y + rect->h - 1) << 16;
			pr_info("rect start {%d, %d} size {%d, %d}\n",
				rect->x, rect->y, rect->w, rect->h);
			DCAM_REG_WR(idx, DCAM_MIPI_CAP_END, tmp);

			break;
		}
	case DCAM_CAP_IMAGE_XY_DECI:
		{
			struct dcam_cap_dec *cap_dec =
				(struct dcam_cap_dec *)param;
			reg = DCAM_MIPI_CAP_CFG;

			if (cap_dec->x_factor > DCAM_CAP_X_DECI_FAC_MAX ||
				cap_dec->y_factor > DCAM_CAP_Y_DECI_FAC_MAX) {
				rtn = DCAM_RTN_CAP_XY_DECI_ERR;
			} else {
				if (DCAM_CAP_MODE_RAWRGB ==
					cap_desc->input_format) {
					if (cap_dec->x_factor > 1
						|| cap_dec->y_factor > 1) {
						rtn = DCAM_RTN_CAP_XY_DECI_ERR;
						break;
					}
				}
				DCAM_REG_MWR(idx, reg, BIT_19 | BIT_18,
					cap_dec->y_factor << 8);
				DCAM_REG_MWR(idx, reg, BIT_17 | BIT_16,
					cap_dec->x_factor << 4);
				}
			break;
		}
	case DCAM_CAP_DATA_PATTERN:
		{
			uint32_t pattern = *(uint32_t *)param;

			if (cap_desc->input_format == DCAM_CAP_MODE_RAWRGB) {
				DCAM_REG_MWR(idx, DCAM_BAYER_INFO_CFG,
						BIT_4 | BIT_5,
						pattern << 4);
			} else if (cap_desc->input_format ==
				DCAM_CAP_MODE_YUV) {
				DCAM_REG_MWR(idx, DCAM_MIPI_CAP_CFG,
					BIT_15 | BIT_14, pattern << 14);
			} else
				rtn = DCAM_RTN_CAP_IN_PATTERN_ERR;
			break;
		}
	case DCAM_CAP_4IN1_BYPASS:
	{
		DCAM_REG_MWR(idx, DCAM_BAYER_INFO_CFG, BIT_2, 0 << 2);
		DCAM_REG_MWR(idx, DCAM_BAYER_INFO_CFG, BIT_1, 0 << 1);
		DCAM_REG_MWR(idx, DCAM_BAYER_INFO_CFG, BIT_0, 0);
		break;
	}
	case DCAM_CAP_DUAL_MODE:
	{
		uint32_t dual_cam = *(uint32_t *)param;
		struct dcam_group *dcam_group = sprd_dcam_drv_group_get();

		if (dcam_group)
			dcam_group->dual_cam = dual_cam;
		break;
	}
	case DCAM_CAP_CPHY_SEL:
	{
		uint32_t cphy_sel = *(uint32_t *)param;

		pr_info("cphy_sel = %d\n", cphy_sel);
		DCAM_REG_MWR(idx, DCAM_MIPI_CAP_CFG,
			BIT_31, cphy_sel << 31);
		break;
	}
	case DCAM_CAP_SLICE_PART:
	{
		uint32_t slice_part = *(uint32_t *)param;

		pr_info("slice_part = %d\n", slice_part);
		DCAM_REG_MWR(idx, DCAM_MIPI_CAP_CFG,
			BIT_30, slice_part << 30);
		break;
	}
	case DCAM_CAP_SLICE_MODE:
	{
		uint32_t slice_mode = *(uint32_t *)param;

		pr_info("slice_mode = %d\n", slice_mode);
		DCAM_REG_MWR(idx, DCAM_MIPI_CAP_CFG,
			BIT_29, slice_mode << 29);
		break;
	}
	case DCAM_CAP_SLICE_EN:
	{
		uint32_t slice_en = *(uint32_t *)param;

		pr_info("slice_en = %d\n", slice_en);
		DCAM_REG_MWR(idx, DCAM_MIPI_CAP_CFG,
			BIT_28, slice_en << 28);
		break;
	}
	case DCAM_CAP_FETCH_MODE_EN:
	{
		uint32_t fetch_mode_en = *(uint32_t *)param;

		pr_info("fetch_mode_en = %d\n", fetch_mode_en);
		DCAM_REG_MWR(idx, DCAM_MIPI_CAP_CFG,
			BIT_12, fetch_mode_en << 12);
		break;
	}
	default:
		rtn = DCAM_RTN_IO_ID_ERR;
		break;

	}

	return -rtn;
}
