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

#ifndef _CAM_FLASH_H_
#define _CAM_FLASH_H_

#include <linux/kthread.h>
#include <linux/delay.h>

#include "dcam_drv.h"
#include "flash_drv.h"

struct flash_led_task {
	struct sprd_img_set_flash set_flash;
	uint32_t frame_skipped;
	uint32_t after_af;
	struct timeval timestamp;
	uint32_t skip_number;/*cap skip*/
	enum dcam_id dcam_idx;
	struct completion flash_thread_com;
	struct task_struct *flash_thread;
	uint32_t is_flash_thread_stop;
};

int sprd_cam_flash_set(enum dcam_id idx,
		struct sprd_img_set_flash *set_flash);
int sprd_cam_flash_start(struct camera_frame *frame, void *param);
int sprd_cam_flash_task_create(struct flash_led_task **task,
	enum dcam_id cam_idx);
int sprd_cam_flash_task_destroy(struct flash_led_task *task);

#endif /* _CAM_FLASH_H_ */
