/*
 * FM Radio driver for RDS with SPREADTRUM SC2331FM Radio chip
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

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include "fmdrv.h"
#include "fmdrv_main.h"
#include "fmdrv_rds_parser.h"

#include "unisoc_fm_log.h"

//#ifdef KERNEL_VERSION_414
#include <misc/marlin_platform.h>
//#else
//#include <linux/marlin_platform.h>
//#endif

extern struct device *fm_miscdev;

struct rds_state_machine {
	signed int state;
	signed int (*state_get)(struct rds_state_machine *thiz);
	signed int (*state_set)(struct rds_state_machine *thiz,
		signed int new_state);
};

static signed int rds_state_get(struct rds_state_machine *thiz)
{
	return thiz->state;
}

static signed int rds_state_set(struct rds_state_machine *thiz,
	signed int new_state)
{
	return thiz->state = new_state;
}

static inline void state_set(struct rds_state_machine *thiz,
	signed int new_state)
{
	if (thiz->state_set)
		thiz->state_set(thiz, new_state);
}

static inline signed int state_get(struct rds_state_machine *thiz)
{
	signed int ret = 0;

	if (thiz->state_get)
		ret = thiz->state_get(thiz);

	return ret;
}

struct rds_bitmap {
	unsigned short bm;
	signed int cnt;
	signed int max_addr;
	unsigned short (*bm_get)(struct rds_bitmap *thiz);
	signed int (*bm_cnt_get)(struct rds_bitmap *thiz);
	signed int (*bm_get_pos)(struct rds_bitmap *thiz);
	signed int (*bm_clr)(struct rds_bitmap *thiz);
	signed int (*bm_cmp)(struct rds_bitmap *thiz, struct rds_bitmap *that);
	signed int (*bm_set)(struct rds_bitmap *thiz, unsigned char addr);
};

static unsigned short rds_bm_get(struct rds_bitmap *thiz)
{
	return thiz->bm;
}

static signed int rds_bm_cnt_get(struct rds_bitmap *thiz)
{
	return thiz->cnt;
}

static signed int rds_bm_get_pos(struct rds_bitmap *thiz)
{
	signed int i = thiz->max_addr;
	signed int j;

	j = 0;
	while ((i > -1) && !(thiz->bm & (1 << i)))
		i--;
#ifdef FM_RDS_USE_SOLUTION_B
	for (j = i; j >= 0; j--) {
		if (!(thiz->bm & (1 << j))) {
			dev_unisoc_fm_info(fm_miscdev,"uncomplete msg 0x%04x, delete it\n", thiz->bm);
			return -1;
		}
	}
#endif

	return i;
}

static signed int rds_bm_clr(struct rds_bitmap *thiz)
{
	thiz->bm = 0x0000;
	thiz->cnt = 0;

	return 0;
}

static signed int rds_bm_cmp(struct rds_bitmap *bitmap1,
	struct rds_bitmap *bitmap2)
{
	return (signed int)(bitmap1->bm - bitmap2->bm);
}

static signed int rds_bm_set(struct rds_bitmap *thiz, unsigned char addr)
{
	struct rds_bitmap bm_old;

	/* text segment addr rang */
	if (addr > thiz->max_addr) {
		dev_unisoc_fm_err(fm_miscdev,"addr invalid(0x%02x)\n", addr);
		return -EFAULT;
	}
	bm_old.bm = thiz->bm;
	thiz->bm |= (1 << addr); /* set bitmap */
	if (!rds_bm_cmp(&bm_old, thiz))
		thiz->cnt++; /* multi get a segment */
	else if (thiz->cnt > 0)
		thiz->cnt--;
	dev_unisoc_fm_info(fm_miscdev,"bitmap=0x%04x, bmcnt=%d\n", thiz->bm, thiz->cnt);

	return 0;
}

void  fmr_assert(unsigned short *a)
{
	if (a == NULL)
		dev_unisoc_fm_info(fm_miscdev,"%s,invalid pointer\n", __func__);
}

/*
* rds_event_set
* To set rds event, and user space can use this flag to juge
* which event happened
* If success return 0, else return error code
*/

static signed int rds_event_set(unsigned short *events, signed int event_mask)
{
	fmr_assert(events);
	*events |= event_mask;
	wake_up_interruptible(&fmdev->rds_han.rx_queue);
	fmdev->rds_han.new_data_flag = 1;

	return 0;
}

/*
* Group types which contain this information:
* TA(Traffic Program) code 0A 0B 14B 15B
*/

void rds_get_eon_ta(unsigned char *buf)
{
	unsigned char *blk_4 = buf + 3 * rds_data_unit_size;
	unsigned char data = *(buf + rds_data_unit_size + 2);
	unsigned char ta_tp;
	unsigned pi_on;

	if (*blk_4  == 0)
		return;
	/* bit3: TA ON  bit4: TP ON */
	ta_tp = (unsigned char)(((data & (1 << 4)) >> 4) | ((data & (1 << 3))
			<< 1));
	bytes_to_short(pi_on, blk_4 + 1);
	/* need add some code to adapter google upper layer  here */
}

/*
* EON = Enhanced Other Networks information
* Group types which contain this information: EON : 14A
* variant code is in blockB low 4 bits
*/

void rds_get_eon(unsigned char *buf)
{
	unsigned char *blk_3 = buf + 2 * rds_data_unit_size;
	unsigned char *blk_4 = buf + 3 * rds_data_unit_size;
	unsigned short pi_on;

	if ((*blk_3 == 0) || (*blk_4 == 0))
		return;
	/* if the upper Layer true */
	bytes_to_short(pi_on, blk_4 + 1);
}

/*
* PTYN = Programme TYpe Name
* From Group 10A, it's a 8 character description transmitted in two 10A group
* block 2 bit0 is PTYN segment address.
* block3 and block4 is PTYN text character
*/

void rds_get_ptyn(unsigned char *buf)
{
	unsigned char *blk_2 = buf + rds_data_unit_size;
	unsigned char *blk_head[2];
	unsigned char seg_addr = ((*(blk_2 + 2)) & 0x01);
	unsigned char ptyn[4], i, step;
	unsigned char *blkc = buf + 2 * rds_data_unit_size;
	unsigned char *blkd = buf + 2 * rds_data_unit_size;

	blk_head[0] = buf + 2 * rds_data_unit_size;
	blk_head[1] = buf + 3 * rds_data_unit_size;
	memcpy((void *)&ptyn[0], (void *)(blk_head[0] + 1), 2);
	memcpy((void *)&ptyn[2], (void *)(blk_head[1] + 1), 2);
	for (i = 0; i < 2; i++) {
		step = i >> 1;
		/* update seg_addr[0,1] if blockC/D is reliable data */
		if ((*blkc == 1) && (*blkd == 1)) {
			/* it's a new PTYN */
			if (memcmp((void *)&ptyn[seg_addr * 4 + step], (void *)
				(ptyn + step), 2) != 0)
				memcpy((void *)&ptyn[seg_addr * 4 + step],
				(void *)(ptyn + step), 2);
		}
	}
}

/*
* EWS = Coding of Emergency Warning Systems
* EWS inclued belows:
* unsigned char data_5b;
* unsigned short data_16b_1;
* unsigned short data_16b_2;
*/

void rds_get_ews(unsigned char *buf)
{
	unsigned char data_5b;
	unsigned short data_16b_1;
	unsigned short data_16b_2;
	unsigned char *blk_2 = buf + rds_data_unit_size;
	unsigned char *blk_3 = buf + 2 * rds_data_unit_size;
	unsigned char *blk_4 = buf + 3 * rds_data_unit_size;

	data_5b = (unsigned char)((*(blk_2 + 2)) & 0x1F);
	bytes_to_short(data_16b_1, (blk_3 + 1));
	bytes_to_short(data_16b_2, (blk_4 + 1));
	/* judge if EWS event Flag was set, and send to read interface */
}

void rfd_get_rtplus(unsigned char *buf)
{
	/* tBTA_RDS_RTP *p_rtp_cb = &p_cb->rtp_cb; */
	unsigned char	*blk_b = buf + rds_data_unit_size;
	unsigned char	*blk_c = buf + 2 * rds_data_unit_size;
	unsigned char	*blk_d = buf + 3 * rds_data_unit_size;
	unsigned char	content_type, s_marker, l_marker;
	bool running;

	running = ((*(blk_b + 2) & 0x08) != 0) ? 1 : 0;
	if ((*blk_c == 1) && (*blk_b == 1)) {
		content_type = ((*(blk_b + 2) & 0x07) << 3) + (*(blk_c + 1)
			>> 5);
		s_marker = (((*(blk_c + 1) & 0x1F) << 1) + (*(blk_c + 2)
			>> 7));
		l_marker = (((*(blk_c + 2)) & 0x7F) >> 1);
	}
	if ((*blk_c == 1) && (*blk_d == 1)) {
		content_type = ((*(blk_c + 2) & 0x01) << 5) +
			(*(blk_d + 1) >> 3);
		s_marker = (*(blk_d + 2) >> 5) + ((*(blk_d + 1) & 0x07) << 3);
		l_marker = (*(blk_d + 2) & 0x1f);
	}
}

/* ODA = Open Data Applications */

void rds_get_oda(unsigned char *buf)
{
	rfd_get_rtplus(buf);
}

/* TDC = Transparent Data Channel */

void rds_get_tdc(unsigned char *buf, unsigned char version)
{
	/* 2nd  block */
	unsigned char	*blk_b	= buf + rds_data_unit_size;
	/* 3rd block */
	unsigned char	*blk_c	= buf + 2*rds_data_unit_size;
	/* 4rd block */
	unsigned char	*blk_d	= buf + 3*rds_data_unit_size;
	unsigned char chnl_num, len = 0, tdc_seg[4];
	/* block C can be BlockC_C */
	/* unrecoverable block 3,or ERROR in block 4, discard this group */
	if ((*blk_b == 0) || (*blk_c == 0) || (*blk_d == 0))
		return;

	/* read TDChannel number */
	chnl_num = *(blk_b + 2) & 0x1f;
	if (version == grp_ver_a) {
		memcpy(tdc_seg, blk_c + 1, 2);
		len = 2;
	}

	memcpy(tdc_seg +  len, blk_d + 1, 2);
	len += 2;
}

/* CT = Programe Clock time */

void rds_get_ct(unsigned char *buf)
{
	unsigned char *blk_2 = buf + rds_data_unit_size;
	unsigned char *blk_3 = buf + 2 * rds_data_unit_size;
	unsigned char *blk_4 = buf + 3 * rds_data_unit_size;
	unsigned char b3_1 = *(blk_3 + 1), b3_2 = *(blk_3 + 2);
	unsigned char b4_1 = *(blk_4 + 1), b4_2 = *(blk_4 + 2);
	unsigned int temp1, temp2;

	unsigned int day = 0;
	unsigned char hour, minute, sense, offset;

	if ((*(blk_3) == 0) || (*(blk_4) == 0))
		return;
	temp1 = (unsigned int) ((b3_1 << 8) | b3_2);
	temp2 = (unsigned int) (*(blk_2 + 2) & 0x03);
	day = (temp2 << 15) | (temp1 >> 1);

	temp1 = (unsigned int)(b3_2 & 0x01);
	temp2 = (unsigned int)(b4_1 & 0xF0);
	hour = (unsigned char)((temp1 << 4) | (temp2 >> 4));
	minute = ((b4_1 & 0x0F) << 2) | ((b4_2 & 0xC0) >> 6);
	sense = (b4_2 & 0x20) >> 5;
	offset = b4_2 & 0x1F;
	/* set RDS EVENT FLAG  in here */
	fmdev->rds_data.CT.day = day;
	fmdev->rds_data.CT.hour = hour;
	fmdev->rds_data.CT.minute = minute;
	fmdev->rds_data.CT.local_time_offset_half_hour = offset;
	fmdev->rds_data.CT.local_time_offset_signbit = sense;
}

void rds_get_oda_aid(unsigned char *buf)
{
}

#ifdef FM_RDS_RT_COMPARISON_ALGORITHM
/*
* rds_rt_addr_get
* To get rt addr form blockB
* If success return 0, else return error code
*/
static void rds_rt_addr_get(unsigned char blkb, unsigned char *addr)
{
	*addr = blkb & 0x0F;
}

static void rds_rt_txtab_get(unsigned char blk,
	unsigned char *txtab, unsigned char *dirty)
{
	if (*txtab != ((blk&0x10) >> 4)) {
		*txtab = (blk & 0x10) >> 4;
		/* yes, we got new txtab code */
		*dirty = fm_true;
	} else {
		/* txtab is the same as last one */
		*dirty = fm_false;
	}
	dev_unisoc_fm_info(fm_miscdev,"txtab=%d, %s\n", *txtab, *dirty ? "new" : "old");
}

static signed int rds_rt_get(unsigned char subtype,
	unsigned char *blkc, unsigned char *blkd,
	unsigned char addr, unsigned char *buf)
{
	unsigned int idx = 0;

	/* text segment addr rang 0~15 */
	if (addr > 0x0F) {
		dev_unisoc_fm_info(fm_miscdev,"addr invalid(0x%02x)\n", addr);
		return -EFAULT;
	}
	switch (subtype) {
	case RDS_GRP_VER_A:
		idx = 4 * addr;
		buf[idx] = *(blkc + 1);
		buf[idx+1] = *(blkc + 2);
		buf[idx+2] = *(blkd + 1);
		buf[idx+3] = *(blkd + 2);
		dev_unisoc_fm_info(fm_miscdev,"rt addr[%02x]:0x%02x 0x%02x 0x%02x 0x%02x\n",
			addr, buf[idx], buf[idx+1], buf[idx+2], buf[idx+3]);
		break;
	case RDS_GRP_VER_B:
		idx = 2 * addr;
		buf[idx] = *(blkd + 1);
		buf[idx+1] = *(blkd + 2);
		dev_unisoc_fm_info(fm_miscdev,"rt addr[%02x]:0x%02x 0x%02x\n",
			addr, buf[idx], buf[idx+1]);
		break;
	default:
		break;
	}
	return 0;
}

static void rds_rt_get_len(unsigned char subtype,
	signed int pos, unsigned int *len)
{
	if (subtype == RDS_GRP_VER_A)
		*len = 4 * (pos + 1);
	else
		*len = 2 * (pos + 1);
}

/*
* rds_rt_cmp
* this function is the most importent flow for RT parsing
* 1.Compare fresh buf with once buf per byte,
* if eque copy this byte to twice buf, else copy it to once buf;
* 2.Check whether we got a full segment, for typeA if copyed
* 4bytes to twice buf, for typeB 2bytes copyed to twice buf;
* 3.Check whether we got the end of RT, if we got 0x0D;
* 4.If we got the end, then caculate the RT length;
* If success return 0, else return error code.
*/
static void rds_rt_cmp(unsigned char addr, unsigned char subtype,
	unsigned char *fresh, unsigned char *once, unsigned char *twice,
	unsigned char *valid, unsigned char *end, unsigned int *len)
{
	unsigned int i = 0, j, cnt = 0;

	/* RT segment width */
	j = (subtype == RDS_GRP_VER_A) ? 4 : 2;
	for (i = 0; i < j; i++) {
		if (fresh[j*addr+i] == once[j*addr+i]) {
			/* get the same byte 2 times */
			twice[j*addr+i] = once[j*addr+i];
			cnt++;
		} else {
			/* use new val */
			once[j*addr+i] = fresh[j*addr+i];
		}
		/* if we got 0x0D twice, it means a RT end */
		if (twice[j*addr+i] == 0x0D) {
			*end = fm_true;
			/* record the length of RT */
			*len = j * addr + i + 1;
		}
	}

	/* check if we got a valid segment 4bytes for typeA, 2bytes for typeB */
	if (cnt == j)
		*valid = fm_true;
	else
		*valid = fm_false;
	dev_unisoc_fm_info(fm_miscdev,"RT seg=%s, end=%s, len=%d\n",
		*valid == fm_true ? "fm_true" : "fm_false",
		*end == fm_true ? "fm_true" : "fm_false",
		*len);
}

void rds_get_rt_cmp(unsigned char *buf, unsigned char grp_type)
{
	unsigned char *blk_2 = buf + rds_data_unit_size;
	unsigned char *blk_3 = buf + 2 * rds_data_unit_size;
	unsigned char *blk_4 = buf + 3 * rds_data_unit_size;
	unsigned char *fresh, *once, *twice, *display;
	static struct rds_state_machine rt_sm = {
		.state = RDS_RT_START,
		.state_get = rds_state_get,
		.state_set = rds_state_set,
	};
	static struct rds_bitmap rt_bm = {
		.bm = 0,
		.cnt = 0,
		.max_addr = 0x15,
		.bm_get = rds_bm_get,
		.bm_cnt_get = rds_bm_cnt_get,
		.bm_set = rds_bm_set,
		.bm_get_pos = rds_bm_get_pos,
		.bm_clr = rds_bm_clr,
		.bm_cmp = rds_bm_cmp,
	};
	unsigned char rt_addr = 0, seg_ok = 0, subtype = RDS_GRP_VER_A;
	/* text AB flag 0 --> 1 or 1-->0 meas new RT incoming */
	unsigned char txtab_change = fm_false;
	/* 0x0D means text end */
	unsigned char txt_end = fm_false;
	signed int pos = 0;
	unsigned int rt_len = 0;
	signed int bufsize = 0;

	if (grp_type == 0x2B)
		subtype = RDS_GRP_VER_B;
	fresh = fmdev->rds_data.rt_data.textdata[0];
	once = fmdev->rds_data.rt_data.textdata[1];
	twice = fmdev->rds_data.rt_data.textdata[2];
	display = fmdev->rds_data.rt_data.textdata[3];
	bufsize = sizeof(fmdev->rds_data.rt_data.textdata[0]);

	/* get basic info: addr, txtab */
	rds_rt_addr_get(*(blk_2 + 2), &rt_addr);
	rds_rt_txtab_get(*(blk_2 + 2),
		&fmdev->rds_data.RDSFLAG.text_ab, &txtab_change);

	/* RT parsing state machine run */
	while (1) {
		switch (state_get(&rt_sm)) {

		case RDS_RT_START:
			if (txtab_change == fm_true) {
				state_set(&rt_sm, RDS_RT_DECISION);
				break;
			}
			if (rds_rt_get(subtype, blk_3, blk_4,
				rt_addr, fresh)) {
				/* if address error, not do parsing */
				state_set(&rt_sm, RDS_RT_FINISH);
				break;
			}
			rds_rt_cmp(rt_addr, subtype,
				fresh, once, twice,
				&seg_ok, &txt_end, &rt_len);
			if (seg_ok == fm_true)
				rt_bm.bm_set(&rt_bm, rt_addr);
			state_set(&rt_sm, RDS_RT_DECISION);
			break;

		case RDS_RT_DECISION:
			if ((txt_end == fm_true) ||
			(rt_bm.bm_get(&rt_bm) == 0xFFFF) ||
			(txtab_change == fm_true) ||
			(rt_bm.bm_cnt_get(&rt_bm) > RDS_RT_MULTI_REV_TH)) {
				/* find 0x0D, and the length has been recorded;
				* get max 64 chars;
				* text AB changed;
				* repeate many times, but no end char get;
				*/
				pos = rt_bm.bm_get_pos(&rt_bm);
				rds_rt_get_len(subtype, pos, &rt_len);
				state_set(&rt_sm, RDS_RT_GETLEN);
			} else
				state_set(&rt_sm, RDS_RT_FINISH);
			break;

		case RDS_RT_GETLEN:
			memcpy(display, twice, bufsize);
			if (rt_len > 0 &&
				((txt_end == fm_true) ||
				(rt_bm.bm_get(&rt_bm) == 0xFFFF))) {
				dev_unisoc_fm_info(fm_miscdev,"RT is %s\n",
					fmdev->rds_data.rt_data.textdata[3]);
				/* yes we got a new RT */
				dev_unisoc_fm_info(fm_miscdev,"Yes, get an RT! [len=%d]\n", rt_len);
			}
			rt_bm.bm_clr(&rt_bm);
			if (txtab_change == fm_true) {
				/* clear buf */
				memset(fresh, 0x20, bufsize);
				memset(once, 0x20, bufsize);
				memset(twice, 0x20, bufsize);
				txtab_change = fm_false;
				/* need to get new RT after show the old RT */
				state_set(&rt_sm, RDS_RT_START);
			} else
				state_set(&rt_sm, RDS_RT_FINISH);
			break;

		case RDS_RT_FINISH:
			rds_event_set(&(fmdev->rds_data.event_status),
				RDS_EVENT_LAST_RADIOTEXT);
			state_set(&rt_sm, RDS_RT_START);
			goto out;

		default:
			break;
		}
	}

out:
	dev_unisoc_fm_info(fm_miscdev,"The RT[3] is %s\n", fmdev->rds_data.rt_data.textdata[3]);
}
#endif

/*
* rt == Radio Text
* Group types which contain this information: 2A 2B
* 2A: address in block2 last 4bits, Text in block3 and block4
* 2B: address in block2 last 4bits, Text in block4(16bits)
*/

void rds_get_rt(unsigned char *buf, unsigned char grp_type)
{
	unsigned char *blk_3 = buf + 2 * rds_data_unit_size;
	unsigned char *blk_4 = buf + 3 * rds_data_unit_size;
	unsigned char addr = ((*(buf + rds_data_unit_size + 2)) & 0x0F);
	unsigned char text_flag = ((*(buf + rds_data_unit_size + 2)) & 0x10);

	dev_unisoc_fm_info(fm_miscdev,"RT Text A/B Flag is %d\n", text_flag);
	/* add for RT not support two types*/
	if (text_flag != 0) {
		rds_event_set(&(fmdev->rds_data.event_status),
			RDS_EVENT_LAST_RADIOTEXT);
		return;
	}

#ifdef FM_RDS_RT_COMPARISON_ALGORITHM
	if (fmdev->rds_han.get_rt_cnt == FM_RDS_RT_CNT_VALUE) {
		memcpy(fmdev->rds_data.rt_data.textdata[0],
			fmdev->rds_data.rt_data.textdata[3], 64);
		memcpy(fmdev->rds_data.rt_data.textdata[1],
			fmdev->rds_data.rt_data.textdata[3], 64);
		memcpy(fmdev->rds_data.rt_data.textdata[2],
			fmdev->rds_data.rt_data.textdata[3], 64);
		fmdev->rds_han.get_rt_cnt++;
	}
	if (fmdev->rds_han.get_rt_cnt > FM_RDS_RT_CNT_VALUE) {
		rds_get_rt_cmp(buf, grp_type);
		return;
	}
	fmdev->rds_han.get_rt_cnt++;
#endif

	if (grp_type == 0x2A) {
		if (*(blk_3 + 1) == 0x0d)
			*(blk_3 + 1) = '\0';
		if (*(blk_3 + 2) == 0x0d)
			*(blk_3 + 2) = '\0';
		if (*(blk_4 + 1) == 0x0d)
			*(blk_4 + 1) = '\0';
		if (*(blk_4 + 2) == 0x0d)
			*(blk_4 + 2) = '\0';
		fmdev->rds_data.rt_data.textdata[3][addr * 4] = *(blk_3 + 1);
		fmdev->rds_data.rt_data.textdata[3][addr * 4 + 1] =
			*(blk_3 + 2);
		fmdev->rds_data.rt_data.textdata[3][addr * 4 + 2] =
			*(blk_4 + 1);
		fmdev->rds_data.rt_data.textdata[3][addr * 4 + 3] =
			*(blk_4 + 2);
	}
	/* group type = 2B */
	else {
		if (*(blk_4 + 1) == 0x0d)
			*(blk_4 + 1) = '\0';
		if (*(blk_4 + 2) == 0x0d)
			*(blk_4 + 2) = '\0';
		fmdev->rds_data.rt_data.textdata[3][addr * 2] = *(blk_4 + 1);
		fmdev->rds_data.rt_data.textdata[3][addr * 2 + 1] =
			*(blk_4 + 2);
	}
	rds_event_set(&(fmdev->rds_data.event_status),
		RDS_EVENT_LAST_RADIOTEXT);
	dev_unisoc_fm_info(fm_miscdev,"RT is %s\n", fmdev->rds_data.rt_data.textdata[3]);
}

/* PIN = Programme Item Number */

void rds_get_pin(unsigned char *buf)
{
	struct RDS_PIN {
		unsigned char day;
		unsigned char hour;
		unsigned char minute;
	} rds_pin;

	unsigned char *blk_4 = buf + 3 * rds_data_unit_size;
	unsigned char byte1 = *(blk_4 + 1), byte2 = *(blk_4 + 2);

	if (*blk_4 == 0)
		return;
	rds_pin.day = ((byte1 & 0xF8) >> 3);
	rds_pin.hour = (byte1 & 0x07) << 2 | ((byte2 & 0xC0) >> 6);
	rds_pin.minute = (byte2 & 0x3F);
}

/*
* SLC = Slow Labelling codes from group 1A, block3
* LA 0 0 0 OPC ECC
*/

void rds_get_slc(unsigned char *buf)
{
	unsigned char *blk_3 = buf + 2 * rds_data_unit_size;
	unsigned char variant_code, slc_type,  paging;
	unsigned char ecc_code = 0;
	unsigned short data;

	if ((*blk_3) == 0)
		return;
	bytes_to_short(data, blk_3);
	data &= 0x0FFF;
	/* take bit12 ~ bit14 of block3 as variant code */
	variant_code = ((*(blk_3 + 1) & 0x70) >> 4);
	if ((variant_code == 0x04) || (variant_code == 0x05))
		slc_type = 0x04;
	else
		slc_type = variant_code;
	if (slc_type == 0) {
		ecc_code = *(blk_3 + 2);
		paging = (*(blk_3 + 1) & 0x0f);
	}
	fmdev->rds_data.extend_country_code = ecc_code;
}


/*
* rds_ps_addr_get
* To get ps addr form blockB, blkB b0~b1
* If success return 0, else return error code
*/
static void rds_ps_addr_get(unsigned char blkb, unsigned char *addr)
{
	*addr = blkb & 0x03;
	dev_unisoc_fm_info(fm_miscdev,"addr=0x%02x\n", *addr);
}

static signed int rds_ps_get(unsigned char *blkd,
	unsigned char addr, unsigned char *buf)
{
	signed int idx = 0;

	/* ps segment addr rang 0~3 */
	if (addr > 0x03) {
		dev_unisoc_fm_err(fm_miscdev,"addr invalid(0x%02x)\n", addr);
		return -EFAULT;
	}
	idx = 2 * addr;
	buf[idx] = *(blkd + 1);
	buf[idx+1] = *(blkd + 2);

	dev_unisoc_fm_info(fm_miscdev,"ps addr[%02x]:0x%02x 0x%02x\n", addr, buf[idx], buf[idx+1]);

	return 0;
}

/*
* rds_ps_cmp
* this function is the most importent flow for PS parsing
* 1.Compare fresh buf with once buf per byte, if eque copy
* this byte to twice buf, else copy it to once buf
* 2.Check whether we got a full segment
* If success return 0, else return error code
*/
static void rds_ps_cmp(unsigned char addr, unsigned char *fresh,
	unsigned char *once, unsigned char *twice, unsigned char *valid)
{
	signed int i = 0;
	signed int j = 0;
	signed int cnt = 0;

	/* PS segment width */
	j = 2;
	for (i = 0; i < j; i++) {
		if (fresh[j*addr+i] == once[j*addr+i]) {
			/* get the same byte 2 times */
			twice[j*addr+i] = once[j*addr+i];
			cnt++;
		} else {
			/* use new val */
			once[j*addr+i] = fresh[j*addr+i];
			}
	}
	/* check if we got a valid segment */
	if (cnt == j)
		*valid = fm_true;
	else
		*valid = fm_false;
	dev_unisoc_fm_info(fm_miscdev,"PS seg = %s\n", *valid == fm_true ? "fm_true" : "fm_false");
}

/*
* Group types which contain this information: 0A 0B
* PS = Programme Service name
* block2 last 2bit stard for address, block4 16bits meaning ps.
*/
void rds_get_ps(unsigned char *buf)
{
	unsigned char *blk_2 = buf + rds_data_unit_size; /* BlockB */
	unsigned char *blk_4 = buf + 3 *  rds_data_unit_size; /* BlockD */
	unsigned char ps_addr;
	unsigned char valid = fm_false;
	signed int pos = 0;
	static struct rds_state_machine ps_sm = {
		.state = RDS_PS_START,
		.state_get = rds_state_get,
		.state_set = rds_state_set,
	};
	static struct rds_bitmap ps_bm = {
		.bm = 0,
		.cnt = 0,
		.max_addr = 0x03,
		.bm_get = rds_bm_get,
		.bm_cnt_get = rds_bm_cnt_get,
		.bm_set = rds_bm_set,
		.bm_get_pos = rds_bm_get_pos,
		.bm_clr = rds_bm_clr,
		.bm_cmp = rds_bm_cmp,
	};
	uint16_t *event = &(fmdev->rds_data.event_status);

	dev_unisoc_fm_info(fm_miscdev,"PS start receive\n");
	dev_unisoc_fm_info(fm_miscdev,"blk2=%d, blk4=%d\n", *blk_2, *blk_4);
	/* parsing Program service name segment (in BlockB/D) */
	valid = *blk_2;
	if (valid == fm_false) {
		dev_unisoc_fm_err(fm_miscdev,"Group0 BlockB crc err\n");
		rds_event_set(event, RDS_EVENT_PROGRAMNAME);
		return;
	}
	rds_ps_addr_get(*(blk_2 + 2), &ps_addr);
	/* PS parsing state machine run */
	while (1) {
		switch (state_get(&ps_sm)) {
		case RDS_PS_START:
			rds_ps_get(blk_4, ps_addr,
				fmdev->rds_data.ps_data.PS[0]);
			rds_ps_cmp(ps_addr, fmdev->rds_data.ps_data.PS[0],
				fmdev->rds_data.ps_data.PS[1],
				fmdev->rds_data.ps_data.PS[2], &valid);
			if (valid == fm_true) {
				ps_bm.bm_set(&ps_bm, ps_addr);
			} else {
			   dev_unisoc_fm_info(fm_miscdev,"before PS[2] is %s\n", fmdev->rds_data.ps_data.PS[2]);
			   memset(fmdev->rds_data.ps_data.PS[2], 0x20, 8);
			   ps_bm.bm_clr(&ps_bm);
			}
			state_set(&ps_sm, RDS_PS_DECISION);
			break;

		case RDS_PS_DECISION:
			/* get max 8 chars */
			/* repeate many times, but no end char get */
			if ((ps_bm.bm_get(&ps_bm) == 0x000F) ||
				(ps_bm.bm_cnt_get(&ps_bm) >
				RDS_RT_MULTI_REV_TH)) {
				pos = ps_bm.bm_get_pos(&ps_bm);
				state_set(&ps_sm, RDS_PS_GETLEN);
			} else
				state_set(&ps_sm, RDS_PS_FINISH);
			break;

		case RDS_PS_GETLEN:
			if (pos == ps_bm.max_addr) {
				/* yes we got a new PS */
				memcpy(fmdev->rds_data.ps_data.PS[3],
					fmdev->rds_data.ps_data.PS[2], 8);
				dev_unisoc_fm_info(fm_miscdev,"PS is %s\n",fmdev->rds_data.ps_data.PS[3]);
				dev_unisoc_fm_info(fm_miscdev,"Yes, get an PS!\n");
			}
			ps_bm.bm_clr(&ps_bm);
			/* clear buf */
			memset(fmdev->rds_data.ps_data.PS[0], 0x20, 8);
			state_set(&ps_sm, RDS_PS_FINISH);
			break;

		case RDS_PS_FINISH:
			rds_event_set(event, RDS_EVENT_PROGRAMNAME);
			state_set(&ps_sm, RDS_PS_START);
			goto out;

		default:
			break;
		}
	}

out:
	dev_unisoc_fm_info(fm_miscdev,"The event is %x\n", fmdev->rds_data.event_status);
	dev_unisoc_fm_info(fm_miscdev,"The PS[3] is %s\n", fmdev->rds_data.ps_data.PS[3]);
}

unsigned short rds_get_freq(void)
{
	return 0;
}
void rds_get_af_method(unsigned char AFH, unsigned char AFL)
{
	static signed short pre_af_num;
	unsigned char  indx, indx2, num;

	dev_unisoc_fm_info(fm_miscdev,"af code is %d and %d\n", AFH, AFL);
	if (AFH >= RDS_AF_NUM_1 && AFH <= RDS_AF_NUM_25) {
		if (AFH == RDS_AF_NUM_1) {
			fmdev->rds_data.af_data.ismethod_a = RDS_AF_M_A;
			fmdev->rds_data.af_data.AF_NUM = 1;
		}
		/* have got af number */
		fmdev->rds_data.af_data.isafnum_get = 0;
		pre_af_num = AFH - 224;
		if (pre_af_num != fmdev->rds_data.af_data.AF_NUM)
			fmdev->rds_data.af_data.AF_NUM = pre_af_num;
		else
			fmdev->rds_data.af_data.isafnum_get = 1;
		if ((AFL < 205) && (AFL > 0)) {
			fmdev->rds_data.af_data.AF[0][0] = AFL + 875;
			/* convert to 100KHz */
#ifdef SPRD_FM_50KHZ_SUPPORT
			fmdev->rds_data.af_data.AF[0][0] *= 10;
#endif
			if ((fmdev->rds_data.af_data.AF[0][0]) !=
				(fmdev->rds_data.af_data.AF[1][0])) {
				fmdev->rds_data.af_data.AF[1][0] =
					fmdev->rds_data.af_data.AF[0][0];
			} else {
				if (fmdev->rds_data.af_data.AF[1][0] !=
					rds_get_freq())
					fmdev->rds_data.af_data.ismethod_a = 1;
				else
					fmdev->rds_data.af_data.ismethod_a = 0;
			}

			/* only one AF handle */
			if ((fmdev->rds_data.af_data.isafnum_get) &&
				(fmdev->rds_data.af_data.AF_NUM == 1)) {
				fmdev->rds_data.af_data.addr_cnt = 0xFF;
			}
		}
	} else if ((fmdev->rds_data.af_data.isafnum_get) &&
		(fmdev->rds_data.af_data.addr_cnt != 0xFF)) {
		/*AF Num correct */
		num = fmdev->rds_data.af_data.AF_NUM;
		num = num >> 1;
/* Put AF freq fm_s32o buffer and check if AF freq is repeat again */
		for (indx = 1; indx < (num + 1); indx++) {
			if ((AFH == (fmdev->rds_data.af_data.AF[0][2*num-1]))
				&& (AFL ==
				(fmdev->rds_data.af_data.AF[0][2*indx]))) {
				dev_unisoc_fm_info(fm_miscdev,"AF same as\n");
				break;
			} else if (!(fmdev->rds_data.af_data.AF[0][2 * indx-1])
				) {
				/*convert to 100KHz */
				fmdev->rds_data.af_data.AF[0][2*indx-1] =
					AFH + 875;
				fmdev->rds_data.af_data.AF[0][2*indx] =
					AFL + 875;
#ifdef MTK_FM_50KHZ_SUPPORT
				fmdev->rds_data.af_data.AF[0][2*indx-1] *= 10;
				fmdev->rds_data.af_data.AF[0][2*indx] *= 10;
#endif
				break;
			}
		}
		num = fmdev->rds_data.af_data.AF_NUM;
		if (num <= 0)
			return;
		if ((fmdev->rds_data.af_data.AF[0][num-1]) == 0)
			return;
		num = num >> 1;
		for (indx = 1; indx < num; indx++) {
			for (indx2 = indx + 1; indx2 < (num + 1); indx2++) {
				AFH = fmdev->rds_data.af_data.AF[0][2*indx-1];
				AFL = fmdev->rds_data.af_data.AF[0][2*indx];
				if (AFH > (fmdev->rds_data.af_data.AF[0][2*indx2
					-1])) {
					fmdev->rds_data.af_data.AF[0][2*indx-1]
					= fmdev->rds_data.af_data.AF[0][2
					*indx2-1];
					fmdev->rds_data.af_data.AF[0][2*indx] =
					fmdev->rds_data.af_data.AF[0][2*indx2];
					fmdev->rds_data.af_data.AF[0][2*indx2-1]
						= AFH;
					fmdev->rds_data.af_data.AF[0][2*indx2]
						= AFL;
				} else if (AFH == (fmdev->rds_data.af_data
					.AF[0][2*indx2-1])) {
					if (AFL > (fmdev->rds_data.af_data.AF[0]
						[2*indx2])) {
						fmdev->rds_data.af_data.AF[0][2
							*indx-1]
						= fmdev->rds_data.af_data
						.AF[0][2*indx2-1];
						fmdev->rds_data.af_data.AF[0][2
							*indx] = fmdev->rds_data
							.af_data.AF[0][2*indx2];
						fmdev->rds_data.af_data.AF[0][2*
							indx2-1] = AFH;
						fmdev->rds_data.af_data.AF[0][2
							*indx2] = AFL;
					}
				}
			}
		}

		/* arrange frequency from low to high:end */
		/* compare AF buff0 and buff1 data:start */
		num = fmdev->rds_data.af_data.AF_NUM;
		indx2 = 0;
		for (indx = 0; indx < num; indx++) {
			if ((fmdev->rds_data.af_data.AF[1][indx]) ==
				(fmdev->rds_data.af_data.AF[0][indx])) {
				if (fmdev->rds_data.af_data.AF[1][indx] != 0)
					indx2++;
				} else
				fmdev->rds_data.af_data.AF[1][indx] =
				fmdev->rds_data.af_data.AF[0][indx];
			}

		/* compare AF buff0 and buff1 data:end */
		if (indx2 == num) {
			fmdev->rds_data.af_data.addr_cnt = 0xFF;
			for (indx = 0; indx < num; indx++) {
				if ((fmdev->rds_data.af_data.AF[1][indx])
					== 0)
					fmdev->rds_data.af_data.addr_cnt = 0x0F;
			}
		} else
			fmdev->rds_data.af_data.addr_cnt = 0x0F;
	}
}
/*
* Group types which contain this information: 0A
* AF = Alternative Frequencies
* af information in block 3
*/

void rds_get_af(unsigned char *buf)
{
	unsigned char *blk_3 = buf + 2 * rds_data_unit_size;

	if (*blk_3 != 1)
		return;
	rds_get_af_method(*(blk_3 + 1), *(blk_3 + 2));
	fmdev->rds_data.af_data.AF[1][24] = 0;
}

/* Group types which contain this information: 0A 0B 15B */
void rds_get_di_ms(unsigned char *buf)
{
}

/*
* Group types which contain this information: TP_all(byte1 bit2);
* TA: 0A 0B 14B 15B(byte2 bit4)
* TP = Traffic Program identification; TA = Traffic Announcement
*/

void rds_get_tp_ta(unsigned char *buf, unsigned char grp_type)
{
	unsigned char *blk_2 = buf + rds_data_unit_size;
	unsigned char byte1 = *(blk_2 + 1), byte2 = *(blk_2 + 2);
	unsigned char ta_tp;
	unsigned short *event = &(fmdev->rds_data.event_status);

	if ((*blk_2) == 0)
		return;
	ta_tp = (unsigned char)((byte1 & (1<<2))>>2);
	if (grp_type == 0x0a || grp_type == 0x0B || grp_type == 0xFB) {
		ta_tp |= (byte2 & (1 << 4));
		rds_event_set(event, RDS_EVENT_TAON_OFF);
	}
}

/*
* Group types which contain this information: all
* block2:Programme Type code = 5 bits($)
* #### ##$$ $$$# ####
*/

void rds_get_pty(unsigned char *buf)
{
	unsigned char *blk_2 = buf + rds_data_unit_size;
	unsigned char byte1 = *(blk_2 + 1), byte2 = *(blk_2 + 2);
	unsigned char	pty = 0;

	if ((*blk_2) == 1)
		pty = ((byte2 >> 5) | ((byte1 & 0x3) << 3));
	fmdev->rds_data.PTY = pty;
}

/*
* Group types which contain this information: all
* Read PI code from the group. grp_typeA: block 1 and block3,
* grp_type B: block3
*/

void rds_get_pi_code(unsigned char *buf, unsigned char version)
{
	unsigned char *blk_3 = buf + 2 * rds_data_unit_size;
	/* pi_code for version A, pi_code_b for version B */
	unsigned short pi_code = 0, pi_code_b = 0;
	unsigned char crc_flag1 = *buf;
	unsigned char crc_flag3 = *(buf + 2 * rds_data_unit_size);

	if (version == invalid_grp_type)
		return;

	if (crc_flag1 == 1)
		bytes_to_short(pi_code, buf+1);
	else
		return;

	if (version == grp_ver_b) {
		if (crc_flag3 == 1)
			bytes_to_short(pi_code_b, blk_3 + 1);
	}

	if (pi_code == 0 && pi_code_b != 0)
		pi_code = pi_code_b;
/* send pi_code value to global and copy to user space in read rds interface */
	fmdev->rds_data.PI = pi_code;
}

/*
* Block 1: PIcode(16bit)+CRC
* Block 2 : Group type code(4bit)
* B0 version(1bit 0:version A; 1:version B)
* TP(1bit)+ PTY(5 bits)
* @ buffer point to the start of Block 1
* Block3: 16bits + 10bits
* Block4: 16bits + 10bits
* rds_get_group_type from Block2
*/
unsigned char rds_get_group_type(unsigned char *buffer)
{
	unsigned char *crc_blk_2 = buffer + rds_data_unit_size;
	unsigned char blk2_byte1 = *(crc_blk_2+1);
	unsigned char group_type;
	unsigned char crc_flag = *crc_blk_2;

	if (crc_flag == 1)
		group_type = (blk2_byte1 & grp_type_mask);
	else
		group_type = invalid_grp_type;
	/* 0:version A, 1: version B */
	if (blk2_byte1 & grp_ver_bit)
		group_type |= grp_ver_b;
	else
		group_type |= grp_ver_a;

	return group_type;
}

void dump_rx_data(unsigned char *buffer, unsigned int len)
{
	unsigned int i;

	dev_unisoc_fm_info(fm_miscdev,"\n fm rx data(%d): ", len);
	for (i = 0; i < len; i++)
		dev_unisoc_fm_info(fm_miscdev,"0x%x__", *(buffer+i));
	dev_unisoc_fm_info(fm_miscdev,"\n");
}

/*
* rds_parser
* Block0: PI code(16bits)
* Block1: Group type(4bits), B0=version code(1bit),
* TP=traffic program code(1bit),
* PTY=program type code(5bits), other(5bits)
* @getfreq - function pointer, AF need get current freq
* Theoretically From FIFO :
* One Group = Block1(16 bits) + CRC(10 bits)
* Block2 +CRC(10 bits)
* Block3(16 bits) + CRC(10 bits)
* Block4(16 bits) + CRC(10 bits)
* From marlin2 chip, the data stream is like below:
* One Group = CRC_Flag(8bit)+Block1(16bits)
* CRC_Flag(8bit)+Block2(16bits)
* CRC_Flag(8bit)+Block3(16bits)
* CRC_Flag(8bit)+Block4(16bits)
*/
void rds_parser(unsigned char *buffer)
{
	unsigned char grp_type;

	fmdev->rds_han.rds_parse_start_time = get_seconds();
	/*dump_rx_data(buffer, len);*/
	grp_type = rds_get_group_type(buffer);
	dev_unisoc_fm_info(fm_miscdev,"group type is : 0x%x\n", grp_type);

	rds_get_pi_code(buffer, grp_type & grp_ver_mask);
	rds_get_pty(buffer);
	rds_get_tp_ta(buffer, grp_type);

	switch (grp_type) {
	case invalid_grp_type:
		dev_unisoc_fm_info(fm_miscdev,"invalid group type\n");
		break;
	/* Processing group 0A */
	case 0x0A:
		rds_get_di_ms(buffer);
		rds_get_af(buffer);
		rds_get_ps(buffer);
		break;
	/* Processing group 0B */
	case 0x0B:
		rds_get_di_ms(buffer);
		rds_get_ps(buffer);
		break;
	case 0x1A:
		rds_get_slc(buffer);
		rds_get_pin(buffer);
		break;
	case 0x1B:
		rds_get_pin(buffer);
		break;
	case 0x2A:
	case 0x2B:
		rds_get_rt(buffer, grp_type);
		break;
	case 0x3A:
		rds_get_oda_aid(buffer);
		break;
	case 0x4A:
		rds_get_ct(buffer);
		break;
	case 0x5A:
	case 0x5B:
		rds_get_tdc(buffer, grp_type & grp_ver_mask);
		break;
	case 0x9a:
		rds_get_ews(buffer);
		break;
	/* 10A group */
	case 0xAA:
		//rds_get_ptyn(buffer);
		break;
	case 0xEA:
		rds_get_eon(buffer);
		break;
	case 0xEB:
		rds_get_eon_ta(buffer);
		break;
	case 0xFB:
		rds_get_di_ms(buffer);
		break;
/* ODA (Open Data Applications) group availability signaled in type 3A groups */
	case 0x3B:
	case 0x4B:
	case 0x6A:
	case 0x6B:
	case 0x7A:
	case 0x7B:
	case 0x8A:
	case 0x8B:
	case 0x9B:
	case 0xAB:
	case 0xBA:
	case 0xBB:
	case 0xCA:
	case 0xCB:
	case 0xDB:
	case 0xDA:
	case 0xFA:
		rds_get_oda(buffer);
		break;
	default:
		dev_unisoc_fm_info(fm_miscdev,"rds group type[0x%x] not to be processed\n", grp_type);
		break;
	}
}

