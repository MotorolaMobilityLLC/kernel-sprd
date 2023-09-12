/*
 * SPDX-FileCopyrightText: 2019-2022 Unisoc (Shanghai) Technologies Co., Ltd
 * SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
 *
 * Copyright 2019-2022 Unisoc (Shanghai) Technologies Co., Ltd.
 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */

#include <linux/dma-mapping.h>
#include <linux/elf.h>
#include <linux/firmware.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/sprd_ion.h>
#include <linux/slab.h>
#include "vdsp_hw.h"
#include "xrp_internal.h"
#include "xrp_kernel_dsp_interface.h"
#include "xrp_faceid.h"
#include "vdsp_trusty.h"
#include "sprd_vdsp_mem_xvp_init.h"

#define SIGN_HEAD_SIZE (512)
#define SIGN_TAIL_SIZE (512)

 //#define PAGE_ALIGN(addr) (((addr)+PAGE_SIZE-1) & (~(PAGE_SIZE-1)))

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: faceid %d %d %s : "\
        fmt, current->pid, __LINE__, __func__

int sprd_cam_pw_on(void);
int sprd_cam_pw_off(void);
int sprd_cam_domain_eb(void);
int sprd_cam_domain_disable(void);

void *sprd_alloc_faceid_weights_buffer(struct xvp *xvp, size_t size)
{
	int align_size = PAGE_ALIGN(size);
	struct xvp_buf *tmp;

	tmp = kzalloc(sizeof(struct xvp_buf), GFP_KERNEL);
	if (!(tmp)) {
		pr_err("Error kzalloc xvp_buf failed\n");
		return NULL;
	}

	tmp->vaddr = ( void *) (( unsigned long) xvp->faceid_fw_buf->vaddr + xvp->faceid_addr_offset);
	tmp->paddr = xvp->faceid_fw_buf->paddr + xvp->faceid_addr_offset;

	xvp->faceid_addr_offset += align_size;

	pr_debug("faceid alloc paddr %lx vaddr:%lx size %ld\n", tmp->paddr, tmp->vaddr, align_size);
	return tmp;
}

void sprd_free_faceid_weights_buffer(struct xvp *xvp, struct xvp_buf *buf)
{
	buf->vaddr = NULL;
	buf->paddr = 0;
	kfree(buf);
}

int sprd_faceid_request_algo_mem(struct xvp *xvp)
{
	xvp->faceid_pool.faceid_mem_pool = sprd_alloc_faceid_weights_buffer(xvp, FACEID_MEM_SIZE);
	if (unlikely(xvp->faceid_pool.faceid_mem_pool == NULL)) {
		pr_err("request faceid mem fail\n");
		return -EFAULT;
	}
	return 0;
}

void sprd_faceid_release_algo_mem(struct xvp *xvp)
{
	sprd_free_faceid_weights_buffer(xvp, xvp->faceid_pool.faceid_mem_pool);
}

int sprd_request_weights(struct xvp *xvp, char *name, struct xvp_buf **coeff_buf)
{
	struct xvp_buf *tmp;

	int ret = request_firmware(&xvp->faceid_fw,
		name, xvp->dev);

	if (unlikely(ret < 0)) {
		pr_err("request %s weights fail\n", name);
		return ret;
	}

	tmp = sprd_alloc_faceid_weights_buffer(xvp, xvp->faceid_fw->size);
	if (unlikely(tmp == NULL)) {
		pr_err("alloc %s weights fail\n", name);
		return ret;
	}

	memcpy(tmp->vaddr, xvp->faceid_fw->data, xvp->faceid_fw->size);

	*coeff_buf = tmp;
	release_firmware(xvp->faceid_fw);

	return ret;
}

int sprd_faceid_request_weights(struct xvp *xvp)
{
	int i = 0, ret;

	char *coeff_name[FACEID_COEFF_NUM] = {"network_coeff_fa.bin",
		"network_coeff_fp.bin",
		"network_coeff_foc.bin"
	};

	struct xvp_buf **coeff_buf[FACEID_COEFF_NUM] = {
		&xvp->faceid_pool.faceid_fa_weights,
		&xvp->faceid_pool.faceid_fp_weights,
		&xvp->faceid_pool.faceid_foc_weights
	};
	for (; i < FACEID_COEFF_NUM; i++) {
		ret = sprd_request_weights(xvp, coeff_name[i], coeff_buf[i]);
		if (ret < 0)
			return ret;
	}
	sprd_faceid_request_algo_mem(xvp);

	return 0;
}

void sprd_faceid_release_weights(struct xvp *xvp)
{
	int i = 0;

	struct xvp_buf *coeff_buf[FACEID_COEFF_NUM] = {
		xvp->faceid_pool.faceid_fa_weights,
		xvp->faceid_pool.faceid_fp_weights,
		xvp->faceid_pool.faceid_foc_weights,
	};

	for (; i < FACEID_COEFF_NUM; i++) {
		sprd_free_faceid_weights_buffer(xvp, coeff_buf[i]);
	}
	sprd_faceid_release_algo_mem(xvp);
}

int sprd_alloc_faceid_combuffer(struct xvp *xvp)
{
	xvp->faceid_com_buf = kzalloc(sizeof(struct xvp_buf), GFP_KERNEL);
	if (!xvp->faceid_com_buf) {
		pr_err("Error kzalloc xvp_buf failed\n");
		return -EFAULT;
	}

	xvp->faceid_com_buf->vaddr = xvp->faceid_fw_buf->vaddr + xvp->faceid_addr_offset;
	xvp->faceid_com_buf->paddr = xvp->faceid_fw_buf->paddr + xvp->faceid_addr_offset;

	pr_debug("faceid com phyaddr %llx, vaddr %llx\n",
		xvp->faceid_com_buf->paddr, xvp->faceid_com_buf->vaddr);
	xvp->faceid_addr_offset += PAGE_SIZE;
	return 0;
}

void sprd_free_faceid_combuffer(struct xvp *xvp)
{
	xvp->faceid_com_buf->vaddr = 0;
	xvp->faceid_com_buf->paddr = 0;
	kfree(xvp->faceid_com_buf);
	xvp->faceid_com_buf = NULL;
}

static int sprd_alloc_faceid_fwbuffer(struct xvp *xvp)
{
	char *name = NULL;
	uint64_t size = 0;
	uint32_t heap_type = 0;
	uint32_t attr = 0;
	struct xvp_buf *fw_sign_buf;
	struct xvp_buf *fw_buf;
	int ret;
	struct vdsp_hw *hw = (struct vdsp_hw *)xvp->hw_arg;

	xvp->faceid_addr_offset = 0;

	ret = request_firmware(&xvp->firmware2_sign, FACEID_FIRMWARE, xvp->dev);

	if (unlikely(ret < 0)) {
		pr_err("request firmware failed ret:%d\n", ret);
		return ret;
	}

	pr_debug("request signed fw size 0x%X\n", xvp->firmware2_sign->size);

	//fw_sign_buf
	name = "xvp faceid_fw_sign_buffer";
	size = VDSP_FACEID_FIRMWIRE_SIZE / 2;
	heap_type = SPRD_VDSP_MEM_HEAP_TYPE_CARVEOUT;
	attr = SPRD_VDSP_MEM_ATTR_WRITECOMBINE;

	fw_sign_buf = xvp_buf_alloc(xvp, name, size, heap_type, attr);
	if (!fw_sign_buf) {
		pr_err("Error:alloc faceid_fw_sign_buffer faild\n");
		return -1;
	}
	xvp->faceid_fws_buf = fw_sign_buf;
	if (xvp_buf_kmap(xvp, xvp->faceid_fws_buf)) {
		xvp_buf_free(xvp, xvp->faceid_fws_buf);
		xvp->faceid_fws_buf = NULL;
		pr_err("Error: xvp_buf_kmap failed\n");
		return -EFAULT;
	}
	pr_debug("signed fw paddr %lx size %d\n", xvp->faceid_fws_buf->paddr,
		xvp->faceid_fws_buf->size);

	//fw_buf
	name = "xvp faceid_fw_buffer";
	size = hw->vdsp_reserved_mem_size - (VDSP_FACEID_FIRMWIRE_SIZE / 2);
	heap_type = SPRD_VDSP_MEM_HEAP_TYPE_CARVEOUT;
	attr = SPRD_VDSP_MEM_ATTR_WRITECOMBINE;

	fw_buf = xvp_buf_alloc(xvp, name, size, heap_type, attr);
	if (!fw_buf) {
		pr_err("Error:alloc faceid_fw_buffer faild\n");
		xvp_buf_free(xvp, fw_sign_buf);
		return -1;
	}
	xvp->faceid_fw_buf = fw_buf;
	if (xvp_buf_kmap(xvp, xvp->faceid_fw_buf)) {
		xvp_buf_kunmap(xvp, xvp->faceid_fws_buf);
		xvp_buf_free(xvp, xvp->faceid_fws_buf);
		xvp->faceid_fws_buf = NULL;
		xvp_buf_free(xvp, xvp->faceid_fw_buf);
		xvp->faceid_fw_buf = NULL;
		pr_err("Error: xvp_buf_kmap failed\n");
		return -EFAULT;
	}
	xvp->faceid_addr_offset += VDSP_FACEID_FIRMWIRE_SIZE;
	pr_debug("fw paddr %lx size %d\n", xvp->faceid_fw_buf->paddr,
		xvp->faceid_fw_buf->size);

	return 0;
}

static int sprd_free_faceid_fwbuffer(struct xvp *xvp)
{
	int ret = 0;

	xvp_buf_kunmap(xvp, xvp->faceid_fws_buf);
	ret = xvp_buf_free(xvp, xvp->faceid_fws_buf);
	if (ret) {
		pr_err("Error: free faceid_fw_sign failed\n");
		return ret;
	}
	xvp->faceid_fws_buf = NULL;

	xvp_buf_kunmap(xvp, xvp->faceid_fw_buf);
	ret = xvp_buf_free(xvp, xvp->faceid_fw_buf);
	if (ret) {
		pr_err("Error: free faceid_fw failed\n");
		return ret;
	}
	xvp->faceid_fw_buf = NULL;
	return 0;
}

int sprd_faceid_iommu_map_buffer(struct xvp *xvp)
{
	int ret = -EFAULT;

	ret = xvp_buf_iommu_map(xvp, xvp->faceid_fw_buf);
	if (ret) {
		pr_err("Error:sprd_iommu_map_faceid_fwbuffer failed\n");
		return ret;
	}

	pr_debug("fw :%lx --> %lx\n",
		xvp->faceid_fw_buf->paddr, xvp->faceid_fw_buf->iova);
	//comm
	xvp->faceid_com_buf->iova = xvp->faceid_fw_buf->iova +
		(xvp->faceid_com_buf->paddr - xvp->faceid_fw_buf->paddr);

	pr_debug("comm :%lx --> %lx\n",
		xvp->faceid_com_buf->paddr, xvp->faceid_com_buf->iova);

	//coeff fa
	xvp->faceid_pool.faceid_fa_weights->iova = xvp->faceid_fw_buf->iova +
		(xvp->faceid_pool.faceid_fa_weights->paddr - xvp->faceid_fw_buf->paddr);

	pr_debug("fa :%lx --> %lx\n",
		xvp->faceid_pool.faceid_fa_weights->paddr,
		xvp->faceid_pool.faceid_fa_weights->iova);

	//coeff fp
	xvp->faceid_pool.faceid_fp_weights->iova = xvp->faceid_fw_buf->iova +
		(xvp->faceid_pool.faceid_fp_weights->paddr - xvp->faceid_fw_buf->paddr);

	pr_debug("fp :%lx --> %lx\n",
		xvp->faceid_pool.faceid_fp_weights->paddr,
		xvp->faceid_pool.faceid_fp_weights->iova);

	//coeff foc
	xvp->faceid_pool.faceid_foc_weights->iova = xvp->faceid_fw_buf->iova +
		(xvp->faceid_pool.faceid_foc_weights->paddr - xvp->faceid_fw_buf->paddr);

	pr_debug("foc :%lx --> %lx\n",
		xvp->faceid_pool.faceid_foc_weights->paddr,
		xvp->faceid_pool.faceid_foc_weights->iova);

	//algo mem pool
	xvp->faceid_pool.faceid_mem_pool->iova = xvp->faceid_fw_buf->iova +
		(xvp->faceid_pool.faceid_mem_pool->paddr - xvp->faceid_fw_buf->paddr);

	pr_debug("mem pool :%lx --> %lx\n",
		xvp->faceid_pool.faceid_mem_pool->paddr,
		xvp->faceid_pool.faceid_mem_pool->iova);

	return 0;
}

int sprd_faceid_iommu_unmap_buffer(struct xvp *xvp)
{
	int ret = 0;

	if (unlikely(xvp->faceid_fw_buf->iova == 0)) {
		pr_err("unmap faceid fw addr is NULL\n");
		return -EFAULT;
	}
	ret = xvp_buf_iommu_unmap(xvp, xvp->faceid_fw_buf);
	if (unlikely(ret)) {
		pr_err("unmap faceid fw failed, ret %d\n", ret);
		return -EFAULT;
	}

	pr_debug("unmap faceid fw :%p \n", xvp->faceid_fw_buf->paddr);

	xvp->faceid_com_buf->iova = 0;
	xvp->faceid_pool.faceid_fa_weights->iova = 0;
	xvp->faceid_pool.faceid_fp_weights->iova = 0;
	xvp->faceid_pool.faceid_foc_weights->iova = 0;
	xvp->faceid_pool.faceid_mem_pool->iova = 0;
	return 0;
}

int sprd_faceid_get_image(struct xvp *xvp, int fd)
{
	struct mem_ctx *mem_ctx = NULL;

	xvp->faceid_img_buf = NULL;

	pr_debug("image buffer id %d\n", fd);

	mem_ctx = xvp->mem_dev->xvp_mem_ctx;
	xvp->faceid_img_buf = xvp_buf_get_by_id(xvp, fd);
	if (!xvp->faceid_img_buf) {
		pr_err("fail to get buffer %d \n", fd);
		return -EINVAL;
	}

	pr_debug("iomap:%llx --> %lx size %d\n",
		xvp->faceid_img_buf->paddr, xvp->faceid_img_buf->iova, xvp->faceid_img_buf->size);
	return 0;
}

int sprd_faceid_sec_sign(struct xvp *xvp)
{
#if 0
	bool ret;
	KBC_LOAD_TABLE_V table;
	unsigned long mem_addr_p;
	size_t img_len;

	ret = trusty_kernelbootcp_connect();
	if (!ret) {
		pr_err("bootcp connect fail\n");
		return -EACCES;
	}

	memset(&table, 0, sizeof(KBC_LOAD_TABLE_V));

	mem_addr_p = xvp_buf_get_iova(xvp->faceid_fws_buf);
	img_len = xvp->firmware2_sign->size;

	table.faceid_fw.img_addr = mem_addr_p;
	table.faceid_fw.img_len = img_len;

	pr_debug("fw sign paddr %lX size %zd\n", mem_addr_p, img_len);

	ret = kernel_bootcp_verify_vdsp(&table);
	if (!ret) {
		pr_err("bootcp verify fail\n");
		trusty_kernelbootcp_disconnect();
		return -EACCES;
	}

	trusty_kernelbootcp_disconnect();
#endif
	return 0;
}

int sprd_faceid_secboot_entry(struct xvp *xvp)
{
	bool ret;

	/*copy fw to continuous physical address */
	memcpy(xvp_buf_get_vaddr(xvp->faceid_fws_buf),
		( void *) xvp->firmware2_sign->data, xvp->firmware2_sign->size);

	if (xvp->tee_con) {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_ENTER_SEC_MODE;
		ret = vdsp_set_sec_mode(&msg);
		if (!ret) {
			pr_err("Entry secure mode fail\n");
			return -EACCES;
		}
	} else {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}
	return 0;
}

int sprd_faceid_secboot_exit(struct xvp *xvp)
{
	bool ret;

	if (xvp->tee_con) {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_EXIT_SEC_MODE;
		ret = vdsp_set_sec_mode(&msg);
		if (!ret) {
			pr_err("Exit secure mode fail\n");
			return -EACCES;
		}
	} else {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}
	return 0;
}

int sprd_faceid_load_firmware(struct xvp *xvp)
{
	bool ret;

	if (xvp->tee_con) {
		struct vdsp_load_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_LOAD_FW;

		msg.firmware_size = PAGE_ALIGN(xvp->firmware2_sign->size);	/*aligned PAGE_SIZE */

		ret = vdsp_load_fw(&msg);
		if (!ret) {
			pr_err("load fw fail\n");
			return -EACCES;
		}
	} else {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}
	return 0;
}

int sprd_faceid_sync_vdsp(struct xvp *xvp)
{
	bool ret;
	struct vdsp_hw *hw = (struct vdsp_hw *)xvp->hw_arg;

	if (xvp->tee_con) {
		struct vdsp_sync_msg msg;

		if (xvp->faceid_fw_buf->iova == 0) {
			pr_err("fw io addr is 0\n");
			return -EACCES;
		}
		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_SYNC_VDSP;

		msg.vdsp_log_addr = xvp->faceid_fw_buf->iova +
			hw->vdsp_reserved_mem_size - VDSP_FACEID_LOG_ADDR_OFFSET;
		pr_debug("vdsp log addr %lx\n", msg.vdsp_log_addr);
		ret = vdsp_sync_sec(&msg);
		if (!ret) {
			pr_err("sync vdsp fail\n");
			return -EACCES;
		}
	} else {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}

	return 0;
}

int sprd_faceid_halt_vdsp(struct xvp *xvp)
{
	bool ret;

	if (xvp->tee_con) {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_HALT_VDSP;
		ret = vdsp_set_sec_mode(&msg);
		if (!ret) {
			pr_err("halt vdsp fail\n");
			return -EACCES;
		}
	} else {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}
	return 0;
}

int sprd_faceid_reset_vdsp(struct xvp *xvp)
{
	bool ret;

	if (xvp->tee_con) {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_RESET_VDSP;
		ret = vdsp_set_sec_mode(&msg);
		if (!ret) {
			pr_err("reset vdsp fail\n");
			return -EACCES;
		}
	} else {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}
	return 0;
}

int sprd_faceid_release_vdsp(struct xvp *xvp)
{
	bool ret;

	if (xvp->tee_con) {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_RELEASE_VDSP;
		ret = vdsp_set_sec_mode(&msg);
		if (!ret) {
			pr_err("release vdsp fail\n");
			return -EACCES;
		}
	} else {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}
	return 0;
}

int sprd_faceid_enable_vdsp(struct xvp *xvp)
{
	bool ret;
	int ret2 = 0;

	ret2 = sprd_cam_pw_on();
	if (ret2) {
		pr_err("[error]cam pw on ret:%d", ret2);
		return -EACCES;
	}
	ret2 = sprd_cam_domain_eb();
	if (ret2) {
		pr_err("[error]cam doamin eb ret:%d", ret2);
		ret2 = -EACCES;
		goto err_cam_eb;
	}

	if (xvp->tee_con) {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_ENABLE_VDSP;
		ret = vdsp_set_sec_mode(&msg);
		if (!ret) {
			pr_err("enable vdsp fail\n");
			ret2 = -EACCES;
			goto err_dsp_pw_on;
		}
	} else {
		pr_err("vdsp tee connect fail\n");
		ret2 = -EACCES;
		goto err_dsp_pw_on;
	}
	return 0;

err_dsp_pw_on:
	sprd_cam_domain_disable();
err_cam_eb:
	sprd_cam_pw_off();
	return ret2;

}

int sprd_faceid_disable_vdsp(struct xvp *xvp)
{
	bool ret;
	int ret2;

	if (xvp->tee_con) {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_DISABLE_VDSP;
		ret = vdsp_set_sec_mode(&msg);
		if (!ret) {
			pr_err("disable vdsp fail\n");
			return -EACCES;
		}
	} else {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}
	ret2 = sprd_cam_domain_disable();
	if (ret2)
		pr_err("[error]cam dm disable ret:%d\n", ret2);

	ret2 = sprd_cam_pw_off();	//set reference num decrease
	if (ret2)
		pr_err("[error]cam pw off ret:%d\n", ret2);

	return 0;
}

int sprd_faceid_free_irq(struct xvp *xvp)
{
	if (xvp->irq_status == IRQ_STATUS_REQUESTED) {
		if (xvp->hw_ops->vdsp_free_irq) {
			xvp->hw_ops->vdsp_free_irq(xvp->dev, xvp->hw_arg);
			xvp->irq_status = IRQ_STATUS_FREED;
		} else {
			pr_err("vdsp_free_irq ops is null \n");
			return -EACCES;
		}
	} else {
		pr_err("irq has been already freed \n");
		return -EACCES;
	}
	return 0;
}

int sprd_faceid_request_irq(struct xvp *xvp)
{
	int ret;

	if (xvp->irq_status == IRQ_STATUS_FREED) {
		if (xvp->hw_ops->vdsp_request_irq) {
			ret = xvp->hw_ops->vdsp_request_irq(xvp->dev, xvp->hw_arg);
			if (ret < 0) {
				pr_err("xvp_request_irq failed %d\n", ret);
				return ret;
			}
			xvp->irq_status = IRQ_STATUS_REQUESTED;
		} else {
			pr_err("vdsp_request_irq ops is null \n");
			return -EACCES;
		}
	} else {
		pr_err("irq has been already requested \n");
	}
	return 0;
}

int sprd_faceid_run_vdsp(struct xvp *xvp, uint32_t in_fd, uint32_t out_fd)
{
	int ret;
	struct xvp_buf *tmp;
	struct vdsp_run_msg msg;

	if (!xvp->tee_con) {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}

	tmp = xvp_buf_get_by_id(xvp, in_fd);
	if (!tmp)
		return -EACCES;

	ret = sprd_faceid_get_image(xvp, out_fd);
	if (ret != 0) {
		return ret;
	}

	msg.vdsp_type = TA_CADENCE_VQ7;
	msg.msg_cmd = TA_FACEID_RUN_VDSP;

	msg.fa_coffe_addr = xvp->faceid_pool.faceid_fa_weights->iova;
	msg.fp_coffe_addr = xvp->faceid_pool.faceid_fp_weights->iova;
	msg.foc_coffe_addr = xvp->faceid_pool.faceid_foc_weights->iova;
	msg.mem_pool_addr = xvp->faceid_pool.faceid_mem_pool->iova;

	msg.in_addr = tmp->paddr;
	msg.out_addr = xvp->faceid_img_buf->iova;

	pr_debug("fa %X, fp %X, foc %X, mem pool %X, in %llX, out %llx\n",
		msg.fa_coffe_addr, msg.fp_coffe_addr, msg.foc_coffe_addr,
		msg.mem_pool_addr, msg.in_addr, msg.out_addr);

	ret = vdsp_run_vdsp(&msg);
	if (!ret) {
		pr_err("run vdsp fail\n");
	}

	return 0;
}

int sprd_faceid_boot_firmware(struct xvp *xvp)
{
	int ret;
	s64 tv0, tv1, tv2;

	tv0 = ktime_to_us(ktime_get());
	ret = sprd_faceid_secboot_entry(xvp);
	if (ret < 0)
		return ret;

	sprd_faceid_halt_vdsp(xvp);
	sprd_faceid_reset_vdsp(xvp);

	ret = sprd_faceid_sec_sign(xvp);
	if (ret < 0) {
		return ret;
	}
	ret = sprd_faceid_load_firmware(xvp);
	if (ret < 0) {
		return ret;
	}

	sprd_faceid_release_vdsp(xvp);
	tv1 = ktime_to_us(ktime_get());

	ret = sprd_faceid_sync_vdsp(xvp);
	if (ret < 0) {
		sprd_faceid_halt_vdsp(xvp);
		pr_err("couldn't synchronize with the DSP core\n");
		xvp->off = true;
		return ret;
	}

	tv2 = ktime_to_us(ktime_get());
	/*request firmware - sync */
	pr_debug("[TIME]request firmware:%lld(us), sync:%lld(us)\n",
		tv1 - tv0, tv2 - tv1);
	return 0;
}

int sprd_faceid_secboot_init(struct xvp *xvp)
{
	bool ret;

	if (sprd_faceid_free_irq(xvp) != 0)
		return -EACCES;

	xvp->tee_con = vdsp_ca_connect();
	if (!xvp->tee_con) {
		pr_err("vdsp_ca_connect fail\n");
		sprd_faceid_request_irq(xvp);
		return -EACCES;
	} else {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ7;
		msg.msg_cmd = TA_FACEID_INIT;
		ret = vdsp_set_sec_mode(&msg);
		if (!ret) {
			pr_err("faceid init fail\n");
			vdsp_ca_disconnect();
			xvp->tee_con = false;
			sprd_faceid_request_irq(xvp);
			return -EACCES;
		}
	}
	xvp->secmode = true;
	return 0;
}

int sprd_faceid_secboot_deinit(struct xvp *xvp)
{
	bool ret;

	if (xvp->secmode) {
		if (xvp->tee_con) {
			struct vdsp_msg msg;

			msg.vdsp_type = TA_CADENCE_VQ7;
			msg.msg_cmd = TA_FACEID_EXIT_SEC_MODE;
			ret = vdsp_set_sec_mode(&msg);
			if (!ret)
				pr_err("sprd_faceid_sec_exit fail\n");

			vdsp_ca_disconnect();
			xvp->tee_con = false;
		}
		xvp->secmode = false;
	}

	return sprd_faceid_request_irq(xvp);
}

int sprd_faceid_init(struct xvp *xvp)
{
	int ret = 0;
	struct vdsp_hw *hw = (struct vdsp_hw *)xvp->hw_arg;

	if ((hw->vdsp_reserved_mem_addr == 0) || (hw->vdsp_reserved_mem_size == 0))
		return -ENOMEM;

	ret = sprd_alloc_faceid_fwbuffer(xvp);
	if (ret < 0) {
		goto err;
	}
	ret = sprd_alloc_faceid_combuffer(xvp);
	if (ret < 0) {
		goto err_alloc_faceid_combuffer;
	}
	ret = sprd_faceid_request_weights(xvp);
	if (ret < 0) {
		goto err_faceid_request_weights;
	}
	return 0;
err_faceid_request_weights:
	sprd_free_faceid_combuffer(xvp);
err_alloc_faceid_combuffer:
	sprd_free_faceid_fwbuffer(xvp);
err:
	return ret;
}

int sprd_faceid_deinit(struct xvp *xvp)
{
	sprd_free_faceid_fwbuffer(xvp);
	sprd_free_faceid_combuffer(xvp);
	sprd_faceid_release_weights(xvp);
	return 0;
}
