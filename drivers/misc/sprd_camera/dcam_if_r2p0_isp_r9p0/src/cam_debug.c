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

#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <asm/unaligned.h>
#include <linux/sprd_iommu.h>
#include <linux/sprd_ion.h>
#include <linux/slab.h>

#include "ion.h"
#include "cam_common.h"

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "cam_debug: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define CAMERA_DATA_FILE "/data/ylog"

struct dump_work_t {
	struct work_struct wk;
	struct workqueue_struct *dump_wq;
	struct camera_frame frame[2];
	struct mutex dump_mutex;
	uint32_t dump_fullpathraw_enable;
	uint32_t dump_binpathraw_enable;
};

static uint32_t dump_dcamraw_flag;
static uint32_t debug_rds_enable;

/* $ sudo adb shell */
/* # setenforce 0 */
/* # cd /sys/class/misc/sprd_image */
/* # echo 1 > sprd_camdebug_dumpraw */
/* # echo 1 > sprd_camdebug_rds */

static ssize_t sprd_camdebug_dumpraw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", dump_dcamraw_flag);
}

static ssize_t sprd_camdebug_dumpraw_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	uint32_t ret;
	unsigned long data;

	ret = kstrtoul(buf, 0, &data);
	if (ret)
		return -EINVAL;

	dump_dcamraw_flag = (uint32_t)data;
	pr_debug("dump_dcamraw_flag %d,\n", dump_dcamraw_flag);

	return strnlen(buf, count);
}

static ssize_t sprd_camdebug_rds_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", debug_rds_enable);
}

static ssize_t sprd_camdebug_rds_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	uint32_t ret;
	unsigned long data;

	ret = kstrtoul(buf, 0, &data);
	if (ret)
		return -EINVAL;

	debug_rds_enable = (uint32_t)data;
	pr_debug("debug_rds_enable %d\n", debug_rds_enable);

	return strnlen(buf, count);
}

static DEVICE_ATTR_RW(sprd_camdebug_dumpraw);
static DEVICE_ATTR_RW(sprd_camdebug_rds);

static struct attribute *sprd_camdebug_attrs[] = {
	&dev_attr_sprd_camdebug_dumpraw.attr,
	&dev_attr_sprd_camdebug_rds.attr,
	NULL,
};

static struct attribute_group sprd_camdebug_attrs_group = {
	.attrs = sprd_camdebug_attrs,
};

static int sprd_camdebug_write_file(struct camera_frame *frame)
{
	int ret = 0;
	char file_subfix[10] = "mipi_raw";
	char file_name[128];
	char *buf = (char *)frame->buf_info.kaddr[0];
	int size = (int)frame->buf_info.size[0];
	int nwrite = 0;
	int offset = 0;
	loff_t pos = 0;
	mm_segment_t old_fs;
	struct file *dump_fp = NULL;
	struct timeval tv = { 0 };

	sprd_cam_com_timestamp(&tv);
	memset(file_name, 0x00, sizeof(file_name));
	sprintf(file_name,
		"%s/%d_frame_%dx%d_%lld.%s",
		CAMERA_DATA_FILE,
		frame->type, frame->width, frame->height,
		tv.tv_sec * 1000000000LL + tv.tv_usec * 1000,
		file_subfix);
	pr_info("create dump file %s\n", file_name);

	dump_fp = filp_open(file_name, O_CREAT | O_RDWR, 0777);

	if (IS_ERR_OR_NULL(dump_fp)) {
		ret = -1;
		pr_err("fail to open file\n");
		return ret;
	}

	old_fs = get_fs();
	set_fs(get_ds());
	pos = (unsigned long)offset;
	nwrite =
		vfs_write(dump_fp, (__force const char __user *)buf,
			size, &pos);
	offset += nwrite;
	pr_debug("pos %d nwrite %d\n", offset, nwrite);
	set_fs(old_fs);

	return ret;
}

static void sprd_camdebug_dump(struct work_struct *work)
{
	struct dump_work_t *cxt = (struct dump_work_t *)work;
	struct camera_frame *dump_frame[2];

	memset(dump_frame, 0, sizeof(dump_frame));

	if (cxt->dump_fullpathraw_enable) {
		if (cxt->frame[0].type == 0) {
			dump_frame[0] = &cxt->frame[0];
			if (dump_frame[0]->buf_info.type != CAM_BUF_KERNEL_TYPE)
				dump_frame[0]->buf_info.kaddr[0] =
				(uint32_t *)sprd_ion_map_kernel(
					dump_frame[0]->buf_info.dmabuf_p[0], 0);
			sprd_camdebug_write_file(dump_frame[0]);
		}
	}
	if (cxt->dump_binpathraw_enable) {
		if (cxt->frame[1].type == 1) {
			dump_frame[1] = &cxt->frame[1];
			if (dump_frame[1]->buf_info.type != CAM_BUF_KERNEL_TYPE)
				dump_frame[1]->buf_info.kaddr[0] =
				(uint32_t *)sprd_ion_map_kernel(
					dump_frame[1]->buf_info.dmabuf_p[0], 0);
			sprd_camdebug_write_file(dump_frame[1]);
		}
	}

#if 0
	pr_debug
		("y uv,0x%x 0x%x,mfd 0x%x,0x%x,iova 0x%x\n",
		(int)dump_frame->yaddr, (int)dump_frame->uaddr,
		(int)dump_frame->buf_info.mfd[0],
		(int)dump_frame->buf_info.mfd[1],
		(int)dump_frame->buf_info.iova[0]);
	pr_debug("num 0x%x,size 0x%x,addr%p\n",
		(int)dump_frame->buf_info.num,
		(int)dump_frame->buf_info.size[0],
		dump_frame->buf_info.kaddr[0]);

	for (i = 0; i < 64 / 4; i += 1) {
		pr_debug("kaddr[%d]= %x\n", 4 * i,
			*(dump_frame->buf_info.kaddr[0] + 4 * i));
		pr_debug("kaddr[%d]= %x\n", 4 * i + 1,
			*(dump_frame->buf_info.kaddr[0] + 4 * i + 1));
		pr_debug("kaddr[%d]= %x\n", 4 * i + 2,
			*(dump_frame->buf_info.kaddr[0] + 4 * i + 2));
		pr_debug("kaddr[%d]= %x\n", 4 * i + 3,
			*(dump_frame->buf_info.kaddr[0] + 4 * i + 3));
	}
#endif
}

void *sprd_cam_debug_init(uint32_t *dump_flag, uint32_t *need_rds)
{
	/* work queue to dump frame data */
	struct dump_work_t *cxt = NULL;

	if (!dump_flag || !need_rds)
		return NULL;

	*dump_flag = dump_dcamraw_flag;
	*need_rds = debug_rds_enable;
	if (!dump_dcamraw_flag && !debug_rds_enable)
		return NULL;

	cxt = kzalloc(sizeof(*cxt), GFP_KERNEL);
	if (!cxt)
		return NULL;

	memset(cxt, 0, sizeof(*cxt));

	INIT_WORK(&cxt->wk, sprd_camdebug_dump);
	cxt->dump_wq = create_workqueue("dump_frame_wq");
	mutex_init(&cxt->dump_mutex);

	return cxt;
}

int sprd_cam_debug_create_attrs_group(struct device *dev)
{
	int ret = 0;

	ret = sysfs_create_group(&dev->kobj,
		&sprd_camdebug_attrs_group);
	if (ret) {
		pr_err("fail to enable to export camdebug sysfs\n");
		return ret;
	}

	return ret;
}

void sprd_cam_debug_remove_attrs_group(struct device *dev)
{
	sysfs_remove_group(&dev->kobj,
		&sprd_camdebug_attrs_group);
}

void sprd_cam_debug_dump(void *handle, struct camera_frame *frame)
{
	struct dump_work_t *cxt = (struct dump_work_t *)handle;

	if (!cxt || !frame)
		return;

	mutex_lock(&cxt->dump_mutex);
	if (frame->type == 0) {
		cxt->frame[0] = *frame;
		cxt->dump_fullpathraw_enable = 1;
	} else {
		cxt->frame[1] = *frame;
		cxt->dump_binpathraw_enable = 1;
	}
	queue_work(cxt->dump_wq, &cxt->wk);
	mutex_unlock(&cxt->dump_mutex);
}

void sprd_cam_debug_deinit(void *handle)
{
	struct dump_work_t *cxt = (struct dump_work_t *)handle;

	if (!cxt)
		return;

	destroy_workqueue(cxt->dump_wq);
	mutex_destroy(&cxt->dump_mutex);
	kfree(handle);
}
