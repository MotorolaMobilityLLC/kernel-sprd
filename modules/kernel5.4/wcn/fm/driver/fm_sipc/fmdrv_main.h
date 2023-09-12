/*
 *  FM Drivr for Connectivity chip of Spreadtrum.
 *
 *  FM driver main module header.
 *
 *  Copyright (C) 2015 Spreadtrum Company
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef _FMDRV_MAIN_H
#define _FMDRV_MAIN_H

#include <linux/fs.h>

#define FM_OFF			0x00
#define FM_POWERUP_CMD		0x00
#define FM_TUNE_CMD		0x01
#define FM_MUTE_CMD 0x02
#define FM_SCAN_ALL_CMD 0x03
#define FM_SEEK_CMD 0x04
#define FM_SEARCH_ABORT 0X05
#define FM_SET_RDS_MODE 0x06
#define FM_SET_RDS_TYPE 0x07
/* audio mode:0:mono, 1:stereo; 2:blending */
#define FM_SET_AUDIO_MODE 0x08
#define FM_SET_AF_ONOFF 0x09
/* #define FM_SET_AUDIO_PATH 0x09 */
#define FM_SET_REGION 0x0A
#define FM_SET_SCAN_STEP 0x0B
#define FM_CONFIG_DEEMPHASIS 0x0C
#define FM_GET_CURPAMD	0x0D
/* audio mode:0:mono, 1:stereo; 2:blending */
#define FM_GET_AUDIO_MODE 0x0E
#define FM_GET_VOLUME_CMD		0x0F
#define FM_SET_VOLUME_CMD		0x10
#define DM_GET_CUR_BLER_CMD	0x11
#define FM_POWERDOWN_CMD 0x12
#define FM_GET_RSSI_CMD 0x13
#define FM_GET_SNR_CMD	0x14
#define FM_SOFTMUTE_ONOFF_CMD 0x15
#define FM_SET_DEEMPHASIS_CMD		0x16
/* Frequency offset, PDP_TH,PHP_TH, SNR_TH,RSS_THI */
#define FM_SET_SEEK_CRITERIA_CMD	0x17
/* softmute ,blending ,snr_th */
#define FM_SET_AUDIO_THRESHOLD_CMD 0x18
/* Frequency offset, PDP_TH,PHP_TH, SNR_TH,RSS_THI */
#define FM_GET_SEEK_CRITERIA_CMD	0x19
/* softmute ,blending ,snr_th */
#define FM_GET_AUDIO_THRESHOLD_CMD 0x1A
#define FM_SET_ANA_SWITCH_CMD		0x1B

#define FM_READ_WRITE_REG_CMD		0x22


extern struct fmdrv_ops *fmdev;

int fm_open(struct inode *inode, struct file *filep);
int fm_powerup(struct fm_tune_parm *);
int fm_powerdown(void);
int fm_tune(void *);
int fm_seek(void *);
int fm_mute(void *);
int fm_getrssi(void *);
int fm_getcur_pamd(void *);
int fm_rds_onoff(void *);
int fm_ana_switch(void *arg);
int fm_af_onoff(void *arg);
int fm_set_volume(void *arg);
int fm_get_volume(void *arg);
int fm_stop_scan(void *arg);
int fm_scan_all(void *arg);
int fm_rw_reg(void *arg);
int fm_get_monostero(void *arg);
int fm_scan_all(void *arg);
int fm_rw_reg(void *arg);
int fm_stop_scan(void *arg);
int fm_rw_reg(void *arg);
int fm_get_monostero(void *arg);
int fm_set_audio_mode(void *arg);
int fm_set_region(void *arg);
int fm_set_scan_step(void *arg);
int fm_config_deemphasis(void *arg);
int fm_get_audio_mode(void *arg);
int fm_get_current_bler(void *arg);
int fm_get_cur_snr(void *arg);
int fm_softmute_onoff(void *arg);
int fm_set_seek_criteria(void *arg);
int fm_set_audio_threshold(void *arg);
int fm_get_seek_criteria(void *arg);
int fm_get_audio_threshold(void *arg);
ssize_t fm_read_rds_data(struct file *filp, char __user *buf,
	size_t count, loff_t *pos);
int fm_sdio_write(unsigned char *, unsigned int);
void fm_handler (int event, void *data);
struct fm_rds_data *get_rds_data(void);

int fm_ana_switch_inner(void *);


/* WARNING: __packed is preferred over __attribute__((packed)) */
struct fm_cmd_hdr {
	/* 01:cmd; 04:event */
	unsigned char header;
	/* vendor specific command 0xFC8C */
	unsigned short opcode;
	/* Number of bytes follows */
	unsigned char len;
	/* FM Sub Command */
	unsigned char fm_subcmd;
} __packed;

struct fm_event_hdr {
	/* 01:cmd; 04:event */
	unsigned char header;
	/* 0e:cmd complete event; FF:vendor specific event */
	unsigned char id;
	/* Number of bytes follows */
	unsigned char len;
} __packed;

#endif
