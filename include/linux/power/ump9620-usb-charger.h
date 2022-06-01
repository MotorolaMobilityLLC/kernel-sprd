#ifndef __LINUX_UMP9620_USB_CHARGER_INCLUDED
#define __LINUX_UMP9620_USB_CHARGER_INCLUDED

#include <linux/delay.h>
#include <linux/regmap.h>
#include <uapi/linux/usb/charger.h>

#define UMP9620_CHARGE_STATUS		0x239c
#define BIT_CHG_DET_DONE		BIT(11)
#define BIT_SDP_INT			BIT(7)
#define BIT_DCP_INT			BIT(6)
#define BIT_CDP_INT			BIT(5)
#define BIT_CHGR_INT			BIT(2)

#define UMP9620_CHG_DET_FGU_CTRL	0x23a0
#define UMP9620_CHG_DET_DELAY_MASK	GENMASK(7, 4)
#define UMP9620_CHG_DET_DELAY_OFFSET	4

#define UMP9620_CHG_BC1P2_CTRL2		0x243c
#define UMP9620_CHG_DET_EB_MASK		GENMASK(0, 0)
#define UMP9620_CHG_DET_DELAY_STEP_MS	(64)
#define UMP9620_CHG_DET_DELAY_MS_MAX	(15 * UMP9620_CHG_DET_DELAY_STEP_MS)
#define UMP9620_CHG_BC1P2_REDET_ENABLE	1
#define UMP9620_CHG_BC1P2_REDET_DISABLE 0
#define UMP9620_CHG_DET_RETRY_COUNT	100
#define UMP9620_CHG_DET_DELAY_MS	20

#define UMP9620_ERROR_NO_ERROR		0
#define UMP9620_ERROR_REGMAP_UPDATE	1
#define UMP9620_ERROR_REGMAP_READ	2
#define UMP9620_ERROR_CHARGER_INIT	3
#define UMP9620_ERROR_CHARGER_DETDONE	4

static u32 det_delay_ms;

static int bc1p2_redetect_control(struct regmap *regmap, bool enable)
{
	int ret;

	if (enable)
		ret = regmap_update_bits(regmap, UMP9620_CHG_BC1P2_CTRL2,
					 UMP9620_CHG_DET_EB_MASK,
					 UMP9620_CHG_BC1P2_REDET_ENABLE);
	else
		ret = regmap_update_bits(regmap, UMP9620_CHG_BC1P2_CTRL2,
					 UMP9620_CHG_DET_EB_MASK,
					 UMP9620_CHG_BC1P2_REDET_DISABLE);

	if (ret)
		pr_info("fail to set/clear redetect bit\n");
	return ret;
}

static enum usb_charger_type sc27xx_charger_detect(struct regmap *regmap)
{
	enum usb_charger_type type;
	u32 status = 0, val;
	int ret, cnt = UMP9620_CHG_DET_RETRY_COUNT;

	cnt += det_delay_ms / UMP9620_CHG_DET_DELAY_MS;
	det_delay_ms = 0;
	do {
		ret = regmap_read(regmap, UMP9620_CHARGE_STATUS, &val);
		if (ret) {
			ret = bc1p2_redetect_control(regmap, false);
			return UNKNOWN_TYPE;
		}

		if (!(val & BIT_CHGR_INT) && cnt < UMP9620_CHG_DET_RETRY_COUNT) {
			ret = bc1p2_redetect_control(regmap, false);
			return UNKNOWN_TYPE;
		}

		if (val & BIT_CHG_DET_DONE) {
			status = val & (BIT_CDP_INT | BIT_DCP_INT | BIT_SDP_INT);
			break;
		}

		msleep(UMP9620_CHG_DET_DELAY_MS);
	} while (--cnt > 0);

	switch (status) {
	case BIT_CDP_INT:
		type = CDP_TYPE;
		break;
	case BIT_DCP_INT:
		type = DCP_TYPE;
		break;
	case BIT_SDP_INT:
		type = SDP_TYPE;
		break;
	default:
		type = UNKNOWN_TYPE;
	}

	pr_info("charger_detect type %d\n", type);
	ret = bc1p2_redetect_control(regmap, false);
	return type;
}

static int sc27xx_charger_phy_redetect_trigger(struct regmap *regmap, u32 time_ms)
{
	int ret;
	u32 reg_val;

	if (time_ms > UMP9620_CHG_DET_DELAY_MS_MAX)
		time_ms = UMP9620_CHG_DET_DELAY_MS_MAX;

	reg_val = time_ms / UMP9620_CHG_DET_DELAY_STEP_MS;
	ret = regmap_update_bits(regmap, UMP9620_CHG_DET_FGU_CTRL,
				 UMP9620_CHG_DET_DELAY_MASK,
				 reg_val << UMP9620_CHG_DET_DELAY_OFFSET);
	if (ret)
		return UMP9620_ERROR_REGMAP_UPDATE;

	ret = bc1p2_redetect_control(regmap, true);
	if (ret)
		return UMP9620_ERROR_REGMAP_UPDATE;

	/* sleep 20ms to wait BC1P2 clear CHG_DET_DONE bit */
	msleep(20);

	ret = regmap_read(regmap, UMP9620_CHARGE_STATUS, &reg_val);
	if (ret)
		return UMP9620_ERROR_REGMAP_READ;

	if (!(reg_val & BIT_CHGR_INT))
		return UMP9620_ERROR_CHARGER_INIT;

	if (reg_val & BIT_CHG_DET_DONE)
		return UMP9620_ERROR_CHARGER_DETDONE;

	det_delay_ms = time_ms;
	return UMP9620_ERROR_NO_ERROR;
}

#endif

