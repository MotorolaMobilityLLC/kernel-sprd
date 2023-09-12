/*
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * VERSION		DATE			AUTHOR
 *
 */
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/input/mt.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/err.h>
#include <linux/suspend.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include "ats_tlsc6x_main.h"
#include "ats_tlsc6x_update.h"
#include "ats_tlsc6x_gesture_binlib.h"
#include "ats_tlsc6x_boot.h"
#include "ats_core.h"

extern unsigned char tlsc6x_chip_name[MAX_CHIP_ID][20];
unsigned int g_tlsc6x_cfg_ver = 0;
unsigned int g_tlsc6x_boot_ver = 0;
unsigned int g_needKeepRamCode = 0;//// diff 3535  3536
unsigned short g_tlsc6x_chip_code = 0;

int tlsc_boot_version=-1;
int tlsc_cfg_version=-1;
int tlsc_vendor_id=-1;
int tlsc_project_id=-1;

unsigned int g_mccode = 0;	/* 0:3535, 1:3536 */

struct i2c_client *g_tlsc6x_client;
extern struct mutex i2c_rw_access;

struct ts_firmware ts_fw;
//#ifdef TLSC_AUTO_UPGRADE
static int new_idx_active = -1;  // diff 3535  3536
unsigned short  buidIn_tls_tp_lut[][102] = {
         {
		0x3A07,
		0x100E,
		0x3303,
		0x5332,
		0x0337,
		0x1F02,
		0xA175,
		0x0000,
		0x0000,
		0x5555,
		0x0000,
		0x1B00,
		0x0026,
		0x1A24,
		0x1923,
		0x1822,
		0x0000,
		0x1721,
		0x1420,
		0x0710,
		0x0811,
		0x0912,
		0x1F16,
		0x0000,
		0x1E15,
		0x040D,
		0x050E,
		0x060F,
		0x131D,
		0x0A1C,
		0x0000,
		0x0B01,
		0x0C02,
		0x0025,
		0x0003,
		0x9A48,
		0x026E,
		0x0045,
		0x02D0,
		0x05A0,
		0x0050,
		0x00F0,
		0x0190,
		0x0200,
		0x0384,
		0x0B0B,
		0x9696,
		0xB4B4,
		0x4064,
		0x8408,
		0x1E06,
		0x2828,
		0x2828,
		0x065C,
		0x0797,
		0x3D3D,
		0x2121,
		0xAD67,
		0x261B,
		0x3901,
		0x1549,
		0xAF86,
		0x649A,
		0x0191,
		0x5CC8,
		0x2524,
		0x1125,
		0x3A2D,
		0x3273,
		0x4D9F,
		0x28AA,
		0x5FEB,
		0x0D5A,
		0xA282,
		0x0614,
		0x0814,
		0x0600,
		0x07EB,
		0x05AD,
		0x076E,
		0x0629,
		0x0867,
		0x0101,
		0x3139,
		0x3139,
		0x0000,
		0x0000,
		0x02D5,
		0x200E,
		0x0B0B,
		0x2428,
		0x0350,
		0x3C29,
		0x141E,
		0x1114,
		0x1511,
		0x1716,
		0x140B,
		0x1114,
		0x1411,
		0x1414,
		0xC035
	}
};
//#endif

/* Telink CTP */
unsigned int MTK_TXRX_BUF;
unsigned int CMD_ADDR;
unsigned int RSP_ADDR;

typedef struct __test_cmd_wr {
	/* offset 0; */
	unsigned char id;	/* cmd_id; */
	unsigned char idv;	/* inverse of cmd_id */
	unsigned short d0;	/* data 0 */
	unsigned short d1;	/* data 1 */
	unsigned short d2;	/* data 2 */
	/* offset 8; */
	unsigned char resv;	/* offset 8 */
	unsigned char tag;	/* offset 9 */
	unsigned short chk;	/* 16 bit checksum */
	unsigned short s2Pad0;	/*  */
	unsigned short s2Pad1;	/*  */
} ctp_test_wr_t;

typedef struct __test_cmd_rd {
	/* offset 0; */
	unsigned char id;	/* cmd_id; */
	unsigned char cc;	/* complete code */
	unsigned short d0;	/* data 0 */
	unsigned short sn;	/* session number */
	unsigned short chk;	/* 16 bit checksum */
} ctp_test_rd_t;
#define DIRECTLY_MODE   (0x0)
#define DEDICATE_MODE   (0x1)
#define MAX_TRX_LEN (64)	/* max IIC data length */
/* #define CMD_ADDR    (0xb400) */
/* #define RSP_ADDR    (0xb440) */
/* #define MTK_TXRX_BUF    (0xcc00)  // 1k, buffer used for memory r &w */

#define LEN_CMD_CHK_TX  (10)
#define LEN_CMD_PKG_TX  (16)

#define LEN_RSP_CHK_RX  (8)
#define MAX_BULK_SIZE    (1024)
unsigned short tl_target_cfg[102];
unsigned short tl_buf_tmpcfg[102];
/* to direct memory access mode */
unsigned char cmd_2dma_42bd[6] = { /*0x42, 0xbd, */ 0x28, 0x35, 0xc1, 0x00, 0x35, 0xae };

/* in directly memory access mode */
/* RETURN:0->pass else->fail */
int tlsc6x_read_bytes_u16addr_sub(struct i2c_client *client, u16 addr, u8 *rxbuf, u16 len)
{
	int err = 0;
	int retry = 0;
	u16 offset = 0;
	u8 buffer[2];

	struct i2c_msg msgs[2] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 2,	/* 16bit memory address */
		 .buf = buffer,
		 },
		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 },
	};

	if (rxbuf == NULL) {
		return -EPERM;
	}
	/* mutex_lock(&g_mutex_i2c_access); */

	while (len > 0) {
		buffer[0] = (u8) ((addr + offset) >> 8);
		buffer[1] = (u8) (addr + offset);

		msgs[1].buf = &rxbuf[offset];
		if (len > MAX_TRX_LEN) {
			len -= MAX_TRX_LEN;
			msgs[1].len = MAX_TRX_LEN;
		} else {
			msgs[1].len = len;
			len = 0;
		}

		retry = 0;
		while (tlsc6x_i2c_read_sub(client, buffer, 2, &rxbuf[offset], msgs[1].len) < 0) {
			if (retry++ == 3) {
				err = -1;
				break;
			}
		}
		offset += MAX_TRX_LEN;
		if (err < 0) {
			break;
		}
	}

	/* mutex_unlock(&g_mutex_i2c_access); */

	return err;
}

int tlsc6x_read_bytes_u16addr(struct i2c_client *client, u16 addr, u8 *rxbuf, u16 len)
{
	int ret = 0;

	mutex_lock(&i2c_rw_access);
	ret = tlsc6x_read_bytes_u16addr_sub(client, addr, rxbuf, len);
	mutex_unlock(&i2c_rw_access);
	return ret;
}

/* in directly memory access mode */
/* RETURN:0->pass else->fail */
int tlsc6x_write_bytes_u16addr_sub(struct i2c_client *client, u16 addr, u8 *txbuf, u16 len)
{
	u8 buffer[MAX_TRX_LEN];
	u16 offset = 0;
	u8 retry = 0;
	int err = 0;

	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.buf = buffer,
	};

	if (txbuf == NULL) {
		return -EPERM;
	}
	/* mutex_lock(&g_mutex_i2c_access); */

	while (len) {
		buffer[0] = (u8) ((addr + offset) >> 8);
		buffer[1] = (u8) (addr + offset);

		if (len > (MAX_TRX_LEN - 2)) {	/* (sizeof(addr)+payload) <= MAX_TRX_LEN */
			memcpy(&buffer[2], &txbuf[offset], (MAX_TRX_LEN - 2));
			len -= (MAX_TRX_LEN - 2);
			offset += (MAX_TRX_LEN - 2);
			msg.len = MAX_TRX_LEN;
		} else {
			memcpy(&buffer[2], &txbuf[offset], len);
			msg.len = len + 2;
			len = 0;
		}

		retry = 0;
		while (tlsc6x_i2c_write_sub(client, buffer, msg.len) < 0) {
			if (retry++ == 3) {
				err = -1;
				break;
			}
		}
		if (err < 0) {
			break;
		}
	}

	/* mutex_unlock(&g_mutex_i2c_access); */

	return err;
}

int tlsc6x_write_bytes_u16addr(struct i2c_client *client, u16 addr, u8 *txbuf, u16 len)
{
	int ret = 0;

	mutex_lock(&i2c_rw_access);
	ret = tlsc6x_write_bytes_u16addr_sub(client, addr, txbuf, len);
	mutex_unlock(&i2c_rw_access);
	return ret;
}

/* <0 : i2c error */
/* 0: direct address mode */
/* 1: protect mode */
int tlsc6x_get_i2cmode(void)
{
	u8 regData[4];

	if (tlsc6x_read_bytes_u16addr_sub(g_tlsc6x_client, 0x01, regData, 3)) {
		return -EPERM;
	}

	if (((u8) (g_tlsc6x_client->addr) == (regData[0] >> 1)) && (regData[2] == 0X01)) {
		return DIRECTLY_MODE;
	}

	return DEDICATE_MODE;
}

/* 0:successful */
int tlsc6x_set_dd_mode_sub(void)
{
	int mod = -1;
	int retry = 0;
	int ret = 0;
	ret = tlsc6x_get_i2cmode();
	if (ret < 0) {
		return ret;
	}
	if (ret == DIRECTLY_MODE) {
		return 0;
	}

	while (retry++ < 5) {
		mdelay(20);
		tlsc6x_write_bytes_u16addr_sub(g_tlsc6x_client, 0x42bd, cmd_2dma_42bd, 6);
		mdelay(30);
		mod = tlsc6x_get_i2cmode();
		if (mod == DIRECTLY_MODE) {
			break;
		}
	}

	if (mod == DIRECTLY_MODE) {
		return 0;
	} else {
		return -EPERM;
	}
}

int tlsc6x_set_dd_mode(void)
{
	int ret = 0;

	mutex_lock(&i2c_rw_access);
	ret = tlsc6x_set_dd_mode_sub();
	mutex_unlock(&i2c_rw_access);
	return ret;
}

/* 0:successful */
int tlsc6x_set_nor_mode_sub(void)
{
	int mod = -1;
	int retry = 0;
	u8 reg = 0x05;

	while (retry++ < 5) {
		tlsc6x_write_bytes_u16addr_sub(g_tlsc6x_client, 0x03, &reg, 1);
		usleep_range(5000, 5500);
		mod = tlsc6x_get_i2cmode();
		if (mod == DEDICATE_MODE) {
			break;
		}
		mdelay(50);
	}
	if (mod == DEDICATE_MODE) {
		return 0;
	} else {
		return -EPERM;
	}
}

int tlsc6x_set_nor_mode(void)
{
	int ret = 0;

	mutex_lock(&i2c_rw_access);
	ret = tlsc6x_set_nor_mode_sub();
	mutex_unlock(&i2c_rw_access);
	return ret;
}

/* ret=0 : successful */
/* write with read-back check, in dd mode */
static int tlsc6x_bulk_down_check(u8 *pbuf, u16 addr, u16 len)
{
	unsigned int j, k, retry;
	u8 rback[128];

	TLSC_FUNC_ENTER();
	while (len) {
		k = (len < 128) ? len : 128;
		retry = 0;
		do {
			rback[k - 1] = pbuf[k - 1] + 1;
			tlsc6x_write_bytes_u16addr(g_tlsc6x_client, addr, pbuf, k);
			tlsc6x_read_bytes_u16addr(g_tlsc6x_client, addr, rback, k);
			for (j = 0; j < k; j++) {
				if (pbuf[j] != rback[j]) {
					break;
				}
			}
			if (j >= k) {
				break;	/* match */
			}
		} while (++retry < 3);

		if (j < k) {
			break;
		}

		addr += k;
		pbuf += k;
		len -= k;
	}

	return (int)len;
}

static u16 tlsc6x_checksum_u16(u16 *buf, u16 length)
{
	unsigned short sum, len, i;

	sum = 0;

	len = length >> 1;

	for (i = 0; i < len; i++) {
		sum += buf[i];
	}

	return sum;
}

static u32 tlsc6x_checksumEx(u8 *buf, u16 length)
{
	u32 combChk;
	u16 k, check, checkEx;

	check = 0;
	checkEx = 0;
	for (k = 0; k < length; k++) {
		check += buf[k];
		checkEx += (u16) (k * buf[k]);
	}
	combChk = (checkEx << 16) | check;

	return combChk;

}

/* 0:successful */
int tlsc6x_download_ramcode(u8 *pcode, u16 len)
{
	u8 dwr, retry;
	int ret = -2;
	int sig;

	TLSC_FUNC_ENTER();
	if (tlsc6x_set_dd_mode()) {
		return -EPERM;
	}

	sig = (int) pcode[3];
	sig = (sig<<8) + (int) pcode[2];
	sig = (sig<<8) + (int) pcode[1];
	sig = (sig<<8) + (int) pcode[0];

	if (sig == 0x6d6f8008) {
		sig = 0;
		tlsc6x_read_bytes_u16addr(g_tlsc6x_client, 0x8000, (u8 *) &sig, 4);
		if (sig == 0x6d6f8008) {
			return 0;
		}
	}

	dwr = 0x05;
	if (tlsc6x_bulk_down_check(&dwr, 0x0602, 1) == 0) {	/* stop mcu */
		dwr = 0x00;
		tlsc6x_bulk_down_check(&dwr, 0x0643, 1);	/* disable irq */
	} else {
		return -EPERM;
	}
	if (tlsc6x_bulk_down_check(pcode, 0x8000, len) == 0) {
		dwr = 0x88;
		retry = 0;
		do {
			ret = tlsc6x_write_bytes_u16addr(g_tlsc6x_client, 0x0602, &dwr, 1);
		} while ((++retry < 3) && (ret != 0));
	}
	/* mdelay(50);   // let caller decide the delay time */

	return ret;
}

/* return 0=successful: send cmd and get rsp. */
static int tlsc6x_cmd_send(ctp_test_wr_t *ptchcw, ctp_test_rd_t *pcr)
{
	int ret;
	u32 retry;

	TLSC_FUNC_ENTER();
	retry = 0;
	tlsc6x_write_bytes_u16addr(g_tlsc6x_client, RSP_ADDR, (u8 *) &retry, 1);

	/* send command */
	ptchcw->idv = ~(ptchcw->id);
	ptchcw->tag = 0x35;
	ptchcw->chk = 1 + ~(tlsc6x_checksum_u16((u16 *) ptchcw, LEN_CMD_CHK_TX));
	ptchcw->tag = 0x30;
	ret = tlsc6x_write_bytes_u16addr(g_tlsc6x_client, CMD_ADDR, (u8 *) ptchcw, LEN_CMD_PKG_TX);
	if (ret) {
		goto exit;
	}
	ptchcw->tag = 0x35;
	ret = tlsc6x_write_bytes_u16addr(g_tlsc6x_client, CMD_ADDR + 9, (u8 *) &(ptchcw->tag), 1);
	if (ret) {
		goto exit;
	}
	/* polling rsp, the caller must init rsp buffer. */
	ret = -1;
	retry = 0;
	while (retry++ < 100) {	/* 2s */
		mdelay(20);
		if (tlsc6x_read_bytes_u16addr(g_tlsc6x_client, RSP_ADDR, (u8 *) pcr, 1)) {
			break;
		}

		if (ptchcw->id != pcr->id) {
			continue;
		}
		/* mdelay(50); */
		tlsc6x_read_bytes_u16addr(g_tlsc6x_client, RSP_ADDR, (u8 *) pcr, LEN_RSP_CHK_RX);
		if (!tlsc6x_checksum_u16((u16 *) pcr, LEN_RSP_CHK_RX)) {
			if ((ptchcw->id == pcr->id) && (pcr->cc == 0)) {
				ret = 0;
			}
		}
		break;
	}
exit:
	/* clean rsp buffer */
	/* retry = 0; */
	/* tlsc6x_write_bytes_u16addr(g_tlsc6x_client, RSP_ADDR, (u8*)&retry, 1); */

	return ret;

}

/* return 0=successful */
int tlsc6x_read_burn_space(u8 *pdes, u16 adr, u16 len)
{
	int rsp;
	u32 left = len;
	u32 combChk, retry;
	ctp_test_wr_t m_cmd;
	ctp_test_rd_t m_rsp;

	TLSC_FUNC_ENTER();
	m_cmd.id = 0x31;
	m_cmd.resv = 0x03;
	while (left) {
		len = (left > MAX_BULK_SIZE) ? MAX_BULK_SIZE : left;

		m_cmd.d0 = adr;
		m_cmd.d1 = len;

		rsp = -1;
		retry = 0;
		while (retry++ < 3) {
			m_rsp.id = 0;
			if (tlsc6x_cmd_send(&m_cmd, &m_rsp) == 0X0) {
				tlsc6x_read_bytes_u16addr(g_tlsc6x_client, MTK_TXRX_BUF, pdes, len);
				combChk = tlsc6x_checksumEx(pdes, len);
				if (m_rsp.d0 == (unsigned short)combChk) {
					if (m_rsp.sn == (unsigned short)(combChk >> 16)) {
						rsp = 0;
						break;
					}
				}
			}
		}

		if (rsp < 0) {
			break;
		}
		left -= len;
		adr += len;
		pdes += len;
	}

	return rsp;
}

int tlsc6x_write_burn_space(u8 *psrc, u16 adr, u16 len)
{
	int rsp = 0;
	u16 left = len;
	u32 retry, combChk;
	ctp_test_wr_t m_cmd;
	ctp_test_rd_t m_rsp;

	TLSC_FUNC_ENTER();
	m_cmd.id = 0x30;
	m_cmd.resv = 0x11;

	while (left) {
		len = (left > MAX_BULK_SIZE) ? MAX_BULK_SIZE : left;
		combChk = tlsc6x_checksumEx(psrc, len);

		m_cmd.d0 = adr;
		m_cmd.d1 = len;
		m_cmd.d2 = (u16) combChk;
		m_cmd.s2Pad0 = (u16) (combChk >> 16);

		rsp = -1;	/* avoid dead loop */
		retry = 0;
		while (retry < 3) {
			tlsc6x_write_bytes_u16addr(g_tlsc6x_client, MTK_TXRX_BUF, psrc, len);
			m_rsp.id = 0;
			rsp = tlsc6x_cmd_send(&m_cmd, &m_rsp);
			if (rsp < 0) {
				if ((m_rsp.d0 == 0X05) && (m_rsp.cc == 0X09)) {	/* fotal error */
					break;
				}
				retry++;
			} else {
				left -= len;
				adr += len;
				psrc += len;
				break;
			}
		}

		if (rsp < 0) {
			break;
		}
	}

	return (!left) ? 0 : -1;
}

static int is_valid_cfg_data(u16 *ptcfg)
{
	if (ptcfg == NULL) {
		return 0;
	}

	if ((u8) ((ptcfg[53] >> 1) & 0x7f) != (u8) (g_tlsc6x_client->addr)) {
		return 0;
	}

	if (tlsc6x_checksum_u16(ptcfg, 204)) {
		return 0;
	}

	return 1;
}

//#ifdef TLSC_AUTO_UPGRADE
static int tlsc6x_tpcfg_ver_comp(unsigned short *ptcfg)
{
	unsigned int u32tmp;
	unsigned short vnow, vbuild;

	TLSC_FUNC_ENTER();
	if (g_tlsc6x_cfg_ver == 0) {	/* no available version information */
		return 0;
	}

	if (is_valid_cfg_data(ptcfg) == 0) {
		return 0;
	}

	u32tmp = ptcfg[1];
	u32tmp = (u32tmp << 16) | ptcfg[0];
	if ((g_tlsc6x_cfg_ver & 0x3ffffff) != (u32tmp & 0x3ffffff)) {
		return 0;
	}

	vnow = (g_tlsc6x_cfg_ver >> 26) & 0x3f;
	vbuild = (u32tmp >> 26) & 0x3f;
	TS_INFO("vnow: 0x%x,vbuild: 0x%x", vnow, vbuild);
#ifdef TS_FORCE_UPDATE_FIRMWARE//force update binhua
#else
	if (vbuild <= vnow) {
		return 0;
	}
#endif
	return 1;
}
//#endif
static int tlsc6x_tpcfg_ver_comp_weak(unsigned short *ptcfg)
{
	unsigned int u32tmp;

	TLSC_FUNC_ENTER();
	if (g_tlsc6x_cfg_ver == 0) {	/* no available version information */
		return 0;
	}

	if (is_valid_cfg_data(ptcfg) == 0) {
		tlsc_err("Tlsc6x:cfg_data is invalid!\n");
		return 0;
	}

	u32tmp = ptcfg[1];
	u32tmp = (u32tmp << 16) | ptcfg[0];
	if ((g_tlsc6x_cfg_ver & 0x3ffffff) != (u32tmp & 0x3ffffff)) {	/*  */
		tlsc_err("tlsc6x:ptcfg version error,g_tlsc6x_cfg_ver is 0x%x:ptcfg version is 0x%x!\n",
		g_tlsc6x_cfg_ver, u32tmp);
		return 0;
	}

	if (g_tlsc6x_cfg_ver == u32tmp) {
		return 0;
	}

	return 1;
}

/* 0 error */
/* 0x7f80 no data */
static u16 tlsx6x_find_last_cfg(void)
{
	unsigned short addr, check;

	addr = 0x7f80 - 256;
	while (addr > 0x6000) {	/* 0x6080 */
		check = 0;
		if (tlsc6x_read_burn_space((u8 *) &check, addr, 2)) {
			addr = 0;
			goto exit;
		}
		if (check == 0xffff) {
			break;
		}
		addr -= 256;
	}

	addr += 256;

exit:

	return addr;
}

//#ifdef TLSC_AUTO_UPGRADE
static int find_last_valid_burn_cfg(u16 *ptcfg)
{
	unsigned short addr;

	if (tlsc6x_download_ramcode(fw_burn_bin, sizeof(fw_burn_bin))) {
		return -EPERM;
	}

	addr = tlsx6x_find_last_cfg();
	if ((addr == 0) || (addr == 0x7f80)) {
		return -EPERM;
	}

	while (addr <= 0x7e80) {
		if (tlsc6x_read_burn_space((u8 *) ptcfg, addr, 204)) {
			addr = 0x7f80;	/* force error */
			break;
		}
		if (is_valid_cfg_data(ptcfg) == 0) {
			addr += 256;
		} else {
			break;
		}
	}

	return (addr <= 0x7e80) ? 0 : -1;

}

int tlsx6x_update_fcomp_cfg(u16 *ptcfg)
{
	if (tlsc6x_tpcfg_ver_comp_weak(ptcfg) == 0) {
		tlsc_err("Tlsc6x:update error:version error!\n");
		return -EPERM;
	}

	if (g_tlsc6x_cfg_ver && ((u16) (g_tlsc6x_cfg_ver & 0xffff) != ptcfg[0])) {
		return -EPERM;
	}

	if (tlsc6x_download_ramcode(fw_fcode_burn, sizeof(fw_fcode_burn))) {
		tlsc_err("Tlsc6x:update error:ram-code error!\n");
		return -EPERM;
	}

	if (tlsc6x_write_burn_space((unsigned char *)ptcfg, 0x8000, 204)) {
		tlsc_err("Tlsc6x:update fcomp_cfg fail!\n");
		return -EPERM;
	}

	tlsc_info("Tlsc6x:update fcomp_cfg pass!\n");

	memcpy(tl_target_cfg, ptcfg, 204);
	g_tlsc6x_cfg_ver = (ptcfg[1] << 16) | ptcfg[0];
	tlsc_cfg_version=g_tlsc6x_cfg_ver>>26;

	return 0;
}

int tlsx6x_update_fcomp_boot(unsigned char *pdata, u16 len)
{
	if (tlsc6x_download_ramcode(fw_fcode_burn, sizeof(fw_fcode_burn))) {
		tlsc_err("Tlsc6x:update error:ram-code error!\n");
		return -EPERM;
	}
	pdata[8] = 0xff;
	if (tlsc6x_write_burn_space((unsigned char *)pdata, 0x00, len)) {
		tlsc_err("Tlsc6x:update fcomp_boot fail!\n");
		return -EPERM;
	}
	pdata[8] = 0x4b;
	if (tlsc6x_write_burn_space(&pdata[8], 0x08, 1)) {
		tlsc_err("Tlsc6x:update fcomp_boot last sig-byte fail!\n");
		return -EPERM;
	}
     	g_tlsc6x_boot_ver = pdata[5];
	g_tlsc6x_boot_ver = (g_tlsc6x_boot_ver<<8) + pdata[4];
	tlsc_boot_version = g_tlsc6x_boot_ver;
	tlsc_info("Tlsc6x:update fcomp_boot pass!\n");

	return 0;
}

int tlsx6x_update_ocomp_boot(unsigned char *pdata, u16 len)
{
	unsigned int oo_tail[4] = { 0x60298bf, 0x15cbf, 0x60301bf, 0x3d43f };

	if (tlsc6x_download_ramcode(fw_burn_bin, sizeof(fw_burn_bin))) {
		tlsc_err("Tlsc6x:update error:ram-code error!\n");
		return -EPERM;
	}

	if (tlsc6x_write_burn_space((unsigned char *)pdata, 0x00, len)) {
		tlsc_err("Tlsc6x:update ocomp_boot fail!\n");
		return -EPERM;
	}

	if (tlsc6x_write_burn_space((unsigned char *)oo_tail, 0x7fec, 16)) {
		tlsc_err("Tlsc6x:update ocomp_boot fail!\n");
		return -EPERM;
	}
	tlsc_info("Tlsc6x:update ocomp_boot pass!\n");

	return 0;
}

int tlsc6x_update_f_combboot(u8 *pdata, u16 len)
{
        int ret = 0;
        int i = 0;
        u32 check_sum = 0;
        unsigned short boot_len;

        TLSC_FUNC_ENTER();

        boot_len = pdata[7];
        boot_len = (boot_len << 8) + pdata[6];
        tlsc_info("Tlsc6x:update file size is %d, and boot len is %d !\n",len,boot_len);

        if (g_mccode == 0) {
                if (g_needKeepRamCode == 0) {
                        tlsc_err("Tlsc6x:update_f_combboot error:mccode error!\n");
                        return -EPERM;
                }
                if ((len >= boot_len) && ((pdata[2] == 0x35) && (pdata[3] == 0x35))) {
                        ret = tlsx6x_update_ocomp_boot(pdata, boot_len);
                        pdata += boot_len;
                        len = len - boot_len;
                }
                if ((ret == 0) && (len >= 204)) {
                        memcpy(tl_buf_tmpcfg, pdata, 204);
                        ret = tlsx6x_update_burn_cfg(tl_buf_tmpcfg);
                }
                return ret;
        }

        if ((len >= boot_len) && ((pdata[2] == 0x36) && (pdata[3] == 0x35))) {
               check_sum = tlsc6x_checksumEx(pdata, boot_len);
               for(i=0;i<4;i++){
                    pdata[boot_len + i] = (check_sum>>(i*8)) & 0xff;
                }
                ret = tlsx6x_update_fcomp_boot(pdata, (boot_len + 4));
                pdata += boot_len;
                len = len - boot_len;
        }

        if ((ret == 0) && (len >= 204)) {
                memcpy(tl_buf_tmpcfg, pdata, 204);
                ret = tlsx6x_update_fcomp_cfg(tl_buf_tmpcfg);
        }

        return ret;

}

int tlsx6x_update_burn_cfg(u16 *ptcfg)
{
	u16 addr, check;

	TLSC_FUNC_ENTER();
	if (g_mccode == 1) {
		return tlsx6x_update_fcomp_cfg(ptcfg);
	}

	if (tlsc6x_tpcfg_ver_comp_weak(ptcfg) == 0) {
		tlsc_err("Tlsc6x:update error:version error!\n");
		return -EPERM;
	}

	if (g_tlsc6x_cfg_ver && ((u16) (g_tlsc6x_cfg_ver & 0xffff) != ptcfg[0])) {
		return -EPERM;
	}

	if (tlsc6x_download_ramcode(fw_burn_bin, sizeof(fw_burn_bin))) {
		tlsc_err("Tlsc6x:update error:ram-code error!\n");
		return -EPERM;
	}

	addr = tlsx6x_find_last_cfg();
	if ((addr <= 0x6180) || (addr == 0x7f80)) {
		tlsc_err("Tlsc6x:update error:time limit!\n");
		return -EPERM;
	}

	addr = addr - 256;

	/* pre-check */
	check = 0;
	if (tlsc6x_read_burn_space((unsigned char *)&check, addr - 256, 2)) {
		tlsc_err("Tlsc6x:update error:pre-read error!\n");
		return -EPERM;
	}
	if (check != 0xffff) {
		tlsc_err("Tlsc6x:update error:pre-read limit!\n");
		return -EPERM;
	}

	if (tlsc6x_write_burn_space((unsigned char *)ptcfg, addr, 204)) {
		tlsc_err("Tlsc6x:update fail!\n");
		return -EPERM;
	}

	tlsc_info("Tlsc6x:update pass!\n");

	memcpy(tl_target_cfg, ptcfg, 204);
	g_tlsc6x_cfg_ver = (ptcfg[1] << 16) | ptcfg[0];
	g_tlsc6x_chip_code = (unsigned short)ptcfg[53];

	tlsc_cfg_version=g_tlsc6x_cfg_ver>>26;

	return 0;
}

/* NOTE:caller guarantee the legitimacy */
/* download tp-cfg. */
int tlsx6x_update_running_cfg(u16 *ptcfg)
{
	unsigned int retry;
	unsigned int tmp[2];

	TLSC_FUNC_ENTER();
	if (is_valid_cfg_data(ptcfg) == 0) {
		return -EPERM;
	}
	if (tlsc6x_set_dd_mode()) {
		return -EPERM;
	}

	if (tlsc6x_bulk_down_check((unsigned char *)ptcfg, 0xd7e0, 204)) {	/* stop mcu */
		goto exit;
	}

	tmp[0] = 0x6798;
	tmp[1] = 0xcd3500ff;

	retry = 0;
	while (++retry < 3) {
		if (tlsc6x_write_bytes_u16addr(g_tlsc6x_client, 0xdf10, (u8 *) &tmp[0], 8)) {
			usleep_range(5000, 5500);
			continue;
		}
		break;
	}

	/* write error? don't care */
	retry = 0;
	while (++retry < 5) {
		usleep_range(10000, 11000);
		tmp[0] = 0;
		tlsc6x_read_bytes_u16addr(g_tlsc6x_client, 0xdf16, (u8 *) &tmp[0], 1);
		if (tmp[0] == 0x30) {
			break;
		}
	}

exit:
	tlsc6x_set_nor_mode();
	memcpy(tl_target_cfg, ptcfg, 204);
	return 0;
}

/* return :0->no hw resetting needed */
/* else -> caller do hw resettting */


/* 0:successful */
static int tlsc6x_download_gestlib_fast(u8 *pcode, u16 len)
{
	u8 dwr, retry;
	int ret = -2;

	TLSC_FUNC_ENTER();
	if (tlsc6x_set_dd_mode()) {
		return -EPERM;
	}

	dwr = 0x05;
	if (tlsc6x_bulk_down_check(&dwr, 0x0602, 1) == 0) {	/* stop mcu */
		dwr = 0x00;
		tlsc6x_bulk_down_check(&dwr, 0x0643, 1);	/* disable irq */
	} else {
		return -EPERM;
	}

	if (tlsc6x_bulk_down_check(pcode, 0x8000, 1024) == 0) {
		if (tlsc6x_write_bytes_u16addr(g_tlsc6x_client, 0x8400, pcode + 1024, len - 1024) == 0) {	/*  */
			ret = 0;
		}
	}

	if (ret == 0) {
		dwr = 0x88;
		retry = 0;
		do {
			ret = tlsc6x_write_bytes_u16addr(g_tlsc6x_client, 0x0602, &dwr, 1);
		} while ((++retry < 3) && (ret != 0));
	}

	mdelay(40);		/* 30ms */
	if (tlsc6x_get_i2cmode() == DIRECTLY_MODE) {
		ret = tlsc6x_download_ramcode(pcode, len);
	}

	return ret;
}

int tlsc6x_load_gesture_binlib(void)
{
	int ret;

	TLSC_FUNC_ENTER();
	ret = tlsc6x_download_gestlib_fast(tlsc6x_gesture_binlib, sizeof(tlsc6x_gesture_binlib));
	if (ret) {
		tlsc_err("Tlsc6x:load gesture binlib error!\n");
		return -EPERM;
	}
	return 0;
}


int tlsc6x_load_ext_binlib(u8 *pdata, u16 len)
{
	int ret = 0;
	unsigned short boot_len;

	TLSC_FUNC_ENTER();

	boot_len = pdata[7];
	boot_len = (boot_len << 8) + pdata[6];

	if (len >= boot_len) {
		ret = tlsc6x_download_ramcode(pdata, boot_len);
		pdata += boot_len;
		len = len - boot_len;
		if ((ret == 0) && (len >= 204)) {
			mdelay(30);
			ret = tlsx6x_update_running_cfg((u16 *) pdata);
		}
	}

	return 0;
}

/*
 *get running time tp-cfg.
 *@ptcfg:	data buffer
 *@addr:	real data address for different chips
 *
 *return: 0 SUCESS else FAIL
 * Note: touch chip 	MUST work in DD-mode.
*/
static int tlsx6x_comb_get_running_cfg(u16 *ptcfg, u16 addr)
{
	int retry, err_type;

	TLSC_FUNC_ENTER();
	retry = 0;
	err_type = 0;

	tlsc6x_set_dd_mode();

	while (++retry < 5) {
		err_type = 0;
		if (tlsc6x_read_bytes_u16addr(g_tlsc6x_client, addr, (u8 *) ptcfg, 204)) {
			mdelay(20);
			err_type = 2;	/* i2c error */
			continue;
		}

		if (is_valid_cfg_data(ptcfg) == 0) {
			tlsc6x_set_dd_mode();
			err_type = 1;	/* data error or no data */
			mdelay(20);
			continue;
		}
		break;
	}

	return err_type;

}


static int tlsx6x_3535get_running_cfg(unsigned short *ptcfg)
{

	TLSC_FUNC_ENTER();

	return tlsx6x_comb_get_running_cfg(ptcfg, 0xd6e0);
}

static int tlsx6x_3536get_running_cfg(unsigned short *ptcfg)
{

	TLSC_FUNC_ENTER();

	return tlsx6x_comb_get_running_cfg(ptcfg, 0x9e00);
}

static int tlsx6x_3535find_lastvaild_ver(void)
{
	if (find_last_valid_burn_cfg(tl_buf_tmpcfg) == 0) {
		g_tlsc6x_cfg_ver = (unsigned int)tl_buf_tmpcfg[1];
		g_tlsc6x_cfg_ver = (g_tlsc6x_cfg_ver<<16) + (unsigned int)tl_buf_tmpcfg[0];

		tlsc_cfg_version = g_tlsc6x_cfg_ver>>26;
		tlsc_vendor_id = (g_tlsc6x_cfg_ver>>9)&0x7F;
		tlsc_project_id = g_tlsc6x_cfg_ver&0x01FF;
	}

	return 0;
}

static int tlsx6x_3536find_lastvaild_ver(void)
{
	if (tlsc6x_download_ramcode(fw_fcode_burn, sizeof(fw_fcode_burn))) {
		tlsc_err("Tlsc6x:update error:ram-code error!\n");
		return -EPERM;
	}

	if (tlsc6x_read_burn_space((u8 *) tl_buf_tmpcfg, 0x8000, 204)) {
		return -EPERM;
	}


	if (is_valid_cfg_data(tl_buf_tmpcfg) == 0) {
		if (tlsc6x_read_burn_space((u8 *) tl_buf_tmpcfg, 0xf000, 204)) {
			return -EPERM;
		}
	}

	if (is_valid_cfg_data(tl_buf_tmpcfg)) {
		g_tlsc6x_cfg_ver = (unsigned int)tl_buf_tmpcfg[1];
		g_tlsc6x_cfg_ver = (g_tlsc6x_cfg_ver<<16) + (unsigned int)tl_buf_tmpcfg[0];
                g_tlsc6x_cfg_ver=g_tlsc6x_cfg_ver&0x3ffffff;
		tlsc_cfg_version = g_tlsc6x_cfg_ver>>26;
		tlsc_vendor_id = (g_tlsc6x_cfg_ver>>9)&0x7F;
		tlsc_project_id = g_tlsc6x_cfg_ver&0x01FF;
	}
	return 0;
}


//#ifdef TLSC_BUILDIN_BOOT
static int tlsc6x_upgrade_romcfg_array(unsigned short *parray, unsigned int cfg_num)
{
	unsigned int  k;

	TLSC_FUNC_ENTER();

	tlsc_info("g_tlsc6x_cfg_ver is 0x%x\n",g_tlsc6x_cfg_ver);
	if (g_tlsc6x_cfg_ver == 0) {	/* no available version information */
		tlsc_err("Tlsc6x:no current version information!\n");
		//return 0;
	}

	new_idx_active = -1;

	for (k = 0; k < cfg_num; k++) {
		if (tlsc6x_tpcfg_ver_comp(parray) == 1) {
			new_idx_active = k;
			tlsc_info("%s, new_idx_active is %d.\n", __func__, new_idx_active);
			break;
		}
		parray = parray + 102;
	}

	if (new_idx_active < 0) {
		tlsc_info("Tlsc6x:auto update skip:no updated version!\n");
		return -EPERM;
	}

	if (tlsc6x_set_dd_mode()) {
		tlsc_err("Tlsc6x:auto update error:can't control hw mode!\n");
		return -EPERM;
	}

	if (tlsx6x_update_burn_cfg(parray) == 0) {
		tlsc_info("Tlsc6x:update pass!\n");
	} else {
		tlsc_err("Tlsc6x:update fail!\n");
	}

	return 1;		/* need hw reset */


}


static int tlsc6x_boot_ver_comp(unsigned int ver)
{
	tlsc_info("Tlsc6x:3536 boot not need update00000!g_tlsc6x_boot_ver = %d : %d\n", g_tlsc6x_boot_ver, ver);
	if (g_tlsc6x_boot_ver == 0) {
		return 1;
	}

	if (ver  > g_tlsc6x_boot_ver ) {
		return 1;
	}

	tlsc_info("Tlsc6x:3536 boot not need update!g_tlsc6x_boot_ver = %d : %d\n", g_tlsc6x_boot_ver, ver);
	return 0;
}

static int tlsc6x_3536boot_update(u8 *pdata, u16 boot_len)
{
	unsigned int ver = 0;

	ver = pdata[5];
	ver = (ver<<8) + pdata[4];
#ifdef TS_FORCE_UPDATE_FIRMWARE//force update binhua
#else
	if (tlsc6x_boot_ver_comp(ver) == 0) {
		tlsc_info("Tlsc6x:3536 boot not need update!\n");
		return 0;
	}
#endif
	return  tlsx6x_update_fcomp_boot(pdata, boot_len);
}

static int tlsc6x_3535boot_update(u8 *pdata, u16 boot_len)
{
	unsigned int ver = 0;

	ver = pdata[5];
	ver = (ver<<8) + pdata[4];

	if (tlsc6x_boot_ver_comp(ver) == 0) {
		tlsc_info("Tlsc6x:3535 boot not need update!\n");
		return 0;
	}

	g_needKeepRamCode = 1;

	return tlsc6x_load_ext_binlib(pdata, boot_len);
}



static int tlsc6x_boot_update(u8 *pdata, u16 boot_len)
{

	if (g_mccode == 0) {
		return tlsc6x_3535boot_update(pdata, boot_len);
	}

	return tlsc6x_3536boot_update(pdata, boot_len);
}


int tlsc6x_update_compat_ctl(u8 *pupd, int len)
{
	u32 k;
	u32 n;
	u32 offset;
	u32 *vlist;
	int ret = -1;

	struct tlsc6x_updfile_header *upd_header;

	if (len < sizeof(struct tlsc6x_updfile_header)){
		return -EPERM;
	}

	upd_header = (struct tlsc6x_updfile_header *) pupd;

	if (upd_header->sig != 0x43534843) {
		return -EPERM;
	}

	n = upd_header->n_cfg;
	offset = (upd_header->n_match * 4) + sizeof(struct tlsc6x_updfile_header);

	if ((offset + upd_header->len_cfg + upd_header->len_boot) != len) {
		return -EPERM;
	}

	if ((n * 204) != upd_header->len_cfg) {
		return -EPERM;
	}

	if (n != 0) {
		tlsc6x_upgrade_romcfg_array((u16 *) (pupd + offset), n);
	}

	n = upd_header->n_match;
	if (n != 0) {
		vlist = (u32 *) (pupd + sizeof(struct tlsc6x_updfile_header));
		offset = offset + upd_header->len_cfg;
		for (k=0; k < n; k++) {
			if (vlist[k] == (g_tlsc6x_cfg_ver & 0xffffff)) {
				ret = tlsc6x_boot_update((pupd + offset), upd_header->len_boot);
				if(0 != ret) {
					return ret;
				} else {
					if(ts_fw.upgrade_fw){
						kfree(ts_fw.upgrade_fw);
						ts_fw.upgrade_fw = NULL;
					}
					break;
				}
			}
		}
	}
	return 0;
}

int  tlsc6x_do_update_ifneed(void)
{
	return tlsc6x_update_compat_ctl((u8 *) ts_fw.upgrade_fw, ts_fw.upgrade_fw_size);
}
//#endif

int tlsc6x_get_running_cfg(unsigned short *ptcfg)
{
	if (g_mccode == 0) {
		return tlsx6x_3535get_running_cfg(ptcfg);
	}
	return tlsx6x_3536get_running_cfg(ptcfg);
}

int tlsc6x_find_ver(void)
{
	if (g_mccode == 0) {
		return tlsx6x_3535find_lastvaild_ver();
	}
	return tlsx6x_3536find_lastvaild_ver();

}




/* NOT a public function, only one caller!!! */
static void tlsc6x_tp_mccode(void)
{
	unsigned int tmp[3];

	mdelay(60);
	g_mccode = 0xff;

	tlsc_info("tlsc6x_tp_mccode000 \n");

	if (tlsc6x_read_bytes_u16addr(g_tlsc6x_client, 0x8000, (u8 *) tmp, 12)) {
		tlsc_info("tlsc6x_tp_mccode111 return \n");
		return;
	}
	if (tmp[2] == 0x544c4e4b) {	/*  boot code  */
		if (tmp[0] == 0x35368008) {
			g_mccode = 1;
			g_tlsc6x_boot_ver = tmp[1] & 0xffff;
			tlsc_boot_version = g_tlsc6x_boot_ver;
		} else if (tmp[0] == 0x35358008) {
			g_mccode = 0;
			g_tlsc6x_boot_ver = tmp[1] & 0xffff;
			tlsc_boot_version = g_tlsc6x_boot_ver;
		}
	}else if (tmp[2] == 0x544c4ebd) {	/*  boot code  */
		if (tmp[0] == 0x35368008) {
			g_mccode = 1;
			g_tlsc6x_boot_ver = 0;
			tlsc_boot_version = g_tlsc6x_boot_ver;
		} else if (tmp[0] == 0x35358008) {
			g_mccode = 0;
			g_tlsc6x_boot_ver = tmp[1] & 0xffff;
			tlsc_boot_version = g_tlsc6x_boot_ver;
		}
	}
	else {	/* none code */
		tmp[0] = 0;
		if (tlsc6x_read_bytes_u16addr(g_tlsc6x_client, 0x09, (u8 *) tmp, 3)) {
			return;
		}
		if ((tmp[0] == 0x444240) || (tmp[0] == 0x5c5c5c)){
			g_mccode = 1;
		} else if (tmp[0] == 0x35358008) {
			g_mccode = 0;
		}
	}

	if (g_mccode == 0) {
		MTK_TXRX_BUF = 0x80cc00;
		CMD_ADDR = 0x80b400;
		RSP_ADDR = 0x80b440;
		//chip_op_if.get_running_cfg = tlsx6x_3535get_running_cfg;
		//chip_op_if.find_ver = tlsx6x_3535find_lastvaild_ver;
		//chip_op_if.update_boot = tlsc6x_3535boot_update;
	} else {
		MTK_TXRX_BUF = 0x809000;
		CMD_ADDR = 0x809f00;
		RSP_ADDR = 0x809f40;
		//chip_op_if.get_running_cfg = tlsx6x_3536get_running_cfg;
		//chip_op_if.find_ver = tlsx6x_3536find_lastvaild_ver;
		//chip_op_if.update_boot = tlsc6x_3536boot_update;
	}

}

#define REG_CHIP_ID 0xa3

static void tlsc6x_reset(void){

	TS_INFO("tlsc6440 reset");
	ts_gpio_output(TSGPIO_RST, 0);
	mdelay(20);
	ts_gpio_output(TSGPIO_RST, 1);
	mdelay(30);
}

static void tlsc6x_tp_reset_active(void){

	TS_INFO("tlsc6440 reset_active");
	ts_gpio_output(TSGPIO_RST, 0);
	mdelay(20);
	ts_gpio_output(TSGPIO_RST, 1);
	mdelay(2);
}
extern void tlsc6x_esd_status_init(void);
int tlsc6x_read_chipid(void){

	int chip_id = 0;
	int loop = 0;
	bool err = false;
	unsigned char tmp[3];

	g_tlsc6x_client = g_client;
        g_client->addr = 0x2E;
	mdelay(100);
	tlsc6x_esd_status_init();
	err |= (1 != ts_reg_read(REG_CHIP_ID, (unsigned char*)&chip_id, 1));

	if(0x36 != chip_id) {
		while(loop++ < 3) {
			tlsc6x_reset();
			mdelay(100);
			if (tlsc6x_set_dd_mode()) {
				tlsc_info("tlsc6x_set_dd_mode fail \n");
			}

			if (tlsc6x_read_bytes_u16addr(g_tlsc6x_client, 0x00, tmp, 1)) {
				tlsc_info("tlsc6x_read_bytes_u16addr 0x00  fail \n");
			} else {
				tlsc_info("tmp[0]=0x%x \n", tmp[0]);
				if(0x1f == tmp[0]) {
					chip_id = 0x36;
					break;
				}
			}
		}
	}

	tlsc_info("chip_id=0x%x\n", chip_id);
	return chip_id;
}

/* is tlsc6x module ? */
int tlsc6x_tp_dect(struct i2c_client *client)
{
	u8 dwr;
	int ret = -1, loop_count=0;

	g_mccode = 1;/* default */
	MTK_TXRX_BUF = 0x809000;
	CMD_ADDR = 0x809f00;
	RSP_ADDR = 0x809f40;

        while(loop_count++<5) {

		if (tlsc6x_set_dd_mode()) {//if firemwork is bad

			tlsc_info("tlsc6x_set_dd_mode000 loop_count=%d failed \n", loop_count);
			tlsc6x_tp_reset_active();
			//mdelay(8);
			dwr = 0x05;
			if (tlsc6x_bulk_down_check(&dwr, 0x0602, 1) == 0) {
				dwr = 0x00;
				tlsc6x_bulk_down_check(&dwr, 0x0643, 1);
			} else {
				tlsc_info("write 0x0602 failed \n");
			}
		} else {
			break;
		}
        }

	tlsc6x_tp_mccode();	/* MUST: call this function there!!! */
	tlsc_info("g_mccode is 0x%x\n",g_mccode);

	if (g_mccode == 0xff) {
		tlsc_err("get mccode fail\n");
		g_mccode = 1;
		MTK_TXRX_BUF = 0x809000;
		CMD_ADDR = 0x809f00;
		RSP_ADDR = 0x809f40;
		g_tlsc6x_boot_ver = 0;
		//goto exit;
	}

	/*try to get running time tp-cfg. if fail : wrong boot? wrong rom-cfg?*/
	if (tlsc6x_get_running_cfg(tl_buf_tmpcfg) == 0) {
		g_tlsc6x_cfg_ver = (unsigned int)tl_buf_tmpcfg[1];
		g_tlsc6x_cfg_ver = (g_tlsc6x_cfg_ver<<16) + (unsigned int)tl_buf_tmpcfg[0];
		g_tlsc6x_chip_code = (unsigned short)tl_buf_tmpcfg[53];
		tlsc_cfg_version = g_tlsc6x_cfg_ver>>26;
		tlsc_vendor_id = (g_tlsc6x_cfg_ver>>9)&0x7F;
		tlsc_project_id = g_tlsc6x_cfg_ver&0x01FF;

	} else {

		if(0 == tl_buf_tmpcfg[2] && 0 == tl_buf_tmpcfg[3]) {//firmware read config from flash fail
			g_mccode = 1;
			MTK_TXRX_BUF = 0x809000;
			CMD_ADDR = 0x809f00;
			RSP_ADDR = 0x809f40;
			g_tlsc6x_boot_ver = 0;
		}
		tlsc6x_find_ver();
	}

	if (g_tlsc6x_cfg_ver == 0) {
		tlsc_err("get cfg-ver fail\n");
		goto exit;
	}

	tlsc6x_get_firmware_data(client);

	if(g_pdata->firmware_update_switch) {
		ret = tlsc6x_do_update_ifneed();
		if(0 != ret) {
			return 0;
		}
	}

	sprintf(g_pdata->firmwork_version, "0x%02X,0x%04X", tlsc_cfg_version,tlsc_boot_version);

exit:
	tlsc6x_set_nor_mode();

	return 1;
}

int  tlsc6x_get_firmware_data(struct i2c_client *client)
{
	int i = 0;
	struct device_node *node;

	u8 fw_node_name[16]={0};
	u8 vendor_node_name[16]={0};
	u32 tlsc_is_project_id;
	unsigned int *is_upgrade_switch;
	unsigned int *upgrade_fw_size;
	int ret = 0;

	node =client->dev.of_node;
	if (!node) {
		tlsc_err("%s can't find touch compatible custom node\n", __func__);
		return -ENODEV;
	}

	node = of_get_child_by_name(node, "ats_tlsc6x");
	of_property_read_u32(node, "tp_vendor_num", &ts_fw.vendor_num);
//	tlsc_info(" tp_vendor_num:%d\n", ts_fw.vendor_num);

	ts_fw.vendor_id = kmalloc(ts_fw.vendor_num, GFP_KERNEL);
	if(ts_fw.vendor_id == NULL){
		return -1;
	}

	is_upgrade_switch = kmalloc(ts_fw.vendor_num*sizeof(u32), GFP_KERNEL);
	if(is_upgrade_switch == NULL){
		ret = -1;
		goto exit_free_vendorid;
	}

	upgrade_fw_size = kmalloc(ts_fw.vendor_num*sizeof(u32), GFP_KERNEL);
	if(upgrade_fw_size == NULL){
		ret = -1;
		goto exit_free_upgrade_switch;
	}

	of_property_read_u32(node, "tpd-project-id", &tlsc_is_project_id);

	of_property_read_u8_array(node, "tp_vendor_id", ts_fw.vendor_id, ts_fw.vendor_num);
	of_property_read_u32_array(node, "tp_upgrade_fw", is_upgrade_switch, ts_fw.vendor_num);
	of_property_read_u32_array(node, "tp_fw_buf_size", upgrade_fw_size, ts_fw.vendor_num);
	for(i=0; i<ts_fw.vendor_num; i++){
		printk("[tlsc] [TS] vendor_id[%d] = 0x%x\n", i, ts_fw.vendor_id[i]);
		printk("[tlsc] [TS] upgrade_switch[%d] = %d,\n", i, is_upgrade_switch[i]);
		printk("[tlsc] [TS] upgrade_fw_size[%d] = %d\n", i, upgrade_fw_size[i]);
	}

	if(tlsc_is_project_id==1)
		tlsc_vendor_id = tlsc_project_id;

	TS_INFO("[tlsc] tlsc_vendor_id=0x%x tlsc_project_id=0x%x \n", tlsc_vendor_id, tlsc_project_id);
	for(i=0; i <  ts_fw.vendor_num; i++ ){
		if (ts_fw.vendor_id[i] ==tlsc_vendor_id){

			ts_fw.upgrade_fw = kmalloc(upgrade_fw_size[i], GFP_KERNEL);
			if(ts_fw.upgrade_fw == NULL){
				ret = -1;
				goto exit_free_fw_size;
			}

			g_pdata->firmware_update_switch = is_upgrade_switch[i];
			ts_fw.upgrade_fw_size = upgrade_fw_size[i];
			//tlsc_info(" is_upgrade_switch=%d, upgrade_fw_size=%d\n", ts_fw.is_upgrade_switch, ts_fw.upgrade_fw_size);

			sprintf(fw_node_name, "tp_fw_buf%d", i);
			sprintf(vendor_node_name, "tp_vendor_name%d", i);
			of_property_read_string(node, vendor_node_name, (char const **)&g_pdata->vendor_string);
			of_property_read_u8_array(node, fw_node_name, ts_fw.upgrade_fw, ts_fw.upgrade_fw_size);

			memset(g_pdata->chip_name, 0x00, sizeof(g_pdata->chip_name));
			memcpy(g_pdata->chip_name, tlsc6x_chip_name[(g_tlsc6x_chip_code>>8) & 0xf], sizeof(g_pdata->chip_name));
			//tlsc_info(" fw_node_name=%s, tlsc_vendor_string=%s\n", fw_node_name, ts_fw.vendor_name);
//			tlsc_info("upgrade_fw buff=0x%x 0x%x  0x%x\n", ts_fw.upgrade_fw[ts_fw.upgrade_fw_size-3],
//				ts_fw.upgrade_fw[ts_fw.upgrade_fw_size-2], ts_fw.upgrade_fw[ts_fw.upgrade_fw_size-1]);
#if 0//def CONFIG_TRANSSION_HWINFO
			transsion_hwinfo_add("touch_vendor", (char *)ts_fw.vendor_name);
			sprintf(ts_fw.fw_version, "0x%02X,0x%04X", tlsc_cfg_version,tlsc_boot_version);
			tlsc_info(" tlsc_cfg_version:tlsc_boot_version:0x%02X,0x%04X:",tlsc_cfg_version,tlsc_boot_version);
			transsion_hwinfo_add("touch_firmware_version", ts_fw.fw_version);
			tlsc_info(" fw_version:%s:", ts_fw.fw_version);
			transsion_hwinfo_add("touch_ic", tlsc6x_chip_name[g_tlsc6x_chip_code>>8]);//tlsc6x_chip_name
			tlsc_info(" tlsc6x_chip_name:%s",tlsc6x_chip_name[g_tlsc6x_chip_code>>8]);
			transsion_hwinfo_add("touch_firmware_update", ts_fw.is_upgrade_switch ? "1":"0" );
#endif

			break;
		}
		if(i==ts_fw.vendor_num-1)
			tlsc_err("[tpd] tlsc_vendor_id:0x%x is not find error\n", tlsc_vendor_id);
	}

exit_free_fw_size:
	kfree(upgrade_fw_size);
exit_free_upgrade_switch:
	kfree(is_upgrade_switch);
exit_free_vendorid:
	kfree(ts_fw.vendor_id);
	return 0;
}

