// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc QOGIRN6PRO VPU driver
 *
 * Copyright (C) 2019 Unisoc, Inc.
 */

#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/dma-heap.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/sprd_iommu.h>
#include <linux/sprd_ion.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <linux/notifier.h>
#include <linux/compat.h>
#include <linux/of.h>
#include "vpu_drv.h"
#include "vpu_sys.h"

#define LEN_MAX 100

struct vpu_iommu_map_entry {
	struct list_head list;

	int fd;
	unsigned long iova_addr;
	size_t iova_size;

	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
	struct sg_table *table;
	void *inst_ptr;
};

struct system_heap_buffer {
	struct dma_heap *heap;
	struct list_head attachments;
	struct mutex lock;
	unsigned long len;
	struct sg_table sg_table;
	int vmap_cnt;
	void *vaddr;

	bool uncached;
};

void vpu_qos_config(struct vpu_platform_data *data)
{
	unsigned int i, vpu_qos_num, dpu_vpu_qos_num;
	int reg_val;

	/*static volatile unsigned int *base_addr_virt;*/
	unsigned int *base_addr_virt;

	clock_enable(data);

	vpu_qos_num = ARRAY_SIZE(vpu_mtx_qos);
	dpu_vpu_qos_num = ARRAY_SIZE(dpu_vpu_mtx_qos);

	for (i = 0; i < vpu_qos_num; i++) {
		base_addr_virt = ioremap(VPU_SOC_QOS_BASE + vpu_mtx_qos[i].offset, 4);
		reg_val = readl_relaxed((void __iomem *)base_addr_virt);
		writel_relaxed((((reg_val) & (~vpu_mtx_qos[i].mask))
		| (vpu_mtx_qos[i].value)), (void __iomem *)base_addr_virt);
		iounmap(base_addr_virt);
	}

	for (i = 0; i < dpu_vpu_qos_num; i++) {
		base_addr_virt = ioremap(DPU_VPU_SOC_QOS_BASE + dpu_vpu_mtx_qos[i].offset, 4);
		reg_val = readl_relaxed((void __iomem *)base_addr_virt);
		writel_relaxed((((reg_val) & (~dpu_vpu_mtx_qos[i].mask))
		| (dpu_vpu_mtx_qos[i].value)), (void __iomem *)base_addr_virt);
		iounmap(base_addr_virt);
	}

	clock_disable(data);
}

static int handle_vpu_dec_interrupt(struct vpu_platform_data *data, int *status)
{
	int i;
	int int_status;
	int mmu_status;
	struct device *dev = data->dev;

	int_status = readl_relaxed(data->glb_reg_base + VPU_INT_RAW_OFF);
	mmu_status = readl_relaxed(data->vpu_base + VPU_MMU_INT_STS_OFF);
	*status |= int_status | (mmu_status << 16);

	if (((int_status & 0xffff) == 0) &&
		((mmu_status & 0xff) == 0)) {
		dev_info(dev, "%s dec IRQ_NONE int_status 0x%x 0x%x",
			__func__, int_status, mmu_status);
		return IRQ_NONE;
	}

	if (int_status & DEC_BSM_OVF_ERR)
		dev_err(dev, "dec_bsm_overflow");

	if (int_status & DEC_VLD_ERR)
		dev_err(dev, "dec_vld_err");

	if (int_status & DEC_TIMEOUT_ERR)
		dev_err(dev, "dec_timeout");

	if (int_status & DEC_MMU_INT_ERR)
		dev_err(dev, "dec_mmu_int_err");

	if (int_status & DEC_AFBCD_HERR)
		dev_err(dev, "dec_afbcd_herr");

	if (int_status & DEC_AFBCD_PERR)
		dev_err(dev, "dec_afbcd_perr");

	if (mmu_status & MMU_RD_WR_ERR) {
		/* mmu ERR */
		dev_err(dev, "dec iommu addr: 0x%x\n",
		       readl_relaxed(data->vpu_base + VPU_MMU_INT_RAW_OFF));

		for (i = 0x44; i <= 0x68; i += 4)
			dev_info(dev, "addr 0x%x is 0x%x\n", i,
				readl_relaxed(data->vpu_base + i));
		WARN_ON_ONCE(1);
	}

	/* clear VSP accelerator interrupt bit */
	clr_vpu_interrupt_mask(data);

	return IRQ_HANDLED;
}

static int handle_vpu_enc_interrupt(struct vpu_platform_data *data, int *status)
{
	int i;
	int int_status;
	int mmu_status;
	struct device *dev = data->dev;

	int_status = readl_relaxed(data->glb_reg_base + VPU_INT_RAW_OFF);
	mmu_status = readl_relaxed(data->vpu_base + VPU_MMU_INT_RAW_OFF);
	*status |= int_status | (mmu_status << 16);

	if (((int_status & 0x7f) == 0) &&
		((mmu_status & 0xff) == 0)) {
		dev_info(dev, "%s enc IRQ_NONE int_status 0x%x 0x%x",
			__func__, int_status, mmu_status);
		return IRQ_NONE;
	}

	if (int_status & ENC_BSM_OVF_ERR)
		dev_err(dev, "enc_bsm_overflow");

	if (int_status & ENC_TIMEOUT_ERR)
		dev_err(dev, "enc_timeout");

	if (int_status & ENC_AFBCD_HERR)
		dev_err(dev, "enc_afbcd_herr");

	if (int_status & ENC_AFBCD_PERR)
		dev_err(dev, "enc_afbcd_perr");

	if (int_status & ENC_MMU_INT_ERR)
		dev_err(dev, "enc_mmu_int_err");

	if (mmu_status & MMU_RD_WR_ERR) {
		/* mmu ERR */
		dev_err(dev, "enc iommu addr: 0x%x\n",
		       readl_relaxed(data->vpu_base + VPU_MMU_INT_RAW_OFF));

		for (i = 0x44; i <= 0x68; i += 4)
			dev_info(dev, "addr 0x%x is 0x%x\n", i,
				readl_relaxed(data->vpu_base + i));
		WARN_ON_ONCE(1);
	}

	/* clear VSP accelerator interrupt bit */
	clr_vpu_interrupt_mask(data);

	return IRQ_HANDLED;
}

void clr_vpu_interrupt_mask(struct vpu_platform_data *data)
{
	int vpu_int_mask = 0;
	int mmu_int_mask = 0;

	vpu_int_mask = 0x1fff;
	mmu_int_mask = 0xff;

	/* set the interrupt mask 0 */
	writel_relaxed(0, data->glb_reg_base + VPU_INT_MASK_OFF);
	writel_relaxed(0, data->vpu_base + VPU_MMU_INT_MASK_OFF);

	/* clear vsp int */
	writel_relaxed(vpu_int_mask, data->glb_reg_base + VPU_INT_CLR_OFF);
	writel_relaxed(mmu_int_mask, data->vpu_base + VPU_MMU_INT_CLR_OFF);
}

static irqreturn_t vpu_dec_isr_handler(struct vpu_platform_data *data)
{
	int ret, status = 0;
	struct vpu_fp *inst_ptr = NULL;

	if (data == NULL) {
		pr_err("%s error occurred, data == NULL\n", __func__);
		return IRQ_NONE;
	}
	inst_ptr = data->inst_ptr;

	if (inst_ptr == NULL) {
		dev_err(data->dev, "%s error occurred, inst_ptr == NULL\n", __func__);
		return IRQ_HANDLED;
	}

	if (inst_ptr->is_clock_enabled == false) {
		dev_err(data->dev, " vpu clk is disabled");
		return IRQ_HANDLED;
	}

	/* check which module occur interrupt and clear corresponding bit */
	ret = handle_vpu_dec_interrupt(data, &status);
	if (ret == IRQ_NONE)
		return IRQ_NONE;

	data->vpu_int_status = status;
	data->condition_work = 1;
	wake_up_interruptible(&data->wait_queue_work);

	return IRQ_HANDLED;
}

static irqreturn_t vpu_enc_isr_handler(struct vpu_platform_data *data)
{
	int ret, status = 0;
	struct vpu_fp *inst_ptr = NULL;

	if (data == NULL) {
		pr_err("%s error occurred, data == NULL\n", __func__);
		return IRQ_NONE;
	}
	inst_ptr = data->inst_ptr;

	if (inst_ptr == NULL) {
		dev_err(data->dev, "%s error occurred, inst_ptr == NULL\n", __func__);
		return IRQ_HANDLED;
	}

	if (inst_ptr->is_clock_enabled == false) {
		dev_err(data->dev, " vpu clk is disabled");
		return IRQ_HANDLED;
	}

	/* check which module occur interrupt and clear corresponding bit */
	ret = handle_vpu_enc_interrupt(data, &status);
	if (ret == IRQ_NONE)
		return IRQ_NONE;

	data->vpu_int_status = status;
	data->condition_work = 1;
	wake_up_interruptible(&data->wait_queue_work);

	return IRQ_HANDLED;
}

irqreturn_t enc_core0_isr(int irq, void *data)
{
	struct vpu_platform_data *vpu_core = data;
	int ret = 0;

	dev_dbg(vpu_core->dev, "%s, isr", vpu_core->p_data->name);

	ret = vpu_enc_isr_handler(data);

	/*Do enc core0 specified work here, if needed.*/

	return ret;
}

irqreturn_t enc_core1_isr(int irq, void *data)
{
	struct vpu_platform_data *vpu_core = data;
	int ret = 0;
	dev_dbg(vpu_core->dev, "%s, isr", vpu_core->p_data->name);

	ret = vpu_enc_isr_handler(data);

	/*Do enc core1 specified work here, if needed.*/

	return ret;
}

irqreturn_t dec_core0_isr(int irq, void *data)
{
	struct vpu_platform_data *vpu_core = data;
	int ret = 0;

	dev_dbg(vpu_core->dev, "%s, isr", vpu_core->p_data->name);

	ret = vpu_dec_isr_handler(data);

	/*Do dec core0 specified work here, if needed.*/

	return ret;
}

struct clk *get_clk_src_name(struct clock_name_map_t clock_name_map[],
				unsigned int freq_level,
				unsigned int max_freq_level)
{
	if (freq_level >= max_freq_level) {
		pr_info("set freq_level to max_freq_level\n");
		freq_level = max_freq_level - 1;
	}

	pr_debug("VPU_CONFIG_FREQ %d freq_level_name %s\n", freq_level,
		 clock_name_map[freq_level].name);
	return clock_name_map[freq_level].clk_parent;
}

int find_freq_level(struct clock_name_map_t clock_name_map[],
			unsigned long freq,
			unsigned int max_freq_level)
{
	int level = 0;
	int i;

	for (i = 0; i < max_freq_level; i++) {
		if (clock_name_map[i].freq == freq) {
			level = i;
			break;
		}
	}
	return level;
}

#ifdef CONFIG_COMPAT
long compat_vpu_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	if (!filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	return filp->f_op->unlocked_ioctl(filp, cmd, (unsigned long)
						  compat_ptr(arg));
}
#endif

void vpu_check_pw_status(struct vpu_platform_data *data)
{
	int ret = 0;
	u32 dpu_vsp_eb, dpu_vsp_apb_regs;

	regmap_read(data->regs[VPU_DOMAIN_EB].gpr,
			data->regs[VPU_DOMAIN_EB].reg, &dpu_vsp_eb);

	/*aon_apb regs BIT(21) DPU_VSP_EB*/
	if ((dpu_vsp_eb & data->regs[VPU_DOMAIN_EB].mask) !=
			data->regs[VPU_DOMAIN_EB].mask) {
		dev_err(data->dev, "dpu_vsp_eb 0x%x\n", dpu_vsp_eb);
		ret = regmap_update_bits(data->regs[VPU_DOMAIN_EB].gpr,
					data->regs[VPU_DOMAIN_EB].reg,
					data->regs[VPU_DOMAIN_EB].mask,
					data->regs[VPU_DOMAIN_EB].mask);
	}

	/*
	 * dpu_vsp_apb_regs 0x30100000
	 * APB_EB(dev_eb) 0x0000 bit3:enc0_eb bit4:enc1_eb bit 5:dec_eb
	 * APB_RST 0x0004 bit3:enc0_rst bit4:enc1_rst bit 5:dec_rst
	 */
	regmap_read(data->regs[RESET].gpr, 0x0, &dpu_vsp_apb_regs); /*dev_eb*/

	if ((dpu_vsp_apb_regs & data->p_data->dev_eb_mask) !=
			data->p_data->dev_eb_mask) {
		dev_err(data->dev, "dpu_vsp_apb_regs APB_EB dev_eb 0x%x\n", dpu_vsp_apb_regs);
		ret = regmap_update_bits(data->regs[RESET].gpr, 0x0,
					data->p_data->dev_eb_mask, data->p_data->dev_eb_mask);
	}

}

int vsp_get_dmabuf(int fd, struct dma_buf **dmabuf, void **buf, size_t *size)
{
	struct system_heap_buffer *buffer;

	if (fd < 0 && !dmabuf) {
		pr_err("%s, input fd: %d, dmabuf: %p error\n", __func__, fd, dmabuf);
		return -EINVAL;
	}

	if (fd >= 0) {
		*dmabuf = dma_buf_get(fd);
		if (IS_ERR_OR_NULL(*dmabuf)) {
			pr_err("%s, dmabuf error: %p !\n", __func__, *dmabuf);
			return PTR_ERR(*dmabuf);
		}
		buffer = (*dmabuf)->priv;
		dma_buf_put(*dmabuf);
	} else {
		buffer = (*dmabuf)->priv;
	}

	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	*buf = (void *)buffer;
	*size = buffer->len;

	return 0;
}

int get_iova(void *inst_ptr, struct vpu_platform_data *data,
		 struct iommu_map_data *mapdata, void __user *arg)
{
	int ret = 0;
	struct sprd_iommu_map_data iommu_map_data;
	struct sprd_iommu_unmap_data iommu_ummap_data;
	struct device *dev = data->dev;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment = NULL;
	struct sg_table *table = NULL;
	struct vpu_iommu_map_entry *entry;

	clock_enable(data);
	vpu_check_pw_status(data);
	ret = vsp_get_dmabuf(mapdata->fd, &dmabuf,
					&(iommu_map_data.buf),
					&iommu_map_data.iova_size);

	if (ret) {
		pr_err("vpu_get_dmabuf failed: ret=%d\n", ret);
		goto err_get_dmabuf;
	}

	if (dma_set_mask(data->dev, DMA_BIT_MASK(64))) {
		pr_err("mydev: No suitable DMA available\n");
		goto ignore_this_device;
	}

	attachment = dma_buf_attach(dmabuf, data->dev);
	if (IS_ERR_OR_NULL(attachment)) {
		pr_err("Failed to attach dmabuf=%p\n", dmabuf);
		ret = PTR_ERR(attachment);
		goto err_attach;
	}

	table = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(table)) {
		pr_err("Failed to map attachment=%p\n", attachment);
		ret = PTR_ERR(table);
		goto err_map_attachment;
	}

	iommu_map_data.ch_type = SPRD_IOMMU_FM_CH_RW;
	ret = sprd_iommu_map(data->dev, &iommu_map_data);
	if (!ret) {
		mutex_lock(&data->map_lock);
		entry = kzalloc(sizeof(*entry), GFP_KERNEL);
		if (!entry) {
			mutex_unlock(&data->map_lock);
			pr_err("fatal error! kzalloc fail!\n");
			iommu_ummap_data.iova_addr = iommu_map_data.iova_addr;
			iommu_ummap_data.iova_size = iommu_map_data.iova_size;
			iommu_ummap_data.ch_type = SPRD_IOMMU_FM_CH_RW;
			iommu_ummap_data.buf = NULL;
			ret = -ENOMEM;
			goto err_kzalloc;
		}
		entry->fd = mapdata->fd;
		entry->iova_addr = iommu_map_data.iova_addr;
		entry->iova_size = iommu_map_data.iova_size;
		entry->dmabuf = dmabuf;
		entry->attachment = attachment;
		entry->table = table;
		entry->inst_ptr = inst_ptr;
		list_add(&entry->list, &data->map_list);
		mutex_unlock(&data->map_lock);

		mapdata->iova_addr = iommu_map_data.iova_addr;
		mapdata->size = iommu_map_data.iova_size;
		dev_dbg(dev, "vpu iommu map success iova addr 0x%llx size 0x%llx\n",
			mapdata->iova_addr, mapdata->size);
		ret = copy_to_user((void __user *)arg, (void *)mapdata,
					sizeof(struct iommu_map_data));
		if (ret) {
			dev_err(dev, "fatal error! copy_to_user failed, ret=%d\n", ret);
			goto err_copy_to_user;
		}
	} else {
		dev_err(dev, "vpu iommu map failed, ret=%d, map_size=%zu\n",
			ret, iommu_map_data.iova_size);
		goto err_iommu_map;
	}
	clock_disable(data);
	return ret;

err_copy_to_user:
		mutex_lock(&data->map_lock);
		list_del(&entry->list);
		kfree(entry);
		mutex_unlock(&data->map_lock);
err_kzalloc:
		ret = sprd_iommu_unmap(data->dev, &iommu_ummap_data);
		if (ret) {
			pr_err("sprd_iommu_unmap failed, ret=%d, addr&size: 0x%lx 0x%zx\n",
				ret, iommu_ummap_data.iova_addr, iommu_ummap_data.iova_size);
		}
err_iommu_map:
		dma_buf_unmap_attachment(attachment, table, DMA_BIDIRECTIONAL);
err_map_attachment:
		dma_buf_detach(dmabuf, attachment);
err_attach:
err_get_dmabuf:
ignore_this_device:
		clock_disable(data);

		return ret;
}

int free_iova(void *inst_ptr, struct vpu_platform_data *data,
		  struct iommu_map_data *ummapdata)
{
	int ret = 0;
	struct vpu_iommu_map_entry *entry = NULL;
	struct sprd_iommu_unmap_data iommu_ummap_data;
	int b_find = 0;

	clock_enable(data);
	mutex_lock(&data->map_lock);
	list_for_each_entry(entry, &data->map_list, list) {
		if (entry->iova_addr == ummapdata->iova_addr &&
			entry->iova_size == ummapdata->size &&
			entry->inst_ptr == inst_ptr) {
			b_find = 1;
			break;
		}
	}

	if (b_find) {
		iommu_ummap_data.iova_addr = entry->iova_addr;
		iommu_ummap_data.iova_size = entry->iova_size;
		iommu_ummap_data.ch_type = SPRD_IOMMU_FM_CH_RW;
		iommu_ummap_data.buf = NULL;
		list_del(&entry->list);
		pr_debug("success to find node(inst %p, iova_addr=%#llx, size=%llu)\n",
			inst_ptr, ummapdata->iova_addr, ummapdata->size);
	} else {
		pr_err("fatal error! not find node(inst %p, iova_addr=%#llx, size=%llu)\n",
				inst_ptr, ummapdata->iova_addr, ummapdata->size);
		mutex_unlock(&data->map_lock);
		clock_disable(data);
		return -EFAULT;
	}
	mutex_unlock(&data->map_lock);

	ret = sprd_iommu_unmap(data->dev, &iommu_ummap_data);
	if (ret) {
		pr_err("sprd_iommu_unmap failed: ret=%d, iova_addr=%#llx, size=%llu\n",
			ret, ummapdata->iova_addr, ummapdata->size);
		clock_disable(data);
		return ret;
	}
	pr_debug("sprd_iommu_unmap success: iova_addr=%#llx size=%llu\n",
		ummapdata->iova_addr, ummapdata->size);
	dma_buf_unmap_attachment(entry->attachment, entry->table, DMA_BIDIRECTIONAL);
	dma_buf_detach(entry->dmabuf, entry->attachment);
	kfree(entry);

	clock_disable(data);

	return ret;
}

void non_free_bufs_check(void *inst_ptr, struct vpu_platform_data *data)
{
	struct vpu_iommu_map_entry *entry = NULL;
	struct sprd_iommu_unmap_data unmapdata = {0};
	int ret = 0;
	int b_find = 0;

	clock_enable(data);
	mutex_lock(&data->map_lock);

	do {
		b_find  = 0;
		list_for_each_entry(entry, &data->map_list, list) {
			if (entry->inst_ptr == inst_ptr) {
				unmapdata.iova_addr = entry->iova_addr;
				unmapdata.iova_size = entry->iova_size;
				unmapdata.ch_type = SPRD_IOMMU_FM_CH_RW;
				unmapdata.buf = NULL;
				b_find = 1;
				list_del(&entry->list);
				break;
			}
		}
		if (b_find) {
			pr_info("%s, inst %p, iova_addr=%#lx, size=%zu)\n", __func__,
				entry->inst_ptr, entry->iova_addr, entry->iova_size);

			ret = sprd_iommu_unmap(data->dev, &unmapdata);
			if (ret) {
				pr_err("sprd_iommu_unmap failed: ret=%d, iova_addr=%#lx, size=%zu\n",
					ret, unmapdata.iova_addr, unmapdata.iova_size);
			}

			kfree(entry);
		}
	} while (b_find);

	mutex_unlock(&data->map_lock);
	clock_disable(data);

}

int get_clk(struct vpu_platform_data *data, struct device_node *np)
{
	int ret = 0, i, j = 0;
	struct clk *clk_dev_eb;
	struct clk *core_clk;
	struct clk *clk_domain_eb;
	struct clk *clk_ckg_eb;
	struct clk *clk_ahb_vsp;
	struct clk *clk_parent;
	struct device *dev = data->dev;

	for (i = 0; i < ARRAY_SIZE(vpu_clk_src); i++) {
		//struct clk *clk_parent;
		unsigned long frequency;

		clk_parent = of_clk_get_by_name(np, vpu_clk_src[i]);
		if (IS_ERR_OR_NULL(clk_parent)) {
			dev_info(dev, "clk %s not found,continue to find next clock\n",
				vpu_clk_src[i]);
			continue;
		}
		frequency = clk_get_rate(clk_parent);

		data->clock_name_map[j].name = vpu_clk_src[i];
		data->clock_name_map[j].freq = frequency;
		data->clock_name_map[j].clk_parent = clk_parent;

		dev_info(dev, "vpu clk in dts file: clk[%d] = (%ld, %s)\n", j,
			frequency, data->clock_name_map[j].name);
		j++;
	}
	data->max_freq_level = j;


	clk_domain_eb = devm_clk_get(data->dev, "clk_domain_eb");

	if (IS_ERR_OR_NULL(clk_domain_eb)) {
		dev_err(dev, "Failed: Can't get clock [%s]! %p\n",
		       "clk_domain_eb", clk_domain_eb);
		data->clk.clk_domain_eb = NULL;
		ret = -EINVAL;
		goto errout;
	} else
		data->clk.clk_domain_eb = clk_domain_eb;

	clk_dev_eb =
		devm_clk_get(data->dev, "clk_dev_eb");

	if (IS_ERR_OR_NULL(clk_dev_eb)) {
		dev_err(dev, "Failed: Can't get clock [%s]! %p\n",
		       "clk_dev_eb", clk_dev_eb);
		ret = -EINVAL;
		data->clk.clk_dev_eb = NULL;
		goto errout;
	} else
		data->clk.clk_dev_eb = clk_dev_eb;

	core_clk = devm_clk_get(data->dev, "clk_vsp");

	if (IS_ERR_OR_NULL(core_clk)) {
		dev_err(dev, "Failed: Can't get clock [%s]! %p\n", "core_clk",
		       core_clk);
		ret = -EINVAL;
		data->clk.core_clk = NULL;
		goto errout;
	} else
		data->clk.core_clk = core_clk;

	clk_ahb_vsp =
		devm_clk_get(data->dev, "clk_ahb_vsp");

	if (IS_ERR_OR_NULL(clk_ahb_vsp)) {
		dev_err(dev, "Failed: Can't get clock [%s]! %p\n",
		       "clk_ahb_vsp", clk_ahb_vsp);
		ret = -EINVAL;
		goto errout;
	} else
		data->clk.clk_ahb_vsp = clk_ahb_vsp;

	clk_ckg_eb =
		devm_clk_get(data->dev, "clk_ckg_eb");

	if (IS_ERR_OR_NULL(clk_ckg_eb)) {
		dev_err(dev, "Failed: Can't get clock [%s]! %p\n",
		       "clk_ckg_eb", clk_ckg_eb);
		ret = -EINVAL;
		goto errout;
	} else
		data->clk.clk_ckg_eb = clk_ckg_eb;

	clk_parent = devm_clk_get(data->dev,
		       "clk_ahb_vsp_parent");

	if (IS_ERR_OR_NULL(clk_parent)) {
		dev_err(dev, "clock[%s]: failed to get parent in probe!\n",
		       "clk_ahb_vsp_parent");
		ret = -EINVAL;
		goto errout;
	} else
		data->clk.ahb_parent_clk = clk_parent;

errout:
	return ret;
}

int clock_enable(struct vpu_platform_data *data)
{
	int ret = 0;
	struct vpu_clk *clk = &data->clk;
	struct device *dev = data->dev;

	if (clk->clk_domain_eb) {
		ret = clk_prepare_enable(clk->clk_domain_eb);
		if (ret) {
			dev_err(dev, "vsp clk_domain_eb: clk_enable failed!\n");
			goto error1;
		}
		dev_dbg(dev, "vsp clk_domain_eb: clk_prepare_enable ok.\n");
	}


	if (clk->clk_dev_eb) {
		ret = clk_prepare_enable(clk->clk_dev_eb);
		if (ret) {
			dev_err(dev, "clk_dev_eb: clk_prepare_enable failed!\n");
			goto error2;
		}
		dev_dbg(dev, "clk_dev_eb: clk_prepare_enable ok.\n");
	}

	if (clk->clk_ahb_vsp) {
		ret = clk_set_parent(clk->clk_ahb_vsp, clk->ahb_parent_clk);
		if (ret) {
			dev_err(dev, "clock[%s]: clk_set_parent() failed!",
				"ahb_parent_clk");
			goto error3;
		}
		ret = clk_prepare_enable(clk->clk_ahb_vsp);
		if (ret) {
			dev_err(dev, "clk_ahb_vsp: clk_prepare_enable failed!\n");
			goto error3;
		}
		dev_dbg(dev, "clk_ahb_vsp: clk_prepare_enable ok.\n");
	}

	ret = clk_set_parent(clk->core_clk, clk->core_parent_clk);
	if (ret) {
		dev_err(dev, "clock[%s]: clk_set_parent() failed!", "clk_core");
		goto error4;
	}

	ret = clk_prepare_enable(clk->core_clk);
	if (ret) {
		dev_err(dev, "core_clk: clk_prepare_enable failed!\n");
		goto error4;
	}
	dev_dbg(dev, "vsp_clk: clk_prepare_enable ok.\n");

	dev_dbg(data->dev, "%s %d,OK\n", __func__, __LINE__);

	return ret;

error4:
	clk_disable_unprepare(clk->clk_ahb_vsp);
error3:
	clk_disable_unprepare(clk->clk_dev_eb);
error2:
	clk_disable_unprepare(clk->clk_domain_eb);
error1:
	return ret;
}

void clock_disable(struct vpu_platform_data *data)
{
	struct vpu_clk *clk = &data->clk;

	clk_disable_unprepare(clk->core_clk);
	clk_disable_unprepare(clk->clk_ahb_vsp);
	clk_disable_unprepare(clk->clk_dev_eb);
	clk_disable_unprepare(clk->clk_domain_eb);
	dev_dbg(data->dev, "%s %d,OK\n", __func__, __LINE__);
}
