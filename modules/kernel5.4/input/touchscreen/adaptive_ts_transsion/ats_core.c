#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/time64.h>
#include <linux/module.h>
#include <linux/pm_wakeup.h>
#include <linux/timer.h>

#include "ats_core.h"
#include "adaptive_ts.h"
#include "transsion_incell.h"
#include "gesture.h"
#include "vendor_cfg.h"
//#include <linux/shub_api.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/ktime.h>
#include <linux/kthread.h>



struct ts_data *g_pdata;
static int i2c_suspend_flags;
static int ts_irq_pending;
struct wakeup_source *ts_ps_wakelock;
struct wakeup_source *ts_gesture_wakelock;
extern const char  *sprd_get_lcd_name(void);
extern int sprd_i2c_is_suspend(void);
struct mutex ats_tp_lock;
unsigned char g_chipid_name[16] = {0};
/* spinlock used to enable/disable irq */
static DEFINE_SPINLOCK(tp_irq_lock);
#define TPD_PLS_DEVICE_NAME		"ltr_558als"
#define LTR_IOCTL_MAGIC			0x1C
#define LTR_IOCTL_GET_PFLAG		_IOR(LTR_IOCTL_MAGIC, 1, int)
#define LTR_IOCTL_GET_LFLAG 		_IOR(LTR_IOCTL_MAGIC, 2, int)
#define LTR_IOCTL_SET_PFLAG 		_IOW(LTR_IOCTL_MAGIC, 3, int)
#define LTR_IOCTL_SET_LFLAG 		_IOW(LTR_IOCTL_MAGIC, 4, int)
#define LTR_IOCTL_GET_DATA		_IOW(LTR_IOCTL_MAGIC, 5, unsigned char)
#define LCD_NAME "lcd_nt36525b_tm_mipi_hdp"
static char lcd_name[100];
#ifdef CONFIG_TRANSSION_GESTURE
static int global_gesture_enable(int onoff)
{
	g_pdata->gesture_enable = !!onoff;
    TS_INFO("global_gesture_enable [%d]\n", g_pdata->gesture_enable);
	return 0;
}

static struct msg_gesture_map *get_key_map(unsigned char key)
{
	int i;
	if(g_pdata->controller && g_pdata->controller->msg_gestures_maps) {

		for (i = 0; i < g_pdata->controller->msg_gestures_count; i++) {

			if(g_pdata->controller->msg_gestures_maps[i].report_key == key) {

				return g_pdata->controller->msg_gestures_maps + i;
			}
		}
	}

	return NULL;
}

static int single_gesture_enable(int onoff, unsigned char key)
{
	struct msg_gesture_map *map = get_key_map(key);
	if (!map) {

		TS_INFO("single_gesture_enable no map !!!\n");
		return -EINVAL;
	}
	if(g_pdata->controller && g_pdata->controller->msg_gestures_maps) {

	   return g_pdata->controller->gesture_config(g_pdata->controller, !!onoff, map);
	}
	else {

		TS_INFO("single_gesture_enable no controller\n");
		return -EINVAL;
	}
}

static unsigned char get_gesture_data(void)
{
    unsigned char data = g_pdata->gesture_data ;
    g_pdata->gesture_data = 0;
    return data ;
}
static struct transsion_gesture_if gif = {
	.support_gesture = 1,
	.global_gesture_enable = global_gesture_enable,
	.single_gesture_enable = single_gesture_enable,
	.get_gesture_data = get_gesture_data,
};
#endif

//#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX) || defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX_SPI)
int nvt_get_device_name(void)
{
	if (!strcmp(g_chipid_name, "nt36xxx"))
		return 1;
	else
		return 0;
}
//#endif
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_ILI9882H)
int ilitek_get_device_name(void)
{
	if (!strcmp(g_chipid_name, "ili9882h"))
		return 1;
	else
		return 0;
}
#endif
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_TLSC6X)
int tlsc6x_get_device_name(void)
{
	if (!strcmp(g_chipid_name, "tlsc6x"))
		return 1;
	else
		return 0;
}
#endif
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_ICNL9911X)
int icnl9911x_get_device_name(void)
{
	if (!strcmp(g_chipid_name, "icnl9911x"))
		return 1;
	else
		return 0;
}
#endif

#ifdef TS_ESD_SUPPORT_EN//binhua
static int ts_esd_flag;
static struct hrtimer ts_esd_kthread_timer;
static DECLARE_WAIT_QUEUE_HEAD(ts_esd_waiter);

static int ts_esd_checker_handler(void *unused)
{

	do {
		wait_event_interruptible(ts_esd_waiter, ts_esd_flag != 0);
		ts_esd_flag = 0;


		//TS_INFO("ESD check entry!!!");
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_TLSC6X)
		if (tlsc6x_esd_condition())
			continue;

		tlsc6x_esd_check_work();
#endif

	} while (!kthread_should_stop());

	return 0;
}

enum hrtimer_restart ts_esd_kthread_hrtimer_func(struct hrtimer *timer)
{
	ktime_t ktime;

	ts_esd_flag = 1;
	wake_up_interruptible(&ts_esd_waiter);

	ktime = ktime_set(4, 0); //Amend by zhujiang
	hrtimer_start(&ts_esd_kthread_timer, ktime, HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}


#endif

static int ts_suspend(struct platform_device *pdev, pm_message_t state);

int ts_proximity_ctrl(int enable)
{
	struct ts_data *pdata = platform_get_drvdata(g_pdata->pdev);

	mutex_lock(&ats_tp_lock);
	pdata->tpd_prox_old_state = 0xf0;
	if (enable == 1) {
		pdata->tpd_prox_active = 1;
		if (pdata->tpm_status == TPM_SUSPENDED) {
			pdata->tps_status = TPS_DEON;
			TS_INFO(" ats psensor on suspended !!!");
			mutex_unlock(&ats_tp_lock);
			return 1;
		}
		else {
			pdata->controller->proximity_switch(1);
			pdata->tps_status = TPS_ON ;
		}
		TS_INFO(" ats psensor on !!!");
	}
	else if (enable == 0) {
		pdata->tpd_prox_active = 0;
		pdata->controller->proximity_switch(0);
		pdata->tps_status = TPS_OFF ;
		if (pdata->tpm_status == TPM_DESUSPEND) {
			TS_INFO("tp demend suspend, ps off call ts_suspend!!!");
			ts_suspend(pdata->pdev, PMSG_SUSPEND);
		}
		TS_INFO("ats psensor off !!!");
	}

	mutex_unlock(&ats_tp_lock);
	return 1;
}

static int tpd_ps_open(struct inode *inode, struct file *file)
{
	return 0;
}
static int tpd_ps_release(struct inode *inode, struct file *file)
{
	TS_INFO("tpd_ps_release binhua");
	return ts_proximity_ctrl(0);
}
static long tpd_ps_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int flag;
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case LTR_IOCTL_SET_PFLAG:
		//TS_INFO("LTR_IOCTL_SET_PFLAG");
		if (copy_from_user(&flag, argp, sizeof(flag))) {
			return -EFAULT;
		}
		if (flag < 0 || flag > 1) {
			return -EINVAL;
		}

		TS_INFO(" dialer flag:%d", flag);
		ts_proximity_ctrl(flag);

		break;
	case LTR_IOCTL_GET_PFLAG:
		{
			struct ts_data *pdata = platform_get_drvdata(g_pdata->pdev);
			flag = pdata->tpd_prox_active;
			TS_INFO(" dialer flag:%d", pdata->tpd_prox_active);
			if (copy_to_user(argp, &flag, sizeof(flag))) {
				return -EFAULT;
			}
		}
		break;
	default:
		break;
	}

	return 0;
}

static struct file_operations tpd_ps_fops = {
	 .owner		= THIS_MODULE,
	 .open		= tpd_ps_open,
	 .release		= tpd_ps_release,
	 .unlocked_ioctl	= tpd_ps_ioctl,
};

static struct miscdevice tpd_pls_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = TPD_PLS_DEVICE_NAME,
	.fops = &tpd_ps_fops,
};

#ifdef CONFIG_TPD_SENSORHUB
static ssize_t show_proximity_sensor(struct device *dev, struct device_attribute *attr, char *buf)
{
	char *ptr = buf;
	struct ts_data *pdata = platform_get_drvdata(g_pdata->pdev);

	ptr += sprintf(ptr, "%d\n", pdata->tpd_prox_active);

	return (ptr-buf);
}
static ssize_t store_proximity_sensor(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	TS_INFO(" :count:%d", count);

	if (buf[0] == '0') {
		//ps off
		ts_proximity_ctrl(0);
	} else if (buf[0] == '1') {
		//ps on
		ts_proximity_ctrl(1);
	}

	return count;
}
static int shub_report_proximity_event(u32 value)
{
	s64 k_timestamp;
	struct shub_event_params event;
	k_timestamp = ktime_to_us(ktime_get_boottime());
	event.Cmd = 129;
	event.HandleID = 58;
	event.fdata[0] = value;
	event.fdata[1] = 0;
	event.fdata[2] = 0;
	event.Length = sizeof(struct shub_event_params);
	event.timestamp = k_timestamp;
	printk("[TS] shub_report_proximity_event:value=%d\n", value);

	peri_send_sensor_event_to_iio((u8 *)&event,
									sizeof(struct shub_event_params));
	return 0;
}
static int shub_report_proximity_flush_event(u32 value)
{
	s64 k_timestamp;
	struct shub_event_params event;

	k_timestamp = ktime_to_us(ktime_get_boottime());

	event.Cmd = 130;//flush
	event.HandleID = 58;
	event.fdata[0] = value;
	event.fdata[1] = 0;
	event.fdata[2] = 0;
	event.Length = sizeof(struct shub_event_params);
	event.timestamp = k_timestamp;
	TS_INFO(" value=0x%x!!!", value);

	peri_send_sensor_event_to_iio((u8 *)&event,
	sizeof(struct shub_event_params));

	return 0;
}

static DEVICE_ATTR(psensor_enable, 0664, show_proximity_sensor, store_proximity_sensor);
static ssize_t proximity_sensor_flush_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	TS_INFO("buf==%d", (int)*buf);
	return sprintf(buf, "tp psensor show\n");
}

static ssize_t proximity_sensor_fulsh_store(struct device *dev,
    struct device_attribute *attr,
    const char *buf, size_t count)
{
	//int ps_enable = 0;
	//sscanf(buf, "%d\n",&ps_enable);
	shub_report_proximity_flush_event(0);
	return count;
}
static DEVICE_ATTR(psensor_flush, 0664, proximity_sensor_flush_show, proximity_sensor_fulsh_store);
#endif

/*struct ts_data *ts_get_ts_data(void)
{
	return g_pdata;
}
EXPORT_SYMBOL(ts_get_ts_data);*/

static inline int ts_get_mode(
	struct ts_data *pdata, unsigned int mode)
{
	return test_bit(mode, &pdata->status);
}

int ts_get_mode_ext(struct ts_data *pdata, unsigned int mode) {

	return test_bit(mode, &pdata->status);
}

static inline void ts_set_mode(
	struct ts_data *pdata, unsigned int mode, bool on)
{
	if (on)
		set_bit(mode, &pdata->status);
	else
		clear_bit(mode, &pdata->status);
}

/*
 * clear all points
 * if use type A, there's no need to clear manually
 */
static void ts_clear_points(struct ts_data *pdata)
{
	int i = 0;
	bool need_sync = false;
	struct ts_point *p = NULL;

	for (i = 0; i < pdata->board->max_touch_num; i++) {
		if (pdata->stashed_points[i].pressed) {
			p = &pdata->stashed_points[i];
			input_mt_slot(pdata->input, p->slot);
			input_mt_report_slot_state(pdata->input, MT_TOOL_FINGER, false);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
				TS_DBG("Point[%d] UP: x=%d, y=%d", p->slot, p->x, p->y);
			need_sync = true;
			p->pressed = 0;
		}
	}

	if (need_sync) {
		input_report_key(pdata->input, BTN_TOUCH, 0);
		input_sync(pdata->input);
	}
}

void ts_clear_points_ext(struct ts_data *pdata)
{
	ts_clear_points(pdata);
}
/*
 * report type A, just report what controller tells us
 */
static int ts_report1(struct ts_data *pdata,
		       struct ts_point *points, int counts)
{
	int i;
	struct ts_point *p;

	if (counts == 0) {
		input_report_key(pdata->input, BTN_TOUCH, 0);
		input_mt_sync(pdata->input);
		if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
			TS_DBG("UP: all points leave");
	} else {
		for (i = 0; i < counts; i++) {
			p = points + i;
			input_report_key(pdata->input, BTN_TOUCH, 1);
			input_report_abs(pdata->input, ABS_MT_POSITION_X, p->x);
			input_report_abs(pdata->input, ABS_MT_POSITION_Y, p->y);
			input_mt_sync(pdata->input);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
				TS_DBG("DOWN: x%d=%d, y%d=%d", i, p->x, i, p->y);
		}
	}

	input_sync(pdata->input);

	return 0;
}

/* TODO: implemented this
 * just report what hardware reports
 */
static int ts_report2(struct ts_data *pdata,
		       struct ts_point *points, int counts)
{
	TS_INFO("binhua");
	return 0;
}

/* if (x, y) is a virtual key then return its keycode, otherwise return 0 */
static inline unsigned int ts_get_keycode(
	struct ts_data *pdata, unsigned short x, unsigned short y)
{
	int i;
	struct ts_virtualkey_info *vkey;
	struct ts_virtualkey_hitbox *hbox;

	if (!ts_get_mode(pdata, TSMODE_VKEY_REPORT_ABS)) {
		vkey = pdata->vkey_list;
		hbox = pdata->vkey_hitbox;
		for (i = 0; i < pdata->vkey_count; i++, vkey++, hbox++) {
			if (x >= hbox->left && x <= hbox->right
				&& y >= hbox->top && y <= hbox->bottom)
				return vkey->keycode;
		}
	}

	return 0;
}

static inline void ts_report_abs(struct ts_data *pdata,
				 struct ts_point *point, bool down)
{//binhua20200219

	input_mt_slot(pdata->input, point->slot);
	if (down) {
		input_mt_report_slot_state(pdata->input, MT_TOOL_FINGER, true);
		input_report_abs(pdata->input, ABS_MT_POSITION_X, point->x);
		input_report_abs(pdata->input, ABS_MT_POSITION_Y, point->y);
		input_report_abs(pdata->input, ABS_MT_TOUCH_MAJOR, point->touch_major);
		input_report_key(pdata->input, BTN_TOUCH, 1);
		printk_ratelimited("[TS] X:Y:%d,%d\n", point->x, point->y);//5second print 10 times
	} else {
		input_mt_report_slot_state(pdata->input, MT_TOOL_FINGER, false);
		printk_ratelimited("[TS] report release up\n");//5second print 10 times
	}
}

static inline void ts_report_translate_key(struct ts_data *pdata,
		       struct ts_point *cur, struct ts_point *last,
		       bool *sync_abs, bool *sync_key, bool *btn_down)
{
	unsigned int kc, kc_last;

	kc = ts_get_keycode(pdata, cur->x, cur->y);
	kc_last = ts_get_keycode(pdata, last->x, last->y);

	if (cur->pressed && last->pressed) {
		if (cur->x == last->x && cur->y == last->y) {
			if (!kc)
				*btn_down = true;
			return;
		}
		if (kc > 0 && kc_last > 0) {
			/* from one virtual key to another */
			input_report_key(pdata->input, kc_last, 0);
			input_report_key(pdata->input, kc, 1);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA)) {
				TS_DBG("Key %s UP", ts_get_keyname(kc_last));
				TS_DBG("Key %s DOWN", ts_get_keyname(kc));
			}
			*sync_key = true;
		} else if (kc > 0) {
			/* from screen to virtual key */
			ts_report_abs(pdata, last, false);
			input_report_key(pdata->input, kc, 1);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA)) {
				TS_DBG("Point[%d] UP: x=%d, y=%d",
				       last->slot, last->x, last->y);
				TS_DBG("Key %s DOWN", ts_get_keyname(kc));
			}
			*sync_key = *sync_abs = true;
		} else if (kc_last > 0) {
			/* from virtual key to screen */
			input_report_key(pdata->input, kc_last, 0);
			ts_report_abs(pdata, cur, true);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA)) {
				TS_DBG("Key %s UP", ts_get_keyname(kc));
				TS_DBG("Point[%d] DOWN: x=%d, y=%d",
				       last->slot, last->x, last->y);
			}
			*btn_down = true;
			*sync_key = *sync_abs = true;
		} else {
			/* from screen to screen */
			ts_report_abs(pdata, cur, true);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
				TS_DBG("Point[%d] MOVE TO: x=%d, y=%d",
				       cur->slot, cur->x, cur->y);
			*btn_down = true;
			*sync_abs = true;
		}
	} else if (cur->pressed) {
		if (kc > 0) {
			/* virtual key pressed */
			input_report_key(pdata->input, kc, 1);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
				TS_DBG("Key %s DOWN", ts_get_keyname(kc));
			*sync_key = true;
		} else {
			/* new point down */
			ts_report_abs(pdata, cur, true);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
				TS_DBG("Point[%d] DOWN: x=%d, y=%d",
				       cur->slot, cur->x, cur->y);
			*btn_down = true;
			*sync_abs = true;
		}
	} else if (last->pressed) {
		if (kc_last > 0) {
			/* virtual key released */
			input_report_key(pdata->input, kc_last, 0);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
				TS_DBG("Key %s UP", ts_get_keyname(kc_last));
			*sync_key = true;
		} else {
			/* point up */
			ts_report_abs(pdata, last, false);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
				TS_DBG("Point[%d] UP: x=%d, y=%d",
				       last->slot, last->x, last->y);
			*sync_abs = true;
		}
	}
}

static inline void ts_report_no_translate(struct ts_data *pdata,
		      struct ts_point *cur, struct ts_point *last,
		      bool *sync_abs, bool *btn_down)
{
	if (cur->pressed && last->pressed) {
		*btn_down = true;
		if (cur->x != last->x || cur->y != last->y) {
			ts_report_abs(pdata, cur, true);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
				TS_DBG("Point[%d] MOVE TO: x=%d, y=%d",
				       cur->slot, cur->x, cur->y);
			*sync_abs = true;
		}
	} else if (cur->pressed) {
		*btn_down = true;
		ts_report_abs(pdata, cur, true);
		if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
			TS_DBG("Point[%d] DOWN: x=%d, y=%d",
			       cur->slot, cur->x, cur->y);
		*sync_abs = true;
	} else if (last->pressed) {
		ts_report_abs(pdata, last, false);
		if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
			TS_DBG("Point[%d] UP: x=%d, y=%d",
			       last->slot, last->x, last->y);
		*sync_abs = true;
	}
}

static inline void ts_fix_UP_if_needed(struct ts_data *pdata,
			struct ts_point *p, enum ts_stashed_status status,
			bool *sync_key, bool *sync_abs)
{
	unsigned int kc;

	if (status == TSSTASH_NEW && p->pressed) {
		if (!ts_get_mode(pdata, TSMODE_VKEY_REPORT_ABS)) {
			kc = ts_get_keycode(pdata, p->x, p->y);
			if (kc) {
				input_report_key(pdata->input, kc, 0);
				*sync_key = true;
				if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
					TS_DBG("Key %s UP",
						ts_get_keyname(kc));
				return;
			}
		}
		ts_report_abs(pdata, p, false);
		*sync_abs = true;
		if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
			TS_DBG("Point[%d] UP: x=%d, y=%d",
				p->slot, p->x, p->y);
	}
}

static int ts_report3(struct ts_data *pdata,
		       struct ts_point *points, int counts)
{
	struct ts_point *cur, *last;
	int i;
	bool sync_abs = false, btn_down = false, sync_key = false;

	for (i = 0; i < counts; i++) {
		cur = points + i;
		if (cur->slot >= pdata->board->max_touch_num) {
			TS_ERR("invalid current slot number: %u", cur->slot);
			continue;
		}

		last = &pdata->stashed_points[cur->slot];
		pdata->stashed_status[cur->slot] = TSSTASH_CONSUMED;
		if (last->slot >= pdata->board->max_touch_num) {
			TS_ERR("invalid last slot number: %u", last->slot);
			continue;
		}

		if (!ts_get_mode(pdata, TSMODE_VKEY_REPORT_ABS))
			ts_report_translate_key(pdata, cur, last, &sync_abs,
					       &sync_key, &btn_down);
		else
			ts_report_no_translate(pdata, cur, last,
					      &sync_abs, &btn_down);
	}

	/* check for disappeared UP events */
	for (i = 0; i < pdata->board->max_touch_num; i++)
		ts_fix_UP_if_needed(pdata,
			&pdata->stashed_points[i], pdata->stashed_status[i],
			&sync_key, &sync_abs);

	/* record current point's status */
	memset(pdata->stashed_status, 0, sizeof(pdata->stashed_status));
	for (i = 0; i < counts; i++) {

		if (points[i].slot >= pdata->board->max_touch_num) {
			TS_ERR("invalid slot number: %u", points[i].slot);
			continue;
		}
		pdata->stashed_status[points[i].slot] = TSSTASH_NEW;
	}


	if (sync_key || sync_abs) {
		if (sync_abs)
			input_report_key(pdata->input, BTN_TOUCH, btn_down);
		input_sync(pdata->input);

		/* record current point */
		memset(pdata->stashed_points, 0x00, sizeof(pdata->stashed_points));
		for (i = 0; i < counts; i++) {//pdata->board->max_touch_num counts

			if (points[i].slot >= pdata->board->max_touch_num) {
				TS_ERR("invalid slot number: %u", points[i].slot);
				continue;
			}
			memcpy(&pdata->stashed_points[points[i].slot],
				&points[i], sizeof(struct ts_point));
		}

	}

	return 0;
}

static int ts_report(struct ts_data *pdata, struct ts_point *points, int counts)
{
	unsigned int type = pdata->controller->config;

	type &= TSCONF_REPORT_TYPE_MASK;
	if (type == TSCONF_REPORT_TYPE_1)
		return ts_report1(pdata, points, counts);
	else if (type == TSCONF_REPORT_TYPE_2)
		return ts_report2(pdata, points, counts);
	else if (type == TSCONF_REPORT_TYPE_3)
		return ts_report3(pdata, points, counts);

	return 0;
}

static int ts_gesture_report(struct ts_data *pdata, unsigned char id)
{
    int i;
    unsigned char gesture_data = 0 ;

	if (pdata->controller && pdata->controller->msg_gestures_maps) {

		for (i = 0; i < pdata->controller->msg_gestures_count; i++) {

			if (pdata->controller->msg_gestures_maps[i].tp_gesture == id &&
				pdata->controller->msg_gestures_maps[i].enabled == 1)
			{
				gesture_data = pdata->controller->msg_gestures_maps[i].report_key;
			}
		}
	}
	if(gesture_data != 0) {

	TS_WARN("Gesture Code=%d", gesture_data);
	pdata->gesture_data = gesture_data;
	__pm_wakeup_event(ts_gesture_wakelock, 500);
	input_report_key(pdata->input, KEY_F13, 1);
	input_sync(pdata->input);
	input_report_key(pdata->input, KEY_F13, 0);
	input_sync(pdata->input);
	}
	return 0;
}
/*static int ts_request_gpio(struct ts_data *pdata)
{
	int retval;
	struct ts_board *board = pdata->board;

	if (!board->int_gpio) {
		TS_WARN("no int on our board!");
	} else {
		retval = devm_gpio_request(&pdata->pdev->dev,
			board->int_gpio, ATS_INT_LABEL);
		if (retval < 0) {
			TS_ERR("failed to request int gpio: %d, retval: %d!",
				board->int_gpio, retval);
			return retval;
		}
		TS_DBG("request int gpio \"%d\"", board->int_gpio);
	}

	if (!board->rst_gpio) {
		TS_WARN("no rst on our board!");
	} else {
		retval = devm_gpio_request(&pdata->pdev->dev,
			board->rst_gpio, ATS_RST_LABEL);
		if (retval < 0) {
			TS_ERR("failed to request rst gpio: %d, retval: %d!",
				board->rst_gpio, retval);
			return retval;
		}
		TS_DBG("request rst gpio \"%d\"", board->rst_gpio);
	}

	return 0;
}*/
static int ts_request_gpio1(struct ts_data *pdata)
{
	int retval;
	struct ts_board *board = pdata->board;
	/*if (!board->int_gpio) {
		TS_WARN("no int on our board!");
	} else {
		retval = devm_gpio_request(&pdata->pdev->dev,
			board->int_gpio, ATS_INT_LABEL);
		if (retval < 0) {
			TS_ERR("failed to request int gpio: %d, retval: %d!",
				board->int_gpio, retval);
			return retval;
		}
		TS_DBG("request int gpio \"%d\"", board->int_gpio);
	}*/
	if (gpio_is_valid(board->int_gpio)) {
		retval = gpio_request(board->int_gpio, "fts_irq_gpio");
		if (retval) {
			TS_ERR("[GPIO]irq gpio request failed");
			return retval;
		}

		retval = gpio_direction_input(board->int_gpio);
		if (retval) {
			TS_ERR("[GPIO]set_direction for irq gpio failed");
			return retval;
		}
	}
	if (gpio_is_valid(board->rst_gpio)) {
		retval = gpio_request(board->rst_gpio, "fts_rst_gpio");
		if (retval) {
			TS_ERR("[GPIO]rst gpio request failed");
			return retval;
		}

		retval = gpio_direction_output(board->rst_gpio, 1);
		if (retval) {
			TS_ERR("[GPIO]set_direction for rst gpio failed");
			return retval;
		}
	}
	/*if (!board->rst_gpio) {
		TS_WARN("no rst on our board!");
	} else {
		retval = devm_gpio_request(&pdata->pdev->dev,
			board->rst_gpio, ATS_RST_LABEL);
		if (retval < 0) {
			TS_ERR("failed to request rst gpio: %d, retval: %d!",
				board->rst_gpio, retval);
			return retval;
		}
		TS_DBG("request rst gpio \"%d\"", board->rst_gpio);
	}*/

	return 0;
}
/*static int ts_export_gpio(struct ts_data *pdata)
{
	int retval;
	struct ts_board *board = pdata->board;

	if (board->int_gpio) {
		retval = gpio_export(board->int_gpio, true);
		if (retval < 0) {
			TS_WARN("failed to export int gpio: %d, retval: %d!",
				board->int_gpio, retval);
			return retval;
		}
		TS_DBG("exported int gpio: %d", board->int_gpio);
	}

	if (board->rst_gpio) {
		retval = gpio_export(board->rst_gpio, true);
		if (retval < 0) {
			TS_WARN("failed to export rst gpio: %d, retval: %d!",
				board->rst_gpio, retval);
			return retval;
		}
		TS_DBG("exported rst gpio: %d", board->rst_gpio);
	}

	return 0;
}*/

struct ts_firmware_upgrade_param {
	struct ts_data *pdata;
	bool force_upgrade;
};

/*
 * firmware upgrading worker, asynchronous callback from firmware subsystem
 */
static void ts_firmware_upgrade_worker(const struct firmware *fw, void *context)
{
	struct ts_firmware_upgrade_param *param = context;
	struct ts_data *pdata = param->pdata;
	enum ts_result ret;

	if (unlikely(fw == NULL)) {
		TS_WARN("upgrading cancel: cannot find such a firmware file");
		return;
	}

	if (!ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)) {
		TS_ERR("controller not ready!");
		return;
	}

	if (!pdata->controller->upgrade_firmware) {
		TS_ERR("controller \"%s\" does not support firmware upgrade",
			pdata->controller->name);
		/* if controller doesn't support, don't hold firmware */
		release_firmware(fw);
		return;
	}

	ts_set_mode(pdata, TSMODE_CONTROLLER_STATUS, false);
	TS_DBG(">>> Upgrade Firmware Begin <<<");
	__pm_stay_awake(pdata->upgrade_lock);
	ret = pdata->controller->upgrade_firmware(pdata->controller,
		fw->data, fw->size, param->force_upgrade);
	__pm_relax(pdata->upgrade_lock);
	TS_DBG(">>> Upgrade Firmware End <<<");
	if (ret == TSRESULT_UPGRADE_FINISHED)
		TS_INFO(">>> Upgrade Result: Success <<<");
	else if (ret == TSRESULT_INVALID_BINARY)
		TS_ERR(">>> Upgrade Result: bad firmware file <<<");
	else if (ret == TSRESULT_OLDER_VERSION)
		TS_WARN(">>> Upgrade Result: older version, no need to upgrade <<<");
	else
		TS_ERR(">>> Upgrade Result: other error <<<");
	ts_set_mode(pdata, TSMODE_CONTROLLER_STATUS, true);

	release_firmware(fw);
}

/*
 * firmware upgrade entry
 */
static int ts_request_firmware_upgrade(struct ts_data *pdata,
	const char *fw_name, bool force_upgrade)
{
	char *buf, *name;
	struct ts_firmware_upgrade_param *param;

	if (!ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)) {
		TS_ERR("controller not exist!");
		return -EBUSY;
	}

	param = devm_kmalloc(&pdata->pdev->dev,
		sizeof(struct ts_firmware_upgrade_param), GFP_KERNEL);
	if (IS_ERR(param) || param == NULL) {
		TS_ERR("fail to allocate firmware upgrade param");
		return -ENOMEM;
	}
	param->force_upgrade = force_upgrade;
	param->pdata = pdata;

	if (!fw_name) {
		buf = devm_kmalloc(&pdata->pdev->dev, 32, GFP_KERNEL);
		if (IS_ERR(buf) || buf == NULL) {
			TS_ERR("fail to allocate buffer for firmware name");
			return -ENOMEM;
		}
		sprintf(buf, "%s-%s.bin", pdata->controller->vendor, pdata->controller->name);
		name = buf;
	} else {
		name = (char *)fw_name;
	}

	TS_DBG("requesting firmware \"%s\"", name);
	if (request_firmware_nowait(THIS_MODULE, true, name, &pdata->pdev->dev,
		GFP_KERNEL, param, ts_firmware_upgrade_worker) < 0) {
		TS_ERR("request firmware failed");
		return -ENOENT;
	}

	return 0;
}

static int ts_register_input_dev(struct ts_data *pdata)
{
	int retval;
	unsigned int i;
	struct input_dev *input;

	input = devm_input_allocate_device(&pdata->pdev->dev);
	if (IS_ERR(input)) {
		TS_ERR("Failed to allocate input device.");
		return -ENOMEM;
	}

	//---set input device info.---
	input->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;
	input->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	input->propbit[0] = BIT(INPUT_PROP_DIRECT);

	input_set_capability(input, EV_KEY, BTN_TOUCH);
	if (ts_get_mode(pdata, TSMODE_VKEY_REPORT_ABS) && pdata->vkey_list) {
		for (i = 0; i < pdata->vkey_count; i++)
			TS_DBG("keycode=%d", pdata->vkey_list[i].keycode);
		input_set_capability(input, EV_KEY, pdata->vkey_list[i].keycode);

	}
	if (ts_get_mode(pdata, TSMODE_GESTURE_STATUS)) {
		input_set_capability(input, EV_KEY, KEY_F13);
		__set_bit(KEY_F13, input->keybit);
	}

	input_set_capability(input, EV_ABS, ABS_MT_POSITION_X);
	input_set_capability(input, EV_ABS, ABS_MT_POSITION_Y);
	input_set_capability(input, EV_ABS, ABS_MT_WIDTH_MAJOR);
	input_set_capability(input, EV_ABS, ABS_MT_TOUCH_MAJOR);
	set_bit(EV_KEY, input->evbit);
	set_bit(EV_ABS, input->evbit);
	/* TODO report pressure */
	/* input_set_capability(input, EV_ABS, ABS_MT_PRESSURE); */

	if (ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)
		&& ((pdata->controller->config & TSCONF_REPORT_TYPE_MASK)
		!= TSCONF_REPORT_TYPE_1))
		input_mt_init_slots(input, pdata->board->max_touch_num, INPUT_MT_DIRECT);

	TS_DBG("pdata->width=%d pdata->height=%d", pdata->width, pdata->height);
	input_set_abs_params(input, ABS_MT_POSITION_X, 0, pdata->width, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0, pdata->height, 0, 0);
	input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, 15, 0, 0);
	input_set_abs_params(input, ABS_MT_WIDTH_MAJOR, 0, 15, 0, 0);

	input->name = ATS_INPUT_DEV;//"adaptive_ts"
	input->dev.parent = &pdata->pdev->dev;
	input->id.bustype = BUS_HOST;

	retval = input_register_device(input);
	if (retval < 0) {
		TS_ERR("Failed to register input device.");
		input_free_device(input);
		return retval;
	}

	TS_DBG("Succeed to register input device.");
	pdata->input = input;

	return 0;
}

/*
 * reset controller
 */
static int ts_reset_controller(struct ts_data *pdata, bool hw_reset)
{
	int level;

//#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX) || defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX_SPI)
	if (nvt_get_device_name())
		return 0;//binhua0218
//#endif
	if (ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)) {
		if (hw_reset) {
			if ((pdata->controller->config & TSCONF_RESET_LEVEL_MASK)
				== TSCONF_RESET_LEVEL_LOW)
				level = 0;
			else
				level = 1;

			//TS_INFO("hardware reset now...");
			ts_gpio_output(TSGPIO_RST, level);
			mdelay(pdata->controller->reset_keep_ms);
			ts_gpio_output(TSGPIO_RST, !level);
			mdelay(pdata->controller->reset_delay_ms);

		} else {
			/* TODO: add software reset */
		}
	}
	return 0;
}


int ts_reset_controller_ex(struct ts_data *pdata, bool hw_reset)
{
	return ts_reset_controller(pdata, hw_reset);
}
//EXPORT_SYMBOL(ts_reset_controller_ex);

/*
 * configure parameters from controller definition and board specification
 * values from board specification always overwrite controller definition
 * if they are found in dts file
 *
 * did not check value of pdata->controller
 */

static int  ts_data_init(struct ts_data *pdata)
{
	pdata->gesture_enable = 0;
	pdata->gesture_state = TPG_OFF;
	pdata->tpd_prox_old_state = 0;
	pdata->tpd_prox_active = 0;
	pdata->touch_point = 0;
	pdata->ps_buf = 0;
	pdata->usb_plug_state = 0;
	pdata->tpm_status = TPM_ACTIVE ;
	pdata->tps_status = TPS_OFF ;
	return 0;
}
static void ts_configure(struct ts_data *pdata)
{
	struct ts_controller *c = pdata->controller;
	struct ts_board *b = pdata->board;
	struct ts_data *p = pdata;
	const struct ts_virtualkey_info *key_source;
	int key_count = 0;

	/* configure touch panel size */
	if (b->panel_width) {
		/* have touch panel size configured in .dts, just use these values */
		p->width = b->panel_width;
		p->height = b->panel_height;
	} else {
		if (b->surface_width && (b->surface_width < b->lcd_width)) {
			/*
			 * got surface in .dts file, and its size is smaller than lcd size
			 * we're simulating low resolution now!!
			 */
			p->width = c->panel_width * b->surface_width / b->lcd_width;
			p->height = c->panel_height * b->surface_height / b->lcd_height;
			TS_INFO(
					"low resolution simulation, surface=%dx%d, lcd=%dx%d",
					b->surface_width, b->surface_height,
					b->lcd_width, b->lcd_height);
		} else {
			/*
			 * nothing special, we just report real size of controller and
			 * let framework to do scaling
			 */
			p->width = c->panel_width;
			p->height = c->panel_height;
		}
	}

	/* configure virtualkeys */
	if (b->virtualkey_switch) {
		if (b->virtualkey_count > 0) {
			/* .dts values come first */
			key_count = b->virtualkey_count;
			key_source = b->virtualkeys;
		} //else if (c->virtualkey_count > 0) {//binhua del user dtsi data
			/* check if controller values exist */
		//	key_count = c->virtualkey_count;
		//	key_source = c->virtualkeys;
		//}
	}

	if (key_count) {
		p->vkey_count = key_count;
		p->vkey_list = devm_kzalloc(&p->pdev->dev,
			sizeof(p->vkey_list[0]) * p->vkey_count, GFP_KERNEL);
		p->vkey_hitbox = devm_kzalloc(&p->pdev->dev,
			sizeof(p->vkey_hitbox[0]) * p->vkey_count, GFP_KERNEL);
		if (IS_ERR_OR_NULL(p->vkey_list) || IS_ERR_OR_NULL(p->vkey_hitbox)) {
			TS_ERR("No memory for virtualkeys!");
			p->vkey_count = 0;
			p->vkey_list = NULL;
			p->vkey_hitbox = NULL;
			return;
		}
		memcpy(p->vkey_list, key_source, sizeof(key_source[0]) * key_count);
		for (key_count = 0; key_count < p->vkey_count; key_count++) {
			p->vkey_hitbox[key_count].top = key_source[key_count].y
				- key_source[key_count].height / 2;
			p->vkey_hitbox[key_count].bottom = key_source[key_count].y
				+ key_source[key_count].height / 2;
			p->vkey_hitbox[key_count].left = key_source[key_count].x
				- key_source[key_count].width / 2;
			p->vkey_hitbox[key_count].right = key_source[key_count].x
				+ key_source[key_count].width / 2;
		}
	}

	/* configure reg_width to ensure communication*/
	if ((c->config & TSCONF_ADDR_WIDTH_MASK) == TSCONF_ADDR_WIDTH_WORD)
		b->bus->reg_width = 2;
	else
		b->bus->reg_width = 1;

	/* configure virtual key reporting */
	ts_set_mode(pdata, TSMODE_VKEY_REPORT_ABS, pdata->board->virtualkey_switch);

	/* configure firmware upgrading */
	if (pdata->board->auto_upgrade_fw)
		ts_set_mode(pdata, TSMODE_AUTO_UPGRADE_FW, pdata->board->auto_upgrade_fw);
	//binhua add for tp
	if (pdata->board->ps_status)
		ts_set_mode(pdata, TSMODE_PS_STATUS, pdata->board->ps_status);
	if (pdata->board->sensorhub_status)
		ts_set_mode(pdata, TSMODE_SENSORHUB_STATUS, pdata->board->sensorhub_status);
	if (pdata->board->gesture_status)
		ts_set_mode(pdata, TSMODE_GESTURE_STATUS, pdata->board->gesture_status);
	if (pdata->board->upgrade_status)
		ts_set_mode(pdata, TSMODE_UPGRADE_STATUS, pdata->board->upgrade_status);
}

/*
 * open controller
 */
static int ts_open_controller(struct ts_data *pdata)
{
	struct ts_controller *c;
	int result;

	if (!ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST))
		return -ENODEV;
	c = pdata->controller;

	/* open board specific regulator */
	if (pdata->board->avdd_supply) {
		if (IS_ERR_OR_NULL(pdata->power)) {
			pdata->power = devm_regulator_get(&pdata->pdev->dev,
				pdata->board->avdd_supply);
			if (IS_ERR_OR_NULL(pdata->power)) {
				TS_ERR("Cannot get regulator \"%s\"", pdata->board->avdd_supply);
				return -ENODEV;
			}
			TS_DBG("get regulator \"%s\"", pdata->board->avdd_supply);
		}
		if (regulator_enable(pdata->power)) {
			TS_ERR("Cannot enable regulator \"%s\"", pdata->board->avdd_supply);
			devm_regulator_put(pdata->power);
			pdata->power = NULL;
		}
		TS_DBG("enable regulator \"%s\"", pdata->board->avdd_supply);
	}

	/* set gpio directions */
	ts_gpio_input(TSGPIO_INT);
	if ((c->config & TSCONF_RESET_LEVEL_MASK) == TSCONF_RESET_LEVEL_LOW)
		ts_gpio_output(TSGPIO_RST, 1);
	else
		ts_gpio_output(TSGPIO_RST, 0);

	if ((c->config & TSCONF_POWER_ON_RESET_MASK) == TSCONF_POWER_ON_RESET)
		ts_reset_controller(pdata, true);

	if (c->handle_event) {
		result = c->handle_event(c, TSEVENT_POWER_ON, pdata->board->priv);
		return result == TSRESULT_EVENT_HANDLED ? 0 : -ENODEV;
	}

	return 0;
}

/*
 * close controller
 */
static void ts_close_controller(struct ts_data *pdata)
{
	if (ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)) {
		/* TODO power off controller */

		if (pdata->board->avdd_supply && !IS_ERR_OR_NULL(pdata->power))
			regulator_disable(pdata->power);
	}
}

/*
 * enable/disable irq
 */
static void ts_enable_irq_unlock(struct ts_data *pdata, bool enable)
{
	if (enable && !ts_get_mode(pdata, TSMODE_IRQ_STATUS)) {
		enable_irq(pdata->irq);
		ts_set_mode(pdata, TSMODE_IRQ_STATUS, true);
		TS_DBG("enable irq");
	} else if (!enable && ts_get_mode(pdata, TSMODE_IRQ_STATUS)) {
		ts_set_mode(pdata, TSMODE_IRQ_STATUS, false);
		disable_irq_nosync(pdata->irq);
		TS_DBG("disable irq");
	} else {
		TS_WARN("Irq status and enable(%s) mis-match!", enable ? "true" : "false");
	}
}

static void ts_enable_irq(struct ts_data *pdata, bool enable)
{
	unsigned long irqflags = 0;

	spin_lock_irqsave(&tp_irq_lock, irqflags);
	ts_enable_irq_unlock(pdata, enable);
	spin_unlock_irqrestore(&tp_irq_lock, irqflags);
}

/*void ts_enable_irq_ex(struct ts_data *pdata, bool enable)
{
	ts_enable_irq(pdata, enable);
}
EXPORT_SYMBOL(ts_enable_irq_ex);*/
/*
 * enable/disable polling
 */
static void ts_enable_polling(struct ts_data *pdata, bool enable)
{
	ts_set_mode(pdata, TSMODE_POLLING_STATUS, enable ? true : false);
	TS_DBG("%s polling", enable ? "enable" : "disable");
}

/*
 * poll worker
 */
static void ts_poll_handler(struct timer_list *timer)
{
	/*struct ts_data *pdata = (struct ts_data *)_data;
	struct ts_point points[TS_MAX_POINTS];
	int counts;

	memset(points, 0, sizeof(struct ts_point) * TS_MAX_POINTS);

	if (ts_get_mode(pdata, TSMODE_POLLING_STATUS)) {
		if (!ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)) {
			TS_ERR("controller not exist");
		} else if (!ts_get_mode(pdata, TSMODE_CONTROLLER_STATUS)) {
			TS_WARN("controller is busy...");

			msleep(100);
		} else {//binhua
			counts = pdata->controller->fetch_points(pdata->controller, points);
			if (counts >= 0)
				ts_report(pdata, points, counts);
		}

		mod_timer(timer, pdata->poll_interval);
	}*/
}

/*
 * enable/disable polling mode
 */
static void ts_poll_control(struct ts_data *pdata, bool enable)
{
	if (enable) {
		if (ts_get_mode(pdata, TSMODE_POLLING_MODE)) {
			TS_WARN("polling mode already enabled");
			return;
		}

		ts_enable_polling(pdata, true);
		mod_timer(&pdata->poll_timer, jiffies + pdata->poll_interval);
		ts_set_mode(pdata, TSMODE_POLLING_MODE, true);
		TS_DBG("succeed to enable polling mode");
	} else {
		if (!ts_get_mode(pdata, TSMODE_POLLING_MODE)) {
			TS_WARN("polling mode already disabled");
			return;
		}

		ts_enable_polling(pdata, false);
		ts_set_mode(pdata, TSMODE_POLLING_MODE, false);
		TS_DBG("succeed to disable polling mode");
	}
}

/*static void ts_usb_plug_switch(void){


		if(g_pdata->usb_plug_state==1){
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_ILI9882H)
			if(ilitek_get_device_name())
				mdelay(90);
#endif
			g_pdata->controller->handle_event(g_pdata->controller, TSEVENT_NOISE_HIGH, NULL);//usb plug in
		}
		else if(g_pdata->usb_plug_state==2){//如果USB已经拨出，重新唤醒时，不需要下命令，固件默认已经是拨出状态
		}
		else {
			g_pdata->controller->handle_event(g_pdata->controller, TSEVENT_NOISE_NORMAL, NULL);//usb plug out
		}

}*/

/*static void ts_prepare_tp_reset(int level)
{
	ts_gpio_output(TSGPIO_RST, level);
	mdelay(5);
}*/

/*static int ts_prepare_lcd_reset(void)
{
	int handle = 0;
	if (g_pdata && g_pdata->controller && g_pdata->controller->incell) {
		if (g_pdata->tpd_prox_active) {

			disable_irq(g_pdata->irq);
			handle = 1;
		}
	}
	return handle;
}*/

/* FIXME 要求：caller传入的handle，必须是ts_prepare_lcd_reset的返回值 */
/*static void ts_post_lcd_reset(int handle)
{
	if (g_pdata && handle)
		enable_irq(g_pdata->irq);
}*/

/*******************************************************
Description:

incell lcd Whether issued order to turn off the backlight only, or Issued by the 28 10
return:
	0 Issued by the 28 10  not incell lcd
	1 Issue an order to turn off the backlight only,but not Issued by the 28 10
	2 Issued by the 28 10  incell lcd for ili9882h
	3 gesture is active
*******************************************************/
/*static int ts_need_keep_power(void)
{

	if (g_pdata->controller->incell) {
		if (g_pdata->tpd_prox_active){
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_ILI9882H)
			if(ilitek_get_device_name()){
				return 2;
			}
#endif
			if(g_pdata->tpm_status == TPM_SUSPENDED)
				return 2;

			return 1;
		}

		if (ts_get_mode(g_pdata, TSMODE_GESTURE_STATUS)&&g_pdata->gesture_state == TPG_ON)
		{
			TS_INFO("gesture is active");
			return 3;
		}

	}
	return 0;
}*/

/*static struct transsion_incell_interface ts_incell_interface = {
	.prepare_tp_reset = ts_prepare_tp_reset,
	.prepare_lcd_reset = ts_prepare_lcd_reset,
	.post_lcd_reset = ts_post_lcd_reset,
	.need_keep_power = ts_need_keep_power,
	.usb_plug_switch = ts_usb_plug_switch,
};*/

/*
 * isr top
 */
static irqreturn_t ts_interrupt_handler(int irq, void *data)
{
	struct ts_data *pdata = data;
	//static struct timespec64 last;
	//struct timespec64 cur;

	/* TODO: change to async for performance reason */
	/*if (ts_get_mode(pdata, TSMODE_DEBUG_IRQTIME)) {
		getnstimeofday64(&cur);
		TS_DBG("time elapsed in two interrupts: %ld ns",
			timespec64_sub(cur, last).tv_nsec);
		memcpy(&last, &cur, sizeof(struct timespec64));
	}*/

	spin_lock(&tp_irq_lock);
	if (i2c_suspend_flags) {
		ts_irq_pending = 1;
		ts_enable_irq_unlock(pdata, false);
		__pm_wakeup_event(ts_ps_wakelock, 1000);
		printk("[TS] sprd_i2c_is_suspend\n");
		spin_unlock(&tp_irq_lock);
		return IRQ_HANDLED;
	}
	spin_unlock(&tp_irq_lock);
	return IRQ_WAKE_THREAD;
}

/*
 * isr bottom
 */
static irqreturn_t ts_interrupt_worker(int irq, void *data)
{
	int counts;
	struct ts_data *pdata = data;
	struct ts_point points[TS_MAX_POINTS];
    unsigned char gesture_id;
	memset(points, 0, sizeof(struct ts_point) * TS_MAX_POINTS);

	if (ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)) {
		if (ts_get_mode(pdata, TSMODE_GESTURE_STATUS) && pdata->gesture_state == TPG_ON) {
			if(pdata->controller->gesture_readdata) {
				gesture_id = pdata->controller->gesture_readdata(pdata->controller);
				ts_gesture_report(pdata ,gesture_id);
			}
			return 0;
		}
		else {

			counts = pdata->controller->fetch_points(pdata->controller, points);
		}
		if ((ts_get_mode(pdata, TSMODE_PS_STATUS) || ts_get_mode(pdata, TSMODE_SENSORHUB_STATUS))
			&& pdata->tpd_prox_active) {
			if((pdata->ps_buf != 0xc0) && (pdata->ps_buf != 0xe0)) {
				pdata->controller->ps_reset();
			}
		}

		if (ts_get_mode(pdata, TSMODE_PS_STATUS)) {
			if (pdata->tpd_prox_active && (pdata->touch_point == 0)) {
				if (((pdata->ps_buf == 0xc0) || (pdata->ps_buf == 0xe0)) && (pdata->tpd_prox_old_state != pdata->ps_buf)) {
					input_report_abs(pdata->ps_input_dev, ABS_DISTANCE, (pdata->ps_buf == 0xc0) ? 0 : 1);
					input_mt_sync(pdata->ps_input_dev);
					input_sync(pdata->ps_input_dev);
					printk("[TS] ps report event:%x\n", pdata->ps_buf);
					__pm_wakeup_event(ts_ps_wakelock, 1000);
				}

				pdata->controller->ps_irq_handler(pdata);
			}
		}

		if (ts_get_mode(pdata, TSMODE_SENSORHUB_STATUS)) {
			if (pdata->tpd_prox_active) {
				if (pdata->tpd_prox_old_state != pdata->ps_buf) {
#ifdef CONFIG_TPD_SENSORHUB
					if (0xC0 == pdata->ps_buf){
						shub_report_proximity_event(0);// 1;  near
					}
					else if (0xE0 == pdata->ps_buf) {
						shub_report_proximity_event(0x40a00000);// 0;  far-away
					} else
#endif
						TS_INFO("Sensor hub report event is error!!!");
					__pm_wakeup_event(ts_ps_wakelock, 1000);
				}
				pdata->controller->ps_irq_handler(pdata);
			}
		}


		mutex_lock(&ats_tp_lock);
        if (counts >= 0 && pdata->tpm_status != TPM_SUSPENDED)//binhua123
            ts_report(pdata, points, counts);
        mutex_unlock(&ats_tp_lock);

	}

	return IRQ_HANDLED;
}

/*
 * (un)register isr
 */
static int ts_isr_control(struct ts_data *pdata, bool _register)
{
	int retval = 0;
	unsigned long flags = IRQF_TRIGGER_FALLING;

	if (_register) {
		if (ts_get_mode(pdata, TSMODE_IRQ_MODE)) {
			TS_WARN("interrupt handler already registered");
			return 0;
		}

		switch (pdata->controller->config & TSCONF_IRQ_TRIG_MASK) {
		case TSCONF_IRQ_TRIG_EDGE_FALLING:
			flags = IRQF_TRIGGER_FALLING;
			break;
		case TSCONF_IRQ_TRIG_EDGE_RISING:
			flags = IRQF_TRIGGER_RISING;
			break;
		case TSCONF_IRQ_TRIG_LEVEL_HIGH:
			flags = IRQF_TRIGGER_HIGH;
			break;
		case TSCONF_IRQ_TRIG_LEVEL_LOW:
			flags = IRQF_TRIGGER_LOW;
			break;
		}

		flags |= IRQF_ONESHOT;//IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND

		retval = devm_request_threaded_irq(&pdata->pdev->dev, pdata->irq,
			ts_interrupt_handler, ts_interrupt_worker, IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
			ATS_IRQ_HANDLER, pdata);
		if (retval < 0) {
			TS_ERR("register interrupt handler fail, retval=%d", retval);
			return retval;
		}

		ts_set_mode(pdata, TSMODE_IRQ_MODE, true);
		ts_set_mode(pdata, TSMODE_IRQ_STATUS, true);
		TS_DBG("succeed to register interrupt handler, irq=%d", pdata->irq);
	} else {
		if (!ts_get_mode(pdata, TSMODE_IRQ_MODE)) {
			TS_WARN("interrupt handler already unregistered");
			return 0;
		}

		ts_enable_irq(pdata, false);
		devm_free_irq(&pdata->pdev->dev, pdata->irq, pdata);
		ts_set_mode(pdata, TSMODE_IRQ_MODE, false);
		TS_DBG("succeed to unregister interrupt handler");
	}

	return retval;
}

int ts_suspend_late(struct device *dev)
{
	unsigned long irqflags = 0;

	printk("haibo.li %s now\n", __func__);
	// FIXME Need spinlock here?
	spin_lock_irqsave(&tp_irq_lock, irqflags);
	i2c_suspend_flags = 1;
	spin_unlock_irqrestore(&tp_irq_lock, irqflags);
	return 0;
}

int ts_resume_early(struct device *dev)
{
	unsigned long irqflags = 0;
	int redo_interrupt = 0;

	printk("haibo.li %s now\n", __func__);

	spin_lock_irqsave(&tp_irq_lock, irqflags);
	i2c_suspend_flags = 0;
	if (ts_irq_pending) {
		ts_irq_pending = 0;
		printk("[TS] irq_resume_early i2c is suspend\n");
		redo_interrupt = 1;
	}
	spin_unlock_irqrestore(&tp_irq_lock, irqflags);

	if (redo_interrupt) {
		ts_interrupt_worker(g_pdata->irq, g_pdata);
		ts_enable_irq(g_pdata, true);
	}

	return 0;
}

/*
 * suspend and turn off controller
 */
static int ts_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct ts_data *pdata = platform_get_drvdata(pdev);

	TS_INFO(" suspend start...");

#ifdef TS_ESD_SUPPORT_EN
	if (!pdata->board->suspend_on_init && pdata->board->esd_check)
		hrtimer_cancel(&ts_esd_kthread_timer);
#endif

	if (!pdata->controller->ps_suspend(pdata)) {

		pdata->tpm_status = TPM_DESUSPEND;
		return 0;
	}
	if (ts_get_mode(pdata, TSMODE_GESTURE_STATUS)) {
        if(pdata->gesture_enable == 1) {
            TS_INFO("entry:tp_gesture_suspend\n");
            if (enable_irq_wake(pdata->irq)) {
                TS_INFO("enable_irq_wake(irq:%d) fail", pdata->irq);
            }
            pdata->gesture_state = TPG_ON;
			if (pdata->controller->gesture_suspend)
			   pdata->controller->gesture_suspend(pdata->controller);
			pdata->tpm_status = TPM_SUSPENDED;
			ts_clear_points(pdata);
			return 0;
        }

    }


	if (pdata->tpm_status == TPM_SUSPENDED)
	{
		return 0 ;
	}
	pdata->tpm_status = TPM_SUSPENDING ;

	/* turn off polling if it's on */
	if (ts_get_mode(pdata, TSMODE_POLLING_MODE)
		&& ts_get_mode(pdata, TSMODE_POLLING_STATUS))
		ts_enable_polling(pdata, false);

	/* disable irq if work in irq mode */
	if (ts_get_mode(pdata, TSMODE_IRQ_MODE)
		&& ts_get_mode(pdata, TSMODE_IRQ_STATUS))
		ts_enable_irq(pdata, false);

	/* notify controller if it has requested */
	/* TODO: pending suspend request if we're busy upgrading firmware */
	if (ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)
		&& ts_get_mode(pdata, TSMODE_CONTROLLER_STATUS)
		&& pdata->controller->handle_event){
		pdata->controller->handle_event(pdata->controller, TSEVENT_SUSPEND, NULL);
	}

	/* clear report data */
	ts_clear_points(pdata);

	if (ts_get_mode(pdata, TSMODE_PS_STATUS) || ts_get_mode(pdata, TSMODE_SENSORHUB_STATUS))
		pdata->tpd_prox_old_state = 0x0f;

	if (!g_pdata->usb_plug_state)//如果USB已经拨出，重新唤醒时，不需要下命令，固件默认已经是拨出状态
		g_pdata->usb_plug_state = 2;

	TS_INFO("tp suspend end...");
	pdata->tpm_status = TPM_SUSPENDED ;

	return 0;
}

/*
 * resume and turn on controller
 * TODO change to async way
 */
static int ts_resume(struct platform_device *pdev)
{
	struct ts_data *pdata = platform_get_drvdata(pdev);

	TS_INFO("tp resume start...");

#ifdef TS_ESD_SUPPORT_EN
	if (pdata->board->esd_check)
		hrtimer_start(&ts_esd_kthread_timer, ktime_set(3, 0), HRTIMER_MODE_REL);
#endif

	if (!pdata->controller->ps_resume(pdata))
	{
		pdata->tpm_status = TPM_ACTIVE ;
		return 0;
	}
	if (ts_get_mode(pdata, TSMODE_GESTURE_STATUS)) {
        if (pdata->gesture_state == TPG_ON) {
            printk("[TS] entry:tp_gesture_resume is on, so return !\n");
            if (disable_irq_wake(pdata->irq)) {
				TS_INFO("disable_irq_wake(irq:%d) fail", pdata->irq);
			}
			pdata->gesture_state = TPG_OFF;
			if (pdata->controller->gesture_resume)
				pdata->controller->gesture_resume(pdata->controller);
			pdata->tpm_status = TPM_ACTIVE ;
			return 0;
		}
	}

	if (pdata->tpm_status != TPM_SUSPENDED)
	{
		return 0 ;
	}
	pdata->tpm_status = TPM_RESUMING ;

	/* notify controller if it has requested */
	if (ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)
		&& ts_get_mode(pdata, TSMODE_CONTROLLER_STATUS)
		&& pdata->controller->handle_event)
		pdata->controller->handle_event(pdata->controller,
		TSEVENT_RESUME, NULL);

	/* turn on polling if needed */
	if (ts_get_mode(pdata, TSMODE_POLLING_MODE)
		&& !ts_get_mode(pdata, TSMODE_POLLING_STATUS))
		ts_enable_polling(pdata, true);

	/* enable irq if needed */
	if (ts_get_mode(pdata, TSMODE_IRQ_MODE)
		&& !ts_get_mode(pdata, TSMODE_IRQ_STATUS))
		ts_enable_irq(pdata, true);

	if (ts_get_mode(pdata, TSMODE_PS_STATUS) || ts_get_mode(pdata, TSMODE_SENSORHUB_STATUS))
		pdata->tpd_prox_old_state = 0x0f;

	//ts_usb_plug_switch();
	pdata->tpm_status = TPM_ACTIVE ;
	TS_INFO("tp resume end...");
	return 0;
}
/*
static void ts_resume_worker(struct work_struct *work)
{
	struct ts_data *pdata = container_of(work, struct ts_data, resume_work);
	ts_resume(pdata->pdev);
}*/

static void ts_notifier_worker(struct work_struct *work)
{
	struct ts_data *pdata = container_of(work, struct ts_data, notifier_work);
	/* notify controller if it has requested */
	if (ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)
		&& ts_get_mode(pdata, TSMODE_CONTROLLER_STATUS)
		&& pdata->controller->handle_event) {

		if (ts_get_mode(pdata, TSMODE_NOISE_STATUS)) {
			ts_set_mode(pdata, TSMODE_NOISE_STATUS, false);
			pdata->usb_plug_state = 1;
			TS_INFO("trying to open hardware anti-noise algorithm...");
			if (pdata->tpm_status == TPM_SUSPENDED) {
				TS_INFO("usb plugin suspend, delay send cmd return \n");
				return;
			}

			pdata->controller->handle_event(pdata->controller, TSEVENT_NOISE_HIGH, NULL);
		} else {
			TS_INFO("closing hardware anti-noise algorithm...");
			pdata->usb_plug_state = 0;
			if (pdata->tpm_status == TPM_SUSPENDED) {
				TS_INFO("usb plugout suspend, delay send cmd return \n");
				return;
			}

			pdata->controller->handle_event(pdata->controller, TSEVENT_NOISE_NORMAL, NULL);
		}
	}
}


/*
 * handle external events
 */
static void ts_ext_event_handler(struct ts_data *pdata, enum ts_event event, void *data)
{
	mutex_lock(&ats_tp_lock);
	TS_INFO("event=%d", event);
	switch (event) {
	case TSEVENT_SUSPEND:
		ts_suspend(pdata->pdev, PMSG_SUSPEND);
		break;
	case TSEVENT_RESUME:
/*		if (ts_get_mode(pdata, TSMODE_WORKQUEUE_STATUS)){
			queue_work(pdata->workqueue, &pdata->resume_work);
		}
		else*/
			ts_resume(pdata->pdev);
		break;
	case TSEVENT_NOISE_HIGH:
		ts_set_mode(pdata, TSMODE_NOISE_STATUS, true);
	case TSEVENT_NOISE_NORMAL:
		queue_work(pdata->notifier_workqueue, &pdata->notifier_work);
		break;
	default:
		TS_WARN("ignore unknown event: %d", event);
		break;
	}
	mutex_unlock(&ats_tp_lock);
}

static ssize_t ts_hardware_reset_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));

	ts_reset_controller(pdata, true);

	return count;
}

static struct device_attribute dev_attr_hardware_reset = {
	.attr = {
		.name = "hardware_reset",
		.mode = S_IWUSR,
	},
	.show = NULL,
	.store = ts_hardware_reset_store,
};

static ssize_t show_ts_gesture(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	char *ptr = buf;
	ptr += sprintf(ptr, "%d\n", g_pdata->gesture_enable);
	return (ptr-buf);
}
static ssize_t store_ts_gesture(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf[0] == '0') {
		//gesture off
		printk("gesture off\n");
		g_pdata->gesture_enable = 0;

	} else if (buf[0] == '1') {
		//gesture on
		printk("gesture on\n");
		g_pdata->gesture_enable = 1;
	}
	return count;
}

static struct device_attribute dev_attr_gesture_info = {
        .attr = {
				.name = "dev_gesture",
				.mode = S_IWUSR | S_IRUGO,
		},
		.show = show_ts_gesture,
		.store = store_ts_gesture,
};


static ssize_t ts_firmware_upgrade_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));
	char name[128] = { 0 };

	count--;

	if ((count > 1) && (count < 128)) {
		memcpy(name, buf, count);
		/* upgrading with designated firmware file */
		ts_request_firmware_upgrade(pdata, name, true);
	} else if (count == 1) {
		ts_request_firmware_upgrade(pdata, NULL, buf[0] == 'f');
	}

	return count + 1;
}

static struct device_attribute dev_attr_firmware_upgrade = {
	.attr = {
		.name = "firmware_upgrade",
		.mode = S_IWUSR,
	},
	.show = NULL,
	.store = ts_firmware_upgrade_store,
};

/*
 * read a register value from an address designated in store() func
 */
static ssize_t ts_register_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));
	int size = 0, ret = 0;
	unsigned char data[1];

	if (!ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)
		|| !ts_get_mode(pdata, TSMODE_CONTROLLER_STATUS)) {
		size += sprintf(buf, "No controller exist or controller busy!\n");
	} else if (pdata->stashed_reg > 0xFFFF) {
		size += sprintf(buf, "Invalid register address: %d\n", pdata->stashed_reg);
	} else if (pdata->stashed_reg > 0xFF
		&& ((pdata->controller->config & TSCONF_ADDR_WIDTH_MASK) == TSCONF_ADDR_WIDTH_BYTE)) {
		size += sprintf(buf, "Register address out of range: 0x%04X\n", pdata->stashed_reg);
	} else {
		ret = ts_reg_read(pdata->stashed_reg, data, 1);
		if (ret != 1) {
			size += sprintf(buf, "Read error!\n");
		} else {
			if ((pdata->controller->config & TSCONF_ADDR_WIDTH_MASK) == TSCONF_ADDR_WIDTH_BYTE)
				size += sprintf(buf, "Address=0x%02X, Val=0x%02X\n",
					pdata->stashed_reg, data[0]);
			else
				size += sprintf(buf, "Address=0x%04X, Val=0x%02X\n",
					pdata->stashed_reg, data[0]);
		}
	}

	return size;
}

/*
 * store user input value for read next time when show() called
 * checks are done in show() func
 */
static ssize_t ts_register_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));
	int value = 0;

	if (kstrtoint(buf, 16, &value) < 0) {
		TS_WARN("fail to convert \"%s\" to integer", buf);
	} else {
		TS_INFO("receive register address: %d", value);
		pdata->stashed_reg = value;
	}

	return count;
}

static struct device_attribute dev_attr_register = {
	.attr = {
		.name = "register",
		.mode = S_IWUSR | S_IRUGO,
	},
	.show = ts_register_show,
	.store = ts_register_store,
};

static ssize_t ts_input_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", ATS_INPUT_DEV);
}

static struct device_attribute dev_attr_input_name = {
	.attr = {
		.name = "input_name",
		.mode = S_IRUGO,
	},
	.show = ts_input_name_show,
	.store = NULL,
};

static ssize_t ts_ui_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));
	int size = 0, i;

	size += sprintf(buf + size, "\n======Current Setting======\n");
	size += sprintf(buf + size, "Report area: %u x %u\n",
		pdata->width, pdata->height);
	if (ts_get_mode(pdata, TSMODE_VKEY_REPORT_ABS)) {
		size += sprintf(buf + size, "Virtual key reported as KEY event");
	} else {
		for (i = 0; i < pdata->vkey_count; i++) {
			size += sprintf(buf + size,
				"Key: %s(%u) --- (%u, %u), width = %u, height = %u\n",
				ts_get_keyname(pdata->vkey_list[i].keycode),
				pdata->vkey_list[i].keycode,
				pdata->vkey_list[i].x, pdata->vkey_list[i].y,
				pdata->vkey_list[i].width, pdata->vkey_list[i].height);
		}
	}
	size += sprintf(buf + size, "======Current Setting======\n");

	size += sprintf(buf + size, "\n======From DTS======\n");
	size += sprintf(buf + size, "LCD size: %u x %u\n",
		pdata->board->lcd_width, pdata->board->lcd_height);
	size += sprintf(buf + size, "Surface area: %u x %u\n",
		pdata->board->surface_width, pdata->board->surface_height);
	size += sprintf(buf + size, "Report area: %u x %u\n",
		pdata->board->panel_width, pdata->board->panel_height);
	for (i = 0; i < pdata->board->virtualkey_count; i++) {
		size += sprintf(buf + size,
			"Key: %s(%u) --- (%u, %u), width = %u, height = %u\n",
			ts_get_keyname(pdata->board->virtualkeys[i].keycode),
			pdata->board->virtualkeys[i].keycode,
			pdata->board->virtualkeys[i].x,
			pdata->board->virtualkeys[i].y,
			pdata->board->virtualkeys[i].width,
			pdata->board->virtualkeys[i].height);
	}
	size += sprintf(buf + size, "======From DTS======\n");

	return size;
}

static struct device_attribute dev_attr_ui_info = {
	.attr = {
		.name = "ui_info",
		.mode = S_IRUGO,
	},
	.show = ts_ui_info_show,
	.store = NULL,
};

/* show detailed controller info, including those declared by itself */
static ssize_t ts_controller_detail_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));
	struct ts_controller *c = NULL;
	int size = 0, i;
	char value[1] = { 0 };

	if (!ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST))
		return sprintf(buf, "Controller doesn't exist!\n");

	c = pdata->controller;
	if (!ts_get_mode(pdata, TSMODE_CONTROLLER_STATUS))
		return sprintf(buf, "Controller is busy!\n");

	size += sprintf(buf + size, "\nBacis Properties:\n");
	size += sprintf(buf + size, ">  name: %s\n", c->name);
	size += sprintf(buf + size, ">  vendor: %s\n", c->vendor);

	size += sprintf(buf + size, "\nUI Properties:\n");
	size += sprintf(buf + size, ">  resolution: %u x %u\n", c->panel_width, c->panel_height);
	for (i = 0; i < c->virtualkey_count; i++)
		size += sprintf(buf + size, ">  virtualkey[%d]: (%u, %u) --- %s (0x%X)\n",
			i + 1, c->virtualkeys[i].x, c->virtualkeys[i].y,
			ts_get_keyname(c->virtualkeys[i].keycode), c->virtualkeys[i].keycode);

	size += sprintf(buf + size, "\nBehaviors:\n");
	if ((c->config & TSCONF_REPORT_TYPE_MASK) == TSCONF_REPORT_TYPE_1)
		size += sprintf(buf + size, ">  report_type: 1(Type-A)\n");
	else if ((c->config & TSCONF_REPORT_TYPE_MASK) == TSCONF_REPORT_TYPE_2)
		size += sprintf(buf + size, ">  report_type: 2(Type-B)\n");
	else
		size += sprintf(buf + size, ">  report_type: 3\n");

	if ((c->config & TSCONF_REPORT_MODE_MASK) == TSCONF_REPORT_MODE_IRQ)
		size += sprintf(buf + size, ">  irq_support: true\n");
	else
		size += sprintf(buf + size, ">  irq_support: false\n");

	if ((c->config & TSCONF_POWER_ON_RESET_MASK) == TSCONF_POWER_ON_RESET)
		size += sprintf(buf + size, ">  power_on_reset: true\n");
	else
		size += sprintf(buf + size, ">  power_on_reset: false\n");

	size += sprintf(buf + size, "\nRegister Values:\n");
	for (i = 0; i < c->register_count; i++) {
		if (1 == ts_reg_read(c->registers[i].reg, value, 1))
			size += sprintf(buf + size, ">  %s: 0x%02X\n",
				c->registers[i].name, value[0]);
		else
			size += sprintf(buf + size, ">  %s: read error!\n",
				c->registers[i].name);
	}

	return size;
}

static struct device_attribute dev_attr_controller_info = {
	.attr = {
		.name = "chip_info",
		.mode = S_IRUGO,
	},
	.show = ts_controller_detail_info_show,
	.store = NULL,
};

static ssize_t ts_controller_basic_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));
	struct ts_controller *c = NULL;
	int i;
	char value_buf[2];

	if (!ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST))
		return sprintf(buf, "Controller doesn't exist!\n");

	c = pdata->controller;

#define __CHECK_ATTR_NAME(s) \
	!strncmp(attr->attr.name, s, min(strlen(s), strlen(attr->attr.name)))
#define __CHECK_DESC(s) \
	!strncmp(c->registers[i].name, s, min(strlen(s), strlen(c->registers[i].name)))

	if (__CHECK_ATTR_NAME("chip_id")) {
		if (!ts_get_mode(pdata, TSMODE_CONTROLLER_STATUS))
			return sprintf(buf, "Controller is busy!\n");

		for (i = 0; i < c->register_count; i++) {
			if (__CHECK_DESC(TSREG_CHIP_ID)) {
				if (1 == ts_reg_read(c->registers[i].reg, value_buf, 1))
					return sprintf(buf, "chip id: 0x%02X\n", value_buf[0]);
				else
					return sprintf(buf, "Read error!\n");
			}
		}

		return sprintf(buf, "chip id: (null)\n");
	} else if (__CHECK_ATTR_NAME("firmware_version")) {
		if (!ts_get_mode(pdata, TSMODE_CONTROLLER_STATUS))
			return sprintf(buf, "Controller is busy!\n");

		for (i = 0; i < c->register_count; i++) {
			if (__CHECK_DESC(TSREG_FW_VER)) {
				if (1 == ts_reg_read(c->registers[i].reg, value_buf, 1))
					return sprintf(buf, "firmware version: 0x%02X\n", value_buf[0]);
				else
					return sprintf(buf, "Read error!\n");
			}
		}

		return sprintf(buf, "firmware version: (null)\n");

	} else if (__CHECK_ATTR_NAME("chip_name")) {
		return sprintf(buf, "chip name: %s\n", c->name);
	} else if (__CHECK_ATTR_NAME("vendor_name")) {
		return sprintf(buf, "vendor name: %s\n", c->vendor);
	}
	return 0;
}

static struct device_attribute dev_attr_chip_id = {
	.attr = {
		.name = "chip_id",
		.mode = S_IRUGO,
	},
	.show = ts_controller_basic_info_show,
	.store = NULL,
};

static struct device_attribute dev_attr_firmware_version = {
	.attr = {
		.name = "firmware_version",
		.mode = S_IRUGO,
	},
	.show = ts_controller_basic_info_show,
	.store = NULL,
};

static struct device_attribute dev_attr_chip_name = {
	.attr = {
		.name = "chip_name",
		.mode = S_IRUGO,
	},
	.show = ts_controller_basic_info_show,
	.store = NULL,
};

static struct device_attribute dev_attr_vendor_name = {
	.attr = {
		.name = "vendor_name",
		.mode = S_IRUGO,
	},
	.show = ts_controller_basic_info_show,
	.store = NULL,
};

static ssize_t ts_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));
	int size = 0;

	size += sprintf(buf, "Status code: 0x%lX\n\n", pdata->status);

	size += sprintf(buf + size, "controller            : %s\n",
		ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST) ? "exist" : "not found");

	size += sprintf(buf + size, "polling               : %s\n",
		ts_get_mode(pdata, TSMODE_POLLING_MODE) ? "enabled" : "not enabled");

	size += sprintf(buf + size, "polling status        : %s\n",
		ts_get_mode(pdata, TSMODE_POLLING_STATUS) ? "working" : "stopped");

	size += sprintf(buf + size, "interrupt             : %s\n",
		ts_get_mode(pdata, TSMODE_IRQ_MODE) ? "enabled" : "not enabled");

	size += sprintf(buf + size, "interrupt status      : %s\n",
		ts_get_mode(pdata, TSMODE_IRQ_STATUS) ? "working" : "stopped");

	size += sprintf(buf + size, "suspend status        : %s\n",
		(pdata->tpm_status == TPM_SUSPENDED) ? "suspend" : "no suspend");

	size += sprintf(buf + size, "controller status     : %s\n",
		ts_get_mode(pdata, TSMODE_CONTROLLER_STATUS) ? "available" : "busy");

	size += sprintf(buf + size, "virtual key report    : %s\n",
		ts_get_mode(pdata, TSMODE_VKEY_REPORT_ABS) ? "abs" : "key");

	size += sprintf(buf + size, "firmware auto upgrade : %s\n",
		ts_get_mode(pdata, TSMODE_AUTO_UPGRADE_FW) ? "enabled" : "not enabled");

	size += sprintf(buf + size, "workqueue status      : %s\n",
		ts_get_mode(pdata, TSMODE_WORKQUEUE_STATUS) ? "enabled" : "not enabled");

	size += sprintf(buf + size, "noise status      : %s\n",
		ts_get_mode(pdata, TSMODE_NOISE_STATUS) ? "enabled" : "not enabled");

	size += sprintf(buf + size, "print UP & DOWN       : %s\n",
		ts_get_mode(pdata, TSMODE_DEBUG_UPDOWN) ? "enabled" : "not enabled");

	size += sprintf(buf + size, "print raw data        : %s\n",
		ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA) ? "enabled" : "not enabled");

	size += sprintf(buf + size, "print irq time        : %s\n",
		ts_get_mode(pdata, TSMODE_DEBUG_IRQTIME) ? "enabled" : "not enabled");

	return size;
}

static ssize_t ts_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));
	int cmd_length = 0, cmd = 0;
	char *c;

	count--; /* ignore '\n' in the end of line */

	c = strchr(buf, ' ');
	cmd_length = c ? c - buf : count;

	if (!strncmp(buf, "irq_enable", min(cmd_length, 10))) {
		if (count  == cmd_length + 2) {
			cmd = buf[count - 1] - '0';
			TS_DBG("irq_enable, cmd = %d", cmd);
			ts_enable_irq(pdata, !!cmd);
		}
	} else if (!strncmp(buf, "raw_data", min(cmd_length, 8))) {
		if (count == cmd_length + 2) {
			cmd = buf[count - 1] - '0';
			TS_DBG("raw_data, cmd = %d", cmd);
			ts_set_mode(pdata, TSMODE_DEBUG_RAW_DATA, !!cmd);
		}
	} else if (!strncmp(buf, "up_down", min(cmd_length, 7))) {
		if (count == cmd_length + 2) {
			cmd = buf[count - 1] - '0';
			TS_DBG("up_down, cmd = %d", cmd);
			ts_set_mode(pdata, TSMODE_DEBUG_UPDOWN, !!cmd);
		}
	} else if (!strncmp(buf, "irq_time", min(cmd_length, 8))) {
		if (count == cmd_length + 2) {
			cmd = buf[count - 1] - '0';
			TS_DBG("irq_time, cmd = %d", cmd);
			ts_set_mode(pdata, TSMODE_DEBUG_IRQTIME, !!cmd);
		}
	} else {
		TS_DBG("unrecognized cmd");
	}

	return count + 1;
}

static struct device_attribute dev_attr_mode = {
	.attr = {
		.name = "mode",
		.mode = S_IWUSR | S_IRUGO,
	},
	.show = ts_mode_show,
	.store = ts_mode_store,
};

static ssize_t ts_debug_level_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ts_dbg_level);
}

static ssize_t ts_debug_level_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (count == 2 && buf[0] >= '0' && buf[0] <= '1')
		ts_dbg_level = buf[0] - '0';

	return count;
}

static struct device_attribute dev_attr_debug_level = {
	.attr = {
		.name = "debug_level",
		.mode = S_IWUSR | S_IRUGO,
	},
	.show = ts_debug_level_show,
	.store = ts_debug_level_store,
};

static ssize_t ts_suspend_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "%s\n", (pdata->tpm_status == TPM_SUSPENDED) ? "true" : "false");
}

#ifdef CONFIG_DSP_NOTIFIER
static ssize_t ts_suspend_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}
#else
static ssize_t ts_suspend_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));

	//TS_INFO("buf=%s !!!", buf);
	mutex_lock(&ats_tp_lock);
	if ((buf[0] == '1') && pdata->tpm_status == TPM_ACTIVE)
		ts_suspend(to_platform_device(dev), PMSG_SUSPEND);
	else if ((buf[0] == '0') && pdata->tpm_status == TPM_SUSPENDED)
		ts_resume(to_platform_device(dev));
	mutex_unlock(&ats_tp_lock);
	return count;
}
#endif
static DEVICE_ATTR_RW(ts_suspend);

static ssize_t ts_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));
	int size = 0;
	char *tpm_status_str[5] = {
	"TPM_ACTIVE",
	"TPM_RESUMING",
	"TPM_SUSPENDED",
	"TPM_SUSPENDING",
	"TPM_DESUSPEND"
	};
	char *tps_status_str[3] = {
	"TPS_OFF",
	"TPS_ON",
	"TPS_DEON"
	};

	size += sprintf(buf, "ps status : %s\n", tps_status_str[pdata->tps_status]);
	size += sprintf(buf + size, "pm status : %s\n", tpm_status_str[pdata->tpm_status]);
	return size;
}

static struct device_attribute dev_attr_status = {
	.attr = {
		.name = "status_show",
		.mode = S_IRUGO,
	},
	.show = ts_status_show,
};


static struct attribute *ts_debug_attrs[] = {
	&dev_attr_debug_level.attr,
	&dev_attr_mode.attr,
	&dev_attr_register.attr,
	&dev_attr_firmware_upgrade.attr,
	&dev_attr_hardware_reset.attr,
	&dev_attr_input_name.attr,
	&dev_attr_chip_id.attr,
	&dev_attr_firmware_version.attr,
	&dev_attr_chip_name.attr,
	&dev_attr_vendor_name.attr,
	&dev_attr_controller_info.attr,
	&dev_attr_ui_info.attr,
	&dev_attr_ts_suspend.attr,
	&dev_attr_gesture_info.attr,
	&dev_attr_status.attr,
	NULL,
};

static struct attribute_group ts_debug_attr_group = {
	.attrs = ts_debug_attrs,
};

static ssize_t ts_virtualkey_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct ts_data *pdata = container_of(attr, struct ts_data, vkey_attr);
	ssize_t size = 0;
	unsigned int i;

	if (pdata->board->lcd_width > 0) {
		for (i = 0; i < pdata->vkey_count; i++) {
			size += sprintf(buf + size, "%s:%u:%u:%u:%u:%u:",
				__stringify(ANDROID_KEYMAP_VERSION),
				pdata->vkey_list[i].keycode,
				pdata->vkey_list[i].x * pdata->board->lcd_width / pdata->width,
				pdata->vkey_list[i].y * pdata->board->lcd_height / pdata->height,
				pdata->vkey_list[i].width * pdata->board->lcd_width / pdata->width,
				pdata->vkey_list[i].height * pdata->board->lcd_height / pdata->height);
		}
	} else {
		for (i = 0; i < pdata->vkey_count; i++) {
			size += sprintf(buf + size, "%s:%u:%u:%u:%u:%u:",
				__stringify(ANDROID_KEYMAP_VERSION),
				pdata->vkey_list[i].keycode,
				pdata->vkey_list[i].x,
				pdata->vkey_list[i].y,
				pdata->vkey_list[i].width,
				pdata->vkey_list[i].height);
		}
	}


	if (size > 0)
		buf[size - 1] = '\n';

/*	printk("[TS] %s:%s:%d:%d:%d:%d:%s:%s:%d:%d:%d:%d:%s:%s:%d:%d:%d:%d\n",
			__stringify(EV_KEY), __stringify(KEY_APPSELECT), pdata->vkey_list[0].x, pdata->vkey_list[0].y, pdata->vkey_list[0].width, pdata->vkey_list[0].height
			,__stringify(EV_KEY), __stringify(KEY_HOMEPAGE), pdata->vkey_list[1].x, pdata->vkey_list[1].y, pdata->vkey_list[1].width, pdata->vkey_list[1].height
			,__stringify(EV_KEY), __stringify(KEY_BACK), pdata->vkey_list[2].x, pdata->vkey_list[2].y, pdata->vkey_list[2].width, pdata->vkey_list[2].height);
*/
	return size;
}

static struct attribute *ts_virtualkey_attrs[] = {
	NULL,
	NULL,
};

static struct attribute_group ts_virtualkey_attr_group = {
	.attrs = ts_virtualkey_attrs,
};

static int ts_filesys_create(struct ts_data *pdata)
{
	int retval;
	struct kobj_attribute *attr;

	/* create sysfs virtualkey files */
	if (ts_get_mode(pdata, TSMODE_VKEY_REPORT_ABS) && pdata->vkey_list) {
		attr = &pdata->vkey_attr;
		attr->attr.name = "virtualkeys.adaptive_ts";
		attr->attr.mode = S_IRUGO;
		attr->show = ts_virtualkey_show;
		/* init attr->key to static to prevent kernel warning */
		sysfs_attr_init(&attr->attr);
		ts_virtualkey_attrs[0] = &attr->attr;

		pdata->vkey_obj = kobject_create_and_add("board_properties", NULL);
		if (IS_ERR_OR_NULL(pdata->vkey_obj)) {
			TS_ERR("Fail to create kobject!");
			return -ENOMEM;
		}
		retval = sysfs_create_group(pdata->vkey_obj, &ts_virtualkey_attr_group);
		if (retval < 0) {
			TS_ERR("Fail to create virtualkey files!");
			kobject_put(pdata->vkey_obj);
			return -ENOMEM;
		}
		TS_DBG("virtualkey sysfiles created");
	}

	/* create sysfs debug files	*/
	retval = sysfs_create_group(&pdata->pdev->dev.kobj,
		&ts_debug_attr_group);
	if (retval < 0) {
		TS_ERR("Fail to create debug files!");
		return -ENOMEM;
	}

	/* convenient access to sysfs node */
	retval = sysfs_create_link(NULL, &pdata->pdev->dev.kobj, "touchscreen");
	if (retval < 0) {
		TS_ERR("Failed to create link!");
		return -ENOMEM;
	}

	return 0;
}

static void ts_filesys_remove(struct ts_data *pdata)
{
	if (ts_get_mode(pdata, TSMODE_VKEY_REPORT_ABS) && pdata->vkey_list) {
		sysfs_remove_group(pdata->vkey_obj, &ts_virtualkey_attr_group);
		kobject_put(pdata->vkey_obj);
	}

	sysfs_remove_link(NULL, "touchscreen");
	sysfs_remove_group(&pdata->pdev->dev.kobj, &ts_debug_attr_group);
}

static void ts_controller_select(void);
static int ts_probe(struct platform_device *pdev)
{
	int retval;
	int reset_count = 0;
	struct ts_data *pdata;

#ifdef CONFIG_TPD_SENSORHUB
	struct class *psensor_class;
	struct device *psensor_dev;
#endif
	pdata = devm_kzalloc(&pdev->dev, sizeof(struct ts_data), GFP_KERNEL);
	if (IS_ERR(pdata)) {
		TS_ERR("Failed to allocate platform data!");
		return -ENOMEM;
	}
	g_pdata = pdata;
	platform_set_drvdata(pdev, pdata);
	pdata->pdev = pdev;
	pdata->board = pdev->dev.platform_data;
	//wakeup_source_init(&pdata->upgrade_lock, ATS_WL_UPGRADE_FW);
	//wakeup_source_init(&ts_ps_wakelock, "ts_ps_wakelock");
	pdata->upgrade_lock = wakeup_source_create(ATS_WL_UPGRADE_FW);
	ts_ps_wakelock = wakeup_source_create("ts_ps_wakelock");
	wakeup_source_add(pdata->upgrade_lock);
	wakeup_source_add(ts_ps_wakelock);
	/* GPIO request first */
	retval = ts_request_gpio1(pdata);
	if (retval) {
		TS_ERR("Failed to request gpios!");
		return retval;
	}

	/* export GPIO for debug use */
	/*retval = ts_export_gpio(pdata);
	if (retval) {
		TS_ERR("failed to export gpio");
		return retval;
	}*/
	//controller select read tp id
	ts_controller_select();

	/* then we find which controller to use */
	pdata->controller = ts_match_controller(pdata->board->controller);
	if (pdata->controller != NULL) {
		pdata->controller->pdata = pdata;
		if (pdata->controller->custom_initialization)
			pdata->controller->custom_initialization();
	}

	TS_INFO("ps_status:%d, sensorhub_status:%d, upgrade_status:%d, upgrade_sw:%d",
		pdata->board->ps_status, pdata->board->sensorhub_status, pdata->board->upgrade_status, pdata->board->auto_upgrade_fw);

	mutex_init(&ats_tp_lock);

	if (pdata->controller) {
		ts_set_mode(pdata, TSMODE_CONTROLLER_EXIST, true);
		TS_INFO("selecting controller \"%s-%s\"", pdata->controller->vendor, pdata->controller->name);
		ts_data_init(pdata);
		ts_configure(pdata);
		retval = ts_open_controller(pdata);
		if (retval < 0) {
			TS_WARN("fail to open controller, maybe firmware is corrupted");
			if (ts_get_mode(pdata, TSMODE_UPGRADE_STATUS)) {
				reset_count = 0;
				while (++reset_count <= 3) {
					ts_reset_controller(pdata, true);
					//ret = pdata->controller->upgrade_firmware();
					TS_INFO("entry_upgrade_firmware");
					retval = pdata->controller->upgrade_status(pdata->controller);
						ts_reset_controller(pdata, true);
					if (!retval) {
						TS_ERR("upgrade-status failed");
						break;
					}
				}
			}

			if (ts_get_mode(pdata, TSMODE_AUTO_UPGRADE_FW))
				ts_request_firmware_upgrade(pdata, NULL, true);
			ts_set_mode(pdata, TSMODE_CONTROLLER_STATUS, true);
		} else if (ts_get_mode(pdata, TSMODE_AUTO_UPGRADE_FW)) {
			//ts_request_firmware_upgrade(pdata, NULL, false);
			if (pdata->controller->upgrade_status) {
				reset_count = 0;
				while (++reset_count <= 3) {
					ts_reset_controller(pdata, true);
					//ret = pdata->controller->upgrade_firmware();
					TS_INFO("entry_upgrade_firmware");
					retval = pdata->controller->upgrade_status(NULL);
						//ts_reset_controller(pdata, true);
					if (retval == 1) {
						TS_INFO("upgrade-status success");
						break;
					}
				}
			}
			ts_set_mode(pdata, TSMODE_CONTROLLER_STATUS, true);
		} else {
			ts_set_mode(pdata, TSMODE_CONTROLLER_STATUS, true);
		}
		if (pdata->board->suspend_on_init)
			ts_suspend(pdata->pdev, PMSG_SUSPEND);
	} else {
		TS_WARN("no matched controller found!");
	}

#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_ILI9882H_SPI)
	if (ilitek_get_device_name()) {//spi interforce already download,do not reset

	} else
#endif
	ts_reset_controller(pdata, true);

	/* next we create debug and virtualkey files */
	retval = ts_filesys_create(pdata);
	if (retval) {
		TS_ERR("Failed to create sys files.");
		return retval;
	}

	/* also we need to register input device */
	retval = ts_register_input_dev(pdata);
	if (retval) {
		TS_ERR("Failed to register input device.");
		return retval;
	}

	if (ts_get_mode(pdata, TSMODE_PS_STATUS)) {
		retval = misc_register(&tpd_pls_device);
		TS_DBG("entry:TSMODE_PS_STATUS register!!!");
		pdata->ps_input_dev = input_allocate_device();
		if (!pdata->ps_input_dev) {
			retval = -ENOMEM;
			TS_ERR("failed to allocate ps-input device");
			return retval;
		}
		pdata->ps_input_dev->name = "alps_pxy";
		//set_bit(EV_ABS, pdata->ps_input_dev->evbit);
		input_set_capability(pdata->ps_input_dev, EV_ABS, ABS_DISTANCE);
		input_set_abs_params(pdata->ps_input_dev, ABS_DISTANCE, 0, 1, 0, -1);
		retval = input_register_device(pdata->ps_input_dev);
		if (retval) {
			TS_ERR("failed to register ps-input device");
			return retval;
		}
	}

#ifdef CONFIG_TPD_SENSORHUB
	if (ts_get_mode(pdata, TSMODE_SENSORHUB_STATUS)) {
		psensor_class = class_create(THIS_MODULE, "sprd_sensorhub_tp");
		if (IS_ERR(psensor_class))
			TS_ERR("Failed to create class!\n");

		psensor_dev = device_create(psensor_class, NULL, 0, NULL, "device");
		if (IS_ERR(psensor_dev))
			TS_ERR("Failed to create device!\n");

		if (device_create_file(psensor_dev, &dev_attr_psensor_enable) < 0) // /sys/class/sprd_sensorhub_tp/device/psensor_enable
			TS_ERR("Failed to create device file(%s)!\n", dev_attr_psensor_enable.attr.name);

		if (device_create_file(psensor_dev, &dev_attr_psensor_flush) < 0) // /sys/class/sprd_sensorhub_tp/device/psensor_enable
			TS_ERR("Failed to create device file(%s)!\n", dev_attr_psensor_enable.attr.name);
	}
#endif

	if (pdata->controller->incell) {

		//register_incell_interface(&ts_incell_interface);
	}
	/* finally we're gonna report data */
	//setup_timer(&pdata->poll_timer, ts_poll_handler, (unsigned long)pdata);
	timer_setup(&pdata->poll_timer, ts_poll_handler, 0);
	pdata->poll_interval = TS_POLL_INTERVAL;

	if (ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)) {
		/* prefer to use irq if supported */
		if ((pdata->controller->config & TSCONF_REPORT_MODE_MASK)
			== TSCONF_REPORT_MODE_IRQ) {
			pdata->irq = gpio_to_irq(pdata->board->int_gpio);
			if (likely(pdata->irq > 0) && !ts_isr_control(pdata, true))//ÖÐ¶Ï
				TS_DBG("works in interrupt mode, irq=%d", pdata->irq);
		}

		/* if not supported, fallback to polling mode */
		if (!ts_get_mode(pdata, TSMODE_IRQ_MODE)) {
			ts_poll_control(pdata, true);
			TS_DBG("works in polling mode");
		}
	}

	if (ts_get_mode(pdata, TSMODE_GESTURE_STATUS)) {
		TS_ERR("entry:tp_gesture_init");
		if (pdata->controller->gesture_init)
			pdata->controller->gesture_init(pdata->controller);
#ifdef CONFIG_TRANSSION_GESTURE
		transsion_gesture_register(&gif);
#endif
		//wakeup_source_init(&ts_gesture_wakelock, "ts_gesture_wakelock");
		ts_gesture_wakelock = wakeup_source_create("ts_gesture_wakelock");
		wakeup_source_add(ts_gesture_wakelock);
	}
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_ILI9882H_SPI)//spi 接口要在中断注册后再读取TP相关信息，否则会读取不到，因为读取信息时需要中断配合
	if (pdata->controller->get_tp_info)
		pdata->controller->get_tp_info();
#endif
	/* use workqueue to resume device async */
/*	INIT_WORK(&pdata->resume_work, ts_resume_worker);
	pdata->workqueue = create_singlethread_workqueue(ATS_WORKQUEUE);
	if (IS_ERR(pdata->workqueue)) {
		TS_WARN("failed to create workqueue!");
		ts_set_mode(pdata, TSMODE_WORKQUEUE_STATUS, false);
	} else {
		ts_set_mode(pdata, TSMODE_WORKQUEUE_STATUS, true);
	}*/

	/* use notifier_workqueue to usb notifier device async */
	INIT_WORK(&pdata->notifier_work, ts_notifier_worker);
	pdata->notifier_workqueue = create_singlethread_workqueue(ATS_NOTIFIER_WORKQUEUE);
	if (IS_ERR(pdata->notifier_workqueue)) {
		retval = -ESRCH;
		TS_ERR("failed to create notifier_workqueue!");
		return retval;
	}

#ifdef TS_ESD_SUPPORT_EN
	if (pdata->board->esd_check) {
	/* esd issue: i2c monitor thread */
		ktime_t ktime = ktime_set(30, 0);

		hrtimer_init(&ts_esd_kthread_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts_esd_kthread_timer.function = ts_esd_kthread_hrtimer_func;
		hrtimer_start(&ts_esd_kthread_timer, ktime, HRTIMER_MODE_REL);
		kthread_run(ts_esd_checker_handler, 0, "ts_esd_helper");
		TS_INFO("ESD check init !!!");
	}
#endif
	/* register external events */
	retval = ts_register_ext_event_handler(pdata, ts_ext_event_handler);
	if (retval < 0)
		TS_WARN("error in register external event!");

#ifdef CONFIG_TRANSSION_HWINFO
	transsion_hwinfo_add("touch_vendor", (char *)g_pdata->vendor_string);
	transsion_hwinfo_add("touch_firmware_version", g_pdata->firmwork_version);
	transsion_hwinfo_add("touch_ic", g_pdata->chip_name);
	transsion_hwinfo_add("touch_firmware_update", g_pdata->firmware_update_switch ? "1":"0");
	TS_INFO(" firmwork_version:%s, chip_name:%s", g_pdata->firmwork_version, g_pdata->chip_name);
#endif

	TS_INFO("ts platform device probe OK");
	return 0;
}

static int ts_remove(struct platform_device *pdev)
{
	struct ts_data *pdata = platform_get_drvdata(pdev);

	ts_unregister_ext_event_handler();
/*
	if (ts_get_mode(pdata, TSMODE_WORKQUEUE_STATUS)) {
		cancel_work_sync(&pdata->resume_work);
		destroy_workqueue(pdata->workqueue);
	}
*/
	cancel_work_sync(&pdata->notifier_work);
	destroy_workqueue(pdata->notifier_workqueue);

	ts_filesys_remove(pdata);

	ts_close_controller(pdata);

	pdata->status = 0;

#ifdef TS_ESD_SUPPORT_EN
	if (pdata->board->esd_check)
		hrtimer_cancel(&ts_esd_kthread_timer);
#endif
	wakeup_source_remove(pdata->upgrade_lock);
	wakeup_source_destroy(pdata->upgrade_lock);
	wakeup_source_remove(ts_ps_wakelock);
	wakeup_source_destroy(ts_ps_wakelock);
	//wakeup_source_trash(pdata->upgrade_lock);
	//wakeup_source_trash(ts_ps_wakelock);

	return 0;
}

static struct platform_driver ats_driver = {
	.driver = {
		.name = ATS_PLATFORM_DEV,
		.owner = THIS_MODULE,
	},
	.probe = ts_probe,
	.remove = ts_remove,
#if TS_USE_LEGACY_SUSPEND
	.suspend = ts_suspend,
	.resume = ts_resume,
#endif
};
static void ts_controller_select(void)
{
	int i = 0;
    int j = 0;
	int chipid = 0;
    int verdor_num = 0;
	int force_matchid = -1 ;
    const char *match_lcdname = NULL ;
    struct lcd_match_info *pmatch_info = NULL;
    enum vendor_match_status match_flag = NO_MATCH;
	for (i = 0; i < ARRAY_SIZE(supported_verdor); i++) {

		if(supported_verdor[i].read_chipid != NULL) {
			if (supported_verdor[i].force_match == 1)
				force_matchid = i ;
			chipid = supported_verdor[i].read_chipid();
			if (chipid == supported_verdor[i].chipid) {

				verdor_num = i ;
				match_flag = CHIPID_MATCH ;
				break;
			}
		}
		else {

			if( force_matchid == -1) {

				verdor_num = i ;
				match_flag = NO_MATCH ;
			}
			else {

				verdor_num = force_matchid;
				match_flag = FORCE_MATCH ;
			}
		}
	}
	TS_ERR("ts_controller_select match_flag=%x", match_flag);
	if(match_flag == NO_MATCH) {

		match_lcdname = "NT36XXX";//sprd_get_lcd_name();
		TS_INFO("chip id match failed, lcdname[%s] match start\n" ,match_lcdname);
		for (i = 0; i < ARRAY_SIZE(supported_verdor); i++) {


		if (supported_verdor[i].match_info != NULL) {

		pmatch_info = supported_verdor[i].match_info;
			for(j = 0 ; j < supported_verdor[i].match_info_count; j++) {

				TS_INFO("lcdname[%s] match to name[%s]\n", match_lcdname , pmatch_info[j].lcdname);
				if(!strcmp(pmatch_info[j].lcdname, match_lcdname)) {

					verdor_num = i ;
					supported_verdor[i].vendorid = pmatch_info[j].vendorid ;
					match_flag = LCDNAME_MATCH ;
					break;
				}
			}
		}
		if (match_flag == LCDNAME_MATCH)
			break;
		verdor_num = i;
		match_flag = NO_MATCH;
		}
	}
	supported_verdor[verdor_num].match_status = match_flag;
	strcat(&g_chipid_name[0], supported_verdor[verdor_num].vendor);
	TS_ERR("%s,verdor_num=%x\n", g_chipid_name,verdor_num);
	supported_verdor[verdor_num].verdor_init();
	ts_controller_setname(supported_verdor[verdor_num].vendor, supported_verdor[verdor_num].name);
	g_pdata->vendor_cfg = &supported_verdor[verdor_num];
	TS_ERR("ts_controller_select match_flag=%x", match_flag);
}
static int get_bootargs(char *current_mode, char *boot_param)
{
	struct device_node *np;
	const char *cmd_line;
	char *s = NULL;

	int ret = 0;
	np = of_find_node_by_path("/chosen");

	if (!np) {
		printk(KERN_ERR "Can't get the /chosen\n");
		return -EIO;
	}

	ret = of_property_read_string(np, "bootargs", &cmd_line);
	if (ret < 0) {
		printk(KERN_ERR "Can't get the bootargs\n");
		return ret;
	}

	s = strstr(cmd_line, boot_param);

	if(s){
		s += (sizeof(boot_param) + 1);
		while(*s != ' ')
			*current_mode++ = *s++;
		*current_mode = '\0';
	} else {
		printk(KERN_ERR "Read bootargs %s fail\n",boot_param);
		return 1;
	}
	return 0;
}
static int __init ts_init(void)
{
	int retval;
	retval = get_bootargs(lcd_name,"lcd_name");
	if((!retval) && (!strstr(lcd_name, LCD_NAME))){
		TS_ERR("%s: match %s fail,return",__func__,lcd_name);
		return 0;
	}
	retval = ts_board_init();
	if (retval) {
		TS_ERR("board init failed!");
		return retval;
	}
	TS_ERR("ts_init init 1!");
	retval = platform_driver_register(&ats_driver);
	TS_ERR("ts_init retval=%x ",retval);
	return retval;//platform_driver_register(&ats_driver);
}

/* default cmd interface(refer to sensor HAL):"/sys/class/sprd-tpd/device/proximity" */
static void __exit ts_exit(void)
{
	int i ;
	for (i = 0; i < ARRAY_SIZE(supported_verdor); i++) {

		if(!strcmp(supported_verdor[i].vendor,g_chipid_name)) {

			supported_verdor[i].verdor_exit();
			break ;
		}
	}
	platform_driver_unregister(&ats_driver);
	ts_board_exit();
}

//late_initcall(ts_init);
//module_exit(ts_exit);
module_init(ts_init);
module_exit(ts_exit);

MODULE_AUTHOR("joseph.cai@spreadtrum.com");
MODULE_DESCRIPTION("Spreadtrum touchscreen core module");
MODULE_LICENSE("GPL");
