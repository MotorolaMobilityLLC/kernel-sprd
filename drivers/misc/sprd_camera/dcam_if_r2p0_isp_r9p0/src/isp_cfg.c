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

#include <linux/printk.h>
#include <asm/cacheflush.h>
#include <video/sprd_mm.h>

#include "isp_cfg.h"
#include "isp_buf.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_CFG: %d: %d %s:" \
	fmt, current->pid, __LINE__, __func__

static uint32_t ISP_CFG_MAP[] __aligned(8) = {
	0x00041A10,   /*0x1A10  - 0x1A10 , 1 , BWU*/
	0x00381B10,   /*0x1B10  - 0x1B44 , 14  , gc*/
	0x00041C10,   /*0x1C10  - 0x1C10 , 1 , VST*/
	0x01702010,   /*0x2010  - 0x217C , 92  , NLM*/
	0x00041E10,   /*0x1E10  - 0x1E10 , 1 , IVST*/
	0x007C1F10,   /*0x1F10  - 0x1F88 , 31  , GC2*/
	0x001C3010,   /*0x3010  - 0x3028 , 7 , CFA_NEW*/
	0x00203810,   /*0x3810  - 0x382C , 8 , frgb_LTM_hists*/
	0x00183110,   /*0x3110  - 0x3124 , 6 , CMC10*/
	0x00183910,   /*0x3910  - 0x3924 , 6 , frgb_LTM_map*/
	0x00043210,   /*0x3210  - 0x3210 , 1 , GAMC_NEW*/
	0x005C3310,   /*0x3310  - 0x3368 , 23  , HSV*/
	0x00043410,   /*0x3410  - 0x3410 , 1 , PSTRZ*/
	0x001C3510,   /*0x3510  - 0x3528 , 7 , CCE*/
	0x001C3610,   /*0x3610  - 0x3628 , 7 , UVD*/
	0x00143710,   /*0x3710  - 0x3720 , 5 , HISTS*/
	0x00185F10,   /*0x5F10  - 0x5F24 , 6 , LTM_map*/
	0x00205510,   /*0x5510  - 0x552C , 8 , yuv_LTM_hists*/
	0x004C5010,   /*0x5010  - 0x5058 , 19  , PRECDN*/
	0x008C5110,   /*0x5110  - 0x5198 , 35  , YNR*/
	0x00485610,   /*0x5610  - 0x5654 , 18  , CDN*/
	0x00D85710,   /*0x5710  - 0x57E4 , 54  , NEW_EE*/
	0x00745A10,   /*0x5A10  - 0x5A80 , 29  , POST_CDN*/
	0x00045B10,   /*0x5B10  - 0x5B10 , 1 , YGAMMA*/
	0x00085C10,   /*0x5C10  - 0x5C14 , 2 , YUVDELAY*/
	0x00C85D10,   /*0x5D10  - 0x5DD4 , 50  , IIRCNR*/
	0x00185E10,   /*0x5E10  - 0x5E24 , 6 , YRANDOM*/
	0x00185910,   /*0x5910  - 0x5924 , 6 , BCHS*/
	0x00449010,   /*0x9010  - 0x9050 , 17  , mem_ctrl*/
	0x00649110,   /*0x9110  - 0x9170 , 25  , nr3_blend*/
	0x00189210,   /*0x9210  - 0x9224 , 6 , 3dnr_store*/
	0x00109310,   /*0x9310  - 0x931C , 4 , crop*/
	0x002C9410,   /*0x9410  - 0x9438 , 11  , fbc_3dnr_store*/
	0x003C9510,   /*0x9510  - 0x9548 , 15  , fbd_nr3_fetch*/
	0x0050D010,   /*0xD010  - 0xD05C , 20  , SCL_VID*/
	0x0030D110,   /*0xD110  - 0xD13C , 12  , SCL_VID_store*/
	0x0030D310,   /*0xD310  - 0xD33C , 12  , scl_vid_fbc_store*/
	0x0044C010,   /*0xC010  - 0xC050 , 17  , SCL_CAP*/
	0x0030C110,   /*0xC110  - 0xC13C , 12  , SCL_CAP_store*/
	0x0044C210,   /*0xC210  - 0xC250 , 17  , SCL_CAP_noisefilter*/
	0x0030C310,   /*0xC310  - 0xC33C , 12  , SCL_CAP_fbc_store*/
	0x0054E010,   /*0xE010  - 0xE060 , 21  , SCL_THUMBNAIL*/
	0x0030E110,   /*0xE110  - 0xE13C , 12  , SCL_THUMBNAIL_store*/
	0x00300110,   /*0x110   - 0x13C  , 12  , FETCH*/
	0x00300210,   /*0x210   - 0x23C  , 12  , STORE*/
	0x001C0310,   /*0x310   - 0x328  , 7 , DISPATCH*/
	0x003C0C10,   /*0xC10   - 0xC48  , 15  , Fetch_FBD*/
	0x05A18000,   /*0x18000 - 0x1859C, 360 , ISP_HSV_BUF0*/
	0x20019000,   /*0x19000 - 0x1AFFC, 2048, ISP_VST_BUF0*/
	0x20029000,   /*0x29000 - 0x2AFFC, 2048, ISP_IVST_BUF0*/
	0x0201F000,   /*0x1F000 - 0x1F1FC, 128 , ISP_RGB_LTM_BUF0*/
	0x0201F400,   /*0x1F400 - 0x1F5FC, 128 , ISP_YUV_LTM_BUF0*/
	0x0401B000,   /*0x1B000 - 0x1B3FC, 256 , ISP_FGAMMA_R_BUF0*/
	0x0401C000,   /*0x1C000 - 0x1C3FC, 256 , ISP_FGAMMA_G_BUF0*/
	0x0401D000,   /*0x1D000 - 0x1D3FC, 256 , ISP_FGAMMA_B_BUF0*/
	0x0201B400,   /*0x1B400 - 0x1B5FC, 128 , ISP_PSTRZ_R_BUF0*/
	0x0201C400,   /*0x1C400 - 0x1C5FC, 128 , ISP_PSTRZ_G_BUF0*/
	0x0201D400,   /*0x1D400 - 0x1D5FC, 128 , ISP_PSTRZ_B_BUF0*/
	0x0205E000,   /*0x1E000 - 0x1E200, 129 , ISP_YGAMMA_BUF0*/
	0x007F9100,   /*0x39100 - 0x39178, 31 , CAP_PATH_HOR_CORF_Y_BUF0*/
	0x003F9300,   /*0x39300 - 0x39338, 15 , CAP_PATH_HOR_CORF_UV_BUF0*/
	0x020F94F0,   /*0x394F0 - 0x396F8, 131 , CAP_PATH_VER_CORF_Y_BUF0*/
	0x020F9AF0,   /*0x39AF0 - 0x39CF8, 131 , CAP_PATH_VER_CORF_UV_BUF0*/
	0x007F8100,   /*0x38100 - 0x38178, 31 , VID_PATH_HOR_CORF_Y_BUF0*/
	0x003F8300,   /*0x38300 - 0x38338, 15 , VID_PATH_HOR_CORF_UV_BUF0*/
	0x020F84F0,   /*0x384F0 - 0x386F8, 131 , VID_PATH_VER_CORF_Y_BUF0*/
	0x020F8AF0,   /*0x38AF0 - 0x38CF8, 131 , VID_PATH_VER_CORF_UV_BUF0*/
};

static unsigned long cfg_cmd_addr[ISP_ID_MAX][ISP_SCENE_MAX] = {
	{
		ISP_CFG_PRE0_CMD_ADDR,
		ISP_CFG_CAP0_CMD_ADDR
	},
	{
		ISP_CFG_PRE1_CMD_ADDR,
		ISP_CFG_CAP1_CMD_ADDR
	}
};

static unsigned long isp_cfg_ctx_addr[ISP_ID_MAX][ISP_SCENE_MAX] = { { 0 } };
static unsigned long isp_cfg_word_buf_addr[ISP_ID_MAX][ISP_SCENE_MAX]
		= { { 0 } };

unsigned long *isp_cfg_poll_addr[ISP_WM_MAX][CFG_CONTEXT_NUM] = {
	{
		&isp_cfg_ctx_addr[0][0],/* p0 */
		&isp_cfg_ctx_addr[0][1],/* c0 */
		&isp_cfg_ctx_addr[1][0],/* p1 */
		&isp_cfg_ctx_addr[1][1]/* c1 */
	},
	{
		&s_isp_regbase[0],
		&s_isp_regbase[0],
		&s_isp_regbase[1],
		&s_isp_regbase[1]
	}
};

unsigned long *isp_cfg_work_poll_addr[ISP_WM_MAX][CFG_CONTEXT_NUM] = {
	{
		&isp_cfg_word_buf_addr[0][0],/* p0 */
		&isp_cfg_word_buf_addr[0][1],/* c0 */
		&isp_cfg_word_buf_addr[1][0],/* p1 */
		&isp_cfg_word_buf_addr[1][1]/* c1 */
	},
	{
		&s_isp_regbase[0],
		&s_isp_regbase[0],
		&s_isp_regbase[1],
		&s_isp_regbase[1]
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
	0, 1, 1, ARRAY_SIZE(ISP_CFG_MAP), ISP_CFG_MAP,
	0, 1, 1, 1, 1,
	0, 0, 0, 0, 0,
};

int sprd_isp_cfg_map_init(struct isp_cfg_ctx_desc *cfg_ctx)
{
	uint32_t i = 0, idx = 0;
	uint32_t cfg_map_size = 0;
	uint32_t *cfg_map = NULL;

	idx = cfg_ctx->cur_isp_id;
	if (atomic_inc_return(&cfg_ctx->cfg_map_lock) == 1) {
		cfg_map_size = s_cfg_settings.num_of_mod;
		cfg_map = s_cfg_settings.isp_cfg_map;
		for (i = 0; i < cfg_map_size; i++) {
			ISP_HREG_WR(idx, ISP_CFG0_BUF + i * 4,
				cfg_map[i]);
			ISP_HREG_WR(idx, ISP_CFG1_BUF + i * 4,
				cfg_map[i]);
		}
	}

	return 0;
}

void sprd_isp_cfg_isp_start(struct isp_cfg_ctx_desc *cfg_ctx)
{
	enum isp_id id = cfg_ctx->cur_isp_id;
	enum isp_scene_id scene_id = cfg_ctx->cur_scene_id;
	enum cfg_context_id cfg_id = scene_id | (id << 1);

	pr_debug("isp%d start:  context  %d, P0_addr 0x%x\n", id, cfg_id,
		ISP_HREG_RD(0, ISP_CFG_PRE0_CMD_ADDR));

	switch (cfg_id) {
	case CFG_CONTEXT_P0:
		ISP_HREG_WR(id, ISP_CFG_PRE0_START, 1);
		break;
	case CFG_CONTEXT_P1:
		ISP_HREG_WR(id, ISP_CFG_PRE1_START, 1);
		break;
	case CFG_CONTEXT_C0:
		ISP_HREG_WR(id, ISP_CFG_CAP0_START, 1);
		break;
	case CFG_CONTEXT_C1:
		ISP_HREG_WR(id, ISP_CFG_CAP1_START, 1);
		break;
	default:
		break;
	}
}

int sprd_isp_cfg_block_config(struct isp_cfg_ctx_desc *cfg_ctx)
{
	int ret = ISP_RTN_SUCCESS;
	uint32_t val = 0;
	enum isp_id id = ISP_ID_0;
	enum isp_scene_id scene_id = ISP_SCENE_PRE;

	if (!cfg_ctx) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	id = cfg_ctx->cur_isp_id;
	scene_id = cfg_ctx->cur_scene_id;

	if (scene_id == ISP_SCENE_CAP) {
		s_cfg_settings.cap0_cmd_ready_mode = 1;
		s_cfg_settings.cap1_cmd_ready_mode = 1;
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

	ISP_HREG_WR(id, ISP_CFG_PAMATER, val);

	if (!s_cfg_settings.tm_bypass) {
		ISP_HREG_WR(id, ISP_CFG_TM_NUM,
			s_cfg_settings.tm_set_number);
		ISP_HREG_WR(id, ISP_CFG_CAP0_TH,
			s_cfg_settings.cap0_th);
		ISP_HREG_WR(id, ISP_CFG_CAP1_TH,
			s_cfg_settings.cap1_th);
	}

	ISP_HREG_MWR(id,
		ISP_ARBITER_ENDIAN_COMM, 0x1, 0x1);

	return ret;
}

int sprd_isp_cfg_buf_update(struct isp_cfg_ctx_desc *cfg_ctx)
{
	int ret = ISP_RTN_SUCCESS;
	unsigned long work_buf_paddr = 0;
	void *shadow_buf_vaddr = NULL;
	void *work_buf_vaddr = NULL;
	enum isp_id id = ISP_ID_0;
	enum cfg_buf_id work_buf_id;
	enum isp_scene_id scene_id = ISP_SCENE_PRE;
	struct isp_cfg_buf *cfg_buf_p;

	id = cfg_ctx->cur_isp_id;
	scene_id = cfg_ctx->cur_scene_id;

	shadow_buf_vaddr = (void *)isp_cfg_ctx_addr[id][scene_id];
	if (scene_id == ISP_SCENE_PRE || scene_id == ISP_SCENE_CAP) {
		cfg_buf_p = &cfg_ctx->cfg_buf[scene_id];

		if (cfg_buf_p->cur_buf_id == CFG_BUF_WORK_PONG) {
			if (cfg_buf_p->cmd_buf[CFG_BUF_WORK_PONG].flag == 0) {
				work_buf_id = CFG_BUF_WORK_PING;
				cfg_buf_p->cur_buf_id = CFG_BUF_WORK_PING;
			} else
				work_buf_id = CFG_BUF_WORK_PONG;
		} else {
			if (cfg_buf_p->cmd_buf[CFG_BUF_WORK_PING].flag == 0) {
				work_buf_id = CFG_BUF_WORK_PONG;
				cfg_buf_p->cur_buf_id = CFG_BUF_WORK_PONG;
			} else
				work_buf_id = CFG_BUF_WORK_PING;
		}
		isp_cfg_word_buf_addr[id][scene_id] =
			(unsigned long)
			cfg_buf_p->cmd_buf[work_buf_id].vir_addr;
		work_buf_paddr =
			(unsigned long)cfg_buf_p->cmd_buf[work_buf_id].phy_addr;
		if (unlikely(!IS_ALIGNED(work_buf_paddr, ISP_REG_SIZE))) {
			pr_err("fail to aligned with 256KB: phy addr\n");
			return -EINVAL;
		}
		work_buf_vaddr = cfg_buf_p->cmd_buf[work_buf_id].vir_addr;
		cfg_buf_p->cmd_buf[work_buf_id].flag = 1; /*buf is used*/

		memcpy(work_buf_vaddr, shadow_buf_vaddr, ISP_REG_SIZE);

		pr_debug("buf_id %d shadow_vaddr:0x%p, work (vaddr 0x%p addr 0x%lx)\n",
			work_buf_id, shadow_buf_vaddr,
			work_buf_vaddr, work_buf_paddr);

#ifdef CONFIG_64BIT
		__flush_dcache_area(work_buf_vaddr, ISP_REG_SIZE);
#else
		flush_kernel_vmap_range(work_buf_vaddr, ISP_REG_SIZE);
#endif

		ISP_HREG_WR(id, cfg_cmd_addr[id][scene_id], work_buf_paddr);
		cfg_buf_p->cmd_buf[cfg_buf_p->cur_buf_id].flag = 0;
	} else {
		pr_err("fail to get right scene id\n");
		return -EINVAL;
	}

	return ret;
}

int sprd_isp_cfg_buf_reset(struct isp_cfg_ctx_desc *cfg_ctx)
{
	int ret = ISP_RTN_SUCCESS;
	enum isp_id id = ISP_ID_0;
	enum isp_scene_id scene_id = ISP_SCENE_PRE;
	struct isp_cfg_buf *cfg_buf_p = NULL;
	int num = 0;

	id = cfg_ctx->cur_isp_id;
	scene_id = cfg_ctx->cur_scene_id;

	for (scene_id = ISP_SCENE_PRE; scene_id < ISP_SCENE_MAX; scene_id++) {
		cfg_buf_p = &cfg_ctx->cfg_buf[scene_id];
		for (num = 0; num < ISP_CFG_BUF_NUM; num++)
			memset(cfg_buf_p->cmd_buf[num].vir_addr,
				0, ISP_REG_SIZE);
	}

	return ret;
}

int sprd_isp_cfg_ctx_buf_init(struct isp_cfg_ctx_desc *cfg_ctx, uint32_t flag)
{
	enum isp_id id = ISP_ID_0;
	enum isp_scene_id scene_id = ISP_SCENE_PRE;
	struct isp_cfg_buf *cfg_buf_p = NULL;

	id = cfg_ctx->cur_isp_id;
	scene_id = cfg_ctx->cur_scene_id;

	for (scene_id = ISP_SCENE_PRE; scene_id < ISP_SCENE_MAX; scene_id++) {
		cfg_buf_p = &cfg_ctx->cfg_buf[scene_id];
		if (flag == 1) {
			isp_cfg_ctx_addr[id][scene_id] =
			(unsigned long)
			cfg_buf_p->cmd_buf[CFG_BUF_SHADOW].vir_addr;

			isp_cfg_word_buf_addr[id][scene_id] =
			(unsigned long)
			cfg_buf_p->cmd_buf[CFG_BUF_WORK_PING].vir_addr;
		} else
			isp_cfg_ctx_addr[id][scene_id] =
				(unsigned long)
				&cfg_ctx->temp_cfg_buf[scene_id][0];

		cfg_buf_p->cur_buf_id = CFG_BUF_SHADOW;
	}

	return 0;
}

int sprd_isp_cfg_ctx_init(struct isp_cfg_ctx_desc *cfg_ctx, uint32_t idx)
{
	int ret = ISP_RTN_SUCCESS;

	if (!cfg_ctx) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	cfg_ctx->cur_isp_id = ISP_GET_ISP_ID(idx);
	cfg_ctx->cur_scene_id = ISP_GET_SCENE_ID(idx);
	cfg_ctx->cur_work_mode = ISP_GET_MODE_ID(idx);

	return ret;
}

int sprd_isp_cfg_ctx_deinit(struct isp_cfg_ctx_desc *cfg_ctx)
{
	int ret = ISP_RTN_SUCCESS;

	if (!cfg_ctx) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	return ret;
}
