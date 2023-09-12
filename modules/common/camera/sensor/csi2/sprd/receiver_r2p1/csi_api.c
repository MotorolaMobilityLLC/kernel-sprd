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
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/semaphore.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mfd/syscon.h>
#include <sprd_mm.h>
#include "sprd_sensor_core.h"
#include "csi_api.h"
#include "csi_driver.h"
#include "dphy_drv.h"

#include "mm_ahb.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "csi_api: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define CSI_PATTERN_ENABLE (1)

static struct csi_dt_node_info *s_csi_dt_info_p[SPRD_SENSOR_ID_MAX];
extern uint32_t g_project_id;

static char *irqname[] = {"CSI0_E0", "CSI0_E1",
			"CSI1_E0", "CSI1_E1",
			"CSI2_E0", "CSI2_E1"};

static struct csi_dt_node_info *csi_get_dt_node_data(int sensor_id)
{
	return s_csi_dt_info_p[sensor_id];
}

static int csi_mipi_clk_enable(int sensor_id, int is_pattern)
{
	struct csi_dt_node_info *dt_info = NULL;
	int ret = 0;

	dt_info = csi_get_dt_node_data(sensor_id);

	/* enable ckg gate */
	regmap_update_bits(dt_info->syscon.mm_ahb, dt_info->syscon.ckg_eb,
		dt_info->syscon.ckg_eb_msk, dt_info->syscon.ckg_eb_msk);

	/* enable csi gate */
	regmap_update_bits(dt_info->syscon.mm_ahb, dt_info->syscon.csi_eb,
		dt_info->syscon.csi_eb_msk, dt_info->syscon.csi_eb_msk);

	/* enable ckg clock */
	regmap_update_bits(dt_info->syscon.mm_ahb, dt_info->syscon.cphy_ckg_eb,
		dt_info->syscon.cphy_ckg_eb_msk, dt_info->syscon.cphy_ckg_eb_msk);

	/* enable cfg gate 0x327d013c */
	regmap_update_bits(dt_info->syscon.aon_apb, dt_info->syscon.cphy_cfg_en,
		dt_info->syscon.cphy_cfg_en_msk, dt_info->syscon.cphy_cfg_en_msk);

	/* set csi clk from pad */
	ret = clk_prepare_enable(dt_info->csi_src_eb);
	if (ret) {
		pr_err("fail to mipi\n");
		return ret;
	}

	/* if pattern enable, clk from soc */
	if (is_pattern) {
		clk_disable_unprepare(dt_info->csi_src_eb);
	}

	return 0;
}

static void csi_mipi_clk_disable(int sensor_id)
{
	struct csi_dt_node_info *dt_info = NULL;

	dt_info = csi_get_dt_node_data(sensor_id);

	clk_disable_unprepare(dt_info->csi_src_eb);

	/* enable cfg gate */
	regmap_update_bits(dt_info->syscon.aon_apb, dt_info->syscon.cphy_cfg_en,
		dt_info->syscon.cphy_ckg_eb_msk, ~(dt_info->syscon.cphy_cfg_en_msk));

	/* enable ckg clock */
	regmap_update_bits(dt_info->syscon.mm_ahb, dt_info->syscon.cphy_ckg_eb,
		dt_info->syscon.cphy_ckg_eb_msk, ~(dt_info->syscon.cphy_ckg_eb_msk));

	/* enable csi clock */
	regmap_update_bits(dt_info->syscon.mm_ahb, dt_info->syscon.csi_eb,
		dt_info->syscon.csi_eb_msk, ~(dt_info->syscon.csi_eb_msk));

	/* enable ckg clock */
	regmap_update_bits(dt_info->syscon.mm_ahb, dt_info->syscon.ckg_eb,
		dt_info->syscon.ckg_eb_msk, ~(dt_info->syscon.ckg_eb_msk));



}

int csi_api_mipi_phy_cfg_init(struct device_node *phy_node, int sensor_id)
{
	unsigned int phy_id = 0;

	if (of_property_read_u32(phy_node, "sprd,phyid", &phy_id)) {
		pr_err("fail to get the sprd_phyid\n");
		return -1;
	}

	if (phy_id >= PHY_MAX) {
		pr_err("%s:fail to parse phyid : phyid:%u\n", __func__, phy_id);
		return -1;
	}

	pr_info("sensor_id %d mipi_phy:phy_id:%u\n", sensor_id, phy_id);

	return phy_id;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
struct regmap *syscon_regmap_lookup_by_name(struct device_node *np,const char *name)
{
	struct device_node *syscon_np;
	struct of_phandle_args args;
	struct regmap *regmap;
	int index = 0;
	int rc;
	if (name)
		index = of_property_match_string(np, "syscon-names", name);
	if (index < 0)
		return ERR_PTR(-EINVAL);
	rc = of_parse_phandle_with_args(np, name, "#syscon-cells", 0,
		&args);
	if (rc)
		return ERR_PTR(rc);
	syscon_np = args.np;
	if (!syscon_np)
		return ERR_PTR(-ENODEV);
	regmap = syscon_node_to_regmap(syscon_np);
	of_node_put(syscon_np);
	return regmap;
}
//EXPORT_SYMBOL_GPL(syscon_regmap_lookup_by_name);
int syscon_get_args_by_name(struct device_node *np,const char *name,int arg_count,unsigned int *out_args)
{
	struct of_phandle_args args;
	int index = 0;
	int rc;
	if (name)
		index = of_property_match_string(np, "syscon-names", name);
	if (index < 0)
		return -EINVAL;
	rc = of_parse_phandle_with_args(np, name, "#syscon-cells", 0,
		&args);
	if (rc)
		return rc;
	if (arg_count > args.args_count)
		arg_count = args.args_count;
	for (index = 0; index < arg_count; index++)
		out_args[index] = args.args[index];
	of_node_put(args.np);
	return arg_count;
}
//EXPORT_SYMBOL_GPL(syscon_get_args_by_name);

struct regmap *syscon_regmap_lookup_by_phandle(struct device_node *np, const char *property)
{
	struct device_node *syscon_np;
	struct regmap *regmap;

	if (property)
		syscon_np = of_parse_phandle(np, property, 0);
	else
		syscon_np = np;

	if (!syscon_np)
		return ERR_PTR(-ENODEV);

	regmap = syscon_node_to_regmap(syscon_np);
	of_node_put(syscon_np);

	return regmap;
}
#endif
//EXPORT_SYMBOL_GPL(syscon_regmap_lookup_by_phandle);

int csi_api_dt_node_init(struct device *dev, struct device_node *dn,
					int sensor_id, unsigned int phy_id)
{
	struct csi_dt_node_info *csi_info = NULL;
	struct resource res;
	void __iomem *reg_base = NULL;
	u32 args[2] = {0};
	int ret = 0;

	pr_info("Entry, phyid:%d, sensor_id:%d\n", phy_id, sensor_id);
	csi_info = devm_kzalloc(dev, sizeof(*csi_info), GFP_KERNEL);
	if (!csi_info)
		return -EINVAL;

	/* read address */
	if (of_address_to_resource(dn, 0, &res)) {
		pr_err("csi2_init:fail to get address info\n");
		return -EINVAL;
	}

	reg_base = ioremap_nocache(res.start, resource_size(&res));
	if (IS_ERR_OR_NULL(reg_base)) {
		pr_err("csi_dt_init:fail to get csi regbase\n");
		return PTR_ERR(reg_base);
	}
	csi_info->reg_base = (unsigned long)reg_base;

	csi_info->phy_id = phy_id;

	if (of_property_read_u32_index(dn, "sprd,csi-id", 0,
				&csi_info->controller_id)) {
		pr_err("get csi id failed\n");
		return -EINVAL;
	}

	/* get mm ahb register handle */
	csi_info->syscon.mm_ahb = syscon_regmap_lookup_by_name(dn, "csi_eb");
	if (IS_ERR_OR_NULL(csi_info->syscon.mm_ahb)) {
		pr_err("get mm ahb syscon failed\n");
		return -EINVAL; /* PRT_ERR(csi_info->syscon.mm_ahb); */
	}

	ret = syscon_get_args_by_name(dn, "csi_dhpy_c_sel", 2, args);
	if (ret == 2) {
		csi_info->syscon.dphy_sel = args[0];
		csi_info->syscon.dphy_msk = args[1];
	} else {
		pr_err("get dphy sel failed\n");
		goto err;
	}

	ret = syscon_get_args_by_name(dn, "csi_eb", 2, args);
	if (ret == 2) {
		csi_info->syscon.csi_eb = args[0];
		csi_info->syscon.csi_eb_msk = args[1];
	} else {
		pr_err("get csi eb failed\n");
		goto err;
	}

	ret = syscon_get_args_by_name(dn, "csi_rst", 2, args);
	if (ret == 2) {
		csi_info->syscon.csi_rst = args[0];
		csi_info->syscon.csi_rst_msk = args[1];
	} else {
		pr_err("get csi rest failed\n");
		goto err;
	}

	ret = syscon_get_args_by_name(dn, "csi_ckg_en", 2, args);
	if (ret == 2) {
		csi_info->syscon.ckg_eb = args[0];
		csi_info->syscon.ckg_eb_msk = args[1];
	} else {
		pr_err("get csi ckg en failed\n");
		goto err;
	}

	ret = syscon_get_args_by_name(dn, "cphy_ckg_en", 2, args);
	if (ret == 2) {
		csi_info->syscon.cphy_ckg_eb = args[0];
		csi_info->syscon.cphy_ckg_eb_msk = args[1];
	} else {
		pr_err("get csi cphy ckg failed\n");
		goto err;
	}

	csi_info->syscon.aon_apb = syscon_regmap_lookup_by_name(dn, "cphy_cfg_en");
	if (IS_ERR_OR_NULL(csi_info->syscon.aon_apb)) {
		pr_err("get aon apb syscon failed\n");
		return -EINVAL;
	}

	ret = syscon_get_args_by_name(dn, "cphy_cfg_en", 2, args);
	if (ret == 2) {
		csi_info->syscon.cphy_cfg_en = args[0];
		csi_info->syscon.cphy_cfg_en_msk = args[1];
	} else {
		pr_err("get csi cphy ckg failed\n");
		goto err;
	}

	csi_info->csi_src_eb = of_clk_get_by_name(dn, "mipi_csi_src");
	if (IS_ERR_OR_NULL(csi_info->csi_src_eb)) {
		pr_err("get mipi_csi_src failed\n");
		goto err;
	}

	csi_info->irq_e0 = irq_of_parse_and_map(dn, 3);
	pr_info("irq e0 id:%d\n", csi_info->irq_e0);
	csi_info->irq_e1 = irq_of_parse_and_map(dn, 2);
	pr_info("irq e1 id:%d\n", csi_info->irq_e1);

	ret = phy_parse_dt(phy_id, dev);
	if (ret != 0) {
		pr_err("parse phy dts efailed\n");
		goto err;
	}

	csi_info->sid = sensor_id;
	csi_reg_base_save(csi_info, sensor_id);
	s_csi_dt_info_p[sensor_id] = csi_info;
	pr_info("csi dt info:sensor_id :%d, phy_id:%d, csi_id:%d, csi_info:0x%p\n",
		sensor_id, csi_info->phy_id, csi_info->controller_id,
		csi_info);

	return 0;
err:
	pr_err("%s failed, ret = %d\n", __func__, ret);
	return -EINVAL;
}

static int dphy_init(unsigned int bps_per_lane,
					unsigned int phy_id, int sensor_id)
{
	int ret = 0;
	struct csi_dt_node_info *dt_info = NULL;

	dt_info = csi_get_dt_node_data(sensor_id);
	csi_phy_power_down(phy_id, dt_info->controller_id, sensor_id, 0);
	/* csi_controller_enable(dt_info, phy_id); */
	dphy_init_state(phy_id, dt_info->controller_id, sensor_id);
	ret = dphy_csi_path_cfg(dt_info);

	return ret;
}

int csi_api_mipi_phy_cfg(void)
{
	int ret = 0;
	return ret;
}

irqreturn_t csi_int0_func(int irq, void *handle)
{
	struct csi_dt_node_info *csi_cxt  = (struct csi_dt_node_info *)handle;

	pr_err_ratelimited("sid:%d, reg:0x%x, val:0x%x\n",
		csi_cxt->sid, ERR0, CSI_REG_RD(csi_cxt->sid, ERR0));
	CSI_REG_WR(csi_cxt->sid, ERR0_CLR, 0xffffffff);

	return 0;
}

irqreturn_t csi_int1_func(int irq, void *handle)
{
	struct csi_dt_node_info *csi_cxt  = (struct csi_dt_node_info *)handle;

	pr_err_ratelimited("sid:%d, reg:0x%x, val:0x%x\n",
		csi_cxt->sid, ERR1, CSI_REG_RD(csi_cxt->sid, ERR1));
	CSI_REG_WR(csi_cxt->sid, ERR1_CLR, 0xffffffff);

	return 0;
}

int csi_api_open(int bps_per_lane, int phy_id, int lane_num, int sensor_id, int is_pattern,
	int is_cphy, uint64_t lane_seq)
{
	int ret = 0;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
	unsigned int csi_base_addr[] = {0x62300000, 0x62400000, 0x62500000};

	pr_info("entry, phyid:%d, lane_number:%d, sensor_id:%d, project_id: %d\n",
			phy_id, lane_num, sensor_id, g_project_id);

	if (!dt_info) {
		pr_err("fail to get valid phy ptr\n");
		return -EINVAL;
	}
	phy_id = dt_info->phy_id;
	if (g_project_id == 0) {
		ret = csi_ahb_reset(dt_info, dt_info->controller_id);
		if (unlikely(ret))
			goto EXIT;

		ret = csi_mipi_clk_enable(sensor_id, is_pattern);
		if (unlikely(ret < 0))
			goto EXIT;

		udelay(1);

		ret = dphy_init(bps_per_lane, phy_id, sensor_id);
		if (unlikely(ret))
			goto EXIT;
	} else { //roc1

		dphy_csi_match(dt_info);
		if (phy_id == PHY_2P2
			|| phy_id == PHY_2P2_M
			|| phy_id == PHY_2P2_S) {
			//m/s_en
			reg_mwr(0x323f0058, BIT_24|BIT_21, BIT_24|BIT_21);
			//set 2p2l mode = 4lane
			if (phy_id == PHY_2P2)
				reg_mwr(0x323f000c, BIT_4, BIT_4);
			else
				reg_mwr(0x323f000c, BIT_4, ~BIT_4);

			//enable clock
			ret = csi_mipi_clk_enable(sensor_id, is_pattern);
			if (unlikely(ret < 0))
				goto EXIT;

			reg_mwr(0x323f0000 + 0x60, BIT_3, BIT_3); //dbg_dsi_if_sel_db = 1
			reg_mwr(0x323f0000 + 0x4C, BIT_5, ~BIT_5); //DSI_IF_SEL_DB = 0
			reg_mwr(0x323f0000 + 0x50, BIT_0, BIT_0); //DSI_TESTCLR_DB = 1
			//testclr = 1
			reg_mwr(csi_base_addr[dt_info->controller_id] + 0x48, BIT_0, BIT_0);
			udelay(100);
			//testclr = 0
			reg_mwr(csi_base_addr[dt_info->controller_id] + 0x48, BIT_0, ~(BIT_0));

			reg_mwr(0x323f0000 + 0x58, BIT_26, BIT_26);  //CSI force S shutdownz
			reg_mwr(0x323f0000 + 0x58, BIT_28, BIT_28);  //CSI force shutdownz

			/* config csi & phy connection, and set phy regs */
			if (dt_info->controller_id == 2)
				reg_mwr(0x62200030, 0x30007fff, 0x30003000);
			else
				reg_mwr(0x62200030, 0x30007fff, 0x1b<<(dt_info->controller_id*6));

			phy_write(dt_info->controller_id, 0x4d, 0x48);
			pr_info("val:0x%x\n", phy_read(dt_info->controller_id, 0x4d));
			phy_write(dt_info->controller_id, 0x5d, 0x68);
			pr_info("val:0x%x\n",  phy_read(dt_info->controller_id, 0x5d));

			dphy_csi_match(dt_info);
		} else {
			pr_info("enable clock\n");
			ret = csi_mipi_clk_enable(sensor_id, is_pattern);
			if (unlikely(ret < 0))
				goto EXIT;
			pr_info("reset phy from controller: %d\n", dt_info->controller_id);
			//testclr = 1
			reg_mwr(csi_base_addr[dt_info->controller_id] + 0x48, BIT_0, BIT_0);
			udelay(100);
			//testclr = 0;
			reg_mwr(csi_base_addr[dt_info->controller_id] + 0x48, BIT_0, ~(BIT_0));

			pr_info("set phy force shutdownz\n");
			reg_mwr(0x323f0000 + 0x58, BIT_26, BIT_26); //CSI force S shutdownz
			reg_mwr(0x323f0000 + 0x58, BIT_28, BIT_28); //CSI force shutdownz
			dphy_csi_match(dt_info);
		}

		if (0) {
			dphy_2p2l_init(phy_id, dt_info->controller_id, sensor_id);
			dphy_csi_match(dt_info);
		}

		ret = csi_ahb_reset(dt_info, dt_info->controller_id);
		if (unlikely(ret))
			goto EXIT;
	}

	ret = request_irq(dt_info->irq_e0, csi_int0_func,
                     IRQF_TRIGGER_HIGH | IRQF_SHARED,
                     irqname[dt_info->controller_id*2], dt_info);
	if (ret < 0)
			pr_info("irq0 register faild: %s\n",
				irqname[dt_info->controller_id*2]);

	ret = request_irq(dt_info->irq_e1, csi_int1_func,
	                     IRQF_TRIGGER_HIGH | IRQF_SHARED,
	                     irqname[dt_info->controller_id*2 + 1], dt_info);
	if (ret < 0)
			pr_info("irq1 register faild: %s\n",
				irqname[dt_info->controller_id*2 + 1]);
	csi_start(sensor_id, dt_info->controller_id);
	csi_set_on_lanes(lane_num, sensor_id);
	if (is_pattern)
		csi_ipg_mode_cfg(sensor_id, 1, 0, 4224, 3136);

	pr_info("exit success\n");

	return ret;
EXIT:
	pr_err("fail to open csi %d\n", ret);
	csi_api_close(phy_id, sensor_id);
	return ret;
}

int csi_api_close(uint32_t phy_id, int sensor_id)
{
	int ret = 0;
	struct csi_dt_node_info *dt_info = NULL;

	dt_info = csi_get_dt_node_data(sensor_id);
	if (!dt_info) {
		pr_err("fail to get valid phy ptr\n");
		return -EINVAL;
	}
	free_irq(dt_info->irq_e0, dt_info);
	free_irq(dt_info->irq_e1, dt_info);

	if (CSI_PATTERN_ENABLE)
		csi_ipg_mode_cfg(sensor_id, 0, 0, 4224, 3136);
	csi_close(sensor_id, dt_info->controller_id);
	csi_controller_disable(dt_info, sensor_id);
	if (g_project_id == 0)
		csi_phy_power_down(phy_id, dt_info->controller_id, sensor_id, 1);
	csi_mipi_clk_disable(sensor_id);
	pr_info("csi api close ret: %d\n", ret);

	return ret;
}

void csi_api_reg_trace(void)
{
	int i = 0;

	for (i = 0; i < CSI_MAX_COUNT; i++)
		csi_reg_trace(i);
}
EXPORT_SYMBOL(csi_api_reg_trace);


int csi_api_switch(int sensor_id)
{
	return 0;
}

