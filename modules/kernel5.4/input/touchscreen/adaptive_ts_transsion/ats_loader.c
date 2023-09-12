#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/types.h>
#include "ats_core.h"
#include "adaptive_ts.h"
#define TS_VIRTUALKEY_DATA_LENGTH	5
#define TS_VIRTUALKEY_MAX_COUNT		4
#define TS_RST_INDEX 0
#define TS_INT_INDEX 1
#define TS_PROP_VIRTUALKEY_SWITCH	"tp_virtualkey_switch"
#define TS_PROP_VIRTUALKEY			"tp_virtualkeys"//"virtualkeys"
#define TS_PROP_VIRTUALKEY_REPORT	"virtualkey-report-abs"
#define TS_PROP_AUTO_UPGRADE_FW	"tp_upgrade_switch"//"firmware-auto-upgrade"
#define TS_PROP_ESD_CHECK        		"tp_esd_check"//"ps-status"
#define TS_PROP_PS_STATUS        		"tp_ps_status"//"ps-status"
#define TS_PROP_SENSORHUB_STATUS      "tp_sensorhub_status"// "sensorhub-status"
#define TS_PROP_GESTURE_STATUS         	"tp_gesture_status"//"gesture-status"
#define TS_PROP_POWER				"avdd-supply"
#define TS_PROP_CONTROLLER			"controller"
#define TS_PROP_WIDTH				"tp_max_x"//"touchscreen-size-x"
#define TS_PROP_HEIGHT				"tp_max_y"//"touchscreen-size-y"
#define TS_PROP_SURFACE_WIDTH		"tp_max_x"//"surface-width"
#define TS_PROP_SURFACE_HEIGHT		"tp_max_y"//"surface-height"
#define TS_PROP_POINT_MAX			"tp_point_max"
#define TS_PROP_PRIV_NODE			"private-data"
#define TS_PROP_UPGRADE_STATUS		"upgrade-status"//"tp_upgrade_status"
#define POOL_MAX_SIZE 64

int ts_dbg_level = TS_DEFAULT_DEBUG_LEVEL;
static struct ts_board *g_board;
struct ts_board *g_board_b;
enum ts_bustype g_bus_type = TSBUS_NONE;
//#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX_SPI)\
//	||defined( CONFIG_TOUCHSCREEN_ADAPTIVE_ILI9882H_SPI)\
//	||defined( CONFIG_TOUCHSCREEN_ADAPTIVE_ICNL9911X_SPI)
struct spi_device *g_client;
//#else
//struct i2c_client *g_client;
//#endif

static LIST_HEAD(controllers);
static DEFINE_SPINLOCK(controller_lock);
static unsigned short address_pool[POOL_MAX_SIZE];
static unsigned int pool_length;
#if 0
static unsigned short lcd_width;
static unsigned short lcd_height;
static bool cali;

static int ts_parse_lcd_size(char *str)
{
	char *c, buf[32] = { 0 };
	int length;

	if (str != NULL) {
		c = strchr(str, 'x');
		if (c != NULL) {
			/* height */
			length = c - str;
			strncpy(buf, str, length);
			if (kstrtou16(buf, 10, &lcd_height))
				lcd_height = 0;
			/* width */
			length = strlen(str) - (c - str) - 1;
			strncpy(buf, c + 1, length);
			buf[length] = '\0';
			if (kstrtou16(buf, 10, &lcd_width))
				lcd_width = 0;
		} else {
			lcd_width = lcd_height = 0;
		}
	}
	return 1;
}
__setup("lcd_size=", ts_parse_lcd_size);

static int ts_parse_cali_mode(char *str)
{
	if (str != NULL && !strncmp(str, "cali", strlen("cali")))
		cali = true;
	else
		cali = false;
	return 0;
}
__setup("androidboot.mode=", ts_parse_cali_mode);
#endif

/* parse key name using key code */
struct ts_virtualkey_pair VIRTUALKEY_PAIRS[] = {
	{
		.name = "BACK",
		.code = KEY_BACK,
	},
	{
		.name = "HOME",
		.code = KEY_HOMEPAGE,
	},
	{
		.name = "APP_SWITCH",
		.code = KEY_APPSELECT,
	},
	{ },
};

/*
 * validate controller
 */
static inline int ts_validate_controller(struct ts_controller *c)
{
	/* controller name and vendor is required */
	if (!c->name || !c->vendor) {
		TS_ERR("no name or vendor!");
		return -EINVAL;
	}

	if (c->addr_count == 0 || !c->addrs) {
		TS_ERR("should provide at least one valid address");
		return -EINVAL;
	}

	/* TODO: remove this later */
	if (!c->fetch_points)
		return -EINVAL;

	return 0;
}

/*
 * register controller and add its address to address_pool for
 * adaptively matching
 */
int ts_register_controller(struct ts_controller *controller)
{
	bool found = false;
	struct ts_controller *dup;
	unsigned int i;

	if (!controller)
		return -ENODEV;

	if (ts_validate_controller(controller) < 0) {
		TS_WARN("ignore controller \"%s\" cuz validation failed!", controller->name);
		return -EINVAL;
	}

	/* prevent duplicated controller */
	spin_lock(&controller_lock);
	list_for_each_entry(dup, &controllers, list) {
		TS_INFO("dup vendor=%s, name=%s", dup->vendor, dup->name);
		if (!strcmp(dup->vendor, controller->vendor)
			&& !strcmp(dup->name, controller->name)) {
			found = true;
			break;
		}
	}
	spin_unlock(&controller_lock);

	if (found) {
		TS_WARN("ignore duplicated registration");
		return -EEXIST;
	}

	spin_lock(&controller_lock);
	/* add new controller */
	list_add_tail(&controller->list, &controllers);

	/*
	 * add address to pool
	 * TODO: increase pool size dynamically
	 */
	if (pool_length < POOL_MAX_SIZE) {
		found = false;
		for (i = 0; i < pool_length; i++) {
			/* TODO: change addrs[0] to addr list */
			if (controller->addrs[0] == address_pool[i]) {
				found = true;
				break;
			}
		}
		if (!found)
			address_pool[pool_length++] = controller->addrs[0];
	}
	spin_unlock(&controller_lock);

	TS_DBG("register controller: \"%s-%s\"",
		controller->vendor, controller->name);
	return 0;
}

/*
 * when controller unregistered, its addresses are not dropped
 */
void ts_unregister_controller(struct ts_controller *controller)
{
	struct ts_controller *c;
	bool del = false;

	if (!controller)
		return;

	spin_lock(&controller_lock);
	list_for_each_entry(c, &controllers, list) {
		if (!strcmp(c->vendor, controller->vendor)
			&& !strcmp(c->name, controller->name)) {
			list_del_init(&c->list);
			del = true;
			break;
		}
	}
	spin_unlock(&controller_lock);

	if (del)
		TS_DBG("unregister controller \"%s-%s\"",
			controller->vendor, controller->name);
	else
		TS_WARN("controller \"%s-%s\" not found.",
			controller->vendor, controller->name);
}

/*
 * matches one controller by name or adaptively
 */
struct ts_controller *ts_match_controller(const char *name)
{
	struct ts_controller *c, *target = NULL;
	char *ch, *t, buf[32];
	const char *s;
	int length = 0, i;

	if (unlikely(!g_board))
		return NULL;

	if (name) {
		/* with designated name, just find what they want */
		TS_DBG(" :%s",name);
		spin_lock(&controller_lock);
		list_for_each_entry(c, &controllers, list) {
			length = sprintf(buf, "%s,%s", c->vendor, c->name);
			if (!strncmp(name, buf, length)) {
				target = c;
				break;
			}
		}
		spin_unlock(&controller_lock);

		if (!target) {
			ch = strchr(name, ',');
			if (ch) {
				s = name;
				t = buf;
				while (s < ch)
					*t++ = *s++;
				*t = '\0';
				TS_DBG("fallback to match vendor \"%s\"", buf);
				spin_lock(&controller_lock);
				list_for_each_entry(c, &controllers, list) {
					if (!strncmp(buf, c->vendor, ch - name)) {
						target = c;
						break;
					}
				}
				spin_unlock(&controller_lock);
			}
		}
	} else {
		/* without designated name. let's see who will be the volunteer */
		TS_INFO("[TS] not match name\n");
		spin_lock(&controller_lock);
		list_for_each_entry(c, &controllers, list) {
			if (c->match) {
				for (i = 0; i < c->addr_count; i++) {
					if (//g_board->bus->client_addr == c->addrs[i]
						//&&
						 c->match(c) == TSRESULT_FULLY_MATCHED) {
						target = c;
						break;
					}
				}
			}
			if (target)
				break;
		}
		spin_unlock(&controller_lock);
	}

	return target;
}

int ts_read(unsigned char *data, unsigned short length)
{
	if (IS_ERR_OR_NULL(g_board) || IS_ERR_OR_NULL(g_board->bus)) {
		TS_ERR("Touchscreen not ready!");
		return -ENODEV;
	}

	return g_board->bus->simple_read(data, length);
}

int ts_write(unsigned char *data, unsigned short length)
{
	if (IS_ERR_OR_NULL(g_board) || IS_ERR_OR_NULL(g_board->bus)) {
		TS_ERR("Touchscreen not ready!");
		return -ENODEV;
	}

	return g_board->bus->simple_write(data, length);
}

int ts_reg_read(unsigned short reg, unsigned char *data, unsigned short length)
{
	if (IS_ERR_OR_NULL(g_board) || IS_ERR_OR_NULL(g_board->bus)) {
		TS_ERR("Touchscreen not ready!");
		return -ENODEV;
	}

	return g_board->bus->read(reg, data, length);
}

int ts_reg_write(unsigned short reg, unsigned char *data, unsigned short length)
{
	if (IS_ERR_OR_NULL(g_board) || IS_ERR_OR_NULL(g_board->bus)) {
		TS_ERR("Touchscreen not ready!");
		return -ENODEV;
	}

	return g_board->bus->write(reg, data, length);
}

int ts_reg_read_fw(unsigned short reg, unsigned char *data, unsigned short length)
{
	if (IS_ERR_OR_NULL(g_board) || IS_ERR_OR_NULL(g_board->bus)) {
		TS_ERR("Touchscreen not ready!");
		return -ENODEV;
	}
	return g_board->bus->read_fw(reg, data, length);
}

int ts_reg_write_fw(unsigned short reg, unsigned char *data, unsigned short length)
{
	if (IS_ERR_OR_NULL(g_board) || IS_ERR_OR_NULL(g_board->bus)) {
		TS_ERR("Touchscreen not ready!");
		return -ENODEV;
	}
	return g_board->bus->write_fw(reg, data, length);
}

/* GPIO operation */
int ts_gpio_get(enum ts_gpio type)
{
	int val = 0;
	struct ts_board *board = g_board;

	if (IS_ERR_OR_NULL(board))
		return -ENODEV;

	if (TSGPIO_INT == type && board->int_gpio > 0) {
		val = gpio_get_value(board->int_gpio);
		if (val < 0)
			TS_ERR("Failed to get INT gpio(%d), err: %d.", board->int_gpio, val);
	} else if (TSGPIO_RST == type && board->rst_gpio > 0) {
		val = gpio_get_value(board->rst_gpio);
		if (val < 0)
			TS_ERR("Failed to set RST gpio(%d), err: %d.", board->rst_gpio, val);
	} else {
		TS_WARN("Unrecognized gpio type, ignore.");
	}

	return val;
}
void ts_gpio_set(enum ts_gpio type, int level)
{
	struct ts_board *board = g_board;

	if (IS_ERR_OR_NULL(board))
		return;

	if (TSGPIO_INT == type && board->int_gpio) {
		gpio_set_value(board->int_gpio, level);
		TS_DBG("set gpio INT (%d) to %d", board->int_gpio, level);
	} else if (TSGPIO_RST == type && board->rst_gpio) {
		gpio_set_value(board->rst_gpio, level);
		TS_DBG("set gpio RST (%d) to %d", board->rst_gpio, level);
	} else {
		TS_WARN("Unrecognized gpio type, ignore.");
	}
}
int ts_gpio_input(enum ts_gpio type)
{
	int retval = 0;
	struct ts_board *board = g_board;

	if (IS_ERR_OR_NULL(board))
		return -ENODEV;

	if (TSGPIO_INT == type && board->int_gpio) {
		retval = gpio_direction_input(board->int_gpio);
		if (retval < 0)
			TS_ERR("Failed to set gpio INT (%d) in, err: %d.", board->int_gpio, retval);
		else
			TS_DBG("set gpio INT (%d) to input", board->int_gpio);
	} else if (TSGPIO_RST == type && board->rst_gpio) {
		retval = gpio_direction_input(board->rst_gpio);
		if (retval < 0)
			TS_ERR("Failed to set gpio RST (%d) in, err: %d.", board->rst_gpio, retval);
		else
			TS_DBG("set gpio RST (%d) to input", board->rst_gpio);
	} else {
		TS_WARN("Unrecognized gpio type, ignore.");
	}

	return retval;
}

int ts_gpio_output(enum ts_gpio type, int level)
{
	int retval = 0;
	struct ts_board *board = g_board;

	if (IS_ERR_OR_NULL(board))
		return -ENODEV;

//#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX)||defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX_SPI)
	if(nvt_get_device_name()&&(TSGPIO_RST == type))//binhua0218
		return 0;
//#endif

	if (TSGPIO_INT == type && board->int_gpio) {
		retval = gpio_direction_output(board->int_gpio, level);
		if (retval < 0)
			TS_ERR("Failed to set gpio INT (%d) out to %d, err: %d.",
				board->int_gpio, level, retval);
		else
			TS_DBG("set gpio INT (%d) to output, level=%d", board->int_gpio, level);
	} else if (TSGPIO_RST == type && board->rst_gpio) {
		retval = gpio_direction_output(board->rst_gpio, level);
		if (retval < 0)
			TS_ERR("Failed to set gpio RST (%d) out to %d, err: %d.",
				board->rst_gpio, level, retval);
		else
			TS_DBG("set gpio RST (%d) to output, level=%d", board->rst_gpio, level);
	} else {
		TS_WARN("Unrecognized gpio type, ignore.");
	}

	return retval;
}

/*
 * parse .dts file to get following info:
 *  - GPIO number for int, rst PIN
 *  - touch panel size
 *  - surface screen size
 *  - if report virtual key as KEY events
 *  - if auto upgrade firmware when boot up
 *  - virtual keys info
 *  - controller used
 *  - regulator name if used
 */
static struct ts_board *ts_parse_dt(struct device_node *pn)
{
	int retval, i, j, irq, rst;
	struct ts_board *board;
	u32 buf[TS_VIRTUALKEY_MAX_COUNT * TS_VIRTUALKEY_DATA_LENGTH];
	size_t size = sizeof(struct ts_board);
//	int count;
/*
	rst = of_get_gpio(pn, TS_RST_INDEX);
	if (rst < 0) {
		TS_ERR("invalid reset gpio number: %d", rst);
		return NULL;
	}
	irq = of_get_gpio(pn, TS_INT_INDEX);
	if (irq < 0) {
		TS_ERR("invalid irq gpio number: %d", irq);
		return NULL;
	}
*/

/*
	 irq = of_get_named_gpio_flags(pn, "touch,irq-gpio", 0, &board->irq_gpio_flags);
	if (irq < 0) {
                TS_ERR("invalid irq gpio number: %d", irq);
                return NULL;
        }

	 rst = of_get_named_gpio_flags(pn, "touch,reset-gpio", 0, &board->rst_gpio_flags);
	 if (rst < 0) {
                TS_ERR("invalid reset gpio number: %d", rst);
                return NULL;
        }
*/
	board = kzalloc(size, GFP_KERNEL);
	if (!board) {
		TS_ERR("failed to allocate board info!!");
		return NULL;
	}

	retval = of_property_read_u32(pn, TS_PROP_VIRTUALKEY_SWITCH/*TS_PROP_VIRTUALKEY_REPORT*/, &board->virtualkey_switch);
        if (retval < 0)
       		TS_ERR("not find %s", TS_PROP_VIRTUALKEY_SWITCH);

	//TS_INFO("board->virtualkey_switch=%d", board->virtualkey_switch);
	if(board->virtualkey_switch==1){
		i = of_property_count_u32_elems(pn, TS_PROP_VIRTUALKEY);
		if (i > 0) {
			if (i > TS_VIRTUALKEY_DATA_LENGTH * TS_VIRTUALKEY_MAX_COUNT
				|| i % TS_VIRTUALKEY_DATA_LENGTH) {
				i = 0;
				TS_ERR("invalid virtualkey data count: %d", i);
			} else {
				retval = of_property_read_u32_array(pn, TS_PROP_VIRTUALKEY, buf, i);
				if (!retval) {
					i /= TS_VIRTUALKEY_DATA_LENGTH;
					size += sizeof(struct ts_virtualkey_info) * i;
				} else {
					i = 0;
					TS_ERR("failed to read virtualkey data, error: %d", retval);
				}
			}
		} else {
			i = 0;
		}
/*	}else{
		i = 0;
	}*/

		board->virtualkey_count = i;
		for (j = 0; j < i; j++) {
			board->virtualkeys[j].keycode = buf[TS_VIRTUALKEY_DATA_LENGTH * j];
			board->virtualkeys[j].x = buf[TS_VIRTUALKEY_DATA_LENGTH * j + 1] & 0xFFFF;
			board->virtualkeys[j].y = buf[TS_VIRTUALKEY_DATA_LENGTH * j + 2] & 0xFFFF;
			board->virtualkeys[j].width = buf[TS_VIRTUALKEY_DATA_LENGTH * j + 3] & 0xFFFF;
			board->virtualkeys[j].height = buf[TS_VIRTUALKEY_DATA_LENGTH * j + 4] & 0xFFFF;
		}

		if (board->virtualkey_count) {
			TS_DBG("board config: read %d virtualkeys", board->virtualkey_count);
			for (i = 0; i < board->virtualkey_count; i++) {
				TS_DBG("board config: x=%u, y=%u, w=%u, h=%u ------ %s",
					board->virtualkeys[i].x, board->virtualkeys[i].y,
					board->virtualkeys[i].width, board->virtualkeys[i].height,
					ts_get_keyname(board->virtualkeys[i].keycode));
			}
		}
	}

	irq = of_get_named_gpio_flags(pn, "touch,irq-gpio", 0, &board->irq_gpio_flags);
        if (irq < 0)
                TS_ERR("invalid irq gpio number: %d", irq);
	rst = of_get_named_gpio_flags(pn, "touch,reset-gpio", 0, &board->rst_gpio_flags);
	if (rst < 0)
	    TS_ERR("invalid reset gpio number: %d", rst);

	board->int_gpio = irq;
	board->rst_gpio = rst;
	TS_INFO("board config: rst_gpio=%d, int_gpio=%d", board->rst_gpio, board->int_gpio);

//#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX_SPI)
	retval = of_property_read_u32(pn, "novatek,swrst-n8-addr", &board->swrst_n8_addr);
	if (retval) {
		TS_ERR("error reading novatek,swrst-n8-addr. ret=%d", retval);
	} else {
		TS_INFO("SWRST_N8_ADDR=0x%06X", board->swrst_n8_addr);
	}

	retval = of_property_read_u32(pn, "novatek,spi-rd-fast-addr", &board->spi_rd_fast_addr);
	if (retval) {
		TS_ERR("not support novatek,spi-rd-fast-addr");
		board->spi_rd_fast_addr = 0;
	} else {
		TS_INFO("SPI_RD_FAST_ADDR=0x%06X", board->spi_rd_fast_addr);
	}
//#endif

	retval = of_property_read_u32(pn, TS_PROP_ESD_CHECK, &board->esd_check);
        if (retval < 0)
		TS_ERR("not find esd_check");
	retval = of_property_read_u32(pn, TS_PROP_PS_STATUS, &board->ps_status);
        if (retval < 0)
       		TS_ERR("not find ps_status");
	retval = of_property_read_u32(pn, TS_PROP_GESTURE_STATUS, &board->gesture_status);
        if (retval < 0)
		TS_ERR("not find gesture_status");
	retval = of_property_read_u32(pn, TS_PROP_SENSORHUB_STATUS, &board->sensorhub_status);
	if (retval < 0)
               TS_ERR("not find sensorhub_status");

	retval = of_property_read_u32(pn, TS_PROP_WIDTH, &board->panel_width);
	if (retval < 0)
		board->panel_width = 0;
	retval = of_property_read_u32(pn, TS_PROP_HEIGHT, &board->panel_height);
	if (retval < 0)
		board->panel_height = 0;
	TS_INFO("board config: report_region=%ux%u", board->panel_width, board->panel_height);

	retval = of_property_read_u32(pn, TS_PROP_SURFACE_WIDTH, &board->surface_width);
	if (retval < 0)
		board->surface_width = 0;
	retval = of_property_read_u32(pn, TS_PROP_SURFACE_HEIGHT, &board->surface_height);
	if (retval < 0)
		board->surface_height = 0;

	board->lcd_width = board->panel_width;
	board->lcd_height = board->panel_height;

	retval = of_property_read_u32(pn, TS_PROP_POINT_MAX, &board->max_touch_num);
	if (retval < 0)
               TS_ERR("not find tp_point_max");

	//board->vkey_report_abs = board->virtualkey_switch;//!!of_get_property(pn, TS_PROP_VIRTUALKEY_REPORT, NULL);
	//TS_INFO("board config: report virtual key as %s event", board->vkey_report_abs ? "ABS" : "KEY");

	//board->auto_upgrade_fw = !!of_get_property(pn, TS_PROP_AUTO_UPGRADE_FW, NULL);
	retval = of_property_read_u32(pn, TS_PROP_AUTO_UPGRADE_FW, &board->auto_upgrade_fw);
        if (retval < 0)
       		TS_ERR("not find %s", TS_PROP_AUTO_UPGRADE_FW);
	//TS_INFO("board config: %d trying to auto upgrade firmware", board->auto_upgrade_fw);
/*
	count = of_property_read_string_array(pn, TS_PROP_CONTROLLER, NULL, 0);
	if (count < 0)
		TS_ERR("find controller property failed %d", count);
	//TS_INFO("controller: count: %d", count);

	board->controller = kcalloc(count, sizeof(*board->controller), GFP_KERNEL);
	if (!board->controller) {
		TS_ERR("kcalloc controller failed !");
		return NULL;
	}

	retval = of_property_read_string_array(pn, TS_PROP_CONTROLLER, board->controller, count);
	if (retval < 0)
		TS_ERR("find controller property failed !!!%d", retval);

	for(i=0;i<count;i++)
		TS_INFO("controller name[%d]=%s", i, board->controller[i]);
*/
/*	retval = of_property_read_string(pn, TS_PROP_CONTROLLER, (const char**) &board->controller);
	if (retval < 0)
		TS_ERR("find controller property failed !!!%d", retval);

	TS_INFO("controller name=%s",  board->controller);
*/
	retval = of_property_read_string(pn, TS_PROP_POWER, &board->avdd_supply);
	if (retval < 0)
		board->avdd_supply = NULL;
	if (board->avdd_supply)
		TS_INFO("board config: requesting avdd-supply=\"%s\"", board->avdd_supply);

	/* add private data */
	board->priv = of_get_child_by_name(pn, TS_PROP_PRIV_NODE);

//	retval = of_property_read_u32(pn, TS_PROP_UPGRADE_STATUS, &board->upgrade_status);
//	if (retval < 0)
//    		TS_ERR("not find upgrade_status");

	return board;
}

void ts_controller_setname(const char* vendor, const char* name){

	const char comma[]=",";
	char tempbuf[128] = {0};

	strcat(&tempbuf[0], vendor);
	strcat(tempbuf, comma);
	strcat(tempbuf, name);
	if(strlen(tempbuf)>1){
		g_board->controller = kmalloc(strlen(tempbuf)+1, GFP_KERNEL);
		if (g_board->controller == NULL) {
			TS_ERR("Failed to allocate memory for g_board->controller\n");
			return;
		}
		memset(g_board->controller, 0x00, strlen(tempbuf)+1);
		memcpy(g_board->controller, tempbuf, strlen(tempbuf));
		g_board->controller[strlen(tempbuf)] = '\0';
	}
	TS_DBG("controller=%s", g_board->controller);
}

static void ts_release_platform_dev(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	kfree(pdev);
}

/* register low layer bus access */
int ts_register_bus_dev(struct device *parent)
{
	struct platform_device *pdev;
	struct ts_board *board = g_board;
	struct ts_bus_access *bus;
	int retval;

	if (unlikely(IS_ERR_OR_NULL(board)))
		return -ENODEV;
	bus = dev_get_drvdata(parent);
	if (IS_ERR_OR_NULL(bus) || bus->bus_type == TSBUS_NONE
		|| !bus->read || !bus->write
		|| !bus->simple_read || !bus->simple_write
		|| !bus->simple_read_reg || !bus->simple_write_reg) {
		TS_ERR("incomplete bus interface!");
		return -ENXIO;
	}
	board->bus = bus;

	pdev = kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	if (IS_ERR_OR_NULL(pdev)) {
		TS_ERR("failed to allocate platform device!");
		return -ENOMEM;
	}

	pdev->name = ATS_PLATFORM_DEV;
	pdev->id = 0;
	pdev->num_resources = 0;
	pdev->dev.parent = parent;
	pdev->dev.platform_data = board;
	pdev->dev.release = ts_release_platform_dev;

	retval = platform_device_register(pdev);
	TS_ERR("ts_register_bus_dev retval=%x",retval);
	if (retval < 0) {
		TS_ERR("failed to register platform device!");
		kfree(pdev);
		return retval;
	}

	board->pdev = pdev;
	TS_DBG("succeed to register platform device.");
	return 0;
}

void ts_unregister_bus_dev(void)
{
	struct ts_board *board = g_board;

	if (!IS_ERR_OR_NULL(board) && !IS_ERR_OR_NULL(board->pdev))
		platform_device_unregister(board->pdev);
	board->pdev = NULL;
}

/* initialize bus device according to node type */
static enum ts_bustype ts_bus_init(struct device_node *bus_node, bool adaptive)
{
	if (IS_ERR_OR_NULL(bus_node)) {
		TS_ERR("cannot decide bus type because of_node is null!");
		return TSBUS_NONE;
	}

	if (!strncmp(bus_node->name, "i2c", 3)) {
	//	if (adaptive) {
	//		address_pool[pool_length] = I2C_CLIENT_END;
	//		return ts_i2c_init(bus_node, address_pool)	?
	//			TSBUS_NONE : TSBUS_I2C;
	//	}
		TS_ERR("ts_bus_init cannot support i2c!");
	//	if (!ts_i2c_init(bus_node, NULL)){
	//	#if !defined( CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX_SPI) \
	//		&&!defined( CONFIG_TOUCHSCREEN_ADAPTIVE_ILI9882H_SPI) \
	//		&&!defined( CONFIG_TOUCHSCREEN_ADAPTIVE_ICNL9911X_SPI)
	//		g_client = g_i2c_client;
	//	#endif
	//		return TSBUS_I2C;
	//	}
	} else if (!strncmp(bus_node->name, "spi", 3)) {
		/* TODO add spi support */
		if(!ts_spi_init()){
		//#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX_SPI)\
		//	||defined( CONFIG_TOUCHSCREEN_ADAPTIVE_ILI9882H_SPI)\
		//	||defined( CONFIG_TOUCHSCREEN_ADAPTIVE_ICNL9911X_SPI)

			g_client = g_spi_client;
		//#endif
			return TSBUS_SPI;
		}
	} else {
		TS_WARN("unknown bus type: \"%s\"", bus_node->name);
	}

	return TSBUS_NONE;
}

#ifndef CONFIG_TRANSSION_CALI_BACKLIGHT_ENABLE
#ifdef CONFIG_TRANSSION_SUPPORT
extern bool transsion_cali_mode;
#endif
#endif

/*
 * init board related configurations, providing bus access and configs
 * for touchscreen core module
 */
int ts_board_init(void)
{
	struct device_node *pn;
	struct ts_board *board = NULL;
	//enum ts_bustype bus_type = TSBUS_NONE;

	pn = of_find_compatible_node(NULL, NULL, ATS_COMPATIBLE);
	if (IS_ERR_OR_NULL(pn)) {
		TS_ERR("cannot find compatible node \"%s\"", ATS_COMPATIBLE);
		return -ENODEV;
	}
	board = ts_parse_dt(pn);
	if (IS_ERR_OR_NULL(board)) {
		TS_ERR("parsing board info failed!");
		return -ENODEV;
	}
#ifndef CONFIG_TRANSSION_CALI_BACKLIGHT_ENABLE
#ifdef CONFIG_TRANSSION_SUPPORT
		board->suspend_on_init = transsion_cali_mode;
#else
		board->suspend_on_init = false;//binhua cali;
#endif
#endif

	g_board = board;
	g_board_b = board;

	g_bus_type = ts_bus_init(pn->parent, board->controller == NULL);
	if (g_bus_type == TSBUS_NONE) {
		TS_ERR("bus init failed!");
		kfree(board);
		g_board = NULL;
		return -ENODEV;
	}
	TS_ERR("board init OK, bus type is %d.", g_bus_type);
	return 0;
}

void ts_board_exit(void)
{
	struct ts_board *board = g_board;

	if (IS_ERR_OR_NULL(board))
		return;

	switch (board->bus->bus_type) {
	case TSBUS_I2C:
		ts_i2c_exit();
		break;
	case TSBUS_SPI:
		ts_spi_exit();
		break;
	default:
		break;
	}

	kfree(board);
	g_board = NULL;
}

MODULE_AUTHOR("joseph.cai@spreadtrum.com");
MODULE_DESCRIPTION("Spreadtrum touchscreen controller loader");
MODULE_LICENSE("GPL");
