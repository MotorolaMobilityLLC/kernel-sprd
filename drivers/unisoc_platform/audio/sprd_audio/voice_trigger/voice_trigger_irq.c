#define pr_fmt(fmt) "[voice-trigger-irq] "fmt

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_runtime.h>

#include "agdsp_access.h"
#define WAKEUP_TIME_MS 1000

struct voice_trigger_irq_t {
	int irq;
	struct device *dev;
	struct wakeup_source *wake_lock;
	struct regmap *agcp_glb;
	u32 audcp_glb_eic_int_reg;
	u32 audcp_glb_eic_int_mask;
};

static irqreturn_t sprd_eic_vts_handler(int irq, void *dev)
{
	struct voice_trigger_irq_t *voice_trigger_irq = dev;
	int ret;
	int reg_val;

	pr_info("%s enter\n", __func__);

	if (!voice_trigger_irq || !voice_trigger_irq->agcp_glb) {
		pr_err("%s voice_trigger_irq or agcp_glb NULL\n", __func__);
		return IRQ_HANDLED;
	}
	if (agdsp_access_enable()) {
		dev_err(voice_trigger_irq->dev, "%s agdsp_access_enable error\n",
			__func__);
		return IRQ_HANDLED;
	}
	ret = regmap_update_bits(voice_trigger_irq->agcp_glb,
			voice_trigger_irq->audcp_glb_eic_int_reg,
			voice_trigger_irq->audcp_glb_eic_int_mask,
			0);
	if (ret != 0) {
		pr_err("%s, regmap_update_bits AUDACCESS_APB_AGCP_CTRL error!\n",
		__func__);
		goto exit;
	}
	if (voice_trigger_irq->wake_lock)
		pm_wakeup_ws_event(voice_trigger_irq->wake_lock, WAKEUP_TIME_MS, false);

	ret = regmap_read(voice_trigger_irq->agcp_glb,
			voice_trigger_irq->audcp_glb_eic_int_reg, &reg_val);
	if (ret != 0)
		pr_err("%s, regmap_update_bits audcp_glb_eic_int_reg error!\n",
		__func__);

	pr_info("%s:reg_val is %d\n", __func__, reg_val);

exit:
	agdsp_access_disable();

	return IRQ_HANDLED;
}

static int voice_trigger_irq_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct voice_trigger_irq_t *voice_trigger_irq;
	unsigned int args[2] = {0};
	struct regmap *regmap;
	struct device *dev = &pdev->dev;
	int irq, ret = -1;

	voice_trigger_irq = devm_kzalloc(dev, sizeof(struct voice_trigger_irq_t), GFP_KERNEL);
	if (voice_trigger_irq == NULL) {
		pr_err("%s:Failed to allocate\n", __func__);
		return -ENOMEM;
	}

	regmap = syscon_regmap_lookup_by_phandle_args(node, "audcp_glb_eic_int",
		2, args);
	if (IS_ERR(regmap)) {
		pr_info("audcp_glb_eic_int not exist.\n");
	} else {
		voice_trigger_irq->audcp_glb_eic_int_reg = args[0];
		voice_trigger_irq->audcp_glb_eic_int_mask = args[1];
		pr_info("audcp_glb_eic_int:reg:%x,mask:%x\n",
			args[0], args[1]);
	}

	regmap = syscon_regmap_lookup_by_phandle(node, "sprd,syscon-agcp-glb");
	if (IS_ERR(regmap)) {
		pr_warn("%s,audcp aon apb syscon not exist  %ld\n",
			__func__, PTR_ERR(regmap));
		voice_trigger_irq->agcp_glb = NULL;
	} else {
		voice_trigger_irq->agcp_glb = regmap;
	}

	voice_trigger_irq->dev = dev;

	platform_set_drvdata(pdev, voice_trigger_irq);
	irq = platform_get_irq(pdev, 0);
	voice_trigger_irq->irq = irq;
	pr_info("vts:irq:%d\n", irq);
	if (irq < 0) {
		dev_warn(&pdev->dev,
			"no irq resource: %d\n", irq);
	} else {
		voice_trigger_irq->wake_lock =
			wakeup_source_register(&pdev->dev, "vts_wake");
		ret = devm_request_threaded_irq(
			&pdev->dev, irq, NULL,
			sprd_eic_vts_handler,
			IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND | IRQF_ONESHOT,
			"eic_vts", voice_trigger_irq);
		if (ret < 0) {
			pr_err("failed to request IRQ_%d\n", irq);
			if (voice_trigger_irq->wake_lock)
				wakeup_source_unregister(voice_trigger_irq->wake_lock);
		}
	}

	return ret;
}

static int voice_trigger_irq_remove(struct platform_device *pdev)
{
	struct voice_trigger_irq_t *voice_trigger_irq = platform_get_drvdata(pdev);

	devm_kfree(&pdev->dev, voice_trigger_irq);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static const struct of_device_id voice_trigger_irq_of_match[] = {
	{ .compatible = "sprd,voice_trigger_irq" },
	{ },
};

static struct platform_driver voice_trigger_irq_driver = {
	.probe = voice_trigger_irq_probe,
	.remove = voice_trigger_irq_remove,
	.driver = {
		.name   = "voice-triggrt-irq",
		.of_match_table = voice_trigger_irq_of_match,
	},
};

module_platform_driver(voice_trigger_irq_driver);

MODULE_DESCRIPTION("UNISOC Voice Trigger interrupt driver");
MODULE_AUTHOR("Jinbo Yang <jinbo.yang@unisoc.com>");
MODULE_LICENSE("GPL v2");
