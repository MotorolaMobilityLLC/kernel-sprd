/*
 * FPC Fingerprint sensor device driver
 *
 * This driver will control the platform resources that the FPC fingerprint
 * sensor needs to operate. The major things are probing the sensor to check
 * that it is actually connected and let the Kernel know this and with that also
 * enabling and disabling of regulators, enabling and disabling of platform
 * clocks.
 * *
 * The driver will expose most of its available functionality in sysfs which
 * enables dynamic control of these features from eg. a user space process.
 *
 * The sensor's IRQ events will be pushed to Kernel's event handling system and
 * are exposed in the drivers event node. This makes it possible for a user
 * space process to poll the input node and receive IRQ events easily. Usually
 * this node is available under /dev/input/eventX where 'X' is a number given by
 * the event system. A user space process will need to traverse all the event
 * nodes and ask for its parent's name (through EVIOCGNAME) which should match
 * the value in device tree named input-device-name.
 *
 *
 * Copyright (c) 2018 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#define DEBUG
#define pr_fmt(fmt) "fingerprint %s +%d, " fmt, __func__,__LINE__

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/pm_wakeup.h>
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/clk.h>


//#define USE_REGULATOR
/*
#ifndef CONFIG_FB
#define CONFIG_FB
#endif
*/
#ifdef PIN_CONTROL
static const char * const pctl_names[] = {
    "fpc1020_reset_reset",
	"fpc1020_reset_active",
	"fpc1020_irq_active",
	"fpc1020_power_active",
	"fpc1020_power_deactive",
};
#endif

#define HWID_SIZE		40
#define FPC_RESET_LOW_US 5000
#define FPC_RESET_HIGH1_US 100
#define FPC_RESET_HIGH2_US 5000
#define FPC_TTW_HOLD_TIME 1000
#define FPC_POWEROFF_SLEEP_US 1000

#define GET_STR_BYSTATUS(status) (status==0? "Success":"Failure")
#define ERROR_LOG       (0)
#define INFO_LOG        (1)
#define DEBUG_LOG       (2)

/* debug log setting */
static u8 debug_level = DEBUG_LOG;
#define fpsensor_log(level, fmt, args...) do { \
    if (debug_level >= level) {\
        if( level == DEBUG_LOG) { \
            printk("fingerprint %s +%d, " fmt, __func__, __LINE__, ##args); \
        } else { \
            printk("fingerprint: " fmt, ##args); \
        }\
    } \
} while (0)
#define func_entry()    do{fpsensor_log(DEBUG_LOG, "%s, %d, entry\n", __func__, __LINE__);}while(0)
#define func_exit()     do{fpsensor_log(DEBUG_LOG, "%s, %d, exit\n", __func__, __LINE__);}while(0)
#ifdef CONFIG_FB
#define DRM_MODE_DPMS_ON        0
#define DRM_MODE_DPMS_STANDBY   1
#define DRM_MODE_DPMS_SUSPEND   2
#define DRM_MODE_DPMS_OFF       3
//extern int dsp_register_client(struct notifier_block *nb);
//extern int dsp_unregister_client(struct notifier_block *nb);
#endif
#ifdef USE_REGULATOR
struct vreg_config {
	char *name;
	unsigned long vmin;
	unsigned long vmax;
	int ua_load;
};

static const struct vreg_config vreg_conf[] = {
	{ "vdd_io", 3300000UL, 3300000UL, 6000, },
};
#endif

struct fpc_data {
    struct device *dev;
    struct platform_device *pdev;
#ifdef USE_REGULATOR
	struct regulator *vreg[ARRAY_SIZE(vreg_conf)];
#endif
#ifdef PIN_CONTROL
    struct pinctrl *fpc_pinctrl;
	struct pinctrl_state *pinctrl_state[ARRAY_SIZE(pctl_names)];
#endif
    int prop_wakeup;
    int irq_gpio;
    int irq_num;
    int rst_gpio;
    int power_ctl_gpio;
    bool wakeup_enabled;
	char hwid[HWID_SIZE];
    struct wakeup_source ttw_wl;
    bool clocks_enabled;
#ifdef CONFIG_FB
	struct notifier_block fb_notif;
	int screen_status;
#endif
};

static DEFINE_MUTEX(fpc_set_gpio_mutex);

static int fpc_dts_pin_init( struct fpc_data * fpc);

static int fpc_dts_irq_init(struct fpc_data *fpc);
static irqreturn_t fpc_irq_handler(int irq, void *handle);


#ifdef CONFIG_FB
static int fb_notifier_callback(struct notifier_block *self,
unsigned long event, void *data)
{
	int *p = data;
	unsigned int blank = 0;
	struct fpc_data *fpc = NULL;
	func_entry();

	if(p != NULL)
		blank = *p;

	if (event != 0x10 && event != 0x11)
	{
		fpsensor_log(ERROR_LOG, "%s worng event, return 0!\n", __func__);
		return 0;
	}

	fpc = container_of(self, struct fpc_data, fb_notif);

	fpsensor_log(INFO_LOG, "%s blank:0x%x.\n", __func__, blank);

	switch(blank)
	{
		case DRM_MODE_DPMS_ON:
			fpc->screen_status = 1;
			fpsensor_log(INFO_LOG, "%s lcd on notify, screen_status:%d.\n", __func__, fpc->screen_status);
			break;
		case DRM_MODE_DPMS_OFF:
			fpc->screen_status = 0;
			fpsensor_log(INFO_LOG, "%s lcd off notify, screen_status:%d.\n", __func__, fpc->screen_status);
			break;
		default:
			fpsensor_log(INFO_LOG, "%s other status notify, will ignore. screen_status:%d.\n", __func__, fpc->screen_status);
			break;
	}
	func_exit();
	return NOTIFY_OK;
}
#endif

#ifdef PIN_CONTROL
static int select_pin_ctl(struct fpc_data *fpc, const char *name)
{
    size_t i;
    int rc;
	fpsensor_log(INFO_LOG, "fpc %s entry.\n", __func__);
    for (i = 0; i < ARRAY_SIZE(fpc->pinctrl_state); i++) {
        const char *n = pctl_names[i];
        if (!strncmp(n, name, strlen(n))) {
            mutex_lock(&fpc_set_gpio_mutex);
            rc = pinctrl_select_state(fpc->fpc_pinctrl, fpc->pinctrl_state[i]);
            mutex_unlock(&fpc_set_gpio_mutex);
            if (rc)
                fpsensor_log(ERROR_LOG, "fpc %s cannot select '%s'\n", __func__, name);
            else
                fpsensor_log(ERROR_LOG, "fpc %s selected '%s'\n", __func__, name);
            goto exit;
        }
    }
    rc = -EINVAL;
exit:
	fpsensor_log(INFO_LOG, "fpc %s:'%s' found.\n", __func__, name);
    return rc;
}
#endif
#ifdef USE_REGULATOR
static int vreg_setup(struct fpc_data *fpc1020, const char *name,
	bool enable)
{
	size_t i;
	int rc;
	struct regulator *vreg;
	struct device *dev = fpc1020->dev;

	for (i = 0; i < ARRAY_SIZE(fpc1020->vreg); i++) {
		const char *n = vreg_conf[i].name;

		if (!strncmp(n, name, strlen(n)))
			goto found;
	}

	dev_err(dev, "Regulator %s not found\n", name);

	return -EINVAL;

found:
	vreg = fpc1020->vreg[i];
	if (enable) {
		if (!vreg) {
			vreg = devm_regulator_get(dev, name);
			if (IS_ERR_OR_NULL(vreg)) {
				dev_err(dev, "Unable to get %s\n", name);
				return PTR_ERR(vreg);
			}
		}

		if (regulator_count_voltages(vreg) > 0) {
			rc = regulator_set_voltage(vreg, vreg_conf[i].vmin,
					vreg_conf[i].vmax);
			if (rc)
				dev_err(dev,
					"Unable to set voltage on %s, %d\n",
					name, rc);
		}

		rc = regulator_set_load(vreg, vreg_conf[i].ua_load);
		if (rc < 0)
			dev_err(dev, "Unable to set current on %s, %d\n",
					name, rc);

		rc = regulator_enable(vreg);
		if (rc) {
			dev_err(dev, "error enabling %s: %d\n", name, rc);
			vreg = NULL;
		}
		fpc1020->vreg[i] = vreg;
	} else {
		if (vreg) {
			if (regulator_is_enabled(vreg)) {
				regulator_disable(vreg);
				dev_dbg(dev, "disabled %s\n", name);
			}
			fpc1020->vreg[i] = NULL;
		}
		rc = 0;
	}

	return rc;
}

#endif

static int set_clks(struct fpc_data *fpc, bool enable)
{
    int rc = 0;
    if(fpc->clocks_enabled == enable)
        return rc;
    if (enable) {
        fpc->clocks_enabled = true;
        rc = 1;
    } else {
        fpc->clocks_enabled = false;
        rc = 0;
    }

    return rc;
}

static int hw_reset(struct  fpc_data *fpc)
{
    int irq_value = 0;
	int rc = 0;
	func_entry();

#ifdef USE_REGULATOR
		rc = vreg_setup(fpc, "vdd_io", false);
		if (rc != 0 ){
			fpsensor_log(ERROR_LOG, "fpc %s vreg_setup:false failed!\n", __func__);
			return rc;
		}
		usleep_range(FPC_POWEROFF_SLEEP_US, FPC_POWEROFF_SLEEP_US + 100);
		rc = vreg_setup(fpc, "vdd_io", true);
		if (rc !=0 ){
			fpsensor_log(ERROR_LOG, "fpc %s vreg_setup:true failed!\n", __func__);
			return rc;
		}
		usleep_range(FPC_POWEROFF_SLEEP_US, FPC_POWEROFF_SLEEP_US + 100);
#endif

#ifdef PIN_CONTROL
	rc = select_pin_ctl(fpc, "fpc1020_reset_active");
	if(rc){
		fpsensor_log(ERROR_LOG, "fpc %s pin ctrl reset active failed.\n", __func__);
		return rc;
	}
	usleep_range(FPC_RESET_HIGH1_US, FPC_RESET_HIGH1_US + 100);

	rc = select_pin_ctl(fpc, "fpc1020_reset_reset");
	if(rc){
		fpsensor_log(ERROR_LOG, "fpc %s pin ctrl reset reset failed.\n", __func__);
		return rc;
	}
	usleep_range(FPC_RESET_LOW_US, FPC_RESET_LOW_US + 100);

	rc = select_pin_ctl(fpc, "fpc1020_reset_active");
	if(rc){
		fpsensor_log(ERROR_LOG, "fpc %s pin ctrl reset active failed.\n", __func__);
		return rc;
	}
	usleep_range(FPC_RESET_HIGH2_US, FPC_RESET_HIGH2_US + 100);
#else
	gpio_direction_output(fpc->rst_gpio, 1);
	usleep_range(FPC_RESET_HIGH1_US, FPC_RESET_HIGH1_US + 100);
	gpio_direction_output(fpc->rst_gpio, 0);
	usleep_range(FPC_RESET_LOW_US, FPC_RESET_LOW_US + 100);
	gpio_direction_output(fpc->rst_gpio, 1);
	usleep_range(FPC_RESET_HIGH2_US, FPC_RESET_HIGH2_US + 100);
#endif

    irq_value = gpio_get_value(fpc->irq_gpio);
    fpsensor_log(DEBUG_LOG, "fpc %s IRQ after reset is %d\n", __func__, irq_value);
	func_exit();
    return rc;
}

static ssize_t hw_reset_set(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct  fpc_data *fpc = dev_get_drvdata(dev);
	fpsensor_log(DEBUG_LOG, "fpc %s cmd=%s\n", __func__, buf);
	
    if (!strncmp(buf, "reset", strlen("reset"))) {
        int rc;
        rc = hw_reset(fpc);
        return rc ? rc : count;
    }
    else
        return -EINVAL;
}
static DEVICE_ATTR(hw_reset, S_IWUSR, NULL, hw_reset_set);

/**
 * sysfs node for controlling whether the driver is allowed
 * to wake up the platform on interrupt.
 */
static ssize_t wakeup_enable_set(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct  fpc_data *fpc = dev_get_drvdata(dev);
    ssize_t ret = count;

    fpsensor_log(DEBUG_LOG, "fpc %s config=%s\n", __func__, buf);

    if (!strncmp(buf, "enable", strlen("enable"))) {
        fpc->wakeup_enabled = true;
        smp_wmb();
    } else if (!strncmp(buf, "disable", strlen("disable"))) {
        fpc->wakeup_enabled = false;
        smp_wmb();
    } else {
        ret = -EINVAL;
    }

    return ret;
}
static DEVICE_ATTR(wakeup_enable, S_IWUSR, NULL, wakeup_enable_set);

/**
 * sysf node to check the interrupt status of the sensor, the interrupt
 * handler should perform sysf_notify to allow userland to poll the node.
 */
static ssize_t irq_get(struct device *device,
        struct device_attribute *attribute,
        char* buffer)
{
    struct fpc_data *fpc = dev_get_drvdata(device);
    int irq = gpio_get_value(fpc->irq_gpio);

    return scnprintf(buffer, PAGE_SIZE, "%i\n", irq);
}

/**
 * writing to the irq node will just drop a printk message
 * and return success, used for latency measurement.
 */
static ssize_t irq_ack(struct device *device,
        struct device_attribute *attribute,
        const char *buffer, size_t count)
{
    struct fpc_data *fpc = dev_get_drvdata(device);
    dev_dbg(fpc->dev, "%s\n", __func__);

    return count;
}
static DEVICE_ATTR(irq, S_IRUSR | S_IWUSR, irq_get, irq_ack);

static ssize_t clk_enable_set(struct device *device,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct fpc_data *fpc = dev_get_drvdata(device);
	fpsensor_log(DEBUG_LOG, "fpc %s config=%s\n", __func__, buf);
    return set_clks(fpc, (*buf == '1')) ? : count;
}
static DEVICE_ATTR(clk_enable, S_IWUSR, NULL, clk_enable_set);


static ssize_t sensor_init(struct fpc_data *fpc)
{
    
    int ret = -1;
    int irqf = 0;

    fpsensor_log(INFO_LOG, "fpc %s entry\n", __func__);

    if(fpc == NULL) {
        fpsensor_log(ERROR_LOG, "fpc %s data is null, init failed", __func__);
        return -EINVAL;
    }
    ret = fpc_dts_pin_init(fpc);
    if( ret) {
        fpsensor_log(ERROR_LOG, "fpc %s fpc_dts_pin_init fpc init failed\n", __func__);
        goto init_exit;
    }
    ret = fpc_dts_irq_init(fpc);
    if( ret) {
        fpsensor_log(ERROR_LOG, "fpc %s fpc_dts_irq_init fpc init failed\n", __func__);
        goto init_exit;
    }

    fpc->clocks_enabled = false;
    fpc->wakeup_enabled = false;

    irqf = IRQF_TRIGGER_RISING | IRQF_ONESHOT ;
    if(fpc->prop_wakeup){
        irqf |= IRQF_NO_SUSPEND;
        device_init_wakeup(fpc->dev, 1);
    }
    ret = devm_request_threaded_irq( fpc->dev, fpc->irq_num,
            NULL, fpc_irq_handler, irqf,
            dev_name(fpc->dev), fpc);
    if (ret) {
        fpsensor_log(ERROR_LOG, "fpc %s could not request irq %d\n", __func__, fpc->irq_num);
        disable_irq_wake(fpc->irq_num); 
        free_irq(fpc->irq_num, fpc);
        //wakeup_source_trash(&fpc->ttw_wl);
        wakeup_source_remove(&fpc->ttw_wl);
        ret = devm_request_threaded_irq( fpc->dev, fpc->irq_num,
                NULL, fpc_irq_handler, irqf,
                dev_name(fpc->dev), fpc);
        fpsensor_log(ERROR_LOG, "fpc %s the value of request irq is %d\n", __func__, ret);
        if (ret) {
            goto init_exit;
        }
    }
    fpsensor_log(INFO_LOG, "fpc %s requested irq, irq_gpio=%d, irq_num=%d\n", __func__, fpc->irq_gpio, fpc->irq_num);

    /* Request that the interrupt should be wakeable */
    enable_irq_wake(fpc->irq_num);
    //wakeup_source_init(&fpc->ttw_wl, "fpc_ttw_wl");
    wakeup_source_create("fpc_ttw_wl");
    wakeup_source_add(&fpc->ttw_wl);
    //fpsensor_log(INFO_LOG, "fpc %s the value of fpc->power_ctl_gpio is %d\n", __func__, fpc->power_ctl_gpio);

#ifdef USE_REGULATOR
	ret = vreg_setup(fpc, "vdd_io", true);
	if (ret)
		goto init_exit;

#else

#ifdef PIN_CONTROL
	ret = select_pin_ctl(fpc, "fpc1020_power_active");
	if(ret){
		fpsensor_log(ERROR_LOG, "fpc %s pin ctrl power active failed.\n", __func__);
		goto init_exit;
	}
#else
	gpio_direction_output(fpc->power_ctl_gpio, 1);
	fpsensor_log(INFO_LOG, "fpc %s the value after set fpc->power_ctl_gpio 1 is %d\n", __func__, gpio_get_value(fpc->power_ctl_gpio));
#endif

#endif

    (void)hw_reset(fpc);

    fpsensor_log(INFO_LOG, "fpc %s exit\n", __func__);
init_exit:
    return ret;
}

static ssize_t sensor_release(struct fpc_data *fpc)
{
    struct device *dev = &fpc->pdev->dev;
	int ret = 0;
	
    fpsensor_log(INFO_LOG, "fpc %s entry\n", __func__);
    if(fpc->irq_num) {
        disable_irq_wake(fpc->irq_num);
        free_irq(fpc->irq_num, fpc);
		//wakeup_source_trash(&fpc->ttw_wl);
        wakeup_source_remove(&fpc->ttw_wl);
        fpc->irq_num = 0;
    }
    
	if (gpio_is_valid(fpc->rst_gpio)) {
		devm_gpio_free(dev, fpc->rst_gpio);
		fpsensor_log(DEBUG_LOG, "fpc %s remove rst_gpio success.\n", __func__);
	}

	if (gpio_is_valid(fpc->irq_gpio)) {
		devm_gpio_free(dev, fpc->irq_gpio);
		fpsensor_log(DEBUG_LOG, "fpc %s remove irq_gpio success.\n", __func__);
	}


#ifdef USE_REGULATOR
	ret = vreg_setup(fpc, "vdd_io", false);
	if (ret != 0){
		fpsensor_log(ERROR_LOG, "fpc %s vreg_setup:false failed!\n", __func__);
		return ret;
	}
#else
    if (gpio_is_valid(fpc->power_ctl_gpio)) { 

#ifdef PIN_CONTROL
		ret = select_pin_ctl(fpc, "fpc1020_power_deactive");
		if(ret){
			fpsensor_log(ERROR_LOG, "fpc %s pin ctrl power deactive failed.\n", __func__);
		}
#else
		gpio_direction_output(fpc->power_ctl_gpio, 0);
#endif
	
   		fpsensor_log(DEBUG_LOG, "cutoff power value fpc->power_ctl_gpio = %d\n", gpio_get_value(fpc->power_ctl_gpio));
        devm_gpio_free(dev, fpc->power_ctl_gpio);
    	fpsensor_log(DEBUG_LOG, "fpc %s remove power_ctl_gpio success.\n", __func__);

    }
#endif
	fpsensor_log(INFO_LOG, "fpc %s exit\n", __func__);
    return ret;
}

static ssize_t sensor_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	struct fpc_data *fpc = dev_get_drvdata(dev);
	fpsensor_log(DEBUG_LOG, "fpc %s config=%s\n", __func__, buf);
	if (!memcmp(buf, "init", strlen("init")))
		rc = sensor_init(fpc);
	else if (!memcmp(buf, "release", strlen("release")))
		rc = sensor_release(fpc);
	else
		return -EINVAL;
				
	return rc ? rc : count;
}
static DEVICE_ATTR(sensor, S_IWUSR, NULL, sensor_set);


static ssize_t hwinfo_set(struct device *device,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct fpc_data *fpc = dev_get_drvdata(device);
	fpsensor_log(INFO_LOG, "fpc %s set hwinfo:%s", __func__, buf);
	snprintf(fpc->hwid, HWID_SIZE, "%s", buf);
	return count;
}

static ssize_t hwinfo_get(struct device *device,
        struct device_attribute *attribute,
        char* buffer)
{
    struct fpc_data *fpc = dev_get_drvdata(device);
    fpsensor_log(INFO_LOG, "fpc %s get hwinfo:%s", __func__, fpc->hwid);
    return snprintf(buffer, HWID_SIZE, "%s\n", fpc->hwid);
}

static DEVICE_ATTR(hwinfo, S_IRUSR | S_IWUSR, hwinfo_get, hwinfo_set);

/*
static ssize_t screen_get(struct device *device,
		struct device_attribute *attr,
		char *buf)
{
	struct fpc_data *fpc = dev_get_drvdata(device);
	fpsensor_log(INFO_LOG, "fpc %s get screen_status:%d", __func__, fpc->screen_status);
    return snprintf(buf, PAGE_SIZE, "%d\n", fpc->screen_status);
}

static ssize_t screen_set(struct device *device,
		struct device_attribute *attr, const char *buf, size_t count)
{
    int value;
	struct fpc_data *fpc = dev_get_drvdata(device);
	fpsensor_log(INFO_LOG, "fpc %s set screen_status:%s", __func__, buf);
    if (sscanf(buf, "%d", &value) == 1) {
        fpc->screen_status = value;
        return count;
    }
    return -EINVAL;
}

static DEVICE_ATTR(screen, S_IWUSR | S_IRUGO, screen_get, screen_set);
*/
static struct attribute *fpc_attributes[] = {
    &dev_attr_hw_reset.attr,
    &dev_attr_wakeup_enable.attr,
    &dev_attr_clk_enable.attr,
    &dev_attr_irq.attr,
    &dev_attr_sensor.attr,
    &dev_attr_hwinfo.attr,
    //&dev_attr_screen.attr,
    NULL
};

static const struct attribute_group fpc_attribute_group = {
    .attrs = fpc_attributes,
};
static irqreturn_t fpc_irq_handler(int irq, void *handle)
{
    struct fpc_data *fpc = handle;
    struct device *dev = fpc->dev;

    /* Make sure 'wakeup_enabled' is updated before using it
     ** since this is interrupt context (other thread...) */
    smp_rmb();
    if (fpc->wakeup_enabled) {
        __pm_wakeup_event(&fpc->ttw_wl, FPC_TTW_HOLD_TIME);
    }

    sysfs_notify(&dev->kobj, NULL, dev_attr_irq.attr.name);

    return IRQ_HANDLED;
}
static int fpc_dts_irq_init(struct fpc_data *fpc)
{
    int error = 0;

    func_entry();

	if(gpio_is_valid(fpc->irq_gpio)){
#ifdef PIN_CONTROL
		error = select_pin_ctl(fpc, "fpc1020_irq_active");
		if(error){
			fpsensor_log(ERROR_LOG, "fpc %s pin ctrl irq active failed.\n", __func__);
			return error;
		}
#else
		gpio_direction_output(fpc->irq_gpio, 0);
#endif
    	error = gpio_direction_input(fpc->irq_gpio);
		if (error != 0) {
	        fpsensor_log(ERROR_LOG,"%s setup fpsensor irq gpio for input failed!error[%d]\n", __func__, error);
	        return -EINVAL;
	    }
		fpc->irq_num = gpio_to_irq(fpc->irq_gpio);
		if (!fpc->irq_num) {
            fpsensor_log(ERROR_LOG,"%s fpc irq_of_parse_and_map failed, irq_gpio=%d, irq_num=%d\n", __func__, 
                    fpc->irq_gpio, fpc->irq_num);
            return -EINVAL;
        }
		fpsensor_log(INFO_LOG,"%s fpc->irq_gpio=%d, fpc->irq_num=%d\n", __func__, fpc->irq_gpio,
                fpc->irq_num);
	}
	else{
		fpsensor_log(ERROR_LOG,"%s fpc->irq_gpio isn't valid!\n", __func__);
        return -EINVAL;
	}
	
	func_exit();
    return error;
}

static int fpc_dts_pin_init( struct fpc_data * fpc)
{
    struct device_node *node = NULL;
    struct device *dev = &fpc->pdev->dev;
    int ret = 0;
	func_entry();
    node = of_find_compatible_node(NULL, NULL, "fpc,fpc1553");
    if (node) {

#ifndef USE_REGULATOR
         fpc->power_ctl_gpio = of_get_named_gpio(node, "fpc,power_ctl_gpio", 0);
         if (fpc->power_ctl_gpio < 0) {
             fpsensor_log(ERROR_LOG,"%s failed to get fpc,power_ctl_gpio!\n", __func__);
             return fpc->power_ctl_gpio;
         }
         ret = devm_gpio_request(dev, fpc->power_ctl_gpio, "fpc,power_ctl_gpio");
         if (ret) {
             fpsensor_log(ERROR_LOG,"%s failed to request power_ctl_gpio %d, ret = %d.\n", __func__, fpc->power_ctl_gpio, ret);
             gpio_free(fpc->power_ctl_gpio);
             ret = devm_gpio_request(dev, fpc->power_ctl_gpio, "fpc,power_ctl_gpio");
			 if(ret){
				fpsensor_log(ERROR_LOG,"%s failed to request power_ctl_gpio %d, ret = %d. retry failed.\n", __func__, fpc->power_ctl_gpio, ret);
				return ret;
			 }
         }
		 fpsensor_log(INFO_LOG,"%s the value of request fpc->power_ctl_gpio is %d\n", __func__, fpc->power_ctl_gpio);
#endif
		fpc->rst_gpio = of_get_named_gpio(node, "fpc,rst_gpio", 0); 
        if (fpc->rst_gpio < 0) {
            fpsensor_log(ERROR_LOG,"%s failed to get fpc,rst_gpio! \n", __func__);
            return fpc->rst_gpio;
        }
		
		ret = devm_gpio_request(dev, fpc->rst_gpio, "fpc,rst_gpio");
		if (ret) {
	    	fpsensor_log(ERROR_LOG,"%s failed to request rst_gpio %d, ret = %d.\n", __func__, fpc->rst_gpio, ret);
            gpio_free(fpc->rst_gpio);
            ret = devm_gpio_request(dev, fpc->rst_gpio, "fpc,rst_gpio");
			if(ret){
				fpsensor_log(ERROR_LOG,"%s failed to request rst_gpio %d, ret = %d. retry failed.\n", __func__, fpc->rst_gpio, ret);
				return ret;
			}
            
		}
		fpsensor_log(INFO_LOG,"%s the value of request fpc->rst_gpio is %d\n", __func__, fpc->rst_gpio);

		fpc->irq_gpio = of_get_named_gpio(node, "fpc,irq_gpio", 0); 
        if (fpc->irq_gpio < 0) {
            fpsensor_log(ERROR_LOG,"%s failed to get fpc,irq_gpio!\n", __func__);
            return fpc->irq_gpio;
        } 
        ret = devm_gpio_request(dev, fpc->irq_gpio, "fpc,irq_gpio");
        if (ret) {
            fpsensor_log(ERROR_LOG,"%s failed to request irq_gpio %d, ret = %d.\n", __func__, fpc->irq_gpio, ret);
            gpio_free(fpc->irq_gpio);
            ret = devm_gpio_request(dev, fpc->irq_gpio, "fpc,irq_gpio");
			if(ret){
				fpsensor_log(ERROR_LOG,"%s failed to request irq_gpio %d, ret = %d.retry failed.\n", __func__, fpc->irq_gpio, ret);
				return ret;
			}
        }
		fpsensor_log(INFO_LOG,"%s the value of request fpc->irq_gpio is %d\n", __func__, fpc->irq_gpio);
        fpsensor_log(INFO_LOG,"%s fpc->irq_gpio =%d, fpc->rst_gpio = %d\n", __func__, fpc->irq_gpio, fpc->rst_gpio);

#ifdef PIN_CONTROL
		ret = select_pin_ctl(fpc, "fpc1020_reset_active");
		if(ret){
			fpsensor_log(ERROR_LOG, "fpc %s pin ctrl reset active failed.\n", __func__);
			return ret;
		}
#else
		gpio_direction_output(fpc->rst_gpio, 1);
#endif
		func_exit();
		return ret;
    } else {
        fpsensor_log(ERROR_LOG,"%s Cannot find node!\n", __func__);
        ret = -ENODEV;
		func_exit();
		return ret;
	}
}

static int fpc_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct fpc_data *fpc;
    int rc = 0;
#ifdef PIN_CONTROL
	int i;
#endif

    func_entry();

    fpc = devm_kzalloc(dev, sizeof(*fpc), GFP_KERNEL);
    if (fpc == NULL) {
        fpsensor_log(ERROR_LOG, "%s failed to allocate memory for struct fpc_data\n", __func__);
        rc = -ENOMEM;
        goto fpc_probe_exit;
    }
    memset( fpc, 0, sizeof(struct fpc_data));

    fpc->dev = dev;
    fpc->pdev = pdev;

	dev_set_drvdata(dev, fpc);

#ifdef PIN_CONTROL
	fpc->fpc_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(fpc->fpc_pinctrl)) {
		if (PTR_ERR(fpc->fpc_pinctrl) == -EPROBE_DEFER) {
			fpsensor_log(ERROR_LOG, "%s pinctrl not ready\n", __func__);
			rc = -EPROBE_DEFER;
			goto fpc_probe_exit;
		}
		fpsensor_log(ERROR_LOG, "%s Target does not use pinctrl\n", __func__);
		fpc->fpc_pinctrl = NULL;
		rc = -EINVAL;
		goto fpc_probe_exit;
	}

	for (i = 0; i < ARRAY_SIZE(pctl_names); i++) {
		const char *n = pctl_names[i];
		struct pinctrl_state *state =
			pinctrl_lookup_state(fpc->fpc_pinctrl, n);
		if (IS_ERR(state)) {
			fpsensor_log(ERROR_LOG, "%s cannot find '%s'\n", __func__, n);
			rc = -EINVAL;
			goto fpc_probe_exit;
		}
		fpsensor_log(INFO_LOG, "%s found pin control %s in [%d]\n", __func__, n, i);
		fpc->pinctrl_state[i] = state;
	}
#endif

    if (of_property_read_bool(dev->of_node, "fpc,enable-wakeup")) {
        fpc->prop_wakeup = 1;
    } else {
        fpc->prop_wakeup = 0;
    }
    fpsensor_log(DEBUG_LOG, "%s prop_wakeup=%d\n", __func__, fpc->prop_wakeup);

    
    rc = sysfs_create_group(&dev->kobj, &fpc_attribute_group);
    if (rc != 0) {
        fpsensor_log(ERROR_LOG, "%s create sysfs failed\n", __func__);
        goto fpc_probe_exit;
    }

	rc = sprintf(fpc->hwid,"%s","NULL");
	if(rc < 0){
		fpsensor_log(ERROR_LOG, "%s init hwid failed\n", __func__);
	}

#ifdef CONFIG_FB
	fpc->screen_status = 1;
	fpc->fb_notif.notifier_call = fb_notifier_callback;
	//rc = fb_register_client(&fpc->fb_notif);
	//rc = dsp_register_client(&fpc->fb_notif);
	if (rc)
		fpsensor_log(ERROR_LOG, "%s Unable to register fb_notifier: %d\n", __func__, rc);
#endif

    fpsensor_log(INFO_LOG, "%s FPC Module Probing Success\n", __func__);
	rc = 0;
	func_exit();
    return rc;

fpc_probe_exit:
    if(fpc != NULL) {
        kfree(fpc);
    }
    fpsensor_log(ERROR_LOG, "%s FPC Module Probing Failure, ret=%d\n", __func__, rc);
	func_exit();
    return rc;
}

static int fpc_remove(struct platform_device *pdev)
{
    struct  fpc_data *fpc = dev_get_drvdata(&pdev->dev);
	func_entry();
    sysfs_remove_group(&pdev->dev.kobj, &fpc_attribute_group);
    //wakeup_source_trash(&fpc->ttw_wl);
    wakeup_source_remove(&fpc->ttw_wl);
	//dsp_unregister_client(&fpc->fb_notif);
    fpsensor_log(INFO_LOG, "%s FPC Module Remove\n", __func__);
	func_exit();
    return 0;
}
static struct of_device_id fpc_of_match[] = {
    { .compatible = "fpc,fpc1553", },
    {}
};
MODULE_DEVICE_TABLE(of, fpc_of_match);

static struct platform_driver fpc_plat_driver =
{
    .driver = {
        .name   = "fpc",
        .bus    = &platform_bus_type,
        .owner	= THIS_MODULE,
        .of_match_table = fpc_of_match,

    },
    .probe	= fpc_probe,
    .remove	= fpc_remove,
};

static int __init fpc_init(void)
{
    int status;

    status = platform_driver_register(&fpc_plat_driver);
	func_entry();
    fpsensor_log(INFO_LOG, "%s FPC Module Init, %s\n", __func__, GET_STR_BYSTATUS(status));
	func_exit();
    return status;
}
module_init(fpc_init);

static void __exit fpc_exit(void)
{
	func_entry();
    platform_driver_unregister(&fpc_plat_driver);
    func_exit();
}
module_exit(fpc_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("fingerprint driver, fpc, sw48.0");
MODULE_ALIAS("fingerprint:fpc-drivers");
