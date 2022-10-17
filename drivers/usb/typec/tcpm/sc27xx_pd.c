// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2019 Spreadtrum Communications Inc.

#include <linux/delay.h>
#include <linux/extcon-provider.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/usb/typec.h>
#include <linux/usb/sprd_tcpm.h>
#include <linux/usb/sprd_pd.h>
#include <linux/usb/typec_dp.h>

/* PMIC global registers definition */
#define SC27XX_MODULE_EN		0x1808
#define SC27XX_ARM_CLK_EN0		0x180c
#define SC27XX_RTC_CLK_EN0		0x1810
#define SC27XX_XTL_WAIT_CTRL0		0x1b78
#define UMP9620_MODULE_EN		0x2008
#define UMP9620_ARM_CLK_EN0		0x200c
#define UMP9620_RTC_CLK_EN0		0x2010
#define UMP9620_XTL_WAIT_CTRL0		0x2378
#define SC27XX_TYPEC_PD_EN		BIT(13)
#define SC27XX_CLK_PD_EN		BIT(9)
#define SC27XX_XTL_EN			BIT(8)

/* Typec controller registers definition */
#define SC27XX_TYPC_PD_CFG		0x8
#define SC27XX_TYPEC_STATUS		0x1c
#define SC27XX_TYPEC_SW_CFG		0x54
#define SC27XX_TYPEC_DBG1		0x60
#define SC27XX_TYPEC_IBIAS		0x70

/* PD controller registers definition */
#define SC27XX_PD_TX_BUF		0x0
#define SC27XX_PD_RX_BUF		0x4
#define SC27XX_PD_HEAD_CFG		0x8
#define SC27XX_PD_CTRL			0xc
#define SC27XX_PD_CFG0			0x10
#define SC27XX_PD_CFG1			0x14
#define SC27XX_PD_MESG_ID_CFG		0x18
#define SC27XX_PD_STS0			0x1c
#define SC27XX_PD_STS1			0x20
#define SC27XX_INT_STS			0x24
#define SC27XX_INT_FLG			0x28
#define SC27XX_INT_CLR			0x2c
#define SC27XX_INT_EN			0x30
#define SC27XX_PD_PHY_CFG0		0x50
#define SC27XX_PD_PHY_CFG1		0x54
#define SC27XX_PD_PHY_CFG2		0x58

/* Bits definitions for SC27XX_TYPC_PD_CFG register */
#define SC27XX_TYPEC_PD_SUPPORT		BIT(0)
#define SC27XX_TYPEC_PD_CONSTRACT	BIT(6)
#define SC27XX_TYPEC_PD_NO_CHEK_DETACH	BIT(9)
#define SC27XX_TYPEC_SW_FORCE_CC(x)	(((x) << 10) & GENMASK(11, 10))
#define SC27XX_TYPEC_VCCON_LDO_RDY	BIT(12)
#define SC27XX_TYPEC_VCCON_LDO_EN	BIT(13)

/* Bits definitions for SC27XX_TYPEC_STATUS register */
#define SC27XX_TYPEC_CURRENT_STATUS	GENMASK(4, 0)
#define SC27XX_TYPEC_FINAL_SWITCH	BIT(5)
#define SC27XX_TYPEC_VBUS_CL(x)		(((x) & GENMASK(7, 6)) >> 6)

/* Bits definitions for SC27XX_TYPEC_DBG1 register */
#define SC27XX_TYPEC_VBUS_OK		BIT(8)
#define SC27XX_TYPEC_CONN_CC		BIT(9)

/* Bits definitions for SC27XX_TYPEC_IBIAS register */
#define SC27XX_TYPEC_RX_REF_DECREASE	BIT(7)

/* Bits definitions for SC27XX_PD_HEAD_CFG register */
#define SC27XX_PD_EXTHEAD		BIT(15)
#define SC27XX_PD_NUM_DO(x)		(((x) << 12) & GENMASK(14, 12))
#define SC27XX_PD_MESS_ID(x)		(((x) << 9) & GENMASK(11, 9))
#define SC27XX_PD_POWER_ROLE		BIT(8)
#define SC27XX_PD_SPEC_REV(x)		(((x) << 6) & GENMASK(7, 6))
#define SC27XX_PD_DATA_ROLE		BIT(5)
#define SC27XX_PD_MESSAGE_TYPE(x)	((x) & GENMASK(4, 0))
#define SC27XX_PD_SPEC_MASK		GENMASK(7, 6)

/* Bits definitions for SC27XX_PD_CTRL register */
#define SC27XX_PD_TX_START		BIT(0)
#define SC27XX_PD_HARD_RESET		BIT(2)
#define SC27XX_PD_TX_FLASH		BIT(3)
#define SC27XX_PD_RX_FLASH		BIT(4)
#define SC27XX_PD_FAST_START		BIT(6)
#define SC27XX_PD_CABLE_RESET		BIT(7)
#define SC27XX_PD_RX_ID_CLR		BIT(8)
#define SC27XX_PD_RP_SINKTXNG_CLR	BIT(9)

/* Bits definitions for SC27XX_PD_CFG0 register */
#define SC27XX_PD_RP_CONTROL(x)		((x) & GENMASK(1, 0))
#define SC27XX_PD_SINK_RP		GENMASK(3, 2)
#define SC27XX_PD_EN_SOP1_TX		BIT(6)
#define SC27XX_PD_EN_SOP2_TX		BIT(7)
#define SC27XX_PD_EN_SOP		BIT(8)
#define SC27XX_PD_SRC_SINK_MODE		BIT(9)
#define SC27XX_PD_CTL_EN		BIT(10)
#define SC27XX_PD_BIST_MODE_EN		BIT(11)

/* Bits definitions for SC27XX_PD_CFG1 register */
#define SC27XX_PD_AUTO_RETRY		BIT(0)
#define SC27XX_PD_RETRY(x)		(((x) << 1) & GENMASK(2, 1))
#define SC27XX_PD_HEADER_REG_EN		BIT(3)
#define SC27XX_PD_EN_SOP1_RX		BIT(6)
#define SC27XX_PD_EN_SOP2_RX		BIT(7)
#define SC27XX_PD_EN_SOP_RX		BIT(8)
#define SC27XX_PD_RX_AUTO_GOOD_CRC	BIT(9)
#define SC27XX_PD_FRS_DETECT_EN		BIT(10)
#define SC27XX_PD_PHY_13M		BIT(11)
#define SC27XX_PD_TX_AUTO_GOOD_CRC	BIT(12)

/* Bits definitions for SC27XX_PD_MESG_ID_CFG register */
#define SC27XX_PD_MESS_ID_TX(x)		((x) & GENMASK(2, 0))
#define SC27XX_PD_MESS_ID_RX(x)		(((x) << 4) & GENMASK(6, 4))
#define SC27XX_PD_RETRY_MASK		GENMASK(2, 1)
#define SC27XX_PD_MESS_ID_MASK		GENMASK(2, 0)

/* Bits definitions for SC27XX_TYPEC_SW_CFG register */
#define SC27XX_TYPEC_SW_SWITCH(x)	(((x) << 10) & GENMASK(11, 10))

/* Bits definitions for SC27XX_INT_FLG register */
#define SC27XX_PD_HARD_RST_FLAG		BIT(0)
#define SC27XX_PD_CABLE_RST_FLAG	BIT(1)
#define SC27XX_PD_SOFT_RST_FLAG		BIT(2)
#define SC27XX_PD_PS_RDY_FLAG		BIT(3)
#define SC27XX_PD_PKG_RV_FLAG		BIT(4)
#define SC27XX_PD_TX_OK_FLAG		BIT(5)
#define SC27XX_PD_TX_ERROR_FLAG		BIT(6)
#define SC27XX_PD_TX_COLLSION_FLAG	BIT(7)
#define SC27XX_PD_PKG_RV_ERROR_FLAG	BIT(8)
#define SC27XX_PD_FRS_RV_FLAG		BIT(9)
#define SC27XX_PD_RX_FIFO_OVERFLOW_FLAG	BIT(10)

/* Bits definitions for SC27XX_PD_STS1 register */
#define SC27XX_PD_RX_DATA_NUM_MASK	GENMASK(8, 0)
#define SC27XX_PD_RX_EMPTY		BIT(13)

/* Bits definitions for SC27XX_INT_CLR register */
#define SC27XX_PD_HARD_RST_RV_CLR	BIT(0)
#define SC27XX_PD_CABLE_RST_RV_CLR	BIT(1)
#define SC27XX_PD_SOFT_RST_RV_CLR	BIT(2)
#define SC27XX_PD_PS_RDY_CLR		BIT(3)
#define SC27XX_PD_PKG_RV_CLR		BIT(4)
#define SC27XX_PD_TX_OK_CLR		BIT(5)
#define SC27XX_PD_TX_ERROR_CLR		BIT(6)
#define SC27XX_PD_TX_COLLSION_CLR	BIT(7)
#define SC27XX_PD_PKG_RV_ERROR_CLR	BIT(8)
#define SC27XX_PD_FRS_RV_CLR		BIT(9)
#define SC27XX_PD_RX_FIFO_OVERFLOW_CLR	BIT(10)

/* Bits definitions for SC27XX_INT_EN register */
#define SC27XX_PD_HARD_RST_RV_EN	BIT(0)
#define SC27XX_PD_CABLE_RST_RV_EN	BIT(1)
#define SC27XX_PD_SOFT_RST_RV_EN	BIT(2)
#define SC27XX_PD_PS_RDY_EN		BIT(3)
#define SC27XX_PD_PKG_RV_EN		BIT(4)
#define SC27XX_PD_TX_OK_EN		BIT(5)
#define SC27XX_PD_TX_ERROR_EN		BIT(6)
#define SC27XX_PD_TX_COLLSION_EN	BIT(7)
#define SC27XX_PD_PKG_RV_ERROR_EN	BIT(8)
#define SC27XX_PD_FRS_RV_EN		BIT(9)
#define SC27XX_PD_RX_FIFO_OVERFLOW_EN	BIT(10)

/* SC27XX_PD_PHY_CFG1 */
#define SC27XX_PD_CC1_SW		BIT(3)
#define SC27XX_PD_CC2_SW		BIT(2)

/* SC27XX_PD_PHY_CFG2 */
#define SC27XX_PD_CFG2_PD_CLK_BIT		BIT(6)
#define SC27XX_PD_CFG2_RX_REF_CAL_BIT		BIT(7)
#define SC27XX_PD_CFG2_RX_REF_CAL_SHIFT		7

#define SC27XX_TX_RX_BUF_MASK		GENMASK(15, 0)
#define SC27XX_PD_INT_CLR		GENMASK(13, 0)
#define SC27XX_STATE_MASK		GENMASK(4, 0)
#define SC27XX_EVENT_MASK		GENMASK(15, 0)
#define SC27XX_TYPEC_INT_CLR_MASK	GENMASK(9, 0)
#define SC27XX_PD_HEAD_CONFIG_MASK	GENMASK(15, 0)
#define SC27XX_PD_CFG0_MASK		GENMASK(8, 0)
#define SC27XX_PD_PHY_CFG0_MASK		0x5102
#define	SC27XX_PD_PHY_CFG1_MASK		0x463c
#define SC27XX_PD_PHY_CFG2_MASK		0x40

#define SC27XX_PD_CFG0_RCCAL_MASK	GENMASK(8, 6)
#define SC27XX_PD_CFG0_VTL_MASK		GENMASK(3, 2)
#define SC27XX_PD_CFG0_VTH_MASK		GENMASK(1, 0)
#define SC27XX_PD_CFG0_RCCAL_SHIFT	6
#define SC27XX_PD_CFG0_VTL_SHIFT	2
#define SC27XX_PD_CFG1_VREF_SEL_MASK		GENMASK(7, 6)
#define SC27XX_PD_CFG1_REF_CAL_MASK		GENMASK(14, 12)
#define SC27XX_PD_CFG1_VREF_SEL_SHIFT		6
#define SC27XX_PD_CFG1_REF_CAL_SHIFT		12

#define SC27XX_PD_DATA_MASK		GENMASK(15, 0)
#define SC27XX_INT_CLR_MASK		0x3fff
#define SC27XX_INT_EN_MASK		0x85f7
#define SC27xx_DETECT_TYPEC_DELAY	700

/* chunked extended message */
#define SC27XX_CHUNKED_EXT_MSG_MASK	GENMASK(7, 0)
#define SC27XX_CHUNKED_EXT_MSG_SHIFT	8

/* Timeout (us) for pd data ready according pd datasheet */
#define SC27XX_PD_RDY_TIMEOUT		2000
#define SC27XX_PD_POLL_RAW_STATUS	50

#define SC27XX_TYPEC_VBUS_OK_TIMEOUT	80000
#define SC27XX_TYPEC_VBUS_OK_STATUS	100

/* pmic compatible */
#define SC2730_RC_EFUSE_SHIFT		9
#define SC2730_REF_EFUSE_SHIFT		12
#define SC2730_DELTA_EFUSE_SHIFT	7

#define UMP9620_RC_EFUSE_SHIFT		5
#define UMP9620_REF_EFUSE_SHIFT		6
#define UMP9620_DELTA_EFUSE_SHIFT	9

/* soc compatible */
#define UMS9620_MASK_AON_APB_R2G_ANALOG_BB_TOP_SINDRV_ENA	0x0020
#define UMS9620_REG_AON_APB_MIPI_CSI_POWER_CTRL			0x0350

#define SC27XX_PD_SHIFT(n)		(n)

#define PMIC_SC2730			1
#define PMIC_UMP9620			2

enum sc27xx_state {
	SC27XX_DETACHED_SNK,
	SC27XX_ATTACHWAIT_SNK,
	SC27XX_ATTACHED_SNK,
	SC27XX_DETACHED_SRC,
	SC27XX_ATTACHWAIT_SRC,
	SC27XX_ATTACHED_SRC,
	SC27XX_POWERED_CABLE,
};

enum sc27xx_dp_hpd_status {
	SC27XX_DP_HOT_UNPLUG = 0,
	SC27XX_DP_HOT_PLUG,
	SC27XX_DP_HPD_IRQ,
	SC27XX_DP_TYPE_DISCONNECT,
};

struct sc27xx_pd_variant_data {
	u32 efuse_rc_shift;
	u32 efuse_ref_shift;
	u32 efuse_delta_shift;
	u32 id;
	u32 module_en;
	u32 arm_clk_en0;
	u32 rtc_clk_en0;
	u32 xtl_wait_ctrl0;
	u32 aon_apb_r2g_analog_bb_top_sindrv_ena;
	u32 reg_aon_apb_mipi_csi_power_ctrl;
};

static const struct sc27xx_pd_variant_data sc2730_data = {
	.efuse_rc_shift = SC2730_RC_EFUSE_SHIFT,
	.efuse_ref_shift = SC2730_REF_EFUSE_SHIFT,
	.efuse_delta_shift = SC2730_DELTA_EFUSE_SHIFT,
	.id = PMIC_SC2730,
	.module_en = SC27XX_MODULE_EN,
	.arm_clk_en0 = SC27XX_ARM_CLK_EN0,
	.rtc_clk_en0 = SC27XX_RTC_CLK_EN0,
	.xtl_wait_ctrl0 = SC27XX_XTL_WAIT_CTRL0,
};

static const struct sc27xx_pd_variant_data ump9620_data = {
	.efuse_rc_shift = UMP9620_RC_EFUSE_SHIFT,
	.efuse_ref_shift = UMP9620_REF_EFUSE_SHIFT,
	.efuse_delta_shift = UMP9620_DELTA_EFUSE_SHIFT,
	.id = PMIC_UMP9620,
	.module_en = UMP9620_MODULE_EN,
	.arm_clk_en0 = UMP9620_ARM_CLK_EN0,
	.rtc_clk_en0 = UMP9620_RTC_CLK_EN0,
	.xtl_wait_ctrl0 = UMP9620_XTL_WAIT_CTRL0,
	.aon_apb_r2g_analog_bb_top_sindrv_ena =
		UMS9620_MASK_AON_APB_R2G_ANALOG_BB_TOP_SINDRV_ENA,
	.reg_aon_apb_mipi_csi_power_ctrl =
		UMS9620_REG_AON_APB_MIPI_CSI_POWER_CTRL,
};

struct sc27xx_pd {
	struct device *dev;
	struct extcon_dev *edev;
	struct extcon_dev *extcon;
	struct notifier_block extcon_nb;
	struct sprd_tcpm_port *sprd_tcpm_port;
	struct delayed_work typec_detect_work;
	struct delayed_work  read_msg_work;
	struct workqueue_struct *pd_wq;
	struct regmap *regmap;
	struct regmap *aon_apb;
	struct tcpc_dev tcpc;
	struct mutex lock;
	struct regulator *vbus;
	struct regulator *vconn;
	struct tcpc_config config;
	struct work_struct pd_work;
	const struct sc27xx_pd_variant_data *var_data;
	enum sprd_typec_cc_polarity cc_polarity;
	enum sprd_typec_cc_status cc1;
	enum sprd_typec_cc_status cc2;
	enum typec_role role;
	enum typec_data_role data;
	enum sc27xx_state state;
	bool attached;
	bool constructed;
	bool vconn_on;
	bool vbus_on;
	bool charge_on;
	bool vbus_present;
	u32 base;
	u32 typec_base;
	u32 rc_cal;
	u32 delta_cal;
	u32 ref_cal;
	int msg_flag;
	int need_retry;
	bool typec_online;
	bool pd_attached;
	bool shutdown_flag;
};

/*
 * Logging
 */
static void (*sprd_pd_tcpm_log)(struct sprd_tcpm_port *port, const char *dev_tag,
				const char *fmt, va_list args);

static const char *sprd_pd_log_tag = "[sprd_pd_log]";

static void sprd_pd_log(struct sc27xx_pd *pd, const char *fmt, ...)
{
	va_list args;

	if (!pd->pd_attached)
		return;

	va_start(args, fmt);
	sprd_pd_tcpm_log(pd->sprd_tcpm_port, sprd_pd_log_tag, fmt, args);
	va_end(args);
}

static inline struct sc27xx_pd *tcpc_to_sc27xx_pd(struct tcpc_dev *tcpc)
{
	return container_of(tcpc, struct sc27xx_pd, tcpc);
}

static int sc27xx_pd_set_aon_clock(struct sc27xx_pd *pd, bool on)
{
	int ret;
	u32 mask, reg;

	if (!pd->aon_apb) {
		dev_warn(pd->dev, "aon apb NULL\n");
		return 0;
	}

	mask = pd->var_data->aon_apb_r2g_analog_bb_top_sindrv_ena;
	reg = pd->var_data->reg_aon_apb_mipi_csi_power_ctrl;

	if (on) {
		ret = regmap_update_bits(pd->aon_apb, reg, mask, mask);
		if (ret)
			return ret;
	} else {
		ret = regmap_update_bits(pd->aon_apb, reg, mask, ~mask);
		if (ret)
			return ret;
	}

	return 0;
}

static int sc27xx_pd_clk_cfg(struct sc27xx_pd *pd)
{
	int ret;

	ret = regmap_update_bits(pd->regmap, pd->var_data->module_en,
				 SC27XX_TYPEC_PD_EN, SC27XX_TYPEC_PD_EN);
	if (ret)
		return ret;

	ret = regmap_update_bits(pd->regmap, pd->var_data->arm_clk_en0,
				 SC27XX_CLK_PD_EN, SC27XX_CLK_PD_EN);
	if (ret)
		return ret;

	return regmap_update_bits(pd->regmap, pd->var_data->xtl_wait_ctrl0,
				  SC27XX_XTL_EN, SC27XX_XTL_EN);
}

static int sc27xx_pd_disable_clk(struct sc27xx_pd *pd)
{
	int ret;

	ret = regmap_update_bits(pd->regmap, pd->var_data->module_en,
				 SC27XX_TYPEC_PD_EN, 0);
	if (ret)
		return ret;

	return regmap_update_bits(pd->regmap, pd->var_data->arm_clk_en0,
				 SC27XX_CLK_PD_EN, 0);
}

static int sc27xx_pd_start_drp_toggling(struct tcpc_dev *tcpc,
					enum typec_port_type port_type,
					enum sprd_typec_cc_status cc)
{
	return 0;
}

static int sc27xx_pd_set_cc(struct tcpc_dev *tcpc, enum sprd_typec_cc_status cc)
{
	return 0;
}

static int sc27xx_pd_get_cc(struct tcpc_dev *tcpc,
			    enum sprd_typec_cc_status *cc1,
			    enum sprd_typec_cc_status *cc2)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);

	mutex_lock(&pd->lock);
	*cc1 = pd->cc1;
	*cc2 = pd->cc2;
	mutex_unlock(&pd->lock);
	return 0;
}

static int sc27xx_pd_set_vbus(struct tcpc_dev *tcpc, bool on, bool charge)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret;

	mutex_lock(&pd->lock);
	if (pd->vbus_on == on) {
		dev_info(pd->dev, "vbus is already %s\n", on ? "On" : "Off");
	} else {
		if (!pd->vbus) {
			pd->vbus = devm_regulator_get_optional(pd->dev, "vbus");
			if (IS_ERR(pd->vbus)) {
				dev_err(pd->dev, "failed to get vbus supply\n");
				pd->vbus = NULL;
				goto set_vbus_done;
			}
		}

		if (on) {
			if (!regulator_is_enabled(pd->vbus)) {
				ret = regulator_enable(pd->vbus);
				if (ret)  {
					dev_err(pd->dev, "cannot enable vbus regulator, ret=%d\n", ret);
					goto set_vbus_done;
				}
			}
		} else {
			if (regulator_is_enabled(pd->vbus)) {
				ret = regulator_disable(pd->vbus);
				if (ret)  {
					dev_err(pd->dev, "cannot disable vbus regulator, ret=%d\n", ret);
					goto set_vbus_done;
				}
			}
		}

		pd->vbus_on = on;
		dev_info(pd->dev, "vbus := %s", on ? "On" : "Off");
	}

	if (pd->charge_on == charge)
		dev_info(pd->dev,  "charge is already %s\n",
			    charge ? "On" : "Off");
	else
		pd->charge_on = charge;

set_vbus_done:
	mutex_unlock(&pd->lock);

	return 0;
}

static int sc27xx_pd_get_vbus(struct tcpc_dev *tcpc)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret;

	mutex_lock(&pd->lock);
	ret = pd->vbus_present ? 1 : 0;
	mutex_unlock(&pd->lock);
	return ret;
}

static int sc27xx_pd_set_current_limit(struct tcpc_dev *dev, u32 max_ma, u32 mv)
{
	return 0;
}

static int sc27xx_pd_get_current_limit(struct tcpc_dev *dev)
{
	return 0;
}

static int sc27xx_pd_set_polarity(struct tcpc_dev *tcpc,
				  enum sprd_typec_cc_polarity polarity)
{
	return 0;
}

static int sc27xx_pd_set_roles(struct tcpc_dev *tcpc, bool attached,
			       enum typec_role role, enum typec_data_role data)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret;
	u32 mask;

	mutex_lock(&pd->lock);
	pd->role = role;
	pd->data = data;
	pd->attached = attached;
	if (pd->role == TYPEC_SINK)
		mask = (u32)~SC27XX_PD_SRC_SINK_MODE;
	else
		mask = SC27XX_PD_SRC_SINK_MODE;

	ret = regmap_update_bits(pd->regmap,
				 pd->base + SC27XX_PD_CFG0,
				 SC27XX_PD_SRC_SINK_MODE, mask);
	mutex_unlock(&pd->lock);
	return ret;
}

static int sc27xx_pd_set_vconn(struct tcpc_dev *tcpc, bool enable)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret = 0;

	if (!pd->vconn) {
		dev_warn(pd->dev, "vconn NULL!!!\n");
		return -EINVAL;
	}

	mutex_lock(&pd->lock);

	if (pd->vconn_on == enable) {
		dev_info(pd->dev, "vconn already %s\n", enable ? "On" : "Off");
		goto unlock;
	}

	if (enable)
		ret = regulator_enable(pd->vconn);
	else
		ret = regulator_disable(pd->vconn);

	if (!ret)
		pd->vconn_on = enable;

unlock:
	mutex_unlock(&pd->lock);

	return ret;
}

#if IS_ENABLED(CONFIG_SPRD_TYPEC_DP_ALTMODE)
static int sc27xx_pd_dp_altmode_notify(struct tcpc_dev *tcpc, u32 vdo)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	union extcon_property_value hpd_status;

	if (vdo & DP_STATUS_HPD_STATE) {
		if (vdo & DP_STATUS_IRQ_HPD)
			hpd_status.intval = SC27XX_DP_HPD_IRQ;
		else
			hpd_status.intval = SC27XX_DP_HOT_PLUG;
	} else {
		hpd_status.intval = SC27XX_DP_HOT_UNPLUG;
	}

	dev_info(pd->dev, "%s: vdo:0x%x, hpd_status = %d\n", __func__, vdo, hpd_status.intval);
	extcon_set_state(pd->edev, EXTCON_DISP_DP, true);
	extcon_set_property(pd->edev, EXTCON_DISP_DP, EXTCON_PROP_DISP_HPD, hpd_status);
	extcon_sync(pd->edev, EXTCON_DISP_DP);
	return 0;
}
#else
static int sc27xx_pd_dp_altmode_notify(struct tcpc_dev *tcpc, u32 vdo)
{
	return 0;
}
#endif

static int sc27xx_pd_tx_flush(struct sc27xx_pd *pd)
{
	return regmap_update_bits(pd->regmap,
				  pd->base + SC27XX_PD_CTRL, SC27XX_PD_TX_FLASH,
				  SC27XX_PD_TX_FLASH);
}

static int sc27xx_pd_rx_flush(struct sc27xx_pd *pd)
{
	return regmap_update_bits(pd->regmap,
				  pd->base + SC27XX_PD_CTRL, SC27XX_PD_RX_FLASH,
				  SC27XX_PD_RX_FLASH);
}

static int sc27xx_pd_reset(struct sc27xx_pd *pd)
{
	int ret;

	ret = sc27xx_pd_rx_flush(pd);
	if (ret < 0)
		return ret;

	ret = sc27xx_pd_tx_flush(pd);
	if (ret < 0)
		return ret;

	ret = regmap_update_bits(pd->regmap,
				 pd->base + SC27XX_PD_CTRL, SC27XX_PD_RX_ID_CLR,
				 SC27XX_PD_RX_ID_CLR);
	if (ret < 0)
		return ret;

	return regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_MESG_ID_CFG,
				  SC27XX_PD_MESS_ID_MASK, 0x0);
}

static int sc27xx_pd_send_hardreset(struct sc27xx_pd *pd)
{
	int ret, state;

	ret = sc27xx_pd_reset(pd);
	if (ret < 0) {
		dev_err(pd->dev, "cannot PD reset, ret=%d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(pd->regmap,
				 pd->base + SC27XX_PD_CTRL,
				 SC27XX_PD_HARD_RESET, SC27XX_PD_HARD_RESET);
	if (ret < 0)
		return ret;

	state = extcon_get_state(pd->edev, EXTCON_CHG_USB_PD);
	if (state == true)
		extcon_set_state_sync(pd->edev, EXTCON_CHG_USB_PD, false);
	else if (state == false)
		extcon_set_state_sync(pd->edev, EXTCON_CHG_USB_PD, true);

	dev_warn(pd->dev, "IRQ: PD send hardreset\n");

	return 0;
}

static int sc27xx_pd_set_rx(struct tcpc_dev *tcpc, bool on)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	u32 mask = SC27XX_PD_CTL_EN, mask1 = SC27XX_PD_PKG_RV_EN;
	u32 mask2 = SC27XX_PD_RX_AUTO_GOOD_CRC;
	int ret;

	if (pd->shutdown_flag)
		return 0;

	mutex_lock(&pd->lock);
	ret = sc27xx_pd_reset(pd);
	if (ret < 0)
		goto done;

	if (on) {
		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_INT_EN,
					 mask1, mask1);
		if (ret < 0)
			goto done;

		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG1,
					 mask2, mask2);
		if (ret < 0)
			goto done;

		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG0,
					 mask, mask);
		if (ret < 0)
			goto done;
	} else {
		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG0,
					 mask, ~mask);
		if (ret < 0)
			goto done;

		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_INT_EN,
					 mask1, ~mask1);
		if (ret < 0)
			goto done;

		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG1,
					 mask2, ~mask2);
		if (ret < 0)
			goto done;
	}

	dev_info(pd->dev, "pd := %s", on ? "on" : "off");
done:
	mutex_unlock(&pd->lock);
	return ret;
}

static int sc27xx_pd_tx_msg(struct sc27xx_pd *pd, const struct sprd_pd_message *msg)
{
	u16 header;
	u32 data_obj_num, data[SPRD_PD_MAX_PAYLOAD * 2] = {0}, head = 0;
	int i, ret;

	ret = sc27xx_pd_tx_flush(pd);
	if (ret < 0)
		return ret;

	data_obj_num = msg ? sprd_pd_header_cnt_le(msg->header) : 0;
	if (data_obj_num > SPRD_PD_MAX_PAYLOAD) {
		dev_err(pd->dev, "pd tmsg too long, num=%d\n", data_obj_num);
		return -EINVAL;
	}

	header = msg ? le16_to_cpu(msg->header) : 0;
	sprd_pd_log(pd, "tx msg: header = 0x%x", header);
	ret = regmap_write(pd->regmap, pd->base + SC27XX_PD_HEAD_CFG, header);
	if (ret < 0) {
		sprd_pd_log(pd, "write head cfg fail, ret = %d", ret);
		return ret;
	}

	ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_HEAD_CFG, &head);
	if (ret < 0) {
		sprd_pd_log(pd, "read header failed, ret = %d", ret);
		return ret;
	}
	sprd_pd_log(pd, "tx msg: header cfg = 0x%x", head);

	if (msg) {
		for (i = 0; i < data_obj_num; i++) {
			data[2 * i] = le32_to_cpu(msg->payload[i]) &
			SC27XX_PD_DATA_MASK;
			data[2 * i + 1] = (le32_to_cpu(msg->payload[i]) >> 16) &
			SC27XX_PD_DATA_MASK;
		}
	}

	for (i = 0; i < data_obj_num * 2; i++) {
		sprd_pd_log(pd, "tx msg: data[%d] = 0x%x", i, data[i]);
		ret = regmap_write(pd->regmap, pd->base + SC27XX_PD_TX_BUF,
				   data[i]);
		if (ret < 0) {
			sprd_pd_log(pd, "write tx buf fail, ret = %d", ret);
			return ret;
		}
	}

	return regmap_update_bits(pd->regmap,
				  pd->base + SC27XX_PD_CTRL, SC27XX_PD_TX_START,
				  SC27XX_PD_TX_START);
}

static int sc27xx_pd_transmit(struct tcpc_dev *tcpc,
			      enum sprd_tcpm_transmit_type type,
			      const struct sprd_pd_message *msg)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret;

	mutex_lock(&pd->lock);
	switch (type) {
	case SPRD_TCPC_TX_SOP:
		ret = sc27xx_pd_tx_msg(pd, msg);
		if (ret < 0)
			dev_err(pd->dev, "cannot send PD message, ret=%d\n",
				ret);
		break;
	case SPRD_TCPC_TX_HARD_RESET:
		ret = sc27xx_pd_send_hardreset(pd);
		if (ret < 0)
			dev_err(pd->dev, "cann't send hardreset ret=%d\n", ret);
		break;
	default:
		dev_err(pd->dev, "type %d not supported", type);
		ret = -EINVAL;
	}
	mutex_unlock(&pd->lock);

	return ret;
}

static int sc27xx_pd_read_ext_message(struct sc27xx_pd *pd, struct sprd_pd_message *msg)
{
	int ret, i, ext_msg_data_size;
	u32 data[SPRD_PD_MAX_PAYLOAD * 2] = {0};
	u32 ext_msg_header = 0;

	ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_RX_BUF, &ext_msg_header);
	if (ret < 0) {
		dev_err(pd->dev, "%s, failed to read ext_msg_header, ret = %d\n", __func__, ret);
		return ret;
	}

	msg->ext_msg.header = cpu_to_le16(ext_msg_header);
	if (!(le16_to_cpu(msg->ext_msg.header) & SPRD_PD_EXT_HDR_CHUNKED)) {
		dev_info(pd->dev, "%s, unchunked ext_msg unsupported\n", __func__);
		goto done;
	}

	ext_msg_data_size = sprd_pd_ext_header_data_size_le(msg->ext_msg.header);
	if (ext_msg_data_size > SPRD_PD_EXT_MAX_CHUNK_DATA) {
		dev_err(pd->dev, "%s, chunked ext_msg too long, data_size=%d\n",
			__func__, ext_msg_data_size);
		goto done;
	}

	/*
	 * According to the datasheet, sc27xx_pd_rx_buf is 16bit,
	 * but PD protocol source code msg->ext_msg.data is 8bit.
	 */
	for (i = 0; i < (ext_msg_data_size + 1) / 2; i++) {
		ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_RX_BUF,
				(u32 *)&data[i]);
		if (ret < 0) {
			dev_err(pd->dev, "%s, failed to read chunked ext_msg data, ret = %d\n",
				__func__, ret);
			return ret;
		}

		msg->ext_msg.data[2 * i] = data[i] & SC27XX_CHUNKED_EXT_MSG_MASK;
		if (2 * i + 1 < ext_msg_data_size)
			msg->ext_msg.data[2 * i + 1] = (data[i] >> SC27XX_CHUNKED_EXT_MSG_SHIFT) &
						       SC27XX_CHUNKED_EXT_MSG_MASK;
	}

done:
	return sc27xx_pd_rx_flush(pd);
}

static int sc27xx_pd_read_message(struct sc27xx_pd *pd, struct sprd_pd_message *msg)
{
	int ret, i, rx_fifo_data_num;
	u32 data[SPRD_PD_MAX_PAYLOAD * 2] = {0};
	u32 data_obj_num, spec, reg_val = 0, header = 0, type;
	bool vendor_define = false, source_capabilities = false;
	bool data_request = false, data_alert = false, ext_status = false;
	bool is_ext_msg;

	ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_RX_BUF,
			  &header);
	if (ret < 0) {
		sprd_pd_log(pd, "read header failed, ret = %d", ret);
		return ret;
	}

	ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_STS1, &reg_val);
	if (ret < 0) {
		sprd_pd_log(pd, "read sts1 failed, ret = %d", ret);
		return ret;
	}

	sprd_pd_log(pd, "sts1 = 0x%x, header = 0x%x", reg_val, header);
	if (pd->need_retry) {
		pd->need_retry = false;
		cancel_delayed_work(&pd->read_msg_work);
		sprd_pd_log(pd, "cancel retry read msg done");
	}
	rx_fifo_data_num = reg_val & SC27XX_PD_RX_DATA_NUM_MASK;

	header &= SC27XX_TX_RX_BUF_MASK;
	msg->header = cpu_to_le16(header);
	data_obj_num = sprd_pd_header_cnt_le(msg->header);
	spec = sprd_pd_header_rev_le(msg->header);
	type = sprd_pd_header_type_le(msg->header);
	is_ext_msg = le16_to_cpu(msg->header) & SPRD_PD_HEADER_EXT_HDR;

	if (is_ext_msg && (type == SPRD_PD_EXT_STATUS))
		ext_status = true;
	else if (is_ext_msg)
		vendor_define = false;
	else if (data_obj_num && (type == SPRD_PD_DATA_VENDOR_DEF))
		vendor_define = true;
	else if (data_obj_num && (type == SPRD_PD_DATA_SOURCE_CAP))
		source_capabilities = true;
	else if (data_obj_num && (type == SPRD_PD_DATA_REQUEST))
		data_request = true;
	else if (data_obj_num && (type == SPRD_PD_DATA_ALERT))
		data_alert = true;

	sprd_pd_log(pd, "rx fifo data num = %d, msg header = 0x%x, type = %d, spec = %d",
		    rx_fifo_data_num, msg->header, type, spec);
	if ((data_obj_num * 2 + 1) < rx_fifo_data_num && !vendor_define &&
	    !source_capabilities && !data_request && !data_alert && !ext_status) {
		sprd_pd_log(pd, "retry read msg");
		pd->need_retry = true;
		queue_delayed_work(pd->pd_wq,
				   &pd->read_msg_work,
				   msecs_to_jiffies(5));
	}

	if (spec == 1) {
		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG1,
					 SC27XX_PD_RETRY_MASK,
					 SC27XX_PD_RETRY(3));
		if (ret < 0)
			return ret;
	} else if (spec == 2) {
		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG1,
					 SC27XX_PD_RETRY_MASK,
					 SC27XX_PD_RETRY(1));
		if (ret < 0)
			return ret;
	}

	if (data_obj_num &&
	    sprd_pd_header_type_le(msg->header) == SPRD_PD_DATA_VENDOR_DEF)
		pd->msg_flag = 1;
	else
		pd->msg_flag = 0;

	if (data_obj_num > SPRD_PD_MAX_PAYLOAD) {
		sprd_pd_log(pd, "pd msg too long, num=%d", data_obj_num);
		dev_err(pd->dev, "%s, pd msg too long, num=%d\n", __func__, data_obj_num);
		return -EINVAL;
	}

	if (!is_ext_msg) {
		for (i = 0; i < data_obj_num * 2; i++) {
			ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_RX_BUF,
					  (u32 *)&data[i]);
			if (ret < 0) {
				sprd_pd_log(pd, "%s, failed to read msg data, ret = %d", ret);
				dev_err(pd->dev, "%s, failed to read msg data, ret = %d\n",
					__func__, ret);
				return ret;
			}
			sprd_pd_log(pd, "rx msg: data[%d] = 0x%x", i, data[i]);
		}

		/*
		 * According to the datasheet, sc27xx_pd_rx_buf is 16bit,
		 * but PD protocol source code msg->payload is 32bit,
		 * so need two 16bit assignment one 32bit.
		 */
		for (i = 0; i < data_obj_num; i++)
			msg->payload[i] = cpu_to_le32(data[2 * i + 1] << 16 |
					data[2 * i]);
	} else {
		ret = sc27xx_pd_read_ext_message(pd, msg);
		if (ret < 0) {
			dev_err(pd->dev, "%s, not read pd ext_msg, ret=%d\n", __func__, ret);
			return ret;
		}

		sprd_tcpm_pd_receive(pd->sprd_tcpm_port, msg);
		return 0;
	}

	if (!data_obj_num &&
	    sprd_pd_header_type_le(msg->header) == SPRD_PD_CTRL_GOOD_CRC) {
		if (!pd->constructed) {
			ret = regmap_update_bits(pd->regmap, pd->typec_base +
						 SC27XX_TYPC_PD_CFG,
						 SC27XX_TYPEC_PD_CONSTRACT,
						 SC27XX_TYPEC_PD_CONSTRACT);
			if (ret < 0)
				return ret;
			pd->constructed = true;
		}
		sprd_tcpm_pd_transmit_complete(pd->sprd_tcpm_port, SPRD_TCPC_TX_SUCCESS);
	} else {
		sprd_tcpm_pd_receive(pd->sprd_tcpm_port, msg);
	}

	return sc27xx_pd_rx_flush(pd);
}

static int sc27xx_pd_rc_ref_cal(struct sc27xx_pd *pd)
{
	u32 vol = 0;
	u32 pd_ref = 0;
	u32 val = 0;
	u32 cfg2_bit7, cfg0_vth = 0, cfg0_vtl = 0, typec_ibis = 0;
	u32 cfg0, cfg2, cfg0_mask, cfg2_mask;
	int ret;

	vol = (pd->rc_cal >> SC27XX_PD_SHIFT(pd->var_data->efuse_rc_shift)) & 0x7f;
	pd_ref = (pd->ref_cal >> SC27XX_PD_SHIFT(pd->var_data->efuse_ref_shift)) & 0x7;

	/*
	 * According to the datasheet, depending on the calibration
	 * voltage, different register values should be configured.
	 */

	if (vol >= 0 && vol <= 26)
		val = 0x0;
	if (vol >= 27 && vol <= 37)
		val = 0x1;
	if (vol >= 38 && vol <= 48)
		val = 0x2;
	if (vol >= 49 && vol <= 59)
		val = 0x3;
	if (vol >= 60 && vol <= 69)
		val = 0x4;
	if (vol >= 70 && vol <= 80)
		val = 0x5;
	if (vol >= 81 && vol <= 90)
		val = 0x6;
	if (vol >= 91 && vol <= 127)
		val = 0x7;

	switch (pd_ref) {
	case 0:
		cfg2_bit7 = 0;
		cfg0_vth = 0;
		cfg0_vtl = 2;
		typec_ibis = 0;
		break;
	case 1:
		cfg2_bit7 = 0;
		cfg0_vth = 1;
		cfg0_vtl = 1;
		typec_ibis = 1;
		break;
	case 2:
		cfg2_bit7 = 0;
		cfg0_vth = 1;
		cfg0_vtl = 1;
		typec_ibis = 0;
		break;
	case 3:
		cfg2_bit7 = 1;
		cfg0_vth = 1;
		cfg0_vtl = 1;
		typec_ibis = 0;
		break;
	case 4:
		cfg2_bit7 = 0;
		cfg0_vth = 2;
		cfg0_vtl = 0;
		typec_ibis = 0;
		break;
	}

	if (typec_ibis == 1)
		ret = regmap_update_bits(pd->regmap,
					 pd->typec_base + SC27XX_TYPEC_IBIAS,
					 SC27XX_TYPEC_RX_REF_DECREASE,
					 SC27XX_TYPEC_RX_REF_DECREASE);
	else
		ret = regmap_update_bits(pd->regmap,
					 pd->typec_base + SC27XX_TYPEC_IBIAS,
					 SC27XX_TYPEC_RX_REF_DECREASE, 0);

	cfg0 = (cfg0_vtl << SC27XX_PD_CFG0_VTL_SHIFT) & SC27XX_PD_CFG0_VTL_MASK;
	cfg0 |= (val << SC27XX_PD_CFG0_RCCAL_SHIFT) & SC27XX_PD_CFG0_RCCAL_MASK;
	cfg0 |= cfg0_vth & SC27XX_PD_CFG0_VTH_MASK;
	cfg0_mask = SC27XX_PD_CFG0_RCCAL_MASK |
		    SC27XX_PD_CFG0_VTL_MASK | SC27XX_PD_CFG0_VTH_MASK;

	ret = regmap_update_bits(pd->regmap,
				 pd->base + SC27XX_PD_PHY_CFG0,
				 cfg0_mask, cfg0);
	if (ret < 0)
		return ret;

	cfg2 = (cfg2_bit7 << SC27XX_PD_CFG2_RX_REF_CAL_SHIFT) & 0x80;
	cfg2 |= SC27XX_PD_CFG2_PD_CLK_BIT;
	cfg2_mask = SC27XX_PD_CFG2_PD_CLK_BIT | SC27XX_PD_CFG2_RX_REF_CAL_BIT;

	return regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_PHY_CFG2,
				  cfg2_mask, cfg2);
}

static int sc27xx_pd_delta_cal(struct sc27xx_pd *pd)
{
	u32 delta_cal = pd->delta_cal;
	u32 vol, vref_sel = 0, ref_cal = 0, delta = 0;
	u32 cfg1, cfg1_mask;

	cfg1_mask = SC27XX_PD_CC1_SW | SC27XX_PD_CC2_SW |
		    SC27XX_PD_CFG1_VREF_SEL_MASK |
		    SC27XX_PD_CFG1_REF_CAL_MASK;

	delta = ((delta_cal >> SC27XX_PD_SHIFT(pd->var_data->efuse_delta_shift)) & 0x7f);
	/*
	 * According to the datasheet, delta is efuse caliration
	 * vol = delta * 2 + 1000
	 */
	vol = delta * 2 + 1000;

	/*
	 * According to the datasheet, depending on the calibration
	 * voltage, different register values should be configured.
	 */

	if (vol >= 1185 && vol <= 1195) {
		vref_sel = 0x0;
		ref_cal = 0x0;
	} else if (vol >= 1175 && vol < 1185) {
		vref_sel = 0x0;
		ref_cal = 0x1;
	} else if (vol >= 1165 && vol < 1175) {
		vref_sel = 0x0;
		ref_cal = 0x2;
	} else if (vol >= 1155 && vol < 1165) {
		vref_sel = 0x0;
		ref_cal = 0x3;
	} else if (vol >= 1145 && vol < 1155) {
		vref_sel = 0x0;
		ref_cal = 0x4;
	} else if (vol >= 1135 && vol < 1145) {
		vref_sel = 0x0;
		ref_cal = 0x5;
	} else if (vol >= 1125 && vol < 1135) {
		vref_sel = 0x0;
		ref_cal = 0x6;
	} else if (vol >= 1115 && vol < 1125) {
		vref_sel = 0x1;
		ref_cal = 0x2;
	} else if (vol >= 1105 && vol < 1115) {
		vref_sel = 0x1;
		ref_cal = 0x3;
	} else if (vol >= 1095 && vol < 1105) {
		vref_sel = 0x1;
		ref_cal = 0x4;
	} else if (vol >= 1085 && vol < 1095) {
		vref_sel = 0x1;
		ref_cal = 0x5;
	} else if (vol >= 1075 && vol < 1085) {
		vref_sel = 0x1;
		ref_cal = 0x6;
	} else if (vol >= 1065 && vol < 1075) {
		vref_sel = 0x2;
		ref_cal = 0x2;
	} else if (vol >= 1055 && vol < 1065) {
		vref_sel = 0x2;
		ref_cal = 0x3;
	} else if (vol >= 1045 && vol < 1055) {
		vref_sel = 0x2;
		ref_cal = 0x4;
	} else if (vol >= 1035 && vol < 1045) {
		vref_sel = 0x2;
		ref_cal = 05;
	} else if (vol >= 1025 && vol < 1035) {
		vref_sel = 0x2;
		ref_cal = 0x6;
	} else if (vol >= 1015 && vol < 1025) {
		vref_sel = 0x3;
		ref_cal = 0x2;
	} else if (vol >= 1005 && vol < 1015) {
		vref_sel = 0x3;
		ref_cal = 0x3;
	} else if (vol >= 1000 && vol < 1005) {
		vref_sel = 0x3;
		ref_cal = 0x4;
	}

	cfg1 = SC27XX_PD_CC1_SW | SC27XX_PD_CC2_SW;
	cfg1 |= (vref_sel << SC27XX_PD_CFG1_VREF_SEL_SHIFT)
		& SC27XX_PD_CFG1_VREF_SEL_MASK;
	cfg1 |= (ref_cal << SC27XX_PD_CFG1_REF_CAL_SHIFT)
		& SC27XX_PD_CFG1_REF_CAL_MASK;

	return regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_PHY_CFG1,
				  cfg1_mask, cfg1);
}

static int sc27xx_pd_module_init(struct sc27xx_pd *pd)
{
	int ret;
	u32 mask2 = SC27XX_PD_RX_AUTO_GOOD_CRC;

	ret = sc27xx_pd_delta_cal(pd);
	if (ret < 0)
		return ret;

	ret = sc27xx_pd_rc_ref_cal(pd);
	if (ret < 0)
		return ret;

	ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG1,
				 SC27XX_PD_PHY_13M | SC27XX_PD_RETRY(3),
				 SC27XX_PD_PHY_13M | SC27XX_PD_RETRY(3));
	if (ret < 0)
		return ret;
	ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_MESG_ID_CFG,
				 SC27XX_PD_MESS_ID_MASK, 0x0);
	if (ret < 0)
		return ret;

	ret = regmap_update_bits(pd->regmap,
				 pd->base + SC27XX_PD_CFG0,
				 SC27XX_PD_BIST_MODE_EN,
				 SC27XX_PD_BIST_MODE_EN);
	if (ret < 0)
		return ret;

	ret = regmap_write(pd->regmap, pd->base + SC27XX_INT_CLR,
			   SC27XX_INT_CLR_MASK);
	if (ret < 0)
		return ret;

	ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG1,
				 mask2, ~mask2);
	if (ret < 0)
		return ret;

	return regmap_write(pd->regmap, pd->base + SC27XX_INT_EN,
			    SC27XX_INT_EN_MASK);
}

static int sc27xx_pd_init(struct tcpc_dev *tcpc)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret;

	ret = sc27xx_pd_clk_cfg(pd);
	if (ret)
		return ret;

	return sc27xx_pd_module_init(pd);
}

static irqreturn_t sc27xx_pd_irq(int irq, void *dev_id)
{
	struct sc27xx_pd *pd = dev_id;
	struct sprd_pd_message pd_msg;
	u32 int_sts = 0, pd_sts1 = 0;
	int ret, state;

	sprd_pd_log(pd, "pd irq: start handle irq, irq = %d", irq);
	mutex_lock(&pd->lock);
	ret = regmap_read(pd->regmap, pd->base + SC27XX_INT_FLG, &int_sts);
	if (ret < 0) {
		sprd_pd_log(pd, "pd irq: read int sts fail, ret = %d", ret);
		goto done;
	}

	sprd_pd_log(pd, "pd irq: int sts = 0x%x", int_sts);
	ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_INT_CLR,
				 SC27XX_INT_CLR_MASK,
				 SC27XX_INT_CLR_MASK);
	if (ret < 0) {
		dev_err(pd->dev, "failed to clr int mask, int_sts = %d, ret = %d\n", int_sts, ret);
		sprd_pd_log(pd, "failed to clr int mask, int_sts = %d, ret = %d", int_sts, ret);
		goto done;
	}

	if (pd->shutdown_flag) {
		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_INT_CLR,
					 SC27XX_INT_CLR_MASK,
					 SC27XX_INT_CLR_MASK);
		dev_info(pd->dev, "SC27XX_INT_FLG(0x28)=0x%x, ret=%d ->: return!!!\n", int_sts, ret);
		goto done;
	}

	if (int_sts & SC27XX_PD_HARD_RST_FLAG) {
		dev_warn(pd->dev, "IRQ: PD received hardreset\n");
		sprd_pd_log(pd, "pd irq: receive hard reset");
		if (pd->need_retry) {
			pd->need_retry = false;
			cancel_delayed_work(&pd->read_msg_work);
			sprd_pd_log(pd, "cancel retry read msg done");
		}

		ret = sc27xx_pd_reset(pd);
		if (ret < 0) {
			dev_err(pd->dev, "cannot PD reset, ret=%d\n", ret);
			goto done;
		}

		state = extcon_get_state(pd->edev, EXTCON_CHG_USB_PD);
		if (state == true)
			extcon_set_state_sync(pd->edev, EXTCON_CHG_USB_PD, false);
		else if (state == false)
			extcon_set_state_sync(pd->edev, EXTCON_CHG_USB_PD, true);

		sprd_tcpm_pd_hard_reset(pd->sprd_tcpm_port);
	}

	if (int_sts & SC27XX_PD_CABLE_RST_FLAG) {
		sprd_pd_log(pd, "pd irq: cable reset flag");
		dev_warn(pd->dev, "IRQ: PD cable rst flag\n");
	}

	if (int_sts & SC27XX_PD_SOFT_RST_FLAG) {
		sprd_pd_log(pd, "pd irq: soft reset flag");
		ret = sc27xx_pd_reset(pd);
		if (ret < 0) {
			dev_err(pd->dev, "cannot PD reset, ret=%d\n", ret);
			goto done;
		}
	}

	if ((int_sts & SC27XX_PD_PKG_RV_FLAG)) {
		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_INT_CLR,
					 SC27XX_PD_PKG_RV_CLR,
					 SC27XX_PD_PKG_RV_CLR);
		if (ret < 0)
			goto done;

	       /*
		* According to the requirements of ASIC spec, after receiving
		* the interrupt,the data should be read after 500us.
		*/
		udelay(500);
		ret = regmap_read_poll_timeout(pd->regmap,
					       pd->base + SC27XX_PD_STS1,
					       pd_sts1,
					       (pd_sts1 & (~SC27XX_PD_RX_EMPTY)),
					       SC27XX_PD_POLL_RAW_STATUS,
					       SC27XX_PD_RDY_TIMEOUT);
		if (ret < 0) {
			sprd_pd_log(pd, "failed to read rx status, ret = %d", ret);
			dev_err(pd->dev, "failed to read rx status, ret = %d\n", ret);
			goto done;
		}

		ret = sc27xx_pd_read_message(pd, &pd_msg);
		if (ret < 0) {
			sprd_pd_log(pd, "not read PD msg, ret=%d", ret);
			dev_err(pd->dev, "not read PD msg, ret=%d\n", ret);
			goto done;
		}
	}

	if (int_sts & SC27XX_PD_TX_OK_FLAG)
		sprd_tcpm_pd_transmit_complete(pd->sprd_tcpm_port, SPRD_TCPC_TX_SUCCESS);

	if (int_sts & SC27XX_PD_TX_ERROR_FLAG) {
		sprd_pd_log(pd, "pd irq: tx error failed");
		dev_err(pd->dev, "IRQ: tx error failed\n");
		sprd_tcpm_pd_transmit_complete(pd->sprd_tcpm_port, SPRD_TCPC_TX_FAILED);
	}

	if (int_sts & SC27XX_PD_TX_COLLSION_FLAG) {
		sprd_pd_log(pd, "pd irq: PD collision");
		dev_err(pd->dev, "IRQ: PD collision\n");

		sprd_tcpm_pd_transmit_complete(pd->sprd_tcpm_port, SPRD_TCPC_TX_FAILED);
	}

	if (int_sts & SC27XX_PD_PKG_RV_ERROR_FLAG) {
		sprd_pd_log(pd, "pd irq: PD rx error flag");
		dev_err(pd->dev, "IRQ: PD rx error flag\n");
	}

	if (int_sts & SC27XX_PD_FRS_RV_FLAG)
		sprd_pd_log(pd, "pd irq: SC27XX_PD_FRS_RV_FLAG");

	if (int_sts & SC27XX_PD_RX_FIFO_OVERFLOW_FLAG) {
		sprd_pd_log(pd, "pd irq: PD rx fifo overflow flag");
		dev_err(pd->dev, "IRQ: PD rx fifo overflow flag\n");
	}

done:
	mutex_unlock(&pd->lock);
	sprd_pd_log(pd, "pd irq: IRQ_HANDLED");

	return IRQ_HANDLED;
}

static int sc27xx_get_vbus_status(struct sc27xx_pd *pd)
{
	u32 status = 0;
	bool vbus_present;
	int ret;

	if (pd->typec_online) {
		ret = regmap_read(pd->regmap, pd->typec_base +
				  SC27XX_TYPEC_DBG1, &status);
		if (ret < 0) {
			sprd_pd_log(pd, "%s, failed to read typec_reg[0x%x], ret=%d",
				    __func__, SC27XX_TYPEC_DBG1, ret);
			return ret;
		}
	} else {
		/* purpose: wait for vbus to step down */
		ret = regmap_read_poll_timeout(pd->regmap,
					       pd->base + SC27XX_TYPEC_DBG1,
					       status,
					       (!(status & SC27XX_TYPEC_VBUS_OK)),
					       SC27XX_TYPEC_VBUS_OK_STATUS,
					       SC27XX_TYPEC_VBUS_OK_TIMEOUT);
		if (ret < 0) {
			sprd_pd_log(pd, "%s, failed to read typec_reg[0x%x], ret=%d",
				    __func__, SC27XX_TYPEC_DBG1, ret);
			pd->vbus_present = false;
			sprd_tcpm_vbus_change(pd->sprd_tcpm_port);
			return ret;
		}
	}

	vbus_present = !!(status & SC27XX_TYPEC_VBUS_OK);
	if (vbus_present != pd->vbus_present) {
		pd->vbus_present = vbus_present;
		sprd_tcpm_vbus_change(pd->sprd_tcpm_port);
	}

	return 0;
}

static void sc27xx_cc_polarity_status(struct sc27xx_pd *pd, u32 status)
{
	if (status & SC27XX_TYPEC_FINAL_SWITCH)
		pd->cc_polarity = SPRD_TYPEC_POLARITY_CC1;
	else
		pd->cc_polarity = SPRD_TYPEC_POLARITY_CC2;
}

static void sc27xx_cc_status(struct sc27xx_pd *pd, u32 status)
{
	u32 cc_rp, rp_sts = SC27XX_TYPEC_VBUS_CL(status);

	switch (rp_sts) {
	case 0:
		cc_rp = SPRD_TYPEC_CC_RP_DEF;
		break;
	case 1:
		cc_rp = SPRD_TYPEC_CC_RP_1_5;
		break;
	case 2:
		cc_rp = SPRD_TYPEC_CC_RP_3_0;
		break;
	default:
		cc_rp = SPRD_TYPEC_CC_OPEN;
		break;
	}

	switch (pd->state) {
	case SC27XX_ATTACHED_SNK:
		if (pd->cc_polarity == SPRD_TYPEC_POLARITY_CC1) {
			pd->cc1 = cc_rp;
			pd->cc2 = SPRD_TYPEC_CC_OPEN;
		} else {
			pd->cc1 = SPRD_TYPEC_CC_OPEN;
			pd->cc2 = cc_rp;
		}
		break;

	case SC27XX_ATTACHED_SRC:
		if (pd->cc_polarity == SPRD_TYPEC_POLARITY_CC1) {
			pd->cc1 = SPRD_TYPEC_CC_RD;
			pd->cc2 = SPRD_TYPEC_CC_OPEN;
		} else {
			pd->cc1 = SPRD_TYPEC_CC_OPEN;
			pd->cc2 = SPRD_TYPEC_CC_RD;
		}
		break;

	case SC27XX_POWERED_CABLE:
		if (pd->cc_polarity == SPRD_TYPEC_POLARITY_CC1) {
			pd->cc1 = SPRD_TYPEC_CC_RD;
			pd->cc2 = SPRD_TYPEC_CC_RA;
		} else {
			pd->cc1 = SPRD_TYPEC_CC_RA;
			pd->cc2 = SPRD_TYPEC_CC_RD;
		}
		break;
	default:
		pd->cc1 = SPRD_TYPEC_CC_OPEN;
		pd->cc2 = SPRD_TYPEC_CC_OPEN;
		break;
	}

	sprd_tcpm_cc_change(pd->sprd_tcpm_port);
}

static int sc27xx_pd_check_vbus_cc_status(struct sc27xx_pd *pd)
{
	u32 val = 0;
	int ret;

	ret = regmap_read(pd->regmap, pd->typec_base + SC27XX_TYPEC_STATUS,
			  &val);
	if (ret)
		return ret;

	ret = sc27xx_pd_set_aon_clock(pd, true);
	if (ret)
		return ret;

	ret = sc27xx_pd_clk_cfg(pd);
	if (ret)
		return ret;

	pd->state = val & SC27XX_STATE_MASK;
	if (pd->state == SC27XX_ATTACHED_SNK || pd->state == SC27XX_ATTACHED_SRC) {
		sprd_pd_log(pd, "typec plug in");
		pd->typec_online = true;
	} else {
		sprd_pd_log(pd, "typec plug out");
		pd->typec_online = false;
	}

	sc27xx_cc_polarity_status(pd, val);
	sc27xx_cc_status(pd, val);
	sc27xx_get_vbus_status(pd);

	return 0;
}

static int sc27xx_pd_extcon_event(struct notifier_block *nb,
				  unsigned long event, void *param)
{
	struct sc27xx_pd *pd = container_of(nb, struct sc27xx_pd, extcon_nb);
#if IS_ENABLED(CONFIG_SPRD_TYPEC_DP_ALTMODE)
	union extcon_property_value hpd_status;

	hpd_status.intval = SC27XX_DP_TYPE_DISCONNECT;
	extcon_set_state(pd->edev, EXTCON_DISP_DP, true);
	extcon_set_property(pd->edev, EXTCON_DISP_DP, EXTCON_PROP_DISP_HPD, hpd_status);
	extcon_sync(pd->edev, EXTCON_DISP_DP);
	dev_info(pd->dev, "%s:hpd_status = %d\n", __func__, hpd_status.intval);
#endif
	dev_info(pd->dev, "typec in or out, pd attached = %d\n", pd->pd_attached);
	if (pd->pd_attached)
		schedule_work(&pd->pd_work);
	return NOTIFY_OK;
}

static void sc27xx_pd_work(struct work_struct *work)
{
	struct sc27xx_pd *pd = container_of(work, struct sc27xx_pd, pd_work);
	int ret;

	dev_info(pd->dev, "check vbus and cc\n");
	ret = sc27xx_pd_check_vbus_cc_status(pd);
	if (ret)
		dev_err(pd->dev, "failed to check vbus and cc status\n");
}

#define DP_PIN_ASSIGN_GEN2_BR	(BIT(DP_PIN_ASSIGN_A) | \
					 BIT(DP_PIN_ASSIGN_B))

/* Pin assignments that use DP v1.3 signaling to carry DP protocol */
#define DP_PIN_ASSIGN_DP_BR		(BIT(DP_PIN_ASSIGN_C) | \
					 BIT(DP_PIN_ASSIGN_D) | \
					 BIT(DP_PIN_ASSIGN_E) | \
					 BIT(DP_PIN_ASSIGN_F))
#define DP_CAP_PIN_DFP_ASSIGN(_cap_)	((((_cap_) & GENMASK(7, 0)) << 16) | \
					 (((_cap_) & GENMASK(7, 0)) << 8))

static const struct typec_altmode_desc sc27xx_alt_modes = {
	.svid = USB_TYPEC_DP_SID,
	.mode = USB_TYPEC_DP_MODE,
	.vdo = DP_CAP_CAPABILITY(DP_CAP_DFP_D) | DP_CAP_DP_SIGNALING | \
		DP_CAP_RECEPTACLE | \
		DP_CAP_PIN_DFP_ASSIGN(DP_PIN_ASSIGN_DP_BR),
};

static const struct tcpc_config sc27xx_pd_config = {
	.type = TYPEC_PORT_DRP,
	.default_role = TYPEC_SOURCE,
	.alt_modes = &sc27xx_alt_modes,
	.nr_alt_modes = 1,
};

static void sc27xx_init_tcpc_dev(struct sc27xx_pd *pd)
{
	pd->tcpc.config = &pd->config;
	pd->tcpc.init = sc27xx_pd_init;
	pd->tcpc.get_vbus = sc27xx_pd_get_vbus;
	pd->tcpc.get_current_limit = sc27xx_pd_get_current_limit;
	pd->tcpc.set_cc = sc27xx_pd_set_cc;
	pd->tcpc.get_cc = sc27xx_pd_get_cc;
	pd->tcpc.set_polarity = sc27xx_pd_set_polarity;
	pd->tcpc.set_vconn = sc27xx_pd_set_vconn;
	pd->tcpc.set_vbus = sc27xx_pd_set_vbus;
	pd->tcpc.set_current_limit = sc27xx_pd_set_current_limit;
	pd->tcpc.set_pd_rx = sc27xx_pd_set_rx;
	pd->tcpc.set_roles = sc27xx_pd_set_roles;
	pd->tcpc.start_toggling = sc27xx_pd_start_drp_toggling;
	pd->tcpc.pd_transmit = sc27xx_pd_transmit;
	pd->tcpc.dp_altmode_notify = sc27xx_pd_dp_altmode_notify;
}

static int sc27xx_pd_efuse_read(struct sc27xx_pd *pd,
				const char *cell_id, u32 *val)
{
	struct nvmem_cell *cell;
	void *buf;
	size_t len = 0;

	cell = nvmem_cell_get(pd->dev, cell_id);
	if (IS_ERR_OR_NULL(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(val, buf, min(len, sizeof(u16)));

	kfree(buf);
	return 0;
}

static int sc27xx_pd_cal(struct sc27xx_pd *pd)
{
	int ret;

	ret = sc27xx_pd_efuse_read(pd, "pdrc_calib", &pd->rc_cal);
	if (ret)
		return ret;

	ret = sc27xx_pd_efuse_read(pd, "pddelta_calib", &pd->delta_cal);
	if (ret)
		return ret;

	if (pd->var_data->id == PMIC_SC2730) {
		ret = sc27xx_pd_efuse_read(pd, "pdref_calib", &pd->ref_cal);
		if (ret)
			return ret;
	} else if (pd->var_data->id == PMIC_UMP9620) {
		/*
		 * ump9620 pdref_calib use the same block
		 * with pddelta_calib--block 36.
		 */
		ret = sc27xx_pd_efuse_read(pd, "pddelta_calib", &pd->ref_cal);
		if (ret)
			return ret;
	}

	return 0;
}

static void sc27xx_pd_read_msg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sc27xx_pd *pd = container_of(dwork, struct sc27xx_pd,
					   read_msg_work);
	struct sprd_pd_message pd_msg;

	mutex_lock(&pd->lock);

	if (!pd->need_retry)
		goto out;

	pd->need_retry = false;

	sc27xx_pd_read_message(pd, &pd_msg);

out:
	mutex_unlock(&pd->lock);
}

static void sc27xx_pd_detect_typec_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sc27xx_pd *pd = container_of(dwork, struct sc27xx_pd,
					   typec_detect_work);

	dev_info(pd->dev, "pd try to detect typec extcon\n");
	if (extcon_get_state(pd->extcon, EXTCON_USB) ||
	    extcon_get_state(pd->extcon, EXTCON_USB_HOST))
		sc27xx_pd_check_vbus_cc_status(pd);

#if IS_ENABLED(CONFIG_SPRD_TYPEC_DP_ALTMODE)
	extcon_set_property_capability(pd->edev, EXTCON_DISP_DP, EXTCON_PROP_DISP_HPD);
#endif

	pd->pd_attached = true;
}

static const u32 sc27xx_pd_hardreset[] = {
#if IS_ENABLED(CONFIG_SPRD_TYPEC_DP_ALTMODE)
	EXTCON_DISP_DP,
#endif
	EXTCON_CHG_USB_PD,
	EXTCON_NONE,
};

static int sc27xx_pd_probe(struct platform_device *pdev)
{
	struct sc27xx_pd *pd;
	const struct sc27xx_pd_variant_data *pdata;
	int pd_irq, ret;

	pdata = of_device_get_match_data(&pdev->dev);
	if (!pdata) {
		dev_err(&pdev->dev, "No matching driver data found\n");
		return -EINVAL;
	}

	pd = devm_kzalloc(&pdev->dev, sizeof(*pd), GFP_KERNEL);
	if (!pd) {
		dev_err(&pdev->dev, "failed to alloc memory\n");
		return -ENOMEM;
	}

	pd->dev = &pdev->dev;
	sprd_pd_tcpm_log = sprd_tcpm_log_do_outside;
	pd->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!pd->regmap) {
		dev_err(&pdev->dev, "failed to get pd regmap\n");
		return -ENODEV;
	}

	pd->edev = devm_extcon_dev_allocate(&pdev->dev, sc27xx_pd_hardreset);
	if (IS_ERR(pd->edev)) {
		dev_err(&pdev->dev, "failed to allocate extcon device\n");
		return PTR_ERR(pd->edev);
	}

	ret = devm_extcon_dev_register(&pdev->dev, pd->edev);
	if (ret < 0) {
		dev_err(&pdev->dev, "can't register extcon device: %d\n", ret);
		return ret;
	}

	if (of_property_read_bool(pdev->dev.of_node, "extcon")) {
		pd->extcon = extcon_get_edev_by_phandle(&pdev->dev, 0);
		if (IS_ERR(pd->extcon)) {
			dev_err(&pdev->dev, "failed to find extcon device.\n");
			return PTR_ERR(pd->extcon);
		}

		pd->extcon_nb.notifier_call = sc27xx_pd_extcon_event;
		ret = devm_extcon_register_notifier_all(&pdev->dev,
							pd->extcon,
							&pd->extcon_nb);
		if (ret) {
			dev_err(&pdev->dev, "Can't register extcon\n");
			return ret;
		}
	}

	ret = of_property_read_u32_index(pdev->dev.of_node, "reg", 0,
					&pd->base);
	if (ret) {
		dev_err(&pdev->dev, "failed to get pd reg address\n");
		return ret;
	}

	ret = of_property_read_u32_index(pdev->dev.of_node, "reg", 1,
					&pd->typec_base);
	if (ret) {
		dev_err(&pdev->dev, "failed to get typec reg address\n");
		return ret;
	}

	pd_irq = platform_get_irq(pdev, 0);
	if (pd_irq < 0) {
		dev_err(&pdev->dev, "failed to get pd irq number\n");
		return pd_irq;
	}

	mutex_init(&pd->lock);
	pd->var_data = pdata;
	pd->vbus_present = false;
	pd->constructed = false;
	pd->config = sc27xx_pd_config;
	pd->tcpc.config = &pd->config;
	pd->tcpc.fwnode = device_get_named_child_node(&pdev->dev, "connector");

	ret = sc27xx_pd_cal(pd);
	if (ret) {
		dev_err(&pdev->dev, "failed to calibrate with efuse data\n");
		return ret;
	}
	sc27xx_init_tcpc_dev(pd);

	pd->vbus = devm_regulator_get(pd->dev, "vbus");
	if (IS_ERR(pd->vbus)) {
		dev_err(&pdev->dev, "pd failed to get vbus\n");
		return PTR_ERR(pd->vbus);
	}

	pd->vconn = devm_regulator_get_optional(pd->dev, "vconn");
	if (IS_ERR(pd->vconn)) {
		ret = PTR_ERR(pd->vconn);
		if (ret == -ENODEV) {
			dev_warn(pd->dev, "unable to get vddldo supply\n");
			pd->vconn = NULL;
		} else {
			dev_err(pd->dev, "failed to get vddldo supply\n");
			return ret;
		}
	}

	if (of_property_read_bool(pdev->dev.of_node, "sprd,syscon-aon-apb")) {
		pd->aon_apb = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
							      "sprd,syscon-aon-apb");
		if (IS_ERR(pd->aon_apb)) {
			dev_err(pd->dev, "failed to map aon registers (via syscon)\n");
			return PTR_ERR(pd->aon_apb);
		}
	} else {
		pd->aon_apb = NULL;
	}

	ret = devm_request_threaded_irq(pd->dev, pd_irq, NULL,
					sc27xx_pd_irq,
					IRQF_ONESHOT | IRQF_TRIGGER_LOW,
					"sc27xx_pd", pd);
	if (ret < 0) {
		dev_err(pd->dev, "failed to request irq\n");
		return ret;
	}

	pd->pd_wq = create_singlethread_workqueue("sprd_pd_driver");
	if (!pd->pd_wq) {
		dev_err(pd->dev, "failed to create singlethread workqueue\n");
		return -ENOMEM;
	}

	pd->sprd_tcpm_port = sprd_tcpm_register_port(pd->dev, &pd->tcpc);
	if (IS_ERR(pd->sprd_tcpm_port)) {
		dev_err(pd->dev, "failed to register tcpm port\n");
		destroy_workqueue(pd->pd_wq);
		return PTR_ERR(pd->sprd_tcpm_port);
	}

	INIT_DELAYED_WORK(&pd->typec_detect_work, sc27xx_pd_detect_typec_work);
	INIT_DELAYED_WORK(&pd->read_msg_work, sc27xx_pd_read_msg_work);
	INIT_WORK(&pd->pd_work, sc27xx_pd_work);

	platform_set_drvdata(pdev, pd);
	schedule_delayed_work(&pd->typec_detect_work,
			      msecs_to_jiffies(SC27xx_DETECT_TYPEC_DELAY));

	return 0;
}

static int sc27xx_pd_remove(struct platform_device *pdev)
{
	struct sc27xx_pd *pd = platform_get_drvdata(pdev);

	sprd_tcpm_unregister_port(pd->sprd_tcpm_port);
	return 0;
}

static void sc27xx_pd_shutdown(struct platform_device *pdev)
{
	int ret;

	struct sc27xx_pd *pd = platform_get_drvdata(pdev);

	if (!pd->sprd_tcpm_port) {
		dev_warn(pd->dev, "sprd_tcpm_port Null!!!\n");
		return;
	}

	sprd_tcpm_shutdown(pd->sprd_tcpm_port);

	ret = sc27xx_pd_set_rx(&pd->tcpc, false);
	if (ret) {
		dev_err(pd->dev, "failed to disable set_rx at shutdown, ret = %d\n", ret);
		return;
	}

	pd->shutdown_flag = true;

	cancel_delayed_work_sync(&pd->read_msg_work);
	cancel_work_sync(&pd->pd_work);
}

#ifdef CONFIG_PM_SLEEP
static int sc27xx_pd_suspend(struct device *dev)
{
	struct sc27xx_pd *pd = dev_get_drvdata(dev);
	int ret;

	if (pd->typec_online)
		return 0;

	dev_info(pd->dev, "typec offline, disable pd clock when suspend\n");
	ret = sc27xx_pd_disable_clk(pd);
	if (ret) {
		dev_err(pd->dev, "failed to disable pd clock when suspend, ret = %d\n", ret);
		return ret;
	}

	ret = sc27xx_pd_set_aon_clock(pd, false);
	if (ret) {
		dev_err(pd->dev, "failed to disable aon pd clock when suspend, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int sc27xx_pd_resume(struct device *dev)
{
	struct sc27xx_pd *pd = dev_get_drvdata(dev);
	int ret;

	ret = sc27xx_pd_set_aon_clock(pd, true);
	if (ret) {
		dev_err(pd->dev, "failed to enable aon pd clock when suspend, ret = %d\n", ret);
		return ret;
	}

	ret = sc27xx_pd_clk_cfg(pd);
	if (ret) {
		dev_err(pd->dev, "failed to enable pd clock when resume, ret = %d\n", ret);
		return ret;
	}

	return 0;
}
#endif

static const struct dev_pm_ops sc27xx_pd_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sc27xx_pd_suspend, sc27xx_pd_resume)
};

static const struct of_device_id sc27xx_pd_of_match[] = {
	{.compatible = "sprd,sc2730-pd", .data = &sc2730_data},
	{.compatible = "sprd,ump9620-pd", .data = &ump9620_data},
	{}
};

static struct platform_driver sc27xx_pd_driver = {
	.probe = sc27xx_pd_probe,
	.remove = sc27xx_pd_remove,
	.shutdown = sc27xx_pd_shutdown,
	.driver = {
		.name = "sc27xx-typec-pd",
		.of_match_table = sc27xx_pd_of_match,
		.pm = &sc27xx_pd_pm_ops,
	},
};

module_platform_driver(sc27xx_pd_driver);

MODULE_AUTHOR("Freeman Liu <freeman.liu@spreadtrum.com>");
MODULE_DESCRIPTION("Spreadtrum SC27xx typec driver");
MODULE_LICENSE("GPL v2");
