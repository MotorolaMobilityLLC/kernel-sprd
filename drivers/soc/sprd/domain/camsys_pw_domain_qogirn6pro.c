/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
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
#include <linux/kthread.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include <video/sprd_mmsys_pw_domain.h>
#include <video/sprd_mmsys_pw_domain_qogirn6pro.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "cam_sys_pw: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define PD_MM_DOWN_FLAG            0x7
#define PD_MM_DOWN_BIT             24
#define PD_MM_STATUS_SHIFT_BIT     24

#define PD_ISP_STATUS_SHIFT_BIT    16
#define PD_DCAM_STATUS_SHIFT_BIT   0

#define AON_APB_AHB_EN             0x64900000
#define MM_AHB_SYS_EN              0x30000000
#define PW_ON_HAPS

static const char * const syscon_name[] = {
	"force_shutdown",
	"shutdown_en",
	"camera_power_state",
	"isp_force_shutdown",
	"isp_shutdown_en",
	"isp_power_state",
	"dcam_force_shutdown",
	"dcam_shutdown_en",
	"dcam_power_state"
};

enum  {
	CAMSYS_FORCE_SHUTDOWN = 0,
	CAMSYS_SHUTDOWN_EN,
	CAMSYS_PWR_STATUS,
	CAMSYS_ISP_FORCE_SHUTDOWN,
	CAMSYS_ISP_SHUTDOWN_EN,
	CAMSYS_ISP_STATUS,
	CAMSYS_DCAM_FORCE_SHUTDOWN,
	CAMSYS_DCAM_SHUTDOWN_EN,
	CAMSYS_DCAM_STATUS,
};

struct register_gpr {
	struct regmap *gpr;
	uint32_t reg;
	uint32_t mask;
};

struct camsys_power_info {
	atomic_t users_pw;
	atomic_t users_dcam_pw;
	atomic_t users_isp_pw;
	atomic_t users_clk;
	atomic_t inited;
	struct mutex mlock;

	uint32_t mm_eb;
	uint32_t mm_mtx_data_en;
	uint32_t dvfs_en;
	uint32_t ckg_en;

	struct register_gpr regs[ARRAY_SIZE(syscon_name)];
};

static struct camsys_power_info *pw_info;

static unsigned int reg_rd(unsigned int addr)
{
	void __iomem *io_tmp = NULL;
	unsigned int val;

	io_tmp = ioremap_nocache(addr, 0x4);
	val = __raw_readl(io_tmp);
	iounmap(io_tmp);

	return val;
}

static void reg_awr(unsigned int addr, unsigned int val)
{
	void __iomem *io_tmp = NULL;
	unsigned int tmp;

	io_tmp = ioremap_nocache(addr, 0x4);
	tmp = __raw_readl(io_tmp);
	__raw_writel(tmp&val, io_tmp);
	mb();/* asm/barrier.h */
	val = __raw_readl(io_tmp);
	iounmap(io_tmp);
}

static void reg_owr(unsigned int addr, unsigned int val)
{
	void __iomem *io_tmp = NULL;
	unsigned int tmp;

	io_tmp = ioremap_nocache(addr, 0x4);
	tmp = *(volatile u32*)(io_tmp);
	*(volatile u32*)(io_tmp) = tmp|val;
	mb();/* asm/barrier.h */
	val = __raw_readl(io_tmp);
	iounmap(io_tmp);
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

static int sprd_campw_check_drv_init(void)
{
	int ret = 0;

	if (!pw_info)
		ret = -1;
	if (atomic_read(&pw_info->inited) == 0)
		ret = -2;

	return ret;
}

static int sprd_campw_init(struct platform_device *pdev)
{
	int i, ret = 0, val = 0;
	struct device_node *np = pdev->dev.of_node;
	const char *pname;
	struct regmap *tregmap;
	uint32_t args[2];

	pw_info = devm_kzalloc(&pdev->dev, sizeof(*pw_info), GFP_KERNEL);
	if (!pw_info)
		return -ENOMEM;

	pw_info->mm_eb = 0x200;

	pw_info->mm_mtx_data_en = 0x100;

	pw_info->dvfs_en = 0x8;

	pw_info->ckg_en = 0x2;

	/* read global register */
	for (i = 0; i < ARRAY_SIZE(syscon_name); i++) {
		pname = syscon_name[i];
		tregmap =  syscon_regmap_lookup_by_name(np, pname);
		if (IS_ERR_OR_NULL(tregmap)) {
			pr_err("fail to read %s regmap\n", pname);
			continue;
		}
		val = syscon_get_args_by_name(np, pname, 2, args);
		if (val != 2) {
			pr_err("fail to read %s args, ret %d\n",
				pname, val);
			ret = -1;
			continue;
		}
		pw_info->regs[i].gpr = tregmap;
		pw_info->regs[i].reg = args[0];
		pw_info->regs[i].mask = args[1];
		pr_info("dts[%s] 0x%x 0x%x\n", pname,
			pw_info->regs[i].reg,
			pw_info->regs[i].mask);
	}

	mutex_init(&pw_info->mlock);
	atomic_set(&pw_info->inited, 1);

	return ret;
}

int sprd_isp_pw_off(void)
{
	int ret = 0;
	unsigned int power_state1 = 0;
	unsigned int power_state2 = 0;
	unsigned int power_state3 = 0;

	unsigned int read_count = 0;

	ret = sprd_campw_check_drv_init();
	if (ret) {
		pr_err("fail to get init state %d, cb %p, ret %d\n",
			atomic_read(&pw_info->users_isp_pw),
			__builtin_return_address(0), ret);
		return -ENODEV;
	}

	mutex_lock(&pw_info->mlock);
	if (atomic_dec_return(&pw_info->users_isp_pw) == 0) {

		usleep_range(300, 350);

		/* cam domain power off */
		/* 1:auto shutdown en, shutdown with ap; 0: control by b25 */
		regmap_update_bits_mmsys(&pw_info->regs[CAMSYS_ISP_SHUTDOWN_EN],
			0);
		/* set 1 to shutdown */
		regmap_update_bits_mmsys(
			&pw_info->regs[CAMSYS_ISP_FORCE_SHUTDOWN],
			~((uint32_t)0));

		do {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;

			ret = regmap_read_mmsys(
					&pw_info->regs[CAMSYS_ISP_STATUS],
					&power_state1);
			if (ret)
				goto err_isp_pw_off;
			ret = regmap_read_mmsys(
					&pw_info->regs[CAMSYS_ISP_STATUS],
					&power_state2);
			if (ret)
				goto err_isp_pw_off;
			ret = regmap_read_mmsys(
					&pw_info->regs[CAMSYS_ISP_STATUS],
					&power_state3);
			if (ret)
				goto err_isp_pw_off;
		} while (((power_state1 !=
			(PD_MM_DOWN_FLAG << PD_ISP_STATUS_SHIFT_BIT))
			 && read_count < 30) ||
			(power_state1 != power_state2) ||
			(power_state2 != power_state3));

		if (power_state1 !=
		(PD_MM_DOWN_FLAG << PD_ISP_STATUS_SHIFT_BIT)) {
			pr_err("fail to get isp power state 0x%x\n",
				power_state1);
			ret = -1;
			goto err_isp_pw_off;
		}

	}
	mutex_unlock(&pw_info->mlock);
	return 0;

err_isp_pw_off:
	pr_err("fail to power off isp, ret %d, read count %d\n",
		ret, read_count);
	mutex_unlock(&pw_info->mlock);

	return ret;
}
EXPORT_SYMBOL(sprd_isp_pw_off);


int sprd_dcam_pw_off(void)
{
	int ret = 0;
	unsigned int power_state1 = 0;
	unsigned int power_state2 = 0;
	unsigned int power_state3 = 0;

	unsigned int read_count = 0;

	ret = sprd_campw_check_drv_init();
	if (ret) {
		pr_err("fail to get init state %d, cb %p, ret %d\n",
			atomic_read(&pw_info->users_dcam_pw),
			__builtin_return_address(0), ret);
		return -ENODEV;
	}

	mutex_lock(&pw_info->mlock);
	if (atomic_dec_return(&pw_info->users_dcam_pw) == 0) {

		usleep_range(300, 350);

		/* cam domain power off */
		/* 1:auto shutdown en, shutdown with ap; 0: control by b25 */
		regmap_update_bits_mmsys(
			&pw_info->regs[CAMSYS_DCAM_SHUTDOWN_EN],
			0);
		/* set 1 to shutdown */
		regmap_update_bits_mmsys(
			&pw_info->regs[CAMSYS_DCAM_FORCE_SHUTDOWN],
			~((uint32_t)0));

		do {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;

			ret = regmap_read_mmsys(
					&pw_info->regs[CAMSYS_DCAM_STATUS],
					&power_state1);
			if (ret)
				goto err_dcam_pw_off;
			ret = regmap_read_mmsys(
					&pw_info->regs[CAMSYS_DCAM_STATUS],
					&power_state2);
			if (ret)
				goto err_dcam_pw_off;
			ret = regmap_read_mmsys(
					&pw_info->regs[CAMSYS_DCAM_STATUS],
					&power_state3);
			if (ret)
				goto err_dcam_pw_off;
		} while (((power_state1 != PD_MM_DOWN_FLAG) &&
			read_count < 30) ||
			(power_state1 != power_state2) ||
			(power_state2 != power_state3));

		if (power_state1 != PD_MM_DOWN_FLAG) {
			pr_err("fail to get dcam power state 0x%x\n",
				power_state1);
			ret = -1;
			goto err_dcam_pw_off;
		}

	}
	mutex_unlock(&pw_info->mlock);
	return 0;

err_dcam_pw_off:
	pr_err("fail to power off dcam, ret %d, read count %d\n",
		ret, read_count);
	mutex_unlock(&pw_info->mlock);

	return ret;
}
EXPORT_SYMBOL(sprd_dcam_pw_off);

int sprd_cam_pw_off(void)
{
	int ret = 0;
	unsigned int power_state1 = 0;
	unsigned int power_state2 = 0;
	unsigned int power_state3 = 0;

	unsigned int read_count = 0;

	ret = sprd_campw_check_drv_init();
	if (ret) {
		pr_err("fail to get init state %d, cb %p, ret %d\n",
			atomic_read(&pw_info->users_pw),
			__builtin_return_address(0), ret);
		return -ENODEV;
	}

	mutex_lock(&pw_info->mlock);
	if (atomic_dec_return(&pw_info->users_pw) == 0) {

		usleep_range(300, 350);

		/* cam domain power off */
		/* 1:auto shutdown en, shutdown with ap; 0: control by b25 */
		regmap_update_bits_mmsys(&pw_info->regs[CAMSYS_SHUTDOWN_EN],
			0);
		/* set 1 to shutdown */
		regmap_update_bits_mmsys(&pw_info->regs[CAMSYS_FORCE_SHUTDOWN],
			~((uint32_t)0));

		do {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;

			ret = regmap_read_mmsys(
					&pw_info->regs[CAMSYS_PWR_STATUS],
					&power_state1);
			if (ret)
				goto err_pw_off;
			ret = regmap_read_mmsys(
					&pw_info->regs[CAMSYS_PWR_STATUS],
					&power_state2);
			if (ret)
				goto err_pw_off;
			ret = regmap_read_mmsys(
					&pw_info->regs[CAMSYS_PWR_STATUS],
					&power_state3);
			if (ret)
				goto err_pw_off;
		} while (((power_state1 !=
			(PD_MM_DOWN_FLAG << PD_MM_DOWN_BIT)) &&
			read_count < 30) ||
			(power_state1 != power_state2) ||
			(power_state2 != power_state3));

		if (power_state1 != (PD_MM_DOWN_FLAG << PD_MM_DOWN_BIT)) {
			pr_err("fail to get power state 0x%x\n", power_state1);
			ret = -1;
			goto err_pw_off;
		}

	}
	mutex_unlock(&pw_info->mlock);
	return 0;

err_pw_off:
	pr_err("fail to power off cam sys, ret %d, read count %d\n",
		ret, read_count);
	mutex_unlock(&pw_info->mlock);

	return ret;
}
EXPORT_SYMBOL(sprd_cam_pw_off);

int sprd_dcam_pw_on(void)
{
	int ret = 0;
	unsigned int power_state1 = 0;
	unsigned int power_state2 = 0;
	unsigned int power_state3 = 0;

	unsigned int read_count = 0;

	ret = sprd_campw_check_drv_init();
	if (ret) {
		pr_info("fail to get init state %d, cb %p, ret %d\n",
			atomic_read(&pw_info->users_dcam_pw),
			__builtin_return_address(0), ret);
		return -ENODEV;
	}

	mutex_lock(&pw_info->mlock);
	if (atomic_inc_return(&pw_info->users_dcam_pw) == 1) {

		/* dcam domain power on */
		regmap_update_bits_mmsys(
			&pw_info->regs[CAMSYS_DCAM_SHUTDOWN_EN], 0);
		/* power on */
		regmap_update_bits_mmsys(
			&pw_info->regs[CAMSYS_DCAM_FORCE_SHUTDOWN], 0);

		do {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;

			ret = regmap_read_mmsys(
					&pw_info->regs[CAMSYS_DCAM_STATUS],
					&power_state1);
			if (ret)
				goto err_dcam_pw_on;
			ret = regmap_read_mmsys(
					&pw_info->regs[CAMSYS_DCAM_STATUS],
					&power_state2);
			if (ret)
				goto err_dcam_pw_on;
			ret = regmap_read_mmsys(
					&pw_info->regs[CAMSYS_DCAM_STATUS],
					&power_state3);
			if (ret)
				goto err_dcam_pw_on;

			pr_info("dcam pw status, %x, %x, %x\n",
				power_state1, power_state2, power_state3);

		} while ((power_state1 && (read_count < 30)) ||
			(power_state1 != power_state2) ||
			(power_state2 != power_state3));


		if (power_state1) {
			pr_err("fail to get dcam power state 0x%x\n",
				power_state1);
			ret = -1;
			goto err_dcam_pw_on;
		}

	}
	mutex_unlock(&pw_info->mlock);
	return 0;

err_dcam_pw_on:
	atomic_dec_return(&pw_info->users_dcam_pw);
	pr_err("fail to power on cam sys\n");
	mutex_unlock(&pw_info->mlock);

	return ret;
}
EXPORT_SYMBOL(sprd_dcam_pw_on);


int sprd_isp_pw_on(void)
{
	int ret = 0;
	unsigned int power_state1 = 0;
	unsigned int power_state2 = 0;
	unsigned int power_state3 = 0;

	unsigned int read_count = 0;

	ret = sprd_campw_check_drv_init();
	if (ret) {
		pr_info("fail to get init state %d, cb %p, ret %d\n",
			atomic_read(&pw_info->users_isp_pw),
			__builtin_return_address(0), ret);
		return -ENODEV;
	}

	if (atomic_inc_return(&pw_info->users_isp_pw) == 1) {

		/* isp domain power on */
		regmap_update_bits_mmsys(
			&pw_info->regs[CAMSYS_ISP_SHUTDOWN_EN], 0);
		/* power on */
		regmap_update_bits_mmsys(
			&pw_info->regs[CAMSYS_ISP_FORCE_SHUTDOWN], 0);

		do {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;

			ret = regmap_read_mmsys(
					&pw_info->regs[CAMSYS_ISP_STATUS],
					&power_state1);
			if (ret)
				goto err_isp_pw_on;
			ret = regmap_read_mmsys(
					&pw_info->regs[CAMSYS_ISP_STATUS],
					&power_state2);
			if (ret)
				goto err_isp_pw_on;
			ret = regmap_read_mmsys(
					&pw_info->regs[CAMSYS_ISP_STATUS],
					&power_state3);
			if (ret)
				goto err_isp_pw_on;

			pr_info("ISP pw status, %x, %x, %x\n",
				power_state1, power_state2, power_state3);

		} while ((power_state1 && (read_count < 30)) ||
			(power_state1 != power_state2) ||
			(power_state2 != power_state3));


		if (power_state1) {
			pr_err("fail to get isp power state 0x%x\n",
				power_state1);
			ret = -1;
			goto err_isp_pw_on;
		}

	}
	mutex_unlock(&pw_info->mlock);
	return 0;

err_isp_pw_on:
	atomic_dec_return(&pw_info->users_isp_pw);
	pr_err("fail to power on cam sys\n");
	mutex_unlock(&pw_info->mlock);

	return ret;
}
EXPORT_SYMBOL(sprd_isp_pw_on);

int sprd_cam_pw_on(void)
{
	int ret = 0;
	unsigned int power_state1 = 0;
	unsigned int power_state2;
	unsigned int power_state3;

	unsigned int read_count = 0;

	ret = sprd_campw_check_drv_init();
	if (ret) {
		pr_info("fail to get init state %d, cb %p, ret %d\n",
			atomic_read(&pw_info->users_pw),
			__builtin_return_address(0), ret);
		return -ENODEV;
	}

	mutex_lock(&pw_info->mlock);
	if (atomic_inc_return(&pw_info->users_pw) == 1) {
		/* cam domain power on */
		/* clear force shutdown */
		regmap_update_bits_mmsys(&pw_info->regs[CAMSYS_SHUTDOWN_EN],
			0);
		/* power on */
		regmap_update_bits_mmsys(&pw_info->regs[CAMSYS_FORCE_SHUTDOWN],
			0);

		do {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;

			ret = regmap_read_mmsys(
					&pw_info->regs[CAMSYS_PWR_STATUS],
					&power_state1);
			if (ret)
				goto err_pw_on;
			ret = regmap_read_mmsys(
					&pw_info->regs[CAMSYS_PWR_STATUS],
					&power_state2);
			if (ret)
				goto err_pw_on;
			ret = regmap_read_mmsys(
					&pw_info->regs[CAMSYS_PWR_STATUS],
					&power_state3);
			if (ret)
				goto err_pw_on;

			pr_info("cam pw status, %x, %x, %x\n",
				power_state1, power_state2, power_state3);
		} while ((power_state1 && (read_count < 30)) ||
			(power_state1 != power_state2) ||
			(power_state2 != power_state3));


		if (power_state1) {
			pr_err("fail to get power state 0x%x\n", power_state1);
			ret = -1;
			goto err_pw_on;
		}

	}
	mutex_unlock(&pw_info->mlock);
	return 0;

err_pw_on:
	atomic_dec_return(&pw_info->users_pw);
	pr_err("fail to power on cam sys\n");
	mutex_unlock(&pw_info->mlock);

	return ret;
}
EXPORT_SYMBOL(sprd_cam_pw_on);

int sprd_cam_domain_eb(void)
{
	int ret = 0;
	unsigned int val0, val1;

	ret = sprd_campw_check_drv_init();
	if (ret) {
		pr_err("fail to get init state %d, cb %p, ret %d\n",
			atomic_read(&pw_info->users_pw),
			__builtin_return_address(0), ret);
		return -ENODEV;
	}

	pr_debug("users count %d, cb %p\n",
		atomic_read(&pw_info->users_clk),
		__builtin_return_address(0));

	mutex_lock(&pw_info->mlock);
	if (atomic_inc_return(&pw_info->users_clk) == 1) {
		/* config cam emc clk */
#ifdef PW_ON_HAPS

		reg_owr(AON_APB_AHB_EN, pw_info->mm_eb);

		/* mm bus enable */
		reg_owr(MM_AHB_SYS_EN, pw_info->mm_mtx_data_en);
		reg_owr(MM_AHB_SYS_EN, pw_info->dvfs_en);

		reg_owr(MM_AHB_SYS_EN, pw_info->ckg_en);
		val0 = reg_rd(AON_APB_AHB_EN);
		val1 = reg_rd(MM_AHB_SYS_EN);
		pr_info("mm eb status %x, %x\n", val0, val1);
#endif
	}
	mutex_unlock(&pw_info->mlock);

	return 0;
}
EXPORT_SYMBOL(sprd_cam_domain_eb);

int sprd_cam_domain_disable(void)
{
	int ret = 0;

	ret = sprd_campw_check_drv_init();
	if (ret) {
		pr_err("fail to get init state %d, cb %p, ret %d\n",
			atomic_read(&pw_info->users_pw),
			__builtin_return_address(0), ret);
	}

	pr_debug("users count %d, cb %p\n",
		atomic_read(&pw_info->users_clk),
		__builtin_return_address(0));

	mutex_lock(&pw_info->mlock);
	if (atomic_dec_return(&pw_info->users_clk) == 0) {
		/* mm bus enable */
#ifdef PW_ON_HAPS

		reg_awr(MM_AHB_SYS_EN, ~pw_info->mm_mtx_data_en);
		reg_awr(MM_AHB_SYS_EN, ~pw_info->dvfs_en);

		reg_awr(MM_AHB_SYS_EN, ~pw_info->ckg_en);
		reg_awr(AON_APB_AHB_EN, ~pw_info->mm_eb);
#endif
	}
	mutex_unlock(&pw_info->mlock);

	return 0;

}
EXPORT_SYMBOL(sprd_cam_domain_disable);

static int sprd_campw_deinit(struct platform_device *pdev)
{
	devm_kfree(&pdev->dev, pw_info);
	return 0;
}

static int sprd_campw_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("cam power insmod\n");
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
	{ .compatible = "sprd,mm-domain", },
	{},
};

static struct platform_driver sprd_campw_driver = {
	.probe = sprd_campw_probe,
	.remove = sprd_campw_remove,
	.driver = {
		.name = "camsys-power",
		.of_match_table = of_match_ptr(sprd_campw_match_table),
	},
};

module_platform_driver(sprd_campw_driver);

MODULE_DESCRIPTION("Camsys Power Driver");
MODULE_AUTHOR("Multimedia_Camera@unisoc.com");
MODULE_LICENSE("GPL");


