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
#include <linux/version.h>
#include "isp_statis_buf.h"
#include <linux/sprd_ion.h>
#include <linux/sprd_iommu.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <linux/pm_runtime.h>
#include <linux/ion.h>
#else
#include <video/sprd_mmsys_pw_domain.h>
#include "ion.h"
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "STATS_BUF (%d): %d %s: "\
	fmt, current->pid, __LINE__, __func__

#define STATIS_QUEUE_LENGTH		8
#define ISP_IMG_QUEUE_LEN		8

int isp_statis_queue_init(struct isp_statis_buf_queue *queue)
{
	if (queue == NULL) {
		pr_err("fail to get statis queue,is NULL\n");
		return -EINVAL;
	}

	memset(queue, 0x0, sizeof(struct isp_statis_buf_queue));
	queue->write = &queue->buff[0];
	queue->read = &queue->buff[0];
	spin_lock_init(&queue->lock);

	return 0;
}

int isp_statis_queue_read(struct isp_statis_buf_queue *queue,
			  struct isp_statis_buf *buf)
{
	unsigned long flag;

	if (queue == NULL || buf == NULL) {
		pr_err("fail to get valid param, NULL\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&queue->lock, flag);
	if (queue->read != queue->write) {
		*buf = *queue->read++;
		if (queue->read > &queue->buff[ISP_IMG_QUEUE_LEN - 1])
			queue->read = &queue->buff[0];
	} else {
		spin_unlock_irqrestore(&queue->lock, flag);
		return -EAGAIN;
	}
	spin_unlock_irqrestore(&queue->lock, flag);

	return 0;
}

int isp_statis_queue_write(struct isp_statis_buf_queue *queue,
			   struct isp_statis_buf *buf)
{
	struct isp_statis_buf *ori_buf = NULL;
	unsigned long flag;

	if (queue == NULL || buf == NULL) {
		pr_err("fail to get valid param, NULL\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&queue->lock, flag);

	ori_buf = queue->write;
	*queue->write++ = *buf;

	if (queue->write > &queue->buff[ISP_IMG_QUEUE_LEN - 1])
		queue->write = &queue->buff[0];

	if (queue->write == queue->read) {
		queue->write = ori_buf;
		pr_err("fail to write queue,it is full,property %d\n",
		       ori_buf->buf_property);
	}

	spin_unlock_irqrestore(&queue->lock, flag);

	return 0;
}

void isp_statis_frm_queue_init(struct isp_statis_frm_queue *queue)
{
	int size;
	if (ISP_ADDR_INVALID(queue)) {
		pr_err("fail to get valid heap %p\n", queue);
		return;
	}

	memset((void *)queue, 0, sizeof(struct isp_statis_frm_queue));
	/* pitfall of kfifo: always need size as power of 2, so ---
	 * actually need: X
	 * allocate: X * 2
	 * for kfifo: rounded-up of X
	 */
	size = roundup_pow_of_two(sizeof(queue->buf_array)/2);
	spin_lock_init(&queue->lock);
	kfifo_init(&queue->fifo, queue->buf_array, size);
	if (!kfifo_initialized(&queue->fifo))
		pr_err("FATAL: fail to get fifo\n");
}

int isp_statis_enqueue(struct isp_statis_frm_queue *queue,
		       struct isp_statis_buf *frame)
{
	size_t size;

	if (ISP_ADDR_INVALID(queue) || ISP_ADDR_INVALID(frame)) {
		pr_err("fail to get valid parm %p, %p\n", queue, frame);
		return -1;
	}

	size = kfifo_avail(&queue->fifo);
	if (size < sizeof(struct isp_statis_buf)) {
		pr_err("fail to get available size %zu < %zu\n",
			size, sizeof(struct isp_statis_buf));
		return -1;
	}

	if (!kfifo_in_locked(&queue->fifo, frame,
		sizeof(struct isp_statis_buf), &queue->lock)) {
		pr_info("q over flow property %d\n", frame->buf_property);
		return 0;
	}

	pr_debug("en queue, 0x%lx, 0x%lx\n",
		frame->phy_addr, frame->vir_addr);

	return 0;
}

int isp_statis_dequeue(struct isp_statis_frm_queue *queue,
		       struct isp_statis_buf *frame)
{
	if (ISP_ADDR_INVALID(queue) || ISP_ADDR_INVALID(frame)) {
		pr_err("fail to get valid parm %p, %p\n", queue, frame);
		return -1;
	}

	if (!kfifo_out_locked(&queue->fifo, frame,
		sizeof(struct isp_statis_buf), &queue->lock)) {
		pr_debug("q under flow\n");
		return -1;
	}

	pr_debug("de queue, 0x%lx, 0x%lx\n",
		frame->phy_addr, frame->vir_addr);

	return 0;
}

static void set_buffer_queue(struct isp_queue_param *queue_prm,
			     uint32_t *offset, uint32_t buf_num)
{
	uint32_t cnt;
	unsigned long kaddr;
	struct isp_statis_buf frm_statis;
	uint32_t buf_size = queue_prm->frm_statis->buf_size;

	frm_statis.phy_addr = queue_prm->frm_statis->phy_addr + *offset;
	frm_statis.vir_addr = queue_prm->frm_statis->vir_addr + *offset;
#ifdef CONFIG_64BIT
	kaddr = ((unsigned long)queue_prm->frm_statis->kaddr[0] |
		((unsigned long)queue_prm->frm_statis->kaddr[1] << 32))
			+ *offset;
	frm_statis.kaddr[0] = kaddr;
	frm_statis.kaddr[1] = kaddr >> 32;
#else
	kaddr = (unsigned long)queue_prm->frm_statis->kaddr[0] + *offset;
	frm_statis.kaddr[0] = kaddr;
#endif

	frm_statis.pfinfo.mfd[0] = queue_prm->frm_statis->pfinfo.mfd[0];
	frm_statis.buf_size = queue_prm->frm_statis->buf_size;
	frm_statis.buf_property = queue_prm->frm_statis->buf_property;
	frm_statis.addr_offset = *offset;
	for (cnt = 0; cnt < buf_num; cnt++) {
		isp_statis_queue_write(queue_prm->statis_queue,
						&frm_statis);
		frm_statis.phy_addr += buf_size;
		frm_statis.vir_addr += buf_size;
		kaddr += buf_size;
		frm_statis.kaddr[0] = kaddr;
#ifdef CONFIG_64BIT
		frm_statis.kaddr[1] = kaddr >> 32;
#endif
		frm_statis.addr_offset += buf_size;
	}
	memcpy(queue_prm->buf_reserved, &frm_statis,
	       sizeof(struct isp_statis_buf));
	*offset = frm_statis.addr_offset + buf_size;
}

int sprd_isp_cfg_statis_buf(struct isp_pipe_dev *dev,
			    struct dcam_statis_module *dcam_module,
			    struct isp_statis_buf_input *parm)
{
	int ret = 0;
	uint32_t addr_offset = 0;
	struct isp_statis_buf frm_statis_isp = {0};
	struct isp_statis_buf frm_statis_dcam = {0};
	struct isp_statis_buf t_frm_statis_isp = {0};
	struct isp_statis_buf t_frm_statis_dcam = {0};
	struct isp_statis_module *isp_module = NULL;
	struct isp_queue_param queue_prm;
	unsigned long kaddr1 = 0;

	isp_module = &dev->statis_module_info;

	pr_debug("cfg statis buf in.\n");

	memset((void *)&isp_module->img_statis_buf, 0,
		sizeof(isp_module->img_statis_buf));
	memset((void *)&dcam_module->img_statis_buf, 0,
		sizeof(dcam_module->img_statis_buf));

	frm_statis_isp.phy_addr = frm_statis_dcam.phy_addr
		= parm->phy_addr;
	frm_statis_isp.vir_addr = frm_statis_dcam.vir_addr
		= parm->vir_addr;
	frm_statis_isp.buf_size = frm_statis_dcam.buf_size
		= parm->buf_size;
	frm_statis_isp.buf_property = frm_statis_dcam.buf_property
		= parm->buf_property;

	kaddr1 = (unsigned long)isp_buf_get_kaddr(parm->mfd);
	pr_info("kaddr = 0x%lx\n", kaddr1);

#ifdef CONFIG_64BIT
	parm->kaddr[0] = (uint32_t)(kaddr1 & 0xffffffff);
	parm->kaddr[1] = (uint32_t)((kaddr1 >> 32) & 0xffffffff);
#else
	parm->kaddr[0] = (uint32_t)kaddr1;
	parm->kaddr[1] = 0;
#endif

	/* mapping iommu buffer for ISP */
	frm_statis_isp.pfinfo.dev = &s_isp_pdev->dev;
	frm_statis_isp.pfinfo.mfd[0] = parm->mfd;
	ret = pfiommu_get_sg_table(&frm_statis_isp.pfinfo);
	if (ret) {
		pr_err("fail to cfg statis buf addr\n");
		return -1;
	}

	ret = pfiommu_get_addr(&frm_statis_isp.pfinfo);
	if (ret) {
		pr_err("fail to get statis buf addr\n");
		return ret;
	}

	memcpy(&isp_module->img_statis_buf, &frm_statis_isp,
	       sizeof(struct isp_statis_buf));

	/* mapping iommu buffer for dcam */
	frm_statis_dcam.pfinfo.dev = &s_dcam_pdev->dev;
	frm_statis_dcam.pfinfo.mfd[0] = parm->mfd;
	ret = pfiommu_get_sg_table(&frm_statis_dcam.pfinfo);
	if (ret) {
		pr_err("fail to cfg statis buf addr\n");
		return -1;
	}

	ret = pfiommu_get_addr(&frm_statis_dcam.pfinfo);
	if (ret) {
		pr_err("fail to get statis buf addr\n");
		return ret;
	}
	memcpy(&dcam_module->img_statis_buf, &frm_statis_dcam,
	       sizeof(struct isp_statis_buf));

	isp_module->statis_valid = parm->statis_valid;

	t_frm_statis_isp.phy_addr = frm_statis_isp.pfinfo.iova[0];
	t_frm_statis_isp.vir_addr = parm->vir_addr;
	t_frm_statis_isp.kaddr[0] = parm->kaddr[0];
	t_frm_statis_isp.kaddr[1] = parm->kaddr[1];
	t_frm_statis_isp.pfinfo.mfd[0] = frm_statis_isp.pfinfo.mfd[0];

	t_frm_statis_dcam.phy_addr = frm_statis_dcam.pfinfo.iova[0];
	t_frm_statis_dcam.vir_addr = parm->vir_addr;
	t_frm_statis_dcam.kaddr[0] = parm->kaddr[0];
	t_frm_statis_dcam.kaddr[1] = parm->kaddr[1];
	t_frm_statis_dcam.pfinfo.mfd[0] = frm_statis_dcam.pfinfo.mfd[0];
	pr_info("kaddr[0]=%x, kaddr[1]= %x. phys isp %lx dcam %lx\n",
		parm->kaddr[0], parm->kaddr[1],
		t_frm_statis_isp.phy_addr,
		t_frm_statis_dcam.phy_addr);

	/* AEM buffer config */
	t_frm_statis_dcam.buf_size = ISP_AEM_STATIS_BUF_SIZE;
	t_frm_statis_dcam.buf_property = DCAM_AEM_BLOCK;

	queue_prm.frm_statis = &t_frm_statis_dcam;
	queue_prm.statis_queue = &dcam_module->aem_statis_queue;
	queue_prm.buf_reserved = &dcam_module->aem_buf_reserved;
	set_buffer_queue(&queue_prm, &addr_offset,
					ISP_AEM_STATIS_BUF_NUM);

	/* ISP AEM use the same buffer as DCAM AEM */
	addr_offset = 0;
	t_frm_statis_isp.buf_size = ISP_AEM_STATIS_BUF_SIZE;
	t_frm_statis_isp.buf_property = ISP_AEM_BLOCK;

	queue_prm.frm_statis = &t_frm_statis_isp;
	queue_prm.statis_queue = &isp_module->aem_statis_queue;
	queue_prm.buf_reserved = &isp_module->aem_buf_reserved;
	set_buffer_queue(&queue_prm, &addr_offset,
					ISP_AEM_STATIS_BUF_NUM);

	/* AFL buffer config */
	t_frm_statis_isp.buf_size = ISP_AFL_STATIS_BUF_SIZE;
	t_frm_statis_isp.buf_property = ISP_AFL_BLOCK;

	queue_prm.frm_statis = &t_frm_statis_isp;
	queue_prm.statis_queue = &isp_module->afl_statis_queue;
	queue_prm.buf_reserved = &isp_module->afl_buf_reserved;
	set_buffer_queue(&queue_prm, &addr_offset, ISP_AFL_STATIS_BUF_NUM);

	/* AFM buffer config */
	t_frm_statis_isp.buf_size = ISP_AFM_STATIS_BUF_SIZE;
	t_frm_statis_isp.buf_property = ISP_AFM_BLOCK;

	queue_prm.frm_statis = &t_frm_statis_isp;
	queue_prm.statis_queue = &isp_module->afm_statis_queue;
	queue_prm.buf_reserved = &isp_module->afm_buf_reserved;
	set_buffer_queue(&queue_prm, &addr_offset, ISP_AFM_STATIS_BUF_NUM);

	/* PDAF buffer config */
	if (isp_module->statis_valid & ISP_STATIS_VALID_PDAF) {
		t_frm_statis_dcam.buf_size = ISP_PDAF_STATIS_BUF_SIZE;
		t_frm_statis_dcam.buf_property = DCAM_PDAF_BLOCK;

		queue_prm.frm_statis = &t_frm_statis_dcam;
		queue_prm.statis_queue = &dcam_module->pdaf_statis_queue;
		queue_prm.buf_reserved = &dcam_module->pdaf_buf_reserved;
		set_buffer_queue(&queue_prm, &addr_offset,
				 ISP_PDAF_STATIS_BUF_NUM);
	}

	/* EMBED LINE buffer config */
	if (isp_module->statis_valid & ISP_STATIS_VALID_EBD) {
		t_frm_statis_dcam.buf_size = ISP_EBD_STATIS_BUF_SIZE;
		t_frm_statis_dcam.buf_property = DCAM_EBD_BLOCK;

		queue_prm.frm_statis = &t_frm_statis_dcam;
		queue_prm.statis_queue = &dcam_module->ebd_statis_queue;
		queue_prm.buf_reserved = &dcam_module->ebd_buf_reserved;
		set_buffer_queue(&queue_prm, &addr_offset,
				 ISP_EBD_STATIS_BUF_NUM);
	}

	/* BINNING buffer config */
	if (isp_module->statis_valid & ISP_STATIS_VALID_BINNING) {
		t_frm_statis_isp.buf_size = ISP_BINNING_STATIS_BUF_SIZE;
		t_frm_statis_isp.buf_property = ISP_BINNING_BLOCK;
		queue_prm.frm_statis = &t_frm_statis_isp;
		queue_prm.statis_queue = &isp_module->binning_statis_queue;
		queue_prm.buf_reserved = &isp_module->binning_buf_reserved;
		set_buffer_queue(&queue_prm, &addr_offset,
				 ISP_BINNING_STATIS_BUF_NUM);
	}

	/* HIST buffer config */
	if (isp_module->statis_valid & ISP_STATIS_VALID_HIST) {
		t_frm_statis_isp.buf_size = ISP_HIST_STATIS_BUF_SIZE;
		t_frm_statis_isp.buf_property = ISP_HIST_BLOCK;

		queue_prm.frm_statis = &t_frm_statis_isp;
		queue_prm.statis_queue = &isp_module->hist_statis_queue;
		queue_prm.buf_reserved = &isp_module->hist_buf_reserved;
		set_buffer_queue(&queue_prm, &addr_offset,
				 ISP_HIST_STATIS_BUF_NUM);
	}

	/* HIST2 buffer config */
	if (isp_module->statis_valid & ISP_STATIS_VALID_HIST2) {
		t_frm_statis_isp.buf_size = ISP_HIST2_STATIS_BUF_SIZE;
		t_frm_statis_isp.buf_property = ISP_HIST2_BLOCK;

		queue_prm.frm_statis = &t_frm_statis_isp;
		queue_prm.statis_queue = &isp_module->hist2_statis_queue;
		queue_prm.buf_reserved = &isp_module->hist2_buf_reserved;
		set_buffer_queue(&queue_prm, &addr_offset,
				 ISP_HIST2_STATIS_BUF_NUM);
	}

	/* RAW buffer config */
	if (isp_module->statis_valid & ISP_STATIS_VALID_RAW) {
		t_frm_statis_dcam.buf_size =
			ISP_RAW_STATIS_BUF_SIZE(parm->width, parm->height);
		t_frm_statis_dcam.buf_property = DCAM_RAW_BLOCK;

		queue_prm.frm_statis = &t_frm_statis_dcam;
		queue_prm.statis_queue = &dcam_module->raw_statis_queue;
		queue_prm.buf_reserved = &dcam_module->raw_buf_reserved;
		set_buffer_queue(&queue_prm, &addr_offset,
				 ISP_RAW_STATIS_BUF_NUM);
	}

	return ret;
}

int sprd_isp_set_statis_addr(struct isp_pipe_dev *dev,
			     struct dcam_statis_module *dcam_module,
			     struct isp_statis_buf_input *parm)
{
	int ret = 0;
	struct isp_statis_buf frm_statis;
	struct isp_statis_module *module = NULL;
	struct isp_statis_buf_queue *statis_queue = NULL;
	int select_device = 0;

	module = &dev->statis_module_info;

	switch (parm->buf_property) {
	case DCAM_AEM_BLOCK:
		statis_queue = &dcam_module->aem_statis_queue;
		select_device = 1;
		break;
	case ISP_AFL_BLOCK:
		statis_queue = &module->afl_statis_queue;
		break;
	case ISP_AFM_BLOCK:
		statis_queue = &module->afm_statis_queue;
		break;
	case ISP_BINNING_BLOCK:
		statis_queue = &module->binning_statis_queue;
		break;
	case DCAM_PDAF_BLOCK:
		statis_queue = &dcam_module->pdaf_statis_queue;
		select_device = 1;
		break;
	case DCAM_EBD_BLOCK:
		statis_queue = &dcam_module->ebd_statis_queue;
		select_device = 1;
		break;
	case ISP_HIST_BLOCK:
		statis_queue = &module->hist_statis_queue;
		break;
	case ISP_HIST2_BLOCK:
		statis_queue = &module->hist2_statis_queue;
		break;
	case DCAM_RAW_BLOCK:
		statis_queue = &dcam_module->raw_statis_queue;
		select_device = 1;
		break;
	}

	memset((void *)&frm_statis, 0, sizeof(struct isp_statis_buf));
	frm_statis.phy_addr = parm->phy_addr;
	frm_statis.vir_addr = parm->vir_addr;
	frm_statis.addr_offset = parm->addr_offset;
	frm_statis.kaddr[0] = parm->kaddr[0];
	frm_statis.kaddr[1] = parm->kaddr[1];
	frm_statis.buf_size = parm->buf_size;
	frm_statis.buf_property = parm->buf_property;

	frm_statis.pfinfo.dev =
		select_device ? &s_dcam_pdev->dev : &s_isp_pdev->dev;
	frm_statis.pfinfo.mfd[0] = parm->reserved[0];
	/* when the statis is running, we need not map again */
	ret = isp_statis_queue_write(statis_queue, &frm_statis);

	pr_debug("set statis buf addr done.\n");
	return ret;
}
