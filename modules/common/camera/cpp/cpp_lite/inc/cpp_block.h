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

#ifndef _CPP_BLOCK_H_
#define _CPP_BLOCK_H_

#include <linux/dma-buf.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/scatterlist.h>
#include <linux/sprd_iommu.h>
#include <linux/sprd_ion.h>
#include <linux/types.h>
#include <sprd_mm.h>

#include "sprd_cpp.h"

#define DMA_ADDR_ALIGN 0x07
#define ROT_ADDR_ALIGN 0x07

enum {
	ROT_ONE_BYTE = 0,
	ROT_TWO_BYTES,
	ROT_FOUR_BYTES,
	ROT_BYTE_MAX
};

enum {
	ROT_UV420 = 0,
	ROT_UV422,
	ROT_DATA_FORMAT_MAX
};

struct cpp_iommu_info {
	struct device *dev;
	unsigned int mfd[3];
	struct sg_table *table;
	void *buf;
	size_t size;
	unsigned long iova[3];
	struct dma_buf *dmabuf_p;
	unsigned int offset[3];
	struct dma_buf_attachment *attachment;
};

struct rot_drv_private {
	struct sprd_cpp_rot_cfg_parm cfg_parm;
	spinlock_t *hw_lock;
	unsigned int rot_fmt;
	unsigned int uv_mode;
	unsigned int rot_mode;
	unsigned int rot_src_addr;
	unsigned int rot_dst_addr;
	struct sprd_cpp_size rot_size;
	unsigned int rot_endian;

	struct cpp_iommu_info iommu_src;
	struct cpp_iommu_info iommu_dst;
	void __iomem *io_base;

	void *priv;
};

struct dma_drv_private {
	struct sprd_cpp_dma_cfg_parm cfg_parm;
	spinlock_t *hw_lock;
	unsigned int dma_src_addr;
	unsigned int dma_dst_addr;
	struct cpp_iommu_info iommu_src;
	struct cpp_iommu_info iommu_dst;
	void __iomem *io_base;
	void *priv;
};

struct scale_drv_private {
	struct sprd_cpp_scale_cfg_parm cfg_parm;
	unsigned int input_fmt;
	unsigned int input_endian;
	unsigned int input_uv_endian;
	unsigned int output_endian;
	unsigned int output_uv_endian;
	unsigned int rch_burst_gap;
	unsigned int wch_burst_gap;
	unsigned int src_pitch;
	struct sprd_cpp_rect src_rect;
	unsigned int ver_deci;
	unsigned int hor_deci;
	/* scaler path */
	struct sprd_cpp_size sc_intrim_src_size;
	struct sprd_cpp_rect sc_intrim_rect;
	struct sprd_cpp_size sc_slice_in_size;
	struct sprd_cpp_size sc_slice_out_size;
	struct sprd_cpp_size sc_full_in_size;
	struct sprd_cpp_size sc_full_out_size;
	struct sprd_cpp_size sc_out_trim_src_size;
	struct sprd_cpp_rect sc_outtrim_rect;
	unsigned int y_hor_ini_phase_int;
	unsigned int y_hor_ini_phase_frac;
	unsigned int uv_hor_ini_phase_int;
	unsigned int uv_hor_ini_phase_frac;
	unsigned int y_ver_ini_phase_int;
	unsigned int y_ver_ini_phase_frac;
	unsigned int uv_ver_ini_phase_int;
	unsigned int uv_ver_ini_phase_frac;
	unsigned int y_ver_tap;
	unsigned int uv_ver_tap;
	unsigned int sc_des_pitch;
	unsigned int sc_deci_val;
	struct sprd_cpp_rect sc_des_rect;
	unsigned int sc_output_fmt;
	/* bypass path */
	unsigned int bp_en;
	struct sprd_cpp_size bp_trim_src_size;
	struct sprd_cpp_rect bp_trim_rect;
	unsigned int bp_des_pitch;
	struct sprd_cpp_rect bp_des_rect;
	unsigned int bp_output_fmt;
	spinlock_t *hw_lock;
	void *priv;
	struct cpp_iommu_info iommu_src;
	struct cpp_iommu_info iommu_dst;/* sc des*/
	struct cpp_iommu_info iommu_dst_bp;
	unsigned long coeff_addr_offset;
	int coeff_arg;
	void __iomem *io_base;
	struct platform_device *pdev;
};

int cpp_k_rot_dev_enable(void *arg);
int cpp_k_rot_dev_disable(void *arg);
int cpp_k_rot_dev_start(void *arg);
int cpp_k_rot_dev_stop(void *arg);
int cpp_k_rot_parm_set(void *arg);
int cpp_k_rot_reg_trace(void *arg);

int cpp_k_scale_dev_start(void *arg);
int cpp_k_scale_des_pitch_set(void *arg);
int cpp_k_scale_input_rect_set(void *arg);
int cpp_k_scale_output_rect_set(void *arg);
int cpp_k_scale_input_format_set(void *arg);
int cpp_k_scale_output_format_set(void *arg);
int cpp_k_scale_deci_set(void *arg);
int cpp_k_scale_input_endian_set(void *arg);
int cpp_k_scale_output_endian_set(void *arg);
int cpp_k_scale_burst_gap_set(void *arg);
int cpp_k_scale_bpen_set(void *arg);
int cpp_k_scale_ini_phase_set(void *arg);
int cpp_k_scale_tap_set(void *arg);
int cpp_k_scale_offset_size_set(void *arg);
int cpp_k_scale_addr_set(void *arg);
int cpp_k_scale_dev_disable(void *arg);
int cpp_k_scale_src_pitch_set(void *arg);
int cpp_k_scale_dev_enable(void *arg);
int cpp_k_scale_dev_stop(void *arg);
int cpp_k_scale_reg_trace(void *arg);
int cpp_k_scale_clk_switch(void *arg);
int cpp_k_scale_luma_hcoeff_set(void *arg);
int cpp_k_scale_chrima_hcoeff_set(void *arg);
int cpp_k_scale_vcoeff_set(void *arg);
int cpp_k_dma_dev_enable(void *arg);
int cpp_k_dma_dev_disable(void *arg);
int cpp_k_dma_dev_start(void *arg);
int cpp_k_dma_dev_stop(void *arg);
int cpp_k_dma_dev_cfg(void *arg);
#endif
