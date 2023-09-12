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
#include "dcam_reg.h"
#include "dcam_int.h"
#include "dcam_interface.h"
#include "cam_buf.h"
#include "cam_test.h"
#include "sprd_cam_test.h"

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

struct dcamt_context_comm {
	struct img_size out_size;
	struct camera_buf out_buf;
};

struct dcamt_context {
	unsigned int state;
	int dcam_idx;
	int path_id[DRV_PATH_NUM];
	int dcam_irq;

	struct img_size in_size;
	struct img_trim in_trim;
	struct dcamt_context_comm comm_info[DRV_PATH_NUM];

	struct img_endian endian;

	struct camera_buf in_buf;

	int path_scl_sel;
	int path_bin_ratio;

	struct cam_hw_info *hw;

	spinlock_t glb_reg_lock;
	struct completion dcam_frame_com[DRV_PATH_NUM];
};

static char *dcam_dev_name[] = {
	"CAMT_DCAM0",
	"CAMT_DCAM1",
	"CAMT_DCAM2"
};
static struct dcamt_context *dcamt_cxt = NULL;


/* internal function implementation */
static void dcam_full_path_done(void *param)
{
	struct dcamt_context *cxt = (struct dcamt_context *)param;
	pr_info("dcam_full_path_done\n");
	complete(&cxt->dcam_frame_com[DCAM_PATH_FULL]);
}

static void dcam_bin_path_done(void *param)
{
	struct dcamt_context *cxt = (struct dcamt_context *)param;
	pr_info("dcam_bin_path_done\n");
	complete(&cxt->dcam_frame_com[DCAM_PATH_BIN]);
}

static int get_dcam_idx(unsigned int camt_dcam_id)
{
	int dcam_idx = -1;

	switch (camt_dcam_id) {
		case CAMT_CHIP_DCAM0:
			dcam_idx = DCAM_ID_0;
			break;
		case CAMT_CHIP_DCAM1:
			dcam_idx = DCAM_ID_1;
			break;
		case CAMT_CHIP_DCAM2:
			dcam_idx = DCAM_ID_2;
			break;
		default:
			pr_info("fail to support dcam %d\n", camt_dcam_id);
			break;
	}

	return dcam_idx;
}

static int get_dcam_path_id(unsigned int camt_dcam_path_id)
{
	int path_id = -1;

	switch (camt_dcam_path_id) {
		case CAMT_DCAM_PATH_FULL:
			path_id = DCAM_PATH_FULL;
			break;
		case CAMT_DCAM_PATH_BIN:
			path_id = DCAM_PATH_BIN;
			break;
		default:
			break;
	}

	return path_id;
}

static int cfg_dcam_path_size(struct dcamt_context *cxt, struct camt_info *test_info, int i)
{
	int ret = 0;
	bool invalid;
	struct img_size crop_size, dst_size;
	struct cam_hw_info *hw = NULL;
	uint32_t dcam_max_w = 0, dcam_max_h = 0;
	struct dcamt_context_comm *dcamt_comm_info = NULL;

	CHECK_NULL(cxt);
	CHECK_NULL(test_info);

	hw = cxt->hw;
	dcam_max_w = hw->ip_dcam[cxt->dcam_idx]->max_width;
	dcam_max_h = hw->ip_dcam[cxt->dcam_idx]->max_height;

	dcamt_comm_info = &cxt->comm_info[i];

	switch (cxt->path_id[i]) {
	case DCAM_PATH_FULL:
		cxt->in_size.w = test_info->input_size.w;
		cxt->in_size.h = test_info->input_size.h;
		cxt->in_trim.start_x = test_info->crop_rect.x;
		cxt->in_trim.start_y = test_info->crop_rect.y;
		cxt->in_trim.size_x = test_info->crop_rect.w;
		cxt->in_trim.size_y = test_info->crop_rect.h;

		invalid = 0;
		invalid |= ((cxt->in_size.w == 0) || (cxt->in_size.h == 0));
		invalid |= (cxt->in_size.w > dcam_max_w);
		invalid |= (cxt->in_size.h > dcam_max_h);
		invalid |= ((cxt->in_trim.start_x + cxt->in_trim.size_x) > cxt->in_size.w);
		invalid |= ((cxt->in_trim.start_y + cxt->in_trim.size_y) > cxt->in_size.h);
		if (invalid) {
			pr_err("fail to get valid size, size:%d %d, trim %d %d %d %d\n",
				cxt->in_size.w, cxt->in_size.h,
				cxt->in_trim.start_x, cxt->in_trim.start_y,
				cxt->in_trim.size_x,
				cxt->in_trim.size_y);
			return -EINVAL;
		}
		if ((cxt->in_size.w > cxt->in_trim.size_x) ||
			(cxt->in_size.h > cxt->in_trim.size_y)) {
			dcamt_comm_info->out_size.w = cxt->in_trim.size_x;
			dcamt_comm_info->out_size.h = cxt->in_trim.size_y;
		} else {
			dcamt_comm_info->out_size.w = cxt->in_size.w;
			dcamt_comm_info->out_size.h = cxt->in_size.h;
		}

		pr_info("cfg dcam full path done. size %d %d %d %d\n",
			cxt->in_size.w, cxt->in_size.h,
			dcamt_comm_info->out_size.w, dcamt_comm_info->out_size.h);
		pr_info("trim %d %d %d %d\n",
			cxt->in_trim.start_x, cxt->in_trim.start_y,
			cxt->in_trim.size_x, cxt->in_trim.size_y);
		break;

	case DCAM_PATH_BIN:
		cxt->in_size.w = test_info->input_size.w;
		cxt->in_size.h = test_info->input_size.h;
		cxt->in_trim.start_x = test_info->crop_rect.x;
		cxt->in_trim.start_y = test_info->crop_rect.y;
		cxt->in_trim.size_x = test_info->crop_rect.w;
		cxt->in_trim.size_y = test_info->crop_rect.h;
		dcamt_comm_info->out_size.w = test_info->output_size.w;
		dcamt_comm_info->out_size.h = test_info->output_size.h;

		invalid = 0;
		invalid |= ((cxt->in_size.w == 0) || (cxt->in_size.h == 0));
		invalid |= (cxt->in_size.w > dcam_max_w);
		invalid |= (cxt->in_size.h > dcam_max_h);

		/* trim should not be out range of source */
		invalid |= ((cxt->in_trim.start_x + cxt->in_trim.size_x) > cxt->in_size.w);
		invalid |= ((cxt->in_trim.start_y + cxt->in_trim.size_y) > cxt->in_size.h);

		/* output size should not be larger than trim ROI */
		invalid |= (cxt->in_trim.size_x < dcamt_comm_info->out_size.w);
		invalid |= (cxt->in_trim.size_y < dcamt_comm_info->out_size.h);

		/* Down scaling should not be smaller then 1/4*/
		invalid |= (cxt->in_trim.size_x > (dcamt_comm_info->out_size.w * DCAM_SCALE_DOWN_MAX));
		invalid |= (cxt->in_trim.size_y > (dcamt_comm_info->out_size.h * DCAM_SCALE_DOWN_MAX));

		if (invalid) {
			pr_err("fail to get valid size, size:%d %d, trim %d %d %d %d, dst %d %d\n",
				cxt->in_size.w, cxt->in_size.h,
				cxt->in_trim.start_x, cxt->in_trim.start_y,
				cxt->in_trim.size_x, cxt->in_trim.size_y,
				dcamt_comm_info->out_size.w, dcamt_comm_info->out_size.h);
			return -EINVAL;
		}

		crop_size.w = cxt->in_trim.size_x;
		crop_size.h = cxt->in_trim.size_y;
		dst_size = dcamt_comm_info->out_size;

		if ((crop_size.w == dst_size.w) && (crop_size.h == dst_size.h))
			cxt->path_scl_sel = 0x2;
		else if ((dst_size.w * 2 == crop_size.w) && (dst_size.h * 2 == crop_size.h)) {
			pr_debug("1/2 binning used. src %d %d, dst %d %d\n",
				crop_size.w, crop_size.h, dst_size.w,
				dst_size.h);
			cxt->path_scl_sel = 0x0;
			cxt->path_bin_ratio = 0;
		} else if ((dst_size.w * 4 == crop_size.w) && (dst_size.h * 4 == crop_size.h)) {
			pr_debug("1/4 binning used. src %d %d, dst %d %d\n",
				crop_size.w, crop_size.h, dst_size.w,
				dst_size.h);
			cxt->path_scl_sel = 0x0;
			cxt->path_bin_ratio = 1;
		} else {
			pr_debug("RDS used. in %d %d, out %d %d\n",
				crop_size.w, crop_size.h, dst_size.w,
				dst_size.h);
			//TODO: for sharkl5pro
		}

		pr_info("cfg dcam bin path done. size %d %d dst %d %d\n",
			cxt->in_size.w, cxt->in_size.h,
			dcamt_comm_info->out_size.w, dcamt_comm_info->out_size.h);
		pr_info("scaler %d. trim %d %d %d %d\n", cxt->path_scl_sel,
			cxt->in_trim.start_x, cxt->in_trim.start_y,
			cxt->in_trim.size_x, cxt->in_trim.size_y);
		break;

	default:
		pr_err("fail to get known path %d\n", cxt->path_id[i]);
		ret = -EFAULT;
		break;
	}

	return ret;
}

static int set_dcam_path_store_addr(struct dcamt_context *cxt, struct camt_info *test_info, int i)
{
	int ret = 0;
	int idx = 0, path_id = 0;
	int iommu_enable = 0;
	unsigned int size;
	unsigned long addr = 0;
	struct cam_hw_info *hw = NULL;
	struct camera_buf *out_buf = NULL;

	CHECK_NULL(cxt);
	CHECK_NULL(test_info);

	hw = cxt->hw;
	idx = cxt->dcam_idx;

	path_id = cxt->path_id[i];
	addr= *(hw->ip_dcam[idx]->store_addr_tab + path_id);

	if (addr == 0L) {
		pr_info("DCAM%d invalid path id %d", idx, path_id);
		ret = -EINVAL;
		goto exit;
	}

	if (cam_buf_iommu_status_get(CAM_IOMMUDEV_DCAM) == 0)
		iommu_enable = 1;
	size = test_info->pitch * test_info->input_size.h;
	out_buf = &cxt->comm_info[i].out_buf;
	out_buf->type = CAM_BUF_USER;
	out_buf->mfd[0] = test_info->outbuf_fd[i];
	ret = cam_buf_ionbuf_get(out_buf);
	if (ret) {
		pr_err("fail to get out ion buffer\n");
		goto exit;
	}

	ret = cam_buf_iommu_map(out_buf, CAM_IOMMUDEV_DCAM);
	if (ret) {
		pr_err("fail to map to iommu\n");
		cam_buf_ionbuf_put(out_buf);
		goto exit;
	}

	DCAM_REG_WR(idx, addr, out_buf->iova[0]);

exit:
	return ret;
}

/*
 * registered sub interrupt service routine
 */
typedef void (*dcam_isr)(void *param);
static const dcam_isr _DCAM_ISRS[] = {
	[DCAM_FULL_PATH_TX_DONE] = dcam_full_path_done,
	[DCAM_PREV_PATH_TX_DONE] = dcam_bin_path_done,
};

/*
 * interested interrupt bits in DCAM0
 */
static const int _DCAM0_SEQUENCE[] = {
	DCAM_PREV_PATH_TX_DONE,/* for bin path */
	DCAM_FULL_PATH_TX_DONE,/* for full path */
};

/*
 * interested interrupt bits in DCAM1
 */
static const int _DCAM1_SEQUENCE[] = {
	DCAM_PREV_PATH_TX_DONE,/* for bin path */
	DCAM_FULL_PATH_TX_DONE,/* for full path */
};

/*
 * interested interrupt bits in DCAM2
 */
static const int _DCAM2_SEQUENCE[] = {
	DCAM2_SENSOR_EOF,/* TODO: why for flash */
	DCAM2_FULL_PATH_TX_DONE,/* for path data */
};

/*
 * interested interrupt bits
 */
static const struct {
	size_t count;
	const int *bits;
} DCAM_SEQUENCES[DCAM_ID_MAX] = {
	{
		ARRAY_SIZE(_DCAM0_SEQUENCE),
		_DCAM0_SEQUENCE,
	},
	{
		ARRAY_SIZE(_DCAM1_SEQUENCE),
		_DCAM1_SEQUENCE,
	},
	{
		ARRAY_SIZE(_DCAM2_SEQUENCE),
		_DCAM2_SEQUENCE,
	},
};

static void print_reg(int idx)
{
	unsigned long addr;

	pr_info("DCAM%d: Register list\n", idx);
	for (addr = DCAM_IP_REVISION; addr <= DCAM_CHECK_LASE_STATUS;
		addr += 16) {
		pr_info("0x%03lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			DCAM_REG_RD(idx, addr),
			DCAM_REG_RD(idx, addr + 4),
			DCAM_REG_RD(idx, addr + 8),
			DCAM_REG_RD(idx, addr + 12));
	}
	pr_info("AXIM: Register list\n");
	for (addr = AXIM_CTRL; addr <= IMG_FETCH_RADDR;
		addr += 16) {
		pr_info("0x%03lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			REG_RD(g_dcam_aximbase[idx] + addr),
			REG_RD(g_dcam_aximbase[idx] + addr + 4),
			REG_RD(g_dcam_aximbase[idx] + addr + 8),
			REG_RD(g_dcam_aximbase[idx] + addr + 12));
	}
}

static irqreturn_t dcam_isr_root(int irq, void *priv)
{
	int i = 0;
	struct dcamt_context *cxt = (struct dcamt_context *)priv;
	unsigned int status = 0;

	if (unlikely(irq != cxt->dcam_irq)) {
		pr_err("fail to get irq, DCAM%u irq %d mismatch %d\n",
			cxt->dcam_idx, irq, cxt->dcam_irq);
		return IRQ_NONE;
	}

	status = DCAM_REG_RD(cxt->dcam_idx, DCAM_INT_MASK);
	status &= DCAMINT_IRQ_LINE_MASK;

	if (unlikely(!status))
		return IRQ_NONE;

	DCAM_REG_WR(cxt->dcam_idx, DCAM_INT_CLR, status);

	if (unlikely(DCAMINT_ALL_ERROR & status)) {
		status &= (~DCAMINT_ALL_ERROR);
		print_reg(cxt->dcam_idx);
	}

	pr_debug("DCAM%u status=0x%x\n", cxt->dcam_idx, status);

	for (i = 0; i < DCAM_SEQUENCES[cxt->dcam_idx].count; i++) {
		int cur_int = DCAM_SEQUENCES[cxt->dcam_idx].bits[i];

		if (status & BIT(cur_int)) {
			if (_DCAM_ISRS[cur_int]) {
				_DCAM_ISRS[cur_int]((void*)cxt);
			} else {
				pr_warn("warning: DCAM%u missing handler for int %d\n",
					cxt->dcam_idx, cur_int);
			}
			status &= ~BIT(cur_int);
			if (!status)
				break;
		}
	}

	if (unlikely(status))
		pr_warn("warning: DCAM%u unhandled int 0x%x\n", cxt->dcam_idx, status);

	return IRQ_HANDLED;
}

static int camt_dcam_hw_init(struct dcamt_context *cxt)
{
	int ret = 0;
	struct cam_hw_info *hw = NULL;

	hw = cxt->hw;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	ret = sprd_cam_pw_on();
	ret |= sprd_cam_domain_eb();
	if (ret)
		return -EFAULT;
#endif

	hw->dcam_ioctl(hw, DCAM_HW_CFG_ENABLE_CLK, NULL);
	cxt->dcam_irq = hw->ip_dcam[cxt->dcam_idx]->irq_no;

	ret = devm_request_irq(&hw->pdev->dev, cxt->dcam_irq, dcam_isr_root,
		IRQF_SHARED, dcam_dev_name[cxt->dcam_idx], cxt);
	if (ret) {
		pr_err("fail to init hw, DCAM%d irq %d\n", cxt->dcam_idx, cxt->dcam_irq);
		return -EFAULT;
	}

	return ret;
}

static int camt_dcam_hw_deinit(struct dcamt_context *cxt)
{
	int ret = 0;
	struct cam_hw_info *hw = NULL;

	hw = cxt->hw;

	devm_free_irq(&hw->pdev->dev, cxt->dcam_irq, cxt);

	hw->dcam_ioctl(hw, DCAM_HW_CFG_DISABLE_CLK, hw);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	ret = sprd_cam_domain_disable();
	ret |= sprd_cam_pw_off();
#endif
	return ret;
}

/* external function implementation */
int dcamt_init(struct cam_hw_info *hw, struct camt_info *info)
{
	int ret = 0;
	int dcam_idx = -1;
	int dcam_path_id[DRV_PATH_NUM] = {-1};
	int i;

	struct dcamt_context *cxt = NULL;

	CHECK_NULL(hw);
	CHECK_NULL(info);

	cxt = kzalloc(sizeof(struct dcamt_context), GFP_KERNEL);
	if (cxt == NULL) {
		pr_err("fail to alloc memory\n");
		return -ENOMEM;
	}

	cxt->hw = hw;
	spin_lock_init(&cxt->glb_reg_lock);
	for(i = 0; i < DRV_PATH_NUM; i++)
		init_completion(&cxt->dcam_frame_com[i]);
	dcamt_cxt = cxt;

	dcam_idx = get_dcam_idx(info->chip);
	if (dcam_idx < 0) {
		ret = -EINVAL;
		goto info_fail;
	}
	cxt->dcam_idx = dcam_idx;

	for(i = 0; i < DRV_PATH_NUM; i++)
		dcam_path_id[i] = get_dcam_path_id(info->path_id[i]);

	for(i = 0; i < DRV_PATH_NUM; i++)
	{
		if (dcam_path_id[i] < 0) {
			ret = -EINVAL;
			goto info_fail;
		}
		cxt->path_id[i] = dcam_path_id[i];
	}

	ret = camt_dcam_hw_init(cxt);
	if (ret)
		return ret;

	/* axi init */
	hw->dcam_ioctl(hw, DCAM_HW_CFG_INIT_AXI, &cxt->dcam_idx);
	ret = hw->dcam_ioctl(hw, DCAM_HW_CFG_RESET, &cxt->dcam_idx);
	if (ret)
		goto reset_fail;

	pr_info("init ok\n");
	return ret;

reset_fail:
	camt_dcam_hw_deinit(cxt);
info_fail:
	kfree(cxt);
	dcamt_cxt = NULL;

	return ret;
}

int dcamt_start(struct camt_info *info)
{
	int rets[DRV_PATH_NUM];
	int ret;
	int result = 0;
	int iommu_enable = 0;
	unsigned int size;
	int i;
	struct camera_buf *in_buf = NULL;
	struct dcam_fetch_info fetch = {0};
	struct dcam_hw_path_start patharg = {0};
	struct dcam_hw_fetch_set fetcharg = {0};
	struct dcam_hw_force_copy copyarg = {0};
	struct dcam_hw_path_size path_size = {0};
	struct cam_hw_info *hw = NULL;
	struct dcamt_context *cxt = NULL;

	CHECK_NULL(info);

	cxt = dcamt_cxt;
	hw = cxt->hw;

	pr_info(" in %d %d crop %d %d %d %d out %d %d", info->input_size.w,
		info->input_size.h, info->crop_rect.x, info->crop_rect.y,
		info->crop_rect.w, info->crop_rect.h, info->output_size.w,
		info->output_size.h);
	info->is_loose = 0;
	info->pitch = cal_sprd_raw_pitch(info->input_size.w, info->is_loose);
	info->endian.y_endian = ENDIAN_LITTLE;

	if (!info->test_mode) {
		pr_info("not support online\n");
		return 0;
	}

	for (i = 0; i < DRV_PATH_NUM; i++) {
		/* check path enable or not */
		/* if not */
		ret = cfg_dcam_path_size(cxt, info, i);
		if (ret)
			goto exit;

		ret = set_dcam_path_store_addr(cxt, info, i);
		if (ret)
			goto exit;

		/* set path size */
		path_size.idx = cxt->dcam_idx;
		path_size.path_id = cxt->path_id[i];
		path_size.scaler_sel = cxt->path_scl_sel;
		path_size.in_size = cxt->in_size;
		path_size.in_trim = cxt->in_trim;
		path_size.out_size = cxt->comm_info[i].out_size;
		hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_SIZE_UPDATE, &path_size);

		/* start dcam path */
		patharg.path_id = cxt->path_id[i];
		patharg.idx = cxt->dcam_idx;
		patharg.cap_info.format = DCAM_CAP_MODE_RAWRGB;
		patharg.pack_bits = info->is_loose;
		patharg.src_sel = 1;
		patharg.in_trim = cxt->in_trim;
		patharg.endian.y_endian = info->endian.y_endian;
		hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_START, &patharg);
	}

	if (cam_buf_iommu_status_get(CAM_IOMMUDEV_DCAM) == 0)
		iommu_enable = 1;

	size = info->pitch * info->input_size.h;
	in_buf = &cxt->in_buf;
	in_buf->type = CAM_BUF_USER;
	in_buf->mfd[0] = info->inbuf_fd;
	ret = cam_buf_ionbuf_get(in_buf);
	if (ret) {
		pr_err("fail to get out ion buffer\n");
		goto exit;
	}

	ret = cam_buf_iommu_map(in_buf, CAM_IOMMUDEV_DCAM);
	if (ret) {
		pr_err("fail to map to iommu\n");
		cam_buf_ionbuf_put(in_buf);
		goto exit;
	}

	/* fetch set */
	fetch.pack_bits = info->is_loose;
	fetch.endian = ENDIAN_LITTLE;
	fetch.pattern = 0;
	fetch.size.w = cxt->in_size.w;
	fetch.size.h = cxt->in_size.h;
	fetch.trim.start_x = 0;
	fetch.trim.start_y = 0;
	fetch.trim.size_x = cxt->in_size.w;
	fetch.trim.size_y = cxt->in_size.h;
	fetch.addr.addr_ch0 = (uint32_t)in_buf->iova[0];
	fetcharg.idx = cxt->dcam_idx;
	fetcharg.fetch_info = &fetch;
	ret = hw->dcam_ioctl(hw, DCAM_HW_CFG_FETCH_SET, &fetcharg);

	/* force copy */
	copyarg.id = DCAM_CTRL_ALL;
	copyarg.idx = cxt->dcam_idx;
	copyarg.glb_reg_lock = cxt->glb_reg_lock;
	hw->dcam_ioctl(hw, DCAM_HW_CFG_FORCE_COPY, &copyarg);
	udelay(500);

	/* fetch start */
	hw->dcam_ioctl(hw, DCAM_HW_CFG_FETCH_START, hw);

	pr_info("wait for frame tx done\n");

	while(1) {
		for(i = 0; i < DRV_PATH_NUM; i++) {
			rets[i] = wait_for_completion_interruptible(&cxt->dcam_frame_com[i]);
			result |= rets[i];
		}
		if(!result)
			break;
	}

	pr_info("wait done\n");

	cam_buf_iommu_unmap(&cxt->in_buf);
	cam_buf_ionbuf_put(&cxt->in_buf);
	memset(&cxt->in_buf, 0, sizeof(struct camera_buf));

	for(i = 0; i < DRV_PATH_NUM; i++) {
		cam_buf_iommu_unmap(&cxt->comm_info[i].out_buf);
		cam_buf_ionbuf_put(&cxt->comm_info[i].out_buf);
		memset(&cxt->comm_info[i].out_buf, 0, sizeof(struct camera_buf));
	}

exit:
	return ret;
}

int dcamt_stop(void)
{
	int ret = 0;
	struct dcamt_context *cxt = NULL;

	cxt = dcamt_cxt;
	ret = camt_dcam_hw_deinit(cxt);

	return ret;
}

int dcamt_deinit(void)
{
	struct dcamt_context *cxt = dcamt_cxt;

	CHECK_NULL(cxt);

	kfree(cxt);
	dcamt_cxt = NULL;

	return 0;
}
