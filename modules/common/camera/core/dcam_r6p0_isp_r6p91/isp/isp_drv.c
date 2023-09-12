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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/vmalloc.h>
#include <linux/sprd_ion.h>
/*#include <mach/sci.h>*/
#include <linux/regmap.h>
/*#include <mach/hardware.h>*/
#include <asm/cacheflush.h>
#include "sprd_isp_r6p91.h"
/*#include "parse_hwinfo.h"*/
/*#include "compat_isp_drv.h"*/
#include "isp_drv.h"
#include "isp_int.h"
#include "isp_buf.h"
#include "isp_reg.h"
#include <sprd_mm.h>
#include <sprd_img.h>
#include <linux/mfd/syscon.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <linux/pm_runtime.h>
#else
#include <video/sprd_mmsys_pw_domain.h>
#endif
#include <dt-bindings/soc/sprd,pike2-mask.h>
#include <dt-bindings/soc/sprd,pike2-regs.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_DRV %d:" fmt,  __LINE__

#define ISP_TIME_OUT_MAX (500)

struct platform_device *s_isp_pdev;
unsigned long s_isp_regbase;
static unsigned int s_isp_irq;
spinlock_t isp_mod_lock;

static struct clk *isp_clk;
static struct clk *isp_clk_high_parent;
static struct clk *isp_clk_low_parent;
static struct clk *isp_clk_default;
static struct clk *isp_axi_clk;
static struct clk *isp_axi_clk_parent;
static struct clk *isp_axi_clk_default;
static struct clk *isp_axi_eb;
static struct clk *isp_eb;
static struct regmap *cam_ahb_gpr;
static struct regmap *aon_apb_gpr;

static void isp_int_init(void)
{
	/*clear the whole int*/
	ISP_REG_WR(ISP_INT_CLR0, ISP_IRQ_HW_MASK);
	ISP_REG_WR(ISP_INT_CLR1, ISP_IRQ_HW_MASK);
	ISP_REG_WR(ISP_INT_CLR2, ISP_IRQ_HW_MASK);
	ISP_REG_WR(ISP_INT_CLR3, ISP_IRQ_HW_MASK);

	/*enable the needed int*/
	ISP_REG_WR(ISP_INT_EN0, ISP_INT_LINE_MASK0);
	ISP_REG_WR(ISP_INT_EN1, ISP_INT_LINE_MASK1);
	ISP_REG_WR(ISP_INT_EN2, ISP_INT_LINE_MASK2);
	ISP_REG_WR(ISP_INT_EN3, ISP_INT_LINE_MASK3);
	pr_debug("0x%x 0x%x 0x%x 0x%x\n",
		ISP_INT_LINE_MASK0,
		ISP_INT_LINE_MASK1,
		ISP_INT_LINE_MASK2,
		ISP_INT_LINE_MASK3);
}

static void isp_int_deinit(void)
{
	/*disable the whole int first*/
	ISP_REG_WR(ISP_INT_EN0, 0);
	ISP_REG_WR(ISP_INT_EN1, 0);
	ISP_REG_WR(ISP_INT_EN2, 0);
	ISP_REG_WR(ISP_INT_EN3, 0);

	/*clear the whole int*/
	ISP_REG_WR(ISP_INT_CLR0, ISP_IRQ_HW_MASK);
	ISP_REG_WR(ISP_INT_CLR1, ISP_IRQ_HW_MASK);
	ISP_REG_WR(ISP_INT_CLR2, ISP_IRQ_HW_MASK);
	ISP_REG_WR(ISP_INT_CLR3, ISP_IRQ_HW_MASK);
}

int isp_axi_waiting(void)
{
	int ret = 0;
	uint32_t reg_value = 0;
	uint32_t time_out_cnt = 0;

	isp_int_deinit();
	ISP_REG_OWR(ISP_AXI_ITI2AXIM_CTRL, BIT_26);

	reg_value = ISP_REG_RD(ISP_INT_STATUS);
	while ((0x00 == (reg_value & BIT_3))
		&& (time_out_cnt < ISP_TIME_OUT_MAX)) {
		time_out_cnt++;
		udelay(50);
		reg_value = ISP_REG_RD(ISP_INT_STATUS);
	}

	if (time_out_cnt >= ISP_TIME_OUT_MAX) {
		ret = -1;
		pr_info("isp_axi_bus_waiting: time out.\n");
	}

	return ret;
}

int isp_set_statis_buf(void *isp_pipe_dev_handle)
{
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	struct isp_statis_module *statis_module = NULL;
	struct isp_pipe_dev *dev = NULL;

	if (!isp_pipe_dev_handle) {
		pr_err("invalid dev!\n");
		return -1;
	}
	dev = (struct isp_pipe_dev *)isp_pipe_dev_handle;
	statis_module = &dev->statis_module_info;

	rtn = isp_set_next_statis_buf(statis_module,
				      ISP_AEM_BLOCK);
	if (rtn) {
		pr_err("fail to set next AEM statis buf\n");
		return -(rtn);
	}
	rtn = isp_set_next_statis_buf(statis_module,
				      ISP_AFL_BLOCK);
	if (rtn) {
		pr_err("fail to set next AFL statis buf\n");
		return -(rtn);
	}
	rtn = isp_set_next_statis_buf(statis_module,
				      ISP_AFM_BLOCK);
	if (rtn) {
		pr_err("fail to set next AFM statis buf\n");
		return -(rtn);
	}
	rtn = isp_set_next_statis_buf(statis_module,
				      ISP_BINNING_BLOCK);
	if (rtn) {
		pr_err("fail to set next binning statis buf\n");
		return -(rtn);
	}
	rtn = isp_set_next_statis_buf(statis_module,
				      ISP_HIST_BLOCK);
	if (rtn) {
		pr_err("fail to set next hist statis buf\n");
		return -(rtn);
	}
/*
	if (statis_module->img_statis_buf.dev_fd <= 0)
		goto exit;
	statis_module->img_statis_buf.ion =
	  (void *)get_ion_buffer(statis_module->img_statis_buf.dev_fd, NULL);
	if ((unsigned int)(statis_module->img_statis_buf.ion) > 0xffffff00) {
		statis_module->img_statis_buf.ion = NULL;
		pr_err("failed to get ion client of statis buf, %ld\n",
			statis_module->img_statis_buf.dev_fd);
	}

exit:
*/
	return rtn;
}

int isp_reset(void)
{
	int ret = 0;
	unsigned int flag = 0;
	ret = isp_axi_waiting();

	flag = MASK_MM_AHB_ISP_LOG_SOFT_RST |
		MASK_MM_AHB_ISP_CFG_SOFT_RST;
	regmap_update_bits(cam_ahb_gpr,
		REG_MM_AHB_AHB_RST, flag, flag);
	udelay(1);
	regmap_update_bits(cam_ahb_gpr,
		REG_MM_AHB_AHB_RST, flag, ~flag);
	ISP_REG_WR(ISP_AXI_ISOLATION, 0);
	isp_int_init();
	/*bypass binning4awb after isp reset*/
	ISP_REG_MWR(ISP_BINNING_PARAM, BIT_0, 1);
	/*bypass antiflicker after isp reset*/
	ISP_REG_MWR(ISP_ANTI_FLICKER_PARAM0, BIT_0, 1);

	sprd_iommu_restore(&s_isp_pdev->dev);

	return ret;
}

static int isp_enable_clk(void)
{
	int ret = 0;

	/*set isp clock to max value*/
	ret = clk_set_parent(isp_clk, isp_clk_high_parent);
	if (ret) {
		pr_info("isp_clk_high_parent error.\n");
		clk_set_parent(isp_clk, isp_clk_default);
		goto exit;
	}

	ret = clk_prepare_enable(isp_clk);
	if (ret) {
		pr_info("isp_clk error.\n");
		clk_set_parent(isp_clk, isp_clk_default);
		goto exit;
	}

	ret = clk_set_parent(isp_axi_clk, isp_axi_clk_parent);
	if (ret) {
		pr_info("isp_axi_clk_parent error.\n");
		clk_set_parent(isp_axi_clk, isp_axi_clk_default);
		clk_disable_unprepare(isp_clk);
		clk_set_parent(isp_clk, isp_clk_default);
		goto exit;
	}

	ret = clk_prepare_enable(isp_axi_clk);
	if (ret) {
		pr_info("isp_axi_clk error.\n");
		clk_set_parent(isp_axi_clk, isp_axi_clk_default);
		clk_disable_unprepare(isp_clk);
		clk_set_parent(isp_clk, isp_clk_default);
		goto exit;
	}

	ret = clk_prepare_enable(isp_eb);
	if (ret) {
		pr_info("isp_eb error.\n");
		goto exit;
	}

	ret = clk_prepare_enable(isp_axi_eb);
	if (ret) {
		pr_info("isp_axi_eb error.\n");
		goto exit;
	}

	pr_info("isp_eb_clk end.\n");
exit:
	return ret;
}

int switch_isp_clk(unsigned char high)
{
	int ret = 0;

	clk_set_parent(isp_clk, isp_clk_default);
	clk_disable_unprepare(isp_clk);

	if (high == 1) {
		pr_info("switch isp clk to high!\n");
		/*set isp clock to max value*/
		ret = clk_set_parent(isp_clk, isp_clk_high_parent);
		if (ret) {
			pr_info("isp_clk_low_parent error.\n");
			clk_set_parent(isp_clk, isp_clk_default);
			goto exit;
		}
	} else {
		pr_info("switch isp clk to low!\n");
		/*set isp clock to low value*/
		ret = clk_set_parent(isp_clk, isp_clk_low_parent);
		if (ret) {
			pr_info("isp_clk_low_parent error.\n");
			clk_set_parent(isp_clk, isp_clk_default);
			goto exit;
		}
	}
	ret = clk_prepare_enable(isp_clk);
	if (ret) {
		pr_info("isp_clk error.\n");
		clk_set_parent(isp_clk, isp_clk_default);
		goto exit;
	}
exit:
	return ret;
}

static int isp_disable_clk(void)
{
	clk_disable_unprepare(isp_eb);
	clk_disable_unprepare(isp_axi_eb);

	/*set isp clock to default value before power off*/
	clk_set_parent(isp_clk, isp_clk_default);
	clk_disable_unprepare(isp_clk);

	clk_set_parent(isp_axi_clk, isp_axi_clk_default);
	clk_disable_unprepare(isp_axi_clk);

	return 0;
}

int sprd_isp_reg_isr(enum isp_irq_id irq_id, isp_isr_func user_func,
			void *user_data)
{
	int ret = 0;

	pr_debug("%s:irq_id %d\n", __func__, irq_id);
	if (irq_id >= ISP_IMG_MAX) {
		pr_err("isp IRQ is error.\n");
		ret = ISP_RTN_IRQ_NUM_ERR;
	} else {
		pr_debug("%s:irq user_data %p\n", __func__, user_data);
		ret = isp_irq_callback(irq_id, user_func, user_data);
		if (ret)
			pr_err("Register isp callback error\n");
	}

	return ret;
}

int sprd_isp_k_ioctl(void *isp_pipe_dev_handle,
		     unsigned int cmd, unsigned long param)
{
	int ret = 0;
	struct isp_pipe_dev *dev = NULL;
	struct isp_statis_buf_input parm_inptr;
	struct isp_k_block *isp_k_param = NULL;

	if (!isp_pipe_dev_handle) {
		ret = -EFAULT;
		pr_err("isp dev handle is NULL\n");
		goto exit;
	}
	dev = (struct isp_pipe_dev *)isp_pipe_dev_handle;

	pr_debug("cmd: 0x%x, %d\n", cmd, _IOC_NR(cmd));
	isp_k_param = &dev->isp_k_param;
	if (!isp_k_param) {
		ret = -EFAULT;
		pr_err("isp_ioctl: isp_private is null error.\n");
		goto exit;
	}
	switch (cmd) {
	case SPRD_ISP_IO_CFG_START:
		isp_k_param->lsc_2d_weight_en = 0;
		isp_k_param->is_lsc_param_init_flag = 0;
		isp_k_param->lsc_param_load_count = 0;
		atomic_set(&isp_k_param->lsc_updated, 0);
		break;
	case SPRD_ISP_IO_CFG_PARAM:
		ret = isp_cfg_param((void *)param, isp_k_param);
		break;
	case SPRD_ISP_IO_SET_STATIS_BUF:
		mutex_lock(&dev->isp_mutex);
		ret = copy_from_user(&parm_inptr,
				     (void __user *)param,
				     sizeof(struct isp_statis_buf_input));
		if (ret != 0) {
			pr_err("isp_ioctl: copy_from_user error\n");
			mutex_unlock(&dev->isp_mutex);
			goto exit;
		}
		if (parm_inptr.buf_flag == STATIS_BUF_FLAG_INIT) {
			ret = sprd_isp_cfg_statis_buf(dev, &parm_inptr);
			dev->statis_module_info.img_statis_buf.dev_fd =
				parm_inptr.dev_fd;
		} else
			ret = sprd_isp_set_statis_addr(dev, &parm_inptr);

		mutex_unlock(&dev->isp_mutex);
		break;
	case SPRD_ISP_IO_CAPABILITY:
		ret = isp_capability((void *)param);
		break;
	case SPRD_ISP_IO_RAW_CAP:
		mutex_lock(&dev->isp_mutex);
		ret = isp_set_statis_buf(dev);
		if (ret) {
			pr_err("isp_ioctl: set statis buf error\n");
			ISP_REG_MWR(ISP_AEM_PARAM, BIT_0, 1);
		} else {
			ISP_REG_MWR(ISP_AEM_PARAM, BIT_0, 0);
		}
		mutex_unlock(&dev->isp_mutex);
		break;
	default:
		pr_err("isp_ioctl, cmd is unsupported: 0x%x\n",
			(int32_t)cmd);
		ret = -EFAULT;
		goto exit;
	}

exit:
	return ret;
}

static int32_t isp_block_buf_alloc(struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;
	uint32_t buf_len = 0;

	buf_len = sizeof(struct isp_dev_gamma_info);

	isp_k_param->full_gamma_buf_addr = (unsigned long)vzalloc(buf_len);
	if (isp_k_param->full_gamma_buf_addr == 0) {
		ret = -1;
		pr_err("isp_block_buf_alloc: no memory error.\n");
	}

	return ret;
}

static int32_t isp_block_buf_free(struct isp_k_block *isp_k_param)
{
	if (isp_k_param->full_gamma_buf_addr != 0x00) {
		vfree((void *)isp_k_param->full_gamma_buf_addr);
		isp_k_param->full_gamma_buf_addr = 0x00;
	}

	return 0;
}

int sprd_isp_dev_init(void **isp_pipe_dev_handle)
{
	int ret = 0;
	struct isp_pipe_dev *dev = NULL;
	struct isp_k_block *isp_k_param = NULL;

	if (!isp_pipe_dev_handle) {
		pr_err("%s input handle is NULL\n", __func__);
		return -EINVAL;
	}

	dev = vzalloc(sizeof(*dev));
	if (dev == NULL)
		return -ENOMEM;

	mutex_init(&dev->isp_mutex);
	isp_k_param = &dev->isp_k_param;
	ret = isp_block_buf_alloc(isp_k_param);
	if (ret != 0)
		goto exit;
	ret = isp_statis_queue_init(&dev->statis_module_info.aem_statis_queue);
	ret = isp_statis_queue_init(&dev->statis_module_info.afl_statis_queue);
	ret = isp_statis_queue_init(&dev->statis_module_info.afm_statis_queue);
	ret = isp_statis_queue_init(
		&dev->statis_module_info.binning_statis_queue);
	ret = isp_statis_queue_init(&dev->statis_module_info.hist_statis_queue);
	atomic_set(&isp_k_param->lsc_updated, 0);
	mutex_lock(&dev->isp_mutex);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	ret = sprd_cam_pw_on();
	if (ret != 0) {
		pr_err("%s : sprd cam power on failed\n", __func__);
		mutex_unlock(&dev->isp_mutex);
		goto irq_exit;
	}
	sprd_cam_domain_eb();
#endif
	isp_enable_clk();

	ret = isp_irq_request(&s_isp_pdev->dev, s_isp_irq, dev);
	if (unlikely(ret != 0)) {
		ret = -EIO;
		pr_err("fail to install isp IRQ %d\n", ret);
		mutex_unlock(&dev->isp_mutex);
		goto irq_exit;
	}

	mutex_unlock(&dev->isp_mutex);
	isp_k_param->isp_pdev = s_isp_pdev;
	*isp_pipe_dev_handle = (void *)dev;

	return ret;
irq_exit:
	isp_block_buf_free(isp_k_param);
exit:
	vfree(dev);
	pr_err("%s: fail to init dev!\n", __func__);

	return ret;
}

int sprd_isp_dev_deinit(void *isp_pipe_dev_handle)
{
	int ret = 0;
	struct isp_pipe_dev *dev = NULL;
	struct isp_k_block *isp_k_param = NULL;

	if (!isp_pipe_dev_handle) {
		pr_err("%s input handle is NULL\n", __func__);
		return -EINVAL;
	}

	isp_int_deinit();
	dev = (struct isp_pipe_dev *)isp_pipe_dev_handle;
	isp_k_param = &dev->isp_k_param;
	atomic_set(&isp_k_param->lsc_updated, 0);
	mutex_lock(&dev->isp_mutex);
	isp_disable_clk();
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	sprd_cam_domain_disable();
	ret = sprd_cam_pw_off();
#endif
	if (unlikely(ret != 0)) {
		ret = -1;
		pr_info("isp_module_dis:isp_set_clk error.\n");
		goto exit;
	}

	ret = isp_irq_free(s_isp_irq, dev);
	if (ret)
		pr_err("fail to free isp IRQ %d\n", ret);

exit:
	isp_block_buf_free(isp_k_param);
	mutex_unlock(&dev->isp_mutex);
	mutex_destroy(&dev->isp_mutex);
	vfree(dev);

	pr_info("%s: dev deinit OK!\n", __func__);

	return ret;
}

int sprd_isp_unmap_buf(void *isp_pipe_dev_handle)
{
	struct isp_pipe_dev *dev = NULL;
	struct isp_k_block *isp_k_param = NULL;
	struct isp_statis_module *statis_module_info = NULL;

	if (!isp_pipe_dev_handle) {
		pr_err("%s input handle is NULL\n", __func__);
		return -EINVAL;
	}

	dev = (struct isp_pipe_dev *)isp_pipe_dev_handle;
	isp_k_param = &dev->isp_k_param;
	statis_module_info = &dev->statis_module_info;
/*
	if (statis_module_info->img_statis_buf.ion != NULL) {
		sprd_ion_client_put(statis_module_info->img_statis_buf.ion);
		statis_module_info->img_statis_buf.ion = NULL;
	}
*/
	sprd_isp_clr_statis_buf(isp_pipe_dev_handle);

	pfiommu_free_addr_with_id(&isp_k_param->lsc_pfinfo,
					SPRD_IOMMU_EX_CH_READ, LENS_RD_CH_ID);
	memset(&isp_k_param->lsc_pfinfo, 0, sizeof(struct pfiommu_info));
	isp_k_param->lsc_buf_phys_addr = 0;

	pfiommu_free_addr_with_id(&isp_k_param->fetch_pfinfo,
					SPRD_IOMMU_EX_CH_READ, FETCH_RD_CH_ID);
	memset(&isp_k_param->fetch_pfinfo, 0, sizeof(struct pfiommu_info));

	pfiommu_free_addr_with_id(&isp_k_param->store_pfinfo,
					SPRD_IOMMU_EX_CH_WRITE, STORE_WR_CH_ID);
	memset(&isp_k_param->store_pfinfo, 0, sizeof(struct pfiommu_info));

	return 0;
}

static int isp_parse_clk(struct device_node *isp_node)
{

	isp_eb = of_clk_get_by_name(isp_node, "isp_eb");
	if (IS_ERR(isp_eb)) {
		pr_err("failed to get isp_eb\n");
		return PTR_ERR(isp_eb);
	}

	isp_axi_eb = of_clk_get_by_name(isp_node, "isp_axi_eb");
	if (IS_ERR(isp_axi_eb)) {
		pr_err("failed to get isp_axi_eb\n");
		return PTR_ERR(isp_axi_eb);
	}

	isp_clk = of_clk_get_by_name(isp_node, "isp_clk");
	if (IS_ERR(isp_clk)) {
		pr_err("failed to get isp_clk\n");
		return PTR_ERR(isp_clk);
	}

	isp_clk_high_parent = of_clk_get_by_name(isp_node,
		"isp_clk_high_parent");
	if (IS_ERR(isp_clk_high_parent)) {
		pr_err("failed to get isp_clk_high_parent\n");
		return PTR_ERR(isp_clk_high_parent);
	}

	isp_clk_low_parent = of_clk_get_by_name(isp_node,
		"isp_clk_low_parent");
	if (IS_ERR(isp_clk_low_parent)) {
		pr_err("failed to get isp_clk_low_parent\n");
		return PTR_ERR(isp_clk_low_parent);
	}

	isp_axi_clk = of_clk_get_by_name(isp_node, "isp_axi_clk");
	if (IS_ERR(isp_axi_clk)) {
		pr_err("failed to get isp_axi_clk\n");
		return PTR_ERR(isp_axi_clk);
	}

	isp_axi_clk_parent = of_clk_get_by_name(isp_node, "isp_axi_clk_parent");
	if (IS_ERR(isp_axi_clk_parent)) {
		pr_err("failed to get isp_axi_clk_parent\n");
		return PTR_ERR(isp_axi_clk_parent);
	}

	cam_ahb_gpr = syscon_regmap_lookup_by_phandle(isp_node,
				      "sprd,cam-ahb-syscon");
	if (IS_ERR(cam_ahb_gpr))
		return PTR_ERR(cam_ahb_gpr);

	aon_apb_gpr = syscon_regmap_lookup_by_phandle(isp_node,
				"sprd,aon-apb-syscon");
	if (IS_ERR(aon_apb_gpr))
		return PTR_ERR(aon_apb_gpr);

	isp_clk_default = clk_get_parent(isp_clk);
	if (IS_ERR(isp_clk_default)) {
		pr_err("failed to get isp_clk_default\n");
		return PTR_ERR(isp_clk_default);
	}

	isp_axi_clk_default = clk_get_parent(isp_axi_clk);
	if (IS_ERR(isp_axi_clk_default)) {
		pr_err("failed to get isp_axi_clk_default\n");
		return PTR_ERR(isp_axi_clk_default);
	}
	return 0;
}

int sprd_isp_parse_dt(struct device_node *dn)
{
	void __iomem *reg_base;
	struct device_node *isp_node = NULL;

	pr_info("isp dev device node %s, full name %s\n",
		dn->name, dn->full_name);
	isp_node = of_parse_phandle(dn, "sprd,isp", 0);
	if (isp_node == NULL) {
		pr_err("fail to parse the property of sprd,isp\n");
		return -EFAULT;
	}

	pr_info("after isp dev device node %s, full name %s\n",
		isp_node->name, isp_node->full_name);
	s_isp_pdev = of_find_device_by_node(isp_node);
	pr_info("sprd s_isp_pdev name %s\n", s_isp_pdev->name);
	if (of_device_is_compatible(isp_node, "sprd,isp")) {
		reg_base = of_iomap(isp_node, 0);
		if (!reg_base) {
			pr_err("fail to get isp reg_base\n");
			return -ENXIO;
		}

		s_isp_regbase = (unsigned long)reg_base;
		s_isp_irq = irq_of_parse_and_map(isp_node, 0);
		isp_parse_clk(isp_node);
	} else {
		pr_err("fail to match isp device node\n");
		return -EINVAL;
	}

	return 0;
}

void sprd_isp_drv_init(void)
{
	isp_mod_lock = __SPIN_LOCK_UNLOCKED(&isp_mod_lock);
}

