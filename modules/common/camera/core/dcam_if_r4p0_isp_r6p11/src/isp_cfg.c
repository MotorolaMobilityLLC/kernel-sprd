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

#include <linux/printk.h>
#include <asm/cacheflush.h>
#include "isp_cfg.h"
#include "isp_buf.h"

#define SIMPLY_MEMCPY

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_CFG: %d: %d %s:" \
	fmt, current->pid, __LINE__, __func__

unsigned long isp_cctx_cmdbuf_addr[ISP_ID_MAX][ISP_SCENE_NUM] = { { 0 } };

/* using work_mode, i.e. GET_MODE(idx), as index */
unsigned long *isp_addr_poll[ISP_WM_MAX][CFG_CONTEXT_NUM] = {
	{
		&isp_cctx_cmdbuf_addr[0][0], /* iid 0, sid 0, p0 */
		&isp_cctx_cmdbuf_addr[0][1], /* iid 0, sid 1, c0 */
		&isp_cctx_cmdbuf_addr[1][0], /* iid 1, sid 0, p1 */
		&isp_cctx_cmdbuf_addr[1][1]  /* iid 1, sid 1, c1 */
	},
	{
		&s_isp_regbase[0],
		&s_isp_regbase[0],
		&s_isp_regbase[1],
		&s_isp_regbase[1]
	}
};

static uint32_t ISP_CFG_MAP[] __aligned(8) = {
	0x00041010, /*0x1010  - 0x1010 , 1   , PGN*/
	0x000C1110, /*0x1110  - 0x1118 , 3   , BLC*/
	0x000C1310, /*0x1310  - 0x1318 , 3   , Post_BLC*/
	0x00281210, /*0x1210  - 0x1234 , 10  , RGBG*/
	0x00A01410, /*0x1410  - 0x14AC , 40  , NLC*/
	0x003C1510, /*0x1510  - 0x1548 , 15  , LENS*/
	0x00401F10, /*0x1F10  - 0x1F4C , 16  , 1D_LNC*/
	0x00141610, /*0x1610  - 0x1620 , 5   , BIN*/
	0x00181810, /*0x1810  - 0x1824 , 6   , AWBC*/
	0x00201910, /*0x1910  - 0x192C , 8   , AEM*/
	0x00A01A10, /*0x1A10  - 0x1AAC , 40  , BPC*/
	0x00381B10, /*0x1B10  - 0x1B44 , 14  , GC*/
	0x00041C10, /*0x1C10  - 0x1C10 , 1   , VST*/
	0x00DC1D10, /*0x1D10  - 0x1DE8 , 55  , NLM*/
	0x00041E10, /*0x1E10  - 0x1E10 , 1   , IVST*/
	0x00503010, /*0x3010  - 0x305C , 20  , CFA_NEW*/
	0x00183110, /*0x3110  - 0x3124 , 6   , CMC10*/
	0x00043210, /*0x3210  - 0x3210 , 1   , GAMC_NEW*/
	0x00403310, /*0x3310  - 0x334C , 16  , HSV*/
	0x00243410, /*0x3410  - 0x3430 , 9   , PSTRZ*/
	0x001C3510, /*0x3510  - 0x3528 , 7   , CCE*/
	0x001C3610, /*0x3610  - 0x3628 , 7   , UVD*/
	0x01943810, /*0x3810  - 0x39A0 , 101 , AFMrgb*/
	0x00344610, /*0x4610  - 0x4640 , 13  , afl_new*/
	0x004C5010, /*0x5010  - 0x5058 , 19  , PRECDN*/
	0x00845110, /*0x5110  - 0x5190 , 33  , YNR*/
	0x00045210, /*0x5210  - 0x5210 , 1   , BRTA*/
	0x00045310, /*0x5310  - 0x5310 , 1   , CNTA*/
	0x000C5410, /*0x5410  - 0x5418 , 3   , HISTS*/
	0x00145510, /*0x5510  - 0x5520 , 5   , HISTS2*/
	0x00485610, /*0x5610  - 0x5654 , 18  , CDN*/
	0x00745710, /*0x5710  - 0x5780 , 29  , NEW_EE*/
	0x00045810, /*0x5810  - 0x5810 , 1   , CSA*/
	0x00045910, /*0x5910  - 0x5910 , 1   , HUA*/
	0x00745A10, /*0x5A10  - 0x5A80 , 29  , POST_CDN*/
	0x00045B10, /*0x5B10  - 0x5B10 , 1   , YGAMMA*/
	0x00085C10, /*0x5C10  - 0x5C14 , 2   , YUVDELAY*/
	0x00C85D10, /*0x5D10  - 0x5DD4 , 50  , IIRCNR*/
	0x00185E10, /*0x5E10  - 0x5E24 , 6   , YRANDOM*/
	0x0054D010, /*0xD010  - 0xD060 , 21  , SCL_VID*/
	0x0034D110, /*0xD110  - 0xD140 , 13  , SCL_VID_store*/
	0x007CD400, /*0xD400  - 0xD478 , 31  , LUMA_HCOEF*/
	0x003CD480, /*0xD480  - 0xD4B8 , 15  , CHROMA_HCOEF*/
	0x020CD4F0, /*0xD4F0  - 0xD6F8 , 131 , LUMA_VCOEF*/
	0x020CD8F0, /*0xD8F0  - 0xDAF8 , 131 , CHROMA_VCOEF*/
	0x0054C010, /*0xC010  - 0xC060 , 21  , SCL_CAP*/
	0x0034C110, /*0xC110  - 0xC140 , 13  , SCL_CAP_store*/
	0x007CC400, /*0xC400  - 0xC478 , 31  , LUMA_HCOEF*/
	0x003CC480, /*0xC480  - 0xC4B8 , 15  , CHROMA_HCOEF*/
	0x020CC4F0, /*0xC4F0  - 0xC6F8 , 131 , LUMA_VCOEF*/
	0x020CC8F0, /*0xC8F0  - 0xCAF8 , 131 , CHROMA_VCOEF*/
	0x0044C210, /*0xC210  - 0xC250 , 17  , SCL_CAP_noisefilter_add_rdm*/
	0x00640110, /*0x110   - 0x170  , 25  , FETCH*/
	0x00300210, /*0x210   - 0x23C  , 12  , STORE*/
	0x00180310, /*0x310   - 0x324  , 6   , DISPATCH*/
	0x05A18000, /*0x18000 - 0x1859C, 360 , ISP_HSV_BUF0_CH0*/
	0x10019000, /*0x19000 - 0x19FFC, 1024, ISP_VST_BUF0_CH0*/
	0x1001A000, /*0x1A000 - 0x1AFFC, 1024, ISP_IVST_BUF0_CH0*/
	0x0401B000, /*0x1B000 - 0x1B3FC, 256 , ISP_FGAMMA_R_BUF0_CH0*/
	0x0401C000, /*0x1C000 - 0x1C3FC, 256 , ISP_FGAMMA_G_BUF0_CH0*/
	0x0401D000, /*0x1D000 - 0x1D3FC, 256 , ISP_FGAMMA_B_BUF0_CH0*/
	0x0205E000, /*0x1E000 - 0x1E200, 129 , ISP_YGAMMA_BUF0_CH0*/
	0x23029000, /*0x29000 - 0x2B2FC, 2240, ISP_LENS_BUF0_CH0*/
	0x04030000, /*0x30000 - 0x303FC, 256 , ISP_1D_LENS_GR_BUF0_CH0*/
	0x04030400, /*0x30400 - 0x307FC, 256 , ISP_1D_LENS_R_BUF0_CH0*/
	0x04030800, /*0x30800 - 0x30BFC, 256 , ISP_1D_LENS_B_BUF0_CH0*/
	0x04030C00, /*0x30C00 - 0x30FFC, 256 , ISP_1D_LENS_GB_BUF0_CH0*/
	0x04136400, /*0x36400 - 0x3680C, 260 , ISP_LENS_WEIGHT_BUF0*/
};

static uint32_t cfg_cmd_addr[ISP_ID_MAX][ISP_SCENE_NUM] = {
	{
		ISP_CFG_PRE0_CMD_ADDR,
		ISP_CFG_CAP0_CMD_ADDR
	},

	{
		ISP_CFG_PRE1_CMD_ADDR,
		ISP_CFG_CAP1_CMD_ADDR
	}
};

struct isp_dev_cfg_info {
	uint32_t bypass;
	uint32_t tm_bypass;
	uint32_t sdw_mode;
	uint32_t num_of_mod;
	uint32_t *isp_cfg_map;
	uint32_t cfg_main_sel;
	uint32_t bp_pre0_pixel_rdy;
	uint32_t bp_pre1_pixel_rdy;
	uint32_t bp_cap0_pixel_rdy;
	uint32_t bp_cap1_pixel_rdy;
	uint32_t cap0_cmd_ready_mode;
	uint32_t cap1_cmd_ready_mode;
	uint32_t tm_set_number;
	uint32_t cap0_th;
	uint32_t cap1_th;
} s_cfg_settings = {
	/*cowork with dcam with sof_in*/
	0, 1, 1, ARRAY_SIZE(ISP_CFG_MAP), ISP_CFG_MAP,
	0, 1, 1, 1, 1,
	0, 0, 0, 0, 0,
};

/*
 * Definitions of internal functions
 */

static void cctx_init_page_buf_addr(struct isp_cctx_desc *cctx_desc,
				   void *sw_addr, void *hw_addr)
{
	unsigned long base, ofst;
	struct isp_cctx_ion_buf *ion_buf;
	int _sid, bid;

	for (_sid = 0; _sid < ISP_SCENE_NUM; _sid++) {
		base = _sid * ISP_CCTX_CMD_BUF_SIZE;
		ion_buf = &cctx_desc->ion_buf[_sid];
		for (bid = 0; bid < ISP_CCTX_CMD_BUF_NUM; bid++) {
			ofst = bid * ISP_REG_SIZE;
			if (sw_addr) {
				ion_buf->cmd_buf[bid].sw_addr =
					sw_addr + base + ofst;
				pr_info("cfg_cmd_buf sid%d_bid%d:sw=0x%lx\n",
					_sid, bid,
					(unsigned long)
					ion_buf->cmd_buf[bid].sw_addr);
			}

			if (hw_addr) {
				ion_buf->cmd_buf[bid].hw_addr =
					hw_addr + base + ofst;
				pr_info("cfg_cmd_buf sid%d_bid%d:hw=0x%lx\n",
					_sid, bid,
					(unsigned long)
					ion_buf->cmd_buf[bid].hw_addr);
			}
		}
	}
}

static void cctx_deinit_page_buf_addr(struct isp_cctx_desc *cctx_desc,
				      bool set_sw_addr, bool set_hw_addr)
{
	struct isp_cctx_ion_buf *ion_buf;
	int _sid, bid;

	for (_sid = 0; _sid < ISP_SCENE_NUM; _sid++) {
		ion_buf = &cctx_desc->ion_buf[_sid];
		for (bid = 0; bid < ISP_CCTX_CMD_BUF_NUM; bid++) {
			if (set_sw_addr) {
				ion_buf->cmd_buf[bid].sw_addr = NULL;
				pr_info("clear cfg_cmd_buf sw addr%d_%d\n",
					_sid, bid);
			}

			if (set_hw_addr) {
				ion_buf->cmd_buf[bid].hw_addr = NULL;
				pr_info("clea cfg_cmd_buf hw addr%d_%d\n",
					_sid, bid);
			}
		}
	}
}

static void cctx_init_cmdbuf_addr(struct isp_cctx_desc *cctx_desc,
				  enum isp_id iid)
{
	unsigned long flag;
	struct isp_cctx_ion_buf *ion_buf_p;
	struct cmd_buf_info *cur_cmdbuf_p;
	enum cmdbuf_id cur_cmdbuf_id;
	int _sid;

	pr_debug(" +\n");

	spin_lock_irqsave(&cctx_desc->lock, flag);

	/*
	 * Init context reg buf for each isp instance.
	 * Each instance has two contexts, pre and cap.
	 * Each context has two buf, shadow and work.
	 * Using shadow buf as initial buf.
	 * @isp_cctx_cmdbuf_addr will be used when using ISP_BASE_ADDR.
	 */
	for (_sid = ISP_SCENE_PRE; _sid < ISP_SCENE_NUM; _sid++) {
		ion_buf_p = &cctx_desc->ion_buf[_sid];
		ion_buf_p->cur_buf_id = cur_cmdbuf_id = CMD_BUF_SHADOW;
		cur_cmdbuf_p = &ion_buf_p->cmd_buf[cur_cmdbuf_id];
		isp_cctx_cmdbuf_addr[iid][_sid] =
			(unsigned long)cur_cmdbuf_p->sw_addr;
		pr_info("init cctx_buf[iid%d][sid%d] sw=0x%p, hw=0x%p\n",
			iid, _sid, cur_cmdbuf_p->sw_addr,
			cur_cmdbuf_p->hw_addr);
	}

	spin_unlock_irqrestore(&cctx_desc->lock, flag);

	pr_debug(" -\n");
}

static void cctx_deinit_cmdbuf_addr(struct isp_cctx_desc *cctx_desc,
				    enum isp_id iid)
{
	unsigned long flag;
	int _sid;

	pr_debug(" +\n");

	spin_lock_irqsave(&cctx_desc->lock, flag);

	for (_sid = ISP_SCENE_PRE; _sid < ISP_SCENE_NUM; _sid++)
		isp_cctx_cmdbuf_addr[iid][_sid] = 0UL;

	spin_unlock_irqrestore(&cctx_desc->lock, flag);

	pr_debug(" -\n");
}

static void cctx_buf_align_256kb(struct isp_cctx_desc *cctx_desc,
				  void **sw_addr, void **hw_addr)
{
	void *tmp_addr = NULL;
	int ofst_align = 0;

	struct isp_buf_info *ion_pool_p = &cctx_desc->ion_pool;

	if (ion_pool_p->sw_addr && ion_pool_p->hw_addr) {
		if (IS_ALIGNED((unsigned long)ion_pool_p->hw_addr,
			       ISP_REG_SIZE)) {
			*hw_addr = ion_pool_p->hw_addr;
			*sw_addr = ion_pool_p->sw_addr;
		} else {
			tmp_addr = (void *)ALIGN((unsigned long)
						 ion_pool_p->hw_addr,
						 ISP_REG_SIZE);
			ofst_align = tmp_addr - ion_pool_p->hw_addr;
			*sw_addr = ion_pool_p->sw_addr + ofst_align;
			*hw_addr = tmp_addr;
			cctx_desc->ofst_align = ofst_align;
		}
	}
}

/*
 * Definitions of external functions
 */

static int isp_cfg_map_init(struct isp_pipe_dev *dev)
{
	unsigned int i = 0;
	uint32_t cfg_map_size = 0;
	uint32_t *cfg_map = NULL;

	if (!dev) {
		pr_err("fail to get isp dev\n");
		return -EINVAL;
	}

	if (atomic_inc_return(&dev->cfg_map_lock) == 1) {
		cfg_map_size = s_cfg_settings.num_of_mod;
		cfg_map = s_cfg_settings.isp_cfg_map;
		for (i = 0; i < cfg_map_size; i++) {
			ISP_HREG_WR(dev->com_idx, ISP_CFG0_BUF + i * 4,
				cfg_map[i]);
			ISP_HREG_WR(dev->com_idx, ISP_CFG1_BUF + i * 4,
				cfg_map[i]);
		}
		pr_info("CFG map init successfully!\n");
	} else
		pr_info("CFG map already been inited!\n");

	return 0;
}

/*
 * Mapping table
 * ------------------------------------------------------
 * iid sid  iid|sid<<1	-> cfg__id	cfg_id alias
 * ------------------------------------------------------
 * 0	0   0|0<<1	-> 00		P0
 * 0	1   0|1<<1	-> 10		C0
 * 1	0   1|0<<1	-> 01		P1
 * 1	1   1|1<<1	-> 11		C1
 * ------------------------------------------------------
 *
 * Although ISP_HREG_WR needs com_idx as parameter, we use iid as
 * parameter in this file, the same effect as com_idx for macro
 * ISP_HREG_WR, because the iid sites on the lowest bits of com_idx.
 *
 */
static void isp_cctx_buf_ready(struct isp_cctx_desc *cctx_desc,
			       enum isp_id iid,
			       enum isp_scene_id sid)
{
	enum cfg_context_id cfg_id = ISP_GET_CFG_ID(sid, iid);

	switch (cfg_id) {
	case CFG_CONTEXT_P0:
		ISP_HREG_WR(iid, ISP_CFG_PRE0_START, BIT_0);
		break;
	case CFG_CONTEXT_P1:
		ISP_HREG_WR(iid, ISP_CFG_PRE1_START, BIT_0);
		break;
	case CFG_CONTEXT_C0:
		ISP_HREG_WR(iid, ISP_CFG_CAP0_START, BIT_0);
		break;
	case CFG_CONTEXT_C1:
		ISP_HREG_WR(iid, ISP_CFG_CAP1_START, BIT_0);
		break;
	default:
		break;
	}

	pr_debug("isp cfg start -- context %d\n", cfg_id);
}

static int isp_cctx_init_hw(struct isp_cctx_desc *cctx_desc,
			    enum isp_id iid,
			    enum isp_scene_id sid)
{
	int ret = 0;
	uint32_t val = 0;
	void *work_buf_va = NULL;
	void *shadow_buf_va = NULL;
	struct cmd_buf_info *cmdbuf_p = NULL;
	struct isp_cctx_ion_buf *ion_buf_p = NULL;
	unsigned long work_buf_pa = 0;
	enum cmdbuf_id work_buf_id;

	pr_debug("+\n");
	if (!cctx_desc) {
		pr_err("fail to init cctx_desc is NULL\n");
		return -EFAULT;
	}

	shadow_buf_va = (void *)isp_cctx_cmdbuf_addr[iid][sid];

	/*
	 * For capture scenario, using cctx pong buf as "work
	 * buf" temporarily, and ping buf as "shadow buf", and
	 * copy cap-frame registers values from shadow to work buf.
	 * Because slice capture may be delayed by new coming
	 * preview frame, so here we use work buf to keep the registers
	 * values of capture frame not being overwirtten by new coming
	 * cap-frame registers values in zsl scenario. Thus it can make
	 * sure the cap-frame's registers value will not be changed until
	 * next capture starting, even if the slice cap being delayed
	 * serval frames by preview.
	 * TODO: need to check if ping-pong buf necessary,
	 * if not, it's better to change "ping" as "shadow",
	 * and pong as "work", beware of misunderstanding.
	 */
	if (sid == ISP_SCENE_PRE || sid == ISP_SCENE_CAP) {
		ion_buf_p = &cctx_desc->ion_buf[sid];
		work_buf_id = ion_buf_p->cur_buf_id ^ 1;
		cmdbuf_p = &ion_buf_p->cmd_buf[work_buf_id];
		work_buf_pa = (unsigned long)cmdbuf_p->hw_addr;
		if (unlikely(!IS_ALIGNED(work_buf_pa, ISP_REG_SIZE))) {
			pr_err("fail to get cmd addr,should be aligned with 256KB!\n");
			return -EINVAL;
		}
		work_buf_va = cmdbuf_p->sw_addr;

		memcpy(work_buf_va, shadow_buf_va, ISP_REG_SIZE);

		pr_debug("shadow cfg cmd buf, shadow_va:0x%p, work_va:0x%p, work_pa:0x%lx\n",
			 shadow_buf_va, work_buf_va, work_buf_pa);
	} else {
		pr_err("fail to get valid sid\n");
		return -EINVAL;
	}

	/*
	 * SharkLe has only one fmcu, so not necessary to set
	 * this cap_cmd_ready_mode bit separately. Here, we set
	 * the two bits in CFG_PARAM together.
	 * After setting this cap0_cmd_ready_mode, fmcu can use
	 * fmcu_write_reg_ready after configing slice info done.
	 */
	if (sid == ISP_SCENE_CAP) {
		s_cfg_settings.cap0_cmd_ready_mode = 1;
		s_cfg_settings.cap1_cmd_ready_mode = 1;
		pr_debug("set cap%d_cmd_ready_mode for capture.\n", iid);
	}

	val = (s_cfg_settings.cap1_cmd_ready_mode << 25)|
		(s_cfg_settings.cap0_cmd_ready_mode << 24)|
		(s_cfg_settings.bp_cap1_pixel_rdy << 23) |
		(s_cfg_settings.bp_cap0_pixel_rdy << 22) |
		(s_cfg_settings.bp_pre1_pixel_rdy << 21) |
		(s_cfg_settings.bp_pre0_pixel_rdy << 20) |
		(s_cfg_settings.cfg_main_sel << 16) |
		(s_cfg_settings.num_of_mod << 8) |
		(s_cfg_settings.sdw_mode << 5) |
		(s_cfg_settings.tm_bypass << 4) |
		(s_cfg_settings.bypass);

	ISP_HREG_WR(iid, ISP_CFG_PAMATER, val);

	if (!s_cfg_settings.tm_bypass) {
		ISP_HREG_WR(iid, ISP_CFG_TM_NUM,
			    s_cfg_settings.tm_set_number);
		ISP_HREG_WR(iid, ISP_CFG_CAP0_TH,
			    s_cfg_settings.cap0_th);
		ISP_HREG_WR(iid, ISP_CFG_CAP1_TH,
			    s_cfg_settings.cap1_th);
	}

#if 0
#ifdef CONFIG_64BIT
	__flush_dcache_area(work_buf_va, ISP_REG_SIZE);
#else
	flush_kernel_vmap_range(work_buf_va, ISP_REG_SIZE);
#endif
#endif

	ISP_HREG_WR(iid, cfg_cmd_addr[iid][sid], work_buf_pa);
	ISP_HREG_MWR(iid, ISP_ARBITER_ENDIAN_COMM, 0x1, 0x1);

	pr_debug("-\n");
	return ret;
}

static void isp_cctx_reset_page_buf(struct isp_cctx_desc *cctx_desc,
				    enum isp_id iid)
{
	struct cmd_buf_info *cmdbuf_p = NULL;
	struct isp_cctx_ion_buf *ion_buf_p = NULL;
	enum cmdbuf_id bid;
	void *sw_addr;
	int _sid;

	for (_sid = ISP_SCENE_PRE; _sid < ISP_SCENE_NUM; _sid++) {
		ion_buf_p = &cctx_desc->ion_buf[_sid];
		bid = ion_buf_p->cur_buf_id;
		cmdbuf_p = &ion_buf_p->cmd_buf[bid];
		sw_addr = cmdbuf_p->sw_addr;
		if (sw_addr) {
			memcpy(sw_addr, ISP_REG_DEFAULT,
					sizeof(ISP_REG_DEFAULT));
			memset(sw_addr + sizeof(ISP_REG_DEFAULT),
					0x0,
					ISP_REG_SIZE - sizeof(ISP_REG_DEFAULT));
			pr_info("iid%d reset isp cfg page buf%d for sid%d\n",
				iid, bid, _sid);
		}
	}
}

static int isp_cctx_init(struct isp_cctx_desc *cctx_desc, enum isp_id iid)
{
	int ret = 0;
	struct isp_buf_info *ion_pool_p = NULL;
	void *sw_addr = NULL;
	void *hw_addr = NULL;

	pr_debug("+\n");
	if (!cctx_desc) {
		pr_err("fail to init empty cctx_desc\n");
		return -EFAULT;
	}

	ion_pool_p = &cctx_desc->ion_pool;
	cctx_desc->client = NULL;
	cctx_desc->handle = NULL;
	cctx_desc->lock = __SPIN_LOCK_UNLOCKED(cctx_desc_spinlock);
	cctx_desc->ofst_align = 0;

	sprintf(ion_pool_p->name, "iid%d_cfg", iid);
	ion_pool_p->size = ISP_CCTX_ALL_CMD_BUF_ALIGN_SIZE;
	ret = isp_gen_buf_alloc(ion_pool_p);
	if (ret != 0) {
		pr_err("iid%d fail to alloc buf for cctx_buf, ret %d\n",
		       iid, ret);
		return -ENOMEM;
	}

	ret = isp_gen_buf_hw_map(ion_pool_p);
	if (ret) {
		pr_err("fail to  map buf for cctx_buf, iid%d ret %d\n",
		       iid, ret);
		goto _err_free;
	}

	pr_info("iid:%d cctx cmd ion buf hw_addr:0x%p, sw_addr:0x%p, len:0x%zx, ofst:%d\n",
		iid, ion_pool_p->hw_addr, ion_pool_p->sw_addr,
		ion_pool_p->size, cctx_desc->ofst_align);

	cctx_buf_align_256kb(cctx_desc, &sw_addr, &hw_addr);
	cctx_init_page_buf_addr(cctx_desc, sw_addr, hw_addr);
	cctx_init_cmdbuf_addr(cctx_desc, iid);
	isp_cctx_reset_page_buf(cctx_desc, iid);

	pr_debug("-\n");
	return ret;

_err_free:
	isp_gen_buf_free(ion_pool_p);

	return ret;
}

static int isp_cctx_deinit(struct isp_cctx_desc *cctx_desc, enum isp_id iid)
{
	int ret = 0;
	struct isp_buf_info *ion_pool_p = NULL;

	pr_info("+\n");
	if (!cctx_desc) {
		pr_err("fail to get cctx_desc is NULL\n");
		return -EFAULT;
	}

	ion_pool_p = &cctx_desc->ion_pool;

	cctx_deinit_cmdbuf_addr(cctx_desc, iid);
	cctx_deinit_page_buf_addr(cctx_desc,
				  ion_pool_p->sw_addr,
				  ion_pool_p->hw_addr);
	isp_gen_buf_hw_unmap(ion_pool_p);
	isp_gen_buf_free(ion_pool_p);

	pr_info("-\n");
	return ret;
}

static struct isp_cctx_intf ctx_intf = {
	.init_cctx = isp_cctx_init,
	.init_hw = isp_cctx_init_hw,
	.init_cfg_map = isp_cfg_map_init,
	.deinit_cctx = isp_cctx_deinit,
	.rst_page_buf = isp_cctx_reset_page_buf,
	.buf_ready = isp_cctx_buf_ready,
};

static struct isp_cctx_desc s_isp_cctx[ISP_ID_MAX] = {
	{
		.owner = "ISP_ID_0",
		.intf = &ctx_intf,
	},

	{
		.owner = "ISP_ID_1",
		.intf = &ctx_intf,
	},
};


/*
 * Get CFG context description for each isp instance.
 * @iid, isp instance index.
 */
struct isp_cctx_desc *isp_cctx_get_desc(enum isp_id iid)
{
	return &s_isp_cctx[iid];
}

