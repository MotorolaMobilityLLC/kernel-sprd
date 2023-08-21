// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for Spreadtrum SC27XX USB Type-C
 *
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/usb/typec.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/extcon.h>
#include <linux/kernel.h>
#include <linux/random.h>
#include <linux/nvmem-consumer.h>
#include <linux/slab.h>
#include <linux/extcon-provider.h>
#include <linux/usb/sprd_typec.h>
#include <linux/usb/sprd_tcpm.h>
#include <linux/usb/sprd_commonphy.h>

/* registers definitions for controller REGS_TYPEC */
#define SC27XX_EN			0x00
#define SC27XX_MODE			0x04
#define SC27XX_INT_EN			0x0c
#define SC27XX_INT_CLR			0x10
#define SC27XX_INT_RAW			0x14
#define SC27XX_INT_MASK			0x18
#define SC27XX_STATUS			0x1c
#define SC27XX_TCCDE_CNT		0x20
#define SC27XX_RTRIM			0x3c
#define SC27XX_SW_CFG			0x54

/* SC2730_DBG1 */
#define SC2730_CC_MASK			GENMASK(7, 0)
#define SC2730_CC_1_DETECT		BIT(0)
#define SC2730_CC_2_DETECT		BIT(4)

/* SC2721_STATUS */
#define SC2721_CC_MASK			GENMASK(6, 0)
#define SC2721_CC_1_DETECT		BIT(5)
#define SC2721_CC_2_DETECT		BIT(7)

/* SC27XX_TYPEC_EN */
#define SC27XX_TYPEC_USB20_ONLY		BIT(4)

/* SC27XX_TYPEC MODE */
#define SC27XX_MODE_SNK			0
#define SC27XX_MODE_SRC			1
#define SC27XX_MODE_DRP			2
#define SC27XX_MODE_MASK		3

/* SC27XX_INT_EN */
#define SC27XX_ATTACH_INT_EN		BIT(0)
#define SC27XX_DETACH_INT_EN		BIT(1)

/* SC27XX_INT_CLR */
#define SC27XX_ATTACH_INT_CLR		BIT(0)
#define SC27XX_DETACH_INT_CLR		BIT(1)

/* SC27XX_INT_MASK */
#define SC27XX_ATTACH_INT		BIT(0)
#define SC27XX_DETACH_INT		BIT(1)

#define SC27XX_STATE_MASK		GENMASK(4, 0)
#define SC27XX_EVENT_MASK		GENMASK(9, 0)

#define SC27XX_DE_ATTACH_MASK		GENMASK(1, 0)

#define SC2730_EFUSE_CC1_SHIFT		5
#define SC2730_EFUSE_CC2_SHIFT		0
#define SC2721_EFUSE_CC1_SHIFT		11
#define SC2721_EFUSE_CC2_SHIFT		6
#define UMP9620_EFUSE_CC1_SHIFT		1
#define UMP9620_EFUSE_CC2_SHIFT		11

#define SC27XX_CC1_MASK(n)		GENMASK((n) + 4, (n))
#define SC27XX_CC2_MASK(n)		GENMASK((n) + 4, (n))
#define SC27XX_CC_SHIFT(n)		(n)

/* sc2721 registers definitions for controller REGS_TYPEC */
#define SC2721_EN			0x00
#define SC2721_CLR			0x04
#define SC2721_MODE			0x08

/* SC2721_INT_EN */
#define SC2721_ATTACH_INT_EN		BIT(5)
#define SC2721_DETACH_INT_EN		BIT(6)

#define SC2721_STATE_MASK		GENMASK(3, 0)
#define SC2721_EVENT_MASK		GENMASK(6, 0)

/* modify sc2730 tcc debunce */
#define SC27XX_TCC_DEBOUNCE_CNT		0xc7f

/* modify sc27xx tdrp */
#define SC2730_TDRP_CNT		0x48
#define SC27XX_MIN_TDRP_CNT		0x63f
#define SC27XX_MIN_TDRP_MS		50
#define SC27XX_TDRP_RANGE		0x640
#define SC27XX_TDRP_TIMER		32

/* sc2730 registers definitions for controller REGS_TYPEC */
#define SC2730_TYPEC_PD_CFG		0x08
/* SC2730_TYPEC_PD_CFG */
#define SC27XX_VCONN_LDO_EN		BIT(13)
#define SC27XX_VCONN_LDO_RDY		BIT(12)

/* pmic name string */
#define SC2721				0x01
#define SC2730				0x02
#define UMP9620				0x03

/* CC DETECT */
#define SC2730_CC1_CHECK		BIT(5)
#define SC2721_CC1_CHECK		BIT(4)

/*CC RP RD*/
#define SC27XX_CC1_SW_SWITCH(x)		(((x) << 12) & GENMASK(13, 12))
#define SC27XX_CC1_SW_SWITCH_MASK	GENMASK(13, 12)
#define SC27XX_CC2_SW_SWITCH(x)		(((x) << 14) & GENMASK(15, 14))
#define SC27XX_CC2_SW_SWITCH_MASK	GENMASK(15, 14)

/*CC RP LEVEL*/
#define SC27XX_TYPEC_RP_LEVEL(x)	(((x) << 2) & GENMASK(3, 2))

enum typec_rp_level {
	RP_DISABLED = 0,
	RP_80UA,
	RP_180UA,
	RP_330UA,
};

enum typec_rp_rd_state {
	SC27XX_TYPEC_HW = 0,
	SC27XX_TYPEC_NC,
	SC27XX_TYPEC_RD,
	SC27XX_TYPEC_RP,
};

static const char *const typec_rp_rd_value[] = {
	[SC27XX_TYPEC_HW] = "hw",
	[SC27XX_TYPEC_NC] = "not connect",
	[SC27XX_TYPEC_RD] = "rd",
	[SC27XX_TYPEC_RP] = "rp",
};

static const char *const typec_rp_level_value[] = {
	[RP_DISABLED] = "disabled",
	[RP_80UA] = "80UA",
	[RP_180UA] = "180UA",
	[RP_330UA] = "330UA",
};

static const char *typec_rp_rd_string(enum typec_rp_rd_state state)
{
	if (state >= ARRAY_SIZE(typec_rp_rd_value))
		return "unknown";

	return typec_rp_rd_value[state];
}

static const char *typec_rp_level_string(enum typec_rp_level state)
{
	if (state >= ARRAY_SIZE(typec_rp_level_value))
		return "unknown";

	return typec_rp_level_value[state];
}

enum sc27xx_typec_connection_state {
	SC27XX_DETACHED_SNK,
	SC27XX_ATTACHWAIT_SNK,
	SC27XX_ATTACHED_SNK,
	SC27XX_DETACHED_SRC,
	SC27XX_ATTACHWAIT_SRC,
	SC27XX_ATTACHED_SRC,
	SC27XX_POWERED_CABLE,
	SC27XX_AUDIO_CABLE,
	SC27XX_DEBUG_CABLE,
	SC27XX_TOGGLE_SLEEP,
	SC27XX_ERR_RECOV,
	SC27XX_DISABLED,
	SC27XX_TRY_SNK,
	SC27XX_TRY_WAIT_SRC,
	SC27XX_TRY_SRC,
	SC27XX_TRY_WAIT_SNK,
	SC27XX_UNSUPOORT_ACC,
	SC27XX_ORIENTED_DEBUG,
};

struct sprd_typec_variant_data {
	u8 pmic_name;
	u32 efuse_cc1_shift;
	u32 efuse_cc2_shift;
	u32 int_en;
	u32 int_clr;
	u32 mode;
	u32 attach_en;
	u32 detach_en;
	u32 state_mask;
	u32 event_mask;
};

static const struct sprd_typec_variant_data sc2730_data = {
	.pmic_name = SC2730,
	.efuse_cc1_shift = SC2730_EFUSE_CC1_SHIFT,
	.efuse_cc2_shift = SC2730_EFUSE_CC2_SHIFT,
	.int_en = SC27XX_INT_EN,
	.int_clr = SC27XX_INT_CLR,
	.mode = SC27XX_MODE,
	.attach_en = SC27XX_ATTACH_INT_EN,
	.detach_en = SC27XX_DETACH_INT_EN,
	.state_mask = SC27XX_STATE_MASK,
	.event_mask = SC27XX_EVENT_MASK,
};

static const struct sprd_typec_variant_data sc2721_data = {
	.pmic_name = SC2721,
	.efuse_cc1_shift = SC2721_EFUSE_CC1_SHIFT,
	.efuse_cc2_shift = SC2721_EFUSE_CC2_SHIFT,
	.int_en = SC2721_EN,
	.int_clr = SC2721_CLR,
	.mode = SC2721_MODE,
	.attach_en = SC2721_ATTACH_INT_EN,
	.detach_en = SC2721_DETACH_INT_EN,
	.state_mask = SC2721_STATE_MASK,
	.event_mask = SC2721_EVENT_MASK,
};

static const struct sprd_typec_variant_data ump9620_data = {
	.pmic_name = UMP9620,
	.efuse_cc1_shift = UMP9620_EFUSE_CC1_SHIFT,
	.efuse_cc2_shift = UMP9620_EFUSE_CC2_SHIFT,
	.int_en = SC27XX_INT_EN,
	.int_clr = SC27XX_INT_CLR,
	.mode = SC27XX_MODE,
	.attach_en = SC27XX_ATTACH_INT_EN,
	.detach_en = SC27XX_DETACH_INT_EN,
	.state_mask = SC27XX_STATE_MASK,
	.event_mask = SC27XX_EVENT_MASK,
};

struct sc27xx_typec {
	struct device *dev;
	struct regmap *regmap;
	u32 base;
	int irq;
	int mode;
	struct extcon_dev *edev;
	struct extcon_dev *vbus_dev;
	struct notifier_block vbus_nb;

	bool vbus_connect_value;
	bool vbus_only_connect;
	bool vbus_events;
	bool usb20_only;
	bool partner_connected;
	bool soft_typec_out;

	u8 dr_mode;
	u8 pr_mode;
	u8 pd_swap_evt;
	spinlock_t lock;
	spinlock_t vbus_lock;
	enum sc27xx_typec_connection_state state;
	enum sc27xx_typec_connection_state pre_state;
	enum sprd_typec_cc_polarity cc_polarity;
	struct typec_port *port;
	struct typec_partner *partner;
	struct typec_capability typec_cap;
	const struct sprd_typec_variant_data *var_data;
	/* when pr swap, clear invalid interrupt */
	struct delayed_work typec_int_clr_work;
	/* delayed work for handling dr swap */
	struct delayed_work drswap_work;
	/* delayed work for handling vbus_only */
	struct delayed_work vbus_only_work;
	bool use_pdhub_c2c;
};
#if IS_ENABLED(CONFIG_SPRD_TYPEC_TCPM)
struct sprd_typec_device_ops sc27xx_typec_ops;
#endif
struct sc27xx_typec *typec_sc;
/* during dr_swap, begin --> 1  end --> 0  */
static atomic_t pd_dr_swap_event;
/* interrupt, attach --> 1  detach --> 0  */
static atomic_t typec_attach;

static int sc27xx_typec_set_rp_rd(enum sprd_typec_cc_status cc);
static int sc27xx_typec_set_rp_level(enum sprd_typec_cc_status cc);
/* set pd_dr_swap_event */
void sc27xx_set_dr_swap_executing(int event)
{
	atomic_set(&pd_dr_swap_event, event);
}

/* get pd_dr_swap_event */
int sc27xx_get_dr_swap_executing(void)
{
	return atomic_read(&pd_dr_swap_event);
}
EXPORT_SYMBOL_GPL(sc27xx_get_dr_swap_executing);

/* set typec_attach */
void sc27xx_set_current_status_detach_or_attach(int event)
{
	atomic_set(&typec_attach, event);
}

/* get typec_attach */
int sc27xx_get_current_status_detach_or_attach(void)
{
	return atomic_read(&typec_attach);
}
EXPORT_SYMBOL_GPL(sc27xx_get_current_status_detach_or_attach);

static const char * const sprd_typec_cc_polarity_roles[] = {
	[SPRD_TYPEC_POLARITY_CC1] = "cc_1",
	[SPRD_TYPEC_POLARITY_CC2] = "cc_2",
};

static void typec_set_cc_polarity_role(struct sc27xx_typec *sc,
				enum sprd_typec_cc_polarity polarity)
{
	if (sc->cc_polarity == polarity)
		return;

	sc->cc_polarity = polarity;
	sysfs_notify(&sc->dev->kobj, NULL, "typec_cc_polarity_role");
	kobject_uevent(&sc->dev->kobj, KOBJ_CHANGE);
}

static int sc27xx_typec_set_cc_polarity_role(struct sc27xx_typec *sc)
{
	enum sprd_typec_cc_polarity cc_polarity;
	u32 val;
	int ret;

	ret = regmap_read(sc->regmap, sc->base + SC27XX_STATUS, &val);
	if (ret < 0) {
		dev_err(sc->dev, "failed to read STATUS register.\n");
		return ret;
	}

	if (sc->var_data->pmic_name == SC2721)
		val &= SC2721_CC1_CHECK;
	else
		val &= SC2730_CC1_CHECK;

	if (val)
		cc_polarity = SPRD_TYPEC_POLARITY_CC1;
	else
		cc_polarity = SPRD_TYPEC_POLARITY_CC2;

	typec_set_cc_polarity_role(sc, cc_polarity);

	return ret;
}

static int sc27xx_connect_set_status_use_pdhubc2c(struct sc27xx_typec *sc, u32 status)
{

	spin_lock(&sc->lock);
	if (sc->partner_connected) {
		spin_unlock(&sc->lock);
		dev_info(sc->dev, "typec has already connected!\n");
		return 0;
	}

	switch (sc->state) {
	case SC27XX_ATTACHED_SNK:
	case SC27XX_DEBUG_CABLE:
		sc->pre_state = SC27XX_ATTACHED_SNK;
		sc->pr_mode = EXTCON_SINK;
		sc->dr_mode = EXTCON_USB;
		spin_unlock(&sc->lock);

		sc27xx_set_current_status_detach_or_attach(1);
		extcon_set_state_sync(sc->edev, EXTCON_SINK, true);
		extcon_set_state_sync(sc->edev, EXTCON_USB, true);
		dev_info(sc->dev, "%s sink connect !\n", __func__);
		break;
	case SC27XX_ATTACHED_SRC:
		sc->pre_state = SC27XX_ATTACHED_SRC;
		sc->pr_mode = EXTCON_SOURCE;
		sc->dr_mode = EXTCON_USB_HOST;
		spin_unlock(&sc->lock);

		sc27xx_set_current_status_detach_or_attach(1);
		extcon_set_state_sync(sc->edev, EXTCON_SOURCE, true);
		extcon_set_state_sync(sc->edev, EXTCON_USB_HOST, true);
		dev_info(sc->dev, "%s source connect!\n", __func__);
		break;
	case SC27XX_AUDIO_CABLE:
		sc->pre_state = SC27XX_AUDIO_CABLE;
		spin_unlock(&sc->lock);

		extcon_set_state_sync(sc->edev, EXTCON_JACK_HEADPHONE, true);
		break;
	default:
		spin_unlock(&sc->lock);
		/* The pmic may report both attach and detach states, which
		 * is abnormal phenomenon.
		 */
		dev_info(sc->dev, "%s workaround error status: %d state",
				__func__, sc->state);
		return 0;
	}

	spin_lock(&sc->lock);
	sc->partner_connected = true;
	spin_unlock(&sc->lock);

	sc27xx_typec_set_cc_polarity_role(sc);

	return 0;
}

static int sc27xx_connect_set_status_no_pdhubc2c(struct sc27xx_typec *sc, u32 status)
{
	enum typec_data_role data_role = TYPEC_DEVICE;
	enum typec_role power_role = TYPEC_SOURCE;
	enum typec_role vconn_role = TYPEC_SOURCE;
	struct typec_partner_desc desc;

	if (sc->partner)
		return 0;

	switch (sc->state) {
	case SC27XX_ATTACHED_SNK:
	case SC27XX_DEBUG_CABLE:
		power_role = TYPEC_SINK;
		data_role = TYPEC_DEVICE;
		vconn_role = TYPEC_SINK;
		break;
	case SC27XX_ATTACHED_SRC:
	case SC27XX_AUDIO_CABLE:
		power_role = TYPEC_SOURCE;
		data_role = TYPEC_HOST;
		vconn_role = TYPEC_SOURCE;
		break;
	default:
		/* The pmic may report both attach and detach states, which
		 * is abnormal phenomenon.
		 */
		dev_info(sc->dev, "workaround error status: %d state", sc->state);
		return 0;
	}

	desc.usb_pd = 0;
	desc.identity = NULL;
	if (sc->state == SC27XX_AUDIO_CABLE)
		desc.accessory = TYPEC_ACCESSORY_AUDIO;
	else
		desc.accessory = TYPEC_ACCESSORY_NONE;

	sc->partner = typec_register_partner(sc->port, &desc);
	if (!sc->partner)
		return -ENODEV;

	if (sc->state != SC27XX_AUDIO_CABLE) {
		typec_set_pwr_opmode(sc->port, TYPEC_PWR_MODE_USB);
		typec_set_pwr_role(sc->port, power_role);
		typec_set_data_role(sc->port, data_role);
		typec_set_vconn_role(sc->port, vconn_role);
	}

	switch (sc->state) {
	case SC27XX_ATTACHED_SNK:
	case SC27XX_DEBUG_CABLE:
		sc->pre_state = SC27XX_ATTACHED_SNK;
		extcon_set_state_sync(sc->edev, EXTCON_USB, true);
		break;
	case SC27XX_ATTACHED_SRC:
		sc->pre_state = SC27XX_ATTACHED_SRC;
		extcon_set_state_sync(sc->edev, EXTCON_USB_HOST, true);
		break;
	case SC27XX_AUDIO_CABLE:
		sc->pre_state = SC27XX_AUDIO_CABLE;
		extcon_set_state_sync(sc->edev, EXTCON_JACK_HEADPHONE, true);
		break;
	default:
		break;
	}

	spin_lock(&sc->lock);
	sc->partner_connected = true;
	spin_unlock(&sc->lock);
	sc27xx_typec_set_cc_polarity_role(sc);

	return 0;
}

static int sc27xx_typec_connect(struct sc27xx_typec *sc, u32 status)
{
	int ret;

	if (sc->use_pdhub_c2c)
		ret = sc27xx_connect_set_status_use_pdhubc2c(sc, status);
	else
		ret = sc27xx_connect_set_status_no_pdhubc2c(sc, status);

	return ret;
}
static void sc27xx_disconnect_set_status_use_pdhubc2c(struct sc27xx_typec *sc)
{
	u8 pr_mode, dr_mode;

	/*rp rd is controlled by HW*/
	sc27xx_typec_set_rp_rd(SPRD_TYPEC_CC_OPEN);
	/*default rp level*/
	sc27xx_typec_set_rp_level(SPRD_TYPEC_CC_RP_DEF);
	spin_lock(&sc->lock);
	sc->partner_connected = false;
	sc->pd_swap_evt = TYPEC_NO_SWAP;
	switch (sc->pre_state) {
	case SC27XX_ATTACHED_SNK:
	case SC27XX_DEBUG_CABLE:
	case SC27XX_ATTACHED_SRC:
		pr_mode = sc->pr_mode;
		dr_mode = sc->dr_mode;
		sc->pr_mode = EXTCON_NONE;
		sc->dr_mode = EXTCON_NONE;
		break;
	default:
		break;
	}
	spin_unlock(&sc->lock);

	switch (sc->pre_state) {
	case SC27XX_ATTACHED_SNK:
	case SC27XX_DEBUG_CABLE:
	case SC27XX_ATTACHED_SRC:
		dev_info(sc->dev, "%s dr_mode:%d  pr_mode:%d!\n",
					__func__, dr_mode, pr_mode);
		sc27xx_set_current_status_detach_or_attach(0);

		if (pr_mode == EXTCON_SOURCE)
			extcon_set_state_sync(sc->edev, EXTCON_SOURCE, false);
		else if (pr_mode == EXTCON_SINK)
			extcon_set_state_sync(sc->edev, EXTCON_SINK, false);

		if (dr_mode == EXTCON_USB_HOST)
			extcon_set_state_sync(sc->edev, EXTCON_USB_HOST, false);
		else if (dr_mode == EXTCON_USB)
			extcon_set_state_sync(sc->edev, EXTCON_USB, false);

		break;
	case SC27XX_AUDIO_CABLE:
		extcon_set_state_sync(sc->edev, EXTCON_JACK_HEADPHONE, false);
		break;
	default:
		break;
	}

}

static void sc27xx_disconnect_set_status_no_pdhubc2c(struct sc27xx_typec *sc)
{
	typec_unregister_partner(sc->partner);
	sc->partner = NULL;
	typec_set_pwr_opmode(sc->port, TYPEC_PWR_MODE_USB);
	typec_set_pwr_role(sc->port, TYPEC_SINK);
	typec_set_data_role(sc->port, TYPEC_DEVICE);
	typec_set_vconn_role(sc->port, TYPEC_SINK);

	spin_lock(&sc->lock);
	sc->partner_connected = false;
	spin_unlock(&sc->lock);

	switch (sc->pre_state) {
	case SC27XX_ATTACHED_SNK:
	case SC27XX_DEBUG_CABLE:
		extcon_set_state_sync(sc->edev, EXTCON_USB, false);
		break;
	case SC27XX_ATTACHED_SRC:
		extcon_set_state_sync(sc->edev, EXTCON_USB_HOST, false);
		break;
	case SC27XX_AUDIO_CABLE:
		extcon_set_state_sync(sc->edev, EXTCON_JACK_HEADPHONE, false);
		break;
	default:
		break;
	}
}

static void sc27xx_typec_disconnect(struct sc27xx_typec *sc, u32 status)
{
	if (sc->use_pdhub_c2c)
		sc27xx_disconnect_set_status_use_pdhubc2c(sc);
	else
		sc27xx_disconnect_set_status_no_pdhubc2c(sc);
}

static int sc27xx_typec_random_tdrp(struct sc27xx_typec *sc)
{
	int ret = 0;
	u32 rand;

	/* workaround: the two devices are connected through CC cables and restarted
	 * at the same time. there is no interruption in insertion and removal. Set
	 * the toggle period 50~100 ms of two mobile phones at random.
	 */
	get_random_bytes(&rand, 4);
	rand &= 0xfff;
	rand = SC27XX_MIN_TDRP_CNT + rand % SC27XX_TDRP_RANGE;

	if (sc->var_data->pmic_name == SC2730 || sc->var_data->pmic_name == UMP9620) {
		ret = regmap_write(sc->regmap, sc->base + SC2730_TDRP_CNT, rand);
		dev_info(sc->dev, "SC27XX_MIN_TERR_CNT = %d: %d ms\n", rand,
					(rand - SC27XX_MIN_TDRP_CNT) / SC27XX_TDRP_TIMER +
					SC27XX_MIN_TDRP_MS);
	}

	return ret;
}

static irqreturn_t sc27xx_typec_interrupt(int irq, void *data)
{
	struct sc27xx_typec *sc = data;
	u32 event;
	int ret;

	dev_info(sc->dev, "%s enter line %d\n", __func__, __LINE__);
	ret = regmap_read(sc->regmap, sc->base + SC27XX_INT_MASK, &event);
	if (ret)
		return ret;

	event &= sc->var_data->event_mask;

	ret = regmap_read(sc->regmap, sc->base + SC27XX_STATUS, &sc->state);
	if (ret)
		goto clear_ints;

	sc->state &= sc->var_data->state_mask;

	if (event & SC27XX_ATTACH_INT) {
		ret = sc27xx_typec_connect(sc, sc->state);
		if (ret)
			dev_warn(sc->dev, "failed to register partner\n");
	} else if (event & SC27XX_DETACH_INT) {
		sc27xx_typec_disconnect(sc, sc->state);
		ret = sc27xx_typec_random_tdrp(sc);
		if (ret)
			dev_warn(sc->dev, "failed to random tdrp\n");
	}

clear_ints:
	regmap_write(sc->regmap, sc->base + sc->var_data->int_clr, event);

	dev_info(sc->dev, "now works as DRP and is in %d state, event %d\n",
		sc->state, event);
	return IRQ_HANDLED;
}

static int sc27xx_typec_enable(struct sc27xx_typec *sc)
{
	int ret;
	u32 val;

	/* Set typec mode */
	ret = regmap_read(sc->regmap, sc->base + sc->var_data->mode, &val);
	if (ret)
		return ret;

	val &= ~SC27XX_MODE_MASK;
	switch (sc->mode) {
	case TYPEC_PORT_DFP:
		val |= SC27XX_MODE_SRC;
		break;
	case TYPEC_PORT_UFP:
		val |= SC27XX_MODE_SNK;
		break;
	case TYPEC_PORT_DRP:
		val |= SC27XX_MODE_DRP;
		break;
	default:
		return -EINVAL;
	}

	ret = regmap_write(sc->regmap, sc->base + sc->var_data->mode, val);
	if (ret)
		return ret;

	/* typec USB20 only flag, only work in snk mode */
	if (!sc->use_pdhub_c2c) {
		if (sc->typec_cap.data == TYPEC_PORT_UFP && sc->usb20_only) {
			ret = regmap_update_bits(sc->regmap, sc->base + SC27XX_EN,
						SC27XX_TYPEC_USB20_ONLY,
						SC27XX_TYPEC_USB20_ONLY);
			if (ret)
				return ret;
		}
	}

	/* modify sc2730 tcc debounce to 100ms while PD signal occur at 150ms
	 * and effect tccde reginize.Reason is hardware signal and clk not
	 * accurate.
	 */
	if (sc->var_data->pmic_name == SC2730 || sc->var_data->pmic_name == UMP9620) {
		ret = regmap_write(sc->regmap, sc->base + SC27XX_TCCDE_CNT,
				SC27XX_TCC_DEBOUNCE_CNT);
		if (ret)
			return ret;
	}

	/* Enable typec interrupt and enable typec */
	ret = regmap_read(sc->regmap, sc->base + sc->var_data->int_en, &val);
	if (ret)
		return ret;

	val |= sc->var_data->attach_en | sc->var_data->detach_en;
	return regmap_write(sc->regmap, sc->base + sc->var_data->int_en, val);
}

static const u32 sc27xx_typec_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_SOURCE,
	EXTCON_SINK,
	EXTCON_JACK_HEADPHONE,
	EXTCON_NONE,
};

static int sc27xx_typec_get_cc1_efuse(struct sc27xx_typec *sc)
{
	struct nvmem_cell *cell;
	u32 calib_data = 0;
	void *buf;
	size_t len;

	cell = nvmem_cell_get(sc->dev, "typec_cc1_cal");
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(&calib_data, buf, min(len, sizeof(u32)));
	calib_data = (calib_data & SC27XX_CC1_MASK(sc->var_data->efuse_cc1_shift))
			>> SC27XX_CC_SHIFT(sc->var_data->efuse_cc1_shift);
	kfree(buf);

	return calib_data;
}

static int sc27xx_typec_get_cc2_efuse(struct sc27xx_typec *sc)
{
	struct nvmem_cell *cell;
	u32 calib_data = 0;
	void *buf;
	size_t len = 0;

	cell = nvmem_cell_get(sc->dev, "typec_cc2_cal");
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(&calib_data, buf, min(len, sizeof(u32)));
	calib_data = (calib_data & SC27XX_CC2_MASK(sc->var_data->efuse_cc2_shift))
			>> SC27XX_CC_SHIFT(sc->var_data->efuse_cc2_shift);
	kfree(buf);

	return calib_data;
}

static int typec_set_rtrim(struct sc27xx_typec *sc)
{
	int calib_cc1;
	int calib_cc2;
	u32 rtrim;

	calib_cc1 = sc27xx_typec_get_cc1_efuse(sc);
	if (calib_cc1 < 0)
		return calib_cc1;
	calib_cc2 = sc27xx_typec_get_cc2_efuse(sc);
	if (calib_cc2 < 0)
		return calib_cc2;

	rtrim = calib_cc1 | calib_cc2<<5;

	return regmap_write(sc->regmap, sc->base + SC27XX_RTRIM, rtrim);
}

static ssize_t
typec_cc_polarity_role_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sc27xx_typec *sc = dev_get_drvdata(dev);

	return snprintf(buf, 5, "%s\n", sprd_typec_cc_polarity_roles[sc->cc_polarity]);
}
static DEVICE_ATTR_RO(typec_cc_polarity_role);

static struct attribute *sc27xx_typec_attrs[] = {
	&dev_attr_typec_cc_polarity_role.attr,
	NULL,
};
ATTRIBUTE_GROUPS(sc27xx_typec);

int sc27xx_set_typec_int_clear(void)
{
	struct sc27xx_typec *sc = typec_sc;
	u32 event;
	int ret;

	ret = regmap_read(sc->regmap, sc->base + SC27XX_INT_RAW, &event);
	if (ret)
		return ret;

	event &= sc->var_data->event_mask;
	/* preserve the con/discon interrupt, clr_work selective clear int */
	event &= ~SC27XX_DE_ATTACH_MASK;

	dev_info(sc->dev, "%s SC27XX_INT_RAW:0x%x!\n", __func__, event);

	regmap_write(sc->regmap, sc->base + sc->var_data->int_clr, event);

	/* pd swap,cc need debounce to report interrupt */
	queue_delayed_work(system_unbound_wq, &sc->typec_int_clr_work, msecs_to_jiffies(300));

	return 0;
}

int sc27xx_set_typec_int_disable(void)
{
	struct sc27xx_typec *sc = typec_sc;
	int ret = 0;
	u32 val;

	dev_info(sc->dev, "%s enter line %d\n", __func__, __LINE__);
	sc->soft_typec_out = true;

	if (!cancel_delayed_work_sync(&sc->typec_int_clr_work)) {
		ret = regmap_read(sc->regmap,
				sc->base + sc->var_data->int_en, &val);
		if (ret)
			return ret;

		ret = val & (sc->var_data->attach_en | sc->var_data->detach_en);
		if (!ret)
			msleep(20);
	}

	/* disable typec interrupt for attach/detach */
	ret = regmap_read(sc->regmap, sc->base + sc->var_data->int_en, &val);
	if (ret)
		return ret;

	val &= ~(sc->var_data->attach_en | sc->var_data->detach_en);
	return regmap_write(sc->regmap, sc->base + sc->var_data->int_en, val);
}

int sc27xx_set_typec_int_enable(void)
{
	struct sc27xx_typec *sc = typec_sc;
	int ret = 0;
	u32 val;

	dev_info(sc->dev, "%s enter line %d\n", __func__, __LINE__);

	/* Enable typec interrupt and enable typec */
	ret = regmap_read(sc->regmap, sc->base + sc->var_data->int_en, &val);
	if (ret)
		return ret;

	val |= sc->var_data->attach_en | sc->var_data->detach_en;
	return regmap_write(sc->regmap, sc->base + sc->var_data->int_en, val);
}

/*
 *  TYPEC_DEVICE_TO_HOST	device->host
 *  TYPEC_HOST_TO_DEVICE	host->device
 */
int sc27xx_typec_set_pd_dr_swap_flag(u8 flag)
{
	struct sc27xx_typec *sc = typec_sc;

	dev_info(sc->dev, "%s enter line %d\n", __func__, __LINE__);

	return 0;
}

/*
 * TYPEC_SINK_TO_SOURCE		sink->source
 * TYPEC_SOURCE_TO_SINK		source->sink
 */
int sc27xx_typec_set_pr_swap_flag(u8 flag)
{
	struct sc27xx_typec *sc = typec_sc;

	dev_info(sc->dev, "%s flag:%d!\n", __func__, flag);

	return 0;
}

/*
 * if pd pr/dr swap ok,send extcon event by set api.
 */
int sc27xx_typec_set_pd_swap_event(u8 pd_swap_flag)
{
	struct sc27xx_typec *sc = typec_sc;

	dev_info(sc->dev, "%s pd_swap_flag:%d!\n", __func__, pd_swap_flag);

	spin_lock(&sc->lock);
	if (!sc->partner_connected) {
		spin_unlock(&sc->lock);
		dev_info(sc->dev, "typec has disconnected!\n");
		return 0;
	}

	switch (pd_swap_flag) {
	case TYPEC_SOURCE_TO_SINK:
		sc->pr_mode = EXTCON_SINK;
		spin_unlock(&sc->lock);
		extcon_set_state_sync(sc->edev, EXTCON_SOURCE, false);
		extcon_set_state_sync(sc->edev, EXTCON_SINK, true);
		break;
	case TYPEC_SINK_TO_SOURCE:
		sc->pr_mode = EXTCON_SOURCE;
		spin_unlock(&sc->lock);
		extcon_set_state_sync(sc->edev, EXTCON_SINK, false);
		extcon_set_state_sync(sc->edev, EXTCON_SOURCE, true);
		break;
	case TYPEC_HOST_TO_DEVICE:
	case TYPEC_DEVICE_TO_HOST:
		if (pd_swap_flag != sc->pd_swap_evt) {
			sc->pd_swap_evt = pd_swap_flag;
			spin_unlock(&sc->lock);
			/* delay one jiffies for scheduling purpose */
			schedule_delayed_work(&sc->drswap_work, msecs_to_jiffies(5));
		} else {
			spin_unlock(&sc->lock);
			dev_err(sc->dev, "%s igore pd_swap_flag:%d!\n",
						__func__, pd_swap_flag);
		}
		break;
	default:
		spin_unlock(&sc->lock);
		return -EINVAL;
	}

	return 0;
}

static int sc27xx_typec_set_rp_level_value(enum typec_rp_level value)
{
	struct sc27xx_typec *sc = typec_sc;
	int ret;

	dev_info(sc->dev, "rp level: %s", typec_rp_level_string(value));
	ret =  regmap_update_bits(sc->regmap, sc->base + SC27XX_MODE,
				SC27XX_TYPEC_RP_LEVEL(3), SC27XX_TYPEC_RP_LEVEL(value));
	if (ret < 0) {
		dev_err(sc->dev, "failed to set typec rp level, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int sc27xx_typec_set_rp_rd_value(enum typec_rp_rd_state value)
{
	struct sc27xx_typec *sc = typec_sc;
	int ret;

	dev_info(sc->dev, "set %s %s",
				sprd_typec_cc_polarity_roles[sc->cc_polarity],
				typec_rp_rd_string(value));
	if (sc->cc_polarity == SPRD_TYPEC_POLARITY_CC1) {
		ret = regmap_update_bits(sc->regmap,
					sc->base + SC27XX_SW_CFG,
					SC27XX_CC1_SW_SWITCH_MASK,
					SC27XX_CC1_SW_SWITCH(value));
	} else {
		ret = regmap_update_bits(sc->regmap,
					sc->base + SC27XX_SW_CFG,
					SC27XX_CC2_SW_SWITCH_MASK,
					SC27XX_CC2_SW_SWITCH(value));
	}

	if (ret < 0) {
		dev_err(sc->dev, "failed to set typec rp rd, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int sc27xx_typec_set_rp_level(enum sprd_typec_cc_status cc)
{
	struct sc27xx_typec *sc = typec_sc;
	int ret;

	dev_info(sc->dev, "%s cc:%d!\n", __func__, cc);

	switch (cc) {
	case SPRD_TYPEC_CC_RP_DEF:
		ret = sc27xx_typec_set_rp_level_value(RP_80UA);
		break;
	case SPRD_TYPEC_CC_RP_1_5:
		ret = sc27xx_typec_set_rp_level_value(RP_180UA);
		break;
	case SPRD_TYPEC_CC_RP_3_0:
		ret = sc27xx_typec_set_rp_level_value(RP_330UA);
		break;
	default:
		/*rp level default */
		ret = sc27xx_typec_set_rp_level_value(RP_80UA);
		break;
	}

	if (ret < 0)
		return ret;

	return 0;
}

static int sc27xx_typec_set_rp_rd(enum sprd_typec_cc_status cc)
{
	struct sc27xx_typec *sc = typec_sc;
	int ret;

	dev_info(sc->dev, "%s cc:%d!\n", __func__, cc);

	switch (cc) {
	case SPRD_TYPEC_CC_RD:
		/* force cc connected by rd */
		ret = sc27xx_typec_set_rp_rd_value(SC27XX_TYPEC_RD);
		break;
	case SPRD_TYPEC_CC_RP_DEF:
	case SPRD_TYPEC_CC_RP_1_5:
	case SPRD_TYPEC_CC_RP_3_0:
		/* force cc connected by rp */
		ret = sc27xx_typec_set_rp_rd_value(SC27XX_TYPEC_RP);
		break;
	case SPRD_TYPEC_CC_OPEN:
	default:
		/* cc_switch is controlled by HW */
		ret = sc27xx_typec_set_rp_rd_value(SC27XX_TYPEC_HW);
		break;
	}

	if (ret < 0)
		return ret;

	return 0;
}

#if IS_ENABLED(CONFIG_SPRD_TYPEC_TCPM)
static int sc27xx_typec_register_ops(void)
{
	if (!typec_sc->use_pdhub_c2c)
		return 0;

	sc27xx_typec_ops.name = "sc27xx-typec";
	sc27xx_typec_ops.set_typec_int_clear = sc27xx_set_typec_int_clear;
	sc27xx_typec_ops.set_typec_int_disable = sc27xx_set_typec_int_disable;
	sc27xx_typec_ops.set_typec_int_enable = sc27xx_set_typec_int_enable;
	sc27xx_typec_ops.typec_set_pd_dr_swap_flag = sc27xx_typec_set_pd_dr_swap_flag;
	sc27xx_typec_ops.typec_set_pr_swap_flag = sc27xx_typec_set_pr_swap_flag;
	sc27xx_typec_ops.typec_set_pd_swap_event = sc27xx_typec_set_pd_swap_event;
	sc27xx_typec_ops.set_typec_rp_rd = sc27xx_typec_set_rp_rd;
	sc27xx_typec_ops.set_typec_rp_level = sc27xx_typec_set_rp_level;

	sprd_tcpm_typec_device_ops_register(&sc27xx_typec_ops);

	return 0;
}
#else
static int sc27xx_typec_register_ops(void)
{
	return 0;
}
#endif

static void sc27xx_typec_int_clr_work(struct work_struct *work)
{
	struct sc27xx_typec *sc = typec_sc;
	bool is_detach;
	u32 event;
	u32 temp_event;
	u32 state;
	int ret;

	dev_info(sc->dev, "%s enter line %d\n", __func__, __LINE__);
	/* pd swap,cc need debounce to report interrupt */
	ret = regmap_read(sc->regmap, sc->base + SC27XX_INT_RAW, &event);
	if (ret)
		return;

	event &= sc->var_data->event_mask;
	temp_event = event;

	ret = regmap_read(sc->regmap, sc->base + SC27XX_STATUS, &state);
	if (ret)
		return;

	state &= sc->var_data->state_mask;

	/*
	 * workaround：when pr swap and typec pull out, we need to preserve the
	 * disconnect interrupt
	 */
	dev_info(sc->dev, "%s SC27XX_INT_RAW:0x%x state:%d!\n", __func__, event, state);

	switch (state) {
	case SC27XX_DETACHED_SNK:
	case SC27XX_DETACHED_SRC:
	case SC27XX_TOGGLE_SLEEP:
		is_detach = true;
		break;
	default:
		is_detach = false;
		break;
	}

	if (is_detach || (((temp_event & SC27XX_DE_ATTACH_MASK) == SC27XX_DETACH_INT))) {
		event &= ~SC27XX_DETACH_INT;
		if (sc->soft_typec_out) {
			sc->soft_typec_out = false;
			dev_info(sc->dev, "discon int not clear, need to retry!\n");
			queue_delayed_work(system_unbound_wq, &sc->typec_int_clr_work,
				msecs_to_jiffies(3000));
			return;
		}
		dev_info(sc->dev, "workaround : maybe typec pull out!\n");
	}

	regmap_write(sc->regmap, sc->base + sc->var_data->int_clr, event);

	ret = regmap_read(sc->regmap, sc->base + SC27XX_INT_RAW, &event);
	if (ret)
		return;

	event &= sc->var_data->event_mask;
	dev_info(sc->dev, "%s SC27XX_INT_RAW:0x%x!\n", __func__, event);

	sc27xx_set_typec_int_enable();
}

static void sc27xx_typec_extcon_drswap_work(struct work_struct *work)
{
	struct sc27xx_typec *sc = typec_sc;

	dev_info(sc->dev, "%s pd_swap_evt:%d!\n", __func__, sc->pd_swap_evt);

	spin_lock(&sc->lock);
	switch (sc->pd_swap_evt) {
	case TYPEC_HOST_TO_DEVICE:
		sc->dr_mode = EXTCON_USB;
		spin_unlock(&sc->lock);

		sc27xx_set_dr_swap_executing(1);
		extcon_set_state_sync(sc->edev, EXTCON_USB_HOST, false);
		if (sc27xx_get_current_status_detach_or_attach())
			extcon_set_state_sync(sc->edev, EXTCON_USB, true);
		sc27xx_set_dr_swap_executing(0);
		break;
	case TYPEC_DEVICE_TO_HOST:
		sc->dr_mode = EXTCON_USB_HOST;
		spin_unlock(&sc->lock);

		sc27xx_set_dr_swap_executing(1);
		extcon_set_state_sync(sc->edev, EXTCON_USB, false);
		if (sc27xx_get_current_status_detach_or_attach())
			extcon_set_state_sync(sc->edev, EXTCON_USB_HOST, true);
		sc27xx_set_dr_swap_executing(0);
		break;
	default:
		spin_unlock(&sc->lock);
		return;
	}
}

static int sc27xx_typec_get_vbus_notify(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct sc27xx_typec *sc = container_of(nb,
				  struct sc27xx_typec, vbus_nb);

	spin_lock(&sc->vbus_lock);
	sc->vbus_events = true;
	sc->vbus_connect_value = event;
	spin_unlock(&sc->vbus_lock);

	dev_info(sc->dev, "vbus_connect_value:%d\n", sc->vbus_connect_value);
	queue_delayed_work(system_unbound_wq, &sc->vbus_only_work, 0);

	return 0;
}

static void sc27xx_typec_handle_vbus(struct sc27xx_typec *sc)
{
	u32 timeout = 25;

	dev_info(sc->dev, "%s event %d", __func__, sc->vbus_connect_value);
	if (sc->vbus_connect_value) {
		do {
			/* typec has attach,not vbusonly */
			if (sc->partner_connected) {
				dev_info(sc->dev, "typec connect, not vbusonly\n");
				return;
			}
			msleep(20);
			if (!sc->vbus_connect_value) {
				dev_info(sc->dev, "vbusonly plug out during 500ms\n");
				return;
			}

		} while (--timeout > 0);
		/* vbus attach and 1s no typec attch,
		 * attach cable recognize vbusonly cable.
		 */
		sc->vbus_only_connect = true;
		dev_info(sc->dev, "vbus_only_connect\n");
		extcon_set_state_sync(sc->edev, EXTCON_USB, true);
		extcon_set_state_sync(sc->edev, EXTCON_SINK, true);
		return;
	}

	if (sc->vbus_only_connect) {
		dev_info(sc->dev, "vbus_only_disconnect\n");
		extcon_set_state_sync(sc->edev, EXTCON_USB, false);
		extcon_set_state_sync(sc->edev, EXTCON_SINK, false);
	}
	sc->vbus_only_connect = false;
}

static void sc27xx_typec_vbus_only_work(struct work_struct *work)
{
	struct sc27xx_typec *sc = typec_sc;

	spin_lock(&sc->vbus_lock);
	while (sc->vbus_events) {
		sc->vbus_events = false;
		spin_unlock(&sc->vbus_lock);
		sc27xx_typec_handle_vbus(sc);
		spin_lock(&sc->vbus_lock);
	}

	spin_unlock(&sc->vbus_lock);
}

static int sc27xx_typec_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	struct sc27xx_typec *sc;
	const struct sprd_typec_variant_data *pdata;
	int mode, ret;

	pdata = of_device_get_match_data(dev);
	if (!pdata) {
		dev_err(&pdev->dev, "No matching driver data found\n");
		return -EINVAL;
	}

	sc = devm_kzalloc(&pdev->dev, sizeof(*sc), GFP_KERNEL);
	if (!sc)
		return -ENOMEM;

	sc->edev = devm_extcon_dev_allocate(&pdev->dev, sc27xx_typec_cable);
	if (IS_ERR(sc->edev)) {
		dev_err(&pdev->dev, "failed to allocate extcon device\n");
		return PTR_ERR(sc->edev);
	}

	ret = devm_extcon_dev_register(&pdev->dev, sc->edev);
	if (ret < 0) {
		dev_err(&pdev->dev, "can't register extcon device: %d\n", ret);
		return ret;
	}

	sc->dev = &pdev->dev;
	sc->irq = platform_get_irq(pdev, 0);
	if (sc->irq < 0) {
		dev_err(sc->dev, "failed to get typec interrupt.\n");
		return sc->irq;
	}

	sc->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!sc->regmap) {
		dev_err(sc->dev, "failed to get regmap.\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(node, "reg", &sc->base);
	if (ret) {
		dev_err(dev, "failed to get reg offset!\n");
		return ret;
	}

	ret = of_property_read_u32(node, "sprd,mode", &mode);
	if (ret) {
		dev_err(dev, "failed to get typec port mode type\n");
		return ret;
	}

	sc->use_pdhub_c2c = of_property_read_bool(node, "use_pdhub_c2c");

	if (mode < TYPEC_PORT_DFP || mode > TYPEC_PORT_DRP
	    || mode == TYPEC_PORT_UFP) {
		mode = TYPEC_PORT_UFP;
		sc->usb20_only = true;
		dev_info(dev, "usb 2.0 only is enabled\n");
	}

	sc->var_data = pdata;
	sc->mode = mode;
	if (!sc->use_pdhub_c2c) {
		sc->typec_cap.type = mode;
		sc->typec_cap.data = TYPEC_PORT_DRD;
		//sc->typec_cap.dr_set = sc27xx_typec_dr_set;
		//sc->typec_cap.pr_set = sc27xx_typec_pr_set;
		//sc->typec_cap.vconn_set = sc27xx_typec_vconn_set;
		sc->typec_cap.revision = USB_TYPEC_REV_1_2;
		sc->typec_cap.prefer_role = TYPEC_NO_PREFERRED_ROLE;
		sc->port = typec_register_port(&pdev->dev, &sc->typec_cap);
		if (!sc->port) {
			dev_err(sc->dev, "failed to register port!\n");
			return -ENODEV;
		}
	}

	typec_sc = sc;
	spin_lock_init(&sc->lock);
	spin_lock_init(&sc->vbus_lock);
	sc27xx_set_dr_swap_executing(0);
	sc27xx_set_current_status_detach_or_attach(0);

	ret = sysfs_create_groups(&sc->dev->kobj, sc27xx_typec_groups);
	if (ret < 0)
		dev_err(sc->dev, "failed to create cc_polarity %d\n", ret);

	ret = typec_set_rtrim(sc);
	if (ret < 0) {
		dev_err(sc->dev, "failed to set typec rtrim %d\n", ret);
		goto error;
	}

	ret = devm_request_threaded_irq(sc->dev, sc->irq, NULL,
					sc27xx_typec_interrupt,
					IRQF_EARLY_RESUME | IRQF_ONESHOT,
					dev_name(sc->dev), sc);
	if (ret) {
		dev_err(sc->dev, "failed to request irq %d\n", ret);
		goto error;
	}

	/* typec get vbus extcon notify */
	if (of_property_read_bool(pdev->dev.of_node, "extcon")) {
		sc->vbus_dev = extcon_get_edev_by_phandle(&pdev->dev, 0);
		if (IS_ERR(sc->vbus_dev)) {
			ret = PTR_ERR(sc->vbus_dev);
			dev_err(sc->dev, "typec failed to get vbus extcon.\n");
			goto error;
		}
		sc->vbus_nb.notifier_call = sc27xx_typec_get_vbus_notify;
		ret = extcon_register_notifier(sc->vbus_dev, EXTCON_USB,
						&sc->vbus_nb);
		if (ret) {
			dev_err(sc->dev, "failed to register vbus extcon.\n");
			goto error;
		}
		dev_info(sc->dev, "typec vbus notify on.\n");
	} else {
		dev_info(sc->dev, "typec vbus notify off.\n");
	}

	sc->pd_swap_evt = TYPEC_NO_SWAP;
	INIT_DELAYED_WORK(&sc->typec_int_clr_work, sc27xx_typec_int_clr_work);
	INIT_DELAYED_WORK(&sc->drswap_work, sc27xx_typec_extcon_drswap_work);
	INIT_DELAYED_WORK(&sc->vbus_only_work, sc27xx_typec_vbus_only_work);
	if (extcon_get_state(sc->vbus_dev, EXTCON_USB) == true)
		sc27xx_typec_get_vbus_notify(&sc->vbus_nb, true, sc->vbus_dev);

	sc27xx_typec_register_ops();

	ret = sc27xx_typec_enable(sc);
	if (ret)
		goto error;

	platform_set_drvdata(pdev, sc);
	return 0;

error:
	if (!sc->use_pdhub_c2c)
		typec_unregister_port(sc->port);
	return ret;
}

static int sc27xx_typec_remove(struct platform_device *pdev)
{
	struct sc27xx_typec *sc = platform_get_drvdata(pdev);

	sysfs_remove_groups(&sc->dev->kobj, sc27xx_typec_groups);
	sc27xx_typec_disconnect(sc, 0);
	if (!sc->use_pdhub_c2c)
		typec_unregister_port(sc->port);

	return 0;
}

static const struct of_device_id typec_sprd_match[] = {
	{.compatible = "sprd,sc2730-typec", .data = &sc2730_data},
	{.compatible = "sprd,sc2721-typec", .data = &sc2721_data},
	{.compatible = "sprd,ump96xx-typec", .data = &ump9620_data},
	{},
};
MODULE_DEVICE_TABLE(of, typec_sprd_match);

static struct platform_driver sc27xx_typec_driver = {
	.probe = sc27xx_typec_probe,
	.remove = sc27xx_typec_remove,
	.driver = {
		.name = "sc27xx-typec",
		.of_match_table = typec_sprd_match,
	},
};
module_platform_driver(sc27xx_typec_driver);
MODULE_LICENSE("GPL v2");

