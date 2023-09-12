#include <linux/delay.h>
#include "adaptive_ts.h"
#include <linux/module.h>
#include <uapi/linux/input.h>
//#include <transsion/gesture.h>

#include "focaltech_core.h"

static const unsigned short fts_addrs[] = {0x38};
#define FTS_I2C_Address 0x38

struct msg_gesture_map  fts_gestures_maps[] = {
	{0,	GESTURE_LEFT,   GESTURE_LF, 	0xD1,	0},
	{0,	GESTURE_UP,     GESTURE_up,		0xD1,	2},
	{0,	GESTURE_RIGHT,	GESTURE_RT,	    0xD1,	1},
	{0,	GESTURE_DOWN,	GESTURE_down,	0xD1,	3},
	{0,	GESTURE_DOUBLECLICK,    GESTURE_DC,	    0xD1,	4},
	{0,	GESTURE_W,	GESTURE_w,		0xD2,	1},
	{0,	GESTURE_O,	GESTURE_o,		0xD2,	0},
	{0,	GESTURE_M,	GESTURE_m,		0xD2,	2},
	{0,	GESTURE_E,	GESTURE_e,		0xD2,	3},
	{0,	GESTURE_C,	GESTURE_c,		0xD2,	4},
	{0,	GESTURE_Z,	GESTURE_z,		0xD5,	1},
	{0,	GESTURE_S,	GESTURE_s,		0xD5,	6},
	{0,	GESTURE_V,	GESTURE_v,		0xD6,	4},
};

static const struct ts_virtualkey_info fts_virtualkeys[] = {
    DECLARE_VIRTUALKEY(120, 1500, 60, 45, KEY_BACK),
    DECLARE_VIRTUALKEY(360, 1500, 60, 45, KEY_HOMEPAGE),
    DECLARE_VIRTUALKEY(600, 1500, 60, 45, KEY_APPSELECT),
};

static const struct ts_register_info fts_registers[] = {
	DECLARE_REGISTER(TSREG_CHIP_ID, 0xA3),
/*  DECLARE_REGISTER(TSREG_MOD_ID, REG_MODULE_ID),
    DECLARE_REGISTER(TSREG_FW_VER, REG_FIRMWARE_VERSION),
    DECLARE_REGISTER("frequency", REG_SCANNING_FREQ),
    DECLARE_REGISTER("charger_indicator", REG_CHARGER_INDICATOR),
*/
};

int ft6436u_read_id(void)
{
	int ret = 1;
	int pdata_size = sizeof(struct fts_ts_platform_data);

    /* malloc memory for global struct variable */
    fts_data = (struct fts_ts_data *)kzalloc(sizeof(*fts_data), GFP_KERNEL);
    if (!fts_data) {
        FTS_ERROR("allocate memory for fts_data fail");
        return -ENOMEM;
    }

    fts_data->client = g_client;
    fts_data->dev = &g_client->dev;
	g_client->addr = FTS_I2C_Address;
    fts_data->log_level = 1;
    fts_data->fw_is_running = 0;
    fts_data->bus_type = BUS_TYPE_I2C;

	fts_data->pdata = kzalloc(pdata_size, GFP_KERNEL);
    if (!fts_data->pdata) {
        FTS_ERROR("allocate memory for platform_data fail");
        return -ENOMEM;
    }
	fts_data->pdata->reset_gpio = g_board_b->rst_gpio;
	fts_data->pdata->irq_gpio = g_board_b->int_gpio;
	fts_data->pdata->max_touch_number = 5 ;
	fts_data->pdata->have_key = 0 ;

    fts_data->ts_workqueue = create_singlethread_workqueue("fts_wq");
    if (!fts_data->ts_workqueue) {
        FTS_ERROR("create fts workqueue fail");
    }

    spin_lock_init(&fts_data->irq_lock);
    mutex_init(&fts_data->report_mutex);
    mutex_init(&fts_data->bus_lock);

    /* Init communication interface */
    ret = fts_bus_init(fts_data);
    if (ret) {
        FTS_ERROR("bus initialize fail");
    }

	ret = fts_report_buffer_init(fts_data);
    if (ret) {
        FTS_ERROR("report buffer init fail");
    }

	ret = fts_get_ic_information(fts_data);
	if (ret) {
		FTS_ERROR("not focal IC, unregister driver");
		ret = -1;
	}
	else
		ret = 1;

    return ret;

}

static enum ts_result fts_handle_event(
	struct ts_controller *controller, enum ts_event event, void *data)
{
	int ret ;
	switch (event) {
	case TSEVENT_POWER_ON:
		FTS_INFO("TSEVENT_POWER_ON\n");
		break;
	case TSEVENT_SUSPEND:
		FTS_INFO("TSEVENT_SUSPEND\n");
#if FTS_ESDCHECK_EN
        fts_esdcheck_suspend();
#endif
        ret = fts_write_reg(FTS_REG_POWER_MODE, FTS_REG_POWER_MODE_SLEEP);
        if (ret < 0)
            FTS_ERROR("set TP to sleep mode fail, ret=%d", ret);
		if(!g_pdata->gesture_state)
			ts_reset_controller_ex(g_pdata, false);
		break;
	case TSEVENT_RESUME:
		FTS_INFO("TSEVENT_RESUME\n");
#if FTS_ESDCHECK_EN
        fts_esdcheck_resume();
#endif
		break;
	case TSEVENT_NOISE_HIGH:
		FTS_INFO("TSEVENT_NOISE_HIGH\n");
        if (!fts_data->charger_mode) {
            FTS_DEBUG("enter charger mode");
            ret = fts_ex_mode_switch(MODE_CHARGER, ENABLE);
            if (ret >= 0) {
                fts_data->charger_mode = ENABLE;
            }
        }
		break;
	case TSEVENT_NOISE_NORMAL:
		FTS_INFO("TSEVENT_NOISE_NORMAL\n");
        if (fts_data->charger_mode) {
            FTS_DEBUG("exit charger mode");
            ret = fts_ex_mode_switch(MODE_CHARGER, DISABLE);
            if (ret >= 0) {
                fts_data->charger_mode = DISABLE;
            }
        }
		break;
	default:
		break;
	}

	return TSRESULT_EVENT_HANDLED;
}


static enum ts_result fts_match(struct ts_controller *c){

	TS_INFO("fts match control");

	return TSRESULT_FULLY_MATCHED;
}

int  fts_upgrade_init(void){

	int ret=0, i=0;
	uint8_t *tpd_vendor_id={0};
	uint8_t vendor_name[16]={0};
	u32 *tpd_firmware_update={0};
	struct device_node	*p_node;

	p_node = fts_data->client->dev.of_node;

	fts_data->update_node = of_get_child_by_name(p_node, "ats_ft6436u");
	of_property_read_u32(fts_data->update_node, "", &fts_data->vendor_nums);

	tpd_vendor_id = kmalloc(fts_data->vendor_nums, GFP_KERNEL);
	if(tpd_vendor_id == NULL){
		FTS_ERROR("tpd_vendor_id kmalloc is not found\n");
		return -1;
	}

	tpd_firmware_update = kmalloc(fts_data->vendor_nums*sizeof(u32), GFP_KERNEL);
	if(tpd_firmware_update == NULL){
		FTS_ERROR("tpd_firmware kmalloc is not found\n");
		return -1;
	}

	of_property_read_u32_array(fts_data->update_node, "tp_upgrade_fw", tpd_firmware_update, fts_data->vendor_nums);
	of_property_read_u8_array(fts_data->update_node, "tp_vendor_id", tpd_vendor_id, fts_data->vendor_nums);

	for(i=0; i <  fts_data->vendor_nums; i++ ){
		if (tpd_vendor_id[i] ==fts_data->ic_info.vendor_id){

			fts_data->vendor_num = i;
			g_pdata->firmware_update_switch = tpd_firmware_update[i];

			sprintf(vendor_name, "tp_vendor_name%d", i);
			of_property_read_string(fts_data->update_node, vendor_name, (char const **)&g_pdata->vendor_string);

			break;
		}
	}

	memset(g_pdata->chip_name, 0x00, sizeof(g_pdata->chip_name));
	sprintf(g_pdata->chip_name, "ft%02x%02x", fts_data->ic_info.ids.chip_idh, fts_data->ic_info.ids.chip_idl);

	return ret;
}

static void fts_custom_initialization(void)
{
	int ret = 0;
	ret = fts_create_apk_debug_channel(fts_data);
	if (ret) {
		FTS_ERROR("create apk debug node fail");
	}

	ret = fts_create_sysfs(fts_data);
	if (ret) {
		FTS_ERROR("create sysfs node fail");
	}

#if FTS_POINT_REPORT_CHECK_EN
	ret = fts_point_report_check_init(fts_data);
	if (ret) {
		FTS_ERROR("init point report check fail");
	}
#endif

	ret = fts_ex_mode_init(fts_data);
	if (ret) {
		FTS_ERROR("init glove/cover/charger fail");
	}
	ret = fts_fwupg_init(fts_data);
    if (ret) {
        FTS_ERROR("init fw upgrade fail");
 	}
    #if FTS_ESDCHECK_EN
    ret = fts_esdcheck_init(fts_data);
    if (ret) {
        FTS_ERROR("init esd check fail");
    }
    #endif
}

int fts_upgrade_status(struct ts_controller *c)
{
	return 0;
	#if 0
	int ret=-1;
	static int is_init=1;

	if(is_init){
		is_init=0;
		ret = fts_upgrade_init();
	}
	else{
		if(!fts_fw_upgrade())
			ret =1;
	}
	return ret;
	#endif
}


static int fts_fetch(struct ts_controller *c, struct ts_point *points)
{
	int ret = 0;
	int i = 0;
	u8 pointid = 0;
	int base = 0;
	int max_touch_num = 5;
	u8 *buf = fts_data->point_buf;
#if FTS_ESDCHECK_EN
    fts_esdcheck_set_intr(1);
#endif
	ret = fts_read_touchdata(fts_data);
	if (ret) {
		return ret;
	}
#if FTS_ESDCHECK_EN
    fts_esdcheck_set_intr(0);
#endif

	fts_data->point_num = buf[FTS_TOUCH_POINT_NUM] & 0x0F;
	fts_data->touch_point = 0;

	c->pdata->ps_buf=buf[1];
    c->pdata->touch_point=buf[FTS_TOUCH_POINT_NUM] & 0x0F;

    if ((fts_data->point_num == 0x0F) && (buf[2] == 0xFF) && (buf[3] == 0xFF)
        && (buf[4] == 0xFF) && (buf[5] == 0xFF) && (buf[6] == 0xFF)) {
        FTS_DEBUG("touch buff is 0xff, need recovery state");
        //fts_release_all_finger();
        //fts_tp_state_recovery(fts_data);
        fts_data->point_num = 0;
        return -EIO;
    }


	for (i = 0; i < max_touch_num; i++) {
		base = FTS_ONE_TCH_LEN * i;
		pointid = (buf[FTS_TOUCH_ID_POS + base]) >> 4;
		if (pointid >= FTS_MAX_ID)
			break;
		else if (pointid >= max_touch_num) {
			FTS_ERROR("ID(%d) beyond max_touch_number", pointid);
			return -EINVAL;
		}

		fts_data->touch_point++;
		points[i].x = ((buf[FTS_TOUCH_X_H_POS + base] & 0x0F) << 8) +
					  (buf[FTS_TOUCH_X_L_POS + base] & 0xFF);
		points[i].y = ((buf[FTS_TOUCH_Y_H_POS + base] & 0x0F) << 8) +
					  (buf[FTS_TOUCH_Y_L_POS + base] & 0xFF);
		points[i].pressed = ~(buf[FTS_TOUCH_EVENT_POS + base] >> 6);
		points[i].slot = buf[FTS_TOUCH_ID_POS + base] >> 4;
		points[i].touch_major = buf[FTS_TOUCH_AREA_POS + base] >> 4;
		points[i].pressure =  buf[FTS_TOUCH_PRE_POS + base];

		if (points[i].pressed && (fts_data->point_num == 0)) {
			FTS_INFO("abnormal touch data from fw");
			return -EIO;
		}
	}
	c->pdata->touch_point = (unsigned short)i;
	if (fts_data->touch_point == 0) {
		FTS_INFO("no touch point information(%02x)", buf[2]);
		return 0;
	}

	return fts_data->touch_point;
}

static  unsigned char fts_gesture_readdata_c(struct ts_controller *c)
{
    unsigned char value = fts_gesture_readdata(fts_data, NULL);
	if ( -1 != value) {
        //c->pdata->gesture_data = value ;
		FTS_INFO("succuss to get gesture data in irq handler");
		//return 1;
	}
	return value;
}

#define FT_GEST_ID  0xD1
int fts_gesture_config(struct ts_controller * c , int onoff, struct msg_gesture_map *map)
{
	unsigned char value;

	if (map->enabled == onoff)
		return 0;
	value = c->gesture_reg[map->reg - FT_GEST_ID];
	if (onoff){
		value |= (1 << map->bit);
	}else{
		value &= ~(1 << map->bit);
	}
	c->gesture_reg[map->reg - FT_GEST_ID] = value;
	//FTS_INFO("set ftx_gestrue_id_0 0x%X ftx_gestrue_id_1 0x%X ftx_gestrue_id_4 0x%X ftx_gestrue_id_5 0x%X\n", ftx_gestrue_id[0], ftx_gestrue_id[1],ftx_gestrue_id[4],  ftx_gestrue_id[5]);
	//fts_write_reg(map->reg, value);
	FTS_INFO("set ftx_reg 0x%X to 0x%X\n", map->reg, value);
	map->enabled = onoff;
	return 0;
}

static void fts_gesture_init_c(struct ts_controller *c)
{
	struct ts_data *pdata = c->pdata ;
	int ret;
	fts_data->irq = pdata->irq ;
    fts_data->controller = c;
	FTS_INFO("irq[%d]!!\n" , fts_data->irq);
	fts_data->input_dev = pdata->input ;
	ret = fts_gesture_init(fts_data);
    if (ret) {
        FTS_ERROR("init gesture fail");
    }

}

static int fts_gesture_exit_c(struct ts_controller *c)
{
	fts_gesture_exit(fts_data);
	return 0;
}

static int fts_gesture_suspend_c(struct ts_controller *c)
{
    struct ts_data *pdata = c->pdata ;
    fts_data->gesture_mode = pdata->gesture_state;
	fts_gesture_suspend(fts_data);
    #if FTS_ESDCHECK_EN
        fts_esdcheck_suspend();
    #endif
	return 0;
 }

static int fts_gesture_resume_c(struct ts_controller *c)
{
    struct ts_data *pdata = c->pdata ;
    fts_data->gesture_mode = pdata->gesture_state;
    fts_gesture_resume(fts_data);
    #if FTS_ESDCHECK_EN
        fts_esdcheck_resume();
    #endif
	return 0;
}


static int fts_ps_resume(struct ts_data *pdata) {
	if (ts_get_mode_ext(pdata, TSMODE_PS_STATUS)||ts_get_mode_ext(pdata, TSMODE_SENSORHUB_STATUS)){
		if (pdata->tpd_prox_active && (pdata->tpm_status == TPM_DESUSPEND)) {

			u8 temp = 0xa0;

			fts_write_reg(0xa0, temp);

			pdata->tpd_prox_old_state = 0x0f;

			printk("[fts] ps_resume ps is on, so return !!!\n");
			return 0;
		}
	}

	ts_reset_controller_ex(pdata, true);

	if (ts_get_mode_ext(pdata, TSMODE_PS_STATUS)||ts_get_mode_ext(pdata, TSMODE_SENSORHUB_STATUS)){
		if(pdata->tps_status == TPS_DEON && pdata->tpd_prox_active){
			u8 temp = 1;
			fts_write_reg(0xb0, temp);

			pdata->tps_status = TPS_ON ;
		}
	}

	return 1;
}

static int fts_ps_suspend(struct ts_data *pdata) {
	if (ts_get_mode_ext(pdata, TSMODE_PS_STATUS)||ts_get_mode_ext(pdata, TSMODE_SENSORHUB_STATUS)){
		if (pdata->tpd_prox_active) {

			printk("[TS] ps_suspend:ps is on, so return!!!\n");
			return 0;
		}
	}
	return 1;
}

static void fts_proximity_switch(bool onoff) {
    u8 state;
    int ret = -1;
    fts_read_reg(FTS_REG_PSENSOR_ENABLE, &state);
    if (onoff)
        state |= FTS_PSENSOR_ENABLE_MASK;
    else
        state &= ~FTS_PSENSOR_ENABLE_MASK;

    ret = fts_write_reg(FTS_REG_PSENSOR_ENABLE, state);
    if (ret < 0)
        FTS_ERROR("write psensor switch command failed");
    return;
}

static void fts_ps_irq_handler(struct ts_data *pdata) {
//		if((fts_data->psensor_pdata->tp_psensor_data== 0xc0) || (fts_data->psensor_pdata->tp_psensor_data== 0xe0)){
//			pdata->tpd_prox_old_state = fts_data->psensor_pdata->tp_psensor_data;;
//			}

		pdata->tpd_prox_old_state = pdata->ps_buf;

}
static void fts_ps_reset(void){
//	u8 temp = 0xa0;
	fts_write_reg(0xb0, 0x01);
//	fts_read_reg(0x01, &temp);
//	printk("[TS] fts reset ps mode...,temp\n");
}

static struct ts_controller FtsController = {
	.name = "FT6436U",
	.vendor = "focaltech",
	.incell = 1,
	.config = TSCONF_ADDR_WIDTH_BYTE
		| TSCONF_POWER_ON_RESET
		| TSCONF_RESET_LEVEL_LOW
		| TSCONF_REPORT_MODE_IRQ
		| TSCONF_IRQ_TRIG_EDGE_FALLING
		| TSCONF_REPORT_TYPE_3,
	.addr_count = ARRAY_SIZE(fts_addrs),
	.addrs = fts_addrs,
	.virtualkey_count = ARRAY_SIZE(fts_virtualkeys),
	.virtualkeys = fts_virtualkeys,
	.register_count = ARRAY_SIZE(fts_registers),
	.registers = fts_registers,
	.panel_width = 480,
	.panel_height = 1014,
	.reset_keep_ms = 20,
	.reset_delay_ms = 30,
	.parser = {
	},
    .ps_reset = fts_ps_reset,
    .gesture_reg = {0},
    .msg_gestures_maps = fts_gestures_maps,
    .msg_gestures_count = ARRAY_SIZE(fts_gestures_maps),
	.custom_initialization = fts_custom_initialization,
	.match = fts_match,
	.fetch_points = fts_fetch,
	.handle_event = fts_handle_event,
	.upgrade_firmware = NULL,
	.upgrade_status = fts_upgrade_status,
	.gesture_readdata = fts_gesture_readdata_c,
	.gesture_config = fts_gesture_config,
	.gesture_init = fts_gesture_init_c,
	.gesture_exit = fts_gesture_exit_c,
	.gesture_suspend = fts_gesture_suspend_c,
	.gesture_resume = fts_gesture_resume_c,
	.ps_resume = fts_ps_resume,
	.ps_suspend = fts_ps_suspend,
	.proximity_switch = fts_proximity_switch,
	.ps_irq_handler = fts_ps_irq_handler,
};

int ft6436u_init(void)
{
	ts_register_controller(&FtsController);
	return 0;
}

void ft6436u_exit(void)
{
	ts_unregister_controller(&FtsController);
}
