// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/string.h>
#include <linux/file.h>
#include <linux/kernel.h>
#include <linux/reboot.h>
#include <linux/kmsg_dump.h>
#include <linux/uio.h>
#include "unisoc_sysdump.h"
#include <linux/time.h>

static const char devicename[] = "/dev/block/by-name/common_rs1_a";

static DEFINE_MUTEX(kmsg_buf_lock);
static char *kmsg_buf;
#define KMSG_BUF_SIZE (128 * 1024)
#define DEFAULT_REASON "Normal"
#define PANIC_REASON "Panic"
static int kmsg_get_flag;
int shutdown_save_log_flag;

#define LAST_KMSG_OFFSET	0
#define LAST_ANDROID_LOG_OFFSET	(1 * 1024 * 1024)

int get_kernel_log_to_buffer(char *buf, size_t buf_size)
{
	struct kmsg_dump_iter iter;
	size_t dump_size;

	if (!buf)
		return -1;

	kmsg_dump_rewind(&iter);

	/* Write dump contents. */
	if (!kmsg_dump_get_buffer(&iter, true, buf, buf_size, &dump_size)) {
		pr_err("get last kernel message failed\n");
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(get_kernel_log_to_buffer);
static void get_last_kmsg(char *buf, size_t buf_size, const char *why)
{
	int header_size = 0;

	if (!buf)
		return;
	if (kmsg_get_flag > 0)
		return;
	kmsg_get_flag = 1;

	header_size = snprintf(buf, buf_size, "%s\n", why);

	header_size = ALIGN(header_size, 8);
	buf_size -= header_size;
	if (get_kernel_log_to_buffer(buf + header_size, buf_size))
		pr_err("save kernel log to buffer failed!\n");
}
static int write_data_to_partition(struct file *filp, char *buf, size_t buf_size, int offset)
{
	struct iov_iter iiter;
	struct kiocb kiocb;
	struct kvec iov;
	int ret;

	iov.iov_base = (void *)buf;
	iov.iov_len = buf_size;

	kiocb.ki_filp = filp;
	kiocb.ki_flags = IOCB_DSYNC;
	kiocb.ki_pos = offset;

	iiter.iter_type = ITER_KVEC | WRITE;
	iiter.kvec = &iov;
	iiter.nr_segs = 1;
	iiter.iov_offset = 0;
	iiter.count = buf_size;

	ret = filp->f_op->write_iter(&kiocb, &iiter);

	return ret;

}
static int save_log_to_partition_handler(struct notifier_block *nb, unsigned long event, void *buf)
{
	int ret;
	static struct file *plog_file;

	if (shutdown_save_log_flag == 1)
		return 0;

	plog_file = filp_open_block(devicename, O_RDWR | O_DSYNC | O_NOATIME | O_EXCL, 0);
	if (IS_ERR(plog_file)) {
		ret = PTR_ERR(plog_file);
		pr_err("failed to open '%s':%d!\n", devicename, ret);
		return -1;
	}

	/* handle last kmsg */
	mutex_lock(&kmsg_buf_lock);
	get_last_kmsg(kmsg_buf, KMSG_BUF_SIZE, buf);
	mutex_unlock(&kmsg_buf_lock);

	ret = write_data_to_partition(plog_file, kmsg_buf, KMSG_BUF_SIZE, LAST_KMSG_OFFSET);
	if (ret)
		pr_err("write kmsg to partition error! '%s':%d!\n", devicename, ret);

	/* handle last android log */
	if (ylog_buffer == NULL) {
		pr_err("ylog_buffer is null, return!\n");
		return -1;
	}
	ret = write_data_to_partition(plog_file, ylog_buffer, YLOG_BUF_SIZE,
			LAST_ANDROID_LOG_OFFSET);
	if (ret)
		pr_err("write ylog to partition error! '%s':%d!\n", devicename, ret);

	fput(plog_file);

	return 0;

}
const char *kmsg_dump_reason(enum kmsg_dump_reason reason)
{
	switch (reason) {
	case KMSG_DUMP_PANIC:
		return "Panic";
	case KMSG_DUMP_OOPS:
		return "Oops";
	case KMSG_DUMP_EMERG:
		return "Emergency";
	case KMSG_DUMP_SHUTDOWN:
		return "Shutdown";
	default:
		return "Unknown";
	}
}
void shutdown_save_log_to_partition(char *time, char *reason)
{
	char save_reason[100] = {0};

	shutdown_save_log_flag = 0;
	if ((time != NULL) && (reason != NULL))
		snprintf(save_reason, 99, "%s-%s-%s\n", "shutdown_error", time, reason);
	else
		snprintf(save_reason, 99, "%s\n", "shutdown_error");

	if (save_log_to_partition_handler(NULL, 0, save_reason)) {
		pr_err("save log to partition failed!\n");
		return;
	}
	shutdown_save_log_flag = 1;
}
EXPORT_SYMBOL(shutdown_save_log_to_partition);
static void last_kmsg_handler(struct kmsg_dumper *dumper,
		enum kmsg_dump_reason reason)
{
	const char *why;

	shutdown_save_log_flag = 0;
	why = kmsg_dump_reason(reason);
	if (kmsg_buf == NULL) {
		pr_err("no available buf, do nothing!\n");
	} else {
		mutex_lock(&kmsg_buf_lock);
		get_last_kmsg(kmsg_buf, KMSG_BUF_SIZE, why);
		mutex_unlock(&kmsg_buf_lock);
	}
}

static struct notifier_block sysdump_log_notifier = {
	.notifier_call  = save_log_to_partition_handler,
};

static struct kmsg_dumper sysdump_dumper = {
	.dump = last_kmsg_handler,
	.max_reason = KMSG_DUMP_MAX,
};

int last_kmsg_init(void)
{
	int ret;
	phys_addr_t lkmsg_paddr;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	struct page **pages;
	void *vaddr;
	unsigned int i;

	kmsg_buf = kzalloc(KMSG_BUF_SIZE, GFP_KERNEL);
	if (kmsg_buf == NULL)
		return -1;

	SetPageReserved(virt_to_page(kmsg_buf));

	pr_info("register sysdump log notifier\n");
	ret = register_reboot_notifier(&sysdump_log_notifier);

	minidump_save_extend_information("last_kmsg", __pa(kmsg_buf),
			__pa(kmsg_buf + KMSG_BUF_SIZE));
	/* map buffer as noncached */
	lkmsg_paddr = __pa(kmsg_buf);
	if (!pfn_valid(lkmsg_paddr >> PAGE_SHIFT)) {
		pr_err("invalid pfn, do nothing\n");
		return -1;
	}
	page_start = lkmsg_paddr - offset_in_page(lkmsg_paddr);
	page_count = DIV_ROUND_UP(KMSG_BUF_SIZE + offset_in_page(lkmsg_paddr), PAGE_SIZE);
	prot = pgprot_writecombine(PAGE_KERNEL);
	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		pr_err("%s: Failed to allocate array for %u pages\n",
				__func__, page_count);
		return -1;
	}
	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;

		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);
	kmsg_buf = vaddr + offset_in_page(lkmsg_paddr);

	kmsg_dump_register(&sysdump_dumper);
	return 0;
}
void last_kmsg_exit(void)
{
	if (kmsg_buf != NULL) {
		ClearPageReserved(virt_to_page(kmsg_buf));
		kfree(kmsg_buf);
	}
	unregister_reboot_notifier(&sysdump_log_notifier);
	kmsg_dump_unregister(&sysdump_dumper);
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("save kernel log to disk");
