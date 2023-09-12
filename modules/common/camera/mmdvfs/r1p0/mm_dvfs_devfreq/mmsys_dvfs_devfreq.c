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

#include "mmsys_dvfs.h"
#include "top_dvfs_reg.h"
#include "mmsys_dvfs_comm.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
#include <video/sprd_mmsys_pw_domain.h>
#else
#include <sprd_camsys_domain.h>
#endif


static int mmsys_dvfs_target(struct device *dev, unsigned long *freq,
                             u32 flags);
static int mmsys_dvfs_get_dev_status(struct device *dev,
                                     struct devfreq_dev_status *stat);
static int mmsys_dvfs_get_cur_freq(struct device *dev, unsigned long *freq);
static int mmsys_dvfs_probe(struct platform_device *pdev);
static int mmsys_dvfs_remove(struct platform_device *pdev);

static const struct of_device_id mmsys_dvfs_of_match[] = {
    {.compatible = "sprd,hwdvfs-mmsys"}, {},
};
MODULE_DEVICE_TABLE(of, mmsys_dvfs_of_match);

static struct platform_driver mmsys_dvfs_driver = {
    .probe = mmsys_dvfs_probe,
    .remove = mmsys_dvfs_remove,
    .driver =
        {
            .name = "mmsys-dvfs", .of_match_table = mmsys_dvfs_of_match,
        },
};

static struct devfreq_dev_profile mmsys_dvfs_profile = {
    .polling_ms = 200,
    .target = mmsys_dvfs_target,
    .get_dev_status = mmsys_dvfs_get_dev_status,
    .get_cur_freq = mmsys_dvfs_get_cur_freq,
};

static int mmsys_dvfs_target(struct device *dev, unsigned long *freq,
                             u32 flags) {
    pr_info("%s:\n", __func__);
    /* TODO for sw dvfs  & sys dvfs */
    return MM_DVFS_SUCCESS;
}

int mmsys_dvfs_get_dev_status(struct device *dev,
                              struct devfreq_dev_status *stat) {
    /* TODO for sw dvfs  & sys dvfs */
    pr_info("devfreq_dev_profile-->get_dev_status\n");
    return MM_DVFS_SUCCESS;
}

static int mmsys_dvfs_get_cur_freq(struct device *dev, unsigned long *freq) {
    /* TODO for sw dvfs  & sys dvfs */
    pr_info("devfreq_dev_profile-->get_cur_freq\n");
    return MM_DVFS_SUCCESS;
}

static int mmsys_dvfs_notify_callback(struct notifier_block *nb,
                                      unsigned long type, void *data) {
    struct mmsys_dvfs *mmsys = container_of(nb, struct mmsys_dvfs, pw_nb);

    pr_info("mmsys_dvfs_notify_callback%lu \n", type);
    switch (type) {

    case MMSYS_POWER_ON:
        if (mmsys->dvfs_ops != NULL && mmsys->dvfs_ops->power_on_nb != NULL) {
            mmsys->dvfs_ops->power_on_nb(mmsys->devfreq);
        }
        break;

    case MMSYS_POWER_OFF:
        if (mmsys->dvfs_ops != NULL && mmsys->dvfs_ops->power_off_nb != NULL) {
            mmsys->dvfs_ops->power_off_nb(mmsys->devfreq);
        }
        break;

    default:
        return -EINVAL;
    }

    return NOTIFY_OK;
}

static int mmsys_dvfs_probe(struct platform_device *pdev) {
    struct device *dev = &pdev->dev;
    struct device_node *np = pdev->dev.of_node;
    struct device_node *top_node = NULL;
    struct mmsys_dvfs *mmsys;
    void __iomem *reg_base = NULL;
    struct resource reg_res = {0};
    int ret = MM_DVFS_SUCCESS;

    pr_info("mmsys-dvfs initialized\n");

    mmsys = devm_kzalloc(dev, sizeof(*mmsys), GFP_KERNEL);
    if (!mmsys)
        return -ENOMEM;

    mutex_init(&mmsys->lock);

#ifdef SUPPORT_SWDVFS

    mmsys->clk_mmsys_core = devm_clk_get(dev, "clk_mmsys_core");
    if (IS_ERR(mmsys->clk_mmsys_core)) {
        dev_err(dev, "Cannot get the clk_mmsys_core clk\n");
    };
#endif

    reg_res.start = REGS_MM_DVFS_AHB_START;
    reg_res.end = REGS_MM_DVFS_AHB_END;
    reg_base = ioremap(reg_res.start, reg_res.end - reg_res.start + 1);
    if (!reg_base) {
        pr_err("fail to map mmsys dvfs ahb reg base\n");
        goto err_iounmap;
    }
    g_mmreg_map.mmdvfs_ahb_regbase = (unsigned long)reg_base;

    reg_res.start = REGS_MM_TOP_DVFS_START;
    reg_res.end = REGS_MM_TOP_DVFS_END;
    reg_base = ioremap(reg_res.start, reg_res.end - reg_res.start + 1);
    if (!reg_base) {
        pr_err("fail to map mmsys dvfs top reg base\n");
        goto err_iounmap;
    }
    top_node = of_parse_phandle(np, "sprd,topdvfs_controller", 0);
    if (!top_node){
      pr_err("Failed to find 'sprd,topdvfs_controller' node\n");
      ret = -EINVAL;
      goto err;
    }
    g_mmreg_map.mmdvfs_top_regmap = syscon_node_to_regmap(top_node);
    if (IS_ERR(g_mmreg_map.mmdvfs_top_regmap)) {
      pr_err("No regmap for syscon mmdvfs_top_regmap\n");
      ret = -ENODEV;
      goto err;
    }

    reg_res.start = REGS_MM_AHB_REG_START;
    reg_res.end = REGS_MM_AHB_REG_END;
    reg_base = ioremap(reg_res.start, reg_res.end - reg_res.start + 1);
    if (!reg_base) {
        pr_err("fail to map mm_ahb reg base\n");
        goto err_iounmap;
    }
    g_mmreg_map.mm_ahb_regbase = (unsigned long)reg_base;

    reg_res.start = REGS_MM_POWER_ON_REG_START;
    reg_res.end = REGS_MM_POWER_ON_REG_END;
    reg_base = ioremap(reg_res.start, reg_res.end - reg_res.start + 1);
    if (!reg_base) {
        pr_err("fail to map mm_ahb reg base\n");
        goto err_iounmap;
    }
    g_mmreg_map.mm_power_on_regbase = (unsigned long)reg_base;

    reg_res.start = REGS_MM_POWER_REG_START;
    reg_res.end = REGS_MM_POWER_REG_END;
    reg_base = ioremap(reg_res.start, reg_res.end - reg_res.start + 1);
    if (!reg_base) {
        pr_err("fail to map mm_ahb reg base\n");
        goto err_iounmap;
    }
    g_mmreg_map.mm_power_regbase = (unsigned long)reg_base;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
    ret = dev_pm_opp_of_add_table(dev);
    if (ret) {
        dev_err(dev, "mmsys_dvfs: Invalid operating-points in device tree.\n");
        goto err;
    }
#endif

    platform_set_drvdata(pdev, mmsys);
    mmsys->devfreq =
        devm_devfreq_add_device(dev, &mmsys_dvfs_profile, "mmsys_dvfs", NULL);
    if (IS_ERR(mmsys->devfreq)) {
        dev_err(dev, "failed to add devfreq dev with mmsys-dvfs governor\n");
        ret = PTR_ERR(mmsys->devfreq);
        goto err;
    }

    mmsys->pw_nb.priority = 0;
    mmsys->pw_nb.notifier_call = mmsys_dvfs_notify_callback;
    ret = mmsys_register_notifier(&mmsys->pw_nb);

    return MM_DVFS_SUCCESS;

err_iounmap:
err:
    if (top_node)
      of_node_put(top_node);
    return ret;
}

static int mmsys_dvfs_power(struct notifier_block *self, unsigned long event,
                            void *ptr) {
    int ret = MM_DVFS_SUCCESS;
    pr_err("%s:on=%ld begin", __func__, event);
    if (event == _E_PW_ON)
        mmsys_notify_call_chain(MMSYS_POWER_ON);
    else
        mmsys_notify_call_chain(MMSYS_POWER_OFF);

    pr_err("%s: end", __func__);

    return ret;
}

static struct notifier_block dvfs_power_notifier = {
    .notifier_call = mmsys_dvfs_power,
};

/*static int  mmsys_dvfs_power(int on, void *pDat)
{
    int ret = 0;
        pr_err("%s:on=%d ", __func__,on);
        if(on==MMSYS_POWER_ON)
                mmsys_notify_call_chain(MMSYS_POWER_ON);
        else
                mmsys_notify_call_chain(MMSYS_POWER_OFF);

        pr_err("%s:", __func__);

    return ret;

}*/

static int mmsys_dvfs_remove(struct platform_device *pdev) {
    pr_err("%s:", __func__);
    return MM_DVFS_SUCCESS;
}

#ifndef KO_DEFINE

int mmsys_dvfs_init(void) {
    int ret = MM_DVFS_SUCCESS;

    ret = register_ip_dvfs_ops(&mmsys_dvfs_ops);
    if (ret) {
        pr_err("%s: failed to add ops: %d\n", __func__, ret);
        return ret;
    }

    ret = sprd_mm_pw_notify_register(&dvfs_power_notifier);
    if (ret) {
        pr_err("%s: failed to add dvfs_power: %d\n", __func__, ret);
        return ret;
    }

    ret = devfreq_add_governor(&mmsys_dvfs_gov);
    if (ret) {
        pr_err("%s: failed to add governor: %d\n", __func__, ret);
        return ret;
    }

    ret = platform_driver_register(&mmsys_dvfs_driver);

    if (ret)
        devfreq_remove_governor(&mmsys_dvfs_gov);

    return ret;
}

void mmsys_dvfs_exit(void) {
    int ret = MM_DVFS_SUCCESS;

    platform_driver_unregister(&mmsys_dvfs_driver);

    ret = devfreq_remove_governor(&mmsys_dvfs_gov);
    if (ret)
        pr_err("%s: failed to remove governor: %d\n", __func__, ret);

    ret = unregister_ip_dvfs_ops(&mmsys_dvfs_ops);
    if (ret)
        pr_err("%s: failed to remove ops: %d\n", __func__, ret);
    ret = sprd_mm_pw_notify_unregister((&dvfs_power_notifier));
    if (ret)
       pr_err("%s: failed to remove mm_dvfs_power: %d\n", __func__, ret);
}

#else

int __init mmsys_dvfs_init(void) {
    int ret = MM_DVFS_SUCCESS;

    ret = register_ip_dvfs_ops(&mmsys_dvfs_ops);
    if (ret) {
        pr_err("%s: failed to add ops: %d\n", __func__, ret);
        return ret;
    }

    ret = devfreq_add_governor(&mmsys_dvfs_gov);
    if (ret) {
        pr_err("%s: failed to add governor: %d\n", __func__, ret);
        return ret;
    }

    ret = platform_driver_register(&mmsys_dvfs_driver);

    if (ret)
        devfreq_remove_governor(&mmsys_dvfs_gov);

    return ret;
}

void __exit mmsys_dvfs_exit(void) {
    int ret = MM_DVFS_SUCCESS;

    platform_driver_unregister(&mmsys_dvfs_driver);

    ret = devfreq_remove_governor(&mmsys_dvfs_gov);
    if (ret)
        pr_err("%s: failed to remove governor: %d\n", __func__, ret);

    ret = unregister_ip_dvfs_ops(&mmsys_dvfs_ops);
    if (ret)
        pr_err("%s: failed to remove ops: %d\n", __func__, ret);
}

#endif

#ifndef KO_DEFINE

int __init mmsys_module_init(void) {
    int ret = MM_DVFS_SUCCESS;

    pr_err("enter mmsys_moudule_init \n");

    ret = mmsys_dvfs_init();
    ret = cpp_dvfs_init();
    ret = fd_dvfs_init();
    ret = isp_dvfs_init();
    ret = jpg_dvfs_init();
    ret = dcam_if_dvfs_init();
    ret = dcam_axi_dvfs_init();
    ret = mtx_dvfs_init();
    pr_err("end mmsys_moudule_init \n");

    return ret;
}

void __exit mmsys_module_exit(void) {
    pr_err("enter mmsys_moudule_exit \n");
    fd_dvfs_exit();
    cpp_dvfs_exit();
    isp_dvfs_exit();
    jpg_dvfs_exit();
    dcam_if_dvfs_exit();
    dcam_axi_dvfs_exit();
    mtx_dvfs_exit();

    mmsys_dvfs_exit();
    pr_err("end mmsys_moudule_exit \n");
}

module_init(mmsys_module_init);
module_exit(mmsys_module_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Sprd mmsys devfreq driver");
MODULE_AUTHOR("Multimedia_Camera@UNISOC");

#endif
