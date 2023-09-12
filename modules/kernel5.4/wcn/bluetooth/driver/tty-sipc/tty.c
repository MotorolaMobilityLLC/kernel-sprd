/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/console.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/types.h>
#include <linux/vt_kern.h>
#include <linux/sipc.h>
#include <linux/io.h>
#include "alignment/sitm.h"
#include "unisoc_bt_log.h"

#include <misc/wcn_integrate_platform.h>
#include <misc/wcn_bus.h>

#include "tty.h"
#include "rfkill.h"

#define STTY_DEV_MAX_NR		1
#define STTY_MAX_DATA_LEN	4096
#define STTY_STATE_OPEN		1
#define STTY_STATE_CLOSE	0
#define COMMAND_HEAD        1

struct stty_device {
	struct stty_init_data	*pdata;
	struct tty_port		*port;
	struct tty_struct	*tty;
	struct tty_driver	*driver;

	/* stty state */
	uint32_t		state;
	struct mutex		stat_lock;
};

static bool is_user_debug = false;
static bool is_dumped = false;
struct device *ttyBT_dev = NULL;
static struct stty_device *stty_dev;

#if 0
static void stty_address_init(void);
static unsigned long bt_data_addr;
#else
static struct tty_struct *mtty;
#endif

static ssize_t dumpmem_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    if (buf[0] == 2) {
        dev_unisoc_bt_info(ttyBT_dev,
                           "Set is_user_debug true!\n");
        is_user_debug = true;
        return 0;
    }

    if (is_dumped == false) {
        dev_unisoc_bt_info(ttyBT_dev,
                           "mtty BT start dump cp mem !\n");
    } else {
        dev_unisoc_bt_info(ttyBT_dev,
                           "mtty BT has dumped cp mem, pls restart phone!\n");
    }
    is_dumped = true;

    return 0;
}

static void hex_dump(unsigned char *bin, size_t binsz)
{
  char *str, hex_str[]= "0123456789ABCDEF";
  size_t i;

  str = (char *)vmalloc(binsz * 3);
  if (!str) {
    return;
  }

  for (i = 0; i < binsz; i++) {
      str[(i * 3) + 0] = hex_str[(bin[i] >> 4) & 0x0F];
      str[(i * 3) + 1] = hex_str[(bin[i]     ) & 0x0F];
      str[(i * 3) + 2] = ' ';
  }
  str[(binsz * 3) - 1] = 0x00;
  dev_unisoc_bt_info(ttyBT_dev,
                     "%s\n", str);
  vfree(str);
}

static void hex_dump_block(unsigned char *bin, size_t binsz)
{
#define HEX_DUMP_BLOCK_SIZE 8
	int loop = binsz / HEX_DUMP_BLOCK_SIZE;
	int tail = binsz % HEX_DUMP_BLOCK_SIZE;
	int i;

	if (!loop) {
		hex_dump(bin, binsz);
		return;
	}

	for (i = 0; i < loop; i++) {
		hex_dump(bin + i * HEX_DUMP_BLOCK_SIZE, HEX_DUMP_BLOCK_SIZE);
	}

	if (tail)
		hex_dump(bin + i * HEX_DUMP_BLOCK_SIZE, tail);
}

static ssize_t chipid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i = 0, id;

	id = wcn_get_aon_chip_id();
	dev_unisoc_bt_info(ttyBT_dev,
						"%s: %d",
						__func__, id);

	i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", id);
	return i;
}

static DEVICE_ATTR_RO(chipid);
static DEVICE_ATTR_WO(dumpmem);

static struct attribute *bluetooth_attrs[] = {
	&dev_attr_chipid.attr,
	&dev_attr_dumpmem.attr,
	NULL,
};

static struct attribute_group bluetooth_group = {
	.name = NULL,
	.attrs = bluetooth_attrs,
};


static void stty_handler (int event, void *data)
{
	struct stty_device *stty = data;
	int i, cnt = 0, ret = -1, retry_count = 10;
	unsigned char *buf;

	buf = kzalloc(STTY_MAX_DATA_LEN, GFP_KERNEL);
	if (!buf) {
		return;
	}

	dev_unisoc_bt_dbg(ttyBT_dev,
						"stty handler event=%d\n", event);

	switch (event) {
	case SBUF_NOTIFY_WRITE:
		break;
	case SBUF_NOTIFY_READ:
		do {
			cnt = sbuf_read(stty->pdata->dst,
					stty->pdata->channel,
					stty->pdata->rx_bufid,
					(void *)buf,
					STTY_MAX_DATA_LEN,
					0);
			dev_unisoc_bt_dbg(ttyBT_dev,
								"%s read data len =%d\n",__func__, cnt);
			mutex_lock(&(stty->stat_lock));
			if ((stty->state == STTY_STATE_OPEN) && (cnt > 0)) {
				for (i = 0; i < cnt; i++) {
					ret = tty_insert_flip_char(stty->port,
								buf[i],
								TTY_NORMAL);
					while((ret != 1) && retry_count--) {
						msleep(2);
						dev_unisoc_bt_info(ttyBT_dev,
											"stty insert data fail ret =%d, retry_count = %d\n",
											ret, 10 - retry_count);
						ret = tty_insert_flip_char(stty->port,
									buf[i],
									TTY_NORMAL);
					}
					if(retry_count != 10)
						retry_count = 10;
				}
				tty_schedule_flip(stty->port);
			}
			mutex_unlock(&(stty->stat_lock));
		} while(cnt == STTY_MAX_DATA_LEN);
		break;
	default:
		dev_unisoc_bt_info(ttyBT_dev,
							"Received event is invalid(event=%d)\n", event);
	}

	kfree(buf);
}

static int stty_open(struct tty_struct *tty, struct file *filp)
{
	struct stty_device *stty = NULL;
	struct tty_driver *driver = NULL;
	int ret = -1;

	dev_unisoc_bt_info(ttyBT_dev,
						"stty_open\n");

	if (tty == NULL) {
		dev_unisoc_bt_err(ttyBT_dev,
							"stty open input tty is NULL!\n");
		return -ENOMEM;
	}
	driver = tty->driver;
	stty = (struct stty_device *)driver->driver_state;

	if (stty == NULL) {
		dev_unisoc_bt_err(ttyBT_dev,
							"stty open input stty NULL!\n");
		return -ENOMEM;
	}

	if (sbuf_status(stty->pdata->dst, stty->pdata->channel) != 0) {
		dev_unisoc_bt_err(ttyBT_dev,
							"stty_open sbuf not ready to open!dst=%d,channel=%d\n",
							stty->pdata->dst, stty->pdata->channel);
		return -ENODEV;
	}
	stty->tty = tty;
	tty->driver_data = (void *)stty;

	mtty = tty;

	mutex_lock(&(stty->stat_lock));
	stty->state = STTY_STATE_OPEN;
	mutex_unlock(&(stty->stat_lock));

#ifdef CONFIG_ARCH_SCX20
	rf2351_vddwpa_ctrl_power_enable(1);
#endif

	dev_unisoc_bt_info(ttyBT_dev,
						"stty_open device success!\n");
	sitm_ini();
	ret = start_marlin(WCN_MARLIN_BLUETOOTH);
	dev_unisoc_bt_info(ttyBT_dev,
						"stty_open power on state ret = %d!\n",
						ret);
#if 0
	stty_address_init();
#endif
	return 0;
}

static void stty_close(struct tty_struct *tty, struct file *filp)
{
	struct stty_device *stty = NULL;
	int ret = -1;

	dev_unisoc_bt_info(ttyBT_dev,
						"stty_close\n");

	if (tty == NULL) {
		dev_unisoc_bt_err(ttyBT_dev,
							"stty close input tty is NULL!\n");
		return;
	}
	stty = (struct stty_device *) tty->driver_data;
	if (stty == NULL) {
		dev_unisoc_bt_err(ttyBT_dev,
							"stty close s tty is NULL!\n");
		return;
	}

	mutex_lock(&(stty->stat_lock));
	stty->state = STTY_STATE_CLOSE;
	mutex_unlock(&(stty->stat_lock));

	dev_unisoc_bt_info(ttyBT_dev,
						"stty_close device success !\n");
	sitm_cleanup();
	ret = stop_marlin(WCN_MARLIN_BLUETOOTH);
	dev_unisoc_bt_info(ttyBT_dev,
						"stty_close power off state ret = %d!\n",
						ret);
#ifdef CONFIG_ARCH_SCX20
	rf2351_vddwpa_ctrl_power_enable(0);
#endif

}

#if 0
static void stty_address_init(void)
{
	static unsigned long bt_aon_addr;
	static unsigned long bt_aon_enable_addr;
	static unsigned long aon_enable_data;
	bt_aon_addr = (unsigned long)ioremap_nocache(0x40440FF8,0x10);
	bt_data_addr = (unsigned long)ioremap_nocache(0x40440FFC,0x10);
	bt_aon_enable_addr = (unsigned long)ioremap_nocache(0x402e00b0,0x10);
	aon_enable_data = __raw_readw((void __iomem *)(bt_aon_enable_addr));
	aon_enable_data = aon_enable_data | 0x0800; //bit11 need write 1
	__raw_writel(aon_enable_data, (void __iomem *)(bt_aon_enable_addr));
	__raw_writel(0x51004000, (void __iomem *)(bt_aon_addr));
}

static int stty_write(struct tty_struct *tty,
		      const unsigned char *buf,
		      int count)
{
    int loop_count = count / 4;
    int loop_count_i = 0;
    unsigned long bt_data2send = 0;
    unsigned char *tx_buf = kmalloc(count, GFP_KERNEL);
    unsigned char *p_buf = tx_buf;
    memset(tx_buf, 0, count);
    memcpy(tx_buf, buf, count);

	dev_unisoc_bt_dbg(ttyBT_dev,
						"stty write buf length = %d\n",count);

    for(loop_count_i = 0; loop_count_i < loop_count; loop_count_i++)
    {
        bt_data2send = (*p_buf) + ((*(p_buf + 1)) << 8) + ((*(p_buf + 2)) << 16) + ((*(p_buf + 3)) << 24);
        __raw_writel(bt_data2send, (void __iomem *)(bt_data_addr));

        p_buf += 4;
    }
    kfree(tx_buf);
    tx_buf = NULL;

    return count;
}
#else
static int stty_write(struct tty_struct *tty,
		      const unsigned char *buf,
		      int count)
{
	struct stty_device *stty = tty->driver_data;
	int write_length = 0, left_legnth = 0;
	if(COMMAND_HEAD == buf[0]){
		dev_unisoc_bt_info(ttyBT_dev,
							"%s bufwrite_length = %d\n",
							__func__, count);
		if(count <= 16){
			hex_dump_block((unsigned char *)buf, count);
		}
		else{
			hex_dump_block((unsigned char*)buf, 8);
			hex_dump_block((unsigned char*)(buf+count-8), 8);
		}
	}
	left_legnth = count;
	while(left_legnth > 0) {
		write_length = sbuf_write(stty->pdata->dst,
				 stty->pdata->channel,
				 stty->pdata->tx_bufid,
				 (void *)(buf + count - left_legnth), count, -1);
		dev_unisoc_bt_dbg(ttyBT_dev,
							"stty write bufwrite_length = %d, left_legnth = %d\n",
							write_length, left_legnth);
		left_legnth = left_legnth - write_length;
	}
	return left_legnth;
}
#endif

static int stty_data_transmit(uint8_t *data, size_t count)
{
#if 0
	return stty_write(NULL, data, count);
#else
	dev_unisoc_bt_dbg(ttyBT_dev,
						"stty_data_transmit\n");
	return stty_write(mtty, data, count);
#endif
}

static int stty_write_plus(struct tty_struct *tty,
	      const unsigned char *buf, int count)
{
	dev_unisoc_bt_dbg(ttyBT_dev,
						"stty_write_plus\n");
	return sitm_write(buf, count, stty_data_transmit);
}

static void stty_flush_chars(struct tty_struct *tty)
{
}

static int stty_write_room(struct tty_struct *tty)
{
	return INT_MAX;
}

static const struct tty_operations stty_ops = {
	.open  = stty_open,
	.close = stty_close,
	.write = stty_write_plus,
	.flush_chars = stty_flush_chars,
	.write_room  = stty_write_room,
};

static struct tty_port *stty_port_init(void)
{
	struct tty_port *port = NULL;

	port = kzalloc(sizeof(struct tty_port), GFP_KERNEL);
	if (port == NULL)
		return NULL;
	tty_port_init(port);
	return port;
}

static int stty_driver_init(struct stty_device *device)
{
	struct tty_driver *driver;
	int ret = 0;

	mutex_init(&(device->stat_lock));

	device->port = stty_port_init();
	if (!device->port)
		return -ENOMEM;

	driver = alloc_tty_driver(STTY_DEV_MAX_NR);
	if (!driver) {
		kfree(device->port);
		return -ENOMEM;
	}

	/*
	 * Initialize the tty_driver structure
	 * Entries in stty_driver that are NOT initialized:
	 * proc_entry, set_termios, flush_buffer, set_ldisc, write_proc
	 */
	driver->owner = THIS_MODULE;
	driver->driver_name = device->pdata->name;
	driver->name = device->pdata->name;
	driver->major = 0;
	driver->type = TTY_DRIVER_TYPE_SYSTEM;
	driver->subtype = SYSTEM_TYPE_TTY;
	driver->init_termios = tty_std_termios;
	driver->driver_state = (void *)device;
	device->driver = driver;
	 /* initialize the tty driver */
	tty_set_operations(driver, &stty_ops);
	tty_port_link_device(device->port, driver, 0);
	ret = tty_register_driver(driver);
	if (ret) {
		put_tty_driver(driver);
		tty_port_destroy(device->port);
		kfree(device->port);
		return ret;
	}
	return ret;
}

static void stty_driver_exit(struct stty_device *device)
{
	struct tty_driver *driver = device->driver;

	tty_unregister_driver(driver);
	put_tty_driver(driver);
	tty_port_destroy(device->port);
}

static int stty_parse_dt(struct stty_init_data **init, struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct stty_init_data *pdata = NULL;
	int ret;

	pdata = devm_kzalloc(dev, sizeof(struct stty_init_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = of_property_read_string(np,
				      "sprd,name",
				      (const char **)&pdata->name);
	if (ret) {
		goto error;
	}
	/*sprd,dst*/
	pdata->dst = SPRD_BT_DST;
	/*sprd,channel*/
	pdata->channel = SPRD_BT_CHANNEL;
	/*sprd,tx_bufid*/
	pdata->tx_bufid = SPRD_BT_TX_BUFID;
	/*sprd,rx_bufid*/
	pdata->rx_bufid = SPRD_BT_RX_BUFID;
	/*Send the parsed data back*/
	*init = pdata;
	return 0;
error:
	devm_kfree(dev, pdata);
	*init = NULL;
	return ret;
}

static inline void stty_destroy_pdata(struct stty_init_data **init,
	struct device *dev)
{
	struct stty_init_data *pdata = *init;

	devm_kfree(dev, pdata);

	*init = NULL;
}

static int stty_bluetooth_reset(struct notifier_block *this, unsigned long ev, void *ptr)
{
#define RESET_BUFSIZE 5

    int ret = 0;
    unsigned char reset_buf[RESET_BUFSIZE]= {0x04, 0xff, 0x02, 0x57, 0xa5};
    int i = 0, retry_count = 10;

    dev_unisoc_bt_info(ttyBT_dev,
                                "%s:reset callback coming\n", __func__);
    if (stty_dev != NULL) {
        dev_unisoc_bt_info(ttyBT_dev,
                                    "%s tty_insert_flip_string\n", __func__);
        mutex_lock(&(stty_dev->stat_lock));
        if ((stty_dev->state == STTY_STATE_OPEN) && (RESET_BUFSIZE > 0)) {
            for (i = 0; i < RESET_BUFSIZE; i++) {
                ret = tty_insert_flip_char(stty_dev->port,
                        reset_buf[i],
                        TTY_NORMAL);
                while((ret != 1) && retry_count--) {
                    msleep(2);
                    dev_unisoc_bt_info(ttyBT_dev,
                                "stty_dev insert data fail ret =%d, retry_count = %d\n",
                                ret, 10 - retry_count);
                    ret = tty_insert_flip_char(stty_dev->port,
                                reset_buf[i],
                                TTY_NORMAL);
                }
                if(retry_count != 10)
                    retry_count = 10;
            }
            tty_schedule_flip(stty_dev->port);
        }
        mutex_unlock(&(stty_dev->stat_lock));
    }
    return NOTIFY_DONE;
}


static struct notifier_block bluetooth_reset_block = {
    .notifier_call = stty_bluetooth_reset,
};

static int  stty_probe(struct platform_device *pdev)
{
	struct stty_init_data *pdata = (struct stty_init_data *)pdev->
					dev.platform_data;
	struct stty_device *stty;
	int rval = 0;

	if (pdev->dev.of_node && !pdata) {
		rval = stty_parse_dt(&pdata, &pdev->dev);
		if (rval) {
			dev_unisoc_bt_err(ttyBT_dev,
								"failed to parse styy device tree, ret=%d\n",
								rval);
			return rval;
		}
	}
	dev_unisoc_bt_info(ttyBT_dev,
						"stty: after parse device tree, name=%s, dst=%u, channel=%u, tx_bufid=%u, rx_bufid=%u\n",
						pdata->name, pdata->dst, pdata->channel, pdata->tx_bufid, pdata->rx_bufid);

	stty = devm_kzalloc(&pdev->dev, sizeof(struct stty_device), GFP_KERNEL);
    ttyBT_dev = &pdev->dev;
	if (stty == NULL) {
		stty_destroy_pdata(&pdata, &pdev->dev);
		dev_unisoc_bt_err(ttyBT_dev,
							"stty Failed to allocate device!\n");
		return -ENOMEM;
	}

	stty->pdata = pdata;
	rval = stty_driver_init(stty);
	if (rval) {
		devm_kfree(&pdev->dev, stty);
		stty_destroy_pdata(&pdata, &pdev->dev);
		dev_unisoc_bt_err(ttyBT_dev,
							"stty driver init error!\n");
		return -EINVAL;
	}

	rval = sbuf_register_notifier(pdata->dst, pdata->channel,
					pdata->rx_bufid, stty_handler, stty);
	if (rval) {
		stty_driver_exit(stty);
		kfree(stty->port);
		devm_kfree(&pdev->dev, stty);
		dev_unisoc_bt_err(ttyBT_dev,
							"regitster notifier failed (%d)\n",
							rval);
		return rval;
	}

	dev_unisoc_bt_info(ttyBT_dev,
						"stty_probe init device addr: 0x%p\n",
						stty);
	platform_set_drvdata(pdev, stty);

	if (sysfs_create_group(&pdev->dev.kobj,
			&bluetooth_group)) {
		dev_unisoc_bt_err(ttyBT_dev,
							"%s failed to create bluetooth tty attributes.\n",
							__func__);
	}

    rfkill_bluetooth_init(pdev);
    stty_dev = stty;
    atomic_notifier_chain_register(&wcn_reset_notifier_list, &bluetooth_reset_block);
    return 0;
}

static int  stty_remove(struct platform_device *pdev)
{
	struct stty_device *stty = platform_get_drvdata(pdev);
	int rval;

	rval = sbuf_register_notifier(stty->pdata->dst, stty->pdata->channel,
					stty->pdata->rx_bufid, NULL, NULL);
	if (rval) {
		dev_unisoc_bt_err(ttyBT_dev,
							"unregitster notifier failed (%d)\n",
							rval);
		return rval;
	}

	stty_driver_exit(stty);
	kfree(stty->port);
	stty_destroy_pdata(&stty->pdata, &pdev->dev);
	devm_kfree(&pdev->dev, stty);
	platform_set_drvdata(pdev, NULL);
	sysfs_remove_group(&pdev->dev.kobj, &bluetooth_group);
	return 0;
}

static const struct of_device_id stty_match_table[] = {
	{ .compatible = "sprd,wcn_bt", },
	{ },
};

static struct platform_driver stty_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "ttyBT",
		.of_match_table = stty_match_table,
	},
	.probe = stty_probe,
	.remove = stty_remove,
};

static int __init stty_init(void)
{
	return platform_driver_register(&stty_driver);
}

static void __exit stty_exit(void)
{
	platform_driver_unregister(&stty_driver);
}

late_initcall(stty_init);
module_exit(stty_exit);

MODULE_AUTHOR("Spreadtrum Bluetooth");
MODULE_DESCRIPTION("SIPC/stty driver");
MODULE_LICENSE("GPL");
