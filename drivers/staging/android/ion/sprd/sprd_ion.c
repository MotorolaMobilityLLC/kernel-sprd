/*
 * Copyright (C) 2016 Spreadtrum Communications Inc.
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

#include <asm/cacheflush.h>
#include <linux/compat.h>
#include <linux/dma-buf.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sprd_ion.h>
#include <linux/uaccess.h>
#include "../ion.h"

struct ion_device *idev;
EXPORT_SYMBOL(idev);
static int num_heaps;
static struct ion_heap **heaps;
static u32 phys_offset;

#ifndef CONFIG_64BIT
static unsigned long user_va2pa(struct mm_struct *mm, unsigned long addr)
{
	pgd_t *pgd = pgd_offset(mm, addr);
	unsigned long pa = 0;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *ptep, pte;

	if (pgd_none(*pgd))
		return 0;

	pud = pud_offset(pgd, addr);

	if (pud_none(*pud))
		return 0;

	pmd = pmd_offset(pud, addr);

	if (pmd_none(*pmd))
		return 0;

	ptep = pte_offset_map(pmd, addr);
	pte = *ptep;
	if (pte_present(pte))
		pa = pte_val(pte) & PAGE_MASK;
	pte_unmap(ptep);

	return pa;
}
#endif

static struct ion_buffer *get_ion_buffer(int fd, struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer;

	if (fd < 0 && !dmabuf) {
		pr_err("%s, input fd: %d, dmabuf: %p error\n", __func__, fd,
		       dmabuf);
		return ERR_PTR(-EINVAL);
	}

	if (fd >= 0) {
		dmabuf = dma_buf_get(fd);
		if (IS_ERR_OR_NULL(dmabuf)) {
			pr_err("%s, dmabuf=%p dma_buf_get error!\n", __func__,
			       dmabuf);
			return ERR_PTR(-EBADF);
		}
		buffer = dmabuf->priv;
		dma_buf_put(dmabuf);
	} else {
		buffer = dmabuf->priv;
	}

	return buffer;
}

int sprd_ion_is_reserved(int fd, struct dma_buf *dmabuf, bool *reserved)
{
	struct ion_buffer *buffer;

	buffer = get_ion_buffer(fd, dmabuf);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	/* The range of system reserved memory is in 0x5 xxxxxxxx,*/
	/* so master must use IOMMU to access it*/

	if (buffer->heap->type == ION_HEAP_TYPE_CARVEOUT &&
	    buffer->heap->id != ION_HEAP_ID_SYSTEM)
		*reserved = true;
	else
		*reserved = false;

	return 0;
}

int sprd_ion_get_sg_table(int fd, struct dma_buf *dmabuf,
			  struct sg_table **table, size_t *size)
{
	struct ion_buffer *buffer;

	buffer = get_ion_buffer(fd, dmabuf);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	*table = buffer->sg_table;
	*size = buffer->size;

	return 0;
}

int sprd_ion_get_buffer(int fd, struct dma_buf *dmabuf,
			void **buf, size_t *size)
{
	struct ion_buffer *buffer;

	buffer = get_ion_buffer(fd, dmabuf);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	*buf = (void *)buffer;
	*size = buffer->size;

	return 0;
}

int sprd_ion_get_sg(void *buf, struct sg_table **table)
{
	struct ion_buffer *buffer;

	if (!buf) {
		pr_err("%s, buf==NULL", __func__);
		return -EINVAL;
	}

	buffer = (struct ion_buffer *)buf;
	*table = buffer->sg_table;

	return 0;
}

void sprd_ion_set_dma(void *buf, int id)
{
	struct ion_buffer *buffer = (struct ion_buffer *)buf;

	buffer->iomap_cnt[id]++;
}

void sprd_ion_put_dma(void *buf, int id)
{
	struct ion_buffer *buffer = (struct ion_buffer *)buf;

	buffer->iomap_cnt[id]--;
}

int sprd_ion_get_phys_addr(int fd, struct dma_buf *dmabuf,
			   unsigned long *phys_addr, size_t *size)
{
	int ret = 0;
	struct ion_buffer *buffer;
	struct sg_table *table = NULL;
	struct scatterlist *sgl = NULL;

	buffer = get_ion_buffer(fd, dmabuf);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	if (buffer->heap->type == ION_HEAP_TYPE_CARVEOUT) {
		table = buffer->sg_table;
		if (table && table->sgl) {
			sgl = table->sgl;
		} else {
			if (!table)
				pr_err("invalid table\n");
			else if (!table->sgl)
				pr_err("invalid table->sgl\n");
			return -EINVAL;
		}

		*phys_addr = sg_phys(sgl) - phys_offset;
		*size = buffer->size;
	} else {
		pr_err("%s, buffer heap type:%d error\n", __func__,
		       buffer->heap->type);
		return -EPERM;
	}

	return ret;
}

int sprd_ion_check_phys_addr(struct dma_buf *dmabuf)
{
	struct sg_table *table = NULL;
	struct scatterlist *sgl = NULL;

	if (dmabuf && dmabuf->priv) {
		table = ((struct ion_buffer *)(dmabuf->priv))->priv_virt;
	} else {
		if (!dmabuf)
			pr_err("invalid dmabuf\n");
		else if (!dmabuf->priv)
			pr_err("invalid dmabuf->priv\n");

		return -1;
	}
	if (table && table->sgl) {
		sgl = table->sgl;
	} else {
		if (!table)
			pr_err("invalid table\n");
		else if (!table->sgl)
			pr_err("invalid table->sgl\n");

		return -1;
	}

#ifdef CONFIG_DEBUG_SG
	if (sgl->sg_magic != SG_MAGIC) {
		pr_err("the sg_magic isn't right, 0x%lx!\n", sgl->sg_magic);
		return -1;
	}
#endif

	return 0;
}

static struct ion_platform_heap *sprd_ion_parse_dt(struct platform_device *pdev)
{
	int i = 0, ret = 0;
	const struct device_node *parent = pdev->dev.of_node;
	struct device_node *child = NULL;
	struct ion_platform_heap *ion_heaps = NULL;
	struct platform_device *new_dev = NULL;
	u32 val = 0, type = 0;
	const char *name;
	u32 out_values[4];
	struct device_node *np_memory;

	ret = of_property_read_u32(parent, "phys-offset", &phys_offset);
	if (ret)
		phys_offset = 0;
	else
		pr_info("%s: phys_offset=0x%x\n", __func__, phys_offset);

	for_each_child_of_node(parent, child)
		num_heaps++;
	if (!num_heaps)
		return ERR_PTR(-EINVAL);

	pr_info("%s: num_heaps=%d\n", __func__, num_heaps);

	ion_heaps = kcalloc(num_heaps, sizeof(struct ion_platform_heap),
			    GFP_KERNEL);
	if (!ion_heaps)
		return ERR_PTR(-ENOMEM);

	for_each_child_of_node(parent, child) {
		new_dev = of_platform_device_create(child, NULL, &pdev->dev);
		if (!new_dev) {
			pr_err("Failed to create device %s\n", child->name);
			goto out;
		}

		ion_heaps[i].priv = &new_dev->dev;

		ret = of_property_read_u32(child, "reg", &val);
		if (ret) {
			pr_err("%s: Unable to find reg key, ret=%d", __func__,
			       ret);
			goto out;
		}
		ion_heaps[i].id = val;

		ret = of_property_read_string(child, "label", &name);
		if (ret) {
			pr_err("%s: Unable to find label key, ret=%d", __func__,
			       ret);
			goto out;
		}
		ion_heaps[i].name = name;

		ret = of_property_read_u32(child, "type", &type);
		if (ret) {
			pr_err("%s: Unable to find type key, ret=%d", __func__,
			       ret);
			goto out;
		}
		ion_heaps[i].type = type;

		np_memory = of_parse_phandle(child, "memory-region", 0);

		if (!np_memory) {
			ion_heaps[i].base = 0;
			ion_heaps[i].size = 0;
		} else {
#ifdef CONFIG_64BIT
			ret = of_property_read_u32_array(np_memory, "reg",
							 out_values, 4);
			if (!ret) {
				ion_heaps[i].base = out_values[0];
				ion_heaps[i].base = ion_heaps[i].base << 32;
				ion_heaps[i].base |= out_values[1];

				ion_heaps[i].size = out_values[2];
				ion_heaps[i].size = ion_heaps[i].size << 32;
				ion_heaps[i].size |= out_values[3];
			} else {
				ion_heaps[i].base = 0;
				ion_heaps[i].size = 0;
			}
#else
			ret = of_property_read_u32_array(np_memory, "reg",
							 out_values, 2);
			if (!ret) {
				ion_heaps[i].base = out_values[0];
				ion_heaps[i].size = out_values[1];
			} else {
				ion_heaps[i].base = 0;
				ion_heaps[i].size = 0;
			}
#endif
		}

		pr_info("%s: heaps[%d]: %s type: %d base: 0x%llx size 0x%zx\n",
			__func__, i,
			ion_heaps[i].name,
			ion_heaps[i].type,
			ion_heaps[i].base,
			ion_heaps[i].size);
		++i;
	}
	return ion_heaps;
out:
	kfree(ion_heaps);
	return ERR_PTR(ret);
}

#ifdef CONFIG_E_SHOW_MEM
static int ion_e_show_mem_handler(struct notifier_block *nb,
				  unsigned long val, void *data)
{
	int i;
	enum e_show_mem_type type = (enum e_show_mem_type)val;
	unsigned long total_used = 0;

	pr_info("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	pr_info("Enhanced Mem-info :ION\n");
	for (i = 0; i < num_heaps; i++) {
		if (type != E_SHOW_MEM_BASIC ||
		    heaps[i]->type == ION_HEAP_TYPE_SYSTEM ||
		    heaps[i]->type == ION_HEAP_TYPE_SYSTEM_CONTIG) {
			ion_debug_heap_show_printk(heaps[i], type, &total_used);
		}
	}

	pr_info("Total allocated from Buddy: %lu kB\n", total_used / 1024);
	return 0;
}

static struct notifier_block ion_e_show_mem_notifier = {
	.notifier_call = ion_e_show_mem_handler,
};
#endif

static int sprd_ion_probe(struct platform_device *pdev)
{
	int i = 0, ret = -1;
	struct ion_platform_heap *ion_heaps = NULL;
	u32 need_free_pdata;

	num_heaps = 0;

	if (pdev->dev.of_node) {
		ion_heaps = sprd_ion_parse_dt(pdev);
		if (IS_ERR(ion_heaps))
			return PTR_ERR(ion_heaps);
		need_free_pdata = 1;
	} else {
		ion_heaps = pdev->dev.platform_data;

		if (!ion_heaps) {
			pr_err("%s failed: No platform data!\n", __func__);
			return -ENODEV;
		}

		if (!num_heaps)
			return -EINVAL;
		need_free_pdata = 0;
	}

	heaps = kcalloc(num_heaps, sizeof(struct ion_heap *), GFP_KERNEL);
	if (!heaps) {
		ret = -ENOMEM;
		goto out1;
	}

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &ion_heaps[i];

		if (!pdev->dev.of_node)
			heap_data->priv = &pdev->dev;
		heaps[i] = ion_carveout_heap_create(heap_data);
		if (IS_ERR_OR_NULL(heaps[i])) {
			pr_err("%s,heaps is null, i:%d\n", __func__, i);
			ret = PTR_ERR(heaps[i]);
			goto out;
		}
		ion_device_add_heap(heaps[i]);
	}
#ifdef CONFIG_E_SHOW_MEM
	register_e_show_mem_notifier(&ion_e_show_mem_notifier);
#endif

	if (need_free_pdata)
		kfree(ion_heaps);

	return 0;
out:
	kfree(heaps);
out1:
	if (need_free_pdata)
		kfree(ion_heaps);

	return ret;
}

static int sprd_ion_remove(struct platform_device *pdev)
{
#ifdef CONFIG_E_SHOW_MEM
	unregister_e_show_mem_notifier(&ion_e_show_mem_notifier);
#endif
	kfree(heaps);

	return 0;
}

static const struct of_device_id sprd_ion_ids[] = {
	{ .compatible = "sprd,ion"},
	{},
};

static struct platform_driver ion_driver = {
	.probe = sprd_ion_probe,
	.remove = sprd_ion_remove,
	.driver = {
		.name = "ion",
		.of_match_table = of_match_ptr(sprd_ion_ids),
	}
};

static int __init sprd_ion_init(void)
{
	int result = 0;

	result = platform_driver_register(&ion_driver);
	pr_info("%s,result:%d\n", __func__, result);
	return result;
}

static void __exit sprd_ion_exit(void)
{
	platform_driver_unregister(&ion_driver);
}

device_initcall(sprd_ion_init);
