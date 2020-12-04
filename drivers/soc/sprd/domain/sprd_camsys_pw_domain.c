// SPDX-License-Identifier: GPL-2.0
//
// Spreatrum UMS512 camera pd driver
//
// Copyright (C) 2018 Spreadtrum, Inc.
// Author: Hongjian Wang <hongjian.wang@spreadtrum.com>


#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/kthread.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/pm_domain.h>
#include <asm/cacheflush.h>
#include <linux/notifier.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd_campd: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define PD_MM_DOWN_FLAG    0x7
#define ARQOS_THRESHOLD    0x0D
#define AWQOS_THRESHOLD    0x0D
#define SHIFT_MASK(a)      (ffs(a) ? ffs(a) - 1 : 0)

enum {
	_E_PW_OFF = 0,
	_E_PW_ON  = 1,
};

static const char * const syscon_name[] = {
	"force-shutdown",
	"shutdown-en", /* clear */
	"power-state", /* on: 0; off:7 */
	"qos-ar",
	"qos-aw",
};

enum  {
	FORCE_SHUTDOWN = 0,
	SHUTDOWN_EN,
	PWR_STATUS0,
	QOS_AR,
	QOS_AW,
};

struct register_gpr {
	struct regmap *gpr;
	uint32_t reg;
	uint32_t mask;
};

struct camsys_power_info {
	atomic_t users_pw;
	atomic_t users_clk;
	atomic_t inited;
	uint8_t mm_qos_ar;
	uint8_t mm_qos_aw;
	struct mutex mlock;

	struct clk *cam_mm_eb;
	struct clk *cam_mm_ahb_eb;

	struct clk *cam_ahb_clk;
	struct clk *cam_ahb_clk_parent;
	struct clk *cam_ahb_clk_default;

	struct clk *cam_mtx_clk;
	struct clk *cam_mtx_clk_parent;
	struct clk *cam_mtx_clk_default;

	struct clk *isppll_clk;

	struct generic_pm_domain pd;
	struct register_gpr regs[ARRAY_SIZE(syscon_name)];
};

static struct camsys_power_info *pw_info;
static BLOCKING_NOTIFIER_HEAD(mmsys_chain);

static int mmsys_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&mmsys_chain, val, v);
}

static void regmap_update_bits_mmsys(struct register_gpr *p, uint32_t val)
{
	if ((!p) || (!(p->gpr)))
		return;

	regmap_update_bits(p->gpr, p->reg, p->mask, val);
}

static int regmap_read_mmsys(struct register_gpr *p, uint32_t *val)
{
	int ret = 0;

	if ((!p) || (!(p->gpr)) || (!val))
		return -1;
	ret = regmap_read(p->gpr, p->reg, val);
	if (!ret)
		*val &= (uint32_t)p->mask;

	return ret;
}

static int check_drv_init(void)
{
	int ret = 0;

	if (!pw_info)
		ret = -1;
	if (atomic_read(&pw_info->inited) == 0)
		ret = -2;

	return ret;
}

static int sprd_cam_domain_eb(void)
{
	int ret = 0;
	uint32_t tmp = 0;

	ret = check_drv_init();
	if (ret) {
		pr_err("fail to get init state %d, cb %p, ret %d\n",
			atomic_read(&pw_info->users_pw),
			__builtin_return_address(0), ret);
		return -ENODEV;
	}

	pr_info("users count %d, cb %p\n",
		atomic_read(&pw_info->users_clk),
		__builtin_return_address(0));

	mutex_lock(&pw_info->mlock);
	if (atomic_inc_return(&pw_info->users_clk) == 1) {
		/* mm bus enable */
		clk_prepare_enable(pw_info->cam_mm_eb);
		clk_prepare_enable(pw_info->cam_mm_ahb_eb);
		/* config cam ahb clk */
		clk_set_parent(pw_info->cam_ahb_clk,
			pw_info->cam_ahb_clk_parent);
		clk_prepare_enable(pw_info->cam_ahb_clk);

		/* config cam mtx clk */
		clk_set_parent(pw_info->cam_mtx_clk,
			pw_info->cam_mtx_clk_parent);
		clk_prepare_enable(pw_info->cam_mtx_clk);

		clk_prepare_enable(pw_info->isppll_clk);

		/* Qos ar */
		tmp = pw_info->mm_qos_ar;
		regmap_update_bits_mmsys(&pw_info->regs[QOS_AR],
			tmp << SHIFT_MASK(pw_info->regs[QOS_AR].mask));
		/* Qos aw */
		tmp = pw_info->mm_qos_aw;
		regmap_update_bits_mmsys(&pw_info->regs[QOS_AW],
			tmp << SHIFT_MASK(pw_info->regs[QOS_AW].mask));

		mmsys_notifier_call_chain(_E_PW_ON, NULL);
	}
	mutex_unlock(&pw_info->mlock);
	return ret;
}

static int sprd_cam_domain_disable(void)
{
	int ret = 0;

	ret = check_drv_init();
	if (ret) {
		pr_err("fail to get init state %d, cb %p, ret %d\n",
			atomic_read(&pw_info->users_pw),
			__builtin_return_address(0), ret);
	}

	pr_info("users count %d, cb %p\n",
		atomic_read(&pw_info->users_clk),
		__builtin_return_address(0));

	mutex_lock(&pw_info->mlock);
	if (atomic_dec_return(&pw_info->users_clk) == 0) {
		mmsys_notifier_call_chain(_E_PW_OFF, NULL);

		clk_disable_unprepare(pw_info->isppll_clk);

		clk_set_parent(pw_info->cam_ahb_clk,
			pw_info->cam_ahb_clk_default);
		clk_disable_unprepare(pw_info->cam_ahb_clk);

		clk_set_parent(pw_info->cam_mtx_clk,
			pw_info->cam_mtx_clk_default);
		clk_disable_unprepare(pw_info->cam_mtx_clk);

		clk_disable_unprepare(pw_info->cam_mm_ahb_eb);
		clk_disable_unprepare(pw_info->cam_mm_eb);
	}
	mutex_unlock(&pw_info->mlock);
	return ret;
}

static int sprd_cam_pw_off(struct generic_pm_domain *domain)
{
	int ret = 0;
	unsigned int power_state1;
	unsigned int power_state2;
	unsigned int power_state3;
	unsigned int read_count = 0;
	int shift = 0;

	ret = check_drv_init();
	if (ret) {
		pr_err("fail to check drv init. uses: %d, cb: %p, ret %d\n",
			atomic_read(&pw_info->users_pw),
			__builtin_return_address(0), ret);
		return -ENODEV;
	}
	sprd_cam_domain_disable();

	mutex_lock(&pw_info->mlock);
	if (atomic_dec_return(&pw_info->users_pw) == 0) {
		/* 1:auto shutdown en, shutdown with ap; 0: control by b25 */
		regmap_update_bits_mmsys(&pw_info->regs[SHUTDOWN_EN],
			0);
		/* set 1 to shutdown */
		regmap_update_bits_mmsys(&pw_info->regs[FORCE_SHUTDOWN],
			~((uint32_t)0));
		/* shift for power off status bits */
		if (pw_info->regs[PWR_STATUS0].gpr != NULL)
			shift = SHIFT_MASK(pw_info->regs[PWR_STATUS0].mask);
		do {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;

			ret = regmap_read_mmsys(&pw_info->regs[PWR_STATUS0],
					&power_state1);
			if (ret)
				goto err_pw_off;
			ret = regmap_read_mmsys(&pw_info->regs[PWR_STATUS0],
					&power_state2);
			if (ret)
				goto err_pw_off;
			ret = regmap_read_mmsys(&pw_info->regs[PWR_STATUS0],
					&power_state3);
			if (ret)
				goto err_pw_off;
		} while (((power_state1 != (PD_MM_DOWN_FLAG << shift)) &&
			(read_count < 30)) ||
				(power_state1 != power_state2) ||
				(power_state2 != power_state3));
		if (power_state1 != (PD_MM_DOWN_FLAG << shift)) {
			pr_err("fail to get power_state1 : 0x%x\n", power_state1);
			ret = -1;
			goto err_pw_off;
		}
	}
	mutex_unlock(&pw_info->mlock);
	/* if count != 0, other using */
	pr_info("Done, read count %d, cb: %p\n",
		read_count, __builtin_return_address(0));
	return ret;

err_pw_off:
	mutex_unlock(&pw_info->mlock);
	pr_err("fail to pw off. ret: %d, count: %d, cb: %p\n", ret, read_count,
		__builtin_return_address(0));

	return ret;
}

static int sprd_cam_pw_on(struct generic_pm_domain *domain)
{
	int ret = 0;
	unsigned int power_state1;
	unsigned int power_state2;
	unsigned int power_state3;
	unsigned int read_count = 0;

	ret = check_drv_init();
	if (ret) {
		pr_err("fail to check drv init: %d, cb: %p, ret %d\n",
			atomic_read(&pw_info->users_pw),
			__builtin_return_address(0), ret);
		return -ENODEV;
	}

	mutex_lock(&pw_info->mlock);
	if (atomic_inc_return(&pw_info->users_pw) == 1) {
		/* clear force shutdown */
		regmap_update_bits_mmsys(&pw_info->regs[FORCE_SHUTDOWN], 0);
		/* power on */
		regmap_update_bits_mmsys(&pw_info->regs[SHUTDOWN_EN], 0);

		do {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;

			ret = regmap_read_mmsys(&pw_info->regs[PWR_STATUS0],
					&power_state1);
			if (ret)
				goto err_pw_on;
			ret = regmap_read_mmsys(&pw_info->regs[PWR_STATUS0],
					&power_state2);
			if (ret)
				goto err_pw_on;
			ret = regmap_read_mmsys(&pw_info->regs[PWR_STATUS0],
					&power_state3);
			if (ret)
				goto err_pw_on;
		} while ((power_state1 && read_count < 30) ||
				(power_state1 != power_state2) ||
				(power_state2 != power_state3));

		if (power_state1) {
			pr_err("fail to pw on cam domain. power_state1 : 0x%x\n", power_state1);
			ret = -1;
			goto err_pw_on;
		}
	}
	mutex_unlock(&pw_info->mlock);
	/* if count != 0, other using */
	pr_info("Done, uses: %d, read count %d, cb: %p\n",
		atomic_read(&pw_info->users_pw), read_count,
		__builtin_return_address(0));
	sprd_cam_domain_eb();
	return ret;
err_pw_on:
	atomic_dec_return(&pw_info->users_pw);
	mutex_unlock(&pw_info->mlock);
	pr_err("fail to power on, ret = %d\n", ret);
	return ret;
}

static int sprd_campw_init(struct platform_device *pdev)
{
	int i, ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *np_qos;

	const char *pname;
	struct regmap *tregmap;
	uint32_t args[2];

	pw_info = devm_kzalloc(&pdev->dev, sizeof(*pw_info), GFP_KERNEL);
	if (IS_ERR_OR_NULL(pw_info)) {
		pr_err("fail to alloc pw_info\n");
		return -ENOMEM;
	}

	pw_info->cam_mm_eb =
		of_clk_get_by_name(np, "clk_mm_eb");
	if (IS_ERR_OR_NULL(pw_info->cam_mm_eb))
		return PTR_ERR(pw_info->cam_mm_eb);

	pw_info->cam_mm_ahb_eb =
		of_clk_get_by_name(np, "clk_mm_ahb_eb");
	if (IS_ERR_OR_NULL(pw_info->cam_mm_ahb_eb))
		return PTR_ERR(pw_info->cam_mm_ahb_eb);

	pw_info->cam_ahb_clk =
		of_clk_get_by_name(np, "clk_mm_ahb");
	if (IS_ERR_OR_NULL(pw_info->cam_ahb_clk))
		return PTR_ERR(pw_info->cam_ahb_clk);

	pw_info->cam_ahb_clk_parent =
		of_clk_get_by_name(np, "clk_mm_ahb_parent");
	if (IS_ERR_OR_NULL(pw_info->cam_ahb_clk_parent))
		return PTR_ERR(pw_info->cam_ahb_clk_parent);

	pw_info->cam_ahb_clk_default =
		clk_get_parent(pw_info->cam_ahb_clk);
	if (IS_ERR_OR_NULL(pw_info->cam_ahb_clk_default))
		return PTR_ERR(pw_info->cam_ahb_clk_default);

	/* need set cgm_mm_emc_sel :512m , DDR  matrix clk*/
	pw_info->cam_mtx_clk =
		of_clk_get_by_name(np, "clk_mm_mtx");
	if (IS_ERR_OR_NULL(pw_info->cam_mtx_clk))
		return PTR_ERR(pw_info->cam_mtx_clk);

	pw_info->cam_mtx_clk_parent =
		of_clk_get_by_name(np, "clk_mm_mtx_parent");
	if (IS_ERR_OR_NULL(pw_info->cam_mtx_clk_parent))
		return PTR_ERR(pw_info->cam_mtx_clk_parent);

	pw_info->cam_mtx_clk_default =
		clk_get_parent(pw_info->cam_mtx_clk);
	if (IS_ERR_OR_NULL(pw_info->cam_mtx_clk_default))
		return PTR_ERR(pw_info->cam_mtx_clk_default);

	pw_info->isppll_clk = of_clk_get_by_name(np, "clk_isppll");
	if (IS_ERR_OR_NULL(pw_info->isppll_clk))
		return PTR_ERR(pw_info->isppll_clk);

	/* read global register */
	for (i = 0; i < ARRAY_SIZE(syscon_name); i++) {
		pname = syscon_name[i];
		tregmap = syscon_regmap_lookup_by_phandle_args(np, pname, 2, args);
		if (IS_ERR_OR_NULL(tregmap)) {
			pr_err("fail to read %s reg_map %p %d\n",
					pname, tregmap, IS_ERR_OR_NULL(tregmap));
			continue;
		}
		pw_info->regs[i].gpr = tregmap;
		pw_info->regs[i].reg = args[0];
		pw_info->regs[i].mask = args[1];
		pr_debug("dts[%s] 0x%x 0x%x\n", pname,
			pw_info->regs[i].reg,
			pw_info->regs[i].mask);
	}

	np_qos = of_parse_phandle(np, "mm_qos_threshold", 0);
	if (!IS_ERR_OR_NULL(np_qos)) {
		/* read qos ar aw */
		ret = of_property_read_u8(np_qos, "arqos-threshold",
			&pw_info->mm_qos_ar);
		if (ret) {
			pw_info->mm_qos_ar = ARQOS_THRESHOLD;
			pr_warn("fail to read arqos-threshold, default %d\n",
				pw_info->mm_qos_ar);
		}
		ret = of_property_read_u8(np_qos, "awqos-threshold",
			&pw_info->mm_qos_aw);
		if (ret) {
			pw_info->mm_qos_aw = AWQOS_THRESHOLD;
			pr_warn("fail to read awqos-threshold, default %d\n",
				pw_info->mm_qos_aw);
		}
	} else {
		pw_info->mm_qos_ar = ARQOS_THRESHOLD;
		pw_info->mm_qos_aw = AWQOS_THRESHOLD;
		pr_warn("fail to read mm qos threshold. Use default[%x %x]\n",
			pw_info->mm_qos_ar, pw_info->mm_qos_aw);
	}

	pw_info->pd.name = kstrdup(np->name, GFP_KERNEL);
	pw_info->pd.power_off = sprd_cam_pw_off;
	pw_info->pd.power_on = sprd_cam_pw_on;
	pm_genpd_init(&pw_info->pd, NULL, true);
	of_genpd_add_provider_simple(np, &pw_info->pd);
	mutex_init(&pw_info->mlock);
	atomic_set(&pw_info->inited, 1);
	return ret;
}

static void sprd_campw_deinit(struct platform_device *pdev)
{
	pm_genpd_remove(&pw_info->pd);
	devm_kfree(&pdev->dev, pw_info);
}

static int sprd_campw_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = sprd_campw_init(pdev);
	if (ret) {
		pr_err("fail to init cam power domain\n");
		return -ENODEV;
	}

	return ret;
}

static int sprd_campw_remove(struct platform_device *pdev)
{
	sprd_campw_deinit(pdev);

	return 0;
}

static const struct of_device_id sprd_campw_match_table[] = {
	{ .compatible = "sprd,camsys-domain", },
	{},
};

static struct platform_driver sprd_campw_driver = {
	.probe = sprd_campw_probe,
	.remove = sprd_campw_remove,
	.driver = {
		.name = "sprd_campd",
		.of_match_table = sprd_campw_match_table,
	},
};
module_platform_driver(sprd_campw_driver);

MODULE_DESCRIPTION("Camsys Power Domain Driver");
MODULE_AUTHOR("hongjian.wang@unisoc.com");
MODULE_LICENSE("GPL v2");
