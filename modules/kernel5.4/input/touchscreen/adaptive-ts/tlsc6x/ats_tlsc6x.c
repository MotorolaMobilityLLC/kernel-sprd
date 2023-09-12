#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include "adaptive_ts.h"
#include <linux/module.h>
#include <linux/of.h>
#include <uapi/linux/input.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include "ats_core.h"
#include "ats_tlsc6x_main.h"

#define FT_MAX_POINTS 2
#define FT_POINT_LEN 6
#define FT_HEADER_LEN 3
#define FT_MAX_UPGRADE_RETRY 30
#define FT_FIRMWARE_PACKET_LENGTH 128
#define FT_ALL_DATA_LEN  18//(FT_HEADER_LEN + FT_MAX_POINTS * FT_POINT_LEN)

#define REG_SCANNING_FREQ     0x88
#define REG_CHARGER_INDICATOR 0x8B
#define REG_CHIP_ID           0xA3
#define REG_POWER_MODE        0xA5
#define REG_FIRMWARE_VERSION  0xA6
#define REG_MODULE_ID         0xA8
#define REG_WORKING_STATE     0xFC
#define GESTURE_LEFT		0x20
#define GESTURE_RIGHT		0x21
#define GESTURE_UP		    0x22
#define GESTURE_DOWN		0x23
#define GESTURE_DOUBLECLICK	0x24
#define GESTURE_O		    0x30
#define GESTURE_W		    0x31
#define GESTURE_M		    0x32
#define GESTURE_E		    0x33
#define GESTURE_C		    0x34
#define GESTURE_S           0x46
#define GESTURE_V           0x54
#define GESTURE_Z           0x65
#define GESTURE_L           0x44

DEFINE_MUTEX(i2c_rw_access);
#define to_tlsc6x_controller(ptr) \
	container_of(ptr, struct tlsc6x_controller, controller)
extern struct i2c_client *g_client;
int tlsc6x_esdHelperFreeze;
static const unsigned short tlsc6x_addrs[] = { 0x2e};
unsigned char tlsc6x_chip_name[MAX_CHIP_ID][20] =
	{"null", "tlsc6206a", "tlsc6306", "tlsc6206",
	 "tlsc6324", "tlsc6332", "chsc6440", "chsc6448",
	 "chsc6432", "chsc6424", "chsc6306BF", "chsc6417",
	 "chsc6413", "chsc6540"
    };

#ifdef TLSC_APK_DEBUG
unsigned char proc_out_len;
unsigned char proc_out_buf[257];

static unsigned char send_data_flag = 0;
static unsigned char get_data_flag = 0;
static int send_count = 0;
static unsigned int frame_cnt;

unsigned char debug_type;
unsigned char iic_reg[2];
unsigned char sync_flag_addr[3];
unsigned char sync_buf_addr[2];
unsigned char reg_len;

static char buf_in[1026] = {0};
static char buf_out[1026] = {0};

int auto_upd_busy = 0;

static struct proc_dir_entry *tlsc6x_proc_entry = NULL;


int tssc_get_debug_info(struct i2c_client *i2c_client, char *p_data)
{
	char writebuf[10] = {0};
	short size = 61;
	unsigned char loop, k;
	unsigned char cmd[2];
	unsigned short check, rel_size;
	char buft[128];
	unsigned short *p16_buf = (unsigned short *)buft;
	cmd[0] = 1;

	loop = 0;
	rel_size = size * 2;

	while (loop++ < 2) {
		writebuf[0] = 0x9f; //0xc2; //pt_dbg_mtk
		writebuf[1] = 0x26; //0x80; //0x80c280
		tlsc6x_i2c_read(i2c_client, writebuf,  2, buft, rel_size + 2);  //124 2->checksum
		for (k = 0, check = 0; k < size; k++) {
			check += p16_buf[k];
			//printk("Alpha:debug p16_buf is %d\n",p16_buf[k]);
			//printk("Alpha:debug buft is %d,%d\n",buft[k*2],buft[k*2+1]);
		}
		//printk("Alpha:debug check is %d\n",check);
		if (check == p16_buf[size]) {
			p16_buf[size] = 0x5555;
			break;
		} else {
			p16_buf[size] = 0xaaaa;
		}
		//printk("Alpha:debug p16_buf last is %d\n",p16_buf[size]);
		//printk("Alpha:debug check is %d\n",check);
	}
	//*((unsigned int)(&buft[124])) = cnt;
	buft[124] = (frame_cnt) & 0xff;
	buft[125] = (frame_cnt >> 8) & 0xff;
	buft[126] = (frame_cnt >> 16) & 0xff;
	buft[127] = (frame_cnt >> 24) & 0xff;
	frame_cnt++;

        memcpy(&buf_in[send_count * 128], buft, (sizeof(char) * 128));
        if(send_count++ >= 7){
		//tlsc6x_irq_disable();
		memcpy(buf_out, buf_in, (sizeof(char) * 1024));
		memset(buf_in, 0xff,(sizeof(char) * 1024));
		//tlsc6x_irq_enable();
		send_count = 0;
		get_data_flag = 1;
        }

	{
		static unsigned char msk_o;
		unsigned char msk = 0;
		unsigned short *p_point = &p16_buf[61 - 5];
		unsigned char tcnt = buft[61*2 - 2] & 0xf;
		unsigned short x0 = p_point[0];
		unsigned char id0 = (x0 & 0x8000) >> 15;
		unsigned short y0;
		unsigned char id1;
		unsigned short x1;
		unsigned short y1;
		unsigned char mch;
		unsigned char act;

		x0 = x0 & 0x3fff;
		y0 = p_point[1];
		if((x0>0)&&(y0>0))
			msk = 1 << id0;
		x1 = p_point[2];
		id1 = (x1 & 0x8000) >> 15;
		x1 = x1 & 0x3fff;
		y1 = p_point[3];
		if((x1>0)&&(y1>0))
			msk |= 1 << id1;

		mch = msk ^ msk_o;
		if ((3 == mch) && (1 == tcnt)) {
			tcnt = 0;
			msk = 0;
			mch = msk_o;
			x0 = x1 = 0;
		}
		msk_o = msk;
		memset(p_data, 0xff, FT_ALL_DATA_LEN);

		p_data[0] = 0;
		p_data[1] = 0;
		p_data[2] = tcnt;

		if(g_pdata->tpd_prox_active) {
			if(p_point[0]&0x4000) {
				p_data[1] = 0xC0;
			}
			else {
				p_data[1] = 0xE0;
			}
		}

		act = 0;
		if (x0 > 0 && y0 > 0) {
			act = (0 == (mch & (0x01 << id0))) ? 0x80 : 0;
		} else {
			act = 0x40;
			id0 = !id1;
		}
		p_data[3] = (act | (x0 >> 8));
		p_data[4] = (x0 & 0xff);
		p_data[5] = (id0 << 4) | (y0 >> 8);
		p_data[6] = (y0 & 0xff);
		p_data[7] = 0x0d;
		p_data[8] = 0x10;

		if (x1 > 0 && y1 > 0) {
			act = (0 == (mch & (0x01 << id1))) ? 0x80 : 0;
		} else {
			act = 0x40;
			id1 = !id0;
		}
		p_data[9] = (act | (x1 >> 8));
		p_data[10] = (x1 & 0xff);
		p_data[11] = (id1 << 4) | (y1 >> 8);
		p_data[12] = (y1 & 0xff);
		p_data[13] = 0x0d;
		p_data[14] = 0x10;
	}

	return 0;
}

/* 0:success */
/* 1: no file OR open fail */
/* 2: wrong file size OR read error */
/* -1:op-fial */
int tlsc6x_proc_cfg_update(u8 *dir, int behave)
{
	int ret = 1;
	u8 *pbt_buf = NULL;
	s32 fileSize;
	//mm_segment_t old_fs;
	static struct file *file = NULL;

	TLSC_FUNC_ENTER();
	tlsc_info("tlsc6x proc-file:%s\n", dir);

	file = filp_open(dir, O_RDONLY, 0);
	if (IS_ERR(file)) {
		tlsc_err("tlsc6x proc-file:open error!\n");
	} else {
		ret = 2;
		//old_fs = get_fs();
		//set_fs(KERNEL_DS);
		fileSize = file->f_op->llseek(file, 0, SEEK_END);
		tlsc_info("tlsc6x proc-file, size:%d\n", fileSize);
		if(fileSize > 0 )
			pbt_buf = kmalloc(fileSize, GFP_KERNEL);

		file->f_op->llseek(file, 0, SEEK_SET);
		//if (fileSize == file->f_op->read(file, (char *)pbt_buf, fileSize, &file->f_pos)) {
        if (fileSize == kernel_read(file, (char *)pbt_buf, fileSize, &file->f_pos)){
			tlsc_info("tlsc6x proc-file, read ok1!\n");
			ret = 3;
		}

		if (ret == 3) {
			auto_upd_busy = 1;
			//tlsc6x_irq_disable();
			ts_enable_irq_ex(g_pdata, false);
			msleep(1000);
			__pm_wakeup_event((g_pdata->upgrade_lock), 1000);
			if (behave == 0) {
				if (fileSize == 204) {
					ret = tlsx6x_update_running_cfg((u16 *) pbt_buf);
				} else if (fileSize > 0x400) {
					tlsc6x_load_ext_binlib((u8 *) pbt_buf, (u16) fileSize);
				}
			} else if (behave == 1) {
				if (fileSize == 204) {
					ret = tlsx6x_update_burn_cfg((u16 *) pbt_buf);
				} else if (fileSize > 0x400) {
					ret = tlsc6x_update_f_combboot((u8 *) pbt_buf, (u16) fileSize);
				}
				//tlsc6x_tpd_reset();
				ts_reset_controller_ex(ts_get_ts_data(), true);
			}
			//tlsc6x_irq_enable();
			ts_enable_irq_ex(g_pdata, true);
			auto_upd_busy = 0;
		}

		filp_close(file, NULL);
		//set_fs(old_fs);

		kfree(pbt_buf);
	}

	return ret;
}

static int debug_read(char *writebuf, int writelen, char *readbuf, int readlen)
{
	int ret = 0;

	mutex_lock(&i2c_rw_access);
	tlsc6x_set_dd_mode_sub();

	ret = tlsc6x_i2c_read_sub(g_tlsc6x_client, writebuf, writelen, readbuf, readlen);

	tlsc6x_set_nor_mode_sub();
	mutex_unlock(&i2c_rw_access);
	if (ret > 0) {
		ret = readlen;
	}
	return ret;
}

static int debug_write(char *writebuf, int writelen)
{
	int ret = 0;

	mutex_lock(&i2c_rw_access);
	tlsc6x_set_dd_mode_sub();

	ret = tlsc6x_i2c_write_sub(g_tlsc6x_client, writebuf, writelen);

	tlsc6x_set_nor_mode_sub();
	mutex_unlock(&i2c_rw_access);
	if (ret > 0) {
		ret = writelen;
	}
	return ret;
}

static int debug_read_sync(char *writebuf, int writelen, char *readbuf, int readlen)
{
	int ret = 0;
	int retryTime;

	mutex_lock(&i2c_rw_access);
	tlsc6x_set_dd_mode_sub();
	sync_flag_addr[2] = 1;
	ret = tlsc6x_i2c_write_sub(g_tlsc6x_client, sync_flag_addr, 3);

	retryTime = 100;
	do {
		ret = tlsc6x_i2c_read_sub(g_tlsc6x_client, sync_flag_addr, 2, &sync_flag_addr[2], 1);
		if (ret < 0) {
			mutex_unlock(&i2c_rw_access);
			return ret;
		}
		retryTime--;
	} while (retryTime>0&&sync_flag_addr[2] == 1);
	if(retryTime==0&&sync_flag_addr[2] == 1) {
		mutex_unlock(&i2c_rw_access);
		return -EFAULT;
	}
	if (ret >= 0) {
		/* read data */
		ret = tlsc6x_i2c_read_sub(g_tlsc6x_client, sync_buf_addr, 2, readbuf, readlen);
	}

	tlsc6x_set_nor_mode_sub();
	mutex_unlock(&i2c_rw_access);
	if (ret > 0) {
		ret = readlen;
	}
	return ret;
}

unsigned char tlsc6x_local_buf[8]={0};
void tlsc6x_apkdebug_mode(void){

		char writebuf[10] = {0};
                tlsc6x_set_dd_mode();
                // send addr
                writebuf[0] = 0x9f; //0xc3; //flag
                writebuf[1] = 0x22; //
                writebuf[2] = tlsc6x_local_buf[2]; //addr
                writebuf[3] = tlsc6x_local_buf[3]; //
                tlsc6x_i2c_write(g_tlsc6x_client, writebuf, 4);

                // send len
                writebuf[0] = 0x9f; //dbg_mtk_syn
                writebuf[1] = 0x20; //
                writebuf[2] = 61; //local_buf[4]; //
                //writebuf[3] = 0;//local_buf[5]; //
                tlsc6x_i2c_write(g_tlsc6x_client, writebuf, 3);

                writebuf[0] = 0x9f; //dbg_mtk_syn
                writebuf[1] = 0x24; //
                writebuf[2] = 0x01; //
                tlsc6x_i2c_write(g_tlsc6x_client, writebuf, 3);
		send_data_flag = 1;
}

static int tlsc6x_rawdata_test_3535allch(u8 * buf,int len)
{
	int ret;
	int retryTime;
	u8 writebuf[4];
	buf[len] = '\0';
	ret=0;

	ts_reset_controller_ex(ts_get_ts_data(), false);
	//tlsc6x_irq_disable();
	ts_enable_irq_ex(g_pdata, false);
	tlsc6x_esdHelperFreeze=1;
	//tlsc6x_tpd_reset();
	ts_reset_controller_ex(ts_get_ts_data(), true);
	printk("jiejianadd:tlsc6x_rawdata_test_3535allch\n");
	if (tlsc6x_load_ext_binlib((u8 *) &buf[2], len-2)){
	ret = -EIO;
	}
	msleep(30);

	mutex_lock(&i2c_rw_access);
	//write addr
	writebuf[0]= 0x9F;
	writebuf[1]= 0x20;
	writebuf[2]= 48;
	writebuf[3]= 0xFF;
	ret = tlsc6x_i2c_write_sub(g_tlsc6x_client, writebuf, 4);
	writebuf[0]= 0x9F;
	writebuf[1]= 0x24;
	writebuf[2]= 1;

	ret = tlsc6x_i2c_write_sub(g_tlsc6x_client, writebuf, 3);
	retryTime = 100;
	do {
		ret = tlsc6x_i2c_read_sub(g_tlsc6x_client, writebuf, 2, &writebuf[2], 1);
		if (ret < 0) {
			break;
		}
		retryTime--;
		msleep(30);
	} while (retryTime>0&&writebuf[2] == 1);

	if (ret>=0) {
		writebuf[0]= 0x9F;
		writebuf[1]= 0x26;
		ret = tlsc6x_i2c_read_sub(g_tlsc6x_client, writebuf, 2, proc_out_buf, 96);
		if (ret>=0){
			proc_out_len=96;
		}
	}

	mutex_unlock(&i2c_rw_access);
	ts_reset_controller_ex(ts_get_ts_data(), true);
	tlsc6x_esdHelperFreeze=0;
	//tlsc6x_irq_enable();
	ts_enable_irq_ex(g_pdata, true);
	return ret;
}
static ssize_t tlsc6x_proc_write(struct file *filp, const char __user *buff, size_t len, loff_t *data)
{
	int ret;
	int buflen = len;
	u8 writebuf[4];
	unsigned char *local_buf;
	if (buflen > 4100) {
		return -EFAULT;
	}
	local_buf = kmalloc(buflen+1, GFP_KERNEL);
	if(local_buf == NULL) {
		tlsc_err("%s,Can not malloc the buf!\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(local_buf, buff, buflen)) {
		tlsc_err("%s:copy from user error\n", __func__);
		kfree(local_buf);
		return -EFAULT;
	}
	ret = 0;
	debug_type = local_buf[0];
	/* format:cmd+para+data0+data1+data2... */
	switch (local_buf[0]) {
	case 0:		/* cfg version */
		proc_out_len = 4;
		proc_out_buf[0] = g_tlsc6x_cfg_ver;
		proc_out_buf[1] = g_tlsc6x_cfg_ver >> 8;
		proc_out_buf[2] = g_tlsc6x_cfg_ver >> 16;
		proc_out_buf[3] = g_tlsc6x_cfg_ver >> 24;
		break;
	case 1:
		local_buf[buflen] = '\0';
		if (tlsc6x_proc_cfg_update(&local_buf[2], 0)<0) {
			len = -EIO;
		}
		break;
	case 2:
		local_buf[buflen] = '\0';
		if (tlsc6x_proc_cfg_update(&local_buf[2], 1)<0) {
			len = -EIO;
		}
		break;
	case 3:
		ret = debug_write(&local_buf[1], len - 1);
		break;
	case 4:		/* read */
		reg_len = local_buf[1];
		iic_reg[0] = local_buf[2];
		iic_reg[1] = local_buf[3];
		break;
	case 5:		/* read with sync */
		ret = debug_write(&local_buf[1], 4);	/* write size */
		if (ret >= 0) {
			ret = debug_write(&local_buf[5], 4);	/* write addr */
		}
		sync_flag_addr[0] = local_buf[9];
		sync_flag_addr[1] = local_buf[10];
		sync_buf_addr[0] = local_buf[11];
		sync_buf_addr[1] = local_buf[12];
		break;
	case 8: // Force reset ic
		//tlsc6x_tpd_reset_force();
		ts_reset_controller_ex(ts_get_ts_data(), true);
		break;
	case 9: // Force reset ic
		ret=tlsc6x_rawdata_test_3535allch(local_buf,buflen);
		break;
	case 14:	/* e, esd control */
		tlsc6x_esdHelperFreeze = (int)local_buf[1];
		break;
        case 15:
		memcpy(tlsc6x_local_buf,local_buf,8);
		tlsc6x_esdHelperFreeze = 1;
                tlsc6x_set_dd_mode();
		{
                    // send addr
                    writebuf[0] = 0x9f; //0xc3; //flag
                    writebuf[1] = 0x22; //
                    writebuf[2] = local_buf[2]; //addr
                    writebuf[3] = local_buf[3]; //
                    tlsc6x_i2c_write(g_tlsc6x_client, writebuf, 4);

                    // send len
                    writebuf[0] = 0x9f; //dbg_mtk_syn
                    writebuf[1] = 0x20; //
                    writebuf[2] = 61; //local_buf[4]; //
                    //writebuf[3] = 0;//local_buf[5]; //
                    tlsc6x_i2c_write(g_tlsc6x_client, writebuf, 3);

                    writebuf[0] = 0x9f; //dbg_mtk_syn
                    writebuf[1] = 0x24; //
                    writebuf[2] = 0x01; //
                    tlsc6x_i2c_write(g_tlsc6x_client, writebuf, 3);
       		    send_data_flag = 1;
       		    send_count = 0;
       		    get_data_flag = 0;
       		    frame_cnt = 0;
		}
		break;
        case 16:
		writebuf[0] = 0x9f; //0xc3; //flag
		writebuf[1] = 0x22; //
		writebuf[2] = 0xff; //addr
		writebuf[3] = 0xff; //
		tlsc6x_i2c_write(g_tlsc6x_client, writebuf, 4);
		msleep(20);
		tlsc6x_esdHelperFreeze = 0;
                send_data_flag = 0;
                tlsc6x_set_nor_mode();
		break;

	default:
		break;
	}
	if (ret < 0) {
		len = ret;
	}
	kfree(local_buf);
	return len;
}

static ssize_t tlsc6x_proc_read(struct file *filp, char __user *page, size_t len, loff_t *pos)
{
	int ret = 0;
	if (*pos!=0) {
		return 0;
	}
	switch (debug_type) {
	case 0:		/* version information */
		proc_out_len = 4;
		proc_out_buf[0] = g_tlsc6x_cfg_ver;
		proc_out_buf[1] = g_tlsc6x_cfg_ver >> 8;
		proc_out_buf[2] = g_tlsc6x_cfg_ver >> 16;
		proc_out_buf[3] = g_tlsc6x_cfg_ver >> 24;
		if (copy_to_user(page, proc_out_buf, proc_out_len)) {
			ret = -EFAULT;
		} else {
			ret = proc_out_len;
		}
		break;
	case 1:
		break;
	case 2:
		break;
	case 3:
		break;
	case 4:
		len = debug_read(iic_reg, reg_len, proc_out_buf, len);
		if (len > 0 && len < sizeof(proc_out_buf)) {
			if (copy_to_user(page, proc_out_buf, len)) {
				ret = -EFAULT;
			} else {
				ret = len;
			}
		} else {
			ret = len;
		}
		break;
	case 5:
		len = debug_read_sync(iic_reg, reg_len, proc_out_buf, len);
		if (len > 0 && len < sizeof(proc_out_buf)) {
			if (copy_to_user(page, proc_out_buf, len)) {
				ret = -EFAULT;
			} else {
				ret = len;
			}
		} else {
			ret = len;
		}
		break;
	case 9:
		if (proc_out_buf>0){
			if (len > 0 && len < sizeof(proc_out_buf)) {
				if (copy_to_user(page, proc_out_buf, len)) {
					ret = -EFAULT;
				} else {
					ret = proc_out_len;
				}
			}else{
				ret = len;
			}
		}
		break;
        case 15:
		if (1 == get_data_flag){
			get_data_flag = 0;
			if (len > 0 && len < sizeof(buf_out)) {
				if (copy_to_user(page, buf_out, len)) {
					ret = -EFAULT;
				} else {
					ret = len;
				}
			}else{
				ret = len;
			}
		}
		else{
			ret = -EFAULT;
		}
		break;
	default:
		break;
	}
	if(ret>0) {
		*pos +=ret;
	}

	return ret;
}

static struct file_operations tlsc6x_proc_ops = {
	.owner = THIS_MODULE,
	.read = tlsc6x_proc_read,
	.write = tlsc6x_proc_write,
};

void tlsc6x_release_apk_debug_channel(void)
{
	if (tlsc6x_proc_entry) {
		remove_proc_entry("tlsc6x-debug", NULL);
	}
}

int tlsc6x_create_apk_debug_channel(struct i2c_client *client)
{
	tlsc6x_proc_entry = proc_create("tlsc6x-debug", 0777, NULL, &tlsc6x_proc_ops);

	if (tlsc6x_proc_entry == NULL) {
		dev_err(&client->dev, "Couldn't create proc entry!\n");
		return -ENOMEM;
	}
	dev_info(&client->dev, "Create proc entry success!\n");

	return 0;
}

#if 0
static ssize_t proc_selftest_read(struct file *filp, char __user *page, size_t len, loff_t *pos)
{
    char selftest_buffer[128] = {0};
    int read_len = 0 ;
    int ret ;
	if (*pos!=0) {
		return 0;
	}
    tlsc_factory_test(selftest_buffer , &read_len);
	if (copy_to_user(page, selftest_buffer, read_len))
    {
		ret = -EFAULT;
    }
    else
    {
       ret = read_len ;
    }

    if(ret>0) {
		*pos +=ret;
	}

	return ret;
}

static struct file_operations proc_selftest_ops = {
	.owner = THIS_MODULE,
	.read = proc_selftest_read,
};

static void release_selftest_proc(void)
{
	if (proc_selftest) {
		remove_proc_entry("tp_selftest", NULL);
	}
}

static int create_selftest_proc(struct i2c_client *client)
{
	proc_selftest = proc_create("tp_selftest", 0444, NULL, &proc_selftest_ops);

	if (proc_selftest == NULL) {
		dev_err(&client->dev, "Couldn't create proc selftest!\n");
		return -ENOMEM;
	}
	dev_info(&client->dev, "Create proc selftest success!\n");

	return 0;
}
#endif



#endif
#if defined(TS_ESD_SUPPORT_EN)
#include <linux/ktime.h>
#include <linux/kthread.h>
unsigned char g_tlsc6x_esdtar = 0x36;
static int tlsc6x_esd_status_flag = 0;
static struct hrtimer tlsc6x_esd_status_kthread_timer;
static DECLARE_WAIT_QUEUE_HEAD(tlsc6x_esd_status_waiter);

int tlsc6x_esd_condition(void){

	int ret = 0;

	//tlsc_info("tlsc6x_esdHelperFreeze=%d, auto_upd_busy=%d\n", tlsc6x_esdHelperFreeze, auto_upd_busy);
	if (tlsc6x_esdHelperFreeze)
		ret = 1;

#if (defined TPD_AUTO_UPGRADE_PATH) || (defined TLSC_APK_DEBUG)
	if (auto_upd_busy)
		ret = 1;
#endif

	return ret;
}
EXPORT_SYMBOL_GPL(tlsc6x_esd_condition);

int tlsc6x_esd_check_work(void){

	int ret = -1;
	u8 esdValue = 0;
	ktime_t ktime;

	if(g_pdata->tpm_status == TPM_SUSPENDED)
		return 0;

	ktime = ktime_set(1, 0);
	hrtimer_start(&tlsc6x_esd_status_kthread_timer, ktime, HRTIMER_MODE_REL);

	ret = tlsc6x_read_reg(g_tlsc6x_client, 0xa3, &esdValue);

	//���ESDʱI2C��SDA�������ˣ���ô��ȡ������ֵ��0��ret��ֵҲ��0��������ret<0
	if (esdValue != 0x36) {		/* maybe confused by some noise,so retry is make sense. */
		msleep(10);
		ret = tlsc6x_read_reg(g_tlsc6x_client, 0xa3, &esdValue);
		if(esdValue != 0x36){
			ret = tlsc6x_read_reg(g_tlsc6x_client, 0xa3, &esdValue);
			if(esdValue != 0x36){
				g_tlsc6x_esdtar = 0x36;
				tlsc_err("[IIC]: i2c_transfer1 error, addr= 0xa3 !!!\n");
			}
		}
	}

	//tlsc_info("g_tlsc6x_esdtar=0x%x, esdValue=0x%x\n", g_tlsc6x_esdtar, esdValue);
	if ((ret >= 0) && (g_tlsc6x_esdtar != 0)) {
		if (g_tlsc6x_esdtar != esdValue) {
			ret = -1;
		}
	}

	if (ret < 0) {
		/* re-power-on */
		//	tlsc6x_power_off(g_tp_drvdata->reg_vdd);
			msleep(20);
		//	tlsc6x_power_on(g_tp_drvdata->reg_vdd);

		ts_reset_controller_ex(ts_get_ts_data(), true);
		if (tlsc6x_read_reg(g_tlsc6x_client, 0xa3, &g_tlsc6x_esdtar) < 0) {
			g_tlsc6x_esdtar = 0x36;
			tlsc_err("[IIC]: i2c_transfer2 error, addr= 0xa3 !!!\n");
		}
	}

	hrtimer_cancel(&tlsc6x_esd_status_kthread_timer);

	return ret;
}

EXPORT_SYMBOL_GPL(tlsc6x_esd_check_work);

enum hrtimer_restart tlsc6x_esd_status_flag_kthread_hrtimer_func(struct hrtimer *timer)
{
	tlsc6x_esd_status_flag = 1;
	wake_up_interruptible(&tlsc6x_esd_status_waiter);

	return HRTIMER_NORESTART;
}

static void tlsc6x_esd_status_check(void) {

	pr_info(" entry!!!");
	ts_reset_controller_ex(g_pdata, true);
	hrtimer_cancel(&tlsc6x_esd_status_kthread_timer);
}

static int tlsc6x_esd_status_handler(void *unused) {

//	ktime_t ktime;

	do {
		wait_event_interruptible(tlsc6x_esd_status_waiter, tlsc6x_esd_status_flag != 0);
		tlsc6x_esd_status_flag = 0;

//		ktime = ktime_set(1, 0);
//		hrtimer_start(&tlsc6x_esd_status_kthread_timer, ktime, HRTIMER_MODE_REL);

		pr_info(" entry!!!");
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_TLSC6X)
		if(tlsc6x_esd_condition())
			continue;

		tlsc6x_esd_status_check();
#endif

	} while (!kthread_should_stop());

	return 0;
}
void tlsc6x_esd_status_init(void){

	if(g_pdata->board->esd_check)
	{/* esd issue: i2c monitor thread */
		ktime_t ktime = ktime_set(30, 0);

		hrtimer_init(&tlsc6x_esd_status_kthread_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		tlsc6x_esd_status_kthread_timer.function = tlsc6x_esd_status_flag_kthread_hrtimer_func;
		hrtimer_start(&tlsc6x_esd_status_kthread_timer, ktime, HRTIMER_MODE_REL);
		kthread_run(tlsc6x_esd_status_handler, 0, "tlsc6x_esd_status");
		pr_info("tlsc6x_esd_status init !!!");
	}

}
EXPORT_SYMBOL_GPL(tlsc6x_esd_status_init);
#endif

struct tlsc6x_controller {
	struct ts_controller controller;
	unsigned char a3;
	unsigned char a8;
	bool single_transfer_only;
};

int tlsc6x_i2c_read_sub(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen)
{
	int ret = 0;

	if (client == NULL) {
		tlsc_err("[IIC][%s]i2c_client==NULL!\n", __func__);
		return -EINVAL;
	}

	if (readlen > 0) {
		if (writelen > 0) {
			struct i2c_msg msgs[] = {
				{
				 .addr = client->addr,
				 .flags = 0,
				 .len = writelen,
				 .buf = writebuf,
				 },
				{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
				 },
			};
			ret = i2c_transfer(client->adapter, msgs, 1);
			if (ret < 0) {
				tlsc_err("[IIC]: i2c_transfer(2) error, addr= 0x%x!!\n", writebuf[0]);
				tlsc_err("[IIC]: i2c_transfer(2) error, ret=%d, rlen=%d, wlen=%d!!\n", ret, readlen, writelen);
			}
			else {
				ret = i2c_transfer(client->adapter, &msgs[1], 1);
				if (ret < 0) {
					tlsc_err("[IIC]: i2c_transfer(2) error, addr= 0x%x!!\n", writebuf[0]);
					tlsc_err("[IIC]: i2c_transfer(2) error, ret=%d, rlen=%d, wlen=%d!!\n", ret, readlen, writelen);
				}
			}
		} else {
			struct i2c_msg msgs[] = {
				{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
				 },
			};
			ret = i2c_transfer(client->adapter, msgs, 1);
			if (ret < 0) {
				tlsc_err("[IIC]: i2c_transfer(read) error, ret=%d, rlen=%d, wlen=%d!!", ret, readlen, writelen);
			}
		}
	}

	return ret;
}

int tlsc6x_i2c_read(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen)
{
	int ret = 0;

	/* lock in this function so we can do direct mode iic transfer in debug fun */
	mutex_lock(&i2c_mutex);
	ret = tlsc6x_i2c_read_sub(g_tlsc6x_client, writebuf, writelen, readbuf, readlen);

	mutex_unlock(&i2c_mutex);

	return ret;
}

int tlsc6x_i2c_write_sub(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret = 0;

	if (client == NULL) {
		tlsc_err("[IIC][%s]i2c_client==NULL!\n", __func__);
		return -EINVAL;
	}

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0) {
			tlsc_err("[IIC]: i2c_transfer(write) error, ret=%d!!\n", ret);
		}
	}

	return ret;

}

int tlsc6x_i2c_write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret = 0;

	mutex_lock(&i2c_mutex);
	ret = tlsc6x_i2c_write_sub(g_tlsc6x_client, writebuf, writelen);
	mutex_unlock(&i2c_mutex);

	mdelay(5);

	return ret;
}

int tlsc6x_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue)
{
	unsigned char buf[2] = { 0 };

	buf[0] = regaddr;
	buf[1] = regvalue;

	return tlsc6x_i2c_write(client, buf, sizeof(buf));
}

int tlsc6x_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue)
{
	return tlsc6x_i2c_read(client, &regaddr, 1, regvalue, 1);
}

static inline int tlsc6x_hid_to_i2c(void)
{
	unsigned char buf[3] = { 0xEB, 0xAA, 0x09 };
	ts_write(buf, 3);
	msleep(10);
	buf[0] = buf[1] = buf[2] = 0;
	ts_read(buf, 3);

	return buf[0] != 0xEB || buf[1] != 0xAA || buf[2] != 0x08;
}

/* firmware upgrade procedure */
static enum ts_result tlsc6x_upgrade_firmware(struct ts_controller *c,
	const unsigned char *data, size_t size, bool force)
{
  	return 1;
}
//static enum ts_result tlsc6x_upgrade_firmware(struct ts_controller *c,
//	const unsigned char *data, size_t size, bool force)
//static enum ts_result tlsc6x_upgrade_firmware(void)
int  tlsc6x_upgrade_status(struct ts_controller *c)
{
	return tlsc6x_tp_dect(g_tlsc6x_client);
}

#if 0
unsigned int tlsc6x_read_chipid(void){

	unsigned int chip_id = 0;
	bool err = false;

	g_tlsc6x_client = g_client;
        g_client->addr = 0x2E;
	err |= (1 != ts_reg_read(REG_CHIP_ID, (unsigned char*)&chip_id, 1));

	if(0x36 != chip_id) {

	}

	tlsc_info("chip_id=0x%x\n", chip_id);
	return chip_id;
}
#endif

/* read chip id and module id to match controller */
static enum ts_result tlsc6x_match(struct ts_controller *c)
{
	unsigned char chip_id = 0, module_id = 0;
	bool err = false;
         g_client->addr=0x2E;

	err |= (1 != ts_reg_read(REG_CHIP_ID, &chip_id, 1));
	err |= (1 != ts_reg_read(REG_MODULE_ID, &module_id, 1));
	tlsc_info("tlsc6x_match:a3:a8:0x%02x,0x%04x\n",chip_id,module_id);
	return (err) ? TSRESULT_NOT_MATCHED : TSRESULT_FULLY_MATCHED;
}

static int tlsc6x_fetch(struct ts_controller *c, struct ts_point *points)
{
	int i, j = 0;
	int p_num = 0;
	unsigned char buf[FT_ALL_DATA_LEN] = { 0 };

#ifdef TLSC_APK_DEBUG
        if(send_data_flag) {
		//send_count++;
		tssc_get_debug_info(g_tlsc6x_client,buf);
        }
        else {
    		/* read all bytes once! */
    		if (tlsc6x_i2c_read(g_tlsc6x_client, buf, 1, buf, FT_ALL_DATA_LEN) < 0) {
    			pr_err("failed to read data apk debug");
    			return -1;
    		}
        }
#else
	/* read all bytes once! */
	if (tlsc6x_i2c_read(g_tlsc6x_client, buf, 1, buf, FT_ALL_DATA_LEN) < 0) {
		pr_err("failed to read data");
		return -1;
	}
#endif

	c->pdata->ps_buf=buf[1];
	p_num = buf[FT_HEADER_LEN - 1]&0x07;
        c->pdata->touch_point=p_num;
	//tlsc_info("entry:tlsc6x_fetch:p_num:%d touch_point:%d\n", p_num, c->pdata->touch_point);

	/* check point number */
	if (p_num > FT_MAX_POINTS) {
		pr_warn("invalid point_num: %d, ignore this packet",
			buf[FT_HEADER_LEN - 1]);
		return -1;
	} else if (p_num == 0) {
		/* here we report UP event for all points for convenience */
		/* TODO: change to last point number */
		for (i = 0; i < FT_MAX_POINTS; i++) {
			points[i].pressed = 0;
			points[i].slot = i;
		}

		//pr_warn("p_num == 0 ...\n");
		return FT_MAX_POINTS;
	}

	/* read one more point to ensure getting data for all points */
	p_num++;
	if (p_num > FT_MAX_POINTS)
		p_num = FT_MAX_POINTS;

	/* calculate points */
	for (i = 0; i < p_num; i++) {

		/* filter out invalid points */
		if ((buf[FT_HEADER_LEN+FT_POINT_LEN*i] & 0xc0) == 0xc0)
			continue;
		points[j].x = ((buf[FT_HEADER_LEN+FT_POINT_LEN*i] & 0x0f) << 8)
			| buf[FT_HEADER_LEN+FT_POINT_LEN*i+1];
		points[j].y = ((buf[FT_HEADER_LEN+FT_POINT_LEN*i+2] & 0x0f) << 8)
			| buf[FT_HEADER_LEN+FT_POINT_LEN*i+3];
		points[j].pressure = buf[FT_HEADER_LEN+FT_POINT_LEN*i+4];
		points[j].pressed = !(buf[FT_HEADER_LEN+FT_POINT_LEN*i] & 0x40);
		points[j].slot = buf[FT_HEADER_LEN+FT_POINT_LEN*i+2] >> 4;
		j++;
		//TS_DBG("points[j].x:points[j].y:%d,%d\n",points[j].x,points[j].y);
	}

	return j;
}

static  int tlsc6x_check_gesture(struct ts_controller *c, int gesture_id)
{
    int keycode = 0;

    tlsc_info("kaka gesture_id==0x%x\n ",gesture_id);
    switch(gesture_id){
        case GESTURE_LEFT:
            keycode = KEY_LEFT;
            break;
        case GESTURE_RIGHT:
            keycode = KEY_RIGHT;
            break;
        case GESTURE_UP:
            keycode = KEY_UP;
            break;
        case GESTURE_DOWN:
            keycode = KEY_DOWN;
            break;
        case GESTURE_DOUBLECLICK:
            keycode = KEY_POWER;    //KEY_POWER;//
            break;
        case GESTURE_O:
            keycode = KEY_O;
            break;
        case GESTURE_W:
            keycode = KEY_W;
            break;
        case GESTURE_M:
            keycode = KEY_M;
            break;
        case GESTURE_E:
            keycode = KEY_E;
            break;
        case GESTURE_C:
            keycode = KEY_C;
            break;
        case GESTURE_S:
            keycode = KEY_S;
            break;
         case GESTURE_V:
            keycode = KEY_V;
            break;
        case GESTURE_Z:
            keycode = KEY_UP;
            break;
        case GESTURE_L:
            keycode = KEY_L;
            break;
        default:
            break;
    }
    if(keycode){
	tlsc_info("keycode:%d\n",keycode);
        input_report_key(c->pdata->input, keycode, 1);
        input_sync(c->pdata->input);
        input_report_key(c->pdata->input, keycode, 0);
        input_sync(c->pdata->input);
    }
    return keycode;
}

static unsigned char tlsc6x_gesture_readdata(struct ts_controller *c)
{
    int ret = -1 ;
	int gestrue_id = 0;
	u8 buf[4] = {0xd3, 0xd3};

	ret = tlsc6x_i2c_read(g_tlsc6x_client, buf, 1, buf, 2);
	if(ret < 0){
		tlsc_err("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
		return 1;
	}
	if(buf[1] != 0){
		gestrue_id = 0x24;
	}else{
		gestrue_id = buf[0];
	}
	tlsc_info("gestrue_id:%d\n",gestrue_id);
	tlsc6x_check_gesture(c, gestrue_id);

	return 0;
}

static  void tlsc6x_gesture_init(struct ts_controller *c)
{

	input_set_capability(c->pdata->input, EV_KEY, KEY_POWER);
	input_set_capability(c->pdata->input, EV_KEY, KEY_U);
	input_set_capability(c->pdata->input, EV_KEY, KEY_UP);
	input_set_capability(c->pdata->input, EV_KEY, KEY_DOWN);
	input_set_capability(c->pdata->input, EV_KEY, KEY_LEFT);
	input_set_capability(c->pdata->input, EV_KEY, KEY_RIGHT);
	input_set_capability(c->pdata->input, EV_KEY, KEY_O);
	input_set_capability(c->pdata->input, EV_KEY, KEY_E);
	input_set_capability(c->pdata->input, EV_KEY, KEY_M);
	input_set_capability(c->pdata->input, EV_KEY, KEY_L);
	input_set_capability(c->pdata->input, EV_KEY, KEY_W);
	input_set_capability(c->pdata->input, EV_KEY, KEY_S);
	input_set_capability(c->pdata->input, EV_KEY, KEY_V);
	input_set_capability(c->pdata->input, EV_KEY, KEY_Z);
	input_set_capability(c->pdata->input, EV_KEY, KEY_C);

	__set_bit(KEY_LEFT,  c->pdata->input->keybit);
	__set_bit(KEY_RIGHT,  c->pdata->input->keybit);
	__set_bit(KEY_UP,  c->pdata->input->keybit);
	__set_bit(KEY_DOWN,  c->pdata->input->keybit);
	__set_bit(KEY_D,  c->pdata->input->keybit);
	__set_bit(KEY_O,  c->pdata->input->keybit);
	__set_bit(KEY_W,  c->pdata->input->keybit);
	__set_bit(KEY_M,  c->pdata->input->keybit);
	__set_bit(KEY_E,  c->pdata->input->keybit);
	__set_bit(KEY_C,  c->pdata->input->keybit);
	__set_bit(KEY_S,  c->pdata->input->keybit);
	__set_bit(KEY_V,  c->pdata->input->keybit);
	__set_bit(KEY_Z,  c->pdata->input->keybit);
}

static  int tlsc6x_gesture_exit(struct ts_controller *c)
{
	tlsc6x_write_reg(g_client, 0xd0, 0x00);
	return 0;
}

static  int tlsc6x_gesture_suspend(struct ts_controller *c)
{
	unsigned char temp = 0;
	c->pdata->gesture_enable=1;

	if(c->pdata->gesture_enable == 1){
		c->pdata->gesture_state= 0x01;
		temp = 0x01;
		tlsc6x_write_reg(g_client, 0xd0, 0x01);
		return 0;
	}

	return 0;
 }

static  int tlsc6x_gesture_resume(struct ts_controller *c)
{
	unsigned char temp = 0;

	if(c->pdata->gesture_enable== 1){
		temp = 0x00;
		c->pdata->gesture_state =0;
		tlsc6x_write_reg(g_client, 0xd0, 0x00);
	}

	return 0;
}

static enum ts_result tlsc6x_handle_event(
	struct ts_controller *c, enum ts_event event, void *data)
{
	int retvel = -1;
	struct tlsc6x_controller *ftc = to_tlsc6x_controller(c);
	struct device_node *pn = NULL;

	switch (event) {
	case TSEVENT_POWER_ON:
		if (data) {
			pn = (struct device_node *)data;
			if (!of_property_read_u8(pn, "a8", &ftc->a8))
				TS_DBG("parse a8 value: 0x%02X", ftc->a8);
			ftc->single_transfer_only = !!of_get_property(pn,
				"single-transfer-only", NULL);
			if (ftc->single_transfer_only)
				TS_DBG("single transfer only");
		}
        #if 0
		if ((ts_reg_read(REG_CHIP_ID, &a3, 1) != 1)
			|| (ts_reg_read(REG_MODULE_ID, &a8, 1) != 1)
			|| (a3 != ftc->a3))
			ret = TSRESULT_EVENT_NOT_HANDLED;
		TS_DBG("read a8 value from chip: 0x%02X", a8);
        #endif
		break;
	case TSEVENT_SUSPEND:
		//ts_reg_write(REG_POWER_MODE, &val, 1);
		retvel = tlsc6x_write_reg(g_client, 0xa5, 0x03);
		if (retvel < 0) {
			tlsc_err("tlsc6x error::setup suspend fail!\n");
		}
		break;
	case TSEVENT_RESUME:
		//ts_reset_controller_ex(g_pdata, true);
		break;
	case TSEVENT_NOISE_HIGH:
		//ts_reg_write(REG_CHARGER_INDICATOR, &val, 1);
		mdelay(5);
		tlsc6x_write_reg(g_client, REG_CHARGER_INDICATOR, 0x01);
		break;
	case TSEVENT_NOISE_NORMAL:
		mdelay(5);
                tlsc6x_write_reg(g_client, REG_CHARGER_INDICATOR, 0x00);
		//ts_reg_write(REG_CHARGER_INDICATOR, &val, 1);
		break;
	default:
		break;
	}

	return TSRESULT_EVENT_HANDLED;
}

static void tlsc6x_ps_reset(void){

	unsigned char buf[FT_ALL_DATA_LEN] = { 0 };
	tlsc6x_write_reg(g_client, 0xb0, 0x01);
	tlsc6x_read_reg(g_client, 0x01, &buf[1]);
	printk("[TS] tlsc6x reset ps mode...\n");
}

static const struct ts_virtualkey_info tlsc6x_virtualkeys[] = {
	DECLARE_VIRTUALKEY(120, 1500, 60, 45, KEY_BACK),
	DECLARE_VIRTUALKEY(360, 1500, 60, 45, KEY_HOMEPAGE),
	DECLARE_VIRTUALKEY(600, 1500, 60, 45, KEY_APPSELECT),
};

static const struct ts_register_info tlsc6x_registers[] = {
	DECLARE_REGISTER(TSREG_CHIP_ID, REG_CHIP_ID),
	DECLARE_REGISTER(TSREG_MOD_ID, REG_MODULE_ID),
	DECLARE_REGISTER(TSREG_FW_VER, REG_FIRMWARE_VERSION),
	DECLARE_REGISTER("frequency", REG_SCANNING_FREQ),
	DECLARE_REGISTER("charger_indicator", REG_CHARGER_INDICATOR),
};

static int tlsc6x_ps_resume(struct ts_data *pdata){

	if (ts_get_mode_ext(pdata, TSMODE_PS_STATUS)||ts_get_mode_ext(pdata, TSMODE_SENSORHUB_STATUS)){
		if (pdata->tpd_prox_active && (pdata->tpm_status == TPM_DESUSPEND)) {
			u8 temp = 0xa0;

			ts_reg_write(0xa0, &temp, 1);
			printk("[TS] entry:ps_resume ps is on, so return !!!\n");
			return 0;
		}
	}

	ts_reset_controller_ex(pdata, true);

#ifdef TLSC_APK_DEBUG
	if(send_data_flag){
		mdelay(30);
		tlsc6x_apkdebug_mode();
	}
#endif

	if (ts_get_mode_ext(pdata, TSMODE_PS_STATUS)||ts_get_mode_ext(pdata, TSMODE_SENSORHUB_STATUS)){
		if(pdata->tps_status == TPS_DEON && pdata->tpd_prox_active){
			u8 temp = 1;
			ts_reg_write(0xb0, &temp, 1);

			pdata->tps_status = TPS_ON ;
		}
	}

	return 1;
}

static int tlsc6x_ps_suspend(struct ts_data *pdata){

	if (ts_get_mode_ext(pdata, TSMODE_PS_STATUS)||ts_get_mode_ext(pdata, TSMODE_SENSORHUB_STATUS)){
		if (pdata->tpd_prox_active) {

			printk("[TS] ps_suspend:ps is on, so return!!!\n");
			return 0;
		}
	}

	return 1;
}

static void tlsc6x_proximity_switch(bool onoff){

	u8 temp=1;

	if(onoff)//proximity on
		ts_reg_write(0xb0, &temp, 1);
	else{//proximity off
		temp=0;
		ts_reg_write(0xb0, &temp, 1);
	}
}

static void tlsc6x_ps_irq_handler(struct ts_data *pdata){

	pdata->tpd_prox_old_state = pdata->ps_buf;
}

static struct tlsc6x_controller tlsc6x = {
	.controller = {
		.name = "TLSC6X",
		.vendor = "tlsc6x",
		.config = TSCONF_ADDR_WIDTH_BYTE
			| TSCONF_POWER_ON_RESET
			| TSCONF_RESET_LEVEL_LOW
			| TSCONF_REPORT_MODE_IRQ
			| TSCONF_IRQ_TRIG_EDGE_FALLING
			| TSCONF_REPORT_TYPE_3,
		.addr_count = ARRAY_SIZE(tlsc6x_addrs),
		.addrs = tlsc6x_addrs,
		.virtualkey_count = ARRAY_SIZE(tlsc6x_virtualkeys),
		.virtualkeys = tlsc6x_virtualkeys,
		.register_count = ARRAY_SIZE(tlsc6x_registers),
		.registers = tlsc6x_registers,
		.panel_width = 720,
		.panel_height = 1560,
		.reset_keep_ms = 20,
		.reset_delay_ms = 30,
		.parser = {
		},
		.ps_reset = tlsc6x_ps_reset,
		.custom_initialization = NULL,
		.match = tlsc6x_match,
		.fetch_points = tlsc6x_fetch,
		.handle_event = tlsc6x_handle_event,
		.upgrade_firmware = tlsc6x_upgrade_firmware,
		.upgrade_status=tlsc6x_upgrade_status,
		.gesture_readdata = tlsc6x_gesture_readdata,
		.gesture_init = tlsc6x_gesture_init,
		.gesture_exit = tlsc6x_gesture_exit,
		.gesture_suspend = tlsc6x_gesture_suspend,
		.gesture_resume = tlsc6x_gesture_resume,
		.ps_resume = tlsc6x_ps_resume,
		.ps_suspend = tlsc6x_ps_suspend,
		.proximity_switch = tlsc6x_proximity_switch,
		.ps_irq_handler = tlsc6x_ps_irq_handler,
#ifdef CONFIG_FACTORY_TEST_EN
		.ts_factory_testing = tlsc_factory_test,
#endif
	},
	.a3 = 0x54,
	.a8 = 0x87,
	.single_transfer_only = false,
};

int tlsc6x_init(void)
{
	ts_register_controller(&tlsc6x.controller);
	tlsc6x_create_apk_debug_channel(g_tlsc6x_client);
//    create_selftest_proc(g_tlsc6x_client);
	return 0;
}

void tlsc6x_exit(void)
{
	ts_unregister_controller(&tlsc6x.controller);
	tlsc6x_release_apk_debug_channel();
  //  release_selftest_proc();
}

//REGISTER_CONTROLLER(tlsc6x.controller);

//module_init(tlsc6x_init);
//module_exit(tlsc6x_exit);

MODULE_AUTHOR("joseph.cai@spreadtrum.com");
MODULE_DESCRIPTION("Spreadtrum touchscreen driver for Focaltech");
MODULE_LICENSE("GPL");
