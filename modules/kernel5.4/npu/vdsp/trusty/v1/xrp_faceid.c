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

#include "vdsp_hw.h"
#include "xrp_internal.h"
#include "xrp_kernel_dsp_interface.h"
#include "xrp_faceid.h"
#include "vdsp_trusty.h"

#define SIGN_HEAD_SIZE (512)
#define SIGN_TAIL_SIZE (512)

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: faceid %d %d %s : "\
        fmt, current->pid, __LINE__, __func__

static int sprd_alloc_faceid_weights_buffer(struct xvp *xvp,
	struct ion_buf *ion_buf,
	size_t size)
{
	int ret;

	ret = xvp->vdsp_mem_desc->ops->mem_alloc(xvp->vdsp_mem_desc, ion_buf,
		ION_HEAP_ID_MASK_VDSP,	/*todo vdsp head id */ size);
	if (unlikely(0 != ret)) {
		pr_err("alloc weights failed\n");
		return -ENOMEM;
	}
	ret = xvp->vdsp_mem_desc->ops->mem_kmap(xvp->vdsp_mem_desc, ion_buf);
	if (unlikely(0 != ret)) {
		xvp->vdsp_mem_desc->ops->mem_free(xvp->vdsp_mem_desc, ion_buf);
		return -EFAULT;
	}
	ion_buf->dev = xvp->dev;
	pr_debug("faceid alloc addr_p %lx  vaddr:%lx,size %ld\n",
		ion_buf->addr_p, ion_buf->addr_k, ion_buf->size);
	return 0;

}

static int sprd_free_faceid_weights_buffer(struct xvp *xvp,
	struct ion_buf *ion_buf)
{
	unsigned long dst_viraddr = ion_buf->addr_k;

	if (dst_viraddr) {
		xvp->vdsp_mem_desc->ops->mem_kunmap(xvp->vdsp_mem_desc, ion_buf);
		xvp->vdsp_mem_desc->ops->mem_free(xvp->vdsp_mem_desc, ion_buf);
	}
	return 0;
}

int sprd_faceid_request_algo_mem(struct xvp *xvp)
{
	int ret = sprd_alloc_faceid_weights_buffer(xvp, &xvp->faceid_pool.ion_fd_mem_pool,
		FACEID_FD_MEM_SIZE);

	if (unlikely(ret < 0)) {
		pr_err("request fd mem fail\n");
		return ret;
	}

	return 0;
}

int sprd_faceid_release_algo_mem(struct xvp *xvp)
{
	sprd_free_faceid_weights_buffer(xvp, &xvp->faceid_pool.ion_fd_mem_pool);
	return 0;
}

int sprd_request_weights(struct xvp *xvp, char *name, struct ion_buf *coeff_ion)
{
	unsigned long dst = 0;
	int ret = request_firmware(&xvp->faceid_fw, name, xvp->dev);

	if (unlikely(ret < 0)) {
		pr_err("request %s weights fail\n", name);
		return ret;
	}

	ret = sprd_alloc_faceid_weights_buffer(xvp, coeff_ion, xvp->faceid_fw->size);

	if (unlikely(ret < 0)) {
		pr_err("alloc %s weights fail\n", name);
		return ret;
	}

	dst = coeff_ion->addr_k;
	memcpy(( void *) dst, xvp->faceid_fw->data, xvp->faceid_fw->size);

	release_firmware(xvp->faceid_fw);

	return ret;
}

int sprd_faceid_request_weights(struct xvp *xvp)
{
	int i = 0, ret;

	char *coeff_name[FACEID_COEFF_NUM] = {
		"network_coeff_fd_p.bin",
		"network_coeff_fd_r.bin",
		"network_coeff_fd_o.bin",
		"network_coeff_fp.bin",
		"network_coeff_flv.bin",
		"network_coeff_fv.bin"
	};

	struct ion_buf *coeff_ion[FACEID_COEFF_NUM] = {
		&xvp->faceid_pool.ion_fd_weights_p,
		&xvp->faceid_pool.ion_fd_weights_r,
		&xvp->faceid_pool.ion_fd_weights_o,
		&xvp->faceid_pool.ion_fp_weights,
		&xvp->faceid_pool.ion_flv_weights,
		&xvp->faceid_pool.ion_fo_weights,
	};

	for (; i < FACEID_COEFF_NUM; i++) {
		ret = sprd_request_weights(xvp, coeff_name[i], coeff_ion[i]);
		if (ret < 0)
			return ret;
	}
	sprd_faceid_request_algo_mem(xvp);

	return 0;
}

void sprd_faceid_release_weights(struct xvp *xvp)
{
	int i = 0;

	struct ion_buf *coeff_ion[FACEID_COEFF_NUM] = {
		&xvp->faceid_pool.ion_fd_weights_p,
		&xvp->faceid_pool.ion_fd_weights_r,
		&xvp->faceid_pool.ion_fd_weights_o,
		&xvp->faceid_pool.ion_fp_weights,
		&xvp->faceid_pool.ion_flv_weights,
		&xvp->faceid_pool.ion_fo_weights,
	};

	for (; i < FACEID_COEFF_NUM; i++) {
		sprd_free_faceid_weights_buffer(xvp, coeff_ion[i]);
	}
	sprd_faceid_release_algo_mem(xvp);
}

int sprd_alloc_faceid_combuffer(struct xvp *xvp)
{
	int ret;

	ret = xvp->vdsp_mem_desc->ops->mem_alloc(xvp->vdsp_mem_desc,
		&xvp->ion_faceid_comm, ION_HEAP_ID_MASK_VDSP, PAGE_SIZE);
	if (0 != ret) {
		pr_err("alloc faceid com buffer failed,ret %d\n", ret);
		return -ENOMEM;
	}

	xvp->ion_faceid_comm.dev = xvp->dev;

	pr_debug("faceid com phyaddr %llX\n", xvp->ion_faceid_comm.addr_p);
	return 0;
}

int sprd_free_faceid_combuffer(struct xvp *xvp)
{

	xvp->vdsp_mem_desc->ops->mem_free(xvp->vdsp_mem_desc, &xvp->ion_faceid_comm);

	return 0;
}

static int sprd_alloc_faceid_fwbuffer(struct xvp *xvp)
{
	int ret;

	ret = xvp->vdsp_mem_desc->ops->mem_alloc(xvp->vdsp_mem_desc, &xvp->ion_faceid_fw_sign,
		ION_HEAP_ID_MASK_VDSP, VDSP_FACEID_FIRMWIRE_SIZE);
	if (unlikely(0 != ret)) {
		pr_err("alloc sign fw buffer failed,ret %d\n", ret);
		return -ENOMEM;
	}
	ret = xvp->vdsp_mem_desc->ops->mem_kmap(xvp->vdsp_mem_desc, &xvp->ion_faceid_fw_sign);
	if (unlikely(0 != ret)) {
		xvp->vdsp_mem_desc->ops->mem_free(xvp->vdsp_mem_desc, &xvp->ion_faceid_fw_sign);
		pr_err("kmap fw buffer failed,ret %d\n", ret);
		return -EFAULT;
	}
	xvp->ion_faceid_fw_sign.dev = xvp->dev;
	pr_debug("ion_faceid_fw_sign phyaddr %llX\n",
		xvp->ion_faceid_fw_sign.addr_k, xvp->ion_faceid_fw_sign.addr_p);

	xvp->ion_faceid_fw.addr_p = 0;
	ret = xvp->vdsp_mem_desc->ops->mem_alloc(xvp->vdsp_mem_desc, &xvp->ion_faceid_fw,
		ION_HEAP_ID_MASK_VDSP, VDSP_FACEID_FIRMWIRE_SIZE);
	if (unlikely(0 != ret)) {
		pr_err("alloc fw buffer failed,ret %d\n", ret);
		return -ENOMEM;
	}

	xvp->ion_faceid_fw.dev = xvp->dev;
	pr_debug("ion_faceid_fw phyaddr %llX\n", xvp->ion_faceid_fw.addr_p);
	return 0;
}

static int sprd_free_faceid_fwbuffer(struct xvp *xvp)
{
	xvp->vdsp_mem_desc->ops->mem_kunmap(xvp->vdsp_mem_desc, &xvp->ion_faceid_fw_sign);
	xvp->vdsp_mem_desc->ops->mem_free(xvp->vdsp_mem_desc, &xvp->ion_faceid_fw_sign);

	xvp->vdsp_mem_desc->ops->mem_free(xvp->vdsp_mem_desc, &xvp->ion_faceid_fw);

	xvp->ion_faceid_fw.addr_p = 0;
	return 0;
}

void sprd_release_faceid_firmware(struct xvp *xvp)
{
	release_firmware(xvp->firmware2_sign);
}

int sprd_request_faceid_firmware(struct xvp *xvp)
{
	int ret = request_firmware(&xvp->firmware2_sign, FACEID_FIRMWARE, xvp->dev);

	if (unlikely(ret < 0)) {
		pr_err("request firmware failed ret:%d\n", ret);
		return ret;
	}

	return ret;
}

int sprd_iommu_map_faceid_fwbuffer(struct xvp *xvp)
{
	int ret = -EFAULT;

	if (unlikely(xvp->ion_faceid_fw.addr_p == 0)) {
		pr_err("map faceid fw addr is NULL \n");
		return ret;
	}

	ret = xvp->vdsp_mem_desc->ops->mem_iommu_map(xvp->vdsp_mem_desc, &xvp->ion_faceid_fw,
		IOMMU_ALL);
	if (unlikely(ret)) {
		pr_err("map faceid fw buffer failed\n");
		return ret;
	}
	xvp->dsp_firmware_addr = xvp->ion_faceid_fw.iova;

	pr_debug("iomap:%p --> %lx\n",
		xvp->ion_faceid_fw.addr_p, ( unsigned long) xvp->dsp_firmware_addr);
	return ret;
}

int sprd_iommu_unmap_faceid_fwbuffer(struct xvp *xvp)
{
	int ret = 0;

	if (unlikely(xvp->ion_faceid_fw.addr_p == 0)) {
		pr_err("unmap faceid fw addr is NULL\n");
		return -EFAULT;
	}

	ret = xvp->vdsp_mem_desc->ops->mem_iommu_unmap(xvp->vdsp_mem_desc,
		&xvp->ion_faceid_fw, IOMMU_ALL);
	if (unlikely(ret)) {
		pr_err("unmap faceid fw fialed, ret %d\n", ret);
		return -EFAULT;
	}

	pr_debug("unmap faceid fw :%p \n", xvp->ion_faceid_fw.addr_p);
	return 0;
}

unsigned long sprd_faceid_get_ion_phy(int fd)
{
	int ret;
	struct ion_buf tmp = {0};

	ret = sprd_ion_get_buffer(fd, tmp.dmabuf_p, &tmp.buf, &tmp.size);
	if (ret) {
		pr_err("fail to get %d ionbuf \n", fd);
		return 0;
	}

	ret = sprd_ion_get_phys_addr(fd, tmp.dmabuf_p, &tmp.addr_p, &tmp.size);
	if (ret) {
		pr_err("fail to get %d phy_addr, ret %d\n", fd, ret);
		return 0;
	}

	pr_debug("Get ion %d phy %llX\n", fd, tmp.addr_p);
	return tmp.addr_p;
}

int sprd_faceid_sec_sign(struct xvp *xvp)
{
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

	mem_addr_p = xvp->ion_faceid_fw_sign.addr_p;
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
	return 0;
}

int sprd_faceid_secboot_entry(struct xvp *xvp)
{
	bool ret;

	/*copy fw to continuous physical address */
	memcpy(( void *) xvp->ion_faceid_fw_sign.addr_k,
		( void *) xvp->firmware2_sign->data, xvp->firmware2_sign->size);

	if (xvp->tee_con) {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ6;
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

		msg.vdsp_type = TA_CADENCE_VQ6;
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
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ6;
		msg.msg_cmd = TA_FACEID_LOAD_FW;
		ret = vdsp_set_sec_mode(&msg);
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
	struct vdsp_side_sync_data *hw_sync_data = NULL;
	struct vdsp_sync_msg msg;
	size_t sz = 0;

	if (xvp->tee_con == false) {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}

	msg.vdsp_type = TA_CADENCE_VQ6;
	msg.msg_cmd = TA_FACEID_SYNC_VDSP;

	hw_sync_data = xvp->hw_ops->get_hw_sync_data(xvp->hw_arg, &sz, xvp->ion_vdsp_log.addr_p);
	if (unlikely(!hw_sync_data)) {
		return -ENOMEM;
	}

	msg.device_mmio_base = hw_sync_data->device_mmio_base;
	msg.host_irq_mode = hw_sync_data->host_irq_mode;
	msg.host_irq_offset = hw_sync_data->host_irq_offset;
	msg.host_irq_bit = hw_sync_data->host_irq_bit;
	msg.device_irq_mode = hw_sync_data->device_irq_mode;
	msg.device_irq_offset = hw_sync_data->device_irq_offset;
	msg.device_irq_bit = hw_sync_data->device_irq_bit;
	msg.device_irq = hw_sync_data->device_irq;
	msg.vdsp_log_addr = 0;

	ret = vdsp_sync_sec(&msg);
	kfree(hw_sync_data);

	if (!ret) {
		pr_err("sync vdsp fail\n");
		return -EACCES;
	}
	return 0;
}

int sprd_faceid_halt_vdsp(struct xvp *xvp)
{
	bool ret;

	if (xvp->tee_con) {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ6;
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

		msg.vdsp_type = TA_CADENCE_VQ6;
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

		msg.vdsp_type = TA_CADENCE_VQ6;
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

	if (xvp->tee_con) {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ6;
		msg.msg_cmd = TA_FACEID_ENABLE_VDSP;
		ret = vdsp_set_sec_mode(&msg);
		if (!ret) {
			pr_err("enable vdsp fail\n");
			return -EACCES;
		}
	} else {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}
	return 0;
}

int sprd_faceid_disable_vdsp(struct xvp *xvp)
{
	bool ret;

	if (xvp->tee_con) {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ6;
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
	return 0;
}

void sprd_faceid_free_irq(struct xvp *xvp)
{
	if (xvp->irq_status == IRQ_STATUS_REQUESTED) {
		if (xvp->hw_ops->vdsp_free_irq) {
			xvp->hw_ops->vdsp_free_irq(xvp->dev, xvp->hw_arg);
			xvp->irq_status = IRQ_STATUS_FREED;
		} else {
			pr_err("vdsp_free_irq ops is null \n");
		}
	} else {
		pr_err("irq has been already freed \n");
	}
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
	bool ret;

	if (xvp->tee_con) {
		struct vdsp_run_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ6;
		msg.msg_cmd = TA_FACEID_RUN_VDSP;

		msg.fd_p_coffe_addr = xvp->faceid_pool.ion_fd_weights_p.addr_p;
		msg.fd_r_coffe_addr = xvp->faceid_pool.ion_fd_weights_r.addr_p;
		msg.fd_o_coffe_addr = xvp->faceid_pool.ion_fd_weights_o.addr_p;
		msg.fp_coffe_addr = xvp->faceid_pool.ion_fp_weights.addr_p;
		msg.flv_coffe_addr = xvp->faceid_pool.ion_flv_weights.addr_p;
		msg.fv_coffe_addr = xvp->faceid_pool.ion_fo_weights.addr_p;
		msg.mem_pool_addr = xvp->faceid_pool.ion_fd_mem_pool.addr_p;

		msg.in_addr = sprd_faceid_get_ion_phy(in_fd);
		msg.out_addr = 0;

		if (0 == msg.in_addr)
			return -1;

		pr_debug
		("fd_p %X,fd_r %X,fd_o %X,fp %X,flv %X,fv %X,mem pool %X,in %llX\n",
			msg.fd_p_coffe_addr, msg.fd_r_coffe_addr,
			msg.fd_o_coffe_addr, msg.fp_coffe_addr, msg.flv_coffe_addr,
			msg.fv_coffe_addr, msg.mem_pool_addr, msg.in_addr);

		ret = vdsp_run_vdsp(&msg);
		if (!ret) {
			pr_err("run vdsp fail\n");
		}

		return 0;
	} else {
		pr_err("vdsp tee connect fail\n");
		return -EACCES;
	}
}

int sprd_faceid_secboot_init(struct xvp *xvp)
{
	bool ret;

	sprd_faceid_free_irq(xvp);

	xvp->tee_con = vdsp_ca_connect();
	if (!xvp->tee_con) {
		pr_err("vdsp_ca_connect fail\n");
		sprd_faceid_request_irq(xvp);
		return -EACCES;
	} else {
		struct vdsp_msg msg;

		msg.vdsp_type = TA_CADENCE_VQ6;
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

			msg.vdsp_type = TA_CADENCE_VQ6;
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

	ret = sprd_alloc_faceid_combuffer(xvp);
	if (ret < 0) {
		return ret;
	}

	ret = sprd_alloc_faceid_fwbuffer(xvp);
	if (ret < 0) {
		sprd_free_faceid_combuffer(xvp);
		return ret;
	}
	ret = sprd_request_faceid_firmware(xvp);
	if (ret < 0) {
		sprd_free_faceid_combuffer(xvp);
		sprd_free_faceid_fwbuffer(xvp);
		return ret;
	}

	sprd_faceid_request_weights(xvp);
	return 0;
}

int sprd_faceid_deinit(struct xvp *xvp)
{
	sprd_release_faceid_firmware(xvp);
	sprd_free_faceid_fwbuffer(xvp);
	sprd_free_faceid_combuffer(xvp);
	sprd_faceid_release_weights(xvp);
	return 0;
}
