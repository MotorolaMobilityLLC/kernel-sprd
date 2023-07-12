/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_SPRD_SPRD_BC1P2_INCLUDED
#define __LINUX_SPRD_SPRD_BC1P2_INCLUDED

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/regmap.h>
#include <linux/usb/phy.h>
#include <uapi/linux/sched/types.h>
#include <uapi/linux/usb/charger.h>

#define SC2720_CHARGE_STATUS	(0xe14)
#define SC2720_CHG_DET_FGU_CTRL	(0xe18)
#define SC2721_CHARGE_STATUS	(0xec8)
#define SC2721_CHG_DET_FGU_CTRL	(0xecc)
#define SC2730_CHARGE_STATUS	(0x1b9c)
#define SC2730_CHG_DET_FGU_CTRL	(0x1ba0)
#define SC2731_CHARGE_STATUS	(0xedc)
#define SC2731_CHG_DET_FGU_CTRL	(0xed8)
#define UMP9620_CHARGE_STATUS	(0x239c)
#define UMP9620_CHG_DET_FGU_CTRL	(0x23a0)
#define UMP9620_CHG_BC1P2_CTRL2	(0x243c)
#define UMP518_CHG_BC1P2_CTRL2	(0x1d24)

/* Pls keep the same definition as musb_sprd */
#define CHARGER_DETECT_DONE		BIT(0)
#define CHARGER_HW_REDETECT_EN		BIT(29)
#define CHARGER_2NDDETECT_ENABLE	BIT(30)
#define CHARGER_2NDDETECT_SELECT	BIT(31)

#define BIT_CHG_DET_DONE		BIT(11)
#define BIT_SDP_INT			BIT(7)
#define BIT_DCP_INT			BIT(6)
#define BIT_CDP_INT			BIT(5)
#define BIT_CHGR_INT			BIT(2)

#define CHG_INT_DELAY_128MS	2
#define SC27XX_CHG_INT_DELAY_MASK	GENMASK(11, 9)
#define SC27XX_CHG_INT_DELAY_OFFSET	9
#define UMP96XX_CHG_INT_DELAY_MASK	GENMASK(11, 8)
#define UMP96XX_CHG_INT_DELAY_OFFSET	8
#define UMP96XX_CHG_REDET_DELAY_MASK	GENMASK(7, 4)
#define UMP96XX_CHG_REDET_DELAY_OFFSET	4

#define UMP96XX_CHG_DET_EB_MASK		GENMASK(0, 0)
#define UMP96XX_CHG_DET_DELAY_STEP_MS	(64)
#define UMP96XX_CHG_DET_DELAY_MS_MAX	(15 * UMP96XX_CHG_DET_DELAY_STEP_MS)
#define UMP96XX_CHG_BC1P2_REDET_ENABLE	1
#define UMP96XX_CHG_BC1P2_REDET_DISABLE 0
#define UMP96XX_CHG_DET_RETRY_COUNT	60
#define UMP96XX_CHG_DET_DELAY_MS	20

#define UMP96XX_ERROR_NO_ERROR		0
#define UMP96XX_ERROR_REGMAP_UPDATE	1
#define UMP96XX_ERROR_REGMAP_READ	2
#define UMP96XX_ERROR_CHARGER_INIT	3
#define UMP96XX_ERROR_CHARGER_DETDONE	4

#define UMP96XX_CHG_REDET_DELAY_MS	960

/* Default current range by charger type. */
#define DEFAULT_SDP_CUR_MIN	2
#define DEFAULT_SDP_CUR_MAX	500
#define DEFAULT_SDP_CUR_MIN_SS	150
#define DEFAULT_SDP_CUR_MAX_SS	900
#define DEFAULT_DCP_CUR_MIN	500
#define DEFAULT_DCP_CUR_MAX	5000
#define DEFAULT_CDP_CUR_MIN	1500
#define DEFAULT_CDP_CUR_MAX	5000
#define DEFAULT_ACA_CUR_MIN	1500
#define DEFAULT_ACA_CUR_MAX	5000

struct sprd_bc1p2_data {
	u32 charge_status;
	u32 chg_det_fgu_ctrl;
	u32 chg_bc1p2_ctrl2;
	u32 chg_int_delay_mask;
	u32 chg_int_delay_offset;
};

struct sprd_bc1p2_priv {
	struct usb_phy *phy;
	struct kthread_worker bc1p2_kworker;
	struct kthread_work bc1p2_kwork;
	struct task_struct *bc1p2_thread;
	spinlock_t vbus_event_lock;
	bool vbus_events;
};

struct sprd_bc1p2 {
	struct mutex bc1p2_lock;
	struct regmap *regmap;
	const struct sprd_bc1p2_data *data;
	enum usb_charger_type type;
	enum usb_charger_state chg_state;
	bool redetect_enable;
};

#if IS_ENABLED(CONFIG_SPRD_BC1P2)
extern void sprd_usb_changed(struct sprd_bc1p2_priv *bc1p2_info, enum usb_charger_state state);
extern int usb_add_bc1p2_init(struct sprd_bc1p2_priv *bc1p2_info, struct usb_phy *x);
extern void usb_remove_bc1p2(struct sprd_bc1p2_priv *bc1p2_info);
extern void usb_phy_notify_charger(struct usb_phy *x);
extern void sprd_bc1p2_notify_charger(struct usb_phy *x);

extern enum usb_charger_type sprd_bc1p2_charger_detect(struct usb_phy *x);
#else
static inline void sprd_usb_changed(struct sprd_bc1p2_priv *bc1p2_info,
				    enum usb_charger_state state) {}
static inline int usb_add_bc1p2_init(struct sprd_bc1p2_priv *bc1p2_info, struct usb_phy *x)
{
	return 0;
}
static inline void usb_remove_bc1p2(struct sprd_bc1p2_priv *bc1p2_info) {}
static inline void usb_phy_notify_charger(struct usb_phy *x) {}
static inline void sprd_bc1p2_notify_charger(struct usb_phy *x) {}

static inline enum usb_charger_type sprd_bc1p2_charger_detect(struct usb_phy *x)
{
	return UNKNOWN_TYPE;
}
#endif

#endif
