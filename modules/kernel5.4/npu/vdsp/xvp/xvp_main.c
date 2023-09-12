/**
 * Copyright (C) 2019-2022 UNISOC Technologies Co.,Ltd.
 */

/*
 * XRP: Linux device driver for Xtensa Remote Processing
 *
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Alternatively you can use and distribute this file under the terms of
 * the GNU General Public License version 2 or later.
 */

/*
 * This file has been modified by UNISOC to expanding communication module to
 * realize vdsp device communication and add memory,dvfs, faceid contol
 */

#include <linux/version.h>
#include <linux/atomic.h>
#include <linux/acpi.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0)
#include <linux/dma-mapping.h>
#else
#include <linux/dma-direct.h>
#endif
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/hashtable.h>
#include <linux/highmem.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/property.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/soc/sprd/sprd_systimer.h>
#include <linux/timer.h>
#include <asm/mman.h>
#include <asm/uaccess.h>

#include "sprd_vdsp_iommus.h"
#include "sprd_vdsp_mem_core.h"
#include "sprd_vdsp_mem_xvp_init.h"
#include "sprd_vdsp_mem_xvpfile.h"
#include "sprd_iommu_test_debug.h"
#include "sprd_vdsp_mem_test_debug.h"
#include "vdsp_hw.h"
#include "vdsp_log.h"
#include "vdsp_dvfs.h"
#include "vdsp_dump.h"
#include "vdsp_debugfs.h"
#ifdef MYN6P
#include "vdsp_mailbox_drv.h"
#endif
#ifdef MYL5
#include "vdsp_ipi_drv.h"
#endif
#include "xrp_firmware.h"
#include "xrp_internal.h"
#include "xrp_kernel_defs.h"
#include "xrp_kernel_dsp_interface.h"
#ifdef FACEID_VDSP_FULL_TEE
#include "xrp_faceid.h"
#endif
#include "xvp_main.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: xvp_main %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define XRP_DEFAULT_TIMEOUT 	200
static struct mutex xvp_global_lock;
static unsigned long firmware_command_timeout = XRP_DEFAULT_TIMEOUT;
static unsigned long firmware_boot_timeout = 25;

module_param(firmware_command_timeout, ulong, 0644);
MODULE_PARM_DESC(firmware_command_timeout, "Firmware command timeout in seconds.");
static int firmware_reboot = 1;
module_param(firmware_reboot, int, 0644);
MODULE_PARM_DESC(firmware_reboot, "Reboot firmware on command timeout.");
static DEFINE_IDA(xvp_nodeid);

static long xrp_ioctl_submit_sync(struct file *filp, struct xrp_ioctl_queue __user * p,
	struct xrp_request *pkernel);

struct iova_reserve iova_reserve_data[1] = {
	[0] = {
		.name = "fw_buf",
		.fixed.offset = 0,
		.size = 6 * 1024 * 1024,
	},
};

/**************** know file start *****************/
static void init_files_know_info(struct xvp *xvp)
{
	int i;

	for (i = 0; i < (1 << 10); i++)
		xvp->xrp_known_files[i].first = NULL;
	mutex_init(&xvp->xrp_known_files_lock);
}

static void xrp_add_known_file(struct file *filp)
{
	struct xrp_known_file *p = kmalloc(sizeof(*p), GFP_KERNEL);
	struct xvp *xvp = ((struct xvp_file *)(filp->private_data))->xvp;

	if (unlikely(!p))
		return;

	p->filp = filp;
	mutex_lock(&xvp->xrp_known_files_lock);
	hash_add(xvp->xrp_known_files, &p->node, (unsigned long)filp);
	mutex_unlock(&xvp->xrp_known_files_lock);
}

static void xrp_remove_known_file(struct file *filp)
{
	struct xrp_known_file *p;
	struct xrp_known_file *pf = NULL;
	struct xvp *xvp = ((struct xvp_file *)(filp->private_data))->xvp;

	mutex_lock(&xvp->xrp_known_files_lock);
	hash_for_each_possible(xvp->xrp_known_files, p, node, (unsigned long)filp) {
		if (p->filp == filp) {
			hash_del(&p->node);
			pf = p;
			break;
		}
	}
	mutex_unlock(&xvp->xrp_known_files_lock);
	if (pf)
		kfree(pf);
}

/**************** know file end *****************/
static int compare_queue_priority(const void *a, const void *b)
{
	const void *const *ppa = a;
	const void *const *ppb = b;

	const struct xrp_comm *pa = *ppa, *pb = *ppb;

	if (pa->priority == pb->priority)
		return 0;
	else
		return pa->priority < pb->priority ? -1 : 1;
}

static inline void xrp_comm_write32(volatile void __iomem *addr, u32 v)
{
	__raw_writel(v, addr);
}

static inline u32 xrp_comm_read32(volatile void __iomem *addr)
{
	return __raw_readl(addr);
}

static inline void __iomem *xrp_comm_put_tlv(void __iomem **addr,
        uint32_t type, uint32_t length)
{
	struct xrp_dsp_tlv __iomem *tlv = *addr;
	xrp_comm_write32(&tlv->type, type);
	xrp_comm_write32(&tlv->length, length);
	*addr = tlv->value + ((length + 3) / 4);
	return tlv->value;
}

static inline void __iomem *xrp_comm_get_tlv(void __iomem **addr,
        uint32_t *type, uint32_t *length)
{
	struct xrp_dsp_tlv __iomem *tlv = *addr;
	*type = xrp_comm_read32(&tlv->type);
	*length = xrp_comm_read32(&tlv->length);
	*addr = tlv->value + ((*length + 3) / 4);
	return tlv->value;
}

static inline void xrp_comm_write(volatile void __iomem *addr, const void *p, size_t sz)
{
	size_t sz32 = sz & ~3;
	u32 v = 0;

	while (sz32) {
		memcpy(&v, p, sizeof(v));
		__raw_writel(v, addr);
		p += 4;
		addr += 4;
		sz32 -= 4;
	}

	sz &= 3;

	if (sz) {
		v = 0;
		memcpy(&v, p, sz);
		__raw_writel(v, addr);
	}
}

static inline void xrp_comm_read(volatile void __iomem *addr, void *p, size_t sz)
{
	size_t sz32 = sz & ~3;
	u32 v = 0;

	while (sz32) {
		v = __raw_readl(addr);
		memcpy(p, &v, sizeof(v));
		p += 4;
		addr += 4;
		sz32 -= 4;
	}

	sz &= 3;

	if (sz) {
		v = __raw_readl(addr);
		memcpy(p, &v, sz);
	}
}

static inline void xrp_send_device_irq(struct xvp *xvp)
{
	if (likely(xvp->hw_ops->send_irq))
		xvp->hw_ops->send_irq(xvp->hw_arg);
}

static inline bool xrp_panic_check(struct xvp *xvp)
{
	if (unlikely(xvp->hw_ops->panic_check))
		return xvp->hw_ops->panic_check(xvp->hw_arg);
	else
		return false;
}

static void xrp_sync_v2(struct xvp *xvp, void *hw_sync_data, size_t sz)
{
	unsigned i;
	struct xrp_dsp_sync_v2 __iomem *queue_sync;
	struct xrp_dsp_sync_v2 __iomem *shared_sync = xvp_buf_get_vaddr(xvp->ipc_buf);
	void __iomem *addr = shared_sync->hw_sync_data;
	unsigned int log_para[2];	//mode+level

	xrp_comm_write(xrp_comm_put_tlv(&addr, XRP_DSP_SYNC_TYPE_HW_SPEC_DATA, sz), hw_sync_data, sz);
	if (xvp->n_queues > 1) {
		xrp_comm_write(xrp_comm_put_tlv(&addr, XRP_DSP_SYNC_TYPE_HW_QUEUES, xvp->n_queues * sizeof(u32)),
			xvp->queue_priority, xvp->n_queues * sizeof(u32));

		for (i = 1; i < xvp->n_queues; ++i) {
			queue_sync = xvp->queue[i].comm;
			xrp_comm_write32(&queue_sync->sync, XRP_DSP_SYNC_IDLE);
		}
	}
	/*debugfs log */
	log_para[0] = vdsp_debugfs_log_mode();
	log_para[1] = vdsp_debugfs_log_level();

	xrp_comm_write(xrp_comm_put_tlv(&addr, XRP_DSP_SYNC_TYPE_LOG, 2 * sizeof(u32)), log_para, 2 * sizeof(u32));
	xrp_comm_put_tlv(&addr, XRP_DSP_SYNC_TYPE_LAST, 0);
}

static int xrp_sync_complete_v2(struct xvp *xvp, size_t sz)
{
	struct xrp_dsp_sync_v2 __iomem *shared_sync = xvp_buf_get_vaddr(xvp->ipc_buf);
	void __iomem *addr = shared_sync->hw_sync_data;
	u32 type, len;

	xrp_comm_get_tlv(&addr, &type, &len);
	if (len != sz) {
		pr_err("HW spec data size modified by the DSP\n");
		return -EINVAL;
	}
	if (!(type & XRP_DSP_SYNC_TYPE_ACCEPT))
		pr_err("HW spec data not recognized by the DSP\n");

	if (xvp->n_queues > 1) {
		void __iomem *p = xrp_comm_get_tlv(&addr, &type, &len);

		if (len != xvp->n_queues * sizeof(u32)) {
			pr_err("Queue priority size modified by the DSP\n");
			return -EINVAL;
		}
		if (type & XRP_DSP_SYNC_TYPE_ACCEPT) {
			xrp_comm_read(p, xvp->queue_priority, xvp->n_queues * sizeof(u32));
		} else {
			pr_err("Queue priority data not recognized by the DSP\n");
			xvp->n_queues = 1;
		}
	}
	return 0;
}

static int xrp_synchronize(struct xvp *xvp)
{
	size_t sz = 0;
	void *hw_sync_data;
	unsigned long deadline = jiffies + (unsigned long)(firmware_boot_timeout * (unsigned long)HZ);
	struct xrp_dsp_sync_v1 __iomem *shared_sync = xvp_buf_get_vaddr(xvp->ipc_buf);
	int ret;
	u32 v, v1;

	/*
	 * TODO
	 * BAD METHOD
	 * Just Using sz temp for transfer share memory address
	 */
	hw_sync_data = xvp->hw_ops->get_hw_sync_data(xvp->hw_arg, &sz, xvp_buf_get_iova(xvp->log_buf));
	if (unlikely(!hw_sync_data)) {
		ret = -ENOMEM;
		goto err;
	}
	ret = -ENODEV;
	xrp_comm_write32(&shared_sync->sync, XRP_DSP_SYNC_START);
	pr_debug("start sync:%d\n", XRP_DSP_SYNC_START);
	mb();
	do {
		v = xrp_comm_read32(&shared_sync->sync);
		if (v != XRP_DSP_SYNC_START)
			break;
		if (xrp_panic_check(xvp))
			goto err;
		schedule();
	} while (time_before(jiffies, deadline));
	pr_debug("sync:%d\n", v);

	switch (v) {
		case XRP_DSP_SYNC_DSP_READY_V1:
			if (xvp->n_queues > 1) {
				pr_err("[ERROR]Queue priority data not recognized\n");
				xvp->n_queues = 1;
			}

			xrp_comm_write(&shared_sync->hw_sync_data, hw_sync_data, sz);
			break;

		case XRP_DSP_SYNC_DSP_READY_V2:
			xrp_sync_v2(xvp, hw_sync_data, sz);
			break;

		case XRP_DSP_SYNC_START:
			pr_err("[ERROR]DSP is not ready for synchronization\n");
			goto err;

		default:
			pr_err("[ERROR]DSP response to XRP_DSP_SYNC_START is not recognized\n");
			goto err;
	}

	mb();
	xrp_comm_write32(&shared_sync->sync, XRP_DSP_SYNC_HOST_TO_DSP);

	do {
		mb();
		v1 = xrp_comm_read32(&shared_sync->sync);
		if (v1 == XRP_DSP_SYNC_DSP_TO_HOST)
			break;
		if (xrp_panic_check(xvp))
			goto err;
		schedule();
	} while (time_before(jiffies, deadline));
	if (v1 != XRP_DSP_SYNC_DSP_TO_HOST) {
		pr_err("[ERROR]DSP haven't confirmed initialization data reception\n");
		goto err;
	}

	if (v == XRP_DSP_SYNC_DSP_READY_V2) {
		ret = xrp_sync_complete_v2(xvp, sz);
		if (ret < 0)
			goto err;
	}

	xrp_send_device_irq(xvp);
	pr_debug("completev2 end, send irq-32k timer[%lld]\n", sprd_sysfrt_read());

	if (xvp->host_irq_mode) {
		int res = wait_for_completion_timeout(&xvp->queue[0].completion,
			(unsigned long)(firmware_boot_timeout * (unsigned long)HZ));

		ret = -ENODEV;
		if (xrp_panic_check(xvp))
			goto err;
		if (res == 0) {
			pr_err("wait dsp send irq to host timeout\n");
			goto err;
		}
	}
	ret = 0;
err:
	if (hw_sync_data)
		kfree(hw_sync_data);
	xrp_comm_write32(&shared_sync->sync, XRP_DSP_SYNC_IDLE);
	pr_debug("sync end ret:%d\n", ret);

	return ret;
}

static bool xrp_cmd_complete(struct xrp_comm *xvp)
{
	struct xrp_dsp_cmd __iomem *cmd = xvp->comm;
	u32 flags = xrp_comm_read32(&cmd->flags);

	rmb();
	return (flags & (XRP_DSP_CMD_FLAG_REQUEST_VALID | XRP_DSP_CMD_FLAG_RESPONSE_VALID)) ==
	       (XRP_DSP_CMD_FLAG_REQUEST_VALID | XRP_DSP_CMD_FLAG_RESPONSE_VALID);
}

#ifdef MYN6
irqreturn_t xrp_irq_handler(void *msg, struct xvp * xvp)
{
	unsigned i, n = 0;

	if (unlikely(!xvp_buf_get_vaddr(xvp->ipc_buf)))
		return IRQ_NONE;

	for (i = 0; i < xvp->n_queues; ++i) {
		if (xrp_cmd_complete(xvp->queue + i)) {
			complete(&xvp->queue[i].completion);
			++n;
		}
	}
	return n ? IRQ_HANDLED : IRQ_NONE;
}
#endif
#ifdef MYL5
irqreturn_t xrp_irq_handler(int irq, struct xvp * xvp)
{
	unsigned i, n = 0;

	if (unlikely(!xvp->ipc_buf))
		return IRQ_NONE;

	for (i = 0; i < xvp->n_queues; ++i) {
		if (xrp_cmd_complete(xvp->queue + i)) {
			complete(&xvp->queue[i].completion);
			++n;
		}
	}
	return n ? IRQ_HANDLED : IRQ_NONE;
}
#endif
EXPORT_SYMBOL(xrp_irq_handler);

#ifdef MYL5
static irqreturn_t xrp_hw_irq_handler_ex(int irq, void *data)
{
	struct xvp *xvp = data;

	return xrp_irq_handler(irq, xvp);
}
#endif

static long xvp_complete_cmd_irq(struct xvp *xvp, struct xrp_comm *comm,
	bool(*cmd_complete) (struct xrp_comm * p))
{
	long timeout = (long)((long)firmware_command_timeout * (long)HZ);

	if (cmd_complete(comm))
		return 0;
	if (xrp_panic_check(xvp)) {
		pr_err("[error]xrp panic\n");
		return -EBUSY;
	}
	do {
		timeout = wait_for_completion_timeout(&comm->completion, timeout);

		pr_debug("wait_for_completion_timeout %ld\n", timeout);
		if (cmd_complete(comm))
			return 0;
		if (xrp_panic_check(xvp)) {
			pr_err("[error]xrp panic\n");
			return -EBUSY;
		}
	} while (timeout > 0);

	if (timeout == 0) {
		pr_err("[error]vdsp timeout\n");
		return -EBUSY;
	}

	pr_debug("xvp_complete_cmd_irq %ld\n", timeout);
	return timeout;
}

static long xvp_complete_cmd_poll(struct xvp *xvp, struct xrp_comm *comm,
	bool(*cmd_complete) (struct xrp_comm * p))
{
	unsigned long deadline = jiffies + (unsigned long)(firmware_command_timeout * (unsigned long) HZ);

	do {
		if (cmd_complete(comm))
			return 0;
		if (xrp_panic_check(xvp))
			return -EBUSY;
		schedule();
	} while (time_before(jiffies, deadline));

	return -EBUSY;
}

static inline int xvp_enable_dsp(struct xvp *xvp)
{
#ifdef FACEID_VDSP_FULL_TEE
	if (xvp->secmode) {
		return sprd_faceid_enable_vdsp(xvp);
	} else
#endif
	{
		if (xvp->hw_ops->enable)
			return xvp->hw_ops->enable(xvp->hw_arg);
		else
			return 0;
	}
}

static inline void xvp_disable_dsp(struct xvp *xvp)
{
#ifdef FACEID_VDSP_FULL_TEE
	if (xvp->secmode) {
		sprd_faceid_disable_vdsp(xvp);
	} else
#endif
	{
		if (xvp->hw_ops->disable)
			xvp->hw_ops->disable(xvp->hw_arg);
	}
}

static inline void xrp_reset_dsp(struct xvp *xvp)
{
#ifdef FACEID_VDSP_FULL_TEE
	if (xvp->secmode) {
		sprd_faceid_reset_vdsp(xvp);
	} else
#endif
	{
		if (xvp->hw_ops->reset)
			xvp->hw_ops->reset(xvp->hw_arg);
	}
}

static inline void xrp_halt_dsp(struct xvp *xvp)
{
#ifdef FACEID_VDSP_FULL_TEE
	if (xvp->secmode) {
		sprd_faceid_halt_vdsp(xvp);
	} else
#endif
	{
		if (xvp->hw_ops->halt)
			xvp->hw_ops->halt(xvp->hw_arg);
	}
}

#ifdef MYN6
static inline void xrp_stop_dsp(struct xvp *xvp)
{
	if (xvp->secmode) {
	} else {
		if (xvp->hw_ops->stop_vdsp)
			xvp->hw_ops->stop_vdsp(xvp->hw_arg);
	}
}
#endif

static inline void xrp_release_dsp(struct xvp *xvp)
{
#ifdef FACEID_VDSP_FULL_TEE
	if (xvp->secmode) {
		sprd_faceid_release_vdsp(xvp);
	} else
#endif
	{
		if (xvp->hw_ops->release)
			xvp->hw_ops->release(xvp->hw_arg);
	}
}
static inline void xvp_set_qos(struct xvp *xvp)
{
	if (xvp->hw_ops->set_qos)
		xvp->hw_ops->set_qos(xvp->hw_arg);
}

#ifdef MYN6
static inline int xvp_init_communicaiton(struct xvp *xvp)
{
	if (xvp->hw_ops->init_communication_hw)
		return xvp->hw_ops->init_communication_hw(xvp->hw_arg);
	pr_err("invalid parameters\n");
	return -1;
}

static inline int xvp_deinit_communicaiton(struct xvp *xvp)
{
	if (xvp->hw_ops->deinit_communication_hw)
		return xvp->hw_ops->deinit_communication_hw(xvp->hw_arg);
	pr_err("invalid parameters\n");
	return -1;
}
#endif

/**************** dvfs related function ****************/
static inline int vdsp_dvfs_init(struct xvp *xvp)
{
	struct vdsp_dvfs_desc *dvfs = get_vdsp_dvfs_desc();

	if (dvfs->ops->init)
		return dvfs->ops->init(xvp);
	return -1;
}

static inline void vdsp_dvfs_deinit(struct xvp *xvp)
{
	struct vdsp_dvfs_desc *dvfs = get_vdsp_dvfs_desc();

	if (dvfs->ops->deinit)
		dvfs->ops->deinit(xvp);
}

static inline void vdsp_dvfs_enable(struct xvp *xvp)
{
	struct vdsp_dvfs_desc *dvfs = get_vdsp_dvfs_desc();

	if (dvfs->ops->enable)
		dvfs->ops->enable(xvp->hw_arg);
}

static inline void vdsp_dvfs_disable(struct xvp *xvp)
{
	struct vdsp_dvfs_desc *dvfs = get_vdsp_dvfs_desc();

	if (dvfs->ops->disable)
		dvfs->ops->disable(xvp->hw_arg);
}

static inline void vdsp_dvfs_set(struct xvp *xvp, uint32_t level)
{
	struct vdsp_dvfs_desc *dvfs = get_vdsp_dvfs_desc();

	if (dvfs->ops->setdvfs)
		dvfs->ops->setdvfs(xvp->hw_arg, level);
}
static inline void vdsp_dvfs_release_powerhint(void *filp)
{
	struct vdsp_dvfs_desc *dvfs = get_vdsp_dvfs_desc();

	if (dvfs->ops->release_powerhint)
		dvfs->ops->release_powerhint(filp);
}

static inline int32_t vdsp_dvfs_set_powerhint(void *data, int32_t level, uint32_t flag)
{
	struct vdsp_dvfs_desc *dvfs = get_vdsp_dvfs_desc();

	if (dvfs->ops->set_powerhint)
		return dvfs->ops->set_powerhint(data, level, flag);
	return -1;
}

static inline void vdsp_dvfs_preprocess(struct xvp *xvp)
{
	struct vdsp_dvfs_desc *dvfs = get_vdsp_dvfs_desc();

	if (dvfs->ops->preprocess)
		dvfs->ops->preprocess(xvp);
}

static inline void vdsp_dvfs_postprocess(struct xvp *xvp)
{
	struct vdsp_dvfs_desc *dvfs = get_vdsp_dvfs_desc();

	if (dvfs->ops->postprocess)
		dvfs->ops->postprocess(xvp);
}

static int xvp_file_release_list(struct file *filp)
{
	uint32_t find = 0;
	int32_t result = 0;
	unsigned long bkt;
	long ret;
	struct loadlib_info *libinfo, *libinfo1, *tmp, *tmp1;
	struct xvp_file *tmp_xvpfile, *xvp_file;
	struct xrp_known_file *p;
	struct xvp *xvp;
	struct xrp_unload_cmdinfo unloadinfo;
	char libname[XRP_NAMESPACE_ID_SIZE];

	xvp_file = (struct xvp_file *)filp->private_data;
	xvp = xvp_file->xvp;
	libinfo = libinfo1 = tmp = tmp1 = NULL;

	pr_debug("xvp file release list\n");

	mutex_lock(&(xvp->load_lib.libload_mutex));

	/*
	 * list_for_each_entry_safe(pos, n, head, member)
	 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
	 * @pos :  the type * to use as a loop cursor
	 * @n:		another tye * to  use as temporary storage
	 * @head: the head for your list
	 * @member: the name of the list_head within the struct.
	 */

	/*release lib load list */
	list_for_each_entry_safe(libinfo, tmp, &xvp_file->load_lib_list, node_libinfo) {
		find = 0;
		/*check whether other xvp_file in system has loaded this lib */
		mutex_lock(&xvp->xrp_known_files_lock);
		hash_for_each(xvp->xrp_known_files, bkt, p, node) {
			if (((struct file *)(p->filp))->private_data != xvp_file) {
				tmp_xvpfile = (struct xvp_file *)(((struct file *)(p->filp))->private_data);
				find = 0;
				list_for_each_entry_safe(libinfo1, tmp1, &tmp_xvpfile->load_lib_list, node_libinfo) {
					if (0 == strcmp(libinfo1->libname, libinfo->libname)) {
						/*find the same lib loaded by other file */
						find = 1;
						break;
					}
				}
				if (1 == find) {
					pr_debug("find :%s %s\n", libinfo1->libname, libinfo->libname);
					break;
				}
			}
		}
		mutex_unlock(&xvp->xrp_known_files_lock);
		if (1 != find) {
			/*if not find in other files need unload */
			/*do unload process--------------- later */

			ret = xrp_create_unload_cmd(filp, libinfo, &unloadinfo);
			if (ret != LIB_RESULT_OK) {
				pr_err("xrp_create_unload_cmd failed, maybe library leak\n");
				result = -EINVAL;
				continue;
			}
			pr_debug("send unload cmd\n");
			mutex_unlock(&(xvp->load_lib.libload_mutex));
			libinfo->load_count = 1;	/*force set 1 here */
			snprintf(libname, XRP_NAMESPACE_ID_SIZE, "%s", libinfo->libname);
			ret = xrp_ioctl_submit_sync(filp, NULL, unloadinfo.rq);
			mutex_lock(&(xvp->load_lib.libload_mutex));
			if (ret == -ENODEV) {
				/*if went off , release here */
				xrp_library_decrease(filp, libname);
			}
			xrp_free_unload_cmd(filp, &unloadinfo);
		} else {
			list_del(&libinfo->node_libinfo);
			vfree(libinfo);
		}
	}
	mutex_unlock(&(xvp->load_lib.libload_mutex));

	/*release power hint later */
	vdsp_dvfs_release_powerhint(filp);
	return result;
}

static int xrp_boot_firmware(struct xvp *xvp)
{
	int ret;
	struct xrp_dsp_sync_v1 __iomem *shared_sync = xvp_buf_get_vaddr(xvp->ipc_buf);
	s64 tv0, tv1, tv2;

	tv0 = ktime_to_us(ktime_get());
	xrp_halt_dsp(xvp);
	xrp_reset_dsp(xvp);
	pr_debug("boot firmware name:%s,\n", xvp->firmware_name);

	if (likely(xvp->firmware_name)) {
		ret = xrp_request_firmware(xvp);

		if (ret < 0) {
			/*may be halt vdsp here, and set went off true? */
			xrp_halt_dsp(xvp);
			xvp->off = true;
			pr_err("xrp_request_firmware failed\n");
			return ret;
		}

		xrp_comm_write32(&shared_sync->sync, XRP_DSP_SYNC_IDLE);
		mb();
	}

	xrp_release_dsp(xvp);
	tv1 = ktime_to_us(ktime_get());
	ret = xrp_synchronize(xvp);

	if (unlikely(ret < 0)) {
		xrp_halt_dsp(xvp);
		pr_err("couldn't synchronize with the DSP core\n");
		xvp->off = true;
		return ret;
	}

	tv2 = ktime_to_us(ktime_get());
	/*request firmware - sync */
	pr_info("[TIME]boot firmware ok,request fw(%s):%lld (us), sync:%lld (us)\n",
		xvp->firmware_name, tv1 - tv0, tv2 - tv1);
	return 0;
}

static int sprd_vdsp_boot_firmware(struct xvp *xvp)
{
#ifdef FACEID_VDSP_FULL_TEE
	if (xvp->secmode)
		return sprd_faceid_boot_firmware(xvp);
#endif
	return xrp_boot_firmware(xvp);
}

static int sprd_unmap_request(struct file *filp, struct xrp_request *rq, uint32_t krqflag)
{
	size_t n_buffers = rq->n_buffers;
	int ret = 0;
	struct xvp_buf *in_buf;
	struct xvp_file *xvp_file = filp->private_data;

	if (krqflag == 1) {
		pr_debug("kernel request, no need unmap\n");
		return 0;
	}
	if (rq->ioctl_queue.in_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE) {
		in_buf = xvpfile_buf_get(xvp_file, rq->ioctl_queue.in_data_fd);
		if (!in_buf) {
			pr_err("Error get in_buf failed\n");
			ret |= -EFAULT;
		}
	}
	if (rq->ioctl_queue.out_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE) {
	} else {
		if (copy_to_user((void __user*)(unsigned long)rq->ioctl_queue.out_data_addr,
			rq->out_data, rq->ioctl_queue.out_data_size)) {
			pr_err("out_data could not be copied\n");
			ret |= -EFAULT;
		}
	}
	pr_debug("rq->n_buffers = %zu\n", n_buffers);
	if (n_buffers) {
		xvpfile_buf_kunmap(xvp_file, rq->dsp_buf);
		ret = xvpfile_buf_free_with_iommu(xvp_file, rq->dsp_buf);
		kfree(rq->id_dsp_pool);
		rq->n_buffers = 0;
	}
	if (ret != 0)
		pr_err("[ERROR] sprd_unmap_request failed ret:%d\n", ret);
	return ret;
}

static int sprd_map_request(struct file *filp, struct xrp_request *rq, uint32_t krqflag)
{
	struct xvp_file *xvp_file = filp->private_data;
	struct xrp_ioctl_buffer __user *buffer;
	int n_buffers = 0;
	int i;
	int nbufferflag = 0;
	int ret = 0;
	char *name = NULL;
	uint64_t size = 0;
	uint32_t heap_type = 0;
	uint32_t attr = 0;
	struct xvp_buf *dsp_buf = NULL;
	struct xvp_buf *in_buf = NULL;
	struct xvp_buf *out_buf = NULL;
	unsigned long addr = 0;

	if (1 == krqflag) {
		pr_debug("kernel request, no need map\n");
		return 0;
	}

	if ((rq->ioctl_queue.flags & XRP_QUEUE_FLAG_NSID) &&
		copy_from_user(rq->nsid, (void __user*)(unsigned long)rq->ioctl_queue.nsid_addr,
			sizeof(rq->nsid))) {
		pr_err("[ERROR]nsid could not be copied\n");
		return -EINVAL;
	}

	n_buffers = rq->ioctl_queue.buffer_size / sizeof(struct xrp_ioctl_buffer);
	rq->n_buffers = n_buffers;
	if (n_buffers) {
		nbufferflag = 1;
		name = "xvp dsp_buffer";
		size = n_buffers * sizeof(struct xrp_dsp_buffer);
		heap_type = SPRD_VDSP_MEM_HEAP_TYPE_UNIFIED;
		attr = SPRD_VDSP_MEM_ATTR_WRITECOMBINE;
		dsp_buf = xvpfile_buf_alloc_with_iommu(xvp_file, name, size, heap_type, attr);
		if (!dsp_buf) {
			pr_err("Error:xvpfile_buf_creat_alloc_kmap faild\n");
			return -1;
		}

		rq->dsp_buf = dsp_buf;
		ret = xvpfile_buf_kmap(xvp_file, rq->dsp_buf);
		if (ret) {
			pr_err("Error: xvpfile_buf_kmap failed\n");
			xvpfile_buf_free_with_iommu(xvp_file, rq->dsp_buf);
			return -EFAULT;
		}
		rq->id_dsp_pool = kzalloc(n_buffers * sizeof(*rq->id_dsp_pool), GFP_KERNEL);
		if (!rq->id_dsp_pool) {
			pr_err("[ERROR]fail to kmalloc id_dsp_pool\n");
			xvpfile_buf_kunmap(xvp_file, rq->dsp_buf);
			xvpfile_buf_free_with_iommu(xvp_file, rq->dsp_buf);
			return -ENOMEM;
		}
	}
	//in_data addr
	if ((rq->ioctl_queue.in_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE)) {
		if (rq->ioctl_queue.in_data_fd < 0) {
			pr_err("[ERROR]in data fd is:%d\n", rq->ioctl_queue.in_data_fd);
			ret = -EFAULT;
			goto free_indata_err;
		}

		in_buf = xvpfile_buf_get(xvp_file, rq->ioctl_queue.in_data_fd);
		if (!in_buf) {
			pr_err("Error get in_buf failed,buf_id=%d\n", rq->ioctl_queue.in_data_fd);
			ret = -EFAULT;
			goto free_indata_err;
		}
		rq->in_buf = in_buf;
		pr_info("in_data_fd=%d, in_data_phys=0x%x size=%d\n",
			(int)in_buf->buf_id, (uint32_t)in_buf->iova, (uint32_t)in_buf->size);
	} else {
		if (copy_from_user(rq->in_data, (void __user*)(unsigned long)rq->ioctl_queue.in_data_addr,
			rq->ioctl_queue.in_data_size)) {
			pr_err("[ERROR]in_data could not be copied\n");
			ret = -EFAULT;
			goto free_indata_err;
		}
	}

	//out_data addr
	if ((rq->ioctl_queue.out_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE)
		&& (rq->ioctl_queue.out_data_fd >= 0)) {
		out_buf = xvpfile_buf_get(xvp_file, rq->ioctl_queue.out_data_fd);
		if (!out_buf) {
			pr_err("Error get out_buf failed,buf_id=%d\n", rq->ioctl_queue.out_data_fd);
			ret = -EFAULT;
			goto free_outdata_err;
		}
		rq->out_buf = out_buf;

		pr_info("out_data_fd=%d, out_data_phys=0x%x, size=%d\n",
			(int)out_buf->buf_id, (uint32_t)out_buf->iova, (uint32_t)out_buf->size);
	}
	//bufer addr
	pr_debug("n_buffers [%d]\n", n_buffers);
	if (n_buffers) {
		buffer = (void __user*)(unsigned long)rq->ioctl_queue.buffer_addr;

		for (i = 0; i < n_buffers; ++i) {
			struct xrp_ioctl_buffer ioctl_buffer;
			struct xrp_dsp_buffer *dsp_buffer = NULL;

			if (copy_from_user(&ioctl_buffer, buffer + i, sizeof(ioctl_buffer))) {
				ret = -EFAULT;
				pr_err("[ERROR]copy from user failed\n");
				goto share_err;
			}
			if (ioctl_buffer.fd >= 0) {
				rq->id_dsp_pool[i] = ioctl_buffer.fd;
				addr = (unsigned long)rq->dsp_buf->vaddr + sizeof(struct xrp_dsp_buffer) * i;
				dsp_buffer = (struct xrp_dsp_buffer*)addr;
				dsp_buffer->flags = ioctl_buffer.flags;
				dsp_buffer->size = ioctl_buffer.size;
				dsp_buffer->addr = (uint32_t)xvpfile_buf_get_iova(
					xvpfile_buf_get(xvp_file, ioctl_buffer.fd));
				dsp_buffer->fd = ioctl_buffer.fd;

				pr_info("dsp_buffer[%d] addr:0x%x, size:%d, fd:%d, flags:%d\n",
					i, dsp_buffer->addr, dsp_buffer->size, dsp_buffer->fd, dsp_buffer->flags);
			}
		}
	}
	return 0;
share_err:
	if (ret < 0)
		sprd_unmap_request(filp, rq, krqflag);

	return ret;
free_outdata_err:
free_indata_err:
	if (nbufferflag == 1) {
		xvpfile_buf_free_with_iommu(xvp_file, dsp_buf);
		kfree(rq->id_dsp_pool);
	}
	pr_err("[MAP][OUT] map request error\n");
	return ret;
}

static void sprd_fill_hw_request(struct xrp_dsp_cmd __iomem * cmd,
	struct xrp_request *rq)
{
	xrp_comm_write32(&cmd->in_data_size, rq->ioctl_queue.in_data_size);
	xrp_comm_write32(&cmd->out_data_size, rq->ioctl_queue.out_data_size);
	xrp_comm_write32(&cmd->buffer_size, rq->n_buffers * sizeof(struct xrp_dsp_buffer));

	if (rq->ioctl_queue.in_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE) {
		xrp_comm_write32(&cmd->in_data_addr, xvpfile_buf_get_iova(rq->in_buf));
	} else {
		xrp_comm_write(&cmd->in_data, rq->in_data, rq->ioctl_queue.in_data_size);
	}

	if (rq->ioctl_queue.out_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE)
		xrp_comm_write32(&cmd->out_data_addr, xvpfile_buf_get_iova(rq->out_buf));

	if (rq->n_buffers) {
		if (rq->n_buffers > XRP_DSP_CMD_INLINE_BUFFER_COUNT)
			xrp_comm_write32(&cmd->buffer_addr, xvpfile_buf_get_iova(rq->dsp_buf));
		else
			xrp_comm_write(&cmd->buffer_data, xvpfile_buf_get_vaddr(rq->dsp_buf),
				rq->n_buffers * sizeof(struct xrp_dsp_buffer));
	}

	if (rq->ioctl_queue.flags & XRP_QUEUE_FLAG_NSID)
		xrp_comm_write(&cmd->nsid, rq->nsid, sizeof(rq->nsid));

	wmb();
	/* update flags */
	xrp_comm_write32(&cmd->flags,
		(rq->ioctl_queue.flags & ~XRP_DSP_CMD_FLAG_RESPONSE_VALID) |
		XRP_DSP_CMD_FLAG_REQUEST_VALID);
}

static long sprd_complete_hw_request(struct xrp_dsp_cmd __iomem * cmd,
	struct xrp_request *rq)
{
	u32 flags = xrp_comm_read32(&cmd->flags);

	if (rq->ioctl_queue.out_data_size <= XRP_DSP_CMD_INLINE_DATA_SIZE)
		xrp_comm_read(&cmd->out_data, rq->out_data, rq->ioctl_queue.out_data_size);
	if (rq->n_buffers <= XRP_DSP_CMD_INLINE_BUFFER_COUNT && rq->n_buffers != 0)
		xrp_comm_read(&cmd->buffer_data, xvpfile_buf_get_vaddr(rq->dsp_buf),
			rq->n_buffers * sizeof(struct xrp_dsp_buffer));
	xrp_comm_write32(&cmd->flags, 0);

	return (flags & XRP_DSP_CMD_FLAG_RESPONSE_DELIVERY_FAIL) ? -ENXIO : 0;
}

static long xrp_ioctl_submit_sync(struct file *filp, struct xrp_ioctl_queue __user * p,
	struct xrp_request *pk_rq)
{
	struct xvp_file *xvp_file = filp->private_data;
	struct xvp *xvp = xvp_file->xvp;
	struct xrp_comm *queue = xvp->queue;
	struct xrp_request xrp_rq = { 0 };
	struct xrp_request *rq = &xrp_rq;
	long ret = 0;
	bool went_off = false;
	enum load_unload_flag load_flag = XRP_LOAD_LIB_FLAG_MAX;
	char libname[XRP_NAMESPACE_ID_SIZE] = { 0 };
	bool rebootflag = 0;
	int32_t lib_result = 0;
	uint32_t krqflag = 0;

	s64 tv0, tv1, tv2, tv3, tv4, tv5;

	tv0 = tv1 = tv2 = tv3 = tv4 = tv5 = 0;
	tv0 = ktime_to_us(ktime_get());
	if (p != NULL) {
		if (unlikely(copy_from_user(&rq->ioctl_queue, p, sizeof(*p))))
			return -EFAULT;
	} else if (pk_rq != NULL) {
		/*null for kernel */
		krqflag = 1;
		memcpy(rq, pk_rq, sizeof(*pk_rq));
	} else {
		pr_err("Invalid argument\n");
		return -EINVAL;
	}
	if (unlikely(rq->ioctl_queue.flags & ~XRP_QUEUE_VALID_FLAGS)) {
		pr_err("invalid flags 0x%08x\n", rq->ioctl_queue.flags);
		return -EINVAL;
	}

	if (xvp->n_queues > 1) {
		unsigned n = (rq->ioctl_queue.flags & XRP_QUEUE_FLAG_PRIO) >> XRP_QUEUE_FLAG_PRIO_SHIFT;

		if (n >= xvp->n_queues)
			n = xvp->n_queues - 1;
		queue = xvp->queue_ordered[n];
		pr_debug("queue index:%d, priority: %d\n", n, queue->priority);
	}
	tv1 = ktime_to_us(ktime_get());
	ret = sprd_map_request(filp, rq, krqflag);
	tv1 = ktime_to_us(ktime_get()) - tv1;

	if (unlikely(ret < 0)) {
		pr_err("[ERROR]map request fail\n");
		return ret;
	}

	{
		int reboot_cycle;

retry:
		mutex_lock(&queue->lock);
		reboot_cycle = atomic_read(&xvp->reboot_cycle);
		if (reboot_cycle != atomic_read(&xvp->reboot_cycle_complete)) {
			mutex_unlock(&queue->lock);
			goto retry;
		}

		if (unlikely(xvp->off)) {
			ret = -ENODEV;
		} else {
			/*check whether libload command and if it is, do load */
			tv2 = ktime_to_us(ktime_get());
			load_flag = xrp_check_load_unload(xvp, rq, krqflag);
			pr_info("cmd nsid:%s,(cmd:0/load:1/unload:2)flag:%d, filp:[%p]\n",
				rq->nsid, load_flag, filp);
			mutex_lock(&(xvp->load_lib.libload_mutex));
			lib_result = xrp_pre_process_request(filp, rq, load_flag, libname, krqflag);
			tv2 = ktime_to_us(ktime_get()) - tv2;	//lib load/unload
			if (lib_result != 0) {
				mutex_unlock(&queue->lock);
				mutex_unlock(&(xvp->load_lib.libload_mutex));
				ret = sprd_unmap_request(filp, rq, krqflag);
				if (lib_result == -EEXIST) {
					return 0;
				} else {
					pr_err("xrp pre process failed lib_result:%d\n", lib_result);
					return lib_result;
				}
			} else if ((load_flag != XRP_UNLOAD_LIB_FLAG) && (load_flag != XRP_LOAD_LIB_FLAG)) {
				mutex_unlock(&(xvp->load_lib.libload_mutex));
			}

			sprd_fill_hw_request(queue->comm, rq);
			tv3 = ktime_to_us(ktime_get());

			xrp_send_device_irq(xvp);
			pr_info("send vdsp cmd-32k time[%lld]\n", sprd_sysfrt_read());

			if (xvp->host_irq_mode) {
				ret = xvp_complete_cmd_irq(xvp, queue, xrp_cmd_complete);
			} else {
				ret = xvp_complete_cmd_poll(xvp, queue, xrp_cmd_complete);
			}
			tv3 = ktime_to_us(ktime_get()) - tv3;	//send irq->reci

			pr_debug("xvp_complete_cmd_irq ret:%ld\n", ret);
			/* copy back inline data */
			if (likely(ret == 0)) {
				ret = sprd_complete_hw_request(queue->comm, rq);
			} else if (ret == -EBUSY && firmware_reboot &&
			           atomic_inc_return(&xvp->reboot_cycle) == reboot_cycle + 1) {
				int rc;
				unsigned i;

				if ((load_flag == XRP_LOAD_LIB_FLAG) || (load_flag == XRP_UNLOAD_LIB_FLAG)) {
					mutex_unlock(&(xvp->load_lib.libload_mutex));
				}
				//dump vdsp
				vdsp_log_coredump(xvp);
				pr_info("###enter reboot flow!###\n");
				for (i = 0; i < xvp->n_queues; ++i)
					if (xvp->queue + i != queue)
						mutex_lock(&xvp->queue[i].lock);
				rc = sprd_vdsp_boot_firmware(xvp);

				/*release library loaded here because vdsp is reseting ok
				   so release all library resource here, but if boot failed
				   the libraries loaded may be leaked because we don't know whether
				   vdsp processing these resource or not */
				if (rc == 0) {
					mutex_lock(&(xvp->load_lib.libload_mutex));
					xrp_library_release_all(xvp);
					mutex_unlock(&(xvp->load_lib.libload_mutex));
				}
				atomic_set(&xvp->reboot_cycle_complete, atomic_read(&xvp->reboot_cycle));
				for (i = 0; i < xvp->n_queues; ++i)
					if (xvp->queue + i != queue)
						mutex_unlock(&xvp->queue[i].lock);
				if (unlikely(rc < 0)) {
					ret = rc;
					went_off = xvp->off;
					pr_err("vdsp reboot failed may be encounter fatal error!!!\n");
				}
				pr_debug("###reboot flow end!###\n");
				rebootflag = 1;
			}
			if (likely(0 == rebootflag)) {
				if ((load_flag != XRP_LOAD_LIB_FLAG) && (load_flag != XRP_UNLOAD_LIB_FLAG))
					mutex_lock(&(xvp->load_lib.libload_mutex));

				lib_result = post_process_request(filp, rq, libname, load_flag, ret);

				if ((load_flag == XRP_LOAD_LIB_FLAG) || (load_flag == XRP_UNLOAD_LIB_FLAG)) {
					mutex_unlock(&(xvp->load_lib.libload_mutex));
				} else {
					if (unlikely(lib_result != 0)) {
						mutex_unlock(&queue->lock);
						mutex_unlock(&(xvp->load_lib.libload_mutex));
						sprd_unmap_request(filp, rq, krqflag);
						pr_err("[ERROR]lib result error\n");
						return -EFAULT;
					} else {
						mutex_unlock(&(xvp->load_lib.libload_mutex));
					}
				}
			}
		}
		mutex_unlock(&queue->lock);
	}

	tv4 = ktime_to_us(ktime_get());
	if (likely(ret == 0))
		ret = sprd_unmap_request(filp, rq, krqflag);
	else if (!went_off)
		sprd_unmap_request(filp, rq, krqflag);
	/*
	 * Otherwise (if the DSP went off) all mapped buffers are leaked here.
	 * There seems to be no way to recover them as we don't know what's
	 * going on with the DSP; the DSP may still be reading and writing
	 * this memory.
	 */
	tv5 = ktime_to_us(ktime_get());
	pr_info("[TIME](cmd->nsid:%s)total:%lld(us),map:%lld(us),load/unload:%lld(us),"
	     "vdsp:%lld(us),unmap:%lld(us),ret:%ld\n", rq->nsid, tv5 - tv0, tv1,
	     tv2, tv3, tv5 - tv4, ret);

	return ret;
}

static long xrp_ioctl_faceid_cmd(struct file *filp, struct xrp_faceid_ctrl __user *arg)
{
#ifdef FACEID_VDSP_FULL_TEE
	struct xrp_faceid_ctrl faceid;
	struct xvp_file *xvp_file = filp->private_data;
	struct xvp *xvp = xvp_file->xvp;

	if (unlikely(copy_from_user(&faceid, arg, sizeof(struct xrp_faceid_ctrl)))) {
		pr_err("[ERROR]copy from user failed\n");
		return -EFAULT;
	}
	pr_debug("faceid:in %d, image %d\n", faceid.inout_fd, faceid.img_fd);

	sprd_faceid_run_vdsp(xvp, faceid.inout_fd, faceid.img_fd);
#endif
	return 0;
}

static long xrp_ioctl_set_dvfs(struct file *filp, struct xrp_dvfs_ctrl __user *arg)
{
	struct xrp_dvfs_ctrl dvfs;
	struct xvp_file *xvp_file = filp->private_data;
	struct xvp *xvp = xvp_file->xvp;

	if (unlikely(copy_from_user(&dvfs, arg, sizeof(struct xrp_dvfs_ctrl)))) {
		pr_err("[ERROR]copy from user failed\n");
		return -EFAULT;
	}
	if (0 == dvfs.en_ctl_flag) {
		vdsp_dvfs_set(xvp, dvfs.level);
	} else {
		if (dvfs.enable)
			vdsp_dvfs_enable(xvp);
		else
			vdsp_dvfs_disable(xvp);
	}
	return 0;
}

static long xrp_ioctl_set_powerhint(struct file *filp, struct xrp_powerhint_ctrl __user * arg)
{
	struct xrp_powerhint_ctrl powerhint;

	if (unlikely(copy_from_user(&powerhint, arg, sizeof(struct xrp_powerhint_ctrl)))) {
		pr_err("copy_from_user failed\n");
		return -EFAULT;
	}
	return (long)vdsp_dvfs_set_powerhint(filp, powerhint.level, powerhint.flag);
}

/******************* mem related ioctl **********************/
static long xrp_ioctl_mem_query(struct file *filp, struct xrp_heaps_ctrl __user *arg)
{
	struct xrp_heaps_ctrl data;
	int ret;
	int id, i = 0;

	memset(&data, 0, sizeof(data));

	for (id = SPRD_VDSP_MEM_MAN_MIN_HEAP;
		id <= SPRD_VDSP_MEM_MAN_MAX_HEAP && i < SPRD_MAX_HEAPS; id++) {
		uint8_t type;
		uint32_t attrs;

		ret = sprd_vdsp_mem_get_heap_info(id, &type, &attrs);
		if (!ret) {
			struct xrp_heap_data *info = &data.heaps[i++];

			info->id = id;
			info->type = type;
			info->attributes = attrs;
		}
	}
	if (copy_to_user(arg, &data, sizeof(data)))
		return -EFAULT;

	return 0;
}

static int xrp_ioctl_mem_import(struct file *filp, struct xrp_import_ctrl __user *arg)
{
	struct xrp_import_ctrl ctrl;
	struct xvp_file *xvp_file = filp->private_data;
	struct xvp_buf *buf = NULL;

	if (unlikely(copy_from_user(&ctrl, arg, sizeof(struct xrp_import_ctrl)))) {
		pr_err("[ERROR]copy from user failed\n");
		return -EFAULT;
	}
	pr_debug("import:size %lld, fd %llX, cpu_ptr %llX, heap_id %d attr %d, name %s\n ",
		ctrl.size, ctrl.buf_fd, ctrl.cpu_ptr, ctrl.heap_id, ctrl.attributes, ctrl.name);
	buf = xvpfile_buf_import(xvp_file, ctrl.name, (size_t)ctrl.size, ctrl.heap_id, ctrl.attributes,
		ctrl.buf_fd, ctrl.cpu_ptr);
	if (!buf) {
		pr_err("Error: xrp_ioctl_mem_import failed\n");
		return -1;
	}
	ctrl.buf_id = buf->buf_id;

	pr_info("import:size %lld, fd %llX, cpu_ptr %llX, heap_id %d attr %d, name %s -> buf_id=%d\n",
		ctrl.size, ctrl.buf_fd, ctrl.cpu_ptr, ctrl.heap_id, ctrl.attributes, ctrl.name, buf->buf_id);
	if (unlikely(copy_to_user(arg, &ctrl, sizeof(struct xrp_import_ctrl)))) {
		pr_err("[ERROR]copy to user failed\n");
		return -EFAULT;
	}

	return 0;
}

static long xrp_ioctl_mem_export(struct file *filp, struct xrp_export_ctrl __user *arg)
{
	struct xrp_export_ctrl ctrl;
	struct xvp_file *xvp_file = filp->private_data;
	struct xvp_buf *buf = NULL;
	int ret;

	if (unlikely(copy_from_user(&ctrl, arg, sizeof(struct xrp_export_ctrl)))) {
		pr_err("[ERROR]copy from user failed\n");
		return -EFAULT;
	}
	pr_info("export:buf_id %d, size %lld, attr %d\n", ctrl.buf_id, ctrl.size, ctrl.attributes);
	buf = xvpfile_buf_get(xvp_file, ctrl.buf_id);
	if (!buf) {
		pr_err("xvpfile_buf_get fail\n");
		return -1;
	}
	ret = xvpfile_buf_export(xvp_file, buf);
	if (ret) {
		pr_err("xvpfile_buf_export fail\n");
		return ret;
	}
	if (unlikely(copy_to_user(arg, &ctrl, sizeof(struct xrp_export_ctrl)))) {
		pr_err("[ERROR]copy to user failed\n");
		return -EFAULT;
	}

	return 0;
}

static long xrp_ioctl_mem_alloc(struct file *filp, struct xrp_alloc_ctrl __user * arg)
{
	struct xrp_alloc_ctrl ctrl;
	struct xvp_file *xvp_file = filp->private_data;
	struct xvp_buf *buf = NULL;

	if (unlikely(copy_from_user(&ctrl, arg, sizeof(struct xrp_alloc_ctrl)))) {
		pr_err("[ERROR]copy from user failed\n");
		return -EFAULT;
	}
	pr_debug("alloc:size %lld, heap_id %d, attr %d, name %s\n",
		ctrl.size, ctrl.heap_id, ctrl.attributes, ctrl.name);

	buf = xvpfile_buf_alloc(xvp_file, ctrl.name, ctrl.size, ctrl.heap_id, ctrl.attributes);
	if (!buf) {
		pr_err("Error: xvpfile_buf_alloc\n");
		return -1;
	}
	ctrl.buf_id = buf->buf_id;
	pr_info("alloc:size %lld, heap_id %d, attr %d, name %s --> buf id=%d\n",
		ctrl.size, ctrl.heap_id, ctrl.attributes, ctrl.name, buf->buf_id);

	if (unlikely(copy_to_user(arg, &ctrl, sizeof(struct xrp_alloc_ctrl)))) {
		pr_err("[ERROR]copy to user failed\n");
		xvpfile_buf_free(xvp_file, buf);
		return -EFAULT;
	}
	return 0;
}

static long xrp_ioctl_mem_free(struct file *filp, struct xrp_free_ctrl __user *arg)
{
	struct xrp_free_ctrl ctrl;
	struct xvp_file *xvp_file = filp->private_data;

	if (unlikely(copy_from_user(&ctrl, arg, sizeof(struct xrp_free_ctrl)))) {
		pr_err("[ERROR]copy from user failed\n");
		return -EFAULT;
	}

	pr_debug("free:buf_id %d \n", ctrl.buf_id);
	xvpfile_buf_free(xvp_file, xvpfile_buf_get(xvp_file, ctrl.buf_id));

	return 0;
}

static int xrp_ioctl_mem_iommu_map(struct file *filp, struct xrp_map_ctrl __user *arg)
{
	struct xrp_map_ctrl ctrl;
	struct xvp_file *xvp_file = filp->private_data;
	struct xvp_buf *buf = NULL;
	int ret;

	if (unlikely(copy_from_user(&ctrl, arg, sizeof(struct xrp_map_ctrl)))) {
		pr_err("[ERROR]copy from user failed\n");
		return -EFAULT;
	}
	pr_debug("buf_id %d, flags %d\n", ctrl.buf_id, ctrl.flags);

	buf = xvpfile_buf_get(xvp_file, ctrl.buf_id);
	if (!buf) {
		pr_err("Error: xvpfile_buf_get failed\n");
		//whether return or not
	}
	ret = xvpfile_buf_iommu_map(xvp_file, buf);
	if (ret) {
		pr_err("Error: xvpfile_buf_iommu_map, ret %d\n", ret);
		return -1;
	}

	return ret;
}

static long xrp_ioctl_mem_iommu_unmap(struct file *filp, struct xrp_unmap_ctrl __user *arg)
{
	struct xrp_unmap_ctrl ctrl;
	struct xvp_file *xvp_file = filp->private_data;

	int ret;

	if (unlikely(copy_from_user(&ctrl, arg, sizeof(struct xrp_unmap_ctrl)))) {
		pr_err("[ERROR]copy from user failed\n");
		return -EFAULT;
	}
	pr_debug("iommu unmap buf_id=%d\n", ctrl.buf_id);
	ret = xvpfile_buf_iommu_unmap(xvp_file, xvpfile_buf_get(xvp_file, ctrl.buf_id));
	if (ret) {
		pr_err("Error: xvpfile_buf_iommu_unmap, ret %d\n", ret);
	}
	return ret;
}

static long xrp_ioctl_mem_cpu_to_device(struct file *filp,
	struct xrp_sync_cpu_to_device_ctrl __user *arg)
{
	struct xrp_sync_cpu_to_device_ctrl ctrl;
	struct xvp_file *xvp_file = filp->private_data;
	struct xvp *xvp = xvp_file->xvp;
	int ret;

	if (unlikely(copy_from_user(&ctrl, arg, sizeof(struct xrp_sync_cpu_to_device_ctrl)))) {
		pr_err("[ERROR]copy from user failed\n");
		return -EFAULT;
	}
	pr_debug("buf_id=%d\n", ctrl.buf_id);
	ret = sprd_vdsp_mem_sync_cpu_to_device(xvp->drv_mem_ctx, ctrl.buf_id);
	if (ret) {
		pr_err("Error: xrp_ioctl_mem_cpu_to_device, ret %d\n", ret);
	}
	return ret;
}

static long xrp_ioctl_mem_device_to_cpu(struct file *filp,
	struct xrp_sync_device_to_cpu_ctrl __user *arg)
{
	struct xrp_sync_device_to_cpu_ctrl ctrl;
	struct xvp_file *xvp_file = filp->private_data;
	struct xvp *xvp = xvp_file->xvp;
	int ret;

	if (unlikely(copy_from_user(&ctrl, arg, sizeof(struct xrp_sync_device_to_cpu_ctrl)))) {
		pr_err("[ERROR]copy from user failed\n");
		return -EFAULT;
	}
	pr_debug("buf_id=%d\n", ctrl.buf_id);
	ret = sprd_vdsp_mem_sync_device_to_cpu(xvp->drv_mem_ctx, ctrl.buf_id);
	if (ret) {
		pr_err("Error: xrp_ioctl_mem_device_to_cpu, ret %d\n", ret);
	}
	return ret;
}

static long xvp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long retval = -EINVAL;
	struct xvp_file *xvp_file = NULL;

	pr_debug("cmd:%x, filp[%p]:cmd_name:%s\n",
		cmd, filp, debug_get_ioctl_cmd_name(cmd));
	mutex_lock(&xvp_global_lock);
	if (unlikely(filp->private_data == NULL)) {
		mutex_unlock(&xvp_global_lock);
		pr_warn("filp private is NULL\n");
		return retval;
	}
	xvp_file = filp->private_data;
	mutex_lock(&xvp_file->lock);
	xvp_file->working = 1;
	mutex_unlock(&xvp_file->lock);
	mutex_unlock(&xvp_global_lock);

	switch (cmd) {
		case XRP_IOCTL_ALLOC:
		case XRP_IOCTL_FREE:
			break;

		case XRP_IOCTL_QUEUE:
		case XRP_IOCTL_QUEUE_NS:
			vdsp_dvfs_preprocess(((struct xvp_file *)(filp->private_data))->xvp);
			retval = xrp_ioctl_submit_sync(filp, (struct xrp_ioctl_queue __user *) arg, NULL);
			vdsp_dvfs_postprocess(((struct xvp_file *)(filp->private_data))->xvp);
			break;

		case XRP_IOCTL_SET_DVFS:
			retval = xrp_ioctl_set_dvfs(filp, (struct xrp_dvfs_ctrl __user *) arg);
			break;

		case XRP_IOCTL_SET_POWERHINT:
			retval = xrp_ioctl_set_powerhint(filp, (struct xrp_powerhint_ctrl __user *) arg);
			break;

		case XRP_IOCTL_FACEID_CMD:
			retval = xrp_ioctl_faceid_cmd(filp, (struct xrp_faceid_ctrl __user *) arg);
			break;

		case XRP_IOCTL_MEM_QUERY:
			retval = xrp_ioctl_mem_query(filp, (struct xrp_heaps_ctrl __user *) arg);
			break;

		case XRP_IOCTL_MEM_IMPORT:
			retval = xrp_ioctl_mem_import(filp, (struct xrp_import_ctrl __user *) arg);
			break;

		case XRP_IOCTL_MEM_EXPORT:
			retval = xrp_ioctl_mem_export(filp, (struct xrp_export_ctrl __user *) arg);
			break;

		case XRP_IOCTL_MEM_ALLOC:
			retval = xrp_ioctl_mem_alloc(filp, (struct xrp_alloc_ctrl __user *) arg);
			break;

		case XRP_IOCTL_MEM_FREE:
			retval = xrp_ioctl_mem_free(filp, (struct xrp_free_ctrl __user *) arg);
			break;

		case XRP_IOCTL_MEM_IOMMU_MAP:
			retval = xrp_ioctl_mem_iommu_map(filp, (struct xrp_map_ctrl __user *) arg);
			break;

		case XRP_IOCTL_MEM_IOMMU_UNMAP:
			retval = xrp_ioctl_mem_iommu_unmap(filp, (struct xrp_unmap_ctrl __user *) arg);
			break;

		case XRP_IOCTL_MEM_CPU_TO_DEVICE:
			retval = xrp_ioctl_mem_cpu_to_device(filp, (struct xrp_sync_cpu_to_device_ctrl __user *) arg);
			break;

		case XRP_IOCTL_MEM_DEVICE_TO_CPU:
			retval = xrp_ioctl_mem_device_to_cpu(filp, (struct xrp_sync_device_to_cpu_ctrl __user *) arg);
			break;

		default:
			retval = -EINVAL;
			break;
	}
	mutex_lock(&xvp_file->lock);
	xvp_file->working = 0;
	mutex_unlock(&xvp_file->lock);
	return retval;
}

static int xrp_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct xvp_file *xvp_file = NULL;
	int buf_id;

	if (unlikely(filp->private_data == NULL)) {
		pr_err("filp private is NULL\n");
		return -EINVAL;
	}
	xvp_file = filp->private_data;

	buf_id = vma->vm_pgoff;
	pr_debug("vma start %#lx end %#lx, buf_id %d\n", vma->vm_start, vma->vm_end, buf_id);

	return sprd_vdsp_mem_map_um(xvp_file->xvp->drv_mem_ctx, buf_id, vma);
}

int sprd_vdsp_misc_bufs_init(struct xvp *xvp)
{
	char *name = NULL;
	uint64_t size = 0;
	uint32_t heap_type = 0;
	uint32_t attr = 0;
	struct xvp_buf *buf = NULL;
	int ret = 0;

#ifdef FACEID_VDSP_FULL_TEE
	if (xvp->secmode) {
		ret = sprd_faceid_iommu_map_buffer(xvp);
		if (ret != 0) {
			pr_err("Error: sprd_iommu_map_faceid_fwbuffer failed\n");
			return ret;
		}
	} else
#endif
	{
		//extra buffer NOTE:6M, so alloc when open
		name = "xvp fw buffer";
		size = VDSP_FIRMWIRE_SIZE;
		heap_type = SPRD_VDSP_MEM_HEAP_TYPE_UNIFIED;
		attr = SPRD_VDSP_MEM_ATTR_WRITECOMBINE;
		buf = xvp_buf_alloc(xvp, name, size, heap_type, attr);
		if (!buf) {
			pr_err("Error:xvp_buf_alloc faild\n");
			goto err;
		}
		xvp->fw_buf = buf;
		ret = xvp_buf_kmap(xvp, xvp->fw_buf);
		if (ret) {
			pr_err("Error: xvp_buf_kmap failed\n");
			goto err_xvp_buf_kmap;
		}

		// fw buffer iommu map
		xvp->fw_buf->isfixed = 1;    // iommu map fixed offset
		xvp->fw_buf->fixed_data = 0;  // finxed offset
		if (xvp_buf_iommu_map(xvp, xvp->fw_buf)) {	//NOTE:: fw buffer iova must offset 0
			goto err_extrabuf_iommu_map;
		}
		// ipc buffer  iommu map
		if (xvp_buf_iommu_map(xvp, xvp->ipc_buf)) {
			goto err_com_iommu_map;
		}
		if (vdsp_log_buf_iommu_map(xvp)) {
			goto err_log_buf_iommu_map;
		}
	}
	return 0;
err_log_buf_iommu_map:
	xvp_buf_iommu_unmap(xvp, xvp->ipc_buf);
err_com_iommu_map:
	xvp_buf_iommu_unmap(xvp, buf);
err_extrabuf_iommu_map:
	xvp_buf_kunmap(xvp, xvp->fw_buf);
err_xvp_buf_kmap:
	xvp_buf_free(xvp, buf);
	xvp->fw_buf = NULL;
err:
	return -1;
}

static int sprd_vdsp_misc_bufs_deinit(struct xvp *xvp)
{
#ifdef FACEID_VDSP_FULL_TEE
	int ret = 0;
	if (xvp->secmode) {
		ret = sprd_faceid_iommu_unmap_buffer(xvp);
		if (ret != 0) {
			pr_err("Error: sprd_iommu_unmap_faceid_fwbuffer failed\n");
		}
	} else
#endif
	{
		if (vdsp_log_buf_iommu_unmap(xvp)) {
			goto err;
		}
		if (xvp_buf_iommu_unmap(xvp, xvp->ipc_buf)) {
			goto err;
		}
		if (xvp_buf_iommu_unmap(xvp, xvp->fw_buf)) {
			goto err;
		}
		xvp_buf_kunmap(xvp, xvp->fw_buf);
		if (xvp_buf_free(xvp, xvp->fw_buf)) {
			pr_err("Error:xvp_buf_free faild\n");
			goto err;
		}
	}
	return 0;
err:
	return -1;
}

int xvp_open(struct inode *inode, struct file *filp)
{
	struct xvp *xvp = container_of(filp->private_data, struct xvp, miscdev);
	struct xvp_file *xvp_file;
	int ret = 0;
	uint32_t opentype = 0xffffffff;
	s64 tv0, tv1, tv2, tv3;

#ifdef MYL5
	struct vdsp_ipi_ctx_desc *vdsp_ipi_desc = get_vdsp_ipi_ctx_desc();
#endif

	tv0 = ktime_to_us(ktime_get());

	pr_info("vdsp open, xvp is:%p, filp:%p, flags:0x%x, fmode:0x%x\n",
		xvp, filp, filp->f_flags, filp->f_mode);
	mutex_lock(&xvp_global_lock);
#ifdef FACEID_VDSP_FULL_TEE
	if (filp->f_flags & O_APPEND) {
		/*check cur open type */
		if (likely((xvp->cur_opentype == 0xffffffff) || (xvp->cur_opentype == 1))) {
			pr_debug("open faceid mode!!!!!\n");
			ret = sprd_faceid_secboot_init(xvp);
			if (unlikely(ret < 0)) {
				goto err_unlock;
			}
			opentype = 1;
		} else {
			pr_err("open faceid mode but refused by curr opentype:%u\n", xvp->cur_opentype);
			ret = -EINVAL;
			goto err_unlock;
		}
	} else
#endif
	{
		if (unlikely((xvp->cur_opentype != 0xffffffff) && (xvp->cur_opentype != 0))) {
			pr_err("open failed refused by curr opentype:%u\n", xvp->cur_opentype);
			ret = -EINVAL;
			goto err_unlock;
		}
#ifdef MYN6
		if (unlikely(vdsp_hw_irq_register(xvp->hw_arg))) {
			pr_err("vdsp_hw_irq_register failed\n");
			ret = -EINVAL;
			goto err_unlock;
		}
#endif
#ifdef MYL5
		vdsp_ipi_desc->ops->irq_register(0, xrp_hw_irq_handler_ex, xvp);
#endif
		opentype = 0;
	}
	xvp_file = devm_kzalloc(xvp->dev, sizeof(*xvp_file), GFP_KERNEL);
	if (unlikely(!xvp_file)) {
		pr_err("Error: devm_kzalloc failed\n");
		ret = -ENOMEM;
		goto alloc_xvp_file_fault;
	} else {
		/*init lists in xvp_file */
		INIT_LIST_HEAD(&xvp_file->load_lib_list);
		mutex_init(&xvp_file->lock);
	}
	ret = xvpfile_buf_init(xvp_file);
	if (ret) {
		pr_err("Error: xvpfile_buf_init failed\n");
		goto xvpfile_buf_init_fault;
	}

	tv1 = ktime_to_us(ktime_get());
	if (!xvp->open_count) {
		ret = xvp_enable_dsp(xvp);
		//TBD enable function can not return failed status
		if (unlikely(ret < 0)) {
			pr_err("[ERROR]couldn't enable DSP\n");
			goto xvp_enable_dsp_fault;
		}
		ret = sprd_vdsp_misc_bufs_init(xvp);
		if (unlikely(ret < 0)) {
			pr_err("Error: sprd_vdsp_misc_bufs_init failed\n");
			goto first_open_buf_init_fault;
		}
		ret = vdsp_dvfs_init(xvp);
		if (unlikely(ret < 0)) {
			pr_err("vdsp dvfs init failed\n");
			goto dvfs_init_fault;
		}
#ifdef MYN6
		/*init commuincation hw */
		ret = xvp_init_communicaiton(xvp);	//enable mailbox
		if (unlikely(ret != 0)) {
			pr_err("init communication hw failed\n");
			goto init_communicaiton_fault;
		}
#endif

#ifdef MYL5
		ret = pm_runtime_get_sync(xvp->dev);
		if (unlikely(ret < 0)) {
			pr_err("[ERROR]pm_runtime_get_sync failed\n");
			goto dvfs_fault;
		} else {
			ret = 0;
		}
#endif

		xvp_set_qos(xvp);
		ret = sprd_vdsp_boot_firmware(xvp);
		if (unlikely(ret < 0)) {
			pr_err("[ERROR]vdsp boot firmware failed\n");
			goto boot_fault;
		}
		xvp->off = false;
	} else {
#ifdef MYL5
		ret = pm_runtime_get_sync(xvp->dev);
		if (unlikely(ret < 0)) {
			pr_err("[ERROR]pm_runtime_get_sync failed\n");
			goto dvfs_fault;
		} else {
			ret = 0;
		}
#endif
	}
	tv2 = ktime_to_us(ktime_get());
	xvp_file->xvp = xvp;
	filp->private_data = xvp_file;
	xrp_add_known_file(filp);
	tv3 = ktime_to_us(ktime_get());
	/*total - map */
	pr_info("[TIME]VDSP Open total(xvp->sync done):%lld(us),map firmware:%lld(us),ret:%d\n",
		tv3 - tv0, tv2 - tv1, ret);
	xvp->open_count++;
	xvp->cur_opentype = opentype;
	mutex_unlock(&xvp_global_lock);
	return ret;

boot_fault:
#ifdef MYL5
	pm_runtime_put_sync(xvp->dev);
dvfs_fault:
#endif
#ifdef MYN6
	xvp_deinit_communicaiton(xvp);
init_communicaiton_fault:
#endif
	vdsp_dvfs_deinit(xvp);
dvfs_init_fault:
	sprd_vdsp_misc_bufs_deinit(xvp);
first_open_buf_init_fault:
	xvp_disable_dsp(xvp);
xvp_enable_dsp_fault:
	xvpfile_buf_deinit(xvp_file);
xvpfile_buf_init_fault:
	devm_kfree(xvp->dev, xvp_file);
alloc_xvp_file_fault:
err_unlock:
	pr_err("[ERROR]ret = %d\n", ret);
#ifdef FACEID_VDSP_FULL_TEE
	if (opentype == 1) {
		sprd_faceid_secboot_deinit(xvp);
	}
#endif
	mutex_unlock(&xvp_global_lock);
	return ret;
}

static int xvp_close(struct inode *inode, struct file *filp)
{
	int ret = 0;
	int retmid = 0;
	uint32_t vdsp_count = 0;
	struct xvp_file *xvp_file = filp->private_data;
	struct xvp *xvp = (struct xvp *)(xvp_file->xvp);

	mutex_lock(&xvp_global_lock);
	mutex_lock(&xvp_file->lock);
	while (xvp_file->working) {
		mutex_unlock(&xvp_file->lock);
		pr_warn("xvpfile is working\n");
		schedule();	//what is the meaning? automaticlly abandon CPU.
		mutex_lock(&xvp_file->lock);
	}
#ifdef MYL5
	/*wait for xvp_file idle */
	pm_runtime_put_sync(xvp_file->xvp->dev);
#endif
	xvp_file->xvp->open_count--;
	vdsp_count = xvp_file->xvp->open_count;
	pr_debug("xvp_close open_count is:%d, filp:%p\n", xvp_file->xvp->open_count, filp);
	// debug_check_xvp_buf_leak(xvp_file); //for DEBUG
	if (0 == xvp_file->xvp->open_count) {
#ifdef MYN6
		xrp_stop_dsp(xvp_file->xvp);
		retmid = xvp_deinit_communicaiton(xvp);	//disable mailbox
#endif
		/*release xvp load_lib info */
		mutex_lock(&(xvp->load_lib.libload_mutex));
		ret = xrp_library_release_all(xvp_file->xvp);
		mutex_unlock(&(xvp->load_lib.libload_mutex));
		xvpfile_buf_deinit(xvp_file);
		sprd_vdsp_misc_bufs_deinit(xvp_file->xvp);
		xvp_file->xvp->cur_opentype = 0xffffffff;
		vdsp_dvfs_deinit(xvp_file->xvp);
		xvp_disable_dsp(xvp_file->xvp);
	} else {
		xvp_file_release_list(filp);
		xvpfile_buf_deinit(xvp_file);
	}
#ifdef FACEID_VDSP_FULL_TEE
	ret = sprd_faceid_secboot_deinit(xvp_file->xvp);
#endif
	/*release lists in xvp_file */
	xrp_remove_known_file(filp);
	filp->private_data = NULL;
	mutex_unlock(&xvp_file->lock);
	devm_kfree(xvp_file->xvp->dev, xvp_file);
	mutex_unlock(&xvp_global_lock);

	pr_info("[OUT]vdsp close, open count:%d, ret:%d\n", vdsp_count, ret | retmid);
	return (ret | retmid);
}

static const struct file_operations xvp_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.unlocked_ioctl = xvp_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = xvp_ioctl,
#endif
	.open = xvp_open,
	.release = xvp_close,
	.mmap = xrp_mmap,
};

#ifdef MYL5
int vdsp_runtime_resume(struct device *dev)
{
	struct vdsp_ipi_ctx_desc *ipidesc = NULL;
	struct xvp *xvp = dev_get_drvdata(dev);

	if (unlikely(xvp->off)) {
		//TBD CHECK
	}

	ipidesc = get_vdsp_ipi_ctx_desc();
	if (ipidesc) {
		pr_debug("ipi init called\n");
		ipidesc->ops->ctx_init(ipidesc);
	}

	/*set qos */
	xvp_set_qos(xvp);

	return 0;
}

EXPORT_SYMBOL(vdsp_runtime_resume);

int vdsp_runtime_suspend(struct device *dev)
{
	struct vdsp_ipi_ctx_desc *ipidesc = NULL;
	struct xvp *xvp = dev_get_drvdata(dev);

	xrp_halt_dsp(xvp);
	ipidesc = get_vdsp_ipi_ctx_desc();
	if (ipidesc) {
		pr_debug("ipi deinit called\n");
		ipidesc->ops->ctx_deinit(ipidesc);
	}

	return 0;
}

EXPORT_SYMBOL(vdsp_runtime_suspend);
#endif

static int sprd_vdsp_init_buffer(struct platform_device *pdev, struct xvp *xvp)
{
	char *name = NULL;
	uint64_t size = 0;
	uint32_t heap_type = 0;
	uint32_t attr = 0;
	struct xvp_buf *buf = NULL;
	int ret = 0;

	//xvp ipc buffer for message
	name = "xvp ipc buffer";
	size = PAGE_SIZE;
	heap_type = SPRD_VDSP_MEM_HEAP_TYPE_UNIFIED;
	attr = SPRD_VDSP_MEM_ATTR_WRITECOMBINE;
	buf = xvp_buf_alloc(xvp, name, size, heap_type, attr);
	if (!buf) {
		pr_err("Error:xvp_buf_alloc faild\n");
		return -1;
	}
	xvp->ipc_buf = buf;
	ret = xvp_buf_kmap(xvp, xvp->ipc_buf);
	if (ret) {
		xvp_buf_free(xvp, xvp->ipc_buf);
		xvp->ipc_buf = NULL;
		pr_err("Error: xvp_buf_kmap failed\n");
		return -EFAULT;
	}
#ifdef FACEID_VDSP_FULL_TEE
	if (unlikely(sprd_faceid_init(xvp) != 0)) {
		pr_err("sprd_alloc_faceid buffer failed\n");
		xvp_buf_free(xvp, buf);
		xvp->ipc_buf = NULL;
		return -EFAULT;
	}
#endif
	return 0;
}

static int sprd_vdsp_free_buffer(struct xvp *xvp)
{
	xvp_buf_kunmap(xvp, xvp->ipc_buf);
	if (xvp_buf_free(xvp, xvp->ipc_buf) < 0) {
		pr_err("Error: xvp_buf_free failed\n");
		return -1;
	}
#ifdef FACEID_VDSP_FULL_TEE
	sprd_faceid_deinit(xvp);
#endif
	return 0;
}

static int sprd_vdsp_parse_soft_dt(struct xvp *xvp, struct platform_device *pdev)
{
	int i, ret = 0;

	ret = device_property_read_u32_array(xvp->dev, "queue-priority", NULL, 0);
	if (ret > 0) {
		xvp->n_queues = ret;
		xvp->queue_priority = devm_kmalloc(&pdev->dev, ret * sizeof(u32), GFP_KERNEL);
		if (xvp->queue_priority == NULL) {
			pr_err("failed to kmalloc queue priority \n");
			return -EFAULT;
		}

		ret = device_property_read_u32_array(xvp->dev, "queue-priority",
			xvp->queue_priority, xvp->n_queues);
		if (ret < 0) {
			pr_err("failed to read queue priority \n");
			return -EFAULT;
		}

		pr_debug("multiqueue (%d) configuration, queue priorities:\n", xvp->n_queues);
		for (i = 0; i < xvp->n_queues; ++i)
			pr_debug("	%d\n", xvp->queue_priority[i]);

	} else {
		xvp->n_queues = 1;
	}

	xvp->queue = devm_kmalloc(&pdev->dev, xvp->n_queues * sizeof(*xvp->queue), GFP_KERNEL);
	xvp->queue_ordered = devm_kmalloc(&pdev->dev, xvp->n_queues * sizeof(*xvp->queue_ordered),
		GFP_KERNEL);
	if (xvp->queue == NULL || xvp->queue_ordered == NULL) {
		pr_err("failed to kmalloc queue ordered \n");
		return -EFAULT;
	}

	for (i = 0; i < xvp->n_queues; ++i) {
		mutex_init(&xvp->queue[i].lock);
		xvp->queue[i].comm = xvp_buf_get_vaddr(xvp->ipc_buf) + XRP_DSP_CMD_STRIDE * i;
		init_completion(&xvp->queue[i].completion);
		if (xvp->queue_priority)
			xvp->queue[i].priority = xvp->queue_priority[i];
		xvp->queue_ordered[i] = xvp->queue + i;
		pr_debug("queue i:%d, comm:%p\n", i, xvp->queue[i].comm);
	}
	sort(xvp->queue_ordered, xvp->n_queues, sizeof(*xvp->queue_ordered),
		compare_queue_priority, NULL);

	ret = device_property_read_string(xvp->dev, "firmware-name", &xvp->firmware_name);
	if (unlikely(ret == -EINVAL || ret == -ENODATA))
		pr_err("no firmware-name property, not loading firmware\n");
	else if (unlikely(ret < 0))
		pr_err("invalid firmware name (%d)\n", ret);
	return ret;
}

static int vdsp_dma_set_mask_and_coherent(struct device *dev)
{
	if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64))) {
		if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(36))) {
			pr_err("Error: failed to set dma mask!\n");
			return -1;
		}
		pr_debug("vdsp: set dma mask as 36bit\n");
	} else {
		pr_debug("vdsp: set dma mask as 64bit\n");
	}
	return 0;
}

static long vdsp_init_common(struct platform_device *pdev,
	enum vdsp_init_flags init_flags,
	const struct xrp_hw_ops *hw_ops, void *hw_arg,
	int (*vdsp_init_bufs) (struct platform_device *pdev, struct xvp *xvp))
{
	int nodeid;
	unsigned int index;
	long ret;
	char nodename[sizeof("vdsp") + 3 * sizeof(int)];
	struct xvp *xvp;

	/*debug fs */
	vdsp_debugfs_init();

	xvp = devm_kzalloc(&pdev->dev, sizeof(*xvp), GFP_KERNEL);
	if (unlikely(!xvp)) {
		ret = -ENOMEM;
		goto err;
	}

	/* dma_mask_set*/
	vdsp_dma_set_mask_and_coherent(&pdev->dev);

	/* iommus init */
	xvp->iommus = devm_kzalloc(&pdev->dev, sizeof(struct sprd_vdsp_iommus), GFP_KERNEL);
	if (xvp->iommus == NULL) {
		pr_err("Error: fail to kzalloc\n");
		ret = -ENOMEM;
		goto err_iommus_devm_kzalloc;
	}
	xvp->iommus->ops = &iommus_ops;
	ret = xvp->iommus->ops->init(xvp->iommus, pdev->dev.of_node, &pdev->dev);
	if (ret) {
		pr_err("Error: xvp iommus init failed\n");
		goto err_iommus_init;
	}
	for (index = 0; index < SPRD_VDSP_IOMMU_MAX; index++) {
		debug_print_iommu_dev(xvp->iommus->iommu_devs[index]);
	}

	/*  mem init */
	ret = sprd_vdsp_mem_xvp_init(xvp);
	if (ret) {
		pr_err("Error: sprd_vdsp_mem_xvp_init failed\n");
		goto err_mem_xvp_init;
	}

	/* iova_reserve_init */
	ret = xvp->iommus->ops->reserve_init(xvp->iommus, iova_reserve_data,
		ARRAY_SIZE(iova_reserve_data));
	if (ret) {
		pr_err("Error: iova_reserve_init failed\n");
		goto err_iova_reserve_init;
	}

	/* other parameters */
	mutex_init(&(xvp->load_lib.libload_mutex));
	mutex_init(&(xvp->dvfs_info.dvfs_lock));
	mutex_init(&xvp_global_lock);
	init_files_know_info(xvp);
	xvp->open_count = 0;
	xvp->dvfs_info.dvfs_init = 0;
	xvp->cur_opentype = 0xffffffff;	/*0 is normal type , 1 is faceid , 0xffffffff is initialized value */
	xvp->dev = &pdev->dev;
	xvp->hw_ops = hw_ops;
	xvp->hw_arg = hw_arg;
	xvp->secmode = false;
	xvp->tee_con = false;
	xvp->firmware_name = "vdsp_firmware.bin";
#ifdef FACEID_VDSP_FULL_TEE
	xvp->irq_status = IRQ_STATUS_REQUESTED;
#endif

	if (init_flags & XRP_INIT_USE_HOST_IRQ)
		xvp->host_irq_mode = true;

	/* need understand */
	platform_set_drvdata(pdev, xvp);

	if (unlikely(vdsp_init_bufs(pdev, xvp))) {
		ret = -1;
		goto err_vdsp_init_bufs;
	}
	if (unlikely(sprd_vdsp_parse_soft_dt(xvp, pdev))) {
		ret = -1;
		goto err_parse_soft_dt;
	}
	if (unlikely(vdsp_log_init(xvp) != 0))
		pr_err("vdsp log init fail.\n");

	/* pm runtime, there need modify later */
#ifdef MYN6
	pm_runtime_set_active(xvp->dev);
	pm_runtime_enable(xvp->dev);
#endif
#ifdef MYL5
	pm_runtime_enable(xvp->dev);
	if (!pm_runtime_enabled(xvp->dev)) {
		ret = vdsp_runtime_resume(xvp->dev);
		if (ret)
			goto err_pm_disable;
	}
#endif

	/* device register */
	nodeid = ida_simple_get(&xvp_nodeid, 0, 0, GFP_KERNEL);
	if (unlikely(nodeid < 0)) {
		ret = nodeid;
		goto err_ida_simple_get;
	}
	xvp->nodeid = nodeid;
	sprintf(nodename, "vdsp%u", nodeid);

	xvp->miscdev = (struct miscdevice) {
		.minor = MISC_DYNAMIC_MINOR,
		.name = devm_kstrdup(&pdev->dev, nodename, GFP_KERNEL),
		.nodename = devm_kstrdup(&pdev->dev, nodename, GFP_KERNEL),
		.fops = &xvp_fops,
	};

	ret = misc_register(&xvp->miscdev);
	if (unlikely(ret < 0))
		goto err_misc_register;

	pr_info("vdsp_init_common end, vdsp init sucessed\n");

	return PTR_ERR(xvp);

err_misc_register:
	pr_err("Error: err_misc_register \n");
	ida_simple_remove(&xvp_nodeid, nodeid);
err_ida_simple_get:
	pr_err("Error: err_ida_simple_get \n");

#ifdef MYL5
err_pm_disable :
	pm_runtime_disable(xvp->dev);
#endif

err_parse_soft_dt:
	pr_err("Error: err_parse_soft_dt \n");
	sprd_vdsp_free_buffer(xvp);

err_vdsp_init_bufs:
	pr_err("Error: err_vdsp_init_bufs \n");

err_iova_reserve_init:
	sprd_vdsp_mem_xvp_release(xvp->mem_dev);

err_mem_xvp_init:
	pr_err("Error: err_mem_xvp_init \n");
	xvp->iommus->ops->release(xvp->iommus);

err_iommus_init:
	pr_err("Error: err_iommus_init \n");
	devm_kfree(&pdev->dev, xvp->iommus);

err_iommus_devm_kzalloc:
	pr_err("Error: err_iommus_devm_kzalloc \n");
	devm_kfree(&pdev->dev, xvp);

err:
	vdsp_debugfs_exit();
	pr_err("Error:ret = %ld\n", ret);
	return ret;
}

long sprd_vdsp_init(struct platform_device *pdev, enum vdsp_init_flags flags,
	const struct xrp_hw_ops *hw_ops, void *hw_arg)
{
	return vdsp_init_common(pdev, flags, hw_ops, hw_arg, sprd_vdsp_init_buffer);
}

EXPORT_SYMBOL(sprd_vdsp_init);

int sprd_vdsp_deinit(struct platform_device *pdev)
{
	struct xvp *xvp = platform_get_drvdata(pdev);

	/* pm runtime*/
	pm_runtime_disable(xvp->dev);

#ifdef MYL5
	if (!pm_runtime_status_suspended(xvp->dev))
		vdsp_runtime_suspend(xvp->dev);
#endif
	misc_deregister(&xvp->miscdev);
	release_firmware(xvp->firmware);
	sprd_vdsp_free_buffer(xvp);
	vdsp_log_deinit(xvp);
	sprd_vdsp_mem_destroy_proc_ctx(xvp->drv_mem_ctx);
	xvp->iommus->ops->reserve_release(xvp->iommus);
	sprd_vdsp_mem_xvp_release(xvp->mem_dev);
	xvp->iommus->ops->release(xvp->iommus);
	ida_simple_remove(&xvp_nodeid, xvp->nodeid);
	/*debug fs */
	vdsp_debugfs_exit();
	return 0;
}

EXPORT_SYMBOL(sprd_vdsp_deinit);

MODULE_AUTHOR("Vision DSP");
MODULE_DESCRIPTION("XRP: Linux device driver for Xtensa Remote Processing");
MODULE_LICENSE("Dual MIT/GPL");
