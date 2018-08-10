/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <misc/marlin_platform.h>

#include <misc/wcn_bus.h>

#include "bufring.h"
#include "rdc_debug.h"
#include "wcn_txrx.h"
#include "wcn_log.h"
#include "../include/wcn_glb_reg.h"

static int smp_calc_chsum(unsigned short *buf, unsigned int size)
{
	unsigned long int cksum = 0;
	unsigned short data;

	while (size > 1) {
		data = *buf;
		buf++;
		cksum += data;
		size -= sizeof(unsigned short);
	}

	if (size)
		cksum += *buf & 0xff;

	while (cksum >> 16)
		cksum = (cksum >> 16) + (cksum & 0xffff);

	return (unsigned short)(~cksum);
}

static int mdbg_write_smp_head(unsigned int len)
{
	struct smp_head *smp;
	unsigned char *smp_buf, *tmp;
	int smp_len;

	smp_len = sizeof(struct smp_head) + sizeof(struct sme_head_tag);
	smp_buf = kmalloc(smp_len, GFP_KERNEL);
	if (!smp_buf)
		return -ENOMEM;

	/* Smp header */
	smp = (struct smp_head *)smp_buf;
	smp->sync_code = SMP_HEADERFLAG;
	smp->length = smp_len + len - SYSNC_CODE_LEN;
	smp->channel_num = SMP_DSP_CHANNEL_NUM;
	smp->packet_type = SMP_DSP_TYPE;
	smp->reserved = SMP_RESERVEDFLAG;
	smp->check_sum = smp_calc_chsum(&smp->length, sizeof(struct smp_head)
		- SYSNC_CODE_LEN - CHKSUM_LEN);

	/*
	 * Diag header: Needs use these bytes for ARM log tool,
	 * And it need't 0x7e head and without 0x7e tail
	 */
	tmp = smp_buf + sizeof(struct smp_head);
	((struct sme_head_tag *)tmp)->seq_num = 0;
	((struct sme_head_tag *)tmp)->len = smp_len
		+ len - sizeof(struct smp_head);
	((struct sme_head_tag *)tmp)->type = SMP_DSP_TYPE;
	((struct sme_head_tag *)tmp)->subtype = SMP_DSP_DUMP_TYPE;

	mdbg_ring_write(mdbg_dev->ring_dev->ring, smp_buf, smp_len);

	kfree(smp_buf);

	return 0;
}

static int mdbg_dump_data(unsigned int start_addr,
			  char *str, int len, int str_len)
{
	unsigned char *buf, *temp_buf;
	int count, trans_size, err = 0, i, prin_temp = 2;
	int temp_len;

	if (unlikely(!mdbg_dev->ring_dev)) {
		WCN_ERR("mdbg_dump ring_dev is NULL\n");
		return -1;
	}
	if (str) {
		WCN_INFO("mdbg str_len:%d\n", str_len);
		if (mdbg_dev->ring_dev->flag_smp == 1)
			mdbg_write_smp_head(str_len);

		if ((mdbg_ring_free_space(mdbg_dev->ring_dev->ring) - 1)
			 < str_len) {
			wake_up_log_wait();
			temp_len
			= mdbg_ring_free_space(mdbg_dev->ring_dev->ring)
						- 1;
			if (temp_len > 0)
				mdbg_ring_write(mdbg_dev->ring_dev->ring,
						str, temp_len);
			if (temp_len < 0) {
				WCN_ERR("ringbuf str error\n");
				return 0;
			}
			str += temp_len;
			str_len -= temp_len;
			wake_up_log_wait();
		}

		while ((mdbg_ring_free_space(mdbg_dev->ring_dev->ring)
			- 1 == 0) && (mdbg_dev->open_count != 0)) {
			WCN_ERR("no space to write mem, sleep...\n");
			wake_up_log_wait();
			msleep(20);
		}

		mdbg_ring_write(mdbg_dev->ring_dev->ring, str, str_len);
		wake_up_log_wait();
	}

	if (len == 0)
		return 0;

	buf = kmalloc(DUMP_PACKET_SIZE, GFP_KERNEL);
	temp_buf = buf;
	if (!buf)
		return -ENOMEM;

	count = 0;
	while (count < len) {
		trans_size = (len - count) > DUMP_PACKET_SIZE ?
			DUMP_PACKET_SIZE : (len - count);
		temp_buf = buf;
		err = sprdwcn_bus_direct_read(start_addr + count, buf,
					      trans_size);
		if (err < 0) {
			WCN_ERR("%s dump memory error:%d\n", __func__, err);
			goto out;
		}
		if (prin_temp == 0) {
			prin_temp = 1;
			for (i = 0; i < 5; i++)
				WCN_ERR("mdbg *****buf[%d]:0x%x\n",
				       i, buf[i]);
		}
		if (mdbg_dev->ring_dev->flag_smp == 1)
			mdbg_write_smp_head(trans_size);

		if ((mdbg_ring_free_space(mdbg_dev->ring_dev->ring) - 1)
			 < trans_size) {
			wake_up_log_wait();
			temp_len
			= mdbg_ring_free_space(mdbg_dev->ring_dev->ring)
				- 1;
			if (temp_len > 0)
				mdbg_ring_write(mdbg_dev->ring_dev->ring,
						temp_buf, temp_len);
			if (temp_len < 0) {
				WCN_ERR("ringbuf data error\n");
				return 0;
			}
			temp_buf += temp_len;
			trans_size -= temp_len;
			count += temp_len;
			wake_up_log_wait();
		}
		while ((mdbg_ring_free_space(mdbg_dev->ring_dev->ring) - 1 == 0)
			&& (mdbg_dev->open_count != 0)) {
			WCN_ERR("no space buf to write mem, sleep...\n");
			wake_up_log_wait();
			msleep(20);
		}

		mdbg_ring_write(mdbg_dev->ring_dev->ring, temp_buf, trans_size);
		count += trans_size;
		wake_up_log_wait();
	}

out:
	kfree(buf);

	return count;
}

static void mdbg_clear_log(void)
{
	if (mdbg_dev->ring_dev->ring->rp
		!= mdbg_dev->ring_dev->ring->wp) {
		WCN_INFO("log:%ld left in ringbuf not read\n",
			mdbg_dev->ring_dev->ring->wp
		- mdbg_dev->ring_dev->ring->rp);
		mdbg_ring_clear(mdbg_dev->ring_dev->ring);
	}
}
static int cp_dcache_clean_invalid_all(void)
{
	int ret;
	unsigned int reg_val = 0;

	/*
	 * 1.AP write DCACHE REG CMD by sdio dt mode
	 * 2.delay little time for dcache clean excuting and polling done raw
	 * 3.clear done raw
	 * 4.if sdio dt mode is breaked,
	 *   cp cpu reset and dcache REG is default.
	 *   cache_debug mode must be set normal mode.
	 *   cache_size set 32K
	 */
	ret = sprdwcn_bus_reg_read(DCACHE_REG_ENABLE, &reg_val, 4);
	if (!(ret == 0)) {
		pr_info("Marlin3_Dcache REG sdiohal_dt_read error !\n");
		return ret;
	}
	if (!(reg_val & DCACHE_ENABLE_MASK)) {
		WCN_INFO("CP DCACHE DISENABLE\n");
		return ret;
	}
	WCN_INFO("CP DCACHE ENABLE\n");
	ret = sprdwcn_bus_reg_read(DCACHE_CFG0, &reg_val, 4);
	if (!(ret == 0)) {
		pr_info("Marlin3_Dcache REG sdiohal_dt_read error !\n");
		return ret;
	}
	if (reg_val & DCACHE_DEBUG_EN) {
		reg_val &= ~(DCACHE_DEBUG_EN);
		/* dcache set normal mode */
		ret = sprdwcn_bus_reg_write(DCACHE_CFG0, &reg_val, 4);
		if (!(ret == 0)) {
			pr_info("Marlin3_Dcache REG sdiohal_dt_write error !\n");
			return ret;
		}
	}
	ret = sprdwcn_bus_reg_read(DCACHE_CFG0, &reg_val, 4);
	if ((reg_val & DCACHE_SIZE_SEL_MASK) != DCACHE_SIZE_SEL_MASK) {
		reg_val |= ((DCACHE_SIZE_32K<<28)&DCACHE_SIZE_SEL_MASK);
		/* cache size set 32K */
		ret = sprdwcn_bus_reg_write(DCACHE_CFG0, &reg_val, 4);
	}
	reg_val = (
		(DCACHE_CMD_ISSUE_START | DCACHE_CMD_CLEAN_INVALID_ALL)&
		DCACHE_CMD_CFG2_MASK);
	ret = sprdwcn_bus_reg_write(DCACHE_CMD_CFG2, &reg_val, 4);
	/* cmd excuting */
	ret = sprdwcn_bus_reg_read(DCACHE_INT_RAW_STS, &reg_val, 4);
	/* read raw */
	if ((reg_val & 0X00000001) == 0) {
		pr_info("Marlin3_Dcache clear cost time not enough !\n");
		return ret;
	}
	reg_val = (DCACHE_CMD_IRQ_CLR);
	/* clear raw */
	ret = sprdwcn_bus_reg_write(DCACHE_INT_CLR, &reg_val, 4);

	return ret;
}

/* select aon_apb_dap DAP(Debug Access Port) */
static void dap_sel_btwf(void)
{
	int ret;
	unsigned int reg_val;

	ret = sprdwcn_bus_reg_read(DJTAG_DAP_SEL, &reg_val, 4);
	if (ret < 0) {
		WCN_ERR("%s read DJTAG_DAP_SEL error:%d\n", __func__, ret);
		WCN_INFO("dt fail,start reset pin!\n");
		ret = marlin_reset_reg();
		if (ret < 0) {
			WCN_ERR("dt fail,reset pin fail!\n");
			return;
		}
		ret = sprdwcn_bus_reg_read(DJTAG_DAP_SEL, &reg_val, 4);
		if (ret < 0) {
			WCN_ERR("after reset,dt read still fail!\n");
			return;
		}
	}
	MDBG_LOG("%s DJTAG_DAP_SEL:0x%x\n", __func__, reg_val);

	reg_val |= CM4_DAP_SEL_BTWF | CM4_DAP_SEL_GNSS;
	ret = sprdwcn_bus_reg_write(DJTAG_DAP_SEL, &reg_val, 4);
	if (ret < 0) {
		WCN_ERR("%s write DJTAG_DAP_SEL error:%d\n", __func__, ret);
		return;
	}
	MDBG_LOG("%s DJTAG_DAP_SEL:0x%x\n", __func__, reg_val);

	ret = sprdwcn_bus_reg_read(DJTAG_DAP_SEL, &reg_val, 4);
	if (ret < 0) {
		WCN_ERR("%s read2 DJTAG_DAP_SEL error:%d\n", __func__, ret);
		return;
	}
	MDBG_LOG("%s 2:DJTAG_DAP_SEL:0x%x\n", __func__, reg_val);
}

/* select aon_apb_dap DAP(Debug Access Port) */
static void dap_sel_default(void)
{
	int ret;
	unsigned int reg_val;

	reg_val = 0;
	ret = sprdwcn_bus_reg_write(DJTAG_DAP_SEL, &reg_val, 4);
	if (ret < 0) {
		WCN_ERR("%s write DJTAG_DAP_SEL error:%d\n", __func__, ret);
		return;
	}
}

/* disable aon_apb_dap_rst */
static void apb_rst(void)
{
	int ret;
	unsigned int reg_val;

	ret = sprdwcn_bus_reg_read(APB_RST, &reg_val, 4);
	if (ret < 0) {
		WCN_ERR("%s read APB_RST error:%d\n", __func__, ret);
		return;
	}
	MDBG_LOG("%s APB_RST:0x%x\n", __func__, reg_val);

	reg_val &= ~CM4_DAP0_SOFT_RST & ~CM4_DAP1_SOFT_RST;
	ret = sprdwcn_bus_reg_write(APB_RST, &reg_val, 4);
	if (ret < 0) {
		WCN_ERR("%s write APB_RST error:%d\n", __func__, ret);
		return;
	}
	MDBG_LOG("%s APB_RST:0x%x\n", __func__, reg_val);

	ret = sprdwcn_bus_reg_read(APB_RST, &reg_val, 4);
	if (ret < 0) {
		WCN_ERR("%s read2 APB_RST error:%d\n", __func__, ret);
		return;
	}
	MDBG_LOG("%s 2:APB_RST:0x%x\n", __func__, reg_val);
}

/* enable aon_apb_dap_en */
static void apb_eb(void)
{
	int ret;
	unsigned int reg_val;

	ret = sprdwcn_bus_reg_read(APB_EB, &reg_val, 4);
	if (ret < 0) {
		WCN_ERR("%s read APB_EB error:%d\n", __func__, ret);
		return;
	}
	MDBG_LOG("%s APB_EB:0x%x\n", __func__, reg_val);

	reg_val |= CM4_DAP0_EB | CM4_DAP1_EB;
	ret = sprdwcn_bus_reg_write(APB_EB, &reg_val, 4);
	if (ret < 0) {
		WCN_ERR("%s write APB_EB error:%d\n", __func__, ret);
		return;
	}
	MDBG_LOG("%s APB_EB:0x%x\n", __func__, reg_val);

	ret = sprdwcn_bus_reg_read(APB_EB, &reg_val, 4);
	if (ret < 0) {
		WCN_ERR("%s read2 APB_EB error:%d\n", __func__, ret);
		return;
	}
	MDBG_LOG("%s 2:APB_EB:0x%x\n", __func__, reg_val);
}

static void check_dap_is_ok(void)
{
	int ret;
	unsigned int reg_val;

	ret = sprdwcn_bus_reg_read(BTWF_STATUS_REG, &reg_val, 4);
	if (ret < 0) {
		WCN_ERR("%s read error:%d\n", __func__, ret);
		return;
	}
	MDBG_LOG("%s :0x%x\n", __func__, reg_val);

	if (reg_val == BTWF_OK_VALUE)
		WCN_INFO("btwf dap is ready\n");
}

/*
 * Debug Halting Control status Register
 * (0xe000edf0) = 0xa05f0003
 */
static void hold_btwf_core(void)
{
	int ret, i;
	unsigned int reg_val;
	unsigned int a[][2] = {
			{ARM_DAP_REG1, 0x22000012},
			{ARM_DAP_REG2, 0xe000edf0},
			{ARM_DAP_REG3, 0xa05f0003} }; /* 0xa05f0007 try */

	for (i = 0; i < 3; i++) {
		reg_val = a[i][1];
		ret = sprdwcn_bus_reg_write(a[i][0], &reg_val, 4);
		if (ret < 0) {
			WCN_ERR("%s  error:%d\n", __func__, ret);
			return;
		}
	}
}

/*
 * Debug Halting Control status Register
 * (0xe000edf0) = 0xa05f0003
 */
static void release_btwf_core(void)
{
	int ret, i;
	unsigned int reg_val;
	unsigned int a[][2] = {
			{ARM_DAP_REG1, 0x22000012},
			{ARM_DAP_REG2, 0xe000edf0},
			{ARM_DAP_REG3, 0xa05f0000} }; /* 0xa05f is a key */

	for (i = 0; i < 3; i++) {
		reg_val = a[i][1];
		ret = sprdwcn_bus_reg_write(a[i][0], &reg_val, 4);
		if (ret < 0) {
			WCN_ERR("%s  error:%d\n", __func__, ret);
			return;
		}
	}
}

/* Debug Exception and Monitor Control Register */
static void set_debug_mode(void)
{
	int ret, i;
	unsigned int reg_val;
	unsigned int a[][2] = {
			{ARM_DAP_REG1, 0x22000012},
			{ARM_DAP_REG2, 0xe000edfC},
			{ARM_DAP_REG3, 0x010007f1} };

	for (i = 0; i < 3; i++) {
		reg_val = a[i][1];
		ret = sprdwcn_bus_reg_write(a[i][0], &reg_val, 4);
		if (ret < 0) {
			WCN_ERR("%s  error:%d\n", __func__, ret);
			return;
		}
	}
}

/*
 * Debug core Register Selector Register
 * The index R0 is 0, R1 is 1
 */
static void set_core_reg(unsigned int index)
{
	int ret, i;
	unsigned int reg_val;
	unsigned int a[][2] = {
			{ARM_DAP_REG1, 0x22000012},
			{ARM_DAP_REG2, 0xe000edf4},
			{ARM_DAP_REG3, index} };

	for (i = 0; i < 3; i++) {
		reg_val = a[i][1];
		ret = sprdwcn_bus_reg_write(a[i][0], &reg_val, 4);
		if (ret < 0) {
			WCN_ERR("%s  error:%d\n", __func__, ret);
			return;
		}
	}
}

/*
 * write_core_reg_value - write arm reg = value.
 * Example: write PC(R15)=0x12345678
 * reg_index = 15, value = 0x12345678
 */
static void write_core_reg_value(unsigned int reg_index, unsigned int value)
{
	int ret, i;
	unsigned int reg_val;
	unsigned int a[][3] = {
			{ARM_DAP_REG1, 0x22000012, 0x22000012},
			{ARM_DAP_REG2, 0xe000edf8, 0xe000edf4},
			{ARM_DAP_REG3, value, 0x10000+reg_index} };

	for (i = 0; i < 3; i++) {
		reg_val = a[i][1];
		ret = sprdwcn_bus_reg_write(a[i][0], &reg_val, 4);
		if (ret < 0) {
			WCN_ERR("%s  error:%d\n", __func__, ret);
			return;
		}
	}

	for (i = 0; i < 2; i++) {
		reg_val = a[i][1];
		ret = sprdwcn_bus_reg_write(a[i][0], &reg_val, 4);
		if (ret < 0) {
			WCN_ERR("%s  error:%d\n", __func__, ret);
			return;
		}
	}

	sprdwcn_bus_reg_read(a[2][0], &reg_val, 4);
	MDBG_LOG("%s value: 0x%x, reg_value:0x%x\n", __func__, value, reg_val);

	for (i = 0; i < 3; i++) {
		reg_val = a[i][2];
		ret = sprdwcn_bus_reg_write(a[i][0], &reg_val, 4);
		if (ret < 0) {
			WCN_ERR("%s  error:%d\n", __func__, ret);
			return;
		}
	}
}

void sprdwcn_bus_armreg_write(unsigned int reg_index, unsigned int value)
{
	dap_sel_btwf();
	apb_rst();
	apb_eb();
	check_dap_is_ok();
	hold_btwf_core();
	set_debug_mode();
	write_core_reg_value(reg_index, value);

	/* make sure btwf core can run */
	release_btwf_core();

	/* make sure JTAG can connect dap */
	dap_sel_default();
}

/* Debug Core register Data Register */
static void read_core_reg(unsigned int value, unsigned int *p)
{
	int ret, i;
	unsigned int reg_val;
	unsigned int a[][2] = {
			{ARM_DAP_REG1, 0x22000012},
			{ARM_DAP_REG2, 0xe000edf8},
			{ARM_DAP_REG3, 0x00000000} };

	for (i = 0; i < 2; i++) {
		reg_val = a[i][1];
		ret = sprdwcn_bus_reg_write(a[i][0], &reg_val, 4);
		if (ret < 0) {
			WCN_ERR("%s  error:%d\n", __func__, ret);
			return;
		}
	}

	sprdwcn_bus_reg_read(a[2][0], &reg_val, 4);
	p[value] = reg_val;

	MDBG_LOG("%s ****R[%d]: 0x%x****\n", __func__, value, reg_val);
}


static int dump_arm_reg(void)
{
	unsigned int i;
	static const char *core_reg_name[19] = {
		"R0 ", "R1 ", "R2 ", "R3 ", "R4 ", "R5 ", "R6 ", "R7 ", "R8 ",
		"R9 ", "R10", "R11", "R12", "R13", "R14", "R15", "PSR", "MSP",
		"PSP",
	};
	unsigned int *p;

	p = kzalloc(19 * 4, GFP_KERNEL);
	if (!p) {
		WCN_ERR("Can not allocate ARM REG Buffer\n");
		return -ENOMEM;
	}

	memset(p, 0, 19 * 4);
	dap_sel_btwf();
	apb_rst();
	apb_eb();
	check_dap_is_ok();
	hold_btwf_core();
	set_debug_mode();
	for (i = 0; i < 19; i++) {
		set_core_reg(i);
		read_core_reg(i, p);
	}
	WCN_INFO("------------[ ARM REG ]------------\n");
	for (i = 0; i < 19; i++)
		WCN_INFO("[%s] = \t0x%08x\n", core_reg_name[i], p[i]);

	WCN_INFO("------------[ ARM END ]------------\n");
	kfree(p);
	/* make sure JTAG can connect dap */
	dap_sel_default();

	return 0;
}

static int check_wifi_power_domain_ison(void)
{
	int ret = 0;
	unsigned int temp;

	ret = sprdwcn_bus_reg_read(CHIP_SLP, &temp, 4);
	if (ret < 0) {
		WCN_ERR("%s read CHIP_SLP reg error:%d\n", __func__, ret);
		return ret;
	}
	WCN_INFO("%s CHIP_SLP reg val:0x%x\n", __func__, temp);

	if ((temp & WIFI_ALL_PWRON) != WIFI_ALL_PWRON) {
		/* WIFI WRAP */
		if ((temp & WIFI_WRAP_PWRON) != WIFI_WRAP_PWRON) {
			WCN_INFO("WIFI WRAP have power down\n");
			/* WRAP power on */
			WCN_INFO("WIFI WRAP start power on\n");
			ret = sprdwcn_bus_reg_read(PD_WIFI_AON_CFG4, &temp, 4);
			temp = temp & (~WIFI_WRAP_PWR_DOWN);
			ret = sprdwcn_bus_reg_write(PD_WIFI_AON_CFG4, &temp, 4);
			udelay(200);
			/* MAC power on */
			WCN_INFO("WIFI MAC start power on\n");
			ret = sprdwcn_bus_reg_read(PD_WIFI_MAC_AON_CFG4,
						   &temp, 4);
			temp = temp & (~WIFI_MAC_PWR_DOWN);
			ret = sprdwcn_bus_reg_write(PD_WIFI_MAC_AON_CFG4,
						    &temp, 4);
			udelay(200);
			/* PHY power on */
			WCN_INFO("WIFI PHY start power on\n");
			ret = sprdwcn_bus_reg_read(PD_WIFI_PHY_AON_CFG4,
						   &temp, 4);
			temp = temp & (~WIFI_PHY_PWR_DOWN);
			ret = sprdwcn_bus_reg_write(PD_WIFI_PHY_AON_CFG4,
						    &temp, 4);
			/* retention */
			WCN_INFO("WIFI retention start power on\n");
			ret = sprdwcn_bus_reg_read(PD_WIFI_AON_CFG4, &temp, 4);
			temp = temp | WIFI_RETENTION;
			ret = sprdwcn_bus_reg_write(PD_WIFI_AON_CFG4, &temp, 4);
		}
		/* WIFI MAC */
		else if ((temp & WIFI_MAC_PWRON) != WIFI_MAC_PWRON) {
			WCN_INFO("WIFI MAC have power down\n");
			/* MAC_AON_WIFI_DOZE_CTL [bit1 =0] */
			ret = sprdwcn_bus_reg_read(DUMP_WIFI_AON_MAC_ADDR,
						   &temp, 4);
			temp = temp & (~(1 << 1));
			ret = sprdwcn_bus_reg_write(DUMP_WIFI_AON_MAC_ADDR,
						    &temp, 4);
			udelay(300);
			/* WIFI_MAC_RTN_SLEEPPS_CTL [bit0] =0 */
			ret = sprdwcn_bus_reg_read(WIFI_MAC_RTN_SLEEPPS_CTL,
						   &temp, 4);
			temp = temp & (~(1 << 0));
			ret = sprdwcn_bus_reg_write(WIFI_MAC_RTN_SLEEPPS_CTL,
						    &temp, 4);
		}

	}

	ret = sprdwcn_bus_reg_read(WIFI_ENABLE, &temp, 4);
	if (ret < 0) {
		WCN_ERR("%s read WIFI_ENABLE reg error:%d\n", __func__, ret);
		return ret;
	}
	WCN_INFO("%s WIFI_ENABLE reg val:0x%x\n", __func__, temp);

	if ((temp & WIFI_ALL_EN) == WIFI_ALL_EN)
		return 0;

	WCN_INFO("WIFI_en and wifi_mac_en is disable\n");
	ret = sprdwcn_bus_reg_read(WIFI_ENABLE, &temp, 4);
	temp = temp | WIFI_EN;
	temp = temp | WIFI_MAC_EN;
	ret = sprdwcn_bus_reg_write(WIFI_ENABLE, &temp, 4);

	return 0;
}
/*
 * 0x400F0000 - 0x400F0108 MAC AON
 * check 1:
 * AON APB status Reg(0x4083C00C
 * AON APB Control Reg(0x4083C088   bit1 wrap pwr on(0)/down(1))
 * AON APB Control Reg(0x4083C0A8  bit2 Mac Pwr on(0)/dwn(1))
 * AON APB Control Reg(0x4083C0B8 bit2 Phy pwr on(0)/dwn (1))
 * check 2:
 * Wifi EB : 0x40130004 Wifi EB(bit5)  wifi mac  enable:1
 *
 * 0x40300000 - 0x40358000  wifi 352k share RAM
 * 0x400f1000 - 0x400fe100  wifi reg
 */
int mdbg_dump_mem(void)
{
	long int count;
	int ret;

	/* DUMP ARM REG */
	dump_arm_reg();
	mdbg_clear_log();
	cp_dcache_clean_invalid_all();
	count = mdbg_dump_data(CP_START_ADDR, NULL, FIRMWARE_MAX_SIZE, 0);
	if (count <= 0) {
		WCN_INFO("mdbg start reset marlin reg!\n");
		ret = marlin_reset_reg();
		if (ret < 0)
			return 0;
		cp_dcache_clean_invalid_all();
		count = mdbg_dump_data(CP_START_ADDR, NULL,
				       FIRMWARE_MAX_SIZE, 0);

		WCN_INFO("mdbg only dump ram %ld ok!\n", count);

		goto end;
	}
	WCN_INFO("mdbg dump ram %ld ok!\n", count);

	count = mdbg_dump_data(DUMP_SDIO_ADDR, "start_dump_sdio_reg",
			       DUMP_SDIO_ADDR_SIZE,
			      strlen("start_dump_sdio_reg"));
	WCN_INFO("mdbg dump sdio %ld ok!\n", count);

	/* for dump wifi reg */
	if (DUMP_WIFI_AON_MAC_ADDR)
		count = mdbg_dump_data(DUMP_WIFI_AON_MAC_ADDR,
						"start_dump_wifi_aon_reg",
		DUMP_WIFI_AON_MAC_ADDR_SIZE, strlen("start_dump_wifi_aon_reg"));

	if (DUMP_WIFI_REF_ADDR)
		count = mdbg_dump_data(DUMP_WIFI_REF_ADDR,
						"start_dump_wifi_ref_reg",
		DUMP_WIFI_REF_ADDR_SIZE, strlen("start_dump_wifi_ref_reg"));

	ret = check_wifi_power_domain_ison();
	if (ret != 0) {
		WCN_ERR("********:-) :-) :-) :-)*********\n");
		WCN_ERR("!!!mdbg wifi power domain is down!!\n");
		goto next;
	}
	if (DUMP_WIFI_RTN_PD_MAC_ADDR)
		count = mdbg_dump_data(DUMP_WIFI_RTN_PD_MAC_ADDR,
				       "start_dump_wifi_RTN+PD_reg",
				       DUMP_WIFI_RTN_PD_MAC_ADDR_SIZE,
				       strlen("start_dump_wifi_RTN+PD_reg"));

	if (DUMP_WIFI_352K_RAM_ADDR) {
		count = mdbg_dump_data(DUMP_WIFI_352K_RAM_ADDR,
				       "start_dump_wifi_352K_RAM_reg",
				       DUMP_WIFI_352K_RAM_ADDR_SIZE,
				       strlen("start_dump_wifi_352K_RAM_reg"));
		WCN_INFO("mdbg dump wifi %ld ok!\n", count);
	}

next:
	if (DUMP_INTC_ADDR) {
		count = mdbg_dump_data(DUMP_INTC_ADDR, "start_dump_intc_reg",
			       DUMP_REG_SIZE,
			       strlen("start_dump_intc_reg"));
		WCN_INFO("mdbg dump intc %ld ok!\n", count);
	}

	if (DUMP_SYSTIMER_ADDR) {
		count = mdbg_dump_data(DUMP_SYSTIMER_ADDR,
					"start_dump_systimer_reg",
			       DUMP_REG_SIZE,
			       strlen("start_dump_systimer_reg"));
		WCN_INFO("mdbg dump systimer %ld ok!\n", count);
	}

	if (DUMP_WDG_ADDR) {
		count = mdbg_dump_data(DUMP_WDG_ADDR, "start_dump_wdg_reg",
			DUMP_REG_SIZE, strlen("start_dump_wdg_reg"));
		WCN_INFO("mdbg dump wdg %ld ok!\n", count);
	}

	if (DUMP_APB_ADDR) {
		count = mdbg_dump_data(DUMP_APB_ADDR, "start_dump_apb_reg",
		DUMP_REG_SIZE, strlen("start_dump_apb_reg"));
		WCN_INFO("mdbg dump apb %ld ok!\n", count);
	}

	if (DUMP_DMA_ADDR) {
		count = mdbg_dump_data(DUMP_DMA_ADDR, "start_dump_dma_reg",
		DUMP_REG_SIZE, strlen("start_dump_dma_reg"));
		WCN_INFO("mdbg dump dma %ld ok!\n", count);
	}

	if (DUMP_AHB_ADDR) {
		count = mdbg_dump_data(DUMP_AHB_ADDR, "start_dump_ahb_reg",
			DUMP_REG_SIZE, strlen("start_dump_ahb_reg"));
		WCN_INFO("mdbg dump ahb %ld ok!\n", count);
	}

	count = mdbg_dump_data(DUMP_FM_ADDR, "start_dump_fm_reg",
		DUMP_FM_ADDR_SIZE, strlen("start_dump_fm_reg"));
	WCN_INFO("mdbg dump fm %ld ok!\n", count);

	count = mdbg_dump_data(DUMP_FM_RDS_ADDR, "start_dump_fm_reg",
		DUMP_FM_RDS_ADDR_SIZE, strlen("start_dump_fm_rds_reg"));
	WCN_INFO("mdbg dump fm rds %ld ok!\n", count);

	if (DUMP_WIFI_ADDR) {
		count = mdbg_dump_data(DUMP_WIFI_ADDR, "start_dump_wifi_reg",
			DUMP_WIFI_ADDR_SIZE, strlen("start_dump_wifi_reg"));
		WCN_INFO("mdbg dump wifi %ld ok!\n", count);
	}

	if (DUMP_BT_CMD_ADDR != 0) {
		count = mdbg_dump_data(DUMP_BT_CMD_ADDR,
			"start_dump_bt_cmd_buf",
		DUMP_BT_CMD_ADDR_SIZE, strlen("start_dump_bt_cmd_buf"));
		WCN_INFO("mdbg dump bt cmd %ld ok!\n", count);
	}

	if (DUMP_BT_ADDR) {
		count = mdbg_dump_data(DUMP_BT_ADDR, "start_dump_bt_reg",
		DUMP_BT_ADDR_SIZE, strlen("start_dump_bt_reg"));
		WCN_INFO("mdbg dump bt %ld ok!\n", count);
	}

end:
	/* Make sure only string "marlin_memdump_finish" to slog one time */
	msleep(40);
	count = mdbg_dump_data(0, "marlin_memdump_finish",
		0, strlen("marlin_memdump_finish"));

	WCN_INFO("mdbg dump memory finish\n");
	if ((functionmask[7] & CP2_FLAG_YLOG) == 1)
		complete(&dumpmem_complete);

	return 0;
}

