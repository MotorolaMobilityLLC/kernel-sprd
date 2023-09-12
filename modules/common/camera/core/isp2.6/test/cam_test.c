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
#include <linux/fs.h>

#include "cam_test.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CAM_TEST: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define CHECK_NULL(p)	{		\
	if ((p) == NULL) {		\
		pr_err("null param\n");	\
		return -EINVAL;		\
	}				\
}

#define BYTE_PER_ONCE (4096)
#define CAMT_DUMP_PATH "/data/ylog/"

/* external function implementation */
void camt_write_image_to_file(unsigned char *buffer,
	unsigned int size, const char *file)
{
	unsigned int result = 0, total = 0, cur = 0, left = 0;
	unsigned int per = BYTE_PER_ONCE;
	struct file *wfp;

	wfp = cam_filp_open(file, O_CREAT|O_RDWR, 0666);
	if (IS_ERR_OR_NULL(wfp)) {
		pr_err("fail to open file %s\n", file);
		return;
	}
	left = size;
	pr_info("write image buf=%p, size=%d\n", buffer, left);
	do {
		cur = min(left, per);
		result = cam_kernel_write(wfp, buffer, cur, &wfp->f_pos);
		if (result > 0) {
			left -= result;
			buffer += result;
		}
		pr_debug("write result: %d, size: %d, pos: %d\n",
		(uint32_t)result, (uint32_t)size, (uint32_t)wfp->f_pos);
		total += result;
	} while ((result > 0) && (left > 0));
	filp_close(wfp, NULL);
	pr_debug("write image done, total=%d\n", total);
}

void read_image_from_file(unsigned char *buffer,
	unsigned int size, const char *file)
{
	unsigned int result = 0, cur = 0, left = 0;
	unsigned int per = BYTE_PER_ONCE;
	struct file *fp;

	fp = cam_filp_open(file, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fp)) {
		pr_err("fail to open file %s\n", file);
		return;
	}

	left = size;
	pr_info("read image buf=%p, size=%d\n", buffer, left);
	do {
		cur = min(left, per);
		result = cam_kernel_read(fp, buffer, cur, &fp->f_pos);
		if (result > 0) {
			left -= result;
			buffer += result;
		}
	} while (result > 0 && left > 0);
	filp_close(fp, 0);
	pr_debug("read image done, total=%d\n", size);
}

static int camt_init(struct cam_hw_info *hw, struct camt_info *info)
{
	int ret = 0;
	CHECK_NULL(info);

	switch (info->chip) {
	case CAMT_CHIP_DCAM0:
	case CAMT_CHIP_DCAM1:
	case CAMT_CHIP_DCAM2:
	case CAMT_CHIP_DCAM_LITE0:
	case CAMT_CHIP_DCAM_LITE1:
		ret = dcamt_init(hw, info);
		break;
	case CAMT_CHIP_ISP0:
	case CAMT_CHIP_ISP1:
		ret = ispt_init(hw, info);
		break;
	default:
		pr_err("fail to support test chip %d\n", info->chip);
		ret = -EFAULT;
		break;
	}

	pr_info("init ok\n");

	return 0;
}

static int camt_start(struct camt_info *info)
{
	int ret = 0;
	CHECK_NULL(info);

	switch (info->chip) {
	case CAMT_CHIP_DCAM0:
	case CAMT_CHIP_DCAM1:
	case CAMT_CHIP_DCAM2:
	case CAMT_CHIP_DCAM_LITE0:
	case CAMT_CHIP_DCAM_LITE1:
		ret = dcamt_start(info);
		break;
	case CAMT_CHIP_ISP0:
	case CAMT_CHIP_ISP1:
		ret = ispt_start(info);
		break;
	default:
		pr_err("fail to support test chip %d\n", info->chip);
		ret = -EFAULT;
		break;
	}

	pr_info("start ok\n");

	return 0;
}

static int camt_stop(struct camt_info *info)
{
	int ret = 0;
	CHECK_NULL(info);

	switch (info->chip) {
	case CAMT_CHIP_DCAM0:
	case CAMT_CHIP_DCAM1:
	case CAMT_CHIP_DCAM2:
	case CAMT_CHIP_DCAM_LITE0:
	case CAMT_CHIP_DCAM_LITE1:
		ret = dcamt_stop();
		break;
	case CAMT_CHIP_ISP0:
	case CAMT_CHIP_ISP1:
		ret = ispt_stop();
		break;
	default:
		pr_err("fail to support test chip %d\n", info->chip);
		ret = -EFAULT;
		break;
	}

	pr_info("stop ok\n");

	return ret;
}

static int camt_deinit(struct camt_info *info)
{
	int ret = 0;
	CHECK_NULL(info);

	switch (info->chip) {
	case CAMT_CHIP_DCAM0:
	case CAMT_CHIP_DCAM1:
	case CAMT_CHIP_DCAM2:
	case CAMT_CHIP_DCAM_LITE0:
	case CAMT_CHIP_DCAM_LITE1:
		ret = dcamt_deinit();
		break;
	case CAMT_CHIP_ISP0:
	case CAMT_CHIP_ISP1:
		ret = ispt_deinit();
		break;
	default:
		pr_err("fail to support test chip %d\n", info->chip);
		ret = -EFAULT;
		break;
	}

	pr_info("deinit ok\n");

	return ret;
}

int camt_test(struct cam_hw_info *hw, unsigned long arg)
{
	int ret = 0;
	struct camt_info info;

	CHECK_NULL(hw);
	memset(&info, 0, sizeof(info));
	ret = copy_from_user(&info, (void __user *)arg, sizeof(struct camt_info));
	if (ret) {
		pr_err("fail to get user info\n");
		return -EFAULT;
	}

	switch (info.cmd) {
	case CAMT_CMD_INIT:
		ret = camt_init(hw, &info);
		break;
	case CAMT_CMD_START:
		ret = camt_start(&info);
		break;
	case CAMT_CMD_STOP:
		ret = camt_stop(&info);
		break;
	case CAMT_CMD_DEINIT:
		ret = camt_deinit(&info);
		break;
	default:
		pr_err("fail to support test cmd %d\n", info.cmd);
		ret = -EFAULT;
		break;
	}

	return ret;
}

