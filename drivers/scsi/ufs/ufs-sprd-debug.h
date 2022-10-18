// SPDX-License-Identifier: GPL-2.0-only
/*
 * UFS Host Controller driver for Unisoc specific extensions
 *
 * Copyright (C) 2022 Unisoc, Inc.
 *
 */

#ifdef CONFIG_SPRD_DEBUG
/* Userdebug ver default ON!!! */
#define UFS_DEBUG_ON_DEF true
#define UFS_DEBUG_ERR_PANIC_DEF true
#else
#define UFS_DEBUG_ON_DEF false
#define UFS_DEBUG_ERR_PANIC_DEF false
#endif

#define UFS_CMD_RECORD_DEPTH (300)
#define UFS_SINGLE_LINE_STR_LIMIT (230)
#define DUMP_BUFFER_S (UFS_CMD_RECORD_DEPTH * UFS_SINGLE_LINE_STR_LIMIT)

enum ufs_event_list {
	UFS_TRACE_SEND,
	UFS_TRACE_COMPLETED,
	UFS_TREAC_SCSI_TIME_OUT,
	/* QUERY \ NOP_OUT&IN \ REJECT CMD */
	UFS_TRACE_DEV_SEND,
	UFS_TRACE_DEV_COMPLETED,
	UFS_TRACE_TM_SEND,
	UFS_TRACE_TM_COMPLETED,
	UFS_TRACE_TM_ERR,
	UFS_TRACE_UIC_SEND,
	UFS_TRACE_UIC_CMPL,
	UFS_TRACE_CLK_GATE,

	UFS_TRACE_RESET_AND_RESTORE,
	UFS_TRACE_INT_ERROR,
	UFS_TRACE_DEBUG_TRIGGER,

	UFS_MAX_EVENT,
};

struct ufs_cmd_info {
	u8 opcode;
	u8 lun;
	u8 crypto_en;
	u8 keyslot;
	u32 tag;
	u32 transfer_len;
	sector_t lba;
	bool fua;
	u64 time_cost;
	unsigned short cmd_len;
	char cmnd[UFS_CDB_SIZE];
	int ocs;
	int trans_type;
	int scsi_stat;
	int sd_size;
	char sense_data[UFS_SENSE_SIZE];
};

struct ufs_devcmd_info {
	u8 lun;
	u32 tag;
	u64 time_cost;
	int ocs;
	struct utp_upiu_req req;
	struct utp_upiu_rsp rsp;
};

struct ufs_uic_cmd_info {
	u32 cmd;
	u32 argu1;
	u32 argu2;
	u32 argu3;
};

struct ufs_tm_cmd_info {
	u8 tm_func;
	u32 param1;
	u32 param2;
	int ocs;
};

struct ufs_int_error {
	u32 errors;
	u32 uic_error;
};

struct ufs_clk_dbg {
	u32 status;
	u32 on;
};

struct ufs_event_info {
	enum ufs_event_list event;
	pid_t pid;
	u32 cpu;
	bool flag;
	bool panic_f;
	ktime_t time;
	union {
		struct ufs_cmd_info ci;
		struct ufs_uic_cmd_info uci;
		struct ufs_devcmd_info dmi;
		struct ufs_tm_cmd_info tmi;
		struct ufs_int_error ie;
		struct ufs_clk_dbg cd;
	} pkg;
};

enum err_type {
	UFS_SPRD_RESET,
	UFS_LINE_RESET,
};

struct ufs_err_cnt {
	unsigned long long sprd_reset_cnt;
	unsigned long long line_reset_cnt;
};

#define PRINT_SWITCH(m, d, fmt, args...) \
do { \
	if (m) \
		seq_printf(m, fmt, ##args); \
	else if (d) { \
		u32 var = snprintf(d, UFS_SINGLE_LINE_STR_LIMIT, fmt, ##args); \
		if (var > 0) { \
			if (var > UFS_SINGLE_LINE_STR_LIMIT) \
				var = UFS_SINGLE_LINE_STR_LIMIT; \
			d += var; \
		} \
	} else \
		pr_info(fmt, ##args); \
} while (0)

int ufs_sprd_debug_init(struct ufs_hba *hba);
void ufshcd_transfer_event_trace(struct ufs_hba *hba,
				      enum ufs_event_list event, unsigned int tag);
void ufshcd_common_trace(struct ufs_hba *hba, enum ufs_event_list event, void *data);
bool sprd_ufs_debug_is_supported(struct ufs_hba *hba);
void sprd_ufs_debug_err_dump(struct ufs_hba *hba);
void sprd_ufs_print_err_cnt(struct ufs_hba *hba);
void ufs_sprd_update_err_cnt(struct ufs_hba *hba, u32 reg, enum err_type type);
void ufs_sprd_sysfs_add_nodes(struct ufs_hba *hba);

