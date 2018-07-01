/*
 *Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 *This software is licensed under the terms of the GNU General Public
 *License version 2, as published by the Free Software Foundation, and
 *may be copied, distributed, and modified under those terms.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 */

#ifndef __SPRD_DSI_H__
#define __SPRD_DSI_H__

#include <linux/of.h>
#include <linux/device.h>

#include <drm/drm_of.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_encoder_slave.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_panel.h>

#include "disp_lib.h"

#define DSI_INT_STS_NEED_SOFT_RESET	BIT(0)
#define DSI_INT_STS_NEED_HARD_RESET	BIT(1)

enum dsi_work_mode {
	DSI_MODE_CMD = 0,
	DSI_MODE_VIDEO
};

enum video_burst_mode {
	VIDEO_NON_BURST_WITH_SYNC_PULSES = 0,
	VIDEO_NON_BURST_WITH_SYNC_EVENTS,
	VIDEO_BURST_WITH_SYNC_PULSES
};

enum dsi_color_coding {
	COLOR_CODE_16BIT_CONFIG1	=  0,
	COLOR_CODE_16BIT_CONFIG2	=  1,
	COLOR_CODE_16BIT_CONFIG3	=  2,
	COLOR_CODE_18BIT_CONFIG1	=  3,
	COLOR_CODE_18BIT_CONFIG2	=  4,
	COLOR_CODE_24BIT		=  5,
	COLOR_CODE_20BIT_YCC422_LOOSELY	=  6,
	COLOR_CODE_24BIT_YCC422		=  7,
	COLOR_CODE_16BIT_YCC422		=  8,
	COLOR_CODE_30BIT		=  9,
	COLOR_CODE_36BIT		= 10,
	COLOR_CODE_12BIT_YCC420		= 11,
	COLOR_CODE_COMPRESSTION		= 12,
	COLOR_CODE_MAX
};

struct dpi_video_param {
	/* Number of lanes used to send current video */
	u8  lanes;
	/* Virtual channel number to send this video stream */
	u8  channel;
	/* burst or non-burst mode */
	u8 burst_mode;
	/* Maximum byte clock cycles for data lanes from HS to LP */
	u8 data_hs2lp;
	/* Maximum byte clock cycles for data lanes from LP to HS */
	u8 data_lp2hs;
	/* Maximum byte clock cycles for clk lanes from HS to LP */
	u8 clk_hs2lp;
	/* Maximum byte clock cycles for clk lanes from LP to HS */
	u8 clk_lp2hs;
	/* Enable receiving of ack packets */
	int frame_ack_en;
	/* Byte (lane) clock [KHz] */
	u32 byte_clk;
	/* Pixel (DPI) Clock [KHz]*/
	u32 pixel_clk;
	/* Colour coding - BPP and Pixel configuration */
	u8 coding;
	/* Is 18-bit loosely packets (valid only when BPP == 18) */
	int is_18_loosely;
	/* the polarity of dpicolorm pin - Synopsys Only */
	int color_mode_pol;
	/* the polarity of dpishutdn pin - Synopsys Only */
	int shut_down_pol;
	/* the polarity of dpidaten pin - Synopsys Only */
	int de_pol;
	/* the polarity of dpihsync pin - Synopsys Only */
	int hsync_pol;
	/* the polarity of dpivsync pin - Synopsys Only */
	int vsync_pol;
	/* Horizontal resolution or Active Pixels */
	u16 hact;
	/* Horizontal Sync Pixels */
	u16 hsync;
	/* Horizontal back porch pixels */
	u16 hbp;
	/* Total Horizontal pixels */
	u16 hline;
	/* Vertical active lines (resolution) */
	u16 vact;
	/* Vertical sync lines */
	u16 vsync;
	/* Vertical back porch lines */
	u16 vbp;
	/* Vertical front porch lines */
	u16 vfp;
};

struct edpi_video_param {
	/* virtual channel */
	u8 channel;
	/* Commands to be sent in high speed or low power */
	int lp;
	/* Colour coding - BPP and Pixel configuration */
	u8 coding;
	/* Top horizontal pixel position in the display */
	u16 hstart;
	/* Left most line position in the display */
	u16 vstart;
	/* Horizontal resolution or Active Pixels */
	u16 hact;
	/* Vertical active lines (resolution) */
	u16 vact;
	/* Whether Tearing effect should be requested */
	int te;
	/* packet size of write memory command */
	u16 pkt_size;
};

struct dsi_context;
struct dsi_core_ops {
	bool (*check_version)(struct dsi_context *ctx);
	void (*power_en)(struct dsi_context *ctx, int enable);
	void (*video_mode)(struct dsi_context *ctx);
	void (*cmd_mode)(struct dsi_context *ctx);
	bool (*is_cmd_mode)(struct dsi_context *ctx);
	void (*rx_vcid)(struct dsi_context *ctx, u8 vc);
	void (*video_vcid)(struct dsi_context *ctx, u8 vc);
	void (*dpi_video_burst_mode)(struct dsi_context *ctx, int mode);
	void (*dpi_color_coding)(struct dsi_context *ctx, int coding);
	void (*dpi_18_loosely_packet_en)(struct dsi_context *ctx, int enable);
	void (*dpi_color_mode_pol)(struct dsi_context *ctx, int active_low);
	void (*dpi_shut_down_pol)(struct dsi_context *ctx, int active_low);
	void (*dpi_hsync_pol)(struct dsi_context *ctx, int active_low);
	void (*dpi_vsync_pol)(struct dsi_context *ctx, int active_low);
	void (*dpi_data_en_pol)(struct dsi_context *ctx, int active_low);
	void (*dpi_sig_delay)(struct dsi_context *ctx, u16 byte_cycle);
	void (*dpi_hline_time)(struct dsi_context *ctx, u16 byte_cycle);
	void (*dpi_hsync_time)(struct dsi_context *ctx, u16 byte_cycle);
	void (*dpi_hbp_time)(struct dsi_context *ctx, u16 byte_cycle);
	void (*dpi_vact)(struct dsi_context *ctx, u16 lines);
	void (*dpi_vfp)(struct dsi_context *ctx, u16 lines);
	void (*dpi_vbp)(struct dsi_context *ctx, u16 lines);
	void (*dpi_vsync)(struct dsi_context *ctx, u16 lines);
	void (*dpi_hporch_lp_en)(struct dsi_context *ctx, int enable);
	void (*dpi_vporch_lp_en)(struct dsi_context *ctx, int enable);
	void (*dpi_frame_ack_en)(struct dsi_context *ctx, int enable);
	void (*dpi_null_packet_en)(struct dsi_context *ctx, int enable);
	void (*dpi_multi_packet_en)(struct dsi_context *ctx, int enable);
	void (*dpi_chunk_num)(struct dsi_context *ctx, u16 no);
	void (*dpi_null_packet_size)(struct dsi_context *ctx, u16 size);
	void (*dpi_video_packet_size)(struct dsi_context *ctx, u16 size);
	void (*edpi_max_pkt_size)(struct dsi_context *ctx, u16 size);
	void (*edpi_video_hs_en)(struct dsi_context *ctx, int enable);
	void (*tear_effect_ack_en)(struct dsi_context *ctx, int enable);
	void (*cmd_ack_request_en)(struct dsi_context *ctx, int enable);
	void (*cmd_mode_lp_cmd_en)(struct dsi_context *ctx, int enable);
	void (*video_mode_lp_cmd_en)(struct dsi_context *ctx, int enable);
	void (*set_packet_header)(struct dsi_context *ctx, u8 vc, u8 type,
							u8 wc_lsb, u8 wc_msb);
	void (*set_packet_payload)(struct dsi_context *ctx, u32 payload);
	u32 (*get_rx_payload)(struct dsi_context *ctx);
	void (*bta_en)(struct dsi_context *ctx, int enable);
	void (*eotp_rx_en)(struct dsi_context *ctx, int enable);
	void (*eotp_tx_en)(struct dsi_context *ctx, int enable);
	void (*ecc_rx_en)(struct dsi_context *ctx, int enable);
	void (*crc_rx_en)(struct dsi_context *ctx, int enable);
	bool (*is_bta_returned)(struct dsi_context *ctx);
	bool (*is_rx_payload_fifo_full)(struct dsi_context *ctx);
	bool (*is_rx_payload_fifo_empty)(struct dsi_context *ctx);
	bool (*is_tx_payload_fifo_full)(struct dsi_context *ctx);
	bool (*is_tx_payload_fifo_empty)(struct dsi_context *ctx);
	bool (*is_tx_cmd_fifo_full)(struct dsi_context *ctx);
	bool (*is_tx_cmd_fifo_empty)(struct dsi_context *ctx);
	void (*datalane_hs2lp_config)(struct dsi_context *ctx, u16 byte_cycle);
	void (*datalane_lp2hs_config)(struct dsi_context *ctx, u16 byte_cycle);
	void (*clklane_hs2lp_config)(struct dsi_context *ctx, u16 byte_cycle);
	void (*clklane_lp2hs_config)(struct dsi_context *ctx, u16 byte_cycle);
	void (*max_read_time)(struct dsi_context *ctx, u16 byte_cycle);
	void (*nc_clk_en)(struct dsi_context *ctx, int enable);
	void (*tx_escape_division)(struct dsi_context *ctx, u8 div);
	void (*timeout_clock_division)(struct dsi_context *ctx,	u8 div);
	void (*lp_rx_timeout)(struct dsi_context *ctx, u16 count);
	void (*hs_tx_timeout)(struct dsi_context *ctx, u16 count);
	void (*bta_presp_timeout)(struct dsi_context *ctx, u16 byteclk);
	void (*lp_write_presp_timeout)(struct dsi_context *ctx, u16 byteclk);
	void (*lp_read_presp_timeout)(struct dsi_context *ctx, u16 byteclk);
	void (*hs_write_presp_timeout)(struct dsi_context *ctx, u16 byteclk);
	void (*hs_read_presp_timeout)(struct dsi_context *ctx, u16 byteclk);
	u32 (*int0_status)(struct dsi_context *ctx);
	u32 (*int1_status)(struct dsi_context *ctx);
	u32 (*int2_status)(struct dsi_context *ctx);
	void (*int0_mask)(struct dsi_context *ctx, u32 mask);
	void (*int1_mask)(struct dsi_context *ctx, u32 mask);
	void (*int2_mask)(struct dsi_context *ctx, u32 mask);
};

struct dsi_glb_ops {
	int (*parse_dt)(struct dsi_context *ctx,
			struct device_node *np);
	void (*enable)(struct dsi_context *ctx);
	void (*disable)(struct dsi_context *ctx);
	void (*reset)(struct dsi_context *ctx);
	void (*power)(struct dsi_context *ctx, int enable);
};

struct dsi_context {
	/* Physical base address of controller - REQUIRED */
	unsigned long base;
	int irq0;
	int irq1;
	/* D-PHY frequency */
	u32 freq;
	/* Number of lanes connected to controller - REQUIRED */
	u8 lanes;
	/* master or slave id */
	u8 id;
	/* Maximum byte clock cycles for BTA operation - REQUIRED */
	u16 max_rd_time;
	/* Enable non coninuous clock for energy saving */
	int nc_clk_en;

	u32 int0_mask;
	u32 int1_mask;
};

struct sprd_dsi {
	struct device dev;
	struct mipi_dsi_host host;
	struct drm_encoder encoder;
	struct drm_connector connector;
	struct drm_bridge *bridge;
	struct drm_panel *panel;
	struct dsi_core_ops *core;
	struct dsi_glb_ops *glb;
	struct dsi_context ctx;
	struct drm_display_mode cur_mode;
};

extern struct list_head dsi_core_head;
extern struct list_head dsi_glb_head;

#define dsi_core_ops_register(entry) \
	disp_ops_register(entry, &dsi_core_head)
#define dsi_glb_ops_register(entry) \
	disp_ops_register(entry, &dsi_glb_head)

#define dsi_core_ops_attach(str) \
	disp_ops_attach(str, &dsi_core_head)
#define dsi_glb_ops_attach(str) \
	disp_ops_attach(str, &dsi_glb_head)

#endif /* __SPRD_DSI_H__ */
