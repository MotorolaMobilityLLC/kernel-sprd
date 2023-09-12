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

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/sprd_iommu.h>
#include <linux/sprd_ion.h>
#include <linux/workqueue.h>
#include <linux/pm_wakeup.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <linux/pm_runtime.h>
#include <linux/ion.h>
#endif

#include "dcam_core.h"
#include "dcam_buf.h"
#include "sprd_sensor_drv.h"
#if defined(CONFIG_COMPAT)
#include "compat_isp_drv.h"
#endif
#include "cam_dbg.h"
struct wakeup_source *Camera_Sys_Wakelock;

/* Macro Definitions */
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "DCAM_CORE: %d %d %s : " \
	fmt, current->pid, __LINE__, __func__

#define DISCARD_FRAME_TIME		10000

/* Static Variables Declaration */
static struct camera_format dcam_img_fmt[] = {
	{
		.name = "4:2:2, packed, YUYV",
		.fourcc = IMG_PIX_FMT_YUYV,
		.depth = 16,
	},
	{
		.name = "4:2:2, packed, YVYU",
		.fourcc = IMG_PIX_FMT_YVYU,
		.depth = 16,
	},
	{
		.name = "4:2:2, packed, UYVY",
		.fourcc = IMG_PIX_FMT_UYVY,
		.depth = 16,
	},
	{
		.name = "4:2:2, packed, VYUY",
		.fourcc = IMG_PIX_FMT_VYUY,
		.depth = 16,
	},
	{
		.name = "YUV 4:2:2, planar, (Y-Cb-Cr)",
		.fourcc = IMG_PIX_FMT_YUV422P,
		.depth = 16,
	},
	{
		.name = "YUV 4:2:0 planar (Y-CbCr)",
		.fourcc = IMG_PIX_FMT_NV12,
		.depth = 12,
	},
	{
		.name = "YVU 4:2:0 planar (Y-CrCb)",
		.fourcc = IMG_PIX_FMT_NV21,
		.depth = 12,
	},
	{
		.name = "YUV 4:2:0 planar (Y-Cb-Cr)",
		.fourcc = IMG_PIX_FMT_YUV420,
		.depth = 12,
	},
	{
		.name = "YVU 4:2:0 planar (Y-Cr-Cb)",
		.fourcc = IMG_PIX_FMT_YVU420,
		.depth = 12,
	},
	{
		.name = "RGB565 (LE)",
		.fourcc = IMG_PIX_FMT_RGB565,
		.depth = 16,
	},
	{
		.name = "RGB565 (BE)",
		.fourcc = IMG_PIX_FMT_RGB565X,
		.depth = 16,
	},
	{
		.name = "RawRGB",
		.fourcc = IMG_PIX_FMT_GREY,
		.depth = 8,
	},
	{
		.name = "JPEG",
		.fourcc = IMG_PIX_FMT_JPEG,
		.depth = 8,
	},
};

typedef int (*dcam_cfg_fun_ptr)(struct isp_io_param *dcam_param,
				enum dcam_id idx);

struct dcam_cfg_fun {
	uint32_t sub_block;
	dcam_cfg_fun_ptr cfg_fun;
};

static struct dcam_cfg_fun dcam_cfg_fun_tab[] = {
	 {DCAM_BLOCK_BLC,		dcam_k_cfg_blc},
	 {DCAM_BLOCK_RAW_AEM,		dcam_k_cfg_raw_aem},
};

int sprd_camera_stream_off(struct camera_group *group,
				  enum dcam_id idx);

/* Internal Function Implementation */
int img_get_timestamp(struct timeval *tv)
{
	struct timespec ts;

	ktime_get_ts(&ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / NSEC_PER_USEC;

	return 0;
}

/* Internal Function Implementation */
void gen_frm_timestamp(struct frm_timestamp *pts)
{
	struct timespec ts;

	ktime_get_ts(&ts);
//	pts->boot_time = timespec_to_ktime(ts);
	pts->boot_time = ktime_get_boottime();
	pts->time.tv_sec = ts.tv_sec;
	pts->time.tv_usec = ts.tv_nsec / NSEC_PER_USEC;
}

static int sprd_img_buf_queue_init(struct camera_img_buf_queue *queue)
{
	if (queue == NULL)
		return -EINVAL;

	memset(queue, 0, sizeof(*queue));
	queue->write = &queue->buf_addr[0];
	queue->read = &queue->buf_addr[0];

	return 0;
}

static int sprd_img_setflash(enum dcam_id idx,
			     struct sprd_img_set_flash *set_flash)
{

	sprd_flash_ctrl(set_flash);
	DCAM_TRACE("%d set flash\n", idx);

	return 0;
}

static int sprd_img_opt_flash(struct camera_frame *frame, void *param)
{
	struct camera_dev *dev = (struct camera_dev *)param;
	struct camera_info *info = NULL;
	uint32_t led0_ctrl;
	uint32_t led1_ctrl;
	uint32_t led0_status;
	uint32_t led1_status;

	if (dev == NULL) {
		DCAM_TRACE("dev is NULL\n");
		return 0;
	}

	info = &dev->dcam_cxt;
	led0_ctrl = info->set_flash.led0_ctrl;
	led1_ctrl = info->set_flash.led1_ctrl;
	led0_status = info->set_flash.led0_status;
	led1_status = info->set_flash.led1_status;

	if ((led0_ctrl && led0_status < FLASH_STATUS_MAX) ||
	    (led1_ctrl && led1_status < FLASH_STATUS_MAX)) {
		DCAM_TRACE("led0_status %d led1_status %d\n",
			   led0_status, led1_status);
		if (led0_status == FLASH_CLOSE_AFTER_AUTOFOCUS ||
		    led1_status == FLASH_CLOSE_AFTER_AUTOFOCUS) {
			img_get_timestamp(&info->timestamp);
			info->after_af = 1;
			DCAM_TRACE("time, %d %d\n",
				   (int)info->timestamp.tv_sec,
				   (int)info->timestamp.tv_usec);
		}
		sprd_img_setflash(dev->idx, &info->set_flash);
		info->set_flash.led0_ctrl = 0;
		info->set_flash.led1_ctrl = 0;
		info->set_flash.led0_status = FLASH_STATUS_MAX;
		info->set_flash.led1_status = FLASH_STATUS_MAX;
	}

	return 0;
}

int sprd_img_start_flash(struct camera_frame *frame, void *param)
{
	struct camera_dev *dev = (struct camera_dev *)param;
	struct camera_info *info = NULL;
	uint32_t need_light = 1;
	uint32_t led0_ctrl;
	uint32_t led1_ctrl;
	uint32_t led0_status;
	uint32_t led1_status;

	if (dev == NULL) {
		DCAM_TRACE("dev is NULL\n");
		return -1;
	}

	info = &dev->dcam_cxt;
	led0_ctrl = info->set_flash.led0_ctrl;
	led1_ctrl = info->set_flash.led1_ctrl;
	led0_status = info->set_flash.led0_status;
	led1_status = info->set_flash.led1_status;

	if ((led0_ctrl && led0_status < FLASH_STATUS_MAX) ||
		(led1_ctrl && led1_status < FLASH_STATUS_MAX)) {
		if ((led0_ctrl && FLASH_HIGH_LIGHT == led0_status) ||
			(led1_ctrl && FLASH_HIGH_LIGHT == led1_status)) {
			dev->frame_skipped++;
			if (dev->frame_skipped >= info->skip_number) {
				/* flash lighted at the last SOF before
				* the right capture frame
				*/
				DCAM_TRACE("waiting finished\n");
			} else {
				need_light = 0;
				DCAM_TRACE("wait for the next SOF, %d %d\n",
					dev->frame_skipped,
					info->skip_number);
			}
		}
		if (need_light)
			complete(&dev->flash_thread_com);
	}

	return 0;
}

static int flash_thread_loop(void *arg)
{
	struct camera_dev *dev = (struct camera_dev *)arg;
	struct sprd_img_set_flash set_flash;

	if (dev == NULL) {
		DCAM_TRACE("flash_thread_loop, dev is NULL\n");
		return -1;
	}
	while (1) {
		if (wait_for_completion_interruptible(
			&dev->flash_thread_com) == 0) {
			if (dev->is_flash_thread_stop) {
				set_flash.led0_ctrl = 1;
				set_flash.led1_ctrl = 1;
				set_flash.led0_status = FLASH_CLOSE;
				set_flash.led1_status = FLASH_CLOSE;
				set_flash.flash_index = 0;
				sprd_img_setflash(dev->idx, &set_flash);
				set_flash.flash_index = 1;
				sprd_img_setflash(dev->idx, &set_flash);
				DCAM_TRACE("flash_thread_loop stop\n");
				break;
			}
			sprd_img_opt_flash(NULL, arg);
		} else {
			DCAM_TRACE("flash int!");
			break;
		}
	}
	dev->is_flash_thread_stop = 0;

	return 0;
}

static int dcam_create_flash_thread(void *param)
{
	struct camera_dev *dev = (struct camera_dev *)param;
	char thread_name[20] = { 0 };

	if (dev == NULL) {
		DCAM_TRACE("dev is NULL\n");
		return -1;
	}
	dev->is_flash_thread_stop = 0;
	init_completion(&dev->flash_thread_com);
	sprintf(thread_name, "dcam%d_flash_thread", dev->idx);
	dev->flash_thread = kthread_run(flash_thread_loop, param, "%s",
					thread_name);
	if (IS_ERR(dev->flash_thread)) {
		pr_err("fail to create flash thread\n");
		return -1;
	}

	return 0;
}

static int dcam_stop_flash_thread(void *param)
{
	struct camera_dev *dev = (struct camera_dev *)param;

	if (dev == NULL) {
		DCAM_TRACE("dev is NULL\n");
		return -1;
	}

	if (dev->flash_thread) {
		dev->is_flash_thread_stop = 1;
		complete(&dev->flash_thread_com);
		if (dev->is_flash_thread_stop != 0) {
			while (dev->is_flash_thread_stop)
				udelay(1000);
		}
		dev->flash_thread = NULL;
	}

	return 0;
}

static int sprd_img_get_path_index(uint32_t channel_id)
{
	int path_index;

	switch (channel_id) {
	case CAMERA_PRE_PATH:
		path_index = ISP_PATH_IDX_PRE;
		break;
	case CAMERA_VID_PATH:
		path_index = ISP_PATH_IDX_VID;
		break;
	case CAMERA_CAP_PATH:
		path_index = ISP_PATH_IDX_CAP;
		break;
	default:
		path_index = ISP_PATH_IDX_ALL;
		pr_info("fail to get path index, channel %d\n", channel_id);
	}

	return path_index;
}

static int sprd_img_pulse_queue_init(struct camera_pulse_queue *queue)
{
	unsigned long flags;

	if (queue == NULL)
		return -EINVAL;

	spin_lock_irqsave(&queue->lock, flags);
	memset(queue->node, 0,
	       sizeof(struct sprd_img_vcm_param) * CAMERA_QUEUE_LENGTH);
	queue->write = &queue->node[0];
	queue->read = &queue->node[0];
	queue->wcnt = 0;
	queue->rcnt = 0;
	spin_unlock_irqrestore(&queue->lock, flags);

	return 0;
}

static int sprd_img_pulse_write(struct camera_pulse_queue *queue,
				struct sprd_img_vcm_param *node)
{
	unsigned long flags;
	struct sprd_img_vcm_param *ori_node;

	if (NULL == queue || NULL == node)
		return -EINVAL;

	spin_lock_irqsave(&queue->lock, flags);
	ori_node = queue->write;
	queue->wcnt++;
	*queue->write++ = *node;
	if (queue->write > &queue->node[CAMERA_QUEUE_LENGTH - 1])
		queue->write = &queue->node[0];

	if (queue->write == queue->read) {
		queue->write = ori_node;
		pr_warn("warning, queue is full\n");
	}
	spin_unlock_irqrestore(&queue->lock, flags);

	return 0;
}

static int sprd_img_pulse_read(struct camera_pulse_queue *queue,
			       struct sprd_img_vcm_param *node)
{
	unsigned long flags;
	int ret = DCAM_RTN_SUCCESS;
	int flag = 0;

	if (NULL == queue || NULL == node)
		return -EINVAL;

	spin_lock_irqsave(&queue->lock, flags);
	if (queue->read != queue->write) {
		flag = 1;
		*node = *queue->read;
		queue->read++;
		queue->rcnt++;
		if (queue->read > &queue->node[CAMERA_QUEUE_LENGTH - 1])
			queue->read = &queue->node[0];
	}

	if (!flag)
		ret = -EAGAIN;
	spin_unlock_irqrestore(&queue->lock, flags);

	return ret;
}

static int sprd_img_queue_init(struct camera_queue *queue)
{
	unsigned long flags;

	if (queue == NULL)
		return -EINVAL;

	spin_lock_irqsave(&queue->lock, flags);
	memset(queue->node, 0,
	       sizeof(struct camera_node) * CAMERA_QUEUE_LENGTH);
	queue->write = &queue->node[0];
	queue->read = &queue->node[0];
	queue->wcnt = 0;
	queue->rcnt = 0;
	spin_unlock_irqrestore(&queue->lock, flags);

	return 0;
}

static int dcam_core_stats_init(struct camera_dev *dev)
{
	int ret = 0;

	ret = dcam_statis_queue_init(&dev->statis_module_info.aem_statis_queue);
	if (unlikely(ret != 0)) {
		pr_err("fail to init aem queue\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = dcam_statis_queue_init(
			&dev->statis_module_info.pdaf_statis_queue);
	if (unlikely(ret != 0)) {
		pr_err("fail to init pdaf queue\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = dcam_statis_queue_init(
			&dev->statis_module_info.ebd_statis_queue);
	if (unlikely(ret != 0)) {
		pr_err("fail to init embed line queue\n");
		ret = -EINVAL;
		goto exit;
	}
	ret = dcam_statis_queue_init(
			&dev->statis_module_info.raw_statis_queue);
	if (unlikely(ret != 0)) {
		pr_err("fail to init raw dump queue\n");
		ret = -EINVAL;
		goto exit;
	}

	dcam_statis_frm_queue_init(
		&dev->statis_module_info.aem_statis_frm_queue);
	dcam_statis_frm_queue_init(
		&dev->statis_module_info.pdaf_statis_frm_queue);
	dcam_statis_frm_queue_init(
		&dev->statis_module_info.ebd_statis_frm_queue);
	dcam_statis_frm_queue_init(
		&dev->statis_module_info.raw_statis_frm_queue);

exit:
	return ret;
}

void sprd_isp_buffer_deinit(void *isp_handle)
{
	struct isp_pipe_dev *dev = NULL;
	struct isp_module *module = NULL;
	struct isp_path_desc *path = NULL;
	uint32_t i = 0;

	if (!isp_handle)
		return;

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->module_info;

	for (i = ISP_SCL_PRE; i <= ISP_SCL_CAP; i++){
		path = &module->isp_path[i];
		isp_buf_queue_init(&path->buf_queue);
	}
}

static int sprd_img_local_deinit(struct camera_dev *dev)
{
	int ret = 0;
	int i;
	struct camera_path_spec *path;

	sprd_isp_buffer_deinit(dev->isp_dev_handle);

	for (i = 0; i < CAMERA_MAX_PATH; i++) {
		path = &dev->dcam_cxt.dcam_path[i];
		DCAM_TRACE("local_deinit, path %d cnt %d\n",
			   i, path->frm_cnt_act);
		if (unlikely(NULL == dev || NULL == path))
			return -EINVAL;

		path->is_work = 0;
		path->frm_cnt_act = 0;
		sprd_img_buf_queue_init(&path->buf_queue);
		sprd_img_buf_queue_init(&path->tmp_buf_queue);
	}

	ret = sprd_img_queue_init(&dev->queue);
	if (unlikely(ret != 0))
		pr_err("fail to init queue\n");

	ret = sprd_img_pulse_queue_init(
		&dev->dcam_cxt.pulse_info.vcm_queue);
	if (unlikely(ret != 0))
		pr_err("fail to init pulse queue\n");

	ret = dcam_core_stats_init(dev);
	if (unlikely(ret != 0))
		pr_err("fail to init stats\n");

	ret = pfiommu_free_addr(&dev->statis_module_info.img_statis_buf.pfinfo);
	if (ret != 0)
		pr_err("fail to free img statis buf\n");

	DCAM_TRACE("local_deinit, frm_cnt_act %d\n",
		   path->frm_cnt_act);

	return 0;
}

static int sprd_dcam_cfg_raw_path(struct camera_path_spec *path,
	enum dcam_id idx)
{
	int ret = 0;
	struct camera_img_buf_addr *cur_node;
	struct camera_img_buf_queue *queue;
	struct camera_addr frm_addr;
	uint32_t param;
	struct size_transfer tmp;

	if (path == NULL) {
		ret = -EINVAL;
		goto exit;
	}

	ret = set_dcam_raw_path_cfg(idx, DCAM_PATH_FRM_DECI,
		&path->path_frm_deci);
	if (unlikely(ret)) {
		pr_err("fail to raw path cfg deci %d", ret);
		goto exit;
	}

	ret = set_dcam_raw_path_cfg(idx, DCAM_PATH_DATA_ENDIAN,
		&path->end_sel);
	if (unlikely(ret)) {
		pr_err("fail to raw path cfg endian %d", ret);
		goto exit;
	}

	/* save raw mode(Bin or Full) */
	param = path->is_work;
	ret = set_dcam_raw_path_cfg(idx, DCAM_PATH_ENABLE, &param);
	if (unlikely(ret)) {
		pr_err("fail to raw path cfg en %d", ret);
		goto exit;
	}
	/* set binning ratio, must before DCAM_PATH_INPUT_RECT */
	ret = set_dcam_raw_path_cfg(idx, DCAM_PATH_BIN_RATIO,
		&path->bin_ratio);
	if (unlikely(ret)) {
		pr_err("fail to raw path cfg bin ratio %d", ret);
		goto exit;
	}
	/* for transfer parameter: size in, out, in_rect */
	tmp.pin = &(path->in_size);
	tmp.pout = &(path->out_size);
	tmp.prect = &(path->in_rect);
	ret = set_dcam_raw_path_cfg(idx, DCAM_PATH_INPUT_RECT, &tmp);
	if (unlikely(ret)) {
		pr_err("fail to raw path cfg input rect %d", ret);
		goto exit;
	}

	queue = &path->buf_queue;
	for (cur_node = queue->read; cur_node != queue->write; cur_node++) {
		if (cur_node > &queue->buf_addr[DCAM_FRM_CNT_MAX - 1])
			cur_node = &queue->buf_addr[0];

		frm_addr.yaddr = cur_node->frm_addr.yaddr;
		frm_addr.uaddr = cur_node->frm_addr.uaddr;
		frm_addr.vaddr = cur_node->frm_addr.vaddr;
		frm_addr.yaddr_vir = cur_node->frm_addr_vir.yaddr;
		frm_addr.uaddr_vir = cur_node->frm_addr_vir.uaddr;
		frm_addr.vaddr_vir = cur_node->frm_addr_vir.vaddr;
		frm_addr.mfd_y = cur_node->frm_addr.mfd_y;
		frm_addr.mfd_u = cur_node->frm_addr.mfd_u;
		frm_addr.mfd_v = cur_node->frm_addr.mfd_v;
		ret = set_dcam_raw_path_cfg(idx, DCAM_PATH_OUTPUT_ADDR,
			&frm_addr);
		if (unlikely(ret)) {
			pr_err("fail to raw path cfg output addr %d", ret);
			goto exit;
		}
	}

	ret = set_dcam_raw_path_cfg(idx, DCAM_PATH_OUTPUT_RESERVED_ADDR,
		&path->frm_reserved_addr);
	if (unlikely(ret)) {
		pr_err("fail to raw path cfg output reserved addr %d", ret);
		goto exit;
	}

	path->status = PATH_RUN;
exit:

	return ret;
}

static int sprd_dcam_cfg_full_path(struct camera_path_spec *path,
				   enum dcam_id idx)
{
	int ret = 0;
	uint32_t param;
	struct size_transfer tmp;

	if (path == NULL || (int)idx < 0 || idx >= DCAM_ID_MAX) {
		ret = -EINVAL;
		goto exit;
	}

	ret = set_dcam_full_path_cfg(idx, DCAM_PATH_DATA_ENDIAN,
		&path->end_sel);
	if (unlikely(ret)) {
		pr_err("fail to full path cfg endian %d", ret);
		goto exit;
	}
	/* set binning ratio, must before DCAM_PATH_INPUT_RECT */
	ret = set_dcam_full_path_cfg(idx, DCAM_PATH_BIN_RATIO,
		&path->bin_ratio);
	if (unlikely(ret)) {
		pr_err("fail to full path cfg bin ratio %d", ret);
		goto exit;
	}
	/* for transfer parameter: size in, out, in_rect */
	tmp.pin = &(path->in_size);
	tmp.pout = &(path->out_size);
	tmp.prect = &(path->in_rect);
	ret = set_dcam_full_path_cfg(idx, DCAM_PATH_INPUT_RECT, &tmp);
	if (unlikely(ret)) {
		pr_err("fail to full path cfg input rect %d", ret);
		goto exit;
	}

	param = 1;
	ret = set_dcam_full_path_cfg(idx, DCAM_PATH_ENABLE, &param);
	if (unlikely(ret)) {
		pr_err("fail to full path cfg en %d", ret);
		goto exit;
	}
	path->status = PATH_RUN;
exit:

	return ret;
}

static int sprd_dcam_cfg_bin_path(struct camera_path_spec *path,
				  enum dcam_id idx)
{
	int ret = 0;
	uint32_t param;
	struct size_transfer tmp;

	if (path == NULL || (int)idx < 0 || idx >= DCAM_ID_MAX) {
		ret = -EINVAL;
		goto exit;
	}

	ret = set_dcam_bin_path_cfg(idx, DCAM_PATH_FRM_DECI,
		&path->path_frm_deci);
	if (unlikely(ret)) {
		pr_err("fail to bin path cfg deci %d", ret);
		goto exit;
	}

	ret = set_dcam_bin_path_cfg(idx, DCAM_PATH_DATA_ENDIAN,
		&path->end_sel);
	if (unlikely(ret)) {
		pr_err("fail to bin path cfg endian %d", ret);
		goto exit;
	}
	/* set binning ratio, must before DCAM_PATH_INPUT_RECT */
	ret = set_dcam_bin_path_cfg(idx, DCAM_PATH_BIN_RATIO, &path->bin_ratio);
	if (unlikely(ret)) {
		pr_err("fail to bin path cfg bin ratio %d", ret);
		goto exit;
	}

	/* for transfer parameter: size in, out, in_rect */
	tmp.pin = &(path->in_size);
	tmp.pout = &(path->out_size);
	tmp.prect = &(path->in_rect);
	ret = set_dcam_bin_path_cfg(idx, DCAM_PATH_INPUT_RECT, &tmp);
	if (unlikely(ret)) {
		pr_err("fail to bin path cfg input rect %d", ret);
		goto exit;
	}

	pr_debug("Final path src size:%d %d, trim:%d %d, out:%d %d\n",
			path->in_size.w, path->in_size.h,
			path->in_rect.w, path->in_rect.h,
			path->out_size.w, path->out_size.h);

	param = 1;
	ret = set_dcam_bin_path_cfg(idx, DCAM_PATH_ENABLE, &param);
	if (unlikely(ret)) {
		pr_err("fail to bin path cfg en %d", ret);
		goto exit;
	}
	path->status = PATH_RUN;
exit:

	return ret;
}

static int sprd_dcam_cfg_pdaf_path(struct camera_path_spec *path_pdaf,
	enum dcam_id idx)
{
	int ret = 0;
	uint32_t param;

	if (path_pdaf == NULL) {
		ret = -EINVAL;
		goto exit;
	}
	ret = set_dcam_pdaf_path_cfg(idx, DCAM_PDAF_CONTROL,
		&path_pdaf->pdaf_ctrl);
	if (unlikely(ret)) {
		pr_err("fail to pdaf path cfg ctl %d", ret);
		goto exit;
	}

	param = 1;
	ret = set_dcam_pdaf_path_cfg(idx, DCAM_PATH_ENABLE, &param);
	if (unlikely(ret)) {
		pr_err("fail to pdaf path cfg en %d", ret);
		goto exit;
	}
	path_pdaf->status = PATH_RUN;

exit:
	return ret;
}

static int sprd_dcam_cfg_aem_path(struct camera_path_spec *aem_path,
	enum dcam_id idx)
{
	int ret = 0;
	uint32_t param;

	if (aem_path == NULL) {
		ret = -EINVAL;
		goto exit;
	}

	param = 1;
	ret = set_dcam_aem_path_cfg(idx, DCAM_PATH_ENABLE, &param);
	if (unlikely(ret)) {
		pr_err("fail to aem path cfg en %d", ret);
		goto exit;
	}
	param = 2; /* little endian */
	ret = set_dcam_aem_path_cfg(idx, DCAM_PATH_DATA_ENDIAN, &param);
	if (unlikely(ret)) {
		pr_err("fail to aem path cfg endian %d", ret);
		goto exit;
	}
	aem_path->status = PATH_RUN;
exit:
	return ret;
}

static int sprd_dcam_cfg_ebd_path(struct camera_path_spec *path,
	enum dcam_id idx)
{
	int ret = 0;
	uint32_t param = 0;

	if (path == NULL) {
		ret = -EINVAL;
		goto exit;
	}
	ret = set_dcam_ebd_path_cfg(idx, DCAM_EBD_CONTROL,
		&path->ebd_ctrl);
	if (unlikely(ret)) {
		pr_err("fail to path cfg ctl %d", ret);
		goto exit;
	}

	param = 1;
	ret = set_dcam_ebd_path_cfg(idx, DCAM_PATH_ENABLE, &param);
	if (unlikely(ret)) {
		pr_err("fail to path cfg en %d", ret);
		goto exit;
	}
	path->status = PATH_RUN;

exit:
	return ret;
}

static int set_isp_path_buf_cfg(void *handle, enum isp_cfg_id cfg_id,
				enum isp_path_index path_index,
				struct camera_img_buf_queue *queue)
{
	int ret = 0;
	int fret = -1;
	struct camera_img_buf_addr *cur_node;
	struct camera_addr frm_addr;

	if (!queue || !handle) {
		ret = -EINVAL;
		pr_err("fail to get params queue %p handle %p", queue, handle);
		goto exit;
	}

	for (cur_node = queue->read; cur_node != queue->write; cur_node++) {
		if (cur_node > &queue->buf_addr[DCAM_FRM_CNT_MAX - 1])
			cur_node = &queue->buf_addr[0];

		frm_addr.yaddr = cur_node->frm_addr.yaddr;
		frm_addr.uaddr = cur_node->frm_addr.uaddr;
		frm_addr.vaddr = cur_node->frm_addr.vaddr;
		frm_addr.yaddr_vir = cur_node->frm_addr_vir.yaddr;
		frm_addr.uaddr_vir = cur_node->frm_addr_vir.uaddr;
		frm_addr.vaddr_vir = cur_node->frm_addr_vir.vaddr;
		frm_addr.mfd_y = cur_node->frm_addr.mfd_y;
		frm_addr.mfd_u = cur_node->frm_addr.mfd_u;
		frm_addr.mfd_v = cur_node->frm_addr.mfd_v;
		frm_addr.user_fid = cur_node->frm_addr.user_fid;
		ret = set_isp_path_cfg(handle, path_index, cfg_id, &frm_addr);
		if(unlikely(fret)){
		    fret=ret;
		}
	}

exit:
    if(ret==0 || fret==0)
       return 0;
	return ret;
}

static int sprd_isp_path_cfg_block(struct camera_path_spec *path, void *handle,
				   enum isp_path_index path_index)
{
	int ret = 0;
	uint32_t param = 0;
	struct isp_endian_sel endian;
	struct camera_rect isp_in_rect;

	memset(&endian, 0, sizeof(struct isp_endian_sel));

	if (!path || !handle) {
		pr_err("fail to get ptr path %p handle %p\n", path, handle);
		return -EINVAL;
	}

	ret = set_isp_path_cfg(handle, path_index, ISP_PATH_INPUT_SIZE,
			       &path->out_size);
	if (unlikely(ret)) {
		pr_err("fail to isp path cfg input size %d", ret);
		goto exit;
	}

	isp_in_rect.w = path->in_rect.w;
	isp_in_rect.h = path->in_rect.h;
	isp_in_rect.x = path->in_rect.x;
	isp_in_rect.y = path->in_rect.y;
	ret = set_isp_path_cfg(handle, path_index, ISP_PATH_INPUT_RECT,
			       &isp_in_rect);
	if (unlikely(ret)) {
		pr_err("fail to isp path cfg input rect %d", ret);
		goto exit;
	}

	ret = set_isp_path_cfg(handle, path_index, ISP_PATH_INPUT_FORMAT,
			       &path->isp_fetch_fmt);
	if (unlikely(ret)) {
		pr_err("fail to isp path cfg input format %d", ret);
		goto exit;
	}

	ret = set_isp_path_cfg(handle, path_index, ISP_PATH_OUTPUT_SIZE,
			       &path->isp_out_size);
	if (unlikely(ret)) {
		pr_err("fail to isp path cfg output size %d", ret);
		goto exit;
	}

	ret = set_isp_path_cfg(handle, path_index, ISP_PATH_OUTPUT_FORMAT,
			       &path->out_fmt);
	if (unlikely(ret)) {
		pr_err("fail to isp path cfg output format %d", ret);
		goto exit;
	}

	ret = set_isp_path_buf_cfg(handle, ISP_PATH_OUTPUT_ADDR, path_index,
				   &path->buf_queue);
	if (unlikely(ret)) {
		pr_err("fail to isp path cfg output addr %d", ret);
		goto exit;
	}

	ret = set_isp_path_cfg(handle, path_index,
			       ISP_PATH_OUTPUT_RESERVED_ADDR,
			       &path->frm_reserved_addr);
	if (unlikely(ret)) {
		pr_err("fail to isp path cfg output reserved addr %d", ret);
		goto exit;
	}

	ret = set_isp_path_cfg(handle, path_index, ISP_PATH_FRM_DECI,
			       &path->path_frm_deci);
	if (unlikely(ret)) {
		pr_err("fail to isp path cfg deci %d", ret);
		goto exit;
	}

	ret = set_isp_path_cfg(handle, path_index, ISP_PATH_MODE,
			       &path->path_mode);
	if (unlikely(ret)) {
		pr_err("fail to isp path cfg mode %d", ret);
		goto exit;
	}

	param = 1;
	ret = set_isp_path_cfg(handle, path_index, ISP_PATH_ENABLE, &param);
	if (unlikely(ret)) {
		pr_err("fail to isp path cfg en %d", ret);
		goto exit;
	}

	endian.y_endian = path->end_sel.y_endian;
	endian.uv_endian = path->end_sel.uv_endian;
	ret = set_isp_path_cfg(handle, path_index, ISP_PATH_DATA_ENDIAN,
			       &endian);
	if (unlikely(ret)) {
		pr_err("fail to isp path cfg endian %d", ret);
		goto exit;
	}
exit:
	return ret;
}

static int sprd_img_check_path_raw_cap(uint32_t fourcc,
				       struct sprd_img_format *f,
				       struct camera_info *info)
{
	struct camera_path_spec *path = &info->dcam_path[CAMERA_RAW_PATH];

	DCAM_TRACE("check format for raw path\n");

	path->frm_type = f->channel_id;
	path->is_work = 0;

	switch (fourcc) {
	case IMG_PIX_FMT_GREY:
		path->out_fmt = DCAM_RAWRGB;
		path->isp_fetch_fmt = info->is_loose ?
			ISP_FETCH_RAW_10 :
			ISP_FETCH_CSI2_RAW_10;
		path->end_sel.y_endian = DCAM_ENDIAN_LITTLE;
		break;

	default:
		pr_err("fail to supported format for raw path 0x%x\n", fourcc);
		return -EINVAL;
	}
	path->fourcc = fourcc;

	DCAM_TRACE("check format for raw path: out_fmt=%d, is_loose=%d\n",
		   path->out_fmt, info->is_loose);
/*
*	path->out_size.w = f->width;
*	path->out_size.h = f->height;
*	DCAM_TRACE("check format for path: out_size, w=%d, h=%d\n",
*			path->out_size.w, path->out_size.h);
*/
	path->is_work = 1;

	return 0;
}

static struct camera_format *sprd_img_get_format(uint32_t fourcc)
{
	struct camera_format *fmt;
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(dcam_img_fmt); i++) {
		fmt = &dcam_img_fmt[i];
		if (fmt->fourcc == fourcc)
			break;
	}

	if (unlikely(i >= ARRAY_SIZE(dcam_img_fmt)))
		return NULL;

	return &dcam_img_fmt[i];
}

static int sprd_img_check_binning(struct sprd_img_format *f,
				  struct camera_info *info,
				  struct camera_path_spec *path)
{
	uint32_t tempw;

	if (info->cap_out_size.w > DCAM_ISP_LINE_BUF_LENGTH) {
		if (info->if_mode == DCAM_CAP_IF_CCIR) {
			/* CCIR CAP, no binning */
			pr_err("fail to support CCIR CAP, no binning\n");
			return -EINVAL;
		} else if (info->if_mode == DCAM_CAP_IF_CSI2) {
			/* MIPI CAP,
			 * support 1/2 binning
			 */
			/* TODO: this should be moved to dcam_drv.c */
			DCAM_TRACE("Need Binning\n");
			tempw = path->in_rect.w;
			tempw = tempw >> 1;
			if (tempw > DCAM_ISP_LINE_BUF_LENGTH)
				return -EINVAL;

			info->img_deci.x_factor = 1;
			f->need_binning = 1;
			path->in_size.w = path->in_size.w >> 1;
			path->in_rect.x = path->in_rect.x >> 1;
			path->in_rect.w = path->in_rect.w >> 1;
			path->in_rect.w = path->in_rect.w & (~3);
		}
	}

	return 0;
}

static int sprd_img_check_scaling(struct sprd_img_format *f,
				  struct camera_info *info,
				  struct camera_path_spec *path,
				  uint32_t line_buf_size)
{
	uint32_t maxw, maxh, tempw, temph;

	tempw = path->in_rect.w;
	temph = path->in_rect.h;

	/* no need to scale */
	if (tempw == f->width && temph == f->height)
		return 0;

	/* scaling needed */
	switch (info->sn_mode) {
	case DCAM_CAP_MODE_RAWRGB:
		maxw = f->width * CAMERA_SC_COEFF_DOWN_MAX;
		maxw = maxw * (1 << CAMERA_PATH_DECI_FAC_MAX);
		maxh = f->height * CAMERA_SC_COEFF_DOWN_MAX;
		maxh = maxh * (1 << CAMERA_PATH_DECI_FAC_MAX);
		if (unlikely(tempw > maxw || temph > maxh)) {
			/* out of scaling capbility */
			pr_err("fail to scale out of capbility (%u %u) > (%u %u)\n",
				tempw, temph, maxw, maxh);
			return -EINVAL;
		}

		if (unlikely(f->width > line_buf_size)) {
			/* out of scaling capbility. TBD */
			pr_info("scale out of capbility %u %u\n",
				f->width, line_buf_size);
			/*return -EINVAL;*/
		}

		maxw = tempw * CAMERA_SC_COEFF_UP_MAX;
		maxh = temph * CAMERA_SC_COEFF_UP_MAX;
		if (unlikely(f->width > maxw || f->height > maxh)) {
			/* out of scaling capbility */
			pr_err("fail to scale out of capbility (%u %u) > (%u %u)\n",
				f->width, f->height, maxw, maxh);
			return -EINVAL;
		}
		break;

	default:
		break;
	}

	return 0;
}

static void sprd_img_endian_sel(uint32_t fourcc,
				struct camera_path_spec *path)
{
	if (fourcc == IMG_PIX_FMT_YUV422P ||
	    fourcc == IMG_PIX_FMT_RGB565 ||
	    fourcc == IMG_PIX_FMT_RGB565X) {
		if (fourcc == IMG_PIX_FMT_YUV422P) {
			path->out_fmt = DCAM_YUV422;
		} else {
			path->out_fmt = DCAM_RGB565;
			if (fourcc == IMG_PIX_FMT_RGB565)
				path->end_sel.y_endian = DCAM_ENDIAN_HALFBIG;
			else
				path->end_sel.y_endian = DCAM_ENDIAN_BIG;
		}
	} else {
		if (fourcc == IMG_PIX_FMT_YUV420 ||
		    fourcc == IMG_PIX_FMT_YVU420)
			path->out_fmt = DCAM_YUV420_3FRAME;
		else {
			if (fourcc == IMG_PIX_FMT_NV12) {
				path->out_fmt = DCAM_YVU420;
				path->end_sel.uv_endian = DCAM_ENDIAN_LITTLE;
			} else {
				path->out_fmt = DCAM_YUV420;
				path->end_sel.uv_endian = DCAM_ENDIAN_LITTLE;
			}
		}
	}
}

static int sprd_img_check_path_cap(uint32_t fourcc,
				   struct sprd_img_format *f,
				   struct camera_info *info,
				   enum camera_path_id path_id)
{
	int ret = 0;
	uint32_t tempw, temph;
	uint32_t line_buf_size;
	struct camera_path_spec *path;

	DCAM_TRACE("check format for path %d\n", path_id);

	switch (path_id) {
	case CAMERA_PRE_PATH:
		line_buf_size = ISP_PATH1_LINE_BUF_LENGTH;
		break;
	case CAMERA_VID_PATH:
		line_buf_size = ISP_PATH2_LINE_BUF_LENGTH;
		break;
	case CAMERA_CAP_PATH:
		line_buf_size = ISP_PATH3_LINE_BUF_LENGTH;
		break;
	default:
		return -EINVAL;
	}

	path = &info->dcam_path[path_id];
	path->frm_type = f->channel_id;
	path->is_from_isp = f->need_isp;
	path->rot_mode = f->flip_on;
	path->end_sel.y_endian = DCAM_ENDIAN_LITTLE;
	path->end_sel.uv_endian = DCAM_ENDIAN_LITTLE;
	path->is_work = 0;
	path->img_deci.x_factor = 0;
	path->img_deci.y_factor = 0;
	tempw = path->in_rect.w;
	temph = path->in_rect.h;
	info->img_deci.x_factor = 0;
	f->need_binning = 0;
	/* app should fill in this field(fmt.pix.priv) to set the base index
	 * of frame buffer, and lately this field will return the flag
	 * whether ISP is needed for this work path
	 */
	switch (fourcc) {
	case IMG_PIX_FMT_GREY:
	case IMG_PIX_FMT_JPEG:
	case IMG_PIX_FMT_YUYV:
	case IMG_PIX_FMT_YVYU:
	case IMG_PIX_FMT_UYVY:
	case IMG_PIX_FMT_VYUY:
		if (unlikely(f->width != tempw || f->height != temph)) {
			/* need need scaling or triming */
			pr_err("fail to scale src %d %d, dst %d %d\n",
			       tempw, temph, f->width, f->height);
			return -EINVAL;
		}

		if (fourcc == IMG_PIX_FMT_GREY) {
			if (unlikely(info->sn_mode != DCAM_CAP_MODE_RAWRGB)) {
				/* the output of sensor is not RawRGB
				 * which is needed by app
				 */
				pr_err("fail to support, it's not RawRGB sensor\n");
				return -EINVAL;
			}

			path->out_fmt = DCAM_RAWRGB;
			path->end_sel.y_endian = DCAM_ENDIAN_BIG;
		} else if (fourcc == IMG_PIX_FMT_JPEG) {
			if (unlikely(info->sn_mode != DCAM_CAP_MODE_JPEG)) {
				/* the output of sensor is not JPEG
				 * which is needed by app
				 */
				pr_err("fail to support, it's not JPEG sensor\n");
				return -EINVAL;
			}
			path->out_fmt = DCAM_JPEG;
		}
		break;
	case IMG_PIX_FMT_YUV422P:
	case IMG_PIX_FMT_YUV420:
	case IMG_PIX_FMT_YVU420:
	case IMG_PIX_FMT_NV12:
	case IMG_PIX_FMT_NV21:
	case IMG_PIX_FMT_RGB565:
	case IMG_PIX_FMT_RGB565X:
		if (info->sn_mode == DCAM_CAP_MODE_RAWRGB) {
			path->isp_fetch_fmt = info->is_loose ?
				ISP_FETCH_RAW_10 :
				ISP_FETCH_CSI2_RAW_10;

			if (path->is_from_isp) {
				/* check binning */
				ret = sprd_img_check_binning(f, info, path);
				if (ret)
					return ret;

				/* check scaling */
				ret = sprd_img_check_scaling(f, info, path,
							     line_buf_size);
				if (ret)
					return ret;
			} else {
				/* no isp, only RawRGB data can be sampled */
				pr_err("fail to support, RawRGB format 0x%x\n",
				       fourcc);
				return -EINVAL;
			}
		} else if (info->sn_mode == DCAM_CAP_MODE_YUV) {
			pr_debug("yuv sensor path id %d\n", path_id);
			path->isp_fetch_fmt = ISP_FETCH_YVU420_2FRAME;
		} else {
			pr_err("fail to support, sensor mode is 0x%x\n",
			       info->sn_mode);
			return -EINVAL;
		}
		sprd_img_endian_sel(fourcc, path);
		break;
	default:
		pr_err("fail to support, image format for path %d 0x%x\n",
			path_id, fourcc);
		return -EINVAL;
	}

	path->fourcc = fourcc;

	path->isp_out_size.w = f->width;
	path->isp_out_size.h = f->height;
	pr_debug("path %d: f->width %d, f->height %d\n", path_id,
		path->isp_out_size.w,
		path->isp_out_size.h);

	path->is_work = 1;

	return 0;
}

static int sprd_img_queue_write(struct camera_queue *queue,
				struct camera_node *node)
{
	unsigned long flags;
	struct camera_node *ori_node;

	if (NULL == queue || NULL == node)
		return -EINVAL;

	spin_lock_irqsave(&queue->lock, flags);
	ori_node = queue->write;
	queue->wcnt++;
	*queue->write++ = *node;
	if (queue->write > &queue->node[CAMERA_QUEUE_LENGTH - 1])
		queue->write = &queue->node[0];

	if (queue->write == queue->read) {
		queue->write = ori_node;
		pr_warn("warning, queue is full\n");
	}
	spin_unlock_irqrestore(&queue->lock, flags);

	return 0;
}

static int sprd_img_queue_read(struct camera_queue *queue,
			       struct camera_node *node)
{
	unsigned long flags;
	int ret = DCAM_RTN_SUCCESS;
	int flag = 0;

	if (NULL == queue || NULL == node)
		return -EINVAL;

	spin_lock_irqsave(&queue->lock, flags);
	if (queue->read != queue->write) {
		flag = 1;
		*node = *queue->read;
		queue->read->yaddr = 0;
		queue->read->yaddr_vir = 0;
		queue->read++;
		queue->rcnt++;
		if (queue->read > &queue->node[CAMERA_QUEUE_LENGTH - 1])
			queue->read = &queue->node[0];
	}

	if (!flag)
		ret = -EAGAIN;
	spin_unlock_irqrestore(&queue->lock, flags);

	return ret;
}

static int sprd_img_buf_queue_write(struct camera_img_buf_queue *queue,
				    struct camera_img_buf_addr *buf_addr)
{
	struct camera_img_buf_addr *ori_node;

	if (NULL == queue || NULL == buf_addr)
		return -EINVAL;

	ori_node = queue->write;
	*queue->write++ = *buf_addr;
	queue->wcnt++;
	if (queue->write > &queue->buf_addr[DCAM_FRM_CNT_MAX - 1])
		queue->write = &queue->buf_addr[0];

	if (queue->write == queue->read) {
		queue->write = ori_node;
		pr_info("warning, queue is full\n");
	}

	return 0;
}

static int sprd_img_tx_error(struct camera_frame *frame, void *param)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_dev *dev = (struct camera_dev *)param;
	struct camera_node node;

	if (NULL == param || 0 == atomic_read(&dev->stream_on))
		return -EINVAL;

	memset((void *)&node, 0, sizeof(struct camera_node));
	atomic_set(&dev->run_flag, 1);
	node.irq_flag = IMG_TX_ERR;
	node.irq_type = CAMERA_IRQ_IMG;
	node.irq_property = IRQ_MAX_DONE;
	if (frame != NULL) {
		node.f_type = frame->type;
		node.index = frame->fid;
		node.height = frame->height;
		node.yaddr = frame->yaddr;
		node.uaddr = frame->uaddr;
		node.vaddr = frame->vaddr;
		node.yaddr_vir = frame->yaddr_vir;
		node.uaddr_vir = frame->uaddr_vir;
		node.vaddr_vir = frame->vaddr_vir;
	}
	ret = sprd_img_queue_write(&dev->queue, &node);
	if (ret)
		return ret;

	complete(&dev->irq_com);
	pr_info("tx error\n");
	return ret;
}

int report_isp_err(void *param)
{
	pr_info("reporting isp err\n");
	return sprd_img_tx_error(NULL, param);
}

static int sprd_img_tx_done(struct camera_frame *frame, void *param)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_dev *dev = (struct camera_dev *)param;
	struct camera_path_spec *path;
	struct camera_node node;

	pr_debug("enter from %pS\n", __builtin_return_address(0));

	if (NULL == frame || NULL == param || 0 == atomic_read(&dev->stream_on))
		return -EINVAL;

	atomic_set(&dev->run_flag, 1);

	memset((void *)&node, 0, sizeof(struct camera_node));

	if (frame->irq_type == CAMERA_IRQ_IMG) {
		path = &dev->dcam_cxt.dcam_path[frame->type];
		if (path->status == PATH_IDLE)
			return ret;

		node.irq_flag = IMG_TX_DONE;
		node.irq_type = frame->irq_type;
		node.irq_property = IRQ_MAX_DONE;
		if (frame->type == CAMERA_RAW_PATH)
			node.irq_property = IRQ_RAW_CAP_DONE;
		node.f_type = frame->type;
		node.index = frame->fid;
		node.height = frame->height;
		node.yaddr = frame->yaddr;
		node.uaddr = frame->uaddr;
		node.vaddr = frame->vaddr;
		node.yaddr_vir = frame->yaddr_vir;
		node.uaddr_vir = frame->uaddr_vir;
		node.vaddr_vir = frame->vaddr_vir;
		node.frame_id = frame->frame_id;
		memcpy(node.mfd, frame->pfinfo.mfd, sizeof(uint32_t) * 3);
		pr_debug("send to user: frm_num %d mfd 0x%x,0x%x\n",
			 node.frame_id, node.mfd[0], node.mfd[1]);
	} else if (frame->irq_type == CAMERA_IRQ_STATIS) {
		node.irq_flag = IMG_TX_DONE;
		node.irq_type = frame->irq_type;
		node.irq_property = frame->irq_property;
		node.phy_addr = frame->phy_addr;
		node.vir_addr = frame->vir_addr;
		node.kaddr[0] = frame->kaddr[0];
		node.kaddr[1] = frame->kaddr[1];
		node.addr_offset = frame->addr_offset;
		node.buf_size = frame->buf_size;
		node.frame_id = frame->frame_id;
		memcpy(node.mfd, frame->pfinfo.mfd, sizeof(uint32_t) * 3);
	} else if (frame->irq_type == CAMERA_IRQ_DONE) {
		node.irq_flag = IMG_TX_DONE;
		node.irq_type = frame->irq_type;
		node.irq_property = frame->irq_property;
		node.frame_id = dev->frame_id++;
	} else {
		pr_err("fail to support, irq_type %d\n", frame->irq_type);
		return -EINVAL;
	}

	if (frame->sof_ts.boot_time != 0) {
		node.boot_time = frame->sof_ts.boot_time;
		node.time = frame->sof_ts.time;
	} else {
		if (frame->btu_ts.boot_time == 0)
			gen_frm_timestamp(&frame->btu_ts);
		node.boot_time = frame->btu_ts.boot_time;
		node.time = frame->btu_ts.time;
	}

	ret = sprd_img_queue_write(&dev->queue, &node);
	if (ret) {
		pr_err("fail to write dev->queue\n");
		return ret;
	}

	pr_debug("tm: %lu.%06lu %llu, irq f 0x%x t 0x%x p %u, fn %u ft %d mfd 0x%x %x %x\n",
		 node.time.tv_sec, node.time.tv_usec, node.boot_time,
		 node.irq_flag, node.irq_type, node.irq_property,
		 node.frame_id, node.f_type,
		 node.mfd[0], node.mfd[1], node.mfd[2]);

	if (node.irq_property == IRQ_RAW_CAP_DONE)
		pr_debug("RAW CAP tx done flag %d type %d\n",
			node.irq_flag, node.irq_type);

	complete(&dev->irq_com);

	pr_debug("dev%d [%d] send info to userspace\n",
		 frame->type, dev->idx);

	pr_debug("exit to %pS\n", __builtin_return_address(0));

	return ret;
}


static int dcam_stats_set_next_frm(struct camera_dev *dev)
{
	int ret = DCAM_RTN_SUCCESS;
	struct dcam_statis_module *module = NULL;
	struct isp_pipe_dev *isp_dev = NULL;
	struct isp_statis_module *isp_module = NULL;

	module = &dev->statis_module_info;
	isp_dev = (struct isp_pipe_dev *)dev->isp_dev_handle;
	isp_module = &isp_dev->statis_module_info;

	if (isp_module->statis_valid & ISP_STATIS_VALID_EBD) {
		ret = dcam_ebd_set_next_frm(module, dev->idx, DCAM_EBD_BLOCK);
		if (unlikely(ret))
			pr_err("fail to set ebd next frm %d", ret);
	}

	return ret;
}

static int sprd_img_raw_rt_tx_done(struct camera_frame *frame, void *param)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_dev *dev = (struct camera_dev *)param;
	struct isp_statis_buf node;
	struct dcam_statis_module *module = NULL;
	struct camera_frame frame_info;
	struct isp_statis_frm_queue *statis_heap = NULL;

	memset(&frame_info, 0x00, sizeof(frame_info));
	module = &dev->statis_module_info;
	statis_heap = &module->raw_statis_frm_queue;

	/*dequeue the statis buf from a array*/
	ret = isp_statis_dequeue(statis_heap, &node);
	if (unlikely(ret)) {
		pr_err("fail to dequeue RAW statis buf\n");
		return -1;
	}

	if (node.phy_addr != module->raw_buf_reserved.phy_addr) {
		frame_info.buf_size = node.buf_size;
		memcpy(frame_info.pfinfo.mfd, node.pfinfo.mfd,
			sizeof(uint32_t) * 3);
		frame_info.phy_addr = node.phy_addr;
		frame_info.vir_addr = node.vir_addr;
		frame_info.irq_type = CAMERA_IRQ_STATIS;
		frame_info.irq_property = IRQ_RAW_STATIS;
		frame_info.frame_id = module->raw_statis_cnt;

		sprd_img_tx_done(&frame_info, param);
	} else{
		pr_err("fail to upload raw realtime frame");
	}
	module->raw_statis_cnt++;

	ret = dcam_raw_rt_set_next_frm(module, dev->idx, DCAM_RAW_BLOCK);
	if (unlikely(ret))
		pr_err("fail to set raw realtime next frm %d", ret);
	DCAM_TRACE_INT("raw realtime tx done\n");

	return ret;
}

static int sprd_img_sof_tx_done(struct camera_frame *frame, void *param)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_dev *dev = (struct camera_dev *)param;

	if (frame->flags == ISP_OFF_BUF_NONE) {
		ret = dcam_stats_set_next_frm(dev);
		if (unlikely(ret))
			pr_err("fail to set stats next frm %d", ret);
		ret = sprd_img_tx_done(frame, param);
	} else if ((frame->flags == ISP_OFF_BUF_BIN) ||
		   (frame->flags == ISP_OFF_BUF_FULL)) {
		pr_debug("dcam%d SoF, %d, %s\n", dev->idx, ret,
			 frame->flags == ISP_OFF_BUF_BIN?"bin":"full");
		ret = sprd_isp_get_offline_buffer(dev->isp_dev_handle,
						  frame->flags, frame);
	}

	return ret;
}

static int sprd_img_tx_stop(void *param)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_dev *dev = (struct camera_dev *)param;
	struct camera_node node;

	memset((void *)&node, 0, sizeof(struct camera_node));
	node.irq_flag = IMG_TX_STOP;
	ret = sprd_img_queue_write(&dev->queue, &node);
	if (ret)
		return ret;

	complete(&dev->irq_com);
	DCAM_TRACE_INT("tx stop\n");

	return ret;
}

static int sprd_img_pdaf_tx_done(struct camera_frame *frame, void *param)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_dev *dev = (struct camera_dev *)param;
	struct isp_statis_buf node;
	struct dcam_statis_module *module = NULL;
	struct camera_frame frame_info;
	struct isp_statis_frm_queue *statis_heap = NULL;

	memset(&frame_info, 0x00, sizeof(frame_info));
	module = &dev->statis_module_info;
	statis_heap = &module->pdaf_statis_frm_queue;

	/*dequeue the statis buf from a array*/
	ret = isp_statis_dequeue(statis_heap, &node);
	if (unlikely(ret)) {
		pr_err("fail to dequeue PDAF buf\n");
		return -1;
	}

	module->pdaf_statis_cnt++;
	if (dev->dcam_cxt.flash_skip_fid == module->pdaf_statis_cnt){
		pr_debug("flash: skip this frm, pdaf statis cnt %d\n", module->pdaf_statis_cnt);
		ret = isp_statis_queue_write(&module->pdaf_statis_queue, &node);
		if (ret)
			pr_err("fail to write pdaf buf queue\n");
		goto next_pdaf_frm;
	}

	if (node.phy_addr != module->pdaf_buf_reserved.phy_addr) {
		frame_info.buf_size = node.buf_size;
		memcpy(frame_info.pfinfo.mfd, node.pfinfo.mfd,
		       sizeof(uint32_t) * 3);
		frame_info.phy_addr = node.phy_addr;
		frame_info.vir_addr = node.vir_addr;
		frame_info.irq_type = CAMERA_IRQ_STATIS;
		frame_info.irq_property = IRQ_PDAF_STATIS;
		frame_info.frame_id = module->pdaf_statis_cnt;

		sprd_img_tx_done(&frame_info, param);
	}

next_pdaf_frm:
	ret = dcam_pdaf_set_next_frm(module, dev->idx, DCAM_PDAF_BLOCK);
	if (unlikely(ret))
		pr_err("fail to set padf next frm %d", ret);
	pr_debug("pdaf tx done\n");

	return ret;
}

static int sprd_img_ebd_tx_done(struct camera_frame *frame, void *param)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_dev *dev = (struct camera_dev *)param;
	struct isp_statis_buf node;
	struct dcam_statis_module *module = NULL;
	struct camera_frame frame_info;
	struct isp_statis_frm_queue *statis_heap = NULL;

	memset(&frame_info, 0x00, sizeof(frame_info));
	module = &dev->statis_module_info;
	statis_heap = &module->ebd_statis_frm_queue;

	ret = isp_statis_dequeue(statis_heap, &node);
	if (unlikely(ret)) {
		pr_err("fail to dequeue buf\n");
		return -1;
	}

	if (node.phy_addr != module->ebd_buf_reserved.phy_addr) {
		frame_info.buf_size = node.buf_size;
		memcpy(frame_info.pfinfo.mfd, node.pfinfo.mfd,
		       sizeof(uint32_t) * 3);
		frame_info.phy_addr = node.phy_addr;
		frame_info.kaddr[0] = node.kaddr[0];
		frame_info.kaddr[1] = node.kaddr[1];

		frame_info.vir_addr = node.vir_addr;
		frame_info.irq_type = CAMERA_IRQ_STATIS;
		frame_info.irq_property = IRQ_EBD_STATIS;
		frame_info.frame_id = module->ebd_statis_cnt;

		sprd_img_tx_done(&frame_info, param);
	}

	module->ebd_statis_cnt++;

	return ret;
}

static int sprd_img_full_tx_done(struct camera_frame *frame, void *param)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_dev *dev = (struct camera_dev *)param;

	if (dev->dcam_cxt.need_isp_tool || dev->dcam_cxt.raw_callback) {
		pr_debug("raw callback %d need isp tool %d\n", dev->dcam_cxt.raw_callback,
			dev->dcam_cxt.need_isp_tool);
		sprd_img_tx_done(frame, param);
		return 0;
	}
	if (dev->dcam_cxt.is_raw_rt) {
		sprd_img_raw_rt_tx_done(frame, param);
		return 0;
	}
	if (dev->dcam_cxt.sn_mode == DCAM_CAP_MODE_YUV)
		ret = sprd_isp_set_offline_buffer(dev->isp_dev_handle,
						  ISP_OFF_BUF_BOTH);
	else
		ret = sprd_isp_set_offline_buffer(dev->isp_dev_handle,
						  ISP_OFF_BUF_FULL);
	pr_debug("full path tx done dcam%d\n", dev->idx);

	return ret;
}

/*
 * need log and timestamp to debug
 * depend on ALG
static void sprd_img_afm_timestamp_log(struct camera_frame *frame, void *param)
{
	struct camera_dev *dev = (struct camera_dev *)param;
	struct timespec ts;

	ktime_get_ts(&ts);
	pr_info("sof id %04d time %012ld afm id %04d afm time %012ld\n",
		 dev->bin_frame_id,
		 ts.tv_sec * 1000000L + ts.tv_nsec / NSEC_PER_USEC,
		 frame->frame_id,
		 frame->time);
}
*/

static int sprd_img_handle_afm_stats(void *param)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_dev *dev = (struct camera_dev *)param;
	struct camera_frame *afm_frame;
	struct camera_info *info = NULL;

	if (!dev) {
		pr_err("fail to get dev\n");
		return -1;
	}

	info = &dev->dcam_cxt;
	if (info->is_slow_motion) {
		/* TBD we need get the num 3 from cfg deci */
		if ((dev->bin_frame_id % 3) != 0) {
			dev->afm_delta_frame++;
			return ret;
		}
	}

	ret = sprd_isp_get_afm_frame_info(dev->isp_dev_handle, &afm_frame);
	if (ret) {
		pr_err_ratelimited("fail to get afm frame %d\n", ret);
	} else {
		if (afm_frame->frame_invalid == 0) {
			pr_debug("afm not init\n");
			dev->afm_delta_frame++;
		} else if (afm_frame->frame_invalid == -1) {
			pr_info_ratelimited(
				"afm not ready drop afm once\n");
		} else {
			if ((dev->bin_frame_id - dev->afm_delta_frame) !=
			    afm_frame->frame_id) {
				pr_info_ratelimited(
				    "sof afm don't match %04d %04d\n",
				    dev->bin_frame_id,
				    afm_frame->frame_id);
				dev->afm_delta_frame =
					dev->bin_frame_id -
					afm_frame->frame_id;
			}
			ret = sprd_img_tx_done(afm_frame, param);
			/* sprd_img_afm_timestamp_log(afm_frame, param); */
		}
	}
	return ret;
}

static int sprd_img_binning_tx_done(struct camera_frame *frame, void *param)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_dev *dev = (struct camera_dev *)param;
	struct isp_pipe_dev *isp_dev = NULL;
	struct isp_module *module = NULL;
	struct offline_buf_desc *buf_desc = NULL;
	struct camera_frame out_frame;

	dev->bin_frame_id++;

	if (dev->bin_frame_id == dev->dcam_cxt.flash_skip_fid){
		isp_dev = (struct isp_pipe_dev *)dev->isp_dev_handle;
		module = &isp_dev->module_info;
		buf_desc = isp_offline_sel_buf(&module->off_desc,
					       ISP_OFF_BUF_BIN);
		if (module->off_desc.read_buf_err == 1) {
			module->off_desc.read_buf_err = 0;
			pr_info("bin buf didn't set, no need to skip\n");
			return 0;
		}

		if (isp_frame_dequeue(&buf_desc->frame_queue, &out_frame)) {
			pr_err("fail to dequeue dcam frm queue : bin path\n");
			return 0;
		}

		if (isp_buf_queue_write(&buf_desc->tmp_buf_queue, &out_frame))
			pr_err("fail to write dcam buf queue : bin path\n");
		pr_debug("flash: skip this frame, bin frame id = %d\n", dev->bin_frame_id);

	} else {
		ret = sprd_img_handle_afm_stats(dev);
		if (unlikely(ret))
			pr_err("fail to halde afm stats\n");

		if (dev->dcam_cxt.sn_mode != DCAM_CAP_MODE_YUV)
			ret = sprd_isp_set_offline_buffer(dev->isp_dev_handle,
							  ISP_OFF_BUF_BIN);
	}

	pr_debug("binning path tx done dcam%d\n", dev->idx);

	return ret;
}

static int sprd_img_aem_tx_done(struct camera_frame *frame, void *param)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_dev *dev = (struct camera_dev *)param;
	struct isp_statis_buf node;
	struct dcam_statis_module *module = NULL;
	struct camera_frame frame_info;
	struct isp_statis_frm_queue *statis_heap = NULL;

	memset(&frame_info, 0x00, sizeof(frame_info));
	module = &dev->statis_module_info;
	statis_heap = &module->aem_statis_frm_queue;

	/*dequeue the statis buf from a array*/
	ret = isp_statis_dequeue(statis_heap, &node);
	if (unlikely(ret)) {
		pr_err("fail to dequeue AEM buf\n");
		return -1;
	}

	module->aem_statis_cnt++;
	if (dev->dcam_cxt.flash_skip_fid == module->aem_statis_cnt){
		pr_debug("flash: skip this frame, aem statis cnt %d\n", module->aem_statis_cnt);
		ret = isp_statis_queue_write(&module->aem_statis_queue, &node);
		if (ret)
			pr_err("fail to write aem buf queue\n");
		goto next_aem_frm;
	}

	if (node.phy_addr != module->aem_buf_reserved.phy_addr) {
		frame_info.buf_size = node.buf_size;
		memcpy(frame_info.pfinfo.mfd, node.pfinfo.mfd,
			sizeof(uint32_t) * 3);
		frame_info.phy_addr = node.phy_addr;
		frame_info.vir_addr = node.vir_addr;
		frame_info.irq_type = CAMERA_IRQ_STATIS;
		frame_info.irq_property = IRQ_AEM_STATIS;
		frame_info.frame_id = module->aem_statis_cnt;

		sprd_img_tx_done(&frame_info, param);
	}

next_aem_frm:
	ret = dcam_aem_set_next_frm(module, dev->idx, DCAM_AEM_BLOCK);
	if (unlikely(ret))
		pr_err("fail to set aem next frm %d", ret);
	DCAM_TRACE_INT("aem tx done\n");

	return ret;
}

static int sprd_img_pulse_line_tx_done(struct camera_frame *frame, void *param)
{
	struct camera_dev *dev = (struct camera_dev *)param;
	struct timeval time;
	struct camera_info *info = NULL;

	if (!dev) {
		pr_err("fail to get dev\n");
		return -1;
	}

	info = &dev->dcam_cxt;

	if (info->pulse_info.enable_debug_info) {
		img_get_timestamp(&time);
		info->pulse_info.dac_info.pulse_sec = time.tv_sec;
		info->pulse_info.dac_info.pulse_usec = time.tv_usec;
	}
	info->pulse_info.dac_info.frame_id++;
	queue_work(info->pulse_info.pulse_work_queue,
		   &info->pulse_info.pulse_work);
	return 0;
};

static void sprd_img_set_vcm_pos(struct sprd_img_vcm_param *vcm_info)
{
	struct sensor_i2c_tag i2c_tab;

	pr_debug("i2c set vcm pos %d\n", vcm_info->next_vcm_pos);
	i2c_tab.i2c_data = vcm_info->vcm_i2c_data;
	i2c_tab.i2c_count = vcm_info->vcm_i2c_count;
	i2c_tab.slave_addr = vcm_info->vcm_slave_addr;

	sprd_sensor_write_i2c(&i2c_tab, 0);
}

static int sprd_img_pulse_line_done(struct camera_info *info)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_node node;
	struct camera_dev *dev = NULL;

	dev = container_of(info, struct camera_dev, dcam_cxt);
	memset((void *)&node, 0, sizeof(struct camera_node));

	node.irq_flag = IMG_TX_DONE;
	node.irq_type = CAMERA_IRQ_STATIS;
	node.irq_property = IRQ_DCAM_PULSE;
	node.dac_info = info->pulse_info.dac_info;

	ret = sprd_img_queue_write(&dev->queue, &node);
	if (ret)
		return ret;
	complete(&dev->irq_com);

	return ret;
}

static void sprd_img_handle_pulse_line(struct work_struct *work)
{
	int ret = 0;
	struct timeval time;
	struct camera_pulse_type *pulse =
		container_of(work, struct camera_pulse_type, pulse_work);
	struct camera_info *info =
		container_of(pulse, struct camera_info, pulse_info);
	struct sprd_img_vcm_param vcm_info;

	ret = sprd_img_pulse_read(&info->pulse_info.vcm_queue, &vcm_info);
	if (ret != 0)
		goto exit;

	pr_debug("pos %d last pos %d debug %d\n",
		 vcm_info.next_vcm_pos,
		 info->pulse_info.last_vcm_pos,
		 info->pulse_info.enable_debug_info);

	if (vcm_info.next_vcm_pos == -1) {
		pr_debug("vcm cancel\n");
		goto exit;
	}

	if (vcm_info.next_vcm_pos != info->pulse_info.last_vcm_pos) {
		info->pulse_info.last_vcm_pos = vcm_info.next_vcm_pos;
		if (info->pulse_info.enable_debug_info) {
			img_get_timestamp(&time);
			info->pulse_info.dac_info.vcm_mv_sec =
				time.tv_sec;
			info->pulse_info.dac_info.vcm_mv_usec =
				time.tv_usec;
			info->pulse_info.dac_info.dac =
				vcm_info.next_vcm_pos;
			info->pulse_info.dac_info.pulse_line =
				info->pulse_info.pulse_line;

			sprd_img_pulse_line_done(info);
		}
		sprd_img_set_vcm_pos(&vcm_info);
	}
exit:
	return;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static void sprd_timer_callback(struct timer_list *t)
{
	struct camera_dev *dev = from_timer(dev, t, dcam_timer);
#else
static void sprd_timer_callback(unsigned long data)
{
	struct camera_dev *dev = (struct camera_dev *)data;
#endif
	struct camera_node node = {0};
	int ret = 0;

	if (!dev || 0 == atomic_read(&dev->stream_on)) {
		pr_err("fail to call timer cb\n");
		return;
	}

	if (atomic_read(&dev->run_flag) == 0) {
		pr_err("fail to work DCAM timeout.\n");
		node.irq_flag = IMG_TIMEOUT;
		node.invalid_flag = 0;
		ret = sprd_img_queue_write(&dev->queue, &node);
		if (ret)
			pr_err("fail to work timer cb write queue\n");

		dcam_dbg_reg_trace(dev, CAM_DBG_FORCE_DUMP_REGS);
		complete(&dev->irq_com);
	}
}

static void sprd_init_timer(struct timer_list *dcam_timer, unsigned long data)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	timer_setup(dcam_timer, sprd_timer_callback, 0);
#else
	setup_timer(dcam_timer, sprd_timer_callback, data);
#endif
}

static int sprd_start_timer(struct timer_list *dcam_timer,
			    uint32_t time_val)
{
	int ret = 0;

	DCAM_TRACE("starting timer %ld\n", jiffies);
	ret = mod_timer(dcam_timer, jiffies + msecs_to_jiffies(time_val));
	if (ret)
		pr_err("fail to start in mod_timer %d\n", ret);

	return ret;
}

static int sprd_stop_timer(struct timer_list *dcam_timer)
{
	DCAM_TRACE("stop timer\n");
	del_timer_sync(dcam_timer);

	return 0;
}

static int sprd_init_handle(struct camera_dev *dev)
{
	struct camera_info *info = &dev->dcam_cxt;
	struct camera_path_spec *path;
	uint32_t i = 0;

	if (info == NULL) {
		pr_err("fail to get info null\n");
		return -EINVAL;
	}

	info->flash_skip_fid = 0xffffffff;
	info->set_flash.led0_ctrl = 0;
	info->set_flash.led1_ctrl = 0;
	info->set_flash.led0_status = FLASH_STATUS_MAX;
	info->set_flash.led1_status = FLASH_STATUS_MAX;
	info->set_flash.flash_index = 0;
	info->after_af = 0;
	info->flash_last_status = FLASH_STATUS_MAX;
	info->frame_index = 0;

	for (i = 0; i < CAMERA_MAX_PATH; i++) {
		path = &info->dcam_path[i];
		if (path == NULL) {
			pr_err("fail to init path %d\n", i);
			return -EINVAL;
		}

		path->frm_cnt_act = 0;
		sprd_img_buf_queue_init(&path->buf_queue);
		sprd_img_buf_queue_init(&path->tmp_buf_queue);
		path->status = PATH_IDLE;
	}
	atomic_set(&dev->stream_on, 0);

	pr_info("sprd_img: init handle end!\n");

	return 0;
}

static void sprd_camera_dev_deinit(struct camera_group *group, enum dcam_id idx)
{
	struct camera_dev *dev = NULL;

	if (unlikely((int)idx < 0 || idx >= DCAM_ID_MAX)) {
		pr_err("fail to deinit dcam idx=%d\n", idx);
		return;
	}
	dev = group->dev[idx];

	mutex_lock(&dev->dcam_mutex);
	atomic_set(&dev->stream_on, 0);

	sprd_isp_dev_deinit(dev->isp_dev_handle, (enum isp_id)idx);
	destroy_workqueue(dev->dcam_cxt.pulse_info.pulse_work_queue);
	mutex_destroy(&dev->dcam_cxt.pulse_info.pulse_mutex);
	dcam_stop_flash_thread(dev);
	sprd_stop_timer(&dev->dcam_timer);
	atomic_dec(&dev->users);
	mutex_unlock(&dev->dcam_mutex);

	if (dev->frame)
		vfree(dev->frame);
	mutex_destroy(&dev->dcam_mutex);
	mutex_destroy(&dev->cap_mutex);

	vfree(dev);
	group->dev[idx] = NULL;
	group->dev_inited &= (~(1 << idx));
	pr_info("dev[%d] deinit OK!\n", idx);
}

static int sprd_camera_dev_init(struct camera_group *group, enum dcam_id idx)
{
	int ret = 0;
	struct camera_dev *dev = NULL;

	if (!group) {
		pr_err("fail to get ptr NULL\n");
		return -EFAULT;
	}

	if (group->dev_inited & (1 << (int)idx)) {
		pr_info("sprd_img: dev%d already inited\n", idx);
		return 0;
	}

	pr_info("sprd_img: camera dev %d init start!\n", idx);
	dev = vzalloc(sizeof(*dev));
	if (unlikely(IS_ERR_OR_NULL(dev))) {
		pr_err("fail to alloc mem, ret:%ld!", PTR_ERR(dev));
		return -ENOMEM;
	}

	atomic_set(&dev->run_flag, 1);
	if (unlikely((int)idx < 0 || idx >= DCAM_ID_MAX)) {
		vfree(dev);
		pr_err("fail to deinit dcam idx=%d\n", idx);
		return -EFAULT;
	}
	dev->idx = idx;
	mutex_init(&dev->dcam_mutex);
	mutex_init(&dev->cap_mutex);
	init_completion(&dev->irq_com);
	spin_lock_init(&dev->queue.lock);

	if (unlikely(atomic_inc_return(&dev->users) > 1)) {
		pr_err("fail to deinit dev users isn't 0, %d\n",
		       atomic_read(&dev->users));
		ret = -EBUSY;
		goto users_exit;
	}

	ret = sprd_img_queue_init(&dev->queue);
	if (unlikely(ret != 0)) {
		pr_err("fail to init queue\n");
		ret = -EINVAL;
		goto users_exit;
	}

	ret = sprd_img_pulse_queue_init(
		&dev->dcam_cxt.pulse_info.vcm_queue);
	if (unlikely(ret != 0)) {
		pr_err("fail to init pulse queue\n");
		ret = -EINVAL;
		goto users_exit;
	}

	ret = sprd_init_handle(dev);
	if (unlikely(ret != 0)) {
		pr_err("fail to init queue\n");
		ret = -EINVAL;
		goto users_exit;
	}

	ret = dcam_core_stats_init(dev);
	if (unlikely(ret != 0)) {
		pr_err("fail to init stats\n");
		ret = -EINVAL;
		goto users_exit;
	}

	dev->frame = vzalloc(2 * sizeof(struct camera_frame));
	if (unlikely(IS_ERR_OR_NULL(dev->frame))) {
		pr_err("fail to alloc mem, ret:%ld!", PTR_ERR(dev->frame));
		ret = -ENOMEM;
		goto users_exit;
	}

	sprd_init_timer(&dev->dcam_timer, (unsigned long)dev);

	ret = dcam_create_flash_thread(dev);
	if (unlikely(ret != 0)) {
		pr_err("fail to create flash thread\n");
		ret = -EINVAL;
		goto flash_exit;
	}

	dev->dcam_cxt.pulse_info.last_vcm_pos = -1;
	mutex_init(&dev->dcam_cxt.pulse_info.pulse_mutex);

	dev->dcam_cxt.pulse_info.pulse_work_queue =
		create_singlethread_workqueue("dcam_pulse_line");
	if (!dev->dcam_cxt.pulse_info.pulse_work_queue) {
		pr_err("fail to create pulse line wq\n");
		ret = -EINVAL;
		goto pulse_wq_exit;
	}
	INIT_WORK(&dev->dcam_cxt.pulse_info.pulse_work,
		  sprd_img_handle_pulse_line);

	ret = sprd_isp_dev_init(&dev->isp_dev_handle, (enum isp_id)idx);
	if (unlikely(ret != 0)) {
		pr_err("fail to dev initISP_ID_%d dev init fail\n", idx);
		ret = -EINVAL;
		goto isp_exit;
	}

	group->dev[idx] = dev;
	group->dev_inited |= 1 << idx;
	group->mode_inited = 0;
	((struct isp_pipe_dev *)dev->isp_dev_handle)->cam_grp = group;
	dev->cam_grp = group;

	pr_info("dev[%d] init OK %p!\n", idx, dev);
	return ret;

isp_exit:
	destroy_workqueue(dev->dcam_cxt.pulse_info.pulse_work_queue);
pulse_wq_exit:
	dcam_stop_flash_thread(dev);
flash_exit:
	sprd_stop_timer(&dev->dcam_timer);
	if (dev->frame)
		vfree(dev->frame);
users_exit:
	atomic_dec(&dev->users);
	vfree(dev);

	return ret;
}

static int sprd_img_get_res(struct camera_group *group,
			    struct sprd_img_res *res)
{
	int ret = 0;
	int dcam_id = 0;

	dcam_id = sprd_sensor_find_dcam_id(res->sensor_id);
	/*normal mode*/
	if (dcam_id == DCAM_ID_0) {
		if (0 == ((DCAM_RES_DCAM0_CAP |
			DCAM_RES_DCAM0_PATH)
			& group->dcam_res_used)) {
			pr_info("real sensor:dcam0\n");
			res->flag = DCAM_RES_DCAM0_CAP |
				DCAM_RES_DCAM0_PATH;
			group->dcam_res_used |=
				DCAM_RES_DCAM0_CAP |
				DCAM_RES_DCAM0_PATH;
		} else {
			pr_err("fail to get valid dcam for real sensor!\n");
			ret = -1;
			goto exit;
		}
	} else if (dcam_id == DCAM_ID_1) {
		if (0 == ((DCAM_RES_DCAM1_CAP |
			DCAM_RES_DCAM1_PATH) &
			group->dcam_res_used)) {
			pr_info("front sensor:dcam1\n");
			res->flag = DCAM_RES_DCAM1_CAP |
				DCAM_RES_DCAM1_PATH;
			group->dcam_res_used |=
				DCAM_RES_DCAM1_CAP |
				DCAM_RES_DCAM1_PATH;
		} else {
			pr_err("fail to get valid dcam for front sensor!\n");
			ret = -1;
			goto exit;
		}
	}
exit:
	if (ret)
		res->flag = 0;
	return ret;
}

static int sprd_img_put_res(struct camera_group *group,
			    struct sprd_img_res *res)
{
	int ret = 0;
	int dcam_id = 0;

	dcam_id = sprd_sensor_find_dcam_id(res->sensor_id);

	if (dcam_id == DCAM_ID_0) {
		if ((DCAM_RES_DCAM0_CAP |
			DCAM_RES_DCAM0_PATH) ==
			(res->flag &
			group->dcam_res_used)) {
			pr_info("put dcam0 for rear sensor\n");
			group->dcam_res_used &= ~(DCAM_RES_DCAM0_CAP |
				DCAM_RES_DCAM0_PATH);
		} else if (DCAM_RES_DCAM0_CAP ==
			(res->flag & group->dcam_res_used)) {
			pr_info("put dcam0 top for rear sensor\n");
			group->dcam_res_used &= ~DCAM_RES_DCAM0_CAP;
			goto exit;
		} else {
			pr_err("fail to put dcam0 for rear sensor!\n");
			ret = -1;
			goto exit;
		}
	} else if (dcam_id == DCAM_ID_1) {
		if ((DCAM_RES_DCAM1_CAP |
			DCAM_RES_DCAM1_PATH) ==
			(res->flag & group->dcam_res_used)) {
			pr_info("put dcam1 for front sensor\n");
			group->dcam_res_used &= ~(DCAM_RES_DCAM1_CAP |
				DCAM_RES_DCAM1_PATH);
		} else if (DCAM_RES_DCAM1_CAP ==
			(res->flag &
			group->dcam_res_used)) {
			pr_info("put dcam1 top for front sensor\n");
			group->dcam_res_used &= ~DCAM_RES_DCAM1_CAP;
			goto exit;
		} else {
				pr_err("fail to put dcam1 for front sensor!\n");
				ret = -1;
				goto exit;
		}
	}

exit:
	if (ret)
		res->flag = 0;
	return ret;
}

static int sprd_img_k_open(struct inode *node, struct file *file)
{
	int ret = 0;
	struct camera_file *camerafile = NULL;
	struct miscdevice *md = file->private_data;
	uint32_t count = 0;

	camerafile = vzalloc(sizeof(struct camera_file));
	if (unlikely(IS_ERR_OR_NULL(camerafile))) {
		pr_err("fail to alloc mem, ret:%ld!", PTR_ERR(camerafile));
		return -ENOMEM;
	}

	camerafile->grp = md->this_device->platform_data;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	ret = pm_runtime_get_sync(&camerafile->grp->pdev->dev);
#endif
	file->private_data = (void *)camerafile;
	count = camerafile->grp->dcam_count;

	if (atomic_inc_return(&camerafile->grp->camera_opened) > 1) {
		pr_info("sprd_img: the camera has been inited!\n");
		return 0;
	}

	if (count == 0 || count > DCAM_ID_MAX) {
		pr_err("fail to cfg dts tree config count\n");
		ret = -ENODEV;
		goto OPEN_EXIT2;
	}

	ret = sprd_camera_dev_init(camerafile->grp, DCAM_ID_0);
	if (unlikely(ret != 0)) {
		pr_err("fail to init camera 0\n");
		ret = -EINVAL;
		goto OPEN_EXIT2;
	}

	if (count == DCAM_ID_MAX)
		ret = sprd_camera_dev_init(camerafile->grp, DCAM_ID_1);

	if (unlikely(ret != 0)) {
		pr_err("fail to init camera 1\n");
		sprd_camera_dev_deinit(camerafile->grp, DCAM_ID_0);
		ret = -EINVAL;
		goto OPEN_EXIT2;
	}

	atomic_set(&camerafile->grp->dcam_run_count, 0);
	sprd_isp_drv_init_isp_cnt();
	mutex_init(&camerafile->grp->camera_dualcam_mutex);
	init_completion(&camerafile->grp->dualcam_recovery_com);
	is_dual_cam_dore = 0;
	dual_cam_cap_sta = 0;

	__pm_stay_awake(&camerafile->grp->ws);
	pr_info("sprd_img: open end!\n");

	return 0;

OPEN_EXIT2:
	atomic_dec(&camerafile->grp->camera_opened);
	vfree(camerafile);
	return ret;
}

static int sprd_img_k_release(struct inode *node, struct file *file)
{
	int i = 0, ret = 0;
	struct camera_file *camerafile = NULL;
	struct camera_group *group = NULL;

	camerafile = file->private_data;
	if (!camerafile ||!camerafile->grp){
                pr_err("camerafile or camerafile->grp is NULL!\n");
		goto exit;
        }
	group = camerafile->grp;

	if (group->dev_inited & (1 << 0))
		complete(&((struct camera_dev *)group->dev[0])->irq_com);

	camerafile->private_key = 0;

	pr_info("sprd_img: release start.\n");
	if (atomic_dec_return(&group->camera_opened) == 0) {
		for (i = 0; i < group->dcam_count; i++) {
			if (!(group->dev_inited & (1 << i))) {
				pr_info("sprd_img: dev%d already deinited\n",
					i);
				return -1;
			}
			if (sprd_camera_stream_off(group, i))
				pr_err("fail to stream off, dev %d\n", i);
			sprd_isp_module_dis(group->dev[i]->isp_dev_handle, i);
			sprd_dcam_module_dis(i);
			sprd_camera_dev_deinit(group, i);

			group->dcam_res_used &= ~(DCAM_RES_DCAM0_CAP |
						  DCAM_RES_DCAM0_PATH);
			group->dcam_res_used &= ~(DCAM_RES_DCAM1_CAP |
						  DCAM_RES_DCAM1_PATH);

			group->dev_inited &= ~(1<<i);
			group->mode_inited = 0;
		}

		mutex_destroy(&camerafile->grp->camera_dualcam_mutex);
		__pm_relax(&camerafile->grp->ws);
	}

	vfree(camerafile);
	file->private_data = NULL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	ret = pm_runtime_put_sync(&group->pdev->dev);
#endif

	pr_info("sprd_img: release end!\n");
exit:
	return 0;
}

static int sprd_dcam_cfg_block(struct camera_info *info, enum dcam_id idx,
	enum isp_path_index path_index)
{
	int ret = DCAM_RTN_SUCCESS;
	uint32_t param = 0;

	if (info == NULL) {
		ret = -EINVAL;
		goto exit;
	}

	ret = set_dcam_cap_cfg(idx, DCAM_CAP_INTERFACE, &info->if_mode);
	if (unlikely(ret)) {
		pr_err("fail to cap cfg interface %d", ret);
		goto exit;
	}

	ret = set_dcam_cap_cfg(idx, DCAM_CAP_SENSOR_MODE, &info->sn_mode);
	if (unlikely(ret)) {
		pr_err("fail to cap cfg sensor mode %d", ret);
		goto exit;
	}

	if (info->sn_mode == DCAM_CAP_MODE_RAWRGB) {
		ret = set_dcam_cap_cfg(idx, DCAM_CAP_BAYER_PATTERN,
				       &info->img_ptn);
		if (unlikely(ret)) {
			pr_err("fail to cap cfg bayer pattern %d", ret);
			goto exit;
		}
	} else if (info->sn_mode == DCAM_CAP_MODE_YUV) {
		ret = set_dcam_cap_cfg(idx, DCAM_CAP_YUV_TYPE, &info->img_ptn);
		if (unlikely(ret)) {
			pr_err("fail to cap cfg yuv mode %d", ret);
			goto exit;
		}
	}

	ret = set_dcam_cap_cfg(idx, DCAM_CAP_SYNC_POL, &info->sync_pol);
	if (unlikely(ret)) {
		pr_err("fail to cap cfg sync pol %d", ret);
		goto exit;
	}

	if ((info->dcam_path[CAMERA_PRE_PATH].is_work) ||
	    (info->dcam_path[CAMERA_VID_PATH].is_work) ||
	    (info->dcam_path[CAMERA_CAP_PATH].is_work))
		param = 1;
	else
		param = 0;

	ret = set_dcam_cap_cfg(idx, DCAM_CAP_DATA_BITS, &info->data_bits);
	if (unlikely(ret)) {
		pr_err("fail to cap cfg data bits %d", ret);
		goto exit;
	}

	if (info->sn_mode == DCAM_CAP_MODE_RAWRGB &&
	    info->if_mode == DCAM_CAP_IF_CSI2) {
		ret = set_dcam_cap_cfg(idx, DCAM_CAP_DATA_PACKET,
					&info->is_loose);
		if (unlikely(ret)) {
			pr_err("fail to cap cfg data packet %d", ret);
			goto exit;
		}
	}

	ret = set_dcam_cap_cfg(idx, DCAM_CAP_FRM_DECI, &info->frm_deci);
	if (unlikely(ret)) {
		pr_err("fail to cap cfg deci %d", ret);
		goto exit;
	}

	ret = set_dcam_cap_cfg(idx, DCAM_CAP_INPUT_RECT, &info->cap_in_rect);
	if (unlikely(ret)) {
		pr_err("fail to cap cfg input rect %d", ret);
		goto exit;
	}

	ret = set_dcam_cap_cfg(idx, DCAM_CAP_FRM_COUNT_CLR, NULL);
	if (unlikely(ret)) {
		pr_err("fail to cap cfg cout clr %d", ret);
		goto exit;
	}

	ret = set_dcam_cap_cfg(idx, DCAM_CAP_PRE_SKIP_CNT, &info->skip_number);
	if (unlikely(ret)) {
		pr_err("fail to cap cfg pre skip cnt %d", ret);
		goto exit;
	}

	ret = set_dcam_cap_cfg(idx, DCAM_CAP_SAMPLE_MODE, &info->capture_mode);
	if (unlikely(ret)) {
		pr_err("fail to cap cfg sample mode %d", ret);
		goto exit;
	}

	ret = set_dcam_cap_cfg(idx, DCAM_CAP_IMAGE_XY_DECI, &info->img_deci);
	if (unlikely(ret)) {
		pr_err("fail to cap cfg img xy dect %d", ret);
		goto exit;
	}

	ret = set_dcam_cap_cfg(idx, DCAM_CAP_FRM_PULSE_LINE,
			       &info->pulse_info.pulse_line);
	if (unlikely(ret)) {
		pr_err("fail to cap cfg pulse line %d", ret);
		goto exit;
	}
exit:

	return ret;
}


static int sprd_dcam_block_reg_isr(struct camera_dev *param)
{
	sprd_dcam_reg_isr(param->idx, DCAM_OVF,
			  sprd_img_tx_error, param);
	sprd_dcam_reg_isr(param->idx, DCAM_AEM_HOLD_OVF,
			  sprd_img_tx_error, param);
	sprd_dcam_reg_isr(param->idx, DCAM_CAP_LINE_ERR,
			  sprd_img_tx_error, param);
	sprd_dcam_reg_isr(param->idx, DCAM_CAP_FRM_ERR,
			  sprd_img_tx_error, param);
	sprd_dcam_reg_isr(param->idx, DCAM_FULL_PATH_TX_DONE,
			  sprd_img_full_tx_done, param);
	sprd_dcam_reg_isr(param->idx, DCAM_BIN_PATH_TX_DONE,
			  sprd_img_binning_tx_done, param);
	sprd_dcam_reg_isr(param->idx, DCAM_PDAF_PATH_TX_DONE,
			  sprd_img_pdaf_tx_done, param);
	sprd_dcam_reg_isr(param->idx, DCAM_VCH2_PATH_TX_DONE,
			  sprd_img_pdaf_tx_done, param);
	sprd_dcam_reg_isr(param->idx, DCAM_VCH3_PATH_TX_DONE,
			  sprd_img_ebd_tx_done, param);
	sprd_dcam_reg_isr(param->idx, DCAM_AEM_PATH_TX_DONE,
			  sprd_img_aem_tx_done, param);
	sprd_dcam_reg_isr(param->idx, DCAM_CAP_SOF,
			  sprd_img_sof_tx_done, param);
	sprd_dcam_reg_isr(param->idx, DCAM_ISP_ENABLE_PULSE,
			  sprd_img_pulse_line_tx_done, param);

	return 0;
}

static int sprd_dcam_block_unreg_isr(struct camera_dev *param)
{
	sprd_dcam_reg_isr(param->idx, DCAM_OVF, NULL, param);
	sprd_dcam_reg_isr(param->idx, DCAM_AEM_HOLD_OVF, NULL, param);
	sprd_dcam_reg_isr(param->idx, DCAM_CAP_LINE_ERR, NULL, param);
	sprd_dcam_reg_isr(param->idx, DCAM_CAP_FRM_ERR, NULL, param);
	sprd_dcam_reg_isr(param->idx, DCAM_FULL_PATH_TX_DONE, NULL, param);
	sprd_dcam_reg_isr(param->idx, DCAM_BIN_PATH_TX_DONE, NULL, param);
	sprd_dcam_reg_isr(param->idx, DCAM_PDAF_PATH_TX_DONE, NULL, param);
	sprd_dcam_reg_isr(param->idx, DCAM_VCH2_PATH_TX_DONE, NULL, param);
	sprd_dcam_reg_isr(param->idx, DCAM_VCH3_PATH_TX_DONE, NULL, param);
	sprd_dcam_reg_isr(param->idx, DCAM_AEM_PATH_TX_DONE, NULL, param);
	sprd_dcam_reg_isr(param->idx, DCAM_CAP_SOF, NULL, param);
	sprd_dcam_reg_isr(param->idx, DCAM_ISP_ENABLE_PULSE, NULL, param);

	return 0;
}


static int sprd_dcam_cfg(struct camera_dev *dev)
{
	int ret = 0;
	struct camera_info *info = NULL;
	struct camera_path_spec *raw_path = NULL;
	struct camera_path_spec *pre_path = NULL;
	struct camera_path_spec *vid_path = NULL;
	struct camera_path_spec *cap_path = NULL;
	struct camera_path_spec *pdaf_path = NULL;
	struct camera_path_spec *aem_path = NULL;
	struct camera_path_spec *ebd_path = NULL;
	enum dcam_id idx = 0;

	if (!dev) {
		pr_err("fail to get dev NULL\n");
		return -EFAULT;
	}

	mutex_lock(&dev->dcam_mutex);
	info = &dev->dcam_cxt;
	idx = dev->idx;
	raw_path = &info->dcam_path[CAMERA_RAW_PATH];
	pre_path = &(info->dcam_path[CAMERA_PRE_PATH]);
	vid_path = &(info->dcam_path[CAMERA_VID_PATH]);
	cap_path = &(info->dcam_path[CAMERA_CAP_PATH]);
	pdaf_path = &info->dcam_path[CAMERA_PDAF_PATH];
	aem_path = &info->dcam_path[CAMERA_AEM_PATH];
	ebd_path = &info->dcam_path[CAMERA_EBD_PATH];
	if (info->sn_mode == DCAM_CAP_MODE_YUV) {
		if (pre_path->is_work && !cap_path->is_work) {
			pr_debug("this is video mode\n");
			memcpy(cap_path, pre_path, sizeof(*pre_path));
		}
	}

	ret = sprd_dcam_module_init(idx, (void *)dev);
	if (unlikely(ret)) {
		pr_err("fail to init dcam module\n");
		goto exit;
	}

	ret = sprd_dcam_block_reg_isr(dev);
	if (unlikely(ret)) {
		pr_err("fail to register dcam isr\n");
		goto exit;
	}

	/* config cap sub-module */
	/* TODO: dcam path should be configured as what scenarios need */
	ret = sprd_dcam_cfg_block(info, idx, ISP_PATH_IDX_PRE);
	if (unlikely(ret)) {
		pr_err("fail to config cap\n");
		goto exit;
	}

	/* config dcam_if raw path */
	if (raw_path->is_work) {
		if (info->raw_callback) {
			raw_path->is_work = RAW_CALLBACK;
			pre_path->is_work = 0;
			cap_path->is_work = 0;
			vid_path->is_work = 0;
			pdaf_path->is_work = 0;
			ebd_path->is_work = 0;
			pr_debug("it is raw callback\n");
		} else {
			/* set raw path mode, Binn or full raw */
			raw_path->is_work = info->need_isp_tool;
			pr_debug("full for raw cap\n");
		}

		/* set size, for in, out */
		raw_path->in_size = info->cap_out_size;
		ret = sprd_dcam_cfg_raw_path(raw_path, idx);
		if (unlikely(ret))
			pr_err("fail to config dcam raw path cap\n");
		goto exit;
	}

	/* config dcam_if prev path or vid path:bin path */
	if (pre_path->is_work)
		pre_path->in_size = info->cap_out_size;

	if (vid_path->is_work)
		vid_path->in_size = info->cap_out_size;

	if (pre_path->is_work && vid_path->is_work) {
		if (vid_path->isp_out_size.w > pre_path->isp_out_size.w ||
			vid_path->isp_out_size.h > pre_path->isp_out_size.h) {
			ret = sprd_dcam_cfg_bin_path(vid_path, idx);
			pre_path->out_size = vid_path->out_size;
			pre_path->bin_ratio = vid_path->bin_ratio;
			update_trim_size(idx, &(pre_path->in_rect),
				CAMERA_VID_PATH);
			pr_info("bin for vid(&pre)\n");
		} else {
			ret = sprd_dcam_cfg_bin_path(pre_path, idx);
			/* The same size for input to isp */
			vid_path->out_size = pre_path->out_size;
			vid_path->bin_ratio = pre_path->bin_ratio;
			update_trim_size(idx, &(vid_path->in_rect),
				CAMERA_PRE_PATH);
			pr_info("bin for pre(&vid)\n");
		}
	} else if (pre_path->is_work) {
		ret = sprd_dcam_cfg_bin_path(pre_path, idx);
		pr_info("bin for pre\n");
	} else if (vid_path->is_work) {
		ret = sprd_dcam_cfg_bin_path(vid_path, idx);
		pr_info("bin for vid\n");
	}
	if ((pre_path->is_work || vid_path->is_work) && ret) {
		pr_err("fail to config dcam binning path\r");
		goto exit;
	}

	if (cap_path->is_work) {
		pr_info("full for cap\n");
		cap_path->in_size = info->cap_out_size;
		ret = sprd_dcam_cfg_full_path(cap_path, idx);
		if (unlikely(ret)) {
			pr_err("fail to config dcam full path\n");
			goto exit;
		}
	}

	if (pdaf_path->is_work) {
		ret = sprd_dcam_cfg_pdaf_path(pdaf_path, idx);
		if (unlikely(ret)) {
			pr_err("fail to config path pdaf cap");
			goto exit;
		}
	}

	if (info->sn_mode == DCAM_CAP_MODE_RAWRGB) {
		ret = sprd_dcam_cfg_aem_path(aem_path, idx);
		if (unlikely(ret)) {
			pr_err("fail to config path aem cap");
			goto exit;
		}
	}

	if (ebd_path->is_work) {
		ret = sprd_dcam_cfg_ebd_path(ebd_path, idx);
		if (unlikely(ret)) {
			pr_err("fail to config path");
			goto exit;
		}
	}

	pr_debug("end dcam cfg.\n");
exit:
	mutex_unlock(&dev->dcam_mutex);

	return ret;
}

static int sprd_isp_path_mode_cfg(struct camera_info *info)
{
	int ret = 0;
	struct camera_path_spec *path_pre = NULL;
	struct camera_path_spec *path_vid = NULL;
	struct camera_path_spec *path_cap = NULL;

	if (!info) {
		ret = -EFAULT;
		pr_err("fail to get info NULL\n");
		goto exit;
	}

	path_pre = &info->dcam_path[CAMERA_PRE_PATH];
	path_vid = &info->dcam_path[CAMERA_VID_PATH];
	path_cap = &info->dcam_path[CAMERA_CAP_PATH];

#if 0
	if (info->is_slow_motion) {
		path_pre->path_mode = ISP_PRE_ONLINE;
		path_vid->path_mode = ISP_VID_ONLINE;
		path_cap->path_mode = ISP_CAP_OFFLINE;
	} else {
		tempw = path_pre->out_size.w * ISP_ONLINE_ZFACTOR_MAX;
		if (path_pre->in_size.w >= tempw) {
			path_pre->path_mode = ISP_PRE_ONLINE;
			path_vid->path_mode = ISP_VID_ONLINE;
			tempw = path_vid->out_size.w * ISP_ONLINE_ZFACTOR_MAX;
			if (path_pre->in_size.w < tempw)
				path_vid->path_mode = ISP_VID_ONLINE_CPP;
		} else {
			path_pre->path_mode = ISP_PRE_OFFLINE;
			path_vid->path_mode = ISP_VID_OFFLINE;
		}
		path_pre->path_mode = ISP_PRE_ONLINE;
		path_vid->path_mode = ISP_VID_ONLINE;
		path_cap->path_mode = ISP_CAP_OFFLINE;
	}
#endif

	path_pre->path_mode = ISP_PRE_ONLINE;
	path_vid->path_mode = ISP_VID_ONLINE;
	path_cap->path_mode = ISP_CAP_OFFLINE;

	pr_info("prv path_mode: 0x%x, cap path_mode 0x%x, vid path mode 0x%x\n",
		path_pre->path_mode, path_cap->path_mode, path_vid->path_mode);
exit:
	return ret;
}

static int sprd_img_isp_reg_isr(struct camera_dev *dev)
{
	enum isp_id iid = ISP_ID_0;

	if (!dev) {
		pr_err("fail to get dev NULL\n");
		return -EFAULT;
	}

	if (dev->idx == DCAM_ID_0)
		iid = ISP_ID_0;
	else if (dev->idx == DCAM_ID_1)
		iid = ISP_ID_1;
	else {
		pr_err("fail to support dev idx %d", dev->idx);
		return -EFAULT;
	}

	sprd_isp_reg_isr(iid, ISP_PATH_PRE_DONE, sprd_img_tx_done, dev);
	sprd_isp_reg_isr(iid, ISP_PATH_VID_DONE, sprd_img_tx_done, dev);
	sprd_isp_reg_isr(iid, ISP_PATH_CAP_DONE, sprd_img_tx_done, dev);
	sprd_isp_reg_isr(iid, ISP_AFL_DONE, sprd_img_tx_done, dev);
	sprd_isp_reg_isr(iid, ISP_AFM_DONE, sprd_img_tx_done, dev);
	sprd_isp_reg_isr(iid, ISP_BINNING_DONE, sprd_img_tx_done, dev);
	sprd_isp_reg_isr(iid, ISP_HIST_DONE, sprd_img_tx_done, dev);
	sprd_isp_reg_isr(iid, ISP_HIST2_DONE, sprd_img_tx_done, dev);

	return 0;
}

static int sprd_img_isp_unreg_isr(struct camera_dev *dev)
{
	enum isp_id iid = ISP_ID_0;

	if (!dev) {
		pr_err("fail to get dev NULL\n");
		return -EFAULT;
	}

	if (dev->idx == DCAM_ID_0)
		iid = ISP_ID_0;
	else if (dev->idx == DCAM_ID_1)
		iid = ISP_ID_1;
	else {
		pr_err("fail to support dev idx %d", dev->idx);
		return -EFAULT;
	}
	sprd_isp_reg_isr(iid, ISP_PATH_PRE_DONE, NULL, dev);
	sprd_isp_reg_isr(iid, ISP_PATH_VID_DONE, NULL, dev);
	sprd_isp_reg_isr(iid, ISP_PATH_CAP_DONE, NULL, dev);
	sprd_isp_reg_isr(iid, ISP_AFL_DONE, NULL, dev);
	sprd_isp_reg_isr(iid, ISP_AFM_DONE, NULL, dev);
	sprd_isp_reg_isr(iid, ISP_BINNING_DONE, NULL, dev);
	sprd_isp_reg_isr(iid, ISP_HIST_DONE, NULL, dev);
	sprd_isp_reg_isr(iid, ISP_HIST2_DONE, NULL, dev);

	return 0;
}

static int sprd_isp_3dnr_path_cfg(struct isp_pipe_dev *isp_dev,
				  struct camera_size *small_size)
{
	struct isp_path_desc *cap = &isp_dev->module_info.isp_path[ISP_SCL_CAP];
	struct isp_path_desc *vid = &isp_dev->module_info.isp_path[ISP_SCL_VID];
	uint32_t ret = 0;

	ret = set_isp_path_cfg(isp_dev,
		ISP_PATH_IDX_VID,
		ISP_PATH_OUTPUT_SIZE,
		small_size);
	if (unlikely(ret))
		return ret;

	vid->src = cap->src;
	vid->trim0_info = cap->trim0_info;
	vid->data_endian = cap->data_endian;
	vid->output_format = cap->output_format;
	vid->input_format = cap->input_format;
	vid->in_size = cap->in_size;
	vid->dst = vid->out_size;
	vid->trim1_info.start_x = 0;
	vid->trim1_info.start_y = 0;
	vid->trim1_info.size_x = vid->out_size.w;
	vid->trim1_info.size_y = vid->out_size.h;

	isp_dev->is_3dnr_path_cfg = 1;

	pr_info("isp 3dnr path configured, input_size w %d h %d, out_size w %d h %d\n",
		vid->in_size.w, vid->in_size.h,
		vid->out_size.w, vid->out_size.h);
	return ret;
}

static int sprd_isp_path_cfg(struct camera_dev *dev)
{
	int ret = 0;
	int path_not_work = 0;
	struct camera_info *info = NULL;
	struct isp_path_info path_info;
	struct isp_pipe_dev *isp_dev = NULL;
	struct camera_path_spec *path_pre = NULL;
	struct camera_path_spec *path_vid = NULL;
	struct camera_path_spec *path_cap = NULL;
	enum dcam_id idx = 0;

	if (!dev) {
		ret = -EFAULT;
		pr_err("fail to get dev NULL\n");
		goto exit;
	}

	mutex_lock(&dev->dcam_mutex);
	info = &dev->dcam_cxt;
	idx = dev->idx;
	isp_dev = (struct isp_pipe_dev *)dev->isp_dev_handle;
	path_pre = &info->dcam_path[CAMERA_PRE_PATH];
	path_vid = &info->dcam_path[CAMERA_VID_PATH];
	path_cap = &info->dcam_path[CAMERA_CAP_PATH];
	path_info.is_slow_motion = info->is_slow_motion;
	if (dev->dcam_cxt.sn_mode == DCAM_CAP_MODE_YUV)
		isp_dev->is_yuv_sn = true;
	else
		isp_dev->is_yuv_sn = false;

	if (info->is_loose)
		path_cap->is_loose = 1;
	/* need config pre&vid onlie/offline/use cpp or not here*/
	ret = sprd_isp_path_mode_cfg(info);
	if (unlikely(ret)) {
		pr_err("fail to config isp path mode\n");
		mutex_unlock(&dev->dcam_mutex);
		goto exit;
	}

	ret = sprd_img_isp_reg_isr(dev);
	if (unlikely(ret)) {
		pr_err("fail to register isp isr\n");
		mutex_unlock(&dev->dcam_mutex);
		goto exit;
	}

	ret = set_isp_path_cfg(dev->isp_dev_handle, ISP_PATH_IDX_PRE,
			       ISP_PATH_ZOOM_MODE, &info->is_smooth_zoom);
	if (unlikely(ret)) {
		pr_err("fail to config isp path zoom mode\n");
		mutex_unlock(&dev->dcam_mutex);
		goto exit;
	}

	do {
		/* config isp pre path */
		if (dev->use_path && path_pre->is_work) {
			ret = sprd_isp_path_cfg_block(path_pre,
						      dev->isp_dev_handle,
						      ISP_PATH_IDX_PRE);
			if (unlikely(ret)) {
				pr_err("fail to config path_pre");
				break;
			}

			set_isp_path_cfg(dev->isp_dev_handle, ISP_PATH_IDX_PRE,
					 ISP_PATH_SN_MAX_SIZE,
					 &path_pre->out_size);
			set_isp_path_cfg(dev->isp_dev_handle, ISP_PATH_IDX_PRE,
					 ISP_PATH_UFRAME_SYNC,
					 &info->scene_mode);

			path_pre->status = PATH_RUN;
		} else {
			ret = set_isp_path_cfg(dev->isp_dev_handle,
					       ISP_PATH_IDX_PRE,
					       ISP_PATH_ENABLE,
					       &path_not_work);
			if (unlikely(ret)) {
				pr_err("fail to config isp path pre\n");
				break;
			}
		}

		/* config isp vid path*/
		if (dev->use_path && path_vid->is_work) {
			pr_debug("[Vid En] use_path %d, is_work %d\n",
				 dev->use_path, path_vid->is_work);
			ret = sprd_isp_path_cfg_block(path_vid,
						      dev->isp_dev_handle,
						      ISP_PATH_IDX_VID);
			if (unlikely(ret)) {
				pr_err("fail to config path_vid");
				break;
			}

			set_isp_path_cfg(dev->isp_dev_handle, ISP_PATH_IDX_VID,
					ISP_PATH_SN_MAX_SIZE,
					&path_vid->out_size);
			set_isp_path_cfg(dev->isp_dev_handle, ISP_PATH_IDX_VID,
					 ISP_PATH_UFRAME_SYNC,
					 &info->scene_mode);
			ret = sprd_isp_slw_flags_init(dev->isp_dev_handle,
						      &path_info);
			if (unlikely(ret)) {
				pr_err("fail to config slow motion");
				break;
			}
			path_vid->status = PATH_RUN;
		} else {
			pr_debug("[Vid Dis] use_path %d, is_work %d\n",
				 dev->use_path, path_vid->is_work);
			ret = set_isp_path_cfg(dev->isp_dev_handle,
					       ISP_PATH_IDX_VID,
					       ISP_PATH_ENABLE,
					       &path_not_work);
			if (unlikely(ret)) {
				pr_err("fail to config isp path pre\n");
				break;
			}
		}

		/* config isp cap path*/
		if (dev->use_path && path_cap->is_work) {
			ret = sprd_isp_path_cfg_block(path_cap,
						      dev->isp_dev_handle,
						      ISP_PATH_IDX_CAP);
			if (unlikely(ret)) {
				pr_err("fail to config path_cap\n");
				break;
			}

			ret = set_isp_path_cfg(dev->isp_dev_handle,
					       ISP_PATH_IDX_CAP,
					       ISP_PATH_SN_MAX_SIZE,
					       &path_cap->out_size);

			set_isp_path_cfg(dev->isp_dev_handle, ISP_PATH_IDX_CAP,
					 ISP_PATH_UFRAME_SYNC,
					 &info->scene_mode);

			path_cap->status = PATH_RUN;
			if (info->is_3dnr) {
				ret = sprd_isp_3dnr_path_cfg(
					dev->isp_dev_handle, &info->small_size);
				if (unlikely(ret)) {
					pr_err("fail to config 3dnr path\n");
					break;
				}
			}
		} else {
			ret = set_isp_path_cfg(dev->isp_dev_handle,
					       ISP_PATH_IDX_CAP,
					       ISP_PATH_ENABLE,
					       &path_not_work);
			if (unlikely(ret)) {
				pr_err("fail to config isp path pre\n");
				break;
			}
		}

	} while (0);

	mutex_unlock(&dev->dcam_mutex);
	pr_debug("end isp path cfg\n");
exit:
	return ret;
}

static int sprd_camera_stream_on(struct camera_file *camerafile)
{
	int ret = 0;
	struct camera_dev *dev = NULL;
	struct camera_group *group = NULL;
	enum dcam_id idx = DCAM_ID_0;

	if (!camerafile) {
		ret = -EFAULT;
		pr_err("fail to get camerafile NULL\n");
		goto exit;
	}

	group = camerafile->grp;
	idx = camerafile->idx;
	pr_info("dcam%d stream on +\n", idx);

	if (unlikely((int)idx < 0 || idx >= DCAM_ID_MAX)) {
		pr_err("fail to get dcam idx=%d\n", idx);
		ret = -EFAULT;
		goto exit;
	}

	if (group->dev_inited & (1 << (int)idx)) {
		dev = group->dev[idx];
		if (!dev) {
			ret = -EFAULT;
			pr_err("fail to get dev[%d] NULL\n", idx);
			goto exit;
		}
	} else {
		ret = -EFAULT;
		pr_err("fail to init, dev[%d] hasn't been inited!\n", idx);
		goto exit;
	}

	ret = sprd_img_queue_init(&dev->queue);
	if (unlikely(ret != 0)) {
		pr_err("fail to init queue\n");
		goto exit;
	}
	ret = sprd_img_pulse_queue_init(
		&dev->dcam_cxt.pulse_info.vcm_queue);
	if (unlikely(ret != 0)) {
		pr_err("fail to init pulse queue\n");
		goto exit;
	}

	log_frame_init();
	ret = sprd_dcam_cfg(dev);
	if (unlikely(ret)) {
		pr_err("fail to config dcam param\n");
		goto exit;
	}

	ret = sprd_isp_path_cfg(dev);
	if (unlikely(ret)) {
		pr_err("fail to config isp path\n");
		goto exit;
	}

	dev->frame_skipped = 0;

	if ((dev->dcam_cxt.set_flash.led0_ctrl &&
	     dev->dcam_cxt.set_flash.led0_status == FLASH_HIGH_LIGHT) ||
	    (dev->dcam_cxt.set_flash.led1_ctrl &&
	     dev->dcam_cxt.set_flash.led1_status == FLASH_HIGH_LIGHT)) {
		if (dev->dcam_cxt.skip_number == 0)
			sprd_img_start_flash(NULL, dev);
	}

	ret = sprd_isp_start(dev->isp_dev_handle, dev->frame);
	if (unlikely(ret)) {
		pr_err("fail to start isp path\n");
		goto exit;
	}

	mutex_lock(&dev->dcam_mutex);
	ret = sprd_dcam_start(idx, dev->frame, &dev->statis_module_info);
	if (unlikely(ret)) {
		pr_err("fail to start dcam\n");
		ret = sprd_img_isp_unreg_isr(dev);
		mutex_unlock(&dev->dcam_mutex);
		goto exit;
	} else {
		atomic_set(&dev->run_flag, 0);
		sprd_start_timer(&dev->dcam_timer, DCAM_TIMEOUT);
	}

	isp_dbg_dump_input_init(dev->isp_dev_handle);

	atomic_set(&dev->stream_on, 1);
	atomic_inc(&group->dcam_run_count);
	sprd_dcam_enable_int(idx);
	dev->is_simulation_mode = 0;
	mutex_unlock(&dev->dcam_mutex);

	pr_debug("dcam%d stream on end\n", idx);
exit:
	return ret;
}

int sprd_camera_stream_off(struct camera_group *group,
				  enum dcam_id idx)
{
	int ret = 0;
	struct camera_dev *dev = NULL;
	struct camera_path_spec *path_0 = NULL;
	struct isp_pipe_dev *isp_dev = NULL;

	pr_info("dcam%d stream off +\n", idx);
	if (unlikely((int)idx < 0 || idx >= DCAM_ID_MAX)) {
		pr_err("fail to get dcam idx=%d\n", idx);
		ret = -EFAULT;
		goto exit;
	}
	if (group->dev_inited & (1 << (int)idx)) {
		dev = group->dev[idx];
		if (!dev) {
			ret = -EFAULT;
			pr_err("fail to get dev[%d] NULL\n", idx);
			goto exit;
		}
	} else {
		ret = -EFAULT;
		pr_info("dev[%d] hasn't inited or has deinited!\n", idx);
		goto exit;
	}

	isp_dbg_dump_input_deinit(dev->isp_dev_handle);

	mutex_lock(&dev->dcam_mutex);
	if (is_dual_cam)
		mutex_lock(&group->camera_dualcam_mutex);

	path_0 = &dev->dcam_cxt.dcam_path[CAMERA_RAW_PATH];

	if (unlikely(dev->is_simulation_mode)) {
		pr_info("simulation mode\n");
		sprd_img_queue_init(&dev->queue);
		sprd_img_pulse_queue_init(&dev->dcam_cxt.pulse_info.vcm_queue);

		ret = sprd_isp_external_unmap(dev->isp_dev_handle);
		if (unlikely(ret)) {
			pr_info("fail to unmap isp buf");
			goto simul_exit;
		}
		ret = sprd_isp_stop(dev->isp_dev_handle, 0);
		if (unlikely(ret)) {
			pr_err("fail to stop isp\n");
			goto simul_exit;
		}
		ret = sprd_img_isp_unreg_isr(dev);
		if (unlikely(ret)) {
			pr_err("fail to unregister isp isr\n");
			goto simul_exit;
		}

		atomic_set(&dev->stream_on, 0);
	}

	if (unlikely(atomic_read(&dev->stream_on) == 0)) {
		pr_info("stream not on, idx: %d\n", idx);
		ret = sprd_img_local_deinit(dev);
		if (unlikely(ret))
			pr_err("fail to deinit g local\n");
		if (is_dual_cam)
			mutex_unlock(&group->camera_dualcam_mutex);
		mutex_unlock(&dev->dcam_mutex);
		goto exit;
	}
	do {
		ret = sprd_stop_timer(&dev->dcam_timer);
		if (unlikely(ret)) {
			pr_err("fail to stop timer\n");
			break;
		}
		ret = sprd_dcam_stop(idx, 0, sprd_isp_external_unmap,
				     dev->isp_dev_handle);
		if (unlikely(ret)) {
			pr_err("fail to stop dcam\n");
			break;
		}
		ret = sprd_dcam_block_unreg_isr(dev);
		if (unlikely(ret)) {
			pr_err("fail to unregister isr\n");
			break;
		}

		/* check if stop dcam first, then isp, or not? */
		ret = sprd_isp_stop(dev->isp_dev_handle, 0);
		if (unlikely(ret)) {
			pr_err("fail to stop isp\n");
			break;
		}

		ret = sprd_img_isp_unreg_isr(dev);
		if (unlikely(ret)) {
			pr_err("fail to unregister isp isr\n");
			break;
		}

		isp_dev = dev->isp_dev_handle;

		isp_offline_put_buf(&isp_dev->module_info.off_desc,
				    ISP_OFF_BUF_BIN);
		isp_offline_init_buf(&isp_dev->module_info.off_desc,
				     ISP_OFF_BUF_BIN, false);
		isp_offline_put_buf(&isp_dev->module_info.off_desc,
				    ISP_OFF_BUF_FULL);
		isp_offline_init_buf(&isp_dev->module_info.off_desc,
				     ISP_OFF_BUF_FULL, false);

		if (path_0->is_work) {
			path_0->status = PATH_IDLE;
			path_0->is_work = 0;
		}

		if (unlikely(dev->cap_flag != DCAM_CAPTURE_STOP)) {
			pr_warn("fail to get cap_flag: %d\n", dev->cap_flag);
			dev->cap_flag = DCAM_CAPTURE_STOP;
		}

		atomic_set(&dev->stream_on, 0);

#if 0
		if (dev->init_inptr.statis_valid) {
			if (group->cam_ion_client[idx]) {
				sprd_ion_client_put(
					group->cam_ion_client[idx]);
				group->cam_ion_client[idx] = NULL;
			}
		}
#endif
		ret = sprd_dcam_module_deinit(idx);
		if (unlikely(ret)) {
			pr_err("fail to deinit dcam module\n");
			break;
		}

		ret = sprd_img_local_deinit(dev);
		if (unlikely(ret)) {
			pr_err("fail to local deinit\n");
			break;
		}

		if (atomic_dec_return(&group->dcam_run_count) == 0)
			sprd_iommu_restore(&s_dcam_pdev->dev);
	} while (0);

simul_exit:
	if (is_dual_cam)
		mutex_unlock(&group->camera_dualcam_mutex);
	mutex_unlock(&dev->dcam_mutex);

	pr_info("dcam%d stream off end\n", idx);
exit:
	return ret;
}

static int sprd_img_update_video(struct camera_dev *dev, uint32_t channel_id)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_path_spec *path = NULL;
	enum isp_path_index path_index;

	mutex_lock(&dev->dcam_mutex);
	path = &dev->dcam_cxt.dcam_path[channel_id];
	path_index = sprd_img_get_path_index(channel_id);
	DCAM_TRACE("update video channel %d, %d\n", channel_id, path_index);
	update_trim_size(dev->idx, &(path->in_rect), channel_id);
	DCAM_TRACE("isp in_size{%d %d}, trim{%d %d %d %d}\n",
		   path->out_size.w, path->out_size.h, path->in_rect.x,
		   path->in_rect.y, path->in_rect.w, path->in_rect.h);
	DCAM_TRACE("isp out_size{%d %d}\n",
		   path->isp_out_size.w, path->isp_out_size.h);
	ret = sprd_isp_update_zoom_param(dev->isp_dev_handle,
					 path_index,
					 &path->out_size,
					 &path->in_rect,
					 &path->isp_out_size);

	mutex_unlock(&dev->dcam_mutex);
	DCAM_TRACE("update video ret %d\n", ret);

	if (unlikely(ret))
		pr_err("fail to update video 0x%x\n", ret);

	return ret;
}

static int sprd_img_check_fmt(struct camera_file *camerafile,
			      struct sprd_img_format *img_format)
{
	int ret = 0;
	uint32_t channel_id;
	struct camera_dev *dev = NULL;
	struct camera_group *group = NULL;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_format *fmt;

	if (!camerafile) {
		ret = -EFAULT;
		pr_err("fail to get camerafile NULL\n");
		goto exit;
	}

	if (!img_format) {
		ret = -EFAULT;
		pr_err("fail to get img format parm NULL\n");
		goto exit;
	}

	group = camerafile->grp;
	idx = camerafile->idx;
	if (unlikely((int)idx < 0 || idx >= DCAM_ID_MAX)) {
		pr_err("fail to get dcam idx=%d\n", idx);
		ret = -EFAULT;
		goto exit;
	}
	if (group->dev_inited & (1 << (int)idx)) {
		dev = group->dev[idx];
		if (!dev) {
			ret = -EFAULT;
			pr_err("fail to get dev[%d] NULL\n", idx);
			goto exit;
		}
	} else {
		ret = -EFAULT;
		pr_err("fail to init, dev[%d] hasn't been inited!\n", idx);
		goto exit;
	}
	dev->use_path = img_format->buffer_cfg_isp;

	fmt = sprd_img_get_format(img_format->fourcc);
	if (unlikely(!fmt)) {
		pr_err("fail to get fourcc format (0x%08x) invalid.\n",
		       img_format->fourcc);
		ret = -EINVAL;
		goto exit;
	}

	if (img_format->channel_id == CAMERA_RAW_PATH) {
		mutex_lock(&dev->dcam_mutex);
		ret = sprd_img_check_path_raw_cap(fmt->fourcc, img_format,
						  &dev->dcam_cxt);
		mutex_unlock(&dev->dcam_mutex);
		channel_id = CAMERA_RAW_PATH;
	} else {
		mutex_lock(&dev->dcam_mutex);
		ret = sprd_img_check_path_cap(fmt->fourcc, img_format,
					      &dev->dcam_cxt,
					      img_format->channel_id);
		mutex_unlock(&dev->dcam_mutex);
		channel_id = img_format->channel_id;
	}

	if (channel_id < CAMERA_PDAF_PATH) {
		img_format->endian.y_endian =
			dev->dcam_cxt.dcam_path[channel_id].end_sel.y_endian;
		img_format->endian.uv_endian =
			dev->dcam_cxt.dcam_path[channel_id].end_sel.uv_endian;
	}

	if ((ret == 0) && (atomic_read(&dev->stream_on) != 0)) {
		if (channel_id == CAMERA_PRE_PATH ||
		    channel_id == CAMERA_VID_PATH ||
		    channel_id == CAMERA_CAP_PATH)
			ret = sprd_img_update_video(dev, channel_id);
	}

exit:
	return ret;
}


static int sprd_img_set_crop(struct camera_file *camerafile,
			     struct sprd_img_parm *p)
{
	int ret = 0;
	struct camera_dev *dev = NULL;
	struct camera_group *group = NULL;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_rect *input_rect;
	struct camera_size *input_size;

	if (!camerafile) {
		ret = -EFAULT;
		pr_err("fail to get camerafile NULL\n");
		goto exit;
	}

	if (!p) {
		ret = -EFAULT;
		pr_err("fail to get img parm NULL\n");
		goto exit;
	}

	group = camerafile->grp;
	idx = camerafile->idx;
	if (unlikely((int)idx < 0 || idx >= DCAM_ID_MAX)) {
		pr_err("fail to get dcam idx=%d\n", idx);
		ret = -EFAULT;
		goto exit;
	}
	if (group->dev_inited & (1 << (int)idx)) {
		dev = group->dev[idx];
		if (!dev) {
			ret = -EFAULT;
			pr_err("fail to get dev[%d] NULL\n", idx);
			goto exit;
		}
	} else {
		ret = -EFAULT;
		pr_err("fail to init, dev[%d] hasn't been inited!\n", idx);
		goto exit;
	}

	if (p->crop_rect.x + p->crop_rect.w > dev->dcam_cxt.cap_in_size.w ||
	    p->crop_rect.y + p->crop_rect.h > dev->dcam_cxt.cap_in_size.h) {
		ret = -EINVAL;
		pr_err("fail to set crop_rect, window %d %d %d %d\n",
		       p->crop_rect.x, p->crop_rect.y,
		       p->crop_rect.w, p->crop_rect.h);
		goto exit;
	}
#if 0
	/* Resolves size issue for front cam/rear two cam */
	/* only for bringup preview, remove this when isp enable scaling */
	do {
		struct camera_info *info = &dev->dcam_cxt;
		/* if need 1/2 binning,default: dst_size < cap_out_size */
		if (info->cap_out_size.w >= (2 * info->dst_size.w)) {
			p->crop_rect.w = info->dst_size.w * 2;
			p->crop_rect.h = info->dst_size.h * 2;
			p->crop_rect.x = (info->cap_out_size.w -
						p->crop_rect.w) / 2;
			p->crop_rect.y = (info->cap_out_size.h -
						p->crop_rect.h) / 2;
		} else {
			p->crop_rect.w = dev->dcam_cxt.dst_size.w;
			p->crop_rect.h = dev->dcam_cxt.dst_size.h;
			p->crop_rect.x = (info->cap_out_size.w -
						p->crop_rect.w) / 2;
			p->crop_rect.y = (info->cap_out_size.h -
						p->crop_rect.h) / 2;
		}
	} while (0);
	/* end */
#endif
	DCAM_TRACE("dcam%d: set crop, window %d %d %d %d\n", idx,
		   p->crop_rect.x, p->crop_rect.y,
		   p->crop_rect.w, p->crop_rect.h);

	switch (p->channel_id) {
	case CAMERA_RAW_PATH:
	case CAMERA_PRE_PATH:
	case CAMERA_VID_PATH:
	case CAMERA_CAP_PATH:
	case CAMERA_PDAF_PATH:
	case CAMERA_AEM_PATH:
		input_size = &dev->dcam_cxt.dcam_path[p->channel_id].in_size;
		input_rect = &dev->dcam_cxt.dcam_path[p->channel_id].in_rect;
		break;
	default:
		pr_err("dcam%d: fail to get right channel id %d\n",
		       idx, p->channel_id);
		ret = -EINVAL;
		goto exit;
	}

	input_size->w = dev->dcam_cxt.cap_out_size.w;
	input_size->h = dev->dcam_cxt.cap_out_size.h;
	input_rect->x = p->crop_rect.x;
	input_rect->y = p->crop_rect.y;
	input_rect->w = p->crop_rect.w;
	input_rect->h = p->crop_rect.h;
	DCAM_TRACE("path %d, in %d %d, rect %d %d %d %d\n",
		p->channel_id, input_size->w, input_size->h,
		input_rect->x, input_rect->y, input_rect->w, input_rect->h);

exit:
	return ret;
}

static int sprd_img_set_sensor_if(struct camera_file *camerafile,
				  struct sprd_img_sensor_if *sensor_if)
{
	int ret = 0;
	struct camera_dev *dev = NULL;
	struct camera_group *group = NULL;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_info *info = NULL;

	if (!camerafile) {
		ret = -EFAULT;
		pr_err("fail to get camerafile NULL\n");
		goto exit;
	}

	if (!sensor_if) {
		ret = -EFAULT;
		pr_err("fail to get sensor_if parm NULL\n");
		goto exit;
	}

	group = camerafile->grp;
	idx = camerafile->idx;
	if ((int)idx < 0 || idx >= DCAM_ID_MAX) {
		ret = -EFAULT;
		pr_err("fail to get idx=%d\n", idx);
		goto exit;
	}
	if (group->dev_inited & (1 << (int)idx)) {
		dev = group->dev[idx];
		if (!dev) {
			ret = -EFAULT;
			pr_err("fail to get dev[%d] NULL\n", idx);
			goto exit;
		}
	} else {
		ret = -EFAULT;
		pr_err("fail to init, dev[%d] hasn't been inited!\n", idx);
		goto exit;
	}

	if (sensor_if->res[0] != IF_OPEN)
		goto exit; /* TBD: ret == 0? */

	info = &dev->dcam_cxt;
	info->if_mode = sensor_if->if_type;
	info->sn_mode = sensor_if->img_fmt;
	info->img_ptn = sensor_if->img_ptn;
	info->frm_deci = sensor_if->frm_deci;

	DCAM_TRACE("dcam%d: if %d mode %d deci %d bayer %d\n",
		   idx, info->if_mode, info->sn_mode,
		   info->frm_deci, info->img_ptn);

	if (info->if_mode == DCAM_CAP_IF_CCIR) {
		/* CCIR interface */
		info->sync_pol.vsync_pol = sensor_if->if_spec.ccir.v_sync_pol;
		info->sync_pol.hsync_pol = sensor_if->if_spec.ccir.h_sync_pol;
		info->sync_pol.pclk_pol = sensor_if->if_spec.ccir.pclk_pol;
		info->data_bits = 8;
	} else {
		info->sync_pol.need_href = sensor_if->if_spec.mipi.use_href;
		info->is_loose = sensor_if->if_spec.mipi.is_loose;
		info->data_bits = sensor_if->if_spec.mipi.bits_per_pxl;
	}

exit:

	return ret;
}

static int sprd_img_set_frame_addr(struct camera_file *camerafile,
				   struct sprd_img_parm *p)
{
	int ret = 0;
	uint32_t i = 0;
	struct camera_dev *dev = NULL;
	struct camera_group *group = NULL;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_path_spec *path = NULL;
	enum isp_path_index path_index;

	if (!camerafile) {
		ret = -EFAULT;
		pr_err("fail to get camerafile NULL\n");
		goto exit;
	}

	if (!p) {
		ret = -EFAULT;
		pr_err("fail to get img parm NULL\n");
		goto exit;
	}

	group = camerafile->grp;
	idx = camerafile->idx;
	if (unlikely((int)idx < 0 || idx >= DCAM_ID_MAX)) {
		pr_err("fail to get dcam idx=%d\n", idx);
		ret = -EFAULT;
		goto exit;
	}
	if (group->dev_inited & (1 << (int)idx)) {
		dev = group->dev[idx];
		if (!dev) {
			ret = -EFAULT;
			pr_err("fail to get dev[%d] NULL\n", idx);
			goto exit;
		}
	} else {
		ret = -EFAULT;
		pr_err("fail to init, dev[%d] hasn't been inited!\n", idx);
		goto exit;
	}

	switch (p->channel_id) {
	case CAMERA_RAW_PATH:
	case CAMERA_PRE_PATH:
	case CAMERA_VID_PATH:
	case CAMERA_CAP_PATH:
	case CAMERA_PDAF_PATH:
		path = &dev->dcam_cxt.dcam_path[p->channel_id];
		break;
	default:
		pr_err("fail to get valid channel %d\n", p->channel_id);
		ret = -EINVAL;
		goto exit;
	}

	pr_debug("set path%d frame addr,status %d cnt %d reserved_buf %d\n",
		p->channel_id, path->status, path->frm_cnt_act,
		p->is_reserved_buf);

	if (unlikely(p->fd_array[0] == 0)) {
		pr_err("fail to get fd 0\n");
		ret = -EINVAL;
		goto exit;
	}

	if (p->is_reserved_buf == 1) {
		path->frm_reserved_addr.yaddr = p->frame_addr_array[0].y;
		path->frm_reserved_addr.uaddr = p->frame_addr_array[0].u;
		path->frm_reserved_addr.vaddr = p->frame_addr_array[0].v;
		path->frm_reserved_addr_vir.yaddr =
						p->frame_addr_vir_array[0].y;
		path->frm_reserved_addr_vir.uaddr =
						p->frame_addr_vir_array[0].u;
		path->frm_reserved_addr_vir.vaddr =
						p->frame_addr_vir_array[0].v;
		path->frm_reserved_addr.mfd_y = p->fd_array[0];
		path->frm_reserved_addr.mfd_u = p->fd_array[0];
		path->frm_reserved_addr.mfd_v = p->fd_array[0];
	} else {
		struct camera_addr frame_addr;
		struct camera_img_buf_addr buf_addr;

		if (atomic_read(&dev->stream_on) == 1 &&
			path->status == PATH_RUN) {
			path_index = sprd_img_get_path_index(p->channel_id);
			for (i = 0; i < p->buffer_count; i++) {
				if (p->fd_array[0] == 0) {
					pr_info("no fd\n");
					ret = -EINVAL;
					goto exit;
				}
				frame_addr.yaddr =
					p->frame_addr_array[i].y;
				frame_addr.uaddr =
					p->frame_addr_array[i].u;
				frame_addr.vaddr =
					p->frame_addr_array[i].v;
				frame_addr.yaddr_vir =
					p->frame_addr_vir_array[i].y;
				frame_addr.uaddr_vir =
					p->frame_addr_vir_array[i].u;
				frame_addr.vaddr_vir =
					p->frame_addr_vir_array[i].v;
				frame_addr.mfd_y = p->fd_array[i];
				frame_addr.mfd_u = p->fd_array[i];
				frame_addr.mfd_v = p->fd_array[i];
				frame_addr.user_fid = p->user_fid;
				pr_debug("after stream_on phy:0x%08x, 0x%08x, 0x%08x\n",
						frame_addr.yaddr,
						frame_addr.uaddr,
						frame_addr.vaddr);
				pr_debug("a chn_id %d, user_fid %d, vir:0x%08x, 0x%08x, 0x%08x\n",
						p->channel_id,
						p->user_fid,
						frame_addr.yaddr_vir,
						frame_addr.uaddr_vir,
						frame_addr.vaddr_vir);

				switch (p->channel_id) {
				case CAMERA_RAW_PATH:
					ret = set_dcam_raw_path_cfg(idx,
						DCAM_PATH_OUTPUT_ADDR,
						&frame_addr);
					break;
				case CAMERA_PDAF_PATH:
					ret = set_dcam_pdaf_path_cfg(idx,
						DCAM_PATH_PDAF_OUTPUT_ADDR,
						&frame_addr);
					break;
				default:
					ret = set_isp_path_cfg(
						dev->isp_dev_handle,
						path_index,
						ISP_PATH_OUTPUT_ADDR,
						&frame_addr);
				}
				if (unlikely(ret)) {
					pr_err("fail to isp cfg channel %d,ret %d",
						p->channel_id, ret);
					goto exit;
				}
			}
		} else {
			for (i = 0; i < p->buffer_count; i++) {
				if (unlikely(p->fd_array[0] == 0)) {
					pr_info("no fd\n");
					ret = -EINVAL;
					goto exit;
				}
				buf_addr.frm_addr.yaddr =
						p->frame_addr_array[i].y;
				buf_addr.frm_addr.uaddr =
						p->frame_addr_array[i].u;
				buf_addr.frm_addr.vaddr =
						p->frame_addr_array[i].v;
				buf_addr.frm_addr_vir.yaddr =
						p->frame_addr_vir_array[i].y;
				buf_addr.frm_addr_vir.uaddr =
						p->frame_addr_vir_array[i].u;
				buf_addr.frm_addr_vir.vaddr =
						p->frame_addr_vir_array[i].v;
				buf_addr.frm_addr.mfd_y = p->fd_array[i];
				buf_addr.frm_addr.mfd_u = p->fd_array[i];
				buf_addr.frm_addr.mfd_v = p->fd_array[i];
				buf_addr.frm_addr.user_fid = p->user_fid;

				pr_debug("before stream_on, phy:0x%08x, 0x%08x, 0x%08x\n",
						buf_addr.frm_addr.yaddr,
						buf_addr.frm_addr.uaddr,
						buf_addr.frm_addr.vaddr);
				pr_debug("b chn_id %d, user_fid %d, vir:0x%08x, 0x%08x, 0x%08x\n",
						p->channel_id,
						p->user_fid,
						buf_addr.frm_addr_vir.yaddr,
						buf_addr.frm_addr_vir.uaddr,
						buf_addr.frm_addr_vir.vaddr);
				if(p->user_fid!=-1 && p->channel_id ==2 && atomic_read(&dev->stream_on) == 0){
				    goto exit;
				}
				ret = sprd_img_buf_queue_write(
					&path->buf_queue,
					&buf_addr);
			}
		}
	}

exit:
	return ret;
}

static int sprd_img_get_free_channel(struct camera_file *camerafile,
				     uint32_t *channel_id, uint32_t scene_mode)
{
	int ret = 0;
	struct camera_dev *dev = NULL;
	struct camera_group *group = NULL;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_path_spec *path = NULL;
	struct camera_get_path_id path_id;
	struct isp_pipe_dev *isp_dev = NULL;
	int i;

	if (!camerafile) {
		ret = -EFAULT;
		pr_err("fail to get camerafile NULL\n");
		goto exit;
	}

	if (!channel_id) {
		ret = -EFAULT;
		pr_err("fail to get parm NULL\n");
		goto exit;
	}

	group = camerafile->grp;
	idx = camerafile->idx;
	if (unlikely((int)idx < 0 || idx >= DCAM_ID_MAX)) {
		pr_err("fail to get dcam idx=%d\n", idx);
		ret = -EFAULT;
		goto exit;
	}
	if (group->dev_inited & (1 << (int)idx)) {
		dev = group->dev[idx];
		if (!dev) {
			ret = -EFAULT;
			pr_err("fail to get dev[%d] NULL\n", idx);
			goto exit;
		}
	} else {
		ret = -EFAULT;
		pr_err("fail to init, dev[%d] hasn't been inited!\n", idx);
		goto exit;
	}

	isp_dev = (struct isp_pipe_dev *)dev->isp_dev_handle;
	memset((void *)&path_id, 0, sizeof(struct camera_get_path_id));
	path_id.input_size.w = dev->dcam_cxt.cap_in_rect.w;
	path_id.input_size.h = dev->dcam_cxt.cap_in_rect.h;
	path_id.output_size.w = dev->dcam_cxt.dst_size.w;
	path_id.output_size.h = dev->dcam_cxt.dst_size.h;
	path_id.fourcc = dev->dcam_cxt.pxl_fmt;
	path_id.need_isp_tool = dev->dcam_cxt.need_isp_tool;
	path_id.raw_callback = dev->dcam_cxt.raw_callback;
	path_id.need_isp = dev->dcam_cxt.need_isp;
	path_id.rt_refocus = dev->dcam_cxt.rt_refocus;
	path_id.input_trim.x = dev->dcam_cxt.path_input_rect.x;
	path_id.input_trim.y = dev->dcam_cxt.path_input_rect.y;
	path_id.input_trim.w = dev->dcam_cxt.path_input_rect.w;
	path_id.input_trim.h = dev->dcam_cxt.path_input_rect.h;

	if (dev->dcam_cxt.need_isp_tool && isp_dev) {
		pr_info("raw_capture enabled\n");
		isp_dev->is_raw_capture = 1;
	}

	for (i = 0; i < CAMERA_MAX_PATH; i++) {
		path = &(dev->dcam_cxt.dcam_path[i]);
		DCAM_TRACE("get parm, CAMERA_PATH:%d, is_work:%d\n",
				i, path->is_work);
		path_id.is_path_work[i] = path->is_work;
	}
	ret = sprd_camera_get_path_id(&path_id, channel_id, scene_mode);
	DCAM_TRACE("get channel %d\n", *channel_id);

exit:
	return ret;
}

static int sprd_statistics_k_ioctl(void *dev_handle, struct camera_info *info,
				   uint32_t cmd, unsigned long arg,
				   enum dcam_id idx)
{
	int ret = 0;
	struct isp_pipe_dev *dev = NULL;
	dcam_cfg_fun_ptr cfg_fun_ptr = NULL;
	struct isp_k_block *isp_k_param = NULL;
	uint32_t i = 0;
	struct isp_io_param isp_param = {0, 0, 0, 0, NULL};
	struct isp_io_param __user *data;

	data = (void __user *)arg;

	if (unlikely(!dev_handle)) {
		ret = -EFAULT;
		pr_err("fail to get dev handle is NULL\n");
		goto exit;
	}

	dev = (struct isp_pipe_dev *)dev_handle;

	isp_k_param = &dev->isp_k_param;
	if (unlikely(!isp_k_param)) {
		ret = -EFAULT;
		pr_err("fail to get isp_k_param is null\n");
		goto exit;
	}

	switch (cmd) {
	case SPRD_STATIS_IO_CFG_PARAM:
		ret = copy_from_user(&isp_param, (void __user *)data,
				     sizeof(struct isp_io_param));
		if (unlikely(ret)) {
			pr_err("fail to get user info\n");
			goto exit;
		}

		if (isp_param.sub_block == DCAM_BLOCK_2D_LSC) {
			ret = dcam_k_cfg_2d_lsc(&isp_param, isp_k_param, idx);
		} else if (isp_param.sub_block == DCAM_BLOCK_PDAF) {
			ret = dcam_k_cfg_pdaf(&isp_param, idx,
				&info->dcam_path[CAMERA_PDAF_PATH].pdaf_ctrl);
		} else {
			for (i = 0; i < ARRAY_SIZE(dcam_cfg_fun_tab); i++) {
				if (isp_param.sub_block ==
					dcam_cfg_fun_tab[i].sub_block) {
					cfg_fun_ptr =
						dcam_cfg_fun_tab[i].cfg_fun;
					break;
				}
			}

			if (cfg_fun_ptr != NULL)
				ret = cfg_fun_ptr(&isp_param, idx);
		}
		break;
	default:
		pr_err("fail to supported, cmd = 0x%x\n", cmd);
		ret = -EFAULT;
		break;
	}

exit:
	return ret;
}

int sprd_img_get_dcam_dev(struct camera_file *pcamerafile,
				 struct camera_dev **ppdev,
				 struct camera_info **ppinfo)
{
	int ret = 0;
	int idx = 0;
	struct camera_group *group = NULL;

	if (!pcamerafile) {
		ret = -EFAULT;
		pr_err("fail to get camerafile\n");
		goto exit;
	}
	group = pcamerafile->grp;
	if (!group) {
		ret = -EFAULT;
		pr_err("fail to get group\n");
		goto exit;
	}

	idx = pcamerafile->idx;
	if (unlikely(idx < 0 || idx >= DCAM_ID_MAX)) {
		pr_err("fail to get dcam idx=%d\n", idx);
		ret = -EFAULT;
		goto exit;
	}
	if (group->dev_inited & (1 << (int)idx)) {
		*ppdev = group->dev[idx];
		if (!*ppdev) {
			ret = -EFAULT;
			pr_err("fail to get dcam dev[%d]\n", idx);
			goto exit;
		}
		*ppinfo = &(*ppdev)->dcam_cxt;
	} else {
		ret = -EFAULT;
		pr_err("fail to get dcam dev[%d] and info\n", idx);
		goto exit;
	}

exit:
	return ret;
}

#define FEATRUE_DCAM_IOCTRL
#include "dcam_ioctrl.c"
#undef FEATRUE_DCAM_IOCTRL

static long sprd_img_k_ioctl(struct file *file, uint32_t cmd,
			     unsigned long arg)
{
	long ret = 0;
	struct camera_file *camerafile = NULL;
	dcam_io_fun io_ctrl = NULL;

	camerafile = (struct camera_file *)file->private_data;
	if (!camerafile) {
		ret = -EFAULT;
		pr_err("fail to get valid input ptr\n");
		goto exit;
	}
	if(cmd == SPRD_IMG_IO_SET_KEY || camerafile->private_key == 1) {
		io_ctrl = dcam_ioctl_get_fun(cmd);
		if (io_ctrl != NULL) {
			ret = io_ctrl(camerafile, arg, cmd);
			if (ret) {
				pr_err("fail to cmd %d\n", _IOC_NR(cmd));
				goto exit;
			}
		} else {
			pr_debug("fail to get valid cmd 0x%x 0x%x\n", cmd,
				 _IOC_NR(cmd));
		}
	}
exit:
	return 0;
}

static ssize_t sprd_img_read(struct file *file, char __user *u_data,
			     size_t cnt, loff_t *cnt_ret)
{
	int ret = 0;
	int i = 0;
	struct camera_file *camerafile = file->private_data;
	struct camera_group *group = camerafile->grp;
	int idx = camerafile->idx;
	struct camera_dev *dev = group->dev[idx];
	struct camera_path_spec *path;
	struct sprd_img_read_op read_op;
	struct camera_node node;
	struct dcam_path_info *info;
	struct sprd_img_path_info *info_ret;
	struct dcam_path_capability path_capability;

	if (unlikely(idx < 0 || idx >= DCAM_ID_MAX)) {
		pr_err("fail to get dcam idx=%d\n", idx);
		return -EFAULT;
	}
	if (cnt != sizeof(struct sprd_img_read_op)) {
		pr_err("fail to read, cnt %zd read_op %zd\n", cnt,
		       sizeof(struct sprd_img_read_op));
		return -EIO;
	}

	if (copy_from_user(&read_op, (void __user *)u_data, cnt)) {
		pr_err("fail to get user info\n");
		return -EFAULT;
	}

	if (!dev) {
		pr_err("fail to get dev is not init %p\n", dev);
		return -EFAULT;
	}

	switch (read_op.cmd) {
	case SPRD_IMG_GET_SCALE_CAP:
		DCAM_TRACE("get scale capbility\n");
		read_op.parm.reserved[0] = ISP_PATH2_LINE_BUF_LENGTH;
		read_op.parm.reserved[1] = CAMERA_SC_COEFF_UP_MAX;
		read_op.parm.reserved[2] = DCAM_SCALING_THRESHOLD;
		DCAM_TRACE("line threshold %d, sc factor %d, scaling %d.\n",
			   read_op.parm.reserved[0], read_op.parm.reserved[1],
			   read_op.parm.reserved[2]);
		break;
	case SPRD_IMG_GET_FRM_BUFFER:
		idx = sprd_sensor_find_dcam_id(read_op.sensor_id);
		if (idx < 0) {
			pr_err("fail to get dcam_id %d\n", idx);
			return -EFAULT;
		}
		dev = group->dev[idx];
		memset(&read_op, 0, sizeof(struct sprd_img_read_op));
		while (1) {
			ret = wait_for_completion_interruptible(&dev->irq_com);
			if (ret == 0)
				break;
			else if (ret == -ERESTARTSYS) {
				read_op.evt = IMG_SYS_BUSY;
				ret = DCAM_RTN_SUCCESS;
				goto read_end;
			} else {
				pr_err("fail to read frame buf %d\n", ret);
				WARN_ON(1);
				return -EPERM;
			}
		}

		if (sprd_img_queue_read(&dev->queue, &node)) {
			DCAM_TRACE("read frame buffer, queue is null\n");
			read_op.evt = IMG_SYS_BUSY;
			ret = DCAM_RTN_SUCCESS;
			goto read_end;
		} else {
			if (node.invalid_flag) {
				DCAM_TRACE("read frame buffer, invalid node\n");
				read_op.evt = IMG_SYS_BUSY;
				ret = DCAM_RTN_SUCCESS;
				goto read_end;
			}
		}

		read_op.evt = node.irq_flag;
		if (read_op.evt == IMG_TX_DONE) {
			read_op.parm.frame.channel_id = node.f_type;
			path =
			&dev->dcam_cxt.dcam_path[read_op.parm.frame.channel_id];
			read_op.parm.frame.index = path->frm_id_base;
			read_op.parm.frame.height = node.height;
			read_op.parm.frame.length = node.reserved[0];
			read_op.parm.frame.sec = node.time.tv_sec;
			read_op.parm.frame.usec = node.time.tv_usec;
			read_op.parm.frame.monoboottime = node.boot_time;
			read_op.parm.frame.frm_base_id = path->frm_id_base;
			read_op.parm.frame.img_fmt = path->fourcc;
			read_op.parm.frame.yaddr = node.yaddr;
			read_op.parm.frame.uaddr = node.uaddr;
			read_op.parm.frame.vaddr = node.vaddr;
			read_op.parm.frame.yaddr_vir = node.yaddr_vir;
			read_op.parm.frame.uaddr_vir = node.uaddr_vir;
			read_op.parm.frame.vaddr_vir = node.vaddr_vir;
			read_op.parm.frame.phy_addr = node.phy_addr;
			read_op.parm.frame.vir_addr = node.vir_addr;
			read_op.parm.frame.addr_offset = node.addr_offset;
			read_op.parm.frame.kaddr[0] = node.kaddr[0];
			read_op.parm.frame.kaddr[1] = node.kaddr[1];
			read_op.parm.frame.buf_size = node.buf_size;
			read_op.parm.frame.irq_type = node.irq_type;
			read_op.parm.frame.irq_property = node.irq_property;
			read_op.parm.frame.frame_id = node.frame_id;
			read_op.parm.frame.mfd = node.mfd[0];
			read_op.parm.frame.dac_info = node.dac_info;
			DCAM_TRACE("index%d real_index %d frm_id_base 0x%x\n",
				   read_op.parm.frame.index,
				   read_op.parm.frame.real_index,
				   read_op.parm.frame.frm_base_id);
		} else {
			if (read_op.evt == IMG_TIMEOUT ||
			    read_op.evt == IMG_TX_ERR) {
				read_op.parm.frame.irq_type = node.irq_type;
				read_op.parm.frame.irq_property
					= node.irq_property;
				pr_info("DCAM_CORE: error evt %d\n",
					read_op.evt);
			#if 0
				csi_api_reg_trace();
			#endif

			}
		}
		DCAM_TRACE("read frame,evt 0x%x channel 0x%x index 0x%x\n",
			   read_op.evt, read_op.parm.frame.channel_id,
			   read_op.parm.frame.index);

		if (read_op.parm.frame.irq_property == IRQ_RAW_CAP_DONE)
			pr_info("sprd_img_read: raw cap\n");

		break;
	case SPRD_IMG_GET_PATH_CAP:
		DCAM_TRACE("get path capbility\n");
		sprd_dcam_get_path_capability(&path_capability);
		read_op.parm.capability.count = path_capability.count;
		read_op.parm.capability.support_3dnr_mode =
			path_capability.support_3dnr_mode;
		for (i = 0; i < path_capability.count; i++) {
			info = &path_capability.path_info[i];
			info_ret = &read_op.parm.capability.path_info[i];
			info_ret->line_buf = info->line_buf;
			info_ret->support_yuv = info->support_yuv;
			info_ret->support_raw = info->support_raw;
			info_ret->support_jpeg = info->support_jpeg;
			info_ret->support_scaling = info->support_scaling;
			info_ret->support_trim = info->support_trim;
			info_ret->is_scaleing_path = info->is_scaleing_path;
		}
		break;
	default:
		pr_err("fail to get valid cmd\n");
		return -EINVAL;
	}

read_end:
	if (copy_to_user((void __user *)u_data, &read_op, cnt))
		ret = -EFAULT;

	if (ret)
		cnt = ret;

	return cnt;
}

static ssize_t sprd_img_write(struct file *file, const char __user *u_data,
			      size_t cnt, loff_t *cnt_ret)
{
	struct camera_file *camerafile = file->private_data;
	struct camera_group *group = camerafile->grp;
	int idx = camerafile->idx;
	struct camera_dev *dev = group->dev[idx];
	struct camera_info *info;
	struct sprd_img_write_op write_op;
	struct camera_path_spec *path;
	int ret = 0;
	uint32_t index;

	if (unlikely(idx < 0 || idx >= DCAM_ID_MAX)) {
		pr_err("fail to get dcam idx=%d\n", idx);
		return -EFAULT;
	}
	if (cnt != sizeof(struct sprd_img_write_op)) {
		pr_err("fail to write, cnt %zd write_op %zd\n", cnt,
		       sizeof(struct sprd_img_write_op));
		return -EIO;
	}

	if (copy_from_user(&write_op, (void __user *)u_data, cnt)) {
		pr_err("fail to get user info\n");
		return -EFAULT;
	}

	if (!dev)
		return -EFAULT;
	info = &dev->dcam_cxt;

	switch (write_op.cmd) {
	case SPRD_IMG_STOP_DCAM:
		idx = sprd_sensor_find_dcam_id(write_op.sensor_id);
		if (idx < 0) {
			pr_err("fail to get dcam_id %d\n", idx);
			return -EFAULT;
		}
		dev = group->dev[idx];
		if (!dev) {
			pr_err("fail to get dev NULL\n");
			return -EFAULT;
		}
		mutex_lock(&dev->dcam_mutex);
		ret = sprd_img_tx_stop(dev);
		mutex_unlock(&dev->dcam_mutex);
		break;
	case SPRD_IMG_FREE_FRAME:
		if (atomic_read(&dev->stream_on) == 0) {
			pr_info("dev close, no need free!");
			break;
		}

		switch (write_op.channel_id) {
		case CAMERA_RAW_PATH:
		case CAMERA_PRE_PATH:
		case CAMERA_VID_PATH:
		case CAMERA_CAP_PATH:
		case CAMERA_PDAF_PATH:
		case CAMERA_AEM_PATH:
			path = &info->dcam_path[write_op.channel_id];
			break;
		default:
			pr_err("fail to free frame buf, channel %d\n",
			       write_op.channel_id);
			return -EINVAL;
		}

		if (path->status == PATH_IDLE) {
			DCAM_TRACE("fail to free frame buf, channel %d\n",
				   write_op.channel_id);
			return -EINVAL;
		}

		if (write_op.index >
		    (path->frm_id_base + path->frm_cnt_act - 1)) {
			pr_err("fail to get index %d, frm_id_base %d frm_cnt_act %d\n",
			       write_op.index, path->frm_id_base,
			       path->frm_cnt_act);
			ret = -EINVAL;
		} else if (write_op.index < path->frm_id_base) {
			pr_err("fail to get index %d, frm_id_base %d\n",
			       write_op.index, path->frm_id_base);
			ret = -EINVAL;
		} else {
			index = write_op.index - path->frm_id_base;

			DCAM_TRACE("free frame buf, channel %d, index 0x%x\n",
				   write_op.channel_id, write_op.index);
		}
		break;
	default:
		pr_err("fail to get cmd\n");
		ret = -EINVAL;
		break;
	}

	if (ret)
		cnt = ret;

	return cnt;
}

static const struct file_operations image_fops = {
	.open = sprd_img_k_open,
	.unlocked_ioctl = sprd_img_k_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_sprd_img_k_ioctl,
#endif
	.release = sprd_img_k_release,
	.read = sprd_img_read,
	.write = sprd_img_write,
};

static struct miscdevice image_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = IMG_DEVICE_NAME,
	.fops = &image_fops,
};

static int sprd_img_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct camera_group *group = NULL;

	if (unlikely(!pdev)) {
		pr_err("fail to get pdev is NULL\n");
		return -EFAULT;
	}

	group = vzalloc(sizeof(struct camera_group));
	if (unlikely(IS_ERR_OR_NULL(group))) {
		pr_err("fail to alloc mem, ret:%ld!", PTR_ERR(group));
		return -ENOMEM;
	}

	ret = misc_register(&image_dev);
	if (unlikely(ret)) {
		pr_err("fail to register misc devices, ret %d\n", ret);
		vfree(group);
		return -EACCES;
	}

	image_dev.this_device->of_node = pdev->dev.of_node;
	image_dev.this_device->platform_data = (void *)group;
	group->pdev = pdev;
	atomic_set(&group->camera_opened, 0);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
    Camera_Sys_Wakelock = wakeup_source_create("Camera Sys Wakelock");
    wakeup_source_add(Camera_Sys_Wakelock);
#else
	wakeup_source_init(&group->ws, "Camera Sys Wakelock");
#endif
	pr_info("sprd img probe pdev name %s\n", pdev->name);
	pr_info("sprd dcam dev name %s\n", pdev->dev.init_name);
	ret = sprd_dcam_parse_dt(pdev->dev.of_node, &group->dcam_count);
	if (unlikely(ret)) {
		pr_err("fail to parse dcam dts\n");
		goto err_exit;
	}

	ret = sprd_isp_parse_dt(pdev->dev.of_node, &group->isp_count);
	if (unlikely(ret)) {
		pr_err("fail to parse isp dts\n");
		goto err_exit;
	}

	if (sprd_dcam_drv_init(pdev)) {
		pr_err("fail to call sprd_dcam_drv_init\n");
		ret = -EFAULT;
		goto err_dcam_init_exit;
	}

	if (sprd_isp_drv_init(pdev)) {
		pr_err("fail to call sprd_isp_drv_init\n");
		ret = -EFAULT;
		goto err_isp_init_exit;
	}

	ret = cam_dbg_init((void *)&image_dev);
	if (unlikely(ret)) {
		pr_err("fail to init cam_dbg!\n");
		goto err_dbg_init_exit;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
#endif
	ret = sysfs_create_group(&image_dev.this_device->kobj,
				 &isp_dbg_img_attrs_group);
	if (unlikely(ret)) {
		pr_err("fail to enable to export sprd_image sysfs\n");
		goto err_dev_grp_exit;
	}

	/* convenient to access sysfs node */
	ret = sysfs_create_link(NULL, &image_dev.this_device->kobj,
				IMG_DEVICE_NAME);
	if (unlikely(ret)) {
		pr_err("fail to create link!\n");
		goto err_dev_link_exit;
	}

	ret = sysfs_create_group(&image_dev.this_device->kobj,
				 &isp_dbg_sblk_attrs_group);
	if (unlikely(ret)) {
		pr_err("fail to enable to export isp_sblk sysfs\n");
		goto err_sblk_grp_exit;
	}

	return 0;

err_sblk_grp_exit:
	sysfs_remove_link(NULL, IMG_DEVICE_NAME);
err_dev_link_exit:
	sysfs_remove_group(&image_dev.this_device->kobj,
			   &isp_dbg_img_attrs_group);
err_dev_grp_exit:
	cam_dbg_deinit((void *)&image_dev);
err_dbg_init_exit:
	sprd_isp_drv_deinit();
err_isp_init_exit:
	sprd_dcam_drv_deinit();
err_dcam_init_exit:
err_exit:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
    wakeup_source_remove(Camera_Sys_Wakelock);
    wakeup_source_destroy(Camera_Sys_Wakelock);
#else
    wakeup_source_trash(&group->ws);
#endif
	vfree(group);
	misc_deregister(&image_dev);

	return ret;
}

static int sprd_img_remove(struct platform_device *pdev)
{
	struct camera_group *group = NULL;

	if (!pdev) {
		pr_err("fail to get pdev is NULL\n");
		return -EFAULT;
	}

	group = image_dev.this_device->platform_data;

	sysfs_remove_group(&image_dev.this_device->kobj,
			   &isp_dbg_sblk_attrs_group);
	sysfs_remove_link(NULL, IMG_DEVICE_NAME);
	sysfs_remove_group(&image_dev.this_device->kobj,
			   &isp_dbg_img_attrs_group);

	cam_dbg_deinit((void *)&image_dev);
	sprd_isp_drv_deinit();
	sprd_dcam_drv_deinit();
	if (group) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
        wakeup_source_remove(Camera_Sys_Wakelock);
        wakeup_source_destroy(Camera_Sys_Wakelock);
#else
        wakeup_source_trash(&group->ws);
#endif
		vfree(group);
	}
	misc_deregister(&image_dev);

	return 0;
}

static void sprd_img_shutdown(struct platform_device *pdev)
{
	struct sprd_img_set_flash set_flash;

	set_flash.led0_ctrl = 1;
	set_flash.led1_ctrl = 1;
	set_flash.led0_status = FLASH_CLOSE;
	set_flash.led1_status = FLASH_CLOSE;
	set_flash.flash_index = 0;
	sprd_img_setflash(DCAM_ID_0, &set_flash);
	set_flash.flash_index = 1;
	sprd_img_setflash(DCAM_ID_0, &set_flash);
}

static const struct of_device_id sprd_dcam_of_match[] = {
	{ .compatible = "sprd,dcam", },
	{},
};

static struct platform_driver sprd_img_driver = {
	.probe = sprd_img_probe,
	.remove = sprd_img_remove,
	.shutdown = sprd_img_shutdown,
	.driver = {
		.owner = THIS_MODULE,
		.name = IMG_DEVICE_NAME,
		.of_match_table = of_match_ptr(sprd_dcam_of_match),
	},
};

module_platform_driver(sprd_img_driver);

MODULE_DESCRIPTION("Sprd DCAM Driver");
MODULE_AUTHOR("Multimedia_Camera@Spreadtrum");
MODULE_LICENSE("GPL");
