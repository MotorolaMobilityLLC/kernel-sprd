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

/* #include "mm_dvfs_reg.h" */
#include "mmsys_dvfs_comm.h"
#include "mmsys_dvfs.h"
#include "mm_dvfs_table.h"

u8 mm_power_flag = 0;

static int get_ip_status(struct devfreq *devfreq,
                         struct ip_dvfs_status *ip_status) {
    u32 volt_reg = 0, ip_clk = 0;
    u32 clk_reg = 0, ip_volt = 0;
    u32 i = 0;
    int ret = 0;
    pr_info("mm_power_flag %d", mm_power_flag);
    if (mm_power_flag != 1)
        return -1;
    volt_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_MM_DVFS_VOLTAGE_DBG);
    clk_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_MM_DVFS_CGM_CFG_DBG);
    // top
    ret = regmap_read(g_mmreg_map.mmdvfs_top_regmap, REG_TOP_DVFS_APB_DCDC_MM_DVFS_STATE_DBG, &ip_volt);
    if (ret != 0) {
        pr_err("%s, regmap_read fail!\n", __func__);
        return 0;
    }
    ip_volt = (ip_volt >> 20) & 0x07;

    for (i = 0; i < 8; i++) {
        if (ip_volt == isp_dvfs_config_table[i].volt) {
            ip_status->top_volt = isp_dvfs_config_table[i].volt_value;
            break;
        }
    }
    // mm
    ip_volt = (volt_reg >> SHFT_BITS_MM_CURRENT_VOLTAGE) &
              MASK_BITS_MM_CURRENT_VOLTAGE;

    for (i = 0; i < 8; i++) {
        if (ip_volt == isp_dvfs_config_table[i].volt) {
            ip_status->mm_vote_volt = isp_dvfs_config_table[i].volt_value;
            break;
        }
    }

    // isp
    ip_clk =
        (clk_reg >> SHFT_BITS_CGM_ISP_SEL_DVFS) & MASK_BITS_CGM_ISP_SEL_DVFS;
    ip_volt = ((volt_reg >> SHFT_BITS_ISP_VOLTAGE) & MASK_BITS_ISP_VOLTAGE);
    for (i = 0; i < 8; i++) {
        if (ip_clk == isp_dvfs_config_table[i].clk) {
            ip_status->isp_clk = isp_dvfs_config_table[i].clk_freq;
            break;
        }
    }
    for (i = 0; i < 8; i++) {
        if (ip_volt == isp_dvfs_config_table[i].volt) {
            ip_status->isp_vote_volt = isp_dvfs_config_table[i].volt_value;
            break;
        }
    }

    // cpp
    ip_clk =
        (clk_reg >> SHFT_BITS_CGM_CPP_SEL_DVFS) & MASK_BITS_CGM_CPP_SEL_DVFS;
    ip_volt = ((volt_reg >> SHFT_BITS_CPP_VOLTAGE) & MASK_BITS_CPP_VOLTAGE);
    for (i = 0; i < 8; i++) {
        if (ip_clk == cpp_dvfs_config_table[i].clk) {
            ip_status->cpp_clk = cpp_dvfs_config_table[i].clk_freq;
            break;
        }
    }
    for (i = 0; i < 8; i++) {
        if (ip_volt == cpp_dvfs_config_table[i].volt) {
            ip_status->cpp_vote_volt = cpp_dvfs_config_table[i].volt_value;
            break;
        }
    }
    // fd
    ip_clk = (clk_reg >> SHFT_BITS_CGM_FD_SEL_DVFS) & MASK_BITS_CGM_FD_SEL_DVFS;
    ip_volt = ((volt_reg >> SHFT_BITS_FD_VOLTAGE) & MASK_BITS_FD_VOLTAGE);
    for (i = 0; i < 8; i++) {
        if (ip_clk == fd_dvfs_config_table[i].clk) {
            ip_status->fd_clk = fd_dvfs_config_table[i].clk_freq;
            break;
        }
    }
    for (i = 0; i < 8; i++) {
        if (ip_volt == fd_dvfs_config_table[i].volt) {
            ip_status->fd_vote_volt = fd_dvfs_config_table[i].volt_value;
            break;
        }
    }

    // jpeg

    ip_clk =
        (clk_reg >> SHFT_BITS_CGM_JPG_SEL_DVFS) & MASK_BITS_CGM_JPG_SEL_DVFS;
    ip_volt = ((volt_reg >> SHFT_BITS_JPG_VOLTAGE) & MASK_BITS_JPG_VOLTAGE);
    for (i = 0; i < 8; i++) {
        if (ip_clk == jpg_dvfs_config_table[i].clk) {
            ip_status->jpg_clk = jpg_dvfs_config_table[i].clk_freq;
            break;
        }
    }
    for (i = 0; i < 8; i++) {
        if (ip_volt == jpg_dvfs_config_table[i].volt) {
            ip_status->jpg_vote_volt = jpg_dvfs_config_table[i].volt_value;
            break;
        }
    }
    // dcam_axi
    ip_clk = (clk_reg >> SHFT_BITS_CGM_DCAM_AXI_SEL_DVFS) &
             MASK_BITS_CGM_DCAM_AXI_SEL_DVFS;
    ip_volt =
        ((volt_reg >> SHFT_BITS_DCAM_AXI_VOLTAGE) & MASK_BITS_DCAM_AXI_VOLTAGE);
    for (i = 0; i < 8; i++) {
        if (ip_clk == dcamaxi_dvfs_config_table[i].clk) {
            ip_status->dcam_axi_clk = dcamaxi_dvfs_config_table[i].clk_freq;
            break;
        }
    }
    for (i = 0; i < 8; i++) {
        if (ip_volt == dcamaxi_dvfs_config_table[i].volt) {
            ip_status->dcam_axi_vote_volt =
                dcamaxi_dvfs_config_table[i].volt_value;
            break;
        }
    }
    // dcam_if
    ip_clk = (clk_reg >> SHFT_BITS_CGM_DCAM_IF_SEL_DVFS) &
             MASK_BITS_CGM_DCAM_IF_SEL_DVFS;
    ip_volt =
        ((volt_reg >> SHFT_BITS_DCAM_IF_VOLTAGE) & MASK_BITS_DCAM_IF_VOLTAGE);
    for (i = 0; i < 8; i++) {
        if (ip_clk == dcam_dvfs_config_table[i].clk) {
            ip_status->dcam_if_clk = dcam_dvfs_config_table[i].clk_freq;
            break;
        }
    }
    for (i = 0; i < 8; i++) {
        if (ip_volt == dcam_dvfs_config_table[i].volt) {
            ip_status->dcam_if_vote_volt = dcam_dvfs_config_table[i].volt_value;
            break;
        }
    }
    // mtx
    ip_clk = (clk_reg >> SHFT_BITS_CGM_MM_MTX_SEL_DVFS) &
             MASK_BITS_CGM_MM_MTX_SEL_DVFS;
    ;
    ip_volt =
        ((volt_reg >> SHFT_BITS_MM_MTX_VOLTAGE) & MASK_BITS_MM_MTX_VOLTAGE);
    for (i = 0; i < 8; i++) {
        if (ip_clk == mtx_dvfs_config_table[i].clk) {
            ip_status->mtx_clk = mtx_dvfs_config_table[i].clk_freq;
            break;
        }
    }
    for (i = 0; i < 8; i++) {
        if (ip_volt == mtx_dvfs_config_table[i].volt) {
            ip_status->mtx_vote_volt = mtx_dvfs_config_table[i].volt_value;
            break;
        }
    }
    return 1;
}

static void mm_dvfs_auto_gate_sel(u32 on) {
    u32 reg_temp = 0;

    pr_info("dvfs ops: %s , %d\n", __func__, on);

    mutex_lock(&mmsys_glob_reg_lock);
    reg_temp = DVFS_REG_RD(REG_MM_DVFS_AHB_CGM_MM_DVFS_CLK_GATE_CTRL);
    if (on)
        DVFS_REG_WR(REG_MM_DVFS_AHB_CGM_MM_DVFS_CLK_GATE_CTRL,
                    (reg_temp | BIT_CGM_MM_DVFS_AUTO_GATE_SEL));
    else
        DVFS_REG_WR(REG_MM_DVFS_AHB_CGM_MM_DVFS_CLK_GATE_CTRL,
                    reg_temp & (~BIT_CGM_MM_DVFS_AUTO_GATE_SEL));

    mutex_unlock(&mmsys_glob_reg_lock);
}

static void mm_dvfs_force_en(u32 on) {
    u32 reg_temp = 0;

    mutex_lock(&mmsys_glob_reg_lock);

    reg_temp = DVFS_REG_RD(REG_MM_DVFS_AHB_CGM_MM_DVFS_CLK_GATE_CTRL);

    if (on)
        DVFS_REG_WR(REG_MM_DVFS_AHB_CGM_MM_DVFS_CLK_GATE_CTRL,
                    reg_temp | BIT_CGM_MM_DVFS_FORCE_EN);
    else
        DVFS_REG_WR(REG_MM_DVFS_AHB_CGM_MM_DVFS_CLK_GATE_CTRL,
                    reg_temp & (~BIT_CGM_MM_DVFS_FORCE_EN));

    mutex_unlock(&mmsys_glob_reg_lock);
}

void mm_sw_dvfs_onoff(u32 on) {


    pr_debug("dvfs ops: %s , %d\n", __func__, on);

    mutex_lock(&mmsys_glob_reg_lock);


    if (on)
        regmap_update_bits(g_mmreg_map.mmdvfs_top_regmap, REG_TOP_DVFS_APB_SUBSYS_SW_DVFS_EN_CFG,
                        BIT_MM_SYS_SW_DVFS_EN, BIT_MM_SYS_SW_DVFS_EN);
    else
        regmap_update_bits(g_mmreg_map.mmdvfs_top_regmap, REG_TOP_DVFS_APB_SUBSYS_SW_DVFS_EN_CFG,
                        BIT_MM_SYS_SW_DVFS_EN, 0);
    mutex_unlock(&mmsys_glob_reg_lock);
}

void mmdcdc_sw_dvfs_onoff(u32 on) {


    pr_debug("dvfs ops: %s , %d\n", __func__, on);

    mutex_lock(&mmsys_glob_reg_lock);


    if (on)
        regmap_update_bits(g_mmreg_map.mmdvfs_top_regmap, REG_TOP_DVFS_APB_DCDC_MM_SW_DVFS_CTRL,
                        BIT_DCDC_MM_SW_TUNE_EN, BIT_DCDC_MM_SW_TUNE_EN);
    else
        regmap_update_bits(g_mmreg_map.mmdvfs_top_regmap, REG_TOP_DVFS_APB_DCDC_MM_SW_DVFS_CTRL,
                        BIT_DCDC_MM_SW_TUNE_EN, 0);
    mutex_unlock(&mmsys_glob_reg_lock);
}

void mmsys_sw_cgb_enable(u32 on) {

    u32 temp_reg = 0;
    pr_debug("dvfs ops: %s , %d\n", __func__, on);

    mutex_lock(&mmsys_glob_reg_lock);

    temp_reg = DVFS_REG_RD_AHB(REG_MM_AHB_AHB_EB);
    if (on)
        DVFS_REG_WR_AHB(REG_MM_AHB_AHB_EB, temp_reg | BIT_MM_CKG_EB);
    else
        DVFS_REG_WR_AHB(REG_MM_AHB_AHB_EB, temp_reg & (~BIT_MM_CKG_EB));

    mutex_unlock(&mmsys_glob_reg_lock);
}

void mm_dvfs_hold_onoff(u32 on) {
    u32 holdon_reg = 0;
    pr_info("dvfs ops: %s , %d\n", __func__, on);

    mutex_lock(&mmsys_glob_reg_lock);

    holdon_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_MM_DVFS_HOLD_CTRL);
    if (on)
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_DVFS_HOLD_CTRL,
                    holdon_reg | BIT_MM_DVFS_HOLD);
    else
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_DVFS_HOLD_CTRL,
                    holdon_reg & (~BIT_MM_DVFS_HOLD));
    mutex_unlock(&mmsys_glob_reg_lock);
}
/* userspace  interface*/
static int set_mmsys_sw_dvfs_en(struct devfreq *devfreq,
                                unsigned int sw_dvfs_eb) {
    mm_sw_dvfs_onoff(sw_dvfs_eb);
    pr_info("dvfs ops: %s\n", __func__);
    return MM_DVFS_SUCCESS;
}

static int set_mmsys_dvfs_hold_en(struct devfreq *devfreq,
                                  unsigned int hold_en) {
    u32 sw_ctrl_reg = 0;

    pr_info("dvfs ops: %s , %d\n", __func__, hold_en);

    sw_ctrl_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_MM_DVFS_HOLD_CTRL);
    if (hold_en)
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_DVFS_HOLD_CTRL,
                    sw_ctrl_reg | BIT_MM_DVFS_HOLD);
    else
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_DVFS_HOLD_CTRL,
                    sw_ctrl_reg & (~BIT_MM_DVFS_HOLD));

    pr_info("dvfs ops: %s\n", __func__);
    return MM_DVFS_SUCCESS;
}

static int set_mmsys_dvfs_clk_gate_ctrl(struct devfreq *devfreq,
                                        unsigned int clk_gate) {

    mm_dvfs_auto_gate_sel(clk_gate);

    pr_info("dvfs ops: %s\n", __func__);
    return MM_DVFS_SUCCESS;
}

static int set_mmsys_dvfs_wait_window(struct devfreq *devfreq,
                                      unsigned int wait_window) {

    DVFS_REG_WR(REG_MM_DVFS_AHB_MM_DVFS_WAIT_WINDOW_CFG, wait_window);

    pr_info("dvfs ops: %s\n", __func__);
    return MM_DVFS_SUCCESS;
}

static int set_mmsys_dvfs_min_volt(struct devfreq *devfreq,
                                   unsigned int min_volt) {

    u32 reg_temp = 0;

    reg_temp = DVFS_REG_RD(REG_MM_DVFS_AHB_MM_MIN_VOLTAGE_CFG);

    DVFS_REG_WR(REG_MM_DVFS_AHB_MM_MIN_VOLTAGE_CFG,
                reg_temp | (min_volt & 0x7));

    pr_info("dvfs ops: %s\n", __func__);
    return MM_DVFS_SUCCESS;
}

static int mmsys_init(struct devfreq *devfreq) {

    struct mmsys_dvfs *mmsys;

    mmsys = dev_get_drvdata(devfreq->dev.parent);

    pr_info("mmsys_init: Top SW is %d MM SW is %d ", TOP_SW_DVFS_ENABLE, MM_SW_DVFS_ENABLE);
    /* sys clk enable */
    mmsys->mmsys_dvfs_para.sys_sw_cgb_enable = 1;
    mmsys_sw_cgb_enable(mmsys->mmsys_dvfs_para.sys_sw_cgb_enable);

    /* mm hw dvfs */
    mmdcdc_sw_dvfs_onoff(TOP_SW_DVFS_ENABLE);
    mm_sw_dvfs_onoff(MM_SW_DVFS_ENABLE);
    mm_dvfs_force_en(1);
// mm_sw_dvfs_onoff(mmsys->mmsys_dvfs_para.sys_sw_dvfs_en);
// mm_dvfs_force_en(mmsys->mmsys_dvfs_para.sys_dvfs_force_en);

#if 0

	mm_dvfs_hold_onoff(mmsys->mmsys_dvfs_para.sys_dvfs_hold_en);
	mm_dvfs_force_en(mmsys->mmsys_dvfs_para.sys_dvfs_force_en);
	set_mmsys_dvfs_min_volt(devfreq,
		mmsys->mmsys_dvfs_para.sys_dvfs_min_volt);
	set_mmsys_dvfs_wait_window(devfreq,
		mmsys->mmsys_dvfs_para.sys_dvfs_wait_window);
	set_mmsys_dvfs_clk_gate_ctrl(devfreq,
		mmsys->mmsys_dvfs_para.sys_dvfs_clk_gate_ctrl);
#endif
    return MM_DVFS_SUCCESS;
}

static void power_on_nb(struct devfreq *devfreq) {
    pr_debug("dvfs ops: mmsys %s\n", __func__);
    mmsys_init(devfreq);
    mm_power_flag = 1;
}
static void power_off_nb(struct devfreq *devfreq) { mm_power_flag = 0; }

static void mmsys_power_ctrl(struct devfreq *devfreq, unsigned int on) {
    /* DVFS_REG_WR_ABS(0x327e0024,0x3208004);
     * DVFS_REG_WR_ABS(0x327d0000,0x3c280);
     */
    DVFS_REG_WR_POWER(0x00, 0x3208004);
    DVFS_REG_WR_ON(0x00, 0x3c280);
    pr_info("dvfs ops: %s\n", __func__);
    if (on) {
        mmsys_notify_call_chain(MMSYS_POWER_ON);
    } else {
        mmsys_notify_call_chain(MMSYS_POWER_OFF);
    }
}

struct ip_dvfs_ops mmsys_dvfs_ops = {

    .name = "MMSYS_DVFS_OPS",
    .available = 1,

    .ip_dvfs_init = mmsys_init,
    .get_ip_status = get_ip_status,
    .mmsys_dvfs_clk_gate_ctrl = set_mmsys_dvfs_clk_gate_ctrl,
    .mmsys_sw_dvfs_en = set_mmsys_sw_dvfs_en,
    .mmsys_dvfs_hold_en = set_mmsys_dvfs_hold_en,
    .mmsys_dvfs_wait_window = set_mmsys_dvfs_wait_window,
    .mmsys_dvfs_min_volt = set_mmsys_dvfs_min_volt,
    .mmsys_power_ctrl = mmsys_power_ctrl,
    .power_on_nb = power_on_nb,
    .power_off_nb = power_off_nb,
};
