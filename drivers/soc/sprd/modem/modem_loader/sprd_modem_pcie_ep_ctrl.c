/*
 * SPRD external modem control driver in AP side.
 *
 * Copyright (C) 2019 Spreadtrum Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 of
 * the License as published by the Free Software Foundation.
 *
 * This program is used to control external modem in AP side for
 * Spreadtrum SoCs.
 */

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/sipc.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/gpio/consumer.h>

#include "sprd_modem_loader.h"

/*
 * BASE ADDR 0x6401_0000, value 0x03A0, name PCIE_SLP_CFG
 * [2] PCIE_LOAD_DONE RW 1'b0, set/clear, ep rom code will polling this bit,
 * if 1'b1, go on access SRAM. PCIE RC set this bit after load SPL to SRAM"
 * [11:7] AP_CP_HANDSHK RW 5'h0set/clear
 * sw use these bits to sync between AP and CP
 */

#define EP_REMOTR_BASE		0x64010000
#define EP_REMOTR_OFFSET	0x03a0
#define BIT_SET_OFFSET		0x1000
#define BIT_CLR_OFFSET		0x2000
#define EP_REMOTR_MASK		0xf84

#define SPL_DONE_BIT		BIT(2)
#define DDR_READY_BIT		BIT(7)
#define MODEM_DONE_BIT		BIT(8)
#define MINIAP_DONE_BIT		BIT(9)
#define MINIAP_PANIC_BIT	BIT(10)
#define MINIAP_RESERVE_BIT	BIT(11)

#define FLAG_HANDSHK_SHIFT	7
#define FLAG_HANDSHK_MASK	GENMASK(11, 7)


#define REBOOT_MODEM_DELAY	1000
#define MINI_REGION_SIZE	0x1000

static void modem_ep_get_remote_flag(struct modem_device *modem)
{
	u32 flag;
	void __iomem *base;

	base = modem_ram_vmap_nocache(modem->modem_type,
				      EP_REMOTR_BASE,
				      MINI_REGION_SIZE);
	if (base) {
		flag = readl_relaxed(base + EP_REMOTR_OFFSET);
		dev_dbg(modem->p_dev, "ep get flag = 0x%x!\n", flag);
		modem_ram_unmap(modem->modem_type, base);
	}

	modem->remote_flag = 0;
	modem->remote_flag = (flag & FLAG_HANDSHK_MASK) >> FLAG_HANDSHK_SHIFT;

	if (flag & SPL_DONE_BIT)
		modem->remote_flag |= SPL_IMAGE_DONE_FLAG;
}

static void modem_ep_set_remote_flag(struct modem_device *modem)
{
	u32 flag;
	void __iomem *base;

	flag = (modem->remote_flag << FLAG_HANDSHK_SHIFT) & FLAG_HANDSHK_MASK;

	if (modem->remote_flag & SPL_IMAGE_DONE_FLAG)
		flag |= SPL_DONE_BIT;

	dev_dbg(modem->p_dev, "ep set flag = 0x%x!\n", flag);

	base = modem_ram_vmap_nocache(modem->modem_type,
				      EP_REMOTR_BASE,
				      MINI_REGION_SIZE);
	if (base) {
		writel_relaxed(EP_REMOTR_MASK,
			       base + BIT_CLR_OFFSET + EP_REMOTR_OFFSET);
		writel_relaxed(flag,
			       base + BIT_SET_OFFSET + EP_REMOTR_OFFSET);
		modem_ram_unmap(modem->modem_type, base);
	}
}

#ifdef CONFIG_SPRD_EXT_MODEM_POWER_CTRL
static int modem_ep_reboot_ext_modem(struct modem_device *modem, u8 b_reset)
{
	struct gpio_desc *gpio;

	gpio = b_reset ? modem->modem_reset : modem->modem_power;
	gpiod_set_value_cansleep(gpio, 1);
	/* the gpio need to continue pull up for a moments */
	msleep(REBOOT_MODEM_DELAY);
	gpiod_set_value_cansleep(gpio, 0);

	return 0;
}

static int modem_ep_poweroff_ext_modem(struct modem_device *modem)
{
	/* todo */

	return 0;
}
#endif

static const struct ext_modem_operations pcie_ep_modem_ops = {
	.get_remote_flag = modem_ep_get_remote_flag,
	.set_remote_flag = modem_ep_set_remote_flag,
#ifdef CONFIG_SPRD_EXT_MODEM_POWER_CTRL
	.reboot = modem_ep_reboot_ext_modem,
	.poweroff = modem_ep_poweroff_ext_modem
#endif
};

void modem_get_ext_modem_ops(const struct ext_modem_operations **ops)
{
	*ops = &pcie_ep_modem_ops;
}
