#include <linux/sipa.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-mapping.h>
#include <linux/skbuff.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include "pam_ipa_core.h"
#include "../sipa_delegate/sipa_delegate.h"

#define DRV_NAME "sprd-pam-ipa"

#define PAM_IPA_DDR_MAP_OFFSET_L				0x0
#define PAM_IPA_DDR_MAP_OFFSET_H				0x2

#define PAM_IPA_PCIE_RC_BASE_L					0x0
#define PAM_IPA_PCIE_RC_BASE_H					0x0

struct pam_ipa_cfg_tag *pam_ipa_cfg;

static const struct of_device_id pam_ipa_plat_drv_match[] = {
	{ .compatible = "sprd,pam-ipa", },
	{}
};

static int pam_ipa_alloc_buf(
	struct device *dev,
	struct pam_ipa_cfg_tag *cfg)
{
	cfg->dl_buf = kzalloc(cfg->local_cfg.dl_fifo.fifo_depth *
						  PAM_AKB_BUF_SIZE,
						  GFP_KERNEL | GFP_DMA);
	if (!cfg->dl_buf)
		return -ENOMEM;

	cfg->dl_dma_addr = dma_map_single(dev,
				cfg->dl_buf,
				cfg->local_cfg.dl_fifo.fifo_depth *
				PAM_AKB_BUF_SIZE,
				DMA_BIDIRECTIONAL);
	if (dma_mapping_error(dev, cfg->dl_dma_addr)) {
		dev_err(dev, "[PAM_IPA] dl_buf map fail!\n");
		return -ENOMEM;
	}

	cfg->ul_buf = kzalloc(cfg->local_cfg.ul_fifo.fifo_depth *
						  PAM_AKB_BUF_SIZE,
						  GFP_KERNEL | GFP_DMA);
	if (!cfg->ul_buf)
		return -ENOMEM;

	cfg->ul_dma_addr = dma_map_single(dev,
				cfg->ul_buf,
				cfg->local_cfg.ul_fifo.fifo_depth *
				PAM_AKB_BUF_SIZE,
				DMA_BIDIRECTIONAL);
	if (dma_mapping_error(dev, cfg->ul_dma_addr)) {
		dev_err(dev, "ul_buf map fail!\n");
		return -ENOMEM;
	}

	return 0;
}

static void pam_ipa_free_buf(struct device *dev,
					struct pam_ipa_cfg_tag *cfg)
{
	dma_unmap_single(dev,
			 cfg->dl_dma_addr,
			 cfg->local_cfg.dl_fifo.fifo_depth *
			 PAM_AKB_BUF_SIZE,
			 DMA_BIDIRECTIONAL);
	dma_unmap_single(dev,
			 cfg->ul_dma_addr,
			 cfg->local_cfg.ul_fifo.fifo_depth *
			 PAM_AKB_BUF_SIZE,
			 DMA_BIDIRECTIONAL);
	kfree(cfg->ul_buf);
	kfree(cfg->dl_buf);
}

static int pam_ipa_parse_dts_configuration(
	struct platform_device *pdev,
	struct pam_ipa_cfg_tag *cfg)
{
	int ret;
	u32 reg_info[2];
	struct resource *resource;

	/* get IPA global register base  address */
	resource = platform_get_resource_byname(pdev,
											IORESOURCE_MEM,
											"pam-ipa-base");
	if (!resource) {
		pr_err("%s :get resource failed for glb-base!\n",
			   __func__);
		return -ENODEV;
	}
	cfg->reg_base = devm_ioremap_nocache(&pdev->dev,
										 resource->start,
										 resource_size(resource));
	memcpy(&cfg->pam_ipa_res, resource,
		   sizeof(struct resource));

	/* get enable register informations */
	cfg->enable_regmap = syscon_regmap_lookup_by_name(pdev->dev.of_node,
			     "enable");
	if (IS_ERR(cfg->enable_regmap))
		pr_warn("%s :get enable regmap fail!\n", __func__);

	ret = syscon_get_args_by_name(pdev->dev.of_node,
				      "enable", 2,
				      reg_info);
	if (ret < 0 || ret != 2)
		pr_warn("%s :get enable register info fail!\n", __func__);
	else {
		cfg->enable_reg = reg_info[0];
		cfg->enable_mask = reg_info[1];
	}

	of_property_read_u32(pdev->dev.of_node,
						 "sprd,cp-ul-intr-to-ap",
						 &cfg->pam_local_param.recv_param.intr_to_ap);

	of_property_read_u32(pdev->dev.of_node,
						 "sprd,cp-ul-threshold",
						 &cfg->pam_local_param.recv_param.tx_intr_threshold);

	of_property_read_u32(pdev->dev.of_node,
						 "sprd,cp-ul-timeout",
						 &cfg->pam_local_param.recv_param.tx_intr_delay_us);

	of_property_read_u32(pdev->dev.of_node,
						 "sprd,cp-ul-flowctrl-mode",
						 &cfg->pam_local_param.recv_param.flow_ctrl_cfg);

	of_property_read_u32(pdev->dev.of_node,
						 "sprd,cp-ul-enter-flowctrl-watermark",
						 &cfg->pam_local_param.recv_param.tx_enter_flowctrl_watermark);

	of_property_read_u32(pdev->dev.of_node,
						 "sprd,cp-ul-exit-flowctrl-watermark",
						 &cfg->pam_local_param.recv_param.tx_leave_flowctrl_watermark);

	of_property_read_u32(pdev->dev.of_node,
						 "sprd,cp-dl-intr-to-ap",
						 &cfg->pam_local_param.send_param.intr_to_ap);

	of_property_read_u32(pdev->dev.of_node,
						 "sprd,cp-dl-threshold",
						 &cfg->pam_local_param.send_param.tx_intr_threshold);

	of_property_read_u32(pdev->dev.of_node,
						 "sprd,cp-dl-timeout",
						 &cfg->pam_local_param.send_param.tx_intr_delay_us);

	of_property_read_u32(pdev->dev.of_node,
						 "sprd,cp-dl-flowctrl-mode",
						 &cfg->pam_local_param.send_param.flow_ctrl_cfg);

	of_property_read_u32(pdev->dev.of_node,
						 "sprd,cp-dl-enter-flowctrl-watermark",
						 &cfg->pam_local_param.send_param.tx_enter_flowctrl_watermark);

	of_property_read_u32(pdev->dev.of_node,
						 "sprd,cp-dl-exit-flowctrl-watermark",
						 &cfg->pam_local_param.send_param.tx_leave_flowctrl_watermark);

	return 0;
}

static int pam_ipa_plat_drv_probe(struct platform_device *pdev_p)
{
	int ret;
	struct pam_ipa_cfg_tag *cfg;
	struct sipa_to_pam_info local_cfg;
	struct sipa_to_pam_info remote_cfg;

	ret = sipa_get_ep_info(SIPA_EP_VCP, &local_cfg);
	if (ret) {
		dev_err(&pdev_p->dev, "[PAM_IPA] local ipa not ready\n");
		return ret;
	}

	ret = modem_sipa_connect(&remote_cfg);
	if (ret) {
		dev_err(&pdev_p->dev, "[PAM_IPA] remote ipa not ready\n");
		return ret;
	}

	cfg = devm_kzalloc(&pdev_p->dev, sizeof(*cfg),
					   GFP_KERNEL);
	if (!cfg) {
		return -ENOMEM;
	}

	pam_ipa_cfg = cfg;

	memcpy(&cfg->local_cfg, &local_cfg, sizeof(local_cfg));
	memcpy(&cfg->remote_cfg, &remote_cfg, sizeof(remote_cfg));

	pam_ipa_parse_dts_configuration(pdev_p, cfg);

	pam_ipa_init_api(&cfg->hal_ops);
	cfg->pcie_offset = PAM_IPA_STI_64BIT(
				   PAM_IPA_DDR_MAP_OFFSET_L,
				   PAM_IPA_DDR_MAP_OFFSET_H);
	cfg->pcie_rc_base = PAM_IPA_STI_64BIT(
				    PAM_IPA_PCIE_RC_BASE_L,
				    PAM_IPA_PCIE_RC_BASE_H);

	ret = pam_ipa_alloc_buf(&pdev_p->dev, cfg);
	if (ret)
		goto err_alloc;

	cfg->pam_local_param.send_param.data_ptr =
		cfg->dl_dma_addr;
	cfg->pam_local_param.send_param.data_ptr_cnt =
		PAM_FREE_FIFO_SIZE;
	cfg->pam_local_param.send_param.buf_size =
		PAM_AKB_BUF_SIZE;

	cfg->pam_local_param.recv_param.data_ptr =
		cfg->ul_dma_addr;
	cfg->pam_local_param.recv_param.data_ptr_cnt =
		PAM_FREE_FIFO_SIZE;
	cfg->pam_local_param.recv_param.buf_size =
		PAM_AKB_BUF_SIZE;

	cfg->pam_local_param.id = SIPA_EP_VCP;
	ret = sipa_pam_connect(&cfg->pam_local_param);
	if (ret) {
		dev_err(&pdev_p->dev, "[PAM_IPA] local ipa connect failed\n");
		goto err_alloc;
	}

	ret = pam_ipa_init(cfg);
	if (ret) {
		dev_err(&pdev_p->dev, "[PAM_IPA] init failed\n");
		goto err_alloc;
	}

	return 0;

err_alloc:
	pam_ipa_free_buf(&pdev_p->dev, cfg);
	return ret;
}

static int pam_ipa_ap_suspend(struct device *dev)
{
	return 0;
}

static int pam_ipa_ap_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops pam_ipa_pm_ops = {
	.suspend_noirq = pam_ipa_ap_suspend,
	.resume_noirq = pam_ipa_ap_resume,
};

static struct platform_driver pam_ipa_plat_drv = {
	.probe = pam_ipa_plat_drv_probe,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &pam_ipa_pm_ops,
		.of_match_table = pam_ipa_plat_drv_match,
	},
};

static int __init pam_ipa_module_init(void)
{
	pr_debug("PAM-IPA module init\n");

	/* Register as a platform device driver */
	return platform_driver_register(&pam_ipa_plat_drv);
}

static void __exit pam_ipa_module_exit(void)
{
	pr_debug("PAM-IPA module exit\n");

	platform_driver_unregister(&pam_ipa_plat_drv);
}

module_init(pam_ipa_module_init);
module_exit(pam_ipa_module_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum PAM IPA HW device driver");

