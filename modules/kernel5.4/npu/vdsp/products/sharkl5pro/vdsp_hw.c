/*
* SPDX-FileCopyrightText: 2019-2022 Unisoc (Shanghai) Technologies Co., Ltd
* SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
*
* Copyright 2019-2022 Unisoc (Shanghai) Technologies Co., Ltd.
* Licensed under the Unisoc General Software License, version 1.0 (the License);
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
* Software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* See the Unisoc General Software License, version 1.0 for more details.
*/

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/soc/sprd/hwfeature.h>
#include <asm/cacheflush.h>
#include "vdsp_debugfs.h"
#include "vdsp_hw.h"
#include "vdsp_qos.h"
#include "xrp_internal.h"
#include "xrp_kernel_defs.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: hw %d: %d %s:" \
	fmt, current->pid, __LINE__, __func__


#define PMU_SET_OFFSET		0x1000
#define PMU_CLR_OFFSET		0x2000
#define APAHB_SET_OFFSET	0x1000
#define APAHB_CLR_OFFSET	0x2000

 /***********************register function********************/
static int vdsp_regmap_read(struct regmap *regmap, uint32_t reg, uint32_t *val)
{
	return regmap_read(regmap, reg, val);
}

static int vdsp_regmap_write(struct regmap *regmap, uint32_t reg, uint32_t val)
{
	return regmap_write(regmap, reg, val);
}

int vdsp_regmap_read_mask(struct regmap *regmap, uint32_t reg,
	uint32_t mask, uint32_t *val)
{
	int ret = 0;

	if ((!(regmap)) || (!val))
		return -EINVAL;
	ret = regmap_read(regmap, reg, val);
	if (!ret)
		*val &= ( uint32_t) mask;

	return ret;
}

static int regmap_set(struct regmap *regmap, uint32_t offset, uint32_t val, enum reg_type rt)
{
	int ret = -1;

	switch (rt) {
	case RT_PMU:
		ret = vdsp_regmap_write(regmap, offset + PMU_SET_OFFSET, val);
		break;
	case RT_APAHB:
		ret = vdsp_regmap_write(regmap, offset + APAHB_SET_OFFSET, val);
		break;
	default:
		break;
	};
	pr_debug("return:%d, reg type:%d\n", ret, rt);
	return ret;
}

static int regmap_clr(struct regmap *regmap, uint32_t offset, uint32_t val, enum reg_type rt)
{
	int ret = -1;

	switch (rt) {
	case RT_PMU:
		ret = vdsp_regmap_write(regmap, offset + PMU_CLR_OFFSET, val);
		break;
	case RT_APAHB:
		ret = vdsp_regmap_write(regmap, offset + APAHB_CLR_OFFSET, val);
		break;
	default:
		break;
	};
	pr_debug("return:%d, reg type:%d\n", ret, rt);
	return ret;
}

static int vdsp_regmap_set_clr(struct regmap *regmap, uint32_t offset,
	uint32_t mask, uint32_t val, enum reg_type rt)
{
	uint32_t set, clr;
	int ret = 0;

	set = val & mask;
	clr = (~val) & mask;

	pr_debug("regmap:%#lx, offset:%#x, mask:%#x val:%#x,rt:%#x\n", regmap, offset, mask, val, rt);

	if (set) {
		ret = regmap_set(regmap, offset, set, rt);
		if (ret) {
			pr_err("regmap_set failed, regmap:%#lx, offset:%#x, set:%#x,rt:%#x\n",
				regmap, offset, set, rt);
			goto end;
		}
	}
	if (clr) {
		ret = regmap_clr(regmap, offset, clr, rt);
		if (ret) {
			pr_err("regmap_clr failed, regmap:%#lx, offset:%#x, clr:%#x,rt:%#x\n",
				regmap, offset, clr, rt);
			goto end;
		}
	}
end:
	return ret;
}

static int vdsp_regmap_not_set_clr(struct regmap *regmap, uint32_t offset,
	uint32_t mask, uint32_t val, enum reg_type rt)
{
	uint32_t tmp;
	int ret = -1;

	pr_debug("regmap:%#lx, offset:%#x, mask:%#x, val:%#x, rt:%#x\n", regmap, offset, mask, val, rt);

	ret = vdsp_regmap_read(regmap, offset, &tmp);
	if (ret) {
		pr_err("regmap read  error!\n");
		return ret;
	}
	tmp &= ~mask;
	ret = vdsp_regmap_write(regmap, offset, tmp | (val & mask));

	return ret;
}

int vdsp_regmap_update_bits(struct regmap *regmap, uint32_t offset,
	uint32_t mask, uint32_t val, enum reg_type rt)
{
	if (rt == RT_PMU || rt == RT_APAHB) {
		return vdsp_regmap_set_clr(regmap, offset, mask, val, rt);
	} else if (rt == RT_NO_SET_CLR) {
		return vdsp_regmap_not_set_clr(regmap, offset, mask, val, rt);
	} else {
		pr_err("[error]input unknowned reg type\n");
		return -1;
	}
}

/***********************register function end********************/

static void *get_hw_sync_data(void *hw_arg, size_t *sz, uint32_t log_addr)
{
	static const u32 irq_mode[] = {
		[XRP_IRQ_NONE] = XRP_DSP_SYNC_IRQ_MODE_NONE,
		[XRP_IRQ_LEVEL] = XRP_DSP_SYNC_IRQ_MODE_LEVEL,
		[XRP_IRQ_EDGE] = XRP_DSP_SYNC_IRQ_MODE_EDGE,
		[XRP_IRQ_EDGE_SW] = XRP_DSP_SYNC_IRQ_MODE_EDGE,
	};
	struct vdsp_hw *hw = hw_arg;
	struct vdsp_side_sync_data *hw_sync_data = kmalloc(sizeof(*hw_sync_data), GFP_KERNEL);

	if (!hw_sync_data) {
		pr_err("hw_sync_data is NULL !!!\n");
		return NULL;
	}

	*hw_sync_data = (struct vdsp_side_sync_data) {
		.device_mmio_base = hw->ipi_phys,
		.host_irq_mode = hw->host_irq_mode,
		.host_irq_offset = hw->host_irq[0],
		.host_irq_bit = hw->host_irq[1],
		.device_irq_mode = irq_mode[hw->device_irq_mode],
		.device_irq_offset = hw->device_irq[0],
		.device_irq_bit = hw->device_irq[1],
		.device_irq = hw->device_irq[2],
		.vdsp_smsg_addr = (unsigned int)*sz,
		.vdsp_log_addr = log_addr,
	};
	pr_debug("device_mmio_base:%lx, (host_irq)mode:%d, offset:%d, bit:%d,"
		"(device_irq)mode:%d, offset:%d, bit:%d, irq:%d,"
		"vdsp_smsg addr:0x%lx, vdsp_log_addr:0x%lx\n",
		hw_sync_data->device_mmio_base, hw_sync_data->host_irq_mode,
		hw_sync_data->host_irq_offset, hw_sync_data->host_irq_bit,
		hw_sync_data->device_irq_mode, hw_sync_data->device_irq_offset,
		hw_sync_data->device_irq_bit, hw_sync_data->device_irq,
		hw_sync_data->vdsp_smsg_addr, hw_sync_data->vdsp_log_addr);

	*sz = sizeof(*hw_sync_data);

	return hw_sync_data;
}

static void reset(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	pr_debug("arg:%p ,offset:%x, value:0\n", hw->ahb_regmap, REG_RESET);
	vdsp_regmap_update_bits(hw->ahb_regmap, REG_RESET, (0x3 << 9), (0x3 << 9), RT_APAHB);
	udelay(10);
	vdsp_regmap_update_bits(hw->ahb_regmap, REG_RESET, (0x3 << 9), 0, RT_APAHB);
}

static void halt(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	pr_debug("arg:%p ,offset:%x, value:1\n", hw->ahb_regmap, REG_RUNSTALL);
	vdsp_regmap_update_bits(hw->ahb_regmap, REG_RUNSTALL, (0x1 << 2), (0x1 << 2), RT_NO_SET_CLR);
}

static void release(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	pr_debug("arg:%p ,offset:%x, value:0\n", hw->ahb_regmap, REG_RUNSTALL);
	vdsp_regmap_update_bits(hw->ahb_regmap, REG_RUNSTALL, (0x1 << 2), 0, RT_NO_SET_CLR);
}


static int enable(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;
	uint32_t rdata = 1;
	int ret = 0;

	/*pd_ap_vdsp_force_shutdown bit */
	vdsp_regmap_update_bits(hw->pmu_regmap, REG_PD_AP_VDSP_CORE_INT_DISABLE, (0x1 << 0), 0, RT_PMU);
	vdsp_regmap_update_bits(hw->pmu_regmap, REG_PD_AP_VDSP_CFG, (0x1 << 25), 0, RT_PMU);
	vdsp_regmap_update_bits(hw->pmu_regmap, REG_PD_AP_VDSP_DLSP_ENA, (0x1 << 0), 0, RT_PMU);
	/*vdsp_stop_en */
	vdsp_regmap_update_bits(hw->ahb_regmap, REG_LP_CTL, 0xC, 0x8, RT_NO_SET_CLR);
	/*isppll open for 936M */
	vdsp_regmap_update_bits(hw->pmu_regmap, REG_ISPPLL_REL_CFG, (0x1 << 0), 0x1, RT_PMU);
	/* loop PD_AD_VDSP_STATE */
	do {
		if (vdsp_regmap_read_mask(hw->pmu_regmap, 0xbc, 0xff000000, &rdata)) {
			pr_err("[error] get vdsp power states");
			ret = -ENXIO;
		}
	} while (rdata);

	/* IPI &vdma enable */
	vdsp_regmap_update_bits(hw->ahb_regmap, 0x0, (0x1 << 6) | (0x1 << 3), (0x1 << 6) | (0x1 << 3), RT_APAHB);
	/*vdsp_all_int_mask = 0 */
	vdsp_regmap_update_bits(hw->ahb_regmap, REG_VDSP_INT_CTL, (0x1 << 13), 0, RT_NO_SET_CLR);

	return ret;
}

static void disable(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;
	uint32_t rdata = 0;
	uint32_t count = 0;

	/*vdma&IPI  disable */
	vdsp_regmap_update_bits(hw->ahb_regmap, 0x0, (0x1 << 3) | (0x1 << 6), 0, RT_APAHB);
	/*vdsp_stop_en = 1 */
	vdsp_regmap_update_bits(hw->ahb_regmap, REG_LP_CTL, (0x1 << 2), (0x1 << 2), RT_NO_SET_CLR);
	/*mask all int */
	vdsp_regmap_update_bits(hw->ahb_regmap, REG_VDSP_INT_CTL, 0x1ffff, 0x1ffff, RT_NO_SET_CLR);
	/*pmu ap_vdsp_core_int_disable set 1 */
	vdsp_regmap_update_bits(hw->pmu_regmap, REG_PD_AP_VDSP_CORE_INT_DISABLE,
		0x1, 0x1, RT_PMU);
	udelay(1);
	/*wait for vdsp enter pwait mode */
	while (((rdata & (0x1 << 5)) == 0) && (count < 100)) {
		count++;
		if (vdsp_regmap_read_mask(hw->ahb_regmap, REG_LP_CTL, 0x20, &rdata)) {
			pr_err("[error] get vdsp pwaitmode");
		}
		/*delay 1 ms */
		udelay(1000);
	}
	pr_debug("disable wait count:%d\n", count);
	if (count < 100) {
		/*pmu auto shutdown by vdsp core */
		vdsp_regmap_update_bits(hw->pmu_regmap, REG_PD_AP_VDSP_CFG, 0x9000000, 0x1000000, RT_PMU);	/*bit 24 27 */
		vdsp_regmap_update_bits(hw->pmu_regmap, REG_PD_AP_VDSP_DLSP_ENA, 0x1, 0x1, RT_PMU);
	} else {
		pr_err("timed out need force shut down\n");
		/*bit25 =1 , bit24 = 0 */
		vdsp_regmap_update_bits(hw->pmu_regmap, REG_PD_AP_VDSP_CFG, 0xb000000, 0x2000000, RT_PMU);
	}
}

static void send_irq(void *hw_arg)
{
	struct vdsp_hw *hw = hw_arg;

	hw->vdsp_ipi_desc->ops->irq_send(hw->device_irq[1]);
}

static irqreturn_t xrp_hw_irq_handler(int irq, void *dev_id)
{
	struct vdsp_hw *hw = dev_id;

	return xrp_irq_handler(irq, hw->xrp);
}

static irqreturn_t xrp_hw_log_irq_handler(int irq, void *dev_id)
{
	struct vdsp_hw *hw = dev_id;

	return vdsp_log_irq_handler(irq, hw->xrp);
}

int vdsp_request_irq(void *xvp_arg, void *hw_arg)
{
	struct device *dev = (struct device *)xvp_arg;
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;
	int ret;

	pr_debug("dev %p ,request irq %d handle %p hw %p\n",
		dev, hw->client_irq, hw->vdsp_ipi_desc->ops->irq_handler, hw);
	ret = devm_request_irq(dev, hw->client_irq, hw->vdsp_ipi_desc->ops->irq_handler,
		IRQF_SHARED, DRIVER_NAME, hw);
	if (ret < 0) {
		pr_err("devm_request_irq fail, ret %d\n", ret);
		return ret;
	}

	return ret;
}

void vdsp_free_irq(void *xvp_arg, void *hw_arg)
{
	struct device *dev = (struct device *)xvp_arg;
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	pr_debug("free irq %d dev %p hw %p\n", hw->client_irq, dev, hw);
	devm_free_irq(dev, hw->client_irq, hw);
}

static const struct xrp_hw_ops hw_ops = {
	.halt = halt,
	.release = release,
	.reset = reset,
	.get_hw_sync_data = get_hw_sync_data,
	.send_irq = send_irq,
	.memcpy_tohw = NULL, /*memcpy_hw_function*/
	.memset_hw = NULL, /*memset_hw_function*/
	.enable = enable,
	.disable = disable,
	.set_qos = set_qos,
	.vdsp_request_irq = vdsp_request_irq,
	.vdsp_free_irq = vdsp_free_irq,
};

static long sprd_vdsp_parse_hw_dt(struct platform_device *pdev,
	struct vdsp_hw *hw, int mem_idx, enum vdsp_init_flags *init_flags)
{
	long ret;
	struct resource *mem;
	struct device_node *np;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, mem_idx);
	if (unlikely(!mem)) {
		pr_err("get mem_idx:%d failed\n", mem_idx);
		return -ENODEV;
	}
	hw->ipi_phys = mem->start;
	hw->ipi = devm_ioremap_resource(&pdev->dev, mem);
	pr_debug("ipi = %pap/%p\n", &mem->start, hw->ipi);

	np = pdev->dev.of_node;
	if (np == NULL) {
		pr_err("np is null\n");
		return -EINVAL;
	}
	hw->ahb_regmap = syscon_regmap_lookup_by_phandle(np, "sprd,syscon-ap-ahb");
	if (IS_ERR(hw->ahb_regmap)) {
		pr_err("can not get ahb_regmap regmap struct!\n");
		return -EINVAL;
	}
	hw->pmu_regmap = syscon_regmap_lookup_by_phandle(np, "sprd,syscon-pmu");
	if (IS_ERR(hw->ahb_regmap)) {
		pr_err("can not get pmu_regmap regmap struct!\n");
		return -EINVAL;
	}
	/* qos */
	parse_qos(hw, pdev->dev.of_node);
	/* irq */
	ret = of_property_read_u32_array(pdev->dev.of_node, "device-irq", hw->device_irq,
		ARRAY_SIZE(hw->device_irq));
	if (ret) {
		pr_debug("using polling mode on the device side\n");
	} else {
		u32 device_irq_host_offset = 0;
		u32 device_irq_mode = 0;

		ret = of_property_read_u32(pdev->dev.of_node, "device-irq-host-offset",
			&device_irq_host_offset);
		if (ret == 0)
			hw->device_irq_host_offset = device_irq_host_offset;
		else
			hw->device_irq_host_offset = hw->device_irq[0];

		ret = of_property_read_u32(pdev->dev.of_node, "device-irq-mode", &device_irq_mode);
		if (likely(device_irq_mode < XRP_IRQ_MAX)) {
			hw->device_irq_mode = device_irq_mode;
			pr_debug("device IRQ MMIO host offset = 0x%08x,"
				"offset = 0x%08x, bit = %d,"
				"device IRQ = %d, IRQ mode = %d",
				hw->device_irq_host_offset,
				hw->device_irq[0], hw->device_irq[1],
				hw->device_irq[2], hw->device_irq_mode);
		}
	}

	ret = of_property_read_u32_array(pdev->dev.of_node, "host-irq", hw->host_irq,
		ARRAY_SIZE(hw->host_irq));
	if (ret == 0) {
		u32 host_irq_mode;

		ret = of_property_read_u32(pdev->dev.of_node, "host-irq-mode", &host_irq_mode);
		if (likely(host_irq_mode < XRP_IRQ_MAX))
			hw->host_irq_mode = host_irq_mode;
		else
			return -ENOENT;
	}

	if (ret == 0 && hw->host_irq_mode != XRP_IRQ_NONE)
		hw->client_irq = platform_get_irq(pdev, 0);
	else
		hw->client_irq = -1;

	pr_debug("irq is:%d , ret:%ld , host_irq_mode:%d\n", hw->client_irq, ret, hw->host_irq_mode);
	if (hw->client_irq >= 0) {
		hw->vdsp_ipi_desc = get_vdsp_ipi_ctx_desc();
		if (hw->vdsp_ipi_desc) {
			hw->vdsp_ipi_desc->base_addr = hw->ahb_regmap;
			hw->vdsp_ipi_desc->ipi_addr = hw->ipi;
			hw->vdsp_ipi_desc->irq_mode = hw->host_irq_mode;

			ret = vdsp_request_irq(&pdev->dev, hw);
			if (ret < 0) {
				pr_err("request_irq %d failed\n", hw->client_irq);
				goto err;
			}

			hw->vdsp_ipi_desc->ops->irq_register(0, xrp_hw_irq_handler, hw);
			hw->vdsp_ipi_desc->ops->irq_register(1, xrp_hw_irq_handler, hw);
			hw->vdsp_ipi_desc->ops->irq_register(2, xrp_hw_log_irq_handler, hw);

			*init_flags |= XRP_INIT_USE_HOST_IRQ;
		}
	} else {
		pr_debug("using polling mode on the host side\n");
	}
	ret = 0;
err:
	return ret;
}

static long init_sprd(struct platform_device *pdev, struct vdsp_hw *hw)
{
	long ret;
	enum vdsp_init_flags init_flags = 0;

	ret = sprd_vdsp_parse_hw_dt(pdev, hw, 0, &init_flags);
	if (unlikely(ret < 0))
		return ret;
	return sprd_vdsp_init(pdev, init_flags, &hw_ops, hw);
}

// #ifdef CONFIG_OF
static const struct of_device_id vdsp_device_match[] = {
	{
		.compatible = "sprd,sharkl5pro-vdsp",
		.data = init_sprd,
	},
	{},
};

MODULE_DEVICE_TABLE(of, vdsp_device_match);
// #endif

static int vdsp_driver_probe(struct platform_device *pdev)
{
	struct vdsp_hw *hw = devm_kzalloc(&pdev->dev, sizeof(*hw), GFP_KERNEL);
	const struct of_device_id *match;
	long (*init) (struct platform_device *pdev, struct vdsp_hw *hw);
	long ret;

	pr_info(" sprd-vdsp: vdsp_driver probe \n");//for debug
	if (!hw)
		return -ENOMEM;

	match = of_match_device(of_match_ptr(vdsp_device_match), &pdev->dev);
	if (!match)
		return -ENODEV;

	init = match->data;
	ret = init(pdev, hw);
	if (IS_ERR_VALUE(ret)) {
		return ret;
	} else {
		hw->xrp = ERR_PTR(ret);
		return 0;
	}
}

static int vdsp_driver_remove(struct platform_device *pdev)
{
	return sprd_vdsp_deinit(pdev);
}

static const struct dev_pm_ops vdsp_pm_ops = {
	SET_RUNTIME_PM_OPS(vdsp_runtime_suspend, vdsp_runtime_resume, NULL)
};

static struct platform_driver vdsp_driver = {
	.probe = vdsp_driver_probe,
	.remove = vdsp_driver_remove,
	.driver = {
		   .name = DRIVER_NAME,
		   .of_match_table = of_match_ptr(vdsp_device_match),
		   .pm = &vdsp_pm_ops,
		   },
};

module_platform_driver(vdsp_driver);

MODULE_DESCRIPTION("Sprd VDSP Driver");
MODULE_AUTHOR("Vdsp@unisoc");
MODULE_LICENSE("GPL");
