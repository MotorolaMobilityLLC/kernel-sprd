#define DEBUG

#include <linux/device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>


//#define PINCTRL_STATE_ACTIVE   "boardid_active"
//#define PINCTRL_STATE_SUSPEND  "boardid_suspend"
//#define PINCTRL_STATE_RELEASE  "boardid_release"
extern unsigned int platform_board_id;
extern unsigned int platform_nfc_flag;
extern unsigned int platform_prj_ver_flag;

static const char * const boardid_gpios[] = {
	"gpio,boardid0",
	"gpio,boardid1",
	"gpio,boardid2",
//	"gpio,boardid3", //this pin no func ,the point from lixiaohui@20210721
//	"gpio,boardid4",
};
/* BEGIN Ontim, yangruining, 20210403, 10015326, St-result:PASS, Add project version drive device note. */
static const char * const boardprjid_gpios[] = {
//	"gpio,boardprjid",
//	"gpio,boardprjid0",
//	"gpio,boardprjid1",
};

struct gpio_data {
	int value;
	int nfcvalue;
	int prjvalue;
	struct boardid *boardid_gpio;
	struct boardid *boardprjid_gpio;
	//struct pinctrl *boardid_pinctrl;
	//struct pinctrl_state *pinctrl_state_active;
	//struct pinctrl_state *pinctrl_state_suspend;
	//struct pinctrl_state *pinctrl_state_release;
};
/* END 10015326 */
struct boardid {
	unsigned int gpio;
	char gpio_name[32];
};


static int nlsx_parse_dt(struct device *dev, struct gpio_data *data)
{
	struct device_node *of_node = dev->of_node;
	int i = 0;

	data->boardid_gpio = devm_kzalloc(dev,sizeof(struct boardid) *
			ARRAY_SIZE(boardid_gpios),GFP_KERNEL);
	if (!data->boardid_gpio)
		return -ENOMEM;
	for (i = 0; i < ARRAY_SIZE(boardid_gpios); i++) {
		data->boardid_gpio[i].gpio = of_get_named_gpio(of_node,boardid_gpios[i],0);

		if (data->boardid_gpio[i].gpio < 0) {
			dev_err(dev, " dts get gpio error\n");
			return -EINVAL;
		}

		strlcpy(data->boardid_gpio[i].gpio_name,boardid_gpios[i],
				sizeof(data->boardid_gpio[i].gpio_name));

		printk(KERN_ERR "data->boardid_gpio[%d].gpio = %d, data->boardid_gpio[%d].gpio_name = %s", i, data->boardid_gpio[i].gpio, i, data->boardid_gpio[i].gpio_name);

	}

	//prj id
	data->boardprjid_gpio = devm_kzalloc(dev,sizeof(struct boardid) *
			ARRAY_SIZE(boardprjid_gpios),GFP_KERNEL);
	if (!data->boardprjid_gpio)
		return -ENOMEM;
	for (i = 0; i < ARRAY_SIZE(boardprjid_gpios); i++) {
		data->boardprjid_gpio[i].gpio = of_get_named_gpio(of_node,boardprjid_gpios[i],0);

		if (data->boardprjid_gpio[i].gpio < 0) {
			dev_err(dev, " dts get gpio error\n");
			return -EINVAL;
		}

		strlcpy(data->boardprjid_gpio[i].gpio_name,boardprjid_gpios[i],
				sizeof(data->boardprjid_gpio[i].gpio_name));

		printk(KERN_ERR "data->boardprjid_gpio[%d].gpio = %d, data->boardprjid_gpio[%d].gpio_name = %s", i, data->boardprjid_gpio[i].gpio, i, data->boardprjid_gpio[i].gpio_name);

	}

	return 0;
}
#if 0
static int gpio_boardid_pinctrl_init(struct device *dev, struct gpio_data *data)
{
	int retval;

	/* Get pinctrl if target uses pinctrl */
	data->boardid_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(data->boardid_pinctrl)) {
		retval = PTR_ERR(data->boardid_pinctrl);
		dev_dbg(dev,
				"Target does not use pinctrl %d\n", retval);
		goto err_pinctrl_get;
	}

	data->pinctrl_state_active = pinctrl_lookup_state(data->boardid_pinctrl,
			PINCTRL_STATE_ACTIVE);
	if (IS_ERR_OR_NULL(data->pinctrl_state_active)) {
		retval = PTR_ERR(data->pinctrl_state_active);
		dev_err(dev,
				"Can not lookup %s pinstate %d\n",
				PINCTRL_STATE_ACTIVE, retval);
		goto err_pinctrl_lookup;
	}

	data->pinctrl_state_suspend
		= pinctrl_lookup_state(data->boardid_pinctrl,
				PINCTRL_STATE_SUSPEND);
	if (IS_ERR_OR_NULL(data->pinctrl_state_suspend)) {
		retval = PTR_ERR(data->pinctrl_state_suspend);
		dev_err(dev,
				"Can not lookup %s pinstate %d\n",
				PINCTRL_STATE_SUSPEND, retval);
		goto err_pinctrl_lookup;
	}

	data->pinctrl_state_release
		= pinctrl_lookup_state(data->boardid_pinctrl,
				PINCTRL_STATE_RELEASE);
	if (IS_ERR_OR_NULL(data->pinctrl_state_release)) {
		retval = PTR_ERR(data->pinctrl_state_release);
		dev_dbg(dev,
				"Can not lookup %s pinstate %d\n",
				PINCTRL_STATE_RELEASE, retval);
	}

	return 0;
err_pinctrl_lookup:
	devm_pinctrl_put(data->boardid_pinctrl);
err_pinctrl_get:
	data->boardid_pinctrl = NULL;
	return retval;
}
#endif
static int gpio_boardid_suspend(struct device *dev)
{
#if 0
	struct gpio_data *data = dev_get_drvdata(dev);
	pr_debug("gpio_boardid suspend enter\n");
#endif
	return 0;
}
static int gpio_boardid_resume(struct device *dev)
{
#if 0
	struct gpio_data *data = dev_get_drvdata(dev);
	pr_debug("gpio_boardid resume\n");
#endif
	return 0;
}
static const struct dev_pm_ops gpio_boardid_pm_ops = {
	.suspend = gpio_boardid_suspend,
	.resume = gpio_boardid_resume,
};

static int gpio_boardid_probe(struct platform_device *pdev)
{
	struct gpio_data *data;
	int err = 0;
	int i;

	//dev_dbg(&pdev->dev, "%s start\n",__func__);
	printk(KERN_ERR "%s start\n",__func__);
	data = devm_kzalloc(&pdev->dev, sizeof(struct gpio_data), GFP_KERNEL);
	if (data == NULL) {
		err = -ENOMEM;
		dev_err(&pdev->dev, "failed to allocate memory %d\n", err);
		goto alloc_data_failed;
	}

	dev_set_drvdata(&pdev->dev, data);
	if (pdev->dev.of_node) {
		err = nlsx_parse_dt(&pdev->dev, data);
		if (err < 0) {
			dev_err(&pdev->dev, "Failed to parse device tree\n");
			goto parse_dt_failed;
		}
	} else if (pdev->dev.platform_data != NULL) {
		memcpy(data, pdev->dev.platform_data, sizeof(*data));
	} else {
		dev_err(&pdev->dev, "No valid platform data.\n");
		err = -ENODEV;
		goto invalid_pdata;
	}

	for (i = 0; i < ARRAY_SIZE(boardid_gpios); i++) {
		if (gpio_is_valid(data->boardid_gpio[i].gpio)) {
			err = gpio_request(data->boardid_gpio[i].gpio,data->boardid_gpio[i].gpio_name);
			if (err) {
				dev_err(&pdev->dev, "request %d gpio failed, err = %d\n",data->boardid_gpio[i].gpio, err);
				goto gpio_request_error;
			}

			err = gpio_direction_input(data->boardid_gpio[i].gpio);
			if (err) {
				dev_err(&pdev->dev,"gpio %d set input failed\n",data->boardid_gpio[i].gpio);
				goto gpio_request_error;
			}

			data->value |= ((gpio_get_value(data->boardid_gpio[i].gpio)) ? 1 : 0) << i;
		}
	}

	platform_board_id = data->value;
	printk(KERN_ERR "deng %s ok boardid= %04d,value = %04d\n",__func__,platform_board_id,data->value);

    //yrn add project_id to distinguish project  Aruba:0 Cyprus:1
	for (i = 0; i < ARRAY_SIZE(boardprjid_gpios); i++) {
		if (gpio_is_valid(data->boardprjid_gpio[i].gpio)) {
			err = gpio_request(data->boardprjid_gpio[i].gpio,data->boardprjid_gpio[i].gpio_name);
			if (err) {
				dev_err(&pdev->dev, "[nfc]request %d gpio failed, err = %d\n",data->boardprjid_gpio[i].gpio, err);
				goto gpio_request_error;
			}

			err = gpio_direction_input(data->boardprjid_gpio[i].gpio);
			if (err) {
				dev_err(&pdev->dev,"[nfc]gpio %d set input failed\n",data->boardprjid_gpio[i].gpio);
				goto gpio_request_error;
			}

			data->prjvalue |= ((gpio_get_value(data->boardprjid_gpio[i].gpio)) ? 1 : 0) << i;
		}
	}

	platform_prj_ver_flag = data->prjvalue;
	printk(KERN_ERR "yrn %s ok boardproject_id= %04d,value = %04d\n",__func__,platform_prj_ver_flag,data->prjvalue);

#if 0
	err = gpio_boardid_pinctrl_init(&pdev->dev, data);
	if (!err && data->boardid_pinctrl) {
		/*
		 * Pinctrl handle is optional. If pinctrl handle is found
		 * let pins to be configured in active state. If not
		 * found continue further without error.
		 */
		err = pinctrl_select_state(data->boardid_pinctrl,
				data->pinctrl_state_active);
		if (err < 0) {
			dev_err(&pdev->dev,
					"failed to select pin to active state");
			goto pinctrl_invalid;
		}
	}
#endif
	return err;
#if 0
pinctrl_invalid:
	if (data->boardid_pinctrl) {
		if (IS_ERR_OR_NULL(data->pinctrl_state_release)) {
			devm_pinctrl_put(data->boardid_pinctrl);
			data->boardid_pinctrl = NULL;
		} else {
			err = pinctrl_select_state(data->boardid_pinctrl,
					data->pinctrl_state_release);
			if (err)
				pr_err("failed to select relase pinctrl state\n");
		}
	}
#endif
gpio_request_error:
	for (i = 0; i < ARRAY_SIZE(boardid_gpios); i++) {
		if (gpio_is_valid(data->boardid_gpio[i].gpio)) {
			gpio_free(data->boardid_gpio[i].gpio);
		}
	}
invalid_pdata:
parse_dt_failed:
alloc_data_failed:
	devm_kfree(&pdev->dev, data);
	return err;
}

static int gpio_boardid_remove(struct platform_device *pdev)
{

	//struct gpio_data *data = dev_get_drvdata(pdev);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct of_device_id platform_match_table[] = {
	{ .compatible = "gpio,boardid",},
	{ },
};

static struct platform_driver gpio_boardid_driver = {
	.driver = {
		.name           = "gpio_boardid",
		.of_match_table	= platform_match_table,
		.pm             = &gpio_boardid_pm_ops,
		.owner	        = THIS_MODULE,
	},
	.probe      = gpio_boardid_probe,
	.remove     = gpio_boardid_remove,
};


static int __init gpio_boardid_init(void)
{
	    return platform_driver_register(&gpio_boardid_driver);
}
module_init(gpio_boardid_init);

static void __exit gpio_boardid_exit(void)
{
	    return platform_driver_unregister(&gpio_boardid_driver);
}
module_exit(gpio_boardid_exit);

MODULE_DESCRIPTION("gpio boardid driver");
MODULE_LICENSE("GPL");
