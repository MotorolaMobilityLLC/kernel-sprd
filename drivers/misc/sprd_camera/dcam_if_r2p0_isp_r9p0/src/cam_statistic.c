/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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
#include <linux/sprd_ion.h>
#include <linux/sprd_iommu.h>
#include <video/sprd_mm.h>

#include "cam_statistic.h"
#include "dcam_drv.h"
#include "isp_drv.h"

#define ION
#ifdef ION
#include "ion.h"
#include "ion_priv.h"
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CAM_STATICS: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int sprd_camstatistic_buf_split(
	struct cam_statis_buf *frm_statis_head,
	struct cam_buf_queue *statis_queue,
	struct cam_statis_buf *buf_reserved,
	struct device *dev,
	uint32_t buf_num,
	uint32_t buf_size,
	uint32_t buf_property,
	uint32_t iova_addr,
	uint32_t vir_addr,
	unsigned long kaddr,
	uint32_t *addr_offset,
	uint32_t is_reserved)
{
	int ret = 0;
	int cnt = 0;
	struct cam_statis_buf frm_statis;

	memset((void *)&frm_statis, 0x00, sizeof(frm_statis));

	for (cnt = 0; cnt < buf_num; cnt++) {
		frm_statis.phy_addr = iova_addr;
		frm_statis.vir_addr = vir_addr;
		frm_statis.kaddr[0] = kaddr;
#ifdef CONFIG_64BIT
		frm_statis.kaddr[1] = kaddr >> 32;
#endif
		frm_statis.addr_offset = *addr_offset;
		frm_statis.buf_info.mfd[0] =
			frm_statis_head->buf_info.mfd[0];
		frm_statis.buf_size = buf_size;
		frm_statis.buf_property = buf_property;
		frm_statis.buf_info.dev = dev;

		ret = sprd_cam_queue_buf_write(statis_queue,
			&frm_statis);
		iova_addr += buf_size;
		vir_addr += buf_size;
		kaddr += buf_size;
		*addr_offset += buf_size;
	}
	if (is_reserved) {
		buf_reserved->phy_addr = iova_addr;
		buf_reserved->vir_addr = vir_addr;
		buf_reserved->kaddr[0] = kaddr;
#ifdef CONFIG_64BIT
		buf_reserved->kaddr[1] = kaddr >> 32;
#endif
		buf_reserved->addr_offset = *addr_offset;
		buf_reserved->buf_info.mfd[0] =
			frm_statis_head->buf_info.mfd[0];
		buf_reserved->buf_size = buf_size;
		buf_reserved->buf_property = buf_property;
		buf_reserved->buf_info.dev = dev;

		*addr_offset += buf_size;
	}

	return ret;
}

int sprd_cam_statistic_queue_init(struct cam_statis_module *module,
	uint32_t idx, int module_flag)
{
	int ret = ISP_RTN_SUCCESS;

	if (!module) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	module->idx = idx;
	module->module_flag = module_flag;
	module->statis_valid = 0;

	if (module_flag == DCAM_DEV_STATIS && idx == DCAM_ID_2)
		return ret;
	else if (module_flag == ISP_DEV_STATIS && idx > 1)
		return ret;
	if (module_flag == DCAM_DEV_STATIS) {
		ret = sprd_cam_queue_buf_init(&module->aem_statis_queue,
			sizeof(struct cam_statis_buf),
			ISP_STATISTICS_QUEUE_LEN, "ae buf_queue");
		ret = sprd_cam_queue_buf_init(&module->afl_statis_queue,
			sizeof(struct cam_statis_buf),
			ISP_STATISTICS_QUEUE_LEN, "afl buf_queue");
		ret = sprd_cam_queue_buf_init(&module->afm_statis_queue,
			sizeof(struct cam_statis_buf),
			ISP_STATISTICS_QUEUE_LEN, "afm buf_queue");
		ret = sprd_cam_queue_buf_init(
			&module->pdaf_statis_queue,
			sizeof(struct cam_statis_buf),
			ISP_STATISTICS_QUEUE_LEN, "pdaf buf_queue");
		ret = sprd_cam_queue_buf_init(
			&module->ebd_statis_queue,
			sizeof(struct cam_statis_buf),
			ISP_STATISTICS_QUEUE_LEN, "embedded line buf_queue");

		ret = sprd_cam_queue_frm_init(&module->aem_statis_frm_queue,
			sizeof(struct cam_statis_buf),
			ISP_STATISTICS_QUEUE_LEN, "ae frm_queue");
		ret = sprd_cam_queue_frm_init(&module->afl_statis_frm_queue,
			sizeof(struct cam_statis_buf),
			ISP_STATISTICS_QUEUE_LEN, "afl frm_queue");
		ret = sprd_cam_queue_frm_init(&module->afm_statis_frm_queue,
			sizeof(struct cam_statis_buf),
			ISP_STATISTICS_QUEUE_LEN, "afm frm_queue");
		ret = sprd_cam_queue_frm_init(
			&module->pdaf_statis_frm_queue,
			sizeof(struct camera_frame),
			ISP_STATISTICS_QUEUE_LEN, "pdaf frm_queue");
		ret = sprd_cam_queue_frm_init(
			&module->ebd_statis_frm_queue,
			sizeof(struct camera_frame),
			ISP_STATISTICS_QUEUE_LEN, "embedded line frm_queue");
	} else if (module_flag == ISP_DEV_STATIS) {
		ret = sprd_cam_queue_buf_init(&module->hist_statis_queue,
			sizeof(struct cam_statis_buf),
			ISP_STATISTICS_QUEUE_LEN, "hist buf_queue");
		ret = sprd_cam_queue_frm_init(&module->hist_statis_frm_queue,
			sizeof(struct cam_statis_buf),
			ISP_STATISTICS_QUEUE_LEN, "hist frm_queue");
	} else {
		pr_err("fail to get valid module:not dcam or isp\n");
	}
	CAM_TRACE("statis buf init ok.\n");
	return ret;
}

void sprd_cam_statistic_queue_deinit(struct cam_statis_module *module)
{
	if (!module) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	if (module->module_flag == DCAM_DEV_STATIS) {
		sprd_cam_queue_buf_deinit(&module->aem_statis_queue);
		sprd_cam_queue_buf_deinit(&module->afl_statis_queue);
		sprd_cam_queue_buf_deinit(&module->afm_statis_queue);
		sprd_cam_queue_buf_deinit(&module->pdaf_statis_queue);
		sprd_cam_queue_buf_deinit(&module->ebd_statis_queue);

		sprd_cam_queue_frm_deinit(&module->aem_statis_frm_queue);
		sprd_cam_queue_frm_deinit(&module->afl_statis_frm_queue);
		sprd_cam_queue_frm_deinit(&module->afm_statis_frm_queue);
		sprd_cam_queue_frm_deinit(&module->pdaf_statis_frm_queue);
		sprd_cam_queue_frm_deinit(&module->ebd_statis_frm_queue);
	} else if (module->module_flag == ISP_DEV_STATIS) {
		sprd_cam_queue_buf_deinit(&module->hist_statis_queue);
		sprd_cam_queue_frm_deinit(&module->hist_statis_frm_queue);
	} else {
		pr_err("fail to get valid module:dcam or isp\n");
	}

	CAM_TRACE("statis buf deinit ok.\n");
}

void sprd_cam_statistic_queue_clear(struct cam_statis_module *module)
{
	if (!module) {
		pr_err("fail to get valid input ptr\n");
		return;
	}
	if (module->module_flag == DCAM_DEV_STATIS && module->idx == DCAM_ID_2)
		return;
	else if (module->module_flag == ISP_DEV_STATIS && module->idx > 1)
		return;

	if (module->module_flag == DCAM_DEV_STATIS) {
		sprd_cam_queue_buf_clear(&module->aem_statis_queue);
		sprd_cam_queue_buf_clear(&module->afl_statis_queue);
		sprd_cam_queue_buf_clear(&module->afm_statis_queue);
		sprd_cam_queue_buf_clear(&module->pdaf_statis_queue);
		sprd_cam_queue_buf_clear(&module->ebd_statis_queue);

		sprd_cam_queue_frm_clear(&module->aem_statis_frm_queue);
		sprd_cam_queue_frm_clear(&module->afl_statis_frm_queue);
		sprd_cam_queue_frm_clear(&module->afm_statis_frm_queue);
		sprd_cam_queue_frm_clear(&module->pdaf_statis_frm_queue);
		sprd_cam_queue_frm_clear(&module->ebd_statis_frm_queue);
	} else if (module->module_flag == ISP_DEV_STATIS) {
		sprd_cam_queue_buf_clear(&module->hist_statis_queue);
		sprd_cam_queue_frm_clear(&module->hist_statis_frm_queue);
	} else {
		pr_err("fail to get valid module:dcam or isp\n");
	}

	CAM_TRACE("statis buf clear ok.\n");
}

int sprd_cam_statistic_map(struct cam_statis_buf *statis_buf)
{
	int ret = 0;

	if (!statis_buf) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	ret = sprd_cam_buf_sg_table_get(&statis_buf->buf_info);
	if (ret) {
		pr_err("fail to get buf sg table\n");
		return ret;
	}
	ret = sprd_cam_buf_addr_map(&statis_buf->buf_info);
	if (ret) {
		pr_err("fail to map buf addr\n");
		return ret;
	}

	return ret;
}

void sprd_cam_statistic_unmap(struct cam_statis_buf *statis_buf)
{
	if (!statis_buf) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	sprd_cam_buf_addr_unmap(&statis_buf->buf_info);
	memset(&statis_buf->buf_info, 0x00, sizeof(statis_buf->buf_info));
}

int sprd_cam_statistic_buf_cfg(
	struct cam_statis_module *module,
	struct isp_statis_buf_input *parm)
{
	int ret = ISP_RTN_SUCCESS;
	uint32_t iova_addr = 0;
	uint32_t vir_addr = 0;
	unsigned long kaddr = 0;
	uint32_t addr_offset = 0;
	struct cam_statis_buf frm_statis_head;

	if (!module || !parm) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	if (module->module_flag == DCAM_DEV_STATIS && module->idx == DCAM_ID_2)
		return 0;
	else if (module->module_flag == ISP_DEV_STATIS && module->idx > 1)
		return 0;

	module->statis_valid = parm->statis_valid;
	pr_info("statistic_queue %d flag, mask 0x%x 0x%x\n",
		module->idx, module->module_flag,
		module->statis_valid);

	if (module->module_flag == ISP_DEV_STATIS) {
		uint32_t dcam_statis_size = 0;
		struct cam_buf_info *buf_info = NULL;

		dcam_statis_size = parm->dcam_stat_buf_size;
		memset((void *)&module->img_statis_buf, 0,
			sizeof(module->img_statis_buf));
		memset((void *)&frm_statis_head, 0x00, sizeof(frm_statis_head));
		frm_statis_head.phy_addr = parm->phy_addr + dcam_statis_size;
		frm_statis_head.vir_addr = parm->vir_addr + dcam_statis_size;
		frm_statis_head.buf_size = parm->buf_size -
			parm->dcam_stat_buf_size;
		frm_statis_head.buf_property = parm->buf_property;
		buf_info = &frm_statis_head.buf_info;
		buf_info->idx = module->idx;
		buf_info->dev = &s_isp_pdev->dev;
		buf_info->num = 1;
		buf_info->mfd[0] = parm->mfd;
		ret = sprd_ion_get_buffer(buf_info->mfd[0],
					  NULL,
					  &buf_info->buf[0],
					  &buf_info->size[0]);
		if (ret) {
			pr_err("fail to get sg table mfd 0x%x\n",
			       buf_info->mfd[0]);
			return -EFAULT;
		}
		memcpy(&module->img_statis_buf, &frm_statis_head,
		       sizeof(struct cam_statis_buf));

		/*hist statis buf cfg*/
		addr_offset = dcam_statis_size;
		iova_addr = frm_statis_head.phy_addr;
		vir_addr = frm_statis_head.vir_addr;
#ifdef CONFIG_64BIT
		kaddr = (unsigned long)parm->kaddr[0]
			| ((unsigned long)parm->kaddr[1] << 32);
#else
		kaddr = (unsigned long)parm->kaddr[0];
#endif
		pr_info("kaddr[0]=0x%x, kaddr[1]= 0x%x\n",
			parm->kaddr[0],
			parm->kaddr[1]);
		kaddr += addr_offset;
		if (parm->statis_valid & ISP_STATIS_VALID_HIST)
			sprd_camstatistic_buf_split(&frm_statis_head,
				&module->hist_statis_queue,
				NULL,
				&s_isp_pdev->dev,
				ISP_HIST_STATIS_BUF_NUM,
				ISP_HIST_STATIS_BUF_SIZE,
				ISP_HIST_BLOCK,
				iova_addr,
				vir_addr,
				kaddr,
				&addr_offset,
				0);

		return ret;
	}

	memset((void *)&module->img_statis_buf, 0,
		sizeof(module->img_statis_buf));

	memset((void *)&frm_statis_head, 0x00, sizeof(frm_statis_head));
	frm_statis_head.phy_addr = parm->phy_addr;
	frm_statis_head.vir_addr = parm->vir_addr;
	frm_statis_head.buf_size = parm->dcam_stat_buf_size;
	frm_statis_head.buf_property = parm->buf_property;
	frm_statis_head.buf_info.idx = module->idx;
	frm_statis_head.buf_info.dev = &s_dcam_pdev->dev;
	frm_statis_head.buf_info.num = 1;
	frm_statis_head.buf_info.mfd[0] = parm->mfd;

	ret = sprd_cam_statistic_map(&frm_statis_head);
	if (ret) {
		pr_err("fail to map cam statistic\n");
		return ret;
	}

	memcpy(&module->img_statis_buf, &frm_statis_head,
		sizeof(struct cam_statis_buf));
	iova_addr = frm_statis_head.buf_info.iova[0];
	vir_addr = parm->vir_addr;
#ifdef CONFIG_64BIT
	kaddr = (unsigned long)parm->kaddr[0]
		| ((unsigned long)parm->kaddr[1] << 32);
#else
	kaddr = (unsigned long)parm->kaddr[0];
#endif
	pr_debug("kaddr[0]=0x%x, kaddr[1]= 0x%x\n",
		parm->kaddr[0],
		parm->kaddr[1]);

	if (parm->statis_valid & ISP_STATIS_VALID_AEM)
		sprd_camstatistic_buf_split(&frm_statis_head,
			&module->aem_statis_queue,
			&module->aem_buf_reserved,
			&s_dcam_pdev->dev,
			ISP_AEM_STATIS_BUF_NUM,
			ISP_AEM_STATIS_BUF_SIZE,
			ISP_AEM_BLOCK,
			iova_addr + addr_offset,
			vir_addr + addr_offset,
			kaddr + addr_offset,
			&addr_offset,
			1);

	if (parm->statis_valid & ISP_STATIS_VALID_AFL)
		sprd_camstatistic_buf_split(&frm_statis_head,
			&module->afl_statis_queue,
			&module->afl_buf_reserved,
			&s_dcam_pdev->dev,
			ISP_AFL_STATIS_BUF_NUM,
			ISP_AFL_STATIS_BUF_SIZE,
			ISP_AFL_BLOCK,
			iova_addr + addr_offset,
			vir_addr + addr_offset,
			kaddr + addr_offset,
			&addr_offset,
			1);

	if (parm->statis_valid & ISP_STATIS_VALID_AFM)
		sprd_camstatistic_buf_split(&frm_statis_head,
			&module->afm_statis_queue,
			&module->afm_buf_reserved,
			&s_dcam_pdev->dev,
			ISP_AFM_STATIS_BUF_NUM,
			ISP_AFM_STATIS_BUF_SIZE,
			ISP_AFM_BLOCK,
			iova_addr + addr_offset,
			vir_addr + addr_offset,
			kaddr + addr_offset,
			&addr_offset,
			1);

	if (parm->statis_valid & ISP_STATIS_VALID_PDAF)
		sprd_camstatistic_buf_split(&frm_statis_head,
			&module->pdaf_statis_queue,
			&module->pdaf_buf_reserved,
			&s_dcam_pdev->dev,
			ISP_PDAF_STATIS_BUF_NUM,
			ISP_PDAF_STATIS_BUF_SIZE,
			ISP_PDAF_BLOCK,
			iova_addr + addr_offset,
			vir_addr + addr_offset,
			kaddr + addr_offset,
			&addr_offset,
			1);

	if (parm->statis_valid & ISP_STATIS_VALID_EBD)
		sprd_camstatistic_buf_split(&frm_statis_head,
					&module->ebd_statis_queue,
					&module->ebd_buf_reserved,
					&s_dcam_pdev->dev,
					ISP_EBD_STATIS_BUF_NUM,
					ISP_EBD_STATIS_BUF_SIZE,
					ISP_EBD_BLOCK,
					iova_addr + addr_offset,
					vir_addr + addr_offset,
					kaddr + addr_offset,
					&addr_offset,
					1);

	CAM_TRACE("cfg statis buf out\n");
	return ret;
}

int sprd_cam_statistic_next_buf_set(
			struct cam_statis_module *module,
			enum isp_3a_block_id block_index,
			uint32_t frame_id)
{
	int rtn = 0;
	int use_reserve_buf = 0;
	struct cam_frm_queue *statis_heap = NULL;
	struct cam_buf_queue *p_buf_queue = NULL;
	struct cam_statis_buf *reserved_buf = NULL;
	struct cam_statis_buf node;
	int idx = module->idx;

	if (module->module_flag == DCAM_DEV_STATIS && module->idx == DCAM_ID_2)
		return 0;
	else if (module->module_flag == ISP_DEV_STATIS && module->idx > 1)
		return 0;

	memset(&node, 0x00, sizeof(node));
	if (block_index == ISP_AEM_BLOCK) {
		if (!(module->statis_valid & ISP_STATIS_VALID_AEM))
			return rtn;
		p_buf_queue = &module->aem_statis_queue;
		statis_heap = &module->aem_statis_frm_queue;
		reserved_buf = &module->aem_buf_reserved;
	} else if (block_index == ISP_AFL_BLOCK) {
		if (!(module->statis_valid & ISP_STATIS_VALID_AFL))
			return rtn;
		p_buf_queue = &module->afl_statis_queue;
		statis_heap = &module->afl_statis_frm_queue;
		reserved_buf = &module->afl_buf_reserved;
	} else if (block_index == ISP_PDAF_BLOCK) {
		if (!(module->statis_valid & ISP_STATIS_VALID_PDAF))
			return rtn;
		p_buf_queue = &module->pdaf_statis_queue;
		statis_heap = &module->pdaf_statis_frm_queue;
		reserved_buf = &module->pdaf_buf_reserved;
	} else if (block_index == ISP_EBD_BLOCK) {
		if (!(module->statis_valid & ISP_STATIS_VALID_EBD))
			return rtn;
		p_buf_queue = &module->ebd_statis_queue;
		statis_heap = &module->ebd_statis_frm_queue;
		reserved_buf = &module->ebd_buf_reserved;
	} else if (block_index == ISP_AFM_BLOCK) {
		if (!(module->statis_valid & ISP_STATIS_VALID_AFM))
			return rtn;
		p_buf_queue = &module->afm_statis_queue;
		statis_heap = &module->afm_statis_frm_queue;
		reserved_buf = &module->afm_buf_reserved;
	} else if (block_index == ISP_HIST_BLOCK) {
		if (!(module->statis_valid & ISP_STATIS_VALID_HIST))
			return rtn;
		p_buf_queue = &module->hist_statis_queue;
		statis_heap = &module->hist_statis_frm_queue;
	}

	/*read buf addr from in_buf_queue*/
	if (sprd_cam_queue_buf_read(p_buf_queue, &node) != 0) {
		CAM_TRACE("NO type %d free statis buf\n", block_index);
		/*use reserved buffer*/
		if (reserved_buf == NULL)
			return -EPERM;
		if (!sprd_cam_buf_is_valid(&reserved_buf->buf_info))
			pr_info("NO need to cfg cam%d type %d statis buf\n",
				idx, block_index);

		memcpy(&node, reserved_buf, sizeof(struct cam_statis_buf));
		use_reserve_buf = 1;
	}

	if (node.buf_info.dev == NULL)
		pr_info("dev is NULL.\n");

	node.frame_id = frame_id;

	/*enqueue the statis buf into the array*/
	rtn = sprd_cam_queue_frm_enqueue(statis_heap, &node);
	if (rtn) {
		pr_err("fail to enqueue statis buf\n");
		return rtn;
	}
	/*update buf addr to isp ddr addr*/
	if (block_index == ISP_AEM_BLOCK) {
		DCAM_REG_WR(idx, DCAM_AEM_BASE_WADDR, node.phy_addr);
	} else if (block_index == ISP_AFL_BLOCK) {
		DCAM_REG_WR(idx,
			ISP_ANTI_FLICKER_GLB_WADDR, node.phy_addr);
		DCAM_REG_WR(idx, ISP_ANTI_FLICKER_REGION_WADDR,
				(node.phy_addr + node.buf_size / 2));

		DCAM_REG_MWR(idx, ISP_ANTI_FLICKER_NEW_CFG_READY, BIT_0, 1);
	} else if (block_index == ISP_AFM_BLOCK) {
		DCAM_REG_WR(idx, ISP_RAW_AFM_ADDR, node.phy_addr);
	} else if (block_index == ISP_PDAF_BLOCK) {
		DCAM_REG_WR(idx, DCAM_PDAF_BASE_WADDR, node.phy_addr);
		DCAM_REG_WR(idx, DCAM_VCH2_BASE_WADDR,
				node.phy_addr + node.buf_size / 2);
	} else if (block_index == ISP_EBD_BLOCK) {
		DCAM_REG_WR(idx, DCAM_VCH3_BASE_WADDR, node.phy_addr);
	}

	return rtn;
}

int sprd_cam_statistic_addr_set(struct cam_statis_module *module,
	struct isp_statis_buf_input *parm)
{
	int ret = 0;
	struct cam_statis_buf frm_statis;
	struct cam_statis_buf *statis_buf_reserved = NULL;
	struct cam_buf_queue *statis_queue = NULL;
	int select_device = 0;

	if (!module || !parm) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	if (module->module_flag == DCAM_DEV_STATIS && module->idx == DCAM_ID_2)
		return 0;
	else if (module->module_flag == ISP_DEV_STATIS && module->idx > 1)
		return 0;

	switch (parm->buf_property) {
	case ISP_AEM_BLOCK:
		statis_queue = &module->aem_statis_queue;
		statis_buf_reserved = &module->aem_buf_reserved;
		break;
	case ISP_AFL_BLOCK:
		statis_queue = &module->afl_statis_queue;
		statis_buf_reserved = &module->afl_buf_reserved;
		break;
	case ISP_AFM_BLOCK:
		statis_queue = &module->afm_statis_queue;
		statis_buf_reserved = &module->afm_buf_reserved;
		break;
	case ISP_PDAF_BLOCK:
		statis_queue = &module->pdaf_statis_queue;
		statis_buf_reserved = &module->pdaf_buf_reserved;
		break;
	case ISP_HIST_BLOCK:
		statis_queue = &module->hist_statis_queue;
		select_device = 1;
		break;
	case ISP_EBD_BLOCK:
		statis_queue = &module->ebd_statis_queue;
		statis_buf_reserved = &module->ebd_buf_reserved;
		break;
	default:
		pr_err("fail to get statis block %d\n", parm->buf_property);
		return -EFAULT;
	}

	memset((void *)&frm_statis, 0x00, sizeof(frm_statis));
	frm_statis.phy_addr = parm->phy_addr;
	frm_statis.vir_addr = parm->vir_addr;
	frm_statis.addr_offset = parm->addr_offset;
	frm_statis.kaddr[0] = parm->kaddr[0];
	frm_statis.kaddr[1] = parm->kaddr[1];
	frm_statis.buf_size = parm->buf_size;
	frm_statis.buf_property = parm->buf_property;

	frm_statis.buf_info.dev =
		select_device ? &s_isp_pdev->dev : &s_dcam_pdev->dev;
	frm_statis.buf_info.mfd[0] = parm->reserved[0];
	/*when the statis is running, we need not map again*/
	ret = sprd_cam_queue_buf_write(statis_queue, &frm_statis);

	CAM_TRACE("set statis buf addr done.\n");
	return ret;
}

int sprd_cam_statistic_buf_set(struct cam_statis_module *module)
{
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	struct dcam_module *module_dev = NULL;
	int idx = 0;

	module_dev = container_of(module, struct dcam_module,
				statis_module_info);

	if (!module) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	if (module->module_flag == DCAM_DEV_STATIS && module->idx == DCAM_ID_2)
		return 0;
	else if (module->module_flag == ISP_DEV_STATIS && module->idx > 1)
		return 0;

	idx = module->idx;

	if (module->statis_valid & ISP_STATIS_VALID_AEM) {
		rtn = sprd_cam_statistic_next_buf_set(module,
				ISP_AEM_BLOCK, 0);
		if (rtn) {
			pr_err("fail to set next AEM statis buf\n");
			return -(rtn);
		}
		sprd_dcam_drv_force_copy(idx, AEM_COPY);
	}

	if (module->statis_valid & ISP_STATIS_VALID_AFL) {
		rtn = sprd_cam_statistic_next_buf_set(module,
				ISP_AFL_BLOCK, 0);
		if (rtn) {
			pr_err("fail to set next AFL statis buf\n");
			return -(rtn);
		}
		sprd_dcam_drv_force_copy(idx, AEM_COPY);
	}

	if (module->statis_valid & ISP_STATIS_VALID_AFM) {
		rtn = sprd_cam_statistic_next_buf_set(module,
				ISP_AFM_BLOCK, 0);
		if (rtn) {
			pr_err("fail to set next AFM statis buf\n");
			return -(rtn);
		}
		sprd_dcam_drv_force_copy(idx, BIN_COPY);
	}

	if (module->statis_valid & ISP_STATIS_VALID_PDAF) {
		rtn = sprd_cam_statistic_next_buf_set(module,
			ISP_PDAF_BLOCK, 0);
		if (rtn) {
			pr_err("fail to set next pdaf statis buf\n");
			return -(rtn);
		}
		sprd_dcam_drv_force_copy(idx, PDAF_COPY);
		sprd_dcam_drv_force_copy(idx, VCH2_COPY);
	}

	if (module->statis_valid & ISP_STATIS_VALID_EBD) {
		rtn = sprd_cam_statistic_next_buf_set(module,
				ISP_EBD_BLOCK, 0);
		if (rtn) {
			pr_err("fail to set next ebd statis buf\n");
			return -(rtn);
		}
		sprd_dcam_drv_force_copy(idx, VCH3_COPY);
	}

	CAM_TRACE("set statis buf done.\n");
	return rtn;
}
