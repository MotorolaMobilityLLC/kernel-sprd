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

#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "cam_dbg.h"
#include "dcam_core.h"
#include "isp_drv.h"
#include "dcam_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CAM_DBG: %d: %d %s:" fmt, current->pid, __LINE__, __func__

#define CAMERA_DUMP_PATH	"/data/ylog/"
#define BYTES_DUMP_PER_TRY	4096

#define get_raw10_pic_size(width, height) ((width)*(height)*5/4)

/* Make a decision if we use bin_lock or full_lock */
#define sel_lock_dump_isp_input(scene, dump_info, lock) \
	do { \
		if ((scene) == ISP_SCENE_PRE) \
			lock = &(dump_info)->bin_lock; \
		else if ((scene) == ISP_SCENE_CAP) \
			lock = &(dump_info)->full_lock; \
		else lock = NULL; \
	} while (0)

#define ISP_SBLK_MAP_SIZE (sizeof(int) * BITS_PER_BYTE) /* unit: bit */

/* The sub_block sit at which map, or which row? */
#define isp_get_sblk_map(nu) ((nu) / ISP_SBLK_MAP_SIZE)

/* The sub_block sit at which position in the map, or which col? */
#define isp_get_sblk_site(nu) ((nu) % ISP_SBLK_MAP_SIZE)

/* Update the specified bit in bitmap */
#define isp_up_sblk_map(map, bit_site, byp_flag) \
	(((map) & ~(1 << (bit_site))) | ((byp_flag) << (bit_site)))

#define MAX_WORD_DUMP_INTO_LOG	0x200
#define SBLK_NAME(name)		#name
#define SYSCMD_INPUT_MAX_CHAR	4

struct isp_sub_blk_base {
	const char *name;
	unsigned long base_addr;
};

struct dcam_dbg_info {
	bool dbg_on;
};

struct isp_dbg_info {
	bool dbg_on; /* global switch to contrl isp debug */

	/* the other switch below depends on dbg_on */
	bool fmcu_dbg_on;
	bool int_dbg_on;
	bool dump_input_on;

	/*
	 * Use this value to record the max word size
	 * to dump the isp regs, and the remainings will
	 * be collected by another thread, now in todo list.
	 */
	uint32_t dump2log_max_word_size;

	/* start and end address, total 3 sets */
	uint32_t dump_range[6];

	/*
	 * used to save the bypass/work configs from sysfs,
	 * and map sub-blocks of isp, each bit for one
	 * sub-block.
	 */
	uint32_t sblk_maps[ISP_SBLK_MAP_CNT];
	uint32_t sblk_cnt;
	struct isp_sub_blk_base *sblk_base;
};

struct cam_dbg_info {
	struct dcam_dbg_info dcam_dbg;
	struct isp_dbg_info isp_dbg;
};

#define ISP_INVALID_REG_ADDR		0xBADBEEF
/*
 * isp sub_block table, not including path related blocks.
 * New adding sub_block should before the "all" item.
 */
static struct isp_sub_blk_base isp_sblk_base[] __aligned(8) = {
	/* RAW RGB */
	{SBLK_NAME(raw_pgg),	ISP_PGG_PARAM},
	{SBLK_NAME(raw_blc),	ISP_BLC_PARAM},
	{SBLK_NAME(raw_rgbg),	ISP_RGBG_PARAM},
	{SBLK_NAME(raw_rgbd),	ISP_RGBG_PARAM0},
	{SBLK_NAME(raw_postblc),ISP_POST_BLC_PARA},
	{SBLK_NAME(raw_nlc),	ISP_NLC_PARA},
	{SBLK_NAME(raw_2dlsc),	ISP_LENS_PARAM},
	{SBLK_NAME(raw_bin),	ISP_BINNING_PARAM},
	{SBLK_NAME(raw_awb),	ISP_AWBC_PARAM},
	{SBLK_NAME(raw_aem),	ISP_AEM_PARAM},
	{SBLK_NAME(raw_bpc),	ISP_BPC_PARAM},
	{SBLK_NAME(raw_grgbc),	ISP_GRGB_CTRL},
	{SBLK_NAME(raw_vst),	ISP_VST_PARA},
	{SBLK_NAME(raw_nlm),	ISP_NLM_PARA},
	{SBLK_NAME(raw_ivst),	ISP_IVST_PARA},
	{SBLK_NAME(raw_rlsc),	ISP_RLSC_CTRL},
	{SBLK_NAME(raw_afm),	ISP_RGB_AFM_PARAM},
	{"all_raw", ISP_INVALID_REG_ADDR},

	/* FULL RGB */
	{SBLK_NAME(full_cmc),	ISP_CMC10_PARAM},
	{SBLK_NAME(full_gama),	ISP_GAMMA_PARAM},
	{SBLK_NAME(full_hsv),	ISP_HSV_PARAM},
	{SBLK_NAME(full_pstrz),	ISP_PSTRZ_PARAM},
	{SBLK_NAME(full_uvd),	ISP_UVD_PARAM},
	{"all_full", ISP_INVALID_REG_ADDR},

	/* YUV */
	{SBLK_NAME(yuv_afl),	ISP_ANTI_FLICKER_NEW_PARAM0},
	{SBLK_NAME(yuv_precdn),	ISP_PRECDN_PARAM},
	{SBLK_NAME(yuv_ynr),	ISP_YNR_CTRL0},
	{SBLK_NAME(yuv_brta),	ISP_BRIGHT_PARAM},
	{SBLK_NAME(yuv_cnta),	ISP_CONTRAST_PARAM},
	{SBLK_NAME(yuv_hist),	ISP_HIST_PARAM},
	{SBLK_NAME(yuv_hist2),	ISP_HIST2_PARAM},
	{SBLK_NAME(yuv_cdn),	ISP_CDN_PARAM},
	{SBLK_NAME(yuv_edge),	ISP_EE_PARAM},
	{SBLK_NAME(yuv_csa),	ISP_CSA_PARAM},
	{SBLK_NAME(yuv_hua),	ISP_HUA_PARAM},
	{SBLK_NAME(yuv_postcdn),ISP_POSTCDN_COMMON_CTRL},
	{SBLK_NAME(yuv_gama),	ISP_YGAMMA_PARAM},
	{SBLK_NAME(yuv_iircnr),	ISP_IIRCNR_PARAM},
	{SBLK_NAME(yuv_random),	ISP_YRANDOM_PARAM1},
	{SBLK_NAME(yuv_nf),	ISP_YUV_NF_CTRL},
	{"all_yuv", ISP_INVALID_REG_ADDR},

	/* TODO: add other sub blocks before "all" */

	/* used to control all sub_block in this table */
	{"all",	ISP_INVALID_REG_ADDR},
};

static inline struct camera_group *dev_to_camgrp(struct device *dev)
{
	return (struct camera_group *)dev->platform_data;
}

static inline struct cam_dbg_info *camgrp_to_camdbg(struct camera_group *grp)
{
	return (struct cam_dbg_info *)grp->p_dbg_info;
}

static inline struct camera_group *camdev_to_camgrp(struct camera_dev *camdev)
{
	return (struct camera_group *)camdev->cam_grp;
}

static inline struct camera_dev *ispdev_to_camdev(struct isp_pipe_dev *ispdev)
{
	return ispdev->cam_grp->dev[ISP_GET_IID(ispdev->com_idx)];
}

static inline struct camera_dbg_dump *to_bin_dumpinfo(struct work_struct *work)
{
	return container_of(work, struct camera_dbg_dump, dump_bin_work);
}

static inline struct camera_dbg_dump *to_full_dumpinfo(struct work_struct *work)
{
	return container_of(work, struct camera_dbg_dump, dump_full_work);
}

static struct file *_open_file_frame_dump(char *path, struct camera_frame *frm,
					  uint32_t idx, uint32_t scene)
{
	struct file *fp = NULL;

	if (unlikely(!path || !frm))
		return NULL;

	sprintf(path, CAMERA_DUMP_PATH"cam%d_s%d_m%d_%dx%d_frame%d.mipi_raw",
		ISP_GET_IID(idx), scene, ISP_GET_MID(idx),
		frm->width, frm->height, frm->fid);

	fp = filp_open(path, O_CREAT|O_RDWR, 0600);
	if (IS_ERR_OR_NULL(fp)) {
		pr_err("fail to open file %s, err:%ld\n", path, PTR_ERR(fp));
		return NULL;
	}

	return fp;
}

static void _write_file_frame_dump(struct file *fp, char *buf, ssize_t size)
{
	ssize_t result = 0, ws = 0;

	if (!fp || !buf || size ==0)
		return;

	do {
		ws = (BYTES_DUMP_PER_TRY < size) ? BYTES_DUMP_PER_TRY : size;
		result = kernel_write(fp, buf, ws, &fp->f_pos);
		if (result > 0) {
			size -= result;
			buf += result;
		}
	} while ((result > 0) && (size > 0));
}

static void _close_file_frame_dump(struct file *fp)
{
	if (!fp)
		return;

	filp_close(fp, NULL);
}

static void dump_into_file(enum isp_scene_id scene,
			   struct camera_dbg_dump *dump_info, ssize_t size)
{
	char path[80] = "";
	struct file *fp = NULL;
	char *buf = NULL;
	uint32_t idx = 0;
	struct mutex *lock = NULL;
	struct camera_frame *frame = NULL;
	uint32_t *cnt = NULL;

	if(!dump_info || size == 0)
		return;

	if (scene == ISP_SCENE_PRE) {
		cnt = &dump_info->bin_cnt;
		frame = dump_info->bin_frame;
	} else if (scene == ISP_SCENE_CAP) {
		cnt = &dump_info->full_cnt;
		frame = dump_info->full_frame;
	} else
		return;

	if (!frame || !frame->kva)
		return;

	buf = (char *)frame->kva;
	idx = frame->cam_id;

	sel_lock_dump_isp_input(scene, dump_info, lock);
	if (!lock)
		return;

	mutex_lock(lock);
	fp = _open_file_frame_dump(path, frame, idx, scene);
	if (!fp)
		goto exit;

	_write_file_frame_dump(fp, buf, size);
	_close_file_frame_dump(fp);

	if (scene == ISP_SCENE_PRE)
		dump_info->bin_frame = NULL;
	else if (scene == ISP_SCENE_CAP)
		dump_info->full_frame = NULL;

	pr_debug("idx%x dump %d frame done\n", frame->cam_id, *cnt++);

exit:
	mutex_unlock(lock);
	return;
}

static void dump_isp_input_bin(struct work_struct *work)
{
	struct camera_dbg_dump *dump_info = to_bin_dumpinfo(work);
	uint32_t size = get_raw10_pic_size(dump_info->bin_frame->width,
					   dump_info->bin_frame->height);

	dump_into_file(ISP_SCENE_PRE, dump_info, size);
}

static void dump_isp_input_full(struct work_struct *work)
{
	struct camera_dbg_dump *dump_info = to_full_dumpinfo(work);
	uint32_t size = get_raw10_pic_size(dump_info->full_frame->width,
					   dump_info->full_frame->height);

	dump_into_file(ISP_SCENE_CAP, dump_info, size);
}

int isp_dbg_dump_input_init(void *isp_dev)
{
	struct isp_pipe_dev *ispdev = (struct isp_pipe_dev *)isp_dev;
	struct camera_dev *camdev = NULL;
	struct camera_dbg_dump *dump_info = NULL;
	struct workqueue_struct *wq = NULL;

	if (ispdev && ispdev->cam_grp && ispdev_to_camdev(ispdev)) {
		camdev = ispdev_to_camdev(ispdev);
		dump_info = &camdev->dcam_cxt.dump_info;
	} else
		return -ENODEV;

	if (dump_info->is_inited)
		return 0;

	mutex_init(&camdev->dcam_cxt.dump_info.bin_lock);
	mutex_init(&camdev->dcam_cxt.dump_info.full_lock);
	wq = create_singlethread_workqueue("dump_work_queue");
	if (unlikely(IS_ERR_OR_NULL(wq))) {
		mutex_destroy(&camdev->dcam_cxt.dump_info.bin_lock);
		mutex_destroy(&camdev->dcam_cxt.dump_info.full_lock);
		pr_err("fail to create dump work queue, err%ld\n", PTR_ERR(wq));
		return -EFAULT;
	}

	camdev->dcam_cxt.dump_info.dump_work_queue = wq;
	INIT_WORK(&camdev->dcam_cxt.dump_info.dump_bin_work,
		  dump_isp_input_bin);
	INIT_WORK(&camdev->dcam_cxt.dump_info.dump_full_work,
		  dump_isp_input_full);
	dump_info->is_inited = true;

	pr_info("init done\n");
	return 0;
}

int isp_dbg_dump_input_deinit(void *isp_dev)
{
	struct isp_pipe_dev *ispdev = (struct isp_pipe_dev *)isp_dev;
	struct camera_dev *camdev = NULL;
	struct camera_dbg_dump *dump_info = NULL;

	if (ispdev && ispdev->cam_grp)
		camdev = ispdev_to_camdev(ispdev);
	else
		return -ENODEV;

	if (camdev) {
		dump_info = &camdev->dcam_cxt.dump_info;
		if (!dump_info->is_inited)
			return 0;
		mutex_lock(&camdev->dcam_cxt.dump_info.bin_lock);
		mutex_lock(&camdev->dcam_cxt.dump_info.full_lock);

		destroy_workqueue(camdev->dcam_cxt.dump_info.dump_work_queue);

		mutex_unlock(&camdev->dcam_cxt.dump_info.full_lock);
		mutex_destroy(&camdev->dcam_cxt.dump_info.full_lock);

		mutex_unlock(&camdev->dcam_cxt.dump_info.bin_lock);
		mutex_destroy(&camdev->dcam_cxt.dump_info.bin_lock);

		dump_info->is_inited = false;
		pr_info("deinit done\n");
	} else
		return -ENODEV;

	return 0;
}

void isp_dbg_trigger_dump_input(void *isp_dev, void *frame, uint32_t com_idx)
{
	struct isp_pipe_dev *ispdev = (struct isp_pipe_dev *)isp_dev;
	struct camera_dev *camdev = NULL;
	struct camera_dbg_dump *dump_info = NULL;
	struct mutex *lock = NULL;
	enum isp_scene_id scene = ISP_GET_SID(com_idx);

	if (!ispdev || !frame ||
	    !isp_dbg_check_switch_on((void *)ispdev, ISP_DUMP_INPUT))
		return;

	camdev = ispdev_to_camdev(ispdev);
	if (camdev)
		dump_info = &camdev->dcam_cxt.dump_info;
	else {
		pr_err("fail to init\n");
		return;
	}

	sel_lock_dump_isp_input(scene, dump_info, lock);
	if (!lock)
		return;

	if (!mutex_trylock(lock)) {
		pr_info("skip this frame %d dump\n",
			((struct camera_frame *)frame)->fid);
		return;
	}

	if (scene == ISP_SCENE_PRE) {
		dump_info->bin_frame = frame;
		queue_work(dump_info->dump_work_queue,
			   &dump_info->dump_bin_work);
	} else if (scene == ISP_SCENE_CAP) {
		dump_info->full_frame = frame;
		queue_work(dump_info->dump_work_queue,
			   &dump_info->dump_full_work);
	} else {
		pr_err("fail to check scene %d \n", scene);
	}

	mutex_unlock(lock);
	return;
}

static void dump_isp_hw_regs(struct isp_pipe_dev *dev, unsigned long start_addr,
			     unsigned long end_addr)
{
	unsigned long addr = 0;
	int idx = 0;

	if (!dev)
		return;

	idx = dev->com_idx;
	pr_info("--ISP HW REGS [%d][%d]--\n"
		"cb:%pS\n", ISP_GET_SID(idx), ISP_GET_IID(idx),
		__builtin_return_address(0));
	for (addr = start_addr; addr <= end_addr; addr += 16) {
		pr_info("0x%5lx:	0x%8x  %8x  %8x  %8x\n",
			addr,
			ISP_HREG_RD(idx, addr),
			ISP_HREG_RD(idx, addr + 4),
			ISP_HREG_RD(idx, addr + 8),
			ISP_HREG_RD(idx, addr + 12));
	}
}

static void dump_isp_page_regs(struct isp_pipe_dev* dev,
			       unsigned long start_addr,
			       unsigned long end_addr)
{
	unsigned long addr = 0;
	int idx = 0;

	if (!dev)
		return;

	idx = dev->com_idx;
	pr_info("--ISP PAGE REGS [%d][%d]--\n"
		"cb:%pS\n", ISP_GET_SID(idx), ISP_GET_IID(idx),
		__builtin_return_address(0));
	for (addr = start_addr; addr <= end_addr; addr += 16) {
		pr_info("0x%5lx:	0x%8x  %8x  %8x  %8x\n",
			addr,
			ISP_PAGE_REG_RD(idx, addr),
			ISP_PAGE_REG_RD(idx, addr + 4),
			ISP_PAGE_REG_RD(idx, addr + 8),
			ISP_PAGE_REG_RD(idx, addr + 12));
	}
}

static void dump_isp_fmcu_cmd_q(struct isp_pipe_dev *dev)
{
	int i;
	unsigned long addr = 0;
	uint32_t nu = 0;

	if (!dev)
		return;

	nu = dev->fmcu_slice.fmcu_num;
	addr = (unsigned long)dev->fmcu_addr_vir;

	pr_info("--FMCU cmd queue [%d] --\n"
		"fmcu slice cmd num total:%d, cb:%pS\n",
		ISP_GET_IID(dev->com_idx), nu,	__builtin_return_address(0));

	for (i = 0; i <= nu; i += 2) {
		pr_info("0x%5lx:        0x%08x 0x%08x 0x%08x 0x%08x\n",
			addr,
			REG_RD(addr),
			REG_RD(addr + 4),
			REG_RD(addr + 8),
			REG_RD(addr + 12));
		addr += 16;
	}
}

/*
 * get bypass_flag, setting from sysfs, in bitmap for one sub-block.
 *
 * @nu:		The number of bit in bitmap of isp sblk,
 *		from 0 to ISP_SBLK_CNT.
 * @byp_flag:	Save the bypass flag achieved from bitmap.
 */
static void isp_get_sblk_byp_flag(struct isp_dbg_info *p_ispdbg, uint32_t nu,
				  uint32_t byp_flag)
{
	uint32_t map, map_id, site;

	if (!p_ispdbg)
		return;

	if (byp_flag < SBLK_BYP_FLAG_CNT && nu < ISP_SBLK_CNT) {
		map_id = isp_get_sblk_map(nu);
		site = isp_get_sblk_site(nu);
		map = p_ispdbg->sblk_maps[map_id];
		byp_flag = (map & (1 << site)) >> site;
	}
}

/*
 * set bypass flag in bitmap for one sub-block, via sysfs dynamic control.
 *
 * @nu:		The number of bit in bitmap of isp sblk,
 *		from 0 to ISP_SBLK_CNT.
 * @byp_flag:	The bypass flag seting value: 0/1, from sysfs.
 */
static void isp_set_sblk_byp_flag(struct isp_dbg_info *p_ispdbg, uint32_t nu,
				  uint32_t byp_flag)
{
	uint32_t map_id = 0, site = 0, old_map = 0, new_map = 0;
	uint32_t i = 0, byp_start = 0, byp_end = 0;

	if (!p_ispdbg || nu > ISP_SBLK_CNT)
		return;

	if (byp_flag < SBLK_BYP_FLAG_CNT && nu < ISP_SBLK_CNT) {
		switch (nu) {
		case all_raw:
			byp_start = 0;
			byp_end = all_raw - 1;
			break;
		case all_full:
			byp_start = all_raw + 1;
			byp_end = all_full - 1;
			break;
		case all_yuv:
			byp_start = all_full + 1;
			byp_end = all_yuv - 1;
			break;
		default:
			byp_start = nu;
			byp_end = nu;
		}

		for (i = byp_start; i <= byp_end; i++) {
			map_id = isp_get_sblk_map(i);
			site = isp_get_sblk_site(i);
			old_map = p_ispdbg->sblk_maps[map_id];
			new_map = isp_up_sblk_map(old_map, site, byp_flag);
			p_ispdbg->sblk_maps[map_id] = new_map;
		}
	}
}

/*
 * set bypass flag from sysfs, for all sub-blocks,
 *
 * @byp_flag: the bypass flag seting value: 0/1, from sysfs.
 */
static void isp_set_all_sblk_byp_flag(struct isp_dbg_info *p_ispdbg,
				      uint32_t byp_flag)
{
	uint32_t new_map, map_id;
	uint32_t map_cnt = DIV_ROUND_UP(ISP_SBLK_CNT, ISP_SBLK_MAP_SIZE);

	if (!p_ispdbg)
		return;

	if (byp_flag < SBLK_BYP_FLAG_CNT) {
		if (byp_flag == SBLK_BYPASS)
			new_map = 0xffffffff;
		else
			new_map = 0x0;

		for (map_id = 0; map_id < map_cnt; map_id++)
			p_ispdbg->sblk_maps[map_id] = new_map;
	}
}

/*
 * set the original bypass state of each isp sub-blocks.
 *
 * @bid: the index of isp sub-block
 * @iid: the index of isp_dev
 */
void isp_dbg_s_ori_byp(uint32_t byp_flag, uint32_t bid, uint32_t iid)
{
	uint32_t map_id, site, old_map, new_map;
	struct isp_pipe_dev *isp_dev = NULL;

	if (CHECK_ID_VALID(iid) && bid < ISP_SBLK_CNT &&
	    byp_flag < SBLK_BYP_FLAG_CNT) {
		isp_dev = g_isp_dev_parray[iid];
		if (isp_dev) {
			map_id = isp_get_sblk_map(bid);
			site = isp_get_sblk_site(bid);
			old_map = isp_dev->sblk_ori_byp_map[map_id];
			new_map = isp_up_sblk_map(old_map, site, byp_flag);
			isp_dev->sblk_ori_byp_map[map_id] = new_map;
		}
	}
}

/*
 * This function will actually do the bypass or work on
 * isp sub block with the help of
 * sys/sprd_image/isp_sblk/<sblk_file>.
 *
 * TODO: maybe need to mask this dbg function for user version,
 * using macro like: CONFIG_CAM_DEBUG.
 */
void isp_dbg_bypass_sblk(void *isp_dev, uint32_t idx)
{
	struct isp_dbg_info *ispdbg = NULL;
	struct cam_dbg_info *camdbg = NULL;
	struct isp_pipe_dev *dev = (struct isp_pipe_dev *)isp_dev;
	uint32_t cur_map, ori_map = 0;
	uint32_t map_id, sblk_id = 0;
	uint32_t sblk_cnt, addr = 0;
	struct isp_sub_blk_base *sblk_base = NULL;
	uint32_t bypass = 0;
	int i = 0;

	if (!dev || !dev->cam_grp ||
	    !isp_dbg_check_switch_on((void *)dev, ISP_DBG_SW))
		return;

	pr_debug("enter, cb:%pS\n", __builtin_return_address(0));

	camdbg = camgrp_to_camdbg(dev->cam_grp);
	ispdbg = &camdbg->isp_dbg;
	sblk_cnt = ispdbg->sblk_cnt;
	sblk_base = ispdbg->sblk_base;

	for (map_id = 0; map_id < ISP_SBLK_MAP_CNT; map_id++) {
		/* generate the cur_map through sysfs operation */
		cur_map = ispdbg->sblk_maps[map_id];
		/* generate the ori_map through ioctl operation */
		ori_map = dev->sblk_ori_byp_map[map_id];

		pr_debug("map %d: cur_map: 0x%x, ori_map: 0x%x\n",
			 map_id, cur_map, ori_map);
		for (i = 0; i < ISP_SBLK_MAP_SIZE; i++) {
			sblk_id = map_id * ISP_SBLK_MAP_SIZE + i;

			/* walk through all the isp sub-blocks, time to go home */
			if (sblk_id == sblk_cnt)
				return;

			addr = sblk_base[sblk_id].base_addr;

			/*
			 * For the fake sub-blockblk: all_raw, all_full, all_yuv
			 * the reg addr is specified with ISP_INVALID_REG_ADDR,
			 * just jump high, haha :-), and ignore this fake block,
			 * then continue to walk forward.
			 *
			 * DON'T forget to move forward the bit position in bitmap.
			 */
			if (addr == ISP_INVALID_REG_ADDR) {
				cur_map >>= 1;
				continue;
			}

			/*
			 * Get the original bypass flag, if user has already
			 * set the bypass flag 1 from sysfs, updating hardware
			 * bypass regs using 1, or else using the original
			 * bypass flag.
			 */
			bypass = (ori_map & (1 << i)) >> i;
			if (cur_map & 1) {
				bypass = SBLK_BYPASS;
				pr_debug("isp: bypass %s, com_idx:0x%x\n",
					 sblk_base[sblk_id].name, idx);
			}

			ISP_REG_MWR(idx, addr, BIT_0, bypass);
			cur_map >>= 1;
		}
	}
	pr_debug("exit\n");
}

 /*
  * sys/sprd_image/dcam_dbg can control the dcam_dbg.dbg_on
  */
void dcam_dbg_reg_trace(void *cam_dev, uint32_t force_dump_flag)
{
	struct camera_dev *camdev = (struct camera_dev *)cam_dev;
	struct camera_group *grp = NULL;
	struct cam_dbg_info *camdbg = NULL;
	uint32_t addr = 0;
	uint32_t start_addr = DCAM0_CFG;
	uint32_t end_addr = DCAM0_APB_SRAM_CTRL;
	uint32_t idx = 0;

	if (camdev && camdev->cam_grp) {
		grp = camdev_to_camgrp(camdev);
		camdbg = camgrp_to_camdbg(grp);
		idx = camdev->idx;
	} else
		return;

	/* in interrupt context or set force, ignore the dbg switch check below */
	if (in_interrupt() || force_dump_flag == CAM_DBG_FORCE_DUMP_REGS)
		goto force_dump;

	if (!camdbg || !camdbg->dcam_dbg.dbg_on)
		return;

force_dump:
	pr_info("--DCAM HW REGS [%d]--\n"
		"cb:%pS\n", idx, __builtin_return_address(0));
	for (addr = start_addr; addr <= end_addr; addr += 16) {
		pr_info("0x%3x:	%8x  %8x  %8x  %8x\n",
			addr,
			DCAM_REG_RD(idx, addr),
			DCAM_REG_RD(idx, addr + 4),
			DCAM_REG_RD(idx, addr + 8),
			DCAM_REG_RD(idx, addr + 12));
	}
}

/*
 * sys/sprd_image/isp_dbg can control This
 * function's process, reg_start, reg_end...
 * This function can be called anywhere, according
 * to user's request.
 */
void isp_dbg_reg_trace(void *isp_dev, uint32_t idx)
{
	uint32_t i = 0;
	unsigned long reg_start, reg_end;
	uint32_t limit_size = 0;
	uint32_t to_log_word_size = 0;
	uint32_t remain_word_size = 0;
	struct isp_dbg_info *ispdbg = NULL;
	struct cam_dbg_info *camdbg = NULL;
	struct isp_pipe_dev *dev = (struct isp_pipe_dev *)isp_dev;

	if (!dev || !dev->cam_grp ||
	    !isp_dbg_check_switch_on((void *)dev, ISP_DBG_SW))
		return;

	pr_debug("enter, cb:%pS\n", __builtin_return_address(0));
	camdbg = camgrp_to_camdbg(dev->cam_grp);
	ispdbg = &camdbg->isp_dbg;
	limit_size = ispdbg->dump2log_max_word_size;

	for (i = 0; i < ARRAY_SIZE(ispdbg->dump_range); i += 2) {
		/*
		 * TODO: maybe necessary using a thread to process
		 * the remain_word_size and save the dump data into
		 * a specified file.
		 * Now I just ignore the remain and output warning.
		 */
		reg_start = ispdbg->dump_range[i];
		reg_end = ispdbg->dump_range[i+1];
		to_log_word_size = reg_end - reg_start + 1;
		if (reg_end == 0 || to_log_word_size == 0)
			return;

		if (to_log_word_size > limit_size) {
			remain_word_size = to_log_word_size - limit_size;
			reg_end = reg_start +  limit_size;
			pr_warn("larger than %d words, ignore the remainings\n",
				limit_size);
		}

		pr_info("====Dump ISP REG Section-%d, com_idx 0x%x, cb:%pS\n",
			i/2, idx, __builtin_return_address(0));

		dump_isp_hw_regs(dev, reg_start, reg_end);

		/* With the isp offline architecture, most of regs configed
		 * in DDR page buf.*/
		if (ISP_GET_MID(idx) == ISP_CFG_MODE)
			dump_isp_page_regs(dev, reg_start, reg_end);
	}
	pr_debug("exit\n");
}

/*
 * /sys/sprd_image/fmcu_dbg can control This
 * function's process.
 * This function can be called anywhere, according
 * to user's request.
 */
void isp_dbg_dump_fmcu_cmd_q(void *isp_dev)
{
	struct isp_pipe_dev *dev = (struct isp_pipe_dev *)isp_dev;;

	if (!dev || !dev->fmcu_addr_vir || !dev->cam_grp ||
	    !isp_dbg_check_switch_on((void *)dev, ISP_FMCU_DBG_SW))
		return;

	pr_debug("enter, cb:%pS\n", __builtin_return_address(0));
	dump_isp_hw_regs(dev, ISP_FMCU_STATUS0, ISP_FMCU_SW_TRIGGER);
	dump_isp_fmcu_cmd_q(dev);

	pr_debug("exit\n");
}

/*
 * Control isp debug,
 * This control is the premise of dumping isp regs and controling
 * isp sub_block's working state.
 *
 * TODO: maybe need to mask these dbg related function for user version,
 * using macro like: CONFIG_CAM_DEBUG.
 */
static ssize_t isp_dbg_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct camera_group *grp = dev_to_camgrp(dev);
	struct cam_dbg_info *camdbg = camgrp_to_camdbg(grp);

	if (camdbg->isp_dbg.dbg_on)
		return sprintf(buf, "on\n");
	else
		return sprintf(buf, "off\n");
}

static ssize_t isp_dbg_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct camera_group *grp = dev_to_camgrp(dev);
	struct cam_dbg_info *camdbg = camgrp_to_camdbg(grp);
	char tune[SYSCMD_INPUT_MAX_CHAR] = "";

	if (count > SYSCMD_INPUT_MAX_CHAR)
		return -EPERM;

	if(!sscanf(buf, "%s", tune))
		return -EINVAL;

	if (!strcmp("on", tune))
		camdbg->isp_dbg.dbg_on = true;
	else if (!strcmp("off", tune))
		camdbg->isp_dbg.dbg_on = false;
	else
		return -EINVAL;

	return strnlen(buf, count);
}
static DEVICE_ATTR_RW(isp_dbg);

/*
 * Control dumping isp fmcu cmd queue,
 * The premise of these sub_block's control is that isp_dbg is on,
 */
static ssize_t isp_fmcu_dbg_show(struct device *dev,
				 struct device_attribute *attr,  char *buf)
{
	struct camera_group *grp = dev_to_camgrp(dev);
	struct cam_dbg_info *camdbg = camgrp_to_camdbg(grp);

	if (!camdbg->isp_dbg.dbg_on)
		return sprintf(buf, "isp_dbg: off\n");

	if (camdbg->isp_dbg.fmcu_dbg_on)
		return sprintf(buf, "on\n");
	else
		return sprintf(buf, "off\n");

}

static ssize_t isp_fmcu_dbg_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct camera_group *grp = dev_to_camgrp(dev);
	struct cam_dbg_info *camdbg = camgrp_to_camdbg(grp);
	char tune[SYSCMD_INPUT_MAX_CHAR] = "";

	if (count > SYSCMD_INPUT_MAX_CHAR)
		return -EPERM;

	if (!camdbg->isp_dbg.dbg_on)
		return 0;

	if(!sscanf(buf, "%s", tune))
		return -EINVAL;

	if (!strcmp("on", tune))
		camdbg->isp_dbg.fmcu_dbg_on = true;
	else if (!strcmp("off", tune))
		camdbg->isp_dbg.fmcu_dbg_on = false;
	else
		return -EINVAL;

	return strnlen(buf, count);
}
static DEVICE_ATTR_RW(isp_fmcu_dbg);

/*
 * Control isp int debug,
 */
static ssize_t isp_int_dbg_show(struct device *dev,
				struct device_attribute *attr,  char *buf)
{
	struct camera_group *grp = dev_to_camgrp(dev);
	struct cam_dbg_info *camdbg = camgrp_to_camdbg(grp);

	if (camdbg->isp_dbg.int_dbg_on)
		return sprintf(buf, "on\n");
	else
		return sprintf(buf, "off\n");
}

static ssize_t isp_int_dbg_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct camera_group *grp = dev_to_camgrp(dev);
	struct cam_dbg_info *camdbg = camgrp_to_camdbg(grp);
	char tune[SYSCMD_INPUT_MAX_CHAR] = "";

	if (count > SYSCMD_INPUT_MAX_CHAR)
		return -EPERM;

	if (!camdbg->isp_dbg.dbg_on)
		return 0;

	if(!sscanf(buf, "%s", tune))
		return -EINVAL;

	if (!strcmp("on", tune))
		camdbg->isp_dbg.int_dbg_on = true;
	else if (!strcmp("off", tune))
		camdbg->isp_dbg.int_dbg_on = false;
	else
		return -EINVAL;

	return strnlen(buf, count);
}
static DEVICE_ATTR_RW(isp_int_dbg);

/*
 * Control isp dump input data,
 */
static ssize_t isp_dump_input_show(struct device *dev,
				   struct device_attribute *attr,  char *buf)
{
	struct camera_group *grp = dev_to_camgrp(dev);
	struct cam_dbg_info *camdbg = camgrp_to_camdbg(grp);

	if (camdbg->isp_dbg.dump_input_on)
		return sprintf(buf, "on\n");
	else
		return sprintf(buf, "off\n");
}

static ssize_t isp_dump_input_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct camera_group *grp = dev_to_camgrp(dev);
	struct cam_dbg_info *camdbg = camgrp_to_camdbg(grp);
	char tune[SYSCMD_INPUT_MAX_CHAR] = "";

	if (count > SYSCMD_INPUT_MAX_CHAR)
		return -EPERM;

	if (!sscanf(buf, "%s", tune))
		return -EINVAL;

	if (!strcmp("on", tune))
		camdbg->isp_dbg.dump_input_on = true;
	else if (!strcmp("off", tune))
		camdbg->isp_dbg.dump_input_on = false;
	else
		return -EINVAL;

	return strnlen(buf, count);
}
static DEVICE_ATTR_RW(isp_dump_input);

/*
 * Control isp hw/page regs dump,
 * The premise of these sub_block's control is that isp_dbg is on,
 */
static ssize_t isp_dump_cfg_show(struct device *dev,
				 struct device_attribute *attr,  char *buf)
{
	struct camera_group *grp = dev_to_camgrp(dev);
	struct cam_dbg_info *camdbg = camgrp_to_camdbg(grp);
	uint32_t i, cnt = 0;

	if (!camdbg->isp_dbg.dbg_on) {
		cnt += sprintf(buf + cnt, "isp_dbg: off\n");
		return cnt;
	}

	cnt += sprintf(buf + cnt,
		       "limit word size to be dumped into kernel log: 0x%x\n",
		       camdbg->isp_dbg.dump2log_max_word_size);

	cnt += sprintf(buf + cnt,
		       "Only support dump %d sections of reg address range!\n",
		       (uint32_t)
		       ARRAY_SIZE(camdbg->isp_dbg.dump_range)/2);

	for (i = 0; i < 6; i += 2) {
		cnt += sprintf(buf + cnt,
			       "[section-%d-]start_addr:0x%x, end_addr:0x%x\n",
			       i/2,
			       camdbg->isp_dbg.dump_range[i],
			       camdbg->isp_dbg.dump_range[i + 1]);
	}

	return cnt;
}

static ssize_t isp_dump_cfg_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int ret = 0;
	uint32_t limit_size = 0;
	uint32_t start_addr_0, end_addr_0;
	uint32_t start_addr_1, end_addr_1;
	uint32_t start_addr_2, end_addr_2;
	struct camera_group *grp = dev_to_camgrp(dev);
	struct cam_dbg_info *camdbg = camgrp_to_camdbg(grp);

	if (!camdbg->isp_dbg.dbg_on)
		return 0;

	ret = sscanf(buf, "%x, %x %x %x %x %x %x",
		     &limit_size,
		     &start_addr_0, &end_addr_0,
		     &start_addr_1, &end_addr_1,
		     &start_addr_2, &end_addr_2);
	if (!ret)
		return -EINVAL;

	if (start_addr_0 > ISP_REG_SIZE || end_addr_0 > ISP_REG_SIZE ||
	    start_addr_1 > ISP_REG_SIZE || end_addr_1 > ISP_REG_SIZE ||
	    start_addr_2 > ISP_REG_SIZE || end_addr_2 > ISP_REG_SIZE ||
	    start_addr_0 > end_addr_0 ||
	    start_addr_1 > end_addr_1 ||
	    start_addr_2 > end_addr_2) {
		pr_info("dump ranges seems not valid, using default value\n");
		return -EINVAL;
	}

	if (limit_size == 0 || limit_size > ISP_REG_SIZE)
		limit_size = MAX_WORD_DUMP_INTO_LOG;
	camdbg->isp_dbg.dump2log_max_word_size = limit_size;
	camdbg->isp_dbg.dump_range[0] = start_addr_0;
	camdbg->isp_dbg.dump_range[1] = end_addr_0;
	camdbg->isp_dbg.dump_range[2] = start_addr_1;
	camdbg->isp_dbg.dump_range[3] = end_addr_1;
	camdbg->isp_dbg.dump_range[4] = start_addr_2;
	camdbg->isp_dbg.dump_range[5] = end_addr_2;

	return strnlen(buf, count);
}
static DEVICE_ATTR_RW(isp_dump_cfg);

/*
 * Control dcam debug
 */
static ssize_t dcam_dbg_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct camera_group *grp = dev_to_camgrp(dev);
	struct cam_dbg_info *camdbg = camgrp_to_camdbg(grp);

	if (camdbg->dcam_dbg.dbg_on)
		return sprintf(buf, "on\n");
	else
		return sprintf(buf, "off\n");

}

static ssize_t dcam_dbg_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	char tune[SYSCMD_INPUT_MAX_CHAR] = "";
	struct camera_group *grp = dev_to_camgrp(dev);
	struct cam_dbg_info *camdbg = camgrp_to_camdbg(grp);

	if (count > SYSCMD_INPUT_MAX_CHAR)
		return -EPERM;

	if(!sscanf(buf, "%s", tune))
		return -EINVAL;

	if (!strcmp("on", tune))
		camdbg->dcam_dbg.dbg_on = true;
	else if (!strcmp("off", tune))
		camdbg->dcam_dbg.dbg_on = false;
	else
		return -EINVAL;

	return strnlen(buf, count);
}
static DEVICE_ATTR_RW(dcam_dbg);

static struct attribute *img_attrs[] = {
	&dev_attr_isp_dbg.attr,
	&dev_attr_isp_fmcu_dbg.attr,
	&dev_attr_isp_int_dbg.attr,
	&dev_attr_isp_dump_input.attr,
	&dev_attr_isp_dump_cfg.attr,
	&dev_attr_dcam_dbg.attr,
	NULL,
};

struct attribute_group isp_dbg_img_attrs_group = {
	.attrs = img_attrs,
};

static ssize_t isp_sblk_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	uint32_t bypass_flag = SBLK_WORK;
	struct camera_group *grp = dev_to_camgrp(dev);
	struct cam_dbg_info *camdbg = camgrp_to_camdbg(grp);
	struct isp_dbg_info *ispdbg = &camdbg->isp_dbg;
	const char *name = NULL;
	int n; /* number of bit in map */

	pr_debug("+\n");

	if (!ispdbg->dbg_on)
		return sprintf(buf, "isp_dbg: off\n");

	name = attr->attr.name;
	for (n = 0; n <= ISP_SBLK_CNT; n++) {
		if (!strcmp(name, isp_sblk_base[n].name)) {
			/* not show any thing for "all" node in isp_sblk dir */
			if (n == ISP_SBLK_CNT)
				return 0;
			isp_get_sblk_byp_flag(ispdbg, n, bypass_flag);
			break;
		}
	}

	pr_debug("-\n");
	return sprintf(buf, "%s\n", bypass_flag ? "off" : "on");
}

static ssize_t isp_sblk_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	uint32_t bypass_flag = SBLK_WORK;
	struct camera_group *grp = dev_to_camgrp(dev);
	struct cam_dbg_info *camdbg = camgrp_to_camdbg(grp);
	struct isp_dbg_info *ispdbg = &camdbg->isp_dbg;
	char tune[SYSCMD_INPUT_MAX_CHAR] = "";
	const char *name = NULL;
	int i, nu;

	pr_debug("+\n");

	if (count > SYSCMD_INPUT_MAX_CHAR)
		return -EPERM;

	if (!ispdbg->dbg_on)
		return 0;

	if (!sscanf(buf, "%s", tune))
		return -EINVAL;

	if (!strcmp("off", tune))
		bypass_flag = SBLK_BYPASS;
	else if (!strcmp("on", tune))
		bypass_flag = SBLK_WORK;
	else
		return -EINVAL;

	name = attr->attr.name;
	for (nu = 0; nu <= ISP_SBLK_CNT; nu++) {
		/* hit the specified sblk or all sblks */
		if (!strcmp(name, isp_sblk_base[nu].name)) {
			if (nu < ISP_SBLK_CNT) /* sub_blocks */
				isp_set_sblk_byp_flag(ispdbg, nu, bypass_flag);
			else /* the end of table, all sblks */
				isp_set_all_sblk_byp_flag(ispdbg, bypass_flag);

			break;
		}
	}

	for (i = 0; i < ISP_SBLK_MAP_CNT; i++)
		pr_debug("isp_sblk map[%d]: 0x%x\n", i, ispdbg->sblk_maps[i]);

	pr_debug("-\n");
	return strnlen(buf, count);
}

static struct device_attribute isp_sblk_dev_attrs[] __aligned(8) = {
	[0 ... ISP_SBLK_CNT] = __ATTR(xy, S_IWUSR | S_IRUGO,
				      isp_sblk_show,
				      isp_sblk_store),
};

/* valid sblk_cnt, "all", "null" -> total: ISP_SBLK_CNT + 1 */
static struct attribute *isp_sblk_attrs[] __aligned(8) = {
	[0 ... ISP_SBLK_CNT + 1] = &isp_sblk_dev_attrs[0].attr,
};

struct attribute_group isp_dbg_sblk_attrs_group = {
	.name = "isp_sblk",
	.attrs = isp_sblk_attrs,
};

static int isp_sblk_dev_attrs_init(struct camera_group *p_grp)
{
	struct device_attribute *dev_attr = NULL;
	struct isp_dbg_info *ispdbg = NULL;
	struct cam_dbg_info *camdbg = NULL;
	int i;

	if (p_grp && p_grp->p_dbg_info) {
		camdbg = camgrp_to_camdbg(p_grp);
		ispdbg = &camdbg->isp_dbg;
	} else
		return -EPERM;

	for (i = 0; i <= ISP_SBLK_CNT; i++) {
		dev_attr = &isp_sblk_dev_attrs[i];
		dev_attr->attr.name = isp_sblk_base[i].name;
		isp_sblk_attrs[i] = &dev_attr->attr;
	}

	isp_sblk_attrs[i] = NULL;
	ispdbg->sblk_cnt = ISP_SBLK_CNT;
	ispdbg->sblk_base = isp_sblk_base;

	return 0;
}

static bool do_check_isp_dbg_switch(struct isp_dbg_info *dbg, enum dbg_sw_id did)
{
	bool sw = false;

	if (!dbg || !dbg->dbg_on)
		return false;

	switch (did) {
	case ISP_DBG_SW:
		sw = true;
		break;
	case ISP_FMCU_DBG_SW:
		if (dbg->fmcu_dbg_on)
			sw = true;
		break;
	case ISP_INT_DBG_SW:
		if (dbg->int_dbg_on)
			sw = true;
		break;
	case ISP_DUMP_INPUT:
		if (dbg->dump_input_on)
			sw = true;
		break;
	default:
		break;
	}

	pr_debug("dbg_sw_id:%d, switch:%d\n", did, sw);
	return sw;
}

bool isp_dbg_check_switch_on(void *isp_dev, enum dbg_sw_id did)
{
	struct isp_pipe_dev *dev = NULL;
	struct cam_dbg_info *camdbg = NULL;
	struct isp_dbg_info *ispdbg = NULL;

	dev = (struct isp_pipe_dev *)isp_dev;
	if (likely(dev && dev->cam_grp)) {
		camdbg = camgrp_to_camdbg(dev->cam_grp);
		if (unlikely(!camdbg))
			return false;
		ispdbg = &camdbg->isp_dbg;
		return do_check_isp_dbg_switch(ispdbg, did);
	} else
		return false;
}

int cam_dbg_init(void *miscdev)
{
	struct miscdevice *miscd = NULL;
	struct camera_group *grp = NULL;
	void *dbg = NULL;

	pr_info("start\n");
	miscd = (struct miscdevice *)miscdev;
	if (miscd && miscd->this_device)
		grp = dev_to_camgrp(miscd->this_device);
	else
		return -ENODEV;

	if (grp) {
		dbg = kzalloc(sizeof(struct cam_dbg_info), GFP_KERNEL);
		if (IS_ERR_OR_NULL(dbg))
			return -ENOMEM;

		grp->p_dbg_info = dbg;
		if (isp_sblk_dev_attrs_init(grp)) {
			kfree(dbg);
			grp->p_dbg_info = NULL;
			return -EFAULT;
		}
	} else
		return -EPERM;

	pr_info("done\n");
	return 0;
}

int cam_dbg_deinit(void *miscdev)
{
	struct miscdevice *miscd = NULL;
	struct camera_group *grp = NULL;

	pr_info("start\n");
	miscd = (struct miscdevice *)miscdev;
	if (miscd && miscd->this_device)
		grp = dev_to_camgrp(miscd->this_device);
	else
		return -ENODEV;

	if (grp && grp->p_dbg_info) {
		kfree(grp->p_dbg_info);
		grp->p_dbg_info = NULL;
	}

	pr_info("done\n");
	return 0;
}

