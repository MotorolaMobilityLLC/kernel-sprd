#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <linux/seq_file.h>

enum debug_ts_index {
	RX_SDIO_PORT,
	MAX_DEBUG_TS_INDEX,
};

static inline char *ts_index2str(u8 index)
{
	switch (index) {
	case RX_SDIO_PORT:
		return "RX_SDIO_PORT";
	default:
		return "UNKNOWN_DEBUG_TS_INDEX";
	}
}

enum debug_cnt_index {
	REORDER_TIMEOUT_CNT,
	MAX_DEBUG_CNT_INDEX,
};

static inline char *cnt_index2str(u8 index)
{
	switch (index) {
	case REORDER_TIMEOUT_CNT:
		return "REORDER_TIMEOUT_CNT";
	default:
		return "UNKNOWN_DEBUG_CNT_INDEX";
	}
}

enum debug_record_index {
	TX_CREDIT_RECORD,
	TX_CREDIT_TIME_DIFF,
	TX_CREDIT_PER_ADD,
	TX_CREDIT_ADD,
	MAX_DEBUG_RECORD_INDEX,
};

static inline char *record_index2str(u8 index)
{
	switch (index) {
	case TX_CREDIT_RECORD:
		return "TX_CREDIT_RECORD";
	case TX_CREDIT_TIME_DIFF:
		return "TX_CREDIT_TIME_DIFF";
	case TX_CREDIT_PER_ADD:
		return "TX_CREDIT_PER_ADD";
	case TX_CREDIT_ADD:
		return "TX_CREDIT_ADD";
	default:
		return "UNKNOWN_DEBUG_RECORD_INDEX";
	}
}

void debug_ctrl_init(void);
void adjust_ts_cnt_debug(char *buf, unsigned char offset);

void debug_ts_enter(enum debug_ts_index index);
void debug_ts_leave(enum debug_ts_index index);
void debug_ts_show(struct seq_file *s, enum debug_ts_index index);

void debug_cnt_inc(enum debug_cnt_index index);
void debug_cnt_dec(enum debug_cnt_index index);
void debug_cnt_add(enum debug_cnt_index index, int num);
void debug_cnt_sub(enum debug_cnt_index index, int num);
void debug_cnt_show(struct seq_file *s, enum debug_cnt_index index);

void debug_record_add(enum debug_record_index index, int record);
void debug_record_show(struct seq_file *s, enum debug_record_index index);

#endif /* __DEBUG_H__ */
