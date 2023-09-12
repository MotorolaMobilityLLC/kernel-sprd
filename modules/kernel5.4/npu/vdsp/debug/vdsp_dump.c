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

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/hashtable.h>
#include "xrp_library_loader.h"
#include "xrp_internal.h"
#include "vdsp_hw.h"
#include "xvp_main.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: dump %d %d %s : "\
        fmt, current->pid, __LINE__, __func__

#define VDSP_DUMP_PATH	"/data/vendor/cameraserver/"
int32_t xrp_save_file(const char *filename, const char *buffer, uint32_t size)
{
	int ret = 0;
#if 0 //gki
	struct file *fp;
	loff_t pos = 0;

	fp = filp_open(filename, O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		pr_err("filp open failed:%s\n", filename);
		return -1;
	}
	ret = kernel_write(fp, buffer, size, &pos);
	filp_close(fp, NULL);
#endif
	return ret;
}

int32_t xrp_dump_libraries(struct xvp *xvp)
{
	struct loadlib_info *libinfo = NULL;
	struct loadlib_info *temp;
	unsigned long bkt;
	int32_t ret;
	struct xrp_known_file *p;
	struct xvp_file *xvp_file;
	void __iomem *pdram;
	char filename[128] = {'\0'};

	pr_debug("dump vdsp data start\n");
	/*dump library */
	mutex_lock(&xvp->xrp_known_files_lock);
	hash_for_each(xvp->xrp_known_files, bkt, p, node) {
		xvp_file = (struct xvp_file *)(((struct file *)(p->filp))->private_data);
		list_for_each_entry_safe(libinfo, temp, &xvp_file->load_lib_list, node_libinfo) {
			pr_debug("dump lib info, addr:%lx-libname:%s\n",
				libinfo->ion_phy, libinfo->libname);
		}
	}
	mutex_unlock(&xvp->xrp_known_files_lock);
	/*dump vdsp firmware */
	sprintf(filename, VDSP_DUMP_PATH "vdsp_firmware.dump");
	ret = xrp_save_file(filename, xvp_buf_get_vaddr(xvp->fw_buf), VDSP_FIRMWIRE_SIZE);
	if (ret != VDSP_FIRMWIRE_SIZE) {
		pr_err("dump vdsp firmware failed, ret:%d, length:0x%x\n", ret, VDSP_FIRMWIRE_SIZE);
		return -1;
	}
	/*dump vdsp dram */
	pdram = ioremap(VDSP_DRAM_ADDR, VDSP_DRAM_SIZE);
	if (unlikely(!pdram)) {
		pr_err("couldn't ioremap %x\n", VDSP_DRAM_ADDR);
		return -1;
	}
	sprintf(filename, VDSP_DUMP_PATH "vdsp_dram.dump");
	ret = xrp_save_file(filename, pdram, VDSP_DRAM_SIZE);
	iounmap(pdram);
	if (unlikely(ret != VDSP_DRAM_SIZE)) {
		pr_err("dump dram failed\n");
		return -1;
	}
	pr_debug("dump vdsp data end\n");
	return 0;
}
