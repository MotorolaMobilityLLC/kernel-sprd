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

#include "pam_ipa_core.h"

#define DRV_NAME "sprd-pam-ipa"

u32 pam_buf_index;

struct pam_ipa_cfg_tag *pam_ipa_cfg;

static const struct of_device_id pam_ipa_plat_drv_match[] = {
	{ .compatible = "sprd,pam-ipa", },
	{}
};

static int pam_ipa_alloc_skb_buf(
	struct device *dev,
	struct pam_ipa_cfg_tag *cfg)
{
	u32 i;
	dma_addr_t dma_addr;

	cfg->skb1 = alloc_skb(PAM_AKB_BUF_SIZE *
						  (PAM_FREE_FIFO_SIZE / 4),
						  GFP_KERNEL | GFP_DMA);
	memset(cfg->skb1->data, 0xE7,
		   PAM_AKB_BUF_SIZE *
		   (PAM_FREE_FIFO_SIZE / 4));

	if (!cfg->skb1) {
		pr_err("alloc skb1 fail\n");
		return -1;
	}
	for (i = 0; i < (PAM_FREE_FIFO_SIZE / 4); i++) {
		dma_addr = dma_map_single(dev,
								  skb_put(cfg->skb1, PAM_AKB_BUF_SIZE),
								  PAM_AKB_BUF_SIZE, DMA_FROM_DEVICE);
		cfg->dma_addr_buf[pam_buf_index++] = dma_addr;
	}

	cfg->skb2 = alloc_skb(PAM_AKB_BUF_SIZE *
						  (PAM_FREE_FIFO_SIZE / 4),
						  GFP_KERNEL | GFP_DMA);
	memset(cfg->skb2->data, 0xE7,
		   PAM_AKB_BUF_SIZE *
		   (PAM_FREE_FIFO_SIZE / 4));

	if (!cfg->skb2) {
		consume_skb(cfg->skb1);
		pr_err("alloc skb2 fail\n");
		return -1;
	}
	for (i = 0; i < (PAM_FREE_FIFO_SIZE / 4); i++) {
		dma_addr = dma_map_single(dev,
								  skb_put(cfg->skb2, PAM_AKB_BUF_SIZE),
								  PAM_AKB_BUF_SIZE, DMA_FROM_DEVICE);
		cfg->dma_addr_buf[pam_buf_index++] = dma_addr;
	}

	cfg->skb3 = alloc_skb(PAM_AKB_BUF_SIZE *
						  (PAM_FREE_FIFO_SIZE / 4),
						  GFP_KERNEL | GFP_DMA);
	memset(cfg->skb3->data, 0xE7,
		   PAM_AKB_BUF_SIZE *
		   (PAM_FREE_FIFO_SIZE / 4));

	if (!cfg->skb3) {
		consume_skb(cfg->skb1);
		consume_skb(cfg->skb2);
		pr_err("alloc skb3 fail\n");
		return -1;
	}
	for (i = 0; i < (PAM_FREE_FIFO_SIZE / 4); i++) {
		dma_addr = dma_map_single(dev,
								  skb_put(cfg->skb3, PAM_AKB_BUF_SIZE),
								  PAM_AKB_BUF_SIZE, DMA_FROM_DEVICE);
		cfg->dma_addr_buf[pam_buf_index++] = dma_addr;
	}

	cfg->skb4 = alloc_skb(PAM_AKB_BUF_SIZE *
						  (PAM_FREE_FIFO_SIZE / 4),
						  GFP_KERNEL | GFP_DMA);
	memset(cfg->skb4->data, 0xE7,
		   PAM_AKB_BUF_SIZE *
		   (PAM_FREE_FIFO_SIZE / 4));

	if (!cfg->skb4) {
		consume_skb(cfg->skb1);
		consume_skb(cfg->skb2);
		consume_skb(cfg->skb3);
		pr_err("alloc skb4 fail\n");
		return -1;
	}
	for (i = 0; i < (PAM_FREE_FIFO_SIZE / 4); i++) {
		dma_addr = dma_map_single(dev,
								  skb_put(cfg->skb4, PAM_AKB_BUF_SIZE),
								  PAM_AKB_BUF_SIZE, DMA_FROM_DEVICE);
		cfg->dma_addr_buf[pam_buf_index++] = dma_addr;
	}

	return 0;
}

static int pam_ipa_parse_dts_configuration(
	struct platform_device *pdev,
	struct pam_ipa_cfg_tag *cfg)
{
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

	cfg = kzalloc(sizeof(struct pam_ipa_cfg_tag), GFP_KERNEL);
	if (!cfg) {
		pr_err("[PAM_IPA] alloc cfg mem failed\n");
		return -1;
	}
	pam_ipa_cfg = cfg;

	pam_ipa_parse_dts_configuration(pdev_p, cfg);

	pam_ipa_init_api(&cfg->hal_ops);
	cfg->pcie_offset = cfg->hal_ops.get_ddr_mapping();
	cfg->pcie_rc_base = cfg->hal_ops.get_pcie_rc_base();

	pam_ipa_alloc_skb_buf(&pdev_p->dev, cfg);

	cfg->pam_local_param.send_param.data_ptr =
		cfg->dma_addr_buf;
	cfg->pam_local_param.send_param.data_ptr_cnt =
		PAM_FREE_FIFO_SIZE;
	cfg->pam_local_param.send_param.buf_size =
		PAM_AKB_BUF_SIZE;

	cfg->pam_local_param.id = SIPA_EP_VCP;
	ret = sipa_pam_connect(&cfg->pam_local_param,
						   &cfg->local_cfg);
	if (ret)
		pr_err("[PAM_IPA] local ipa not ready\n");
#if 0
	cfg->pam_remote_param.id = SIPA_EP_REMOTE_PCIE;
	ret = sipa_pam_connect(&cfg->pam_remote_param,
						   &cfg->remote_cfg);
	if (ret)
		pr_err("[PAM_IPA] remote ipa not ready\n");
#endif
	//pam_ipa_init(cfg);

	return 0;
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

