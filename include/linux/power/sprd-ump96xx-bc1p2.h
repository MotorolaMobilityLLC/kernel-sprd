/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_SPRD_UMP96XX_BC1P2_INCLUDED
#define __LINUX_SPRD_UMP96XX_BC1P2_INCLUDED

#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>

#define BIT_CHG_DET_DONE		BIT(11)
#define BIT_SDP_INT			BIT(7)
#define BIT_DCP_INT			BIT(6)
#define BIT_CDP_INT			BIT(5)
#define BIT_CHGR_INT			BIT(2)

#define UMP96XX_CHG_DET_DELAY_MASK	GENMASK(7, 4)
#define UMP96XX_CHG_DET_DELAY_OFFSET	4

#define UMP96XX_CHG_DET_EB_MASK		GENMASK(0, 0)
#define UMP96XX_CHG_DET_DELAY_STEP_MS	(64)
#define UMP96XX_CHG_DET_DELAY_MS_MAX	(15 * UMP96XX_CHG_DET_DELAY_STEP_MS)
#define UMP96XX_CHG_BC1P2_REDET_ENABLE	1
#define UMP96XX_CHG_BC1P2_REDET_DISABLE 0
#define UMP96XX_CHG_DET_RETRY_COUNT	50
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

struct ump96xx_bc1p2 {
	struct mutex bc1p2_lock;
	struct regmap *regmap;
	u32 charge_status;
	u32 chg_det_fgu_ctrl;
	u32 chg_bc1p2_ctrl2;
};

#if IS_ENABLED(CONFIG_SPRD_UMP96XX_BC1P2)
extern void sprd_bc1p2_notify_charger(struct usb_phy *x);

extern enum usb_charger_type sprd_bc1p2_charger_detect(struct usb_phy *x);
#else
static inline void sprd_bc1p2_notify_charger(struct usb_phy *x) {}

static inline enum usb_charger_type sprd_bc1p2_charger_detect(struct usb_phy *x)
{
	return UNKNOWN_TYPE;
}
#endif

#endif
