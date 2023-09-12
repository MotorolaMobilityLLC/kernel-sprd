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
#include "mm_devfreq_common.h"
#include "mm_dvfs_coffe.h"
#include "mm_dvfs_table.h"
#ifdef DVFS_VERSION_N6P
/* userspace  interface*/
static int ip_hw_dvfs_en(struct devfreq *devfreq, unsigned int dvfs_eb) {

 /*   u32 dfs_en_reg;

    mutex_lock(&mmsys_glob_reg_lock);
    dfs_en_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_MM_DFS_EN_CTRL);  //l6pro没有该地址

    if (dvfs_eb)
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_DFS_EN_CTRL, //l6pro没有该地址
                    dfs_en_reg | BIT_DEPTH_DFS_EN); //l6pro没有该地址
    else
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_DFS_EN_CTRL,
                    dfs_en_reg & (~BIT_DEPTH_DFS_EN)); //l6pro没有该地址
    mutex_unlock(&mmsys_glob_reg_lock);

    pr_debug("dvfs ops: %s, dvfs_eb=%d\n", __func__, dvfs_eb); */
    return MM_DVFS_SUCCESS;
}

static int ip_auto_tune_en(struct devfreq *devfreq, unsigned long dvfs_eb) {
    pr_debug("dvfs ops: %s\n", __func__);
    /* cpp has no this funcation */
    return MM_DVFS_SUCCESS;
}

/*work-idle dvfs map  ops*/
static int get_ip_dvfs_table(struct devfreq *devfreq,
                             struct ip_dvfs_map_cfg *dvfs_table) {
    int i = 0;

    pr_info("dvfs ops: %s\n", __func__);
    for (i = 0; i < 8; i++) {
        dvfs_table[i].map_index = depth_dvfs_config_table[i].map_index;
        dvfs_table[i].clk_freq = depth_dvfs_config_table[i].clk_freq;
        dvfs_table[i].clk = depth_dvfs_config_table[i].clk;
        dvfs_table[i].volt_value = depth_dvfs_config_table[i].volt_value;
        dvfs_table[i].volt = depth_dvfs_config_table[i].volt;
        dvfs_table[i].dcam_axi_index = depth_dvfs_config_table[i].dcam_axi_index;
        dvfs_table[i].mm_mtx_index = depth_dvfs_config_table[i].mm_mtx_index;
        dvfs_table[i].reg_add = depth_dvfs_config_table[i].reg_add;
    }

    return MM_DVFS_SUCCESS;
}

static int set_ip_dvfs_table(struct devfreq *devfreq,
                             struct ip_dvfs_map_cfg *dvfs_table) {
    memcpy(depth_dvfs_config_table, dvfs_table, sizeof(struct ip_dvfs_map_cfg));
    pr_debug("dvfs ops: %s\n", __func__);
    return MM_DVFS_SUCCESS;
}

static void get_ip_index_from_table(struct ip_dvfs_map_cfg *dvfs_cfg,
                                    unsigned long work_freq,
                                    unsigned int *index) {
    unsigned long set_clk = 0;
    u32 i;

    *index = 0;
    for (i = 0; i < 8; i++) {
        set_clk = depth_dvfs_config_table[i].clk_freq;

        if (work_freq == set_clk || work_freq < set_clk) {
            *index = i;
            break;
        }
        if (i == 7)
            *index = 7;
    }
    pr_debug("dvfs ops: %s,index=%d work_freq %ld", __func__, *index, work_freq);
}

static int set_work_freq(struct devfreq *devfreq, unsigned long work_freq) {
    u32 index_cfg_reg = 0;
    u32 index = 0;

    get_ip_index_from_table(depth_dvfs_config_table, work_freq, &index);

    index_cfg_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_DEPTH_DVFS_INDEX_CFG);
    index_cfg_reg = (index_cfg_reg & (~0x7)) | index;
    DVFS_REG_WR(REG_MM_DVFS_AHB_DEPTH_DVFS_INDEX_CFG, index_cfg_reg);
    pr_info("dvfs ops: %s, depth_work_freq=%lu, index=%d,\n", __func__, work_freq, index);
    return MM_DVFS_SUCCESS;
}

static int set_idle_freq(struct devfreq *devfreq, unsigned long idle_freq) {
    u32 index_cfg_reg = 0;
    u32 index = 0;

    get_ip_index_from_table(depth_dvfs_config_table, idle_freq, &index);

    index_cfg_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_DEPTH_DVFS_INDEX_IDLE_CFG);
    index_cfg_reg = (index_cfg_reg & (~0x7)) | index;
    DVFS_REG_WR(REG_MM_DVFS_AHB_DEPTH_DVFS_INDEX_IDLE_CFG, index_cfg_reg);

    pr_info("dvfs ops: %s, work_freq=%lu, index=%d,\n", __func__, idle_freq,
            index);
    return MM_DVFS_SUCCESS;
}

/* get ip current volt ,clk & map index*/
static int get_ip_status(struct devfreq *devfreq,
                         struct ip_dvfs_status *ip_status) {

    u32 volt_reg;
    u32 clk_reg;
    u32 i;
    volt_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_MM_DVFS_VOLTAGE_DBG1);
    clk_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_MM_DEPTH_DVFS_CGM_CFG_DBG);

    ip_status->current_ip_clk =
        (clk_reg >> SHFT_BITS_CGM_DEPTH_SEL_DVFS) & MASK_BITS_CGM_DEPTH_SEL_DVFS;
    for (i = 0; i < 8; i++) {
        if (ip_status->current_ip_clk == depth_dvfs_config_table[i].clk) {
            ip_status->current_ip_clk = depth_dvfs_config_table[i].clk_freq;
            break;
        }
    }
    ip_status->current_sys_volt =
        ((volt_reg >> SHFT_BITS_DEPTH_VOLTAGE) & MASK_BITS_DEPTH_VOLTAGE); //l6pro没有该地址
    pr_info("dvfs ops: %s\n", __func__);
    return MM_DVFS_SUCCESS;
}

/*coffe setting ops*/
static void set_ip_gfree_wait_delay(unsigned int wind_para) {

    DVFS_REG_WR(REG_MM_DVFS_AHB_MM_GFREE_WAIT_DELAY_CFG0,
                BITS_CPP_GFREE_WAIT_DELAY(wind_para));

    pr_debug("dvfs ops: %s\n", __func__);
}

static void set_ip_freq_upd_en_byp(unsigned int on) {

    u32 reg_temp = 0;

    mutex_lock(&mmsys_glob_reg_lock);
    reg_temp = DVFS_REG_RD(REG_MM_DVFS_AHB_MM_FREQ_UPDATE_BYPASS);

    if (on)
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_FREQ_UPDATE_BYPASS,
                    reg_temp | BIT_REG_DEPTH_FREQ_UPD_EN_BYP);
    else
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_FREQ_UPDATE_BYPASS,
                    reg_temp & (~BIT_REG_DEPTH_FREQ_UPD_EN_BYP));
    mutex_unlock(&mmsys_glob_reg_lock);
    pr_debug("dvfs ops: %s\n", __func__);
}

static void set_ip_freq_upd_delay_en(unsigned int on) {

    u32 reg_temp = 0;

    mutex_lock(&mmsys_glob_reg_lock);
    reg_temp = DVFS_REG_RD(REG_MM_DVFS_AHB_MM_FREQ_UPD_TYPE_CFG0);
    if (on)
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_FREQ_UPD_TYPE_CFG0,
                    reg_temp | BIT_DEPTH_FREQ_UPD_DELAY_EN);
    else
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_FREQ_UPD_TYPE_CFG0,
                    reg_temp & (~(BIT_DEPTH_FREQ_UPD_DELAY_EN)));
    mutex_unlock(&mmsys_glob_reg_lock);

    pr_debug("dvfs ops: %s\n", __func__);
}

static void set_ip_freq_upd_hdsk_en(unsigned int on) {

    u32 reg_temp = 0;

    mutex_lock(&mmsys_glob_reg_lock);
    reg_temp = DVFS_REG_RD(REG_MM_DVFS_AHB_MM_FREQ_UPD_TYPE_CFG0);

    if (on)
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_FREQ_UPD_TYPE_CFG0,
                    reg_temp | BIT_DEPTH_FREQ_UPD_HDSK_EN);
    else
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_FREQ_UPD_TYPE_CFG0,
                    reg_temp & (~(BIT_DEPTH_FREQ_UPD_HDSK_EN)));
    mutex_unlock(&mmsys_glob_reg_lock);

    pr_debug("dvfs ops: %s\n", __func__);
}

static void set_ip_dvfs_swtrig_en(unsigned int en) {
/*    u32 dfs_swtrig_en;

    mutex_lock(&mmsys_glob_reg_lock);

    dfs_swtrig_en = DVFS_REG_RD(REG_MM_DVFS_AHB_MM_DFS_SW_TRIG_CFG);
    if (en)
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_DFS_SW_TRIG_CFG,
                    dfs_swtrig_en | BIT_DEPTH_DFS_SW_TRIG); 
    else
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_DFS_SW_TRIG_CFG,
                    dfs_swtrig_en & (~BIT_DEPTH_DFS_SW_TRIG)); //l6pro没有该地址
    mutex_unlock(&mmsys_glob_reg_lock); */

    pr_debug("dvfs ops: %s\n", __func__);
}

/*work-idle dvfs index ops*/
static void set_ip_dvfs_work_index(struct devfreq *devfreq,
                                   unsigned int index) {
    u32 index_cfg_reg = 0;
    index_cfg_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_DEPTH_DVFS_INDEX_CFG);
    index_cfg_reg = (index_cfg_reg & (~0x7)) | index;
    DVFS_REG_WR(REG_MM_DVFS_AHB_DEPTH_DVFS_INDEX_CFG, index_cfg_reg);
    pr_debug("dvfs ops: %s, index_cfg=%d, work index=%d\n", __func__, index, index_cfg_reg);
}

/*work-idle dvfs index ops*/
static void get_ip_dvfs_work_index(struct devfreq *devfreq,
                                   unsigned int *index) {

    *index = DVFS_REG_RD(REG_MM_DVFS_AHB_DEPTH_DVFS_INDEX_CFG);

    pr_debug("dvfs ops: %s, work_index=%d\n", __func__, *index);
}

static void depth_dvfs_map_cfg(void) {
    u32 map_cfg_reg = 0;
    u32 i = 0;

    for (i = 0; i < 8; i++) {
        map_cfg_reg = 0x0;
        map_cfg_reg =
            (map_cfg_reg & (~0x1ff)) |
            BITS_DEPTH_VOL_INDEX0(depth_dvfs_config_table[i].volt) |
            BITS_CGM_DEPTH_SEL_INDEX0(depth_dvfs_config_table[i].clk);

        /* DVFS_REG_WR(REG_MM_DVFS_AHB_DEPTH_INDEX0_MAP, map_cfg_reg); */
        DVFS_REG_WR((depth_dvfs_config_table[i].reg_add), map_cfg_reg);
    }
}

static void set_ip_dvfs_idle_index(struct devfreq *devfreq,
                                   unsigned int index) {
    u32 index_cfg_reg = 0;

    pr_debug("dvfs ops: %s, work index=%d\n", __func__, index);

    index_cfg_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_DEPTH_DVFS_INDEX_IDLE_CFG);
    index_cfg_reg = (index_cfg_reg & (~0x7)) | index;
    DVFS_REG_WR(REG_MM_DVFS_AHB_DEPTH_DVFS_INDEX_IDLE_CFG, index_cfg_reg);
    depth_dvfs_map_cfg();
}

static int ip_dvfs_init(struct devfreq *devfreq) {

    struct module_dvfs *depth;
    depth = dev_get_drvdata(devfreq->dev.parent);
    if (!depth) {
        pr_info("undefined cpp_dvfs\n");
        return -EINVAL;
    }
    depth_dvfs_map_cfg();
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
    devfreq->max_freq = depth_dvfs_config_table[7].clk_freq;
    devfreq->min_freq = depth_dvfs_config_table[0].clk_freq;
#endif
    set_ip_freq_upd_en_byp(DEPTH_FREQ_UPD_EN_BYP);
    set_ip_freq_upd_delay_en(DEPTH_FREQ_UPD_DELAY_EN);
    set_ip_freq_upd_hdsk_en(DEPTH_FREQ_UPD_HDSK_EN);
    set_ip_gfree_wait_delay(DEPTH_GFREE_WAIT_DELAY);
    //set_ip_dvfs_swtrig_en(DEPTH_SW_TRIG_EN);
    set_ip_dvfs_work_index(devfreq, DEPTH_WORK_INDEX_DEF);
    set_ip_dvfs_idle_index(devfreq, DEPTH_IDLE_INDEX_DEF);
    //ip_hw_dvfs_en(devfreq, DEPTH_DFS_EN);
    depth->dvfs_enable = TRUE;
    depth->freq = depth_dvfs_config_table[DEPTH_WORK_INDEX_DEF].clk_freq;
    pr_info("depth dvfs init param: HDSK_EN %d WORK_INDEX_DEF %d IDLE_INDEX_DEF %d\n", DEPTH_FREQ_UPD_HDSK_EN
            , DEPTH_WORK_INDEX_DEF, DEPTH_IDLE_INDEX_DEF);
    return MM_DVFS_SUCCESS;
}

static int updata_target_freq(struct devfreq *devfreq, unsigned long volt,
                              unsigned long freq, unsigned int eb_sysgov) {


    if (eb_sysgov == TRUE)
        mmsys_adjust_target_freq(DVFS_DEPTH, volt, freq, eb_sysgov);
    else
        set_work_freq(devfreq, freq);

    pr_debug("dvfs ops: %s\n", __func__);
    return MM_DVFS_SUCCESS;
}

/*debug interface */
static int set_fix_dvfs_value(struct devfreq *devfreq, unsigned long fix_volt) {

    mmsys_set_fix_dvfs_value(fix_volt);

    pr_info("dvfs ops: %s\n,fix_volt=%lu", __func__, fix_volt);
    return 1;
}

static void power_on_nb(struct devfreq *devfreq) {
    ip_dvfs_init(devfreq);
    pr_debug("dvfs ops: isp %s\n", __func__);
}

static void power_off_nb(struct devfreq *devfreq) {
    struct module_dvfs *depth_dvfs;
    depth_dvfs = dev_get_drvdata(devfreq->dev.parent);
    if (depth_dvfs != NULL) {
        pr_info("depth_dvfs dvfs power off\n");
        depth_dvfs->dvfs_enable = 0;
    }
}

static int top_current_volt(struct devfreq *devfreq, unsigned int *top_volt) {
    unsigned int ret;

    ret = top_mm_dvfs_current_volt(devfreq);
    *top_volt = ret;
    pr_info("dvfs ops: %s\n", __func__);
    return MM_DVFS_SUCCESS;
}

static int mm_current_volt(struct devfreq *devfreq, unsigned int *mm_volt) {
    unsigned int ret;

    ret = mm_dvfs_read_current_volt(devfreq);
    *mm_volt = ret;
    pr_info("dvfs ops: %s\n", __func__);
    return MM_DVFS_SUCCESS;
}

struct ip_dvfs_ops depth_dvfs_ops = {

    .name = "DEPTH_DVFS_OPS",
    .available = 1,

    .ip_dvfs_init = ip_dvfs_init,
    .ip_hw_dvfs_en = ip_hw_dvfs_en,
    .ip_auto_tune_en = ip_auto_tune_en,
    .set_work_freq = set_work_freq,
    .set_idle_freq = set_idle_freq,
    .get_ip_dvfs_table = get_ip_dvfs_table,
    .set_ip_dvfs_table = set_ip_dvfs_table,
    .get_ip_status = get_ip_status,
    .set_ip_dvfs_work_index = set_ip_dvfs_work_index,
    .get_ip_dvfs_work_index = get_ip_dvfs_work_index,
    .set_ip_dvfs_idle_index = set_ip_dvfs_idle_index,
    .get_ip_work_index_from_table = get_ip_index_from_table,
    .get_ip_idle_index_from_table = get_ip_index_from_table,

    .set_ip_gfree_wait_delay = set_ip_gfree_wait_delay,
    .set_ip_freq_upd_en_byp = set_ip_freq_upd_en_byp,
    .set_ip_freq_upd_delay_en = set_ip_freq_upd_delay_en,
    .set_ip_freq_upd_hdsk_en = set_ip_freq_upd_hdsk_en,
    .set_ip_dvfs_swtrig_en = set_ip_dvfs_swtrig_en,

    .set_fix_dvfs_value = set_fix_dvfs_value,
    .updata_target_freq = updata_target_freq,
    .power_on_nb = power_on_nb,
    .power_off_nb = power_off_nb,
    .top_current_volt = top_current_volt,
    .mm_current_volt = mm_current_volt,
    .event_handler = NULL,
};
#endif