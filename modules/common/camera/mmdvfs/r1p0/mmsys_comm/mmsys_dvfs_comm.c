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

#include "mmsys_dvfs_comm.h"
#include "top_dvfs_reg.h"
#include "mm_dvfs.h"
#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
#include <video/sprd_mmsys_pw_domain.h>
#else 
#include <sprd_camsys_domain.h>
#endif


static LIST_HEAD(ip_dvfs_ops_list);
static DEFINE_MUTEX(ip_dvfsops_list_lock);
static BLOCKING_NOTIFIER_HEAD(mmsys_notifier_list);
DEFINE_MUTEX(mmsys_glob_reg_lock);

struct mmreg_map g_mmreg_map;

int mmsys_adjust_target_freq(mmsys_module md_type, unsigned long volt,
                             unsigned long freq, unsigned int enable_sysgov) {

    pr_info("dvfs ops: %s\n", __func__);
    return MM_DVFS_SUCCESS;
}

/*debug interface */
int mmsys_set_fix_dvfs_value(unsigned long fix_volt) {
    u32 fix_reg = 0;
    u32 fix_on = 0;

    fix_on = ((fix_volt >> 31) & 0x1);
    fix_reg = (fix_volt & 0x7) << 1;

    if (fix_on)
        regmap_update_bits(g_mmreg_map.mmdvfs_top_regmap, REG_TOP_DVFS_APB_DCDC_MM_FIX_VOLTAGE_CTRL,
                         BIT_DCDC_MM_FIX_VOLTAGE_EN, (fix_reg | BIT_DCDC_MM_FIX_VOLTAGE_EN));
    else
        regmap_update_bits(g_mmreg_map.mmdvfs_top_regmap, REG_TOP_DVFS_APB_DCDC_MM_FIX_VOLTAGE_CTRL,
                        BIT_DCDC_MM_FIX_VOLTAGE_EN, fix_reg & (~BIT_DCDC_MM_FIX_VOLTAGE_EN));


    pr_info("dvfs ops: %s\n", __func__);
    return MM_DVFS_SUCCESS;
}

int top_mm_dvfs_current_volt(struct devfreq *devfreq) {
    unsigned int volt_reg = 0;
    int ret = 0;
    msleep(1);
    ret = regmap_read(g_mmreg_map.mmdvfs_top_regmap, REG_TOP_DVFS_APB_DCDC_MM_DVFS_STATE_DBG, &volt_reg);
    if (ret != 0) {
        pr_err("%s, regmap_read fail!\n", __func__);
        return 0;
    }
    volt_reg = (volt_reg >> 20) & 0x7;
    pr_info("dvfs_debug : %s volt_reg=%d \n", __func__, volt_reg);
    return volt_reg;
}

int mm_dvfs_read_current_volt(struct devfreq *devfreq) {
    unsigned int volt_reg;

    msleep(1);
    volt_reg = DVFS_REG_RD(REG_MM_DVFS_AHB_MM_DVFS_VOLTAGE_DBG);

    pr_info("dvfs_debug : %s volt_reg=%d \n", __func__, volt_reg);
    return (unsigned int)((volt_reg >> SHFT_BITS_MM_CURRENT_VOLTAGE) &
                          MASK_BITS_MM_CURRENT_VOLTAGE);
}

/* *
 * register_ip_dvfs_ops() - Add ip ops
 * @dvfs_ops:     the dvfs_ops to be added
 */
int register_ip_dvfs_ops(struct ip_dvfs_ops *dvfs_ops) {
    struct ip_dvfs_ops *g;
    int err = MM_DVFS_SUCCESS;

    if (!dvfs_ops) {
        pr_err("%s: Invalid parameters.\n", __func__);
        return -EINVAL;
    }

    pr_err("  Enter  %s:  \n", __func__);

    g = get_ip_dvfs_ops(dvfs_ops->name);

    if (!IS_ERR(g)) {
        pr_err("%s: ip dvfs ops %s already registered\n", __func__, g->name);
        err = -EINVAL;
        goto err_out;
    }

    mutex_lock(&ip_dvfsops_list_lock);
    list_add(&dvfs_ops->node, &ip_dvfs_ops_list);
    mutex_unlock(&ip_dvfsops_list_lock);

err_out:
    pr_err("  Exit %s:  \n", __func__);

    return err;
}

/* *
 * unregister_ip_dvfs_ops() - Remove ip ops feature from ops list
 * @dvfs_ops:     the ip dvfs_ops to be removed
 */
int unregister_ip_dvfs_ops(struct ip_dvfs_ops *dvfs_ops) {
    struct ip_dvfs_ops *g;
    int err = MM_DVFS_SUCCESS;

    if (!dvfs_ops) {
        pr_err("%s: Invalid parameters.\n", __func__);
        return -EINVAL;
    }

    pr_info("Enter   %s:  \n", __func__);

    g = get_ip_dvfs_ops(dvfs_ops->name);
    if (IS_ERR(g)) {
        pr_err("%s: governor %s not registered\n", __func__, dvfs_ops->name);
        err = PTR_ERR(g);
        goto err_out;
    }

    mutex_lock(&ip_dvfsops_list_lock);
    list_del(&dvfs_ops->node);
    mutex_unlock(&ip_dvfsops_list_lock);

err_out:
    pr_info("Exit  %s:  \n", __func__);

    return err;
}

/* *
 * find_devfreq_dvfsops() - find devfreq dvfsops from name
 * @name:     name of the dvfsops
 *
 * Search the list of ip ops and return the matched
 * dvfsops's pointer. ip_dvfsops_list_lock should be held by the caller.
 */
struct ip_dvfs_ops *get_ip_dvfs_ops(const char *name) {
    struct ip_dvfs_ops *tmp_ip_ops;

    pr_info("dvfs ops:  mmsys %s\n", __func__);

    if (IS_ERR_OR_NULL(name)) {
        pr_err("DEVFREQ: %s: Invalid parameters\n", __func__);
        return ERR_PTR(-EINVAL);
    }
    mutex_lock(&ip_dvfsops_list_lock);
    // WARN(!mutex_is_locked(&ip_dvfsops_list_lock),
    //	"ip_dvfsops_list_lock must be locked.");

    list_for_each_entry(tmp_ip_ops, &ip_dvfs_ops_list, node) {
        if (!strncmp(tmp_ip_ops->name, name, DEVFREQ_NAME_LEN)) {
            pr_err(" mmsys %s:  \n", __func__);
            mutex_unlock(&ip_dvfsops_list_lock);
            return tmp_ip_ops;
        }
    }
    mutex_unlock(&ip_dvfsops_list_lock);

    return ERR_PTR(-ENODEV);
}

/* *
 * mmsys_register_notifier() - Register mmsys notifier
 * @nb:     The notifier block to register.
 */
int mmsys_register_notifier(struct notifier_block *nb) {
    int ret = MM_DVFS_SUCCESS;

    if (!nb)
        return -EINVAL;

    ret = blocking_notifier_chain_register(&mmsys_notifier_list, nb);

    return ret;
}

/*
 * mmsys_unregister_notifier() - Unregister notifier
 * @nb:     The notifier block to be unregistered.
 */
int mmsys_unregister_notifier(struct notifier_block *nb) {
    int ret = MM_DVFS_SUCCESS;

    if (!nb)
        return -EINVAL;

    ret = blocking_notifier_chain_unregister(&mmsys_notifier_list, nb);

    return ret;
}

int mmsys_notify_call_chain(mmsys_notifier_type notifier_type) {
    int ret = MM_DVFS_SUCCESS;
    static atomic_t pw_opened;

    pr_info("%s:   notifier_type :%d enter\n", __func__, notifier_type);
    pr_info("%s:   pw opened count %d\n", __func__, atomic_read(&pw_opened));

    switch (notifier_type) {
    case MMSYS_POWER_ON:

#if 1
        if (atomic_inc_return(&pw_opened) > 1) {
            pr_err("dvfs:  pw opened already.");
            atomic_dec(&pw_opened);
            return -EMFILE;
        }
#endif

        pr_info("%s:  pw opened count %d\n", __func__, atomic_read(&pw_opened));

        ret = blocking_notifier_call_chain(&mmsys_notifier_list, notifier_type,
                                           NULL);
        break;

    case MMSYS_POWER_OFF:
        if (atomic_dec_return(&pw_opened) != 0) {
            pr_err("%s:  err: pw opened count= %d\n", __func__,
                   atomic_read(&pw_opened));
        }
        ret = blocking_notifier_call_chain(&mmsys_notifier_list, notifier_type,
                                           NULL);
        break;
    default:
        return -EINVAL;
    }

    pr_info("%s: notifier_type :%d exit\n", __func__, notifier_type);

    return MM_DVFS_SUCCESS;
}

/*int mmsys_dvfs_power (int on, void *pDat)
{
    int ret = 0;
        if(on==MMSYS_POWER_ON)
        mmsys_notify_call_chain(MMSYS_POWER_ON);
        else
        mmsys_notify_call_chain(MMSYS_POWER_OFF);

    return ret;

}*/

#if 0
 int mmsys_dvfs_pw_on(void)
{
	int ret = 0;
	ret = sprd_cam_pw_on();
	mmsys_notify_call_chain(MMSYS_POWER_ON);

	return ret;
}
 EXPORT_SYMBOL(mmsys_dvfs_pw_on);


 int mmsys_dvfs_pw_off(void)
{
	int ret = 0;
	ret = sprd_cam_pw_off();
	mmsys_notify_call_chain(MMSYS_POWER_OFF);
	return ret;
}
 EXPORT_SYMBOL(mmsys_dvfs_pw_off);
#endif
