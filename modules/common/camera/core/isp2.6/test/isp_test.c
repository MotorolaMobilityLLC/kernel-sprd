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

#include <linux/delay.h>
#include "sprd_mm.h"
#include "dcam_hw_adpt.h"
#include "isp_hw_adpt.h"
#include "cam_buf.h"
#include "cam_test.h"

#include "isp_reg.h"
#include "isp_int.h"
#include "isp_cfg.h"
#include "sprd_cam_test.h"
#include "isp_core.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CAMT: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define CHECK_NULL(p)	{		\
	if ((p) == NULL) {		\
		pr_err("null param\n");	\
		return -EINVAL;		\
	}				\
}

#define CAMT_CFG_SIZE			0x40000UL
#define CAMT_ISP_CFG_BUF_SIZE	\
	(CAMT_CFG_SIZE * ISP_HW_MAX_COUNT + CAMT_CFG_SIZE)

unsigned long camt_cfg_addr_reg[ISP_HW_MAX_COUNT] = {
	ISP_CFG_PRE0_CMD_ADDR,
	ISP_CFG_CAP0_CMD_ADDR,
	ISP_CFG_PRE1_CMD_ADDR,
	ISP_CFG_CAP1_CMD_ADDR
};

struct camt_isp_hw_context {
	int used;

	int isp_hw_id;
	int fmcu_used;

	void *cfg_sw_addr;
	unsigned long cfg_hw_addr;

	struct img_size in_size;
	struct img_trim in_trim;
	struct img_size out_size;

	struct img_endian endian;

	struct camera_buf in_buf;
	struct camera_buf out_buf;
};

struct ispt_context {
	atomic_t cfg_map_cnt;
	uint32_t isp_irq[ISP_LOGICAL_COUNT];

	struct camt_isp_hw_context isp_cxt[ISP_HW_MAX_COUNT];

	struct camera_buf in_buf;
	struct camera_buf out_buf;
	struct camera_buf cfg_buf;

	struct cam_hw_info *hw;
	struct isp_hw_fetch_info fetch_info;
	struct isp_hw_path_scaler path_scaler;
	struct isp_hw_path_store path_store;

	struct completion frame_done;
};

static char *isp_dev_name[] = {
	"CAMT_ISP0",
	"CAMT_ISP1"
};

static struct ispt_context *ispt_cxt = NULL;
unsigned long tmp_addr[4] = {0};

static void isp_all_done(int isp_hw_id, void *param)
{
	struct ispt_context *cxt = (struct ispt_context *)param;

	if (isp_hw_id < 0 || isp_hw_id > 3) {
		pr_err("fail to get isp_hw_id %d\n", isp_hw_id);
		return;
	}

	if (cxt->isp_cxt[isp_hw_id].fmcu_used) {
		pr_debug("fmcu started, skip all done\n ");
		return;
	}

	pr_info("isp_hw_id:%d all done\n", isp_hw_id);
	complete(&cxt->frame_done);
}

static void isp_fmcu_store_done(int isp_hw_id, void *param)
{
	struct ispt_context *cxt = (struct ispt_context *)param;

	if (isp_hw_id < 0 || isp_hw_id > 3) {
		pr_err("fail to get isp_hw_id %d\n", isp_hw_id);
		return;
	}

	pr_debug("fmcu done isp_hw_id:%d\n", isp_hw_id);
	complete(&cxt->frame_done);
}

typedef void(*isp_isr)(int id, void *param);

/* FMCU can only be used by C0 only */
static const unsigned int isp_irq_process_c0[] = {
	ISP_INT_SHADOW_DONE,
	ISP_INT_DISPATCH_DONE,
	ISP_INT_STORE_DONE_PRE,
	ISP_INT_STORE_DONE_VID,
	ISP_INT_NR3_ALL_DONE,
	ISP_INT_NR3_SHADOW_DONE,
	ISP_INT_FMCU_LOAD_DONE,
	ISP_INT_FMCU_SHADOW_DONE,
	ISP_INT_ISP_ALL_DONE,
	ISP_INT_FMCU_STORE_DONE,
};

static const unsigned int isp_irq_process[] = {
	ISP_INT_SHADOW_DONE,
	ISP_INT_DISPATCH_DONE,
	ISP_INT_STORE_DONE_PRE,
	ISP_INT_STORE_DONE_VID,
	ISP_INT_NR3_ALL_DONE,
	ISP_INT_NR3_SHADOW_DONE,
	ISP_INT_ISP_ALL_DONE,
};


static isp_isr isp_isr_handler[32] = {
	[ISP_INT_ISP_ALL_DONE] = isp_all_done,
	[ISP_INT_FMCU_STORE_DONE] = isp_fmcu_store_done,
};

struct isp_int_context {
	unsigned long reg_offset;
	unsigned int err_mask;
	unsigned int irq_numbers;
	const unsigned int *irq_vect;
} isp_int_cxts[4] = {
		{ /* P0 */
			ISP_P0_INT_BASE,
			ISP_INT_LINE_MASK_ERR,
			(unsigned int)ARRAY_SIZE(isp_irq_process),
			isp_irq_process,
		},
		{ /* C0 */
			ISP_C0_INT_BASE,
			ISP_INT_LINE_MASK_ERR,
			(unsigned int)ARRAY_SIZE(isp_irq_process_c0),
			isp_irq_process_c0,
		},
		{ /* P1 */
			ISP_P1_INT_BASE,
			ISP_INT_LINE_MASK_ERR,
			(unsigned int)ARRAY_SIZE(isp_irq_process),
			isp_irq_process,
		},
		{ /* C1 */
			ISP_C1_INT_BASE,
			ISP_INT_LINE_MASK_ERR,
			(unsigned int)ARRAY_SIZE(isp_irq_process),
			isp_irq_process,
		},
};

static irqreturn_t isp_isr_root(int irq, void *priv)
{
	int c_id;
	unsigned long irq_offset;
	unsigned int iid;
	unsigned int sid, k;
	unsigned int err_mask;
	unsigned int irq_line = 0;
	unsigned int mmu_irq_line = 0;
	unsigned int irq_numbers = 0;
	const unsigned int *irq_vect = NULL;
	unsigned int val;
	struct ispt_context *cxt = (struct ispt_context *)priv;

	if (irq == cxt->isp_irq[0]) {
		iid = 0;
	} else if (irq == cxt->isp_irq[1]) {
		iid = 1;
	} else {
		pr_err("fail to get irq %d mismatched\n", irq);
		return IRQ_NONE;
	}

	for (sid = 0; sid < 2; sid++) {
		c_id = (iid << 1) | sid;

		irq_offset = isp_int_cxts[c_id].reg_offset;
		err_mask = isp_int_cxts[c_id].err_mask;
		irq_numbers = isp_int_cxts[c_id].irq_numbers;
		irq_vect = isp_int_cxts[c_id].irq_vect;

		irq_line = ISP_HREG_RD(irq_offset + ISP_INT_INT0);
		if (unlikely(irq_line == 0)) {
			continue;
		}

		/*clear the interrupt*/
		ISP_HREG_WR(irq_offset + ISP_INT_CLR0, irq_line);

		pr_debug("isp ctx %d irqno %d, INT: 0x%x\n", c_id, irq, irq_line);
		if (unlikely(err_mask & irq_line)) {
			pr_err("fail to handle,isp ctx%d status 0x%x\n", c_id, irq_line);
			/*handle the error here*/
			//TODO:
		}

		mmu_irq_line = ISP_MMU_RD(ISP_MMU_INT_STS);
		if (unlikely(ISP_INT_LINE_MASK_MMU & mmu_irq_line)) {
			pr_info("isp ctx%d status 0x%x\n", c_id, irq_line);
			val = ISP_MMU_RD(ISP_MMU_INT_STS);
			//TODO:

			ISP_MMU_WR(ISP_MMU_INT_CLR, mmu_irq_line);
		}

		for (k = 0; k < irq_numbers; k++) {
			unsigned int irq_id = irq_vect[k];

			if (irq_line & (1 << irq_id)) {
				if (isp_isr_handler[irq_id])
					isp_isr_handler[irq_id](c_id, cxt);
			}
			irq_line  &= ~(1 << irq_id);
			if (!irq_line)
				break;
		}
	}

	return IRQ_HANDLED;
}

static int camt_isp_hw_init(struct ispt_context *cxt)
{
	int ret = 0;
	int i;
	struct cam_hw_info *hw = NULL;
	struct isp_hw_default_param tmp_default;

	hw = cxt->hw;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	ret = sprd_cam_pw_on();
	ret |= sprd_cam_domain_eb();
#endif

	hw->isp_ioctl(hw, ISP_HW_CFG_ENABLE_CLK, NULL);
	hw->isp_ioctl(hw, ISP_HW_CFG_RESET, NULL);

	for (i = 0; i < ISP_LOGICAL_COUNT; i++) {
		cxt->isp_irq[i] = s_isp_irq_no[i];
		ret = devm_request_irq(&hw->pdev->dev, cxt->isp_irq[i], isp_isr_root,
			IRQF_SHARED, isp_dev_name[i], cxt);
		if (ret) {
			pr_err("fail to init hw, ISP%d irq %d\n", i, cxt->isp_irq[i]);
			return -EFAULT;
		}
	}
	pr_info("fail to get isp irq %d %d\n", cxt->isp_irq[0], cxt->isp_irq[1]);

	/* set hw default param */
	tmp_default.type = ISP_HW_PARA;
	hw->isp_ioctl(hw, ISP_HW_CFG_DEFAULT_PARA_SET, &tmp_default);

	/* enable & clear irq for 4 hw context */
	for (i = 0; i < ISP_HW_MAX_COUNT; i++) {
		hw->isp_ioctl(hw, ISP_HW_CFG_ENABLE_IRQ, &i);
		hw->isp_ioctl(hw, ISP_HW_CFG_CLEAR_IRQ, &i);
	}

	return ret;
}

static int camt_isp_hw_deinit(struct ispt_context *cxt)
{
	int ret = 0;
	int i;
	struct cam_hw_info *hw = NULL;

	hw = cxt->hw;

	hw->isp_ioctl(hw, ISP_HW_CFG_RESET, NULL);
	for (i = 0; i < ISP_LOGICAL_COUNT; i++) {
		devm_free_irq(&hw->pdev->dev, cxt->isp_irq[i], cxt);
	}
	hw->isp_ioctl(hw, ISP_HW_CFG_DISABLE_CLK, NULL);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	sprd_cam_domain_disable();
	sprd_cam_pw_off();
#endif
	return ret;
}

static int camt_isp_cfg_init(struct ispt_context *ctx, struct camt_info *info)
{
	int ret = 0, i = 0;
	int iommu_enable = 0;
	size_t size;
	void *sw_addr = NULL;
	unsigned long hw_addr = 0, aligned_addr = 0;
	struct camera_buf *ion_buf = NULL;
	struct isp_hw_cfg_map maparg;
	struct isp_dev_cfg_info cfg_settings = {
		0, 1, 1, 0, NULL,
		0, 1, 1, 1, 1,
		0, 0, 0, 0,
		0, 0, 0
	};

	/*alloc cfg context buffer*/
	ion_buf = &ctx->cfg_buf;
	memset(ion_buf, 0, sizeof(ctx->cfg_buf));
	sprintf(ion_buf->name, "isp_cfg_ctx");

	if (cam_buf_iommu_status_get(CAM_IOMMUDEV_ISP) == 0) {
		pr_debug("isp iommu enable\n");
		iommu_enable = 1;
	} else {
		pr_debug("isp iommu disable\n");
		iommu_enable = 0;
	}
	size = CAMT_ISP_CFG_BUF_SIZE;
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

	sw_addr = (void *)ion_buf->addr_k[0];
	hw_addr = ion_buf->iova[0];
	if (!IS_ERR_OR_NULL(sw_addr) && hw_addr) {
		if (!IS_ALIGNED(hw_addr, CAMT_CFG_SIZE)) {
			aligned_addr = ALIGN(hw_addr, CAMT_CFG_SIZE);
			sw_addr = sw_addr + aligned_addr - hw_addr;
			hw_addr = aligned_addr;
		}
		pr_info("aligned sw: %p, hw: 0x%lx", sw_addr, hw_addr);
	}
	pr_info("cmd sw: %p, hw: 0x%lx, size:0x%x\n",
		sw_addr, hw_addr, (int)ion_buf->size[0]);

	for (i = 0; i < ISP_HW_MAX_COUNT; i++) {
		ctx->isp_cxt[i].cfg_sw_addr = sw_addr + i * CAMT_CFG_SIZE;
		ctx->isp_cxt[i].cfg_hw_addr = hw_addr + i * CAMT_CFG_SIZE;
		tmp_addr[i] = (unsigned long)ctx->isp_cxt[i].cfg_sw_addr;
		isp_cfg_poll_addr[i] = &tmp_addr[i];
		pr_info("isp ctx %d sw=%p, hw:0x%lx\n",
			i, ctx->isp_cxt[i].cfg_sw_addr, ctx->isp_cxt[i].cfg_hw_addr);
	}

	ctx->hw->isp_ioctl(ctx->hw, ISP_HW_CFG_CFG_MAP_INFO_GET, &cfg_settings);
	maparg.map_cnt = ctx->cfg_map_cnt;
	maparg.s_cfg_settings = &cfg_settings;
	if (info->test_mode == ISP_CFG_MODE)
		ctx->hw->isp_ioctl(ctx->hw, ISP_HW_CFG_MAP_INIT, &maparg);
	return ret;

err_hwmap_cfg:
	cam_buf_kunmap(ion_buf);
err_kmap_cfg:
	cam_buf_free(ion_buf);
err_alloc_cfg:
	return ret;
}

static int camt_isp_get_ctx_id(struct camt_info *info)
{
	uint32_t ctx_id = 0;

	CHECK_NULL(info);

	if (info->chip == CAMT_CHIP_ISP0) {
		ctx_id = (0 << 1) | (info->path_id[0]);
	} else if (info->chip == CAMT_CHIP_ISP1) {
		ctx_id = (1 << 1) | (info->path_id[0]);
	} else {
		pr_err("fail to get valid chip %d\n", info->chip);
		return 0;
	}

	return ctx_id;
}

static int camt_isp_cfg_fetch(struct ispt_context *ctx,
	struct camt_info *info)
{
	int ret = 0;
	uint32_t ctx_id = 0;
	int iommu_enable = 0;
	unsigned long trim_offset[3] = { 0 };
	struct camera_buf *in_buf = NULL;
	struct isp_hw_fetch_info *fetch = NULL;

	CHECK_NULL(ctx);
	CHECK_NULL(info);

	fetch = &ctx->fetch_info;
	ctx_id = camt_isp_get_ctx_id(info);

	fetch->ctx_id = ctx_id;
	fetch->bayer_pattern = info->bayer_mode;
	fetch->dispatch_color = 0;
	fetch->fetch_path_sel = 0;
	fetch->pack_bits = 0;
	fetch->sec_mode = 0;
	fetch->fetch_fmt = ISP_FETCH_RAW10;
	fetch->in_trim.start_x = info->crop_rect.x;
	fetch->in_trim.start_y = info->crop_rect.y;
	fetch->in_trim.size_x = info->crop_rect.w;
	fetch->in_trim.size_y = info->crop_rect.h;

	switch (fetch->fetch_fmt) {
	case ISP_FETCH_YUV422_3FRAME:
	case ISP_FETCH_YUYV:
	case ISP_FETCH_UYVY:
	case ISP_FETCH_YVYU:
	case ISP_FETCH_VYUY:
	case ISP_FETCH_RAW10:
		fetch->pitch.pitch_ch0 = cal_sprd_raw_pitch(info->input_size.w, 1);
		trim_offset[0] = 0;
		break;
	case ISP_FETCH_YUV422_2FRAME:
	case ISP_FETCH_YVU422_2FRAME:
	case ISP_FETCH_YUV420_2FRAME:
	case ISP_FETCH_YVU420_2FRAME:
	case ISP_FETCH_FULL_RGB10:
		/* TBD */
		break;
	case ISP_FETCH_CSI2_RAW10:
	{
		uint32_t mipi_byte_info = 0;
		uint32_t mipi_word_info = 0;
		uint32_t start_col = info->crop_rect.x;
		uint32_t start_row = info->crop_rect.y;
		uint32_t end_col = info->crop_rect.x + info->crop_rect.w - 1;
		uint32_t mipi_word_num_start[16] = {
			0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5};
		uint32_t mipi_word_num_end[16] = {
			0, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5};

		mipi_byte_info = start_col & 0xF;
		mipi_word_info =
			((end_col + 1) >> 4) * 5
			+ mipi_word_num_end[(end_col + 1) & 0xF]
			- ((start_col + 1) >> 4) * 5
			- mipi_word_num_start[(start_col + 1) & 0xF] + 1;
		fetch->mipi_byte_rel_pos = mipi_byte_info;
		fetch->mipi_word_num = mipi_word_info;
		fetch->pitch.pitch_ch0 = cal_sprd_raw_pitch(info->input_size.w, 0);
		/* same as slice starts */
		trim_offset[0] = start_row * fetch->pitch.pitch_ch0
					+ (start_col >> 2) * 5
					+ (start_col & 0x3);
		break;
	}
	default:
		pr_err("fail to get fetch format: %d\n", fetch->fetch_fmt);
		break;
	}

	fetch->trim_off.addr_ch0 = trim_offset[0];
	fetch->trim_off.addr_ch1 = trim_offset[1];
	fetch->trim_off.addr_ch2 = trim_offset[2];

	if (cam_buf_iommu_status_get(CAM_IOMMUDEV_ISP) == 0) {
		pr_debug("isp iommu enable\n");
		iommu_enable = 1;
	} else {
		pr_debug("isp iommu disable\n");
		iommu_enable = 0;
	}

	in_buf = &ctx->in_buf;
	in_buf->type = CAM_BUF_USER;
	in_buf->mfd[0] = info->inbuf_fd;
	ret = cam_buf_ionbuf_get(in_buf);
	if (ret) {
		pr_err("fail to get out ion buffer\n");
		goto exit;
	}

	ret = cam_buf_iommu_map(in_buf, CAM_IOMMUDEV_ISP);
	if (ret) {
		pr_err("fail to map to iommu\n");
		cam_buf_ionbuf_put(in_buf);
		goto exit;
	}

	fetch->addr_hw.addr_ch0 = in_buf->iova[0];
	fetch->addr_hw.addr_ch1 = in_buf->iova[1];
	fetch->addr_hw.addr_ch2 = in_buf->iova[2];
	fetch->addr_hw.addr_ch0 += fetch->trim_off.addr_ch0;
	fetch->addr_hw.addr_ch1 += fetch->trim_off.addr_ch1;
	fetch->addr_hw.addr_ch2 += fetch->trim_off.addr_ch2;

	return ret;
exit:
	return ret;
}

static int camt_isp_cfg_store(struct ispt_context *ctx,
	struct camt_info *info, int i)
{
	int ret = 0;
	uint32_t ctx_id = 0;
	int iommu_enable = 0;
	unsigned int size;
	struct camera_buf *out_buf = NULL;
	struct isp_hw_path_store *store_info = NULL;
	struct isp_store_info *store = NULL;

	CHECK_NULL(ctx);
	CHECK_NULL(info);

	store_info = &ctx->path_store;
	store = &store_info->store;
	ctx_id = camt_isp_get_ctx_id(info);

	store_info->ctx_id = ctx_id;
	store_info->spath_id = info->path_id[i];
	store->color_fmt = ISP_STORE_YUV420_2FRAME;
	store->bypass = 0;
	store->endian = ENDIAN_LITTLE;
	store->speed_2x = 1;
	store->mirror_en = 0;
	store->max_len_sel = 0;
	store->shadow_clr_sel = 1;
	store->shadow_clr = 1;
	store->store_res = 1;
	store->rd_ctrl = 0;

	store->size.w = info->output_size.w;
	store->size.h = info->output_size.h;
	switch (store->color_fmt) {
	case ISP_STORE_UYVY:
	case ISP_STORE_YUV422_2FRAME:
	case ISP_STORE_YVU422_2FRAME:
	case ISP_STORE_YUV422_3FRAME:
	case ISP_STORE_YUV420_3FRAME:
		/* TBD */
		break;
	case ISP_STORE_YUV420_2FRAME:
	case ISP_STORE_YVU420_2FRAME:
		store->pitch.pitch_ch0 = store->size.w;
		store->pitch.pitch_ch1 = store->size.w;
		store->total_size = store->size.w * store->size.h * 3 / 2;
		break;
	default:
		pr_err("fail to get support store fmt: %d\n", store->color_fmt);
			store->pitch.pitch_ch0 = 0;
			store->pitch.pitch_ch1 = 0;
			store->pitch.pitch_ch2 = 0;
		break;
	}

	if (cam_buf_iommu_status_get(CAM_IOMMUDEV_ISP) == 0) {
		pr_debug("isp iommu enable\n");
		iommu_enable = 1;
	} else {
		pr_debug("isp iommu disable\n");
		iommu_enable = 0;
	}

	size = store->pitch.pitch_ch0 * info->input_size.h;
	out_buf = &ctx->out_buf;
	out_buf->type = CAM_BUF_USER;
	out_buf->mfd[0] = info->outbuf_fd[i];
	ret = cam_buf_ionbuf_get(out_buf);
	if (ret) {
		pr_err("fail to get out ion buffer\n");
		goto exit;
	}

	ret = cam_buf_iommu_map(out_buf, CAM_IOMMUDEV_ISP);
	if (ret) {
		pr_err("fail to map to iommu\n");
		cam_buf_ionbuf_put(out_buf);
		goto exit;
	}

	store->addr.addr_ch0 = out_buf->iova[0];
	store->addr.addr_ch1 = out_buf->iova[1];
	store->addr.addr_ch2 = out_buf->iova[2];
	store->addr.addr_ch1 = store->addr.addr_ch0 + size;

	return ret;
exit:
	return ret;
}

int ispt_init(struct cam_hw_info *hw, struct camt_info *info)
{
	int ret = 0;
	int i = 0;
	struct ispt_context *cxt = NULL;

	CHECK_NULL(hw);
	CHECK_NULL(info);

	cxt = kzalloc(sizeof(struct ispt_context), GFP_KERNEL);
	if (cxt == NULL) {
		pr_err("fail to alloc memory\n");
		return -ENOMEM;
	}

	cxt->hw = hw;
	ispt_cxt = cxt;

	atomic_set(&cxt->cfg_map_cnt, 0);
	init_completion(&cxt->frame_done);
	ret = camt_isp_hw_init(cxt);
	if (ret) {
		pr_err("fail to init hw\n");
		goto exit;
	}

	ret = camt_isp_cfg_init(cxt, info);
	if (ret) {
		pr_err("fail to init cfg\n");
		goto cfg_init_fail;
	}

	if (info->test_mode == ISP_AP_MODE) {
		for (i = 0; i < ISP_HW_MAX_COUNT; i++)
			isp_cfg_poll_addr[i] = &s_isp_regbase[0];
	}
	pr_info("init ok\n");
	return ret;

cfg_init_fail:
	camt_isp_hw_deinit(cxt);
exit:
	kfree(cxt);
	ispt_cxt = NULL;
	return ret;
}

int ispt_start(struct camt_info *info)
{
	int ret = 0;
	uint32_t ctx_id = 0;
	int i = 0;
	struct ispt_context *ctx = NULL;
	struct cam_hw_info *hw = NULL;
	struct isp_hw_path_scaler *path_scaler = NULL;
	struct isp_hw_default_param dfult_param;

	ctx = ispt_cxt;
	hw = ctx->hw;
	path_scaler = &ctx->path_scaler;
	ret = camt_isp_cfg_fetch(ctx, info);
	if (ret) {
		pr_err("fail to cfg isp param\n");
		goto exit;
	}

	pr_debug("chip %d mode %d srcw%d h%d crop w%d h%d out w%d h%d\n",
		info->chip, info->test_mode, info->input_size.w, info->input_size.h,
		info->crop_rect.w, info->crop_rect.h,
		info->output_size.w, info->output_size.h);
	ctx_id = ctx->fetch_info.ctx_id;
	dfult_param.type = ISP_CFG_PARA;
	dfult_param.index = ctx_id;
	hw->isp_ioctl(hw, ISP_HW_CFG_DEFAULT_PARA_SET, &dfult_param);
	hw->isp_ioctl(hw, ISP_HW_CFG_FETCH_FRAME_ADDR, &ctx->fetch_info);
	hw->isp_ioctl(hw, ISP_HW_CFG_FETCH_SET, &ctx->fetch_info);

	for (i = 0; i < DRV_PATH_NUM; i++) {
		path_scaler->ctx_id = ctx_id;
		path_scaler->uv_sync_v = 0;
		path_scaler->frm_deci = 0;
		path_scaler->scaler.odata_mode = 1;
		path_scaler->spath_id = info->path_id[i];
		path_scaler->deci.deci_x_eb = 0;
		path_scaler->deci.deci_y_eb = 0;
		path_scaler->deci.deci_x = 0;
		path_scaler->deci.deci_y = 0;
		path_scaler->src.w = info->input_size.w;
		path_scaler->src.h = info->input_size.h;
		path_scaler->in_trim.start_x = info->crop_rect.x;
		path_scaler->in_trim.start_y = info->crop_rect.y;
		path_scaler->in_trim.size_x = info->crop_rect.w;
		path_scaler->in_trim.size_y = info->crop_rect.h;
		path_scaler->out_trim.start_x = 0;
		path_scaler->out_trim.start_y = 0;
		path_scaler->out_trim.size_x = info->output_size.w;
		path_scaler->out_trim.size_y = info->output_size.h;
		path_scaler->dst.w = info->output_size.w;
		path_scaler->dst.h = info->output_size.h;
		/* TBD just bypass scaler */
		path_scaler->scaler.scaler_bypass = 1;
		hw->isp_ioctl(hw, ISP_HW_CFG_SET_PATH_SCALER, path_scaler);

		ret = camt_isp_cfg_store(ctx, info, i);
		if (ret) {
			pr_err("fail to cfg isp param\n");
			goto exit;
		}

		hw->isp_ioctl(hw, ISP_HW_CFG_STORE_FRAME_ADDR, &ctx->path_store);
		hw->isp_ioctl(hw, ISP_HW_CFG_SET_PATH_STORE, &ctx->path_store);
	}

	pr_info("fail to ctx_id %d addr %lx\n", ctx_id, ctx->isp_cxt[ctx_id].cfg_hw_addr);
	ISP_HREG_WR(camt_cfg_addr_reg[ctx_id], ctx->isp_cxt[ctx_id].cfg_hw_addr);

	if (info->test_mode == 1) {
		/* AP mode */
		hw->isp_ioctl(hw, ISP_HW_CFG_FETCH_START, NULL);
	} else {
		/* CFG mode */
		hw->isp_ioctl(hw, ISP_HW_CFG_START_ISP, &ctx_id);
	}

	pr_info("wait for frame tx done\n");
	ret = wait_for_completion_interruptible(&ctx->frame_done);
	pr_info("wait done\n");

	cam_buf_iommu_unmap(&ctx->in_buf);
	cam_buf_ionbuf_put(&ctx->in_buf);
	memset(&ctx->in_buf, 0, sizeof(struct camera_buf));

	cam_buf_iommu_unmap(&ctx->out_buf);
	cam_buf_ionbuf_put(&ctx->out_buf);
	memset(&ctx->out_buf, 0, sizeof(struct camera_buf));

	pr_info("ispt_start done\n");
	return ret;
exit:
	return ret;
}

int ispt_stop(void)
{
	int ret = 0, i = 0;
	struct ispt_context *ctx = NULL;
	struct camera_buf *ion_buf = NULL;

	ctx = ispt_cxt;

	ion_buf = &ctx->cfg_buf;
	atomic_set(&ctx->cfg_map_cnt, 0);
	for (i = 0; i < ISP_HW_MAX_COUNT; i++) {
		ctx->isp_cxt[i].cfg_sw_addr = NULL;
		ctx->isp_cxt[i].cfg_hw_addr = 0;
		isp_cfg_poll_addr[i] = NULL;
	}

	cam_buf_iommu_unmap(ion_buf);
	cam_buf_kunmap(ion_buf);
	cam_buf_free(ion_buf);

	camt_isp_hw_deinit(ctx);

	return ret;
}

int ispt_deinit(void)
{
	int ret = 0;
	struct ispt_context *ctx = ispt_cxt;

	CHECK_NULL(ctx);

	kfree(ctx);
	ispt_cxt = NULL;

	return ret;
}

