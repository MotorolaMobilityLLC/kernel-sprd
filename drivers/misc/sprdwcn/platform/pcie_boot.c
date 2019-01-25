/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <misc/marlin_platform.h>

#include "pcie.h"
#include "pcie_boot.h"
#include "pcie_dbg.h"
#include "wcn_log.h"

#define FIRMWARE_PATH "/dev/block/platform/soc/soc:ap-apb/71400000.sdio/by-name/wcnmodem"
#define FIRMWARE_SIZE_MAX 0xf0c00

static char *load_firmware_data(int size)
{
	int read_len;
	char *buffer = NULL, *data = NULL;
	struct file *file;
	loff_t offset = 0;

	PCIE_INFO("%s enter,size=0X%x", __func__, size);
	file = filp_open(FIRMWARE_PATH, O_RDONLY, 0);

	if (IS_ERR(file)) {
		PCIE_INFO("firmware open fail errno=%d", IS_ERR(file));
		return NULL;
	}

	buffer = vmalloc(size);
	if (!buffer) {
		fput(file);
		PCIE_INFO("no memory for image\n");
		return NULL;
	}

	data = buffer;
	do {
		read_len = kernel_read(file, buffer, size, &offset);
		if (read_len > 0) {
			size -= read_len;
			buffer += read_len;
		}

		PCIE_INFO("size=0X%x, read_len=0X%x", size, read_len);
	} while ((read_len > 0) && (size > 0));

	fput(file);
	PCIE_INFO("%s finish", __func__);

	return data;

}
int wcn_boot_init(struct wcn_pcie_info *pcie_info)
{
	unsigned int reg_val = 0;
	char *buffer = NULL;
	char a[10];
	int i;
	static int dbg_cnt;

	PCIE_INFO("%s enter\n", __func__);

retry:
	buffer = load_firmware_data(FIRMWARE_SIZE_MAX);
	if (!buffer && ((dbg_cnt++) < 10)) {
		PCIE_INFO("%s: can't download firmware, retry %d\n",
			  __func__, dbg_cnt);
		msleep(3000);
		goto retry;
	}
	if (!buffer) {
		PCIE_INFO("%s: can't download firmware, return -1\n", __func__);
		return -1;
	}

	/* download firmware */
	sprd_pcie_bar_map(pcie_info, 0, 0x40400000);
	pcie_bar_write(pcie_info, 0, 0x100000, buffer, FIRMWARE_SIZE_MAX);
	pcie_bar_read(pcie_info, 0, 0x100000, a, 10);
	for (i = 0; i < 10; i++)
		PCIE_INFO("a[%d]= 0x%x\n", i, a[i]);

	sprd_pcie_bar_map(pcie_info, 0, 0x40000000);
	/* release cpu */
	pcie_bar_read(pcie_info, 0, 0x88288, (char *)&reg_val, 0x4);
	PCIE_INFO("-->reset reg is %d\n", reg_val);
	reg_val = 0;
	pcie_bar_write(pcie_info, 0, 0x88288, (char *)&reg_val, 0x4);
	PCIE_INFO("<--reset reg is %d\n", reg_val);
	vfree(buffer);
	PCIE_INFO("%s ok\n", __func__);

	return 0;
}
EXPORT_SYMBOL(wcn_boot_init);

int pcie_boot(enum marlin_sub_sys subsys)
{
	struct wcn_pcie_info *pdev;

	pdev = get_wcn_device_info();
	if (!pdev) {
		PCIE_ERR("%s:maybe PCIE device link error\n", __func__);
		return -1;
	}
	if (wcn_boot_init(pdev) < 0) {
		PCIE_ERR("%s: call wcn_boot_init fail\n", __func__);
		return -1;
	}

	return 0;
}

