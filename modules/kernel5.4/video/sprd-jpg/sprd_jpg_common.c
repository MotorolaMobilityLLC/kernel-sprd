// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc UMS512 VSP driver
 *
 * Copyright (C) 2019 Unisoc, Inc.
 */

#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/sprd_iommu.h>
#include <linux/ion.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include "sprd_jpg.h"
#include "sprd_jpg_common.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-jpg: " fmt

struct jpg_iommu_map_entry {
	struct list_head list;

	int fd;
	unsigned long iova_addr;
	size_t iova_size;

	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
	struct sg_table *table;
};

struct clk *jpg_get_clk_src_name(struct clock_name_map_t clock_name_map[],
				unsigned int freq_level,
				unsigned int max_freq_level)
{
	if (freq_level >= max_freq_level) {
		pr_info("set freq_level to 0\n");
		freq_level = 0;
	}

	pr_debug(" freq_level %d %s\n", freq_level,
		 clock_name_map[freq_level].name);
	return clock_name_map[freq_level].clk_parent;
}

int jpg_find_freq_level(struct clock_name_map_t clock_name_map[],
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

int jpg_get_mm_clk(struct jpg_dev_t *hw_dev)
{
	int ret = 0;
	struct clk *jpg_domain_eb;
	struct clk *clk_aon_jpg_emc_eb;
	struct clk *jpg_dev_eb;
	struct clk *jpg_ckg_eb;
	struct clk *clk_vsp_mq_ahb_eb;
	struct clk *clk_ahb_vsp;
	struct clk *clk_emc_vsp;
	struct clk *jpg_clk;
	struct clk *clk_parent;

	jpg_domain_eb = devm_clk_get(hw_dev->jpg_dev, "jpg_domain_eb");

	if (IS_ERR(jpg_domain_eb)) {
		dev_err(hw_dev->jpg_dev,
		       "Failed : Can't get clock [%s]!\n", "jpg_domain_eb");
		dev_err(hw_dev->jpg_dev,
		       "jpg_domain_eb =  %p\n", jpg_domain_eb);
		hw_dev->jpg_domain_eb = NULL;
		ret = PTR_ERR(jpg_domain_eb);
	} else {
		hw_dev->jpg_domain_eb = jpg_domain_eb;
	}

	if (hw_dev->version == SHARKL3) {
		clk_aon_jpg_emc_eb = devm_clk_get(hw_dev->jpg_dev,
					"clk_aon_jpg_emc_eb");
		if (IS_ERR(clk_aon_jpg_emc_eb)) {
			dev_err(hw_dev->jpg_dev,
				"Can't get clock [%s]!\n", "clk_aon_jpg_emc_eb");
			dev_err(hw_dev->jpg_dev,
				"clk_aon_jpg_emc_eb = %p\n", clk_aon_jpg_emc_eb);
			hw_dev->clk_aon_jpg_emc_eb = NULL;
			ret = PTR_ERR(clk_aon_jpg_emc_eb);
		} else {
			hw_dev->clk_aon_jpg_emc_eb = clk_aon_jpg_emc_eb;
		}
	}

	jpg_dev_eb =
	    devm_clk_get(hw_dev->jpg_dev, "jpg_dev_eb");
	if (IS_ERR(jpg_dev_eb)) {
		dev_err(hw_dev->jpg_dev,
			"Failed : Can't get clock [%s]!\n", "jpg_dev_eb");
		dev_err(hw_dev->jpg_dev,
			"jpg_dev_eb =  %p\n", jpg_dev_eb);
		hw_dev->jpg_dev_eb = NULL;
		ret = PTR_ERR(jpg_dev_eb);
	} else {
		hw_dev->jpg_dev_eb = jpg_dev_eb;
	}

	jpg_ckg_eb =
	    devm_clk_get(hw_dev->jpg_dev, "jpg_ckg_eb");

	if (IS_ERR(jpg_ckg_eb)) {
		dev_err(hw_dev->jpg_dev,
			"Failed : Can't get clock [%s]!\n",
			"jpg_ckg_eb");
		dev_err(hw_dev->jpg_dev,
			"jpg_ckg_eb =  %p\n", jpg_ckg_eb);
		hw_dev->jpg_ckg_eb = NULL;
		ret = PTR_ERR(jpg_ckg_eb);
	} else {
		hw_dev->jpg_ckg_eb = jpg_ckg_eb;
	}

	if (hw_dev->version == PIKE2) {
		clk_vsp_mq_ahb_eb =
		    devm_clk_get(hw_dev->jpg_dev, "clk_vsp_mq_ahb_eb");

		if (IS_ERR(clk_vsp_mq_ahb_eb)) {
			dev_err(hw_dev->jpg_dev,
				"Failed: Can't get clock [%s]! %p\n",
				"clk_vsp_mq_ahb_eb", clk_vsp_mq_ahb_eb);
			hw_dev->clk_vsp_mq_ahb_eb = NULL;
			ret = PTR_ERR(clk_vsp_mq_ahb_eb);
		} else {
			hw_dev->clk_vsp_mq_ahb_eb = clk_vsp_mq_ahb_eb;
		}
	}

	if (hw_dev->version == SHARKL3) {
		clk_ahb_vsp =
		    devm_clk_get(hw_dev->jpg_dev, "clk_ahb_vsp");

		if (IS_ERR(clk_ahb_vsp)) {
			dev_err(hw_dev->jpg_dev,
				"Failed: Can't get clock [%s]! %p\n",
				"clk_ahb_vsp", clk_ahb_vsp);
			hw_dev->clk_ahb_vsp = NULL;
			ret = PTR_ERR(clk_ahb_vsp);
		} else
			hw_dev->clk_ahb_vsp = clk_ahb_vsp;

		clk_parent = devm_clk_get(hw_dev->jpg_dev,
				"clk_ahb_vsp_parent");
		if (IS_ERR(clk_parent)) {
			dev_err(hw_dev->jpg_dev,
				"clock[%s]: failed to get parent in probe!\n",
				"clk_ahb_vsp_parent");
			ret = PTR_ERR(clk_parent);
		} else
			hw_dev->ahb_parent_clk = clk_parent;

		clk_emc_vsp =
			devm_clk_get(hw_dev->jpg_dev, "clk_emc_vsp");

		if (IS_ERR(clk_emc_vsp)) {
			dev_err(hw_dev->jpg_dev,
				"Failed: Can't get clock [%s]! %p\n",
				"clk_emc_vsp", clk_emc_vsp);
			hw_dev->clk_emc_vsp = NULL;
			ret = PTR_ERR(clk_emc_vsp);
		} else
			hw_dev->clk_emc_vsp = clk_emc_vsp;

		clk_parent = devm_clk_get(hw_dev->jpg_dev,
				"clk_emc_vsp_parent");
		if (IS_ERR(clk_parent)) {
			dev_err(hw_dev->jpg_dev,
				"clock[%s]: failed to get parent in probe!\n",
				"clk_emc_vsp_parent");
			ret = PTR_ERR(clk_parent);
		} else
			hw_dev->emc_parent_clk = clk_parent;
	}

	jpg_clk = devm_clk_get(hw_dev->jpg_dev, "jpg_clk");

	if (IS_ERR(jpg_clk)) {
		dev_err(hw_dev->jpg_dev,
			"Failed : Can't get clock [%s}!\n", "jpg_clk");
		dev_err(hw_dev->jpg_dev, "jpg_clk =  %p\n", jpg_clk);
		hw_dev->jpg_clk = NULL;
		ret = PTR_ERR(jpg_clk);
	} else {
		hw_dev->jpg_clk = jpg_clk;
	}

	clk_parent = jpg_get_clk_src_name(hw_dev->clock_name_map, 0,
			hw_dev->max_freq_level);
	hw_dev->jpg_parent_clk_df = clk_parent;

	return ret;
}

#ifdef CONFIG_COMPAT
long compat_jpg_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	long ret = 0;
	struct jpg_fh *jpg_fp = filp->private_data;

	if (!filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	if (jpg_fp == NULL) {
		pr_err("%s, jpg_ioctl error occurred, jpg_fp == NULL\n",
		       __func__);
		return -EINVAL;
	}

	switch (cmd) {
	default:
		return filp->f_op->unlocked_ioctl(filp, cmd, (unsigned long)
						  compat_ptr(arg));
	}

	return ret;
}
#endif

int jpg_get_iova(struct jpg_dev_t *hw_dev,
		 struct jpg_iommu_map_data *mapdata, void __user *arg)
{
	int ret = 0;
	/*struct jpg_iommu_map_data mapdata; */
	struct sprd_iommu_map_data iommu_map_data;
	struct sprd_iommu_unmap_data iommu_ummap_data;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
	struct sg_table *table;
	struct jpg_iommu_map_entry *entry;

	if (sprd_iommu_attach_device(hw_dev->jpg_dev) == 0) {
		dmabuf = dma_buf_get(mapdata->fd);
		if (IS_ERR_OR_NULL(dmabuf)) {
			pr_err("get dmabuf failed\n");
			ret = PTR_ERR(dmabuf);
			goto err_get_dmabuf;
		}
		dma_buf_put(dmabuf);

		attachment = dma_buf_attach(dmabuf, hw_dev->jpg_dev);
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

		iommu_map_data.buf = dmabuf->priv;
		iommu_map_data.iova_size = ((struct ion_buffer *)(dmabuf->priv))->size;
		iommu_map_data.ch_type = SPRD_IOMMU_FM_CH_RW;
		ret = sprd_iommu_map(hw_dev->jpg_dev, &iommu_map_data);
		if (!ret) {
			mutex_lock(&hw_dev->map_lock);
			entry = kzalloc(sizeof(*entry), GFP_KERNEL);
			if (!entry) {
				mutex_unlock(&hw_dev->map_lock);
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
			list_add(&entry->list, &hw_dev->map_list);
			mutex_unlock(&hw_dev->map_lock);

			mapdata->iova_addr = iommu_map_data.iova_addr;
			mapdata->size = iommu_map_data.iova_size;
			pr_debug("jpg iommu map success iova addr=%llu size=%llu\n",
				mapdata->iova_addr, mapdata->size);
			ret =
			    copy_to_user((void __user *)arg,
					 (void *)mapdata,
					 sizeof(struct jpg_iommu_map_data));
			if (ret) {
				pr_err("fatal error! copy_to_user failed, ret=%d\n", ret);
				goto err_copy_to_user;
			}
			pr_debug("suceess to add map_node(iova_addr=%llu, size=%llu)\n",
				mapdata->iova_addr, mapdata->size);
		} else {
			pr_err("jpg iommu map failed, ret=%d, map_size=%zu\n",
				ret, iommu_map_data.iova_size);
			goto err_iommu_map;
		}
	} else {
		ret =
		    copy_to_user((void __user *)arg,
				 (void *)mapdata,
				 sizeof(struct jpg_iommu_map_data));
		if (ret) {
			dev_err(hw_dev->jpg_dev,
				"copy_to_user failed, ret %d\n", ret);
			return -EFAULT;
		}
	}
	return ret;

err_copy_to_user:
	mutex_lock(&hw_dev->map_lock);
	list_del(&entry->list);
	kfree(entry);
	mutex_unlock(&hw_dev->map_lock);
err_kzalloc:
	ret = sprd_iommu_unmap(hw_dev->jpg_dev, &iommu_ummap_data);
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

	return ret;
}

int jpg_free_iova(struct jpg_dev_t *hw_dev,
		  struct jpg_iommu_map_data *ummapdata)
{
	int ret = 0;
	struct jpg_iommu_map_entry *entry = NULL;
	struct sprd_iommu_unmap_data iommu_ummap_data;
	int b_find = 0;

	if (sprd_iommu_attach_device(hw_dev->jpg_dev) == 0) {
		mutex_lock(&hw_dev->map_lock);
		list_for_each_entry(entry, &hw_dev->map_list, list) {
			if (entry->iova_addr == ummapdata->iova_addr) {
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
			pr_debug("success to find node(iova_addr=%llu, size=%llu)\n",
				ummapdata->iova_addr, ummapdata->size);
		} else {
			pr_err("fatal error! not find node(iova_addr=%llu, size=%llu)\n",
				ummapdata->iova_addr, ummapdata->size);
			mutex_unlock(&hw_dev->map_lock);
			return -EFAULT;
		}
		mutex_unlock(&hw_dev->map_lock);

		ret =
		    sprd_iommu_unmap(hw_dev->jpg_dev,
					  &iommu_ummap_data);
		if (ret) {
			pr_err("sprd_iommu_unmap failed: ret=%d, iova_addr=%llu, size=%llu\n",
				ret, ummapdata->iova_addr, ummapdata->size);
			return ret;
		}
		pr_debug("sprd_iommu_unmap success: iova_addr=%llu size=%llu\n",
			ummapdata->iova_addr, ummapdata->size);
		dma_buf_unmap_attachment(entry->attachment, entry->table, DMA_BIDIRECTIONAL);
		dma_buf_detach(entry->dmabuf, entry->attachment);
		kfree(entry);
	}

	return ret;
}

int jpg_poll_mbio_vlc_done(struct jpg_dev_t *hw_dev, int cmd0)
{
	int ret = 0;

	dev_dbg(hw_dev->jpg_dev, "jpg_poll_begin\n");
	if (cmd0 == INTS_MBIO) {
		/* JPG_ACQUAIRE_MBIO_DONE */
		ret = wait_event_interruptible_timeout(
				hw_dev->wait_queue_work_MBIO,
				hw_dev->condition_work_MBIO,
				msecs_to_jiffies(JPG_TIMEOUT_MS));

		if (ret == -ERESTARTSYS) {
			dev_err(hw_dev->jpg_dev,
				"jpg error start -ERESTARTSYS\n");
			ret = -EINVAL;
		} else if (ret == 0) {
			dev_err(hw_dev->jpg_dev,
				"jpg error start  timeout\n");
			ret = readl_relaxed(
				(void __iomem *)(hw_dev->sprd_jpg_virt +
				GLB_INT_STS_OFFSET));
			dev_err(hw_dev->jpg_dev, "jpg_int_status %x", ret);
			ret = -ETIMEDOUT;
		} else {
			ret = 0;
		}

		if (ret) {
			/* timeout, clear jpg int */
			writel_relaxed((1 << 3) | (1 << 2) | (1 << 1) |
				(1 << 0),
				(void __iomem *)(hw_dev->sprd_jpg_virt +
				GLB_INT_CLR_OFFSET));
			ret = 1;
		} else {
			/* poll successful */
			ret = 0;
		}

		hw_dev->jpg_int_status &= (~0x8);
		hw_dev->condition_work_MBIO = 0;
	} else if (cmd0 == INTS_VLC) {
		/* JPG_ACQUAIRE_VLC_DONE */
		ret = wait_event_interruptible_timeout
			    (hw_dev->wait_queue_work_VLC,
			     hw_dev->condition_work_VLC,
			     msecs_to_jiffies(JPG_TIMEOUT_MS));

		if (ret == -ERESTARTSYS) {
			dev_err(hw_dev->jpg_dev,
				"jpg error start -ERESTARTSYS\n");
			ret = -EINVAL;
		} else if (ret == 0) {
			dev_err(hw_dev->jpg_dev,
				"jpg error start  timeout\n");
			ret = readl_relaxed(
				(void __iomem *)(hw_dev->sprd_jpg_virt +
							   GLB_INT_STS_OFFSET));
			dev_err(hw_dev->jpg_dev, "jpg_int_status %x", ret);
			ret = -ETIMEDOUT;
		} else {
			ret = 0;
		}

		if (ret) {
			/* timeout, clear jpg int */
			writel_relaxed((1 << 3) | (1 << 2) | (1 << 1) |
				(1 << 0),
				(void __iomem *)(hw_dev->sprd_jpg_virt +
				GLB_INT_CLR_OFFSET));
			ret = 1;
		} else {
			/* poll successful */
			ret = 4;
		}

		hw_dev->jpg_int_status &= (~0x2);
		hw_dev->condition_work_VLC = 0;
	} else {
		dev_err(hw_dev->jpg_dev,
			"JPG_ACQUAIRE_MBIO_DONE error arg");
		ret = -1;
	}
	dev_dbg(hw_dev->jpg_dev, "jpg_poll_end\n");
	return ret;
}

int jpg_clk_enable(struct jpg_dev_t *hw_dev)
{
	int ret = 0;

	dev_info(hw_dev->jpg_dev, "jpg JPG_ENABLE\n");

	ret = clk_prepare_enable(hw_dev->jpg_domain_eb);
	if (ret) {
		dev_err(hw_dev->jpg_dev,
			"jpg jpg_domain_eb clk_prepare_enable failed!\n");
		return ret;
	}
	dev_dbg(hw_dev->jpg_dev,
		"jpg jpg_domain_eb clk_prepare_enable ok.\n");

	if (hw_dev->version == SHARKL3 &&
			hw_dev->clk_aon_jpg_emc_eb) {
		ret = clk_prepare_enable(hw_dev->clk_aon_jpg_emc_eb);
		if (ret) {
			dev_err(hw_dev->jpg_dev,
				"clk_aon_jpg_emc_eb clk_enable failed!\n");
			goto clk_disable_0;
		}
		dev_dbg(hw_dev->jpg_dev,
			"clk_aon_jpg_emc_eb clk_prepare_enable ok.\n");
	}

	ret = clk_prepare_enable(hw_dev->jpg_dev_eb);
	if (ret) {
		dev_err(hw_dev->jpg_dev,
			"jpg_dev_eb clk_prepare_enable failed!\n");
		goto clk_disable_1;
	}
	dev_dbg(hw_dev->jpg_dev,
		"jpg_dev_eb clk_prepare_enable ok.\n");

	ret = clk_prepare_enable(hw_dev->jpg_ckg_eb);
	if (ret) {
		dev_err(hw_dev->jpg_dev,
			"jpg_ckg_eb prepare_enable failed!\n");
		goto clk_disable_2;
	}
	dev_dbg(hw_dev->jpg_dev,
		"jpg_ckg_eb clk_prepare_enable ok.\n");

	if (hw_dev->version == SHARKL3) {
		ret = clk_set_parent(hw_dev->clk_ahb_vsp,
				   hw_dev->ahb_parent_clk);
		if (ret) {
			dev_err(hw_dev->jpg_dev,
				"clock[%s]: clk_set_parent() failed!",
				"ahb_parent_clk");
			goto clk_disable_3;
		}

		ret = clk_prepare_enable(hw_dev->clk_ahb_vsp);
		if (ret) {
			dev_err(hw_dev->jpg_dev,
				"clk_ahb_vsp: clk_prepare_enable failed!\n");
			goto clk_disable_3;
		}
		dev_info(hw_dev->jpg_dev,
			"clk_ahb_vsp: clk_prepare_enable ok.\n");

		ret = clk_set_parent(hw_dev->clk_emc_vsp,
				   hw_dev->emc_parent_clk);
		if (ret) {
			dev_err(hw_dev->jpg_dev,
				"clock[%s]: clk_set_parent() failed!",
				"emc_parent_clk");
			goto clk_disable_4;
		}

		ret = clk_prepare_enable(hw_dev->clk_emc_vsp);
		if (ret) {
			dev_err(hw_dev->jpg_dev,
				   "clk_emc_vsp: clk_prepare_enable failed!\n");
			goto clk_disable_4;
		}
		dev_dbg(hw_dev->jpg_dev,
				"clk_emc_vsp: clk_prepare_enable ok.\n");
	}

	if (hw_dev->clk_vsp_mq_ahb_eb) {
		ret = clk_prepare_enable(hw_dev->clk_vsp_mq_ahb_eb);
		if (ret) {
			dev_err(hw_dev->jpg_dev,
				"clk_vsp_mq_ahb_eb: clk_prepare_enable failed!");
			goto clk_disable_5;
		}
		dev_dbg(hw_dev->jpg_dev,
			"jpg clk_vsp_mq_ahb_eb clk_prepare_enable ok.\n");
	}

	ret = clk_set_parent(hw_dev->jpg_clk,
		hw_dev->jpg_parent_clk_df);
	if (ret) {
		dev_err(hw_dev->jpg_dev,
				"clock[%s]: clk_set_parent() failed!",
				"clk_jpg");
		goto clk_disable_6;
	}

	ret = clk_set_parent(hw_dev->jpg_clk,
			hw_dev->jpg_parent_clk);
	if (ret) {
		dev_err(hw_dev->jpg_dev,
			"clock[%s]: clk_set_parent() failed!",
			"clk_jpg");
		goto clk_disable_6;
	}

	ret = clk_prepare_enable(hw_dev->jpg_clk);
	if (ret) {
		dev_err(hw_dev->jpg_dev,
			"jpg_clk clk_prepare_enable failed!\n");
		goto clk_disable_6;
	}

	dev_info(hw_dev->jpg_dev,
		"jpg_clk clk_prepare_enable ok.\n");
	return ret;

clk_disable_6:
	if (hw_dev->version == PIKE2)
		clk_disable_unprepare(hw_dev->clk_vsp_mq_ahb_eb);
clk_disable_5:
	if (hw_dev->version == SHARKL3)
		clk_disable_unprepare(hw_dev->clk_emc_vsp);
clk_disable_4:
	if (hw_dev->version == SHARKL3)
		clk_disable_unprepare(hw_dev->clk_ahb_vsp);
clk_disable_3:
	clk_disable_unprepare(hw_dev->jpg_ckg_eb);
clk_disable_2:
	clk_disable_unprepare(hw_dev->jpg_dev_eb);
clk_disable_1:
	if (hw_dev->version == SHARKL3 && hw_dev->clk_aon_jpg_emc_eb)
		clk_disable_unprepare(hw_dev->clk_aon_jpg_emc_eb);
clk_disable_0:
	clk_disable_unprepare(hw_dev->jpg_domain_eb);

	return ret;
}


void jpg_clk_disable(struct jpg_dev_t *hw_dev)
{
	if (hw_dev->version == SHARKL3) {
		if (hw_dev->clk_ahb_vsp)
			clk_disable_unprepare(hw_dev->clk_ahb_vsp);
		if (hw_dev->clk_emc_vsp)
			clk_disable_unprepare(hw_dev->clk_emc_vsp);
	}
	clk_disable_unprepare(hw_dev->jpg_clk);
	if (hw_dev->version == SHARKL3) {
		if (hw_dev->clk_aon_jpg_emc_eb)
			clk_disable_unprepare(hw_dev->clk_aon_jpg_emc_eb);
	}
	if (hw_dev->clk_vsp_mq_ahb_eb)
		clk_disable_unprepare(hw_dev->clk_vsp_mq_ahb_eb);
	clk_disable_unprepare(hw_dev->jpg_ckg_eb);
	clk_disable_unprepare(hw_dev->jpg_dev_eb);
	clk_disable_unprepare(hw_dev->jpg_domain_eb);
}
