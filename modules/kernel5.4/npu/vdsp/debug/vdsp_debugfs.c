/*
 * SPDX-FileCopyrightText: 2020-2022 Unisoc (Shanghai) Technologies Co., Ltd
 * SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
 *
 * Copyright 2020-2022 Unisoc (Shanghai) Technologies Co., Ltd.
 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include "vdsp_debugfs.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: debugfs %d: %d %s:" \
	fmt, current->pid, __LINE__, __func__

struct xrp_debug_para {
	unsigned int vdsp_log_mode;
	unsigned int vdsp_log_level;
	unsigned int vdsp_dvfs_level;
	unsigned int vdsp_trace_mem;
};

static struct dentry *root_d;
static struct xrp_debug_para xrp_para;

int vdsp_debugfs_init(void)
{
	pr_debug("vdsp_debugfs: initialing...\n");
	memset(&xrp_para, 0, sizeof(struct xrp_debug_para));

	root_d = debugfs_create_dir("sprd_vdsp", NULL);
	if (!root_d) {
		pr_err("vdsp_debugfs: error create root dir\n");
		return -ENOMEM;
	}
	debugfs_create_u32("log_mode", 0664, root_d, &xrp_para.vdsp_log_mode);
	debugfs_create_u32("log_level", 0664, root_d, &xrp_para.vdsp_log_level);
	debugfs_create_u32("dvfs_level", 0664, root_d, &xrp_para.vdsp_dvfs_level);
	debugfs_create_u32("trace_mem", 0664, root_d, &xrp_para.vdsp_trace_mem);
	return 0;
}

unsigned int vdsp_debugfs_log_mode(void)
{
	return xrp_para.vdsp_log_mode;
}

unsigned int vdsp_debugfs_log_level(void)
{
	return xrp_para.vdsp_log_level;
}

unsigned int vdsp_debugfs_dvfs_level(void)
{
	return xrp_para.vdsp_dvfs_level;
}

unsigned int vdsp_debugfs_trace_mem(void)
{
	return xrp_para.vdsp_trace_mem;
}

void vdsp_debugfs_exit(void)
{
	pr_debug("vdsp_debugfs: exiting...\n");
	if (root_d)
#if 1//(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
		debugfs_remove(root_d);
#else
		debugfs_remove_recursive(root_d);
#endif
}

