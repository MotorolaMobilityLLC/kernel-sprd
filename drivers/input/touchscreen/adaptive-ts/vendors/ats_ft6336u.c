#include <linux/delay.h>
#include "adaptive_ts.h"
#include <linux/module.h>
#include <linux/of.h>
#include <uapi/linux/input.h>
#include <linux/i2c.h>
#include "ats_core.h"
#define FT_MAX_POINTS 2
#define FT_POINT_LEN 6
#define FT_HEADER_LEN 3
#define FT_MAX_UPGRADE_RETRY 30
#define FT_FIRMWARE_PACKET_LENGTH 128
#define FT_ALL_DATA_LEN (FT_HEADER_LEN + FT_MAX_POINTS * FT_POINT_LEN)

#define REG_SCANNING_FREQ     0x88
#define REG_CHARGER_INDICATOR 0x8B
#define REG_CHIP_ID           0xA3
#define REG_POWER_MODE        0xA5
#define REG_FIRMWARE_VERSION  0xA6
#define REG_MODULE_ID         0xA8
#define REG_WORKING_STATE     0xFC
#define GESTURE_LEFT		0x20
#define GESTURE_RIGHT		0x21
#define GESTURE_UP		    0x22
#define GESTURE_DOWN		0x23
#define GESTURE_DOUBLECLICK	0x24
#define GESTURE_O		    0x30
#define GESTURE_W		    0x31
#define GESTURE_M		    0x32
#define GESTURE_E		    0x33
#define GESTURE_C		    0x34
#define GESTURE_S           0x46
#define GESTURE_V           0x54
#define GESTURE_Z           0x65
#define GESTURE_L           0x44
/* print focaltech raw data according to register map specification */
#if 0
#define FT_DEBUG_RAW_POINT
#endif

#define to_focaltech_controller(ptr) \
	container_of(ptr, struct focaltech_controller, controller)
extern struct i2c_client *g_client;
static const unsigned short focaltech_addrs[] = { 0x38 };
struct focaltech_controller {
	struct ts_controller controller;
	unsigned char a3;
	unsigned char a8;
	bool single_transfer_only;
};

static inline int focaltech_hid_to_i2c(void)
{
	unsigned char buf[3] = { 0xEB, 0xAA, 0x09 };
	ts_write(buf, 3);
	msleep(10);
	buf[0] = buf[1] = buf[2] = 0;
	ts_read(buf, 3);

	return buf[0] != 0xEB || buf[1] != 0xAA || buf[2] != 0x08;
}

/* firmware upgrade procedure */
static enum ts_result focaltech_upgrade_firmware(struct ts_controller *c,
	const unsigned char *data, size_t size, bool force)
{
	unsigned char fw_version_host = 0, fw_version = 0, crc = 0;
	unsigned char module_id_host = 0, module_id = 0;
	unsigned char buf[10] = { 0 };
	unsigned char fw_buf[FT_FIRMWARE_PACKET_LENGTH + 6] = { 0 };
	int ret = 0, i, j, packet_count, j_index, left;
	module_id_host = data[size - 1];
	ts_reg_read(REG_MODULE_ID, &module_id, 1);
	fw_version_host = data[size - 2];
	ts_reg_read(REG_FIRMWARE_VERSION, &fw_version, 1);

	/*
	 * 1. force upgrade
	 * 2. firmware is corrupted
	 * 3. host have a newer version of firmware
	 */
	if (!force) {
		if (module_id != 0 && module_id != module_id_host) {
			TS_ERR("module id(0x%02X) does not match 0x%02X!",
				module_id_host, module_id);
			return TSRESULT_INVALID_BINARY;
		}

		if ((fw_version != 0) && (fw_version != REG_FIRMWARE_VERSION)
			&& (fw_version >= fw_version_host)) {
			TS_WARN("our firmware(%d) is newer than host version(%d)"
				, fw_version, fw_version_host);
			return TSRESULT_OLDER_VERSION;
		}
	}

	TS_DBG("current version is %d, host version is %d",
		fw_version, fw_version_host);
	TS_DBG("current module is 0x%02X, host module is 0x%02X",
		module_id, module_id_host);

	/* check firmware size */
	if (size < 8 || size > 54 * 1024) {
		TS_ERR("invalid firmware size: %zu", size);
		return TSRESULT_INVALID_BINARY;
	}

	focaltech_hid_to_i2c();
	for (i = 0; i < FT_MAX_UPGRADE_RETRY; i++) {
		TS_DBG("Phase 1: reset chip");
		/* reset chip */
		buf[0] = 0xAA;
		ts_reg_write(REG_WORKING_STATE, buf, 1);
		msleep(2);
		buf[0] = 0x55;
		ts_reg_write(REG_WORKING_STATE, buf, 1);
		msleep(200);

		/* enter upgrade mode */
		TS_DBG("Phase 2: enter mode");
		if (focaltech_hid_to_i2c()) {
			TS_DBG("hid_to_i2c fail, try again");
			continue;
		}
		msleep(10);

		buf[0] = 0x55;
		buf[1] = 0xAA;
		if (ts_write(buf, 2) < 0) {
			TS_DBG("write 0x55, 0xAA fail, try again");
			continue;
		}

		/* check READ-ID */
		TS_DBG("Phase 3: check ID");
		msleep(1);
		buf[0] = 0x90;
		buf[1] = buf[2] = buf[3] = 0;
		ts_write(buf, 4);
		buf[0] = buf[1] = 0;
		ts_read(buf, 2);
		if (buf[0] == 0x54 && buf[1] == 0x2C)
			break;

		TS_DBG("read id fail: 0x%02X, 0x%02X", buf[0], buf[1]);
	}

	if (i >= FT_MAX_UPGRADE_RETRY)
		return TSRESULT_OTHER_UPGRADE_ERROR;

	/* erase app data and panel parameter area */
	TS_DBG("Phase 4: erase data");
	buf[0] = 0x61;
	ts_write(buf, 1);
	msleep(1350);

	for (i = 0; i < FT_MAX_UPGRADE_RETRY / 2; i++) {
		buf[0] = 0x6A;
		ts_write(buf, 1);
		buf[0] = buf[1] = 0;
		ts_read(buf, 2);
		if (buf[0] == 0xF0 && buf[1] == 0xAA)
			break;

		msleep(50);
	}

	buf[0] = 0xB0;
	buf[1] = (size >> 16) & 0xFF;
	buf[2] = (size >> 8) & 0xFF;
	buf[3] = size & 0xFF;
	ts_write(buf, 4);

	/* write firmware into flash */
	TS_DBG("Phase 5: flash firmware");
	packet_count = size / FT_FIRMWARE_PACKET_LENGTH;
	fw_buf[0] = 0xBF;
	fw_buf[1] = 0;

	for (j = 0; j < packet_count; j++) {
		j_index = j * FT_FIRMWARE_PACKET_LENGTH;
		fw_buf[2] = (j_index >> 8) & 0xFF;
		fw_buf[3] = j_index & 0xFF;
		fw_buf[4] = (FT_FIRMWARE_PACKET_LENGTH >> 8) & 0xFF;
		fw_buf[5] = FT_FIRMWARE_PACKET_LENGTH & 0xFF;

		for (i = 0; i < FT_FIRMWARE_PACKET_LENGTH; i++) {
			fw_buf[6 + i] = data[j_index + i];
			crc ^= fw_buf[6 + i];
		}
		ts_write(fw_buf, FT_FIRMWARE_PACKET_LENGTH + 6);

		for (i = 0; i < FT_MAX_UPGRADE_RETRY; i++) {
			buf[0] = 0x6A;
			ts_write(buf, 1);
			buf[0] = buf[1] = 0;
			ts_read(buf, 2);

			if (((buf[0] << 8) | buf[1]) == (j + 0x1000)) {
				TS_DBG("   flash %d bytes of packet %d",
					FT_FIRMWARE_PACKET_LENGTH, j + 1);
				break;
			}
			msleep(1);
		}
	}

	left = size % FT_FIRMWARE_PACKET_LENGTH;
	if (left) {
		j_index = packet_count * FT_FIRMWARE_PACKET_LENGTH;
		fw_buf[2] = (j_index >> 8) & 0xFF;
		fw_buf[3] = j_index & 0xFF;
		fw_buf[4] = (left >> 8) & 0xFF;
		fw_buf[5] = left & 0xFF;

		for (i = 0; i < left; i++) {
			fw_buf[6 + i] = data[j_index + i];
			crc ^= fw_buf[6 + i];
		}
		ts_write(fw_buf, left + 6);

		for (i = 0; i < FT_MAX_UPGRADE_RETRY; i++) {
			buf[0] = 0x6A;
			ts_write(buf, 1);
			buf[0] = buf[1] = 0;
			ts_read(buf, 2);

			if (((buf[0] << 8) | buf[1]) == (j + 0x1000)) {
				TS_DBG("   flash last %d bytes of packet %d", left, j + 1);
				break;
			}
			msleep(1);
		}
	}
	msleep(50);

	/* read checksum */
	TS_DBG("Phase 6: validate checksum");
	buf[0] = 0x64;
	ts_write(buf, 1);
	msleep(300);
	buf[0] = 0x65;
	buf[1] = buf[2] = buf[3] = 0;
	buf[4] = (size >> 8) & 0xFF;
	buf[5] = size & 0xFF;
	ts_write(buf, 6);
	msleep(size / 256);

	for (i = 0; i < 100; i++) {
		buf[0] = 0x6A;
		ts_write(buf, 1);
		buf[0] = buf[1] = 0;
		ts_read(buf, 2);

		if (buf[0] == 0xF0 && buf[1] == 0x55)
			break;

		msleep(1);
	}
	buf[0] = 0x66;
	ts_write(buf, 1);
	buf[0] = 0;
	ts_read(buf, 1);
	if (buf[0] != crc) {
		TS_DBG("checksum error: should be 0x%02X, but we get 0x%02X",
			crc, buf[0]);
		return -EIO;
	}

	/* reset */
	TS_DBG("Phase 7: reset new firmware");
	buf[0] = 0x07;
	ts_write(buf, 1);
	msleep(130);

	msleep(300);

	fw_version = 0;
	ts_reg_read(REG_FIRMWARE_VERSION, &fw_version, 1);
	if (!ret)
		TS_DBG("upgrade finished, new version is %d", fw_version);

	return TSRESULT_UPGRADE_FINISHED;
}


int  focaltech_upgrade_status(struct ts_controller *c)
{
  return 1;
}
/* read chip id and module id to match controller */
static enum ts_result focaltech_match(struct ts_controller *c)
{
	unsigned char chip_id = 0, module_id = 0;
	bool err = false;
	//struct focaltech_controller *ftc = to_focaltech_controller(c);
//	 g_client->addr=ftc->controller.addrs[0];
         g_client->addr=c->addrs[0];
	printk("focaltech_match:0x%04x\n",g_client->addr);
	err |= (1 != ts_reg_read(REG_CHIP_ID, &chip_id, 1));
	err |= (1 != ts_reg_read(REG_MODULE_ID, &module_id, 1));
	printk("focaltech_match:a3:a8:0x%02x,0x%04x",chip_id,module_id);
//	return (err )//|| (chip_id != ftc->a3) || (module_id != ftc->a8))
//		? TSRESULT_NOT_MATCHED : TSRESULT_FULLY_MATCHED;
	return (err) ? TSRESULT_NOT_MATCHED : TSRESULT_FULLY_MATCHED;
}

static int focaltech_fetch(struct ts_controller *c, struct ts_point *points)
{
	int i, j = 0;	
	unsigned char buf[FT_ALL_DATA_LEN] = { 0 };
	int p_num = 0;
	struct focaltech_controller *ftc = to_focaltech_controller(c);
	if (!ftc->single_transfer_only) {
		if (ts_reg_read(0, buf, FT_HEADER_LEN) != FT_HEADER_LEN) {
			TS_ERR("failed to read head data");
			return -1;
		}

	} else {
		/* read all bytes once! */
		if (ts_reg_read(0, buf, FT_ALL_DATA_LEN) != FT_ALL_DATA_LEN) {
			TS_ERR("failed to read data");
			return -1;
		}
	}
	c->pdata->ps_buf=buf[1];
	p_num = buf[FT_HEADER_LEN - 1]&0x07;
	c->pdata->touch_point=p_num;
	printk("p_num=%d\n", p_num);
#ifdef FT_DEBUG_RAW_POINT
	TS_DBG("p_num=%d", p_num);
#endif
	/* check point number */
	if (p_num > FT_MAX_POINTS) {
		TS_WARN("invalid point_num: %d, ignore this packet",
			buf[FT_HEADER_LEN - 1]);
		return -1;
	} else if (p_num == 0) {
		/* here we report UP event for all points for convenience */
		/* TODO: change to last point number */
		for (i = 0; i < FT_MAX_POINTS; i++) {
			points[i].pressed = 0;
			points[i].slot = i;
		}
		return FT_MAX_POINTS;
	}

	/* read one more point to ensure getting data for all points */
	p_num++;
	if (p_num > FT_MAX_POINTS)
		p_num = FT_MAX_POINTS;

	if (!ftc->single_transfer_only) {
		if (FT_POINT_LEN * p_num != ts_reg_read(FT_HEADER_LEN,
			buf + FT_HEADER_LEN, FT_POINT_LEN * p_num)) {
			TS_ERR("failed to read point data");
			return -1;
		}
	}

	/* calculate points */
	for (i = 0; i < p_num; i++) {
#ifdef FT_DEBUG_RAW_POINT
		TS_DBG("i=%d, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X",
			i, buf[FT_HEADER_LEN+FT_POINT_LEN*i],
			buf[FT_HEADER_LEN+FT_POINT_LEN*i+1],
			buf[FT_HEADER_LEN+FT_POINT_LEN*i+2],
			buf[FT_HEADER_LEN+FT_POINT_LEN*i+3],
			buf[FT_HEADER_LEN+FT_POINT_LEN*i+4],
			buf[FT_HEADER_LEN+FT_POINT_LEN*i+5]);
#endif
		/* filter out invalid points */
		if ((buf[FT_HEADER_LEN+FT_POINT_LEN*i] & 0xc0) == 0xc0)
			continue;
		points[j].x = ((buf[FT_HEADER_LEN+FT_POINT_LEN*i] & 0x0f) << 8)
			| buf[FT_HEADER_LEN+FT_POINT_LEN*i+1];
		points[j].y = ((buf[FT_HEADER_LEN+FT_POINT_LEN*i+2] & 0x0f) << 8)
			| buf[FT_HEADER_LEN+FT_POINT_LEN*i+3];
		points[j].pressure = buf[FT_HEADER_LEN+FT_POINT_LEN*i+4];
		points[j].pressed = !(buf[FT_HEADER_LEN+FT_POINT_LEN*i] & 0x40);
		points[j].slot = buf[FT_HEADER_LEN+FT_POINT_LEN*i+2] >> 4;
		j++;
		printk("points[j].x:points[j].y:%d,%d\n",points[j].x,points[j].y);
	}

	return j;
}

static  int focaltech_check_gesture(struct ts_controller *c, int gesture_id)
{	
    int keycode = 0;
 
    printk("kaka gesture_id==0x%x\n ",gesture_id);
    switch(gesture_id){
        case GESTURE_LEFT:
            keycode = KEY_LEFT;
            break;
        case GESTURE_RIGHT:
            keycode = KEY_RIGHT;
            break;
        case GESTURE_UP:
            keycode = KEY_UP;
            break;
        case GESTURE_DOWN:
            keycode = KEY_DOWN;
            break;
        case GESTURE_DOUBLECLICK:
            keycode = KEY_POWER;    //KEY_POWER;//
            break;
        case GESTURE_O:
            keycode = KEY_O;
            break;
        case GESTURE_W:
            keycode = KEY_W;
            break;
        case GESTURE_M:
            keycode = KEY_M;
            break;
        case GESTURE_E:
            keycode = KEY_E;
            break;
        case GESTURE_C:
            keycode = KEY_C;
            break;
        case GESTURE_S:
            keycode = KEY_S;
            break;
         case GESTURE_V:
            keycode = KEY_V;
            break;
        case GESTURE_Z:
            keycode = KEY_UP;
            break;
        case GESTURE_L:
            keycode = KEY_L;
            break;
        default:
            break;
    }
    if(keycode){
	printk("keycode:%d\n",keycode);
        input_report_key(c->pdata->input, keycode, 1);
        input_sync(c->pdata->input);
        input_report_key(c->pdata->input, keycode, 0);
        input_sync(c->pdata->input);
    }
    return keycode;
}

static  int focaltech_gesture_readdata(struct ts_controller *c)
{
    int ret = -1;
    int gestrue_id = 0;
    u8 buf[4] = {0xd3, 0xd3};
    
    ret = ts_reg_read(0xd3, buf, 2);
    if(ret < 0){
        pr_err("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
        return ret;
    }
    if(buf[1] != 0){
        gestrue_id = 0x24;
    }else{
        gestrue_id = buf[0];
    }
	printk("gestrue_id:%d\n",gestrue_id);
    focaltech_check_gesture(c, gestrue_id);
    return 0;
}

static  void focaltech_gesture_init(struct ts_controller *c)
{

    input_set_capability(c->pdata->input, EV_KEY, KEY_POWER);
    input_set_capability(c->pdata->input, EV_KEY, KEY_U);
    input_set_capability(c->pdata->input, EV_KEY, KEY_UP);
    input_set_capability(c->pdata->input, EV_KEY, KEY_DOWN);
    input_set_capability(c->pdata->input, EV_KEY, KEY_LEFT);
    input_set_capability(c->pdata->input, EV_KEY, KEY_RIGHT);
    input_set_capability(c->pdata->input, EV_KEY, KEY_O);
    input_set_capability(c->pdata->input, EV_KEY, KEY_E);
    input_set_capability(c->pdata->input, EV_KEY, KEY_M);
    input_set_capability(c->pdata->input, EV_KEY, KEY_L);
    input_set_capability(c->pdata->input, EV_KEY, KEY_W);
    input_set_capability(c->pdata->input, EV_KEY, KEY_S);
    input_set_capability(c->pdata->input, EV_KEY, KEY_V);
    input_set_capability(c->pdata->input, EV_KEY, KEY_Z);
    input_set_capability(c->pdata->input, EV_KEY, KEY_C);
	
    __set_bit(KEY_LEFT,  c->pdata->input->keybit);
    __set_bit(KEY_RIGHT,  c->pdata->input->keybit);
    __set_bit(KEY_UP,  c->pdata->input->keybit);
    __set_bit(KEY_DOWN,  c->pdata->input->keybit);
    __set_bit(KEY_D,  c->pdata->input->keybit);
    __set_bit(KEY_O,  c->pdata->input->keybit);
    __set_bit(KEY_W,  c->pdata->input->keybit);
    __set_bit(KEY_M,  c->pdata->input->keybit);
    __set_bit(KEY_E,  c->pdata->input->keybit);
    __set_bit(KEY_C,  c->pdata->input->keybit);
    __set_bit(KEY_S,  c->pdata->input->keybit);
    __set_bit(KEY_V,  c->pdata->input->keybit);
    __set_bit(KEY_Z,  c->pdata->input->keybit);
}

static  int focaltech_gesture_exit(struct ts_controller *c)
{
   unsigned char temp = 0; 
   temp = 0x00;
   ts_reg_write(0xd0, &temp, 1);
   return 0;
}

static  int focaltech_gesture_suspend(struct ts_controller *c)
{

   unsigned char temp = 0;
    c->pdata->gesture_enable=1;	
    if(c->pdata->gesture_enable == 1){
          c->pdata->gesture_state= 0x01;
	  temp = 0x01;
    	  ts_reg_write(0xd0, &temp, 1);
          return 0;
   }
    return 0;
 }


static  int focaltech_gesture_resume(struct ts_controller *c)
{

   unsigned char temp = 0;
    if(c->pdata->gesture_enable== 1){
       temp = 0x00;
       c->pdata->gesture_state =0;
       ts_reg_write(0xd0, &temp, 1);
    }
  return 0;

}
static enum ts_result focaltech_handle_event(
	struct ts_controller *c, enum ts_event event, void *data)
{
	struct focaltech_controller *ftc = to_focaltech_controller(c);
	unsigned char val = 0, a3 = 0, a8 = 0;
	struct device_node *pn = NULL;
	enum ts_event ret = TSRESULT_EVENT_HANDLED;
	printk("focaltech_handle_event:%d\n",event);
	switch (event) {
	case TSEVENT_POWER_ON:
		if (data) {
			pn = (struct device_node *)data;
			if (!of_property_read_u8(pn, "a8", &ftc->a8))
				TS_DBG("parse a8 value: 0x%02X", ftc->a8);
			ftc->single_transfer_only = !!of_get_property(pn,
				"single-transfer-only", NULL);
			if (ftc->single_transfer_only)
				TS_DBG("single transfer only");
		}
		if ((ts_reg_read(REG_CHIP_ID, &a3, 1) != 1)
			|| (ts_reg_read(REG_MODULE_ID, &a8, 1) != 1)
			|| (a3 != ftc->a3))
			ret = TSRESULT_EVENT_NOT_HANDLED;
		TS_DBG("read a8 value from chip: 0x%02X", a8);
		break;
	case TSEVENT_SUSPEND:
		val = 0x03;
		ts_reg_write(REG_POWER_MODE, &val, 1);
		break;
	case TSEVENT_RESUME:
		ts_gpio_set(TSGPIO_RST, 0);
		msleep(10);
		ts_gpio_set(TSGPIO_RST, 1);
		msleep(200);
		break;
	case TSEVENT_NOISE_HIGH:
		val = 0x01;
		ts_reg_write(REG_CHARGER_INDICATOR, &val, 1);
		break;
	case TSEVENT_NOISE_NORMAL:
		val = 0x00;
		ts_reg_write(REG_CHARGER_INDICATOR, &val, 1);
		break;
	default:
		break;
	}

	return ret;
}


static const struct ts_virtualkey_info focaltech_virtualkeys[] = {
	DECLARE_VIRTUALKEY(600, 1350, 60, 45, KEY_BACK),
	DECLARE_VIRTUALKEY(360, 1350, 60, 45, KEY_HOMEPAGE),
	DECLARE_VIRTUALKEY(120, 1350, 60, 45, KEY_APPSELECT),
};

static const struct ts_register_info focaltech_registers[] = {
	DECLARE_REGISTER(TSREG_CHIP_ID, REG_CHIP_ID),
	DECLARE_REGISTER(TSREG_MOD_ID, REG_MODULE_ID),
	DECLARE_REGISTER(TSREG_FW_VER, REG_FIRMWARE_VERSION),
	DECLARE_REGISTER("frequency", REG_SCANNING_FREQ),
	DECLARE_REGISTER("charger_indicator", REG_CHARGER_INDICATOR),
};

static struct focaltech_controller ft5x46 = {
	.controller = {
		.name = "FT5x46",
		.vendor = "focaltech",
		.config = TSCONF_ADDR_WIDTH_BYTE
			| TSCONF_POWER_ON_RESET
			| TSCONF_RESET_LEVEL_LOW
			| TSCONF_REPORT_MODE_IRQ
			| TSCONF_IRQ_TRIG_EDGE_FALLING
			| TSCONF_REPORT_TYPE_3,
		.addr_count = ARRAY_SIZE(focaltech_addrs),
		.addrs = focaltech_addrs,
		.virtualkey_count = ARRAY_SIZE(focaltech_virtualkeys),
		.virtualkeys = focaltech_virtualkeys,
		.register_count = ARRAY_SIZE(focaltech_registers),
		.registers = focaltech_registers,
		.panel_width = 480,
		.panel_height = 854,
		.reset_keep_ms = 10,
		.reset_delay_ms = 200,
		.parser = {
		},
		.match = focaltech_match,
		.fetch_points = focaltech_fetch,
		.handle_event = focaltech_handle_event,
		.upgrade_firmware = focaltech_upgrade_firmware,
		.upgrade_status = focaltech_upgrade_status,
		.gesture_readdata = focaltech_gesture_readdata,
		.gesture_init = focaltech_gesture_init,
		.gesture_exit = focaltech_gesture_exit,
		.gesture_suspend = focaltech_gesture_suspend,
		.gesture_resume = focaltech_gesture_resume,
	},
	.a3 = 0x54,
	.a8 = 0x87,
	.single_transfer_only = false,
};

int focaltech_init(void)
{

	ts_register_controller(&ft5x46.controller);
	return 0;
}

void focaltech_exit(void)
{
	ts_unregister_controller(&ft5x46.controller);
}

//module_init(focaltech_init);
//module_exit(focaltech_exit);

//MODULE_AUTHOR("joseph.cai@spreadtrum.com");
//MODULE_DESCRIPTION("Spreadtrum touchscreen driver for Focaltech");
//MODULE_LICENSE("GPL");
