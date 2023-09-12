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
#include "dcam_int.h"
#include "dcam_path.h"
#include "dcam_hw_adpt.h"

#include "isp_reg.h"
#include "isp_hw_adpt.h"
#include "isp_core.h"
#include "isp_slice.h"
#include "isp_cfg.h"
#include "isp_path.h"
#include "isp_fmcu.h"
#include "isp_dec_int.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CAM_HW_IF_N6PRO: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

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

	soc_isp->mtx_en = of_clk_get_by_name(isp_node, "isp_mtx_en");
	if (IS_ERR_OR_NULL(soc_isp->mtx_en)) {
		pr_err("fail to read dts isp mtx_en\n");
		return -EFAULT;
	}

	soc_isp->tck_en = of_clk_get_by_name(isp_node, "isp_tck_en");
	if (IS_ERR_OR_NULL(soc_isp->tck_en)) {
		pr_err("fail to read dts isp tck_en\n");
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
	ret |= IS_ERR_OR_NULL(soc_dcam->core_eb);

	soc_dcam->clk = of_clk_get_by_name(dn, "dcam_clk");
	ret |= IS_ERR_OR_NULL(soc_dcam->clk);

	soc_dcam->clk_parent = of_clk_get_by_name(dn, "dcam_clk_parent");
	ret |= IS_ERR_OR_NULL(soc_dcam->clk_parent);
	soc_dcam->clk_default = clk_get_parent(soc_dcam->clk);

	soc_dcam->mtx_en = of_clk_get_by_name(dn, "dcam_mtx_en");
	ret |= IS_ERR_OR_NULL(soc_dcam->mtx_en);

	soc_dcam->tck_en = of_clk_get_by_name(dn, "dcam_tck_en");
	ret |= IS_ERR_OR_NULL(soc_dcam->tck_en);

	soc_dcam->blk_cfg_en = of_clk_get_by_name(dn, "dcam_blk_cfg_en");
	ret |= IS_ERR_OR_NULL(soc_dcam->blk_cfg_en);

	soc_dcam->axi_clk = of_clk_get_by_name(dn, "dcam_axi_clk");
	ret |= IS_ERR_OR_NULL(soc_dcam->axi_clk);
	soc_dcam->axi_clk_parent = of_clk_get_by_name(dn, "dcam_axi_clk_parent");
	ret |= IS_ERR_OR_NULL(soc_dcam->axi_clk_parent);
	soc_dcam->axi_clk_default = clk_get_parent(soc_dcam->axi_clk);

	soc_dcam->mtx_clk = of_clk_get_by_name(dn, "dcam_mtx_clk");
	ret |= IS_ERR_OR_NULL(soc_dcam->mtx_clk);
	soc_dcam->mtx_clk_parent = of_clk_get_by_name(dn, "dcam_mtx_clk_parent");
	ret |= IS_ERR_OR_NULL(soc_dcam->mtx_clk_parent);
	soc_dcam->mtx_clk_default = clk_get_parent(soc_dcam->mtx_clk);

	soc_dcam->blk_cfg_clk = of_clk_get_by_name(dn, "dcam_blk_cfg_clk");
	ret |= IS_ERR_OR_NULL(soc_dcam->blk_cfg_clk);
	soc_dcam->blk_cfg_clk_parent = of_clk_get_by_name(dn, "dcam_blk_cfg_clk_parent");
	ret |= IS_ERR_OR_NULL(soc_dcam->blk_cfg_clk_parent);
	soc_dcam->blk_cfg_clk_default = clk_get_parent(soc_dcam->blk_cfg_clk);

	if (ret)
		pr_err("fail to read clk\n");
	return ret;
}

static int camhw_get_all_rst(void *handle, void *arg)
{
	int ret = 0;
	struct cam_hw_info *hw = NULL;
	uint32_t args[2] = {0};
	struct device_node *dn = (struct device_node *)arg;
	struct cam_hw_ip_info *dcam_info = NULL;

	if (!handle) {
		pr_err("fail to get invalid handle\n");
		return -EINVAL;
	}

	hw = (struct cam_hw_info *)handle;

	ret = cam_syscon_get_args_by_name(dn, "dcam01_axi_reset", ARRAY_SIZE(args), args);
	if (ret) {
		pr_err("fail to get dcam axi reset syscon\n");
		return -EINVAL;
	}

	dcam_info = hw->ip_dcam[0];
	dcam_info->syscon.axi_rst_mask= args[1];
	dcam_info = hw->ip_dcam[1];
	dcam_info->syscon.axi_rst_mask= args[1];

	ret = cam_syscon_get_args_by_name(dn, "dcam01_all_reset", ARRAY_SIZE(args), args);
	if (ret) {
		pr_err("fail to get dcam all reset syscon\n");
		return -EINVAL;
	}

	dcam_info = hw->ip_dcam[0];
	dcam_info->syscon.all_rst = args[0];
	dcam_info->syscon.all_rst_mask= args[1];
	dcam_info = hw->ip_dcam[1];
	dcam_info->syscon.all_rst = args[0];
	dcam_info->syscon.all_rst_mask= args[1];

	return ret;
}

static int camhw_get_axi_base(void *handle, void *arg)
{
	struct cam_hw_info *hw = NULL;
	struct device_node *dn = (struct device_node *)arg;
	int pos = 0;
	uint32_t count = 0;
	struct resource reg_res = {0};
	void __iomem *reg_base = NULL;
	struct cam_hw_soc_info *soc_dcam;
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

	g_dcam_aximbase[0] = (unsigned long)reg_base;
	g_dcam_aximbase[1] = (unsigned long)reg_base;
	soc_dcam->axi_reg_base = (unsigned long)reg_base;

	pos = count + 1;
	if (of_address_to_resource(dn, pos, &reg_res)) {
		pr_err("fail to get FMCU phy addr\n");
		goto err_axi_iounmap;
	}

	reg_base = ioremap(reg_res.start, reg_res.end - reg_res.start + 1);
	if (!reg_base) {
		pr_err("fail to map FMCU reg base\n");
		goto err_fmcu_iounmap;
	}
	g_dcam_fmcubase = (unsigned long)reg_base;

	return 0;
err_fmcu_iounmap:
	g_dcam_fmcubase = 0;
	if (pos == (count + 2))
		iounmap((void __iomem *)g_dcam_fmcubase);
err_axi_iounmap:
	if (pos == (count + 1))
		iounmap((void __iomem *)g_dcam_aximbase[0]);
	g_dcam_aximbase[0] = 0;
	g_dcam_aximbase[1] = 0;
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
		DCAM_STORE0_SLICE_Y_ADDR,
		DCAM_STORE0_SLICE_Y_ADDR,
		DCAM_STORE0_SLICE_Y_ADDR,
		DCAM_STORE0_SLICE_Y_ADDR
	},
	{
		DCAM_AEM_BASE_WADDR,
		DCAM_AEM_BASE_WADDR,
		DCAM_AEM_BASE_WADDR,
		DCAM_AEM_BASE_WADDR
	},
	{
		DCAM_BAYER_HIST_BASE_WADDR,
		DCAM_BAYER_HIST_BASE_WADDR,
		DCAM_BAYER_HIST_BASE_WADDR,
		DCAM_BAYER_HIST_BASE_WADDR
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
	[DCAM_PATH_FRGB_HIST] = DCAM_CTRL_COEF,
	[DCAM_PATH_3DNR] = DCAM_CTRL_COEF,
	[DCAM_PATH_BPC] = DCAM_CTRL_COEF,
	[DCAM_PATH_LSCM] = DCAM_CTRL_COEF,
};

static unsigned long dcam_store_addr[DCAM_PATH_MAX] = {
	[DCAM_PATH_FULL] = DCAM_STORE4_SLICE_Y_ADDR,
	[DCAM_PATH_BIN] = DCAM_STORE0_SLICE_Y_ADDR,
	[DCAM_PATH_RAW] = DCAM_RAW_PATH_BASE_WADDR,
	[DCAM_PATH_PDAF] = DCAM_PDAF_BASE_WADDR,
	[DCAM_PATH_VCH2] = DCAM_VCH2_BASE_WADDR,
	[DCAM_PATH_VCH3] = DCAM_VCH3_BASE_WADDR,
	[DCAM_PATH_AEM] = DCAM_AEM_BASE_WADDR,
	[DCAM_PATH_AFM] = DCAM_AFM_LUM_FV_BASE_WADDR,
	[DCAM_PATH_AFL] = ISP_AFL_DDR_INIT_ADDR,
	[DCAM_PATH_HIST] = DCAM_BAYER_HIST_BASE_WADDR,
	[DCAM_PATH_FRGB_HIST] = DCAM_HIST_ROI_BASE_WADDR,
	[DCAM_PATH_3DNR] = DCAM_NR3_WADDR,
	[DCAM_PATH_BPC] = DCAM_BPC_OUT_ADDR,
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
static struct cam_hw_soc_info dcam_lite_soc_info;
static struct cam_hw_soc_info isp_soc_info;
static struct cam_hw_ip_info dcam[DCAM_ID_MAX] = {
	[DCAM_ID_0] = {
		.aux_dcam_path = DCAM_PATH_BIN,
		.slm_path = BIT(DCAM_PATH_BIN) | BIT(DCAM_PATH_AEM) | BIT(DCAM_PATH_HIST),
		.lbuf_share_support = 1,
		.offline_slice_support = 1,
		.afl_gbuf_size = STATIS_AFL_GBUF_SIZE,
		.superzoom_support = 1,
		.dcam_full_fbc_mode = DCAM_FBC_FULL_10_BIT,
		.dcam_bin_fbc_mode = DCAM_FBC_BIN_10_BIT,
		.dcam_raw_fbc_mode = DCAM_FBC_DISABLE,
		.dcam_offline_fbc_mode = DCAM_FBC_DISABLE,
		.store_addr_tab = dcam_store_addr,
		.path_ctrl_id_tab = path_ctrl_id,
		.pdaf_type3_reg_addr = DCAM_PPE_RIGHT_WADDR,
		.rds_en = 0,
		.dcam_raw_path_id = DCAM_PATH_RAW,
		.pyramid_support = 1,
		.fmcu_support = 1,
		.sensor_raw_fmt = DCAM_RAW_14,
		.save_band_for_bigsize = 1,
		.raw_fmt_support[0] = DCAM_RAW_14,
		.raw_fmt_support[1] = DCAM_RAW_8,
		.raw_fmt_support[2] = DCAM_RAW_HALFWORD_10,
		.raw_fmt_support[3] = DCAM_RAW_PACK_10,
		.dcam_zoom_mode = ZOOM_SCALER,
		.dcam_output_support[0] = DCAM_STORE_10_BIT,
		.dcam_output_support[1] = DCAM_STORE_8_BIT,
		.recovery_support = 0,
	},
	[DCAM_ID_1] = {
		.aux_dcam_path = DCAM_PATH_BIN,
		.slm_path = BIT(DCAM_PATH_BIN) | BIT(DCAM_PATH_AEM) | BIT(DCAM_PATH_HIST),
		.lbuf_share_support = 1,
		.offline_slice_support = 1,
		.afl_gbuf_size = STATIS_AFL_GBUF_SIZE,
		.superzoom_support = 1,
		.dcam_full_fbc_mode = DCAM_FBC_FULL_10_BIT,
		.dcam_bin_fbc_mode = DCAM_FBC_BIN_10_BIT,
		.dcam_raw_fbc_mode = DCAM_FBC_DISABLE,
		.dcam_offline_fbc_mode = DCAM_FBC_DISABLE,
		.store_addr_tab = dcam_store_addr,
		.path_ctrl_id_tab = path_ctrl_id,
		.pdaf_type3_reg_addr = DCAM_PPE_RIGHT_WADDR,
		.rds_en = 0,
		.dcam_raw_path_id = DCAM_PATH_RAW,
		.pyramid_support = 1,
		.fmcu_support = 1,
		.sensor_raw_fmt = DCAM_RAW_14,
		.save_band_for_bigsize = 1,
		.raw_fmt_support[0] = DCAM_RAW_14,
		.raw_fmt_support[1] = DCAM_RAW_8,
		.raw_fmt_support[2] = DCAM_RAW_HALFWORD_10,
		.raw_fmt_support[3] = DCAM_RAW_PACK_10,
		.dcam_zoom_mode = ZOOM_SCALER,
		.dcam_output_support[0] = DCAM_STORE_10_BIT,
		.dcam_output_support[1] = DCAM_STORE_8_BIT,
		.recovery_support = 0,
	},
	[DCAM_ID_2] = {
		.aux_dcam_path = DCAM_PATH_FULL,
		.slm_path = BIT(DCAM_PATH_BIN),
		.lbuf_share_support = 0,
		.offline_slice_support = 0,
		.afl_gbuf_size = 0,
		.superzoom_support = 1,
		.dcam_full_fbc_mode = DCAM_FBC_DISABLE,
		.dcam_bin_fbc_mode = DCAM_FBC_DISABLE,
		.dcam_offline_fbc_mode = DCAM_FBC_DISABLE,
		.store_addr_tab = dcam_store_addr,
		.path_ctrl_id_tab = path_ctrl_id,
		.pdaf_type3_reg_addr = 0,
		.rds_en = 0,
		.dcam_raw_path_id = DCAM_PATH_FULL,
		.pyramid_support = 0,
		.fmcu_support = 0,
		.sensor_raw_fmt = DCAM_RAW_PACK_10,
		.save_band_for_bigsize = 0,
		.raw_fmt_support[0] = DCAM_RAW_PACK_10,
		.raw_fmt_support[1] = DCAM_RAW_HALFWORD_10,
		.raw_fmt_support[2] = DCAM_RAW_MAX,
		.dcam_output_support[0] = DCAM_STORE_10_BIT,
		.dcam_output_support[1] = DCAM_STORE_8_BIT,
		.recovery_support = 0,
	},
	[DCAM_ID_3] = {
		.aux_dcam_path = DCAM_PATH_FULL,
		.slm_path = BIT(DCAM_PATH_BIN),
		.lbuf_share_support = 0,
		.offline_slice_support = 0,
		.afl_gbuf_size = 0,
		.superzoom_support = 1,
		.dcam_full_fbc_mode = DCAM_FBC_DISABLE,
		.dcam_bin_fbc_mode = DCAM_FBC_DISABLE,
		.dcam_offline_fbc_mode = DCAM_FBC_DISABLE,
		.store_addr_tab = dcam_store_addr,
		.path_ctrl_id_tab = path_ctrl_id,
		.pdaf_type3_reg_addr = 0,
		.rds_en = 0,
		.dcam_raw_path_id = DCAM_PATH_FULL,
		.pyramid_support = 0,
		.fmcu_support = 0,
		.sensor_raw_fmt = DCAM_RAW_PACK_10,
		.save_band_for_bigsize = 0,
		.raw_fmt_support[0] = DCAM_RAW_PACK_10,
		.raw_fmt_support[1] = DCAM_RAW_HALFWORD_10,
		.raw_fmt_support[2] = DCAM_RAW_MAX,
		.dcam_output_support[0] = DCAM_STORE_10_BIT,
		.dcam_output_support[1] = DCAM_STORE_8_BIT,
		.recovery_support = 0,
	},
};
static struct cam_hw_ip_info isp = {
	.slm_cfg_support = 1,
	.scaler_coeff_ex = 1,
	.scaler_bypass_ctrl = 0,
	.ctx_fmcu_support = isp_ctx_fmcu_support,
	.rgb_ltm_support = 1,
	.yuv_ltm_support = 0,
	.pyr_rec_support = 1,
	.pyr_dec_support = 1,
	.fbd_yuv_support = 1,
	.fbd_raw_support = 0,
	.rgb_gtm_support = 0,
	.dewarp_support = 0,
	.frbg_hist_support = 0,
	.nr3_mv_alg_version = ALG_NR3_MV_VER_1,
	.dyn_overlap_version = ALG_ISP_OVERLAP_VER_2,
	.fetch_raw_support = 0,
	.nr3_compress_support = 0,
	.capture_thumb_support = 1,
	.thumb_scaler_cal_version = ISP_THUMB_SCL_VER_1,
};

struct cam_hw_info qogirn6pro_hw_info = {
	.prj_id = QOGIRN6pro,
	.pdev = NULL,
	.soc_dcam = &dcam_soc_info,
	.soc_dcam_lite = &dcam_lite_soc_info,
	.soc_isp = &isp_soc_info,
	.ip_dcam[DCAM_ID_0] = &dcam[DCAM_ID_0],
	.ip_dcam[DCAM_ID_1] = &dcam[DCAM_ID_1],
	.ip_dcam[DCAM_ID_2] = &dcam[DCAM_ID_2],
	.ip_dcam[DCAM_ID_3] = &dcam[DCAM_ID_3],
	.ip_isp = &isp,
	.dcam_ioctl = camhwif_dcam_ioctl,
	.isp_ioctl = camhwif_isp_ioctl,
	.cam_ioctl = camhwif_cam_ioctl,
	.csi_connect_type = DCAM_BIND_DYNAMIC,
};

