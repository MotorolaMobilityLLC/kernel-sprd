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

#ifndef __SPRD_ACCESS_PROP_FD_H__
#define __SPRD_ACCESS_PROP_FD_H__

#include <linux/ioctl.h>

#define SPRD_FD_DEVICE_NAME "sprd_fd"

enum sprd_fd_reg_param {
    SPRD_FD_REG_PARAM_RUN,
    SPRD_FD_REG_PARAM_INFO,
    SPRD_FD_REG_PARAM_MOD_CFG,
    SPRD_FD_REG_PARAM_CMD_BADDR,
    SPRD_FD_REG_PARAM_CMD_NUM,
    SPRD_FD_REG_PARAM_IMAGE_BADDR,
    SPRD_FD_REG_PARAM_IMAGE_LINENUM,
    SPRD_FD_REG_PARAM_FACE_NUM,
    SPRD_FD_REG_PARAM_OUT_BUF_ADDR,
    SPRD_FD_REG_PARAM_DIM_BUF_ADDR,
    SPRD_FD_REG_PARAM_CROP_START_STS,
    SPRD_FD_REG_PARAM_CROP_SIZE_STS,
    SPRD_FD_REG_PARAM_DIM_STS,
    SPRD_FD_REG_PARAM_WKC_STS,
    SPRD_FD_REG_PARAM_PAD_STS,
    SPRD_FD_REG_PARAM_MODEL_CUT0_STS,
    SPRD_FD_REG_PARAM_MODEL_CUT1_STS,
    SPRD_FD_REG_PARAM_MODEL0_STS,
    SPRD_FD_REG_PARAM_MODEL1_STS,
    SPRD_FD_REG_PARAM_MODEL2_STS,
    SPRD_FD_REG_PARAM_MODEL3_STS,
    SPRD_FD_REG_PARAM_MODEL4_STS,
    SPRD_FD_REG_PARAM_MODEL5_STS,
    SPRD_FD_REG_PARAM_MODEL6_STS,
    SPRD_FD_REG_PARAM_MODEL7_STS,
    SPRD_FD_REG_PARAM_MODEL8_STS,
    SPRD_FD_REG_PARAM_MODEL9_STS,
    SPRD_FD_REG_PARAM_MODEL10_STS,
    SPRD_FD_REG_PARAM_MODEL11_STS,
    SPRD_FD_REG_PARAM_IP_REV,
    SPRD_FD_REG_PARAM_AXI_DEBUG,
    SPRD_FD_REG_PARAM_BUSY_DEBUG,
    SPRD_FD_REG_PARAM_DET_DEBUG0,
    SPRD_FD_REG_PARAM_DET_DEBUG1,
    SPRD_FD_REG_PARAM_SCL_DEBUG,
    SPRD_FD_REG_PARAM_FEAT_DEBUG0,
    SPRD_FD_REG_PARAM_FEAT_DEBUG1,
    SPRD_FD_REG_PARAM_SPARE_GATE,
    SPRD_FD_REG_PARAM_CFG_EN,
    SPRD_FD_REG_PARAM_CFG_BADDR,
    SPRD_FD_REG_PARAM_MODEL_01_FINE,
    SPRD_FD_REG_PARAM_MODEL_23_FINE,
    SPRD_FD_REG_PARAM_MODEL_45_FINE,
    SPRD_FD_REG_PARAM_MODEL_67_FINE,
    SPRD_FD_REG_PARAM_INT_EN,
    SPRD_FD_REG_PARAM_INT_CLR,
    SPRD_FD_REG_PARAM_INT_RAW,
    SPRD_FD_REG_PARAM_INT_MASK,
    SPRD_FD_REG_PARAM_MAX
};

enum sprd_fd_dvfs_index {
    SPRD_FD_DVFS_INDEX0,
    SPRD_FD_DVFS_INDEX1,
    SPRD_FD_DVFS_INDEX2,
    SPRD_FD_DVFS_INDEX3,
    SPRD_FD_DVFS_INDEX4,
    SPRD_FD_DVFS_INDEX5,
    SPRD_FD_DVFS_INDEX6,
    SPRD_FD_DVFS_INDEX7,
    SPRD_FD_DVFS_INDEX_MAX
};

enum sprd_fd_clk_freq {
    SPRD_FD_CLK_FREQ_76_8M,
    SPRD_FD_CLK_FREQ_128M,
    SPRD_FD_CLK_FREQ_256M,
    SPRD_FD_CLK_FREQ_384M,
    SPRD_FD_CLK_FREQ_INVALID
};

enum sprd_fd_iommu_status {
    SPRD_FD_IOMMU_ENABLED,
    SPRD_FD_IOMMU_DISABLED
};
/*structure for fd
 * reg_param--> send the addr param to be updated
 * reg_val--> value to the corresponding bit
 */

struct sprd_fd_cfg_param {
    unsigned int reg_param;
    unsigned int reg_val;
};

struct sprd_fd_multi_reg_cfg_param {
    struct sprd_fd_cfg_param *reg_info_tab;
    uint32_t size;
};

#define SPRD_FD_IO_MAGIC           'm'
#define SPRD_FD_IO_WRITE \
	_IOW(SPRD_FD_IO_MAGIC, 0, struct sprd_fd_cfg_param)
#define SPRD_FD_IO_WRITE_WITHBIT \
	_IOW(SPRD_FD_IO_MAGIC, 1, struct sprd_fd_cfg_param)
#define SPRD_FD_IO_MULTI_WRITE \
	_IOW(SPRD_FD_IO_MAGIC, 2, struct sprd_fd_multi_reg_cfg_param)
#define SPRD_FD_IO_READ \
	_IOR(SPRD_FD_IO_MAGIC, 3, struct sprd_fd_cfg_param)
#define SPRD_FD_IO_MULTI_READ \
	_IOR(SPRD_FD_IO_MAGIC, 4, struct sprd_fd_multi_reg_cfg_param)
#define SPRD_FD_IO_WORK_CLOCK_SEL\
	_IOR(SPRD_FD_IO_MAGIC, 5, unsigned int)
#define SPRD_FD_IO_IDLE_CLOCK_SEL \
	_IOR(SPRD_FD_IO_MAGIC, 6, unsigned int)
#define SPRD_FD_IO_GET_IOMMU_STATUS \
	_IOR(SPRD_FD_IO_MAGIC, 7, unsigned int)
#endif
