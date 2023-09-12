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

#include <linux/uaccess.h>
#include <sprd_mm.h>
#include <linux/mutex.h>
#include "isp_gtm.h"
#include "isp_hw.h"

 #ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_GTM: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static struct isp_gtm_sync s_rgb_gtm_sync[GTM_ID_MAX];

static int ispgtm_cfg_param(void *handle,
		 enum isp_gtm_cfg_cmd cmd, void *param)
{
	 int ret = 0;
	 struct isp_gtm_ctx_desc *gtm_ctx = NULL;

	 if (!handle || !param) {
		 pr_err("fail to get valid input ptr\n");
		 return -EFAULT;
	 }

	 gtm_ctx = (struct isp_gtm_ctx_desc *)handle;

	 switch (cmd) {
	 case ISP_GTM_CFG_EB:
		 gtm_ctx->enable = *(uint32_t *)param;
		 pr_debug("GTM ctx_id %d, enable %d\n", gtm_ctx->ctx_id, gtm_ctx->enable);
		 break;
	 case ISP_GTM_CFG_MODE:
		 gtm_ctx->mode = *(uint32_t *)param;
		 pr_debug("GTM ctx_id %d, mode %d\n", gtm_ctx->ctx_id, gtm_ctx->mode);
		 break;
	case ISP_GTM_CFG_FRAME_ID:
		gtm_ctx->fid = *(uint32_t *)param;
		pr_debug("GTM ctx_id %d, frame id %d\n", gtm_ctx->ctx_id, gtm_ctx->fid);
		break;
	case ISP_GTM_CFG_HIST_BYPASS:
		gtm_ctx->gtm_hist_stat_bypass = !(*(uint32_t *)param);
		pr_debug("GTM ctx_id %d, frame id %d, hist bypass %d\n", gtm_ctx->ctx_id, gtm_ctx->fid, gtm_ctx->gtm_hist_stat_bypass);
		break;
	case ISP_GTM_CFG_MAP_BYPASS:
		gtm_ctx->gtm_map_bypass = !(*(uint32_t *)param);
		pr_debug("GTM ctx_id %d, frame id %d, map bypass %d\n", gtm_ctx->ctx_id, gtm_ctx->fid, gtm_ctx->gtm_map_bypass);
		break;
	case ISP_GTM_CFG_MOD_EN:
		gtm_ctx->gtm_mode_en= *(uint32_t *)param;
		pr_debug("GTM ctx_id %d, frame id %d, mod_en %d\n", gtm_ctx->ctx_id, gtm_ctx->fid, gtm_ctx->gtm_mode_en);
		break;
	case ISP_GTM_CFG_CALC_MODE:
		gtm_ctx->calc_mode = *(uint32_t *)param;
		pr_debug("GTM ctx_id %d, calc mode %d\n", gtm_ctx->ctx_id, gtm_ctx->calc_mode);
		break;
	 default:
		 pr_debug("fail to get known cmd: %d\n", cmd);
		 ret = -EFAULT;
		 break;
	 }

	 return ret;
}

static int ispgtm_sync_completion_done(void *handle)
{
	int fid = 0;
	struct isp_gtm_ctx_desc *gtm_ctx = NULL;
	struct isp_gtm_sync *sync = NULL;

	if (!handle) {
		pr_err("fail to get invalid ptr\n");
		return -EFAULT;
	}

	gtm_ctx = (struct isp_gtm_ctx_desc *)handle;
	sync = gtm_ctx->sync;
	if (!sync) {
		pr_err("fail to get sync ptr\n");
		return -1;
	}

	fid = atomic_read(&sync->wait_completion_fid);
	if (fid) {
		atomic_set(&sync->wait_completion_fid, 0);
		complete(&sync->share_comp);
	}

	return fid;
}

static int ispgtm_sync_completion_get(void *handle)
{
	int fid = 0;
	struct isp_gtm_ctx_desc *gtm_ctx = NULL;
	struct isp_gtm_sync *sync = NULL;

	if (!handle) {
		pr_err("fail to get invalid ptr\n");
		return -EFAULT;
	}

	gtm_ctx = (struct isp_gtm_ctx_desc *)handle;
	sync = gtm_ctx->sync;

	fid = atomic_read(&sync->wait_completion_fid);

	return fid;
}

static int ispgtm_sync_completion_set(struct isp_gtm_sync *gtm_sync, int fid)
{
	if(!gtm_sync) {
		pr_err("fail to get invalid ptr\n");
		return -EFAULT;
	}

	atomic_set(&gtm_sync->wait_completion_fid, fid);

	return 0;
}

static int ispgtm_get_preview_hist_cal(void *handle)
{
	struct isp_gtm_ctx_desc *gtm_ctx = NULL;
	struct isp_gtm_mapping *mapping = NULL;
	struct isp_hw_gtm_func gtm_func;

	if (!handle) {
		pr_err("fail to get invaild ptr\n");
		return -EFAULT;
	}

	gtm_ctx = (struct isp_gtm_ctx_desc *)handle;
	mapping = &gtm_ctx->sync->mapping;

	gtm_func.index = ISP_K_GTM_MAPPING_GET;
	gtm_ctx->hw->isp_ioctl(gtm_ctx->hw, ISP_HW_CFG_GTM_FUNC_GET, &gtm_func);
	gtm_func.k_blk_func(mapping);

	mapping->fid = gtm_ctx->fid;
	pr_debug("gtm ctx_id %d, mapping fid %d\n", gtm_ctx->ctx_id, gtm_ctx->fid);

	return 0;
}

static int ispgtm_capture_tunning_set(int cam_id,
	struct dcam_dev_raw_gtm_block_info *tunning)
{
	int i = 0;

	if (cam_id >=GTM_ID_MAX || !tunning) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	while(i < GTM_ID_MAX) {
		if (cam_id == i)
			break;
		i++;
	}

	if (i == GTM_ID_MAX) {
		pr_err("fail to get cam id\n");
		return -1;
	}

	memcpy(&s_rgb_gtm_sync[i].tuning, tunning, sizeof(struct dcam_dev_raw_gtm_block_info));
	pr_debug("cam_id %d, mod_en %d\n", cam_id, s_rgb_gtm_sync[i].tuning.bypass_info.gtm_mod_en);
	return 0;
}

static int ispgtm_capture_tunning_get(int cam_id,
	struct dcam_dev_raw_gtm_block_info *tunning)
{
	int i = 0;

	if (cam_id >= GTM_ID_MAX || !tunning) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	while(i < GTM_ID_MAX) {
		if (cam_id == i)
			break;
		i++;
	}

	if (i == GTM_ID_MAX) {
		pr_err("fail to get cam id\n");
		return -1;
	}

	memcpy(tunning, &s_rgb_gtm_sync[i].tuning, sizeof(struct dcam_dev_raw_gtm_block_info));
	tunning->isupdate = 1;
	pr_debug("cam_id %d, mod_en %d\n", cam_id, s_rgb_gtm_sync[i].tuning.bypass_info.gtm_mod_en);
	return 0;
}

static int ispgtm_pipe_proc(void *handle, void *param, void *param2)
{
	int ret = 0;
	uint32_t idx = 0;
	int timeout = 0;
 	struct isp_gtm_ctx_desc *gtm_ctx = NULL;
	struct isp_gtm_sync *gtm_sync = NULL;
	struct isp_gtm_mapping *mapping = NULL;
	struct isp_gtm_k_block gtm_k_block ={0};
	struct isp_hw_gtm_func gtm_func;
	struct isp_gtm_bypass_param gtm_bypass = {0};

	if (!handle || !param) {
		 pr_err("fail to get valid input ptr NULL\n");
		 return -EFAULT;
	}

	gtm_ctx = (struct isp_gtm_ctx_desc *)handle;

	switch (gtm_ctx->mode) {
	case MODE_GTM_PRE:
		gtm_k_block.ctx = gtm_ctx;
		gtm_k_block.tuning = (struct dcam_dev_raw_gtm_block_info *)param;

		if (gtm_ctx->fid == 0)
			gtm_k_block.map= (struct cam_gtm_mapping *)param2;
		gtm_func.index = ISP_K_GTM_BLOCK_SET;
		gtm_ctx->hw->isp_ioctl(gtm_ctx->hw, ISP_HW_CFG_GTM_FUNC_GET, &gtm_func);
		gtm_func.k_blk_func(&gtm_k_block);

		if (gtm_k_block.tuning->bypass_info.gtm_mod_en) {
			pr_debug("ctx %d, fid %d, do preview mapping\n", gtm_ctx->ctx_id, gtm_ctx->fid);
			gtm_func.index = ISP_K_GTM_MAPPING_SET;
			gtm_ctx->hw->isp_ioctl(gtm_ctx->hw, ISP_HW_CFG_GTM_FUNC_GET, &gtm_func);
			if (gtm_ctx->calc_mode == GTM_SW_CALC) {
				struct isp_gtm_mapping map = {0};
				gtm_k_block.map= (struct cam_gtm_mapping *)param2;
				map.ctx_id= gtm_ctx->ctx_id;
				map.sw_mode = 1;
				map.gtm_hw_ymin = gtm_k_block.map->ymin;
				map.gtm_hw_ymax = gtm_k_block.map->ymax;
				map.gtm_hw_yavg = gtm_k_block.map->yavg;
				map.gtm_hw_target_norm = gtm_k_block.map->target;
				map.gtm_hw_lr_int = gtm_k_block.map->lr_int;
				map.gtm_hw_log_diff_int = gtm_k_block.map->log_diff_int;
				map.gtm_hw_log_min_int = gtm_k_block.map->log_min_int;
				map.gtm_hw_log_diff = gtm_k_block.map->diff;
				gtm_func.k_blk_func(&map);
			} else if (atomic_read(&gtm_ctx->cnt) > 1) {
				mapping = &gtm_ctx->sync->mapping;
				mapping->ctx_id = gtm_ctx->ctx_id;
				mapping->sw_mode = 1;
				gtm_func.k_blk_func(&gtm_ctx->sync->mapping);
			}
		}

		gtm_bypass.ctx_id = gtm_ctx->ctx_id;
		gtm_bypass.hist_bypass = gtm_ctx->gtm_hist_stat_bypass || gtm_k_block.tuning->bypass_info.gtm_hist_stat_bypass;
		if ((gtm_ctx->calc_mode != GTM_SW_CALC) && (gtm_ctx->fid == 0))
			gtm_bypass.map_bypass = 1;
		else
			gtm_bypass.map_bypass = gtm_ctx->gtm_map_bypass || gtm_k_block.tuning->bypass_info.gtm_map_bypass;
		gtm_bypass.mod_en = gtm_ctx->gtm_mode_en && gtm_k_block.tuning->bypass_info.gtm_mod_en;
		gtm_func.index = ISP_K_GTM_BYPASS_SET;
		gtm_ctx->hw->isp_ioctl(gtm_ctx->hw, ISP_HW_CFG_GTM_FUNC_GET, &gtm_func);
		gtm_func.k_blk_func(&gtm_bypass);

		ret = ispgtm_capture_tunning_set(gtm_ctx->cam_id, param);
		if (ret) {
			pr_err("fail to ctx_id %d cam_id %d set tuning param\n", gtm_ctx->ctx_id, gtm_ctx->cam_id);
			return -1;
		}
		break;
	 case MODE_GTM_CAP:
		gtm_sync = gtm_ctx->sync;
		if (!gtm_sync) {
			pr_err("fail to ctx_id %d get valid gtm_sync ptr NULL\n", gtm_ctx->ctx_id);
			return -1;
		}

		ret = ispgtm_capture_tunning_get(gtm_ctx->cam_id, param);
		if (ret) {
			pr_err("fail to ctx_id %d get tuning param\n", gtm_ctx->ctx_id);
			return -1;
		}

		gtm_k_block.ctx = gtm_ctx;
		gtm_k_block.tuning = param;
		gtm_k_block.tuning->bypass_info.gtm_hist_stat_bypass = 1;

		if (gtm_k_block.tuning->bypass_info.gtm_mod_en == 0) {
			gtm_bypass.ctx_id = gtm_ctx->ctx_id;
			gtm_bypass.hist_bypass = gtm_ctx->gtm_hist_stat_bypass || gtm_k_block.tuning->bypass_info.gtm_hist_stat_bypass;
			gtm_bypass.map_bypass = gtm_ctx->gtm_map_bypass || gtm_k_block.tuning->bypass_info.gtm_map_bypass;
			gtm_bypass.mod_en = gtm_ctx->gtm_mode_en && gtm_k_block.tuning->bypass_info.gtm_mod_en;
			gtm_func.index = ISP_K_GTM_BYPASS_SET;
			gtm_ctx->hw->isp_ioctl(gtm_ctx->hw, ISP_HW_CFG_GTM_FUNC_GET, &gtm_func);
			gtm_func.k_blk_func(&gtm_bypass);
			pr_debug("capture frame ctx_id %d, mod_en off\n", gtm_ctx->ctx_id);
			goto exit;
		}
		if (gtm_ctx->calc_mode == GTM_SW_CALC) {
			struct isp_gtm_mapping map = {0};

			gtm_func.index = ISP_K_GTM_BLOCK_SET;
			gtm_ctx->hw->isp_ioctl(gtm_ctx->hw, ISP_HW_CFG_GTM_FUNC_GET, &gtm_func);
			gtm_func.k_blk_func(&gtm_k_block);

			gtm_k_block.map= (struct cam_gtm_mapping *)param2;
			map.ctx_id= gtm_ctx->ctx_id;
			map.sw_mode = 1;
			map.gtm_hw_ymin = gtm_k_block.map->ymin;
			map.gtm_hw_ymax = gtm_k_block.map->ymax;
			map.gtm_hw_yavg = gtm_k_block.map->yavg;
			map.gtm_hw_target_norm = gtm_k_block.map->target;
			map.gtm_hw_lr_int = gtm_k_block.map->lr_int;
			map.gtm_hw_log_diff_int = gtm_k_block.map->log_diff_int;
			map.gtm_hw_log_min_int = gtm_k_block.map->log_min_int;
			map.gtm_hw_log_diff = gtm_k_block.map->diff;
			gtm_func.index = ISP_K_GTM_MAPPING_SET;
			gtm_ctx->hw->isp_ioctl(gtm_ctx->hw, ISP_HW_CFG_GTM_FUNC_GET, &gtm_func);
			gtm_func.k_blk_func(&map);
		} else {
			ispgtm_sync_completion_set(gtm_sync, 1);
			timeout = wait_for_completion_interruptible_timeout(&gtm_sync->share_comp, ISP_GM_TIMEOUT);
			if (timeout <= 0) {
				pr_err("fail to wait gtm completion [%ld]\n", timeout);
				gtm_ctx->mode = MODE_GTM_OFF;
				gtm_ctx->bypass = 1;
				ret = -1;
				goto exit;
			}

			pr_debug("gtm capture: ctx_id %d, capture fid %d\n", gtm_ctx->ctx_id, gtm_ctx->fid);
			mapping = &gtm_ctx->sync->mapping;
			gtm_func.index = ISP_K_GTM_BLOCK_SET;
			gtm_ctx->hw->isp_ioctl(gtm_ctx->hw, ISP_HW_CFG_GTM_FUNC_GET, &gtm_func);
			gtm_func.k_blk_func(&gtm_k_block);

			mapping->ctx_id = gtm_ctx->ctx_id;
			mapping->sw_mode = 1;
			gtm_func.index = ISP_K_GTM_MAPPING_SET;
			gtm_ctx->hw->isp_ioctl(gtm_ctx->hw, ISP_HW_CFG_GTM_FUNC_GET, &gtm_func);
			gtm_func.k_blk_func(mapping);
		}

		gtm_bypass.ctx_id = gtm_ctx->ctx_id;
		gtm_bypass.hist_bypass = gtm_ctx->gtm_hist_stat_bypass || gtm_k_block.tuning->bypass_info.gtm_hist_stat_bypass;
		gtm_bypass.map_bypass = gtm_ctx->gtm_map_bypass || gtm_k_block.tuning->bypass_info.gtm_map_bypass;
		gtm_bypass.mod_en = gtm_ctx->gtm_mode_en && gtm_k_block.tuning->bypass_info.gtm_mod_en;
		gtm_func.index = ISP_K_GTM_BYPASS_SET;
		gtm_ctx->hw->isp_ioctl(gtm_ctx->hw, ISP_HW_CFG_GTM_FUNC_GET, &gtm_func);
		gtm_func.k_blk_func(&gtm_bypass);

		idx = gtm_ctx->ctx_id;
		gtm_func.index = ISP_K_GTM_STATUS_GET;
		gtm_ctx->hw->isp_ioctl(gtm_ctx->hw, ISP_HW_CFG_GTM_FUNC_GET, &gtm_func);
		gtm_ctx->gtm_mode_en = gtm_func.k_blk_func(&idx) & gtm_k_block.tuning->bypass_info.gtm_mod_en;
		pr_debug("gtm%d sw mode %d, mod en %d, hist bypass %d, map bypass %d\n",
			idx, gtm_ctx->calc_mode, gtm_bypass.mod_en, gtm_bypass.hist_bypass, gtm_bypass.map_bypass);
		break;
	case MODE_GTM_OFF:
		pr_debug("ctx_id %d, GTM off\n", gtm_ctx->ctx_id);
		break;
	default:
		pr_debug("waring , ctx_id %d, GTM need to check, mode %d\n", gtm_ctx->ctx_id, gtm_ctx->mode);
		break;
	 }
exit:
	return ret;
}

static struct isp_gtm_ctx_desc *ispgtm_ctx_init(uint32_t idx, uint32_t cam_id, void *hw)
{
	struct isp_gtm_ctx_desc *gtm_ctx = NULL;

	gtm_ctx = vzalloc(sizeof(struct isp_gtm_ctx_desc));
	if (!gtm_ctx) {
		pr_err("fail to alloc isp %d gtm ctx\n", idx);
		return NULL;
	}

	gtm_ctx->ctx_id = idx;
	gtm_ctx->cam_id = cam_id;
	gtm_ctx->hw = hw;
	atomic_set(&gtm_ctx->cnt, 0);
	gtm_ctx->gtm_ops.cfg_param = ispgtm_cfg_param;
	gtm_ctx->gtm_ops.pipe_proc = ispgtm_pipe_proc;
	gtm_ctx->gtm_ops.sync_completion_get = ispgtm_sync_completion_get;
	gtm_ctx->gtm_ops.sync_completion_done = ispgtm_sync_completion_done;
	gtm_ctx->gtm_ops.get_preview_hist_cal = ispgtm_get_preview_hist_cal;
	gtm_ctx->sync = &s_rgb_gtm_sync[cam_id];

	return gtm_ctx;
}

void isp_gtm_rgb_ctx_put(void *gtm_handle)
{
	struct isp_gtm_ctx_desc *gtm_ctx = NULL;

	if (!gtm_handle) {
		pr_err("fail to get valid gtm_handle\n");
		return;
	}

	gtm_ctx = (struct isp_gtm_ctx_desc *)gtm_handle;

	if (gtm_ctx)
		vfree(gtm_ctx);

	gtm_ctx = NULL;
}

void *ispgtm_rgb_ctx_get(uint32_t idx, enum camera_id cam_id, void *hw)
{
	struct isp_gtm_ctx_desc *gtm_ctx = NULL;

	gtm_ctx = ispgtm_ctx_init(idx, cam_id, hw);
	if (!gtm_ctx) {
		pr_err("fail to get invalid ltm_ctx\n");
		return NULL;
	}

	return gtm_ctx;
}

void isp_gtm_sync_init(void)
{
	int i = 0;
	struct isp_gtm_sync *sync = NULL;

	for (; i < GTM_ID_MAX ; i++) {
		sync = &s_rgb_gtm_sync[i];
		atomic_set(&sync->prev_fid, 0);
		atomic_set(&sync->user_cnt, 0);
		atomic_set(&sync->wait_completion_fid, 0);
		init_completion(&sync->share_comp);
	}
}

void isp_gtm_sync_deinit(void)
{
	isp_gtm_sync_init();
}


