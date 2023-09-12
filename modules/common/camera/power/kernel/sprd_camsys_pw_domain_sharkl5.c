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
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <linux/pm_runtime.h>
#include <linux/ion.h>
#else
#include <video/sprd_mmsys_pw_domain.h>
#include "ion.h"
#endif
#include "sprd_camsys_domain.h"

/* Macro Definitions */
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd_campd_r6p0: %d %d %s: " \
	fmt, current->pid, __LINE__, __func__

static const char * const syscon_name[] = {
	"force_shutdown",
	"shutdown_en", /* clear */
	"power_state", /* on: 0; off:7 */
	"qos_ar",
	"qos_aw",
	"aon-apb-mm-eb",
};
enum  {
	_e_force_shutdown,
	_e_auto_shutdown,
	_e_power_state,
	_e_qos_ar,
	_e_qos_aw,
	_e_camsys_mm_eb,
};

#define PD_MM_DOWN_FLAG			0x7
#define ARQOS_THRESHOLD			0x0D
#define AWQOS_THRESHOLD			0x0D
#define SHIFT_MASK(a)			(ffs(a) ? ffs(a) - 1 : 0)

static BLOCKING_NOTIFIER_HEAD(mmsys_chain);

static int boot_mode_check(void)
{
	struct device_node *np;
	const char *cmd_line;
	int ret = 0;

	np = of_find_node_by_path("/chosen");
	if (!np)
		return 0;

	ret = of_property_read_string(np, "bootargs", &cmd_line);
	if (ret < 0)
		return 0;

	if (strstr(cmd_line, "androidboot.mode=cali"))
		ret = 1;

	return ret;
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

static int sprd_cam_domain_eb(struct camsys_power_info *pw_info)
{
	uint32_t tmp = 0;

	/* enable */
	clk_prepare_enable(pw_info->u.l5.mm_eb);
	clk_prepare_enable(pw_info->u.l5.mm_ahb_eb);
	clk_prepare_enable(pw_info->u.l5.mm_mtx_eb);
	/* ahb clk */
	clk_set_parent(pw_info->u.l5.ahb_clk, pw_info->u.l5.ahb_clk_parent);
	clk_prepare_enable(pw_info->u.l5.ahb_clk);
	/* mm mtx clk */
	clk_set_parent(pw_info->u.l5.mtx_clk, pw_info->u.l5.mtx_clk_parent);
	clk_prepare_enable(pw_info->u.l5.mtx_clk);

	/* Qos ar */
	tmp = pw_info->u.l5.mm_qos_ar;
	regmap_update_bits_mmsys(&pw_info->u.l5.syscon_regs[_e_qos_ar],
		tmp << SHIFT_MASK(pw_info->u.l5.syscon_regs[_e_qos_ar].mask));
	/* Qos aw */
	tmp = pw_info->u.l5.mm_qos_aw;
	regmap_update_bits_mmsys(&pw_info->u.l5.syscon_regs[_e_qos_aw],
		tmp << SHIFT_MASK(pw_info->u.l5.syscon_regs[_e_qos_aw].mask));

	return 0;
}


static int sprd_cam_domain_disable(struct camsys_power_info *pw_info)
{
	/* ahb clk */
	clk_set_parent(pw_info->u.l5.ahb_clk, pw_info->u.l5.ahb_clk_default);
	clk_disable_unprepare(pw_info->u.l5.ahb_clk);
	/* mm mtx clk */
	clk_set_parent(pw_info->u.l5.mtx_clk, pw_info->u.l5.mtx_clk_default);
	clk_disable_unprepare(pw_info->u.l5.mtx_clk);
	/* disable */
	clk_disable_unprepare(pw_info->u.l5.mm_mtx_eb);
	clk_disable_unprepare(pw_info->u.l5.mm_ahb_eb);
	clk_disable_unprepare(pw_info->u.l5.mm_eb);

	return 0;
}

static int sprd_cam_pw_off(struct camsys_power_info *pw_info)
{
	int ret = 0;
	unsigned int power_state1 = 0;
	unsigned int power_state2 = 0;
	unsigned int power_state3 = 0;
	unsigned int read_count = 0;
	int shift = 0;

	/* 1:auto shutdown en, shutdown with ap; 0: control by b25 */
	regmap_update_bits_mmsys(&pw_info->u.l5.syscon_regs[_e_auto_shutdown], 0);
	/* set 1 to shutdown */
	regmap_update_bits_mmsys(&pw_info->u.l5.syscon_regs[_e_force_shutdown],
		~((uint32_t)0));
	/* shift for power off status bits */
	if (pw_info->u.l5.syscon_regs[_e_power_state].gpr != NULL)
		shift = SHIFT_MASK(pw_info->u.l5.syscon_regs[_e_power_state].mask);
	do {
		cpu_relax();
		usleep_range(300, 350);
		read_count++;

		ret = regmap_read_mmsys(&pw_info->u.l5.syscon_regs[_e_power_state],
				&power_state1);
		if (ret)
			pr_err("failed, ret: %d, count: %d, cb: %p\n", ret, read_count,
				__builtin_return_address(0));

		ret = regmap_read_mmsys(&pw_info->u.l5.syscon_regs[_e_power_state],
				&power_state2);
		if (ret)
			pr_err("failed, ret: %d, count: %d, cb: %p\n", ret, read_count,
				__builtin_return_address(0));

		ret = regmap_read_mmsys(&pw_info->u.l5.syscon_regs[_e_power_state],
				&power_state3);
		if (ret)
			pr_err("failed, ret: %d, count: %d, cb: %p\n", ret, read_count,
				__builtin_return_address(0));

	} while (((power_state1 != (PD_MM_DOWN_FLAG << shift)) &&
		(read_count < 10)) ||
			(power_state1 != power_state2) ||
			(power_state2 != power_state3));
	if (power_state1 != (PD_MM_DOWN_FLAG << shift)) {
		pr_err("failed, power_state1=0x%x\n", power_state1);
		ret = -1;
		pr_err("failed, ret: %d, count: %d, cb: %p\n", ret, read_count,
			__builtin_return_address(0));

	}
	pr_info("Done, read count %d, cb: %p\n",
		read_count, __builtin_return_address(0));

	return 0;
}


static int sprd_cam_pw_on(struct camsys_power_info *pw_info)
{
	int ret = 0;
	unsigned int power_state1= 0;
	unsigned int power_state2= 0;
	unsigned int power_state3= 0;
	unsigned int read_count = 0;

	/* clear force shutdown */
	regmap_update_bits_mmsys(&pw_info->u.l5.syscon_regs[_e_force_shutdown], 0);
	/* power on */
	regmap_update_bits_mmsys(&pw_info->u.l5.syscon_regs[_e_auto_shutdown], 0);

	do {
		cpu_relax();
		usleep_range(300, 350);
		read_count++;

		ret = regmap_read_mmsys(&pw_info->u.l5.syscon_regs[_e_power_state],
				&power_state1);
		if (ret)
			pr_info("cam domain, failed to power on, ret = %d\n", ret);


		ret = regmap_read_mmsys(&pw_info->u.l5.syscon_regs[_e_power_state],
				&power_state2);
		if (ret)
			pr_info("cam domain, failed to power on, ret = %d\n", ret);

		ret = regmap_read_mmsys(&pw_info->u.l5.syscon_regs[_e_power_state],
				&power_state3);
		if (ret)
			pr_info("cam domain, failed to power on, ret = %d\n", ret);

	} while ((power_state1 && read_count < 10) ||
			(power_state1 != power_state2) ||
			(power_state2 != power_state3));

	if (power_state1) {
		pr_err("cam domain pw on failed 0x%x\n", power_state1);
		ret = -1;
		pr_info("cam domain, failed to power on, ret = %d\n", ret);

	}
	pr_info("Done, read count %d, cb: %pS\n",
		read_count, __builtin_return_address(0));

	return 0;
}

static long sprd_campw_init(struct platform_device *pdev, struct camsys_power_info *pw_info)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *np_qos;
	struct regmap *tregmap;
	uint32_t args[2];
	const char *pname;
	int i = 0;
	int ret = 0;

	pr_info("E\n");
	/* read global register */
	for (i = 0; i < ARRAY_SIZE(syscon_name); i++) {
		pname = syscon_name[i];
		tregmap =  syscon_regmap_lookup_by_phandle_args(np, pname, 2, args);
		if (IS_ERR_OR_NULL(tregmap)) {
			pr_err("fail to read %s regmap\n", pname);
			continue;
		}

		pw_info->u.l5.syscon_regs[i].gpr = tregmap;
		pw_info->u.l5.syscon_regs[i].reg = args[0];
		pw_info->u.l5.syscon_regs[i].mask = args[1];
		pr_info("DTS[%s]%p, 0x%x, 0x%x\n", pname,
			pw_info->u.l5.syscon_regs[i].gpr, pw_info->u.l5.syscon_regs[i].reg,
			pw_info->u.l5.syscon_regs[i].mask);
	}
	np_qos = of_parse_phandle(np, "mm_qos_threshold", 0);
	if (!IS_ERR_OR_NULL(np_qos)) {
		/* read qos ar aw */
		ret = of_property_read_u8(np_qos, "arqos-threshold",
			&pw_info->u.l5.mm_qos_ar);
		if (ret) {
			pw_info->u.l5.mm_qos_ar = ARQOS_THRESHOLD;
			pr_warn("read arqos-threshold fail, default %d\n",
				pw_info->u.l5.mm_qos_ar);
		}
		ret = of_property_read_u8(np_qos, "awqos-threshold",
			&pw_info->u.l5.mm_qos_aw);
		if (ret) {
			pw_info->u.l5.mm_qos_aw = AWQOS_THRESHOLD;
			pr_warn("read awqos-threshold fail, default %d\n",
				pw_info->u.l5.mm_qos_aw);
		}
	} else {
		pw_info->u.l5.mm_qos_ar = ARQOS_THRESHOLD;
		pw_info->u.l5.mm_qos_aw = AWQOS_THRESHOLD;
		pr_warn("read mm qos threshold fail, default[%x %x]\n",
			pw_info->u.l5.mm_qos_ar, pw_info->u.l5.mm_qos_aw);
	}

	ret = 0;
	/* read clk */
	pw_info->u.l5.mm_eb = of_clk_get_by_name(np, "mm_eb");
	if (IS_ERR_OR_NULL(pw_info->u.l5.mm_eb)) {
		pr_err("Get mm_eb clk fail, ret %d\n",
			(int)PTR_ERR(pw_info->u.l5.mm_eb));
		ret |= BIT(0);
	}
	pw_info->u.l5.mm_ahb_eb = of_clk_get_by_name(np, "mm_ahb_eb");
	if (IS_ERR_OR_NULL(pw_info->u.l5.mm_ahb_eb)) {
		pr_err("Get mm_ahb_eb clk fail, ret %d\n",
			(int)PTR_ERR(pw_info->u.l5.mm_ahb_eb));
		ret |= BIT(1);
	}
	pw_info->u.l5.ahb_clk = of_clk_get_by_name(np, "clk_mm_ahb");
	if (IS_ERR_OR_NULL(pw_info->u.l5.ahb_clk)) {
		pr_err("Get clk_mm_ahb clk fail, ret %d\n",
			(int)PTR_ERR(pw_info->u.l5.ahb_clk));
		ret |= BIT(2);
	}
	pw_info->u.l5.ahb_clk_parent = of_clk_get_by_name(np, "clk_mm_ahb_parent");
	if (IS_ERR_OR_NULL(pw_info->u.l5.ahb_clk_parent)) {
		pr_err("Get mm_ahb_parent clk fail, ret %d\n",
			(int)PTR_ERR(pw_info->u.l5.ahb_clk_parent));
		ret |= BIT(3);
	}
	pw_info->u.l5.ahb_clk_default = clk_get_parent(pw_info->u.l5.ahb_clk);
	/* read mm mtx clk */
	pw_info->u.l5.mm_mtx_eb = of_clk_get_by_name(np, "mm_mtx_eb");
	if (IS_ERR_OR_NULL(pw_info->u.l5.mm_mtx_eb)) {
		pr_err("Get mm_mtx_eb clk fail, ret %d\n",
			(int)PTR_ERR(pw_info->u.l5.mm_mtx_eb));
		ret |= BIT(4);
	}
	pw_info->u.l5.mtx_clk = of_clk_get_by_name(np, "clk_mm_mtx");
	if (IS_ERR_OR_NULL(pw_info->u.l5.mtx_clk)) {
		pr_err("Get clk_mm_mtx clk fail, ret %d\n",
			(int)PTR_ERR(pw_info->u.l5.mtx_clk));
		ret |= BIT(5);
		}
	pw_info->u.l5.mtx_clk_parent = of_clk_get_by_name(np, "clk_mm_mtx_parent");
	if (IS_ERR_OR_NULL(pw_info->u.l5.mtx_clk_parent)) {
		pr_err("Get clk_mm_mtx_parent clk fail, ret %d\n",
			(int)PTR_ERR(pw_info->u.l5.mtx_clk_parent));
		ret |= BIT(6);
	}
	pw_info->u.l5.mtx_clk_default = clk_get_parent(pw_info->u.l5.mtx_clk);
	if (ret) {
		atomic_set(&pw_info->inited, 0);
		pr_err("ret = 0x%x\n", ret);
	} else {
		atomic_set(&pw_info->inited, 1);
		pr_info("Read DTS OK\n");
	}

	//fix bug 1915366
	if (boot_mode_check()) {
		regmap_update_bits_mmsys(&pw_info->u.l5.syscon_regs[_e_camsys_mm_eb],
				0);
		regmap_update_bits_mmsys(&pw_info->u.l5.syscon_regs[_e_auto_shutdown],
				0);
		regmap_update_bits_mmsys(&pw_info->u.l5.syscon_regs[_e_force_shutdown],
				~((uint32_t)0));
		pr_info("calibration mode MM SHUTDOWN");
	}
	return ret;
}

struct camsys_power_ops camsys_power_ops_l5 = {
	.sprd_campw_init = sprd_campw_init,
	.sprd_cam_pw_on = sprd_cam_pw_on,
	.sprd_cam_pw_off = sprd_cam_pw_off,
	.sprd_cam_domain_eb = sprd_cam_domain_eb,
	.sprd_cam_domain_disable = sprd_cam_domain_disable,
};
