#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include "ats_core.h"
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>

#ifdef TS_USE_ADF_NOTIFIER
#include <video/adf_notifier.h>
#endif

#ifdef CONFIG_DSP_NOTIFIER
#define DRM_MODE_DPMS_ON        0
#define DRM_MODE_DPMS_STANDBY   1
#define DRM_MODE_DPMS_SUSPEND   2
#define DRM_MODE_DPMS_OFF       3
extern int dsp_register_client(struct notifier_block *nb);
extern int dsp_unregister_client(struct notifier_block *nb);
static int dsp_event_handler(struct notifier_block *nb, unsigned long action, void *data);
#endif

/*
 * stores notifier to receive external events
 */
struct ts_event_receiver {
	struct ts_data *pdata;
	event_handler_t inform;
#ifdef TS_USE_ADF_NOTIFIER
	struct notifier_block adf_event_block;
#endif
#ifdef CONFIG_DSP_NOTIFIER
	struct notifier_block dsp_notifier;
#endif
	struct usb_phy *phy;
	struct notifier_block usb_event_block;
};

static struct ts_event_receiver receiver;

#ifdef TS_USE_ADF_NOTIFIER
/*
 * touchscreen's suspend and resume state should rely on screen state,
 * as fb_notifier and early_suspend are all disabled on our platform,
 * we can only use adf_event now
 */
static int ts_adf_event_handler(
	struct notifier_block *nb, unsigned long action, void *data)
{
	struct ts_event_receiver *p = container_of(
		nb, struct ts_event_receiver, adf_event_block);
	struct adf_notifier_event *event = data;
	int adf_event_data;
	if ((action != ADF_EVENT_BLANK) && (action != ADF_EVENT_MODE_SET))
		return NOTIFY_DONE;

	if (action == ADF_EVENT_MODE_SET) {
		p->inform(p->pdata, TSEVENT_SETSURFACE, data);
		return NOTIFY_OK;
	}

	adf_event_data = *(int *)event->data;
	TS_DBG("receive adf event with adf_event_data=%d", adf_event_data);

	switch (adf_event_data) {
	case DRM_MODE_DPMS_ON:
		p->inform(p->pdata, TSEVENT_RESUME, NULL);
		break;
	case DRM_MODE_DPMS_OFF:
		p->inform(p->pdata, TSEVENT_SUSPEND, NULL);
		break;
	default:
		TS_WARN("receive adf event with error data, adf_event_data=%d",
			adf_event_data);
		break;
	}

	return NOTIFY_OK;
}
#endif

#ifdef CONFIG_DSP_NOTIFIER
static int dsp_event_handler(struct notifier_block *nb, unsigned long action, void *data)
{
	struct ts_event_receiver *p = container_of(nb, struct ts_event_receiver, dsp_notifier);
	int dsp_val;

	if (action != 0x10&&action != 0x11) {
		return NOTIFY_DONE;
	}
	if (data) dsp_val = *(int *)data;

	if(action==0x10 && dsp_val==DRM_MODE_DPMS_OFF) {
		TS_INFO("receive adf event with dsp_val=%d", dsp_val);
		p->inform(p->pdata, TSEVENT_SUSPEND, NULL);
	} else if(action==0x11 && dsp_val==DRM_MODE_DPMS_ON) {
		TS_INFO("receive adf event with dsp_val=%d", dsp_val);
		p->inform(p->pdata, TSEVENT_RESUME, NULL);
	}

	return NOTIFY_OK;
}
#endif

/*static int ts_usb_event_handler(
	struct notifier_block *nb, unsigned long action, void *data)
{
	struct ts_event_receiver *p = container_of(
		nb, struct ts_event_receiver, usb_event_block);

	switch (p->phy->chg_state) {
	case USB_CHARGER_PRESENT:
		TS_DBG("receive usb plug-in event");
		p->inform(p->pdata, TSEVENT_NOISE_HIGH, NULL);
		break;
	case USB_CHARGER_ABSENT:
		TS_DBG("receive usb plug-out event");
		p->inform(p->pdata, TSEVENT_NOISE_NORMAL, NULL);
		break;
	default:
		TS_WARN("receive usb event with unknown action: %u", p->phy->chg_state);
		break;
	}
	return NOTIFY_OK;
}*/

/*
 * register handler and send external events to ats core
 */
int ts_register_ext_event_handler(
	struct ts_data *pdata,	event_handler_t handler)
{
	int ret = 0;

	receiver.pdata = pdata;
	receiver.inform = handler;
//	TS_INFO(" entry");
#ifdef TS_USE_ADF_NOTIFIER
	receiver.adf_event_block.notifier_call = ts_adf_event_handler;
	ret = adf_register_client(&receiver.adf_event_block);
	if (ret < 0)
		TS_WARN("register adf notifier fail, cannot sleep when screen off");
	else
		TS_DBG("register adf notifier succeed");
#endif
#ifdef CONFIG_DSP_NOTIFIER
	receiver.dsp_notifier.notifier_call = dsp_event_handler;
	ret = dsp_register_client(&receiver.dsp_notifier);
	if (ret < 0)
		TS_WARN("register dsp callback failed");
	else
		TS_INFO("register adf notifier succeed");
#endif

	/*receiver.phy = usb_get_phy(USB_PHY_TYPE_USB2);
	if (IS_ERR(receiver.phy)) {
		ret = PTR_ERR(receiver.phy);
		TS_WARN("can't get usb phy, err %d", ret);
	} else {
		receiver.usb_event_block.notifier_call = ts_usb_event_handler;
		ret = usb_register_notifier(receiver.phy, &receiver.usb_event_block);
		if (ret < 0)
			TS_WARN("register usb notifier fail");
		else
			TS_DBG("register usb notifier succeed");
	}*/

	return ret;
}

void ts_unregister_ext_event_handler(void)
{
#ifdef TS_USE_ADF_NOTIFIER
	adf_unregister_client(&receiver.adf_event_block);
#endif
#ifdef CONFIG_DSP_NOTIFIER
	dsp_unregister_client(&receiver.dsp_notifier);
#endif

	if (!IS_ERR(receiver.phy))
		usb_unregister_notifier(receiver.phy, &receiver.usb_event_block);
}

MODULE_AUTHOR("joseph.cai@spreadtrum.com");
MODULE_DESCRIPTION("Spreadtrum touchscreen external event receiver");
MODULE_LICENSE("GPL");
