#include <linux/dma-mapping.h>
#include <linux/swiotlb.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/sysctl.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <asm/memory.h>
#include <linux/hugetlb.h>
#include <asm/pgtable.h>
#include <linux/syscore_ops.h>
#include <linux/device.h>
#include "sprd_past_record.h"

int serror_debug_status;
EXPORT_SYMBOL(serror_debug_status);

struct sprd_debug_reg_record *sprd_past_reg_record;
EXPORT_SYMBOL(sprd_past_reg_record);

#ifdef CONFIG_ENABLE_SPRD_DEEP_SLEEP_TRACING
#define SPRD_DS_DDR_TEST_ARRAY_LEN 1024
#define SPRD_DS_DDR_TEST_ERR_PRINT_LEN 8
#define SPRD_DS_DDR_TEST_FEATURE_CHAR 0xf0
u8 *sprd_ds_ddr_test_array;
EXPORT_SYMBOL(sprd_ds_ddr_test_array);
struct deep_sleep_info *sprd_deep_sleep_info;
EXPORT_SYMBOL(sprd_deep_sleep_info);
static struct device *sprd_past_record_dev;
static dma_addr_t sprd_ds_tracing_dma_addr;
#endif

unsigned long sprd_debug_virt_to_phys(void __iomem *addr)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	unsigned long phys_addr;
	unsigned long page_addr;
	unsigned long page_offset;
	unsigned long virt_addr;

	virt_addr = (unsigned long) addr;

	pgd = pgd_offset_k(virt_addr);
	if (pgd_none(*pgd))
		return 0;

#ifdef CONFIG_ARM64
	pud = pud_offset(pgd, virt_addr);
	if (pud_none(*pud))
		return 0;
	pmd = pmd_offset(pud, virt_addr);
	if (pmd_none(*pmd))
		return 0;
	if (pmd_val(*pmd) && !(pmd_val(*pmd) & (_AT(pmdval_t, 1) << 1))) {
		page_addr = page_to_phys(pmd_page(*pmd));
		page_offset = virt_addr & ~PAGE_MASK;
		phys_addr = (0x7fffffffffUL) & (page_addr | page_offset);
		return phys_addr;
	}
#else
	pud = (pud_t *)pgd;
	pmd = (pmd_t *)pud;
	if (pmd_none(*pmd))
		return 0;
	if (pmd_large(*pmd)) {
		phys_addr = (pmd_val(*pmd) & PMD_MASK) + (virt_addr & ~PMD_MASK);
		return phys_addr;
	}
#endif

	pte = pte_offset_kernel(pmd, virt_addr);
	if (pte_none(*pte))
		return 0;

	page_addr = pte_val(*pte) & PAGE_MASK;
	page_offset = virt_addr & ~PAGE_MASK;
	phys_addr = (0x7fffffffffUL) & (page_addr | page_offset);

	return phys_addr;
}
EXPORT_SYMBOL(sprd_debug_virt_to_phys);

static int sprd_debug_past_record_alloc(void)
{
	/*sprd_past_reg_record*/
	sprd_past_reg_record = vmalloc(sizeof(struct sprd_debug_reg_record));
	if (unlikely(sprd_past_reg_record == NULL))
		return -ENOMEM;
	pr_info("***serror debug %s, sprd_past_reg_record:%p ***\n",
		__func__, sprd_past_reg_record);

	pr_info("***serror debug %s, alloc done!\n", __func__);
	return 0;
}

static int sprd_debug_past_record_free(void)
{
	/*sprd_past_reg_record*/
	if (sprd_past_reg_record)
		vfree(sprd_past_reg_record);
	sprd_past_reg_record = NULL;
	pr_info("***serror debug %s, sprd_past_reg_record:%p ***\n",
		__func__, sprd_past_reg_record);

	pr_info("***serror debug %s, free done!\n", __func__);
	return 0;
}

static int sprd_serror_debug_read(struct seq_file *s, void *v)
{
	seq_printf(s, "%d\n", serror_debug_status);
	return 0;
}

static int sprd_serror_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_serror_debug_read, NULL);
}

static ssize_t sprd_serror_debug_write(struct file *file, const char __user *buf,
	size_t count, loff_t *data)
{
	char serror_buf[SERROR_PROC_BUF_LEN] = {0};

	pr_info("***serror debug %s: start!!!\n", __func__);
	pr_info("SERROR_START_ADDR = 0x%lx\n", SERROR_START_ADDR);
	pr_info("SERROR_END_ADDR = 0x%lx\n", SERROR_END_ADDR);

	if (count && (count < SERROR_PROC_BUF_LEN)) {
		if (copy_from_user(serror_buf, buf, count)) {
			pr_err("***serror debug %s: copy_from_user failed!!!\n", __func__);
			return -1;
		}
		serror_buf[count] = '\0';

		if (!strncmp(serror_buf, "on", 2)) {
			pr_info("***serror debug %s: enable serror debug!!!\n", __func__);
			sprd_debug_past_record_alloc();
			serror_debug_status = 1;
		} else if (!strncmp(serror_buf, "off", 3)) {
			pr_info("***serror debug %s: disable serror debug!!!\n", __func__);
			serror_debug_status = 0;
		} else if (!strncmp(serror_buf, "null", 4)) {
			pr_info("***serror debug %s  null pointer !!\n", __func__);
			BUG_ON(1);
		}
	}

	pr_info("***serror debug%s: End!!!\n", __func__);
	return count;
}

static const struct file_operations serror_proc_fops = {
	.owner = THIS_MODULE,
	.open = sprd_serror_debug_open,
	.read = seq_read,
	.write = sprd_serror_debug_write,
	.llseek = seq_lseek,
	.release = single_release,
};

#ifdef CONFIG_ENABLE_SPRD_DEEP_SLEEP_TRACING
static void sprd_deep_sleep_ddr_test(void)
{
	u16 count = SPRD_DS_DDR_TEST_ARRAY_LEN;
	u16 err_count = SPRD_DS_DDR_TEST_ERR_PRINT_LEN;
	u8 *pos;
	u8 standard = SPRD_DS_DDR_TEST_FEATURE_CHAR;

	if (sprd_ds_ddr_test_array) {
		do {
			pos = sprd_ds_ddr_test_array + SPRD_DS_DDR_TEST_ARRAY_LEN - count;
			if (*pos != SPRD_DS_DDR_TEST_FEATURE_CHAR) {
				pr_info("%s: ddr test region error at pos 0x%p\n", __func__, pos);
				pr_info("%s: 0x%x is expected\n", __func__, standard);
				if (err_count > count)
					err_count = count;
				pr_info("real value:\n");
				while (err_count--)
					pr_info("0x%x\n", *(pos++));
				BUG_ON(1);
			}
		} while (--count > 0);
	}
}

static void sprd_past_record_resume(void)
{
	sprd_deep_sleep_ddr_test();
}

static int sprd_past_record_suspend(void)
{
	sprd_deep_sleep_ddr_test();

	return 0;
}

static struct syscore_ops sprd_past_record_syscore_ops = {
	.resume	= sprd_past_record_resume,
	.suspend = sprd_past_record_suspend,
};

static u64 sprd_past_record_dev_dma_mask = DMA_BIT_MASK(32);

static int sprd_debug_deep_sleep_tracing_init(void)
{
	pr_info("%s in\n", __func__);

	sprd_ds_ddr_test_array = kmalloc(sizeof(u8) * SPRD_DS_DDR_TEST_ARRAY_LEN, GFP_KERNEL);
	memset(sprd_ds_ddr_test_array, SPRD_DS_DDR_TEST_FEATURE_CHAR, SPRD_DS_DDR_TEST_ARRAY_LEN);

	register_syscore_ops(&sprd_past_record_syscore_ops);

	sprd_past_record_dev = kzalloc(sizeof(struct device), GFP_KERNEL);
	if (!sprd_past_record_dev)
		return -ENOMEM;
	sprd_past_record_dev->coherent_dma_mask = DMA_BIT_MASK(32);
	sprd_past_record_dev->dma_mask = &sprd_past_record_dev_dma_mask;

	sprd_deep_sleep_info = dma_alloc_coherent(sprd_past_record_dev,
								sizeof(struct deep_sleep_info),
								&sprd_ds_tracing_dma_addr,
								GFP_KERNEL);
	if (!sprd_deep_sleep_info)
		return -ENOMEM;
	sprd_deep_sleep_info->phase = SPRD_DEEP_STATE_NOT_IN_DEEP;
	pr_info("%s dma addr = 0x%llx\n", __func__, sprd_ds_tracing_dma_addr);

	pr_info("%s done\n", __func__);
	return 0;
}

static void sprd_debug_deep_sleep_tracing_free(void)
{
	if (sprd_deep_sleep_info)
		dma_free_coherent(sprd_past_record_dev,
			sizeof(struct deep_sleep_info),
			sprd_deep_sleep_info,
			sprd_ds_tracing_dma_addr);

	if (sprd_past_record_dev)
		kfree(sprd_past_record_dev);

	pr_info("%s done\n", __func__);
}
#endif

static __init int past_record_sysctl_init(void)
{
	struct proc_dir_entry *serror_proc;

	serror_proc = proc_create("sprd_serror_debug", 0660, NULL, &serror_proc_fops);
	if (!serror_proc)
		return -ENOMEM;
#ifdef CONFIG_ENABLE_SPRD_IO_TRACING
	serror_debug_status = 1;
	sprd_debug_past_record_alloc();
#else
	serror_debug_status = 0;
#endif
#ifdef CONFIG_ENABLE_SPRD_DEEP_SLEEP_TRACING
	sprd_debug_deep_sleep_tracing_init();
#endif

	pr_info("***past record debug init success!\n");
	return 0;
}

static __exit void past_record_sysctl_exit(void)
{
	remove_proc_entry("sprd_serror_debug", NULL);
	sprd_debug_past_record_free();
#ifdef CONFIG_ENABLE_SPRD_DEEP_SLEEP_TRACING
	sprd_debug_deep_sleep_tracing_free();
#endif
	pr_info("***past record debug exit\n");
}

module_init(past_record_sysctl_init);
module_exit(past_record_sysctl_exit);

MODULE_AUTHOR("xiaoguang.li <xiaoguang.li@unisoc.com>");
MODULE_DESCRIPTION("unisoc past record debug ");
MODULE_LICENSE("GPL");
