/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#include <linux/delay.h>
#include <linux/irqreturn.h>
//#include <video/mipi_display.h>

//#include "sprd_dsi_api.h"
#include "sprd_dsi_hal.h"

static u16 calc_bytes_per_pixel_x100(int coding)
{
	u16 Bpp_x100;

	switch (coding) {
	case COLOR_CODE_16BIT_CONFIG1:
	case COLOR_CODE_16BIT_CONFIG2:
	case COLOR_CODE_16BIT_CONFIG3:
		Bpp_x100 = 200;
		break;
	case COLOR_CODE_18BIT_CONFIG1:
	case COLOR_CODE_18BIT_CONFIG2:
		Bpp_x100 = 225;
		break;
	case COLOR_CODE_24BIT:
		Bpp_x100 = 300;
		break;
	case COLOR_CODE_COMPRESSTION:
		Bpp_x100 = 100;
		break;
	case COLOR_CODE_20BIT_YCC422_LOOSELY:
		Bpp_x100 = 250;
		break;
	case COLOR_CODE_24BIT_YCC422:
		Bpp_x100 = 300;
		break;
	case COLOR_CODE_16BIT_YCC422:
		Bpp_x100 = 200;
		break;
	case COLOR_CODE_30BIT:
		Bpp_x100 = 375;
		break;
	case COLOR_CODE_36BIT:
		Bpp_x100 = 450;
		break;
	case COLOR_CODE_12BIT_YCC420:
		Bpp_x100 = 150;
		break;
	default:
		pr_err("invalid color coding");
		Bpp_x100 = 0;
		break;
	}

	return Bpp_x100;
}

static u8 calc_video_size_step(int coding)
{
	u8 video_size_step;

	switch (coding) {
	case COLOR_CODE_16BIT_CONFIG1:
	case COLOR_CODE_16BIT_CONFIG2:
	case COLOR_CODE_16BIT_CONFIG3:
	case COLOR_CODE_18BIT_CONFIG1:
	case COLOR_CODE_18BIT_CONFIG2:
	case COLOR_CODE_24BIT:
	case COLOR_CODE_COMPRESSTION:
		return video_size_step = 1;
	case COLOR_CODE_20BIT_YCC422_LOOSELY:
	case COLOR_CODE_24BIT_YCC422:
	case COLOR_CODE_16BIT_YCC422:
	case COLOR_CODE_30BIT:
	case COLOR_CODE_36BIT:
	case COLOR_CODE_12BIT_YCC420:
		return video_size_step = 2;
	default:
		pr_err("invalid color coding");
		return 0;
	}
}

static u16 round_video_size(int coding, u16 video_size)
{
	switch (coding) {
	case COLOR_CODE_16BIT_YCC422:
	case COLOR_CODE_24BIT_YCC422:
	case COLOR_CODE_20BIT_YCC422_LOOSELY:
	case COLOR_CODE_12BIT_YCC420:
		/* round up active H pixels to a multiple of 2 */
		if ((video_size % 2) != 0)
			video_size += 1;
		break;
	default:
		break;
	}

	return video_size;
}

/*
 * unc:sprd_dsi_set_lp_clock
 * esc:in low-power mode, max PHY frequency should smaller than 20MHz,
 *     in synopsys IP, low-power clock divide from high-speed clock,
 *     this function is designed to adapt low-power clock to various
 * high-speed clock.
 *
 *     rithmetic:
 *     Fhs : high-speed frequence
 *     Flp : low-power frequence
 *     div : dsi_hal_tx_escape_division() param, byte clock unit.
 *     because of the 20MHz demand, Flp == Fhs/(div*8) <= 20 MHz,
 * here 500000 present 500MHz, so MHz equal 1000
 *     so div >= { Fhs/(20Mhz*8) == Fhs/(20000*8) == (Fhs>>4)/10000 }
 */

int sprd_dsi_init(struct sprd_dsi *dsi)
{
	int div;
	struct dsi_context *ctx = &dsi->ctx;

	dsi_hal_power_en(dsi, 0);
	dsi_hal_int0_mask(dsi, 0xffffffff);
	dsi_hal_int1_mask(dsi, 0xffffffff);
	dsi_hal_cmd_mode(dsi);
	dsi_hal_eotp_rx_en(dsi, 0);
	dsi_hal_eotp_tx_en(dsi, 0);
	dsi_hal_ecc_rx_en(dsi, 1);
	dsi_hal_crc_rx_en(dsi, 1);
	dsi_hal_bta_en(dsi, 1);
	dsi_hal_video_vcid(dsi, 0);
	dsi_hal_rx_vcid(dsi, 0);
	dsi_hal_nc_clk_en(dsi, ctx->nc_clk_en);
	dsi_hal_max_read_time(dsi, ctx->max_rd_time);

	div = ((ctx->freq >> 4) + 9999) / 10000;
	dsi_hal_tx_escape_division(dsi, div);
	pr_info("escape clk div = %d\n", div);

	dsi_hal_power_en(dsi, 1);

	return 0;
}

/**
 * Close DSI Host driver
 * - Free up resources and shutdown host controller and PHY
 * @param dsi pointer to structure holding the DSI Host core information
 * @return int
 */
int sprd_dsi_uninit(struct sprd_dsi *dsi)
{
	dsi_hal_int0_mask(dsi, 0xffffffff);
	dsi_hal_int1_mask(dsi, 0xffffffff);
	dsi_hal_power_en(dsi, 0);

	return 0;
}

/**
 * Configure DPI video interface
 * - If not in burst mode, it will compute the video and null packet sizes
 * according to necessity
 * - Configure timers for data lanes and/or clock lane to return to LP when
 * bandwidth is not filled by data
 * @param dsi pointer to structure holding the DSI Host core information
 * @param param pointer to video stream-to-send information
 * @return error code
 */
int sprd_dsi_dpi_video(struct sprd_dsi *dsi, struct dpi_video_param *param)
{
	u16 Bpp_x100 = 0;
	u16 video_size = 0;
	u32 ratio_x1000 = 0;
	u16 null_pkt_size = 0;
	u8 video_size_step = 1;
	u32 hs_to = 0;
	u32 total_bytes = 0;
	u32 bytes_per_chunk = 0;
	u32 chunks = 0;
	u32 bytes_left = 0;
	u32 chunk_overhead = 0;
	const u8 pkt_header = 6;
	int div = 0;

	video_size = round_video_size(param->coding, param->hact);
	Bpp_x100 = calc_bytes_per_pixel_x100(param->coding);
	video_size_step = calc_video_size_step(param->coding);
	ratio_x1000 = param->byte_clk * 1000 / param->pixel_clk;

	dsi_hal_power_en(dsi, 0);
	dsi_hal_dpi_frame_ack_en(dsi, param->frame_ack_en);
	dsi_hal_dpi_color_coding(dsi, param->coding);
	dsi_hal_dpi_video_burst_mode(dsi, param->burst_mode);
	dsi_hal_dpi_sig_delay(dsi, 95 * param->hline * ratio_x1000 / 100000);
	dsi_hal_dpi_hline_time(dsi, param->hline * ratio_x1000 / 1000);
	dsi_hal_dpi_hsync_time(dsi, param->hsync * ratio_x1000 / 1000);
	dsi_hal_dpi_hbp_time(dsi, param->hbp * ratio_x1000 / 1000);
	dsi_hal_dpi_vact(dsi, param->vact);
	dsi_hal_dpi_vfp(dsi, param->vfp);
	dsi_hal_dpi_vbp(dsi, param->vbp);
	dsi_hal_dpi_vsync(dsi, param->vsync);
	dsi_hal_dpi_hporch_lp_en(dsi, 1);
	dsi_hal_dpi_vporch_lp_en(dsi, 1);
	dsi_hal_dpi_hsync_pol(dsi, param->hsync_pol);
	dsi_hal_dpi_vsync_pol(dsi, param->vsync_pol);
	dsi_hal_dpi_data_en_pol(dsi, param->de_pol);
	dsi_hal_dpi_color_mode_pol(dsi, param->color_mode_pol);
	dsi_hal_dpi_shut_down_pol(dsi, param->shut_down_pol);

	dsi_hal_datalane_hs2lp_config(dsi, param->data_hs2lp);
	dsi_hal_datalane_lp2hs_config(dsi, param->data_lp2hs);
	dsi_hal_clklane_hs2lp_config(dsi, param->clk_hs2lp);
	dsi_hal_clklane_lp2hs_config(dsi, param->clk_lp2hs);

	hs_to = (param->hline * param->vact) + (2 * Bpp_x100) / 100;
	for (div = 0x80; (div < hs_to) && (div > 2); div--) {
		if ((hs_to % div) == 0) {
			dsi_hal_timeout_clock_division(dsi, div);
			dsi_hal_lp_rx_timeout(dsi, hs_to / div);
			dsi_hal_hs_tx_timeout(dsi, hs_to / div);
			break;
		}
	}

	if (param->burst_mode == VIDEO_BURST_WITH_SYNC_PULSES) {
		dsi_hal_dpi_video_packet_size(dsi, video_size);
		dsi_hal_dpi_null_packet_size(dsi, 0);
		dsi_hal_dpi_chunk_num(dsi, 0);
	} else {
		/* non burst transmission */
		null_pkt_size = 0;

		/* bytes to be sent - first as one chunk */
		bytes_per_chunk = param->hact * Bpp_x100 / 100 + pkt_header;

		/* hline total bytes from the DPI interface */
		total_bytes = (param->hline - param->hsync - param->hbp) *
					ratio_x1000 / param->lanes / 1000;

		/* check if the pixels actually fit on the DSI link */
		if (total_bytes < bytes_per_chunk) {
			pr_err("current resolution can not be set\n");
			return -1;
		}

		chunk_overhead = total_bytes - bytes_per_chunk;

		/* overhead higher than 1 -> enable multi packets */
		if (chunk_overhead > 1) {

			/* multi packets */
			for (video_size = video_size_step;
			     video_size < param->hact;
			     video_size += video_size_step) {

				if (param->hact * 1000 / video_size % 1000)
					continue;

				chunks = param->hact / video_size;
				bytes_per_chunk = Bpp_x100 * video_size / 100
						  + pkt_header;
				if (total_bytes >= (bytes_per_chunk * chunks)) {
					bytes_left = total_bytes -
						     bytes_per_chunk * chunks;
					break;
				}
			}

			/* prevent overflow (unsigned - unsigned) */
			if (bytes_left > (pkt_header * chunks)) {
				null_pkt_size = (bytes_left -
						pkt_header * chunks) / chunks;
				/* avoid register overflow */
				if (null_pkt_size > 1023)
					null_pkt_size = 1023;
			}

		} else {

			/* single packet */
			chunks = 1;

			/* must be a multiple of 4 except 18 loosely */
			for (video_size = param->hact;
			    (video_size % video_size_step) != 0;
			     video_size++)
				;
		}

		dsi_hal_dpi_video_packet_size(dsi, video_size);
		dsi_hal_dpi_null_packet_size(dsi, null_pkt_size);
		dsi_hal_dpi_chunk_num(dsi, chunks);
	}

	dsi_hal_int0_mask(dsi, dsi->ctx.int0_mask);
	dsi_hal_int1_mask(dsi, dsi->ctx.int1_mask);
	dsi_hal_power_en(dsi, 1);

	return 0;
}

int sprd_dsi_edpi_video(struct sprd_dsi *dsi, struct edpi_video_param *param)
{
	const u32 fifo_depth = 1096;
	const u32 word_length = 4;
	u32 Bpp_x100;
	u32 max_fifo_len;

	Bpp_x100 = calc_bytes_per_pixel_x100(param->coding);
	max_fifo_len = word_length * fifo_depth * 100 / Bpp_x100;

	dsi_hal_power_en(dsi, 0);
	dsi_hal_dpi_color_coding(dsi, param->coding);
	dsi_hal_tear_effect_ack_en(dsi, param->te);

	if (max_fifo_len > param->hact)
		dsi_hal_edpi_max_pkt_size(dsi, param->hact);
	else
		dsi_hal_edpi_max_pkt_size(dsi, max_fifo_len);

	dsi_hal_int0_mask(dsi, dsi->ctx.int0_mask);
	dsi_hal_int1_mask(dsi, dsi->ctx.int1_mask);
	dsi_hal_power_en(dsi, 1);

	return 0;
}

int sprd_dsi_htime_update(struct sprd_dsi *dsi, struct dpi_video_param *param)
{
	u32 ratio_x1000 = 0;

	ratio_x1000 = param->byte_clk * 1000 / param->pixel_clk;

	dsi_hal_power_en(dsi, 0);
	dsi_hal_dpi_sig_delay(dsi, 95 * param->hline * ratio_x1000 / 100000);
	dsi_hal_dpi_hline_time(dsi, param->hline * ratio_x1000 / 1000);
	dsi_hal_dpi_hsync_time(dsi, param->hsync * ratio_x1000 / 1000);
	dsi_hal_dpi_hbp_time(dsi, param->hbp * ratio_x1000 / 1000);
	dsi_hal_power_en(dsi, 1);

	return 0;
}

/**
 * Send a packet on the generic interface
 * @param dsi pointer to structure holding the DSI Host core information
 * @param vc destination virtual channel
 * @param data_type type of command, inserted in first byte of header
 * @param params byte array of command parameters
 * @param param_length length of the above array
 * @return error code
 * @note this function has an active delay to wait for the buffer to clear.
 * The delay is limited to:
 * (param_length / 4) x DSIH_FIFO_ACTIVE_WAIT x register access time
 * @note the controller restricts the sending of .
 * This function will not be able to send Null and Blanking packets due to
 *  controller restriction
 */
int sprd_dsi_wr_pkt(struct sprd_dsi *dsi, u8 vc, u8 type,
			const u8 *param, u16 len)
{
	int i = 0;
	int j = 0;
	u32 payload;
	u8 wc_lsbyte, wc_msbyte;

	if (vc > 3)
		return -EINVAL;

	/* 1st: for long packet, must config payload first */
	if (!dsi_hal_wait_tx_payload_fifo_empty(dsi))
		return -1;

	if (len > 2) {
		for (i = 0; i < len; i += j) {
			payload = 0;
			for (j = 0; (j < 4) && ((j + i) < (len)); j++)
				payload |= param[i + j] << (j * 8);

			dsi_hal_set_packet_payload(dsi, payload);
		}
		wc_lsbyte = len & 0xff;
		wc_msbyte = len >> 8;
	} else {
		wc_lsbyte = (len > 0) ? param[0] : 0;
		wc_msbyte = (len > 1) ? param[1] : 0;
	}

	/* 2nd: then set packet header */
	if (!dsi_hal_wait_tx_cmd_fifo_empty(dsi))
		return -EINVAL;

	dsi_hal_set_packet_header(dsi, vc, type, wc_lsbyte, wc_msbyte);

	return 0;
}


/**
 * Send READ packet to peripheral using the generic interface
 * This will force command mode and stop video mode (because of BTA)
 * @param dsi pointer to structure holding the DSI Host core information
 * @param vc destination virtual channel
 * @param data_type generic command type
 * @param lsb_byte first command parameter
 * @param msb_byte second command parameter
 * @param bytes_to_read no of bytes to read (expected to arrive at buffer)
 * @param read_buffer pointer to 8-bit array to hold the read buffer words
 * return read_buffer_length
 * @note this function has an active delay to wait for the buffer to clear.
 * The delay is limited to 2 x DSIH_FIFO_ACTIVE_WAIT
 * (waiting for command buffer, and waiting for receiving)
 * @note this function will enable BTA
 */
int sprd_dsi_rd_pkt(struct sprd_dsi *dsi, u8 vc, u8 type,
			u8 msb_byte, u8 lsb_byte,
			u8 *buffer, u8 bytes_to_read)
{
	int i;
	int count = 0;
	u32 temp;

	if (vc > 3)
		return -EINVAL;

	/* 1st: send read command to peripheral */
	if (!dsi_hal_wait_tx_cmd_fifo_empty(dsi))
		return -EINVAL;

	dsi_hal_set_packet_header(dsi, vc, type, lsb_byte, msb_byte);

	/* 2nd: wait peripheral response completed */
	dsi_hal_wait_rd_resp_completed(dsi);

	/* 3rd: get data from rx payload fifo */
	if (dsi_hal_is_rx_payload_fifo_empty(dsi)) {
		pr_err("rx payload fifo empty\n");
		return -EINVAL;
	}

	for (i = 0; i < 100; i++) {
		temp = dsi_hal_get_rx_payload(dsi);

		if (count < bytes_to_read)
			buffer[count++] = temp & 0xff;
		if (count < bytes_to_read)
			buffer[count++] = (temp >> 8) & 0xff;
		if (count < bytes_to_read)
			buffer[count++] = (temp >> 16) & 0xff;
		if (count < bytes_to_read)
			buffer[count++] = (temp >> 24) & 0xff;

		if (dsi_hal_is_rx_payload_fifo_empty(dsi))
			return count;
	}

	pr_err("read too many buffers\n");
	return -EINVAL;
}

void sprd_dsi_set_work_mode(struct sprd_dsi *dsi, u8 mode)
{
	if (mode == DSI_MODE_CMD)
		dsi_hal_cmd_mode(dsi);
	else
		dsi_hal_video_mode(dsi);
}

int sprd_dsi_get_work_mode(struct sprd_dsi *dsi)
{
	if (dsi_hal_is_cmd_mode(dsi))
		return DSI_MODE_CMD;
	else
		return DSI_MODE_VIDEO;
}

void sprd_dsi_lp_cmd_enable(struct sprd_dsi *dsi, int enable)
{
	if (dsi_hal_is_cmd_mode(dsi))
		dsi_hal_cmd_mode_lp_cmd_en(dsi, enable);
	else
		dsi_hal_video_mode_lp_cmd_en(dsi, enable);
}

void sprd_dsi_state_reset(struct sprd_dsi *dsi)
{
	dsi_hal_power_en(dsi, 0);
	udelay(100);
	dsi_hal_power_en(dsi, 1);
}

u32 sprd_dsi_int_status(struct sprd_dsi *dsi, int index)
{
	u32 status = 0;

	switch (index) {
	case 0:
		status = dsi_hal_int0_status(dsi);
		break;

	case 1:
		status = dsi_hal_int1_status(dsi);
		break;

	case 2:
		status = dsi_hal_int2_status(dsi);
		break;

	default:
		break;
	}

	return status;
}

void sprd_dsi_int_mask(struct sprd_dsi *dsi, int index)
{
	switch (index) {
	case 0:
		dsi_hal_int0_mask(dsi, dsi->ctx.int0_mask);
		break;

	case 1:
		dsi_hal_int1_mask(dsi, dsi->ctx.int1_mask);
		break;

	default:
		break;
	}
}
