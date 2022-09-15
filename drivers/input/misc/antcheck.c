#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/types.h>   

#include <linux/miscdevice.h>

#include <linux/of.h>
#include <linux/device.h>
#include <linux/string.h>

#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/proc_fs.h>
#include <linux/input.h>


static int antcheck_gpio_pin = -1;
static int antcheck_irq_gpio_pin = -1;
static int antcheck_irq_num_pin = -1; 

static struct device_node* np; 

static struct input_dev*  touch_dev;

int value = 1;
int old_value = -1;

static irqreturn_t antcheck_irq_func(int irq, void *data)
{
	printk(KERN_ALERT "%s antcheck interrupt start", __func__);
	value = gpio_get_value(antcheck_irq_gpio_pin);
	if(value != old_value)
	{
		if (value == 0)
		{
			input_report_key(touch_dev, KEY_ANT, 1);
			old_value = value;
		} else {
			input_report_key(touch_dev, KEY_ANT, 0);
			old_value = value;
		}
		input_sync(touch_dev);
	}
	return IRQ_HANDLED;
}

static int gpio_proc_show(struct seq_file *file, void *data)
{

	printk("%s keven: antcheck sar_int_gpio was %d \n", __func__, antcheck_irq_gpio_pin);
	value = gpio_get_value(antcheck_gpio_pin);
	printk("%s keven:  value=%d \n", __func__, value);
	seq_printf(file, "%d\n", value);

	return 0;
}

static int gpio_proc_open (struct inode *inode, struct file *file)
{
	return single_open(file, gpio_proc_show, inode->i_private);
}

static const struct file_operations gpio_status_ops = {
	.open = gpio_proc_open,
	.read = seq_read,
};

static struct proc_dir_entry *gpio_status;
#define GOIP_STATUS "antcheck"

static int antcheck_probe(struct platform_device *pdev)
{
	int ret = -1;
	struct device* dev = &pdev->dev;
	np = dev->of_node;

	printk(KERN_ALERT"+++++antcheck_probe ok++++++++");

    //gpio
	antcheck_gpio_pin = of_get_named_gpio(np, "vituralsar,irq-gpio", 0);
	printk(KERN_ALERT "+++++++++++gpio_pin_antcheck = %d++++++++", antcheck_gpio_pin);
	ret = gpio_request(antcheck_gpio_pin, "SAR_ANTCHECK_IRQ");
	if (ret != 0){
		printk(KERN_ALERT "+++gpio_request failed+++");
		goto err_gpio_request;
	}

	ret = gpio_direction_input(antcheck_gpio_pin);
	if (ret) {
		printk("[GPIO]set_direction for irq gpio failed");
		goto err_gpio_request;
	}

    //中断
	antcheck_irq_gpio_pin = of_get_named_gpio(np, "vituralsar,irq-gpio", 0);
	printk(KERN_ALERT "antcheck_irq_gpio_pin = %d\n", antcheck_irq_gpio_pin);
	antcheck_irq_num_pin = gpio_to_irq(antcheck_irq_gpio_pin);
	ret = request_threaded_irq(antcheck_irq_num_pin, NULL, antcheck_irq_func, IRQ_TYPE_EDGE_RISING | IRQF_ONESHOT, "platform-antcheck", NULL);
	if(ret != 0){
		printk(KERN_ALERT "+++++request_threaded_irq is failed  ret = %d", ret);
		goto err_request_threaded_irq;
	}

    //input子系统
    touch_dev = input_allocate_device();
	if(!touch_dev)
	    printk("+++++++++input_allocate_device failed++++++++++");
    touch_dev->name = "antcheck_input";
	__set_bit(EV_SYN, touch_dev->evbit);
	__set_bit(EV_ABS, touch_dev->evbit);
	__set_bit(EV_KEY, touch_dev->evbit);

	input_set_abs_params(touch_dev, KEY_ANT, -1, 100, 0, 0);

	ret = input_register_device(touch_dev);
	if(ret){
		printk("input_register_device failed ret=%d\n", ret);
		goto err_input_register_device;
	}
	input_set_capability(touch_dev, EV_KEY, KEY_ANT);

	gpio_status = proc_create(GOIP_STATUS, 0644, NULL, &gpio_status_ops);
	if (gpio_status == NULL) {
		printk("tpd, create_proc_entry gpio_status_ops failed\n");
	}

    return 0;

err_input_register_device:
	input_free_device(touch_dev);
err_request_threaded_irq:
	free_irq(antcheck_irq_num_pin, NULL);
err_gpio_request:
	gpio_free(antcheck_gpio_pin);

	return 0;
}

static int antcheck_remove(struct platform_device *pdev)
{
//	misc_deregister(&miscdev);
	free_irq(antcheck_irq_num_pin, NULL);
	return 0;
}
static struct of_device_id antcheck_dtb_table[] = {
	{.compatible = "virtualsar,antcheck",},
	{ },
};

static struct platform_driver antcheck_driver = {
	.probe = antcheck_probe,
	.remove = antcheck_remove,
	.driver = {
		.name = "platform-antcheck",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(antcheck_dtb_table),
	},
};

static int __init antcheck_init(void)
{
	platform_driver_register(&antcheck_driver);
    return 0;
}

static void __exit antcheck_exit(void)
{
	platform_driver_unregister(&antcheck_driver);
}

MODULE_LICENSE("GPL");

module_init(antcheck_init);
module_exit(antcheck_exit);
