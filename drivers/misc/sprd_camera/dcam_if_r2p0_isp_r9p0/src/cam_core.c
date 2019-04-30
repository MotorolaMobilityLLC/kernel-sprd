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

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/sprd_iommu.h>

#include "cam_pw_domain.h"
#include "dcam_drv.h"
#include "isp_path.h"
#include "isp_3dnr_drv.h"
#include "cam_flash.h"

#include "sprd_sensor_drv.h"
#ifdef CONFIG_COMPAT
#include "cam_drv_compat.h"
#endif
#include "csi_api.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CAM_CORE: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define IMG_DEVICE_NAME            "sprd_image"

#define CAM_ALIGN4_SIZE            4
#define CAM_ALIGNTO4(size)         ((size) & ~(CAM_ALIGN4_SIZE - 1))
#define CAM_ALIGN_SIZE             2
#define CAM_ALIGNTO(size)          ((size) & ~(CAM_ALIGN_SIZE - 1))
#define MAX(_x, _y)                (((_x) > (_y)) ? (_x) : (_y))

#ifdef CONFIG_SPRD_CAMERA_ON_FPGA
#define DCAM_TIMEOUT               (2000*100)
#else
#define DCAM_TIMEOUT               1500
#endif

enum {
	PATH_IDLE = 0x00,
	PATH_RUN,
};

struct camera_format {
	char *name;
	uint32_t fourcc;
	int depth;
};

struct camera_node {
	uint32_t irq_flag; /*irq result*/
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
	uint32_t invalid_flag;
	uint32_t frame_id;
	uint32_t mfd[3];
	uint32_t zoom_ratio;
	uint32_t reserved[2];
	struct camera_time time;
	struct dual_sync_info dual_info;
};

struct camera_path_spec {
	uint32_t is_work;/*path param check ok*/
	uint32_t frm_type;/*channel id*/
	uint32_t path_frm_deci;
	uint32_t skip_num;
	uint32_t src_sel;  /*path source sel*/
	uint32_t status;
	struct camera_size in_size;
	struct camera_path_dec img_deci;/*size_deci*/
	struct camera_rect in_rect;
	struct camera_size out_size;
	struct camera_size max_out_size;
	enum dcam_fmt out_fmt;
	struct camera_endian_sel end_sel;
	uint32_t fourcc;
	uint32_t pixel_depth;/*bits of 1 pixel*/
	uint32_t frm_id_base;
	struct camera_addr frm_reserved_addr;
	struct cam_buf_queue buf_queue;
	struct dcam_regular_desc regular_desc;
	struct sprd_pdaf_control pdaf_ctrl;
	struct sprd_ebd_control ebd_ctrl;
	uint32_t assoc_idx;/*dcam cowork with isp path*/
	uint32_t buf_num;
	uint32_t need_downsizer;
	struct zoom_info_t zoom_info;
	unsigned char bin_crop_bypass;
};

struct camera_context {
	uint32_t sn_mode;
	uint32_t img_ptn;
	uint32_t frm_deci;
	uint32_t data_bits;
	uint32_t is_loose;/*only for raw10*/
	uint32_t lane_num;/*only for csi2*/
	uint32_t bps_per_lane;/*only for csi2*/
	uint32_t is_high_fps;
	uint32_t high_fps_skip_num;
	uint32_t pxl_fmt;
	uint32_t sn_fmt;
	uint32_t need_isp_tool;
	uint32_t need_isp;
	uint32_t need_4in1;
	uint32_t need_3dnr;
	uint32_t need_downsizer;
	uint32_t dual_cam;
	uint32_t scene_mode;
	uint32_t is_slow_motion;
	uint32_t skip_number;/*cap skip*/
	uint32_t capture_mode;/*multiple or single frame*/
	struct dcam_cap_sync_pol sync_pol;
	struct camera_size cap_in_size;
	struct camera_rect cap_in_rect;
	struct camera_size cap_out_size;
	struct camera_size sn_max_size;
	struct camera_size dst_size;
	struct camera_rect path_input_rect;
	struct camera_path_spec cam_path[CAMERA_MAX_PATH];
	struct dcam_cap_dec img_deci;
};

struct camera_dev {
	enum dcam_id idx;
	struct mutex cam_mutex;
	struct camera_context cam_ctx;/*store info from user ioctl*/
	uint32_t use_path;
	atomic_t stream_on;
	atomic_t run_flag;
	struct completion irq_com;
	struct cam_buf_queue queue;/*message queue with user*/
	struct timer_list cam_timer;
	struct flash_led_task *flash_task;

	struct isp_statis_buf_input init_inptr;
	void *isp_dev_handle;
	enum isp_id isp_id;/* attached isp ch id */
	uint32_t cap_flag;
	uint32_t raw_cap;
	uint32_t raw_phase;
	uint32_t zoom_ratio;
	uint32_t isp_work;
	struct isp_dev_fetch_info fetch_info;
	struct camera_group *grp;
	uint32_t need_downsizer;
};

struct camera_group {
	struct mutex grp_mutex;
	uint32_t dev_inited;/*dev alloc flag*/
	uint32_t mode_inited;/*dev used flag*/
	uint32_t fetch_inited;/*dev used flag*/
	uint32_t dcam_res_used;
	uint32_t dcam_count;/*dts cfg dcam count*/
	uint32_t isp_count;/*dts cfg isp count*/
	atomic_t camera_opened;
	struct camera_dev *dev[CAMERA_MAX_COUNT];
	struct ion_client *cam_ion_client[CAMERA_MAX_COUNT];
	atomic_t dcam_run_count;
	atomic_t isp_run_count;
	struct miscdevice *md;
	struct platform_device *pdev;
	struct wake_lock wakelock;
	struct completion fetch_com;
	void *dump_work;
	uint32_t dump_dcamraw;
	uint32_t need_downsizer;
};

struct camera_file {
	int idx;/*attached cam index*/
	struct camera_group *grp;
};

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

static struct miscdevice image_dev;

#define FEATRUE_DCAM_IOCTRL
#include "cam_ioctl.c"
#undef FEATRUE_DCAM_IOCTRL

static int sprd_camcore_buf_queue_init(struct camera_dev *dev)
{
	int ret = 0;
	uint32_t i = 0;
	struct camera_context *ctx = NULL;
	struct camera_path_spec *path = NULL;

	if (dev == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	ctx = &dev->cam_ctx;

	ret = sprd_cam_queue_buf_init(&dev->queue,
		sizeof(struct camera_node),
		CAMERA_QUEUE_LENGTH, "img msg queue");
	if (unlikely(ret != 0)) {
		pr_err("fail to init queue\n");
		return -ENOMEM;
	}

	for (i = 0; i < CAMERA_MAX_PATH; i++) {
		path = &ctx->cam_path[i];
		if (path == NULL) {
			pr_err("fail to init path %d\n", i);
			sprd_cam_queue_buf_deinit(&dev->queue);
			return -EINVAL;
		}
		sprd_cam_queue_buf_init(&path->buf_queue,
			sizeof(struct camera_addr),
			DCAM_FRM_QUEUE_LENGTH * 2, "img path buf queue");
	}
	pr_debug("sprd_img:buf queue init handle end!\n");

	return 0;
}

static int sprd_camcore_buf_queue_deinit(struct camera_dev *dev)
{
	uint32_t i = 0;
	struct camera_context *ctx = NULL;
	struct camera_path_spec *path = NULL;

	if (dev == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	ctx = &dev->cam_ctx;

	for (i = 0; i < CAMERA_MAX_PATH; i++) {
		path = &ctx->cam_path[i];
		if (path == NULL) {
			pr_err("fail to init path %d\n", i);
			return -EINVAL;
		}
		sprd_cam_queue_buf_deinit(&path->buf_queue);
	}

	sprd_cam_queue_buf_deinit(&dev->queue);
	pr_debug("sprd_img: deinit handle end!\n");

	return 0;
}

static int sprd_camcore_dev_init(struct camera_group *group,
	enum dcam_id idx)
{
	int ret = 0;
	struct camera_dev *dev = NULL;

	if (!group) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	if (group->dev_inited & (1 << (int)idx)) {
		pr_info("sprd_img: dev%d already inited\n", idx);
		return 0;
	}

	dev = vzalloc(sizeof(struct camera_dev));
	if (!dev) {
		ret = -ENOMEM;
		pr_err("fail to alloc camera dev\n");
		goto vzalloc_exit;
	}
	atomic_set(&dev->run_flag, 1);

	dev->idx = idx;
	mutex_init(&dev->cam_mutex);
	atomic_set(&dev->stream_on, 0);
	init_completion(&dev->irq_com);

	ret = sprd_camcore_buf_queue_init(dev);
	if (unlikely(ret != 0)) {
		pr_err("fail to init path buf queue\n");
		ret = -ENOMEM;
		goto buf_queue_exit;
	}
	sprd_camioctl_timer_init(&dev->cam_timer, (unsigned long)dev);

	ret = sprd_cam_flash_task_create(&dev->flash_task, idx);
	if (unlikely(ret != 0)) {
		pr_err("fail to create flash service\n");
		ret = -EINVAL;
		goto flash_exit;
	}

	ret = sprd_dcam_drv_dev_init(idx);
	if (unlikely(ret != 0)) {
		pr_err("fail to init DCAM_ID_%d dev\n", idx);
		ret = -EINVAL;
		goto dcam_exit;
	}

	if (idx != DCAM_ID_2)
		dev->isp_id = (enum isp_id)idx;
	else
		dev->isp_id = (enum isp_id)ISP_ID_1;

	if (dev->isp_id < ISP_ID_MAX) {
		ret = sprd_isp_drv_dev_init(&dev->isp_dev_handle,
			dev->isp_id);
		if (unlikely(ret != 0)) {
			pr_err("fail to init ISP_ID_%d dev\n", idx);
			ret = -EINVAL;
			goto isp_exit;
		}
	}

	group->dev[idx] = dev;
	group->dev_inited |= 1 << idx;
	dev->grp = group;

	return ret;

isp_exit:
	sprd_dcam_drv_dev_deinit(idx);
dcam_exit:
	sprd_cam_flash_task_destroy(dev->flash_task);
flash_exit:
	sprd_camioctl_timer_stop(&dev->cam_timer);
	sprd_camcore_buf_queue_deinit(dev);
buf_queue_exit:
	vfree(dev);
vzalloc_exit:
	return ret;
}

static void sprd_camcore_dev_deinit(struct camera_group *group,
	enum dcam_id idx)
{
	struct camera_dev *dev = NULL;

	dev = group->dev[idx];

	mutex_lock(&dev->cam_mutex);
	atomic_set(&dev->stream_on, 0);

	if (dev->isp_id < ISP_ID_MAX)
		sprd_isp_drv_dev_deinit(dev->isp_dev_handle);

	sprd_dcam_drv_dev_deinit(idx);
	sprd_cam_flash_task_destroy(dev->flash_task);
	sprd_camioctl_timer_stop(&dev->cam_timer);
	sprd_cam_queue_buf_deinit(&dev->queue);
	sprd_camcore_buf_queue_deinit(dev);

	group->dev[idx] = NULL;
	group->dev_inited &= ~((1 << idx) - 1);
	mutex_unlock(&dev->cam_mutex);
	mutex_destroy(&dev->cam_mutex);

	vfree(dev);
}

static int sprd_camcore_open(struct inode *node, struct file *file)
{
	int ret = 0;
	struct camera_file *camerafile = NULL;
	struct camera_group *grp = NULL;
	struct miscdevice *md = file->private_data;
	uint32_t count = 0;

	grp = md->this_device->platform_data;
	count = grp->dcam_count;

	if (atomic_inc_return(&grp->camera_opened) > count) {
		pr_err("fail to open camera: the camera has been all used %d\n",
			atomic_read(&grp->camera_opened));
		return -ENOMEM;
	}

	if (count == 0 || count > CAMERA_MAX_COUNT) {
		pr_err("fail to get valid dts tree config count\n");
		ret = -ENODEV;
		goto vzalloc_fail;
	}

	camerafile = vzalloc(sizeof(struct camera_file));
	if (camerafile == NULL) {
		ret = -ENOMEM;
		goto vzalloc_fail;
	}

	camerafile->grp = grp;
	if (atomic_read(&grp->camera_opened) == 1) {
		mutex_lock(&grp->grp_mutex);
		grp->dump_work =
			sprd_cam_debug_init(&grp->dump_dcamraw,
				&grp->need_downsizer);
		ret = sprd_camcore_dev_init(grp, DCAM_ID_0);
		if (unlikely(ret != 0)) {
			pr_err("fail to init camera 0\n");
			ret = -EINVAL;
			goto dev0_fail;
		}

		if (count > 1) {
			ret = sprd_camcore_dev_init(grp, DCAM_ID_1);
			if (unlikely(ret != 0)) {
				pr_err("fail to init camera 1\n");
				ret = -EINVAL;
				goto dev1_fail;
			}
		}

		if (count > 2) {
			ret = sprd_camcore_dev_init(grp, DCAM_ID_2);
			if (unlikely(ret != 0)) {
				pr_err("fail to init camera 2\n");
				ret = -EINVAL;
				goto dev2_fail;
			}
		}
		grp->mode_inited = 0;
		atomic_set(&grp->dcam_run_count, 0);
		atomic_set(&grp->isp_run_count, 0);
		mutex_unlock(&grp->grp_mutex);
	}

	file->private_data = (void *)camerafile;
	wake_lock(&grp->wakelock);
	pr_info("Camera open success!\n");

	return ret;

dev2_fail:
	sprd_camcore_dev_deinit(grp, DCAM_ID_1);
dev1_fail:
	sprd_camcore_dev_deinit(grp, DCAM_ID_0);
dev0_fail:
	mutex_unlock(&grp->grp_mutex);
	vfree(camerafile);
vzalloc_fail:
	atomic_dec_return(&grp->camera_opened);
	pr_err("fail to alloc memory for camerafile\n");

	return ret;
}

static int sprd_camcore_release(struct inode *node, struct file *file)
{
	int i = 0;
	struct camera_file *camerafile = NULL;
	struct camera_group *group = NULL;
	struct camera_dev *dev = NULL;

	camerafile = file->private_data;
	if (!camerafile) {
		pr_err("fail to get valid camerafile\n");
		goto exit;
	}
	group = camerafile->grp;

	if (atomic_dec_return(&group->camera_opened) == 0) {
		mutex_lock(&group->grp_mutex);
		for (i = 0; i < group->dcam_count; i++) {
			dev = group->dev[i];
			if (!dev || !(group->dev_inited & (1 << i))) {
				pr_info("sprd_img: dev%d already deinited\n",
					i);
				continue;
			}
			if (group->dev_inited & (1 << i))
				complete(&dev->irq_com);
			sprd_camioctl_io_stream_off(camerafile, dev, 0);
			if (i < ISP_MAX_COUNT)
				sprd_isp_drv_module_dis(dev->isp_dev_handle);
			sprd_dcam_drv_module_dis(i);
			sprd_camcore_dev_deinit(group, i);
		}
		group->dcam_res_used = 0;
		group->dev_inited = 0;
		group->mode_inited = 0;
		sprd_cam_debug_deinit(group->dump_work);
		mutex_unlock(&group->grp_mutex);
	}
	vfree(camerafile);
	file->private_data = NULL;
	wake_unlock(&group->wakelock);

	pr_info("Camera close success!\n");
exit:
	return 0;
}

static long sprd_camcore_ioctl(struct file *file, uint32_t cmd,
			unsigned long arg)
{
	int ret = 0;
	struct camera_dev *dev = NULL;
	struct camera_context *ctx = NULL;
	struct camera_file *camerafile = NULL;
	enum dcam_id idx = DCAM_ID_0;
	dcam_io_fun io_ctrl = NULL;

	camerafile = (struct camera_file *)file->private_data;
	if (!camerafile) {
		ret = -EFAULT;
		pr_err("fail to get valid input ptr\n");
		goto exit;
	}

	idx = camerafile->idx;

	ret = sprd_camioctl_dev_get(camerafile, &dev, &ctx);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}

	pr_debug("cam ioctl: %d, cmd: 0x%x, %s 0x%x\n",
			idx, cmd,
			sprd_camioctl_get_str(cmd),
			sprd_camioctl_get_val(cmd));

	io_ctrl = sprd_camioctl_get_fun(cmd);
	if (io_ctrl != NULL) {
		ret = io_ctrl(camerafile, dev, arg);
		if (ret) {
			pr_err("fail to cmd %d\n", _IOC_NR(cmd));
			goto exit;
		}
	} else {
		pr_debug("fail to get valid cmd 0x%x 0x%x %s\n", cmd,
			sprd_camioctl_get_val(cmd),
			sprd_camioctl_get_str(cmd));
	}

	CAM_TRACE("cam ioctl end: %d, cmd: 0x%x, %d\n",
			idx, cmd, _IOC_NR(cmd));
exit:
	return ret;
}

static ssize_t sprd_camcore_read(struct file *file,
	char __user *u_data, size_t cnt, loff_t *cnt_ret)
{
	int ret = 0, i = 0;
	struct camera_file *camerafile = file->private_data;
	enum dcam_id idx = camerafile->idx;
	struct camera_group *group = camerafile->grp;
	struct camera_dev *dev = group->dev[idx];
	struct camera_path_spec *path;
	struct sprd_img_read_op read_op;
	struct camera_node node;
	struct cam_path_info *info;
	struct sprd_img_path_info *info_ret;
	struct cam_path_capability path_capability;
	struct completion *irq_com = NULL;

	if (cnt < sizeof(struct sprd_img_read_op)) {
		pr_err("fail to img read, cnt %zd read_op %d\n", cnt,
		(int32_t)sizeof(struct sprd_img_read_op));
		return -EIO;
	}

	if (copy_from_user(&read_op, (void __user *)u_data, cnt)) {
		pr_err("fail to get user info\n");
		return -EFAULT;
	}

	if (!dev) {
		pr_err("fail to get inited dev %p\n", dev);
		return -EFAULT;
	}

	switch (read_op.cmd) {
	case SPRD_IMG_GET_SCALE_CAP:
		read_op.parm.reserved[0] = ISP_PATH2_LINE_BUF_LENGTH;
		read_op.parm.reserved[1] = CAMERA_SC_COEFF_UP_MAX;
		read_op.parm.reserved[2] = DCAM_SCALING_THRESHOLD;
		CAM_TRACE("line threshold %d, sc factor %d, scaling %d.\n",
			read_op.parm.reserved[0], read_op.parm.reserved[1],
			read_op.parm.reserved[2]);
		break;
	case SPRD_IMG_GET_FRM_BUFFER:
		irq_com = &dev->irq_com;
		memset(&read_op, 0x00, sizeof(read_op));
		while (1) {
			ret = wait_for_completion_interruptible(irq_com);
			if (ret == 0) {
				break;
			} else if (ret == -ERESTARTSYS) {
				read_op.evt = IMG_SYS_BUSY;
				ret = DCAM_RTN_SUCCESS;
				goto read_end;
			} else {
				pr_err("read frame buf, fail to down, %d\n",
				ret);
				WARN_ON(1);
				return -EPERM;
			}
		}

		if (sprd_cam_queue_buf_read(&dev->queue, &node)) {
			CAM_TRACE("fail to read frame buffer queue\n");
			read_op.evt = IMG_SYS_BUSY;
			ret = DCAM_RTN_SUCCESS;
			goto read_end;
		} else {
			if (node.invalid_flag) {
				CAM_TRACE("fail to get valid node\n");
				read_op.evt = IMG_SYS_BUSY;
				ret = DCAM_RTN_SUCCESS;
				goto read_end;
			}
		}

		read_op.evt = node.irq_flag;
		if (read_op.evt == IMG_TX_DONE) {
			read_op.parm.frame.channel_id = node.f_type;
			path = &dev->cam_ctx.cam_path
				[read_op.parm.frame.channel_id];
			read_op.parm.frame.index = path->frm_id_base;
			read_op.parm.frame.height = node.height;
			read_op.parm.frame.length = node.reserved[0];
			read_op.parm.frame.sec = node.time.timeval.tv_sec;
			read_op.parm.frame.usec = node.time.timeval.tv_usec;
			read_op.parm.frame.monoboottime =
				node.time.boot_time.tv64;
			read_op.parm.frame.reserved[0] =
				node.dual_info.is_last_frm;
			read_op.parm.frame.reserved[1] =
				node.dual_info.time_diff;
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
			read_op.parm.frame.zoom_ratio = node.zoom_ratio;
			read_op.parm.frame.mfd = node.mfd[0];
			CAM_TRACE("index %d real_index %d base_id %d mfd %x\n",
				read_op.parm.frame.index,
				read_op.parm.frame.real_index,
				read_op.parm.frame.frm_base_id,
				read_op.parm.frame.mfd);
		} else {
			if (read_op.evt == IMG_TIMEOUT ||
				read_op.evt == IMG_TX_ERR) {
				pr_err("fail to get right evt %d\n",
					read_op.evt);
				read_op.parm.frame.irq_type = node.irq_type;
				read_op.parm.frame.irq_property
					= node.irq_property;
				csi_api_reg_trace();
				sprd_dcam_drv_reg_trace(idx);
			}
		}
		CAM_TRACE("read frame buf, evt 0x%x channel 0x%x index 0x%x\n",
			read_op.evt, read_op.parm.frame.channel_id,
			read_op.parm.frame.index);

		if (read_op.parm.frame.irq_property == IRQ_RAW_CAP_DONE) {
			pr_info("raw capture\n");
			if (dev->raw_cap == 1) {
				sprd_camioctl_dcam_fetch_stop(dev->idx,
					dev->idx, group);
				ret = sprd_cam_queue_buf_clear
					(&dev->queue);
				if (unlikely(ret != 0))
					pr_err("fail to init queue\n");
				ret = sprd_camioctl_isp_unreg_isr(dev);
				if (unlikely(ret))
					pr_err("fail to register isp isr\n");
				sprd_dcam_drv_reset(idx, 0);
				dev->raw_cap = 0;
				atomic_set(&dev->stream_on, 0);
				complete(&group->fetch_com);
			}
		}
		if (read_op.evt == IMG_TX_STOP)
			pr_info("tx stop\n");
		if (read_op.parm.frame.channel_id == CAMERA_PDAF_PATH)
			pr_info("pdaf\n");

		break;
	case SPRD_IMG_GET_PATH_CAP:
		CAM_TRACE("get path capbility\n");
		sprd_camioctl_path_capability_get(&path_capability);
		read_op.parm.capability.count = path_capability.count;
		read_op.parm.capability.support_3dnr_mode =
			path_capability.support_3dnr_mode;
		read_op.parm.capability.support_4in1 =
			path_capability.support_4in1;
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

static ssize_t sprd_camcore_write(struct file *file, const char __user *u_data,
			size_t cnt, loff_t *cnt_ret)
{
	int ret = 0;
	struct camera_file *camerafile = file->private_data;
	enum dcam_id idx = camerafile->idx;
	struct camera_group *group = camerafile->grp;
	struct camera_dev *dev = group->dev[idx];
	struct sprd_img_write_op write_op;

	if (cnt < sizeof(struct sprd_img_write_op)) {
		pr_err("fail to write, cnt %zd read_op %d\n", cnt,
		(int32_t)sizeof(struct sprd_img_write_op));
		return -EIO;
	}

	if (copy_from_user(&write_op, (void __user *)u_data, cnt)) {
		pr_err("fail to get user info\n");
		return -EFAULT;
	}

	if (!dev)
		return -EFAULT;

	switch (write_op.cmd) {
	case SPRD_IMG_STOP_DCAM:
		if (!dev) {
			pr_err("fail to get valid dev:NULL\n");
			return -EFAULT;
		}
		mutex_lock(&dev->cam_mutex);
		ret = sprd_camioctl_tx_stop(dev, write_op.sensor_id);
		mutex_unlock(&dev->cam_mutex);
		break;
	default:
		pr_err("fail to get valid cmd!\n");
		ret = -EINVAL;
		break;
	}

	if (ret)
		cnt = ret;

	return cnt;
}

static const struct file_operations image_fops = {
	.open = sprd_camcore_open,
	.unlocked_ioctl = sprd_camcore_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = sprd_cam_drv_compat_ioctl,
#endif
	.release = sprd_camcore_release,
	.read = sprd_camcore_read,
	.write = sprd_camcore_write,
};

static struct miscdevice image_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = IMG_DEVICE_NAME,
	.fops = &image_fops,
};

static int sprd_camcore_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct camera_group *group = NULL;

	if (!pdev) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	pr_info("Start camera img probe\n");
	group = vzalloc(sizeof(struct camera_group));
	if (group == NULL)
		return -ENOMEM;

	ret = misc_register(&image_dev);
	if (ret) {
		pr_err("fail to register misc devices, ret %d\n", ret);
		return -EACCES;
	}

	image_dev.this_device->of_node = pdev->dev.of_node;
	image_dev.this_device->platform_data = (void *)group;
	group->md = &image_dev;
	group->pdev = pdev;
	atomic_set(&group->camera_opened, 0);
	wake_lock_init(&group->wakelock, WAKE_LOCK_SUSPEND,
		"Camera Sys Wakelock");
	mutex_init(&group->grp_mutex);
	init_completion(&group->fetch_com);
	complete(&group->fetch_com);

	pr_info("sprd img probe pdev name %s\n", pdev->name);
	ret = sprd_cam_pw_domain_init(pdev);
	if (ret) {
		pr_err("fail to init pw domain\n");
		goto exit;
	}

	pr_info("sprd dcam dev name %s\n", pdev->dev.init_name);
	ret = sprd_dcam_drv_dt_parse(pdev, &group->dcam_count);
	if (ret) {
		pr_err("fail to parse dcam dts\n");
		goto exit;
	}

	pr_info("sprd isp dev name %s\n", pdev->dev.init_name);
	ret = sprd_isp_drv_dt_parse(pdev->dev.of_node, &group->isp_count);
	if (ret) {
		pr_err("fail to parse isp dts\n");
		goto exit;
	}

	if (sprd_dcam_drv_init(pdev)) {
		pr_err("fail to call sprd_dcam_drv_init\n");
		ret = -EINVAL;
		goto exit;
	}

	if (sprd_isp_drv_init(pdev)) {
		pr_err("fail to call sprd_isp_drv_init\n");
		misc_deregister(&image_dev);
		sprd_dcam_drv_deinit();
		return -EINVAL;
	}

	sprd_cam_debug_create_attrs_group(image_dev.this_device);

	return ret;

exit:
	misc_deregister(&image_dev);
	return ret;
}

static int sprd_camcore_remove(struct platform_device *pdev)
{
	struct camera_group *group = NULL;

	if (!pdev) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	sprd_cam_debug_remove_attrs_group(image_dev.this_device);

	group = image_dev.this_device->platform_data;
	sprd_isp_drv_deinit();
	sprd_dcam_drv_deinit();
	wake_lock_destroy(&group->wakelock);
	mutex_destroy(&group->grp_mutex);
	vfree(image_dev.this_device->platform_data);
	misc_deregister(&image_dev);

	return 0;
}

static void sprd_camcore_shutdown(struct platform_device *pdev)
{
	struct sprd_img_set_flash set_flash;

	set_flash.led0_ctrl = 1;
	set_flash.led1_ctrl = 1;
	set_flash.led0_status = FLASH_CLOSE;
	set_flash.led1_status = FLASH_CLOSE;
	set_flash.flash_index = 0;
	sprd_cam_flash_set(DCAM_ID_0, &set_flash);
	set_flash.flash_index = 1;
	sprd_cam_flash_set(DCAM_ID_0, &set_flash);
}

static const struct of_device_id sprd_dcam_of_match[] = {
	{ .compatible = "sprd,dcam", },
	{},
};

static struct platform_driver sprd_img_driver = {
	.probe = sprd_camcore_probe,
	.remove = sprd_camcore_remove,
	.shutdown = sprd_camcore_shutdown,
	.driver = {
		.name = IMG_DEVICE_NAME,
		.of_match_table = of_match_ptr(sprd_dcam_of_match),
	},
};

module_platform_driver(sprd_img_driver);

MODULE_DESCRIPTION("Sprd CAM Driver");
MODULE_AUTHOR("Multimedia_Camera@Spreadtrum");
MODULE_LICENSE("GPL");
