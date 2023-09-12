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

#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <sprd_mm.h>
#include "sprd_img.h"

#include "cam_types.h"
#include "flash_interface.h"

#include "flash_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "FLASH_CORE: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int img_opt_flash(void *param);

static int sprd_cam_flash_ctrl(uint32_t dcam_idx,
		struct sprd_img_set_flash *set_flash)
{
	int ret = 0;

	ret = sprd_flash_ctrl(set_flash);
	pr_info("%d set flash\n", dcam_idx);
	return ret;
}

static int sprd_cam_flash_open(struct cam_flash_task_info *flash_ctx, void *arg)
{
	return 0;
}

static int sprd_cam_flash_close(struct cam_flash_task_info *flash_ctx)
{
	return 0;
}

static int sprd_cam_flash_cfg(struct cam_flash_task_info *flash_ctx, void *cfg_parm)
{
	int ret = 0;
	struct sprd_flash_cfg_param *cfg_p = (struct sprd_flash_cfg_param *)cfg_parm;

	if (!flash_ctx || !cfg_p) {
		pr_err("fail to get flash handle\n");
		goto exit;
	}

	if (cfg_p->io_id >= FLASH_IOID_MAX || cfg_p->flash_idx >= SPRD_FLASH_MAX) {
		pr_err("fail to get vaild cfg_param io_id :%d flash_idx :%d\n", cfg_p->io_id, cfg_p->flash_idx);
		goto exit;
	}

	ret = sprd_flash_cfg(cfg_p);
	if (ret)
		pr_err("fail to sprd_flash_cfg\n");

exit:
	return ret;
}

static int sprd_cam_flash_info_get(struct cam_flash_task_info *flash_ctx, void *arg)
{
	sprd_flash_get_info(flash_ctx->set_flash.flash_index,
		SPRD_FLASH_LED_ALL, (struct sprd_flash_capacity *)arg);

	return 0;
}

static int sprd_cam_flash_start(struct cam_flash_task_info *flash_ctx, void *arg)
{
	int ret = 0;
	uint32_t need_light = 1;
	struct sprd_img_set_flash *set_flash = NULL;

	if (flash_ctx == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	set_flash = (struct sprd_img_set_flash *)arg;
	flash_ctx->set_flash.led0_ctrl = set_flash->led0_ctrl;
	flash_ctx->set_flash.led1_ctrl = set_flash->led1_ctrl;
	flash_ctx->set_flash.led0_status = set_flash->led0_status;
	flash_ctx->set_flash.led1_status = set_flash->led1_status;

	pr_debug("led_ctrl %d %d status %d %d\n", flash_ctx->set_flash.led0_ctrl,
		flash_ctx->set_flash.led1_ctrl, flash_ctx->set_flash.led0_status,
		flash_ctx->set_flash.led1_status);
	if ((set_flash->led0_ctrl && set_flash->led0_status < FLASH_STATUS_MAX) ||
		(set_flash->led1_ctrl && set_flash->led1_status < FLASH_STATUS_MAX)) {

			flash_ctx->frame_skipped++;
			if (flash_ctx->frame_skipped >=
				flash_ctx->skip_number) {
				/* flash lighted at the last SOF before
				 * the right capture frame
				 */
				pr_debug("waiting finished\n");
			} else {
				need_light = 0;
				pr_debug("wait for the next SOF, %d %d\n",
					flash_ctx->frame_skipped,
					flash_ctx->skip_number);
			}

			if (need_light) {
				complete(&flash_ctx->flash_thread_com);
			}
		}

	return ret;
}


static void sprd_cam_flash_set_frame_skip(struct cam_flash_task_info *flash_ctx, int skip_frame)
{
	if (flash_ctx)
		flash_ctx->skip_number = skip_frame;
}

static int img_opt_flash(void *param)
{
	struct cam_flash_task_info *flash_ctx = NULL;
	uint32_t led0_ctrl;
	uint32_t led1_ctrl;
	uint32_t led0_status;
	uint32_t led1_status;

	flash_ctx = (struct cam_flash_task_info *)param;
	if (!flash_ctx)
		return 0;

	led0_ctrl = flash_ctx->set_flash.led0_ctrl;
	led1_ctrl = flash_ctx->set_flash.led1_ctrl;
	led0_status = flash_ctx->set_flash.led0_status;
	led1_status = flash_ctx->set_flash.led1_status;

	pr_debug("led_ctrl %d %d status %d %d\n", led0_ctrl, led1_ctrl, led0_status, led1_status);
	if ((led0_ctrl && led0_status < FLASH_STATUS_MAX) ||
		(led1_ctrl && led1_status < FLASH_STATUS_MAX)) {
		if (led0_status == FLASH_CLOSE_AFTER_AUTOFOCUS ||
			led1_status == FLASH_CLOSE_AFTER_AUTOFOCUS) {
			/*cam_get_timestamp(&falsh_task->timestamp);*/
			flash_ctx->after_af = 1;
			pr_debug("time, %d %d\n",
				(int)flash_ctx->timestamp.tv_sec,
				(int)flash_ctx->timestamp.tv_usec);
		}
		sprd_cam_flash_ctrl(flash_ctx->cam_idx, &flash_ctx->set_flash);
		flash_ctx->set_flash.led0_ctrl = 0;
		flash_ctx->set_flash.led1_ctrl = 0;
		flash_ctx->set_flash.led0_status = FLASH_STATUS_MAX;
		flash_ctx->set_flash.led1_status = FLASH_STATUS_MAX;
	}

	return 0;
}

static int flash_thread_loop(void *arg)
{
	struct cam_flash_task_info *flash_ctx = NULL;
	struct sprd_img_set_flash set_flash;

	flash_ctx = (struct cam_flash_task_info *)arg;

	if (flash_ctx == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	while (1) {
		if (wait_for_completion_interruptible(
			&flash_ctx->flash_thread_com) == 0) {
			if (flash_ctx->is_flash_thread_stop) {
				set_flash.led0_ctrl = 1;
				set_flash.led1_ctrl = 1;
				set_flash.led0_status = FLASH_CLOSE;
				set_flash.led1_status = FLASH_CLOSE;
				set_flash.flash_index = 0;
				sprd_cam_flash_ctrl(flash_ctx->cam_idx,
					&set_flash);
				set_flash.flash_index = 1;
				sprd_cam_flash_ctrl(flash_ctx->cam_idx,
					&set_flash);
				pr_debug("_flash_thread_loop stop\n");
				break;
			}
			img_opt_flash(arg);
		} else {
			pr_debug("flash int!");
			break;
		}
	}
	flash_ctx->is_flash_thread_stop = 0;

	return 0;
}

static struct cam_flash_ops flash_core_ops = {
	.open = sprd_cam_flash_open,
	.close = sprd_cam_flash_close,
	.cfg_flash = sprd_cam_flash_cfg,
	.get_flash = sprd_cam_flash_info_get,
	.start_flash = sprd_cam_flash_start,
	.set_frame_skip = sprd_cam_flash_set_frame_skip,
};

struct cam_flash_task_info *get_cam_flash_handle(uint32_t cam_idx)
{
	struct cam_flash_task_info *flash_ctx = NULL;
	char thread_name[20] = { 0 };

	flash_ctx = kzalloc(sizeof(*flash_ctx), GFP_KERNEL);
	if (!flash_ctx) {
		pr_err("fail to alloc flash task.\n");
		return NULL;
	}

	flash_ctx->cam_idx = cam_idx;
	flash_ctx->set_flash.led0_ctrl = 0;
	flash_ctx->set_flash.led1_ctrl = 0;
	flash_ctx->set_flash.led0_status = FLASH_STATUS_MAX;
	flash_ctx->set_flash.led1_status = FLASH_STATUS_MAX;
	flash_ctx->set_flash.flash_index = 0;

	flash_ctx->after_af = 0;

	flash_ctx->flash_core_ops = &flash_core_ops;

	flash_ctx->is_flash_thread_stop = 0;
	init_completion(&flash_ctx->flash_thread_com);
	sprintf(thread_name, "cam%d_flash_thread", flash_ctx->cam_idx);
	flash_ctx->flash_thread = kthread_run(flash_thread_loop,
		flash_ctx, "%s", thread_name);
	if (IS_ERR_OR_NULL(flash_ctx->flash_thread)) {
		pr_err("fail to create flash thread\n");
		kfree(flash_ctx);
		return NULL;
	}

	return flash_ctx;
}

int put_cam_flash_handle(struct cam_flash_task_info *flash_ctx)
{
	int ret = 0;

	if (!flash_ctx) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	if (flash_ctx->flash_thread) {
		flash_ctx->is_flash_thread_stop = 1;
		complete(&flash_ctx->flash_thread_com);
		if (flash_ctx->is_flash_thread_stop != 0) {
			while (flash_ctx->is_flash_thread_stop)
				udelay(1000);
		}
		flash_ctx->flash_thread = NULL;
	}
	kfree(flash_ctx);
	return ret;
}

