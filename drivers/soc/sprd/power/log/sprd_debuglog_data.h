/*
 * sprd_debuglog_data.h -- Sprd Debug Power data type description support.
 *
 * Copyright (C) 2020, 2021 unisoc.
 *
 * Author: James Chen <Jamesj.Chen@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Debug Power data type description.
 */

#ifndef _SPRD_DEBUGLOG_DATA_H
#define _SPRD_DEBUGLOG_DATA_H

#include "sprd_debuglog.h"

/* INTC information init */
#define INTC_INFO_INIT(dts_name, bit_set)				\
	{								\
		.dts = dts_name,					\
		.bits = bit_set,					\
	}

#define INTC_HANDLER_INIT(bit_num, phandler)				\
	{								\
		.bit = bit_num,						\
		.ph = phandler,						\
	}

#define INTC_HANDLER_SET_INIT(handler_num, handler_set)			\
	{								\
		.num = handler_num,					\
		.set = handler_set,					\
	}

/* Register init macor */
#define REG_BIT_INIT(bit_name, bit_mask, bit_offset, bit_expect)	\
	{								\
		.name = bit_name,					\
		.mask = bit_mask,					\
		.offset = bit_offset,					\
		.expect = bit_expect,					\
	}

#define REG_INFO_INIT(reg_name, reg_offset, bit_set)			\
	{								\
		.name = reg_name,					\
		.offset = reg_offset,					\
		.num = ARRAY_SIZE(bit_set),				\
		.bit = bit_set,						\
	}

#define REG_TABLE_INIT(table_name, dts_name, reg_set)			\
	{								\
		.name = table_name,					\
		.dts = dts_name,					\
		.num = ARRAY_SIZE(reg_set),				\
		.reg = reg_set,						\
	}


/* INTC description */
struct intc_info {
	char *dts;
	char *bits[32];
};

struct intc_handler {
	u32 bit;
	int (*ph)(char *buff, u32 second, u32 thrid);
};

struct intc_handler_set {
	u32 num;
	struct intc_handler *set;
};

/* Register description */

/* bit */
struct reg_bit {
	char *name;
	u32 mask;
	u32 offset;
	u32 expect;
};

/* register */
struct reg_info {
	char *name;
	u32 offset;
	u32 num;
	struct reg_bit *bit;
};

/* table */
struct reg_table {
	char *name;
	char *dts;
	u32 num;
	struct reg_info *reg;
};

/* data set */
struct data_set {
	int num;
	void *data;
};

/* Debug data set */
struct debug_data {
	struct data_set intc;
	struct data_set check;
	struct data_set monitor;
	int (*wakeup_source_match)(char *buff, u32 intc, u32 second, u32 thrid);
};

#endif /* _SPRD_DEBUGLOG_DATA_H */
