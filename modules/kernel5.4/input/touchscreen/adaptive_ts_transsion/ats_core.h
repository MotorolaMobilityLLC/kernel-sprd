#ifndef __ATS_CORE_H__
#define __ATS_CORE_H__

#include <linux/input.h>
#include "adaptive_ts.h"
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <uapi/linux/time.h>
#ifdef CONFIG_TRANSSION_HWINFO
#include <transsion/hwinfo.h>
#endif
extern struct mutex i2c_mutex;
extern struct ts_data *g_pdata;
extern struct ts_board *g_board_b;
extern struct wakeup_source *ts_ps_wakelock;
extern enum ts_bustype g_bus_type;
extern struct spi_device *g_spi_client;
extern struct i2c_client *g_i2c_client;
//#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX_SPI)
extern struct spi_device *g_client;
//#else
//extern struct i2c_client *g_client;
//#endif

/* ================================================== *
 *                       MACROs                       *
 * ================================================== */

/* names to recognize adaptive-ts in system */
#define ATS_COMPATIBLE    "adaptive-touchscreen-transsion"//"adaptive-touchscreen"
#define ATS_PLATFORM_DEV  "adaptive_ts_drv_transsion"
#define ATS_I2C_DEV       "adaptive_ts_i2c_drv"
#define ATS_SPI_DEV       "adaptive_ts_spi_drv"//"adaptive_ts_i2c_drv"
#define ATS_INPUT_DEV     "adaptive_ts"
#define ATS_INT_LABEL     "adaptive_ts_int"
#define ATS_RST_LABEL     "adaptive_ts_rst"
#define ATS_WORKQUEUE     "adaptive_ts_workqueue"
#define ATS_WL_UPGRADE_FW "adaptive_ts_fw_upgrade"
#define ATS_IRQ_HANDLER   "adaptive_ts-irq"
#define ATS_NOTIFIER_WORKQUEUE     "adaptive_ts_notifier_workqueue"

/* default point buffer size - support max 10 points */
/* default polling frequency 100Hz */
#define TS_POLL_INTERVAL 10000
/* default debug level */
#define TS_DEFAULT_DEBUG_LEVEL 0
/* use adf notifier to control suspend/resume */
#ifdef CONFIG_ADF_SPRD
#define TS_USE_ADF_NOTIFIER
#endif
/* use legacy linux suspend/resume */
#define TS_USE_LEGACY_SUSPEND 0

/* key map version for Android */
#define ANDROID_KEYMAP_VERSION 0x01

/* haibo.li add comment for these BITS
   TSMODE_IRQ_MODE: 中断模式，并且已经注册了中断处理函数
   TSMODE_IRQ_STATUS: 中断被使能。用来防止中断二次禁止或者二次使能，目前看它无法支撑NVT的需求
 * */
#define TSMODE_CONTROLLER_EXIST     	0       /* bit0: controller existence status */
#define TSMODE_POLLING_MODE         	1       /* bit1: polling mode status */
#define TSMODE_POLLING_STATUS       	2       /* bit2: polling enable status */
#define TSMODE_IRQ_MODE             		3       /* bit3: irq mode status */
#define TSMODE_IRQ_STATUS           		4       /* bit4: irq enable status */
#define TSMODE_SUSPEND_STATUS       	5       /* bit5: suspend status */
#define TSMODE_CONTROLLER_STATUS    	6       /* bit6: whether controller is available */
#define TSMODE_VKEY_REPORT_ABS      	7       /* bit7: report virtualkey as ABS or KEY event */
#define TSMODE_WORKQUEUE_STATUS     	8       /* bit8: workqueue status */
#define TSMODE_AUTO_UPGRADE_FW      	9       /* bit9: auto upgrade firmware when boot up */
#define TSMODE_NOISE_STATUS         		10      /* bit10: noise status */
#define TSMODE_DEBUG_UPDOWN         	16      /* bit16: print point up & down info */
#define TSMODE_DEBUG_RAW_DATA       	17      /* bit17: print raw data */
#define TSMODE_DEBUG_IRQTIME        	18      /* bit18: print irq time info */
#define TSMODE_PS_STATUS        		19 
#define TSMODE_GESTURE_STATUS        	20 
#define TSMODE_SENSORHUB_STATUS	21
#define TSMODE_UPGRADE_STATUS        	22

//#define TS_FORCE_UPDATE_FIRMWARE//binhua
#define TS_ESD_SUPPORT_EN
int ts_prox_ctrl(int enable);
#define MULTI_PROTOCOL_TYPE_B 1
#define TS36XXX_UPGRADE_FW0_FILE "nt36xxx_fw_sample.i"
#define TS36XXX_UPGRADE_FW1_FILE "nt36xxx_fw1_sample.i"

/* ================================================== *
 *                      structs                       *
 * ================================================== */

/*
 * defines supported low layer bus type
 */
enum ts_bustype {
	TSBUS_NONE = 0,
	TSBUS_I2C,
	TSBUS_SPI,
};

/*
 * struct ts_bus_access
 *
 * Provide a set of methods to access the bus.
 *
 * bus_type         : bus type
 * client_addr      : controller slave address
 * reg_width        : width of controller's registers
 * simple_read      : read data from bus, length limited
 * simple_write     : write data to bus, length limited
 * read             : read data from some register address(full version)
 * write            : write data to some register address(full version)
 * simple_read_reg  : read data from some register, length limited
 * simple_write_reg : write data to some register, length limited
 *
 */
struct ts_bus_access {
	enum ts_bustype bus_type;
	unsigned short client_addr;
	unsigned char reg_width;
	int (*simple_read)(unsigned char *, unsigned short);
	int (*simple_write)(unsigned char *, unsigned short);
	int (*read)(unsigned short, unsigned char *, unsigned short);
	int (*write)(unsigned short, unsigned char *, unsigned short);
	int (*read_fw)(unsigned short, unsigned char *, unsigned short);
	int (*write_fw)(unsigned short, unsigned char *, unsigned short);
	int (*simple_read_reg)(unsigned short, unsigned char *, unsigned short);
	int (*simple_write_reg)(unsigned short, unsigned char *, unsigned short);
};

/*
 * struct ts_board
 *
 * contains board info in .dts file
 *
 * bus              : bus access info
 * pdev             : platform device
 * priv             : private data node
 * int_gpio         : interrupt GPIO number
 * rst_gpio         : reset GPIO number
 * panel_width      : touch panel width
 * panel_height     : touch panel height
 * surface_width    : surface width of screen
 * surface_height   : surface height of screen
 * lcd_width        : lcd panel width
 * lcd_height       : lcd panel height
 * avdd_supply      : some controller may require special power supply
 * controller       : controller name
 * vkey_report_abs  : whether we treat virtual keys as abs events
 * auto_upgrade_fw  : whether upgrade firmware when boot up
 * suspend_on_init  : suspend touch panel after init complete
 * virtualkey_count : number of virtual key defined
 * virtualkeys      : virtual keys info
 *
 */
struct ts_board {
	struct ts_bus_access *bus;
	struct platform_device *pdev;
	struct device_node *priv;
	int int_gpio;
	int rst_gpio;
	u32 irq_gpio_flags;
	u32 rst_gpio_flags;
	unsigned int panel_width;
	unsigned int panel_height;
	unsigned int surface_width;
	unsigned int surface_height;
	unsigned int lcd_width;
	unsigned int lcd_height;
	const char *avdd_supply;
	char *controller;
	bool vkey_report_abs;
	unsigned int auto_upgrade_fw;
	bool suspend_on_init;
	int virtualkey_count;

	unsigned int esd_check;
	unsigned int virtualkey_switch;
	unsigned int ps_status;
	unsigned int sensorhub_status;
	unsigned int max_touch_num;
	unsigned int gesture_status;
	unsigned int upgrade_status;
//#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX_SPI)
	unsigned int  swrst_n8_addr;
	unsigned int  spi_rd_fast_addr;
//#endif
	struct ts_virtualkey_info virtualkeys[8];
};

/*
 * struct ts_virtualkey_hitbox
 */
struct ts_virtualkey_hitbox {
	unsigned short top;
	unsigned short bottom;
	unsigned short left;
	unsigned short right;
};

/*
 * struct ts_virtualkey_pair
 *
 * stores virtual key name and keycode
 */
struct ts_virtualkey_pair {
	const char *name;
	const unsigned int code;
};
/*
enum ts_stashed_status {
	TSSTASH_INVALID = 0,
	TSSTASH_NEW,
	TSSTASH_CONSUMED,
};
*/
struct ts_firmware {

	char fw_version[8];
	unsigned int vendor_num;
	unsigned int is_upgrade_switch;
	const char * vendor_name;
	u8 *vendor_id;
	u8 *upgrade_fw;
	unsigned int upgrade_fw_size;

};

/* ================================================== *
 *                    functions                       *
 * ================================================== */

/*
 * initialize I2C device
 */
int ts_i2c_init(struct device_node *, unsigned short *);
void ts_i2c_exit(void);

int ts_spi_init(void);
void ts_spi_exit(void);
/*
 * initialize board settings
 */
int ts_board_init(void);
void ts_board_exit(void);

/*
 * register bus device to board settings
 */
int ts_register_bus_dev(struct device *);
void ts_unregister_bus_dev(void);

/*
 * handler from core module to handle our inner events
 */
typedef void (*event_handler_t)(struct ts_data *, enum ts_event, void *);
int ts_register_ext_event_handler(struct ts_data *, event_handler_t);
void ts_unregister_ext_event_handler(void);

/*
 * match controller from name, if no name provided, choose a controller
 */
struct ts_controller *ts_match_controller(const char *);

/*
 * transform keycode to key name
 */
static inline const char *ts_get_keyname(const unsigned int keycode)
{
	extern struct ts_virtualkey_pair VIRTUALKEY_PAIRS[];
	struct ts_virtualkey_pair *pair = VIRTUALKEY_PAIRS;

	while (pair->code && pair->code != keycode)
		pair++;

	return pair->name;
}

extern int ts_reset_controller_ex(struct ts_data *pdata, bool hw_reset);
//extern void ts_enable_irq_ex(struct ts_data *pdata, bool enable);
extern void ts_clear_points_ext(struct ts_data *pdata);
//extern struct ts_data * ts_get_ts_data(void);
extern int ps_en_flag;
extern int ts_get_mode_ext(struct ts_data *pdata, unsigned int mode);

//#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX)||defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX_SPI)
extern int nvt_get_device_name(void);
extern void nvt_customizeCmd_ext(uint8_t u8Cmd);
//#endif

#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_ILI9882H)
extern int ili_ic_func_ctrl(const char *name, int ctrl);
extern int ili_ic_proximity_resume(void);
#endif

extern int tlsc6x_esd_condition(void);
extern int tlsc6x_esd_check_work(void);
extern void ts_controller_setname(const char* vendor,const char* name);

extern int ts_suspend_late(struct device *dev);
extern int ts_resume_early(struct device *dev);

#endif
