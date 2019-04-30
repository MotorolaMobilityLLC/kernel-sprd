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

#include <linux/uaccess.h>
#include <video/sprd_mm.h>

#include "isp_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CAPABILITY: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define ISP_MAX_SLICE_WIDTH       4672
#define ISP_MAX_SLICE_HEIGHT      3504

static int isp_k_capability_continue_size(struct isp_capability *param)
{
	int ret = 0;
	struct isp_img_size size = {0, 0};

	size.width = ISP_MAX_SLICE_WIDTH;
	size.height = ISP_MAX_SLICE_HEIGHT;

	ret = copy_to_user(param->property_param,
			(void *)&size, sizeof(size));
	if (ret != 0) {
		ret = -1;
		pr_err("fail to copy from user, ret = %d\n", ret);
	}

	return ret;
}

static int isp_k_capability_time(struct isp_capability *param)
{
	int ret = 0;
	struct isp_time time;
	struct timespec ts;

	ktime_get_ts(&ts);
	time.sec = ts.tv_sec;
	time.usec = ts.tv_nsec / NSEC_PER_USEC;

	ret = copy_to_user(param->property_param,
		(void *)&time, sizeof(time));
	if (ret != 0) {
		ret = -1;
		pr_err("fail to copy from user, ret = %d\n", ret);
	}

	return ret;
}

int isp_capability(void *param)
{
	int ret = 0;
	struct isp_capability cap_param = {0, 0, NULL};

	if (!param) {
		pr_err("fail to get param\n");
		return -1;
	}

	ret = copy_from_user((void *)&cap_param,
			(void *)param, sizeof(cap_param));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	if (cap_param.property_param == NULL) {
		pr_err("fail to get property_param\n");
		return -1;
	}

	switch (cap_param.index) {
	case ISP_CAPABILITY_CONTINE_SIZE:
		ret = isp_k_capability_continue_size(&cap_param);
		break;
	case ISP_CAPABILITY_TIME:
		ret = isp_k_capability_time(&cap_param);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			cap_param.index);
		break;
	}

	return ret;
}
