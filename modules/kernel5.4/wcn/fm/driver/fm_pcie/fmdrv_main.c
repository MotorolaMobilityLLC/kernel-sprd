/*
 * FM Radio  driver  with SPREADTRUM SC2331FM Radio chip
 *
 * Copyright (c) 2015 Spreadtrum
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define DRIVER_AUTHOR "Songhe Wei<songhe.wei@spreadtrum.com>"
#define DRIVER_DESC "SDIO radio driver for marlin3 FM Radio Receivers,"\
"share SDIO with BT/WIFI"

#include <linux/miscdevice.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/err.h>
#include <linux/errno.h>
#include  <linux/module.h>
#include  <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/version.h>
#include "fmdrv.h"
/*#include <soc/sprd/sdio_dev.h>*/
/*#include <misc/mdbg_sdio.h>*/
#include "fmdrv_ops.h"
#include "fmdrv_main.h"
#include "fmdrv_rds_parser.h"
#include "fm_rf_marlin3.h"

#include <misc/marlin_platform.h>
//#include "wakelock.h"

#include <misc/wcn_bus.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>

/*#define RDS_DEBUG*/

#ifdef CONFIG_OF
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#define HCI_GRP_VENDOR_SPECIFIC		0x3F
#define FM_SPRD_OP_CODE			0x008C
#define hci_opcode_pack(ogf, ocf) \
	((unsigned short) ((ocf & 0x03ff) | (ogf << 10)))
#define HCI_EV_CMD_COMPLETE		0x0e
#define HCI_VS_EVENT			0xFF

#define FM_DUMP_DATA

#define seekformat "rssi_th =%d,snr_th =%d,freq_offset_th =%d,"\
                   "pilot_power_th= %d,noise_power_th=%d"
#define audioformat "hbound=%d,lbound =%d,power_th =%d,"\
                    "phyt= %d,snr_th=%d"
#define timeoutformat "(fmdrv) %s(): Timeout(%d sec),didn't get fm SubCmd"\
			"0x%02X completion signal from RX tasklet"
#if 0
#define copy_from_user(x,y,z) (memcpy((x),(y),(z)),0)
#define copy_to_user(x,y,z) (memcpy((x),(y),(z)),0)
#endif
bool read_flag;
struct fmdrv_ops *fmdev;
static struct fm_rds_data *g_rds_data_string;
extern struct platform_device *g_fm_pdev;
#ifdef RDS_DEBUG
unsigned global_freq = 8750;
struct fm_rds_data rds_debug_data;
#endif

/**************************for test*************************************/
#ifdef FM_TEST
#define RX_NUM 100
static unsigned char *buf_addr;
static char a[RX_NUM] = {1, 2, 3, 4, 5};
static unsigned char r1[11] = {0x04, 0x0e, 0x08, 0x01, 0x8c, 0xfc, 0x00, 0xa1, 0x23, 0x12, 0x2A};
static unsigned char r2[9] = {0x04, 0xFF, 0x6, 0x30, 0x00, 0x12, 0x13, 0xb4, 0x23};
static unsigned int (*rx_cb)(void *addr, unsigned int len, unsigned int fifo_id);
static unsigned int (*tx_cb)(void *addr);
static struct timer_list test_timer;
static void sdiom_register_pt_rx_process(unsigned int type, unsigned int subtype, void *func) {
    rx_cb = func;
}
static void sdiom_register_pt_tx_release(unsigned int type, unsigned int subtype, void *func) {
    tx_cb = func;
}
unsigned int sdiom_pt_write(void *buf, unsigned int len, int type, int subtype) {
    int i = 0;

    buf_addr = buf;
    pr_err("fmdrv sdiom_pt_write len is %d\n", len);
    for (i = 0; i < len; i++)
        pr_err("fmdrv send data is %x\n", *(buf_addr+i));
    mod_timer(&test_timer, jiffies + msecs_to_jiffies(30));
    return 0;
}
unsigned int sdiom_pt_read_release(unsigned int fifo_id) {
    return 0;
}
void timer_cb(unsigned long data) {
    rx_cb(r1, 11, 0);
    if (*(buf_addr+4) == 0x04) {
        mdelay(100);
        rx_cb(r2, 9, 0);
    }
}

void test_init(void) {
    int i;

    for (i = 0; i < RX_NUM; i++)
        a[i] = i;
}
#endif

#ifdef FM_DUMP_DATA
static void dump_buf(unsigned char *buf, unsigned char len, const char *func) {
    int i;
    pr_info("%s xpbuf size: (%d)\n", func, len);
    for (i = 0; i < len; i++)
        pr_info("%s buf [%d]: 0x%02X\n", func, i, buf[i]);
    pr_info("\n");
}
#endif

static int fm_send_cmd(unsigned char subcmd, void *payload, int payload_len) {
    int num = 1;
    unsigned char *cmd_buf;
    struct fm_cmd_hdr *cmd_hdr;
    int size;

    mbuf_t *tx_head = NULL;
    mbuf_t *tx_tail = NULL;

    size = sizeof(struct fm_cmd_hdr) + ((payload == NULL) ? 0 : payload_len);
    cmd_buf = kmalloc(size + FM_SDIO_HEAD_LEN, GFP_KERNEL);
	//cmd_buf = (unsigned char *)dma_alloc_coherent(dm, size + FM_SDIO_HEAD_LEN, (dma_addr_t *)(&(tx_head->phy)), GFP_DMA);
    memset(cmd_buf, 0, size + FM_SDIO_HEAD_LEN);
    if (!cmd_buf) {
        pr_err("(fmdrv):%s():No memory to create new command buf\n", __func__);
        return -ENOMEM;
    }

    /* Fill command information */
    cmd_hdr = (struct fm_cmd_hdr *)(cmd_buf + FM_SDIO_HEAD_LEN);
    cmd_hdr->header = 0x01;
    /* cmd_hdr->cmd = 0xFC8C; */
    cmd_hdr->opcode = hci_opcode_pack(HCI_GRP_VENDOR_SPECIFIC, FM_SPRD_OP_CODE);
    cmd_hdr->len = ((payload == NULL) ? 0 : payload_len) + 1;
    cmd_hdr->fm_subcmd = subcmd;

    if (payload != NULL)
        memcpy(cmd_buf + FM_SDIO_HEAD_LEN + sizeof(struct fm_cmd_hdr), payload, payload_len);

    if (!sprdwcn_bus_list_alloc(FM_TX_CHANNEL, &tx_head, &tx_tail, &num)) {
		int ret = 0;
        struct device *dm = &g_fm_pdev->dev;
		pr_err("%s() sprdwcn_bus_list_alloc() success dm %p tx_head %p tx_tail %p num %d\n", __func__, dm, tx_head, tx_tail, num);

#if 1
        if ((ret = dma_set_mask(dm, DMA_BIT_MASK(64)))) {
            printk(KERN_ERR "dma_set_mask err ret %d\n", ret);
            if ((ret = dma_set_coherent_mask(dm, DMA_BIT_MASK(64)))) {
                printk(KERN_ERR "dma_set_coherent_mask err ret %d\n", ret);
                return -ENOMEM;
            }
        }

#elif 0
        {
            dm_t dm = {0};
            ret = dmalloc(&dm, count);
            pr_err("%s:line:%d dmalloc ret %d\n",__func__,__LINE__);
            return -ENOMEM;
        }
#endif

		tx_head->buf = (unsigned char *)dma_alloc_coherent(dm, size, (dma_addr_t *)(&(tx_head->phy)), GFP_DMA);
		if(!tx_head->buf){
			pr_err("%s:line:%d dma_alloc_coherent err dev %p size %d phy %p\n", __func__, __LINE__, &fmdev->pdev->dev, size, &(tx_head->phy));
            return -ENOMEM;
        }
		//fmdev->tx_head->buf = cmd_buf;
		memcpy(tx_head->buf, cmd_buf + FM_SDIO_HEAD_LEN, size);
        tx_head->len = size;
        tx_head->next = NULL;
#ifdef FM_DUMP_DATA
	dump_buf((unsigned char *)tx_head->buf, (unsigned char)(size + FM_SDIO_HEAD_LEN), __func__);
#endif
        if (sprdwcn_bus_push_list(FM_TX_CHANNEL, tx_head, tx_tail, num)) {
            pr_err("fmdrv write cmd to sdiom fail, free buf\n");
            dma_free_coherent(dm, size, (void *)tx_head->buf, tx_head->phy);

            tx_head->buf = NULL;
            sprdwcn_bus_list_free(FM_TX_CHANNEL, tx_head, tx_tail, num);
        }
    } else {
        pr_err("%s:%d mchn_bus_list_alloc fail\n", __func__, __LINE__);
    }
    kfree(cmd_buf);
    return 0;
}

static int fm_write_cmd(unsigned char subcmd, void *payload,
                        unsigned char payload_len,  void *response,
                        unsigned char *response_len) {
    /*struct fm_event_hdr *fm_evt_head;*/
    unsigned long timeleft;
    int ret;

    //wake_lock(&fm_wakelock);
    /*marlin_set_sleep(MARLIN_FM, 0);
    if (marlin_set_wakeup(MARLIN_FM) != 0) {
        pr_err("AP wake up marlin3 %s failed\n", __func__);
        return -EFAULT;
    }*/
    mutex_lock(&fmdev->mutex);
    ret = fm_send_cmd(subcmd, payload, payload_len);
    if (ret < 0) {
        /*marlin_set_sleep(MARLIN_FM, 1);*/
        //wake_unlock(&fm_wakelock);
        return ret;
    }

    timeleft = wait_for_completion_timeout(&fmdev->commontask_completion, FM_DRV_TX_TIMEOUT);
    if (!timeleft) {
        pr_err(timeoutformat"\n", __func__, jiffies_to_msecs(FM_DRV_TX_TIMEOUT) / 1000, subcmd);
        /*marlin_set_sleep(MARLIN_FM, 1);*/
        //wake_unlock(&fm_wakelock);
        mutex_unlock(&fmdev->mutex);
        return -ETIMEDOUT;
    }
    mutex_unlock(&fmdev->mutex);
    pr_debug("fmdrv wait command have complete\n");
    /* 0:len; XX XX XX sttaus*/
    if ((fmdev->com_respbuf[4]) != 0) {
        pr_err("(fmdrv) %s(): Response status not success for 0x%02X\n", __func__, subcmd);
        return -EFAULT;
    }
    pr_info("(fmdrv) %s(): Response status success for 0x%02X: %d\n", __func__, subcmd, fmdev->com_respbuf[4]);
    /*the event : 04 0e len 01 8C  fc  00(status) rssi snr freq .p->len*/
    if (response != NULL && response_len != NULL)
        memcpy(response, &(fmdev->com_respbuf[5]), fmdev->com_respbuf[0]-4);
    return 0;
}

/*
* 04 FF len  parameters
* (fm_evt_head->id ==0xFF)
* pdata == len
* RDS event:04 FF len 00~20  parameters
* new RDS interface 04 FF len 00 0ne group(crc_flag +block)
* Search  result event:04 FF len 30 parameters
* AF Jump evnet:04 FF len 31 parameters
* the data from SDIO is like this:
* 04 0e len 01 8C FC status rssi snr freq
* 04 FF len subevent status rssi snr freq
*/

static void receive_tasklet(unsigned long arg)
{
    struct fmdrv_ops *fmdev;
    struct fm_rx_data *rx = NULL;
    /* the data from SDIO is event data */
    //struct mbuf_t *head, *tail;
	mbuf_t *head, *tail;
    unsigned int channel, num;
    struct fm_sdio_hdr *sdio_hdr;
    unsigned char *receive_buf;

    fmdev = (struct fmdrv_ops *)arg;

    if (unlikely(!fmdev)) {
        pr_err("fm_rx_task fmdev is NULL\n");
        return;
    }
    pr_info("fm receive_tasklet start running\n");
    while (!list_empty(&fmdev->rx_head)) {
        spin_lock_bh(&fmdev->rw_lock);
        rx = list_first_entry_or_null(&fmdev->rx_head, struct fm_rx_data, entry);
        if (rx)
            list_del(&rx->entry);
        else {
            spin_unlock_bh(&fmdev->rw_lock);
            return;
        }
        head = rx->head;
        tail = rx->tail;
        channel = rx->channel;
        num = rx->num;
        sdio_hdr = kmalloc(sizeof(struct fm_sdio_hdr), GFP_ATOMIC);
        if (!sdio_hdr){
            pr_err("fm sdio_hdr kmalloc fail\n");
            return;
        }
        parse_sdio_header(head, tail, num, sdio_hdr);

        receive_buf = head->buf + FM_SDIO_HEAD_LEN;
        if ((*(receive_buf + 1)) == 0x0e) {
            memcpy(fmdev->com_respbuf, receive_buf + 2, (*(receive_buf+2)) + 1);
            pr_debug("fm RX before commontask_completion=0x%x\n", fmdev->commontask_completion.done);
            complete(&fmdev->commontask_completion);
            pr_debug("fm RX after commontask_completion=0x%x\n", fmdev->commontask_completion.done);
        } else if (((*((receive_buf)+1)) == 0xFF) && ((*(receive_buf+3)) == 0x30)) {
            memcpy(fmdev->seek_respbuf, receive_buf + 2, (*(receive_buf+2)) + 1);
            pr_debug("fm RX before seektask_completion=0x%x\n", fmdev->seektask_completion.done);
            complete(&fmdev->seektask_completion);
            pr_debug("fm RX after seektask_completion=0x%x\n", fmdev->seektask_completion.done);
        } else if (((*(receive_buf+1)) == 0xFF) && ((*(receive_buf+3)) == 0x00)) {
            rds_parser(receive_buf + 4);
        } else {
            pr_err("fmdrv error:unknown event !!!\n");
        }
        sprdwcn_bus_push_list(channel, head, tail, num);
        kfree(sdio_hdr);
        sdio_hdr = NULL;
        kfree(rx);
        rx = NULL;
        spin_unlock_bh(&fmdev->rw_lock);
    }
}

ssize_t fm_read_rds_data(struct file *filp, char __user *buf, size_t count, loff_t *pos) {
    int timeout = -1;
    pr_info("(FM_RDS) fm start to read RDS data\n");
#ifdef RDS_DEBUG
    sprintf(rds_debug_data.ps_data.PS[3], "PS_debug");
    sprintf(rds_debug_data.rt_data.textdata[3], "Welcome to spreadtrum, This is for RT data debug");
    pr_info("fm ioctl read rds freq =%d\n", global_freq);
    rds_debug_data.af_data.AF_NUM = 3;
    rds_debug_data.af_data.AF[1][0] = 1065;
    rds_debug_data.af_data.AF[1][1] = 1077;
    rds_debug_data.af_data.AF[1][2] = 1001;
    rds_debug_data.event_status = 0xFFF;
    rds_debug_data.rt_data.textlength = strlen(rds_debug_data.rt_data.textdata[3]);

    if (copy_to_user(buf, &(rds_debug_data), sizeof(rds_debug_data))) {
        pr_err("fm_read_rds_data ret value is -eFAULT\n");
        return -EFAULT;
    }
    return sizeof(rds_debug_data);
#endif

    if (filp->f_flags & O_NONBLOCK) {
        timeout = 0;
        pr_err("fm_read_rds_data NON BLOCK!!!\n");
        return -EWOULDBLOCK;
    }

    fmdev->rds_data.rt_data.textlength = strlen(fmdev->rds_data.rt_data.textdata[3]);
    pr_info("fm RT len is %d\n", fmdev->rds_data.rt_data.textlength);
    if (copy_to_user(buf, &(fmdev->rds_data), sizeof(fmdev->rds_data))) {
        pr_info("fm_read_rds_data ret value is -eFAULT\n");
        return -EFAULT;
    }
    pr_info("(fm drs) fm event is %x\n", fmdev->rds_data.event_status);
    #if(LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0))
    fmdev->rds_han.rds_parse_stop_time = ktime_get_real_seconds();
    #else
    fmdev->rds_han.rds_parse_stop_time = get_seconds();
    #endif
    if ((fmdev->rds_han.rds_parse_stop_time -
        fmdev->rds_han.rds_parse_start_time) >
        FM_RDS_PARSE_TIME) {
        pr_info("fm clear RDS event! [%ld]-[%ld]\n",
            fmdev->rds_han.rds_parse_start_time,
            fmdev->rds_han.rds_parse_stop_time);
        fmdev->rds_data.event_status = 0;
    }
    pr_info("PS=%s\n", fmdev->rds_data.ps_data.PS[3]);
	pr_info("RT=%s\n", fmdev->rds_data.rt_data.textdata[3]);
    pr_info("fm_read_rds_data end....\n");
    return sizeof(fmdev->rds_data);
}

int parse_sdio_header(mbuf_t *head,mbuf_t *tail, int num, struct fm_sdio_hdr *hdr)
{
	hdr->length = head->buf[2];
	pr_info("%s xp length: %d", __func__, hdr->length);
    dump_buf((unsigned char *)head->buf, (hdr->length)+3, __func__);
    return 0;
}

//chinaycheng
#if 0
void parse_at_fm_cmd(unsigned int *freq_found)
{
	int comma_cou = 0;
	int i = 0;
	int cmdstart = 0;
	int len = 0;
	char *cur_ptr;
	char num_str[32] = {0};
	int result = 0;

	cur_ptr = fmdev->read_buf;
	read_flag = 0;
	for (i = 0; i < 32 && cur_ptr[i] != '\0'; i++) {
		if (cur_ptr[i] == ',')
			comma_cou++;
		if (comma_cou == 3) {
			comma_cou = 0;
			cmdstart = i;
		}
	}
	for (i = 0, cmdstart++; cmdstart < 32 && cur_ptr[cmdstart] != '\0'
		&& cur_ptr[cmdstart] != ','; i++, cmdstart++) {
		if (cur_ptr[cmdstart] >= '0' && cur_ptr[cmdstart] <= '9')
			num_str[i] = cur_ptr[cmdstart];
		else if (cur_ptr[cmdstart] == ' ')
			break;
	}
	len = strlen(num_str);
	cur_ptr = num_str;
	result = cur_ptr[0] - '0';
	for (i = 1; i < len; i++)
		result = result * 10 + cur_ptr[i] - '0';
	*freq_found = result;
	pr_info("fm seek event have come freq=%d\n", result);
}
#endif

//chinaycheng
#if 0
void fm_sdio_read(void) {
    memset(fmdev->read_buf, 0, FM_READ_SIZE);
    /* fmdev->rcv_len = sdio_dev_get_chn_datalen(FM_CHANNEL_READ); */
    if (fmdev->rcv_len <= 0) {
        pr_err("FM_CHANNEL_READ len err\n");
        return;
    }
    if (fmdev->rcv_len > FM_READ_SIZE)
        pr_err("The read data len:%d, beyond max read:%d",
    fmdev->rcv_len, FM_READ_SIZE);
    /* sdio_dev_read(FM_CHANNEL_READ, fmdev->read_buf, &fmdev->rcv_len); */
    pr_info("* fmdev->read_buf: %s *\n", (char *)fmdev->read_buf);
}

int fm_sdio_write(unsigned char *buffer, unsigned int size) {
    /* sdiom_pt_write(buffer, size, FM_TYPE, FM_SUBTYPE0); */
    /* sdio_dev_write(FM_CHANNEL_WRITE, buffer, size); */
    printk_ratelimited("%s size: %d\n", __func__, size);
    return size;
}

int fm_sdio_init(void)
{
return 0;
}
#endif

//chinaycheng
#if 0
int fm_write(unsigned char *array, unsigned char len)
{
	unsigned long timeleft;
	int cnt = 0;

	cnt = 0;
	/* len = strlen(array); */
	fm_sdio_write(array, len);
	pr_info("fm: write_buf[0]: %x\n", array[0]);
	pr_info("TX before_completion done=0X%x\n", fmdev->completed.done);
	timeleft = wait_for_completion_timeout(&fmdev->completed,
		FM_DRV_TX_TIMEOUT);
	pr_info("TX_after_completion done=0X%x\n", fmdev->completed.done);
	if (!timeleft) {
		pr_err("Timeout, %d\n", ETIMEDOUT);
		return -ETIMEDOUT;

	} else {
		pr_info("success!\n");
	}

	return 0;
}
#endif

int fm_powerup(void) {
    struct fm_tune_parm parm;
    unsigned short payload[65];
    int ret = -1;
    struct fm_config_t fm_data;
    fmdev-> power_status = 0;
    fmdev-> power_status ++;

    memset(&parm, 0, sizeof(struct fm_tune_parm));
    parm.freq = 8750;

    memset(&fm_data, 0, sizeof(struct fm_config_t));
    pr_info("fm ioctl power up freq= %d\n", parm.freq);
    get_fm_config_param(&fm_data);
    payload[0] = parm.freq;
    memcpy(&payload[1],&fm_data,sizeof(struct fm_config_t));
    ret = fm_write_cmd(FM_POWERUP_CMD, payload, sizeof(payload), NULL, NULL);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM write pwrup cmd status failed %d\n", __func__, ret);
        return ret;
    }
    return ret;
}

int fm_powerdown(void) {
    int ret = -EINVAL;
    unsigned char payload;
    fmdev->power_status --;
    payload = FM_OFF;
    pr_info("fm ioctl power down\n");
    fmdev->rds_han.new_data_flag = 1;
    wake_up_interruptible(&fmdev->rds_han.rx_queue);
    ret = fm_write_cmd(FM_POWERDOWN_CMD, &payload, sizeof(payload),NULL, NULL);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM write pwrdown cmd status failed %d\n", __func__, ret);
        return ret;
    }

    return ret;
}

/* through SDIO to deliver data & command */
int fm_tune(void *arg){
    struct fm_tune_parm parm;
    int ret = 0;
    unsigned char respond_buf[5], respond_len;
    unsigned short freq;

    if (copy_from_user(&parm, arg, sizeof(parm))) {
        pr_info("fm tune 's ret value is -eFAULT\n");
        return -EFAULT;
    }
    //parm.freq *= 10;
#ifdef RDS_DEBUG
    global_freq = parm.freq;
#endif
    pr_info("fm ioctl tune freq = %d\n", parm.freq);
    ret = fm_write_cmd(FM_TUNE_CMD, &parm.freq, sizeof(parm.freq), respond_buf, &respond_len);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM write tune cmd status failed %d\n", __func__, ret);
        return ret;
    }
    freq = respond_buf[3] + (respond_buf[4] << 8);
    pr_info("(fmdrv) fm tune have finshed!!status =%d,RSSI=%d\n"
            "(fmdrv) SNR=%d,freq=%d\n", respond_buf[0], respond_buf[1],respond_buf[2], freq);
    return ret;
}
/*
* seek cmd :01 8C FC 04(length) 04 freq(16bit) seekdir(8bit)
* payload == freq,seekdir
* seek event:status,RSSI,SNR,Freq
*/
int fm_seek(void *arg) {
    struct fm_seek_parm parm;
    int ret = 0;
    unsigned char payload[3];
    unsigned char respond_buf[5];
    unsigned long timeleft;

    if (copy_from_user(&parm, arg, sizeof(parm))) {
        pr_info("fm seek 's ret value is -eFAULT\n");
        return -EFAULT;
    }
    //parm.freq *= 10;
    payload[0] = (parm.freq & 0xFF);
    payload[1] = (parm.freq >> 8);
    payload[2] = parm.seekdir;
    pr_info("fm ioctl seek freq=%d,dir =%d\n", parm.freq, parm.seekdir);
    ret = fm_write_cmd(FM_SEEK_CMD, payload, sizeof(payload), NULL, NULL);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM write seek cmd status failed %d\n", __func__, ret);
        return ret;
    }
    init_completion(&fmdev->seektask_completion);
    timeleft = wait_for_completion_timeout(&fmdev->seektask_completion,
    FM_DRV_RX_SEEK_TIMEOUT);
    if (!timeleft) {
        pr_err("(fmdrv) %s(): Timeout(%d sec),didn't get fm seek end !\n", __func__, jiffies_to_msecs(FM_DRV_RX_SEEK_TIMEOUT) / 1000);
        /* -110 */
        return -ETIMEDOUT;
    }

    memcpy(respond_buf, &(fmdev->seek_respbuf[2]),
    fmdev->seek_respbuf[0] - 1);
#if 0
	rx = fmdev->seek_response;
	if (fmdev->seek_response == NULL)
		pr_err("(fmdrv): error:fmdev->seek_response is  NULL\n");
	fm_evt_head = (struct fm_event_hdr *)rx->addr;
	p_data = &(fm_evt_head->len);
	pr_info("fm seek event id is 0x%x\n", fm_evt_head->id);
	if (fm_evt_head->id == HCI_VS_EVENT) {
		if (*(p_data + 1) == 0x30)
			memcpy(respond_buf, p_data + 2, fm_evt_head->len - 1);
		else
			pr_err("fm seek data error\n");
	}
#endif
    parm.freq = respond_buf[3] + (respond_buf[4] << 8);
    //parm.freq /= 10;
    pr_info("(fmdrv) fm seek have finshed!!status = %d, RSSI=%d\n"
            "(fmdrv) fm seek SNR=%d, freq=%d\n", respond_buf[0], respond_buf[1], respond_buf[2], parm.freq);
    /* pass the value to user space */
    if (copy_to_user(arg, &parm, sizeof(parm)))
        ret = -EFAULT;
    return ret;
}

/*
* mute cmd :01 8C FC  02(length)  02 mute(8bit)
* mute event:status,ismute
*
*/
int fm_mute(void *arg) {
    unsigned char mute = 0;
    int ret = -1;
    if (copy_from_user(&mute, arg, sizeof(mute))) {
        pr_err("fm mute 's ret value is -eFAULT\n");
        return -EFAULT;
    }
    pr_info("fm ioctl mute: %d (1:mute 0:unmute)\n", mute);
    if (mute != 1  && mute != 0) {
        pr_err("fm ioctl mute get wrong parameter\n");
        return ret;
    }
    ret = fm_write_cmd(FM_MUTE_CMD, &mute, sizeof(mute), NULL, NULL);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM write mute cmd result failed %d\n", __func__, ret);
        return ret;
    }
    return ret;
}

int fm_set_volume(void *arg) {
    unsigned char vol;
    int ret = 0;

    if (copy_from_user(&vol, arg, sizeof(vol))) {
        pr_err("fm set volume 's ret value is -eFAULT\n");
        return -EFAULT;
    }
    pr_info("fm ioctl set_volume =%d\n", vol);
    ret = fm_write_cmd(FM_SET_VOLUME_CMD, &vol, sizeof(vol), NULL, NULL);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM set volume result failed %d\n", __func__, ret);
        return ret;
    }
    return ret;
}

int fm_get_volume(void *arg) {
    unsigned char payload = 0;
    unsigned char res_len;
    int volume = 0;
    unsigned char resp_buf[1];
    int ret = -1;

    pr_info("fm ioctl get volume =0x%x\n", volume);
    ret = fm_write_cmd(FM_GET_VOLUME_CMD, &payload, sizeof(payload), &resp_buf[0], &res_len);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM write get volime cmd result failed %d\n", __func__, ret);
        return ret;
    }

    volume = (int)resp_buf[0];
    if (copy_to_user(arg, &volume, sizeof(volume)))
        ret = -EFAULT;
    return ret;
}

int fm_stop_scan(void *arg) {
    int ret = -EINVAL;

    pr_info("fm ioctl stop scan\n");
    ret = fm_write_cmd(FM_SEARCH_ABORT, NULL, 0, NULL, NULL);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM write stop scan cmd result failed %d\n", __func__, ret);
        return ret;
    }
    return ret;
}

int fm_scan_all(void *arg) {
    struct fm_scan_all_parm parm;
    int ret = 0;
    unsigned char respond_len;
    struct fm_scan_all_parm respond_buf;

    pr_info("fm ioctl scan all\n");
    if (copy_from_user(&parm, arg, sizeof(parm))) {
        pr_err("fm search all 's ret value is -eFAULT\n");
        return -EFAULT;
    }

    ret = fm_write_cmd(FM_SCAN_ALL_CMD, &parm, sizeof(parm), &respond_buf, &respond_len);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM write scan all cmd result failed %d\n", __func__, ret);
        return ret;
    }
    if (copy_to_user(arg, &parm, sizeof(parm)))
        ret = -EFAULT;
    return ret;
}

int fm_rw_reg(void *arg) {
    struct fm_reg_ctl_parm parm;
    int ret = 0;
    unsigned char  respond_len;

    if (copy_from_user(&parm, arg, sizeof(parm))) {
        pr_err("fm read and write register 's ret value is -eFAULT\n");
        return -EFAULT;
    }
    pr_info("fm ioctl read write reg = %d\n", parm.rw_flag);
    ret = fm_write_cmd(FM_READ_WRITE_REG_CMD, &parm, sizeof(parm), &parm, &respond_len);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM write register cmd result failed %d\n", __func__, ret);
        return ret;
    }
    if (copy_to_user(arg, &parm, sizeof(parm)))
        ret = -EFAULT;
    return ret;
}
int fm_get_monostero(void *arg) {
    return 0;
}

/* audio mode: 0:None   1: mono  2:steron  */
int fm_set_audio_mode(void *arg) {
    unsigned char mode;
    int ret = 0;

    if (copy_from_user(&mode, arg, sizeof(mode))) {
        pr_err("fm set audio mode 's ret value is -eFAULT\n");
        return -EFAULT;
    }
    pr_info("fm ioctl set audio mode =%d\n", mode);
    ret = fm_write_cmd(FM_SET_AUDIO_MODE, &mode, sizeof(mode), NULL, NULL);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM set audio mode result failed %d\n", __func__, ret);
        return ret;
    }
    return ret;
}

/*
* MAX_FREQUENCY_JAPAN=9000;
* MIN_FREQUENCY_JAPAN=7600;
* MAX_FREQUENCY_US_EUROPE=10800;
* MIN_FREQUENCY_US_EUROPE=8750;
* MAX_FREQUENCY_JAPAN_II=10800;
* MIN_FREQUENCY_JAPAN_II=9000;
	default: US_EUROP
	0: JAPAN
	1:US_EUROPE
	2: JAPAN_II
*/
int fm_set_region(void *arg) {
    unsigned char region;
    int ret = 0;

    if (copy_from_user(&region, arg, sizeof(region))) {
        pr_err("fm set region 's ret value is -eFAULT\n");
        return -EFAULT;
    }
    pr_info("fm ioctl set region =%d\n", region);
    ret = fm_write_cmd(FM_SET_REGION, &region, sizeof(region), NULL, NULL);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM set region result failed %d\n", __func__, ret);
        return ret;
    }
    return ret;
}

/*
*SCAN_STEP_50KHZ  0
*SCAN_STEP_100KHZ 1
*SCAN_STEP_200KHZ  2
*/
int fm_set_scan_step(void *arg) {
    unsigned char step;
    int ret = 0;

    if (copy_from_user(&step, arg, sizeof(step))) {
        pr_err("fm set scan step 's ret value is -eFAULT\n");
        return -EFAULT;
    }
    pr_info("fm ioctl set scan step =%d\n", step);
    ret = fm_write_cmd(FM_SET_SCAN_STEP, &step, sizeof(step), NULL, NULL);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM set scan step result failed %d\n", __func__, ret);
        return ret;
    }
    return ret;
}

int fm_config_deemphasis(void *arg) {
    unsigned char dp;
    int ret = 0;

    if (copy_from_user(&dp, arg, sizeof(dp))) {
        pr_err("fm config_deemphasis 's ret value is -eFAULT\n");
        return -EFAULT;
    }
    pr_info("fm ioctl config_deemphasis =%d\n", dp);
    ret = fm_write_cmd(FM_CONFIG_DEEMPHASIS, &dp, sizeof(dp), NULL, NULL);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM config_deemphasis result failed %d\n", __func__, ret);
        return ret;
    }
    return ret;
}

int fm_get_audio_mode(void *arg) {
    unsigned char res_len;
    int audio_mode;
    unsigned char resp_buf[2];
    int ret = -1;

    pr_info("fm ioctl get audio mode\n");
    ret = fm_write_cmd(FM_GET_AUDIO_MODE, NULL, 0, &resp_buf[0], &res_len);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM get audio mode cmd result failed %d\n", __func__, ret);
        return ret;
    }

    audio_mode = (int)resp_buf[1];
    if (copy_to_user(arg, &audio_mode, sizeof(audio_mode)))
        ret = -EFAULT;
    return ret;
}

int fm_get_current_bler(void *arg) {
    unsigned char res_len;
    int BLER;
    unsigned char resp_buf[1];
    int ret = -1;

    pr_info("fm ioctl get current BLER\n");
    ret = fm_write_cmd(DM_GET_CUR_BLER_CMD, NULL, 0, &resp_buf[0], &res_len);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM get BLER cmd result failed %d\n", __func__, ret);
        return ret;
    }

    BLER = (int)resp_buf[0];
    if (copy_to_user(arg, &BLER, sizeof(BLER)))
        ret = -EFAULT;
    return ret;
}

int fm_get_cur_snr(void *arg) {
    unsigned char res_len;
    int SNR;
    unsigned char resp_buf[1];
    int ret = -1;

    pr_info("fm ioctl get current SNR\n");
    ret = fm_write_cmd(FM_GET_SNR_CMD, NULL, 0, &resp_buf[0], &res_len);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM get SNR cmd status failed %d\n", __func__, ret);
        return ret;
    }

    SNR = (int)resp_buf[0];
    if (copy_to_user(arg, &SNR, sizeof(SNR)))
        ret = -EFAULT;
    return ret;
}

int fm_softmute_onoff(void *arg) {
    unsigned char softmute_on;
    int ret = 0;

    if (copy_from_user(&softmute_on, arg, sizeof(softmute_on))) {
        pr_err("fm softmute_onoff 's ret value is -eFAULT\n");
        return -EFAULT;
    }
    pr_info("fm ioctl softmute: %d (0:OFF  1:ON)\n", softmute_on);
    if (softmute_on != 0  && softmute_on != 1) {
        pr_err("fm ioctl unknown softmute\n");
        return ret;
    }
    ret = fm_write_cmd(FM_SOFTMUTE_ONOFF_CMD, &softmute_on, sizeof(softmute_on), NULL, NULL);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM write softmute onoff cmd status failed %d\n", __func__, ret);
        return ret;
    }
    return ret;
}

int fm_set_seek_criteria(void *arg)
{
    struct fm_seek_criteria_parm parm;
    int ret = 0;

    if (copy_from_user(&parm, arg, sizeof(parm))) {
        pr_err("fm set_seek_criteria 's ret value is -eFAULT\n");
        return -EFAULT;
    }

    pr_info("fm ioctl set_seek_criteria "seekformat"\n",
                          parm.rssi_th,parm.snr_th, parm.freq_offset_th,
                          parm.pilot_power_th, parm.noise_power_th);
    ret = fm_write_cmd(FM_SET_SEEK_CRITERIA_CMD, &parm, sizeof(parm), NULL, NULL);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM set seek criteria cmd status failed %d\n", __func__, ret);
        return ret;
    }
    return ret;
}

/*******************************************************
* 1. soft_mute---soft mute parameters
*	hbound >= lbound;
*	hbound : valid range is 402 - 442(-70dbm ~ -110 dbm)
*	lbound: valid range is 402 - 442(-70dbm ~ -110 dbm)
* Example
*		lbound   422(-90dbm) hbound 427(-85dbm)
*		Inpwr < -85dbm,   enable softmute
*		Inpwr > -90dbm ,disable softmute
*
* 2. blend----stereo/mono blend threshold
*	power_th: the signal intensity,
*			 valid range 402~432(Mean:-80dbm~-110dbm)
*			 default value is 442
*	phyt:  Retardation coefficient valid range is 0~ 7; default value is 5
* Example:
*		Power_th 422(-90dbm), Hyst 2
*		inpwr< power_threshold- hyst\uff08420 mean-92dbm), switch mono
*		inpwr>power_threshold+hyst (424 mean -88dbm), switch stereo
* 3. SNR_TH
**************************************************************/
int fm_set_audio_threshold(void *arg) {
    struct fm_audio_threshold_parm parm;
    int ret = 0;

    if (copy_from_user(&parm, arg, sizeof(parm))) {
        pr_err("fm set_audio_threshold 's ret value is -eFAULT\n");
        return -EFAULT;
    }

    pr_info("fm ioctl set_audio_threshold" audioformat"\n",
                               parm.hbound, parm.lbound,
                               parm.power_th, parm.phyt, parm.snr_th);
    ret = fm_write_cmd(FM_SET_AUDIO_THRESHOLD_CMD, &parm, sizeof(parm), NULL, NULL);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM set audio threshold cmd status failed %d\n", __func__, ret);
        return ret;
    }
    return ret;
}

int fm_get_seek_criteria(void *arg) {
    struct fm_seek_criteria_parm parm;
    unsigned char res_len;
    int ret = -1;

    pr_info("fm ioctl get_seek_criteria\n");
    ret = fm_write_cmd(FM_GET_SEEK_CRITERIA_CMD, NULL, 0, &parm, &res_len);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM write get seek_criteria cmd status failed %d\n", __func__, ret);
        return ret;
    }
    if (copy_to_user(arg, &parm, sizeof(parm)))
        ret = -EFAULT;
    return ret;
}

int fm_get_audio_threshold(void *arg) {
    struct fm_audio_threshold_parm parm;
    unsigned char res_len;
    int ret = -1;

    pr_info("fm ioctl get_audio_threshold\n");
    ret = fm_write_cmd(FM_GET_AUDIO_THRESHOLD_CMD, NULL, 0, &parm, &res_len);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM write get audio_thresholdi cmd status failed %d\n", __func__, ret);
        return ret;
    }

    if (copy_to_user(arg, &parm, sizeof(parm)))
        ret = -EFAULT;
    return ret;
}


int fm_getrssi(void *arg) {
    unsigned char payload = 0;
    unsigned char res_len;
    int rssi;
    unsigned char resp_buf[1];
    int ret = -1;

    ret = fm_write_cmd(FM_GET_RSSI_CMD, &payload, sizeof(payload), &resp_buf[0], &res_len);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM write getrssi cmd status failed %d\n", __func__, ret);
        return ret;
    }

    rssi = (int)resp_buf[0];
    if (copy_to_user(arg, &rssi, sizeof(rssi)))
        ret = -EFAULT;
    return ret;
}

struct fm_rds_data *get_rds_data(void) {
    pr_info("fm get rds data\n");
    return g_rds_data_string;
}

/*
* rdsonoff cmd :01 8C FC  03(length)  06 rdson(8bit) afon(8bit)
* rdsonoff event:status,rdson,afon
*
*/
int fm_rds_onoff(void *arg) {
    unsigned char rds_on, af_on;
    int ret = 0;
    unsigned char payload[2];

	int status = 0;

	if (fmdev->fm_invalid == 1 ){
		status = fm_powerup();
		if(status != 0){
			ret = reset_open_state;
			pr_info("fm wcnd reset stay command invalid status\n");
			return ret;
		} else {
			fmdev->fm_invalid = 0;
			pr_info("fm wcnd reset stay command valid status\n");
		}
    }

    if (copy_from_user(&rds_on, arg, sizeof(rds_on))) {
        pr_err("fm rds_onoff 's ret value is -eFAULT\n");
        return -EFAULT;
    }
    if (rds_on == 0) {
        fmdev->rds_han.new_data_flag = 1;
		fmdev->rds_han.get_rt_cnt = 0;
        memset(&fmdev->rds_data, 0, sizeof(fmdev->rds_data));
        wake_up_interruptible(&fmdev->rds_han.rx_queue);
        pr_info("fm ioctl RDS OFF\n");
    } else if (rds_on == 1) {
        fmdev->rds_han.new_data_flag = 0;
		fmdev->rds_han.get_rt_cnt = 0;
        pr_info("fm ioctl RDS ON\n");
    }
    else
        pr_info("fm ioctl unknown RDS\n");
    payload[0] = rds_on;
    payload[1] = rds_on;
    af_on = rds_on;
    pr_debug("fm cmd: %d,%d,%d\n", FM_SET_RDS_MODE, rds_on, af_on);
    ret = fm_write_cmd(FM_SET_RDS_MODE, payload, sizeof(payload), NULL, NULL);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM write rds mode cmd status failed %d\n", __func__, ret);
        return ret;
    }

    #if(LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0))
    fmdev->rds_han.rds_parse_start_time = ktime_get_real_seconds();
    #else
    fmdev->rds_han.rds_parse_start_time = get_seconds();
    #endif
    return ret;
}

/*0:short ana; 1:long ana*/
int fm_ana_switch(void *arg) {
    int antenna;
    int ret = 0;
    unsigned char payload;

    if (copy_from_user(&antenna, arg, sizeof(antenna))) {
        pr_err("fm ana switch 's ret value is -eFAULT\n");
        return -EFAULT;
    }
    pr_info("fm ioctl ana switch is %d\n", antenna);

    payload = antenna;
    ret = fm_write_cmd(FM_SET_ANA_SWITCH_CMD, &payload, sizeof(payload), NULL, NULL);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM write ANA switch cmd status failed %d\n", __func__, ret);
        return ret;
    }
    return ret;
}

int fm_af_onoff(void *arg) {
    unsigned char af_on;
    int ret = 0;

    if (copy_from_user(&af_on, arg, sizeof(af_on))) {
        pr_err("fm af_onoff 's ret value is -eFAULT\n");
        return -EFAULT;
    }
    pr_info("fm ioctl AF: %d (0:OFF 1:ON)\n", af_on);
    if (af_on != 0 && af_on != 1) {
        pr_info("fm ioctl unknown AF\n");
        return ret;
    }
    ret = fm_write_cmd(FM_SET_AF_ONOFF, &af_on, sizeof(af_on), NULL, NULL);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM write af on off cmd status failed %d\n", __func__, ret);
        return ret;
    }
    return ret;
}

/*
* get RSSI for every freq in AF list
* rdsonoff cmd :01 8C FC  01(length)  0D
* rdsonoff event:status,rdson,afon
*
*/
int fm_getcur_pamd(void *arg) {
    unsigned char PAMD_LEN;
    unsigned short PAMD;
    int ret = -1;
    unsigned char resp_buf[1];
#ifdef RDS_DEBUG
    /*
    * PAMD>=8 not jump
    * if <8.jump >12
    * if < 8, list >8, < 12  tune again and again wrong
    */
    PAMD = 5;
    if (global_freq == 10770)
        PAMD = 9;
    if (global_freq == 10650)
        PAMD = 15;
    if (global_freq == 10010)
        PAMD = 1;
    if (global_freq == 9140)
        PAMD = 11;

    if (copy_to_user(arg, &PAMD, sizeof(PAMD)))
        ret = -EFAULT;
    return ret;
#endif

    ret = fm_write_cmd(FM_GET_CURPAMD, NULL, 0, &resp_buf[0], &PAMD_LEN);
    if (ret < 0) {
        pr_err("(fmdrv) %s FM write getcur PAMD cmd status failed %d\n", __func__, ret);
        return ret;
    }

    PAMD = (unsigned short)resp_buf[0];
    pr_info("fm get PAMD =%d\n", PAMD);
    if (copy_to_user(arg, &PAMD, sizeof(PAMD)))
        ret = -EFAULT;
    return ret;
}

void set_rds_drv_data(struct fm_rds_data *fm_rds_info) {
    g_rds_data_string = fm_rds_info;
}

void fm_rds_init(void) {
    fmdev->rds_han.new_data_flag = 0;
}

#define SPRDWL_MH_ADDRESS_BIT (1UL << 39)

int __init init_fm_driver(void) {
    int ret = 0;
    struct fm_rds_data *fm_rds_info;
    fmdev = kzalloc(sizeof(struct fmdrv_ops), GFP_KERNEL);
    if (!fmdev)
        return -ENOMEM;

    //init_completion(&fmdev->completed);
    init_completion(&fmdev->commontask_completion);
    init_completion(&fmdev->seektask_completion);
    spin_lock_init(&(fmdev->rw_lock));
    mutex_init(&fmdev->mutex);
    INIT_LIST_HEAD(&(fmdev->rx_head));

    //fmdev->read_buf =  kzalloc(FM_READ_SIZE, GFP_KERNEL);
    /* malloc mem for rds struct */
    fm_rds_info = kzalloc(sizeof(struct fm_rds_data), GFP_KERNEL);
    if (NULL == fm_rds_info) {
        pr_err("fm can't allocate FM RDS buffer\n");
        return ret;
    }
    set_rds_drv_data(fm_rds_info);

    /* Register FM Tx and Rx callback */
    //sdiom_register_pt_rx_process(FM_TYPE, FM_SUBTYPE0, fm_rx_cback);
    //sdiom_register_pt_tx_release(FM_TYPE, FM_SUBTYPE0, fm_tx_cback);
    pr_err("chinaycheng fm init channel...\n");

     /* retval = sdiodev_readchn_init(FM_CHANNEL_READ, fm_read, 0);*/
    ret = fm_device_init_driver();
    tasklet_init(&fmdev->rx_task, receive_tasklet, (unsigned long)fmdev);
    /* RDS init */
    fm_rds_init();
    init_waitqueue_head(&fmdev->rds_han.rx_queue);

#ifdef FM_TEST
    setup_timer(&test_timer, timer_cb, 0);
    test_init();
#endif
    return ret;
}

void __exit exit_fm_driver(void) {
    fm_device_exit_driver();
    //tasklet_kill(&fmdev->tx_task);
    tasklet_kill(&fmdev->rx_task);
    //kfree(fmdev->read_buf);
    //fmdev->read_buf = NULL;
    kfree(fmdev);
    fmdev = NULL;
}

module_init(init_fm_driver);
module_exit(exit_fm_driver);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE("GPL");
MODULE_VERSION(FM_VERSION);
