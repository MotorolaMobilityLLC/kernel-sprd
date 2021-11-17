#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>

// ---------------------------------------------------------------------------
//  DEBUG
// ---------------------------------------------------------------------------
#define LOG_TAG_K "[USBCABLE]"
#define DEBUG_ANT_DETECT

#ifdef DEBUG_ANT_DETECT
#define PK_XLOG_ERR(fmt, arg...)	printk(KERN_ERR LOG_TAG_K "ERROR:%s(Line:%d): " fmt "\n", __FUNCTION__ , __LINE__ ,##arg)
#define PK_XLOG_INFO(fmt, arg...)	printk(KERN_INFO LOG_TAG_K "%s(Line:%d): " fmt "\n", __FUNCTION__ , __LINE__ ,##arg)
#define FUNCTION_ENTER 				printk(KERN_INFO LOG_TAG_K "-------Enter: %s(Line:%d)\n", __FUNCTION__ , __LINE__)
#define FUNCTION_EXIT 				printk(KERN_INFO LOG_TAG_K "-------Exit: %s(Line:%d)\n", __FUNCTION__ , __LINE__)
#else
#define PK_XLOG_ERR(a,...)
#define PK_XLOG_INFO(a...)
#define FUNCTION_ENTER
#define FUNCTION_EXIT
#endif

static u32 usbcable_eint_num = 0;
static u32 usbcable_gpio_num = 0,usbcable_gpio_deb = 0;
static int last_eint_level = 0;
static struct input_dev *cable_input_dev;

static int usbcable_probe(struct platform_device *pdev);

static const struct of_device_id usbcable_of_match[] = {
	{ .compatible = "sprd,usb_cable_eint", },
	{},
};

MODULE_DEVICE_TABLE(of, usbirq_dt_match);

static struct platform_driver usbcable_driver = {
	.probe = usbcable_probe,
	.driver = {
		.name = "Usbcable_driver",
		.owner	= THIS_MODULE,
		.of_match_table = usbcable_of_match,
	},
};
//module_platform_driver(usbcable_driver);/*USB CABLE GPIO EINT19*/

unsigned int get_rf_gpio_value(void)
{
	return last_eint_level;
}
EXPORT_SYMBOL(get_rf_gpio_value);

 static irqreturn_t usbcable_irq_handler(int irq, void *dev)
 {
	int level = 0;
	//FUNCTION_ENTER;

	level = gpio_get_value(usbcable_gpio_num);
	//PK_XLOG_INFO(" gpio_value=%d,last_value=%d\n",level,last_eint_level);
	if(level!=last_eint_level){
		PK_XLOG_INFO("report key [%d]\n",level);
		input_report_key(cable_input_dev, KEY_TWEN, level);
 		input_sync(cable_input_dev);
		last_eint_level = level;
 	}
	//FUNCTION_EXIT;
	return IRQ_HANDLED;
}

static int usbcable_probe(struct platform_device *pdev){
	int ret = 0;
	struct device_node *node = NULL;
	//struct pinctrl_state *pins_default = NULL;
	//static struct pinctrl *usbcable_pinctrl;
	FUNCTION_ENTER;

	//use pinctrl set default state
	/*usbcable_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(usbcable_pinctrl)) {
		ret = PTR_ERR(usbcable_pinctrl);
		dev_notice(&pdev->dev, "get usbcable_pinctrl fail.\n");
		return ret;
	}

	pins_default = pinctrl_lookup_state(usbcable_pinctrl, "default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		dev_notice(&pdev->dev, "lookup default pinctrl fail\n");
		return ret;
	}

	pinctrl_select_state(usbcable_pinctrl, pins_default);*/

	//get device node
	node = of_find_matching_node(NULL, usbcable_of_match);
	if (!node) {
		PK_XLOG_ERR("can't find compatible node\n");
		return -1;
	}

	//get usbcable-gpio num and set gpio dir/debounce...
	usbcable_gpio_num = of_get_named_gpio(node, "usbcable-gpio", 0);
	ret = of_property_read_u32(node, "debounce", &usbcable_gpio_deb);
	if (ret < 0) {
		PK_XLOG_ERR("gpiodebounce not found,ret:%d\n", ret);
		return ret;
	}

    ret = gpio_request_one(usbcable_gpio_num, GPIOF_IN,"usbcable-gpio" "-int");
    if (ret) {
        PK_XLOG_ERR("Request INT gpio (%d) failed %d", usbcable_gpio_num, ret);
        return ret;
    }
	
    ret = gpio_direction_input(usbcable_gpio_num);                                                                                                                            
    if (ret < 0) {
        PK_XLOG_ERR("gpio-%d input set fail!!!\n", usbcable_gpio_num);
        return ret;
    }
	
	gpio_set_debounce(usbcable_gpio_num, usbcable_gpio_deb);
	PK_XLOG_INFO("usbcable_gpio_num<%d>debounce<%d>,\n", usbcable_gpio_num,usbcable_gpio_deb);
	
	//alloc input device and register it.
	cable_input_dev = input_allocate_device();
	if(!cable_input_dev)
		return -1;
	cable_input_dev->name = "USBCABLE";
	cable_input_dev->id.bustype = BUS_HOST;

	__set_bit(EV_KEY, cable_input_dev->evbit);
	__set_bit(KEY_TWEN, cable_input_dev->keybit);

	ret = input_register_device(cable_input_dev);
	if (ret){
		input_free_device(cable_input_dev);
		PK_XLOG_ERR("input_register_device fail, ret:%d.\n",ret);
		return -1;
	}

	//usbcable_eint_num = irq_of_parse_and_map(node, 0);
	usbcable_eint_num = gpio_to_irq(usbcable_gpio_num);
    if (usbcable_eint_num < 0) {
        PK_XLOG_ERR("Parse irq failed %d", ret);
        return usbcable_eint_num;
    }
	PK_XLOG_INFO("usbcable_eint_num<%d>\n",usbcable_eint_num);
	ret = request_irq(usbcable_eint_num, usbcable_irq_handler,IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING , "USB_CABLE", NULL);
	if (ret) {
		PK_XLOG_ERR("request_irq fail, ret:%d.\n",ret);
		return ret;
	}
	//enable_irq_wake(usbcable_eint_num);
	PK_XLOG_INFO("usbcable eint request success!\n");

	ret = gpio_get_value(usbcable_gpio_num);
	if(ret!=last_eint_level){
		PK_XLOG_INFO("probe report key [%d]\n",ret);
		input_report_key(cable_input_dev, KEY_TWEN, ret);
		input_sync(cable_input_dev);
		last_eint_level = ret;
	}

	PK_XLOG_INFO("usbcable_gpio_val<%d>\n",ret);
	FUNCTION_EXIT;
	return 0;
}

static int __init usbcable_mod_init(void)
{
	int ret = 0;
	ret = platform_driver_register(&usbcable_driver);
 	if (ret)
 		PK_XLOG_ERR("usbcable platform_driver_register error:(%d)\n", ret);
 	FUNCTION_EXIT;
 	return ret;
}

static void __exit usbcable_mod_exit(void)
{
 	FUNCTION_ENTER;
	platform_driver_unregister(&usbcable_driver);
}

module_init(usbcable_mod_init);
module_exit(usbcable_mod_exit);

MODULE_AUTHOR("Sha Bei<bei.sha@ontim.cn>");
MODULE_DESCRIPTION("usb cable gpio irq driver");
MODULE_LICENSE("GPL");
