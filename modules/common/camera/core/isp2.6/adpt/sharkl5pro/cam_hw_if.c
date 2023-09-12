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

#include <linux/clk.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <sprd_mm.h>
#include <linux/mfd/syscon.h>
#include <linux/of_address.h>

#include "cam_trusty.h"
#include "dcam_reg.h"
#include "dcam_hw_adpt.h"
#include "dcam_int.h"
#include "dcam_path.h"

#include "isp_reg.h"
#include "isp_hw_adpt.h"
#include "isp_core.h"
#include "isp_slice.h"
#include "isp_cfg.h"
#include "isp_path.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CAM_HW_IF_L5PRO: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static unsigned long coff_buf_addr[2][3][4] = {
	{
		{
			ISP_SCALER_PRE_LUMA_HCOEFF_BUF0,
			ISP_SCALER_PRE_CHROMA_HCOEFF_BUF0,
			ISP_SCALER_PRE_LUMA_VCOEFF_BUF0,
			ISP_SCALER_PRE_CHROMA_VCOEFF_BUF0,
		},
		{
			ISP_SCALER_VID_LUMA_HCOEFF_BUF0,
			ISP_SCALER_VID_CHROMA_HCOEFF_BUF0,
			ISP_SCALER_VID_LUMA_VCOEFF_BUF0,
			ISP_SCALER_VID_CHROMA_VCOEFF_BUF0,
		}
	},
	{
		{
			ISP_SCALER_PRE_LUMA_HCOEFF_BUF1,
			ISP_SCALER_PRE_CHROMA_HCOEFF_BUF1,
			ISP_SCALER_PRE_LUMA_VCOEFF_BUF1,
			ISP_SCALER_PRE_CHROMA_VCOEFF_BUF1,
		},
		{
			ISP_SCALER_VID_LUMA_HCOEFF_BUF1,
			ISP_SCALER_VID_CHROMA_HCOEFF_BUF1,
			ISP_SCALER_VID_LUMA_VCOEFF_BUF1,
			ISP_SCALER_VID_CHROMA_VCOEFF_BUF1,
		}
	},
};

#define CAM_HW_ADPT_LAYER
#include "dcam_hw.c"
#include "isp_hw.c"

static int camhw_get_isp_dts_clk(void *handle, void *arg)
{
	struct cam_hw_soc_info *soc_isp = NULL;
	struct cam_hw_info *hw = NULL;
	struct device_node *isp_node = (struct device_node *)arg;

	hw = (struct cam_hw_info *)handle;
	if (!handle) {
		pr_err("fail to get invalid handle\n");
		return -EINVAL;
	}

	soc_isp = hw->soc_isp;

	soc_isp->core_eb = of_clk_get_by_name(isp_node, "isp_eb");
	if (IS_ERR_OR_NULL(soc_isp->core_eb)) {
		pr_err("fail to read dts isp eb\n");
		return -EFAULT;
	}
	soc_isp->axi_eb = of_clk_get_by_name(isp_node, "isp_axi_eb");
	if (IS_ERR_OR_NULL(soc_isp->axi_eb)) {
		pr_err("fail to read dts isp axi eb\n");
		return -EFAULT;
	}
	soc_isp->clk = of_clk_get_by_name(isp_node, "isp_clk");
	if (IS_ERR_OR_NULL(soc_isp->clk)) {
		pr_err("fail to read dts isp clk\n");
		return -EFAULT;
	}
	soc_isp->clk_parent = of_clk_get_by_name(isp_node, "isp_clk_parent");
	if (IS_ERR_OR_NULL(soc_isp->clk_parent)) {
		pr_err("fail to read dts isp clk parent\n");
		return -EFAULT;
	}
	soc_isp->clk_default = clk_get_parent(soc_isp->clk);
	return 0;
}

static int camhw_get_dcam_dts_clk(void *handle, void *arg)
{
	int ret = 0;
	struct cam_hw_info *hw = NULL;
	struct device_node *dn = (struct device_node *)arg;
	struct cam_hw_soc_info *soc_dcam;

	hw = (struct cam_hw_info *)handle;
	if (!handle) {
		pr_err("fail to get invalid handle\n");
		return -EINVAL;
	}

	soc_dcam = hw->soc_dcam;

	soc_dcam->core_eb = of_clk_get_by_name(dn, "dcam_eb");
	if (IS_ERR_OR_NULL(soc_dcam->core_eb)) {
		pr_err("fail to read clk, dcam_eb\n");
		ret = 1;
	}

	soc_dcam->axi_eb = of_clk_get_by_name(dn, "dcam_axi_eb");
	if (IS_ERR_OR_NULL(soc_dcam->axi_eb)) {
		pr_err("fail to read clk, dcam_axi_eb\n");
		ret = 1;
	}

	soc_dcam->clk = of_clk_get_by_name(dn, "dcam_clk");
	if (IS_ERR_OR_NULL(soc_dcam->clk)) {
		pr_err("fail to read clk, dcam_clk\n");
		ret = 1;
	}

	soc_dcam->clk_parent = of_clk_get_by_name(dn, "dcam_clk_parent");
	if (IS_ERR_OR_NULL(soc_dcam->clk_parent)) {
		pr_err("fail to read clk, dcam_clk_parent\n");
		ret = 1;
	}
	soc_dcam->clk_default = clk_get_parent(soc_dcam->clk);

	soc_dcam->axi_clk = of_clk_get_by_name(dn, "dcam_axi_clk");
	if (IS_ERR_OR_NULL(soc_dcam->axi_clk)) {
		pr_err("fail to read clk, axi_clk\n");
		ret = 1;
	}
	soc_dcam->axi_clk_parent = of_clk_get_by_name(dn, "dcam_axi_clk_parent");
	if (IS_ERR_OR_NULL(soc_dcam->axi_clk_parent)) {
		pr_err("fail to read clk, axi_clk_parent\n");
		ret = 1;
	}
	soc_dcam->axi_clk_default = clk_get_parent(soc_dcam->axi_clk);

	return ret;
}

static int camhw_get_all_rst(void *handle, void *arg)
{
	int ret = 0;
	struct cam_hw_info *hw = NULL;
	uint32_t args[2];
	struct device_node *dn = (struct device_node *)arg;
	struct cam_hw_ip_info *dcam_info = NULL;

	int i = 0;
	if (!handle) {
		pr_err("fail to get invalid handle\n");
		return -EINVAL;
	}

	hw = (struct cam_hw_info *)handle;

	ret = cam_syscon_get_args_by_name(dn, "dcam_all_reset", ARRAY_SIZE(args), args);
	if (ret) {
		pr_err("fail to get dcam all reset syscon\n");
		return -EINVAL;
	}

	for (i = 0; i < DCAM_ID_MAX; i++) {
		dcam_info = hw->ip_dcam[i];
		dcam_info->syscon.all_rst = args[0];
		dcam_info->syscon.all_rst_mask= args[1];
	}

	return ret;
}

static int camhw_get_axi_base(void *handle, void *arg)
{
	struct cam_hw_info *hw = NULL;
	struct device_node *dn = (struct device_node *)arg;
	int pos = 0;
	uint32_t count;
	struct resource reg_res = {0};
	void __iomem *reg_base = NULL;
	struct cam_hw_soc_info *soc_dcam;
	int i = 0;
	if (!handle) {
		pr_err("fail to get invalid handle\n");
		return -EINVAL;
	}

	hw = (struct cam_hw_info *)handle;
	soc_dcam = hw->soc_dcam;
	if (of_property_read_u32(dn, "sprd,dcam-count", &count)) {
		pr_err("fail to parse the property of sprd,dcam-count\n");
		return -EINVAL;
	}

	pos = count;
	if (of_address_to_resource(dn, pos, &reg_res)) {
		pr_err("fail to get AXIM phy addr\n");
		goto err_axi_iounmap;
	}

	reg_base = ioremap(reg_res.start, reg_res.end - reg_res.start + 1);
	if (!reg_base) {
		pr_err("fail to map AXIM reg base\n");
		goto err_axi_iounmap;
	}

	for (i = 0; i < count; i++)
		g_dcam_aximbase[i] = (unsigned long)reg_base;

	soc_dcam->axi_reg_base = (unsigned long)reg_base;

	return 0;

err_axi_iounmap:
	for (i = 0; i < DCAM_ID_MAX; i++) {
		g_dcam_aximbase[i] = 0;
	}
	return -1;
}

static struct hw_io_ctrl_fun cam_ioctl_fun_tab[] = {
	{CAM_HW_GET_ALL_RST,            camhw_get_all_rst},
	{CAM_HW_GET_AXI_BASE,           camhw_get_axi_base},
	{CAM_HW_GET_DCAM_DTS_CLK,       camhw_get_dcam_dts_clk},
	{CAM_HW_GET_ISP_DTS_CLK,        camhw_get_isp_dts_clk},
};

static hw_ioctl_fun camhw_ioctl_fun_get(enum cam_hw_cfg_cmd cmd)
{
	hw_ioctl_fun hw_ctrl = NULL;
	uint32_t total_num = 0;
	uint32_t i = 0;

	total_num = sizeof(cam_ioctl_fun_tab) / sizeof(struct hw_io_ctrl_fun);
	for (i = 0; i < total_num; i++) {
		if (cmd == cam_ioctl_fun_tab[i].cmd) {
			hw_ctrl = cam_ioctl_fun_tab[i].hw_ctrl;
			break;
		}
	}

	return hw_ctrl;
}

#undef CAM_HW_ADPT_LAYER

const unsigned long slowmotion_store_addr[3][4] = {
	{
		DCAM_BIN_BASE_WADDR0,
		DCAM_BIN_BASE_WADDR1,
		DCAM_BIN_BASE_WADDR2,
		DCAM_BIN_BASE_WADDR3
	},
	{
		DCAM_AEM_BASE_WADDR,
		DCAM_AEM_BASE_WADDR1,
		DCAM_AEM_BASE_WADDR2,
		DCAM_AEM_BASE_WADDR3
	},
	{
		DCAM_HIST_BASE_WADDR,
		DCAM_HIST_BASE_WADDR1,
		DCAM_HIST_BASE_WADDR2,
		DCAM_HIST_BASE_WADDR3
	}
};

static uint32_t path_ctrl_id[DCAM_PATH_MAX] = {
	[DCAM_PATH_FULL] = DCAM_CTRL_FULL,
	[DCAM_PATH_BIN] = DCAM_CTRL_BIN,
	[DCAM_PATH_PDAF] = DCAM_CTRL_PDAF,
	[DCAM_PATH_VCH2] = DCAM_CTRL_VCH2,
	[DCAM_PATH_VCH3] = DCAM_CTRL_VCH3,
	[DCAM_PATH_AEM] = DCAM_CTRL_COEF,
	[DCAM_PATH_AFM] = DCAM_CTRL_COEF,
	[DCAM_PATH_AFL] = DCAM_CTRL_COEF,
	[DCAM_PATH_HIST] = DCAM_CTRL_COEF,
	[DCAM_PATH_3DNR] = DCAM_CTRL_COEF,
	[DCAM_PATH_BPC] = DCAM_CTRL_COEF,
	[DCAM_PATH_LSCM] = DCAM_CTRL_COEF,
};

static unsigned long dcam_store_addr[DCAM_PATH_MAX] = {
	[DCAM_PATH_FULL] = DCAM_FULL_BASE_WADDR,
	[DCAM_PATH_BIN] = DCAM_BIN_BASE_WADDR0,
	[DCAM_PATH_PDAF] = DCAM_PDAF_BASE_WADDR,
	[DCAM_PATH_VCH2] = DCAM_VCH2_BASE_WADDR,
	[DCAM_PATH_VCH3] = DCAM_VCH3_BASE_WADDR,
	[DCAM_PATH_AEM] = DCAM_AEM_BASE_WADDR,
	[DCAM_PATH_AFM] = ISP_AFM_BASE_WADDR,
	[DCAM_PATH_AFL] = ISP_AFL_GLB_WADDR,
	[DCAM_PATH_HIST] = DCAM_HIST_BASE_WADDR,
	[DCAM_PATH_3DNR] = ISP_NR3_WADDR,
	[DCAM_PATH_BPC] = ISP_BPC_OUT_ADDR,
	[DCAM_PATH_LSCM] = DCAM_LSCM_BASE_WADDR,
};

static uint32_t isp_ctx_fmcu_support[ISP_CONTEXT_HW_NUM] = {
	[ISP_CONTEXT_HW_P0] = 1,
	[ISP_CONTEXT_HW_C0] = 1,
	[ISP_CONTEXT_HW_P1] = 1,
	[ISP_CONTEXT_HW_C1] = 1,
};

static int camhwif_dcam_ioctl(void *handle,
	enum dcam_hw_cfg_cmd cmd, void *arg)
{
	int ret = 0;
	hw_ioctl_fun hw_ctrl = NULL;

	hw_ctrl = dcamhw_ioctl_fun_get(cmd);
	if (hw_ctrl != NULL)
		ret = hw_ctrl(handle, arg);
	else
		pr_debug("hw_core_ctrl_fun is null, cmd %d\n", cmd);

	return ret;
}

static int camhwif_isp_ioctl(void *handle,
	enum isp_hw_cfg_cmd cmd, void *arg)
{
	int ret = 0;
	hw_ioctl_fun hw_ctrl = NULL;

	hw_ctrl = isphw_ioctl_fun_get(cmd);
	if (hw_ctrl != NULL)
		ret = hw_ctrl(handle, arg);
	else
		pr_debug("hw_core_ctrl_fun is null, cmd %d\n", cmd);

	return ret;
}

static int camhwif_cam_ioctl(void *handle,
	enum cam_hw_cfg_cmd cmd, void *arg)
{
	int ret = 0;
	hw_ioctl_fun hw_ctrl = NULL;

	hw_ctrl = camhw_ioctl_fun_get(cmd);
	if (hw_ctrl != NULL)
		ret = hw_ctrl(handle, arg);
	else
		pr_debug("hw_core_ctrl_fun is null, cmd %d\n", cmd);

	return ret;
}

static struct cam_hw_soc_info dcam_soc_info;
static struct cam_hw_soc_info isp_soc_info;
static struct cam_hw_ip_info dcam[DCAM_ID_MAX] = {
	[DCAM_ID_0] = {
		.aux_dcam_path = DCAM_PATH_BIN,
		.slm_path = BIT(DCAM_PATH_BIN) | BIT(DCAM_PATH_AEM) | BIT(DCAM_PATH_HIST),
		.lbuf_share_support = 1,
		.offline_slice_support = 1,
		.afl_gbuf_size = STATIS_AFL_GBUF_SIZE,
		.superzoom_support = 1,
		.dcam_full_fbc_mode = DCAM_FBC_DISABLE,
		.dcam_bin_fbc_mode = DCAM_FBC_DISABLE,
		.dcam_offline_fbc_mode = DCAM_FBC_DISABLE,
		.store_addr_tab = dcam_store_addr,
		.path_ctrl_id_tab = path_ctrl_id,
		.pdaf_type3_reg_addr = DCAM_PPE_RIGHT_WADDR,
		.rds_en = 0,
		.dcam_raw_path_id = DCAM_PATH_FULL,
		.pyramid_support = 0,
		.fmcu_support = 0,
		.sensor_raw_fmt = DCAM_RAW_14,
		.save_band_for_bigsize = 0,
		.raw_fmt_support[0] = DCAM_RAW_14,
		.raw_fmt_support[1] = DCAM_RAW_8,
		.raw_fmt_support[2] = DCAM_RAW_HALFWORD_10,
		.raw_fmt_support[3] = DCAM_RAW_PACK_10,
		.dcam_output_support[0] = DCAM_STORE_10_BIT,
		.dcam_output_support[1] = DCAM_STORE_8_BIT,
		.recovery_support = 0,/*DCAMINT_INT0_FATAL_ERROR,*/
	},
	[DCAM_ID_1] = {
		.aux_dcam_path = DCAM_PATH_BIN,
		.slm_path = BIT(DCAM_PATH_BIN) | BIT(DCAM_PATH_AEM) | BIT(DCAM_PATH_HIST),
		.lbuf_share_support = 1,
		.offline_slice_support = 1,
		.afl_gbuf_size = STATIS_AFL_GBUF_SIZE,
		.superzoom_support = 1,
		.dcam_full_fbc_mode = DCAM_FBC_DISABLE,
		.dcam_bin_fbc_mode = DCAM_FBC_DISABLE,
		.dcam_offline_fbc_mode = DCAM_FBC_DISABLE,
		.store_addr_tab = dcam_store_addr,
		.path_ctrl_id_tab = path_ctrl_id,
		.pdaf_type3_reg_addr = DCAM_PPE_RIGHT_WADDR,
		.rds_en = 0,
		.dcam_raw_path_id = DCAM_PATH_FULL,
		.pyramid_support = 0,
		.fmcu_support = 0,
		.sensor_raw_fmt = DCAM_RAW_14,
		.save_band_for_bigsize = 0,
		.raw_fmt_support[0] = DCAM_RAW_14,
		.raw_fmt_support[1] = DCAM_RAW_8,
		.raw_fmt_support[2] = DCAM_RAW_HALFWORD_10,
		.raw_fmt_support[3] = DCAM_RAW_PACK_10,
		.dcam_output_support[0] = DCAM_STORE_10_BIT,
		.dcam_output_support[1] = DCAM_STORE_8_BIT,
		.recovery_support = 0,/*DCAMINT_INT0_FATAL_ERROR,*/
	},
	[DCAM_ID_2] = {
		.aux_dcam_path = DCAM_PATH_BIN,
		.slm_path = BIT(DCAM_PATH_BIN) | BIT(DCAM_PATH_AEM) | BIT(DCAM_PATH_HIST),
		.lbuf_share_support = 0,
		.offline_slice_support = 0,
		.afl_gbuf_size = STATIS_AFL_GBUF_SIZE,
		.superzoom_support = 1,
		.dcam_full_fbc_mode = DCAM_FBC_DISABLE,
		.dcam_bin_fbc_mode = DCAM_FBC_DISABLE,
		.dcam_offline_fbc_mode = DCAM_FBC_DISABLE,
		.store_addr_tab = dcam_store_addr,
		.path_ctrl_id_tab = path_ctrl_id,
		.pdaf_type3_reg_addr = DCAM_PPE_RIGHT_WADDR,
		.rds_en = 0,
		.dcam_raw_path_id = DCAM_PATH_FULL,
		.pyramid_support = 0,
		.fmcu_support = 0,
		.sensor_raw_fmt = DCAM_RAW_14,
		.save_band_for_bigsize = 0,
		.raw_fmt_support[0] = DCAM_RAW_14,
		.raw_fmt_support[1] = DCAM_RAW_8,
		.raw_fmt_support[2] = DCAM_RAW_HALFWORD_10,
		.raw_fmt_support[3] = DCAM_RAW_PACK_10,
		.dcam_output_support[0] = DCAM_STORE_10_BIT,
		.dcam_output_support[1] = DCAM_STORE_8_BIT,
		.recovery_support = 0,/*DCAMINT_INT0_FATAL_ERROR,*/
	},
};
static struct cam_hw_ip_info isp = {
	.slm_cfg_support = 1,
	.scaler_coeff_ex = 0,
	.scaler_bypass_ctrl = 0,
	.ctx_fmcu_support = isp_ctx_fmcu_support,
	.rgb_ltm_support = 1,
	.yuv_ltm_support = 0,
	.pyr_rec_support = 0,
	.pyr_dec_support = 0,
	.fbd_yuv_support = 0,
	.fbd_raw_support = 1,
	.rgb_gtm_support = 0,
	.dewarp_support = 0,
	.frbg_hist_support = 1,
	.nr3_mv_alg_version = ALG_NR3_MV_VER_0,
	.dyn_overlap_version = ALG_ISP_DYN_OVERLAP_NONE,
	.fetch_raw_support = 1,
	.nr3_compress_support = 0,
	.capture_thumb_support = 1,
	.thumb_scaler_cal_version = ISP_THUMB_SCL_VER_0,
};

struct cam_hw_info sharkl5pro_hw_info = {
	.prj_id = SHARKL5pro,
	.pdev = NULL,
	.soc_dcam = &dcam_soc_info,
	.soc_isp = &isp_soc_info,
	.soc_dcam_lite = NULL,
	.ip_dcam[DCAM_ID_0] = &dcam[DCAM_ID_0],
	.ip_dcam[DCAM_ID_1] = &dcam[DCAM_ID_1],
	.ip_dcam[DCAM_ID_2] = &dcam[DCAM_ID_2],
	.ip_isp = &isp,
	.dcam_ioctl = camhwif_dcam_ioctl,
	.isp_ioctl = camhwif_isp_ioctl,
	.cam_ioctl = camhwif_cam_ioctl,
	.csi_connect_type = DCAM_BIND_FIXED,
};
