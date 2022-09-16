// SPDX-License-Identifier: GPL-2.0-only
/*
 * UFS Host Controller driver for Unisoc specific extensions
 *
 * Copyright (C) 2022 Unisoc, Inc.
 *
 */

#include <linux/proc_fs.h>
#include <linux/panic_notifier.h>
#include <../drivers/unisoc_platform/sysdump/unisoc_sysdump.h>

#include "ufs.h"
#include "ufshcd.h"
#include "ufs-sprd-debug.h"

#ifdef CONFIG_SPRD_DEBUG
#define UFS_DBG_ACS_LVL 0660
static bool ufs_debug_en = true;
#else
#define UFS_DBG_ACS_LVL 0440
static bool ufs_debug_en;
#endif

static struct ufs_event_info ufs_event_info[UFS_CMD_RECORD_DEPTH];
static int cmd_record_index = -1;
static int exceed_max_depth;
static spinlock_t ufs_debug_dump;
static char *ufs_cmd_history_str;

static const char *ufs_event_str[UFS_MAX_EVENT] = {
	"SCSI Send     ",
	"SCSI Complete ",
	"DM Send       ",
	"DM Complete   ",
	"TM Send       ",
	"TM Complete   ",
	"UIC Send      ",
	"UIC Complete  ",
	"Host RESET    ",
	"INT ERROR     ",
	"Debug Trigger "
};

bool sprd_ufs_debug_is_supported(void)
{
	return ufs_debug_en;
}

void ufshcd_common_trace(struct ufs_hba *hba, enum ufs_event_list event, void *data)
{
	int index;
	unsigned long flags;

	if (ufs_debug_en == false)
		return;

	if (data == NULL && event < UFS_TRACE_RESET_AND_RESTORE)
		return;

	spin_lock_irqsave(&ufs_debug_dump, flags);

	cmd_record_index++;
	if (cmd_record_index >= UFS_CMD_RECORD_DEPTH) {
		cmd_record_index = 0;
		exceed_max_depth = 1;
	}
	index = cmd_record_index;

	ufs_event_info[index].event = event;
	ufs_event_info[index].cpu = smp_processor_id();
	ufs_event_info[index].pid = current->pid;
	ufs_event_info[index].time = ktime_get_boottime();

	switch (event) {
	case UFS_TRACE_SEND:
	case UFS_TRACE_COMPLETED:
		memcpy(&ufs_event_info[index].pkg, data, sizeof(struct ufs_cmd_info));
		break;
	case UFS_TRACE_DEV_SEND:
	case UFS_TRACE_DEV_COMPLETED:
		memcpy(&ufs_event_info[index].pkg, data, sizeof(struct ufs_devcmd_info));
		break;
	case UFS_TRACE_TM_SEND:
	case UFS_TRACE_TM_COMPLETED:
		memcpy(&ufs_event_info[index].pkg, data, sizeof(struct ufs_tm_cmd_info));
		break;
	case UFS_TRACE_UIC_SEND:
	case UFS_TRACE_UIC_CMPL:
		memcpy(&ufs_event_info[index].pkg, data, sizeof(struct ufs_uic_cmd_info));
		break;
	case UFS_TRACE_DEBUG_TRIGGER:
		ufs_event_info[index].flag = ufs_debug_en;
		break;
	case UFS_TRACE_INT_ERROR:
		ufs_event_info[index].pkg.ie.errors = hba->errors;
		ufs_event_info[index].pkg.ie.uic_error = hba->uic_error;
		break;
	default:
		break;
	}

	spin_unlock_irqrestore(&ufs_debug_dump, flags);
}

void ufshcd_update_common_event_trace(struct ufs_hba *hba,
				      enum ufs_event_list event,
				      unsigned int tag)
{
	u32 crypto;
	struct ufshcd_lrb *lrbp = &hba->lrb[tag];
	struct scsi_cmnd *cmd = lrbp->cmd;
	struct request *rq = scsi_cmd_to_rq(cmd);
	struct ufs_cmd_info cmd_tmp = {};
	struct ufs_devcmd_info devcmd_tmp = {};

	if (ufs_debug_en == false)
		return;

	if (lrbp->cmd) {
		cmd_tmp.opcode = cmd->cmnd[0];
		cmd_tmp.tag = tag;

		/* inline crypto info */
		crypto = le32_to_cpu(lrbp->utr_descriptor_ptr->header.dword_0) &
			UTP_REQ_DESC_CRYPTO_ENABLE_CMD;
		cmd_tmp.crypto_en = crypto ? 1 : 0;
		cmd_tmp.keyslot = crypto ? hba->lrb[tag].crypto_key_slot : 0;
		cmd_tmp.lun = lrbp->lun;

		if (event == UFS_TRACE_COMPLETED) {
			cmd_tmp.time_cost = ktime_get() - lrbp->issue_time_stamp;
			/* response info */
			cmd_tmp.ocs =
				le32_to_cpu(lrbp->utr_descriptor_ptr->header.dword_2) & MASK_OCS;
			memcpy(&cmd_tmp.rsp, lrbp->ucd_rsp_ptr, sizeof(struct utp_upiu_rsp));
		} else if (cmd->cmd_len <= UFS_CDB_SIZE)
			/* SCSI CDB info */
			memcpy(&cmd_tmp.cmnd, cmd->cmnd, cmd->cmd_len);

		/* specific scsi cmd */
		if ((cmd_tmp.opcode == READ_10) || (cmd_tmp.opcode == WRITE_10)) {
			cmd_tmp.lba = scsi_get_lba(cmd);
			cmd_tmp.transfer_len =
				be32_to_cpu(lrbp->ucd_req_ptr->sc.exp_data_transfer_len);
		} else if (cmd_tmp.opcode == UNMAP) {
			cmd_tmp.lba = scsi_get_lba(cmd);
			cmd_tmp.transfer_len = blk_rq_bytes(rq);
		} else {
			cmd_tmp.lba = -1;
			cmd_tmp.transfer_len = -1;
		}

		ufshcd_common_trace(hba, event, &cmd_tmp);
	} else if (lrbp->command_type == UTP_CMD_TYPE_DEV_MANAGE ||
		   lrbp->command_type == UTP_CMD_TYPE_UFS_STORAGE) {

		devcmd_tmp.tag = tag;
		devcmd_tmp.lun = lrbp->lun;

		if (event == UFS_TRACE_DEV_COMPLETED) {
			devcmd_tmp.time_cost = ktime_get() - lrbp->issue_time_stamp;
			/* response info */
			devcmd_tmp.ocs =
				le32_to_cpu(lrbp->utr_descriptor_ptr->header.dword_2) & MASK_OCS;
			memcpy(&devcmd_tmp.rsp, lrbp->ucd_rsp_ptr, sizeof(struct utp_upiu_rsp));
		} else {
			devcmd_tmp.time_cost = 0;
			memcpy(&devcmd_tmp.req, lrbp->ucd_req_ptr, sizeof(struct utp_upiu_req));
		}

		ufshcd_common_trace(hba, event, &devcmd_tmp);
	}
}

static void ufs_sprd_dbg_dump_trace(u32 dump_req, struct seq_file *m, bool dump)
{
	int ptr;
	int i = 0;
	int actual_dump_num;
	unsigned long flags;
	char *dump_pos = NULL;
	long long time_sec, time_ns;
	ktime_t cur_time;
	char b[120];
	int k, n;
	int sb = (int)sizeof(b);
	int transaction_type;
	int scsi_status;
	int sd_size;

	spin_lock_irqsave(&ufs_debug_dump, flags);

	if (dump == 1)
		dump_pos = (char *) ufs_cmd_history_str;

	if (exceed_max_depth == 1)
		actual_dump_num = UFS_CMD_RECORD_DEPTH;
	else
		actual_dump_num = cmd_record_index + 1;

	if (dump_req)
		actual_dump_num = min_t(u32, dump_req, actual_dump_num);

	ptr = ((cmd_record_index + 1) / actual_dump_num) ?
		(cmd_record_index + 1 - actual_dump_num) :
		(cmd_record_index + 1 + UFS_CMD_RECORD_DEPTH - actual_dump_num);

	PRINT_SWITCH(m, dump_pos, "[UFS] CMD History: total_dump_num=%d\n",
		     actual_dump_num);

	for (; i < actual_dump_num; i++, ptr++) {
		k = 0;
		n = 0;
		if (ptr == UFS_CMD_RECORD_DEPTH)
			ptr = 0;

		time_sec = ufs_event_info[ptr].time / NSEC_PER_SEC;
		time_ns = ufs_event_info[ptr].time % NSEC_PER_SEC;
		switch (ufs_event_info[ptr].event) {
		case UFS_TRACE_SEND:
			/* CDB info */
			for (; k < UFS_CDB_SIZE && n < sb; ++k)
				n += scnprintf(b + n, sb - n, "%02x ",
					       (u32)ufs_event_info[ptr].pkg.ci.cmnd[k]);

			PRINT_SWITCH(m, dump_pos,
			"[%lld.%09lld] [%s]-c[%d]-p[%5d]: opc=0x%2x,tag=%2d,lun=0x%2x,\
LBA=%10lld,len=%6d. ICE is %s,KS=%2d. CDB=(%s)\n",
			time_sec, time_ns,
			ufs_event_str[ufs_event_info[ptr].event],
			ufs_event_info[ptr].cpu,
			ufs_event_info[ptr].pid,
			ufs_event_info[ptr].pkg.ci.opcode,
			ufs_event_info[ptr].pkg.ci.tag,
			ufs_event_info[ptr].pkg.ci.lun,
			(u64)ufs_event_info[ptr].pkg.ci.lba,
			ufs_event_info[ptr].pkg.ci.transfer_len,
			ufs_event_info[ptr].pkg.ci.crypto_en ? "ON " : "OFF",
			ufs_event_info[ptr].pkg.ci.keyslot, b);
			break;
		case UFS_TRACE_COMPLETED:
			/* RSP info */
			transaction_type =
				be32_to_cpu(ufs_event_info[ptr].pkg.ci.rsp.header.dword_0) >> 24;
			scsi_status =
				be32_to_cpu(ufs_event_info[ptr].pkg.ci.rsp.header.dword_1) &
				MASK_SCSI_STATUS;
			sd_size = min_t(int, UFS_SENSE_SIZE,
				be16_to_cpu(ufs_event_info[ptr].pkg.ci.rsp.sr.sense_data_len));

			/* sense data info */
			if (ufs_event_info[ptr].pkg.ci.ocs == SUCCESS &&
			    transaction_type == UPIU_TRANSACTION_RESPONSE &&
			    (scsi_status & ~(SAM_STAT_CHECK_CONDITION |
					    SAM_STAT_TASK_SET_FULL |
					    SAM_STAT_BUSY | SAM_STAT_TASK_ABORTED)) != 0 &&
			    be32_to_cpu(ufs_event_info[ptr].pkg.ci.rsp.header.dword_2) &
					MASK_RSP_UPIU_DATA_SEG_LEN)
				for (; k < sd_size && n < sb; ++k)
					n += scnprintf(b + n, sb - n, "%02x ",
					   (u32)ufs_event_info[ptr].pkg.ci.rsp.sr.sense_data[k]);

			PRINT_SWITCH(m, dump_pos,
			"[%lld.%09lld] [%s]-c[%d]-p[%5d]: opc=0x%2x,tag=%2d,lun=0x%2x,\
LBA=%10lld,len=%6d. ICE is %s,KS=%2d. LAT=%lluns. OCS=0x%2x,TT=0x%2x,SS=0x%2x,SD=(%s).\n",
			time_sec, time_ns,
			ufs_event_str[ufs_event_info[ptr].event],
			ufs_event_info[ptr].cpu,
			ufs_event_info[ptr].pid,
			ufs_event_info[ptr].pkg.ci.opcode,
			ufs_event_info[ptr].pkg.ci.tag,
			ufs_event_info[ptr].pkg.ci.lun,
			(u64)ufs_event_info[ptr].pkg.ci.lba,
			ufs_event_info[ptr].pkg.ci.transfer_len,
			ufs_event_info[ptr].pkg.ci.crypto_en ? "ON " : "OFF",
			ufs_event_info[ptr].pkg.ci.keyslot,
			(u64)ufs_event_info[ptr].pkg.ci.time_cost,
			ufs_event_info[ptr].pkg.ci.ocs,
			transaction_type, scsi_status, n ? b : "Not included SENSEDATA");
			break;
		case UFS_TRACE_DEV_SEND:
			PRINT_SWITCH(m, dump_pos,
			"[%lld.%09lld] [%s]-c[%d]-p[%5d]: opc=0x%2x,tag=%2d,lun=0x%2x,\
idn=0x%x,idx=0x%x,sel=0x%x. LAT=%lluns\n",
			time_sec, time_ns,
			ufs_event_str[ufs_event_info[ptr].event],
			ufs_event_info[ptr].cpu,
			ufs_event_info[ptr].pid,
			ufs_event_info[ptr].pkg.dmi.req.qr.opcode,
			ufs_event_info[ptr].pkg.dmi.tag,
			ufs_event_info[ptr].pkg.dmi.lun,
			ufs_event_info[ptr].pkg.dmi.req.qr.idn,
			ufs_event_info[ptr].pkg.dmi.req.qr.index,
			ufs_event_info[ptr].pkg.dmi.req.qr.selector,
			(u64)ufs_event_info[ptr].pkg.dmi.time_cost);
			break;
		case UFS_TRACE_DEV_COMPLETED:
			transaction_type =
				be32_to_cpu(ufs_event_info[ptr].pkg.dmi.rsp.header.dword_0) >> 24;
			PRINT_SWITCH(m, dump_pos,
			"[%lld.%09lld] [%s]-c[%d]-p[%5d]: opc=0x%2x,tag=%2d,lun=0x%2x,\
idn=0x%x,idx=0x%x,sel=0x%x. LAT=%lluns. OCS=0x%2x,TT=0x%2x,query_rsp=%4d\n",
			time_sec, time_ns,
			ufs_event_str[ufs_event_info[ptr].event],
			ufs_event_info[ptr].cpu,
			ufs_event_info[ptr].pid,
			ufs_event_info[ptr].pkg.dmi.rsp.qr.opcode,
			ufs_event_info[ptr].pkg.dmi.tag,
			ufs_event_info[ptr].pkg.dmi.lun,
			ufs_event_info[ptr].pkg.dmi.rsp.qr.idn,
			ufs_event_info[ptr].pkg.dmi.rsp.qr.index,
			ufs_event_info[ptr].pkg.dmi.rsp.qr.selector,
			(u64)ufs_event_info[ptr].pkg.dmi.time_cost,
			ufs_event_info[ptr].pkg.dmi.ocs,
			transaction_type,
			transaction_type == UPIU_TRANSACTION_QUERY_RSP ?
			(int)((be32_to_cpu(ufs_event_info[ptr].pkg.dmi.rsp.header.dword_1) &
			      MASK_RSP_UPIU_RESULT) >> UPIU_RSP_CODE_OFFSET) : -1);
			break;
		case UFS_TRACE_TM_SEND:
			PRINT_SWITCH(m, dump_pos,
			"[%lld.%09lld] [%s]-c[%d]-p[%5d]: tm_func=0x%2x,param1=0x%8x,param2=0x%8x\n",
			time_sec, time_ns,
			ufs_event_str[ufs_event_info[ptr].event],
			ufs_event_info[ptr].cpu,
			ufs_event_info[ptr].pid,
			ufs_event_info[ptr].pkg.tmi.tm_func,
			ufs_event_info[ptr].pkg.tmi.param1,
			ufs_event_info[ptr].pkg.tmi.param2);
			break;
		case UFS_TRACE_TM_COMPLETED:
			PRINT_SWITCH(m, dump_pos,
			"[%lld.%09lld] [%s]-c[%d]-p[%5d]: tm_func=0x%2x,param1=0x%8x,param2=0x%8x. OCS=0x%2x\n",
			time_sec, time_ns,
			ufs_event_str[ufs_event_info[ptr].event],
			ufs_event_info[ptr].cpu,
			ufs_event_info[ptr].pid,
			ufs_event_info[ptr].pkg.tmi.tm_func,
			ufs_event_info[ptr].pkg.tmi.param1,
			ufs_event_info[ptr].pkg.tmi.param2,
			ufs_event_info[ptr].pkg.tmi.ocs);
			break;
		case UFS_TRACE_UIC_SEND:
		case UFS_TRACE_UIC_CMPL:
			PRINT_SWITCH(m, dump_pos,
			"[%lld.%09lld] [%s]-c[%d]-p[%5d]: cmd=0x%2x,arg1=0x%x,arg2=0x%x,arg3=0x%x\n",
			time_sec, time_ns,
			ufs_event_str[ufs_event_info[ptr].event],
			ufs_event_info[ptr].cpu,
			ufs_event_info[ptr].pid,
			ufs_event_info[ptr].pkg.uci.cmd,
			ufs_event_info[ptr].pkg.uci.argu1,
			ufs_event_info[ptr].pkg.uci.argu2,
			ufs_event_info[ptr].pkg.uci.argu3);
			break;
		case UFS_TRACE_RESET_AND_RESTORE:
			PRINT_SWITCH(m, dump_pos,
			"[%lld.%09lld] [%s]-c[%d]-p[%5d]\n",
			time_sec, time_ns,
			ufs_event_str[ufs_event_info[ptr].event],
			ufs_event_info[ptr].cpu,
			ufs_event_info[ptr].pid);
			break;
		case UFS_TRACE_DEBUG_TRIGGER:
			PRINT_SWITCH(m, dump_pos,
			"[%lld.%09lld] [%s]-c[%d]-p[%5d]: debug is %s\n",
			time_sec, time_ns,
			ufs_event_str[ufs_event_info[ptr].event],
			ufs_event_info[ptr].cpu,
			ufs_event_info[ptr].pid,
			ufs_event_info[ptr].flag ? "ON " : "OFF");
			break;
		case UFS_TRACE_INT_ERROR:
			PRINT_SWITCH(m, dump_pos,
			"[%lld.%09lld] [%s]-c[%d]-p[%5d]: errors=0x%08x, uic_error=0x%08x,\
need to queue eh_work!!\n",
			time_sec, time_ns,
			ufs_event_str[ufs_event_info[ptr].event],
			ufs_event_info[ptr].cpu,
			ufs_event_info[ptr].pid,
			ufs_event_info[ptr].pkg.ie.errors,
			ufs_event_info[ptr].pkg.ie.uic_error);
			break;
		default:
			break;
		}
	}
	cur_time = ktime_get_boottime();
	PRINT_SWITCH(m, dump_pos, "current time : %lld.%09lld\n",
		     cur_time / NSEC_PER_SEC, cur_time % NSEC_PER_SEC);
	if (dump == 1)
		PRINT_SWITCH(m, dump_pos, "Dump buffer used : 0x%x / (0x%x)\n",
			     (u32)(dump_pos - ufs_cmd_history_str), DUMP_BUFFER_S);

	spin_unlock_irqrestore(&ufs_debug_dump, flags);
}

static int ufs_sprd_dbg_info_show(struct seq_file *m, void *v)
{
	seq_puts(m, "========== UFS Debug Dump START ==========\n\n");

	ufs_sprd_dbg_dump_trace(UFS_CMD_RECORD_DEPTH, m, 0);

	seq_puts(m, "\n=========== UFS Debug Dump END ===========\n");

	return 0;
}

static int ufs_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufs_sprd_dbg_info_show, inode->i_private);
}

static const struct proc_ops ufs_debug_fops = {
	.proc_open = ufs_debug_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int ufs_dbg_ctl_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "debug control status : %d\n", ufs_debug_en);
	return 0;
}

static int ufs_dbg_ctl_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufs_dbg_ctl_proc_show, inode->i_private);
}

static ssize_t ufs_dbg_ctl_proc_write(struct file *file,
				      const char __user *buffer,
				      size_t count, loff_t *pos)
{
	char val[32];
	unsigned long cmd = 0;

	if (count == 0 || count > 32)
		return -EINVAL;

	if (copy_from_user(val, buffer, count))
		return -EINVAL;

	if (kstrtoul(val, 10, &cmd))
		return -EINVAL;
	if (cmd == 1)
		ufs_debug_en = true;
	else
		ufs_debug_en = false;

	ufshcd_common_trace(NULL, UFS_TRACE_DEBUG_TRIGGER, NULL);

	return count;
}

static const struct proc_ops ufs_debug_ctl_fops = {
	.proc_open = ufs_dbg_ctl_proc_open,
	.proc_write = ufs_dbg_ctl_proc_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int sprd_ufs_panic_handler(struct notifier_block *self,
			       unsigned long val, void *reason)
{
	if (ufs_cmd_history_str)
		ufs_sprd_dbg_dump_trace(UFS_CMD_RECORD_DEPTH, NULL, 1);
	return NOTIFY_DONE;
}

static struct notifier_block sprd_ufs_event_nb = {
	.notifier_call	= sprd_ufs_panic_handler,
	.priority	= INT_MAX,
};

int ufs_sprd_debug_proc_init(struct ufs_hba *hba)
{
	struct proc_dir_entry *ufs_dir;
	struct proc_dir_entry *prEntry;

	if (!hba || !hba->priv) {
		pr_info("%s: NULL host, exiting\n", __func__);
		return -EINVAL;
	}

	spin_lock_init(&ufs_debug_dump);

	ufs_dir = proc_mkdir("ufs", NULL);
	if (!ufs_dir) {
		pr_err("%s: failed to create /proc/ufs\n",
			__func__);
		return -1;
	}

	prEntry = proc_create("debug_info", 0440, ufs_dir, &ufs_debug_fops);
	if (!prEntry)
		pr_info("%s: failed to create /proc/ufs/debug_info\n",
			__func__);

	prEntry = proc_create("debug_control", UFS_DBG_ACS_LVL, ufs_dir,
			      &ufs_debug_ctl_fops);
	if (!prEntry)
		pr_info("%s: failed to create /proc/ufs/debug_control\n",
			__func__);

	ufs_cmd_history_str = devm_kzalloc(hba->dev, DUMP_BUFFER_S, GFP_KERNEL);
	if (!ufs_cmd_history_str) {
		dev_err(hba->dev, "%s devm_kzalloc dump buffer fail!\n",
			__func__);
		return -ENOMEM;
	}

	if (minidump_save_extend_information("ufs_cmd_history",
					     __pa(ufs_cmd_history_str),
					     __pa(ufs_cmd_history_str + DUMP_BUFFER_S)))
		pr_info("%s: failed to link ufs_cmd_history to minidump\n",
			__func__);

	atomic_notifier_chain_register(&panic_notifier_list,
				       &sprd_ufs_event_nb);

	return 0;
}
EXPORT_SYMBOL_GPL(ufs_sprd_debug_proc_init);
