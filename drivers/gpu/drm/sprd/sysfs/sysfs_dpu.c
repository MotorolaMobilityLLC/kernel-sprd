// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>

#include "disp_lib.h"
#include "sprd_dpu.h"
#include "sprd_dsi_panel.h"
#include "sysfs_display.h"

struct dpu_sysfs {
	uint32_t bg_color;
};

static struct dpu_sysfs *sysfs;

static ssize_t run_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", !dpu->ctx.stopped);

	return ret;
}

static ssize_t run_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	int enable;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 10, &enable);
	if (ret) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}

	if (enable)
		sprd_dpu_run(dpu);
	else
		sprd_dpu_stop(dpu);

	return count;
}
static DEVICE_ATTR_RW(run);

static ssize_t refresh_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct sprd_crtc *crtc = dpu->crtc;
	struct sprd_crtc_context *ctx = &dpu->ctx;

	down(&ctx->lock);

	pr_info("[drm] %s()\n", __func__);

	if (!ctx->enabled) {
		pr_err("dpu is powered off\n");
		up(&ctx->lock);
		return -1;
	}

	dpu->core->flip(ctx, crtc->layers, crtc->pending_planes);

	up(&ctx->lock);

	return count;
}
static DEVICE_ATTR_WO(refresh);

static ssize_t bg_color_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%x\n", sysfs->bg_color);

	return ret;
}

static ssize_t bg_color_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct sprd_crtc_context *ctx = &dpu->ctx;

	if (!dpu->core->bg_color)
		return -EIO;

	pr_info("[drm] %s()\n", __func__);

	ret = kstrtou32(buf, 16, &sysfs->bg_color);
	if (ret) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}

	down(&ctx->lock);

	if (!ctx->enabled) {
		pr_err("dpu is not initialized\n");
		up(&ctx->lock);
		return -EINVAL;
	}

	dpu->core->bg_color(ctx, sysfs->bg_color);

	up(&ctx->lock);

	return count;
}
static DEVICE_ATTR_RW(bg_color);

static ssize_t actual_fps_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct sprd_crtc_context *ctx = &dpu->ctx;
	struct videomode vm = ctx->vm;
	uint32_t act_fps_int, act_fps_frac;
	uint32_t total_pixels;
	int ret;

	total_pixels = (vm.hsync_len + vm.hback_porch +
			vm.hfront_porch + vm.hactive) *
			(vm.vsync_len + vm.vback_porch +
			vm.vfront_porch + vm.vactive);

	act_fps_int = vm.pixelclock / total_pixels;
	act_fps_frac = vm.pixelclock % total_pixels;
	act_fps_frac = act_fps_frac * 100 / total_pixels;

	ret = snprintf(buf, PAGE_SIZE, "%u.%u\n", act_fps_int, act_fps_frac);

	return ret;
}
static DEVICE_ATTR_RO(actual_fps);

static ssize_t regs_offset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct sprd_crtc_context *ctx = &dpu->ctx;

	ret = snprintf(buf, PAGE_SIZE,
			"dpu reg offset: %x\n"
			"dpu reg length: %x\n",
			ctx->base_offset[0],
			ctx->base_offset[1]);

	return ret;
}

static ssize_t regs_offset_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct sprd_crtc_context *ctx = &dpu->ctx;

	str_to_u32_array(buf, 16, ctx->base_offset);

	return count;
}
static DEVICE_ATTR_RW(regs_offset);

static ssize_t wr_regs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	unsigned int i;
	unsigned int reg;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct sprd_crtc_context *ctx = &dpu->ctx;
	unsigned int offset = ctx->base_offset[0];
	unsigned int length = ctx->base_offset[1];

	for (i = 0; i < length; i++) {
		reg = readl((void __iomem *)(ctx->base + offset));
		ret += snprintf(buf + ret, PAGE_SIZE, "%x ", reg);
	}

	return ret;
}

static ssize_t wr_regs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct sprd_crtc_context *ctx = &dpu->ctx;

	uint32_t temp = ctx->base_offset[0];
	uint32_t length = ctx->base_offset[1];
	uint32_t *value;
	uint32_t i;

	value = kzalloc(length * 4, GFP_KERNEL);
	if (!value)
		return -ENOMEM;

	str_to_u32_array(buf, 16, value);

	for (i = 0; i < length; i++) {
		writel(value[i], (void __iomem *)(ctx->base + temp));
		temp += 0x04;
	}

	kfree(value);

	return count;
}
static DEVICE_ATTR_RW(wr_regs);

static ssize_t dpu_version_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct sprd_crtc_context *ctx = &dpu->ctx;

	ret = snprintf(buf, PAGE_SIZE, "%s\n", ctx->version);

	return ret;
}
static DEVICE_ATTR_RO(dpu_version);

static ssize_t irq_register_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	uint32_t value;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct sprd_crtc_context *ctx = &dpu->ctx;
	int ret;

	if (kstrtou32(buf, 10, &value)) {
		pr_err("Invalid input for irq_register\n");
		return -EINVAL;
	}

	if (value > 0 && ctx->irq) {
		down(&ctx->lock);
		if (!ctx->enabled) {
			pr_err("dpu is not initialized!\n");
			up(&ctx->lock);
			return -EINVAL;
		}

		if (dpu->core->enable_vsync)
			dpu->core->enable_vsync(ctx);

		ret = devm_request_irq(&dpu->dev, ctx->irq, ctx->dpu_isr,
			0, "DISPC", dpu);
		if (ret) {
			up(&ctx->lock);
			pr_err("error: dpu request irq failed\n");
			return ret;
		}

		/*
		 *We request dpu isr on sprd crtc driver and set the IRQ_NOAUTOEN flag,
		 *so if not clear this flag, need to call "enable_irq" enable it.
		 */
		enable_irq(ctx->irq);
		up(&ctx->lock);
	}

	return count;
}
static DEVICE_ATTR_WO(irq_register);

static ssize_t irq_unregister_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	uint32_t value;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct sprd_crtc_context *ctx = &dpu->ctx;

	if (kstrtou32(buf, 10, &value)) {
		pr_err("Invalid input for irq_unregister\n");
		return -EINVAL;
	}

	if (value > 0 && ctx->irq) {
		down(&ctx->lock);
		if (!ctx->enabled) {
			pr_err("dpu is not initialized!\n");
			up(&ctx->lock);
			return -EINVAL;
		}

		if (dpu->core->disable_vsync)
			dpu->core->disable_vsync(ctx);

		/*
		 *We request dpu isr on sprd crtc driver and set the IRQ_NOAUTOEN flag,
		 *so if not clear this flag, need to call "disable_irq" disable it.
		 */
		disable_irq(ctx->irq);
		devm_free_irq(&dpu->dev, ctx->irq, dpu);
		up(&ctx->lock);
	}

	return count;
}

static DEVICE_ATTR_WO(irq_unregister);

static struct attribute *dpu_attrs[] = {
	&dev_attr_run.attr,
	&dev_attr_refresh.attr,
	&dev_attr_bg_color.attr,
	&dev_attr_actual_fps.attr,
	&dev_attr_regs_offset.attr,
	&dev_attr_wr_regs.attr,
	&dev_attr_dpu_version.attr,
	&dev_attr_irq_register.attr,
	&dev_attr_irq_unregister.attr,
	NULL,
};
static const struct attribute_group dpu_group = {
	.attrs = dpu_attrs,
};

int sprd_dpu_sysfs_init(struct device *dev)
{
	int rc;

	sysfs = kzalloc(sizeof(*sysfs), GFP_KERNEL);
	if (!sysfs) {
		pr_err("alloc dpu sysfs failed\n");
		return -ENOMEM;
	}
	rc = sysfs_create_group(&(dev->kobj), &dpu_group);
	if (rc)
		pr_err("create dpu attr node failed, rc=%d\n", rc);

	return rc;
}
EXPORT_SYMBOL(sprd_dpu_sysfs_init);

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("Add dpu attribute nodes for userspace");
MODULE_LICENSE("GPL v2");
