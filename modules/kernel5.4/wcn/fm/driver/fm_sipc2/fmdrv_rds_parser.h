/*
 *  FM Drivr for Connectivity chip of Spreadtrum.
 *
 *  FM RDS Parser module header.
 *
 *  Copyright (C) 2015 Spreadtrum Company
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 */

#ifndef _FMDRV_RDS_PARSER
#define _FMDRV_RDS_PARSER
/* Block1 */
#define RDS_BLCKA		0x00
/* Block2 */
#define RDS_BLCKB		0x10
/* Block3 */
#define RDS_BLCKC		0x20
/* Block4 */
#define RDS_BLCKD		0x30
/* BlockC hyphen */
#define RDS_BLCKC_C		0x40
/* BlockE in RBDS */
#define RDS_BLCKE_B		0x50
/* Block E  */
#define RDS_BLCKE		0x60

/* 3bytes = 8bit(CRC flag) + 16bits (1 block ) */
#define rds_data_unit_size	3
#define rds_data_group_size	(3*4)
#define grp_type_mask		0xF0
#define grp_ver_mask		0x0F
/* 0:version A, 1: version B */
#define grp_ver_bit		(0x01<<3)
#define grp_ver_a		0x0A
#define grp_ver_b		0x0B
#define invalid_grp_type	0x00

/* AF fill in code */
#define RDS_AF_FILL		205
/* AF invalid code low marker */
#define RDS_AF_INVAL_L		205
/* AF invalid code middle marker */
#define RDS_AF_INVAL_M		223
/* 0 AF follow */
#define RDS_AF_NONE		224
/* 1 AF follow */
#define RDS_AF_NUM_1		225
/* 25 AFs follow */
#define RDS_AF_NUM_25		249
/* LF/MF follow */
#define RDS_LF_MF		250
/* AF invalid code high marker */
#define RDS_AF_INVAL_H		251
/* AF invalid code top marker */
#define RDS_AF_INVAL_T		255
/* lowest MF frequency */
#define RDS_MF_LOW		0x10

/* FM base frequency */
#define RDS_FM_BASE		875
/* MF base frequency */
#define RDS_MF_BASE		531
/* LF base frequency */
#define RDS_LF_BASE		153

/* minimum day */
#define RDS_MIN_DAY		1
/* maximum day */
#define RDS_MAX_DAY		31
/* minimum hour */
#define RDS_MIN_HUR		0
/* maximum hour */
#define RDS_MAX_HUR		23
/* minimum minute */
#define RDS_MIN_MUT		0
/* maximum minute */
#define RDS_MAX_MUT		59
/* left over rds data length max in control block */
#define BTA_RDS_LEFT_LEN         24
/* Max radio text length */
#define BTA_RDS_RT_LEN           64
/* 8 character RDS feature length, i.e. PS, PTYN */
#define BTA_RDS_LEN_8            8

/* AF encoding method */
enum {
	/* unknown */
	RDS_AF_M_U,
	/* method - A */
	RDS_AF_M_A,
	/* method - B */
	RDS_AF_M_B
};

#define FM_RDS_USE_SOLUTION_B
#define FM_RDS_RT_COMPARISON_ALGORITHM
/*
 * According to RDS spec, Radiotext(RT):
 * A total of 16 type 2A groups are required to transmit a 64 character
 * RT message and therefore to transmit this message in 5 seconds,
 * 3.2 type 2A groups will be required per second.
 */
#define FM_RDS_PARSE_TIME (5)
#define FM_RDS_RT_CNT_VALUE (0)

#define RDS_RT_MULTI_REV_TH (16)
enum rds_ps_state_machine_t {
	RDS_PS_START = 0,
	RDS_PS_DECISION,
	RDS_PS_GETLEN,
	RDS_PS_DISPLAY,
	RDS_PS_FINISH,
	RDS_PS_MAX
};

enum rds_rt_state_machine_t {
	RDS_RT_START = 0,
	RDS_RT_DECISION,
	RDS_RT_GETLEN,
	RDS_RT_DISPLAY,
	RDS_RT_FINISH,
	RDS_RT_MAX
};
enum {
	RDS_GRP_VER_A = 0, /* group version A */
	RDS_GRP_VER_B
};


/* change 8 bits to 16bits */
#define bytes_to_short(dest, src)  (dest = (unsigned short)(((unsigned short)\
	(*(src)) << 8) + (unsigned short)(*((src) + 1))))

void dump_rx_data(unsigned char *, unsigned int);
void rds_parser(unsigned char *);

#endif
