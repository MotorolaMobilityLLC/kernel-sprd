#include <linux/delay.h>
#include "adaptive_ts.h"
#include <linux/module.h>
#include <uapi/linux/input.h>

static enum ts_result handle_event(
	struct ts_controller *controller, enum ts_event event, void *data)
{
	unsigned char val;

	switch (event) {
	case TSEVENT_SUSPEND:
		val = 0x03;
		ts_reg_write(0xA5, &val, 1);
		break;
	case TSEVENT_RESUME:
		ts_gpio_set(TSGPIO_INT, 0);
		msleep(10);
		ts_gpio_set(TSGPIO_INT, 1);
		msleep(200);
		break;
	default:
		break;
	}

	return TSRESULT_EVENT_HANDLED;
}

static const unsigned short mAddrs[] = {
	0x38,
};

static const struct ts_virtualkey_info mVirtualkeys[] = {
	DECLARE_VIRTUALKEY(600, 1350, 60, 45, KEY_BACK),
	DECLARE_VIRTUALKEY(360, 1350, 60, 45, KEY_HOMEPAGE),
	DECLARE_VIRTUALKEY(120, 1350, 60, 45, KEY_APPSELECT),
};

static const struct ts_register_info mRegisters[] = {
	DECLARE_REGISTER(TSREG_CHIP_ID, 0xA3),
	DECLARE_REGISTER(TSREG_MOD_ID, 0xA8),
	DECLARE_REGISTER(TSREG_FW_VER, 0xA6),
	DECLARE_REGISTER("frequency", 0x88),
	DECLARE_REGISTER("charger_indicator", 0x8B),
};

static enum ts_result dummy_match(struct ts_controller *c){
	
	TS_INFO("dummy not match control");
	
	return TSRESULT_NOT_MATCHED;
}

static int DummyController_fetch(struct ts_controller *c, struct ts_point *points)
{
	return 0;
}

static int dummy_ps_resume(struct ts_data *pdata) {

	return 0;
}

static int dummy_ps_suspend(struct ts_data *pdata) {

	return 0;
}

static void dummy_proximity_switch(bool onoff) {


}

static void dummy_ps_irq_handler(struct ts_data *pdata) {

}

static struct ts_controller DummyController = {
	.name = "dummy_ts",
	.vendor = "sprd",
	.config = TSCONF_ADDR_WIDTH_BYTE
		| TSCONF_POWER_ON_RESET
		| TSCONF_RESET_LEVEL_LOW
		| TSCONF_REPORT_MODE_IRQ
		| TSCONF_IRQ_TRIG_EDGE_FALLING
		| TSCONF_REPORT_TYPE_3,
	.addr_count = ARRAY_SIZE(mAddrs),
	.addrs = mAddrs,
	.virtualkey_count = ARRAY_SIZE(mVirtualkeys),
	.virtualkeys = mVirtualkeys,
	.register_count = ARRAY_SIZE(mRegisters),
	.registers = mRegisters,
	.panel_width = 720,
	.panel_height = 1280,
	.reset_keep_ms = 20,
	.reset_delay_ms = 30,
	.parser = {
	},
	.custom_initialization = NULL,
	.match = dummy_match,
	.fetch_points = DummyController_fetch,
	.handle_event = handle_event,
	.upgrade_firmware = NULL,
	.upgrade_status = NULL,
	.ps_resume = dummy_ps_resume,
	.ps_suspend = dummy_ps_suspend,
	.proximity_switch = dummy_proximity_switch,
	.ps_irq_handler = dummy_ps_irq_handler,
};

int DummyController_init(void)
{
	ts_register_controller(&DummyController);
	return 0;
}

void DummyController_exit(void)
{
	ts_unregister_controller(&DummyController);
}

//REGISTER_CONTROLLER(DummyController);

MODULE_AUTHOR("joseph.cai@spreadtrum.com");
MODULE_DESCRIPTION("Spreadtrum dummy touchscreen driver");
MODULE_LICENSE("GPL");
