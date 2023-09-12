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

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/sprd_iommu.h>
#include <linux/sprd_ion.h>
#include <sprd_mm.h>

#include "cam_types.h"
#include "dcam_reg.h"
#include "dcam_fmcu.h"
#include "cam_hw.h"

#define DCAM_FMCU_STOP_TIMEOUT             2000

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "DCAM_FMCU %d: %d %s:" \
	fmt, current->pid, __LINE__, __func__

static int dcamfmcu_cmd_push(struct dcam_fmcu_ctx_desc *fmcu_ctx,
				uint32_t addr, uint32_t cmd)
{
	int ret = 0;
	uint32_t *ptr;

	if (!fmcu_ctx) {
		pr_err("fail to get fmcu_ctx pointer\n");
		return -EFAULT;
	}
	if (fmcu_ctx->cmdq_pos[fmcu_ctx->cur_buf_id]
		> (fmcu_ctx->cmdq_size / sizeof(uint32_t))) {
		pr_err("fail to get fmcu%d cmdq, overflow.\n", fmcu_ctx->fid);
		return -EFAULT;
	}

	ptr = fmcu_ctx->cmd_buf[fmcu_ctx->cur_buf_id]
				+ fmcu_ctx->cmdq_pos[fmcu_ctx->cur_buf_id];
	*ptr++ = cmd;
	*ptr++ = addr;
	fmcu_ctx->cmdq_pos[fmcu_ctx->cur_buf_id] += 2;

	return ret;
}

static int dcam_fmcu_cmd_agined(struct dcam_fmcu_ctx_desc *fmcu_ctx)
{
	int cmd_num = 0;
	uint32_t addr = 0, cmd = 0;

	if (!fmcu_ctx) {
	pr_err("fail to get fmcu_ctx pointer\n");
	return -EFAULT;
	}

	cmd_num = (int)fmcu_ctx->cmdq_pos[fmcu_ctx->cur_buf_id] / 2;

	if (cmd_num % 2) {
		addr = DCAM_GET_REG(fmcu_ctx->hw_ctx_id, DCAM_IP_REVISION);
		cmd = 0;
		dcamfmcu_cmd_push(fmcu_ctx, addr, cmd);
	}

	return 0;
}
static int dcamfmcu_cmd_ready(struct dcam_fmcu_ctx_desc *fmcu_ctx)
{
	int ret = 0;
	int cmd_num;
	struct dcam_hw_fmcu_cmd cmdarg;

	if (!fmcu_ctx) {
		pr_err("fail to get fmcu_ctx pointer\n");
		return -EFAULT;
	}
	pr_debug("set fmcu cmd ready%d\n", fmcu_ctx->fid);

	if (fmcu_ctx->cmdq_pos[fmcu_ctx->cur_buf_id]
		> (fmcu_ctx->cmdq_size / sizeof(uint32_t))) {
		pr_err("fail to get fmcu%d cmdq, overflow.\n", fmcu_ctx->fid);
		return -EFAULT;
	}

#if 0
	{
		unsigned int i = 0;
		unsigned long addr =
			(unsigned long)fmcu_ctx->cmd_buf[fmcu_ctx->cur_buf_id];
		pr_info("fmcu %d  cmd num %d\n",
				(int)fmcu_ctx->fid,  cmd_num);

		for (i = 0; i <= cmd_num; i += 2) {
			pr_info("a:0x%08x c: 0x%08x | a:0x%08x c: 0x%08x\n",
				*(uint32_t *)(addr + 4),
				*(uint32_t *)(addr),
				*(uint32_t *)(addr + 12),
				*(uint32_t *)(addr + 8));
			addr += 16;
		}
	}
#endif
	dcam_fmcu_cmd_agined(fmcu_ctx);
	cmd_num = (int) fmcu_ctx->cmdq_pos[fmcu_ctx->cur_buf_id] / 2;
	cmdarg.hw_addr = fmcu_ctx->hw_addr[fmcu_ctx->cur_buf_id];
	cmdarg.cmd_num = cmd_num;
	fmcu_ctx->hw->dcam_ioctl(fmcu_ctx->hw, DCAM_HW_CFG_FMCU_CMD, &cmdarg);

	pr_debug("fmcu%d start done, cmdq len %d\n",
		fmcu_ctx->fid,
		(uint32_t)fmcu_ctx->cmdq_pos[fmcu_ctx->cur_buf_id] * 4);

	fmcu_ctx->cur_buf_id = !(fmcu_ctx->cur_buf_id);

	return ret;
}

static int dcamfmcu_start(struct dcam_fmcu_ctx_desc *fmcu_ctx)
{
	int ret = 0;
	int cmd_num;
	struct dcam_hw_fmcu_start startarg;

	if (!fmcu_ctx) {
		pr_err("fail to get fmcu_ctx pointer\n");
		return -EFAULT;
	}
	pr_debug("start fmcu%d\n", fmcu_ctx->fid);

	if (fmcu_ctx->cmdq_pos[fmcu_ctx->cur_buf_id]
		> (fmcu_ctx->cmdq_size / sizeof(uint32_t))) {
		pr_err("fail to get fmcu%d cmdq, overflow.\n", fmcu_ctx->fid);
		return -EFAULT;
	}


/*
 * FLUSH_DCACHE(fmcu_ctx->cmd_buf[fmcu_ctx->cur_buf_id],
 * fmcu_ctx->cmdq_pos[fmcu_ctx->cur_buf_id] * sizeof(uint32_t));
 * sprd_ion_flush_dcache_area_wrapper(fmcu_ctx->cmd_buf[fmcu_ctx->cur_buf_id],
 * fmcu_ctx->cmdq_pos[fmcu_ctx->cur_buf_id] * sizeof(uint32_t));
 */

	dcam_fmcu_cmd_agined(fmcu_ctx);
	cmd_num = (int) fmcu_ctx->cmdq_pos[fmcu_ctx->cur_buf_id] / 2;

	{
		unsigned int i = 0;
		unsigned long addr =
			(unsigned long)fmcu_ctx->cmd_buf[fmcu_ctx->cur_buf_id];
		pr_debug("fmcu %d  cmd num %d\n",
				(int)fmcu_ctx->fid,  cmd_num);

		for (i = 0; i <= cmd_num; i += 2) {
			pr_debug(" a:0x%08x c: 0x%08x | a:0x%08x c: 0x%08x\n",
				*(uint32_t *)(addr + 4),
				*(uint32_t *)(addr),
				*(uint32_t *)(addr + 12),
				*(uint32_t *)(addr + 8));
			addr += 16;
		}
	}

	startarg.hw_addr = fmcu_ctx->hw_addr[fmcu_ctx->cur_buf_id];
	startarg.cmd_num = cmd_num;
	fmcu_ctx->hw->dcam_ioctl(fmcu_ctx->hw, DCAM_HW_CFG_FMCU_START, &startarg);

	pr_debug("fmcu%d start done, cmdq len %d\n",
		fmcu_ctx->fid,
		(uint32_t)fmcu_ctx->cmdq_pos[fmcu_ctx->cur_buf_id] * 4);

	fmcu_ctx->cur_buf_id = !(fmcu_ctx->cur_buf_id);

	return ret;
}

static int dcamfmcu_ctx_reset(struct dcam_fmcu_ctx_desc *fmcu_ctx)
{
	int ret = 0;

	if (!fmcu_ctx) {
		pr_err("fail to get fmcu_ctx pointer\n");
		return -EFAULT;
	}

	pr_debug("Enter\n");

	fmcu_ctx->cmdq_pos[fmcu_ctx->cur_buf_id] = 0;
	memset(fmcu_ctx->cmd_buf[fmcu_ctx->cur_buf_id], 0, fmcu_ctx->cmdq_size);

	pr_debug("Done\n");
	return ret;
}

static int dcamfmcu_buf_map(void *handle)
{
	int ret = 0, i = 0;
	struct camera_buf *ion_buf = NULL;
	struct dcam_fmcu_ctx_desc *fmcu_ctx = NULL;

	if (!handle) {
		pr_err("fail to get fmcu_ctx pointer\n");
		return -EFAULT;
	}

	fmcu_ctx = (struct dcam_fmcu_ctx_desc *)handle;

	for (i = 0; i < DCAM_FMCU_BUF_MAX; i++) {
		ion_buf = &fmcu_ctx->ion_pool[i];

		ret = cam_buf_iommu_map(ion_buf, CAM_IOMMUDEV_DCAM);
		if (ret) {
			pr_err("fail to map fmcu buffer\n");
			ret = -EFAULT;
			goto err_hwmap_fmcu;
		}

		fmcu_ctx->hw_addr[i] = ion_buf->iova[0];
		fmcu_ctx->cmdq_pos[i] = 0;

		pr_info("fmcu%d cmd buf hw_addr:0x%lx, sw_addr:%p, size:%zd\n",
			i, fmcu_ctx->hw_addr[i], fmcu_ctx->cmd_buf[i], ion_buf->size[0]);
	}

	return 0;

err_hwmap_fmcu:
	for (i = 0; i < DCAM_FMCU_BUF_MAX; i++) {
		ion_buf = &fmcu_ctx->ion_pool[i];
		if (ion_buf)
			cam_buf_iommu_unmap(ion_buf);
	}
	pr_err("fail to map fmcu%d.\n", fmcu_ctx->fid);
	return ret;
}

static int dcamfmcu_buf_unmap(void *handle)
{
	int ret = 0, i = 0;
	struct camera_buf *ion_buf = NULL;
	struct dcam_fmcu_ctx_desc *fmcu_ctx = NULL;

	if (!handle) {
		pr_err("fail to get fmcu_ctx pointer\n");
		return -EFAULT;
	}

	fmcu_ctx = (struct dcam_fmcu_ctx_desc *)handle;
	for (i = 0; i < DCAM_FMCU_BUF_MAX; i++) {
		ion_buf = &fmcu_ctx->ion_pool[i];
		cam_buf_iommu_unmap(ion_buf);
	}
	pr_debug("fmcu buf unmap done\n");
	return ret;
}

static int dcamfmcu_ctx_init(struct dcam_fmcu_ctx_desc *fmcu_ctx)
{
	int ret = 0;
	int i;
	int iommu_enable = 0;
	struct camera_buf *ion_buf = NULL;

	if (!fmcu_ctx) {
		pr_err("fail to get fmcu_ctx pointer\n");
		return -EFAULT;
	}
	pr_debug("Enter\n");

	fmcu_ctx->cmdq_size = DCAM_FMCU_CMDQ_SIZE;
	fmcu_ctx->lock = __SPIN_LOCK_UNLOCKED(&fmcu_ctx->lock);

	/*alloc cmd queue buffer*/
	for (i = 0; i < DCAM_FMCU_BUF_MAX; i++) {
		ion_buf = &fmcu_ctx->ion_pool[i];
		memset(ion_buf, 0, sizeof(fmcu_ctx->ion_pool[i]));
		sprintf(ion_buf->name, "dcam_fmcu_ctx%d", i);

		if (cam_buf_iommu_status_get(CAM_IOMMUDEV_DCAM) == 0) {
			pr_debug("dcam iommu enable\n");
			iommu_enable = 1;
		} else {
			pr_debug("dcam iommu disable\n");
			iommu_enable = 0;
		}
		ret = cam_buf_alloc(ion_buf, fmcu_ctx->cmdq_size, iommu_enable);
		if (ret) {
			pr_err("fail to get fmcu buffer\n");
			ret = -EFAULT;
			goto err_alloc_fmcu;
		}
		ret = cam_buf_kmap(ion_buf);
		if (ret) {
			pr_err("fail to kmap fmcu buffer\n");
			ret = -EFAULT;
			goto err_kmap_fmcu;
		}

		fmcu_ctx->cmd_buf[i] = (uint32_t *)ion_buf->addr_k[0];
	}

	return 0;

err_kmap_fmcu:
	for (i = 0; i < DCAM_FMCU_BUF_MAX; i++) {
		ion_buf = &fmcu_ctx->ion_pool[i];
		if (ion_buf)
			cam_buf_kunmap(ion_buf);
	}

err_alloc_fmcu:
	for (i = 0; i < DCAM_FMCU_BUF_MAX; i++) {
		ion_buf = &fmcu_ctx->ion_pool[i];
		if (ion_buf)
			cam_buf_free(ion_buf);
	}
	pr_err("fail to init fmcu%d.\n", fmcu_ctx->fid);
	return ret;
}

static int dcamfmcu_ctx_deinit(struct dcam_fmcu_ctx_desc *fmcu_ctx)
{
	int ret = 0;
	int i;
	struct camera_buf *ion_buf = NULL;

	if (!fmcu_ctx) {
		pr_err("fail to get fmcu_ctx pointer\n");
		return -EFAULT;
	}

	pr_debug("Enter\n");
	for (i = 0; i < DCAM_FMCU_BUF_MAX; i++) {
		ion_buf = &fmcu_ctx->ion_pool[i];
		cam_buf_kunmap(ion_buf);
		cam_buf_free(ion_buf);
	}

	pr_debug("Done\n");
	return ret;
}

static struct dcam_fmcu_ops fmcu_ops = {
	.ctx_init = dcamfmcu_ctx_init,
	.ctx_deinit = dcamfmcu_ctx_deinit,
	.ctx_reset = dcamfmcu_ctx_reset,
	.push_cmdq = dcamfmcu_cmd_push,
	.hw_start = dcamfmcu_start,
	.cmd_ready = dcamfmcu_cmd_ready,
	.buf_map = dcamfmcu_buf_map,
	.buf_unmap = dcamfmcu_buf_unmap,
};

static struct dcam_fmcu_ctx_desc s_fmcu_desc[DCAM_FMCU_NUM] = {
	{
		.fid = DCAM_FMCU_0,
		.ops = &fmcu_ops,
		.cur_buf_id = DCAM_FMCU_PING,
	},
};

struct dcam_fmcu_ctx_desc *dcam_fmcu_ctx_desc_get(void *arg, uint32_t hw_idx)
{
	int i = 0;
	struct dcam_fmcu_ctx_desc *fmcu = NULL;
	struct cam_hw_info *hw = NULL;

	if (!arg) {
		pr_err("fail to get valid arg\n");
		return NULL;
	}

	hw = (struct cam_hw_info *)arg;
	for (i = 0; i < DCAM_FMCU_NUM; i++) {
		if (atomic_inc_return(&s_fmcu_desc[i].user_cnt) == 1) {
			fmcu = &s_fmcu_desc[i];
			fmcu->hw_ctx_id = hw_idx;
			fmcu->hw = hw;
			pr_info("fmcu %d , %px\n", fmcu->fid, fmcu);
			break;
		}
		atomic_dec(&s_fmcu_desc[i].user_cnt);
	}

	return fmcu;
}

int dcam_fmcu_ctx_desc_put(struct dcam_fmcu_ctx_desc *fmcu)
{
	int i;
	struct dcam_fmcu_enable fmcu_enable =  {0};
	struct cam_hw_info *hw = NULL;

	if (!fmcu) {
		pr_warn("warning: fmcu already put\n");
		return 0;
	}

	pr_info("fmcu %d. %p\n", fmcu->fid, fmcu);
	for (i = 0; i < DCAM_FMCU_NUM; i++) {
		if (fmcu == &s_fmcu_desc[i]) {
			atomic_dec(&s_fmcu_desc[i].user_cnt);
			hw = fmcu->hw;
			fmcu_enable.enable = 0;
			fmcu_enable.idx = fmcu->hw_ctx_id;
			hw->dcam_ioctl(hw, DCAM_HW_FMCU_EBABLE, &fmcu_enable);
			fmcu->hw_ctx_id = DCAM_HW_CONTEXT_MAX;
			fmcu->hw = NULL;
			break;
		}
	}

	if (fmcu->hw != NULL)
		pr_err("fail to match original ptr %p\n", fmcu);

	return 0;
}
