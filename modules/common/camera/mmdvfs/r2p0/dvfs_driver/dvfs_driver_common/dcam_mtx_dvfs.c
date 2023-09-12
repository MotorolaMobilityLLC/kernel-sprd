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

/* userspace  interface*/
static int ip_hw_dvfs_en(struct devfreq *devfreq, unsigned int dvfs_eb) {

    return MM_DVFS_SUCCESS;
}

static int ip_auto_tune_en(struct devfreq *devfreq, unsigned long dvfs_eb) {
#ifdef DVFS_VERSION_N6L
	u32 dfs_en_reg;
	mutex_lock(&mmsys_glob_reg_lock);
	dfs_en_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_MM_AUTO_TUNE_EN_CTRL);
	if (dvfs_eb)
		DVFS_REG_WR(REG_MM_DVFS_AHB_MM_AUTO_TUNE_EN_CTRL,
		dfs_en_reg | BIT_DCAM_MTX_AUTO_TUNE_EN);
	else
		DVFS_REG_WR(REG_MM_DVFS_AHB_MM_AUTO_TUNE_EN_CTRL,
		dfs_en_reg & (~BIT_DCAM_MTX_AUTO_TUNE_EN));
	mutex_unlock(&mmsys_glob_reg_lock);
#endif
	return MM_DVFS_SUCCESS;
}


/*work-idle dvfs map  ops*/
static int get_ip_dvfs_table(struct devfreq *devfreq,
                             struct ip_dvfs_map_cfg *dvfs_table) {
    int i = 0;

    pr_info("dvfs ops: %s\n", __func__);
    for (i = 0; i < 8; i++) {
        dvfs_table[i].map_index = dcam_mtx_dvfs_config_table[i].map_index;
        dvfs_table[i].clk_freq = dcam_mtx_dvfs_config_table[i].clk_freq;
        dvfs_table[i].clk = dcam_mtx_dvfs_config_table[i].clk;
        dvfs_table[i].volt_value = dcam_mtx_dvfs_config_table[i].volt_value;
        dvfs_table[i].volt = dcam_mtx_dvfs_config_table[i].volt;
        dvfs_table[i].dcam_axi_index = dcam_mtx_dvfs_config_table[i].dcam_axi_index;
        dvfs_table[i].mm_mtx_index = dcam_mtx_dvfs_config_table[i].mm_mtx_index;
        dvfs_table[i].reg_add = dcam_mtx_dvfs_config_table[i].reg_add;
    }

    return MM_DVFS_SUCCESS;
}

static int set_ip_dvfs_table(struct devfreq *devfreq,
                             struct ip_dvfs_map_cfg *dvfs_table) {
    memcpy(dcam_mtx_dvfs_config_table, dvfs_table,
           sizeof(struct ip_dvfs_map_cfg));
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
        set_clk = dcam_mtx_dvfs_config_table[i].clk_freq;

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

    get_ip_index_from_table(dcam_mtx_dvfs_config_table, work_freq, &index);

    index_cfg_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_DCAM_MTX_DVFS_INDEX_CFG);
    index_cfg_reg = (index_cfg_reg & (~0x7)) | index;
    DVFS_REG_WR(REG_MM_DVFS_AHB_DCAM_MTX_DVFS_INDEX_CFG, index_cfg_reg);
    pr_info("dvfs ops: %s, dcam_mtx_work_freq=%lu, index=%d,\n", __func__, work_freq, index);
    return MM_DVFS_SUCCESS;
}

int set_dcam_mtx_work_freq(unsigned int index){
	u32 index_cfg_reg = 0;

	index_cfg_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_DCAM_MTX_DVFS_INDEX_CFG);
	index_cfg_reg = (index_cfg_reg & (~0x7)) | index;
	DVFS_REG_WR(REG_MM_DVFS_AHB_DCAM_MTX_DVFS_INDEX_CFG, index_cfg_reg);
	pr_info("dvfs ops: %s, DCAM_MTX, work_index=%d,\n", __func__,
		index);
	return MM_DVFS_SUCCESS;

}


static int set_idle_freq(struct devfreq *devfreq, unsigned long idle_freq) {
    u32 index_cfg_reg = 0;
    u32 index = 0;

    get_ip_index_from_table(dcam_mtx_dvfs_config_table, idle_freq, &index);

    index_cfg_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_DCAM_MTX_DVFS_INDEX_IDLE_CFG);
    index_cfg_reg = (index_cfg_reg & (~0x7)) | index;
    DVFS_REG_WR(REG_MM_DVFS_AHB_DCAM_MTX_DVFS_INDEX_IDLE_CFG, index_cfg_reg);

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
    volt_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_MM_DVFS_VOLTAGE_DBG0);
    clk_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_MM_DCAM_MTX_DVFS_CGM_CFG_DBG);

    ip_status->current_ip_clk = (clk_reg >> SHFT_BITS_CGM_DCAM_MTX_SEL_DVFS) &
                                MASK_BITS_CGM_DCAM_MTX_SEL_DVFS;
    for (i = 0; i < 8; i++) {
        if (ip_status->current_ip_clk == dcam_mtx_dvfs_config_table[i].clk) {
            ip_status->current_ip_clk = dcam_mtx_dvfs_config_table[i].clk_freq;
            break;
        }
    }
    ip_status->current_sys_volt =
        ((volt_reg >> SHFT_BITS_DCAM_MTX_VOLTAGE) & MASK_BITS_DCAM_MTX_VOLTAGE);
    pr_info("dvfs ops: %s\n", __func__);
    return MM_DVFS_SUCCESS;
}

/*coffe setting ops*/
static void set_ip_gfree_wait_delay(unsigned int wind_para) {

    DVFS_REG_WR(REG_MM_DVFS_AHB_MM_GFREE_WAIT_DELAY_CFG0,
                BITS_DCAM_MTX_GFREE_WAIT_DELAY(wind_para));

    pr_debug("dvfs ops: %s\n", __func__);
}

static void set_ip_freq_upd_en_byp(unsigned int on) {

    u32 reg_temp = 0;

    mutex_lock(&mmsys_glob_reg_lock);
    reg_temp = DVFS_REG_RD(REG_MM_DVFS_AHB_MM_FREQ_UPDATE_BYPASS);

    if (on)
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_FREQ_UPDATE_BYPASS,
                    reg_temp | BIT_REG_DCAM_MTX_FREQ_UPD_EN_BYP);
    else
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_FREQ_UPDATE_BYPASS,
                    reg_temp & (~BIT_REG_DCAM_MTX_FREQ_UPD_EN_BYP));
    mutex_unlock(&mmsys_glob_reg_lock);
    pr_debug("dvfs ops: %s\n", __func__);
}

static void set_ip_freq_upd_delay_en(unsigned int on) {

    u32 reg_temp = 0;

    mutex_lock(&mmsys_glob_reg_lock);
    reg_temp = DVFS_REG_RD(REG_MM_DVFS_AHB_MM_FREQ_UPD_TYPE_CFG0);
    if (on)
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_FREQ_UPD_TYPE_CFG0,
                    reg_temp | BIT_DCAM_MTX_FREQ_UPD_DELAY_EN);
    else
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_FREQ_UPD_TYPE_CFG0,
                    reg_temp & (~(BIT_DCAM_MTX_FREQ_UPD_DELAY_EN)));
    mutex_unlock(&mmsys_glob_reg_lock);

    pr_debug("dvfs ops: %s\n", __func__);
}

static void set_ip_freq_upd_hdsk_en(unsigned int on) {

    u32 reg_temp = 0;

    mutex_lock(&mmsys_glob_reg_lock);
    reg_temp = DVFS_REG_RD(REG_MM_DVFS_AHB_MM_FREQ_UPD_TYPE_CFG0);

    if (on)
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_FREQ_UPD_TYPE_CFG0,
                    reg_temp | BIT_DCAM_MTX_FREQ_UPD_HDSK_EN);
    else
        DVFS_REG_WR(REG_MM_DVFS_AHB_MM_FREQ_UPD_TYPE_CFG0,
                    reg_temp & (~(BIT_DCAM_MTX_FREQ_UPD_HDSK_EN)));
    mutex_unlock(&mmsys_glob_reg_lock);

    pr_debug("dvfs ops: %s\n", __func__);
}

static void set_ip_dvfs_work_index(struct devfreq *devfreq,
                                   unsigned int index) {
    u32 index_cfg_reg = 0;
    index_cfg_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_DCAM_MTX_DVFS_INDEX_CFG);
    index_cfg_reg = (index_cfg_reg & (~0x7)) | index;
    DVFS_REG_WR(REG_MM_DVFS_AHB_DCAM_MTX_DVFS_INDEX_CFG, index_cfg_reg);
    pr_debug("dvfs ops: %s, index_cfg=%d\n", __func__, index_cfg_reg);
}

/*work-idle dvfs index ops*/
static void get_ip_dvfs_work_index(struct devfreq *devfreq,
                                   unsigned int *index) {

    *index = DVFS_REG_RD(REG_MM_DVFS_AHB_DCAM_MTX_DVFS_INDEX_CFG);

    pr_debug("dvfs ops: %s, work_index=%d\n", __func__, *index);
}

static void set_ip_dvfs_idle_index(struct devfreq *devfreq,
                                   unsigned int index) {
    u32 index_cfg_reg = 0;

    pr_debug("dvfs ops: %s, work index=%d\n", __func__, index);

    index_cfg_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_DCAM_MTX_DVFS_INDEX_IDLE_CFG);
    index_cfg_reg = (index_cfg_reg & (~0x7)) | index;
    DVFS_REG_WR(REG_MM_DVFS_AHB_DCAM_MTX_DVFS_INDEX_IDLE_CFG, index_cfg_reg);
}

static void dcam_mtx_dvfs_map_cfg(void) {
    u32 map_cfg_reg = 0;
    u32 i = 0;

    for (i = 0; i < 8; i++) {
        map_cfg_reg = 0x0;

        map_cfg_reg =
            (map_cfg_reg & (~0x1ff)) |
            BITS_DCAM_MTX_VOL_INDEX0(dcam_mtx_dvfs_config_table[i].volt) |
            BITS_CGM_DCAM_MTX_SEL_INDEX0(dcam_mtx_dvfs_config_table[i].clk);
        DVFS_REG_WR((dcam_mtx_dvfs_config_table[i].reg_add), map_cfg_reg);
    }
}

static int ip_dvfs_init(struct devfreq *devfreq) {

    struct module_dvfs *dcam_mtx;
    dcam_mtx = dev_get_drvdata(devfreq->dev.parent);
    if (!dcam_mtx) {
        pr_info("undefined dcamaxi_dvfs\n");
        return -EINVAL;
    }

    dcam_mtx_dvfs_map_cfg();
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
    devfreq->max_freq = dcam_mtx_dvfs_config_table[7].clk_freq;
    devfreq->min_freq = dcam_mtx_dvfs_config_table[0].clk_freq;
#endif
    set_ip_freq_upd_en_byp(DCAM_MTX_FREQ_UPD_EN_BYP);
    set_ip_freq_upd_delay_en(DCAM_MTX_FREQ_UPD_DELAY_EN);
    set_ip_freq_upd_hdsk_en(DCAM_MTX_FREQ_UPD_HDSK_EN);
    set_ip_gfree_wait_delay(DCAM_MTX_GFREE_WAIT_DELAY);
    set_ip_dvfs_work_index(devfreq, DCAM_MTX_WORK_INDEX_DEF);
    set_ip_dvfs_idle_index(devfreq, DCAM_MTX_IDLE_INDEX_DEF);
#ifdef DVFS_VERSION_N6L
    ip_auto_tune_en(devfreq, DCAM_MTX_AUTO_TUNE);
#endif
    dcam_mtx->dvfs_enable = TRUE;
    dcam_mtx->freq = dcam_mtx_dvfs_config_table[DCAM_MTX_WORK_INDEX_DEF].clk_freq;

    pr_info("dcam_mtx dvfs init param: HDSK_EN %d WORK_INDEX_DEF %d IDLE_INDEX_DEF %d\n", DCAM_MTX_FREQ_UPD_HDSK_EN, DCAM_MTX_WORK_INDEX_DEF,
            DCAM_MTX_IDLE_INDEX_DEF);

    return MM_DVFS_SUCCESS;
}

static int updata_target_freq(struct devfreq *devfreq, unsigned long volt,
                              unsigned long freq, unsigned int eb_sysgov) {

    if (eb_sysgov == TRUE)
        mmsys_adjust_target_freq(DVFS_DCAM_MTX, volt, freq, eb_sysgov);
    else
        set_work_freq(devfreq, freq);

    pr_debug("dvfs ops: %s\n", __func__);
    return MM_DVFS_SUCCESS;
}

/*debug interface */
static int set_fix_dvfs_value(struct devfreq *devfreq, unsigned long fix_volt) {
    mmsys_set_fix_dvfs_value(fix_volt);

    pr_debug("dvfs ops: %s\n", __func__);
    return MM_DVFS_SUCCESS;
}

static void power_on_nb(struct devfreq *devfreq) {
    ip_dvfs_init(devfreq);
    pr_debug("dvfs ops: dcam_axi %s\n", __func__);
}

static void power_off_nb(struct devfreq *devfreq) {
    struct module_dvfs *dcam_mtx_dvfs;
    dcam_mtx_dvfs = dev_get_drvdata(devfreq->dev.parent);
    if (dcam_mtx_dvfs != NULL) {
        pr_info("dcam_mtx_dvfs dvfs power off\n");
        dcam_mtx_dvfs->dvfs_enable = 0;
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

struct ip_dvfs_ops dcam_mtx_dvfs_ops = {

    .name = "DMTX_DVFS_OPS",
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
    .set_fix_dvfs_value = set_fix_dvfs_value,
    .updata_target_freq = updata_target_freq,
    .power_on_nb = power_on_nb,
    .power_off_nb = power_off_nb,
    .top_current_volt = top_current_volt,
    .mm_current_volt = mm_current_volt,
    .event_handler = NULL,
};
