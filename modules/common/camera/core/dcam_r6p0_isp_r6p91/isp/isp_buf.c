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

#include "isp_buf.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_BUF: %d : " fmt, __LINE__

#define STATIS_QUEUE_LENGTH		8
#define ISP_IMG_QUEUE_LEN		8

int isp_statis_queue_init(struct isp_statis_buf_queue *queue)
{
	if (queue == NULL) {
		pr_err("fail to get valid input ptr\n");
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
		pr_err("fail to get valid input ptr\n");
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
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&queue->lock, flag);

	ori_buf = queue->write;
	*queue->write++ = *buf;

	if (queue->write > &queue->buff[ISP_IMG_QUEUE_LEN - 1])
		queue->write = &queue->buff[0];

	if (queue->write == queue->read) {
		queue->write = ori_buf;
		pr_err("fail to write queue:full, %p\n", queue);
	}

	spin_unlock_irqrestore(&queue->lock, flag);

	return 0;
}

void isp_statis_frm_queue_clear(struct isp_statis_frm_queue *queue)
{
	if (ISP_ADDR_INVALID(queue)) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	memset((void *)queue, 0, sizeof(struct isp_statis_frm_queue));
}

int isp_statis_enqueue(struct isp_statis_frm_queue *queue,
	struct isp_statis_buf *frame)
{
	if (ISP_ADDR_INVALID(queue) || ISP_ADDR_INVALID(frame)) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}

	if (queue->valid_cnt >= STATIS_QUEUE_LENGTH) {
		pr_err("q over flow\n");
		return -1;
	}

	memcpy(&queue->buf_array[queue->valid_cnt], frame,
	       sizeof(struct isp_statis_buf));
	queue->valid_cnt++;
	pr_debug("en queue, %d, 0x%lx, 0x%lx\n",
		   queue->valid_cnt, frame->phy_addr, frame->vir_addr);

	return 0;
}

int isp_statis_dequeue(struct isp_statis_frm_queue *queue,
	struct isp_statis_buf *frame)
{
	unsigned int i = 0;

	if (ISP_ADDR_INVALID(queue) || ISP_ADDR_INVALID(frame)) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}

	if (queue->valid_cnt == 0) {
		pr_debug("q under flow\n");
		return -1;
	}

	memcpy(frame, &queue->buf_array[0], sizeof(struct isp_statis_buf));
	queue->valid_cnt--;
	for (i = 0; i < queue->valid_cnt; i++) {
		memcpy(&queue->buf_array[i], &queue->buf_array[i + 1],
		       sizeof(struct isp_statis_buf));
	}
	pr_debug("de queue, %d\n", queue->valid_cnt);

	return 0;
}

int sprd_isp_cfg_statis_buf(struct isp_pipe_dev *dev,
	struct isp_statis_buf_input *parm)
{
	int ret = ISP_RTN_SUCCESS;
	int cnt = 0;
	uint32_t aem_iova_addr = 0, aem_vir_addr = 0;
	unsigned long aem_kaddr = 0;
	uint32_t afl_iova_addr = 0, afl_vir_addr = 0;
	unsigned long afl_kaddr = 0;
	uint32_t afm_iova_addr = 0, afm_vir_addr = 0;
	unsigned long afm_kaddr = 0;
	uint32_t binning_iova_addr = 0, binning_vir_addr = 0;
	unsigned long binning_kaddr = 0;
	uint32_t hist_iova_addr = 0, hist_vir_addr = 0;
	unsigned long hist_kaddr = 0;
	uint32_t addr_offset = 0;
	struct isp_statis_buf frm_statis;
	struct isp_statis_buf aem_frm_statis;
	struct isp_statis_buf afl_frm_statis;
	struct isp_statis_buf afm_frm_statis;
	struct isp_statis_buf binning_frm_statis;
	struct isp_statis_buf hist_frm_statis;
	struct isp_statis_module *module = NULL;
	size_t statis_mem_size = 0;

	module = &dev->statis_module_info;

	pr_debug("cfg statis buf in.\n");
	isp_statis_queue_init(&module->aem_statis_queue);
	isp_statis_queue_init(&module->afl_statis_queue);
	isp_statis_queue_init(&module->afm_statis_queue);
	isp_statis_queue_init(&module->binning_statis_queue);
	isp_statis_queue_init(&module->hist_statis_queue);
	isp_statis_frm_queue_clear(&module->aem_statis_frm_queue);
	isp_statis_frm_queue_clear(&module->afl_statis_frm_queue);
	isp_statis_frm_queue_clear(&module->afm_statis_frm_queue);
	isp_statis_frm_queue_clear(&module->binning_statis_frm_queue);
	isp_statis_frm_queue_clear(&module->hist_statis_frm_queue);
	memset((void *)&module->img_statis_buf, 0,
	       sizeof(struct isp_statis_buf));
	memset((void *)&frm_statis, 0, sizeof(struct isp_statis_buf));
	memset((void *)&aem_frm_statis, 0, sizeof(struct isp_statis_buf));
	memset((void *)&afl_frm_statis, 0, sizeof(struct isp_statis_buf));
	memset((void *)&afm_frm_statis, 0, sizeof(struct isp_statis_buf));
	memset((void *)&binning_frm_statis, 0, sizeof(struct isp_statis_buf));
	memset((void *)&hist_frm_statis, 0, sizeof(struct isp_statis_buf));

	frm_statis.phy_addr = parm->phy_addr;
	frm_statis.vir_addr = parm->vir_addr;
	frm_statis.buf_size = parm->buf_size;
	frm_statis.buf_property = parm->buf_property;
	frm_statis.pfinfo.dev = &s_isp_pdev->dev;
	frm_statis.pfinfo.mfd[0] = parm->mfd;

	/*mapping iommu buffer*/
	ret = pfiommu_get_sg_table(&frm_statis.pfinfo);
	if (ret) {
		pr_err("fail to cfg statis buf addr\n");
		ret = -1;
		return ret;
	}

	ret = pfiommu_get_addr(&frm_statis.pfinfo);
	pr_debug("fd: 0x%x, iova_addr: 0x%x, size:0x%x.\n",
		frm_statis.pfinfo.mfd[0],
		(unsigned int)frm_statis.pfinfo.iova[0],
		(unsigned int)frm_statis.pfinfo.size[0]);
	memcpy(&module->img_statis_buf, &frm_statis,
	       sizeof(struct isp_statis_buf));
	aem_iova_addr = frm_statis.pfinfo.iova[0];
	aem_vir_addr = parm->vir_addr;
	aem_kaddr = pfiommu_get_kaddr(&frm_statis.pfinfo);
	if (aem_kaddr == 0) {
		pr_err("fail to get statis buf kaddr\n");
		pfiommu_free_addr(&frm_statis.pfinfo);
		return -1;
	}

	statis_mem_size = frm_statis.pfinfo.size[0];
	/*split the big buffer to some little buffer*/
	for (cnt = 0; cnt < ISP_AEM_STATIS_BUF_NUM; cnt++) {
		aem_frm_statis.phy_addr = aem_iova_addr;
		aem_frm_statis.vir_addr = aem_vir_addr;
		aem_frm_statis.kaddr[0] = aem_kaddr;
		aem_frm_statis.addr_offset = addr_offset;
		aem_frm_statis.pfinfo.mfd[0] = frm_statis.pfinfo.mfd[0];
		aem_frm_statis.buf_size = ISP_AEM_STATIS_BUF_SIZE;
		aem_frm_statis.buf_property = ISP_AEM_BLOCK;

		ret = isp_statis_queue_write(&module->aem_statis_queue,
					     &aem_frm_statis);
		aem_iova_addr += ISP_AEM_STATIS_BUF_SIZE;
		aem_vir_addr += ISP_AEM_STATIS_BUF_SIZE;
		aem_kaddr += ISP_AEM_STATIS_BUF_SIZE;
		addr_offset += ISP_AEM_STATIS_BUF_SIZE;
	}
	/*init reserved aem statis buf*/
	module->aem_buf_reserved.phy_addr = aem_iova_addr;
	module->aem_buf_reserved.vir_addr = aem_vir_addr;
	module->aem_buf_reserved.kaddr[0] = aem_kaddr;
	module->aem_buf_reserved.addr_offset = addr_offset;
	module->aem_buf_reserved.pfinfo.mfd[0] = frm_statis.pfinfo.mfd[0];
	module->aem_buf_reserved.buf_size = ISP_AEM_STATIS_BUF_SIZE;
	module->aem_buf_reserved.buf_property = ISP_AEM_BLOCK;

	/*afl statis buf cfg*/
	afl_iova_addr = aem_iova_addr + ISP_AEM_STATIS_BUF_SIZE;
	afl_vir_addr = aem_vir_addr + ISP_AEM_STATIS_BUF_SIZE;
	afl_kaddr = aem_kaddr + ISP_AEM_STATIS_BUF_SIZE;
	addr_offset += ISP_AEM_STATIS_BUF_SIZE;
	/*split the big buffer to some little buffer*/
	for (cnt = 0; cnt < ISP_AFL_STATIS_BUF_NUM; cnt++) {
		afl_frm_statis.phy_addr = afl_iova_addr;
		afl_frm_statis.vir_addr = afl_vir_addr;
		afl_frm_statis.kaddr[0] = afl_kaddr;
		afl_frm_statis.addr_offset = addr_offset;
		afl_frm_statis.pfinfo.mfd[0] = frm_statis.pfinfo.mfd[0];
		afl_frm_statis.buf_size = ISP_AFL_STATIS_BUF_SIZE;
		afl_frm_statis.buf_property = ISP_AFL_BLOCK;
		ret = isp_statis_queue_write(&module->afl_statis_queue,
					     &afl_frm_statis);
		afl_iova_addr += ISP_AFL_STATIS_BUF_SIZE;
		afl_vir_addr += ISP_AFL_STATIS_BUF_SIZE;
		afl_kaddr += ISP_AFL_STATIS_BUF_SIZE;
		addr_offset += ISP_AFL_STATIS_BUF_SIZE;
	}
	/*init reserved afl statis buf*/
	module->afl_buf_reserved.phy_addr = afl_iova_addr;
	module->afl_buf_reserved.vir_addr = afl_vir_addr;
	module->afl_buf_reserved.kaddr[0] = afl_kaddr;
	module->afl_buf_reserved.addr_offset = addr_offset;
	module->afl_buf_reserved.pfinfo.mfd[0] = frm_statis.pfinfo.mfd[0];
	module->afl_buf_reserved.buf_size = ISP_AFL_STATIS_BUF_SIZE;
	module->afl_buf_reserved.buf_property = ISP_AFL_BLOCK;

	/*afm statis buf cfg*/
	/* need to be fixed, afm statis buffer no need iommu operation*/
	afm_iova_addr = afl_iova_addr + ISP_AFL_STATIS_BUF_SIZE;
	afm_vir_addr = afl_vir_addr + ISP_AFL_STATIS_BUF_SIZE;
	afm_kaddr = afl_kaddr + ISP_AFL_STATIS_BUF_SIZE;
	addr_offset += ISP_AFL_STATIS_BUF_SIZE;
	/*slip the big buffer to some little buffer*/
	for (cnt = 0; cnt < ISP_AFM_STATIS_BUF_NUM; cnt++) {
		afm_frm_statis.phy_addr = afm_iova_addr;
		afm_frm_statis.vir_addr = afm_vir_addr;
		afm_frm_statis.kaddr[0] = afm_kaddr;
		afm_frm_statis.addr_offset = addr_offset;
		afm_frm_statis.pfinfo.mfd[0] = frm_statis.pfinfo.mfd[0];
		afm_frm_statis.buf_size = ISP_AFM_STATIS_BUF_SIZE;
		afm_frm_statis.buf_property = ISP_AFM_BLOCK;
		ret = isp_statis_queue_write(&module->afm_statis_queue,
					     &afm_frm_statis);
		afm_iova_addr += ISP_AFM_STATIS_BUF_SIZE;
		afm_vir_addr += ISP_AFM_STATIS_BUF_SIZE;
		afm_kaddr += ISP_AFM_STATIS_BUF_SIZE;
		addr_offset += ISP_AFM_STATIS_BUF_SIZE;
	}
	/*init reserved afl statis buf*/
	module->afm_buf_reserved.phy_addr = afm_iova_addr;
	module->afm_buf_reserved.vir_addr = afm_vir_addr;
	module->afm_buf_reserved.kaddr[0] = afm_kaddr;
	module->afm_buf_reserved.addr_offset = addr_offset;
	module->afm_buf_reserved.pfinfo.mfd[0] = frm_statis.pfinfo.mfd[0];
	module->afm_buf_reserved.buf_size = ISP_AFM_STATIS_BUF_SIZE;
	module->afm_buf_reserved.buf_property = ISP_AFM_BLOCK;

	/*binning statis buf cfg*/
	binning_iova_addr = afm_iova_addr + ISP_AFM_STATIS_BUF_SIZE;
	binning_vir_addr = afm_vir_addr + ISP_AFM_STATIS_BUF_SIZE;
	binning_kaddr = afm_kaddr + ISP_AFM_STATIS_BUF_SIZE;
	addr_offset += ISP_AFM_STATIS_BUF_SIZE;
	/*slip the big buffer to some little buffer*/
	for (cnt = 0; cnt < ISP_BINNING_STATIS_BUF_NUM; cnt++) {
		binning_frm_statis.phy_addr = binning_iova_addr;
		binning_frm_statis.vir_addr = binning_vir_addr;
		binning_frm_statis.kaddr[0] = binning_kaddr;
		binning_frm_statis.addr_offset = addr_offset;
		binning_frm_statis.pfinfo.mfd[0] = frm_statis.pfinfo.mfd[0];
		binning_frm_statis.buf_size = ISP_BINNING_STATIS_BUF_SIZE;
		binning_frm_statis.buf_property = ISP_BINNING_BLOCK;
		ret = isp_statis_queue_write(&module->binning_statis_queue,
					     &binning_frm_statis);
		binning_iova_addr += ISP_BINNING_STATIS_BUF_SIZE;
		binning_vir_addr += ISP_BINNING_STATIS_BUF_SIZE;
		binning_kaddr += ISP_BINNING_STATIS_BUF_SIZE;
		addr_offset += ISP_BINNING_STATIS_BUF_SIZE;
	}
	/*init reserved binning statis buf*/
	module->binning_buf_reserved.phy_addr = binning_iova_addr;
	module->binning_buf_reserved.vir_addr = binning_vir_addr;
	module->binning_buf_reserved.kaddr[0] = binning_kaddr;
	module->binning_buf_reserved.kaddr[1] = binning_kaddr;
	module->binning_buf_reserved.addr_offset = addr_offset;
	module->binning_buf_reserved.pfinfo.mfd[0] = frm_statis.pfinfo.mfd[0];
	module->binning_buf_reserved.buf_size = ISP_BINNING_STATIS_BUF_SIZE;
	module->binning_buf_reserved.buf_property = ISP_BINNING_BLOCK;

	/*hist statis buf cfg*/
	hist_iova_addr = binning_iova_addr + ISP_BINNING_STATIS_BUF_SIZE;
	hist_vir_addr = binning_vir_addr + ISP_BINNING_STATIS_BUF_SIZE;
	hist_kaddr = binning_kaddr + ISP_BINNING_STATIS_BUF_SIZE;
	addr_offset += ISP_BINNING_STATIS_BUF_SIZE;
	/*slip the big buffer to some little buffer*/
	for (cnt = 0; cnt < ISP_HIST_STATIS_BUF_NUM; cnt++) {
		hist_frm_statis.phy_addr = hist_iova_addr;
		hist_frm_statis.vir_addr = hist_vir_addr;
		hist_frm_statis.kaddr[0] = hist_kaddr;
		hist_frm_statis.addr_offset = addr_offset;
		hist_frm_statis.pfinfo.mfd[0] = frm_statis.pfinfo.mfd[0];
		hist_frm_statis.buf_size = ISP_HIST_STATIS_BUF_SIZE;
		hist_frm_statis.buf_property = ISP_HIST_BLOCK;
		ret = isp_statis_queue_write(&module->hist_statis_queue,
					     &hist_frm_statis);
		hist_iova_addr += ISP_HIST_STATIS_BUF_SIZE;
		hist_vir_addr += ISP_HIST_STATIS_BUF_SIZE;
		hist_kaddr += ISP_HIST_STATIS_BUF_SIZE;
		addr_offset += ISP_HIST_STATIS_BUF_SIZE;
	}
	/*init reserved hist statis buf*/
	module->hist_buf_reserved.phy_addr = hist_iova_addr;
	module->hist_buf_reserved.vir_addr = hist_vir_addr;
	module->hist_buf_reserved.kaddr[0] = hist_kaddr;
	module->hist_buf_reserved.kaddr[1] = hist_kaddr;
	module->hist_buf_reserved.addr_offset = addr_offset;
	module->hist_buf_reserved.pfinfo.mfd[0] = frm_statis.pfinfo.mfd[0];
	module->hist_buf_reserved.buf_size = ISP_HIST_STATIS_BUF_SIZE;
	module->hist_buf_reserved.buf_property = ISP_HIST_BLOCK;

	pr_debug("cfg statis buf out.\n");

	return ret;
}

int sprd_isp_clr_statis_buf(void *isp_pipe_dev_handle)
{
	struct isp_pipe_dev *dev = NULL;
	struct isp_k_block *isp_k_param = NULL;
	struct isp_statis_module *statis_module_info = NULL;

	if (!isp_pipe_dev_handle) {
		pr_err("%s input handle is NULL\n", __func__);
		return -EINVAL;
	}

	dev = (struct isp_pipe_dev *)isp_pipe_dev_handle;
	isp_k_param = &dev->isp_k_param;
	statis_module_info = &dev->statis_module_info;

	isp_statis_queue_init(&statis_module_info->aem_statis_queue);
	isp_statis_queue_init(&statis_module_info->afl_statis_queue);
	isp_statis_queue_init(&statis_module_info->afm_statis_queue);
	isp_statis_queue_init(
		&statis_module_info->binning_statis_queue);
	isp_statis_queue_init(&statis_module_info->hist_statis_queue);

	memset((void *)&statis_module_info->aem_buf_reserved, 0,
		sizeof(struct isp_statis_buf));
	memset((void *)&statis_module_info->afl_buf_reserved, 0,
		sizeof(struct isp_statis_buf));
	memset((void *)&statis_module_info->afm_buf_reserved, 0,
		sizeof(struct isp_statis_buf));
	memset((void *)&statis_module_info->binning_buf_reserved, 0,
		sizeof(struct isp_statis_buf));
	memset((void *)&statis_module_info->hist_buf_reserved, 0,
		sizeof(struct isp_statis_buf));

	statis_module_info->aem_statis_cnt = 0;
	statis_module_info->afl_statis_cnt = 0;
	statis_module_info->afm_statis_cnt = 0;
	statis_module_info->binning_statis_cnt = 0;
	statis_module_info->hist_statis_cnt = 0;

	isp_statis_frm_queue_clear(&statis_module_info->aem_statis_frm_queue);
	isp_statis_frm_queue_clear(&statis_module_info->afl_statis_frm_queue);
	isp_statis_frm_queue_clear(&statis_module_info->afm_statis_frm_queue);
	isp_statis_frm_queue_clear(
		&statis_module_info->binning_statis_frm_queue);
	isp_statis_frm_queue_clear(&statis_module_info->hist_statis_frm_queue);

	pfiommu_free_addr_with_id(&statis_module_info->img_statis_buf.pfinfo,
				SPRD_IOMMU_EX_CH_WRITE, STATIS_WR_CH_ID);
	memset(&statis_module_info->img_statis_buf.pfinfo, 0,
		sizeof(struct pfiommu_info));

	return 0;
}

int sprd_isp_set_statis_addr(struct isp_pipe_dev *dev,
	struct isp_statis_buf_input *parm)
{
	int ret = 0;
	struct isp_statis_buf frm_statis;
	struct isp_statis_buf *statis_buf_reserved = NULL;
	struct isp_statis_module *module = NULL;
	struct isp_statis_buf_queue *statis_queue = NULL;

	module = &dev->statis_module_info;

	switch (parm->buf_property) {
	case ISP_AEM_BLOCK:
		statis_queue = &module->aem_statis_queue;
		statis_buf_reserved = &module->aem_buf_reserved;
		break;
	case ISP_AFL_BLOCK:
		statis_queue = &module->afl_statis_queue;
		statis_buf_reserved = &module->afl_buf_reserved;
		break;
	case ISP_AFM_BLOCK:
		statis_queue = &module->afm_statis_queue;
		statis_buf_reserved = &module->afm_buf_reserved;
		break;
	case ISP_BINNING_BLOCK:
		statis_queue = &module->binning_statis_queue;
		statis_buf_reserved = &module->binning_buf_reserved;
		break;
	case ISP_HIST_BLOCK:
		statis_queue = &module->hist_statis_queue;
		statis_buf_reserved = &module->hist_buf_reserved;
		break;
	default:
		pr_err("fail to get statis block %d\n", parm->buf_property);
		return -EFAULT;
	}

	/*config statis buf*/
	if (parm->is_statis_buf_reserved == 1) {
		statis_buf_reserved->phy_addr = parm->phy_addr;
		statis_buf_reserved->vir_addr = parm->vir_addr;
		statis_buf_reserved->addr_offset = parm->addr_offset;
		statis_buf_reserved->kaddr[0] = parm->kaddr[0];
		statis_buf_reserved->kaddr[1] = parm->kaddr[1];
		statis_buf_reserved->buf_size = parm->buf_size;
		statis_buf_reserved->buf_property =
			parm->buf_property;
		statis_buf_reserved->pfinfo.dev = &s_isp_pdev->dev;
		statis_buf_reserved->pfinfo.mfd[0] = parm->reserved[0];
	} else {
		memset((void *)&frm_statis, 0, sizeof(struct isp_statis_buf));
		frm_statis.phy_addr = parm->phy_addr;
		frm_statis.vir_addr = parm->vir_addr;
		frm_statis.addr_offset = parm->addr_offset;
		frm_statis.kaddr[0] = parm->kaddr[0];
		frm_statis.kaddr[1] = parm->kaddr[1];
		frm_statis.buf_size = parm->buf_size;
		frm_statis.buf_property = parm->buf_property;

		frm_statis.pfinfo.dev = &s_isp_pdev->dev;
		frm_statis.pfinfo.mfd[0] = parm->reserved[0];
		/*when the statis is running, we need not map again*/
		ret = isp_statis_queue_write(statis_queue, &frm_statis);
	}

	pr_debug("set statis buf addr done.\n");
	return ret;
}
