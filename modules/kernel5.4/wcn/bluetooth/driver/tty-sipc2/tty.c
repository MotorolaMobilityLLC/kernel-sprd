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
#include <linux/notifier.h>
#include "alignment/sitm.h"
#include "unisoc_bt_log.h"

#include <misc/wcn_integrate_platform.h>
#include <misc/marlin_platform.h>
#include <misc/wcn_bus.h>

#include "lpm.h"

#include "tty.h"
#include "rfkill.h"

static unsigned int log_level = MTTY_LOG_LEVEL_VER;

#define STTY_DEV_MAX_NR     1
#define STTY_MAX_DATA_LEN   4096
#define STTY_STATE_OPEN     1
#define STTY_STATE_CLOSE    0
#define COMMAND_HEAD        1
#define ISO_HEAD            5

#define DOWN_ACQUIRE_TIMEOUT_MS 20

static struct semaphore sem_id;

struct rx_data {
	unsigned int channel;
	struct mbuf_t *head;
	struct mbuf_t *tail;
	unsigned int num;
	struct list_head entry;
};

struct stty_device {
	struct stty_init_data	*pdata;
	struct tty_port		*port;
	struct tty_struct	*tty;
	struct tty_driver	*driver;

	struct platform_device *pdev;

	/* stty state */
	//uint32_t		stty_state;
	struct mutex		stat_lock;

	/* mtty state */
	atomic_t state;
	/*spinlock_t    rw_lock;*/
	struct mutex    rw_mutex;
	struct list_head rx_head;
	/*struct tasklet_struct rx_task;*/
	struct work_struct bt_rx_work;
	struct workqueue_struct *bt_rx_workqueue;

};

struct stty_device *stty_dev;

extern int set_power_ret;

//static struct stty_device *stty_dev;
struct mchn_ops_t bt_sipc_rx_ops;
struct mchn_ops_t bt_sipc_tx_ops;

static unsigned int que_task = 1;
static int que_sche = 1;

//static int wcn_hw_type = 0;

static bool is_user_debug = false;
static bool is_dumped = false;
struct device *ttyBT_dev = NULL;

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

static ssize_t chipid_show(struct device *dev,
       struct device_attribute *attr, char *buf)
{
    int i = 0, id;
    const char *id_str = NULL;

    id = wcn_get_aon_chip_id();
    id_str = wcn_get_chip_name();
    dev_unisoc_bt_info(ttyBT_dev,
                       "%s: chipid: %d, chipid_str: %s",
                       __func__, id, id_str);

    i = scnprintf(buf, PAGE_SIZE, "%d/", id);
    dev_unisoc_bt_info(ttyBT_dev,
                       "%s: buf: %s, i = %d",
                       __func__, buf, i);
    strncat(buf, id_str, 32);
    i += scnprintf(buf + i, PAGE_SIZE - i, "%s", buf + i);
    dev_unisoc_bt_info(ttyBT_dev,
                       "%s: buf: %s, i = %d",
                       __func__, buf, i);
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

/* static void mtty_rx_task(unsigned long data) */
static void stty_rx_work_queue(struct work_struct *work)
{
	int i, ret = 0;
	/*struct stty_device *mtty = (struct stty_device *)data;*/
	struct stty_device *stty;
	struct rx_data *rx = NULL;

	que_task = que_task + 1;
	if (que_task > 65530)
		que_task = 0;
	dev_unisoc_bt_info(ttyBT_dev,
						"mtty que_task= %d\n",
						que_task);
	que_sche = que_sche - 1;

	stty = container_of(work, struct stty_device, bt_rx_work);
	if (unlikely(!stty)) {
		dev_unisoc_bt_err(ttyBT_dev,
							"mtty_rx_task mtty is NULL\n");
		return;
	}
	if (atomic_read(&stty->state) == STTY_STATE_OPEN) {
		do {
			mutex_lock(&stty->rw_mutex);
			if (list_empty_careful(&stty->rx_head)) {
				dev_unisoc_bt_err(ttyBT_dev,
									"mtty over load queue done\n");
				mutex_unlock(&stty->rw_mutex);
				break;
			}
			rx = list_first_entry_or_null(&stty->rx_head,
											struct rx_data, entry);
			if (!rx) {
				dev_unisoc_bt_err(ttyBT_dev,
									"mtty over load queue abort\n");
				mutex_unlock(&stty->rw_mutex);
				break;
			}
			list_del(&rx->entry);
			mutex_unlock(&stty->rw_mutex);

			dev_unisoc_bt_err(ttyBT_dev,
								"mtty over load working at channel: %d, len: %d\n",
								rx->channel, rx->head->len);
			for (i = 0; i < rx->head->len; i++) {
				ret = tty_insert_flip_char(mtty->port,
											*(rx->head->buf+i), TTY_NORMAL);
				if (ret != 1) {
					i--;
					continue;
				} else {
					tty_flip_buffer_push(mtty->port);
				}
			}
			dev_unisoc_bt_err(ttyBT_dev,
								"mtty over load cut channel: %d\n",
								rx->channel);
			kfree(rx->head->buf);
			kfree(rx);

		} while (1);
	} else {
		dev_unisoc_bt_info(ttyBT_dev,
							"mtty status isn't open, status:%d\n",
							atomic_read(&stty->state));
		mutex_unlock(&stty->stat_lock);
	}
}

static int mtty_sipc_rx_cb(int chn, struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	int ret = 0, len_send;
	struct rx_data *rx;
	len_send = head->len;

	bt_wakeup_host();
	if (atomic_read(&stty_dev->state) == STTY_STATE_CLOSE) {
		dev_unisoc_bt_err(ttyBT_dev,
							"%s mtty bt is closed abnormally\n",
							__func__);
		sprdwcn_bus_push_list(chn, head, tail, num);
		return -1;
	}

	if (stty_dev != NULL) {
		if (!work_pending(&stty_dev->bt_rx_work)) {
			dev_unisoc_bt_dbg(ttyBT_dev,
								"%s tty_insert_flip_string",
								__func__);
			ret = tty_insert_flip_string(stty_dev->port,
					(unsigned char *)head->buf + BT_SIPC_HEAD_LEN,
					len_send);   // -BT_SDIO_HEAD_LEN
			dev_unisoc_bt_dbg(ttyBT_dev,
								"%s ret: %d, len: %d\n",
								__func__, ret, len_send);
			if (ret)
				tty_flip_buffer_push(stty_dev->port);
			if (ret == (len_send)) {
				dev_unisoc_bt_dbg(ttyBT_dev,
									"%s send success",
									__func__);
				sprdwcn_bus_push_list(chn, head, tail, num);
				return 0;
			}
		}

		rx = kmalloc(sizeof(struct rx_data), GFP_KERNEL);
		if (rx == NULL) {
			dev_unisoc_bt_err(ttyBT_dev,
								"%s rx == NULL\n",
								__func__);
			sprdwcn_bus_push_list(chn, head, tail, num);
			return -ENOMEM;
		}

		rx->head = head;
		rx->tail = tail;
		rx->channel = chn;
		rx->num = num;
		rx->head->len = (len_send) - ret;
		rx->head->buf = kmalloc(rx->head->len, GFP_KERNEL);
		if (rx->head->buf == NULL) {
			dev_unisoc_bt_err(ttyBT_dev,
								"mtty low memory!\n");
			kfree(rx);
			sprdwcn_bus_push_list(chn, head, tail, num);
			return -ENOMEM;
		}

		memcpy(rx->head->buf, (unsigned char *)head->buf + BT_SIPC_HEAD_LEN + ret, rx->head->len);
		sprdwcn_bus_push_list(chn, head, tail, num);
		mutex_lock(&stty_dev->rw_mutex);
		dev_unisoc_bt_err(ttyBT_dev,
							"mtty over load push %d -> %d, channel: %d len: %d\n",
							len_send, ret, rx->channel, rx->head->len);
		list_add_tail(&rx->entry, &stty_dev->rx_head);
		mutex_unlock(&stty_dev->rw_mutex);
		if (!work_pending(&stty_dev->bt_rx_work)) {
		dev_unisoc_bt_err(ttyBT_dev,
							"work_pending\n");
		queue_work(stty_dev->bt_rx_workqueue,
					&stty_dev->bt_rx_work);
		}
		return 0;
	}
	dev_unisoc_bt_err(ttyBT_dev,
						"mtty_rx_cb stty_dev is NULL!!!\n");

	return -1;
}

static int mtty_sipc_tx_cb(int chn, struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	int i;
	struct mbuf_t *pos = NULL;
	dev_unisoc_bt_dbg(ttyBT_dev,
						"%s channel: %d, head: %p, tail: %p num: %d\n",
						__func__, chn, head, tail, num);
	pos = head;
	for (i = 0; i < num; i++, pos = pos->next) {
		kfree(pos->buf);
		pos->buf = NULL;
	}
	if ((sprdwcn_bus_list_free(chn, head, tail, num)) == 0)
	{
		up(&sem_id);
	}
	else
		dev_unisoc_bt_err(ttyBT_dev,"%s sprdwcn_bus_list_free() fail\n", __func__);

	return 0;
}

static int stty_open(struct tty_struct *tty, struct file *filp)
{
	struct stty_device *stty = NULL;
	struct tty_driver *driver = NULL;
	int ret = -1;

	dev_unisoc_bt_info(ttyBT_dev,"stty_open\n");

	if (tty == NULL) {
		dev_unisoc_bt_err(ttyBT_dev,"stty open input tty is NULL!\n");
		return -ENOMEM;
	}
	driver = tty->driver;
	stty = (struct stty_device *)driver->driver_state;

	if (stty == NULL) {
		dev_unisoc_bt_err(ttyBT_dev,
							"stty open input stty NULL!\n");
		return -ENOMEM;
	}

	dev_unisoc_bt_err(ttyBT_dev,"dst:%d, channel:%d\n",stty->pdata->dst,stty->pdata->channel);
	stty->tty = tty;
	tty->driver_data = (void *)stty;
	mtty = tty;
	atomic_set(&stty->state, STTY_STATE_OPEN);

#ifdef CONFIG_ARCH_SCX20
	rf2351_vddwpa_ctrl_power_enable(1);
#endif

	dev_unisoc_bt_info(ttyBT_dev,"stty_open device success!\n");
	sitm_ini();
#if 0
	stty_address_init();
#endif
	ret = start_marlin(MARLIN_BLUETOOTH);
	dev_unisoc_bt_info(ttyBT_dev,
						"mtty_open power on state ret = %d!\n",
						ret);
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

	dev_unisoc_bt_info(ttyBT_dev,
						"stty_close device success !\n");

	sitm_cleanup();
#ifdef CONFIG_ARCH_SCX20
	rf2351_vddwpa_ctrl_power_enable(0);
#endif
	ret = stop_marlin(MARLIN_BLUETOOTH);
	dev_unisoc_bt_info(ttyBT_dev,
						"mtty_close power off state ret = %d!\n",
						ret);
}

static int mtty_sipc_write(struct tty_struct *tty,
			const unsigned char *buf, int count)
{
	int num = 1, ret;
	struct mbuf_t *tx_head = NULL, *tx_tail = NULL;
	unsigned char *block = NULL;

	if (log_level == MTTY_LOG_LEVEL_VER) {
		if (buf[0] == COMMAND_HEAD) {
			dev_unisoc_bt_err(ttyBT_dev,
								"%s dump cmd %02X %02X %02X %02X \n",
								__func__, buf[0], buf[1], buf[2],buf[3]);
		}
	}

	block = kmalloc(count + BT_SIPC_HEAD_LEN, GFP_KERNEL);

	if (!block) {
		dev_unisoc_bt_err(ttyBT_dev,
							"%s kmalloc failed\n",
							__func__);
		return -ENOMEM;
	}
	memset(block, 0, count + BT_SIPC_HEAD_LEN);
	memcpy(block + BT_SIPC_HEAD_LEN, buf, count);
	ret = down_timeout(&sem_id,msecs_to_jiffies(DOWN_ACQUIRE_TIMEOUT_MS));
	if (ret) {
		dev_unisoc_bt_err(ttyBT_dev,"%s acquire sem fail",__func__);
		kfree(block);
		block = NULL;
		return ret;
	}
	ret = sprdwcn_bus_list_alloc(BT_SIPC_TX_CHANNEL, &tx_head, &tx_tail, &num);
	if (ret) {
		dev_unisoc_bt_err(ttyBT_dev,
							"%s sprdwcn_bus_list_alloc failed: %d\n",
							__func__, ret);
		up(&sem_id);
		kfree(block);
		block = NULL;
		return -ENOMEM;
	}

	if (block[BT_SIPC_HEAD_LEN] == ISO_HEAD && (block[BT_SIPC_HEAD_LEN + 4]&0xC0)) {
			block[BT_SIPC_HEAD_LEN + 4] &= 0x3f;
			dev_unisoc_bt_err(ttyBT_dev,
								"%s dump ISO %02X %02X %02X %02X \n",
								__func__, block[0], block[1], block[2],block[3]);
	}
	tx_head->buf = block;
	tx_head->len = count;
	tx_head->next = NULL;

	ret = sprdwcn_bus_push_list(BT_SIPC_TX_CHANNEL, tx_head, tx_tail, num);
	if (ret) {
		dev_unisoc_bt_err(ttyBT_dev,
							"%s sprdwcn_bus_push_list failed: %d\n",
							__func__, ret);
		kfree(tx_head->buf);
		tx_head->buf = NULL;
		sprdwcn_bus_list_free(BT_SIPC_TX_CHANNEL, tx_head, tx_tail, num);
		up(&sem_id);
		return -EBUSY;
	}

	return count;
}

static int mtty_write(struct tty_struct *tty,
			const unsigned char *buf, int count)
{
	int mcount;
	mcount = mtty_sipc_write(tty,buf,count);
	return mcount;
}

static int stty_data_transmit(uint8_t *data, size_t count)
{
	return mtty_write(NULL, data, count);
}

static int stty_write_plus(struct tty_struct *tty,
			const unsigned char *buf, int count)
{
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
	uint32_t data;

	pdata = devm_kzalloc(dev, sizeof(struct stty_init_data), GFP_KERNEL);
	if (!pdata){
		dev_unisoc_bt_err(ttyBT_dev,"dubugw pdata!\n");
		return -ENOMEM;
	}
	ret = of_property_read_string(np,
									"sprd,name",
									(const char **)&pdata->name);
	if (ret){
		dev_unisoc_bt_err(ttyBT_dev,"dubugw name!\n");
		goto error;
	}

	ret = of_property_read_u32(np, "sprd,dst", (uint32_t *)&data);
	if (ret){
		dev_unisoc_bt_err(ttyBT_dev,"dubugw dst!\n");
		goto error;
	}
	pdata->dst = (uint8_t)data;

	ret = of_property_read_u32(np, "sprd,channel", (uint32_t *)&data);
	if (ret){
		dev_unisoc_bt_err(ttyBT_dev,"dubugw channel!\n");
		goto error;
	}
	pdata->channel = (uint8_t)data;

	ret = of_property_read_u32(np, "sprd,tx_bufid", (uint32_t *)&pdata->tx_bufid);
	if (ret){
		dev_unisoc_bt_err(ttyBT_dev,"dubugw tx_bufid!\n");
		goto error;
	}

	ret = of_property_read_u32(np, "sprd,rx_bufid", (uint32_t *)&pdata->rx_bufid);
	if (ret){
		dev_unisoc_bt_err(ttyBT_dev,"dubugw rx_bufid!\n");
		goto error;
	}

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

struct mchn_ops_t bt_sipc_rx_ops = {
	.channel = BT_SIPC_RX_CHANNEL,
	.hif_type = HW_TYPE_SIPC,
	.inout = BT_RX_INOUT,
	.pool_size = BT_RX_POOL_SIZE,
	.pop_link = mtty_sipc_rx_cb,
};

struct mchn_ops_t bt_sipc_tx_ops = {
	.channel = BT_SIPC_TX_CHANNEL,
	.hif_type = HW_TYPE_SIPC,
	.inout = BT_TX_INOUT,
	.pool_size = BT_TX_POOL_SIZE,
	.pop_link = mtty_sipc_tx_cb,
};

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
		if ((atomic_read(&stty_dev->state) == STTY_STATE_OPEN) && (RESET_BUFSIZE > 0)) {
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

	stty = kzalloc(sizeof(struct stty_device), GFP_KERNEL);
	ttyBT_dev = &pdev->dev;
	if (stty == NULL) {
		stty_destroy_pdata(&pdata, &pdev->dev);
		pr_err("stty Failed to allocate device!\n");
		return -ENOMEM;
	}
	memset(stty, 0 ,sizeof(struct stty_device));

	stty->pdata = pdata;
	rval = stty_driver_init(stty);
	if (rval) {
		devm_kfree(&pdev->dev, stty);
		stty_destroy_pdata(&pdata, &pdev->dev);
		dev_unisoc_bt_err(ttyBT_dev,
							"stty driver init error!\n");
		return -EINVAL;
	}

	dev_unisoc_bt_info(ttyBT_dev,
						"stty_probe init device addr: 0x%p\n",
						stty);
	platform_set_drvdata(pdev, stty);

	stty_dev = stty;

	atomic_set(&stty->state, STTY_STATE_CLOSE);
	sema_init(&sem_id, BT_TX_POOL_SIZE - 1);
	mutex_init(&stty->rw_mutex);
	INIT_LIST_HEAD(&stty->rx_head);

	stty->bt_rx_workqueue = create_singlethread_workqueue("SPRDBT_RX_QUEUE");
	if (!stty->bt_rx_workqueue) {
		stty_driver_exit(stty);
		kfree(stty->port);
		kfree(stty);
		stty_destroy_pdata(&pdata, &pdev->dev);
		dev_unisoc_bt_err(ttyBT_dev,
							"%s SPRDBT_RX_QUEUE create failed",
							__func__);
		return -ENOMEM;
	}
	INIT_WORK(&stty->bt_rx_work, stty_rx_work_queue);

	if (sysfs_create_group(&pdev->dev.kobj,
			&bluetooth_group)) {
		dev_unisoc_bt_err(ttyBT_dev,
							"%s failed to create bluetooth tty attributes.\n",
							__func__);
	}

	rfkill_bluetooth_init(pdev);
	bluesleep_init();
	atomic_notifier_chain_register(&wcn_reset_notifier_list, &bluetooth_reset_block);
	sprdwcn_bus_chn_init(&bt_sipc_rx_ops);
	sprdwcn_bus_chn_init(&bt_sipc_tx_ops);

	dev_unisoc_bt_info(ttyBT_dev,"mtty_probe successful!\n");

	return 0;
}

static int stty_remove(struct platform_device *pdev)
{
	struct stty_device *stty = platform_get_drvdata(pdev);

	stty_driver_exit(stty);
	sprdwcn_bus_chn_deinit(&bt_sipc_rx_ops);
	sprdwcn_bus_chn_deinit(&bt_sipc_tx_ops);
	kfree(stty->port);
	stty_destroy_pdata(&stty->pdata, &pdev->dev);
	flush_workqueue(stty_dev->bt_rx_workqueue);
	destroy_workqueue(stty_dev->bt_rx_workqueue);
	devm_kfree(&pdev->dev, stty);
	platform_set_drvdata(pdev, NULL);
	sysfs_remove_group(&pdev->dev.kobj, &bluetooth_group);
	return 0;
}

static const struct of_device_id stty_match_table[] = {
	{ .compatible = "sprd,wcn_internal_chip", },
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
