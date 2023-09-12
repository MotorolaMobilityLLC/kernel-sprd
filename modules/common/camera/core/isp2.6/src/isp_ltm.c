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

#include "isp_ltm.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_LTM: %d %d %s : "fmt, current->pid, __LINE__, __func__

#define ISP_LTM_TIMEOUT         msecs_to_jiffies(100)

/*
 * 1. The static of preview frame N can be applied to another preview frame N+1
 * 2. The static of preview frame N can be applied to capture frame N
 *
 * 3. histogram statics support 128 bins (size 16 bits), Max statics is 65535 ?
 * 4. ROI crop, max support is 240 x 180
 * 5. Horizen (8, 6, 4, 2), no limitation in vertical
 * 6. Min tile is 128 x 20
 * 7. Max tile is 65536 (tile_height x tile_width)
 */
#define BIN_NUM_BIT             7
#define TILE_NUM_MIN            4
#define TILE_NUM_MAX            8
#define TILE_MAX_SIZE           (1 << 16)
#define TILE_WIDTH_MIN          160

/*
 * row : 2, 4, 8
 * col : 2, 4, 8
 */
#define MAX_TILE                64

/*
 * LTM Share ctx for pre / cap
 */
static struct isp_ltm_sync s_rgb_ltm_sync[LTM_ID_MAX];
static struct isp_ltm_sync s_yuv_ltm_sync[LTM_ID_MAX];

static void ispltm_sync_status_set(void *handle, int status)
{
	struct isp_ltm_ctx_desc *ltm_ctx = NULL;
	struct isp_ltm_sync *sync = NULL;

	if (!handle) {
		pr_err("fail to get invalid ptr\n");
		return;
	}

	ltm_ctx = (struct isp_ltm_ctx_desc *)handle;
	sync = ltm_ctx->sync;
	mutex_lock(&sync->share_mutex);
	if (ltm_ctx->mode == MODE_LTM_PRE) {
		sync->pre_ctx_status = status;
		sync->pre_cid = ltm_ctx->ctx_id;
	} else {
		sync->cap_ctx_status = status;
		sync->cap_cid = ltm_ctx->ctx_id;
	}
	mutex_unlock(&sync->share_mutex);
}

static int ispltm_sync_status_get(void *handle, int type)
{
	int status = 0;
	struct isp_ltm_ctx_desc *ltm_ctx = NULL;
	struct isp_ltm_sync *sync = NULL;

	if (!handle) {
		pr_err("fail to get invalid ptr\n");
		return -EFAULT;
	}

	ltm_ctx = (struct isp_ltm_ctx_desc *)handle;
	sync = ltm_ctx->sync;

	mutex_lock(&sync->share_mutex);
	if (type == MODE_LTM_PRE)
		status = sync->pre_ctx_status;
	else
		status = sync->cap_ctx_status;
	mutex_unlock(&sync->share_mutex);

	return status;
}

static int ispltm_sync_completion_set(void *handle, int frame_idx)
{
	struct isp_ltm_ctx_desc *ltm_ctx = NULL;
	struct isp_ltm_sync *sync = NULL;

	if (!handle) {
		pr_err("fail to get invalid ptr\n");
		return -EFAULT;
	}

	ltm_ctx = (struct isp_ltm_ctx_desc *)handle;
	sync = ltm_ctx->sync;
	atomic_set(&sync->wait_completion, frame_idx);

	return 0;
}

static int ispltm_sync_completion_get(void *handle)
{
	int idx = 0;
	struct isp_ltm_ctx_desc *ltm_ctx = NULL;
	struct isp_ltm_sync *sync = NULL;

	if (!handle) {
		pr_err("fail to get invalid ptr\n");
		return -EFAULT;
	}

	ltm_ctx = (struct isp_ltm_ctx_desc *)handle;
	sync = ltm_ctx->sync;

	idx = atomic_read(&sync->wait_completion);
	return idx;
}

static int ispltm_sync_completion_done(void *handle)
{
	int idx = 0;
	struct isp_ltm_ctx_desc *ltm_ctx = NULL;
	struct isp_ltm_sync *sync = NULL;

	if (!handle) {
		pr_err("fail to get invalid ptr\n");
		return -EFAULT;
	}

	ltm_ctx = (struct isp_ltm_ctx_desc *)handle;
	sync = ltm_ctx->sync;

	idx = atomic_read(&sync->wait_completion);
	if (idx) {
		atomic_set(&sync->wait_completion, 0);
		complete(&sync->share_comp);
	}

	return idx;
}

static void ispltm_sync_fid_set(void *handle)
{
	struct isp_ltm_ctx_desc *ltm_ctx = NULL;
	struct isp_ltm_sync *sync = NULL;

	if (!handle) {
		pr_err("fail to get invalid ptr\n");
		return;
	}

	ltm_ctx = (struct isp_ltm_ctx_desc *)handle;
	sync = ltm_ctx->sync;
	atomic_set(&sync->pre_fid, ltm_ctx->fid);
}

static int ispltm_sync_config_set(struct isp_ltm_ctx_desc *ctx,
		struct isp_ltm_hists *hists)
{
	struct isp_ltm_sync *sync = NULL;

	if (!ctx || !hists) {
		pr_err("fail to get invalid ptr %p\n", ctx);
		return -EFAULT;
	}

	sync = ctx->sync;
	if (ctx->mode != MODE_LTM_PRE) {
		pr_err("fail to set share ctx, only pre support, except ctx[%d]\n",
			ctx->mode);
		return 0;
	}

	mutex_lock(&sync->share_mutex);
	sync->pre_hist_bypass = hists->bypass;
	sync->pre_frame_h = ctx->frame_height_stat;
	sync->pre_frame_w = ctx->frame_width_stat;
	sync->tile_num_x_minus = hists->tile_num_x_minus;
	sync->tile_num_y_minus = hists->tile_num_y_minus;
	sync->tile_width = hists->tile_width;
	sync->tile_height = hists->tile_height;
	mutex_unlock(&sync->share_mutex);

	return 0;
}

static int ispltm_sync_config_get(struct isp_ltm_ctx_desc *ctx,
		struct isp_ltm_hists *hists)
{
	struct isp_ltm_sync *sync = NULL;

	if (!ctx || !hists) {
		pr_err("fail to get invalid ptr %p\n", ctx);
		return -EFAULT;
	}

	sync = ctx->sync;
	if (ctx->mode != MODE_LTM_CAP) {
		pr_err("fail to set share ctx, only cap support, except ctx[%d]\n", ctx->mode);
		return 0;
	}

	mutex_lock(&sync->share_mutex);
	ctx->frame_height_stat = sync->pre_frame_h;
	ctx->frame_width_stat = sync->pre_frame_w;
	hists->tile_num_x_minus = sync->tile_num_x_minus;
	hists->tile_num_y_minus = sync->tile_num_y_minus;
	hists->tile_width = sync->tile_width;
	hists->tile_height = sync->tile_height;
	mutex_unlock(&sync->share_mutex);

	return 0;
}

static void ispltm_sync_ctx_init(struct isp_ltm_sync *sync)
{
	if (!sync) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	sync->pre_ctx_status = 0;
	sync->cap_ctx_status = 0;
	sync->pre_cid = 0;
	sync->cap_cid = 0;
	sync->pre_update = 0;
	sync->cap_update = 0;
	sync->pre_hist_bypass = 1;
	sync->pre_frame_h = 0;
	sync->pre_frame_w = 0;
	sync->cap_frame_h = 0;
	sync->cap_frame_w = 0;
	sync->tile_num_x_minus = 0;
	sync->tile_num_y_minus = 0;
	sync->tile_width = 0;
	sync->tile_height = 0;
	atomic_set(&sync->wait_completion, 0);
	init_completion(&sync->share_comp);
	mutex_init(&sync->share_mutex);
}

static void ispltm_sync_ctx_deinit(struct isp_ltm_sync *sync)
{
	if (!sync) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	sync->pre_ctx_status = 0;
	sync->cap_ctx_status = 0;
	sync->pre_cid = 0;
	sync->cap_cid = 0;
	sync->pre_update = 0;
	sync->cap_update = 0;
	sync->pre_frame_h = 0;
	sync->pre_frame_w = 0;
	sync->cap_frame_h = 0;
	sync->cap_frame_w = 0;
	sync->tile_num_x_minus = 0;
	sync->tile_num_y_minus = 0;
	sync->tile_width = 0;
	sync->tile_height = 0;
}

static void ispltm_sync_status_clear(void *handle)
{
	struct isp_ltm_ctx_desc *ltm_ctx = NULL;
	struct isp_ltm_sync *sync = NULL;

	if (!handle) {
		pr_err("fail to get invalid ptr\n");
		return;
	}

	ltm_ctx = (struct isp_ltm_ctx_desc *)handle;
	sync = ltm_ctx->sync;
	mutex_lock(&sync->share_mutex);
	sync->pre_hist_bypass = 1;
	mutex_unlock(&sync->share_mutex);
}


/*
 * LTM logical and algorithm
 */

/*
 * Input:
 * frame size: x, y
 *
 * Output:
 * frame size after binning: x, y
 * binning factor
 *
 * Notes:
 * Sharkl5 ONLY suppout 1/2 binning
 *
 */
static int ispltm_binning_factor_calc(ltm_param_t *histo)
{
	int ret = 0;
	int min_tile_num = 0;
	int set_tile_num = 0;
	int frame_size = 0;
	int binning_factor = 0;
	int pow_factor = 0;

	frame_size = histo->frame_width * histo->frame_height;
	/*
	 * min_tile_num = (uint8)ceil(
	 * (float)(frame_width*frame_height)/TILE_MAX_SIZE);
	 */
	min_tile_num = (frame_size + TILE_MAX_SIZE - 1) / TILE_MAX_SIZE;
	set_tile_num = histo->tile_num_x * histo->tile_num_y;

	/*
	 * binning_factor = (uint8)ceil(MAX(log(min_tile_num/64.0)/log(4.0),0));
	 */
	if (min_tile_num <= set_tile_num) {
		binning_factor = 0;
		pow_factor = 1; /* pow(2.0, binning_factor) */
	} else if (min_tile_num <= set_tile_num * 4) {
		binning_factor = 1;
		pow_factor = 2; /* pow(2.0, binning_factor) */
	} else {
		BUG_ON(0);
	}

	/*
	 * frame_width  = frame_width /(2*(uint16)pow(2.0, binning_factor)) *2;
	 * frame_height = frame_height/(2*(uint16)pow(2.0, binning_factor)) *2;
	 */
	pr_debug("B binning_factor[%d], pow_factor[%d], frame_width[%d], frame_height[%d]\n",
		binning_factor, pow_factor, histo->frame_width, histo->frame_height);
	if (pow_factor != 0) {
		histo->frame_width = histo->frame_width / (2 * pow_factor) * 2;
		histo->frame_height = histo->frame_height / (2 * pow_factor) * 2;
	}
	histo->binning_en = binning_factor;
	pr_debug("A binning_factor[%d], pow_factor[%d], frame_width[%d], frame_height[%d]\n",
		binning_factor, pow_factor, histo->frame_width, histo->frame_height);

	return ret;
}

static void ispltm_rgb_map_dump_data_rtl(ltm_param_t *param_map,
				uint32_t *img_info,
				ltm_map_rtl_t *param_map_rtl)
{
	int temp;
	int tile_index_xs, tile_index_xe;
	int tile_index_ys, tile_index_ye;
	int tile_1st_xs, tile_1st_ys;
	uint8_t tile_x_num_out, tile_y_num_out;
	uint8_t tile_left_flag = 0, tile_right_flag = 0;
	uint16_t tile0_start_x, tile0_start_y, tileX_start_x;
	uint16_t tile1_start_x, tile1_start_y;
	int img_tile1_xs_offset, img_tile1_ys_offset;

	/* slice infomation */
	int img_start_x = img_info[0];
	int img_start_y = img_info[1];
	int img_end_x = img_info[2];
	int img_end_y = img_info[3];

	/* frame infomation */
	uint8_t cropUp = param_map->cropUp;
	/* uint8_t cropDown = param_map->cropDown; */
	uint8_t cropLeft = param_map->cropLeft;
	/* uint8_t cropRight = param_map->cropRight; */
	uint8_t tile_num_x = param_map->tile_num_x;
	uint8_t tile_num_y = param_map->tile_num_y;
	uint16_t tile_width = param_map->tile_width;
	uint16_t tile_height = param_map->tile_height;

	tile_index_xs = (img_start_x + tile_width / 2 - cropLeft) / tile_width - 1;
	if (tile_index_xs < 0)
		tile_index_xs = 0;
	tile_index_xe = (img_end_x + tile_width / 2 - cropLeft) / tile_width;
	if (tile_index_xe > tile_num_x - 1)
		tile_index_xe = tile_num_x - 1;
	tile_x_num_out = tile_index_xe - tile_index_xs + 1;

	tile_index_ys = (img_start_y + tile_height / 2 - cropUp) / tile_height - 1;
	if (tile_index_ys < 0)
		tile_index_ys = 0;
	tile_index_ye = (img_end_y + tile_height / 2 - cropUp) / tile_height;
	if (tile_index_ye > tile_num_y - 1)
		tile_index_ye = tile_num_y - 1;
	tile_y_num_out = tile_index_ye - tile_index_ys + 1;

	tile_1st_xs = (img_start_x - cropLeft) / tile_width;
	if (tile_1st_xs < 0)
		tile_1st_xs = 0;
	if (tile_1st_xs > tile_num_x - 1)
		tile_1st_xs = tile_num_x - 1;
	pr_debug("img_start_x[%d], cropLeft[%d], tile_width[%d], tile_1st_xs[%d]\n",
		img_start_x, cropLeft, tile_width, tile_1st_xs);

	tile_1st_ys = (img_start_y - cropUp) / tile_height;
	if (tile_1st_ys < 0)
		tile_1st_ys = 0;
	if (tile_1st_ys > tile_num_y - 1)
		tile_1st_ys = tile_num_y - 1;
	pr_debug("img_start_y[%d], cropUp[%d], tile_height[%d], tile_1st_ys[%d]\n",
		img_start_y, cropUp, tile_height, tile_1st_ys);

	tile1_start_x = tile_1st_xs * tile_width + cropLeft;
	tile1_start_y = tile_1st_ys * tile_height + cropUp;
	img_tile1_xs_offset = img_start_x - tile1_start_x;
	img_tile1_ys_offset = img_start_y - tile1_start_y;
	pr_debug("tile_1st_xs[%d], cropLeft[%d], tile_width[%d], img_start_x[%d], tile1_start_x[%d], img_tile1_xs_offset[%d]\n",
		tile_1st_xs, cropLeft, tile_width, img_start_x, tile1_start_x, img_tile1_xs_offset);

	tile0_start_x = tile_index_xs * tile_width + cropLeft;
	tile0_start_y = tile_index_ys * tile_height + cropUp;

	tileX_start_x = tile_index_xe * tile_width + cropLeft;
	temp = img_start_x - (int)tile0_start_x;
	if ((temp >= tile_width) && (temp < tile_width * 3 / 2))
		tile_left_flag = 1;
	temp = (int)tileX_start_x - img_end_x;
	if ((temp > 0) && (temp <= tile_width / 2))
		tile_right_flag = 1;

	/* output parameters for rtl */
	param_map_rtl->tile_index_xs = tile_index_xs;
	param_map_rtl->tile_index_ys = tile_index_ys;
	param_map_rtl->tile_index_xe = tile_index_xe;
	param_map_rtl->tile_index_ye = tile_index_ye;
	param_map_rtl->tile_x_num_rtl = tile_x_num_out - 1;
	param_map_rtl->tile_y_num_rtl = tile_y_num_out - 1;
	param_map_rtl->tile_width_rtl = tile_width;
	param_map_rtl->tile_height_rtl = tile_height;
	param_map_rtl->tile_size_pro_rtl = tile_width * tile_height;
	param_map_rtl->tile_start_x_rtl = img_tile1_xs_offset;
	param_map_rtl->tile_start_y_rtl = img_tile1_ys_offset;
	param_map_rtl->tile_left_flag_rtl = tile_left_flag;
	param_map_rtl->tile_right_flag_rtl = tile_right_flag;
}

static int ispltm_histo_param_calc(ltm_param_t *param_histo, uint32_t alignment)
{
	uint32_t max_tile_col, min_tile_row;
	uint32_t tile_num_x, tile_num_y;
	uint32_t cropRows, cropCols, cropUp, cropLeft, cropRight, cropDown;
	uint32_t min_tile_width, max_tile_height, tile_width, tile_height;
	uint32_t clipLimit_min, clipLimit;
	uint32_t strength = param_histo->strength;
	uint32_t frame_width = param_histo->frame_width;
	uint32_t frame_height = param_histo->frame_height;
        uint32_t calculate_times = 0;

	ispltm_binning_factor_calc(param_histo);

	frame_width = param_histo->frame_width;
	frame_height = param_histo->frame_height;

	if (param_histo->tile_num_auto) {
		int v_ceil = 0;
		int tmp = 0;

		max_tile_col = MAX(MIN(frame_width / (TILE_WIDTH_MIN * 2) * 2,
				TILE_NUM_MAX), TILE_NUM_MIN);
		min_tile_width = frame_width / (max_tile_col * alignment) * alignment;
		max_tile_height = TILE_MAX_SIZE / (min_tile_width * 2) * 2;
		/*
		 * min_tile_row = (uint8)MAX(MIN(ceil((float)frame_height /
		 *  max_tile_height), TILE_NUM_MAX), TILE_NUM_MIN);
		 */
		v_ceil = (frame_height + max_tile_height - 1) / max_tile_height;
		min_tile_row = MAX(MIN(v_ceil, TILE_NUM_MAX), TILE_NUM_MIN);

		tile_num_y = (min_tile_row / 2) * 2;
		tile_num_x = MIN(MAX(((tile_num_y * frame_width / frame_height) / 2) * 2,
				TILE_NUM_MIN), max_tile_col);

		tile_width = frame_width / (alignment * tile_num_x) * alignment;
		tile_height = frame_height / (2 * tile_num_y) * 2;

		while (tile_width * tile_height >= TILE_MAX_SIZE) {
			tile_num_y = MIN(MAX(tile_num_y + 2, TILE_NUM_MIN), TILE_NUM_MAX);
			tmp = ((tile_num_y * frame_width / frame_height) / 2) * 2;
			tile_num_x = MIN(MAX(tmp, TILE_NUM_MIN), max_tile_col);

			tile_width = frame_width / (alignment * tile_num_x) * alignment;
			tile_height = frame_height / (2 * tile_num_y) * 2;
                        calculate_times++;
                        if (calculate_times > 2) {
                            tile_num_y = 8;
                            tile_num_x = 8;
                            tile_width = frame_width / (4 * tile_num_x) *4;
                            tile_height = frame_height / (2 * tile_num_y) *2;
                            break;
                        }
		}
	} else {
		tile_num_x = param_histo->tile_num_x;
		tile_num_y = param_histo->tile_num_y;
		tile_width = frame_width / (alignment * tile_num_x) * alignment;
		tile_height = frame_height / (2 * tile_num_y) * 2;
	}

	cropRows = frame_height - tile_height * tile_num_y;
	cropCols = frame_width - tile_width * tile_num_x;
	cropUp = cropRows >> 1;
	cropDown = cropRows >> 1;
	cropLeft = cropCols >> 1;
	cropRight = cropCols >> 1;

	clipLimit_min = (tile_width * tile_height) >> BIN_NUM_BIT;
	clipLimit = clipLimit_min + ((clipLimit_min * strength) >> 3);

	/* update patameters */
	param_histo->cropUp = cropUp;
	param_histo->cropDown = cropDown;
	param_histo->cropLeft = cropLeft;
	param_histo->cropRight = cropRight;
	param_histo->cropRows = cropRows;
	param_histo->cropCols = cropCols;
	param_histo->tile_width = tile_width;
	param_histo->tile_height = tile_height;
	param_histo->frame_width = frame_width;
	param_histo->frame_height = frame_height;
	param_histo->clipLimit = clipLimit;
	param_histo->clipLimit_min = clipLimit_min;
	param_histo->tile_num_x = tile_num_x;
	param_histo->tile_num_y = tile_num_y;
	param_histo->tile_size = tile_width * tile_height;

	return 0;
}

static int ispltm_histo_config_gen(struct isp_ltm_ctx_desc *ctx,
			struct isp_ltm_stat_info *tuning)
{
	int ret = 0;
	int idx = 0;
	struct isp_ltm_hist_param hist_param = {0};
	struct isp_ltm_hist_param *param = &hist_param;
	struct isp_ltm_hists *hists = &ctx->hists;

	/* Check bypass condition */
	param->bypass = tuning->bypass || hists->bypass;

	pr_debug("ltm hist, ctx bypass %d, tuning bypass %d\n", param->bypass, tuning->bypass);
	if (param->bypass) {
		hists->bypass = 1;
		/* set value to 0, preview case
		 * let map block will be disable when next frame
		 */
		if (ctx->mode == MODE_LTM_PRE) {
			ctx->frame_width = 0;
			ctx->frame_height = 0;
		}
		return 0;
	}

	param->strength = tuning->strength;
	param->channel_sel = tuning->channel_sel;
	param->region_est_en = tuning->region_est_en;
	param->text_point_thres = tuning->text_point_thres;
	param->text_proportion = tuning->ltm_text.textture_proporion;
	param->tile_num_auto = tuning->tile_num_auto;
	param->tile_num_x = tuning->tile_num.tile_num_x;
	param->tile_num_y = tuning->tile_num.tile_num_y;
	param->frame_height = ctx->frame_height;
	param->frame_width = ctx->frame_width;
	pr_debug("tunning: strength %d, channel_sel %d, region_est_en %d, text_point_thres %d, textture_proporion %d, num_auto %d, num_x %d, num_y %d\n",
			tuning->strength, tuning->channel_sel, tuning->region_est_en, tuning->text_point_thres, tuning->ltm_text.textture_proporion,
			tuning->tile_num_auto, tuning->tile_num.tile_num_x, tuning->tile_num.tile_num_y);
	pr_debug("frame height %d, width %d\n",  ctx->frame_height, ctx->frame_width);

	ispltm_histo_param_calc(param, ISP_LTM_ALIGNMENT);

	hists->bypass = param->bypass;
	hists->channel_sel = param->channel_sel;
	hists->binning_en = param->binning_en;
	hists->region_est_en = param->region_est_en;
	hists->buf_sel = 0;
	hists->buf_full_mode = 0;
	hists->roi_start_x = param->cropLeft;
	hists->roi_start_y = param->cropUp;
	hists->tile_width = param->tile_width;
	hists->tile_num_x_minus = param->tile_num_x - 1;
	hists->tile_height = param->tile_height;
	hists->tile_num_y_minus = param->tile_num_y - 1;

	if ((hists->tile_width * hists->tile_height >
		LTM_MAX_TILE_RANGE) ||
		hists->tile_width < LTM_MIN_TILE_WIDTH ||
		hists->tile_height < LTM_MIN_TILE_HEIGHT ||
		hists->roi_start_x > LTM_MAX_ROI_X ||
		hists->roi_start_y > LTM_MAX_ROI_Y)
		hists->bypass = 1;

	if (hists->bypass) {
		pr_debug("tile_width %d, tile_height %d, roi_start_x %d, roi_start_y %d\n",
			hists->tile_width, hists->tile_height, hists->roi_start_x, hists->roi_start_y);
		if (ctx->mode == MODE_LTM_PRE) {
			ctx->frame_width = 0;
			ctx->frame_height = 0;
		}
		return 0;
	}

	if (!ctx->buf_info[idx]) {
		pr_err("fail to ctx id %d, buf_id %d\n", ctx->ctx_id, ctx->buf_info[idx]);
		return -1;
	}

	idx = ctx->fid % ISP_LTM_BUF_NUM;
	hists->clip_limit = param->clipLimit;
	hists->clip_limit_min = param->clipLimit_min;
	hists->texture_proportion = param->text_proportion;
	hists->text_point_thres = param->text_point_thres;
	hists->addr = ctx->buf_info[idx]->iova[0];
	hists->pitch = param->tile_num_x - 1;
	hists->wr_num = param->tile_num_x * 32;

	memcpy(hists->ltm_hist_table, tuning->ltm_hist_table, sizeof(tuning->ltm_hist_table));

	ctx->frame_width_stat = param->frame_width;
	ctx->frame_height_stat = param->frame_height;

	pr_debug("ltm hist idx[%d], hist addr[0x%lx] bypass %d\n", ctx->fid, hists->addr, hists->bypass);
	pr_debug("binning_en[%d], tile_num_x_minus[%d], tile_num_y_minus[%d], tile_num_auto[%d]\n",
		hists->binning_en, hists->tile_num_x_minus, hists->tile_num_y_minus, tuning->tile_num_auto);
	pr_debug("tile_height[%d], tile_width[%d], clip_limit_min[%d], clip_limit[%d]\n",
		hists->tile_height, hists->tile_width, hists->clip_limit_min, hists->clip_limit);
	pr_debug("idx[%d], roi_start_x[%d], roi_start_y[%d], addr[0x%lx]\n",
		ctx->fid, hists->roi_start_x, hists->roi_start_y, hists->addr);
	pr_debug("region_est_en[%d], texture_proportion[%d]\n",
			hists->region_est_en, hists->texture_proportion);

	return ret;
}

static int ispltm_map_config_gen(struct isp_ltm_ctx_desc *ctx,
			struct isp_ltm_map_info *tuning, int type)
{
	int idx = 0;

	struct isp_ltm_hist_param map_param;
	struct isp_ltm_hist_param *param = &map_param;
	struct isp_ltm_rtl_param  rtl_param;
	struct isp_ltm_rtl_param  *prtl = &rtl_param;

	struct isp_ltm_hists *hists = &ctx->hists;
	struct isp_ltm_map *map = &ctx->map;

	struct isp_ltm_tile_num_minus1 mnum;
	struct isp_ltm_tile_size ts;
	struct isp_ltm_tile_size tm;

	uint32_t frame_width_stat, frame_height_stat;
	uint32_t frame_width_map,  frame_height_map;

	uint32_t ratio = 0;
	uint32_t ratio_w = 0;
	uint32_t ratio_h = 0;
	uint32_t crop_cols_curr, crop_rows_curr;
	uint32_t crop_up_curr,   crop_down_curr;
	uint32_t crop_left_curr, crop_right_curr;

	uint32_t slice_info[4];

	map->bypass = map->bypass || tuning->bypass;
	pr_debug("ltm: type %d, map bypass %d, tuning bypass %d\n",
			type, map->bypass, tuning->bypass);

	if (map->bypass)
		return 0;

	mnum.tile_num_x = hists->tile_num_x_minus + 1;
	mnum.tile_num_y = hists->tile_num_y_minus + 1;
	ts.tile_width = hists->tile_width;
	ts.tile_height = hists->tile_height;
	frame_width_stat = ctx->frame_width_stat;
	frame_height_stat = ctx->frame_height_stat;
	frame_width_map = ctx->frame_width;
	frame_height_map = ctx->frame_height;

	if ((frame_width_stat == 0) || (frame_height_stat == 0) || frame_width_map == 0 || frame_height_map == 0) {
		pr_err("fail to get input param, width stat %d, height stat %d\n",
			frame_width_stat, frame_height_stat);
		map->bypass = 1;
		return 0;
	}

	pr_debug("tile_num_x[%d], tile_num_y[%d], tile_width[%d], tile_height[%d], \
		frame_width_stat[%d], frame_height_stat[%d], \
		frame_width_map[%d], frame_height_map[%d] ALIGNMENT[%d]\n",
		mnum.tile_num_x, mnum.tile_num_y,
		ts.tile_width, ts.tile_height,
		frame_width_stat, frame_height_stat,
		frame_width_map, frame_height_map,
		ISP_LTM_ALIGNMENT);

	/*
	 * frame_width_map/frame_width_stat should be
	 * equal to frame_height_map/frame_height_stat
	 */
	if (ISP_LTM_ALIGNMENT == 4) {
		if (frame_width_stat != 0 && frame_height_stat != 0) {
			ratio_w = ((frame_width_map << 8) + (frame_width_stat / 2)) / frame_width_stat;
			ratio_h = ((frame_height_map << 8) + (frame_height_stat / 2)) / frame_height_stat;
		}
		tm.tile_width = ((ratio_w * ts.tile_width) >> 10) << 2;
		tm.tile_height = ((ratio_h * ts.tile_height) >> 9) << 1;
	} else {
		if (frame_width_stat != 0)
			ratio = (frame_width_map << 7) / frame_width_stat;

		tm.tile_width = ((ratio * ts.tile_width  + 128) >> 8) << 1;
		tm.tile_height = ((ratio * ts.tile_height + 128) >> 8) << 1;
	}

	crop_cols_curr = frame_width_map - tm.tile_width * mnum.tile_num_x;
	crop_rows_curr = frame_height_map - tm.tile_height * mnum.tile_num_y;
	crop_up_curr = crop_rows_curr >> 1;
	crop_down_curr = crop_rows_curr >> 1;
	crop_left_curr = crop_cols_curr >> 1;
	crop_right_curr = crop_cols_curr >> 1;

	/* update parameters */
	param->cropUp = crop_up_curr;
	param->cropDown = crop_down_curr;
	param->cropLeft = crop_left_curr;
	param->cropRight = crop_right_curr;
	param->cropCols = crop_cols_curr;
	param->cropRows = crop_rows_curr;

	param->tile_num_x = mnum.tile_num_x;
	param->tile_num_y = mnum.tile_num_y;
	param->tile_width = tm.tile_width;
	param->tile_height = tm.tile_height;
	param->frame_width = frame_width_map;
	param->frame_height = frame_height_map;
	param->tile_size = tm.tile_width * tm.tile_height;

	slice_info[0] = 0;
	slice_info[1] = 0;
	slice_info[2] = frame_width_map - 1;
	slice_info[3] = frame_height_map - 1;

	ispltm_rgb_map_dump_data_rtl(param, slice_info, prtl);
	/*
	 * burst8_en : 0 ~ burst8; 1 ~ burst16
	 */
	map->burst8_en = 0;
	/*
	 * map auto bypass if hist error happen
	 */
	map->hist_error_en = 0;
	/*
	 * wait_en & wait_line
	 * fetch data raw, rgb: set 0
	 * fetch data yuv     : set 1
	 */
	map->fetch_wait_en = 0;
	map->fetch_wait_line = 0;

	map->tile_width = tm.tile_width;
	map->tile_height = tm.tile_height;
	map->tile_x_num = prtl->tile_x_num_rtl;
	map->tile_y_num = prtl->tile_y_num_rtl;
	map->tile_size_pro = tm.tile_width * tm.tile_height;
	map->tile_start_x = prtl->tile_start_x_rtl;
	map->tile_left_flag = prtl->tile_left_flag_rtl;
	map->tile_start_y = prtl->tile_start_y_rtl;
	map->tile_right_flag = prtl->tile_right_flag_rtl;
	map->hist_pitch = mnum.tile_num_x - 1;
	idx = ctx->fid % ISP_LTM_BUF_NUM;

	if (tuning->ltm_map_video_mode) {
		if (idx == 0)
			idx = ISP_LTM_BUF_NUM;
		map->mem_init_addr = ctx->buf_info[idx - 1]->iova[0];
	} else {
		map->mem_init_addr = ctx->buf_info[idx]->iova[0];
	}

	pr_debug("tile_width[%d], tile_height[%d], tile_x_num[%d], tile_y_num[%d]\n",
		map->tile_width, map->tile_height, map->tile_x_num, map->tile_y_num);
	pr_debug("tile_size_pro[%d], tile_start_x[%d], tile_left_flag[%d]\n",
		map->tile_size_pro, map->tile_start_x, map->tile_left_flag);
	pr_debug("tile_start_y[%d], tile_right_flag[%d], hist_pitch[%d]\n",
		map->tile_start_y, map->tile_right_flag, map->hist_pitch);
	pr_debug("ltm map idx[%d], map addr[0x%lx]\n",
		ctx->fid, map->mem_init_addr);

	return 0;
}

static int ispltm_pipe_proc(void *handle, void *param)
{
	int ret = 0;
	struct isp_ltm_ctx_desc *ctx = NULL;
	struct isp_ltm_info *ltm_info = NULL;
	struct isp_ltm_sync *sync = NULL;
	struct isp_hw_k_blk_func ltm_cfg_func;
	int pre_fid = 0;
	long timeout = 0;

	if (!handle || !param) {
		pr_err("fail to get valid input ptr %p\n", handle);
		return -EFAULT;
	}

	ctx = (struct isp_ltm_ctx_desc *)handle;
	ltm_info = (struct isp_ltm_info *)param;
	sync = ctx->sync;

	if (ctx->enable == 0)
		return 0;

	ltm_cfg_func.index = ISP_K_BLK_LTM;
	ctx->hw->isp_ioctl(ctx->hw, ISP_HW_CFG_K_BLK_FUNC_GET, &ltm_cfg_func);
	if (ltm_cfg_func.k_blk_func == NULL)
		return 0;

	pr_debug("type[%d], fid[%d], frame_width[%d], frame_height[%d] bypass %d, static %d, map %d\n",
		ctx->mode, ctx->fid, ctx->frame_width, ctx->frame_height, ctx->bypass, ltm_info->ltm_stat.bypass, ltm_info->ltm_map.bypass);

	switch (ctx->mode) {
	case MODE_LTM_PRE:
		ltm_info->ltm_map.ltm_map_video_mode = 1;
		ret = ispltm_histo_config_gen(ctx, &ltm_info->ltm_stat);
		if (ret < 0) {
			pr_err("faill to preview hist config, ctx id %d, fid %d\n", ctx->ctx_id, ctx->fid);
			break;
		}
		ispltm_map_config_gen(ctx, &ltm_info->ltm_map, ISP_PRO_LTM_PRE_PARAM);
		ltm_cfg_func.k_blk_func(ctx);
		ispltm_sync_config_set(ctx, &ctx->hists);
		break;
	case MODE_LTM_CAP:
		ispltm_sync_config_get(ctx, &ctx->hists);
		pre_fid = atomic_read(&sync->pre_fid);
		ctx->map.bypass = sync->pre_hist_bypass;
		ctx->hists.bypass = 1;
		pr_debug("LTM capture ctx_id %d, map %d\n", ctx->ctx_id, ctx->map.bypass);
		if (!ctx->map.bypass) {
			while (ctx->fid > pre_fid) {
				pr_debug("LTM capture fid [%d] > previre fid [%d]\n", ctx->fid, pre_fid);

				if (ispltm_sync_status_get(ctx, MODE_LTM_PRE) == 0) {
					pr_err("fail to use free pre context\n");
					ctx->mode = MODE_LTM_OFF;
					ctx->bypass = 1;
					ret = -1;
					break;
				}

				ispltm_sync_completion_set(ctx, ctx->fid);
				timeout = wait_for_completion_interruptible_timeout(
					&sync->share_comp, ISP_LTM_TIMEOUT);
				if (timeout <= 0) {
					pr_err("fail to wait completion [%ld]\n", timeout);
					ctx->mode = MODE_LTM_OFF;
					ctx->bypass = 1;
					ret = -1;
					break;
				}

				pre_fid = atomic_read(&sync->pre_fid);
				if (ctx->fid > pre_fid) {
					/*
					 * Still cap fid > pre fid
					 * Means context of pre has release
					 * this complete from isp_core before release
					 */
					pr_err("fail to use free pre context\n");
					ctx->mode = MODE_LTM_OFF;
					ctx->bypass = 1;
					ret = -1;
					break;
				}
			}
			/*
			* Capture size ratio must be same as preview size ratio
			* Because Capture Stats depend on Preview Stats
			* Transform (A/B != C/D) to (A*D != B*C)
			*/
			if ((sync->pre_frame_w * ctx->frame_height) != (sync->pre_frame_h * ctx->frame_width)) {
				pr_err("fail to match prv size with cap size\n");
				ctx->map.bypass = 1;
				ret = -1;
			}
		}

		ret = ispltm_histo_config_gen(ctx, &ltm_info->ltm_stat);
		if (ret < 0) {
			pr_err("fail to capture hist config fail, maybe none-zsl, ctx id %d, fid %d\n", ctx->ctx_id, ctx->fid);
			break;
		}

		ltm_info->ltm_map.ltm_map_video_mode = 0;
		ispltm_map_config_gen(ctx, &ltm_info->ltm_map, ISP_PRO_LTM_CAP_PARAM);
		ltm_cfg_func.k_blk_func(ctx);
		break;

	case MODE_LTM_OFF:
	default:
		ctx->bypass = 1;
		ltm_cfg_func.k_blk_func(ctx);
		break;
	}

	return ret;
}

static int ispltm_cfg_param(void *handle,
		enum isp_ltm_cfg_cmd cmd, void *param)
{
	int ret = 0;
	uint32_t i = 0, fid = 0;
	struct isp_ltm_ctx_desc *ltm_ctx = NULL;
	struct camera_frame * pframe = NULL;
	struct img_trim *crop = NULL;

	if (!handle || !param) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	ltm_ctx = (struct isp_ltm_ctx_desc *)handle;
	switch (cmd) {
	case ISP_LTM_CFG_EB:
		ltm_ctx->enable = *(uint32_t *)param;
		pr_debug("ctx_id %d, LTM enable %d\n", ltm_ctx->ctx_id, ltm_ctx->enable);
		break;
	case ISP_LTM_CFG_MODE:
		ltm_ctx->mode = *(uint32_t *)param;
		pr_debug("ctx_id %d, LTM mode %d\n", ltm_ctx->ctx_id,ltm_ctx->mode);
		break;
	case ISP_LTM_CFG_BUF:
		pframe = (struct camera_frame *)param;
		if ((pframe->buf.mapping_state & CAM_BUF_MAPPING_DEV) == 0) {
			ret = cam_buf_iommu_map(&pframe->buf, CAM_IOMMUDEV_ISP);
			if (ret) {
				pr_err("fail to map isp ltm iommu buf.\n");
				ret = -EINVAL;
				goto exit;
			}
		}

		for (i = 0; i < ISP_LTM_BUF_NUM; i++) {
			if (ltm_ctx->buf_info[i] == NULL) {
				ltm_ctx->buf_info[i] = &pframe->buf;
				pr_debug("ctx id %d, LTM CFGB[%d][0x%p] = 0x%lx\n", ltm_ctx->ctx_id, i, pframe, ltm_ctx->buf_info[i]->iova[0]);
				break;
			}
		}

		if (i == ISP_LTM_BUF_NUM)
			pr_err("fail to get ltm frame buffer, ctx_id %d, mode %d\n", ltm_ctx->ctx_id, ltm_ctx->mode);
		break;
	case ISP_LTM_CFG_FRAME_ID:
		fid = *(uint32_t *)param;
		if (ltm_ctx->mode == MODE_LTM_PRE) {
			if (((fid - ltm_ctx->fid) != 1) || (fid == 0)) {
				ltm_ctx->map.bypass = 1;
			}
		}
		ltm_ctx->fid = fid;
		pr_debug("LTM frame id %d, map %d\n", ltm_ctx->fid, ltm_ctx->map.bypass);
		break;
	case ISP_LTM_CFG_SIZE_INFO:
		crop = (struct img_trim *)param;
		if (ltm_ctx->mode == MODE_LTM_PRE) {
			if ((crop->size_x != ltm_ctx->frame_width) ||
				(crop->size_y != ltm_ctx->frame_height)) {
				ltm_ctx->map.bypass = 1;
			}
		}
		ltm_ctx->frame_width = crop->size_x;
		ltm_ctx->frame_height = crop->size_y;
		pr_debug("LTM frame id %d, crop %d, %d, map %d\n", ltm_ctx->fid, crop->size_x, crop->size_y, ltm_ctx->map.bypass);
		break;
	case ISP_LTM_CFG_HIST_BYPASS:
		ltm_ctx->hists.bypass = !(*(uint32_t *)param);
		pr_debug("LTM frame id %d, hist bypass %d\n", ltm_ctx->fid, ltm_ctx->hists.bypass);
		break;
	case ISP_LTM_CFG_MAP_BYPASS:
		ltm_ctx->map.bypass = !(*(uint32_t *)param);
		pr_debug("LTM frame id %d, map bypass %d\n", ltm_ctx->fid, ltm_ctx->map.bypass);
		break;
	default:
		pr_err("fail to get known cmd: %d\n", cmd);
		ret = -EFAULT;
		break;
	}

exit:
	return ret;
}

static struct isp_ltm_ctx_desc *ispltm_ctx_init(uint32_t idx, uint32_t cam_id, void *hw)
{
	struct isp_ltm_ctx_desc *ltm_ctx = NULL;

	ltm_ctx = vzalloc(sizeof(struct isp_ltm_ctx_desc));
	if (!ltm_ctx) {
		pr_err("fail to alloc isp %d ltm ctx\n", idx);
		return NULL;
	}

	ltm_ctx->ctx_id = idx;
	ltm_ctx->cam_id = cam_id;
	ltm_ctx->hw = hw;
	ltm_ctx->ltm_ops.core_ops.cfg_param = ispltm_cfg_param;
	ltm_ctx->ltm_ops.core_ops.pipe_proc = ispltm_pipe_proc;
	ltm_ctx->ltm_ops.sync_ops.set_status = ispltm_sync_status_set;
	ltm_ctx->ltm_ops.sync_ops.clear_status = ispltm_sync_status_clear;
	ltm_ctx->ltm_ops.sync_ops.set_frmidx = ispltm_sync_fid_set;
	ltm_ctx->ltm_ops.sync_ops.get_completion = ispltm_sync_completion_get;
	ltm_ctx->ltm_ops.sync_ops.do_completion = ispltm_sync_completion_done;

	return ltm_ctx;
}

static void ispltm_ctx_deinit(void *handle)
{
	uint32_t i = 0;
	struct isp_ltm_ctx_desc *ltm_ctx = NULL;
	struct camera_buf *buf_info = NULL;

	if (!handle) {
		pr_err("fail to get valid ltm handle\n");
		return;
	}

	ltm_ctx = (struct isp_ltm_ctx_desc *)handle;
	if (ltm_ctx->enable && ltm_ctx->mode == MODE_LTM_PRE) {
		ltm_ctx->ltm_ops.sync_ops.clear_status(ltm_ctx);
		ltm_ctx->ltm_ops.sync_ops.do_completion(ltm_ctx);
		for (i = 0; i < ISP_LTM_BUF_NUM; i++) {
			buf_info = ltm_ctx->buf_info[i];
			pr_debug("ctx id %d, LTM CFGB[%d], frame_buf 0x%p, iova 0x%lx\n", ltm_ctx->ctx_id, i, buf_info, ltm_ctx->buf_info[i]->iova[0]);
			if (buf_info && buf_info->mapping_state & CAM_BUF_MAPPING_DEV) {
				cam_buf_iommu_unmap(buf_info);
				buf_info = NULL;
			}
		}
	}

	if (ltm_ctx->enable && ltm_ctx->mode == MODE_LTM_PRE) {
		ltm_ctx->ltm_ops.sync_ops.clear_status(ltm_ctx);
		for (i = 0; i < ISP_LTM_BUF_NUM; i++) {
			buf_info = ltm_ctx->buf_info[i];
			if (buf_info)
				buf_info = NULL;
		}
	}

	if (ltm_ctx)
		vfree(ltm_ctx);
	ltm_ctx = NULL;
}

/* * external function interface * */
int isp_ltm_map_slice_config_gen(struct isp_ltm_ctx_desc *ctx,
			struct isp_ltm_rtl_param *prtl, uint32_t *slice_info)
{
	struct isp_ltm_hist_param map_param;
	struct isp_ltm_hist_param *param = &map_param;
	/*
	 * struct isp_ltm_rtl_param  rtl_param;
	 * struct isp_ltm_rtl_param *prtl = &rtl_param;
	 */
	struct isp_ltm_hists *hists = &ctx->hists;

	struct isp_ltm_tile_num_minus1 mnum;
	struct isp_ltm_tile_size ts;
	struct isp_ltm_tile_size tm;

	uint32_t frame_width_stat, frame_height_stat;
	uint32_t frame_width_map, frame_height_map;

	uint32_t ratio = 0;
	uint32_t ratio_w = 0;
	uint32_t ratio_h = 0;
	uint32_t crop_cols_curr, crop_rows_curr;
	uint32_t crop_up_curr, crop_down_curr;
	uint32_t crop_left_curr, crop_right_curr;

	mnum.tile_num_x = hists->tile_num_x_minus + 1;
	mnum.tile_num_y = hists->tile_num_y_minus + 1;
	ts.tile_width = hists->tile_width;
	ts.tile_height = hists->tile_height;
	frame_width_stat = ctx->frame_width_stat;
	frame_height_stat = ctx->frame_height_stat;
	frame_width_map = ctx->frame_width;
	frame_height_map = ctx->frame_height;

	if (ctx->mode == MODE_LTM_CAP) {
		pr_debug("tile_num_x[%d], tile_num_y[%d], tile_width[%d], tile_height[%d], \
			frame_width_stat[%d], frame_height_stat[%d], \
			frame_width_map[%d], frame_height_map[%d]\n",
			mnum.tile_num_x, mnum.tile_num_y,
			ts.tile_width, ts.tile_height,
			frame_width_stat, frame_height_stat,
			frame_width_map, frame_height_map);
	}

	/*
	 * frame_width_map/frame_width_stat should be
	 * equal to frame_height_map/frame_height_stat
	 */
	if (ISP_LTM_ALIGNMENT == 4) {
		if (frame_width_stat != 0 && frame_height_stat != 0) {
			ratio_w = ((frame_width_map << 8) + (frame_width_stat / 2)) / frame_width_stat;
			ratio_h = ((frame_height_map << 8) + (frame_height_stat / 2)) / frame_height_stat;
		}

		tm.tile_width = (ratio_w * ts.tile_width) >> 10 << 2;
		tm.tile_height = (ratio_h * ts.tile_height) >> 9 << 1;
	} else {
		if (frame_width_stat != 0)
			ratio = (frame_width_map << 7) / frame_width_stat;

		tm.tile_width = (ratio * ts.tile_width  + 128) >> 8 << 1;
		tm.tile_height = (ratio * ts.tile_height + 128) >> 8 << 1;
	}

	crop_cols_curr = frame_width_map - tm.tile_width * mnum.tile_num_x;
	crop_rows_curr = frame_height_map - tm.tile_height * mnum.tile_num_y;
	crop_up_curr = crop_rows_curr >> 1;
	crop_down_curr = crop_rows_curr >> 1;
	crop_left_curr = crop_cols_curr >> 1;
	crop_right_curr = crop_cols_curr >> 1;

	/* update parameters */
	param->cropUp = crop_up_curr;
	param->cropDown = crop_down_curr;
	param->cropLeft = crop_left_curr;
	param->cropRight = crop_right_curr;
	param->cropCols = crop_cols_curr;
	param->cropRows = crop_rows_curr;

	param->tile_num_x = mnum.tile_num_x;
	param->tile_num_y = mnum.tile_num_y;
	param->tile_width = tm.tile_width;
	param->tile_height = tm.tile_height;
	param->frame_width = frame_width_map;
	param->frame_height = frame_height_map;
	param->tile_size = tm.tile_width * tm.tile_height;

	ispltm_rgb_map_dump_data_rtl(param, slice_info, prtl);

	return 0;
}

void *isp_ltm_rgb_ctx_get(uint32_t idx, enum camera_id cam_id, void *hw)
{
	struct isp_ltm_ctx_desc *ltm_ctx = NULL;

	ltm_ctx = ispltm_ctx_init(idx, cam_id, hw);
	if (!ltm_ctx) {
		pr_err("fail to get invalid ltm_ctx\n");
		return NULL;
	}

	ltm_ctx->sync = &s_rgb_ltm_sync[cam_id];
	ltm_ctx->ltm_id = LTM_RGB;

	return ltm_ctx;
}

void isp_ltm_rgb_ctx_put(void *ltm_handle)
{
	ispltm_ctx_deinit(ltm_handle);
}

void *isp_ltm_yuv_ctx_get(uint32_t idx, enum camera_id cam_id, void *hw)
{
	struct isp_ltm_ctx_desc *ltm_ctx = NULL;

	ltm_ctx = ispltm_ctx_init(idx, cam_id, hw);
	if (!ltm_ctx) {
		pr_err("fail to get invalid ltm_ctx\n");
		return NULL;
	}

	ltm_ctx->sync = &s_yuv_ltm_sync[cam_id];
	ltm_ctx->ltm_id = LTM_YUV;

	return ltm_ctx;
}

void isp_ltm_yuv_ctx_put(void *ltm_handle)
{
	ispltm_ctx_deinit(ltm_handle);
}

void isp_ltm_sync_init(void)
{
	uint32_t i = 0;

	for (i = 0; i < LTM_ID_MAX; i++) {
		ispltm_sync_ctx_init(&s_rgb_ltm_sync[i]);
		ispltm_sync_ctx_init(&s_yuv_ltm_sync[i]);
	}
}

void isp_ltm_sync_deinit(void)
{
	uint32_t i = 0;

	for (i = 0; i < LTM_ID_MAX; i++) {
		ispltm_sync_ctx_deinit(&s_rgb_ltm_sync[i]);
		ispltm_sync_ctx_deinit(&s_yuv_ltm_sync[i]);
	}
}
