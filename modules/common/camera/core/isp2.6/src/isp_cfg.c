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
#include <sprd_mm.h>
#include <linux/sprd_iommu.h>
#include <linux/sprd_ion.h>
#include <linux/seq_file.h>

#include "isp_reg.h"
#include "isp_core.h"
#include "isp_cfg.h"
#include "cam_debugger.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_CFG: %d: %d %s:" \
	fmt, current->pid, __LINE__, __func__

static DEFINE_MUTEX(buf_mutex);

struct isp_cfg_map_sector {
	uint32_t offset;
	uint32_t size;
};

/* s_map_sec_cnt/s_map_sectors only configure once
 * due to ISP_CFG_MAP[] is static and fixed
 */
static uint32_t s_map_sec_cnt;
static struct isp_cfg_map_sector s_map_sectors[ISP_CFG_MAP_MAX];
unsigned long isp_cfg_ctx_addr[ISP_CONTEXT_SW_NUM] = { 0 };

static void ispcfg_cctx_page_hw_buf_aligned(struct isp_cfg_ctx_desc *cfg_ctx,
		void **sw_addr, unsigned long *hw_addr)
{
	void *kaddr = NULL;
	unsigned long ofst_align = 0;
	unsigned long iova = 0, aligned_iova = 0;
	struct camera_buf *ion_buf = &cfg_ctx->ion_pool;

	kaddr = (void *)ion_buf->addr_k[0];
	iova = ion_buf->iova[0];

	if (!IS_ERR_OR_NULL(kaddr) && iova) {
		if (IS_ALIGNED(iova, ALIGN_PADDING_SIZE)) {
			*hw_addr = iova;
			*sw_addr = kaddr;
		} else {
			aligned_iova = ALIGN(iova, ALIGN_PADDING_SIZE);
			*hw_addr = aligned_iova;

			ofst_align = aligned_iova - iova;
			*sw_addr = kaddr + ofst_align;
		}
		pr_debug("aligned sw: %p, hw: 0x%lx, ofs: 0x%lx",
			*sw_addr, *hw_addr, ofst_align);
	}
}

static void ispcfg_cctx_page_buf_addr_deinit(struct isp_cfg_ctx_desc *cfg_ctx)
{
	struct isp_cfg_buf *cfg_buf = NULL;
	int c_id = 0, bid = 0;

	for (c_id = 0; c_id < ISP_CONTEXT_SW_NUM; c_id++) {
		cfg_buf = &cfg_ctx->cfg_buf[c_id];
		for (bid = 0; bid < CFG_BUF_NUM; bid++) {
			cfg_buf->reg_buf[bid].sw_addr = NULL;
			cfg_buf->reg_buf[bid].hw_addr = 0UL;
		}
	}
}

static void ispcfg_cctx_page_hw_buf_addr_init(
		struct isp_cfg_ctx_desc *cfg_ctx,
		void *sw_addr, unsigned long hw_addr)
{
	unsigned long offset = 0;
	struct isp_cfg_buf *cfg_buf = NULL;
	int c_id = 0;

	for (c_id = 0; c_id < ISP_CONTEXT_HW_NUM; c_id++) {
		offset = c_id * ISP_CFG_BUF_SIZE;
		cfg_buf = &cfg_ctx->cfg_buf[c_id];
		cfg_buf->reg_buf[CFG_BUF_HW].sw_addr = sw_addr + offset;
		cfg_buf->reg_buf[CFG_BUF_HW].hw_addr = hw_addr + offset;
		pr_debug("isp ctx_hw %d: sw=%p, hw:0x%lx\n",
			c_id, cfg_buf->reg_buf[CFG_BUF_HW].sw_addr,
			cfg_buf->reg_buf[CFG_BUF_HW].hw_addr);
	}
}

static void ispcfg_cctx_page_sw_buf_addr_init(
		struct isp_cfg_ctx_desc *cfg_ctx, void *sw_addr)
{
	unsigned long offset = 0;
	struct isp_cfg_buf *cfg_buf = NULL;
	int c_id = 0;

	for (c_id = 0; c_id < ISP_CONTEXT_SW_NUM; c_id++) {
		offset = c_id * ISP_CFG_BUF_SIZE;
		cfg_buf = &cfg_ctx->cfg_buf[c_id];
		cfg_buf->reg_buf[CFG_BUF_SW].sw_addr = sw_addr + offset;
		cfg_buf->reg_buf[CFG_BUF_SW].hw_addr = 0UL;
		pr_debug("isp ctx_sw %d: sw=%p, hw:0x%lx\n",
			c_id, cfg_buf->reg_buf[CFG_BUF_SW].sw_addr,
			cfg_buf->reg_buf[CFG_BUF_SW].hw_addr);
	}
}

static void ispcfg_cctx_regbuf_addr_init(struct isp_cfg_ctx_desc *cfg_ctx)
{
	struct isp_cfg_buf *cfg_buf = NULL;
	struct regfile_buf_info *cur_regbuf_p = NULL;
	int c_id = 0;

	/*
	 * Init context reg buf for each isp context.
	 * Each context has two buf, shadow and work.
	 * Using shadow buf as initial buf.
	 * @isp_cfg_ctx_addr will be used when using ISP_BASE_ADDR.
	 */
	for (c_id = 0; c_id < ISP_CONTEXT_SW_NUM; c_id++) {
		cfg_buf = &cfg_ctx->cfg_buf[c_id];
		cfg_buf->cur_buf_id = CFG_BUF_SW;
		cur_regbuf_p = &cfg_buf->reg_buf[CFG_BUF_SW];
		isp_cfg_ctx_addr[c_id] = (unsigned long)cur_regbuf_p->sw_addr;
		pr_debug("init cctx_buf[%d] sw=%p, hw=0x%lx\n",
			c_id, cur_regbuf_p->sw_addr,
			cur_regbuf_p->hw_addr);
	}

}

static void ispcfg_cctx_regbuf_addr_deinit(struct isp_cfg_ctx_desc *cfg_ctx)
{
	int c_id = 0;

	mutex_lock(&buf_mutex);
	for (c_id = 0; c_id < ISP_CONTEXT_SW_NUM; c_id++)
		isp_cfg_ctx_addr[c_id] = 0UL;
	mutex_unlock(&buf_mutex);
}

static int ispcfg_cctx_buf_init(struct isp_cfg_ctx_desc *cfg_ctx)
{
	int ret = 0;
	unsigned int iommu_enable = 0;
	size_t size = 0;
	void *sw_addr = NULL;
	unsigned long hw_addr = 0;
	struct camera_buf *ion_buf = NULL;
	struct camera_buf *ion_buf_cached = NULL;

	/*alloc cfg context buffer*/
	ion_buf = &cfg_ctx->ion_pool;
	memset(ion_buf, 0, sizeof(cfg_ctx->ion_pool));
	sprintf(ion_buf->name, "isp_cfg_ctx");

	if (cam_buf_iommu_status_get(CAM_IOMMUDEV_ISP) == 0) {
		pr_debug("isp iommu enable\n");
		iommu_enable = 1;
	} else {
		pr_debug("isp iommu disable\n");
		iommu_enable = 0;
	}
	size = ISP_CFG_BUF_SIZE_HW_PADDING;
	ret = cam_buf_alloc(ion_buf, size, iommu_enable);
	if (ret) {
		pr_err("fail to get cfg buffer\n");
		ret = -EFAULT;
		goto err_alloc_cfg;
	}

	ret = cam_buf_kmap(ion_buf);
	if (ret) {
		pr_err("fail to kmap cfg buffer\n");
		ret = -EFAULT;
		goto err_kmap_cfg;
	}

	ret = cam_buf_iommu_map(ion_buf, CAM_IOMMUDEV_ISP);
	if (ret) {
		pr_err("fail to map cfg buffer\n");
		ret = -EFAULT;
		goto err_hwmap_cfg;
	}

	/*alloc cfg sw context buffer*/
	ion_buf_cached = &cfg_ctx->ion_pool_cached;
	memset(ion_buf_cached, 0, sizeof(cfg_ctx->ion_pool_cached));
	sprintf(ion_buf_cached->name, "isp_cfg_swctx");

	size = ISP_CFG_BUF_SIZE_SW_ALL;
	ret = cam_buf_alloc(ion_buf_cached, size, iommu_enable);
	if (ret) {
		pr_err("fail to get cfg buffer\n");
		ret = -EFAULT;
		goto err_alloc_cfg1;
	}

	ret = cam_buf_kmap(ion_buf_cached);
	if (ret) {
		pr_err("fail to kmap cfg buffer\n");
		ret = -EFAULT;
		goto err_kmap_cfg1;
	}

	ispcfg_cctx_page_hw_buf_aligned(cfg_ctx, &sw_addr, &hw_addr);
	ispcfg_cctx_page_hw_buf_addr_init(cfg_ctx, sw_addr, hw_addr);
	ispcfg_cctx_page_sw_buf_addr_init(cfg_ctx, (void *)ion_buf_cached->addr_k[0]);
	ispcfg_cctx_regbuf_addr_init(cfg_ctx);

	pr_debug("cmd sw: %p, hw: 0x%lx, size:0x%x\n",
		sw_addr, hw_addr, (int)ion_buf->size[0]);
	pr_debug("cmd1 sw: 0x%lx, size:0x%x\n",
		ion_buf_cached->addr_k[0], (int)ion_buf_cached->size[0]);

	return 0;

err_kmap_cfg1:
	cam_buf_free(ion_buf_cached);
err_alloc_cfg1:
	cam_buf_iommu_unmap(ion_buf);
err_hwmap_cfg:
	cam_buf_kunmap(ion_buf);
err_kmap_cfg:
	cam_buf_free(ion_buf);
err_alloc_cfg:
	return ret;
}

static int ispcfg_cctx_buf_deinit(struct isp_cfg_ctx_desc *cfg_ctx)
{
	struct camera_buf *ion_buf = NULL;

	ion_buf = &cfg_ctx->ion_pool;

	ispcfg_cctx_regbuf_addr_deinit(cfg_ctx);
	ispcfg_cctx_page_buf_addr_deinit(cfg_ctx);

	cam_buf_iommu_unmap(ion_buf);
	cam_buf_kunmap(ion_buf);
	cam_buf_free(ion_buf);

	ion_buf = &cfg_ctx->ion_pool_cached;
	cam_buf_kunmap(ion_buf);
	cam_buf_free(ion_buf);

	return 0;
}

/*  Interface */
static int ispcfg_map_init(struct isp_cfg_ctx_desc *cfg_ctx)
{
	uint32_t i = 0;
	uint32_t cfg_map_size = 0;
	uint32_t *cfg_map = NULL;
	struct isp_hw_cfg_map maparg;
	struct isp_dev_cfg_info s_cfg_settings = {
		0, 1, 1, 0, NULL,
		0, 1, 1, 1, 1,
		0, 0, 0, 0,
		0, 0, 0
	};

	pr_debug("enter.");
	cfg_ctx->hw->isp_ioctl(cfg_ctx->hw, ISP_HW_CFG_CFG_MAP_INFO_GET, &s_cfg_settings);
	cfg_map_size = s_cfg_settings.num_of_mod;
	cfg_map = s_cfg_settings.isp_cfg_map;

	/* only config once after system running */
	if (s_map_sec_cnt == 0) {
		uint32_t item = 0;
		struct isp_cfg_map_sector *cur = &s_map_sectors[0];

		if (cfg_map_size > ISP_CFG_MAP_MAX) {
			pr_warn("warning: cfg map array is too large %d\n", cfg_map_size);
			s_map_sec_cnt = 0;
			goto setting;
		}

		pr_debug("map sections %d\n", cfg_map_size);
		s_map_sec_cnt = cfg_map_size;
		for (i = 0; i < cfg_map_size; i++) {
			item = cfg_map[i];
			cur->offset = item & 0x3ffff;
			cur->size = ((item >> 18) & 0x3fff) * 4;
			pr_debug("No.%d. (0x%08x, %d)\n", i, cur->offset, cur->size);
			cur++;
		}
	}

setting:

	maparg.map_cnt = cfg_ctx->map_cnt;
	maparg.s_cfg_settings = &s_cfg_settings;
	cfg_ctx->hw->isp_ioctl(cfg_ctx->hw, ISP_HW_CFG_MAP_INIT, &maparg);

	return 0;
}

static int ispcfg_block_config(
		struct isp_cfg_ctx_desc *cfg_ctx,
		enum isp_context_id sw_ctx_id,
		enum isp_context_hw_id hw_ctx_id,
		uint32_t fmcu_enable)
{
	int ret = 0;
	void *shadow_buf_vaddr = NULL;
	void *work_buf_vaddr = NULL;
	unsigned long hw_addr = 0;
	struct isp_cfg_buf *cfg_buf_p = NULL;
	struct isp_hw_cfg_info cfg_info = { 0 };

	if (!cfg_ctx) {
		pr_err("fail to get cfg_ctx pointer\n");
		return -EFAULT;
	}
	pr_debug("cfg isp sw ctx %d hw ctx %d\n", sw_ctx_id, hw_ctx_id);

	shadow_buf_vaddr = (void *)isp_cfg_ctx_addr[sw_ctx_id];

	if (hw_ctx_id < ISP_CONTEXT_HW_NUM) {
		cfg_buf_p = &cfg_ctx->cfg_buf[hw_ctx_id];
		hw_addr = cfg_buf_p->reg_buf[CFG_BUF_HW].hw_addr;
		work_buf_vaddr = cfg_buf_p->reg_buf[CFG_BUF_HW].sw_addr;

		pr_debug("ctx %d cmd buf %p, %p\n",
			hw_ctx_id, work_buf_vaddr, shadow_buf_vaddr);
		if (s_map_sec_cnt > 0)
			goto copy_sec;

		pr_debug("copy whole cfg buffer\n");
		memcpy(work_buf_vaddr, shadow_buf_vaddr, ISP_REG_SIZE);
	} else {
		pr_err("fail to get valid isp hw ctx_id %d\n", hw_ctx_id);
		return -EINVAL;
	}

copy_sec:
	if (s_map_sec_cnt > 0) {
		uint32_t i;
		unsigned long src = (unsigned long)shadow_buf_vaddr;
		unsigned long dst = (unsigned long)work_buf_vaddr;
		struct isp_cfg_map_sector *cur = &s_map_sectors[0];

		pr_debug("copy %d sections for cfg buffer\n", s_map_sec_cnt);
		for (i = 0; i < s_map_sec_cnt; i++) {
			memcpy((void *)(dst + cur->offset), (void *)(src + cur->offset), cur->size);
			cur++;
		}
	}

	/* FLUSH_DCACHE(work_buf_vaddr, ISP_REG_SIZE); */
	/* sprd_ion_flush_dcache_area_wrapper(work_buf_vaddr, ISP_REG_SIZE); */

	pr_debug("shadow: 0x%p, work: 0x%p, hw_addr: 0x%lx\n",
		shadow_buf_vaddr, work_buf_vaddr, hw_addr);

	cfg_info.fmcu_enable = fmcu_enable;
	cfg_info.hw_ctx_id = hw_ctx_id;
	cfg_info.hw_addr = hw_addr;
	cfg_ctx->hw->isp_ioctl(cfg_ctx->hw, ISP_HW_CFG_CMD_READY, &cfg_info);

	pr_debug("Done\n");
	return ret;
}

static int ispcfg_ctx_init(struct isp_cfg_ctx_desc *cfg_ctx)
{
	int i = 0, ret = 0;

	if (!cfg_ctx) {
		pr_err("fail to get cfg_ctx pointer\n");
		return -EFAULT;
	}
	pr_debug("Enter\n");

	if (atomic_inc_return(&cfg_ctx->user_cnt) > 1) {
		pr_info("already done.\n");
		goto exit;
	}

	atomic_set(&cfg_ctx->map_cnt, 0);

	ret = ispcfg_cctx_buf_init(cfg_ctx);
	if (ret) {
		pr_err("fail to init isp cfg ctx buffer.\n");
		return -EFAULT;
	}

	for (i = 0; i < ISP_CONTEXT_SW_NUM; i++)
		isp_cfg_poll_addr[i] = &isp_cfg_ctx_addr[i];

exit:
	pr_info("cfg ctx init done\n");

	return ret;
}

static int ispcfg_ctx_deinit(struct isp_cfg_ctx_desc *cfg_ctx)
{
	int ret = 0;

	if (!cfg_ctx) {
		pr_err("fail to get cfg_ctx pointer\n");
		return -EFAULT;
	}

	pr_debug("Enter\n");
	if (atomic_dec_return(&cfg_ctx->user_cnt) > 0) {
		pr_info("isp cfg still have user.\n");
		goto exit;
	}

	atomic_set(&cfg_ctx->map_cnt, 0);
	ret = ispcfg_cctx_buf_deinit(cfg_ctx);
	if (ret)
		pr_err("fail to deinit isp cfg ctx buffer.\n");
exit:
	pr_info("Done\n");

	return ret;
}

static int ispcfg_ctx_buf_reset(
		struct isp_cfg_ctx_desc *cfg_ctx,
		enum isp_context_id ctx_id)
{
	void *shadow_buf_vaddr = NULL;

	if (!cfg_ctx) {
		pr_err("fail to get cfg_ctx pointer\n");
		return -EFAULT;
	}
	pr_debug("Enter\n");

	shadow_buf_vaddr = (void *)isp_cfg_ctx_addr[ctx_id];
	if (!IS_ERR_OR_NULL(shadow_buf_vaddr)) {
		memset(shadow_buf_vaddr, 0x0, ISP_REG_SIZE);
		pr_debug("ctx %d reset shadow page buf\n", ctx_id);
	}

	pr_debug("Done\n");

	return 0;
}

struct isp_cfg_ops cfg_ops = {
	.ctx_init = ispcfg_ctx_init,
	.ctx_deinit = ispcfg_ctx_deinit,
	.ctx_reset = ispcfg_ctx_buf_reset,
	.hw_init = ispcfg_map_init,
	.hw_cfg = ispcfg_block_config,
};

struct isp_cfg_ctx_desc s_ctx_desc = {
	.ops = &cfg_ops,
};

struct isp_cfg_ctx_desc *isp_cfg_ctx_desc_get()
{
	return &s_ctx_desc;
}

int isp_cfg_ctx_desc_put(struct isp_cfg_ctx_desc *param)
{
	if (&s_ctx_desc == param)
		return 0;
	pr_err("fail to match param %p, %p\n",
		param, &s_ctx_desc);
	return -EINVAL;
}

int isp_cfg_ctx_reg_buf_debug_show(void *param)
{
	struct seq_file *s = (struct seq_file *)param;
	struct cam_debug_bypass *debug_bypass = NULL;
	struct cam_hw_info *hw = NULL;
	uint32_t ctx_id = 0;
	uint32_t i, item, start, count;
	uint32_t addr;
	unsigned long datap;
	uint32_t cfg_map_size = 0;
	uint32_t *cfg_map = NULL;
	struct isp_dev_cfg_info s_cfg_settings = {
		0, 1, 1, 0, NULL,
		0, 1, 1, 1, 1,
		0, 0, 0, 0,
		0, 0, 0
	};

	debug_bypass = (struct cam_debug_bypass *)s->private;
	ctx_id = debug_bypass->idx;
	hw = debug_bypass->hw;

	mutex_lock(&buf_mutex);
	datap = isp_cfg_ctx_addr[ctx_id];
	hw->isp_ioctl(hw, ISP_HW_CFG_CFG_MAP_INFO_GET, &s_cfg_settings);
	cfg_map_size = s_cfg_settings.num_of_mod;
	cfg_map = s_cfg_settings.isp_cfg_map;
	if (datap == 0UL) {
		mutex_unlock(&buf_mutex);
		seq_printf(s, "----ctx %d cfg buf freed.---\n", ctx_id);
		return 0;
	}

	seq_printf(s, "dump regsigters buf of ISP context %d - kaddr: 0x%lx\n", ctx_id, datap);

	for (i = 0; i < cfg_map_size; i++) {
		seq_puts(s, "---------------\n");
		item = cfg_map[i];
		start = item & 0x3ffff;
		count = (item >> 18) & 0x3ffff;
		for (addr = start; addr < (start + count); addr += 4)
			seq_printf(s, "%04x:  %08x\n", addr, *(uint32_t *)(datap + addr));
	}
	mutex_unlock(&buf_mutex);
	seq_puts(s, "------------------------\n");
	return 0;
}
