#define LOG_TAG         "Driver"

#include "ats_cts_config.h"
#include "ats_cts_platform.h"
#include "ats_cts_core.h"
#include "ats_cts_sysfs.h"
#include "ats_cts_charger_detect.h"
#include "ats_cts_earjack_detect.h"
#include "ats_cts_strerror.h"
#include "ats_cts_firmware.h"

struct device *g_cts_dev=NULL;

struct icnl9911x_controller {
	struct ts_controller controller;
	unsigned char a3;
	unsigned char a8;
	bool single_transfer_only;
};

#define to_icnl9911x_controller(ptr) \
	container_of(ptr, struct icnl9911x_controller, controller)

static const unsigned short icnl9911x_addrs[] = { 0x48};

static const struct ts_virtualkey_info icnl9911x_virtualkeys[] = {
	DECLARE_VIRTUALKEY(120, 1500, 60, 45, KEY_BACK),
	DECLARE_VIRTUALKEY(360, 1500, 60, 45, KEY_HOMEPAGE),
	DECLARE_VIRTUALKEY(600, 1500, 60, 45, KEY_APPSELECT),
};

static const struct ts_register_info icnl9911x_registers[] = {
	DECLARE_REGISTER(TSREG_CHIP_ID, 0xA3),
/*	DECLARE_REGISTER(TSREG_MOD_ID, REG_MODULE_ID),
	DECLARE_REGISTER(TSREG_FW_VER, REG_FIRMWARE_VERSION),
	DECLARE_REGISTER("frequency", REG_SCANNING_FREQ),
	DECLARE_REGISTER("charger_indicator", REG_CHARGER_INDICATOR),
*/
};

static void icnl9911x_ps_reset(void) {

}

static void icnl9911x_custom_initialization(void) {

	int ret = 0;
	struct chipone_ts_data *cts_data = dev_get_drvdata(g_cts_dev);

	ret = cts_tool_init(cts_data);
	if (ret < 0) {
		cts_warn("Init tool node failed %d", ret);
	}

	ret = cts_sysfs_add_device(cts_data->device);
	if (ret < 0) {
		cts_warn("Add sysfs entry for device failed %d", ret);
	}
}

int icnl9911x_cts_driver_init(u32 *hwid) {

	int ret = 0;
	struct chipone_ts_data *cts_data = NULL;

	cts_info("cts_driver_init entry!!!");
	cts_data = kzalloc(sizeof(struct chipone_ts_data), GFP_KERNEL);
	if (cts_data == NULL) {
		cts_err("Alloc chipone_ts_data failed");
		return -ENOMEM;
	}

	cts_data->pdata = kzalloc(sizeof(struct cts_platform_data), GFP_KERNEL);
	if (cts_data->pdata == NULL) {
		cts_err("Alloc cts_platform_data failed");
		ret = -ENOMEM;
		goto err_free_cts_data;
	}

#if defined( CONFIG_CTS_SPI_HOST)
	cts_data->pdata->spi_client = g_client;
	cts_data->cts_dev.bus_type = CTS_SPI_BUS;
	//cts_data->pdata->spi_speed = CFG_CTS_SPI_SPEED_KHZ;
#else
	//cts_data->pdata->I2C_client = g_client;
	cts_data->cts_dev.bus_type = CTS_I2C_BUS;
#endif

	cts_data->device = &g_client->dev;
	g_cts_dev = cts_data->device;
    	cts_data->cts_dev.pdata = cts_data->pdata;
    	cts_data->pdata->cts_dev = &cts_data->cts_dev;

	cts_data->pdata->int_gpio = g_board_b->int_gpio;
	cts_data->pdata->rst_gpio = g_board_b->rst_gpio;
	cts_data->pdata->res_x = g_board_b->panel_width;
	cts_data->pdata->res_y = g_board_b->panel_height;

	ret = cts_probe_device_ext(&cts_data->cts_dev);
	if(!ret)
		*hwid = cts_data->pdata->cts_dev->hwdata->hwid;

	dev_set_drvdata(cts_data->device, cts_data);

	rt_mutex_init(&cts_data->pdata->dev_lock);

	cts_info("cts_driver_init end!!!");

	return ret;

err_free_cts_data:
	kfree(cts_data);

	return ret;
}

int icnl9911x_driver_get_hwid(void){

	int ret = -1;
	u32 hwid=0;

	icnl9911x_cts_driver_init(&hwid);

	if(hwid==0x991110)
		ret =1;

	return ret;
}

/* read chip id and module id to match controller */
static enum ts_result icnl9911x_match(struct ts_controller *c)
{
	int32_t ret = -1;
	struct chipone_ts_data *cts_data = dev_get_drvdata(g_cts_dev);

	if(cts_data->cts_dev.hwdata->hwid==0x991110)
		ret =0;

	return (ret) ? TSRESULT_NOT_MATCHED : TSRESULT_FULLY_MATCHED;
}

u8 g_ps_state_old;
static int icnl9911x_fetch(struct ts_controller *c, struct ts_point *points)
{
	int32_t i = 0, j = 0;
	unsigned short x, y;
	u8  ps_state;

        struct cts_device_touch_info *touch_info;
	struct chipone_ts_data *cts_data = dev_get_drvdata(g_cts_dev);

	rt_mutex_lock(&cts_data->pdata->dev_lock);
        touch_info = &cts_data->cts_dev.pdata->touch_info;

	cts_get_touchinfo_ext(&cts_data->cts_dev, touch_info);

	cts_dbg("Process touch %d msgs", touch_info->num_msg);
	ps_state = cts_data->cts_dev.pdata->touch_info.vkey_state;

	if(g_pdata->board->ps_status || g_pdata->board->sensorhub_status){
		if(g_pdata->tpd_prox_active){

			if ((ps_state!=0x80) && (ps_state!=0x00)){
				//cts_send_command(&cts_data->cts_dev, CTS_CMD_PROXI_EN);
				//printk("[TS] reset ps mode ps_state=0x%x\n", ps_state);
			}

			printk_ratelimited("[TS] dial:%d, ps_state=0x%x\n", g_pdata->tpd_prox_active, ps_state );

			if (ps_state==0x80){//near
				//cts_info("binhua near !!!");
				c->pdata->ps_buf = 0xc0;
				c->pdata->touch_point = 0;
				goto OUT_REPORT;
			}
			else if (ps_state==0x00 && g_ps_state_old == 0x80){//far-away
				//cts_info("binhua far-away !!!");
				c->pdata->ps_buf = 0xe0;
				c->pdata->touch_point = 0;
				goto OUT_REPORT;
			}
		}
	}

	if (touch_info->num_msg ==0 || touch_info->num_msg > CFG_CTS_MAX_TOUCH_NUM) {

		//cts_err("num_msg data error !!\n");
		rt_mutex_unlock(&cts_data->pdata->dev_lock);

		c->pdata->touch_point = 0;
		goto OUT_REPORT;
	}

	for (i = 0; i < touch_info->num_msg; i++) {

		x = le16_to_cpu(touch_info->msgs[i].x);
		y = le16_to_cpu(touch_info->msgs[i].y);

		points[j].x = (unsigned short)x;
		points[j].y = (unsigned short)y;

		cts_dbg("  ............Process touch msg[%d]: id[%d] ev=%d x=%d y=%d p=%d",
		    i, touch_info->msgs[i].id, touch_info->msgs[i].event, x, y, touch_info->msgs[i].pressure);

		if (touch_info->msgs[i].event == CTS_DEVICE_TOUCH_EVENT_DOWN
		|| touch_info->msgs[i].event == CTS_DEVICE_TOUCH_EVENT_MOVE
		|| touch_info->msgs[i].event == CTS_DEVICE_TOUCH_EVENT_STAY) {
			if (touch_info->msgs[i].id < CFG_CTS_MAX_TOUCH_NUM) {
				points[j].pressed = 1;
			}else
				points[j].pressed = 0;
		}
		else
			points[i].pressed = 0;

		//points[i].pressed = touch_info->msgs[i].event;
		points[j].slot = touch_info->msgs[i].id;//MT_TOOL_FINGER
		points[j].pressure = (unsigned short)touch_info->msgs[i].pressure;//ABS_MT_PRESSURE
		points[j].touch_major = (unsigned short)touch_info->msgs[i].pressure;//ABS_MT_TOUCH_MAJOR
		//printk("[[TS]NVT-ts]:X:Y:%d %d", points[j].x, points[j].y);

		j++;
	}

	c->pdata->touch_point = (unsigned short)touch_info->num_msg;

OUT_REPORT:

	if(j==0){
		j=c->pdata->board->max_touch_num;
	}

	rt_mutex_unlock(&cts_data->pdata->dev_lock);

	if(g_pdata->tpd_prox_active)
		g_ps_state_old = ps_state;

	return j;
}

static enum ts_result icnl9911x_handle_event(
	struct ts_controller *c, enum ts_event event, void *data)
{

	struct device_node *pn = NULL;
	struct icnl9911x_controller *ftc = to_icnl9911x_controller(c);
	struct chipone_ts_data *cts_data = dev_get_drvdata(g_cts_dev);

	switch (event) {
	case TSEVENT_POWER_ON:
		if (data) {
			pn = (struct device_node *)data;
			if (!of_property_read_u8(pn, "a8", &ftc->a8))
				cts_dbg("parse a8 value: 0x%02X", ftc->a8);
			ftc->single_transfer_only = !!of_get_property(pn, "single-transfer-only", NULL);
			if (ftc->single_transfer_only)
				cts_dbg("single transfer only");
		}
		break;
	case TSEVENT_SUSPEND:	
		cts_suspend_device(&cts_data->cts_dev);
		break;
	case TSEVENT_RESUME:
		cts_resume_device(&cts_data->cts_dev);
		break;
	case TSEVENT_NOISE_HIGH:
		cts_set_dev_charger_attached(&cts_data->cts_dev, true);
		break;
	case TSEVENT_NOISE_NORMAL:
		cts_set_dev_charger_attached(&cts_data->cts_dev, false);
		break;
	default:
		break;
	}

	return TSRESULT_EVENT_HANDLED;
}

extern uint32_t lcd_moden_from_uboot;
int  incl9911x_upgrade_init(void)
{
	int ret=-1, i=0;
	uint8_t *tpd_vendor_id={0};
	int nvt_vendor_id=0;
	uint8_t nvt_vendor_name[16]={0};
	uint8_t *tpd_firmware_update={0};
	struct device_node *update_node;
	struct chipone_ts_data *cts_data = dev_get_drvdata(g_cts_dev);

	update_node = cts_data->device->of_node;

	update_node = of_get_child_by_name(update_node, "ats_incl9911x");
	of_property_read_u32(update_node, "tp_vendor_num", &cts_data->pdata->vendor_nums);
	cts_data->pdata->update_node = update_node;

	tpd_vendor_id = kmalloc(cts_data->pdata->vendor_nums, GFP_KERNEL);
	if(tpd_vendor_id == NULL){
		cts_err("tpd_vendor_id kmalloc is not found\n");
		return -1;
	}

	tpd_firmware_update = kmalloc((cts_data->pdata->vendor_nums < sizeof(u32)) ? sizeof(u32): cts_data->pdata->vendor_nums, GFP_KERNEL);
	if(tpd_firmware_update == NULL){
		cts_err("tpd_firmware kmalloc is not found\n");
		return -1;
	}

	of_property_read_u32(update_node, "tp_upgrade_fw", (u32 *)tpd_firmware_update);
	of_property_read_u8_array(update_node, "tp_vendor_id", tpd_vendor_id, cts_data->pdata->vendor_nums);

	for(i=0; i<cts_data->pdata->vendor_nums; i++){
		cts_info("[tpd] tp_vendor_id[%d] = 0x%x\n",  i, tpd_vendor_id[i]);
		cts_info("[tpd] tp_upgrade_switch[%d] = %d\n",  i, tpd_firmware_update[i]);
	}

	if(lcd_moden_from_uboot==0)
		nvt_vendor_id = 0x01;
	else if(lcd_moden_from_uboot==1)
		nvt_vendor_id = 0x02;
	else
		nvt_vendor_id = 0x01;

	cts_info("[tpd] ili_vendor_id=0x%x \n", nvt_vendor_id);

	for(i=0; i <  cts_data->pdata->vendor_nums; i++ ){
		if (tpd_vendor_id[i] ==nvt_vendor_id){

			cts_data->pdata->vendor_num = i;
			g_pdata->firmware_update_switch = tpd_firmware_update[i];

			sprintf(nvt_vendor_name, "tp_vendor_name%d", i);
			of_property_read_string(update_node, nvt_vendor_name, (char const **)&g_pdata->vendor_string);

			break;
		}
	}

	memset(g_pdata->chip_name, 0x00, sizeof(g_pdata->chip_name));
	memcpy(g_pdata->chip_name, cts_data->pdata->cts_dev->hwdata->name, strlen(cts_data->pdata->cts_dev->hwdata->name));

	return ret;
}

int  icnl9911x_upgrade_status(struct ts_controller *c) {

	int ret=-1;

	static int is_first_init=1;
        const struct cts_firmware *firmware;
	struct chipone_ts_data *cts_data = dev_get_drvdata(g_cts_dev);

	if(is_first_init){
		is_first_init=0;
		ret = incl9911x_upgrade_init();
	}

        cts_dbg("Need update firmware when resume");
        firmware = cts_request_firmware(cts_data->cts_dev.hwdata->hwid,
                cts_data->cts_dev.hwdata->fwid, 0);

        if (firmware) {
		ret = cts_update_firmware(&cts_data->cts_dev, firmware, true);

		memset(g_pdata->firmwork_version, 0x00, sizeof(g_pdata->firmwork_version));
		sprintf(g_pdata->firmwork_version, "0x%02x", cts_data->pdata->cts_dev->fwdata.version&0xff);

		cts_release_firmware(firmware);

		if (ret) {
			cts_err("Update default firmware failed %d", ret);
			goto err_set_program_mode;
		}
        } else {
		cts_err("Request default firmware failed %d, "
			"please update manually!!", ret);

            	goto err_set_program_mode;
        }

	return 1;

err_set_program_mode:
	cts_data->cts_dev.rtdata.program_mode = true;
	cts_data->cts_dev.rtdata.slave_addr   = CTS_DEV_PROGRAM_MODE_SPIADDR;
	cts_data->cts_dev.rtdata.addr_width   = CTS_DEV_PROGRAM_MODE_ADDR_WIDTH;

	return ret;

}

static int icnl9911x_ps_resume(struct ts_data *pdata) {

	struct chipone_ts_data *cts_data = dev_get_drvdata(g_cts_dev);

	if(g_fw_control_icnl){//FAE调试时需要，此TP在VIO18不下电的情况下，不需要重新加载固件，即只是AVDD AVEE掉电时，固件还是在的，此TP只在开机时加载一次固件即可。
		if(icnl9911x_upgrade_status(NULL)!=1)
			cts_err("icnl9911x_spi upgrade firmware failed !!!");
	}

	if (ts_get_mode_ext(pdata, TSMODE_PS_STATUS)||ts_get_mode_ext(pdata, TSMODE_SENSORHUB_STATUS)){
		if (pdata->tpd_prox_active && (pdata->tpm_status == TPM_DESUSPEND)) {

			cts_send_command(&cts_data->cts_dev, CTS_CMD_PROXI_EN);
			pdata->tpd_prox_old_state = 0x0f;
			printk("[TS] ps_resume ps is on, so return !!!\n");
			return 0;
		}
	}

	ts_reset_controller_ex(pdata, true);

	if (ts_get_mode_ext(pdata, TSMODE_PS_STATUS)||ts_get_mode_ext(pdata, TSMODE_SENSORHUB_STATUS)){
		if(pdata->tps_status == TPS_DEON && pdata->tpd_prox_active){

			cts_send_command(&cts_data->cts_dev, CTS_CMD_PROXI_EN);
			pdata->tps_status = TPS_ON ;
		}
	}

	return 1;
}

static int icnl9911x_ps_suspend(struct ts_data *pdata) {

	if (ts_get_mode_ext(pdata, TSMODE_PS_STATUS)||ts_get_mode_ext(pdata, TSMODE_SENSORHUB_STATUS)){
		if (pdata->tpd_prox_active) {

			ts_clear_points_ext(pdata);
			printk("[TS] ps_suspend:ps is on, so return!!!\n");
			return 0;
		}
	}

	return 1;
}

static void icnl9911x_proximity_switch(bool onoff) {

	struct chipone_ts_data *cts_data = dev_get_drvdata(g_cts_dev);

	if(onoff)
		cts_send_command(&cts_data->cts_dev, CTS_CMD_PROXI_EN);
	else
		cts_send_command(&cts_data->cts_dev, CTS_CMD_PROXI_DISABLE);
}

static void icnl9911x_ps_irq_handler(struct ts_data *pdata) {

	if (ts_get_mode_ext(pdata, TSMODE_PS_STATUS)){
		if(((pdata->ps_buf== 0xc0) || (pdata->ps_buf== 0xe0)) && (pdata->tpd_prox_old_state != pdata->ps_buf)){
			pdata->tpd_prox_old_state = pdata->ps_buf;
		}
	}

	if(ts_get_mode_ext(pdata, TSMODE_SENSORHUB_STATUS)){
		if(pdata->tpd_prox_old_state != pdata->ps_buf){
			pdata->tpd_prox_old_state = pdata->ps_buf;
		}
	}
}

static struct icnl9911x_controller icnl9911x = {
	.controller = {
		.name = "ICNL9911X",
		.vendor = "icnl9911x",
		.incell = 1,
		.config = TSCONF_ADDR_WIDTH_BYTE
			| TSCONF_POWER_ON_RESET
			| TSCONF_RESET_LEVEL_LOW
			| TSCONF_REPORT_MODE_IRQ
			| TSCONF_IRQ_TRIG_EDGE_FALLING
			| TSCONF_REPORT_TYPE_3,
		.addr_count = ARRAY_SIZE(icnl9911x_addrs),
		.addrs = icnl9911x_addrs,
		.virtualkey_count = ARRAY_SIZE(icnl9911x_virtualkeys),
		.virtualkeys = icnl9911x_virtualkeys,
		.register_count = ARRAY_SIZE(icnl9911x_registers),
		.registers = icnl9911x_registers,
		.panel_width = 720,
		.panel_height = 1640,
		.reset_keep_ms = 10,
		.reset_delay_ms = 40,
		.parser = {
		},
		.ps_reset = icnl9911x_ps_reset,
		.custom_initialization = icnl9911x_custom_initialization,
		.match = icnl9911x_match,
		.fetch_points = icnl9911x_fetch,
		.handle_event = icnl9911x_handle_event,
		.upgrade_firmware = NULL,//icnl9911x_upgrade_firmware,
		.upgrade_status = icnl9911x_upgrade_status,
		.gesture_readdata = NULL,
		.gesture_init = NULL,
		.gesture_exit = NULL,
		.gesture_suspend = NULL,
		.gesture_resume = NULL,
		.ps_resume = icnl9911x_ps_resume,
		.ps_suspend = icnl9911x_ps_suspend,
		.proximity_switch = icnl9911x_proximity_switch,
		.ps_irq_handler = icnl9911x_ps_irq_handler,

	},
	.a3 = 0x54,
	.a8 = 0x87,
	.single_transfer_only = false,
};

int icnl9911x_init(void)
{
	ts_register_controller(&icnl9911x.controller);
	return 0;
}

void icnl9911x_exit(void)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(g_cts_dev);

	ts_unregister_controller(&icnl9911x.controller);

        cts_tool_deinit(cts_data);
        cts_sysfs_remove_device(cts_data->device);

}

