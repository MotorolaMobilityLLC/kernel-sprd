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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/highmem.h>
#include <linux/freezer.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/io.h>
#include <sprd_img.h>
#include <linux/vmalloc.h>
#include "dcam_drv.h"
#include "isp_drv.h"
#include "flash_drv.h"
#include "img_scale.h"
#include "img_rot.h"
#include "csi_api.h"
//#include "csi_driver.h"
//#include <video/sprd_mmsys_pw_domain.h>
#include <linux/delay.h>
#include <linux/sprd_iommu.h>
#include <linux/sprd_ion.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <linux/pm_runtime.h>
#include <linux/ion.h>
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "SPRD_IMG: %d " fmt, __LINE__


#define IMG_DEVICE_NAME                         "sprd_image"
#define IMAGE_MINOR                             MISC_DYNAMIC_MINOR
#define DCAM_INVALID_FOURCC                     0xFFFFFFFF
#define DCAM_MAJOR_VERSION                      1
#define DCAM_MINOR_VERSION                      0
#define DCAM_RELEASE                            0
#define DCAM_QUEUE_LENGTH                       128
#define DCAM_TIMING_LEN                         16

#ifdef CONFIG_SC_FPGA
#define DCAM_TIMEOUT                            (2000*100)
#else
#define DCAM_TIMEOUT                            1500
#endif

#define DCAM_ZOOM_LEVEL_MAX                     4
#define DCAM_ZOOM_STEP(x, y)                 (((x) - (y)) / DCAM_ZOOM_LEVEL_MAX)
#define DCAM_PIXEL_ALIGNED                         4
#define DCAM_WIDTH(w)                        ((w) & ~(DCAM_PIXEL_ALIGNED - 1))
#define DCAM_HEIGHT(h)                       ((h) & ~(DCAM_PIXEL_ALIGNED - 1))

#define DCAM_VERSION \
	KERNEL_VERSION(DCAM_MAJOR_VERSION, DCAM_MINOR_VERSION, DCAM_RELEASE)

typedef int (*path_cfg_func)(enum dcam_cfg_id, void *);

enum {
	PATH_IDLE  = 0x00,
	PATH_RUN,
};

struct dcam_format {
	char                       *name;
	uint32_t                   fourcc;
	int                        depth;
};

struct dcam_node {
	uint32_t irq_flag;
	uint32_t irq_type;
	uint32_t f_type;
	uint32_t index;
	uint32_t height;
	uint32_t yaddr;
	uint32_t uaddr;
	uint32_t vaddr;
	uint32_t yaddr_vir;
	uint32_t uaddr_vir;
	uint32_t vaddr_vir;
	uint32_t phy_addr;
	uint32_t vir_addr;
	uint32_t addr_offset;
	uint32_t kaddr[2];
	uint32_t buf_size;
	uint32_t irq_property;
	uint32_t frame_id;
	uint32_t invalid_flag;
	struct timeval time;
	uint32_t reserved;
	uint32_t mfd[3];
};

struct dcam_queue {
	struct dcam_node           node[DCAM_QUEUE_LENGTH];
	struct dcam_node           *write;
	struct dcam_node           *read;
	uint32_t                           wcnt;
	uint32_t                           rcnt;
};

struct dcam_img_buf_addr {
	struct dcam_addr           frm_addr;
	struct dcam_addr           frm_addr_vir;
};

struct dcam_img_buf_queue {
	struct dcam_img_buf_addr   buf_addr[DCAM_FRM_CNT_MAX];
	struct dcam_img_buf_addr   *write;
	struct dcam_img_buf_addr   *read;
	uint32_t                   wcnt;
	uint32_t                   rcnt;
};

struct dcam_path_spec {
	uint32_t                   is_work;
	uint32_t                   is_from_isp;
	uint32_t                   rot_mode;
	uint32_t                   status;
	struct camera_size         in_size;
	struct dcam_path_dec       img_deci;
	struct camera_rect         in_rect;
	struct camera_rect         in_rect_current;
	struct camera_rect         in_rect_backup;
	struct camera_size         out_size;
	enum   dcam_fmt            out_fmt;
	struct dcam_endian_sel     end_sel;
	uint32_t                   fourcc;
	uint32_t                   pixel_depth;
	uint32_t                   frm_id_base;
	uint32_t                   frm_type;
	uint32_t                   index[DCAM_FRM_CNT_MAX];
	/*struct dcam_addr           frm_addr[DCAM_FRM_CNT_MAX];*/
	/*struct dcam_addr           frm_addr_vir[DCAM_FRM_CNT_MAX];*/
	struct dcam_img_buf_queue  buf_queue;
	struct dcam_addr           frm_reserved_addr;
	struct dcam_addr           frm_reserved_addr_vir;
	struct camera_frame        *frm_ptr[DCAM_FRM_CNT_MAX];
	uint32_t                   frm_cnt_act;
	uint32_t                   path_frm_deci;

	struct dcam_regular_desc   regular_desc;
	struct sprd_pdaf_control   pdaf_ctrl;
};

struct dcam_info {
	uint32_t                   if_mode;
	uint32_t                   sn_mode;
	uint32_t                   yuv_ptn;
	uint32_t                   data_bits;
	uint32_t                   is_loose;
	uint32_t                   lane_num;
	uint32_t                   pclk;
	struct dcam_cap_sync_pol   sync_pol;
	uint32_t                   frm_deci;
	struct dcam_cap_dec        img_deci;
	struct camera_size         cap_in_size;
	struct camera_rect         cap_in_rect;
	struct camera_size         cap_out_size;
	struct camera_size         dst_size;
	uint32_t                   pxl_fmt;
	uint32_t                   need_isp_tool;
	uint32_t                   need_isp;
	uint32_t                   interp_mode;
/*	uint32_t                   need_shrink;*/
	struct camera_rect         path_input_rect;

	struct dcam_path_spec      dcam_path[DCAM_PATH_NUM];

	uint32_t                   capture_mode;
	uint32_t                   skip_number;
	struct sprd_img_set_flash  set_flash;
	uint32_t                   after_af;
	uint32_t                   is_smooth_zoom;
	struct timeval             timestamp;
};

struct dcam_dev {
	struct semaphore         flash_thread_sem;
	struct semaphore         zoom_thread_sem;

	struct completion        irq_com;
	struct completion        flash_thread_com;
	struct completion        zoom_thread_com;

	struct mutex             dcam_mutex;
	atomic_t                 users;
	struct dcam_info         dcam_cxt;
	atomic_t                 stream_on;
	uint32_t                 stream_mode;
	struct dcam_queue        queue;
	struct timer_list        dcam_timer;
	atomic_t                 run_flag;
	uint32_t                 got_resizer;
	struct proc_dir_entry   *proc_file;
	struct task_struct      *flash_thread;
	uint32_t                 is_flash_thread_stop;
	uint32_t                 frame_skipped;
	struct task_struct      *zoom_thread;
	uint32_t                 is_zoom_thread_stop;
	uint32_t                 zoom_level;
	uint32_t                 channel_id;
	void                     *driver_data;
	void                     *isp_dev_handle;
	uint32_t                 private_key;
	uint32_t                 idx;
};

struct dcam_group {
	unsigned int dev_inited;
	unsigned int mode_inited;
	unsigned int dcam_res_used;
	unsigned int cowork_mod;
	unsigned int dcam_count;
	atomic_t dcam_opened;
	struct dcam_dev *dev[DCAM_ID_MAX];
	struct miscdevice *md;
	struct platform_device *pdev;
};

#ifndef __SIMULATOR__

static struct dcam_format dcam_img_fmt[] = {
	{
		.name     = "4:2:2, packed, YUYV",
		.fourcc   = IMG_PIX_FMT_YUYV,
		.depth    = 16,
	},
	{
		.name     = "4:2:2, packed, YVYU",
		.fourcc   = IMG_PIX_FMT_YVYU,
		.depth    = 16,
	},
	{
		.name     = "4:2:2, packed, UYVY",
		.fourcc   = IMG_PIX_FMT_UYVY,
		.depth    = 16,
	},
	{
		.name     = "4:2:2, packed, VYUY",
		.fourcc   = IMG_PIX_FMT_VYUY,
		.depth    = 16,
	},
	{
		.name     = "YUV 4:2:2, planar, (Y-Cb-Cr)",
		.fourcc   = IMG_PIX_FMT_YUV422P,
		.depth    = 16,
	},
	{
		.name     = "YUV 4:2:0 planar (Y-CbCr)",
		.fourcc   = IMG_PIX_FMT_NV12,
		.depth    = 12,
	},
	{
		.name     = "YVU 4:2:0 planar (Y-CrCb)",
		.fourcc   = IMG_PIX_FMT_NV21,
		.depth    = 12,
	},

	{
		.name     = "YUV 4:2:0 planar (Y-Cb-Cr)",
		.fourcc   = IMG_PIX_FMT_YUV420,
		.depth    = 12,
	},
	{
		.name     = "YVU 4:2:0 planar (Y-Cr-Cb)",
		.fourcc   = IMG_PIX_FMT_YVU420,
		.depth    = 12,
	},
	{
		.name     = "RGB565 (LE)",
		.fourcc   = IMG_PIX_FMT_RGB565,
		.depth    = 16,
	},
	{
		.name     = "RGB565 (BE)",
		.fourcc   = IMG_PIX_FMT_RGB565X,
		.depth    = 16,
	},
	{
		.name     = "RawRGB",
		.fourcc   = IMG_PIX_FMT_GREY,
		.depth    = 8,
	},
	{
		.name     = "JPEG",
		.fourcc   = IMG_PIX_FMT_JPEG,
		.depth    = 8,
	},
};
#else
static struct dcam_format dcam_img_fmt[] = {
	{"4:2:2, packed, YUYV", IMG_PIX_FMT_YUYV, 16},
	{"4:2:2, packed, YVYU", IMG_PIX_FMT_YVYU, 16},
	{"4:2:2, packed, UYVY", IMG_PIX_FMT_UYVY, 16},
	{"4:2:2, packed, VYUY", IMG_PIX_FMT_VYUY, 16},
	{"YUV 4:2:2, planar, (Y-Cb-Cr)", IMG_PIX_FMT_YUV422P, 16},
	{"YUV 4:2:0 planar (Y-CbCr)", IMG_PIX_FMT_NV12, 12},
	{"YVU 4:2:0 planar (Y-CrCb)", IMG_PIX_FMT_NV21, 12},
	{"YUV 4:2:0 planar (Y-Cb-Cr)", IMG_PIX_FMT_YUV420, 12},
	{"YVU 4:2:0 planar (Y-Cr-Cb)", IMG_PIX_FMT_YVU420, 12},
	{"RGB565 (LE)", IMG_PIX_FMT_RGB565, 16},
	{"RGB565 (BE)", IMG_PIX_FMT_RGB565X, 16},
	{"RawRGB", IMG_PIX_FMT_GREY, 8},
	{"JPEG", IMG_PIX_FMT_JPEG, 8}
};
#endif

#define DISCARD_FRAME_TIME (10000)

static int img_get_timestamp(struct timeval *tv)
{
	struct timespec ts;

	ktime_get_ts(&ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / NSEC_PER_USEC;
	return 0;
}

/*TODO: flash moved to flash_drv.c , WE MUST Follow the code in this branch*/
static int sprd_img_setflash(struct sprd_img_set_flash *set_flash)
{
	/* need to do this later */
	sprd_flash_ctrl(set_flash);
	DCAM_TRACE("sprd_img_setflash: flash_mode 0x%x.\n",
		set_flash->led0_status);
	return 0;
}

static int sprd_img_opt_flash(struct camera_frame *frame, void *param)
{
	struct dcam_dev          *dev = (struct dcam_dev *)param;
	struct dcam_info         *info = NULL;
	uint32_t led0_ctrl;
	uint32_t led1_ctrl;
	uint32_t led0_status;
	uint32_t led1_status;

	if (dev == NULL) {
		DCAM_TRACE("sprd_img_opt_flash, dev is NULL.\n");
		return 0;
	}

	info = &dev->dcam_cxt;
	led0_ctrl = info->set_flash.led0_ctrl;
	led1_ctrl = info->set_flash.led1_ctrl;
	led0_status = info->set_flash.led0_status;
	led1_status = info->set_flash.led1_status;

	if ((led0_ctrl && led0_status < FLASH_STATUS_MAX) ||
		(led1_ctrl && led1_status < FLASH_STATUS_MAX)) {
		DCAM_TRACE("sprd_img_opt_flash, status %d.\n",
		led0_status);
		if (led0_status == FLASH_CLOSE_AFTER_AUTOFOCUS ||
		    led1_status == FLASH_CLOSE_AFTER_AUTOFOCUS) {
			img_get_timestamp(&info->timestamp);
			info->after_af = 1;
			DCAM_TRACE("SPRD_IMG:sprd_img_opt_flash, time,%d %d.\n",
				(int)info->timestamp.tv_sec,
				(int)info->timestamp.tv_usec);
		}

		sprd_img_setflash(&info->set_flash);
		info->set_flash.led0_ctrl = 0;
		info->set_flash.led1_ctrl = 0;
		info->set_flash.led0_status = FLASH_STATUS_MAX;
		info->set_flash.led1_status = FLASH_STATUS_MAX;
	}

	return 0;
}

static int sprd_img_start_flash(struct camera_frame *frame, void *param)
{
	struct dcam_dev          *dev = (struct dcam_dev *)param;
	struct dcam_info         *info = NULL;
	uint32_t                 need_light = 1;
	uint32_t                 led0_ctrl;
	uint32_t                 led1_ctrl;
	uint32_t                 led0_status;
	uint32_t                 led1_status;

	if (dev == NULL) {
		DCAM_TRACE("sprd_img_start_flash, dev is NULL.\n");
		return -1;
	}

	info = &dev->dcam_cxt;
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
				/*flash lighted at the last SOF before
				 * the right capture frame
				 */
				DCAM_TRACE("waiting finished.\n");
			} else {
				need_light = 0;
				DCAM_TRACE("wait for the next SOF, %d ,%d.\n",
					dev->frame_skipped, info->skip_number);
			}
		}
		if (need_light)
			/*up(&dev->flash_thread_sem);*/
			complete(&dev->flash_thread_com);
	}

	return 0;
}

static int flash_thread_loop(void *arg)
{
	struct dcam_dev          *dev = (struct dcam_dev *)arg;
	struct sprd_img_set_flash set_flash = {0};
	int ret = 0;

	if (dev == NULL) {
		DCAM_TRACE("flash_thread_loop, dev is NULL.\n");
		return -1;
	}
	while (1) {
		/*if (down_interruptible(&dev->flash_thread_sem) == 0) {*/
		ret = wait_for_completion_interruptible(&dev->flash_thread_com);
		if (ret == 0) {
			if (dev->is_flash_thread_stop) {
				set_flash.led0_ctrl = 1;
				set_flash.led1_ctrl = 1;
				set_flash.led0_status = FLASH_CLOSE;
				set_flash.led1_status = FLASH_CLOSE;
				set_flash.flash_index = 0;
				sprd_img_setflash(&set_flash);
				set_flash.flash_index = 1;
				sprd_img_setflash(&set_flash);
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
	struct dcam_dev *dev = (struct dcam_dev *)param;

	if (dev == NULL) {
		DCAM_TRACE("%s:  dev is NULL.\n", __func__);
		return -1;
	}
	dev->is_flash_thread_stop = 0;
	/*sema_init(&dev->flash_thread_sem, 0);*/
	init_completion(&dev->flash_thread_com);
	dev->flash_thread = kthread_run(flash_thread_loop, param,
			    "dcam_flash_thread");
	if (IS_ERR(dev->flash_thread)) {
		pr_info("dcam_create_flash_thread error!\n");
		return -1;
	}
	return 0;
}

static int dcam_stop_flash_thread(void *param)
{
	struct dcam_dev          *dev = (struct dcam_dev *)param;

	if (dev == NULL) {
		pr_err("dcam_stop_flash_thread failed, dev is NULL.\n");
		return -1;
	}
	if (dev->flash_thread) {
		dev->is_flash_thread_stop = 1;
		/*up(&dev->flash_thread_sem);*/
		complete(&dev->flash_thread_com);
		if (dev->is_flash_thread_stop != 0) {
			while (dev->is_flash_thread_stop)
				usleep_range(1000, 1500);
		}
		dev->flash_thread = NULL;
	}

	return 0;
}

static int sprd_img_discard_frame(struct camera_frame *frame, void *param)
{
	int                                  ret = DCAM_RTN_PARA_ERR;
	struct dcam_dev          *dev = (struct dcam_dev *)param;
	struct dcam_info         *info = NULL;
	struct timeval           timestamp;
	uint32_t                 flag = 0;

	info = &dev->dcam_cxt;
	img_get_timestamp(&timestamp);
	DCAM_TRACE("sprd_img_discard_frame, time, %d %d.\n",
		(int)timestamp.tv_sec, (int)timestamp.tv_usec);
	if ((timestamp.tv_sec == info->timestamp.tv_sec) &&
	  (timestamp.tv_usec - info->timestamp.tv_usec >= DISCARD_FRAME_TIME)) {
		flag = 1;
	} else if (timestamp.tv_sec > info->timestamp.tv_sec) {
		if ((1000000 - info->timestamp.tv_sec) + timestamp.tv_sec >=
		     DISCARD_FRAME_TIME) {
			flag = 1;
		}
	}

	if (flag) {
		DCAM_TRACE("sprd_img_discard_frame,unlock frame %p.\n", frame);
		/*dcam_frame_unlock(frame);*/
		ret =  DCAM_RTN_SUCCESS;
	}
	return ret;
}

static struct dcam_format *sprd_img_get_format(uint32_t fourcc)
{
	struct dcam_format       *fmt;
	uint32_t                 i;

	for (i = 0; i < ARRAY_SIZE(dcam_img_fmt); i++) {
		fmt = &dcam_img_fmt[i];
		if (fmt->fourcc == fourcc)
			break;
	}

	if (unlikely(i == ARRAY_SIZE(dcam_img_fmt)))
		return NULL;

	return &dcam_img_fmt[i];
}

static uint32_t sprd_img_get_deci_factor(uint32_t src_size, uint32_t dst_size)
{
	uint32_t                 factor = 0;

	if (src_size == 0 || dst_size == 0)
		return factor;

	for (factor = 0; factor < DCAM_CAP_X_DECI_FAC_MAX; factor++) {
		if (src_size < (uint32_t)(dst_size * (1 << factor)))
			break;
	}

	return factor;
}

static uint32_t sprd_img_endian_sel(uint32_t fourcc,
					struct dcam_path_spec *path)
{
	uint32_t                 depth = 0;

	if (fourcc == IMG_PIX_FMT_YUV422P ||
		fourcc == IMG_PIX_FMT_RGB565 ||
		fourcc == IMG_PIX_FMT_RGB565X) {
		depth = 16;
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
		depth = 12;
		if (fourcc == IMG_PIX_FMT_YUV420 ||
			fourcc == IMG_PIX_FMT_YVU420) {
			path->out_fmt = DCAM_YUV420_3FRAME;
		} else {
			path->out_fmt = DCAM_YUV420;
			if (fourcc == IMG_PIX_FMT_NV12)
				path->end_sel.uv_endian = DCAM_ENDIAN_LITTLE;
			else
				path->end_sel.uv_endian = DCAM_ENDIAN_HALFBIG;
		}
	}

	return depth;
}

static int sprd_img_check_binning(struct sprd_img_format *f,
				  struct dcam_info *info,
				  struct dcam_path_spec *path)
{
	unsigned int tempw;

	if (unlikely(info->cap_out_size.w > DCAM_ISP_LINE_BUF_LENGTH)) {
		if (info->if_mode == DCAM_CAP_IF_CCIR) {
			/*CCIR CAP, no bining*/
			pr_info("CCIR CAP, no bining");
			return -EINVAL;
		} else if (info->if_mode == DCAM_CAP_IF_CSI2) {
			/*MIPI CAP, support 1/2 bining*/
			pr_info("Need Binning.\n");
			tempw = path->in_rect.w;
			tempw = tempw >> 1;
			if (tempw > DCAM_ISP_LINE_BUF_LENGTH) {
				pr_err("width out of ISP linebuffer,%d %d.\n",
					tempw,
					DCAM_ISP_LINE_BUF_LENGTH);
				return -EINVAL;
			}
			info->img_deci.x_factor = 1;
			f->need_binning = 1;
			DCAM_TRACE("x_factor, %d.\n", info->img_deci.x_factor);
			path->in_size.w = path->in_size.w >> 1;
			path->in_rect.x = path->in_rect.x >> 1;
			path->in_rect.w = path->in_rect.w >> 1;
			path->in_rect.w = path->in_rect.w & (~3);
		}
	}

	return 0;
}

static int sprd_img_check_scaling(struct sprd_img_format *f,
				 struct dcam_info *info,
				 struct dcam_path_spec *path,
				 uint32_t line_buf_size)
{
	uint32_t maxw, maxh, tempw, temph;
	uint32_t need_recal = 0;

	tempw = path->in_rect.w;
	temph = path->in_rect.h;

	/* no need to scale */
	if (tempw == f->width && temph == f->height)
		return 0;

	/* scaling needed */
	switch (info->sn_mode) {
	case DCAM_CAP_MODE_RAWRGB:
		/*scaling needed*/
		maxw = f->width * DCAM_SC_COEFF_DOWN_MAX;
		maxw = maxw * (1 << DCAM_PATH_DECI_FAC_MAX);
		maxh = f->height * DCAM_SC_COEFF_DOWN_MAX;
		maxh = maxh * (1 << DCAM_PATH_DECI_FAC_MAX);
		if (unlikely(tempw > maxw || temph > maxh)) {
			/*out of scaling capbility*/
			pr_err("size too small.\n");
			return -EINVAL;
		}

		if (unlikely(f->width > line_buf_size)) {
			/*out of scaling capbility*/
			pr_err("width more than %d.\n", f->width);
			return -EINVAL;
		}

		maxw = tempw * DCAM_SC_COEFF_UP_MAX;
		maxh = temph * DCAM_SC_COEFF_UP_MAX;
		if (unlikely(f->width > maxw || f->height > maxh)) {
			/*out of scaling capbility*/
			pr_err("size too large.\n");
			return -EINVAL;
		}

		break;

	case DCAM_CAP_MODE_YUV:
		/*scaling needed*/
		if (unlikely(f->width > line_buf_size)) {
			/*out of scaling capbility*/
			pr_err("output width %d can't more than %d.\n",
				f->width,
				line_buf_size);
			return -EINVAL;
		}

		/* To check whether the output size is too lager*/
		maxw = tempw * DCAM_SC_COEFF_UP_MAX;
		maxh = temph * DCAM_SC_COEFF_UP_MAX;
		if (f->width > maxw || f->height > maxh) {
			/*out of scaling capbility*/
			pr_err("output size is too large, %d %d.\n",
				f->width,
				f->height);
			return -EINVAL;
		}

		/* To check whether the output size is too small*/
		maxw = f->width * DCAM_SC_COEFF_DOWN_MAX;
		if (unlikely(tempw > maxw)) {
			path->img_deci.x_factor =
				sprd_img_get_deci_factor(tempw, maxw);
			if (path->img_deci.x_factor >
					DCAM_PATH_DECI_FAC_MAX) {
				pr_err("outputsize too small, %d %d.\n",
					f->width, f->height);
				return -EINVAL;
			}
		}

		maxh = f->height * DCAM_SC_COEFF_DOWN_MAX;
		if (unlikely(temph > maxh)) {
			path->img_deci.y_factor =
				sprd_img_get_deci_factor(temph, maxh);
			if (path->img_deci.y_factor >
					DCAM_PATH_DECI_FAC_MAX) {
				pr_err("outputsize too small, %d %d.\n",
					f->width, f->height);
				return -EINVAL;
			}
		}

		if (path->img_deci.x_factor) {
			tempw           = path->in_rect.w >> 1;
			need_recal      = 1;
		}

		if (path->img_deci.y_factor) {
			temph           = path->in_rect.h >> 1;
			need_recal      = 1;
		}

		if (need_recal && (tempw != f->width || temph != f->height)) {
			/*scaling needed*/
			if (unlikely(f->width > line_buf_size)) {
				/*out of scaling capbility*/
				pr_err("output width %d cant more than %d.\n",
					f->width,
					line_buf_size);
				return -EINVAL;
			}
		}

		break;
	default:
		break;
	}

	return 0;
}

static int sprd_img_check_path_cap(uint32_t fourcc,
				  struct sprd_img_format *f,
				  struct dcam_info   *info,
				  uint32_t path_id)
{
	uint32_t                 ret = 0;
	uint32_t                 tempw, temph;
	uint32_t                 depth_pixel = 0;
	struct dcam_path_spec    *path = &info->dcam_path[path_id];
	uint32_t                 line_buf_size;

	DCAM_TRACE("check format for path.\n");
	switch (path_id) {
	case DCAM_PATH1:
		line_buf_size = DCAM_PATH1_LINE_BUF_LENGTH;
		break;
	case DCAM_PATH2:
		line_buf_size = DCAM_PATH2_LINE_BUF_LENGTH;
		break;
	default:
		return -EINVAL;
	}

	path->frm_type = f->channel_id;
	path->is_from_isp = f->need_isp;
	path->rot_mode = f->reserved[0];
	path->end_sel.y_endian = DCAM_ENDIAN_LITTLE;
	path->end_sel.uv_endian = DCAM_ENDIAN_LITTLE;
	path->is_work = 0;
	path->pixel_depth = 0;
	path->img_deci.x_factor = 0;
	path->img_deci.y_factor = 0;
	tempw = path->in_rect.w;
	temph = path->in_rect.h;
	info->img_deci.x_factor = 0;
	f->need_binning = 0;
	DCAM_TRACE("sprd_img_check_path1_cap  rot_mode:%d\n", path->rot_mode);
	/* app should fill in this field(fmt.pix.priv) to set the base index of
	 * frame buffer, and lately this field will return the flag whether ISP
	 * is needed for this work path
	 */

	switch (fourcc) {
	case IMG_PIX_FMT_GREY:
	case IMG_PIX_FMT_JPEG:
	case IMG_PIX_FMT_YUYV:
	case IMG_PIX_FMT_YVYU:
	case IMG_PIX_FMT_UYVY:
	case IMG_PIX_FMT_VYUY:
		if (unlikely(f->width != tempw ||
			f->height != temph)) {
			/*need need scaling or triming*/
			pr_err("Cant scaling this img fmt src%d %d,dst%d %d\n",
					tempw,
					temph,
					f->width,
					f->height);
			return -EINVAL;
		}
		if (unlikely(info->sn_mode != DCAM_CAP_MODE_RAWRGB &&
			fourcc == IMG_PIX_FMT_GREY)) {
			/* the output of sensor is not RawRGB
			 * which is needed by app
			 */
			pr_err("It's not RawRGB sensor.\n");
			return -EINVAL;
		}
		if (fourcc == IMG_PIX_FMT_GREY) {
			if (info->if_mode == DCAM_CAP_IF_CSI2 &&
			    info->is_loose == 0) {
				depth_pixel = 10;
			} else {
				depth_pixel = 16;
			}
			DCAM_TRACE("RawRGB sensor, %d %d.\n",
				    info->is_loose, depth_pixel);
		} else if (fourcc == IMG_PIX_FMT_JPEG) {
			depth_pixel = 8;
		} else {
			depth_pixel = 16;
		}

		if (fourcc == IMG_PIX_FMT_GREY) {
			path->out_fmt = DCAM_RAWRGB;
			path->end_sel.y_endian = DCAM_ENDIAN_BIG;
		} else {
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
				/*no ISP ,only RawRGB data can be sampled*/
				pr_err("format 0x%x can't be supported\n",
					fourcc);
				return -EINVAL;

			}
		} else if (info->sn_mode == DCAM_CAP_MODE_YUV) {
			/* check scaling */
			ret = sprd_img_check_scaling(f, info, path,
						     line_buf_size);
			if (ret)
				return ret;
		}

		depth_pixel = sprd_img_endian_sel(fourcc, path);
		break;
	default:
		pr_err("unsupported image format for path2 0x%x.\n",
			fourcc);
		return -EINVAL;
	}

	path->fourcc = fourcc;
	path->pixel_depth = depth_pixel;
	f->bytesperline = (f->width * depth_pixel) >> 3;
	path->out_size.w = f->width;
	path->out_size.h = f->height;
	path->is_work = 1;
	return 0;

}

static inline int sprd_img_check_path1_cap(unsigned int fourcc,
					   struct sprd_img_format *f,
					   struct dcam_info *info)
{
	return sprd_img_check_path_cap(fourcc, f, info, DCAM_PATH1);
}

static inline int sprd_img_check_path2_cap(unsigned int fourcc,
					   struct sprd_img_format *f,
					   struct dcam_info *info)
{
	return sprd_img_check_path_cap(fourcc, f, info, DCAM_PATH2);

}

static int sprd_img_check_path0_cap(uint32_t fourcc,
					struct sprd_img_format *f,
					struct dcam_info   *info)
{
	struct dcam_path_spec    *path = &info->dcam_path[DCAM_PATH0];

	DCAM_TRACE("check format for path0.\n");

	path->is_from_isp = f->need_isp;

	path->frm_type = f->channel_id;
	path->is_work = 0;

	switch (fourcc) {
	case IMG_PIX_FMT_GREY:
		/* 0 - word/packet, 1 - half word*/
		/*path->out_fmt = info->is_loose;*/
		path->out_fmt = DCAM_RAWRGB;
		path->end_sel.y_endian = DCAM_ENDIAN_BIG;
		break;

	case IMG_PIX_FMT_YUV420:
	case IMG_PIX_FMT_YVU420:
		path->out_fmt = DCAM_YUV420_3FRAME;
		path->end_sel.y_endian = DCAM_ENDIAN_LITTLE;
		break;

	case IMG_PIX_FMT_YUV422P:
		pr_info("IMG_PIX_FMT_YUYV=.\n");
		path->out_fmt = DCAM_YUV422;
		path->end_sel.y_endian = DCAM_ENDIAN_LITTLE;
		break;

	case IMG_PIX_FMT_NV21:
		path->out_fmt = DCAM_YUV420;
		path->end_sel.y_endian = DCAM_ENDIAN_LITTLE;
		path->end_sel.uv_endian = DCAM_ENDIAN_HALFBIG;
		pr_info("path0 uv_endian=%d.\n", path->end_sel.uv_endian);
		break;

	case IMG_PIX_FMT_NV12:
		path->out_fmt = DCAM_YUV420;
		path->end_sel.y_endian = DCAM_ENDIAN_LITTLE;
		path->end_sel.uv_endian = DCAM_ENDIAN_LITTLE;
		pr_info("path0 uv_endian=%d.\n", path->end_sel.uv_endian);
		break;

	default:
		pr_info("unsupported image format for path0 0x%x.\n",
			fourcc);
		return -EINVAL;
	}

	path->fourcc = fourcc;

	DCAM_TRACE("check format for path0:out_fmt=%d,is_loose=%d.\n",
		    path->out_fmt, info->is_loose);
	path->out_size.w = f->width;
	path->out_size.h = f->height;

	path->is_work = 1;

	return 0;
}

static int sprd_img_cap_cfg(struct dcam_info *info)
{
	int                      ret = DCAM_RTN_SUCCESS;
	uint32_t                 param = 0;

	if (info == NULL)
		return -EINVAL;

	ret = dcam_cap_cfg(DCAM_CAP_SYNC_POL, &info->sync_pol);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	if ((info->dcam_path[DCAM_PATH0].is_work
		&& info->dcam_path[DCAM_PATH0].is_from_isp)
		|| (info->dcam_path[DCAM_PATH1].is_work
		&& info->dcam_path[DCAM_PATH1].is_from_isp)
		|| (info->dcam_path[DCAM_PATH2].is_work
		&& info->dcam_path[DCAM_PATH2].is_from_isp)) {
		param = 1;
	} else if (!info->dcam_path[DCAM_PATH0].is_work
		&& !info->dcam_path[DCAM_PATH1].is_work
		&& !info->dcam_path[DCAM_PATH2].is_work
		&& (info->sn_mode == DCAM_CAP_MODE_RAWRGB)) {
		param = 1;
	} else {
		param = 0;
	}

	ret = dcam_cap_cfg(DCAM_CAP_TO_ISP, &param);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = dcam_cap_cfg(DCAM_CAP_YUV_TYPE, &info->yuv_ptn);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = dcam_cap_cfg(DCAM_CAP_DATA_BITS, &info->data_bits);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	if (info->sn_mode == DCAM_CAP_MODE_RAWRGB
		&& info->if_mode == DCAM_CAP_IF_CSI2) {
		ret = dcam_cap_cfg(DCAM_CAP_DATA_PACKET, &info->is_loose);
		if (unlikely(ret)) {
			pr_err("%s err, code %d", __func__, ret);
			goto exit;
		}
	}

	ret = dcam_cap_cfg(DCAM_CAP_DATA_TYPE, &info->sn_mode);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = dcam_cap_cfg(DCAM_CAP_DATA_MODE, &info->sn_mode);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = dcam_cap_cfg(DCAM_CAP_FRM_DECI, &info->frm_deci);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = dcam_cap_cfg(DCAM_CAP_INPUT_RECT, &info->cap_in_rect);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	if (info->cap_in_rect.w <= ISP_LOW_CLOCK_WIDTH)
		switch_isp_clk(0);
	else
		switch_isp_clk(1);

	ret = dcam_cap_cfg(DCAM_CAP_FRM_COUNT_CLR, NULL);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = dcam_cap_cfg(DCAM_CAP_PRE_SKIP_CNT, &info->skip_number);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = dcam_cap_cfg(DCAM_CAP_SAMPLE_MODE, &info->capture_mode);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = dcam_cap_cfg(DCAM_CAP_ZOOM_MODE, &info->is_smooth_zoom);

	ret = dcam_cap_cfg(DCAM_CAP_IMAGE_XY_DECI, &info->img_deci);

exit:
	return ret;
}

static int sprd_img_buf_queue_init(struct dcam_img_buf_queue *queue)
{
	if (queue == NULL)
		return -EINVAL;

	pr_debug("sprd_img_buf_queue_init.\n");
	memset(queue, 0, sizeof(*queue));
	queue->write = &queue->buf_addr[0];
	queue->read  = &queue->buf_addr[0];

	return 0;
}

static int sprd_img_buf_queue_write(struct dcam_img_buf_queue *queue,
				struct dcam_img_buf_addr *buf_addr)
{
	struct dcam_img_buf_addr         *ori_node;

	if (queue == NULL || buf_addr == NULL)
		return -EINVAL;

	ori_node = queue->write;
	DCAM_TRACE("sprd_img_buf_queue_write.\n");
	*queue->write++ = *buf_addr;
	queue->wcnt++;
	if (queue->write > &queue->buf_addr[DCAM_FRM_CNT_MAX-1])
		queue->write = &queue->buf_addr[0];

	if (queue->write == queue->read) {
		queue->write = ori_node;
		pr_err("warning, queue is full, cannot write %d %d.",
			queue->wcnt, queue->rcnt);
		return -EINVAL;
	}

	return 0;
}

static int sprd_img_queue_write(struct dcam_queue *queue,
				struct dcam_node *node)
{
	struct dcam_node         *ori_node;

	if (queue == NULL || node == NULL ||
	    queue->read == NULL || queue->write == NULL)
		return -EINVAL;

	ori_node = queue->write;
	queue->wcnt++;
	DCAM_TRACE("sprd_img_queue_write.\n");
	*queue->write++ = *node;
	if (queue->write > &queue->node[DCAM_QUEUE_LENGTH-1])
		queue->write = &queue->node[0];

	if (queue->write == queue->read) {
		if (node->irq_flag != IMG_TX_STOP)
			queue->write = ori_node;
		else
			queue->read = ori_node;

		if (node->irq_type == CAMERA_IRQ_IMG
			|| node->irq_flag == IMG_TX_STOP
			|| node->irq_flag == IMG_TX_ERR) {
			pr_err("q full, flag 0x%x path %d index 0x%x wr %d/%d\n",
				node->irq_flag, node->f_type, node->index,
				queue->wcnt, queue->rcnt);
		} else {
			pr_err("q full, isp irq %d, property %d, flag 0x%x, wr %d/%d\n",
				node->irq_type, node->irq_property,
				node->irq_flag, queue->wcnt, queue->rcnt);
			if (node->irq_property == IRQ_DCAM_SOF)
				return -EINVAL;
		}
	}

	return 0;
}

static int sprd_img_queue_init(struct dcam_queue *queue)
{
	int ret = DCAM_RTN_SUCCESS;

	if (queue == NULL) {
		pr_err("invalid queue!\n");
		return -EINVAL;
	}

	pr_debug("sprd_img_queue_init.\n");
	memset(&queue->node[0], 0,
		DCAM_QUEUE_LENGTH * sizeof(struct dcam_node));
	queue->write = &queue->node[0];
	queue->read  = &queue->node[0];
	queue->wcnt = 0;
	queue->rcnt = 0;

	return ret;
}

static int sprd_img_queue_read(struct dcam_queue *queue, struct dcam_node *node)
{
	int                      ret = DCAM_RTN_SUCCESS;
	int                      flag = 0;

	if (queue == NULL || node == NULL ||
	    queue->read == NULL || queue->write == NULL)
		return -EINVAL;

	DCAM_TRACE("sprd_img_queue_read.\n");

	if (queue->read != queue->write) {
		flag = 1;
		*node = *queue->read;
		if (node->irq_flag == IMG_TX_STOP)
			pr_err("DCAM IMG_TX_STOP thr.\n");
		queue->read->yaddr = 0;
		queue->read->yaddr_vir = 0;
		queue->read++;
		queue->rcnt++;
		if (queue->read > &queue->node[DCAM_QUEUE_LENGTH-1])
			queue->read = &queue->node[0];
	}
	if (!flag)
		ret = EAGAIN;

	DCAM_TRACE("sprd_img_queue_read type %d.\n", node->f_type);
	return ret;
}

static int sprd_img_queue_disable(struct dcam_queue *queue, uint32_t channel_id)
{
	struct dcam_node            *cur_node;

	if (queue == NULL)
		return -EINVAL;

	cur_node = queue->read;
	while (cur_node != queue->write) {
		if (channel_id == cur_node->f_type)
			cur_node->invalid_flag = 1;

		if (cur_node >= &queue->node[DCAM_QUEUE_LENGTH-1])
			cur_node = &queue->node[0];
		else
			cur_node++;
	}


	return 0;
}

static int sprd_img_tx_done(struct camera_frame *frame, void *param)
{
	int                      ret = DCAM_RTN_SUCCESS;
	struct dcam_dev         *dev = (struct dcam_dev *)param;
	struct dcam_path_spec   *path = NULL;
	struct dcam_node         node;

	if (frame == NULL || param == NULL)
		return -EINVAL;
	/*
	* In case of isp simulation mode, stream_on
	* will not be called, yet isp is in mode
	* DDR_in_DDR_out, so we still have to handle
	* aem done int and store done int
	*/
	if (atomic_read(&dev->stream_on) == 0
			&& (frame->irq_property != IRQ_RAW_CAP_DONE
			&& frame->irq_property != IRQ_AEM_STATIS))
		return -EINVAL;

	atomic_set(&dev->run_flag, 1);
	memset((void *)&node, 0, sizeof(struct dcam_node));
	img_get_timestamp(&node.time);
	DCAM_TRACE("time, %ld %ld, %d.\n",
		(unsigned long)node.time.tv_sec,
		(unsigned long)node.time.tv_usec, frame->irq_type);
	if (frame->irq_type == CAMERA_IRQ_IMG) {
		path = &dev->dcam_cxt.dcam_path[frame->type];

		if (path->status == PATH_IDLE) {
			pr_info("DCAM: path id %d idle\n", frame->type);
			return ret;
		}

		node.irq_flag = IMG_TX_DONE;
		node.irq_type = frame->irq_type;
		node.irq_property = IRQ_MAX_DONE;
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
		node.reserved = frame->zsl_private;
		memcpy(node.mfd, frame->pfinfo.mfd, sizeof(unsigned int) * 3);
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
		memcpy(node.mfd, frame->pfinfo.mfd, sizeof(unsigned int) * 3);
	} else if (frame->irq_type == CAMERA_IRQ_DONE) {
		node.irq_flag = IMG_TX_DONE;
		node.irq_type = frame->irq_type;
		node.irq_property = frame->irq_property;
		node.frame_id = frame->frame_id;
	} else {
		pr_err("Not support irq_type %d\n", frame->irq_type);
		return -EINVAL;
	}

	DCAM_TRACE("flag 0x%x type 0x%x index 0x%x.\n",
		node.irq_flag, node.f_type, node.index);


	if (dev->dcam_cxt.after_af && frame->type == DCAM_PATH1) {
		ret = sprd_img_discard_frame(frame, param);
		if (ret == DCAM_RTN_SUCCESS) {
			dev->dcam_cxt.after_af = 0;
			node.irq_flag = IMG_CANCELED_BUF;
		}
	}

	ret = sprd_img_queue_write(&dev->queue, &node);

	if (ret)
		return ret;

	complete(&dev->irq_com);
	return ret;
}

static int sprd_img_tx_error(struct camera_frame *frame, void *param)
{
	int                      ret = DCAM_RTN_SUCCESS;
	struct dcam_dev          *dev = (struct dcam_dev *)param;
	struct dcam_node         node;

	if (param == NULL || atomic_read(&dev->stream_on) == 0)
		return -EINVAL;

	memset((void *)&node, 0, sizeof(struct dcam_node));
	atomic_set(&dev->run_flag, 1);/*to avoid time out processing*/
	node.irq_flag = IMG_TX_ERR;
	if (frame != NULL) {
		node.f_type   = frame->type;
		node.index    = frame->fid;
		node.height   = frame->height;
		node.yaddr    = frame->yaddr;
		node.uaddr    = frame->uaddr;
		node.vaddr    = frame->vaddr;
		node.yaddr_vir = frame->yaddr_vir;
		node.uaddr_vir = frame->uaddr_vir;
		node.vaddr_vir = frame->vaddr_vir;
		node.reserved = frame->zsl_private;
	}
	ret = sprd_img_queue_write(&dev->queue, &node);
	if (ret)
		return ret;

	complete(&dev->irq_com);

	pr_info("tx_error.\n");
	/*mm_clk_register_trace();*/
	return ret;
}

static int sprd_img_tx_stop(void *param)
{
	int                      ret = DCAM_RTN_SUCCESS;
	struct dcam_dev          *dev = (struct dcam_dev *)param;
	struct dcam_node         node;

	ret = sprd_img_queue_init(&dev->queue);
	if (unlikely(ret != 0))
		pr_err("Failed to init queue STOP.\n");

	memset((void *)&node, 0, sizeof(struct dcam_node));
	node.irq_flag = IMG_TX_STOP;
	ret = sprd_img_queue_write(&dev->queue, &node);
	if (ret)
		return ret;

	complete(&dev->irq_com);
	pr_info("sprd_img_tx_stop.\n");
	return ret;
}

static int sprd_img_start_zoom(struct camera_frame *frame, void *param)
{
	struct dcam_dev     *dev = (struct dcam_dev *)param;

	if (dev == NULL) {
		DCAM_TRACE("sprd_img_start_zoom, dev is NULL.\n");
		return -1;
	}
	DCAM_TRACE("start zoom level %d.\n", dev->zoom_level);
	if (dev->zoom_level <= DCAM_ZOOM_LEVEL_MAX &&
		dev->dcam_cxt.is_smooth_zoom) {
		/*up(&dev->zoom_thread_sem);*/
		complete(&dev->zoom_thread_com);
	} else {
		dcam_stop_sc_coeff();
	}

	return 0;
}

static int sprd_img_dcam_reg_isr(struct dcam_dev *param)
{
	dcam_reg_isr(DCAM_PATH0_DONE,   sprd_img_tx_done,       param);
	dcam_reg_isr(DCAM_PATH1_DONE,   sprd_img_tx_done,       param);
	dcam_reg_isr(DCAM_PATH0_OV,     sprd_img_tx_error,      param);
	dcam_reg_isr(DCAM_PATH1_OV,     sprd_img_tx_error,      param);
	dcam_reg_isr(DCAM_ISP_OV,       sprd_img_tx_error,      param);
	dcam_reg_isr(DCAM_MIPI_OV,      sprd_img_tx_error,      param);
	dcam_reg_isr(DCAM_SN_LINE_ERR,  sprd_img_tx_error,      param);
	dcam_reg_isr(DCAM_SN_FRAME_ERR, sprd_img_tx_error,      param);
	dcam_reg_isr(DCAM_SN_EOF,       sprd_img_start_flash,   param);
	dcam_reg_isr(DCAM_PATH1_SOF,    sprd_img_start_zoom,    param);

	return 0;
}

static int sprd_img_dcam_unreg_isr(struct dcam_dev *param)
{
	dcam_reg_isr(DCAM_PATH0_DONE,   NULL, param);
	dcam_reg_isr(DCAM_PATH1_DONE,   NULL, param);
	dcam_reg_isr(DCAM_PATH0_OV,     NULL, param);
	dcam_reg_isr(DCAM_PATH1_OV,     NULL, param);
	dcam_reg_isr(DCAM_ISP_OV,       NULL, param);
	dcam_reg_isr(DCAM_MIPI_OV,      NULL, param);
	dcam_reg_isr(DCAM_SN_LINE_ERR,  NULL, param);
	dcam_reg_isr(DCAM_SN_FRAME_ERR, NULL, param);
	dcam_reg_isr(DCAM_SN_EOF,       NULL,   param);
	dcam_reg_isr(DCAM_PATH1_SOF,    NULL,    param);

	return 0;
}

static int sprd_img_reg_path2_isr(struct dcam_dev *param)
{
	dcam_reg_isr(DCAM_PATH2_DONE,   sprd_img_tx_done,  param);
	dcam_reg_isr(DCAM_PATH2_OV,     sprd_img_tx_error, param);

	return 0;
}

static int sprd_img_unreg_path2_isr(struct dcam_dev *param)
{
	dcam_reg_isr(DCAM_PATH2_DONE,   NULL,  NULL);
	dcam_reg_isr(DCAM_PATH2_OV,     NULL,  NULL);

	return 0;
}

static int sprd_img_isp_reg_isr(struct dcam_dev *dev)
{
	if (!dev) {
		pr_err("Input dev ptr is NULL\n");
		return -EFAULT;
	}

	sprd_isp_reg_isr(ISP_AEM_DONE, sprd_img_tx_done, dev);
	sprd_isp_reg_isr(ISP_AFL_DONE, sprd_img_tx_done, dev);
	sprd_isp_reg_isr(ISP_AFM_DONE, sprd_img_tx_done, dev);
	sprd_isp_reg_isr(ISP_BINNING_DONE, sprd_img_tx_done, dev);
	sprd_isp_reg_isr(ISP_HIST_DONE, sprd_img_tx_done, dev);
	sprd_isp_reg_isr(ISP_DCAM_SOF, sprd_img_tx_done, dev);
	sprd_isp_reg_isr(ISP_STORE_DONE, sprd_img_tx_done, dev);

	return 0;
}

static int sprd_img_isp_unreg_isr(struct dcam_dev *dev)
{
	if (!dev) {
		pr_err("Input dev ptr is NULL\n");
		return -EFAULT;
	}

	sprd_isp_reg_isr(ISP_AEM_DONE, NULL, dev);
	sprd_isp_reg_isr(ISP_AFL_DONE, NULL, dev);
	sprd_isp_reg_isr(ISP_AFM_DONE, NULL, dev);
	sprd_isp_reg_isr(ISP_BINNING_DONE, NULL, dev);
	sprd_isp_reg_isr(ISP_DCAM_SOF, NULL, dev);
	sprd_isp_reg_isr(ISP_STORE_DONE, NULL, dev);

	return 0;
}

static int sprd_img_path_cfg_output_addr(path_cfg_func path_cfg,
					struct dcam_path_spec *path_spec)
{
	int                         ret = DCAM_RTN_SUCCESS;
	struct dcam_img_buf_addr   *cur_node;
	struct dcam_img_buf_queue  *queue;
	struct dcam_addr            frm_addr;

	if (path_cfg == NULL || path_spec == NULL)
		return -EINVAL;

	queue = &path_spec->buf_queue;

	for (cur_node = queue->read; cur_node != queue->write; cur_node++) {
		if (cur_node > &queue->buf_addr[DCAM_FRM_CNT_MAX-1])
			cur_node = &queue->buf_addr[0];

		frm_addr.yaddr = cur_node->frm_addr.yaddr;
		frm_addr.uaddr = cur_node->frm_addr.uaddr;
		frm_addr.vaddr = cur_node->frm_addr.vaddr;
		frm_addr.yaddr_vir = cur_node->frm_addr_vir.yaddr;
		frm_addr.uaddr_vir = cur_node->frm_addr_vir.uaddr;
		frm_addr.vaddr_vir = cur_node->frm_addr_vir.vaddr;
		frm_addr.zsl_private = cur_node->frm_addr_vir.zsl_private;
		frm_addr.mfd_y = cur_node->frm_addr.mfd_y;
		frm_addr.mfd_u = cur_node->frm_addr.mfd_u;
		frm_addr.mfd_v = cur_node->frm_addr.mfd_v;
		ret = path_cfg(DCAM_PATH_OUTPUT_ADDR, &frm_addr);
		if (unlikely(ret)) {
			pr_err("%s err, code %d", __func__, ret);
			goto exit;
	}
	}

exit:
	return ret;
}

static int sprd_img_path0_cfg(path_cfg_func path_cfg,
				struct dcam_path_spec *path_spec)
{
	int                      ret = DCAM_RTN_SUCCESS;
	uint32_t                 param;

	if (path_cfg == NULL || path_spec == NULL)
		return -EINVAL;

	if (path_spec->is_from_isp)
		param = 1;
	else
		param = 0;

	ret = path_cfg(DCAM_PATH_SRC_SEL, &param);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = path_cfg(DCAM_PATH_INPUT_SIZE, &path_spec->in_size);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = path_cfg(DCAM_PATH_INPUT_RECT, &path_spec->in_rect);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = path_cfg(DCAM_PATH_OUTPUT_SIZE, &path_spec->out_size);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}
	ret = path_cfg(DCAM_PATH_OUTPUT_FORMAT, &path_spec->out_fmt);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = path_cfg(DCAM_PATH_FRAME_BASE_ID, &path_spec->frm_id_base);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = path_cfg(DCAM_PATH_FRAME_TYPE, &path_spec->frm_type);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = path_cfg(DCAM_PATH_DATA_ENDIAN, &path_spec->end_sel);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = path_cfg(DCAM_PATH_FRM_DECI, &path_spec->path_frm_deci);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	if (path_spec->pdaf_ctrl.mode) {
		ret = path_cfg(DCAM_PDAF_CONTROL, &path_spec->pdaf_ctrl);
		if (unlikely(ret)) {
			pr_err("%s err, code %d", __func__, ret);
			goto exit;
		}
	}

	ret = sprd_img_path_cfg_output_addr(path_cfg, path_spec);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = path_cfg(DCAM_PATH_OUTPUT_RESERVED_ADDR,
			&path_spec->frm_reserved_addr);

	param = 1;
	ret = path_cfg(DCAM_PATH_ENABLE, &param);

exit:
	return ret;
}

static int sprd_img_path_cfg(path_cfg_func path_cfg,
			    struct dcam_path_spec *path_spec)
{
	int                      ret = DCAM_RTN_SUCCESS;
	uint32_t                 param;

	if (path_cfg == NULL || path_spec == NULL)
		return -EINVAL;

	if (path_spec->is_from_isp)
		param = 1;
	else
		param = 0;

	ret = path_cfg(DCAM_PATH_SRC_SEL, &param);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = path_cfg(DCAM_PATH_ROT_MODE, &path_spec->rot_mode);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = path_cfg(DCAM_PATH_INPUT_SIZE, &path_spec->in_size);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = path_cfg(DCAM_PATH_INPUT_RECT, &path_spec->in_rect);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = path_cfg(DCAM_PATH_OUTPUT_SIZE, &path_spec->out_size);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = path_cfg(DCAM_PATH_OUTPUT_FORMAT, &path_spec->out_fmt);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = path_cfg(DCAM_PATH_FRAME_BASE_ID, &path_spec->frm_id_base);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = path_cfg(DCAM_PATH_FRAME_TYPE, &path_spec->frm_type);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = path_cfg(DCAM_PATH_DATA_ENDIAN, &path_spec->end_sel);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = path_cfg(DCAM_PATH_FRM_DECI, &path_spec->path_frm_deci);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = sprd_img_path_cfg_output_addr(path_cfg, path_spec);
	if (unlikely(ret)) {
		pr_err("%s err, code %d", __func__, ret);
		goto exit;
	}

	ret = path_cfg(DCAM_PATH_OUTPUT_RESERVED_ADDR,
			&path_spec->frm_reserved_addr);

	ret = path_cfg(DCAM_PATH_SHRINK, &path_spec->regular_desc.regular_mode);

	param = 1;
	ret = path_cfg(DCAM_PATH_ENABLE, &param);

exit:
	return ret;
}

static int sprd_img_local_deinit(struct dcam_dev *dev)
{
	int ret = 0;
	struct dcam_path_spec *path = NULL;

	if (unlikely(dev == NULL)) {
		pr_err("deinit error: dev is NULL!\n");
		return -EINVAL;
	}

	path = &dev->dcam_cxt.dcam_path[DCAM_PATH0];
	if (unlikely(path == NULL)) {
		pr_err("deinit error path0 is NULL\n");
		return -EINVAL;
	}
	pr_debug("path0 frm_cnt_act %d.\n", path->frm_cnt_act);
	path->is_work = 0;
	path->frm_cnt_act = 0;
	memset((void *)&path->frm_reserved_addr, 0, sizeof(struct dcam_addr));
	sprd_img_buf_queue_init(&path->buf_queue);

	path = &dev->dcam_cxt.dcam_path[DCAM_PATH1];
	if (unlikely(path == NULL)) {
		pr_err("deinit error path1 is NULL\n");
		return -EINVAL;
	}
	pr_debug("path1 frm_cnt_act %d.\n", path->frm_cnt_act);
	path->is_work = 0;
	path->frm_cnt_act = 0;
	memset((void *)&path->frm_reserved_addr, 0, sizeof(struct dcam_addr));
	sprd_img_buf_queue_init(&path->buf_queue);

	path = &dev->dcam_cxt.dcam_path[DCAM_PATH2];
	if (unlikely(path == NULL)) {
		pr_err("deinit error path2 is NULL\n");
		return -EINVAL;
	}
	pr_debug("path2 frm_cnt_act %d.\n", path->frm_cnt_act);
	path->is_work = 0;
	path->frm_cnt_act = 0;
	memset((void *)&path->frm_reserved_addr, 0, sizeof(struct dcam_addr));
	sprd_img_buf_queue_init(&path->buf_queue);

	ret = sprd_img_queue_init(&dev->queue);
	if (unlikely(ret != 0)) {
		pr_err("Failed to init img queue.\n");
		return -EINVAL;
	}

	return 0;
}

static int sprd_img_queue_enable(struct dcam_queue *queue, uint32_t channel_id)
{
	unsigned int                    i = 0;

	if (queue == NULL)
		return -EINVAL;

	for (i = 0 ; i < DCAM_QUEUE_LENGTH ; i++)
		queue->node[i].invalid_flag = 0;

	return 0;
}

static int sprd_img_get_path_index(uint32_t channel_id)
{
	int path_index;

	switch (channel_id) {
	case DCAM_PATH0:
		path_index = DCAM_PATH_IDX_0;
		break;
	case DCAM_PATH1:
		path_index = DCAM_PATH_IDX_1;
		break;
	case DCAM_PATH2:
		path_index = DCAM_PATH_IDX_2;
		break;
	default:
		path_index = DCAM_PATH_IDX_NONE;
		pr_err("get path index error, invalid channel_id=0x%x.\n",
			channel_id);
	}
	return path_index;
}

static int img_get_zoom_rect(struct camera_rect *src_rect,
			    struct camera_rect *dst_rect,
			    struct camera_rect *output_rect,
			    uint32_t zoom_level)
{
	uint32_t trim_width = 0;
	uint32_t trim_height = 0;
	uint32_t zoom_step_w = 0, zoom_step_h = 0;

	if (src_rect == NULL || dst_rect == NULL || output_rect == NULL) {
		pr_err("img_get_zoom_rect: %p, %p, %p.\n",
			src_rect, dst_rect, output_rect);
		return -EINVAL;
	}

	if (dst_rect->w == 0 || dst_rect->h == 0) {
		pr_err("img_get_zoom_rect: dst0x%x, 0x%x.\n",
			dst_rect->w, dst_rect->h);
		return -EINVAL;
	}

	if (src_rect->w > dst_rect->w && src_rect->h > dst_rect->h) {
		zoom_step_w = DCAM_ZOOM_STEP(src_rect->w, dst_rect->w);
		zoom_step_w &= ~1;
		zoom_step_w *= zoom_level;
		trim_width = src_rect->w - zoom_step_w;

		zoom_step_h = DCAM_ZOOM_STEP(src_rect->h, dst_rect->h);
		zoom_step_h &= ~1;
		zoom_step_h *= zoom_level;
		trim_height = src_rect->h - zoom_step_h;

		output_rect->x = src_rect->x +
				((src_rect->w - trim_width) >> 1);
		output_rect->y = src_rect->y +
				((src_rect->h - trim_height) >> 1);
	} else if (src_rect->w < dst_rect->w && src_rect->h < dst_rect->h) {
		zoom_step_w = DCAM_ZOOM_STEP(dst_rect->w, src_rect->w);
		zoom_step_w &= ~1;
		zoom_step_w *= zoom_level;
		trim_width = src_rect->w + zoom_step_w;

		zoom_step_h = DCAM_ZOOM_STEP(dst_rect->h, src_rect->h);
		zoom_step_h &= ~1;
		zoom_step_h *= zoom_level;
		trim_height = src_rect->h + zoom_step_h;

		output_rect->x = src_rect->x -
				((trim_width - src_rect->w) >> 1);
		output_rect->y = src_rect->y -
				((trim_height - src_rect->h) >> 1);
	} else {
		pr_err("img_get_zoom_rect: param error.\n");
		return -EINVAL;
	}

	output_rect->x = DCAM_WIDTH(output_rect->x);
	output_rect->y = DCAM_HEIGHT(output_rect->y);
	output_rect->w = DCAM_WIDTH(trim_width);
	output_rect->h = DCAM_HEIGHT(trim_height);
	DCAM_TRACE("zoom_level %d, trim rect, %d %d %d %d.\n",
		zoom_level,
		output_rect->x,
		output_rect->y,
		output_rect->w,
		output_rect->h);

	return 0;
}

static int sprd_img_zoom_thread_loop(void *arg)
{
	struct dcam_dev         *dev = (struct dcam_dev *)arg;
	int                      ret = DCAM_RTN_SUCCESS;
	struct camera_rect       zoom_rect = {0};
	struct dcam_path_spec   *path = NULL;
	enum dcam_path_index     path_index;

	if (dev == NULL) {
		pr_err("zoom_thread_loop, dev is NULL.\n");
		return -1;
	}
	while (1) {
		if (wait_for_completion_interruptible(
				&dev->zoom_thread_com) == 0) {

			DCAM_TRACE("zoom thread level %d.\n",
				dev->zoom_level);
			if (dev->is_zoom_thread_stop) {
				DCAM_TRACE("zoom_thread_loop stop.\n");
				break;
			}

			if (dev->zoom_level > DCAM_ZOOM_LEVEL_MAX)
				continue;

			mutex_lock(&dev->dcam_mutex);
			path = &dev->dcam_cxt.dcam_path[dev->channel_id];
			path_index = sprd_img_get_path_index(dev->channel_id);
			if (dev->zoom_level < DCAM_ZOOM_LEVEL_MAX) {
				ret = img_get_zoom_rect(&path->in_rect_backup,
							&path->in_rect,
							&zoom_rect,
							dev->zoom_level);
				if (!ret) {
					memcpy((void *)&path->in_rect_current,
						(void *)&zoom_rect,
						sizeof(struct camera_rect));
					dcam_update_path(path_index,
							&path->in_size,
							&zoom_rect,
							&path->out_size);
				}
			} else {
				dcam_update_path(path_index,
						&path->in_size,
						&path->in_rect,
						&path->out_size);
				memcpy((void *)&path->in_rect_backup,
					(void *)&path->in_rect,
					sizeof(struct camera_rect));
				memcpy((void *)&path->in_rect_current,
					(void *)&path->in_rect_backup,
					sizeof(struct camera_rect));
			}
			dev->zoom_level++;
			mutex_unlock(&dev->dcam_mutex);
			DCAM_TRACE("zoom thread level  %d  end.\n",
					dev->zoom_level);

		} else {
			pr_err("zoom int!");
			break;
		}
	}
	dev->is_zoom_thread_stop = 0;

	return 0;
}

static int sprd_img_create_zoom_thread(void *param)
{
	struct dcam_dev     *dev = (struct dcam_dev *)param;

	if (dev == NULL) {
		DCAM_TRACE("create_zoom_thread, dev is NULL.\n");
		return -1;
	}
	pr_info("create_zoom_thread E!.\n");

	dev->is_zoom_thread_stop = 0;
	dev->zoom_level = DCAM_ZOOM_LEVEL_MAX + 1;
	/*sema_init(&dev->zoom_thread_sem, 0);*/
	init_completion(&dev->zoom_thread_com);
	dev->zoom_thread = kthread_run(sprd_img_zoom_thread_loop, param,
			   "img_zoom_thread");
	if (IS_ERR(dev->zoom_thread)) {
		pr_err("create_zoom_thread error!\n");
		return -1;
	}
	return 0;
}

static int sprd_img_stop_zoom_thread(void *param)
{
	struct dcam_dev     *dev = (struct dcam_dev *)param;

	if (dev == NULL) {
		pr_err("stop_zoom_thread failed, dev is NULL.\n");
		return -1;
	}
	DCAM_TRACE("stop_zoom_thread E!\n");
	if (dev->zoom_thread) {
		dev->is_zoom_thread_stop = 1;
		/*up(&dev->zoom_thread_sem);*/
		complete(&dev->zoom_thread_com);
		if (dev->is_zoom_thread_stop != 0) {
			while (dev->is_zoom_thread_stop)
				usleep_range(1000, 1500);
		}
		dev->zoom_thread = NULL;
	}

	return 0;
}

static int sprd_img_update_video(struct file *file, uint32_t channel_id)
{
	struct dcam_dev          *dev = file->private_data;
	int                      ret = DCAM_RTN_SUCCESS;
	struct dcam_path_spec    *path = NULL;
/*	path_cfg_func            path_cfg;*/
	enum dcam_path_index     path_index;


	DCAM_TRACE("sprd_img_update_video, channel=%d.\n", channel_id);

	mutex_lock(&dev->dcam_mutex);

	path = &dev->dcam_cxt.dcam_path[channel_id];
	path_index = sprd_img_get_path_index(channel_id);
/*
*	if (channel_id == DCAM_PATH0) {
*		path_cfg = dcam_path0_cfg;
*	} else if (channel_id == DCAM_PATH1) {
*		path_cfg = dcam_path1_cfg;
*	}else if (channel_id == DCAM_PATH2) {
*		path_cfg = dcam_path2_cfg;
*	}
*/
	if (dev->dcam_cxt.is_smooth_zoom && DCAM_PATH1 == channel_id) {
		dev->zoom_level = 1;
		dev->channel_id = channel_id;
		if (path->in_rect_backup.w == 0 ||
		    path->in_rect_backup.h == 0) {
			path->in_rect_backup.x = 0;
			path->in_rect_backup.y = 0;
			path->in_rect_backup.w = path->in_size.w;
			path->in_rect_backup.h = path->in_size.h;
			memcpy((void *)&path->in_rect_current,
				(void *)&path->in_rect_backup,
				sizeof(struct camera_rect));
		} else {
			memcpy((void *)&path->in_rect_backup,
				(void *)&path->in_rect_current,
				sizeof(struct camera_rect));
		}

		DCAM_TRACE("in_size{%d %d}, in_rect{%d %d %d %d}\n",
			   path->in_size.w, path->in_size.h, path->in_rect.x,
			   path->in_rect.y, path->in_rect.w, path->in_rect.h);
		DCAM_TRACE("in_rect_backup{%d %d %d %d}, out_size{%d %d}\n",
			   path->in_rect_backup.x, path->in_rect_backup.y,
			   path->in_rect_backup.w, path->in_rect_backup.h,
			   path->out_size.w, path->out_size.h);
	} else {
		DCAM_TRACE("in_size{%d %d}, in_rect{%d %d %d %d}\n",
			   path->in_size.w, path->in_size.h, path->in_rect.x,
			   path->in_rect.y, path->in_rect.w, path->in_rect.h);
		DCAM_TRACE("in_rect_backup{%d %d %d %d}, out_size{%d %d}\n",
			   path->in_rect_backup.x, path->in_rect_backup.y,
			   path->in_rect_backup.w, path->in_rect_backup.h,
			   path->out_size.w, path->out_size.h);
		ret = dcam_update_path(path_index, &path->in_size,
				       &path->in_rect, &path->out_size);
	}

	mutex_unlock(&dev->dcam_mutex);
	DCAM_TRACE("update video 0x%x.\n", ret);
	if (ret)
		pr_err("Failed to update video 0x%x.\n", ret);

	return ret;
}

static int sprd_img_streampause(struct file *file, uint32_t channel_id,
				uint32_t reconfig_flag)
{
	struct dcam_dev          *dev = file->private_data;
	struct dcam_path_spec    *path = NULL;
	int                      ret = 0;
	enum dcam_path_index     path_index;

	pr_info("pause, channel %d ,recfg flag %d.\n", channel_id,
		reconfig_flag);

	path = &dev->dcam_cxt.dcam_path[channel_id];
	path_index = sprd_img_get_path_index(channel_id);

	if (path->status == PATH_RUN) {
		path->status = PATH_IDLE;
		ret = dcam_stop_path(path_index);
		if (ret)
			pr_err("%s err, code %d", __func__, ret);

		if ((reconfig_flag)/* && (DCAM_PATH2 == channel_id)*/) {
			/*path->is_work = 0;*/
			path->frm_cnt_act = 0;
			sprd_img_buf_queue_init(&path->buf_queue);
			sprd_img_queue_disable(&dev->queue, channel_id);
			ret = sprd_img_unreg_path2_isr(dev);
			if (ret)
				pr_err("%s err, code %d", __func__, ret);
		}

		if (dev->got_resizer && channel_id == DCAM_PATH2) {
			dcam_rel_resizer();
			dev->got_resizer = 0;
		}
		pr_info("pause, channel=%d done.\n", channel_id);
	} else {
		pr_info("pause, path %d not running, status=%d.\n",
			channel_id, path->status);
	}

	return ret;
}

static int sprd_img_streamresume(struct file *file, uint32_t channel_id)
{
	struct dcam_dev          *dev = NULL;
	struct dcam_path_spec    *path = NULL;
	enum dcam_path_index     path_index;
	path_cfg_func            path_cfg;
	int                      ret = 0;
	int                      on_flag = 1;

	DCAM_TRACE("resume, channel=%d.\n", channel_id);

	dev = file->private_data;
	if (!dev) {
		pr_err("dev is NULL.\n");
		return -EFAULT;
	}

	path = &dev->dcam_cxt.dcam_path[channel_id];
	path_index = sprd_img_get_path_index(channel_id);

	if (unlikely(atomic_read(&dev->stream_on) == 0)) {
		pr_err("resume stream not on.\n");
		ret = sprd_img_local_deinit(dev);
		if (unlikely(ret))
			pr_err("%s error", __func__);
		on_flag = 0;
	}
	if (on_flag && path->status == PATH_IDLE) {
		if (path->is_work) {
			if (channel_id == DCAM_PATH0) {
				path_cfg = dcam_path0_cfg;
			} else if (channel_id == DCAM_PATH1) {
				path_cfg = dcam_path1_cfg;
			} else if (channel_id == DCAM_PATH2) {
				if (dev->got_resizer == 0) {
				/* if not owned resiezer,try to get it*/
					dcam_get_resizer();
					dev->got_resizer = 1;
				}
				ret = sprd_img_reg_path2_isr(dev);
				if (ret)
					pr_err("%s err,code %d", __func__, ret);
				path_cfg = dcam_path2_cfg;
			} else {
				pr_err("resume, invalid channel_id=0x%x.\n",
					channel_id);
				return -EINVAL;
			}

			if (channel_id == DCAM_PATH0)
				ret = sprd_img_path0_cfg(path_cfg, path);
			else
				ret = sprd_img_path_cfg(path_cfg, path);
			if (unlikely(ret)) {
				pr_err("%s err, code %d", __func__, ret);
				goto exit;
			}
			sprd_img_queue_enable(&dev->queue, channel_id);
			path->status = PATH_RUN;
			ret = dcam_start_path(path_index);
			if (ret)
				pr_err("%s err,code %d", __func__, ret);
		} else {
			DCAM_TRACE("resume, path %d is_work %d, can't resume\n",
				   channel_id, path->is_work);
		}
	} else {
		DCAM_TRACE("resume, path %d status %d, can't resume\n",
			   channel_id, path->status);
	}
exit:
	if (ret) {
		DCAM_TRACE("fail resume, path %d, ret = 0x%x.\n",
			channel_id, ret);
	}
	return ret;
}

static void sprd_img_print_reg(void)
{
	uint32_t                *reg_buf = NULL;
	uint32_t                 reg_buf_len = 0x400;
	int                      ret;
	uint32_t                 print_len = 0, print_cnt = 0;

	reg_buf = vzalloc(reg_buf_len);
	if (reg_buf == NULL)
		return;

	ret = dcam_read_registers(reg_buf, &reg_buf_len);
	if (ret) {
		vfree(reg_buf);
		return;
	}

	/*mm_clk_register_trace();*/
	pr_info("dcam registers.\n");
	while (print_len < reg_buf_len) {
		pr_info("offset 0x%03x : 0x%08x, 0x%08x, 0x%08x, 0x%08x.\n",
			print_len,
			reg_buf[print_cnt],
			reg_buf[print_cnt+1],
			reg_buf[print_cnt+2],
			reg_buf[print_cnt+3]);
		print_cnt += 4;
		print_len += 16;
	}

	udelay(1);
	vfree(reg_buf);

	return;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static void sprd_timer_callback(struct timer_list *t)
{
	struct dcam_dev        *dev = from_timer(dev, t, dcam_timer);
#else
static void sprd_timer_callback(unsigned long data)
{
	struct dcam_dev        *dev = (struct dcam_dev *)data;
#endif
	struct dcam_node         node;
	int                      ret = 0;

	DCAM_TRACE("sprd_timer_callback.\n");

	if (dev == 0 || atomic_read(&dev->stream_on) == 0) {
		pr_err("timer cb error.\n");
		return;
	}

	if (atomic_read(&dev->run_flag) == 0) {
		pr_err("DCAM timeout.\n");
		memset(&node, 0, sizeof(struct dcam_node));
		node.irq_flag = IMG_TIMEOUT;
		node.invalid_flag = 0;
		ret = sprd_img_queue_write(&dev->queue, &node);
		if (ret)
			pr_err("timer cb write queue error.\n");
		complete(&dev->irq_com);
	}
}

static int sprd_init_timer(struct timer_list *dcam_timer, unsigned long data)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	timer_setup(dcam_timer, sprd_timer_callback, 0);
#else
	setup_timer(dcam_timer, sprd_timer_callback, data);
#endif
	return 0;
}

static int sprd_start_timer(struct timer_list *dcam_timer, uint32_t time_val)
{
	int                      ret = 0;

	DCAM_TRACE("starting timer %ld.\n", jiffies);
	ret = mod_timer(dcam_timer, jiffies + msecs_to_jiffies(time_val));
	if (ret)
		pr_err("Error in mod_timer %d.\n", ret);
	return ret;
}

static int sprd_stop_timer(struct timer_list *dcam_timer)
{
	pr_debug("stop timer.\n");
	del_timer_sync(dcam_timer);
	return 0;
}

static int sprd_init_handle(struct dcam_dev *dev)
{
	struct dcam_info         *info = &dev->dcam_cxt;
	struct dcam_path_spec    *path;
	uint32_t                 i = 0;

	if (info == NULL) {
		pr_err("init handle fail.\n");
		return -EINVAL;
	}
	info->set_flash.led0_ctrl = 0;
	info->set_flash.led1_ctrl = 0;
	info->set_flash.led0_status = FLASH_STATUS_MAX;
	info->set_flash.led1_status = FLASH_STATUS_MAX;
	info->set_flash.flash_index = 0;
	info->after_af = 0;
	for (i = 0; i < DCAM_PATH_MAX; i++) {
		path = &info->dcam_path[i];
		if (path == NULL) {
			pr_err("init path %d fail.\n", i);
			return -EINVAL;
		}
		memset((void *)path->frm_ptr,
			0,
			(uint32_t)(DCAM_FRM_CNT_MAX *
				sizeof(struct camera_frame *)));
		path->frm_cnt_act = 0;
		sprd_img_buf_queue_init(&path->buf_queue);
		path->status = PATH_IDLE;
	}
	atomic_set(&dev->stream_on, 0);
	dev->got_resizer = 0;
	return 0;
}

static int sprd_img_set_crop(struct file *file, struct sprd_img_parm *p)
{
	int ret = 0;
	int ratio = 0;
	struct dcam_info *info = NULL;
	struct dcam_dev *dev = NULL;
	struct camera_rect *input_rect;
	struct camera_size *input_size;

	dev = file->private_data;
	if (!dev) {
		ret = -EFAULT;
		pr_err("%s: dev is NULL.\n", __func__);
		goto exit;
	}

	info = &dev->dcam_cxt;

	if (!sprd_get_ver_id()) {
		if (p->crop_rect.x == 0 && p->crop_rect.y == 0
			&& p->channel_id != DCAM_PATH0) {
			ratio = p->crop_rect.w * 100 / p->crop_rect.h;
			if (ratio == 133) {
				p->crop_rect.x = 8;
				p->crop_rect.y = 6;
				p->crop_rect.w -= 16;
				p->crop_rect.h -= 12;
			} else if (ratio == 177) {
				p->crop_rect.x = 16;
				p->crop_rect.y = 9;
				p->crop_rect.w -= 32;
				p->crop_rect.h -= 18;
			} else {
				p->crop_rect.x = 2;
				p->crop_rect.y = 2;
				p->crop_rect.w -= 4;
				p->crop_rect.h -= 4;
			}
			pr_info("crop window %d %d %d %d.\n",
				p->crop_rect.x, p->crop_rect.y,
				p->crop_rect.w, p->crop_rect.h);
		}
	}

	if (unlikely(p->crop_rect.x + p->crop_rect.w >
		info->cap_in_size.w ||
		p->crop_rect.y + p->crop_rect.h >
		info->cap_in_size.h)) {
		ret = -EINVAL;
		goto exit;
	}

	DCAM_TRACE("SPRD_IMG_IO_SET_CROP, window %d %d %d %d.\n",
		p->crop_rect.x, p->crop_rect.y,
		p->crop_rect.w, p->crop_rect.h);

	switch (p->channel_id) {
	case DCAM_PATH0:
	case DCAM_PATH1:
	case DCAM_PATH2:
		input_size = &dev->dcam_cxt.dcam_path[p->channel_id].in_size;
		input_rect = &dev->dcam_cxt.dcam_path[p->channel_id].in_rect;
		break;
	default:
		pr_err("Wrong channel ID, %d .\n", p->channel_id);
		goto exit;
	}

	input_size->w = info->cap_out_size.w;
	input_size->h = info->cap_out_size.h;
	input_rect->x = p->crop_rect.x;
	input_rect->y = p->crop_rect.y;
	input_rect->w = p->crop_rect.w;
	input_rect->h = p->crop_rect.h;

	DCAM_TRACE("%s: Path %d, cap_rect %d %d %d %d, cap_out:%d %d.\n",
			__func__, p->channel_id,
			input_rect->x, input_rect->y,
			input_rect->w, input_rect->h,
			input_size->w, input_size->h);

exit:
	return ret;
}

static int sprd_img_set_sensor_if(struct file *file,
				 struct sprd_img_sensor_if *sensor_if)
{
	int ret = 0;
	struct dcam_info *info = NULL;
	struct dcam_dev *dev = NULL;

	dev = file->private_data;
	if (!dev) {
		ret = -EFAULT;
		pr_err("%s: dev is NULL.\n", __func__);
		goto exit;
	}

	if (sensor_if->res[0] != IF_OPEN)
		goto exit;

	info = &dev->dcam_cxt;
	info->if_mode     = sensor_if->if_type;
	info->sn_mode     = sensor_if->img_fmt;
	info->yuv_ptn     = sensor_if->img_ptn;
	info->frm_deci    = sensor_if->frm_deci;

	DCAM_TRACE("interface %d, mode %d frm_deci %d.\n",
		info->if_mode, info->sn_mode, info->frm_deci);

	if (info->if_mode == DCAM_CAP_IF_CCIR) {
		/* CCIR interface */
		info->sync_pol.vsync_pol = sensor_if->if_spec.ccir.v_sync_pol;
		info->sync_pol.hsync_pol = sensor_if->if_spec.ccir.h_sync_pol;
		info->sync_pol.pclk_pol  = sensor_if->if_spec.ccir.pclk_pol;
		info->data_bits          = 8;
		DCAM_TRACE("CCIR interface, vsync:%d,hsync:%d",
			info->sync_pol.vsync_pol,
			info->sync_pol.hsync_pol);
		DCAM_TRACE("pclk:%d, psrc:%d, bits:%d.\n",
			info->sync_pol.pclk_pol,
			info->sync_pol.pclk_src,
			info->data_bits);
	} else {
		info->sync_pol.need_href = sensor_if->if_spec.mipi.use_href;
		info->is_loose           = sensor_if->if_spec.mipi.is_loose;
		info->data_bits          = sensor_if->if_spec.mipi.bits_per_pxl;
		info->lane_num           = sensor_if->if_spec.mipi.lane_num;
		info->pclk               = sensor_if->if_spec.mipi.pclk;
		DCAM_TRACE("MIPI interface, ref %d is_loose %d",
			info->sync_pol.need_href,
			info->is_loose);

		DCAM_TRACE("bits %d lanes %d pclk %d.\n",
			info->data_bits,
			info->lane_num,
			info->pclk);
	}

exit:
	return ret;
}


static int sprd_img_set_frame_addr(struct file *file,
				   struct sprd_img_parm *p)
{
	int ret = 0;
	unsigned int i = 0;
	uint32_t                 path_cnt;
	struct dcam_info *info = NULL;
	struct dcam_dev *dev = NULL;
	struct dcam_path_spec    *path = NULL;
	path_cfg_func            path_cfg;

	dev = file->private_data;
	info = &dev->dcam_cxt;

	switch (p->channel_id) {
	case DCAM_PATH0:
		path = &info->dcam_path[DCAM_PATH0];
		path_cnt = DCAM_PATH_0_FRM_CNT_MAX;
		path_cfg = dcam_path0_cfg;
		break;
	case DCAM_PATH1:
		path = &info->dcam_path[DCAM_PATH1];
		path_cnt = DCAM_PATH_1_FRM_CNT_MAX;
		path_cfg = dcam_path1_cfg;
		break;
	case DCAM_PATH2:
		path = &info->dcam_path[DCAM_PATH2];
		path_cnt = DCAM_PATH_2_FRM_CNT_MAX;
		path_cfg = dcam_path2_cfg;
		break;
	default:
		pr_err("SET_FRAME_ADDR, path 0x%x.\n", p->channel_id);
		mutex_unlock(&dev->dcam_mutex);
		return -EINVAL;
	}


	DCAM_TRACE("SPRD_IMG_IO_SET_FRAME_ADDR, status %d, frm_cnt_act %d.\n",
		path->status, path->frm_cnt_act);

	if (unlikely(p->fd_array[0] == 0)) {
		pr_info("fail to get fd\n");
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
		/*pr_info("frame buf, reserved path%d, fd 0x%x\n",
		*	p->channel_id, p->fd_array[0]);
		*/
	} else {
		struct dcam_addr         frame_addr;
		struct dcam_img_buf_addr buf_addr;

		if (atomic_read(&dev->stream_on) == 1
			&& path->status == PATH_RUN
			&& p->buf_flag == IMG_BUF_FLAG_RUNNING) {

			for (i = 0; i < p->buffer_count; i++) {
				if (p->fd_array[i] == 0) {
					pr_info("DCAM: get fd[%d] fail\n", i);
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
				/*pr_info("buf, running path%d, fd 0x%x\n",
				*	p->channel_id, p->fd_array[i]);
				*/
				ret = path_cfg(DCAM_PATH_OUTPUT_ADDR,
						&frame_addr);
				if (unlikely(ret)) {
					pr_err("fail to cfg path%d output addr\n",
						p->channel_id);
					goto exit;
				}
			}
		} else {
			if (p->buf_flag == IMG_BUF_FLAG_INIT) {

				for (i = 0; i < p->buffer_count; i++) {
					if (unlikely(p->fd_array[i] == 0)) {
						pr_info("get init fd[%d] fail\n",
							i);
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
					buf_addr.frm_addr.mfd_y =
						p->fd_array[i];
					buf_addr.frm_addr.mfd_u =
						p->fd_array[i];
					buf_addr.frm_addr.mfd_v =
						p->fd_array[i];
					/*pr_info("buf init path%d, fd 0x%x\n",
					*	p->channel_id, p->fd_array[i]);
					*/
					ret = sprd_img_buf_queue_write(
						&path->buf_queue,
						&buf_addr);
				}
			} else {
				pr_err("no need to SET_FRAME_ADDR.\n");
			}
		}
	}
exit:
	return ret;
}

static int sprd_img_stream_on(struct file *file)
{
	int ret = 0;
	struct dcam_dev *dev = NULL;
	struct dcam_path_spec *path_0 = NULL;
	struct dcam_path_spec *path_1 = NULL;
	struct dcam_path_spec *path_2 = NULL;

	pr_info("%s start\n", __func__);
	dev = file->private_data;
	if (!dev) {
		ret = -EFAULT;
		pr_err("%s: dev is NULL.\n", __func__);
		goto exit;
	}

	mutex_lock(&dev->dcam_mutex);

	path_0 = &dev->dcam_cxt.dcam_path[DCAM_PATH0];
	path_1 = &dev->dcam_cxt.dcam_path[DCAM_PATH1];
	path_2 = &dev->dcam_cxt.dcam_path[DCAM_PATH2];
	memset((void *)path_0->frm_ptr,
		0,
		(uint32_t)(DCAM_FRM_CNT_MAX * sizeof(struct camera_frame *)));
	memset((void *)path_1->frm_ptr,
		0,
		(uint32_t)(DCAM_FRM_CNT_MAX * sizeof(struct camera_frame *)));
	memset((void *)path_2->frm_ptr,
		0,
		(uint32_t)(DCAM_FRM_CNT_MAX * sizeof(struct camera_frame *)));

	pr_info("is_work: path_0:%d,path_1:%d,path_2:%d, stream_on:%d.\n",
		path_0->is_work, path_1->is_work, path_2->is_work,
		atomic_read(&dev->stream_on));

	memset((void *)&path_1->in_rect_backup, 0x00,
			sizeof(struct camera_rect));
	memset((void *)&path_1->in_rect_current, 0x00,
			sizeof(struct camera_rect));

	do {
		/* dcam driver module initialization */
		ret = dcam_module_init(dev->dcam_cxt.if_mode,
			dev->dcam_cxt.sn_mode,
			dev->isp_dev_handle);
		if (unlikely(ret)) {
			pr_err("dcam_module_init failed: %d", ret);
			break;
		}

		ret = sprd_img_queue_init(&dev->queue);
		if (unlikely(ret != 0)) {
			pr_err("Failed to init queue.\n");
			break;
		}

		if (path_0->is_work || path_1->is_work || path_2->is_work) {
			ret = sprd_img_dcam_reg_isr(dev);
			if (unlikely(ret)) {
				pr_err("%s error", __func__);
				break;
			}
			ret = sprd_img_isp_reg_isr(dev);
			if (unlikely(ret)) {
				pr_err("%s error", __func__);
				break;
			}
		}
		/* config cap sub-module */
		ret = sprd_img_cap_cfg(&dev->dcam_cxt);
		if (unlikely(ret)) {
			pr_err("%s error", __func__);
			break;
		}

		/* config path1 sub-module if necessary*/
		if (path_1->is_work) {
			ret = sprd_img_path_cfg(dcam_path1_cfg, path_1);
			if (unlikely(ret)) {
				pr_err("%s error", __func__);
				break;
			}
			path_1->status = PATH_RUN;
		} else {
			ret = dcam_path1_cfg(DCAM_PATH_ENABLE,
					     &path_1->is_work);
			if (unlikely(ret)) {
				pr_err("%s error", __func__);
				break;
			}
		}

		/* config path2 sub-module if necessary*/
		if (path_2->is_work) {
			ret = sprd_img_path_cfg(dcam_path2_cfg, path_2);
			if (unlikely(ret)) {
				pr_err("%s error, line:%d", __func__, __LINE__);
				break;
			}
			ret = sprd_img_reg_path2_isr(dev);
			if (unlikely(ret)) {
				pr_err("%s error, line:%d", __func__, __LINE__);
				break;
			}
			path_2->status = PATH_RUN;
		} else {
			ret = dcam_path2_cfg(DCAM_PATH_ENABLE,
						&path_2->is_work);
			if (unlikely(ret)) {
				pr_err("%s error, line:%d", __func__, __LINE__);
				break;
			}
		}

		if (path_0->is_work) {
			ret = sprd_img_path0_cfg(dcam_path0_cfg, path_0);
			if (unlikely(ret)) {
				pr_err("%s error, line:%d", __func__, __LINE__);
				break;
			}
			path_0->status = PATH_RUN;
		} else {
			ret = dcam_path0_cfg(DCAM_PATH_ENABLE,
					     &path_0->is_work);
			if (unlikely(ret)) {
				pr_err("%s error, line:%d", __func__, __LINE__);
				break;
			}
		}
	} while (0);
	dev->frame_skipped = 0;

	if ((dev->dcam_cxt.set_flash.led0_ctrl &&
		dev->dcam_cxt.set_flash.led0_status == FLASH_HIGH_LIGHT) ||
		(dev->dcam_cxt.set_flash.led1_ctrl &&
		dev->dcam_cxt.set_flash.led1_status == FLASH_HIGH_LIGHT)) {
		if (dev->dcam_cxt.skip_number == 0)
			sprd_img_start_flash(NULL, dev);
	}

	ret = isp_set_statis_buf(dev->isp_dev_handle);
	if (ret) {
		pr_err("set isp statis buf failed, %d\n", ret);
		mutex_unlock(&dev->dcam_mutex);
		return ret;
	}
	ret = dcam_start();
	atomic_set(&dev->stream_on, 1);

	if (ret) {
		sprd_img_unreg_path2_isr(dev);
		sprd_img_dcam_unreg_isr(dev);
		sprd_img_isp_unreg_isr(dev);
		pr_err("sprd_img_stream_on failed: %d.\n", ret);
	} else {
		atomic_set(&dev->run_flag, 0);
		if (path_0->is_work || path_1->is_work || path_2->is_work)
			sprd_start_timer(&dev->dcam_timer, DCAM_TIMEOUT);
	}

	mutex_unlock(&dev->dcam_mutex);
	pr_info("%s end, ret: %d\n", __func__, ret);
exit:
	return ret;
}

static int sprd_img_stream_off(struct file *file)
{
	int ret = 0;
	struct dcam_dev *dev = NULL;
	struct dcam_path_spec *path_0 = NULL;
	struct dcam_path_spec *path_1 = NULL;
	struct dcam_path_spec *path_2 = NULL;

	dev = file->private_data;
	if (!dev) {
		ret = -EFAULT;
		pr_err("dev is NULL!\n");
		goto exit;
	}

	mutex_lock(&dev->dcam_mutex);

	path_0 = &dev->dcam_cxt.dcam_path[DCAM_PATH0];
	path_1 = &dev->dcam_cxt.dcam_path[DCAM_PATH1];
	path_2 = &dev->dcam_cxt.dcam_path[DCAM_PATH2];

	if (unlikely(atomic_read(&dev->stream_on) == 0)) {
		pr_debug("stream not on.\n");
		ret = sprd_img_local_deinit(dev);
		mutex_unlock(&dev->dcam_mutex);
		goto exit;
	}

	pr_info("%s start, path_0:%d, path_1:%d, path_2:%d, stream_on:%d.\n",
		__func__,
		path_0->is_work, path_1->is_work,
		path_2->is_work, atomic_read(&dev->stream_on));

	/*wait for scale done*/
	if (!path_2->is_work || path_2->status == PATH_IDLE) {
		dcam_get_resizer();
		dev->got_resizer = 1;
	}
	do {
		ret = sprd_stop_timer(&dev->dcam_timer);
		ret = dcam_stop(0);
		if (unlikely(ret)) {
			pr_err("dcam_stop failed: %d!\n", ret);
			break;
		}
		isp_reset();

		ret = sprd_img_dcam_unreg_isr(dev);
		if (unlikely(ret)) {
			pr_err("sprd_img_dcam_unreg_isr failed: %d!\n", ret);
			break;
		}

		ret = sprd_img_isp_unreg_isr(dev);
		if (unlikely(ret)) {
			pr_err("sprd_img_isp_unreg_isr failed: %d\n", ret);
			break;
		}

		if (path_0->is_work) {
			path_0->status = PATH_IDLE;
			path_0->is_work = 0;
		}

		if (path_1->is_work) {
			path_1->status = PATH_IDLE;
			path_1->is_work = 0;
		}

		if (path_2->is_work) {
			path_2->status = PATH_IDLE;

			if (dev->got_resizer) {
				ret = sprd_img_unreg_path2_isr(dev);
				if (unlikely(ret)) {
					pr_err("unreg_path2_isr failed:%d!\n",
					       ret);
					break;
				}
			}

			path_2->is_work = 0;
		}

		atomic_set(&dev->stream_on, 0);

		ret = dcam_module_deinit(dev->dcam_cxt.if_mode,
					 dev->dcam_cxt.sn_mode);
		if (unlikely(ret)) {
			pr_err("dcam_module_deinit failed:%d\n", ret);
			break;
		}

		ret = sprd_img_local_deinit(dev);
		if (unlikely(ret)) {
			pr_err("sprd_img_local_deinit failed:%d\n", ret);
			break;
		}
		dev->dcam_cxt.interp_mode = 0;
	} while (0);
	pfiommu_put_sg_table();
	dcam_rel_resizer();
	dev->got_resizer = 0;

	mutex_unlock(&dev->dcam_mutex);
	pr_info("%s end, ret: %d\n", __func__, ret);
exit:
	return ret;
}

static atomic_t s_dcam_cnt = ATOMIC_INIT(0);
static int sprd_img_k_open(struct inode *node, struct file *file)
{
	struct dcam_dev          *dev = NULL;
	struct miscdevice        *md = file->private_data;
	int                      ret = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	ret = pm_runtime_get(md->this_device->platform_data);
#endif
	if (atomic_read(&s_dcam_cnt) >= 1)
		return -EBADFD;
	pr_info("%s start\n", __func__);
	dev = vzalloc(sizeof(*dev));
	if (!dev) {
		ret = -ENOMEM;
		pr_err("sprd_img_k_open fail alloc.\n");
		return ret;
	}
	pr_info("dev:0x%x", (unsigned int)dev);

	mutex_init(&dev->dcam_mutex);
	init_completion(&dev->irq_com);
	ret = dcam_module_en(md->this_device->of_node);
	if (unlikely(ret != 0)) {
		pr_err("Failed to enable dcam module.\n");
		ret = -EIO;
		goto exit;
	}

	ret = sprd_init_timer(&dev->dcam_timer, (unsigned long)dev);
	ret = sprd_init_handle(dev);
	if (unlikely(ret != 0)) {
		pr_err("Failed to init queue.\n");
		ret = -EIO;
		goto exit;
	}

	ret = dcam_create_flash_thread(dev);
	if (unlikely(ret != 0)) {
		pr_err("Failed to create flash thread.\n");
		dev->flash_thread = NULL;
		ret = -EIO;
		goto exit;
	}

	ret = sprd_img_create_zoom_thread(dev);
	if (unlikely(ret != 0)) {
		pr_err("Failed to create zoom thread.\n");
		dev->zoom_thread = NULL;
		ret = -EIO;
		goto exit;
	}

	ret = sprd_isp_dev_init(&dev->isp_dev_handle);
	if (unlikely(ret != 0)) {
		pr_err("sprd_img:fail to init isp dev\n");
		ret = -EINVAL;
		goto exit;
	}
	dcam_reset(DCAM_RST_ALL);
	isp_reset();

	dev->driver_data = (void *)md;
	file->private_data = (void *)dev;
	atomic_inc(&s_dcam_cnt);

exit:
	if (unlikely(ret)) {
		pr_err("sprd_img_k_open failed!\n");
		dcam_module_dis(md->this_device->of_node);
		dcam_stop_flash_thread(dev);
		sprd_img_stop_zoom_thread(dev);
		if (dev->isp_dev_handle)
			sprd_isp_dev_deinit(dev->isp_dev_handle);
		vfree(dev);
	}

	pr_info("%s end, ret: %d\n", __func__, ret);
	return ret;
}

static int sprd_img_k_release(struct inode *node, struct file *file)
{
	int ret = 0;
	struct dcam_dev *dev = NULL;
	struct miscdevice	*md = NULL;

	if (atomic_read(&s_dcam_cnt) <= 0)
		return -EBADFD;
	pr_info("%s start\n", __func__);
	dev = file->private_data;
	if (!dev || !dev->driver_data) {
		pr_err("dev or dev->driver_data is NULL!\n");
		goto exit;
	}
	dev->private_key = 0;

	md = dev->driver_data;
	ret = sprd_img_stream_off(file);

	mutex_lock(&dev->dcam_mutex);
	init_completion(&dev->irq_com);
	ret = dcam_module_dis(md->this_device->of_node);
	if (unlikely(ret != 0)) {
		pr_err("dcam_module_dis failed: %d\n", ret);
		ret = -EIO;
	}
	sprd_isp_dev_deinit(dev->isp_dev_handle);
	dcam_stop_flash_thread(dev);
	sprd_img_stop_zoom_thread(dev);
	mutex_unlock(&dev->dcam_mutex);
	mutex_destroy(&dev->dcam_mutex);

	vfree(dev);
	dev = NULL;
	file->private_data = NULL;
	atomic_dec(&s_dcam_cnt);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	ret = pm_runtime_put_autosuspend(md->this_device->platform_data);
#endif
exit:
	pr_info("%s end\n", __func__);
	return ret;
}

static int dcam_get_sg(struct sprd_img_iova *data)
{
	int ret = 0;

	ret = sprd_ion_get_buffer(data->fd, NULL,
					&(data->sg_table),
					&(data->size));
	if (ret) {
		pr_err("iommu get_sg_table failed, ret %d, fd 0x%x sg %p\n",
			ret, data->fd, data->sg_table);
	}
	return ret;
}

static int dcam_get_iova(struct device *dev,
		 struct sprd_img_iova *mapdata, void __user *arg)
{
	int ret = 0;
	struct sprd_iommu_map_data iommu_map_data;

	if (dev == NULL) {
		pr_err("iommu dcam_get_iova fail, dev null\n");
		return -EFAULT;
	}

	if (sprd_iommu_attach_device(dev) == 0) {
		iommu_map_data.buf = mapdata->sg_table;
		iommu_map_data.iova_size = mapdata->size;
		iommu_map_data.ch_type = SPRD_IOMMU_FM_CH_RW;
		ret = sprd_iommu_map(dev, &iommu_map_data);
		if (ret)
			pr_err("map fail, ret %d, fd 0x%x iova 0x%lx size 0x%zx sg %p\n",
				ret, mapdata->fd, iommu_map_data.iova_addr,
				mapdata->size, mapdata->sg_table);
		else
			pr_info("map succe, fd 0x%x iova 0x%lx size 0x%zx sg %p\n",
				mapdata->fd, iommu_map_data.iova_addr,
				mapdata->size, mapdata->sg_table);
	}
	return ret;
}

static int dcam_free_iova(struct device *dev,
		  struct sprd_img_iova *unmapdata)
{
	int ret = 0;
	struct sprd_iommu_unmap_data iommu_data;

	if (dev == NULL) {
		pr_err("iommu dcam_free_iova fail, dev null\n");
		return -EFAULT;
	}

	if (sprd_iommu_attach_device(dev) == 0) {
		iommu_data.buf = unmapdata->sg_table;
		iommu_data.iova_size = unmapdata->size;
		iommu_data.iova_addr = 0;
		ret = sprd_iommu_unmap(dev, &iommu_data);
		if (ret)
			pr_err("unmap fail, ret %d, fd  0x%x iova 0x%x size 0x%zx sg %p\n",
				ret, unmapdata->fd,
				unmapdata->size,
				(unsigned int)iommu_data.iova_addr,
				unmapdata->sg_table);
		else
			pr_info("unmap succe, fd  0x%x iova 0x%x size 0x%zx sg %p\n",
				unmapdata->fd,
				(unsigned int)iommu_data.iova_addr,
				unmapdata->size,
				unmapdata->sg_table);
	}

	return ret;
}



static long sprd_img_k_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	struct dcam_dev          *dev = NULL;
	struct dcam_info         *info = NULL;
	uint32_t                 channel_id;
	uint32_t                 mode;
	uint32_t                 skip_num;
	uint32_t                 freq_flag;
	uint32_t                 zoom;
	uint32_t                 led0_ctrl;
	uint32_t                 led1_ctrl;
	uint32_t                 led0_status;
	uint32_t                 led1_status;
	uint32_t                 iommu_enable;
	uint32_t                 param;
	struct sprd_img_parm     parm;
	struct sprd_img_size     size;
	struct sprd_img_rect     rect;
	struct dcam_path_spec    *path = NULL;
	int                      ret = DCAM_RTN_SUCCESS;

	DCAM_TRACE("sprd_img_k_ioctl: cmd: 0x%x.\n", cmd);

	dev = file->private_data;
	if (!dev) {
		ret = -EFAULT;
		pr_err("sprd_img_k_ioctl: dev is NULL.\n");
		goto exit;
	}

	info = &dev->dcam_cxt;
	if(cmd == SPRD_IMG_IO_SET_KEY || dev->private_key == 1){}
	else {
		pr_err("%s private key error\n", __func__);
		goto exit;
	}

	switch (cmd) {
	case SPRD_IMG_IO_SET_MODE:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&mode, (void __user *)arg,
				sizeof(uint32_t));
		if (ret) {
			pr_err("sprd_img_k_ioctl: fail to get user info.\n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		dev->dcam_cxt.capture_mode = mode;
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("capture mode %d.\n",
			    dev->dcam_cxt.capture_mode);
		break;

	case SPRD_IMG_IO_SET_FREQ_FLAG:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&freq_flag, (void __user *)arg,
				sizeof(uint32_t));
		if (ret) {
			pr_err("sprd_img_k_ioctl: fail to get user info.\n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
#ifdef CONFIG_CPLL_1024M
		dcam_set_highclk_flag(freq_flag);
#endif
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("freq flag %d.\n", freq_flag);
		break;

	case SPRD_IMG_IO_SET_CAP_SKIP_NUM:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&skip_num, (void __user *)arg,
		      sizeof(uint32_t));
		if (ret) {
			pr_err("sprd_img_k_ioctl: fail to get user info.\n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		dev->dcam_cxt.skip_number = skip_num;
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("cap skip number %d.\n",
			  dev->dcam_cxt.skip_number);
		break;

	case SPRD_IMG_IO_SET_SENSOR_SIZE:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&size, (void __user *)arg,
			sizeof(struct sprd_img_size));
		if (ret) {
			pr_err("sprd_img_k_ioctl: fail to get user info.\n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		dev->dcam_cxt.cap_in_size.w = size.w;
		dev->dcam_cxt.cap_in_size.h = size.h;
		csi_api_set_mode_size(size.w, size.h);
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("sensor size %d %d.\n",
			dev->dcam_cxt.cap_in_size.w,
			dev->dcam_cxt.cap_in_size.h);
		break;

	case SPRD_IMG_IO_SET_SENSOR_TRIM:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&rect, (void __user *)arg,
			sizeof(struct sprd_img_rect));
		if (ret) {
			pr_err("sprd_img_k_ioctl: fail to get user info.\n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		dev->dcam_cxt.cap_in_rect.x = rect.x;
		dev->dcam_cxt.cap_in_rect.y = rect.y;
		dev->dcam_cxt.cap_in_rect.w = rect.w;
		dev->dcam_cxt.cap_in_rect.h = rect.h;

		dev->dcam_cxt.cap_out_size.w = dev->dcam_cxt.cap_in_rect.w;
		dev->dcam_cxt.cap_out_size.h = dev->dcam_cxt.cap_in_rect.h;
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("sensor trim x y w h %d %d %d %d.\n",
			dev->dcam_cxt.cap_in_rect.x,
			dev->dcam_cxt.cap_in_rect.y,
			dev->dcam_cxt.cap_in_rect.w,
			dev->dcam_cxt.cap_in_rect.h);
		break;

	case SPRD_IMG_IO_SET_FRM_ID_BASE:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&parm, (void __user *)arg,
			sizeof(struct sprd_img_parm));
		if (ret) {
			pr_err("sprd_img_k_ioctl: fail to get user info.\n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		switch (parm.channel_id) {
		case DCAM_PATH0:
		case DCAM_PATH1:
		case DCAM_PATH2:
			dev->dcam_cxt.dcam_path[parm.channel_id].frm_id_base =
				parm.frame_base_id;
			break;
		default:
			pr_err("Wrong channel ID, %d .\n",
				parm.channel_id);
			mutex_unlock(&dev->dcam_mutex);
			ret = -EFAULT;
			goto exit;
		}
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("channel %d, base id 0x%x.\n",
			parm.channel_id,
			parm.frame_base_id);
		break;

	case SPRD_IMG_IO_SET_CROP:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&parm, (void __user *)arg,
				sizeof(struct sprd_img_parm));
		if (ret) {
			pr_err("sprd_img_k_ioctl: fail to get user info.\n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		ret = sprd_img_set_crop(file, &parm);

		mutex_unlock(&dev->dcam_mutex);
		break;

	case SPRD_IMG_IO_SET_FLASH:
		/*TODO:HAL flash info struct should change like this*/
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&info->set_flash, (void __user *)arg,
					sizeof(struct sprd_img_set_flash));
		if (ret) {
			pr_err("sprd_img_k_ioctl: fail to get user info.\n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		led0_ctrl = info->set_flash.led0_ctrl;
		led1_ctrl = info->set_flash.led1_ctrl;
		led0_status = info->set_flash.led0_status;
		led1_status = info->set_flash.led1_status;
		mutex_unlock(&dev->dcam_mutex);
		if ((led0_ctrl &&
		     (led0_status == FLASH_CLOSE_AFTER_OPEN ||
		      led0_status == FLASH_CLOSE ||
		      led0_status == FLASH_CLOSE_AFTER_AUTOFOCUS)) ||
		    (led1_ctrl &&
		     (led1_status == FLASH_CLOSE_AFTER_OPEN ||
		      led1_status == FLASH_CLOSE ||
		      led1_status == FLASH_CLOSE_AFTER_AUTOFOCUS))) {
			complete(&dev->flash_thread_com);
		}
		DCAM_TRACE("led0_ctrl %d led0_status %d\n",
			   info->set_flash.led0_ctrl,
			   info->set_flash.led0_status);
		DCAM_TRACE("led1_ctrl %d led1_status %d\n",
			   info->set_flash.led1_ctrl,
			   info->set_flash.led1_status);
		break;

	case SPRD_IMG_IO_SET_OUTPUT_SIZE:
		DCAM_TRACE("set output size.\n");
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&parm, (void __user *)arg,
				sizeof(struct sprd_img_parm));
		if (ret) {
			pr_err("sprd_img_k_ioctl: fail to get user info.\n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		dev->dcam_cxt.dst_size.w = parm.dst_size.w;
		dev->dcam_cxt.dst_size.h = parm.dst_size.h;
		dev->dcam_cxt.pxl_fmt = parm.pixel_fmt;
		dev->dcam_cxt.need_isp_tool = parm.need_isp_tool;
		dev->dcam_cxt.need_isp = parm.need_isp;
		/*dev->dcam_cxt.need_shrink = parm.shrink;*/
		dev->dcam_cxt.path_input_rect.x = parm.crop_rect.x;
		dev->dcam_cxt.path_input_rect.y = parm.crop_rect.y;
		dev->dcam_cxt.path_input_rect.w = parm.crop_rect.w;
		dev->dcam_cxt.path_input_rect.h = parm.crop_rect.h;
		mutex_unlock(&dev->dcam_mutex);
		break;

	case SPRD_IMG_IO_SET_ZOOM_MODE:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&zoom, (void __user *)arg,
					sizeof(uint32_t));
		if (ret) {
			pr_err("sprd_img_k_ioctl: fail to get user info.\n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		dev->dcam_cxt.is_smooth_zoom = zoom;
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("SPRD_IMG_IO_SET_ZOOM_MODE, zoom mode %d.\n",
				dev->dcam_cxt.is_smooth_zoom);
		break;

	case SPRD_IMG_IO_SET_SENSOR_IF:
	{
		struct sprd_img_sensor_if sensor_if;

		DCAM_TRACE("set sensor if.\n");
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&sensor_if, (void __user *)arg,
				sizeof(struct sprd_img_sensor_if));
		if (unlikely(ret)) {
			pr_err("sprd_img_k_ioctl: fail to get user info.\n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		ret = sprd_img_set_sensor_if(file, &sensor_if);
		mutex_unlock(&dev->dcam_mutex);
		break;
	}

	case SPRD_IMG_IO_SET_FRAME_ADDR:
		DCAM_TRACE("set frame addr.\n");
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&parm, (void __user *)arg,
				sizeof(struct sprd_img_parm));
		if (ret) {
			pr_err("sprd_img_k_ioctl: fail to get user info.\n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		ret = sprd_img_set_frame_addr(file, &parm);
		mutex_unlock(&dev->dcam_mutex);
		break;

	case SPRD_IMG_IO_PATH_FRM_DECI:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&parm, (void __user *)arg,
				sizeof(struct sprd_img_parm));
		if (ret) {
			pr_err("sprd_img_k_ioctl: fail to get user info.\n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		path = &dev->dcam_cxt.dcam_path[parm.channel_id];
		path->path_frm_deci = parm.deci;
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("channel %d, frm_deci=%d.\n",
			parm.channel_id, path->path_frm_deci);
		break;

	case SPRD_IMG_IO_SET_SHRINK:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&parm, (void __user *)arg,
				sizeof(struct sprd_img_parm));
		if (ret) {
			pr_err("sprd_img_k_ioctl: fail to get user info.\n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		path = &dev->dcam_cxt.dcam_path[parm.channel_id];
		path->regular_desc = parm.regular_desc;
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("channel %d, shrink=%d.\n",
			parm.channel_id, path->regular_desc.regular_mode);
		break;

	case SPRD_IMG_IO_PATH_PAUSE:
		ret = copy_from_user(&parm, (void __user *)arg,
					sizeof(struct sprd_img_parm));
		if (ret) {
			pr_err("sprd_img_k_ioctl: fail to get user info.\n");
			goto exit;
		}
		sprd_img_streampause(file, parm.channel_id, parm.reserved[0]);
		break;

	case SPRD_IMG_IO_PATH_RESUME:
		ret = copy_from_user(&channel_id, (void __user *)arg,
				sizeof(uint32_t));
		if (ret) {
			pr_err("sprd_img_k_ioctl: fail to get user info.\n");
			goto exit;
		}
		sprd_img_streamresume(file, channel_id);
		break;

	case SPRD_IMG_IO_STREAM_ON:
	{
		DCAM_TRACE("stream on.\n");
		ret = sprd_img_stream_on(file);
		break;
	}

	case SPRD_IMG_IO_STREAM_OFF:
	{
		DCAM_TRACE("stream off.\n");
		ret = sprd_img_stream_off(file);
		break;
	}

	case SPRD_IMG_IO_GET_FMT:
	{
		struct dcam_format          *fmt;
		struct sprd_img_get_fmt     fmt_desc;

		DCAM_TRACE("SPRD_IMG_IO_GET_FMT.\n");
		ret = copy_from_user(&fmt_desc, (void __user *)arg,
				sizeof(struct sprd_img_get_fmt));
		if (ret) {
			pr_err("sprd_img_k_ioctl: fail to get user info.\n");
			goto exit;
		}
		if (unlikely(fmt_desc.index >= ARRAY_SIZE(dcam_img_fmt)))
			return -EINVAL;

		fmt = &dcam_img_fmt[fmt_desc.index];
		fmt_desc.fmt = fmt->fourcc;

		ret = copy_to_user((void __user *)arg, &fmt_desc,
				sizeof(struct sprd_img_get_fmt));
		break;
	}

	case SPRD_IMG_IO_SET_SENSOR_MAX_SIZE:
		mutex_lock(&dev->dcam_mutex);
		dev->dcam_cxt.interp_mode = 1;
		mutex_unlock(&dev->dcam_mutex);
		break;

	case SPRD_IMG_IO_GET_CH_ID:
	{
		struct dcam_path_spec    *path_0 = NULL;
		struct dcam_path_spec    *path_1 = NULL;
		struct dcam_path_spec    *path_2 = NULL;
		struct dcam_get_path_id  path_id;

		DCAM_TRACE("SPRD_IMG_IO_GET_CH_ID.\n");
		path_0 = &dev->dcam_cxt.dcam_path[DCAM_PATH0];
		path_1 = &dev->dcam_cxt.dcam_path[DCAM_PATH1];
		path_2 = &dev->dcam_cxt.dcam_path[DCAM_PATH2];

		memset((void *)&path_id, 0, sizeof(struct dcam_get_path_id));
		path_id.input_size.w = dev->dcam_cxt.cap_in_rect.w;
		path_id.input_size.h = dev->dcam_cxt.cap_in_rect.h;
		path_id.output_size.w = dev->dcam_cxt.dst_size.w;
		path_id.output_size.h = dev->dcam_cxt.dst_size.h;
		path_id.fourcc = dev->dcam_cxt.pxl_fmt;
		path_id.need_isp_tool = dev->dcam_cxt.need_isp_tool;
		path_id.need_isp = dev->dcam_cxt.need_isp;
		path_id.need_interp = dev->dcam_cxt.interp_mode;
/*		path_id.need_shrink = dev->dcam_cxt.need_shrink;*/
		path_id.input_trim.x = dev->dcam_cxt.path_input_rect.x;
		path_id.input_trim.y = dev->dcam_cxt.path_input_rect.y;
		path_id.input_trim.w = dev->dcam_cxt.path_input_rect.w;
		path_id.input_trim.h = dev->dcam_cxt.path_input_rect.h;

		DCAM_TRACE("get param, path work %d %d %d.\n",
			path_0->is_work, path_1->is_work, path_2->is_work);

		path_id.is_path_work[DCAM_PATH0] = path_0->is_work;
		path_id.is_path_work[DCAM_PATH1] = path_1->is_work;
		path_id.is_path_work[DCAM_PATH2] = path_2->is_work;
		ret = dcam_get_path_id(&path_id, &channel_id);
		ret = copy_to_user((void __user *)arg, &channel_id,
				    sizeof(uint32_t));
		DCAM_TRACE("get channel_id %d.\n", channel_id);
		break;
	}

	case SPRD_IMG_IO_GET_TIME:
	{
		struct timeval           time;
		struct sprd_img_time     utime;

		DCAM_TRACE("SPRD_IMG_IO_GET_TIME.\n");
		img_get_timestamp(&time);
		utime.sec = time.tv_sec;
		utime.usec = time.tv_usec;
		ret = copy_to_user((void __user *)arg, &utime,
				    sizeof(struct sprd_img_time));
		break;
	}

	case SPRD_IMG_IO_CHECK_FMT:
	{
		struct dcam_format       *fmt;
		struct sprd_img_format   img_format;

		DCAM_TRACE("SPRD_IMG_IO_CHECK_FMT.\n");
		ret = copy_from_user(&img_format, (void __user *)arg,
					sizeof(struct sprd_img_format));
		if (ret) {
			pr_err("sprd_img_k_ioctl: fail to get user info.\n");
			goto exit;
		}

		fmt = sprd_img_get_format(img_format.fourcc);
		if (unlikely(!fmt)) {
			pr_err("Fourcc format (0x%08x) invalid..\n",
				img_format.fourcc);
			return -EINVAL;
		}

		if (img_format.channel_id == DCAM_PATH1) {
			mutex_lock(&dev->dcam_mutex);
			ret = sprd_img_check_path1_cap(fmt->fourcc, &img_format,
					&dev->dcam_cxt);
			mutex_unlock(&dev->dcam_mutex);
			channel_id = DCAM_PATH1;
		} else if (img_format.channel_id == DCAM_PATH2) {
			if (img_format.is_lightly) {
				mutex_lock(&dev->dcam_mutex);
				ret = sprd_img_check_path2_cap(fmt->fourcc,
						&img_format, &dev->dcam_cxt);
				mutex_unlock(&dev->dcam_mutex);
			} else {
				dcam_get_resizer();
				dev->got_resizer = 1;
				mutex_lock(&dev->dcam_mutex);
				ret = sprd_img_check_path2_cap(fmt->fourcc,
					&img_format, &dev->dcam_cxt);
				mutex_unlock(&dev->dcam_mutex);
				if (ret) {
					dcam_rel_resizer();
					dev->got_resizer = 0;
				}
			}
			channel_id = DCAM_PATH2;
		} else if (img_format.channel_id == DCAM_PATH0) {
			mutex_lock(&dev->dcam_mutex);
			ret = sprd_img_check_path0_cap(fmt->fourcc, &img_format,
						&dev->dcam_cxt);
			mutex_unlock(&dev->dcam_mutex);
			channel_id = DCAM_PATH0;
		} else {
			pr_err("Buf type invalid.\n");
			return -EINVAL;
		}
		if (ret)
			goto exit;

		memcpy((void *)&img_format.endian,
			(void *)&dev->dcam_cxt.dcam_path[channel_id].end_sel,
			sizeof(struct dcam_endian_sel));
		if ((ret == 0) && (atomic_read(&dev->stream_on) != 0)) {
			if (channel_id == DCAM_PATH0 ||
				channel_id == DCAM_PATH1 ||
				channel_id == DCAM_PATH2) {
				ret = sprd_img_update_video(file, channel_id);
			}
		}
		ret = copy_to_user((void __user *)arg, &img_format,
				    sizeof(struct sprd_img_format));
		break;
	}
	case SPRD_IMG_IO_CFG_FLASH:
	{
		/*TODO: struct sprd_flash_cfg_param define in sprd_img.h changed
		 * so we should follow the latest Definition both in HAL&here
		 */
		struct sprd_flash_cfg_param	 cfg_param;

		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&cfg_param, (void __user *)arg,
				sizeof(struct sprd_flash_cfg_param));
		if (ret) {
			pr_err("SPRD_IMG_IO_CFG_FLASH:fail to get userinfo.\n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		/*need to do later*/
		//ret = sprd_flash_cfg(&cfg_param);
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("SPRD_IMG_IO_CFG_FLASH, ret=%d.\n", ret);
	}
		break;
	case SPRD_IMG_IO_PDAF_CONTROL:
	{
	/*mode: 0: no imagedata    1: image datatype    2: image vir channel
	 *raw10:  image_vc:1    image_dt: 0x2b
	 *yuv422: image_vc:1    image_dt: 0x1e
	 */
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&parm, (void __user *)arg,
				sizeof(struct sprd_img_parm));
		if (ret) {
			pr_err("failed to get user info.\n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		path = &info->dcam_path[parm.channel_id];
		path->pdaf_ctrl.mode = parm.pdaf_ctrl.mode;
		path->pdaf_ctrl.image_vc = parm.pdaf_ctrl.image_vc;
		path->pdaf_ctrl.image_dt = parm.pdaf_ctrl.image_dt;
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("channel %d, pdaf mode %d type %x %x.\n",
			   parm.channel_id, path->pdaf_ctrl.mode,
			   path->pdaf_ctrl.image_vc,
			   path->pdaf_ctrl.image_dt);

		break;
	}

	case SPRD_IMG_IO_GET_IOMMU_STATUS:
	{
		struct platform_device *pdev = sprd_dcam_get_pdev();

		ret = copy_from_user(&iommu_enable, (void __user *)arg,
					sizeof(unsigned char));
		if (ret) {
			pr_err("copy_from_user failed\n");
			goto exit;
		}

		if (sprd_iommu_attach_device(&pdev->dev) == 0)
			iommu_enable = 1;
		else
			iommu_enable = 0;

		ret = copy_to_user((void __user *)arg, &iommu_enable,
					sizeof(unsigned char));

		break;
	}
	case SPRD_ISP_IO_CAPABILITY:
	case SPRD_ISP_IO_CFG_START:
	case SPRD_ISP_IO_CFG_PARAM:
	case SPRD_ISP_IO_SET_STATIS_BUF:
	case SPRD_ISP_IO_RAW_CAP:
		mutex_lock(&dev->dcam_mutex);
		DCAM_TRACE("start isp ioctl cmd%d dev %p\n",
			_IOC_NR(cmd), dev);
		ret = sprd_isp_k_ioctl(dev->isp_dev_handle, cmd, arg);
		mutex_unlock(&dev->dcam_mutex);
		break;

	case SPRD_IMG_IO_MAP_IOVA:
	{
		struct platform_device *pdev = NULL;
		struct sprd_img_iova mapdata = {0};

		ret = copy_from_user((void *)&mapdata,
				(const void __user *)arg,
				sizeof(struct sprd_img_iova));
		if (ret) {
			pr_err("copy mapdata failed, ret %d\n", ret);
			return -EFAULT;
		}

		pdev = sprd_dcam_get_pdev();
		ret = dcam_get_iova(&pdev->dev, &mapdata,
					(void __user *)arg);
		break;
	}

	case SPRD_IMG_IO_UNMAP_IOVA:
	{
		struct platform_device *pdev = NULL;
		struct sprd_img_iova ummapdata = {0};

		ret = copy_from_user((void *)&ummapdata,
				(const void __user *)arg,
				sizeof(struct sprd_img_iova));
		if (ret) {
			pr_err("copy ummapdata failed, ret %d\n", ret);
			return -EFAULT;
		}

		pdev = sprd_dcam_get_pdev();
		ret = dcam_free_iova(&pdev->dev, &ummapdata);
		break;
	}

	case SPRD_IMG_IO_GET_SG:
	{
		struct sprd_img_iova data = {0};

		ret = copy_from_user((void *)&data,
				(const void __user *)arg,
				sizeof(struct sprd_img_iova));
		if (ret) {
			pr_err("copy sg data failed, ret %d\n", ret);
			return -EFAULT;
		}

		ret = dcam_get_sg(&data);
		if (copy_to_user((void __user *)arg, &data,
				sizeof(struct sprd_img_iova))) {
			pr_err("%s, copy to user error!\n", __func__);
			return -EFAULT;
		}
		break;
	}

	case SPRD_ISP_IO_REG_ISP_ISR:
		ret = copy_from_user((void *)&mode,
				(void __user *)arg, sizeof(uint32_t));
		if (unlikely(ret)) {
			pr_err("copy from user err\n");
			return ret;
		}
		if (mode)
			sprd_img_isp_reg_isr(dev);
		else
			sprd_img_isp_unreg_isr(dev);
		break;
	case SPRD_IMG_IO_SET_KEY:
		ret = 0;
		param = 0;
		ret = copy_from_user(&param, (void __user *)arg, sizeof(uint32_t));
		if (unlikely(ret)) {
			pr_err("fail to copy from user, ret %d\n", ret);
			return -EFAULT;
		}
		if (param == CAM_IOCTL_PRIVATE_KEY) {
			dev->private_key = 1;
		}
		pr_info("cam%d get ioctrl permission %d\n", dev->idx, dev->private_key);
		break;
	default:
		pr_err("sprd_img_k_ioctl: invalid cmd NR %u\n", _IOC_NR(cmd));
		break;
	}

exit:
	if (ret) {
		pr_err("sprd_img_k_ioctl fail, cmd NR %u\n", _IOC_NR(cmd));
		ret = -EFAULT;
	}

	return ret;
}

static ssize_t sprd_img_read(struct file *file, char __user *u_data,
		      size_t cnt, loff_t *cnt_ret)
{
	struct dcam_dev          *dev = file->private_data;
	struct dcam_node         node;
	struct dcam_path_spec    *path;
	struct sprd_img_read_op  read_op;
	struct dcam_path_capability path_capability;
	uint32_t                 channel_id;
	int                      fmr_index = 0, i;
	int                      ret = 0;

	DCAM_TRACE("sprd_img_read.\n");

	if (!dev)
		return -EFAULT;

	if (cnt < sizeof(struct sprd_img_read_op)) {
		/*pr_err("sprd_img_read: error, cnt %ld read_op %ld.\n",
		*		cnt, sizeof(struct sprd_img_read_op));
		*/
		return -EIO;
	}

	ret = copy_from_user(&read_op, (void __user *)u_data,
				sizeof(struct sprd_img_read_op));
	if (ret) {
		pr_err("sprd_img_read: fail to get user info.\n");
		return -EFAULT;
	}

	switch (read_op.cmd) {
	case SPRD_IMG_GET_SCALE_CAP:
		DCAM_TRACE("get scale capbility.\n");
		read_op.parm.reserved[0] = DCAM_PATH2_LINE_BUF_LENGTH;
		read_op.parm.reserved[1] = DCAM_SC_COEFF_UP_MAX;
		read_op.parm.reserved[2] = DCAM_SCALING_THRESHOLD;
		DCAM_TRACE("line threshold %d, sc factor %d, scaling %d.\n",
			    read_op.parm.reserved[0],
			    read_op.parm.reserved[1],
			    read_op.parm.reserved[2]);

		break;

	case SPRD_IMG_GET_FRM_BUFFER:
		DCAM_TRACE("read frame buffer.\n");
		memset(&read_op, 0, sizeof(struct sprd_img_read_op));
		memset(&node, 0, sizeof(struct dcam_node));
		while (1) {
			ret = wait_for_completion_interruptible(&dev->irq_com);
			if (ret == 0) {
				break;
			} else if (ret == -ERESTARTSYS) {
				read_op.evt = IMG_SYS_BUSY;
				ret = DCAM_RTN_SUCCESS;
				goto read_end;
			} else {
				pr_err("read frame buffer,failed to down.\n");
				return -EPERM;
			}
		}

		if (sprd_img_queue_read(&dev->queue, &node)) {
			pr_err("read frame buffer,queue is null.\n");
			read_op.evt = IMG_SYS_BUSY;
			ret = DCAM_RTN_SUCCESS;
			goto read_end;
		} else {
			if (node.invalid_flag) {
				pr_err("read frm buf,invalid node.\n");
				read_op.evt = IMG_SYS_BUSY;
				ret = DCAM_RTN_SUCCESS;
				goto read_end;
			}
		}

		DCAM_TRACE("time, %ld %ld.\n",
			(unsigned long)node.time.tv_sec,
			(unsigned long)node.time.tv_usec);

		read_op.evt = node.irq_flag;
		if (read_op.evt == IMG_TX_DONE ||
		    read_op.evt == IMG_CANCELED_BUF) {
			read_op.parm.frame.channel_id = node.f_type;
			channel_id = node.f_type;
			path = &dev->dcam_cxt.dcam_path[channel_id];
			read_op.parm.frame.index = path->frm_id_base;
			read_op.parm.frame.height = node.height;
			read_op.parm.frame.length = node.reserved;
			read_op.parm.frame.sec = node.time.tv_sec;
			read_op.parm.frame.usec = node.time.tv_usec;
			/*fmr_index  = node.index - path->frm_id_base;*/
			/*read_op.parm.frame.real_index =
			 * path->index[fmr_index];
			 */
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
			read_op.parm.frame.reserved[0] = node.reserved;
			DCAM_TRACE("index %d real_index %d",
				read_op.parm.frame.index,
				read_op.parm.frame.real_index);
			DCAM_TRACE("frm_id_base %d fmr_index %d.\n",
				read_op.parm.frame.frm_base_id,
				fmr_index);
			DCAM_TRACE("dcam_img fd= 0x%x.\n",
				read_op.parm.frame.mfd);
		} else {
			if (read_op.evt == IMG_TIMEOUT) {
				csi_api_reg_trace();
				sprd_img_print_reg();
				print_isp_regs();
			}
		}
		DCAM_TRACE("read frmbuf,evt 0x%x channel_id 0x%x index 0x%x.\n",
			read_op.evt,
			read_op.parm.frame.channel_id,
			read_op.parm.frame.index);
		break;

	case SPRD_IMG_GET_PATH_CAP:
		DCAM_TRACE("get path capbility.\n");
		dcam_get_path_capability(&path_capability);
		read_op.parm.capability.count = path_capability.count;
		for (i = 0; i < path_capability.count; i++) {
			read_op.parm.capability.path_info[i].line_buf =
				path_capability.path_info[i].line_buf;
			read_op.parm.capability.path_info[i].support_yuv =
				path_capability.path_info[i].support_yuv;
			read_op.parm.capability.path_info[i].support_raw =
				path_capability.path_info[i].support_raw;
			read_op.parm.capability.path_info[i].support_jpeg =
				path_capability.path_info[i].support_jpeg;
			read_op.parm.capability.path_info[i].support_scaling =
				path_capability.path_info[i].support_scaling;
			read_op.parm.capability.path_info[i].support_trim =
				path_capability.path_info[i].support_trim;
			read_op.parm.capability.path_info[i].is_scaleing_path =
				path_capability.path_info[i].is_scaleing_path;
		}
		break;

	default:
		pr_err("sprd_img_read, invalid cmd.\n");
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
	struct dcam_dev          *dev = file->private_data;
	struct dcam_info         *info;
	struct dcam_path_spec    *path;
	struct sprd_img_write_op write_op;
	uint32_t                 index;
	int                      ret = 0;

	DCAM_TRACE("sprd_img_write.\n");

	if (!dev)
		return -EFAULT;
	info = &dev->dcam_cxt;

	if (cnt < sizeof(struct sprd_img_write_op)) {
		pr_err("sprd_img_write: error cnt %d.\n", cnt);
		return -EIO;
	}

	ret = copy_from_user(&write_op, (void __user *)u_data,
			     sizeof(struct sprd_img_write_op));
	if (ret) {
		pr_err("sprd_img_write: fail to get user info.\n");
		return -EFAULT;
	}

	switch (write_op.cmd) {
	case SPRD_IMG_STOP_DCAM:
		mutex_lock(&dev->dcam_mutex);
		ret = sprd_img_tx_stop(dev);
		mutex_unlock(&dev->dcam_mutex);
		pr_err("STOP_DCAM.\n");
		break;

	case SPRD_IMG_FREE_FRAME:
		if (atomic_read(&dev->stream_on) == 0) {
			pr_err("dev close, no need free!");
			break;
		}
		switch (write_op.channel_id) {
		case DCAM_PATH0:
		case DCAM_PATH1:
		case DCAM_PATH2:
			path = &info->dcam_path[write_op.channel_id];
			break;
		default:
			pr_err("error free frame buffer, channel_id 0x%x.\n",
				write_op.channel_id);
			return -EINVAL;
		}

		if (path->status == PATH_IDLE) {
			DCAM_TRACE("error free frame buffer,channel_id 0x%x.\n",
				write_op.channel_id);
			return -EINVAL;
		}

		if (unlikely(write_op.index >
			path->frm_id_base + path->frm_cnt_act - 1)) {
			pr_err("index %d, frm_id_base %d frm_cnt_act %d.\n",
				write_op.index, path->frm_id_base,
				path->frm_cnt_act);
			ret = -EINVAL;
		} else if (write_op.index < path->frm_id_base) {
			pr_err("error, index %d, frm_id_base %d.\n",
				write_op.index, path->frm_id_base);
			ret = -EINVAL;
		} else {
			index = write_op.index - path->frm_id_base;
			if (path->frm_ptr[index])
				dcam_frame_unlock(path->frm_ptr[index]);
			DCAM_TRACE("free frm buf,channel_id 0x%x,index 0x%x.\n",
				    write_op.channel_id, write_op.index);
		}
		break;

	default:
		pr_err("cmd error!.\n");
		ret = -EINVAL;
		break;
	}

	if (ret)
		cnt = ret;

	return cnt;
}

int32_t sprd_dcam_registers_dump(void *buf, uint32_t buf_len)
{
	uint32_t *reg_buf = NULL;
	uint32_t reg_buf_len;
	int ret;

	if (buf == NULL || buf_len < 0x400) {
		pr_err("%s input para is error", __func__);
		return -1;
	}

	reg_buf = (uint32_t *)buf;
	reg_buf_len = 0x400;

	ret = dcam_read_registers(reg_buf, &reg_buf_len);
	if (ret) {
		pr_err("dcam_read_registers return error: %d", ret);
		return -1;
	}

	return reg_buf_len;
}


static const struct file_operations image_fops = {
	.owner          = THIS_MODULE,
	.open           = sprd_img_k_open,
	.unlocked_ioctl = sprd_img_k_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = sprd_img_k_ioctl,
#endif
	.release        = sprd_img_k_release,
	.read           = sprd_img_read,
	.write          = sprd_img_write,
};

static struct miscdevice image_dev = {
	.minor = IMAGE_MINOR,
	.name = IMG_DEVICE_NAME,
	.fops = &image_fops,
};


static int sprd_img_probe(struct platform_device *pdev)
{
	int ret = 0;
	pr_err("sprd_img_probe called.\n");

	ret = misc_register(&image_dev);
	if (ret) {
		pr_err("cannot register miscdev on minor=%d (%d).\n",
			IMAGE_MINOR, ret);
		ret = -EACCES;
		goto exit;
	}
	image_dev.this_device->of_node = pdev->dev.of_node;
	/*TODO: parse DT will call later.*/
	//sprd_cam_pw_domain_init(pdev);
	sprd_dcam_parse_regbase(pdev);
	sprd_dcam_parse_clk(pdev);
	sprd_dcam_parse_irq(pdev);
	sprd_dcam_drv_init(pdev);
	ret = sprd_isp_parse_dt(pdev->dev.of_node);
	if (ret) {
		pr_err("fail to parse isp dts\n");
		goto parse_exit;
	}
	sprd_isp_drv_init();
	scale_k_init();
	rot_k_init();
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	image_dev.this_device->platform_data = &pdev->dev;
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
#endif
	pr_info("sprd_img_probe Success.\n");
	goto exit;
parse_exit:
	sprd_dcam_drv_deinit();
	misc_deregister(&image_dev);
exit:
	return ret;
}

static int sprd_img_remove(struct platform_device *pdev)
{
	sprd_dcam_drv_deinit();
	misc_deregister(&image_dev);
	scale_k_exit();
	rot_k_exit();
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
	sprd_img_setflash(&set_flash);
	set_flash.flash_index = 1;
	sprd_img_setflash(&set_flash);
}

static const struct of_device_id  of_match_table_dcam[] = {
	{ .compatible = "sprd,dcam"},
	{ },
};

static struct platform_driver sprd_img_driver = {
	.probe = sprd_img_probe,
	.remove = sprd_img_remove,
	.shutdown = sprd_img_shutdown,
	.driver = {
		/*.owner = THIS_MODULE,*/
		.name = IMG_DEVICE_NAME,
		.of_match_table = of_match_ptr(of_match_table_dcam),
		},
};

module_platform_driver(sprd_img_driver);

MODULE_DESCRIPTION("DCAM Driver");
MODULE_AUTHOR("Multimedia_Camera@Spreadtrum");
MODULE_LICENSE("GPL");

