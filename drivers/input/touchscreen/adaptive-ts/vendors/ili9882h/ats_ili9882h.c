/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 *
 * $Revision: 47247 $
 * $Date: 2019-07-10 10:41:36 +0800 (Wed, 10 Jul 2019) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
//#include <linux/wakelock.h>

#if defined(CONFIG_ADF)
#include <linux/notifier.h>
#include <video/adf_notifier.h>
#endif

#if defined(CONFIG_FB)
#ifdef CONFIG_DRM_MSM
#include <linux/msm_drm_notify.h>
#endif
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#if defined(CONFIG_TPD_SENSORHUB)
#include <linux/shub_api.h>
#endif

#include"ats_ili9882.h"

struct mutex ili_i2cwr_lock;

struct ili9882h_controller {
	struct ts_controller controller;
	unsigned char a3;
	unsigned char a8;
	bool single_transfer_only;
};

#define to_ili9882h_controller(ptr) \
	container_of(ptr, struct ili9882h_controller, controller)

static const unsigned short ili9882h_addrs[] = { 0x41};

/*******************************************************
  Create Device Node (Proc Entry)
*******************************************************/

static void ili9882h_custom_initialization(void){

}
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_ILI9882H_SPI)
int g_ili9882n_info=0;
static void ili9882h_get_tp_info(void){
	TS_INFO("entry !!!");
	ili_ic_get_core_ver();
	ili_ic_get_protocl_ver();
	ili_ic_get_fw_ver();
	ili_ic_get_tp_info();
	ili_ic_get_panel_info();
	g_ili9882n_info = 1;

	memset(g_pdata->firmwork_version, 0x00, sizeof(g_pdata->firmwork_version));
	sprintf(g_pdata->firmwork_version, "0x%x", ilits->chip->tran_ver);
}
#endif
static enum ts_result ili9882h_handle_event(
	struct ts_controller *c, enum ts_event event, void *data){

	//uint8_t buf[4] = {0};
	struct device_node *pn = NULL;
	//enum ts_event ret = TSRESULT_EVENT_HANDLED;
	struct ili9882h_controller *ftc = to_ili9882h_controller(c);

	switch (event) {
	case TSEVENT_POWER_ON:
		if (data) {
			pn = (struct device_node *)data;
			if (!of_property_read_u8(pn, "a8", &ftc->a8))
				TS_DBG("parse a8 value: 0x%02X", ftc->a8);
			ftc->single_transfer_only = !!of_get_property(pn, "single-transfer-only", NULL);
			if (ftc->single_transfer_only)
				TS_DBG("single transfer only");
		}
		break;
	case TSEVENT_SUSPEND:	
		ili_sleep_handler(TP_DEEP_SLEEP);
		break;
	case TSEVENT_RESUME:
		ili_sleep_handler(TP_RESUME);
		break;
	case TSEVENT_NOISE_HIGH:
		ili_ic_func_ctrl("plug", 0);
		break;
	case TSEVENT_NOISE_NORMAL:
		ili_ic_func_ctrl("plug", 1);
		break;
	default:
		break;
	}

	return TSRESULT_EVENT_HANDLED;
}

/* read chip id and module id to match controller */
static enum ts_result ili9882h_match(struct ts_controller *c)
{
	int32_t ret = 0;

#if !defined( CONFIG_TOUCHSCREEN_ADAPTIVE_ILI9882H_SPI)
         g_client->addr=c->addrs[0];
#endif
	return (ret) ? TSRESULT_NOT_MATCHED : TSRESULT_FULLY_MATCHED;
}

static const struct ts_virtualkey_info ili9882h_virtualkeys[] = {
	DECLARE_VIRTUALKEY(120, 1500, 60, 45, KEY_BACK),
	DECLARE_VIRTUALKEY(360, 1500, 60, 45, KEY_HOMEPAGE),
	DECLARE_VIRTUALKEY(600, 1500, 60, 45, KEY_APPSELECT),
};

static const struct ts_register_info ili9882h_registers[] = {
//	DECLARE_REGISTER(TSREG_CHIP_ID, 0xA3),
/*	DECLARE_REGISTER(TSREG_MOD_ID, REG_MODULE_ID),
	DECLARE_REGISTER(TSREG_FW_VER, REG_FIRMWARE_VERSION),
	DECLARE_REGISTER("frequency", REG_SCANNING_FREQ),
	DECLARE_REGISTER("charger_indicator", REG_CHARGER_INDICATOR),
*/
};
extern struct mutex ats_tp_lock;
static int ili9882h_fetch(struct ts_controller *c, struct ts_point *points)
{
	int ret = 0, pid = 0;
	u8  checksum = 0, pack_checksum = 0;
	u8 *trdata = NULL;
	int rlen = 0;
	int tmp = debug_en;

	if (atomic_read(&ilits->cmd_int_check) == ENABLE) {
		atomic_set(&ilits->cmd_int_check, DISABLE);
		ILI_INFO("CMD INT detected, ignore\n");
		wake_up(&(ilits->inq));
		return IRQ_HANDLED;
	}

	mutex_lock(&ilits->touch_mutex);
	ilits->finger = -1;

//	ILI_INFO("flag=%d,%d,%d,%d,%d,%d\n", ilits->report, atomic_read(&ilits->tp_reset) , atomic_read(&ilits->fw_stat), 
//		atomic_read(&ilits->tp_sw_mode), atomic_read(&ilits->mp_stat), atomic_read(&ilits->tp_sleep));
	/* Just in case these stats couldn't be blocked in top half context */
	if (!ilits->report || atomic_read(&ilits->tp_reset) ||
		atomic_read(&ilits->fw_stat) || atomic_read(&ilits->tp_sw_mode) ||
		atomic_read(&ilits->mp_stat) || atomic_read(&ilits->tp_sleep)) {
		ILI_INFO("ignore report request\n");
		mutex_unlock(&ilits->touch_mutex);
		return -EINVAL;
	}

	if (ilits->irq_after_recovery) {
		ILI_INFO("ignore int triggered by recovery\n");
		ilits->irq_after_recovery = false;
		mutex_unlock(&ilits->touch_mutex);
		return -EINVAL;
	}

	ili_wq_ctrl(WQ_ESD, DISABLE);
	ili_wq_ctrl(WQ_BAT, DISABLE);

	if (ilits->actual_tp_mode == P5_X_FW_GESTURE_MODE) {
		__pm_stay_awake(ilits->ws);

		if (ilits->pm_suspend) {
			/* Waiting for pm resume completed */
			ret = wait_for_completion_timeout(&ilits->pm_completion, msecs_to_jiffies(700));
			if (!ret) {
				ILI_ERR("system(spi) can't finished resuming procedure.");
			}
		}
	}

	rlen = ilits->tp_data_len;
	ILI_DBG("Packget length = %d\n", rlen);

	if (!rlen || rlen > TR_BUF_SIZE) {
		ILI_ERR("Length of packet is invaild\n");
		goto out;
	}

	memset(ilits->tr_buf, 0x0, TR_BUF_SIZE);

	ret = ilits->wrapper(NULL, 0, ilits->tr_buf, rlen, OFF, OFF);
	if (ret < 0) {
		ILI_ERR("Read report packet failed, ret = %d\n", ret);
		if (ret == DO_SPI_RECOVER) {
			ili_ic_get_pc_counter(DO_SPI_RECOVER);
			if (ilits->prox_near) {
				ILI_ERR("33333 prox failed, doing prox recovery\n");
				if (ili_gesture_recovery() < 0)
					ILI_ERR("Failed to recover prox\n");
				ilits->irq_after_recovery = true;
			} else {
				ILI_ERR("33333 SPI ACK failed, doing spi recovery\n");
				ili_spi_recovery();
				ili_ic_func_ctrl_plug();

				ilits->irq_after_recovery = true;
			}
		}
		goto out;
	}

	ili_dump_data(ilits->tr_buf, 8, rlen, 0, "finger report");

//binhua ps on
	if((g_pdata->board->ps_status || g_pdata->board->sensorhub_status) && g_pdata->tpd_prox_active ){

/*		if ((ilits->tr_buf[0]==0xBC)&&((ilits->tr_buf[1] != 0x01) && (ilits->tr_buf[1] != 0x00))){
			ili_ic_func_ctrl("proximity", ENABLE);
			printk("[TS] reset ps mode ilits->tr_buf[1]=0x%x\n", ilits->tr_buf[1]);
		}*/

		printk_ratelimited("[TS][ILITEK] dial:%d, 0x%x, 0x%x\n", 
			g_pdata->tpd_prox_active, (u8 )ilits->tr_buf[0], (u8 )ilits->tr_buf[1] );
		if(ilits->tr_buf[0]==0xBC){
			ilits->finger = 0;

			if (0x01 == ilits->tr_buf[1]){//near
				c->pdata->ps_buf = 0xc0;
			}
			else if (0x00 == ilits->tr_buf[1]){//far-away
				c->pdata->ps_buf = 0xe0;
			}
			else{
				ILI_ERR("ilits->tr_buf[1] data error /ps failed!!\n");
			}
			goto out;
		}
	}

	trdata = ilits->tr_buf;
	pid = trdata[0];
	ILI_DBG("Packet ID = %x\n", pid);

	if(ilits->tr_buf[0] != 0xbc){
		checksum = ili_calc_packet_checksum(ilits->tr_buf, rlen - 1);
		pack_checksum = ilits->tr_buf[rlen-1];

		if (checksum != pack_checksum && pid != P5_X_I2CUART_PACKET_ID) {
			ILI_ERR("Checksum Error (0x%X)! Pack = 0x%X, len = %d\n", checksum, pack_checksum, rlen);
			debug_en = DEBUG_ALL;
			ili_dump_data(trdata, 8, rlen, 0, "finger report with wrong");
			debug_en = tmp;
			ret = -EINVAL;
			goto out;
		}
	}else{//binhua ps on
		goto out;
	}

	if (pid == P5_X_INFO_HEADER_PACKET_ID) {
		trdata = ilits->tr_buf + P5_X_INFO_HEADER_LENGTH;
		pid = trdata[0];
	}

	ilits->points = points;
	switch (pid) {
	case P5_X_DEMO_PACKET_ID:
		ili_report_ap_mode(trdata, rlen);
		break;
	case P5_X_DEBUG_PACKET_ID:
		ili_report_debug_mode(trdata, rlen);
		break;
	case P5_X_DEBUG_LITE_PACKET_ID:
		ili_report_debug_lite_mode(trdata, rlen);
		break;
	case P5_X_I2CUART_PACKET_ID:
		ili_report_i2cuart_mode(trdata, rlen);
		break;
	case P5_X_GESTURE_PACKET_ID:
		ili_report_gesture_mode(trdata, rlen);
		break;
	case P5_X_GESTURE_FAIL_ID:
		ILI_INFO("gesture fail reason code = 0x%02x", trdata[1]);
		break;
	case P5_X_DEMO_DEBUG_INFO_PACKET_ID:
		ili_demo_debug_info_mode(trdata, rlen);
		break;
	default:
		mutex_lock(&ats_tp_lock);
		ILI_DBG("Unknown packet id, %x\n", pid);
#if ( TDDI_INTERFACE == BUS_I2C )
		msleep(50);
		ili_ic_get_pc_counter(DO_I2C_RECOVER);
		if (ilits->fw_latch !=0){
			msleep(50);
			ili_ic_func_ctrl_reset();
			ILI_ERR("I2C func_ctrl_reset\n");
		}
		if ((ilits->actual_tp_mode == P5_X_FW_GESTURE_MODE) && ilits->fw_latch !=0){
			msleep(50);
			ili_set_gesture_symbol();
			ILI_ERR("I2C gesture_symbol\n");
		}
#endif
		mutex_unlock(&ats_tp_lock);
		break;
	}

out:
	if (ilits->actual_tp_mode != P5_X_FW_GESTURE_MODE) {
		ili_wq_ctrl(WQ_ESD, ENABLE);
		ili_wq_ctrl(WQ_BAT, ENABLE);
	}

	if (ilits->actual_tp_mode == P5_X_FW_GESTURE_MODE)
		__pm_relax(ilits->ws);

	c->pdata->touch_point = (unsigned short)ilits->finger;
	mutex_unlock(&ilits->touch_mutex);

	return ilits->finger;
}

/* firmware upgrade procedure */
/*
static enum ts_result ili9882h_upgrade_firmware(struct ts_controller *c,
	const unsigned char *data, size_t size, bool force)
{
  	return 1;
}*/
extern uint32_t lcd_moden_from_uboot;
int  ili9882h_upgrade_init(void){

	int ret=0, i=0;
	uint8_t *tpd_vendor_id={0};
	int nvt_vendor_id=0;
	uint8_t nvt_vendor_name[16]={0};
	u32 *tpd_firmware_update={0};

#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_ILI9882H_SPI)
	ilits->update_node = ilits->spi->dev.of_node;
#else
	ilits->update_node = ilits->i2c->dev.of_node;
#endif
	ilits->update_node = of_get_child_by_name(ilits->update_node, "ats_ili9882h");
	of_property_read_u32(ilits->update_node, "tp_vendor_num", &ilits->vendor_nums);

	tpd_vendor_id = kmalloc(ilits->vendor_nums, GFP_KERNEL);
	if(tpd_vendor_id == NULL){
		ILI_ERR("tpd_vendor_id kmalloc is not found\n");
		return -1;
	}

	tpd_firmware_update = kmalloc(ilits->vendor_nums*sizeof(u32), GFP_KERNEL);
	if(tpd_firmware_update == NULL){
		ILI_ERR("tpd_firmware kmalloc is not found\n");
		return -1;
	}

	of_property_read_u32_array(ilits->update_node, "tp_upgrade_fw", tpd_firmware_update, ilits->vendor_nums);
	of_property_read_u8_array(ilits->update_node, "tp_vendor_id", tpd_vendor_id, ilits->vendor_nums);
/*
	for(i=0; i<ilits->vendor_nums; i++){
		ILI_INFO("[tpd] tp_vendor_id[%d] = 0x%x\n",  i, tpd_vendor_id[i]);
		ILI_INFO("[tpd] tp_upgrade_switch[%d] = %d\n",  i, tpd_firmware_update[i]);
	}
*/
	if(lcd_moden_from_uboot==0)
		nvt_vendor_id = 0x01;
	else if(lcd_moden_from_uboot==1)
		nvt_vendor_id = 0x02;
	else
		nvt_vendor_id = 0x01;

	ILI_DBG("[tpd] ili_vendor_id=0x%x \n", nvt_vendor_id);

	for(i=0; i <  ilits->vendor_nums; i++ ){
		if (tpd_vendor_id[i] ==nvt_vendor_id){

			ilits->vendor_num = i;
			g_pdata->firmware_update_switch = tpd_firmware_update[i];

			sprintf(nvt_vendor_name, "tp_vendor_name%d", i);
			of_property_read_string(ilits->update_node, nvt_vendor_name, (char const **)&g_pdata->vendor_string);

			break;
		}
	}

	if(g_pdata->firmware_update_switch){
		if(!ili_fw_upgrade_handler(NULL))
			ret =1;
	}

	memset(g_pdata->firmwork_version, 0x00, sizeof(g_pdata->firmwork_version));
	sprintf(g_pdata->firmwork_version, "0x%x", ilits->chip->tran_ver);

	memset(g_pdata->chip_name, 0x00, sizeof(g_pdata->chip_name));
	sprintf(g_pdata->chip_name, "ili%x", ilits->chip->id);

	return ret;
}

int  ili9882h_upgrade_status(struct ts_controller *c)
{
	int ret=-1;
	static int is_init=1;

	mutex_lock(&ilits->touch_mutex);

	if(is_init){
		is_init=0;
		ret = ili9882h_upgrade_init();
	}
	else{
		if(!ili_fw_upgrade_handler(NULL))
			ret =1;
	}
	mutex_unlock(&ilits->touch_mutex);

	return ret;
}

static void ili9882h_ps_reset(void){

}

int ili9882h_read_id(void){

	ilits = devm_kzalloc(&g_client->dev, sizeof(struct ilitek_ts_data), GFP_KERNEL);
	if (ERR_ALLOC_MEM(ilits)) {
		ILI_ERR("Failed to allocate ts memory, %ld\n", PTR_ERR(ilits));
		return -1;
	}

	/* Used for receiving touch data only, do not mix up with others. */
	ilits->tr_buf = kzalloc(TR_BUF_SIZE, GFP_ATOMIC);
	if (ERR_ALLOC_MEM(ilits->tr_buf)) {
		ILI_ERR("failed to allocate touch report buffer\n");
		return -1;
	}

	ilits->gcoord = kzalloc(sizeof(struct gesture_coordinate), GFP_KERNEL);
	if (ERR_ALLOC_MEM(ilits->gcoord)) {
		ILI_ERR("Failed to allocate gresture coordinate buffer\n");
		return -1;
	}

#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_ILI9882H_SPI)
	ilits->update_buf = kzalloc(MAX_HEX_FILE_SIZE, GFP_KERNEL | GFP_DMA);
	if (ERR_ALLOC_MEM(ilits->update_buf)) {
		ILI_ERR("fw kzalloc error\n");
		return -ENOMEM;
	}

	/* Used for receiving touch data only, do not mix up with others. */

	ilits->spi_tx = kzalloc(SPI_TX_BUF_SIZE, GFP_KERNEL | GFP_DMA);
	if (ERR_ALLOC_MEM(ilits->spi_tx)) {
		ILI_ERR("Failed to allocate spi tx buffer\n");
		return -ENOMEM;
	}

	ilits->spi_rx = kzalloc(SPI_RX_BUF_SIZE, GFP_KERNEL | GFP_DMA);
	if (ERR_ALLOC_MEM(ilits->spi_rx)) {
		ILI_ERR("Failed to allocate spi rx buffer\n");
		return -ENOMEM;
	}

	ilits->i2c = NULL;
	ilits->spi = g_client;
	ilits->dev = &g_client->dev;
	//ilits->hwif = info->hwif;
	ilits->phys = "SPI";
	ilits->wrapper = ili_spi_wrapper;
	ilits->detect_int_stat = ili_ic_check_int_pulse;
	ilits->int_pulse = true;
	ilits->mp_retry = false;

#if SPI_DMA_TRANSFER_SPLIT
	ilits->spi_write_then_read = ili_spi_write_then_read_split;
#else
	ilits->spi_write_then_read = ili_spi_write_then_read_direct;
#endif

	ilits->actual_tp_mode = P5_X_FW_AP_MODE;
	ilits->tp_data_format = DATA_FORMAT_DEMO;
	ilits->tp_data_len = P5_X_DEMO_MODE_PACKET_LEN;

	if (TDDI_RST_BIND)
		ilits->reset = TP_IC_WHOLE_RST;
	else
		ilits->reset = TP_HW_RST_ONLY;

	ilits->rst_edge_delay = 10;
	ilits->fw_open = FILP_OPEN;
	ilits->fw_upgrade_mode = UPGRADE_IRAM;
	ilits->mp_move_code = ili_move_mp_code_iram;
	ilits->gesture_move_code = ili_move_gesture_code_iram;
	ilits->esd_recover = ili_wq_esd_spi_check;
	ilits->ges_recover = ili_touch_esd_gesture_iram;
	ilits->gesture_mode = DATA_FORMAT_GESTURE_INFO;
	ilits->gesture_demo_ctrl = DISABLE;
	ilits->wtd_ctrl = OFF;
	ilits->report = ENABLE;
	ilits->netlink = DISABLE;
	ilits->dnp = DISABLE;
	ilits->irq_tirgger_type = IRQF_TRIGGER_FALLING;
	ilits->info_from_hex = ENABLE;//����DISABLE�Ǵ�IC��ȡ�̼��汾�ţ�����ENABLE���Ǵ�DTSI��ȡ�̼��汾��
	ilits->wait_int_timeout = AP_INT_TIMEOUT;
#else
	ilits->i2c = g_client;
	ilits->spi = NULL;
	ilits->dev = &g_client->dev;
//	ilits->hwif = info->hwif;
	ilits->phys = "I2C";
	ilits->wrapper = ili_i2c_wrapper;
	ilits->detect_int_stat = ili_ic_check_int_pulse;
	ilits->int_pulse = true;
	ilits->mp_retry = false;

	ilits->actual_tp_mode = P5_X_FW_AP_MODE;

	if (TDDI_RST_BIND)
		ilits->reset = TP_IC_WHOLE_RST;
	else
		ilits->reset = TP_HW_RST_ONLY;

	ilits->rst_edge_delay = 100;
	ilits->fw_open = FILP_OPEN;
	ilits->fw_upgrade_mode = UPGRADE_FLASH;
	ilits->mp_move_code = ili_move_mp_code_flash;
	ilits->gesture_move_code = ili_move_gesture_code_flash;
	ilits->esd_recover = ili_wq_esd_i2c_check;
	ilits->ges_recover = ili_touch_esd_gesture_flash;
	ilits->gesture_mode = DATA_FORMAT_GESTURE_INFO;
	ilits->gesture_demo_ctrl = DISABLE;
	ilits->wtd_ctrl = OFF;
	ilits->report = ENABLE;
	ilits->netlink = DISABLE;
	ilits->dnp = DISABLE;
	ilits->irq_tirgger_type = IRQF_TRIGGER_FALLING;
	ilits->info_from_hex = DISABLE;
	ilits->wait_int_timeout = AP_INT_TIMEOUT;

	g_client->addr = TDDI_I2C_ADDR;//0x41;
#endif

	ilits->tp_int = g_board_b->int_gpio;
	ilits->tp_rst =	g_board_b->rst_gpio;
	//ilits->irq_num = g_board_b->irq;

	mutex_init(&ili_i2cwr_lock);

	if (ili_tddi_init() < 0) {
		ILI_ERR("ILITEK Driver probe failed\n");
		return -1;
	}

	ilits->pm_suspend = false;

	if (ili_ice_mode_ctrl(ENABLE, OFF) < 0) 
		ILI_ERR("Failed to enable ice mode during ili_tddi_init\n");

	if ( ili_ic_dummy_check() < 0 ){
		ILI_ERR("Not found ilitek chip\n");
		if(ilits&&ilits->gcoord)
			kfree(ilits->gcoord);
		if(ilits&&ilits->tr_buf)
			kfree(ilits->tr_buf);
		if(ilits&&ilits->update_buf)
			kfree(ilits->update_buf);
		if(ilits&&ilits->spi_tx)
			kfree(ilits->spi_tx);
		if(ilits&&ilits->spi_rx)
			kfree(ilits->spi_rx);
		if(ilits)
			kfree(ilits);

		return -1;
	}

	if (ili_ice_mode_ctrl(DISABLE, OFF) < 0)
		ILI_ERR("Failed to disable ice mode failed during init\n");

	return 1;
}

static int ili9882h_ps_resume(struct ts_data *pdata){

#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_ILI9882H_SPI)
	if(pdata->tpm_status == TPM_SUSPENDED)//奕力屏由于LCD TP复位引脚是分开的，在开PS时，如果是靠近灭屏，然后亮屏，则不需要重新下固件
	{
		if(0 != ili_fw_upgrade_handler(NULL))
			TS_INFO("ts upgrade firmware failed !!!");
	}
#endif

	if (ts_get_mode_ext(pdata, TSMODE_PS_STATUS)||ts_get_mode_ext(pdata, TSMODE_SENSORHUB_STATUS)){
		if (pdata->tpd_prox_active && pdata->tpm_status == TPM_DESUSPEND) {

#if !defined(CONFIG_TOUCHSCREEN_ADAPTIVE_ILI9882H_SPI)
			if((pdata->tpd_prox_old_state==0xc0)||(pdata->tpd_prox_old_state==0xf0)){
				ts_reset_controller_ex(pdata, true);
				ili_ic_func_ctrl("proximity", 1);
			}
			else
#endif
				ili_ic_proximity_resume();
			pdata->tpd_prox_old_state = 0x0f;

			printk("[TS] ps_resume ps is on, so return !!!\n");
			return 0;
		}
	}

	if (ts_get_mode_ext(pdata, TSMODE_PS_STATUS)||ts_get_mode_ext(pdata, TSMODE_SENSORHUB_STATUS)){
		if(pdata->tps_status == TPS_DEON && pdata->tpd_prox_active){

			ili_ic_func_ctrl("proximity", 1);
			pdata->tps_status = TPS_ON;
		}
	}

	return 1;
}

static int ili9882h_ps_suspend(struct ts_data *pdata){

	if (ts_get_mode_ext(pdata, TSMODE_PS_STATUS)||ts_get_mode_ext(pdata, TSMODE_SENSORHUB_STATUS)){
		if (pdata->tpd_prox_active) {

			ili_ic_func_ctrl("lpwg", 0x20); //通知IC在灭屏情况下要去检测接近远离动作不然灭屏后不会检测的
			ts_clear_points_ext(pdata);

			printk("[TS] ps_suspend:ps is on, so return!!!\n");
			return 0;
		}
	}

	return 1;
}

static void ili9882h_proximity_switch(bool onoff){

	if(onoff)//proximity on
		ili_ic_func_ctrl("proximity", 1);
	else//proximity off
		ili_ic_func_ctrl("proximity", 0);
}

static void ili9882h_ps_irq_handler(struct ts_data *pdata){

	pdata->tpd_prox_old_state = pdata->ps_buf;
}

static struct ili9882h_controller ili9882h = {
	.controller = {
		.name = "ILI9882H",
		.vendor = "ili9882h",
		.incell = 1,
		.config = TSCONF_ADDR_WIDTH_BYTE
			| TSCONF_POWER_ON_RESET
			| TSCONF_RESET_LEVEL_LOW
			| TSCONF_REPORT_MODE_IRQ
			| TSCONF_IRQ_TRIG_EDGE_FALLING
			| TSCONF_REPORT_TYPE_3,
		.addr_count = ARRAY_SIZE(ili9882h_addrs),
		.addrs = ili9882h_addrs,
		.virtualkey_count = ARRAY_SIZE(ili9882h_virtualkeys),
		.virtualkeys = ili9882h_virtualkeys,
		.register_count = ARRAY_SIZE(ili9882h_registers),
		.registers = ili9882h_registers,
		.panel_width = 720,
		.panel_height = 1600,
		.reset_keep_ms = 5,
		.reset_delay_ms = 10,
		.parser = {
		},
		.ps_reset = ili9882h_ps_reset,
		.custom_initialization = ili9882h_custom_initialization,
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_ILI9882H_SPI)
		.get_tp_info = ili9882h_get_tp_info,
#endif
		.match = ili9882h_match,
		.fetch_points = ili9882h_fetch,
		.handle_event = ili9882h_handle_event,
		.upgrade_firmware = NULL,//ili9882h_upgrade_firmware,
		.upgrade_status = ili9882h_upgrade_status,
		.gesture_readdata = NULL,
		.gesture_init = NULL,
		.gesture_exit = NULL,
		.gesture_suspend = NULL,
		.gesture_resume = NULL,
		.ps_resume = ili9882h_ps_resume,
		.ps_suspend = ili9882h_ps_suspend,
		.proximity_switch = ili9882h_proximity_switch,
		.ps_irq_handler = ili9882h_ps_irq_handler,

	},
	.a3 = 0x54,
	.a8 = 0x87,
	.single_transfer_only = false,
};


int ili9882h_init(void)
{       
	ts_register_controller(&ili9882h.controller);
	return 0;
}

void ili9882h_exit(void)
{       
	ts_unregister_controller(&ili9882h.controller);
}
