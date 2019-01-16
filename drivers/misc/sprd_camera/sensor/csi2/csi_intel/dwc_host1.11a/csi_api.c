/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon/sprd-glb.h>
#include <linux/mfd/syscon.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include "csi_api.h"
#include "csi_driver.h"
#include "sprd_sensor_drv.h"

#include "video/sprd_mm.h"

static struct csi_dt_node_info *s_csi_dt_info_p[4];

#define SWITCH_REG (0xd210005c)
#define PHY0_REG (0xE4180000)
#define PHY1_REG (0xE4190000)
#define HSEQ (0x64)
#define HSRCOMP (0x18)
#define RCOMPCTL (0x24)
#define RXCLK (0xe42e01e8)
#define RX0CLK (BIT_9)
#define RX1CLK (BIT_10)

#define csi_reg_rd(iobase, reg) \
	readl_relaxed((iobase) + (reg))

#define csi_reg_wr(iobase, reg, val) \
	writel_relaxed((val), ((iobase) + (reg)))

#define csi_reg_and(iobase, reg, val) \
	writel_relaxed((readl_relaxed((iobase) + (reg)) & (val)), \
		       ((iobase) + (reg)))

#define csi_reg_or(iobase, reg, val) \
	writel_relaxed((readl_relaxed((iobase) + (reg)) | (val)), \
		       ((iobase) + (reg)))

#define csi_reg_mwr(io_base, reg, mask, val) \
	do { \
		unsigned int v = readl_relaxed((io_base) + (reg)); \
		v &= ~(mask); \
		writel_relaxed((v | ((mask) & (val))), \
			       ((io_base) + (reg))); \
	} while (0)


/* csi0 0xe418_0000 */
static int csi_calibra_func(int phyid)
{
	int hsr = 0;
	int rcp = 0;
	int hseq = 0;

	pr_info("%s E, phyid:%d\n", __func__, phyid);

#if 0
	if (phyid == 0) {
		iomem_regs = ioremap_nocache(PHY0_REG, HSEQ);
		csi_reg_or(iomem_regs, HSRCOMP, BIT_0);
		csi_reg_or(iomem_regs, RCOMPCTL, BIT_4);
	}

	if (phyid == 1) {
		iomem_regs = ioremap_nocache(PHY1_REG, HSEQ);
		csi_reg_or(iomem_regs, HSRCOMP, BIT_0);
		csi_reg_or(iomem_regs, RCOMPCTL, BIT_4);
	}
#endif
	pr_info("%s, hsr:0x%x, rcp:0x%x, hseq:0x%x\n",
				__func__, hsr, rcp, hseq);

	return 0;
}

static struct csi_dt_node_info *csi_get_dt_node_data(int sensor_id)
{
	return s_csi_dt_info_p[sensor_id];
}

static int csi_mipi_clk_enable(int sensor_id)
{
	int ret = 0;
	int val = 0;
	struct csi_dt_node_info *dt_info = s_csi_dt_info_p[sensor_id];

	pr_info("%s phy id %d\n", __func__, dt_info->phy.phy_id);

	if (!dt_info || !dt_info->mipi_gate_clk ||
	    !dt_info->cphy_gate_clk || !dt_info->csi_eb_clk) {
		pr_err("%s invalid parm\n", __func__);
		return -EINVAL;
	}

	ret = clk_prepare_enable(dt_info->cphy_gate_clk);
	if (ret) {
		pr_err("mipi csi cphy clk err\n");
		return ret;
	}
	ret = clk_prepare_enable(dt_info->mipi_gate_clk);
	if (ret) {
		pr_err("mipi csi mipi gate clk err\n");
		return ret;
	}
	ret = clk_prepare_enable(dt_info->csi_eb_clk);
	if (ret) {
		pr_err("csi eb clk err\n");
		return ret;
	}
	ret = clk_prepare_enable(dt_info->mipi_csi_eb_clk);
	if (ret) {
		pr_err("mipi csi eb clk err\n");
		return ret;
	}

	if (dt_info->id == 0) {
		val = RX0CLK;
		csi_reg_or(dt_info->rxclk, 0x00, val);
	} else {
		val = RX1CLK;
		csi_reg_or(dt_info->rxclk, 0x00, val);
	}

	pr_debug("csi mipi clk enable OK\n");

	return ret;
}

static void csi_mipi_clk_disable(int sensor_id)
{
	int val = 0;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);

	if (!dt_info || !dt_info->mipi_gate_clk ||
	    !dt_info->cphy_gate_clk || !dt_info->csi_eb_clk) {
		pr_err("%s invalid parm\n", __func__);
		return;
	}
	clk_disable_unprepare(dt_info->cphy_gate_clk);
	clk_disable_unprepare(dt_info->mipi_gate_clk);
	clk_disable_unprepare(dt_info->csi_eb_clk);
	clk_disable_unprepare(dt_info->mipi_csi_eb_clk);

	if (dt_info->id == 0) {
		val = RX0CLK;
		csi_reg_and(dt_info->rxclk, 0x00, ~val);
	} else {
		val = RX1CLK;
		csi_reg_and(dt_info->rxclk, 0x00, ~val);
	}
}

int csi_api_dt_node_init(struct device *dev, struct device_node *dn,
			 int sensor_id, unsigned int phy_id)
{
	struct csi_dt_node_info *csi_info = NULL;
	struct resource res;
	void __iomem *reg_base;
	struct regmap *syscon_gpr;
	void __iomem *iomem_regs = NULL;
	int val;

	csi_info = devm_kzalloc(dev, sizeof(*csi_info), GFP_KERNEL);
	if (!csi_info)
		return -ENOMEM;

	/* get reg base address */
	of_address_to_resource(dn, 0, &res);
	reg_base = devm_ioremap_nocache(dev, res.start, resource_size(&res));
	if (IS_ERR(reg_base))
		return PTR_ERR(reg_base);
	csi_info->reg_base = (unsigned long)reg_base;

	csi_info->phy.phy_id = phy_id;
	csi_info->sensor_id = sensor_id;
	pr_info("csi name %s, phy_id = %d, sensorid = %d\n",
		dn->name, phy_id, sensor_id);
	if (strncasecmp(dn->name, "csi0", 4) == 0)
		csi_info->id = 0;
	else if (strncasecmp(dn->name, "csi1", 4) == 0)
		csi_info->id = 1;
	else
		pr_info("warning: not support currently\n");
	pr_info("csi %d reg base %lx\n", csi_info->id, csi_info->reg_base);

	/* get clk */
	csi_info->cphy_gate_clk = of_clk_get_by_name(dn, "clk_cphy_gate");
	if (csi_info->cphy_gate_clk == NULL) {
		pr_err("csi get clk_cphy_gate fialed!\n");
		return -EINVAL;
	}
	csi_info->mipi_gate_clk = of_clk_get_by_name(dn, "clk_mipi_gate");
	if (csi_info->mipi_gate_clk == NULL) {
		pr_err("csi get clk_mipi_gate fialed!\n");
		return -EINVAL;
	}
	csi_info->csi_eb_clk = of_clk_get_by_name(dn, "clk_csi_eb");
	if (csi_info->csi_eb_clk == NULL) {
		pr_err("csi get clk_csi_eb fialed!\n");
		return -EINVAL;
	}
	csi_info->mipi_csi_eb_clk = of_clk_get_by_name(dn, "clk_mipi_csi_eb");
	if (csi_info->mipi_csi_eb_clk == NULL) {
		pr_err("csi get clk_mipi_csi_eb fialed!\n");
		return -EINVAL;
	}

	/* read version */
	if (of_property_read_u32_index(dn, "sprd,ip-version", 0,
				       &csi_info->ip_version)) {
		pr_err("failed to get csi ip version\n");
		devm_kfree(dev, csi_info);
		return -EINVAL;
	}

	syscon_gpr = syscon_regmap_lookup_by_compatible("sprd,iwhale2-lpc-ahb");
	if (IS_ERR(syscon_gpr)) {
		pr_err("csi get sprd,iwhale2-lpc-ahb failed\n");
		return PTR_ERR(syscon_gpr);
	}
	csi_info->phy.cam_ahb_gpr = syscon_gpr;

#if 0
	syscon_gpr = syscon_regmap_lookup_by_compatible("sprd,iwhale2-aon-apb");
	if (IS_ERR(syscon_gpr))
		return PTR_ERR(syscon_gpr);
	csi_info->phy.aon_apb_gpr = syscon_gpr;
#endif

	syscon_gpr = syscon_regmap_lookup_by_compatible("sprd,anlg_phy_g8");
	if (IS_ERR(syscon_gpr)) {
		pr_err("csi get sprd,anlg_phy_g8 failed!\n");
		return PTR_ERR(syscon_gpr);
	}
	csi_info->phy.anlg_phy_g8_gpr = syscon_gpr;

	/* get chip Axx's ID to avoid current leakage */
	iomem_regs = ioremap_nocache(0xe42e3120, 0x4);
	csi_info->phy.chip_id = csi_reg_rd(iomem_regs, 0x00);
	pr_info("csi chip id is %d\n", csi_info->phy.chip_id);
	iounmap(iomem_regs);

	/* workaround for csi phy leakage */
	if (csi_info->id == 0 && csi_info->phy.chip_id != 2)
		csi_phy_init(&csi_info->phy);

	csi_phy_power_down(&csi_info->phy, csi_info->id, 1);

	if (csi_info->id == 0 || csi_info->id == 1) {
		csi_info->rxclk = ioremap_nocache(RXCLK, 0x4);
		val = (BIT_9 | BIT_10);
		csi_reg_and(csi_info->rxclk, 0x00, ~val);
	} else {
		csi_info->rxclk = NULL;
	}

#if __INT_CSI__
	csi_info->irq = irq_of_parse_and_map(dn, 0);
	pr_info("csi: irq is %d\n", csi_info->irq);
#endif
	s_csi_dt_info_p[sensor_id] = csi_info;
	pr_info("%s success!\n", __func__);

	return 0;
}

int csi_api_mipi_phy_cfg_init(struct device_node *phy_node, int sensor_id)
{
	unsigned int phy_id = 0;

	of_property_read_u32(phy_node, "sprd,phyid", &phy_id);
	if (phy_id > 2) {
		pr_err("%s: get phy id error:%d\n", __func__, phy_id);
		return -1;
	}
	pr_info("sensor id %d phy id %d\n", sensor_id, phy_id);

	return phy_id;
}

int csi_api_mipi_phy_cfg(void)
{
	return 0;
}

static int csi_api_init(unsigned int bps_per_lane, unsigned int phy_id,
			int sensor_id)
{
	int ret = SUCCESS;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);

	pr_info("%s sensor id %d, phy_id = %d\n", __func__, sensor_id, phy_id);
	do {
		csi_phy_power_down(&dt_info->phy, dt_info->id, 0);
		pr_info("%s sensor id %d\n", __func__, sensor_id);
		csi_enable(&dt_info->phy, dt_info->id);
		ret = csi_init(dt_info->reg_base, dt_info->ip_version,
			       sensor_id);
		if (ret != SUCCESS) {
			pr_err("%s unable to initialise driver", __func__);
			break;
		}
		dphy_init(&dt_info->phy, bps_per_lane, phy_id, sensor_id);
	} while (0);

	return ret;
}

static int csi_api_start(int sensor_id)
{
	int ret = SUCCESS;
#if __INT_CSI__
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
#endif
	pr_debug("start csi\n");

	do {
		/* set only one lane (lane 0) as active (on) */
		ret = csi_set_on_lanes(1, sensor_id);
		if (ret != SUCCESS) {
			pr_err("%s unable to set lanes\n", __func__);
			break;
		}
		ret = csi_shut_down_phy(0, sensor_id);
		if (ret != SUCCESS) {
			pr_err("%s unable to bring up PHY\n", __func__);
			break;
		}
		ret = csi_reset_phy(sensor_id);
		if (ret != SUCCESS) {
			pr_err("%s unable to reset PHY\n", __func__);
			break;
		}
		ret = csi_reset_controller(sensor_id);
		if (ret != SUCCESS) {
			pr_err("%s unable to reset controller\n", __func__);
			break;
		}

		/* mask all interrupts */
		csi_event_disable(0xffffffff, 1, sensor_id);
		csi_event_disable(0xffffffff, 2, sensor_id);
		csi_event_disable(0xffffffff, 3, sensor_id);
		csi_event_disable(0xffffffff, 4, sensor_id);
		csi_event_disable(0xffffffff, 5, sensor_id);
		csi_event_disable(0xffffffff, 6, sensor_id);
		csi_event_disable(0xffffffff, 7, sensor_id);
#if __INT_CSI__
		pr_info("%s sensor_id:%d, request irq\n", __func__, sensor_id);
		if (sensor_id == 0) {
			ret = request_irq(dt_info->irq, csi_api_event_handler,
				IRQF_TRIGGER_HIGH,
				"csi_1", &dt_info->sensor_id);
		} else if (sensor_id == 2 || sensor_id == 1) {
			ret = request_irq(dt_info->irq, csi_api_event_handler,
				IRQF_TRIGGER_HIGH,
				"csi_2", &dt_info->sensor_id);
		}
		if (ret)
			pr_err("csi: register csi irq fail!!\n");
#endif
	} while (0);

	return ret;
}

void csi_dcam_switch(unsigned int switch_en)
{
	int val = 0;
	void __iomem *iomem_regs = NULL;

	iomem_regs = ioremap_nocache(SWITCH_REG, 0x4);

	if (switch_en == 0) {
		csi_reg_wr(iomem_regs, 0x00, 0x00);
		val = csi_reg_rd(iomem_regs, 0x00);
		pr_debug("[%s]:<0xd210005c>[0x%x] switch_en= %d\n", __func__,
			 val, switch_en);
	}

	if (switch_en == 1) {
		/* dcam SWITCH csi1 -> dcam0 */
		csi_reg_wr(iomem_regs, 0x00, 0x10);
		val = csi_reg_rd(iomem_regs, 0x00);
		pr_debug("[%s]:<0xd210005c>[0x%x] switch_en= %d\n", __func__,
			 val, switch_en);
	}

	iounmap(iomem_regs);
}

int csi_api_open(int bps_per_lane, int phy_id, int lane_num, int sensor_id)
{
	int ret = 0;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
	int dcam_id = sprd_sensor_find_dcam_id(sensor_id);

	if (dcam_id == 0 && dt_info->id == 1)
		csi_dcam_switch(1);
	else
		csi_dcam_switch(0);

	ret = csi_mipi_clk_enable(sensor_id);
	if (ret) {
		pr_err("csi: failed enable mipi clk\n");
		csi_mipi_clk_disable(sensor_id);
		goto exit;
	}

	udelay(1);
	ret = csi_api_init(bps_per_lane, phy_id, sensor_id);
	if (ret)
		goto exit;

	ret = csi_api_start(sensor_id);
	if (ret)
		goto exit;

	ret = csi_set_on_lanes(lane_num, sensor_id);
	if (ret)
		goto exit;

	csi_calibra_func(dt_info->phy.phy_id);
	pr_info("csi: open csi success!\n");
	return ret;

exit:
	pr_err("csi: failed open csi %d\n", ret);
	csi_api_close(phy_id, sensor_id);
	return ret;
}

int csi_api_close(unsigned int phy_id, int sensor_id)
{
	int ret = 0;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);

#if __INT_CSI__
	free_irq(dt_info->irq, &dt_info->sensor_id);
#endif
	csi_shut_down_phy(1, sensor_id);
	ret = csi_close(sensor_id);
	csi_mipi_clk_disable(sensor_id);
	csi_phy_power_down(&dt_info->phy, dt_info->id, 1);
	pr_info("close csi, ret %d\n", ret);

	csi_dcam_switch(0);

	return ret;
}

int csi_api_switch(int sensor_id)
{
	return 0;
}
