/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
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

#ifndef _SPRD_PANEL_H_
#define _SPRD_PANEL_H_

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

enum {
	CMD_CODE_INIT = 0,
	CMD_CODE_SLEEP_IN,
	CMD_CODE_SLEEP_OUT,
	CMD_OLED_BRIGHTNESS,
	CMD_OLED_REG_LOCK,
	CMD_OLED_REG_UNLOCK,
	CMD_CODE_RESERVED0,
	CMD_CODE_RESERVED1,
	CMD_CODE_RESERVED2,
	CMD_CODE_RESERVED3,
	CMD_CODE_RESERVED4,
	CMD_CODE_RESERVED5,
	CMD_CODE_MAX,
};

enum {
	SPRD_DSI_MODE_CMD = 0,
	SPRD_DSI_MODE_VIDEO_BURST,
	SPRD_DSI_MODE_VIDEO_SYNC_PULSE,
	SPRD_DSI_MODE_VIDEO_SYNC_EVENT,
};

struct dsi_cmd_desc {
	uint8_t data_type;
	uint8_t wait;
	uint8_t wc_h;
	uint8_t wc_l;
	uint8_t payload[];
};

struct panel_info {
	/* common parameters */
	struct device_node *of_node;
	struct drm_display_mode mode;
	const void *cmds[CMD_CODE_MAX];
	int cmds_len[CMD_CODE_MAX];

	/* MIPI DSI specific parameters */
	u32 format;
	u32 lanes;
	u32 mode_flags;
	bool use_dcs;
};

struct sprd_panel {
	struct device dev;
	struct drm_panel base;
	struct mipi_dsi_device *slave;
	struct panel_info info;

	bool prepared;
	bool enabled;
};

#endif
