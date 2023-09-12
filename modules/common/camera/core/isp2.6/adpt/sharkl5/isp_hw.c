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

#include "isp_hw.h"
#ifdef CAM_HW_ADPT_LAYER

#define ISP_AXI_STOP_TIMEOUT			1000

static unsigned long irq_base[4] = {
	ISP_P0_INT_BASE,
	ISP_C0_INT_BASE,
	ISP_P1_INT_BASE,
	ISP_C1_INT_BASE
};

unsigned long cfg_cmd_addr_reg[ISP_CONTEXT_HW_NUM] = {
	ISP_CFG_PRE0_CMD_ADDR,
	ISP_CFG_CAP0_CMD_ADDR,
	ISP_CFG_PRE1_CMD_ADDR,
	ISP_CFG_CAP1_CMD_ADDR
};

static unsigned long store_base[ISP_SPATH_NUM] = {
	ISP_STORE_PRE_CAP_BASE,
	ISP_STORE_VID_BASE,
	ISP_STORE_THUMB_BASE,
};

static unsigned long scaler_base[ISP_SPATH_NUM] = {
	ISP_SCALER_PRE_CAP_BASE,
	ISP_SCALER_VID_BASE,
	ISP_SCALER_THUMB_BASE,
};

static const struct bypass_tag dcam_bypass_tab[] = {
	[_E_4IN1] = {"4in1", DCAM_MIPI_CAP_CFG,           12}, /* 0x100.b12 */
	[_E_PDAF] = {"pdaf", DCAM_PPE_FRM_CTRL0,          1}, /* 0x120.b1 */
	[_E_LSC]  = {"lsc",  DCAM_LENS_LOAD_ENABLE,       0}, /* 0x138.b0 */
	[_E_AEM]  = {"aem",  DCAM_AEM_FRM_CTRL0,          0}, /* 0x150.b0 */
	[_E_HIST] = {"hist", DCAM_HIST_FRM_CTRL0,         0}, /* 0x160.b0 */
	[_E_AFL]  = {"afl",  ISP_AFL_FRM_CTRL0,           0}, /* 0x170.b0 */
	[_E_AFM]  = {"afm",  ISP_AFM_FRM_CTRL,            0}, /* 0x1A0.b0 */
	[_E_BPC]  = {"bpc",  ISP_BPC_PARAM,               0}, /* 0x200.b0 */
	[_E_BLC]  = {"blc",  DCAM_BLC_BYPASS_CTRL,        31}, /* 0x100.b18 */
	[_E_RAND] = {"rgb", ISP_RGBG_YRANDOM_PARAMETER0, 0}, /* 0x238.b0 */
	[_E_RAND] = {"rand", ISP_RGBG_YRANDOM_PARAMETER0, 1}, /* 0x278.b1 */
	[_E_PPI]  = {"ppi",  ISP_PPI_PARAM,               0}, /* 0x284.b0 */
	[_E_AWBC] = {"awbc", ISP_AWBC_GAIN0,              31}, /* 0x380.b31 */
	[_E_NR3]  = {"nr3",  NR3_FAST_ME_PARAM,           0}, /* 0x3F0.b0 */
};

static const struct bypass_tag isp_bypass_tab[] = {
[_EISP_GC]         = {"grgb",       ISP_GRGB_CTRL,           0, 1}, /* GrGb correction */
[_EISP_NLM]        = {"nlm",        ISP_NLM_PARA,            0, 1},
[_EISP_VST]        = {"vst",        ISP_VST_PARA,            0, 1},
[_EISP_IVST]       = {"ivst",       ISP_IVST_PARA,           0, 1},
[_EISP_CFA]        = {"cfa",        ISP_CFAE_NEW_CFG0,       0, 1},
[_EISP_CMC]        = {"cmc",        ISP_CMC10_PARAM,         0, 1},
[_EISP_GAMC]       = {"gamma-c",    ISP_GAMMA_PARAM,         0, 1}, /* Gamma correction */
[_EISP_HSV]        = {"hsv",        ISP_HSV_PARAM,           0, 1},
[_EISP_HIST]       = {"hist",       ISP_HIST_PARAM,          0, 1},
[_EISP_HIST2]      = {"hist2",      ISP_HIST2_PARAM,         0, 1},
[_EISP_PSTRZ]      = {"pstrz",      ISP_PSTRZ_PARAM,         0, 1},
[_EISP_PRECDN]     = {"precdn",     ISP_PRECDN_PARAM,        0, 1},
[_EISP_YNR]        = {"ynr",        ISP_YNR_CONTRL0,         0, 1},
[_EISP_EE]         = {"ee",         ISP_EE_PARAM,            0, 1},
[_EISP_GAMY]       = {"ygamma",     ISP_YGAMMA_PARAM,        0, 1},
[_EISP_CDN]        = {"cdn",        ISP_CDN_PARAM,           0, 1},
[_EISP_POSTCDN]    = {"postcdn",    ISP_POSTCDN_COMMON_CTRL, 0, 1},
[_EISP_UVD]        = {"uvd",        ISP_UVD_PARAM,           0, 1},
[_EISP_IIRCNR]     = {"iircnr",     ISP_IIRCNR_PARAM,        0, 1},
[_EISP_YRAND]      = {"yrandom",    ISP_YRANDOM_PARAM1,      0, 1},
[_EISP_BCHS]       = {"bchs",       ISP_BCHS_PARAM,          0, 1},
[_EISP_YUVNF]      = {"yuvnf",      ISP_YUV_NF_CTRL,         0, 1},
	{"ydelay",    ISP_YDELAY_PARAM, 0, 1},
	{"cce",    ISP_CCE_PARAM, 0, 0},
	/* can't bypass when prev */
	{"scale-pre", ISP_SCALER_PRE_CAP_BASE + ISP_SCALER_CFG, 20, 0},
	{"store-pre", ISP_STORE_PRE_CAP_BASE + ISP_STORE_PARAM, 0, 0},

	{"scale-vid", ISP_SCALER_VID_BASE + ISP_SCALER_CFG, 20, 1},
	{"store-vid", ISP_STORE_VID_BASE + ISP_STORE_PARAM, 0, 1},

	{"scale-thb", ISP_SCALER_THUMB_BASE + ISP_SCALER_CFG, 20, 1},
	{"store-thb", ISP_STORE_THUMB_BASE + ISP_STORE_PARAM, 0, 1},
	/* ltm */
	{"ltm-map",   ISP_LTM_MAP_PARAM0, 0, 1},
	{"ltm-hist",  ISP_LTM_HIST_PARAM, 0, 1},
	/* 3dnr/nr3 */
	{"nr3-crop",  ISP_3DNR_MEM_CTRL_PRE_PARAM0, 0, 1},
	{"nr3-store", ISP_3DNR_STORE_PARAM, 0, 1},
	{"nr3-mem",   ISP_3DNR_MEM_CTRL_PARAM0, 0, 1},

	{"fetch",     ISP_FETCH_PARAM, 0, 0},
	{"cfg",       ISP_CFG_PAMATER, 0, 0},
};


static int isphw_bypass_count_get(void *handle, void *arg)
{
	int cnt = 0;
	uint32_t type = 0;

	type = *(uint32_t *)arg;

	switch (type) {
	case DCAM_BYPASS_TYPE:
		cnt = sizeof(dcam_bypass_tab) /
			sizeof(dcam_bypass_tab[0]);
		break;
	case ISP_BYPASS_TYPE:
		cnt = sizeof(isp_bypass_tab) /
			sizeof(isp_bypass_tab[0]);
		break;
	default:
		pr_err("fail to support bypass type %d\n", type);
		break;
	}

	if (cnt == 0)
		pr_err("fail to get valid bypass %d\n", type);

	return cnt;
}

static int isphw_bypass_data_get(void *handle, void *arg)
{
	struct bypass_tag *bypass = NULL;
	struct cam_hw_bypass_data *data = NULL;

	data = (struct cam_hw_bypass_data *)arg;

	switch (data->type) {
	case DCAM_BYPASS_TYPE:
		bypass = (struct bypass_tag *)&dcam_bypass_tab[data->i];
		break;
	case ISP_BYPASS_TYPE:
		bypass = (struct bypass_tag *)&isp_bypass_tab[data->i];
		break;
	default:
		pr_err("fail to support bypass type %d\n", data->type);
		break;
	}

	if (bypass == NULL){
		pr_err("fail to get valid block func %d\n", data->type);
		return -EFAULT;
	}

	data->tag = bypass;

	return 0;
}

static uint32_t cam_reg_trace_tab[] = {
	DCAM_CFG,
	DCAM_APB_SRAM_CTRL,
	DCAM_IMAGE_CONTROL,
	DCAM_PDAF_CONTROL,
	DCAM_LENS_LOAD_ENABLE,
	ISP_BPC_PARAM,
	DCAM_AEM_FRM_CTRL0,
	ISP_AFM_FRM_CTRL,
	ISP_AFL_FRM_CTRL0,
	DCAM_HIST_FRM_CTRL0,
	NR3_FAST_ME_PARAM,
	DCAM_FULL_BASE_WADDR,
	DCAM_BIN_BASE_WADDR0,
	DCAM_PDAF_BASE_WADDR,
	DCAM_VCH2_BASE_WADDR,
	DCAM_VCH3_BASE_WADDR,
	DCAM_AEM_BASE_WADDR,
	DCAM_HIST_BASE_WADDR,
	DCAM_PPE_RIGHT_WADDR,
	ISP_AFL_GLB_WADDR,
	ISP_AFL_REGION_WADDR,
	ISP_BPC_OUT_ADDR,
	ISP_AFM_BASE_WADDR,
	ISP_NR3_WADDR,
};

static int isphw_reg_trace(void *handle, void *arg)
{
	unsigned long addr = 0;
	uint32_t val_mmu, val[8], i, j, n, cnt;
	struct cam_hw_reg_trace *trace = NULL;

	trace = (struct cam_hw_reg_trace *)arg;

	if (trace->type == NORMAL_REG_TRACE) {
		goto normal_reg_trace;
	} else if (trace->type == ABNORMAL_REG_TRACE) {
		goto abnormal_reg_trace;
	} else {
		pr_err("fail to get valid type %d\n", trace->type);
		return -EFAULT;
	}

abnormal_reg_trace:
	if (trace->idx >= DCAM_HW_CONTEXT_MAX) {
		pr_err("fail to get vaild dcam hw idx %d\n", trace->idx);
		return 0;
	}

	pr_info("DCAM%d: Register list\n", trace->idx);
	for (addr = DCAM_IP_REVISION; addr <= ISP_AFL_SUM2;
		addr += 16) {
		pr_info("0x%03lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			DCAM_REG_RD(trace->idx, addr),
			DCAM_REG_RD(trace->idx, addr + 4),
			DCAM_REG_RD(trace->idx, addr + 8),
			DCAM_REG_RD(trace->idx, addr + 12));
	}
	pr_info("AXIM: Register list\n");
	for (addr = AXIM_CTRL; addr <= IMG_FETCH_RADDR;
		addr += 16) {
		pr_info("0x%03lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			DCAM_AXIM_RD(addr),
			DCAM_AXIM_RD(addr + 4),
			DCAM_AXIM_RD(addr + 8),
			DCAM_AXIM_RD(addr + 12));
	}

	pr_info("ISP: Register list\n");
	for (addr = ISP_INT_EN0; addr <= ISP_INT_ALL_DONE_SRC_CTRL;
		addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
	}

	pr_info("ISP fetch: register list\n");
	for (addr = ISP_FETCH_PARAM; addr <= ISP_FETCH_START; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
	}

	pr_info("ISP dispatch: register list\n");
	for (addr = ISP_DISPATCH_CH0_BAYER; addr <= ISP_DISPATCH_CHK_SUM;
		addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
	}

	pr_info("ISP scaler: pre_cap register list\n");
	for (addr = ISP_SCALER_PRE_CAP_BASE + ISP_SCALER_CFG; addr <= ISP_SCALER_PRE_CAP_BASE + ISP_SCALER_REGULAR_CFG;
			addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
	}

	pr_info("ISP scaler: vid register list\n");
	for (addr = ISP_SCALER_VID_BASE + ISP_SCALER_CFG; addr <= ISP_SCALER_VID_BASE + ISP_SCALER_REGULAR_CFG;
			addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
	}

	pr_info("ISP store: register list\n");
	for (addr = ISP_STORE_PRE_CAP_BASE + ISP_STORE_PARAM; addr <= ISP_STORE_PRE_CAP_BASE + ISP_STORE_SHADOW_CLR;
			addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
	}


	pr_info("ISP mmu: register list\n");
	for (addr = ISP_MMU_INT_EN; addr <= ISP_MMU_INT_RAW;
			addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(addr),
			ISP_HREG_RD(addr + 4),
			ISP_HREG_RD(addr + 8),
			ISP_HREG_RD(addr + 12));
	}


normal_reg_trace:
	val_mmu = DCAM_MMU_RD(MMU_EN);
	cnt = sizeof(cam_reg_trace_tab) /
		sizeof(cam_reg_trace_tab[0]);
	pr_info("dcam%d: 0x%08x, cnt %d\n", trace->idx, val_mmu, cnt);

	for (i = 0; i < cnt; i += 8) {
		memset(val, 0, sizeof(val));
		n = ((cnt - i) < 8) ? (cnt - i) : 8;
		for (j = 0; j < n; j++) {
			addr = cam_reg_trace_tab[i + j];
			val[j] = DCAM_REG_RD(trace->idx, addr);
		}
		pr_info("n=%d, %08x %08x %08x %08x %08x %08x %08x %08x\n", n,
			val[0], val[1], val[2], val[3], val[4], val[5], val[6], val[7]);
	}
	return 0;
}

static int isphw_clk_eb(void *handle, void *arg)
{
	int ret = 0;
	struct cam_hw_info *hw = NULL;
	struct cam_hw_soc_info *soc = NULL;

	pr_debug(",E\n");
	if (!handle) {
		pr_err("fail to get invalid hw\n");
		return -EINVAL;
	}

	hw = (struct cam_hw_info *)handle;
	soc = hw->soc_isp;
	ret = clk_set_parent(soc->clk, soc->clk_parent);
	if (ret) {
		pr_err("fail to set parent, ret = %d\n", ret);
		clk_set_parent(soc->clk, soc->clk_default);
		return ret;
	}
	ret = clk_prepare_enable(soc->clk);
	if (ret) {
		pr_err("fail to enable isp clk, ret = %d\n", ret);
		clk_set_parent(soc->clk, soc->clk_default);
		return ret;
	}
	ret = clk_prepare_enable(soc->core_eb);
	if (ret) {
		pr_err("fail to set isp eb, ret = %d\n", ret);
		clk_disable_unprepare(soc->clk);
		return ret;
	}
	ret = clk_prepare_enable(soc->axi_eb);
	if (ret) {
		pr_err("fail to set isp axi eb, ret = %d\n", ret);
		clk_disable_unprepare(soc->clk);
		clk_disable_unprepare(soc->core_eb);
		return ret;
	}

	return ret;
}

static int isphw_clk_dis(void *handle, void *arg)
{
	int ret = 0;
	struct cam_hw_info *hw = NULL;
	struct cam_hw_soc_info *soc = NULL;

	pr_debug(",E\n");
	if (!handle) {
		pr_err("fail to get valid hw\n");
		return -EINVAL;
	}

	hw = (struct cam_hw_info *)handle;
	soc = hw->soc_isp;
	clk_set_parent(soc->clk, soc->clk_default);
	clk_disable_unprepare(soc->clk);
	clk_disable_unprepare(soc->axi_eb);
	clk_disable_unprepare(soc->core_eb);

	return ret;
}

static int isphw_reset(void *handle, void *arg)
{
	int rtn = 0;
	uint32_t cid;
	uint32_t time_out = 0;
	uint32_t flag = 0;
	struct cam_hw_soc_info *soc = NULL;
	struct cam_hw_ip_info *ip = NULL;
	struct cam_hw_info *hw = NULL;

	if (!handle) {
		pr_err("fail to get valid hw\n");
		return -EINVAL;
	}

	hw = (struct cam_hw_info *)handle;
	soc = hw->soc_isp;
	ip = hw->ip_isp;
	pr_info("ISP%d: reset\n", ip->idx);

	/* firstly stop axim transfering */
	ISP_HREG_MWR(ISP_AXI_ITI2AXIM_CTRL, BIT(26), BIT(26));

	/* then wait for AHB busy cleared */
	while (++time_out < ISP_AXI_STOP_TIMEOUT) {
		/* bit3: 1 - axi idle;  0 - axi busy */
		if  (ISP_HREG_RD(ISP_INT_STATUS) & BIT_3)
			break;
		udelay(1000);
	}

	if (time_out >= ISP_AXI_STOP_TIMEOUT) {
		pr_info("ISP reset timeout %d\n", time_out);
	} else {
		flag = ip->syscon.rst_mask
			| ip->syscon.rst_ahb_mask
			| ip->syscon.rst_vau_mask;
		regmap_update_bits(soc->cam_ahb_gpr,
			ip->syscon.rst, flag, flag);
		udelay(10);
		regmap_update_bits(soc->cam_ahb_gpr,
			ip->syscon.rst, flag, ~flag);
	}

	/* enable axim transfering */
	ISP_HREG_MWR(ISP_AXI_ITI2AXIM_CTRL, BIT_26, 0);

	for (cid = 0; cid < 4; cid++) {
		hw->isp_ioctl(hw, ISP_HW_CFG_DISABLE_IRQ, &cid);
		hw->isp_ioctl(hw, ISP_HW_CFG_CLEAR_IRQ, &cid);
	}

	pr_info("ISP%d: reset end\n", ip->idx);
	return rtn;
}

static int isphw_irq_enable(void *handle, void *arg)
{
	uint32_t ctx_id;
	uint32_t mask = ~0;

	if (!arg) {
		pr_err("fail to get valid hw\n");
		return -EFAULT;
	}

	ctx_id = *(uint32_t *)arg;
	if (ctx_id >= 4) {
		pr_err("fail to get valid ctx id %d\n", ctx_id);
		return -EFAULT;
	}

	ISP_HREG_MWR(irq_base[ctx_id] + ISP_INT_EN0, mask, mask);

	return 0;
}

static int isphw_irq_disable(void *handle, void *arg)
{
	uint32_t ctx_id;

	if (!arg) {
		pr_err("fail to get valid hw\n");
		return -EFAULT;
	}

	ctx_id = *(uint32_t *)arg;
	if (ctx_id >= 4) {
		pr_err("fail to get valid  ctx id %d\n", ctx_id);
		return -EFAULT;
	}

	ISP_HREG_WR(irq_base[ctx_id] + ISP_INT_EN0, 0);
	ISP_HREG_WR(irq_base[ctx_id] + ISP_INT_EN1, 0);

	return 0;
}

static int isphw_irq_clear(void *handle, void *arg)
{
	uint32_t ctx_id;

	if (!arg) {
		pr_err("fail to get valid hw\n");
		return -EFAULT;
	}

	ctx_id = *(uint32_t *)arg;
	if (ctx_id >= 4) {
		pr_err("fail to get valid ctx id %d\n", ctx_id);
		return -EFAULT;
	}

	ISP_HREG_WR(irq_base[ctx_id] + ISP_INT_CLR0, 0xFFFFFFFF);
	ISP_HREG_WR(irq_base[ctx_id] + ISP_INT_CLR1, 0xFFFFFFFF);

	return 0;
}

static uint32_t ISP_CFG_MAP[] __aligned(8) = {
		0x00080710, /*0x0710  - 0x0714 , 2   , common path sel*/
		0x00381B10, /*0x1B10  - 0x1B44 , 14  , GRGB*/
		0x00041C10, /*0x1C10  - 0x1C10 , 1   , VST*/
		0x01AC2010, /*0x2010  - 0x21B8 , 107 , NLM*/
		0x00041E10, /*0x1E10  - 0x1E10 , 1   , IVST*/
		0x004C3010, /*0x3010  - 0x3058 , 19  , CFA_NEW*/
		0x00183110, /*0x3110  - 0x3124 , 6   , CMC10*/
		0x00043210, /*0x3210  - 0x3210 , 1   , GAMC_NEW*/
		0x005C3310, /*0x3310  - 0x3368 , 23  , HSV*/
		0x00143710, /*0x3710  - 0x3720 , 5   , HISTS*/
		0x00043410, /*0x3410  - 0x3410 , 1   , PSTRZ*/
		0x001C3510, /*0x3510  - 0x3528 , 7   , CCE*/
		0x001C3610, /*0x3610  - 0x3628 , 7   , UVD*/
		0x00205510, /*0x5510  - 0x552C , 8   , LTM HISTS*/
		0x00185F10, /*0x5F10  - 0x5F24 , 6   , LTM MAP*/
		0x004C5010, /*0x5010  - 0x5058 , 19  , PRECDN*/
		0x008C5110, /*0x5110  - 0x5198 , 35  , YNR*/
		0x00485610, /*0x5610  - 0x5654 , 18  , CDN*/
		0x00D85710, /*0x5710  - 0x57E4 , 54  , NEW_EE*/
		0x00745A10, /*0x5A10  - 0x5A80 , 29  , POST_CDN*/
		0x00045B10, /*0x5B10  - 0x5B10 , 1   , YGAMMA*/
		0x00085C10, /*0x5C10  - 0x5C14 , 2   , YUVDELAY*/
		0x00C85D10, /*0x5D10  - 0x5DD4 , 50  , IIRCNR*/
		0x00185E10, /*0x5E10  - 0x5E24 , 6   , YRANDOM*/
		0x00185910, /*0x5910  - 0x5924 , 6   , BCHS*/
		0x00449010, /*0x9010  - 0x9050 , 17  , 3DNR mem ctrl*/
		0x00649110, /*0x9110  - 0x9170 , 25  , 3DNR blend*/
		0x00189210, /*0x9210  - 0x9224 , 6   , 3DNR store*/
		0x00109310, /*0x9310  - 0x931C , 4   , 3DNR crop*/
		0x002C9410, /*0x9410  - 0x9438 , 11  , FBC 3DNR store*/
		0x003C9510, /*0x9510  - 0x9548 , 15  , FBD 3DNR fetch*/
		0x0050D010, /*0xD010  - 0xD05C , 20  , SCL_VID*/
		0x0030D110, /*0xD110  - 0xD13C , 12  , SCL_VID_store*/
		/*0x0030D310, *0xD310  - 0xD33C , 12  , SCL_VID_FBC_store*/
		0x0044C010, /*0xC010  - 0xC050 , 17  , SCL_CAP*/
		0x0030C110, /*0xC110  - 0xC13C , 12  , SCL_CAP_store*/
		0x0044C210, /*0xC210  - 0xC250 , 17  , SCL_CAP_Noisefilter_add_rdm*/
		/*0x0030C310, *0xC310  - 0xC33C , 12  , SCL_CAP_FBC_store*/
		0x0054E010, /*0xE010  - 0xE060 , 21  , SCL_THUMB*/
		0x0030E110, /*0xE110  - 0xE13C , 12  , SCL_THUMB_store*/
		0x00300110, /*0x110   - 0x13C  , 12  , FETCH*/
		0x00300210, /*0x210   - 0x23C  , 12  , STORE*/
		0x001C0310, /*0x310   - 0x328  , 7   , DISPATCH*/
		0x00340C10, /*0x0C10  - 0x0C40 , 13  , Fetch_FBD*/
		0x05A18000, /*0x18000 - 0x1859C, 360 , ISP_HSV_BUF0_CH0*/
		0x10019000, /*0x19000 - 0x19FFC, 1024, ISP_VST_BUF0_CH0*/
		0x1001A000, /*0x1A000 - 0x1AFFC, 1024, ISP_IVST_BUF0_CH0*/
		0x0401B000, /*0x1B000 - 0x1B3FC, 256 , ISP_FGAMMA_R_BUF0_CH0*/
		0x0401C000, /*0x1C000 - 0x1C3FC, 256 , ISP_FGAMMA_G_BUF0_CH0*/
		0x0401D000, /*0x1D000 - 0x1D3FC, 256 , ISP_FGAMMA_B_BUF0_CH0*/
		0x0201B400, /*0x1B400 - 0x1B5FC, 128 , ISP_PSTRZ_R_BUF0_CH0*/
		0x0201C400, /*0x1C400 - 0x1C5FC, 128 , ISP_PSTRZ_G_BUF0_CH0*/
		0x0201D400, /*0x1D400 - 0x1D5FC, 128 , ISP_PSTRZ_B_BUF0_CH0*/
		0x0205E000, /*0x1E000 - 0x1E200, 129 , ISP_YGAMMA_BUF0_CH0*/
		0x00839100, /*0x39100 - 0x3917C, 32  , CAP_HOR_CORF_Y_BUF0_CH0*/
		0x00439300, /*0x39300 - 0x3933C, 16  , CAP_HOR_CORF_UV_BUF0*/
		0x021394F0, /*0x394F0 - 0x396FC, 132 , CAP_VER_CORF_Y_BUF0_CH0*/
		0x02139AF0, /*0x39AF0 - 0x39CFC, 132 , CAP_VER_CORF_UV_BUF0*/
		0x00838100, /*0x38100 - 0x3817C, 32  , VID_HOR_CORF_Y_BUF0_CH0*/
		0x00438300, /*0x38300 - 0x3833C, 16  , VID_HOR_CORF_UV_BUF0*/
		0x021384F0, /*0x384F0 - 0x386FC, 132 , VID_VER_CORF_Y_BUF0_CH0*/
		0x02138AF0, /*0x38AF0 - 0x38CFC, 132 , VID_VER_CORF_UV_BUF0*/
};

static int isphw_cfg_map_info_get(void *handle, void *arg)
{
	struct isp_dev_cfg_info *info = NULL;

	if (!arg) {
		pr_err("fail to get valid arg\n");
		return -EFAULT;
	}

	info = (struct isp_dev_cfg_info *)arg;
	info->num_of_mod = ARRAY_SIZE(ISP_CFG_MAP);
	info->isp_cfg_map = ISP_CFG_MAP;

	return 0;
}

static int isphw_default_param_set(void *handle, void *arg)
{
	uint32_t idx = 0;
	uint32_t bypass = 1;
	uint32_t wqos_val = 0;
	uint32_t rqos_val = 0;
	struct isp_hw_default_param *param = NULL;
	struct cam_hw_info *hw = NULL;

	param = (struct isp_hw_default_param *)arg;
	hw = (struct cam_hw_info *)handle;

	if (param->type == ISP_HW_PARA) {
		goto isp_hw_para;
	} else if (param->type == ISP_CFG_PARA) {
		idx = param->index;
		goto isp_cfg_para;
	} else {
		pr_err("fail to get valid type %d\n", param->type);
	}


isp_hw_para:
	wqos_val = (0x1 << 13) | (0x0 << 12) | (0x4 << 8) |
			((hw->soc_isp->awqos_high & 0xF) << 4) |
			(hw->soc_isp->awqos_low & 0xF);
	rqos_val = (0x0 << 8) |
			((hw->soc_isp->arqos_high & 0xF) << 4) |
			(hw->soc_isp->arqos_low & 0xF);
	ISP_HREG_MWR(ISP_AXI_ARBITER_WQOS,
					0x37FF,
					wqos_val);
	ISP_HREG_MWR(ISP_AXI_ARBITER_RQOS,
					0x1FF,
					rqos_val);
	ISP_HREG_WR(ISP_CORE_PMU_EN, 0xFFFF0000);
	ISP_HREG_WR(ISP_COMMON_GCLK_CTRL_0, 0xFFFF0000);
	ISP_HREG_WR(ISP_COMMON_GCLK_CTRL_1, 0xFFFF0000);
	ISP_HREG_WR(ISP_COMMON_GCLK_CTRL_2, 0xFFFF0000);
	ISP_HREG_WR(ISP_COMMON_GCLK_CTRL_3, 0xFF00);
	ISP_HREG_MWR(ISP_AXI_ISOLATION, BIT_0, 0);
	ISP_HREG_MWR(ISP_ARBITER_ENDIAN0, BIT_0, 0);
	ISP_HREG_MWR(ISP_ARBITER_ENDIAN1, BIT(0) | BIT(1), 0);
	ISP_HREG_WR(ISP_ARBITER_CHK_SUM_CLR, 0xF10);
	ISP_HREG_WR(ISP_ARBITER_CHK_SUM0, 0x0);
	/* enable axim transfering */
	ISP_HREG_MWR(ISP_AXI_ITI2AXIM_CTRL, BIT_26, 0);
	/* to be defined. */
	ISP_HREG_MWR(
		ISP_COMMON_SHADOW_CTRL_CH0, BIT_16, (1 << 16));
	ISP_HREG_MWR(
		ISP_COMMON_SHADOW_CTRL_CH0, BIT_21, (0 << 21));
	ISP_HREG_MWR(ISP_COMMON_PMU_RAM_MASK, BIT_0, 1);
	ISP_HREG_MWR(ISP_BLOCK_MODE, 0xF, 0);
	/* dispatch_done should be disable? */
	ISP_HREG_MWR(ISP_INT_ALL_DONE_CTRL, 0x1F, 0x1C);
	/* bypass config mode by default */
	ISP_HREG_MWR(ISP_CFG_PAMATER, BIT_0, 1);

	pr_debug("end\n");
	return 0;

isp_cfg_para:
	ISP_REG_MWR(idx, ISP_STORE_DEBUG_BASE + ISP_STORE_PARAM, BIT_0, bypass);

	/* bypass all path scaler & store */
	ISP_REG_MWR(idx, ISP_SCALER_PRE_CAP_BASE + ISP_SCALER_CFG,
		BIT_31, 0 << 31);
	ISP_REG_MWR(idx, ISP_SCALER_VID_BASE + ISP_SCALER_CFG,
		BIT_31, 0 << 31);

	ISP_REG_MWR(idx, ISP_STORE_PRE_CAP_BASE + ISP_STORE_PARAM,
		BIT_0, 1);
	ISP_REG_MWR(idx, ISP_STORE_VID_BASE + ISP_STORE_PARAM,
		BIT_0, 1);
	ISP_REG_MWR(idx, ISP_STORE_THUMB_BASE + ISP_STORE_PARAM,
		BIT_0, 1);

	/* default bypass all blocks */
	ISP_REG_MWR(idx, ISP_NLM_PARA, BIT_0, bypass);
	ISP_REG_MWR(idx, ISP_VST_PARA, BIT_0, bypass);
	ISP_REG_MWR(idx, ISP_IVST_PARA, BIT_0, bypass);

	ISP_REG_MWR(idx, ISP_CMC10_PARAM, BIT_0, bypass);
	ISP_REG_MWR(idx, ISP_GAMMA_PARAM, BIT_0, bypass);
	ISP_REG_MWR(idx, ISP_HSV_PARAM, BIT_0, bypass);
	ISP_REG_MWR(idx, ISP_PSTRZ_PARAM, BIT_0, bypass);
	ISP_REG_MWR(idx, ISP_CCE_PARAM, BIT_0, bypass);

	ISP_REG_MWR(idx, ISP_UVD_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_PRECDN_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_YNR_CONTRL0, BIT_0|BIT_1, 0x3);
	ISP_REG_MWR(idx, ISP_HIST_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_HIST_CFG_READY, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_HIST2_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_HIST2_CFG_RDY, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_CFAE_NEW_CFG0, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_EE_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_GRGB_CTRL, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_YUV_NF_CTRL, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_LTM_HIST_YUV_BASE +
		ISP_LTM_HIST_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_LTM_MAP_YUV_BASE +
		ISP_LTM_MAP_PARAM0, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_CDN_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_EE_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_BCHS_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_POSTCDN_COMMON_CTRL, BIT_0|BIT_1, 0x3);
	ISP_REG_MWR(idx, ISP_YGAMMA_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_IIRCNR_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_YRANDOM_PARAM1, BIT_0, 1);

	/* 3DNR bypass */
	ISP_REG_MWR(idx, ISP_3DNR_MEM_CTRL_PARAM0, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_3DNR_BLEND_CONTROL0, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_3DNR_STORE_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_3DNR_MEM_CTRL_PRE_PARAM0, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_FBC_3DNR_STORE_PARAM, BIT_0, 1);

	/*CFA*/
	ISP_REG_MWR(idx, ISP_CFAE_NEW_CFG0, BIT_0, 0);
	ISP_REG_WR(idx, ISP_CFAE_INTP_CFG0, 0x1F4 | 0x1F4 << 16);
	ISP_REG_WR(idx, ISP_CFAE_INTP_CFG1,
		(0x1 << 31) | (0x14 << 12) | (0x7F << 4) | 0x4);
	ISP_REG_WR(idx, ISP_CFAE_INTP_CFG2, 0x8 | (0x0 << 8));
	ISP_REG_WR(idx, ISP_CFAE_INTP_CFG3,
		(0x8 << 20) | (0x8 << 12) | 0x118);
	ISP_REG_WR(idx, ISP_CFAE_INTP_CFG4, 0x64 | (0x64 << 16));
	ISP_REG_WR(idx, ISP_CFAE_INTP_CFG5, 0xC8 | (0xC8 << 16));

	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG0, 0x64 | (0x96 << 16));
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG1, 0x14 | (0x5 << 16));
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG2,
		(0x28 << 20) | (0x1E << 10) | 0xF);
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG3, 0xC8);
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG4, (0x5 << 10));
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG5, (0x50 << 9) | 0x78);
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG6,
		(0x32 << 18) | (0x32 << 9));
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG7, (0x64 << 18));
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG8, 0x3C | (0x8 << 17));
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG9,
		(0x1FC << 20) | (0x134 << 10) | 0x27C);
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG10,
		(0x214 << 20) | (0x1FF << 10) | 0x1CD);
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG11, 0x22D << 10 | 0x205);

	/*CCE*/
	ISP_REG_MWR(idx, ISP_CCE_PARAM, BIT_0, 0);
	ISP_REG_WR(idx, ISP_CCE_MATRIX0, (150 << 11) | 77);
	ISP_REG_WR(idx, ISP_CCE_MATRIX1, ((-43) << 11) | 29);
	ISP_REG_WR(idx, ISP_CCE_MATRIX2, 0x407AB);
	ISP_REG_WR(idx, ISP_CCE_MATRIX3, ((-107) << 11) | 128);
	ISP_REG_WR(idx, ISP_CCE_MATRIX4, (-21));
	ISP_REG_WR(idx, ISP_CCE_SHIFT, 0);

	pr_info("end\n");
	return 0;
}

static int isphw_path_store(void *handle, void *arg)
{
	int ret = 0;
	uint32_t val = 0;
	uint32_t idx = 0;
	struct isp_hw_path_store *path_store = NULL;
	struct isp_store_info *store_info = NULL;
	unsigned long addr = 0;

	path_store = (struct isp_hw_path_store *)arg;
	idx = path_store->ctx_id;
	store_info = &path_store->store;
	addr = store_base[path_store->spath_id];

	pr_debug("isp set store in.  bypass %d, path_id:%d, w:%d,h:%d\n",
			store_info->bypass, path_store->spath_id,
			store_info->size.w, store_info->size.h);

	ISP_REG_MWR(idx, addr + ISP_STORE_PARAM,
		BIT_0, store_info->bypass);
	if (store_info->bypass)
		return 0;

	ISP_REG_MWR(idx, addr + ISP_STORE_PARAM,
		BIT_1, (store_info->max_len_sel << 1));
	ISP_REG_MWR(idx, addr + ISP_STORE_PARAM,
		BIT_2, (store_info->speed_2x << 2));
	ISP_REG_MWR(idx, addr + ISP_STORE_PARAM,
		BIT_3, (store_info->mirror_en << 3));
	ISP_REG_MWR(idx, addr + ISP_STORE_PARAM,
		0xF0, (store_info->color_fmt << 4));
	ISP_REG_MWR(idx, addr + ISP_STORE_PARAM,
		0x300, (store_info->endian << 8));

	val = ((store_info->size.h & 0xFFFF) << 16) |
		(store_info->size.w & 0xFFFF);
	ISP_REG_WR(idx, addr + ISP_STORE_SLICE_SIZE, val);

	ISP_REG_WR(idx, addr + ISP_STORE_BORDER, 0);
	ISP_REG_WR(idx, addr + ISP_STORE_Y_PITCH, store_info->pitch.pitch_ch0);
	ISP_REG_WR(idx, addr + ISP_STORE_U_PITCH, store_info->pitch.pitch_ch1);
	ISP_REG_WR(idx, addr + ISP_STORE_V_PITCH, store_info->pitch.pitch_ch2);

	pr_debug("set_store size %d %d\n",
		store_info->size.w, store_info->size.h);

	ISP_REG_MWR(idx, addr + ISP_STORE_READ_CTRL,
		0x3, store_info->rd_ctrl);
	ISP_REG_MWR(idx, addr + ISP_STORE_READ_CTRL,
		0xFFFFFFFC, store_info->store_res << 2);

	ISP_REG_MWR(idx, addr + ISP_STORE_SHADOW_CLR_SEL,
		BIT_1, store_info->shadow_clr_sel << 1);
	ISP_REG_MWR(idx, addr + ISP_STORE_SHADOW_CLR,
		BIT_0, store_info->shadow_clr);

	return ret;
}

static void isphw_path_shrink_info_set(
			uint32_t idx, unsigned long  scaler_base,
			struct isp_regular_info *regular_info)
{
	unsigned long addr = 0;
	uint32_t reg_val = 0;

	pr_debug("regular_mode %d\n", regular_info->regular_mode);
	addr = ISP_SCALER_CFG + scaler_base;
	ISP_REG_MWR(idx, addr, (BIT_25 | BIT_26),
		regular_info->regular_mode << 25);

	/*TBD
	 * the value need to update.
	 */
	if (regular_info->regular_mode == DCAM_REGULAR_SHRINK) {
		regular_info->shrink_y_up_th = SHRINK_Y_UP_TH;
		regular_info->shrink_y_dn_th = SHRINK_Y_DN_TH;
		regular_info->shrink_uv_up_th = SHRINK_UV_UP_TH;
		regular_info->shrink_uv_dn_th = SHRINK_UV_DN_TH;
		addr = ISP_SCALER_SHRINK_CFG + scaler_base;
		reg_val = ((regular_info->shrink_uv_dn_th & 0xFF) << 24) |
			((regular_info->shrink_uv_up_th & 0xFF) << 16);
		reg_val |= ((regular_info->shrink_y_dn_th  & 0xFF) << 8) |
			((regular_info->shrink_y_up_th & 0xFF));
		ISP_REG_WR(idx, addr, reg_val);

		regular_info->shrink_y_offset = SHRINK_Y_OFFSET;
		regular_info->shrink_y_range = SHRINK_Y_RANGE;
		regular_info->shrink_c_offset = SHRINK_C_OFFSET;
		regular_info->shrink_c_range = SHRINK_C_RANGE;
		addr = ISP_SCALER_REGULAR_CFG + scaler_base;
		reg_val = ((regular_info->shrink_c_range & 0xF) << 24) |
			((regular_info->shrink_c_offset & 0x1F) << 16);
		reg_val |= ((regular_info->shrink_y_range & 0xF) << 8) |
			(regular_info->shrink_y_offset & 0x1F);
		ISP_REG_WR(idx, addr, reg_val);
	} else if (regular_info->regular_mode == DCAM_REGULAR_CUT) {
		addr = ISP_SCALER_SHRINK_CFG + scaler_base;
		reg_val = ((regular_info->shrink_uv_dn_th & 0xFF) << 24) |
			((regular_info->shrink_uv_up_th & 0xFF) << 16);
		reg_val |= ((regular_info->shrink_y_dn_th  & 0xFF) << 8) |
			((regular_info->shrink_y_up_th & 0xFF));
		ISP_REG_WR(idx, addr, reg_val);
	} else if (regular_info->regular_mode == DCAM_REGULAR_EFFECT) {
		addr = ISP_SCALER_EFFECT_CFG + scaler_base;
		reg_val = ((regular_info->effect_v_th & 0xFF) << 16) |
				((regular_info->effect_u_th & 0xFF) << 8);
		reg_val |= (regular_info->effect_y_th & 0xFF);
		ISP_REG_WR(idx, addr, reg_val);
	} else
		pr_debug("regular_mode %d\n", regular_info->regular_mode);
}

static int isphw_path_scaler_coeff_set(
			uint32_t idx, unsigned long  scaler_base,
			uint32_t *coeff_buf,
			uint32_t spath_id)
{
	int i = 0, rtn = 0;
	uint32_t buf_sel;
	struct coeff_arg arg;

	/* ping pong buffer. */
	buf_sel = ISP_REG_RD(idx, scaler_base + ISP_SCALER_CFG);
	buf_sel = (~((buf_sel & BIT_30) >> 30)) & 1;

	/* temp set: config mode always select buf 0 */
	buf_sel = 0;

	arg.h_coeff = coeff_buf;
	arg.v_coeff = coeff_buf + (ISP_SC_COEFF_COEF_SIZE / 4);
	arg.v_chroma_coeff = arg.v_coeff + (ISP_SC_COEFF_COEF_SIZE / 4);
	arg.h_coeff_addr = coff_buf_addr[buf_sel][spath_id][0];
	arg.h_chroma_coeff_addr = coff_buf_addr[buf_sel][spath_id][1];
	arg.v_coeff_addr = coff_buf_addr[buf_sel][spath_id][2];
	arg.v_chroma_coeff_addr = coff_buf_addr[buf_sel][spath_id][3];

	for (i = 0; i < ISP_SC_COEFF_H_NUM; i++) {
		ISP_REG_WR(idx, arg.h_coeff_addr, *arg.h_coeff);
		arg.h_coeff_addr += 4;
		arg.h_coeff++;
	}

	for (i = 0; i < ISP_SC_COEFF_H_CHROMA_NUM; i++) {
		ISP_REG_WR(idx, arg.h_chroma_coeff_addr, *arg.h_coeff);
		arg.h_chroma_coeff_addr += 4;
		arg.h_coeff++;
	}

	for (i = 0; i < ISP_SC_COEFF_V_NUM; i++) {
		ISP_REG_WR(idx, arg.v_coeff_addr, *arg.v_coeff);
		arg.v_coeff_addr += 4;
		arg.v_coeff++;
	}

	for (i = 0; i < ISP_SC_COEFF_V_CHROMA_NUM; i++) {
		ISP_REG_WR(idx, arg.v_chroma_coeff_addr, *arg.v_chroma_coeff);
		arg.v_chroma_coeff_addr += 4;
		arg.v_chroma_coeff++;
	}

	ISP_REG_MWR(idx, scaler_base + ISP_SCALER_CFG,
			BIT_30, buf_sel << 30);

	pr_debug("end. buf_sel %d\n", buf_sel);
	return rtn;
}

static int isphw_path_scaler(void *handle, void *arg)
{
	uint32_t reg_val, idx;
	struct isp_hw_path_scaler *path_scaler = NULL;
	struct yuv_scaler_info *scalerInfo = NULL;
	struct img_deci_info *deciInfo = NULL;
	unsigned long addr;
	uint32_t path_mask[ISP_SPATH_NUM] = {
		BIT_1 | BIT_0,
		BIT_3 | BIT_2,
		BIT_5 | BIT_4
	};
	uint32_t path_off[ISP_SPATH_NUM] = {0, 2, 4};

	path_scaler = (struct isp_hw_path_scaler *)arg;
	scalerInfo = &path_scaler->scaler;
	idx = path_scaler->ctx_id;
	deciInfo = &path_scaler->deci;
	addr = scaler_base[path_scaler->spath_id];

	ISP_REG_MWR(idx, ISP_COMMON_SCL_PATH_SEL,
		path_mask[path_scaler->spath_id],
		(0 << path_off[path_scaler->spath_id]));

	/* set path_eb*/
	ISP_REG_MWR(idx, addr + ISP_SCALER_CFG,
		BIT_31, 1 << 31); /* path enable */
	ISP_REG_MWR(idx, addr + ISP_SCALER_CFG,
		BIT_30, 0 << 30); /* CLK_SWITCH*/
	ISP_REG_MWR(idx, addr + ISP_SCALER_CFG,
		BIT_29, 0 << 29); /* sw_switch_en*/
	ISP_REG_MWR(idx, addr + ISP_SCALER_CFG,
		BIT_9, 0 << 9); /* bypass all scaler */
	ISP_REG_MWR(idx, addr + ISP_SCALER_CFG,
		BIT_8, 0 << 8); /* scaler path stop */
	ISP_REG_MWR(idx, addr + ISP_SCALER_CFG,
		BIT_10, path_scaler->uv_sync_v << 10);

	ISP_REG_MWR(idx, addr + ISP_SCALER_CFG, (BIT_23 | BIT_24),
			(path_scaler->frm_deci & 3) << 23);
	ISP_REG_MWR(idx, addr + ISP_SCALER_CFG, BIT_6 | BIT_7,
			scalerInfo->odata_mode << 6);

	/*set X/Y deci */
	ISP_REG_MWR(idx, addr + ISP_SCALER_CFG, BIT_2,
		deciInfo->deci_x_eb << 2);
	ISP_REG_MWR(idx, addr + ISP_SCALER_CFG, (BIT_0 | BIT_1),
		deciInfo->deci_x);
	ISP_REG_MWR(idx, addr + ISP_SCALER_CFG, BIT_5,
		deciInfo->deci_y_eb << 5);
	ISP_REG_MWR(idx, addr + ISP_SCALER_CFG, (BIT_3 | BIT_4),
		deciInfo->deci_y << 3);

	/*src size*/
	ISP_REG_WR(idx, addr + ISP_SCALER_SRC_SIZE,
				((path_scaler->src.h & 0x3FFF) << 16) |
					(path_scaler->src.w & 0x3FFF));
	/* trim0 */
	ISP_REG_WR(idx, addr + ISP_SCALER_TRIM0_START,
				((path_scaler->in_trim.start_y & 0x3FFF) << 16) |
					(path_scaler->in_trim.start_x & 0x3FFF));
	ISP_REG_WR(idx, addr + ISP_SCALER_TRIM0_SIZE,
				((path_scaler->in_trim.size_y & 0x3FFF) << 16) |
					(path_scaler->in_trim.size_x & 0x3FFF));
	/* trim1 */
	ISP_REG_WR(idx, addr + ISP_SCALER_TRIM1_START,
				((path_scaler->out_trim.start_y & 0x3FFF) << 16) |
					(path_scaler->out_trim.start_x & 0x3FFF));
	ISP_REG_WR(idx, addr + ISP_SCALER_TRIM1_SIZE,
				((path_scaler->out_trim.size_y & 0x3FFF) << 16) |
					(path_scaler->out_trim.size_x & 0x3FFF));
	/* des size */
	ISP_REG_WR(idx, addr + ISP_SCALER_DES_SIZE,
				((path_scaler->dst.h & 0x3FFF) << 16) |
					(path_scaler->dst.w & 0x3FFF));
	pr_debug("sw %d, path src: %d %d; in_trim:%d %d %d %d, out_trim: %d %d %d %d, dst: %d %d \n",
		idx, path_scaler->src.w, path_scaler->src.h, path_scaler->in_trim.start_x,
		path_scaler->in_trim.start_y, path_scaler->in_trim.size_x, path_scaler->in_trim.size_y,
		path_scaler->out_trim.start_x, path_scaler->out_trim.start_y, path_scaler->out_trim.size_x,
		path_scaler->out_trim.size_y, path_scaler->dst.w, path_scaler->dst.h);

	ISP_REG_MWR(idx, addr + ISP_SCALER_CFG, BIT_20,
			scalerInfo->scaler_bypass << 20);
	ISP_REG_MWR(idx, addr + ISP_SCALER_CFG, 0xF0000,
			scalerInfo->scaler_y_ver_tap << 16);
	ISP_REG_MWR(idx, addr + ISP_SCALER_CFG, 0xF800,
			scalerInfo->scaler_uv_ver_tap << 11);

	reg_val = ((scalerInfo->scaler_ip_int & 0xF) << 16) |
			(scalerInfo->scaler_ip_rmd & 0x3FFF);
	ISP_REG_WR(idx, addr + ISP_SCALER_IP, reg_val);
	reg_val = ((scalerInfo->scaler_cip_int & 0xF) << 16) |
			(scalerInfo->scaler_cip_rmd & 0x3FFF);
	ISP_REG_WR(idx, addr + ISP_SCALER_CIP, reg_val);
	reg_val = ((scalerInfo->scaler_factor_in & 0x3FFF) << 16) |
			(scalerInfo->scaler_factor_out & 0x3FFF);
	ISP_REG_WR(idx, addr + ISP_SCALER_FACTOR, reg_val);

	reg_val = ((scalerInfo->scaler_ver_ip_int & 0xF) << 16) |
				(scalerInfo->scaler_ver_ip_rmd & 0x3FFF);
	ISP_REG_WR(idx, addr + ISP_SCALER_VER_IP, reg_val);
	reg_val = ((scalerInfo->scaler_ver_cip_int & 0xF) << 16) |
				(scalerInfo->scaler_ver_cip_rmd & 0x3FFF);
	ISP_REG_WR(idx, addr + ISP_SCALER_VER_CIP, reg_val);
	reg_val = ((scalerInfo->scaler_ver_factor_in & 0x3FFF) << 16) |
				(scalerInfo->scaler_ver_factor_out & 0x3FFF);
	ISP_REG_WR(idx, addr + ISP_SCALER_VER_FACTOR, reg_val);

	pr_debug("set_scale_info in %d %d out %d %d\n",
		scalerInfo->scaler_factor_in,
		scalerInfo->scaler_ver_factor_in,
		scalerInfo->scaler_factor_out,
		scalerInfo->scaler_ver_factor_out);

	if (!scalerInfo->scaler_bypass)
		isphw_path_scaler_coeff_set(idx,
			addr, scalerInfo->coeff_buf, path_scaler->spath_id);

	if (path_scaler->spath_id == ISP_SPATH_VID)
		isphw_path_shrink_info_set(idx, addr, &path_scaler->regular_info);

	return 0;
}

static int isphw_path_thumbscaler(void *handle, void *arg)
{
	uint32_t val, idx;
	struct isp_hw_thumbscaler_info *scalerInfo = NULL;

	scalerInfo =(struct isp_hw_thumbscaler_info *)arg;
	idx = scalerInfo->idx;

	ISP_REG_MWR(idx, ISP_COMMON_SCL_PATH_SEL, BIT_5 | BIT_4, (0 << 4));
	ISP_REG_MWR(idx, ISP_THMB_CFG, BIT_0, scalerInfo->scaler_bypass & 0x1);

	val = ((scalerInfo->frame_deci & 0x3) << 2) |
		((scalerInfo->odata_mode & 0x3) << 4) |
		((scalerInfo->y_deci.deci_x & 0x3) << 16) |
		((scalerInfo->y_deci.deci_x_eb & 0x1) << 19) |
		((scalerInfo->y_deci.deci_y & 0x3) << 20) |
		((scalerInfo->y_deci.deci_y_eb & 0x1) << 23) |
		((scalerInfo->uv_deci.deci_x & 0x3) << 24) |
		((scalerInfo->uv_deci.deci_x_eb & 0x1) << 27) |
		((scalerInfo->uv_deci.deci_y & 0x3) << 28) |
		((scalerInfo->uv_deci.deci_y_eb & 0x1) << 31);
	ISP_REG_MWR(idx, ISP_THMB_CFG, 0xBBBB003C, val);

	val = ((scalerInfo->y_factor_in.w & 0x1FFF) << 16) |
		(scalerInfo->y_factor_out.w & 0x3FF);
	ISP_REG_WR(idx, ISP_THMB_Y_FACTOR_HOR, val);

	val = ((scalerInfo->y_factor_in.h & 0x1FFF) << 16) |
		(scalerInfo->y_factor_out.h & 0x3FF);
	ISP_REG_WR(idx, ISP_THMB_Y_FACTOR_VER, val);

	val = ((scalerInfo->uv_factor_in.w & 0x1FFF) << 16) |
		(scalerInfo->uv_factor_out.w & 0x3FF);
	ISP_REG_WR(idx, ISP_THMB_UV_FACTOR_HOR, val);

	val = ((scalerInfo->uv_factor_in.h & 0x1FFF) << 16) |
		(scalerInfo->uv_factor_out.h & 0x3FF);
	ISP_REG_WR(idx, ISP_THMB_UV_FACTOR_VER, val);

	val = ((scalerInfo->src0.h & 0x1FFF) << 16) |
		(scalerInfo->src0.w & 0x1FFF);
	ISP_REG_WR(idx, ISP_THMB_BEFORE_TRIM_SIZE, val);

	val = ((scalerInfo->y_src_after_deci.h & 0x1FFF) << 16) |
		(scalerInfo->y_src_after_deci.w & 0x1FFF);
	ISP_REG_WR(idx, ISP_THMB_Y_SLICE_SRC_SIZE, val);

	val = ((scalerInfo->y_dst_after_scaler.h & 0x3FF) << 16) |
		(scalerInfo->y_dst_after_scaler.w & 0x3FF);
	ISP_REG_WR(idx, ISP_THMB_Y_DES_SIZE, val);

	val = ((scalerInfo->y_trim.start_y & 0x1FFF) << 16) |
		(scalerInfo->y_trim.start_x & 0x1FFF);
	ISP_REG_WR(idx, ISP_THMB_Y_TRIM0_START, val);

	val = ((scalerInfo->y_trim.size_y & 0x1FFF) << 16) |
		(scalerInfo->y_trim.size_x & 0x1FFF);
	ISP_REG_WR(idx, ISP_THMB_Y_TRIM0_SIZE, val);

	val = ((scalerInfo->y_init_phase.h & 0x3FF) << 16) |
		(scalerInfo->y_init_phase.w & 0x3FF);
	ISP_REG_WR(idx, ISP_THMB_Y_INIT_PHASE, val);

	val = ((scalerInfo->uv_src_after_deci.h & 0x1FFF) << 16) |
		(scalerInfo->uv_src_after_deci.w & 0x1FFF);
	ISP_REG_WR(idx, ISP_THMB_UV_SLICE_SRC_SIZE, val);

	val = ((scalerInfo->uv_dst_after_scaler.h & 0x3FF) << 16) |
		(scalerInfo->uv_dst_after_scaler.w & 0x3FF);
	ISP_REG_WR(idx, ISP_THMB_UV_DES_SIZE, val);

	val = ((scalerInfo->uv_trim.start_y & 0x1FFF) << 16) |
		(scalerInfo->uv_trim.start_x & 0x1FFF);
	ISP_REG_WR(idx, ISP_THMB_UV_TRIM0_START, val);

	val = ((scalerInfo->uv_trim.size_y & 0x1FFF) << 16) |
		(scalerInfo->uv_trim.size_x & 0x1FFF);
	ISP_REG_WR(idx, ISP_THMB_UV_TRIM0_SIZE, val);

	val = ((scalerInfo->uv_init_phase.h & 0x3FF) << 16) |
		(scalerInfo->uv_init_phase.w & 0x3FF);
	ISP_REG_WR(idx, ISP_THMB_UV_INIT_PHASE, val);

	/* bypass regular. */
	ISP_REG_WR(idx, ISP_THMB_EFFECT_CFG, 0);

	return 0;
}

static int isphw_slice_scaler(void *handle, void *arg)
{
	uint32_t addr = 0, cmd = 0;
	uint32_t base = 0;
	struct isp_hw_slice_scaler *update = NULL;

	update = (struct isp_hw_slice_scaler *)arg;
	base = (uint32_t)scaler_base[update->spath_id];

	if (!update->path_en) {
		addr = ISP_SCALER_CFG + base;
		cmd = (0 << 31) | (1 << 8) | (1 << 9);
		ISP_REG_WR(update->ctx_id, addr, cmd);
		return 0;
	}

	/* bit31 enable path */
	addr = ISP_SCALER_CFG + base;
	cmd = ISP_REG_RD(update->ctx_id, base + ISP_SCALER_CFG);
	cmd |= (1 << 31);
	ISP_REG_WR(update->ctx_id, addr, cmd);

	addr = ISP_SCALER_SRC_SIZE + base;
	cmd = (update->slc_scaler->src_size_x & 0x3FFF) |
			((update->slc_scaler->src_size_y & 0x3FFF) << 16);
	ISP_REG_WR(update->ctx_id, addr, cmd);

	addr = ISP_SCALER_DES_SIZE + base;
	cmd = (update->slc_scaler->dst_size_x & 0x3FFF) |
			((update->slc_scaler->dst_size_y & 0x3FFF) << 16);
	ISP_REG_WR(update->ctx_id, addr, cmd);

	addr = ISP_SCALER_TRIM0_START + base;
	cmd = (update->slc_scaler->trim0_start_x & 0x1FFF) |
			((update->slc_scaler->trim0_start_y & 0x1FFF) << 16);
	ISP_REG_WR(update->ctx_id, addr, cmd);

	addr = ISP_SCALER_TRIM0_SIZE + base;
	cmd = (update->slc_scaler->trim0_size_x & 0x1FFF) |
			((update->slc_scaler->trim0_size_y & 0x1FFF) << 16);
	ISP_REG_WR(update->ctx_id, addr, cmd);

	addr = ISP_SCALER_IP + base;
	cmd = (update->slc_scaler->scaler_ip_rmd & 0x1FFF) |
			((update->slc_scaler->scaler_ip_int & 0xF) << 16);
	ISP_REG_WR(update->ctx_id, addr, cmd);

	addr = ISP_SCALER_CIP + base;
	cmd = (update->slc_scaler->scaler_cip_rmd & 0x1FFF) |
			((update->slc_scaler->scaler_cip_int & 0xF) << 16);
	ISP_REG_WR(update->ctx_id, addr, cmd);

	addr = ISP_SCALER_TRIM1_START + base;
	cmd = (update->slc_scaler->trim1_start_x & 0x1FFF) |
			((update->slc_scaler->trim1_start_y & 0x1FFF) << 16);
	ISP_REG_WR(update->ctx_id, addr, cmd);

	addr = ISP_SCALER_TRIM1_SIZE + base;
	cmd = (update->slc_scaler->trim1_size_x & 0x1FFF) |
			((update->slc_scaler->trim1_size_y & 0x1FFF) << 16);
	ISP_REG_WR(update->ctx_id, addr, cmd);

	addr = ISP_SCALER_VER_IP + base;
	cmd = (update->slc_scaler->scaler_ip_rmd_ver & 0x1FFF) |
			((update->slc_scaler->scaler_ip_int_ver & 0xF) << 16);
	ISP_REG_WR(update->ctx_id, addr, cmd);

	addr = ISP_SCALER_VER_CIP + base;
	cmd = (update->slc_scaler->scaler_cip_rmd_ver & 0x1FFF) |
			((update->slc_scaler->scaler_cip_int_ver & 0xF) << 16);
	ISP_REG_WR(update->ctx_id, addr, cmd);

	return 0;
}

static int isphw_slice_store(void *handle, void *arg)
{
	uint32_t addr = 0, cmd = 0;
	uint32_t base = 0;
	struct isp_hw_slice_store *store = NULL;

	store = (struct isp_hw_slice_store *)arg;
	base = (uint32_t)store_base[store->spath_id];

	if (!store->path_en) {
		/* bit0 bypass store */
		addr = ISP_STORE_PARAM + base;
		cmd = 1;
		ISP_REG_WR(store->ctx_id, addr, cmd);
		return 0;
	}
	addr = ISP_STORE_PARAM + base;
	cmd = ISP_REG_RD(store->ctx_id, base + ISP_STORE_PARAM) & ~1;
	ISP_REG_WR(store->ctx_id, addr, cmd);

	addr = ISP_STORE_SLICE_SIZE + base;
	cmd = ((store->slc_store->size.h & 0xFFFF) << 16) |
			(store->slc_store->size.w & 0xFFFF);
	ISP_REG_WR(store->ctx_id, addr, cmd);

	addr = ISP_STORE_BORDER + base;
	cmd = (store->slc_store->border.up_border & 0xFF) |
			((store->slc_store->border.down_border & 0xFF) << 8) |
			((store->slc_store->border.left_border & 0xFF) << 16) |
			((store->slc_store->border.right_border & 0xFF) << 24);
	ISP_REG_WR(store->ctx_id, addr, cmd);

	addr = ISP_STORE_SLICE_Y_ADDR + base;
	cmd = store->slc_store->addr.addr_ch0;
	ISP_REG_WR(store->ctx_id, addr, cmd);

	addr = ISP_STORE_SLICE_U_ADDR + base;
	cmd = store->slc_store->addr.addr_ch1;
	ISP_REG_WR(store->ctx_id, addr, cmd);

	addr = ISP_STORE_SLICE_V_ADDR + base;
	cmd = store->slc_store->addr.addr_ch2;
	ISP_REG_WR(store->ctx_id, addr, cmd);

	return 0;
}

static struct isp_cfg_entry isp_cfg_func_tab[ISP_BLOCK_TOTAL - ISP_BLOCK_BASE] = {
[ISP_BLOCK_BCHS - ISP_BLOCK_BASE]	= {ISP_BLOCK_BCHS, isp_k_cfg_bchs},
[ISP_BLOCK_YGAMMA - ISP_BLOCK_BASE]	= {ISP_BLOCK_YGAMMA, isp_k_cfg_ygamma},
[ISP_BLOCK_GAMMA - ISP_BLOCK_BASE]	= {ISP_BLOCK_GAMMA, isp_k_cfg_gamma},
[ISP_BLOCK_NLM - ISP_BLOCK_BASE]	= {ISP_BLOCK_NLM, isp_k_cfg_nlm},
[ISP_BLOCK_YNR - ISP_BLOCK_BASE]		= {ISP_BLOCK_YNR, isp_k_cfg_ynr},
[ISP_BLOCK_YUV_LTM - ISP_BLOCK_BASE] = {ISP_BLOCK_YUV_LTM, isp_k_cfg_yuv_ltm},
[ISP_BLOCK_CCE - ISP_BLOCK_BASE]	= {ISP_BLOCK_CCE, isp_k_cfg_cce},
[ISP_BLOCK_UVD - ISP_BLOCK_BASE]	= {ISP_BLOCK_UVD, isp_k_cfg_uvd},
[ISP_BLOCK_CFA - ISP_BLOCK_BASE]	= {ISP_BLOCK_CFA, isp_k_cfg_cfa},
[ISP_BLOCK_CMC - ISP_BLOCK_BASE]	= {ISP_BLOCK_CMC, isp_k_cfg_cmc10},
[ISP_BLOCK_CDN - ISP_BLOCK_BASE]	= {ISP_BLOCK_CDN, isp_k_cfg_cdn},
[ISP_BLOCK_HSV - ISP_BLOCK_BASE]	= {ISP_BLOCK_HSV, isp_k_cfg_hsv},
[ISP_BLOCK_GRGB - ISP_BLOCK_BASE]	= {ISP_BLOCK_GRGB, isp_k_cfg_grgb},
[ISP_BLOCK_EDGE - ISP_BLOCK_BASE]	= {ISP_BLOCK_EDGE, isp_k_cfg_edge},
[ISP_BLOCK_HIST2 - ISP_BLOCK_BASE]	= {ISP_BLOCK_HIST2, isp_k_cfg_hist2},
[ISP_BLOCK_IIRCNR - ISP_BLOCK_BASE]	= {ISP_BLOCK_IIRCNR, isp_k_cfg_iircnr},
[ISP_BLOCK_PRE_CDN - ISP_BLOCK_BASE]	= {ISP_BLOCK_PRE_CDN, isp_k_cfg_pre_cdn},
[ISP_BLOCK_POST_CDN - ISP_BLOCK_BASE]	= {ISP_BLOCK_POST_CDN, isp_k_cfg_post_cdn},
[ISP_BLOCK_PSTRZ - ISP_BLOCK_BASE]	= {ISP_BLOCK_PSTRZ, isp_k_cfg_pstrz},
[ISP_BLOCK_YRANDOM - ISP_BLOCK_BASE]	= {ISP_BLOCK_YRANDOM, isp_k_cfg_yrandom},
};

static int isphw_block_func_get(void *handle, void *arg)
{
	void *block_func = NULL;
	struct isp_hw_block_func *func_arg = NULL;

	func_arg = (struct isp_hw_block_func *)arg;

	if (func_arg->index < (ISP_BLOCK_TOTAL - ISP_BLOCK_BASE)) {
		block_func = (struct isp_cfg_entry*)&isp_cfg_func_tab[func_arg->index];
		func_arg->isp_entry= block_func;
	}

	if (block_func == NULL)
		pr_err("fail to get valid block func %d\n", ISP_BLOCK_TYPE);

	return 0;
}

static struct isp_cfg_pre_param isp_hw_cfg_param_func_tab[ISP_BLOCK_TOTAL - ISP_BLOCK_BASE] = {
	[ISP_BLOCK_BCHS - ISP_BLOCK_BASE]     = {ISP_BLOCK_BCHS,     isp_k_cpy_bchs},
	[ISP_BLOCK_CCE - ISP_BLOCK_BASE]      = {ISP_BLOCK_CCE,      isp_k_cpy_cce},
	[ISP_BLOCK_CDN - ISP_BLOCK_BASE]      = {ISP_BLOCK_CDN,      isp_k_cpy_cdn},
	[ISP_BLOCK_CFA - ISP_BLOCK_BASE]      = {ISP_BLOCK_CFA,      isp_k_cpy_cfa},
	[ISP_BLOCK_CMC - ISP_BLOCK_BASE]      = {ISP_BLOCK_CMC,      isp_k_cpy_cmc10},
	[ISP_BLOCK_EDGE - ISP_BLOCK_BASE]     = {ISP_BLOCK_EDGE,     isp_k_cpy_edge},
	[ISP_BLOCK_GAMMA - ISP_BLOCK_BASE]    = {ISP_BLOCK_GAMMA,    isp_k_cpy_gamma},
	[ISP_BLOCK_HSV - ISP_BLOCK_BASE]      = {ISP_BLOCK_HSV,      isp_k_cpy_hsv},
	[ISP_BLOCK_IIRCNR - ISP_BLOCK_BASE]   = {ISP_BLOCK_IIRCNR,   isp_k_cpy_iircnr},
	[ISP_BLOCK_YUV_LTM - ISP_BLOCK_BASE]  = {ISP_BLOCK_YUV_LTM,  isp_k_cpy_yuv_ltm},
	[ISP_BLOCK_NLM - ISP_BLOCK_BASE]      = {ISP_BLOCK_NLM,      isp_k_cpy_nlm},
	[ISP_BLOCK_POST_CDN - ISP_BLOCK_BASE] = {ISP_BLOCK_POST_CDN, isp_k_cpy_post_cdn},
	[ISP_BLOCK_PRE_CDN - ISP_BLOCK_BASE]  = {ISP_BLOCK_PRE_CDN,  isp_k_cpy_pre_cdn},
	[ISP_BLOCK_UVD - ISP_BLOCK_BASE]      = {ISP_BLOCK_UVD,      isp_k_cpy_uvd},
	[ISP_BLOCK_YGAMMA - ISP_BLOCK_BASE]   = {ISP_BLOCK_YGAMMA,   isp_k_cpy_ygamma},
	[ISP_BLOCK_YNR - ISP_BLOCK_BASE]      = {ISP_BLOCK_YNR,      isp_k_cpy_ynr},
	[ISP_BLOCK_YRANDOM- ISP_BLOCK_BASE]   = {ISP_BLOCK_YRANDOM,  isp_k_cpy_yrandom},
	[ISP_BLOCK_3DNR - ISP_BLOCK_BASE]     = {ISP_BLOCK_3DNR,     isp_k_cpy_3dnr},
};

static int isphw_param_get(void *handle, void *arg)
{
	void *block_func = NULL;
	struct isp_cfg_block_param *func_arg = NULL;

	func_arg = (struct isp_cfg_block_param *)arg;
	if (func_arg->index < (ISP_BLOCK_TOTAL - ISP_BLOCK_BASE)) {
		block_func = (struct isp_cfg_pre_param *)&isp_hw_cfg_param_func_tab[func_arg->index];
		func_arg->isp_param = block_func;
	}

	return 0;
}

static isp_k_blk_func isp_hw_k_blk_func_tab[ISP_K_BLK_MAX] = {
	[ISP_K_BLK_LTM] = isp_ltm_config_param,
	[ISP_K_BLK_NLM_UPDATE] = isp_k_update_nlm,
	[ISP_K_BLK_YNR_UPDATE] = isp_k_update_ynr,
};

static int isphw_k_blk_func_get(void *handle, void *arg)
{
	struct isp_hw_k_blk_func *func_arg = NULL;

	func_arg = (struct isp_hw_k_blk_func *)arg;

	if (func_arg->index < ISP_K_BLK_MAX)
		func_arg->k_blk_func = isp_hw_k_blk_func_tab[func_arg->index];

	return 0;
}

static int isphw_fetch_set(void *handle, void *arg)
{
	uint32_t idx = 0;
	struct isp_hw_fetch_info *fetch = NULL;

	if (!arg) {
		pr_err("fail to get valid arg\n");
		return -EFAULT;
	}

	fetch = (struct isp_hw_fetch_info *)arg;
	idx = fetch->ctx_id;
	pr_debug("enter: fmt:%d, w:%d, h:%d\n", fetch->fetch_fmt,
			fetch->in_trim.size_x, fetch->in_trim.size_y);

	ISP_REG_MWR(idx, ISP_COMMON_SPACE_SEL,
			BIT_1 | BIT_0, fetch->dispatch_color);
	/* 11b: close store_dbg module */
	ISP_REG_MWR(idx, ISP_COMMON_SPACE_SEL,
			BIT_3 | BIT_2, 3 << 2);
	ISP_REG_MWR(idx, ISP_COMMON_SPACE_SEL, BIT_4, 0 << 4);

	ISP_REG_MWR(idx, ISP_COMMON_SCL_PATH_SEL,
			BIT_10, fetch->fetch_path_sel  << 10);
	ISP_REG_MWR(idx, ISP_COMMON_SCL_PATH_SEL,
			BIT_8, 0 << 8); /* 3dnr off */
	ISP_REG_MWR(idx, ISP_COMMON_SCL_PATH_SEL,
			BIT_5 | BIT_4, 3 << 4);  /* thumb path off */
	ISP_REG_MWR(idx, ISP_COMMON_SCL_PATH_SEL,
			BIT_3 | BIT_2, 3 << 2); /* vid path off */
	ISP_REG_MWR(idx, ISP_COMMON_SCL_PATH_SEL,
			BIT_1 | BIT_0, 3 << 0);  /* pre/cap path off */
	ISP_REG_MWR(idx, ISP_FBD_RAW_SEL, BIT(0), 0x1);/* fbd off */

	ISP_REG_MWR(idx, ISP_FETCH_PARAM, BIT_0, 0);
	ISP_REG_MWR(idx, ISP_FETCH_PARAM,
			(0xF << 4), fetch->fetch_fmt << 4);
	ISP_REG_WR(idx, ISP_FETCH_MEM_SLICE_SIZE,
			fetch->in_trim.size_x | (fetch->in_trim.size_y << 16));

	pr_debug("camca, isp sec mode=%d , is_loose=%d, pitch_ch0=0x%x, 0x%x, 0x%x\n",
		fetch->sec_mode,
		fetch->pack_bits,
		fetch->pitch.pitch_ch0,
		fetch->pitch.pitch_ch1,
		fetch->pitch.pitch_ch2);

	if (fetch->sec_mode == SEC_SPACE_PRIORITY) {
		cam_trusty_isp_pitch_set(fetch->pitch.pitch_ch0,
			fetch->pitch.pitch_ch1,
			fetch->pitch.pitch_ch2);
	} else {
		ISP_REG_WR(idx, ISP_FETCH_SLICE_Y_PITCH, fetch->pitch.pitch_ch0);
		ISP_REG_WR(idx, ISP_FETCH_SLICE_U_PITCH, fetch->pitch.pitch_ch1);
		ISP_REG_WR(idx, ISP_FETCH_SLICE_V_PITCH, fetch->pitch.pitch_ch2);
	}

	ISP_REG_WR(idx, ISP_FETCH_LINE_DLY_CTRL, 0x8);
	ISP_REG_WR(idx, ISP_FETCH_MIPI_INFO,
		fetch->mipi_word_num | (fetch->mipi_byte_rel_pos << 16));

	ISP_REG_WR(idx, ISP_DISPATCH_DLY,  0x253C);
	ISP_REG_WR(idx, ISP_DISPATCH_LINE_DLY1,  0x8280001C);
	ISP_REG_WR(idx, ISP_DISPATCH_PIPE_BUF_CTRL_CH0,  0x64043C);
	ISP_REG_WR(idx, ISP_DISPATCH_CH0_SIZE,
		fetch->in_trim.size_x | (fetch->in_trim.size_y << 16));
	ISP_REG_WR(idx, ISP_DISPATCH_CH0_BAYER, fetch->bayer_pattern);

	ISP_REG_WR(idx, ISP_YDELAY_STEP, 0x144);
	ISP_REG_WR(idx, ISP_SCALER_PRE_CAP_BASE
		+ ISP_SCALER_HBLANK, 0x4040);
	ISP_REG_WR(idx, ISP_SCALER_PRE_CAP_BASE + ISP_SCALER_RES, 0xFF);
	ISP_REG_WR(idx, ISP_SCALER_PRE_CAP_BASE + ISP_SCALER_DEBUG, 1);

	pr_debug("end\n");
	return 0;
}

static int isphw_fetch_fbd_set(void *handle, void *arg)
{
	struct isp_fbd_raw_info *fbd_raw = NULL;
	uint32_t idx = 0;

	if (!arg) {
		pr_err("fail to get valid arg\n");
		return -EFAULT;
	}

	fbd_raw = (struct isp_fbd_raw_info *)arg;
	idx = fbd_raw->ctx_id;

	ISP_REG_MWR(idx, ISP_FBD_RAW_SEL, BIT(0), fbd_raw->fetch_fbd_bypass);
	ISP_REG_MWR(idx, ISP_FBD_RAW_SEL, 0x00003f00, fbd_raw->pixel_start_in_hor << 8);
	ISP_REG_MWR(idx, ISP_FBD_RAW_SEL, 0x00000030, fbd_raw->pixel_start_in_ver << 4);
	ISP_REG_WR(idx, ISP_FBD_RAW_SLICE_SIZE, fbd_raw->width | (fbd_raw->height << 16));
	ISP_REG_WR(idx, ISP_FBD_RAW_PARAM0,
		fbd_raw->tiles_num_in_hor | (fbd_raw->tiles_num_in_ver << 16));
	ISP_REG_WR(idx, ISP_FBD_RAW_PARAM1, 0xff << 16 |
		(fbd_raw->tiles_start_odd & 0x1) << 8 | ((fbd_raw->tiles_num_pitch) & 0xff));
	ISP_REG_WR(idx, ISP_FBD_RAW_LOW_PARAM1, fbd_raw->low_bit_pitch);
	ISP_REG_MWR(idx, ISP_FETCH_PARAM, BIT_0, 0x1);
	pr_debug("enable fbd: %d\n", !fbd_raw->fetch_fbd_bypass);

	return 0;
}

/* workaround: temp disable FMCU 1 for not working */
static int isphw_fmcu_available(void *handle, void *arg)
{
	uint32_t fmcu_valid = 0;
	struct isp_hw_fmcu_sel *fmcu_sel = NULL;
	uint32_t reg_bits[ISP_CONTEXT_HW_NUM] = {
			ISP_CONTEXT_HW_P0, ISP_CONTEXT_HW_P1,
			ISP_CONTEXT_HW_C0, ISP_CONTEXT_HW_C1};
	uint32_t reg_offset[ISP_FMCU_NUM] = {
			ISP_COMMON_FMCU0_PATH_SEL,
			ISP_COMMON_FMCU1_PATH_SEL};

	if (!arg) {
		pr_err("fail to get valid arg\n");
		return -EINVAL;
	}

	fmcu_sel = (struct isp_hw_fmcu_sel *)arg;
	fmcu_valid = (fmcu_sel->fmcu_id > 0) ? 0 : 1;

	if (fmcu_valid)
		ISP_HREG_MWR(reg_offset[fmcu_sel->fmcu_id], BIT_1 | BIT_0,
			reg_bits[fmcu_sel->hw_idx]);

	return fmcu_valid;
}

static int isphw_ltm_slice_set(void *handle, void *arg)
{
	uint32_t addr = 0, cmd = 0, base = 0;
	struct isp_hw_ltm_slice *ltm_slice = NULL;
	struct isp_fmcu_ctx_desc *fmcu = NULL;
	struct slice_ltm_map_info *map = NULL;

	ltm_slice = (struct isp_hw_ltm_slice *)arg;
	fmcu = ltm_slice->fmcu_handle;
	map = ltm_slice->map;

	if (map->bypass) {
		return 0;
	}

	switch (ltm_slice->ltm_id) {
	case LTM_YUV:
		base = ISP_LTM_MAP_YUV_BASE;
		break;
	default:
		pr_err("fail to get cmd id:%d, not supported.\n", ltm_slice->ltm_id);
		return -1;
	}

	addr = ISP_GET_REG(base + ISP_LTM_MAP_PARAM1);
	cmd = ((map->tile_num_y  & 0x7)   << 28) |
	      ((map->tile_num_x  & 0x7)   << 24) |
	      ((map->tile_height & 0x3FF) << 12) |
	       (map->tile_width  & 0x3FF);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(base + ISP_LTM_MAP_PARAM3);
	cmd = ((map->tile_right_flag & 0x1)   << 23) |
	      ((map->tile_start_y    & 0x7FF) << 12) |
	      ((map->tile_left_flag  & 0x1)   << 11) |
	       (map->tile_start_x    & 0x7FF);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(base + ISP_LTM_MAP_PARAM4);
	cmd = map->mem_addr;
	FMCU_PUSH(fmcu, addr, cmd);

	return 0;
}

static int isphw_subblock_cfg(void *handle, void *arg)
{
	uint32_t idx = 0;
	struct isp_hw_fetch_info *fetch = NULL;

	fetch = (struct isp_hw_fetch_info *)arg;
	idx = fetch->ctx_id;
	pr_debug("superzoom enter: fmt:%d, in_trim %d %d, src %d %d\n",
		fetch->fetch_fmt, fetch->in_trim.size_x, fetch->in_trim.size_y,
		fetch->src.w, fetch->src.h);

	ISP_REG_MWR(idx, ISP_COMMON_SPACE_SEL, BIT_1 | BIT_0, 2);
	ISP_REG_MWR(idx, ISP_COMMON_SPACE_SEL, 0x0F, 0x0A);

	ISP_REG_MWR(idx, ISP_FETCH_PARAM,
			(0xF << 4), fetch->fetch_fmt << 4);
	ISP_REG_WR(idx, ISP_FETCH_SLICE_Y_PITCH,fetch->pitch.pitch_ch0);
	ISP_REG_WR(idx, ISP_FETCH_SLICE_U_PITCH, fetch->pitch.pitch_ch1);
	ISP_REG_WR(idx, ISP_FETCH_SLICE_V_PITCH, fetch->pitch.pitch_ch2);
	ISP_REG_WR(idx, ISP_FETCH_SLICE_Y_ADDR, fetch->addr.addr_ch0);
	ISP_REG_WR(idx, ISP_FETCH_SLICE_U_ADDR, fetch->addr.addr_ch1);
	ISP_REG_WR(idx, ISP_FETCH_SLICE_V_ADDR, fetch->addr.addr_ch2);

	ISP_REG_WR(idx, ISP_FETCH_MEM_SLICE_SIZE,
				fetch->src.w | (fetch->src.h << 16));
	ISP_REG_WR(idx, ISP_FETCH_LINE_DLY_CTRL, 0x8);

	ISP_REG_WR(idx, ISP_DISPATCH_DLY,  0x253C);
	ISP_REG_WR(idx, ISP_DISPATCH_LINE_DLY1,  0x8280001C);
	ISP_REG_WR(idx, ISP_DISPATCH_PIPE_BUF_CTRL_CH0,  0x64043C);
	ISP_REG_WR(idx, ISP_DISPATCH_CH0_SIZE,
				fetch->in_trim.size_x | (fetch->in_trim.size_y << 16));
	ISP_REG_WR(idx, ISP_DISPATCH_CH0_BAYER, fetch->bayer_pattern);
	pr_debug("pitch ch0 %d, ch1 %d, ch2 %d\n",
		fetch->pitch.pitch_ch0, fetch->pitch.pitch_ch1, fetch->pitch.pitch_ch2);

	return 0;
}

static int isphw_slw_fmcu_cmds(void *handle, void *arg)
{
	int i;
	unsigned long base, sbase;
	uint32_t ctx_idx;
	uint32_t reg_off, addr = 0, cmd = 0;
	struct isp_fmcu_ctx_desc *fmcu;
	struct isp_path_desc *path;
	struct img_addr *fetch_addr, *store_addr;
	struct cam_hw_info *hw = NULL;
	struct isp_hw_afbc_fmcu_addr fmcu_addr;
	struct isp_hw_slw_fmcu_cmds *slw = NULL;
	struct isp_hw_fmcu_cfg cfg;

	uint32_t shadow_done_cmd[ISP_CONTEXT_HW_NUM] = {
		PRE0_SHADOW_DONE, CAP0_SHADOW_DONE,
		PRE1_SHADOW_DONE, CAP1_SHADOW_DONE,
	};
	uint32_t all_done_cmd[ISP_CONTEXT_HW_NUM] = {
		PRE0_ALL_DONE, CAP0_ALL_DONE,
		PRE1_ALL_DONE, CAP1_ALL_DONE,
	};

	slw = (struct isp_hw_slw_fmcu_cmds *)arg;

	if (!slw) {
		pr_err("fail to get valid input ptr, slw%p\n", slw);
		return -EFAULT;
	}

	fmcu = slw->fmcu_handle;
	base = (fmcu->fid == 0) ? ISP_FMCU0_BASE : ISP_FMCU1_BASE;
	fetch_addr = &slw->fetchaddr;
	ctx_idx = slw->ctx_id;
	hw = (struct cam_hw_info *)handle;
	cfg.fmcu = fmcu;
	cfg.ctx_id = ctx_idx;

	hw->isp_ioctl(hw, ISP_HW_CFG_FMCU_CFG, &cfg);

	addr = ISP_GET_REG(ISP_FETCH_SLICE_Y_ADDR);
	cmd = fetch_addr->addr_ch0;
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_FETCH_SLICE_U_ADDR);
	cmd = fetch_addr->addr_ch1;
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_FETCH_SLICE_V_ADDR);
	cmd = fetch_addr->addr_ch2;
	FMCU_PUSH(fmcu, addr, cmd);

	for (i = 0; i < ISP_SPATH_NUM; i++) {
		path = &slw->isp_path[i];

		if (atomic_read(&path->user_cnt) < 1)
			continue;

		store_addr = &slw->store[i].addr;
		sbase = store_base[i];

		addr = ISP_GET_REG(ISP_STORE_SLICE_Y_ADDR) + sbase;
		cmd = store_addr->addr_ch0;
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_STORE_SLICE_U_ADDR) + sbase;
		cmd = store_addr->addr_ch1;
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_STORE_SLICE_V_ADDR) + sbase;
		cmd = store_addr->addr_ch2;
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_STORE_SHADOW_CLR) + sbase;
		cmd = 1;
		FMCU_PUSH(fmcu, addr, cmd);

		if ((i < AFBC_PATH_NUM) && (slw->afbc_store[i].bypass == 0)){
			fmcu_addr.yheader = slw->afbc_store[i].yheader;
			fmcu_addr.yaddr = slw->afbc_store[i].yaddr;
			fmcu_addr.fmcu = fmcu;
			fmcu_addr.index = i;
			hw->isp_ioctl(hw, ISP_HW_CFG_AFBC_FMCU_ADDR_SET, &fmcu_addr);
		}
	}

	reg_off = ISP_CFG_CAP_FMCU_RDY;
	addr = ISP_GET_REG(reg_off);
	cmd = 1;
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(base + ISP_FMCU_CMD);
	cmd = shadow_done_cmd[ctx_idx];
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(base + ISP_FMCU_CMD);
	cmd = all_done_cmd[ctx_idx];
	FMCU_PUSH(fmcu, addr, cmd);

	return 0;
}

static int isphw_fmcu_cfg(void *handle, void *arg)
{
	uint32_t addr = 0, cmd = 0;
	struct isp_hw_fmcu_cfg *cfg = NULL;
	unsigned long base;
	unsigned long reg_addr[ISP_CONTEXT_HW_NUM] = {
		ISP_CFG_PRE0_START,
		ISP_CFG_CAP0_START,
		ISP_CFG_PRE1_START,
		ISP_CFG_CAP1_START,
	};

	cfg = (struct isp_hw_fmcu_cfg *)arg;
	addr = ISP_GET_REG(reg_addr[cfg->ctx_id]);
	cmd = 1;
	FMCU_PUSH(cfg->fmcu, addr, cmd);

	/*
	 * When setting CFG_TRIGGER_PULSE cmd, fmcu will wait
	 * until CFG module configs isp registers done.
	 */
	if (cfg->fmcu->fid == 0)
		base =  ISP_FMCU0_BASE;
	else
		base =  ISP_FMCU1_BASE;
	addr = ISP_GET_REG(base + ISP_FMCU_CMD);
	cmd = CFG_TRIGGER_PULSE;
	FMCU_PUSH(cfg->fmcu, addr, cmd);

	return 0;
}

static int isphw_slice_fetch(void *handle, void *arg)
{
	uint32_t addr = 0, cmd = 0;
	struct isp_hw_slice_fetch *fetch = NULL;

	fetch = (struct isp_hw_slice_fetch *)arg;
	addr = ISP_FETCH_MEM_SLICE_SIZE;
	cmd = ((fetch->fetch_info->size.h & 0xFFFF) << 16) |
			(fetch->fetch_info->size.w & 0xFFFF);
	ISP_REG_WR(fetch->ctx_id, addr, cmd);

	addr = ISP_FETCH_SLICE_Y_ADDR;
	cmd = fetch->fetch_info->addr.addr_ch0;
	ISP_REG_WR(fetch->ctx_id, addr, cmd);

	addr = ISP_FETCH_SLICE_U_ADDR;
	cmd = fetch->fetch_info->addr.addr_ch1;
	ISP_REG_WR(fetch->ctx_id, addr, cmd);

	addr = ISP_FETCH_SLICE_V_ADDR;
	cmd = fetch->fetch_info->addr.addr_ch2;
	ISP_REG_WR(fetch->ctx_id, addr, cmd);

	addr = ISP_FETCH_MIPI_INFO;
	cmd = fetch->fetch_info->mipi_word_num |
			(fetch->fetch_info->mipi_byte_rel_pos << 16);
	ISP_REG_WR(fetch->ctx_id, addr, cmd);

	/* dispatch size same as fetch size */
	addr = ISP_DISPATCH_CH0_SIZE;
	cmd = ((fetch->fetch_info->size.h & 0xFFFF) << 16) |
			(fetch->fetch_info->size.w & 0xFFFF);
	ISP_REG_WR(fetch->ctx_id, addr, cmd);

	return 0;
}

static int isphw_slice_nr_info(void *handle, void *arg)
{
	uint32_t addr = 0, cmd = 0;
	struct isp_hw_slice_nr_info *info = NULL;

	info = (struct isp_hw_slice_nr_info *)arg;

	/* NLM */
	addr = ISP_NLM_RADIAL_1D_DIST;
	cmd = ((info->cur_slc->slice_nlm.center_y_relative & 0x3FFF) << 16) |
		(info->cur_slc->slice_nlm.center_x_relative & 0x3FFF);
	ISP_REG_WR(info->ctx_id, addr, cmd);

	/* Post CDN */
	addr = ISP_POSTCDN_SLICE_CTRL;
	cmd = info->cur_slc->slice_postcdn.start_row_mod4;
	ISP_REG_WR(info->ctx_id, addr, cmd);

	/* YNR */
	addr = ISP_YNR_CFG31;
	cmd = ((info->cur_slc->slice_ynr.center_offset_y & 0xFFFF) << 16) |
		(info->cur_slc->slice_ynr.center_offset_x & 0xFFFF);
	ISP_REG_WR(info->ctx_id, addr, cmd);

	addr = ISP_YNR_CFG33;
	cmd = ((info->cur_slc->slice_ynr.slice_height & 0xFFFF) << 16) |
		(info->cur_slc->slice_ynr.slice_width & 0xFFFF);
	ISP_REG_WR(info->ctx_id, addr, cmd);

	return 0;
}

static int isphw_slices_fmcu_cmds(void *handle, void *arg)
{
	uint32_t reg_off, addr = 0, cmd = 0;
	unsigned long base = 0;
	struct isp_hw_slices_fmcu_cmds *parg = NULL;
	uint32_t shadow_done_cmd[ISP_CONTEXT_HW_NUM] = {
		PRE0_SHADOW_DONE, CAP0_SHADOW_DONE,
		PRE1_SHADOW_DONE, CAP1_SHADOW_DONE,
	};
	uint32_t all_done_cmd[ISP_CONTEXT_HW_NUM] = {
		PRE0_ALL_DONE, CAP0_ALL_DONE,
		PRE1_ALL_DONE, CAP1_ALL_DONE,
	};

	parg = (struct isp_hw_slices_fmcu_cmds *)arg;

	if (parg->fmcu->fid == 0)
		base = ISP_FMCU0_BASE;
	else
		base = ISP_FMCU1_BASE;

	if (parg->wmode == ISP_CFG_MODE) {
		reg_off = ISP_CFG_CAP_FMCU_RDY;
		addr = ISP_GET_REG(reg_off);
	} else
		addr = ISP_GET_REG(ISP_FETCH_START);
	cmd = 1;
	FMCU_PUSH(parg->fmcu, addr, cmd);

	addr = ISP_GET_REG(base + ISP_FMCU_CMD);
	cmd = shadow_done_cmd[parg->hw_ctx_id];
	FMCU_PUSH(parg->fmcu, addr, cmd);

	addr = ISP_GET_REG(base + ISP_FMCU_CMD);
	cmd = all_done_cmd[parg->hw_ctx_id];
	FMCU_PUSH(parg->fmcu, addr, cmd);

	return 0;
}

static int isphw_slice_nofilter(void *handle, void *arg)
{
	uint32_t addr = 0, cmd = 0;
	struct isp_hw_slice_nofilter *slicearg = NULL;
	struct slice_noisefilter_info *map = NULL;

	slicearg = (struct isp_hw_slice_nofilter *)arg;
	map = slicearg->noisefilter_info;
	pr_debug("seed0=%d,seed1=%d,seed2=%d,seed3=%d\n", map->seed0, map->seed1,
		map->seed2, map->seed3);
	addr = ISP_GET_REG(ISP_YUV_NF_SEED0);
	cmd = map->seed0;
	FMCU_PUSH(slicearg->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_YUV_NF_SEED1);
	cmd = map->seed1;
	FMCU_PUSH(slicearg->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_YUV_NF_SEED2);
	cmd = map->seed2;
	FMCU_PUSH(slicearg->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_YUV_NF_SEED3);
	cmd = map->seed3;
	FMCU_PUSH(slicearg->fmcu, addr, cmd);

	return 0;
}

static int isphw_slice_3dnr_crop(void *handle, void *arg)
{
	uint32_t addr = 0, cmd = 0;
	struct isp_hw_slice_3dnr_crop *croparg = NULL;

	croparg = (struct isp_hw_slice_3dnr_crop *)arg;

	if (croparg->crop->bypass)
		return 0;

	addr = ISP_GET_REG(ISP_3DNR_MEM_CTRL_PRE_PARAM0);
	cmd = croparg->crop->bypass & 0x1;
	FMCU_PUSH(croparg->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_3DNR_MEM_CTRL_PRE_PARAM1);
	cmd = ((croparg->crop->src.h & 0xFFFF) << 16) |
		(croparg->crop->src.w & 0xFFFF);
	FMCU_PUSH(croparg->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_3DNR_MEM_CTRL_PRE_PARAM2);
	cmd = ((croparg->crop->dst.h & 0xFFFF) << 16) |
		(croparg->crop->dst.w & 0xFFFF);
	FMCU_PUSH(croparg->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_3DNR_MEM_CTRL_PRE_PARAM3);
	cmd = ((croparg->crop->start_x & 0xFFFF) << 16) |
		(croparg->crop->start_y & 0xFFFF);
	FMCU_PUSH(croparg->fmcu, addr, cmd);

	return 0;
}

static int isphw_slice_3dnr_store(void *handle, void *arg)
{
	uint32_t addr = 0, cmd = 0;
	struct isp_hw_slice_3dnr_store *storearg = NULL;

	storearg = (struct isp_hw_slice_3dnr_store *)arg;

	if (storearg->store->bypass)
		return 0;

	addr = ISP_GET_REG(ISP_3DNR_STORE_SIZE);
	cmd = ((storearg->store->size.h & 0xFFFF) << 16) |
		(storearg->store->size.w & 0xFFFF);
	FMCU_PUSH(storearg->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_3DNR_STORE_LUMA_ADDR);
	cmd = storearg->store->addr.addr_ch0;
	FMCU_PUSH(storearg->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_3DNR_STORE_CHROMA_ADDR);
	cmd = storearg->store->addr.addr_ch1;
	FMCU_PUSH(storearg->fmcu, addr, cmd);

	return 0;
}

static int isphw_slice_3dnr_memctrl(void *handle, void *arg)
{
	uint32_t addr = 0, cmd = 0;
	struct isp_hw_slice_3dnr_memctrl *memarg = NULL;

	memarg = (struct isp_hw_slice_3dnr_memctrl *)arg;

	if (memarg->mem_ctrl->bypass)
		return 0;

	addr = ISP_GET_REG(ISP_3DNR_MEM_CTRL_PARAM1);
	cmd = ((memarg->mem_ctrl->start_col & 0x1FFF) << 16) |
		(memarg->mem_ctrl->start_row & 0x1FFF);
	FMCU_PUSH(memarg->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_3DNR_MEM_CTRL_PARAM3);
	cmd = ((memarg->mem_ctrl->src.h & 0x1FFF) << 16) |
		(memarg->mem_ctrl->src.w & 0x1FFF);
	FMCU_PUSH(memarg->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_3DNR_MEM_CTRL_PARAM4);
	cmd = ((memarg->mem_ctrl->ft_y.h & 0x1FFF) << 16) |
		(memarg->mem_ctrl->ft_y.w & 0x1FFF);
	FMCU_PUSH(memarg->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_3DNR_MEM_CTRL_PARAM5);
	cmd = ((memarg->mem_ctrl->ft_uv.h & 0x1FFF) << 16) |
		(memarg->mem_ctrl->ft_uv.w & 0x1FFF);
	FMCU_PUSH(memarg->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_3DNR_MEM_CTRL_FT_CUR_LUMA_ADDR);
	cmd = memarg->mem_ctrl->addr.addr_ch0;
	FMCU_PUSH(memarg->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_3DNR_MEM_CTRL_FT_CUR_CHROMA_ADDR);
	cmd = memarg->mem_ctrl->addr.addr_ch1;
	FMCU_PUSH(memarg->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_3DNR_MEM_CTRL_LINE_MODE);
	cmd = ((memarg->mem_ctrl->last_line_mode & 0x1) << 1) |
		(memarg->mem_ctrl->first_line_mode & 0x1);
	FMCU_PUSH(memarg->fmcu, addr, cmd);

	return 0;
}

static int isphw_slice_spath_store(void *handle, void *arg)
{
	uint32_t addr = 0, cmd = 0;
	unsigned long base = 0;
	struct isp_hw_slice_spath *spath = NULL;

	spath = (struct isp_hw_slice_spath *)arg;
	base = store_base[spath->spath_id];

	if (!spath->path_en) {
		/* bit0 bypass store */
		addr = ISP_GET_REG(ISP_STORE_PARAM) + base;
		cmd = 1;
		FMCU_PUSH(spath->fmcu, addr, cmd);
		return 0;
	}
	addr = ISP_GET_REG(ISP_STORE_PARAM) + base;
	cmd = ISP_REG_RD(spath->ctx_idx, base + ISP_STORE_PARAM) & ~1;
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_STORE_SLICE_SIZE) + base;
	cmd = ((spath->slc_store->size.h & 0xFFFF) << 16) |
			(spath->slc_store->size.w & 0xFFFF);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_STORE_BORDER) + base;
	cmd = (spath->slc_store->border.up_border & 0xFF) |
			((spath->slc_store->border.down_border & 0xFF) << 8) |
			((spath->slc_store->border.left_border & 0xFF) << 16) |
			((spath->slc_store->border.right_border & 0xFF) << 24);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_STORE_SLICE_Y_ADDR) + base;
	cmd = spath->slc_store->addr.addr_ch0;
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_STORE_SLICE_U_ADDR) + base;
	cmd = spath->slc_store->addr.addr_ch1;
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_STORE_SLICE_V_ADDR) + base;
	cmd = spath->slc_store->addr.addr_ch2;
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_STORE_SHADOW_CLR) + base;
	cmd = 1;
	FMCU_PUSH(spath->fmcu, addr, cmd);

	return 0;
}

static int isphw_slice_spath_scaler(void *handle, void *arg)
{
	uint32_t addr = 0, cmd = 0;
	unsigned long base = 0;
	struct isp_hw_slice_spath *spath = NULL;

	spath = (struct isp_hw_slice_spath *)arg;
	base = scaler_base[spath->spath_id];

	if (!spath->path_en) {
		addr = ISP_GET_REG(ISP_SCALER_CFG) + base;
		cmd = (0 << 31) | (1 << 8) | (1 << 9);
		FMCU_PUSH(spath->fmcu, addr, cmd);
		return 0;
	}

	/* bit31 enable path */
	addr = ISP_GET_REG(ISP_SCALER_CFG) + base;
	cmd = ISP_REG_RD(spath->ctx_idx, base + ISP_SCALER_CFG);
	cmd |= (1 << 31);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_SCALER_SRC_SIZE) + base;
	cmd = (spath->slc_scaler->src_size_x & 0x3FFF) |
			((spath->slc_scaler->src_size_y & 0x3FFF) << 16);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_SCALER_DES_SIZE) + base;
	cmd = (spath->slc_scaler->dst_size_x & 0x3FFF) |
			((spath->slc_scaler->dst_size_y & 0x3FFF) << 16);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_SCALER_TRIM0_START) + base;
	cmd = (spath->slc_scaler->trim0_start_x & 0x1FFF) |
			((spath->slc_scaler->trim0_start_y & 0x1FFF) << 16);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_SCALER_TRIM0_SIZE) + base;
	cmd = (spath->slc_scaler->trim0_size_x & 0x1FFF) |
			((spath->slc_scaler->trim0_size_y & 0x1FFF) << 16);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_SCALER_IP) + base;
	cmd = (spath->slc_scaler->scaler_ip_rmd & 0x1FFF) |
			((spath->slc_scaler->scaler_ip_int & 0xF) << 16);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_SCALER_CIP) + base;
	cmd = (spath->slc_scaler->scaler_cip_rmd & 0x1FFF) |
			((spath->slc_scaler->scaler_cip_int & 0xF) << 16);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_SCALER_TRIM1_START) + base;
	cmd = (spath->slc_scaler->trim1_start_x & 0x1FFF) |
			((spath->slc_scaler->trim1_start_y & 0x1FFF) << 16);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_SCALER_TRIM1_SIZE) + base;
	cmd = (spath->slc_scaler->trim1_size_x & 0x1FFF) |
			((spath->slc_scaler->trim1_size_y & 0x1FFF) << 16);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_SCALER_VER_IP) + base;
	cmd = (spath->slc_scaler->scaler_ip_rmd_ver & 0x1FFF) |
			((spath->slc_scaler->scaler_ip_int_ver & 0xF) << 16);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_SCALER_VER_CIP) + base;
	cmd = (spath->slc_scaler->scaler_cip_rmd_ver & 0x1FFF) |
			((spath->slc_scaler->scaler_cip_int_ver & 0xF) << 16);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	return 0;
}

static int isphw_slice_spath_thumbscaler(void *handle, void *arg)
{
	uint32_t addr = 0, cmd = 0;
	struct isp_hw_slice_spath_thumbscaler *spath = NULL;

	spath = (struct isp_hw_slice_spath_thumbscaler *)arg;

	if (!spath->path_en)
		return 0;

	addr = ISP_GET_REG(ISP_THMB_BEFORE_TRIM_SIZE);
	cmd = ((spath->slc_scaler->src0.h & 0x1FFF) << 16) |
		(spath->slc_scaler->src0.w & 0x1FFF);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_THMB_Y_SLICE_SRC_SIZE);
	cmd = ((spath->slc_scaler->y_src_after_deci.h & 0x1FFF) << 16) |
		(spath->slc_scaler->y_src_after_deci.w & 0x1FFF);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_THMB_Y_DES_SIZE);
	cmd = ((spath->slc_scaler->y_dst_after_scaler.h & 0x3FF) << 16) |
		(spath->slc_scaler->y_dst_after_scaler.w & 0x3FF);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_THMB_Y_TRIM0_START);
	cmd = ((spath->slc_scaler->y_trim.start_y & 0x1FFF) << 16) |
		(spath->slc_scaler->y_trim.start_x & 0x1FFF);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_THMB_Y_TRIM0_SIZE);
	cmd = ((spath->slc_scaler->y_trim.size_y & 0x1FFF) << 16) |
		(spath->slc_scaler->y_trim.size_x & 0x1FFF);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_THMB_Y_INIT_PHASE);
	cmd = ((spath->slc_scaler->y_init_phase.h & 0x3FF) << 16) |
		(spath->slc_scaler->y_init_phase.w & 0x3FF);
	FMCU_PUSH(spath->fmcu, addr, cmd);


	addr = ISP_GET_REG(ISP_THMB_UV_SLICE_SRC_SIZE);
	cmd = ((spath->slc_scaler->uv_src_after_deci.h & 0x1FFF) << 16) |
		(spath->slc_scaler->uv_src_after_deci.w & 0x1FFF);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_THMB_UV_DES_SIZE);
	cmd = ((spath->slc_scaler->uv_dst_after_scaler.h & 0x3FF) << 16) |
		(spath->slc_scaler->uv_dst_after_scaler.w & 0x3FF);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_THMB_UV_TRIM0_START);
	cmd = ((spath->slc_scaler->uv_trim.start_y & 0x1FFF) << 16) |
		(spath->slc_scaler->uv_trim.start_x & 0x1FFF);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_THMB_UV_TRIM0_SIZE);
	cmd = ((spath->slc_scaler->uv_trim.size_y & 0x1FFF) << 16) |
		(spath->slc_scaler->uv_trim.size_x & 0x1FFF);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_THMB_UV_INIT_PHASE);
	cmd = ((spath->slc_scaler->uv_init_phase.h & 0x3FF) << 16) |
		(spath->slc_scaler->uv_init_phase.w & 0x3FF);
	FMCU_PUSH(spath->fmcu, addr, cmd);

	return 0;
}

static int isphw_slice_fetch_set(void *handle, void *arg)
{
	uint32_t addr = 0, cmd = 0;
	struct isp_hw_set_slice_fetch *fetcharg = NULL;

	fetcharg = (struct isp_hw_set_slice_fetch *)arg;

	addr = ISP_GET_REG(ISP_FETCH_MEM_SLICE_SIZE);
	cmd = ((fetcharg->fetch_info->size.h & 0xFFFF) << 16) |
			(fetcharg->fetch_info->size.w & 0xFFFF);
	FMCU_PUSH(fetcharg->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_FETCH_SLICE_Y_ADDR);
	cmd = fetcharg->fetch_info->addr.addr_ch0;
	FMCU_PUSH(fetcharg->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_FETCH_SLICE_U_ADDR);
	cmd = fetcharg->fetch_info->addr.addr_ch1;
	FMCU_PUSH(fetcharg->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_FETCH_SLICE_V_ADDR);
	cmd = fetcharg->fetch_info->addr.addr_ch2;
	FMCU_PUSH(fetcharg->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_FETCH_MIPI_INFO);
	cmd = fetcharg->fetch_info->mipi_word_num |
			(fetcharg->fetch_info->mipi_byte_rel_pos << 16);
	FMCU_PUSH(fetcharg->fmcu, addr, cmd);

	/* dispatch size same as fetch size */
	addr = ISP_GET_REG(ISP_DISPATCH_CH0_SIZE);
	cmd = ((fetcharg->fetch_info->size.h & 0xFFFF) << 16) |
			(fetcharg->fetch_info->size.w & 0xFFFF);
	FMCU_PUSH(fetcharg->fmcu, addr, cmd);

	return 0;
}

static int isphw_slice_nr_info_set(void *handle, void *arg)
{
	uint32_t addr = 0, cmd = 0;
	struct isp_hw_set_slice_nr_info *nrarg = NULL;

	nrarg = (struct isp_hw_set_slice_nr_info *)arg;
	/* NLM */
	addr = ISP_GET_REG(ISP_NLM_RADIAL_1D_DIST);
	cmd = ((nrarg->slice_nlm->center_y_relative & 0x3FFF) << 16) |
		(nrarg->slice_nlm->center_x_relative & 0x3FFF);
	FMCU_PUSH(nrarg->fmcu, addr, cmd);

	/* Post CDN */
	addr = ISP_GET_REG(ISP_POSTCDN_SLICE_CTRL);
	cmd = nrarg->start_row_mod4;
	FMCU_PUSH(nrarg->fmcu, addr, cmd);

	/* YNR */
	addr = ISP_GET_REG(ISP_YNR_CFG31);
	cmd = ((nrarg->slice_ynr->center_offset_y & 0xFFFF) << 16) |
		(nrarg->slice_ynr->center_offset_x & 0xFFFF);
	FMCU_PUSH(nrarg->fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_YNR_CFG33);
	cmd = ((nrarg->slice_ynr->slice_height & 0xFFFF) << 16) |
		(nrarg->slice_ynr->slice_width & 0xFFFF);
	FMCU_PUSH(nrarg->fmcu, addr, cmd);

	return 0;
}

static int isphw_ltm_param_set(void *handle, void *arg)
{
	struct isp_hw_ltm_3dnr_param *parm = NULL;

	parm = (struct isp_hw_ltm_3dnr_param *)arg;

	ISP_REG_MWR(parm->idx, ISP_LTM_HIST_PARAM, BIT_0, parm->val);
	ISP_REG_MWR(parm->idx, ISP_LTM_MAP_PARAM0, BIT_0, parm->val);

	return 0;
}

static int isphw_3dnr_param_set(void *handle, void *arg)
{
	struct isp_hw_ltm_3dnr_param *parm = NULL;

	parm = (struct isp_hw_ltm_3dnr_param *)arg;

	ISP_REG_MWR(parm->idx, ISP_3DNR_MEM_CTRL_PARAM0, BIT_0, parm->val);
	ISP_REG_MWR(parm->idx, ISP_3DNR_BLEND_CONTROL0, BIT_0, parm->val);
	ISP_REG_MWR(parm->idx, ISP_3DNR_STORE_PARAM, BIT_0, parm->val);
	ISP_REG_MWR(parm->idx, ISP_3DNR_MEM_CTRL_PRE_PARAM0, BIT_0, parm->val);
	/* ISP_HREG_MWR(ISP_FBC_3DNR_STORE_PARAM, BIT_0, val); */

	return 0;
}

static int isphw_radius_parm_adpt(void *handle, void *arg)
{
	struct isp_hw_nlm_ynr *parm = NULL;

	parm = (struct isp_hw_nlm_ynr *)arg;

	parm->val = ISP_REG_RD(parm->ctx_id, ISP_NLM_RADIAL_1D_DIST);
	parm->slc_cfg_in->nlm_center_x = parm->val & 0x3FFF;
	parm->slc_cfg_in->nlm_center_y = (parm->val >> 16) & 0x3FFF;

	parm->val = ISP_REG_RD(parm->ctx_id, ISP_YNR_CFG31);
	parm->slc_cfg_in->ynr_center_x = parm->val & 0xFFFF;
	parm->slc_cfg_in->ynr_center_y = (parm->val >> 16) & 0xfFFF;
	pr_debug("ctx %d,  nlm center %d %d, ynr center %d, %d\n",
		parm->ctx_id, parm->slc_cfg_in->nlm_center_x, parm->slc_cfg_in->nlm_center_y,
		parm->slc_cfg_in->ynr_center_x, parm->slc_cfg_in->ynr_center_y);

	return 0;
}

static int isphw_hw_stop(void *handle, void *arg)
{
	uint32_t id;
	uint32_t cid;
	struct cam_hw_info *hw = NULL;

	hw = (struct cam_hw_info *)handle;
	id = hw->ip_isp->idx;

	ISP_HREG_MWR(ISP_AXI_ITI2AXIM_CTRL, BIT_26, 1 << 26);

	pr_info("ISP%d:ISP_AXI_AXIM_CTRL 0x%x INT STATUS 0x%x  0x%x 0x%x 0x%x\n",
		id, ISP_HREG_RD(ISP_AXI_ITI2AXIM_CTRL),
		ISP_HREG_RD(ISP_P0_INT_BASE + ISP_INT_STATUS),
		ISP_HREG_RD(ISP_C0_INT_BASE + ISP_INT_STATUS),
		ISP_HREG_RD(ISP_P1_INT_BASE + ISP_INT_STATUS),
		ISP_HREG_RD(ISP_C1_INT_BASE + ISP_INT_STATUS));
	udelay(10);

	for (cid = 0; cid < 4; cid++)
		hw->isp_ioctl(hw, ISP_HW_CFG_CLEAR_IRQ, &cid);

	return 0;
}

static int isphw_frame_addr_store(void *handle, void *arg)
{
	uint32_t idx = 0;
	struct isp_hw_path_store *path_store = NULL;
	struct isp_store_info *store_info = NULL;
	unsigned long addr = 0;

	path_store = (struct isp_hw_path_store *)arg;
	idx = path_store->ctx_id;
	store_info = &path_store->store;
	addr = store_base[path_store->spath_id];

	ISP_REG_WR(idx, addr + ISP_STORE_SLICE_Y_ADDR, store_info->addr.addr_ch0);
	ISP_REG_WR(idx, addr + ISP_STORE_SLICE_U_ADDR, store_info->addr.addr_ch1);
	ISP_REG_WR(idx, addr + ISP_STORE_SLICE_V_ADDR, store_info->addr.addr_ch2);

	return 0;
}

static int isphw_frame_addr_fetch(void *handle, void *arg)
{
	uint32_t idx = 0;
	struct isp_hw_fetch_info *fetch = NULL;

	if (!arg) {
		pr_err("fail to get valid arg\n");
		return -EFAULT;
	}

	fetch = (struct isp_hw_fetch_info *)arg;
	idx = fetch->ctx_id;

	ISP_REG_WR(idx, ISP_FETCH_SLICE_Y_ADDR, fetch->addr_hw.addr_ch0);
	ISP_REG_WR(idx, ISP_FETCH_SLICE_U_ADDR, fetch->addr_hw.addr_ch1);
	ISP_REG_WR(idx, ISP_FETCH_SLICE_V_ADDR, fetch->addr_hw.addr_ch2);

	return 0;
}

static int isphw_map_init_cfg(void *handle, void *arg)
{
	uint32_t val = 0;
	uint32_t i = 0;
	struct isp_hw_cfg_map *maparg = NULL;

	maparg = (struct isp_hw_cfg_map *)arg;

	val = (maparg->s_cfg_settings->pre1_cmd_ready_mode << 27)|
		(maparg->s_cfg_settings->pre0_cmd_ready_mode << 26)|
		(maparg->s_cfg_settings->cap1_cmd_ready_mode << 25)|
		(maparg->s_cfg_settings->cap0_cmd_ready_mode << 24)|
		(maparg->s_cfg_settings->bp_cap1_pixel_rdy << 23) |
		(maparg->s_cfg_settings->bp_cap0_pixel_rdy << 22) |
		(maparg->s_cfg_settings->bp_pre1_pixel_rdy << 21) |
		(maparg->s_cfg_settings->bp_pre0_pixel_rdy << 20) |
		(maparg->s_cfg_settings->cfg_main_sel << 16) |
		(maparg->s_cfg_settings->num_of_mod << 8) |
		(maparg->s_cfg_settings->sdw_mode << 5) |
		(maparg->s_cfg_settings->tm_bypass << 4) |
		(maparg->s_cfg_settings->bypass);

	if (atomic_inc_return(&maparg->map_cnt) == 1) {
		pr_info("cfg map init start\n");
		for (i = 0; i < maparg->s_cfg_settings->num_of_mod; i++) {
			ISP_HREG_WR(ISP_CFG0_BUF + i * 4,
				maparg->s_cfg_settings->isp_cfg_map[i]);
			ISP_HREG_WR(ISP_CFG1_BUF + i * 4,
				maparg->s_cfg_settings->isp_cfg_map[i]);
		}
	}

	ISP_HREG_WR(ISP_CFG_PAMATER, val);

	if (!maparg->s_cfg_settings->tm_bypass) {
		ISP_HREG_WR(ISP_CFG_TM_NUM,
				maparg->s_cfg_settings->tm_set_number);
		ISP_HREG_WR(ISP_CFG_CAP0_TH,
				maparg->s_cfg_settings->cap0_th);
		ISP_HREG_WR(ISP_CFG_CAP1_TH,
				maparg->s_cfg_settings->cap1_th);
	}

	ISP_HREG_MWR(ISP_ARBITER_ENDIAN_COMM, BIT_0, 0x1);

	return 0;
}

static int isphw_cfg_cmd_ready(void *handle, void *arg)
{
	struct isp_hw_cfg_info *cfg_info = (struct isp_hw_cfg_info *)arg;
	struct cam_hw_info *hw = NULL;
	uint32_t val = 0;
	uint32_t hw_ctx_id = cfg_info->hw_ctx_id;
	uint32_t ready_mode[ISP_CONTEXT_HW_NUM] = {
		BIT_26,/* pre0_cmd_ready_mode */
		BIT_24,/* cap0_cmd_ready_mode */
		BIT_27,/* pre1_cmd_ready_mode */
		BIT_25/* cap1_cmd_ready_mode */
	};

	hw = (struct cam_hw_info *)handle;
	if (cfg_info->fmcu_enable)
		val = ready_mode[hw_ctx_id];
	else
		val = 0;

	spin_lock(&hw->isp_cfg_lock);
	ISP_HREG_MWR(ISP_CFG_PAMATER, ready_mode[hw_ctx_id], val);
	spin_unlock(&hw->isp_cfg_lock);

	ISP_HREG_WR(cfg_cmd_addr_reg[hw_ctx_id], cfg_info->hw_addr);
	pr_debug("ctx %d,  reg %08x  %08x, hw_addr %lx, val %08x\n",
		hw_ctx_id,
		(uint32_t)cfg_cmd_addr_reg[hw_ctx_id],
		(uint32_t)ISP_GET_REG(cfg_cmd_addr_reg[hw_ctx_id]),
		cfg_info->hw_addr,
		ISP_HREG_RD(cfg_cmd_addr_reg[hw_ctx_id]));

	return 0;
}

static int isphw_isp_start_cfg(void *handle, void *arg)
{
	uint32_t ctx_id = 0;
	unsigned long reg_addr[] = {
		ISP_CFG_PRE0_START,
		ISP_CFG_CAP0_START,
		ISP_CFG_PRE1_START,
		ISP_CFG_CAP1_START,
	};

	ctx_id = *(uint32_t *)arg;

	pr_debug("isp cfg start:  context_id %d, P0_addr 0x%x\n", ctx_id,
		ISP_HREG_RD(ISP_CFG_PRE0_CMD_ADDR));

	ISP_HREG_WR(reg_addr[ctx_id], 1);
	return 0;
}

static int isphw_hist_roi_update(void *handle, void *arg)
{
	uint32_t val = 0;
	struct isp_hw_hist_roi *hist_arg = NULL;

	hist_arg = (struct isp_hw_hist_roi *)arg;

	val = (hist_arg->hist_roi->start_y & 0xFFFF) | ((hist_arg->hist_roi->start_x & 0xFFFF) << 16);
	ISP_REG_WR(hist_arg->ctx_id, ISP_HIST2_ROI_S0, val);

	val = (hist_arg->hist_roi->end_y & 0xFFFF) | ((hist_arg->hist_roi->end_x & 0xFFFF) << 16);
	ISP_REG_WR(hist_arg->ctx_id, ISP_HIST2_ROI_E0, val);

	return 0;
}

static int isphw_fetch_start(void *handle, void *arg)
{
	ISP_HREG_WR(ISP_FETCH_START, 1);

	return 0;
}

static int isphw_fmcu_cmd_set(void *handle, void *arg)
{
	struct isp_hw_fmcu_cmd *cmdarg = NULL;

	cmdarg = (struct isp_hw_fmcu_cmd *)arg;

	ISP_HREG_WR(cmdarg->base + ISP_FMCU_DDR_ADDR, cmdarg->hw_addr);
	ISP_HREG_MWR(cmdarg->base + ISP_FMCU_CTRL, 0xFFFF0000, cmdarg->cmd_num << 16);
	ISP_HREG_WR(cmdarg->base + ISP_FMCU_CMD_READY, 1);

	return 0;
}

static int isphw_fmcu_start(void *handle, void *arg)
{
	struct isp_hw_fmcu_start *startarg = NULL;
	uint32_t base = 0;

	startarg = (struct isp_hw_fmcu_start *)arg;

	if (startarg->fid == 0)
		base = ISP_FMCU0_BASE;
	else if (startarg->fid == 1)
		base = ISP_FMCU1_BASE;
	ISP_HREG_WR(base + ISP_FMCU_DDR_ADDR, startarg->hw_addr);
	ISP_HREG_MWR(base + ISP_FMCU_CTRL, 0xFFFF0000, startarg->cmd_num << 16);
	ISP_HREG_WR(base + ISP_FMCU_ISP_REG_REGION, ISP_OFFSET_RANGE);
	ISP_HREG_WR(base + ISP_FMCU_START, 1);

	return 0;
}

static int isphw_yuv_block_ctrl(void *handle, void *arg)
{
	uint32_t ret = 0;
	uint32_t idx = 0;
	uint32_t type = 0;
	struct isp_k_block *p = NULL;
	struct isp_hw_yuv_block_ctrl *blk_ctrl = NULL;

	if (!arg) {
		pr_err("fail to get valid input arg\n");
		return -EFAULT;
	}

	blk_ctrl = (struct isp_hw_yuv_block_ctrl *)arg;
	type = blk_ctrl->type;
	idx = blk_ctrl->idx;

	if (type == ISP_YUV_BLOCK_CFG) {
		p = blk_ctrl->blk_param;
		goto BLOCK_CFG;
	} else if (type == ISP_YUV_BLOCK_DISABLE) {
		goto BLOCK_BYPASS;
	} else {
		pr_err("fail to support type %d\n", type);
	}

BLOCK_CFG:
	ISP_REG_MWR(idx, ISP_CCE_PARAM, BIT_0, p->cce_info.bypass);
	ISP_REG_MWR(idx, ISP_BCHS_PARAM, BIT_0, p->bchs_info.bchs_bypass);
	ISP_REG_MWR(idx, ISP_CDN_PARAM, BIT_0, p->cdn_info.bypass);
	ISP_REG_MWR(idx, ISP_EE_PARAM, BIT_0, p->edge_info.bypass);
	ISP_REG_MWR(idx, ISP_IIRCNR_PARAM, BIT_0, p->iircnr_info.bypass);
	ISP_REG_MWR(idx, ISP_YUV_NF_CTRL, BIT_0, p->nf_info.yrandom_bypass);
	ISP_REG_MWR(idx, ISP_POSTCDN_COMMON_CTRL,
		BIT_0, p->post_cdn_info.bypass);
	ISP_REG_MWR(idx, ISP_PRECDN_PARAM, BIT_0, p->pre_cdn_info.bypass);
	ISP_REG_MWR(idx, ISP_PSTRZ_PARAM, BIT_0, p->pstrz_info_v2.bypass);
	ISP_REG_MWR(idx, ISP_UVD_PARAM, BIT_0, p->uvd_info_v2.bypass);
	ISP_REG_MWR(idx, ISP_YGAMMA_PARAM, BIT_0, p->ygamma_info.bypass);
	ISP_REG_MWR(idx, ISP_YNR_CONTRL0, BIT_0, p->ynr_info_v2.bypass);
	ISP_REG_MWR(idx, ISP_YRANDOM_PARAM1, BIT_0, p->yrandom_info.bypass);
	return ret;

BLOCK_BYPASS:
	ISP_REG_MWR(idx, ISP_CCE_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_BCHS_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_CDN_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_EE_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_HIST2_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_IIRCNR_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_YUV_NF_CTRL, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_3DNR_MEM_CTRL_PARAM0, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_POSTCDN_COMMON_CTRL, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_PRECDN_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_PSTRZ_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_UVD_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_YGAMMA_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_YNR_CONTRL0, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_YRANDOM_PARAM1, BIT_0, 1);

	return ret;
}

static int isphw_subblock_reconfig(void *handle, void *arg)
{
	uint32_t ret = 0, idx = 0;
	struct isp_k_block *p = NULL;
	struct isp_hw_yuv_block_ctrl *blk_ctrl = NULL;

	if (!arg) {
		pr_err("fail to get valid input arg\n");
		return -EFAULT;
	}

	blk_ctrl = (struct isp_hw_yuv_block_ctrl *)arg;
	p = blk_ctrl->blk_param;
	idx = blk_ctrl->idx;
	if (!p) {
		pr_err("fail to get reconfig blk param\n");
		return -EFAULT;
	}
	/* reconfig isp sublock */
	isp_k_bchs_block(p, idx);
	isp_k_cce_block(p, idx);
	isp_k_cdn_block(p, idx);
	isp_k_cfa_block(p, idx);
	isp_k_cmc10_block(p, idx);
	isp_k_edge_block(p, idx);
	isp_k_gamma_block(p, idx);
	isp_k_hsv_block(p, idx);
	isp_k_iircnr_block(p, idx);
	isp_k_nlm_block(p, idx);
	isp_k_nlm_imblance(p, idx);
	isp_k_post_cdn_block(p, idx);
	isp_k_precdn_block(p, idx);
	isp_k_yrandom_block(p, idx);
	isp_k_uvd_block(p, idx);
	isp_k_ynr_block(p, idx);
	isp_k_ygamma_block(p, idx);
	isp_3dnr_config_blend(idx, &p->nr3_info_base.blend);
	return ret;
}

static struct hw_io_ctrl_fun isp_ioctl_fun_tab[] = {
	{ISP_HW_CFG_ENABLE_CLK,              isphw_clk_eb},
	{ISP_HW_CFG_DISABLE_CLK,             isphw_clk_dis},
	{ISP_HW_CFG_RESET,                   isphw_reset},
	{ISP_HW_CFG_ENABLE_IRQ,              isphw_irq_enable},
	{ISP_HW_CFG_DISABLE_IRQ,             isphw_irq_disable},
	{ISP_HW_CFG_CLEAR_IRQ,               isphw_irq_clear},
	{ISP_HW_CFG_FETCH_SET,               isphw_fetch_set},
	{ISP_HW_CFG_FETCH_FBD_SET,           isphw_fetch_fbd_set},
	{ISP_HW_CFG_DEFAULT_PARA_SET,        isphw_default_param_set},
	{ISP_HW_CFG_BLOCK_FUNC_GET,          isphw_block_func_get},
	{ISP_HW_CFG_PARAM_BLOCK_FUNC_GET,    isphw_param_get},
	{ISP_HW_CFG_K_BLK_FUNC_GET,          isphw_k_blk_func_get},
	{ISP_HW_CFG_CFG_MAP_INFO_GET,        isphw_cfg_map_info_get},
	{ISP_HW_CFG_FMCU_VALID_GET,          isphw_fmcu_available},
	{ISP_HW_CFG_BYPASS_DATA_GET,         isphw_bypass_data_get},
	{ISP_HW_CFG_BYPASS_COUNT_GET,        isphw_bypass_count_get},
	{ISP_HW_CFG_REG_TRACE,               isphw_reg_trace},
	{ISP_HW_CFG_ISP_CFG_SUBBLOCK,        isphw_subblock_cfg},
	{ISP_HW_CFG_SET_PATH_STORE,          isphw_path_store},
	{ISP_HW_CFG_SET_PATH_SCALER,         isphw_path_scaler},
	{ISP_HW_CFG_SET_PATH_THUMBSCALER,    isphw_path_thumbscaler},
	{ISP_HW_CFG_SLICE_SCALER,            isphw_slice_scaler},
	{ISP_HW_CFG_SLICE_STORE,             isphw_slice_store},
	{ISP_HW_CFG_LTM_SLICE_SET,           isphw_ltm_slice_set},
	{ISP_HW_CFG_SLW_FMCU_CMDS,           isphw_slw_fmcu_cmds},
	{ISP_HW_CFG_FMCU_CFG,                isphw_fmcu_cfg},
	{ISP_HW_CFG_SLICE_FETCH,             isphw_slice_fetch},
	{ISP_HW_CFG_SLICE_NR_INFO,           isphw_slice_nr_info},
	{ISP_HW_CFG_SLICE_FMCU_CMD,          isphw_slices_fmcu_cmds},
	{ISP_HW_CFG_SLICE_NOFILTER,          isphw_slice_nofilter},
	{ISP_HW_CFG_SLICE_3DNR_CROP,         isphw_slice_3dnr_crop},
	{ISP_HW_CFG_SLICE_3DNR_STORE,        isphw_slice_3dnr_store},
	{ISP_HW_CFG_SLICE_3DNR_MEMCTRL,      isphw_slice_3dnr_memctrl},
	{ISP_HW_CFG_SLICE_SPATH_STORE,       isphw_slice_spath_store},
	{ISP_HW_CFG_SLICE_SPATH_SCALER,      isphw_slice_spath_scaler},
	{ISP_HW_CFG_SLICE_SPATH_THUMBSCALER, isphw_slice_spath_thumbscaler},
	{ISP_HW_CFG_SET_SLICE_FETCH,         isphw_slice_fetch_set},
	{ISP_HW_CFG_SET_SLICE_NR_INFO,       isphw_slice_nr_info_set},
	{ISP_HW_CFG_LTM_PARAM,               isphw_ltm_param_set},
	{ISP_HW_CFG_3DNR_PARAM,              isphw_3dnr_param_set},
	{ISP_HW_CFG_GET_NLM_YNR,             isphw_radius_parm_adpt},
	{ISP_HW_CFG_STOP,                    isphw_hw_stop},
	{ISP_HW_CFG_STORE_FRAME_ADDR,        isphw_frame_addr_store},
	{ISP_HW_CFG_FETCH_FRAME_ADDR,        isphw_frame_addr_fetch},
	{ISP_HW_CFG_MAP_INIT,                isphw_map_init_cfg},
	{ISP_HW_CFG_CMD_READY,               isphw_cfg_cmd_ready},
	{ISP_HW_CFG_START_ISP,               isphw_isp_start_cfg},
	{ISP_HW_CFG_UPDATE_HIST_ROI,         isphw_hist_roi_update},
	{ISP_HW_CFG_FETCH_START,             isphw_fetch_start},
	{ISP_HW_CFG_FMCU_CMD,                isphw_fmcu_cmd_set},
	{ISP_HW_CFG_FMCU_START,              isphw_fmcu_start},
	{ISP_HW_CFG_YUV_BLOCK_CTRL_TYPE,     isphw_yuv_block_ctrl},
	{ISP_HW_CFG_SUBBLOCK_RECFG,          isphw_subblock_reconfig},
};

static hw_ioctl_fun isphw_ioctl_fun_get(enum isp_hw_cfg_cmd cmd)
{
	hw_ioctl_fun hw_ctrl = NULL;
	uint32_t total_num = 0;
	uint32_t i = 0;

	total_num = sizeof(isp_ioctl_fun_tab) / sizeof(struct hw_io_ctrl_fun);
	for (i = 0; i < total_num; i++) {
		if (cmd == isp_ioctl_fun_tab[i].cmd) {
			hw_ctrl = isp_ioctl_fun_tab[i].hw_ctrl;
			break;
		}
	}

	return hw_ctrl;
}
#endif
